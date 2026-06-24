# REDARC RedVision / TVMS — Protocol & Signal Reference

Technical reference for the reverse-engineered REDARC bus. The
[`RedVision_TVMS_components.dbc`](RedVision_TVMS_components.dbc) file is the
source of truth for signal scaling and confirmation status; this document is the
human-readable companion. For setup and usage, see the [README](README.md).

> **Status legend:** `CONFIRMED` = validated against capture, safe to rely on.
> `PARTLY CONFIRMED` = purpose known, some bytes unknown.
> `CANDIDATE` / `DIAGNOSTIC` = exposed for testing, not yet locked in.

---

## 1. The protocol in brief

The bus is **250 kbit/s** CAN using **29-bit extended IDs** in a J1939 / RV-C
style layout. Every ID is treated as three fields:

```text
29-bit CAN ID  =  Priority   +   DGN (message group)   +   Source Address
                  (top bits)     (PGN-style middle)        (low 8 bits)
```

Example — `0x13F10208`:

```text
DGN (group)    = 0x1F102   ((id >> 8) & 0x1FFFF)
Source Address = 0x08      (id & 0xFF)  -> Battery Sensor
```

The firmware always keys off the **full 29-bit ID**, then matches on
`(DGN, source address)` so it can tell, e.g., the Manager's status from the
display's rebroadcast of the same numbers.

### Who is on the bus (source addresses)

| Addr | Device | Role |
|---:|---|---|
| `0x01` | Manager30 | Battery manager / charger / solar MPPT |
| `0x08` | Battery Sensor | Shunt / BMS (current, voltage, temp, SOC) |
| `0x20` | RedVision display 1 | Screen; also the source of most button commands |
| `0x21` | RedVision display 2 | Second screen / status |
| `0x24` | TVMS1280 | 10 relay outputs + inverter, tanks, temps, voltage inputs |
| `0x30` | TVMS Rogue | 10 dimmable outputs, hardware buttons, tanks, input V/A |
| `0x22` | **This ESP32 bridge** | Our own address, used as the source of command/poll frames (configurable) |
| `0xFA` | Diagnostic / request tool | Readout & history-request traffic |

### Recurring decode patterns

- **Source-node currents** are clean 32-bit values centred on 1,000,000:
  `Current_A = raw / 1000 - 1000` (so `1,020,000` = +20 A, `990,000` = -10 A).
- **Display rebroadcast currents** are 16-bit, centred on 10,000:
  `Display_A = raw / 10 - 1000`.
- **Status frames are multiplexed**: byte `D1` is a *page / item index* and the
  remaining bytes mean different things per page (e.g. TVMS1280 `0x1BFD0224`
  carries tanks, temps and voltages on different `D1` values).
- **Commands** are short frames sent *from a display address* to the target
  hardware (e.g. `0x0F00FF20` from display `0x20`), with `D1` selecting the
  command and later bytes the value.

---

## 2. How the firmware decodes the bus

Everything ships as one ESPHome external component, `redarc`. A singleton
`RedarcCanDispatcher` subscribes to the CAN bus once and hands every frame to
each device's listener; a device simply asks "did this frame match my
`(DGN, source address)`?". No YAML `on_frame:` automation is needed.

- RTR (remote-request) frames are logged with an `rtr=` flag but **not decoded**.
- Decode helpers: `u16_le` / `u32_le`, `rvc_dgn` / `rvc_source_address`,
  `rvc_matches`, and the `current_32_centered` / `current_display_16_centered`
  scalers described above.

### History polling (RTR requests)

The Battery and Manager components poll for stored history at a configurable
interval (`soc_history_poll_interval` / `solar_history_poll_interval`, default
60 s, `0s` disables). Each poll sends an **RTR** frame — extended ID, DLC 8, no
payload — and the source node answers with its paged history:

| Request (RTR) | Answer | Feeds |
|---|---|---|
| `0x0FFCD0··` | `0x13FCD008` pages | SOC Hourly History |
| `0x0FFCD2··` | `0x13FCD208` pages | SOC daily low → Daily Range History |
| `0x0FFCD4··` | `0x13FCD408` pages | SOC daily high → Daily Range History |
| `0x0FFCD6··` | `0x03FCD601` pages | Solar Energy + Solar Day 1-12 History |

The `··` low byte is our `host_address`. The raw replies (and the RTR requests,
tagged `rtr=1`) appear in the DEBUG CAN log, so this doubles as a way to
re-fetch history on demand without opening the physical screen.

---

## 3. Command frames (sent from a display address)

```text
TVMS1280 output : 0x0F002420   CB 00 FF <ch> <00|01> 00 00 00     (ch 0x04..0x0E)
Rogue on/off    : 0x0F003020   CB 00 FF <ch> <00|01> 00 00 00     (ch 0x0C..0x15)
Rogue dim       : 0x0F053020   <ch> 01 <dir> 05 00 FF FF FF        (dir 01=down,64=up,FF=release)
Charging mode   : 0x0F00FF20   43 00 FF FF <00|01> 00 00 00        (00=Touring,01=Storage)
```

## 4. Feedback frames

```text
TVMS1280 out fb : 0x1BFD0024   <base> <s0..s6>     (00=off,01=on,06=fuse-blown,0A=over-temp,14/15=off/on-override,F8=unconfigured,FF=ignore)
Rogue level fb  : 0x1BFD1230   <base> <l0..l6>     (% per channel)
Rogue buttons   : 0x1BFD0030   <base> <b1..b7>     (00=inactive,01=active; D1=0x08 D2 = button 8)
```

## 5. Signal table (full)

| Device | CAN ID | DGN | MUX | Signal | Bytes | Decode | Status |
|---|---:|---:|---:|---|---|---|---|
| Manager30 `0x01` | `0x03F20A01` | `0x1F20A` | — | Output Current A | D1-D4 | raw/1000−1000 | CONFIRMED |
| Manager30 `0x01` | `0x03F20A01` | `0x1F20A` | — | Battery Voltage | D5-D6 | raw×0.001 V | CONFIRMED |
| Manager30 `0x01` | `0x03F20401` | `0x1F204` | — | AC Input Current A | D1-D4 | raw/1000−1000 (LE) | CONFIRMED |
| Manager30 `0x01` | `0x03F20401` | `0x1F204` | — | AC Input Voltage | D5-D6 | raw V (LE) | CONFIRMED |
| Manager30 `0x01` | `0x03F20401` | `0x1F204` | — | DC Input Voltage Raw | D7-D8 | signed raw | UNCONFIRMED |
| Manager30 `0x01` | `0x03F20801` | `0x1F208` | — | Solar Current A | D1-D4 | raw/1000−1000 | CONFIRMED (verify DBC export) |
| Manager30 `0x01` | `0x03F20801` | `0x1F208` | — | Solar Voltage | D5-D6 | raw×0.001 V | CONFIRMED |
| Manager30 `0x01` | `0x03F20801` | `0x1F208` | — | Solar Power W | derived | I × V | DERIVED |
| Manager30 `0x01` | `0x03FCD601` | `0x1FCD6` | D1 page | Solar Energy Wh (per-day buckets) | D2-D7 | 16-bit LE Wh per day slot | CONFIRMED |
| Manager30 `0x01` | `0x03F10801` | `0x1F108` | — | Charging Mode | D1 bit0 | 0=Touring,1=Storage (preferred mode source) | CONFIRMED |
| Manager30 `0x01` | `0x03F20001` | `0x1F200` | — | Charging Stage + Mode | D1 | stage = D1 & 0xFE: 0x00 Not Charging, 0x10 Desulphation, 0x20 Soft-start, 0x30 Boost, 0x40 Absorption, 0x50 Battery Test, 0x60 Equalize, 0x70 Float, 0x80 Maintenance. D1 bit0 = mode (0=Touring, 1=Storage), used as a fallback when `0x1F108` has not been seen | CONFIRMED |
| Manager30 `0x01` | `0x03F20601` | `0x1F206` | — | Vehicle Input Current A | D1-D4 | raw/1000−1000 | CONFIRMED |
| Manager30 `0x01` | `0x03F20601` | `0x1F206` | — | Vehicle Input Voltage | D5-D6 | raw×0.001 V | CONFIRMED |
| Manager30 `0x01` | `0x03F20601` | `0x1F206` | — | Vehicle Input Trigger | D8 | 0 Auto, 1 12V, 2 24V, 3 Ignition, 5 On | CONFIRMED |
| Battery `0x08` | `0x13F10208` | `0x1F102` | — | Current A | D1-D4 | raw/1000−1000 | CONFIRMED |
| Battery `0x08` | `0x13F10208` | `0x1F102` | — | Voltage | D5-D6 | raw×0.001 V | CONFIRMED |
| Battery `0x08` | `0x13F10208` | `0x1F102` | — | Temperature | D7 | raw−60 | CONFIRMED |
| Battery `0x08` | `0x13F10408` | `0x1F104` | — | SOC % | D1 | raw % | CONFIRMED |
| RedVision `0x20` | `0x13F28020` | `0x1F280` | — | Battery Current Display A | D1-D2 | raw/10−1000 | CONFIRMED |
| RedVision `0x20` | `0x13F28020` | `0x1F280` | — | Device Current Display A | D5-D6 | raw/10−1000 | CONFIRMED |
| RedVision `0x21` | `0x13F28221` | `0x1F282` | — | Manager Output Current Display A | D7-D8 | raw/10−1000 | CONFIRMED |
| RedVision disp | `0x13F28220` | `0x1F282` | — | Vehicle Current Display A | D3-D4 | raw/10−1000 | CONFIRMED |
| RedVision disp | `0x13F28420` | `0x1F284` | — | Vehicle Voltage Display | D3-D4 | raw×0.1 V | CONFIRMED |
| TVMS Rogue `0x30` | `0x1BFD0230` | `0x1FD02` | D1=`0x09` | Water Tank 1 % | D2 | raw % | CONFIRMED |
| TVMS Rogue `0x30` | `0x1BFD0230` | `0x1FD02` | D1=`0x09` | Water Tank 2 % | D3 | raw % | CONFIRMED |
| TVMS Rogue `0x30` | `0x1BFD1230` | `0x1FD12` | D1 base | Output Level +0..6 | D2-D8 | raw % | CONFIRMED |
| TVMS Rogue `0x30` | `0x1BFD0030` | `0x1FD00` | D1 base | Input Button 1..8 | D2-D8 | 0=inactive,1=active | CONFIRMED |
| TVMS Rogue `0x30` | `0x1BFD1430` | `0x1FD14` | D1 base | Output Dim Activity +0..6 | D2-D8 | activity only, not input state | CONFIRMED |
| TVMS Rogue `0x30` | `0x1BFD0230` | `0x1FD02` | D1=`0x16` | Input Voltage | D2-D3 | uint16 LE × 0.001 V | CONFIRMED |
| TVMS Rogue `0x30` | `0x1BFD0230` | `0x1FD02` | D1=`0x16` | Input Current | D4-D5 | uint16 LE × 0.001 A | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0024` | `0x1FD00` | D1 base | Output Status +0..6 | D2-D8 | 00=off,01=on,06=fuse-blown,0A=over-temp,14/15=off/on-override,F8=unconfigured,FF=ignore | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x14` | Temperature 1 °C | D2 | raw−100 | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x11` | Temperature 2 °C | D6 | raw−100 | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x14` | Tank 1 % | D4 | raw % | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x14` | Tank 2 % | D6 | raw % | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x17` | Tank 3 % | D2 | raw % | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x17` | Tank 4 % | D4 | raw % | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x17` | Tank 5 % | D6 | raw % | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x1A` | Tank 6 % | D2 | raw % | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x11` | Voltage Input 1 | D2-D3 | uint16 LE mV/1000 | CONFIRMED (item `0x11`) |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1FD02` | D1=`0x11` | Voltage Input 2 | D4-D5 | uint16 LE mV/1000 | CONFIRMED (item `0x12`) |
| TVMS1280 `0x24` | `0x1BFD0024` | `0x1FD00` | D1 base | Digital Inputs 1–3 | ch `0x01`-`0x03` | 0=off,1=on | CANDIDATE |

> **Solar Energy note:** the DBC also documents a single 32-bit `Solar_Energy_Wh`
> on `D2-D5` of page `0x00`. That is a low-value special case; the firmware uses
> the **per-day 16-bit bucket** layout — `D1` is the page index and `D2-D7` are
> three days' Wh, so 4 pages cover **12 days** (page 0 = today/−1/−2 … page 3 =
> −9/−10/−11). It sums all known buckets into Solar Energy and publishes the
> per-day values to the history text sensor. The pages are requested with an
> **RTR** frame to `0x0FFCD6··` (`··` = our `host_address`); the Manager answers
> on `0x03FCD601`.

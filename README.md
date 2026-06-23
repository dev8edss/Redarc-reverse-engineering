# RedVision / TVMS CAN Bridge

An ESP32 / ESPHome firmware that bridges a **REDARC RedVision / TVMS** RV power
system onto **Home Assistant** over CAN. It passively decodes the live bus
traffic (battery, charger/manager, solar, tanks, temperatures, lighting) and,
in `NORMAL` mode, can transmit commands to switch and dim the TVMS outputs — all
exposed as native Home Assistant entities.

The decode is reverse-engineered from captured bus traffic. The byte-level
findings live in the companion DBC file
[`Current_SOC_v53_rouge_input_current.dbc`](Current_SOC_v53_rouge_input_current.dbc);
this README is the human-readable summary plus a build/wiring guide.

> **Status legend** (used throughout): `CONFIRMED` = validated against capture,
> safe to rely on. `PARTLY CONFIRMED` = purpose known, some bytes unknown.
> `DIAGNOSTIC / CANDIDATE` = exposed for testing, not yet locked in.

---

## 1. The REDARC protocol in brief

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
| `0x30` | TVMS Rouge | 10 dimmable outputs, hardware buttons, tanks, input V/A |
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

A full byte-by-byte signal table is in [Section 6](#6-signal-reference). The DBC
is the source of truth for scaling and confirmation status.

---

## 2. Hardware and wiring

### Board

```text
M5Stack Atom Lite (ESP32-PICO)
  + M5Stack Atom CAN base (CA-IS3050G transceiver)
```

### CAN pins

| Function | GPIO |
|---|---|
| CAN TX | `GPIO22` |
| CAN RX | `GPIO19` |

### Bus settings

```text
Bit rate : 250 kbit/s   (250KBPS)
Frames   : extended 29-bit IDs
Mode     : NORMAL      -> read + transmit (switch/dim/commands)
           LISTENONLY  -> passive monitor only (no TX, cannot disturb the bus)
```

### Connecting to the RedVision / TVMS bus (RJ45)

Observed pinout on the RedVision / TVMS RJ45 connectors:

| RJ45 pin | Signal | Status |
|---:|---|---|
| 4 | CAN&nbsp;L | confirmed |
| 5 | CAN&nbsp;H | confirmed |
| 8 | Ground | confirmed |
| (unknown) | 24 V supply | **unconfirmed — do not assume** |

Wire **CAN H → GPIO22 side transceiver H**, **CAN L → L**, and share **ground**.
Power the Atom from USB (or a verified 5 V source) — do **not** feed the
unconfirmed RJ45 24 V pin into the Atom. A correctly terminated bus already has
its 120 Ω end resistors; the Atom CAN base is a stub, so don't add termination
unless you are the physical end of the bus.

> Start with `can_mode: LISTENONLY` to confirm you see traffic safely, then
> switch to `NORMAL` once you want control.

---

## 3. ESPHome application layout

### File map

```text
redvision_tvms_atom_lite_cais3050g_v86_all_new_info.yaml   <- the device config you flash
packages/base.yaml                                         <- shared infra (esp32, wifi, api, canbus)
components/
  redarc_common/            <- CAN dispatcher, address claim, decode helpers (no entities)
  redarc_manager/           <- Manager30  (0x01)
  redarc_battery_sensor/    <- Battery     (0x08)
  redarc_redvision_display/ <- Display(s)  (0x20 / 0x21)
  redarc_tvms_rouge/        <- TVMS Rouge  (0x30)
  redarc_tvms_1280/         <- TVMS1280    (0x24)
redvision_tvms_dashboard_all_new_info.yaml                 <- example Home Assistant Lovelace dashboard
Current_SOC_v53_rouge_input_current.dbc                    <- byte-level decode reference
```

The top-level YAML is intentionally thin: a `substitutions:` block (pins, bus
speed, host address, intervals), a `packages:` include of `base.yaml`, and one
block per device you actually have on your bus.

### How the components fit together

`packages/base.yaml` builds the platform and the **single** CAN interface, then
fans every received frame out to a shared dispatcher:

```yaml
canbus:
  - platform: esp32_can
    id: rv_can
    on_frame:
      - can_id: 0x00000000
        can_id_mask: 0x00000000        # match everything
        # No remote_transmission_request filter, so RTR request frames are also
        # captured and logged instead of being dropped.
        then:
          - lambda: esphome::redarc_common::RedarcCanDispatcher::instance()
                      .dispatch(can_id & 0x1FFFFFFFUL, x, remote_transmission_request);
```

`redarc_common` is the glue (it has **no entities of its own**):

- **`RedarcCanDispatcher`** — a singleton. Each device component calls
  `add_listener(...)` in `setup()`; `dispatch()` logs the frame (at DEBUG, with
  an `rtr=` flag) and hands it to every listener. **RTR (remote-request) frames
  are logged but not decoded** — they carry no payload. So all decoding is just
  "did this data frame match my `(DGN, source address)`?".
- **Bus handle** — on boot it stores the single `canbus` interface in the
  dispatcher (at `setup_priority::BUS`, so it is ready before any device
  component's `loop()` runs). Transmit helpers just null-check that handle; the
  bridge does not announce/claim an address on the bus.
- **Decode helpers** — `u16_le` / `u32_le`, `rvc_dgn` / `rvc_source_address`,
  `rvc_matches`, and the `current_32_centered` / `current_display_16_centered`
  scalers described above.

Each **device component** (`redarc_manager`, `redarc_tvms_rouge`, …) is a normal
ESPHome external component. Its Python `__init__.py` declares a config schema and
**auto-generates the entity IDs/names** (you don't list every sensor in YAML — you
just give the block an `id:` prefix and a `source_address:`). Its C++ registers a
dispatcher listener, parses the frames for its address, and publishes to the
entities; for controllable devices it also builds the outbound command frames.

`external_components:` in `base.yaml` pulls these from GitHub (the `ref:`
controls which branch — `main` for stable). Clearing the ESPHome external-
component cache is required if a schema change seems to be ignored.

---

## 4. Configuring the device

In the top YAML, set the basics and then enable only the blocks for hardware you
own (delete the rest). The `id:` you give a block becomes the **entity name
prefix** (e.g. `id: Manager30` → "Manager30 Output Current"); `source_address:`
must match the device on your bus.

```yaml
substitutions:
  can_tx_pin: GPIO22
  can_rx_pin: GPIO19
  can_bit_rate: 250KBPS
  can_mode: NORMAL                 # or LISTENONLY for a monitor build
  host_address: "0x22"             # this ESP32's address on the bus
  history_poll_interval: 60s       # how often to request SOC/solar history (0 = off)

redarc_battery_sensor:
  id: Battery
  source_address: 0x08
  host_address: ${host_address}
  soc_history_poll_interval: ${history_poll_interval}

redarc_manager:
  id: Manager30
  source_address: 0x01
  host_address: ${host_address}
  battery_sensor_id: Battery       # lets the Manager compute device current
  solar_history_poll_interval: ${history_poll_interval}

redarc_redvision_display:          # MULTI_CONF: list every display you have
  - { id: Redvision1, source_address: 0x20 }
  - { id: Redvision2, source_address: 0x21 }

redarc_tvms_rouge: { id: TVMS_Rouge, source_address: 0x30, host_address: ${host_address} }
redarc_tvms_1280:  { id: TVMS1280,   source_address: 0x24, host_address: ${host_address} }
```

`secrets.yaml` must provide `wifi_ssid`, `wifi_password`, `api_pass`, `ota_pass`.

---

## 5. What each component exposes, and how it is found

This is the "where do the entities come from" map. Entity names are prefixed by
the block `id` (shown here with the example ids). Items tagged *(diagnostic)* are
placed in Home Assistant's diagnostic category.

### Manager30 — `redarc_manager` (source `0x01`)

Listens to the manager's status frames and publishes:

| Entity | Type | From frame (DGN) | Notes |
|---|---|---|---|
| Output Current | sensor A | `0x1F20A` | 32-bit centred current |
| Battery Voltage | sensor V | `0x1F20A` | D5-D6 × 0.001 |
| Device Current | sensor A | derived | `Output − Battery` (needs `battery_sensor_id`) |
| Solar Current / Voltage / Power | sensors A/V/W | `0x1F208` | power = I × V (computed) |
| **Solar Energy** | sensor Wh | `0x1FCD6` | total of the 12 daily Wh buckets (today + −1..−11) |
| Solar Day -1..-11 History | text *(diagnostic)* | `0x1FCD6` | CSV of the previous 11 days' Wh (255 = unknown) |
| AC Input Voltage | sensor V | `0x1F204` | D5-D6 |
| Vehicle Input Trigger | **select** | `0x1F206` / cmd | Auto/12V/24V/Ignition/On (writable) |
| Charging Mode | **select** | `0x1F108` / cmd | Touring / Storage (writable) |
| Charging Stage | text | `0x1F200` | Not Charging / Soft-start / Boost / Absorption / Float / Maintenance |
| CAN Date / Time / Date Time | text | `0x1F304` | bus clock |

Writable entities (the two **selects**) build a command frame from the
`host_address` and send it via the shared bus.

> Note: the Manager polls its own solar history with an RTR request to
> `0x0FFCD6 ··` every `solar_history_poll_interval` (default 60 s; set `0s` to
> disable). The Manager answers on `0x03FCD601` with paged 16-bit Wh buckets —
> 4 pages × 3 days = **12 days** (today + −1..−11) — feeding both the Solar
> Energy total and the Solar Day -1..-11 History text sensor.

### Battery — `redarc_battery_sensor` (source `0x08`)

| Entity | Type | From | Notes |
|---|---|---|---|
| Current / Voltage / Temperature | sensors | `0x1F102` | core shunt data |
| SOC | sensor % | `0x1F104` | state of charge (force-updated) |
| Battery Type | **select** | `0x1F100` / cmd | Gel/AGM/LeadAcid/Calcium/Lithium |
| Configured Capacity | **number** | `0x1F100` / cmd `0x12` | Ah, writable |
| Max Charge Current | **number** | `0x1F100` / cmd `0x14` | A, writable |
| Low SOC Alarm | **number** | `0x1F10A` / cmd `0x41` | %, writable |
| Low Voltage Alarm | **number** | `0x1F10A` / cmd `0x42` | V, writable |
| Last SOC Calibration Target | sensor *(diagnostic)* | `0x...` | last calibrate value |
| SOC Hourly History | text *(diagnostic)* | `0x13FCD0` pages | CSV of the last 24 h of SOC % |
| SOC Daily Range History | text *(diagnostic)* | `0x13FCD2`/`0x13FCD4` pages | CSV of daily `low-high` SOC % pairs |
| **Calibrate SOC Full** | **button** | cmd `0x15` | press to send "SOC = 100%" |

The **numbers/selects/button** are the "inputs": each writes a config command
addressed from `host_address` to the battery. The two **SOC-history text
sensors** are populated from history pages the battery returns after the
component's RTR poll (see below); the daily low and high are combined into the
single `low-high` Range sensor.

### RedVision displays — `redarc_redvision_display` (sources `0x20`, `0x21`)

`MULTI_CONF`: list one block per physical display. They share three sensors
(created once) fed from whichever display rebroadcasts them:

- Redvision Battery Current Display (A)
- Redvision Device Current Display (A)
- Redvision Manager Output Current Display (A)

These are the display's own 16-bit rebroadcast values — handy as a cross-check
against the Manager/Battery source readings.

### TVMS Rouge — `redarc_tvms_rouge` (source `0x30`)

The dimmable-lighting module.

| Entity | Type | From / to | Notes |
|---|---|---|---|
| Output 1–10 | **light** (dimmable) | cmd `0x0F0030` / dim `0x0F0530`; level fb `0x1FD12` | channels `0x0C`–`0x15` |
| Output 1–10 Level | sensor % *(diagnostic)* | `0x1FD12` | real hardware brightness during ramps |
| Input Button 1–8 | binary *(diagnostic)* | `0x1FD00` | physical wall-button state |
| Master | **switch** | channel `0x0B` | module master |
| Tank 1 / Tank 2 | sensors % | `0x1FD02` (D1=`0x09`) | analog tank levels |
| Input Voltage | sensor V | `0x1F108` | supply voltage |

Dimming uses a **timed hold/ramp**: HA sets a target brightness, the component
decides up vs down from the latest `0x1FD12` feedback, sends timed hold pulses
plus a release frame, and holds the requested value until feedback reaches it.
CAN "on" turns an output to 100 %; the hardware button can recall a remembered
brightness (the CAN "recall remembered brightness" command is still unknown).

### TVMS1280 — `redarc_tvms_1280` (source `0x24`)

The relay / inverter module.

| Entity | Type | From / to | Notes |
|---|---|---|---|
| Output 0–9 | **switch** | cmd `0x0F0024` / fb `0x1FD00` | channels `0x04`–`0x0D` |
| Inverter | **switch** | channel `0x0E` | inverter output |
| Master | **switch** | channel `0x0F` | module master |
| Digital Input 1–3 | binary *(diagnostic)* | `0x1FD00` candidate | hardware inputs `0x01`–`0x03` |
| Temperature 1 / 2 | sensors °C | `0x1FD02` (D1=`0x14`/`0x11`) | raw − 100 |
| Voltage Input 1 / 2 | sensors V | `0x1FD02` (D1=`0x11`) | items `0x11`/`0x12`, mV/1000 |
| Tank 1–6 | sensors % | `0x1FD02` (D1=`0x14`/`0x17`/`0x1A`) | multiplexed by page |

TVMS1280 has **no** Rouge-style dimming; its outputs are on/off switches and its
authoritative state comes from the `0x1BFD0024` feedback frame.

### History polling (RTR requests)

Instead of a manual replay button, the Battery and Manager components poll for
history themselves at a configurable interval (`soc_history_poll_interval` /
`solar_history_poll_interval`, default 60 s, `0s` disables). Each poll sends an
**RTR** (remote-request) frame — extended ID, DLC 8, **no payload** — and the
source node answers with its paged history:

| Request (RTR) | Answer | Feeds |
|---|---|---|
| `0x0FFCD0··` | `0x13FCD008` pages | SOC Hourly History |
| `0x0FFCD2··` | `0x13FCD208` pages | SOC daily low → Daily Range History |
| `0x0FFCD4··` | `0x13FCD408` pages | SOC daily high → Daily Range History |
| `0x0FFCD6··` | `0x03FCD601` pages | Solar Energy + Solar Day -1..-11 History |

The `··` low byte is our `host_address`. The raw replies (and the RTR requests,
tagged `rtr=1`) appear in the DEBUG CAN log, so this also doubles as a way to
re-fetch history on demand without opening the physical screen.

---

## 6. Signal reference

### Command frames (sent from a display address)

```text
TVMS1280 output : 0x0F002420   CB 00 FF <ch> <00|01> 00 00 00     (ch 0x04..0x0E)
Rouge on/off    : 0x0F003020   CB 00 FF <ch> <00|01> 00 00 00     (ch 0x0C..0x15)
Rouge dim       : 0x0F053020   <ch> 01 <dir> 05 00 FF FF FF        (dir 01=down,64=up,FF=release)
Charging mode   : 0x0F00FF20   43 00 FF FF <00|01> 00 00 00        (00=Touring,01=Storage)
```

### Feedback frames

```text
TVMS1280 out fb : 0x1BFD0024   <base> <s0..s6>     (00=off,01=on,F8/FF=ignore)
Rouge level fb  : 0x1BFD1230   <base> <l0..l6>     (% per channel)
Rouge buttons   : 0x1BFD0030   <base> <b1..b7>     (00=inactive,01=active; D1=0x08 D2 = button 8)
```

### Signal table (full)

| Device | CAN ID | DGN | MUX | Signal | Bytes | Decode | Status |
|---|---:|---:|---:|---|---|---|---|
| Manager30 `0x01` | `0x03F20A01` | `0x1F20A` | — | Output Current A | D1-D4 | raw/1000−1000 | CONFIRMED |
| Manager30 `0x01` | `0x03F20A01` | `0x1F20A` | — | Battery Voltage | D5-D6 | raw×0.001 V | CONFIRMED |
| Manager30 `0x01` | `0x03F20401` | `0x1F204` | — | AC Input Voltage | D5-D6 | raw V (LE) | CONFIRMED |
| Manager30 `0x01` | `0x03F20401` | `0x1F204` | — | DC Input Voltage Raw | D7-D8 | signed raw | UNCONFIRMED |
| Manager30 `0x01` | `0x03F20801` | `0x1F208` | — | Solar Current A | D1-D4 | raw/1000−1000 | CONFIRMED (verify DBC export) |
| Manager30 `0x01` | `0x03F20801` | `0x1F208` | — | Solar Voltage | D5-D6 | raw×0.001 V | CONFIRMED |
| Manager30 `0x01` | `0x03F20801` | `0x1F208` | — | Solar Power W | derived | I × V | DERIVED |
| Manager30 `0x01` | `0x03FCD601` | `0x1FCD6` | D1 page | Solar Energy Wh (per-day buckets) | D2-D7 | 16-bit LE Wh per day slot | CONFIRMED |
| Manager30 `0x01` | `0x03F10801` | `0x1F108` | — | Charging Mode | D1 bit0 | 0=Touring,1=Storage | CONFIRMED |
| Manager30 `0x01` | `0x03F20001` | `0x1F200` | — | Charging Stage | D1 | 0x21 Soft-start, 0x30/31 Boost, 0x40/41 Absorption, 0x70/71 Float, 0x80/81 Maintenance | CONFIRMED |
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
| TVMS Rouge `0x30` | `0x1BFD0230` | `0x1FD02` | D1=`0x09` | Water Tank 1 % | D2 | raw % | CONFIRMED |
| TVMS Rouge `0x30` | `0x1BFD0230` | `0x1FD02` | D1=`0x09` | Water Tank 2 % | D3 | raw % | CONFIRMED |
| TVMS Rouge `0x30` | `0x1BFD1230` | `0x1FD12` | D1 base | Output Level +0..6 | D2-D8 | raw % | CONFIRMED |
| TVMS Rouge `0x30` | `0x1BFD0030` | `0x1FD00` | D1 base | Input Button 1..8 | D2-D8 | 0=inactive,1=active | CONFIRMED |
| TVMS Rouge `0x30` | `0x1BFD1430` | `0x1FD14` | D1 base | Output Dim Activity +0..6 | D2-D8 | activity only, not input state | CONFIRMED |
| TVMS Rouge `0x30` | `0x13F10830` | `0x1F108` | — | Input Voltage | D1-D2 | uint16 LE ×0.01 V | CONFIRMED |
| TVMS1280 `0x24` | `0x1BFD0024` | `0x1FD00` | D1 base | Output Feedback +0..6 | D2-D8 | 0=off,1=on,F8/FF=ignore | CONFIRMED |
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
> −9/−10/−11). It sums all known buckets into Solar Energy and publishes days
> −1..−11 to the history text sensor. The pages are requested with an **RTR**
> frame to `0x0FFCD6··` (`··` = our `host_address`); the Manager answers on
> `0x03FCD601`.

---

## 7. Notes & gotchas

- **Control needs `NORMAL` mode.** A `LISTENONLY` build cannot transmit
  (switch/dim/config) or send the history-poll RTR requests, but is the safe way
  to first verify the bus.
- **Stale component cache.** If ESPHome rejects new YAML keys after a component
  change, clear `/data/external_components/*` and rebuild (or "Clean Build Files"
  in the ESPHome dashboard) — the loaded schema is stale.
- **Dashboard history charts need ApexCharts.** The SOC/solar history bar charts
  in the example dashboard use the HACS **ApexCharts Card** (`custom:apexcharts-card`)
  to plot the CSV history text sensors; install it or those cards error.
- **Entity renames.** Home Assistant entity IDs can drift from the ESPHome names
  if you rename them in the HA UI.
- **Don't trust the RJ45 24 V pin** until measured; power the Atom over USB.
- **Confirmation policy:** `CONFIRMED` decodes should not be changed unless a new
  capture disproves them; `CANDIDATE` / `DIAGNOSTIC` entities exist for testing
  and still need one-at-a-time toggle confirmation before being locked in.

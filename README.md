# RedVision / TVMS CAN Bridge

Bring your **REDARC RedVision / TVMS** RV power system into **Home Assistant**.

This is firmware for a small ESP32 board that plugs into your REDARC CAN bus. It
reads everything the system is doing — battery, charger, solar, water tanks,
temperatures and lighting — and shows it in Home Assistant as normal sensors,
lights and switches. It can also send commands back to switch and dim your TVMS
outputs.

Everything it reports was worked out by listening to the real bus traffic. If you
want the technical detail behind the decoding, see
[PROTOCOL.md](PROTOCOL.md) and the
[`RedVision_TVMS_components.dbc`](RedVision_TVMS_components.dbc) file.

---

## What you need

- **[M5Stack Atom Lite](https://shop.m5stack.com/products/atom-lite-esp32-development-kit)**
  (ESP32) + **[M5Stack Atom CAN base](https://shop.m5stack.com/products/atomic-canbus-base-ca-is3050g)**
  (CA-IS3050G).
- A way to connect to your REDARC bus (the system uses standard RJ45 / network
  cables — see wiring below).
- Home Assistant with the ESPHome add-on.

---

## Wiring

The bridge talks to REDARC over CAN. On the M5Stack Atom CAN base:

| Signal | Pin |
|---|---|
| CAN TX | `GPIO22` |
| CAN RX | `GPIO19` |

REDARC carries the bus on the RJ45 (network-style) connectors. Measured pinout:

| RJ45 pin | Wire (T568B) | Signal |
|---:|---|---|
| 4 | Blue | CAN&nbsp;L |
| 5 | Blue/White | CAN&nbsp;H |
| 6 | Green | 12–30 V (power, exact purpose unknown) |
| 7 | Brown/White | 12–30 V (power, exact purpose unknown) |
| 8 | Brown | Ground |

Connect **CAN H**, **CAN L** and **Ground** between the RJ45 and the Atom's CAN
base. Power the Atom from **USB** (or a proper regulated 5 V supply).

> **Safety:**
> - Don't connect the 12–30 V RJ45 pins (6 and 7) to the Atom — power it over USB.
> - Don't assume pins 6 and 7 are the same or join them together. Measure each
>   against pin 8 before using either to power anything.
> - A correctly wired REDARC bus already has its end resistors. The Atom is a
>   "stub" off the bus, so don't add termination unless you're at the very end.

---

## Set it up

1. **Install the config.** Flash
   [`RedVision_TVMS_Atom_lite_Cais3050g.yaml`](RedVision_TVMS_Atom_lite_Cais3050g.yaml)
   through ESPHome. It pulls in the `redarc` component automatically from GitHub.

2. **Add your secrets.** Your `secrets.yaml` must provide `wifi_ssid`,
   `wifi_password`, `api_pass` and `ota_pass`.

3. **Start safe, then enable control.** Begin in **listen-only** mode to confirm
   you can see the bus without touching it, then switch to **normal** mode when
   you want switching and dimming:

   ```yaml
       mode: LISTENONLY   # safe monitor: read only
       # mode: NORMAL     # read + control (switch / dim / commands)
   ```

4. **Choose your devices.** In the config, keep only the device blocks for the
   hardware you actually have and delete the rest. The `id:` you give a block
   becomes the name prefix in Home Assistant (e.g. `id: Manager30` →
   "Manager30 Output Current").

   ```yaml
   redarc:
     canbus:
       tx_pin: GPIO22
       rx_pin: GPIO19
       bit_rate: 250KBPS
       mode: LISTENONLY            # switch to NORMAL for control

     host_address: "0xFF"          # this bridge's address on the bus

     battery_sensor:
       - id: Battery
         source_address: 0x08

     manager:
       - id: Manager30
         source_address: 0x01
         battery_sensor_id: Battery

     redvision_display:
       - id: Redvision1
         source_address: 0x20
       - id: Redvision2
         source_address: 0x21

     tvms_rogue:
       - id: TVMS_Rogue
         source_address: 0x30
         dimmable_outputs: [2, 3, 4, 5, 6, 7, 10]
         transition_length: 3s

     tvms_1280:
       - id: TVMS1280
         source_address: 0x24
   ```

Each device has a sensible default address (battery `0x08`, manager `0x01`,
display `0x20`, Rogue `0x30`, TVMS1280 `0x24`), so the defaults will often work as-is
and you can leave `source_address` out entirely.

### Configure Rogue dimming

All ten Rogue outputs remain Home Assistant `light` entities. Only outputs listed
under `dimmable_outputs` expose a brightness slider and use `transition_length`.
Every unlisted output is still a light entity, but it is ON/OFF only and sends only
the Rogue ON and OFF commands.

```yaml
redarc:
  tvms_rogue:
    - id: TVMS_Rogue
      source_address: 0x30
      dimmable_outputs: [2, 3, 4, 5, 6, 7, 10]
      transition_length: 3s
```

Important points:

- Output numbers must be between `1` and `10` and cannot be repeated.
- `dimmable_outputs` defaults to `[]`; no output is assumed dimmable unless listed.
- `transition_length` belongs only inside the `tvms_rogue` block.
- `transition_length` affects only outputs listed in `dimmable_outputs`.
- Non-dimmable outputs continue to report their actual ON/OFF state from Rogue
  output-level feedback.

### Device discovery and Rogue validation logs

If a device uses a different address, the easiest way to find it is the
**device logs**. When you open the ESPHome log viewer, the bridge prints a table
of every device it found on the bus, with each address shown in decimal and hex:

```text
Discovered devices:
  Device Type         Address       Serial No
  Manager30             1 (0x01)    2410######-####
  BMS Battery Sensor    8 (0x08)    2409######-####
  RedVision Display    32 (0x20)    ...
```

At the same logger connection, each configured Rogue requests output
configuration DGN `0x1FD0E` and compares the Rogue's programmed dimmable flags
with the YAML `dimmable_outputs` list. This validation is informational and does
not change the Home Assistant entity capabilities selected by YAML.

A matching configuration logs:

```text
[I][redarc_tvms_rogue]: Rogue dimming configuration matches YAML for 10 reported outputs
```

A mismatch is printed at INFO level for each affected output:

```text
[I][redarc_tvms_rogue]: Output 1 dimming mismatch: YAML=Non-Dimmable Rogue=Dimmable
[I][redarc_tvms_rogue]: Output 10 dimming mismatch: YAML=Dimmable Rogue=Non-Dimmable
```

If the Rogue does not return configuration records, ESPHome logs a warning that
the dimming list could not be validated. The YAML configuration remains active.
The validation request requires CAN `NORMAL` mode; it cannot transmit in
`LISTENONLY` mode.

You can also read the address off the **RedVision display**, where it is shown in
**decimal**. Enter that number straight into the config — no conversion needed.
`source_address` and `host_address` accept either plain decimal or hex with a
`0x` prefix, so `source_address: 48` and `source_address: 0x30` mean the same
thing. **Two devices cannot share the same address**, so if you run two of the
same type, the second one must have its own address.

### Handy shared settings

These live on the top-level `redarc:` block and apply to the appropriate devices:

| Setting | Default | What it does |
|---|---|---|
| `host_address` | `0xFF` | This bridge's address; used when sending commands |
| `filter_interval` | `5s` | How often throttled values are published |
| `history_poll_interval` | `60s` | How often battery SOC / solar history is fetched (`0s` = off) |
| `time` | on | Adds a Home Assistant clock and the Manager's Set Time button. Use `time: false` to disable it |

Rogue-only settings such as `dimmable_outputs`, `transition_length` and
`true_off_threshold` belong inside each `tvms_rogue` entry, not on the top-level
`redarc:` block.

> Using an external CAN board (like an MCP2515) instead of the M5Stack base is
> possible but **untested** — see
> [README_external_components.md](README_external_components.md).

---

## What you get in Home Assistant

Entity names are prefixed with the `id:` you gave each block. Items marked
*(diagnostic)* show up under Home Assistant's diagnostic section.

### Manager30 (charger / solar)

| Entity | What it is |
|---|---|
| Output Current, Battery Voltage | Manager's main output |
| Device Current | Output minus battery (needs `battery_sensor_id`) |
| Solar Current / Voltage / Power | Live solar input |
| Solar Energy Today / Solar Energy Total | Today's and 12-day total solar generation (Wh) |
| Solar Day 1-12 History *(diagnostic)* | Last 12 days of daily solar Wh |
| AC Input Current / Voltage | Mains charger input |
| Vehicle Input Current / Voltage | DC / vehicle input |
| Vehicle Input Trigger | Auto / 12V / 24V / Ignition / On (**changeable**) |
| Charging Mode | Touring / Storage (**changeable**) |
| Charging Stage | e.g. Boost, Absorption, Float, Maintenance |
| CAN Date / Time | The system clock |

### Battery

| Entity | What it is |
|---|---|
| Current / Voltage / Temperature | Core shunt readings |
| SOC | State of charge (%) |
| Battery Type | Gel / AGM / Lead Acid / Calcium / Lithium (**changeable**) |
| Configured Capacity, Max Charge Current | Battery settings (**changeable**) |
| Low SOC Alarm, Low Voltage Alarm | Alarm thresholds (**changeable**) |
| SOC Hourly History *(diagnostic)* | Last 24 h of SOC |
| SOC Daily Range History *(diagnostic)* | Daily low–high SOC |
| Calibrate SOC Full | Button: tell the battery "I'm at 100%" |

### RedVision displays

The screens rebroadcast a few readings, handy as a cross-check:

- Battery Current, Device Current, Manager Output Current (as shown on the display)

### TVMS Rogue

| Entity | What it is |
|---|---|
| Output 1–10 | Home Assistant **lights**; listed outputs are dimmable, unlisted outputs are ON/OFF only |
| Output 1–10 Level *(diagnostic)* | Actual hardware output percentage from CAN feedback |
| Input 1–8 *(diagnostic)* | Physical input/button state |
| Master | Module master **switch** |
| Tank 1 / Tank 2 | Water tank levels |
| Input Voltage / Current | Module input |
| Output Status *(diagnostic)* | Reports faults such as fuse blown or over temperature |

### TVMS1280 (relays / inverter)

| Entity | What it is |
|---|---|
| Output 1–10 | On/off **switches** |
| Inverter, Master | Inverter and module master **switches** |
| Temperature 1 / 2 | Module temperatures |
| Voltage Input 1 / 2 | Voltage inputs |
| Tank 1–6 | Water tank levels |
| Digital Input 1–3 *(diagnostic)* | Hardware inputs |
| Output Status *(diagnostic)* | Reports faults (fuse blown, over temp, etc.) |

---

## Using entities in your own automations

Every entity gets a stable id derived from its device block's `id:`, in the form
`<device id>_<entity>`. So with `id: TVMS_Rogue` and `id: TVMS1280` you can refer
to them directly in your own YAML — no extra config, no C++:

```text
id(TVMS_Rogue_input_8).state             # bool: physical input/button 8 pressed
id(TVMS1280_output_1).turn_on();         # switch output 1 on
id(TVMS1280_output_1).toggle();          # toggle output 1
id(Manager30_solar_power).state          # float: solar watts
id(TVMS_Rogue_output_3)                  # Rogue output 3 light entity
```

Every id suffix matches that entity's name in Home Assistant: e.g.
`Manager30_output_current`, `Battery_soc`, `TVMS1280_output_1`…`output_10`,
`TVMS1280_input_1`…`3`, `TVMS_Rogue_input_1`…`8` (physical inputs),
`TVMS_Rogue_output_1`…`10` (light entities), `TVMS_Rogue_master`,
`Manager30_charging_mode`, `Manager30_set_time`,
`Battery_calibrate_soc_full`. The entity-kind word (select/number/button) is
dropped. Watch the boot logs or ESPHome's generated code if you are unsure of an
exact suffix.

**Example — Rogue input 8 toggles TVMS1280 output 1** (the cross-device link),
done entirely in your device YAML:

```yaml
interval:
  - interval: 100ms
    then:
      - lambda: |-
          static bool last = false;
          const bool now = id(TVMS_Rogue_input_8).state;
          if (now && !last) id(TVMS1280_output_1).toggle();
          last = now;
```

(Needs `NORMAL` CAN mode to actually send the command. The device block must
have an explicit `id:` for the derived ids to exist.)

---

## Troubleshooting

- **No control / cannot switch anything.** You are in `LISTENONLY` mode. Switch
  to `NORMAL` to send commands.
- **A Rogue output has the wrong brightness controls.** Update that Rogue's
  `dimmable_outputs` list and rebuild. Entity capabilities are selected at compile
  time; the `0x1FD0E` validation reports mismatches but does not alter entities.
- **No Rogue configuration validation response.** Confirm the CAN bus is in
  `NORMAL` mode and that `source_address` and `host_address` are correct.
- **History charts on the example dashboard are empty or erroring.** They need
  the **ApexCharts Card** from HACS.
- **Entity names look different in Home Assistant.** HA remembers any entity you
  rename in its own UI, which can drift from the names here.

The example Home Assistant dashboard is in
[`RedVision_TVMS_Dashboard.yaml`](RedVision_TVMS_Dashboard.yaml).

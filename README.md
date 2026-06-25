# RedVision / TVMS CAN Bridge

Bring your **REDARC RedVision / TVMS** RV power system into **Home Assistant**.

This is firmware for a small ESP32 board that plugs into your REDARC CAN bus. It
reads everything the system is doing — battery, charger, solar, water tanks,
temperatures and lighting — and shows it in Home Assistant as normal sensors and
switches. It can also send commands back to switch and dim your TVMS outputs.

Everything it reports was worked out by listening to the real bus traffic. If you
want the technical detail behind the decoding, see
[PROTOCOL.md](PROTOCOL.md) and the
[`RedVision_TVMS_components.dbc`](RedVision_TVMS_components.dbc) file.

---

## What you need

- **[M5Stack Atom Lite](https://shop.m5stack.com/products/atom-lite-esp32-development-kit)**
  (ESP32) + **[M5Stack Atom CAN base](https://shop.m5stack.com/products/atom-can-kit-ca-is3050g)**
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

     tvms_1280:
       - id: TVMS1280
         source_address: 0x24
   ```

Each device has a sensible default address (battery `0x08`, manager `0x01`,
display `0x20`, Rogue `0x30`, TVMS1280 `0x24`), so the defaults will often work as-is
and you can leave `source_address` out entirely.

If a device uses a different address, the easiest way to find it is the
**device logs**: at the start of the logs (when you open the ESPHome log viewer)
the bridge prints a table of every device it found on the bus, with each
address shown in both decimal and hex, e.g.:

```text
Discovered devices:
  Device Type         Address       Serial No
  Manager30             1 (0x01)    2410143248-0004
  BMS Battery Sensor    8 (0x08)    2409142757-0006
  RedVision Display    32 (0x20)    ...
```

You can also read the address off the **RedVision display**, where it's shown in
**decimal**. Either way, enter that number straight into the config — no
conversion needed. `source_address` (and `host_address`) accept either plain
decimal or hex with a `0x` prefix, so `source_address: 48` and
`source_address: 0x30` mean exactly the same thing. **Two devices can't share the
same address**, so if you run two of the same type, the second one must have its
own address.

### Handy settings

These live on the `redarc:` block and apply to every device (override inside a
device block only if it needs to differ):

| Setting | Default | What it does |
|---|---|---|
| `host_address` | `0xFF` | This bridge's address; used when sending commands |
| `filter_interval` | `5s` | How often values are published (throttle) |
| `history_poll_interval` | `60s` | How often battery SOC / solar history is fetched (`0s` = off) |
| `time` | on | Adds a Home Assistant clock + the Manager's "Set Time" button. Use `time: false` to turn off |

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

### TVMS Rogue (dimmable lighting)

| Entity | What it is |
|---|---|
| Output 1–10 | Dimmable **lights** |
| Output 1–10 Level *(diagnostic)* | Real hardware brightness |
| Input Button 1–8 *(diagnostic)* | Physical wall-button state |
| Master | Module master **switch** |
| Tank 1 / Tank 2 | Water tank levels |
| Input Voltage / Current | Module input |
| Output Status *(diagnostic)* | Reports faults (fuse blown, over temp, etc.) |

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

## Troubleshooting

- **No control / can't switch anything.** You're in `LISTENONLY` mode. Switch to
  `NORMAL` to send commands.
- **History charts on the example dashboard are empty / error.** They need the
  **ApexCharts Card** from HACS. Install it.
- **Entity names look different in Home Assistant.** HA remembers any entity you
  rename in its own UI, which can drift from the names here.

The example Home Assistant dashboard is in
[`RedVision_TVMS_Dashboard.yaml`](RedVision_TVMS_Dashboard.yaml).

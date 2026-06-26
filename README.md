# RedVision / TVMS CAN Bridge

Bring a **REDARC RedVision / TVMS** RV power system into **Home Assistant** with
an ESP32 CAN bridge.

The bridge reads battery, charger, solar, tanks, temperatures, digital inputs and
output states. It also sends supported control commands for Manager30, TVMS
Rogue and TVMS1280 devices.

Protocol notes are in [PROTOCOL.md](PROTOCOL.md) and
[`RedVision_TVMS_components.dbc`](RedVision_TVMS_components.dbc).

## Hardware

The primary tested hardware is:

- M5Stack Atom Lite
- M5Stack Atomic CAN base using the CA-IS3050G transceiver
- ESPHome and Home Assistant

### CAN pins

| Signal | Atom pin |
|---|---|
| CAN TX | `GPIO22` |
| CAN RX | `GPIO19` |

### REDARC RJ45 pinout

| RJ45 pin | Signal |
|---:|---|
| 4 | CAN L |
| 5 | CAN H |
| 7 | 12–30 V supply |
| 8 | Ground |

Do not connect the REDARC supply directly to an ESP32 input. Use a correctly
rated and protected regulator or an isolated power design.

## Example configuration

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dev8edss/Redarc-reverse-engineering
      ref: dev
    refresh: 0s
    components:
      - redarc

redarc:
  canbus:
    tx_pin: GPIO22
    rx_pin: GPIO19
    bit_rate: 250KBPS
    mode: NORMAL

  host_address: 0xFF
  filter_interval: 5s
  history_poll_interval: 60s

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
      transition_length: 3s

  tvms_1280:
    - id: TVMS1280
      source_address: 0x24
```

Use `NORMAL` CAN mode for commands and active requests. `LISTENONLY` is suitable
only for passive monitoring.

## Device discovery

REDARC devices periodically broadcast identity DGN `0x1F404`. When an ESPHome
API client or log viewer connects, the bridge prints the devices it has found:

```text
Discovered devices:
  Device Type         Address       Serial No
  Manager30             1 (0x01)    ...
  BMS Battery Sensor    8 (0x08)    ...
  RedVision Display    32 (0x20)    ...
  TVMS Rogue           48 (0x30)    ...
```

`source_address` and `host_address` accept decimal or hexadecimal values.
Two devices cannot use the same source address.

## TVMS Rogue output capabilities

All ten Rogue outputs are Home Assistant `light` entities. The component does
not require a YAML list of dimmable outputs.

At startup it requests DGN `0x1FD0E` from the Rogue and reads the programmed
capability of each output:

- Dimmable output → ESPHome `BRIGHTNESS` light mode
- Non-dimmable output → ESPHome `ON_OFF` light mode

The internal command behavior changes immediately:

- Dimmable outputs accept brightness and `transition_length`.
- Non-dimmable outputs send only ON and OFF commands.
- DGN `0x1FD12` remains the actual hardware level/state feedback source.

Example startup messages:

```text
[I][redarc_tvms_rogue]: Requested Rogue dimming configuration
[I][redarc_tvms_rogue]: Output 2 changed to Dimmable
[I][redarc_tvms_rogue]: Output 10 changed to Dimmable
[I][redarc_tvms_rogue]: Updated 7 Rogue output capabilities; reconnect Home Assistant to refresh light controls
```

ESPHome changes the runtime light traits and current colour mode immediately.
Home Assistant normally refreshes the available brightness controls when its
ESPHome API connection reconnects or the integration is reloaded.

### Recheck Dimmable Outputs button

Each Rogue creates this diagnostic button:

```text
TVMS Rogue Recheck Dimmable Outputs
```

Its generated ESPHome ID is:

```cpp
id(TVMS_Rogue_recheck_dimmable_outputs)
```

Press it after changing output programming on the RedVision system. The bridge
requests `0x1FD0E` again, updates every changed runtime capability and logs the
changes. Reconnect Home Assistant afterward if a brightness slider needs to be
added or removed.

If the request gets no response, ESPHome logs:

```text
[W][redarc_tvms_rogue]: No Rogue dimming configuration response received
```

No response is expected in `LISTENONLY` mode because the bridge cannot transmit
the request.

### Rogue transition settings

`transition_length` is a Rogue-only setting and belongs inside the
`tvms_rogue` block:

```yaml
redarc:
  tvms_rogue:
    - id: TVMS_Rogue
      source_address: 0x30
      transition_length: 3s
      true_off_threshold: 1.5
```

Transitions apply only to outputs discovered as dimmable. During a fade, the
component sends changed integer percentages. During fade-out it sends a real OFF
command when the level reaches `true_off_threshold`.

## Home Assistant entities

### Manager30

- Output current and battery voltage
- Derived device current
- Solar current, voltage, power and energy history
- AC and vehicle input measurements
- Charging mode and charging stage
- CAN date/time and Set Time button

### Battery sensor

- Current, voltage, temperature and SOC
- Battery settings and alarm thresholds
- SOC history and calibration actions

### TVMS Rogue

- Output 1–10 light entities
- Actual Output 1–10 Level diagnostic sensors
- Input 1–8 diagnostic binary sensors
- Master switch
- Tank 1 and Tank 2
- Input voltage and current
- Output fault/status diagnostic
- Recheck Dimmable Outputs diagnostic button

### TVMS1280

- Output 1–10 switches
- Inverter and master switches
- Digital inputs
- Tanks, temperatures and voltage inputs
- Output fault/status diagnostic

## Stable ESPHome IDs

An explicit device `id:` becomes the prefix for generated child IDs:

```cpp
id(TVMS_Rogue_output_1)
id(TVMS_Rogue_output_1_level).state
id(TVMS_Rogue_input_8).state
id(TVMS_Rogue_recheck_dimmable_outputs).press()
id(TVMS1280_output_1).turn_on()
id(Manager30_solar_power).state
```

Renaming an entity in Home Assistant does not change its ESPHome compile-time ID.

## Development workflow

While testing changes from `dev`:

1. Keep `ref: dev` and `refresh: 0s`.
2. Run **Clean Build Files**, or clear `/data/external_components/*`.
3. Validate and compile.
4. Flash the bridge.
5. Check the startup dimming discovery and device-discovery logs.
6. Remove `refresh: 0s` or use a normal refresh interval afterward.

## Troubleshooting

- **No control or active configuration request:** use CAN `NORMAL` mode.
- **Brightness slider does not match the latest Rogue programming:** press
  **Recheck Dimmable Outputs**, then reconnect/reload the Home Assistant ESPHome
  integration.
- **No Rogue capability response:** confirm the Rogue `source_address`, bridge
  `host_address`, CAN wiring and transmit-capable mode.
- **Old YAML option errors or stale behavior:** clean ESPHome build files and the
  external-component cache.
- **Dashboard history cards fail:** install the ApexCharts Card if the supplied
  dashboard uses it.

The example dashboard is in [`RedVision_TVMS_Dashboard.yaml`](RedVision_TVMS_Dashboard.yaml).

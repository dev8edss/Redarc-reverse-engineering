# Redarc / RedVision ESPHome External Components v86

This package splits the RedVision / TVMS CAN bridge into ESPHome external components so the main YAML can stay mostly declarative. The YAML still owns the CAN bus and dispatches every extended CAN frame to the component hubs; each component then handles only the frames for its source device or command group.

## Components

| Component | Purpose |
|---|---|
| `redarc_common` | Shared CAN byte helpers, source-address helpers, and current/voltage decoding helpers. |
| `tvms_rouge` | TVMS Rouge dimmable output lights, ON/OFF commands, dim hold/ramp protocol, output level feedback, hardware-button/dimming-active binary sensors, input voltage, and tank sensors. |
| `tvms_1280` | TVMS1280 relay output switches, inverter switch, output feedback from RedVision/display changes, tank sensors, temperature sensors, supply voltage, and voltage-input diagnostic candidates. |
| `manager30` | Manager30 output current, battery voltage, source-derived device current, solar current/voltage/power, solar Wh/yield, and AC input voltage. |
| `battery_sensor` | Battery shunt current, voltage, SOC, and temperature. |
| `redvision_display` | RedVision display/rebroadcast current values for display/source comparison. |

## ESPHome 2026.5 compatibility notes

The current ESPHome API needs these external component schema names:

```python
canbus.CanbusComponent
switch.switch_schema(...)
```

Do **not** use the older Python-side names:

```python
canbus.Canbus
switch.SWITCH_SCHEMA
```

The C++ side can still use ESPHome's CAN bus C++ class/pointers where the component code already does so.

## External component YAML

During normal use:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dev8edss/Redarc-reverse-engineering
      ref: main
    components:
      - redarc_common
      - tvms_rouge
      - tvms_1280
      - manager30
      - battery_sensor
      - redvision_display
```

During active development, add `refresh: 0s` temporarily so ESPHome re-checks the git source every validation:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dev8edss/Redarc-reverse-engineering
      ref: main
    refresh: 0s
    components:
      - redarc_common
      - tvms_rouge
      - tvms_1280
      - manager30
      - battery_sensor
      - redvision_display
```

After the component has stabilised, remove `refresh: 0s` or use a gentler value such as `5min`.

If ESPHome validates against an old component schema, clear the cache:

```bash
rm -rf /data/external_components/*
```

## CAN bus setup

Current control-capable YAML uses `NORMAL` mode because ESPHome transmits TVMS1280 and TVMS Rouge commands:

```yaml
canbus:
  - platform: esp32_can
    id: rv_can
    tx_pin: GPIO22
    rx_pin: GPIO19
    can_id: 0x7FE
    use_extended_id: true
    bit_rate: 250KBPS
    mode: NORMAL
    rx_queue_len: 64
    on_frame:
      - can_id: 0x00000000
        can_id_mask: 0x00000000
        use_extended_id: true
        remote_transmission_request: false
        then:
          - lambda: |-
              const uint32_t id_rx = can_id & 0x1FFFFFFFUL;
              id(tvms_rouge_hub).handle_can_frame(id_rx, x);
              id(tvms1280_hub).handle_can_frame(id_rx, x);
              id(manager30_hub).handle_can_frame(id_rx, x);
              id(battery_sensor_hub).handle_can_frame(id_rx, x);
              id(redvision1_hub).handle_can_frame(id_rx, x);
              id(redvision2_hub).handle_can_frame(id_rx, x);
```

Use `LISTENONLY` only for passive monitoring YAMLs where nothing is transmitted.

## TVMS Rouge

### Output channel map

| Output | Channel |
|---:|---:|
| 1 | `0x0C` |
| 2 | `0x0D` |
| 3 | `0x0E` |
| 4 | `0x0F` |
| 5 | `0x10` |
| 6 | `0x11` |
| 7 | `0x12` |
| 8 | `0x13` |
| 9 | `0x14` |
| 10 | `0x15` |

### Command and feedback frames

| Purpose | CAN ID | Decode/use |
|---|---:|---|
| ON/OFF command | `0x0F003020` | `CB 00 FF <channel> <00 off / 01 on> 00 00 00` |
| Dim hold command | `0x0F053020` | `<channel> 01 <01 down / 64 up> 05 00 FF FF FF` |
| Dim release | `0x0F053020` | `<channel> 01 FF 00 00 FF FF FF` |
| Keepalive during dim hold | `0x0FE6FF20` | `FF FF FF FF FF FF FF FF` |
| Actual output level | `0x1BFD1230` | `D1=base channel`, `D2-D8=0-100% levels for base+0..base+6` |
| Hardware button / dim-active | `0x1BFD1430` | `D1=base channel`, `D2-D8`; `0x02=active`, `0x00=inactive`, ignore other values |
| Coarse output status | `0x1BFD0030` | `D1=base channel`, `D2-D8`; `0x00=off`, `0x01=on`, `0xF8/0xFF/special values=ignore/unknown` |
| Tank feedback | `0x1BFD0230` | Tank 1 and Tank 2 percentage feedback |
| Input/status voltage | `0x13F10830` | `D1-D2` little-endian, scale `0.01 V/count`; examples `F0 04 = 12.64 V`, `EC 04 = 12.60 V` |

### Current dimming behaviour

Rouge dimming is working as a timed hold/ramp protocol, not as an absolute brightness command. Home Assistant sends a target brightness, the component estimates the required hold time, sends dim-up or dim-down holds, then releases.

Current UX behaviour:

- `1BFD1230` remains the source of truth for real hardware brightness.
- The HA light entity keeps the requested target while an HA-driven dim operation is active, so it no longer jumps back to the old feedback value mid-ramp.
- The diagnostic `Output X Level` sensor still tracks the actual hardware level live while the output ramps.
- Physical Rouge hardware buttons can turn an output on at its remembered dim level, for example `0% -> 85%` or `0% -> 70%`, but the known CAN ON command still turns outputs on at `100%` before dimming down.

Recommended Rouge YAML options:

```yaml
tvms_rouge:
  id: tvms_rouge_hub
  canbus_id: rv_can
  output_command_id: 0x0F003020
  dim_command_id: 0x0F053020
  keepalive_id: 0x0FE6FF20
  level_feedback_id: 0x1BFD1230
  tank_feedback_id: 0x1BFD0230
  button_status_id: 0x1BFD1430
  input_status_id: 0x13F10830

  true_off_threshold: 1.0
  target_debounce: 600ms
  start_deadline: 2500ms
  default_deadband: 3.0
  initial_rate_ms_per_percent: 50.0
  learning_gain: 0.25
  approach: 0.80
  max_pulse: 1200ms
  settle_time: 700ms
  max_iterations: 12

  input_voltage:
    name: "TVMS Rouge Input Voltage"
    unit_of_measurement: "V"
    device_class: voltage
    state_class: measurement
    accuracy_decimals: 2

  button_states:
    - name: "TVMS Rouge Output 1 Button Active"
    - name: "TVMS Rouge Output 2 Button Active"
    - name: "TVMS Rouge Output 3 Button Active"
    - name: "TVMS Rouge Output 4 Button Active"
    - name: "TVMS Rouge Output 5 Button Active"
    - name: "TVMS Rouge Output 6 Button Active"
    - name: "TVMS Rouge Output 7 Button Active"
    - name: "TVMS Rouge Output 8 Button Active"
    - name: "TVMS Rouge Output 9 Button Active"
    - name: "TVMS Rouge Output 10 Button Active"
```

If `button_status_id`, `button_states`, `input_status_id`, or `input_voltage` are rejected as invalid YAML options, ESPHome is loading an older `tvms_rouge` schema. Push the latest component code and clear `/data/external_components/*`.

## TVMS1280

### Output channel map

The project now labels TVMS1280 relay outputs as `0-9` to match the zero-based component configuration.

| Output label | Channel | Status |
|---:|---:|---|
| Output 0 | `0x04` | assumed sequence |
| Output 1 | `0x05` | assumed sequence |
| Output 2 | `0x06` | assumed sequence |
| Output 3 | `0x07` | assumed sequence |
| Output 4 | `0x08` | observed in feedback group |
| Output 5 | `0x09` | confirmed toggled |
| Output 6 | `0x0A` | confirmed toggled |
| Output 7 | `0x0B` | assumed sequence |
| Output 8 | `0x0C` | assumed sequence |
| Output 9 | `0x0D` | assumed sequence |
| Inverter | `0x0E` | observed/confirmed command pattern |

### Command and feedback frames

| Purpose | CAN ID | Decode/use |
|---|---:|---|
| ON/OFF command | `0x0F002420` | `CB 00 FF <channel> <00 off / 01 on> 00 00 00` |
| Output feedback | `0x1BFD0024` | `D1=base channel`, `D2-D8=states for base+0..base+6`; `0x00=off`, `0x01=on`, `0xF8/0xFF=ignore` |
| Temp/tank/status | `0x1BFD0224` | Muxed temp/tank/status frame used for temperatures, tanks, and voltage-input diagnostic candidates |
| Channel/status companion | `0x1BFCF024` | Partly decoded/status only; not the primary HA state source |
| Supply/input status | `0x13F10824` | `D1-D2` little-endian, scale `0.01 V/count`; interpreted as TVMS1280 supply voltage / Voltage Input 2 |

Recommended TVMS1280 voltage diagnostic YAML options:

```yaml
tvms_1280:
  input_status_id: 0x13F10824

  supply_voltage:
    name: "TVMS1280 Supply Voltage"
    unit_of_measurement: "V"
    device_class: voltage
    state_class: measurement
    accuracy_decimals: 2
    entity_category: diagnostic

  voltage_input_1:
    name: "TVMS1280 Voltage Input 1 Candidate"
    unit_of_measurement: "V"
    device_class: voltage
    state_class: measurement
    accuracy_decimals: 1
    entity_category: diagnostic

  voltage_input_2:
    name: "TVMS1280 Voltage Input 2 Candidate"
    unit_of_measurement: "V"
    device_class: voltage
    state_class: measurement
    accuracy_decimals: 1
    entity_category: diagnostic
```

Current diagnostic interpretation:

- `13F10824 D1-D2 / 100` is TVMS1280 supply voltage / Voltage Input 2 and appears around `12.64 V`.
- `1BFD0224` mux/page `0x11`, `D2 / 10` is a candidate for Voltage Input 1 and should read close to `0.0 V` if disconnected.
- `1BFD0224` mux/page `0x14`, `D2 / 10` is a candidate voltage/status field that has appeared around `11.9-12.0 V`.
- The voltage-input candidates overlap the existing temp/tank/status PGN and should remain diagnostic until confirmed against the RedVision app/display.

## Manager30

| Purpose | CAN ID | Decode/use |
|---|---:|---|
| Output/load current and Manager battery voltage | `0x03F20A01` | Current `D1-D4 raw/1000-1000`; voltage `D5-D6 * 0.001 V` |
| Solar current/voltage/power source | `0x03F20801` | Solar current and voltage decode; ESPHome publishes exact live power as current × voltage |
| AC input voltage | `0x03F20401` | `D5-D6` little-endian, `1 V/count`; confirmed mains/240 V style signal |
| Solar energy/yield | `0x03FCD601` | Decode only when `D1 == 0x00`; `D2-D5` little-endian uint32 = Wh; capture showed `14 -> 19 Wh` |

Recommended solar Wh YAML option:

```yaml
manager30:
  solar_energy:
    name: "Solar Energy"
    unit_of_measurement: "Wh"
    device_class: energy
    state_class: total_increasing
    accuracy_decimals: 0
```

## Battery sensor

| Purpose | CAN ID | Decode/use |
|---|---:|---|
| Battery current/voltage/temp | `0x13F10208` | Current `D1-D4 raw/1000-1000`; voltage `D5-D6 * 0.001 V`; temperature `D7 - 60` |
| Battery SOC | `0x13F10408` | `D1 = %` |

## RedVision display rebroadcasts

| Purpose | CAN ID | Decode/use |
|---|---:|---|
| RedVision 1 battery/device current display | `0x13F28020` | 16-bit display current fields centred at 10000: `raw/10 - 1000` |
| RedVision 2 manager output current display | `0x13F28221` | 16-bit display current field centred at 10000: `raw/10 - 1000` |

## RJ45 pinout

| RJ45 pin | Function | Status |
|---:|---|---|
| 4 | CAN L | confirmed |
| 5 | CAN H | confirmed |
| 8 | Ground | confirmed |
| unknown | 24 V supply | unconfirmed |

Do not assume the 24 V pin until verified with a meter or wiring reference.

## Development checklist

After applying component patches to the GitHub repo:

1. Commit and push the repo.
2. In ESPHome/Home Assistant terminal, clear the external component cache:

   ```bash
   rm -rf /data/external_components/*
   ```

3. Keep `refresh: 0s` only while testing.
4. Validate, compile, and flash.
5. After testing, remove `refresh: 0s` or set a normal refresh interval.

## Known limitations / next targets

- The known CAN ON command for TVMS Rouge turns an output on at `100%`; hardware Rouge buttons can recall a saved dim level, but the external CAN command for “turn on at remembered brightness” has not been identified.
- TVMS1280 voltage input 1/2 candidates are diagnostic until confirmed against the RedVision UI or direct wiring tests.
- Some status/config PGNs remain observed-only and should not be used for HA entities until tested.

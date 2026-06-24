# Redarc / RedVision ESPHome External Components v86

This package ships the RedVision / TVMS CAN bridge as a **single** ESPHome external component, `redarc`. The component owns the CAN bus and the Home Assistant time source internally (configured under `redarc: canbus:` / `redarc: time:`), subscribes to the bus on boot, and dispatches every extended CAN frame to the device sub-blocks; each nested device handles only the frames for its source device or command group. No separate `base.yaml`, top-level `canbus:`/`time:`, or `on_frame:` automation is needed.

## Component and its device sub-blocks

The one `redarc:` component holds the shared bus/dispatcher settings plus a nested sub-block per device:

| Sub-block | Purpose |
|---|---|
| (top level) | `host_address`, `discovery_delay`, shared `filter_interval` / `history_poll_interval` / `transition_length`; CAN byte/decode helpers and startup device discovery. |
| `canbus` | The `esp32_can` CAN interface, built internally (same options as ESPHome's `esp32_can`). Or reference an external bus with `canbus_id`. |
| `time` | Optional internal Home Assistant time source for the Manager Set Time button. |
| `tvms_rogue` | TVMS Rogue dimmable output lights, direct set-level and OFF commands, output level feedback, physical input/button binary sensors, input voltage, candidate input current, and tank sensors. |
| `tvms_1280` | TVMS1280 relay output switches, inverter switch, output feedback from RedVision/display changes, tank sensors, temperature sensors, supply voltage, voltage-input diagnostic candidates, and notes for the 3 unused digital inputs. |
| `manager` | Manager30 output current, battery voltage, source-derived device current, solar current/voltage/power, solar Wh/yield, AC input current/voltage, charging mode select, and charging stage text. |
| `battery_sensor` | Battery shunt current, voltage, SOC, and temperature. |
| `redvision_display` | RedVision display/rebroadcast current values for display/source comparison (list — one entry per display). |

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
      - redarc
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
      - redarc
```

After the component has stabilised, remove `refresh: 0s` or use a gentler value such as `5min`.

If ESPHome validates against an old component schema, clear the cache:

```bash
rm -rf /data/external_components/*
```

## CAN bus setup

The CAN bus is configured **inside** the component as `redarc: canbus:` — there is
no separate top-level `canbus:` block and no `on_frame:` automation. The options
are exactly ESPHome's `esp32_can` platform options, so anything `esp32_can`
supports (pins, `bit_rate`, `mode`, `rx_queue_len`, …) can be set here:

```yaml
redarc:
  canbus:
    tx_pin: GPIO22
    rx_pin: GPIO19
    bit_rate: 250KBPS
    mode: NORMAL          # NORMAL to transmit commands; LISTENONLY for monitor-only
    rx_queue_len: 64
  # ... time:, host_address:, and the device sub-blocks ...
```

The component subscribes to the bus on boot and dispatches every received frame
to the device listeners itself, so no `on_frame:` lambda is needed. `can_id`
defaults to `0x7FE` and `use_extended_id` to `true`. Use `LISTENONLY` only for
passive monitoring builds where nothing is transmitted.

To reference an externally-declared `canbus:` instead of the internal one, omit
`redarc: canbus:` and set `redarc: canbus_id: <id>` (exactly one of the two is
required).

## TVMS Rogue

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
| Set output level | `0x0F003020` | `5A 01 FF <channel> <percent 00-64> 00 00 00`; example output 4 at 65% is `5A 01 FF 0F 41 00 00 00` |
| ON command | `0x0F003020` | `CB 00 FF <channel> 01 00 00 00` |
| OFF command | `0x0F003020` | `CB 00 FF <channel> 00 00 00 00` |
| Actual output level | `0x1BFD1230` | `D1=base channel`, `D2-D8=0-100% levels for base+0..base+6` |
| Physical input/button state | `0x1BFD0030` | `D1=0x01` maps `D2-D8` to inputs/buttons 1-7; `D1=0x08` maps `D2` to input/button 8; `0x00=inactive`, `0x01=active` |
| Output button / dim-active | `0x1BFD1430` | Output/dimming activity only; stayed idle during physical input/button testing |
| Tank feedback | `0x1BFD0230` | Tank 1 and Tank 2 percentage feedback |
| Input/status voltage | `0x13F10830` | `D1-D2` little-endian, scale `0.01 V/count`; examples `F0 04 = 12.64 V`, `EC 04 = 12.60 V` |

### Current dimming behavior

Rogue dimming now uses the absolute set-level command observed from the RedVision display. Home Assistant sends a target brightness and the component sends one direct `0x0F003020` frame with the requested percent; full-on requests use the basic ON command.

Current UX behaviour:

- `1BFD1230` remains the source of truth for real hardware brightness.
- The HA light entity publishes the requested target immediately, then reconciles from hardware feedback.
- The diagnostic `Output X Level` sensor tracks the actual hardware level from `1BFD1230`.
- The old timed dim-hold/ramp protocol is no longer used by this component.

Recommended Rogue YAML options:

```yaml
tvms_rogue:
  id: tvms_rogue_hub
  canbus_id: rv_can
  output_command_id: 0x0F003020
  level_feedback_id: 0x1BFD1230
  tank_feedback_id: 0x1BFD0230
  button_status_id: 0x1BFD0030
  input_status_id: 0x13F10830

  true_off_threshold: 1.0

  input_voltage:
    name: "TVMS Rogue Input Voltage"
    unit_of_measurement: "V"
    device_class: voltage
    state_class: measurement
    accuracy_decimals: 2

  button_states:
    - name: "TVMS Rogue Input Button 1"
    - name: "TVMS Rogue Input Button 2"
    - name: "TVMS Rogue Input Button 3"
    - name: "TVMS Rogue Input Button 4"
    - name: "TVMS Rogue Input Button 5"
    - name: "TVMS Rogue Input Button 6"
    - name: "TVMS Rogue Input Button 7"
    - name: "TVMS Rogue Input Button 8"
```

If `button_status_id`, `button_states`, `input_status_id`, or `input_voltage` are rejected as invalid YAML options, ESPHome is loading an older `tvms_rogue` schema. Push the latest component code and clear `/data/external_components/*`.


### TVMS Rogue input current candidate

The Rogue label pages identify:

```text
0x16 = Input Voltage
0x17 = Input Current
```

The current candidate decode uses the grouped Rogue `0x1BFD0230` status frame:

```text
CAN ID: 0x1BFD0230
When D1/base = 0x16:
  D2 = item 0x16 Input Voltage candidate, scale still secondary to 0x13F10830
  D3 = item 0x17 Input Current candidate, D3 / 10 = A
```

Observed examples include:

```text
1BFD0230  16 C2 2F FF FF FF FF FF
1BFD0230  16 C1 2F FF FF FF FF FF
1BFD0230  16 BC 2F FF FF FF FF FF
```

`0x2F / 10 = 4.7 A`. This is exposed as a diagnostic/candidate sensor until confirmed with a controlled Rogue input-current change test.

## TVMS1280

### Output channel map

The project labels TVMS1280 relay outputs as `1-10` (channels are still the zero-based `0x04`–`0x0D`).

| Output label | Channel | Status |
|---:|---:|---|
| Output 1 | `0x04` | assumed sequence |
| Output 2 | `0x05` | assumed sequence |
| Output 3 | `0x06` | assumed sequence |
| Output 4 | `0x07` | assumed sequence |
| Output 5 | `0x08` | observed in feedback group |
| Output 6 | `0x09` | confirmed toggled |
| Output 7 | `0x0A` | confirmed toggled |
| Output 8 | `0x0B` | assumed sequence |
| Output 9 | `0x0C` | assumed sequence |
| Output 10 | `0x0D` | assumed sequence |
| Inverter | `0x0E` | observed/confirmed command pattern |

### Command and feedback frames

| Purpose | CAN ID | Decode/use |
|---|---:|---|
| ON/OFF command | `0x0F002420` | `CB 00 FF <channel> <00 off / 01 on> 00 00 00` |
| Output feedback | `0x1BFD0024` | `D1=base channel`, `D2-D8=states for base+0..base+6`; `0x00=off`, `0x01=on`, `0xF8/0xFF=ignore` |
| Temp/tank/status | `0x1BFD0224` | Muxed temp/tank/status frame used for temperatures, tanks, and voltage inputs |
| Channel/status companion | `0x1BFCF024` | Partly decoded/status only; not the primary HA state source |
| Static input/status candidate | `0x13F10824` | Not used as the live TVMS1280 voltage input source |

Recommended TVMS1280 voltage diagnostic YAML options:

```yaml
tvms_1280:
  voltage_input_1:
    name: "TVMS1280 Voltage Input 1"
    unit_of_measurement: "V"
    device_class: voltage
    state_class: measurement
    accuracy_decimals: 1
    entity_category: diagnostic

  voltage_input_2:
    name: "TVMS1280 Voltage Input 2"
    unit_of_measurement: "V"
    device_class: voltage
    state_class: measurement
    accuracy_decimals: 1
    entity_category: diagnostic
```

Current voltage-input interpretation:

- TVMS1280 Voltage Input 1 is item/channel `0x11` on `1BFD0224` mux/page `0x11`, `D2-D3` little-endian millivolts.
- TVMS1280 Voltage Input 2 is item/channel `0x12` on the same page, `D4-D5` little-endian millivolts.
- `13F10824` is not used as the live TVMS1280 voltage input source.

### Digital inputs

- TVMS1280 hardware has **3 digital inputs**.
- Channel map for testing: digital inputs are `0x01`, `0x02`, and `0x03`; outputs are `0x04`-`0x0D`; inverter is `0x0E`.
- ESPHome exposes candidate Home Assistant binary sensors for the 3 digital input states using `0x1BFD0024` channel feedback.
- Do not treat these as Rogue-style hardware button inputs. Rogue physical input/button state is `0x1BFD0030`; `0x1BFD1430` is only output/dimming activity.
- A one-at-a-time input toggle capture is still needed before marking the live-state decode confirmed.

## Manager30

| Purpose | CAN ID | Decode/use |
|---|---:|---|
| Output/load current and Manager battery voltage | `0x03F20A01` | Current `D1-D4 raw/1000-1000`; voltage `D5-D6 * 0.001 V` |
| Solar current/voltage/power source | `0x03F20801` | Solar current and voltage decode; ESPHome publishes exact live power as current × voltage |
| AC input current | `0x03F20401` | `D1-D4` little-endian, `raw/1000 - 1000 A` (0.001 A resolution); preferred over the display rebroadcast `0x1F282` which is rounded to 0.1 A |
| AC input voltage | `0x03F20401` | `D5-D6` little-endian, `1 V/count`; confirmed mains/240 V style signal |
| Solar energy/yield | `0x03FCD601` | Decode only when `D1 == 0x00`; `D2-D5` little-endian uint32 = Wh; capture showed `14 -> 19 Wh` |
| Charging mode status | `0x03F10801` | `D1 bit0`: `0=Touring`, `1=Storage`; preferred status source |
| Charging stage/status | `0x03F20001` | Stage `= D1 & 0xFE`: `0x00=Not Charging`, `0x10=Desulphation`, `0x20=Soft-start`, `0x30=Boost`, `0x40=Absorption`, `0x50=Battery Test`, `0x60=Equalize`, `0x70=Float`, `0x80=Maintenance`. `D1 bit0` = mode (`0=Touring`, `1=Storage`), used as a fallback mode source when `0x1F108` has not been seen |
| Charging mode command | `0x0F00FF20` | RedVision/source `0x20`, `43 00 FF FF <mode> 00 00 00`; `<mode>` is `0x00=Touring`, `0x01=Storage` |
| Charge-cycle request candidate | `0x0F00FF20` | `4D 00 FF FF 01 00 00 00`; likely force boost / restart charge cycle, kept candidate until more tests |

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

- TVMS Rogue brightness uses the direct set-level command `5A 01 FF <channel> <percent> 00 00 00`; the older timed dim-hold/ramp command path is no longer used.
- TVMS1280 voltage input 1/2 candidates are diagnostic until confirmed against the RedVision UI or direct wiring tests.
- Some status/config PGNs remain observed-only and should not be used for HA entities until tested.

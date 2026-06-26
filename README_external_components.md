# Redarc / RedVision ESPHome External Component

This repository ships the RedVision / TVMS CAN bridge as one ESPHome external
component named `redarc`.

The component owns or references the CAN bus, creates the Home Assistant time
source used by Manager30 clock commands, receives every extended CAN frame, and
dispatches frames to nested device blocks. No YAML `on_frame:` automation is
required.

## Component structure

```yaml
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

  tvms_rogue:
    - id: TVMS_Rogue
      source_address: 0x30
      dimmable_outputs: [2, 3, 4, 5, 6, 7, 10]
      transition_length: 3s

  tvms_1280:
    - id: TVMS1280
      source_address: 0x24
```

| Sub-block | Purpose |
|---|---|
| Top level | Shared `host_address`, `filter_interval`, `history_poll_interval`, CAN setup and passive device discovery |
| `canbus` | Internal `esp32_can` interface; alternatively use `canbus_id` for an externally declared CAN platform |
| `time` | Internal Home Assistant time source; enabled by default, `time: false` disables it and the Manager Set Time button |
| `battery_sensor` | Battery current, voltage, temperature, SOC, configuration and history |
| `manager` | Manager30 charging, solar, AC, vehicle input, history and clock functions |
| `redvision_display` | RedVision display/rebroadcast values; accepts a list for multiple displays |
| `tvms_rogue` | Rogue light outputs, dimming configuration, master, inputs, tanks, voltage/current and output diagnostics |
| `tvms_1280` | Relay outputs, inverter, master, digital inputs, tanks, temperatures and voltage inputs |

`transition_length` is not a top-level option. It belongs only in a
`tvms_rogue` entry.

## External-component source

Stable branch:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dev8edss/Redarc-reverse-engineering
      ref: main
    components:
      - redarc
```

Development branch:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dev8edss/Redarc-reverse-engineering
      ref: dev
    refresh: 0s
    components:
      - redarc
```

Use `refresh: 0s` only while testing. If ESPHome continues to validate against an
old schema, use **Clean Build Files** or remove the external-component cache:

```bash
rm -rf /data/external_components/*
```

## ESPHome compatibility

The current Python component uses ESPHome's current schema classes, including:

```python
canbus.CanbusComponent
switch.switch_schema(...)
```

Older names such as `canbus.Canbus` and `switch.SWITCH_SCHEMA` must not be used.

## CAN bus setup

### Internal ESP32 CAN

The normal M5Stack Atom CAN-base configuration is nested inside `redarc:`:

```yaml
redarc:
  canbus:
    tx_pin: GPIO22
    rx_pin: GPIO19
    bit_rate: 250KBPS
    can_id: 0x7FE
    use_extended_id: true
    mode: NORMAL
    rx_queue_len: 64
```

Use `NORMAL` when commands, configuration requests or history requests must be
transmitted. `LISTENONLY` is suitable only for passive monitoring.

### External CAN platforms

An external platform such as MCP2515 can be declared normally and referenced by
`canbus_id`:

```yaml
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

canbus:
  - platform: mcp2515
    id: rv_can
    cs_pin: GPIO5
    can_id: 0x7FE
    use_extended_id: true
    bit_rate: 250KBPS

redarc:
  canbus_id: rv_can
```

MCP2515 support remains untested on physical hardware.

## Device discovery and logger-connected actions

The bridge passively collects DGN `0x1F404` identity broadcasts. When an ESPHome
API client or log viewer connects, it prints the discovered-device table after a
short delay:

```text
Discovered devices:
  Device Type         Address       Serial No
  Manager30             1 (0x01)    ...
  BMS Battery Sensor    8 (0x08)    ...
  RedVision Display    32 (0x20)    ...
  TVMS Rogue           48 (0x30)    ...
```

The same logger-connected event notifies device components that need an active
validation action. A configured Rogue uses this event to request its output
configuration and compare it with YAML.

## TVMS Rogue

### YAML configuration

```yaml
redarc:
  tvms_rogue:
    - id: TVMS_Rogue
      source_address: 0x30
      dimmable_outputs: [2, 3, 4, 5, 6, 7, 10]
      transition_length: 3s
      true_off_threshold: 1.5
```

Rogue-specific settings:

| Setting | Default | Purpose |
|---|---:|---|
| `source_address` | `0x30` | Rogue CAN source address |
| `host_address` | inherited | Bridge source address used for commands and requests |
| `filter_interval` | inherited | Throttle for slow diagnostic sensors |
| `dimmable_outputs` | `[]` | Output numbers `1`–`10` that expose brightness controls |
| `transition_length` | `0s` | ESPHome transition duration for listed dimmable outputs |
| `true_off_threshold` | `1.5` | Percentage at or below which a dimmable output is sent a real OFF command |

Duplicate entries in `dimmable_outputs` are rejected. Values outside `1`–`10`
are rejected.

### Home Assistant entity behaviour

All ten Rogue channels remain Home Assistant `light` entities:

- Listed outputs use the `BRIGHTNESS` light mode.
- Unlisted outputs use the `ON_OFF` light mode and have no brightness slider.
- Unlisted outputs never send the absolute-level command; they send only Rogue
  ON and OFF commands.
- `transition_length` applies only to listed dimmable outputs.
- The diagnostic `Output N Level` sensor continues to publish the real CAN level
  for every output.

This keeps one consistent `light.tvms_rogue_output_N` style domain while avoiding
brightness controls on outputs programmed as non-dimmable.

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

### Commands and feedback

The exact CAN identifier includes the configured Rogue and host addresses. The
examples below use Rogue source `0x30` and RedVision host `0x20` where applicable.

| Purpose | CAN ID / DGN | Decode/use |
|---|---:|---|
| Set output level | command group `0x0F00` | `5A 01 FF <channel> <percent 00-64> 00 00 00` |
| ON command | command group `0x0F00` | `CB 00 FF <channel> 01 00 00 00` |
| OFF command | command group `0x0F00` | `CB 00 FF <channel> 00 00 00 00` |
| Actual output level | DGN `0x1FD12` | `D1=base channel`, following bytes are `0`–`100%` levels |
| Input/master/output status | DGN `0x1FD00` | Paginated channel status and fault values |
| Tank/input electrical values | DGN `0x1FD02` | Tank page and grouped input voltage/current page |
| Output configuration | DGN `0x1FD0E` | One record per output when explicitly requested |
| Output/dimming activity | DGN `0x1FD14` | Runtime activity only; not used as the static dimming capability source |

### Transition behaviour

For dimmable outputs, ESPHome's transition engine updates the light's interpolated
`current_values`. The component sends changed integer percentage steps to the
Rogue and deduplicates repeated percentages. During fade-out it sends stepped
levels until `true_off_threshold`, followed by one real OFF command.

CAN level feedback does not replace the final Home Assistant target while an
ESPHome transition is active. After the transition completes, DGN `0x1FD12`
remains authoritative for actual hardware state and brightness.

For non-dimmable outputs, transitions and percentage commands are bypassed.

### Dimming configuration validation

When a log viewer connects, the Rogue component sends a configuration request:

```text
Request CAN ID: 0x0F03<rogue address><host address>
Request data:   0E FD 00 00 00 00 00 00
```

For Rogue `0x30` and bridge host `0xFF`, this is:

```text
0F0330FF  0E FD 00 00 00 00 00 00
```

The Rogue replies with DGN `0x1FD0E` records. The validator maps D1 channels
`0x0C`–`0x15` to Outputs 1–10 and currently interprets D2 bit 7 as the programmed
dimmable flag.

The request is used only to validate YAML. It does not dynamically alter light
traits because Home Assistant entity capabilities are selected at compile time.

Matching configuration:

```text
[I][redarc_tvms_rogue]: Rogue dimming configuration matches YAML for 10 reported outputs
```

Per-output mismatch:

```text
[I][redarc_tvms_rogue]: Output 1 dimming mismatch: YAML=Non-Dimmable Rogue=Dimmable
[I][redarc_tvms_rogue]: Output 10 dimming mismatch: YAML=Dimmable Rogue=Non-Dimmable
```

Mismatch lines are informational. Missing or incomplete responses remain warnings:

```text
[W][redarc_tvms_rogue]: No DGN 0x1FD0E response; Rogue dimming configuration could not be validated
[W][redarc_tvms_rogue]: Received Rogue configuration for 7/10 outputs; validation is incomplete
```

No response is expected in `LISTENONLY` mode because the request cannot be sent.

### Rogue input electrical values

DGN `0x1FD02`, page/item `0x16`, is currently decoded as:

```text
D2-D3 little-endian / 1000 = input voltage in volts
D4-D5 little-endian / 1000 = input current in amps
```

These values are exposed as Rogue Input Voltage and Rogue Input Current sensors.

### Rogue output status values

DGN `0x1FD00` output status values currently include:

| Value | Meaning |
|---:|---|
| `0x00` | Off |
| `0x01` | On |
| `0x06` | Fuse blown |
| `0x0A` | Over temperature |
| `0x14` | Off override |
| `0x15` | On override |
| `0xF8` | Unconfigured |
| `0xFF` | No data |

These runtime values must not be used as the static dimming-capability flag.

## TVMS1280

The project labels TVMS1280 relay outputs as Outputs 1–10. Their channels are
`0x04`–`0x0D`; the inverter is channel `0x0E`.

| Purpose | DGN / command | Decode/use |
|---|---:|---|
| Output ON/OFF command | command group `0x0F00` | `CB 00 FF <channel> <00/01> 00 00 00` |
| Output/input/master feedback | DGN `0x1FD00` | Paginated channel-state array |
| Tank/temperature/voltage pages | DGN `0x1FD02` | Multiplexed diagnostic pages |
| Inverter companion status | DGN `0x1FCF0` | Inverter feedback path |

Effective output-state handling recognises normal ON/OFF plus override/master-off
states. Local commands are published optimistically and then reconciled from CAN
feedback. Digital inputs publish only when their state changes.

## Manager30

| Purpose | DGN / command | Decode/use |
|---|---:|---|
| Output current and battery voltage | DGN `0x1F20A` | Manager output/load values |
| Solar current and voltage | DGN `0x1F208` | Live solar values; power is current × voltage |
| AC and vehicle input candidates | DGN `0x1F204` | Input current/voltage fields |
| Solar energy/yield | DGN `0x1FCD6` | Wh history/yield data |
| Charging stage and mode | DGN `0x1F200` | Stage `D1 & 0xFE`; mode `D1 bit0`, `0=Touring`, `1=Storage` |
| Charging-mode command | command `0x43` | Mode byte `0=Touring`, `1=Storage` |

DGN `0x1F108` is not used as the authoritative charging-mode source because it
can remain fixed and overwrite valid changes. Charging mode is updated from the
`0x43` command path and DGN `0x1F200`.

Confirmed charging-stage bases:

| Base | Stage |
|---:|---|
| `0x00` | Not Charging |
| `0x10` | Desulphation |
| `0x20` | Soft-start |
| `0x30` | Boost |
| `0x40` | Absorption |
| `0x50` | Battery Test |
| `0x60` | Equalize |
| `0x70` | Float |
| `0x80` | Maintenance |

## Battery sensor

| Purpose | DGN | Decode/use |
|---|---:|---|
| Current, voltage and temperature | `0x1F102` | Core shunt readings |
| SOC | `0x1F104` | State of charge percentage |

Battery history sensors avoid forced duplicate updates and publish when new data
is received.

## RedVision display rebroadcasts

The display component handles DGN `0x1F280` and `0x1F282` independently, with a
separate throttle timestamp for each page. This prevents one page from suppressing
the other and allows the first valid page to publish immediately.

## Stable generated entity IDs

When a device block has an explicit `id:`, generated child IDs use that prefix:

```text
id(TVMS_Rogue_output_1)
id(TVMS_Rogue_output_1_level).state
id(TVMS_Rogue_input_8).state
id(TVMS1280_output_1).turn_on()
id(Manager30_solar_power).state
```

Renaming the Home Assistant entity in the HA UI does not change the ESPHome
compile-time ID.

## RJ45 pinout

Measured REDARC RJ45 signals:

| RJ45 pin | Function | Status |
|---:|---|---|
| 4 | CAN L | Confirmed |
| 5 | CAN H | Confirmed |
| 7 | 12–30 V supply | User-confirmed for current hardware design |
| 8 | Ground | Confirmed |

Do not connect bus voltage directly to an ESP32 input. Use a correctly rated,
protected regulator or isolated power design.

## Development checklist

After changing the component on `dev`:

1. Commit and push the repository.
2. Keep the device's external component on `ref: dev`.
3. Run **Clean Build Files** or clear `/data/external_components/*`.
4. Validate and compile.
5. Flash and open the ESPHome logs.
6. Confirm the discovered-device table and Rogue dimming validation output.
7. Remove `refresh: 0s` or select a normal refresh interval after testing.

## Current limitations

- Full ESPHome compilation is not performed by repository-side edits; validate
  changes with the target ESPHome version before moving `dev` to `main`.
- Rogue dimming capability is configured in YAML. DGN `0x1FD0E` validates the
  list but cannot change entity traits at runtime.
- The `0x1FD0E` validation requires transmit-capable `NORMAL` CAN mode.
- External CAN platforms such as MCP2515 remain untested on real hardware.

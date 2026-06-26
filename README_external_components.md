# Redarc / RedVision ESPHome External Component

The repository exposes one ESPHome external component named `redarc`. It owns or
references the CAN bus, creates the shared time source, receives extended CAN
frames and dispatches them to nested device components.

No YAML `on_frame:` automation is required.

## Source configuration

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

Use **Clean Build Files** after schema or C++ changes. The external-component
cache can also be removed manually:

```bash
rm -rf /data/external_components/*
```

## Current nested schema

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
      transition_length: 3s
      true_off_threshold: 1.5

  tvms_1280:
    - id: TVMS1280
      source_address: 0x24
```

The previous `dimmable_outputs` option has been removed. Rogue capability is now
read from the module at runtime.

`transition_length` is not a shared top-level setting. It belongs only inside a
`tvms_rogue` entry.

## CAN setup

The internal `redarc: canbus:` block builds an `esp32_can` interface. For an
external CAN platform, declare it normally and reference it with `canbus_id`:

```yaml
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

External MCP2515 hardware remains untested.

Use `NORMAL` mode for control and active requests. `LISTENONLY` cannot request
Rogue capabilities, history pages or other command-driven data.

## Device discovery

The common component passively records DGN `0x1F404` identity broadcasts. When
an API client connects it prints a device/address table. This behavior is
independent of Rogue output-capability discovery.

## TVMS Rogue

### Runtime capability discovery

All ten outputs are compiled as light entities. They initially use ON/OFF mode.
One second after component setup, the Rogue sends this request:

```text
CAN ID: 0x0F03<rogue address><host address>
Data:   0E FD 00 00 00 00 00 00
```

Example for Rogue `0x30` and host `0xFF`:

```text
0F0330FF  0E FD 00 00 00 00 00 00
```

The Rogue replies with DGN `0x1FD0E`, one record for each output. The component
maps D1 channels `0x0C` through `0x15` to Outputs 1 through 10 and interprets D2
bit 7 as the programmed dimmable capability.

For each response:

- bit 7 set → `light::ColorMode::BRIGHTNESS`
- bit 7 clear → `light::ColorMode::ON_OFF`

The component updates:

- the output's internal `dimmable_` flag
- the dynamic `LightTraits`
- `current_values.color_mode`
- `remote_values.color_mode`
- command behavior for that output

A changed output is logged:

```text
[I][redarc_tvms_rogue]: Output 1 changed to Non-Dimmable
[I][redarc_tvms_rogue]: Output 10 changed to Dimmable
```

After one or more changes:

```text
[I][redarc_tvms_rogue]: Updated 2 Rogue output capabilities; reconnect Home Assistant to refresh light controls
```

ESPHome behavior changes immediately. Home Assistant normally learns the revised
supported light modes when its native API connection reconnects or the ESPHome
integration is reloaded.

If no record arrives:

```text
[W][redarc_tvms_rogue]: No Rogue dimming configuration response received
```

Partial responses produce a warning with the received count.

### Diagnostic recheck button

Each Rogue creates a diagnostic button:

```text
TVMS Rogue Recheck Dimmable Outputs
```

Generated ID for `id: TVMS_Rogue`:

```cpp
id(TVMS_Rogue_recheck_dimmable_outputs)
```

The button calls the same configuration request used at startup. It is intended
for use after changing output programming through the RedVision system.

The button updates ESPHome behavior immediately. Reload or reconnect the Home
Assistant ESPHome integration if the brightness control displayed in HA must be
added or removed.

### Light behavior

Dimmable outputs:

- advertise BRIGHTNESS mode
- accept brightness commands
- use the configured `transition_length`
- send changed integer percentages during transitions
- send a real OFF command at `true_off_threshold`

Non-dimmable outputs:

- advertise ON_OFF mode
- do not expose brightness after Home Assistant refreshes capabilities
- send only ON and OFF commands
- ignore transition percentage stepping

DGN `0x1FD12` remains the actual output level/state feedback source for every
output.

### Rogue settings

| Setting | Default | Purpose |
|---|---:|---|
| `source_address` | `0x30` | Rogue CAN address |
| `host_address` | inherited | Bridge source address used for commands and requests |
| `filter_interval` | inherited | Slow diagnostic publish throttle |
| `transition_length` | `0s` | Fade duration for outputs discovered as dimmable |
| `true_off_threshold` | `1.5` | Percentage at which fade-out becomes a real OFF command |

### Rogue channel map

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

### Rogue frames

| Purpose | DGN / command | Use |
|---|---:|---|
| Set level | command `0x5A` | Absolute `0`–`100%` level for dimmable outputs |
| ON/OFF | command `0xCB` | Binary output command |
| Output state/level | DGN `0x1FD12` | Actual hardware level feedback |
| Output/input status | DGN `0x1FD00` | Inputs, master, output state and fault codes |
| Tank/electrical data | DGN `0x1FD02` | Tank and input voltage/current pages |
| Output configuration | DGN `0x1FD0E` | Requested static output capability records |
| Dimming activity | DGN `0x1FD14` | Runtime activity only; not the capability source |

## TVMS1280

TVMS1280 exposes Outputs 1–10 as switches, plus inverter and master switches.
Digital inputs and feedback are change-published. Effective output status handles
normal, override and master-off states.

## Manager30

Manager30 charging mode is updated from the `0x43` command path and DGN
`0x1F200` bit 0. DGN `0x1F108` is not used as the authoritative mode source
because it can remain fixed.

Charging stage uses:

```text
stage_base = D1 & 0xFE
```

Confirmed bases are Not Charging, Desulphation, Soft-start, Boost, Absorption,
Battery Test, Equalize, Float and Maintenance.

## Stable generated IDs

With explicit device IDs, generated child entities use stable prefixes:

```cpp
id(TVMS_Rogue_output_1)
id(TVMS_Rogue_output_1_level).state
id(TVMS_Rogue_input_8).state
id(TVMS_Rogue_recheck_dimmable_outputs).press()
id(TVMS1280_output_1).turn_on()
id(Manager30_solar_power).state
```

## Development checklist

1. Commit changes on `dev`.
2. Keep the ESPHome source on `ref: dev`.
3. Clean build files and the external-component cache.
4. Validate and compile.
5. Flash the device in CAN `NORMAL` mode.
6. Confirm the startup capability request and response count.
7. Confirm Home Assistant controls after an API reconnect.

A full target ESPHome compile is still required before merging changes from
`dev` to `main`.

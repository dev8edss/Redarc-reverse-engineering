# Redarc / RedVision ESPHome External Components v86

This package splits the RedVision / TVMS CAN bridge into external ESPHome components so the main YAML is no longer a large block of CAN decode lambdas.

## Components

| Component | Purpose |
|---|---|
| `redarc_common` | Shared CAN byte helpers and current decoding helpers |
| `tvms_rouge` | TVMS Rouge dimmable output lights, output level feedback, tank sensors, dim hold/ramp protocol |
| `tvms_1280` | TVMS1280 relay output switches, inverter switch, tank sensors, temperature sensors |
| `manager30` | Manager30 output current, battery voltage, solar input data, AC input voltage |
| `battery_sensor` | Battery current, voltage, SOC, temperature |
| `redvision_display` | Redvision display rebroadcast current values |

## YAML

Use:

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

The YAML still has a single `canbus.on_frame` dispatcher because the ESPHome CAN component receives all frames there. The actual decode logic is now inside the components:

```cpp
id(tvms_rouge_hub).handle_can_frame(id_rx, x);
id(tvms1280_hub).handle_can_frame(id_rx, x);
id(manager30_hub).handle_can_frame(id_rx, x);
id(battery_sensor_hub).handle_can_frame(id_rx, x);
id(redvision1_hub).handle_can_frame(id_rx, x);
id(redvision2_hub).handle_can_frame(id_rx, x);
```

## Current confirmed/working assumptions

### TVMS Rouge

- Outputs use channels `0x0C` through `0x15`.
- Rouge output dimming uses a hold/ramp command, not an absolute brightness command.
- Dim down hold:

```text
0x0F053020  <channel> 01 01 05 00 FF FF FF
```

- Dim up hold:

```text
0x0F053020  <channel> 01 64 05 00 FF FF FF
```

- Release:

```text
0x0F053020  <channel> 01 FF 00 00 FF FF FF
```

- Keepalive during dim hold:

```text
0x0FE6FF20  FF FF FF FF FF FF FF FF
```

- True off threshold is now `<=1%`.
- `gamma_correct: 1.0` and `default_transition_length: 0s` are kept in the light YAML entries to avoid HA/ESPHome intermediate brightness targets.

### TVMS1280

- Outputs 1–10 are mapped as channels `0x04–0x0D`.
- Output 6 = `0x09` and Output 7 = `0x0A` are confirmed.
- Inverter = `0x0E`.
- TVMS1280 has no hardware button inputs.

### RJ45 pinout

| RJ45 pin | Function | Status |
|---:|---|---|
| 4 | CAN L | confirmed |
| 5 | CAN H | confirmed |
| 8 | Ground | confirmed |
| unknown | 24V | unconfirmed |

## Notes

This package was syntax-checked as YAML/Python in the ChatGPT container, but it was not fully compiled with ESPHome because ESPHome is not installed in this environment. If ESPHome reports a codegen or API error, the likely fix will be inside the small component files rather than the large YAML.

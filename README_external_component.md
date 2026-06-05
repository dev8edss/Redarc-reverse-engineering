# RedVision / TVMS Rouge ESPHome External Component

This package moves the TVMS Rouge dimming controller out of the main ESPHome YAML and into a local external component.

## Included

```text
components/
└── tvms_rouge/
    ├── __init__.py
    ├── light.py
    ├── tvms_rouge.h
    └── tvms_rouge.cpp

redvision_tvms_atom_lite_cais3050g_v84_external_tvms_rouge.yaml
```

## Behaviour

- Rouge outputs are exposed as dimmable Home Assistant light entities.
- True OFF threshold is changed to **≤1%**.
- Brightness values above 1% use the confirmed Rouge dim-hold protocol:

```text
0x0F053020  <channel> 01 01 05 00 FF FF FF   # dim down hold
0x0F053020  <channel> 01 64 05 00 FF FF FF   # dim up hold
0x0F053020  <channel> 01 FF 00 00 FF FF FF   # release
```

- During dim hold it sends the Redvision keepalive:

```text
0x0FE6FF20  FF FF FF FF FF FF FF FF
```

- ON/OFF command remains:

```text
0x0F003020  CB 00 FF <channel> <0/1> 00 00 00
```

## Usage from GitHub

After pushing this folder to:

```text
https://github.com/dev8edss/Redarc-reverse-engineering
```

you can reference it from ESPHome as:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dev8edss/Redarc-reverse-engineering
      ref: main
    components: [tvms_rouge]
```

For local testing, the supplied YAML uses:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [tvms_rouge]
```

## Important integration detail

The CAN `on_frame` lambda must pass frames to the component:

```cpp
id(tvms_rouge_hub).handle_can_frame(id_rx, x);
```

The component uses only the Rouge output level feedback frame:

```text
0x1BFD1230
```

## Tuning controls

These remain exposed as Home Assistant config number entities:

- `TVMS Rouge Deadband`
- `TVMS Rouge Initial Ramp Rate`
- `TVMS Rouge Learning Gain`
- `TVMS Rouge Approach`
- `TVMS Rouge Max Pulse`
- `TVMS Rouge Settle Time`
- `TVMS Rouge Max Iterations`

## Notes

This is the first external-component refactor of the working v83 logic. The YAML has been syntax-parsed, but a full ESPHome compile was not available in this environment. If ESPHome reports a codegen/compiler error, keep the error text; it should be straightforward to patch inside `components/tvms_rouge/` without expanding the main YAML again.

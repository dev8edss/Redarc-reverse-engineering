# CAN-level TVMS Rogue emulator

This branch adds a standalone `tvms_rogue_emulator` device under the existing
`redarc:` component. It behaves as an active virtual TVMS Rogue on the CAN bus
and exposes its state to Home Assistant through ESPHome.

> Do not run the emulator and a real Rogue with the same source address on the
> same unfiltered CAN bus. The default emulator address is `0x30`.

## Current behaviour

### Device identity

The emulator broadcasts DGN `0x1F404` as device type `0x16` and responds to
directed `0x0F03<destination><requester>` requests for:

- `0x1F400` firmware/hardware version records.
- `0x1F402` manufacturing date.
- `0x1F403` segmented product name.
- `0x1F404` serial identity.
- `0x1F405` unique identifier.

### Configuration reads

Object `2` returns a complete 4,672-byte configuration object reconstructed
from a real TVMS Rogue. It contains the real Rogue channel structure, including:

- Eight digital inputs.
- Two tank inputs.
- Master channel.
- Ten output channels.
- Input voltage and input current channels.
- Remote input records and captured channel labels/settings.

The configured emulator serial number and a product name up to 12 ASCII
characters are patched into the captured object before its whole-object CRC-32C
is recalculated. Longer product names are still returned correctly through DGN
`0x1F403`, while object `2` retains the captured `TVMS Rogue` name.

The object service supports:

- `0x0E85` object selection with `0x0280` acknowledgement.
- `0x0E86` block reads using `0x0281` data frames and a `0x0284` length/CRC trailer.
- `0x0E89` object close with acknowledgement.

Programming remains deliberately non-persistent:

- `0x0E81` write data is discarded.
- `0x0E83` receives an empty `0x0284` pre-write response.
- `0x0E87`, `0x0E88`, and `0x0E8A` are acknowledged but ignored.
- No received programming data is retained, committed or applied.

### Active CAN emulation

The emulator periodically broadcasts the same status families used by a real
Rogue:

- `0x1FD00` digital input, master and output states.
- `0x1FD02` tank levels, input voltage and input current.
- `0x1FD0E` output capabilities; all ten outputs report as dimmable.
- `0x1FD12` output brightness levels.
- `0x1FD14` output activity pages.

It also answers directed requests for those DGNs.

Supported output commands:

- `0x0F00<rogue><requester>` command `0xCB` for master/output ON and OFF.
- `0x0F00<rogue><requester>` command `0x5A` for absolute output percentage.
- `0x0F05<rogue><requester>` legacy dim-up, dim-down and release commands.

Every accepted output command immediately updates:

1. The internal virtual Rogue state.
2. Home Assistant light and level entities.
3. `0x1FD00`, `0x1FD12`, and `0x1FD14` CAN feedback.
4. INFO-level logs showing the origin, channel and new level.

### Home Assistant entities

Each emulator instance automatically creates:

- Ten dimmable light entities.
- Ten diagnostic output-level sensors.
- Eight diagnostic digital-input binary sensors.
- Tank 1 and Tank 2 sensors.
- Input voltage and input current sensors.
- A combined output-status text sensor.

Tank levels, voltage and current change randomly at the configured interval.
When `randomize_inputs` is enabled, one digital input is also toggled on each
random update. Random changes are published to Home Assistant and broadcast on
CAN.

All identity responses, object operations, output changes, randomized sensor
changes and status broadcasts are logged at INFO level.

## Configuration

The CAN interface must be in `NORMAL` mode because the emulator transmits.
Remove or comment out the normal `tvms_rogue:` block before enabling an emulator
at the same source address.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dev8edss/Redarc-reverse-engineering
      ref: emulated-rogue
    refresh: 0s
    components:
      - redarc

redarc:
  canbus:
    mode: NORMAL

  tvms_rogue_emulator:
    - id: Virtual_Rogue
      source_address: 0x30

      # Periodic broadcasts
      identity_interval: 1s
      status_interval: 1s
      random_update_interval: 5s
      randomize_inputs: true

      # DGN 0x1F404 identity and object-2 Rogue record
      serial_prefix: 2606260001
      serial_suffix: 1
      device_subtype: 0x00

      # DGN 0x1F400. Multiple records may be supplied.
      version_records:
        - product_number: 323
          major: 1
          minor: 4
          record_index: 0
        - product_number: 323
          major: 1
          minor: 0
          record_index: 1

      # DGN 0x1F402
      manufacturing_date:
        day: 26
        month: 6
        year: 2026

      # DGN 0x1F403. A name of 12 characters or fewer is also patched into
      # the captured object-2 module record.
      product_name: "TVMS Rogue"

      # DGN 0x1F405. Exactly seven bytes; a 14-digit hex string is accepted.
      unique_identifier: "01:23:45:67:89:AB:CD"
      unique_identifier_record_index: 0
```

The automatically generated entity IDs use the emulator ID as their prefix.
For the example above, they include:

```text
light.virtual_rogue_output_1
sensor.virtual_rogue_output_1_level
binary_sensor.virtual_rogue_input_1
sensor.virtual_rogue_tank_1
sensor.virtual_rogue_input_voltage
sensor.virtual_rogue_input_current
sensor.virtual_rogue_output_status
```

Run **Clean Build Files** before compiling after switching branches or changing
external-component revisions.

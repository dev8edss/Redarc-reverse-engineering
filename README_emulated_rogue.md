# CAN-level TVMS Rogue emulator

This branch adds a standalone `tvms_rogue_emulator` device under the existing
`redarc:` component. It behaves as a Rogue node on the CAN bus; it does not
control or decode a physical Rogue.

> Do not run the emulator and a real Rogue with the same source address on the
> same unfiltered CAN bus. The default emulator address is `0x30`.

## Current behaviour

- Broadcasts DGN `0x1F404` as device type `0x16` (TVMS Rogue).
- Responds to directed `0x0F03<destination><requester>` requests for:
  - `0x1F400` firmware/hardware version records.
  - `0x1F402` manufacturing date.
  - `0x1F403` segmented product name.
  - `0x1F404` serial identity.
  - `0x1F405` unique identifier.
- Recognises object/programming services `0x0E81`, `0x0E83`, `0x0E85`,
  `0x0E86`, `0x0E87`, `0x0E88`, `0x0E89`, and `0x0E8A` addressed to the
  emulator. It returns a complete `0x0280` acknowledgement with the opcode
  echoed in D3, but deliberately does not read, write, commit, close, or apply
  any configuration object yet.

No Rogue output/status frames are emulated in this first stage.

## Configuration

The CAN interface must be in `NORMAL` mode because the emulator transmits.
Remove or comment out the normal `tvms_rogue:` block before enabling this at
source address `0x30`.

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
      identity_interval: 1s

      # DGN 0x1F404 identity broadcast
      serial_prefix: 2606260001
      serial_suffix: 1
      device_subtype: 0x00

      # DGN 0x1F400. Add multiple records when firmware and hardware are
      # represented by separate record indexes.
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

      # DGN 0x1F403, transmitted as seven ASCII characters per segment.
      product_name: "TVMS Rogue"

      # DGN 0x1F405. Exactly seven bytes; a 14-digit hex string is also accepted.
      unique_identifier: "01:23:45:67:89:AB:CD"
      unique_identifier_record_index: 0
```

`version_records` defaults to one record. The manufacturing date, product name,
serial values, subtype, and unique identifier also have defaults, but explicit
values are recommended so the emulated node is stable between devices.

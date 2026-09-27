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

The captured configuration object is paired with the real Rogue firmware
records `1.4` and `0.4`.

### Configuration reads

On a fresh device, object `2` returns a complete 4,672-byte configuration object
reconstructed from a real TVMS Rogue. It contains the real Rogue channel
structure, including:

- Eight digital inputs.
- Two tank inputs.
- Master channel.
- Ten output channels.
- Input voltage and input current channels.
- Remote input records and captured channel labels/settings.

The configured emulator serial number and a product name up to 12 ASCII
characters are patched into the factory object before its whole-object CRC-32C
is recalculated. Longer product names are still returned correctly through DGN
`0x1F403`, while the factory object retains the captured `TVMS Rogue` name.

After a valid configuration write, the exact committed object replaces the
factory object. The saved object may have a different length; objects up to
8,192 bytes are supported. Subsequent `0x0E86` reads and reads after an ESP32
restart return the persisted object byte-for-byte.

The object service supports:

- `0x0E85` object selection with `0x0280` acknowledgement.
- `0x0E86` padded block reads using `0x0281` data frames and a `0x0284`
  length/CRC trailer.
- `0x0E89` object close with acknowledgement.

### Configuration writes and persistence

Object `2` writes are transactional:

1. `0x0E83` reports the captured 1,024-byte write-transfer capability.
2. `0x0E87` starts a new staging transaction.
3. `0x0E81` frames are collected into the current block.
4. `0x0E88` validates that block's CRC-32C and stores it at the supplied offset.
5. `0x0E89` closes the staging session.
6. `0x0E8A` validates complete byte coverage, declared object length and the
   whole-object CRC-32C before committing.

The app writes page `0x0000`, containing the object header and whole-object CRC,
last. The emulator therefore tracks received bytes independently of write order
and safely accepts repeated pages.

A valid object is written to ESPHome's persistent preferences storage and is
loaded on the next boot. An interrupted write, missing page, bad block CRC,
invalid declared length, bad whole-object CRC or failed flash save leaves the
previous configuration unchanged.

Erasing the ESPHome device flash/preferences restores use of the factory
captured object.

The current persistence stage stores and returns the raw object. The separate
configuration DGN replies (`0x1FD04`, `0x1FD06`, `0x1FD07`, `0x1FD0A`,
`0x1FD0C`, `0x1FD0E`, and `0x1FD10`) still reproduce the captured Rogue
responses. Decoding a saved object into dynamic DGN replies and runtime channel
settings is the next development stage.

### Active CAN emulation

The emulator periodically broadcasts the same status families used by a real
Rogue:

- `0x1FD00` digital input, master and output states.
- `0x1FD02` tank levels, input voltage and input current.
- `0x1FD0E` output capabilities.
- `0x1FD12` output brightness levels.
- `0x1FD14` output activity pages.

It also answers directed requests for those DGNs.

Supported output commands:

- `0x0F00<rogue><requester>` command `0xCB` for master/output ON and OFF.
- `0x0F00<rogue><requester>` command `0x5A` for absolute output percentage.
- `0x0F05<rogue><requester>` hold dim-up, hold dim-down and release commands.

Screen hold dimming reproduces the captured Rogue behaviour: a start command
runs a continuous ramp, the duration byte controls full-scale ramp time, and a
release command stops at the current level.

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

## Complete ESPHome configuration

This example is for an M5Stack Atom Lite with an Atomic CAN base (CAN TX
`GPIO22`, RX `GPIO19`). Supply your own ESPHome `wifi_ssid`, `wifi_password`
and `api_encryption_key` secrets. Use a different source address from every
real device on the bus: `0x36` below is an example, while a real Rogue often
uses `0x30`. The CAN interface must use `NORMAL` mode to transmit. Do not
configure a `tvms_rogue:` monitor at the emulator's source address.

```yaml
esphome:
  name: virtual-tvms-rogue
  friendly_name: Virtual TVMS Rogue

esp32:
  board: m5stack-atom
  framework:
    type: esp-idf

logger:
  level: INFO

api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

external_components:
  - source:
      type: git
      url: https://github.com/dev8edss/Redarc-reverse-engineering
      ref: emulated-rogue
    components:
      - redarc

redarc:
  canbus:
    tx_pin: GPIO22
    rx_pin: GPIO19
    bit_rate: 250KBPS
    mode: NORMAL

  tvms_rogue_emulator:
    - id: virtual_rogue
      source_address: 0x36
      serial_prefix: 2606260001
      product_name: "TVMS Rogue"
      randomize_inputs: false
```

The `redarc:` block requires **exactly one** of `canbus:` (as above) or
`canbus_id:` (an existing top-level CAN bus). The repository component supplies
CAN defaults of TX `GPIO22`, RX `GPIO19`, `250KBPS`, CAN ID `0x7FE`, extended
IDs enabled and RX queue length `64` when supported by the installed ESPHome.
Set `mode: NORMAL` explicitly: the mode follows ESPHome's CAN schema, rather
than a default added by this component. ESPHome's own `esphome`, `esp32`,
`api`, `ota`, `wifi` and `logger` settings are shown as a working device example;
their full options are governed by ESPHome.

### Emulator options

All options below are inside one `redarc.tvms_rogue_emulator` list entry.
Only the list entry itself is needed to enable an emulator. `id` is optional
in the schema but is recommended for stable Home Assistant entity names.

| Option | Required? | Schema default | Effective behaviour / purpose |
|---|---|---|---|
| `id` | No | Generated by ESPHome | Prefix for generated entity names. |
| `source_address` | No | `0x30` | Set a unique address; `0x36` in the example avoids the usual real Rogue address. |
| `identity_interval` | No | `1s` | Periodic identity broadcast. Positive duration. |
| `status_interval` | No | `1s` | Periodic status broadcast. Positive duration. |
| `random_update_interval` | No | `5s` | Random tank/voltage/current updates. Schema requires a positive duration; `0s` is rejected. Supplying a tank source disables the random update timer at runtime. |
| `randomize_inputs` | No | `true` | Whether random updates also toggle a digital input. `false` still permits random sensor updates. |
| `serial_prefix` | No | `0` | Four-byte identity prefix; set it to the intended module's serial prefix. |
| `serial_suffix` | No | `1` | **Ignored by the current C++ setter:** effective suffix is forced to `0x0013` (decimal 19), unless later changed by a saved configuration object. |
| `device_subtype` | No | `0` | Identity subtype byte. |
| `version_records` | No | One record: product `323`, major `1`, minor `0`, index `0` | **Ignored by the current C++ adder:** transmitted records are fixed at product `323`, versions `1.4` (index `0`) and `0.4` (index `1`). |
| `manufacturing_date` | No | `day: 1`, `month: 1`, `year: 2026` | All three fields are required if supplied; must form a valid date. |
| `product_name` | No | `TVMS Rogue` | ASCII only. Names of at most 12 characters are also patched into the factory configuration object. |
| `unique_identifier` | No | `00:00:00:00:00:00:01` | Exactly seven bytes, as hex text or a seven-item byte list. |
| `unique_identifier_record_index` | No | `0` | Record index sent with the unique identifier. |
| `tank1_source` | No | No source | ID of an existing ESPHome sensor supplying Tank 1 percent. |
| `tank2_source` | No | No source | ID of an existing ESPHome sensor supplying Tank 2 percent. |

If you specify `version_records`, each entry requires `product_number`,
`major` and `minor`; `record_index` defaults to `0` and indexes must be unique.
The current implementation accepts but does not use those entries. Do not
configure `serial_suffix` or `version_records` expecting them to change the
advertised values until the C++ setters are updated.

An optional external tank source can be declared with ESPHome's `sensor:`
component and referenced by its ID, for example:

```yaml
sensor:
  - platform: homeassistant
    id: fresh_tank_percent
    entity_id: sensor.fresh_tank_percent

redarc:
  tvms_rogue_emulator:
    - id: virtual_rogue
      tank1_source: fresh_tank_percent
```

Merge the optional lines into the full example rather than creating a second
`redarc:` block. The source must publish a percentage from `0` to `100`;
assigning either tank source currently turns off the random update timer for
the whole emulator.

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

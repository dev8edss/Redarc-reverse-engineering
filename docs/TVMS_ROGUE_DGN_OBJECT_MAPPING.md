# TVMS Rogue object-2 to CAN DGN mapping

This document maps the TVMS Rogue configuration object to the CAN DGNs used by
RedVision Configurator. It distinguishes confirmed object-derived fields from
live status fields and generic requests for which a real Rogue remains silent.

## Confidence terms

- **Confirmed**: reproduced by the typed object and a real Rogue CAN response,
  or isolated by a controlled setting change.
- **Structure confirmed**: framing and item positions are known; one or more
  value enums remain unknown.
- **Candidate**: supported by correlation but needs a one-setting controlled
  capture.

## Configuration object transport

Object 2 is transferred through the directed `0E85/0E86/0281/0284/0E89`
service and written through `0E87/0E81/0E88/0E89/0E8A`.

- `0E85`: select object.
- `0E86`: D1-D4 offset LE32, D5-D8 requested byte count LE32.
- `0281`: sequential object bytes.
- `0284`: D1-D4 returned count, D5-D8 CRC-32C.
- `0E81`: sequential write bytes.
- `0E88`: D1-D4 destination offset, D5-D8 block CRC-32C.
- Whole-object header: magic `08 00 00 04`, length LE32, CRC-32C with the CRC
  word zeroed.

## Typed object format

| Type | High byte | Payload |
|---|---:|---|
| String | `01` | UTF-8 bytes |
| Vector | `02` | LE32 entries |
| Map | `03` | repeated LE32 key/value pairs |
| Container | `04` | container words |
| Byte blob | `05` | binary bytes |

Immediate integer encoding is `stored = value * 4 + 1`. Four-byte aligned
values that point to a valid node header are object offsets.

## DGN 0x1F108 — load-disconnect configuration

**Status: fully confirmed and object-derived.**

Object path:

```text
root[1] -> device-types[2] -> Rogue[0x16] -> load-disconnect[4]
```

| Object key | Meaning | DGN field |
|---:|---|---|
| 1 | Trigger enum | D1 bits 2..4 |
| 2 | Disconnect threshold, mV | D2-D3 LE16 |
| 3 | Reconnect threshold, mV | D4-D5 LE16 |
| 4 | Disconnect SOC, % | D6 |
| 5 | Reconnect SOC, % | D7 |

Trigger enum:

| Raw | Meaning |
|---:|---|
| 0 | Always |
| 1 | BMS Voltage |
| 2 | BMS SOC |
| 3 | Never |
| 4 | Voltage |

Encoding:

```text
D1 = 0xE0 | ((trigger & 7) << 2)
D2-D3 = disconnect mV, little-endian
D4-D5 = reconnect mV, little-endian
D6 = disconnect SOC
D7 = reconnect SOC
D8 = 0
```

Current object example:

```text
13F10836  EC D8 27 50 2D 17 3D 00
```

## DGN 0x1FD04 — channel labels

**Status: fully confirmed and object-derived.**

Object path for each channel:

```text
Rogue[2] -> channel map -> channel -> record[1] string
```

- D1: channel/item number `1..33`.
- D2: zero-based segment index.
- D3-D8: six ASCII bytes.
- Unused bytes are `FF`.
- The real device emits `(length // 6) + 1` segments, including an empty
  terminator when the text length is an exact multiple of six.

## DGN 0x1FD06 — sensor alarm/range configuration

**Status: fully confirmed for current Rogue channel classes.**

Frame layout:

| Byte | Meaning |
|---|---|
| D1 | channel |
| D2 | alarm mode |
| D3-D4 | lower threshold LE16 |
| D5-D6 | upper threshold LE16 |
| D7-D8 | `FF FF` |

Channels emitted by the Rogue:

- `09`, `0A`: tank alarm map at channel record key 5 -> key 7 -> keys 1..3.
- `16`, `17`: analogue range/alarm map at channel record key 8 -> key 1 ->
  keys 1..3.

Alarm mode enum:

| Raw | Meaning |
|---:|---|
| 0 | Disabled |
| 1 | High limit |
| 2 | Low limit |

## DGN 0x1FD0A — channel inventory and definition

**Status: fully confirmed for the current Rogue classes.**

One frame is emitted for each channel `1..33`.

| Byte | Meaning | Object source |
|---|---|---|
| D1 | Channel | channel map key |
| D2 | Channel class | hardware class table below |
| D3-D4 | Class subtype LE16 | class-specific object field |
| D5-D6 | Icon/type LE16 | channel record key 3 |
| D7-D8 | Enabled/present LE16 | channel record key 2 |

Channel classes:

| Channels | D2 | Class | D3-D4 |
|---|---:|---|---|
| `01-08` | `00` | switch input | `0000` |
| `09-0A` | `0C` | tank | tank settings key 4 |
| `0B` | `08` | master | `FFFF` |
| `0C-15` | `0A` | output | `FFFF` |
| `16` | `02` | input voltage | `0064` |
| `17` | `02` | input current | `0065` |
| `18-21` | `0B` | remote input | `FFFF` |

The enabled field must come from object key 2; it must not be inferred from the
channel range. A disabled physical output therefore returns `00 00` in D7-D8.

## DGN 0x1FD0E — output capabilities/control mode

**Status: fully confirmed and object-derived.**

One frame is emitted for output channels `0C..15`.

| Byte | Meaning |
|---|---|
| D1 | output channel |
| D2 | capability/control byte |
| D3 | `00` |
| D4-D8 | `FF` |

Object path: channel record key 6.

- nested key 1: dimming.
- nested key 2: control mode (`0` Always On, `1` User Control).

Encoding:

| Dimming | Control | D2 |
|---:|---|---:|
| 1 | either | `83` |
| 0 | Always On | `01` |
| 0 | User Control | `03` |

## DGN 0x1FD10 — switch-input secondary configuration

**Status: structure confirmed; semantic mapping not yet confirmed.**

- D1: input channel `1..8`.
- D2-D3: two configuration bytes.
- D4-D8: `FF`.

The current real Rogue returns `00 00` for all eight inputs even when the object
contains non-zero enable and electrical-type values. Therefore D2-D3 are **not**
the nested input enable/key-4 pair. Until a controlled real-Rogue capture changes
one electrical profile and reads this DGN, the emulator should reproduce the
known `00 00` response rather than invent a mapping.

## DGN 0x1FD0C — sensor engineering metadata

**Status: structure confirmed; field enum partly unresolved.**

Current Rogue frames:

```text
09 64 00 00 00 00 64 00
0A 64 00 00 00 00 64 00
16 61 00 00 00 00 60 EA
17 61 00 00 00 00 60 EA
```

This is stable sensor-format/range metadata, separate from alarm thresholds in
`1FD06`. Current evidence supports:

- D1: channel.
- D2: format/engineering code (`64` tank-percent, `61` analogue).
- D3-D6: zero in the current configuration.
- D7-D8: maximum/range value (`100` for tanks, `60000` for analogue channels).

The exact enum names for `61` and `64` still require a second device or
controlled sensor-profile capture.

## DGN 0x1FD07 — sensor validity/status array

**Status: paging structure confirmed; status enum unresolved.**

- D1: base item.
- D2-D8: one status byte for base item through base+6.

Current Rogue pages in the available complete configuration captures:

```text
09 FF FF FF FF FF FF FF
16 FF FF FF FF FF FF FF
```

Other TVMS-device captures contain `FC` and `FD`, but no controlled Rogue capture
yet proves their names. Treat the bytes as validity/status values and retain the
real Rogue's current all-`FF` response until disconnect/reconfigure tests isolate
each state.

## DGN 0x1FD08 — active-channel inventory

**Status: exact payload known; semantics not fully decoded.**

Current Rogue payload:

```text
21 FF FF 1E FF FF FF FF
```

The first byte correlates with the highest channel/item (`0x21`). The meaning of
D2-D8, especially `1E`, is not proven and must not be labelled an active mask
without a controlled channel-count/configuration capture.

## Runtime DGNs

### 0x1FD00 — channel state array

**Structure confirmed.** D1 is a base channel and D2-D8 are seven channel-state
bytes. `00`/`01` are off/on. Real captures also contain `24`, `F8`, and `FF`;
their exact inactive/unavailable meanings are not fully resolved.

### 0x1FD02 — sensor values

**Confirmed for current Rogue:**

- page `09`: D2 tank 1 %, D3 tank 2 %.
- page `16`: D2-D3 input voltage in mV LE; D4-D5 input current in mA LE.

### 0x1FD12 — output level array

**Confirmed.** D1 is base channel and D2-D8 are seven raw percentages. Physical
outputs occupy channels `0C..15`.

### 0x1FD14 — output activity/secondary state

**Hold-dimming state confirmed; other activity values unresolved.** D1 is the
base output channel and D2-D8 are per-output activity values. In the controlled
real-versus-emulator hold-dimming capture, the selected output changes from
`00` to `02` for the complete held ramp and returns to `00` on release. Both dim
up and dim down use `02`; direction is carried by the `0F05` command, not this
status DGN.

| Value | Meaning | Confidence |
|---:|---|---|
| `00` | idle | confirmed |
| `02` | hold-dimming/ramp active | confirmed |

Further controlled transient, override and protected/fault tests are required
to name any additional values.

## Device identity DGNs

| DGN | Decode | Confidence |
|---|---|---|
| `1F400` | version records; D1-D2 product 323, D3-D4 version, D7 record index | high |
| `1F402` | D1 day, D2 month, D3-D4 year LE | high |
| `1F403` | D1 segment; D2-D8 seven ASCII characters | confirmed |
| `1F404` | D1-D4 serial prefix, D5-D6 suffix, D7 device type `16`, D8 subtype | confirmed |
| `1F405` | D1 page/index, D2-D8 56-bit unique identifier | high |

## Generic probes for which a real Rogue is silent

The Configurator asks all devices for several capability DGNs. In the real Rogue
capture there is no reply to:

- `1F40A`
- `1F207`
- `1F205`
- `1F20F`

Silence is the correct Rogue behaviour and the emulator should not fabricate
responses merely because the requests appear in the log.

## Remaining controlled captures

To finish the unresolved enums rather than guess:

1. Change exactly one Rogue input electrical type, then request `1FD10`.
2. Disconnect/reconnect one tank and one analogue input while logging `1FD07`.
3. Change one tank sender profile and inspect `1FD0C` and object key changes.
4. Enable/disable one channel at a time and inspect `1FD08`.
5. Hold-dim a single output and correlate `1FD14` values with start, active,
   release, and limits.

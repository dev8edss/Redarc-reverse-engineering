# Arduino IDE TVMS Rogue Emulator

This branch is a standalone Arduino IDE port of the `emulated-rogue` ESPHome work.

The goal is deliberately narrow:

- emulate a **TVMS Rogue / DPDM** CAN device,
- run from the Arduino IDE on an ESP32,
- use the ESP32 built-in TWAI/CAN driver,
- avoid ESPHome and Home Assistant dependencies,
- keep the reverse-engineered Rogue protocol behavior in normal C++/Arduino code.

This sketch is intended for reverse-engineering and test-bench use. It is not a complete REDARC firmware replacement.

## Folder

Open this sketch in Arduino IDE:

```text
arduinoIDE/TVMS_Rogue_Emulator/TVMS_Rogue_Emulator.ino
```

The embedded Object 2 readback data is in:

```text
arduinoIDE/TVMS_Rogue_Emulator/RogueObject2.h
```

## Hardware target

Primary target:

- ESP32, such as M5Stack Atom Lite
- CAN transceiver/base, such as M5Stack Atomic CAN Base / CA-IS3050G
- REDARC CAN bus at 250 kbit/s
- Extended CAN IDs

Default pins:

| Signal | ESP32 pin |
|---|---|
| CAN TX | `GPIO22` |
| CAN RX | `GPIO19` |

REDARC RJ45 pinout used throughout this project:

| RJ45 pin | Signal |
|---:|---|
| 4 | CAN L |
| 5 | CAN H |
| 7 | 12–30 V supply |
| 8 | Ground |

Do not feed the REDARC supply directly into the ESP32. Use a suitable regulator or isolated design.

## Required Arduino setup

Use Arduino IDE with an ESP32 board package that exposes the ESP-IDF TWAI driver.

Select an ESP32 board such as:

```text
M5Stack-ATOM
ESP32 Dev Module
```

No external CAN library is used. The sketch includes:

```cpp
#include <driver/twai.h>
```

## Source address

The sketch defaults to source address:

```cpp
SOURCE_ADDRESS = 0x36
```

A real TVMS Rogue normally uses `0x30`. Do not run the emulator at `0x30` on the same CAN bus as a real Rogue unless the real device is disconnected.

## What is currently ported

The Arduino IDE port includes the main live CAN behavior learned in the `emulated-rogue` branch.

### Runtime broadcasts

The sketch periodically sends:

| DGN / ID family | Meaning |
|---|---|
| `0x1F108` | load-disconnect configuration |
| `0x1FD00` | channel status pages |
| `0x1FD02` | tank, input voltage and input current values |
| `0x1FD08` | active-channel inventory |
| `0x1FD0E` | output capabilities |
| `0x1FD12` | output levels |
| `0x1FD14` | output activity / hold-dim activity |

### Identity/config responses

The sketch responds to DGN requests for:

| DGN | Meaning |
|---|---|
| `0x1F108` | load-disconnect configuration |
| `0x1F400` | firmware/version records |
| `0x1F403` | product name chunks |
| `0x1F404` | serial/device type information |
| `0x1F405` | unique/device ID |
| `0x1FD00` | channel status |
| `0x1FD02` | tanks / voltage / current |
| `0x1FD08` | active-channel inventory |
| `0x1FD0E` | output capabilities |
| `0x1FD12` | output levels |
| `0x1FD14` | output activity |

### REDARC object readback

The sketch now embeds the captured Rogue **Object 2** image from the ESPHome emulator and serves it through the REDARC object read protocol.

| CAN service / response | Meaning |
|---|---|
| `0x0E85` | select object, for example object `0x02` |
| `0x0E86` | read object block, offset + length little-endian |
| `0x0281` | returned object data, 8-byte chunks |
| `0x0284` | returned length and CRC-32C trailer |

Current embedded object:

```text
Object: 2 / main configuration object
ROGUE_OBJECT2_SIZE: 4748 bytes
Stored object CRC field: 0xFA84819A
```

Readback behavior:

- If selected object is `0x02`, reads return bytes from the captured Rogue object.
- If the request reads beyond the object end, the sketch pads with `0xFF`, matching the ESPHome emulator behavior.
- Unsupported selected objects return `0xFF` data with a valid block CRC.
- Trailer CRC uses CRC-32C / Castagnoli reflected polynomial `0x82F63B78`.
- Oversized block reads above `8192` bytes are refused to protect ESP32 RAM.

The object data is stored as base64 text in flash/PROGMEM and decoded byte-by-byte when a read request arrives. That keeps RAM use low while still giving the display/configurator a real object image.

### Commands

The sketch handles:

| Command | Meaning |
|---|---|
| `0xCB` | master/output on/off |
| `0x5A` | absolute output level |
| `0x0F05` | legacy/display hold dimming |
| `0x0F04` | direct-command ACK reply |

## Serial commands

Open Serial Monitor at `115200`.

Commands:

```text
help
status
t1 <0-100>        set Tank 1 percent
t2 <0-100>        set Tank 2 percent
v <millivolts>    set input voltage, e.g. v 13600
i <milliamps>     set input current, e.g. i 2500
m <0|1>           master off/on
o<n> <0-100>      set output, e.g. o1 75
identity          send identity frames now
send              send status frames now
```

Examples:

```text
t1 25
t2 80
o1 100
o6 55
m 1
v 13750
i 3200
```

Tank values are expected as percentages. They are clamped/rounded to `0–100%` before being sent on `0x1FD02`.

## Important differences from ESPHome branch

This Arduino sketch does not create Home Assistant entities and does not depend on ESPHome component code.

The ESPHome branch has:

- generated HA lights and sensors,
- external tank source sensor binding,
- ESPHome callbacks,
- Home Assistant API integration,
- ESPHome external component packaging,
- a larger set of active-object derived configuration responses.

The Arduino branch currently has:

- raw CAN/TWAI setup,
- serial command control,
- hardcoded emulator runtime state,
- simple periodic identity/status loop,
- core output/tank/status command behavior,
- embedded Rogue Object 2 readback.

## Current limits / next work

Known limits:

- Some identity/config DGNs are conservative approximations.
- Some fields are fixed to values captured from the current Rogue object.
- It does not yet support BLE/RBus emulation.
- It does not yet include a full Arduino library abstraction; it is currently one sketch.
- It has not been compile-tested in this chat environment.

Likely next steps:

1. Compile in Arduino IDE against your ESP32 board package.
2. Fix any ESP32-core/TWAI compatibility issues.
3. Run on an isolated CAN bench first.
4. Capture what the RedVision display requests from source `0x36`.
5. Compare responses against the ESPHome `emulated-rogue` branch.
6. Add more active-object derived DGN responses as needed.
7. Split the sketch into a small reusable Arduino library once the base compiles and behaves correctly.

## CAN safety

Use a bench setup first. Running this emulator on the same bus as a live Redarc system means it will actively transmit frames. Keep its source address unique and avoid source `0x30` unless the real Rogue is unplugged.

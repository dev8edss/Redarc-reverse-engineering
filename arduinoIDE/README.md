# Arduino IDE TVMS Rogue Emulator

This branch starts a standalone Arduino IDE port of the `emulated-rogue` ESPHome work.

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

The first Arduino IDE port includes the main live CAN behavior learned in the `emulated-rogue` branch.

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
- full captured object readback in the current emulator.

The Arduino branch currently has:

- raw CAN/TWAI setup,
- serial command control,
- hardcoded emulator state,
- simple periodic identity/status loop,
- the core output/tank/status command behavior.

## Current limits / next work

The port is a first working base, not the finished replacement.

Known limits:

- Full REDARC object readback still needs to be added to the Arduino sketch.
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
6. Add full captured Object 2 readback.
7. Split the sketch into a small reusable Arduino library once the base compiles and behaves correctly.

## CAN safety

Use a bench setup first. Running this emulator on the same bus as a live Redarc system means it will actively transmit frames. Keep its source address unique and avoid source `0x30` unless the real Rogue is unplugged.

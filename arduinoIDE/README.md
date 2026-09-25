# Arduino IDE TVMS Rogue Emulator

This branch is the standalone Arduino IDE port of the REDARC / RedVision `emulated-rogue` work.

The goal is to emulate a **TVMS Rogue / DPDM** CAN device from an ESP32 using the Arduino IDE, without ESPHome or Home Assistant runtime dependencies.

At the moment this is still inside the original repository because the GitHub connector available here does not expose a fork/create-repository action. Treat `arduinoIDE/` as the Arduino-only project area. Main and `emulated-rogue` are untouched.

## Sketch files

Open this sketch in Arduino IDE:

```text
arduinoIDE/TVMS_Rogue_Emulator/TVMS_Rogue_Emulator.ino
```

Supporting files beside the sketch:

```text
arduinoIDE/TVMS_Rogue_Emulator/RogueObject2.h
arduinoIDE/TVMS_Rogue_Emulator/RoguePreferences.h
```

`RogueObject2.h` stores the captured Object 2 image. `RoguePreferences.h` stores the NVS schema, defaults, IO assignment types and settings load/save helpers.

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

## Arduino setup

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

The sketch also uses ESP32 NVS via:

```cpp
#include <Preferences.h>
```

## Source address

The default emulator source address is:

```cpp
0x36
```

A real TVMS Rogue normally uses `0x30`. Do not run this emulator at `0x30` on the same CAN bus as a real Rogue unless the real Rogue is disconnected.

You can change and persist the source address from Serial Monitor:

```text
sa 0x36
```

## Input/output assignment modes

All 8 digital inputs and all 10 outputs now have a persisted assignment mode.

| Mode | Meaning |
|---|---|
| `simulated` | Internal emulator-only value. No ESP32 pin is used. |
| `variable` | Internal variable hook. Can be driven by sketch code or Serial commands. |
| `pin` | Bound to an ESP32 GPIO pin. Inputs are read from the pin; outputs drive the pin. |

Defaults:

- all inputs are `simulated`, no GPIO pin,
- all outputs are `simulated`, no GPIO pin.

Examples:

```text
input 1 pin 33
input 2 variable
input 3 simulated

output 1 pin 25
output 2 variable
output 3 simulated
```

GPIO assignments are saved in NVS. After reboot, the sketch loads them, configures input pins with `pinMode(pin, INPUT)`, configures output pins with `pinMode(pin, OUTPUT)`, and drives each output pin according to the current output level.

For output GPIO mode, the first implementation is digital-only:

```text
0%    -> LOW
1-100 -> HIGH
```

Brightness percentages are still sent on CAN as normal Rogue output levels. GPIO PWM can be added later if needed.

Use this to show current mappings:

```text
io
```

Use this to drive variable/simulated inputs from Serial Monitor:

```text
in1 1
in1 0
```

## Runtime broadcasts

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

## Identity/config DGN responses

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
| `0x1FD04` | channel labels |
| `0x1FD06` | alarm/range configuration |
| `0x1FD07` | alarm/status placeholder pages |
| `0x1FD08` | active-channel inventory |
| `0x1FD0A` | channel details / inventory |
| `0x1FD0C` | analog scaling metadata |
| `0x1FD0E` | output capabilities |
| `0x1FD10` | digital-input configuration placeholder pages |
| `0x1FD12` | output levels |
| `0x1FD14` | output activity |

Some bytes are still conservative/capture-matched placeholders rather than fully understood semantics. They are ported from the ESPHome emulator work so the RedVision side sees familiar responses.

## REDARC Object 2 readback

The sketch embeds the captured Rogue **Object 2** image from the ESPHome emulator and serves it through the REDARC object-read protocol.

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

## Startup Object 2 self-test

At boot, the sketch checks the embedded Object 2 image:

- object header declared length,
- stored whole-object CRC field,
- calculated whole-object CRC-32C with bytes `8..11` zeroed.

Expected Serial Monitor line:

```text
Object2 size=4748 declared_len=4748 stored_crc=0xFA84819A calc_crc=0xFA84819A OK
```

You can run it again:

```text
crc
```

## Commands handled over CAN

| Command | Meaning |
|---|---|
| `0xCB` | master/output on/off |
| `0x5A` | absolute output level |
| `0x0F05` | legacy/display hold dimming |
| `0x0F04` | direct-command ACK reply |

## Serial Monitor commands

Open Serial Monitor at `115200`.

```text
help
status
io

t1 <0-100>            set/persist Tank 1 percent
t2 <0-100>            set/persist Tank 2 percent
v <millivolts>        set input voltage, e.g. v 13600
i <milliamps>         set input current, e.g. i 2500
m <0|1>               master off/on
o<n> <0-100>          set output, e.g. o1 75
in<n> <0|1>           set input variable/simulated state, e.g. in1 1

input <n> <simulated|variable|pin> [gpio]
output <n> <simulated|variable|pin> [gpio]

sa <0x01-0xFE>        set/persist source address
serial <prefix> <suffix>
name <text>

save                  save settings to NVS
defaults              restore default persisted settings
crc                   run Object 2 CRC self-test
identity              send identity frames now
send                  send status frames now
```

Examples:

```text
sa 0x36
serial 2606260001 0x0013
name TVMS Rogue

input 1 pin 33
input 2 variable
in2 1

output 1 pin 25
output 2 variable
o1 100
o2 55

t1 25
t2 80
m 1
v 13750
i 3200
```

Tank values are expected as percentages. They are clamped/rounded to `0–100%` before being sent on `0x1FD02`.

## NVS persistence

The following are saved in ESP32 NVS by `RoguePreferences.h`:

- source address,
- Tank 1 percent,
- Tank 2 percent,
- serial prefix,
- serial suffix,
- product name,
- input 1–8 assignment mode,
- input 1–8 GPIO pin,
- output 1–10 assignment mode,
- output 1–10 GPIO pin.

Use `defaults` to restore the built-in defaults.

## Current limits

- It has not been compile-tested in this chat environment.
- GPIO outputs are currently digital on/off only, not PWM brightness.
- GPIO inputs are read as active-high using `pinMode(pin, INPUT)`.
- Object 2 is still the captured/static object image; changing serial/name/source address changes live identity frames but does not rewrite the embedded Object 2 image.
- BLE/RBus emulation is not included.

## CAN safety

Use a bench setup first. Running this emulator on the same bus as a live Redarc system means it will actively transmit frames. Keep its source address unique and avoid source `0x30` unless the real Rogue is unplugged.

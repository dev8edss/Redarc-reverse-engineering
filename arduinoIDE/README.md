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
arduinoIDE/TVMS_Rogue_Emulator/RoguePreferencesRuntime.h
```

`RogueObject2.h` stores the captured Object 2 image. `RoguePreferences.h` is intentionally only editable variables and values. `RoguePreferencesRuntime.h` owns the structs, NVS schema, validation, and load/save helpers.

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

Use Arduino IDE with the esp32 board package **3.x** (Espressif). The sketch uses the ESP-IDF TWAI driver and the 3.x `ledcAttach`/`ledcWrite` PWM API, and will not build on 2.x.

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

## Preference defaults file

Edit this file for built-in defaults:

```text
arduinoIDE/TVMS_Rogue_Emulator/RoguePreferences.h
```

It is deliberately simple variable/value configuration. Each tank, input and output has one assignment string, and its mode is derived from that text:

| Assignment text | Mode | Meaning |
|---|---|---|
| `GPIO34` | pin | Bound to that ESP32 GPIO. Tanks read analog ADC; inputs read digital; outputs drive digital. |
| `simulate` | simulated | Internal emulator-only value. No GPIO is used. |
| `disabled` | disabled | Channel unused. Tank reads 0%, input reads off, output stays 0% and on/level/dim commands for it are ignored. |
| anything else | variable | A named variable, e.g. `fresh_water`. Max 15 characters, no spaces. Driven with `set <name> <value>`. |

Matching is case-insensitive (`gpio34`, `Simulate`, `DISABLED` all work). Text starting with `GPIO` that is not followed by a number is rejected rather than treated as a variable name.

Example defaults:

```cpp
static const char pref_tank1[]    = "GPIO34";
static const char pref_tank2[]    = "grey_water";
static const char pref_input_1[]  = "GPIO33";
static const char pref_input_2[]  = "disabled";
static const char pref_output_1[] = "GPIO25";
```

The shipped defaults are `simulate` for every tank, input and output.

After changing the defaults file, run `defaults` from Serial Monitor to clear NVS and reload the built-in defaults.

## GPIO checks

A GPIO assignment is refused (from Serial) or the channel is set to `disabled` with a message (at boot) when the pin:

- is not a GPIO on the chip,
- is GPIO6–11 on the original ESP32 (wired to the SPI flash),
- is the CAN TX/RX pin (`GPIO22` / `GPIO19`),
- is input-only (GPIO34–39) and is assigned to an output,
- is not an ADC pin and is assigned to a tank,
- is already used by another tank/input/output.

When a channel is moved off a GPIO, that pin is driven LOW (outputs) and returned to high-impedance input, so it does not stay on.

Tank GPIO mode uses `analogRead()`:

```text
ADC raw 0    -> 0%
ADC raw 4095 -> 100%
```

Input GPIO mode is active-high:

```text
LOW  -> off
HIGH -> on
```

Output GPIO mode uses LEDC PWM, so the Rogue output level sets the duty cycle:

```text
0%     -> LOW (off)
1-99%  -> PWM, duty = level
100%   -> HIGH (fully on)
```

Dimming from RedVision (`0x5A` levels and `0x0F05` hold-dim) therefore dims a load on the GPIO. Frequency and resolution are set in `RoguePreferences.h`:

```cpp
static uint32_t pref_output_pwm_frequency_hz    = 5000;
static uint8_t  pref_output_pwm_resolution_bits = 10;
```

Duty is linear in the level (no gamma correction). If no LEDC channel is free for a pin (the original ESP32 has 16, ESP32-S3 has 8, ESP32-C3 has 6), that output falls back to on/off and a message is printed.

Do not drive a relay coil or other on/off-only load directly from a PWM output at levels between 1% and 99%. Keep such outputs at 0% or 100%.

Use this to show current mappings:

```text
io
```

Use these to drive tanks, inputs and outputs from Serial Monitor:

```text
set grey_water 80     every channel assigned to variable grey_water
t1 25                 tank 1 (simulated or variable)
in1 1                 input 1 (simulated or variable)
o1 75                 output 1
```

`set` applies to every tank, input and output using that variable name. Tanks and outputs take `0-100`; inputs take `0` or `1`.

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

## CAN health

- Frames are queued without blocking. If the TX queue is full (typically no other node on the bus to ACK), frames are dropped and a single summary line is printed every 5 s instead of one line per frame.
- If the controller goes bus-off, the sketch starts recovery automatically and restarts CAN when recovery completes.
- If the TWAI driver fails to install or start at boot, it is retried every 5 s. `status` shows `CAN=running` or `CAN=down`.

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
in<n> <0|1>           set input simulated/variable state, e.g. in1 1
set <name> <value>    set every channel assigned to a named variable

tank <n> <GPIO<n>|simulate|disabled|variable-name>
input <n> <GPIO<n>|simulate|disabled|variable-name>
output <n> <GPIO<n>|simulate|disabled|variable-name>

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

tank 1 GPIO34
tank 2 grey_water
set grey_water 80

input 1 GPIO33
input 2 door_switch
input 3 disabled
set door_switch 1

output 1 GPIO25
output 2 awning
output 3 disabled
o1 100
set awning 55

t1 25
m 1
v 13750
i 3200
```

Tank values are expected as percentages in `simulate` and variable mode. In GPIO mode, the tank pin is read with `analogRead()` and converted to `0–100%` before being sent on `0x1FD02`.

## NVS persistence

The following are saved in ESP32 NVS by `RoguePreferencesRuntime.h`:

- source address,
- Tank 1 percent,
- Tank 2 percent,
- Tank 1–2 assignment text (`ta1`, `ta2`),
- serial prefix,
- serial suffix,
- product name,
- input 1–8 assignment text (`ia1`..`ia8`),
- output 1–10 assignment text (`oa1`..`oa10`).

Assignments saved by older builds as separate mode/pin keys (`t1m`/`t1p` etc.) are converted to assignment text on first boot and the old keys are removed. Old `variable` assignments had no name, so they become `tank1`, `input1`, `output1` and so on.

Use `defaults` to restore the built-in defaults.

## Current limits

- Compile-tested with arduino-cli and the esp32 core 3.3.12 (M5Stack-ATOM and ESP32 Dev Module). Not yet tested on hardware.
- Tank GPIO mode currently uses fixed raw ADC scaling `0..4095 -> 0..100%`; calibration can be added later.
- GPIO output PWM is linear duty with no gamma correction, and is active-high only.
- GPIO inputs are read as active-high using `pinMode(pin, INPUT)`.
- GPIO6–11 are blocked only on the original ESP32; flash/PSRAM pins on other ESP32 variants are not blocked.
- Master OFF sets all outputs to 0% but does not stop outputs being switched on again while master is off.
- Object 2 is still the captured/static object image; changing serial/name/source address changes live identity frames but does not rewrite the embedded Object 2 image.
- BLE/RBus emulation is not included.

## CAN safety

Use a bench setup first. Running this emulator on the same bus as a live Redarc system means it will actively transmit frames. Keep its source address unique and avoid source `0x30` unless the real Rogue is unplugged.

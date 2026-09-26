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

`RogueObject2.h` stores the captured Object 2 image. `RoguePreferences.h` holds only the built-in default values, one `name = value;` per line, with no types or code. The types are declared in `struct RogueDefaults` in `TVMS_Rogue_Emulator.ino`, which includes the file inside `RogueDefaults::load()`; the code reads the values as `defaults.<name>`. `RoguePreferencesRuntime.h` owns the structs, NVS schema, validation, and load/save helpers.

## Hardware target

Primary target:

- ESP32, such as M5Stack Atom Lite
- CAN transceiver/base, such as M5Stack Atomic CAN Base / CA-IS3050G
- REDARC CAN bus at 250 kbit/s
- Extended CAN IDs

Default CAN pins (change them in `RoguePreferences.h`):

| Signal | ESP32 pin |
|---|---|
| CAN TX | `GPIO22` |
| CAN RX | `GPIO19` |

```cpp
can_tx_pin = 22;
can_rx_pin = 19;
```

At boot the sketch checks the CAN pins. If TX and RX are the same pin, TX is input-only, or either is a flash pin (GPIO6–11 on the original ESP32), CAN stays off and the Serial Monitor says why. The CAN pins can't be assigned to a tank, input or output.

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
tank1    = "GPIO34";
tank2    = "grey_water";
input_1  = "GPIO33";
input_2  = "disabled";
output_1 = "GPIO25";
```

The shipped defaults are `simulate` for every tank, input and output.

After changing the defaults file, run `defaults` from Serial Monitor to clear NVS and reload the built-in defaults.

## GPIO checks

A GPIO assignment is refused (from Serial) or the channel is set to `disabled` with a message (at boot) when the pin:

- is not a GPIO on the chip,
- is GPIO6–11 on the original ESP32 (wired to the SPI flash),
- is one of the CAN TX/RX pins (`can_tx_pin` / `can_rx_pin`, default `GPIO22` / `GPIO19`),
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

Whether each output is dimmable comes from the Rogue configuration (Object 2), exactly as a real Rogue's programming defines it. At boot, and after every configuration write, the sketch reads each output's settings from the object (channel `0x0C`..`0x15`, output settings key `0x06`, dimmable flag key `0x01`, switchable flag key `0x02`). The result is used for both:

- the `0x1FD0E` capability byte sent on CAN (bit 7 `0x80` = dimmable), and
- how the output behaves on its GPIO.

| Programmed as | `0x1FD0E` | GPIO drive | Output level |
|---|---|---|---|
| dimmable | `0x83` | LEDC PWM, duty = level | 0–100% as commanded; hold-dim works |
| on/off (switchable) | `0x03` | digital on/off | any non-zero level becomes 100%; hold-dim ignored |
| always on | `0x01` | digital, held HIGH | fixed at 100%; on/off, level, hold-dim and master OFF are ignored |

The factory captured object programs outputs 1–7 as dimmable, output 8 as always on, and outputs 9–10 as on/off. The boot log shows the result, and `io` shows the type per output:

```text
Outputs: 1=dimmable 2=dimmable 3=dimmable 4=dimmable 5=dimmable 6=dimmable 7=dimmable 8=always-on 9=on/off 10=on/off
```

An output assigned `disabled` stays at 0% even if it is programmed always on.

If the object cannot be decoded, the captured capability values are used and a message is printed.

PWM frequency and resolution for dimmable outputs are set in `RoguePreferences.h`:

```cpp
output_pwm_frequency_hz    = 5000;
output_pwm_resolution_bits = 10;
```

Duty is linear in the level (no gamma correction). If no LEDC channel is free for a pin (the original ESP32 has 16, ESP32-S3 has 8, ESP32-C3 has 6), that output falls back to on/off and a message is printed.

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

| DGN | Meaning | Source |
|---|---|---|
| `0x1F108` | load-disconnect configuration | Object 2: Rogue root key 4 (trigger, disconnect/reconnect mV and SOC) |
| `0x1F400` | firmware/version records | fixed |
| `0x1F403` | product name chunks | Object 2: this device's record, key 4 (see [Identity](#identity)) |
| `0x1F404` | serial/device type information | `serial_prefix` / `serial_suffix` settings |
| `0x1F405` | unique/device ID | fixed |
| `0x1FD00` | channel status | live state |
| `0x1FD02` | tanks / voltage / current | live state |
| `0x1FD04` | channel labels | Object 2: channel record key 1 |
| `0x1FD06` | alarm mode and thresholds (tanks, input V/A) | Object 2: tanks record key 5 → 7, input V/A record key 8 → 1 |
| `0x1FD07` | sensor validity/status pages | captured Rogue reply |
| `0x1FD08` | active-channel inventory | captured Rogue reply |
| `0x1FD0A` | channel class, subtype, icon, enabled | Object 2: channel record keys 2, 3 (tanks also key 5 → 4) |
| `0x1FD0C` | sensor engineering metadata | captured Rogue reply |
| `0x1FD0E` | output capabilities | Object 2: channel record key 6 |
| `0x1FD10` | digital-input secondary configuration | captured Rogue reply |
| `0x1FD12` | output levels | live state |
| `0x1FD14` | output activity | live state |

Replies marked Object 2 are built from the active configuration each time they are sent, so they follow a configuration written from RedVision straight away. The object paths are the ones confirmed against a real Rogue in `docs/TVMS_ROGUE_DGN_OBJECT_MAPPING.md` on the `emulated-rogue` branch. That document also explains why `0x1FD07`, `0x1FD08`, `0x1FD0C` and `0x1FD10` are not derived from the object yet: a real Rogue's reply does not follow the object fields found so far, so the captured reply is sent unchanged.

If the active object does not contain a field a reply needs, that reply is not sent, and the Serial Monitor prints `Object2: cannot build 0x1FDxx from the active configuration` once per configuration.

## Identity

The device list in Object 2 (Rogue root key 1) holds a record for every device in the RedVision system: serial prefix, serial suffix, device type and name. The captured object lists two TVMS Rogues:

| Record | Serial | Name |
|---|---|---|
| 0 | 2506156912-0019 | TVMS Rogue (1) |
| 6 | 2606260001-0019 | TVMS Rogue (2) |

The serial number is fixed hardware identity on a real Rogue, so both parts are settings: `serial_prefix` and `serial_suffix` in `RoguePreferences.h`, or `serial <prefix> [suffix]` from Serial Monitor. They are sent on `0x1F404`.

The name comes from Object 2. The emulator uses the **TVMS Rogue record with the same serial, prefix and suffix**, and sends that record's name on `0x1F403`. The default serial `2606260001-0019` selects "TVMS Rogue (2)". Renaming the device in RedVision writes a new object, and the new name is sent straight away.

If no TVMS Rogue record has that serial, the emulator uses the standard name `TVMS Rogue` and prints a message. It does not borrow another Rogue's record. There is no name setting; `name` in Serial Monitor explains where the name comes from.

The boot log shows the result:

```text
Identity: 2606260001-0019 "TVMS Rogue (2)" (name from Object 2)
```

## REDARC Object 2 (configuration)

Object 2 is the Rogue's main configuration object. The sketch starts from the captured factory image embedded in `RogueObject2.h` (4748 bytes, CRC field `0xFA84819A`). RedVision can read it and write a new configuration, as with a real Rogue. The active object lives in RAM, and every part of the sketch that uses the configuration reads from it.

### Reading

| CAN service / response | Meaning |
|---|---|
| `0x0E85` | select object, for example object `0x02`; acknowledged on `0x0280` |
| `0x0E86` | read object block, offset + length little-endian |
| `0x0281` | returned object data, 8-byte chunks |
| `0x0284` | returned length and CRC-32C trailer |
| `0x0E89` | close the session; acknowledged on `0x0280` |

- If the selected object is `0x02`, reads return bytes from the active object.
- If the request reads beyond the object end, the sketch pads with `0xFF`.
- Unsupported selected objects return `0xFF` data with a valid block CRC.
- Trailer CRC uses CRC-32C / Castagnoli reflected polynomial `0x82F63B78`.
- Block reads above `8192` bytes are refused to protect ESP32 RAM.

### Writing (programming)

Writes are transactional, ported from the ESPHome `emulated-rogue` emulator:

| CAN service | Meaning | Reply on `0x0280` |
|---|---|---|
| `0x0E83` | pre-write query | captured 1,024-byte transfer window on `0x0281`/`0x0284` |
| `0x0E87` | start a write to the selected object (must be `0x02`) | `00` OK / `03` error |
| `0x0E81` | write data frames, collected into the current block (max 1,024 bytes) | none |
| `0x0E88` | end of block: offset + CRC-32C of the block | `01` busy, then `00` OK / `03` error |
| `0x0E89` | close the write session | `00` OK / `03` error |
| `0x0E8A` | commit | `00` OK / `03` error |

Blocks can arrive in any order and may repeat; RedVision writes page `0x0000` (the header and whole-object CRC) last. The commit is accepted only when:

- every byte up to the declared length has been received,
- the declared length is 12–8192 bytes and the whole-object CRC-32C matches, and
- the object was saved to NVS.

Anything else (a bad block CRC, a missing page, a bad whole-object CRC, a failed save or an interrupted write) is answered with `03` and leaves the previous configuration in place.

After a successful commit the new object is used immediately, with no reboot:

- the device name and output types (dimmable / on-off / always-on) are re-read, and identity and `0x1FD0E` capabilities are re-broadcast,
- GPIO outputs switch between PWM and on/off to match,
- `0x0E86` reads return the new object byte-for-byte.

The committed object is saved in NVS (namespace `rogueobj`, key `obj2`) and loaded at the next boot. If the saved object fails its length/CRC checks at boot, the factory object is used instead.

To go back to the factory configuration, run `factory` from Serial Monitor. `defaults` does not touch the saved configuration.

NVS size: the default partition has 20 KB of NVS, and replacing a saved object briefly needs room for two copies. Objects up to about 7 KB save reliably; the captured object is 4.7 KB. If a save fails, the commit is rejected and the previous configuration stays.

### Self-test

At boot, and when you run `crc`, the sketch checks the active object's declared length and whole-object CRC-32C (with bytes `8..11` zeroed):

```text
Object2 (factory) size=4748 declared_len=4748 stored_crc=0xFA84819A calc_crc=0xFA84819A OK
```

After a configuration write it shows `(saved)` and the new CRC.

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
serial <prefix> [suffix]  set/persist this device's serial (selects its Object 2 name record)

save                  save settings to NVS
defaults              restore default persisted settings
factory               erase the saved Object 2 configuration and use the factory one
crc                   run Object 2 CRC self-test
identity              send identity frames now
send                  send status frames now
```

Examples:

```text
sa 0x36
serial 2606260001 0x0013

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
- input 1–8 assignment text (`ia1`..`ia8`),
- output 1–10 assignment text (`oa1`..`oa10`),
- the committed Object 2 configuration (separate namespace `rogueobj`, key `obj2`; see [Writing](#writing-programming)).

Assignments saved by older builds as separate mode/pin keys (`t1m`/`t1p` etc.) are converted to assignment text on first boot and the old keys are removed. Old `variable` assignments had no name, so they become `tank1`, `input1`, `output1` and so on.

Use `defaults` to restore the built-in defaults, and `factory` to erase the saved Object 2 configuration.

## Current limits

- Compile-tested with arduino-cli and the esp32 core 3.3.12 (M5Stack-ATOM and ESP32 Dev Module). Not yet tested on hardware.
- Tank GPIO mode currently uses fixed raw ADC scaling `0..4095 -> 0..100%`; calibration can be added later.
- GPIO output PWM is linear duty with no gamma correction, and is active-high only.
- Configuration writes were tested against a simulated write sequence built from the ESPHome emulator notes, not yet against RedVision on a real bus.
- `0x1FD07`, `0x1FD08`, `0x1FD0C` and `0x1FD10` still send the captured Rogue replies.
- GPIO inputs are read as active-high using `pinMode(pin, INPUT)`.
- GPIO6–11 are blocked only on the original ESP32; flash/PSRAM pins on other ESP32 variants are not blocked.
- Master OFF sets all outputs to 0% but does not stop outputs being switched on again while master is off.
- Changing the serial or source address from Serial Monitor does not rewrite Object 2.
- BLE/RBus emulation is not included.

## CAN safety

Use a bench setup first. Running this emulator on the same bus as a live Redarc system means it will actively transmit frames. Keep its source address unique and avoid source `0x30` unless the real Rogue is unplugged.

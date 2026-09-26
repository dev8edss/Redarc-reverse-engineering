# Prime / TVMS1280 Arduino emulator

Open this sketch in Arduino IDE:

```text
arduinoIDE/Prime_1280_Emulator/Prime_1280_Emulator.ino
```

Supporting files:

```text
arduinoIDE/Prime_1280_Emulator/PrimeObject2.h
arduinoIDE/Prime_1280_Emulator/PrimePreferences.h
arduinoIDE/Prime_1280_Emulator/PrimePreferencesRuntime.h
```

## What it emulates

This is a standalone ESP32/TWAI Arduino sketch for a REDARC Prime / TVMS1280 style node.

It is now structured like the Rogue emulator:

- `PrimePreferences.h` is the editable defaults file.
- `PrimePreferencesRuntime.h` owns the NVS settings schema and assignment parser.
- `Prime_1280_Emulator.ino` owns the CAN protocol, Object 2 read/write, runtime state and GPIO behavior.

Prime-specific channel layout used by this sketch:

| Prime feature | Count | Captured channel range |
|---|---:|---|
| Digital inputs | 3 | `0x01..0x03` |
| Outputs | 11 | `0x04..0x0E` |
| Master switch | 1 | `0x0F` |
| Tanks / analogue levels | 6 | `0x15..0x1A` |

## Captured Object 2

`PrimeObject2.h` contains Prime Object 2 from `readwrite changed 6.csv`:

```text
Declared length: 2628 bytes
Stored whole-object CRC-32C: 0x3815A305
Captured source address: 0x24
```

The sketch supports the same REDARC object services as the Rogue emulator:

| Service | Meaning |
|---|---|
| `0x0E85` | select object |
| `0x0E86` | read object block |
| `0x0281` | returned object data |
| `0x0284` | returned length + CRC trailer |
| `0x0E83` | write capability query |
| `0x0E87` | write open |
| `0x0E81` | write data |
| `0x0E88` | write block commit |
| `0x0E89` | close |
| `0x0E8A` | final commit |

Valid written Object 2 images are saved to ESP32 NVS and loaded at boot. `factory` erases the saved object and restores the embedded one.

## Source address

The real Prime / TVMS1280 in the captures uses source address `0x24`.

This emulator defaults to:

```text
0x25
```

That makes it safer to power on while a real Prime is still present.

To use it as a direct replacement for the real Prime, disconnect the real Prime and run:

```text
sa 0x24
```

## Preference defaults

Edit this file:

```text
PrimePreferences.h
```

Each assignment can be:

| Assignment text | Mode | Meaning |
|---|---|---|
| `GPIO34` | pin | Bound to that ESP32 GPIO. Tanks read analog ADC; inputs read digital; outputs drive PWM. |
| `simulate` | simulated | Internal emulator value only. |
| `disabled` | disabled | Channel is forced off/zero and commands are ignored. |
| anything else | variable | Named variable driven with `set <name> <value>`. |

Examples:

```cpp
tank1 = "GPIO34";
tank2 = "fresh_water";
input_1 = "GPIO33";
output_1 = "GPIO25";
output_11 = "inverter";
```

After changing `PrimePreferences.h`, run this in Serial Monitor:

```text
defaults
```

## GPIO behavior

- Tank GPIO mode uses `analogRead()` and maps raw ADC `0..4095` to `0..100%`.
- Input GPIO mode is active-high: LOW = off, HIGH = on.
- Output GPIO mode uses LEDC PWM, so output level `0..100%` becomes PWM duty.
- GPIO6–11 are blocked on original ESP32 because they are flash pins.
- CAN TX/RX pins cannot be assigned to a tank/input/output.
- A GPIO can only be assigned to one channel.

Default CAN pins:

```cpp
can_tx_pin = 22;
can_rx_pin = 19;
```

## Serial Monitor commands

Open Serial Monitor at `115200`.

```text
help
status
io
crc
identity
send
factory
defaults
save

sa <0x01-0xFE>
serial <prefix> [suffix]
name <text>

tank <1-6> <GPIO<n>|simulate|disabled|variable-name>
t1 <0-100>
t2 <0-100>
...
t6 <0-100>

input <1-3> <GPIO<n>|simulate|disabled|variable-name>
in1 <0|1>
in2 <0|1>
in3 <0|1>

output <1-11> <GPIO<n>|simulate|disabled|variable-name>
o1 <0-100>
o2 <0-100>
...
o11 <0-100>

set <variable-name> <value>
```

Example:

```text
sa 0x25

tank 1 GPIO34
tank 2 fresh_water
set fresh_water 80

input 1 GPIO33
input 2 door_switch
set door_switch 1

output 1 GPIO25
output 11 inverter
set inverter 100
```

## Config DGN replies

The sketch still uses captured Prime/1280 config replies for:

- `0x1FD04`
- `0x1FD06`
- `0x1FD07`
- `0x1FD08`
- `0x1FD0A`
- `0x1FD0C`
- `0x1FD0E`
- `0x1FD10`

Runtime status/tank/input/output values are live and come from the assignment system.

## Current limits

- Not compile-tested in Arduino IDE here.
- Prime config DGN replies are captured frames, not decoded dynamically from Object 2 yet.
- Output PWM is generic `0..100%` duty; Prime-specific output capability bits are still sent from the captured frames.
- This emulates a Prime node on CAN only; it does not update the broader RedVision system configuration to add a second Prime module.

# Prime / TVMS1280 Arduino emulator

Open this sketch in Arduino IDE:

```text
arduinoIDE/Prime_1280_Emulator/Prime_1280_Emulator.ino
```

Supporting file:

```text
arduinoIDE/Prime_1280_Emulator/PrimeObject2.h
```

## What it emulates

This is a standalone ESP32/TWAI Arduino sketch for a REDARC Prime / TVMS1280 style node.

It currently supports:

- CAN/TWAI at 250 kbit/s, extended IDs.
- Default CAN pins: TX `GPIO22`, RX `GPIO19`.
- Default source address `0x25` so it does not collide with a real Prime at `0x24`.
- Device type `0x0E`.
- Prime Object 2 readback from `readwrite changed 6.csv`.
- Object 2 startup CRC/self-test.
- Object read services:
  - `0x0E85` select object,
  - `0x0E86` read object block,
  - `0x0281` data response,
  - `0x0284` length/CRC trailer.
- Object write/programming services:
  - `0x0E83` write capability,
  - `0x0E87` write open,
  - `0x0E81` write data,
  - `0x0E88` block commit,
  - `0x0E89` close,
  - `0x0E8A` final commit.
- Saved Object 2 configuration in ESP32 NVS after a valid write.
- Captured Prime/1280 config DGN replies for:
  - `0x1FD04`,
  - `0x1FD06`,
  - `0x1FD07`,
  - `0x1FD08`,
  - `0x1FD0A`,
  - `0x1FD0C`,
  - `0x1FD0E`,
  - `0x1FD10`.

## Source address

The real Prime / TVMS1280 in the captures uses source address `0x24`.

This emulator defaults to:

```text
0x25
```

That makes it safer to power on while a real Prime is still present.

To use it as a direct replacement for the real Prime, disconnect the real Prime and run this from Serial Monitor:

```text
sa 0x24
```

## Serial Monitor commands

Open Serial Monitor at `115200`.

```text
help
status
crc
identity
send
factory
save

sa <0x01-0xFE>
serial <prefix> [suffix]
name <text>
o<n> <0-100>
```

Output command mapping:

```text
o1  -> Prime channel 0x04
o11 -> Prime channel 0x0E
```

## Object 2

Embedded factory object:

```text
Length: 2628 bytes
Stored whole-object CRC-32C: 0x3815A305
```

At boot the sketch checks the declared length and recalculates the whole-object CRC-32C with bytes `8..11` zeroed.

Expected line:

```text
Prime Object2 (factory) size=2628 declared_len=2628 stored_crc=0x3815A305 calc_crc=0x3815A305 OK
```

Use `factory` to erase any saved Object 2 from NVS and return to the embedded object.

## Current limits

- This is a first Prime emulator sketch, separate from the Rogue emulator.
- It has not been compile-tested in Arduino IDE here.
- DGN config replies are captured/static frames, not yet dynamically rebuilt from Object 2.
- Runtime tank/voltage/status pages are minimal captured Prime frames.
- GPIO pin/variable/simulated mappings from the Rogue sketch have not yet been ported to Prime.
- If you run it as a second Prime at `0x25`, RedVision still needs a system configuration that actually contains a second Prime module. A different CAN source address alone may not create a second UI module.

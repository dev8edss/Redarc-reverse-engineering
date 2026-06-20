# RedVision / TVMS CAN reverse-engineering notes

This document summarises the current DBC, ESPHome bridge setup, and the J1939-style extended CAN ID layout used by the RedVision / TVMS system.

## CAN ID structure

Each 29-bit CAN ID is treated as:

```text
CAN ID = Priority + message type / PGN-style field + Source Address
```

Example:

```text
0x13F10208
```

Working breakdown:

```text
Priority = upper arbitration bits
Message  = 0x13F102 / PGN-style group 0x3F102 depending on mask convention
Source   = 0x08 = Battery Sensor
```

For this project, the full 29-bit CAN ID is always recorded in the DBC and ESPHome components to avoid ambiguity.

## Source addresses

| Address | Device |
|---:|---|
| `0x01` | Manager30 |
| `0x08` | Battery Sensor |
| `0x20` | RedVision display 1 / command source |
| `0x21` | RedVision display 2 |
| `0x24` | TVMS1280 |
| `0x30` | TVMS Rouge |
| `0xFA` | Diagnostic / request / readout tool traffic |

## Hardware capability map

| Device | Hardware / role | Status |
|---|---|---|
| Manager30 `0x01` | Battery manager / charger integration | confirmed |
| Battery Sensor `0x08` | Battery shunt / BMS sensor | confirmed |
| RedVision 1 `0x20` | Display used to press buttons and send commands | confirmed |
| RedVision 2 `0x21` | Display / status display | confirmed |
| TVMS1280 `0x24` | 10 relay outputs, 1 inverter output, 6 tank inputs, 2 temperature sensors, 2 voltage inputs, 3 digital inputs | confirmed hardware; voltage-input fields are partly diagnostic; digital inputs are exposed as test/candidate entities |
| TVMS Rouge `0x30` | 10 dimmable outputs, 8 hardware button inputs, 2 analog tank sensors, input voltage, input current candidate | confirmed hardware; input-current decode is diagnostic/candidate |

Important corrections:

- TVMS1280 does **not** have Rouge-style hardware button/dimming inputs.
- TVMS1280 **does** have 3 digital inputs on channels `0x01`, `0x02`, and `0x03`; ESPHome exposes candidate binary sensors from the 1280 channel feedback frame for testing.
- Button/dimming-active data seen so far belongs to TVMS Rouge hardware button inputs and/or RedVision display activity, not TVMS1280.
- TVMS1280 output state should be taken from `0x1BFD0024`, not from button-style frames.

## Output and channel mapping

### TVMS1280 output channels

The active ESPHome component now uses zero-based labels `Output 0` through `Output 9`.

| TVMS1280 output label | Channel ID | Status |
|---:|---:|---|
| Output 0 | `0x04` | assumed from sequence |
| Output 1 | `0x05` | assumed from sequence |
| Output 2 | `0x06` | assumed from sequence |
| Output 3 | `0x07` | assumed from sequence |
| Output 4 | `0x08` | observed in feedback group |
| Output 5 | `0x09` | confirmed toggled |
| Output 6 | `0x0A` | confirmed toggled |
| Output 7 | `0x0B` | assumed from sequence |
| Output 8 | `0x0C` | assumed from sequence |
| Output 9 | `0x0D` | assumed from sequence |
| Inverter | `0x0E` | observed/confirmed command pattern |

TVMS1280 command frame:

```text
CAN ID: 0x0F002420
Source: RedVision 1 / 0x20
Target hardware: TVMS1280 / 0x24

Data:
CB 00 FF <channel> <state> 00 00 00

state:
00 = Off
01 = On
```

Observed inverter examples:

```text
ON:  0F002420  CB 00 FF 0E 01 00 00 00
OFF: 0F002420  CB 00 FF 0E 00 00 00 00
```

TVMS1280 feedback frame:

```text
CAN ID: 0x1BFD0024
Source: TVMS1280 / 0x24

D1    = base channel
D2-D8 = states for base+0 through base+6
0x00  = Off
0x01  = On
0xF8 / 0xFF = unavailable / ignore
```

Example:

```text
1BFD0024  08 F8 01 00 F8 F8 F8 00
```

Decode:

```text
base = 0x08
D3   = channel 0x09 = ON
D4   = channel 0x0A = OFF
D8   = channel 0x0E = OFF / inverter state candidate
```

### TVMS Rouge output channels

| TVMS Rouge output | Channel ID | Status |
|---:|---:|---|
| Output 1 | `0x0C` | confirmed |
| Output 2 | `0x0D` | observed/toggled |
| Output 3 | `0x0E` | observed/toggled and dim activity |
| Output 4 | `0x0F` | observed/toggled and dim activity |
| Output 5 | `0x10` | observed/toggled |
| Output 6 | `0x11` | observed/toggled; hardware button example `0% -> 85%` |
| Output 7 | `0x12` | observed/toggled; hardware button example `0% -> 70%` |
| Output 8 | `0x13` | observed in grouped status/level frames |
| Output 9 | `0x14` | observed/toggled |
| Output 10 | `0x15` | confirmed/observed |

Rouge ON/OFF command frame:

```text
CAN ID: 0x0F003020
Source: RedVision 1 / 0x20
Target hardware: TVMS Rouge / 0x30

Data:
CB 00 FF <channel> <state> 00 00 00

state:
00 = Off
01 = On
```

Known behaviour:

- The CAN ON command turns a Rouge output on at `100%`.
- The Rouge hardware button can turn an output on at remembered brightness, e.g. `85%` or `70%`.
- The external CAN command for “recall remembered brightness” is still unknown.

## Rouge dimming status

Rouge dimming is now working through a timed hold/ramp protocol.

```text
Dim down:
0x0F053020  <channel> 01 01 05 00 FF FF FF

Dim up:
0x0F053020  <channel> 01 64 05 00 FF FF FF

Release / stop dimming:
0x0F053020  <channel> 01 FF 00 00 FF FF FF
```

Component behaviour:

- Home Assistant sends a target brightness.
- The component decides whether to dim up or down from the latest `0x1BFD1230` feedback.
- It sends timed hold pulses and release frames until the feedback reaches the target/deadband.
- The HA light entity holds the requested brightness while HA-driven dimming is active.
- The separate `Output X Level` diagnostic sensor continues to show the real hardware brightness during the ramp.

## Signal table

| Device | CAN ID | Message / PGN-style group | MUX | Signal | Bytes | Decode | Status |
|---|---:|---:|---:|---|---|---|---|
| Manager30 `0x01` | `0x03F20A01` | `0x03F20A` | — | `Manager_Output_Current_A` | D1-D4 | raw / 1000 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| Manager30 `0x01` | `0x03F20A01` | `0x03F20A` | — | `Manager_Battery_Voltage` | D5-D6 | raw × 0.001 V | CONFIRMED CORRECT DO NOT EDIT |
| Manager30 `0x01` | `0x03F20401` | `0x03F204` | — | `AC_Input_Voltage` | D5-D6 | little-endian raw V | CONFIRMED CORRECT DO NOT EDIT |
| Manager30 `0x01` | `0x03F20401` | `0x03F204` | — | `DC_Input_Voltage_Raw` | D7-D8 | signed raw | UNCONFIRMED PENDING TEST |
| Manager30 `0x01` | `0x03F20801` | `0x03F208` | — | `Solar_Input_Current_A` | D1-D4 | raw / 1000 - 1000 | WORKING IN ESPHOME / VERIFY DBC EXPORT |
| Manager30 `0x01` | `0x03F20801` | `0x03F208` | — | `Solar_Input_Voltage` | D5-D6 | raw × 0.001 V | WORKING IN ESPHOME |
| Manager30 `0x01` | `0x03F20801` | `0x03F208` | — | `Solar_Input_Power_W` | derived | current × voltage | DERIVED IN ESPHOME/HA |
| Manager30 `0x01` | `0x03FCD601` | `0x03FCD6` | D1=`0x00` | `Solar_Energy_Wh` | D2-D5 | uint32 little-endian Wh | CONFIRMED FROM 14→19 Wh CAPTURE |
| Battery Sensor `0x08` | `0x13F10208` | `0x13F102` | — | `Battery_Current_A` | D1-D4 | raw / 1000 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| Battery Sensor `0x08` | `0x13F10208` | `0x13F102` | — | `Battery_Voltage` | D5-D6 | raw × 0.001 V | CONFIRMED CORRECT DO NOT EDIT |
| Battery Sensor `0x08` | `0x13F10208` | `0x13F102` | — | `Battery_Temperature` | D7 | raw - 60 | CONFIRMED CORRECT DO NOT EDIT |
| Battery Sensor `0x08` | `0x13F10408` | `0x13F104` | — | `Battery_SOC_Percent` | D1 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| RedVision 1 `0x20` | `0x13F28020` | `0x13F280` | — | `RV1_Battery_Current_Display_A` | D1-D2 | raw / 10 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| RedVision 1 `0x20` | `0x13F28020` | `0x13F280` | — | `RV1_Device_Current_Display_A` | D5-D6 | raw / 10 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| RedVision 2 `0x21` | `0x13F28221` | `0x13F282` | — | `RV2_Manager_Output_Current_Display_A` | D7-D8 | raw / 10 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| TVMS Rouge `0x30` | `0x1BFD0230` | `0x1BFD02` | D1=`0x09` | `WaterTank1_Percent` | D2 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS Rouge `0x30` | `0x1BFD0230` | `0x1BFD02` | D1=`0x09` | `WaterTank2_Percent` | D3 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS Rouge `0x30` | `0x1BFD1230` | `0x1BFD12` | D1 base | `Output_Level_BasePlus0..6` | D2-D8 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS Rouge `0x30` | `0x1BFD1430` | `0x1BFD14` | D1 base | `ButtonActivity_BasePlus0..6` | D2-D8 | `0x02=active`, `0x00=inactive`, ignore others | CONFIRMED CORRECT DO NOT EDIT |
| TVMS Rouge `0x30` | `0x1BFD0030` | `0x1BFD00` | D1 base | `CoarseStatus_BasePlus0..6` | D2-D8 | `0x00=off`, `0x01=on`, `0xF8/0xFF/special=ignore` | PARTLY CONFIRMED |
| TVMS Rouge `0x30` | `0x13F10830` | `0x13F108` | — | `TVMSRouge_Input_Voltage` | D1-D2 | uint16 little-endian × 0.01 V | CONFIRMED CORRECT DO NOT EDIT |
| TVMS Rouge `0x30` | `0x1BFD0230` | `0x1BFD02` | D1=`0x16` | `TVMSRouge_Input_Current_A_Candidate` | D3 | raw / 10 A | DIAGNOSTIC CANDIDATE; label pages identify item 0x17 as Input Current; needs controlled current-change test |
| RedVision 1 `0x20` | `0x0F003020` | `0x0F0030` | — | `TVMSRouge_Output_Command` | D4/D5 | D4 channel, D5 `0x00/0x01` | CONFIRMED COMMAND PATTERN |
| RedVision 1 `0x20` | `0x0F053020` | `0x0F0530` | — | `TVMSRouge_Dim_Command` | D1-D4 | D1 channel, D3 direction `1/100/255` | CONFIRMED COMMAND PATTERN |
| TVMS1280 `0x24` | `0x1BFD0024` | `0x1BFD00` | D1 base | `TVMS1280_Feedback_BasePlus0..6` | D2-D8 | `0x00=off`, `0x01=on`, `0xF8/0xFF=ignore` | CONFIRMED CORRECT DO NOT EDIT |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x14` | `TVMS1280_Temp1_C` | D2 | raw - 100 | CONFIRMED CORRECT DO NOT EDIT |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x11` | `TVMS1280_Temp2_C` | D6 | raw - 100 | CONFIRMED CORRECT DO NOT EDIT |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x14` | `TVMS1280_Tank1_Percent` | D4 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x14` | `TVMS1280_Tank2_Percent` | D6 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x17` | `TVMS1280_Tank3_Percent` | D2 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x17` | `TVMS1280_Tank4_Percent` | D4 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x17` | `TVMS1280_Tank5_Percent` | D6 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x1A` | `TVMS1280_Tank6_Percent` | D2 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS1280 `0x24` | `0x13F10824` | `0x13F108` | — | `TVMS1280_Supply_Voltage` | D1-D2 | uint16 little-endian × 0.01 V | CONFIRMED / interpreted as supply voltage |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x11` | `TVMS1280_Voltage_Input_1_Candidate` | D2 | raw / 10 V | DIAGNOSTIC CANDIDATE; expected near 0 V when disconnected |
| TVMS1280 `0x24` | `0x1BFD0224` | `0x1BFD02` | D1=`0x14` | `TVMS1280_Voltage_Input_2_Candidate` | D2 | raw / 10 V | DIAGNOSTIC CANDIDATE; observed around 11.9–12.0 V |
| RedVision 1 `0x20` | `0x0F002420` | `0x0F0024` | — | `TVMS1280_Output_Command` | D4/D5 | D4 channel, D5 `0x00/0x01` | CONFIRMED COMMAND PATTERN |
| TVMS1280 `0x24` | `0x1BFD0024` candidate | `0x1BFD00` | D1 base | `TVMS1280_Digital_Inputs_1_to_3` | channels `0x01`-`0x03` | `0x00=off`, `0x01=on`, `0xF8/0xFF=ignore` | TEST CANDIDATE |

## Derived values

| Derived value | Formula | Notes |
|---|---|---|
| `Device_Current_A` from source nodes | `Manager_Output_Current_A - Battery_Current_A` | Battery current is positive while charging and negative while discharging. |
| `Solar_Input_Power_W` exact/live | `Solar_Input_Current_A × Solar_Input_Voltage` | Standard DBC cannot multiply fields, so ESPHome/Home Assistant should calculate exact live watts. |
| `Solar_Energy_Wh` | `0x03FCD601 D1=0x00 D2-D5 uint32 little-endian` | Capture showed 14 Wh through 19 Wh. |
| `TVMS1280_Digital_Inputs_1_to_3` | candidate from `0x1BFD0024` channel status | Hardware channels are `0x01`-`0x03`; HA binary sensors are present for testing and need one-at-a-time toggle confirmation. |

## Current decoding patterns

The source devices use clean 32-bit current values centred at `1,000,000`:

```text
Current_A = raw / 1000 - 1000
```

Examples:

```text
raw 1,000,000 = 0 A
raw 1,020,000 = +20 A
raw   990,000 = -10 A
raw   820,000 = -180 A
```

The RedVision display/rebroadcast values use 16-bit display values centred at `10,000`:

```text
Display_Current_A = raw / 10 - 1000
```

Examples:

```text
raw 10,000 = 0 A
raw 10,200 = +20 A
raw  9,900 = -10 A
```


## TVMS Rouge input current candidate

Rouge text/label pages identify item `0x16` as **Input Voltage** and item `0x17` as **Input Current**. The current candidate decode is in the grouped `0x1BFD0230` frame:

```text
CAN ID: 0x1BFD0230
D1 = 0x16
D3 = item 0x17 Input Current candidate
Scale: D3 / 10 = A
```

Observed payloads:

```text
16 C2 2F FF FF FF FF FF
16 C1 2F FF FF FF FF FF
16 BC 2F FF FF FF FF FF
```

`0x2F / 10 = 4.7 A`. Treat this as a diagnostic/candidate signal until a controlled load-current test confirms the value changes with Rouge input current.

## TVMS1280 digital inputs

TVMS1280 hardware has **3 digital inputs** on channels `0x01`, `0x02`, and `0x03`.

ESPHome exposes candidate binary sensors using `0x1BFD0024` channel feedback. Do not map them to Rouge button/dimming-active frames. A one-at-a-time toggle capture is still needed before marking the live-state decode confirmed.

## ESPHome device setup

The ESPHome device is:

```text
M5Stack Atom Lite / ESP32-PICO
+
Atom CAN Base with CA-IS3050G transceiver
```

Pins:

| Function | Pin |
|---|---|
| CAN TX | `GPIO22` |
| CAN RX | `GPIO19` |

Bus settings:

```text
Bit rate: 250KBPS
Frame type: extended 29-bit CAN IDs
```

Use:

```yaml
can_mode: NORMAL
```

for control-capable builds that send TVMS1280/Rouge commands.

Use:

```yaml
can_mode: LISTENONLY
```

only for passive monitor builds.

## RJ45 CAN pinout

Observed RedVision / TVMS RJ45 pinout:

| RJ45 pin | Function | Status |
|---:|---|---|
| 4 | CAN L | confirmed |
| 5 | CAN H | confirmed |
| 8 | Ground | confirmed |
| unknown | 24 V supply | unconfirmed |

Do not assume the 24 V pin until it has been verified.

## ESPHome published values

Current full component/YAML set can publish:

- Manager30 output current
- Manager30 battery voltage
- Manager30 AC input voltage
- Manager30 solar input current
- Manager30 solar input voltage
- Manager30 derived solar power
- Manager30 solar energy/yield Wh
- Battery Sensor current
- source-derived device current
- Battery SOC
- Battery voltage
- Battery temperature
- RedVision 1 battery/device display currents
- RedVision 2 manager display current
- TVMS Rouge Tank 1 and Tank 2
- TVMS Rouge Outputs 1-10 as dimmable lights
- TVMS Rouge Output 1-10 actual level sensors
- TVMS Rouge Output 1-10 button/dimming-active binary sensors
- TVMS Rouge input voltage
- TVMS Rouge input current candidate
- TVMS1280 Outputs 0-9 as switches
- TVMS1280 inverter switch
- TVMS1280 output feedback state from `0x1BFD0024`
- TVMS1280 Tank 1 through Tank 6
- TVMS1280 Temperature 1 and Temperature 2
- TVMS1280 supply voltage
- TVMS1280 Voltage Input 1/2 diagnostic candidates

## ESPHome and Home Assistant notes

- Control-capable YAML must use CAN `NORMAL` mode.
- External component cache can hide schema updates; clear `/data/external_components/*` after pushing component changes.
- If ESPHome rejects new YAML keys such as `button_status_id`, `input_status_id`, `button_states`, `input_voltage`, `supply_voltage`, or `solar_energy`, the external component schema loaded by ESPHome is stale or missing the latest patch.
- Home Assistant entity IDs may differ slightly from the ESPHome names if entities were renamed in the UI.

## Confirmation status policy

- `CONFIRMED CORRECT DO NOT EDIT` means the decode should not be changed unless a new capture explicitly disproves it.
- `PARTLY CONFIRMED` means the frame purpose and core decode are understood, but some byte meanings remain unknown.
- `DIAGNOSTIC CANDIDATE` means useful in Home Assistant for testing, but not yet locked into the DBC as a final physical signal.
- Do not rename confirmed signals when adding experimental control or dimming features.

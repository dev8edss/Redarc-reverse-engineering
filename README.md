# RedVision / TVMS CAN Decode

Latest confirmed sensor DBC reference: `Current_SOC_v49_TVMS1280_Tank5_raw_percent.dbc`

Latest experimental/control DBC reference: `Current_SOC_v58_TVMS1280_outputs_04_0D_Rouge_dimming.dbc`

Latest ESPHome transmit/control YAML reference: `redvision_tvms_atom_lite_cais3050g_v59_rouge_timed_dimming.yaml`

This document summarises the current DBC, ESPHome bridge setup, and the J1939-style CAN ID layout used by the RedVision / TVMS system.

## CAN ID structure

Each 29-bit CAN ID is treated as:

```text
CAN ID = Priority + PGN + Source Address
```

Example:

```text
0x13F10208
```

Breakdown:

```text
Priority = 4
PGN      = 0xF102
Source   = 0x08 = Battery Sensor
```

The priority affects bus arbitration. Lower priority number wins arbitration first, but it usually does not change the data decode.

The PGN is the message type.

The source address identifies which device sent the message.

## Source addresses

| Address | Device |
|---:|---|
| `0x01` | Manager30 |
| `0x08` | Battery Sensor |
| `0x20` | Redvision 1 |
| `0x21` | Redvision 2 |
| `0x24` | TVMS 1280 |
| `0x30` | TVMS Rouge |

## Hardware capability map

| Device | Hardware / role | Status |
|---|---|---|
| Manager30 `0x01` | Battery manager / charger integration | confirmed |
| Battery Sensor `0x08` | Battery shunt / BMS sensor | confirmed |
| Redvision 1 `0x20` | Display used to press buttons and send commands | confirmed |
| Redvision 2 `0x21` | Display / status display | confirmed |
| TVMS 1280 `0x24` | 10 relay outputs, 1 inverter output, 6 tank inputs, 2 temperature sensors | confirmed hardware |
| TVMS Rouge `0x30` | 10 dimmable outputs, 8 hardware button inputs, 2 analog tank sensors | confirmed hardware |

Important correction:

- TVMS 1280 does **not** have hardware button inputs.
- Any button press data should be attributed to Redvision displays or TVMS Rouge hardware button inputs, not TVMS 1280.
- TVMS 1280 input/status frames should be labelled as tank/status/input-state only, not buttons.

## Output and channel mapping

### TVMS 1280 output channels

Working assumption:

| TVMS 1280 output | Channel ID | Status |
|---|---:|---|
| Output 1 | `0x04` | unconfirmed assumed |
| Output 2 | `0x05` | unconfirmed assumed |
| Output 3 | `0x06` | unconfirmed assumed |
| Output 4 | `0x07` | unconfirmed assumed |
| Output 5 | `0x08` | unconfirmed assumed |
| Output 6 | `0x09` | confirmed toggled |
| Output 7 | `0x0A` | confirmed toggled |
| Output 8 | `0x0B` | unconfirmed assumed |
| Output 9 | `0x0C` | unconfirmed assumed |
| Output 10 | `0x0D` | unconfirmed assumed |
| Inverter | `0x0E` | observed from Redvision display capture |

TVMS 1280 command frame:

```text
CAN ID: 0x0F002420
Source: Redvision 1 / 0x20
Target hardware: TVMS 1280 / 0x24

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

### TVMS Rouge output channels

Known Rouge output channel range:

| TVMS Rouge output | Channel ID | Status |
|---|---:|---|
| Output 1 | `0x0C` | confirmed mapping from user |
| Output 2 | `0x0D` | observed/toggled |
| Output 3 | `0x0E` | observed/toggled and dim-capture activity |
| Output 4 | `0x0F` | observed/toggled and dim-capture activity |
| Output 5 | `0x10` | observed/toggled |
| Output 6 | `0x11` | observed/toggled |
| Output 7 | `0x12` | not observed in large log 2 |
| Output 8 | `0x13` | specifically not toggled in large log 2 |
| Output 9 | `0x14` | observed/toggled |
| Output 10 | `0x15` | confirmed mapping from user / observed |

Rouge on/off command frame:

```text
CAN ID: 0x0F003020
Source: Redvision 1 / 0x20
Target hardware: TVMS Rouge / 0x30

Data:
CB 00 FF <channel> <state> 00 00 00

state:
00 = Off
01 = On
```

Rouge output level feedback:

```text
CAN ID: 0x1BFD1230
Source: TVMS Rouge / 0x30

Meaning:
Output level feedback, 0-100 %
```

Current behaviour seen in Home Assistant testing:

- Rouge outputs turn on and off correctly.
- Rouge output level feedback remains at `100%` while the output is on.
- The level feedback can be used as an on/off state source: `0% = off`, `>0% = on`.
- This does not prove that dimming control works.

## Rouge dimming status

Rouge dimming is **not confirmed working** from ESPHome.

The earlier absolute-brightness implementation did not work. The likely reason is that captured dimming traffic appears to behave like a Redvision button-hold dim command, not a direct absolute brightness set command.

Candidate dimming frames from capture:

```text
Dim down / low target candidate:
0x0F053020  <channel> 01 01 05 00 FF FF FF

Dim up / high target candidate:
0x0F053020  <channel> 01 64 05 00 FF FF FF

Release / stop dimming:
0x0F053020  <channel> 01 FF 00 00 FF FF FF
```

Status:

| Item | Status |
|---|---|
| Rouge on/off command | works |
| Rouge level feedback | works as on/off feedback; reports 100% while output is on |
| Rouge absolute dim command | not working / unconfirmed |
| Rouge timed-hold dim command | experimental / unconfirmed |
| ESPHome dimmable light entities | experimental only until dimming is validated |

Recommendation:

- Keep Rouge outputs as simple switches if reliable control is required.
- Use Rouge output level feedback only for state tracking.
- Treat all dimming transmit frames as experimental until a new capture confirms the exact Redvision sequence and required timing.

## Signal table

| Device | CAN ID | PGN | MUX | Signal | Bytes | Decode | Status |
|---|---:|---:|---:|---|---|---|---|
| Manager30 `0x01` | `0x03F20A01` | `0x3F20A` | — | `Manager_Output_Current_A` | D1-D4 | raw / 1000 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| Manager30 `0x01` | `0x03F20A01` | `0x3F20A` | — | `Manager_Battery_Voltage` | D5-D6 | raw × 0.001 V | CONFIRMED CORRECT DO NOT EDIT |
| Battery Sensor `0x08` | `0x13F10208` | `0x3F102` | — | `Battery_Current_A` | D1-D4 | raw / 1000 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| Battery Sensor `0x08` | `0x13F10208` | `0x3F102` | — | `Battery_Voltage` | D5-D6 | raw × 0.001 V | CONFIRMED CORRECT DO NOT EDIT |
| Battery Sensor `0x08` | `0x13F10208` | `0x3F102` | — | `Battery_Temperature` | D7 | raw - 60 | CONFIRMED CORRECT DO NOT EDIT |
| Battery Sensor `0x08` | `0x13F10408` | `0x3F104` | — | `Battery_SOC_Percent` | D1 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| Redvision 1 `0x20` | `0x13F28020` | `0x3F280` | — | `RV1_Battery_Current_Display_A` | D1-D2 | raw / 10 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| Redvision 1 `0x20` | `0x13F28020` | `0x3F280` | — | `RV1_Device_Current_Display_A` | D5-D6 | raw / 10 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| Redvision 2 `0x21` | `0x13F28221` | `0x3F282` | — | `RV2_Manager_Output_Current_Display_A` | D7-D8 | raw / 10 - 1000 | CONFIRMED CORRECT DO NOT EDIT |
| TVMS Rouge `0x30` | `0x1BFD0230` | `0x3FD02` | — | `WaterTank_MUX` | D1 | mux | CONFIRMED CORRECT DO NOT EDIT |
| TVMS Rouge `0x30` | `0x1BFD0230` | `0x3FD02` | 0x09 | `WaterTank1_Percent` | D2 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS Rouge `0x30` | `0x1BFD0230` | `0x3FD02` | 0x09 | `WaterTank2_Percent` | D3 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0x3FD02` | — | `TVMS1280_Status_MUX` | D1 | mux | CONFIRMED CORRECT DO NOT EDIT |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0x3FD02` | 0x14 | `TVMS1280_Temp1_C` | D2 | raw - 100 | CONFIRMED CORRECT DO NOT EDIT |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0x3FD02` | 0x14 | `TVMS1280_Tank1_Percent` | D4 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0x3FD02` | 0x14 | `TVMS1280_Tank2_Percent` | D6 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0x3FD02` | 0x17 | `TVMS1280_Tank3_Percent` | D2 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0x3FD02` | 0x17 | `TVMS1280_Tank4_Percent` | D4 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0x3FD02` | 0x17 | `TVMS1280_Tank5_Percent` | D6 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0x3FD02` | 0x1A | `TVMS1280_Tank6_Percent` | D2 | raw % | CONFIRMED CORRECT DO NOT EDIT |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0x3FD02` | 0x11 | `TVMS1280_Temp2_C` | D6 | raw - 100 | CONFIRMED CORRECT DO NOT EDIT |
| Manager30 `0x01` | `0x03F20401` | `0x3F204` | — | `AC_Input_Voltage` | D5-D6 | raw V | CONFIRMED CORRECT DO NOT EDIT |
| Manager30 `0x01` | `0x03F20401` | `0x3F204` | — | `DC_Input_Voltage_Raw` | D7-D8 | raw signed | UNCONFIRMED PENDING TEST |
| Manager30 `0x01` | `0x03FCD601` | `0x3FCD6` | — | `Solar_Energy_MUX` | D1 | mux | UNCONFIRMED PENDING TEST |
| Manager30 `0x01` | `0x03FCD601` | `0x3FCD6` | 0x00 | `Solar_Energy_Wh` | D2-D3 | raw Wh | UNCONFIRMED PENDING TEST |
| Manager30 `0x01` | `0x03F20801` | `0x3F208` | — | `Solar_Input_Power_W_Est` | D1-D4 | raw × 0.022 - 22000 | UNCONFIRMED PENDING TEST |
| Manager30 `0x01` | `0x03F20801` | `0x3F208` | — | `Solar_Input_Voltage` | D5-D6 | raw × 0.001 V | UNCONFIRMED PENDING TEST |

## Derived values

| Derived value | Formula | Notes |
|---|---|---|
| `Device_Current_A` from source nodes | `Manager_Output_Current_A - Battery_Current_A` | `Battery_Current_A` is positive while charging and negative while discharging. |
| `Solar_Input_Power_W` exact/live | `Solar_Input_Current_A × Solar_Input_Voltage` | Standard DBC cannot multiply fields. ESPHome/Home Assistant should calculate exact live watts. |
| `Solar_Input_Power_W_Est` | `raw × 0.022 - 22000` | DBC-only estimate using nominal 22 V conversion from the former solar current field. |

## Current decoding

The source devices use clean 32-bit values centred at `1,000,000`:

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

The Redvision display/rebroadcast values use 16-bit values centred at `10,000`:

```text
Display_Current_A = raw / 10 - 1000
```

Examples:

```text
raw 10,000 = 0 A
raw 10,200 = +20 A
raw  9,900 = -10 A
```

## ESPHome device setup

The ESPHome device is a read-only CAN-to-Home Assistant bridge using:

```text
M5Stack Atom Lite / ESP32-PICO
+
Atom CAN Base with CA-IS3050G transceiver
```

The YAML is configured for the Atom CAN Base pins:

| Function | Pin |
|---|---|
| CAN TX | `GPIO22` |
| CAN RX | `GPIO19` |

The ESP32 uses its internal CAN/TWAI controller. The CA-IS3050G is the CAN transceiver connected to CAN-H and CAN-L.

The YAML uses:

```yaml
can_mode: LISTENONLY
```

This makes the ESP32 passively listen to the existing RedVision / TVMS CAN bus without transmitting frames.

## RJ45 CAN pinout

Observed RedVision / TVMS RJ45 pinout for all devices:

| RJ45 pin | Function | Status |
|---:|---|---|
| 4 | CAN L | confirmed |
| 5 | CAN H | confirmed |
| 8 | Ground | confirmed |
| unknown | 24 V supply | unconfirmed |

Do not assume the 24 V pin until it has been verified with a meter or wiring reference.

## ESPHome source-address configuration

The ESPHome YAML keeps source addresses configurable at the top:

```yaml
manager30_sa: "0x01"
battery_sensor_sa: "0x08"
redvision_1_sa: "0x20"
redvision_2_sa: "0x21"
tvms_1280_sa: "0x24"
tvms_rouge_sa: "0x30"
```

Changing one of these substitutions changes the source address used by the decode logic.

## ESPHome averaging

The YAML averages numeric sensors over 5 seconds before publishing to Home Assistant:

```yaml
sensor_average_interval: 5s
```

Most numeric sensors use:

```yaml
filters:
  - throttle_average: 5s
```

This smooths rapid CAN updates and reduces Home Assistant update noise.

## ESPHome published values

The ESPHome bridge publishes:

- Manager30 output current
- Battery Sensor current
- source-derived device current
- Battery SOC
- Battery voltage
- Battery temperature
- Redvision 1 battery/device display currents
- Redvision 2 manager display current
- TVMS Rouge tank 1 and tank 2
- TVMS1280 tank 1 through tank 6
- TVMS1280 temperature 1 and temperature 2
- solar voltage/current-derived power fields, where included in the YAML version

Rouge output command tracking was intentionally removed from the latest YAML, so the bridge is monitor-only for the confirmed sensor data.

### ESPHome transmit/control YAML versions

Some later YAML versions are no longer monitor-only. Output-control versions must use:

```yaml
can_mode: NORMAL
```

Use `LISTENONLY` only when passive monitoring is required.

Known ESPHome control behaviour:

| Feature | Behaviour |
|---|---|
| TVMS 1280 Output 6 / 7 switches | expected to work using channels `0x09` and `0x0A` |
| TVMS 1280 Inverter switch | expected to work using channel `0x0E` |
| TVMS 1280 Outputs 1-5 and 8-10 | included only from assumed channel sequence `0x04-0x0D`; unconfirmed |
| TVMS Rouge output switches | on/off works |
| TVMS Rouge dimmable light control | not working yet / experimental |
| Rouge HA state tracking | should follow `0x1BFD1230` output level feedback instead of unreliable bitfield-only state |

## Confirmation status

Signals marked `CONFIRMED CORRECT DO NOT EDIT` are locked and should not be changed unless a correction is explicitly requested.

Signals marked `UNCONFIRMED PENDING TEST`, `UNCONFIRMED ASSUMED`, or `EXPERIMENTAL` are included for testing only. Once validated, they can be marked confirmed.

Do not overwrite or rename confirmed sensor decodes when adding output-control or dimming experiments.

# RedVision / TVMS CAN Decode

Latest DBC reference: `Current_SOC_v49_TVMS1280_Tank5_raw_percent.dbc`

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

## Confirmation status

Signals marked `CONFIRMED CORRECT DO NOT EDIT` are locked and should not be changed unless a correction is explicitly requested.

Signals marked `UNCONFIRMED PENDING TEST` are included for testing only. Once validated, they can be marked confirmed.

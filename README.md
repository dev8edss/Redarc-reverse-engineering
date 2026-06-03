# RedVision / TVMS Confirmed CAN Decode

Latest DBC reference: `Current_SOC_v36_TVMS1280_all_6_tanks.dbc`

## Confirmed signals

| Device | CAN ID | PGN | MUX | Signal | Bytes | Decode |
|---|---:|---:|---:|---|---|---|
| Manager30 `0x01` | `0x03F20A01` | `0xF20A` | — | `Manager_Output_Current_A` | `D1-D4` | `raw / 1000 - 1000` |
| Battery Sensor `0x08` | `0x13F10208` | `0xF102` | — | `Battery_Current_A` | `D1-D4` | `raw / 1000 - 1000` |
| Battery Sensor `0x08` | `0x13F10408` | `0xF104` | — | `Battery_SOC_Percent` | `D1` | raw `%` |
| Redvision 1 `0x20` | `0x13F28020` | `0xF280` | — | `RV1_Battery_Current_Display_A` | `D1-D2` | `raw / 10 - 1000` |
| Redvision 1 `0x20` | `0x13F28020` | `0xF280` | — | `RV1_Device_Current_Display_A` | `D5-D6` | `raw / 10 - 1000` |
| Redvision 2 `0x21` | `0x13F28221` | `0xF282` | — | `RV2_Manager_Output_Current_Display_A` | `D7-D8` | `raw / 10 - 1000` |
| TVMS Rouge `0x30` | `0x1BFD0230` | `0xFD02` | `0x09` | `WaterTank1_Percent` | `D2` | raw `%` |
| TVMS Rouge `0x30` | `0x1BFD0230` | `0xFD02` | `0x09` | `WaterTank2_Percent` | `D3` | raw `%` |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0xFD02` | `0x14` | `TVMS1280_Temp1_C` | `D2` | `raw - 100` |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0xFD02` | `0x11` | `TVMS1280_Temp2_C` | `D6` | `raw - 100` |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0xFD02` | `0x14` | `TVMS1280_Tank1_Percent` | `D4` | raw `%` |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0xFD02` | `0x14` | `TVMS1280_Tank2_Percent` | `D6` | raw `%` |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0xFD02` | `0x17` | `TVMS1280_Tank3_Percent` | `D2` | raw `%` |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0xFD02` | `0x17` | `TVMS1280_Tank4_Percent` | `D4` | raw `%` |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0xFD02` | `0x17` | `TVMS1280_Tank5_Percent` | `D6` | `raw × 1.25` |
| TVMS 1280 `0x24` | `0x1BFD0224` | `0xFD02` | `0x1A` | `TVMS1280_Tank6_Percent` | `D2` | raw `%` |

## Derived values

| Derived value | Formula |
|---|---|
| `Device_Current_A` from source nodes | `Manager_Output_Current_A - Battery_Current_A` |

## Source addresses

| Address | Device |
|---:|---|
| `0x01` | Manager30 |
| `0x08` | Battery Sensor |
| `0x20` | Redvision 1 |
| `0x21` | Redvision 2 |
| `0x24` | TVMS 1280 |
| `0x30` | TVMS Rouge |

## Notes

- CAN IDs are J1939-style 29-bit identifiers made from priority, PGN, and source address.
- For the current source values, `Battery_Current_A` is positive when charging into the batteries and negative when discharging.
- Redvision current values are display/rebroadcast values.
- TVMS Rouge tank levels use MUX `0x09`.
- TVMS1280 tank values are multiplexed on CAN ID `0x1BFD0224`.

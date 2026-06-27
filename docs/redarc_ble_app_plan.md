# Redarc BLE App Plan

## Objective

Create a direct Bluetooth app for the ESP32 Redarc CAN bridge so a phone can read live caravan/RV data and change selected settings without needing Home Assistant or Wi-Fi.

## Transport

Use Bluetooth Low Energy with the ESP32 acting as a GATT server.

## First safe scope

1. BLE advertising as `Redarc Bridge`.
2. Live sensor snapshot characteristic.
3. Command characteristic for output control.
4. Settings characteristic for app-safe settings.
5. Flutter app that scans, connects, displays values, and sends sample output commands.

## Next step

Wire real values into the BLE payload from the existing Redarc component state, then replace command logging with validated calls into the existing output and dimming code.

## Safety rules for write commands

- Reject unknown command types.
- Reject channel numbers outside the actual device range.
- Clamp brightness to 0-100 percent.
- Do not expose raw CAN writes in the app.
- Keep master-off as a deliberate action with confirmation in the mobile UI.

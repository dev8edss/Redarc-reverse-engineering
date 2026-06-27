# Redarc BLE ESPHome Component

Starter BLE GATT server component for the ESP32 Redarc bridge.

This is intended to run beside the existing RedVision, TVMS and Manager30 components and expose a direct phone-to-ESP Bluetooth interface.

## Example ESPHome usage

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dev8edss/Redarc-reverse-engineering
      ref: app
    components:
      - redarc_ble

redarc_ble:
  id: redarc_ble_server
  device_name: "Redarc Bridge"
```

The first implementation sends demo values so the mobile app can be developed before real CAN sensors are wired into the BLE payload.

import '../models/redarc_sensor_data.dart';

class RedarcBleService {
  static const serviceUuid = '7b3a0001-2f43-4c52-9f3d-343a5f6b0001';
  static const sensorDataUuid = '7b3a0002-2f43-4c52-9f3d-343a5f6b0001';
  static const commandUuid = '7b3a0003-2f43-4c52-9f3d-343a5f6b0001';
  static const settingsUuid = '7b3a0004-2f43-4c52-9f3d-343a5f6b0001';
  static const deviceInfoUuid = '7b3a0005-2f43-4c52-9f3d-343a5f6b0001';

  RedarcSensorData demoData() {
    return const RedarcSensorData(
      soc: 98,
      batteryVoltage: 13.4,
      batteryCurrent: -4.2,
      solarWatts: 95,
      managerCurrent: 30.6,
      tank1: 75,
    );
  }

  Future<void> scan() async {}
  Future<void> connect(Object device) async {}
  Future<void> setOutput({required int channel, required bool state, int? brightness}) async {}
  Future<void> writeSetting(String key, Object value) async {}
}

class RedarcSensorData {
  const RedarcSensorData({
    this.soc,
    this.batteryVoltage,
    this.batteryCurrent,
    this.solarWatts,
    this.managerCurrent,
    this.tank1,
  });

  final num? soc;
  final num? batteryVoltage;
  final num? batteryCurrent;
  final num? solarWatts;
  final num? managerCurrent;
  final num? tank1;
}

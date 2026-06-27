import 'package:flutter/material.dart';

import '../services/redarc_ble_service.dart';

class BleDashboardScreen extends StatefulWidget {
  const BleDashboardScreen({super.key});

  @override
  State<BleDashboardScreen> createState() => _BleDashboardScreenState();
}

class _BleDashboardScreenState extends State<BleDashboardScreen> {
  final RedarcBleService ble = RedarcBleService();

  Widget metric(String label, Object? value, String unit) {
    return Card(
      child: ListTile(
        title: Text(label),
        trailing: Text(value == null ? '--' : '$value $unit'),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final data = ble.demoData();

    return Scaffold(
      appBar: AppBar(title: const Text('Redarc BLE')),
      body: ListView(
        padding: const EdgeInsets.all(12),
        children: [
          const Text('Bluetooth app scaffold'),
          const SizedBox(height: 12),
          FilledButton.icon(
            onPressed: () => ble.scan(),
            icon: const Icon(Icons.bluetooth_searching),
            label: const Text('Scan'),
          ),
          const Divider(height: 32),
          metric('Battery SOC', data.soc, '%'),
          metric('Battery Voltage', data.batteryVoltage, 'V'),
          metric('Battery Current', data.batteryCurrent, 'A'),
          metric('Solar Power', data.solarWatts, 'W'),
          metric('Manager Output Current', data.managerCurrent, 'A'),
          metric('Tank 1', data.tank1, '%'),
          const SizedBox(height: 12),
          FilledButton(
            onPressed: () => ble.setOutput(channel: 6, state: true, brightness: 85),
            child: const Text('Set Output 6 to 85%'),
          ),
          OutlinedButton(
            onPressed: () => ble.setOutput(channel: 6, state: false),
            child: const Text('Turn Output 6 Off'),
          ),
        ],
      ),
    );
  }
}

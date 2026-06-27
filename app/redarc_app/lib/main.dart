import 'package:flutter/material.dart';

import 'screens/ble_dashboard_screen.dart';

void main() {
  runApp(const RedarcBleApp());
}

class RedarcBleApp extends StatelessWidget {
  const RedarcBleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Redarc BLE',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.red),
        useMaterial3: true,
      ),
      home: const BleDashboardScreen(),
    );
  }
}

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:nckh_app/components/home_page.dart';
import 'package:nckh_app/services/mqtt_service.dart';

void main() {
  runApp(
    // Dùng .value để Provider trỏ vào singleton thay vì tạo instance mới
    ChangeNotifierProvider<MqttService>.value(
      value: MqttService.instance,
      child: const MyApp(),
    ),
  );
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: HomePage(),
    );
  }
}

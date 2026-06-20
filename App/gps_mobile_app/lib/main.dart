import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:nckh_app/components/home_page.dart';
import 'package:nckh_app/services/mqtt_service.dart';
import 'package:nckh_app/services/rfid_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Khởi tạo RfidService: load dữ liệu từ bộ nhớ + lắng nghe MQTT stream
  await RfidService.instance.init();

  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider<MqttService>.value(value: MqttService.instance),
        ChangeNotifierProvider<RfidService>.value(value: RfidService.instance),
      ],
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

import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'mqtt_service.dart';

class RfidService extends ChangeNotifier {
  RfidService._();
  static final RfidService instance = RfidService._();

  static const String _prefsKey = 'saved_rfid_list';

  List<String> _rfidList = [];
  List<String> get rfidList => List.unmodifiable(_rfidList);

  StreamSubscription<String>? _newRfidSub;

  // Gọi 1 lần khi app khởi động (trước runApp)
  Future<void> init() async {
    final prefs = await SharedPreferences.getInstance();
    _rfidList = prefs.getStringList(_prefsKey) ?? [];

    // Lắng nghe thẻ mới từ MQTT
    _newRfidSub = MqttService.instance.newRfidStream.listen(_onNewRfid);
  }

  void _onNewRfid(String rfid) {
    if (_rfidList.contains(rfid)) return;
    _rfidList.add(rfid);
    _persist();
    notifyListeners();
  }

  /// Xóa thẻ khỏi danh sách và gửi lệnh xóa lên ESP32.
  Future<void> removeRfid(String rfid) async {
    _rfidList.remove(rfid);
    await _persist();
    notifyListeners();
    MqttService.instance.publishRemoveCard(rfid);
  }

  Future<void> _persist() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setStringList(_prefsKey, _rfidList);
  }

  @override
  void dispose() {
    _newRfidSub?.cancel();
    super.dispose();
  }
}

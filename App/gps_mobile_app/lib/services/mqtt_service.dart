import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart'
    show ChangeNotifier, debugPrint, debugPrintStack, kIsWeb;
import 'package:latlong2/latlong.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'mqtt_client_helper.dart';

// ─── Broker config ────────────────────────────────────────────────────────────

class _BrokerConfig {
  const _BrokerConfig({
    required this.host,
    required this.port,
    required this.useTls,
    required this.webSocketPath,
    required this.isWebSocketEndpoint,
  });

  final String host;
  final int port;
  final bool useTls;
  final String webSocketPath;
  final bool isWebSocketEndpoint;
}

bool _isValidHost(String host) {
  if (host.isEmpty || host.startsWith('.') || host.endsWith('.')) return false;
  for (final unit in host.codeUnits) {
    final ok = (unit >= 97 && unit <= 122) ||
        (unit >= 65 && unit <= 90) ||
        (unit >= 48 && unit <= 57) ||
        unit == 46 ||
        unit == 45;
    if (!ok) return false;
  }
  return true;
}

// ─── Status enum ──────────────────────────────────────────────────────────────

enum MqttConnectionStatus { disconnected, connecting, connected, error }

// ─── Service ──────────────────────────────────────────────────────────────────

class MqttService extends ChangeNotifier {
  MqttService._();
  static final MqttService instance = MqttService._();

  static const String brokerEndpoint = 'mqtt://broker.chtlab.us:1883';
  static const int _portTcp = 1883;
  static const int _portTls = 8883;
  static const String _username = 'username';
  static const String _password = 'password';
  static const String _subscribeTopic = 'esp32/pubtopic';

  MqttClient? _client;
  bool _isConnecting = false;

  MqttConnectionStatus _status = MqttConnectionStatus.disconnected;
  MqttConnectionStatus get status => _status;

  String _statusMessage = 'Chưa kết nối';
  String get statusMessage => _statusMessage;

  String _lastMessage = '';
  String get lastMessage => _lastMessage;

  String _lastTopic = '';
  String get lastTopic => _lastTopic;

  DateTime? _lastMessageTime;
  DateTime? get lastMessageTime => _lastMessageTime;

  LatLng? _gpsPosition;
  LatLng? get gpsPosition => _gpsPosition;

  final List<({String topic, String payload, DateTime time})> _messageHistory =
      [];
  List<({String topic, String payload, DateTime time})> get messageHistory =>
      List.unmodifiable(_messageHistory);

  bool get isConnected =>
      _client?.connectionStatus?.state == MqttConnectionState.connected;

  // ─── Broker URL parser ──────────────────────────────────────────────────────

  _BrokerConfig _resolveBrokerConfig() {
    final endpoint = brokerEndpoint.trim();
    final uri = Uri.tryParse(endpoint);

    if (uri == null) {
      throw const FormatException('Broker endpoint không hợp lệ');
    }

    final String host = uri.hasScheme ? uri.host : endpoint;
    final String path =
        (uri.hasScheme && uri.path.isNotEmpty) ? uri.path : '/mqtt';

    if (!_isValidHost(host)) {
      throw FormatException('Broker host không hợp lệ: "$host"');
    }

    if (!uri.hasScheme) {
      return _BrokerConfig(
        host: host,
        port: _portTcp,
        useTls: false,
        webSocketPath: '/mqtt',
        isWebSocketEndpoint: false,
      );
    }

    switch (uri.scheme) {
      case 'mqtt':
        return _BrokerConfig(
          host: host,
          port: uri.hasPort ? uri.port : _portTcp,
          useTls: false,
          webSocketPath: path,
          isWebSocketEndpoint: false,
        );
      case 'mqtts':
        return _BrokerConfig(
          host: host,
          port: uri.hasPort ? uri.port : _portTls,
          useTls: true,
          webSocketPath: path,
          isWebSocketEndpoint: false,
        );
      case 'ws':
        return _BrokerConfig(
          host: host,
          port: uri.hasPort ? uri.port : 80,
          useTls: false,
          webSocketPath: path,
          isWebSocketEndpoint: true,
        );
      case 'wss':
        return _BrokerConfig(
          host: host,
          port: uri.hasPort ? uri.port : 443,
          useTls: true,
          webSocketPath: path,
          isWebSocketEndpoint: true,
        );
      default:
        throw FormatException(
          'Scheme "${uri.scheme}" không được hỗ trợ. '
          'Chỉ dùng mqtt://, mqtts://, ws:// hoặc wss://',
        );
    }
  }

  // ─── Connect ────────────────────────────────────────────────────────────────

  Future<void> connect() async {
    if (_isConnecting || isConnected) return;

    _isConnecting = true;
    _setStatus(MqttConnectionStatus.connecting, 'Đang kết nối MQTT…');

    // Yield để UI render trạng thái "Đang kết nối" trước khi block
    await Future.delayed(Duration.zero);

    late final _BrokerConfig cfg;
    try {
      cfg = _resolveBrokerConfig();
    } catch (e) {
      _setStatus(MqttConnectionStatus.error, 'Lỗi cấu hình broker: $e');
      _isConnecting = false;
      return;
    }

    // Web chỉ hỗ trợ WebSocket; mqtt:// và mqtts:// là TCP → không dùng được
    if (kIsWeb && !cfg.isWebSocketEndpoint) {
      _setStatus(
        MqttConnectionStatus.error,
        'Flutter Web chỉ hỗ trợ MQTT qua WebSocket (ws:// hoặc wss://).\n'
        'Endpoint hiện tại dùng TCP — không thể kết nối từ trình duyệt.\n'
        'Hãy chạy app trên thiết bị Android/iOS để dùng MQTT TCP.',
      );
      _isConnecting = false;
      return;
    }

    // Native (mobile/desktop) không dùng ws/wss endpoint
    if (!kIsWeb && cfg.isWebSocketEndpoint) {
      _setStatus(
        MqttConnectionStatus.error,
        'Endpoint ws/wss chỉ dùng cho Flutter Web. '
        'Mobile/Desktop hãy dùng mqtt:// hoặc mqtts://',
      );
      _isConnecting = false;
      return;
    }

    final clientId = 'nckh_app_${DateTime.now().millisecondsSinceEpoch}';

    // Web cần full WebSocket URL; native dùng host thuần
    final brokerArg = kIsWeb
        ? '${cfg.useTls ? 'wss' : 'ws'}://${cfg.host}:${cfg.port}${cfg.webSocketPath}'
        : cfg.host;

    final client = createMqttClient(brokerArg, clientId);
    client.port = cfg.port;
    client.setProtocolV311();
    client.keepAlivePeriod = 20;
    client.logging(on: false);
    client.autoReconnect = true;

    // TLS cho native (no-op trên web)
    applyNativeTlsConfig(client, useTls: cfg.useTls);

    client.onConnected = _onConnected;
    client.onDisconnected = _onDisconnected;
    client.onAutoReconnect = _onAutoReconnect;
    client.onAutoReconnected = () {
      _subscribeToTopic(client);
      _setStatus(MqttConnectionStatus.connected, 'Kết nối lại MQTT thành công');
    };

    client.connectionMessage = MqttConnectMessage()
        .withClientIdentifier(clientId)
        .authenticateAs(_username, _password)
        .startClean();

    try {
      await client.connect().timeout(
        const Duration(seconds: 15),
        onTimeout: () {
          throw TimeoutException('Kết nối MQTT quá thời gian chờ (15 giây)');
        },
      );

      if (client.connectionStatus?.state == MqttConnectionState.connected) {
        _client = client;
        _subscribeToTopic(client);
        client.updates!.listen(_onMessageReceived);
      } else {
        client.disconnect();
        _setStatus(
          MqttConnectionStatus.error,
          'Kết nối thất bại: ${client.connectionStatus?.returnCode}',
        );
      }
    } on TimeoutException catch (e) {
      client.disconnect();
      _setStatus(MqttConnectionStatus.error, e.message ?? 'Quá thời gian chờ');
    } catch (e, st) {
      client.disconnect();
      debugPrint('[MQTT] Lỗi kết nối: $e');
      debugPrintStack(stackTrace: st);
      _setStatus(MqttConnectionStatus.error, 'Lỗi kết nối: $e');
    } finally {
      _isConnecting = false;
    }
  }

  void disconnect() {
    _client?.disconnect();
    _setStatus(MqttConnectionStatus.disconnected, 'Đã ngắt kết nối');
  }

  // ─── Internal ───────────────────────────────────────────────────────────────

  void _subscribeToTopic(MqttClient client) {
    client.subscribe(_subscribeTopic, MqttQos.atLeastOnce);
  }

  void _onMessageReceived(List<MqttReceivedMessage<MqttMessage>> events) {
    try {
      if (events.isEmpty) return;

      final first = events.first;
      final payload = first.payload;
      if (payload is! MqttPublishMessage) return;

      final decoded = MqttPublishPayload.bytesToStringAsString(
        payload.payload.message,
      );

      _lastTopic = first.topic;
      _lastMessage = decoded;
      _lastMessageTime = DateTime.now();

      _messageHistory.insert(0, (
        topic: first.topic,
        payload: decoded,
        time: _lastMessageTime!,
      ));
      if (_messageHistory.length > 50) _messageHistory.removeLast();

      _parseGps(decoded);

      notifyListeners();
    } catch (e, st) {
      debugPrint('[MQTT] Lỗi xử lý tin nhắn: $e');
      debugPrintStack(stackTrace: st);
    }
  }

  // Parse gói tin GPS từ ESP32.
  void _parseGps(String raw) {
    try {
      final map = json.decode(raw);
      if (map is! Map<String, dynamic>) return;

      final gps = map['gps'];
      if (gps is! Map<String, dynamic>) return;

      final dynamic lonField = gps['longitude'];
      final dynamic latField = gps['latitude'];

      if (lonField == null || latField == null) return;

      final double lonValue = (lonField as num).toDouble();
      final double latValue = (latField as num).toDouble();

      // ESP32 hiện tại gửi ngược: field "longitude" chứa giá trị lat (~21),
      // field "latitude" chứa giá trị lon (~105). Hoán đổi lại:
      final double actualLat = lonValue; // ~20.98 → vĩ độ
      final double actualLon = latValue; // ~105.81 → kinh độ

      if (actualLat < -90 || actualLat > 90) return;
      if (actualLon < -180 || actualLon > 180) return;

      _gpsPosition = LatLng(actualLat, actualLon);
    } catch (e) {
      debugPrint('[MQTT] Lỗi parse GPS: $e');
    }
  }

  void _onConnected() {
    _setStatus(MqttConnectionStatus.connected, 'Đã kết nối MQTT');
  }

  void _onDisconnected() {
    if (_status != MqttConnectionStatus.error) {
      _setStatus(MqttConnectionStatus.disconnected, 'Mất kết nối MQTT');
    }
  }

  void _onAutoReconnect() {
    _setStatus(MqttConnectionStatus.connecting, 'Đang tự kết nối lại MQTT…');
  }

  void _setStatus(MqttConnectionStatus newStatus, String message) {
    _status = newStatus;
    _statusMessage = message;
    notifyListeners();
  }

  @override
  void dispose() {
    _client?.disconnect();
    super.dispose();
  }
}

// Conditional export: chọn đúng implementation theo nền tảng.
// - dart.library.html  → Flutter Web  → WebSocket (MqttBrowserClient)
// - dart.library.io    → Native/Desktop → TCP      (MqttServerClient)
export 'mqtt_client_helper_native.dart'
    if (dart.library.html) 'mqtt_client_helper_web.dart';

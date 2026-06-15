import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_browser_client.dart';

MqttClient createMqttClient(String brokerArg, String clientId) {
  return MqttBrowserClient(brokerArg, clientId);
}

// No-op on web — TLS is handled by the ws/wss scheme in the broker URL.
void applyNativeTlsConfig(MqttClient client, {required bool useTls}) {}

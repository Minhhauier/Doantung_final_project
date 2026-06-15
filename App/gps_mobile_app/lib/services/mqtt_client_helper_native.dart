import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

MqttClient createMqttClient(String brokerArg, String clientId) {
  return MqttServerClient(brokerArg, clientId);
}

void applyNativeTlsConfig(MqttClient client, {required bool useTls}) {
  if (client is MqttServerClient) {
    client.secure = useTls;
    if (useTls) {
      client.onBadCertificate = (dynamic cert) => true;
    }
  }
}

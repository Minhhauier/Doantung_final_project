import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:nckh_app/components/chart_page.dart';
import 'package:nckh_app/components/setting_page.dart';
import 'package:nckh_app/services/mqtt_service.dart';

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  int _currentIndex = 0;

  static const List<String> _titles = <String>[
    'Home Page',
    'Map Page',
    'Setting Page',
  ];

  @override
  Widget build(BuildContext context) {
    final List<Widget> pages = [
      const _HomeTab(),
      const ChartPage(),
      const SettingPage(),
    ];

    return Scaffold(
      appBar: AppBar(
        title: Text(_titles[_currentIndex]),
        actions: const [_MqttStatusBadge()],
      ),
      body: IndexedStack(
        index: _currentIndex,
        children: pages,
      ),
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: _currentIndex,
        onTap: (int index) {
          setState(() {
            _currentIndex = index;
          });
        },
        items: const <BottomNavigationBarItem>[
          BottomNavigationBarItem(
            icon: Icon(Icons.home),
            label: 'Home',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.map),
            label: 'Map',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.settings),
            label: 'Setting',
          ),
        ],
      ),
    );
  }
}

// ─── AppBar badge ────────────────────────────────────────────────────────────

class _MqttStatusBadge extends StatelessWidget {
  const _MqttStatusBadge();

  @override
  Widget build(BuildContext context) {
    final status = context.watch<MqttService>().status;
    final (color, icon, label) = switch (status) {
      MqttConnectionStatus.connected => (Colors.green, Icons.wifi, 'Đã kết nối'),
      MqttConnectionStatus.connecting =>
        (Colors.orange, Icons.wifi_find, 'Đang kết nối…'),
      MqttConnectionStatus.error => (Colors.red, Icons.wifi_off, 'Lỗi'),
      MqttConnectionStatus.disconnected =>
        (Colors.grey, Icons.wifi_off, 'Chưa kết nối'),
    };

    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 12),
      child: Row(
        children: [
          Icon(icon, color: color, size: 18),
          const SizedBox(width: 4),
          Text(
            label,
            style: TextStyle(color: color, fontSize: 12),
          ),
        ],
      ),
    );
  }
}

// ─── Home tab ─────────────────────────────────────────────────────────────────

class _HomeTab extends StatefulWidget {
  const _HomeTab();

  @override
  State<_HomeTab> createState() => _HomeTabState();
}

class _HomeTabState extends State<_HomeTab> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      context.read<MqttService>().connect();
    });
  }

  @override
  Widget build(BuildContext context) {
    final mqtt = context.watch<MqttService>();

    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _MqttStatusCard(mqtt: mqtt),
          const SizedBox(height: 16),
          _MqttControlCard(mqtt: mqtt),
          const SizedBox(height: 16),
          if (mqtt.messageHistory.isNotEmpty) _MessageHistoryCard(mqtt: mqtt),
        ],
      ),
    );
  }
}

// ─── Status card ─────────────────────────────────────────────────────────────

class _MqttStatusCard extends StatelessWidget {
  const _MqttStatusCard({required this.mqtt});
  final MqttService mqtt;

  @override
  Widget build(BuildContext context) {
    final (bgColor, iconColor, statusText, statusIcon) = switch (mqtt.status) {
      MqttConnectionStatus.connected => (
          Colors.green.shade50,
          Colors.green,
          'Đã kết nối',
          Icons.check_circle,
        ),
      MqttConnectionStatus.connecting => (
          Colors.orange.shade50,
          Colors.orange,
          'Đang kết nối…',
          Icons.sync,
        ),
      MqttConnectionStatus.error => (
          Colors.red.shade50,
          Colors.red,
          'Lỗi kết nối',
          Icons.error,
        ),
      MqttConnectionStatus.disconnected => (
          Colors.grey.shade100,
          Colors.grey,
          'Chưa kết nối',
          Icons.wifi_off,
        ),
    };

    return Card(
      color: bgColor,
      elevation: 2,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.router, color: iconColor, size: 22),
                const SizedBox(width: 8),
                Text(
                  'MQTT Broker',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        fontWeight: FontWeight.bold,
                      ),
                ),
                const Spacer(),
                Icon(statusIcon, color: iconColor),
                const SizedBox(width: 4),
                Text(
                  statusText,
                  style: TextStyle(
                    color: iconColor,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ],
            ),
            const Divider(height: 20),
            _InfoRow(label: 'Broker', value: 'broker.chtlab.us:1883'),
            const SizedBox(height: 4),
            _InfoRow(label: 'Topic', value: 'esp32/pubtopic'),
            if (mqtt.status == MqttConnectionStatus.error) ...[
              const SizedBox(height: 8),
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Icon(Icons.warning_amber, color: Colors.red, size: 16),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      mqtt.statusMessage,
                      style: const TextStyle(color: Colors.red, fontSize: 12),
                    ),
                  ),
                ],
              ),
            ],
            if (mqtt.status == MqttConnectionStatus.connected &&
                mqtt.lastMessage.isNotEmpty) ...[
              const Divider(height: 20),
              Text(
                'Tin nhắn mới nhất',
                style: TextStyle(
                  fontSize: 12,
                  color: Colors.grey.shade600,
                  fontWeight: FontWeight.w500,
                ),
              ),
              const SizedBox(height: 6),
              Container(
                width: double.infinity,
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: Colors.white,
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(color: Colors.green.shade200),
                ),
                child: Text(
                  mqtt.lastMessage,
                  style: const TextStyle(
                    fontFamily: 'monospace',
                    fontSize: 13,
                  ),
                ),
              ),
              if (mqtt.lastMessageTime != null)
                Padding(
                  padding: const EdgeInsets.only(top: 4),
                  child: Text(
                    _formatTime(mqtt.lastMessageTime!),
                    style: TextStyle(
                      fontSize: 11,
                      color: Colors.grey.shade500,
                    ),
                  ),
                ),
            ],
          ],
        ),
      ),
    );
  }

  String _formatTime(DateTime dt) {
    final local = dt.toLocal();
    return '${local.hour.toString().padLeft(2, '0')}:'
        '${local.minute.toString().padLeft(2, '0')}:'
        '${local.second.toString().padLeft(2, '0')}';
  }
}

// ─── Control card ─────────────────────────────────────────────────────────────

class _MqttControlCard extends StatelessWidget {
  const _MqttControlCard({required this.mqtt});
  final MqttService mqtt;

  @override
  Widget build(BuildContext context) {
    final isConnected = mqtt.status == MqttConnectionStatus.connected;
    final isConnecting = mqtt.status == MqttConnectionStatus.connecting;

    return Card(
      elevation: 2,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        child: Row(
          children: [
            Expanded(
              child: ElevatedButton.icon(
                onPressed: isConnecting ? null : () => mqtt.connect(),
                icon: isConnecting
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: Colors.white,
                        ),
                      )
                    : const Icon(Icons.link),
                label: Text(isConnecting ? 'Đang kết nối…' : 'Kết nối'),
                style: ElevatedButton.styleFrom(
                  backgroundColor:
                      isConnected ? Colors.grey.shade400 : Colors.blue,
                  foregroundColor: Colors.white,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(8),
                  ),
                ),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: ElevatedButton.icon(
                onPressed: isConnected ? () => mqtt.disconnect() : null,
                icon: const Icon(Icons.link_off),
                label: const Text('Ngắt kết nối'),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.red,
                  foregroundColor: Colors.white,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(8),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

// ─── Message history card ─────────────────────────────────────────────────────

class _MessageHistoryCard extends StatelessWidget {
  const _MessageHistoryCard({required this.mqtt});
  final MqttService mqtt;

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 2,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                const Icon(Icons.history, size: 20),
                const SizedBox(width: 8),
                Text(
                  'Lịch sử tin nhắn',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        fontWeight: FontWeight.bold,
                      ),
                ),
                const Spacer(),
                Text(
                  '${mqtt.messageHistory.length} tin',
                  style: TextStyle(
                    fontSize: 12,
                    color: Colors.grey.shade600,
                  ),
                ),
              ],
            ),
            const Divider(height: 16),
            ListView.separated(
              shrinkWrap: true,
              physics: const NeverScrollableScrollPhysics(),
              itemCount:
                  mqtt.messageHistory.length > 10 ? 10 : mqtt.messageHistory.length,
              separatorBuilder: (context2, idx) => const SizedBox(height: 6),
              itemBuilder: (context, index) {
                final msg = mqtt.messageHistory[index];
                return Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 10,
                    vertical: 8,
                  ),
                  decoration: BoxDecoration(
                    color: index == 0
                        ? Colors.blue.shade50
                        : Colors.grey.shade50,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(
                      color: index == 0
                          ? Colors.blue.shade200
                          : Colors.grey.shade200,
                    ),
                  ),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Expanded(
                        child: Text(
                          msg.payload,
                          style: const TextStyle(
                            fontFamily: 'monospace',
                            fontSize: 13,
                          ),
                        ),
                      ),
                      const SizedBox(width: 8),
                      Text(
                        _formatTime(msg.time),
                        style: TextStyle(
                          fontSize: 11,
                          color: Colors.grey.shade500,
                        ),
                      ),
                    ],
                  ),
                );
              },
            ),
          ],
        ),
      ),
    );
  }

  String _formatTime(DateTime dt) {
    final local = dt.toLocal();
    return '${local.hour.toString().padLeft(2, '0')}:'
        '${local.minute.toString().padLeft(2, '0')}:'
        '${local.second.toString().padLeft(2, '0')}';
  }
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

class _InfoRow extends StatelessWidget {
  const _InfoRow({required this.label, required this.value});
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Text(
          '$label: ',
          style: const TextStyle(
            fontWeight: FontWeight.w500,
            fontSize: 13,
          ),
        ),
        Expanded(
          child: Text(
            value,
            style: const TextStyle(fontSize: 13),
            overflow: TextOverflow.ellipsis,
          ),
        ),
      ],
    );
  }
}

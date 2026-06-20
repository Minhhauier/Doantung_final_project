import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:nckh_app/components/card_management_page.dart';
import 'package:nckh_app/components/chart_page.dart';
import 'package:nckh_app/components/setting_page.dart';
import 'package:nckh_app/services/mqtt_service.dart';
import 'package:nckh_app/services/rfid_service.dart';

// ─── Shell ────────────────────────────────────────────────────────────────────

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  int _currentIndex = 0;

  static const _navItems = <BottomNavigationBarItem>[
    BottomNavigationBarItem(icon: Icon(Icons.dashboard), label: 'Bảng điều khiển'),
    BottomNavigationBarItem(icon: Icon(Icons.map_outlined), label: 'Bản đồ'),
    BottomNavigationBarItem(icon: Icon(Icons.style_outlined), label: 'Quản lý thẻ'),
    BottomNavigationBarItem(icon: Icon(Icons.settings_outlined), label: 'Cài đặt'),
  ];

  @override
  Widget build(BuildContext context) {
    final rfidCount = context.watch<RfidService>().rfidList.length;
    final pages = [
      const _DashboardTab(),
      const ChartPage(),
      const CardManagementPage(),
      const SettingPage(),
    ];

    final titles = [
      'Bảng điều khiển',
      'Bản đồ GPS',
      'Quản lý thẻ${rfidCount > 0 ? ' ($rfidCount)' : ''}',
      'Cài đặt',
    ];

    return Scaffold(
      backgroundColor: const Color(0xFFF2F4F8),
      appBar: AppBar(
        backgroundColor: const Color(0xFF1565C0),
        foregroundColor: Colors.white,
        elevation: 0,
        title: Text(
          titles[_currentIndex],
          style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 18),
        ),
        actions: const [_MqttAppBarChip()],
      ),
      body: IndexedStack(index: _currentIndex, children: pages),
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: _currentIndex,
        onTap: (i) => setState(() => _currentIndex = i),
        selectedItemColor: const Color(0xFF1565C0),
        unselectedItemColor: Colors.grey.shade500,
        backgroundColor: Colors.white,
        elevation: 8,
        items: _navItems,
      ),
    );
  }
}

// ─── AppBar chip ──────────────────────────────────────────────────────────────

class _MqttAppBarChip extends StatelessWidget {
  const _MqttAppBarChip();

  @override
  Widget build(BuildContext context) {
    final status = context.watch<MqttService>().status;
    final (dot, label) = switch (status) {
      MqttConnectionStatus.connected => (Colors.greenAccent, 'Online'),
      MqttConnectionStatus.connecting => (Colors.orangeAccent, 'Đang kết nối'),
      MqttConnectionStatus.error => (Colors.redAccent, 'Lỗi'),
      MqttConnectionStatus.disconnected => (Colors.white54, 'Offline'),
    };

    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
        decoration: BoxDecoration(
          color: Colors.white.withValues(alpha: 0.15),
          borderRadius: BorderRadius.circular(20),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              width: 7,
              height: 7,
              decoration: BoxDecoration(color: dot, shape: BoxShape.circle),
            ),
            const SizedBox(width: 5),
            Text(label,
                style: const TextStyle(color: Colors.white, fontSize: 12)),
          ],
        ),
      ),
    );
  }
}

// ─── Dashboard tab ────────────────────────────────────────────────────────────

class _DashboardTab extends StatefulWidget {
  const _DashboardTab();

  @override
  State<_DashboardTab> createState() => _DashboardTabState();
}

class _DashboardTabState extends State<_DashboardTab> {
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
    final isConnected = mqtt.status == MqttConnectionStatus.connected;

    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(16, 20, 16, 24),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _VehicleStatusCard(mqtt: mqtt),
          const SizedBox(height: 14),
          _MqttConnectionCard(mqtt: mqtt),
          const SizedBox(height: 14),
          _ControlCard(
            isConnected: isConnected,
            hornOn: mqtt.hornStatus,
            lockOn: mqtt.lockStatus,
            onHornChanged: mqtt.setHornStatus,
            onLockChanged: mqtt.setLockStatus,
          ),
          const SizedBox(height: 14),
          _GpsCard(mqtt: mqtt),
        ],
      ),
    );
  }
}

// ─── Vehicle status card ──────────────────────────────────────────────────────

class _VehicleStatusCard extends StatelessWidget {
  const _VehicleStatusCard({required this.mqtt});
  final MqttService mqtt;

  @override
  Widget build(BuildContext context) {
    final (bg, accent, icon, label, sub) = switch (mqtt.vehicleStatus) {
      VehicleStatus.moving => (
          const Color(0xFFE8F5E9),
          const Color(0xFF2E7D32),
          Icons.directions_car,
          'Đang di chuyển',
          'Xe đang trong trạng thái di chuyển',
        ),
      VehicleStatus.stationary => (
          const Color(0xFFE3F2FD),
          const Color(0xFF1565C0),
          Icons.local_parking,
          'Đứng yên',
          'Xe đang đỗ tại chỗ',
        ),
      VehicleStatus.unknown => (
          const Color(0xFFF5F5F5),
          const Color(0xFF757575),
          Icons.help_outline,
          'Không xác định',
          'Chưa nhận tín hiệu GPS',
        ),
    };

    return Container(
      decoration: BoxDecoration(
        color: bg,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: accent.withValues(alpha: 0.3)),
        boxShadow: [
          BoxShadow(
            color: accent.withValues(alpha: 0.08),
            blurRadius: 8,
            offset: const Offset(0, 3),
          ),
        ],
      ),
      padding: const EdgeInsets.all(20),
      child: Row(
        children: [
          Container(
            width: 60,
            height: 60,
            decoration: BoxDecoration(
              color: accent.withValues(alpha: 0.12),
              shape: BoxShape.circle,
            ),
            child: Icon(icon, color: accent, size: 32),
          ),
          const SizedBox(width: 16),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Trạng thái xe',
                  style: TextStyle(
                    fontSize: 12,
                    color: accent.withValues(alpha: 0.7),
                    fontWeight: FontWeight.w500,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  label,
                  style: TextStyle(
                    fontSize: 20,
                    color: accent,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  sub,
                  style: TextStyle(fontSize: 12, color: Colors.grey.shade600),
                ),
              ],
            ),
          ),
          if (mqtt.vehicleStatus == VehicleStatus.moving)
            _PulsingDot(color: accent),
        ],
      ),
    );
  }
}

// Chấm nhấp nháy cho trạng thái đang di chuyển
class _PulsingDot extends StatefulWidget {
  const _PulsingDot({required this.color});
  final Color color;

  @override
  State<_PulsingDot> createState() => _PulsingDotState();
}

class _PulsingDotState extends State<_PulsingDot>
    with SingleTickerProviderStateMixin {
  late final AnimationController _ctrl;
  late final Animation<double> _anim;

  @override
  void initState() {
    super.initState();
    _ctrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 900),
    )..repeat(reverse: true);
    _anim = Tween<double>(begin: 0.4, end: 1.0).animate(_ctrl);
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return FadeTransition(
      opacity: _anim,
      child: Container(
        width: 12,
        height: 12,
        decoration: BoxDecoration(
          color: widget.color,
          shape: BoxShape.circle,
        ),
      ),
    );
  }
}

// ─── MQTT connection card ─────────────────────────────────────────────────────

class _MqttConnectionCard extends StatelessWidget {
  const _MqttConnectionCard({required this.mqtt});
  final MqttService mqtt;

  @override
  Widget build(BuildContext context) {
    final isConnected = mqtt.status == MqttConnectionStatus.connected;
    final isConnecting = mqtt.status == MqttConnectionStatus.connecting;

    final (dotColor, statusText) = switch (mqtt.status) {
      MqttConnectionStatus.connected => (Colors.green, 'Đã kết nối'),
      MqttConnectionStatus.connecting => (Colors.orange, 'Đang kết nối…'),
      MqttConnectionStatus.error => (Colors.red, 'Lỗi kết nối'),
      MqttConnectionStatus.disconnected => (Colors.grey, 'Chưa kết nối'),
    };

    return _Card(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.router_outlined, size: 20, color: Color(0xFF1565C0)),
              const SizedBox(width: 8),
              const Text(
                'Kết nối MQTT',
                style: TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
              const Spacer(),
              Container(
                width: 8,
                height: 8,
                decoration:
                    BoxDecoration(color: dotColor, shape: BoxShape.circle),
              ),
              const SizedBox(width: 6),
              Text(
                statusText,
                style: TextStyle(
                  color: dotColor,
                  fontSize: 13,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          _InfoRow(label: 'Broker', value: 'broker.chtlab.us:1883'),
          const SizedBox(height: 3),
          _InfoRow(label: 'Topic sub', value: 'esp32/pubtopic'),
          if (mqtt.status == MqttConnectionStatus.error) ...[
            const SizedBox(height: 8),
            Container(
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: Colors.red.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.red.shade200),
              ),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Icon(Icons.warning_amber_rounded,
                      color: Colors.red, size: 16),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      mqtt.statusMessage,
                      style: const TextStyle(color: Colors.red, fontSize: 12),
                    ),
                  ),
                ],
              ),
            ),
          ],
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: _ActionButton(
                  label: isConnecting ? 'Đang kết nối…' : 'Kết nối',
                  icon: isConnecting ? Icons.sync : Icons.link,
                  color: isConnected
                      ? Colors.grey.shade400
                      : const Color(0xFF1565C0),
                  loading: isConnecting,
                  onPressed: isConnecting ? null : mqtt.connect,
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _ActionButton(
                  label: 'Ngắt kết nối',
                  icon: Icons.link_off,
                  color: Colors.red.shade400,
                  onPressed: isConnected ? mqtt.disconnect : null,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

// ─── Device control card ──────────────────────────────────────────────────────

class _ControlCard extends StatelessWidget {
  const _ControlCard({
    required this.isConnected,
    required this.hornOn,
    required this.lockOn,
    required this.onHornChanged,
    required this.onLockChanged,
  });

  final bool isConnected;
  final bool hornOn;
  final bool lockOn;
  final ValueChanged<bool> onHornChanged;
  final ValueChanged<bool> onLockChanged;

  @override
  Widget build(BuildContext context) {
    return _Card(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.tune, size: 20, color: Color(0xFF1565C0)),
              const SizedBox(width: 8),
              const Text(
                'Điều khiển thiết bị',
                style: TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
              if (!isConnected) ...[
                const Spacer(),
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                  decoration: BoxDecoration(
                    color: Colors.grey.shade100,
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Text(
                    'Cần kết nối MQTT',
                    style: TextStyle(fontSize: 11, color: Colors.grey.shade500),
                  ),
                ),
              ],
            ],
          ),
          const SizedBox(height: 6),
          const Divider(height: 16),
          _SwitchTile(
            icon: Icons.notifications_active_outlined,
            iconColor: hornOn ? Colors.amber.shade700 : Colors.grey,
            title: 'Điều khiển còi',
            subtitle: hornOn ? 'Còi đang bật' : 'Còi tắt',
            value: hornOn,
            enabled: isConnected,
            onChanged: onHornChanged,
          ),
          const Divider(height: 16),
          _SwitchTile(
            icon: lockOn
                ? Icons.lock_outline
                : Icons.lock_open_outlined,
            iconColor: lockOn ? Colors.red.shade400 : Colors.grey,
            title: 'Khóa điện tử',
            subtitle: lockOn ? 'Đang khóa' : 'Đang mở khóa',
            value: lockOn,
            enabled: isConnected,
            onChanged: onLockChanged,
          ),
        ],
      ),
    );
  }
}

class _SwitchTile extends StatelessWidget {
  const _SwitchTile({
    required this.icon,
    required this.iconColor,
    required this.title,
    required this.subtitle,
    required this.value,
    required this.enabled,
    required this.onChanged,
  });

  final IconData icon;
  final Color iconColor;
  final String title;
  final String subtitle;
  final bool value;
  final bool enabled;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) {
    return Opacity(
      opacity: enabled ? 1.0 : 0.45,
      child: Row(
        children: [
          Container(
            width: 40,
            height: 40,
            decoration: BoxDecoration(
              color: iconColor.withValues(alpha: 0.1),
              borderRadius: BorderRadius.circular(10),
            ),
            child: Icon(icon, color: iconColor, size: 22),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(title,
                    style: const TextStyle(
                        fontSize: 14, fontWeight: FontWeight.w600)),
                Text(subtitle,
                    style: TextStyle(
                        fontSize: 12, color: Colors.grey.shade500)),
              ],
            ),
          ),
          Switch.adaptive(
            value: value,
            onChanged: enabled ? onChanged : null,
            activeTrackColor: const Color(0xFF1565C0),
          ),
        ],
      ),
    );
  }
}

// ─── GPS info card ────────────────────────────────────────────────────────────

class _GpsCard extends StatelessWidget {
  const _GpsCard({required this.mqtt});
  final MqttService mqtt;

  @override
  Widget build(BuildContext context) {
    final pos = mqtt.gpsPosition;
    final time = mqtt.lastGpsTime;

    return _Card(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(
                pos != null ? Icons.gps_fixed : Icons.gps_not_fixed,
                size: 20,
                color:
                    pos != null ? const Color(0xFF1565C0) : Colors.grey,
              ),
              const SizedBox(width: 8),
              const Text(
                'Thông tin GPS',
                style:
                    TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
              const Spacer(),
              if (pos != null)
                Container(
                  padding:
                      const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                  decoration: BoxDecoration(
                    color: Colors.green.shade50,
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(color: Colors.green.shade200),
                  ),
                  child: Text(
                    'Có tín hiệu',
                    style: TextStyle(
                        fontSize: 11, color: Colors.green.shade700),
                  ),
                )
              else
                Container(
                  padding:
                      const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                  decoration: BoxDecoration(
                    color: Colors.grey.shade100,
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Text(
                    'Chờ tín hiệu',
                    style: TextStyle(
                        fontSize: 11, color: Colors.grey.shade500),
                  ),
                ),
            ],
          ),
          const SizedBox(height: 12),
          if (pos == null)
            Center(
              child: Padding(
                padding: const EdgeInsets.symmetric(vertical: 12),
                child: Column(
                  children: [
                    Icon(Icons.satellite_alt,
                        size: 36, color: Colors.grey.shade300),
                    const SizedBox(height: 8),
                    Text(
                      'Chưa nhận được tín hiệu GPS từ ESP32',
                      style: TextStyle(
                          color: Colors.grey.shade400, fontSize: 13),
                    ),
                  ],
                ),
              ),
            )
          else ...[
            _GpsRow(
              label: 'Vĩ độ',
              value: pos.latitude.toStringAsFixed(6),
              icon: Icons.north,
            ),
            const SizedBox(height: 6),
            _GpsRow(
              label: 'Kinh độ',
              value: pos.longitude.toStringAsFixed(6),
              icon: Icons.east,
            ),
            if (time != null) ...[
              const SizedBox(height: 10),
              Row(
                children: [
                  Icon(Icons.access_time,
                      size: 14, color: Colors.grey.shade400),
                  const SizedBox(width: 4),
                  Text(
                    'Cập nhật lần cuối: ${_formatTime(time)}',
                    style: TextStyle(
                        fontSize: 12, color: Colors.grey.shade500),
                  ),
                ],
              ),
            ],
          ],
        ],
      ),
    );
  }

  String _formatTime(DateTime dt) {
    final t = dt.toLocal();
    return '${t.hour.toString().padLeft(2, '0')}:'
        '${t.minute.toString().padLeft(2, '0')}:'
        '${t.second.toString().padLeft(2, '0')}';
  }
}

class _GpsRow extends StatelessWidget {
  const _GpsRow({
    required this.label,
    required this.value,
    required this.icon,
  });
  final String label;
  final String value;
  final IconData icon;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: const Color(0xFFF2F4F8),
        borderRadius: BorderRadius.circular(10),
      ),
      child: Row(
        children: [
          Icon(icon, size: 16, color: const Color(0xFF1565C0)),
          const SizedBox(width: 8),
          Text(label,
              style: TextStyle(
                  fontSize: 13,
                  color: Colors.grey.shade600,
                  fontWeight: FontWeight.w500)),
          const Spacer(),
          Text(value,
              style: const TextStyle(
                  fontSize: 14,
                  fontWeight: FontWeight.w600,
                  fontFamily: 'monospace')),
        ],
      ),
    );
  }
}

// ─── Shared helpers ───────────────────────────────────────────────────────────

class _Card extends StatelessWidget {
  const _Card({required this.child});
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(16),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.06),
            blurRadius: 8,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      padding: const EdgeInsets.all(16),
      child: child,
    );
  }
}

class _ActionButton extends StatelessWidget {
  const _ActionButton({
    required this.label,
    required this.icon,
    required this.color,
    this.loading = false,
    this.onPressed,
  });

  final String label;
  final IconData icon;
  final Color color;
  final bool loading;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    return ElevatedButton.icon(
      onPressed: onPressed,
      icon: loading
          ? const SizedBox(
              width: 15,
              height: 15,
              child: CircularProgressIndicator(
                  strokeWidth: 2, color: Colors.white))
          : Icon(icon, size: 18),
      label: Text(label, style: const TextStyle(fontSize: 13)),
      style: ElevatedButton.styleFrom(
        backgroundColor: color,
        foregroundColor: Colors.white,
        disabledBackgroundColor: Colors.grey.shade200,
        padding: const EdgeInsets.symmetric(vertical: 10),
        shape:
            RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
        elevation: 0,
      ),
    );
  }
}

class _InfoRow extends StatelessWidget {
  const _InfoRow({required this.label, required this.value});
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Text(
          '$label:  ',
          style: TextStyle(
              fontSize: 12,
              color: Colors.grey.shade500,
              fontWeight: FontWeight.w500),
        ),
        Expanded(
          child: Text(
            value,
            style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w500),
            overflow: TextOverflow.ellipsis,
          ),
        ),
      ],
    );
  }
}

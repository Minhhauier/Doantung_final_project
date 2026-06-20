import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:nckh_app/services/mqtt_service.dart';
import 'package:nckh_app/services/rfid_service.dart';

class CardManagementPage extends StatelessWidget {
  const CardManagementPage({super.key});

  @override
  Widget build(BuildContext context) {
    final rfidService = context.watch<RfidService>();
    final mqtt = context.watch<MqttService>();
    final isConnected = mqtt.status == MqttConnectionStatus.connected;
    final cards = rfidService.rfidList;

    return cards.isEmpty
        ? _EmptyState(isConnected: isConnected)
        : ListView.separated(
            padding: const EdgeInsets.fromLTRB(16, 20, 16, 24),
            itemCount: cards.length,
              separatorBuilder: (ctx, idx) => const SizedBox(height: 10),
            itemBuilder: (context, index) {
              final rfid = cards[index];
              return _RfidCard(
                rfid: rfid,
                isConnected: isConnected,
                onAdd: () {
                  mqtt.publishAddCard(rfid);
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(
                      content: Text('Đã gửi lệnh thêm thẻ $rfid'),
                      behavior: SnackBarBehavior.floating,
                      backgroundColor: Colors.green.shade700,
                      duration: const Duration(seconds: 2),
                    ),
                  );
                },
                onDelete: () => _confirmDelete(context, rfid, rfidService),
              );
            },
          );
  }

  Future<void> _confirmDelete(
    BuildContext context,
    String rfid,
    RfidService service,
  ) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        shape:
            RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
        title: const Text('Xóa thẻ RFID'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Bạn muốn xóa thẻ:'),
            const SizedBox(height: 8),
            Container(
              padding:
                  const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
              decoration: BoxDecoration(
                color: Colors.grey.shade100,
                borderRadius: BorderRadius.circular(8),
              ),
              child: Text(
                rfid,
                style: const TextStyle(
                  fontFamily: 'monospace',
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
            const SizedBox(height: 8),
            Text(
              'Thẻ sẽ bị xóa khỏi app và gửi lệnh xóa tới khóa thông minh.',
              style:
                  TextStyle(fontSize: 13, color: Colors.grey.shade600),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Hủy'),
          ),
          ElevatedButton(
            onPressed: () => Navigator.pop(ctx, true),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.red,
              foregroundColor: Colors.white,
              shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(8)),
            ),
            child: const Text('Xóa'),
          ),
        ],
      ),
    );

    if (confirmed == true && context.mounted) {
      await service.removeRfid(rfid);
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Đã xóa thẻ $rfid'),
            behavior: SnackBarBehavior.floating,
            backgroundColor: Colors.red.shade700,
            duration: const Duration(seconds: 2),
          ),
        );
      }
    }
  }
}

// ─── RFID Card ────────────────────────────────────────────────────────────────

class _RfidCard extends StatelessWidget {
  const _RfidCard({
    required this.rfid,
    required this.isConnected,
    required this.onAdd,
    required this.onDelete,
  });

  final String rfid;
  final bool isConnected;
  final VoidCallback onAdd;
  final VoidCallback onDelete;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(14),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.06),
            blurRadius: 8,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      padding: const EdgeInsets.all(14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Container(
                padding: const EdgeInsets.all(8),
                decoration: BoxDecoration(
                  color: const Color(0xFFE3F2FD),
                  borderRadius: BorderRadius.circular(10),
                ),
                child: const Icon(
                  Icons.nfc,
                  color: Color(0xFF1565C0),
                  size: 22,
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'Thẻ RFID',
                      style: TextStyle(
                        fontSize: 11,
                        color: Colors.grey,
                        fontWeight: FontWeight.w500,
                      ),
                    ),
                    Text(
                      rfid,
                      style: const TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                        fontFamily: 'monospace',
                        letterSpacing: 1.2,
                      ),
                    ),
                  ],
                ),
              ),
              // Nút xóa
              IconButton(
                onPressed: onDelete,
                icon: const Icon(Icons.delete_outline, color: Colors.red),
                tooltip: 'Xóa thẻ',
                style: IconButton.styleFrom(
                  backgroundColor: Colors.red.shade50,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(10),
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          SizedBox(
            width: double.infinity,
            child: ElevatedButton.icon(
              onPressed: isConnected ? onAdd : null,
              icon: const Icon(Icons.add_card, size: 18),
              label: const Text('Thêm UUID vào khóa thông minh'),
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF1565C0),
                foregroundColor: Colors.white,
                disabledBackgroundColor: Colors.grey.shade200,
                padding: const EdgeInsets.symmetric(vertical: 10),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(10),
                ),
                elevation: 0,
              ),
            ),
          ),
          if (!isConnected)
            Padding(
              padding: const EdgeInsets.only(top: 6),
              child: Text(
                'Cần kết nối MQTT để gửi lệnh',
                style: TextStyle(fontSize: 11, color: Colors.grey.shade400),
              ),
            ),
        ],
      ),
    );
  }
}

// ─── Empty state ──────────────────────────────────────────────────────────────

class _EmptyState extends StatelessWidget {
  const _EmptyState({required this.isConnected});
  final bool isConnected;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.style_outlined, size: 72, color: Colors.grey.shade300),
            const SizedBox(height: 16),
            Text(
              'Chưa có thẻ RFID nào',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.w600,
                color: Colors.grey.shade500,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              isConnected
                  ? 'Đưa thẻ RFID mới vào đầu đọc của khóa.\nThẻ mới sẽ tự động hiển thị tại đây.'
                  : 'Hãy kết nối MQTT trước,\nsau đó đưa thẻ RFID vào đầu đọc của khóa.',
              textAlign: TextAlign.center,
              style: TextStyle(fontSize: 14, color: Colors.grey.shade400),
            ),
          ],
        ),
      ),
    );
  }
}

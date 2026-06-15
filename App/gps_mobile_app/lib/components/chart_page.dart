import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:provider/provider.dart';
import 'package:nckh_app/services/mqtt_service.dart';

class ChartPage extends StatefulWidget {
  const ChartPage({super.key});

  @override
  State<ChartPage> createState() => _ChartPageState();
}

class _ChartPageState extends State<ChartPage> {
  static const LatLng _defaultPosition = LatLng(21.028511, 105.804817);
  static const double _defaultZoom = 15;

  final MapController _mapController = MapController();

  LatLng _markerPosition = _defaultPosition;
  bool _hasReceivedGps = false;

  @override
  void initState() {
    super.initState();
    MqttService.instance.addListener(_onMqttUpdate);

    // Nếu service đã có GPS từ trước (tab được chuyển lại)
    final existing = MqttService.instance.gpsPosition;
    if (existing != null) {
      _markerPosition = existing;
      _hasReceivedGps = true;
    }
  }

  @override
  void dispose() {
    MqttService.instance.removeListener(_onMqttUpdate);
    super.dispose();
  }

  void _onMqttUpdate() {
    final pos = MqttService.instance.gpsPosition;
    if (pos == null || pos == _markerPosition) return;

    setState(() {
      _markerPosition = pos;
      _hasReceivedGps = true;
    });

    // Di chuyển camera đến vị trí mới, giữ nguyên zoom hiện tại
    try {
      _mapController.move(pos, _mapController.camera.zoom);
    } catch (_) {
      // Camera chưa sẵn sàng (widget chưa render xong) — bỏ qua
    }
  }

  @override
  Widget build(BuildContext context) {
    // Lắng nghe để badge GPS cập nhật trạng thái kết nối
    context.watch<MqttService>();

    return Stack(
      children: [
        FlutterMap(
          mapController: _mapController,
          options: MapOptions(
            initialCenter: _markerPosition,
            initialZoom: _defaultZoom,
          ),
          children: <Widget>[
            TileLayer(
              urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
              userAgentPackageName: 'nckh_app',
            ),
            MarkerLayer(
              markers: <Marker>[
                Marker(
                  point: _markerPosition,
                  width: 56,
                  height: 56,
                  child: const _DeviceMarker(),
                ),
              ],
            ),
            const RichAttributionWidget(
              attributions: <SourceAttribution>[
                TextSourceAttribution('OpenStreetMap contributors'),
              ],
              alignment: AttributionAlignment.bottomLeft,
            ),
          ],
        ),

        // Badge toạ độ + trạng thái GPS
        Positioned(
          top: 12,
          left: 12,
          right: 12,
          child: _GpsInfoBadge(
            position: _hasReceivedGps ? _markerPosition : null,
          ),
        ),

        // Nút về vị trí hiện tại
        if (_hasReceivedGps)
          Positioned(
            bottom: 32,
            right: 12,
            child: FloatingActionButton.small(
              heroTag: 'center_map',
              tooltip: 'Về vị trí thiết bị',
              onPressed: () => _mapController.move(
                _markerPosition,
                _defaultZoom,
              ),
              child: const Icon(Icons.my_location),
            ),
          ),
      ],
    );
  }
}

// ─── Device marker ────────────────────────────────────────────────────────────

class _DeviceMarker extends StatelessWidget {
  const _DeviceMarker();

  @override
  Widget build(BuildContext context) {
    return Stack(
      alignment: Alignment.center,
      children: [
        Container(
          width: 20,
          height: 20,
          decoration: BoxDecoration(
            color: Colors.blue.withValues(alpha: 0.25),
            shape: BoxShape.circle,
            border: Border.all(color: Colors.blue.shade300, width: 1.5),
          ),
        ),
        Container(
          width: 10,
          height: 10,
          decoration: const BoxDecoration(
            color: Colors.blue,
            shape: BoxShape.circle,
          ),
        ),
        const Align(
          alignment: Alignment.topCenter,
          child: Icon(Icons.location_pin, color: Colors.red, size: 32),
        ),
      ],
    );
  }
}

// ─── GPS info badge ───────────────────────────────────────────────────────────

class _GpsInfoBadge extends StatelessWidget {
  const _GpsInfoBadge({required this.position});
  final LatLng? position;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.93),
        borderRadius: BorderRadius.circular(10),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.12),
            blurRadius: 6,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      child: position == null
          ? const Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(Icons.gps_not_fixed, size: 16, color: Colors.grey),
                SizedBox(width: 6),
                Text(
                  'Chưa nhận tín hiệu GPS',
                  style: TextStyle(fontSize: 12, color: Colors.grey),
                ),
              ],
            )
          : Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(Icons.gps_fixed, size: 16, color: Colors.green),
                const SizedBox(width: 6),
                Text(
                  'Lat: ${position!.latitude.toStringAsFixed(6)}  '
                  'Lon: ${position!.longitude.toStringAsFixed(6)}',
                  style: const TextStyle(
                    fontSize: 12,
                    fontFamily: 'monospace',
                    fontWeight: FontWeight.w500,
                  ),
                ),
              ],
            ),
    );
  }
}

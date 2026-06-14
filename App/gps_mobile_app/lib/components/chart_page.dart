import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';

class ChartPage extends StatelessWidget {
  const ChartPage({super.key});

  static const LatLng _initialPosition = LatLng(21.028511, 105.804817);

  @override
  Widget build(BuildContext context) {
    return FlutterMap(
      options: const MapOptions(
        initialCenter: _initialPosition,
        initialZoom: 14,
      ),
      children: <Widget>[
        TileLayer(
          urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
          userAgentPackageName: 'nckh_app',
        ),
        const MarkerLayer(
          markers: <Marker>[
            Marker(
              point: _initialPosition,
              width: 48,
              height: 48,
              child: Icon(
                Icons.location_pin,
                color: Colors.red,
                size: 42,
              ),
            ),
          ],
        ),
        const RichAttributionWidget(
          attributions: <SourceAttribution>[
            TextSourceAttribution(
              'OpenStreetMap contributors',
            ),
          ],
          alignment: AttributionAlignment.bottomLeft,
        ),
      ],
    );
  }
}

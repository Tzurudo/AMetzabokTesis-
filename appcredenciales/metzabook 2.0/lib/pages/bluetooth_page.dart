import 'dart:async';
import 'package:flutter/material.dart';
import 'package:bluetooth_classic/models/device.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../services/bluetooth_manager.dart';
import '../widgets/foco_switch.dart';
import 'settings_page.dart';

class BluetoothPage extends StatefulWidget {
  const BluetoothPage({super.key});

  @override
  State<BluetoothPage> createState() => _BluetoothPageState();
}

class _BluetoothPageState extends State<BluetoothPage> {
  final BluetoothManager _btManager = BluetoothManager();
  StreamSubscription<Device>? _scanSubscription;
  StreamSubscription<String>? _dataSubscription;
  Timer? _scanTimeoutTimer;
  List<Device> _devices = [];
  bool _isScanning = false;
  bool _isConnecting = false;

  List<String> _labels = ['Interruptor 1', 'Interruptor 2', 'Interruptor 3', 'Interruptor 4'];

  @override
  void initState() {
    super.initState();
    _loadLabels();
    _btManager.isConnected.addListener(_onConnectionChanged);
    _btManager.relayStates.addListener(_rebuild);
    _btManager.isGlobalAuto.addListener(_rebuild);
    _btManager.channelNames.addListener(_onNamesChanged);
    _initBluetooth();
  }

  void _rebuild() { if (mounted) setState(() {}); }
  void _onNamesChanged() {
    if (mounted) setState(() => _labels = List.from(_btManager.channelNames.value));
  }

  void _onConnectionChanged() {
    if (!mounted) return;
    setState(() {});
    if (_btManager.isConnected.value) {
      _subscribeToData();
    } else {
      _dataSubscription?.cancel();
      // Mantener la página abierta para reconectar
    }
  }

  void _subscribeToData() {
    _dataSubscription?.cancel();
    _dataSubscription = _btManager.deviceDataStream.listen((line) {
      if (!mounted) return;
      // Confirmaciones visuales de comandos enviados
      if (line == 'TIME_SYNC_OK') {
        _showSnack('✅ Hora sincronizada correctamente');
      } else if (line.startsWith('SCHED_SAVED:')) {
        _showSnack('✅ Horario guardado en el Metzabook');
      } else if (line.startsWith('SCHED_DISABLED:')) {
        _showSnack('🗑 Horario eliminado');
      } else if (line.startsWith('SCHEDS_CLEARED:')) {
        _showSnack('🗑 Todos los horarios del canal eliminados');
      }
    });
  }

  void _showSnack(String msg, {Color color = Colors.green}) {
    if (!mounted) return;
    ScaffoldMessenger.of(context).hideCurrentSnackBar();
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(
      content: Text(msg),
      backgroundColor: color,
      duration: const Duration(seconds: 3),
    ));
  }

  Future<void> _loadLabels() async {
    final prefs = await SharedPreferences.getInstance();
    if (!mounted) return;
    setState(() {
      _labels = [
        prefs.getString('foco1_label') ?? 'Interruptor 1',
        prefs.getString('foco2_label') ?? 'Interruptor 2',
        prefs.getString('foco3_label') ?? 'Interruptor 3',
        prefs.getString('foco4_label') ?? 'Interruptor 4',
      ];
    });
  }

  Future<void> _initBluetooth() async {
    await _btManager.initPermissions();
    if (!_btManager.isConnected.value) await _startScan();
  }

  Future<void> _startScan() async {
    if (_btManager.isConnected.value || _isScanning) return;
    if (!mounted) return;
    setState(() { _devices = []; _isScanning = true; });

    try {
      final paired = await _btManager.getPairedDevices();
      if (mounted) setState(() => _devices = paired);
    } catch (e) { debugPrint("Error vinculados: $e"); }

    try {
      await _scanSubscription?.cancel();
      _scanSubscription = _btManager.onDeviceDiscovered().listen((d) {
        if (!mounted) return;
        if (!_devices.any((x) => x.address == d.address)) {
          setState(() => _devices.add(d));
        }
      });
      await _btManager.startScan();
      _scanTimeoutTimer?.cancel();
      _scanTimeoutTimer = Timer(const Duration(seconds: 15), _stopScan);
    } catch (e) {
      debugPrint("Error scan: $e");
      await _stopScan();
    }
  }

  Future<void> _stopScan() async {
    await _btManager.stopScan();
    await _scanSubscription?.cancel();
    _scanSubscription = null;
    _scanTimeoutTimer?.cancel();
    if (mounted) setState(() => _isScanning = false);
  }

  Future<void> _connectToDevice(Device device) async {
    await _stopScan();
    if (!mounted) return;

    setState(() => _isConnecting = true);
    // Dialog de "Conectando..."
    unawaited(showDialog(
      context: context,
      barrierDismissible: false,
      builder: (_) => const Dialog(
        child: Padding(
          padding: EdgeInsets.all(24),
          child: Row(children: [
            CircularProgressIndicator(),
            SizedBox(width: 20),
            Text("Conectando..."),
          ]),
        ),
      ),
    ));

    try {
      await _btManager.connect(device.address, device.name ?? "Metzabok");
      if (mounted && Navigator.canPop(context)) Navigator.pop(context);
      _showSnack('✅ Conectado a ${device.name ?? "Metzabok"}');
    } catch (e) {
      if (mounted && Navigator.canPop(context)) Navigator.pop(context);
      _showSnack('❌ Error al conectar: $e', color: Colors.red);
    } finally {
      if (mounted) setState(() => _isConnecting = false);
    }
  }

  /// Envía un comando por BT si está conectado.
  void _sendCommand(String command) {
    if (!_btManager.isConnected.value) {
      _showSnack('Sin conexión Bluetooth. Conecta primero.', color: Colors.orange);
      return;
    }
    _btManager.write(command);
  }

  void _syncTime() {
    if (!_btManager.isConnected.value) {
      _showSnack('Sincronizar Hora requiere conexión Bluetooth activa.', color: Colors.orange);
      return;
    }
    final now = DateTime.now();
    final cmd = "SETTIME:"
        "${now.hour.toString().padLeft(2, '0')}:"
        "${now.minute.toString().padLeft(2, '0')}:"
        "${now.second.toString().padLeft(2, '0')}:"
        "${now.day.toString().padLeft(2, '0')}:"
        "${now.month.toString().padLeft(2, '0')}:"
        "${now.year}";
    _btManager.write(cmd);
    // La confirmación viene del stream: TIME_SYNC_OK
  }

  // ── BUILD ──────────────────────────────────────────────────────────────
  @override
  Widget build(BuildContext context) {
    final bool connected = _btManager.isConnected.value;
    final bool hasAnyChannel = connected;

    return PopScope(
      canPop: true,
      onPopInvokedWithResult: (didPop, _) {
        // Al salir en modo MANUAL por BT, apaga todo por seguridad
        if (connected && !_btManager.isGlobalAuto.value) {
          _btManager.write("ALLOFF");
        }
      },
      child: Scaffold(
        appBar: AppBar(
          title: const Text("Metzabok - Control"),
          actions: [
            IconButton(
              icon: const Icon(Icons.settings),
              tooltip: "Configuración",
              onPressed: () => Navigator.push(
                context,
                MaterialPageRoute(builder: (_) => const SettingsPage()),
              ).then((_) => _loadLabels()),
            ),
            if (!connected)
              IconButton(
                icon: _isScanning
                    ? const SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white))
                    : const Icon(Icons.refresh),
                tooltip: "Buscar dispositivos",
                onPressed: _isScanning ? null : _startScan,
              ),
          ],
        ),
        body: Column(
          children: [
            _buildConnectionBanner(connected),
            Expanded(
              child: hasAnyChannel
                  ? _buildControlPanel(connected)
                  : _buildDeviceList(),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildConnectionBanner(bool connected) {
    final Color bg;
    final String text;
    final IconData icon;

    if (connected) {
      bg = Colors.green;
      text = "● Bluetooth Conectado";
      icon = Icons.bluetooth_connected;
    } else {
      bg = Colors.red;
      text = "Sin conexión";
      icon = Icons.bluetooth_disabled;
    }

    return Container(
      color: bg,
      padding: const EdgeInsets.symmetric(vertical: 10, horizontal: 16),
      child: Row(
        children: [
          Icon(icon, color: Colors.white, size: 18),
          const SizedBox(width: 10),
          Expanded(child: Text(text, style: const TextStyle(color: Colors.white, fontWeight: FontWeight.bold))),
        ],
      ),
    );
  }

  Widget _buildDeviceList() {
    return Column(
      children: [
        if (_isScanning)
          const LinearProgressIndicator(),
        Container(
          margin: const EdgeInsets.all(16),
          padding: const EdgeInsets.all(12),
          decoration: BoxDecoration(
            color: Colors.orange[50],
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: Colors.orange[200]!),
          ),
          child: Row(
            children: [
              Icon(Icons.bluetooth_searching, color: Colors.orange[700]),
              const SizedBox(width: 12),
              const Expanded(
                child: Text(
                  "Vincula tu Metzabok en la configuración Bluetooth del teléfono y tócalo aquí para conectar.",
                  style: TextStyle(color: Colors.deepOrange, fontWeight: FontWeight.w500),
                ),
              ),
            ],
          ),
        ),
        if (_devices.isEmpty && !_isScanning)
          const Expanded(child: Center(child: Text("No se encontraron dispositivos", style: TextStyle(color: Colors.grey)))),
        Expanded(
          child: ListView.builder(
            itemCount: _devices.length,
            itemBuilder: (_, i) => ListTile(
              leading: const Icon(Icons.bluetooth, color: Colors.blue),
              title: Text(_devices[i].name ?? "Desconocido"),
              subtitle: Text(_devices[i].address),
              trailing: _isConnecting ? const SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2)) : const Icon(Icons.link),
              onTap: _isConnecting ? null : () => _connectToDevice(_devices[i]),
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildControlPanel(bool connected) {
    final states = _btManager.relayStates.value;
    final isAuto = _btManager.isGlobalAuto.value;

    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          // Tarjeta de Modo Global
          Card(
            margin: const EdgeInsets.only(bottom: 12),
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
            color: isAuto ? Colors.blue[50] : Colors.orange[50],
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        "Modo Global",
                        style: TextStyle(
                          fontSize: 16, fontWeight: FontWeight.bold,
                          color: isAuto ? Colors.blue[800] : Colors.orange[900],
                        ),
                      ),
                      Text(
                        isAuto ? "AUTOMÁTICO" : "MANUAL",
                        style: TextStyle(
                          fontSize: 12, fontWeight: FontWeight.bold,
                          color: isAuto ? Colors.blue : Colors.deepOrange,
                        ),
                      ),
                    ],
                  ),
                  Transform.scale(
                    scale: 1.1,
                    child: Switch(
                      value: isAuto,
                      activeThumbColor: Colors.blue,
                      inactiveThumbColor: Colors.orange,
                      onChanged: (val) {
                        _btManager.isGlobalAuto.value = val;
                        if (val) {
                          _btManager.relayStates.value = {1: false, 2: false, 3: false, 4: false};
                        }
                        _sendCommand(val ? "GLOBAL_AUTO" : "GLOBAL_MANUAL");
                      },
                    ),
                  ),
                ],
              ),
            ),
          ),

          for (int i = 0; i < 4; i++)
            _buildFocoItem(
              channel: i + 1,
              label: _labels[i],
              state: isAuto ? false : (states[i + 1] ?? false),
              isAuto: isAuto,
              onSwitch: (v) => _sendCommand(v ? 'ON${i + 1}' : 'OFF${i + 1}'),
            ),

          const SizedBox(height: 12),

          // Botones de acción
          Wrap(
            spacing: 12,
            runSpacing: 12,
            alignment: WrapAlignment.center,
            children: [
              ElevatedButton.icon(
                icon: const Icon(Icons.flash_off),
                label: const Text("EMERGENCIA: ALL OFF"),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.red,
                  foregroundColor: Colors.white,
                ),
                onPressed: () {
                  _sendCommand('ALLOFF');
                  // Actualizar UI optimistamente
                  _btManager.relayStates.value = {1: false, 2: false, 3: false, 4: false};
                },
              ),
              if (connected)
                ElevatedButton.icon(
                  icon: const Icon(Icons.access_time),
                  label: const Text("Sincronizar Hora"),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.blueGrey,
                    foregroundColor: Colors.white,
                  ),
                  onPressed: _syncTime,
                ),
            ],
          ),

          const SizedBox(height: 16),

          if (connected)
            OutlinedButton.icon(
              icon: const Icon(Icons.bluetooth_disabled, color: Colors.red),
              label: const Text("Desconectar Bluetooth", style: TextStyle(color: Colors.red)),
              onPressed: () async {
                await _btManager.disconnect();
                _showSnack("Bluetooth desconectado", color: Colors.grey);
              },
            ),
        ],
      ),
    );
  }

  Widget _buildFocoItem({
    required int channel,
    required String label,
    required bool state,
    required bool isAuto,
    required Function(bool) onSwitch,
  }) {
    return Card(
      elevation: 2,
      margin: const EdgeInsets.symmetric(vertical: 5),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
        child: FocoSwitch(
          titulo: label,
          estado: state,
          enabled: !isAuto,
          onChanged: isAuto
              ? (_) => _showSnack("Modo AUTOMÁTICO activo. Cambia a MANUAL para control directo.", color: Colors.orange)
              : (v) {
                  final updated = Map<int, bool>.from(_btManager.relayStates.value);
                  updated[channel] = v;
                  _btManager.relayStates.value = updated;
                  onSwitch(v);
                },
        ),
      ),
    );
  }

  @override
  void dispose() {
    _scanSubscription?.cancel();
    _scanTimeoutTimer?.cancel();
    _dataSubscription?.cancel();
    _btManager.isConnected.removeListener(_onConnectionChanged);
    _btManager.relayStates.removeListener(_rebuild);
    _btManager.isGlobalAuto.removeListener(_rebuild);
    _btManager.channelNames.removeListener(_onNamesChanged);
    _btManager.stopScan();
    super.dispose();
  }
}

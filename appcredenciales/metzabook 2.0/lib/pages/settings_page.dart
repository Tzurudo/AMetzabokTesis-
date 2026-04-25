import 'package:flutter/material.dart';
import 'dart:async';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:bluetooth_classic/models/device.dart';
import '../services/bluetooth_manager.dart';

class SettingsPage extends StatefulWidget {
  const SettingsPage({super.key});

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  final BluetoothManager _btManager = BluetoothManager();

  final TextEditingController _foco1Controller = TextEditingController();
  final TextEditingController _foco2Controller = TextEditingController();
  final TextEditingController _foco3Controller = TextEditingController();
  final TextEditingController _foco4Controller = TextEditingController();

  final TextEditingController _ssidController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();
  
  StreamSubscription<Device>? _scanSubscription;
  StreamSubscription<String>? _dataSubscription;
  Timer? _scanTimeoutTimer;

  List<Device> _devices = [];
  bool isConnecting = false;
  bool _permissionsReady = false;
  bool _isScanning = false;

  @override
  void initState() {
    super.initState();
    _loadLabels();
    _loadWifiCredentials();
    _btManager.isConnected.addListener(_onConnectionChanged);
    _initBluetoothOnce();
  }

  Future<void> _initBluetoothOnce() async {
    try {
      await _btManager.initPermissions();
      _permissionsReady = true;
      if (_btManager.isConnected.value) {
        _subscribeToData();
        return;
      }
      await _startScan();
    } catch (e) {
      debugPrint("Error init bluetooth: $e");
    }
  }

  void _onConnectionChanged() {
    if (!mounted) return;
    setState(() {});
    if (_btManager.isConnected.value) {
      _subscribeToData();
    } else {
      _dataSubscription?.cancel();
      _dataSubscription = null;
    }
  }

  void _subscribeToData() {
    _dataSubscription?.cancel();
    _dataSubscription = _btManager.deviceDataStream.listen(_processLine);
  }

  void _processLine(String line) {
    if (line.isEmpty || !mounted) return;
    debugPrint('BT Cfg ← $line');

    void snack(String msg, {Color color = Colors.green}) {
      ScaffoldMessenger.of(context).hideCurrentSnackBar();
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(msg), backgroundColor: color, duration: const Duration(seconds: 3)),
      );
    }

    if (line.contains('WiFi saved')) {
      snack('✅ WiFi guardado en el Metzabook');
    }
  }

  Future<void> _loadLabels() async {
    if (!mounted) return;
    final names = _btManager.channelNames.value;
    setState(() {
      _foco1Controller.text = names[0];
      _foco2Controller.text = names[1];
      _foco3Controller.text = names[2];
      _foco4Controller.text = names[3];
    });
  }

  Future<void> _loadWifiCredentials() async {
    final prefs = await SharedPreferences.getInstance();
    if (!mounted) return;
    setState(() {
      _ssidController.text = prefs.getString('wifi_ssid') ?? '';
      _passwordController.text = prefs.getString('wifi_password') ?? '';
    });
  }

  Future<void> _saveLabels() async {
    await _btManager.updateChannelName(0, _foco1Controller.text.trim());
    await _btManager.updateChannelName(1, _foco2Controller.text.trim());
    await _btManager.updateChannelName(2, _foco3Controller.text.trim());
    await _btManager.updateChannelName(3, _foco4Controller.text.trim());

    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Configuración guardada exitosamente')),
    );
  }

  Future<List<Device>> _startScan() async {
    if (!_permissionsReady) {
      await _initBluetoothOnce();
      return _devices;
    }
    if (_btManager.isConnected.value || _isScanning) return _devices;
    if (!mounted) return _devices;

    setState(() {
      _devices = [];
      _isScanning = true;
    });

    try {
      final pairedDevices = await _btManager.getPairedDevices();
      if (mounted) {
        setState(() {
          for (final d in pairedDevices) {
            if (!_devices.any((x) => x.address == d.address)) {
              _devices.add(d);
            }
          }
        });
      }
    } catch (e) {
      debugPrint("Error obteniendo vinculados: $e");
    }

    try {
      await _scanSubscription?.cancel();
      _scanSubscription = _btManager.onDeviceDiscovered().listen((device) {
        if (!mounted) return;
        final exists = _devices.any((d) => d.address == device.address);
        if (!exists) {
          setState(() => _devices.add(device));
        }
      });

      await _btManager.startScan();
      _scanTimeoutTimer?.cancel();
      _scanTimeoutTimer = Timer(const Duration(seconds: 15), () async {
        await _stopScan();
      });
      return _devices;
    } catch (e) {
      debugPrint("Error escaneando: $e");
      await _stopScan();
      return _devices;
    }
  }

  Future<void> _stopScan() async {
    try {
      await _btManager.stopScan();
    } catch (_) {}
    await _scanSubscription?.cancel();
    _scanSubscription = null;
    _scanTimeoutTimer?.cancel();
    _scanTimeoutTimer = null;
    if (mounted) setState(() => _isScanning = false);
  }

  Future<void> _connectToDevice(Device device) async {
    await _stopScan();
    if (!mounted) return;

    unawaited(
      showDialog(
        context: context,
        barrierDismissible: false,
        builder: (context) => const Dialog(
          child: Padding(
            padding: EdgeInsets.all(20.0),
            child: Row(
              children: [
                CircularProgressIndicator(),
                SizedBox(width: 20),
                Text("Conectando..."),
              ],
            ),
          ),
        ),
      ),
    );

    if (mounted) setState(() => isConnecting = true);

    try {
      await _btManager.connect(device.address, device.name ?? "Unknown");
      if (!mounted) return;
      if (Navigator.canPop(context)) Navigator.pop(context);
      setState(() => isConnecting = false);
    } catch (e) {
      if (!mounted) return;
      if (Navigator.canPop(context)) Navigator.pop(context);
      setState(() => isConnecting = false);
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text("Error al conectar: $e")));
    }
  }

  void _showWifiDialog() {
    bool obscurePassword = true;
    showDialog(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, setStateDialog) => AlertDialog(
          title: const Text('Configurar WiFi en Metzabok'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(
                controller: _ssidController,
                decoration: const InputDecoration(
                  labelText: 'Nombre de la Red (SSID)',
                  prefixIcon: Icon(Icons.wifi),
                ),
              ),
              const SizedBox(height: 15),
              TextField(
                controller: _passwordController,
                decoration: InputDecoration(
                  labelText: 'Contraseña',
                  prefixIcon: const Icon(Icons.lock),
                  suffixIcon: IconButton(
                    icon: Icon(obscurePassword ? Icons.visibility : Icons.visibility_off),
                    onPressed: () => setStateDialog(() => obscurePassword = !obscurePassword),
                  ),
                ),
                obscureText: obscurePassword,
              ),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('Cancelar'),
            ),
            ElevatedButton(
              onPressed: () {
                final ssid = _ssidController.text.trim();
                final pass = _passwordController.text.trim();
                if (ssid.isNotEmpty) {
                  _btManager.write('SET_WIFI:$ssid:$pass');
                  Navigator.pop(context);
                }
              },
              child: const Text('Enviar'),
            ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _stopScan();
    _btManager.isConnected.removeListener(_onConnectionChanged);
    _dataSubscription?.cancel();
    _dataSubscription = null;
    _foco1Controller.dispose();
    _foco2Controller.dispose();
    _foco3Controller.dispose();
    _foco4Controller.dispose();
    _ssidController.dispose();
    _passwordController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final bool connected = _btManager.isConnected.value;
    return Scaffold(
      backgroundColor: const Color(0xFFF8F9FA),
      appBar: AppBar(
        title: const Text("Configuración", style: TextStyle(fontWeight: FontWeight.bold)),
        centerTitle: true,
        backgroundColor: Colors.white,
        foregroundColor: const Color(0xFF333333),
        elevation: 0,
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            _buildSection("Personalización", Icons.palette_outlined, [
              _buildTextField(_foco1Controller, "Nombre Interruptor 1"),
              _buildTextField(_foco2Controller, "Nombre Interruptor 2"),
              _buildTextField(_foco3Controller, "Nombre Interruptor 3"),
              _buildTextField(_foco4Controller, "Nombre Interruptor 4"),
              const SizedBox(height: 10),
              ElevatedButton.icon(
                onPressed: _saveLabels,
                icon: const Icon(Icons.save),
                label: const Text("Guardar Nombres"),
                style: ElevatedButton.styleFrom(minimumSize: const Size(double.infinity, 50)),
              ),
            ]),
            const SizedBox(height: 20),
            _buildSection("Conexión Bluetooth", Icons.bluetooth, [
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: connected ? Colors.green[50] : Colors.red[50],
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(color: connected ? Colors.green : Colors.red),
                ),
                child: Center(
                  child: Text(
                    connected ? "Conectado" : "Desconectado",
                    style: TextStyle(
                      fontWeight: FontWeight.bold,
                      color: connected ? Colors.green[700] : Colors.red[700],
                    ),
                  ),
                ),
              ),
              const SizedBox(height: 15),
              if (!connected) ...[
                ElevatedButton.icon(
                  onPressed: _isScanning ? null : _startScan,
                  icon: _isScanning
                      ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2))
                      : const Icon(Icons.search),
                  label: Text(_isScanning ? "Buscando..." : "Buscar Metzabok"),
                  style: ElevatedButton.styleFrom(minimumSize: const Size(double.infinity, 50)),
                ),
                const SizedBox(height: 10),
                if (_devices.isNotEmpty)
                  ListView.builder(
                    shrinkWrap: true,
                    physics: const NeverScrollableScrollPhysics(),
                    itemCount: _devices.length,
                    itemBuilder: (context, i) => ListTile(
                      title: Text(_devices[i].name ?? "Desconocido"),
                      subtitle: Text(_devices[i].address),
                      onTap: () => _connectToDevice(_devices[i]),
                      trailing: const Icon(Icons.link),
                    ),
                  ),
              ] else
                ElevatedButton.icon(
                  onPressed: () => _btManager.disconnect(),
                  icon: const Icon(Icons.bluetooth_disabled),
                  label: const Text("Desconectar"),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.redAccent,
                    foregroundColor: Colors.white,
                    minimumSize: const Size(double.infinity, 50),
                  ),
                ),
            ]),
            const SizedBox(height: 20),
            _buildSection("Configuración WiFi", Icons.wifi, [
              const Text(
                "Configura la red WiFi a la que se conectará el Metzabook. Una vez conectado, podrás usar el control local.",
                style: TextStyle(fontSize: 13, color: Colors.grey),
              ),
              const SizedBox(height: 15),
              ElevatedButton.icon(
                onPressed: connected ? _showWifiDialog : null,
                icon: const Icon(Icons.send),
                label: const Text("Enviar WiFi al Metzabook"),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.orange,
                  foregroundColor: Colors.white,
                  minimumSize: const Size(double.infinity, 50),
                ),
              ),
            ]),
          ],
        ),
      ),
    );
  }

  Widget _buildSection(String title, IconData icon, List<Widget> children) {
    return Card(
      elevation: 2,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(15)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(icon, color: Colors.blue),
                const SizedBox(width: 8),
                Text(title, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
              ],
            ),
            const Divider(height: 24),
            ...children,
          ],
        ),
      ),
    );
  }

  Widget _buildTextField(TextEditingController controller, String label) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: TextField(
        controller: controller,
        decoration: InputDecoration(
          labelText: label,
          border: OutlineInputBorder(borderRadius: BorderRadius.circular(10)),
          contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        ),
      ),
    );
  }
}


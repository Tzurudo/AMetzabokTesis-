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
  bool isConnecting = false;
  bool _hasConnectedOnce = false;

  String foco1Label = 'Foco 1';
  String foco2Label = 'Foco 2';
  String foco3Label = 'Foco 3';
  String foco4Label = 'Foco 4';

  @override
  void initState() {
    super.initState();
    _initBluetooth();
    _loadLabels();

    // Escuchar cambios de conexión global
    _btManager.isConnected.addListener(_onConnectionChanged);
    _btManager.relayStates.addListener(_onStateChanged);
    _btManager.isGlobalAuto.addListener(_onStateChanged);
    _btManager.isTelegramMode.addListener(_onTelegramModeChanged);
  }

  void _onTelegramModeChanged() {
    if (_btManager.isTelegramMode.value && mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Row(
            children: [
              Icon(Icons.send, color: Colors.white),
              SizedBox(width: 10),
              Text('Control Remoto (Telegram) Activo'),
            ],
          ),
          backgroundColor: Colors.blue,
          duration: Duration(seconds: 3),
        ),
      );
      if (mounted) setState(() {});
    }
  }

  void _onConnectionChanged() {
    if (mounted) setState(() {});

    if (_btManager.isConnected.value) {
      _hasConnectedOnce = true;
      _subscribeToData();
      _requestStatus();
    } else {
      _dataSubscription?.cancel();
      if (_hasConnectedOnce && mounted) {
        if (Navigator.canPop(context)) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text("Desconectado. Regresando al menú..."),
              duration: Duration(seconds: 2),
            ),
          );
          Navigator.pop(context);
        }
      }
    }
  }

  void _onStateChanged() {
    if (mounted) setState(() {});
  }

  void _subscribeToData() {
    _dataSubscription?.cancel();
    _dataSubscription = _btManager.deviceDataStream.listen((_) {});
  }

  void _requestStatus() {
    _btManager.write("STATUS");
  }

  Future<void> _loadLabels() async {
    final prefs = await SharedPreferences.getInstance();
    setState(() {
      foco1Label = prefs.getString('foco1_label') ?? 'Foco 1';
      foco2Label = prefs.getString('foco2_label') ?? 'Foco 2';
      foco3Label = prefs.getString('foco3_label') ?? 'Foco 3';
      foco4Label = prefs.getString('foco4_label') ?? 'Foco 4';
    });
  }

  void _initBluetooth() async {
    await _btManager.initPermissions();
    await _startScan();
  }

  Future<List<Device>> _startScan() async {
    if (_btManager.isConnected.value) return _devices;
    setState(() => _devices = []);

    try {
      final List<Device> pairedDevices = await _btManager.getPairedDevices();
      if (mounted) setState(() => _devices = pairedDevices);
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
        if (mounted) await _btManager.stopScan();
      });
      return _devices;
    } catch (e) {
      debugPrint("Error escaneando: $e");
      return _devices;
    }
  }

  Future<void> _connectToDevice(Device device) async {
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

    setState(() => isConnecting = true);

    try {
      await _btManager.connect(device.address, device.name ?? "Unknown");
      if (mounted && Navigator.canPop(context)) Navigator.pop(context);
      setState(() => isConnecting = false);
    } catch (e) {
      debugPrint('Error de conexión: $e');
      if (mounted && Navigator.canPop(context)) Navigator.pop(context);
      setState(() => isConnecting = false);
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text("Error al conectar: $e")));
      }
    }
  }

  void _sendBTCommand(String command) {
    if (!_btManager.isConnected.value) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(const SnackBar(content: Text('No conectado')));
      return;
    }
    _btManager.write(command);
  }

  void _syncTime() {
    final DateTime now = DateTime.now();
    final String h = now.hour.toString().padLeft(2, '0');
    final String m = now.minute.toString().padLeft(2, '0');
    final String s = now.second.toString().padLeft(2, '0');
    final String d = now.day.toString().padLeft(2, '0');
    final String mo = now.month.toString().padLeft(2, '0');
    final String y = now.year.toString();
    _sendBTCommand("SETTIME:$h:$m:$s:$d:$mo:$y");
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Hora sincronizada con el dispositivo')),
    );
  }

  @override
  Widget build(BuildContext context) {
    return PopScope(
      canPop: true,
      onPopInvokedWithResult: (didPop, result) {
        if (_btManager.isConnected.value && !_btManager.isGlobalAuto.value) {
          debugPrint(
            "BluetoothPage -> Saliendo en MODO MANUAL: Enviando ALLOFF",
          );
          _btManager.write("ALLOFF");
        }
      },
      child: Scaffold(
        appBar: AppBar(
          title: const Text("Metzabok - Serial BT"),
          actions: [
            IconButton(
              icon: const Icon(Icons.settings),
              onPressed: () {
                Navigator.push(
                  context,
                  MaterialPageRoute(builder: (context) => const SettingsPage()),
                ).then((_) => _loadLabels());
              },
            ),
            IconButton(icon: const Icon(Icons.refresh), onPressed: _startScan),
          ],
        ),
        body: SingleChildScrollView(
          child: Column(
            children: [
              _buildStatusBanner(),
              if (!_btManager.isConnected.value)
                Container(
                  width: double.infinity,
                  padding: const EdgeInsets.all(12),
                  margin: const EdgeInsets.symmetric(
                    horizontal: 16,
                    vertical: 10,
                  ),
                  decoration: BoxDecoration(
                    color: Colors.orange[50],
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(color: Colors.orange[200]!),
                  ),
                  child: Row(
                    children: [
                      Icon(
                        Icons.bluetooth_searching,
                        color: Colors.orange[700],
                      ),
                      const SizedBox(width: 12),
                      const Expanded(
                        child: Text(
                          "Si no ves tu dispositivo, enciende el Bluetooth y vincula Metzabok.",
                          style: TextStyle(
                            color: Colors.orange,
                            fontWeight: FontWeight.w500,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              _btManager.isConnected.value
                  ? _buildControlPanel()
                  : _buildDeviceList(),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildStatusBanner() {
    final bool connected = _btManager.isConnected.value;
    return Container(
      color: connected ? Colors.green : Colors.red,
      padding: const EdgeInsets.all(8),
      child: Center(
        child: Text(
          connected ? "CONECTADO" : "DESCONECTADO",
          style: const TextStyle(
            color: Colors.white,
            fontWeight: FontWeight.bold,
          ),
        ),
      ),
    );
  }

  Widget _buildDeviceList() {
    return ListView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      itemCount: _devices.length,
      itemBuilder: (context, i) => ListTile(
        title: Text(_devices[i].name ?? "Desconocido"),
        subtitle: Text(_devices[i].address),
        onTap: () => _connectToDevice(_devices[i]),
        trailing: const Icon(Icons.link),
      ),
    );
  }

  Widget _buildControlPanel() {
    final states = _btManager.relayStates.value;
    final isAuto = _btManager.isGlobalAuto.value;
    final isTelegram = _btManager.isTelegramMode.value;
    final isConnected = _btManager.isConnected.value;

    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          if (!isConnected && isTelegram)
            Container(
              margin: const EdgeInsets.only(bottom: 10),
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: Colors.blue[50],
                borderRadius: BorderRadius.circular(10),
                border: Border.all(color: Colors.blue[200]!),
              ),
              child: Row(
                children: [
                  const Icon(Icons.send, color: Colors.blue),
                  const SizedBox(width: 8),
                  const Text('Control Remoto (Telegram)'),
                  const Spacer(),
                  TextButton(
                    onPressed: () => _btManager.disconnectRemote(),
                    child: const Text(
                      'Desactivar',
                      style: TextStyle(color: Colors.red),
                    ),
                  ),
                ],
              ),
            ),
          Card(
            margin: const EdgeInsets.only(bottom: 16, top: 8),
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(12),
            ),
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
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                          color: isAuto ? Colors.blue[800] : Colors.orange[900],
                        ),
                      ),
                      Text(
                        isAuto ? "AUTOMÁTICO" : "MANUAL",
                        style: TextStyle(
                          fontSize: 12,
                          fontWeight: FontWeight.bold,
                          color: isAuto ? Colors.blue : Colors.deepOrange,
                        ),
                      ),
                    ],
                  ),
                  Transform.scale(
                    scale: 1.2,
                    child: Switch(
                      value: isAuto,
                      activeThumbColor: Colors.blue,
                      inactiveThumbColor: Colors.orange,
                      trackColor: WidgetStateProperty.resolveWith((states) {
                        return states.contains(WidgetState.selected)
                            ? Colors.blue[200]
                            : Colors.orange[200];
                      }),
                      onChanged: (val) {
                        _btManager.isGlobalAuto.value = val;
                        _sendBTCommand(val ? "GLOBAL_AUTO" : "GLOBAL_MANUAL");
                      },
                    ),
                  ),
                ],
              ),
            ),
          ),
          _buildFocoItem(
            1,
            foco1Label,
            states[1] ?? false,
            isAuto,
            (v) => _sendBTCommand(v ? 'ON1' : 'OFF1'),
          ),
          _buildFocoItem(
            2,
            foco2Label,
            states[2] ?? false,
            isAuto,
            (v) => _sendBTCommand(v ? 'ON2' : 'OFF2'),
          ),
          _buildFocoItem(
            3,
            foco3Label,
            states[3] ?? false,
            isAuto,
            (v) => _sendBTCommand(v ? 'ON3' : 'OFF3'),
          ),
          _buildFocoItem(
            4,
            foco4Label,
            states[4] ?? false,
            isAuto,
            (v) => _sendBTCommand(v ? 'ON4' : 'OFF4'),
          ),
          const SizedBox(height: 12),
          Wrap(
            spacing: 12,
            runSpacing: 12,
            alignment: WrapAlignment.center,
            children: [
              ElevatedButton.icon(
                onPressed: () {
                  _sendBTCommand('ALLOFF');
                  _btManager.relayStates.value = {
                    1: false,
                    2: false,
                    3: false,
                    4: false,
                  };
                },
                icon: const Icon(Icons.flash_off),
                label: const Text('EMERGENCIA: ALL OFF'),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.red,
                  foregroundColor: Colors.white,
                ),
              ),
              ElevatedButton.icon(
                onPressed: _syncTime,
                icon: const Icon(Icons.access_time),
                label: const Text('Sincronizar Hora'),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.blueGrey,
                  foregroundColor: Colors.white,
                ),
              ),
            ],
          ),
          const SizedBox(height: 20),
          if (isConnected)
            ElevatedButton(
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.redAccent,
              ),
              onPressed: () => _btManager.disconnect(),
              child: const Text("Desconectar Bluetooth"),
            ),
        ],
      ),
    );
  }

  Widget _buildFocoItem(
    int ch,
    String label,
    bool state,
    bool isAuto,
    Function(bool) onSwitch,
  ) {
    return Card(
      elevation: 2,
      margin: const EdgeInsets.symmetric(horizontal: 4, vertical: 6),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(15)),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        child: FocoSwitch(
          titulo: label,
          estado: state,
          enabled: !isAuto,
          onChanged: isAuto
              ? (v) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(
                      content: Text(
                        "Modo AUTOMÁTICO activo: Cambia a MANUAL para control directo.",
                      ),
                      duration: Duration(seconds: 2),
                    ),
                  );
                }
              : (v) {
                  if (mounted) {
                    setState(() {
                      final currentStates = Map<int, bool>.from(
                        _btManager.relayStates.value,
                      );
                      currentStates[ch] = v;
                      _btManager.relayStates.value = currentStates;
                    });
                  }
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
    _btManager.relayStates.removeListener(_onStateChanged);
    _btManager.isGlobalAuto.removeListener(_onStateChanged);
    _btManager.isTelegramMode.removeListener(_onTelegramModeChanged);
    _btManager.stopScan();
    super.dispose();
  }
}

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../widgets/foco_switch.dart';
import '../services/bluetooth_manager.dart';
import '../services/wifi_manager.dart';

class WifiPage extends StatefulWidget {
  const WifiPage({super.key});

  @override
  State<WifiPage> createState() => _WifiPageState();
}

class _WifiPageState extends State<WifiPage> {
  final WifiManager _wifiManager = WifiManager();
  final BluetoothManager _btManager = BluetoothManager();

  bool _estaConectado = false;
  bool _cargando = false;
  Timer? _refreshTimer;

  List<String> _labels = [
    'Interruptor 1',
    'Interruptor 2',
    'Interruptor 3',
    'Interruptor 4',
  ];

  @override
  void initState() {
    super.initState();
    _initData();
    _btManager.channelNames.addListener(_onNamesChanged);
    _wifiManager.relayStates.addListener(_rebuild);
    _wifiManager.isAutoMode.addListener(_rebuild);

    // Refresh status every 5 seconds if available
    _refreshTimer = Timer.periodic(const Duration(seconds: 5), (timer) {
      if (mounted) _wifiManager.checkConnection();
    });
  }

  void _rebuild() {
    if (mounted) setState(() {});
  }

  void _onNamesChanged() {
    if (mounted) {
      setState(() => _labels = List.from(_btManager.channelNames.value));
    }
  }

  Future<void> _initData() async {
    await _loadLabels();
    await _checkConnection();
  }

  Future<void> _checkConnection() async {
    setState(() => _cargando = true);
    final ok = await _wifiManager.checkConnection();
    if (mounted) {
      setState(() {
        _estaConectado = ok;
        _cargando = false;
      });
    }
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

  Future<void> _setRelay(int i, bool v) async {
    setState(() => _cargando = true);
    final ok = await _wifiManager.setRelay(i, v);
    if (mounted) {
      setState(() => _cargando = false);
      if (!ok) {
        _showSnack("Error de conexión con metzabok.local", color: Colors.red);
      }
    }
  }

  Future<void> _setMode(bool v) async {
    setState(() => _cargando = true);
    final ok = await _wifiManager.setMode(v);
    if (mounted) {
      setState(() => _cargando = false);
      if (!ok) _showSnack("Error al cambiar modo", color: Colors.red);
    }
  }

  Future<void> _allOff() async {
    setState(() => _cargando = true);
    final ok = await _wifiManager.allOff();
    if (mounted) {
      setState(() => _cargando = false);
      if (ok) {
        _showSnack("Todos los focos apagados", color: Colors.green);
      } else {
        _showSnack("Error de conexión con metzabok.local", color: Colors.red);
      }
    }
  }

  void _showSnack(String msg, {Color color = Colors.green}) {
    if (!mounted) return;
    ScaffoldMessenger.of(context).hideCurrentSnackBar();
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(msg), backgroundColor: color));
  }

  @override
  Widget build(BuildContext context) {
    final bool isAuto = _wifiManager.isAutoMode.value;
    final List<bool> states = _wifiManager.relayStates.value;

    return Scaffold(
      appBar: AppBar(
        title: const Text(
          "Control WiFi Local",
          style: TextStyle(
            color: Color(0xFFD4AF37),
            fontWeight: FontWeight.bold,
          ),
        ),
        backgroundColor: Colors.white,
        elevation: 0,
        actions: [
          IconButton(
            icon: Icon(
              Icons.refresh,
              color: _estaConectado ? Colors.green : Colors.grey,
            ),
            onPressed: _cargando ? null : _checkConnection,
          ),
        ],
      ),
      body: RefreshIndicator(
        onRefresh: _wifiManager.checkConnection,
        child: SingleChildScrollView(
          physics: const AlwaysScrollableScrollPhysics(),
          padding: const EdgeInsets.all(16),
          child: Column(
            children: [
              _buildStatusBanner(),
              const SizedBox(height: 20),

              // Modo de operación
              Card(
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(12),
                ),
                color: isAuto ? Colors.blue[50] : Colors.orange[50],
                child: ListTile(
                  title: const Text(
                    "Modo de Operación",
                    style: TextStyle(fontWeight: FontWeight.bold),
                  ),
                  subtitle: Text(
                    isAuto ? "AUTOMÁTICO (Calendario)" : "MANUAL (Directo)",
                  ),
                  trailing: Switch(
                    value: isAuto,
                    onChanged: _cargando ? null : (val) => _setMode(val),
                  ),
                ),
              ),
              const SizedBox(height: 12),

              // Switches de canales
              for (int i = 0; i < 4; i++)
                FocoSwitch(
                  titulo: _labels[i],
                  estado: isAuto ? false : states[i],
                  enabled: !isAuto && !_cargando && _estaConectado,
                  loading: _cargando,
                  onChanged: (v) => _setRelay(i, v),
                ),

              const SizedBox(height: 16),
              ElevatedButton.icon(
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.redAccent,
                  foregroundColor: Colors.white,
                  minimumSize: const Size.fromHeight(48),
                ),
                icon: const Icon(Icons.power_settings_new),
                label: const Text("Apagar todos los focos"),
                onPressed: (_cargando || !_estaConectado) ? null : _allOff,
              ),

              const SizedBox(height: 20),

              if (!_estaConectado) _buildHelpCard(),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildStatusBanner() {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: _estaConectado ? Colors.green[50] : Colors.red[50],
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: _estaConectado ? Colors.green : Colors.red),
      ),
      child: Row(
        children: [
          Icon(
            _estaConectado ? Icons.check_circle : Icons.error_outline,
            color: _estaConectado ? Colors.green : Colors.red,
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              _estaConectado
                  ? "Conectado a metzabok.local. Control directo activo."
                  : "No se encuentra Metzabook en la red local. Verifica que tu móvil esté en la misma WiFi.",
              style: TextStyle(
                fontWeight: FontWeight.bold,
                color: _estaConectado ? Colors.green[700] : Colors.red[700],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildHelpCard() {
    return const Card(
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          children: [
            Icon(Icons.help_outline, size: 48, color: Colors.blue),
            SizedBox(height: 12),
            Text(
              "¿Cómo conectar?",
              style: TextStyle(fontWeight: FontWeight.bold),
            ),
            SizedBox(height: 8),
            Text(
              "1. Asegúrate de que el Metzabook esté en modo WiFi.\n"
              "2. Tu teléfono debe estar en la misma red WiFi.\n"
              "3. Si configuraste WiFi hace poco, espera 10 segundos.\n"
              "4. El dominio es: http://metzabok.local",
              textAlign: TextAlign.left,
            ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _refreshTimer?.cancel();
    _btManager.channelNames.removeListener(_onNamesChanged);
    _wifiManager.relayStates.removeListener(_rebuild);
    _wifiManager.isAutoMode.removeListener(_rebuild);
    super.dispose();
  }
}

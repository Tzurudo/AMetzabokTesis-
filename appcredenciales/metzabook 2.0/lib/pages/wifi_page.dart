import 'dart:async';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../widgets/foco_switch.dart';
import '../services/bluetooth_manager.dart';
import '../services/telegram_service.dart';
import 'settings_page.dart';

class WifiPage extends StatefulWidget {
  const WifiPage({super.key});

  @override
  State<WifiPage> createState() => _WifiPageState();
}

class _WifiPageState extends State<WifiPage> {
  final TelegramService _telegram = TelegramService();
  final BluetoothManager _btManager = BluetoothManager();

  bool _estaConfigurado = false;
  bool _enviando = false;

  List<String> _labels = ['Interruptor 1', 'Interruptor 2', 'Interruptor 3', 'Interruptor 4'];

  @override
  void initState() {
    super.initState();
    _loadInitialState();
    _btManager.isGlobalAuto.addListener(_rebuild);
    _btManager.channelNames.addListener(_onNamesChanged);
  }

  void _rebuild() { if (mounted) setState(() {}); }
  void _onNamesChanged() {
    if (mounted) setState(() => _labels = List.from(_btManager.channelNames.value));
  }

  Future<void> _loadInitialState() async {
    await _telegram.init();
    await _loadLabels();
    if (mounted) {
      setState(() => _estaConfigurado = _telegram.isConfigured);
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

  Future<void> _enviarComando(String cmd) async {
    if (!_estaConfigurado) {
      _showSnack("Telegram no está configurado en la App", color: Colors.red);
      return;
    }
    if (_enviando) return;
    setState(() => _enviando = true);

    final exito = await _telegram.sendCommand(cmd);

    if (mounted) {
      setState(() => _enviando = false);
      if (!exito) {
        _showSnack("❌ Error al enviar comando vía Telegram", color: Colors.red);
      } else {
        _showSnack("✅ Comando enviado: $cmd");
      }
    }
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

  @override
  Widget build(BuildContext context) {
    final bool isAuto = _btManager.isGlobalAuto.value;

    return Scaffold(
      appBar: AppBar(
        title: Text(
          _estaConfigurado ? "Metzabok — Telegram" : "Configuración Requerida",
          style: const TextStyle(color: Color(0xFFD4AF37), fontWeight: FontWeight.bold),
        ),
        backgroundColor: Colors.white,
        elevation: 0,
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 12),
            child: Icon(
              _estaConfigurado ? Icons.cloud_done : Icons.cloud_off,
              color: _estaConfigurado ? Colors.green : Colors.red,
            ),
          ),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            _buildStatusBanner(),
            const SizedBox(height: 20),

            if (!_estaConfigurado)
              _buildConfigWarning()
            else ...[
              // Modo de operación
              Card(
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                color: isAuto ? Colors.blue[50] : Colors.orange[50],
                child: ListTile(
                  title: const Text("Modo de Operación", style: TextStyle(fontWeight: FontWeight.bold)),
                  subtitle: Text(isAuto ? "AUTOMÁTICO" : "MANUAL"),
                  trailing: Switch(
                    value: isAuto,
                    onChanged: _enviando
                        ? null
                        : (val) {
                            _btManager.isGlobalAuto.value = val;
                            _enviarComando(val ? "auto" : "manual");
                          },
                  ),
                ),
              ),
              const SizedBox(height: 12),

              // Switches de canales
              for (int i = 0; i < 4; i++)
                FocoSwitch(
                  titulo: _labels[i],
                  estado: false, // Telegram es fire-and-forget, no tenemos estado de retorno
                  enabled: !isAuto && !_enviando,
                  loading: _enviando,
                  onChanged: isAuto
                      ? (_) => _showSnack("Modo AUTOMÁTICO activo.", color: Colors.orange)
                      : (v) => _enviarComando(v ? "on${i + 1}" : "off${i + 1}"),
                ),

              const SizedBox(height: 20),

              ElevatedButton.icon(
                icon: _enviando
                    ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white))
                    : const Icon(Icons.refresh),
                label: const Text("Solicitar Estado"),
                style: ElevatedButton.styleFrom(minimumSize: const Size(double.infinity, 50)),
                onPressed: _enviando ? null : () => _enviarComando("status"),
              ),

              const SizedBox(height: 12),

              ElevatedButton.icon(
                icon: const Icon(Icons.flash_off),
                label: const Text("EMERGENCIA: Apagar Todo"),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.red,
                  foregroundColor: Colors.white,
                  minimumSize: const Size(double.infinity, 50),
                ),
                onPressed: _enviando ? null : () => _enviarComando("alloff"),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildStatusBanner() {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: _estaConfigurado ? Colors.green[50] : Colors.red[50],
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: _estaConfigurado ? Colors.green : Colors.red),
      ),
      child: Row(
        children: [
          Icon(
            _estaConfigurado ? Icons.check_circle : Icons.error_outline,
            color: _estaConfigurado ? Colors.green : Colors.red,
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              _estaConfigurado
                  ? "Bot de Telegram listo. Los comandos se envían directamente al Metzabook."
                  : "Configura el Token y Chat ID en Ajustes para usar el control remoto.",
              style: TextStyle(
                fontWeight: FontWeight.bold,
                color: _estaConfigurado ? Colors.green[700] : Colors.red[700],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildConfigWarning() {
    return Card(
      elevation: 2,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          children: [
            const Icon(Icons.settings_suggest, size: 56, color: Colors.blue),
            const SizedBox(height: 16),
            const Text("Falta Configuración", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            const Text(
              "Para controlar tu Metzabook desde cualquier lugar, necesitas configurar tu Bot de Telegram.",
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 20),
            ElevatedButton.icon(
              icon: const Icon(Icons.settings),
              label: const Text("Ir a Configuración"),
              style: ElevatedButton.styleFrom(minimumSize: const Size(double.infinity, 50)),
              onPressed: () => Navigator.push(
                context,
                MaterialPageRoute(builder: (_) => const SettingsPage()),
              ).then((_) => _loadInitialState()),
            ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _btManager.isGlobalAuto.removeListener(_rebuild);
    _btManager.channelNames.removeListener(_onNamesChanged);
    super.dispose();
  }
}

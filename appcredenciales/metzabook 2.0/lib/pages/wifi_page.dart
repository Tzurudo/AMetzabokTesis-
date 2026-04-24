import 'dart:async';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../widgets/foco_switch.dart';
import '../services/bluetooth_manager.dart';
import '../services/telegram_service.dart';
import 'settings_page.dart';
import 'about_page.dart';

class WifiPage extends StatefulWidget {
  const WifiPage({super.key});

  @override
  State<WifiPage> createState() => _WifiPageState();
}

class _WifiPageState extends State<WifiPage> {
  final TelegramService _telegram = TelegramService();
  bool _estaConectado = false;
  bool _enviando = false;

  // Estados locales (estimados, ya que Telegram no es tiempo real bidireccional sin polling)
  bool foco1 = false;
  bool foco2 = false;
  bool foco3 = false;
  bool foco4 = false;

  String foco1Label = 'Interruptor 1';
  String foco2Label = 'Interruptor 2';
  String foco3Label = 'Interruptor 3';
  String foco4Label = 'Interruptor 4';

  @override
  void initState() {
    super.initState();
    _loadInitialState();
    BluetoothManager().isGlobalAuto.addListener(_onModeChanged);
  }

  void _onModeChanged() {
    if (mounted) setState(() {});
  }

  Future<void> _loadInitialState() async {
    await _telegram.init();
    await _loadLabels();
    setState(() {
      _estaConectado = _telegram.isConfigured;
    });
  }

  Future<void> _loadLabels() async {
    final prefs = await SharedPreferences.getInstance();
    if (mounted) {
      setState(() {
        foco1Label = prefs.getString('foco1_label') ?? 'Interruptor 1';
        foco2Label = prefs.getString('foco2_label') ?? 'Interruptor 2';
        foco3Label = prefs.getString('foco3_label') ?? 'Interruptor 3';
        foco4Label = prefs.getString('foco4_label') ?? 'Interruptor 4';
      });
    }
  }

  Future<void> _enviarComando(String cmd, bool nuevoEstado, int index) async {
    if (!_estaConectado) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Telegram no está configurado en la App")),
      );
      return;
    }

    setState(() => _enviando = true);
    
    final exito = await _telegram.sendCommand(cmd);
    
    if (mounted) {
      setState(() {
        _enviando = false;
        if (exito) {
          if (index == 1) foco1 = nuevoEstado;
          if (index == 2) foco2 = nuevoEstado;
          if (index == 3) foco3 = nuevoEstado;
          if (index == 4) foco4 = nuevoEstado;
        } else {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text("Error al enviar comando via Telegram")),
          );
        }
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(
          _estaConectado ? "Metzabok - Telegram" : "Configuración Requerida",
          style: const TextStyle(
            color: Color(0xFFD4AF37),
            fontWeight: FontWeight.bold,
          ),
        ),
        backgroundColor: Colors.white,
        elevation: 0,
        actions: [
          Icon(
            _estaConectado ? Icons.cloud_done : Icons.cloud_off,
            color: _estaConectado ? Colors.green : Colors.red,
          ),
          const SizedBox(width: 15),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            _buildStatusBanner(),
            const SizedBox(height: 20),
            if (!_estaConectado)
              _buildConfigWarning()
            else ...[
              _buildModoCard(),
              const SizedBox(height: 10),
              _buildFocoSwitch(1, foco1Label, foco1, (v) => _enviarComando(v ? "on1" : "off1", v, 1)),
              _buildFocoSwitch(2, foco2Label, foco2, (v) => _enviarComando(v ? "on2" : "off2", v, 2)),
              _buildFocoSwitch(3, foco3Label, foco3, (v) => _enviarComando(v ? "on3" : "off3", v, 3)),
              _buildFocoSwitch(4, foco4Label, foco4, (v) => _enviarComando(v ? "on4" : "off4", v, 4)),
              const SizedBox(height: 20),
              ElevatedButton.icon(
                onPressed: () => _enviarComando("status", false, 0),
                icon: const Icon(Icons.refresh),
                label: const Text("Solicitar Estado a Telegram"),
                style: ElevatedButton.styleFrom(
                  minimumSize: const Size(double.infinity, 50),
                ),
              ),
            ]
          ],
        ),
      ),
    );
  }

  Widget _buildStatusBanner() {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: _estaConectado ? Colors.green[50] : Colors.red[50],
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: _estaConectado ? Colors.green : Colors.red),
      ),
      child: Row(
        children: [
          Icon(_estaConectado ? Icons.check_circle : Icons.error, color: _estaConectado ? Colors.green : Colors.red),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              _estaConectado 
                ? "Listo para enviar comandos remotos." 
                : "Configura el Token y Chat ID en Ajustes para usar el control remoto.",
              style: TextStyle(fontWeight: FontWeight.bold, color: _estaConectado ? Colors.green[700] : Colors.red[700]),
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
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            const Icon(Icons.settings_suggest, size: 48, color: Colors.blue),
            const SizedBox(height: 16),
            const Text(
              "Falta Configuración",
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 8),
            const Text(
              "Para controlar tu Metzabook desde cualquier lugar, necesitas configurar tu Bot de Telegram.",
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 16),
            ElevatedButton(
              onPressed: () => Navigator.push(context, MaterialPageRoute(builder: (_) => const SettingsPage())).then((_) => _loadInitialState()),
              child: const Text("Ir a Configuración"),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildModoCard() {
    final bool isAuto = BluetoothManager().isGlobalAuto.value;
    return Card(
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      color: isAuto ? Colors.blue[50] : Colors.orange[50],
      child: ListTile(
        title: const Text("Modo de Operación", style: TextStyle(fontWeight: FontWeight.bold)),
        subtitle: Text(isAuto ? "AUTOMÁTICO" : "MANUAL"),
        trailing: Switch(
          value: isAuto,
          onChanged: (val) {
            BluetoothManager().isGlobalAuto.value = val;
            _enviarComando(val ? "auto" : "manual", val, 0);
          },
        ),
      ),
    );
  }

  Widget _buildFocoSwitch(int index, String label, bool state, Function(bool) onChanged) {
    return FocoSwitch(
      titulo: label,
      estado: state,
      enabled: !BluetoothManager().isGlobalAuto.value && !_enviando,
      onChanged: onChanged,
    );
  }

  @override
  void dispose() {
    BluetoothManager().isGlobalAuto.removeListener(_onModeChanged);
    super.dispose();
  }
}

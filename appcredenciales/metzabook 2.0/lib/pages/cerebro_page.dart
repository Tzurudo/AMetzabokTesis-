import 'package:flutter/material.dart';
import 'bluetooth_page.dart';
import 'wifi_page.dart';
import 'settings_page.dart';
import 'about_page.dart';
import 'automation_page.dart';
import '../services/bluetooth_manager.dart';
import '../services/wifi_manager.dart';

class CerebroPage extends StatefulWidget {
  const CerebroPage({super.key});

  @override
  State<CerebroPage> createState() => _CerebroPageState();
}

class _CerebroPageState extends State<CerebroPage> {
  bool _canPop = false;
  final BluetoothManager _btManager = BluetoothManager();
  final WifiManager _wifiManager = WifiManager();

  static const Color premiumGold = Color.fromARGB(255, 41, 122, 243);
  static const Color darkSilver = Color.fromARGB(255, 37, 37, 37);

  @override
  void initState() {
    super.initState();
    _btManager.isConnected.addListener(_updateState);
    _btManager.isGlobalAuto.addListener(_updateState);
    _wifiManager.isWifiAvailable.addListener(_updateState);
    _initConnectionCheck();
  }

  Future<void> _initConnectionCheck() async {
    await _btManager.initRemote();
    await _wifiManager.checkConnection();
    if (mounted) setState(() {});
  }

  @override
  void dispose() {
    _btManager.isConnected.removeListener(_updateState);
    _btManager.isGlobalAuto.removeListener(_updateState);
    _wifiManager.isWifiAvailable.removeListener(_updateState);
    super.dispose();
  }

  void _updateState() {
    if (mounted) setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return PopScope(
      canPop: _canPop,
      onPopInvokedWithResult: (didPop, result) async {
        if (didPop) return;

        final bool isAuto = _btManager.isGlobalAuto.value || _wifiManager.isAutoMode.value;

        final bool? shouldPop = await showDialog<bool>(
          context: context,
          builder: (context) {
            return AlertDialog(
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(15)),
              title: const Text("¿Salir del Menú?"),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      const Text("Modo actual: "),
                      Text(
                        isAuto ? "AUTOMÁTICO" : "MANUAL",
                        style: TextStyle(
                          fontWeight: FontWeight.bold,
                          color: isAuto ? Colors.blue : Colors.orange,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  Text(
                    "¿Estás seguro de que quieres salir dejando el sistema en modo ${isAuto ? 'AUTOMÁTICO' : 'MANUAL'}?",
                    style: const TextStyle(fontSize: 14),
                  ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.of(context).pop(false),
                  child: const Text("Cancelar"),
                ),
                TextButton(
                  onPressed: () => Navigator.of(context).pop(true),
                  child: const Text("Salir", style: TextStyle(color: Colors.red)),
                ),
              ],
            );
          },
        );

        if (shouldPop == true && context.mounted) {
          setState(() => _canPop = true);
          Navigator.of(context).pop();
        }
      },
      child: Scaffold(
        appBar: AppBar(
          title: const Text(
            "Metzabok",
            style: TextStyle(color: Color(0xFFD4AF37), fontWeight: FontWeight.bold),
          ),
          backgroundColor: Colors.white,
          elevation: 0,
          centerTitle: true,
          actions: [
            Builder(
              builder: (context) => IconButton(
                icon: const Icon(Icons.menu),
                onPressed: () => Scaffold.of(context).openDrawer(),
              ),
            ),
          ],
        ),
        drawer: Drawer(
          child: ListView(
            padding: EdgeInsets.zero,
            children: [
              const DrawerHeader(
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    colors: [Colors.white, Color(0xFFE0E0E0)],
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                  ),
                ),
                child: Text(
                  'Menú',
                  style: TextStyle(color: premiumGold, fontSize: 24, fontWeight: FontWeight.bold),
                ),
              ),
              ListTile(
                leading: const Icon(Icons.settings, color: darkSilver),
                title: const Text('Configuración', style: TextStyle(color: darkSilver)),
                onTap: () {
                  Navigator.pop(context);
                  Navigator.push(context, MaterialPageRoute(builder: (context) => const SettingsPage()));
                },
              ),
              ListTile(
                leading: const Icon(Icons.info, color: darkSilver),
                title: const Text('Acerca de', style: TextStyle(color: darkSilver)),
                onTap: () {
                  Navigator.pop(context);
                  Navigator.push(context, MaterialPageRoute(builder: (context) => const AboutPage()));
                },
              ),
            ],
          ),
        ),
        body: Container(
          decoration: const BoxDecoration(
            gradient: LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment.bottomCenter,
              colors: [Colors.white, Color(0xFFF5F5F5)],
            ),
          ),
          child: SingleChildScrollView(
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Container(
                    margin: const EdgeInsets.only(bottom: 20),
                    padding: const EdgeInsets.all(16),
                    decoration: BoxDecoration(
                      color: const Color(0xFFFFF8E1),
                      borderRadius: BorderRadius.circular(15),
                      border: Border.all(color: const Color(0xFFD4AF37), width: 1),
                      boxShadow: [
                        BoxShadow(
                          color: Colors.black.withValues(alpha: 0.05),
                          blurRadius: 5,
                          offset: const Offset(0, 2),
                        ),
                      ],
                    ),
                    child: const Row(
                      children: [
                        Icon(Icons.wifi_rounded, color: Color(0xFFD4AF37), size: 28),
                        SizedBox(width: 12),
                        Expanded(
                          child: Text(
                            "Conexión Local: Controla Metzabok vía WiFi usando metzabok.local cuando estés en casa.",
                            style: TextStyle(
                              color: Color.fromARGB(255, 60, 60, 60),
                              fontSize: 13,
                              height: 1.4,
                              fontWeight: FontWeight.w500,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                  if (_btManager.isConnected.value || _wifiManager.isWifiAvailable.value)
                    Container(
                      margin: const EdgeInsets.only(bottom: 20),
                      padding: const EdgeInsets.all(12),
                      decoration: BoxDecoration(
                        color: _getPanelColor(),
                        borderRadius: BorderRadius.circular(15),
                        border: Border.all(color: _getBorderColor(), width: 1),
                      ),
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Icon(_getStatusIcon(), color: _getStatusColor(), size: 24),
                          const SizedBox(width: 12),
                          Text(
                            _getStatusText(),
                            style: TextStyle(
                              fontWeight: FontWeight.bold,
                              fontSize: 16,
                              color: _getTextColor(),
                            ),
                          ),
                        ],
                      ),
                    ),
                  _buildPremiumButton(
                    context,
                    "Control por WiFi (Local)",
                    Icons.wifi_rounded,
                    const WifiPage(),
                    premiumGold,
                  ),
                  const SizedBox(height: 12),
                  _buildPremiumButton(
                    context,
                    "Vincular Bluetooth",
                    Icons.bluetooth,
                    const BluetoothPage(),
                    premiumGold,
                  ),
                  const SizedBox(height: 12),
                  _buildPremiumButton(
                    context,
                    "Programar Horarios",
                    Icons.auto_mode,
                    const AutomationPage(),
                    const Color(0xFFD4AF37),
                  ),
                  const SizedBox(height: 12),
                  IconButton(
                    onPressed: _initConnectionCheck,
                    icon: const Icon(Icons.refresh),
                    tooltip: "Refrescar conexión",
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }

  Color _getPanelColor() => (_btManager.isConnected.value || _wifiManager.isWifiAvailable.value)
      ? const Color(0xFFE8F5E9)
      : const Color(0xFFFFEBEE);

  Color _getBorderColor() => (_btManager.isConnected.value || _wifiManager.isWifiAvailable.value)
      ? Colors.green
      : Colors.red;

  IconData _getStatusIcon() {
    if (_btManager.isConnected.value) return Icons.bluetooth_connected;
    if (_wifiManager.isWifiAvailable.value) return Icons.wifi_rounded;
    return Icons.cloud_off;
  }

  Color _getStatusColor() => (_btManager.isConnected.value || _wifiManager.isWifiAvailable.value)
      ? Colors.green
      : Colors.red;

  String _getStatusText() {
    if (_btManager.isConnected.value) return "Conectado (Bluetooth)";
    if (_wifiManager.isWifiAvailable.value) return "Conectado (WiFi Local)";
    return "Sin conexión";
  }

  Color _getTextColor() => (_btManager.isConnected.value || _wifiManager.isWifiAvailable.value)
      ? Colors.black87
      : Colors.red;

  Widget _buildPremiumButton(BuildContext context, String texto, IconData icono, Widget destino, Color color) {
    return Container(
      width: double.infinity,
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(15),
        boxShadow: [
          BoxShadow(
            color: color.withValues(alpha: 0.2),
            blurRadius: 10,
            offset: const Offset(0, 4),
          ),
        ],
        border: Border.all(color: color.withValues(alpha: 0.5), width: 1),
      ),
      child: Material(
        color: Colors.transparent,
        child: InkWell(
          borderRadius: BorderRadius.circular(15),
          onTap: () => Navigator.push(context, MaterialPageRoute(builder: (_) => destino)),
          child: Padding(
            padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 16),
            child: Row(
              children: [
                Icon(icono, color: color, size: 24),
                const SizedBox(width: 12),
                Expanded(
                  child: Text(
                    texto,
                    style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600, color: Colors.black87),
                  ),
                ),
                const Icon(Icons.arrow_forward_ios, color: Colors.grey, size: 14),
              ],
            ),
          ),
        ),
      ),
    );
  }
}


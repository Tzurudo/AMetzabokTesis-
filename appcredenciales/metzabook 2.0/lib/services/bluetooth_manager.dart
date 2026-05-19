import 'dart:async';
import 'dart:collection';
import 'dart:convert';
import 'package:bluetooth_classic/bluetooth_classic.dart';
import 'package:bluetooth_classic/models/device.dart';
import 'package:flutter/foundation.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';

class BluetoothManager {
  static final BluetoothManager _instance = BluetoothManager._internal();
  factory BluetoothManager() => _instance;

  BluetoothManager._internal() {
    _bluetooth = BluetoothClassic();
  }

  late BluetoothClassic _bluetooth;

  // ── Observables ──────────────────────────────────────────────────────────
  final ValueNotifier<bool> isConnected = ValueNotifier<bool>(false);
  final ValueNotifier<Map<int, bool>> relayStates =
      ValueNotifier<Map<int, bool>>({1: false, 2: false, 3: false, 4: false});
  final ValueNotifier<bool> isGlobalAuto = ValueNotifier<bool>(false);
  final ValueNotifier<List<String>> channelNames =
      ValueNotifier<List<String>>([
    'Interruptor 1',
    'Interruptor 2',
    'Interruptor 3',
    'Interruptor 4',
  ]);

  // ── Stream de datos entrantes ────────────────────────────────────────────
  final StreamController<String> _dataStreamController =
      StreamController<String>.broadcast();
  Stream<String> get deviceDataStream => _dataStreamController.stream;

  // ── Bluetooth internals ──────────────────────────────────────────────────
  StreamSubscription<Uint8List>? _subscription;
  String _rxBuffer = '';

  final Queue<String> _commandQueue = Queue<String>();
  bool _isProcessingQueue = false;

  BluetoothClassic get instance => _bluetooth;

  // ── Inicialización ───────────────────────────────────────────────────────
  Future<void> initPermissions() async {
    await [
      Permission.bluetooth,
      Permission.bluetoothConnect,
      Permission.bluetoothScan,
      Permission.location,
    ].request();
    await _bluetooth.initPermissions();
    await _loadChannelNames();
  }

  Future<void> initRemote() async {
    // legacy method, no longer needed but kept for CerebroPage compatibility
  }

  // ── Nombres de canales ───────────────────────────────────────────────────
  Future<void> _loadChannelNames() async {
    final prefs = await SharedPreferences.getInstance();
    channelNames.value = [
      prefs.getString('foco1_label') ?? 'Interruptor 1',
      prefs.getString('foco2_label') ?? 'Interruptor 2',
      prefs.getString('foco3_label') ?? 'Interruptor 3',
      prefs.getString('foco4_label') ?? 'Interruptor 4',
    ];
  }

  Future<void> updateChannelName(int index, String newName) async {
    if (index < 0 || index >= 4) return;
    final List<String> current = List.from(channelNames.value);
    current[index] = newName;
    channelNames.value = current;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('foco${index + 1}_label', newName);
  }

  // ── Scan ─────────────────────────────────────────────────────────────────
  Future<List<Device>> getPairedDevices() => _bluetooth.getPairedDevices();
  Stream<Device> onDeviceDiscovered() => _bluetooth.onDeviceDiscovered();
  Future<void> startScan() => _bluetooth.startScan();
  Future<void> stopScan() async {
    try {
      await _bluetooth.stopScan();
    } catch (_) {}
  }

  // ── Conexión Bluetooth ───────────────────────────────────────────────────
  Future<void> connect(String address, String name) async {
    try {
      await stopScan();
      await Future.delayed(const Duration(milliseconds: 600));

      await _bluetooth.connect(address, "00001101-0000-1000-8000-00805f9b34fb");
      isConnected.value = true;
      debugPrint("BluetoothManager: Conectado a $name");

      _rxBuffer = '';
      await _subscription?.cancel();
      _subscription = _bluetooth.onDeviceDataReceived().listen(
        _onRawData,
        onDone: () => disconnect(),
        onError: (e) {
          debugPrint("BluetoothManager: Error de datos - $e");
          disconnect();
        },
      );

      await Future.delayed(const Duration(milliseconds: 300));
      _enqueue("STATUS");
    } catch (e) {
      debugPrint("BluetoothManager: Error al conectar - $e");
      isConnected.value = false;
      rethrow;
    }
  }

  void _onRawData(Uint8List data) {
    final String chunk = utf8.decode(data, allowMalformed: true);
    _rxBuffer += chunk;

    if (_rxBuffer.length > 8192) {
      _rxBuffer = "";
      debugPrint("BluetoothManager: RX buffer overflow");
    }

    final List<String> parts = _rxBuffer.split('\n');
    if (parts.length > 1) {
      for (int i = 0; i < parts.length - 1; i++) {
        final String line = parts[i].trim();
        if (line.isNotEmpty) {
          _processIncomingLine(line);
          _dataStreamController.add(line);
        }
      }
      _rxBuffer = parts.last;
    }
  }

  void _processIncomingLine(String line) {
    debugPrint("BT ← $line");

    // Handle STATUS response: S:1111:A (Relays: 1=ON, 0=OFF, Mode: A=Auto, M=Manual)
    if (line.startsWith('S:') && line.contains(':')) {
      final parts = line.split(':');
      if (parts.length >= 3) {
        final relayBits = parts[1];
        final mode = parts[2];

        // Update relays
        final updated = Map<int, bool>.from(relayStates.value);
        for (int i = 0; i < relayBits.length && i < 4; i++) {
          updated[i + 1] = relayBits[i] == '1';
        }
        relayStates.value = updated;

        // Update mode
        final isAuto = mode == 'A';
        if (isGlobalAuto.value != isAuto) isGlobalAuto.value = isAuto;
      }
    } 
    // Handle specific relay updates: CH1=ON
    else if (line.startsWith('CH') && line.contains('=')) {
      final parts = line.split('=');
      final chMatch = RegExp(r'CH(\d+)').firstMatch(parts[0]);
      if (chMatch != null) {
        final ch = int.tryParse(chMatch.group(1)!);
        if (ch != null && ch >= 1 && ch <= 4) {
          final state = parts[1].trim().toUpperCase() == 'ON';
          final updated = Map<int, bool>.from(relayStates.value);
          updated[ch] = state;
          relayStates.value = updated;
        }
      }
    } 
    // Handle mode changes: MODE:GLOBAL:AUTO or ACK:A
    else if (line.startsWith('MODE:GLOBAL:') || line.startsWith('ACK:')) {
      final isAuto = line.contains('AUTO') || line.contains(':A');
      if (isGlobalAuto.value != isAuto) isGlobalAuto.value = isAuto;
    }
  }

  Future<void> disconnect() async {
    try {
      await _bluetooth.disconnect();
    } catch (e) {
      debugPrint("BluetoothManager: Error al desconectar - $e");
    } finally {
      isConnected.value = false;
      unawaited(_subscription?.cancel());
      _subscription = null;
      _commandQueue.clear();
      _isProcessingQueue = false;
      _rxBuffer = '';
    }
  }

  // ── Escritura Bluetooth ──────────────────────────────────────────────────
  void write(String command) {
    if (!isConnected.value) return;
    _enqueue(command);
  }

  void _enqueue(String command) {
    _commandQueue.add(command);
    if (!_isProcessingQueue) _processQueue();
  }

  Future<void> _processQueue() async {
    if (_isProcessingQueue) return;
    _isProcessingQueue = true;
    while (_commandQueue.isNotEmpty && isConnected.value) {
      final command = _commandQueue.removeFirst();
      try {
        await _bluetooth.write("$command\n");
        await Future.delayed(const Duration(milliseconds: 100));
      } catch (e) {
        debugPrint("BluetoothManager: Error escribiendo '$command' - $e");
        break;
      }
    }
    _isProcessingQueue = false;
  }

  // ── Disposal ─────────────────────────────────────────────────────────────
  void dispose() {
    _dataStreamController.close();
  }
}


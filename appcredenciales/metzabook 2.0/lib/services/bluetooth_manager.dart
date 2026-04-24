import 'dart:async';
import 'dart:collection';
import 'dart:convert';
import 'package:bluetooth_classic/bluetooth_classic.dart';
import 'package:bluetooth_classic/models/device.dart';
import 'package:flutter/foundation.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'telegram_service.dart';

class BluetoothManager {
  static final BluetoothManager _instance = BluetoothManager._internal();

  factory BluetoothManager() {
    return _instance;
  }

  BluetoothManager._internal() {
    _bluetooth = BluetoothClassic();
  }

  late BluetoothClassic _bluetooth;

  // Estado de conexión observable
  final ValueNotifier<bool> isConnected = ValueNotifier<bool>(false);

  // Estados de los canales (centralizado)
  final ValueNotifier<Map<int, bool>> relayStates =
      ValueNotifier<Map<int, bool>>({1: false, 2: false, 3: false, 4: false});

  // Modo Global (AUTO/MANUAL)
  final ValueNotifier<bool> isGlobalAuto = ValueNotifier<bool>(false);

  // Nombres de los interruptores (Centralizado para sincronización fluida)
  final ValueNotifier<List<String>> channelNames = ValueNotifier<List<String>>([
    'Interruptor 1',
    'Interruptor 2',
    'Interruptor 3',
    'Interruptor 4',
  ]);

  // Stream de datos (broadcast para múltiples oyentes)
  final StreamController<String> _dataStreamController =
      StreamController<String>.broadcast();
  Stream<String> get deviceDataStream => _dataStreamController.stream;

  StreamSubscription<Uint8List>? _subscription;
  final StreamSubscription<String>? _dataSubscription = null; 
  String _rxBuffer = '';

  // Cola de comandos para evitar saturar el buffer
  final Queue<String> _commandQueue = Queue<String>();
  bool _isProcessingQueue = false;

  BluetoothClassic get instance => _bluetooth;

  // --- Telegram Mode ---
  final ValueNotifier<bool> _isTelegramMode = ValueNotifier<bool>(false);
  ValueNotifier<bool> get isTelegramMode => _isTelegramMode;

  final TelegramService _telegram = TelegramService();

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

  Future<void> _loadChannelNames() async {
    final prefs = await SharedPreferences.getInstance();
    final List<String> loadedNames = [];
    loadedNames.add(prefs.getString('foco1_label') ?? 'Interruptor 1');
    loadedNames.add(prefs.getString('foco2_label') ?? 'Interruptor 2');
    loadedNames.add(prefs.getString('foco3_label') ?? 'Interruptor 3');
    loadedNames.add(prefs.getString('foco4_label') ?? 'Interruptor 4');
    channelNames.value = loadedNames;
  }

  Future<void> updateChannelName(int index, String newName) async {
    if (index < 0 || index >= 4) return;
    final List<String> current = List.from(channelNames.value);
    current[index] = newName;
    channelNames.value = current;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('foco${index + 1}_label', newName);
  }

  Future<List<Device>> getPairedDevices() => _bluetooth.getPairedDevices();

  Stream<Device> onDeviceDiscovered() => _bluetooth.onDeviceDiscovered();

  Future<void> startScan() => _bluetooth.startScan();

  Future<void> stopScan() => _bluetooth.stopScan();

  Future<void> connect(String address, String name) async {
    try {
      await stopScan();
      await _bluetooth.connect(address, "00001101-0000-1000-8000-00805f9b34fb");

      isConnected.value = true;
      debugPrint("BluetoothManager: Conectado a $name");

      _rxBuffer = '';
      await _subscription?.cancel();
      _subscription = _bluetooth.onDeviceDataReceived().listen(
        (Uint8List data) {
          final String chunk = utf8.decode(data, allowMalformed: true);
          _rxBuffer += chunk;
          if (_rxBuffer.length > 4096) {
            _rxBuffer = "";
            debugPrint("BluetoothManager: Buffer overflow protection triggered");
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
        },
        onDone: () => disconnect(),
        onError: (e) => disconnect(),
      );
      write("STATUS");
    } catch (e) {
      debugPrint("BluetoothManager: Error al conectar - $e");
      isConnected.value = false;
      rethrow;
    }
  }

  void _processIncomingLine(String line) {
    if (line.startsWith('CH') && line.contains('=')) {
      final parts = line.split('=');
      final chMatch = RegExp(r'CH(\d+)').firstMatch(parts[0]);
      if (chMatch != null) {
        final ch = int.tryParse(chMatch.group(1)!);
        if (ch != null && ch >= 1 && ch <= 4) {
          final state = parts[1].trim().toUpperCase() == 'ON';
          if (relayStates.value[ch] != state) {
            final currentStates = Map<int, bool>.from(relayStates.value);
            currentStates[ch] = state;
            relayStates.value = currentStates;
          }
        }
      }
    } else if (line.startsWith('MODE:GLOBAL:')) {
      final isAuto = line.contains('AUTO');
      if (isGlobalAuto.value != isAuto) {
        isGlobalAuto.value = isAuto;
      }
    } else if (line.startsWith('WIFI:')) {
      // Ignorar por ahora, o usar para un indicador de WiFi
    } else if (line.startsWith('TG:')) {
      final ok = line.contains('OK');
      // Si el ESP32 dice que Telegram está OK, nos aseguramos de que la App lo sepa
      if (ok && !_isTelegramMode.value) {
        _isTelegramMode.value = true;
      }
    }
  }

  Future<void> disconnect() async {
    try {
      await _bluetooth.disconnect();
    } catch (e) {
      debugPrint("BluetoothManager: Error desconectando - $e");
    } finally {
      isConnected.value = false;
      unawaited(_subscription?.cancel());
      _commandQueue.clear();
      _isProcessingQueue = false;
    }
  }

  void write(String command) {
    if (!isConnected.value) return;
    if (_commandQueue.contains(command)) return;
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
        await Future.delayed(const Duration(milliseconds: 80));
      } catch (e) {
        debugPrint("BluetoothManager: Error escribiendo - $e");
        break;
      }
    }
    _isProcessingQueue = false;
  }

  void dispose() {
    unawaited(_dataSubscription?.cancel());
    _dataStreamController.close();
  }

  // --- Remote (Telegram) Methods ---

  Future<void> initRemote() async {
    await _telegram.init();
    _isTelegramMode.value = _telegram.isConfigured;
  }

  void disconnectRemote() {
    _isTelegramMode.value = false;
  }

  void sendCommand(String command) {
    if (isConnected.value) {
      write(command);
    } else if (_isTelegramMode.value) {
      // Mapear comandos internos a comandos de Telegram
      String tgCmd = command.toLowerCase();
      if (command == "GLOBAL_AUTO") tgCmd = "auto";
      if (command == "GLOBAL_MANUAL") tgCmd = "manual";
      _telegram.sendCommand(tgCmd);
    }
  }
}

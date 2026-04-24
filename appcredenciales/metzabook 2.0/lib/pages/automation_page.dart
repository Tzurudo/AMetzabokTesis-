import 'dart:async';
import 'package:flutter/material.dart';
import '../services/bluetooth_manager.dart';
import 'package:shared_preferences/shared_preferences.dart';

class AutomationPage extends StatefulWidget {
  const AutomationPage({super.key});

  @override
  State<AutomationPage> createState() => _AutomationPageState();
}

class SchedInterval {
  final int index;
  final String timeRange;
  final int mask;

  SchedInterval({
    required this.index,
    required this.timeRange,
    required this.mask,
  });

  Map<String, dynamic> toJson() => {
    'idx': index,
    'range': timeRange,
    'mask': mask,
  };
  factory SchedInterval.fromJson(Map<String, dynamic> json) => SchedInterval(
    index: json['idx'] as int? ?? 0,
    timeRange: json['range'] as String? ?? "00:00 - 00:00",
    mask: json['mask'] as int? ?? 0,
  );
}

class _AutomationPageState extends State<AutomationPage> {
  final BluetoothManager _btManager = BluetoothManager();

  List<String> _labels = ['Foco 1', 'Foco 2', 'Foco 3', 'Foco 4'];
  final List<List<SchedInterval>> _channelSchedules = [[], [], [], []];
  List<List<SchedInterval>> _tempSyncSchedules = [[], [], [], []];
  StreamSubscription? _btDataSub;

  final List<String> _dayNames = [
    "Domingo",
    "Lunes",
    "Martes",
    "Miércoles",
    "Jueves",
    "Viernes",
    "Sábado",
  ];

  void _onStateChanged() {
    if (mounted) setState(() {});
  }

  @override
  void initState() {
    super.initState();
    _loadLabels();
    _btDataSub = _btManager.deviceDataStream.listen(_handleIncomingData);
    _btManager.isGlobalAuto.addListener(_onStateChanged);
    _btManager.isConnected.addListener(_onConnectionChanged);
    _btManager.channelNames.addListener(_onNamesChanged);

    if (_btManager.isConnected.value) {
      _syncFromDevice();
    }
  }

  void _onConnectionChanged() {
    if (!mounted) return;
    setState(() {});
    if (_btManager.isConnected.value) {
      // Recién conectado: sincronizar horarios
      _syncFromDevice();
    }
  }

  void _syncFromDevice() {
    _tempSyncSchedules = [[], [], [], []];
    _btManager.write("GETSCHEDS");
  }

  void _handleIncomingData(String line) {
    if (line.startsWith("LSCHED:")) {
      // FORMATO: LSCHED:CH:IDX:MASK:ON_H:ON_M:OFF_H:OFF_M
      final parts = line.split(':');
      if (parts.length == 8) {
        final int ch = int.tryParse(parts[1]) ?? 0;
        final int idx = int.tryParse(parts[2]) ?? 0;
        final int mask = int.tryParse(parts[3]) ?? 0;
        final int onH = int.tryParse(parts[4]) ?? 0;
        final int onM = int.tryParse(parts[5]) ?? 0;
        final int offH = int.tryParse(parts[6]) ?? 0;
        final int offM = int.tryParse(parts[7]) ?? 0;

        if (ch >= 1 && ch <= 4) {
          final onTime = TimeOfDay(hour: onH, minute: onM);
          final offTime = TimeOfDay(hour: offH, minute: offM);

          _tempSyncSchedules[ch - 1].add(
            SchedInterval(
              index: idx,
              timeRange:
                  "${onTime.hour.toString().padLeft(2, '0')}:${onTime.minute.toString().padLeft(2, '0')} - ${offTime.hour.toString().padLeft(2, '0')}:${offTime.minute.toString().padLeft(2, '0')}",
              mask: mask,
            ),
          );
        }
      }
    } else if (line == "SYNC_DONE") {
      setState(() {
        for (int i = 0; i < 4; i++) {
          _channelSchedules[i] = List.from(_tempSyncSchedules[i]);
          _saveLocalScheds(i + 1);
        }
      });
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text("Sincronización completa")),
        );
      }
    }
  }

  Future<void> _loadLabels() async {
    final prefs = await SharedPreferences.getInstance();
    if (!mounted) return;
    setState(() {
      _labels = List.from(_btManager.channelNames.value);

      // Load saved schedules from local storage
      for (int i = 0; i < 4; i++) {
        final List<String>? saved = prefs.getStringList('ch${i + 1}_scheds');
        if (saved != null) {
          _channelSchedules[i] = saved
              .map(
                (s) => SchedInterval.fromJson(
                  Map<String, dynamic>.from(
                    Uri.splitQueryString(
                      s,
                    ).map((k, v) => MapEntry(k, int.tryParse(v) ?? v)),
                  ),
                ),
              )
              .toList();
        }
      }
    });
  }

  Future<void> _saveLocalScheds(int ch) async {
    final prefs = await SharedPreferences.getInstance();
    final List<String> data = _channelSchedules[ch - 1]
        .map((s) => 'idx=${s.index}&range=${s.timeRange}&mask=${s.mask}')
        .toList();
    await prefs.setStringList('ch${ch}_scheds', data);
  }

  void _sendBTCommand(String command) {
    if (!_btManager.isConnected.value) {
      if (mounted) {
        ScaffoldMessenger.of(context).hideCurrentSnackBar();
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Conecta por Bluetooth para gestionar horarios'),
            backgroundColor: Colors.orange,
          ),
        );
      }
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
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(const SnackBar(content: Text('Hora sincronizada')));
  }

  Future<void> _addSchedule(int channel, String label) async {
    if (_channelSchedules[channel - 1].length >= 5) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Máximo 5 horarios por foco')),
      );
      return;
    }

    final TimeOfDay? onTime = await showTimePicker(
      context: context,
      initialTime: const TimeOfDay(hour: 8, minute: 0),
      helpText: "Encendido para $label",
    );
    if (!mounted || onTime == null) return;

    final TimeOfDay? offTime = await showTimePicker(
      context: context,
      initialTime: const TimeOfDay(hour: 18, minute: 0),
      helpText: "Apagado para $label",
    );
    if (!mounted || offTime == null) return;

    // Selector de Días
    final int? mask = await showDialog<int>(
      context: context,
      builder: (context) {
        final List<int> tempSelected = [1, 2, 3, 4, 5];
        return StatefulBuilder(
          builder: (context, setDialogState) {
            return AlertDialog(
              title: const Text("Seleccionar Días"),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                children: List.generate(7, (index) {
                  return CheckboxListTile(
                    title: Text(_dayNames[index]),
                    value: tempSelected.contains(index),
                    onChanged: (v) {
                      setDialogState(() {
                        if (v == true) {
                          tempSelected.add(index);
                        } else {
                          tempSelected.remove(index);
                        }
                      });
                    },
                  );
                }),
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(context, null),
                  child: const Text("Cancelar"),
                ),
                ElevatedButton(
                  onPressed: () {
                    int m = 0;
                    for (final int d in tempSelected) {
                      m |= (1 << d);
                    }
                    Navigator.pop(context, m);
                  },
                  child: const Text("Agregar"),
                ),
              ],
            );
          },
        );
      },
    );

    if (mask == null || mask == 0 || !mounted) return;

    // Find first available local index slot
    int newIdx = 0;
    final List<int> usedIdx = _channelSchedules[channel - 1]
        .map((e) => e.index)
        .toList();
    while (usedIdx.contains(newIdx)) {
      newIdx++;
    }

    if (newIdx >= 5) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Límite de 5 horarios alcanzado")),
      );
      return;
    }

    // Protocol: SETSCHED:CH:IDX:MASK:ON_H:ON_M:OFF_H:OFF_M
    final String cmd =
        "SETSCHED:$channel:$newIdx:$mask:${onTime.hour}:${onTime.minute}:${offTime.hour}:${offTime.minute}";
    _sendBTCommand(cmd);

    setState(() {
      _channelSchedules[channel - 1].add(
        SchedInterval(
          index: newIdx,
          timeRange: "${onTime.format(context)} - ${offTime.format(context)}",
          mask: mask,
        ),
      );
    });
    unawaited(_saveLocalScheds(channel));
  }

  String _getMaskString(int mask) {
    if (mask == 127) return "Todos los días";
    final List<String> activeDays = [];
    for (int i = 0; i < 7; i++) {
      if ((mask & (1 << i)) != 0) {
        activeDays.add(_dayNames[i].substring(0, 3));
      }
    }
    return activeDays.join(", ");
  }

  void _removeSchedule(int channel, int listIdx) {
    final int btIdx = _channelSchedules[channel - 1][listIdx].index;
    _sendBTCommand("DIS_SCHED:$channel:$btIdx");
    setState(() {
      _channelSchedules[channel - 1].removeAt(listIdx);
    });
    unawaited(_saveLocalScheds(channel));
  }

  void _clearAllSchedules(int channel) {
    _sendBTCommand("CLEAR_SCHEDS:$channel");
    setState(() {
      _channelSchedules[channel - 1].clear();
    });
    unawaited(_saveLocalScheds(channel));
  }

  void _onNamesChanged() {
    if (mounted) {
      setState(() {
        _labels = List.from(_btManager.channelNames.value);
      });
    }
  }

  @override
  void dispose() {
    unawaited(_btDataSub?.cancel());
    _btManager.isGlobalAuto.removeListener(_onStateChanged);
    _btManager.isConnected.removeListener(_onConnectionChanged);
    _btManager.channelNames.removeListener(_onNamesChanged);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final bool connected = _btManager.isConnected.value;
    final bool isAuto = _btManager.isGlobalAuto.value;

    return Scaffold(
      appBar: AppBar(
        title: const Text("Modo Automático"),
        actions: [
          if (connected)
            IconButton(
              icon: const Icon(Icons.sync),
              onPressed: _syncTime,
              tooltip: "Sincronizar Hora",
            ),
        ],
      ),
      body: Column(
        children: [
          Container(
            width: double.infinity,
            color: connected ? Colors.green[50] : Colors.red[50],
            padding: const EdgeInsets.all(15),
            child: Column(
              children: [
                Text(
                  connected
                      ? "✅ Dispositivo Conectado"
                      : "❌ No conectado por Bluetooth",
                  style: TextStyle(
                    color: connected ? Colors.green[800] : Colors.red[800],
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  "Aquí puedes programar horarios automáticos. El dispositivo guardará estos horarios incluso si se apaga.",
                  textAlign: TextAlign.center,
                  style: TextStyle(color: Colors.grey[700], fontSize: 13),
                ),
              ],
            ),
          ),
          if (!isAuto)
            Expanded(
              child: Center(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(
                      Icons.lock_clock_outlined,
                      size: 64,
                      color: Colors.grey[400],
                    ),
                    const SizedBox(height: 16),
                    Text(
                      "CALENDARIO DESACTIVADO",
                      style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                        color: Colors.grey[600],
                      ),
                    ),
                    const SizedBox(height: 8),
                    Padding(
                      padding: const EdgeInsets.symmetric(horizontal: 40),
                      child: Text(
                        "Cambia a MODO AUTOMÁTICO en la pestaña de inicio para habilitar la programación.",
                        textAlign: TextAlign.center,
                        style: TextStyle(color: Colors.grey[500]),
                      ),
                    ),
                  ],
                ),
              ),
            )
          else
            Expanded(
              child: ListView.builder(
                padding: const EdgeInsets.symmetric(
                  horizontal: 16,
                  vertical: 10,
                ),
                itemCount: 4,
                itemBuilder: (context, index) {
                  final int ch = index + 1;
                  final String label = _labels[index];
                  final List<SchedInterval> scheds = _channelSchedules[index];

                  return Card(
                    margin: const EdgeInsets.only(bottom: 16),
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(15),
                    ),
                    elevation: 3,
                    child: ExpansionTile(
                      leading: CircleAvatar(
                        backgroundColor: scheds.isNotEmpty
                            ? Colors.blue
                            : Colors.grey[200],
                        child: Icon(
                          scheds.isNotEmpty
                              ? Icons.auto_mode
                              : Icons.timer_off_outlined,
                          color: scheds.isNotEmpty ? Colors.white : Colors.grey,
                        ),
                      ),
                      title: Text(
                        label,
                        style: const TextStyle(
                          fontWeight: FontWeight.bold,
                          fontSize: 18,
                        ),
                      ),
                      subtitle: Row(
                        children: [
                          Text(
                            scheds.isEmpty
                                ? "Sin horarios"
                                : "${scheds.length} horarios",
                            style: TextStyle(
                              color: scheds.isEmpty ? Colors.grey : Colors.blue,
                            ),
                          ),
                          const SizedBox(width: 8),
                          Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 6,
                              vertical: 2,
                            ),
                            decoration: BoxDecoration(
                              color: Colors.blue[100],
                              borderRadius: BorderRadius.circular(4),
                            ),
                            child: const Text(
                              "AUTO",
                              style: TextStyle(
                                fontSize: 10,
                                color: Colors.blue,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ),
                        ],
                      ),
                      children: [
                        ...scheds.asMap().entries.map((entry) {
                          final int listIdx = entry.key;
                          final SchedInterval interval = entry.value;
                          return ListTile(
                            dense: true,
                            title: Text(
                              interval.timeRange,
                              style: const TextStyle(
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            subtitle: Text(_getMaskString(interval.mask)),
                            trailing: IconButton(
                              icon: const Icon(
                                Icons.delete_outline,
                                color: Colors.red,
                              ),
                              onPressed: () => _removeSchedule(ch, listIdx),
                            ),
                          );
                        }),
                        Padding(
                          padding: const EdgeInsets.all(8.0),
                          child: Row(
                            mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                            children: [
                              TextButton.icon(
                                onPressed: connected
                                    ? () => _addSchedule(ch, label)
                                    : null,
                                icon: const Icon(Icons.add),
                                label: const Text("Agregar Horario"),
                              ),
                              if (scheds.isNotEmpty)
                                TextButton.icon(
                                  onPressed: connected
                                      ? () => _clearAllSchedules(ch)
                                      : null,
                                  icon: const Icon(
                                    Icons.clear_all,
                                    color: Colors.orange,
                                  ),
                                  label: const Text(
                                    "Limpiar Todo",
                                    style: TextStyle(color: Colors.orange),
                                  ),
                                ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  );
                },
              ),
            ),
          if (connected)
            Padding(
              padding: const EdgeInsets.all(20),
              child: ElevatedButton.icon(
                onPressed: _syncTime,
                icon: const Icon(Icons.access_time),
                label: const Text("Sincronizar hora del sistema"),
                style: ElevatedButton.styleFrom(
                  minimumSize: const Size(double.infinity, 50),
                  backgroundColor: Colors.blueGrey,
                  foregroundColor: Colors.white,
                ),
              ),
            ),
        ],
      ),
    );
  }
}

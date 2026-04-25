import 'package:http/http.dart' as http;
import 'dart:convert';
import 'package:flutter/foundation.dart';

class WifiManager {
  static final WifiManager _instance = WifiManager._internal();
  factory WifiManager() => _instance;
  WifiManager._internal();

  final String _baseUrl = "http://metzabok.local";

  final ValueNotifier<bool> isWifiAvailable = ValueNotifier<bool>(false);
  final ValueNotifier<List<bool>> relayStates = ValueNotifier<List<bool>>([
    false,
    false,
    false,
    false,
  ]);
  final ValueNotifier<bool> isAutoMode = ValueNotifier<bool>(false);

  Future<bool> checkConnection() async {
    try {
      final response = await http
          .get(Uri.parse("$_baseUrl/status"))
          .timeout(const Duration(seconds: 3));
      if (response.statusCode == 200) {
        isWifiAvailable.value = true;
        _parseStatus(response.body);
        return true;
      }
    } catch (e) {
      debugPrint("WiFi connection error: $e");
    }
    isWifiAvailable.value = false;
    return false;
  }

  void _parseStatus(String body) {
    try {
      final data = jsonDecode(body);
      isAutoMode.value = data['mode'] == 'auto';
      final List<dynamic> relays = data['relays'];
      relayStates.value = relays.cast<bool>();
    } catch (e) {
      debugPrint("Error parsing status: $e");
    }
  }

  Future<bool> sendCommand(String path) async {
    try {
      final response = await http
          .get(Uri.parse("$_baseUrl$path"))
          .timeout(const Duration(seconds: 5));
      if (response.statusCode == 200) {
        await checkConnection(); // Update states
        return true;
      }
    } catch (e) {
      debugPrint("Error sending command: $e");
    }
    return false;
  }

  Future<bool> setRelay(int index, bool state) async {
    final String action = state ? "on" : "off";
    return await sendCommand("/foco${index + 1}/$action");
  }

  Future<bool> setMode(bool auto) async {
    final String action = auto ? "auto" : "manual";
    return await sendCommand("/mode/$action");
  }

  Future<bool> allOff() async {
    return await sendCommand("/alloff");
  }
}

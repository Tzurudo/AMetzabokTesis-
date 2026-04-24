import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';
import 'package:flutter/foundation.dart';

class TelegramService {
  static final TelegramService _instance = TelegramService._internal();
  factory TelegramService() => _instance;
  TelegramService._internal();

  String? _token;
  String? _chatId;

  Future<void> init() async {
    final prefs = await SharedPreferences.getInstance();
    _token = prefs.getString('telegram_token');
    _chatId = prefs.getString('telegram_chat_id');
  }

  bool get isConfigured => _token != null && _token!.isNotEmpty && _chatId != null && _chatId!.isNotEmpty;

  Future<bool> sendCommand(String text) async {
    if (!isConfigured) {
      debugPrint("TelegramService: Not configured");
      return false;
    }

    final url = Uri.parse("https://api.telegram.org/bot$_token/sendMessage");
    try {
      final response = await http.post(
        url,
        body: {
          'chat_id': _chatId,
          'text': text,
          'parse_mode': 'Markdown',
        },
      ).timeout(const Duration(seconds: 10));

      if (response.statusCode == 200) {
        debugPrint("TelegramService: Command sent successfully: $text");
        return true;
      } else {
        debugPrint("TelegramService: Error sending command: ${response.body}");
        return false;
      }
    } catch (e) {
      debugPrint("TelegramService: Exception: $e");
      return false;
    }
  }

  Future<void> saveCredentials(String token, String chatId) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('telegram_token', token);
    await prefs.setString('telegram_chat_id', chatId);
    _token = token;
    _chatId = chatId;
  }
}

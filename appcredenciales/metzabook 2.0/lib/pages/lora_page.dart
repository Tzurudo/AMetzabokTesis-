import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';

class LoraPage extends StatefulWidget {
  const LoraPage({super.key});

  @override
  State<LoraPage> createState() => _LoraPageState();
}

class _LoraPageState extends State<LoraPage>
    with SingleTickerProviderStateMixin {
  late AnimationController _controller;
  late Animation<double> _fadeAnim;
  late Animation<Offset> _slideAnim;

  // Paleta consistente con la app (blanco/oro/plata)
  static const Color premiumGold = Color(0xFFD4AF37);

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 900),
    );
    _fadeAnim = CurvedAnimation(parent: _controller, curve: Curves.easeOut);
    _slideAnim = Tween<Offset>(
      begin: const Offset(0, 0.06),
      end: Offset.zero,
    ).animate(
        CurvedAnimation(parent: _controller, curve: Curves.easeOutCubic));
    _controller.forward();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  Future<void> _launchMetzabok() async {
    final uri = Uri.parse('http://metzabok.local');
    if (!await launchUrl(uri, mode: LaunchMode.externalApplication)) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: const Text(
              'No se pudo abrir metzabok.local — asegúrate de estar en la misma red WiFi',
              style: TextStyle(color: Colors.white),
            ),
            backgroundColor: Colors.red[700],
            behavior: SnackBarBehavior.floating,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(10),
            ),
          ),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.white,
      appBar: AppBar(
        backgroundColor: Colors.white,
        elevation: 0,
        leading: IconButton(
          icon: const Icon(Icons.arrow_back_ios_new, color: premiumGold),
          onPressed: () => Navigator.pop(context),
        ),
        title: const Text(
          'LoRa — Acceso Remoto',
          style: TextStyle(
            color: premiumGold,
            fontWeight: FontWeight.bold,
            fontSize: 18,
            letterSpacing: 1.1,
          ),
        ),
        centerTitle: true,
      ),
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: [Colors.white, Color(0xFFF5F5F5)],
          ),
        ),
        child: FadeTransition(
          opacity: _fadeAnim,
          child: SlideTransition(
            position: _slideAnim,
            child: SingleChildScrollView(
              padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 10),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  // ── Hero card ──
                  _HeroCard(),
                  const SizedBox(height: 24),

                  // ── Sección: ¿Cómo funciona? ──
                  const _SectionLabel('¿Cómo funciona LoRa?'),
                  const SizedBox(height: 12),

                  const _StepCard(
                    step: '1',
                    icon: Icons.router_outlined,
                    title: 'Gateway ESP32-C3',
                    body:
                        'El Gateway ESP32-C3 recibe los comandos por Bluetooth (BLE) desde tu teléfono y los retransmite al dispositivo Metzabok vía radio LoRa a 915 MHz.',
                  ),
                  const SizedBox(height: 10),
                  const _StepCard(
                    step: '2',
                    icon: Icons.settings_input_antenna,
                    title: 'Enlace de largo alcance',
                    body:
                        'LoRa (Long Range) permite comunicación hasta 2 km en campo abierto, ideal para invernaderos o terrenos amplios sin cobertura WiFi.',
                  ),
                  const SizedBox(height: 10),
                  const _StepCard(
                    step: '3',
                    icon: Icons.wifi_tethering,
                    title: 'Modo WiFi opcional',
                    body:
                        'Una vez que el Metzabok esté conectado a tu red WiFi, podrás controlarlo directamente desde el navegador en metzabok.local, sin necesidad de Bluetooth.',
                  ),
                  const SizedBox(height: 10),
                  const _StepCard(
                    step: '4',
                    icon: Icons.electric_bolt_outlined,
                    title: 'Control de relays',
                    body:
                        'Los 4 canales del Metzabok responden a comandos en tiempo real (ON/OFF), programaciones horarias y modo automático, independientemente del canal de comunicación activo.',
                  ),
                  const SizedBox(height: 28),

                  // ── Requisitos ──
                  const _SectionLabel('Requisitos para usar la web'),
                  const SizedBox(height: 12),
                  const _RequirementTile(
                    icon: Icons.wifi,
                    text:
                        'Tu teléfono y el Metzabok deben estar en la misma red WiFi.',
                    ok: true,
                  ),
                  const _RequirementTile(
                    icon: Icons.language,
                    text:
                        'El Metzabok debe haber recibido el comando SETWIFI y conectarse exitosamente.',
                    ok: true,
                  ),
                  const _RequirementTile(
                    icon: Icons.dns_outlined,
                    text:
                        'Si metzabok.local no resuelve, usa la IP mostrada en la pantalla OLED del dispositivo.',
                    ok: null,
                  ),
                  const SizedBox(height: 32),

                  // ── Botón Comenzar ──
                  _ComenzarButton(onTap: _launchMetzabok),
                  const SizedBox(height: 40),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

// ─────────────────────────────────────────────
// Widgets internos — Paleta blanco/oro/plata
// ─────────────────────────────────────────────

class _HeroCard extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(18),
        color: const Color(0xFFFFF8E1),
        border: Border.all(
          color: const Color(0xFFD4AF37),
          width: 1,
        ),
        boxShadow: [
          BoxShadow(
            color: const Color(0xFFD4AF37).withValues(alpha: 0.15),
            blurRadius: 16,
            offset: const Offset(0, 5),
          ),
        ],
      ),
      child: Row(
        children: [
          Container(
            padding: const EdgeInsets.all(14),
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: Colors.white,
              border: Border.all(
                color: const Color(0xFFD4AF37).withValues(alpha: 0.6),
                width: 1.5,
              ),
              boxShadow: [
                BoxShadow(
                  color: const Color(0xFFD4AF37).withValues(alpha: 0.2),
                  blurRadius: 10,
                ),
              ],
            ),
            child: const Icon(
              Icons.settings_input_antenna,
              color: Color(0xFFD4AF37),
              size: 30,
            ),
          ),
          const SizedBox(width: 16),
          const Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Metzabok LoRa',
                  style: TextStyle(
                    color: Color(0xFF2D2D2D),
                    fontSize: 20,
                    fontWeight: FontWeight.bold,
                    letterSpacing: 0.8,
                  ),
                ),
                SizedBox(height: 4),
                Text(
                  'Control inalámbrico de largo alcance\nvía 915 MHz • WiFi • Bluetooth',
                  style: TextStyle(
                    color: Color(0xFF757575),
                    fontSize: 12.5,
                    height: 1.5,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _SectionLabel extends StatelessWidget {
  final String text;
  const _SectionLabel(this.text);

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Container(
          width: 3,
          height: 18,
          decoration: BoxDecoration(
            color: const Color(0xFFD4AF37),
            borderRadius: BorderRadius.circular(2),
          ),
        ),
        const SizedBox(width: 8),
        Text(
          text,
          style: const TextStyle(
            color: Color(0xFF2D2D2D),
            fontSize: 15,
            fontWeight: FontWeight.bold,
            letterSpacing: 0.5,
          ),
        ),
      ],
    );
  }
}

class _StepCard extends StatelessWidget {
  final String step;
  final IconData icon;
  final String title;
  final String body;

  const _StepCard({
    required this.step,
    required this.icon,
    required this.title,
    required this.body,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
          color: const Color(0xFFD4AF37).withValues(alpha: 0.3),
          width: 1,
        ),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.04),
            blurRadius: 6,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Step badge
          Container(
            width: 32,
            height: 32,
            alignment: Alignment.center,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              gradient: const LinearGradient(
                colors: [Color(0xFFD4AF37), Color(0xFFF7E6AD)],
              ),
              boxShadow: [
                BoxShadow(
                  color: const Color(0xFFD4AF37).withValues(alpha: 0.3),
                  blurRadius: 6,
                ),
              ],
            ),
            child: Text(
              step,
              style: const TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.bold,
                fontSize: 13,
              ),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Icon(icon, color: const Color(0xFFD4AF37), size: 16),
                    const SizedBox(width: 6),
                    Text(
                      title,
                      style: const TextStyle(
                        color: Color(0xFF2D2D2D),
                        fontWeight: FontWeight.w600,
                        fontSize: 14,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 6),
                Text(
                  body,
                  style: const TextStyle(
                    color: Color(0xFF757575),
                    fontSize: 12.5,
                    height: 1.55,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _RequirementTile extends StatelessWidget {
  final IconData icon;
  final String text;
  final bool? ok;

  const _RequirementTile({
    required this.icon,
    required this.text,
    required this.ok,
  });

  @override
  Widget build(BuildContext context) {
    final Color color =
        ok == null
            ? const Color(0xFFF5A623)
            : ok!
            ? const Color(0xFF4CAF50)
            : const Color(0xFFE53935);

    final IconData badge =
        ok == null
            ? Icons.info_outline
            : ok!
            ? Icons.check_circle_outline
            : Icons.cancel_outlined;

    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(badge, color: color, size: 18),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              text,
              style: const TextStyle(
                color: Color(0xFF757575),
                fontSize: 13,
                height: 1.5,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _ComenzarButton extends StatefulWidget {
  final VoidCallback onTap;
  const _ComenzarButton({required this.onTap});

  @override
  State<_ComenzarButton> createState() => _ComenzarButtonState();
}

class _ComenzarButtonState extends State<_ComenzarButton> {
  bool _pressed = false;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) => setState(() => _pressed = true),
      onTapUp: (_) {
        setState(() => _pressed = false);
        widget.onTap();
      },
      onTapCancel: () => setState(() => _pressed = false),
      child: AnimatedScale(
        scale: _pressed ? 0.96 : 1.0,
        duration: const Duration(milliseconds: 100),
        child: Container(
          width: double.infinity,
          height: 58,
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(30),
            gradient: const LinearGradient(
              colors: [
                Color(0xFFD4AF37),
                Color(0xFFF7E6AD),
                Color(0xFFD4AF37),
              ],
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
            ),
            boxShadow: [
              BoxShadow(
                color: const Color(0xFFD4AF37).withValues(alpha: 0.45),
                blurRadius: 20,
                offset: const Offset(0, 6),
              ),
            ],
          ),
          child: const Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(Icons.language, color: Colors.black87, size: 22),
              SizedBox(width: 10),
              Text(
                'Comenzar — Abrir metzabok.local',
                style: TextStyle(
                  color: Colors.black87,
                  fontSize: 16,
                  fontWeight: FontWeight.bold,
                  letterSpacing: 0.5,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

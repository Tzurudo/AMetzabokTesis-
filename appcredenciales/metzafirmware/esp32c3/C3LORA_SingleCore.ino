/* ═══════════════════════════════════════════════════════════════
   METZABOOK ESP32-C3 — GATEWAY WiFi AP + LoRa + mDNS
   ═══════════════════════════════════════════════════════════════ */
#include <ESPmDNS.h>
#include <LoRa.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>

// ==================== PINES LoRa (ESP32-C3) ====================
#define PIN_LORA_NSS 7
#define PIN_LORA_RST 3
#define PIN_LORA_DIO0 1
#define PIN_LORA_SCK 6
#define PIN_LORA_MOSI 4
#define PIN_LORA_MISO 5
#define LED_STATUS 8

// ==================== CONFIGURACIÓN WiFi AP ====================
const char *ap_ssid = "Metzabook-Gateway";
const char *ap_password = "12345678"; // mínimo 8 caracteres
const IPAddress ap_ip(192, 168, 4, 1);
const IPAddress ap_gateway(192, 168, 4, 1);
const IPAddress ap_subnet(255, 255, 255, 0);

// ==================== mDNS ====================
const char *mdns_hostname =
    "metzabook"; // accesible como http://metzabook.local

// ==================== DIRECCIONES LoRa ====================
#define GATEWAY_ADDR 0x01
#define SLAVE_ADDR 0x02

// ==================== CONFIGURACIÓN LoRa ====================
#define LORA_FREQ 915E6
#define LORA_TX_POWER 20
#define LORA_SF 12
#define LORA_BW 125E3
#define LORA_CR 5

// ==================== SERVIDOR WEB ====================
WebServer server(80);

// ==================== VARIABLES GLOBALES ====================
String lastLoRaResponse = "";
unsigned long lastResponseTime = 0;

// ==================== FUNCIÓN: Enviar LoRa al Esclavo ====================
bool sendLoRaToSlave(const char *message) {
  if (!message || strlen(message) == 0)
    return false;

  LoRa.beginPacket();
  LoRa.write(SLAVE_ADDR);   // Destino: esclavo 0x02
  LoRa.write(GATEWAY_ADDR); // Origen: gateway 0x01
  LoRa.write((uint8_t)strlen(message));
  LoRa.print(message);

  bool success = LoRa.endPacket();

  Serial.print("📡 [LoRa TX] → 0x02: ");
  Serial.println(message);

  if (!success) {
    Serial.println("❌ [LoRa TX] Falló el envío");
  }

  return success;
}

// ==================== FUNCIÓN: Recibir LoRa ====================
void receiveLoRa() {
  int packetSize = LoRa.parsePacket();
  if (packetSize < 4)
    return;

  uint8_t dest = LoRa.read();
  uint8_t src = LoRa.read();
  uint8_t len = LoRa.read();

  if (len > 120)
    len = 120;

  char buffer[121] = {0};
  for (uint8_t i = 0; i < len; i++) {
    buffer[i] = (char)LoRa.read();
  }

  // Verificar si el paquete es para este gateway
  if (dest != GATEWAY_ADDR && dest != 0xFF)
    return;

  // solo respuestas del esclavo autorizado
  if (src == SLAVE_ADDR) {
    lastLoRaResponse = String(buffer);
    lastResponseTime = millis();

    Serial.print("📡 [LoRa RX] ← 0x");
    Serial.print(src, HEX);
    Serial.print(": ");
    Serial.println(buffer);
    Serial.print("   RSSI: ");
    Serial.println(LoRa.packetRssi());
  }
}

// ==================== ENDPOINTS HTTP ====================

// Enviar respuesta JSON
void sendJSON(const char *status, const char *message) {
  String json = "{\"status\":\"" + String(status) + "\",";
  json += "\"message\":\"" + String(message) + "\",";
  json += "\"timestamp\":\"" + String(millis()) + "\"}";
  server.send(200, "application/json", json);
}

// Raíz - Panel de control HTML
// Raíz - Panel de control HTML Premium
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Metzabok — Gateway</title>
<style>
  :root{
    --gold:#D4AF37; --gold-light:#F7E6AD; --gold-soft:rgba(212,175,55,0.1);
    --white:#FFFFFF; --silver:#E0E0E0; --bg:#F8F9FA;
    --text:#2D2D2D; --muted:#757575; --green:#4CAF50; --red:#E53935;
    --blue:#297AF3; --shadow:0 10px 30px rgba(0,0,0,0.08);
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:linear-gradient(135deg, var(--white), var(--bg)); color:var(--text);
       font-family:'Segoe UI',system-ui,sans-serif; min-height:100vh; overflow-x:hidden}
  
  nav{background:rgba(255,255,255,0.9); backdrop-filter:blur(10px); border-bottom:1px solid var(--silver);
      display:flex; align-items:center; justify-content:space-between;
      padding:16px 24px; position:sticky; top:0; z-index:999; box-shadow:0 2px 15px rgba(0,0,0,0.03)}
  .logo{display:flex; align-items:center; gap:12px}
  .logo svg{width:32px; height:32px}
  .logo span{font-size:1.4rem; font-weight:800; color:var(--text); letter-spacing:1px}
  .logo span b{color:var(--gold)}
  .subtitle{font-size:.7rem; color:var(--muted); font-weight:600; text-transform:uppercase}
  #status-badge{font-size:.75rem; padding:5px 12px; border-radius:30px;
                background:rgba(41,122,243,0.1); color:var(--blue); border:1px solid var(--blue); font-weight:700}

  .tabs{display:flex; justify-content:center; gap:10px; background:var(--white); padding:10px; margin: 15px 24px;
        border-radius:15px; box-shadow:var(--shadow)}
  .tab{padding:12px 20px; cursor:pointer; font-size:.9rem; font-weight:700; border-radius:10px;
       color:var(--muted); transition:0.3s; display:flex; align-items:center; gap:8px}
  .tab.active{color:var(--white); background:var(--gold); box-shadow:0 4px 12px rgba(212,175,55,0.4)}

  .section{display:none; padding:10px 24px; max-width:800px; margin:0 auto; animation:fadeIn 0.4s ease}
  .section.active{display:block}
  @keyframes fadeIn { from{opacity:0; transform:translateY(10px)} to{opacity:1; transform:translateY(0)} }

  .relay-grid{display:grid; grid-template-columns:repeat(auto-fit, minmax(180px, 1fr)); gap:15px; margin-top:20px}
  .relay-card{background:var(--white); border-radius:20px; padding:20px; text-align:center;
              border:1px solid rgba(0,0,0,0.05); position:relative; overflow:hidden;
              box-shadow:var(--shadow); transition:0.3s}
  .relay-card.pending{opacity:0.6; pointer-events:none}
  
  .ch-num{position:absolute; top:12px; right:12px; font-size:0.7rem; font-weight:800; color:var(--silver)}
  .relay-icon{font-size:2.5rem; margin-bottom:10px; display:block}
  .relay-name{font-weight:700; font-size:1.05rem; display:block; margin-bottom:15px}
  
  .toggle-btn{width:100%; padding:12px; border:none; border-radius:15px; font-weight:800;
              cursor:pointer; transition:0.3s; letter-spacing:1px; text-transform:uppercase; font-size:0.8rem}
  .btn-off{background:var(--bg); color:var(--muted); border:1px solid var(--silver)}
  .btn-on{background:linear-gradient(135deg, var(--gold), var(--gold-light)); color:var(--white)}

  .terminal-container{margin:30px 0; background:#1E1E1E; border-radius:15px; overflow:hidden; border:1px solid #333}
  .term-header{background:#2D2D2D; padding:10px 15px; display:flex; align-items:center; gap:8px}
  .term-dot{width:10px; height:10px; border-radius:50%}
  #terminal{height:180px; overflow-y:auto; padding:15px; font-family:monospace; font-size:0.85rem; color:#00FF00}
  .term-line{margin-bottom:4px}
  .term-line.cmd{color:#00AAFF}

  #toast{position:fixed; bottom:30px; left:50%; transform:translateX(-50%);
         background:#2D2D2D; color:var(--white); padding:12px 25px; border-radius:30px;
         font-size:0.9rem; font-weight:600; opacity:0; transition:0.4s; z-index:9999}
  #toast.show{opacity:1; bottom:50px}
</style>
</head>
<body>

<nav>
  <div class="logo">
    <svg viewBox="0 0 100 100" fill="none" xmlns="http://www.w3.org/2000/svg">
      <circle cx="50" cy="50" r="45" stroke="#D4AF37" stroke-width="8"/>
      <path d="M30 50 L45 65 L70 35" stroke="#D4AF37" stroke-width="8" stroke-linecap="round"/>
    </svg>
    <div>
      <span>Metza<b>Gateway</b></span>
      <div class="subtitle">Enlace LoRa Chapingo</div>
    </div>
  </div>
  <span id="status-badge">CONECTADO</span>
</nav>

<div class="tabs">
  <div class="tab active" onclick="switchTab('control',this)">Control LoRa</div>
  <div class="tab" onclick="switchTab('term',this)">Consola Radio</div>
</div>

<div class="section active" id="sec-control">
  <div class="relay-grid">
    <script>
      for(let i=1; i<=4; i++){
        document.write(`
          <div class="relay-card" id="card-${i}">
            <span class="ch-num">CH${i}</span>
            <span class="relay-icon">&#128225;</span>
            <span class="relay-name">Canal ${i}</span>
            <div style="display:flex; gap:8px">
              <button class="toggle-btn btn-on" onclick="send('R${i}1')">ON</button>
              <button class="toggle-btn btn-off" onclick="send('R${i}0')">OFF</button>
            </div>
          </div>
        `);
      }
    </script>
  </div>
  
  <div style="margin-top:25px; background:var(--white); padding:20px; border-radius:20px; box-shadow:var(--shadow); display:flex; gap:10px">
    <button class="toggle-btn btn-off" style="background:#2D2D2D; color:white" onclick="send('GM')">MANUAL</button>
    <button class="toggle-btn btn-on" onclick="send('GA')">AUTO</button>
    <button class="toggle-btn btn-off" style="background:var(--blue); color:white" onclick="send('S?')">ESTADO</button>
  </div>
</div>

<div class="section" id="sec-term">
  <div class="terminal-container">
    <div class="term-header">
      <div class="term-dot" style="background:#FF5F56"></div>
      <div class="term-dot" style="background:#27C93F"></div>
      <span style="color:#AAA; font-size:0.7rem; margin-left:10px">GATEWAY MONITOR</span>
    </div>
    <div id="terminal"></div>
  </div>
</div>

<div id="toast"></div>

<script>
function log(msg, type=''){
  const t = document.getElementById('terminal');
  const line = document.createElement('div');
  line.className = 'term-line ' + type;
  line.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  t.appendChild(line);
  t.scrollTop = t.scrollHeight;
}

async function send(cmd){
  log(`Enviando Comando: ${cmd}`, 'cmd');
  toast(`Enviando ${cmd}...`);
  try {
    const r = await fetch(`/cmd?q=${cmd}`);
    const d = await r.json();
    log(`Respuesta: ${d.message}`);
    toast(`Respuesta: ${d.message}`);
  } catch(e) {
    log('ERROR de conexión', 'err');
    toast('Fallo de conexión', '#E53935');
  }
}

function switchTab(id, el){
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.querySelectorAll('.section').forEach(s=>s.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('sec-'+id).classList.add('active');
}

function toast(msg, color='#D4AF37'){
  const t = document.getElementById('toast');
  t.textContent = msg; t.style.border = `1px solid ${color}`;
  t.classList.add('show');
  setTimeout(()=>t.classList.remove('show'), 3000);
}

log('Gateway iniciado.');
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// Endpoint para comandos
void handleCmd() {
  if (!server.hasArg("q")) {
    sendJSON("error", "Falta parámetro 'q'");
    return;
  }

  String cmd = server.arg("q");
  cmd.trim();
  cmd.toUpperCase();

  Serial.print("📱 [HTTP] Comando recibido: ");
  Serial.println(cmd);

  // Comandos locales del gateway
  if (cmd == "PING") {
    sendJSON("ok", "PONG:GATEWAY:0x01");
    return;
  }

  if (cmd == "HELP") {
    sendJSON(
        "ok",
        "COMANDOS: "
        "ON1-4|OFF1-4|ALLOFF|STATUS|GLOBAL_AUTO|GLOBAL_MANUAL|SETTIME|PING");
    return;
  }

  if (cmd == "STATUS") {
    if (lastLoRaResponse.length() > 0) {
      sendJSON("ok", lastLoRaResponse.c_str());
    } else {
      sendJSON("waiting", "Esperando respuesta del esclavo...");
    }
    return;
  }

  // Enviar comando al esclavo por LoRa
  if (sendLoRaToSlave(cmd.c_str())) {
    // Esperar respuesta (máximo 2 segundos)
    unsigned long start = millis();
    lastLoRaResponse = "";

    while (millis() - start < 2000) {
      receiveLoRa(); // procesar paquetes entrantes
      if (lastLoRaResponse.length() > 0) {
        sendJSON("ok", lastLoRaResponse.c_str());
        return;
      }
      delay(10);
    }

    sendJSON("pending", "Comando enviado, esperando respuesta del esclavo...");
  } else {
    sendJSON("error", "Fallo al enviar por LoRa");
  }
}

// Endpoint de estado
void handleStatus() {
  String json = "{";
  json += "\"gateway\":\"0x01\",";
  json += "\"ssid\":\"" + String(ap_ssid) + "\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"mdns\":\"http://metzabook.local\",";
  json += "\"lora_ok\":true,";
  json += "\"uptime\":" + String(millis());
  json += "}";
  server.send(200, "application/json", json);
}

// Endpoint para comandos vía POST (para la App)
void handleApiCmd() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Body required\"}");
    return;
  }

  String cmd = server.arg("plain");
  cmd.trim();

  Serial.print("📱 [POST API] Comando: ");
  Serial.println(cmd);

  if (sendLoRaToSlave(cmd.c_str())) {
    server.send(200, "application/json",
                "{\"status\":\"ok\",\"cmd\":\"" + cmd + "\"}");
  } else {
    server.send(500, "application/json",
                "{\"status\":\"error\",\"msg\":\"LoRa TX failed\"}");
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, LOW);

  Serial.println("\n╔════════════════════════════════════════════════════╗");
  Serial.println("║     METZABOOK GATEWAY - ESP32-C3                 ║");
  Serial.println("║     WiFi AP + LoRa + mDNS                        ║");
  Serial.println("╚════════════════════════════════════════════════════╝\n");

  // ==================== 1. INICIAR WiFi AP ====================
  Serial.println("📡 Configurando Punto de Acceso WiFi...");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_ip, ap_gateway, ap_subnet);
  WiFi.softAP(ap_ssid, ap_password);

  Serial.print("✅ AP Iniciado: ");
  Serial.println(ap_ssid);
  Serial.print("   Contraseña: ");
  Serial.println(ap_password);
  Serial.print("   IP del Gateway: ");
  Serial.println(WiFi.softAPIP());

  // ==================== 2. INICIAR mDNS ====================
  if (MDNS.begin(mdns_hostname)) {
    Serial.print("✅ mDNS iniciado: http://");
    Serial.print(mdns_hostname);
    Serial.println(".local");
  } else {
    Serial.println("❌ Error al iniciar mDNS");
  }

  // ==================== 3. CONFIGURAR SERVIDOR WEB ====================
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/status", handleStatus);
  server.on("/api/cmd", HTTP_POST, handleApiCmd);
  server.onNotFound([]() {
    server.send(404, "application/json",
                "{\"error\":\"Endpoint no encontrado\"}");
  });

  server.begin();
  Serial.println("✅ Servidor HTTP iniciado en puerto 80");

  // ==================== 4. INICIAR LoRa ====================
  Serial.println("\n📡 Inicializando LoRa...");

  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
  LoRa.setPins(PIN_LORA_NSS, PIN_LORA_RST, PIN_LORA_DIO0);

  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("❌ ERROR CRÍTICO: LoRa no responde!");
    Serial.println("   Verifica las conexiones:");
    Serial.println("   ┌─────────────┬──────┐");
    Serial.println("   │ NSS  → GPIO7 │ RST  → GPIO3 │");
    Serial.println("   │ DIO0 → GPIO1 │ SCK  → GPIO6 │");
    Serial.println("   │ MOSI → GPIO4 │ MISO → GPIO5 │");
    Serial.println("   └─────────────┴──────┘");

    while (true) {
      digitalWrite(LED_STATUS, HIGH);
      delay(200);
      digitalWrite(LED_STATUS, LOW);
      delay(200);
    }
  }

  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);

  Serial.println("✅ LoRa inicializado correctamente");
  Serial.print("   Frecuencia: ");
  Serial.print(LORA_FREQ / 1E6);
  Serial.println(" MHz");
  Serial.print("   Dirección Gateway: 0x");
  Serial.println(GATEWAY_ADDR, HEX);

  // ==================== 5. RESÚMEN FINAL ====================
  Serial.println("\n╔════════════════════════════════════════════════════╗");
  Serial.println("║              ✅ GATEWAY LISTO!                    ║");
  Serial.println("╠════════════════════════════════════════════════════╣");
  Serial.println("║  🌐 WiFi AP:                                     ║");
  Serial.print("║     SSID:      ");
  Serial.print(ap_ssid);
  Serial.println("                  ║");
  Serial.print("║     Password:  ");
  Serial.print(ap_password);
  Serial.println("                  ║");
  Serial.print("║     IP:        ");
  Serial.print(WiFi.softAPIP());
  Serial.println("               ║");
  Serial.println("║                                                 ║");
  Serial.println("║  🔗 Accesos:                                     ║");
  Serial.print("║     http://");
  Serial.print(ap_ip);
  Serial.println("                   ║");
  Serial.print("║     http://");
  Serial.print(mdns_hostname);
  Serial.println(".local            ║");
  Serial.println("║                                                 ║");
  Serial.println("║  📡 LoRa:                                        ║");
  Serial.print("║     Frecuencia: ");
  Serial.print(LORA_FREQ / 1E6);
  Serial.println(" MHz                 ║");
  Serial.println("║     Esclavo: 0x02                               ║");
  Serial.println("╚════════════════════════════════════════════════════╝\n");
}

// ==================== LOOP PRINCIPAL ====================
void loop() {
  // Atender peticiones HTTP
  server.handleClient();

  // Procesar paquetes LoRa entrantes
  receiveLoRa();

  // LED heartbeat
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    lastBlink = millis();
    digitalWrite(LED_STATUS, HIGH);
    delay(30);
    digitalWrite(LED_STATUS, LOW);
  }

  // Mostrar actividad de red cada 30 segundos
  static unsigned long lastNetworkLog = 0;
  if (millis() - lastNetworkLog > 30000) {
    lastNetworkLog = millis();
    Serial.print("📊 Clientes conectados: ");
    Serial.println(WiFi.softAPgetStationNum());
  }
}
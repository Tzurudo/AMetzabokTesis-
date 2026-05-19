#include "CommManager.h"
#include "Storage.h"
#include <ESPmDNS.h>
#include <SPI.h>
#include <LoRa.h>

// ─── Dirección propia del esclavo ───
#define SLAVE_ADDR   0x02
#define GATEWAY_ADDR 0x01

// ─── Enviar respuesta al Gateway por LoRa ───
static void sendLoRaResponse(const char* response) {
  LoRa.beginPacket();
  LoRa.write(GATEWAY_ADDR);           // destino: gateway
  LoRa.write(SLAVE_ADDR);             // origen:  este esclavo
  LoRa.write((uint8_t)strlen(response));
  LoRa.print(response);
  LoRa.endPacket();
  Serial.print("[LoRa TX] -> GW: ");
  Serial.println(response);
}

// ─── Helper: responder por el canal activo ───
static void reply(const char* msg) {
  if (bluetoothEnabled) {
    SerialBT.println(msg);
  } else {
    sendLoRaResponse(msg);
  }
}

// ─── LoRa Setup ───
void setupLoRa() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(915E6)) {
    Serial.println("LoRa init FAILED");
  } else {
    Serial.println("LoRa init OK (915MHz)");
    LoRa.setSpreadingFactor(12);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(5);
    LoRa.setTxPower(20);
  }
}

// ─── LoRa Receive: parsea correctamente los 3 bytes de cabecera ───
void handleLoRa() {
  int packetSize = LoRa.parsePacket();
  if (packetSize < 3) return;

  uint8_t dest = LoRa.read();   // byte 0: destino
  uint8_t src  = LoRa.read();   // byte 1: origen
  uint8_t len  = LoRa.read();   // byte 2: longitud del payload

  // Leer el payload como texto
  char buffer[129] = {0};
  for (uint8_t i = 0; i < len && i < 128; i++) {
    int c = LoRa.read();
    if (c < 0) break;
    buffer[i] = (char)c;
  }

  // Solo procesar si el paquete es para este esclavo o broadcast
  if (dest != SLAVE_ADDR && dest != 0xFF) return;
  // Solo aceptar del gateway autorizado
  if (src != GATEWAY_ADDR) return;

  Serial.print("[LoRa RX] <- GW: ");
  Serial.println(buffer);

  processCommand(String(buffer));
}

// ─── WiFi no-bloqueante ───
void handleWiFi() {
  if (currentWiFiStatus == WIFI_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      currentWiFiStatus = WIFI_CONNECTED;
      reply(("WIFI OK! IP: " + WiFi.localIP().toString()).c_str());
      MDNS.begin("metzabok");
      MDNS.addService("http", "tcp", 80);
      setupWebServer();
    } else if (millis() - wifiConnectStart > WIFI_TIMEOUT) {
      currentWiFiStatus = WIFI_DISCONNECTED;
      reply("WIFI ERROR: Tiempo agotado");
      WiFi.disconnect();
    }
  }
}

// ─── Página HTML Premium — Paleta blanco/oro/plata (como la app) ────────────
static const char HTML_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Metzabok — Control Premium</title>
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
  
  /* ── NAV ── */
  nav{background:rgba(255,255,255,0.9); backdrop-filter:blur(10px); border-bottom:1px solid var(--silver);
      display:flex; align-items:center; justify-content:space-between;
      padding:16px 24px; position:sticky; top:0; z-index:999; box-shadow:0 2px 15px rgba(0,0,0,0.03)}
  .logo{display:flex; align-items:center; gap:12px}
  .logo svg{width:32px; height:32px; filter:drop-shadow(0 2px 4px rgba(212,175,55,0.3))}
  .logo span{font-size:1.4rem; font-weight:800; color:var(--text); letter-spacing:1px}
  .logo span b{color:var(--gold)}
  .subtitle{font-size:.7rem; color:var(--muted); font-weight:600; text-transform:uppercase; letter-spacing:0.5px}
  #status-badge{font-size:.75rem; padding:5px 12px; border-radius:30px;
                background:var(--gold-soft); color:var(--gold); border:1px solid var(--gold); font-weight:700}

  /* ── TABS ── */
  .tabs{display:flex; justify-content:center; gap:10px; background:var(--white); padding:10px; margin: 15px 24px;
        border-radius:15px; box-shadow:var(--shadow)}
  .tab{padding:12px 20px; cursor:pointer; font-size:.9rem; font-weight:700; border-radius:10px;
       color:var(--muted); transition:0.3s; display:flex; align-items:center; gap:8px}
  .tab:hover{color:var(--gold); background:var(--gold-soft)}
  .tab.active{color:var(--white); background:var(--gold); box-shadow:0 4px 12px rgba(212,175,55,0.4)}

  /* ── SECTIONS ── */
  /* ── SECTIONS ── */
  .section{display:none; padding:10px 24px; max-width:800px; margin:0 auto; animation:fadeIn 0.4s ease}
  .section.active{display:block}
  @keyframes fadeIn { from{opacity:0; transform:translateY(10px)} to{opacity:1; transform:translateY(0)} }

  /* ── MODE SWITCH ── */
  .mode-selector{background:var(--white); border-radius:20px; padding:20px; margin:20px 24px; box-shadow:var(--shadow);
                  display:flex; justify-content:space-between; align-items:center; border:1px solid var(--gold-soft)}
  .mode-label{font-weight:800; font-size:1.1rem; color:var(--text)}
  .mode-label small{display:block; font-size:0.75rem; color:var(--muted); font-weight:600}
  
  .switch{position:relative; display:inline-block; width:60px; height:34px}
  .switch input{opacity:0; width:0; height:0}
  .slider{position:absolute; cursor:pointer; top:0; left:0; right:0; bottom:0; background-color:var(--silver); 
          transition:.4s; border-radius:34px}
  .slider:before{position:absolute; content:""; height:26px; width:26px; left:4px; bottom:4px; background-color:white; 
                 transition:.4s; border-radius:50%}
  input:checked + .slider{background-color:var(--gold)}
  input:checked + .slider:before{transform:translateX(26px)}

  /* ── RELAY GRID ── */
  .relay-grid{display:grid; grid-template-columns:repeat(auto-fit, minmax(180px, 1fr)); gap:15px; margin-top:20px}
  .relay-card{background:var(--white); border-radius:20px; padding:20px; text-align:center;
              border:1px solid rgba(0,0,0,0.05); position:relative; overflow:hidden;
              box-shadow:var(--shadow); transition:0.3s}
  .relay-card.disabled{opacity:0.5; filter:grayscale(0.8); pointer-events:none}
  .relay-card.pending{opacity:0.7; pointer-events:none}
  
  .ch-num{position:absolute; top:12px; right:12px; font-size:0.7rem; font-weight:800; color:var(--silver)}
  .relay-icon{font-size:2.5rem; margin-bottom:10px; display:block; transition:0.3s}
  .relay-name{font-weight:700; font-size:1.05rem; display:block; margin-bottom:15px}
  
  .toggle-btn{width:100%; padding:12px; border:none; border-radius:15px; font-weight:800;
              cursor:pointer; transition:0.3s; letter-spacing:1px; text-transform:uppercase; font-size:0.8rem}
  .btn-off{background:var(--bg); color:var(--muted); border:1px solid var(--silver)}
  .btn-on{background:linear-gradient(135deg, var(--gold), var(--gold-light)); color:var(--white);
          box-shadow:0 4px 15px rgba(212,175,55,0.3)}

  /* ── SCHEDULES ── */
  #sched-container{margin-top:20px; animation:fadeIn 0.4s ease}
  .sched-card{background:var(--white); border-radius:20px; padding:15px; margin-bottom:12px; 
              border-left:5px solid var(--gold); box-shadow:var(--shadow); display:flex; justify-content:space-between; align-items:center}
  .sched-info{flex:1}
  .sched-ch{font-size:0.7rem; font-weight:800; color:var(--gold); text-transform:uppercase}
  .sched-time{font-size:1.1rem; font-weight:700; color:var(--text)}
  .sched-days{font-size:0.75rem; color:var(--muted); font-weight:600}

  /* ── TERMINAL ── */
  .terminal-container{margin:30px 24px; background:#1E1E1E; border-radius:15px; overflow:hidden;
                      box-shadow:0 15px 40px rgba(0,0,0,0.2); border:1px solid #333}
  .term-header{background:#2D2D2D; padding:10px 15px; display:flex; align-items:center; gap:8px}
  .term-dot{width:10px; height:10px; border-radius:50%}
  .term-header span{color:#AAA; font-size:0.75rem; font-weight:600; font-family:monospace}
  #terminal{height:150px; overflow-y:auto; padding:15px; font-family:'Courier New', monospace;
            font-size:0.85rem; line-height:1.4; color:#00FF00}
  .term-line{margin-bottom:4px}
  .term-line.cmd{color:#00AAFF}
  .term-line.err{color:#FF5555}

  /* ── TOAST ── */
  #toast{position:fixed; bottom:30px; left:50%; transform:translateX(-50%);
         background:var(--text); color:var(--white); padding:12px 25px; border-radius:30px;
         font-size:0.9rem; font-weight:600; opacity:0; transition:0.4s; z-index:9999}
  #toast.show{opacity:1; bottom:50px}

  @media(max-width:480px){
    .relay-grid{grid-template-columns:1fr 1fr}
    .tabs .tab{padding:10px 12px; font-size:.8rem}
  }
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
      <span>Metza<b>bok</b></span>
      <div class="subtitle">Univ. Aut&oacute;noma Chapingo</div>
    </div>
  </div>
  <span id="status-badge">ONLINE</span>
</nav>

<div class="mode-selector">
  <div class="mode-label">
    MODO <span id="mode-text">MANUAL</span>
    <small id="mode-desc">Control directo de relevadores</small>
  </div>
  <label class="switch">
    <input type="checkbox" id="mode-switch" onchange="toggleMode()">
    <span class="slider"></span>
  </label>
</div>

<div class="tabs">
  <div class="tab active" onclick="switchTab('control',this)"><span>&#9889;</span> Panel</div>
  <div class="tab" onclick="switchTab('term',this)"><span>&#62;_</span> Consola</div>
</div>

<div class="section active" id="sec-control">
  <!-- Grid Manual -->
  <div id="manual-ui" class="relay-grid"></div>
  
  <!-- Lista Auto -->
  <div id="auto-ui" style="display:none">
    <div style="font-weight:700; color:var(--muted); font-size:0.8rem; margin-bottom:15px; text-transform:uppercase">Horarios Activos</div>
    <div id="sched-container"></div>
  </div>
</div>

<div class="section" id="sec-term">
  <div class="terminal-container">
    <div class="term-header">
      <div class="term-dot" style="background:#FF5F56"></div>
      <div class="term-dot" style="background:#FFBD2E"></div>
      <div class="term-dot" style="background:#27C93F"></div>
      <span>LORA COMMAND TERMINAL</span>
    </div>
    <div id="terminal"></div>
  </div>
</div>

<div id="toast"></div>

<script>
let relayNames = ['RELAYNAMES'];
let relays = [false,false,false,false];
let autoMode = false;
let pending = [false,false,false,false];
let schedules = [];

function log(msg, type=''){
  const t = document.getElementById('terminal');
  const line = document.createElement('div');
  line.className = 'term-line ' + type;
  line.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  t.appendChild(line);
  t.scrollTop = t.scrollHeight;
}

function renderUI(){
  // Update Mode Switch
  document.getElementById('mode-switch').checked = autoMode;
  document.getElementById('mode-text').textContent = autoMode ? 'AUTOMÁTICO' : 'MANUAL';
  document.getElementById('mode-desc').textContent = autoMode ? 'Basado en horarios programados' : 'Control directo de relevadores';
  
  document.getElementById('manual-ui').style.display = autoMode ? 'none' : 'grid';
  document.getElementById('auto-ui').style.display = autoMode ? 'block' : 'none';

  if(!autoMode){
    const g = document.getElementById('manual-ui');
    g.innerHTML = '';
    for(let i=0; i<4; i++){
      const on = relays[i];
      const isPending = pending[i];
      g.innerHTML += `
        <div class="relay-card ${isPending?'pending':''}">
          <span class="ch-num">CH${i+1}</span>
          <span class="relay-icon" style="color:${on?'var(--gold)':'var(--silver)'}">${on?'&#128161;':'&#9898;'}</span>
          <span class="relay-name">${relayNames[i]}</span>
          <button class="toggle-btn ${on?'btn-on':'btn-off'}" onclick="toggleRelay(${i})">
            ${isPending ? '...' : (on?'ON':'OFF')}
          </button>
        </div>`;
    }
  } else {
    const sCont = document.getElementById('sched-container');
    sCont.innerHTML = schedules.length ? '' : '<div style="text-align:center; padding:40px; color:#AAA">No hay horarios programados</div>';
    const DAYS = ['Dom','Lun','Mar','Mié','Jue','Vie','Sáb'];
    schedules.forEach(s => {
      let dStr = '';
      for(let i=0;i<7;i++) if(s.mask & (1<<i)) dStr += DAYS[i] + ' ';
      sCont.innerHTML += `
        <div class="sched-card">
          <div class="sched-info">
            <div class="sched-ch">Canal ${s.ch} - ${relayNames[s.ch-1]}</div>
            <div class="sched-time">${String(s.onH).padStart(2,'0')}:${String(s.onM).padStart(2,'0')} - ${String(s.offH).padStart(2,'0')}:${String(s.offM).padStart(2,'0')}</div>
            <div class="sched-days">${dStr}</div>
          </div>
          <div class="status-badge" style="background:var(--gold-soft); color:var(--gold); border-radius:10px; padding:5px 10px; font-size:0.7rem; font-weight:800">ACTIVO</div>
        </div>
      `;
    });
  }
}

async function toggleRelay(i){
  const action = relays[i] ? '0' : '1';
  pending[i] = true;
  renderUI();
  log(`Enviando R${i+1}${action}...`, 'cmd');
  try {
    const r = await fetch(`/r/${i+1}/${action}`);
    if(!r.ok) throw new Error();
  } catch(e) {
    pending[i] = false;
    toast('Error de conexión', 'var(--red)');
    renderUI();
  }
}

async function toggleMode(){
  autoMode = document.getElementById('mode-switch').checked;
  const target = autoMode ? 'auto' : 'manual';
  log(`Cambiando a modo ${target}...`, 'cmd');
  try {
    await fetch(`/mode/${target}`);
    renderUI();
  } catch(e) {
    autoMode = !autoMode;
    renderUI();
    toast('Fallo al cambiar modo', 'var(--red)');
  }
}

async function poll(){
  try {
    const r = await fetch('/api/status');
    const d = await r.json();
    
    let changed = false;
    if(autoMode !== d.auto) { autoMode = d.auto; changed = true; }
    
    for(let i=0; i<4; i++){
      if(relays[i] !== d.relays[i]) { relays[i] = d.relays[i]; changed = true; }
      if(pending[i]) { pending[i] = false; changed = true; log(`Confirmado: CH${i+1}=${relays[i]?'ON':'OFF'}`); }
    }
    
    // Sync schedules
    if(JSON.stringify(schedules) !== JSON.stringify(d.schedules)){
      schedules = d.schedules;
      changed = true;
    }

    if(changed) renderUI();
  } catch(e) {}
}

function switchTab(id, el){
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.querySelectorAll('.section').forEach(s=>s.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('sec-'+id).classList.add('active');
}

function toast(msg, color='var(--gold)'){
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.style.background = color === 'var(--gold)' ? '#2D2D2D' : color;
  t.classList.add('show');
  setTimeout(()=>t.classList.remove('show'), 2500);
}

renderUI();
setInterval(poll, 3000);
poll(); // Initial sync
log('Sistema iniciado. Monitoreando...');
</script>
</body>
</html>
)rawliteral";

// ─── API JSON de estado ────────────────────────────────────────────────────
static void sendStatusJson() {
  String json = "{\"relays\":[";
  for (int i = 0; i < 4; i++) {
    json += (relayStatus[i] ? "true" : "false");
    if (i < 3) json += ",";
  }
  json += "],\"auto\":";
  json += autoMode ? "true" : "false";
  json += ",\"schedules\":[";

  bool first = true;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < MAX_SCHEDS; j++) {
      if (channelSchedules[i][j].enabled) {
        if (!first) json += ",";
        first = false;
        json += "{\"ch\":" + String(i + 1);
        json += ",\"mask\":"  + String(channelSchedules[i][j].daysMask);
        json += ",\"onH\":"   + String(channelSchedules[i][j].onHour);
        json += ",\"onM\":"   + String(channelSchedules[i][j].onMinute);
        json += ",\"offH\":"  + String(channelSchedules[i][j].offHour);
        json += ",\"offM\":"  + String(channelSchedules[i][j].offMinute);
        json += "}";
      }
    }
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// ─── Web Server del Slave ───
void setupWebServer() {
  server.on("/", []() {
    String nameList = "";
    for (int i = 0; i < 4; i++) {
      nameList += "'" + relayNames[i] + "'";
      if (i < 3) nameList += ",";
    }
    String page = String(HTML_TEMPLATE);
    page.replace("'RELAYNAMES'", nameList);
    server.send(200, "text/html", page);
  });

  server.on("/api/status", sendStatusJson);

  for (int i = 0; i < 4; i++) {
    String pathOn  = "/r/" + String(i + 1) + "/1";
    String pathOff = "/r/" + String(i + 1) + "/0";
    server.on(pathOn.c_str(), [i]() {
      if (!autoMode) {
        digitalWrite(RELAY_PINS[i], HIGH);
        relayStatus[i] = true;
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400, "text/plain", "ERR:AUTO");
      }
    });
    server.on(pathOff.c_str(), [i]() {
      if (!autoMode) {
        digitalWrite(RELAY_PINS[i], LOW);
        relayStatus[i] = false;
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400, "text/plain", "ERR:AUTO");
      }
    });
  }

  server.on("/mode/auto", []() {
    autoMode = true; saveMode();
    for (int i = 0; i < 4; i++) {
      digitalWrite(RELAY_PINS[i], LOW);
      relayStatus[i] = false;
      lastSchedState[i] = false;
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/mode/manual", []() {
    autoMode = false; saveMode();
    server.send(200, "text/plain", "OK");
  });
  server.on("/alloff", []() {
    if (autoMode) {
      server.send(400, "text/plain", "ERR:AUTO");
      return;
    }
    for (int i = 0; i < 4; i++) {
      digitalWrite(RELAY_PINS[i], LOW);
      relayStatus[i] = false;
    }
    server.send(200, "text/plain", "OK");
  });
  server.begin();
}

// ─── Procesador de comandos (Bluetooth O LoRa) ───
void processCommand(String cmd) {
  cmd.trim();
  Serial.println("CMD: " + cmd);
  lastBTActivity = millis();
  char replBuf[64];

  // ── Protocolo Corto LoRa ──
  if (cmd.length() == 1) {
    if (cmd == "A") { autoMode = true; saveMode(); reply("ACK:A"); return; }
    if (cmd == "M") { autoMode = false; saveMode(); reply("ACK:M"); return; }
  }

  if (cmd.startsWith("B:") && cmd.length() == 6) {
    if (autoMode) { reply("ERR:AUTO"); return; }
    for (int i = 0; i < 4; i++) {
      int st = cmd[i + 2] - '0';
      digitalWrite(RELAY_PINS[i], st ? HIGH : LOW);
      relayStatus[i] = (st == 1);
    }
    reply("ACK:B");
    return;
  }

  if (cmd == "ALLOFF") {
    if (autoMode) { reply("ERR:AUTO"); return; }
    for (int i = 0; i < 4; i++) {
      digitalWrite(RELAY_PINS[i], LOW);
      relayStatus[i] = false;
    }
    reply("ACK:ALLOFF");
    return;
  }

  if (cmd == "GA" || cmd == "A") { 
    autoMode = true; saveMode(); 
    for (int i = 0; i < 4; i++) {
      digitalWrite(RELAY_PINS[i], LOW);
      relayStatus[i] = false;
      lastSchedState[i] = false;
    }
    reply("ACK:A"); 
    return; 
  }
  if (cmd == "GM" || cmd == "M") { autoMode = false; saveMode(); reply("ACK:M"); return; }

  if (cmd == "S?" || cmd == "STATUS") {
    snprintf(replBuf, sizeof(replBuf), "S:%d%d%d%d:%c", 
             relayStatus[0], relayStatus[1], relayStatus[2], relayStatus[3], 
             autoMode ? 'A' : 'M');
    reply(replBuf);
    return;
  }

  // ── Protocolo Extendido ──
  for (int i = 0; i < 4; i++) {
    if (cmd == "ON" + String(i+1)) {
      if (autoMode) { reply("ERR:AUTO"); return; }
      digitalWrite(RELAY_PINS[i], HIGH); relayStatus[i] = true;
      reply(("CH" + String(i+1) + "=ON").c_str()); return;
    }
    if (cmd == "OFF" + String(i+1)) {
      if (autoMode) { reply("ERR:AUTO"); return; }
      digitalWrite(RELAY_PINS[i], LOW); relayStatus[i] = false;
      reply(("CH" + String(i+1) + "=OFF").c_str()); return;
    }
  }

  if (cmd == "GLOBAL_AUTO") { 
    autoMode = true; saveMode(); 
    for (int i = 0; i < 4; i++) {
      digitalWrite(RELAY_PINS[i], LOW);
      relayStatus[i] = false;
      lastSchedState[i] = false;
    }
    reply("MODE:GLOBAL:AUTO"); 
    return; 
  }
  if (cmd == "GLOBAL_MANUAL") { autoMode = false; saveMode(); reply("MODE:GLOBAL:MAN"); return; }


  if (cmd == "GETSCHEDS") {
    char sb[64];
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < MAX_SCHEDS; j++) {
        if (channelSchedules[i][j].enabled) {
          snprintf(sb, sizeof(sb), "LSCHED:%d:%d:%d:%d:%d:%d:%d", 
                   i + 1, j, 
                   channelSchedules[i][j].daysMask,
                   channelSchedules[i][j].onHour, channelSchedules[i][j].onMinute,
                   channelSchedules[i][j].offHour, channelSchedules[i][j].offMinute);
          reply(sb);
          delay(50); // Pequeño delay para estabilidad (BT y LoRa)
        }
      }
    }
    reply("SYNC_DONE");
    return;
  }

  if (cmd.startsWith("SETSCHED:") || cmd.startsWith("SC:")) {
    int p[8]; int count = 0, pos = 0;
    while (pos != -1 && count < 8) {
      int next = cmd.indexOf(':', pos);
      p[count++] = (next == -1) ? cmd.substring(pos).toInt() : cmd.substring(pos, next).toInt();
      pos = (next == -1) ? -1 : next + 1;
    }
    if (count == 8) {
      // Si el comando es SC:, asumimos índices 0-basados en el comando corto o ajustamos según se necesite.
      // Pero para compatibilidad asumiremos que canal viene 1-4, igual que SETSCHED.
      int ch = p[1] - 1, idx = p[2];
      if (ch >= 0 && ch < 4 && idx >= 0 && idx < MAX_SCHEDS) {
        channelSchedules[ch][idx].onHour = (uint8_t)p[4];
        channelSchedules[ch][idx].onMinute = (uint8_t)p[5];
        channelSchedules[ch][idx].offHour = (uint8_t)p[6];
        channelSchedules[ch][idx].offMinute = (uint8_t)p[7];
        channelSchedules[ch][idx].daysMask = (uint8_t)p[3];
        channelSchedules[ch][idx].enabled = true;
        
        Serial.printf("SAVING SCHED: CH=%d, IDX=%d, MASK=%d, ON=%02d:%02d, OFF=%02d:%02d\n", 
                      ch+1, idx, p[3], p[4], p[5], p[6], p[7]);
        
        saveSchedules(); reply("SCHED_SAVED:OK");
      }
    }
    return;
  }
  
  if (cmd.startsWith("DIS_SCHED:")) {
    int f = cmd.indexOf(':'), s = cmd.indexOf(':', f+1);
    if (s != -1) {
      int ch = cmd.substring(f+1, s).toInt() - 1;
      int idx = cmd.substring(s+1).toInt();
      if (ch >= 0 && ch < 4 && idx >= 0 && idx < MAX_SCHEDS) {
        channelSchedules[ch][idx].enabled = false;
        saveSchedules(); reply("SCHED_DISABLED:OK");
      }
    }
    return;
  }

  if (cmd.startsWith("SETTIME:")) {
    int p[6]; int count = 0, pos = 8; // "SETTIME:" has 8 chars
    while (pos != -1 && count < 6) {
      int next = cmd.indexOf(':', pos);
      p[count++] = (next == -1) ? cmd.substring(pos).toInt() : cmd.substring(pos, next).toInt();
      pos = (next == -1) ? -1 : next + 1;
    }
    if (count == 6) {
      rtc.adjust(DateTime(p[5], p[4], p[3], p[0], p[1], p[2]));
      reply("TIME_SYNC_OK");
    }
    return;
  }

  if (cmd.startsWith("CLEAR_SCHEDS:")) {
    int ch = cmd.substring(cmd.indexOf(':') + 1).toInt() - 1;
    if (ch >= 0 && ch < 4) {
      for (int j = 0; j < MAX_SCHEDS; j++) channelSchedules[ch][j].enabled = false;
      saveSchedules(); reply("SCHEDS_CLEARED:OK");
    }
    return;
  }

  if (cmd.startsWith("SETWIFI:")) {
    int f = cmd.indexOf(':'), s = cmd.indexOf(':', f+1);
    if (s != -1) {
      String ssid = cmd.substring(f+1, s); String pass = cmd.substring(s+1);
      WiFi.begin(ssid.c_str(), pass.c_str());
      currentWiFiStatus = WIFI_CONNECTING; wifiConnectStart = millis();
      reply("WIFI_TRYING");
    }
    return;
  }
  
  if (cmd.startsWith("SETNAME:")) {
    int f = cmd.indexOf(':'), s = cmd.indexOf(':', f+1);
    if (s != -1) {
      int ch = cmd.substring(f+1, s).toInt() - 1;
      String name = cmd.substring(s+1);
      if (ch >= 0 && ch < 4) {
        relayNames[ch] = name; saveRelayNames();
        reply("NAME OK");
      }
    }
    return;
  }
}

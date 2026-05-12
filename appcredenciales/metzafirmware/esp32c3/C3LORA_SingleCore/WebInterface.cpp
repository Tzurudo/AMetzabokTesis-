#include "WebInterface.h"
#include "LoRaManager.h"

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=yes, viewport-fit=cover">
    <title>Metzabok Gateway Pro</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Montserrat:wght@400;600;700;800&display=swap');
        :root {
            --gold: #C5A048; --gold-dark: #8E6D28; --silver: #E0E0E0; --white: #FFFFFF;
            --black: #0F1419; --bg: #F8F9FA; --text-main: #2C3E50; --text-light: #7F8C8D;
            --danger: #C0392B; --success: #27AE60; --card-shadow: 0 10px 30px rgba(0,0,0,0.05);
        }
        * { margin: 0; padding: 0; box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
        body { background-color: var(--bg); font-family: 'Montserrat', sans-serif; color: var(--text-main); padding-bottom: 50px; }
        .header { background: var(--white); padding: 30px 20px; border-bottom: 4px solid var(--gold); text-align: center; }
        .header h1 { font-size: 1.8rem; font-weight: 800; letter-spacing: 2px; color: var(--black); text-transform: uppercase; }
        .system-bar { display: flex; justify-content: space-between; padding: 12px 20px; background: #F1F3F5; font-size: 0.65rem; font-weight: 700; color: var(--text-light); }
        .content { padding: 20px; max-width: 600px; margin: 0 auto; }
        .card { background: var(--white); border-radius: 24px; padding: 24px; margin-bottom: 20px; box-shadow: var(--card-shadow); }
        .card-title { font-size: 0.7rem; font-weight: 800; color: var(--gold-dark); text-transform: uppercase; letter-spacing: 2px; margin-bottom: 18px; display: flex; justify-content: space-between; }
        
        .mode-card { display: flex; gap: 10px; margin-bottom: 25px; }
        .mode-btn { flex: 1; padding: 18px; border-radius: 15px; border: 2px solid var(--silver); background: var(--white); font-weight: 800; cursor: pointer; transition: 0.3s; }
        .mode-btn.active { background: var(--gold); border-color: var(--gold); color: var(--white); box-shadow: 0 5px 15px rgba(197, 160, 72, 0.3); }

        /* Conditional Sections */
        #manualSection, #autoSection { display: none; }
        .show { display: block !important; }

        /* Manual UI */
        .output-row { display: flex; align-items: center; justify-content: space-between; padding: 15px 0; border-bottom: 1px solid #F1F3F5; }
        .output-row:last-child { border-bottom: none; }
        .output-name { font-weight: 700; font-size: 0.9rem; }
        .toggle { position: relative; width: 50px; height: 26px; }
        .toggle input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #E9ECEF; border-radius: 34px; transition: 0.3s; border: 1px solid var(--silver); }
        .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 2px; bottom: 2px; background-color: white; border-radius: 50%; transition: 0.3s; }
        input:checked + .slider { background-color: var(--gold); }
        input:checked + .slider:before { transform: translateX(24px); }

        /* Auto UI - JComboBox style */
        .auto-output-box { border: 1px solid var(--silver); border-radius: 18px; padding: 18px; margin-bottom: 15px; }
        .output-label { font-size: 0.75rem; font-weight: 800; color: var(--text-main); margin-bottom: 10px; display: block; }
        select.sched-select { width: 100%; padding: 12px; border-radius: 10px; border: 1px solid var(--silver); font-family: inherit; font-weight: 600; font-size: 0.85rem; background: #fff; margin-bottom: 10px; }
        .sched-actions { display: flex; gap: 8px; }
        .btn-small { flex: 1; padding: 10px; border-radius: 10px; border: none; font-weight: 700; font-size: 0.65rem; cursor: pointer; text-transform: uppercase; }
        .btn-edit { background: var(--gold); color: white; }
        .btn-delete { background: #fbe9e7; color: var(--danger); }
        .btn-add { background: #E9ECEF; color: var(--text-main); width: 100%; margin-top: 5px; }

        /* General Buttons */
        .btn-main { width: 100%; padding: 18px; border-radius: 15px; border: none; font-weight: 800; text-transform: uppercase; letter-spacing: 1.5px; cursor: pointer; margin-top: 10px; }
        .btn-gold { background: var(--gold); color: white; }
        .btn-danger { background: var(--danger); color: white; }

        /* Overlay & Modals */
        .overlay { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(255,255,255,0.95); z-index: 9000; display: none; flex-direction: column; align-items: center; justify-content: center; }
        .spinner { width: 40px; height: 40px; border: 4px solid var(--silver); border-top-color: var(--gold); border-radius: 50%; animation: spin 1s linear infinite; }
        @keyframes spin { to { transform: rotate(360deg); } }
        .loader-text { margin-top: 15px; font-weight: 800; font-size: 0.7rem; color: var(--gold-dark); letter-spacing: 2px; }

        .modal-wrap { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); backdrop-filter: blur(4px); z-index: 5000; display: none; align-items: center; justify-content: center; }
        .modal { background: white; width: 90%; max-width: 400px; border-radius: 24px; padding: 30px; box-shadow: 0 20px 40px rgba(0,0,0,0.2); }
        .form-group { margin-bottom: 15px; }
        .form-group label { display: block; font-size: 0.65rem; font-weight: 800; color: var(--text-light); text-transform: uppercase; margin-bottom: 5px; }
        .form-group input, .form-group select { width: 100%; padding: 12px; border-radius: 10px; border: 1px solid var(--silver); font-family: inherit; font-weight: 600; }
        .day-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
        .day-opt { font-size: 0.6rem; font-weight: 800; display: flex; align-items: center; gap: 4px; }
        
        .tabs-mini { display: flex; gap: 10px; margin-top: 20px; }
        .tab-mini-btn { flex: 1; padding: 10px; font-size: 0.6rem; font-weight: 800; border: none; background: #eee; border-radius: 8px; cursor: pointer; color: #777; }
        .tab-mini-btn.active { background: var(--black); color: white; }

        .toast { position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%) translateY(100px); background: #333; color: white; padding: 10px 25px; border-radius: 50px; font-weight: 700; font-size: 0.75rem; transition: 0.3s; z-index: 10000; }
        .toast.active { transform: translateX(-50%) translateY(0); }
    </style>
</head>
<body>

<div id="loader" class="overlay">
    <div class="spinner"></div>
    <div class="loader-text">SINCRONIZANDO LORA</div>
</div>

<div class="header">
    <h1>METZABOK</h1>
</div>

<div class="system-bar">
    <span id="statusLabel">CONECTADO</span>
    <span id="syncLabel">--:--:--</span>
</div>

<div class="content">
    <div class="mode-card">
        <button id="modeM" class="mode-btn" onclick="setMode('M')">MANUAL</button>
        <button id="modeA" class="mode-btn" onclick="setMode('A')">AUTOMÁTICO</button>
    </div>

    <!-- Sección Manual -->
    <div id="manualSection">
        <div class="card">
            <div class="card-title">Control de Dispositivos</div>
            <div id="manualGrid"></div>
            <button class="btn-main btn-gold" onclick="applyManual()">APLICAR CAMBIOS</button>
        </div>
        <button class="btn-main btn-danger" onclick="emergencyStop()">PARO DE EMERGENCIA</button>
    </div>

    <!-- Sección Automática -->
    <div id="autoSection">
        <div class="card">
            <div class="card-title">Programación de Salidas</div>
            <div id="autoBoxes"></div>
        </div>
    </div>

    <!-- Sección Configuración (Nombres) -->
    <div id="configSection" style="display:none">
        <div class="card">
            <div class="card-title">Renombrar Salidas</div>
            <div id="nameInputs"></div>
            <button class="btn-main btn-gold" onclick="saveSettings()">GUARDAR AJUSTES</button>
        </div>
    </div>

    <div class="tabs-mini">
        <button id="tabMain" class="tab-mini-btn active" onclick="showSub('main')">PANEL PRINCIPAL</button>
        <button id="tabSettings" class="tab-mini-btn" onclick="showSub('config')">AJUSTES</button>
    </div>
</div>

<!-- Modal de Horario -->
<div id="schedModal" class="modal-wrap">
    <div class="modal">
        <div class="card-title" id="modalTitle">Nuevo Horario</div>
        <input type="hidden" id="editCh">
        <input type="hidden" id="editIdx" value="0">
        <div class="form-group">
            <label>Días</label>
            <div class="day-grid" id="dayPicker">
                <label class="day-opt"><input type="checkbox" value="1" checked> LUN</label>
                <label class="day-opt"><input type="checkbox" value="2" checked> MAR</label>
                <label class="day-opt"><input type="checkbox" value="3" checked> MIE</label>
                <label class="day-opt"><input type="checkbox" value="4" checked> JUE</label>
                <label class="day-opt"><input type="checkbox" value="5" checked> VIE</label>
                <label class="day-opt"><input type="checkbox" value="6" checked> SAB</label>
                <label class="day-opt"><input type="checkbox" value="0" checked> DOM</label>
            </div>
        </div>
        <div style="display:flex; gap:10px">
            <div class="form-group" style="flex:1">
                <label>Inicio</label>
                <input type="time" id="sOn" value="08:00">
            </div>
            <div class="form-group" style="flex:1">
                <label>Fin</label>
                <input type="time" id="sOff" value="18:00">
            </div>
        </div>
        <div style="display:flex; gap:10px; margin-top:10px">
            <button class="btn-main" style="background:#eee; color:#333" onclick="closeModal()">CANCELAR</button>
            <button class="btn-main btn-gold" onclick="submitSched()">GUARDAR</button>
        </div>
    </div>
</div>

<div id="toast" class="toast">MENSAJE</div>

<script>
    let relayNames = ['SALIDA 1', 'SALIDA 2', 'SALIDA 3', 'SALIDA 4'];
    let relayStates = [false, false, false, false];
    let autoMode = false;
    let schedules = [[], [], [], []]; // Agrupados por canal
    let isPending = false;

    function showToast(m) { const t=document.getElementById('toast'); t.textContent=m; t.classList.add('active'); setTimeout(()=>t.classList.remove('active'),2500); }
    function setLoader(v, t="SINCRONIZANDO LORA") { document.getElementById('loader').style.display = v?'flex':'none'; document.querySelector('.loader-text').textContent=t; }
    
    function showSub(s) {
        document.getElementById('configSection').style.display = (s==='config'?'block':'none');
        document.getElementById('tabMain').classList.toggle('active', s==='main');
        document.getElementById('tabSettings').classList.toggle('active', s==='config');
        updateVisibility();
    }

    function updateVisibility() {
        const isConfig = document.getElementById('configSection').style.display === 'block';
        if(isConfig) {
            document.getElementById('manualSection').classList.remove('show');
            document.getElementById('autoSection').classList.remove('show');
        } else {
            document.getElementById('manualSection').classList.toggle('show', !autoMode);
            document.getElementById('autoSection').classList.toggle('show', autoMode);
        }
        document.getElementById('modeM').classList.toggle('active', !autoMode);
        document.getElementById('modeA').classList.toggle('active', autoMode);
    }

    async function call(q) { try { const r=await fetch(`/cmd?q=${encodeURIComponent(q)}`); return await r.json(); } catch(e) { return {status:'error'}; } }

    async function setMode(m) {
        setLoader(true, "CAMBIANDO MODO...");
        const d = await call(m);
        if(d.status === 'ok') {
            autoMode = (m === 'A');
            await syncAll();
            showToast("MODO ACTUALIZADO");
        }
        setLoader(false);
    }

    async function applyManual() {
        if(autoMode) return;
        setLoader(true, "TRANSMITIENDO...");
        let cmd = "B:";
        relayStates.forEach(s => cmd += (s?1:0));
        await call(cmd);
        await syncAll();
        setLoader(false);
        showToast("CAMBIOS APLICADOS");
    }

    async function emergencyStop() {
        setLoader(true, "PARO DE EMERGENCIA...");
        await call("ALLOFF");
        await call("M");
        autoMode = false;
        await syncAll();
        setLoader(false);
        showToast("PARO DE EMERGENCIA");
    }

    async function syncAll() {
        const d = await call("S?");
        if(d.status === 'ok' && d.message.startsWith('S:')) {
            const p = d.message.split(':');
            const states = p[1];
            autoMode = (p[2] === 'A');
            for(let i=0; i<4; i++) relayStates[i] = (states[i]==='1');
        }
        if(autoMode) {
            const d2 = await call("GETSCHEDS");
            schedules = [[], [], [], []];
            if(d2.message) {
                d2.message.split('\\n').forEach(l => {
                    if(l.startsWith("LSCHED:")) {
                        const p = l.split(':');
                        const ch = parseInt(p[1]) - 1;
                        schedules[ch].push({ idx:p[2], mask:p[3], onH:p[4], onM:p[5], offH:p[6], offM:p[7] });
                    }
                });
            }
        }
        document.getElementById('syncLabel').textContent = new Date().toLocaleTimeString();
        renderAll();
        updateVisibility();
    }

    function renderAll() {
        // Manual
        const mGrid = document.getElementById('manualGrid');
        mGrid.innerHTML = '';
        relayNames.forEach((n, i) => {
            mGrid.innerHTML += `<div class="output-row">
                <span class="output-name">${n}</span>
                <label class="toggle"><input type="checkbox" ${relayStates[i]?'checked':''} onchange="relayStates[${i}]=this.checked">
                <span class="slider"></span></label>
            </div>`;
        });

        // Auto (ComboBox style)
        const aBoxes = document.getElementById('autoBoxes');
        aBoxes.innerHTML = '';
        relayNames.forEach((n, i) => {
            let options = schedules[i].length ? '' : '<option value="">SIN HORARIOS</option>';
            schedules[i].forEach((s, idx) => {
                options += `<option value="${idx}">${s.onH}:${s.onM} - ${s.offH}:${s.offM}</option>`;
            });

            aBoxes.innerHTML += `
                <div class="auto-output-box">
                    <span class="output-label">${n}</span>
                    <select class="sched-select" id="sel-${i}">${options}</select>
                    <div class="sched-actions">
                        <button class="btn-small btn-edit" onclick="editSched(${i})">MODIFICAR</button>
                        <button class="btn-small btn-delete" onclick="delSched(${i})">BORRAR</button>
                    </div>
                    <button class="btn-small btn-add" onclick="openAdd(${i})">+ AGREGAR</button>
                </div>
            `;
        });

        // Config
        const nInputs = document.getElementById('nameInputs');
        nInputs.innerHTML = '';
        relayNames.forEach((n, i) => {
            nInputs.innerHTML += `<div class="form-group"><label>SALIDA ${i+1}</label>
                <input type="text" id="nn-${i}" value="${n}"></div>`;
        });
    }

    function openAdd(ch) {
        document.getElementById('editCh').value = ch + 1;
        document.getElementById('editIdx').value = 0; // Se usará 0 o un slot libre en el esclavo
        document.getElementById('modalTitle').textContent = "AGREGAR A " + relayNames[ch];
        document.getElementById('schedModal').style.display = 'flex';
    }

    function editSched(ch) {
        const sel = document.getElementById('sel-' + ch);
        if(!sel.value || schedules[ch].length === 0) return;
        const s = schedules[ch][sel.value];
        document.getElementById('editCh').value = ch + 1;
        document.getElementById('editIdx').value = s.idx;
        document.getElementById('sOn').value = `${String(s.onH).padStart(2,'0')}:${String(s.onM).padStart(2,'0')}`;
        document.getElementById('sOff').value = `${String(s.offH).padStart(2,'0')}:${String(s.offM).padStart(2,'0')}`;
        document.getElementById('modalTitle').textContent = "EDITAR EN " + relayNames[ch];
        document.getElementById('schedModal').style.display = 'flex';
    }

    async function delSched(ch) {
        const sel = document.getElementById('sel-' + ch);
        if(!sel.value || schedules[ch].length === 0) return;
        const s = schedules[ch][sel.value];
        setLoader(true, "BORRANDO...");
        await call(`DIS_SCHED:${ch+1}:${s.idx}`);
        await syncAll();
        setLoader(false);
        showToast("HORARIO ELIMINADO");
    }

    async function submitSched() {
        const ch = document.getElementById('editCh').value;
        const idx = document.getElementById('editIdx').value;
        const on = document.getElementById('sOn').value.split(':');
        const off = document.getElementById('sOff').value.split(':');
        let mask = 0;
        document.querySelectorAll('#dayPicker input:checked').forEach(i => mask |= (1 << parseInt(i.value)));
        
        closeModal();
        setLoader(true, "GUARDANDO...");
        await call(`SC:${ch}:${idx}:${mask}:${on[0]}:${on[1]}:${off[0]}:${off[1]}`);
        await syncAll();
        setLoader(false);
        showToast("PROGRAMACIÓN GUARDADA");
    }

    function closeModal() { document.getElementById('schedModal').style.display = 'none'; }

    async function saveSettings() {
        const names = [];
        for(let i=0; i<4; i++) names.push(document.getElementById('nn-' + i).value.toUpperCase());
        setLoader(true, "GUARDANDO...");
        const r = await fetch('/config', { method:'POST', body:JSON.stringify({names:names}) });
        if(r.ok) { relayNames = names; showToast("AJUSTES GUARDADOS"); renderAll(); }
        setLoader(false);
    }

    async function init() {
        try {
            const r = await fetch('/names');
            const d = await r.json();
            if(d.names) relayNames = d.names;
        } catch(e) {}
        await syncAll();
        setInterval(syncAll, 30000);
    }

    init();
</script>
</body>
</html>
)rawliteral";

void setupWebServer() {
  server.on("/", []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/status", []() {
    String json = "{\"uptime\":" + String(millis()) + "}";
    server.send(200, "application/json", json);
  });

  server.on("/cmd", []() {
    if (!server.hasArg("q")) {
      server.send(400, "application/json", "{\"error\":\"Missing q\"}");
      return;
    }
    String q = server.arg("q");
    q.trim();
    q.toUpperCase();

    if (sendLoRaToSlave(q.c_str())) {
      lastLoRaResponse = "";
      unsigned long start = millis();
      String accumulated = "";
      bool isMulti = q.startsWith("GETSCHEDS");
      unsigned long timeout = isMulti ? 5000 : 2500;
      
      while (millis() - start < timeout) {
        receiveLoRa();
        if (lastLoRaResponse.length() > 0) {
          accumulated += lastLoRaResponse;
          if (isMulti) {
            accumulated += "\\n";
            if (lastLoRaResponse == "SYNC_DONE") break;
            lastLoRaResponse = "";
          } else {
            break;
          }
        }
        delay(10);
      }
      String msg = (accumulated.length() > 0) ? accumulated : "Comando enviado";
      server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"" + msg + "\"}");
    } else {
      server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"LoRa TX failed\"}");
    }
  });

  server.on("/names", HTTP_GET, []() {
    String json = "{\"names\":[";
    for(int i = 0; i < NUM_RELAYS; i++) {
      json += "\"" + relayNames[i] + "\"";
      if(i < NUM_RELAYS - 1) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  server.on("/config", HTTP_POST, []() {
    if(!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data\"}");
      return;
    }
    
    String body = server.arg("plain");
    int start = body.indexOf("[");
    int end = body.lastIndexOf("]");
    
    if(start != -1 && end != -1) {
      String ns = body.substring(start + 1, end);
      int idx = 0;
      int nameIdx = 0;
      while(nameIdx < NUM_RELAYS && idx < ns.length()) {
        int q1 = ns.indexOf('"', idx);
        if(q1 == -1) break;
        int q2 = ns.indexOf('"', q1 + 1);
        if(q2 == -1) break;
        String name = ns.substring(q1 + 1, q2);
        if(name.length() > 0) setRelayName(nameIdx, name);
        nameIdx++; idx = q2 + 1;
      }
      saveRelayNames();
      server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuración guardada\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"JSON inválido\"}");
    }
  });

  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  server.begin();
  Serial.println("WebServer listo en puerto 80");
}

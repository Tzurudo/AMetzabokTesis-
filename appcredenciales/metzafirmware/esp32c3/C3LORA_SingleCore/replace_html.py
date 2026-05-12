import re

with open("WebInterface.cpp", "r") as f:
    content = f.read()

new_html = r"""<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Metzabok - Gateway Premium</title>
<link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;500;600;700&display=swap" rel="stylesheet">
<style>
  :root{
    --gold:#D4AF37; --gold-dark:#A88000;
    --white:#FFFFFF; --bg-light:#F0F4F8; --bg:#E8EEF5;
    --text:#1A2B4E; --text-light:#5A6B7F; --green:#2ECC71; --red:#E74C3C;
    --blue:#3498DB; 
    --shadow:0 8px 24px rgba(0,0,0,0.1);
    --shadow-sm:0 2px 8px rgba(0,0,0,0.08);
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{
    font-family:'Poppins',sans-serif;
    background:linear-gradient(135deg, var(--bg-light) 0%, var(--bg) 100%);
    color:var(--text);
    min-height:100vh;
    padding:0 0 50px;
  }
  @keyframes fadeIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
  
  .header{
    background:linear-gradient(135deg, var(--text) 0%, #1A3A5C 100%);
    padding:24px 16px;
    text-align:center;
    box-shadow:var(--shadow);
    border-bottom:4px solid var(--gold);
  }
  .header h1{color:var(--white);font-size:1.8rem;letter-spacing:2px;text-shadow:2px 2px 4px rgba(0,0,0,0.2);}
  .header p{color:var(--gold);font-size:0.8rem;letter-spacing:1px;margin-top:4px;font-weight:600;}

  .mode-container {
    display:flex; gap:12px; margin:20px 16px; animation:fadeIn 0.4s ease-out;
  }
  .mode-btn {
    flex:1; padding:16px 8px; border-radius:16px; font-weight:700; font-size:1rem; 
    border:none; cursor:pointer; transition:all 0.3s; color:white; box-shadow:var(--shadow-sm);
  }
  .mode-btn.active.manual { background:var(--green); box-shadow:0 6px 16px rgba(46,204,113,0.3); transform:scale(1.02); }
  .mode-btn.active.auto { background:var(--green); box-shadow:0 6px 16px rgba(46,204,113,0.3); transform:scale(1.02); }
  .mode-btn.inactive { background:var(--red); opacity:0.85; }

  .relay-list {
    display:flex; flex-direction:column; gap:12px; margin:0 16px; animation:fadeIn 0.5s ease-out;
  }
  .relay-item {
    background:var(--white); border-radius:16px; padding:18px 20px; 
    display:flex; align-items:center; justify-content:space-between; 
    box-shadow:var(--shadow-sm); border-left:4px solid var(--gold);
    transition:transform 0.2s;
  }
  .relay-info { display:flex; flex-direction:column; gap:4px; }
  .ch-tag { font-size:0.65rem; font-weight:800; color:var(--gold); letter-spacing:1px; }
  .relay-name { font-weight:700; color:var(--text); font-size:1.05rem; }

  .channel-switch{ position:relative; display:inline-block; width:64px; height:34px; }
  .channel-switch input{ opacity:0; width:0; height:0; }
  .switch-slider{
    position:absolute; cursor:pointer; top:0; left:0; right:0; bottom:0;
    background-color:#E53935; transition:0.3s; border-radius:34px;
    box-shadow:inset 0 2px 4px rgba(0,0,0,0.1);
  }
  .switch-slider:before{
    position:absolute; content:""; height:26px; width:26px; left:4px; bottom:4px;
    background-color:white; transition:0.3s; border-radius:50%; box-shadow:0 2px 6px rgba(0,0,0,0.2);
  }
  input:checked + .switch-slider{ background-color:var(--green); }
  input:checked + .switch-slider:before{ transform:translateX(30px); }
  input:disabled + .switch-slider{ opacity:0.5; cursor:not-allowed; }

  .action-bar {
    display:flex; gap:12px; margin:24px 16px; animation:fadeIn 0.6s ease-out;
  }
  .btn {
    padding:16px 12px; border-radius:16px; border:none; font-family:inherit; 
    font-weight:700; font-size:0.85rem; cursor:pointer; transition:all 0.3s; 
    text-transform:uppercase; letter-spacing:1px; box-shadow:var(--shadow-sm);
  }
  .btn:active{transform:scale(0.96);}
  .btn-save { flex:2; background:linear-gradient(135deg, var(--blue), #0984e3); color:white; }
  .btn-off { flex:1; background:var(--red); color:white; }

  .console-box{ background:#0F1419; border-radius:16px; padding:16px; margin:24px 16px; box-shadow:var(--shadow); }
  #console{ height:120px; overflow-y:auto; font-family:monospace; font-size:0.8rem; color:#0F0; line-height:1.5; }
  .log-line{ margin-bottom:4px; animation:fadeIn 0.3s; }
  .log-cmd{color:#00AAFF;} .log-err{color:#FF6B6B;} .log-ok{color:#51CF66;}

  .config-btn{
    position:fixed; bottom:20px; right:20px; width:50px; height:50px; border-radius:50%;
    background:var(--gold); color:white; border:none; font-size:1.5rem; cursor:pointer;
    box-shadow:var(--shadow); z-index:100;
  }
  .config-modal{
    display:none; position:fixed; top:0; left:0; width:100%; height:100%;
    background:rgba(0,0,0,0.6); z-index:1000; justify-content:center; align-items:center;
  }
  .config-modal.active{display:flex;}
  .config-content{
    background:var(--white); border-radius:24px; padding:24px; width:90%; max-width:400px;
  }
  .form-group{ display:flex; flex-direction:column; gap:6px; margin-bottom:12px; }
  .form-group input{ padding:10px; border:2px solid #ddd; border-radius:10px; font-family:inherit; }
  .btn-save-cfg{ background:var(--green); color:white; padding:12px; border:none; border-radius:10px; width:100%; font-weight:bold; margin-top:10px; cursor:pointer;}
  .btn-cancel-cfg{ background:#ddd; color:var(--text); padding:12px; border:none; border-radius:10px; width:100%; font-weight:bold; margin-top:10px; cursor:pointer;}
</style>
</head>
<body>

<div class="header">
  <h1>METZABOOK</h1>
  <p>Gateway de Control</p>
</div>

<div class="mode-container">
  <button id="btn-manual" class="mode-btn inactive manual" onclick="toggleMode('M')">MANUAL</button>
  <button id="btn-auto" class="mode-btn inactive auto" onclick="toggleMode('A')">AUTOMÁTICO</button>
</div>

<div class="relay-list" id="relayList"></div>

<div class="action-bar">
  <button class="btn btn-save" onclick="sendChanges()">ENVIAR CAMBIOS</button>
  <button class="btn btn-off" onclick="sendAllOff()">APAGAR TODOS</button>
</div>

<div class="console-box">
  <div id="console"></div>
  <div style="display:flex; gap:10px; margin-top:12px">
    <input type="text" id="cmdInput" placeholder="Comando libre..." style="flex:1; padding:8px; border-radius:8px; border:none; background:#1A2332; color:#0F0; outline:none">
    <button class="btn" style="background:#333;color:white;padding:8px 16px" onclick="sendCmdInput()">Send</button>
  </div>
</div>

<button class="config-btn" onclick="openConfigModal()">⚙️</button>
<div id="configModal" class="config-modal">
  <div class="config-content">
    <h3 style="margin-bottom:16px">Configurar Nombres</h3>
    <div class="form-group"><input type="text" id="name-0" maxlength="19"></div>
    <div class="form-group"><input type="text" id="name-1" maxlength="19"></div>
    <div class="form-group"><input type="text" id="name-2" maxlength="19"></div>
    <div class="form-group"><input type="text" id="name-3" maxlength="19"></div>
    <div style="display:flex;gap:10px">
      <button class="btn-save-cfg" onclick="saveConfig()">Guardar</button>
      <button class="btn-cancel-cfg" onclick="closeConfigModal()">Cancelar</button>
    </div>
  </div>
</div>

<script>
let relayNames = ['Salida 1', 'Salida 2', 'Salida 3', 'Salida 4'];
let relayOn = [false,false,false,false];
let relayPending = [false,false,false,false];
let autoMode = false;
let hasUnsavedChanges = false;

function log(msg, type=''){
  const c = document.getElementById('console');
  const line = document.createElement('div');
  line.className = 'log-line ' + type;
  line.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  c.insertBefore(line, c.firstChild);
}

function openConfigModal(){
  document.getElementById('configModal').classList.add('active');
  for(let i=0; i<4; i++) document.getElementById('name-'+i).value = relayNames[i];
}
function closeConfigModal(){
  document.getElementById('configModal').classList.remove('active');
}

async function saveConfig(){
  const names = [];
  for(let i=0; i<4; i++) names.push(document.getElementById('name-'+i).value);
  try {
    const r = await fetch('/config', { method:'POST', body:JSON.stringify({names}) });
    const d = await r.json();
    if(d.status === 'ok'){
      for(let i=0; i<4; i++) relayNames[i] = names[i] || ('Salida '+(i+1));
      renderUI();
      closeConfigModal();
      log('Config guardada', 'log-ok');
    }
  } catch(e) { log('Error guardando', 'log-err'); }
}

function markChanged(ch) {
  relayPending[ch] = document.getElementById('sw-'+ch).checked;
  hasUnsavedChanges = true;
}

function renderUI(){
  const btnM = document.getElementById('btn-manual');
  const btnA = document.getElementById('btn-auto');
  if(autoMode) {
    btnA.className = 'mode-btn active auto';
    btnM.className = 'mode-btn inactive manual';
  } else {
    btnM.className = 'mode-btn active manual';
    btnA.className = 'mode-btn inactive auto';
  }

  const list = document.getElementById('relayList');
  if(list.innerHTML === '') {
    let html = '';
    for(let i=0; i<4; i++){
      relayPending[i] = relayOn[i];
      html += `
        <div class="relay-item">
          <div class="relay-info">
            <span class="ch-tag">CANAL ${i+1}</span>
            <span class="relay-name" id="label-${i}">${relayNames[i]}</span>
          </div>
          <label class="channel-switch">
            <input type="checkbox" id="sw-${i}" ${relayPending[i]?'checked':''} onchange="markChanged(${i})">
            <span class="switch-slider"></span>
          </label>
        </div>`;
    }
    list.innerHTML = html;
  } else {
    for(let i=0; i<4; i++) {
      document.getElementById('label-'+i).textContent = relayNames[i];
      if(!hasUnsavedChanges) {
        relayPending[i] = relayOn[i];
        document.getElementById('sw-'+i).checked = relayOn[i];
      }
    }
  }
}

async function toggleMode(target){
  log(`Cambiando a: ${target==='A'?'Auto':'Manual'}`, 'log-cmd');
  try {
    await fetch(`/cmd?q=${target}`);
    hasUnsavedChanges = false;
  } catch(e) { log('Error cambiando modo','log-err'); }
}

async function sendChanges(){
  if(!hasUnsavedChanges) {
    log('No hay cambios pendientes', '');
    return;
  }
  log('Enviando cambios...', 'log-cmd');
  for(let i=0; i<4; i++){
    if(relayPending[i] !== relayOn[i]){
      try {
        await fetch(`/cmd?q=R${i+1}${relayPending[i]?1:0}`);
        relayOn[i] = relayPending[i];
      } catch(e) { log('Error en CH'+(i+1), 'log-err'); }
    }
  }
  hasUnsavedChanges = false;
  renderUI();
  log('Cambios aplicados', 'log-ok');
}

async function sendAllOff(){
  log('Apagando todos...', 'log-cmd');
  hasUnsavedChanges = false;
  for(let i=0; i<4; i++) relayPending[i] = false;
  renderUI();
  try {
    await fetch('/cmd?q=ALLOFF');
    for(let i=0; i<4; i++) relayOn[i] = false;
    renderUI();
  } catch(e) { log('Error en ALLOFF','log-err'); }
}

async function sendCmdInput(){
  const i = document.getElementById('cmdInput');
  const v = i.value.trim().toUpperCase();
  if(!v) return;
  i.value = '';
  log(`> ${v}`, 'log-cmd');
  try {
    const r = await fetch(`/cmd?q=${encodeURIComponent(v)}`);
    const d = await r.json();
    log(`${d.message}`);
  } catch(e) {}
}

async function syncStatus(){
  try {
    const r = await fetch('/cmd?q=S?');
    const d = await r.json();
    if(d.message && d.message.startsWith('S:')){
      const p = d.message.split(':');
      if(p.length >= 3){
        autoMode = (p[2] === 'A');
        for(let i=0; i<4; i++) relayOn[i] = (p[1][i] === '1');
        renderUI();
      }
    }
  } catch(e) {}
}

async function loadNames(){
  try{
    const r = await fetch('/names');
    const d = await r.json();
    if(d.names) relayNames = d.names;
  }catch(e){}
  setInterval(syncStatus, 4000);
  syncStatus();
}

loadNames();
</script>
</body>
</html>"""

new_content = re.sub(r'<!DOCTYPE html>.*?</html>', new_html, content, flags=re.DOTALL)

with open("WebInterface.cpp", "w") as f:
    f.write(new_content)

#include "WebInterface.h"
#include "LoRaManager.h"

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Metzabok - Gateway Premium</title>
<link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;500;600;700&display=swap" rel="stylesheet">
<style>
  :root{
    --gold:#D4AF37; --gold-light:#F7E6AD; --gold-dark:#A88000;
    --white:#FFFFFF; --bg-light:#F0F4F8; --bg:#E8EEF5;
    --text:#1A2B4E; --text-light:#5A6B7F; --green:#2ECC71; --red:#E74C3C;
    --blue:#3498DB; --purple:#9B59B6;
    --shadow:0 8px 24px rgba(0,0,0,0.1);
    --shadow-sm:0 2px 8px rgba(0,0,0,0.08);
  }
  
  *{box-sizing:border-box;margin:0;padding:0}
  
  html{scroll-behavior:smooth}
  body{
    font-family:'Poppins',sans-serif;
    background:linear-gradient(135deg, var(--bg-light) 0%, var(--bg) 100%);
    color:var(--text);
    min-height:100vh;
    padding:0 0 50px;
    position:relative;
  }
  
  /* Animaciones */
  @keyframes fadeIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
  @keyframes slideDown{from{opacity:0;transform:translateY(-20px)}to{opacity:1;transform:translateY(0)}}
  @keyframes spin{from{transform:rotate(0deg)}to{transform:rotate(360deg)}}
  
  /* Header */
  .header{
    background:linear-gradient(135deg, var(--text) 0%, #1A3A5C 100%);
    padding:32px 24px;
    text-align:center;
    box-shadow:var(--shadow);
    border-bottom:4px solid var(--gold);
    position:relative;
    overflow:hidden;
    animation:slideDown 0.6s ease-out;
  }
  
  .header::before{
    content:"";
    position:absolute;
    top:0;
    left:0;
    width:100%;
    height:100%;
    background:url('data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><circle cx="20" cy="20" r="1" fill="white" opacity="0.1"/><circle cx="80" cy="80" r="1" fill="white" opacity="0.1"/></svg>');
    opacity:0.3;
  }
  
  .header-content{position:relative;z-index:1}
  
  .header h1{
    color:var(--white);
    font-size:2rem;
    font-weight:700;
    letter-spacing:3px;
    margin:0;
    text-shadow:2px 2px 4px rgba(0,0,0,0.2);
  }
  
  .header p{
    color:var(--gold);
    font-size:0.85rem;
    letter-spacing:2px;
    margin-top:8px;
    text-transform:uppercase;
    font-weight:600;
  }

  /* Mode Switch Bar */
  .mode-bar{
    background:var(--white);
    margin:24px 16px;
    padding:22px;
    border-radius:24px;
    box-shadow:var(--shadow);
    display:flex;
    justify-content:space-between;
    align-items:center;
    border:2px solid var(--gold);
    animation:fadeIn 0.6s ease-out 0.2s both;
    transition:transform 0.3s ease, box-shadow 0.3s ease;
  }
  
  .mode-bar:hover{
    transform:translateY(-4px);
    box-shadow:0 12px 32px rgba(0,0,0,0.15);
  }
  
  .mode-info{flex:1}
  .mode-title{font-weight:700;font-size:1.15rem;display:block;color:var(--text)}
  .mode-desc{font-size:0.8rem;color:var(--text-light);font-weight:500;margin-top:4px}
  
  .switch{position:relative;display:inline-block;width:70px;height:38px}
  .switch input{opacity:0;width:0;height:0}
  .slider{
    position:absolute;
    cursor:pointer;
    top:0;
    left:0;
    right:0;
    bottom:0;
    background-color:#ddd;
    transition:0.4s;
    border-radius:38px;
    border:2px solid #ccc;
  }
  .slider:before{
    position:absolute;
    content:"";
    height:30px;
    width:30px;
    left:3px;
    bottom:2px;
    background-color:white;
    transition:0.4s;
    border-radius:50%;
    box-shadow:0 2px 8px rgba(0,0,0,0.2);
  }
  input:checked + .slider{
    background:linear-gradient(135deg, var(--gold) 0%, var(--gold-dark) 100%);
    border-color:var(--gold-dark);
  }
  input:checked + .slider:before{
    transform:translateX(32px);
  }

  /* Layout */
  .page{max-width:900px;margin:0 auto;padding:10px 16px}
  
  .section-title{
    font-size:0.75rem;
    font-weight:700;
    letter-spacing:2.5px;
    text-transform:uppercase;
    color:var(--gold);
    margin:28px 0 16px;
    border-left:5px solid var(--gold);
    padding-left:12px;
    animation:fadeIn 0.6s ease-out;
  }

  /* Manual UI: Grid */
  .relay-grid{
    display:grid;
    grid-template-columns:repeat(auto-fit,minmax(200px,1fr));
    gap:14px;
    animation:fadeIn 0.6s ease-out 0.3s both;
  }
  
  .relay-card{
    background:var(--white);
    border-radius:20px;
    padding:20px 18px;
    text-align:center;
    box-shadow:var(--shadow-sm);
    border:2px solid transparent;
    transition:all 0.3s ease;
    position:relative;
    overflow:hidden;
  }
  
  .relay-card::before{
    content:"";
    position:absolute;
    top:0;
    left:0;
    width:100%;
    height:4px;
    background:linear-gradient(90deg, var(--blue), var(--gold));
  }
  
  .relay-card:hover{
    transform:translateY(-8px);
    box-shadow:var(--shadow);
    border-color:var(--gold-light);
  }
  
  .relay-card.pending{
    opacity:0.7;
    pointer-events:none;
  }
  
  .relay-card.pending .relay-icon{
    animation:spin 2s linear infinite;
  }
  
  .ch-tag{
    position:absolute;
    top:12px;
    right:12px;
    font-size:0.65rem;
    font-weight:700;
    color:var(--gold);
    background:rgba(212,175,55,0.1);
    padding:6px 10px;
    border-radius:12px;
  }
  
  .relay-icon{
    font-size:3rem;
    margin-bottom:10px;
    display:block;
  }
  
  .relay-icon.on{color:var(--green)}
  .relay-icon.off{color:#E53935}
  
  .relay-name{font-weight:700;font-size:0.95rem;display:block;margin-bottom:6px;color:var(--text)}
  
  .relay-status{
    font-size:0.8rem;
    font-weight:700;
    margin-bottom:12px;
    letter-spacing:0.5px;
    padding:6px 10px;
    border-radius:8px;
    display:inline-block;
  }

  .relay-status.on{
    background:rgba(46,204,113,0.15);
    color:var(--green);
  }

  .relay-status.off{
    background:rgba(229,57,53,0.15);
    color:#E53935;
  }

  /* Channel Switches */
  .channel-switch{
    position:relative;
    display:inline-block;
    width:80px;
    height:42px;
    margin-top:8px;
  }

  .channel-switch input{
    opacity:0;
    width:0;
    height:0;
  }

  .switch-slider{
    position:absolute;
    cursor:pointer;
    top:0;
    left:0;
    right:0;
    bottom:0;
    background-color:#ccc;
    transition:0.3s;
    border-radius:42px;
    border:3px solid #999;
  }

  .switch-slider:before{
    position:absolute;
    content:"";
    height:32px;
    width:32px;
    left:4px;
    bottom:3px;
    background-color:white;
    transition:0.3s;
    border-radius:50%;
    box-shadow:0 2px 10px rgba(0,0,0,0.25);
  }

  /* Manual mode - Green ON switch */
  .channel-switch.manual input:checked + .switch-slider{
    background:linear-gradient(135deg, var(--green) 0%, #27ae60 100%);
    border-color:#229954;
    box-shadow:0 0 20px rgba(46,204,113,0.4);
  }

  .channel-switch.manual input:checked + .switch-slider:before{
    transform:translateX(38px);
  }

  /* OFF state - Red/Gray switch */
  .channel-switch.manual:not(.on) .switch-slider{
    background:linear-gradient(135deg, #E53935 0%, #C62828 100%);
    border-color:#B71C1C;
    box-shadow:0 0 20px rgba(229,57,53,0.3);
  }
  
  .btn-row{display:flex;gap:10px;justify-content:center}
  
  .btn{
    padding:14px 12px;
    border-radius:16px;
    border:none;
    font-family:inherit;
    font-weight:700;
    font-size:0.85rem;
    cursor:pointer;
    flex:1;
    transition:all 0.3s ease;
    text-transform:uppercase;
    letter-spacing:1px;
    position:relative;
    overflow:hidden;
  }
  
  .btn::before{
    content:"";
    position:absolute;
    top:50%;
    left:50%;
    width:0;
    height:0;
    border-radius:50%;
    background:rgba(255,255,255,0.3);
    transition:width 0.6s, height 0.6s;
    transform:translate(-50%,-50%);
  }
  
  .btn:active::before{
    width:300px;
    height:300px;
  }
  
  .btn-on{
    background:linear-gradient(135deg, var(--blue), #0984e3);
    color:white;
    box-shadow:0 4px 15px rgba(52,152,219,0.3);
  }
  
  .btn-on:hover{
    transform:translateY(-2px);
    box-shadow:0 6px 20px rgba(52,152,219,0.4);
  }
  
  .btn-off{
    background:var(--bg-light);
    color:var(--text-light);
    border:2px solid #ddd;
  }
  
  .btn-off:hover{
    background:#e0e6ed;
    border-color:var(--text-light);
    transform:translateY(-2px);
  }

  /* Auto UI: Schedules */
  .sched-list{
    display:flex;
    flex-direction:column;
    gap:14px;
    animation:fadeIn 0.6s ease-out 0.3s both;
  }
  
  .sched-card{
    background:var(--white);
    border-radius:24px;
    padding:22px;
    box-shadow:var(--shadow-sm);
    display:flex;
    justify-content:space-between;
    align-items:center;
    border-left:6px solid var(--gold);
    transition:all 0.3s ease;
    position:relative;
  }
  
  .sched-card::before{
    content:"";
    position:absolute;
    top:0;
    left:0;
    width:100%;
    height:4px;
    background:linear-gradient(90deg, var(--gold), transparent);
  }
  
  .sched-card:hover{
    transform:translateX(8px);
    box-shadow:var(--shadow);
  }
  
  .sched-ch{
    font-size:0.7rem;
    font-weight:800;
    color:var(--gold);
    text-transform:uppercase;
    letter-spacing:1.5px;
  }
  
  .sched-time{
    font-size:1.25rem;
    font-weight:700;
    display:block;
    margin:6px 0;
    color:var(--text);
  }
  
  .sched-days{
    font-size:0.8rem;
    color:var(--text-light);
    font-weight:500;
  }
  
  .active-badge{
    background:linear-gradient(135deg, var(--gold-light), #e6d676);
    color:var(--gold-dark);
    padding:8px 16px;
    border-radius:16px;
    font-size:0.7rem;
    font-weight:800;
    letter-spacing:1px;
    text-transform:uppercase;
    box-shadow:0 4px 12px rgba(212,175,55,0.2);
  }

  /* Console */
  .console-box{
    background:#0F1419;
    border-radius:24px;
    padding:18px;
    margin-top:28px;
    box-shadow:var(--shadow);
    border:2px solid #252d38;
    animation:fadeIn 0.6s ease-out 0.4s both;
  }
  
  #console{
    height:160px;
    overflow-y:auto;
    font-family:'Courier New',monospace;
    font-size:0.85rem;
    color:#00FF00;
    line-height:1.6;
    padding:12px;
  }
  
  #console::-webkit-scrollbar{width:6px}
  #console::-webkit-scrollbar-track{background:#1a2332;border-radius:10px}
  #console::-webkit-scrollbar-thumb{background:#4CAF50;border-radius:10px}
  
  .log-line{
    margin-bottom:6px;
    padding:4px 8px;
    border-radius:4px;
    animation:fadeIn 0.3s ease-out;
  }
  
  .log-cmd{color:#00AAFF}
  .log-err{color:#FF6B6B}
  .log-ok{color:#51CF66}

  /* Config Section */
  .config-btn{
    position:fixed;
    bottom:24px;
    right:24px;
    width:56px;
    height:56px;
    border-radius:50%;
    background:linear-gradient(135deg, var(--gold), var(--gold-dark));
    color:var(--text);
    border:none;
    font-weight:700;
    font-size:1.6rem;
    cursor:pointer;
    box-shadow:var(--shadow);
    transition:all 0.3s ease;
    z-index:100;
  }
  
  .config-btn:hover{
    transform:scale(1.1);
    box-shadow:0 12px 32px rgba(212,175,55,0.4);
  }
  
  .config-modal{
    display:none;
    position:fixed;
    top:0;
    left:0;
    width:100%;
    height:100%;
    background:rgba(0,0,0,0.6);
    z-index:1000;
    justify-content:center;
    align-items:center;
    animation:fadeIn 0.3s ease-out;
  }
  
  .config-modal.active{display:flex}
  
  .config-content{
    background:var(--white);
    border-radius:28px;
    padding:32px;
    width:90%;
    max-width:500px;
    max-height:80vh;
    overflow-y:auto;
    box-shadow:0 20px 60px rgba(0,0,0,0.3);
    animation:slideDown 0.3s ease-out;
  }
  
  .config-header{
    font-size:1.4rem;
    font-weight:700;
    margin-bottom:24px;
    color:var(--text);
    display:flex;
    justify-content:space-between;
    align-items:center;
  }
  
  .config-close{
    background:none;
    border:none;
    font-size:1.5rem;
    cursor:pointer;
    color:var(--text-light);
    transition:color 0.3s ease;
  }
  
  .config-close:hover{color:var(--text)}
  
  .config-form{display:flex;flex-direction:column;gap:16px}
  
  .form-group{
    display:flex;
    flex-direction:column;
    gap:6px;
  }
  
  .form-group label{
    font-weight:700;
    font-size:0.9rem;
    color:var(--text);
    letter-spacing:0.5px;
  }
  
  .form-group input{
    padding:12px;
    border:2px solid #ddd;
    border-radius:12px;
    font-family:inherit;
    font-size:0.9rem;
    transition:all 0.3s ease;
  }
  
  .form-group input:focus{
    outline:none;
    border-color:var(--gold);
    box-shadow:0 0 0 3px rgba(212,175,55,0.1);
  }
  
  .config-actions{
    display:flex;
    gap:12px;
    margin-top:24px;
  }
  
  .config-actions button{
    flex:1;
    padding:14px;
    border:none;
    border-radius:12px;
    font-weight:700;
    cursor:pointer;
    transition:all 0.3s ease;
  }
  
  .btn-save{
    background:linear-gradient(135deg, var(--green), #27ae60);
    color:white;
  }
  
  .btn-save:hover{
    transform:translateY(-2px);
    box-shadow:0 6px 16px rgba(46,204,113,0.3);
  }
  
  .btn-cancel{
    background:var(--bg-light);
    color:var(--text);
    border:2px solid #ddd;
  }
  
  .btn-cancel:hover{
    background:#e0e6ed;
    border-color:var(--text-light);
  }

  /* Responsive */

  @media(max-width:640px){
    .header h1{font-size:1.6rem}
    .relay-grid{grid-template-columns:1fr 1fr;gap:14px}
    .mode-bar{margin:20px 12px;padding:18px}
    .page{padding:8px 12px}
    .section-title{margin-top:20px;margin-bottom:12px}
  }
  
  @media(max-width:480px){
    .header{padding:24px 16px}
    .header h1{font-size:1.4rem;letter-spacing:1.5px}
    .relay-grid{grid-template-columns:1fr}
    .btn-row{gap:6px}
  }
</style>
</head>
<body>

<div class="header">
  <h1>METZABOOK</h1>
  <p>Gateway de Control LoRa</p>
</div>

<div class="mode-bar">
  <div class="mode-info">
    <span class="mode-title" id="mode-text">MODO MANUAL</span>
    <span class="mode-desc" id="mode-desc">Control directo de dispositivos</span>
  </div>
  <label class="switch">
    <input type="checkbox" id="mode-switch" onchange="toggleMode()">
    <span class="slider"></span>
  </label>
</div>

<div class="page">
  <div id="manual-ui">
    <div class="section-title">Control de Relevadores</div>
    <div class="relay-grid" id="relayGrid"></div>
  </div>

  <div id="auto-ui" style="display:none">
    <div class="section-title">Horarios Programados</div>
    <div class="sched-list" id="schedList"></div>
  </div>

  <div class="section-title">Monitor de Sistema</div>
  <div class="console-box">
    <div id="console"></div>
  </div>
</div>

<!-- Botón de Configuración -->
<button class="config-btn" onclick="openConfigModal()">⚙️</button>

<!-- Modal de Configuración -->
<div id="configModal" class="config-modal">
  <div class="config-content">
    <div class="config-header">
      <span>Configuración</span>
      <button class="config-close" onclick="closeConfigModal()">✕</button>
    </div>
    <form class="config-form">
      <div class="form-group">
        <label>Canal 1</label>
        <input type="text" id="name-0" maxlength="19" placeholder="Nombre para Salida 1">
      </div>
      <div class="form-group">
        <label>Canal 2</label>
        <input type="text" id="name-1" maxlength="19" placeholder="Nombre para Salida 2">
      </div>
      <div class="form-group">
        <label>Canal 3</label>
        <input type="text" id="name-2" maxlength="19" placeholder="Nombre para Salida 3">
      </div>
      <div class="form-group">
        <label>Canal 4</label>
        <input type="text" id="name-3" maxlength="19" placeholder="Nombre para Salida 4">
      </div>
      <div class="config-actions">
        <button type="button" class="config-actions button btn-save" onclick="saveConfig()">Guardar</button>
        <button type="button" class="config-actions button btn-cancel" onclick="closeConfigModal()">Cancelar</button>
      </div>
    </form>
  </div>
</div>

<script>
let relayNames = ['Salida 1', 'Salida 2', 'Salida 3', 'Salida 4'];
let relayOn = [false,false,false,false];
let autoMode = false;
let pending = [false,false,false,false];
let schedules = [];

function openConfigModal(){
  document.getElementById('configModal').classList.add('active');
  for(let i = 0; i < 4; i++){
    document.getElementById('name-' + i).value = relayNames[i];
  }
}

function closeConfigModal(){
  document.getElementById('configModal').classList.remove('active');
}

async function saveConfig(){
  const names = [];
  for(let i = 0; i < 4; i++){
    names.push(document.getElementById('name-' + i).value);
  }
  
  log('Guardando configuración...', 'log-cmd');
  try {
    const r = await fetch('/config', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({names: names})
    });
    const d = await r.json();
    if(d.status === 'ok'){
      for(let i = 0; i < 4; i++){
        relayNames[i] = names[i] || ('Salida ' + (i+1));
      }
      renderUI();
      closeConfigModal();
      log('Configuración guardada exitosamente', 'log-ok');
    } else {
      log('Error al guardar: ' + d.message, 'log-err');
    }
  } catch(e) {
    log('Error de conexión', 'log-err');
  }
}

window.addEventListener('keydown', function(e){
  if(e.key === 'Escape') closeConfigModal();
});

function log(msg, type=''){
  const c = document.getElementById('console');
  const line = document.createElement('div');
  line.className = 'log-line ' + type;
  line.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  c.insertBefore(line, c.firstChild);
}

function renderUI(){
  document.getElementById('mode-switch').checked = autoMode;
  document.getElementById('mode-text').textContent = autoMode ? 'MODO AUTOMÁTICO' : 'MODO MANUAL';
  document.getElementById('mode-desc').textContent = autoMode ? 'Operación basada en horarios' : 'Control directo de dispositivos';
  
  document.getElementById('manual-ui').style.display = autoMode ? 'none' : 'block';
  document.getElementById('auto-ui').style.display = autoMode ? 'block' : 'none';

  if(!autoMode){
    const grid = document.getElementById('relayGrid');
    grid.innerHTML = '';
    for(let i=0; i<4; i++){
      const on = relayOn[i];
      grid.innerHTML += `
        <div class="relay-card ${pending[i]?'pending':''}">
          <span class="ch-tag">CANAL ${i+1}</span>
          <span class="relay-icon ${on?'on':'off'}">${on?'⚡':'📴'}</span>
          <span class="relay-name">${relayNames[i] || 'Salida '+(i+1)}</span>
          <div class="relay-status ${on?'on':'off'}">${on?'✓ ENCENDIDO':'✗ APAGADO'}</div>
          <label class="channel-switch manual ${on?'on':''}">
            <input type="checkbox" ${on?'checked':''} onchange="sendRelay(${i+1},this.checked?1:0)" ${pending[i]?'disabled':''}>
            <span class="switch-slider"></span>
          </label>
        </div>`;
    }
  } else {
    const list = document.getElementById('schedList');
    list.innerHTML = schedules.length ? '' : '<div style="text-align:center;padding:30px;color:#999">No hay horarios configurados</div>';
    const DAYS = ['Dom','Lun','Mar','Mié','Jue','Vie','Sáb'];
    schedules.forEach(s => {
      let dStr = '';
      for(let i=0;i<7;i++) if(s.mask & (1<<i)) dStr += DAYS[i] + ' ';
      list.innerHTML += `
        <div class="sched-card">
          <div>
            <span class="sched-ch">${relayNames[s.ch-1]}</span>
            <span class="sched-time">${String(s.onH).padStart(2,'0')}:${String(s.onM).padStart(2,'0')} - ${String(s.offH).padStart(2,'0')}:${String(s.offM).padStart(2,'0')}</span>
            <span class="sched-days">${dStr}</span>
          </div>
          <span class="active-badge">ACTIVO</span>
        </div>`;
    });
  }
}

async function sendRelay(ch, st){
  pending[ch-1] = true;
  renderUI();
  log(`Enviando: R${ch}${st}`, 'log-cmd');
  try {
    const r = await fetch(`/cmd?q=R${ch}${st}`);
    const d = await r.json();
    log(`Respuesta: ${d.message}`);
  } catch(e) {
    pending[ch-1] = false;
    renderUI();
    log('Error de conexión','err');
  }
}

async function toggleMode(){
  const target = document.getElementById('mode-switch').checked ? 'A' : 'M';
  log(`Cambiando modo a: ${target==='A'?'Auto':'Manual'}`, 'log-cmd');
  try {
    const r = await fetch(`/cmd?q=${target}`);
    const d = await r.json();
    log(`Respuesta: ${d.message}`);
    // No cambiamos autoMode aquí, esperamos al poll()
  } catch(e) {
    document.getElementById('mode-switch').checked = autoMode;
    log('Error al cambiar modo','err');
  }
}

async function poll(){
  try {
    const r = await fetch('/status');
    const d = await r.json();
    // En el gateway modular, necesitamos pedir el estado al esclavo via /cmd?q=S?
    // o el gateway puede cachear la respuesta.
    // Por ahora, simulamos la sincronización basada en el /cmd previo.
  } catch(e) {}
}

// Para el gateway, necesitamos un poll que pida S? periódicamente
async function syncStatus(){
  try {
    const r = await fetch('/cmd?q=S?');
    const d = await r.json();
    const msg = d.message; // Ej: S:1010:A
    if(msg.startsWith('S:')){
      const parts = msg.split(':');
      const states = parts[1];
      const mode = parts[2];
      
      autoMode = (mode === 'A');
      for(let i=0; i<4; i++){
        relayOn[i] = (states[i] === '1');
        pending[i] = false;
      }
      renderUI();
    }
  } catch(e) {}
}

// Cargar nombres guardados
async function loadNames(){
  try{
    const r = await fetch('/names');
    const d = await r.json();
    if(d.names && Array.isArray(d.names)){
      relayNames = d.names;
    }
  }catch(e){}
  setInterval(syncStatus, 4000);
  syncStatus();
  renderUI();
  log('Gateway listo. Monitoreando esclavo...');
}

loadNames();
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

    // El Gateway simplemente pasa el comando al Slave
    if (sendLoRaToSlave(q.c_str())) {
      lastLoRaResponse = "";
      unsigned long start = millis();
      while (millis() - start < 2000) {
        receiveLoRa();
        if (lastLoRaResponse.length() > 0) break;
        delay(10);
      }
      String msg = (lastLoRaResponse.length() > 0) ? lastLoRaResponse : "Comando enviado";
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
    // Parsear JSON simple: {"names":["name1","name2",...]}
    int start = body.indexOf("[");
    int end = body.lastIndexOf("]");
    
    if(start != -1 && end != -1) {
      String names = body.substring(start + 1, end);
      int idx = 0;
      int nameIdx = 0;
      
      while(nameIdx < NUM_RELAYS && idx < names.length()) {
        int quote1 = names.indexOf('"', idx);
        if(quote1 == -1) break;
        int quote2 = names.indexOf('"', quote1 + 1);
        if(quote2 == -1) break;
        
        String name = names.substring(quote1 + 1, quote2);
        if(name.length() > 0) {
          setRelayName(nameIdx, name);
        }
        nameIdx++;
        idx = quote2 + 1;
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

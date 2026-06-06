#pragma once

// SPA servida desde flash (PROGMEM) — sin dependencias externas
static const char PORTAL_HTML[] PROGMEM = R"PWUI(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PhaseWatch S3</title>
<style>
:root{--bg:#0f1419;--card:#1a2332;--border:#2d3a4d;--text:#e7ecf3;--muted:#8b9cb3;--accent:#3b82f6;--ok:#22c55e;--warn:#eab308;--err:#ef4444;--radius:12px}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--text);min-height:100vh}
.app{max-width:960px;margin:0 auto;padding:16px}
header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:16px;flex-wrap:wrap}
.brand h1{font-size:1.25rem;font-weight:700}
.brand p{color:var(--muted);font-size:.85rem}
.badge{display:inline-flex;align-items:center;gap:6px;padding:6px 10px;border-radius:999px;font-size:.75rem;border:1px solid var(--border);background:var(--card)}
.badge::before{content:'';width:8px;height:8px;border-radius:50%;background:var(--muted)}
.badge.ok::before{background:var(--ok)}.badge.err::before{background:var(--err)}.badge.warn::before{background:var(--warn)}
.tabs{display:flex;gap:8px;margin-bottom:16px;flex-wrap:wrap}
.tab{flex:1;min-width:100px;padding:10px 14px;border:1px solid var(--border);background:var(--card);color:var(--text);border-radius:var(--radius);cursor:pointer;font-weight:600}
.tab.active{border-color:var(--accent);box-shadow:0 0 0 1px var(--accent)}
.panel{display:none}.panel.active{display:block}
.grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(140px,1fr))}
.card{background:var(--card);border:1px solid var(--border);border-radius:var(--radius);padding:14px}
.card h3{font-size:.8rem;color:var(--muted);margin-bottom:8px;text-transform:uppercase;letter-spacing:.04em}
.val{font-size:1.35rem;font-weight:700}
.val.sm{font-size:1rem;font-weight:600;word-break:break-all}
.phase{display:flex;align-items:center;justify-content:space-between;padding:12px 14px;border-radius:var(--radius);border:1px solid var(--border);background:var(--card);margin-bottom:8px}
.phase .name{font-weight:600}
.status-pill{padding:4px 10px;border-radius:999px;font-size:.75rem;font-weight:700}
.status-pill.ok{background:rgba(34,197,94,.15);color:var(--ok)}
.status-pill.err{background:rgba(239,68,68,.15);color:var(--err)}
.row{display:grid;gap:12px;grid-template-columns:1fr}
@media(min-width:700px){.row{grid-template-columns:1fr 1fr}}
label{display:block;font-size:.85rem;color:var(--muted);margin:10px 0 6px}
input{width:100%;padding:10px 12px;border-radius:8px;border:1px solid var(--border);background:#111827;color:var(--text)}
button,.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;padding:10px 14px;border:none;border-radius:8px;background:var(--accent);color:#fff;font-weight:600;cursor:pointer;width:100%;margin-top:10px}
button.secondary{background:#334155}
button.danger{background:var(--err)}
button:disabled{opacity:.6;cursor:not-allowed}
.actions{display:grid;gap:8px;grid-template-columns:1fr}
@media(min-width:500px){.actions{grid-template-columns:1fr 1fr}}
#logsBox,#atBox{background:#0b1220;border:1px solid var(--border);border-radius:var(--radius);padding:12px;overflow:auto;font:12px/1.45 ui-monospace,SFMono-Regular,Menlo,monospace;white-space:pre-wrap}
#logsBox{min-height:220px;max-height:320px}
#atBox{min-height:140px;max-height:220px;margin-top:10px}
.at-row{display:grid;gap:8px;grid-template-columns:1fr auto;align-items:end;margin-top:10px}
.at-row button{margin-top:0;width:auto;min-width:100px}
.at-presets{display:flex;gap:6px;flex-wrap:wrap;margin-top:8px}
.at-presets button{width:auto;margin-top:0;padding:6px 10px;font-size:.75rem}
.topic-list{display:grid;gap:8px}
.topic-item{padding:10px;border:1px solid var(--border);border-radius:8px;background:#111827}
.topic-item span{display:block;font-size:.75rem;color:var(--muted);margin-bottom:4px}
.toast{position:fixed;right:16px;bottom:16px;padding:12px 14px;border-radius:8px;background:#1e293b;border:1px solid var(--border);box-shadow:0 8px 24px rgba(0,0,0,.35);opacity:0;transform:translateY(8px);transition:.2s;z-index:20}
.toast.show{opacity:1;transform:translateY(0)}
.banner{padding:10px 12px;border-radius:8px;background:rgba(59,130,246,.12);border:1px solid rgba(59,130,246,.35);margin-bottom:12px;font-size:.9rem}
.danger-zone{margin-top:20px;padding:14px;border:1px solid rgba(239,68,68,.35);border-radius:var(--radius);background:rgba(239,68,68,.08)}
.danger-zone h3{color:var(--err);font-size:.9rem;margin-bottom:6px}
.danger-zone p{color:var(--muted);font-size:.85rem;margin-bottom:10px}
.modal-overlay{position:fixed;inset:0;background:rgba(0,0,0,.6);display:none;align-items:center;justify-content:center;padding:16px;z-index:30}
.modal-overlay.show{display:flex}
.modal{background:var(--card);border:1px solid var(--border);border-radius:var(--radius);padding:20px;max-width:400px;width:100%}
.modal h3{margin-bottom:8px}
.modal p{color:var(--muted);font-size:.9rem;margin-bottom:16px;line-height:1.4}
.modal-actions{display:grid;gap:8px;grid-template-columns:1fr 1fr}
</style>
</head>
<body>
<div class="app">
<header>
  <div class="brand"><h1>PhaseWatch S3</h1><p id="deviceId">—</p></div>
  <div id="modeBadge" class="badge warn">Cargando</div>
</header>
<div id="setupBanner" class="banner" hidden>Modo configuracion inicial. Completa la pestaña <b>Configuracion</b> para conectar el dispositivo.</div>
<nav class="tabs">
  <button class="tab active" data-tab="dashboard">Dashboard</button>
  <button class="tab" data-tab="config">Configuracion</button>
  <button class="tab" data-tab="logs">Logs</button>
</nav>
<section id="dashboard" class="panel active">
  <div class="grid" style="margin-bottom:12px">
    <div class="card"><h3>Criticidad</h3><div class="val" id="critVal">—</div></div>
    <div class="card"><h3>Uptime</h3><div class="val" id="uptimeVal">—</div></div>
    <div class="card"><h3>Red activa</h3><div class="val sm" id="netVal">—</div></div>
    <div class="card"><h3>Senal RSSI</h3><div class="val" id="rssiVal">—</div></div>
  </div>
  <h3 style="margin:8px 0;color:var(--muted);font-size:.85rem">FASES</h3>
  <div id="phases"></div>
  <div class="row" style="margin-top:12px">
    <div class="card">
      <h3>Conectividad</h3>
      <p style="margin:8px 0"><span id="mqttBadge" class="badge">MQTT</span></p>
      <p style="margin:8px 0"><span id="gsmDetBadge" class="badge">GSM modulo</span></p>
      <p style="margin:8px 0"><span id="gsmRegBadge" class="badge">GSM red</span></p>
      <p style="margin-top:10px;color:var(--muted);font-size:.85rem" id="battInfo">Bateria: —</p>
      <p style="margin-top:8px;color:var(--muted);font-size:.85rem" id="mdnsInfo">mDNS: —</p>
      <p style="margin-top:4px;color:var(--muted);font-size:.85rem" id="otaInfo">OTA: —</p>
      <p style="margin-top:8px;font-size:.8rem;color:var(--muted);word-break:break-word" id="gsmDebug">GSM debug: —</p>
    </div>
    <div class="card">
      <h3>MQTT Topics</h3>
      <div class="topic-list" id="topics"></div>
    </div>
  </div>
  <div class="actions" style="margin-top:12px">
    <button id="btnTest" class="secondary">Enviar notificacion de prueba</button>
    <button id="btnGsmRetry" class="secondary">Reintentar conexion GSM</button>
  </div>
</section>
<section id="config" class="panel">
  <form id="cfgForm">
    <div class="row">
      <div>
        <h3 style="margin-bottom:8px">Wi-Fi</h3>
        <label>SSID</label><input name="wifiSSID" id="wifiSSID" required>
        <label>Password</label><input name="wifiPass" id="wifiPass" type="password" placeholder="Dejar vacio para no cambiar">
      </div>
      <div>
        <h3 style="margin-bottom:8px">MQTT</h3>
        <label>Host</label><input name="mqttHost" id="mqttHost" required>
        <label>Puerto</label><input name="mqttPort" id="mqttPort" type="number" required>
        <label>Usuario</label><input name="mqttUser" id="mqttUser">
        <label>Password</label><input name="mqttPass" id="mqttPass" type="password" placeholder="Dejar vacio para no cambiar">
      </div>
    </div>
    <h3 style="margin:12px 0 8px">Telefonos SMS</h3>
    <div class="row">
      <div><label>Telefono 1</label><input name="phone0" id="phone0"></div>
      <div><label>Telefono 2</label><input name="phone1" id="phone1"></div>
    </div>
    <label>Telefono 3</label><input name="phone2" id="phone2">
    <h3 style="margin:16px 0 8px">Dispositivo</h3>
    <div class="row">
      <div>
        <label>Nombre mDNS</label>
        <input name="hostname" id="hostname" placeholder="phase" pattern="[a-zA-Z0-9-]+" title="Solo letras, numeros y guiones. Se publica como nombre.local">
        <p style="color:var(--muted);font-size:.8rem;margin-top:4px">Por defecto: <b>phase.local</b></p>
      </div>
      <div>
        <label>Password OTA</label>
        <input name="otaPass" id="otaPass" type="password" placeholder="Dejar vacio para no cambiar">
        <p style="color:var(--muted);font-size:.8rem;margin-top:4px">Por defecto: <b>Phase123</b></p>
      </div>
    </div>
    <button type="submit" id="btnSave">Guardar y reiniciar</button>
  </form>
  <div id="resetZone" class="danger-zone" hidden>
    <h3>Zona de peligro</h3>
    <p>Borra toda la configuracion guardada y reinicia el dispositivo en modo Access Point (PhaseWatch_Config).</p>
    <button type="button" id="btnReset" class="danger">Reset de fabrica</button>
  </div>
</section>
<section id="logs" class="panel">
  <pre id="logsBox">Cargando logs...</pre>
  <button class="secondary" id="btnRefreshLogs">Actualizar logs</button>
  <h3 style="margin:16px 0 8px;color:var(--muted);font-size:.85rem">CONSOLA AT (SIM800 @9600)</h3>
  <p style="color:var(--muted);font-size:.8rem;margin-bottom:8px">Enviá comandos al modem por UART. Podés escribir <b>AT+CREG?</b> o solo <b>+CREG?</b>.</p>
  <div class="at-row">
    <div><label>Comando</label><input id="atCmd" placeholder="AT+CSQ" autocomplete="off" spellcheck="false"></div>
    <button type="button" id="btnSendAt" class="secondary">Enviar</button>
  </div>
  <div class="at-presets">
    <button type="button" class="secondary at-preset" data-cmd="AT">AT</button>
    <button type="button" class="secondary at-preset" data-cmd="AT+CPIN?">CPIN?</button>
    <button type="button" class="secondary at-preset" data-cmd="AT+CREG?">CREG?</button>
    <button type="button" class="secondary at-preset" data-cmd="AT+CSQ">CSQ</button>
    <button type="button" class="secondary at-preset" data-cmd="AT+CCID">CCID</button>
    <button type="button" class="secondary at-preset" data-cmd="AT+COPS?">COPS?</button>
  </div>
  <pre id="atBox">Respuestas AT aparecerán aquí...</pre>
</section>
</div>
<div id="resetModal" class="modal-overlay">
  <div class="modal">
    <h3>Confirmar reset de fabrica</h3>
    <p>Se borraran Wi-Fi, MQTT, telefonos y toda la configuracion. El dispositivo volvera al hotspot <b>PhaseWatch_Config</b>.</p>
    <div class="modal-actions">
      <button type="button" class="secondary" id="btnResetCancel">Cancelar</button>
      <button type="button" class="danger" id="btnResetConfirm">Si, resetear</button>
    </div>
  </div>
</div>
<div id="toast" class="toast"></div>
<script>
const $=s=>document.querySelector(s);
const tabs=[...document.querySelectorAll('.tab')];
const panels=[...document.querySelectorAll('.panel')];
let statusKey='',logsTimer=null,pollMs=2000;

tabs.forEach(t=>t.onclick=()=>{
  tabs.forEach(x=>x.classList.remove('active'));
  panels.forEach(p=>p.classList.remove('active'));
  t.classList.add('active');
  $('#'+t.dataset.tab).classList.add('active');
  if(t.dataset.tab==='logs') refreshLogs();
});

function toast(msg){const el=$('#toast');el.textContent=msg;el.classList.add('show');setTimeout(()=>el.classList.remove('show'),2600)}
function setBadge(el,ok,label){el.className='badge '+(ok?'ok':(ok===false?'err':'warn'));el.textContent=label}
function fmtUptime(s){const h=Math.floor(s/3600),m=Math.floor((s%3600)/60),sec=s%60;return `${h}h ${m}m ${sec}s`}

function renderPhases(d){
  const wrap=$('#phases');wrap.innerHTML='';
  [['L1',d.l1],['L2',d.l2],['L3',d.l3]].forEach(([n,ok])=>{
    const div=document.createElement('div');
    div.className='phase';
    div.innerHTML=`<span class="name">${n}</span><span class="status-pill ${ok?'ok':'err'}">${ok?'OK':'FALLA'}</span>`;
    wrap.appendChild(div);
  });
}

function renderTopics(t){
  const wrap=$('#topics');wrap.innerHTML='';
  Object.entries(t).forEach(([k,v])=>{
    const item=document.createElement('div');
    item.className='topic-item';
    item.innerHTML=`<span>${k}</span><div class="val sm">${v}</div>`;
    wrap.appendChild(item);
  });
}

function applyStatus(d){
  const key=JSON.stringify({l1:d.l1,l2:d.l2,l3:d.l3,crit:d.criticality,mqtt:d.mqtt_connected,gsm:d.gsm_detected,gsmR:d.gsm_registered,net:d.network,rssi:d.rssi,batt:d.battery_mv,mdns:d.mdns_active,ota:d.ota_active,dbg:d.gsm_debug});
  if(key===statusKey) return;
  statusKey=key;
  $('#deviceId').textContent='ID '+d.device_id;
  $('#critVal').textContent=d.criticality;
  $('#uptimeVal').textContent=fmtUptime(d.uptime_s);
  $('#netVal').textContent=d.network;
  $('#rssiVal').textContent=d.rssi;
  renderPhases(d);
  setBadge($('#mqttBadge'),d.mqtt_connected,d.mqtt_connected?'MQTT conectado':'MQTT desconectado');
  setBadge($('#gsmDetBadge'),d.gsm_detected,d.gsm_detected?'GSM detectado':'GSM no detectado');
  setBadge($('#gsmRegBadge'),d.gsm_registered,d.gsm_registered?'GSM en red':'GSM sin red');
  $('#battInfo').textContent=d.battery_available?`Bateria (SIM800): ${(d.battery_mv/1000).toFixed(2)} V (${d.battery_pct}%) — tension de alimentacion del modulo`:'Bateria: no disponible (requiere modulo GSM)';
  $('#mdnsInfo').textContent=d.mdns_active?`mDNS activo: http://${d.mdns_fqdn}`:`mDNS: inactivo (${d.mdns_fqdn} al conectar Wi-Fi)`;
  $('#otaInfo').textContent=d.ota_active?`OTA activo en ${d.mdns_fqdn}`:'OTA: inactivo (requiere Wi-Fi)';
  $('#gsmDebug').textContent='GSM debug: '+(d.gsm_debug||'—');
  renderTopics(d.mqtt_topics);
  setBadge($('#modeBadge'),!d.config_mode,d.config_mode?'Modo AP':'Operativo');
  $('#setupBanner').hidden=!d.config_mode;
  $('#resetZone').hidden=d.config_mode;
}

async function pollStatus(){
  try{
    const r=await fetch('/api/status');
    if(!r.ok) throw new Error('status');
    applyStatus(await r.json());
  }catch(e){setBadge($('#modeBadge'),null,'Sin datos')}
}

async function loadConfig(){
  try{
    const r=await fetch('/api/config');
    if(!r.ok) return;
    const c=await r.json();
    $('#wifiSSID').value=c.wifiSSID||'';
    $('#mqttHost').value=c.mqttHost||'';
    $('#mqttPort').value=c.mqttPort||1883;
    $('#mqttUser').value=c.mqttUser||'';
    $('#phone0').value=c.phones?.[0]||'';
    $('#phone1').value=c.phones?.[1]||'';
    $('#phone2').value=c.phones?.[2]||'';
    $('#hostname').value=c.hostname||'phase';
    $('#otaPass').value='';
    $('#otaPass').placeholder=c.configured?'Dejar vacio para no cambiar':'Phase123';
  }catch(e){}
}

async function refreshLogs(){
  try{
    const r=await fetch('/api/logs');
    const j=await r.json();
    $('#logsBox').textContent=j.logs||'';
    $('#logsBox').scrollTop=$('#logsBox').scrollHeight;
  }catch(e){$('#logsBox').textContent='Error cargando logs'}
}

$('#cfgForm').onsubmit=async e=>{
  e.preventDefault();
  $('#btnSave').disabled=true;
  try{
    const fd=new FormData(e.target);
    const r=await fetch('/api/save',{method:'POST',body:fd});
    const j=await r.json();
    toast(j.message||'Guardado');
    if(j.restarting) setTimeout(()=>location.reload(),2500);
  }catch(err){toast('Error al guardar')}
  $('#btnSave').disabled=false;
};

$('#btnTest').onclick=async()=>{
  $('#btnTest').disabled=true;
  try{
    const r=await fetch('/api/test',{method:'POST'});
    toast((await r.json()).message||'Enviado');
  }catch(e){toast('Error de prueba')}
  $('#btnTest').disabled=false;
};

$('#btnGsmRetry').onclick=async()=>{
  $('#btnGsmRetry').disabled=true;
  toast('Reintentando GSM...');
  try{
    const r=await fetch('/api/gsm/retry',{method:'POST'});
    const j=await r.json();
    toast(j.message||'GSM actualizado');
    if(j.debug) $('#gsmDebug').textContent='GSM debug: '+j.debug;
    statusKey='';
    pollStatus();
    if($('#logs').classList.contains('active')) refreshLogs();
  }catch(e){toast('Error al reintentar GSM')}
  $('#btnGsmRetry').disabled=false;
};

const resetModal=$('#resetModal');
$('#btnReset').onclick=()=>resetModal.classList.add('show');
$('#btnResetCancel').onclick=()=>resetModal.classList.remove('show');
resetModal.onclick=e=>{if(e.target===resetModal) resetModal.classList.remove('show')};
$('#btnResetConfirm').onclick=async()=>{
  resetModal.classList.remove('show');
  $('#btnResetConfirm').disabled=true;
  try{
    await fetch('/api/reset',{method:'POST'});
    toast('Reseteando dispositivo...');
    setTimeout(()=>location.reload(),2000);
  }catch(e){toast('Error al resetear')}
  $('#btnResetConfirm').disabled=false;
};

const atHistory=[];
async function sendAt(){
  const cmd=$('#atCmd').value.trim();
  if(!cmd){toast('Escribe un comando AT');return;}
  $('#btnSendAt').disabled=true;
  try{
    const r=await fetch('/api/gsm/at',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd})});
    const j=await r.json();
    if(!j.ok){toast(j.message||'Error AT');return;}
    const block=`[${new Date().toLocaleTimeString()}] >>> ${j.command}\n${j.response}\n`;
    atHistory.unshift(block);
    if(atHistory.length>25) atHistory.pop();
    $('#atBox').textContent=atHistory.join('\n');
    refreshLogs();
  }catch(e){toast('Error al enviar AT')}
  $('#btnSendAt').disabled=false;
}
$('#btnSendAt').onclick=sendAt;
$('#atCmd').addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();sendAt();}});
document.querySelectorAll('.at-preset').forEach(b=>b.onclick=()=>{$('#atCmd').value=b.dataset.cmd;sendAt();});
$('#btnRefreshLogs').onclick=refreshLogs;
loadConfig();
pollStatus();
setInterval(pollStatus,pollMs);
setInterval(()=>{if($('#logs').classList.contains('active')) refreshLogs();},4000);
</script>
</body>
</html>)PWUI";

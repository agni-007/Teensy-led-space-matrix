#include <WiFi.h>
#include <WebServer.h>

const char* ssid     = "RGB_Control";
const char* password = "rgb12345";

WebServer server(80);
HardwareSerial teensySerial(1);  // UART1: RX=GPIO4, TX=GPIO5

String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Massive Matrix Controller</title>
  <style>
    * { box-sizing:border-box; margin:0; padding:0; }
    body { background:#0d0d12; color:#f0f0f0; font-family:sans-serif; padding:14px; }
    h2 { color:#00c8d4; text-align:center; margin-bottom:14px; font-size:18px; letter-spacing:1px; }
    .card { background:#16161f; border-radius:10px; padding:14px; margin-bottom:12px; }
    .label { font-size:11px; color:#00c8d4; text-transform:uppercase; letter-spacing:1.2px; font-weight:bold; margin-bottom:6px; }
    select, input[type=text] {
      width:100%; padding:10px; border-radius:6px;
      background:#222230; color:white; border:1px solid #00c8d4; font-size:14px;
    }
    input[type=range] { width:100%; accent-color:#00c8d4; margin-top:4px; }
    input[type=color] {
      width:60px; height:60px; border:none; border-radius:50%;
      cursor:pointer; background:none;
    }
    .row2 { display:flex; gap:10px; align-items:center; }
    .row2 > * { flex:1; }
    button {
      background:#00c8d4; color:#0d0d12; border:none; padding:11px;
      font-size:14px; border-radius:6px; cursor:pointer; font-weight:bold;
      width:100%; margin-top:8px; letter-spacing:0.4px;
    }
    button:active { background:#009aa4; }
    .btn-off   { background:#333; color:#aaa; }
    #status { text-align:center; font-size:12px; color:#00c8d4; margin-top:10px; min-height:16px; }
  </style>
</head>
<body>

<h2>Massive Matrix (18x508)</h2>

<div class="card">
  <div class="label">Theme</div>
  <select id="theme" onchange="sendTheme()">
    <option value="0">Solid Fill</option>
    <option value="1">Wipe ← Left</option>
    <option value="2">Wipe Right →</option>
    <option value="3">Wipe ↓ Top</option>
    <option value="4">Wipe ↑ Bottom</option>
    <option value="7">Dual Sync ←→</option>
    <option value="8">Dual Sync ↕</option>
    <option value="9">All Sides Converge</option>
    <option value="11">Scroll Text</option>
  </select>
</div>

<div class="card">
  <div class="label">Speed &nbsp;<span id="speedLbl" style="color:#fff">3</span> / 10</div>
  <input type="range" id="speed" min="1" max="10" value="3"
    oninput="document.getElementById('speedLbl').innerText=this.value"
    onchange="sendTheme()">
</div>

<div class="card">
  <div class="label">Master Brightness &nbsp;<span id="brightLbl" style="color:#fff">40</span></div>
  <input type="range" id="brightness" min="0" max="255" value="40"
    oninput="document.getElementById('brightLbl').innerText=this.value"
    onchange="sendBrightness()">
</div>

<div class="card">
  <div class="label">Color Control</div>
  <div class="row2" style="justify-content:center;">
    <input type="color" id="color" value="#00c8d4" onchange="sendColor()">
    <div style="flex:1">
      <button onclick="setPreset(255,0,0)">Red</button>
      <button onclick="setPreset(0,255,0)" style="margin-top:4px">Green</button>
      <button onclick="setPreset(0,0,255)" style="margin-top:4px">Blue</button>
      <button onclick="setPreset(255,255,255)" style="margin-top:4px">White</button>
    </div>
  </div>
</div>

<div class="card">
  <div class="label">Scrolling Text</div>
  <input type="text" id="scrollMsg" placeholder="TYPE MESSAGE" maxlength="60">
  <button onclick="sendScrollText()">Scroll This Text</button>
</div>

<button class="btn-off" onclick="clearAll()">⬛ Blackout All</button>

<p id="status">System Online</p>

<script>
function hexToRgb(hex) {
  return { r:parseInt(hex.substr(1,2),16), g:parseInt(hex.substr(3,2),16), b:parseInt(hex.substr(5,2),16) };
}
function rgbToHex(r,g,b) {
  return '#' + [r,g,b].map(v=>v.toString(16).padStart(2,'0')).join('');
}
function send(url) {
  return fetch(url).then(r=>r.text()).catch(e=>setStatus('Error: '+e));
}
function setStatus(t) { document.getElementById('status').innerText = t; }

function sendTheme() {
  let t = document.getElementById('theme').value;
  let s = document.getElementById('speed').value;
  send('/setAnim?mode=' + t + '&speed=' + s);
  setStatus('Theme updated');
}

function sendBrightness() {
  let b = document.getElementById('brightness').value;
  send('/brightness?val=' + b);
  setStatus('Brightness: ' + b);
}

function sendColor() {
  let rgb = hexToRgb(document.getElementById('color').value);
  send('/set?r='+rgb.r+'&g='+rgb.g+'&b='+rgb.b);
  setStatus('Color applied');
}

function setPreset(r, g, b) {
  document.getElementById('color').value = rgbToHex(r, g, b);
  send('/set?r='+r+'&g='+g+'&b='+b);
  setStatus('Preset applied');
}

function sendScrollText() {
  let msg = document.getElementById('scrollMsg').value;
  if (!msg) { setStatus('Type a message first'); return; }
  document.getElementById('theme').value = "11";
  sendTheme();
  send('/text?msg=' + encodeURIComponent(msg)).then(() => setStatus('Scrolling...'));
}

function clearAll() {
  send('/clear');
  setStatus('Blackout active');
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200,"text/html", page); }

void handleSetAnim() {
  String msg = "<A," + server.arg("mode") + "," + server.arg("speed") + ">\n";
  teensySerial.print(msg); teensySerial.flush();
  server.send(200,"text/plain","OK");
}

void handleSet() {
  String msg = "<S," + server.arg("r") + "," + server.arg("g") + "," + server.arg("b") + ">\n";
  teensySerial.print(msg); teensySerial.flush();
  server.send(200,"text/plain","OK");
}

void handleBrightness() {
  String msg = "<B," + server.arg("val") + ">\n";
  teensySerial.print(msg); teensySerial.flush();
  server.send(200,"text/plain","OK");
}

void handleText() {
  String msg = server.arg("msg");
  msg.toUpperCase();
  String safe = "";
  for (unsigned int i = 0; i < msg.length(); i++) {
    char c = msg[i];
    bool ok = (c==' ') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') ||
              c=='.'||c==','||c=='!'||c=='?'||c==':'||c=='-'||c=='\'';
    safe += ok ? c : ' ';
  }
  if (safe.length() > 60) safe = safe.substring(0, 60);
  String out = "<T," + safe + ">\n";
  teensySerial.print(out); teensySerial.flush();
  server.send(200,"text/plain","OK");
}

void handleClear() {
  teensySerial.print("<X>\n"); teensySerial.flush();
  server.send(200,"text/plain","OK");
}

void setup() {
  teensySerial.begin(115200, SERIAL_8N1, 4, 5);
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/setAnim", handleSetAnim);
  server.on("/set", handleSet);
  server.on("/brightness", handleBrightness);
  server.on("/text", handleText);
  server.on("/clear", handleClear);
  server.begin();
}

void loop() {
  server.handleClient();
}

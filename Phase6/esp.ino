#include <WiFi.h>
#include <WebServer.h>

// =========================================================
// 🌐 WIFI CONFIG
// =========================================================
// AP mode: ESP32 creates its own network — no router needed,
// just connect your phone directly to it. This is what your
// original build used, kept as the default here.
const char* ssid     = "RGB_Control";
const char* password = "rgb12345";

// Want it to join your home WiFi instead? Set this true and
// point ssid/password at your router's credentials above.
#define USE_STATION_MODE false
// =========================================================

WebServer server(80);

// UART1 to the Teensy — RX=GPIO4, TX=GPIO5 (matches your tested wiring)
HardwareSerial teensySerial(1);

// ---------- Dashboard page (served from flash) ----------
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Massive Matrix Controller</title>
  <style>
    * { box-sizing:border-box; margin:0; padding:0; }
    body { background:#0d0d12; color:#f0f0f0; font-family:sans-serif; padding:14px; max-width:480px; margin:0 auto; }
    h2 { color:#00c8d4; text-align:center; margin-bottom:14px; font-size:18px; letter-spacing:1px; }
    .card { background:#16161f; border-radius:10px; padding:14px; margin-bottom:12px; }
    .label { font-size:11px; color:#00c8d4; text-transform:uppercase; letter-spacing:1.2px; font-weight:bold; margin-bottom:8px; }
    input[type=text] {
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
    .theme-grid { display:grid; grid-template-columns:repeat(3,1fr); gap:8px; }
    .theme-btn {
      background:#222230; border:1px solid #00c8d4; color:#f0f0f0;
      padding:11px 4px; border-radius:6px; font-size:11.5px; cursor:pointer;
    }
    .theme-btn.active { background:#00c8d4; color:#0d0d12; font-weight:bold; }
    button {
      background:#00c8d4; color:#0d0d12; border:none; padding:11px;
      font-size:14px; border-radius:6px; cursor:pointer; font-weight:bold;
      width:100%; margin-top:8px; letter-spacing:0.4px;
    }
    button:active { background:#009aa4; }
    .btn-off   { background:#333; color:#aaa; }
    .preset-btn { margin-top:4px; padding:9px; font-size:13px; }
    #status { text-align:center; font-size:12px; color:#00c8d4; margin-top:10px; min-height:16px; }
  </style>
</head>
<body>

<h2>Massive Matrix (18x508)</h2>

<div class="card">
  <div class="label">Theme</div>
  <div class="theme-grid" id="themeGrid"></div>
</div>

<div class="card">
  <div class="label">Speed &nbsp;<span id="speedLbl" style="color:#fff">3</span> / 10</div>
  <input type="range" id="speed" min="1" max="10" value="3"
    oninput="document.getElementById('speedLbl').innerText=this.value"
    onchange="sendTheme()">
</div>

<div class="card">
  <div class="label">Master Brightness &nbsp;<span id="brightLbl" style="color:#fff">35</span></div>
  <input type="range" id="brightness" min="0" max="255" value="35"
    oninput="document.getElementById('brightLbl').innerText=this.value"
    onchange="sendBrightness()">
</div>

<div class="card">
  <div class="label">Color Control</div>
  <div class="row2" style="justify-content:center;">
    <input type="color" id="color" value="#00c8d4" onchange="sendColor()">
    <div style="flex:1">
      <button class="preset-btn" onclick="setPreset(255,0,0)">Red</button>
      <button class="preset-btn" onclick="setPreset(0,255,0)">Green</button>
      <button class="preset-btn" onclick="setPreset(0,0,255)">Blue</button>
      <button class="preset-btn" onclick="setPreset(255,255,255)">White</button>
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
const THEMES = [
  { id: 0,  name: "Solid Fill" },
  { id: 1,  name: "Wipe ← Left" },
  { id: 2,  name: "Wipe Right →" },
  { id: 3,  name: "Wipe ↓ Top" },
  { id: 4,  name: "Wipe ↑ Bottom" },
  { id: 7,  name: "Dual Sync ←→" },
  { id: 8,  name: "Dual Sync ↕" },
  { id: 9,  name: "All Sides" },
  { id: 11, name: "Scroll Text" },
  { id: 12, name: "Pac-Man" },
  { id: 13, name: "Chrome Dino" },
];

let activeTheme = 0;
const grid = document.getElementById('themeGrid');
THEMES.forEach(t => {
  const b = document.createElement('button');
  b.className = 'theme-btn' + (t.id === activeTheme ? ' active' : '');
  b.textContent = t.name;
  b.dataset.id = t.id;
  b.onclick = () => selectTheme(t.id);
  grid.appendChild(b);
});

function markActive(id) {
  document.querySelectorAll('.theme-btn').forEach(b => {
    b.classList.toggle('active', parseInt(b.dataset.id) === id);
  });
}

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

function selectTheme(id) {
  activeTheme = id;
  markActive(id);
  sendTheme();
}

function sendTheme() {
  let s = document.getElementById('speed').value;
  send('/setAnim?mode=' + activeTheme + '&speed=' + s);
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
  activeTheme = 11;
  markActive(11);
  send('/text?msg=' + encodeURIComponent(msg)).then(() => setStatus('Scrolling...'));
}

function clearAll() {
  activeTheme = 0;
  markActive(0);
  send('/clear');
  setStatus('Blackout active');
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleSetAnim() {
  if (!server.hasArg("mode") || !server.hasArg("speed")) {
    server.send(400, "text/plain", "missing mode/speed"); return;
  }
  String msg = "<A," + server.arg("mode") + "," + server.arg("speed") + ">\n";
  teensySerial.print(msg); teensySerial.flush();
  server.send(200, "text/plain", "OK");
}

void handleSet() {
  if (!server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b")) {
    server.send(400, "text/plain", "missing r/g/b"); return;
  }
  String msg = "<S," + server.arg("r") + "," + server.arg("g") + "," + server.arg("b") + ">\n";
  teensySerial.print(msg); teensySerial.flush();
  server.send(200, "text/plain", "OK");
}

void handleBrightness() {
  if (!server.hasArg("val")) { server.send(400, "text/plain", "missing val"); return; }
  String msg = "<B," + server.arg("val") + ">\n";
  teensySerial.print(msg); teensySerial.flush();
  server.send(200, "text/plain", "OK");
}

// Sanitize to exactly the charset FONT_5x7 supports (see fontIndex() in
// the Teensy sketch) so what you type is what actually renders instead
// of silently turning into blank spaces on the matrix.
void handleText() {
  if (!server.hasArg("msg")) { server.send(400, "text/plain", "missing msg"); return; }
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
  server.send(200, "text/plain", "OK");
}

void handleClear() {
  teensySerial.print("<X>\n"); teensySerial.flush();
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  teensySerial.begin(115200, SERIAL_8N1, 4, 5);

#if USE_STATION_MODE
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP: ");
  Serial.println(WiFi.localIP());
#else
  WiFi.softAP(ssid, password);
  Serial.print("AP started, IP: ");
  Serial.println(WiFi.softAPIP());
#endif

  server.on("/", handleRoot);
  server.on("/setAnim", handleSetAnim);
  server.on("/set", handleSet);
  server.on("/brightness", handleBrightness);
  server.on("/text", handleText);
  server.on("/clear", handleClear);
  server.begin();
  Serial.println("Dashboard server started");
}

void loop() {
  server.handleClient();
}

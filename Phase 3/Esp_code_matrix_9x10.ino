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
  <title>TinkerSpace Matrix</title>
  <style>
    * { box-sizing:border-box; margin:0; padding:0; }
    body { background:#0d0d12; color:#f0f0f0; font-family:sans-serif; padding:14px; }
    h2 { color:#00c8d4; text-align:center; margin-bottom:14px; font-size:18px; letter-spacing:1px; }

    .card { background:#16161f; border-radius:10px; padding:14px; margin-bottom:12px; }
    .label { font-size:11px; color:#00c8d4; text-transform:uppercase;
             letter-spacing:1.2px; font-weight:bold; margin-bottom:6px; }

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
    .val { width:30px; text-align:right; font-size:13px; color:#aaa; flex:0 0 30px; }

    button {
      background:#00c8d4; color:#0d0d12; border:none; padding:11px;
      font-size:14px; border-radius:6px; cursor:pointer; font-weight:bold;
      width:100%; margin-top:8px; letter-spacing:0.4px;
    }
    button:active { background:#009aa4; }
    .btn-clear { background:#c0392b; color:white; }
    .btn-off   { background:#333; color:#aaa; }

    /* 9x10 pixel grid */
    #grid {
      display:grid;
      grid-template-columns: repeat(10, 1fr);
      gap:4px;
      margin-top:8px;
    }
    .cell {
      aspect-ratio:1;
      background:#222230;
      border-radius:4px;
      display:flex;
      align-items:center;
      justify-content:center;
      font-size:9px;
      color:#555;
      cursor:pointer;
      border:1px solid #2a2a3a;
      transition:background 0.15s;
      user-select:none;
    }
    .cell:active { transform:scale(0.92); }
    .cell.lit { background:#00c8d4; color:#000; border-color:#00c8d4; }

    #status { text-align:center; font-size:12px; color:#00c8d4;
              margin-top:10px; min-height:16px; }
  </style>
</head>
<body>

<h2>TinkerSpace Matrix Control</h2>

<!-- THEME -->
<div class="card">
  <div class="label">Theme</div>
  <select id="theme" onchange="sendTheme()">
    <option value="0">Solid Fill</option>
    <option value="1">Wipe ← Left</option>
    <option value="2">Wipe Right →</option>
    <option value="3">Wipe ↓ Top</option>
    <option value="4">Wipe ↑ Bottom</option>
    <option value="5">Row Scan (debug)</option>
    <option value="6">Column Scan (debug)</option>
    <option value="7">Dual Sync ←→</option>
    <option value="8">Dual Sync ↕</option>
    <option value="9">All Sides Converge</option>
  </select>
</div>

<!-- SPEED -->
<div class="card">
  <div class="label">Speed &nbsp;<span id="speedLbl" style="color:#fff">3</span> / 10</div>
  <div class="row2">
    <input type="range" id="speed" min="1" max="10" value="3"
      oninput="document.getElementById('speedLbl').innerText=this.value"
      onchange="sendTheme()">
  </div>
  <div style="display:flex;justify-content:space-between;font-size:10px;color:#555;margin-top:2px;">
    <span>← Debug slow</span><span>Fast →</span>
  </div>
</div>

<!-- BRIGHTNESS -->
<div class="card">
  <div class="label">Brightness &nbsp;<span id="brightLbl" style="color:#fff">80</span></div>
  <input type="range" id="brightness" min="5" max="255" value="80"
    oninput="document.getElementById('brightLbl').innerText=this.value"
    onchange="sendBrightness()">
</div>

<!-- COLOR -->
<div class="card">
  <div class="label">Color</div>
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

<!-- PIXEL GRID -->
<div class="card">
  <div class="label">Pixel Grid — tap to toggle individual LEDs</div>
  <div id="grid"></div>
  <button class="btn-clear" onclick="clearGrid()" style="margin-top:8px">Clear Grid</button>
</div>

<!-- CLEAR ALL -->
<button class="btn-off" onclick="clearAll()">⬛ All Off</button>

<p id="status">Ready</p>

<script>
// ---- state ----
let litPixels = {};   // "r,c" -> true

// ---- build 9x10 grid ----
const grid = document.getElementById('grid');
for (let r = 0; r < 9; r++) {
  for (let c = 0; c < 10; c++) {
    const cell = document.createElement('div');
    cell.className = 'cell';
    cell.innerText = r + ',' + c;
    cell.dataset.r = r;
    cell.dataset.c = c;
    cell.onclick = () => togglePixel(r, c, cell);
    grid.appendChild(cell);
  }
}

function togglePixel(r, c, cell) {
  const key = r + ',' + c;
  if (litPixels[key]) {
    delete litPixels[key];
    cell.classList.remove('lit');
    // turn pixel off by sending black color then pixel command
    send('/set?r=0&g=0&b=0').then(() => send('/pixel?r=' + r + '&c=' + c));
  } else {
    litPixels[key] = true;
    cell.classList.add('lit');
    // send current color first, then pixel command
    let rgb = hexToRgb(document.getElementById('color').value);
    send('/set?r='+rgb.r+'&g='+rgb.g+'&b='+rgb.b)
      .then(() => send('/pixel?r=' + r + '&c=' + c));
  }
  setStatus('Pixel (' + r + ',' + c + ') toggled');
}

function clearGrid() {
  litPixels = {};
  document.querySelectorAll('.cell').forEach(c => c.classList.remove('lit'));
  send('/clear');
  setStatus('Grid cleared');
}

// ---- controls ----
function sendTheme() {
  let t = document.getElementById('theme').value;
  let s = document.getElementById('speed').value;
  send('/setAnim?mode=' + t + '&speed=' + s);
  setStatus('Theme ' + t + ' speed ' + s);
}

function sendBrightness() {
  let b = document.getElementById('brightness').value;
  send('/brightness?val=' + b);
  setStatus('Brightness ' + b);
}

function sendColor() {
  let rgb = hexToRgb(document.getElementById('color').value);
  send('/set?r='+rgb.r+'&g='+rgb.g+'&b='+rgb.b);
  setStatus('Color updated');
}

function setPreset(r, g, b) {
  document.getElementById('color').value = rgbToHex(r, g, b);
  send('/set?r='+r+'&g='+g+'&b='+b);
  setStatus('Preset color set');
}

function clearAll() {
  litPixels = {};
  document.querySelectorAll('.cell').forEach(c => c.classList.remove('lit'));
  send('/clear');
  setStatus('All off');
}

// ---- helpers ----
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
</script>
</body>
</html>
)rawliteral";

// ---- handlers ----
void handleRoot()       { server.send(200,"text/html", page); }

void handleSetAnim() {
  String mode  = server.arg("mode");
  String speed = server.arg("speed");
  String msg   = "<A," + mode + "," + speed + ">\n";
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

void handlePixel() {
  String msg = "<P," + server.arg("r") + "," + server.arg("c") + ">\n";
  teensySerial.print(msg); teensySerial.flush();
  server.send(200,"text/plain","OK");
}

void handleClear() {
  teensySerial.print("<X>\n"); teensySerial.flush();
  server.send(200,"text/plain","OK");
}

void setup() {
  Serial.begin(115200);
  teensySerial.begin(115200, SERIAL_8N1, 4, 5);  // RX=4 TX=5

  WiFi.softAP(ssid, password);
  server.on("/",           handleRoot);
  server.on("/setAnim",    handleSetAnim);
  server.on("/set",        handleSet);
  server.on("/brightness", handleBrightness);
  server.on("/pixel",      handlePixel);
  server.on("/clear",      handleClear);
  server.begin();

  Serial.print("Dashboard at: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
}

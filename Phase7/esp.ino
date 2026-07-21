#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// =========================================================
// 🌐 WIFI CONFIG
// =========================================================
const char* ssid     = "RGB_Control";
const char* password = "rgb12345";
#define USE_STATION_MODE false

WebServer server(80);

// UART1 to Teensy at 1.5 Mbps — RX=GPIO4, TX=GPIO5
HardwareSerial teensySerial(1);

// Thread Safety for UART
SemaphoreHandle_t uartMutex;

// FreeRTOS Task Handles
TaskHandle_t NetworkTaskHandle = NULL;
TaskHandle_t GameTaskHandle    = NULL;

// Global Shared States (Volatile for cross-core safety)
volatile uint8_t activeThemeID = 11; 
volatile uint8_t activeSpeed   = 3;
volatile bool    resetGame     = false;

// Play Mode Flags: 0 = Manual Control (3 tries), 1 = Full Automatic Infinite Looprun
volatile uint8_t dinoPlayMode  = 1; 
volatile bool    manualJump    = false;

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

<div class="card" id="dinoCard" style="display:none; text-align:center;">
  <div class="label">Chrome Dino Controls</div>
  <div class="row2" style="margin-top:8px; margin-bottom:8px;">
    <button id="modeManualBtn" onclick="setDinoMode(0)" style="background:#222230; color:#00c8d4; border:1px solid #00c8d4;">Manual</button>
    <button id="modeAutoBtn" onclick="setDinoMode(1)" style="background:#00c8d4; color:#0d0d12;">Full Auto</button>
  </div>
  <button id="jumpBtn" style="background:#ff0055; color:#fff; font-size:22px; padding:16px; border-radius:12px; display:none;" onclick="sendJump()">🦖 JUMP!</button>
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

let activeTheme = 11; 
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
  document.getElementById('dinoCard').style.display = (id === 13) ? 'block' : 'none';
  sendTheme();
}

function setDinoMode(mode) {
  fetch('/dinoMode?mode=' + mode).then(r => r.text()).then(() => {
    if (mode === 0) {
      document.getElementById('modeManualBtn').style.background = '#00c8d4';
      document.getElementById('modeManualBtn').style.color = '#0d0d12';
      document.getElementById('modeAutoBtn').style.background = '#222230';
      document.getElementById('modeAutoBtn').style.color = '#00c8d4';
      document.getElementById('jumpBtn').style.display = 'block';
      setStatus('Manual Play Mode Active');
    } else {
      document.getElementById('modeAutoBtn').style.background = '#00c8d4';
      document.getElementById('modeAutoBtn').style.color = '#0d0d12';
      document.getElementById('modeManualBtn').style.background = '#222230';
      document.getElementById('modeManualBtn').style.color = '#00c8d4';
      document.getElementById('jumpBtn').style.display = 'none';
      setStatus('Full Auto Looprun Active');
    }
  });
}

function sendTheme() {
  let s = document.getElementById('speed').value;
  send('/setAnim?mode=' + activeTheme + '&speed=' + s);
  setStatus('Theme updated');
}

function sendJump() {
  send('/jump');
  setStatus('Jump Triggered!');
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
  send('/set?r='+r+'&g='+r+'&b='+b);
  setStatus('Preset applied');
}

function sendScrollText() {
  let msg = document.getElementById('scrollMsg').value;
  if (!msg) { setStatus('Type a message first'); return; }
  send('/text?msg=' + encodeURIComponent(msg)).then(() => setStatus('Scrolling...'));
}

function clearAll() {
  activeTheme = 0;
  markActive(0);
  document.getElementById('dinoCard').style.display = 'none';
  send('/clear');
  setStatus('Blackout active');
}
</script>
</body>
</html>
)rawliteral";

// =========================================================
// 🌐 WEB SERVER ROUTE HANDLERS
// =========================================================
void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleSetAnim() {
  if (!server.hasArg("mode") || !server.hasArg("speed")) {
    server.send(400, "text/plain", "missing mode/speed"); return;
  }
  
  uint8_t newMode = server.arg("mode").toInt();
  uint8_t newSpeed = server.arg("speed").toInt();

  if (activeThemeID != newMode || newMode == 12 || newMode == 13) {
    resetGame = true;
  }
  
  activeThemeID = newMode;
  activeSpeed   = newSpeed;

  String msg = "<A," + String(activeThemeID) + "," + String(activeSpeed) + ">\n";
  
  xSemaphoreTake(uartMutex, portMAX_DELAY);
  teensySerial.print(msg); 
  teensySerial.flush();
  xSemaphoreGive(uartMutex);
  
  server.send(200, "text/plain", "OK");
}

void handleDinoMode() {
  if (server.hasArg("mode")) {
    dinoPlayMode = server.arg("mode").toInt();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing mode");
  }
}

void handleJump() {
  if (dinoPlayMode == 0) {
    manualJump = true;
  }
  server.send(200, "text/plain", "JUMP");
}

void handleSet() {
  if (!server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b")) {
    server.send(400, "text/plain", "missing r/g/b"); return;
  }
  String msg = "<S," + server.arg("r") + "," + server.arg("g") + "," + server.arg("b") + ">\n";
  
  xSemaphoreTake(uartMutex, portMAX_DELAY);
  teensySerial.print(msg); 
  teensySerial.flush();
  xSemaphoreGive(uartMutex);
  
  server.send(200, "text/plain", "OK");
}

void handleBrightness() {
  if (!server.hasArg("val")) { server.send(400, "text/plain", "missing val"); return; }
  String msg = "<B," + server.arg("val") + ">\n";
  
  xSemaphoreTake(uartMutex, portMAX_DELAY);
  teensySerial.print(msg); 
  teensySerial.flush();
  xSemaphoreGive(uartMutex);
  
  server.send(200, "text/plain", "OK");
}

void handleText() {
  if (!server.hasArg("msg")) { server.send(400, "text/plain", "missing msg"); return; }
  String msg = server.arg("msg");
  msg.toUpperCase();
  String out = "<T," + msg + ">\n";
  
  xSemaphoreTake(uartMutex, portMAX_DELAY);
  teensySerial.print(out); 
  teensySerial.flush();
  xSemaphoreGive(uartMutex);
  
  server.send(200, "text/plain", "OK");
}

void handleClear() {
  xSemaphoreTake(uartMutex, portMAX_DELAY);
  teensySerial.print("<X>\n"); 
  teensySerial.flush();
  xSemaphoreGive(uartMutex);
  
  server.send(200, "text/plain", "OK");
}

// =========================================================
// 🧠 CORE 0: NETWORK & OTA TASK
// =========================================================
void TaskCore0_Network(void *pvParameters) {
  for (;;) {
    ArduinoOTA.handle();   
    server.handleClient(); 
    vTaskDelay(5 / portTICK_PERIOD_MS); 
  }
}

// =========================================================
// ⚡ CORE 1: GAME PHYSICS ENGINE
// =========================================================
void TaskCore1_GameEngine(void *pvParameters) {
  uint32_t lastFrameMicros = micros();

  float pacmanPos = 0;
  
  // Dino Game Physics & State
  float dinoY = 0;
  float dinoVy = 0;
  bool isJumping = false;
  float cactusPos[3] = { 508.0f, 658.0f, 808.0f }; 
  
  int dinoScore = 0;
  int dinoLives = 3;
  float scoreAccum = 0;
  int dinoState = 0; // 0=PLAYING, 1=CRASHED, 2=GAME_OVER
  float stateTimer = 0;

  // Color Cycling Palette (10 distinct RGB colors, 10 seconds each)
  const uint8_t colorPalette[10][3] = {
    {0, 200, 255}, {0, 255, 100}, {255, 255, 0}, {255, 128, 0}, {255, 0, 128},
    {128, 0, 255}, {0, 255, 255}, {255, 0, 0}, {0, 255, 0}, {0, 128, 255}
  };
  float colorTimer = 0;
  int colorIndex = 0;

  for (;;) {
    uint32_t now = micros();
    float dt = (now - lastFrameMicros) / 1000000.0f;
    lastFrameMicros = now;

    if (resetGame) {
      pacmanPos = -16.0f;
      dinoY = 0.0f;
      dinoVy = 0.0f;
      isJumping = false;
      cactusPos[0] = 508.0f;
      cactusPos[1] = cactusPos[0] + (float)random(120, 250);
      cactusPos[2] = cactusPos[1] + (float)random(120, 250);
      dinoScore = 0;
      dinoLives = 3;
      scoreAccum = 0;
      dinoState = 0;
      colorTimer = 0;
      colorIndex = 0;
      manualJump = false;
      resetGame = false;
    }

    uint8_t mode = activeThemeID;
    uint8_t speed = activeSpeed;

    if (mode == 12) { // Pac-Man
      float pacmanSpeed = 15.0f + (speed - 1) * (180.0f / 9.0f);
      pacmanPos += dt * pacmanSpeed;
      if (pacmanPos > 508.0f) pacmanPos = -16.0f;
      
      xSemaphoreTake(uartMutex, portMAX_DELAY);
      teensySerial.printf("<F,%d,0,0,0,0,0,0,0>\n", (int)pacmanPos);
      xSemaphoreGive(uartMutex);
    }
    else if (mode == 13) { // Dino
      float gameSpeed = 50.0f + (speed - 1) * (180.0f / 9.0f);

      if (dinoPlayMode == 1) {
        // Full Auto Mode: Infinite Looprun, Auto-Jump, Color Cycling every 10s
        colorTimer += dt;
        if (colorTimer >= 10.0f) {
          colorTimer = 0;
          colorIndex = (colorIndex + 1) % 10;
          xSemaphoreTake(uartMutex, portMAX_DELAY);
          teensySerial.printf("<S,%d,%d,%d>\n", colorPalette[colorIndex][0], colorPalette[colorIndex][1], colorPalette[colorIndex][2]);
          xSemaphoreGive(uartMutex);
        }

        scoreAccum += dt * (gameSpeed / 10.0f);
        if (scoreAccum >= 1.0f) {
            dinoScore += (int)scoreAccum;
            scoreAccum -= (int)scoreAccum;
        }

        for (int i = 0; i < 3; i++) {
          cactusPos[i] -= dt * gameSpeed;
          if (cactusPos[i] < -20.0f) {
            float maxC = 508.0f;
            for (int j = 0; j < 3; j++) { if (cactusPos[j] > maxC) maxC = cactusPos[j]; }
            cactusPos[i] = maxC + (float)random(100, 280); // Random Gap
          }
        }

        if (!isJumping) {
          float jumpTriggerDist = gameSpeed * 0.42f; 
          for (int i = 0; i < 3; i++) {
            if (cactusPos[i] > 40.0f && cactusPos[i] < 40.0f + jumpTriggerDist) {
              isJumping = true;
              dinoVy = 45.0f; 
              break;
            }
          }
        }

        if (isJumping) {
          dinoY += dinoVy * dt;
          dinoVy -= 110.0f * dt; 
          if (dinoY <= 0.0f) {
            dinoY = 0.0f;
            isJumping = false;
            dinoVy = 0.0f;
          }
        }

        dinoState = 0; // Infinite loop state
      } 
      else { 
        // Manual Mode (0): 3 Tries, Hitboxes, Scoring, Game Over states
        if (dinoState == 0) { 
          scoreAccum += dt * (gameSpeed / 10.0f);
          if (scoreAccum >= 1.0f) {
              dinoScore += (int)scoreAccum;
              scoreAccum -= (int)scoreAccum;
          }

          for (int i = 0; i < 3; i++) {
            cactusPos[i] -= dt * gameSpeed;
            if (cactusPos[i] < -20.0f) {
              float maxC = 508.0f;
              for (int j = 0; j < 3; j++) { if (cactusPos[j] > maxC) maxC = cactusPos[j]; }
              cactusPos[i] = maxC + (float)random(100, 280); // Random Gap
            }
          }

          bool hit = false;
          for (int i = 0; i < 3; i++) {
            if (cactusPos[i] < 46.0f && cactusPos[i] > 36.0f) { 
              if (dinoY < 6.0f) { 
                hit = true;
                break;
              }
            }
          }

          if (hit) {
            dinoLives--;
            if (dinoLives <= 0) {
              dinoState = 2; // GAME OVER
              stateTimer = 4.0f; 
            } else {
              dinoState = 1; // CRASHED
              stateTimer = 2.0f; 
            }
          } else {
            if (!isJumping) {
              if (manualJump) {
                isJumping = true;
                dinoVy = 45.0f; 
                manualJump = false; 
              }
            } else {
              manualJump = false; 
            }
            
            if (isJumping) {
              dinoY += dinoVy * dt;
              dinoVy -= 110.0f * dt; 
              if (dinoY <= 0.0f) {
                dinoY = 0.0f;
                isJumping = false;
                dinoVy = 0.0f;
              }
            }
          }
        } 
        else if (dinoState == 1) { 
          stateTimer -= dt;
          if (stateTimer <= 0) {
            cactusPos[0] = 508.0f; cactusPos[1] = 658.0f; cactusPos[2] = 808.0f;
            dinoY = 0.0f; isJumping = false; dinoVy = 0.0f;
            dinoState = 0; 
          }
        } 
        else if (dinoState == 2) { 
          stateTimer -= dt;
          if (stateTimer <= 0) {
            dinoScore = 0;
            dinoLives = 3;
            scoreAccum = 0;
            cactusPos[0] = 508.0f; cactusPos[1] = 658.0f; cactusPos[2] = 808.0f;
            dinoY = 0.0f; isJumping = false; dinoVy = 0.0f;
            dinoState = 0; 
          }
        }
      }

      xSemaphoreTake(uartMutex, portMAX_DELAY);
      teensySerial.printf("<F,%d,%d,%d,%d,%d,%d,%d,%d>\n", 
        dinoState, (int)dinoY, isJumping ? 1 : 0, 
        (int)cactusPos[0], (int)cactusPos[1], (int)cactusPos[2],
        dinoScore, dinoLives);
      xSemaphoreGive(uartMutex);
    }

    vTaskDelay(16 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);
  teensySerial.begin(1500000, SERIAL_8N1, 4, 5);
  uartMutex = xSemaphoreCreateMutex();

#if USE_STATION_MODE
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(400); }
#else
  WiFi.softAP(ssid, password);
#endif

  ArduinoOTA.setHostname("TinkerMatrix-ESP32");
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/setAnim", handleSetAnim);
  server.on("/dinoMode", handleDinoMode);
  server.on("/jump", handleJump);
  server.on("/set", handleSet);
  server.on("/brightness", handleBrightness);
  server.on("/text", handleText);
  server.on("/clear", handleClear);
  server.begin();
  
  xTaskCreatePinnedToCore(TaskCore0_Network, "NetworkTask", 8192, NULL, 1, &NetworkTaskHandle, 0);
  xTaskCreatePinnedToCore(TaskCore1_GameEngine, "GameTask", 8192, NULL, 2, &GameTaskHandle, 1);

  // 🚀 Startup Welcome Text Sequence
  delay(500);
  xSemaphoreTake(uartMutex, portMAX_DELAY);
  teensySerial.print("<A,11,3>\n"); 
  teensySerial.print("<T,WELCOME TO TINKERSPACE>\n");
  xSemaphoreGive(uartMutex);
}

void loop() {
  vTaskDelete(NULL); 
}

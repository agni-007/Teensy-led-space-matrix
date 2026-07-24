#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>

// =========================================================
// 🌐 WIFI CONFIG
// =========================================================
const char* ssid     = "RGB_Control";
const char* password = "rgb12345";
#define USE_STATION_MODE false

WebServer server(80);
WebSocketsServer webSocket(81);

// UART1 to Teensy at 1.5 Mbps — RX=GPIO4, TX=GPIO5
HardwareSerial teensySerial(1);

// Thread Safety for UART
SemaphoreHandle_t uartMutex;

// FreeRTOS Task Handles
TaskHandle_t NetworkTaskHandle = NULL;
TaskHandle_t GameTaskHandle    = NULL;

// Shared state is protected because the WebSocket task runs on core 0 and
// game physics runs on core 1. `volatile` alone is not cross-core locking.
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t activeThemeID = 11;
uint8_t activeSpeed   = 3;
uint8_t activeBrightness = 35;
uint8_t dinoPlayMode  = 1; // 0=manual, 1=automatic
uint8_t activeColorR  = 0;
uint8_t activeColorG  = 200;
uint8_t activeColorB  = 255;
bool resetGame        = false;
bool manualJump       = false;

constexpr uint8_t MAX_SAFE_BRIGHTNESS = 35;
constexpr size_t MAX_WS_COMMAND_LENGTH = 96;
constexpr float DINO_X_POSITION = 458.0f;

// ---------- Dashboard page (served from flash) ----------
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Massive Matrix Controller</title>
  <style>
    :root { color-scheme:dark; --bg:#0d0d12; --panel:#171720; --control:#252532; --ink:#f4f6f8; --muted:#b7bdc7; --accent:#00c8d4; --danger:#ff326f; }
    * { box-sizing:border-box; }
    body { background:var(--bg); color:var(--ink); font-family:system-ui,-apple-system,sans-serif; padding:14px; max-width:520px; margin:0 auto; }
    h1 { color:var(--accent); text-align:center; margin:4px 0 14px; font-size:19px; letter-spacing:.02em; }
    .card { background:var(--panel); border-radius:12px; padding:14px; margin-bottom:12px; }
    .label { display:block; font-size:12px; color:var(--accent); font-weight:750; margin-bottom:9px; }
    .value { color:var(--ink); font-variant-numeric:tabular-nums; }
    input[type=text] { width:100%; min-height:44px; padding:10px 12px; border-radius:7px; background:var(--control); color:var(--ink); border:1px solid #686879; font-size:16px; }
    input[type=range] { width:100%; min-height:34px; accent-color:var(--accent); }
    input[type=color] { width:72px; height:72px; padding:3px; border:2px solid #686879; border-radius:10px; cursor:pointer; background:var(--control); }
    .row { display:flex; gap:10px; align-items:center; }
    .row > * { flex:1; }
    .theme-grid { display:grid; grid-template-columns:repeat(3,1fr); gap:8px; }
    button { min-height:44px; width:100%; padding:10px; border:0; border-radius:7px; background:var(--accent); color:var(--bg); font:700 14px/1.2 system-ui,-apple-system,sans-serif; cursor:pointer; }
    button:hover:not(:disabled) { filter:brightness(1.08); }
    button:active:not(:disabled) { transform:translateY(1px); }
    button:focus-visible, input:focus-visible { outline:3px solid #fff; outline-offset:2px; }
    button:disabled, input:disabled { opacity:.48; cursor:not-allowed; }
    .theme-btn { background:var(--control); border:1px solid #686879; color:var(--ink); font-size:12px; }
    .theme-btn.active { background:var(--accent); border-color:var(--accent); color:var(--bg); }
    .theme-btn.active::before { content:'✓ '; }
    .mode-btn { background:var(--control); border:1px solid #686879; color:var(--ink); }
    .mode-btn.active { background:var(--accent); border-color:var(--accent); color:var(--bg); }
    #jumpBtn { display:none; margin-top:10px; background:var(--danger); color:#fff; font-size:18px; }
    .color-layout { display:flex; gap:12px; align-items:stretch; }
    .preset-grid { flex:1; display:grid; grid-template-columns:repeat(4,1fr); gap:7px; }
    .preset-btn { min-width:0; padding:8px 4px; border:2px solid transparent; color:#fff; font-size:11px; text-shadow:0 1px 2px #000; }
    .preset-btn[data-color="255,255,255"] { color:#111; text-shadow:none; }
    .preset-btn.active { border-color:#fff; }
    .preset-btn.active::after { content:' ✓'; }
    #scrollBtn { margin-top:9px; }
    .btn-off { background:#34343d; color:#f1f1f3; border:1px solid #777783; }
    #status { position:sticky; bottom:8px; padding:10px 12px; border-radius:7px; background:#20202a; color:var(--ink); text-align:center; font-size:13px; min-height:38px; }
    #status[data-state="connected"] { color:#76f7b0; }
    #status[data-state="error"] { color:#ff8bab; }
    @media (max-width:390px) { .theme-grid { grid-template-columns:repeat(2,1fr); } .preset-grid { grid-template-columns:repeat(2,1fr); } }
    @media (prefers-reduced-motion:reduce) { *,*::before,*::after { scroll-behavior:auto!important; transition:none!important; } }
  </style>
</head>
<body>

<h1>Massive Matrix <span aria-label="18 rows by 508 columns">18 × 508</span></h1>

<div class="card">
  <div class="label">Theme</div>
  <div class="theme-grid" id="themeGrid"></div>
</div>

<div class="card" id="dinoCard" hidden>
  <div class="label">Chrome Dino Controls</div>
  <div class="row">
    <button class="mode-btn" id="modeManualBtn" type="button">Manual</button>
    <button class="mode-btn active" id="modeAutoBtn" type="button">Full Auto</button>
  </div>
  <button id="jumpBtn" type="button">Jump</button>
</div>

<div class="card">
  <label class="label" for="speed">Speed <span class="value"><span id="speedLbl">3</span> / 10</span></label>
  <input type="range" id="speed" min="1" max="10" value="3">
</div>

<div class="card">
  <label class="label" for="brightness">Master Brightness <span class="value" id="brightLbl">35</span></label>
  <input type="range" id="brightness" min="0" max="35" value="35">
</div>

<div class="card">
  <div class="label">Color Control</div>
  <div class="color-layout">
    <input type="color" id="color" value="#00c8ff" aria-label="Custom matrix color">
    <div class="preset-grid">
      <button class="preset-btn" type="button" data-color="255,0,0" style="background:#d90032">Red</button>
      <button class="preset-btn" type="button" data-color="0,255,0" style="background:#087f38">Green</button>
      <button class="preset-btn" type="button" data-color="0,0,255" style="background:#174fc8">Blue</button>
      <button class="preset-btn" type="button" data-color="255,255,0" style="background:#a67c00">Yellow</button>
      <button class="preset-btn" type="button" data-color="0,255,255" style="background:#007d87">Cyan</button>
      <button class="preset-btn" type="button" data-color="255,0,255" style="background:#a8179a">Magenta</button>
      <button class="preset-btn" type="button" data-color="255,255,255" style="background:#fff">White</button>
      <button class="preset-btn" type="button" data-color="0,0,0" style="background:#111;border-color:#777">Black</button>
    </div>
  </div>
</div>

<div class="card">
  <div class="label">Scrolling Text</div>
  <input type="text" id="scrollMsg" placeholder="Type a message" maxlength="60" autocomplete="off">
  <button id="scrollBtn" type="button">Scroll This Text</button>
</div>

<button class="btn-off" id="blackoutBtn" type="button">Blackout All</button>

<p id="status" role="status" aria-live="polite">Connecting to control socket…</p>

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
let activeColor = [0, 200, 255];
let dinoMode = 1;
let socket;
let reconnectTimer;
let reconnectDelay = 500;
let pendingSentAt = 0;
let speedTimer;
let brightnessTimer;
const grid = document.getElementById('themeGrid');

THEMES.forEach(t => {
  const b = document.createElement('button');
  b.className = 'theme-btn' + (t.id === activeTheme ? ' active' : '');
  b.textContent = t.name;
  b.dataset.id = t.id;
  b.type = 'button';
  b.setAttribute('aria-pressed', t.id === activeTheme ? 'true' : 'false');
  b.addEventListener('click', () => send('A,' + t.id + ',' + speedValue()));
  grid.appendChild(b);
});

function applyThemeState(id) {
  activeTheme = id;
  document.querySelectorAll('.theme-btn').forEach(b => {
    const selected = Number(b.dataset.id) === id;
    b.classList.toggle('active', selected);
    b.setAttribute('aria-pressed', selected ? 'true' : 'false');
  });
  document.getElementById('dinoCard').hidden = id !== 13;
}

function hexToRgb(hex) {
  return [parseInt(hex.slice(1,3),16), parseInt(hex.slice(3,5),16), parseInt(hex.slice(5,7),16)];
}
function rgbToHex(rgb) {
  return '#' + rgb.map(v => Number(v).toString(16).padStart(2,'0')).join('');
}
function speedValue() { return document.getElementById('speed').value; }
function setStatus(text, state) {
  const status = document.getElementById('status');
  status.textContent = text;
  status.dataset.state = state || '';
}
function setConnected(connected) {
  document.querySelectorAll('button,input').forEach(el => { el.disabled = !connected; });
}
function applyColorState(rgb) {
  activeColor = rgb.map(Number);
  document.getElementById('color').value = rgbToHex(activeColor);
  document.querySelectorAll('.preset-btn').forEach(button => {
    const selected = button.dataset.color === activeColor.join(',');
    button.classList.toggle('active', selected);
    button.setAttribute('aria-pressed', selected ? 'true' : 'false');
  });
}
function applyDinoMode(mode) {
  dinoMode = Number(mode);
  const manual = dinoMode === 0;
  document.getElementById('modeManualBtn').classList.toggle('active', manual);
  document.getElementById('modeAutoBtn').classList.toggle('active', !manual);
  document.getElementById('modeManualBtn').setAttribute('aria-pressed', manual ? 'true' : 'false');
  document.getElementById('modeAutoBtn').setAttribute('aria-pressed', manual ? 'false' : 'true');
  document.getElementById('jumpBtn').style.display = manual ? 'block' : 'none';
}
function applyState(parts) {
  if (parts.length < 4) return;
  applyThemeState(Number(parts[1]));
  document.getElementById('speed').value = parts[2];
  document.getElementById('speedLbl').textContent = parts[2];
  document.getElementById('brightness').value = parts[3];
  document.getElementById('brightLbl').textContent = parts[3];
  if (parts.length >= 7) applyColorState([parts[4], parts[5], parts[6]]);
  if (parts.length >= 8) applyDinoMode(parts[7]);
}

function connectSocket() {
  clearTimeout(reconnectTimer);
  const host = window.location.hostname || '192.168.4.1';
  socket = new WebSocket('ws://' + host + ':81/');

  socket.onopen = () => {
    reconnectDelay = 500;
    setConnected(true);
    setStatus('Connected', 'connected');
  };

  socket.onmessage = event => {
    if (event.data.startsWith('STATE,')) {
      applyState(event.data.split(','));
      return;
    }
    if (event.data.startsWith('OK')) {
      const latency = pendingSentAt ? Math.max(0, Math.round(performance.now() - pendingSentAt)) : 0;
      pendingSentAt = 0;
      setStatus('Applied' + (latency ? ' · ' + latency + ' ms' : ''), 'connected');
      return;
    }
    setStatus(event.data.replace('ERR,', 'Not applied: '), 'error');
  };

  socket.onclose = () => {
    setConnected(false);
    setStatus('Disconnected; reconnecting…', 'error');
    reconnectTimer = setTimeout(connectSocket, reconnectDelay);
    reconnectDelay = Math.min(reconnectDelay * 2, 4000);
  };

  socket.onerror = () => socket.close();
}

function send(command) {
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    setStatus('Control socket is not connected', 'error');
    return false;
  }
  pendingSentAt = performance.now();
  setStatus('Applying…');
  socket.send(command);
  return true;
}

function sendColor() {
  const rgb = hexToRgb(document.getElementById('color').value);
  send('S,' + rgb.join(','));
}

function sendScrollText() {
  const msg = document.getElementById('scrollMsg').value.trim();
  if (!msg) { setStatus('Type a message first', 'error'); return; }
  send('T,' + msg);
}

document.getElementById('speed').addEventListener('input', event => {
  document.getElementById('speedLbl').textContent = event.target.value;
  clearTimeout(speedTimer);
  speedTimer = setTimeout(() => send('A,' + activeTheme + ',' + event.target.value), 120);
});
document.getElementById('brightness').addEventListener('input', event => {
  document.getElementById('brightLbl').textContent = event.target.value;
  clearTimeout(brightnessTimer);
  brightnessTimer = setTimeout(() => send('B,' + event.target.value), 120);
});
document.getElementById('color').addEventListener('change', sendColor);
document.querySelectorAll('.preset-btn').forEach(button => {
  button.addEventListener('click', () => send('S,' + button.dataset.color));
});
document.getElementById('modeManualBtn').addEventListener('click', () => send('D,0'));
document.getElementById('modeAutoBtn').addEventListener('click', () => send('D,1'));
document.getElementById('jumpBtn').addEventListener('click', () => send('J'));
document.getElementById('scrollBtn').addEventListener('click', sendScrollText);
document.getElementById('scrollMsg').addEventListener('keydown', event => {
  if (event.key === 'Enter') sendScrollText();
});
document.getElementById('blackoutBtn').addEventListener('click', () => send('X'));

setConnected(false);
connectSocket();
</script>
</body>
</html>
)rawliteral";

// HTTP is used only once to load the dashboard. All controls use the
// persistent WebSocket on port 81.
void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

bool isSupportedTheme(int mode) {
  return (mode >= 0 && mode <= 4) || (mode >= 7 && mode <= 9) ||
         mode == 11 || mode == 12 || mode == 13;
}

bool parseInteger(const char* text, int minimum, int maximum, int& result) {
  if (text == nullptr || *text == '\0') return false;
  char* end = nullptr;
  long value = strtol(text, &end, 10);
  if (*end != '\0' || value < minimum || value > maximum) return false;
  result = (int)value;
  return true;
}

void sendUart(const char* message, bool waitForTransmit = false) {
  xSemaphoreTake(uartMutex, portMAX_DELAY);
  teensySerial.print(message);
  if (waitForTransmit) teensySerial.flush();
  xSemaphoreGive(uartMutex);
}

void sendSocketError(uint8_t client, const char* reason) {
  char response[80];
  snprintf(response, sizeof(response), "ERR,%s", reason);
  webSocket.sendTXT(client, response);
}

void sendSocketState(uint8_t client) {
  uint8_t theme;
  uint8_t speed;
  uint8_t brightness;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t playMode;
  portENTER_CRITICAL(&stateMux);
  theme = activeThemeID;
  speed = activeSpeed;
  brightness = activeBrightness;
  red = activeColorR;
  green = activeColorG;
  blue = activeColorB;
  playMode = dinoPlayMode;
  portEXIT_CRITICAL(&stateMux);

  char response[48];
  snprintf(response, sizeof(response), "STATE,%u,%u,%u,%u,%u,%u,%u",
           theme, speed, brightness, red, green, blue, playMode);
  webSocket.sendTXT(client, response);
}

void applyWebSocketCommand(uint8_t client, const uint8_t* payload, size_t length) {
  if (length == 0 || length > MAX_WS_COMMAND_LENGTH) {
    sendSocketError(client, "invalid-length");
    return;
  }

  char command[MAX_WS_COMMAND_LENGTH + 1];
  memcpy(command, payload, length);
  command[length] = '\0';

  char* args = strchr(command, ',');
  if (args != nullptr) *args++ = '\0';

  char uartMessage[128];
  bool accepted = false;

  if (strcmp(command, "A") == 0 && args != nullptr) {
    char* separator = strchr(args, ',');
    if (separator != nullptr) {
      *separator++ = '\0';
      int mode;
      int speed;
      if (parseInteger(args, 0, 13, mode) && isSupportedTheme(mode) &&
          parseInteger(separator, 1, 10, speed)) {
        portENTER_CRITICAL(&stateMux);
        if (activeThemeID != mode) resetGame = true;
        activeThemeID = (uint8_t)mode;
        activeSpeed = (uint8_t)speed;
        portEXIT_CRITICAL(&stateMux);
        snprintf(uartMessage, sizeof(uartMessage), "<A,%d,%d>\n", mode, speed);
        sendUart(uartMessage);
        accepted = true;
      }
    }
  } else if (strcmp(command, "S") == 0 && args != nullptr) {
    int rgb[3];
    char* save = nullptr;
    char* token = strtok_r(args, ",", &save);
    bool valid = true;
    for (int i = 0; i < 3; ++i) {
      if (token == nullptr || !parseInteger(token, 0, 255, rgb[i])) {
        valid = false;
        break;
      }
      token = strtok_r(nullptr, ",", &save);
    }
    if (valid && token == nullptr) {
      portENTER_CRITICAL(&stateMux);
      activeColorR = (uint8_t)rgb[0];
      activeColorG = (uint8_t)rgb[1];
      activeColorB = (uint8_t)rgb[2];
      portEXIT_CRITICAL(&stateMux);
      snprintf(uartMessage, sizeof(uartMessage), "<S,%d,%d,%d>\n", rgb[0], rgb[1], rgb[2]);
      sendUart(uartMessage);
      accepted = true;
    }
  } else if (strcmp(command, "B") == 0 && args != nullptr) {
    int brightness;
    if (parseInteger(args, 0, MAX_SAFE_BRIGHTNESS, brightness)) {
      portENTER_CRITICAL(&stateMux);
      activeBrightness = (uint8_t)brightness;
      portEXIT_CRITICAL(&stateMux);
      snprintf(uartMessage, sizeof(uartMessage), "<B,%d>\n", brightness);
      sendUart(uartMessage);
      accepted = true;
    }
  } else if (strcmp(command, "T") == 0 && args != nullptr) {
    char safeText[61];
    size_t outputLength = 0;
    for (size_t i = 0; args[i] != '\0' && outputLength < 60; ++i) {
      char c = args[i];
      if (c >= 'a' && c <= 'z') c -= 32;
      bool supported = c == ' ' || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '.' || c == ',' ||
                       c == '!' || c == '?' || c == ':' || c == '-' || c == '\'';
      safeText[outputLength++] = supported ? c : ' ';
    }
    safeText[outputLength] = '\0';
    if (outputLength > 0) {
      uint8_t speed;
      portENTER_CRITICAL(&stateMux);
      activeThemeID = 11;
      speed = activeSpeed;
      resetGame = true;
      portEXIT_CRITICAL(&stateMux);
      snprintf(uartMessage, sizeof(uartMessage), "<A,11,%u>\n<T,%s>\n", speed, safeText);
      sendUart(uartMessage);
      accepted = true;
    }
  } else if (strcmp(command, "D") == 0 && args != nullptr) {
    int mode;
    if (parseInteger(args, 0, 1, mode)) {
      portENTER_CRITICAL(&stateMux);
      dinoPlayMode = (uint8_t)mode;
      resetGame = true;
      manualJump = false;
      portEXIT_CRITICAL(&stateMux);
      accepted = true;
    }
  } else if (strcmp(command, "J") == 0 && args == nullptr) {
    portENTER_CRITICAL(&stateMux);
    if (dinoPlayMode == 0) manualJump = true;
    portEXIT_CRITICAL(&stateMux);
    accepted = true;
  } else if (strcmp(command, "X") == 0 && args == nullptr) {
    portENTER_CRITICAL(&stateMux);
    activeThemeID = 0;
    activeColorR = 0;
    activeColorG = 0;
    activeColorB = 0;
    resetGame = true;
    manualJump = false;
    portEXIT_CRITICAL(&stateMux);
    // Phase 7's Teensy <X> handler only clears its drawing buffer and the
    // active renderer immediately paints over it. Selecting static mode and
    // setting black produces a persistent blackout without reflashing Teensy.
    sendUart("<A,0,3>\n<S,0,0,0>\n", true);
    accepted = true;
  }

  if (accepted) {
    webSocket.sendTXT(client, "OK");
    sendSocketState(client);
  } else {
    sendSocketError(client, "invalid-command");
  }
}

void webSocketEvent(uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      sendSocketState(client);
      break;
    case WStype_TEXT:
      applyWebSocketCommand(client, payload, length);
      break;
    default:
      break;
  }
}

// =========================================================
// 🧠 CORE 0: NETWORK & OTA TASK
// =========================================================
void TaskCore0_Network(void *pvParameters) {
  for (;;) {
    ArduinoOTA.handle();
    server.handleClient();
    webSocket.loop();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// =========================================================
// ⚡ CORE 1: GAME PHYSICS ENGINE
// =========================================================
void TaskCore1_GameEngine(void *pvParameters) {
  uint32_t lastFrameMicros = micros();
  TickType_t lastWakeTime = xTaskGetTickCount();

  float pacmanPos = 0;
  
  // Dino Game Physics & State
  float dinoY = 0;
  float dinoVy = 0;
  bool isJumping = false;
  float cactusPos[3] = { -10.0f, -170.0f, -340.0f };
  
  int dinoScore = 0;
  int dinoLives = 3;
  float scoreAccum = 0;
  int dinoState = 0; // 0=PLAYING, 1=CRASHED, 2=GAME_OVER
  float stateTimer = 0;

  for (;;) {
    uint32_t now = micros();
    float dt = (now - lastFrameMicros) / 1000000.0f;
    lastFrameMicros = now;

    bool shouldReset;
    bool jumpRequested;
    uint8_t mode;
    uint8_t speed;
    uint8_t playMode;
    portENTER_CRITICAL(&stateMux);
    shouldReset = resetGame;
    resetGame = false;
    jumpRequested = manualJump;
    manualJump = false;
    mode = activeThemeID;
    speed = activeSpeed;
    playMode = dinoPlayMode;
    portEXIT_CRITICAL(&stateMux);

    if (shouldReset) {
      pacmanPos = -16.0f;
      dinoY = 0.0f;
      dinoVy = 0.0f;
      isJumping = false;
      cactusPos[0] = -10.0f;
      cactusPos[1] = cactusPos[0] - (float)random(120, 250);
      cactusPos[2] = cactusPos[1] - (float)random(120, 250);
      dinoScore = 0;
      dinoLives = 3;
      scoreAccum = 0;
      dinoState = 0;
    }

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

      if (playMode == 1) {
        // Full Auto Mode: infinite loop with auto-jump. Foreground color stays
        // under explicit dashboard control instead of changing behind the user.
        scoreAccum += dt * (gameSpeed / 10.0f);
        if (scoreAccum >= 1.0f) {
            dinoScore += (int)scoreAccum;
            scoreAccum -= (int)scoreAccum;
        }

        for (int i = 0; i < 3; i++) {
          cactusPos[i] += dt * gameSpeed;
          if (cactusPos[i] > 518.0f) {
            float minC = 0.0f;
            for (int j = 0; j < 3; j++) { if (cactusPos[j] < minC) minC = cactusPos[j]; }
            cactusPos[i] = minC - (float)random(100, 280);
          }
        }

        if (!isJumping) {
          float jumpTriggerDist = gameSpeed * 0.42f; 
          for (int i = 0; i < 3; i++) {
            if (cactusPos[i] < DINO_X_POSITION &&
                cactusPos[i] > DINO_X_POSITION - jumpTriggerDist) {
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
            cactusPos[i] += dt * gameSpeed;
            if (cactusPos[i] > 518.0f) {
              float minC = 0.0f;
              for (int j = 0; j < 3; j++) { if (cactusPos[j] < minC) minC = cactusPos[j]; }
              cactusPos[i] = minC - (float)random(100, 280);
            }
          }

          bool hit = false;
          for (int i = 0; i < 3; i++) {
            if (cactusPos[i] < DINO_X_POSITION + 10.0f &&
                cactusPos[i] > DINO_X_POSITION - 10.0f) {
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
              if (jumpRequested) {
                isJumping = true;
                dinoVy = 45.0f; 
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
          }
        } 
        else if (dinoState == 1) { 
          stateTimer -= dt;
          if (stateTimer <= 0) {
            cactusPos[0] = -10.0f; cactusPos[1] = -170.0f; cactusPos[2] = -340.0f;
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
            cactusPos[0] = -10.0f; cactusPos[1] = -170.0f; cactusPos[2] = -340.0f;
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

    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(16));
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
  // Favor control latency over ESP32 power consumption.
  WiFi.setSleep(false);

  ArduinoOTA.setHostname("TinkerMatrix-ESP32");
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  webSocket.enableHeartbeat(15000, 3000, 2);
  
  xTaskCreatePinnedToCore(TaskCore0_Network, "NetworkTask", 8192, NULL, 1, &NetworkTaskHandle, 0);
  xTaskCreatePinnedToCore(TaskCore1_GameEngine, "GameTask", 8192, NULL, 2, &GameTaskHandle, 1);

  // 🚀 Startup Welcome Text Sequence
  delay(500);
  sendUart("<A,11,3>\n<T,WELCOME TO TINKERSPACE>\n", true);
}

void loop() {
  vTaskDelete(NULL); 
}

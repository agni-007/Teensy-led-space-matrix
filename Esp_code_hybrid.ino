#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "RGB_Control";
const char* password = "rgb12345";

WebServer server(80);
HardwareSerial mySerial(1);

String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>92-LED Advanced FX Deck</title>
    <style>
        body { text-align:center; font-family:sans-serif; background-color: #121214; color: #f5f5f5; padding: 20px; }
        .container { max-width: 450px; margin: 0 auto; background: #1a1a24; padding: 25px; border-radius: 15px; box-shadow: 0 4px 20px rgba(0,0,0,0.5); }
        input[type="color"] { width: 100px; height: 100px; border: none; border-radius: 50%; cursor: pointer; background: none; margin: 15px 0; }
        select { width: 100%; padding: 14px; border-radius: 6px; background: #252535; color: white; border: 1px solid #00adb5; font-size: 16px; margin-top: 10px; cursor: pointer; }
        button { background-color: #00adb5; color: white; border: none; padding: 14px; font-size: 16px; border-radius: 6px; cursor: pointer; margin-top: 15px; width: 100%; font-weight: bold; }
        button:hover { background-color: #007a80; }
        .section-title { margin-top: 25px; font-size: 13px; color: #00adb5; text-transform: uppercase; text-align: left; letter-spacing: 1.5px; font-weight: bold;}
        #status { margin-top: 20px; font-weight: bold; color: #00adb5; }
    </style>
</head>
<body>
<div class="container">
    <h2>92-LED Custom FX Suite</h2>
    
    <div class="section-title">Select Visual Theme</div>
    <select id="animMode" onchange="sendAnimation()">
        <option value="0">Static Mode (Custom Master Color)</option>
        <option value="1">Fast Travel Pulse</option>
        <option value="2">Dual Collision</option>
        <option value="3">Hyper Flash Strobe</option>
        <option value="4">Hyper-Drive Rainbow Chase</option>
        <option value="5">Cyberpunk Neon Pulse</option>
        <option value="6">Emergency Beacon Sweeper</option>
        <option value="7">Meteor Rain with Tail</option>
    </select>

    <div class="section-title">Global Override Color</div>
    <input type="color" id="masterPicker" value="#00adb5">
    <button onclick="sendMasterColor()">Set Entire Strip</button>
    <p id="status">System Ready</p>
</div>

<script>
function hexToRgb(hex) {
  return {
    r: parseInt(hex.substr(1,2), 16),
    g: parseInt(hex.substr(3,2), 16),
    b: parseInt(hex.substr(5,2), 16)
  };
}

function sendAnimation() {
  let mode = document.getElementById("animMode").value;
  document.getElementById("status").innerText = "Deploying theme...";
  fetch(`/setAnim?mode=${mode}`)
    .then(res => res.text())
    .then(t => document.getElementById("status").innerText = t);
}

function sendMasterColor() {
  document.getElementById("animMode").value = "0"; 
  document.getElementById("status").innerText = "Syncing static frame...";
  
  let hex = document.getElementById("masterPicker").value;
  let rgb = hexToRgb(hex);
  
  fetch(`/set?r=${rgb.r}&g=${rgb.g}&b=${rgb.b}`)
    .then(res => res.text())
    .then(t => document.getElementById("status").innerText = t);
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", page); }

void handleSet() {
  String msg = "<S," + server.arg("r") + "," + server.arg("g") + "," + server.arg("b") + ">\n";
  mySerial.print(msg); mySerial.flush();      
  server.send(200, "text/plain", "Static color active!");
}

void handleSetAnim() {
  String mode = server.arg("mode");
  String msg = "<A," + mode + ">\n";
  mySerial.print(msg); mySerial.flush();
  server.send(200, "text/plain", "Theme initiated!");
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  mySerial.begin(115200, SERIAL_8N1, 4, 5); 
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/setAnim", handleSetAnim);
  server.begin();
  Serial.println("Advanced Suite Master Gateway Online.");
}
void loop() { server.handleClient(); }
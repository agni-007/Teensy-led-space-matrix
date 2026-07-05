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
    <title>Advanced LED Controller</title>
    <style>
        body { text-align:center; font-family:sans-serif; background-color: #1e1e24; color: #f5f5f5; padding: 20px; }
        .container { max-width: 450px; margin: 0 auto; background: #2a2a35; padding: 25px; border-radius: 15px; box-shadow: 0 4px 15px rgba(0,0,0,0.3); }
        .led-row { display: flex; align-items: center; justify-content: space-between; margin: 12px 0; background: #333344; padding: 10px 20px; border-radius: 8px; }
        .led-label { font-size: 16px; font-weight: bold; }
        input[type="color"] { width: 45px; height: 45px; border: none; border-radius: 50%; cursor: pointer; background: none; }
        select { width: 100%; padding: 12px; border-radius: 5px; background: #333344; color: white; border: 1px solid #00adb5; font-size: 16px; margin-top: 10px; }
        button { background-color: #00adb5; color: white; border: none; padding: 14px; font-size: 16px; border-radius: 5px; cursor: pointer; margin-top: 15px; width: 100%; font-weight: bold; }
        button:hover { background-color: #007a80; }
        .section-title { margin-top: 20px; font-size: 14px; color: #a0a0b0; text-transform: uppercase; text-align: left; letter-spacing: 1px;}
        #status { margin-top: 15px; font-weight: bold; color: #00adb5; }
    </style>
</head>
<body>
<div class="container">
    <h2>Dynamic LED Control</h2>
    
    <div class="section-title">FX Animations</div>
    <select id="animMode" onchange="sendAnimation()">
        <option value="0">Static (Use Manual Pickers Below)</option>
        <option value="1">Fast Travel (Pulse Link)</option>
        <option value="2">Dual Collision (Both Sides)</option>
        <option value="3">Hyper Flash Strobe</option>
    </select>

    <div class="section-title">Manual Color Selection</div>
    <div class="led-row"><span class="led-label">LED #1</span><input type="color" id="p0" value="#ff0000"></div>
    <div class="led-row"><span class="led-label">LED #2</span><input type="color" id="p1" value="#ffffff"></div>
    <div class="led-row"><span class="led-label">LED #3</span><input type="color" id="p2" value="#00ffff"></div>
    <div class="led-row"><span class="led-label">LED #4</span><input type="color" id="p3" value="#ff00ff"></div>
    <div class="led-row"><span class="led-label">LED #5</span><input type="color" id="p4" value="#000000"></div>
    
    <button onclick="sendAllColors()">Update Static Colors</button>
    <p id="status">Ready</p>
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
  document.getElementById("status").innerText = "Switching FX Mode...";
  
  fetch(`/setAnim?mode=${mode}`)
    .then(res => res.text())
    .then(t => document.getElementById("status").innerText = t)
    .catch(e => document.getElementById("status").innerText = "Error: " + e);
}

function sendAllColors() {
  // Force dropdown reset to Static Mode when user updates custom colors
  document.getElementById("animMode").value = "0";
  document.getElementById("status").innerText = "Sending static frame...";
  
  let queryParts = [];
  for(let i=0; i<5; i++) {
    let hex = document.getElementById("p" + i).value;
    let rgb = hexToRgb(hex);
    queryParts.push(`r${i}=${rgb.r}&g${i}=${rgb.g}&b${i}=${rgb.b}`);
  }
  
  let queryString = "/set?" + queryParts.join("&");
  fetch(queryString)
    .then(res => res.text())
    .then(t => document.getElementById("status").innerText = t);
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", page);
}

void handleSet() {
  String msg = "<S,"; // 'S' prefix for Static
  for (int i = 0; i < 5; i++) {
    msg += server.arg("r" + String(i)) + ",";
    msg += server.arg("g" + String(i)) + ",";
    msg += server.arg("b" + String(i));
    if (i < 4) msg += ",";
  }
  msg += ">\n";

  mySerial.print(msg);   
  mySerial.flush();      
  server.send(200, "text/plain", "Static frame updated!");
}

void handleSetAnim() {
  String mode = server.arg("mode");
  String msg = "<A," + mode + ">\n"; // 'A' prefix for Animation Mode Switch

  mySerial.print(msg);
  mySerial.flush();
  server.send(200, "text/plain", "Animation mode triggered!");
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  mySerial.begin(115200, SERIAL_8N1, 4, 5); 
  
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/setAnim", handleSetAnim);
  server.begin();
  Serial.println("Advanced FX Master Operational.");
}

void loop() {
  server.handleClient();
}

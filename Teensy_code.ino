#include <Adafruit_NeoPixel.h>

#define LED_PIN     14        
#define NUM_LEDS    5         

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Packet Variables
int rgbValues[15]; 
int valueIndex = 0;
bool recvInProgress = false;
String currentNumber = "";
char packetType = 'S'; // 'S' = Static, 'A' = Animation Mode

// Animation Global State Machine
int activeAnimation = 0;       // 0=Static, 1=Travel, 2=Collision, 3=Strobe
unsigned long lastUpdate = 0;  // Tracks clock steps
int animFrame = 0;             // Tracking parameter inside specific loops
bool animDirection = true;

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);      

  strip.begin();
  strip.setBrightness(128);   
  strip.show();             
  Serial.println("Teensy 4.1 Animation Engine Online.");
}

void loop() {
  // 1. READ SERIAL (Always prioritized, runs instantaneously)
  while (Serial1.available() > 0) {
    char inChar = (char)Serial1.read();

    if (inChar == '<') {
      recvInProgress = true;
      valueIndex = 0;
      currentNumber = "";
      packetType = '\0'; // Clear old flag
      continue;
    }

    if (recvInProgress) {
      // Isolate the prefix flag first
      if (packetType == '\0') {
        packetType = inChar;
        continue;
      }
      
      // Skip structural identifier commas right after prefixes
      if (packetType != '\0' && currentNumber.length() == 0 && inChar == ',') {
        continue;
      }

      if (inChar == ',' || inChar == '>') {
        if (currentNumber.length() > 0) {
          if (packetType == 'S' && valueIndex < 15) {
            rgbValues[valueIndex] = currentNumber.toInt();
            valueIndex++;
          } else if (packetType == 'A') {
            activeAnimation = currentNumber.toInt();
            Serial.printf("Changed active FX to mode: %d\n", activeAnimation);
          }
        }
        currentNumber = ""; 
        
        if (inChar == '>') {
          recvInProgress = false;
          if (packetType == 'S') {
            activeAnimation = 0; // Drop back into manual static mode instantly
            renderStaticFrame();
          }
        }
      } 
      else if (isDigit(inChar)) {
        currentNumber += inChar;
      }
    }
  }

  // 2. RUN ANIMATION LOOPS (If an animation is active, step the frames asynchronously)
  if (activeAnimation > 0) {
    runActiveFxEngine();
  }
}

// --- RENDERING MODULES ---

void renderStaticFrame() {
  if (valueIndex == 15) {
    for (int i = 0; i < NUM_LEDS; i++) {
      int baseIdx = i * 3;
      strip.setPixelColor(i, strip.Color(rgbValues[baseIdx], rgbValues[baseIdx+1], rgbValues[baseIdx+2]));
    }
    strip.show();
  }
}

void runActiveFxEngine() {
  unsigned long now = millis();
  
  switch(activeAnimation) {
    
    // EFFECT 1: Fast Travelling Single Pulse
    case 1:
      if (now - lastUpdate > 60) { // Steps every 60ms (Fast)
        lastUpdate = now;
        strip.clear();
        // Light up one moving pixel using vibrant White-Cyan
        strip.setPixelColor(animFrame, strip.Color(255, 255, 255)); 
        if (animFrame > 0) strip.setPixelColor(animFrame - 1, strip.Color(0, 150, 200)); // Tail trailing effect
        strip.show();
        
        animFrame++;
        if (animFrame >= NUM_LEDS) animFrame = 0; // Loop seamlessly
      }
      break;

    // EFFECT 2: Dual Collision (Coming from both sides into the middle)
    case 2:
      if (now - lastUpdate > 90) { 
        lastUpdate = now;
        strip.clear();
        
        // Match outer pixels toward the interior center
        if (animFrame == 0) {
          strip.setPixelColor(0, strip.Color(255, 0, 100)); // Red-pink flash at bounds
          strip.setPixelColor(4, strip.Color(255, 0, 100));
        } else if (animFrame == 1) {
          strip.setPixelColor(1, strip.Color(0, 255, 100)); // Green flash closer in
          strip.setPixelColor(3, strip.Color(0, 255, 100));
        } else if (animFrame == 2) {
          strip.setPixelColor(2, strip.Color(255, 255, 255)); // Collision impact center white flash
        }
        strip.show();
        
        animFrame++;
        if (animFrame > 2) animFrame = 0; // Reset compression sequence
      }
      break;

    // EFFECT 3: Hyper Flash Strobe
    case 3:
      if (now - lastUpdate > 30) { // Blazing fast 30ms flips
        lastUpdate = now;
        strip.clear();
        if (animDirection) {
          // Alternative flashy colors across the whole group
          for(int i=0; i<NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(255, 200, 0)); // Gold bright flash
        } else {
          for(int i=0; i<NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(150, 0, 255)); // Deep violet flip
        }
        strip.show();
        animDirection = !animDirection; // Toggle flash back-and-forth
      }
      break;
  }
}
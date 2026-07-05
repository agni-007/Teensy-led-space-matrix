#include <Adafruit_NeoPixel.h>

#define LED_PIN     14        
#define NUM_LEDS    92        

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

int rgbValues[3]; 
int valueIndex = 0;
bool recvInProgress = false;
String currentNumber = "";
char packetType = 'S'; 

int activeAnimation = 0;       
unsigned long lastUpdate = 0;  
int animFrame = 0;             
bool animDirection = true;

// Helper function to generate smooth math-based colors (Wheeling)
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);      
  strip.begin();
  strip.setBrightness(64);   // Maintain safe power limit
  strip.show();             
  Serial.println("[SYSTEM RUNNING] Teensy Custom 7-Theme Driver Active.");
}

void loop() {
  while (Serial1.available() > 0) {
    char inChar = (char)Serial1.read();
    if (inChar == '<') {
      recvInProgress = true; valueIndex = 0; currentNumber = ""; packetType = '\0'; 
      continue;
    }
    if (recvInProgress) {
      if (packetType == '\0') { packetType = inChar; continue; }
      if (packetType != '\0' && currentNumber.length() == 0 && inChar == ',') continue;

      if (inChar == ',' || inChar == '>') {
        if (currentNumber.length() > 0) {
          if (packetType == 'S' && valueIndex < 3) {
            rgbValues[valueIndex] = currentNumber.toInt(); valueIndex++;
          } else if (packetType == 'A') {
            activeAnimation = currentNumber.toInt();
            Serial.printf("[SYSTEM] Launching Theme Mode: %d\n", activeAnimation);
            animFrame = 0; 
          }
        }
        currentNumber = ""; 
        if (inChar == '>') {
          recvInProgress = false;
          if (packetType == 'S') { activeAnimation = 0; renderStaticFrame(); }
        }
      } 
      else if (isDigit(inChar)) { currentNumber += inChar; }
    }
  }

  if (activeAnimation > 0) { runActiveFxEngine(); }
}

void renderStaticFrame() {
  if (valueIndex == 3) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(rgbValues[0], rgbValues[1], rgbValues[2]));
    }
    strip.show();
    delay(1); 
  }
}

void runActiveFxEngine() {
  unsigned long now = millis();
  
  switch(activeAnimation) {
    
    // THEME 1: Fast Travelling Single Pulse
    case 1:
      if (now - lastUpdate > 15) { 
        lastUpdate = now; strip.clear();
        strip.setPixelColor(animFrame, strip.Color(255, 255, 255)); 
        for(int t=1; t<=6; t++) {
          if(animFrame - t >= 0) strip.setPixelColor(animFrame - t, strip.Color(0, 180/t, 255/t));
        }
        strip.show();
        animFrame = (animFrame + 1) % NUM_LEDS;
      }
      break;

    // THEME 2: Dual Collision
    case 2:
      if (now - lastUpdate > 20) { 
        lastUpdate = now; strip.clear();
        int mirrorPin = (NUM_LEDS - 1) - animFrame;
        strip.setPixelColor(animFrame, strip.Color(255, 0, 120));
        strip.setPixelColor(mirrorPin, strip.Color(255, 0, 120));
        strip.show();
        animFrame++;
        if (animFrame >= NUM_LEDS / 2) {
          for(int flash=0; flash<4; flash++) {
            for(int i=(NUM_LEDS/2)-6; i<=(NUM_LEDS/2)+6; i++) strip.setPixelColor(i, strip.Color(255,255,255));
            strip.show(); delay(12); strip.clear(); strip.show(); delay(12);
          }
          animFrame = 0; 
        }
      }
      break;

    // THEME 3: Hyper Flash Strobe
    case 3:
      if (now - lastUpdate > 35) { 
        lastUpdate = now; strip.clear();
        for(int i=0; i<NUM_LEDS; i++) {
          strip.setPixelColor(i, animDirection ? strip.Color(0, 255, 150) : strip.Color(200, 0, 255));
        }
        strip.show(); animDirection = !animDirection; 
      }
      break;

    // THEME 4: Hyper-Drive Rainbow Chase (Algorithmic Spectrum Wave)
    case 4:
      if (now - lastUpdate > 8) { // Blazing fast spectrum cycle
        lastUpdate = now;
        for(int i=0; i<NUM_LEDS; i++) {
          // Generates an offset wave phase mapped sequentially over the index
          strip.setPixelColor(i, Wheel(((i * 256 / NUM_LEDS) + animFrame) & 255));
        }
        strip.show();
        animFrame = (animFrame + 4) & 255; // Step frequency size multiplier
      }
      break;

    // THEME 5: Cyberpunk Neon Pulse (Alternating 8-pixel blocks shifting down)
    case 5:
      if (now - lastUpdate > 40) {
        lastUpdate = now;
        for (int i = 0; i < NUM_LEDS; i++) {
          // Math block sizing logic toggles neon cyan vs magenta
          if (((i + animFrame) % 16) < 8) {
            strip.setPixelColor(i, strip.Color(0, 240, 255)); // Neon Cyan
          } else {
            strip.setPixelColor(i, strip.Color(255, 0, 150)); // Neon Magenta
          }
        }
        strip.show();
        animFrame = (animFrame + 1) % 16;
      }
      break;

    // THEME 6: Emergency Beacon Sweeper (Two interlocking waves crossing)
    case 6:
      if (now - lastUpdate > 15) {
        lastUpdate = now;
        strip.clear();
        // Compute triangle mapping waves across array positions
        int sweep1 = animFrame;
        int sweep2 = (NUM_LEDS - 1) - animFrame;
        
        for(int i=-4; i<=4; i++) {
          int p1 = sweep1 + i;
          int p2 = sweep2 + i;
          int intensity = (5 - abs(i)) * 40; // Peak intensity scaling in middle of sweep
          
          if(p1 >= 0 && p1 < NUM_LEDS) strip.setPixelColor(p1, strip.Color(255, intensity/4, 0)); // Intense Amber-Red
          if(p2 >= 0 && p2 < NUM_LEDS) strip.setPixelColor(p2, strip.Color(0, 0, 255));           // Intense Blue
        }
        strip.show();
        animFrame = (animFrame + 2) % NUM_LEDS;
      }
      break;

    // THEME 7: Meteor Rain (Chasing point leaving an organic decay trail)
    case 7:
      if (now - lastUpdate > 20) {
        lastUpdate = now;
        
        // Step through all pixels and dim them organically by 20% to leave a natural trail fade
        for(int i=0; i<NUM_LEDS; i++) {
          uint32_t c = strip.getPixelColor(i);
          byte r = (c >> 16) & 0xFF;
          byte g = (c >> 8) & 0xFF;
          byte b = c & 0xFF;
          strip.setPixelColor(i, strip.Color(r * 0.8, g * 0.8, b * 0.8));
        }
        
        // Inject a blazing bright head core
        if (random(10) > 2) { // 80% generation reliability to mimic sparkling tail disintegration
          strip.setPixelColor(animFrame, strip.Color(255, 255, 255));
          if(animFrame > 0) strip.setPixelColor(animFrame-1, strip.Color(255, 100, 0)); // Fire core glow
        }
        
        strip.show();
        animFrame = (animFrame + 1) % NUM_LEDS;
      }
      break;
  }
}

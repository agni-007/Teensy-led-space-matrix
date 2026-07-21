#include <ObjectFLED.h>
#include <FastLED.h>

#define NUM_ROWS      18
#define LEDS_PER_ROW  508
#define SERIAL_LINK   Serial1

// =========================================================
// 🛠️ MATRIX CONFIGURATION ZONE 
// =========================================================
#define INVERT_X_AXIS true  
#define INVERT_Y_AXIS false  

#define FONT_SCALE_X  2
#define FONT_SCALE_Y  2
#define GLYPH_W       5
#define GLYPH_H       7
#define CHAR_SPACING  1
#define TEXT_ROW_OFFSET 2    
// =========================================================

uint8_t rowPins[NUM_ROWS] = { 3, 5, 7, 9, 11, 24, 26, 28, 30, 32, 37, 39, 41, 14, 16, 18, 20, 22 };

float rowSpeedComp[NUM_ROWS] = {
  1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00,   // rows 0-6
  1.00, 1.00, 1.00, 1.00, 1.00,               // rows 7-11 <- tune these
  1.00, 1.00, 1.00, 1.00, 1.00, 1.00          // rows 12-17
};

CRGB canvas[NUM_ROWS][LEDS_PER_ROW];
ObjectFLED leds(NUM_ROWS * LEDS_PER_ROW, canvas, CORDER_GRB, NUM_ROWS, rowPins, 0);

enum ThemeID {
  THEME_STATIC      = 0,
  THEME_WIPE_L      = 1,
  THEME_WIPE_R      = 2,
  THEME_WIPE_T      = 3,
  THEME_WIPE_B      = 4,
  THEME_DUAL_H      = 7,
  THEME_DUAL_V      = 8,
  THEME_ALL_SIDES   = 9,
  THEME_TEXT_SCROLL = 11,
  THEME_PACMAN      = 12,
  THEME_DINO        = 13
};

ThemeID activeTheme = THEME_STATIC;
uint8_t fgR = 0, fgG = 200, fgB = 255;
uint8_t bright     = 35;
uint8_t themeSpeed = 3;

float    themeClock = 0;
uint32_t lastFrameMicros = 0;

String scrollText = "TINKERSPACE";
float  scrollPos  = 0;
const int CHAR_PITCH = (GLYPH_W + CHAR_SPACING) * FONT_SCALE_X;

// --- Pac-Man Variables ---
#define PACMAN_SCALE 2
float pacmanPos = 0;

const uint8_t PACMAN_CLOSED[8] = {
  0b01111000, 0b11111100, 0b11111111, 0b11111111,
  0b11111111, 0b11111111, 0b11111100, 0b01111000
};
const uint8_t PACMAN_OPEN[8] = {
  0b01111000, 0b11111000, 0b11110000, 0b11100000,
  0b11100000, 0b11110000, 0b11111000, 0b01111000
};

// --- Chrome Dino Variables ---
const float DINO_X_POS = 40.0f;
float dinoY      = 0.0f;  
float dinoVy     = 0.0f;
bool  isJumping  = false;
float cactusPos[3] = { LEDS_PER_ROW * 1.0f, LEDS_PER_ROW + 150.0f, LEDS_PER_ROW + 300.0f };

// 10x10 Dino Sprites (Stored in uint16_t, reading top 10 bits)
const uint16_t DINO_JUMP[10] = {
  0b0000111110, 0b0000101111, 0b0000111111, 0b0000111000, 0b1001111110,
  0b1111111110, 0b0111111000, 0b0001001000, 0b0001001000, 0b0011001100
};
const uint16_t DINO_RUN1[10] = {
  0b0000111110, 0b0000101111, 0b0000111111, 0b0000111000, 0b1001111110,
  0b1111111110, 0b0111111000, 0b0001000000, 0b0001101100, 0b0000000000
};
const uint16_t DINO_RUN2[10] = {
  0b0000111110, 0b0000101111, 0b0000111111, 0b0000111000, 0b1001111110,
  0b1111111110, 0b0111111000, 0b0000001000, 0b0011001000, 0b0000000000
};
const uint16_t DINO_CACTUS[10] = {
  0b0000110000, 0b0000110000, 0b0100110010, 0b0110110110, 0b0110110110,
  0b0011111100, 0b0000110000, 0b0000110000, 0b0000110000, 0b0000110000
};

char   packetType = '\0';
String currentToken = "";
bool   recvInProgress = false;
int    cmdValues[4];
int    cmdValueIndex = 0;

const uint8_t FONT_5x7[][7] = {
  {0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b00000}, // ' '
  {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}, // A
  {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}, // B
  {0b01111,0b10000,0b10000,0b10000,0b10000,0b10000,0b01111}, // C
  {0b11100,0b10010,0b10001,0b10001,0b10001,0b10010,0b11100}, // D
  {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111}, // E
  {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000}, // F
  {0b01111,0b10000,0b10000,0b10011,0b10001,0b10001,0b01111}, // G
  {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}, // H
  {0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110}, // I
  {0b00111,0b00010,0b00010,0b00010,0b00010,0b10010,0b01100}, // J
  {0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001}, // K
  {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111}, // L
  {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001}, // M
  {0b10001,0b11001,0b10101,0b10101,0b10011,0b10001,0b10001}, // N
  {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}, // O
  {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000}, // P
  {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101}, // Q
  {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001}, // R
  {0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110}, // S
  {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100}, // T
  {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}, // U
  {0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100}, // V
  {0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010}, // W
  {0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001}, // X
  {0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100}, // Y
  {0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111}, // Z
  {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110}, // 0
  {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110}, // 1
  {0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111}, // 2
  {0b11111,0b00010,0b00100,0b00010,0b00001,0b10001,0b01110}, // 3
  {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010}, // 4
  {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110}, // 5
  {0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110}, // 6
  {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000}, // 7
  {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110}, // 8
  {0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100}, // 9
  {0b00000,0b00000,0b00000,0b00000,0b00000,0b01100,0b01100}, // .
  {0b00000,0b00000,0b00000,0b00000,0b00000,0b01100,0b01000}, // ,
  {0b00100,0b00100,0b00100,0b00100,0b00100,0b00000,0b00100}, // !
  {0b01110,0b10001,0b00001,0b00010,0b00100,0b00000,0b00100}, // ?
  {0b00000,0b01100,0b01100,0b00000,0b01100,0b01100,0b00000}, // :
  {0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000}, // -
  {0b00100,0b00100,0b01000,0b00000,0b00000,0b00000,0b00000}, // '
};

char toUpperManual(char c) {
  if (c >= 'a' && c <= 'z') return c - 32;
  return c;
}

int fontIndex(char c) {
  c = toUpperManual(c);
  if (c == ' ') return 0;
  if (c >= 'A' && c <= 'Z') return 1 + (c - 'A');
  if (c >= '0' && c <= '9') return 27 + (c - '0');
  switch (c) {
    case '.':  return 37;
    case ',':  return 38;
    case '!':  return 39;
    case '?':  return 40;
    case ':':  return 41;
    case '-':  return 42;
    case '\'': return 43;
  }
  return 0;
}

void setup() {
  SERIAL_LINK.begin(115200);
  leds.begin();
  leds.setBrightness(bright);
  clearCanvas();
  leds.show();
}

void loop() {
  readSerial();

  uint32_t now = micros();
  float dt = (now - lastFrameMicros) / 1000000.0f;
  lastFrameMicros = now;

  float speedMult = 0.05f + (themeSpeed - 1) * (1.95f / 9.0f);
  themeClock += dt * speedMult;

  if (activeTheme == THEME_TEXT_SCROLL) {
    float scrollSpeedPxPerSec = 10.0f + (themeSpeed - 1) * (100.0f / 9.0f);
    scrollPos -= dt * scrollSpeedPxPerSec; 
    
    int textPixelWidth = scrollText.length() * CHAR_PITCH;
    if (scrollPos < -textPixelWidth) {
      scrollPos = LEDS_PER_ROW; 
    }
  }

  if (activeTheme == THEME_PACMAN) {
    float pacmanSpeedPxPerSec = 15.0f + (themeSpeed - 1) * (180.0f / 9.0f);
    pacmanPos += dt * pacmanSpeedPxPerSec;
    int spriteW = 8 * PACMAN_SCALE;
    if (pacmanPos > LEDS_PER_ROW) {
      pacmanPos = -spriteW; 
    }
  }

  if (activeTheme == THEME_DINO) {
    float gameSpeed = 50.0f + (themeSpeed - 1) * (180.0f / 9.0f);

    // 1. Move Cactuses
    for (int i = 0; i < 3; i++) {
      cactusPos[i] -= dt * gameSpeed;
      if (cactusPos[i] < -20.0f) {
        float maxC = LEDS_PER_ROW * 1.0f;
        for (int j = 0; j < 3; j++) { if (cactusPos[j] > maxC) maxC = cactusPos[j]; }
        cactusPos[i] = maxC + random(80, 250);
      }
    }

    // 2. Trigger Auto-Jump (perfectly timed distance based on speed)
    float jumpTriggerDist = gameSpeed * 0.40f; 
    if (!isJumping) {
      for (int i = 0; i < 3; i++) {
        // Triggers just as the cactus enters the danger zone
        if (cactusPos[i] > DINO_X_POS && cactusPos[i] < DINO_X_POS + jumpTriggerDist) {
          isJumping = true;
          dinoVy = 40.0f; // Initial vertical velocity
          break;
        }
      }
    }

    // 3. Gravity Physics
    if (isJumping) {
      dinoY += dinoVy * dt;
      dinoVy -= 100.0f * dt; // Gravity
      if (dinoY <= 0.0f) {
        dinoY = 0.0f;
        isJumping = false;
        dinoVy = 0.0f;
      }
    }
  }

  renderActiveTheme();
  leds.show();
}

void clearCanvas() {
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB::Black;
}

int wipeFront(int maxDist) {
  float period = 1.0f + (float)maxDist;
  float raw    = fmod(themeClock * maxDist, period);
  return (int)constrain(raw, 0, maxDist);
}

int wipeFrontForRow(int maxDist, int row) {
  float rowClock = themeClock * rowSpeedComp[row];
  float period    = 1.0f + (float)maxDist;
  float raw       = fmod(rowClock * maxDist, period);
  return (int)constrain(raw, 0, maxDist);
}

void readSerial() {
  while (SERIAL_LINK.available()) {
    char c = (char)SERIAL_LINK.read();

    if (c == '<') {
      recvInProgress = true;
      cmdValueIndex  = 0;
      currentToken   = "";
      packetType     = '\0';
      continue;
    }
    if (!recvInProgress) continue;

    if (packetType == '\0') { packetType = c; continue; }

    if (packetType == 'T') {
      if (c == '>') {
        scrollText = (currentToken.length() > 0) ? currentToken : " ";
        if (scrollText.length() > 60) scrollText = scrollText.substring(0, 60);
        currentToken = "";
        recvInProgress = false;
        applyCommand();
      } else if (c == ',' && currentToken.length() == 0) {
        continue;
      } else {
        if (currentToken.length() < 60) currentToken += c;
      }
      continue;
    }

    if (currentToken.length() == 0 && c == ',') continue;

    if (c == ',' || c == '>') {
      if (currentToken.length() > 0 && cmdValueIndex < 4)
        cmdValues[cmdValueIndex++] = currentToken.toInt();
      currentToken = "";
      if (c == '>') { recvInProgress = false; applyCommand(); }
    } else if (isDigit(c) || c == '-') {
      currentToken += c;
    }
  }
}

void applyCommand() {
  switch (packetType) {
    case 'A':
      if (cmdValueIndex >= 1) {
        // Upped max constraint to 13 for Dino Theme
        activeTheme = (ThemeID)constrain(cmdValues[0], 0, 13); 
        themeClock  = 0;
        
        if (activeTheme == THEME_TEXT_SCROLL) scrollPos = LEDS_PER_ROW; 
        if (activeTheme == THEME_PACMAN) pacmanPos = -(8 * PACMAN_SCALE);
        if (activeTheme == THEME_DINO) {
          dinoY = 0.0f;
          isJumping = false;
          cactusPos[0] = LEDS_PER_ROW * 1.0f;
          cactusPos[1] = LEDS_PER_ROW + 150.0f;
          cactusPos[2] = LEDS_PER_ROW + 300.0f;
        }
      }
      if (cmdValueIndex >= 2) themeSpeed = constrain(cmdValues[1], 1, 10);
      break;

    case 'S':
      if (cmdValueIndex >= 3) {
        fgR = cmdValues[0]; fgG = cmdValues[1]; fgB = cmdValues[2];
      }
      break;

    case 'B':
      if (cmdValueIndex >= 1) {
        bright = constrain(cmdValues[0], 0, 255);
        leds.setBrightness(bright);
      }
      break;

    case 'T':
      activeTheme = THEME_TEXT_SCROLL;
      scrollPos   = LEDS_PER_ROW; 
      break;

    case 'X':
      clearCanvas();
      leds.show();
      activeTheme = THEME_STATIC;
      fgR = 0; fgG = 0; fgB = 0;
      break;
  }
}

void renderActiveTheme() {
  switch (activeTheme) {
    case THEME_STATIC:      renderStatic();      break;
    case THEME_WIPE_L:      renderWipeL();       break;
    case THEME_WIPE_R:      renderWipeR();       break;
    case THEME_WIPE_T:      renderWipeT();       break;
    case THEME_WIPE_B:      renderWipeB();       break;
    case THEME_DUAL_H:      renderDualH();       break;
    case THEME_DUAL_V:      renderDualV();       break;
    case THEME_ALL_SIDES:   renderAllSides();    break;
    case THEME_TEXT_SCROLL: renderTextScroll();  break;
    case THEME_PACMAN:      renderPacman();      break;
    case THEME_DINO:        renderDino();        break;
  }
}

void renderStatic() {
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

void renderWipeL() {
  clearCanvas();
  for (int r = 0; r < NUM_ROWS; r++) {
    int front = wipeFrontForRow(LEDS_PER_ROW, r);
    for (int c = 0; c < front; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
  }
}

void renderWipeR() {
  clearCanvas();
  for (int r = 0; r < NUM_ROWS; r++) {
    int front = wipeFrontForRow(LEDS_PER_ROW, r);
    for (int c = LEDS_PER_ROW - front; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
  }
}

void renderWipeT() {
  clearCanvas();
  int front = wipeFront(NUM_ROWS);
  for (int r = 0; r < front; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

void renderWipeB() {
  clearCanvas();
  int front = wipeFront(NUM_ROWS);
  for (int r = NUM_ROWS - front; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

void renderDualH() {
  clearCanvas();
  int half = (LEDS_PER_ROW + 1) / 2;
  for (int r = 0; r < NUM_ROWS; r++) {
    int front = wipeFrontForRow(half, r);
    for (int c = 0; c < front; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
    for (int c = LEDS_PER_ROW - front; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
  }
}

void renderDualV() {
  clearCanvas();
  int half  = (NUM_ROWS + 1) / 2;
  int front = wipeFront(half);
  for (int c = 0; c < LEDS_PER_ROW; c++) {
    for (int r = 0; r < front; r++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
    for (int r = NUM_ROWS - front; r < NUM_ROWS; r++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
  }
}

int edgeDist(int r, int c) {
  return min(min(r, NUM_ROWS - 1 - r), min(c, LEDS_PER_ROW - 1 - c));
}

void renderAllSides() {
  clearCanvas();
  int maxRing = (min(NUM_ROWS, LEDS_PER_ROW) + 1) / 2;
  int front   = wipeFront(maxRing);
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      if (edgeDist(r, c) < front)
        canvas[r][c] = CRGB(fgR, fgG, fgB);
}

void renderTextScroll() {
  clearCanvas();
  int textPixelWidth = scrollText.length() * CHAR_PITCH;
  if (textPixelWidth <= 0) return;

  const int glyphPixelWidth  = GLYPH_W * FONT_SCALE_X; 
  const int glyphPixelHeight = GLYPH_H * FONT_SCALE_Y; 

  for (int c = 0; c < LEDS_PER_ROW; c++) {
    int logicalC = INVERT_X_AXIS ? (LEDS_PER_ROW - 1 - c) : c;
    int fontCol  = logicalC - (int)scrollPos;

    if (fontCol >= 0 && fontCol < textPixelWidth) {
      int charIdx   = fontCol / CHAR_PITCH;
      int colInChar = fontCol % CHAR_PITCH;

      if (colInChar < glyphPixelWidth) {
        int glyphIndex = fontIndex(scrollText.charAt(charIdx));
        int bitCol     = colInChar / FONT_SCALE_X; 

        for (int r = 0; r < glyphPixelHeight; r++) {
          int  bitRow = r / FONT_SCALE_Y; 
          bool lit    = (FONT_5x7[glyphIndex][bitRow] >> (4 - bitCol)) & 0x01;

          if (lit) {
            int logicalR = INVERT_Y_AXIS ? (glyphPixelHeight - 1 - r) : r;
            int finalRow = logicalR + TEXT_ROW_OFFSET;

            if (finalRow >= 0 && finalRow < NUM_ROWS) {
              canvas[finalRow][c] = CRGB(fgR, fgG, fgB);
            }
          }
        }
      }
    }
  }
}

void renderPacman() {
  clearCanvas();
  const uint8_t* frame = ((millis() / 150) % 2 == 0) ? PACMAN_OPEN : PACMAN_CLOSED;
  int spriteW    = 8 * PACMAN_SCALE;
  int spriteH    = 8 * PACMAN_SCALE;
  int rowOffset  = (NUM_ROWS - spriteH) / 2; 

  for (int sc = 0; sc < spriteW; sc++) {
    int c = (int)pacmanPos + sc;
    if (c < 0 || c >= LEDS_PER_ROW) continue;

    int bitCol = sc / PACMAN_SCALE; 
    for (int sr = 0; sr < spriteH; sr++) {
      int  bitRow = sr / PACMAN_SCALE;
      bool lit    = (frame[bitRow] >> (7 - bitCol)) & 0x01;

      if (lit) {
        int finalRow = rowOffset + sr;
        if (finalRow >= 0 && finalRow < NUM_ROWS) {
          canvas[finalRow][c] = CRGB(fgR, fgG, fgB);
        }
      }
    }
  }
}

void renderDino() {
  clearCanvas();

  // Draw the ground plane on row 17 (dimmed version of active color)
  CRGB groundColor = CRGB(fgR, fgG, fgB);
  groundColor.fadeToBlackBy(180);
  for (int c = 0; c < LEDS_PER_ROW; c++) {
    canvas[17][c] = groundColor;
  }

  // Draw Cactuses
  for (int i = 0; i < 3; i++) {
    int cx = (int)cactusPos[i];
    for (int sc = 0; sc < 10; sc++) {
      int c = cx + sc;
      if (c < 0 || c >= LEDS_PER_ROW) continue;
      
      for (int sr = 0; sr < 10; sr++) {
        // Bit logic reads the 10-bit block left to right
        bool lit = (DINO_CACTUS[sr] >> (9 - sc)) & 0x01;
        if (lit) {
          int r = 7 + sr; // Resting on row 16 (right above the floor)
          if (r >= 0 && r < NUM_ROWS) {
            canvas[r][c] = CRGB(fgR, fgG, fgB);
          }
        }
      }
    }
  }

  // Draw Dino (Jumping or Running animation)
  const uint16_t* dFrame;
  if (isJumping) {
    dFrame = DINO_JUMP;
  } else {
    dFrame = ((millis() / 100) % 2 == 0) ? DINO_RUN1 : DINO_RUN2;
  }

  int dx = (int)DINO_X_POS;
  int dy = (int)dinoY; // Subtract dy so he moves up the matrix

  for (int sc = 0; sc < 10; sc++) {
    int c = dx + sc;
    if (c < 0 || c >= LEDS_PER_ROW) continue;
    
    for (int sr = 0; sr < 10; sr++) {
      bool lit = (dFrame[sr] >> (9 - sc)) & 0x01;
      if (lit) {
        int r = 7 + sr - dy; // Resting on row 16 minus jump height
        if (r >= 0 && r < NUM_ROWS) {
          // Drawing after cactus ensures Dino renders *over* obstacles on overlap
          canvas[r][c] = CRGB(fgR, fgG, fgB);
        }
      }
    }
  }
}

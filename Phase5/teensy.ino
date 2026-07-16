#include <ObjectFLED.h>
#include <FastLED.h>

#define NUM_ROWS      18
#define LEDS_PER_ROW  508
#define SERIAL_LINK   Serial1

// 18 physical output pins for parallel data transmission
uint8_t rowPins[NUM_ROWS] = { 3, 5, 7, 9, 11, 24, 26, 28, 30, 32, 37, 39, 41, 14, 16, 18, 20, 22 };

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
  THEME_TEXT_SCROLL = 11
};

ThemeID activeTheme = THEME_STATIC;
uint8_t fgR = 0, fgG = 200, fgB = 255;
uint8_t bright     = 40; // Default lowered for massive power safety
uint8_t themeSpeed = 3;

float    themeClock = 0;
uint32_t lastFrameMicros = 0;

String scrollText = "TINKERSPACE";
float  scrollPos  = 0;
const int CHAR_PITCH = 6;

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
    scrollPos += dt * scrollSpeedPxPerSec;
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
        activeTheme = (ThemeID)constrain(cmdValues[0], 0, 11);
        themeClock  = 0;
        if (activeTheme == THEME_TEXT_SCROLL) scrollPos = -LEDS_PER_ROW;
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
      scrollPos   = -LEDS_PER_ROW;
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
  }
}

void renderStatic() {
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

void renderWipeL() {
  clearCanvas();
  int front = wipeFront(LEDS_PER_ROW);
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < front; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

void renderWipeR() {
  clearCanvas();
  int front = wipeFront(LEDS_PER_ROW);
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = LEDS_PER_ROW - front; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
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
  int half  = (LEDS_PER_ROW + 1) / 2;
  int front = wipeFront(half);
  for (int r = 0; r < NUM_ROWS; r++) {
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
  int loopWidth = textPixelWidth + LEDS_PER_ROW;

  // Center the 7-pixel high font on the 18-pixel high row
  int rowOffset = 5; 

  for (int c = 0; c < LEDS_PER_ROW; c++) {
    int x       = (int)scrollPos + c;
    int wrapped = x % loopWidth;
    if (wrapped < 0) wrapped += loopWidth;
    if (wrapped >= textPixelWidth) continue;

    int charIdx   = wrapped / CHAR_PITCH;
    int colInChar = wrapped % CHAR_PITCH;
    if (colInChar >= 5) continue;

    int glyphIndex = fontIndex(scrollText.charAt(charIdx));
    for (int r = 0; r < 7; r++) {
      bool lit = (FONT_5x7[glyphIndex][r] >> (4 - colInChar)) & 0x01;
      // Add the offset to position the text centrally 
      if (lit) canvas[r + rowOffset][c] = CRGB(fgR, fgG, fgB);
    }
  }
}

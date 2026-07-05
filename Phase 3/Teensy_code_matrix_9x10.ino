#include <ObjectFLED.h>
#include <FastLED.h>

#define NUM_ROWS      9
#define LEDS_PER_ROW  10
#define SERIAL_LINK   Serial1

// Pins in reverse order as per your wiring
uint8_t rowPins[NUM_ROWS] = { 30, 28, 26, 24, 11, 9, 7, 5, 3 };

CRGB canvas[NUM_ROWS][LEDS_PER_ROW];
ObjectFLED leds(NUM_ROWS * LEDS_PER_ROW, canvas, CORDER_GRB, NUM_ROWS, rowPins, 0);

// ====================== STATE ======================
enum ThemeID {
  THEME_STATIC    = 0,
  THEME_WIPE_L    = 1,
  THEME_WIPE_R    = 2,
  THEME_WIPE_T    = 3,
  THEME_WIPE_B    = 4,
  THEME_ROW_SCAN  = 5,
  THEME_COL_SCAN  = 6,
  THEME_DUAL_H    = 7,
  THEME_DUAL_V    = 8,
  THEME_ALL_SIDES = 9,
  THEME_PIXEL     = 10   // single pixel control from dashboard
};

ThemeID activeTheme = THEME_STATIC;
uint8_t fgR = 0, fgG = 200, fgB = 255;
uint8_t bright     = 80;   // 0-255
uint8_t themeSpeed = 3;    // 1-10, 1 = very slow (debug visible), 10 = fast

float   themeClock = 0;
uint32_t lastFrameMicros = 0;

// single pixel mode
int pixelRow = -1, pixelCol = -1;

// ====================== SERIAL PROTOCOL ======================
// <A,mode,speed>     switch theme
// <S,r,g,b>          set color
// <B,brightness>     set brightness 0-255
// <P,row,col>        light single pixel (mode 10)
// <X>                clear all (all black)
char   packetType = '\0';
String currentToken = "";
bool   recvInProgress = false;
int    cmdValues[4];
int    cmdValueIndex = 0;

// ====================== SETUP ======================
void setup() {
  Serial.begin(115200);
  SERIAL_LINK.begin(115200);
  leds.begin();
  leds.setBrightness(bright);
  clearCanvas();
  leds.show();
  Serial.println("Teensy matrix ready. 9x10 ObjectFLED parallel output.");
}

// ====================== MAIN LOOP ======================
void loop() {
  readSerial();

  uint32_t now = micros();
  float dt = (now - lastFrameMicros) / 1000000.0f;
  lastFrameMicros = now;

  // Speed mapping: speed 1 = 0.05x, speed 5 = 0.5x, speed 10 = 2x
  // Speed 1 is slow enough to watch individual steps for debug
  float speedMult = 0.05f + (themeSpeed - 1) * (1.95f / 9.0f);
  themeClock += dt * speedMult;

  renderActiveTheme();
  leds.show();
}

// ====================== UTILITIES ======================
void clearCanvas() {
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB::Black;
}

// wipeFront: grows 0 -> maxDist then holds 1 cycle before resetting
int wipeFront(int maxDist) {
  float period   = 1.0f + (float)maxDist;   // dist + 1 unit hold
  float raw      = fmod(themeClock * maxDist, period);
  return (int)constrain(raw, 0, maxDist);
}

// ====================== SERIAL PARSING ======================
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
        activeTheme = (ThemeID)constrain(cmdValues[0], 0, 10);
        themeClock  = 0;
      }
      if (cmdValueIndex >= 2)
        themeSpeed = constrain(cmdValues[1], 1, 10);
      Serial.printf("[CMD] Theme=%d Speed=%d\n", activeTheme, themeSpeed);
      break;

    case 'S':
      if (cmdValueIndex >= 3) {
        fgR = cmdValues[0]; fgG = cmdValues[1]; fgB = cmdValues[2];
      }
      Serial.printf("[CMD] Color=%d,%d,%d\n", fgR, fgG, fgB);
      break;

    case 'B':
      if (cmdValueIndex >= 1) {
        bright = constrain(cmdValues[0], 0, 255);
        leds.setBrightness(bright);
      }
      Serial.printf("[CMD] Brightness=%d\n", bright);
      break;

    case 'P':
      if (cmdValueIndex >= 2) {
        pixelRow    = constrain(cmdValues[0], 0, NUM_ROWS - 1);
        pixelCol    = constrain(cmdValues[1], 0, LEDS_PER_ROW - 1);
        activeTheme = THEME_PIXEL;
      }
      Serial.printf("[CMD] Pixel=%d,%d\n", pixelRow, pixelCol);
      break;

    case 'X':
      clearCanvas();
      leds.show();
      activeTheme = THEME_STATIC;
      fgR = 0; fgG = 0; fgB = 0;
      Serial.println("[CMD] Clear all");
      break;
  }
}

// ====================== RENDER DISPATCH ======================
void renderActiveTheme() {
  switch (activeTheme) {
    case THEME_STATIC:    renderStatic();    break;
    case THEME_WIPE_L:    renderWipeL();     break;
    case THEME_WIPE_R:    renderWipeR();     break;
    case THEME_WIPE_T:    renderWipeT();     break;
    case THEME_WIPE_B:    renderWipeB();     break;
    case THEME_ROW_SCAN:  renderRowScan();   break;
    case THEME_COL_SCAN:  renderColScan();   break;
    case THEME_DUAL_H:    renderDualH();     break;
    case THEME_DUAL_V:    renderDualV();     break;
    case THEME_ALL_SIDES: renderAllSides();  break;
    case THEME_PIXEL:     renderPixel();     break;
  }
}

// ====================== THEME 0: STATIC FILL ======================
void renderStatic() {
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

// ====================== THEME 1: WIPE FROM LEFT ======================
void renderWipeL() {
  clearCanvas();
  int front = wipeFront(LEDS_PER_ROW);
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < front; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

// ====================== THEME 2: WIPE FROM RIGHT ======================
void renderWipeR() {
  clearCanvas();
  int front = wipeFront(LEDS_PER_ROW);
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = LEDS_PER_ROW - front; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

// ====================== THEME 3: WIPE FROM TOP ======================
void renderWipeT() {
  clearCanvas();
  int front = wipeFront(NUM_ROWS);
  for (int r = 0; r < front; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

// ====================== THEME 4: WIPE FROM BOTTOM ======================
void renderWipeB() {
  clearCanvas();
  int front = wipeFront(NUM_ROWS);
  for (int r = NUM_ROWS - front; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB(fgR, fgG, fgB);
}

// ====================== THEME 5: ROW SCAN ======================
void renderRowScan() {
  clearCanvas();
  int row = ((int)(themeClock * NUM_ROWS)) % NUM_ROWS;
  for (int c = 0; c < LEDS_PER_ROW; c++)
    canvas[row][c] = CRGB(fgR, fgG, fgB);

  static int lastRow = -1;
  if (row != lastRow) {
    lastRow = row;
    Serial.printf("[ROW SCAN] Row %d — pin %d\n", row, rowPins[row]);
  }
}

// ====================== THEME 6: COLUMN SCAN ======================
void renderColScan() {
  clearCanvas();
  int col = ((int)(themeClock * LEDS_PER_ROW)) % LEDS_PER_ROW;
  for (int r = 0; r < NUM_ROWS; r++)
    canvas[r][col] = CRGB(fgR, fgG, fgB);

  static int lastCol = -1;
  if (col != lastCol) {
    lastCol = col;
    Serial.printf("[COL SCAN] Column %d\n", col);
  }
}

// ====================== THEME 7: DUAL HORIZONTAL (left+right meet center) ======================
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

// ====================== THEME 8: DUAL VERTICAL (top+bottom meet center) ======================
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

// ====================== THEME 9: ALL SIDES CONVERGE ======================
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

// ====================== THEME 10: SINGLE PIXEL ======================
void renderPixel() {
  clearCanvas();
  if (pixelRow >= 0 && pixelCol >= 0)
    canvas[pixelRow][pixelCol] = CRGB(fgR, fgG, fgB);
}

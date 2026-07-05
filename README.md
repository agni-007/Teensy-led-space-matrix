# Web-to-Hardware LED Interface

A decoupled Web-to-Hardware LED control subsystem using a dual-microcontroller architecture to isolate asynchronous Wi-Fi tasks from precision hardware timing loops.

---

## Project Overview & Architecture

This project separates web-facing networking logic from time-critical LED driving logic by splitting the work across two microcontrollers:

- **Master Gateway (ESP32)** — Configured as a Wi-Fi Access Point running an HTTP web server. Handles incoming parameters from a custom dashboard and serializes them down a hardware UART bus.
- **Slave Driver (Teensy 4.1)** — Listens asynchronously on `Serial1`. Runs a non-blocking state machine that parses incoming payloads and drives WS2812B addressable LED strips.

This decoupling ensures that Wi-Fi client handling and HTTP request processing on the ESP32 never interfere with the high-frequency, timing-sensitive data protocol required to drive WS2812B LEDs.

---

## Hardware Connections & Wiring Matrix

All components share a common reference baseline (ground) to prevent ground loops and signal floating.

| From | Pin | To | Pin | Purpose |
|---|---|---|---|---|
| ESP32 | GPIO5 (TX1) | Teensy 4.1 | Pin 0 (RX1) | UART serial communication |
| ESP32 | GND | Teensy 4.1 | GND | Common logic ground |
| Teensy 4.1 | Pin 14 | WS2812B Strip | DIN | High-speed data line |
| Teensy 4.1 | GND | WS2812B Strip | GND | Return path to common ground |
| 5V Power Panel | V+ | WS2812B Strip | +5V | Dedicated high-current power rail |
| 5V Power Panel | V- | Teensy/ESP32 | GND | Power supply ground integration |

---

## Errors & Resolutions

### 1. Hardware Serial Conflict (Pin 1)

**Symptom:** Teensy compiled cleanly but LED strip failed to illuminate.

**Root Cause:** Data line assigned to Pin 1 (TX1). Teensy was using `Serial1` to communicate with the ESP32, so the internal UART registers locked the pin, blocking the NeoPixel data stream.

**Resolution:** Migrated data wire to Pin 14, which is isolated from all hardware serial lines.

---

### 2. Logic Level & Pin Mapping Confusion

**Symptom:** Moving to lower digital pins still showed 0.01V on multimeter, dead pin state.

**Root Cause:** Multimeter probe mode mismatch combined with FastLED compiler target mismatches on the NXP MIMXRT1062 caused lower pin registers to fail to clear on boot.

**Resolution:** Switched from FastLED to `Adafruit_NeoPixel` on Teensy 4.1, which cleanly overrides the board's clock registers without mapping bugs.

---

### 3. Bit-Drifting / Color Channel Jumbling

**Symptom:** Pixel 0 displayed correctly, trailing pixels showed random colors.

**Root Cause:** String payload used variable-length structures parsed via unstable `indexOf`. When strings fluctuated in length or rapid UART transfers occurred, arrays shifted and threw off RGB channel assignments.

**Resolution:** Replaced arbitrary delimiters with a rigid `<` / `>` framing protocol. Teensy processes data via a strict byte-level sequential tokenizer, ignoring all noise outside the frame markers.

---

### 4. Power Injection Noise on Heavy Frames

**Symptom:** Full white `#FFFFFF` induced data corruption and erratic flashing.

**Root Cause:** All LEDs switching simultaneously caused an instantaneous voltage drop, shrinking the 3.3V logic threshold and corrupting data frames at the first pixel.

**Resolution:** Lowered global brightness to 64 (~25% duty cycle). Added 1ms settling delay after `strip.show()`. Twisted data wire with ground return wire to reduce loop area.

---

## Summary — Phase 1

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 (WiFi AP + HTTP server) | Receives dashboard input, serializes to UART |
| Link | UART Serial1 | Transfers framed packets between boards |
| Hardware | Teensy 4.1 | Parses packets, drives WS2812B strip |
| Output | WS2812B 5-pixel strip | Visual LED output |

---

## Phase 2 — Scaling to 92-LED Strip

### Overview

Scaled from 5-LED test array to a full 92-LED WS2812B strip. Introduced new serial throughput requirements, power rail stability demands, and high-frequency transmission challenges.

---

### 5. Packet-Buffer Lockout & Array Truncation

**Symptom:** Only first 5 pixels lit; remaining 87 stayed dark.

**Root Cause:** UART protocol still ran the original 15-channel loop structure (5 LEDs × 3 colors). Teensy stopped processing after the 15th index.

**Resolution:** Refactored ESP32 and dashboard to send a 3-channel global color packet `<S,R,G,B>` instead of per-pixel addressing. Set `NUM_LEDS` to 92.

---

### 6. High-Speed Signal Ringing & Pattern Scrambling

**Symptom:** Strip output completely chaotic flickering colors despite correct UART reception.

**Root Cause:** Teensy 4.1 at 600MHz produces extremely sharp signal edges. Over a 92-LED data line, these caused electrical reflections. First pixel interpreted ringing as extra clock pulses, scrambling the data stream. Concurrent current surge across 92 LEDs caused microsecond voltage drops, further desynchronizing data framing.

**Resolution:** Added explicit 1ms settling guard after `strip.show()`. Lowered brightness to 64. Inline 330Ω resistor on data line to damp reflections.

---

### Feature — Algorithmic Wave-Generation Engine

Non-blocking animation engine using `millis()` delta timing at 60+ FPS across 7 modes:

1. **Fast Travel Pulse** — Single white core with cyan trail.
2. **Dual Collision** — Two pulses meet at pixel 46, trigger center flash.
3. **Hyper Flash Strobe** — 35ms alternating full-strip strobe, green to violet.
4. **Hyper-Drive Rainbow Chase** — Phase-offset color wheel across all 92 pixels.
5. **Cyberpunk Neon Pulse** — 8-pixel blocks alternating cyan and magenta.
6. **Emergency Beacon Sweeper** — Interlocking triangle-wave pulses, amber-red vs blue.
7. **Meteor Rain** — Per-frame 20% pixel decay generating organic comet trails.

---

## Summary — Phase 2

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 (WiFi AP + HTTP server) | Streams global color packets |
| Link | UART Serial1 | Framed packet transfer |
| Hardware | Teensy 4.1 | Renders 7 wave FX modes at 60+ FPS |
| Output | WS2812B 92-pixel strip | Brightness-limited, ringing-suppressed output |

---

## Phase 3 — 9×10 2D Matrix Test Rig

### Overview

Scaled from single strip to a 9-row × 10-LED matrix (90 pixels). Each row is an independent WS2812B strip driven in true hardware-parallel via ObjectFLED's DMA engine on Teensy 4.1. Built as a validation rig before deployment on the full 18×508 TinkerSpace wall.

---

### Hardware Configuration

| Parameter | Value |
|---|---|
| Matrix | 9 rows × 10 LEDs = 90 pixels |
| LED type | WS2812B, GRB, 800kHz |
| Teensy data pins | `3, 5, 7, 9, 11, 24, 26, 28, 30` |
| ESP32 UART | TX=GPIO5, RX=GPIO4 |
| Power | Each strip independently powered, common GND |

---

### Architecture Changes

| Aspect | Phase 2 | Phase 3 |
|---|---|---|
| LED driver | `Adafruit_NeoPixel`, blocking | `ObjectFLED`, DMA parallel non-blocking |
| Output model | Sequential per-strip `.show()` | Single `.show()` pushes all lanes via DMA |
| Framebuffer | None | `CRGB canvas[NUM_ROWS][LEDS_PER_ROW]` |
| Theme system | 7 hardcoded 1D modes | Enum-dispatched 2D render functions |
| Scaling | Hardcoded `NUM_LEDS` | `#define NUM_ROWS` + `#define LEDS_PER_ROW` |

---

### 7. FastLED `fl::memset` Namespace Collision

**Symptom:** `error: call of overloaded 'memset(CRGB[9][10], int, unsigned int)' is ambiguous`

**Root Cause:** FastLED post-3.7 defines `fl::memset` inside `fl/stl/cstring.h`, polluting the global namespace and creating an ambiguous overload on any bare `memset()` call against a `CRGB` array.

**Resolution:** Replaced all `memset(canvas, 0, sizeof(canvas))` calls with an explicit loop:
```cpp
void clearCanvas() {
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB::Black;
}
```

---

### 8. Capacitive Crosstalk on Parallel Data Lines

**Symptom:** Lighting one column caused both adjacent columns to flicker with random colors intermittently. Row operations were clean; column operations were not.

**Root Cause:** 800kHz WS2812B edges (~150ns rise/fall) capacitively couple onto physically parallel wires. Teensy 4.1 outputs 3.3V logic; WS2812B guaranteed logic-high threshold is 3.5V at 5V supply — signal already marginal, making it more susceptible to coupled noise. Interference was non-deterministic because it depended on instantaneous rail voltage and contact resistance.

**Diagnosis:** Added `delay(20)` settling pause between clear and show. Interference reduced significantly, confirming power rail transients as a contributing factor.

**Resolution:** 330Ω series resistor on each data line at Teensy pin end. GND wire interleaved between every data line in the bundle. 100µF electrolytic at power entry point. 100nF ceramic cap per strip at VCC/GND pads.

---

### 9. Reverse Polarity — Full Matrix Failure

**Symptom:** VCC/GND briefly swapped. After correction, entire 9-row matrix went dark with no visible physical damage.

**Root Cause:** WS2812B has no reverse-polarity protection. Damaged chip failed shorted (not open), pulling the shared 5V rail to near-zero and killing all rows simultaneously.

**Diagnosis:** Multimeter in resistance mode across each strip's VCC/GND pads with power off. Shorted strip reads near-0Ω; healthy strips read high impedance.

**Resolution:** Cut strip at damaged pixel boundaries. Soldered jumper from upstream `DO/GND/5V` pads directly to downstream `DIN/GND/5V` pads, bypassing the dead chip. Matrix restored with one pixel position sacrificed.

---

### 10. Row 2 Dark After Polarity Repair

**Symptom:** After bypass, row index 2 remained completely dark while all other rows responded.

**Root Cause:** Polarity event destroyed the data input of the first pixel on that strip, breaking the chain for the entire row. Same bypass procedure applied to that strip. Pin array remapped to `{ 3, 5, 7, 9, 11, 24, 26, 28, 30 }` after finding original assignments conflicted with Teensy internal peripherals on several pins.

**Resolution:** Same DIN bypass as above on row 2 strip. Confirmed working via row scan debug mode.

---

### Serial Protocol

| Packet | Format | Action |
|---|---|---|
| Static color | `<S,r,g,b>` | Set foreground color |
| Theme + speed | `<A,mode,speed>` | Switch theme 0–10, speed 1–10 |
| Brightness | `<B,val>` | Live brightness 0–255 |
| Single pixel | `<P,row,col>` | Light one pixel at coordinates |
| Clear | `<X>` | All pixels off immediately |

---

### Theme Library

Speed range 1–10. Internal multiplier: `speedMult = 0.05 + (speed-1) × (1.95/9)`. Speed 1 = 5% rate for debug observation. Speed 10 = 200% rate.

| ID | Name | Behaviour |
|---|---|---|
| 0 | Static Fill | Solid foreground color, full matrix |
| 1 | Wipe Left | Fills from left edge, holds, resets |
| 2 | Wipe Right | Fills from right edge |
| 3 | Wipe Top | Fills from top row downward |
| 4 | Wipe Bottom | Fills from bottom row upward |
| 5 | Row Scan | One row at a time, prints row index and pin to Serial |
| 6 | Col Scan | One column at a time, prints column index to Serial |
| 7 | Dual H Sync | Left and right meet at center column |
| 8 | Dual V Sync | Top and bottom meet at center row |
| 9 | All Sides | All four edges converge inward as concentric rings |
| 10 | Single Pixel | One pixel via `<P,row,col>` |

---

### ESP32 Dashboard

**WiFi AP:** `TinkerMatrix` / `tinker123` — open `192.168.4.1`

**Controls:** Theme selector, speed slider (1–10, labelled debug-slow at minimum), brightness slider, color picker, preset color buttons (R/G/B/W), 9×10 interactive pixel grid showing `row,col` on each cell for individual pixel toggling, Clear All button.

---

## Summary — Phase 3

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 WiFi AP + WebServer | Hosts dashboard, forwards 5 command types |
| Link | UART Serial1 115200 | `<...>` framed packets |
| Rendering | Teensy 4.1 + ObjectFLED DMA | 2D canvas, 10 themes, 9-lane parallel output |
| Output | 9×10 WS2812B matrix | Test rig, 90 pixels, all themes validated |

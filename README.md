# Web-to-Hardware LED Interface

A decoupled Web-to-Hardware LED control subsystem using a dual-microcontroller architecture to isolate asynchronous Wi-Fi tasks from precision hardware timing loops.

---

## Project Overview & Architecture

This project separates web-facing networking logic from time-critical LED driving logic by splitting the work across two microcontrollers:

*   **Master Gateway (ESP32)** — Configured as a Wi-Fi Access Point running an HTTP web server. Handles incoming parameters from a custom dashboard and serializes them down a hardware UART bus.
*   **Slave Driver (Teensy 4.1)** — Listens asynchronously on `Serial1`. Runs a non-blocking state machine that parses incoming payloads and drives WS2812B addressable LED strips.

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
*   **Symptom:** Teensy compiled cleanly but LED strip failed to illuminate.
*   **Root Cause:** Data line assigned to Pin 1 (TX1). Teensy was using `Serial1` to communicate with the ESP32, so the internal UART registers locked the pin, blocking the NeoPixel data stream.
*   **Resolution:** Migrated data wire to Pin 14, which is isolated from all hardware serial lines.

### 2. Logic Level & Pin Mapping Confusion
*   **Symptom:** Moving to lower digital pins still showed 0.01V on multimeter, dead pin state.
*   **Root Cause:** Multimeter probe mode mismatch combined with FastLED compiler target mismatches on the NXP MIMXRT1062 caused lower pin registers to fail to clear on boot.
*   **Resolution:** Switched from FastLED to `Adafruit_NeoPixel` on Teensy 4.1, which cleanly overrides the board's clock registers without mapping bugs.

### 3. Bit-Drifting / Color Channel Jumbling
*   **Symptom:** Pixel 0 displayed correctly, trailing pixels showed random colors.
*   **Root Cause:** String payload used variable-length structures parsed via unstable `indexOf`. When strings fluctuated in length or rapid UART transfers occurred, arrays shifted and threw off RGB channel assignments.
*   **Resolution:** Replaced arbitrary delimiters with a rigid `<` / `>` framing protocol. Teensy processes data via a strict byte-level sequential tokenizer, ignoring all noise outside the frame markers.

### 4. Power Injection Noise on Heavy Frames
*   **Symptom:** Full white `#FFFFFF` induced data corruption and erratic flashing.
*   **Root Cause:** All LEDs switching simultaneously caused an instantaneous voltage drop, shrinking the 3.3V logic threshold and corrupting data frames at the first pixel.
*   **Resolution:** Lowered global brightness to 64 (~25% duty cycle). Added 1ms settling delay after `strip.show()`. Twisted data wire with ground return wire to reduce loop area.

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

### 5. Packet-Buffer Lockout & Array Truncation
*   **Symptom:** Only first 5 pixels lit; remaining 87 stayed dark.
*   **Root Cause:** UART protocol still ran the original 15-channel loop structure (5 LEDs × 3 colors). Teensy stopped processing after the 15th index.
*   **Resolution:** Refactored ESP32 and dashboard to send a 3-channel global color packet `<S,R,G,B>` instead of per-pixel addressing. Set `NUM_LEDS` to 92.

### 6. High-Speed Signal Ringing & Pattern Scrambling
*   **Symptom:** Strip output completely chaotic flickering colors despite correct UART reception.
*   **Root Cause:** Teensy 4.1 at 600MHz produces extremely sharp signal edges. Over a 92-LED data line, these caused electrical reflections. First pixel interpreted ringing as extra clock pulses, scrambling the data stream. Concurrent current surge across 92 LEDs caused microsecond voltage drops, further desynchronizing data framing.
*   **Resolution:** Added explicit 1ms settling guard after `strip.show()`. Lowered brightness to 64. Inline 330Ω resistor on data line to damp reflections.

### Feature — Algorithmic Wave-Generation Engine
Non-blocking animation engine using `millis()` delta timing at 60+ FPS across 7 modes:
1.  **Fast Travel Pulse** — Single white core with cyan trail.
2.  **Dual Collision** — Two pulses meet at pixel 46, trigger center flash.
3.  **Hyper Flash Strobe** — 35ms alternating full-strip strobe, green to violet.
4.  **Hyper-Drive Rainbow Chase** — Phase-offset color wheel across all 92 pixels.
5.  **Cyberpunk Neon Pulse** — 8-pixel blocks alternating cyan and magenta.
6.  **Emergency Beacon Sweeper** — Interlocking triangle-wave pulses, amber-red vs blue.
7.  **Meteor Rain** — Per-frame 20% pixel decay generating organic comet trails.

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

### Hardware Configuration

| Parameter | Value |
|---|---|
| Matrix | 9 rows × 10 LEDs = 90 pixels |
| LED type | WS2812B, GRB, 800kHz |
| Teensy data pins | `3, 5, 7, 9, 11, 24, 26, 28, 30` |
| ESP32 UART | TX=GPIO5, RX=GPIO4 |
| Power | Each strip independently powered, common GND |

### Architecture Changes

| Aspect | Phase 2 | Phase 3 |
|---|---|---|
| LED driver | `Adafruit_NeoPixel`, blocking | `ObjectFLED`, DMA parallel non-blocking |
| Output model | Sequential per-strip `.show()` | Single `.show()` pushes all lanes via DMA |
| Framebuffer | None | `CRGB canvas[NUM_ROWS][LEDS_PER_ROW]` |
| Theme system | 7 hardcoded 1D modes | Enum-dispatched 2D render functions |
| Scaling | Hardcoded `NUM_LEDS` | `#define NUM_ROWS` + `#define LEDS_PER_ROW` |

### 7. FastLED `fl::memset` Namespace Collision
*   **Symptom:** `error: call of overloaded 'memset(CRGB[9][10], int, unsigned int)' is ambiguous`
*   **Root Cause:** FastLED post-3.7 defines `fl::memset` inside `fl/stl/cstring.h`, polluting the global namespace and creating an ambiguous overload on any bare `memset()` call against a `CRGB` array.
*   **Resolution:** Replaced all `memset(canvas, 0, sizeof(canvas))` calls with an explicit loop:
```cpp
void clearCanvas() {
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < LEDS_PER_ROW; c++)
      canvas[r][c] = CRGB::Black;
}

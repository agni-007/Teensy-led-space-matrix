# Web-to-Hardware LED Interface

A decoupled Web-to-Hardware LED control subsystem using a dual-microcontroller architecture to isolate asynchronous Wi-Fi tasks from precision hardware timing loops.

---

## 🛠️ Project Overview & Architecture

This project separates web-facing networking logic from time-critical LED driving logic by splitting the work across two microcontrollers:

- **Master Gateway (ESP32)** — Configured as a Wi-Fi Access Point (`RGB_Control`) running an HTTP web server. It handles incoming parameters from a custom dashboard and serializes them down a hardware UART bus.
- **Slave Driver (Teensy 4.1)** — Listens asynchronously on `Serial1`. It runs a non-blocking state machine that parses incoming payloads and drives a 5-pixel WS2812B addressable LED strip.

This decoupling ensures that Wi-Fi client handling and HTTP request processing on the ESP32 never interfere with the high-frequency, timing-sensitive data protocol required to drive WS2812B LEDs.

---

## 🔌 Hardware Connections & Wiring Matrix

All components share a common reference baseline (ground) to prevent ground loops and signal floating.

| From Component | Pin | To Component | Pin | Purpose |
|---|---|---|---|---|
| ESP32 | GPIO 5 (TX1) | Teensy 4.1 | Pin 0 (RX1) | UART serial communication link |
| ESP32 | GND | Teensy 4.1 | G (Ground) | Common logic ground |
| Teensy 4.1 | Pin 14 | WS2812B Strip | DI (Data In) | High-speed data line (via inline 330Ω resistor) |
| Teensy 4.1 | G (Ground) | WS2812B Strip | GND | Return path to common ground |
| 5V Power Panel | V+ (+5V) | WS2812B Strip | +5V | Dedicated high-current power rail |
| 5V Power Panel | V- (GND) | Teensy/ESP32 | GND / G | Power supply ground integration |

---

## ❌ Encountered Errors & Diagnostic Log

### 1. Hardware Serial Conflict (Pin 1)
**Symptom:** The Teensy compiled cleanly but produced compile warnings regarding hardware constraints, and the LED strip failed to illuminate.

**Root Cause:** The data line was initially assigned to Pin 1 (TX1). Because the Teensy was using `Serial1` to talk to the ESP32, the internal UART registers locked the pin, blocking the high-frequency NeoPixel data stream.

### 2. Logic Level Verification & Pin Mapping Confusion
**Symptom:** Moving to lower digital pins still showed 0.01V on a multimeter tracking loop, indicating a dead pin state.

**Root Cause:** Multimeter probe mode mismatch combined with specific FastLED compiler target mismatches on the NXP MIMXRT1062 processor caused the lower pin registers to fail to clear on boot.

### 3. Bit-Drifting / Color Channel Jumbling
**Symptom:** Pixel 0 displayed correctly, but trailing pixels displayed random colors (e.g., selecting White resulted in Cyan or Red down the line).

**Root Cause:** The initial string payload used variable-length structures parsed via unstable index cutting (`indexOf`). When strings fluctuated in length or encountered rapid UART transfers, the arrays shifted, throwing off subsequent RGB channel assignments.

### 4. Power Injection Noise on Heavy Frames
**Symptom:** Selecting high-current colors like full White (`#FFFFFF`) induced data corruption and erratic flashing.

**Root Cause:** Turning on all color dies at once induced an instantaneous voltage drop on the power rails, shrinking the 3.3V logic threshold and corrupting data frames at the first pixel.

---

## ✅ Resolution Actions Taken

1. **Pin Migration** — Moved the data wire permanently to Pin 14 (located mid-way along the right outer edge of the Teensy 4.1), which safely isolates the signal from any hardware serial line interference.
2. **Library Pivot** — Switched from `FastLED` to `Adafruit_NeoPixel` on the Teensy 4.1, which cleanly overrides the board's high-speed clock registers without mapping bugs.
3. **Marker-Based Packet Framing** — Replaced the arbitrary delimiter string with a rigid framing protocol wrapping data between `<` and `>` tags (e.g., `<S,255,0,0...>` and `<A,ModeID>`). The Teensy now ignores external noise and processes data via a strict byte-level sequential tokenizer.
4. **Asynchronous Engine Implementation** — Rewrote the Teensy loop into an async state machine driven by `millis()` deltas rather than blocking `delay()` calls. This preserves 60+ FPS animation frame rates for operations like Fast Travel, Dual Collision, and Strobe while keeping the serial link open to intercept immediate dashboard interruptions.

---

## 📦 Summary

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 (Wi-Fi AP + HTTP server) | Receives dashboard input, serializes to UART |
| Link | UART (Serial1) | Transfers framed packets (`<...>`) between boards |
| Hardware | Teensy 4.1 (async state machine) | Parses packets, drives WS2812B strip |
| Output | WS2812B (5-pixel strip) | Visual LED output |

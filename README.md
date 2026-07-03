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

## 📦 Summary (Phase 1)

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 (Wi-Fi AP + HTTP server) | Receives dashboard input, serializes to UART |
| Link | UART (Serial1) | Transfers framed packets (`<...>`) between boards |
| Hardware | Teensy 4.1 (async state machine) | Parses packets, drives WS2812B strip |
| Output | WS2812B (5-pixel strip) | Visual LED output |

---

## Phase 2: Hardware Scaling & Advanced FX Development

### 1. Architectural Scaling (92-LED Strip Integration)

The system was scaled from an isolated 5-LED testing array to a full-sized 92-LED WS2812B matrix. This hardware scaling introduced new serial throughput requirements, power rail stability demands, and high-frequency data transmission challenges.

---

### ❌ Chronological Log of Latest Errors & Challenges

#### Challenge 5: Fixed Packet-Buffer Lockout & Array Truncation

**Symptom:** After connecting the new 92-LED strip, only the first 5 pixels illuminated; the remaining 87 LEDs remained completely unpowered/dark.

**Root Cause:** The UART protocol was still running the initial rigid 15-channel loop structure (`5 LEDs × 3 colors`). The Teensy driver stopped processing data indices after the 15th position, leaving subsequent LEDs unaddressed.

**Solution:**
- Refactored the web dashboard and ESP32 gateway to stream a streamlined 3-channel global master color packet (`<S,R,G,B>`) instead of mapping individual pixels over the serial bus.
- Scaled `#define NUM_LEDS` to `92` on the Teensy code to open up the entire register.

#### Challenge 6: High-Speed Signal Ringing & Pattern Scrambling

**Symptom:** The Teensy was successfully intercepting commands via UART, but the strip outputted completely chaotic, flickering colors. The traveling patterns were unrecognizable, and the strip desynchronized.

**Root Cause:**
- **Impedance Mismatch / Ringing:** The Teensy 4.1's fast 600MHz processor produces extremely sharp square wave signal edges. Over the extended data line running to 92 LEDs, these sharp transitions caused electrical reflections (ringing). The gatekeeper pixel interpreted this ringing as extra clock pulses, scrambling the incoming data stream.
- **Power Rail Ripple:** The sharp increase in current pull across 92 LEDs induced microsecond voltage drops (ripple noise) on the power lines, causing the LEDs to lose sync with the 3.3V data signal threshold.

**Solution:**
- **Software Signal Slowdown:** Adjusted the code to apply an explicit `delay(1)` (1 millisecond) latching/settling guard window immediately following the `strip.show()` frame rendering cycle to allow electrical noise to dissipate before the next transfer.
- **Current Surge Suppression:** Lowered the global software brightness setting to `64` (~25% duty cycle) to flatten high-current draw spikes and eliminate supply rail ripples.
- **Hardware Backstop:** Twisted the data wire (Pin 14) tightly parallel with the ground return wire to reduce electromagnetic loop area and prepared an inline 330Ω damping resistor to absorb signal reflections.

---

### 🚀 Latest Feature Implementations

#### Algorithmic Wave-Generation Engine

To maximize the visual performance of the stable 92-LED array without blocking the UART stream, the animation engine was upgraded from rigid pixel shifting to Mathematical Phase-Shifting Waves. The engine now runs at 60+ FPS using non-blocking `millis()` tracking across 7 distinct high-velocity modes:

1. **Mode 1: Fast Travel Pulse** — Fast traveling single white core leaving a cyan trail.
2. **Mode 2: Dual Collision** — Dual inbound traveling pulses meeting at the absolute midpoint (pixel 46) triggering an intense flashing center-impact sequence.
3. **Mode 3: Hyper Flash Strobe** — Blazing 35ms alternating full-strip strobe shifts (Neon Green to Deep Violet).
4. **Mode 4: Hyper-Drive Rainbow Chase** — Math-based continuous color spectrum wheel mapped sequentially across all 92 pixels via phase offsets.
5. **Mode 5: Cyberpunk Neon Pulse** — Moving 8-pixel high-contrast blocks shifting Neon Cyan and Neon Magenta across the array.
6. **Mode 6: Emergency Beacon Sweeper** — Interlocking triangle-wave pulses crossing paths with localized peak intensity scaling (Amber-Red vs Blue).
7. **Mode 7: Meteor Rain** — Real-time decay tracking that continuously dims pixels by 20% to generate organic, sparkling trails trailing a high-speed comet head.

---

## 📦 Summary (Phase 2)

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 (Wi-Fi AP + HTTP server) | Streams global master color packets (`<S,R,G,B>`) |
| Link | UART (Serial1) | Transfers framed packets between boards |
| Hardware | Teensy 4.1 (async state machine) | Renders 7 phase-shifting wave FX modes at 60+ FPS |
| Output | WS2812B (92-pixel strip) | Visual LED output with brightness-limited, ringing-suppressed signal |

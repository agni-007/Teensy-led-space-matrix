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
```

### 8. Capacitive Crosstalk on Parallel Data Lines
*   **Symptom:** Lighting one column caused both adjacent columns to flicker with random colors intermittently. Row operations were clean; column operations were not.
*   **Root Cause:** 800kHz WS2812B edges (~150ns rise/fall) capacitively couple onto physically parallel wires. Teensy 4.1 outputs 3.3V logic; WS2812B guaranteed logic-high threshold is 3.5V at 5V supply — signal already marginal, making it more susceptible to coupled noise. Interference was non-deterministic because it depended on instantaneous rail voltage and contact resistance.
*   **Diagnosis:** Added `delay(20)` settling pause between clear and show. Interference reduced significantly, confirming power rail transients as a contributing factor.
*   **Resolution:** 330Ω series resistor on each data line at Teensy pin end. GND wire interleaved between every data line in the bundle. 100µF electrolytic at power entry point. 100nF ceramic cap per strip at VCC/GND pads.

### 9. Reverse Polarity — Full Matrix Failure
*   **Symptom:** VCC/GND briefly swapped. After correction, entire 9-row matrix went dark with no visible physical damage.
*   **Root Cause:** WS2812B has no reverse-polarity protection. Damaged chip failed shorted (not open), pulling the shared 5V rail to near-zero and killing all rows simultaneously.
*   **Diagnosis:** Multimeter in resistance mode across each strip's VCC/GND pads with power off. Shorted strip reads near-0Ω; healthy strips read high impedance.
*   **Resolution:** Cut strip at damaged pixel boundaries. Soldered jumper from upstream `DO/GND/5V` pads directly to downstream `DIN/GND/5V` pads, bypassing the dead chip. Matrix restored with one pixel position sacrificed.

### 10. Row 2 Dark After Polarity Repair
*   **Symptom:** After bypass, row index 2 remained completely dark while all other rows responded.
*   **Root Cause:** Polarity event destroyed the data input of the first pixel on that strip, breaking the chain for the entire row. Same bypass procedure applied to that strip. Pin array remapped to `{ 3, 5, 7, 9, 11, 24, 26, 28, 30 }` after finding original assignments conflicted with Teensy internal peripherals on several pins.
*   **Resolution:** Same DIN bypass as above on row 2 strip. Confirmed working via row scan debug mode.

### Serial Protocol

| Packet | Format | Action |
|---|---|---|
| Static color | `<S,r,g,b>` | Set foreground color |
| Theme + speed | `<A,mode,speed>` | Switch theme 0–10, speed 1–10 |
| Brightness | `<B,val>` | Live brightness 0–255 |
| Single pixel | `<P,row,col>` | Light one pixel at coordinates |
| Clear | `<X>` | All pixels off immediately |

### Theme Library (Phase 3)
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

### ESP32 Dashboard
*   **WiFi AP:** `TinkerMatrix` / `tinker123` — open `192.168.4.1`
*   **Controls:** Theme selector, speed slider (1–10, labelled debug-slow at minimum), brightness slider, color picker, preset color buttons (R/G/B/W), 9×10 interactive pixel grid showing `row,col` on each cell for individual pixel toggling, Clear All button.

---

## Summary — Phase 3

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 WiFi AP + WebServer | Hosts dashboard, forwards 5 command types |
| Link | UART Serial1 115200 | `<...>` framed packets |
| Rendering | Teensy 4.1 + ObjectFLED DMA | 2D canvas, 10 themes, 9-lane parallel output |
| Output | 9×10 WS2812B matrix | Test rig, 90 pixels, all themes validated |

---

## Phase 4 — Scrolling Text & Speed Tuning

### Overview
Added on-the-fly scrolling text rendering to the matrix without reflashing either board. Text content and scroll speed are set entirely from the dashboard over Wi-Fi. A hand-built 5×7 pixel font is rendered directly on the Teensy framebuffer, vertically centered in the 9-row display.

### Feature — Scrolling Text Engine
A new theme (ID 11) renders an arbitrary ASCII message as a continuously looping horizontal marquee. The font and scroll engine are implemented entirely on the Teensy; the ESP32 acts only as a relay.

*   **Font:** 5×7 bitmap, hand-defined per glyph. Supports A–Z, 0–9, and punctuation (`. , ! ? : - '`). Each character occupies 5 pixel columns with a 1-column blank gap (pitch = 6). The 7-row glyph is vertically centered with 1 blank row above and below within the 9-row display, exploiting the tighter vertical LED spacing for maximum readability.
*   **Scroll speed:** Decoupled from the general theme speed multiplier. Maps speed slider 1–10 linearly to 1–20 px/sec, giving a readable slow crawl at minimum and a fast marquee at maximum without modifying any other theme's timing.
*   **Loop behaviour:** Text scrolls fully off the left edge before restarting from the right. A blank gap equal to one screen width separates the end of the message from the next loop, preventing visual wrap-collision.

### Serial Protocol Addition

| Packet | Format | Action |
|---|---|---|
| Scroll text | `<T,MESSAGE>` | Set message string and switch to theme 11 |

*   Message is passed as raw ASCII (uppercase enforced by ESP32 before transmission).
*   Unsupported characters are substituted with a blank space by the ESP32 before the packet is sent.
*   Maximum message length: 60 characters.
*   The `<T,...>` parser on the Teensy uses a dedicated raw-text capture path, completely isolated from the existing numeric tokenizer used by all other packet types.

### Dashboard Addition
A **Scrolling Text** card was added below the Color card:
*   Free-text input field (max 60 chars).
*   **"Scroll This Text"** button — sets theme dropdown to 11, sends `<A,11,speed>` followed by `<T,MESSAGE>`.
*   Speed and Brightness sliders apply to scroll text identically to all other themes — no new controls required.
*   Character set note displayed inline below the input field.

### 11. Scroll Speed Ceiling Too Low
*   **Symptom:** At speed 10 the marquee was readable but not noticeably fast; no visible "high speed" effect on the physical matrix.
*   **Root Cause:** Original speed mapping capped scroll rate at 8 px/sec, which on a 10-column display produces only moderate motion.
*   **Resolution:** Raised speed ceiling from 8 px/sec to 20 px/sec by adjusting the scroll speed multiplier constant:
```cpp
// Before
float scrollSpeedPxPerSec = 1.0f + (themeSpeed - 1) * (7.0f / 9.0f);

// After
float scrollSpeedPxPerSec = 1.0f + (themeSpeed - 1) * (19.0f / 9.0f);
```
Minimum (speed 1) unchanged at 1 px/sec.

---

## Summary — Phase 4

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 WiFi AP + WebServer | Sanitizes and forwards text packets, hosts updated dashboard |
| Link | UART Serial1 115200 | `<T,...>` raw-text packets alongside existing numeric protocol |
| Rendering | Teensy 4.1 + ObjectFLED DMA | 5×7 font engine, scroll position accumulator, theme 11 dispatch |
| Output | 9×10 WS2812B matrix | Scrolling text confirmed readable at all speeds 1–10 |

---

## Phase 5 — The Massive Matrix (18x508)

### Overview
Scaled the project from the 9x10 test rig to the final, full-scale TinkerSpace wall. The new hardware profile is a colossal 18 rows by 508 columns, totaling 9,144 WS2812B LEDs. This phase required massive structural re-tuning for power limits, data speed parity, and layout orientation.

### Hardware Configuration & Power Management

| Parameter | Value |
|---|---|
| Matrix Dimensions | 18 rows × 508 LEDs |
| Total Pixel Count | 9,144 |
| Teensy Data Pins | `3, 5, 7, 9, 11, 24, 26, 28, 30, 32, 37, 39, 41, 14, 16, 18, 20, 22` |
| Master Power Supply | 5V / 80A |

**Power Constraints:** Driving 9,144 LEDs at full white requires hundreds of amps, drastically exceeding the 80A power supply. To prevent PSU overcurrent tripping or fire hazards, software-enforced brightness capping was implemented. The maximum global brightness is constrained strictly to 35 (out of 255).

### Core Architecture Enhancements

#### 1. Per-Row Speed Compensation
*   **Challenge:** Resistance and voltage sag on longer wiring segments for specific row pins caused those strips to lag visibly behind others during full-panel sweeps.
*   **Solution:** Implemented a float array `rowSpeedComp[NUM_ROWS]` to act as an independent clock multiplier for each row. Lagging rows are nudged up (e.g., 1.02 to 1.10) allowing them to "catch up" mathematically and maintain visual sync across the 18-row wipe animations.

#### 2. Physical Axis Inversion flags
*   **Challenge:** Physical wiring orientation resulted in text scrolling backwards or rendering upside down.
*   **Solution:** Added preprocessor definitions `INVERT_X_AXIS` and `INVERT_Y_AXIS`. The rendering loop dynamically maps logical coordinate planes to the physical output, ensuring animations enter from the correct edge without requiring physical rewiring.

#### 3. Font Scaling & Dynamic Centering
*   **Challenge:** The 5x7 test-rig font looked tiny on an 18-row matrix.
*   **Solution:** Added `FONT_SCALE_X` and `FONT_SCALE_Y` constants to scale the binary font mapping to 2x2. The resulting 10x14 text is centered via a `TEXT_ROW_OFFSET`, leaving equal padding above and below.

---

## Summary — Phase 5

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 WiFi AP + WebServer | Serves UI, enforces constraints |
| Link | UART Serial1 115200 | Reliable framing maintained |
| Rendering | Teensy 4.1 + ObjectFLED | Drives 18 parallel strips; processes sub-pixel rendering and font scaling |
| Output | 18×508 WS2812B matrix | Full TinkerSpace deployment, 9,144 LEDs, hardware compensated |

---

## Phase 6 — Sprite Engines & Autonomous Games

### Overview
Introduced dynamic, sprite-based themes to the massive matrix. Removed diagnostic row/column themes (IDs 5, 6, 10) to declutter the dashboard, replacing them with classic arcade animations and autonomous game physics.

### Feature — Sprite Engines & Game Themes

#### Pac-Man Loop (Theme 12)
*   Renders an 8x8 sprite (scaled to 16x16) that continuously wraps around the 508-column width.
*   Mouth animation alternates between open/closed frames synchronously tied to `millis()` (150ms cadence), operating completely independently of the movement speed.

#### Chrome Dino Game (Theme 13)
*   Self-playing emulation of the classic offline dinosaur game.
*   Employs 10x10 sprites (drawn from `uint16_t` arrays) for the Dino and Cactuses.
*   **Auto-Jump Physics Engine:** Cactuses spawn off-screen and scroll left. The logic calculates a `jumpTriggerDist` dynamically based on the current `gameSpeed` slider. Once a cactus enters the calculated danger zone, the system injects vertical velocity (`dinoVy`), smoothly overriding the running animation with a jumping state until gravity returns the sprite to the "floor" plane.

### Updated Theme Library (Final)

| ID | Name | Behaviour |
|---|---|---|
| 0 | Solid Fill | Solid foreground color, full matrix |
| 1 | Wipe ← Left | Fills from left edge, holds, resets |
| 2 | Wipe Right → | Fills from right edge |
| 3 | Wipe ↓ Top | Fills from top row downward |
| 4 | Wipe ↑ Bottom | Fills from bottom row upward |
| 7 | Dual Sync ←→ | Left and right meet at center column |
| 8 | Dual Sync ↕ | Top and bottom meet at center row |
| 9 | All Sides | All four edges converge inward as concentric rings |
| 11 | Scroll Text | Scaled 10x14 marquee, custom message via `<T,...>` |
| 12 | Pac-Man Loop | 16x16 animated sprite wrapping horizontally |
| 13 | Chrome Dino | Procedurally generating, auto-playing physics game |

---

## Summary — Phase 6

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 WiFi AP + WebServer | Exposes game theme selection to the dashboard |
| Hardware | Teensy 4.1 | Processes autonomous physics calculations, sprite rendering, and gravity parameters |
| Output | 18×508 WS2812B matrix | Displays complex multi-layered sprite animations smoothly |


## Phase 7: Fully Wireless Text Rasterization, Dual-Core Optimization, & Infinite Auto-Dino Loop

### Major Architectural Upgrades & Dual-Core Functionality
* **100% Wireless Text Rasterization Engine:** Moved the 5x7 font lookup table and character rasterization logic entirely to the ESP32 Core 1. Text is now dynamically rasterized on the ESP32 and streamed down to the Teensy via high-speed pixel coordinate commands (`<P,r,c>`), eliminating the need to physically flash the Teensy for font or text updates.
* **ESP32 Dual-Core Task Splitting:**
  * **Core 0 (Network, Web & OTA):** Dedicated to handling the embedded web server, dashboard AJAX requests, Wi-Fi stack operations, and continuous over-the-air firmware updates without stuttering the display.
  * **Core 1 (Game Physics, AI & Rasterization):** Runs the locked 60 FPS physics engine, hitbox collision detection, automatic color-cycling timer, and real-time text rasterization.
* **Infinite Auto-Dino Looprun & Random Gaps:** Upgraded the Chrome Dino full-auto mode to run continuously in an infinite loop. Obstacles now spawn with randomized distance gaps (`random(100, 280)`), paired with an automated AI jump calculator and a 10-second 10-color palette shifting loop.
* **Arcade Manual Mode & HUD Scoring:** Integrated a backend 3-try life tracking system, real-time score accumulation, and a localized "GAME OVER" display state driven by fast UART packet streaming (`<F>`).
* **High-Speed UART Pipeline:** Maintained a robust 1.5 Mbps baud rate serial link where the ESP32 acts as the **brains** (math, logic, text parsing, and web state) and the Teensy 4.1 acts purely as a **dumb pixel GPU** (rendering the canvas buffer to the physical LED panels).

---

## Phase 8 — WebSocket Control

### Overview
Phase 8 introduces a persistent WebSocket connection to replace repeated HTTP request overhead, optimizing control latency. It preserves the Phase 7 architecture split, functioning as an ESP32-only update. The ESP32 handles Wi-Fi, browser control, game physics, and state streaming, while the existing Phase 7 Teensy firmware continues rendering to the 18 parallel WS2812B rows via ObjectFLED.

### Major Architectural Upgrades & Optimizations
* **Persistent WebSocket Transport:** A single RFC 6455 WebSocket on port 81 replaces repetitive HTTP requests. It features bounded exponential backoff for auto-reconnection and round-trip time tracking.
* **Compact Browser-to-ESP32 Commands:** Commands for theme/speed (`A`), color (`S`), brightness (`B`), scrolling text (`T`), and game modes (`D`, `J`, `X`) are highly optimized for fast parsing and transmission.
* **RTOS & Networking Enhancements:** The network loop runs every FreeRTOS tick on core 0. Wi-Fi modem sleep is disabled to favor latency. The library's TCP no-delay behavior and heartbeat are enforced.
* **Concurrency Fixes:** Replaced simple `volatile` variables with critical sections for cross-core state management. Serialized UART writes from both cores using a mutex.
* **Game Timing Precision:** Game pacing switched to `vTaskDelayUntil()` to eliminate task-delay drift.

### Summary — Phase 8

| Layer | Component | Role |
|---|---|---|
| Network | ESP32 (WiFi AP + WebSocket) | Fast WebSocket transport on port 81, disables Wi-Fi sleep for low latency |
| Link | UART Serial1 1.5 Mbps | Compact framed commands sent from ESP32, concurrency managed via mutex |
| Rendering | Teensy 4.1 + ObjectFLED | Unchanged from Phase 7; continues to render matrix and handle dumb GPU tasks |
| Output | 18×508 WS2812B matrix | Maintains 35 brightness cap and preset RGB/text routings fixed from ESP32 |

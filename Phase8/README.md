# Phase 8 — WebSocket Control

Phase 8 is an ESP32-only update. It preserves the Phase 7 split: the ESP32 owns
Wi-Fi, browser control, game physics, collision logic, and state streaming;
the already-flashed Phase 7 Teensy firmware continues to render the matrix and
drive 18 parallel WS2812B rows through ObjectFLED.

## Sketches

- `ESP32/ESP32.ino` — the only Phase 8 firmware; deploy it to a classic
  dual-core ESP32 over USB or the existing Arduino OTA service.

No Teensy upload is required. Leave the existing Phase 7 Teensy firmware in
place at 1.5 Mbps UART.

## Transport

The browser performs one HTTP request on port 80 to load the dashboard. After
that, all controls use one persistent RFC 6455 WebSocket on port 81. The page
automatically reconnects with bounded exponential backoff and reports the
command acknowledgement round-trip time.

Browser-to-ESP32 commands are deliberately compact:

| Command | Meaning |
|---|---|
| `A,theme,speed` | Select a supported theme and speed 1–10 |
| `S,r,g,b` | Set foreground color, channels 0–255 |
| `B,value` | Set brightness, constrained to 0–35 |
| `T,message` | Sanitize and start a message of up to 60 characters |
| `D,0` / `D,1` | Dino manual / automatic mode |
| `J` | Manual jump |
| `X` | Persistent blackout |

The ESP32 responds with `OK`, `ERR,reason`, or
`STATE,theme,speed,brightness`. It continues to stream normalized framed UART
commands to the Teensy at 1.5 Mbps. Game render state uses the existing `<F>`
packet.

## Optimizations and fixes

- Persistent WebSocket removes repeated HTTP request/connection overhead.
- The WebSocket network loop runs every FreeRTOS tick on core 0.
- Wi-Fi modem sleep is disabled to favor latency over ESP32 power consumption.
- WebSocket TCP clients use the library's TCP no-delay behavior and heartbeat.
- ESP32 cross-core state uses a critical section instead of `volatile` alone.
- UART writes from both cores are serialized by a mutex.
- Game pacing uses `vTaskDelayUntil()` to avoid accumulating task-delay drift.
- Commands are parsed into fixed stack buffers and range-checked before UART.
- Preset RGB routing and scrolling-text activation are fixed from the ESP32.
- Persistent blackout is implemented as `<A,0,3>` followed by `<S,0,0,0>`,
  avoiding the broken `<X>` behavior in the existing Phase 7 Teensy firmware.
- Brightness 35 is enforced at the ESP32 command boundary. It is still
  necessary to validate the real power distribution, branch fusing, injection,
  and PSU derating before operating all 9,144 LEDs.

## Teensy limitation

The existing Teensy remains a renderer, so this ESP-only update can change
networking, UI, game physics, colors, animation state, text content, and game
state without touching the Teensy. It cannot introduce a new LED rendering
primitive, font glyph, sprite format, or UART packet type that the Phase 7
Teensy does not already understand. Those changes would require one Teensy
firmware update. Phase 7 text supports space, A-Z, and 0-9; punctuation is
sanitized by the ESP but renders as blank with the existing Teensy font.

## Dependencies used for validation

- Arduino CLI 1.5.1
- ESP32 Arduino core 3.3.10, board `esp32:esp32:esp32`
- WebSockets by Markus Sattler 2.7.2

Compile commands:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 Phase8/ESP32
```

The ESP32 sketch compiled successfully with the versions above and without
warnings.

## Wiring retained from Phase 7

- ESP32 GPIO5 TX1 → Teensy pin 0 RX1
- ESP32 ground ↔ Teensy ground
- Teensy rows: `3, 5, 7, 9, 11, 24, 26, 28, 30, 32, 37, 39, 41, 14, 16, 18, 20, 22`
- ESP32 access point: `RGB_Control` / `rgb12345`
- Dashboard: `http://192.168.4.1/`
- WebSocket: `ws://192.168.4.1:81/`

Use 5 V-compatible AHCT buffers or OctoWS2811-class line drivers between the
Teensy outputs and LED data lines. A common ground and series resistors alone do
not replace logic-level translation.

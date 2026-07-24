# Phase 8 — Synchronized Matrix Controller

Phase 8 is a coordinated ESP32 and Teensy update for the 18 × 508 LED matrix.
The ESP32 owns Wi-Fi, the browser dashboard, WebSocket commands, and game
physics. The Teensy renders all 9,144 pixels through 18 parallel ObjectFLED
outputs.

## What changed

- Dashboard controls wait for ESP32-confirmed state instead of showing a false
  selection immediately.
- WebSocket state now synchronizes theme, speed, brightness, RGB, and Dino mode.
- Speed changes no longer restart Pac-Man or Dino.
- All color presets send literal `S,r,g,b` values and expose confirmed selected
  state. Red, green, blue, yellow, cyan, magenta, white, black, and the custom
  picker share the same path.
- Controls disable while disconnected, reconnect automatically, support visible
  keyboard focus, and meet 44 px touch-target sizing.
- Scrolling text maps the 7-row font across every one of the 18 physical rows;
  there is no fixed 2 px top or bottom gutter.
- Dino is anchored at the right, faces left, and jumps over cacti travelling
  from left to right. Score and lives are shown at the opposite, left end.
- Pac-Man now renders a complete chase scene with a two-frame mouth, eye,
  animated ghosts, regular pellets, and pulsing power pellets.

## Required uploads

This revision changes renderer primitives and therefore cannot be deployed as
an ESP32-only OTA update.

1. Upload `Teensy.ino` once to the Teensy 4.1 over USB.
2. Upload `ESP32.ino` to the ESP32 over USB or the existing Arduino OTA service.

After that coordinated update, dashboard/network changes that do not alter
renderer primitives can again be deployed ESP32-only.

## Transport

The browser loads the dashboard over HTTP port 80 and sends all controls through
one persistent RFC 6455 WebSocket on port 81.

| Command | Meaning |
|---|---|
| `A,theme,speed` | Select a supported theme and speed 1–10 |
| `S,r,g,b` | Set foreground color, channels 0–255 |
| `B,value` | Set brightness, constrained to 0–35 |
| `T,message` | Sanitize and start a message of up to 60 characters |
| `D,0` / `D,1` | Dino manual / automatic mode |
| `J` | Manual jump |
| `X` | Persistent blackout |

The ESP32 responds with `OK`, `ERR,reason`, and
`STATE,theme,speed,brightness,r,g,b,dinoMode`. UART packets to the Teensy remain
framed with `<...>` at 1.5 Mbps.

## Color routing and hardware check

The software path is RGB end to end:

`dashboard RGB → WebSocket S,r,g,b → ESP32 range check → UART <S,r,g,b> → CRGB`

ObjectFLED is configured with `CORDER_GRB`, the normal wire order for WS2812B,
and performs that physical channel conversion once while generating DMA data.
Do not compensate for a wiring or pixel-type mismatch by relabelling dashboard
buttons.

After uploading both sketches, test red, green, blue, yellow, cyan, magenta,
white, and black on the real matrix at low brightness. If a primary color is
still wrong, confirm the installed pixel model and change `LED_COLOR_ORDER` in
`Teensy.ino` to the order required by that hardware. A pure channel permutation
cannot by itself explain one primary appearing as a two-channel color; inspect
data level shifting, grounds, connectors, and the affected LED branches too.

## Validation performed

Validated locally with Arduino CLI 1.5.1, ESP32 core 3.3.10, Teensy core 1.62.0,
WebSockets 2.7.2, FastLED 3.10.5, and ObjectFLED 1.0.3:

- ESP32 compiled for `esp32:esp32:esp32` with `--clean --warnings all`.
- Teensy compiled for `teensy:avr:teensy41` with `--clean --warnings all`.
- A host-side dashboard suite passed checks covering connection state, every
  visible control, all preset RGB payloads, server-confirmed UI state,
  full-row text mapping, Dino direction, and Pac-Man scene elements.
- The only compiler warning came from an unused internal variable in the
  installed FastLED Teensy FlexIO driver, not either Phase 8 sketch.

Compilation does not prove physical LED color, orientation, power delivery, or
signal integrity. Those require the on-device checks above.

## Wiring

- ESP32 GPIO5 TX1 → Teensy pin 0 RX1
- ESP32 ground ↔ Teensy ground
- Teensy rows: `3, 5, 7, 9, 11, 24, 26, 28, 30, 32, 37, 39, 41, 14, 16, 18, 20, 22`
- ESP32 access point: `RGB_Control` / `rgb12345`
- Dashboard: `http://192.168.4.1/`
- WebSocket: `ws://192.168.4.1:81/`

Use 5 V-compatible AHCT buffers or OctoWS2811-class line drivers between the
Teensy outputs and LED data lines. Brightness remains capped at 35, but the real
power distribution, branch fusing, injection, and PSU derating must still be
validated before operating the full matrix.

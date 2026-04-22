# Arduino ESP32 Firmware

`watch_firmware_arduino` is the hardware-ready ESP32/Arduino IDE version of the smartwatch logic. It mirrors the same prototype behavior as the local simulator, but in an Arduino sketch structure using `setup()` and `loop()`.

For full project-level technical documentation, see:

- [docs/TECHNICAL_DOCUMENTATION.md](/Users/a13x0r3d/Documents/New%20project/docs/TECHNICAL_DOCUMENTATION.md)

## What This Is For

- showing that the project is ready to move from local simulator to hardware
- opening the watch code directly in Arduino IDE
- compiling for an ESP32 board such as `ESP32 Dev Module`
- serving the same REST API and a browser watch UI from real ESP32 firmware

## Sketch

Open this file in Arduino IDE:

- [watch_firmware_arduino.ino](/Users/a13x0r3d/Documents/New%20project/watch_firmware_arduino/watch_firmware_arduino.ino)

## Firmware Features

- Arduino-style `setup()` / `loop()`
- `WiFi.h` + `WebServer.h` based HTTP API
- browser watch UI served by the firmware itself
- firmware layout separated into screen rendering, sensor simulation, networking, and request handlers
- OLED/LVGL-style screen model with dedicated render functions for main, steps, battery, and alarm screens
- internal watch state:
  - time
  - day
  - date
  - battery
  - steps
  - temperature
  - alarm
  - authenticated status
- SoftAP mode for easy standalone device demos

## Sensor Data In This Prototype

The firmware currently uses mock sensor functions instead of real hardware inputs.

That means:

- `readStepSensorMock()` simulates step increments
- `readTemperatureSensorMock()` simulates temperature changes
- `readBatterySensorMock()` simulates battery drain

This is intentional for the diploma prototype. It allows the firmware to compile, run, and demonstrate the full smartwatch workflow before real hardware is connected.

When moving to a physical device, these mock functions can be replaced by:

- accelerometer-based step detection
- a real temperature sensor
- ADC or battery fuel-gauge readings

## Networking Model

The sketch starts:

- a SoftAP:
  - SSID: `SmartWatchDemo`
  - password: `12345678`

It can also connect to an existing Wi-Fi network if you fill in credentials in the sketch.

## API

- `GET /`
- `GET /status`
- `POST /auth`
- `POST /sync-time`
- `POST /alarm`
- `POST /lock`
- `POST /screen`

## Arduino IDE Setup

1. Open Arduino IDE.
2. Add the ESP32 Board Manager URL:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

3. Install `esp32` from Boards Manager.
4. Open `watch_firmware_arduino.ino`.
5. Select `ESP32 Dev Module` or your actual ESP32 board.
6. Compile or upload.

## Demo Positioning

For diploma presentation:

- use `watch_sim/` for the fully local runnable demo without hardware
- use `watch_firmware_arduino/` to show that the same smartwatch logic is already structured as ESP32 firmware for future physical manufacturing

## Why This Is Close To A Real Device

This sketch is still intentionally simple, but it is already organized like embedded firmware:

- state is stored in a dedicated watch model
- HTTP routes are separated from display rendering
- display output is prepared through screen-specific render functions
- the code can later swap the browser display layer for a real OLED or LVGL screen layer

In a next hardware step, the `renderMainScreen()`, `renderStepsScreen()`, `renderBatteryScreen()`, and `renderAlarmScreen()` functions can be redirected from HTML/Serial output to:

- SSD1306 OLED drawing calls
- TFT display drawing
- LVGL widgets and screen objects

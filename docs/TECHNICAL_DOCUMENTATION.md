# Smart Watch Diploma Prototype

## Technical Documentation

This document contains a detailed technical description of the diploma-project prototype. It is intended for:

- diploma defense preparation
- project handoff
- source code orientation
- explaining architectural and implementation decisions
- mapping source files to functional responsibility

The project is a local-first prototype of an intelligent smartwatch system with mobile authentication. It consists of:

- a local smartwatch simulator
- an Android mobile application
- an ESP32/Arduino firmware prototype

The prototype is intentionally designed for stable local demonstration without physical hardware.

## 1. Project Purpose

The goal of the project is to demonstrate a complete smartwatch system in which:

- a mobile application authenticates the user with a PIN
- the smartwatch exposes device state through a local API
- the mobile app can read and modify smartwatch state
- the watch UI can be shown without real hardware
- the system architecture remains realistic enough to migrate onto a physical ESP32-based device later

This is why the project combines:

- `watch_sim/` as the local software-based smartwatch replacement
- `android_app/` as the mobile client
- `watch_firmware_arduino/` as the hardware-ready firmware layer

## 2. High-Level Architecture

### 2.1 Main Components

The monorepo contains three meaningful modules:

1. `watch_sim/`
   A local C++ smartwatch simulator.
   It acts as the primary backend for the demo.

2. `android_app/`
   A Kotlin Android app that runs in the Android Studio emulator.
   It authenticates and controls the simulated watch through local HTTP.

3. `watch_firmware_arduino/`
   An Arduino IDE compatible ESP32 sketch.
   It mirrors the smartwatch logic to show hardware readiness.

### 2.2 Runtime Communication

For the actual demo flow:

- `watch_sim` runs on the host machine
- `android_app` runs inside the Android emulator
- the emulator accesses the host using `http://10.0.2.2:8080`
- the browser-based watch UI is served by `watch_sim` at `http://127.0.0.1:8080/`

### 2.3 Why HTTP Instead of BLE

BLE was intentionally avoided as the primary communication layer because:

- local emulator + simulator demos are less reliable with BLE
- HTTP is easier to debug and explain
- HTTP is stable in IDE and local development workflows
- the diploma goal is system behavior demonstration, not low-level radio integration

## 3. Monorepo Structure

```text
.
├── README.md
├── docs/
│   └── TECHNICAL_DOCUMENTATION.md
├── android_app/
│   ├── README.md
│   └── app/
├── watch_sim/
│   ├── README.md
│   ├── Makefile
│   └── src/
│       └── main.cpp
└── watch_firmware_arduino/
    ├── README.md
    └── watch_firmware_arduino.ino
```

Each module has a different role:

- `watch_sim/` is the live demo backend
- `android_app/` is the mobile user interface
- `watch_firmware_arduino/` is the embedded firmware prototype

## 4. System Behavior Overview

The full system behavior is as follows:

1. The watch starts in a locked state.
2. The mobile app asks the user for a PIN.
3. The app sends `POST /auth` to the watch.
4. If the PIN is correct, the watch changes `authenticated` to `true`.
5. The app can then:
   - fetch current watch status
   - sync the watch time
   - set an alarm
   - lock the watch again
6. The watch simulator updates its UI, state, and API output based on these actions.

## 5. `watch_sim/` Detailed Documentation

`watch_sim` is the most important part of the demo because it replaces real hardware during the defense.

### 5.1 Main Responsibilities

`watch_sim` is responsible for:

- storing current watch state
- simulating time-related and sensor-like data
- exposing local HTTP endpoints
- serving a browser-based watch UI
- rendering a terminal OLED-like screen
- supporting basic keyboard interaction

### 5.2 Main Source Files

- [watch_sim/src/main.cpp](/Users/a13x0r3d/Documents/New%20project/watch_sim/src/main.cpp)
- [watch_sim/Makefile](/Users/a13x0r3d/Documents/New%20project/watch_sim/Makefile)
- [watch_sim/README.md](/Users/a13x0r3d/Documents/New%20project/watch_sim/README.md)

### 5.3 Internal State Model

The simulator keeps the following logical fields:

- current time
- current day of week
- current date
- battery level
- steps count
- temperature
- alarm time
- authentication state
- currently selected watch screen

These fields are stored in the `WatchState` class.

### 5.4 Important Constants and Global State

In [watch_sim/src/main.cpp](/Users/a13x0r3d/Documents/New%20project/watch_sim/src/main.cpp):

- `kServerPort`
  Defines the local HTTP port. Current value: `8080`.

- `kValidPin`
  Stores the demo PIN. Current value: `1234`.

- `kHtmlContentType`
  Used when serving the browser UI.

- `kJsonContentType`
  Used for JSON API responses.

- `g_running`
  Global lifecycle flag used by the background threads and shutdown logic.

### 5.5 Helper Functions in `main.cpp`

These functions provide low-level parsing and utility behavior:

- `trim(const std::string&)`
  Removes leading and trailing whitespace from strings.
  Used while parsing HTTP headers.

- `toLower(std::string)`
  Lowercases a string.
  Used for case-insensitive header detection.

- `parseContentLength(const std::string&)`
  Extracts the `Content-Length` header from the HTTP request head.
  This is important for correctly reading request bodies from Android `HttpURLConnection`.

- `normalizePath(std::string)`
  Normalizes request paths and strips query strings or full URL prefixes.
  This prevents route mismatches.

- `extractJsonString(const std::string&, const std::string&)`
  A minimal JSON string extractor used for very simple request bodies such as:
  - `{ "pin": "1234" }`
  - `{ "time": "07:30" }`

- `isValidTimeString(const std::string&)`
  Validates `HH:mm` format.

### 5.6 Browser Watch UI

The browser UI is generated by:

- `watchUiHtml()`

Its responsibilities:

- produce full HTML/CSS/JS for the watch page
- display the visual watch frame and screen
- display live status cards
- provide `UA / EN` language switching
- refresh the watch state every 1.5 seconds
- allow local actions from the browser:
  - previous screen
  - next screen
  - lock/unlock
  - manual refresh

The page uses client-side JavaScript for:

- polling `/status`
- translating UI labels
- translating day names and screen names
- remembering selected UI language in `localStorage`

### 5.7 Watch Screen Model

The simulator uses an internal enum:

- `WatchScreen::Main`
- `WatchScreen::Steps`
- `WatchScreen::Battery`
- `WatchScreen::Alarm`

This allows both the terminal UI and browser UI to stay synchronized.

### 5.8 `WatchState` Class

The class `WatchState` is the main application model of the simulator.

#### Constructor

The constructor initializes:

- `timeOffsetMinutes_`
- `batteryLevel_`
- `stepsCount_`
- `temperatureCelsius_`
- `alarmTime_`
- `authenticated_`
- `currentScreen_`
- background simulation timestamps

#### Main Public Methods

- `statusJson()`
  Serializes the current watch state into JSON for `GET /status`.

- `authenticate(const std::string& pin)`
  Compares the incoming PIN against `kValidPin`.
  Returns `true` only for the correct PIN.

- `isAuthenticated()`
  Returns current authentication status.

- `syncTime(const std::string& hhmm)`
  Updates the watch time offset if the watch is authenticated and the format is valid.

- `setAlarm(const std::string& hhmm)`
  Stores a new alarm if the watch is authenticated and the format is valid.

- `setLocked(bool locked)`
  Toggles the logical lock state by changing `authenticated_`.

- `nextScreen()`
  Cycles to the next logical watch screen.

- `previousScreen()`
  Cycles to the previous logical watch screen.

- `tickSimulation()`
  Advances simulated watch behavior:
  - increases steps
  - drains battery
  - shifts temperature

- `renderScreen()`
  Renders the terminal OLED-like output using ANSI clearing and formatted text.

#### Important Private Methods

- `randomInt(int min, int max)`
  Generates bounded random values.

- `currentTimeLocked()`
  Returns the current watch time with the configured offset.

- `currentDayLocked()`
  Returns current day of week.

- `currentDateLocked()`
  Returns current date.

- `screenNameLocked()`
  Converts current screen enum to string representation.

### 5.9 HTTP Response Builder

- `makeHttpResponse(...)`
  Builds a raw HTTP response string.
  This function explicitly sets:
  - status line
  - content type
  - `Access-Control-Allow-Origin`
  - `Content-Length`
  - `Connection: close`

### 5.10 HTTP Request Router

- `handleRequest(WatchState&, const std::string&)`

This is the central route dispatcher.

Supported routes:

- `GET /`
  Returns browser watch UI.

- `GET /status`
  Returns full watch state JSON.

- `POST /auth`
  Checks the PIN.

- `POST /sync-time`
  Updates time.
  Requires authentication.

- `POST /alarm`
  Updates alarm.
  Requires authentication.

- `POST /lock`
  Locks or unlocks the watch.

- `POST /screen`
  Changes active watch screen.

If no route matches, the function returns:

- `404 Route not found`

### 5.11 HTTP Server Thread

- `serveHttp(WatchState&)`

Responsibilities:

- creates the TCP socket
- binds to port `8080`
- listens for incoming connections
- accepts clients
- reads the incoming request
- waits until the request body is complete
- sends the generated response

Important note:

This implementation uses POSIX sockets, which work well on macOS/Linux but require adaptation for native Windows support.

### 5.12 Keyboard Loop

- `keyboardLoop(WatchState&)`

Responsibilities:

- read one character at a time from standard input
- map keys to watch actions

Supported keys:

- `n`
- `p`
- `l`
- `q`

### 5.13 Signal Handling

- `signalHandler(int)`

Used to stop the simulator cleanly on `Ctrl+C`.

### 5.14 Main Entry Point

- `main()`

Responsibilities:

- register `SIGINT` handler
- create `WatchState`
- start HTTP server thread
- start keyboard input thread
- continuously update simulation
- continuously render terminal screen

### 5.15 Build Script

[watch_sim/Makefile](/Users/a13x0r3d/Documents/New%20project/watch_sim/Makefile) is intentionally simple.

Targets:

- `all`
  Builds the simulator.

- `run`
  Builds and runs the simulator.

- `clean`
  Removes the built binary.

Current limitation:

The Makefile is Unix-oriented and not a native Windows build solution.

## 6. `android_app/` Detailed Documentation

`android_app` is the mobile front-end for the smartwatch system.

### 6.1 Main Responsibilities

The Android application:

- authenticates the user with a PIN
- displays watch status
- lets the user trigger watch actions
- handles Ukrainian and English localization
- shows a local notification when watch time reaches alarm time

### 6.2 Android Networking Model

The app uses:

- `http://10.0.2.2:8080`

Why:

- `10.0.2.2` is the Android emulator alias for host localhost
- this allows the emulator to call the host-side `watch_sim`

### 6.3 Main Android Files

- [android_app/app/src/main/AndroidManifest.xml](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/AndroidManifest.xml)
- [LoginActivity.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/LoginActivity.kt)
- [DashboardActivity.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/DashboardActivity.kt)
- [WatchApiClient.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/WatchApiClient.kt)
- [WatchStatus.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/WatchStatus.kt)
- [LocaleHelper.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/LocaleHelper.kt)
- [activity_login.xml](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/res/layout/activity_login.xml)
- [activity_dashboard.xml](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/res/layout/activity_dashboard.xml)

### 6.4 Android Manifest

[AndroidManifest.xml](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/AndroidManifest.xml) declares:

- `INTERNET` permission
- `POST_NOTIFICATIONS` permission
- cleartext networking support
- custom `network_security_config`
- launcher activity: `LoginActivity`

### 6.5 `WatchStatus` Data Class

[WatchStatus.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/WatchStatus.kt)

This is the Android-side representation of watch state.

Fields:

- `time`
- `day`
- `date`
- `battery`
- `steps`
- `temperature`
- `alarm`
- `authenticated`

### 6.6 `WatchApiClient`

[WatchApiClient.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/WatchApiClient.kt) is the HTTP client layer.

#### Core Constants and Structures

- `BASE_URL`
  Points to the host-side watch simulator.

- `ApiResult`
  Holds:
  - request success flag
  - HTTP code
  - response body

#### Main Public Functions

- `getStatus()`
  Calls `GET /status` and parses the result into `WatchStatus`.

- `authenticate(pin: String)`
  Calls `POST /auth`.
  Returns `false` for `401 Unauthorized`.

- `syncTime(time: String)`
  Calls `POST /sync-time`.

- `setAlarm(time: String)`
  Calls `POST /alarm`.

- `setLocked(locked: Boolean)`
  Calls `POST /lock`.

#### Private Helper Functions

- `postTime(path, time)`
  Shared helper for `syncTime` and `setAlarm`.

- `request(method, path, body)`
  Performs the actual HTTP call through `HttpURLConnection`.

- `parseError(body)`
  Reads an `error` field from JSON error responses.

### 6.7 `LocaleHelper`

[LocaleHelper.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/LocaleHelper.kt)

Responsibilities:

- persist selected language in `SharedPreferences`
- return wrapped localized `Context`
- update locale for both screens

Important design choice:

- default language is `uk`

### 6.8 `LoginActivity`

[LoginActivity.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/LoginActivity.kt)

Responsibilities:

- render login screen
- validate PIN input length
- send authentication request
- show request progress
- navigate to dashboard on success
- allow `UA / EN` switching

#### Key Methods

- `attachBaseContext(...)`
  Applies currently selected language.

- `onCreate(...)`
  Initializes UI, binds buttons, and handles login behavior.

- `setupLanguageSwitcher(...)`
  Sets up language switcher listeners.

- `switchLanguageIfNeeded(...)`
  Updates saved language and recreates activity.

- `updateLanguageButtons(...)`
  Visually marks active language button.

### 6.9 `DashboardActivity`

[DashboardActivity.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/DashboardActivity.kt)

Responsibilities:

- display current watch status
- trigger watch actions
- update UI after backend requests
- show alarm notification
- allow language switching

#### Key Fields

The activity stores references to:

- status labels
- action input fields
- progress bar
- current alarm notification state

#### Key Methods

- `attachBaseContext(...)`
  Applies selected locale.

- `onCreate(...)`
  Initializes the full dashboard and binds action buttons.

- `setupLanguageSwitcher(...)`
  Connects `UA / EN` buttons.

- `switchLanguageIfNeeded(...)`
  Changes current app language.

- `updateLanguageButtons(...)`
  Updates the visual state of language buttons.

- `refreshStatus()`
  Loads live status from the watch backend.

- `bindStatus(status)`
  Writes watch data into the visible UI.
  Also checks alarm condition and notification trigger.

- `runRequest(...)`
  Generic background-request runner with loading state and error handling.

- `showAlarmNotification(alarm)`
  Shows a local notification when watch time equals alarm time.

- `createNotificationChannel()`
  Creates Android notification channel for API level 26+.

### 6.10 Android Layouts

#### `activity_login.xml`

Contains:

- language switcher
- title and subtitle
- PIN input
- authenticate button
- progress bar
- status text

#### `activity_dashboard.xml`

Contains:

- language switcher
- watch status card
- action card
- buttons for:
  - fetch status
  - sync time
  - set alarm
  - lock watch

### 6.11 Localization Resources

The app uses:

- [values/strings.xml](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/res/values/strings.xml)
- [values-en/strings.xml](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/res/values-en/strings.xml)

Supported UI languages:

- Ukrainian
- English

### 6.12 Visual Resources

- `colors.xml`
  Defines brand palette.

- `themes.xml`
  Defines Material-based app theme.

- `screen_background.xml`
  Provides soft gradient background for both screens.

### 6.13 Network Security Config

[network_security_config.xml](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/res/xml/network_security_config.xml)

Allows cleartext HTTP for:

- `10.0.2.2`
- `127.0.0.1`
- `localhost`

This is necessary because the prototype intentionally uses local HTTP instead of TLS.

## 7. `watch_firmware_arduino/` Detailed Documentation

`watch_firmware_arduino` is the firmware-oriented version of the watch logic.

### 7.1 Purpose

This module exists to demonstrate that:

- the watch logic is already close to embedded firmware
- the system can later be moved onto a real ESP32 board
- the diploma prototype is not just a desktop-only mockup

### 7.2 Main Source File

- [watch_firmware_arduino/watch_firmware_arduino.ino](/Users/a13x0r3d/Documents/New%20project/watch_firmware_arduino/watch_firmware_arduino.ino)

### 7.3 Main Data Structures

#### `enum ScreenId`

Defines logical screen identifiers:

- `SCREEN_MAIN`
- `SCREEN_STEPS`
- `SCREEN_BATTERY`
- `SCREEN_ALARM`

#### `struct WatchState`

Holds:

- current time
- battery level
- steps count
- temperature
- alarm time
- authentication flag
- day of week
- current date
- active screen
- timestamps for simulated updates

#### `namespace WatchConfig`

Stores firmware configuration:

- station Wi-Fi credentials
- SoftAP SSID
- SoftAP password
- valid PIN

### 7.4 Important Functions

#### UI and Screen Helpers

- `screenName(ScreenId)`
  Converts enum screen to text.

- `authLabel()`
  Converts auth state to human-readable text.

- `nextScreen(...)`
  Returns next screen.

- `prevScreen(...)`
  Returns previous screen.

- `screenHeadline()`
  Produces headline text for active screen.

- `renderMainScreen()`
- `renderStepsScreen()`
- `renderBatteryScreen()`
- `renderAlarmScreen()`
- `renderActiveScreenBody()`

These functions mimic an OLED/LVGL-style separation of screen content.

#### Mock Sensor Functions

- `readStepSensorMock()`
- `readTemperatureSensorMock(int)`
- `readBatterySensorMock(int)`

These abstract the idea of future hardware sensor reading.

#### HTML UI

- `htmlPage()`
  Generates the browser UI served by the ESP32.

#### Parsing and Validation

- `readBody()`
  Reads raw request body from `WebServer`.

- `jsonValue(...)`
  Extracts string values from small JSON payloads.

- `validTime(...)`
  Validates `HH:mm`.

#### State and Serialization

- `updateCalendarFields()`
  Updates day, date, and time from the ESP32 time source.

- `stateJson()`
  Serializes current watch state as JSON.

- `sendJson(...)`
  Sends JSON response through `WebServer`.

#### Route Handlers

- `handleRoot()`
- `handleStatus()`
- `handleAuth()`
- `handleSyncTime()`
- `handleAlarm()`
- `handleLock()`
- `handleScreen()`

These mirror the simulator API behavior.

#### Background Behavior

- `simulateSensors()`
  Updates steps, battery, and temperature over time.

- `printStatusToSerial()`
  Prints textual watch state to Serial Monitor.

#### Networking and Lifecycle

- `connectNetworking()`
  Starts SoftAP and optionally station mode.

- `setup()`
  Initializes Serial, networking, time sync, routes, and HTTP server.

- `loop()`
  Continuously simulates sensors, handles clients, and prints status.

### 7.5 Arduino Network Model

The firmware starts:

- a SoftAP called `SmartWatchDemo`
- password `12345678`

Optionally, it can also join an existing Wi-Fi network when credentials are added into the sketch.

## 8. API Documentation

The prototype API is intentionally small and stable.

### 8.1 `GET /status`

Returns the current full watch state.

Example:

```json
{
  "time": "20:30",
  "day": "Monday",
  "date": "21.04.2026",
  "screen": "Main",
  "battery": 87,
  "steps": 1534,
  "temperature": 24,
  "alarm": "07:30",
  "authenticated": true
}
```

### 8.2 `POST /auth`

Authenticates the watch using a PIN.

Request:

```json
{
  "pin": "1234"
}
```

Success response:

```json
{
  "success": true,
  "authenticated": true
}
```

Error response:

```json
{
  "success": false,
  "authenticated": false,
  "error": "Invalid PIN"
}
```

### 8.3 `POST /sync-time`

Requires authentication.

Request:

```json
{
  "time": "20:31"
}
```

Success response:

```json
{
  "success": true,
  "time": "20:31"
}
```

### 8.4 `POST /alarm`

Requires authentication.

Request:

```json
{
  "time": "07:30"
}
```

Success response:

```json
{
  "success": true,
  "alarm": "07:30"
}
```

### 8.5 `POST /lock`

Changes watch lock state.

Request:

```json
{
  "locked": "true"
}
```

Response:

```json
{
  "authenticated": false
}
```

### 8.6 `POST /screen`

Changes active watch screen.

Request:

```json
{
  "action": "next"
}
```

Response:

- returns the updated full status JSON

## 9. Data Simulation Model

The project uses software-based mock data instead of real sensors.

### 9.1 Simulated Values

Currently simulated:

- steps
- battery level
- temperature

### 9.2 Why This Was Chosen

Reasons:

- no physical hardware is required
- the prototype remains stable in local environments
- the architecture still matches a real smartwatch data model
- the mobile app and firmware can already be built around realistic fields

### 9.3 Real Hardware Mapping

Future replacement path:

- steps -> accelerometer + step detection algorithm
- temperature -> temperature sensor
- battery -> ADC / PMIC / battery fuel gauge

## 10. Authentication Model

Authentication in this prototype is intentionally simple.

### 10.1 Current Strategy

- one static PIN is used
- valid value: `1234`

### 10.2 Protected Actions

The following actions are blocked unless authenticated:

- time synchronization
- alarm update

### 10.3 Why This Is Enough For The Prototype

The goal is not production security.
The goal is to demonstrate:

- user identity check from the mobile app
- watch-side state transition from locked to unlocked
- protected control flow

## 11. Localization

### 11.1 Android App

Supports:

- Ukrainian
- English

Implementation:

- `strings.xml`
- `values-en/strings.xml`
- `LocaleHelper`

### 11.2 Browser Watch UI

The simulator browser UI supports:

- Ukrainian
- English

Implementation:

- JavaScript translation object inside `watchUiHtml()`
- local browser persistence through `localStorage`

## 12. Demo and Defense Usage

### 12.1 Minimal Demo Steps

1. Start `watch_sim`.
2. Open browser watch UI.
3. Run Android app in emulator.
4. Enter PIN `1234`.
5. Fetch status.
6. Sync time.
7. Set alarm.
8. Lock the watch again.

### 12.2 Defense Framing

The recommended explanation is:

- `watch_sim` replaces physical watch hardware for stable local demo
- `android_app` is the mobile user control layer
- `watch_firmware_arduino` proves that the same logic is already structured for ESP32 hardware migration

## 13. Platform Compatibility

### 13.1 macOS

Current local demo is primarily developed and verified on macOS.

### 13.2 Linux

`watch_sim` is conceptually close to Linux support due to POSIX sockets and standard C++.

### 13.3 Windows

`android_app` and Arduino IDE usage are practical on Windows.

However, `watch_sim` is not yet fully native Windows-ready because it currently uses:

- POSIX socket headers and APIs
- Unix-style Makefile
- Unix-oriented terminal behavior

For native Windows support, `watch_sim` would need:

- Winsock compatibility layer
- cross-platform build system such as CMake
- possibly softer terminal rendering assumptions

## 14. Limitations

Current prototype limitations:

- no BLE communication
- no cloud backend
- no persistent storage
- no multi-user model
- no production security
- no real hardware sensors
- simulator not yet fully native Windows-cross-platform

These limitations are intentional for diploma-demo simplicity and reliability.

## 15. Possible Future Improvements

Potential next steps:

- replace mock sensor data with real sensor input
- move watch logic fully onto ESP32 hardware
- implement BLE or Wi-Fi device discovery later
- add persistent watch settings storage
- add richer watch notification system
- make `watch_sim` cross-platform with CMake + Windows socket support
- replace HTML/terminal display with real OLED or LVGL rendering

## 16. Recommended Files To Study First

If someone needs to understand the project quickly, the best reading order is:

1. [README.md](/Users/a13x0r3d/Documents/New%20project/README.md)
2. [watch_sim/src/main.cpp](/Users/a13x0r3d/Documents/New%20project/watch_sim/src/main.cpp)
3. [android_app/app/src/main/java/com/example/smartwatchdemo/WatchApiClient.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/WatchApiClient.kt)
4. [android_app/app/src/main/java/com/example/smartwatchdemo/LoginActivity.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/LoginActivity.kt)
5. [android_app/app/src/main/java/com/example/smartwatchdemo/DashboardActivity.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/DashboardActivity.kt)
6. [watch_firmware_arduino/watch_firmware_arduino.ino](/Users/a13x0r3d/Documents/New%20project/watch_firmware_arduino/watch_firmware_arduino.ino)

## 17. Final Summary

This diploma prototype is intentionally built as a practical demonstration system rather than a production smartwatch platform.

Its main strength is the combination of:

- a runnable local simulator
- a working mobile application
- a firmware-oriented ESP32 prototype

This makes it strong for demonstration, explanation, and later extension into a real embedded implementation.

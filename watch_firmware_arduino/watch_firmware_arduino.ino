#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

// Перелік логічних екранів годинника, близький до того, як це робиться у firmware UI.
enum ScreenId {
  SCREEN_MAIN,
  SCREEN_STEPS,
  SCREEN_BATTERY,
  SCREEN_ALARM
};

struct WatchState {
  String currentTime = "20:30";
  int batteryLevel = 100;
  int stepsCount = 1200;
  int temperatureCelsius = 23;
  String alarmTime = "07:30";
  bool authenticated = false;
  String dayOfWeek = "Tuesday";
  String currentDate = "21.04.2026";
  ScreenId activeScreen = SCREEN_MAIN;
  unsigned long lastStepTick = 0;
  unsigned long lastBatteryTick = 0;
  unsigned long lastTemperatureTick = 0;
};

// Конфігурація мережі та демонстраційних параметрів винесена окремо,
// щоб у майбутньому її було легко замінити на реальні налаштування пристрою.
namespace WatchConfig {
  const char* stationSsid = "";
  const char* stationPassword = "";
  const char* softApSsid = "SmartWatchDemo";
  const char* softApPassword = "12345678";
  const char* validPin = "1234";
}

WatchState watchState;
WebServer server(80);

// Повертає текстову назву екрана для API, Serial і web UI.
String screenName(ScreenId screen) {
  switch (screen) {
    case SCREEN_MAIN: return "Main";
    case SCREEN_STEPS: return "Steps";
    case SCREEN_BATTERY: return "Battery";
    case SCREEN_ALARM: return "Alarm";
  }
  return "Main";
}

// Текстовий статус блокування/розблокування годинника.
String authLabel() {
  return watchState.authenticated ? "Unlocked" : "Locked";
}

// Допоміжна навігація між екранами вперед по колу.
ScreenId nextScreen(ScreenId screen) {
  switch (screen) {
    case SCREEN_MAIN: return SCREEN_STEPS;
    case SCREEN_STEPS: return SCREEN_BATTERY;
    case SCREEN_BATTERY: return SCREEN_ALARM;
    case SCREEN_ALARM: return SCREEN_MAIN;
  }
  return SCREEN_MAIN;
}

// Допоміжна навігація між екранами назад по колу.
ScreenId prevScreen(ScreenId screen) {
  switch (screen) {
    case SCREEN_MAIN: return SCREEN_ALARM;
    case SCREEN_STEPS: return SCREEN_MAIN;
    case SCREEN_BATTERY: return SCREEN_STEPS;
    case SCREEN_ALARM: return SCREEN_BATTERY;
  }
  return SCREEN_MAIN;
}

// Короткий головний рядок для активного екрана.
String screenHeadline() {
  switch (watchState.activeScreen) {
    case SCREEN_MAIN:
      return "Steps " + String(watchState.stepsCount) + " | Temp " + String(watchState.temperatureCelsius) + " C";
    case SCREEN_STEPS:
      return String(watchState.stepsCount) + " steps today";
    case SCREEN_BATTERY:
      return "Battery " + String(watchState.batteryLevel) + "%";
    case SCREEN_ALARM:
      return "Alarm " + watchState.alarmTime;
  }
  return "Watch ready";
}

// Далі йде набір функцій, який стилістично наближає код до OLED/LVGL-підходу:
// логіка екранів відділена від мережі та стану.
String renderMainScreen() {
  return "<div class=\"value\">" + screenHeadline() + "</div>";
}

String renderStepsScreen() {
  return "<div class=\"value\">" + String(watchState.stepsCount) + " steps</div>";
}

String renderBatteryScreen() {
  return "<div class=\"value\">Battery " + String(watchState.batteryLevel) + "%</div>";
}

String renderAlarmScreen() {
  return "<div class=\"value\">Alarm " + watchState.alarmTime + "</div>";
}

String renderActiveScreenBody() {
  switch (watchState.activeScreen) {
    case SCREEN_MAIN: return renderMainScreen();
    case SCREEN_STEPS: return renderStepsScreen();
    case SCREEN_BATTERY: return renderBatteryScreen();
    case SCREEN_ALARM: return renderAlarmScreen();
  }
  return renderMainScreen();
}

int readStepSensorMock() {
  return random(12, 34);
}

// Mock-функції показують, де в реальному пристрої читались би фізичні сенсори.
int readTemperatureSensorMock(int currentValue) {
  return constrain(currentValue + random(-1, 2), 18, 30);
}

int readBatterySensorMock(int currentValue) {
  return max(5, currentValue - 1);
}

String htmlPage() {
  // HTML-сторінка потрібна для локальної демонстрації без фізичного дисплея.
  String page = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Watch UI</title>
  <style>
    :root {
      --bg: #f4efe6; --panel: #f8f3eb; --ink: #132238; --muted: #5f6f82;
      --accent: #12708f; --accent-2: #dd6b20; --watch: #1e293b; --edge: #475569;
      --screen: #07150f; --glow: #96f9c2; --dim: #5bbd8d;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0; min-height: 100vh; font-family: "Avenir Next", "Segoe UI", sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at top left, #fff6d6 0, transparent 28%),
        radial-gradient(circle at bottom right, #d8f6ef 0, transparent 34%),
        linear-gradient(135deg, var(--bg), #e9eef7);
      display: grid; place-items: center; padding: 24px;
    }
    .layout { width: min(1080px, 100%); display: grid; gap: 24px; grid-template-columns: minmax(320px, 420px) 1fr; align-items: center; }
    .shell { position: relative; background: linear-gradient(160deg, var(--edge), var(--watch)); border-radius: 42px; padding: 18px; box-shadow: 0 20px 50px rgba(15,23,42,.28); aspect-ratio: 1 / 1.1; }
    .strap { position: absolute; left: 50%; transform: translateX(-50%); width: 42%; height: 90px; background: linear-gradient(180deg, #2e3c52, #101826); border-radius: 24px; z-index: -1; }
    .top { top: -70px; } .bottom { bottom: -70px; }
    .screen {
      height: 100%; border-radius: 30px; background:
      linear-gradient(180deg, rgba(155,255,204,.08), rgba(80,170,120,.04)),
      radial-gradient(circle at top, rgba(144,255,194,.12), transparent 45%), var(--screen);
      color: var(--glow); padding: 22px 20px; display: flex; flex-direction: column;
      border: 2px solid rgba(151,249,194,.12); box-shadow: inset 0 0 18px rgba(98,255,181,.08);
    }
    .row { display: flex; justify-content: space-between; align-items: center; color: var(--dim); font-size: 12px; text-transform: uppercase; letter-spacing: .08em; }
    .main { flex: 1; display: flex; flex-direction: column; justify-content: center; gap: 14px; }
    .time { font-size: clamp(42px, 9vw, 60px); line-height: 1; font-weight: 700; letter-spacing: .05em; }
    .value { font-size: 28px; font-weight: 700; }
    .meta { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 14px; color: var(--dim); }
    .card { border: 1px solid rgba(151,249,194,.16); border-radius: 14px; padding: 10px 12px; background: rgba(151,249,194,.03); }
    .panel { background: rgba(255,255,255,.72); backdrop-filter: blur(8px); border-radius: 26px; padding: 24px; box-shadow: 0 20px 40px rgba(15,23,42,.12); }
    h1 { margin: 0; font-size: clamp(28px, 4vw, 42px); line-height: 1.05; }
    .subtitle { margin: 10px 0 0; color: var(--muted); max-width: 45ch; line-height: 1.5; }
    .grid { margin-top: 22px; display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 12px; }
    .stat { padding: 14px; border-radius: 16px; background: var(--panel); border: 1px solid rgba(19,34,56,.06); }
    .label { font-size: 12px; text-transform: uppercase; letter-spacing: .08em; color: var(--muted); }
    .stat-value { margin-top: 8px; font-size: 22px; font-weight: 700; }
    .controls { margin-top: 20px; display: flex; flex-wrap: wrap; gap: 10px; }
    button { border: 0; border-radius: 999px; padding: 12px 16px; font-size: 14px; font-weight: 700; cursor: pointer; color: white; background: var(--accent); }
    button.alt { background: var(--accent-2); } button.ghost { background: white; color: var(--ink); border: 1px solid rgba(19,34,56,.12); }
    .pill { display: inline-flex; align-items: center; gap: 8px; padding: 6px 10px; border-radius: 999px; background: rgba(18,112,143,.08); color: var(--accent); font-size: 12px; font-weight: 700; letter-spacing: .06em; text-transform: uppercase; }
    .note { margin-top: 18px; color: var(--muted); font-size: 14px; line-height: 1.5; }
    @media (max-width: 900px) { .layout { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
  <div class="layout">
    <div class="shell">
      <div class="strap top"></div><div class="strap bottom"></div>
      <div class="screen">
        <div class="row"><span id="screenName">)HTML";
  page += screenName(watchState.activeScreen);
  page += R"HTML(</span><span id="authState">)HTML";
  page += authLabel();
  page += R"HTML(</span></div>
        <div class="main">
          <div class="time" id="watchTime">)HTML";
  page += watchState.currentTime;
  page += R"HTML(</div>
          <div id="screenBody">)HTML";
  page += renderActiveScreenBody();
  page += R"HTML(</div>
          <div class="meta">
            <div class="card"><div id="watchDay">)HTML";
  page += watchState.dayOfWeek;
  page += R"HTML(</div><div id="watchDate">)HTML";
  page += watchState.currentDate;
  page += R"HTML(</div></div>
            <div class="card"><div id="watchBattery">Battery )HTML";
  page += String(watchState.batteryLevel);
  page += R"HTML(%</div><div id="watchTemp">Temp )HTML";
  page += String(watchState.temperatureCelsius);
  page += R"HTML( C</div></div>
          </div>
        </div>
        <div class="row"><span id="watchAlarm">Alarm )HTML";
  page += watchState.alarmTime;
  page += R"HTML(</span><span id="watchSteps">)HTML";
  page += String(watchState.stepsCount);
  page += R"HTML( steps</span></div>
      </div>
    </div>
    <div class="panel">
      <div class="pill">ESP32 Firmware UI</div>
      <h1>Hardware-ready smartwatch firmware</h1>
      <p class="subtitle">This UI is served directly by the ESP32 firmware sketch. It mirrors the same watch behavior as the local demo simulator and is ready to be moved onto a hardware board.</p>
      <div class="grid">
        <div class="stat"><div class="label">Time</div><div class="stat-value" id="timeCard">--:--</div></div>
        <div class="stat"><div class="label">Date</div><div class="stat-value" id="dateCard">--</div></div>
        <div class="stat"><div class="label">Day</div><div class="stat-value" id="dayCard">--</div></div>
        <div class="stat"><div class="label">Battery</div><div class="stat-value" id="batteryCard">--</div></div>
        <div class="stat"><div class="label">Steps</div><div class="stat-value" id="stepsCard">--</div></div>
        <div class="stat"><div class="label">Temperature</div><div class="stat-value" id="tempCard">--</div></div>
        <div class="stat"><div class="label">Alarm</div><div class="stat-value" id="alarmCard">--</div></div>
        <div class="stat"><div class="label">Status</div><div class="stat-value" id="authCard">--</div></div>
      </div>
      <div class="controls">
        <button class="ghost" onclick="changeScreen('prev')">Prev Screen</button>
        <button onclick="changeScreen('next')">Next Screen</button>
        <button class="alt" onclick="toggleLock()">Lock / Unlock</button>
        <button class="ghost" onclick="refreshStatus()">Refresh Now</button>
      </div>
      <p class="note">When flashed to ESP32, this watch firmware can serve the same REST API and web UI from the hardware device.</p>
    </div>
  </div>
  <script>
    let latestStatus = null;
    async function postJson(path, payload) {
      const response = await fetch(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) });
      if (!response.ok) throw new Error('Request failed');
      return response.json().catch(() => ({}));
    }
    function render(status) {
      latestStatus = status;
      document.getElementById('screenName').textContent = status.screen;
      document.getElementById('authState').textContent = status.authenticated ? 'Unlocked' : 'Locked';
      document.getElementById('watchTime').textContent = status.time;
      document.getElementById('watchDay').textContent = status.day;
      document.getElementById('watchDate').textContent = status.date;
      document.getElementById('watchBattery').textContent = `Battery ${status.battery}%`;
      document.getElementById('watchTemp').textContent = `Temp ${status.temperature} C`;
      document.getElementById('watchAlarm').textContent = `Alarm ${status.alarm}`;
      document.getElementById('watchSteps').textContent = `${status.steps} steps`;
      document.getElementById('timeCard').textContent = status.time;
      document.getElementById('dateCard').textContent = status.date;
      document.getElementById('dayCard').textContent = status.day;
      document.getElementById('batteryCard').textContent = `${status.battery}%`;
      document.getElementById('stepsCard').textContent = status.steps;
      document.getElementById('tempCard').textContent = `${status.temperature} C`;
      document.getElementById('alarmCard').textContent = status.alarm;
      document.getElementById('authCard').textContent = status.authenticated ? 'Unlocked' : 'Locked';
      const labels = { Main: `Steps ${status.steps} | Temp ${status.temperature} C`, Steps: `${status.steps} steps today`, Battery: `Battery ${status.battery}%`, Alarm: `Alarm ${status.alarm}` };
      document.getElementById('screenBody').innerHTML = `<div class="value">${labels[status.screen] || 'Watch ready'}</div>`;
    }
    async function refreshStatus() { try { const r = await fetch('/status'); render(await r.json()); } catch (e) { document.getElementById('screenBody').innerHTML = '<div class="value">Connection lost</div>'; } }
    async function changeScreen(action) { await postJson('/screen', { action }); await refreshStatus(); }
    async function toggleLock() { const shouldLock = !latestStatus || latestStatus.authenticated; await postJson('/lock', { locked: shouldLock ? 'true' : 'false' }); await refreshStatus(); }
    refreshStatus(); setInterval(refreshStatus, 1500);
  </script>
</body>
</html>
)HTML";
  return page;
}

String readBody() {
  if (!server.hasArg("plain")) {
    return "";
  }
  return server.arg("plain");
}

// Дуже простий витяг значення з JSON, достатній для маленьких payload'ів прототипу.
String jsonValue(const String& body, const String& key) {
  String needle = "\"" + key + "\"";
  int keyPos = body.indexOf(needle);
  if (keyPos < 0) return "";
  int firstQuote = body.indexOf('"', body.indexOf(':', keyPos) + 1);
  if (firstQuote < 0) return "";
  int secondQuote = body.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return "";
  return body.substring(firstQuote + 1, secondQuote);
}

// Валідує час у форматі HH:mm перед зміною стану годинника.
bool validTime(const String& value) {
  if (value.length() != 5 || value.charAt(2) != ':') return false;
  for (int i : {0, 1, 3, 4}) {
    if (!isDigit(value.charAt(i))) return false;
  }
  int hours = value.substring(0, 2).toInt();
  int minutes = value.substring(3, 5).toInt();
  return hours >= 0 && hours <= 23 && minutes >= 0 && minutes <= 59;
}

// Оновлює календарні поля з поточного системного часу ESP32.
void updateCalendarFields() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  if (!timeInfo) return;

  char timeBuffer[6];
  char dayBuffer[16];
  char dateBuffer[16];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", timeInfo);
  strftime(dayBuffer, sizeof(dayBuffer), "%A", timeInfo);
  strftime(dateBuffer, sizeof(dateBuffer), "%d.%m.%Y", timeInfo);

  watchState.currentTime = String(timeBuffer);
  watchState.dayOfWeek = String(dayBuffer);
  watchState.currentDate = String(dateBuffer);
}

// Формує повний JSON-стан годинника для REST API.
String stateJson() {
  updateCalendarFields();
  String json = "{";
  json += "\"time\":\"" + watchState.currentTime + "\",";
  json += "\"day\":\"" + watchState.dayOfWeek + "\",";
  json += "\"date\":\"" + watchState.currentDate + "\",";
  json += "\"screen\":\"" + screenName(watchState.activeScreen) + "\",";
  json += "\"battery\":" + String(watchState.batteryLevel) + ",";
  json += "\"steps\":" + String(watchState.stepsCount) + ",";
  json += "\"temperature\":" + String(watchState.temperatureCelsius) + ",";
  json += "\"alarm\":\"" + watchState.alarmTime + "\",";
  json += "\"authenticated\":" + String(watchState.authenticated ? "true" : "false");
  json += "}";
  return json;
}

void sendJson(int code, const String& json) {
  server.send(code, "application/json", json);
}

// Далі йдуть HTTP-обробники. Вони повторюють той самий контракт,
// що й локальний watch_sim, щоб mobile app могла працювати однаково з обома версіями.
void handleRoot() {
  server.send(200, "text/html; charset=utf-8", htmlPage());
}

void handleStatus() {
  sendJson(200, stateJson());
}

void handleAuth() {
  String pin = jsonValue(readBody(), "pin");
  watchState.authenticated = pin == WatchConfig::validPin;
  if (watchState.authenticated) {
    sendJson(200, "{\"success\":true,\"authenticated\":true}");
  } else {
    sendJson(401, "{\"success\":false,\"authenticated\":false,\"error\":\"Invalid PIN\"}");
  }
}

void handleSyncTime() {
  if (!watchState.authenticated) {
    sendJson(403, "{\"error\":\"Authenticate first\"}");
    return;
  }

  String timeValue = jsonValue(readBody(), "time");
  if (!validTime(timeValue)) {
    sendJson(400, "{\"error\":\"Invalid time\"}");
    return;
  }

  watchState.currentTime = timeValue;
  sendJson(200, "{\"success\":true,\"time\":\"" + timeValue + "\"}");
}

void handleAlarm() {
  if (!watchState.authenticated) {
    sendJson(403, "{\"error\":\"Authenticate first\"}");
    return;
  }

  String timeValue = jsonValue(readBody(), "time");
  if (!validTime(timeValue)) {
    sendJson(400, "{\"error\":\"Invalid time\"}");
    return;
  }

  watchState.alarmTime = timeValue;
  sendJson(200, "{\"success\":true,\"alarm\":\"" + timeValue + "\"}");
}

void handleLock() {
  String lockedValue = jsonValue(readBody(), "locked");
  bool locked = lockedValue != "false";
  watchState.authenticated = !locked;
  sendJson(200, "{\"authenticated\":" + String(watchState.authenticated ? "true" : "false") + "}");
}

void handleScreen() {
  String action = jsonValue(readBody(), "action");
  if (action == "next") {
    watchState.activeScreen = nextScreen(watchState.activeScreen);
  } else if (action == "prev") {
    watchState.activeScreen = prevScreen(watchState.activeScreen);
  } else {
    sendJson(400, "{\"error\":\"Invalid action\"}");
    return;
  }

  sendJson(200, stateJson());
}

void simulateSensors() {
  unsigned long now = millis();

  if (now - watchState.lastStepTick >= 5000) {
    // Prototype mode: steps come from a mock sensor layer.
    // On real hardware this would be replaced by accelerometer step detection.
    watchState.stepsCount += readStepSensorMock();
    watchState.lastStepTick = now;
  }

  if (now - watchState.lastBatteryTick >= 20000) {
    // Prototype mode: battery level is simulated.
    // On a real device this can be replaced by ADC or fuel-gauge readings.
    watchState.batteryLevel = readBatterySensorMock(watchState.batteryLevel);
    watchState.lastBatteryTick = now;
  }

  if (now - watchState.lastTemperatureTick >= 15000) {
    // Prototype mode: temperature is simulated.
    // On real hardware this can come from a dedicated temperature sensor.
    watchState.temperatureCelsius = readTemperatureSensorMock(watchState.temperatureCelsius);
    watchState.lastTemperatureTick = now;
  }
}

// Serial Monitor тут виконує роль резервного "екрана" для налагодження firmware.
void printStatusToSerial() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 1000) return;
  lastPrint = millis();

  updateCalendarFields();
  Serial.println();
  Serial.println("[ESP32 WATCH]");
  Serial.println("Screen: " + screenName(watchState.activeScreen));
  Serial.println("Time: " + watchState.currentTime);
  Serial.println("Day: " + watchState.dayOfWeek);
  Serial.println("Date: " + watchState.currentDate);
  Serial.println("Battery: " + String(watchState.batteryLevel) + "%");
  Serial.println("Steps: " + String(watchState.stepsCount));
  Serial.println("Temp: " + String(watchState.temperatureCelsius) + " C");
  Serial.println("Alarm: " + watchState.alarmTime);
  Serial.println("Auth: " + authLabel());
  Serial.println("OLED Render: " + screenHeadline());
}

// Піднімає SoftAP і, за потреби, підключення до зовнішньої Wi-Fi мережі.
void connectNetworking() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(WatchConfig::softApSsid, WatchConfig::softApPassword);

  if (strlen(WatchConfig::stationSsid) > 0) {
    WiFi.begin(WatchConfig::stationSsid, WatchConfig::stationPassword);
  }

  Serial.println("SoftAP IP: " + WiFi.softAPIP().toString());
}

void setup() {
  // У setup() відбувається одноразова ініціалізація всього "пристрою".
  Serial.begin(115200);
  delay(600);
  randomSeed(micros());

  connectNetworking();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/auth", HTTP_POST, handleAuth);
  server.on("/sync-time", HTTP_POST, handleSyncTime);
  server.on("/alarm", HTTP_POST, handleAlarm);
  server.on("/lock", HTTP_POST, handleLock);
  server.on("/screen", HTTP_POST, handleScreen);
  server.begin();

  Serial.println("ESP32 watch firmware started.");
  Serial.println("Open browser UI on the ESP32 IP address.");
}

void loop() {
  // У loop() постійно підтримується життя пристрою: сенсори, HTTP і логування.
  simulateSensors();
  server.handleClient();
  printStatusToSerial();
}

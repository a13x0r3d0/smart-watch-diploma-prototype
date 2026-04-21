#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <random>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr int kServerPort = 8080;
constexpr const char* kValidPin = "1234";
constexpr const char* kHtmlContentType = "text/html; charset=utf-8";
constexpr const char* kJsonContentType = "application/json";

bool g_running = true;

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string toLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::optional<size_t> parseContentLength(const std::string& requestHead) {
    std::istringstream stream(requestHead);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        const auto colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        const auto headerName = toLower(trim(line.substr(0, colonPos)));
        if (headerName != "content-length") {
            continue;
        }

        const auto headerValue = trim(line.substr(colonPos + 1));
        try {
            return static_cast<size_t>(std::stoul(headerValue));
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::string normalizePath(std::string path) {
    const auto schemePos = path.find("://");
    if (schemePos != std::string::npos) {
        const auto firstSlashAfterHost = path.find('/', schemePos + 3);
        path = firstSlashAfterHost == std::string::npos ? "/" : path.substr(firstSlashAfterHost);
    }

    const auto queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);
    }

    return path;
}

std::optional<std::string> extractJsonString(const std::string& body, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto keyPos = body.find(needle);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }

    const auto colonPos = body.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return std::nullopt;
    }

    const auto firstQuote = body.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) {
        return std::nullopt;
    }

    const auto secondQuote = body.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) {
        return std::nullopt;
    }

    return body.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

std::string watchUiHtml() {
    return R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Watch UI</title>
  <style>
    :root {
      --bg: #f4efe6;
      --panel: #f8f3eb;
      --ink: #132238;
      --muted: #5f6f82;
      --accent: #12708f;
      --accent-2: #dd6b20;
      --watch: #1e293b;
      --watch-edge: #475569;
      --screen: #07150f;
      --screen-glow: #96f9c2;
      --screen-dim: #5bbd8d;
    }

    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Avenir Next", "Segoe UI", sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at top left, #fff6d6 0, transparent 28%),
        radial-gradient(circle at bottom right, #d8f6ef 0, transparent 34%),
        linear-gradient(135deg, var(--bg), #e9eef7);
      display: grid;
      place-items: center;
      padding: 24px;
    }

    .layout {
      width: min(1100px, 100%);
      display: grid;
      gap: 24px;
      grid-template-columns: minmax(320px, 420px) minmax(280px, 1fr);
      align-items: center;
    }

    .watch-shell {
      position: relative;
      background: linear-gradient(160deg, var(--watch-edge), var(--watch));
      border-radius: 42px;
      padding: 18px;
      box-shadow: 0 20px 50px rgba(15, 23, 42, 0.28);
      aspect-ratio: 1 / 1.1;
    }

    .strap {
      position: absolute;
      left: 50%;
      transform: translateX(-50%);
      width: 42%;
      height: 90px;
      background: linear-gradient(180deg, #2e3c52, #101826);
      border-radius: 24px;
      z-index: -1;
    }
    .strap.top { top: -70px; }
    .strap.bottom { bottom: -70px; }

    .side-button {
      position: absolute;
      right: -12px;
      width: 10px;
      border-radius: 999px;
      background: linear-gradient(180deg, #94a3b8, #475569);
    }
    .side-button.one { top: 86px; height: 58px; }
    .side-button.two { top: 164px; height: 42px; }

    .watch-screen {
      height: 100%;
      border-radius: 30px;
      background:
        linear-gradient(180deg, rgba(155, 255, 204, 0.08), rgba(80, 170, 120, 0.04)),
        radial-gradient(circle at top, rgba(144, 255, 194, 0.12), transparent 45%),
        var(--screen);
      color: var(--screen-glow);
      padding: 22px 20px;
      display: flex;
      flex-direction: column;
      border: 2px solid rgba(151, 249, 194, 0.12);
      box-shadow: inset 0 0 18px rgba(98, 255, 181, 0.08);
    }

    .watch-topline, .watch-bottomline {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 10px;
      flex-wrap: wrap;
      color: var(--screen-dim);
      font-size: 11px;
      line-height: 1.35;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }

    .watch-topline span, .watch-bottomline span {
      min-width: 0;
      overflow-wrap: anywhere;
    }

    .watch-main {
      flex: 1;
      display: flex;
      flex-direction: column;
      justify-content: center;
      gap: 14px;
    }

    .watch-time {
      font-size: clamp(42px, 9vw, 60px);
      line-height: 1;
      font-weight: 700;
      letter-spacing: 0.05em;
    }

    .watch-value {
      font-size: clamp(22px, 4vw, 28px);
      font-weight: 700;
      line-height: 1.2;
      overflow-wrap: anywhere;
    }

    .watch-meta {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      font-size: 14px;
      color: var(--screen-dim);
    }

    .watch-card {
      border: 1px solid rgba(151, 249, 194, 0.16);
      border-radius: 14px;
      padding: 10px 12px;
      background: rgba(151, 249, 194, 0.03);
    }

    .panel {
      background: rgba(255, 255, 255, 0.72);
      backdrop-filter: blur(8px);
      border-radius: 26px;
      padding: 24px;
      box-shadow: 0 20px 40px rgba(15, 23, 42, 0.12);
    }

    h1 {
      margin: 0;
      font-size: clamp(28px, 4vw, 42px);
      line-height: 1.05;
    }

    .subtitle {
      margin: 10px 0 0;
      color: var(--muted);
      max-width: 52ch;
      line-height: 1.5;
      overflow-wrap: anywhere;
    }

    .status-grid {
      margin-top: 22px;
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 12px;
    }

    .stat {
      padding: 14px;
      border-radius: 16px;
      background: var(--panel);
      border: 1px solid rgba(19, 34, 56, 0.06);
      min-width: 0;
    }

    .stat-label {
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      color: var(--muted);
    }

    .stat-value {
      margin-top: 8px;
      font-size: clamp(18px, 3vw, 22px);
      font-weight: 700;
      line-height: 1.2;
      word-break: normal;
      overflow-wrap: normal;
    }

    .controls {
      margin-top: 20px;
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 10px;
    }

    button {
      border: 0;
      border-radius: 999px;
      padding: 12px 16px;
      font-size: 14px;
      font-weight: 700;
      cursor: pointer;
      color: white;
      background: var(--accent);
      width: 100%;
      line-height: 1.2;
      text-align: center;
    }

    button.alt { background: var(--accent-2); }
    button.ghost {
      background: white;
      color: var(--ink);
      border: 1px solid rgba(19, 34, 56, 0.12);
    }

    .api-note {
      margin-top: 18px;
      color: var(--muted);
      font-size: 14px;
      line-height: 1.5;
    }

    .pill {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 6px 10px;
      border-radius: 999px;
      background: rgba(18, 112, 143, 0.08);
      color: var(--accent);
      font-size: 12px;
      font-weight: 700;
      letter-spacing: 0.06em;
      text-transform: uppercase;
    }

    .panel-top {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 12px;
      flex-wrap: wrap;
    }

    .lang-switcher {
      display: inline-flex;
      gap: 8px;
      align-items: center;
    }

    .lang-switcher button {
      padding: 8px 12px;
      font-size: 12px;
      background: white;
      color: var(--ink);
      border: 1px solid rgba(19, 34, 56, 0.14);
    }

    .lang-switcher button.active {
      background: var(--accent);
      color: white;
      border-color: transparent;
    }

    @media (max-width: 900px) {
      .layout {
        grid-template-columns: 1fr;
      }
    }

    @media (max-width: 560px) {
      .watch-screen {
        padding: 18px 16px;
      }

      .watch-meta {
        grid-template-columns: 1fr;
      }

      .status-grid {
        grid-template-columns: 1fr;
      }

      .watch-time {
        font-size: clamp(34px, 10vw, 48px);
      }
    }
  </style>
</head>
<body>
  <div class="layout">
    <div class="watch-shell">
      <div class="strap top"></div>
      <div class="strap bottom"></div>
      <div class="side-button one"></div>
      <div class="side-button two"></div>
      <div class="watch-screen">
        <div class="watch-topline">
          <span id="screenName">Main</span>
          <span id="authState">Locked</span>
        </div>
        <div class="watch-main">
          <div class="watch-time" id="watchTime">--:--</div>
          <div class="watch-value" id="primaryValue">Waiting for data</div>
          <div class="watch-meta">
            <div class="watch-card">
              <div id="watchDay">Day</div>
              <div id="watchDate">Date</div>
            </div>
            <div class="watch-card">
              <div id="watchBattery">Battery</div>
              <div id="watchTemp">Temp</div>
            </div>
          </div>
        </div>
        <div class="watch-bottomline">
          <span id="watchAlarm">Alarm --:--</span>
          <span id="watchSteps">0 steps</span>
        </div>
      </div>
    </div>

    <div class="panel">
      <div class="panel-top">
        <div class="pill" id="pillLabel">Local Watch UI</div>
        <div class="lang-switcher">
          <button id="langUk" type="button" onclick="setLanguage('uk')">UA</button>
          <button id="langEn" type="button" onclick="setLanguage('en')">EN</button>
        </div>
      </div>
      <h1 id="mainTitle">Smart watch simulator interface</h1>
      <p class="subtitle" id="subtitleText">
        This UI is powered by the same C++ watch simulator state that the Android app uses.
        If you authenticate, sync time, or change the alarm from the phone app, this screen updates automatically.
      </p>

      <div class="status-grid">
        <div class="stat"><div class="stat-label" id="labelTime">Time</div><div class="stat-value" id="timeCard">--:--</div></div>
        <div class="stat"><div class="stat-label" id="labelDate">Date</div><div class="stat-value" id="dateCard">--</div></div>
        <div class="stat"><div class="stat-label" id="labelDay">Day</div><div class="stat-value" id="dayCard">--</div></div>
        <div class="stat"><div class="stat-label" id="labelBattery">Battery</div><div class="stat-value" id="batteryCard">--</div></div>
        <div class="stat"><div class="stat-label" id="labelSteps">Steps</div><div class="stat-value" id="stepsCard">--</div></div>
        <div class="stat"><div class="stat-label" id="labelTemperature">Temperature</div><div class="stat-value" id="tempCard">--</div></div>
        <div class="stat"><div class="stat-label" id="labelAlarm">Alarm</div><div class="stat-value" id="alarmCard">--</div></div>
        <div class="stat"><div class="stat-label" id="labelStatus">Status</div><div class="stat-value" id="authCard">--</div></div>
      </div>

      <div class="controls">
        <button class="ghost" id="prevButton" onclick="changeScreen('prev')">Prev Screen</button>
        <button id="nextButton" onclick="changeScreen('next')">Next Screen</button>
        <button class="alt" id="lockButton" onclick="toggleLock()">Lock / Unlock</button>
        <button class="ghost" id="refreshButton" onclick="refreshStatus()">Refresh Now</button>
      </div>

      <p class="api-note" id="apiNote">
        Open this page in a browser: <strong>http://127.0.0.1:8080/</strong><br>
        Android emulator uses the same simulator through <strong>http://10.0.2.2:8080</strong>.
      </p>
    </div>
  </div>

  <script>
    let latestStatus = null;
    let currentLanguage = localStorage.getItem('watchUiLang') || 'uk';

    const translations = {
      uk: {
        pill: 'Локальний UI годинника',
        title: 'Інтерфейс симулятора годинника',
        subtitle: 'Цей інтерфейс працює на тому ж стані C++ симулятора, що й Android-застосунок. Автентифікація, синхронізація часу та зміна будильника з телефона одразу оновлюють цей екран.',
        labels: {
          time: 'Час',
          date: 'Дата',
          day: 'День',
          battery: 'Батарея',
          steps: 'Кроки',
          temperature: 'Температура',
          alarm: 'Будильник',
          status: 'Статус'
        },
        buttons: {
          prev: 'Попередній',
          next: 'Наступний',
          lock: 'Блокувати / Розблокувати',
          refresh: 'Оновити'
        },
        words: {
          battery: 'Батарея',
          temp: 'Темп.',
          alarm: 'Будильник',
          steps: 'кроків',
          todaySteps: 'кроків сьогодні',
          locked: 'Заблоковано',
          unlocked: 'Розблоковано',
          ready: 'Годинник готовий',
          connectionLost: 'Звʼязок втрачено'
        },
        screens: {
          Main: 'Головний',
          Steps: 'Кроки',
          Battery: 'Батарея',
          Alarm: 'Будильник'
        },
        days: {
          Monday: 'Понеділок',
          Tuesday: 'Вівторок',
          Wednesday: 'Середа',
          Thursday: 'Четвер',
          Friday: 'Пʼятниця',
          Saturday: 'Субота',
          Sunday: 'Неділя'
        },
        apiNote: 'Відкрийте сторінку: <strong>http://127.0.0.1:8080/</strong><br>Android емулятор використовує той самий симулятор через <strong>http://10.0.2.2:8080</strong>.'
      },
      en: {
        pill: 'Local Watch UI',
        title: 'Smart watch simulator interface',
        subtitle: 'This UI is powered by the same C++ watch simulator state that the Android app uses. If you authenticate, sync time, or change the alarm from the phone app, this screen updates automatically.',
        labels: {
          time: 'Time',
          date: 'Date',
          day: 'Day',
          battery: 'Battery',
          steps: 'Steps',
          temperature: 'Temperature',
          alarm: 'Alarm',
          status: 'Status'
        },
        buttons: {
          prev: 'Prev Screen',
          next: 'Next Screen',
          lock: 'Lock / Unlock',
          refresh: 'Refresh Now'
        },
        words: {
          battery: 'Battery',
          temp: 'Temp',
          alarm: 'Alarm',
          steps: 'steps',
          todaySteps: 'steps today',
          locked: 'Locked',
          unlocked: 'Unlocked',
          ready: 'Watch ready',
          connectionLost: 'Connection lost'
        },
        screens: {
          Main: 'Main',
          Steps: 'Steps',
          Battery: 'Battery',
          Alarm: 'Alarm'
        },
        days: {
          Monday: 'Monday',
          Tuesday: 'Tuesday',
          Wednesday: 'Wednesday',
          Thursday: 'Thursday',
          Friday: 'Friday',
          Saturday: 'Saturday',
          Sunday: 'Sunday'
        },
        apiNote: 'Open this page in a browser: <strong>http://127.0.0.1:8080/</strong><br>Android emulator uses the same simulator through <strong>http://10.0.2.2:8080</strong>.'
      }
    };

    function t() {
      return translations[currentLanguage] || translations.en;
    }

    function translateDay(day) {
      return t().days[day] || day;
    }

    function translateScreen(screen) {
      return t().screens[screen] || screen;
    }

    function applyStaticTranslations() {
      const tr = t();
      document.getElementById('pillLabel').textContent = tr.pill;
      document.getElementById('mainTitle').textContent = tr.title;
      document.getElementById('subtitleText').textContent = tr.subtitle;
      document.getElementById('labelTime').textContent = tr.labels.time;
      document.getElementById('labelDate').textContent = tr.labels.date;
      document.getElementById('labelDay').textContent = tr.labels.day;
      document.getElementById('labelBattery').textContent = tr.labels.battery;
      document.getElementById('labelSteps').textContent = tr.labels.steps;
      document.getElementById('labelTemperature').textContent = tr.labels.temperature;
      document.getElementById('labelAlarm').textContent = tr.labels.alarm;
      document.getElementById('labelStatus').textContent = tr.labels.status;
      document.getElementById('prevButton').textContent = tr.buttons.prev;
      document.getElementById('nextButton').textContent = tr.buttons.next;
      document.getElementById('lockButton').textContent = tr.buttons.lock;
      document.getElementById('refreshButton').textContent = tr.buttons.refresh;
      document.getElementById('apiNote').innerHTML = tr.apiNote;
      document.getElementById('langUk').classList.toggle('active', currentLanguage === 'uk');
      document.getElementById('langEn').classList.toggle('active', currentLanguage === 'en');
    }

    function setLanguage(language) {
      currentLanguage = language;
      localStorage.setItem('watchUiLang', language);
      applyStaticTranslations();
      if (latestStatus) {
        render(latestStatus);
      }
    }

    async function postJson(path, payload) {
      const response = await fetch(path, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      if (!response.ok) {
        const text = await response.text();
        throw new Error(text || 'Request failed');
      }
      return response.json().catch(() => ({}));
    }

    function render(status) {
      const tr = t();
      latestStatus = status;
      document.getElementById('screenName').textContent = translateScreen(status.screen);
      document.getElementById('authState').textContent = status.authenticated ? tr.words.unlocked : tr.words.locked;
      document.getElementById('watchTime').textContent = status.time;
      document.getElementById('watchDay').textContent = translateDay(status.day);
      document.getElementById('watchDate').textContent = status.date;
      document.getElementById('watchBattery').textContent = `${tr.words.battery} ${status.battery}%`;
      document.getElementById('watchTemp').textContent = `${tr.words.temp} ${status.temperature} C`;
      document.getElementById('watchAlarm').textContent = `${tr.words.alarm} ${status.alarm}`;
      document.getElementById('watchSteps').textContent = `${status.steps} ${tr.words.steps}`;

      document.getElementById('timeCard').textContent = status.time;
      document.getElementById('dateCard').textContent = status.date;
      document.getElementById('dayCard').textContent = translateDay(status.day);
      document.getElementById('batteryCard').textContent = `${status.battery}%`;
      document.getElementById('stepsCard').textContent = status.steps;
      document.getElementById('tempCard').textContent = `${status.temperature} C`;
      document.getElementById('alarmCard').textContent = status.alarm;
      document.getElementById('authCard').textContent = status.authenticated ? tr.words.unlocked : tr.words.locked;

      const screenLabels = {
        Main: `${tr.labels.steps} ${status.steps} | ${tr.words.temp} ${status.temperature} C`,
        Steps: `${status.steps} ${tr.words.todaySteps}`,
        Battery: `${tr.words.battery} ${status.battery}%`,
        Alarm: `${tr.words.alarm} ${status.alarm}`
      };
      document.getElementById('primaryValue').textContent = screenLabels[status.screen] || tr.words.ready;
    }

    async function refreshStatus() {
      try {
        const response = await fetch('/status');
        const status = await response.json();
        render(status);
      } catch (error) {
        document.getElementById('primaryValue').textContent = t().words.connectionLost;
      }
    }

    async function changeScreen(action) {
      await postJson('/screen', { action });
      await refreshStatus();
    }

    async function toggleLock() {
      const shouldLock = !latestStatus || latestStatus.authenticated;
      await postJson('/lock', { locked: shouldLock ? 'true' : 'false' });
      await refreshStatus();
    }

    applyStaticTranslations();
    refreshStatus();
    setInterval(refreshStatus, 1500);
  </script>
</body>
</html>
)HTML";
}

bool isValidTimeString(const std::string& value) {
    if (value.size() != 5 || value[2] != ':') {
        return false;
    }

    if (!std::isdigit(value[0]) || !std::isdigit(value[1]) ||
        !std::isdigit(value[3]) || !std::isdigit(value[4])) {
        return false;
    }

    const int hours = std::stoi(value.substr(0, 2));
    const int minutes = std::stoi(value.substr(3, 2));
    return hours >= 0 && hours <= 23 && minutes >= 0 && minutes <= 59;
}

enum class WatchScreen {
    Main,
    Steps,
    Battery,
    Alarm
};

class WatchState {
  public:
    WatchState()
        : timeOffsetMinutes_(0),
          batteryLevel_(100),
          stepsCount_(1200),
          temperatureCelsius_(23),
          alarmTime_("07:30"),
          authenticated_(false),
          currentScreen_(WatchScreen::Main),
          lastBatteryDrain_(std::chrono::steady_clock::now()),
          lastStepIncrement_(std::chrono::steady_clock::now()),
          lastTemperatureShift_(std::chrono::steady_clock::now()) {}

    std::string statusJson() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream stream;
        stream << "{"
               << "\"time\":\"" << currentTimeLocked() << "\","
               << "\"day\":\"" << currentDayLocked() << "\","
               << "\"date\":\"" << currentDateLocked() << "\","
               << "\"screen\":\"" << screenNameLocked() << "\","
               << "\"battery\":" << batteryLevel_ << ","
               << "\"steps\":" << stepsCount_ << ","
               << "\"temperature\":" << temperatureCelsius_ << ","
               << "\"alarm\":\"" << alarmTime_ << "\","
               << "\"authenticated\":" << (authenticated_ ? "true" : "false")
               << "}";
        return stream.str();
    }

    bool authenticate(const std::string& pin) {
        std::lock_guard<std::mutex> lock(mutex_);
        authenticated_ = pin == kValidPin;
        return authenticated_;
    }

    bool isAuthenticated() {
        std::lock_guard<std::mutex> lock(mutex_);
        return authenticated_;
    }

    bool syncTime(const std::string& hhmm) {
        if (!isValidTimeString(hhmm)) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!authenticated_) {
            return false;
        }

        const auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm localTm = *std::localtime(&nowTime);
        const int targetMinutes = std::stoi(hhmm.substr(0, 2)) * 60 + std::stoi(hhmm.substr(3, 2));
        const int currentMinutes = localTm.tm_hour * 60 + localTm.tm_min;
        timeOffsetMinutes_ = targetMinutes - currentMinutes;
        return true;
    }

    bool setAlarm(const std::string& hhmm) {
        if (!isValidTimeString(hhmm)) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!authenticated_) {
            return false;
        }

        alarmTime_ = hhmm;
        return true;
    }

    bool setLocked(bool locked) {
        std::lock_guard<std::mutex> lock(mutex_);
        authenticated_ = !locked;
        return authenticated_;
    }

    void nextScreen() {
        std::lock_guard<std::mutex> lock(mutex_);
        switch (currentScreen_) {
            case WatchScreen::Main:
                currentScreen_ = WatchScreen::Steps;
                break;
            case WatchScreen::Steps:
                currentScreen_ = WatchScreen::Battery;
                break;
            case WatchScreen::Battery:
                currentScreen_ = WatchScreen::Alarm;
                break;
            case WatchScreen::Alarm:
                currentScreen_ = WatchScreen::Main;
                break;
        }
    }

    void previousScreen() {
        std::lock_guard<std::mutex> lock(mutex_);
        switch (currentScreen_) {
            case WatchScreen::Main:
                currentScreen_ = WatchScreen::Alarm;
                break;
            case WatchScreen::Steps:
                currentScreen_ = WatchScreen::Main;
                break;
            case WatchScreen::Battery:
                currentScreen_ = WatchScreen::Steps;
                break;
            case WatchScreen::Alarm:
                currentScreen_ = WatchScreen::Battery;
                break;
        }
    }

    void tickSimulation() {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto now = std::chrono::steady_clock::now();

        if (now - lastStepIncrement_ >= std::chrono::seconds(5)) {
            stepsCount_ += randomInt(12, 33);
            lastStepIncrement_ = now;
        }

        if (now - lastBatteryDrain_ >= std::chrono::seconds(20)) {
            batteryLevel_ = std::max(5, batteryLevel_ - 1);
            lastBatteryDrain_ = now;
        }

        if (now - lastTemperatureShift_ >= std::chrono::seconds(15)) {
            temperatureCelsius_ = std::clamp(temperatureCelsius_ + randomInt(-1, 1), 18, 30);
            lastTemperatureShift_ = now;
        }
    }

    std::string renderScreen() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream stream;
        stream << "\033[2J\033[H";
        stream << "+----------------------+\n";
        stream << "|   ESP32 WATCH SIM    |\n";
        stream << "+----------------------+\n";
        stream << "| Screen: " << std::left << std::setw(12) << screenNameLocked() << "|\n";
        stream << "| Time:   " << std::left << std::setw(12) << currentTimeLocked() << "|\n";
        stream << "| Date:   " << std::left << std::setw(12) << currentDateLocked() << "|\n";
        stream << "| Day:    " << std::left << std::setw(12) << currentDayLocked() << "|\n";
        stream << "| Auth:   " << std::left << std::setw(12) << (authenticated_ ? "Unlocked" : "Locked") << "|\n";

        switch (currentScreen_) {
            case WatchScreen::Main:
                stream << "| Steps:  " << std::left << std::setw(12) << stepsCount_ << "|\n";
                stream << "| Temp:   " << std::left << std::setw(12) << (std::to_string(temperatureCelsius_) + " C") << "|\n";
                break;
            case WatchScreen::Steps:
                stream << "| Today's steps        |\n";
                stream << "| " << std::left << std::setw(20) << stepsCount_ << "|\n";
                break;
            case WatchScreen::Battery:
                stream << "| Battery level        |\n";
                stream << "| " << std::left << std::setw(20) << (std::to_string(batteryLevel_) + "%") << "|\n";
                break;
            case WatchScreen::Alarm:
                stream << "| Alarm time           |\n";
                stream << "| " << std::left << std::setw(20) << alarmTime_ << "|\n";
                break;
        }

        stream << "+----------------------+\n";
        stream << "HTTP API: http://127.0.0.1:" << kServerPort << "\n";
        stream << "Keys: [n] next  [p] prev  [l] lock/unlock  [q] quit\n";
        return stream.str();
    }

  private:
    int randomInt(int min, int max) {
        static std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<int> distribution(min, max);
        return distribution(generator);
    }

    std::string currentTimeLocked() const {
        const auto adjusted = std::chrono::system_clock::now() + std::chrono::minutes(timeOffsetMinutes_);
        std::time_t adjustedTime = std::chrono::system_clock::to_time_t(adjusted);
        std::tm localTm = *std::localtime(&adjustedTime);
        std::ostringstream stream;
        stream << std::put_time(&localTm, "%H:%M");
        return stream.str();
    }

    std::string currentDayLocked() const {
        const auto adjusted = std::chrono::system_clock::now() + std::chrono::minutes(timeOffsetMinutes_);
        std::time_t adjustedTime = std::chrono::system_clock::to_time_t(adjusted);
        std::tm localTm = *std::localtime(&adjustedTime);
        std::ostringstream stream;
        stream << std::put_time(&localTm, "%A");
        return stream.str();
    }

    std::string currentDateLocked() const {
        const auto adjusted = std::chrono::system_clock::now() + std::chrono::minutes(timeOffsetMinutes_);
        std::time_t adjustedTime = std::chrono::system_clock::to_time_t(adjusted);
        std::tm localTm = *std::localtime(&adjustedTime);
        std::ostringstream stream;
        stream << std::put_time(&localTm, "%d.%m.%Y");
        return stream.str();
    }

    std::string screenNameLocked() const {
        switch (currentScreen_) {
            case WatchScreen::Main:
                return "Main";
            case WatchScreen::Steps:
                return "Steps";
            case WatchScreen::Battery:
                return "Battery";
            case WatchScreen::Alarm:
                return "Alarm";
        }
        return "Unknown";
    }

    std::mutex mutex_;
    int timeOffsetMinutes_;
    int batteryLevel_;
    int stepsCount_;
    int temperatureCelsius_;
    std::string alarmTime_;
    bool authenticated_;
    WatchScreen currentScreen_;
    std::chrono::steady_clock::time_point lastBatteryDrain_;
    std::chrono::steady_clock::time_point lastStepIncrement_;
    std::chrono::steady_clock::time_point lastTemperatureShift_;
};

std::string makeHttpResponse(int statusCode,
                             const std::string& statusText,
                             const std::string& body,
                             const std::string& contentType = kJsonContentType) {
    std::ostringstream stream;
    stream << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    stream << "Content-Type: " << contentType << "\r\n";
    stream << "Access-Control-Allow-Origin: *\r\n";
    stream << "Content-Length: " << body.size() << "\r\n";
    stream << "Connection: close\r\n\r\n";
    stream << body;
    return stream.str();
}

std::string handleRequest(WatchState& watch, const std::string& request) {
    const auto headersEnd = request.find("\r\n\r\n");
    const std::string head = headersEnd == std::string::npos ? request : request.substr(0, headersEnd);
    const std::string body = headersEnd == std::string::npos ? "" : request.substr(headersEnd + 4);

    std::istringstream requestLineStream(head);
    std::string method;
    std::string path;
    std::string version;
    requestLineStream >> method >> path >> version;
    const std::string rawPath = path;
    path = normalizePath(path);
    std::cerr << "[watch_sim] " << method << " " << rawPath << " -> " << path << "\n";

    if (method == "GET" && path == "/") {
        return makeHttpResponse(200, "OK", watchUiHtml(), kHtmlContentType);
    }

    if (method == "GET" && path == "/status") {
        return makeHttpResponse(200, "OK", watch.statusJson());
    }

    if (method == "POST" && path == "/auth") {
        const auto pin = extractJsonString(body, "pin");
        if (!pin.has_value()) {
            return makeHttpResponse(400, "Bad Request", "{\"error\":\"Missing pin\"}");
        }

        const bool success = watch.authenticate(pin.value());
        return makeHttpResponse(success ? 200 : 401,
                                success ? "OK" : "Unauthorized",
                                std::string("{\"success\":") + (success ? "true" : "false") +
                                    ",\"authenticated\":" + (success ? "true" : "false") +
                                    (success ? "" : ",\"error\":\"Invalid PIN\"") + "}");
    }

    if (method == "POST" && path == "/sync-time") {
        const auto time = extractJsonString(body, "time");
        if (!time.has_value()) {
            return makeHttpResponse(400, "Bad Request", "{\"error\":\"Missing time\"}");
        }

        if (!watch.isAuthenticated()) {
            return makeHttpResponse(403, "Forbidden", "{\"error\":\"Authenticate first\"}");
        }

        if (!watch.syncTime(time.value())) {
            return makeHttpResponse(400, "Bad Request", "{\"error\":\"Invalid time\"}");
        }

        return makeHttpResponse(200, "OK", std::string("{\"success\":true,\"time\":\"") + time.value() + "\"}");
    }

    if (method == "POST" && path == "/alarm") {
        const auto time = extractJsonString(body, "time");
        if (!time.has_value()) {
            return makeHttpResponse(400, "Bad Request", "{\"error\":\"Missing time\"}");
        }

        if (!watch.isAuthenticated()) {
            return makeHttpResponse(403, "Forbidden", "{\"error\":\"Authenticate first\"}");
        }

        if (!watch.setAlarm(time.value())) {
            return makeHttpResponse(400, "Bad Request", "{\"error\":\"Invalid time\"}");
        }

        return makeHttpResponse(200, "OK", std::string("{\"success\":true,\"alarm\":\"") + time.value() + "\"}");
    }

    if (method == "POST" && path == "/lock") {
        const auto lockedValue = extractJsonString(body, "locked");
        const bool locked = !lockedValue.has_value() || lockedValue.value() != "false";
        const bool authenticated = watch.setLocked(locked);
        return makeHttpResponse(200, "OK",
                                std::string("{\"authenticated\":") + (authenticated ? "true" : "false") + "}");
    }

    if (method == "POST" && path == "/screen") {
        const auto action = extractJsonString(body, "action");
        if (!action.has_value()) {
            return makeHttpResponse(400, "Bad Request", "{\"error\":\"Missing action\"}");
        }

        if (action.value() == "next") {
            watch.nextScreen();
        } else if (action.value() == "prev") {
            watch.previousScreen();
        } else {
            return makeHttpResponse(400, "Bad Request", "{\"error\":\"Invalid action\"}");
        }

        return makeHttpResponse(200, "OK", watch.statusJson());
    }

    return makeHttpResponse(404, "Not Found", "{\"error\":\"Route not found\"}");
}

void serveHttp(WatchState& watch) {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        std::cerr << "Failed to create socket.\n";
        g_running = false;
        return;
    }

    const int reuse = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(kServerPort);

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port " << kServerPort << ".\n";
        close(serverFd);
        g_running = false;
        return;
    }

    if (listen(serverFd, 10) < 0) {
        std::cerr << "Failed to listen for connections.\n";
        close(serverFd);
        g_running = false;
        return;
    }

    while (g_running) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);
        const int clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
        if (clientFd < 0) {
            if (g_running) {
                std::cerr << "Accept failed.\n";
            }
            continue;
        }

        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        std::string request;
        char buffer[4096];
        const auto requestStart = std::chrono::steady_clock::now();
        while (true) {
            const ssize_t received = recv(clientFd, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                if (received < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                    const auto headersEnd = request.find("\r\n\r\n");
                    if (headersEnd != std::string::npos) {
                        break;
                    }
                    if (std::chrono::steady_clock::now() - requestStart < std::chrono::seconds(5)) {
                        continue;
                    }
                }
                break;
            }
            request.append(buffer, static_cast<size_t>(received));

            const auto headersEnd = request.find("\r\n\r\n");
            if (headersEnd != std::string::npos) {
                const auto requestHead = request.substr(0, headersEnd);
                const auto expectedBodyLength = parseContentLength(requestHead).value_or(0);
                const size_t currentBodyLength = request.size() - headersEnd - 4;
                if (currentBodyLength >= expectedBodyLength) {
                    break;
                }
            }
        }

        const std::string response = handleRequest(watch, request);
        send(clientFd, response.c_str(), response.size(), 0);
        close(clientFd);
    }

    close(serverFd);
}

void keyboardLoop(WatchState& watch) {
    while (g_running) {
        char key = 0;
        if (!std::cin.get(key)) {
            g_running = false;
            break;
        }

        if (key == 'n') {
            watch.nextScreen();
        } else if (key == 'p') {
            watch.previousScreen();
        } else if (key == 'l') {
            watch.setLocked(watch.isAuthenticated());
        } else if (key == 'q') {
            g_running = false;
        }
    }
}

void signalHandler(int) {
    g_running = false;
}

}  // namespace

int main() {
    std::signal(SIGINT, signalHandler);

    WatchState watch;

    std::thread serverThread(serveHttp, std::ref(watch));
    std::thread inputThread(keyboardLoop, std::ref(watch));

    while (g_running) {
        watch.tickSimulation();
        std::cout << watch.renderScreen() << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (serverThread.joinable()) {
        serverThread.detach();
    }

    if (inputThread.joinable()) {
        inputThread.detach();
    }

    std::cout << "\nWatch simulator stopped.\n";
    return 0;
}

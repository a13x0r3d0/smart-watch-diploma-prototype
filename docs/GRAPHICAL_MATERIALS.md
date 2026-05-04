# Графічні матеріали до дипломного проєкту

Цей документ містить набір схем для дипломного проєкту "Інтелектуальна система смартгодинника з автентифікацією користувача через мобільний застосунок".

Схеми побудовані за фактичною архітектурою проєкту:

- `watch_sim/` - локальний симулятор смартгодинника;
- `android_app/` - мобільний застосунок Android;
- `watch_firmware_arduino/` - ESP32/Arduino firmware-версія;
- HTTP API - локальний канал взаємодії між застосунком і годинником;
- browser UI - візуальна заміна OLED-дисплея для демонстрації без фізичного пристрою.

## 1. Структурна схема системи

Структурна схема показує основні частини системи та зв'язки між ними.

```mermaid
flowchart LR
    User["Користувач"]

    subgraph Mobile["Мобільний застосунок Android"]
        Login["Екран автентифікації"]
        Dashboard["Головна панель керування"]
        ApiClient["WatchApiClient"]
        Locale["UA / EN локалізація"]
    end

    subgraph WatchSim["Локальний симулятор смартгодинника watch_sim"]
        HttpServer["HTTP сервер :8080"]
        WatchState["Стан годинника"]
        SensorMock["Симуляція сенсорів"]
        BrowserUI["Web UI годинника"]
        TerminalUI["Terminal OLED UI"]
    end

    subgraph Firmware["ESP32 / Arduino firmware prototype"]
        SetupLoop["setup() / loop()"]
        WifiServer["Wi-Fi + WebServer"]
        FirmwareState["WatchState firmware"]
        MockSensors["Mock sensor layer"]
    end

    User --> Login
    Login --> Dashboard
    Dashboard --> ApiClient
    ApiClient -->|"HTTP: 10.0.2.2:8080"| HttpServer

    HttpServer --> WatchState
    WatchState --> BrowserUI
    WatchState --> TerminalUI
    SensorMock --> WatchState

    Firmware -. "показує готовність до hardware" .-> WatchSim
    SetupLoop --> WifiServer
    WifiServer --> FirmwareState
    MockSensors --> FirmwareState
    Locale --> Login
    Locale --> Dashboard
```

Коротке пояснення:

- Користувач взаємодіє з Android-застосунком.
- Android-застосунок відправляє HTTP-запити до локального симулятора годинника.
- `watch_sim` зберігає стан пристрою, обробляє API та показує web UI годинника.
- Arduino firmware є окремою ESP32-версією логіки для демонстрації готовності до перенесення на фізичний пристрій.

## 2. Функціональна схема системи

Функціональна схема показує, які функції виконує кожен компонент системи.

```mermaid
flowchart TB
    System["Система смартгодинника з мобільною автентифікацією"]

    System --> Auth["Автентифікація користувача"]
    System --> Status["Моніторинг стану годинника"]
    System --> Control["Керування годинником"]
    System --> Display["Відображення даних"]
    System --> Simulation["Симуляція фізичних даних"]
    System --> Localization["Локалізація інтерфейсу"]

    Auth --> PinInput["Введення PIN у мобільному застосунку"]
    Auth --> PinCheck["Перевірка PIN на стороні годинника"]
    Auth --> LockState["Стан: заблоковано / розблоковано"]

    Status --> TimeStatus["Поточний час"]
    Status --> DateStatus["Дата і день тижня"]
    Status --> BatteryStatus["Рівень батареї"]
    Status --> StepsStatus["Кількість кроків"]
    Status --> TemperatureStatus["Температура"]
    Status --> AlarmStatus["Час будильника"]

    Control --> SyncTime["Синхронізація часу"]
    Control --> SetAlarm["Встановлення будильника"]
    Control --> LockWatch["Блокування годинника"]
    Control --> ChangeScreen["Перемикання екранів годинника"]

    Display --> MobileDashboard["Dashboard в Android app"]
    Display --> BrowserWatch["Browser UI годинника"]
    Display --> SerialTerminal["Terminal / Serial Monitor UI"]

    Simulation --> StepMock["Mock-генерація кроків"]
    Simulation --> BatteryMock["Mock-розряд батареї"]
    Simulation --> TempMock["Mock-зміна температури"]

    Localization --> Ukrainian["Українська мова"]
    Localization --> English["Англійська мова"]
```

Коротке пояснення:

- Система виконує не одну дію, а набір пов'язаних функцій: автентифікацію, моніторинг, керування, відображення та симуляцію даних.
- Основні дії керування доступні лише після успішної автентифікації.
- Симуляція сенсорів замінює фізичні датчики на етапі програмного прототипу.

## 3. Принципова схема пристрою

Принципова схема показує, як виглядала б апаратна частина смартгодинника при перенесенні прототипу на реальний ESP32-пристрій.

```mermaid
flowchart LR
    Battery["Акумулятор"]
    Power["Модуль живлення / стабілізатор"]
    ESP32["ESP32 мікроконтролер"]
    Display["OLED / TFT дисплей"]
    Buttons["Кнопки керування"]
    Accelerometer["Акселерометр"]
    TempSensor["Датчик температури"]
    BatteryGauge["ADC / Fuel Gauge батареї"]
    WiFi["Wi-Fi модуль ESP32"]
    Mobile["Мобільний застосунок"]

    Battery --> Power
    Power --> ESP32

    Buttons -->|"GPIO"| ESP32
    Accelerometer -->|"I2C / SPI"| ESP32
    TempSensor -->|"I2C / ADC"| ESP32
    BatteryGauge -->|"ADC / I2C"| ESP32
    ESP32 -->|"I2C / SPI"| Display

    ESP32 --> WiFi
    WiFi <-->|"HTTP API"| Mobile

    subgraph FirmwareLogic["Логіка firmware"]
        State["Стан годинника"]
        Auth["PIN-автентифікація"]
        Sensors["Обробка сенсорів"]
        Screens["Екрани годинника"]
        Api["HTTP endpoints"]
    end

    ESP32 --> FirmwareLogic
```

Коротке пояснення:

- У поточному дипломному прототипі реальні сенсори замінені програмною симуляцією.
- У фізичній версії ESP32 отримував би дані від акселерометра, температурного датчика та модуля контролю батареї.
- OLED/TFT-дисплей замінився browser UI у локальній демонстрації.

## 4. Діаграма процесів системи

Діаграма процесів показує типовий сценарій взаємодії користувача, Android-застосунку та годинника.

```mermaid
sequenceDiagram
    actor User as Користувач
    participant App as Android app
    participant ApiClient as WatchApiClient
    participant Watch as watch_sim HTTP API
    participant State as WatchState
    participant UI as Browser Watch UI

    User->>App: Вводить PIN 1234
    App->>ApiClient: authenticate(pin)
    ApiClient->>Watch: POST /auth
    Watch->>State: Перевірити PIN
    State-->>Watch: authenticated = true
    Watch-->>ApiClient: success = true
    ApiClient-->>App: Auth OK
    App-->>User: Відкрити Dashboard

    User->>App: Натискає Fetch Watch Status
    App->>ApiClient: getStatus()
    ApiClient->>Watch: GET /status
    Watch->>State: Отримати поточний стан
    State-->>Watch: JSON стану годинника
    Watch-->>ApiClient: status JSON
    ApiClient-->>App: WatchStatus
    App-->>User: Показати час, дату, батарею, кроки, температуру
    UI->>Watch: Періодичний GET /status
    Watch-->>UI: Оновлений стан годинника

    User->>App: Натискає Sync Time
    App->>ApiClient: syncTime(currentTime)
    ApiClient->>Watch: POST /sync-time
    Watch->>State: Оновити час
    State-->>Watch: Час синхронізовано
    Watch-->>App: success

    User->>App: Встановлює будильник
    App->>ApiClient: setAlarm(time)
    ApiClient->>Watch: POST /alarm
    Watch->>State: Зберегти alarmTime
    State-->>Watch: Будильник оновлено
    Watch-->>App: success
```

Коротке пояснення:

- Центральний процес починається з PIN-автентифікації.
- Після автентифікації мобільний застосунок отримує право змінювати час і будильник.
- Browser UI не є окремим backend-ом, він лише відображає той самий стан, що й Android-застосунок.

## 5. Блок-схема алгоритму роботи основної програми

Ця блок-схема описує загальний алгоритм роботи `watch_sim`.

```mermaid
flowchart TD
    Start([Старт програми])
    InitState["Ініціалізація WatchState"]
    StartServer["Запуск HTTP-сервера на порті 8080"]
    StartKeyboard["Запуск потоку обробки клавіш"]
    MainLoop{"g_running = true?"}
    Tick["Оновити симуляцію: кроки, батарея, температура"]
    Render["Перемалювати terminal OLED UI"]
    Wait["Затримка 1 секунда"]
    Stop["Завершити потоки та програму"]
    End([Кінець])

    Start --> InitState
    InitState --> StartServer
    StartServer --> StartKeyboard
    StartKeyboard --> MainLoop
    MainLoop -- "Так" --> Tick
    Tick --> Render
    Render --> Wait
    Wait --> MainLoop
    MainLoop -- "Ні" --> Stop
    Stop --> End

    subgraph ServerThread["HTTP server thread"]
        Accept["Очікувати HTTP-запит"]
        ReadRequest["Прочитати request"]
        Route["Визначити endpoint"]
        Response["Сформувати HTTP response"]
        Send["Відправити response"]
        Accept --> ReadRequest --> Route --> Response --> Send --> Accept
    end

    subgraph KeyboardThread["Keyboard thread"]
        ReadKey["Зчитати клавішу"]
        KeyAction{"Яка клавіша?"}
        NextScreen["n: наступний екран"]
        PrevScreen["p: попередній екран"]
        LockToggle["l: lock / unlock"]
        Quit["q: завершити програму"]
        ReadKey --> KeyAction
        KeyAction --> NextScreen
        KeyAction --> PrevScreen
        KeyAction --> LockToggle
        KeyAction --> Quit
    end
```

Коротке пояснення:

- Основна програма працює циклічно.
- Окремий потік обробляє HTTP-запити.
- Окремий потік обробляє клавіатурне керування.
- Основний цикл відповідає за симуляцію стану та вивід terminal UI.

## 6. Блок-схеми алгоритмів роботи функцій

У цьому розділі наведено дві функціональні блок-схеми: автентифікація та встановлення будильника.

### 6.1 Блок-схема алгоритму автентифікації користувача

```mermaid
flowchart TD
    Start([Початок])
    Input["Користувач вводить PIN у Android app"]
    ValidateLength{"PIN має 4 символи?"}
    ShowInvalid["Показати помилку введення"]
    SendAuth["Надіслати POST /auth"]
    ServerRead["watch_sim читає поле pin"]
    Compare{"PIN == 1234?"}
    SetAuthTrue["authenticated = true"]
    SetAuthFalse["authenticated = false"]
    OpenDashboard["Відкрити Dashboard"]
    ShowError["Показати повідомлення про неправильний PIN"]
    End([Кінець])

    Start --> Input
    Input --> ValidateLength
    ValidateLength -- "Ні" --> ShowInvalid --> End
    ValidateLength -- "Так" --> SendAuth
    SendAuth --> ServerRead
    ServerRead --> Compare
    Compare -- "Так" --> SetAuthTrue
    Compare -- "Ні" --> SetAuthFalse
    SetAuthTrue --> OpenDashboard --> End
    SetAuthFalse --> ShowError --> End
```

Пояснення:

- Android-застосунок перевіряє тільки базову коректність введення.
- Остаточне рішення про автентифікацію приймає сторона годинника.
- Якщо PIN правильний, відкривається головний екран застосунку.

### 6.2 Блок-схема алгоритму встановлення будильника

```mermaid
flowchart TD
    Start([Початок])
    InputAlarm["Користувач вводить час будильника"]
    ValidateFormat{"Формат HH:mm?"}
    ShowFormatError["Показати помилку формату"]
    SendAlarm["Надіслати POST /alarm"]
    CheckAuth{"Годинник автентифікований?"}
    Reject["Повернути 403 Authenticate first"]
    ValidateServerTime{"Час валідний на сервері?"}
    BadRequest["Повернути 400 Invalid time"]
    SaveAlarm["Зберегти alarmTime"]
    Success["Повернути success = true"]
    RefreshStatus["Оновити dashboard і web UI"]
    End([Кінець])

    Start --> InputAlarm
    InputAlarm --> ValidateFormat
    ValidateFormat -- "Ні" --> ShowFormatError --> End
    ValidateFormat -- "Так" --> SendAlarm
    SendAlarm --> CheckAuth
    CheckAuth -- "Ні" --> Reject --> End
    CheckAuth -- "Так" --> ValidateServerTime
    ValidateServerTime -- "Ні" --> BadRequest --> End
    ValidateServerTime -- "Так" --> SaveAlarm
    SaveAlarm --> Success
    Success --> RefreshStatus
    RefreshStatus --> End
```

Пояснення:

- Встановлення будильника є захищеною дією.
- Якщо користувач не автентифікований, сервер не дозволяє змінювати `alarmTime`.
- Після успіху Android dashboard і browser UI показують оновлений стан.

## Як використовувати ці схеми

Ці схеми можна:

- залишити в Markdown-документації;
- відкрити на GitHub, де Mermaid-діаграми будуть відрендерені автоматично;
- експортувати у зображення або PDF;
- перенести в пояснювальну записку як графічні матеріали.

Для захисту найкраще використовувати:

- структурну схему для пояснення архітектури;
- функціональну схему для пояснення можливостей системи;
- принципову схему для пояснення майбутнього hardware-пристрою;
- діаграму процесів для демонстрації сценарію роботи;
- блок-схеми алгоритмів для пояснення програмної логіки.

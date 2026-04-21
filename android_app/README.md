# Android App

`android_app` is a simple Kotlin Android application that runs in the Android Studio emulator and communicates with the local watch simulator over HTTP.

## What It Does

- Login screen with PIN authentication
- Dashboard screen with watch data:
  - time
  - day of week
  - date
  - battery
  - steps
  - temperature
  - alarm
  - authentication status
- Built-in `UA / EN` language switcher on both screens
- Buttons to:
  - authenticate with PIN
  - fetch watch status
  - sync time
  - set alarm
  - lock the watch again
- Optional local notification when the current watch time matches the alarm

## Important Networking Detail

The app talks to the watch simulator through:

```text
http://10.0.2.2:8080
```

`10.0.2.2` is the Android emulator alias for the host machine’s localhost.

## Open In Android Studio

1. Open the `android_app` folder in Android Studio.
2. Let Gradle sync the project.
3. Run the app on an Android emulator.

If Android Studio asks for missing SDKs or a JDK, install them from the IDE prompts.

## Demo Flow

1. Start the watch simulator first.
2. Launch the Android app in the emulator.
3. Enter PIN `1234`.
4. On the dashboard:
   - tap `Fetch Watch Status`
   - tap `Sync Time`
   - enter an alarm like `07:30`
   - tap `Set Alarm`
   - tap `Lock Watch` to demonstrate protected actions

## Main Files

- [LoginActivity.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/LoginActivity.kt)
- [DashboardActivity.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/DashboardActivity.kt)
- [WatchApiClient.kt](/Users/a13x0r3d/Documents/New%20project/android_app/app/src/main/java/com/example/smartwatchdemo/WatchApiClient.kt)

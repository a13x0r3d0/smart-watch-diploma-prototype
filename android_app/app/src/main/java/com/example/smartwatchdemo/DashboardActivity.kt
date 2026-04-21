package com.example.smartwatchdemo

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.content.pm.PackageManager
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import java.time.LocalTime
import java.time.format.DateTimeFormatter
import java.util.concurrent.Executors

class DashboardActivity : AppCompatActivity() {
    private val executor = Executors.newSingleThreadExecutor()
    private lateinit var timeValue: TextView
    private lateinit var dayValue: TextView
    private lateinit var dateValue: TextView
    private lateinit var batteryValue: TextView
    private lateinit var stepsValue: TextView
    private lateinit var temperatureValue: TextView
    private lateinit var alarmValue: TextView
    private lateinit var authValue: TextView
    private lateinit var statusText: TextView
    private lateinit var alarmInput: EditText
    private lateinit var progressBar: ProgressBar
    private var lastAlarmNotified: String? = null

    override fun attachBaseContext(newBase: Context) {
        super.attachBaseContext(LocaleHelper.wrap(newBase))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_dashboard)
        createNotificationChannel()

        val uaButton = findViewById<Button>(R.id.uaButton)
        val enButton = findViewById<Button>(R.id.enButton)
        timeValue = findViewById(R.id.timeValue)
        dayValue = findViewById(R.id.dayValue)
        dateValue = findViewById(R.id.dateValue)
        batteryValue = findViewById(R.id.batteryValue)
        stepsValue = findViewById(R.id.stepsValue)
        temperatureValue = findViewById(R.id.temperatureValue)
        alarmValue = findViewById(R.id.alarmValue)
        authValue = findViewById(R.id.authValue)
        statusText = findViewById(R.id.dashboardStatusText)
        alarmInput = findViewById(R.id.alarmInput)
        progressBar = findViewById(R.id.dashboardProgress)

        setupLanguageSwitcher(uaButton, enButton)

        findViewById<Button>(R.id.refreshButton).setOnClickListener {
            refreshStatus()
        }

        findViewById<Button>(R.id.syncTimeButton).setOnClickListener {
            val now = LocalTime.now().format(DateTimeFormatter.ofPattern("HH:mm"))
            runRequest(
                loadingMessage = getString(R.string.syncing_time),
                action = { WatchApiClient.syncTime(now).map { now } },
                onSuccess = { synced ->
                    statusText.text = getString(R.string.time_synced, synced)
                    refreshStatus()
                }
            )
        }

        findViewById<Button>(R.id.setAlarmButton).setOnClickListener {
            val alarm = alarmInput.text.toString().trim()
            if (!alarm.matches(Regex("""\d{2}:\d{2}"""))) {
                Toast.makeText(this, R.string.invalid_time, Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            runRequest(
                loadingMessage = getString(R.string.setting_alarm),
                action = { WatchApiClient.setAlarm(alarm).map { alarm } },
                onSuccess = { time ->
                    statusText.text = getString(R.string.alarm_set, time)
                    refreshStatus()
                }
            )
        }

        findViewById<Button>(R.id.lockButton).setOnClickListener {
            runRequest(
                loadingMessage = getString(R.string.updating_lock),
                action = { WatchApiClient.setLocked(true) },
                onSuccess = {
                    statusText.text = getString(R.string.watch_locked)
                    startActivity(Intent(this, LoginActivity::class.java))
                    finish()
                }
            )
        }

        refreshStatus()
    }

    private fun setupLanguageSwitcher(uaButton: Button, enButton: Button) {
        updateLanguageButtons(uaButton, enButton)

        uaButton.setOnClickListener {
            switchLanguageIfNeeded("uk")
        }

        enButton.setOnClickListener {
            switchLanguageIfNeeded("en")
        }
    }

    private fun switchLanguageIfNeeded(language: String) {
        if (LocaleHelper.getSavedLanguage(this) == language) {
            return
        }

        LocaleHelper.updateLanguage(this, language)
        recreate()
    }

    private fun updateLanguageButtons(uaButton: Button, enButton: Button) {
        val selectedLanguage = LocaleHelper.getSavedLanguage(this)
        val activeColor = getColor(R.color.brand)
        val inactiveColor = getColor(android.R.color.white)
        val activeTextColor = getColor(android.R.color.white)
        val inactiveTextColor = getColor(R.color.ink)

        uaButton.setBackgroundColor(if (selectedLanguage == "uk") activeColor else inactiveColor)
        uaButton.setTextColor(if (selectedLanguage == "uk") activeTextColor else inactiveTextColor)
        enButton.setBackgroundColor(if (selectedLanguage == "en") activeColor else inactiveColor)
        enButton.setTextColor(if (selectedLanguage == "en") activeTextColor else inactiveTextColor)
    }

    private fun refreshStatus() {
        runRequest(
            loadingMessage = getString(R.string.loading_status),
            action = WatchApiClient::getStatus,
            onSuccess = { watchStatus ->
                bindStatus(watchStatus)
                statusText.text = getString(R.string.status_loaded)
            }
        )
    }

    private fun bindStatus(status: WatchStatus) {
        timeValue.text = status.time
        dayValue.text = status.day
        dateValue.text = status.date
        batteryValue.text = getString(R.string.battery_format, status.battery)
        stepsValue.text = status.steps.toString()
        temperatureValue.text = getString(R.string.temperature_format, status.temperature)
        alarmValue.text = status.alarm
        authValue.text = if (status.authenticated) getString(R.string.unlocked) else getString(R.string.locked)

        if (!status.authenticated) {
            statusText.text = getString(R.string.auth_required)
        }

        if (status.time == status.alarm && lastAlarmNotified != status.alarm) {
            showAlarmNotification(status.alarm)
            lastAlarmNotified = status.alarm
        }
    }

    private fun <T> runRequest(
        loadingMessage: String,
        action: () -> Result<T>,
        onSuccess: (T) -> Unit
    ) {
        progressBar.visibility = View.VISIBLE
        statusText.text = loadingMessage

        executor.execute {
            val result = action()
            runOnUiThread {
                progressBar.visibility = View.GONE
                result.onSuccess(onSuccess).onFailure { error ->
                    statusText.text = error.message ?: getString(R.string.connection_error)
                    if (error.message?.contains("Authenticate", ignoreCase = true) == true) {
                        Toast.makeText(this, R.string.auth_required, Toast.LENGTH_SHORT).show()
                    }
                }
            }
        }
    }

    private fun showAlarmNotification(alarm: String) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(this, android.Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            return
        }

        val builder = NotificationCompat.Builder(this, "alarm_channel")
            .setSmallIcon(android.R.drawable.ic_lock_idle_alarm)
            .setContentTitle(getString(R.string.alarm_notification_title))
            .setContentText(getString(R.string.alarm_notification_body, alarm))
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)

        NotificationManagerCompat.from(this).notify(1001, builder.build())
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                "alarm_channel",
                getString(R.string.alarm_channel_name),
                NotificationManager.IMPORTANCE_DEFAULT
            )
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        executor.shutdown()
    }
}

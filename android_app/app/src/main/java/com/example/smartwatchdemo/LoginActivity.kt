package com.example.smartwatchdemo

import android.content.Intent
import android.content.Context
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import java.util.concurrent.Executors

// Перший екран застосунку: відповідає тільки за PIN-автентифікацію та вибір мови.
class LoginActivity : AppCompatActivity() {
    private val executor = Executors.newSingleThreadExecutor()

    override fun attachBaseContext(newBase: Context) {
        super.attachBaseContext(LocaleHelper.wrap(newBase))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_login)

        val uaButton = findViewById<Button>(R.id.uaButton)
        val enButton = findViewById<Button>(R.id.enButton)
        val pinInput = findViewById<EditText>(R.id.pinInput)
        val loginButton = findViewById<Button>(R.id.loginButton)
        val progressBar = findViewById<ProgressBar>(R.id.loginProgress)
        val statusText = findViewById<TextView>(R.id.loginStatusText)

        setupLanguageSwitcher(uaButton, enButton)

        loginButton.setOnClickListener {
            val pin = pinInput.text.toString().trim()
            if (pin.length != 4) {
                Toast.makeText(this, R.string.invalid_pin, Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            // Блокуємо кнопку на час запиту, щоб уникнути дубльованих POST /auth.
            progressBar.visibility = View.VISIBLE
            loginButton.isEnabled = false
            statusText.text = getString(R.string.authenticating)

            executor.execute {
                val result = WatchApiClient.authenticate(pin)
                runOnUiThread {
                    progressBar.visibility = View.GONE
                    loginButton.isEnabled = true

                    result.onSuccess { authenticated ->
                        if (authenticated) {
                            statusText.text = getString(R.string.login_success)
                            startActivity(Intent(this, DashboardActivity::class.java))
                            finish()
                        } else {
                            statusText.text = getString(R.string.login_failed)
                        }
                    }.onFailure { error ->
                        statusText.text = error.message ?: getString(R.string.connection_error)
                    }
                }
            }
        }
    }

    // Налаштовує простий двомовний перемикач без окремого settings-екрана.
    private fun setupLanguageSwitcher(uaButton: Button, enButton: Button) {
        updateLanguageButtons(uaButton, enButton)

        uaButton.setOnClickListener {
            switchLanguageIfNeeded("uk")
        }

        enButton.setOnClickListener {
            switchLanguageIfNeeded("en")
        }
    }

    // Якщо мова не змінюється, recreate() не викликаємо зайвий раз.
    private fun switchLanguageIfNeeded(language: String) {
        if (LocaleHelper.getSavedLanguage(this) == language) {
            return
        }

        LocaleHelper.updateLanguage(this, language)
        recreate()
    }

    // Візуально підсвічує активну мову для зрозумілості користувачу.
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

    override fun onDestroy() {
        super.onDestroy()
        executor.shutdown()
    }
}

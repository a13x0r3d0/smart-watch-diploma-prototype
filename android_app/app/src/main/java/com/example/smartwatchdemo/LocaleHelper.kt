package com.example.smartwatchdemo

import android.content.Context
import android.content.res.Configuration
import java.util.Locale

// Централізує логіку локалізації, щоб обидва екрани працювали однаково.
object LocaleHelper {
    private const val prefsName = "language_prefs"
    private const val languageKey = "selected_language"
    private const val defaultLanguage = "uk"

    // Повертає Context з уже підставленою вибраною мовою.
    fun wrap(context: Context): Context {
        val language = getSavedLanguage(context)
        val locale = Locale(language)
        Locale.setDefault(locale)

        val configuration = Configuration(context.resources.configuration)
        configuration.setLocale(locale)
        configuration.setLayoutDirection(locale)

        return context.createConfigurationContext(configuration)
    }

    // Зчитує останню вибрану мову з SharedPreferences.
    fun getSavedLanguage(context: Context): String {
        return context.getSharedPreferences(prefsName, Context.MODE_PRIVATE)
            .getString(languageKey, defaultLanguage)
            ?: defaultLanguage
    }

    // Оновлює збережену мову без складної навігації чи окремого settings-екрана.
    fun updateLanguage(context: Context, language: String) {
        context.getSharedPreferences(prefsName, Context.MODE_PRIVATE)
            .edit()
            .putString(languageKey, language)
            .apply()
    }
}

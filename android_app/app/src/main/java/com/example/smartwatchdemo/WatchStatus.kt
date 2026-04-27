package com.example.smartwatchdemo

// DTO-модель стану годинника, яку Android отримує з локального HTTP API.
data class WatchStatus(
    val time: String,
    val day: String,
    val date: String,
    val battery: Int,
    val steps: Int,
    val temperature: Int,
    val alarm: String,
    val authenticated: Boolean
)

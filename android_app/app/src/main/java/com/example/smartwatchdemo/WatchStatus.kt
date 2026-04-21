package com.example.smartwatchdemo

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

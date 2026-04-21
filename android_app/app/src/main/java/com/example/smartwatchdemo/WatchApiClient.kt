package com.example.smartwatchdemo

import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL

object WatchApiClient {
    private const val BASE_URL = "http://10.0.2.2:8080"

    data class ApiResult(
        val success: Boolean,
        val code: Int,
        val body: String
    )

    fun getStatus(): Result<WatchStatus> {
        return runCatching {
            val response = request("GET", "/status")
            if (response.code !in 200..299) {
                throw IllegalStateException(parseError(response.body))
            }

            val json = JSONObject(response.body)
            WatchStatus(
                time = json.getString("time"),
                day = json.getString("day"),
                date = json.getString("date"),
                battery = json.getInt("battery"),
                steps = json.getInt("steps"),
                temperature = json.getInt("temperature"),
                alarm = json.getString("alarm"),
                authenticated = json.getBoolean("authenticated")
            )
        }
    }

    fun authenticate(pin: String): Result<Boolean> {
        return runCatching {
            val payload = JSONObject().put("pin", pin).toString()
            val response = request("POST", "/auth", payload)
            if (response.code == 401) {
                return@runCatching false
            }
            if (response.code !in 200..299) {
                throw IllegalStateException(parseError(response.body))
            }
            JSONObject(response.body).optBoolean("authenticated", false)
        }
    }

    fun syncTime(time: String): Result<Unit> = postTime("/sync-time", time)

    fun setAlarm(time: String): Result<Unit> = postTime("/alarm", time)

    fun setLocked(locked: Boolean): Result<Boolean> {
        return runCatching {
            val payload = JSONObject().put("locked", locked.toString()).toString()
            val response = request("POST", "/lock", payload)
            if (response.code !in 200..299) {
                throw IllegalStateException(parseError(response.body))
            }
            JSONObject(response.body).optBoolean("authenticated", false)
        }
    }

    private fun postTime(path: String, time: String): Result<Unit> {
        return runCatching {
            val payload = JSONObject().put("time", time).toString()
            val response = request("POST", path, payload)
            if (response.code !in 200..299) {
                throw IllegalStateException(parseError(response.body))
            }
        }
    }

    private fun request(method: String, path: String, body: String? = null): ApiResult {
        val connection = (URL(BASE_URL + path).openConnection() as HttpURLConnection).apply {
            requestMethod = method
            connectTimeout = 3000
            readTimeout = 3000
            setRequestProperty("Content-Type", "application/json")
            doInput = true
        }

        if (body != null) {
            connection.doOutput = true
            OutputStreamWriter(connection.outputStream).use { writer ->
                writer.write(body)
                writer.flush()
            }
        }

        val code = connection.responseCode
        val stream = if (code in 200..299) connection.inputStream else connection.errorStream
        val responseBody = stream?.let { input ->
            BufferedReader(InputStreamReader(input)).use { reader ->
                reader.readText()
            }
        }.orEmpty()

        connection.disconnect()
        return ApiResult(success = code in 200..299, code = code, body = responseBody)
    }

    private fun parseError(body: String): String {
        return runCatching {
            JSONObject(body).optString("error", "Request failed")
        }.getOrDefault("Request failed")
    }
}

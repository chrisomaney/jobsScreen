package au.chrisomaney.kidsadmin.mqtt

import android.os.Handler
import android.os.Looper
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import okio.ByteString.Companion.toByteString
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.random.Random

/**
 * Minimal MQTT 3.1.1 client over WebSockets (binary frames).
 * Same protocol behaviour as the iOS Kids Admin app.
 */
class MqttClient(
    private val clientId: String = "kids-admin-android-${Random.nextInt(1000, 9999)}",
) {
    sealed class State {
        data object Disconnected : State()
        data object Connecting : State()
        data object Connected : State()
        data class Failed(val message: String) : State()
    }

    var onState: ((State) -> Unit)? = null
    var onMessage: ((String, ByteArray) -> Unit)? = null

    private val http = OkHttpClient.Builder()
        .pingInterval(30, TimeUnit.SECONDS)
        .readTimeout(0, TimeUnit.MILLISECONDS)
        .build()

    private var socket: WebSocket? = null
    private var username: String = ""
    private var password: String = ""
    private var pendingSubs: List<String> = emptyList()
    private val open = AtomicBoolean(false)
    private val main = Handler(Looper.getMainLooper())

    private fun emitState(state: State) {
        main.post { onState?.invoke(state) }
    }

    private fun emitMessage(topic: String, payload: ByteArray) {
        main.post { onMessage?.invoke(topic, payload) }
    }

    fun connect(urlString: String, username: String, password: String) {
        disconnect()
        this.username = username
        this.password = password
        emitState(State.Connecting)

        var raw = urlString.trim()
        if (raw.startsWith("http://")) raw = "ws://" + raw.removePrefix("http://")
        if (raw.startsWith("https://")) raw = "wss://" + raw.removePrefix("https://")

        val request = try {
            Request.Builder()
                .url(raw)
                .header("Sec-WebSocket-Protocol", "mqtt")
                .build()
        } catch (e: Exception) {
            emitState(State.Failed("Bad broker URL"))
            return
        }

        socket = http.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                open.set(true)
                webSocket.send(connectPacket().toByteString())
            }

            override fun onMessage(webSocket: WebSocket, bytes: ByteString) {
                handle(bytes.toByteArray())
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                open.set(false)
                emitState(State.Failed(t.message ?: "WebSocket failed"))
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                open.set(false)
                emitState(State.Disconnected)
            }
        })
    }

    fun disconnect() {
        open.set(false)
        socket?.close(1000, "bye")
        socket = null
        emitState(State.Disconnected)
    }

    fun subscribe(topics: List<String>) {
        pendingSubs = topics
        if (open.get() && topics.isNotEmpty()) {
            socket?.send(subscribePacket(topics).toByteString())
        }
    }

    fun publish(topic: String, payload: String, retain: Boolean = false, qos: Int = 0) {
        publish(topic, payload.toByteArray(Charsets.UTF_8), retain, qos)
    }

    fun publish(topic: String, data: ByteArray, retain: Boolean = false, qos: Int = 0) {
        val sock = socket ?: return
        var flags = 0x30
        if (retain) flags = flags or 0x01
        if (qos == 1) flags = flags or 0x02
        val body = ArrayList<Byte>()
        body.addAll(mqttString(topic).toList())
        if (qos > 0) {
            body.add(0x00)
            body.add(0x01)
        }
        body.addAll(data.toList())
        sock.send(mqttPacket(flags, body.toByteArray()).toByteString())
    }

    private fun connectPacket(): ByteArray {
        val payload = ArrayList<Byte>()
        payload.addAll(mqttString("MQTT").toList())
        payload.add(0x04) // 3.1.1
        var flags = 0x02 // clean session
        if (username.isNotEmpty()) {
            flags = flags or 0x80
            if (password.isNotEmpty()) flags = flags or 0x40
        }
        payload.add(flags.toByte())
        payload.add(0x00)
        payload.add(0x3C) // keep alive 60s
        payload.addAll(mqttString(clientId).toList())
        if (username.isNotEmpty()) {
            payload.addAll(mqttString(username).toList())
            if (password.isNotEmpty()) {
                payload.addAll(mqttString(password).toList())
            }
        }
        return mqttPacket(0x10, payload.toByteArray())
    }

    private fun subscribePacket(topics: List<String>): ByteArray {
        val body = ArrayList<Byte>()
        body.add(0x00)
        body.add(0x0A) // packet id
        for (t in topics) {
            body.addAll(mqttString(t).toList())
            body.add(0x00) // QoS 0
        }
        return mqttPacket(0x82, body.toByteArray())
    }

    private fun handle(data: ByteArray) {
        var idx = 0
        while (idx < data.size) {
            val type = (data[idx].toInt() and 0xFF) shr 4
            val (remaining, lenSize) = decodeRemainingLength(data, idx + 1) ?: return
            val header = 1 + lenSize
            val start = idx + header
            val end = start + remaining
            if (end > data.size) return
            val body = data.copyOfRange(start, end)
            when (type) {
                2 -> { // CONNACK
                    val code = if (body.size >= 2) body[1].toInt() and 0xFF else 0xFF
                    if (code == 0) {
                        emitState(State.Connected)
                        if (pendingSubs.isNotEmpty()) {
                            socket?.send(subscribePacket(pendingSubs).toByteString())
                        }
                    } else {
                        emitState(State.Failed("MQTT CONNACK $code"))
                    }
                }
                3 -> parsePublish(data[idx].toInt() and 0xFF, body)
                9, 13 -> Unit // SUBACK / PINGRESP
            }
            idx = end
        }
    }

    private fun parsePublish(flags: Int, body: ByteArray) {
        if (body.size < 2) return
        val tlen = ((body[0].toInt() and 0xFF) shl 8) or (body[1].toInt() and 0xFF)
        if (body.size < 2 + tlen) return
        val topic = String(body, 2, tlen, Charsets.UTF_8)
        var pos = 2 + tlen
        val qos = (flags shr 1) and 0x03
        if (qos > 0) pos += 2
        val payload = body.copyOfRange(pos, body.size)
        emitMessage(topic, payload)
    }

    private fun mqttString(s: String): ByteArray {
        val utf = s.toByteArray(Charsets.UTF_8)
        return byteArrayOf(((utf.size shr 8) and 0xFF).toByte(), (utf.size and 0xFF).toByte()) + utf
    }

    private fun mqttPacket(typeFlags: Int, body: ByteArray): ByteArray {
        return byteArrayOf(typeFlags.toByte()) + encodeRemainingLength(body.size) + body
    }

    private fun encodeRemainingLength(length: Int): ByteArray {
        var x = length
        val out = ArrayList<Byte>()
        do {
            var encoded = x % 128
            x /= 128
            if (x > 0) encoded = encoded or 0x80
            out.add(encoded.toByte())
        } while (x > 0)
        return out.toByteArray()
    }

    private fun decodeRemainingLength(data: ByteArray, start: Int): Pair<Int, Int>? {
        var multiplier = 1
        var value = 0
        var i = start
        while (i < data.size) {
            val encoded = data[i].toInt() and 0xFF
            value += (encoded and 127) * multiplier
            i++
            if (encoded and 128 == 0) return value to (i - start)
            multiplier *= 128
            if (multiplier > 128 * 128 * 128) return null
        }
        return null
    }
}

package au.chrisomaney.kidsadmin.data

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import au.chrisomaney.kidsadmin.mqtt.MqttClient
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import org.json.JSONArray
import org.json.JSONObject

data class AdminUiState(
    val connectionLabel: String = "Offline",
    val connected: Boolean = false,
    val lastError: String? = null,
    val settings: ConnectionSettings = ConnectionSettings(),
    val status: Map<String, KidLiveStatus> = AdminKids.all.associate { it.slug to KidLiveStatus() },
    val jobs: List<JobItem> = emptyList(),
    val jobPoints: Map<String, Int> = emptyMap(),
    val expBundle: Int = 0,
    val customSlotsFree: Int? = null,
    val commonInternetOn: Boolean = false,
    val banner: String? = null,
)

class AdminViewModel(app: Application) : AndroidViewModel(app) {
    private val settingsRepo = SettingsRepository(app)
    private val mqtt = MqttClient()
    private var rules: JSONArray = JSONArray()
    private var pendingPaidPoints: Pair<String, Int>? = null
    private var didAutoConnect = false

    private val _ui = MutableStateFlow(AdminUiState())
    val ui: StateFlow<AdminUiState> = _ui.asStateFlow()

    init {
        mqtt.onState = { state ->
            when (state) {
                is MqttClient.State.Disconnected ->
                    _ui.update { it.copy(connected = false, connectionLabel = "Offline") }
                is MqttClient.State.Connecting ->
                    _ui.update { it.copy(connected = false, connectionLabel = "Connecting…") }
                is MqttClient.State.Connected ->
                    _ui.update {
                        it.copy(connected = true, connectionLabel = "Connected", lastError = null)
                    }
                is MqttClient.State.Failed ->
                    _ui.update {
                        it.copy(connected = false, connectionLabel = "Error", lastError = state.message)
                    }
            }
        }
        mqtt.onMessage = { topic, data -> handle(topic, data) }

        viewModelScope.launch {
            settingsRepo.settings.collect { s ->
                _ui.update { it.copy(settings = s) }
                if (!didAutoConnect && s.remember && s.broker.isNotBlank()) {
                    didAutoConnect = true
                    connect()
                }
            }
        }
    }

    fun updateSettings(transform: (ConnectionSettings) -> ConnectionSettings) {
        val next = transform(_ui.value.settings)
        _ui.update { it.copy(settings = next) }
        viewModelScope.launch { settingsRepo.save(next) }
    }

    fun connect() {
        val s = _ui.value.settings
        mqtt.connect(s.broker, s.username, s.password)
        val topics = mutableListOf(
            "kids/config/state",
            "kids/rewards/rules/state",
            "kids/rewards/catalog",
            "kids/common/internet",
        )
        for (kid in AdminKids.all) {
            topics += "kids/${kid.slug}/progress"
            topics += "kids/${kid.slug}/rewards/tally"
            topics += "kids/${kid.slug}/internet"
        }
        mqtt.subscribe(topics)
    }

    fun disconnect() = mqtt.disconnect()

    fun setInternet(slug: String, mode: InternetMode) {
        mqtt.publish("kids/$slug/internet/set", mode.raw)
        _ui.update { state ->
            val cur = state.status[slug] ?: KidLiveStatus()
            val next = cur.copy(
                override = mode,
                internetOn = when (mode) {
                    InternetMode.On -> true
                    InternetMode.Off -> false
                    InternetMode.Auto -> cur.internetOn
                },
            )
            state.copy(status = state.status + (slug to next))
        }
    }

    fun setCommonInternet(on: Boolean) {
        mqtt.publish("kids/common/internet/set", if (on) "on" else "off")
        _ui.update { it.copy(commonInternetOn = on) }
    }

    fun adjustPoints(slug: String, points: Int) {
        if (points == 0) return
        publishJson("kids/rewards/bonus/set", JSONObject().put("kid", slug).put("points", points))
    }

    fun sendBehaviour(
        slug: String,
        points: Int,
        title: String,
        message: String,
        panel: Boolean,
        tv: Boolean,
        seconds: Int,
    ) {
        val body = JSONObject()
            .put("kid", slug)
            .put("points", points)
            .put("behaviour", true)
            .put("title", title)
            .put("message", message)
            .put("panel", panel)
            .put("tv", tv)
            .put("seconds", seconds)
        publishJson("kids/rewards/bonus/set", body)
        _ui.update { it.copy(banner = if (points > 0) "Reward sent" else "Penalty sent") }
    }

    fun addItem(label: String, cat: String, detail: String, required: Boolean, points: Int?) {
        val add = JSONObject()
            .put("label", label)
            .put("cat", cat)
            .put("required", required)
            .put("kids", JSONArray(AdminKids.all.map { it.slug }))
        if (detail.isNotBlank()) add.put("detail", detail)
        publishJson("kids/config/set", JSONObject().put("add", add))
        if (cat == "paid" && points != null) {
            pendingPaidPoints = label to points
        }
        _ui.update { it.copy(banner = if (cat == "paid") "Adding job…" else "Adding expectation…") }
    }

    fun saveJob(job: JobItem) {
        val fields = JSONObject()
            .put("label", job.label)
            .put("detail", job.detail)
            .put("cat", job.cat)
            .put("required", job.required)
            .put("kids", JSONArray(job.kids))
        publishJson("kids/config/set", JSONObject().put("jobs", JSONObject().put(job.id, fields)))
    }

    fun removeJob(id: String) {
        publishJson("kids/config/set", JSONObject().put("remove", JSONArray().put(id)))
    }

    fun setExpBundle(value: Int) {
        _ui.update { it.copy(expBundle = value.coerceAtLeast(0)) }
    }

    fun setJobPoints(id: String, value: Int) {
        _ui.update { it.copy(jobPoints = it.jobPoints + (id to value.coerceAtLeast(0))) }
    }

    fun savePoints() {
        val points = _ui.value.jobPoints.toMutableMap()
        for (j in _ui.value.jobs) {
            if (j.cat != "paid") points[j.id] = 0
        }
        val jp = JSONObject()
        for ((k, v) in points) jp.put(k, v)
        val body = JSONObject()
            .put("expectations_complete_points", _ui.value.expBundle)
            .put("job_points", jp)
            .put("rules", rules)
        publishJson("kids/rewards/rules/set", body)
        _ui.update { it.copy(jobPoints = points, banner = "Points saved") }
    }

    fun clearBanner() = _ui.update { it.copy(banner = null) }

    private fun handle(topic: String, data: ByteArray) {
        val text = data.toString(Charsets.UTF_8)
        when {
            topic == "kids/common/internet" ->
                _ui.update { it.copy(commonInternetOn = text == "on") }
            topic == "kids/config/state" -> parseConfig(data)
            topic == "kids/rewards/rules/state" -> parseRules(data)
            else -> {
                for (kid in AdminKids.all) {
                    when (topic) {
                        "kids/${kid.slug}/rewards/tally" -> {
                            val tally = text.toIntOrNull() ?: 0
                            patchStatus(kid.slug) { it.copy(tally = tally) }
                        }
                        "kids/${kid.slug}/internet" ->
                            patchStatus(kid.slug) { it.copy(internetOn = text == "on") }
                        "kids/${kid.slug}/progress" -> parseProgress(kid.slug, data)
                    }
                }
            }
        }
    }

    private fun patchStatus(slug: String, transform: (KidLiveStatus) -> KidLiveStatus) {
        _ui.update { state ->
            val cur = state.status[slug] ?: KidLiveStatus()
            state.copy(status = state.status + (slug to transform(cur)))
        }
    }

    private fun parseProgress(slug: String, data: ByteArray) {
        try {
            val obj = JSONObject(String(data, Charsets.UTF_8))
            patchStatus(slug) { cur ->
                cur.copy(
                    tally = obj.optInt("rewards", cur.tally),
                    internetOn = obj.optBoolean("internet", cur.internetOn),
                    jobsReady = obj.optBoolean("jobs_ready", cur.jobsReady),
                    pct = obj.optInt("pct", cur.pct),
                    override = InternetMode.from(obj.optString("override", cur.override.raw)),
                )
            }
        } catch (_: Exception) {
        }
    }

    private fun parseConfig(data: ByteArray) {
        try {
            val obj = JSONObject(String(data, Charsets.UTF_8))
            val prev = _ui.value.jobs.map { it.id }.toSet()
            val arr = obj.optJSONArray("jobs") ?: JSONArray()
            val jobs = buildList {
                for (i in 0 until arr.length()) {
                    val j = arr.getJSONObject(i)
                    val id = j.optString("id")
                    if (id.isBlank()) continue
                    val kidsArr = j.optJSONArray("kids") ?: JSONArray()
                    val kids = buildList {
                        for (k in 0 until kidsArr.length()) add(kidsArr.getString(k))
                    }
                    add(
                        JobItem(
                            id = id,
                            label = j.optString("label", id),
                            cat = if (j.optString("cat") == "paid") "paid" else "expectation",
                            required = j.optBoolean("required", true),
                            custom = j.optBoolean("custom", false),
                            detail = j.optString("detail", ""),
                            kids = kids,
                        )
                    )
                }
            }
            val points = _ui.value.jobPoints.toMutableMap()
            for (j in jobs) {
                if (j.id !in points) points[j.id] = if (j.cat == "paid") 1 else 0
            }
            pendingPaidPoints?.let { (label, pts) ->
                val created = jobs.firstOrNull { it.id !in prev && it.label == label && it.cat == "paid" }
                if (created != null) {
                    points[created.id] = pts
                    pendingPaidPoints = null
                    _ui.update {
                        it.copy(
                            jobs = jobs,
                            jobPoints = points,
                            customSlotsFree = obj.optInt("customSlotsFree").takeIf { obj.has("customSlotsFree") },
                        )
                    }
                    savePoints()
                    return
                }
            }
            _ui.update {
                it.copy(
                    jobs = jobs,
                    jobPoints = points,
                    customSlotsFree = if (obj.has("customSlotsFree")) obj.optInt("customSlotsFree") else null,
                )
            }
        } catch (_: Exception) {
        }
    }

    private fun parseRules(data: ByteArray) {
        try {
            val obj = JSONObject(String(data, Charsets.UTF_8))
            val points = _ui.value.jobPoints.toMutableMap()
            val jp = obj.optJSONObject("job_points")
            if (jp != null) {
                val keys = jp.keys()
                while (keys.hasNext()) {
                    val k = keys.next()
                    points[k] = jp.optInt(k)
                }
            }
            rules = obj.optJSONArray("rules") ?: JSONArray()
            _ui.update {
                it.copy(
                    jobPoints = points,
                    expBundle = obj.optInt("expectations_complete_points", it.expBundle),
                )
            }
        } catch (_: Exception) {
        }
    }

    private fun publishJson(topic: String, obj: JSONObject) {
        mqtt.publish(topic, obj.toString())
    }

    override fun onCleared() {
        mqtt.disconnect()
        super.onCleared()
    }
}

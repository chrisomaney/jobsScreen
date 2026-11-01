import Foundation
import Combine

@MainActor
final class AdminStore: ObservableObject {
    @Published var connectionLabel = "Offline"
    @Published var connected = false
    @Published var status: [String: KidLiveStatus] = [:]
    @Published var jobs: [JobItem] = []
    @Published var jobPoints: [String: Int] = [:]
    @Published var expBundle: Int = 0
    @Published var customSlotsFree: Int? = nil
    @Published var commonInternetOn = false
    @Published var internetSchedules: [InternetSchedule] = []
    @Published var lastError: String?
    @Published var bleReady = false
    @Published var bleScanning = false
    @Published var bleConnecting = false
    @Published var blePanelName: String?
    @Published var bleError: String?

    private let ble = BlePanelClient()

    private var settings: SettingsStore?
    private let mqtt = MqttClient()
    private var rules: [[String: Any]] = []
    private var connectDebounce: Task<Void, Never>?
    private var bleWatchStarted = false
    private var bleBag = Set<AnyCancellable>()
    /// MQTT or BLE is usable for saving jobs.
    var canSaveJobs: Bool { connected || bleReady }

    init() {
        for kid in AdminKid.all {
            status[kid.slug] = KidLiveStatus()
        }
    }

    func attach(settings: SettingsStore) {
        self.settings = settings
        mqtt.onState = { [weak self] state in
            Task { @MainActor in
                guard let self else { return }
                switch state {
                case .disconnected:
                    self.connected = false
                    self.connectionLabel = "Offline"
                case .connecting:
                    self.connected = false
                    self.connectionLabel = "Connecting…"
                case .connected:
                    self.connected = true
                    self.connectionLabel = "Connected"
                    self.lastError = nil
                case .failed(let msg):
                    self.connected = false
                    self.connectionLabel = "Reconnecting…"
                    self.lastError = msg
                }
            }
        }
        mqtt.onMessage = { [weak self] topic, data in
            Task { @MainActor in
                self?.handle(topic: topic, data: data)
            }
        }
        ble.onConfigState = { [weak self] data in
            Task { @MainActor in
                self?.parseConfig(data)
            }
        }
        ble.onAck = { [weak self] ok, message in
            Task { @MainActor in
                if !ok {
                    self?.lastError = message.isEmpty ? "BLE save failed" : message
                } else {
                    self?.lastError = nil
                }
            }
        }
    }

    func connect() {
        guard let settings else { return }
        // Debounce rapid Connect taps so we don't stack WebSocket sessions (UI freeze).
        connectDebounce?.cancel()
        connectDebounce = Task { @MainActor in
            try? await Task.sleep(nanoseconds: 150_000_000)
            guard !Task.isCancelled else { return }
            self.mqtt.connect(
                urlString: settings.broker,
                username: settings.username,
                password: settings.password
            )
            var topics = [
                "kids/config/state",
                "kids/rewards/rules/state",
                "kids/rewards/catalog",
                "kids/common/internet",
                "kids/internet/schedules/state",
            ]
            for kid in AdminKid.all {
                topics.append("kids/\(kid.slug)/progress")
                topics.append("kids/\(kid.slug)/rewards/tally")
                topics.append("kids/\(kid.slug)/internet")
            }
            self.mqtt.subscribe(topics)
        }
    }

    func disconnect() {
        connectDebounce?.cancel()
        mqtt.disconnect()
    }

    func startBle() {
        if !bleWatchStarted {
            bleWatchStarted = true
            ble.$phase
                .receive(on: DispatchQueue.main)
                .sink { [weak self] phase in
                    Task { @MainActor in
                        self?.applyBlePhase(phase)
                    }
                }
                .store(in: &bleBag)
            ble.$panelName
                .receive(on: DispatchQueue.main)
                .sink { [weak self] name in
                    Task { @MainActor in
                        self?.blePanelName = name
                    }
                }
                .store(in: &bleBag)
        }
        ble.startScan()
        applyBlePhase(ble.phase)
    }

    func stopBle() {
        ble.stop()
        applyBlePhase(.idle)
        blePanelName = nil
        bleError = nil
    }

    func refreshBleConfig() {
        ble.requestConfig()
    }

    private func applyBlePhase(_ phase: BlePanelClient.Phase) {
        bleReady = (phase == .ready)
        bleScanning = (phase == .scanning)
        bleConnecting = (phase == .connecting)
        if case .failed(let msg) = phase {
            bleError = msg
        } else if phase != .idle {
            bleError = nil
        }
    }

    // MARK: - Internet

    func setInternet(slug: String, mode: InternetMode) {
        mqtt.publish("kids/\(slug)/internet/set", payload: mode.rawValue, retain: false)
        if var s = status[slug] {
            s.override = mode
            if mode == .on { s.internetOn = true }
            if mode == .off { s.internetOn = false }
            status[slug] = s
        }
    }

    func setCommonInternet(on: Bool) {
        mqtt.publish("kids/common/internet/set", payload: on ? "on" : "off", retain: false)
        commonInternetOn = on
    }

    func saveInternetSchedules(_ list: [InternetSchedule]? = nil) {
        let source = list ?? internetSchedules
        let arr: [[String: Any]] = source.map { s in
            [
                "id": s.id,
                "label": s.label,
                "kids": s.kids,
                "days": s.days,
                "off_hour": s.offHour,
                "off_minute": s.offMinute,
                "on_hour": s.onHour,
                "on_minute": s.onMinute,
                "enabled": s.enabled,
            ]
        }
        guard let data = try? JSONSerialization.data(withJSONObject: arr),
              let str = String(data: data, encoding: .utf8) else { return }
        mqtt.publish("kids/internet/schedules/set", payload: str, retain: false)
        internetSchedules = source
    }

    func upsertInternetSchedule(_ schedule: InternetSchedule) {
        var list = internetSchedules
        if let idx = list.firstIndex(where: { $0.id == schedule.id }) {
            list[idx] = schedule
        } else {
            list.append(schedule)
        }
        saveInternetSchedules(list)
    }

    func removeInternetSchedule(id: String) {
        saveInternetSchedules(internetSchedules.filter { $0.id != id })
    }

    // MARK: - Points / behaviour

    func adjustPoints(slug: String, points: Int) {
        guard points != 0 else { return }
        let body: [String: Any] = ["kid": slug, "points": points]
        publishJSON("kids/rewards/bonus/set", body)
    }

    func sendBehaviour(slug: String, points: Int, title: String, message: String,
                       panel: Bool, tv: Bool, seconds: Int) {
        let body: [String: Any] = [
            "kid": slug,
            "points": points,
            "behaviour": true,
            "title": title,
            "message": message,
            "panel": panel,
            "tv": tv,
            "seconds": seconds,
        ]
        publishJSON("kids/rewards/bonus/set", body)
    }

    // MARK: - Jobs / expectations

    func addItem(label: String, cat: String, detail: String, required: Bool, points: Int?) {
        var add: [String: Any] = [
            "label": label,
            "cat": cat,
            "required": required,
            "kids": AdminKid.all.map(\.slug),
        ]
        if !detail.isEmpty { add["detail"] = detail }
        publishConfig(["add": add])
        if cat == "paid", let points {
            pendingPaidPoints = (label, points)
        }
    }

    func saveJob(_ job: JobItem) {
        let fields: [String: Any] = [
            "label": job.label,
            "detail": job.detail,
            "cat": job.cat,
            "required": job.required,
            "kids": job.kids,
        ]
        publishConfig(["jobs": [job.id: fields]])
    }

    func removeJob(id: String) {
        publishConfig(["remove": [id]])
    }

    func savePoints() {
        for j in jobs where j.cat != "paid" {
            jobPoints[j.id] = 0
        }
        let body: [String: Any] = [
            "expectations_complete_points": expBundle,
            "job_points": jobPoints,
            "rules": rules,
        ]
        publishJSON("kids/rewards/rules/set", body)
    }

    private var pendingPaidPoints: (String, Int)?

    /// Prefer MQTT; fall back to BLE for job config so parents can still set jobs offline.
    private func publishConfig(_ object: [String: Any]) {
        if connected {
            publishJSON("kids/config/set", object)
            return
        }
        if ble.phase == .ready {
            ble.sendConfigSet(object)
            return
        }
        lastError = "Not connected — use Connect (MQTT) or Bluetooth"
    }

    // MARK: - Incoming

    private func handle(topic: String, data: Data) {
        let text = String(data: data, encoding: .utf8) ?? ""

        if topic == "kids/common/internet" {
            commonInternetOn = (text == "on")
            return
        }

        if topic == "kids/config/state" {
            parseConfig(data)
            return
        }
        if topic == "kids/rewards/rules/state" {
            parseRules(data)
            return
        }
        if topic == "kids/internet/schedules/state" {
            parseInternetSchedules(data)
            return
        }

        for kid in AdminKid.all {
            if topic == "kids/\(kid.slug)/rewards/tally" {
                var s = status[kid.slug] ?? KidLiveStatus()
                s.tally = Int(text) ?? 0
                status[kid.slug] = s
            }
            if topic == "kids/\(kid.slug)/internet" {
                var s = status[kid.slug] ?? KidLiveStatus()
                s.internetOn = (text == "on")
                status[kid.slug] = s
            }
            if topic == "kids/\(kid.slug)/progress" {
                parseProgress(slug: kid.slug, data: data)
            }
        }
    }

    private func parseProgress(slug: String, data: Data) {
        guard let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        var s = status[slug] ?? KidLiveStatus()
        if let rewards = obj["rewards"] as? Int { s.tally = rewards }
        if let internet = obj["internet"] as? Bool { s.internetOn = internet }
        if let jobsReady = obj["jobs_ready"] as? Bool { s.jobsReady = jobsReady }
        if let scheduled = obj["scheduled_off"] as? Bool { s.scheduledOff = scheduled }
        if let pct = obj["pct"] as? Int { s.pct = pct }
        if let ov = obj["override"] as? String, let mode = InternetMode(rawValue: ov) {
            s.override = mode
        }
        status[slug] = s
    }

    private func parseInternetSchedules(_ data: Data) {
        guard let arr = try? JSONSerialization.jsonObject(with: data) as? [[String: Any]] else { return }
        internetSchedules = arr.compactMap { o in
            guard let id = o["id"] as? String else { return nil }
            let days: Int
            if let d = o["days"] as? Int { days = d }
            else if let n = o["days"] as? NSNumber { days = n.intValue }
            else { days = 0x7F }
            return InternetSchedule(
                id: id,
                label: (o["label"] as? String) ?? id,
                kids: (o["kids"] as? [String]) ?? [],
                days: days,
                offHour: (o["off_hour"] as? Int) ?? (o["off_hour"] as? NSNumber)?.intValue ?? 20,
                offMinute: (o["off_minute"] as? Int) ?? (o["off_minute"] as? NSNumber)?.intValue ?? 0,
                onHour: (o["on_hour"] as? Int) ?? (o["on_hour"] as? NSNumber)?.intValue ?? 7,
                onMinute: (o["on_minute"] as? Int) ?? (o["on_minute"] as? NSNumber)?.intValue ?? 0,
                enabled: (o["enabled"] as? Bool) ?? true
            )
        }
    }

    private func parseConfig(_ data: Data) {
        guard let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        customSlotsFree = obj["customSlotsFree"] as? Int
        let arr = obj["jobs"] as? [[String: Any]] ?? []
        let prev = Set(jobs.map(\.id))
        jobs = arr.compactMap { j in
            guard let id = j["id"] as? String else { return nil }
            return JobItem(
                id: id,
                label: (j["label"] as? String) ?? id,
                cat: (j["cat"] as? String) == "paid" ? "paid" : "expectation",
                required: (j["required"] as? Bool) ?? true,
                custom: (j["custom"] as? Bool) ?? false,
                detail: (j["detail"] as? String) ?? "",
                kids: (j["kids"] as? [String]) ?? []
            )
        }
        for j in jobs {
            if jobPoints[j.id] == nil {
                jobPoints[j.id] = j.cat == "paid" ? 1 : 0
            }
        }
        if let pending = pendingPaidPoints {
            if let created = jobs.first(where: { !prev.contains($0.id) && $0.label == pending.0 && $0.cat == "paid" }) {
                jobPoints[created.id] = pending.1
                savePoints()
                pendingPaidPoints = nil
            }
        }
    }

    private func parseRules(_ data: Data) {
        guard let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        if let pts = obj["job_points"] as? [String: Any] {
            for (k, v) in pts {
                if let n = v as? Int { jobPoints[k] = n }
                else if let n = v as? NSNumber { jobPoints[k] = n.intValue }
            }
        }
        if let bundle = obj["expectations_complete_points"] as? Int {
            expBundle = bundle
        } else if let n = obj["expectations_complete_points"] as? NSNumber {
            expBundle = n.intValue
        }
        rules = obj["rules"] as? [[String: Any]] ?? []
    }

    private func publishJSON(_ topic: String, _ object: [String: Any]) {
        guard let data = try? JSONSerialization.data(withJSONObject: object),
              let str = String(data: data, encoding: .utf8) else { return }
        mqtt.publish(topic, payload: str, retain: false)
    }
}

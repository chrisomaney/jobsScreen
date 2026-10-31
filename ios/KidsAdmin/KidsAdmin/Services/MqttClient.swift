import Foundation

/// Minimal MQTT 3.1.1 client over WebSockets (binary frames).
/// Fixes flaky connect by waiting for the WebSocket to open before CONNECT,
/// and auto-reconnects with backoff so the app doesn't sit frozen/offline.
final class MqttClient: NSObject {
    enum State: Equatable {
        case disconnected
        case connecting
        case connected
        case failed(String)
    }

    var onState: ((State) -> Void)?
    var onMessage: ((String, Data) -> Void)?

    private let work = DispatchQueue(label: "au.chrisomaney.KidsAdmin.mqtt", qos: .userInitiated)
    private var task: URLSessionWebSocketTask?
    private var session: URLSession?
    private var pingTimer: Timer?
    private var clientId: String
    private var username: String = ""
    private var password: String = ""
    private var brokerURL: String = ""
    private var pendingSubs: [String] = []
    private var wantsConnection = false
    private var socketOpen = false
    private var mqttReady = false
    private var reconnectAttempt = 0
    private var reconnectWorkItem: DispatchWorkItem?
    private var rxBuffer = Data()

    init(clientId: String = "kids-admin-ios-\(Int.random(in: 1000...9999))") {
        self.clientId = clientId
        super.init()
    }

    func connect(urlString: String, username: String, password: String) {
        work.async { [weak self] in
            guard let self else { return }
            self.wantsConnection = true
            self.reconnectAttempt = 0
            self.open(urlString: urlString, username: username, password: password)
        }
    }

    func disconnect() {
        work.async { [weak self] in
            guard let self else { return }
            self.wantsConnection = false
            self.reconnectWorkItem?.cancel()
            self.reconnectWorkItem = nil
            self.teardown(notify: true)
        }
    }

    func subscribe(_ topics: [String]) {
        work.async { [weak self] in
            guard let self else { return }
            self.pendingSubs = topics
            if self.mqttReady {
                self.sendSubscribe(topics)
            }
        }
    }

    func publish(_ topic: String, payload: String, retain: Bool = false, qos: UInt8 = 0) {
        publish(topic, data: Data(payload.utf8), retain: retain, qos: qos)
    }

    func publish(_ topic: String, data: Data, retain: Bool = false, qos: UInt8 = 0) {
        work.async { [weak self] in
            guard let self, self.mqttReady, let task = self.task else { return }
            var flags: UInt8 = 0x30 // PUBLISH
            if retain { flags |= 0x01 }
            if qos == 1 { flags |= 0x02 }
            var body = Data()
            body.append(self.mqttString(topic))
            if qos > 0 {
                body.append(contentsOf: [0x00, 0x01]) // packet id
            }
            body.append(data)
            let packet = self.mqttPacket(flags, body)
            task.send(.data(packet)) { [weak self] err in
                if let err {
                    self?.failAndMaybeReconnect(err.localizedDescription)
                }
            }
        }
    }

    // MARK: - Connection lifecycle

    private func open(urlString: String, username: String, password: String) {
        teardown(notify: false)
        self.username = username
        self.password = password
        self.brokerURL = urlString
        socketOpen = false
        mqttReady = false
        rxBuffer.removeAll(keepingCapacity: true)
        emit(.connecting)

        var raw = urlString.trimmingCharacters(in: .whitespacesAndNewlines)
        if raw.hasPrefix("http://") { raw = "ws://" + raw.dropFirst(7) }
        if raw.hasPrefix("https://") { raw = "wss://" + raw.dropFirst(8) }
        guard let url = URL(string: raw) else {
            emit(.failed("Bad broker URL"))
            return
        }

        let config = URLSessionConfiguration.default
        config.waitsForConnectivity = true
        config.timeoutIntervalForRequest = 30
        config.timeoutIntervalForResource = 60
        let session = URLSession(configuration: config, delegate: self, delegateQueue: nil)
        self.session = session
        var request = URLRequest(url: url)
        request.timeoutInterval = 30
        // Mosquitto websockets MQTT subprotocol
        request.setValue("mqtt", forHTTPHeaderField: "Sec-WebSocket-Protocol")
        let task = session.webSocketTask(with: request)
        self.task = task
        task.resume()
        listen()
        // CONNECT is sent from didOpenWithProtocol — sending earlier causes flaky connects.
    }

    private func teardown(notify: Bool) {
        DispatchQueue.main.async {
            self.pingTimer?.invalidate()
            self.pingTimer = nil
        }
        task?.cancel(with: .goingAway, reason: nil)
        task = nil
        session?.invalidateAndCancel()
        session = nil
        socketOpen = false
        mqttReady = false
        rxBuffer.removeAll()
        if notify {
            emit(.disconnected)
        }
    }

    private func failAndMaybeReconnect(_ message: String) {
        emit(.failed(message))
        teardown(notify: false)
        scheduleReconnect()
    }

    private func scheduleReconnect() {
        guard wantsConnection else { return }
        reconnectWorkItem?.cancel()
        let attempt = reconnectAttempt
        reconnectAttempt = min(reconnectAttempt + 1, 6)
        let delay = min(30.0, pow(2.0, Double(attempt))) // 1,2,4,8,16,30…
        let item = DispatchWorkItem { [weak self] in
            guard let self, self.wantsConnection else { return }
            self.open(urlString: self.brokerURL, username: self.username, password: self.password)
        }
        reconnectWorkItem = item
        work.asyncAfter(deadline: .now() + delay, execute: item)
        emit(.connecting)
    }

    private func emit(_ state: State) {
        DispatchQueue.main.async { [weak self] in
            self?.onState?(state)
        }
    }

    // MARK: - MQTT packets

    private func sendConnectPacket() {
        guard let task else { return }
        var payload = Data()
        payload.append(mqttString("MQTT"))
        payload.append(0x04) // protocol level 3.1.1
        var connectFlags: UInt8 = 0x02 // clean session
        if !username.isEmpty {
            connectFlags |= 0x80
            if !password.isEmpty { connectFlags |= 0x40 }
        }
        payload.append(connectFlags)
        payload.append(contentsOf: [0x00, 0x2D]) // keep alive 45s
        payload.append(mqttString(clientId))
        if !username.isEmpty {
            payload.append(mqttString(username))
            if !password.isEmpty {
                payload.append(mqttString(password))
            }
        }
        let packet = mqttPacket(0x10, payload)
        task.send(.data(packet)) { [weak self] err in
            if let err {
                self?.failAndMaybeReconnect(err.localizedDescription)
            }
        }
    }

    private func sendSubscribe(_ topics: [String]) {
        guard let task, !topics.isEmpty else { return }
        var body = Data([0x00, 0x0A]) // packet id
        for t in topics {
            body.append(mqttString(t))
            body.append(0x00) // QoS 0
        }
        let packet = mqttPacket(0x82, body)
        task.send(.data(packet)) { _ in }
    }

    private func sendPing() {
        work.async { [weak self] in
            guard let self, self.mqttReady else { return }
            self.task?.send(.data(Data([0xC0, 0x00]))) { [weak self] err in
                if let err {
                    self?.failAndMaybeReconnect(err.localizedDescription)
                }
            }
        }
    }

    private func startPing() {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.pingTimer?.invalidate()
            let timer = Timer.scheduledTimer(withTimeInterval: 20, repeats: true) { [weak self] _ in
                self?.sendPing()
            }
            RunLoop.main.add(timer, forMode: .common)
            self.pingTimer = timer
        }
    }

    private func listen() {
        task?.receive { [weak self] result in
            guard let self else { return }
            self.work.async {
                switch result {
                case .failure(let err):
                    if self.wantsConnection {
                        self.failAndMaybeReconnect(err.localizedDescription)
                    }
                case .success(let message):
                    switch message {
                    case .data(let data):
                        self.rxBuffer.append(data)
                        self.drainBuffer()
                    case .string(let str):
                        self.rxBuffer.append(Data(str.utf8))
                        self.drainBuffer()
                    @unknown default:
                        break
                    }
                    if self.wantsConnection, self.task != nil {
                        self.listen()
                    }
                }
            }
        }
    }

    private func drainBuffer() {
        while true {
            guard rxBuffer.count >= 2 else { return }
            let type = rxBuffer[0] >> 4
            guard let (remaining, lenSize) = decodeRemainingLength(rxBuffer, start: 1) else { return }
            let header = 1 + lenSize
            let end = header + remaining
            guard rxBuffer.count >= end else { return }
            let body = rxBuffer.subdata(in: header..<end)
            let flags = rxBuffer[0]
            rxBuffer.removeSubrange(0..<end)

            switch type {
            case 2: // CONNACK
                let code = body.count >= 2 ? body[1] : 0xFF
                if code == 0 {
                    mqttReady = true
                    reconnectAttempt = 0
                    emit(.connected)
                    if !pendingSubs.isEmpty {
                        sendSubscribe(pendingSubs)
                    }
                    startPing()
                } else {
                    failAndMaybeReconnect("MQTT CONNACK \(code)")
                }
            case 3: // PUBLISH
                parsePublish(flags: flags, body: body)
            case 9, 13: // SUBACK / PINGRESP
                break
            default:
                break
            }
        }
    }

    private func parsePublish(flags: UInt8, body: Data) {
        guard body.count >= 2 else { return }
        let tlen = Int(body[0]) << 8 | Int(body[1])
        guard body.count >= 2 + tlen else { return }
        let topicData = body.subdata(in: 2..<(2 + tlen))
        let topic = String(data: topicData, encoding: .utf8) ?? ""
        var pos = 2 + tlen
        let qos = (flags >> 1) & 0x03
        if qos > 0 { pos += 2 }
        guard pos <= body.count else { return }
        let payload = body.subdata(in: pos..<body.count)
        DispatchQueue.main.async { [weak self] in
            self?.onMessage?(topic, payload)
        }
    }

    private func mqttString(_ s: String) -> Data {
        let utf = Array(s.utf8)
        var d = Data([UInt8((utf.count >> 8) & 0xFF), UInt8(utf.count & 0xFF)])
        d.append(contentsOf: utf)
        return d
    }

    private func mqttPacket(_ typeFlags: UInt8, _ body: Data) -> Data {
        var packet = Data([typeFlags])
        packet.append(encodeRemainingLength(body.count))
        packet.append(body)
        return packet
    }

    private func encodeRemainingLength(_ length: Int) -> Data {
        var x = length
        var out = Data()
        repeat {
            var encoded = UInt8(x % 128)
            x /= 128
            if x > 0 { encoded |= 0x80 }
            out.append(encoded)
        } while x > 0
        return out
    }

    private func decodeRemainingLength(_ data: Data, start: Int) -> (Int, Int)? {
        var multiplier = 1
        var value = 0
        var i = start
        while i < data.count {
            let encoded = Int(data[i])
            value += (encoded & 127) * multiplier
            i += 1
            if encoded & 128 == 0 {
                return (value, i - start)
            }
            multiplier *= 128
            if multiplier > 128 * 128 * 128 { return nil }
        }
        return nil
    }
}

extension MqttClient: URLSessionWebSocketDelegate {
    func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask,
                    didOpenWithProtocol protocol: String?) {
        work.async { [weak self] in
            guard let self, self.task === webSocketTask else { return }
            self.socketOpen = true
            self.sendConnectPacket()
        }
    }

    func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask,
                    didCloseWith closeCode: URLSessionWebSocketTask.CloseCode, reason: Data?) {
        work.async { [weak self] in
            guard let self else { return }
            if self.wantsConnection {
                self.failAndMaybeReconnect("WebSocket closed (\(closeCode.rawValue))")
            } else {
                self.emit(.disconnected)
            }
        }
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        guard let error else { return }
        work.async { [weak self] in
            guard let self, self.wantsConnection else { return }
            self.failAndMaybeReconnect(error.localizedDescription)
        }
    }
}

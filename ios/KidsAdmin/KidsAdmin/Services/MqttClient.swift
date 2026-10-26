import Foundation

/// Minimal MQTT 3.1.1 client over WebSockets (binary frames).
/// Enough for Kids Admin: connect, subscribe, publish (incl. retain).
final class MqttClient: NSObject {
    enum State: Equatable {
        case disconnected
        case connecting
        case connected
        case failed(String)
    }

    var onState: ((State) -> Void)?
    var onMessage: ((String, Data) -> Void)?

    private var task: URLSessionWebSocketTask?
    private var session: URLSession?
    private var pingTimer: Timer?
    private var clientId: String
    private var username: String = ""
    private var password: String = ""
    private var pendingSubs: [String] = []

    init(clientId: String = "kids-admin-ios-\(Int.random(in: 1000...9999))") {
        self.clientId = clientId
        super.init()
    }

    func connect(urlString: String, username: String, password: String) {
        disconnect()
        self.username = username
        self.password = password
        onState?(.connecting)

        var raw = urlString.trimmingCharacters(in: .whitespacesAndNewlines)
        if raw.hasPrefix("http://") { raw = "ws://" + raw.dropFirst(7) }
        if raw.hasPrefix("https://") { raw = "wss://" + raw.dropFirst(8) }
        guard let url = URL(string: raw) else {
            onState?(.failed("Bad broker URL"))
            return
        }

        let config = URLSessionConfiguration.default
        config.waitsForConnectivity = true
        let session = URLSession(configuration: config, delegate: self, delegateQueue: nil)
        self.session = session
        var request = URLRequest(url: url)
        // Mosquitto websockets MQTT subprotocol
        request.setValue("mqtt", forHTTPHeaderField: "Sec-WebSocket-Protocol")
        let task = session.webSocketTask(with: request)
        self.task = task
        task.resume()
        sendConnectPacket()
        listen()
        startPing()
    }

    func disconnect() {
        pingTimer?.invalidate()
        pingTimer = nil
        task?.cancel(with: .goingAway, reason: nil)
        task = nil
        session?.invalidateAndCancel()
        session = nil
        onState?(.disconnected)
    }

    func subscribe(_ topics: [String]) {
        pendingSubs = topics
        guard task != nil else { return }
        sendSubscribe(topics)
    }

    func publish(_ topic: String, payload: String, retain: Bool = false, qos: UInt8 = 0) {
        publish(topic, data: Data(payload.utf8), retain: retain, qos: qos)
    }

    func publish(_ topic: String, data: Data, retain: Bool = false, qos: UInt8 = 0) {
        guard let task else { return }
        var flags: UInt8 = 0x30 // PUBLISH
        if retain { flags |= 0x01 }
        if qos == 1 { flags |= 0x02 }
        var body = Data()
        body.append(mqttString(topic))
        if qos > 0 {
            body.append(contentsOf: [0x00, 0x01]) // packet id
        }
        body.append(data)
        let packet = mqttPacket(flags, body)
        task.send(.data(packet)) { [weak self] err in
            if let err {
                self?.onState?(.failed(err.localizedDescription))
            }
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
        payload.append(contentsOf: [0x00, 0x3C]) // keep alive 60s
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
                self?.onState?(.failed(err.localizedDescription))
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
        task?.send(.data(Data([0xC0, 0x00]))) { _ in }
    }

    private func startPing() {
        DispatchQueue.main.async {
            self.pingTimer?.invalidate()
            self.pingTimer = Timer.scheduledTimer(withTimeInterval: 30, repeats: true) { [weak self] _ in
                self?.sendPing()
            }
        }
    }

    private func listen() {
        task?.receive { [weak self] result in
            guard let self else { return }
            switch result {
            case .failure(let err):
                self.onState?(.failed(err.localizedDescription))
            case .success(let message):
                switch message {
                case .data(let data):
                    self.handle(data)
                case .string(let str):
                    self.handle(Data(str.utf8))
                @unknown default:
                    break
                }
                self.listen()
            }
        }
    }

    private func handle(_ data: Data) {
        var idx = 0
        while idx < data.count {
            let type = data[idx] >> 4
            guard let (remaining, lenSize) = decodeRemainingLength(data, start: idx + 1) else { return }
            let header = 1 + lenSize
            let start = idx + header
            let end = start + remaining
            guard end <= data.count else { return }
            let body = data.subdata(in: start..<end)
            switch type {
            case 2: // CONNACK
                let code = body.count >= 2 ? body[1] : 0xFF
                if code == 0 {
                    onState?(.connected)
                    if !pendingSubs.isEmpty {
                        sendSubscribe(pendingSubs)
                    }
                } else {
                    onState?(.failed("MQTT CONNACK \(code)"))
                }
            case 3: // PUBLISH
                parsePublish(flags: data[idx], body: body)
            case 9, 13: // SUBACK / PINGRESP
                break
            default:
                break
            }
            idx = end
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
        let payload = body.subdata(in: pos..<body.count)
        onMessage?(topic, payload)
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
        // CONNECT already sent after resume
    }

    func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask,
                    didCloseWith closeCode: URLSessionWebSocketTask.CloseCode, reason: Data?) {
        onState?(.disconnected)
    }
}

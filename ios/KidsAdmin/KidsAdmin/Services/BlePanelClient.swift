import Foundation
import CoreBluetooth
import Combine

/// Talks to the Kids Panel over BLE when MQTT is unavailable.
/// Protocol: write UTF-8 JSON chunks to the CMD characteristic; panel notifies STATUS.
final class BlePanelClient: NSObject, ObservableObject {
    static let serviceUUID = CBUUID(string: "6B1D0001-5F8A-4B2E-9C71-0A1B2C3D4E5F")
    static let cmdUUID = CBUUID(string: "6B1D0002-5F8A-4B2E-9C71-0A1B2C3D4E5F")
    static let statusUUID = CBUUID(string: "6B1D0003-5F8A-4B2E-9C71-0A1B2C3D4E5F")

    enum Phase: Equatable, Sendable {
        case idle
        case scanning
        case connecting
        case ready
        case failed(String)
    }

    @Published var phase: Phase = .idle
    @Published var panelName: String?

    var onConfigState: ((Data) -> Void)?
    var onAck: ((Bool, String) -> Void)?

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var cmdChar: CBCharacteristic?
    private var statusChar: CBCharacteristic?
    private var rxAssemble = Data()
    private var pendingWrites: [Data] = []
    private var writing = false
    private let chunkSize = 160

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    func startScan() {
        guard central.state == .poweredOn else {
            phase = .failed("Turn Bluetooth on")
            return
        }
        phase = .scanning
        panelName = nil
        central.scanForPeripherals(withServices: [Self.serviceUUID], options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: false,
        ])
        // Also scan by name in case the panel hasn't finished advertising the service UUID yet.
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [weak self] in
            guard let self, self.phase == .scanning else { return }
            self.central.scanForPeripherals(withServices: nil, options: nil)
        }
    }

    func stop() {
        central.stopScan()
        if let peripheral {
            central.cancelPeripheralConnection(peripheral)
        }
        peripheral = nil
        cmdChar = nil
        statusChar = nil
        pendingWrites.removeAll()
        writing = false
        phase = .idle
        panelName = nil
    }

    /// Send a kids/config/set style JSON object to the panel over BLE.
    func sendConfigSet(_ object: [String: Any]) {
        guard let data = try? JSONSerialization.data(withJSONObject: [
            "type": "config_set",
            "payload": object,
        ]) else { return }
        enqueueFramed(data)
    }

    func requestConfig() {
        guard let data = try? JSONSerialization.data(withJSONObject: ["type": "get_config"]) else { return }
        enqueueFramed(data)
    }

    private func enqueueFramed(_ payload: Data) {
        guard phase == .ready, cmdChar != nil else {
            phase = .failed("Panel not connected over Bluetooth")
            return
        }
        // Framing: !LEN:<n>\n then raw bytes in chunkSize pieces, then !END\n
        var frames: [Data] = []
        frames.append(Data("!LEN:\(payload.count)\n".utf8))
        var offset = 0
        while offset < payload.count {
            let end = min(offset + chunkSize, payload.count)
            frames.append(payload.subdata(in: offset..<end))
            offset = end
        }
        frames.append(Data("!END\n".utf8))
        pendingWrites.append(contentsOf: frames)
        pumpWrites()
    }

    private func pumpWrites() {
        guard !writing, let peripheral, let cmdChar else { return }
        guard !pendingWrites.isEmpty else { return }
        writing = true
        let next = pendingWrites.removeFirst()
        let withResponse = cmdChar.properties.contains(.write)
        peripheral.writeValue(next, for: cmdChar, type: withResponse ? .withResponse : .withoutResponse)
        if !withResponse {
            // withoutResponse doesn't callback — pace ourselves
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.03) { [weak self] in
                self?.writing = false
                self?.pumpWrites()
            }
        }
    }

    private func handleStatusChunk(_ data: Data) {
        // Status messages are small JSON objects; may arrive whole.
        if let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
           let type = obj["type"] as? String {
            if type == "config_state", let payload = obj["payload"] {
                if let pdata = try? JSONSerialization.data(withJSONObject: payload) {
                    onConfigState?(pdata)
                }
            } else if type == "ack" {
                let ok = (obj["ok"] as? Bool) ?? false
                let msg = (obj["message"] as? String) ?? ""
                onAck?(ok, msg)
            }
            return
        }
        // Assemble framed responses the same way as CMD if needed.
        rxAssemble.append(data)
        if let text = String(data: rxAssemble, encoding: .utf8), text.contains("!END\n") {
            if let start = text.range(of: "!LEN:"),
               let nl = text[start.upperBound...].firstIndex(of: "\n"),
               let len = Int(text[start.upperBound..<nl]) {
                let bodyStart = text.index(after: nl)
                if let endRange = text.range(of: "!END\n"),
                   bodyStart < endRange.lowerBound {
                    let body = String(text[bodyStart..<endRange.lowerBound])
                    if body.utf8.count >= len || true,
                       let bdata = body.data(using: .utf8) {
                        handleStatusChunk(bdata.prefix(len))
                    }
                }
            }
            rxAssemble.removeAll()
        }
    }
}

extension BlePanelClient: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state != .poweredOn, phase == .scanning || phase == .connecting || phase == .ready {
            phase = .failed("Bluetooth unavailable")
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = peripheral.name
            ?? (advertisementData[CBAdvertisementDataLocalNameKey] as? String)
            ?? ""
        let services = advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? []
        let match = name.localizedCaseInsensitiveContains("KidsPanel")
            || name.localizedCaseInsensitiveContains("kids-jobs")
            || services.contains(Self.serviceUUID)
        guard match else { return }
        central.stopScan()
        self.peripheral = peripheral
        panelName = name.isEmpty ? "Kids Panel" : name
        phase = .connecting
        peripheral.delegate = self
        central.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.discoverServices([Self.serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        phase = .failed(error?.localizedDescription ?? "BLE connect failed")
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        cmdChar = nil
        statusChar = nil
        if phase == .ready {
            phase = .failed(error?.localizedDescription ?? "Panel disconnected")
        }
    }
}

extension BlePanelClient: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error {
            phase = .failed(error.localizedDescription)
            return
        }
        guard let service = peripheral.services?.first(where: { $0.uuid == Self.serviceUUID }) else {
            phase = .failed("Kids Panel BLE service missing")
            return
        }
        peripheral.discoverCharacteristics([Self.cmdUUID, Self.statusUUID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error {
            phase = .failed(error.localizedDescription)
            return
        }
        for c in service.characteristics ?? [] {
            if c.uuid == Self.cmdUUID { cmdChar = c }
            if c.uuid == Self.statusUUID {
                statusChar = c
                peripheral.setNotifyValue(true, for: c)
            }
        }
        guard cmdChar != nil else {
            phase = .failed("BLE command characteristic missing")
            return
        }
        phase = .ready
        requestConfig()
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        writing = false
        if let error {
            phase = .failed(error.localizedDescription)
            return
        }
        pumpWrites()
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, let data = characteristic.value else { return }
        handleStatusChunk(data)
    }
}

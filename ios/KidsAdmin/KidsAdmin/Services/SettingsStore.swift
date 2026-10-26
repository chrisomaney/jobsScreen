import Foundation
import Combine

final class SettingsStore: ObservableObject {
    @Published var broker: String {
        didSet { save() }
    }
    @Published var username: String {
        didSet { save() }
    }
    @Published var password: String {
        didSet { save() }
    }
    @Published var remember: Bool {
        didSet { save() }
    }

    private let defaults = UserDefaults.standard

    init() {
        broker = defaults.string(forKey: "broker") ?? "ws://homeassistant.local:1884"
        username = defaults.string(forKey: "username") ?? ""
        password = defaults.string(forKey: "password") ?? ""
        remember = defaults.object(forKey: "remember") as? Bool ?? true
    }

    private func save() {
        guard remember else {
            defaults.removeObject(forKey: "broker")
            defaults.removeObject(forKey: "username")
            defaults.removeObject(forKey: "password")
            defaults.set(false, forKey: "remember")
            return
        }
        defaults.set(broker, forKey: "broker")
        defaults.set(username, forKey: "username")
        defaults.set(password, forKey: "password")
        defaults.set(true, forKey: "remember")
    }
}

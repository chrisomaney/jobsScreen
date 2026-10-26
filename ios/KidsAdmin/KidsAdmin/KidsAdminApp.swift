import SwiftUI

@main
struct KidsAdminApp: App {
    @StateObject private var settings = SettingsStore()
    @StateObject private var admin = AdminStore()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(settings)
                .environmentObject(admin)
                .onAppear {
                    admin.attach(settings: settings)
                    if settings.remember, !settings.broker.isEmpty {
                        admin.connect()
                    }
                }
        }
    }
}

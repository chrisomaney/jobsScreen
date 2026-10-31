import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var settings: SettingsStore
    @EnvironmentObject private var admin: AdminStore

    var body: some View {
        TabView {
            InternetView()
                .tabItem { Label("Internet", systemImage: "wifi") }
            PointsView()
                .tabItem { Label("Points", systemImage: "plus.forwardslash.minus") }
            BehaviourView()
                .tabItem { Label("Behaviour", systemImage: "star.bubble") }
            CatalogueView()
                .tabItem { Label("Jobs", systemImage: "checklist") }
            WebAdminView()
                .tabItem { Label("Website", systemImage: "safari") }
            BleJobsView()
                .tabItem { Label("Bluetooth", systemImage: "antenna.radiowaves.left.and.right") }
            ConnectView()
                .tabItem { Label("Connect", systemImage: "gearshape") }
        }
        .tint(Color(red: 0.13, green: 0.83, blue: 0.93))
        .preferredColorScheme(.dark)
        .keyboardDoneButton()
    }
}

#Preview {
    ContentView()
        .environmentObject(SettingsStore())
        .environmentObject(AdminStore())
}

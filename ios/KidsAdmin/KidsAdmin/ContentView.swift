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
            PinsView()
                .tabItem { Label("PINs", systemImage: "lock.fill") }
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
        .alert(
            unlockAlertTitle,
            isPresented: Binding(
                get: { admin.pendingUnlockSlug != nil },
                set: { if !$0 { admin.pendingUnlockSlug = nil } }
            )
        ) {
            Button("Approve") {
                if let slug = admin.pendingUnlockSlug {
                    admin.decideUnlock(slug: slug, approve: true)
                }
            }
            Button("Decline", role: .destructive) {
                if let slug = admin.pendingUnlockSlug {
                    admin.decideUnlock(slug: slug, approve: false)
                }
            }
            Button("Later", role: .cancel) {}
        } message: {
            Text("Required jobs look done. Approve internet for today, or decline if they skipped the work.")
        }
    }

    private var unlockAlertTitle: String {
        guard let slug = admin.pendingUnlockSlug,
              let name = AdminKid.all.first(where: { $0.slug == slug })?.name else {
            return "Internet unlock?"
        }
        return "\(name) wants internet"
    }
}

#Preview {
    ContentView()
        .environmentObject(SettingsStore())
        .environmentObject(AdminStore())
}

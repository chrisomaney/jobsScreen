import SwiftUI

struct BleJobsView: View {
    @EnvironmentObject private var admin: AdminStore
    @State private var note = ""

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    HStack {
                        Text(bleLabel)
                            .foregroundStyle(admin.bleReady ? .green : .secondary)
                        Spacer()
                        Circle()
                            .fill(admin.bleReady ? Color.green : Color.orange)
                            .frame(width: 10, height: 10)
                    }
                    if let name = admin.blePanelName {
                        Text("Found: \(name)").font(.footnote)
                    }
                    if let err = admin.bleError {
                        Text(err).font(.footnote).foregroundStyle(.pink)
                    }
                    if !note.isEmpty {
                        Text(note).font(.footnote).foregroundStyle(.secondary)
                    }
                } header: {
                    Text("Bluetooth panel")
                } footer: {
                    Text("Use this when MQTT won’t connect. Stand near the panel, scan, then save jobs from the Jobs tab — they go over BLE.")
                }

                Section {
                    if admin.bleReady {
                        Button("Disconnect Bluetooth", role: .destructive) {
                            admin.stopBle()
                            note = ""
                        }
                        Button("Refresh jobs from panel") {
                            admin.refreshBleConfig()
                            note = "Asked panel for current jobs…"
                        }
                    } else {
                        Button(admin.bleScanning ? "Scanning…" : "Scan for Kids Panel") {
                            note = "Looking for KidsPanel…"
                            admin.startBle()
                        }
                        .disabled(admin.bleScanning || admin.bleConnecting)
                    }
                }

                Section("Tip") {
                    Text("Flash the matching panel firmware so BLE is on. Keep the phone unlocked and Bluetooth enabled. MQTT and BLE can both be used; BLE is the fallback for setting jobs.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Bluetooth")
            .onChange(of: admin.bleReady) { _, ready in
                if ready {
                    note = "Connected — open Jobs and Save to panel"
                }
            }
        }
    }

    private var bleLabel: String {
        if admin.bleReady { return "Ready" }
        if admin.bleConnecting { return "Connecting…" }
        if admin.bleScanning { return "Scanning…" }
        if admin.bleError != nil { return "Error" }
        return "Not connected"
    }
}

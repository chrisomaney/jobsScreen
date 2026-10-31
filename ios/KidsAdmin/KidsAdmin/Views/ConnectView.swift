import SwiftUI

struct ConnectView: View {
    @EnvironmentObject private var settings: SettingsStore
    @EnvironmentObject private var admin: AdminStore

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    HStack {
                        Text(admin.connectionLabel)
                            .foregroundStyle(admin.connected ? .green : .secondary)
                        Spacer()
                        Circle()
                            .fill(admin.connected ? Color.green : Color.pink)
                            .frame(width: 10, height: 10)
                    }
                    if let err = admin.lastError {
                        Text(err).font(.footnote).foregroundStyle(.pink)
                    }
                    if admin.bleReady {
                        Text("Bluetooth backup ready")
                            .font(.footnote)
                            .foregroundStyle(.green)
                    }
                } header: {
                    Text("Status")
                }

                Section {
                    TextField("ws://homeassistant.local:1884", text: $settings.broker)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .keyboardType(.URL)
                        .submitLabel(.done)
                        .onSubmit { Keyboard.dismiss() }
                    TextField("Username", text: $settings.username)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .submitLabel(.next)
                    SecureField("Password", text: $settings.password)
                        .submitLabel(.done)
                        .onSubmit { Keyboard.dismiss() }
                    Toggle("Remember on this iPhone", isOn: $settings.remember)
                } header: {
                    Text("Mosquitto WebSockets")
                } footer: {
                    Text("Same login as Rewards HQ. Use wss:// if you need TLS. The app auto-reconnects if the broker drops. Local Network permission may be required for .local hosts.")
                }

                Section {
                    Toggle("Use bundled website pages", isOn: $settings.useBundledWebsite)
                    if !settings.useBundledWebsite {
                        TextField("http://homeassistant.local:8123/local/kids", text: $settings.websiteBase)
                            .textInputAutocapitalization(.never)
                            .autocorrectionDisabled()
                            .keyboardType(.URL)
                    }
                } header: {
                    Text("Website tab")
                } footer: {
                    Text("Bundled = Jobs Chooser + Rewards HQ shipped inside the app. Or point at your HA /local/kids folder.")
                }

                Section {
                    Button(admin.connected ? "Reconnect" : "Connect") {
                        Keyboard.dismiss()
                        admin.connect()
                    }
                    .disabled(settings.broker.trimmingCharacters(in: .whitespaces).isEmpty)
                    if admin.connected {
                        Button("Disconnect", role: .destructive) {
                            admin.disconnect()
                        }
                    }
                }

                Section("Install tip") {
                    Text("Open this project in Xcode on a Mac, select your iPhone, and tap Run. Allow Local Network + Bluetooth when asked.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .dismissKeyboardOnScroll()
            .navigationTitle("Connect")
        }
    }
}

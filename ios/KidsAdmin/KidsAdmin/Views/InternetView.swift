import SwiftUI

struct InternetView: View {
    @EnvironmentObject private var admin: AdminStore

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text("Force On / Off overrides jobs. Auto follows required jobs done on the panel.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                        .onTapGesture { Keyboard.dismiss() }
                }

                Section("Kids") {
                    ForEach(AdminKid.all) { kid in
                        let live = admin.status[kid.slug] ?? KidLiveStatus()
                        VStack(alignment: .leading, spacing: 10) {
                            HStack {
                                Circle().fill(kid.color).frame(width: 12, height: 12)
                                Text(kid.name).font(.headline)
                                Spacer()
                                Text(live.internetOn ? "ON" : "OFF")
                                    .font(.caption.weight(.bold))
                                    .padding(.horizontal, 10)
                                    .padding(.vertical, 4)
                                    .background(live.internetOn ? Color.green.opacity(0.25) : Color.pink.opacity(0.25))
                                    .clipShape(Capsule())
                            }
                            Text(statusLine(live))
                                .font(.caption)
                                .foregroundStyle(.secondary)
                            Picker("Override", selection: Binding(
                                get: { live.override },
                                set: { admin.setInternet(slug: kid.slug, mode: $0) }
                            )) {
                                ForEach(InternetMode.allCases) { mode in
                                    Text(mode.title).tag(mode)
                                }
                            }
                            .pickerStyle(.segmented)
                            .disabled(!admin.connected)
                        }
                        .padding(.vertical, 4)
                    }
                }

                Section("Common profile") {
                    HStack {
                        Text("kids/common/internet")
                        Spacer()
                        Text(admin.commonInternetOn ? "ON" : "OFF")
                            .font(.caption.weight(.bold))
                            .foregroundStyle(admin.commonInternetOn ? .green : .secondary)
                    }
                    HStack {
                        Button("Force On") { admin.setCommonInternet(on: true) }
                            .buttonStyle(.borderedProminent)
                            .tint(.green)
                        Button("Force Off") { admin.setCommonInternet(on: false) }
                            .buttonStyle(.borderedProminent)
                            .tint(.pink)
                    }
                    .disabled(!admin.connected)
                }
            }
            .dismissKeyboardOnScroll()
            .navigationTitle("Internet")
        }
    }

    private func statusLine(_ live: KidLiveStatus) -> String {
        let jobs = live.jobsReady ? "jobs ready" : "jobs not ready"
        return "Override \(live.override.title) · \(jobs) · \(live.pct)%"
    }
}

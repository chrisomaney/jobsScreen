import SwiftUI

struct BehaviourView: View {
    @EnvironmentObject private var admin: AdminStore
    @State private var kid = AdminKid.all[0].slug
    @State private var points = "1"
    @State private var title = "Great behaviour!"
    @State private var message = ""
    @State private var seconds = "15"
    @State private var panel = true
    @State private var tv = true
    @State private var banner: String?

    var body: some View {
        NavigationStack {
            Form {
                if let banner {
                    Section { Text(banner).foregroundStyle(.green) }
                }
                Section("Who") {
                    Picker("Kid", selection: $kid) {
                        ForEach(AdminKid.all) { k in
                            Text(k.name).tag(k.slug)
                        }
                    }
                    TextField("Points (+ / −)", text: $points)
                        .keyboardType(.numbersAndPunctuation)
                        .submitLabel(.done)
                        .onSubmit { Keyboard.dismiss() }
                }
                Section("Message") {
                    TextField("Title", text: $title)
                        .submitLabel(.next)
                    TextField("Message for panel & TV", text: $message, axis: .vertical)
                        .lineLimit(3...6)
                        .submitLabel(.done)
                        .onSubmit { Keyboard.dismiss() }
                    TextField("Seconds on panel", text: $seconds)
                        .keyboardType(.numberPad)
                    Toggle("Show on panel", isOn: $panel)
                    Toggle("Show on TV (celebrate)", isOn: $tv)
                }
                Section {
                    Button("Reward (+)") { send(sign: 1) }
                        .disabled(!admin.connected)
                    Button("Penalty (−)", role: .destructive) { send(sign: -1) }
                        .disabled(!admin.connected)
                }
            }
            .dismissKeyboardOnScroll()
            .navigationTitle("Behaviour")
        }
    }

    private func send(sign: Int) {
        Keyboard.dismiss()
        let n = abs(Int(points) ?? 0) * sign
        guard n != 0 else { banner = "Enter points"; return }
        let msg = message.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !msg.isEmpty else { banner = "Add a message"; return }
        admin.sendBehaviour(
            slug: kid,
            points: n,
            title: title.trimmingCharacters(in: .whitespacesAndNewlines),
            message: msg,
            panel: panel,
            tv: tv,
            seconds: max(3, Int(seconds) ?? 15)
        )
        banner = n > 0 ? "Reward sent" : "Penalty sent"
    }
}

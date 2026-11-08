import SwiftUI

struct PinsView: View {
    @EnvironmentObject private var admin: AdminStore
    @State private var drafts: [String: String] = [:]

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text("Each kid can have a 4-digit PIN. Empty means the panel opens with no pad. Parents set PINs here only.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                Section("Kids") {
                    ForEach(AdminKid.all) { kid in
                        PinRow(
                            name: kid.name,
                            value: Binding(
                                get: { drafts[kid.slug] ?? admin.pins[kid.slug] ?? "" },
                                set: { drafts[kid.slug] = Self.digitsOnly($0) }
                            ),
                            canSave: admin.canSaveJobs,
                            onSave: {
                                let pin = Self.digitsOnly(drafts[kid.slug] ?? admin.pins[kid.slug] ?? "")
                                admin.setPin(slug: kid.slug, pin: pin)
                                drafts[kid.slug] = pin
                            }
                        )
                    }
                }
            }
            .dismissKeyboardOnScroll()
            .navigationTitle("PINs")
            .onAppear {
                for kid in AdminKid.all {
                    if drafts[kid.slug] == nil {
                        drafts[kid.slug] = admin.pins[kid.slug] ?? ""
                    }
                }
            }
            .onChange(of: admin.pins) { _, newPins in
                for (slug, pin) in newPins {
                    drafts[slug] = pin
                }
            }
        }
    }

    private static func digitsOnly(_ s: String) -> String {
        String(s.filter(\.isNumber).prefix(4))
    }
}

private struct PinRow: View {
    let name: String
    @Binding var value: String
    let canSave: Bool
    let onSave: () -> Void

    var body: some View {
        HStack(spacing: 12) {
            Text(name)
                .frame(minWidth: 72, alignment: .leading)
            SecureField("····", text: $value)
                .keyboardType(.numberPad)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .multilineTextAlignment(.center)
                .frame(width: 88)
                .padding(.vertical, 6)
                .background(Color.secondary.opacity(0.12), in: RoundedRectangle(cornerRadius: 8))
            Button("Set", action: onSave)
                .buttonStyle(.borderedProminent)
                .disabled(!canSave || !(value.isEmpty || value.count == 4))
        }
    }
}

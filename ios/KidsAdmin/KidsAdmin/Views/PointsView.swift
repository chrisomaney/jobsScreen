import SwiftUI

struct PointsView: View {
    @EnvironmentObject private var admin: AdminStore
    @State private var amount = "1"

    var body: some View {
        NavigationStack {
            List {
                amountSection
                kidsSection
            }
            .dismissKeyboardOnScroll()
            .navigationTitle("Points")
        }
    }

    private var amountSection: some View {
        Section {
            TextField("Points amount", text: $amount)
                .keyboardType(.numberPad)
        } header: {
            Text("Amount")
        } footer: {
            Text("Uses + / − below. Tallies never go below 0 on the panel. Tap Done above the keyboard or drag the list to close it.")
        }
    }

    private var kidsSection: some View {
        Section("Kids") {
            ForEach(AdminKid.all) { kid in
                KidPointsRow(
                    kid: kid,
                    tally: admin.status[kid.slug]?.tally ?? 0,
                    connected: admin.connected,
                    onPlus: { adjust(kid.slug, sign: 1) },
                    onMinus: { adjust(kid.slug, sign: -1) }
                )
            }
        }
    }

    private func adjust(_ slug: String, sign: Int) {
        Keyboard.dismiss()
        let n = abs(Int(amount) ?? 1) * sign
        admin.adjustPoints(slug: slug, points: n)
    }
}

private struct KidPointsRow: View {
    let kid: KidInfo
    let tally: Int
    let connected: Bool
    let onPlus: () -> Void
    let onMinus: () -> Void

    var body: some View {
        HStack {
            Circle().fill(kid.color).frame(width: 12, height: 12)
            Text(kid.name)
            Spacer()
            Text("\(tally)")
                .font(.title3.weight(.bold))
                .monospacedDigit()
                .foregroundStyle(Color(red: 0.13, green: 0.83, blue: 0.93))
            Button(action: onPlus) {
                Image(systemName: "plus.circle.fill").font(.title2)
            }
            .tint(.green)
            .disabled(!connected)
            Button(action: onMinus) {
                Image(systemName: "minus.circle.fill").font(.title2)
            }
            .tint(.pink)
            .disabled(!connected)
        }
    }
}

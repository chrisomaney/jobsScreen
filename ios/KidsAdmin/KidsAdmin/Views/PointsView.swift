import SwiftUI

struct PointsView: View {
    @EnvironmentObject private var admin: AdminStore
    @State private var amount = "1"

    var body: some View {
        NavigationStack {
            List {
                Section {
                    TextField("Points amount", text: $amount)
                        .keyboardType(.numberPad)
                } header: {
                    Text("Amount")
                } footer: {
                    Text("Uses + / − below. Tallies never go below 0 on the panel.")
                }

                Section("Kids") {
                    ForEach(AdminKid.all) { kid in
                        let tally = admin.status[kid.slug]?.tally ?? 0
                        HStack {
                            Circle().fill(kid.color).frame(width: 12, height: 12)
                            Text(kid.name)
                            Spacer()
                            Text("\(tally)")
                                .font(.title3.weight(.bold))
                                .monospacedDigit()
                                .foregroundStyle(Color(red: 0.13, green: 0.83, blue: 0.93))
                            Button {
                                admin.adjustPoints(slug: kid.slug, points: abs(Int(amount) ?? 1))
                            } label: {
                                Image(systemName: "plus.circle.fill").font(.title2)
                            }
                            .tint(.green)
                            .disabled(!admin.connected)
                            Button {
                                admin.adjustPoints(slug: kid.slug, points: -abs(Int(amount) ?? 1))
                            } label: {
                                Image(systemName: "minus.circle.fill").font(.title2)
                            }
                            .tint(.pink)
                            .disabled(!admin.connected)
                        }
                    }
                }
            }
            .navigationTitle("Points")
        }
    }
}

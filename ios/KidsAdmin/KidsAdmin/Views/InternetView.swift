import SwiftUI

struct InternetView: View {
    @EnvironmentObject private var admin: AdminStore
    @State private var editing: InternetSchedule?

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text("Force On / Off overrides jobs. Auto follows required jobs. Schedules turn internet off for chosen kids between Off→On times (Force On still wins for emergencies).")
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
                                if live.scheduledOff {
                                    Text("BEDTIME")
                                        .font(.caption2.weight(.bold))
                                        .padding(.horizontal, 8)
                                        .padding(.vertical, 3)
                                        .background(Color.orange.opacity(0.3))
                                        .clipShape(Capsule())
                                }
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

                Section {
                    if admin.internetSchedules.isEmpty {
                        Text("No schedules yet — add a bedtime or homework block.")
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                    ForEach(admin.internetSchedules) { sched in
                        Button {
                            editing = sched
                        } label: {
                            VStack(alignment: .leading, spacing: 4) {
                                HStack {
                                    Text(sched.label).foregroundStyle(.primary)
                                    Spacer()
                                    Text(sched.enabled ? "On" : "Off")
                                        .font(.caption.weight(.bold))
                                        .foregroundStyle(sched.enabled ? .green : .secondary)
                                }
                                Text(sched.summary)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                    .onDelete { idx in
                        for i in idx {
                            admin.removeInternetSchedule(id: admin.internetSchedules[i].id)
                        }
                    }
                    Button {
                        editing = InternetSchedule(
                            id: "sched_\(Int(Date().timeIntervalSince1970))",
                            label: "Bedtime",
                            kids: AdminKid.all.map(\.slug),
                            days: 0x7F,
                            offHour: 20,
                            offMinute: 0,
                            onHour: 7,
                            onMinute: 0,
                            enabled: true
                        )
                    } label: {
                        Label("Add internet-off schedule", systemImage: "plus.circle")
                    }
                    .disabled(!admin.connected)
                } header: {
                    Text("Schedules")
                } footer: {
                    Text("Example: Off 20:00 → On 07:00 for school nights. Overnight windows are supported.")
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
            .sheet(item: $editing) { sched in
                InternetScheduleEditor(
                    schedule: sched,
                    onSave: { saved in
                        admin.upsertInternetSchedule(saved)
                        editing = nil
                    },
                    onCancel: { editing = nil }
                )
            }
        }
    }

    private func statusLine(_ live: KidLiveStatus) -> String {
        var parts = ["Override \(live.override.title)"]
        parts.append(live.jobsReady ? "jobs ready" : "jobs not ready")
        if live.scheduledOff { parts.append("schedule off") }
        parts.append("\(live.pct)%")
        return parts.joined(separator: " · ")
    }
}

struct InternetScheduleEditor: View {
    @State var schedule: InternetSchedule
    var onSave: (InternetSchedule) -> Void
    var onCancel: () -> Void

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Name", text: $schedule.label)
                    Toggle("Enabled", isOn: $schedule.enabled)
                }
                Section("Kids") {
                    ForEach(AdminKid.all) { kid in
                        Toggle(kid.name, isOn: Binding(
                            get: { schedule.kids.contains(kid.slug) },
                            set: { on in
                                if on {
                                    if !schedule.kids.contains(kid.slug) { schedule.kids.append(kid.slug) }
                                } else {
                                    schedule.kids.removeAll { $0 == kid.slug }
                                }
                            }
                        ))
                    }
                }
                Section("Days") {
                    HStack {
                        ForEach(0..<7, id: \.self) { i in
                            Button(InternetSchedule.dayLetters[i]) {
                                schedule.setDay(i, on: !schedule.dayOn(i))
                            }
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 8)
                            .background(schedule.dayOn(i) ? Color.cyan.opacity(0.35) : Color.secondary.opacity(0.15))
                            .clipShape(RoundedRectangle(cornerRadius: 8))
                            .foregroundStyle(.primary)
                        }
                    }
                }
                Section("Internet off from") {
                    Stepper("Hour \(schedule.offHour)", value: $schedule.offHour, in: 0...23)
                    Stepper("Minute \(schedule.offMinute)", value: $schedule.offMinute, in: 0...59, step: 5)
                }
                Section("Back on at") {
                    Stepper("Hour \(schedule.onHour)", value: $schedule.onHour, in: 0...23)
                    Stepper("Minute \(schedule.onMinute)", value: $schedule.onMinute, in: 0...59, step: 5)
                } footer: {
                    Text("Uses the panel’s local clock (NTP). Force On still unlocks a kid during the window.")
                }
            }
            .navigationTitle("Schedule")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        if schedule.label.trimmingCharacters(in: .whitespaces).isEmpty {
                            schedule.label = "Internet off"
                        }
                        onSave(schedule)
                    }
                    .disabled(schedule.kids.isEmpty)
                }
            }
        }
    }
}

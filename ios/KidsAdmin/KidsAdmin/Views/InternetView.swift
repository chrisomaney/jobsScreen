import SwiftUI

struct InternetView: View {
    @EnvironmentObject private var admin: AdminStore
    @State private var editing: InternetSchedule?

    var body: some View {
        NavigationStack {
            List {
                introSection
                kidsSection
                schedulesSection
                commonSection
            }
            .dismissKeyboardOnScroll()
            .navigationTitle("Internet")
            .sheet(item: $editing, content: scheduleEditor)
        }
    }

    private var introSection: some View {
        Section {
            Text("Force On / Off overrides jobs. Auto follows required jobs. Schedules turn internet off for chosen kids between Off→On times (Force On still wins for emergencies).")
                .font(.footnote)
                .foregroundStyle(.secondary)
                .onTapGesture { Keyboard.dismiss() }
        }
    }

    private var kidsSection: some View {
        Section("Kids") {
            ForEach(AdminKid.all) { kid in
                KidInternetRow(
                    kid: kid,
                    live: admin.status[kid.slug] ?? KidLiveStatus(),
                    connected: admin.connected,
                    onModeChange: { mode in
                        admin.setInternet(slug: kid.slug, mode: mode)
                    }
                )
            }
        }
    }

    private var schedulesSection: some View {
        Section {
            if admin.internetSchedules.isEmpty {
                Text("No schedules yet — add a bedtime or homework block.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            ForEach(admin.internetSchedules) { sched in
                ScheduleRowButton(schedule: sched) {
                    editing = sched
                }
            }
            .onDelete(perform: deleteSchedules)
            Button(action: addBedtimeSchedule) {
                Label("Add internet-off schedule", systemImage: "plus.circle")
            }
            .disabled(!admin.connected)
        } header: {
            Text("Schedules")
        } footer: {
            Text("Example: Off 20:00 → On 07:00 for school nights. Overnight windows are supported.")
        }
    }

    private var commonSection: some View {
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

    private func scheduleEditor(_ sched: InternetSchedule) -> some View {
        InternetScheduleEditor(
            schedule: sched,
            onSave: { saved in
                admin.upsertInternetSchedule(saved)
                editing = nil
            },
            onCancel: { editing = nil }
        )
    }

    private func deleteSchedules(at offsets: IndexSet) {
        for i in offsets {
            admin.removeInternetSchedule(id: admin.internetSchedules[i].id)
        }
    }

    private func addBedtimeSchedule() {
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
    }
}

private struct KidInternetRow: View {
    let kid: KidInfo
    let live: KidLiveStatus
    let connected: Bool
    let onModeChange: (InternetMode) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            header
            Text(statusLine)
                .font(.caption)
                .foregroundStyle(.secondary)
            Picker("Override", selection: modeBinding) {
                ForEach(InternetMode.allCases) { mode in
                    Text(mode.title).tag(mode)
                }
            }
            .pickerStyle(.segmented)
            .disabled(!connected)
        }
        .padding(.vertical, 4)
    }

    private var header: some View {
        HStack {
            Circle().fill(kid.color).frame(width: 12, height: 12)
            Text(kid.name).font(.headline)
            Spacer()
            if live.scheduledOff {
                bedtimeBadge
            }
            onOffBadge
        }
    }

    private var bedtimeBadge: some View {
        Text("BEDTIME")
            .font(.caption2.weight(.bold))
            .padding(.horizontal, 8)
            .padding(.vertical, 3)
            .background(Color.orange.opacity(0.3))
            .clipShape(Capsule())
    }

    private var onOffBadge: some View {
        Text(live.internetOn ? "ON" : "OFF")
            .font(.caption.weight(.bold))
            .padding(.horizontal, 10)
            .padding(.vertical, 4)
            .background(live.internetOn ? Color.green.opacity(0.25) : Color.pink.opacity(0.25))
            .clipShape(Capsule())
    }

    private var modeBinding: Binding<InternetMode> {
        Binding(
            get: { live.override },
            set: { onModeChange($0) }
        )
    }

    private var statusLine: String {
        var parts = ["Override \(live.override.title)"]
        parts.append(live.jobsReady ? "jobs ready" : "jobs not ready")
        if live.scheduledOff { parts.append("schedule off") }
        parts.append("\(live.pct)%")
        return parts.joined(separator: " · ")
    }
}

private struct ScheduleRowButton: View {
    let schedule: InternetSchedule
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text(schedule.label).foregroundStyle(.primary)
                    Spacer()
                    Text(schedule.enabled ? "On" : "Off")
                        .font(.caption.weight(.bold))
                        .foregroundStyle(schedule.enabled ? .green : .secondary)
                }
                Text(schedule.summary)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }
}

struct InternetScheduleEditor: View {
    @State var schedule: InternetSchedule
    var onSave: (InternetSchedule) -> Void
    var onCancel: () -> Void

    var body: some View {
        NavigationStack {
            Form {
                basicsSection
                kidsSection
                daysSection
                offTimeSection
                onTimeSection
            }
            .navigationTitle("Schedule")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save", action: save)
                        .disabled(schedule.kids.isEmpty)
                }
            }
        }
    }

    private var basicsSection: some View {
        Section {
            TextField("Name", text: $schedule.label)
            Toggle("Enabled", isOn: $schedule.enabled)
        }
    }

    private var kidsSection: some View {
        Section("Kids") {
            ForEach(AdminKid.all) { kid in
                Toggle(kid.name, isOn: kidBinding(kid.slug))
            }
        }
    }

    private var daysSection: some View {
        Section("Days") {
            HStack {
                ForEach(0..<7, id: \.self) { i in
                    DayToggleButton(
                        letter: InternetSchedule.dayLetters[i],
                        isOn: schedule.dayOn(i),
                        action: { schedule.setDay(i, on: !schedule.dayOn(i)) }
                    )
                }
            }
        }
    }

    private var offTimeSection: some View {
        Section("Internet off from") {
            Stepper("Hour \(schedule.offHour)", value: $schedule.offHour, in: 0...23)
            Stepper("Minute \(schedule.offMinute)", value: $schedule.offMinute, in: 0...59, step: 5)
        }
    }

    private var onTimeSection: some View {
        Section {
            Stepper("Hour \(schedule.onHour)", value: $schedule.onHour, in: 0...23)
            Stepper("Minute \(schedule.onMinute)", value: $schedule.onMinute, in: 0...59, step: 5)
        } header: {
            Text("Back on at")
        } footer: {
            Text("Uses the panel’s local clock (NTP). Force On still unlocks a kid during the window.")
        }
    }

    private func kidBinding(_ slug: String) -> Binding<Bool> {
        Binding(
            get: { schedule.kids.contains(slug) },
            set: { on in
                if on {
                    if !schedule.kids.contains(slug) { schedule.kids.append(slug) }
                } else {
                    schedule.kids.removeAll { $0 == slug }
                }
            }
        )
    }

    private func save() {
        if schedule.label.trimmingCharacters(in: .whitespaces).isEmpty {
            schedule.label = "Internet off"
        }
        onSave(schedule)
    }
}

private struct DayToggleButton: View {
    let letter: String
    let isOn: Bool
    let action: () -> Void

    var body: some View {
        Button(letter, action: action)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 8)
            .background(isOn ? Color.cyan.opacity(0.35) : Color.secondary.opacity(0.15))
            .clipShape(RoundedRectangle(cornerRadius: 8))
            .foregroundStyle(.primary)
    }
}

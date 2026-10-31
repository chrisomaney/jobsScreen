import SwiftUI

struct CatalogueView: View {
    @EnvironmentObject private var admin: AdminStore
    @State private var newLabel = ""
    @State private var newDetail = ""
    @State private var newRequired = true
    @State private var newPts = "1"
    @State private var addCat = "expectation"

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Stepper("Expectations bundle: \(admin.expBundle) pts", value: $admin.expBundle, in: 0...100)
                    Button("Save points to panel") {
                        Keyboard.dismiss()
                        admin.savePoints()
                    }
                        .disabled(!admin.connected) // points still need MQTT / Rewards engine
                } header: {
                    Text("Bundle")
                } footer: {
                    Text("Kids earn this only when all assigned expectations are done (with a job_sum schedule). Drag or tap Done to close the keyboard.")
                }

                Section {
                    Picker("Type", selection: $addCat) {
                        Text("Expectation").tag("expectation")
                        Text("Paid job").tag("paid")
                    }
                    .pickerStyle(.segmented)
                    TextField("Name", text: $newLabel)
                        .submitLabel(.next)
                    TextField("Hint (optional)", text: $newDetail)
                        .submitLabel(.done)
                        .onSubmit { Keyboard.dismiss() }
                    Toggle("Needed for internet", isOn: $newRequired)
                    if addCat == "paid" {
                        TextField("Points", text: $newPts).keyboardType(.numberPad)
                    }
                    Button("Add to panel") {
                        Keyboard.dismiss()
                        let label = newLabel.trimmingCharacters(in: .whitespaces)
                        guard !label.isEmpty else { return }
                        admin.addItem(
                            label: label,
                            cat: addCat,
                            detail: newDetail.trimmingCharacters(in: .whitespaces),
                            required: newRequired,
                            points: addCat == "paid" ? max(0, Int(newPts) ?? 0) : nil
                        )
                        newLabel = ""
                        newDetail = ""
                        newPts = "1"
                        newRequired = true
                    }
                    .disabled(!admin.canSaveJobs || (admin.customSlotsFree ?? 1) == 0)
                    if let free = admin.customSlotsFree {
                        Text(free == 0 ? "No free custom slots" : "\(free) custom slot(s) free")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                } header: {
                    Text("Add")
                }

                Section("Expectations") {
                    ForEach(admin.jobs.filter { $0.cat != "paid" }) { job in
                        NavigationLink(job.label) {
                            JobEditView(job: job)
                        }
                    }
                }

                Section("Jobs (paid)") {
                    ForEach(admin.jobs.filter { $0.cat == "paid" }) { job in
                        HStack {
                            NavigationLink(job.label) {
                                JobEditView(job: job)
                            }
                            Spacer()
                            TextField(
                                "pts",
                                value: Binding(
                                    get: { admin.jobPoints[job.id] ?? 1 },
                                    set: { admin.jobPoints[job.id] = max(0, $0) }
                                ),
                                format: .number
                            )
                            .keyboardType(.numberPad)
                            .multilineTextAlignment(.trailing)
                            .frame(width: 52)
                        }
                    }
                }
            }
            .dismissKeyboardOnScroll()
            .navigationTitle("Jobs")
        }
    }
}

struct JobEditView: View {
    @EnvironmentObject private var admin: AdminStore
    @State var job: JobItem
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        Form {
            Section {
                TextField("Name", text: $job.label)
                    .submitLabel(.next)
                TextField("Hint", text: $job.detail)
                    .submitLabel(.done)
                    .onSubmit { Keyboard.dismiss() }
                Picker("Type", selection: $job.cat) {
                    Text("Expectation").tag("expectation")
                    Text("Paid job").tag("paid")
                }
                Toggle("Needed for internet", isOn: $job.required)
            }
            Section("Who does this") {
                ForEach(AdminKid.all) { kid in
                    Toggle(kid.name, isOn: Binding(
                        get: { job.kids.contains(kid.slug) },
                        set: { on in
                            if on {
                                if !job.kids.contains(kid.slug) { job.kids.append(kid.slug) }
                            } else {
                                job.kids.removeAll { $0 == kid.slug }
                            }
                        }
                    ))
                }
            }
            Section {
                Button("Save to panel") {
                    Keyboard.dismiss()
                    admin.saveJob(job)
                    if job.cat != "paid" { admin.jobPoints[job.id] = 0 }
                    if admin.connected { admin.savePoints() }
                    dismiss()
                }
                .disabled(!admin.canSaveJobs)
                if job.custom {
                    Button("Remove from panel", role: .destructive) {
                        Keyboard.dismiss()
                        admin.removeJob(id: job.id)
                        dismiss()
                    }
                    .disabled(!admin.canSaveJobs)
                }
            }
        }
        .dismissKeyboardOnScroll()
        .keyboardDoneButton()
        .navigationTitle("Edit")
    }
}

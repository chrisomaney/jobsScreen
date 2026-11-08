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
                bundleSection
                addSection
                expectationsSection
                paidJobsSection
            }
            .dismissKeyboardOnScroll()
            .navigationTitle("Jobs")
        }
    }

    private var bundleSection: some View {
        Section {
            Stepper("Expectations bundle: \(admin.expBundle) pts", value: $admin.expBundle, in: 0...100)
            Button("Save points to panel") {
                Keyboard.dismiss()
                admin.savePoints()
            }
            .disabled(!admin.connected)
        } header: {
            Text("Bundle")
        } footer: {
            Text("Kids earn this only when all assigned expectations are done (with a job_sum schedule). Drag or tap Done to close the keyboard.")
        }
    }

    private var addSection: some View {
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
            Button("Add to panel", action: addItem)
                .disabled(!admin.canSaveJobs || (admin.customSlotsFree ?? 1) == 0)
            if let free = admin.customSlotsFree {
                Text(free == 0 ? "No free custom slots" : "\(free) custom slot(s) free")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        } header: {
            Text("Add")
        }
    }

    private var expectationsSection: some View {
        Section("Expectations") {
            ForEach(expectationJobs) { job in
                NavigationLink(job.label) {
                    JobEditView(job: job)
                }
            }
        }
    }

    private var paidJobsSection: some View {
        Section("Jobs (paid)") {
            ForEach(paidJobs) { job in
                PaidJobPointsRow(job: job)
            }
        }
    }

    private var expectationJobs: [JobItem] {
        admin.jobs.filter { $0.cat != "paid" }
    }

    private var paidJobs: [JobItem] {
        admin.jobs.filter { $0.cat == "paid" }
    }

    private func addItem() {
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
}

private struct PaidJobPointsRow: View {
    @EnvironmentObject private var admin: AdminStore
    let job: JobItem

    var body: some View {
        HStack {
            NavigationLink(job.label) {
                JobEditView(job: job)
            }
            Spacer()
            TextField("pts", value: pointsBinding, format: .number)
                .keyboardType(.numberPad)
                .multilineTextAlignment(.trailing)
                .frame(width: 52)
        }
    }

    private var pointsBinding: Binding<Int> {
        Binding(
            get: { admin.jobPoints[job.id] ?? 1 },
            set: { admin.jobPoints[job.id] = max(0, $0) }
        )
    }
}

struct JobEditView: View {
    @EnvironmentObject private var admin: AdminStore
    @State var job: JobItem
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        Form {
            detailsSection
            kidsSection
            actionsSection
        }
        .dismissKeyboardOnScroll()
        .keyboardDoneButton()
        .navigationTitle("Edit")
    }

    private var detailsSection: some View {
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
    }

    private var kidsSection: some View {
        Section("Who does this") {
            ForEach(AdminKid.all) { kid in
                Toggle(kid.name, isOn: kidBinding(kid.slug))
            }
        }
    }

    private var actionsSection: some View {
        Section {
            Button("Save to panel", action: save)
                .disabled(!admin.canSaveJobs)
            if job.custom {
                Button("Remove from panel", role: .destructive, action: remove)
                    .disabled(!admin.canSaveJobs)
            }
        }
    }

    private func kidBinding(_ slug: String) -> Binding<Bool> {
        Binding(
            get: { job.kids.contains(slug) },
            set: { on in
                if on {
                    if !job.kids.contains(slug) { job.kids.append(slug) }
                } else {
                    job.kids.removeAll { $0 == slug }
                }
            }
        )
    }

    private func save() {
        Keyboard.dismiss()
        admin.saveJob(job)
        if job.cat != "paid" { admin.jobPoints[job.id] = 0 }
        if admin.connected { admin.savePoints() }
        dismiss()
    }

    private func remove() {
        Keyboard.dismiss()
        admin.removeJob(id: job.id)
        dismiss()
    }
}

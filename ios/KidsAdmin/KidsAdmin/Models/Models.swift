import Foundation
import SwiftUI

struct KidInfo: Identifiable, Hashable {
    let slug: String
    let name: String
    let color: Color

    var id: String { slug }
}

enum InternetMode: String, CaseIterable, Identifiable {
    case auto
    case on
    case off

    var id: String { rawValue }

    var title: String {
        switch self {
        case .auto: return "Auto"
        case .on: return "On"
        case .off: return "Off"
        }
    }
}

struct KidLiveStatus: Equatable {
    var tally: Int = 0
    var internetOn: Bool = false
    var jobsReady: Bool = false
    var override: InternetMode = .auto
    var pct: Int = 0
}

struct JobItem: Identifiable, Equatable {
    var id: String
    var label: String
    var cat: String
    var required: Bool
    var custom: Bool
    var detail: String
    var kids: [String]
}

enum AdminKid {
    static let all: [KidInfo] = [
        .init(slug: "xavier", name: "Xavier", color: Color(red: 0.23, green: 0.51, blue: 0.96)),
        .init(slug: "presley", name: "Presley", color: Color(red: 0.93, green: 0.28, blue: 0.60)),
        .init(slug: "jax", name: "Jax", color: Color(red: 0.96, green: 0.62, blue: 0.04)),
        .init(slug: "banx", name: "Banx", color: Color(red: 0.13, green: 0.77, blue: 0.37)),
        .init(slug: "hazel", name: "Hazel", color: Color(red: 0.66, green: 0.33, blue: 0.97)),
    ]
}

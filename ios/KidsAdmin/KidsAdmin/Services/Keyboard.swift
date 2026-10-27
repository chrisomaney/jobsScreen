import SwiftUI
import UIKit

enum Keyboard {
    static func dismiss() {
        UIApplication.shared.sendAction(
            #selector(UIResponder.resignFirstResponder),
            to: nil, from: nil, for: nil
        )
    }
}

extension View {
    /// Number pads have no Return key — show a Done button above the keyboard.
    func keyboardDoneButton() -> some View {
        toolbar {
            ToolbarItemGroup(placement: .keyboard) {
                Spacer()
                Button("Done") {
                    Keyboard.dismiss()
                }
                .fontWeight(.semibold)
            }
        }
    }

    /// Drag/scroll the form to put the keyboard away.
    func dismissKeyboardOnScroll() -> some View {
        scrollDismissesKeyboard(.interactively)
    }
}

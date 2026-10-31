import SwiftUI
import WebKit

/// Embeds the Jobs Chooser / Rewards HQ websites inside the app.
struct WebAdminView: View {
    enum Page: String, CaseIterable, Identifiable {
        case chooser = "Jobs Chooser"
        case rewards = "Rewards HQ"
        var id: String { rawValue }

        var bundledFile: String {
            switch self {
            case .chooser: return "index"
            case .rewards: return "rewards-admin"
            }
        }

        var remotePath: String {
            switch self {
            case .chooser: return "index.html"
            case .rewards: return "rewards-admin.html"
            }
        }
    }

    @EnvironmentObject private var settings: SettingsStore
    @State private var page: Page = .chooser
    @State private var reloadToken = UUID()

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                Picker("Page", selection: $page) {
                    ForEach(Page.allCases) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)
                .padding(.horizontal)
                .padding(.vertical, 8)

                WebAdminRepresentable(
                    page: page,
                    settings: settings,
                    reloadToken: reloadToken
                )
                .id("\(page.rawValue)-\(reloadToken)")
            }
            .background(Color.black)
            .navigationTitle("Website")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button {
                        reloadToken = UUID()
                    } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                    .accessibilityLabel("Reload")
                }
            }
        }
    }
}

struct WebAdminRepresentable: UIViewRepresentable {
    let page: WebAdminView.Page
    let settings: SettingsStore
    let reloadToken: UUID

    func makeCoordinator() -> Coordinator { Coordinator(settings: settings) }

    func makeUIView(context: Context) -> WKWebView {
        let config = WKWebViewConfiguration()
        config.defaultWebpagePreferences.allowsContentJavaScript = true
        config.websiteDataStore = .default()
        let web = WKWebView(frame: .zero, configuration: config)
        web.navigationDelegate = context.coordinator
        web.isOpaque = false
        web.backgroundColor = .black
        web.scrollView.backgroundColor = .black
        context.coordinator.webView = web
        load(into: web, coordinator: context.coordinator)
        return web
    }

    func updateUIView(_ webView: WKWebView, context: Context) {
        // Reload is driven by .id change from parent.
    }

    private func load(into web: WKWebView, coordinator: Coordinator) {
        coordinator.pendingBroker = settings.broker
        coordinator.pendingUser = settings.username
        coordinator.pendingPass = settings.password

        if settings.useBundledWebsite {
            let url = Bundle.main.url(forResource: page.bundledFile, withExtension: "html", subdirectory: "Web")
                ?? Bundle.main.url(forResource: page.bundledFile, withExtension: "html")
            if let url {
                web.loadFileURL(url, allowingReadAccessTo: url.deletingLastPathComponent())
                return
            }
        }

        let base = settings.websiteBase.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        if let url = URL(string: "\(base)/\(page.remotePath)") {
            web.load(URLRequest(url: url))
            return
        }

        web.loadHTMLString(
            "<html><body style='background:#111;color:#fff;font-family:-apple-system;padding:24px'><h2>Set a website URL</h2><p>In Connect, enter your HA /local path or turn on bundled pages.</p></body></html>",
            baseURL: nil
        )
    }

    final class Coordinator: NSObject, WKNavigationDelegate {
        let settings: SettingsStore
        weak var webView: WKWebView?
        var pendingBroker = ""
        var pendingUser = ""
        var pendingPass = ""

        init(settings: SettingsStore) {
            self.settings = settings
        }

        func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
            injectCredentials(into: webView)
        }

        private func injectCredentials(into webView: WKWebView) {
            // Prefill the same localStorage keys the web apps already use.
            let broker = pendingBroker.replacingOccurrences(of: "\\", with: "\\\\")
                .replacingOccurrences(of: "'", with: "\\'")
            let user = pendingUser.replacingOccurrences(of: "\\", with: "\\\\")
                .replacingOccurrences(of: "'", with: "\\'")
            let pass = pendingPass.replacingOccurrences(of: "\\", with: "\\\\")
                .replacingOccurrences(of: "'", with: "\\'")
            let js = """
            (function(){
              try {
                var chooser = {url:'\(broker)', user:'\(user)', pass:'\(pass)'};
                localStorage.setItem('jobsChooser.conn', JSON.stringify(chooser));
                localStorage.setItem('rewardsAdmin', JSON.stringify({url:'\(broker)', username:'\(user)', password:'\(pass)', remember:true}));
                var urlEl = document.getElementById('url') || document.getElementById('broker');
                var userEl = document.getElementById('user');
                var passEl = document.getElementById('pass');
                if (urlEl) urlEl.value = '\(broker)';
                if (userEl) userEl.value = '\(user)';
                if (passEl) passEl.value = '\(pass)';
              } catch (e) {}
            })();
            """
            webView.evaluateJavaScript(js, completionHandler: nil)
        }
    }
}

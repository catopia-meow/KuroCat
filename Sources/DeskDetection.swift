import AppKit
import ApplicationServices

final class DeskDetection {
    private let queue = DispatchQueue(label: "KuroCat.desk-detection", qos: .utility)
    private var requestInFlight = false

    func refresh(_ completion: @escaping (Bool) -> Void) {
        guard !requestInFlight, let application = NSWorkspace.shared.frontmostApplication,
              application.processIdentifier != ProcessInfo.processInfo.processIdentifier else { return }
        let name = (application.localizedName ?? "").lowercased()
        let id = (application.bundleIdentifier ?? "").lowercased()
        let names = ["blender", "photoshop", "krita", "clip studio", "zbrush", "3dcoat", "mudbox",
                     "substance", "affinity", "gimp", "sculptris", "paint"]
        let artist = names.contains { name.contains($0) || id.contains($0.replacingOccurrences(of: " ", with: "")) }
        guard artist else { completion(false); return }
        let pid = application.processIdentifier
        requestInFlight = true
        queue.async {
            let editing = Self.hasTextFocus(pid)
            DispatchQueue.main.async {
                self.requestInFlight = false
                if NSWorkspace.shared.frontmostApplication?.processIdentifier == pid { completion(!editing) }
            }
        }
    }

    private static func hasTextFocus(_ pid: pid_t) -> Bool {
        guard AXIsProcessTrusted() else { return false }
        let app = AXUIElementCreateApplication(pid)
        AXUIElementSetMessagingTimeout(app, 0.25)
        var item: CFTypeRef?
        guard AXUIElementCopyAttributeValue(app, kAXFocusedUIElementAttribute as CFString, &item) == .success,
              let item, CFGetTypeID(item) == AXUIElementGetTypeID() else { return false }
        let element = unsafeBitCast(item, to: AXUIElement.self)
        var role: CFTypeRef?
        guard AXUIElementCopyAttributeValue(element, kAXRoleAttribute as CFString, &role) == .success,
              let role = role as? String else { return false }
        return ["AXTextField", "AXTextArea", "AXComboBox", "AXSearchField"].contains(role)
    }
}

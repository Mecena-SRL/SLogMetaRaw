// SPDX-License-Identifier: GPL-3.0-or-later
// The wrapper application of the FxPlug: Final Cut Pro finds the effect inside it. Opening it once registers
// the plug-in; it shows nothing (LSUIElement) and quits at once.
import AppKit

let app = NSApplication.shared
app.setActivationPolicy(.accessory)
DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { NSApp.terminate(nil) }
app.run()

// SPDX-License-Identifier: GPL-3.0-or-later
// The camera values of the clip the effect is on. An FxPlug cannot see the clip's file, but it knows the clip's source
// start time (its timecode), duration and frame duration: the index written by slogmetaraw.fcp_catalog maps those to
// the as-shot values the Resolve node reads from the file. Same matching rule as fcp_catalog.match().
import Foundation
import os

private let catLog = Logger(subsystem: "com.slogmetaraw.fcp", category: "catalog")

struct CameraValues {
    let camera: String
    let ei: Double, kelvin: Double, tint: Double
    let space: Int32, gamma: Int32
}

final class MetadataCatalog {
    static let shared = MetadataCatalog()

    private let support = (NSHomeDirectory() as NSString).appendingPathComponent("Library/Application Support/Log MetaRaw FCP")
    private var catalogPath: String { (support as NSString).appendingPathComponent("catalog.json") }
    private let lock = NSLock()
    private var files: [String: [String: Any]] = [:]
    private var loadedDate: Date?
    private var refreshing = false
    private var lastRefresh = Date.distantPast

    /// The values of the clip whose source starts at `start` and lasts `duration` (seconds), nil when not indexed.
    func lookup(start: Double, duration: Double, frame: Double, media: Double?) -> CameraValues? {
        reloadIfChanged()
        lock.lock(); defer { lock.unlock() }
        guard frame > 0 else { return nil }
        let fps = 1.0 / frame
        let same = files.filter { ($0.value["fps"] as? Double).map { abs($0 - fps) < 0.01 } ?? false }
        let exact = same.filter {
            guard let s = $0.value["start"] as? Double, let d = $0.value["duration"] as? Double else { return false }
            return abs(s - start) < frame / 2 && abs(d - duration) < frame * 1.5
        }
        var hit = exact.count == 1 ? exact.first : nil
        if hit == nil, let t = media {
            let inside = same.filter {
                guard let s = $0.value["start"] as? Double, let d = $0.value["duration"] as? Double else { return false }
                return s - frame / 2 <= t && t < s + d
            }
            if inside.count == 1 { hit = inside.first }
        }
        guard let e = hit, let v = e.value["values"] as? [String: Any],
              let cam = v["camera"] as? String, let space = v["space"] as? Int, let gamma = v["gamma"] as? Int else {
            return nil
        }
        return CameraValues(camera: cam, ei: (v["ei"] as? Double) ?? Double(v["ei"] as? Int ?? 800),
                            kelvin: (v["kelvin"] as? Double) ?? Double(v["kelvin"] as? Int ?? 5600),
                            tint: (v["tint"] as? Double) ?? 0, space: Int32(space), gamma: Int32(gamma))
    }

    private func reloadIfChanged() {
        guard let attrs = try? FileManager.default.attributesOfItem(atPath: catalogPath),
              let date = attrs[.modificationDate] as? Date else { return }
        lock.lock(); defer { lock.unlock() }
        if loadedDate == date { return }
        guard let data = FileManager.default.contents(atPath: catalogPath),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let f = json["files"] as? [String: [String: Any]] else { return }
        files = f
        loadedDate = date
        catLog.info("indice caricato: \(f.count, privacy: .public) file")
    }

    /// Update the index (new files of the libraries) in the background, at most every `minInterval` seconds.
    func refreshInBackground(minInterval: TimeInterval = 60) {
        lock.lock()
        if refreshing || Date().timeIntervalSince(lastRefresh) < minInterval { lock.unlock(); return }
        refreshing = true
        lastRefresh = Date()
        lock.unlock()
        DispatchQueue.global(qos: .utility).async { [self] in
            runIndexer()
            lock.lock(); refreshing = false; lock.unlock()
        }
    }

    /// Update the index now and wait for it (the "Leggi metadata" button).
    func refreshNow() {
        lock.lock(); refreshing = true; lastRefresh = Date(); lock.unlock()
        runIndexer()
        lock.lock(); refreshing = false; lock.unlock()
    }

    private func runIndexer() {
        let python = ["/opt/homebrew/bin/python3", "/usr/local/bin/python3", "/usr/bin/python3"]
            .first { FileManager.default.isExecutableFile(atPath: $0) }
        guard let python else { catLog.error("python3 non trovato: indice non aggiornato"); return }
        let p = Process()
        p.executableURL = URL(fileURLWithPath: python)
        p.arguments = ["-m", "slogmetaraw.fcp_catalog", "--output", catalogPath]
        p.environment = ["PYTHONPATH": (support as NSString).appendingPathComponent("lib"),
                         "PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "HOME": NSHomeDirectory()]
        p.standardOutput = FileHandle.nullDevice
        p.standardError = FileHandle.nullDevice
        do {
            try p.run()
            p.waitUntilExit()
            catLog.info("indice aggiornato, uscita \(p.terminationStatus, privacy: .public)")
        } catch {
            catLog.error("indice non aggiornato: \(error.localizedDescription, privacy: .public)")
        }
    }
}

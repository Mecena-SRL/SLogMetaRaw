// SPDX-License-Identifier: GPL-3.0-or-later
// Log MetaRaw as a Final Cut Pro effect (FxPlug 4, tileable, Metal).
// The panel follows the "S-Log MetaRaw" node of DaVinci Resolve (tests/golden/params_develop.txt): same labels, same
// order, same white-balance behaviour. The maths is the same code (DevelopHost.mm -> gen/DevelopMath.h).
//
// An FxPlug cannot see the file of the clip it is applied to, so it cannot read the camera metadata itself as the
// Resolve node does. The camera values come in with the clip: the metadata tool (slogmetaraw.fcp_io) applies this
// effect already set to each clip's as-shot values, and writes the camera line shown at the top.
import Foundation
import AppKit
import Metal
import simd
import os

private let lmrLog = Logger(subsystem: "com.slogmetaraw.fcp", category: "develop")

// Parameter IDs are permanent (Final Cut stores them in projects and FCPXML keys are built from them:
// 9999/10140/10143/3/10145/<group>/.../<id>). fcp/make_motion_template.py and slogmetaraw/fcp_io.py use the same IDs.
enum P: UInt32 {
    case camera = 1, readMeta = 2
    case shotGroup = 100, whiteBalance = 101, temp = 102, fcTemperature = 103, tint = 104, fcTint = 105,
         ei = 111, fcExposure = 112
    case colourGroup = 200
    case inputGroup = 210, inSpace = 211, inGamma = 212
    case outputGroup = 220, rendering = 221, gamutCompress = 222, gamutMapping = 223, highlightVibrance = 224,
         outSpace = 225, outGamma = 226
    case tonesGroup = 300, highlights = 301, softClipLevel = 302, softClip = 303, saturation = 304, tonesMore = 305,
         contrast = 306, shadows = 307, whites = 308, blacks = 309, vibrance = 310
    case manualGroup = 400, shotEI = 401, shotTemp = 402, shotTint = 403
}

// Menu entries, in the order of kSpaceNames / kGammaNames of the OpenFX plugin (ofx/SLogMetaRaw/src/common/ColourSpaces.cpp).
let spaceNames = ["DaVinci WG", "Rec.709", "Rec.2020", "P3 D65", "P3 D60", "P3 DCI", "S-Gamut", "S-Gamut3",
                  "S-Gamut3.Cine", "ACES AP0", "ACES AP1", "V-Gamut", "Apple Wide Gamut", "D-Gamut",
                  "Cinema Gamut", "ARRI WG3", "ARRI WG4", "RED WG", "BMD WG Gen5", "DJI D-Log M Gamut"]
let gammaNames = ["DaVinci Intermediate", "Linear", "Gamma 2.2", "Gamma 2.4", "Gamma 2.6", "Rec.709", "sRGB",
                  "SLog", "SLog2", "SLog3", "ACEScct", "V-Log", "Apple Log", "D-Log M", "D-Log", "F-Log",
                  "F-Log2", "N-Log", "C-Log2", "C-Log3", "LogC3", "LogC4", "Log3G10", "BMD Film Gen5", "HLG"]
private let wbNames = ["As shot", "Daylight", "Cloudy", "Shade", "Tungsten", "Fluorescent", "Flash", "Custom"]
private let wbKelvin: [Double] = [0, 5600, 6500, 7500, 3200, 4000, 5500]   // kPresetKelvin of DevelopEffect.cpp
private let wbCustom: Int32 = 7
private let renderingNames = ["Nessuno (CST)", "ACES 2.0", "Luminanza (stile DaVinci)", "Filmico", "ACES 1.3", "OpenDRT"]
private let gamutCompressNames = ["Nessuna", "ACES riferimento (RGC)"]
private let gamutMappingNames = ["Nessuna", "Compressione saturazione"]
// Output menus of the OFX node; its "Timeline" is the Final Cut project here (nothing follows the effect)
private let outSpaceNames = ["Progetto"] + Array(spaceNames[0..<11])
private let outGammaNames = ["Progetto"] + Array(gammaNames[0..<11])
private let moreTones: [P] = [.contrast, .shadows, .whites, .blacks, .vibrance]

// set LOGMETARAW_PROBE=1 in the plug-in environment, or keep true while the beta is checked
private let probeEnabled = true

// IEEE half -> Float without Float16 (unavailable in the x86_64 slice)
private func halfToFloat(_ h: UInt16) -> Float {
    let sign: UInt32 = UInt32(h & 0x8000) << 16
    var exp = Int32((h >> 10) & 0x1f), mant = UInt32(h & 0x3ff)
    if exp == 0 {
        if mant == 0 { return Float(bitPattern: sign) }
        while mant & 0x400 == 0 { mant <<= 1; exp -= 1 }   // subnormal: normalise
        exp += 1; mant &= 0x3ff
    } else if exp == 31 {
        return Float(bitPattern: sign | 0x7f80_0000 | (mant << 13))
    }
    return Float(bitPattern: sign | UInt32(exp + 112) << 23 | (mant << 13))
}

let noMetadataText = "Nessun metadata"
let notIndexedText = "Nessun metadata: clip non trovata nell'indice delle librerie"

@objc(LogMetaRawDevelopPlugIn)
class LogMetaRawDevelopPlugIn: NSObject, FxTileableEffect {

    let _apiManager: PROAPIAccessing!
    private var syncing = false   // a value we set ourselves must not bounce back as a user edit
    private var autoKey: String?  // the clip the effect last set itself for (start/duration), once per clip

    required init?(apiManager: PROAPIAccessing) {
        _apiManager = apiManager
    }

    // MARK: Panel

    func addParameters() throws {
        let api = _apiManager!.api(for: FxParameterCreationAPI_v5.self) as! FxParameterCreationAPI_v5
        let none = FxParameterFlags(kFxParameterFlag_DEFAULT)
        let still = FxParameterFlags(kFxParameterFlag_NOT_ANIMATABLE)
        let closed = FxParameterFlags(kFxParameterFlag_COLLAPSED)
        let hidden = FxParameterFlags(kFxParameterFlag_HIDDEN)

        func slider(_ name: String, _ id: P, _ def: Double, _ lo: Double, _ hi: Double, _ delta: Double,
                    sliderLo: Double? = nil, sliderHi: Double? = nil, flags: FxParameterFlags = none) {
            api.addFloatSlider(withName: name, parameterID: id.rawValue, defaultValue: def, parameterMin: lo,
                               parameterMax: hi, sliderMin: sliderLo ?? lo, sliderMax: sliderHi ?? hi, delta: delta,
                               parameterFlags: flags)
        }
        func popup(_ name: String, _ id: P, _ def: UInt32, _ entries: [String]) {
            api.addPopupMenu(withName: name, parameterID: id.rawValue, defaultValue: def, menuEntries: entries,
                             parameterFlags: still)
        }
        func toggle(_ name: String, _ id: P, flags: FxParameterFlags = still) {
            api.addToggleButton(withName: name, parameterID: id.rawValue, defaultValue: false, parameterFlags: flags)
        }
        func group(_ name: String, _ id: P, flags: FxParameterFlags = none, _ body: () -> Void) {
            api.startParameterSubGroup(name, parameterID: id.rawValue, parameterFlags: flags)
            body()
            api.endParameterSubGroup()
        }

        // the camera line: written by the metadata tool, read-only
        api.addStringParameter(withName: "Camera", parameterID: P.camera.rawValue, defaultValue: noMetadataText,
                               parameterFlags: FxParameterFlags(kFxParameterFlag_DISABLED | kFxParameterFlag_NOT_ANIMATABLE))
        api.addPushButton(withName: "Leggi metadata", parameterID: P.readMeta.rawValue,
                          selector: #selector(readMetadata), parameterFlags: none)

        group("Ripresa", .shotGroup) {
            popup("Bilanciamento", .whiteBalance, 0, wbNames)
            slider("Temperatura", .temp, 5600, 2000, 15000, 10)
            toggle("False color temperatura", .fcTemperature)
            slider("Tint", .tint, 0, -100, 100, 0.1)
            toggle("False color tint", .fcTint)
            slider("EI", .ei, 800, 25, 409600, 1, sliderLo: 50, sliderHi: 25600)
            toggle("False color esposizione", .fcExposure)
        }
        group("Toni", .tonesGroup) {
            slider("Highlights", .highlights, 0, -100, 100, 0.1)
            slider("Bianco (stop)", .softClipLevel, 2.5, 1, 10, 0.05, sliderLo: 1, sliderHi: 8)
            toggle("Soft Clip", .softClip)
            slider("Saturation", .saturation, 0, -100, 100, 0.1)
            toggle("Mostra tutti i controlli", .tonesMore)
            slider("Contrast", .contrast, 0, -100, 100, 0.1, flags: hidden)
            slider("Shadows", .shadows, 0, -100, 100, 0.1, flags: hidden)
            slider("Whites", .whites, 0, -100, 100, 0.1, flags: hidden)
            slider("Blacks", .blacks, 0, -100, 100, 0.1, flags: hidden)
            slider("Vibrance", .vibrance, 0, -100, 100, 0.1, flags: hidden)
        }
        group("Spazio colore", .colourGroup, flags: closed) {
            group("Ingresso", .inputGroup) {
                popup("Spazio colore", .inSpace, 8, spaceNames)   // S-Gamut3.Cine
                popup("Gamma", .inGamma, 9, gammaNames)           // SLog3
            }
            group("Uscita", .outputGroup) {
                popup("Tone mapping", .rendering, 0, renderingNames)
                popup("Compressione gamut", .gamutCompress, 0, gamutCompressNames)
                popup("Mappatura gamut", .gamutMapping, 0, gamutMappingNames)
                slider("Vibrance luci", .highlightVibrance, 0, 0, 100, 1)
                popup("Spazio colore", .outSpace, 0, outSpaceNames)
                popup("Gamma", .outGamma, 0, outGammaNames)
            }
        }
        group("Senza metadata", .manualGroup, flags: closed) {
            slider("EI di ripresa", .shotEI, 800, 25, 409600, 1, sliderLo: 100, sliderHi: 25600, flags: still)
            slider("Kelvin di ripresa", .shotTemp, 5600, 2000, 15000, 10, flags: still)
            slider("Tint di ripresa", .shotTint, 0, -100, 100, 0.1, flags: still)
        }
    }

    // White balance as in DevelopEffect::changedParam: a preset sets Temperature (As shot: the camera's values),
    // moving Temperature or Tint makes it Custom. "Mostra tutti i controlli" shows the rest of the tones.
    func parameterChanged(_ paramID: UInt32, at time: CMTime) throws {
        guard !syncing, let id = P(rawValue: paramID) else { return }
        let get = _apiManager!.api(for: FxParameterRetrievalAPI_v6.self) as! FxParameterRetrievalAPI_v6
        let set = _apiManager!.api(for: FxParameterSettingAPI_v5.self) as! FxParameterSettingAPI_v5
        syncing = true
        defer { syncing = false }
        switch id {
        case .whiteBalance:
            var wb: Int32 = 0
            get.getIntValue(&wb, fromParameter: P.whiteBalance.rawValue, at: time)
            if wb == 0 {
                var k = 5600.0, t = 0.0
                get.getFloatValue(&k, fromParameter: P.shotTemp.rawValue, at: time)
                get.getFloatValue(&t, fromParameter: P.shotTint.rawValue, at: time)
                set.setFloatValue(k, toParameter: P.temp.rawValue, at: time)
                set.setFloatValue(t, toParameter: P.tint.rawValue, at: time)
            } else if wb > 0 && wb < wbCustom {
                set.setFloatValue(wbKelvin[Int(wb)], toParameter: P.temp.rawValue, at: time)
                set.setFloatValue(0, toParameter: P.tint.rawValue, at: time)
            }
        case .temp, .tint:
            var wb: Int32 = 0, k = 0.0, t = 0.0, sk = 0.0, st = 0.0
            get.getIntValue(&wb, fromParameter: P.whiteBalance.rawValue, at: time)
            get.getFloatValue(&k, fromParameter: P.temp.rawValue, at: time)
            get.getFloatValue(&t, fromParameter: P.tint.rawValue, at: time)
            get.getFloatValue(&sk, fromParameter: P.shotTemp.rawValue, at: time)
            get.getFloatValue(&st, fromParameter: P.shotTint.rawValue, at: time)
            let asShot = abs(k - sk) < 0.5 && abs(t - st) < 0.05
            let preset = wb > 0 && wb < wbCustom && abs(k - wbKelvin[Int(wb)]) < 0.5 && abs(t) < 0.05
            if wb != wbCustom && !(wb == 0 && asShot) && !preset {
                set.setIntValue(wbCustom, toParameter: P.whiteBalance.rawValue, at: time)
            }
        case .fcExposure, .fcTemperature, .fcTint:
            // one false colour view at a time, as in the Resolve node: turning one on turns the others off
            var on: ObjCBool = false
            get.getBoolValue(&on, fromParameter: paramID, at: time)
            if on.boolValue {
                for other in [P.fcExposure, .fcTemperature, .fcTint] where other != id {
                    set.setBoolValue(false, toParameter: other.rawValue, at: time)
                }
            }
        case .tonesMore:
            var more: ObjCBool = false
            get.getBoolValue(&more, fromParameter: P.tonesMore.rawValue, at: time)
            for p in moreTones {
                set.setParameterFlags(FxParameterFlags(more.boolValue ? kFxParameterFlag_DEFAULT : kFxParameterFlag_HIDDEN),
                                      toParameter: p.rawValue)
            }
        default:
            break
        }
    }

    // MARK: Camera metadata (MetadataCatalog): set the effect to the clip's as-shot values, as the Resolve node does

    private func clipTiming(media: CMTime) -> (start: Double, duration: Double, frame: Double, media: Double)? {
        guard let timing = _apiManager!.api(for: FxTimingAPI_v4.self) as? FxTimingAPI_v4 else { return nil }
        var start = CMTime.zero, dur = CMTime.zero, fd = CMTime.zero
        timing.startTimeOfInput(toFilter: &start)
        timing.durationTimeOfInput(toFilter: &dur)
        timing.frameDuration(&fd)
        guard fd.seconds > 0 else { return nil }
        return (start.seconds, dur.seconds, fd.seconds, media.seconds)
    }

    private func cameraText(at time: CMTime) -> String {
        let get = _apiManager!.api(for: FxParameterRetrievalAPI_v6.self) as! FxParameterRetrievalAPI_v6
        var text: NSString = ""
        get.getStringParameterValue(&text, fromParameter: P.camera.rawValue)
        return text as String
    }

    /// Writes the clip's camera values into the panel (inside an action, so the host accepts it at any time).
    private func apply(_ v: CameraValues) {
        guard let action = _apiManager!.api(for: FxCustomParameterActionAPI_v4.self) as? FxCustomParameterActionAPI_v4,
              let set = _apiManager!.api(for: FxParameterSettingAPI_v5.self) as? FxParameterSettingAPI_v5 else { return }
        let t = action.currentTime()
        syncing = true
        action.startAction(self)
        set.setStringParameterValue(v.camera, toParameter: P.camera.rawValue)
        set.setFloatValue(v.ei, toParameter: P.shotEI.rawValue, at: t)
        set.setFloatValue(v.kelvin, toParameter: P.shotTemp.rawValue, at: t)
        set.setFloatValue(v.tint, toParameter: P.shotTint.rawValue, at: t)
        set.setIntValue(0, toParameter: P.whiteBalance.rawValue, at: t)     // As shot
        set.setFloatValue(v.ei, toParameter: P.ei.rawValue, at: t)
        set.setFloatValue(v.kelvin, toParameter: P.temp.rawValue, at: t)
        set.setFloatValue(v.tint, toParameter: P.tint.rawValue, at: t)
        set.setIntValue(v.space, toParameter: P.inSpace.rawValue, at: t)
        set.setIntValue(v.gamma, toParameter: P.inGamma.rawValue, at: t)
        action.endAction(self)
        syncing = false
        lmrLog.info("metadata applicati: \(v.camera, privacy: .public)")
    }

    /// "Leggi metadata": look the clip up (updating the index first when it is not there) and set the panel.
    @objc func readMetadata() {
        guard let action = _apiManager!.api(for: FxCustomParameterActionAPI_v4.self) as? FxCustomParameterActionAPI_v4,
              let tm = clipTiming(media: action.currentTime()) else { return }
        var v = MetadataCatalog.shared.lookup(start: tm.start, duration: tm.duration, frame: tm.frame, media: tm.media)
        if v == nil {
            MetadataCatalog.shared.refreshNow()
            v = MetadataCatalog.shared.lookup(start: tm.start, duration: tm.duration, frame: tm.frame, media: tm.media)
        }
        if let v {
            apply(v)
        } else if let set = _apiManager!.api(for: FxParameterSettingAPI_v5.self) as? FxParameterSettingAPI_v5 {
            action.startAction(self)
            set.setStringParameterValue(notIndexedText, toParameter: P.camera.rawValue)
            action.endAction(self)
        }
    }

    /// Called while rendering: an effect still without metadata sets itself once its clip is found in the index.
    private func autoApply(at renderTime: CMTime) {
        guard cameraText(at: renderTime).hasPrefix(noMetadataText), let tm = clipTiming(media: renderTime) else { return }
        let key = String(format: "%.3f/%.3f", tm.start, tm.duration)
        guard key != autoKey else { return }
        if let v = MetadataCatalog.shared.lookup(start: tm.start, duration: tm.duration, frame: tm.frame, media: tm.media) {
            autoKey = key
            DispatchQueue.main.async { [weak self] in self?.apply(v) }
        } else {
            MetadataCatalog.shared.refreshInBackground()   // new clips: the next render finds them
        }
    }

    func properties(_ properties: AutoreleasingUnsafeMutablePointer<NSDictionary>?) throws {
        let props: [AnyHashable: Any] = [
            kFxPropertyKey_MayRemapTime: NSNumber(value: false),
            kFxPropertyKey_PixelTransformSupport: NSNumber(value: kFxPixelTransform_ScaleTranslate),
            kFxPropertyKey_VariesWhenParamsAreStatic: NSNumber(value: false),
        ]
        properties?.pointee = NSDictionary(dictionary: props)
    }

    // MARK: Plug-in state: the panel at the render time, already turned into DevelopParams

    func pluginState(_ pluginState: AutoreleasingUnsafeMutablePointer<NSData>?, at renderTime: CMTime, quality qualityLevel: UInt) throws {
        let api = _apiManager!.api(for: FxParameterRetrievalAPI_v6.self) as! FxParameterRetrievalAPI_v6
        func f(_ id: P) -> Double {
            var v = 0.0
            api.getFloatValue(&v, fromParameter: id.rawValue, at: renderTime)
            return v
        }
        func i(_ id: P) -> Int32 {
            var v: Int32 = 0
            api.getIntValue(&v, fromParameter: id.rawValue, at: renderTime)
            return v
        }
        func b(_ id: P) -> Int32 {
            var v: ObjCBool = false
            api.getBoolValue(&v, fromParameter: id.rawValue, at: renderTime)
            return v.boolValue ? 1 : 0
        }
        // the project's working encoding: Rec.709 / Gamma 2.4, or Rec.2020 / HLG for a wide-gamut HDR library
        var projSpace: Int32 = 1, projGamma: Int32 = 3
        if let gamut = _apiManager!.api(for: FxColorGamutAPI_v2.self) as? FxColorGamutAPI_v2,
           gamut.colorPrimaries() == UInt(kFxColorPrimaries_Rec2020) {
            projSpace = 2; projGamma = 24
        }
        var c = LMRDevelopControls(
            shotEI: f(.shotEI), shotTemp: f(.shotTemp), shotTint: f(.shotTint),
            ei: f(.ei), temp: f(.temp), tint: f(.tint),
            inSpace: i(.inSpace), inGamma: i(.inGamma),
            outSpace: i(.outSpace) - 1, outGamma: i(.outGamma) - 1,
            projSpace: projSpace, projGamma: projGamma,
            rendering: i(.rendering), gamutCompress: i(.gamutCompress), gamutMapping: i(.gamutMapping),
            highlightVibrance: f(.highlightVibrance),
            contrast: f(.contrast), highlights: f(.highlights), shadows: f(.shadows), whites: f(.whites),
            blacks: f(.blacks), vibrance: f(.vibrance), saturation: f(.saturation),
            fcExposure: b(.fcExposure), fcTemperature: b(.fcTemperature), fcTint: b(.fcTint),
            softClip: b(.softClip), softClipLevel: f(.softClipLevel))
        pluginState?.pointee = LMRBuildDevelopParams(&c) as NSData
        autoApply(at: renderTime)
        lmrLog.info("pluginState ei=\(c.ei, privacy: .public) shotEI=\(c.shotEI, privacy: .public) space=\(c.inSpace, privacy: .public) gamma=\(c.inGamma, privacy: .public)")
    }

    // MARK: Geometry: a point operation, the tile in is the tile out

    func destinationImageRect(_ destinationImageRect: UnsafeMutablePointer<FxRect>, sourceImages: [FxImageTile], destinationImage: FxImageTile, pluginState: Data?, at renderTime: CMTime) throws {
        destinationImageRect.pointee = sourceImages[0].imagePixelBounds
    }

    func sourceTileRect(_ sourceTileRect: UnsafeMutablePointer<FxRect>, sourceImageIndex: UInt, sourceImages: [FxImageTile], destinationTileRect: FxRect, destinationImage: FxImageTile, pluginState: Data?, at renderTime: CMTime) throws {
        sourceTileRect.pointee = destinationTileRect
    }

    // MARK: Render

    /// Exponent between the clip's code values and the pixels Final Cut gives the effect. Measured in Final Cut Pro 13
    /// with uniform ProRes 10-bit Rec.709 clips (code 0.110 / 0.269 / 0.511 / 0.726 / 0.909 -> pixel = code^1.956..1.964):
    /// Apple's Rec.709 curve, 1.961, for a Rec.709 (SDR) library. Other working spaces are not handled yet: 0 = no change.
    static func hostGamma(for tile: FxImageTile) -> Float {
        let name = (tile.colorSpace?.name as String?) ?? ""
        return name.contains("709") ? 1.961 : 0
    }

    func renderDestinationImage(_ destinationImage: FxImageTile, sourceImages: [FxImageTile], pluginState: Data?, at renderTime: CMTime) throws {
        guard let state = pluginState, state.count == Int(LMRDevelopParamsSize()) else {
            throw NSError(domain: "LogMetaRaw", code: 1, userInfo: [NSLocalizedDescriptionKey: "Parametri del render non validi"])
        }
        let deviceCache = MetalDeviceCache.deviceCache
        let pixelFormat = MetalDeviceCache.FxMTLPixelFormat(for: destinationImage)
        guard let commandQueue = deviceCache.commandQueue(with: sourceImages[0].deviceRegistryID, pixelFormat: pixelFormat) else {
            throw NSError(domain: "LogMetaRaw", code: 2, userInfo: [NSLocalizedDescriptionKey: "Nessuna coda di comandi Metal"])
        }
        defer { deviceCache.returnCommandQueueToCache(commandQueue: commandQueue) }

        let commandBuffer = commandQueue.makeCommandBuffer()!
        commandBuffer.label = "Log MetaRaw Develop"
        commandBuffer.enqueue()

        let inputTexture = sourceImages[0].metalTexture(for: deviceCache.device(with: sourceImages[0].deviceRegistryID))
        let outputTexture = destinationImage.metalTexture(for: deviceCache.device(with: destinationImage.deviceRegistryID))

        let colorAttachment = MTLRenderPassColorAttachmentDescriptor()
        colorAttachment.texture = outputTexture
        colorAttachment.clearColor = MTLClearColorMake(0, 0, 0, 0)
        colorAttachment.loadAction = .clear
        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0] = colorAttachment
        let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: pass)!

        let w = destinationImage.tilePixelBounds.right - destinationImage.tilePixelBounds.left
        let h = destinationImage.tilePixelBounds.top - destinationImage.tilePixelBounds.bottom
        var vertices = [
            LMRVertex2D(position: vector_float2(Float(w) / 2, Float(-h) / 2), textureCoordinate: vector_float2(1, 1)),
            LMRVertex2D(position: vector_float2(Float(-w) / 2, Float(-h) / 2), textureCoordinate: vector_float2(0, 1)),
            LMRVertex2D(position: vector_float2(Float(w) / 2, Float(h) / 2), textureCoordinate: vector_float2(1, 0)),
            LMRVertex2D(position: vector_float2(Float(-w) / 2, Float(h) / 2), textureCoordinate: vector_float2(0, 0)),
        ]
        encoder.setViewport(MTLViewport(originX: 0, originY: 0, width: Double(w), height: Double(h), znear: -1, zfar: 1))
        encoder.setRenderPipelineState(deviceCache.pipelineState(with: sourceImages[0].deviceRegistryID, pixelFormat: pixelFormat)!)
        encoder.setVertexBytes(&vertices, length: MemoryLayout<LMRVertex2D>.size * 4, index: Int(LMRVertices.rawValue))
        var viewport = simd_uint2(UInt32(w), UInt32(h))
        encoder.setVertexBytes(&viewport, length: MemoryLayout.size(ofValue: viewport), index: Int(LMRViewportSize.rawValue))
        encoder.setFragmentTexture(inputTexture, index: Int(LMRSourceImage.rawValue))
        state.withUnsafeBytes { raw in
            encoder.setFragmentBytes(raw.baseAddress!, length: raw.count, index: Int(LMRDevelopParamsIndex.rawValue))
        }
        var hostGamma = Self.hostGamma(for: sourceImages[0])
        encoder.setFragmentBytes(&hostGamma, length: MemoryLayout<Float>.size, index: Int(LMRHostGammaIndex.rawValue))
        encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
        encoder.endEncoding()

        // Diagnostics: the centre pixel in and out, to check the pipeline against the CPU maths (tools/fcp_probe)
        var probe: MTLBuffer? = nil
        if probeEnabled, let input = inputTexture, let output = outputTexture,
           input.pixelFormat == .rgba16Float, output.pixelFormat == .rgba16Float,
           let buf = commandBuffer.device.makeBuffer(length: 16, options: .storageModeShared),
           let blit = commandBuffer.makeBlitCommandEncoder() {
            let o = MTLOrigin(x: input.width / 2, y: input.height / 2, z: 0)
            let size = MTLSize(width: 1, height: 1, depth: 1)
            blit.copy(from: input, sourceSlice: 0, sourceLevel: 0, sourceOrigin: o, sourceSize: size,
                      to: buf, destinationOffset: 0, destinationBytesPerRow: 8, destinationBytesPerImage: 8)
            let oo = MTLOrigin(x: output.width / 2, y: output.height / 2, z: 0)
            blit.copy(from: output, sourceSlice: 0, sourceLevel: 0, sourceOrigin: oo, sourceSize: size,
                      to: buf, destinationOffset: 8, destinationBytesPerRow: 8, destinationBytesPerImage: 8)
            blit.endEncoding()
            probe = buf
        }
        commandBuffer.commit()
        commandBuffer.waitUntilCompleted()
        if let err = commandBuffer.error { lmrLog.error("render failed: \(String(describing: err), privacy: .public)") }
        if let buf = probe {
            let h = buf.contents().bindMemory(to: UInt16.self, capacity: 8)
            lmrLog.info("probe media=\(sourceImages[0].mediaTime.seconds, privacy: .public) in=\(halfToFloat(h[0]), privacy: .public) \(halfToFloat(h[1]), privacy: .public) \(halfToFloat(h[2]), privacy: .public) out=\(halfToFloat(h[4]), privacy: .public) \(halfToFloat(h[5]), privacy: .public) \(halfToFloat(h[6]), privacy: .public) fmt=\(inputTexture?.pixelFormat.rawValue ?? 0, privacy: .public)/\(outputTexture?.pixelFormat.rawValue ?? 0, privacy: .public) cs=\((sourceImages[0].colorSpace?.name as String?) ?? "nil", privacy: .public)")
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoiseEffect.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "ofxColour.h"
#include "ofxImageEffectExt.h"
#include "ofxsMultiThread.h"
#include "../common/ClipCache.h"
#include "../common/ColourSpaces.h"
#include "../common/ImageLayout.h"

namespace {

std::atomic<bool>& skipFlag() { static std::atomic<bool> f(false); return f; }
std::atomic<int>& liveNodes() { static std::atomic<int> n(0); return n; }

// The host's worker threads for the CPU passes.
class HostThreads : public OFX::MultiThread::Processor
{
public:
    HostThreads(int n, const std::function<void(int, int)>& fn) : m_N(n), m_Fn(fn) {}
    void multiThreadFunction(unsigned int t, unsigned int nThreads) override
    {
        m_Fn((int)((long long)m_N * t / nThreads), (int)((long long)m_N * (t + 1) / nThreads));
    }

private:
    int m_N;
    const std::function<void(int, int)>& m_Fn;
};

// Working planes shared by every Noise node, at most two sets kept between frames.
class ScratchLease
{
public:
    ScratchLease()
    {
        std::lock_guard<std::mutex> lock(mutex());
        if (!pool().empty()) {
            m_S = std::move(pool().back());
            pool().pop_back();
        } else {
            m_S.reset(new NoiseScratch());
        }
    }
    ~ScratchLease()
    {
        std::lock_guard<std::mutex> lock(mutex());
        if (pool().size() < 2) pool().push_back(std::move(m_S));
    }
    NoiseScratch& operator*() { return *m_S; }
    static void drop()
    {
        std::lock_guard<std::mutex> lock(mutex());
        pool().clear();
    }

private:
    static std::mutex& mutex() { static std::mutex m; return m; }
    static std::vector<std::unique_ptr<NoiseScratch>>& pool() { static std::vector<std::unique_ptr<NoiseScratch>> p; return p; }
    std::unique_ptr<NoiseScratch> m_S;
};

void serial(int n, const std::function<void(int, int)>& fn) { fn(0, n); }

}  // namespace

bool noiseSkipped() { return skipFlag().load(); }

NoiseEffect::NoiseEffect(OfxImageEffectHandle p_Handle)
    : ImageEffect(p_Handle)
{
    ++liveNodes();
    try {
        m_DstClip = fetchClip(kOfxImageEffectOutputClipName);
        m_SrcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        m_Info = fetchStringParam("noiseInfo");
        m_ProfileInfo = fetchStringParam("profileInfo");
        m_Measured = fetchStringParam("measuredProfile");
        m_BoundPath = fetchStringParam("boundPath");
        m_SettingsVersion = fetchIntParam("settingsVersion");
        m_ProfileMode = fetchChoiceParam("profileMode");
        m_NodeInput = fetchChoiceParam("nodeInput");
        m_Frames = fetchChoiceParam("temporalFrames");
        m_View = fetchChoiceParam("noiseView");
        m_Level = fetchDoubleParam("noiseLevel");
        m_TemporalStrength = fetchDoubleParam("temporalStrength");
        m_Luma = fetchDoubleParam("lumaStrength");
        m_Chroma = fetchDoubleParam("chromaStrength");
        m_Fine = fetchDoubleParam("fineBand");
        m_Medium = fetchDoubleParam("mediumBand");
        m_Coarse = fetchDoubleParam("coarseBand");
        m_Shadows = fetchDoubleParam("shadowStrength");
        m_Preserve = fetchDoubleParam("preserveDetail");
        m_Skin = fetchDoubleParam("skinProtect");
        m_Ready = true;
    } catch (...) {
        fprintf(stderr, "Log MetaRaw Noise: nodo disattivato, parametro mancante\n");
        return;
    }
    try {
        if (m_SettingsVersion->getValue() < kNoiseSettingsVersion) m_SettingsVersion->setValue(kNoiseSettingsVersion);
        std::string text;
        m_Measured->getValue(text);
        m_MeasuredProfile = noiseProfileFromText(text);
        syncClip();
        m_Badge.attach(*this);
    } catch (...) {
    }
}

NoiseEffect::~NoiseEffect()
{
    if (--liveNodes() == 0) ScratchLease::drop();   // the last Noise node is gone: its working planes too
}

void NoiseEffect::syncClip()
{
    const std::string path = getPropertySet().propGetString(kOfxImageEffectPropSrcFilePath, false);
    std::map<std::string, std::string> record;
    NoiseSource src;
    if (!path.empty() && readClipRecord(path, record)) src = noiseSourceFromRecord(record);
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Source = src;
    }
    std::string bound;
    m_BoundPath->getValue(bound);
    if (bound != path) m_BoundPath->setValue(path);
    refreshInfo();
}

// Declared, then what Resolve reports, then the clip's camera curve; S-Log3 when nothing says otherwise.
int NoiseEffect::inputGamma() const
{
    int input = 0;
    m_NodeInput->getValue(input);
    if (input > 0 && input < kNodeInputCount) return kNodeInputPairs[input][1];
    int space = 0, gamma = 0;
    const std::string cs = m_SrcClip->getPropertySet().propGetString(kOfxImageClipPropColourspace, false);
    if (mapColourspace(cs, space, gamma)) return gamma;
    if (!cs.empty()) return -1;
    std::lock_guard<std::mutex> lock(m_Mutex);
    return validCode(m_Source.gamma, kGammaCount) >= 0 ? m_Source.gamma : 9;
}

NoiseModel NoiseEffect::modelAt(double t, int p_FrameWidth) const
{
    int mode = 0;
    m_ProfileMode->getValueAtTime(t, mode);
    const double level = m_Level->getValueAtTime(t);
    NoiseSource src;
    NoiseProfile measured;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        src = m_Source;
        measured = m_MeasuredProfile;
    }
    src.gamma = inputGamma();
    if (mode == 2) {   // Manuale: only the curve, a generic log camera
        NoiseSource generic;
        generic.gamma = src.gamma;
        NoiseModel m = noiseModel(generic, p_FrameWidth, level);
        m.info = "manuale · " + m.info;
        return m;
    }
    NoiseModel m = noiseModel(src, p_FrameWidth, level);
    if (mode == 1) {
        if (measured.valid) {
            const float k = (float)std::max(level, 0.0);
            for (int b = 0; b < kNoiseBins; ++b)
                for (int j = 0; j < kNoiseLevels; ++j) {
                    m.profile.luma[b][j] = measured.luma[b][j] * k;
                    m.profile.chroma[b][j] = measured.chroma[b][j] * k;
                }
            m.info = "misurato dal fotogramma · formato: " + m.info;
        } else {
            m.info = "non ancora misurato (premi Misura) · automatico: " + m.info;
        }
    }
    return m;
}

int NoiseEffect::radiusAt(double t) const
{
    int frames = 0;
    m_Frames->getValueAtTime(t, frames);
    if (frames == 1) return 0;
    if (frames >= 2 && frames <= 4) return frames - 1;
    return modelAt(t, 0).radius;
}

NoiseControls NoiseEffect::controlsAt(double t) const
{
    NoiseControls c;
    c.luma = (float)m_Luma->getValueAtTime(t);
    c.chroma = (float)m_Chroma->getValueAtTime(t);
    c.fine = (float)m_Fine->getValueAtTime(t);
    c.medium = (float)m_Medium->getValueAtTime(t);
    c.coarse = (float)m_Coarse->getValueAtTime(t);
    c.shadows = (float)m_Shadows->getValueAtTime(t);
    c.preserve = (float)m_Preserve->getValueAtTime(t);
    c.skin = (float)m_Skin->getValueAtTime(t);
    c.temporal = (float)m_TemporalStrength->getValueAtTime(t);
    m_View->getValueAtTime(t, c.view);
    return c;
}

bool NoiseEffect::neutralAt(double t) const
{
    if (noiseSkipped() || inputGamma() < 0) return true;
    const NoiseControls c = controlsAt(t);
    if (c.view != kViewOutput) return false;
    const bool spatial = c.luma > 0.0f || c.chroma > 0.0f;
    const bool temporal = c.temporal > 0.0f && radiusAt(t) > 0;
    return !spatial && !temporal;
}

bool NoiseEffect::isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip, double& p_IdentityTime)
{
    if (m_Ready && !neutralAt(p_Args.time)) return false;
    p_IdentityClip = m_SrcClip;
    p_IdentityTime = p_Args.time;
    return true;
}

void NoiseEffect::getFramesNeeded(const OFX::FramesNeededArguments& p_Args, OFX::FramesNeededSetter& p_Frames)
{
    OfxRangeD range;
    const int r = m_Ready && !noiseSkipped() ? radiusAt(p_Args.time) : 0;
    range.min = p_Args.time - r;
    range.max = p_Args.time + r;
    p_Frames.setFramesNeeded(*m_SrcClip, range);
}

void NoiseEffect::render(const OFX::RenderArguments& p_Args)
{
    if (!m_Ready) return;
    if (m_DstClip->getPixelDepth() != OFX::eBitDepthFloat || m_DstClip->getPixelComponents() != OFX::ePixelComponentRGBA)
        OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    std::unique_ptr<OFX::Image> dst(m_DstClip->fetchImage(p_Args.time));
    std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Args.time));
    if (!dst || !src) OFX::throwSuiteStatusException(kOfxStatErrValue);
    if (!sameLayout(*src, *dst)) OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
    const OfxRectI b = src->getBounds();
    const int W = b.x2 - b.x1, H = b.y2 - b.y1;
    if (neutralAt(p_Args.time)) {   // skipped or neutral: the host renders it anyway
        memcpy(dst->getPixelData(), src->getPixelData(), (size_t)src->getRowBytes() * H);
        return;
    }
    const NoiseModel model = modelAt(p_Args.time, W);
    const NoiseControls controls = controlsAt(p_Args.time);
    const int r = controls.temporal > 0.0f ? radiusAt(p_Args.time) : 0;

    // the window in time order, inside the clip; a neighbour of another size is left out
    std::vector<std::unique_ptr<OFX::Image>> held;
    std::vector<NoiseFrame> frames;
    int center = 0;
    const OfxRangeD clip = m_SrcClip->getFrameRange();
    for (int k = -r; k <= r; ++k) {
        if (k == 0) {
            center = (int)frames.size();
            frames.push_back({ static_cast<const float*>(src->getPixelData()), (size_t)src->getRowBytes() / 4 });
            continue;
        }
        const double t = p_Args.time + k;
        if (t < clip.min || t > clip.max) continue;
        std::unique_ptr<OFX::Image> im(m_SrcClip->fetchImage(t));
        if (!im || !sameLayout(*im, *dst)) continue;
        frames.push_back({ static_cast<const float*>(im->getPixelData()), (size_t)im->getRowBytes() / 4 });
        held.push_back(std::move(im));
        if (abort()) return;
    }
    NoiseParallel parallel = [this](int n, const std::function<void(int, int)>& fn) {
        if (abort()) return;
        HostThreads threads(n, fn);
        threads.multiThread();
    };
    ScratchLease scratch;
    noiseRenderCPU(W, H, frames.data(), (int)frames.size(), center, model.profile, controls,
                   static_cast<float*>(dst->getPixelData()), (size_t)dst->getRowBytes() / 4, parallel, *scratch);
}

void NoiseEffect::measure(double t)
{
    std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(t));
    if (!src || src->getPixelDepth() != OFX::eBitDepthFloat || src->getPixelComponents() != OFX::ePixelComponentRGBA) {
        m_ProfileInfo->setValue("Misura non riuscita: nessuna immagine al fotogramma corrente");
        return;
    }
    const OfxRectI b = src->getBounds();
    NoiseScratch s;
    NoiseProfile p;
    const NoiseFrame f = { static_cast<const float*>(src->getPixelData()), (size_t)src->getRowBytes() / 4 };
    if (!noiseMeasure(b.x2 - b.x1, b.y2 - b.y1, f, serial, s, p)) {
        m_ProfileInfo->setValue("Misura non riuscita: troppo poche zone uniformi, prova un altro fotogramma");
        return;
    }
    // the level applies on top: a measure taken at another Livello rumore keeps its own scale
    m_Measured->setValue(noiseProfileToText(p));
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_MeasuredProfile = p;
    }
    m_ProfileMode->setValue(1);
    refreshInfo();
}

void NoiseEffect::refreshInfo()
{
    const double t = timeLineGetTime();
    int input = 0;
    m_NodeInput->getValue(input);
    const int gamma = inputGamma();
    std::string text;
    if (noiseSkipped()) text = "SALTATO in tutto il progetto (fino al riavvio di Resolve o a un nuovo clic) · ";
    if (gamma < 0) {
        text += "ingresso non riconosciuto, nodo neutro · scegli Ingresso (IDT)";
    } else {
        const int r = radiusAt(t);
        text += std::string("Ingresso ") + kGammaNames[gamma] + " · temporale " + (r > 0 ? "±" + std::to_string(r) : "spento")
                + " · CPU";
    }
    std::string cur;
    m_Info->getValue(cur);
    if (cur != text) m_Info->setValue(text);
    const std::string info = gamma < 0 ? std::string("—") : modelAt(t, 0).info;
    m_ProfileInfo->getValue(cur);
    if (cur != info) m_ProfileInfo->setValue(info);
}

void NoiseEffect::beginEdit()
{
    if (!m_Ready) return;
    try {
        syncClip();
        m_Badge.refresh(true);
    } catch (...) {
    }
}

void NoiseEffect::changedClip(const OFX::InstanceChangedArgs&, const std::string& p_ClipName)
{
    if (!m_Ready) return;
    try {
        if (p_ClipName == kOfxImageEffectSimpleSourceClipName) syncClip();
        m_Badge.refresh(false);
    } catch (...) {
    }
}

void NoiseEffect::changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName)
{
    if (!m_Ready) return;
    try {
        if (p_ParamName == "version") {
            m_Badge.clicked();
            return;
        }
        if (p_ParamName == "skipAll") {
            skipFlag().store(!skipFlag().load());
            refreshInfo();
        } else if (p_ParamName == "measure") {
            measure(p_Args.time);
        } else if (p_ParamName == "measuredProfile") {
            std::string text;
            m_Measured->getValue(text);
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_MeasuredProfile = noiseProfileFromText(text);
        } else if (p_ParamName == "profileMode" || p_ParamName == "noiseLevel" || p_ParamName == "nodeInput"
                   || p_ParamName == "temporalFrames") {
            refreshInfo();
        }
    } catch (...) {
    }
}

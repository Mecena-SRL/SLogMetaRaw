// SPDX-License-Identifier: GPL-3.0-or-later
#include "CreativeEffect.h"
#include "ofxImageEffectExt.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#include "ofxColour.h"
#include "ofxsMultiThread.h"
#include "../../metal/MetalKernels.h"
#include "../common/ClipCache.h"
#include "../common/ColourSpaces.h"
#include "../common/ImageLayout.h"
#include "CreativeEngine.h"

static std::atomic<int>& liveNodes() { static std::atomic<int> n(0); return n; }

CreativeEffect::CreativeEffect(OfxImageEffectHandle p_Handle) : ImageEffect(p_Handle)
{
    ++liveNodes();
    try {
        m_DstClip = fetchClip(kOfxImageEffectOutputClipName);
        m_SrcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        m_Info = fetchStringParam("detailInfo");
        m_NodeInput = fetchChoiceParam("nodeInput");
        m_BoundPath = fetchStringParam("boundPath");
        m_CamSpace = fetchIntParam("camSpace");
        m_CamGamma = fetchIntParam("camGamma");
        m_SettingsVersion = fetchIntParam("settingsVersion");
        m_Process = fetchChoiceParam("process");
        m_Ready = true;
    } catch (...) {
        fprintf(stderr, "Log MetaRaw Creative: nodo disattivato, parametro mancante\n");
        return;
    }
    // fetched on the main thread (a first fetch while rendering races with the UI); a missing one reads its default
    try {
        for (int i = 0; i < kCrChoiceCount; ++i)
            if (paramExists(kCrChoices[i].name)) m_Choices[i] = fetchChoiceParam(kCrChoices[i].name);
        for (int i = 0; i < kCrSliderCount; ++i)
            if (paramExists(kCrSliders[i].name)) m_Sliders[i] = fetchDoubleParam(kCrSliders[i].name);
    } catch (...) {
    }
    try {
        if (m_SettingsVersion->getValue() < kCreativeSettingsVersion) m_SettingsVersion->setValue(kCreativeSettingsVersion);
        syncClip();
        updateProcess();
        m_Badge.attach(*this);
    } catch (...) {
    }
}

std::string CreativeEffect::sourcePath() const
{
    return getPropertySet().propGetString(kOfxImageEffectPropSrcFilePath, false);
}

void CreativeEffect::syncClip()
{
    const std::string path = sourcePath();
    std::string bound;
    m_BoundPath->getValue(bound);
    std::map<std::string, std::string> record;
    int space = -1, gamma = -1;
    if (!path.empty() && readClipRecord(path, record)) cameraEncoding(record, space, gamma);
    if (m_CamSpace->getValue() != space) m_CamSpace->setValue(space);
    if (m_CamGamma->getValue() != gamma) m_CamGamma->setValue(gamma);
    if (bound != path) m_BoundPath->setValue(path);
    refreshInfo();
}

// Declared, then what Resolve reports, then the clip's camera encoding (YRGB: the node after the develop still
// receives it). An unrecognised colourspace from the host leaves the node neutral.
InputOrigin CreativeEffect::resolveInput(int& space, int& gamma) const
{
    int input = 0;
    m_NodeInput->getValue(input);
    if (input > 0 && input < kNodeInputCount) {
        space = kNodeInputPairs[input][0];
        gamma = kNodeInputPairs[input][1];
        return InputOrigin::Declared;
    }
    const std::string cs = m_SrcClip->getPropertySet().propGetString(kOfxImageClipPropColourspace, false);
    if (mapColourspace(cs, space, gamma)) return InputOrigin::Host;
    if (!cs.empty()) return InputOrigin::Unknown;
    std::string bound;
    m_BoundPath->getValue(bound);
    const int camS = validCode(m_CamSpace->getValue(), kSpaceCount), camG = validCode(m_CamGamma->getValue(), kGammaCount);
    if (bound == sourcePath() && camS >= 0 && camG >= 0) {
        space = camS;
        gamma = camG;
        return InputOrigin::Camera;
    }
    space = 8;   // S-Gamut3.Cine / S-Log3: what an unmanaged YRGB timeline of Sony footage carries
    gamma = 9;
    return InputOrigin::Assumed;
}

void CreativeEffect::refreshInfo()
{
    int space = 0, gamma = 0;
    const InputOrigin origin = resolveInput(space, gamma);
    std::string text = "Spaziale · escluso da Generate LUT · Ingresso: ";
    if (origin == InputOrigin::Unknown) {
        text += "non riconosciuto, nodo neutro · scegli Processo › Ingresso (IDT)";
    } else {
        static const char* const from[] = { "dichiarato", "da Resolve", "dalla clip", "presunto" };
        text += std::string(kSpaceNames[space]) + " / " + kGammaNames[gamma] + " (" + from[(int)origin] + ")";
    }
    std::string cur;
    m_Info->getValue(cur);
    if (cur != text) m_Info->setValue(text);
}

CreativeControls CreativeEffect::readControls(double t) const
{
    CreativeControls c;
    m_Process->getValueAtTime(t, c.process);
    for (int i = 0; i < kCrChoiceCount; ++i)
        if (m_Choices[i]) m_Choices[i]->getValueAtTime(t, c.*(kCrChoices[i].field));
    for (int i = 0; i < kCrSliderCount; ++i)
        if (m_Sliders[i]) c.*(kCrSliders[i].field) = m_Sliders[i]->getValueAtTime(t);
    return c;
}

void CreativeEffect::updateProcess()
{
    int proc = 0;
    m_Process->getValue(proc);
    if (proc < 0 || proc >= kCrProcessCount) proc = kCrColourNegative;
    const unsigned bit = CR_BIT(proc);
    auto show = [this](const char* name, bool on) {
        if (paramExists(name)) getParam(name)->setIsSecret(!on);
    };
    bool used[kGrCount] = {};
    used[kGrProcess] = used[kGrColour] = true;
    for (const CrChoiceDef& c : kCrChoices) show(c.name, (c.shows & bit) != 0);
    for (const CrSliderDef& s : kCrSliders) {
        const bool on = (s.shows & bit) != 0;
        show(s.name, on);
        used[s.group] = used[s.group] || on;
    }
    for (int g = 0; g < kGrCount; ++g) show(kCrGroups[g].name, used[g]);
    if (paramExists("processInfo")) {
        OFX::StringParam* info = fetchStringParam("processInfo");
        std::string cur;
        info->getValue(cur);
        if (cur != kCrProcessInfo[proc]) info->setValue(kCrProcessInfo[proc]);
    }
}

void CreativeEffect::resetAll()
{
    const CreativeControls def;
    for (int i = 0; i < kCrSliderCount; ++i)
        if (m_Sliders[i]) {
            m_Sliders[i]->deleteAllKeys();
            m_Sliders[i]->setValue(def.*(kCrSliders[i].field));
        }
}

void CreativeEffect::beginEdit()
{
    if (!m_Ready) return;
    try {
        syncClip();
        updateProcess();
        m_Badge.refresh(true);
    } catch (...) {
    }
}

void CreativeEffect::changedClip(const OFX::InstanceChangedArgs&, const std::string& p_ClipName)
{
    if (!m_Ready) return;
    if (p_ClipName == kOfxImageEffectSimpleSourceClipName) syncClip();
    m_Badge.refresh(false);
}

void CreativeEffect::changedParam(const OFX::InstanceChangedArgs&, const std::string& p_ParamName)
{
    if (!m_Ready) return;
    if (p_ParamName == "version") {
        m_Badge.clicked();
        return;
    }
    if (p_ParamName == "creativeReset") resetAll();
    else if (p_ParamName == "nodeInput") refreshInfo();
    else if (p_ParamName == "process") updateProcess();
    m_Badge.refresh(false);
}

bool CreativeEffect::isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip,
                                double& p_IdentityTime)
{
    int space = 0, gamma = 0;
    if (m_Ready && resolveInput(space, gamma) != InputOrigin::Unknown) return false;
    p_IdentityClip = m_SrcClip;
    p_IdentityTime = p_Args.time;
    return true;
}

// the frames the records are exposed from: whole frames after this one
static void recordTimes(const CreativeControls& c, double t, double out[3])
{
    creativeRecordTimes(c, out);
    for (int k = 0; k < 3; ++k) out[k] = t + std::round(out[k]);
}

void CreativeEffect::getFramesNeeded(const OFX::FramesNeededArguments& p_Args, OFX::FramesNeededSetter& p_Frames)
{
    OfxRangeD range;
    range.min = range.max = p_Args.time;
    if (m_Ready) {
        double t[3];
        recordTimes(readControls(p_Args.time), p_Args.time, t);
        for (double v : t) range.max = std::max(range.max, v);
    }
    p_Frames.setFramesNeeded(*m_SrcClip, range);
}

namespace {
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

// Working planes shared by every Creative node, at most kKeep sets. A frame takes the set sized for it; a set more
// than twice its size is freed on release, so an 8K render does not pin memory for HD work.
class ScratchLease
{
public:
    explicit ScratchLease(size_t pixels) : m_Pixels(pixels)
    {
        std::lock_guard<std::mutex> lock(mutex());
        auto& p = pool();
        auto best = p.end();
        for (auto it = p.begin(); it != p.end(); ++it) {
            const size_t c = (*it)->E[0].capacity();
            if (c >= pixels && c <= 2 * pixels) { best = it; break; }
            if (best == p.end() || c < (*best)->E[0].capacity()) best = it;
        }
        if (best != p.end()) {
            m_S = std::move(*best);
            p.erase(best);
        } else {
            m_S.reset(new CreativeScratch());
        }
    }
    ~ScratchLease()
    {
        std::lock_guard<std::mutex> lock(mutex());
        if (pool().size() < kKeep && m_S->E[0].capacity() <= 2 * m_Pixels) pool().push_back(std::move(m_S));
    }
    CreativeScratch& operator*() { return *m_S; }
    static void drop()
    {
        std::lock_guard<std::mutex> lock(mutex());
        pool().clear();
    }

private:
    static const size_t kKeep = 2;
    static std::mutex& mutex() { static std::mutex m; return m; }
    static std::vector<std::unique_ptr<CreativeScratch>>& pool()
    {
        static std::vector<std::unique_ptr<CreativeScratch>> p;
        return p;
    }
    size_t m_Pixels;
    std::unique_ptr<CreativeScratch> m_S;
};
}  // namespace

CreativeEffect::~CreativeEffect()
{
    if (--liveNodes() == 0) {   // the last Creative node is gone: its working planes too
        ScratchLease::drop();
#ifdef __APPLE__
        DropCreativeScratch();
#endif
    }
}

void CreativeEffect::render(const OFX::RenderArguments& p_Args)
{
    if (!m_Ready) return;
    if (m_DstClip->getPixelDepth() != OFX::eBitDepthFloat || m_DstClip->getPixelComponents() != OFX::ePixelComponentRGBA)
        OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    std::unique_ptr<OFX::Image> dst(m_DstClip->fetchImage(p_Args.time));
    std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Args.time));
    if (!dst || !src) OFX::throwSuiteStatusException(kOfxStatErrValue);
    const OfxRectI b = src->getBounds();
    const int W = b.x2 - b.x1, H = b.y2 - b.y1;
    if (!sameLayout(*src, *dst)) OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
    int space = 8, gamma = 9;
    if (resolveInput(space, gamma) == InputOrigin::Unknown) {   // neutral node the host renders anyway
        const size_t rowBytes = (size_t)src->getRowBytes();
#ifdef __APPLE__
        if (p_Args.isEnabledMetalRender) {
            if (!RunCopy(p_Args.pMetalCmdQ, rowBytes * H, static_cast<const float*>(src->getPixelData()),
                         static_cast<float*>(dst->getPixelData())))
                OFX::throwSuiteStatusException(kOfxStatFailed);
            return;
        }
#endif
        memcpy(dst->getPixelData(), src->getPixelData(), rowBytes * H);
        return;
    }
    const CreativeControls controls = readControls(p_Args.time);
    // the records' frames: another frame of the clip when the exposures are sequential, this one past its end
    double times[3];
    recordTimes(controls, p_Args.time, times);
    std::unique_ptr<OFX::Image> other[3];
    const float* inputs[3];
    for (int k = 0; k < 3; ++k) {
        inputs[k] = static_cast<const float*>(src->getPixelData());
        if (times[k] == p_Args.time) continue;
        int same = -1;
        for (int j = 0; j < k; ++j)
            if (times[j] == times[k]) same = j;
        if (same >= 0) {
            inputs[k] = inputs[same];
            continue;
        }
        other[k].reset(m_SrcClip->fetchImage(times[k]));
        if (other[k] && sameLayout(*src, *other[k])) inputs[k] = static_cast<const float*>(other[k]->getPixelData());
    }
    // the whole frame arrives (no tiles), so its height is the reference of every radius: the same look at full
    // size, in proxy and in the viewer
    CreativeParams p;
    creativePrepare(p, controls, W, H, (int)std::lround(p_Args.time), space, gamma);
    p.rowPixels = (int)(src->getRowBytes() / 16);
    p.dstRowPixels = (int)(dst->getRowBytes() / 16);
#ifdef __APPLE__
    if (p_Args.isEnabledMetalRender) {
        if (!RunCreativeKernels(p_Args.pMetalCmdQ, p, inputs, static_cast<float*>(dst->getPixelData())))
            OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }
#endif
    // once the host abandons the frame the remaining passes are skipped; it discards the output
    CreativeParallel parallel = [this](int n, const std::function<void(int, int)>& fn) {
        if (abort()) return;
        HostThreads threads(n, fn);
        threads.multiThread();
    };
    ScratchLease scratch((size_t)W * H);
    creativeRenderCPU(p, inputs, (size_t)src->getRowBytes() / 4, static_cast<float*>(dst->getPixelData()),
                      (size_t)dst->getRowBytes() / 4, parallel, *scratch);
}

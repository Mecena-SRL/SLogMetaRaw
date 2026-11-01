// SPDX-License-Identifier: GPL-3.0-or-later
#include "DetailEffect.h"
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
#include "DetailPasses.h"
#include "../common/ClipCache.h"
#include "../common/ColourSpaces.h"
#include "../common/ImageLayout.h"

static const char* const kIntensities[] = { "localContrast", "localHighlights", "localShadows",
                                            "texture", "clarity", "dehaze" };
// the intensities first, in the order of kIntensities
static const struct { const char* name; double SMDetailControls::*field; } kDetailSliders[kDetailSliderCount] = {
    { "localContrast", &SMDetailControls::localContrast }, { "localHighlights", &SMDetailControls::localHighlights },
    { "localShadows", &SMDetailControls::localShadows }, { "texture", &SMDetailControls::texture },
    { "clarity", &SMDetailControls::clarity }, { "dehaze", &SMDetailControls::dehaze },
    { "preserveDetail", &SMDetailControls::preserveDetail }, { "detailRadius", &SMDetailControls::detailRadius },
    { "edgeThreshold", &SMDetailControls::edgeThreshold }, { "noiseThreshold", &SMDetailControls::noiseThreshold },
    { "clarityCenter", &SMDetailControls::clarityCenter }, { "hazeLevel", &SMDetailControls::hazeLevel },
    { "hazeWarmth", &SMDetailControls::hazeWarmth }, { "localWhite", &SMDetailControls::localWhite },
    { "ratioLight", &SMDetailControls::ratioLight }, { "ratioShadow", &SMDetailControls::ratioShadow },
    { "ratioMeasured", &SMDetailControls::ratioMeasured }, { "ratioTarget", &SMDetailControls::ratioTarget },
    { "ratioHardness", &SMDetailControls::ratioHardness }, { "ratioBackground", &SMDetailControls::ratioBackground },
    { "ratioSubjectLift", &SMDetailControls::ratioSubjectLift } };
// shown only in Cinematografico
static const char* const kCinemaParams[] = { "cinemaGroup", "subjectFace", "subjectLandscape", "ratioLightPos",
                                             "ratioShadowPos", "ratioMeasure", "ratioTarget", "ratioHardness",
                                             "ratioBackground", "ratioSubjectLift", "ratioInfo" };
// what Cinematografico replaces: greyed out there
static const char* const kStandardOnly[] = { "localContrast", "localHighlights", "localShadows" };
static const int kIntensityCount = sizeof(kIntensities) / sizeof(kIntensities[0]);

static std::atomic<int>& liveNodes() { static std::atomic<int> n(0); return n; }

DetailEffect::DetailEffect(OfxImageEffectHandle p_Handle)
    : ImageEffect(p_Handle)
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
        m_Ready = true;
    } catch (...) {
        fprintf(stderr, "S-Log MetaRaw Detail: nodo disattivato, parametro mancante\n");
        return;
    }
    // fetched on the main thread (a first fetch while rendering races with the UI); a missing one reads its default
    try {
        m_Zones.bind(*this, "localZone");
        for (int i = 0; i < kDetailSliderCount; ++i)
            if (paramExists(kDetailSliders[i].name)) m_Sliders[i] = fetchDoubleParam(kDetailSliders[i].name);
        if (paramExists("viewGain")) m_ViewGain = fetchBooleanParam("viewGain");
        if (paramExists("viewBase")) m_ViewBase = fetchBooleanParam("viewBase");
        if (paramExists("devMode")) m_DevMode = fetchChoiceParam("devMode");
        if (paramExists("destination")) m_Destination = fetchChoiceParam("destination");
        if (paramExists("subjectFace")) m_SubjectFace = fetchBooleanParam("subjectFace");
        if (paramExists("subjectLandscape")) m_SubjectLandscape = fetchBooleanParam("subjectLandscape");
    } catch (...) {
    }
    try {
        if (m_SettingsVersion->getValue() == 0) m_SettingsVersion->setValue(kDetailSettingsVersion);
        syncClip();
        updateMode();
        m_Badge.attach(*this);
    } catch (...) {
    }
}

std::string DetailEffect::sourcePath() const
{
    return getPropertySet().propGetString(kOfxImageEffectPropSrcFilePath, false);
}

void DetailEffect::syncClip()
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

// Declared, then what Resolve reports, then the clip's camera encoding (YRGB: the node after the
// develop still receives it). An unrecognised colourspace from the host leaves the node neutral.
InputOrigin DetailEffect::resolveInput(int& space, int& gamma) const
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

void DetailEffect::refreshInfo()
{
    int space = 0, gamma = 0;
    const InputOrigin origin = resolveInput(space, gamma);
    std::string text = "Spaziale · escluso da Generate LUT · Ingresso: ";
    if (origin == InputOrigin::Unknown) {
        text += "non riconosciuto, nodo neutro · scegli Sviluppo e spazio colore › Ingresso (IDT)";
    } else {
        static const char* const from[] = { "dichiarato", "da Resolve", "dalla clip", "presunto" };
        text += std::string(kSpaceNames[space]) + " / " + kGammaNames[gamma] + " (" + from[(int)origin] + ")";
    }
    std::string cur;
    m_Info->getValue(cur);
    if (cur != text) m_Info->setValue(text);
}

bool DetailEffect::neutralAt(double p_Time) const
{
    int space = 0, gamma = 0;
    if (resolveInput(space, gamma) == InputOrigin::Unknown) return true;
    for (int i = 0; i < kIntensityCount; ++i)
        if (m_Sliders[i] && m_Sliders[i]->getValueAtTime(p_Time) != 0.0) return false;
    const ZoneValues z = m_Zones.read(p_Time);
    for (int i = 0; i < kZoneCount; ++i)
        if (z.exp[i] != 0.0) return false;
    const SMDetailControls c = readControls(p_Time);
    if (c.devMode == SM_DT_MODE_CINEMA && c.ratioLight - c.ratioShadow > SM_DT_RATIO_MIN_GAP) return false;
    return !(m_ViewGain && m_ViewGain->getValueAtTime(p_Time)) && !(m_ViewBase && m_ViewBase->getValueAtTime(p_Time));
}

void DetailEffect::resetAll()
{
    for (const char* name : kIntensities) {
        OFX::DoubleParam* p = fetchDoubleParam(name);
        p->deleteAllKeys();
        p->setValue(0.0);
    }
    resetZoneParams(*this, "localZone");
    const struct { const char* name; double v; } cinema[] = { { "ratioBackground", 0.0 }, { "ratioSubjectLift", 0.0 },
                                                             { "ratioLight", 0.0 }, { "ratioShadow", 0.0 },
                                                             { "ratioMeasured", 0.0 }, { "ratioTarget", 1.0 } };
    for (const auto& c : cinema)
        if (paramExists(c.name)) {
            OFX::DoubleParam* p = fetchDoubleParam(c.name);
            p->deleteAllKeys();
            p->setValue(c.v);
        }
    refreshRatioInfo();
}

void DetailEffect::updateMode()
{
    if (!m_DevMode) return;
    int mode = 0;
    m_DevMode->getValue(mode);
    const bool cinema = mode == SM_DT_MODE_CINEMA;
    for (const char* name : kCinemaParams)
        if (paramExists(name)) getParam(name)->setIsSecret(!cinema);
    for (const char* name : kStandardOnly)
        if (paramExists(name)) getParam(name)->setEnabled(!cinema);
    if (paramExists("modeInfo")) {
        OFX::StringParam* info = fetchStringParam("modeInfo");
        std::string cur;
        info->getValue(cur);
        const std::string text = kDetailModeInfo[cinema ? 1 : 0];
        if (cur != text) info->setValue(text);
    }
    if (cinema) refreshRatioInfo();
}

static std::string ratioText(double stops)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "1:%.1f (%.2f stop)", exp2(stops), stops);
    return buf;
}

void DetailEffect::refreshRatioInfo()
{
    if (!paramExists("ratioInfo")) return;
    const SMDetailControls c = readControls(0.0);
    std::string text;
    if (!(c.ratioLight - c.ratioShadow > SM_DT_RATIO_MIN_GAP)) {
        text = "Scegli i due punti e premi Misura rapporto";
    } else {
        const double reached = dt_cinema_reached(&c);
        text = "Misurato " + ratioText(c.ratioMeasured) + " · Obiettivo " + ratioText(c.ratioTarget);
        if (fabs(reached - c.ratioTarget) > 0.05) text += " · raggiungibile " + ratioText(reached);
        const double white = dt_destination_white(&c);
        if (c.ratioLight + (c.ratioSubjectLift / 100.0) > white)
            text += " · luce oltre il bianco della destinazione";
    }
    OFX::StringParam* info = fetchStringParam("ratioInfo");
    std::string cur;
    info->getValue(cur);
    if (cur != text) info->setValue(text);
}

// Misura: the mean linear colour around each point (1 % of the height), from the node's input at this time.
// A face is read as luminance Y (Rec.2020), a landscape as the norm the node works on; the edges of the
// curve always use the norm. Writes the hidden values and, unless asked to keep it, the target.
void DetailEffect::measureRatio(double p_Time, bool p_KeepTarget)
{
    int space = 8, gamma = 9;
    if (resolveInput(space, gamma) == InputOrigin::Unknown) return;
    std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Time));
    if (!src || src->getPixelDepth() != OFX::eBitDepthFloat || src->getPixelComponents() != OFX::ePixelComponentRGBA)
        return;
    const OfxRectI b = src->getBounds();
    const int W = b.x2 - b.x1, H = b.y2 - b.y1;
    if (W <= 0 || H <= 0) return;
    const SMDetailControls rest = sm_detail_defaults();
    DetailParams q;
    dt_prepare(&q, W, H, H, 1.0, space, gamma, &rest, SM_DT_GRID_BASE);
    const OfxRectD rod = m_SrcClip->getRegionOfDefinition(p_Time);
    const char* const names[2] = { "ratioLightPos", "ratioShadowPos" };
    double Ln[2] = { 0.0, 0.0 }, Ly[2] = { 0.0, 0.0 };
    for (int k = 0; k < 2; ++k) {
        double x = 0.5, y = 0.5;
        fetchDouble2DParam(names[k])->getValueAtTime(p_Time, x, y);
        // canonical coordinates as the spec says; a host that hands back the normalised value is read as such
        if (!(x <= 1.0 && y <= 1.0) && rod.x2 > rod.x1 && rod.y2 > rod.y1) {
            x = (x - rod.x1) / (rod.x2 - rod.x1);
            y = (y - rod.y1) / (rod.y2 - rod.y1);
        }
        const int cx = (int)lround(sm_clampd(x, 0, 1) * (W - 1)), cy = (int)lround(sm_clampd(y, 0, 1) * (H - 1));
        const int r = std::max(2, (int)lround(0.01 * H));
        double acc[3] = { 0, 0, 0 };
        int n = 0;
        for (int j = std::max(0, cy - r); j <= std::min(H - 1, cy + r); ++j)
            for (int i = std::max(0, cx - r); i <= std::min(W - 1, cx + r); ++i) {
                const float* px = static_cast<const float*>(src->getPixelAddress(b.x1 + i, b.y1 + j));
                if (!px) continue;
                const SMf3 v = smf3(sm_decode(px[0], gamma), sm_decode(px[1], gamma), sm_decode(px[2], gamma));
                if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) continue;
                acc[0] += v.x; acc[1] += v.y; acc[2] += v.z;
                ++n;
            }
        if (n == 0) return;
        double rgb[3];
        for (int c = 0; c < 3; ++c) {
            const float* m = q.toRef + c * 3;
            rgb[c] = (m[0] * acc[0] + m[1] * acc[1] + m[2] * acc[2]) / n;
        }
        Ln[k] = sm_t5_log(sm_t5_norm(smf3((float)rgb[0], (float)rgb[1], (float)rgb[2])));
        const double Y = 0.2627 * rgb[0] + 0.6780 * rgb[1] + 0.0593 * rgb[2];
        Ly[k] = log2(std::max(Y, (double)SM_T5_EPS) / SM_T5_GREY);
    }
    if (Ln[0] < Ln[1]) { std::swap(Ln[0], Ln[1]); std::swap(Ly[0], Ly[1]); }   // points given the other way round
    const bool face = !(m_SubjectLandscape && m_SubjectLandscape->getValue());
    const double measured = std::max(face ? Ly[0] - Ly[1] : Ln[0] - Ln[1], 0.0);
    fetchDoubleParam("ratioLight")->setValue(Ln[0]);
    fetchDoubleParam("ratioShadow")->setValue(Ln[1]);
    fetchDoubleParam("ratioMeasured")->setValue(measured);
    if (!p_KeepTarget) fetchDoubleParam("ratioTarget")->setValue(std::min(measured, 5.0));
    refreshRatioInfo();
}

void DetailEffect::beginEdit()
{
    if (!m_Ready) return;
    try {
        syncClip();
        updateMode();
        m_Badge.refresh(true);
    } catch (...) {
    }
}

void DetailEffect::changedClip(const OFX::InstanceChangedArgs&, const std::string& p_ClipName)
{
    if (!m_Ready) return;
    if (p_ClipName == kOfxImageEffectSimpleSourceClipName) syncClip();
    m_Badge.refresh(false);
}

void DetailEffect::changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName)
{
    if (!m_Ready) return;
    if (p_ParamName == "version") {
        m_Badge.clicked();
        return;
    }
    if (p_ParamName == "detailReset") resetAll();
    else if (p_ParamName == "nodeInput") refreshInfo();
    else if (p_ParamName == "devMode") updateMode();
    else if (p_ParamName == "ratioMeasure") measureRatio(p_Args.time, false);
    else if ((p_ParamName == "subjectFace" || p_ParamName == "subjectLandscape") && m_SubjectFace && m_SubjectLandscape) {
        // one subject at a time; none ticked reads as a face. A measure is read again in the new metric.
        OFX::BooleanParam* other = p_ParamName == "subjectFace" ? m_SubjectLandscape : m_SubjectFace;
        if (fetchBooleanParam(p_ParamName)->getValue() && other->getValue()) other->setValue(false);
        if (p_Args.reason == OFX::eChangeUserEdit) {
            const SMDetailControls c = readControls(p_Args.time);
            if (c.ratioLight - c.ratioShadow > SM_DT_RATIO_MIN_GAP) measureRatio(p_Args.time, true);
        }
    } else if (p_ParamName == "destination" || p_ParamName.compare(0, 5, "ratio") == 0) refreshRatioInfo();
    else if (p_ParamName == "viewGain" && fetchBooleanParam("viewGain")->getValue())
        fetchBooleanParam("viewBase")->setValue(false);
    else if (p_ParamName == "viewBase" && fetchBooleanParam("viewBase")->getValue())
        fetchBooleanParam("viewGain")->setValue(false);
    m_Badge.refresh(false);
}

bool DetailEffect::isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip, double& p_IdentityTime)
{
    if (m_Ready && !neutralAt(p_Args.time)) return false;
    p_IdentityClip = m_SrcClip;
    p_IdentityTime = p_Args.time;
    return true;
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

// Working planes shared by every Detail node, at most kKeep sets. A frame takes the set sized for it;
// a set more than twice its size is freed on release, so an 8K render does not pin memory for HD work.
class ScratchLease
{
public:
    explicit ScratchLease(size_t pixels) : m_Pixels(pixels)
    {
        std::lock_guard<std::mutex> lock(mutex());
        auto& p = pool();
        auto best = p.end();
        for (auto it = p.begin(); it != p.end(); ++it) {
            const size_t c = (*it)->L0.capacity();
            if (c >= pixels && c <= 2 * pixels) { best = it; break; }
            if (best == p.end() || c < (*best)->L0.capacity()) best = it;
        }
        if (best != p.end()) {
            m_S = std::move(*best);
            p.erase(best);
        } else {
            m_S.reset(new DetailScratch());
        }
    }
    ~ScratchLease()
    {
        std::lock_guard<std::mutex> lock(mutex());
        if (pool().size() < kKeep && m_S->L0.capacity() <= 2 * m_Pixels) pool().push_back(std::move(m_S));
    }
    DetailScratch& operator*() { return *m_S; }
    static void drop()
    {
        std::lock_guard<std::mutex> lock(mutex());
        pool().clear();
    }

private:
    static const size_t kKeep = 2;
    static std::mutex& mutex() { static std::mutex m; return m; }
    static std::vector<std::unique_ptr<DetailScratch>>& pool() { static std::vector<std::unique_ptr<DetailScratch>> p; return p; }
    size_t m_Pixels;
    std::unique_ptr<DetailScratch> m_S;
};
}

DetailEffect::~DetailEffect()
{
    if (--liveNodes() == 0) {   // the last Detail node is gone: its working planes too
        ScratchLease::drop();
#ifdef __APPLE__
        DropDetailScratch();
#endif
    }
}

SMDetailControls DetailEffect::readControls(double t) const
{
    SMDetailControls c = sm_detail_defaults();
    for (int i = 0; i < kDetailSliderCount; ++i)
        if (m_Sliders[i]) c.*(kDetailSliders[i].field) = m_Sliders[i]->getValueAtTime(t);
    const ZoneValues z = m_Zones.read(t);
    c.zonePivot = z.pivot;
    for (int i = 0; i < kZoneCount; ++i) {
        c.zoneExp[i] = z.exp[i];
        c.zoneRange[i] = z.range[i];
        c.zoneFalloff[i] = z.falloff[i];
    }
    c.viewGain = m_ViewGain && m_ViewGain->getValueAtTime(t) ? 1 : 0;
    c.viewBase = m_ViewBase && m_ViewBase->getValueAtTime(t) ? 1 : 0;
    if (m_DevMode) m_DevMode->getValueAtTime(t, c.devMode);
    if (m_Destination) m_Destination->getValueAtTime(t, c.destination);
    c.subject = m_SubjectLandscape && m_SubjectLandscape->getValueAtTime(t) ? 1 : 0;
    return c;
}

void DetailEffect::render(const OFX::RenderArguments& p_Args)
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
    // The whole frame arrives (no tiles), so its height is the reference of every radius: the same
    // look at full size, in proxy and in the viewer.
    const SMDetailControls controls = readControls(p_Args.time);
    DetailParams p;
    dt_prepare(&p, W, H, H, src->getPixelAspectRatio(), space, gamma, &controls, SM_DT_GRID_BASE);
#ifdef __APPLE__
    if (p_Args.isEnabledMetalRender) {
        if (!RunDetailKernels(p_Args.pMetalCmdQ, p, src->getRowBytes() / 16, static_cast<const float*>(src->getPixelData()),
                              static_cast<float*>(dst->getPixelData())))
            OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }
#endif
    // once the host abandons the frame the remaining passes are skipped; it discards the output
    Parallel parallel = [this](int n, const std::function<void(int, int)>& fn) {
        if (abort()) return;
        HostThreads threads(n, fn);
        threads.multiThread();
    };
    ScratchLease scratch((size_t)p.W * p.H);
    detailRenderCPU(p, static_cast<const float*>(src->getPixelData()), (size_t)src->getRowBytes() / 4,
                    static_cast<float*>(dst->getPixelData()), (size_t)dst->getRowBytes() / 4, parallel, *scratch);
}

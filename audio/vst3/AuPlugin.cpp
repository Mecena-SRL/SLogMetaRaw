// SPDX-License-Identifier: GPL-3.0-or-later
// Log MetaRaw Audio as VST3: two plugins in one module, «Ripresa» and «Mix e supporto», each a processor (moves
// samples through AuEngine), a controller (parameters, saved state) and a panel drawn from the rows in AuParams.h.
#include "AuParams.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/utility/stringconvert.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/vstgui.h"
#if SMTG_OS_LINUX
#include "public.sdk/source/vst/vstgui_linux_runloop_support.h"
#include "vstgui/lib/platform/linux/x11frame.h"
#include "vstgui/lib/platform/linux/linuxfactory.h"
#endif

#include <algorithm>
#include <cmath>
#include <random>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

const FUID kRipresaProcessor(0x5F870183, 0x78DA4541, 0x9978C6CF, 0x4B42F5FE);
const FUID kRipresaController(0x6280EBE2, 0x1D3A4C5A, 0x91CCAC6D, 0x1C6EAA1C);
const FUID kMixProcessor(0xCED29DCE, 0x878342A9, 0x851A42EC, 0x3AFCB1A3);
const FUID kMixController(0x70242F10, 0x6FF2474C, 0x9A1DF4AE, 0x84B86BE8);

constexpr int32 kStateMagic = 0x41524D4C;   // "LMRA"
constexpr int32 kStateVersion = 1;

// Normalised ↔ plain for one parameter of one plugin (lists: index; switches: 0/1).
struct Scale
{
    double min = 0, max = 1;
    int steps = 0;   // 0 = continuous
    double plain(double n) const
    {
        double v = min + std::clamp(n, 0.0, 1.0) * (max - min);
        return steps ? std::round(v) : v;
    }
    double norm(double v) const { return max > min ? std::clamp((v - min) / (max - min), 0.0, 1.0) : 0; }
};
Scale scaleOf(AuParamId id, bool mix)
{
    const AuParamDef& d = auParam(id);
    switch (d.kind) {
    case AuKind::List: return { 0, double(auListNames(id, mix).size() - 1), 1 };
    case AuKind::Toggle: return { 0, 1, 1 };
    default: return { d.min, d.max, id == kPCopies ? 1 : 0 };
    }
}

// The saved state, shared by processor and controller: plain values by id; the recorder as family and model, so a
// recorder appended to a family does not shift the saved ones.
void writeState(IBStream* stream, bool mix, const double* v, uint32 seed)
{
    IBStreamer s(stream, kLittleEndian);
    s.writeInt32(kStateMagic);
    s.writeInt32(kStateVersion);
    s.writeInt32u(seed);
    auto ids = auParams(mix);
    ids.erase(std::remove_if(ids.begin(), ids.end(), [](AuParamId id) { return auParam(id).kind == AuKind::Meter; }),
              ids.end());   // levels are read, not saved
    s.writeInt32(int32(ids.size()));
    for (AuParamId id : ids) {
        double value = v[id];
        if (id == kPDevice) {
            int family, model;
            auDeviceSplit(int(value), family, model);
            value = family * 1000 + model;
        }
        s.writeInt32(id);
        s.writeDouble(value);
    }
}
bool readState(IBStream* stream, double* v, uint32& seed)
{
    IBStreamer s(stream, kLittleEndian);
    int32 magic = 0, version = 0, count = 0;
    if (!s.readInt32(magic) || magic != kStateMagic || !s.readInt32(version) || version > kStateVersion ||
        !s.readInt32u(seed) || !s.readInt32(count))
        return false;
    for (int32 i = 0; i < count; ++i) {
        int32 id;
        double value;
        if (!s.readInt32(id) || !s.readDouble(value)) return false;
        if (id < 0 || id >= kPCount) continue;   // a control from a newer version
        if (id == kPDevice) value = auDeviceIndex(int(value) / 1000, int(value) % 1000);
        v[id] = value;
    }
    return true;
}

void defaults(double* v, bool mix)
{
    for (int i = 0; i < kPCount; ++i) v[i] = auDefault(AuParamId(i), mix);
}

// ---- processor

class Processor : public AudioEffect
{
public:
    Processor(bool mix) : mix_(mix)
    {
        setControllerClass(mix ? kMixController : kRipresaController);
        defaults(v_, mix);
        seed_ = std::random_device {}() | 1u;   // each instance its own hiss: two tracks do not share noise
        for (int i = 0; i < kPCount; ++i) scale_[i] = scaleOf(AuParamId(i), mix);
    }
    static FUnknown* ripresa(void*) { return static_cast<IAudioProcessor*>(new Processor(false)); }
    static FUnknown* mixer(void*) { return static_cast<IAudioProcessor*>(new Processor(true)); }

    tresult PLUGIN_API initialize(FUnknown* context) override
    {
        tresult r = AudioEffect::initialize(context);
        if (r != kResultOk) return r;
        addAudioInput(STR16("Ingresso"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Uscita"), SpeakerArr::kStereo);
        return kResultOk;
    }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* in, int32 nIn, SpeakerArrangement* out,
                                          int32 nOut) override
    {
        if (nIn != 1 || nOut != 1 || in[0] != out[0] || SpeakerArr::getChannelCount(in[0]) > 2)
            return kResultFalse;
        return AudioEffect::setBusArrangements(in, nIn, out, nOut);
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override
    {
        return size == kSample32 ? kResultTrue : kResultFalse;
    }
    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override
    {
        fs_ = setup.sampleRate;
        return AudioEffect::setupProcessing(setup);
    }
    tresult PLUGIN_API setActive(TBool state) override
    {
        if (state) {
            BusList* outs = getBusList(kAudio, kOutput);
            channels_ = outs && !outs->empty() ? SpeakerArr::getChannelCount(static_cast<AudioBus*>(outs->at(0).get())->getArrangement()) : 2;
            engine_.prepare(fs_, channels_, auSettings(v_, mix_, seed_));
            engine_.reset();
            dirty_ = false;
        }
        return AudioEffect::setActive(state);
    }

    tresult PLUGIN_API process(ProcessData& data) override
    {
        if (IParameterChanges* changes = data.inputParameterChanges)
            for (int32 i = 0; i < changes->getParameterCount(); ++i) {
                IParamValueQueue* q = changes->getParameterData(i);
                int32 offset, points = q ? q->getPointCount() : 0;
                ParamValue value;
                ParamID id = q ? q->getParameterId() : 0;
                if (points > 0 && id < kPCount && q->getPoint(points - 1, offset, value) == kResultTrue) {
                    v_[id] = scale_[id].plain(value);
                    dirty_ = true;
                }
            }
        if (dirty_) {   // a new chain keeps the running state where its shape is unchanged (no click)
            engine_.prepare(fs_, channels_, auSettings(v_, mix_, seed_));
            dirty_ = false;
        }
        if (data.numInputs < 1 || data.numOutputs < 1 || data.numSamples <= 0) return kResultOk;
        AudioBusBuffers& in = data.inputs[0];
        AudioBusBuffers& out = data.outputs[0];
        const int n = std::min<int>(std::min(in.numChannels, out.numChannels), channels_);
        for (int c = 0; c < n; ++c)
            if (in.channelBuffers32[c] != out.channelBuffers32[c])
                std::copy(in.channelBuffers32[c], in.channelBuffers32[c] + data.numSamples, out.channelBuffers32[c]);
        if (v_[kPBypass] < 0.5 && n == channels_) engine_.process(out.channelBuffers32, data.numSamples);
        out.silenceFlags = 0;
        if (IParameterChanges* levels = data.outputParameterChanges; levels && n > 0) {   // for the panel's meters
            const bool peak = auMeterPeak(v_, mix_);
            for (int c = 0; c < 2; ++c) {
                const float* x = out.channelBuffers32[std::min(c, n - 1)];
                double level = 0;
                for (int32 i = 0; i < data.numSamples; ++i)
                    level = peak ? std::max(level, double(std::fabs(x[i]))) : level + double(x[i]) * x[i];
                if (!peak) level = std::sqrt(level / data.numSamples);
                int32 index = 0;
                if (IParamValueQueue* q = levels->addParameterData(ParamID(kPMeterL + c), index))
                    q->addPoint(0, std::min(1.0, level / 2), index);   // 0.5 = full scale
            }
        }
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream* state) override
    {
        writeState(state, mix_, v_, seed_);
        return kResultOk;
    }
    tresult PLUGIN_API setState(IBStream* state) override
    {
        double v[kPCount];
        defaults(v, mix_);
        uint32 seed = seed_;
        if (!readState(state, v, seed)) return kResultFalse;
        std::copy(v, v + kPCount, v_);
        seed_ = seed;
        dirty_ = true;
        return kResultOk;
    }

private:
    bool mix_;
    double v_[kPCount];
    Scale scale_[kPCount];
    uint32 seed_;
    double fs_ = 48000;
    int channels_ = 2;
    bool dirty_ = true;
    AuEngine engine_;
};

// ---- panel

using namespace VSTGUI;

// The look of a front plate: body (top and bottom of its gradient), name plate and its lettering, meter face, knob
// caps. Drawn, not photographed: every family of recorder and every desk gets an original plate in the materials of
// its era, with no maker's marks.
struct Skin
{
    CColor body, body2, plate, ink, face, cap;
    bool brushed;   // the fine grain of brushed metal
};

const Skin kFamilySkins[] = {   // Ripresa, by family of recorder (kAuFamilies)
    { { 186, 188, 190 }, { 128, 130, 133 }, { 26, 26, 28 }, { 228, 224, 212 }, { 238, 230, 208 }, { 44, 44, 46 }, true },
    { { 198, 192, 178 }, { 156, 150, 136 }, { 70, 66, 60 }, { 240, 232, 214 }, { 240, 232, 210 }, { 62, 58, 54 }, false },
    { { 52, 52, 56 }, { 22, 22, 24 }, { 176, 178, 182 }, { 22, 22, 24 }, { 226, 232, 236 }, { 168, 170, 174 }, true },
    { { 68, 64, 62 }, { 38, 36, 35 }, { 206, 124, 50 }, { 26, 20, 14 }, { 232, 224, 200 }, { 30, 30, 30 }, false },
    { { 124, 78, 42 }, { 76, 42, 20 }, { 198, 162, 94 }, { 50, 32, 12 }, { 238, 226, 192 }, { 172, 138, 74 }, false },
};
const Skin kDeskSkins[] = {   // Mix e supporto, by desk (kAuConsoles)
    { { 74, 76, 80 }, { 44, 46, 50 }, { 30, 30, 32 }, { 214, 210, 200 }, { 232, 226, 206 }, { 52, 52, 54 }, false },
    { { 116, 130, 142 }, { 78, 92, 104 }, { 30, 34, 38 }, { 220, 224, 226 }, { 238, 230, 206 }, { 40, 44, 48 }, false },
    { { 122, 130, 118 }, { 84, 92, 82 }, { 226, 222, 206 }, { 30, 32, 28 }, { 236, 228, 204 }, { 56, 60, 54 }, false },
    { { 100, 112, 130 }, { 64, 76, 92 }, { 202, 198, 186 }, { 28, 30, 34 }, { 236, 230, 212 }, { 120, 44, 40 }, false },
    { { 62, 64, 68 }, { 34, 35, 38 }, { 214, 214, 214 }, { 24, 24, 26 }, { 230, 232, 232 }, { 92, 96, 100 }, false },
    { { 56, 56, 58 }, { 30, 30, 32 }, { 222, 222, 222 }, { 20, 20, 22 }, { 224, 228, 230 }, { 40, 40, 42 }, true },
};

CColor shade(CColor c, double k, uint8_t alpha = 255)   // k < 1 darker, > 1 lighter
{
    auto ch = [k](uint8_t v) { return uint8_t(std::clamp(v * k, 0.0, 255.0)); };
    return CColor(ch(c.red), ch(c.green), ch(c.blue), alpha);
}
void fillGradient(CDrawContext* c, CGraphicsPath* path, const CRect& r, CColor top, CColor bottom)
{
    auto g = VSTGUI::owned(CGradient::create(0, 1, top, bottom));
    c->fillLinearGradient(path, *g, r.getTopLeft(), r.getBottomLeft());
}
void fillEllipse(CDrawContext* c, const CRect& r, CColor top, CColor bottom)
{
    auto path = VSTGUI::owned(c->createGraphicsPath());
    path->addEllipse(r);
    fillGradient(c, path, r, top, bottom);
}

// The plate behind the meters and knobs: the body, its screws and the engraved name plate.
class Plate : public CView
{
public:
    explicit Plate(const CRect& r) : CView(r) {}
    void set(const Skin& skin, const std::string& brand, const std::string& name, const std::string& kind)
    {
        skin_ = skin, brand_ = brand, name_ = name, kind_ = kind;
        invalid();
    }
    void draw(CDrawContext* c) override
    {
        const CRect r = getViewSize();
        auto body = VSTGUI::owned(c->createRoundRectGraphicsPath(r, 8));
        fillGradient(c, body, r, skin_.body, skin_.body2);
        c->setLineWidth(1);
        if (skin_.brushed)
            for (CCoord y = r.top + 3; y < r.bottom - 3; y += 2) {
                c->setFrameColor(CColor(255, 255, 255, uint8_t(6 + (int(y) * 37) % 11)));
                c->drawLine(CPoint(r.left + 3, y), CPoint(r.right - 3, y));
            }
        c->setFrameColor(shade(skin_.body2, 0.6));
        c->drawGraphicsPath(body, CDrawContext::kPathStroked);
        for (CPoint p : { CPoint(r.left + 10, r.top + 10), CPoint(r.right - 10, r.top + 10),
                          CPoint(r.left + 10, r.bottom - 10), CPoint(r.right - 10, r.bottom - 10) }) {
            CRect s(p.x - 4.5, p.y - 4.5, p.x + 4.5, p.y + 4.5);
            fillEllipse(c, s, shade(skin_.body, 1.25), shade(skin_.body2, 0.7));
            c->setFrameColor(shade(skin_.body2, 0.45));
            c->drawLine(CPoint(s.left + 2, s.bottom - 3), CPoint(s.right - 2, s.top + 3));
        }
        CRect np(r.left + 18, r.top + 14, r.left + 256, r.top + 70);
        auto plate = VSTGUI::owned(c->createRoundRectGraphicsPath(np, 4));
        fillGradient(c, plate, np, shade(skin_.plate, 1.12), shade(skin_.plate, 0.88));
        c->setFrameColor(shade(skin_.plate, 0.6));
        c->drawGraphicsPath(plate, CDrawContext::kPathStroked);
        c->setFontColor(skin_.ink);
        c->setFont(kNormalFontSmaller);
        c->drawString(brand_.c_str(), CRect(np.left + 10, np.top + 4, np.right - 10, np.top + 16), kLeftText);
        c->setFont(kNormalFont);
        c->drawString(name_.c_str(), CRect(np.left + 10, np.top + 18, np.right - 10, np.top + 36), kLeftText);
        c->setFont(kNormalFontSmaller);
        c->drawString(kind_.c_str(), CRect(np.left + 10, np.top + 38, np.right - 10, np.top + 52), kLeftText);
        setDirty(false);
    }

private:
    Skin skin_ = kDeskSkins[0];
    std::string brand_, name_, kind_;
};

// A moving-needle meter. A VU reads the average level and settles in about 300 ms, its scale linear in voltage with
// 0 VU at 71 %; a PPM rises at once and falls 24 dB in 2.8 s, its seven marks 4 dB apart with 4 at the reference.
class Meter : public CView
{
public:
    Meter(const CRect& r, const char* channel) : CView(r), channel_(channel) {}
    void setLook(const Skin& skin, bool peak)
    {
        face_ = skin.face, peak_ = peak;
        invalid();
    }
    void feed(double db, double dt)   // db: the level against the calibration (0 = reference)
    {
        const double target = position(db);
        if (!peak_) pos_ += (target - pos_) * (1 - std::exp(-dt / 0.09));
        else if (target > pos_) pos_ += (target - pos_) * (1 - std::exp(-dt / 0.004));
        else pos_ = std::max(target, pos_ - dt * (24 / 2.8) / 24);
        lamp_ = db > (peak_ ? 8 : 3) ? 0.6 : std::max(0.0, lamp_ - dt);   // overload lamp, held a moment
        invalid();
    }
    void draw(CDrawContext* c) override
    {
        const CRect r = getViewSize();
        auto bezel = VSTGUI::owned(c->createRoundRectGraphicsPath(r, 6));
        fillGradient(c, bezel, r, CColor(54, 52, 50), CColor(16, 16, 16));
        CRect face = r;
        face.inset(7, 7);
        auto facePath = VSTGUI::owned(c->createRoundRectGraphicsPath(face, 3));
        fillGradient(c, facePath, face, shade(face_, 1.04), shade(face_, 0.86));
        const CPoint pivot(face.getCenter().x, face.bottom - 8);   // hidden under the bridge
        const double radius = face.getHeight() - 38;
        auto at = [&](double t, double rad) {
            const double a = (-43 + 86 * t) * 3.14159265358979 / 180;
            return CPoint(pivot.x + std::sin(a) * rad, pivot.y - std::cos(a) * rad);
        };
        const CColor ink(34, 30, 26), red(186, 40, 30);
        auto arc = [&](double from, double to, CColor colour, double width) {
            auto path = VSTGUI::owned(c->createGraphicsPath());
            path->beginSubpath(at(from, radius));
            for (int i = 1; i <= 24; ++i) path->addLine(at(from + (to - from) * i / 24, radius));
            c->setFrameColor(colour);
            c->setLineWidth(width);
            c->drawGraphicsPath(path, CDrawContext::kPathStroked);
        };
        const double zero = position(0);
        arc(0, zero, ink, 1.2);
        arc(zero, 1, red, 3);
        c->setFont(kNormalFontSmaller);
        auto mark = [&](double t, const char* label, bool over) {
            c->setFrameColor(over ? red : ink);
            c->setLineWidth(label ? 1.4 : 1);
            c->drawLine(at(t, radius), at(t, radius + (label ? 8 : 5)));
            if (!label) return;
            const CPoint p = at(t, radius + 16);
            c->setFontColor(over ? red : ink);
            c->drawString(label, CRect(p.x - 12, p.y - 6, p.x + 12, p.y + 6), kCenterText);
        };
        if (peak_)
            for (int m = 1; m <= 7; ++m) {
                const char label[2] = { char('0' + m), 0 };
                mark((m - 1) / 6.0, label, m > 4);
            }
        else {
            const struct { int db; const char* label; } marks[] = {
                { -20, "20" }, { -10, "10" }, { -7, nullptr }, { -5, "5" }, { -3, "3" }, { -2, nullptr },
                { -1, nullptr }, { 0, "0" }, { 1, nullptr }, { 2, nullptr }, { 3, "+3" },
            };
            for (auto m : marks) mark(position(m.db), m.label, m.db > 0);
        }
        c->setFontColor(ink);
        c->setFont(kNormalFont);
        c->drawString(peak_ ? "PPM" : "VU", CRect(face.left, face.top + face.getHeight() * 0.5, face.right,
                                                  face.top + face.getHeight() * 0.5 + 16), kCenterText);
        c->setFont(kNormalFontSmaller);
        c->drawString(channel_, CRect(face.left + 6, face.bottom - 30, face.left + 20, face.bottom - 18), kLeftText);
        CRect clip;   // the needle stays behind the glass
        c->getClipRect(clip);
        c->setClipRect(face);
        c->setFrameColor(CColor(20, 18, 16));
        c->setLineWidth(1.6);
        c->drawLine(pivot, at(std::clamp(pos_, -0.03, 1.06), radius + 10));
        c->setClipRect(clip);
        CRect bridge(face.left, face.bottom - 16, face.right, face.bottom);
        auto bridgePath = VSTGUI::owned(c->createRoundRectGraphicsPath(bridge, 2));
        fillGradient(c, bridgePath, bridge, CColor(44, 42, 40), CColor(20, 20, 20));
        CRect lampRect(face.right - 16, face.bottom - 12, face.right - 8, face.bottom - 4);
        fillEllipse(c, lampRect, lamp_ > 0 ? CColor(255, 90, 60) : CColor(90, 24, 18),
                    lamp_ > 0 ? CColor(220, 30, 20) : CColor(50, 14, 10));
        CRect glass(face.left, face.top, face.right, face.top + face.getHeight() * 0.45);
        auto glassPath = VSTGUI::owned(c->createRoundRectGraphicsPath(glass, 3));
        fillGradient(c, glassPath, glass, CColor(255, 255, 255, 46), CColor(255, 255, 255, 0));
        setDirty(false);
    }

private:
    double position(double db) const
    {
        return peak_ ? (db + 12) / 24 : std::pow(10.0, (db - 3) / 20);
    }
    const char* channel_;
    CColor face_ = kDeskSkins[0].face;
    bool peak_ = false;
    double pos_ = 0, lamp_ = 0;
};

// A knob cap on its scale of eleven dots; turned by dragging, double click (or ctrl-click) for the default.
class Knob : public CKnob
{
public:
    Knob(const CRect& r, IControlListener* listener, int32_t tag) : CKnob(r, listener, tag, nullptr, nullptr) {}
    void setLook(const Skin& skin)
    {
        const bool light = skin.cap.red + skin.cap.green + skin.cap.blue > 3 * 110;
        cap_ = skin.cap, ink_ = light ? CColor(24, 24, 24) : CColor(236, 232, 222), scale_ = shade(skin.body2, 0.45);
        invalid();
    }
    void draw(CDrawContext* c) override
    {
        const CRect r = getViewSize();
        const CCoord d = std::min(r.getWidth(), r.getHeight());
        const CPoint m = r.getCenter();
        auto at = [&](double deg, double rad) {
            const double a = deg * 3.14159265358979 / 180;
            return CPoint(m.x + std::sin(a) * rad, m.y - std::cos(a) * rad);
        };
        c->setFillColor(scale_);
        for (int i = 0; i <= 10; ++i) {
            const CPoint p = at(-135 + 27 * i, d / 2 - 2);
            c->drawEllipse(CRect(p.x - 1.3, p.y - 1.3, p.x + 1.3, p.y + 1.3), kDrawFilled);
        }
        CRect body(m.x - d / 2 + 7, m.y - d / 2 + 7, m.x + d / 2 - 7, m.y + d / 2 - 7);
        CRect shadow = body;
        shadow.offset(1, 2);
        c->setFillColor(CColor(0, 0, 0, 90));
        c->drawEllipse(shadow, kDrawFilled);
        fillEllipse(c, body, shade(cap_, 1.5), shade(cap_, 0.7));
        CRect top = body;
        top.inset(3, 3);
        fillEllipse(c, top, shade(cap_, 1.15), shade(cap_, 0.9));
        c->setFrameColor(ink_);
        c->setLineWidth(2);
        const double a = -135 + 270 * getValueNormalized();
        c->drawLine(at(a, body.getWidth() * 0.12), at(a, body.getWidth() / 2 - 2));
        setDirty(false);
    }

private:
    CColor cap_ = kDeskSkins[0].cap, ink_ = kDeskSkins[0].ink, scale_ = CColor(20, 20, 20);
};

class Panel : public VSTGUIEditor, public IControlListener
{
public:
    static constexpr CCoord kW = 660, kRow = 28, kFace = 184, kTop = kFace + 14;

    Panel(EditController* controller, bool mix, ViewRect size) : VSTGUIEditor(controller, &size), mix_(mix) {}
    static ViewRect sizeFor(bool mix) { return ViewRect(0, 0, int32(kW), int32(kTop + auRows(mix).size() * kRow + 10)); }

    bool PLUGIN_API open(void* parent, const PlatformType& platform) override
    {
        if (frame) return false;
        const ViewRect& r = getRect();
        frame = new CFrame(CRect(0, 0, r.getWidth(), r.getHeight()), this);
        frame->setBackgroundColor(CColor(30, 28, 26));
        plate_ = new Plate(CRect(8, 8, kW - 8, kFace));   // the front plate: name, knobs, meters
        frame->addView(plate_);
        CCoord x = 26;
        for (AuParamId id : auKnobs()) {
            auto* knob = new Knob(CRect(x, 86, x + 56, 134), this, id);
            knob->setDefaultValue(float(scaleOf(id, mix_).norm(auDefault(id, mix_))));
            knob->setZoomFactor(4);
            add(knob, id);
            knobs_.push_back(knob);
            auto* name = new CTextLabel(CRect(x - 4, 136, x + 60, 150), id == kPMeterRef ? "Taratura" : auParam(id).name);
            style(name, kNormalFontSmaller, CColor(0, 0, 0));
            captions_.push_back(name);
            frame->addView(name);
            auto* value = new CTextLabel(CRect(x - 4, 150, x + 60, 164));
            style(value, kNormalFontSmaller, CColor(0, 0, 0));
            captions_.push_back(value);
            frame->addView(value);
            values_.push_back({ value, id });
            x += 58;
        }
        meters_[0] = new Meter(CRect(282, 22, 458, 170), "L");
        meters_[1] = new Meter(CRect(466, 22, 642, 170), "R");
        for (Meter* m : meters_) frame->addView(m);
        CCoord y = kTop;
        for (const AuRow& row : auRows(mix_)) {
            auto* label = new CTextLabel(CRect(12, y, 172, y + kRow - 4), row.label);
            style(label, kNormalFont, CColor(220, 214, 200));
            label->setHoriAlign(kLeftText);
            frame->addView(label);
            if (row.toggle >= 0) add(new CCheckBox(CRect(176, y + 3, 196, y + kRow - 5), this, row.toggle), row.toggle);
            if (row.control >= 0) addControl(AuParamId(row.control), CRect(204, y, kW - 84, y + kRow - 4));
            y += kRow;
        }
        IPlatformFrameConfig* config = nullptr;
#if SMTG_OS_LINUX
        X11::FrameConfig x11;   // an X11 host gives its run loop through the frame (as VST3Editor does)
        Steinberg::Linux::setupVSTGUIRunloop(plugFrame);
        if (auto* lf = getPlatformFactory().asLinuxFactory()) x11.runLoop = lf->getRunLoop();
        config = &x11;
#endif
        frame->open(parent, platform, config);
        timer_ = makeOwned<CVSTGUITimer>([this](CVSTGUITimer*) { sync(); }, kTick);
        sync(true);
        return true;
    }
    void PLUGIN_API close() override
    {
        timer_ = nullptr;
        controls_.clear();
        menus_.clear();
        knobs_.clear();
        captions_.clear();
        plate_ = nullptr;
        meters_[0] = meters_[1] = nullptr;
        look_ = -1;
        values_.clear();
        if (frame) {
            frame->forget();
            frame = nullptr;
        }
    }

    void valueChanged(CControl* control) override
    {
        ParamID id = ParamID(control->getTag());
        for (Menu& m : menus_)
            if (m.menu == control) {   // a menu entry stands for a plain value; its children follow it
                int i = int(m.menu->getCurrentIndex());
                if (i < 0 || i >= int(m.plains.size()) || m.plains[size_t(i)] < 0) return sync(true);
                edit(id, m.plains[size_t(i)]);
                double v[kPCount];
                values(v);
                for (AuParamId child : auCoerce(v, mix_)) {
                    getController()->beginEdit(child);
                    edit(child, v[child]);
                    getController()->endEdit(child);
                }
                return sync(true);
            }
        ParamValue n = control->getValueNormalized();
        getController()->setParamNormalized(id, n);
        getController()->performEdit(id, n);
        showValue(id);
        sync();
    }
    void controlBeginEdit(CControl* control) override { getController()->beginEdit(ParamID(control->getTag())); }
    void controlEndEdit(CControl* control) override { getController()->endEdit(ParamID(control->getTag())); }

private:
    struct Menu
    {
        COptionMenu* menu;
        AuParamId id;
        std::vector<int> plains;   // per entry: the parameter's plain value, -1 = a family title
        std::vector<std::string> names;
    };

    static void style(CTextLabel* l, const SharedPointer<CFontDesc>& font, CColor colour)
    {
        l->setFont(font);
        l->setFontColor(colour);
        l->setBackColor(kTransparentCColor);
        l->setFrameColor(kTransparentCColor);
        l->setStyle(CParamDisplay::kNoFrame);
    }
    void add(CControl* c, int id)
    {
        if (auto* box = dynamic_cast<CCheckBox*>(c)) {
            box->setBoxFillColor(CColor(52, 48, 44));
            box->setBoxFrameColor(CColor(120, 110, 96));
            box->setCheckMarkColor(CColor(232, 196, 120));
        }
        frame->addView(c);
        controls_.push_back({ c, id });
    }
    void addControl(AuParamId id, const CRect& r)
    {
        if (auParam(id).kind == AuKind::List) {   // filled by sync(): its entries depend on the other menus
            auto* menu = new COptionMenu(r, this, id);
            menu->setFont(kNormalFont);
            menu->setFontColor(CColor(240, 234, 220));
            menu->setBackColor(CColor(52, 48, 44));
            menu->setFrameColor(CColor(120, 110, 96));
            frame->addView(menu);
            menus_.push_back({ menu, id });
            return;
        }
        auto* slider = new CSlider(CRect(r.left, r.top + 6, r.right, r.bottom - 6), this, id, 0, int32_t(r.getWidth()),
                                   nullptr, nullptr);
        slider->setDrawStyle(CSlider::kDrawFrame | CSlider::kDrawBack | CSlider::kDrawValue);
        slider->setBackColor(CColor(52, 48, 44));
        slider->setFrameColor(CColor(120, 110, 96));
        slider->setValueColor(CColor(232, 196, 120));
        slider->setDefaultValue(scaleOf(id, mix_).norm(auDefault(id, mix_)));
        add(slider, id);
        auto* text = new CTextLabel(CRect(r.right + 6, r.top, kW - 12, r.bottom));
        style(text, kNormalFontSmall, CColor(220, 214, 200));
        text->setHoriAlign(kRightText);
        frame->addView(text);
        values_.push_back({ text, id });
    }
    void showValue(int id)
    {
        for (auto& [text, tag] : values_)
            if (tag == id) {
                String128 s;
                getController()->getParamStringByValue(ParamID(id), getController()->getParamNormalized(ParamID(id)), s);
                std::string value = Steinberg::Vst::StringConvert::convert(s);
                if (*auParam(AuParamId(id)).unit) value += std::string(" ") + auParam(AuParamId(id)).unit;
                text->setText(value.c_str());
            }
    }
    void values(double* v)   // the plain values the controller holds now
    {
        defaults(v, mix_);
        for (AuParamId id : auParams(mix_))
            v[id] = getController()->normalizedParamToPlain(id, getController()->getParamNormalized(id));
    }
    void edit(int id, double plain)
    {
        ParamValue n = getController()->plainParamToNormalized(ParamID(id), plain);
        getController()->setParamNormalized(ParamID(id), n);
        getController()->performEdit(ParamID(id), n);
    }
    // A menu offers what its parents allow; the recorders come grouped under their family. A value loaded from an
    // older project that no longer fits is kept and shown at the end, never changed behind the user's back.
    void fill(Menu& m, const double* v)
    {
        std::vector<int> plains;
        std::vector<std::string> names;
        if (m.id == kPDevice)
            for (int f = 0, flat = 0; f < int(std::size(kAuFamilies)); ++f) {
                plains.push_back(-1);
                names.push_back(kAuFamilies[f].name);
                for (size_t i = 0; i < kAuFamilies[f].count; ++i, ++flat) {
                    plains.push_back(flat);
                    names.push_back(kAuFamilies[f].devices[i].name);
                }
            }
        else
            for (int p : auAllowed(m.id, v, mix_)) {
                plains.push_back(p);
                names.push_back(auEntryName(m.id, p, v, mix_));
            }
        const int current = auPlain(v, m.id);
        if (std::find(plains.begin(), plains.end(), current) == plains.end()) {
            plains.push_back(current);
            names.push_back(auEntryName(m.id, current, v, mix_));
        }
        if (plains != m.plains || names != m.names) {
            m.menu->removeAllEntry();
            for (size_t i = 0; i < names.size(); ++i)
                m.menu->addEntry(names[i].c_str(), -1,
                                 plains[i] < 0 ? CMenuItem::kTitle | CMenuItem::kDisabled : CMenuItem::kNoFlags);
            m.plains = std::move(plains);
            m.names = std::move(names);
        }
        const int at = int(std::find(m.plains.begin(), m.plains.end(), current) - m.plains.begin());
        if (m.menu->getCurrentIndex() != at) {
            m.menu->setCurrent(at);
            m.menu->invalid();
        }
    }
    // Puts on the plate of the chosen recorder family or desk, with its name and the meter that goes with it.
    void dress(const double* v)
    {
        int family = 0, model = 0;
        auDeviceSplit(auPlain(v, kPDevice), family, model);
        const int look = mix_ ? auPlain(v, kPDesk) : family * 100 + model;
        const bool peak = auMeterPeak(v, mix_);
        if (look == look_) return;
        look_ = look;
        const Skin& skin = mix_ ? kDeskSkins[std::clamp(look, 0, int(std::size(kDeskSkins)) - 1)]
                                : kFamilySkins[std::clamp(family, 0, int(std::size(kFamilySkins)) - 1)];
        const int desk = auPlain(v, kPDesk);
        plate_->set(skin, mix_ ? "LOG METARAW · MIX E SUPPORTO" : "LOG METARAW · RIPRESA",
                    mix_ ? (desk ? kAuConsoles[desk].name : "Senza banco") : kAuFamilies[family].devices[model].name,
                    mix_ ? "Banco di missaggio" : kAuFamilies[family].name);
        for (Meter* m : meters_) m->setLook(skin, peak);
        for (Knob* k : knobs_) k->setLook(skin);
        const bool light = skin.body.red + skin.body.green + skin.body.blue > 3 * 120;
        const CColor ink = light ? CColor(28, 26, 24) : CColor(236, 232, 222);   // captions engraved on the plate
        for (CTextLabel* l : captions_) {
            l->setFontColor(ink);
            l->invalid();
        }
    }
    void sync(bool all = false)   // the host or automation may have moved a parameter
    {
        double v[kPCount];
        values(v);
        for (Menu& m : menus_)
            if (all || !m.menu->isEditing()) fill(m, v);
        dress(v);
        for (int c = 0; c < 2; ++c) {   // the levels the processor reported, against the calibration
            const double amp = 2 * getController()->getParamNormalized(ParamID(kPMeterL + c));
            meters_[c]->feed(20 * std::log10(std::max(amp, 1e-5)) - v[kPMeterRef], kTick / 1000.0);
        }
        const bool copies = v[kPTransfer] > 0;   // copies only count when there is a transfer
        for (auto& [control, id] : controls_) {
            if (control->isEditing()) continue;
            float n = float(getController()->getParamNormalized(ParamID(id)));
            if (all || control->getValueNormalized() != n) {
                control->setValueNormalized(n);
                control->invalid();
            }
            if (id == kPCopies && control->getMouseEnabled() != copies) {
                control->setMouseEnabled(copies);
                control->setAlphaValue(copies ? 1 : 0.35f);
            }
            showValue(id);
        }
    }

    static constexpr int kTick = 33;   // ms: meters at 30 frames a second

    bool mix_;
    Plate* plate_ = nullptr;
    Meter* meters_[2] = {};
    std::vector<Knob*> knobs_;
    std::vector<CTextLabel*> captions_;
    int look_ = -1;
    std::vector<std::pair<CControl*, int>> controls_;
    std::vector<Menu> menus_;
    std::vector<std::pair<CTextLabel*, int>> values_;
    SharedPointer<CVSTGUITimer> timer_;
};

// ---- controller

class Controller : public EditController
{
public:
    Controller(bool mix) : mix_(mix) {}
    static FUnknown* ripresa(void*) { return static_cast<IEditController*>(new Controller(false)); }
    static FUnknown* mixer(void*) { return static_cast<IEditController*>(new Controller(true)); }

    tresult PLUGIN_API initialize(FUnknown* context) override
    {
        tresult r = EditController::initialize(context);
        if (r != kResultOk) return r;
        for (AuParamId id : auParams(mix_)) {
            const AuParamDef& d = auParam(id);
            std::u16string name = Steinberg::Vst::StringConvert::convert(d.name);
            std::u16string unit = Steinberg::Vst::StringConvert::convert(d.unit);
            const double def = auDefault(id, mix_);
            int32 flags = ParameterInfo::kCanAutomate;
            Parameter* p = nullptr;
            if (d.kind == AuKind::List) {
                auto* list = new StringListParameter(name.c_str(), id, nullptr, flags | ParameterInfo::kIsList);
                for (const std::string& entry : auListNames(id, mix_)) {
                    String128 s;
                    Steinberg::Vst::StringConvert::convert(entry, s);
                    list->appendString(s);
                }
                p = list;
            } else if (d.kind == AuKind::Meter) {
                p = new RangeParameter(name.c_str(), id, nullptr, 0, 1, 0, 0, ParameterInfo::kIsReadOnly);
            } else if (d.kind == AuKind::Toggle) {
                if (id == kPBypass) flags |= ParameterInfo::kIsBypass;
                p = new RangeParameter(name.c_str(), id, nullptr, 0, 1, def, 1, flags);
            } else {
                p = new RangeParameter(name.c_str(), id, unit.c_str(), d.min, d.max, def,
                                       id == kPCopies ? int32(d.max - d.min) : 0, flags);
            }
            if (d.kind == AuKind::Range) p->setPrecision(id == kPCopies || id == kPMix ? 0 : 1);
            p->getInfo().defaultNormalizedValue = scaleOf(id, mix_).norm(def);
            p->setNormalized(p->getInfo().defaultNormalizedValue);
            parameters.addParameter(p);
        }
        return kResultOk;
    }

    tresult PLUGIN_API setComponentState(IBStream* state) override
    {
        double v[kPCount];
        defaults(v, mix_);
        uint32 seed = 0;
        if (!readState(state, v, seed)) return kResultFalse;
        for (AuParamId id : auParams(mix_)) setParamNormalized(id, scaleOf(id, mix_).norm(v[id]));
        return kResultOk;
    }

    IPlugView* PLUGIN_API createView(FIDString name) override
    {
        return FIDStringsEqual(name, ViewType::kEditor) ? new Panel(this, mix_, Panel::sizeFor(mix_)) : nullptr;
    }

private:
    bool mix_;
};

}  // namespace

#define AU_VENDOR "Mecena SRL"
#define AU_URL "https://github.com/Mecena-SRL/SLogMetaRaw"

// The Audio Unit wrapper takes the first effect a module holds: each Audio Unit embeds a build with one plugin only
// (AU_ONLY 1 = Ripresa, 2 = Mix e supporto); the VST3 for Fairlight holds both.
#ifndef AU_ONLY
#define AU_ONLY 0
#endif

BEGIN_FACTORY_DEF(AU_VENDOR, AU_URL, "")

#if AU_ONLY != 2
DEF_CLASS2(INLINE_UID_FROM_FUID(kRipresaProcessor), PClassInfo::kManyInstances, kVstAudioEffectClass,
           "Log MetaRaw Ripresa", Vst::kDistributable, PlugType::kFx, AU_VERSION, kVstVersionString,
           Processor::ripresa)
DEF_CLASS2(INLINE_UID_FROM_FUID(kRipresaController), PClassInfo::kManyInstances, kVstComponentControllerClass,
           "Log MetaRaw Ripresa Controller", 0, "", AU_VERSION, kVstVersionString, Controller::ripresa)
#endif
#if AU_ONLY != 1
DEF_CLASS2(INLINE_UID_FROM_FUID(kMixProcessor), PClassInfo::kManyInstances, kVstAudioEffectClass,
           "Log MetaRaw Mix e supporto", Vst::kDistributable, PlugType::kFx, AU_VERSION, kVstVersionString,
           Processor::mixer)
DEF_CLASS2(INLINE_UID_FROM_FUID(kMixController), PClassInfo::kManyInstances, kVstComponentControllerClass,
           "Log MetaRaw Mix e supporto Controller", 0, "", AU_VERSION, kVstVersionString, Controller::mixer)
#endif

END_FACTORY

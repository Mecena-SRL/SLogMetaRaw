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

// The look of a front plate, from its row in AuLooks.h: body (top and bottom of its gradient), name plate and its
// lettering, meter face, knob caps.
CColor rgb(uint32_t c, uint8_t alpha = 255) { return CColor(uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), alpha); }
struct Skin
{
    CColor body, body2, plate, ink, face, cap;
    bool brushed;
    explicit Skin(const AuLook& l)
        : body(rgb(l.body)), body2(rgb(l.body2)), plate(rgb(l.plate)), ink(rgb(l.ink)), face(rgb(l.face)),
          cap(rgb(l.cap)), brushed(l.brushed)
    {
    }
};
bool isLight(CColor c) { return c.red + c.green + c.blue > 3 * 120; }

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
void fillRound(CDrawContext* c, const CRect& r, CCoord radius, CColor top, CColor bottom)
{
    auto path = VSTGUI::owned(c->createRoundRectGraphicsPath(r, radius));
    fillGradient(c, path, r, top, bottom);
}
constexpr double kPi = 3.14159265358979;

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
    Skin skin_ { kAuDeskLooks[0] };
    std::string brand_, name_, kind_;
};

// The level meters of the chosen machine, drawn and moving like the ones it had (AuMeter in AuLooks.h): one for both
// channels or one each. Levels arrive in dB against the calibration (0 = the reference, 0 VU).
class Meters : public CView
{
public:
    explicit Meters(const CRect& r) : CView(r) {}
    void setLook(const AuLook& look)
    {
        look_ = look;
        pos_[0] = pos_[1] = hold_[0] = hold_[1] = 0;
        invalid();
    }
    void feed(const double db[2], double dt)
    {
        for (int c = 0; c < look_.meters; ++c) {
            const double level = look_.meters == 1 ? std::max(db[0], db[1]) : db[c];
            const double target = position(level);
            double& p = pos_[c];
            auto approach = [&](double tau) { p += (target - p) * (1 - std::exp(-dt / tau)); };
            auto fall = [&](double dbPerSecond, double range) { p = std::max(target, p - dt * dbPerSecond / range); };
            switch (look_.meter) {
            case AuMeter::VU:
            case AuMeter::Indicator: approach(0.09); break;
            case AuMeter::PPM: target > p ? approach(0.004) : fall(24 / 2.8, 24); break;
            case AuMeter::Modulometer: target > p ? approach(0.005) : fall(12, 35); break;
            case AuMeter::Eye: approach(target > p ? 0.01 : 0.15); break;
            case AuMeter::Led: target > p ? void(p = target) : fall(20, 46); break;
            default: approach(0.15);
            }
            hold_[c] = p >= hold_[c] ? (heldFor_[c] = 0.8, p) : (heldFor_[c] -= dt) > 0 ? hold_[c] : p;
            lamp_[c] = level > overload() ? 0.6 : std::max(0.0, lamp_[c] - dt);
        }
        invalid();
    }
    void draw(CDrawContext* c) override
    {
        const CRect r = getViewSize();
        const CColor ink = isLight(rgb(look_.face)) ? CColor(34, 30, 26) : CColor(236, 232, 222);
        switch (look_.meter) {
        case AuMeter::Eye: drawEye(c, r); break;
        case AuMeter::Led: drawLeds(c, r); break;
        case AuMeter::Automatic:
        case AuMeter::None: drawNote(c, r); break;
        default:
            if (look_.meters == 1) {
                const CCoord w = 232;
                drawNeedle(c, CRect(r.getCenter().x - w / 2, r.top, r.getCenter().x + w / 2, r.bottom), 0, nullptr, ink);
            } else {
                const CCoord w = (r.getWidth() - 8) / 2;
                drawNeedle(c, CRect(r.left, r.top, r.left + w, r.bottom), 0, "L", ink);
                drawNeedle(c, CRect(r.right - w, r.top, r.right, r.bottom), 1, "R", ink);
            }
        }
        setDirty(false);
    }

private:
    // Where a level sits on the scale, 0..1.
    double position(double db) const
    {
        switch (look_.meter) {
        case AuMeter::VU:
        case AuMeter::Indicator: return std::pow(10.0, (db - 3) / 20);   // linear in voltage
        case AuMeter::PPM: return (db + 12) / 24;                         // marks 1..7, 4 dB apart, 4 at 0 dB
        case AuMeter::Modulometer: return (db + 30) / 35;                 // -30..+5 dB
        case AuMeter::Eye: return std::clamp((db + 30) / 33, 0.0, 1.0);
        case AuMeter::Led: return (db + 40) / 46;                         // -40..+6 dB
        default: return std::clamp((db + 40) / 40, 0.0, 1.0);
        }
    }
    double overload() const
    {
        switch (look_.meter) {
        case AuMeter::PPM: return 8;   // past mark 6
        case AuMeter::Modulometer: return 0;
        default: return 3;
        }
    }

    // A needle meter: VU, PPM, modulometer or the small level needle of consumer machines.
    void drawNeedle(CDrawContext* c, const CRect& r, int ch, const char* channel, CColor ink)
    {
        fillRound(c, r, 6, CColor(54, 52, 50), CColor(16, 16, 16));
        CRect face = r;
        face.inset(7, 7);
        const CColor faceColour = rgb(look_.face);
        fillRound(c, face, 3, shade(faceColour, 1.04), shade(faceColour, 0.86));
        const CPoint pivot(face.getCenter().x, face.bottom - 8);   // hidden under the bridge
        const double radius = face.getHeight() - 38;
        auto at = [&](double t, double rad) {
            const double a = (-43 + 86 * t) * kPi / 180;
            return CPoint(pivot.x + std::sin(a) * rad, pivot.y - std::cos(a) * rad);
        };
        const CColor red = isLight(faceColour) ? CColor(186, 40, 30) : CColor(240, 80, 60);
        auto arc = [&](double from, double to, CColor colour, double width) {
            auto path = VSTGUI::owned(c->createGraphicsPath());
            path->beginSubpath(at(from, radius));
            for (int i = 1; i <= 24; ++i) path->addLine(at(from + (to - from) * i / 24, radius));
            c->setFrameColor(colour);
            c->setLineWidth(width);
            c->drawGraphicsPath(path, CDrawContext::kPathStroked);
        };
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
        const char* caption = "VU";
        const double zero = position(0);
        switch (look_.meter) {
        case AuMeter::PPM:   // the broadcast meter: white marks on black, no red zone
            arc(0, 1, ink, 1.2);
            for (int m = 1; m <= 7; ++m) {
                const char label[2] = { char('0' + m), 0 };
                mark((m - 1) / 6.0, label, false);
            }
            caption = "PPM";
            break;
        case AuMeter::Modulometer: {
            arc(0, zero, ink, 1.2);
            arc(zero, 1, red, 3);
            const struct { int db; const char* label; } marks[] = {
                { -30, "30" }, { -25, nullptr }, { -20, "20" }, { -15, nullptr }, { -10, "10" }, { -8, nullptr },
                { -6, nullptr }, { -4, nullptr }, { -2, nullptr }, { 0, "0" }, { 2, nullptr }, { 4, nullptr }, { 5, "+5" },
            };
            for (auto m : marks) mark(position(m.db), m.label, m.db > 0);
            caption = "dB";
            break;
        }
        case AuMeter::Indicator:   // no numbers: a black arc and a red one, as on the small portables
            arc(0, zero, ink, 4);
            arc(zero, 1, red, 4);
            caption = "REC";
            break;
        default: {
            arc(0, zero, ink, 1.2);
            arc(zero, 1, red, 3);
            const struct { int db; const char* label; } marks[] = {
                { -20, "20" }, { -10, "10" }, { -7, nullptr }, { -5, "5" }, { -3, "3" }, { -2, nullptr },
                { -1, nullptr }, { 0, "0" }, { 1, nullptr }, { 2, nullptr }, { 3, "+3" },
            };
            for (auto m : marks) mark(position(m.db), m.label, m.db > 0);
        }
        }
        c->setFontColor(ink);
        c->setFont(kNormalFont);
        c->drawString(caption, CRect(face.left, face.top + face.getHeight() * 0.5, face.right,
                                     face.top + face.getHeight() * 0.5 + 16), kCenterText);
        if (channel) {
            c->setFont(kNormalFontSmaller);
            c->drawString(channel, CRect(face.left + 6, face.bottom - 30, face.left + 20, face.bottom - 18), kLeftText);
        }
        CRect clip;   // the needle stays behind the glass
        c->getClipRect(clip);
        c->setClipRect(face);
        c->setFrameColor(isLight(faceColour) ? CColor(20, 18, 16) : CColor(250, 250, 246));
        c->setLineWidth(1.6);
        c->drawLine(pivot, at(std::clamp(pos_[ch], -0.03, 1.06), radius + 10));
        c->setClipRect(clip);
        fillRound(c, CRect(face.left, face.bottom - 16, face.right, face.bottom), 2, CColor(44, 42, 40),
                  CColor(20, 20, 20));
        lamp(c, CRect(face.right - 16, face.bottom - 12, face.right - 8, face.bottom - 4), lamp_[ch] > 0);
        fillRound(c, CRect(face.left, face.top, face.right, face.top + face.getHeight() * 0.45), 3,
                  CColor(255, 255, 255, 46), CColor(255, 255, 255, 0));
    }

    // The magic eye: a green fan around a black cap; the dark wedge narrows as the level rises.
    void drawEye(CDrawContext* c, const CRect& r)
    {
        const CPoint m = r.getCenter();
        const CCoord d = r.getHeight() - 8;
        CRect bezel(m.x - d / 2, m.y - d / 2, m.x + d / 2, m.y + d / 2);
        fillEllipse(c, bezel, CColor(80, 76, 70), CColor(20, 20, 18));
        CRect glass = bezel;
        glass.inset(10, 10);
        fillEllipse(c, glass, CColor(10, 22, 14), CColor(4, 10, 6));
        CRect glow = glass;
        glow.inset(6, 6);
        fillEllipse(c, glow, CColor(110, 255, 160), CColor(40, 190, 100));
        const double half = 4 + (1 - pos_[0]) * 80;   // degrees either side of the top
        auto wedge = VSTGUI::owned(c->createGraphicsPath());
        wedge->beginSubpath(glow.getCenter());
        wedge->addArc(glow, 270 - half, 270 + half, true);
        wedge->closeSubpath();
        c->setFillColor(CColor(8, 26, 14));
        c->drawGraphicsPath(wedge, CDrawContext::kPathFilled);
        CRect cap(m.x - d * 0.16, m.y - d * 0.16, m.x + d * 0.16, m.y + d * 0.16);
        fillEllipse(c, cap, CColor(40, 40, 40), CColor(6, 6, 6));
        fillEllipse(c, CRect(glass.left, glass.top, glass.right, m.y), CColor(255, 255, 255, 40),
                    CColor(255, 255, 255, 0));
    }

    // A ladder of lamps per channel, green, then yellow, then red, with the peak held a moment.
    void drawLeds(CDrawContext* c, const CRect& r)
    {
        fillRound(c, r, 6, CColor(40, 40, 42), CColor(12, 12, 12));
        static const int kSteps[] = { -40, -30, -20, -15, -10, -7, -5, -3, -1, 0, 3, 6 };
        const int n = int(std::size(kSteps));
        const CCoord left = r.left + 26, width = (r.getWidth() - 40) / n;
        c->setFont(kNormalFontSmaller);
        for (int ch = 0; ch < 2; ++ch) {
            const CCoord top = r.top + 28 + ch * 58;
            c->setFontColor(CColor(200, 200, 196));
            c->drawString(ch ? "R" : "L", CRect(r.left + 8, top, left - 4, top + 22), kLeftText);
            for (int i = 0; i < n; ++i) {
                const double t = position(kSteps[i]);
                const bool on = pos_[ch] >= t || std::fabs(hold_[ch] - t) < 0.5 / n;
                const CColor hue = kSteps[i] > 0 ? CColor(255, 60, 40) : kSteps[i] >= -1 ? CColor(255, 200, 40)
                                                                                           : CColor(60, 230, 90);
                CRect seg(left + i * width + 2, top, left + (i + 1) * width - 2, top + 22);
                fillRound(c, seg, 2, on ? shade(hue, 1.1) : shade(hue, 0.22), on ? shade(hue, 0.8) : shade(hue, 0.14));
            }
        }
        c->setFontColor(CColor(170, 170, 166));
        for (int i = 0; i < n; i += i < 4 ? 2 : 1) {
            char label[8];
            std::snprintf(label, sizeof label, kSteps[i] > 0 ? "+%d" : "%d", kSteps[i]);
            const CCoord x = left + (i + 0.5) * width;
            c->drawString(label, CRect(x - 14, r.top + 52, x + 14, r.top + 82), kCenterText);
        }
        c->drawString("dB", CRect(r.left, r.bottom - 26, r.right - 12, r.bottom - 8), kRightText);
    }

    // No meter: automatic level control (a lamp that lights with the sound), or nothing at all.
    void drawNote(CDrawContext* c, const CRect& r)
    {
        fillRound(c, r, 6, CColor(40, 38, 36), CColor(16, 16, 16));
        const bool automatic = look_.meter == AuMeter::Automatic;
        c->setFontColor(CColor(210, 206, 196));
        c->setFont(kNormalFont);
        c->drawString(automatic ? "Livello automatico" : "Nessun indicatore",
                      CRect(r.left, r.top + 30, r.right, r.top + 50), kCenterText);
        c->setFont(kNormalFontSmaller);
        c->drawString(automatic ? "la macchina regola da sola il livello di registrazione"
                                : "la tromba incide il suono direttamente sulla cera",
                      CRect(r.left + 10, r.top + 54, r.right - 10, r.top + 70), kCenterText);
        if (!automatic) return;
        const CPoint m(r.getCenter().x, r.top + 108);
        const double k = std::clamp(pos_[0], 0.0, 1.0);
        fillEllipse(c, CRect(m.x - 9, m.y - 9, m.x + 9, m.y + 9), CColor(uint8_t(60 + 190 * k), uint8_t(40 + 150 * k), 20),
                    CColor(uint8_t(30 + 140 * k), uint8_t(20 + 90 * k), 10));
    }

    void lamp(CDrawContext* c, const CRect& r, bool on)
    {
        fillEllipse(c, r, on ? CColor(255, 90, 60) : CColor(90, 24, 18), on ? CColor(220, 30, 20) : CColor(50, 14, 10));
    }

    AuLook look_ = kAuDeskLooks[0];
    double pos_[2] = {}, hold_[2] = {}, heldFor_[2] = {}, lamp_[2] = {};
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
    CColor cap_ = CColor(40, 40, 40), ink_ = CColor(236, 232, 222), scale_ = CColor(20, 20, 20);
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
        meters_ = new Meters(CRect(282, 22, 642, 170));
        frame->addView(meters_);
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
        meters_ = nullptr;
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
        const int desk = auPlain(v, kPDesk);
        const int look = mix_ ? desk : family * 100 + model;
        if (look == look_) return;
        look_ = look;
        const AuLook& data = auLook(v, mix_);
        const Skin skin(data);
        plate_->set(skin, mix_ ? "LOG METARAW · MIX E SUPPORTO" : "LOG METARAW · RIPRESA",
                    mix_ ? (desk ? kAuConsoles[desk].name : "Senza banco") : kAuFamilies[family].devices[model].name,
                    mix_ ? "Banco di missaggio" : kAuFamilies[family].name);
        meters_->setLook(data);
        for (Knob* k : knobs_) k->setLook(skin);
        const CColor ink = isLight(skin.body) ? CColor(28, 26, 24) : CColor(236, 232, 222);   // engraved captions
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
        double db[2];   // the levels the processor reported, against the calibration
        for (int c = 0; c < 2; ++c) {
            const double amp = 2 * getController()->getParamNormalized(ParamID(kPMeterL + c));
            db[c] = 20 * std::log10(std::max(amp, 1e-5)) - v[kPMeterRef];
        }
        meters_->feed(db, kTick / 1000.0);
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
    Meters* meters_ = nullptr;
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

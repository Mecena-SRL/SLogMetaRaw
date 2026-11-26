// SPDX-License-Identifier: GPL-3.0-or-later
// Log MetaRaw Audio as VST3: two plugins in one module, «Ripresa» and «Mix e supporto», each a processor (moves
// samples through AuEngine), a controller (parameters, saved state) and a panel drawn from the rows in AuParams.h.
#include "AuFronts.h"
#include "AuMeters.h"

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
#include <deque>
#include <functional>
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
        const AuMeter meter = auLook(v_, mix_).meter;   // the machine's meter moves as it did (AuMeters.h)
        if (meter != meter_ || fs_ != meterFs_) {
            for (AuBallistics& b : ballistics_) b.prepare(fs_, meter);
            meter_ = meter, meterFs_ = fs_;
        }
        for (int c = 0; c < 2 && n > 0; ++c)   // what goes in: read before the chain works in place
            ballistics_[2 + c].run(in.channelBuffers32[std::min(c, n - 1)], data.numSamples);
        for (int c = 0; c < n; ++c)
            if (in.channelBuffers32[c] != out.channelBuffers32[c])
                std::copy(in.channelBuffers32[c], in.channelBuffers32[c] + data.numSamples, out.channelBuffers32[c]);
        if (v_[kPBypass] < 0.5 && n == channels_) engine_.process(out.channelBuffers32, data.numSamples);
        out.silenceFlags = 0;
        for (int c = 0; c < 2 && n > 0; ++c) ballistics_[c].run(out.channelBuffers32[std::min(c, n - 1)], data.numSamples);
        if (IParameterChanges* levels = data.outputParameterChanges; levels && n > 0) {   // for the panel's meters
            const ParamID ids[4] = { kPMeterL, kPMeterR, kPMeterInL, kPMeterInR };
            for (int k = 0; k < 4; ++k) {
                int32 index = 0;
                if (IParamValueQueue* q = levels->addParameterData(ids[k], index))
                    q->addPoint(0, auMeterToNorm(ballistics_[k].db()), index);
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
    AuBallistics ballistics_[4];   // output left, right; input left, right
    AuMeter meter_ = AuMeter::None;
    double meterFs_ = 0;
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

// The front plate: the body, its screws and the engraved name plate where the machine had its own.
class Plate : public CView
{
public:
    explicit Plate(const CRect& r) : CView(r) {}
    void set(const Skin& skin, const std::string& brand, const std::string& name, const std::string& kind, CPoint at)
    {
        skin_ = skin, brand_ = brand, name_ = name, kind_ = kind, at_ = at;
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
        for (CPoint p : { CPoint(r.left + 8, r.top + 8), CPoint(r.right - 8, r.top + 8), CPoint(r.left + 8, r.bottom - 8),
                          CPoint(r.right - 8, r.bottom - 8) }) {
            CRect s(p.x - 4, p.y - 4, p.x + 4, p.y + 4);
            fillEllipse(c, s, shade(skin_.body, 1.25), shade(skin_.body2, 0.7));
            c->setFrameColor(shade(skin_.body2, 0.45));
            c->drawLine(CPoint(s.left + 2, s.bottom - 3), CPoint(s.right - 2, s.top + 3));
        }
        CRect np(r.left + at_.x, r.top + at_.y, r.left + at_.x + 200, r.top + at_.y + 46);
        auto plate = VSTGUI::owned(c->createRoundRectGraphicsPath(np, 3));
        fillGradient(c, plate, np, shade(skin_.plate, 1.12), shade(skin_.plate, 0.88));
        c->setFrameColor(shade(skin_.plate, 0.6));
        c->drawGraphicsPath(plate, CDrawContext::kPathStroked);
        c->setFontColor(skin_.ink);
        c->setFont(kNormalFontVerySmall);
        c->drawString(brand_.c_str(), CRect(np.left + 8, np.top + 3, np.right - 8, np.top + 14), kLeftText);
        c->setFont(kNormalFont);
        c->setFont(kNormalFontSmall);
        c->drawString(name_.c_str(), CRect(np.left + 8, np.top + 14, np.right - 8, np.top + 30), kLeftText);
        c->setFont(kNormalFontVerySmall);
        c->drawString(kind_.c_str(), CRect(np.left + 8, np.top + 31, np.right - 8, np.top + 43), kLeftText);
        setDirty(false);
    }

private:
    Skin skin_ { kAuDeskLooks[0] };
    std::string brand_, name_, kind_;
    CPoint at_ { 16, 16 };
};

CPoint polar(CPoint m, double deg, double radius)   // degrees clockwise from twelve o'clock
{
    const double a = deg * kPi / 180;
    return CPoint(m.x + std::sin(a) * radius, m.y - std::cos(a) * radius);
}
std::vector<std::string> splitBars(const char* s)
{
    std::vector<std::string> out;
    if (!s) return out;
    std::string cur;
    for (const char* p = s;; ++p) {
        if (!*p || *p == '|') {
            out.push_back(cur);
            cur.clear();
            if (!*p) break;
        } else {
            cur += *p;
        }
    }
    return out;
}

// The meters of the chosen machine (AuMeter in AuLooks.h): one, one per channel or one per track. The processor moves
// them (AuMeters.h); the panel draws the reading, in dB against the calibration (0 = 0 VU, PPM 4).
class Meters : public CView
{
public:
    enum Mode { kLevel, kBattery, kFixed, kZero };
    explicit Meters(const CRect& r) : CView(r) {}
    void setLook(const AuLook& look, bool percent)
    {
        look_ = look, percent_ = percent;
        for (int i = 0; i < 4; ++i) pos_[i] = hold_[i] = heldFor_[i] = lamp_[i] = 0;
        invalid();
    }
    void feed(const double db[4], Mode mode, double dt)
    {
        for (int c = 0; c < std::min(look_.meters, 4); ++c) {
            const double level = look_.meters == 1 ? std::max(db[0], db[1]) : db[c];
            double p = position(level);
            if (mode == kBattery) p = 0.8;
            else if (mode == kFixed) p = position(0);
            else if (mode == kZero) p = 0;
            pos_[c] = p;
            hold_[c] = p >= hold_[c] ? (heldFor_[c] = 0.8, p) : (heldFor_[c] -= dt) > 0 ? hold_[c] : p;
            lamp_[c] = mode == kLevel && level > overload() ? 0.6 : std::max(0.0, lamp_[c] - dt);
            levels_[c] = mode == kLevel ? level : -100;
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
        case AuMeter::Neon: drawNeon(c, r); break;
        case AuMeter::Automatic:
        case AuMeter::None: drawNote(c, r); break;
        default: {
            const int n = look_.meters;
            const CCoord gap = 6, w = std::min((r.getWidth() - gap * (n - 1)) / n, r.getHeight() * 1.9);
            const CCoord left = r.getCenter().x - (w * n + gap * (n - 1)) / 2;
            static const char* const kNames[4][4] = { { nullptr }, { "L", "R" }, {}, { "1", "2", "3", "4" } };
            for (int i = 0; i < n; ++i) {
                const CCoord x = left + i * (w + gap);
                drawNeedle(c, CRect(x, r.top, x + w, r.bottom), i, n > 1 ? kNames[n - 1][i] : nullptr, ink);
            }
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
        case AuMeter::PPM: return std::clamp((db + 12) / 24, 0.0, 1.05);  // marks 1..7, 4 dB apart, 4 at 0 dB
        case AuMeter::Modulometer: return std::clamp((db + 30) / 35, 0.0, 1.05);   // -30..+5 dB
        case AuMeter::Eye: return std::clamp(std::pow(10.0, (db - 3) / 20), 0.0, 1.0);   // linear in voltage
        case AuMeter::Led: return std::clamp((db + 40) / 46, 0.0, 1.0);
        default: return std::clamp((db + 40) / 40, 0.0, 1.0);
        }
    }
    double overload() const { return look_.meter == AuMeter::Modulometer ? 0 : 3; }

    // A needle meter: VU, PPM, modulometer or the small level needle of consumer machines.
    void drawNeedle(CDrawContext* c, const CRect& r, int ch, const char* channel, CColor ink)
    {
        fillRound(c, r, 6, CColor(54, 52, 50), CColor(16, 16, 16));
        CRect face = r;
        face.inset(6, 6);
        const CColor faceColour = rgb(look_.face);
        fillRound(c, face, 3, shade(faceColour, 1.04), shade(faceColour, 0.86));
        const CPoint pivot(face.getCenter().x, face.bottom - 6);   // hidden under the bridge
        const double radius = face.getHeight() - 30;
        auto at = [&](double t, double rad) { return polar(pivot, -43 + 86 * t, rad); };
        const CColor red = isLight(faceColour) ? CColor(186, 40, 30) : CColor(240, 80, 60);
        auto arc = [&](double from, double to, double rad, CColor colour, double width) {
            auto path = VSTGUI::owned(c->createGraphicsPath());
            path->beginSubpath(at(from, rad));
            for (int i = 1; i <= 24; ++i) path->addLine(at(from + (to - from) * i / 24, rad));
            c->setFrameColor(colour);
            c->setLineWidth(width);
            c->drawGraphicsPath(path, CDrawContext::kPathStroked);
        };
        const bool small = face.getWidth() < 140;
        c->setFont(small ? kNormalFontVerySmall : kNormalFontSmaller);
        auto mark = [&](double t, const char* label, bool over, double rad) {
            c->setFrameColor(over ? red : ink);
            c->setLineWidth(label ? 1.3 : 1);
            c->drawLine(at(t, rad), at(t, rad + (label ? 7 : 4)));
            if (!label) return;
            const CPoint p = at(t, rad + 14);
            c->setFontColor(over ? red : ink);
            c->drawString(label, CRect(p.x - 14, p.y - 6, p.x + 14, p.y + 6), kCenterText);
        };
        const char* caption = "VU";
        const double zero = position(0);
        switch (look_.meter) {
        case AuMeter::PPM:   // the broadcast meter: white marks on black, no red zone
            arc(0, 1, radius, ink, 1.2);
            for (int m = 1; m <= 7; ++m) {
                const char label[2] = { char('0' + m), 0 };
                mark((m - 1) / 6.0, label, false, radius);
            }
            caption = "PPM";
            break;
        case AuMeter::Modulometer: {   // dB, the overload segment past 0, the battery scale under it
            arc(0, zero, radius, ink, 1.2);
            arc(zero, position(2), radius, CColor(10, 10, 10), 5);
            const struct { int db; const char* label; } marks[] = {
                { -30, "30" }, { -25, nullptr }, { -20, "20" }, { -15, nullptr }, { -10, "10" }, { -8, nullptr },
                { -6, nullptr }, { -4, nullptr }, { -2, nullptr }, { 0, "0" }, { 2, nullptr }, { 5, "+5" },
            };
            for (auto m : marks) mark(position(m.db), m.label, false, radius);
            arc(0.62, 0.92, radius - 18, ink, 1);
            arc(0.74, 0.92, radius - 18, CColor(60, 140, 70), 4);
            c->setFontColor(ink);
            c->setFont(kNormalFontVerySmall);
            const CPoint b = at(0.83, radius - 30);
            c->drawString("BATT", CRect(b.x - 16, b.y - 6, b.x + 16, b.y + 6), kCenterText);
            caption = "dB";
            break;
        }
        case AuMeter::Indicator:   // no numbers: a black arc and a red one, as on the small portables
            arc(0, zero, radius, ink, 4);
            arc(zero, 1, radius, red, 4);
            arc(0.74, 0.92, radius - 12, CColor(60, 140, 70), 3);
            caption = "";
            break;
        default:
            if (percent_) {   // type B: the modulation scale in per cent leads, the VU numbers below
                arc(0, 1, radius, ink, 1.2);
                for (int p = 0; p <= 100; p += 10) mark(p / 100.0 * zero, p % 20 ? nullptr : std::to_string(p).c_str(), false, radius);
                arc(zero, 1, radius, red, 3);
                const struct { int db; const char* label; } vu[] = { { -20, "−20" }, { -10, "−10" }, { -5, "−5" }, { 0, "0" }, { 3, "+3" } };
                c->setFont(kNormalFontVerySmall);
                for (auto m : vu) {
                    const CPoint p = at(position(m.db), radius - 12);
                    c->setFontColor(m.db > 0 ? red : ink);
                    c->drawString(m.label, CRect(p.x - 12, p.y - 6, p.x + 12, p.y + 6), kCenterText);
                }
                caption = "PER CENT";
            } else {
                arc(0, zero, radius, ink, 1.2);
                arc(zero, 1, radius, red, 3);
                const struct { int db; const char* label; } marks[] = {
                    { -20, "20" }, { -10, "10" }, { -7, small ? nullptr : "7" }, { -5, "5" }, { -3, "3" }, { -2, nullptr },
                    { -1, nullptr }, { 0, "0" }, { 1, nullptr }, { 2, nullptr }, { 3, "+3" },
                };
                for (auto m : marks) mark(position(m.db), m.label, m.db > 0, radius);
                if (!small)   // the per-cent scale of the standard face, under the arc
                    for (int p = 20; p <= 100; p += 20) {
                        const CPoint a = at(p / 100.0 * zero, radius - 4);
                        c->setFrameColor(ink);
                        c->drawLine(a, at(p / 100.0 * zero, radius - 8));
                    }
            }
        }
        c->setFontColor(ink);
        c->setFont(small ? kNormalFontSmaller : kNormalFont);
        c->drawString(caption, CRect(face.left, face.top + face.getHeight() * 0.52, face.right,
                                     face.top + face.getHeight() * 0.52 + 16), kCenterText);
        if (channel) {
            c->setFont(kNormalFontVerySmall);
            c->drawString(channel, CRect(face.left + 5, face.bottom - 26, face.left + 20, face.bottom - 15), kLeftText);
        }
        CRect clip;   // the needle stays behind the glass
        c->getClipRect(clip);
        c->setClipRect(face);
        c->setFrameColor(isLight(faceColour) ? CColor(20, 18, 16) : CColor(250, 250, 246));
        c->setLineWidth(1.5);
        c->drawLine(pivot, at(std::clamp(pos_[ch], -0.03, 1.06), radius + 9));
        c->setClipRect(clip);
        fillRound(c, CRect(face.left, face.bottom - 13, face.right, face.bottom), 2, CColor(44, 42, 40), CColor(20, 20, 20));
        lamp(c, CRect(face.right - 14, face.bottom - 10, face.right - 7, face.bottom - 3), lamp_[ch] > 0);
        fillRound(c, CRect(face.left, face.top, face.right, face.top + face.getHeight() * 0.45), 3,
                  CColor(255, 255, 255, 46), CColor(255, 255, 255, 0));
    }

    // The magic eye: a green fan around a black cap; the dark wedge narrows as the level rises.
    void drawEye(CDrawContext* c, const CRect& r)
    {
        const CPoint m = r.getCenter();
        const CCoord d = std::min(r.getWidth(), r.getHeight()) - 8;
        CRect bezel(m.x - d / 2, m.y - d / 2, m.x + d / 2, m.y + d / 2);
        fillEllipse(c, bezel, CColor(80, 76, 70), CColor(20, 20, 18));
        CRect glass = bezel;
        glass.inset(10, 10);
        fillEllipse(c, glass, CColor(10, 22, 14), CColor(4, 10, 6));
        CRect glow = glass;
        glow.inset(6, 6);
        fillEllipse(c, glow, CColor(110, 255, 160), CColor(40, 190, 100));
        const double half = 4 + (1 - std::clamp(pos_[0], 0.0, 1.0)) * 80;   // degrees either side of the top
        auto wedge = VSTGUI::owned(c->createGraphicsPath());
        wedge->beginSubpath(glow.getCenter());
        wedge->addArc(glow, 270 - half, 270 + half, true);
        wedge->closeSubpath();
        c->setFillColor(CColor(8, 26, 14));
        c->drawGraphicsPath(wedge, CDrawContext::kPathFilled);
        CRect cap(m.x - d * 0.16, m.y - d * 0.16, m.x + d * 0.16, m.y + d * 0.16);
        fillEllipse(c, cap, CColor(40, 40, 40), CColor(6, 6, 6));
        fillEllipse(c, CRect(glass.left, glass.top, glass.right, m.y), CColor(255, 255, 255, 40), CColor(255, 255, 255, 0));
    }

    // The neon lamp of the wire recorders: dark, then it strikes when the peak passes the mark.
    void drawNeon(CDrawContext* c, const CRect& r)
    {
        const CPoint m = r.getCenter();
        const CCoord d = std::min(r.getWidth(), r.getHeight()) * 0.7;
        CRect bezel(m.x - d / 2, m.y - d / 2, m.x + d / 2, m.y + d / 2);
        fillEllipse(c, bezel, CColor(150, 130, 90), CColor(60, 48, 30));
        CRect glass = bezel;
        glass.inset(d * 0.16, d * 0.16);
        const bool lit = levels_[0] >= 0;
        fillEllipse(c, glass, lit ? CColor(255, 170, 80) : CColor(90, 44, 26), lit ? CColor(240, 90, 30) : CColor(50, 22, 12));
        CRect hl = glass;
        hl.inset(glass.getWidth() * 0.25, glass.getHeight() * 0.25);
        fillEllipse(c, CRect(hl.left, hl.top, hl.right, hl.getCenter().y), CColor(255, 255, 255, lit ? 120 : 40),
                    CColor(255, 255, 255, 0));
    }

    // A ladder of lamps: five for the pocket recorders, twelve a channel on a desk; the peak held a moment.
    void drawLeds(CDrawContext* c, const CRect& r)
    {
        fillRound(c, r, 6, CColor(40, 40, 42), CColor(12, 12, 12));
        static const int kFive[] = { -20, -10, -5, 0, 4 };
        static const int kTwelve[] = { -30, -20, -10, -7, -4, -2, 0, 2, 4, 7, 10, 16 };
        const bool five = look_.meters == 1;
        const int* steps = five ? kFive : kTwelve;
        const int n = five ? 5 : 12;
        const CCoord left = r.left + 22, width = (r.getWidth() - 32) / n;
        c->setFont(kNormalFontVerySmall);
        for (int ch = 0; ch < (five ? 1 : 2); ++ch) {
            const CCoord top = r.top + 14 + ch * 34;
            if (!five) {
                c->setFontColor(CColor(200, 200, 196));
                c->drawString(ch ? "R" : "L", CRect(r.left + 6, top, left - 2, top + 14), kLeftText);
            }
            for (int i = 0; i < n; ++i) {
                const double t = position(steps[i]);
                const bool on = pos_[ch] >= t - 1e-9 || std::fabs(hold_[ch] - t) < 0.5 / n;
                const CColor hue = steps[i] > (five ? 0 : 4) ? CColor(255, 60, 40)
                                   : steps[i] >= 0           ? CColor(255, 200, 40)
                                                             : CColor(60, 230, 90);
                CRect seg(left + i * width + 2, top, left + (i + 1) * width - 2, top + (five ? 16 : 14));
                fillRound(c, seg, 2, on ? shade(hue, 1.1) : shade(hue, 0.22), on ? shade(hue, 0.8) : shade(hue, 0.14));
            }
        }
        c->setFontColor(CColor(170, 170, 166));
        const CCoord ty = r.top + (five ? 36 : 82);
        for (int i = 0; i < n; ++i) {
            char label[8];
            if (!five && i == n - 1) std::snprintf(label, sizeof label, "CLIP");
            else std::snprintf(label, sizeof label, steps[i] > 0 ? "+%d" : "%d", steps[i]);
            const CCoord x = left + (i + 0.5) * width;
            c->drawString(label, CRect(x - 14, ty, x + 14, ty + 12), kCenterText);
        }
    }

    // No meter: automatic level control (a lamp that lights with the sound), or nothing at all.
    void drawNote(CDrawContext* c, const CRect& r)
    {
        const bool automatic = look_.meter == AuMeter::Automatic;
        c->setFontColor(isLight(rgb(look_.body)) ? CColor(30, 28, 26) : CColor(220, 216, 206));
        c->setFont(kNormalFontSmaller);
        c->drawString(automatic ? "LIVELLO AUTOMATICO" : "nessun indicatore: la tromba incide la cera",
                      CRect(r.left, r.top + 8, r.right, r.top + 24), kCenterText);
        if (!automatic) return;
        const CPoint m(r.getCenter().x, r.top + 60);
        const double k = std::clamp(pos_[0], 0.0, 1.0);
        fillEllipse(c, CRect(m.x - 12, m.y - 12, m.x + 12, m.y + 12), CColor(60, 50, 40), CColor(20, 16, 12));
        fillEllipse(c, CRect(m.x - 8, m.y - 8, m.x + 8, m.y + 8), CColor(uint8_t(60 + 190 * k), uint8_t(40 + 150 * k), 20),
                    CColor(uint8_t(30 + 140 * k), uint8_t(20 + 90 * k), 10));
    }

    void lamp(CDrawContext* c, const CRect& r, bool on)
    {
        fillEllipse(c, r, on ? CColor(255, 90, 60) : CColor(90, 24, 18), on ? CColor(220, 30, 20) : CColor(50, 14, 10));
    }

    AuLook look_ = kAuDeskLooks[0];
    bool percent_ = false;
    double pos_[4] = {}, hold_[4] = {}, heldFor_[4] = {}, lamp_[4] = {}, levels_[4] = { -100, -100, -100, -100 };
};

// One control of a front (AuFronts.h), drawn as the machine had it: pot, pointer selector, coin slot, bat lever,
// slide, keys, fader, quadrant fader, push-button. Its value is the position (0..1) or the index of the position. The
// view is larger than the control, for the engraving around it; only the control itself takes the mouse.
class FrontControl : public CControl
{
public:
    static constexpr CCoord kMx = 30, kTop = 28, kBottom = 16;
    static CCoord margin(const AuControl& d) { return d.kind == AuWidget::Selector || d.kind == AuWidget::Coin ? 72 : kMx; }
    FrontControl(const CRect& inner, IControlListener* listener, int32_t tag, const AuControl& d, const Skin& skin)
        : CControl(CRect(inner.left - margin(d), inner.top - kTop, inner.right + margin(d), inner.bottom + kBottom), listener, tag),
          inner_(inner), d_(d), skin_(skin), dial_(splitBars(d.dial))
    {
        for (int i = 0; i < d.count; ++i) labels_.push_back(d.pos[i].label);
        if (d.use == AuUse::Idle) {
            setMouseEnabled(false);
            setAlphaValue(0.4f);
        }
    }
    const AuControl& def() const { return d_; }
    bool discrete() const { return d_.pos != nullptr; }
    int index() const { return d_.count > 1 ? int(std::lround(getValueNormalized() * (d_.count - 1))) : 0; }
    void setIndex(int i) { setValueNormalized(d_.count > 1 ? float(std::clamp(i, 0, d_.count - 1)) / (d_.count - 1) : 0); }
    bool pressed() const { return pressed_; }
    CLASS_METHODS(FrontControl, CControl)

    bool hitTest(const CPoint& where, const Event& event) override { return inner_.pointInside(where); }

    CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons) override
    {
        if (!buttons.isLeftButton()) return kMouseEventNotHandled;
        const int i = index(), n = d_.count;
        switch (d_.kind) {
        case AuWidget::Knob:
        case AuWidget::Fader:
        case AuWidget::Quadrant:
            if (buttons.isDoubleClick()) {
                beginEdit();
                setValueNormalized(getDefaultValue());
                valueChanged();
                endEdit();
                invalid();
                return kMouseEventHandled;
            }
            dragFrom_ = where, valueFrom_ = getValueNormalized();
            beginEdit();
            return kMouseEventHandled;
        case AuWidget::Selector:
        case AuWidget::Coin: pick(where.x < inner_.getCenter().x ? i - 1 : i + 1); break;
        case AuWidget::Lever: pick(n == 2 ? 1 - i : where.y < inner_.getCenter().y ? i - 1 : i + 1); break;
        case AuWidget::Slide:
        case AuWidget::Keys: pick(int((where.x - inner_.left) / inner_.getWidth() * n)); break;
        case AuWidget::Button:
            pressed_ = true;
            valueChanged();
            invalid();
            return kMouseEventHandled;
        }
        return kMouseEventHandled;
    }
    CMouseEventResult onMouseMoved(CPoint& where, const CButtonState& buttons) override
    {
        if (!buttons.isLeftButton() || discrete()) return kMouseEventNotHandled;
        const double travel = d_.kind == AuWidget::Knob ? 160 : inner_.getHeight();
        setValueNormalized(float(std::clamp(valueFrom_ + (dragFrom_.y - where.y) / travel, 0.0, 1.0)));
        valueChanged();
        invalid();
        return kMouseEventHandled;
    }
    CMouseEventResult onMouseUp(CPoint& where, const CButtonState& buttons) override
    {
        if (d_.kind == AuWidget::Button) {
            pressed_ = false;
            valueChanged();
            invalid();
        } else if (!discrete()) {
            endEdit();
        }
        return kMouseEventHandled;
    }

    void draw(CDrawContext* c) override
    {
        ink_ = isLight(skin_.body) ? CColor(24, 22, 20) : CColor(236, 232, 222);
        cap_ = d_.cap ? rgb(d_.cap) : skin_.cap;
        switch (d_.kind) {
        case AuWidget::Knob: drawKnob(c); break;
        case AuWidget::Selector: drawSelector(c, false); break;
        case AuWidget::Coin: drawSelector(c, true); break;
        case AuWidget::Lever: drawLever(c); break;
        case AuWidget::Slide: drawSlide(c); break;
        case AuWidget::Keys: drawKeys(c); break;
        case AuWidget::Fader: drawFader(c); break;
        case AuWidget::Quadrant: drawQuadrant(c); break;
        case AuWidget::Button: drawButton(c); break;
        }
        setDirty(false);
    }

private:
    void pick(int i)
    {
        i = std::clamp(i, 0, d_.count - 1);
        if (i == index()) return;
        setIndex(i);
        valueChanged();
        invalid();
    }
    void text(CDrawContext* c, const std::string& s, const CRect& r, CHoriTxtAlign align = kCenterText,
              const CFontRef font = kNormalFontVerySmall)
    {
        if (s.empty()) return;
        c->setFont(font);
        c->setFontColor(ink_);
        c->drawString(s.c_str(), r, align);
    }
    void caption(CDrawContext* c, CCoord y)   // the control's name, engraved under it
    {
        text(c, d_.label, CRect(inner_.left - kMx - 10, y, inner_.right + kMx + 10, y + 12));
    }
    void cap(CDrawContext* c, const CRect& body)
    {
        CRect shadow = body;
        shadow.offset(1, 2);
        c->setFillColor(CColor(0, 0, 0, 90));
        c->drawEllipse(shadow, kDrawFilled);
        fillEllipse(c, body, shade(cap_, 1.5), shade(cap_, 0.7));
        CRect top = body;
        top.inset(body.getWidth() * 0.12, body.getWidth() * 0.12);
        fillEllipse(c, top, shade(cap_, 1.15), shade(cap_, 0.9));
    }
    CColor pointerInk() const { return cap_.red + cap_.green + cap_.blue > 3 * 110 ? CColor(24, 24, 24) : CColor(240, 236, 226); }

    void drawKnob(CDrawContext* c)
    {
        const CPoint m = inner_.getCenter();
        const CCoord d = std::min(inner_.getWidth(), inner_.getHeight());
        const bool numbered = dial_.size() > 1 && !(dial_.size() == 2 && dial_[0].empty() && dial_[1].empty());
        c->setFillColor(shade(ink_, 1, 200));
        for (int i = 0; i <= 10; ++i) {   // the scale of eleven dots
            const CPoint p = polar(m, -135 + 27 * i, d / 2 + 3);
            c->drawEllipse(CRect(p.x - 1.2, p.y - 1.2, p.x + 1.2, p.y + 1.2), kDrawFilled);
        }
        if (numbered)
            for (size_t i = 0; i < dial_.size(); ++i) {
                const CPoint p = polar(m, -135 + 270.0 * i / (dial_.size() - 1), d / 2 + 13);
                text(c, dial_[i], CRect(p.x - 16, p.y - 6, p.x + 16, p.y + 6));
            }
        CRect body(m.x - d / 2 + 3, m.y - d / 2 + 3, m.x + d / 2 - 3, m.y + d / 2 - 3);
        cap(c, body);
        c->setFrameColor(pointerInk());
        c->setLineWidth(2);
        const double a = -135 + 270 * getValueNormalized();
        c->drawLine(polar(m, a, body.getWidth() * 0.12), polar(m, a, body.getWidth() / 2 - 2));
        caption(c, inner_.bottom + 3);
    }
    // A pointer knob (or a coin slot) with the name of each position around it.
    void drawSelector(CDrawContext* c, bool coin)
    {
        const CPoint m = inner_.getCenter();
        const CCoord d = std::min(inner_.getWidth(), inner_.getHeight());
        const int n = d_.count;
        const double step = n <= 2 ? 70 : n <= 4 ? 56 : n <= 6 ? 46 : 300.0 / (n - 1);   // labels need room
        const double span = step * (n - 1);
        auto angle = [&](int i) { return n > 1 ? -span / 2 + span * i / (n - 1) : 0; };
        const CCoord r = d / 2 - 6;
        for (int i = 0; i < n; ++i) {
            const double a = angle(i);
            c->setFrameColor(ink_);
            c->setLineWidth(1);
            c->drawLine(polar(m, a, r + 2), polar(m, a, r + 6));
            const CPoint p = polar(m, a, r + 12);
            const CCoord w = 70;
            const CHoriTxtAlign align = std::fabs(a) < 15 || std::fabs(a) > 165 ? kCenterText : a < 0 ? kRightText : kLeftText;
            const CCoord x = align == kCenterText ? p.x - w / 2 : align == kRightText ? p.x - w + 2 : p.x - 2;
            text(c, labels_[size_t(i)], CRect(x, p.y - 6, x + w, p.y + 6), align);
        }
        CRect body(m.x - r + 2, m.y - r + 2, m.x + r - 2, m.y + r - 2);
        const double a = angle(index());
        if (coin) {   // flush with the plate: a disc and its slot
            fillEllipse(c, body, shade(skin_.body2, 0.8), shade(skin_.body, 1.2));
            c->setFrameColor(CColor(10, 10, 10));
            c->setLineWidth(3);
            c->drawLine(polar(m, a, r * 0.75), polar(m, a + 180, r * 0.75));
        } else {      // a chicken-head pointer over a round base
            cap(c, body);
            auto beak = VSTGUI::owned(c->createGraphicsPath());
            beak->beginSubpath(polar(m, a, r + 4));
            beak->addLine(polar(m, a + 90, r * 0.32));
            beak->addLine(polar(m, a + 180, r * 0.55));
            beak->addLine(polar(m, a - 90, r * 0.32));
            beak->closeSubpath();
            fillGradient(c, beak, body, shade(cap_, 1.35), shade(cap_, 0.75));
            c->setFrameColor(pointerInk());
            c->setLineWidth(1.5);
            c->drawLine(polar(m, a, r * 0.2), polar(m, a, r + 2));
        }
        caption(c, inner_.bottom + 3);
    }
    void drawLever(CDrawContext* c)
    {
        const CPoint m = inner_.getCenter();
        CRect plate = inner_;
        plate.inset(inner_.getWidth() * 0.2, 4);
        fillRound(c, plate, 3, shade(skin_.body, 1.25), shade(skin_.body2, 0.8));
        c->setFrameColor(shade(skin_.body2, 0.5));
        c->drawGraphicsPath(VSTGUI::owned(c->createRoundRectGraphicsPath(plate, 3)), CDrawContext::kPathStroked);
        const int n = d_.count, i = index();
        const double t = n > 1 ? double(i) / (n - 1) : 0.5;   // 0 up, 1 down
        const CPoint tip(m.x, inner_.top + 4 + t * (inner_.getHeight() - 8));
        fillEllipse(c, CRect(m.x - 7, m.y - 7, m.x + 7, m.y + 7), CColor(210, 210, 206), CColor(90, 90, 88));
        c->setFrameColor(CColor(70, 70, 68));
        c->setLineWidth(5);
        c->drawLine(m, tip);
        c->setFrameColor(CColor(230, 230, 226));
        c->setLineWidth(3);
        c->drawLine(m, tip);
        fillEllipse(c, CRect(tip.x - 5, tip.y - 5, tip.x + 5, tip.y + 5), CColor(250, 250, 246), CColor(150, 150, 146));
        text(c, labels_.front(), CRect(inner_.left - kMx, inner_.top - 13, inner_.right + kMx, inner_.top - 1));
        text(c, labels_.back(), CRect(inner_.left - kMx, inner_.bottom + 1, inner_.right + kMx, inner_.bottom + 13));
        if (n == 3) text(c, labels_[1], CRect(inner_.right - 4, m.y - 6, inner_.right + kMx, m.y + 6), kLeftText);
        text(c, d_.label, CRect(inner_.left - kMx, inner_.top - 26, inner_.right + kMx, inner_.top - 14));
    }
    void drawSlide(CDrawContext* c)
    {
        const int n = d_.count, i = index();
        const CCoord w = inner_.getWidth() / n;
        fillRound(c, inner_, 3, shade(skin_.body2, 0.4), shade(skin_.body2, 0.7));
        CRect knob(inner_.left + i * w + 2, inner_.top + 2, inner_.left + (i + 1) * w - 2, inner_.bottom - 2);
        fillRound(c, knob, 2, shade(cap_, 1.6), shade(cap_, 0.9));
        for (CCoord x = knob.left + 4; x < knob.right - 3; x += 3) {
            c->setFrameColor(shade(cap_, 0.6));
            c->drawLine(CPoint(x, knob.top + 3), CPoint(x, knob.bottom - 3));
        }
        for (int k = 0; k < n; ++k)
            text(c, labels_[size_t(k)], CRect(inner_.left + k * w - 6, inner_.top - 13, inner_.left + (k + 1) * w + 6, inner_.top - 1));
        caption(c, inner_.bottom + 2);
    }
    void drawKeys(CDrawContext* c)
    {
        const int n = d_.count, i = index();
        const CCoord w = inner_.getWidth() / n;
        for (int k = 0; k < n; ++k) {
            const bool down = k == i && d_.kind == AuWidget::Keys;
            const CColor hue = d_.pos[k].colour ? rgb(d_.pos[k].colour) : isLight(skin_.body) ? CColor(64, 62, 60) : CColor(214, 210, 200);
            CRect key(inner_.left + k * w + 2, inner_.top + (down ? 3 : 0), inner_.left + (k + 1) * w - 2, inner_.bottom);
            c->setFillColor(CColor(0, 0, 0, 110));
            c->drawRect(CRect(key.left, inner_.top + 3, key.right, inner_.bottom + 2), kDrawFilled);
            fillRound(c, key, 2, shade(hue, down ? 0.85 : 1.15), shade(hue, down ? 0.65 : 0.85));
            const CColor on = isLight(hue) ? CColor(20, 20, 20) : CColor(240, 238, 230);
            c->setFont(kNormalFontVerySmall);
            c->setFontColor(on);
            if (c->getStringWidth(labels_[size_t(k)].c_str()) < w - 6)
                c->drawString(labels_[size_t(k)].c_str(), key, kCenterText);
            else
                text(c, labels_[size_t(k)], CRect(key.left - 8, inner_.top - 13, key.right + 8, inner_.top - 1));
        }
        caption(c, inner_.bottom + 3);
    }
    void drawFader(CDrawContext* c)
    {
        const CPoint m = inner_.getCenter();
        const CCoord top = inner_.top + 8, bottom = inner_.bottom - 8;
        CRect slot(m.x - 2, top, m.x + 2, bottom);
        fillRound(c, slot, 2, CColor(10, 10, 10), CColor(40, 40, 40));
        for (size_t k = 0; k < dial_.size(); ++k) {   // the numbers down the left side, top first
            const CCoord y = top + (bottom - top) * k / std::max<size_t>(1, dial_.size() - 1);
            c->setFrameColor(ink_);
            c->drawLine(CPoint(inner_.left - 2, y), CPoint(inner_.left + 4, y));
            text(c, dial_[k], CRect(inner_.left - kMx, y - 6, inner_.left - 3, y + 6), kRightText);
        }
        const CCoord y = bottom - (bottom - top) * getValueNormalized();
        CRect knob(inner_.left, y - 9, inner_.right, y + 9);
        fillRound(c, knob, 2, shade(cap_, 1.5), shade(cap_, 0.7));
        c->setFrameColor(pointerInk());
        c->setLineWidth(1.5);
        c->drawLine(CPoint(knob.left + 2, y), CPoint(knob.right - 2, y));
        caption(c, inner_.bottom + 3);
    }
    // A quadrant fader: the lever pivots below the panel, its knob travels an arc.
    void drawQuadrant(CDrawContext* c)
    {
        const CPoint pivot(inner_.getCenter().x, inner_.bottom + inner_.getHeight() * 0.9);
        const double radius = inner_.getHeight() * 1.6;
        const double v = getValueNormalized();
        auto at = [&](double t, double rad) { return polar(pivot, -12 + 24 * (1 - t), rad); };
        (void)at;
        const CCoord top = inner_.top + 8, bottom = inner_.bottom - 8;
        CRect slot(inner_.getCenter().x - 3, top, inner_.getCenter().x + 3, bottom);
        fillRound(c, slot, 3, CColor(16, 16, 16), CColor(54, 50, 46));
        for (int k = 0; k <= 10; ++k) {
            const CCoord yy = top + (bottom - top) * k / 10.0;
            c->setFrameColor(ink_);
            c->drawLine(CPoint(inner_.left + 2, yy), CPoint(inner_.left + (k % 5 ? 6 : 9), yy));
        }
        const CCoord y = bottom - (bottom - top) * v;
        CRect knob(inner_.left + 3, y - 7, inner_.right - 3, y + 7);
        fillRound(c, knob, 6, shade(cap_, 1.7), shade(cap_, 0.8));
        c->setFrameColor(pointerInk());
        c->drawLine(CPoint(knob.left + 3, y), CPoint(knob.right - 3, y));
        (void)radius;
        caption(c, inner_.bottom + 2);
    }
    void drawButton(CDrawContext* c)
    {
        CRect body = inner_;
        fillEllipse(c, body, shade(skin_.body2, 0.5), shade(skin_.body, 1.2));
        body.inset(4, 4);
        if (pressed_) body.offset(0, 1);
        fillEllipse(c, body, shade(cap_, pressed_ ? 0.8 : 1.4), shade(cap_, pressed_ ? 0.6 : 0.8));
        text(c, labels_.front(), CRect(inner_.left - kMx, inner_.top - 13, inner_.right + kMx, inner_.top - 1));
        caption(c, inner_.bottom + 2);
    }

    CRect inner_;
    AuControl d_;
    Skin skin_;
    std::vector<std::string> dial_, labels_;
    CColor ink_, cap_;
    CPoint dragFrom_;
    double valueFrom_ = 0;
    bool pressed_ = false;
};

// The «?» on the name plate: while the pointer is over it, the card with the machine's story shows.
class HelpBadge : public CView
{
public:
    HelpBadge(const CRect& r, std::function<void(bool)> hover) : CView(r), hover_(std::move(hover)) {}
    void draw(CDrawContext* c) override
    {
        const CRect r = getViewSize();
        fillEllipse(c, r, over_ ? CColor(250, 226, 160) : CColor(236, 230, 214), over_ ? CColor(214, 180, 110) : CColor(170, 162, 146));
        c->setFrameColor(CColor(60, 56, 50));
        c->setLineWidth(1);
        auto path = VSTGUI::owned(c->createGraphicsPath());
        path->addEllipse(r);
        c->drawGraphicsPath(path, CDrawContext::kPathStroked);
        c->setFont(kNormalFontSmall);
        c->setFontColor(CColor(40, 36, 32));
        c->drawString("?", r, kCenterText);
        setDirty(false);
    }
    CMouseEventResult onMouseEntered(CPoint&, const CButtonState&) override { return show(true); }
    CMouseEventResult onMouseExited(CPoint&, const CButtonState&) override { return show(false); }

private:
    CMouseEventResult show(bool on)
    {
        over_ = on;
        invalid();
        hover_(on);
        return kMouseEventHandled;
    }
    std::function<void(bool)> hover_;
    bool over_ = false;
};

class Panel : public VSTGUIEditor, public IControlListener
{
public:
    static constexpr CCoord kW = 660, kRow = 24, kFace = 268, kTop = kFace + 12;
    static constexpr int32_t kFrontTag = 10000;   // tags of front controls: kFrontTag + index in the front

    Panel(EditController* controller, bool mix, ViewRect size) : VSTGUIEditor(controller, &size), mix_(mix)
    {
        for (AuParamId id : auParams(mix)) has_[id] = true;
    }
    static constexpr CCoord kGuideLines = 64;   // the guide's advice under its menu
    static constexpr int32_t kApplyTag = 20000;
    static ViewRect sizeFor(bool mix)
    {
        return ViewRect(0, 0, int32(kW), int32(kTop + (auRows(mix).size() + 1) * kRow + kGuideLines + 10));
    }

    bool PLUGIN_API open(void* parent, const PlatformType& platform) override
    {
        if (frame) return false;
        const ViewRect& r = getRect();
        frame = new CFrame(CRect(0, 0, r.getWidth(), r.getHeight()), this);
        frame->setBackgroundColor(CColor(30, 28, 26));
        plate_ = new Plate(CRect(8, 8, kW - 8, kFace));   // the front: name plate, meters, controls
        frame->addView(plate_);
        meters_ = new Meters(CRect(0, 0, 10, 10));
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
        // The guide: what is being recorded, the advice for this machine, «Applica».
        auto* label = new CTextLabel(CRect(12, y, 172, y + kRow - 4), mix_ ? "Cosa missi" : "Cosa registri");
        style(label, kNormalFont, CColor(232, 196, 120));
        label->setHoriAlign(kLeftText);
        frame->addView(label);
        addControl(kPSource, CRect(204, y, kW - 170, y + kRow - 4));
        auto* apply = new CTextButton(CRect(kW - 162, y, kW - 84, y + kRow - 4), this, kApplyTag, "Applica");
        apply->setFont(kNormalFontSmall);
        apply->setTextColor(CColor(240, 234, 220));
        apply->setGradient(VSTGUI::owned(CGradient::create(0, 1, CColor(84, 76, 66), CColor(52, 48, 44))));
        apply->setGradientHighlighted(VSTGUI::owned(CGradient::create(0, 1, CColor(150, 120, 70), CColor(110, 88, 52))));
        apply->setFrameColor(CColor(120, 110, 96));
        apply->setRoundRadius(3);
        frame->addView(apply);
        y += kRow;
        advice_ = new CMultiLineTextLabel(CRect(12, y, kW - 12, y + kGuideLines));
        advice_->setLineLayout(CMultiLineTextLabel::LineLayout::wrap);
        style(advice_, kNormalFontSmall, CColor(208, 200, 184));
        advice_->setHoriAlign(kLeftText);
        frame->addView(advice_);
        // The «?» and its card, above everything else (dress() keeps them on top).
        card_ = new CMultiLineTextLabel(CRect(0, 0, 10, 10));
        card_->setLineLayout(CMultiLineTextLabel::LineLayout::wrap);
        style(card_, kNormalFontSmall, CColor(34, 30, 26));
        card_->setBackColor(CColor(246, 240, 226));
        card_->setFrameColor(CColor(90, 80, 66));
        card_->setStyle(CParamDisplay::kRoundRectStyle);
        card_->setRoundRectRadius(6);
        card_->setTextInset(CPoint(12, 12));
        card_->setHoriAlign(kLeftText);
        card_->setMouseEnabled(false);
        card_->setVisible(false);
        badge_ = new HelpBadge(CRect(0, 0, 16, 16), [this](bool on) {
            if (card_) {   // the moving meter would draw through the card: it rests while the story is read
                card_->setVisible(on);
                if (meters_ && card_->getViewSize().rectOverlap(meters_->getViewSize())) meters_->setVisible(!on);
                frame->invalid();
            }
        });
        frame->addView(badge_);
        frame->addView(card_);
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
        front_.clear();
        plate_ = nullptr;
        meters_ = nullptr;
        badge_ = nullptr;
        card_ = nullptr;
        advice_ = nullptr;
        peaks_.clear();
        look_ = -1;
        values_.clear();
        if (frame) {
            frame->forget();
            frame = nullptr;
        }
    }

    void valueChanged(CControl* control) override
    {
        if (control->getTag() == kApplyTag) {
            if (control->getValue() > 0.5f) applyGuide();
            return;
        }
        if (control->getTag() >= kFrontTag) return frontChanged(static_cast<FrontControl*>(control));
        ParamID id = ParamID(control->getTag());
        for (Menu& m : menus_)
            if (m.menu == control) {   // a menu entry stands for a plain value; its children follow it
                int i = int(m.menu->getCurrentIndex());
                if (i < 0 || i >= int(m.plains.size()) || m.plains[size_t(i)] < 0) return sync(true);
                edit(id, m.plains[size_t(i)]);
                if (id == kPEraSetup)   // an era: its recorder or desk, then what goes with it
                    for (auto [p, plain] : auSetupValues(m.plains[size_t(i)], mix_))
                        if (has_[p]) editWhole(p, plain);
                coerce();
                return sync(true);
            }
        ParamValue n = control->getValueNormalized();
        getController()->setParamNormalized(id, n);
        getController()->performEdit(id, n);
        showValue(id);
        sync();
    }
    void controlBeginEdit(CControl* control) override
    {
        if (int id = paramOf(control); id >= 0) getController()->beginEdit(ParamID(id));
    }
    void controlEndEdit(CControl* control) override
    {
        if (int id = paramOf(control); id >= 0) getController()->endEdit(ParamID(id));
    }

private:
    struct Menu
    {
        COptionMenu* menu;
        AuParamId id;
        std::vector<int> plains;   // per entry: the parameter's plain value, -1 = a family title
        std::vector<std::string> names;
    };
    struct Front
    {
        FrontControl* view;
        int index;   // the position last chosen (discrete controls), kept when several positions match the values
    };

    int paramOf(CControl* control) const
    {
        const int32_t tag = control->getTag();
        if (tag == kApplyTag) return -1;
        if (tag < kFrontTag) return tag;
        const AuControl& d = static_cast<FrontControl*>(control)->def();
        return !d.pos && d.use == AuUse::Live && d.param >= 0 && has_[d.param] ? d.param : -1;
    }
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
    void editWhole(int id, double plain)   // one complete gesture, for a switch or a menu's consequences
    {
        getController()->beginEdit(ParamID(id));
        edit(id, plain);
        getController()->endEdit(ParamID(id));
    }
    void coerce()   // children that no longer fit their parents move to the first that does
    {
        double v[kPCount];
        values(v);
        for (AuParamId child : auCoerce(v, mix_)) editWhole(child, v[child]);
    }

    // A front control moved: a pot sets its parameter, a switch sets everything its position stands for.
    void frontChanged(FrontControl* view)
    {
        auto it = std::find_if(front_.begin(), front_.end(), [&](const Front& f) { return f.view == view; });
        if (it == front_.end()) return;
        const AuControl& d = view->def();
        if (d.kind == AuWidget::Button) {
            tone_ = view->pressed() && d.pos[0].a.target == kSetTone ? d.pos[0].a.value : kNoTone;
            if (view->pressed() && auPositionReads(d.pos[0]) >= 0) heldReads_ = auPositionReads(d.pos[0]);
            if (!view->pressed()) heldReads_ = -1;
            return;
        }
        if (!d.pos) {
            if (int id = paramOf(view); id >= 0) edit(id, d.from + (d.to - d.from) * view->getValueNormalized());
            return;
        }
        it->index = view->index();
        const AuPos& p = d.pos[it->index];
        if (int r = auPositionReads(p); r >= 0) reads_ = r;
        if (d.use != AuUse::Live) return;
        double v[kPCount];
        values(v);
        for (auto [id, plain] : auPositionValues(p, v, mix_))
            if (has_[id]) editWhole(id, plain);
        coerce();
        sync(true);
    }
    // Does a position stand for the values the parameters hold now? One with no parameter never does by itself.
    bool matches(const AuPos& p, const double* v) const
    {
        auto want = auPositionValues(p, v, mix_);
        bool any = false;
        for (auto [id, plain] : want) {
            if (!has_[id]) continue;
            any = true;
            if (std::fabs(v[id] - plain) > 1e-6) return false;
        }
        return any;
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
    // Puts on the front of the chosen recorder or desk: its plate, its meters, its controls where it had them.
    void dress(const double* v)
    {
        int family = 0, model = 0;
        auDeviceSplit(auPlain(v, kPDevice), family, model);
        const int desk = auPlain(v, kPDesk);
        const int look = mix_ ? desk : family * 100 + model;
        if (look == look_) return;
        look_ = look;
        const AuLook& data = auLook(v, mix_);
        const AuFront& front = auFront(v, mix_);
        const Skin skin(data);
        const CPoint origin(8, 8);
        plate_->set(skin, mix_ ? "LOG METARAW · MIX E SUPPORTO" : "LOG METARAW · RIPRESA",
                    mix_ ? (desk ? kAuConsoles[desk].name : "Senza banco") : kAuFamilies[family].devices[model].name,
                    mix_ ? "Banco di missaggio" : kAuFamilies[family].name, CPoint(front.nx, front.ny));
        meters_->setViewSize(CRect(origin.x + front.mx, origin.y + front.my, origin.x + front.mx + front.mw,
                                   origin.y + front.my + front.mh));
        meters_->setMouseableArea(meters_->getViewSize());
        meters_->setLook(data, mix_ && desk == 1);   // the RCA's VU has the type B (per cent) scale
        for (Front& f : front_) frame->removeView(f.view, true);
        front_.clear();
        for (int i = 0; i < front.count; ++i) {
            const AuControl& d = front.controls[i];
            CRect inner(origin.x + d.x, origin.y + d.y, origin.x + d.x + d.w, origin.y + d.y + d.h);
            auto* view = new FrontControl(inner, this, kFrontTag + i, d, skin);
            if (d.pos) view->setIndex(d.def);
            else if (d.param >= 0 && d.to != d.from)
                view->setDefaultValue(float(std::clamp((auDefault(AuParamId(d.param), mix_) - d.from) / (d.to - d.from), 0.0, 1.0)));
            frame->addView(view);
            front_.push_back({ view, d.pos ? d.def : 0 });
        }
        // the «?» at the right of the name plate; the story below it, inside the front
        const CPoint at(origin.x + front.nx, origin.y + front.ny);
        badge_->setViewSize(CRect(at.x + 180, at.y + 4, at.x + 196, at.y + 20));
        badge_->setMouseableArea(badge_->getViewSize());
        const CCoord cw = 400, ch = 96;
        // under the «?», over the plate and the rows if it must: they stand still while it is read
        const CCoord cx = std::clamp(at.x + 196 - cw, CCoord(14), kW - 14 - cw), cy = at.y + 24;
        card_->setViewSize(CRect(cx, cy, cx + cw, cy + ch));
        card_->setText(auStory(v, mix_));
        for (CView* top : { static_cast<CView*>(badge_), static_cast<CView*>(card_) }) {
            top->remember();
            frame->removeView(top, false);
            frame->addView(top);
            top->forget();
        }
        reads_ = front.reads;
        tone_ = kNoTone;
        heldReads_ = -1;
        frame->invalid();
        place(v, true);
        for (Front& f : front_)   // what the meter reads follows the switches as they stand
            if (f.view->def().pos)
                if (int r = auPositionReads(f.view->def().pos[f.index]); r >= 0) reads_ = r;
    }
    // Moves the front's controls to the values the parameters hold (the host, automation, a menu).
    void place(const double* v, bool all)
    {
        for (Front& f : front_) {
            FrontControl* view = f.view;
            const AuControl& d = view->def();
            if (d.kind == AuWidget::Button || d.use != AuUse::Live) continue;
            if (!d.pos) {
                if (d.param < 0 || !has_[d.param] || d.to == d.from || view->isEditing()) continue;
                const float n = float(std::clamp((v[d.param] - d.from) / (d.to - d.from), 0.0, 1.0));
                if (all || std::fabs(view->getValueNormalized() - n) > 1e-4f) {
                    view->setValueNormalized(n);
                    view->invalid();
                }
                continue;
            }
            int at = f.index;
            if (!matches(d.pos[at], v))
                for (int i = 0; i < d.count; ++i)
                    if (matches(d.pos[i], v)) {
                        at = i;
                        break;
                    }
            if (at != view->index() || all) {
                f.index = at;
                view->setIndex(at);
                view->invalid();
            }
        }
    }
    void sync(bool all = false)   // the host or automation may have moved a parameter
    {
        double v[kPCount];
        values(v);
        for (Menu& m : menus_)
            if (all || !m.menu->isEditing()) fill(m, v);
        dress(v);
        place(v, all);
        double db[4];   // the readings the processor reported, against the calibration
        const int reads = heldReads_ >= 0 ? heldReads_ : reads_;
        for (int c = 0; c < 4; ++c) {
            const bool in = reads == kReadIn;
            const int ch = c & 1;
            double level = auMeterFromNorm(getController()->getParamNormalized(ParamID((in ? kPMeterInL : kPMeterL) + ch)));
            if (in) level += v[kPRecLevel];   // the record level is before the meter
            db[c] = c < 2 ? level - v[kPMeterRef] : -100;   // tracks 3 and 4 get nothing from a stereo signal
            if (tone_ != kNoTone && c < 2) db[c] = tone_;
        }
        const Meters::Mode mode = tone_ != kNoTone ? Meters::kLevel
                                  : reads == kReadBattery ? Meters::kBattery
                                  : reads == kReadFixed   ? Meters::kFixed
                                  : reads == kReadZero    ? Meters::kZero
                                                          : Meters::kLevel;
        meters_->feed(db, mode, kTick / 1000.0);
        guide(v);
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

    // The peaks of the last seconds on the machine's own meter, before the record level: what «Applica» works from.
    double measured() const
    {
        double top = kAuNoMeasure;
        for (double p : peaks_) top = std::max(top, p);
        return top > -40 ? top : kAuNoMeasure;
    }
    void guide(const double* v)
    {
        double in = -100;
        for (int c = 0; c < 2; ++c)
            in = std::max(in, auMeterFromNorm(getController()->getParamNormalized(ParamID(kPMeterInL + c))) - v[kPMeterRef]);
        peaks_.push_back(in);
        while (peaks_.size() > size_t(8000 / kTick)) peaks_.pop_front();   // eight seconds
        const std::string text = auGuideText(v, mix_, measured());
        if (advice_ && text != adviceText_) {
            adviceText_ = text;
            advice_->setText(text.c_str());
            advice_->invalid();
        }
    }
    void applyGuide()
    {
        double v[kPCount];
        values(v);
        for (auto [id, plain] : auGuideApply(v, mix_, measured()))
            if (has_[id]) editWhole(id, plain);
        coerce();
        sync(true);
    }

    static constexpr int kTick = 33;   // ms: meters at 30 frames a second
    static constexpr double kNoTone = -1000;

    bool mix_;
    bool has_[kPCount] = {};
    Plate* plate_ = nullptr;
    Meters* meters_ = nullptr;
    HelpBadge* badge_ = nullptr;
    CMultiLineTextLabel* card_ = nullptr;
    CMultiLineTextLabel* advice_ = nullptr;
    std::string adviceText_;
    std::deque<double> peaks_;
    std::vector<Front> front_;
    int look_ = -1;
    int reads_ = kReadOut, heldReads_ = -1;
    double tone_ = kNoTone;   // the reference tone the meter reads while its button is held
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

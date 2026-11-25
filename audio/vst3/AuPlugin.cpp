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

class Panel : public VSTGUIEditor, public IControlListener
{
public:
    static constexpr CCoord kW = 660, kRow = 28, kTop = 48;

    Panel(EditController* controller, bool mix, ViewRect size) : VSTGUIEditor(controller, &size), mix_(mix) {}
    static ViewRect sizeFor(bool mix) { return ViewRect(0, 0, int32(kW), int32(kTop + auRows(mix).size() * kRow + 12)); }

    bool PLUGIN_API open(void* parent, const PlatformType& platform) override
    {
        if (frame) return false;
        const ViewRect& r = getRect();
        frame = new CFrame(CRect(0, 0, r.getWidth(), r.getHeight()), this);
        frame->setBackgroundColor(CColor(30, 28, 26));
        auto* title = new CTextLabel(CRect(12, 8, kW - 12, 36), mix_ ? "Log MetaRaw · Mix e supporto" : "Log MetaRaw · Ripresa");
        style(title, kNormalFontBig, CColor(232, 196, 120));
        title->setHoriAlign(kLeftText);
        frame->addView(title);
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
        timer_ = makeOwned<CVSTGUITimer>([this](CVSTGUITimer*) { sync(); }, 80);
        sync(true);
        return true;
    }
    void PLUGIN_API close() override
    {
        timer_ = nullptr;
        controls_.clear();
        values_.clear();
        if (frame) {
            frame->forget();
            frame = nullptr;
        }
    }

    void valueChanged(CControl* control) override
    {
        ParamID id = ParamID(control->getTag());
        ParamValue n = control->getValueNormalized();
        getController()->setParamNormalized(id, n);
        getController()->performEdit(id, n);
        showValue(id);
    }
    void controlBeginEdit(CControl* control) override { getController()->beginEdit(ParamID(control->getTag())); }
    void controlEndEdit(CControl* control) override { getController()->endEdit(ParamID(control->getTag())); }

private:
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
        if (auParam(id).kind == AuKind::List) {
            auto* menu = new COptionMenu(r, this, id);
            for (const std::string& name : auListNames(id, mix_)) menu->addEntry(name.c_str());
            menu->setFont(kNormalFont);
            menu->setFontColor(CColor(240, 234, 220));
            menu->setBackColor(CColor(52, 48, 44));
            menu->setFrameColor(CColor(120, 110, 96));
            add(menu, id);
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
    void sync(bool all = false)   // the host or automation may have moved a parameter
    {
        for (auto& [control, id] : controls_) {
            if (control->isEditing()) continue;
            float n = float(getController()->getParamNormalized(ParamID(id)));
            if (all || control->getValueNormalized() != n) {
                control->setValueNormalized(n);
                control->invalid();
            }
            showValue(id);
        }
    }

    bool mix_;
    std::vector<std::pair<CControl*, int>> controls_;
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

BEGIN_FACTORY_DEF(AU_VENDOR, AU_URL, "")

DEF_CLASS2(INLINE_UID_FROM_FUID(kRipresaProcessor), PClassInfo::kManyInstances, kVstAudioEffectClass,
           "Log MetaRaw Ripresa", Vst::kDistributable, PlugType::kFx, AU_VERSION, kVstVersionString,
           Processor::ripresa)
DEF_CLASS2(INLINE_UID_FROM_FUID(kRipresaController), PClassInfo::kManyInstances, kVstComponentControllerClass,
           "Log MetaRaw Ripresa Controller", 0, "", AU_VERSION, kVstVersionString, Controller::ripresa)
DEF_CLASS2(INLINE_UID_FROM_FUID(kMixProcessor), PClassInfo::kManyInstances, kVstAudioEffectClass,
           "Log MetaRaw Mix e supporto", Vst::kDistributable, PlugType::kFx, AU_VERSION, kVstVersionString,
           Processor::mixer)
DEF_CLASS2(INLINE_UID_FROM_FUID(kMixController), PClassInfo::kManyInstances, kVstComponentControllerClass,
           "Log MetaRaw Mix e supporto Controller", 0, "", AU_VERSION, kVstVersionString, Controller::mixer)

END_FACTORY

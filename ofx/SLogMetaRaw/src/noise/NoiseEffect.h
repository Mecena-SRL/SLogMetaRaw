// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <chrono>
#include <mutex>
#include <string>

#include "ofxsImageEffect.h"

#include "../common/UpdateBadge.h"
#include "../develop/ClipMeta.h"
#include "NoiseProfiles.h"

const int kNoiseSettingsVersion = 1;

const int kNoiseProfileCount = 3;
static const char* const kNoiseProfileNames[kNoiseProfileCount] = { "Automatico (camera e formato)",
                                                                     "Misurato (campione o fotogramma)", "Manuale" };
const int kNoiseFramesCount = 5;   // index -> radius: Automatico, Spento, ±1, ±2, ±3
static const char* const kNoiseFramesNames[kNoiseFramesCount] = { "Automatico", "Spento", "±1", "±2", "±3" };
const int kNoiseViewCount = 4;
static const char* const kNoiseViewNames[kNoiseViewCount] = { "Uscita", "Rumore rimosso", "Mappa del rumore",
                                                               "Movimento (fotogrammi usati)" };

class NoiseEffect : public OFX::ImageEffect
{
public:
    explicit NoiseEffect(OfxImageEffectHandle p_Handle);
    ~NoiseEffect() override;

    void render(const OFX::RenderArguments& p_Args) override;
    bool isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip, double& p_IdentityTime) override;
    void getFramesNeeded(const OFX::FramesNeededArguments& p_Args, OFX::FramesNeededSetter& p_Frames) override;
    void changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName) override;
    void changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName) override;
    void beginEdit() override;

private:
    void syncClip(MetaMode p_Mode);
    int inputGamma() const;                           // -1: unknown, the node stays neutral
    int radiusAt(double p_Time) const;
    NoiseModel modelAt(double p_Time, int p_FrameWidth) const;
    NoiseControls controlsAt(double p_Time) const;
    bool neutralAt(double p_Time) const;
    void measure(double p_Time);
    void useSample(double p_Time);
    bool sampleBox(double p_Time, int W, int H, int (&box)[4]) const;   // pixels, from the two corners
    void showSample();
    void lookForRecord();
    void refreshInfo();

    bool m_Ready = false;
    UpdateBadge m_Badge;
    OFX::Clip* m_DstClip = nullptr;
    OFX::Clip* m_SrcClip = nullptr;
    OFX::StringParam* m_Info = nullptr;
    OFX::StringParam* m_ProfileInfo = nullptr;
    OFX::StringParam* m_Measured = nullptr;
    OFX::StringParam* m_BoundPath = nullptr;
    OFX::StringParam* m_SampleText = nullptr;
    OFX::BooleanParam* m_SampleShow = nullptr;
    OFX::Double2DParam* m_SampleA = nullptr;
    OFX::Double2DParam* m_SampleB = nullptr;
    OFX::IntParam* m_SettingsVersion = nullptr;
    OFX::ChoiceParam* m_ProfileMode = nullptr;
    OFX::ChoiceParam* m_NodeInput = nullptr;
    OFX::ChoiceParam* m_Frames = nullptr;
    OFX::ChoiceParam* m_View = nullptr;
    OFX::DoubleParam* m_Level = nullptr;
    OFX::DoubleParam* m_TemporalStrength = nullptr;
    OFX::DoubleParam* m_Luma = nullptr;
    OFX::DoubleParam* m_Chroma = nullptr;
    OFX::DoubleParam* m_Fine = nullptr;
    OFX::DoubleParam* m_Medium = nullptr;
    OFX::DoubleParam* m_Coarse = nullptr;
    OFX::DoubleParam* m_Shadows = nullptr;
    OFX::DoubleParam* m_Preserve = nullptr;
    OFX::DoubleParam* m_Skin = nullptr;

    mutable std::mutex m_Mutex;                       // the clip's recording, read by every render
    NoiseSource m_Source;
    std::string m_ReadStatus;                         // why the camera data are missing; empty when read
    NoiseProfile m_MeasuredProfile;                   // parsed from the hidden parameter
    NoiseSampleFix m_Sample;                          // the sample box's correction, parsed from noiseSample
    mutable std::chrono::steady_clock::time_point m_LastCacheLook;   // a clip without data: the record is looked for again
};

// The session-wide switch: every Noise node passes its input through. Lives in memory only, so reopening Resolve
// turns the nodes back on.
bool noiseSkipped();

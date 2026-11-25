// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include <string>

#include "ofxsImageEffect.h"

#include "../common/UpdateBadge.h"
#include "CreativePanel.h"

// 4: Creative replaces the Detail (its controls are gone: a Detail node opens as a fresh Creative)
const int kCreativeSettingsVersion = 4;

// Where the input encoding came from, shown in the info line.
enum class InputOrigin { Declared, Host, Camera, Assumed, Unknown };

class CreativeEffect : public OFX::ImageEffect
{
public:
    explicit CreativeEffect(OfxImageEffectHandle p_Handle);
    ~CreativeEffect() override;

    void render(const OFX::RenderArguments& p_Args) override;
    bool isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip, double& p_IdentityTime) override;
    void getFramesNeeded(const OFX::FramesNeededArguments& p_Args, OFX::FramesNeededSetter& p_Frames) override;
    void changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName) override;
    void changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName) override;
    void beginEdit() override;

private:
    std::string sourcePath() const;
    void syncClip();   // camera encoding of the clip, from the cache only
    InputOrigin resolveInput(int& space, int& gamma) const;
    CreativeControls readControls(double p_Time) const;
    void refreshInfo();
    void updateProcess();   // the rows and groups of the chosen process
    void resetAll();
    void loadMedium();      // the chosen medium's preset into the response rows
    void fetchRecords(const CreativeControls& c, double t, const OFX::Image& layout, OFX::Image* frame,
                      std::unique_ptr<OFX::Image> keep[3], const float* out[3]);

    bool m_Ready = false;
    UpdateBadge m_Badge;
    OFX::Clip* m_DstClip = nullptr;
    OFX::Clip* m_SrcClip = nullptr;
    OFX::StringParam* m_Info = nullptr;
    OFX::ChoiceParam* m_NodeInput = nullptr;
    OFX::StringParam* m_BoundPath = nullptr;
    OFX::IntParam* m_CamSpace = nullptr;
    OFX::IntParam* m_CamGamma = nullptr;
    OFX::IntParam* m_SettingsVersion = nullptr;
    OFX::ChoiceParam* m_Process = nullptr;
    OFX::IntParam* m_MediumLoaded = nullptr;
    OFX::ChoiceParam* m_Choices[kCrChoiceCount] = {};
    OFX::DoubleParam* m_Sliders[kCrSliderCount] = {};
};

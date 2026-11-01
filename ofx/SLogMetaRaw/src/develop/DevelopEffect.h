// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

#include "ofxsImageEffect.h"

#include "../../gen/DevelopMath.h"
#include "ClipMeta.h"
#include "../common/UpdateBadge.h"
#include "ToneParams.h"

const int kSettingsVersion = 6;   // bump when the meaning of a saved parameter changes
const int kWBCustom = 7;
extern const double kPresetKelvin[];   // index = White Balance choice

class DevelopEffect : public OFX::ImageEffect
{
public:
    explicit DevelopEffect(OfxImageEffectHandle p_Handle);

    void render(const OFX::RenderArguments& p_Args) override;
    bool isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip, double& p_IdentityTime) override;
    void changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName) override;
    void changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName) override;
    void beginEdit() override;

private:
    std::string sourcePath() const;
    bool inputKnown() const { int s, g; return nodeEncoding(s, g); }   // the node knows what encoding enters it
    // Space and gamma entering the node: what is declared in Ingresso, else what Resolve or the camera says.
    // False when a part is neither declared nor known; declared tells whether Ingresso overrides anything.
    bool nodeEncoding(int& p_Space, int& p_Gamma, bool* p_Declared = nullptr) const;
    bool manualReference() const { return m_Unlock->getValue() && !m_MetaValid->getValue(); }
    // DevelopSync.cpp
    void migrateSettings();
    std::string clipIdentity(const std::string& p_Path) const;   // file name + size, to tell a relink from another clip
    void pickWhite();            // the eyedropper: Temperature and Tint that make the picked colour neutral
    void syncMetadata(MetaMode p_Mode);
    void updateEnabledness();
    void refreshNodeInfo();
    void composeStatus();
    void exportLut(double p_Time);   // DevelopLut.cpp, asks where to save
    void onUnlockChanged();
    void onReferenceChanged(const std::string& p_Name);
    // BuildParams.cpp
    bool buildParams(double p_Time, DevelopParams& p_Params, std::string* p_NodeInfo = nullptr,
                     std::string* p_LevelInfo = nullptr);

    bool m_Ready = false;   // false when a parameter is missing: the node then stays transparent
    UpdateBadge m_Badge;
    ToneParamSet m_Tones;
    // what the Camera line and Stato say, composed in one place (composeStatus)
    std::string m_CameraName, m_Detail;
    MetaOutcome m_Outcome = MetaOutcome::Missing;
    bool m_NotLog = false, m_FoundWhileManual = false;
    double m_RefEI = 800.0;      // the reference before the user's edit, to rescale Exposure
    std::string m_LegacyNote;    // 1.1 tone values that no longer apply
    OFX::Clip* m_DstClip = nullptr;
    OFX::Clip* m_SrcClip = nullptr;

    OFX::StringParam* m_Camera = nullptr;
    OFX::StringParam* m_Details[16] = {};  // headroom over kDetailCount (12); keep >= kDetailCount
    OFX::StringParam* m_Status = nullptr;
    OFX::ChoiceParam* m_DecodeUsing = nullptr;
    OFX::ChoiceParam* m_WBMode = nullptr;
    OFX::ChoiceParam* m_ColorSpace = nullptr;
    OFX::ChoiceParam* m_Gamma = nullptr;
    OFX::ChoiceParam* m_LutSize = nullptr;
    OFX::DoubleParam* m_Temp = nullptr;
    OFX::DoubleParam* m_Tint = nullptr;
    OFX::DoubleParam* m_EI = nullptr;
    OFX::BooleanParam* m_FcExposure = nullptr;
    OFX::BooleanParam* m_FcTemperature = nullptr;
    OFX::BooleanParam* m_FcTint = nullptr;
    OFX::BooleanParam* m_FcZones = nullptr;
    OFX::ChoiceParam* m_NodeInput = nullptr;   // before 2.5: space and gamma in one list, read once by migrateSettings
    OFX::ChoiceParam* m_InputSpace = nullptr;
    OFX::ChoiceParam* m_InputGamma = nullptr;
    OFX::RGBParam* m_WhitePick = nullptr;
    OFX::BooleanParam* m_TonesMore = nullptr;
    OFX::StringParam* m_NodeInfo = nullptr;
    OFX::ChoiceParam* m_DataLevel = nullptr;
    OFX::StringParam* m_LevelInfo = nullptr;
    // hidden, saved with the grade
    OFX::StringParam* m_BoundPath = nullptr;
    OFX::StringParam* m_BoundId = nullptr;
    OFX::DoubleParam* m_ShotTemp = nullptr;
    OFX::DoubleParam* m_ShotTint = nullptr;
    OFX::DoubleParam* m_ShotEI = nullptr;
    OFX::IntParam* m_CamSpace = nullptr;
    OFX::IntParam* m_CamGamma = nullptr;
    OFX::IntParam* m_LevelRequired = nullptr;
    OFX::IntParam* m_LevelHost = nullptr;
    OFX::BooleanParam* m_MetaValid = nullptr;
    OFX::BooleanParam* m_Unlock = nullptr;
    OFX::IntParam* m_RefSource = nullptr;   // 0 unknown, 1 camera metadata, 2 typed by the user
    OFX::IntParam* m_SettingsVersion = nullptr;
};

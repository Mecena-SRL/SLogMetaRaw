// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

#include "ofxsImageEffect.h"

#include "../../gen/DevelopMath.h"
#include "../common/UpdateBadge.h"
#include "../common/ZoneParams.h"

const int kDetailSettingsVersion = 1;
const int kDetailSliderCount = 25;

const int kDetailModeCount = 3;
static const char* const kDetailModeNames[kDetailModeCount] = { "Standard", "Cinematografico", "Zonale" };
static const char* const kDetailModeInfo[kDetailModeCount] = {
    "Toni locali, presenza e zone: Gamma dinamica, Presenza, Zone locali",
    "Rapporto di luce sul soggetto: scegli i punti luce e ombra, premi Misura, imposta il rapporto (1:2, 1:4, "
    "1:8). La luce resta ferma, l'ombra si sposta; Spegni sfondo per il chiaroscuro. Local Contrast, Highlights, "
    "Shadows e Zone locali qui non agiscono",
    "Sistema zonale (Ansel Adams): misura un'ombra e una luce, colloca l'ombra su una zona (esposizione) e la luce "
    "sulla sua (sviluppo N+/N-). Zona V = grigio 18%, zona 0 = nero e zona X = bianco della Destinazione. Vista: "
    "zone mostra dove cade ogni area" };
const int kDetailDestCount = 5;   // SM_DT_DEST_COUNT
static const char* const kDetailDestNames[kDetailDestCount] = {
    "Dal Bianco (manuale)", "Rec.709 · CST senza tone mapping", "Rec.709 · DRT (ACES, AgX, DaVinci)",
    "HDR PQ 1000 nit", "Scena · DaVinci WG/Intermediate (ODT dopo)" };

// Where the input encoding came from, shown in the info line.
enum class InputOrigin { Declared, Host, Camera, Assumed, Unknown };

class DetailEffect : public OFX::ImageEffect
{
public:
    explicit DetailEffect(OfxImageEffectHandle p_Handle);
    ~DetailEffect() override;

    void render(const OFX::RenderArguments& p_Args) override;
    bool isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip, double& p_IdentityTime) override;
    void changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName) override;
    void changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName) override;
    void beginEdit() override;

private:
    std::string sourcePath() const;
    void syncClip();                                  // camera encoding of the clip, from the cache only
    InputOrigin resolveInput(int& space, int& gamma) const;
    bool neutralAt(double p_Time) const;
    SMDetailControls readControls(double p_Time) const;
    void refreshInfo();
    void resetAll();
    void updateMode();                                // visibility of the Cinematografico group, mode description
    void measureRatio(double p_Time, bool p_KeepTarget);
    void refreshRatioInfo();
    void measureZones(double p_Time);
    void refreshZoneInfo();
    bool readPoints(double p_Time, const char* const names[2], double Ln[2], double Ly[2]);

    bool m_Ready = false;
    UpdateBadge m_Badge;
    ZoneParamSet m_Zones;
    OFX::Clip* m_DstClip = nullptr;
    OFX::Clip* m_SrcClip = nullptr;
    OFX::StringParam* m_Info = nullptr;
    OFX::ChoiceParam* m_NodeInput = nullptr;
    OFX::StringParam* m_BoundPath = nullptr;
    OFX::IntParam* m_CamSpace = nullptr;
    OFX::IntParam* m_CamGamma = nullptr;
    OFX::IntParam* m_SettingsVersion = nullptr;
    OFX::DoubleParam* m_Sliders[kDetailSliderCount] = {};   // kDetailSliders (DetailEffect.cpp), bound at construction
    OFX::BooleanParam* m_ViewGain = nullptr;
    OFX::BooleanParam* m_ViewBase = nullptr;
    OFX::ChoiceParam* m_DevMode = nullptr;
    OFX::ChoiceParam* m_Destination = nullptr;
    OFX::BooleanParam* m_SubjectFace = nullptr;
    OFX::BooleanParam* m_SubjectLandscape = nullptr;
    OFX::BooleanParam* m_ViewZonesColor = nullptr;
    OFX::BooleanParam* m_ViewZonesGrey = nullptr;
};

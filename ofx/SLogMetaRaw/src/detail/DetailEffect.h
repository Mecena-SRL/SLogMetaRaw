// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

#include "ofxsImageEffect.h"

#include "../../gen/DevelopMath.h"
#include "../common/UpdateBadge.h"
#include "../common/ZoneParams.h"

const int kDetailSettingsVersion = 2;   // 2: Zonale in phases; a new node starts at 1 · Sviluppo
const int kDetailSliderCount = 27;

const int kDetailModeCount = 3;
static const char* const kDetailModeNames[kDetailModeCount] = { "Standard", "Cinematografico", "Zonale" };
static const char* const kDetailModeInfo[kDetailModeCount] = {
    "Toni locali, presenza e zone: Gamma dinamica, Presenza, Zone locali",
    "Rapporto di luce sul soggetto: scegli i punti luce e ombra, premi Misura, imposta il rapporto (1:2, 1:4, "
    "1:8). La luce resta ferma, l'ombra si sposta; Spegni sfondo per il chiaroscuro. Local Contrast, Highlights, "
    "Shadows e Zone locali qui non agiscono",
    "Sistema zonale (Ansel Adams) in tre fasi, un nodo ciascuna: Sviluppo (curva globale: ombra sulla sua zona, luci "
    "con N+/N-), Stampa (per aree e bordi), Presenza (dettaglio). Zona V = grigio, 0 = nero, X = bianco della "
    "Destinazione" };
// Zonale in phases (SM_DT_PHASE_*): one node each, in series, or all in one. The guide line for each.
const int kZonalPhaseCount = 4;
static const char* const kZonalPhaseNames[kZonalPhaseCount] = { "Un nodo (tutte le fasi)", "1 · Sviluppo",
                                                                "2 · Stampa", "3 · Presenza" };
// Guida, at the bottom of the panel: per phase the goal, three steps and what comes next
const int kZonalGuideRows = 5;
static const char* const kZonalGuideNames[kZonalGuideRows] = { "zonalGuide", "zonalGuideStep1", "zonalGuideStep2",
                                                               "zonalGuideStep3", "zonalGuideNext" };
static const char* const kZonalGuideLabels[kZonalGuideRows] = { "Fase", "1", "2", "3", "Poi" };
static const char* const kZonalGuide[kZonalPhaseCount][kZonalGuideRows] = {
    { "Un nodo fa tutto: comodo, ma tre nodi in serie lavorano meglio",
      "Ombra e Luce sul viewer, poi Misura zone",
      "Ombra sulla zona = esposizione, Luci sulla zona = sviluppo N+/N-",
      "Rifinisci aree, bordi e presenza in questo stesso nodo",
      "Per tre nodi: copia il nodo, incollalo dopo, cambia Fase" },
    { "1/3 Sviluppo: esposizione e N+/N-, una curva su tutta l'immagine",
      "Ombra su un'ombra con dettaglio (III), Luce su una luce con dettaglio (VIII)",
      "Misura zone: l'ombra resta sulla sua zona, la luce va dove cade a N 0",
      "Ombra sulla zona = esposizione, Luci sulla zona = sviluppo attorno all'ombra",
      "Copia il nodo, incollalo dopo, Fase: 2 · Stampa" },
    { "2/3 Stampa: schiarire e bruciare per aree, come all'ingranditore",
      "Local Shadows schiarisce le ombre, Local Highlights brucia le luci",
      "Local Contrast: contrasto per aree, come la gradazione della carta",
      "Brucia i bordi e Ampiezza bordi scuriscono i margini del fotogramma",
      "Copia il nodo, incollalo dopo, Fase: 3 · Presenza" },
    { "3/3 Presenza: micro-contrasto, i toni restano sulle loro zone",
      "Texture: dettaglio fine, solo sopra la soglia del rumore",
      "Clarity: contrasto dei bordi medi, attorno a Clarity: centro",
      "Dehaze: toglie il velo, sotto Livello velo",
      "Ultimo nodo prima del CST o DRT d'uscita" } };
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
    void updateMode();                                // what each mode and Zonale phase shows, guide lines
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
    OFX::ChoiceParam* m_ZonalPhase = nullptr;
    OFX::BooleanParam* m_SubjectFace = nullptr;
    OFX::BooleanParam* m_SubjectLandscape = nullptr;
    OFX::BooleanParam* m_ViewZonesColor = nullptr;
    OFX::BooleanParam* m_ViewZonesGrey = nullptr;
};

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

#include "ofxsImageEffect.h"

#include "../../gen/DevelopMath.h"
#include "../common/UpdateBadge.h"

const int kDetailSettingsVersion = 3;   // 3: Zonale is mode 0 (Standard and Cinematografico are gone)
const int kDetailSliderCount = 23;

const int kDetailModeCount = 2;
static const char* const kDetailModeNames[kDetailModeCount] = { "Zonale (Ansel Adams)", "Split grade (stampa)" };
static const char* const kDetailModeInfo[kDetailModeCount] = {
    "Sistema zonale (Ansel Adams) in tre fasi, un nodo ciascuna: Sviluppo (curva globale: ombra sulla sua zona, luci "
    "con N+/N-), Stampa (per aree e bordi), Presenza (dettaglio). Zona V = grigio, 0 = nero, X = bianco della "
    "Destinazione",
    "Stampa a due esposizioni (split grade) in tre fasi: Esposizioni (luci morbide e ombre dure, il grado esce dal "
    "rapporto), Locale (brucia e scherma per aree, bordi), Presenza (dettaglio). Non serve il sistema zonale" };
// The phases (SM_DT_PHASE_*): one node each, in series, or all in one. Zonale and Split grade name them their own way.
const int kPhaseCount = 4;
static const char* const kZonalPhaseNames[kPhaseCount] = { "Un nodo (tutte le fasi)", "1 · Sviluppo", "2 · Stampa",
                                                           "3 · Presenza" };
static const char* const kSplitPhaseNames[kPhaseCount] = { "Un nodo (tutte le fasi)", "1 · Esposizioni", "2 · Locale",
                                                           "3 · Presenza" };
// Guida, at the bottom of the panel: per mode and phase the goal, three steps and what comes next
const int kGuideRows = 5;
static const char* const kGuideNames[kGuideRows] = { "zonalGuide", "zonalGuideStep1", "zonalGuideStep2",
                                                     "zonalGuideStep3", "zonalGuideNext" };
static const char* const kGuideLabels[kGuideRows] = { "Fase", "1", "2", "3", "Poi" };
static const char* const kGuide[kDetailModeCount][kPhaseCount][kGuideRows] = { {
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
    { "3/3 Presenza: micro-contrasto, solo nelle zone II–VIII che hanno dettaglio",
      "Texture: dettaglio fine, solo sopra la soglia del rumore",
      "Clarity: contrasto dei bordi medi, attorno a Clarity: centro",
      "Dehaze: toglie il velo, sotto Livello velo",
      "Ultimo nodo prima del CST o DRT d'uscita" } }, {
    { "Stampa a due esposizioni in un nodo: comodo, ma tre nodi in serie lavorano meglio",
      "Ombra e Luce sul viewer, poi Misura",
      "Luci morbide e Ombre dure (più stop = più scuro); il Grado dice il contrasto ottenuto",
      "Brucia, scherma e presenza rifiniscono in questo stesso nodo",
      "Per tre nodi: copia il nodo, incollalo dopo, cambia Fase" },
    { "1/3 Esposizioni: le due esposizioni della stampa, una curva su tutta l'immagine",
      "Ombra su un'ombra con dettaglio, Luce su una luce con dettaglio, poi Misura",
      "Luci morbide: il tempo dell'esposizione morbida, sposta le luci (come il filtro 00)",
      "Ombre dure: il tempo dell'esposizione dura, sposta le ombre (come il filtro 5). Provino: scegli la cella e Usa la cella",
      "Copia il nodo, incollalo dopo, Fase: 2 · Locale" },
    { "2/3 Locale: bruciare e schermare per aree, come all'ingranditore",
      "Local Highlights brucia le luci, a dettaglio intatto",
      "Local Shadows schiarisce le ombre, Local Contrast dà contrasto per aree",
      "Brucia i bordi e Ampiezza bordi scuriscono i margini del fotogramma",
      "Copia il nodo, incollalo dopo, Fase: 3 · Presenza" },
    { "3/3 Presenza: micro-contrasto, solo nelle zone II–VIII che hanno dettaglio",
      "Texture: dettaglio fine, solo sopra la soglia del rumore",
      "Clarity: contrasto dei bordi medi, attorno a Clarity: centro",
      "Dehaze: toglie il velo, sotto Livello velo",
      "Ultimo nodo prima del CST o DRT d'uscita" } } };
const int kSplitEngineCount = 2;
static const char* const kSplitEngineNames[kSplitEngineCount] = { "Curva (perno scorrevole)", "Due strati (per luce)" };
const int kProvinoKindCount = 3;
static const char* const kProvinoKindNames[kProvinoKindCount] = { "Strisce (morbida e dura)", "Matrice morbida × dura",
                                                                  "Unioni a confronto" };
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
    void measureZones(double p_Time);
    void refreshZoneInfo();
    bool readPoints(double p_Time, const char* const names[2], double Ln[2], double Ly[2]);

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
    OFX::DoubleParam* m_Sliders[kDetailSliderCount] = {};   // kDetailSliders (DetailEffect.cpp), bound at construction
    OFX::BooleanParam* m_ViewGain = nullptr;
    OFX::BooleanParam* m_ViewBase = nullptr;
    OFX::ChoiceParam* m_DevMode = nullptr;
    OFX::ChoiceParam* m_Destination = nullptr;
    OFX::ChoiceParam* m_ZonalPhase = nullptr;
    OFX::ChoiceParam* m_SplitPhase = nullptr;
    OFX::ChoiceParam* m_SplitEngine = nullptr;
    OFX::ChoiceParam* m_ProvinoKind = nullptr;
    OFX::BooleanParam* m_ViewProvino = nullptr;
    void applyProvino(double p_Time);                 // the exposures of the cell the point is on
    OFX::BooleanParam* m_ViewZonesColor = nullptr;
    OFX::BooleanParam* m_ViewZonesGrey = nullptr;
};

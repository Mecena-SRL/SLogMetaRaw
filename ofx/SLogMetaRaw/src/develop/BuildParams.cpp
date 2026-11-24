// SPDX-License-Identifier: GPL-3.0-or-later
// Panel values -> DevelopParams, the only input of the per-pixel maths.
#include "DevelopEffect.h"

#include <cstdio>

#include "ofxColour.h"
#include "../common/ColourSpaces.h"
#include "ToneParams.h"

bool DevelopEffect::buildParams(double p_Time, DevelopParams& p, std::string* p_NodeInfo, std::string* p_LevelInfo,
                                std::string* p_NoiseInfo)
{
    if (p_NoiseInfo) *p_NoiseInfo = "spenta";
    if (!m_Ready) { p.bypass = 1; return false; }
    p = DevelopParams();

    // colour space of the image entering the node
    int nodeSpace = 0, nodeGamma = 0;
    bool declared = false;
    nodeEncoding(nodeSpace, nodeGamma, &declared);
    const std::string hostCs = m_SrcClip->getPropertySet().propGetString(kOfxImageClipPropColourspace, false);
    if (p_NodeInfo) {
        *p_NodeInfo = std::string(kSpaceNames[nodeSpace]) + " / " + kGammaNames[nodeGamma]
                    + (hostCs.empty() ? "" : " (Resolve: " + hostCs + ")");
    }
    p.nodeSpace = nodeSpace;
    p.nodeGamma = nodeGamma;

    // Data level runs before the bypass: a wrong scale is a decode error of the file, worth fixing
    // even with the develop off, and on clips without metadata once the input is declared.
    int levelChoice = 0;
    m_DataLevel->getValue(levelChoice);
    const int camSpaceCode = validCode(m_CamSpace->getValue(), kSpaceCount);
    const int camGammaCode = validCode(m_CamGamma->getValue(), kGammaCount);
    const int domSpace = camSpaceCode >= 0 ? camSpaceCode : (declared ? nodeSpace : -1);
    const int domGamma = camGammaCode >= 0 ? camGammaCode : (declared ? nodeGamma : -1);
    int want = validCode(m_LevelRequired->getValue(), 2);
    if (want < 0 && domGamma >= 7 && domGamma <= 9) want = 1;   // S-Log curves use unscaled code values
    int have = validCode(m_LevelHost->getValue(), 2);
    if (levelChoice == 1) have = 1;
    else if (levelChoice == 2) have = 0;
    std::string levelWhy;
    if (levelChoice == 3) {
        levelWhy = "disattivata";
    } else if (want < 0 || domGamma < 0 || domSpace < 0) {
        levelWhy = "curva della clip non nota: nessuna correzione";
    } else if (have < 0) {
        levelWhy = std::string("Resolve e su Auto, il valore effettivo non e leggibile · questa curva vuole ")
                 + kLevelNames[want] + " · imposta Data Level in Attributi clip (lo fa anche lo script) "
                   "oppure dichiaralo qui sopra";
    } else if (have != want) {
        sm_set_level_fix(&p, domSpace, domGamma, have);
        char buf[320];
        snprintf(buf, sizeof(buf), "in ingresso %s, la curva vuole %s · correzione x%.6f %+.6f su %s / %s",
                 kLevelNames[have], kLevelNames[want], p.levelGain, p.levelOffset,
                 kSpaceNames[p.levelSpace], kGammaNames[p.levelGamma]);
        levelWhy = buf;
    } else {
        levelWhy = std::string("in ingresso ") + kLevelNames[have] + ": corretto, nessuna correzione";
    }
    if (p_LevelInfo) *p_LevelInfo = levelWhy;

    int decode = 1;
    m_DecodeUsing->getValueAtTime(p_Time, decode);
    // typed references are only trusted when the node knows its input: S-Log3 read as another
    // curve would turn Exposure and White Balance into non-linear moves
    const bool active = m_MetaValid->getValue() || (manualReference() && inputKnown());
    if (decode == 0 || !active) {
        p.bypass = 1;
        return false;
    }

    int cs = 0, gm = 0;
    m_ColorSpace->getValueAtTime(p_Time, cs);
    m_Gamma->getValueAtTime(p_Time, gm);
    int drt = 0;
    m_Rendering->getValueAtTime(p_Time, drt);
    p.drt = drt >= 1 && drt <= 5 ? drt : 0;
    int gc = 0, gmap = 0;
    m_GamutCompress->getValue(gc);
    m_GamutMapping->getValue(gmap);
    if (gc == 1) sm_set_gamut_compress(&p, SM_RGC_REFERENCE);
    else if (gc == 2) {
        double v[7];
        for (int i = 0; i < 7; ++i) v[i] = m_GamutParams[i]->getValueAtTime(p_Time);
        sm_set_gamut_compress(&p, v);
    }
    p.gmOn = gmap == 1 ? 1 : 0;
    p.hlVib = p.drt ? (float)sm_clamp(m_HlVib->getValueAtTime(p_Time) / 100.0, 0.0, 1.0) : 0.0f;
    // 0 follows the node; a choice outside the table (a grade from a newer build) does too.
    // A display rendering needs a display: on Timeline it writes Rec.709 / Gamma 2.4.
    p.outSpace = cs >= 1 && cs <= kSpaceCount ? cs - 1 : p.drt ? 1 : nodeSpace;
    p.outGamma = gm >= 1 && gm <= kGammaCount ? gm - 1 : p.drt ? 3 : nodeGamma;
    if (p.drt) sm_display_encoding(&p.outGamma);
    const bool sceneOut = p.drt && !(p.outSpace >= 1 && p.outSpace <= 5 && p.outGamma >= 2 && p.outGamma <= 6);
    p.convert = (p.drt || p.outSpace != nodeSpace || p.outGamma != nodeGamma) ? 1 : 0;
    // the Detail node reads the timeline's encoding, not this node's: after a conversion it would misread
    if (p_NodeInfo && p.convert)
        *p_NodeInfo += std::string(" · Uscita: ") + kSpaceNames[p.outSpace] + " / " + kGammaNames[p.outGamma]
                     + (sceneOut ? " (immagine gia resa: per vederla, un CST verso lo schermo senza tone mapping)" : "")
                     + " (Log MetaRaw Detail va messo prima di questa conversione)"
                     + (p.drt && !hostCs.empty() && hostCs.compare(0, 3, "g24") && hostCs.compare(0, 4, "rec1")
                            && hostCs.compare(0, 4, "srgb") && hostCs.compare(0, 3, "g22")
                        ? " · ATTENZIONE: Resolve gestisce il colore (" + hostCs + ") e dopo il nodo applica la sua "
                          "trasformazione d'uscita: con un rendering l'immagine viene trasformata due volte (scura). "
                          "Usa una timeline DaVinci YRGB non gestita, oppure Rendering display = Nessuno e il tone "
                          "mapping di Resolve"
                        : "");

    const double shotEI = m_ShotEI->getValue();
    const double ei = m_EI->getValueAtTime(p_Time);
    p.expo = (shotEI > 0.0 && ei > 0.0) ? (float)(ei / shotEI) : 1.0f;
    sm_set_white_balance(&p, m_ShotTemp->getValue(), m_ShotTint->getValue(), m_Temp->getValueAtTime(p_Time),
                         m_Tint->getValueAtTime(p_Time));

    int fc = 0;
    if (m_FcExposure->getValue()) fc = 1;
    else if (m_FcTemperature->getValue()) fc = 2;
    else if (m_FcTint->getValue()) fc = 3;
    else if (m_FcZones->getValue()) fc = 4;
    p.fcMode = fc;
    if ((fc == 2 || fc == 3) && !sm_fc_calibrate(&p, m_Temp->getValueAtTime(p_Time), m_Tint->getValueAtTime(p_Time)))
        p.fcMode = 0;   // degenerate basis: show the picture rather than a lie
    if (p_NodeInfo && p.fcMode != 0)
        *p_NodeInfo = std::string("FALSE COLOR ATTIVO (")
                    + (p.fcMode == 1 ? "esposizione" : p.fcMode == 2 ? "temperatura" : p.fcMode == 3 ? "tint" : "zone")
                    + ") · " + *p_NodeInfo;

    const SMToneControls tones = m_Tones.read(p_Time);
    sm_set_tone(&p, &tones, p.outSpace, p.convert, p.expo);

    int noise = 0;
    m_NoiseMask->getValue(noise);
    // the camera row read with the metadata, unless the node decodes another curve (Ingresso, no metadata)
    const NoiseCamera cam = m_NoiseCamera.gamma == nodeGamma ? m_NoiseCamera : noiseCamera(nodeGamma, "", "");
    setNoiseMask(p, cam, noise, m_NoiseView->getValue(), m_NoiseStart->getValueAtTime(p_Time),
                 m_NoiseFade->getValueAtTime(p_Time), shotEI, ei, p_NoiseInfo);
    if (p_NodeInfo && p.nmOn == 1)
        *p_NodeInfo = "NODO MASCHERA RUMORE: immagine invariata, solo alpha · " + *p_NodeInfo;
    if (p_NodeInfo && p.nmOn == 2) *p_NodeInfo = "MASCHERA RUMORE IN VISTA · " + *p_NodeInfo;
    return true;
}

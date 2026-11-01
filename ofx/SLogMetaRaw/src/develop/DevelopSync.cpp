// SPDX-License-Identifier: GPL-3.0-or-later
// Settings migration, the link between the node and the clip's camera metadata, and the
// references typed by hand when there is none.
#include "DevelopEffect.h"

#include <cstdio>

#include "ofxColour.h"
#include "ToneParams.h"
#include "../common/ColourSpaces.h"
#include "../common/Files.h"

static void setText(OFX::StringParam* p, const std::string& v)
{
    std::string cur;
    p->getValue(cur);
    if (cur != v) p->setValue(v);   // do not touch the grade when nothing changed
}

template <typename P, typename V>
static void setIfDifferent(P* p, V v)
{
    if (p->getValue() != v) p->setValue(v);
}

// settingsVersion says which meaning the saved values had; history in RELEASE_NOTES.md.
// 0 is a new node (or a value the host did not save): treated as current.
void DevelopEffect::migrateSettings()
{
    // refSource is newer than the settings versions: a node that already read its clip is camera-referenced
    if (m_RefSource->getValue() == 0 && m_MetaValid->getValue()) m_RefSource->setValue(1);
    m_LegacyNote = legacyTonesNote(*this);
    const int saved = m_SettingsVersion->getValue();
    if (saved == kSettingsVersion) return;
    if (saved > kSettingsVersion) {   // saved by a newer plugin: keep the values as they are
        fprintf(stderr, "S-Log MetaRaw: nodo salvato con impostazioni v%d, questo plugin arriva alla v%d\n",
                saved, kSettingsVersion);
        return;
    }
    if (saved == 1) m_DataLevel->setValue(3);   // the data level correction starts off on a 1.0 grade
    // 2.5 split "Ingresso nodo" in two: the pair it stood for becomes the declared space and gamma
    int list = 0, inSpace = 0, inGamma = 0;
    m_NodeInput->getValue(list);
    m_InputSpace->getValue(inSpace);
    m_InputGamma->getValue(inGamma);
    if (list > 0 && list < kNodeInputCount && inSpace == 0 && inGamma == 0) {
        m_InputSpace->setValue(kNodeInputPairs[list][0] + 1);
        m_InputGamma->setValue(kNodeInputPairs[list][1] + 1);
    }
    m_SettingsVersion->setValue(kSettingsVersion);
}

bool DevelopEffect::nodeEncoding(int& p_Space, int& p_Gamma, bool* p_Declared) const
{
    int inSpace = 0, inGamma = 0;
    m_InputSpace->getValue(inSpace);
    m_InputGamma->getValue(inGamma);
    const int space = inSpace >= 1 && inSpace <= kSpaceCount ? inSpace - 1 : -1;   // 0, or a choice from a newer build: Automatico
    const int gm = inGamma >= 1 && inGamma <= kGammaCount ? inGamma - 1 : -1;
    if (p_Declared) *p_Declared = space >= 0 || gm >= 0;
    int hostSpace = 0, hostGamma = 0;
    bool known = mapColourspace(m_SrcClip->getPropertySet().propGetString(kOfxImageClipPropColourspace, false),
                                hostSpace, hostGamma);
    if (!known) {   // no colour management info: nodes receive the camera encoding (DaVinci YRGB)
        const int cs = validCode(m_CamSpace->getValue(), kSpaceCount);
        const int cg = validCode(m_CamGamma->getValue(), kGammaCount);
        if (cs >= 0 && cg >= 0) { hostSpace = cs; hostGamma = cg; known = true; }
    }
    p_Space = space >= 0 ? space : hostSpace;
    p_Gamma = gm >= 0 ? gm : hostGamma;
    return (space >= 0 || known) && (gm >= 0 || known);
}

// The file name and its size say it is the same clip after a relink or on another computer; a path alone does not.
std::string DevelopEffect::clipIdentity(const std::string& p_Path) const
{
    const long long size = fileSize(p_Path);
    return size < 0 ? "" : p_Path.substr(p_Path.find_last_of("/\\") + 1) + "|" + std::to_string(size);
}

// The eyedropper hands over a colour in the encoding that enters the node. The illuminant that makes it neutral
// is what White Balance describes, so Temperature and Tint follow from the same maths as the render.
void DevelopEffect::pickWhite()
{
    DevelopParams p;
    buildParams(0.0, p);
    double r = 0.5, g = 0.5, b = 0.5;
    m_WhitePick->getValue(r, g, b);
    SMf3 lin = smf3(sm_decode((float)r, p.nodeGamma), sm_decode((float)g, p.nodeGamma), sm_decode((float)b, p.nodeGamma));
    lin = sm_fix_levels(lin, p);
    const SMf3 xyz = sm_to_xyz(lin, p.nodeSpace);
    const double sample[3] = { xyz.x, xyz.y, xyz.z };
    double k = 0.0, tint = 0.0;
    if (!sm_wb_from_xyz(m_ShotTemp->getValue(), m_ShotTint->getValue(), sample, &k, &tint)) {
        setText(m_Status, "Punto bianco: il colore scelto non e un grigio o un bianco (troppo scuro o saturo)");
        return;
    }
    m_WBMode->setValue(kWBCustom);
    m_Temp->setValue(k);
    m_Tint->setValue(tint);
    refreshNodeInfo();
}

// The sliders take the camera values only for a new node, a node moved to another clip, or
// "Rileggi metadata": the colourist's adjustments survive a project reload.
void DevelopEffect::syncMetadata(MetaMode p_Mode)
{
    const std::string path = sourcePath();
    m_FoundWhileManual = false;
    m_NotLog = false;
    if (path.empty()) {
        m_Outcome = MetaOutcome::Missing;
        m_CameraName.clear();
        m_Detail = "Resolve non ha fornito il percorso della clip (compound clip?)";
        updateEnabledness();
        refreshNodeInfo();
        return;
    }
    std::string bound, boundId;
    m_BoundPath->getValue(bound);
    m_BoundId->getValue(boundId);
    const std::string id = clipIdentity(path);
    // a relinked clip (same file name and size, other folder or computer) keeps the grade: no reset to the camera
    const bool sameClip = (bound == path) || (!id.empty() && id == boundId);

    ClipMeta meta;
    m_Outcome = acquireMeta(path, p_Mode, meta, m_Detail);
    if (m_Outcome != MetaOutcome::Found) {
        setIfDifferent(m_MetaValid, false);
        m_CameraName.clear();
        for (int i = 0; i < kDetailCount; ++i) setText(m_Details[i], "—");
        setText(m_Details[kDetailCount - 1], path.substr(path.find_last_of("/\\") + 1));
        if (!sameClip) {   // decoding facts of another clip must not follow a copied node
            setIfDifferent(m_CamSpace, -1);
            setIfDifferent(m_CamGamma, -1);
            setIfDifferent(m_LevelRequired, -1);
            setIfDifferent(m_LevelHost, -1);
        }
        if (m_Unlock->getValue()) setIfDifferent(m_RefSource, 2);
        updateEnabledness();
        refreshNodeInfo();
        return;
    }

    m_CameraName = meta.fields["camera_name"];
    for (int i = 0; i < kDetailCount; ++i) {
        auto it = meta.fields.find(kDetails[i].key);
        setText(m_Details[i], it != meta.fields.end() && !it->second.empty() ? it->second : "—");
    }
    const std::string note = meta.resolveNote.empty() ? "" : " · " + meta.resolveNote;
    if (m_RefSource->getValue() == 2 && sameClip && p_Mode != MetaMode::Reload && meta.supported) {
        // references typed by hand stay until the colourist asks for the camera ones
        m_FoundWhileManual = true;
        m_Detail = "Metadata ora disponibili (EI " + std::to_string((int)meta.shotEI) + " · "
                 + std::to_string((int)meta.shotTemp) + " K): premi Rileggi metadata per usarli";
        updateEnabledness();
        refreshNodeInfo();
        return;
    }
    const bool firstRead = m_RefSource->getValue() == 0;
    // A node pasted or moved to another clip arrives with its Exposure and balance set against the old camera
    const double oldShotEI = m_ShotEI->getValue(), oldEI = m_EI->getValue();
    int oldWB = 0;
    m_WBMode->getValue(oldWB);
    setIfDifferent(m_ShotTemp, meta.shotTemp);
    setIfDifferent(m_ShotTint, meta.shotTint);
    setIfDifferent(m_ShotEI, meta.shotEI);
    setIfDifferent(m_CamSpace, meta.camSpace);
    setIfDifferent(m_CamGamma, meta.camGamma);
    setIfDifferent(m_LevelRequired, meta.levelRequired);
    setIfDifferent(m_LevelHost, meta.levelHost);
    setIfDifferent(m_RefSource, 1);
    m_RefEI = meta.shotEI;
    if (!meta.supported) {
        m_NotLog = true;
        setIfDifferent(m_MetaValid, false);
        m_Detail = "Profilo " + (meta.colorSpace.empty() ? std::string("non riconosciuto") : meta.colorSpace)
                 + " non logaritmico: il nodo resta neutro" + note;
    } else {
        setIfDifferent(m_MetaValid, true);
        m_Detail += (meta.wbEstimated ? " · Kelvin stimato (la camera non lo registra)" : "") + note;
        if (!sameClip || p_Mode == MetaMode::Reload || firstRead) {
            if (!sameClip && !bound.empty() && !firstRead && p_Mode != MetaMode::Reload && oldShotEI > 0.0 && oldEI > 0.0) {
                // copy and paste: the stops above or below the camera EI and a Custom balance travel with the node,
                // As shot follows the new camera
                if (oldWB == 0) {
                    m_Temp->setValue(meta.shotTemp);
                    m_Tint->setValue(meta.shotTint);
                }
                rescaleExposure(meta.shotEI / oldShotEI);
            } else {
                m_WBMode->setValue(0);
                m_Temp->setValue(meta.shotTemp);
                m_Tint->setValue(meta.shotTint);
                m_EI->setValue(meta.shotEI);
            }
        }
    }
    if (bound != path) setText(m_BoundPath, path);   // only after a successful read
    setText(m_BoundId, id);
    updateEnabledness();
    refreshNodeInfo();
}

// The Camera line is the one status always in sight; Stato (Spazio colore › Ingresso) carries the detail.
void DevelopEffect::composeStatus()
{
    const bool unlock = m_Unlock->getValue(), manual = manualReference();
    std::string camera;
    if (manual && !inputKnown()) camera = "Ingresso non noto · Spazio colore › Ingresso";
    else if (m_FoundWhileManual) camera = m_CameraName + " · Metadata trovati: Rileggi li adotta";
    else if (m_Outcome == MetaOutcome::Pending) camera = "Lettura metadata in corso…";
    else if (m_NotLog) camera = m_CameraName + " · profilo non log: nodo neutro";
    else if (m_Outcome != MetaOutcome::Found && manual) camera = "Metadata non trovati · sbloccato";
    else if (m_Outcome != MetaOutcome::Found) camera = unlock ? "Metadata non trovati"
                                                              : "Metadata non trovati · Spazio colore › Senza metadata › Sblocca controlli";
    else camera = m_CameraName;
    setText(m_Camera, camera);

    std::string detail = m_Detail;
    if (!m_LegacyNote.empty()) detail = m_LegacyNote + (detail.empty() ? "" : " · " + detail);
    if (manual) {
        char buf[160];
        snprintf(buf, sizeof(buf), "Controlli sbloccati senza metadata · riferimento EI %g · %g K · tint %g",
                 m_ShotEI->getValue(), m_ShotTemp->getValue(), m_ShotTint->getValue());
        detail = std::string(buf) + (detail.empty() ? "" : " · " + detail);
    }
    setText(m_Status, detail);
}

void DevelopEffect::refreshNodeInfo()
{
    DevelopParams p;
    std::string info, level;
    buildParams(0.0, p, &info, &level);
    if (!info.empty()) setText(m_NodeInfo, info);
    if (!level.empty()) setText(m_LevelInfo, level);
    composeStatus();
}

void DevelopEffect::updateEnabledness()
{
    int decode = 1;
    m_DecodeUsing->getValue(decode);
    const bool manual = manualReference();
    const bool on = (decode == 1) && (m_MetaValid->getValue() || (manual && inputKnown()));
    OFX::ValueParam* const controls[] = { m_WBMode, m_ColorSpace, m_Gamma, m_Temp, m_Tint, m_EI, m_WhitePick };
    for (OFX::ValueParam* c : controls) c->setEnabled(on);
    for (const std::string& name : toneParamNames()) getParam(name)->setEnabled(on);
    refreshToneVisibility(*this, m_Tones, m_TonesMore->getValue());
    OFX::DoubleParam* const refs[] = { m_ShotEI, m_ShotTemp, m_ShotTint };
    for (OFX::DoubleParam* r : refs) {
        r->setIsSecret(!m_Unlock->getValue());
        r->setEnabled(manual);
    }
}

// Ticked with no metadata: the node starts neutral on the typed references.
void DevelopEffect::onUnlockChanged()
{
    if (m_Unlock->getValue() && !m_MetaValid->getValue()) {
        m_RefSource->setValue(2);
        m_WBMode->setValue(0);
        m_EI->setValue(m_ShotEI->getValue());
        m_Temp->setValue(m_ShotTemp->getValue());
        m_Tint->setValue(m_ShotTint->getValue());
    }
    m_RefEI = m_ShotEI->getValue();
    updateEnabledness();
    refreshNodeInfo();
}

// Exposure times ratio, keys included, inside the slider's range.
void DevelopEffect::rescaleExposure(double p_Ratio)
{
    auto clampEI = [](double v) { return v < 25.0 ? 25.0 : (v > 409600.0 ? 409600.0 : v); };
    const unsigned keys = m_EI->getNumKeys();
    if (keys == 0) {
        m_EI->setValue(clampEI(m_EI->getValue() * p_Ratio));
        return;
    }
    for (unsigned i = 0; i < keys; ++i) {
        const double t = m_EI->getKeyTime((int)i);
        m_EI->setValueAtTime(t, clampEI(m_EI->getValueAtTime(t) * p_Ratio));
    }
}

// A reference changes the numbers, not the picture: Exposure follows the EI, keys included.
void DevelopEffect::onReferenceChanged(const std::string& p_Name)
{
    if (p_Name == "shotEI") {
        const double now = m_ShotEI->getValue();
        const double ratio = m_RefEI > 0.0 ? now / m_RefEI : 1.0;
        m_RefEI = now;
        if (!manualReference() || ratio == 1.0) return;
        rescaleExposure(ratio);
    } else if (manualReference()) {
        int wb = 0;
        m_WBMode->getValue(wb);
        if (wb == 0) {   // As shot follows its reference; a Custom balance keeps its numbers
            m_Temp->setValue(m_ShotTemp->getValue());
            m_Tint->setValue(m_ShotTint->getValue());
        }
    }
    refreshNodeInfo();
}

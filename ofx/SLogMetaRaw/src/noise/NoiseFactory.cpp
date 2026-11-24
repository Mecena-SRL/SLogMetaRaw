// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoiseFactory.h"

#include "../common/ColourSpaces.h"
#include "../common/ParamDefs.h"
#include "../common/Update.h"
#include "../common/UpdateBadge.h"
#include "NoiseEffect.h"

using namespace OFX;

NoiseFactory::NoiseFactory() : PluginFactoryHelper<NoiseFactory>(kNoiseIdentifier, 1, 0) {}

void NoiseFactory::load() { Update::pluginLoaded(); }
void NoiseFactory::unload() { Update::pluginUnloaded(); }

void NoiseFactory::describe(ImageEffectDescriptor& p_Desc)
{
    describeEffectCommon(p_Desc, "Log MetaRaw Noise",
                         "Riduzione rumore video con il profilo della camera e del formato (ISO, codec, bitrate): "
                         "temporale con compensazione del movimento e spaziale per bande, luminanza e colore separati. "
                         "Va prima di Log MetaRaw, sull'immagine log. Ivan Mazzone + Claude (@Ivan_94m).",
                         "com.slogmetaraw.SLogMetaRawNoise.png", kSpatial);
    // the neighbour frames are read at other times; Metal on macOS, the CPU elsewhere (CUDA: next step)
    p_Desc.setTemporalClipAccess(true);
#ifdef __APPLE__
    p_Desc.setSupportsMetalRender(true);
#endif
}

void NoiseFactory::describeInContext(ImageEffectDescriptor& d, ContextEnum)
{
    resetParamNames();
    ClipDescriptor* src = d.defineClip(kOfxImageEffectSimpleSourceClipName);
    src->addSupportedComponent(ePixelComponentRGBA);
    src->setTemporalClipAccess(true);
    src->setSupportsTiles(false);
    src->setIsMask(false);
    ClipDescriptor* dst = d.defineClip(kOfxImageEffectOutputClipName);
    dst->addSupportedComponent(ePixelComponentRGBA);
    dst->setSupportsTiles(false);

    PageParamDescriptor* page = d.definePageParam("Controls");
    defineVersionButton(d, page);
    defineInfo(d, page, nullptr, "noiseInfo", "Noise", "Temporale + spaziale · prima di Log MetaRaw");
    defineButton(d, page, "readCamera", "Rileggi metadata",
                 "Come in Log MetaRaw: rilegge dal file della clip camera, ISO, codec, bitrate e formato, li scrive "
                 "nel Media Pool di Resolve, riporta Ingresso (IDT) ad Automatico e riattiva il nodo con il profilo "
                 "della camera");
    defineButton(d, page, "skipAll", "Salta Noise ovunque",
                 "Spegne tutti i nodi Noise del progetto per lavorare veloce; un secondo clic li riaccende. Si "
                 "riaccendono da soli alla riapertura di Resolve. Prima dell'export controlla che la riga Noise non "
                 "dica SALTATO; se un nodo resta spento dopo averli riaccesi, svuota la render cache");

    // In view: profile, sample box, the two strengths and the view. Everything else in Avanzate, closed.
    GroupParamDescriptor* prof = defineGroup(d, page, "profileGroup", "Profilo del rumore", true);
    defineChoice(d, page, "profileMode", "Profilo", kNoiseProfileNames, kNoiseProfileCount, 0,
                 "Automatico: dalla camera, dall'ISO e dal formato della clip, letti da soli. Misurato: il profilo "
                 "della camera corretto sulla zona campione (Usa il campione), o misurato sull'intero fotogramma "
                 "(Misura dal fotogramma). Manuale: un profilo generico, regolato solo da Livello rumore", prof);
    defineInfo(d, page, prof, "profileInfo", "Da", "—");
    defineCheck(d, page, "sampleShow", "Mostra campione", false,
                "Disegna la zona campione (riquadro giallo e nero) e i suoi due angoli nel viewer: trascinali su una "
                "zona piatta, senza dettaglio (cielo, muro, fuori fuoco)", prof);
    definePoint(d, page, prof, "sampleA", "Campione: angolo", 0.42, 0.58,
                "Un angolo della zona campione (frazione del fotogramma, 0,0 in basso a sinistra)", true);
    definePoint(d, page, prof, "sampleB", "Campione: angolo opposto", 0.58, 0.42,
                "L'angolo opposto della zona campione", true);
    defineButton(d, page, "sampleUse", "Usa il campione",
                 "Misura il rumore nella zona campione e corregge su quello il profilo della camera, banda per banda; "
                 "le altre luminosità seguono la curva della camera. Passa a Profilo: Misurato", prof);
    defineSlider(d, page, "noiseLevel", "Livello rumore (×)", 1, 0.1, 8, 0.25, 4, 0.05,
                 "Moltiplica il rumore atteso del profilo: sopra 1 il filtro lo considera più forte e toglie di più", prof);

    GroupParamDescriptor* sp = defineGroup(d, page, "spatialGroup", "Forza", true);
    defineSlider(d, page, "lumaStrength", "Luminanza", 1, 0, 3, 0, 2, 0.05,
                 "Forza sul rumore di luminanza (grana): 1 = soglia di 1,2 volte il rumore atteso di ogni banda, sull'energia dei vicini", sp);
    defineSlider(d, page, "chromaStrength", "Colore", 1, 0, 3, 0, 2, 0.05,
                 "Forza sul rumore di colore (macchie): si può spingere più della luminanza", sp);
    defineChoice(d, page, "temporalFrames", "Fotogrammi", kNoiseFramesNames, kNoiseFramesCount, 0,
                 "Quanti fotogrammi prima e dopo: Automatico dal formato (RAW e Intra ±2, Long GOP compresso ±1). "
                 "Più fotogrammi tolgono più rumore sulle scene ferme e costano di più", sp);
    defineSlider(d, page, "preserveDetail", "Preserva dettaglio", 0.1, 0, 1, 0, 1, 0.01,
                 "Rimette questa parte della grana più fine tolta: evita l'effetto plastica", sp);

    defineChoice(d, page, "noiseView", "Vista", kNoiseViewNames, kNoiseViewCount, 0,
                 "Uscita; Rumore rimosso (grigio = niente, amplificato 4×); Mappa del rumore atteso; Movimento: "
                 "bianco dove i fotogrammi vicini sono stati usati, nero dove il movimento li ha esclusi");

    GroupParamDescriptor* adv = defineGroup(d, page, "advancedGroup", "Avanzate", false);
    defineSlider(d, page, "temporalStrength", "Forza temporale", 1, 0, 2, 0, 2, 0.05,
                 "Quanto il filtro accetta differenze tra un fotogramma e l'altro: più alta = più rumore tolto e più "
                 "rischio di scie sul movimento veloce. 0 = solo il fotogramma corrente", adv);
    defineSlider(d, page, "fineBand", "Dettaglio fine", 1, 0, 3, 0, 2, 0.05, "Bande di 1-2 pixel", adv);
    defineSlider(d, page, "mediumBand", "Dettaglio medio", 1, 0, 3, 0, 2, 0.05, "Banda di 4 pixel", adv);
    defineSlider(d, page, "coarseBand", "Dettaglio grosso", 1, 0, 3, 0, 2, 0.05,
                 "Bande di 8-16 pixel: macchie di colore e blocchi del Long GOP", adv);
    defineSlider(d, page, "shadowStrength", "Più forza nelle ombre", 0, 0, 1, 0, 1, 0.01,
                 "Aggiunge forza scendendo verso il nero, oltre a quella che il profilo dà già alle ombre", adv);
    defineSlider(d, page, "skinProtect", "Protezione pelle", 0.3, 0, 1, 0, 1, 0.01,
                 "Sui toni pelle (tinta calda, poco satura) conserva questa parte della trama fine: volti meno cerosi", adv);
    defineChoice(d, page, "nodeInput", "Ingresso (IDT)", kNodeInputNames, kNodeInputCount, 0,
                 "Codifica che entra nel nodo, per leggere la luminosità: Automatico la chiede a Resolve, poi alla "
                 "clip. Il nodo restituisce la stessa codifica", adv);
    defineButton(d, page, "measure", "Misura dal fotogramma",
                 "Senza zona campione: legge il rumore di tutto il fotogramma corrente per luminosità e banda (blocchi "
                 "più uniformi) e passa a Profilo: Misurato. Serve un fotogramma con zone piatte a più luminosità", adv);

    if (claimName("noiseSample")) hideParam(d.defineStringParam("noiseSample"), page);
    if (claimName("measuredProfile")) hideParam(d.defineStringParam("measuredProfile"), page);
    if (claimName("boundPath")) hideParam(d.defineStringParam("boundPath"), page);
    defineHiddenInt(d, page, "settingsVersion", 0);
}

ImageEffect* NoiseFactory::createInstance(OfxImageEffectHandle p_Handle, ContextEnum)
{
    return new NoiseEffect(p_Handle);
}

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
    defineButton(d, page, "skipAll", "Salta Noise ovunque",
                 "Spegne tutti i nodi Noise del progetto per lavorare veloce; un secondo clic li riaccende. Si "
                 "riaccendono da soli alla riapertura di Resolve. Prima dell'export controlla che la riga Noise non "
                 "dica SALTATO; se un nodo resta spento dopo averli riaccesi, svuota la render cache");

    GroupParamDescriptor* prof = defineGroup(d, page, "profileGroup", "Profilo del rumore", true);
    defineChoice(d, page, "profileMode", "Profilo", kNoiseProfileNames, kNoiseProfileCount, 0,
                 "Automatico: dalla camera, dall'ISO e dal formato della clip (letti da Log MetaRaw). Misurato: dal "
                 "fotogramma, premi Misura su un fotogramma con zone uniformi. Manuale: un profilo generico, regolato "
                 "solo da Livello rumore", prof);
    defineInfo(d, page, prof, "profileInfo", "Da", "—");
    defineButton(d, page, "measure", "Misura dal fotogramma",
                 "Legge il rumore del fotogramma corrente per luminosità e banda (blocchi più uniformi) e passa a "
                 "Profilo: Misurato. Meglio un fotogramma con cielo, muri o zone fuori fuoco a più luminosità", prof);
    defineSlider(d, page, "noiseLevel", "Livello rumore (×)", 1, 0.1, 8, 0.25, 4, 0.05,
                 "Moltiplica il rumore atteso del profilo: sopra 1 il filtro lo considera più forte e toglie di più", prof);
    defineChoice(d, page, "nodeInput", "Ingresso (IDT)", kNodeInputNames, kNodeInputCount, 0,
                 "Codifica che entra nel nodo, per leggere la luminosità: Automatico la chiede a Resolve, poi alla "
                 "clip. Il nodo restituisce la stessa codifica", prof);

    GroupParamDescriptor* tmp = defineGroup(d, page, "temporalGroup", "Temporale", true);
    defineChoice(d, page, "temporalFrames", "Fotogrammi", kNoiseFramesNames, kNoiseFramesCount, 0,
                 "Quanti fotogrammi prima e dopo: Automatico dal formato (RAW e Intra ±2, Long GOP compresso ±1). "
                 "Più fotogrammi tolgono più rumore sulle scene ferme e costano di più", tmp);
    defineSlider(d, page, "temporalStrength", "Forza temporale", 1, 0, 2, 0, 2, 0.05,
                 "Quanto il filtro accetta differenze tra un fotogramma e l'altro: più alta = più rumore tolto e più "
                 "rischio di scie sul movimento veloce. 0 = solo il fotogramma corrente", tmp);

    GroupParamDescriptor* sp = defineGroup(d, page, "spatialGroup", "Spaziale", true);
    defineSlider(d, page, "lumaStrength", "Luminanza", 1, 0, 3, 0, 2, 0.05,
                 "Forza sul rumore di luminanza (grana): 1 = soglia di 1,2 volte il rumore atteso di ogni banda, sull'energia dei vicini", sp);
    defineSlider(d, page, "chromaStrength", "Colore", 1, 0, 3, 0, 2, 0.05,
                 "Forza sul rumore di colore (macchie): si può spingere più della luminanza", sp);
    defineSlider(d, page, "fineBand", "Dettaglio fine", 1, 0, 3, 0, 2, 0.05, "Bande di 1-2 pixel", sp);
    defineSlider(d, page, "mediumBand", "Dettaglio medio", 1, 0, 3, 0, 2, 0.05, "Banda di 4 pixel", sp);
    defineSlider(d, page, "coarseBand", "Dettaglio grosso", 1, 0, 3, 0, 2, 0.05,
                 "Bande di 8-16 pixel: macchie di colore e blocchi del Long GOP", sp);
    defineSlider(d, page, "shadowStrength", "Più forza nelle ombre", 0, 0, 1, 0, 1, 0.01,
                 "Aggiunge forza scendendo verso il nero, oltre a quella che il profilo dà già alle ombre", sp);

    GroupParamDescriptor* pr = defineGroup(d, page, "protectGroup", "Protezione", true);
    defineSlider(d, page, "preserveDetail", "Preserva dettaglio", 0.1, 0, 1, 0, 1, 0.01,
                 "Rimette questa parte della grana più fine tolta: evita l'effetto plastica", pr);
    defineSlider(d, page, "skinProtect", "Protezione pelle", 0.3, 0, 1, 0, 1, 0.01,
                 "Sui toni pelle (tinta calda, poco satura) conserva questa parte della trama fine: volti meno cerosi", pr);

    defineChoice(d, page, "noiseView", "Vista", kNoiseViewNames, kNoiseViewCount, 0,
                 "Uscita; Rumore rimosso (grigio = niente, amplificato 4×); Mappa del rumore atteso; Movimento: "
                 "bianco dove i fotogrammi vicini sono stati usati, nero dove il movimento li ha esclusi");

    if (claimName("measuredProfile")) hideParam(d.defineStringParam("measuredProfile"), page);
    if (claimName("boundPath")) hideParam(d.defineStringParam("boundPath"), page);
    defineHiddenInt(d, page, "settingsVersion", 0);
}

ImageEffect* NoiseFactory::createInstance(OfxImageEffectHandle p_Handle, ContextEnum)
{
    return new NoiseEffect(p_Handle);
}

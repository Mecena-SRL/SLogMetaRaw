// SPDX-License-Identifier: GPL-3.0-or-later
#include "DevelopFactory.h"

#include "../common/ColourSpaces.h"
#include "../common/ParamDefs.h"
#include "../common/Update.h"
#include "../common/UpdateBadge.h"
#include "ClipMeta.h"
#include "DevelopEffect.h"
#include "LutExport.h"
#include "ToneParams.h"

using namespace OFX;

DevelopFactory::DevelopFactory() : PluginFactoryHelper<DevelopFactory>(kDevelopIdentifier, 1, 4) {}

void DevelopFactory::load() { Update::pluginLoaded(); }
void DevelopFactory::unload()
{
    stopReads();
    Update::pluginUnloaded();
}

void DevelopFactory::describe(ImageEffectDescriptor& p_Desc)
{
    describeEffectCommon(p_Desc, "Log MetaRaw",
                         "Controlli Camera Raw (Sony Video) per clip Sony MP4: si imposta da solo con i metadata di "
                         "camera registrati da Log MetaRaw. Ivan Mazzone + Claude (@Ivan_94m).",
                         "com.slogmetaraw.SLogMetaRaw.png", kPointwiseLutSafe);
}

void DevelopFactory::describeInContext(ImageEffectDescriptor& d, ContextEnum)
{
    resetParamNames();
    defineClips(d);
    PageParamDescriptor* page = d.definePageParam("Controls");

    defineVersionButton(d, page);
    defineInfo(d, page, nullptr, "camera", "Camera", "— premi Rileggi metadata");
    defineButton(d, page, "reload", "Rileggi metadata",
                 "Rilegge i metadata della clip dal file, li scrive nel Media Pool di Resolve "
                 "e riporta i controlli ai valori di camera");

    // Ripresa -> Spazio colore -> Toni -> Zone -> Dati di ripresa. Only the layout moved: names are permanent.
    GroupParamDescriptor* shot = defineGroup(d, page, "shotGroup", "Ripresa", true);
    // same order as the native Camera Raw "Sony Video" panel
    static const char* decodeOpts[] = { "Camera metadata", "Clip" };
    defineChoice(d, page, "decodeUsing", "Decodifica", decodeOpts, 2, 1,
                 "Camera metadata: usa i valori di ripresa e blocca i controlli. Clip: puoi modificarli", shot);
    static const char* wbOpts[] = { "As shot", "Daylight", "Cloudy", "Shade", "Tungsten", "Fluorescent", "Flash", "Custom" };
    defineChoice(d, page, "whiteBalance", "Bilanciamento", wbOpts, 8, 0,
                 "As shot riporta ai Kelvin di ripresa; muovendo gli slider diventa Custom", shot);
    defineToggle(d, page, "fcTemperature", "False color", "fc_temperature.png",
                 "Immagine in grigio, dominanti sull'asse della temperatura colorate: blu = fredda "
                 "(alza Temperatura), arancio = calda (abbassala). Muovi finche il neutro resta grigio", shot);
    defineSlider(d, page, "colorTemp", "Temperatura", 5600, 2000, 15000, 2000, 15000, 10,
                 "Temperatura in Kelvin: adattamento cromatico Bradford in luce lineare", shot);
    defineColor(d, page, "whitePick", "Punto bianco",
                "Col contagocce scegli un punto dell'immagine che deve essere bianco o grigio neutro: "
                "Temperatura e Tint si regolano di conseguenza", shot);
    defineSlider(d, page, "tint", "Tint", 0, -100, 100, -100, 100, 0.1, "Verde/magenta, perpendicolare al luogo di Planck", shot);
    defineToggle(d, page, "fcTint", "False color", "fc_tint.png",
                 "Immagine in grigio, dominanti sull'asse della tinta colorate: verde (alza Tint), "
                 "magenta (abbassalo). Muovi finche il neutro resta grigio", shot);
    GroupParamDescriptor* expo = defineGroup(d, page, "exposureGroup", "Esposizione", true, shot);
    defineSlider(d, page, "exposure", "EI", 800, 25, 409600, 50, 25600, 1,
                 "Exposure Index: il doppio dell'EI di ripresa = +1 stop", expo);
    defineToggle(d, page, "fcExposure", "False color", "fc_exposure.png",
                 "Mappa le fermate attorno al grigio 18%: verde = grigio medio, rosa = incarnato "
                 "(una fermata sopra), giallo = vicino al clip, rosso = clip, blu e viola = nero", expo);

    GroupParamDescriptor* space = defineGroup(d, page, "colourGroup", "Spazio colore", true);
    GroupParamDescriptor* input = defineGroup(d, page, "inputGroup", "Ingresso", false, space);
    static const char* spaceOpts[1 + kSpaceCount];   // Automatico, then the tables of DevelopMath.h
    static const char* gammaOpts[1 + kGammaCount];
    spaceOpts[0] = gammaOpts[0] = "Automatico";
    for (int i = 0; i < kSpaceCount; ++i) spaceOpts[i + 1] = kSpaceNames[i];
    for (int i = 0; i < kGammaCount; ++i) gammaOpts[i + 1] = kGammaNames[i];
    const char* inputHint = "Cosa entra nel nodo. Automatico lo chiede a Resolve o lo legge dalla camera: "
                            "cambialo solo se e sbagliato";
    defineChoice(d, page, "inputSpace", "Spazio colore", spaceOpts, 1 + kSpaceCount, 0, inputHint, input);
    defineChoice(d, page, "inputGamma", "Gamma", gammaOpts, 1 + kGammaCount, 0, inputHint, input);
    defineInfo(d, page, input, "nodeInfo", "Rilevato");
    defineButton(d, page, "inputReset", "Ripristina da camera",
                 "Riporta Spazio colore e Gamma in ingresso ad Automatico, i valori con cui ha girato la camera", input);
    static const char* levelOpts[] = { "Automatico", "Full (0-1023)", "Video (64-940)", "Nessuna correzione" };
    defineChoice(d, page, "dataLevel", "Data level", levelOpts, 4, 0,
                 "Scala di code value su cui Resolve ha decodificato la clip. Automatico usa l'attributo "
                 "Data Level della clip: se e su Auto non correggo niente. Full o Video lo dichiari tu, "
                 "utile su un ProRes esterno che nessun NLE segnala", input);
    defineInfo(d, page, input, "levelInfo", "Livello");
    defineButton(d, page, "levelReset", "Ripristina da clip", "Riporta Data level ad Automatico", input);
    defineInfo(d, page, input, "status", "Stato");

    GroupParamDescriptor* manual = defineGroup(d, page, "manualGroup", "Senza metadata", false, space);
    defineCheck(d, page, "unlockNoMeta", "Sblocca controlli", false,
                "Quando i metadata della clip mancano o non si leggono (registratori esterni, ProRes, clip lunghe): "
                "attiva i controlli sui riferimenti di ripresa qui sotto", manual);
    // Named shot* since 1.0 (Resolve restores by name): the camera values, or the ones typed here.
    const char* refHint = "Riferimento di ripresa: cambia lo zero di Exposure e Color Temp, non l'immagine. "
                          "Con White Balance su Custom i valori restano";
    DoubleParamDescriptor* refs[] = {
        defineSlider(d, page, "shotEI", "EI di ripresa", 800, 25, 409600, 100, 25600, 1, refHint, manual),
        defineSlider(d, page, "shotTemp", "Kelvin di ripresa", 5600, 2000, 15000, 2000, 15000, 10, refHint, manual),
        defineSlider(d, page, "shotTint", "Tint di ripresa", 0, -100, 100, -100, 100, 0.1, refHint, manual) };
    for (DoubleParamDescriptor* r : refs) {
        if (!r) continue;
        r->setAnimates(false);
        r->setIsSecret(true);   // shown when the box above is ticked
    }

    GroupParamDescriptor* output = defineGroup(d, page, "outputGroup", "Uscita", true, space);
    static const char* drtOpts[] = { "Nessuno (CST)", "ACES 2.0", "Luminanza (stile DaVinci)", "Filmico", "ACES 1.3",
                                     "OpenDRT" };
    defineChoice(d, page, "rendering", "Tone mapping", drtOpts, 6, 0,
                 "Come la scena diventa immagine per lo schermo (Rendering display). Nessuno: conversione diretta, le "
                 "luci oltre il bianco tagliano. ACES 2.0: Output Transform SDR 100 nit Rec.709. Luminanza: curva sulla "
                 "luminanza, tinte conservate (approssima il tone mapping DaVinci). Filmico: curva per canale come una "
                 "pellicola, le luci forti sfumano nel bianco. ACES 1.3: RRT + ODT Rec.709 100 nit. OpenDRT: v1.1.0 di "
                 "Jed Smith, look Standard, Rec.1886 100 nit. Sceglierlo imposta Compressione e Mappatura gamut "
                 "consigliate, che puoi cambiare. Timeline = Rec.709 / Gamma 2.4. Con uno spazio o una curva di scena "
                 "(DaVinci WG / Intermediate) l'immagine resa per Rec.709 viaggia in quella codifica, per altri plugin: "
                 "un CST verso Rec.709 / Gamma 2.4 senza tone mapping la riporta identica", output);
    static const char* gcOpts[] = { "Nessuna", "ACES riferimento (RGC)", "ACES parametrica" };
    defineChoice(d, page, "gamutCompress", "Compressione gamut", gcOpts, 3, 0,
                 "Come Gamut Compress Type del Color Space Transform: riporta in ACES AP1 i colori della camera fuori "
                 "gamut (LED, luci sature), prima del tone mapping. ACES riferimento: Reference Gamut Compression di "
                 "ACES 1.3. ACES parametrica: soglie, limiti e potenza qui sotto", output);
    const char* gcHint = "ACES parametrica: soglia = distanza dall'acromatico da cui si comprime (per canale: rosso "
                         "verso ciano, verde verso magenta, blu verso giallo), limite = distanza riportata sul bordo "
                         "del gamut, potenza = durezza della curva. Default = riferimento ACES 1.3";
    static const char* const gcNames[7] = { "gamutThrCyan", "gamutThrMagenta", "gamutThrYellow", "gamutLimCyan",
                                            "gamutLimMagenta", "gamutLimYellow", "gamutPower" };
    static const char* const gcLabels[7] = { "Soglia ciano", "Soglia magenta", "Soglia giallo", "Limite ciano",
                                             "Limite magenta", "Limite giallo", "Potenza" };
    for (int i = 0; i < 7; ++i) {
        const bool lim = i >= 3 && i < 6, thr = i < 3;
        DoubleParamDescriptor* s = defineSlider(d, page, gcNames[i], gcLabels[i], SM_RGC_REFERENCE[i],
                                                thr ? 0.0 : lim ? 1.001 : 0.5, thr ? 0.99 : lim ? 2.0 : 4.0,
                                                thr ? 0.5 : lim ? 1.001 : 1.0, thr ? 0.99 : lim ? 1.6 : 2.0, 0.001,
                                                gcHint, output);
        if (s) s->setIsSecret(true);   // shown with ACES parametrica
    }
    static const char* mapOpts[] = { "Nessuna", "Compressione saturazione" };
    defineChoice(d, page, "gamutMapping", "Mappatura gamut", mapOpts, 2, 0,
                 "Come Gamut Mapping Method del Color Space Transform: i colori vicini e oltre il bordo del gamut di "
                 "uscita vi rientrano con la stessa tinta invece di tagliarsi canale per canale (prima del tone "
                 "mapping, ginocchio al 90%). Utile anche con Nessuno (CST) verso Rec.709", output);
    defineSlider(d, page, "highlightVibrance", "Vibrance luci", 0, 0, 100, 0, 100, 1,
                 "Dopo il rendering: ridà colore alle luci chiare e poco sature (un cielo sovraesposto) senza toccare "
                 "quelle già sature (LED, insegne). Luminosità invariata, nessun canale oltre il bianco. Solo con un "
                 "Tone mapping", output);
    static const char* csOpts[] = { "Timeline", "DaVinci WG", "Rec.709", "Rec.2020", "P3 D65", "P3 D60", "P3 DCI",
                                    "S-Gamut", "S-Gamut3", "S-Gamut3.Cine", "ACES AP0", "ACES AP1" };
    defineChoice(d, page, "colorSpace", "Spazio colore", csOpts, 12, 0,
                 "Gamut in uscita, come un Color Space Transform. Timeline non converte", output);
    static const char* gmOpts[] = { "Timeline", "DaVinci Intermediate", "Linear", "Gamma 2.2", "Gamma 2.4", "Gamma 2.6",
                                    "Rec.709", "sRGB", "SLog", "SLog2", "SLog3", "ACEScct" };
    defineChoice(d, page, "gamma", "Gamma", gmOpts, 12, 0, "Curva in uscita. Timeline non converte", output);
    static const char* lutOpts[] = { "17", "33", "65" };
    defineChoice(d, page, "lutSize", "Dimensione LUT", lutOpts, kLutSizeCount, kLutDefaultSize,
                 "Punti per lato della LUT 3D esportata: 33 va bene per quasi tutto, 65 e piu fedele ma pesa di piu", output);
    defineButton(d, page, "exportLut", "Esporta LUT (.cube)",
                 "Salva una LUT 3D .cube con White Balance, Exposure, Toni e conversione Color Space / Gamma di "
                 "questo nodo. Ingresso: il segnale che entra nel nodo", output);

    GroupParamDescriptor* noise = defineGroup(d, page, "noiseGroup", "Maschera rumore", false);
    if (noise) noise->setHint("Per Neat Video e i denoiser: nell'alpha le zone che la camera rende rumorose all'EI "
                              "scelto. L'immagine non cambia");
    static const char* noiseOpts[] = { "Spenta", "Automatica", "Personalizzata" };
    defineChoice(d, page, "noiseMask", "Maschera", noiseOpts, 3, 0,
                 "Scrive nel canale alpha 1 dove la camera ha rumore e 0 dove è pulita, sfumando; i colori restano "
                 "identici. Automatica: soglia misurata o stimata per camera, ISO nativo e codec. Personalizzata: "
                 "Inizio e Sfumatura tuoi. In entrambe la soglia si sposta con l'EI scelto: ogni stop sopra il nativo "
                 "la avvicina di uno stop al grigio. In Resolve collega l'uscita alpha del nodo all'ingresso key "
                 "del nodo con Neat Video", noise);
    const char* noiseHint = "Personalizzata: stop sotto il grigio 18% a cui inizia la maschera all'ISO nativo, e "
                            "quanti stop servono per arrivare piena. L'EI scelto la sposta come in Automatica";
    defineSlider(d, page, "noiseStart", "Inizio", 3.0, 0.0, 10.0, 0.0, 8.0, 0.1, noiseHint, noise);
    defineSlider(d, page, "noiseFade", "Sfumatura", 1.5, 0.25, 4.0, 0.25, 4.0, 0.05, noiseHint, noise);
    defineCheck(d, page, "noiseView", "Vedi maschera", false,
                "Mostra la maschera al posto dell'immagine (bianco = zona da pulire). Solo per controllarla", noise);
    defineInfo(d, page, noise, "noiseInfo", "Soglia");

    GroupParamDescriptor* tones = defineGroup(d, page, "tonesGroup", "Toni", true);
    GroupParamDescriptor* zones = defineGroup(d, page, "zonesGroup", "Zone", false);
    defineToneParams(d, page, tones, zones);

    GroupParamDescriptor* details = defineGroup(d, page, "detailsGroup", "Dati di ripresa", false);
    if (details) details->setHint("Valori letti dalla clip, in sola lettura");
    for (int i = 0; i < kDetailCount; ++i) defineInfo(d, page, details, kDetails[i].param, kDetails[i].label);

    if (claimName("boundPath")) hideParam(d.defineStringParam("boundPath"), page);
    if (claimName("boundId")) hideParam(d.defineStringParam("boundId"), page);   // file name + size: survives a relink
    // Before 2.5 one list held space and gamma together; kept by name, read once to fill the two choices
    if (auto* legacy = defineChoice(d, page, "nodeInput", "Ingresso nodo", kNodeInputNames, kNodeInputCount, 0,
                                    "Sostituito da Spazio colore e Gamma, in Ingresso"))
        legacy->setIsSecret(true);
    defineHiddenInt(d, page, "camSpace", -1);
    defineHiddenInt(d, page, "camGamma", -1);
    defineHiddenInt(d, page, "levelRequired", -1);
    defineHiddenInt(d, page, "levelHost", -1);
    if (claimName("metaValid")) {
        BooleanParamDescriptor* valid = d.defineBooleanParam("metaValid");
        valid->setDefault(false);
        hideParam(valid, page);
    }
    defineHiddenInt(d, page, "refSource", 0);
    defineLegacyTones(d, page);
    defineHiddenInt(d, page, "settingsVersion", 0);
}

ImageEffect* DevelopFactory::createInstance(OfxImageEffectHandle p_Handle, ContextEnum)
{
    return new DevelopEffect(p_Handle);
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "DetailFactory.h"

#include "../common/ColourSpaces.h"
#include "../common/ParamDefs.h"
#include "../common/Update.h"
#include "../common/UpdateBadge.h"
#include "../common/ZoneParams.h"
#include "DetailEffect.h"

using namespace OFX;

DetailFactory::DetailFactory() : PluginFactoryHelper<DetailFactory>(kDetailIdentifier, 1, 0) {}

void DetailFactory::load() { Update::pluginLoaded(); }
void DetailFactory::unload() { Update::pluginUnloaded(); }

void DetailFactory::describe(ImageEffectDescriptor& p_Desc)
{
    describeEffectCommon(p_Desc, "S-Log MetaRaw Detail",
                         "Recupero locale di luci e ombre, Texture, Clarity e Dehaze in luce di scena, dopo S-Log "
                         "MetaRaw. Spaziale: escluso da Generate LUT. Ivan Mazzone + Claude (@Ivan_94m).",
                         "com.slogmetaraw.SLogMetaRawDetail.png", kSpatial);
}

static DoubleParamDescriptor* trim(ImageEffectDescriptor& d, PageParamDescriptor* page, GroupParamDescriptor* group,
                                   const char* name, const char* label, double lo, double hi, const char* hint)
{
    DoubleParamDescriptor* p = defineSlider(d, page, name, label, 0, lo, hi, lo, hi, 0.1, hint, group);
    if (p) p->setDigits(2);
    return p;
}

// A point on the picture, normalised to the frame (0,0 bottom left): where Misura reads light or shadow.
static void definePoint(ImageEffectDescriptor& d, PageParamDescriptor* page, GroupParamDescriptor* group,
                        const char* name, const char* label, double x, double y, const char* hint)
{
    if (!claimName(name)) return;
    Double2DParamDescriptor* p = d.defineDouble2DParam(name);
    p->setLabels(label, label, label);
    p->setHint(hint);
    p->setDoubleType(eDoubleTypeXYAbsolute);
    p->setDefaultCoordinateSystem(eCoordinatesNormalised);
    p->setDefault(x, y);
    p->setAnimates(false);
    p->setIsSecret(true);   // shown in Cinematografico
    p->setParent(*group);
    page->addChild(*p);
}

static void defineHiddenDouble(ImageEffectDescriptor& d, PageParamDescriptor* page, const char* name, double def)
{
    if (!claimName(name)) return;
    DoubleParamDescriptor* p = d.defineDoubleParam(name);
    p->setDefault(def);
    hideParam(p, page);
}

void DetailFactory::describeInContext(ImageEffectDescriptor& d, ContextEnum)
{
    resetParamNames();
    defineClips(d);
    PageParamDescriptor* page = d.definePageParam("Controls");
    defineVersionButton(d, page);
    defineInfo(d, page, nullptr, "detailInfo", "Detail", "Spaziale · escluso da Generate LUT");

    GroupParamDescriptor* mode = defineGroup(d, page, "modeGroup", "Sviluppo e spazio colore", true);
    defineChoice(d, page, "devMode", "Modalità", kDetailModeNames, kDetailModeCount, 0,
                 "Standard: toni locali, presenza e zone. Cinematografico: rapporto di luce sul soggetto misurato "
                 "sull'immagine. Zonale: sistema zonale di Ansel Adams, esposizione e sviluppo per zone", mode);
    defineInfo(d, page, mode, "modeInfo", "Cosa fa", kDetailModeInfo[0]);
    if (auto* p = defineChoice(d, page, "zonalPhase", "Fase", kZonalPhaseNames, kZonalPhaseCount, 0,
                               "Zonale in tre nodi in serie, come in camera oscura: 1 Sviluppo (curva globale, "
                               "senza aloni: esposizione e N+/N-), 2 Stampa (per aree e bordi), 3 Presenza (Texture, "
                               "Clarity, Dehaze). Copia il nodo, incollalo dopo e cambia solo Fase", mode))
        p->setIsSecret(true);   // shown in Zonale
    defineChoice(d, page, "nodeInput", "Ingresso (IDT)", kNodeInputNames, kNodeInputCount, 0,
                 "Codifica che entra nel nodo: la decodifica in luce lineare e la riscrive uguale. Automatico la chiede "
                 "a Resolve, poi alla clip. Va dopo S-Log MetaRaw e prima di CST/LUT", mode);
    defineChoice(d, page, "destination", "Destinazione (ODT)", kDetailDestNames, kDetailDestCount, 0,
                 "La trasformazione dopo il nodo: decide dove arrivano il bianco (zona X) e il nero (zona 0). Dal "
                 "Bianco usa Avanzate › Bianco; le altre lo fissano: CST 2,5 stop sopra il grigio e 4,5 sotto (7 stop "
                 "in tutto), DRT 4,5 / 6, PQ 1000 nit 6,5 / 7, Scena DaVinci WG 9 / 9. Il nodo non converte: lavora "
                 "sempre nella codifica d'ingresso", mode);

    GroupParamDescriptor* cinema = defineGroup(d, page, "cinemaGroup", "Cinematografico", true);
    if (cinema) cinema->setIsSecret(true);
    if (auto* p = defineToggle(d, page, "subjectFace", "Soggetto: viso", "subject_face.png",
                               "Incarnato: misura la luminanza (Y), aree piccole, tiene la pelle", cinema)) {
        p->setDefault(true);
        p->setIsSecret(true);
    }
    if (auto* p = defineToggle(d, page, "subjectLandscape", "Soggetto: paesaggio", "subject_landscape.png",
                               "Sfondo o paesaggio: misura la norma del colore, aree grandi", cinema))
        p->setIsSecret(true);
    definePoint(d, page, cinema, "ratioLightPos", "Viso luce", 0.4, 0.55,
                "Punto sul lato illuminato del soggetto (frazione del fotogramma, 0,0 in basso a sinistra)");
    definePoint(d, page, cinema, "ratioShadowPos", "Viso ombra", 0.6, 0.55,
                "Punto sul lato in ombra del soggetto");
    if (auto* p = defineButton(d, page, "ratioMeasure", "Misura rapporto",
                               "Legge luce e ombra nei due punti (media su un'area dell'1% dell'altezza) e imposta "
                               "il rapporto obiettivo uguale al misurato", cinema))
        p->setIsSecret(true);
    DoubleParamDescriptor* sliders[] = {
        defineSlider(d, page, "ratioTarget", "Rapporto obiettivo (stop)", 1, 0, 5, 0, 5, 0.05,
                     "1 stop = 1:2, 2 = 1:4, 3 = 1:8. La luce resta ferma, l'ombra si sposta. Misura lo imposta da solo",
                     cinema),
        defineSlider(d, page, "ratioHardness", "Durezza passaggio", 50, 0, 100, 0, 100, 1,
                     "Quanto e netto il passaggio dalla luce all'ombra: 0 morbido (luce diffusa), 100 netto (taglio di "
                     "luce)", cinema),
        defineSlider(d, page, "ratioBackground", "Spegni sfondo", 0, 0, 100, 0, 100, 1,
                     "Porta verso il nero cio che e sotto l'ombra del soggetto: il fondo del chiaroscuro. 100 = 4 stop",
                     cinema),
        defineSlider(d, page, "ratioSubjectLift", "Luce sul soggetto", 0, 0, 100, 0, 100, 1,
                     "Alza il lato illuminato e cio che e sopra, rapporto invariato. 100 = 1 stop", cinema) };
    for (DoubleParamDescriptor* p : sliders)
        if (p) {
            p->setDigits(2);
            p->setIsSecret(true);
        }
    if (auto* p = defineInfo(d, page, cinema, "ratioInfo", "Rapporto", "Scegli i due punti e premi Misura rapporto"))
        p->setIsSecret(true);

    GroupParamDescriptor* adams = defineGroup(d, page, "adamsGroup", "Zonale", true);
    if (adams) adams->setIsSecret(true);
    definePoint(d, page, adams, "adamsShadowPos", "Ombra", 0.3, 0.3,
                "Un'ombra in cui vuoi ancora dettaglio (frazione del fotogramma, 0,0 in basso a sinistra)");
    definePoint(d, page, adams, "adamsHighPos", "Luce", 0.7, 0.7, "Una luce in cui vuoi ancora dettaglio");
    if (auto* p = defineButton(d, page, "adamsMeasure", "Misura zone",
                               "Legge i due punti: l'ombra resta sulla sua zona, la luce si colloca dove cade con "
                               "sviluppo normale (N 0)", adams))
        p->setIsSecret(true);
    DoubleParamDescriptor* places[] = {
        defineSlider(d, page, "adamsShadowPlace", "Ombra sulla zona", 3, 0, 10, 0, 10, 0.1,
                     "La zona su cui collocare l'ombra misurata (esposizione): III = ombra con dettaglio", adams),
        defineSlider(d, page, "adamsHighPlace", "Luci sulla zona", 8, 0, 10, 0, 10, 0.1,
                     "La zona su cui portare la luce misurata (sviluppo): sopra quella misurata N+, sotto N-. VIII = "
                     "luce con dettaglio", adams) };
    for (DoubleParamDescriptor* p : places)
        if (p) {
            p->setDigits(1);
            p->setIsSecret(true);
        }
    if (auto* p = defineInfo(d, page, adams, "adamsInfo", "Zone", "Scegli i due punti e premi Misura zone"))
        p->setIsSecret(true);

    GroupParamDescriptor* range = defineGroup(d, page, "rangeGroup", "Gamma dinamica", true);
    defineToggle(d, page, "viewGain", "Vista: guadagno", "view_gain.png",
                 "Mostra quanto il nodo alza (arancio) o abbassa (blu) ogni zona: grigio = invariato. "
                 "Bande a 0,25 / 0,5 / 1 / 2 stop", range);
    defineToggle(d, page, "viewBase", "Vista: base", "view_base.png",
                 "Mostra la base edge-aware su cui lavorano i toni locali, senza il dettaglio", range);
    defineToggle(d, page, "viewZonesColor", "Vista: zone", "view_zones.png",
                 "Mostra la zona (0-X) in cui cade ogni area dopo il nodo, a colori: viola 0, blu I-II, verde-azzurro "
                 "III-IV, grigio V, rosa VI, giallo VII, arancio VIII, rosso IX, bianco X. Zona 0 = nero e zona X = "
                 "bianco della Destinazione", range);
    defineToggle(d, page, "viewZonesGrey", "Vista: zone in grigi", "view_zones_grey.png",
                 "Le stesse zone come scala di undici grigi, dal nero (0) al bianco (X)", range);
    trim(d, page, range, "localContrast", "Local Contrast", -100, 50,
         "Contrasto delle grandi aree attorno al grigio, a dettaglio intatto. Il Contrast del nodo principale e "
         "globale; qui il dettaglio fine resta com'e");
    trim(d, page, range, "localHighlights", "Local Highlights", -100, 100,
         "Negativo: comprime le grandi aree luminose e tiene, anzi rinforza, la texture (a -100 il massimo registrato "
         "arriva sul Bianco di Avanzate); le luci piu compresse vanno verso il bianco. Positivo: piu stacco per aree. "
         "Grigio e toni sotto restano fermi");
    trim(d, page, range, "localShadows", "Local Shadows", -100, 100,
         "Apre (positivo) o chiude le ombre per aree, a dettaglio intatto. 100 = 2 stop");
    if (auto* p = defineSlider(d, page, "printEdgeBurn", "Brucia i bordi (stop)", 0, 0, 2, 0, 2, 0.05,
                               "Zonale, Stampa: scurisce i margini dell'inquadratura come la bruciatura dei bordi in "
                               "camera oscura. Dipende solo dalla posizione: nessun alone", range)) {
        p->setDigits(2);
        p->setIsSecret(true);
    }
    if (auto* p = defineSlider(d, page, "printEdgeSize", "Ampiezza bordi (%)", 30, 10, 60, 10, 60, 1,
                               "Quanta parte dell'inquadratura, dai margini verso il centro, riceve la bruciatura",
                               range)) {
        p->setDigits(0);
        p->setIsSecret(true);
    }

    GroupParamDescriptor* presence = defineGroup(d, page, "presenceGroup", "Presenza", true);
    trim(d, page, presence, "texture", "Texture", -100, 100,
         "Dettaglio fine (pelle, tessuti, foglie). Negativo leviga; la grana sotto la soglia rumore non sale");
    trim(d, page, presence, "clarity", "Clarity", -100, 100,
         "Contrasto locale a media scala, pesato sui mezzitoni");
    trim(d, page, presence, "dehaze", "Dehaze", -100, 100,
         "Toglie (positivo) o aggiunge velo. Il colore e il livello del velo sono in Avanzate › Velo, mai stimati "
         "fotogramma per fotogramma");
    defineButton(d, page, "detailReset", "Azzera dettaglio", "Riporta a zero tutti i controlli di questo nodo", presence);

    GroupParamDescriptor* zones = defineGroup(d, page, "localZonesGroup", "Zone locali", false);
    defineZoneParams(d, page, zones, "localZone", false);

    GroupParamDescriptor* adv = defineGroup(d, page, "detailAdvancedGroup", "Avanzate", false);
    if (auto* p = defineSlider(d, page, "preserveDetail", "Preserva dettaglio", 100, 0, 100, 0, 100, 1,
                               "100 = i toni locali muovono solo le aree; 0 = come il nodo principale, puntuale", adv))
        p->setDigits(0);
    defineSlider(d, page, "detailRadius", "Raggio (% altezza)", 4, 1, 8, 1, 8, 0.1,
                 "Dimensione delle aree dei toni locali, relativa all'altezza: lo stesso look a ogni risoluzione", adv);
    defineSlider(d, page, "edgeThreshold", "Soglia bordi (EV)", 0.5, 0.25, 1.0, 0.25, 1.0, 0.01,
                 "Salto di luminanza che conta come bordo: piu bassa = meno aloni, meno recupero", adv);
    defineSlider(d, page, "noiseThreshold", "Soglia rumore (EV)", 0.04, 0.0, 0.15, 0.0, 0.15, 0.005,
                 "Sotto questa ampiezza il dettaglio e trattato come rumore: Texture non lo alza. 0 = nessuna protezione",
                 adv);
    defineSlider(d, page, "clarityCenter", "Clarity: centro (EV)", 0, -3, 3, -3, 3, 0.05,
                 "Tono, in stop dal grigio 18%, dove Clarity lavora di piu", adv);
    if (auto* p = defineSlider(d, page, "localWhite", "Bianco (stop)", 2.5, 1, 10, 1, 8, 0.05,
                               "Dove Local Highlights -100 porta il massimo registrato, in stop sopra il grigio 18%. "
                               "2,5 = bianco Rec.709 con un CST senza tone mapping; con un DRT (ACES, AgX, DaVinci) "
                               "alza a 4-5. Vale con Destinazione: Dal Bianco", adv))
        p->setDigits(2);

    GroupParamDescriptor* haze = defineGroup(d, page, "hazeGroup", "Velo", false);
    defineSlider(d, page, "hazeLevel", "Livello velo (EV)", 2, -2, 6, -2, 6, 0.05,
                 "Luminanza del velo in stop dal grigio 18%: da li in su Dehaze non agisce", haze);
    defineSlider(d, page, "hazeWarmth", "Colore velo", 0, -100, 100, -100, 100, 0.1,
                 "Negativo = velo freddo (cielo, distanza), positivo = caldo (fumo, polvere)", haze);

    if (auto* g = defineGroup(d, page, "zonalGuideGroup", "Guida", true)) {   // shown in Zonale
        g->setIsSecret(true);
        for (int i = 0; i < kZonalGuideRows; ++i)
            if (auto* p = defineInfo(d, page, g, kZonalGuideNames[i], kZonalGuideLabels[i],
                                     kZonalGuide[SM_DT_PHASE_DEVELOP][i]))
                p->setIsSecret(true);
    }

    if (claimName("boundPath")) hideParam(d.defineStringParam("boundPath"), page);
    defineHiddenInt(d, page, "camSpace", -1);
    defineHiddenInt(d, page, "camGamma", -1);
    defineHiddenInt(d, page, "settingsVersion", 0);
    defineHiddenDouble(d, page, "ratioLight", 0.0);      // L of the norm at the two points, set by Misura
    defineHiddenDouble(d, page, "ratioShadow", 0.0);
    defineHiddenDouble(d, page, "ratioMeasured", 0.0);   // their ratio in stops (Y for a face, the norm for a landscape)
    defineHiddenDouble(d, page, "adamsShadowL", 0.0);    // Zonale: L of the norm at the shadow and the highlight
    defineHiddenDouble(d, page, "adamsHighL", 0.0);
}

ImageEffect* DetailFactory::createInstance(OfxImageEffectHandle p_Handle, ContextEnum)
{
    return new DetailEffect(p_Handle);
}

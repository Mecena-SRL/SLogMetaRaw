// SPDX-License-Identifier: GPL-3.0-or-later
#include "CreativeFactory.h"

#include "../common/ColourSpaces.h"
#include "../common/ParamDefs.h"
#include "../common/Update.h"
#include "../common/UpdateBadge.h"
#include "CreativeEffect.h"
#include "CreativePanel.h"

using namespace OFX;

CreativeFactory::CreativeFactory() : PluginFactoryHelper<CreativeFactory>(kCreativeIdentifier, 1, 0) {}

void CreativeFactory::load() { Update::pluginLoaded(); }
void CreativeFactory::unload() { Update::pluginUnloaded(); }

void CreativeFactory::describe(ImageEffectDescriptor& p_Desc)
{
    describeEffectCommon(p_Desc, "Log MetaRaw Creative",
                         "Sviluppo creativo: pellicole negative e invertibili, bianco e nero, Technicolor, Autochrome, "
                         "tre lastre, dagherrotipo, collodio, stampe antiche, video analogico e camere digitali "
                         "d'epoca, ciascuno con il suo processo. Dopo S-Log MetaRaw. Spaziale: escluso da Generate "
                         "LUT. Ivan Mazzone + Claude (@Ivan_94m).",
                         "com.slogmetaraw.SLogMetaRawDetail.png", kSpatial);
    p_Desc.setTemporalClipAccess(true);   // sequential plates and two-colour cameras read other frames
}

void CreativeFactory::describeInContext(ImageEffectDescriptor& d, ContextEnum)
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
    defineInfo(d, page, nullptr, "detailInfo", "Creative", "Spaziale · escluso da Generate LUT");

    GroupParamDescriptor* groups[kGrCount];
    for (int g = 0; g < kGrCount; ++g)
        groups[g] = defineGroup(d, page, kCrGroups[g].name, kCrGroups[g].label, kCrGroups[g].open);
    GroupParamDescriptor* proc = groups[kGrProcess];
    defineChoice(d, page, "process", "Processo", kCrProcessNames, kCrProcessCount, kCrColourNegative,
                 "Il procedimento: come la luce diventa immagine. Ognuno ha i suoi comandi qui sotto", proc);
    defineInfo(d, page, proc, "processInfo", "Come funziona", kCrProcessInfo[kCrColourNegative]);
    auto choice = [&](const CrChoiceDef& c) {
        defineChoice(d, page, c.name, c.label, c.options, c.count, CreativeControls().*(c.field), c.hint,
                     groups[c.group]);
        if (!std::strcmp(c.name, "medium")) defineInfo(d, page, groups[kGrView], "mediumInfo", "Come si vede", "");
    };
    for (const CrChoiceDef& c : kCrChoices)
        if (std::strcmp(c.name, "mediumRoom") != 0) choice(c);   // the room goes with the light, below
    defineChoice(d, page, "nodeInput", "Ingresso (IDT)", kNodeInputNames, kCreativeInputCount, 0,
                 "Codifica che entra nel nodo. Automatico la chiede a Resolve, poi alla clip. Anche dopo un grade: "
                 "Rec.709 (grigio 18% a 0,41), DaVinci WG, ACES (cct, cg, 2065-1), Cineon. L'uscita è Rec.709 "
                 "Gamma 2.4, o la stessa codifica con Uscita Log",
                 proc);

    for (const CrSliderDef& s : kCrSliders) {
        const double def = CreativeControls().*(s.field);
        if (DoubleParamDescriptor* p = defineSlider(d, page, s.name, s.label, def, s.lo, s.hi, s.lo, s.hi, s.inc,
                                                    s.hint, groups[s.group]))
            p->setDigits(s.inc >= 1 ? 0 : (s.inc >= 0.1 ? 1 : 2));
        if (!std::strcmp(s.name, "mediumNits"))
            for (const CrChoiceDef& c : kCrChoices)
                if (!std::strcmp(c.name, "mediumRoom")) choice(c);
    }
    defineButton(d, page, "creativeReset", "Azzera comandi",
                 "Riporta ai valori del processo tutti i comandi; processo e varianti restano", groups[kGrProcess]);

    if (claimName("boundPath")) hideParam(d.defineStringParam("boundPath"), page);
    defineHiddenInt(d, page, "camSpace", -1);
    defineHiddenInt(d, page, "camGamma", -1);
    defineHiddenInt(d, page, "settingsVersion", 0);
    defineHiddenInt(d, page, "mediumLoaded", -1);   // the medium whose values the response sliders hold
}

ImageEffect* CreativeFactory::createInstance(OfxImageEffectHandle p_Handle, ContextEnum)
{
    return new CreativeEffect(p_Handle);
}

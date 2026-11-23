// SPDX-License-Identifier: GPL-3.0-or-later
// Panel values -> DevelopParams. Same steps and order as ofx/SLogMetaRaw/src/develop/BuildParams.cpp, so the image
// is the one the DaVinci Resolve node produces. Beta slice: no data-level fix, no false colour, no zones.
#import "DevelopHost.h"

#include <cstring>

#include "../../../ofx/SLogMetaRaw/gen/DevelopMath.h"

static const int kSpaceCount = 20;
static const int kGammaCount = 25;

NSData* LMRBuildDevelopParams(const LMRDevelopControls* c)
{
    DevelopParams p = DevelopParams();
    const int nodeSpace = c->inSpace >= 0 && c->inSpace < kSpaceCount ? c->inSpace : 0;
    const int nodeGamma = c->inGamma >= 0 && c->inGamma < kGammaCount ? c->inGamma : 0;
    p.nodeSpace = nodeSpace;
    p.nodeGamma = nodeGamma;

    p.drt = c->rendering >= 1 && c->rendering <= 5 ? c->rendering : 0;
    if (c->gamutCompress == 1) sm_set_gamut_compress(&p, SM_RGC_REFERENCE);
    p.gmOn = c->gamutMapping == 1 ? 1 : 0;
    p.hlVib = p.drt ? (float)sm_clamp(c->highlightVibrance / 100.0, 0.0, 1.0) : 0.0f;

    // Output. In Resolve "Timeline" leaves the node's encoding and a CST follows; in Final Cut nothing follows the
    // effect and the timeline is display-referred, so "Progetto" (-1) converts to the project's encoding.
    const bool projOk = c->projSpace >= 0 && c->projSpace < kSpaceCount && c->projGamma >= 0 && c->projGamma < kGammaCount;
    p.outSpace = c->outSpace >= 0 && c->outSpace < kSpaceCount ? c->outSpace : (projOk ? c->projSpace : 1);
    p.outGamma = c->outGamma >= 0 && c->outGamma < kGammaCount ? c->outGamma : (projOk ? c->projGamma : 3);
    bool forced = false;
    if (p.drt) sm_display_encoding(&p.outSpace, &p.outGamma, &forced);
    p.convert = (p.drt || p.outSpace != nodeSpace || p.outGamma != nodeGamma) ? 1 : 0;

    p.expo = (c->shotEI > 0.0 && c->ei > 0.0) ? (float)(c->ei / c->shotEI) : 1.0f;
    sm_set_white_balance(&p, c->shotTemp, c->shotTint, c->temp, c->tint);

    // false colour: same priority and calibration as BuildParams.cpp
    int fc = c->fcExposure ? 1 : c->fcTemperature ? 2 : c->fcTint ? 3 : 0;
    p.fcMode = fc;
    if ((fc == 2 || fc == 3) && !sm_fc_calibrate(&p, c->temp, c->tint)) p.fcMode = 0;

    SMToneControls t = sm_tone_defaults();
    t.softClip = c->softClip ? 1 : 0;
    if (c->softClipLevel > 0.0) t.softClipLevel = c->softClipLevel;
    t.contrast = c->contrast;
    t.highlights = c->highlights;
    t.shadows = c->shadows;
    t.whites = c->whites;
    t.blacks = c->blacks;
    t.vibrance = c->vibrance;
    t.saturation = c->saturation;
    sm_set_tone(&p, &t, p.outSpace, p.convert, p.expo);

    return [NSData dataWithBytes:&p length:sizeof(DevelopParams)];
}

size_t LMRDevelopParamsSize(void) { return sizeof(DevelopParams); }

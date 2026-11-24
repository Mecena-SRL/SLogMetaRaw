// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Params.h"
#include "Transfer.h"
#include "Gamut.h"
#include "Levels.h"
#include "Tone.h"
#include "Color.h"
#include "FalseColor.h"
#include "Display.h"

// Maschera rumore: 0 above the edge, 1 a fade below it, smoothstep between. It reads the picture, never writes it.
SM_FN float sm_noise_mask(float y, SM_CREF(DevelopParams) p) {
    const float t = sm_clamp((p.nmEdge - SM_LOG2(SM_MAX(y, 1e-10f) / 0.18f)) * p.nmInvFade, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// The developed pixel; mask is the Maschera rumore when p.nmOn, else 0. Resolve keys the node with the alpha of its
// plugin (output where alpha = 1, input where 0): a mask node therefore passes its input untouched and only writes
// the alpha, for the key input of the denoiser's node.
SM_FN SMf3 sm_develop_a(SMf3 in, SM_CREF(DevelopParams) p, SM_OUT(float) mask) {
    mask = 0.0f;
    if (p.bypass != 0 && p.levelFix == 0) return in;
    SMf3 lin = smf3(sm_decode(in.x, p.nodeGamma), sm_decode(in.y, p.nodeGamma),
                    sm_decode(in.z, p.nodeGamma));
    lin = sm_fix_levels(lin, p);
    if (p.bypass != 0)   // data level only: the develop is off for this clip
        return smf3(sm_encode(lin.x, p.nodeGamma), sm_encode(lin.y, p.nodeGamma),
                    sm_encode(lin.z, p.nodeGamma));
    lin = smf3(lin.x * p.expo, lin.y * p.expo, lin.z * p.expo);
    SMf3 xyz = sm_to_xyz(lin, p.nodeSpace);

    // white balance: von Kries in Bradford LMS, as-shot white -> chosen white
    SMf3 lms = sm_mul(SM_BRADFORD, 0, xyz);
    lms = smf3(lms.x * p.ratioL, lms.y * p.ratioM, lms.z * p.ratioS);
    xyz = sm_mul(SM_BRADFORD, 9, lms);
    xyz = smf3(xyz.x * p.norm, xyz.y * p.norm, xyz.z * p.norm);
    if (p.nmOn != 0) {
        mask = sm_noise_mask(xyz.y, p);
        return p.nmOn == 2 ? smf3(mask, mask, mask) : in;
    }

    if (p.fcMode != 0) return sm_fc_emit(sm_false_color(xyz, sm_fc_headroom(lin, p), p), p);
    if (p.toneOn != 0 || p.colorOn != 0 || p.curve.roofOn != 0) xyz = sm_tone5(xyz, p);

    if (p.gcOn != 0) xyz = sm_gamut_compress(xyz, p);
    if (p.drt != 0) {   // rendered for a display gamut: the output one, else Rec.709 carried in the output space
        const int ds = p.outSpace >= 1 && p.outSpace <= 5 ? p.outSpace : 1;
        SMf3 d = sm_display(xyz, p.drt, ds, p.gmOn);
        if (p.hlVib > 0.0f) d = sm_highlight_vibrance(d, p.hlVib, ds);
        if (ds != p.outSpace) d = sm_from_xyz(sm_to_xyz(d, ds), p.outSpace);
        return smf3(sm_encode(d.x, p.outGamma), sm_encode(d.y, p.outGamma), sm_encode(d.z, p.outGamma));
    }
    const int space = p.convert != 0 ? p.outSpace : p.nodeSpace;
    const int gamma = p.convert != 0 ? p.outGamma : p.nodeGamma;
    SMf3 o = sm_from_xyz(xyz, space);
    if (p.gmOn != 0) o = sm_gamut_map(o);
    return smf3(sm_encode(o.x, gamma), sm_encode(o.y, gamma), sm_encode(o.z, gamma));
}

SM_FN SMf3 sm_develop(SMf3 in, SM_CREF(DevelopParams) p) {
    float mask;
    return sm_develop_a(in, p, mask);
}

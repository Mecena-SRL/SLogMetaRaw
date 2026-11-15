// SPDX-License-Identifier: GPL-3.0-or-later
// Display renderings: scene XYZ (D65) -> display-linear RGB 0..1 in the output gamut (Python: tests/model/display.py).
//   1 ACES 1.3: Reference Gamut Compression, RRT, ODT Rec.709 100 nit dim surround (ACES CTL 1.x)
//   2 Luminanza: max-RGB norm through the tonescale, channel ratios kept, path to white near the peak
//   3 Filmico: the tonescale per channel on an inset gamut (AgX-like): highlights reach white on their own
// 2 and 3 share the 100 nit tonescale of ACES 2.0 / OpenDRT (Siragusano): grey 0.18 -> 0.1, 128 -> 1.0,
// and two gamut compressions: ACES RGC in AP1, then into the display gamut (knee 0.9).
#pragma once
#include "Gamut.h"

#define SM_DRT_ACES 1
#define SM_DRT_LUMINANCE 2
#define SM_DRT_FILMIC 3

// ACES native matrices (D60): AP0->AP1, AP1->AP0, AP1->XYZ, XYZ->AP1. The ends use the D65-adapted ones.
SM_CONST float SM_ACES_M[36] = {
    1.4514393161f, -0.2365107469f, -0.2149285693f, -0.0765537734f, 1.1762296998f, -0.0996759264f,
    0.0083161484f, -0.0060324498f, 0.9977163014f,
    0.6954522414f, 0.1406786965f, 0.1638690622f, 0.0447945634f, 0.8596711185f, 0.0955343182f,
    -0.0055258826f, 0.0040252103f, 1.0015006723f,
    0.6624541811f, 0.1340042065f, 0.1561876870f, 0.2722287168f, 0.6740817658f, 0.0536895174f,
    -0.0055746495f, 0.0040607335f, 1.0103391003f,
    1.6410233797f, -0.3248032942f, -0.2364246952f, -0.6636628587f, 1.6153315917f, 0.0167563477f,
    0.0117218943f, -0.0082844420f, 0.9883948585f };

// Segmented splines: RRT C5 (6 coefficients low, 6 high), ODT 48 nit C9 (10 + 10).
SM_CONST float SM_ACES_C5[12] = {
    -4.0f, -4.0f, -3.1573765773f, -0.4852499958f, 1.8477324706f, 1.8477324706f,
    -0.7185482425f, 2.0810307172f, 3.6681241237f, 4.0f, 4.0f, 4.0f };
SM_CONST float SM_ACES_C9[20] = {
    -1.6989700043f, -1.6989700043f, -1.4779f, -1.2291f, -0.8648f, -0.448f, 0.00518f, 0.4511080334f,
    0.9113744414f, 0.9113744414f,
    0.5154386965f, 0.8470437783f, 1.1358f, 1.3802f, 1.5197f, 1.5985f, 1.6467f, 1.6746091357f,
    1.6878733390f, 1.6878733390f };
// log10 of x and y at the min, mid and max points of each spline (C9: the RRT at grey -/+ 6.5 stops).
SM_CONST float SM_ACES_PTS[12] = {
    -5.26017743f, -4.0f, -0.744727495f, 0.681241237f, 4.67381243f, 4.0f,
    -2.54062362f, -1.69897000f, 0.681241237f, 0.681241237f, 3.00247681f, 1.68124124f };

SM_FN float sm_aces_spline(float x, int c9) {
    const int o = c9 != 0 ? 6 : 0, n = c9 != 0 ? 8 : 4, hi = c9 != 0 ? 10 : 6;
    SM_CPTR coef = c9 != 0 ? SM_ACES_C9 : SM_ACES_C5;
    const float lx = SM_LOG10(SM_MAX(x, 5.96046448e-08f));
    const float x0 = SM_ACES_PTS[o], x1 = SM_ACES_PTS[o + 2], x2 = SM_ACES_PTS[o + 4];
    if (lx <= x0) return SM_POW(10.0f, SM_ACES_PTS[o + 1]);
    if (lx >= x2) return SM_POW(10.0f, SM_ACES_PTS[o + 5] + (c9 != 0 ? 0.04f : 0.0f) * (lx - x2));
    float k;
    int base;
    if (lx < x1) { k = (float)(n - 1) * (lx - x0) / (x1 - x0); base = 0; }
    else { k = (float)(n - 1) * (lx - x1) / (x2 - x1); base = hi; }
    const int j = (int)k;
    const float t = k - (float)j;
    const float c0 = coef[base + j], c1 = coef[base + j + 1], c2 = coef[base + j + 2];
    return SM_POW(10.0f, t * t * (0.5f * c0 - c1 + 0.5f * c2) + t * (c1 - c0) + 0.5f * (c0 + c1));
}

SM_FN SMf3 sm_aces_sat(SMf3 c, float s) {
    const float y = 0.2722287168f * c.x + 0.6740817658f * c.y + 0.0536895174f * c.z;
    return smf3((1.0f - s) * y + s * c.x, (1.0f - s) * y + s * c.y, (1.0f - s) * y + s * c.z);
}

// ACES RGC distance compression towards the largest channel: thr and lim per channel, power 1.2.
SM_FN float sm_rgc_dist(float d, float t, float l) {
    if (d <= t) return d;
    const float s = (l - t) / SM_POW(SM_POW((1.0f - t) / (l - t), -1.2f) - 1.0f, 1.0f / 1.2f);
    const float n = (d - t) / s;
    return t + s * n / SM_POW(1.0f + SM_POW(n, 1.2f), 1.0f / 1.2f);
}
SM_FN SMf3 sm_rgc(SMf3 c, float tr, float tg, float tb, float lr, float lg, float lb) {
    const float a = SM_MAX(c.x, SM_MAX(c.y, c.z));
    if (a <= 0.0f) return c;
    return smf3(a - sm_rgc_dist((a - c.x) / a, tr, lr) * a, a - sm_rgc_dist((a - c.y) / a, tg, lg) * a,
                a - sm_rgc_dist((a - c.z) / a, tb, lb) * a);
}
SM_FN SMf3 sm_rgc_ap1(SMf3 xyz) {   // ACES 1.3 Reference Gamut Compression, returned in AP1
    return sm_rgc(sm_from_xyz(xyz, 10), 0.815f, 0.803f, 0.880f, 1.147f, 1.264f, 1.312f);
}

SM_FN SMf3 sm_aces_rrt(SMf3 a) {   // ACES2065-1 -> OCES, returned in AP1 as the ODT reads it
    const float mx = SM_MAX(a.x, SM_MAX(a.y, a.z)), mn = SM_MIN(a.x, SM_MIN(a.y, a.z));
    const float sat = (SM_MAX(mx, 1e-10f) - SM_MAX(mn, 1e-10f)) / SM_MAX(mx, 1e-2f);
    const float ch = SM_SQRT(SM_MAX(a.z * (a.z - a.y) + a.y * (a.y - a.x) + a.x * (a.x - a.z), 0.0f));
    const float yc = (a.z + a.y + a.x + 1.75f * ch) / 3.0f;
    const float sx = (sat - 0.4f) / 0.2f;
    const float st = SM_MAX(1.0f - SM_ABS(sx * 0.5f), 0.0f);
    const float gain = 0.05f * (1.0f + (sx < 0.0f ? -1.0f : 1.0f) * (1.0f - st * st)) * 0.5f;
    const float glow = yc <= 0.08f * 2.0f / 3.0f ? gain : yc >= 0.16f ? 0.0f : gain * (0.08f / yc - 0.5f);
    a = smf3(a.x * (1.0f + glow), a.y * (1.0f + glow), a.z * (1.0f + glow));
    float w = 0.0f;   // red modifier: cubic B-spline basis of width 135 degrees around red
    if (!(a.x == a.y && a.y == a.z)) {
        const float hue = SM_ATAN2(1.7320508f * (a.y - a.z), 2.0f * a.x - a.y - a.z) * 57.2957795f;
        if (hue > -67.5f && hue < 67.5f) {
            const float k = (hue + 67.5f) * 4.0f / 135.0f;
            const int j = (int)k;
            const float t = k - (float)j, t2 = t * t, t3 = t2 * t;
            w = j == 0 ? t3 / 6.0f : j == 1 ? (-3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f) / 6.0f
              : j == 2 ? (3.0f * t3 - 6.0f * t2 + 4.0f) / 6.0f : (1.0f - t) * (1.0f - t) * (1.0f - t) / 6.0f;
            w *= 1.5f;
        }
    }
    a.x += w * sat * (0.03f - a.x) * (1.0f - 0.82f);
    SMf3 p = sm_mul(SM_ACES_M, 0, smf3(SM_MAX(a.x, 0.0f), SM_MAX(a.y, 0.0f), SM_MAX(a.z, 0.0f)));
    p = sm_aces_sat(smf3(SM_MAX(p.x, 0.0f), SM_MAX(p.y, 0.0f), SM_MAX(p.z, 0.0f)), 0.96f);
    return smf3(sm_aces_spline(p.x, 0), sm_aces_spline(p.y, 0), sm_aces_spline(p.z, 0));
}

SM_FN SMf3 sm_aces_odt(SMf3 p) {   // OCES (AP1) -> display-linear AP1, 100 nit dim surround
    p = smf3(sm_aces_spline(p.x, 1), sm_aces_spline(p.y, 1), sm_aces_spline(p.z, 1));
    const float k0 = 1.0f / (48.0f - 0.02f);
    p = smf3((p.x - 0.02f) * k0, (p.y - 0.02f) * k0, (p.z - 0.02f) * k0);
    SMf3 xyz = sm_mul(SM_ACES_M, 18, p);   // dark to dim surround: Y^0.9811 at the same chromaticity
    const float s = xyz.x + xyz.y + xyz.z;
    if (s != 0.0f) {
        const float k = xyz.y > 0.0f ? SM_POW(xyz.y, 0.9811f) / SM_MAX(xyz.y / s, 1e-10f) / s : 0.0f;
        p = sm_mul(SM_ACES_M, 27, smf3(xyz.x * k, xyz.y > 0.0f ? xyz.y * k : 0.0f, xyz.z * k));
    }
    return sm_aces_sat(p, 0.93f);
}

// The 100 nit tonescale: m2 (x / (x + s2))^g, then the toe f^2 / (f + t1); constants from the model.
SM_FN float sm_drt_tonescale(float x) {
    x = SM_MAX(x, 0.0f);
    const float f = 1.04710376f * SM_POW(x / (x + 0.919858254f), 1.15f);
    return SM_MIN(f * f / (f + 0.04f), 1.0f);
}

SM_FN SMf3 sm_drt_into_display(SMf3 xyz, int space) {
    SMf3 c = sm_from_xyz(sm_to_xyz(sm_rgc_ap1(xyz), 10), space);
    c = sm_rgc(c, 0.9f, 0.9f, 0.9f, 1.2f, 1.2f, 1.2f);
    return smf3(SM_MAX(c.x, 0.0f), SM_MAX(c.y, 0.0f), SM_MAX(c.z, 0.0f));
}

SM_FN SMf3 sm_display(SMf3 xyz, int mode, int space) {
    SMf3 c;
    if (mode == SM_DRT_ACES) {
        const SMf3 a = sm_mul(SM_ACES_M, 9, sm_rgc_ap1(xyz));
        c = sm_from_xyz(sm_to_xyz(sm_aces_odt(sm_aces_rrt(a)), 10), space);
    } else if (mode == SM_DRT_LUMINANCE) {
        c = sm_drt_into_display(xyz, space);
        const float n = SM_MAX(c.x, SM_MAX(c.y, c.z));
        if (n <= 0.0f) return smf3(0.0f, 0.0f, 0.0f);
        const float nt = sm_drt_tonescale(n), w2 = nt * nt, w4 = w2 * w2, w = w4 * w4;   // path to white: nt^8
        c = smf3(nt * (c.x / n + w * (1.0f - c.x / n)), nt * (c.y / n + w * (1.0f - c.y / n)),
                 nt * (c.z / n + w * (1.0f - c.z / n)));
    } else {
        c = sm_drt_into_display(xyz, space);
        const int o = space * 9 + 3;   // the luminance row of the gamut
        const float y = SM_RGB_TO_XYZ[o] * c.x + SM_RGB_TO_XYZ[o + 1] * c.y + SM_RGB_TO_XYZ[o + 2] * c.z;
        c = smf3(sm_drt_tonescale(0.8f * c.x + 0.2f * y), sm_drt_tonescale(0.8f * c.y + 0.2f * y),
                 sm_drt_tonescale(0.8f * c.z + 0.2f * y));
        const float yt = SM_RGB_TO_XYZ[o] * c.x + SM_RGB_TO_XYZ[o + 1] * c.y + SM_RGB_TO_XYZ[o + 2] * c.z;
        c = smf3((c.x - 0.18f * yt) / 0.82f, (c.y - 0.18f * yt) / 0.82f, (c.z - 0.18f * yt) / 0.82f);
    }
    return smf3(sm_clamp(c.x, 0.0f, 1.0f), sm_clamp(c.y, 0.0f, 1.0f), sm_clamp(c.z, 0.0f, 1.0f));
}

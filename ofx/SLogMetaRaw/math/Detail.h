// SPDX-License-Identifier: GPL-3.0-or-later
// Per-pixel maths of the Detail node, shared by its CPU passes and Metal kernels.
// Reference: tests/model/detail.py (the algorithm is described there and in docs/DETAIL.md).
#pragma once
#include "Params.h"
#include "Transfer.h"
#include "Gamut.h"
#include "Tone.h"
#include "Color.h"

#ifdef __METAL_VERSION__
#define SM_GRID const device float*
#else
#define SM_GRID const float*
#endif

#define SM_DT_GG 4     // half-kernel lengths: guide sigma 1 px, fine texture, coarse texture on its grid
#define SM_DT_G1 16
#define SM_DT_GT 32

SM_CONST float SM_DT_ETA = 1e-12f;
SM_CONST float SM_DT_PSI = 0.25f;               // Texture soft limit, EV
SM_CONST float SM_DT_CGREY = 74.5762205f;       // S-Log3 code values per stop at grey
SM_CONST float SM_DT_FLOOR_EV = -9.5f;          // lifts fade out below this, so the noise floor stays
SM_CONST float SM_DT_FLOOR_W = 3.0f;
SM_CONST float SM_DT_CLARITY_W = 3.0f;
SM_CONST float SM_DT_HAZE_MIN_T = 0.4f;
SM_CONST float SM_DT_HAZE_G0 = 0.015625f;
SM_CONST float SM_DT_HL_GAMMA = 0.6f;          // Local Highlights raises the kept detail by GAMMA * (1 - T')

struct DetailParams {
    int W, H;          // frame
    int s, w, h;       // working grid: scale and size
    int st, wt, ht;    // coarse texture grid
    int space, gamma;  // input encoding
    float rBx, rBy, r2x, r2y, r3x, r3y, rpx, rpy, rtx, rty;   // box radii on the grid, per axis
    float eps, epsT;
    int nG, n1, nT;    // used lengths of the half kernels below
    float gG[SM_DT_GG];
    float g1[SM_DT_G1];
    float gT[SM_DT_GT];
    SMToneCurve curve;     // Local Contrast, Shadows and zones on the base
    int toneOn;
    float hlE, hlStart;    // Local Highlights: the shoulder on the base with the detail split off
    float hlT1, hlT2;      // detail kept whole up to T1, saturating by T1 + T2 (larger steps are edges)
    float hlB1, hlB2;      // the same for the boost, smaller: on an edge it can only push a little
    float hlTop;           // the recorded top: near it there is no detail left to keep, only the sensor clip
    float hlAmount;        // Local Highlights < 0: its eased amount, the weight of the white taper
    float hlWhiteLin;      // where the top lands, linear: the taper keeps the channels under it
    float lam, tau0, clarity, clarityCenter, texture;
    int hazeOn;
    float hazeOmega, hazeMix, hazeLevel;
    float hazeA[3], hazeInvA[3];
    int view;          // 0 picture, 1 gain, 2 base, 3 zones in colour, 4 zones in grey
    float toRef[9];    // input RGB -> Rec.2020, for the norm
    float zoneWhite;   // the destination's white in stops over grey: zone X (views 3, 4)
    float zoneBlack;   // its black in stops under grey: zone 0
    int zdOn;          // Zonale, development: a curve on the pixel's own L (dt_zonal_gain)
    float zdLs, zdLso, zdK, zdHi, zdHiD, zdLoD;
    float ebAmt, ebR0, ebS;   // the print's burned edges: stops at the edge, where the burn starts, its width
    int pzOn;          // Zonale, presence: Texture and Clarity whole in zones II-VIII, fading to nothing at 0 and X
    // the zone views' legend, in pixels from the bottom left: the bar 0-X (rows y0..y1), the ticks over it, the band
    int lgOn, lgM, lgY0, lgY1, lgTop, lgX0, lgX1, lgSc, lgTk, lgTh, lgTw;
    int lgIn[2], lgOut[2];   // where the shadow and the highlight were read and where they land: x of each tick
};

// Zonale, development: the shadow Ls lands on Lso, the line turns about it with slope k (N+/N-) up to the placed
// highlight hi; under the shadow a toe, over the highlight a shoulder, each with the line's slope where it starts.
SM_FN float dt_zonal_gain(float L, SM_CREF(DetailParams) p) {
    float y = p.zdLso + p.zdK * (L - p.zdLs);
    if (y > p.zdHi) y = p.zdHi + p.zdHiD * (1.0f - SM_EXP(-(y - p.zdHi) / p.zdHiD));
    else if (y < p.zdLso) y = p.zdLso - p.zdLoD * (1.0f - SM_EXP(-(p.zdLso - y) / p.zdLoD));
    return y - L;
}

// The print's burned edges: a superellipse distance, 1 at the middle of each side, the burn growing smoothly over
// its outer ebS. Position only: no neighbours, no halo.
SM_FN float dt_edge_burn(int x, int y, SM_CREF(DetailParams) p) {
    float u = 2.0f * ((float)x + 0.5f) / (float)p.W - 1.0f;
    float w = 2.0f * ((float)y + 0.5f) / (float)p.H - 1.0f;
    float r = SM_SQRT(SM_SQRT(u * u * u * u + w * w * w * w));
    float t = sm_clamp((r - p.ebR0) / p.ebS, 0.0f, 1.0f);
    return -p.ebAmt * t * t * (3.0f - 2.0f * t);
}

SM_FN float dt_luma(SMf3 x, SM_CREF(DetailParams) p) {
    SMf3 r = smf3(p.toRef[0] * x.x + p.toRef[1] * x.y + p.toRef[2] * x.z,
                  p.toRef[3] * x.x + p.toRef[4] * x.y + p.toRef[5] * x.z,
                  p.toRef[6] * x.x + p.toRef[7] * x.y + p.toRef[8] * x.z);
    return sm_clamp(sm_t5_log(sm_t5_norm(r)), -16.0f, 16.0f);   // fmax/fmin: NaN -> -16
}

// The noise floor in EV at log2 luminance B: larger where S-Log3 spends fewer codes per stop.
SM_FN float dt_tau(float B, float tau0) {
    float n = SM_T5_GREY * SM_EXP2(B);
    float c = n >= 0.01125f ? 78.71934387f * n / (n + 0.01f) : 4695.551191f * n;
    return SM_MIN(1.0f, tau0 * SM_DT_CGREY / c);
}

// x up to t1, then saturating smoothly to t1 + t2.
SM_FN float dt_lim(float x, float t1, float t2) {
    float a = SM_ABS(x);
    if (a <= t1) return x;
    float v = t1 + t2 * SM_TANH((a - t1) / t2);
    return x < 0.0f ? -v : v;
}

SM_FN float dt_soft1(float g) {
    float a = SM_ABS(g);
    if (a <= 1.0f) return g;
    float v = 1.0f + 0.5f * SM_TANH((a - 1.0f) / 0.5f);
    return g < 0.0f ? -v : v;
}

SM_FN float dt_bilinear(SM_GRID grid, int w, int h, int s, int x, int y) {
    float u = sm_clamp(((float)x + 0.5f) / (float)s - 0.5f, 0.0f, (float)(w - 1));
    float v = sm_clamp(((float)y + 0.5f) / (float)s - 0.5f, 0.0f, (float)(h - 1));
    int i0 = (int)u, j0 = (int)v;
    int i1 = i0 + 1 < w ? i0 + 1 : w - 1, j1 = j0 + 1 < h ? j0 + 1 : h - 1;
    float fx = u - (float)i0, fy = v - (float)j0;
    float top = grid[j0 * w + i0] + (grid[j0 * w + i1] - grid[j0 * w + i0]) * fx;
    float bot = grid[j1 * w + i0] + (grid[j1 * w + i1] - grid[j1 * w + i0]) * fx;
    return top + (bot - top) * fy;
}

SM_FN float dt_dark_energy(SMf3 xa) {   // xa = x / A per channel: a soft minimum of the three
    return (SM_EXP(-sm_clamp(xa.x, 0.0f, 1.5f) / 0.05f) + SM_EXP(-sm_clamp(xa.y, 0.0f, 1.5f) / 0.05f)
            + SM_EXP(-sm_clamp(xa.z, 0.0f, 1.5f) / 0.05f)) / 3.0f;
}

SM_FN float dt_haze_channel(float c, float a, float t) {
    if (c <= 0.0f) return c;   // no rectification of the noise at black
    float g = 1.0f / t - (1.0f / t - 1.0f) * a / c;
    float u = g / SM_DT_HAZE_G0;
    return c * SM_DT_HAZE_G0 * (u >= 2.0f ? u : 1.0f + SM_EXP(u - 2.0f));
}

SM_FN SMf3 dt_haze_pixel(SMf3 x, float t, SM_CREF(DetailParams) p) {
    if (p.hazeMix > 0.0f)
        return smf3(p.hazeMix * p.hazeA[0] + (1.0f - p.hazeMix) * x.x, p.hazeMix * p.hazeA[1] + (1.0f - p.hazeMix) * x.y,
                    p.hazeMix * p.hazeA[2] + (1.0f - p.hazeMix) * x.z);
    return smf3(dt_haze_channel(x.x, p.hazeA[0], t), dt_haze_channel(x.y, p.hazeA[1], t),
                dt_haze_channel(x.z, p.hazeA[2], t));
}

// The zone (0..X) of a log2 luminance out of the node: zone V is grey, zone 0 the destination's black, zone X its
// white; five zones share the stops on each side. So the scale follows the output: about 7 stops for Rec.709.
SM_FN float dt_zone_of(float Lout, float white, float black) {
    return Lout <= 0.0f ? 5.0f + 5.0f * Lout / black : 5.0f + 5.0f * Lout / white;
}

// Everything after the filters, for one pixel: the log2 gain G, B for the views, and Local Highlights' share of G
// with the weight of its white taper (the colour follows it to white, as in the main node).
struct DtOut { float G; float B; float H; float W; };
SM_FN DtOut dt_gain(float L, float Lu, float Gg, float aB, float bB, float Dg, float G1, float G2, int x, int y,
                    SM_CREF(DetailParams) p) {
    DtOut o;
    float tg = 2.0f * dt_tau(Lu, p.tau0);
    float dg = L - Gg;
    float Lg = L - dg * tg * tg / (dg * dg + tg * tg + SM_DT_ETA);
    float B = aB * Lg + bB;
    float Bl = p.lam * B + (1.0f - p.lam) * L;
    float Gb = 0.0f;
    if (p.toneOn != 0) {
        Gb = sm_t5_curve(Bl, p.curve).d;
        if (Gb > 0.0f) Gb *= sm_t5_ramp((Bl - SM_DT_FLOOR_EV) / SM_DT_FLOOR_W);
    }
    o.H = 0.0f;
    o.W = 0.0f;
    if (p.hlE != 0.0f) {
        // near the top the base is the sensor clip: the detail fades out and the curve takes the pixel whole
        float keep = 1.0f - sm_t5_ramp((Bl - (p.hlTop - 1.5f)) / 1.5f);
        float Dt = keep * dt_lim(L - Bl, p.hlT1, p.hlT2);
        SMShoulder s = sm_t5_shoulder_at(L - Dt, p.hlE, p.hlStart);
        float g = s.d;
        if (p.hlE < 0.0f) {   // a recovery: the detail gains contrast but no pixel ends up brighter
            float Db = keep * dt_lim(L - Bl, p.hlB1, p.hlB2);
            float tn = 3.0f * dt_tau(L - Dt, p.tau0);
            g = SM_MIN(g - SM_DT_HL_GAMMA * s.s * Db * Db * Db / (Db * Db + tn * tn + SM_DT_ETA), 0.0f);
            o.H = g;
            o.W = p.hlAmount * s.w;
        }
        Gb += g;
    }
    float K = 0.0f;
    if (p.clarity != 0.0f) {
        float z = (B - p.clarityCenter) / SM_DT_CLARITY_W;
        K = p.clarity * SM_EXP(-z * z) * Dg;
    }
    float Tx = 0.0f;
    if (p.texture != 0.0f) {
        float dt = G1 - G2;
        float ta = dt_tau(B, p.tau0);
        float gate = p.texture > 0.0f ? dt * dt / (dt * dt + ta * ta + SM_DT_ETA) : 1.0f;
        float q = dt / SM_DT_PSI;
        Tx = p.texture * dt / (1.0f + q * q) * gate;
    }
    float Gz = 0.0f;
    if (p.zdOn != 0) {
        Gz = dt_zonal_gain(L, p);
        if (Gz > 0.0f) Gz *= sm_t5_ramp((L - SM_DT_FLOOR_EV) / SM_DT_FLOOR_W);
    }
    float wz = 1.0f;
    if (p.pzOn != 0) {   // the zone this pixel lands on: presence where the print still shows texture (II-VIII)
        float z = dt_zone_of(L + Gb + Gz, p.zoneWhite, p.zoneBlack);
        wz = sm_t5_ramp((z - 0.5f) / 1.5f) * sm_t5_ramp((9.5f - z) / 1.5f);
    }
    o.G = Gb + dt_soft1((K + Tx) * wz);
    if (p.zdOn != 0) o.G += Gz;
    if (p.ebAmt != 0.0f) o.G += dt_edge_burn(x, y, p);
    o.B = B;
    return o;
}

// viewGain: the picture in grey where |G| < 0.1, then blue (lowered) or orange (raised) bands.
SM_CONST float SM_DT_BLUE[12] = { 0.62f, 0.78f, 1.00f, 0.35f, 0.58f, 1.00f, 0.15f, 0.35f, 0.95f, 0.08f, 0.12f, 0.70f };
SM_CONST float SM_DT_ORANGE[12] = { 1.00f, 0.85f, 0.60f, 1.00f, 0.68f, 0.30f, 1.00f, 0.50f, 0.10f, 0.85f, 0.25f, 0.05f };
SM_FN SMf3 dt_view_gain(float G, float L) {
    float a = SM_ABS(G);
    if (a < 0.1f) {
        float m = sm_clamp((L + 6.0f) / 11.5f, 0.06f, 0.92f);
        return smf3(m, m, m);
    }
    int band = a < 0.25f ? 0 : (a < 0.5f ? 1 : (a < 1.0f ? 2 : 3));
    if (G < 0.0f) return smf3(SM_DT_BLUE[band * 3], SM_DT_BLUE[band * 3 + 1], SM_DT_BLUE[band * 3 + 2]);
    return smf3(SM_DT_ORANGE[band * 3], SM_DT_ORANGE[band * 3 + 1], SM_DT_ORANGE[band * 3 + 2]);
}

// viewZones: the zones of the picture out, as flat colours or as an eleven-step grey scale (display values).
SM_CONST float SM_DT_ZONE_RGB[33] = { 0.25f, 0.00f, 0.35f,  0.10f, 0.10f, 0.55f,  0.10f, 0.35f, 0.85f,
                                      0.00f, 0.60f, 0.70f,  0.25f, 0.65f, 0.30f,  0.50f, 0.50f, 0.50f,
                                      0.95f, 0.65f, 0.55f,  0.95f, 0.85f, 0.25f,  1.00f, 0.60f, 0.10f,
                                      0.95f, 0.20f, 0.10f,  1.00f, 1.00f, 1.00f };
SM_CONST int SM_DT_GLYPH[11] = { 0x7B6F, 0x2C97, 0x73E7, 0x72CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF,
                                  0x5AAD };   // 0-9 and X, 3x5, top row first, left column the high bit
SM_FN SMf3 dt_zone_rgb(int z, int grey) {
    if (grey) {
        float g = 0.1f * (float)z;
        return smf3(g, g, g);
    }
    return smf3(SM_DT_ZONE_RGB[z * 3], SM_DT_ZONE_RGB[z * 3 + 1], SM_DT_ZONE_RGB[z * 3 + 2]);
}
// Zones 0 and X hold no detail: hatched, so what is lost shows at a glance.
SM_FN SMf3 dt_view_zones(float Lout, int x, int y, int grey, SM_CREF(DetailParams) p) {
    int z = (int)sm_clamp(dt_zone_of(Lout, p.zoneWhite, p.zoneBlack) + 0.5f, 0.0f, 10.0f);   // the cast is the floor
    SMf3 c = dt_zone_rgb(z, grey);
    if ((z == 0 || z == 10) && (x + y) % 8 < 3) return smf3(0.5f * c.x + 0.25f, 0.5f * c.y + 0.25f, 0.5f * c.z + 0.25f);
    return c;
}
// The legend over the zone views: a dark band at the bottom, the bar 0-X numbered, and over it the ticks, grey where
// the shadow and the highlight were read, white where they land. Outside the band, the view as it is.
SM_FN SMf3 dt_legend(int x, int y, SMf3 c, int grey, SM_CREF(DetailParams) p) {
    if (p.lgOn == 0 || y >= p.lgTop || x < p.lgX0 || x >= p.lgX1) return c;
    SMf3 band = smf3(0.08f, 0.08f, 0.08f);
    if (y >= p.lgY0 && y < p.lgY1 && x >= p.lgM && x < p.W - p.lgM) {
        int span = p.W - 2 * p.lgM, u = (x - p.lgM) * 11;
        int z = u / span;
        if (u % span < 11) return band;   // the first column of each cell: a gap between the zones
        int sc = p.lgSc, gl = p.lgM + (2 * z + 1) * span / 22 - 3 * sc / 2;
        int gb = p.lgY0 + (p.lgY1 - p.lgY0 - 5 * sc) / 2;
        if (x >= gl && x < gl + 3 * sc && y >= gb && y < gb + 5 * sc) {
            int bit = 14 - 3 * (4 - (y - gb) / sc) - (x - gl) / sc;
            if ((SM_DT_GLYPH[z] >> bit) & 1) {
                float t = z >= 5 ? 0.0f : 0.95f;
                return smf3(t, t, t);
            }
        }
        return dt_zone_rgb(z, grey);
    }
    if (p.lgTk != 0 && y >= p.lgY1 && y < p.lgY1 + p.lgTh) {
        for (int k = 0; k < 2; ++k)
            if (x >= p.lgOut[k] - p.lgTw && x <= p.lgOut[k] + p.lgTw) return smf3(0.95f, 0.95f, 0.95f);
        for (int k = 0; k < 2; ++k)
            if (x >= p.lgIn[k] - p.lgTw / 2 && x <= p.lgIn[k] + p.lgTw / 2) return smf3(0.5f, 0.5f, 0.5f);
    }
    return band;
}

// The picture out, linear in the input gamut: the gain, and where Local Highlights pulls down, the path to white.
SM_FN SMf3 dt_apply(SMf3 j, DtOut r, SM_CREF(DetailParams) p) {
    float g = SM_EXP2(r.G);
    j = smf3(j.x * g, j.y * g, j.z * g);
    if (r.H < 0.0f) j = sm_from_xyz(sm_t5_hl_white(sm_to_xyz(j, p.space), r.H, r.W, p.hlWhiteLin, 1, p.space), p.space);
    return j;
}

// A display colour (Rec.709, gamma 2.2) in the node's encoding, as the false colours do.
SM_FN SMf3 dt_emit(SMf3 c, SM_CREF(DetailParams) p) {
    SMf3 lin = smf3(sm_decode(c.x, 2), sm_decode(c.y, 2), sm_decode(c.z, 2));
    SMf3 o = sm_from_xyz(sm_to_xyz(lin, 1), p.space);
    return smf3(sm_encode(o.x, p.gamma), sm_encode(o.y, p.gamma), sm_encode(o.z, p.gamma));
}

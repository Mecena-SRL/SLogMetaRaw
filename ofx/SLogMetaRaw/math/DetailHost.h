// SPDX-License-Identifier: GPL-3.0-or-later
// CPU only: the Detail node's parameters from the panel and the frame size, in double.
#pragma once
#include "Detail.h"
#include "HostOnly.h"
#include "ToneHost.h"
#ifndef __METAL_VERSION__
#include <algorithm>
#include <vector>

struct SMDetailControls {
    double localContrast, localHighlights, localShadows, texture, clarity, dehaze;
    double preserveDetail, detailRadius, edgeThreshold, noiseThreshold, clarityCenter, hazeLevel, hazeWarmth;
    double localWhite;
    int viewGain, viewBase, viewZones;            // viewZones: 0 off, 1 colours, 2 greys
    int devMode, destination;                     // SM_DT_MODE_*, SM_DT_DEST_*
    double adamsShadowL, adamsHighL, adamsShadowPlace, adamsHighPlace;  // Zonale: L read, zones placed on
    double splitSoft, splitHard;                  // Split grade: stops of the soft and the hard exposure
    int splitEngine;                              // SM_DT_ENGINE_*: how the two exposures join
    int viewProvino, provinoKind;                 // the test print on the picture: SM_DT_PV_*
    double provinoStep;                           // stops between neighbouring cells
    int zonalPhase;                               // SM_DT_PHASE_*: Zonale in one node or in three
    double printEdgeBurn, printEdgeSize;          // Zonale, print: stops burned at the edges, % of the frame
};

static const int SM_DT_GRID_BASE = 540;   // working height: H / s in [0.707, 1.414) * 540
static const double SM_DT_HL_START = -0.5;  // Local Highlights starts half a stop under grey: the detail is kept there
static const int SM_DT_MODE_ZONAL = 0, SM_DT_MODE_SPLIT = 1, SM_DT_MODE_UNITO = 2;   // Zonale (Ansel Adams), Split grade, Unito
static const double SM_DT_SPLIT_RANGE = 3.0;                    // stops either way of each exposure
static const double SM_DT_SPLIT_SHADOW_ZONE = 3.0, SM_DT_SPLIT_HIGH_ZONE = 8.0;   // where a print with both at 0 puts them
static const int SM_DT_ENGINE_CURVE = 0, SM_DT_ENGINE_LAYERS = 1;   // one curve with a sliding pivot; two layers of paper
static const double SM_DT_LAYER_HARD_GRADE = 5.0, SM_DT_LAYER_SOFT_GRADE = 0.0;   // what the two layers are
static const int SM_DT_PV_STRIPS = 0, SM_DT_PV_MATRIX = 1, SM_DT_PV_UNIONS = 2;   // the test print's layouts
// Unito: the negative's development takes the subject's range to the paper's as far as it can (N-/N+), the print's
// grade the rest. Auto zone reads the shadow and the highlight at these percentiles of the picture.
static const double SM_DT_UNITO_K_MIN = 0.5, SM_DT_UNITO_K_MAX = 2.0;
static const double SM_DT_AUTO_LOW = 0.02, SM_DT_AUTO_HIGH = 0.98;
static const double SM_DT_ADAMS_MIN_SPREAD = 0.25;
// Zonale in phases, one node each (or all in one): development (global), print (local), presence (detail)
static const int SM_DT_LG_MIN_H = 48;   // under it the zone views have no legend
static const int SM_DT_PHASE_ALL = 0, SM_DT_PHASE_DEVELOP = 1, SM_DT_PHASE_PRINT = 2, SM_DT_PHASE_PRESENCE = 3;
static const double SM_DT_ZD_K_MIN = 0.25, SM_DT_ZD_K_MAX = 4.0;   // development slope
static const double SM_DT_ZD_ROOM = 1.0;   // toe and shoulder approach a stop beyond zone 0 and zone X
// Destinations: manual (localWhite), Rec.709 CST, Rec.709 DRT, PQ 1000, scene (DaVinci WG/Intermediate, no ODT yet).
// White and black in stops from grey: zone X and zone 0. Rec.709 CST: 2.5 + 4.5 = 7 stops.
static const int SM_DT_DEST_COUNT = 5;
static const double SM_DT_DEST_WHITE[SM_DT_DEST_COUNT] = { 0.0, 2.5, 4.5, 6.5, 9.0 };
static const double SM_DT_DEST_BLACK[SM_DT_DEST_COUNT] = { 4.5, 4.5, 6.0, 7.0, 9.0 };
static const double SM_DT_MIN_GAP = 0.05;   // two measured points closer than this (stops) are not a measure

static inline SMDetailControls sm_detail_defaults(void) {
    SMDetailControls c = SMDetailControls();
    c.preserveDetail = 100.0;
    c.detailRadius = 4.0;
    c.edgeThreshold = 0.5;
    c.noiseThreshold = 0.04;
    c.hazeLevel = 2.0;
    c.localWhite = 2.5;
    c.adamsShadowPlace = 3.0;
    c.adamsHighPlace = 8.0;
    c.printEdgeSize = 30.0;
    c.provinoStep = 0.5;
    return c;
}

// The level the recorded top lands on: the node's own Bianco, or the destination's. And its black.
static inline double dt_destination_white(const SMDetailControls* v) {
    return v->destination >= 1 && v->destination < SM_DT_DEST_COUNT ? SM_DT_DEST_WHITE[v->destination] : v->localWhite;
}
static inline double dt_destination_black(const SMDetailControls* v) {
    return v->destination >= 1 && v->destination < SM_DT_DEST_COUNT ? SM_DT_DEST_BLACK[v->destination]
                                                                     : SM_DT_DEST_BLACK[0];
}

// A zone as log2 luminance out of the node, and back (math/Detail.h dt_zone_of): zone V grey, 0 black, X white.
static inline double dt_zone_L(double z, double white, double black) {
    return z <= 5.0 ? (z - 5.0) * black / 5.0 : (z - 5.0) * white / 5.0;
}
static inline double dt_L_zone(double L, double white, double black) {
    return L <= 0.0 ? 5.0 + 5.0 * L / black : 5.0 + 5.0 * L / white;
}

// Zonale, development (tests/model/detail.py zonal_develop): exposure places the shadow on its zone, the slope
// (N+/N-) turns the line about it so the highlight lands on its own. False without a measure.
struct SMZonalDevelop { double Ls, Lso, k, hi, hiD, loD, sh, k2, Lh, Lho, k1; int two; };   // k1: Unito's development slope, 0 without
// The grade of a slope and its inverse: 2.5 is normal, each doubling of the slope is 2.5 grades (hypothesis, to be
// checked on prints: the sources give one grade per doubling of the soft/hard ratio).
static inline double dt_split_grade(double k) { return 2.5 + 2.5 * log2(k); }
static inline double dt_grade_slope(double g) { return exp2((g - 2.5) / 2.5); }

// The curve at L (math/Detail.h dt_zonal_curve, in double): the line, or the lower of two lines, with toe and shoulder.
static inline double dt_zonal_eval(const SMZonalDevelop* d, double L) {
    double y = d->Lso + d->k * (L - d->Ls);
    if (d->two) {
        const double y2 = d->Lho + d->k2 * (L - d->Lh);
        y = 0.5 * (y + y2 - sqrt((y - y2) * (y - y2) + 0.25));
    }
    if (y > d->sh) y = d->sh + d->hiD * (1.0 - exp(-(y - d->sh) / d->hiD));
    else if (y < d->Lso) y = d->Lso - d->loD * (1.0 - exp(-(d->Lso - y) / d->loD));
    return y;
}

static inline bool dt_zonal_develop(const SMDetailControls* v, SMZonalDevelop* d) {
    const double Ls = v->adamsShadowL, Lh = v->adamsHighL, gap = Lh - Ls;
    if (!(gap > SM_DT_MIN_GAP)) return false;
    const double white = dt_destination_white(v), black = dt_destination_black(v);
    double Lso, Lho;
    const bool split = v->devMode != SM_DT_MODE_ZONAL;
    if (split) {   // the print's two exposures: the soft one places the light, the hard one the shadow
        Lso = dt_zone_L(SM_DT_SPLIT_SHADOW_ZONE, white, black) - v->splitHard;
        Lho = fmax(dt_zone_L(SM_DT_SPLIT_HIGH_ZONE, white, black) - v->splitSoft, Lso + SM_DT_MIN_GAP);
    } else {
        const double zs = v->adamsShadowPlace, zh = fmax(v->adamsHighPlace, zs + SM_DT_ADAMS_MIN_SPREAD);
        Lso = dt_zone_L(zs, white, black);
        Lho = dt_zone_L(zh, white, black);
    }
    double Lh2 = Lh;
    d->k1 = 0.0;
    if (v->devMode == SM_DT_MODE_UNITO) {   // the development first, as far as N-/N+ go; the print takes the rest
        d->k1 = sm_clampd((Lho - Lso) / gap, SM_DT_UNITO_K_MIN, SM_DT_UNITO_K_MAX);
        Lh2 = Ls + d->k1 * gap;
    }
    const double gap2 = Lh2 - Ls;
    d->Ls = Ls;
    d->Lso = Lso;
    d->Lh = Lh2;
    d->Lho = Lho;
    d->two = split && v->splitEngine == SM_DT_ENGINE_LAYERS ? 1 : 0;
    if (d->two) {   // each layer is a line of its own grade; the paper takes the lower
        d->k = dt_grade_slope(SM_DT_LAYER_HARD_GRADE);
        d->k2 = dt_grade_slope(SM_DT_LAYER_SOFT_GRADE);
        d->sh = Lho;
    } else {
        d->k = sm_clampd((Lho - Lso) / gap2, SM_DT_ZD_K_MIN, SM_DT_ZD_K_MAX);
        d->k2 = 0.0;
        d->sh = Lso + d->k * gap2;
    }
    d->hiD = fmax(white + SM_DT_ZD_ROOM - d->sh, 0.25);
    d->loD = fmax(Lso + black + SM_DT_ZD_ROOM, 0.25);
    d->hi = dt_zonal_eval(d, Lh2);
    return true;
}


// The zones the shadow and the highlight land on: exact, unless the slope hits its limit.
static inline void dt_zonal_reached(const SMDetailControls* v, double* zs, double* zh) {
    const double white = dt_destination_white(v), black = dt_destination_black(v);
    SMZonalDevelop d;
    if (!dt_zonal_develop(v, &d)) {
        *zs = dt_L_zone(v->adamsShadowL, white, black);
        *zh = dt_L_zone(v->adamsHighL, white, black);
        return;
    }
    *zs = dt_L_zone(d.Lso, white, black);
    *zh = dt_L_zone(d.hi, white, black);
}

// n with 2 H^2 >= base^2 4^n, integers only: the grid does not jump between proxy and full size.
static inline int dt_grid_scale(int H, int base) {
    int n = 0;
    while (2.0 * H * H >= (double)base * base * (double)(1LL << (2 * (n + 1)))) ++n;
    return n;
}

static inline int dt_gauss_half(double sigma, float* half, int cap) {
    int R = (int)ceil(3.0 * sigma);
    if (R > cap - 1) R = cap - 1;
    for (int k = 0; k <= R; ++k) half[k] = (float)exp(-k * k / (2.0 * sigma * sigma));
    return R + 1;
}

// The veil in the input gamut: a neutral seen under a light of 10^6 / (153.8 + 0.8 v) K, norm 1.
static inline void dt_veil(double warmth, int space, double out[3]) {
    double w[3], w0[3], ls[3], l0[3], lms[3], d65[3] = { 0.95046, 1.0, 1.08906 }, xyz[3];
    sm_white_xyz(1e6 / (153.8 + 0.8 * warmth), 0.0, w);
    sm_white_xyz(1e6 / 153.8, 0.0, w0);
    sm_bradford(w, ls, 0);
    sm_bradford(w0, l0, 0);
    sm_bradford(d65, lms, 0);
    for (int i = 0; i < 3; ++i) lms[i] *= ls[i] / l0[i];
    sm_bradford(lms, xyz, 1);
    const float* m = SM_XYZ_TO_RGB + space * 9;
    for (int i = 0; i < 3; ++i) {
        const double c = m[i * 3] * xyz[0] + m[i * 3 + 1] * xyz[1] + m[i * 3 + 2] * xyz[2];
        out[i] = c > 0.02 ? c : 0.02;
    }
    const double mx = fmax(out[0], fmax(out[1], out[2]));
    double x = out[0] / mx, y = out[1] / mx, z = out[2] / mx;
    double n = mx * (x * x * x + y * y * y + z * z * z) / (x * x + y * y + z * z);
    if (n < 1e-3) n = 1e-3;
    for (int i = 0; i < 3; ++i) out[i] /= n;
}

// href: the height the radii are relative to (the frame height; tests pass another to scale a small image).
// Auto zone: the shadow and the highlight as the 2nd and 98th percentile of the picture's L (nearest rank), in stops.
// L is sorted in place; false for too few samples.
static inline bool dt_auto_zone(std::vector<float>& L, double* Ls, double* Lh) {
    if (L.size() < 16) return false;
    std::sort(L.begin(), L.end());
    const double n1 = (double)(L.size() - 1);
    *Ls = L[(size_t)floor(SM_DT_AUTO_LOW * n1 + 0.5)];
    *Lh = L[(size_t)floor(SM_DT_AUTO_HIGH * n1 + 0.5)];
    return true;
}

// Unito's first stage: the development, a bare line about the shadow (the shadow stays where it was read).
static inline SMZdCurve dt_zd_first(const SMZonalDevelop* d) {
    SMZdCurve c = SMZdCurve();
    c.Ls = c.Lso = (float)d->Ls;
    c.K = (float)d->k1;
    c.kind = 2;
    return c;
}

static inline SMZdCurve dt_zd_curve(const SMZonalDevelop* d, int mark) {
    SMZdCurve c = SMZdCurve();
    c.Ls = (float)d->Ls; c.Lso = (float)d->Lso; c.K = (float)d->k; c.Hi = (float)d->sh;
    c.HiD = (float)d->hiD; c.LoD = (float)d->loD;
    c.K2 = (float)d->k2; c.Lh = (float)d->Lh; c.Lho = (float)d->Lho;
    c.kind = d->two;
    c.mark = mark;
    return c;
}

// The test print (provino): a grid of cells, each the picture developed with its own exposures. Strips: seven steps of
// the soft exposure (top row) and of the hard one (bottom); matrix: soft across, hard down; unions: the two engines
// side by side. The cell in the middle (or the engine in use) is the current one.
static inline void dt_prepare_provino(DetailParams* p, int W, int H, const SMDetailControls* v) {
    const int kind = v->provinoKind;
    const int cols = kind == SM_DT_PV_STRIPS ? 7 : (kind == SM_DT_PV_MATRIX ? 5 : 2);
    const int rows = kind == SM_DT_PV_STRIPS ? 2 : (kind == SM_DT_PV_MATRIX ? 5 : 1);
    p->pvOn = 1;
    p->pvCols = cols;
    p->pvRows = rows;
    p->pvBw = H / 270 > 1 ? H / 270 : 1;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            SMDetailControls cv = *v;
            cv.devMode = SM_DT_MODE_SPLIT;
            int mark = 0;
            if (kind == SM_DT_PV_STRIPS) {
                const double s = (c - cols / 2) * v->provinoStep;
                (r == 0 ? cv.splitSoft : cv.splitHard) += s;
                mark = c == cols / 2;
            } else if (kind == SM_DT_PV_MATRIX) {
                cv.splitSoft += (c - 2) * v->provinoStep;
                cv.splitHard += (r - 2) * v->provinoStep;
                mark = c == 2 && r == 2;
            } else {
                cv.splitEngine = c;
                mark = c == v->splitEngine;
            }
            SMZonalDevelop d;
            if (dt_zonal_develop(&cv, &d)) p->pv[r * cols + c] = dt_zd_curve(&d, mark);
        }
    (void)W;
}

static inline void dt_prepare(DetailParams* p, int W, int H, double href, double par, int space, int gamma,
                              const SMDetailControls* in, int base) {
    SMDetailControls v = *in;
    v.localContrast = sm_clampd(v.localContrast, -100, 50);
    v.localHighlights = sm_clampd(v.localHighlights, -100, 100);
    v.localShadows = sm_clampd(v.localShadows, -100, 100);
    v.texture = sm_clampd(v.texture, -100, 100);
    v.clarity = sm_clampd(v.clarity, -100, 100);
    v.dehaze = sm_clampd(v.dehaze, -100, 100);
    v.preserveDetail = sm_clampd(v.preserveDetail, 0, 100);
    v.detailRadius = sm_clampd(v.detailRadius, 1, 8);
    v.edgeThreshold = sm_clampd(v.edgeThreshold, 0.25, 1.0);
    v.noiseThreshold = sm_clampd(v.noiseThreshold, 0.0, 0.15);
    v.clarityCenter = sm_clampd(v.clarityCenter, -3, 3);
    v.hazeLevel = sm_clampd(v.hazeLevel, -2, 6);
    v.hazeWarmth = sm_clampd(v.hazeWarmth, -100, 100);
    v.localWhite = sm_clampd(v.localWhite, 1, 10);
    v.devMode = v.devMode < 0 ? 0 : (v.devMode > SM_DT_MODE_UNITO ? SM_DT_MODE_UNITO : v.devMode);
    v.splitSoft = sm_clampd(v.splitSoft, -SM_DT_SPLIT_RANGE, SM_DT_SPLIT_RANGE);
    v.splitHard = sm_clampd(v.splitHard, -SM_DT_SPLIT_RANGE, SM_DT_SPLIT_RANGE);
    v.splitEngine = v.splitEngine == SM_DT_ENGINE_LAYERS ? SM_DT_ENGINE_LAYERS : SM_DT_ENGINE_CURVE;
    v.provinoKind = v.provinoKind < 0 ? 0 : (v.provinoKind > SM_DT_PV_UNIONS ? SM_DT_PV_UNIONS : v.provinoKind);
    v.provinoStep = sm_clampd(v.provinoStep, 0.1, 1.5);
    v.destination = v.destination < 0 ? 0 : (v.destination >= SM_DT_DEST_COUNT ? SM_DT_DEST_COUNT - 1 : v.destination);
    v.viewZones = v.viewZones < 0 ? 0 : (v.viewZones > 2 ? 2 : v.viewZones);
    v.adamsShadowL = sm_clampd(v.adamsShadowL, -16, 16);
    v.adamsHighL = sm_clampd(v.adamsHighL, -16, 16);
    v.adamsShadowPlace = sm_clampd(v.adamsShadowPlace, 0, 10);
    v.adamsHighPlace = sm_clampd(v.adamsHighPlace, 0, 10);
    v.zonalPhase = v.zonalPhase < 0 ? 0 : (v.zonalPhase > SM_DT_PHASE_PRESENCE ? SM_DT_PHASE_PRESENCE : v.zonalPhase);
    v.printEdgeBurn = sm_clampd(v.printEdgeBurn, 0, 2);
    v.printEdgeSize = sm_clampd(v.printEdgeSize, 10, 60);
    // Zonale: each phase runs its own part; in one node (all) every part runs
    const int phase = v.zonalPhase;   // Zonale and Split grade share the three phases and the development curve
    const bool develop = phase == SM_DT_PHASE_ALL || phase == SM_DT_PHASE_DEVELOP;
    const bool printing = phase == SM_DT_PHASE_ALL || phase == SM_DT_PHASE_PRINT;
    const bool presence = phase == SM_DT_PHASE_ALL || phase == SM_DT_PHASE_PRESENCE;
    if (!presence) v.texture = v.clarity = v.dehaze = 0.0;
    if (!printing) v.localContrast = v.localShadows = v.localHighlights = 0.0;
    const double radius = v.detailRadius;

    *p = DetailParams();
    p->W = W;
    p->H = H;
    p->space = space;
    p->gamma = gamma;
    const int n = dt_grid_scale(H, base);
    p->s = 1 << n;
    p->w = (W + p->s - 1) / p->s;
    p->h = (H + p->s - 1) / p->s;
    const double gh = href / p->s;
    const double sigma1 = fmax(0.5, 0.0005 * href), sigma2 = 0.004 * href;
    int nt = sigma2 > 0.0 ? (int)floor(log2(sigma2 / 4.0)) : 0;
    nt = nt < 0 ? 0 : (nt > n ? n : nt);
    p->st = 1 << nt;
    p->wt = (W + p->st - 1) / p->st;
    p->ht = (H + p->st - 1) / p->st;
    const double tentVar = p->st >= 2 ? (2.0 * p->st * p->st + 1.0) / 12.0 : 0.0;
    const double sigmaT = sqrt(fmax(sigma2 * sigma2 - tentVar, 0.25)) / p->st;
    const double radii[5] = { radius / 100.0 * gh, 0.005 * gh, 0.025 * gh, 0.005 * gh, 0.02 * gh };
    float* const rx[5] = { &p->rBx, &p->r2x, &p->r3x, &p->rpx, &p->rtx };
    float* const ry[5] = { &p->rBy, &p->r2y, &p->r3y, &p->rpy, &p->rty };
    for (int i = 0; i < 5; ++i) { *rx[i] = (float)(radii[i] / par); *ry[i] = (float)radii[i]; }
    p->eps = (float)((v.edgeThreshold / 2.0) * (v.edgeThreshold / 2.0));
    p->epsT = 0.01f;
    p->nG = dt_gauss_half(1.0, p->gG, SM_DT_GG);
    p->n1 = dt_gauss_half(sigma1, p->g1, SM_DT_G1);
    p->nT = dt_gauss_half(sigmaT, p->gT, SM_DT_GT);

    double nominal[SM_T5_SLOTS][4] = {};
    for (int i = 0; i < SM_T5_SLOTS; ++i) { nominal[i][2] = 1.0; nominal[i][3] = SM_T5_SMAX_CR; }
    const double sh[4] = { 0.02 * v.localShadows, 0.0, 1.5, SM_T5_SMAX_CR };   // the print has three local tones
    for (int k = 0; k < 4; ++k) nominal[5][k] = sh[k];
    sm_zone_policy(exp2(v.localContrast / 100.0) - 1.0, 0.0, nominal, &p->curve);
    SMZonalDevelop zd;
    if (develop && dt_zonal_develop(&v, &zd)) {
        p->zdOn = 1;
        p->zd = dt_zd_curve(&zd, 0);
        if (zd.k1 != 0.0) {
            p->zd1On = 1;
            p->zd1 = dt_zd_first(&zd);
        }
        if (v.viewProvino && v.devMode == SM_DT_MODE_SPLIT) dt_prepare_provino(p, W, H, &v);
    }
    p->ebAmt = printing ? (float)v.printEdgeBurn : 0.0f;
    p->ebS = (float)(v.printEdgeSize / 100.0);
    p->ebR0 = 1.0f - p->ebS;
    int slots = 0;
    for (int i = 0; i < SM_T5_SLOTS; ++i) slots |= p->curve.zoneE[i] != 0.0f;
    p->toneOn = (p->curve.conC != 0.0f || slots) ? 1 : 0;
    // Local Highlights: the main node's shoulder, landing the recorded top on the node's own Bianco at -100
    double amount = fabs(v.localHighlights) / 100.0;
    amount *= 1.5 - 0.5 * amount;
    if (v.localHighlights > 0.0) {
        p->hlE = (float)(amount * SM_T5_HL_CPLUS);
    } else if (v.localHighlights < 0.0) {
        const double white = dt_destination_white(&v);
        const double top = SM_T5_HL_TOP, land = fmin(white, top - SM_T5_HL_MINPULL);
        const double D = sm_shoulder_dd(top - SM_DT_HL_START);
        const double k = D > 0.0 ? fmin(fmax(top - land, 0.0) / D, 1.0 - SM_T5_HL_SMIN) : 0.0;
        p->hlE = (float)(-amount * k);
        p->hlAmount = (float)amount;
        p->hlWhiteLin = (float)(SM_T5_GREY * exp2(fmax(white, top - k * D)));
    }
    p->hlTop = (float)SM_T5_HL_TOP;
    p->hlStart = (float)SM_DT_HL_START;
    // Soglia bordi sets what counts as detail: lower, fewer halos and less texture kept
    p->hlT1 = (float)(0.7 * v.edgeThreshold);
    p->hlT2 = (float)(0.6 * v.edgeThreshold);
    p->hlB1 = (float)(0.3 * v.edgeThreshold);
    p->hlB2 = (float)(0.2 * v.edgeThreshold);
    p->lam = (float)(v.preserveDetail / 100.0);
    p->tau0 = (float)v.noiseThreshold;
    p->clarity = (float)(v.clarity / 100.0);
    p->clarityCenter = (float)v.clarityCenter;
    p->texture = (float)(v.texture / 100.0);

    p->hazeOn = v.dehaze != 0.0 ? 1 : 0;
    p->hazeOmega = v.dehaze > 0.0 ? (float)(0.006 * v.dehaze) : 0.0f;
    p->hazeMix = v.dehaze < 0.0 ? (float)(0.5 * (v.dehaze / 100.0) * (v.dehaze / 100.0)) : 0.0f;
    p->hazeLevel = (float)v.hazeLevel;
    double veil[3];
    dt_veil(v.hazeWarmth, space, veil);
    for (int c = 0; c < 3; ++c) {
        const double a = SM_T5_GREY * exp2(v.hazeLevel) * veil[c];
        p->hazeA[c] = (float)a;
        p->hazeInvA[c] = (float)(1.0 / a);
    }
    p->view = v.viewGain ? 1 : (v.viewBase ? 2 : (v.viewZones ? 2 + v.viewZones : 0));
    p->zoneWhite = (float)dt_destination_white(&v);
    p->zoneBlack = (float)dt_destination_black(&v);
    p->pzOn = presence ? 1 : 0;
    if (p->view >= 3 && H >= SM_DT_LG_MIN_H) {   // the legend: sizes follow the frame, ticks once measured
        const int hb = H / 20 > 6 ? H / 20 : 6, m = W * 3 / 100 > 2 ? W * 3 / 100 : 2, span = W - 2 * m;
        p->lgOn = 1;
        p->lgM = m;
        p->lgY0 = hb / 2;
        p->lgY1 = p->lgY0 + hb;
        p->lgSc = hb * 2 / 15 > 1 ? hb * 2 / 15 : 1;
        p->lgTk = p->zdOn;
        p->lgTh = p->lgTk ? hb * 2 / 3 : 0;
        p->lgTw = hb / 12 > 1 ? hb / 12 : 1;
        p->lgTop = p->lgY1 + p->lgTh + hb / 4;
        p->lgX0 = m - hb / 4;
        p->lgX1 = W - m + hb / 4;
        const double white = p->zoneWhite, black = p->zoneBlack;
        const double z[4] = { dt_L_zone(p->zd.Ls, white, black), dt_L_zone(v.adamsHighL, white, black),
                              dt_L_zone(p->zd.Lso, white, black), dt_L_zone(zd.hi, white, black) };
        int* tick[4] = { &p->lgIn[0], &p->lgIn[1], &p->lgOut[0], &p->lgOut[1] };
        for (int k = 0; k < 4; ++k)
            *tick[k] = m + (int)floor(sm_clampd(z[k] + 0.5, 0.0, 11.0) * span / 11.0);
    }

    const float* a = SM_XYZ_TO_RGB + SM_T5_REF * 9;
    const float* b = SM_RGB_TO_XYZ + space * 9;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            p->toRef[i * 3 + j] = (float)((double)a[i * 3] * b[j] + (double)a[i * 3 + 1] * b[3 + j]
                                          + (double)a[i * 3 + 2] * b[6 + j]);
}
#endif

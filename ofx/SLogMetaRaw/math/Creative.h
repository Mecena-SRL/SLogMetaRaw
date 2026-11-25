// SPDX-License-Identifier: GPL-3.0-or-later
// Per-pixel maths of the Creative node, shared by its CPU passes (src/creative/CreativeEngine.cpp) and its Metal
// kernels (metal/kernels/Creative.metal). Parametric models of our own: no profile, LUT or stock data of any product.
// The physics and the numbers: docs/CREATIVE.md and the research it cites.
//
// One engine for every photographic process (kind CR_KIND_PROCESS):
//   record    up to three records (layers, plates, strips): spectral weights over linear Rec.2020, each read at its
//             own place (registration, aberration, weave) and time (sequential plates), exposure and flash
//   glow      halation (light back from the base into the records) and optical diffusion, on reduced grids
//   curve     per record, in stops from grey: H&D slope and offset (development), toe and shoulder, interlayer,
//             grain, damage, printer lights, the positive (print or reversal) curve
//   image     subtractive (dye densities through an absorption matrix, key image, silver, fading, stain),
//             additive (filter colours through a mosaic or réseau) or monochrome (a toned deposit on its paper)
// and two signal chains: analog video (CR_KIND_ANALOG) and digital cameras (CR_KIND_DIGITAL).
#pragma once
#include "Common.h"
#include "Transfer.h"
#include "Gamut.h"
#include "Tone.h"

#ifdef __METAL_VERSION__
#define CR_PLANE const device float*
#define CR_OUT device float*
#define CR_FLOOR floor
#define CR_CEIL ceil
#define CR_SIN precise::sin
#define CR_COS precise::cos
typedef uint CrU;
#else
#define CR_PLANE const float*
#define CR_OUT float*
#define CR_FLOOR floorf
#define CR_CEIL ceilf
#define CR_SIN sinf
#define CR_COS cosf
typedef unsigned int CrU;
#endif

#define CR_KIND_PROCESS 0
#define CR_KIND_ANALOG 1
#define CR_KIND_DIGITAL 2
#define CR_IMAGE_SUB 0
#define CR_IMAGE_ADD 1
#define CR_IMAGE_MONO 2
#define CR_MOSAIC_NONE 0
#define CR_MOSAIC_GRAINS 1   // dyed starch grains, clumped (Autochrome)
#define CR_MOSAIC_RESEAU 2   // ruled lines and squares (Dufaycolor)
#define CR_OUT_REC709 0
#define CR_OUT_LOG 1
#define CR_GAMMA24 3         // gamma code of Gamma 2.4 (ColourSpaces.cpp)
#define CR_MEDIUM_NONE 0
#define CR_MEDIUM_SCREEN 1   // light on a screen: projection (lamp, lens) or a tube (beam, phosphors, curved glass)
#define CR_MEDIUM_LCD 2      // a small reflective LCD of few pixels and four shades
#define CR_MEDIUM_PAPER 3    // a print on paper under a lamp

SM_CONST float CR_GREY = 0.18f;
SM_CONST float CR_SIGMA = 1.0f / 0.115f - 1.0f;   // the positive curves' half point: grey lands on display 0.115
SM_CONST float CR_EPS = 1e-6f;
SM_CONST float CR_LN10_2 = 3.3219281f;          // 10^-D = 2^(-D * log2 10)

// Everything a frame needs, filled on the host (src/creative/CreativeModel.cpp). Plain ints and floats: the same
// layout in C++ and Metal.
struct CreativeParams {
    int W, H, rowPixels, dstRowPixels;
    int space, gamma, frame, output, kind, image, records;
    float toRef[9], fromRef[9], toOut[9];   // input gamut -> Rec.2020, Rec.2020 -> input gamut, Rec.2020 -> Rec.709
    // record: where (px offset, radial scale), what (weights), how much
    float weaveX, weaveY, jitter, headLines, headShift;
    float recX[3], recY[3], recScale[3];
    int moved, flipX, shared;              // shared: the three records read one frame at one place
    float recW[9];                          // row k: weights of record k over linear Rec.2020
    float exposure, recGain[3], uv, flash, vignette;
    // glow
    int hS, hW, hH, bS, bW, bH;
    float halAmount, halThreshold, halTint[3], bloom, halSigma, bloomSigma;
    // curve, in stops from grey
    float slope[3], offset[3], shoulder, toe, interlayer;
    float grainAmount, grainSize, grainColor, grainLayer[3];
    float dust, scratches, dustSign;
    float printOffset[3], printContrast[3], dmax;
    // colour (stops)
    float splitShadow[3], splitHigh[3], splitBalance, saturation, density;
    // image: subtractive
    float dye[9], keyImage, silver, fade[3], stain[3];
    // image: additive
    float filt[9], area[3], mosaicScale, clump, mosaicTexture, viewLight;
    int mosaic;
    // image: monochrome
    float tone[3], paper[3], mirror, mirrorTint[3], oxidation, border, borderTint[3];
    // analog and digital signal chains
    float lumaSigma, ringSigma, chromaSigma, sharp, noise, chromaNoise, dropouts;
    int chromaDelay, lumaAxes, chromaBlockX, chromaBlockY;
    float clip, knee, contrast, highDesat, levels;
    // analog tape, emulated line by line (tape != 0): field raster, record and playback filters (one-pole k, biquads
    // b0 b1 b2 a1 a2), FM noise, colour-under, timebase residual after the TV's line lock (px), dropouts
    int tape, fieldLines, combRows, docRows;
    float emphK, emphG, clipLo, clipHi, fmNoise, lp[10], crispK, crisp;
    float chromaK, chromaSigmaN, chromaAM, chromaLimit, delayPx, hueGain, huePhase;
    float tbWave, tbWaveLines, tbScrape, headStep, headRows, headTau, flagAmp, flagRows, headNoise;
    float dropDensity, dropSlide, dropLife;
    // the video camera's sensor (sensor != 0), on the records before the camera's curve: a pickup tube's charge
    // carried across fields (lag, comet tails) from the history (up to 3 previous frames), a CCD's vertical smear
    // (column sums), a MOS's fixed pattern; a striped tube's or a mosaic's false colour, along the line (cr_tape_row)
    int sensor, history;
    float tubeK[3], tubeB[3], tubeGamma, tubeBias, tubeQmax, tubeGreen;
    float smear, smearWell, smearMono, fpn, falseColour, fcPeriod;
    // digital cameras, emulated (dig != 0): sensor size dW x dH (dAlias: one sample per pixel), colour planes
    // cW x cH, the ISP's unsharp mask, the codec's quantiser scale (0: no DCT)
    int dig, dW, dH, cW, cH, dAlias;
    float dSharp, dQ;
    // duplicates and print: grain of the intermediates (stops), of the print (density)
    float genGrain, genSize, printGrain, printGrainSize, genSoft;   // genSoft: the duplicates' blur (px)
    // the lens (lens != 0), on the records: barrel distortion per axis (at the corners, which stay), colour of the
    // glass, edge softness and swirl (tangential blur, px at the corner), veiling glare (share of the local mean
    // luminance, its grid), anamorphic streak (highlights above lStreakThr spread along the line, decay length px)
    int lens, lgS, lgW, lgH;
    float lDistX, lDistY, lTint[3], lSoft, lSwirl, lGlare, lGlareTint[3], lgSigma, lStreak, lStreakTint[3],
        lStreakLen, lStreakThr;
    // viewing medium, on the finished picture (display-linear Rec.709)
    int medium, mLines, mMaskType, mCols, mRows;
    float mCurve, mBeamMin, mBeamMax, mMask, mPitch, mFalloff, mGain, mBlack, mWhite[3], mShiftX, mShiftY, mSoft;
    float mTone, mCutoff, mSat, mChroma, mZoom;   // device response (cr_device), overscan
    float mCorner, mEdge, mTexture, mScreenW, mScreenH, mBezel, mDither, mGrid;
    float mPalette[12], mUnlit[3], mBezelTint[3], mBodyTint[3];   // LCD: code values (Gamma 2.4)
    int mGS, mGW, mGH;                     // the glow's grid (scale, size) of the picture's luminance
    float mGlow, mGlowSigma, mGhost;       // mGhost: share of the previous picture an LCD still shows
};

SM_FN SMf3 cr_mul(SM_CPTR m, SMf3 c) {
    return smf3(m[0] * c.x + m[1] * c.y + m[2] * c.z, m[3] * c.x + m[4] * c.y + m[5] * c.z,
                m[6] * c.x + m[7] * c.y + m[8] * c.z);
}
SM_FN float cr_at(SMf3 v, int k) { return k == 0 ? v.x : (k == 1 ? v.y : v.z); }
SM_FN int cr_clamp_i(int i, int n) { return i < 0 ? 0 : (i >= n ? n - 1 : i); }
SM_FN int cr_max_i(int a, int b) { return a > b ? a : b; }   // SM_MAX is fmax: ambiguous on ints in Metal
SM_FN int cr_min_i(int a, int b) { return a < b ? a : b; }
SM_FN float cr_luma(SMf3 c) { return 0.2627f * c.x + 0.6780f * c.y + 0.0593f * c.z; }   // Rec.2020 Y
SM_FN float cr_smooth(float a, float b, float x) {
    const float t = sm_clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
SM_FN float cr_softplus(float x) { return x > 20.0f ? x : SM_LOG(1.0f + SM_EXP(x)); }
SM_FN float cr_stops(float x) { return SM_LOG2(SM_MAX(x, CR_EPS) / CR_GREY); }
SM_FN float cr_pow10m(float D) { return SM_EXP2(-CR_LN10_2 * D); }   // 10^-D

// Integer hash -> [-1, 1): the same bits on the CPU and in Metal.
SM_FN float cr_hash(int x, int y, int z) {
    CrU h = (CrU)x * 73856093u ^ (CrU)y * 19349663u ^ (CrU)z * 83492791u;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return (float)(h & 0xffffffu) * (2.0f / 16777216.0f) - 1.0f;
}
SM_FN float cr_hash01(int x, int y, int z) { return 0.5f + 0.5f * cr_hash(x, y, z); }

// Smooth value noise at (u, v) cells, layer z.
SM_FN float cr_value(float u, float v, int z) {
    const float fu = CR_FLOOR(u), fv = CR_FLOOR(v);
    const int i = (int)fu, j = (int)fv;
    float a = u - fu, b = v - fv;
    a = a * a * (3.0f - 2.0f * a);
    b = b * b * (3.0f - 2.0f * b);
    const float n00 = cr_hash(i, j, z), n10 = cr_hash(i + 1, j, z);
    const float n01 = cr_hash(i, j + 1, z), n11 = cr_hash(i + 1, j + 1, z);
    const float top = n00 + (n10 - n00) * a, bot = n01 + (n11 - n01) * a;
    return top + (bot - top) * b;
}
// Two octaves, about unit variance.
SM_FN float cr_noise(float u, float v, int z) {
    return 2.3f * (cr_value(u, v, z) + 0.5f * cr_value(2.0f * u + 17.3f, 2.0f * v + 5.1f, z + 101));
}

// Bilinear read of a reduced grid (scale s) at the full-size pixel (x, y).
SM_FN float cr_grid(CR_PLANE g, int w, int h, int s, int x, int y) {
    const float u = sm_clamp(((float)x + 0.5f) / (float)s - 0.5f, 0.0f, (float)(w - 1));
    const float v = sm_clamp(((float)y + 0.5f) / (float)s - 0.5f, 0.0f, (float)(h - 1));
    const int i0 = (int)u, j0 = (int)v;
    const int i1 = i0 + 1 < w ? i0 + 1 : w - 1, j1 = j0 + 1 < h ? j0 + 1 : h - 1;
    const float fx = u - (float)i0, fy = v - (float)j0;
    const float top = g[j0 * w + i0] + (g[j0 * w + i1] - g[j0 * w + i0]) * fx;
    const float bot = g[j1 * w + i0] + (g[j1 * w + i1] - g[j1 * w + i0]) * fx;
    return top + (bot - top) * fy;
}

// ---- record

// Horizontal displacement of line y: timebase jitter (analog) and the head-switching shift at the bottom (row 0).
SM_FN float cr_line_shift(int y, SM_CREF(CreativeParams) p) {
    const float head = (float)y < p.headLines ? p.headShift * (1.0f - (float)y / p.headLines) : 0.0f;
    if (p.jitter <= 0.0f) return head;
    const float fy = 1.0f - (float)y / (float)p.H;
    const float wave = CR_SIN(fy * 9.0f + (float)p.frame * 0.7f) * 0.6f + cr_hash(y, p.frame, 7) * 0.4f;
    return head + p.jitter * wave * (1.0f + 2.0f * cr_smooth(0.85f, 1.0f, fy));
}

// The source (RGBA rows of rowPixels) at the fractional pixel (u, v), bilinear on the code values, as linear Rec.2020.
SM_FN SMf3 cr_tap(CR_PLANE src, float u, float v, SM_CREF(CreativeParams) p) {
    u = sm_clamp(u, 0.0f, (float)(p.W - 1));
    v = sm_clamp(v, 0.0f, (float)(p.H - 1));
    const int i0 = (int)u, j0 = (int)v;
    const int i1 = i0 + 1 < p.W ? i0 + 1 : p.W - 1, j1 = j0 + 1 < p.H ? j0 + 1 : p.H - 1;
    const float fx = u - (float)i0, fy = v - (float)j0;
    const int a = (j0 * p.rowPixels + i0) * 4, b = (j0 * p.rowPixels + i1) * 4;
    const int d = (j1 * p.rowPixels + i0) * 4, e = (j1 * p.rowPixels + i1) * 4;
    float o[3];
    for (int c = 0; c < 3; ++c) {
        const float top = src[a + c] + (src[b + c] - src[a + c]) * fx, bot = src[d + c] + (src[e + c] - src[d + c]) * fx;
        o[c] = sm_decode(top + (bot - top) * fy, p.gamma);
    }
    return cr_mul(p.toRef, smf3(o[0], o[1], o[2]));
}

// Linear Rec.2020 seen by record k at pixel (x, y), from its own frame src.
SM_FN SMf3 cr_source(CR_PLANE src, int k, int x, int y, SM_CREF(CreativeParams) p) {
    if (p.moved != 0) {
        float u = (float)x + p.weaveX + cr_line_shift(y, p), v = (float)y + p.weaveY;
        if (p.flipX != 0) u = (float)(p.W - 1) - u;
        const float cx = 0.5f * (float)(p.W - 1), cy = 0.5f * (float)(p.H - 1);
        float du = u - cx, dv = v - cy;
        if (p.lDistX != 0.0f || p.lDistY != 0.0f) {   // barrel: the corners stay, the middle grows
            const float rr = (du * du + dv * dv) / (cx * cx + cy * cy);
            du *= (1.0f + p.lDistX * rr) / (1.0f + p.lDistX);
            dv *= (1.0f + p.lDistY * rr) / (1.0f + p.lDistY);
        }
        return cr_tap(src, cx + du * p.recScale[k] + p.recX[k], cy + dv * p.recScale[k] + p.recY[k], p);
    }
    const int i = (y * p.rowPixels + x) * 4;
    return cr_mul(p.toRef, smf3(sm_decode(src[i], p.gamma), sm_decode(src[i + 1], p.gamma), sm_decode(src[i + 2], p.gamma)));
}

// Exposure of record k from the light c it sees (scene-linear, grey 0.18 at normal exposure).
SM_FN float cr_expose(SMf3 c, int k, int x, int y, SM_CREF(CreativeParams) p) {
    float e = p.recW[k * 3] * c.x + p.recW[k * 3 + 1] * c.y + p.recW[k * 3 + 2] * c.z;
    if (p.uv > 0.0f) e += p.uv * SM_MAX(c.z - 0.5f * c.y, 0.0f);   // blue-sensitive emulsions see the sky's UV
    e = e * p.exposure * p.recGain[k] + p.flash * CR_GREY;
    if (p.vignette != 0.0f) {
        const float dx = ((float)x + 0.5f) / (float)p.W - 0.5f, dy = ((float)y + 0.5f) / (float)p.H - 0.5f;
        e *= SM_EXP2(-p.vignette * cr_smooth(0.25f, 1.1f, SM_SQRT(dx * dx + dy * dy) * 1.41421356f));
    }
    return e;
}

// The lens glass's colour on the light it lets through.
SM_FN SMf3 cr_glass(SMf3 c, SM_CREF(CreativeParams) p) {
    return smf3(c.x * p.lTint[0], c.y * p.lTint[1], c.z * p.lTint[2]);
}

// The three records at (x, y). When they share frame and place (p.shared), the source is read and decoded once.
SM_FN SMf3 cr_records(CR_PLANE s0, CR_PLANE s1, CR_PLANE s2, int x, int y, SM_CREF(CreativeParams) p) {
    const SMf3 c0 = cr_glass(cr_source(s0, 0, x, y, p), p);
    const SMf3 c1 = p.shared != 0 ? c0 : cr_glass(cr_source(s1, 1, x, y, p), p);
    const SMf3 c2 = p.shared != 0 ? c0 : cr_glass(cr_source(s2, 2, x, y, p), p);
    return smf3(cr_expose(c0, 0, x, y, p), cr_expose(c1, 1, x, y, p), cr_expose(c2, 2, x, y, p));
}

// ---- the lens (research DIGITALI_OTTICHE.md, B.14-B.15)

// Plane P (W x H) at the fractional pixel (u, v), bilinear, clamped to the frame.
SM_FN float cr_bilinear(CR_PLANE P, float u, float v, SM_CREF(CreativeParams) p) {
    u = sm_clamp(u, 0.0f, (float)(p.W - 1));
    v = sm_clamp(v, 0.0f, (float)(p.H - 1));
    const int i0 = (int)u, j0 = (int)v;
    const int i1 = i0 + 1 < p.W ? i0 + 1 : p.W - 1, j1 = j0 + 1 < p.H ? j0 + 1 : p.H - 1;
    const float fx = u - (float)i0, fy = v - (float)j0;
    const float top = P[j0 * p.W + i0] + (P[j0 * p.W + i1] - P[j0 * p.W + i0]) * fx;
    const float bot = P[j1 * p.W + i0] + (P[j1 * p.W + i1] - P[j1 * p.W + i0]) * fx;
    return top + (bot - top) * fy;
}

// The records at (x, y) through an old lens's edges: softer towards the corners (field curvature, astigmatism), the
// blur stretched along the circle around the centre (the swirl of a Petzval or a Helios). 16 taps on a golden-angle
// spiral inside an ellipse: tangential half-axis a, radial b.
SM_FN SMf3 cr_lens_soft(CR_PLANE E0, CR_PLANE E1, CR_PLANE E2, int x, int y, SM_CREF(CreativeParams) p) {
    const int k = y * p.W + x;
    const float cx = 0.5f * (float)(p.W - 1), cy = 0.5f * (float)(p.H - 1);
    const float dx = (float)x - cx, dy = (float)y - cy, len = SM_SQRT(dx * dx + dy * dy);
    const float w = cr_smooth(0.2f, 1.0f, len / SM_SQRT(cx * cx + cy * cy));
    const float sig = (p.lSoft * w * w + p.lSwirl * w) * (float)p.H * 0.003f;
    if (sig < 0.3f) return smf3(E0[k], E1[k], E2[k]);
    const float rx = len > 0.0f ? dx / len : 1.0f, ry = len > 0.0f ? dy / len : 0.0f;
    const float a = sig * (1.0f + 2.0f * p.lSwirl * w), b = sig * SM_MAX(1.0f - 0.6f * p.lSwirl, 0.3f);
    float o0 = 0.0f, o1 = 0.0f, o2 = 0.0f;
    for (int i = 0; i < 16; ++i) {
        const float rad = SM_SQRT(((float)i + 0.5f) / 16.0f), ang = (float)i * 2.39996323f;
        const float t = CR_COS(ang) * rad * a, r = CR_SIN(ang) * rad * b;
        const float u = (float)x - ry * t + rx * r, v = (float)y + rx * t + ry * r;
        o0 += cr_bilinear(E0, u, v, p);
        o1 += cr_bilinear(E1, u, v, p);
        o2 += cr_bilinear(E2, u, v, p);
    }
    return smf3(o0 / 16.0f, o1 / 16.0f, o2 / 16.0f);
}

// What a highlight puts into an anamorphic streak: its luminance above the threshold.
SM_FN float cr_streak_source(CR_PLANE E0, CR_PLANE E1, CR_PLANE E2, int k, SM_CREF(CreativeParams) p) {
    return SM_MAX(0.2126f * E0[k] + 0.7152f * E1[k] + 0.0722f * E2[k] - p.lStreakThr, 0.0f);
}

// Line y's streak (one thread per line): each highlight decays exponentially both ways along the line.
SM_FN void cr_streak_row(CR_PLANE E0, CR_PLANE E1, CR_PLANE E2, CR_OUT St, int y, SM_CREF(CreativeParams) p) {
    const float d = SM_EXP(-1.0f / SM_MAX(p.lStreakLen, 1.0f));
    const int row = y * p.W;
    float acc = 0.0f;
    for (int x = 0; x < p.W; ++x) {
        acc = acc * d + cr_streak_source(E0, E1, E2, row + x, p);
        St[row + x] = acc;
    }
    acc = 0.0f;
    for (int x = p.W - 1; x >= 0; --x) {
        const float s = cr_streak_source(E0, E1, E2, row + x, p);
        acc = acc * d + s;
        St[row + x] += acc - s;
    }
}

// The records with the light the lens scatters: the veil (glare) from the local mean and the streak.
SM_FN SMf3 cr_lens_add(SMf3 e, CR_PLANE gl, CR_PLANE St, int x, int y, SM_CREF(CreativeParams) p) {
    const float g = p.lGlare > 0.0f ? p.lGlare * cr_grid(gl, p.lgW, p.lgH, p.lgS, x, y) : 0.0f;
    const float s = p.lStreak > 0.0f ? p.lStreak * St[y * p.W + x] : 0.0f;
    return smf3(e.x + g * p.lGlareTint[0] + s * p.lStreakTint[0], e.y + g * p.lGlareTint[1] + s * p.lStreakTint[1],
                e.z + g * p.lGlareTint[2] + s * p.lStreakTint[2]);
}

// ---- the video camera's sensor (research VIDEO_EMULAZIONE.md, 1.1-1.2)

SM_CONST int CR_SENSOR_TUBE = 1;
SM_CONST int CR_SENSOR_CCD = 2;
SM_CONST int CR_SENSOR_MOS = 3;

// One field of a pickup tube's element (Philips 1963, in units of the peak white's charge per field): the light L
// discharges it, the beam (capacity B) lands at full current down to the cathode, then with an exponential tail
// (K = C V_T): what it cannot take back is the lag, an overload it cannot reach is the comet tail (kept up to qmax).
// q: the charge carried in; returns the charge carried out (x) and the signal read (y).
struct CrTube { float q, s; };
SM_FN CrTube cr_tube_field(float q, float L, float K, float B, SM_CREF(CreativeParams) p) {
    const float q0 = q + SM_EXP2(p.tubeGamma * SM_LOG2(SM_MAX(L, 0.0f) + p.tubeBias + 1e-8f));
    const float r1 = SM_MIN(SM_MAX(q0, 0.0f), B);
    const float rest = q0 - r1, beam = B - r1;
    float q1 = rest;
    if (rest <= 0.0f) q1 = rest - K * SM_LOG(1.0f + beam / K * SM_EXP(rest / K));
    CrTube t;
    t.q = SM_MIN(q1, p.tubeQmax * B);
    t.s = q0 - q1;
    return t;
}
// The charges of the three channels after two fields of light L.
SM_FN SMf3 cr_tube_frame(SMf3 q, SMf3 L, SM_CREF(CreativeParams) p) {
    float o[3];
    for (int c = 0; c < 3; ++c) {
        const float K = SM_MAX(p.tubeK[c], 1e-6f), B = p.tubeB[c];
        CrTube t = cr_tube_field(cr_at(q, c), cr_at(L, c), K, B, p);
        t = cr_tube_field(t.q, cr_at(L, c), K, B, p);
        o[c] = t.q;
    }
    return smf3(o[0], o[1], o[2]);
}

// The records of this pixel through the sensor. h0..h2: the source frames 1, 2, 3 before this one (the first
// `history` of them exist); S: the CCD's smear per column (3 planes of W).
SM_FN SMf3 cr_sensor(SMf3 e, CR_PLANE h0, CR_PLANE h1, CR_PLANE h2, CR_PLANE S, int x, int y,
                     SM_CREF(CreativeParams) p) {
    if (p.sensor == CR_SENSOR_TUBE) {
        // the charge before the oldest frame: that frame's light held a few fields, then every frame two fields
        const int n = p.history;
        const SMf3 L2 = n > 2 ? cr_records(h2, h2, h2, x, y, p) : e;
        const SMf3 L1 = n > 1 ? cr_records(h1, h1, h1, x, y, p) : e;
        const SMf3 L0 = n > 0 ? cr_records(h0, h0, h0, x, y, p) : e;
        const SMf3 oldest = n > 2 ? L2 : (n > 1 ? L1 : L0);
        SMf3 q = cr_tube_frame(cr_tube_frame(smf3(0.0f, 0.0f, 0.0f), oldest, p), oldest, p);
        if (n > 2) q = cr_tube_frame(q, L2, p);
        if (n > 1) q = cr_tube_frame(q, L1, p);
        if (n > 0) q = cr_tube_frame(q, L0, p);
        float o[3], over = 0.0f;
        const float bias = SM_EXP2(p.tubeGamma * SM_LOG2(p.tubeBias + 1e-8f));
        for (int c = 0; c < 3; ++c) {
            const float K = SM_MAX(p.tubeK[c], 1e-6f), B = p.tubeB[c], l = cr_at(e, c);
            const CrTube a = cr_tube_field(cr_at(q, c), l, K, B, p);
            const CrTube b = cr_tube_field(a.q, l, K, B, p);
            // the camera's gamma corrects half of the target's (a vidicon stays softer in the shadows)
            const float sig = SM_MAX(0.5f * (a.s + b.s) - bias, 0.0f);
            o[c] = SM_EXP2(SM_LOG2(sig + 1e-8f) / SM_SQRT(p.tubeGamma));
            over = SM_MAX(over, b.q / B);   // charge left over: the overload
        }
        // a striped tube the beam cannot discharge turns its highlights green
        const float g = p.tubeGreen * cr_smooth(0.0f, 1.0f, over);
        return smf3(o[0] * (1.0f - g), o[1], o[2] * (1.0f - g));
    }
    if (p.sensor == CR_SENSOR_CCD)   // the light on the vertical registers during the transfer: the whole column
        return smf3(e.x + S[x], e.y + S[p.W + x], e.z + S[2 * p.W + x]);
    if (p.sensor == CR_SENSOR_MOS) {   // XY readout: no smear, a fixed pattern of rows and columns
        const float f = 1.0f + p.fpn * (0.7f * cr_hash(0, y, 977) + 0.3f * cr_hash(x, 0, 979));
        return smf3(e.x * f, e.y * f, e.z * f);
    }
    return e;
}

// A CCD's smear of column x: the light above the full well, summed down the column, shared by all of it; a single
// mosaic chip reads it through its filters (about one colour).
SM_FN void cr_smear_col(CR_PLANE E0, CR_PLANE E1, CR_PLANE E2, CR_OUT S, int x, SM_CREF(CreativeParams) p) {
    float a = 0.0f, b = 0.0f, c = 0.0f;
    for (int y = 0; y < p.H; ++y) {
        const int k = y * p.W + x;
        a += SM_MAX(E0[k] - p.smearWell, 0.0f);
        b += SM_MAX(E1[k] - p.smearWell, 0.0f);
        c += SM_MAX(E2[k] - p.smearWell, 0.0f);
    }
    const float g = p.smear / (float)p.H;
    if (p.smearMono > 0.0f) {
        const float m = (a + b + c) * (1.0f / 3.0f);
        a = m;
        b = m * 0.85f;
        c = m;
    }
    S[x] = a * g;
    S[p.W + x] = b * g;
    S[2 * p.W + x] = c * g;
}

// ---- glow

// Highlight energy that exposes the halation: what lies above the threshold, softly (from the red record first).
SM_FN float cr_halation_source(SMf3 e, SM_CREF(CreativeParams) p) {
    const float v = SM_MAX(e.x * 0.6f + e.y * 0.3f + e.z * 0.1f - p.halThreshold, 0.0f);
    return v * v / (v + p.halThreshold);
}

// Diffusion mixes towards its blur, halation adds back the tinted light (per record).
SM_FN SMf3 cr_glow(SMf3 e, SMf3 blur, float hal, SM_CREF(CreativeParams) p) {
    e = smf3(e.x + (blur.x - e.x) * p.bloom, e.y + (blur.y - e.y) * p.bloom, e.z + (blur.z - e.z) * p.bloom);
    const float h = hal * p.halAmount;
    return smf3(e.x + h * p.halTint[0], e.y + h * p.halTint[1], e.z + h * p.halTint[2]);
}

// ---- curve

// H&D in stops: development slope and offset, then a toe below -toe and a shoulder above +shoulder (stops), each
// flattening to a quarter of the slope.
SM_FN float cr_hd(float s, int k, SM_CREF(CreativeParams) p) {
    float v = p.slope[k] * s + p.offset[k];
    v = v - 0.45f * cr_softplus((v - p.shoulder) / 0.6f);
    v = v + 0.45f * cr_softplus((-v - p.toe) / 0.6f);
    return v;
}

// Three grain fields (r, g, b) of unit variance, size px, seeds z + 1..3, sharing all but `colour` of their noise;
// one octave when `fine` (a print's grain, finer than the eye sees in clumps).
SM_FN float cr_grain_field(float u, float v, int z, int fine) {
    return fine != 0 ? 2.57f * cr_value(u, v, z) : cr_noise(u, v, z);
}
SM_FN SMf3 cr_grain_noise(int x, int y, float size, int z, float colour, int fine) {
    const float u = ((float)x + 0.5f) / size, v = ((float)y + 0.5f) / size;
    const float r = cr_grain_field(u * 0.9f + 3.1f, v * 0.9f, z + 1, fine), g = cr_grain_field(u + 7.7f, v + 1.3f, z + 2, fine);
    const float b = cr_grain_field(u * 1.2f + 1.9f, v * 1.2f + 9.4f, z + 3, fine);
    const float mono = (r + g + b) * 0.57735027f;   // the layers' shared part, unit variance
    return smf3(mono + colour * (r - mono), mono + colour * (g - mono), mono + colour * (b - mono));
}

// Grain of the camera film and of the duplicates (interpositive, internegative) in stops, strongest in the
// mid-tones (a bell over density).
SM_FN SMf3 cr_grain(SMf3 s, int x, int y, SM_CREF(CreativeParams) p) {
    if (p.grainAmount <= 0.0f && p.genGrain <= 0.0f) return s;
    const float m = (s.x + s.y + s.z) * (1.0f / 3.0f);
    const float bell = 0.35f + 0.65f * SM_EXP(-(m + 0.5f) * (m + 0.5f) / 12.5f);
    SMf3 n = smf3(0.0f, 0.0f, 0.0f);
    if (p.grainAmount > 0.0f) {
        const SMf3 a = cr_grain_noise(x, y, p.grainSize, p.frame * 16, p.grainColor, 0);
        const float w = p.grainAmount * bell;
        n = smf3(w * p.grainLayer[0] * a.x, w * p.grainLayer[1] * a.y, w * p.grainLayer[2] * a.z);
    }
    if (p.genGrain > 0.0f) {
        const SMf3 b = cr_grain_noise(x, y, p.genSize, p.frame * 16 + 3, p.grainColor, 0);
        const float w = p.genGrain * bell;
        n = smf3(n.x + w * b.x, n.y + w * b.y, n.z + w * b.z);
    }
    return smf3(s.x + n.x, s.y + n.y, s.z + n.z);
}

// Grain of the print stock, in its own densities (transmittances t): RMS grows about with the square root of density.
SM_FN SMf3 cr_print_grain(SMf3 t, int x, int y, SM_CREF(CreativeParams) p) {
    if (p.printGrain <= 0.0f) return t;
    const SMf3 n = cr_grain_noise(x, y, p.printGrainSize, p.frame * 16 + 6, p.grainColor, 1);
    float o[3];
    for (int k = 0; k < 3; ++k) {
        const float v = SM_MAX(cr_at(t, k), 1e-4f), D = -SM_LOG10(v);
        o[k] = v * cr_pow10m(p.printGrain * cr_at(n, k) * SM_SQRT(D) / (1.0f + 0.3f * D));
    }
    return smf3(o[0], o[1], o[2]);
}

// Dust (specks, per record when the records were separate plates) and vertical scratches, in stops.
SM_FN SMf3 cr_damage(SMf3 s, int x, int y, SM_CREF(CreativeParams) p) {
    if (p.dust <= 0.0f && p.scratches <= 0.0f) return s;
    const float cell = SM_MAX(4.0f, (float)p.H / 180.0f);
    const int ci = (int)CR_FLOOR((float)x / cell), cj = (int)CR_FLOOR((float)y / cell);
    float hit[3] = { 0.0f, 0.0f, 0.0f };
    for (int k = 0; k < 3; ++k) {
        const int z = p.records > 1 && p.image != CR_IMAGE_MONO ? k : 0;
        if (cr_hash01(ci, cj, p.frame * 16 + 200 + z) < p.dust * 0.004f) {
            const float px = ((float)ci + cr_hash01(ci, cj, 300 + z)) * cell, py = ((float)cj + cr_hash01(ci, cj, 400 + z)) * cell;
            const float r = cell * (0.15f + 0.35f * cr_hash01(ci, cj, 500 + z));
            const float dx = (float)x - px, dy = (float)y - py;
            hit[k] = 1.0f - cr_smooth(r * 0.6f, r, SM_SQRT(dx * dx + dy * dy));
        }
    }
    if (p.scratches > 0.0f) {   // a few lines per frame, each kept for some frames
        for (int n = 0; n < 3; ++n) {
            const int life = (p.frame / 12) * 3 + n;
            if (cr_hash01(life, 0, 600) < p.scratches) {
                const float sx = cr_hash01(life, 1, 601) * (float)p.W
                                 + 3.0f * cr_value((float)y / (float)p.H * 4.0f, (float)life, 602);
                const float w = 0.6f + (float)p.H / 1080.0f;
                const float a = 1.0f - cr_smooth(0.0f, w, SM_ABS((float)x - sx));
                for (int k = 0; k < 3; ++k) hit[k] = SM_MAX(hit[k], a);
            }
        }
    }
    const float d = 3.0f * p.dustSign;   // + on a positive: dark; - on a negative: white on the print
    return smf3(s.x - d * hit[0], s.y - d * hit[1], s.z - d * hit[2]);
}

// Colour in stops: split-toning by brightness, subtractive saturation, density of saturated colours.
SM_FN SMf3 cr_colour(SMf3 s, SM_CREF(CreativeParams) p) {
    const float m = (s.x + s.y + s.z) * (1.0f / 3.0f);
    const float w = 1.0f / (1.0f + SM_EXP(-(m - p.splitBalance) / 1.2f));
    s = smf3(s.x + (1.0f - w) * p.splitShadow[0] + w * p.splitHigh[0], s.y + (1.0f - w) * p.splitShadow[1] + w * p.splitHigh[1],
             s.z + (1.0f - w) * p.splitShadow[2] + w * p.splitHigh[2]);
    const float n = (s.x + s.y + s.z) * (1.0f / 3.0f);
    s = smf3(n + (s.x - n) * p.saturation, n + (s.y - n) * p.saturation, n + (s.z - n) * p.saturation);
    const float spread = SM_MAX(s.x, SM_MAX(s.y, s.z)) - SM_MIN(s.x, SM_MIN(s.y, s.z));
    const float d = p.density * 0.5f * spread;
    return smf3(s.x - d, s.y - d, s.z - d);
}

// A positive curve: stops -> transmittance or reflectance 0..1 (Naka-Rushton through the display grey).
SM_FN float cr_positive(float s, float c) {
    const float t = SM_EXP2(c * s);
    return t / (t + CR_SIGMA);
}

// ---- image

// Rec.2020 display-linear -> Rec.709 within gamut (towards its own luminance), then Gamma 2.4.
SM_FN SMf3 cr_display(SMf3 d, SM_CREF(CreativeParams) p) {
    SMf3 o = cr_mul(p.toOut, d);
    const float Y = 0.2126f * o.x + 0.7152f * o.y + 0.0722f * o.z;
    const float lo = SM_MIN(o.x, SM_MIN(o.y, o.z));
    if (lo < 0.0f && Y > lo) {
        const float t = Y / (Y - lo);
        o = smf3(Y + (o.x - Y) * t, Y + (o.y - Y) * t, Y + (o.z - Y) * t);
    }
    return smf3(sm_encode(sm_clamp(o.x, 0.0f, 1.0f), CR_GAMMA24), sm_encode(sm_clamp(o.y, 0.0f, 1.0f), CR_GAMMA24),
                sm_encode(sm_clamp(o.z, 0.0f, 1.0f), CR_GAMMA24));
}

// Back to the input encoding (Log output): the stops as scene light.
SM_FN SMf3 cr_encode_input(SMf3 s, SM_CREF(CreativeParams) p) {
    const SMf3 lin = cr_mul(p.fromRef, smf3(CR_GREY * SM_EXP2(s.x), CR_GREY * SM_EXP2(s.y), CR_GREY * SM_EXP2(s.z)));
    return smf3(sm_encode(lin.x, p.gamma), sm_encode(lin.y, p.gamma), sm_encode(lin.z, p.gamma));
}

// Which filter of an additive screen is seen at (x, y), as weights (the areas when the screen is finer than seen).
SM_FN SMf3 cr_mosaic(int x, int y, SM_CREF(CreativeParams) p) {
    const SMf3 a = smf3(p.area[0], p.area[1], p.area[2]);
    if (p.mosaic == CR_MOSAIC_NONE || p.mosaicTexture <= 0.0f) return a;
    SMf3 m = a;
    if (p.mosaic == CR_MOSAIC_GRAINS) {
        // cells of one clump of grains, rows staggered; the clump's dye follows its own draw plus three slow noise
        // fields (aggregation = their weight) and the batch's shares
        const float v = (float)y / p.mosaicScale;
        const int j = (int)CR_FLOOR(v);
        const int i = (int)CR_FLOOR((float)x / p.mosaicScale + 0.5f * cr_hash01(j, 0, 900));
        const float u0 = (float)i * 0.2f, v0 = (float)j * 0.2f;
        const float q0 = cr_hash(i, j, 910) + p.clump * 2.0f * cr_value(u0, v0, 911) + 3.0f * (a.x - 0.333f);
        const float q1 = cr_hash(i, j, 920) + p.clump * 2.0f * cr_value(u0, v0, 921) + 3.0f * (a.y - 0.333f);
        const float q2 = cr_hash(i, j, 930) + p.clump * 2.0f * cr_value(u0, v0, 931) + 3.0f * (a.z - 0.333f);
        m = q0 >= q1 && q0 >= q2 ? smf3(1.0f, 0.0f, 0.0f) : (q1 >= q2 ? smf3(0.0f, 1.0f, 0.0f) : smf3(0.0f, 0.0f, 1.0f));
    } else if (p.mosaic == CR_MOSAIC_RESEAU) {
        // ruled at 23 degrees: a red line (its share of the area), then a line of alternating green and blue squares
        const float c = 0.92050485f, s = 0.39073113f;
        const float u = ((float)x * c + (float)y * s) / p.mosaicScale, v = (-(float)x * s + (float)y * c) / p.mosaicScale;
        if (u - CR_FLOOR(u) < a.x) m = smf3(1.0f, 0.0f, 0.0f);
        else {
            const float fv = 0.5f * v - CR_FLOOR(0.5f * v);
            m = fv < a.y / (a.y + a.z) ? smf3(0.0f, 1.0f, 0.0f) : smf3(0.0f, 0.0f, 1.0f);
        }
    }
    const float t = p.mosaicTexture;
    return smf3(a.x + (m.x - a.x) * t, a.y + (m.y - a.y) * t, a.z + (m.z - a.z) * t);
}

// Distance from (x, y) to the frame's edge (0 at the edge, 1 at the centre), roughened like a poured or brushed plate.
SM_FN float cr_edge(int x, int y, SM_CREF(CreativeParams) p) {
    const float fx = ((float)x + 0.5f) / (float)p.W, fy = ((float)y + 0.5f) / (float)p.H;
    const float d = 2.0f * SM_MIN(SM_MIN(fx, 1.0f - fx), SM_MIN(fy, 1.0f - fy) * (float)p.H / (float)p.W);
    return d + 0.03f * cr_noise(fx * 14.0f, fy * 14.0f, 950);
}

// The image of the positive transmittances t[k], as display-linear Rec.2020.
SM_FN SMf3 cr_image(SMf3 t, int x, int y, SM_CREF(CreativeParams) p) {
    const float floorT = cr_pow10m(p.dmax);
    if (p.image == CR_IMAGE_ADD) {
        const SMf3 m = cr_mosaic(x, y, p);
        SMf3 o = smf3(0.0f, 0.0f, 0.0f), w = smf3(0.0f, 0.0f, 0.0f);
        for (int k = 0; k < 3; ++k) {
            const float v = SM_MAX(cr_at(t, k), floorT) * cr_at(m, k), a = p.area[k];
            o = smf3(o.x + v * p.filt[k * 3], o.y + v * p.filt[k * 3 + 1], o.z + v * p.filt[k * 3 + 2]);
            w = smf3(w.x + a * p.filt[k * 3], w.y + a * p.filt[k * 3 + 1], w.z + a * p.filt[k * 3 + 2]);
        }
        const float g = p.viewLight / SM_MAX(cr_luma(w), CR_EPS);   // 1 = the screen's white seen as white
        return smf3(o.x * g, o.y * g, o.z * g);
    }
    if (p.image == CR_IMAGE_MONO) {
        const float v = SM_MAX(t.x, floorT), D = -SM_LOG10(v);
        SMf3 o = smf3(p.paper[0] * cr_pow10m(D * p.tone[0] + p.stain[0]), p.paper[1] * cr_pow10m(D * p.tone[1] + p.stain[1]),
                      p.paper[2] * cr_pow10m(D * p.tone[2] + p.stain[2]));
        if (p.mirror > 0.0f) {   // polished silver reflects the room where the deposit is thin
            const float r = p.mirror * (1.0f - v);
            o = smf3(o.x * v + r * p.mirrorTint[0], o.y * v + r * p.mirrorTint[1], o.z * v + r * p.mirrorTint[2]);
        }
        if (p.oxidation > 0.0f) {   // tarnish from the edges: thin-film colours (Ag2S, n 2.5) at 610, 550, 465 nm
            const float h = p.oxidation * (1.0f - cr_smooth(0.0f, 0.35f, cr_edge(x, y, p)));
            const float ph = 6.2831853f * 1.25f * h;
            o = smf3(o.x * (1.0f - 0.6f * h * (0.5f + 0.5f * CR_COS(ph / 0.610f))),
                     o.y * (1.0f - 0.6f * h * (0.5f + 0.5f * CR_COS(ph / 0.550f))),
                     o.z * (1.0f - 0.6f * h * (0.5f + 0.5f * CR_COS(ph / 0.465f))));
        }
        if (p.border > 0.0f) {   // where the emulsion or the sensitiser did not reach: the bare support
            const float b = 1.0f - cr_smooth(0.0f, 0.04f, cr_edge(x, y, p) - 0.06f * p.border);
            o = smf3(o.x + (p.borderTint[0] - o.x) * b, o.y + (p.borderTint[1] - o.y) * b, o.z + (p.borderTint[2] - o.z) * b);
        }
        return o;
    }
    // subtractive: one dye per record (red record -> cyan dye), unwanted absorptions in the matrix rows
    const SMf3 d = smf3(-SM_LOG10(SM_MAX(t.x, floorT)) * p.fade[0], -SM_LOG10(SM_MAX(t.y, floorT)) * p.fade[1],
                        -SM_LOG10(SM_MAX(t.z, floorT)) * p.fade[2]);
    const float neutral = p.keyImage * d.y + p.silver * (d.x + d.y + d.z) * (1.0f / 3.0f);   // key image, silver kept
    float o[3];
    for (int c = 0; c < 3; ++c)
        o[c] = cr_pow10m(d.x * p.dye[c] + d.y * p.dye[3 + c] + d.z * p.dye[6 + c] + neutral + p.stain[c]);
    return smf3(o[0], o[1], o[2]);
}

// The process chain from the records' exposures (glows added) to the output code values.
SM_FN SMf3 cr_process(SMf3 e, int x, int y, SM_CREF(CreativeParams) p) {
    SMf3 s = smf3(cr_hd(cr_stops(e.x), 0, p), cr_hd(cr_stops(e.y), 1, p), cr_hd(cr_stops(e.z), 2, p));
    if (p.records == 1) s = smf3(s.x, s.x, s.x);
    const float m = (s.x + s.y + s.z) * (1.0f / 3.0f);
    s = smf3(m + (s.x - m) * p.interlayer, m + (s.y - m) * p.interlayer, m + (s.z - m) * p.interlayer);
    s = cr_grain(s, x, y, p);
    s = cr_damage(s, x, y, p);
    s = cr_colour(s, p);
    if (p.output == CR_OUT_LOG) return cr_encode_input(s, p);
    const SMf3 t = smf3(cr_positive(s.x + p.printOffset[0], p.printContrast[0]),
                        cr_positive(s.y + p.printOffset[1], p.printContrast[1]),
                        cr_positive(s.z + p.printOffset[2], p.printContrast[2]));
    return cr_display(cr_image(cr_print_grain(t, x, y, p), x, y, p), p);
}

// ---- signal chains (analog video, digital cameras): Y Cb Cr planes of Gamma 2.4 code values

SM_FN SMf3 cr_signal(SMf3 c, int x, int y, SM_CREF(CreativeParams) p) {
    SMf3 s = cr_colour(smf3(cr_stops(c.x), cr_stops(c.y), cr_stops(c.z)), p);
    if (p.kind == CR_KIND_DIGITAL && p.noise > 0.0f) {   // sensor noise, before the curve: it shows in the shadows
        const float n = p.noise * cr_hash(x, y, p.frame * 5 + 61), cn = p.chromaNoise;
        s = smf3(s.x + n + cn * cr_hash(x, y, p.frame * 5 + 62), s.y + n, s.z + n + cn * cr_hash(x, y, p.frame * 5 + 63));
    }
    float d[3];
    for (int k = 0; k < 3; ++k) {   // the camera's curve: a knee of width knee below the clip at +clip stops
        float v = cr_at(s, k);
        v = v - p.knee * cr_softplus((v - p.clip) / p.knee + 1.0f);
        d[k] = cr_positive(SM_MIN(v, p.clip), p.contrast);
    }
    SMf3 o = cr_mul(p.toOut, smf3(d[0], d[1], d[2]));
    o = smf3(sm_encode(sm_clamp(o.x, 0.0f, 1.0f), CR_GAMMA24), sm_encode(sm_clamp(o.y, 0.0f, 1.0f), CR_GAMMA24),
             sm_encode(sm_clamp(o.z, 0.0f, 1.0f), CR_GAMMA24));
    const float Y = 0.299f * o.x + 0.587f * o.y + 0.114f * o.z;
    const float hl = 1.0f - p.highDesat * cr_smooth(0.6f, 1.0f, Y);   // colour leaves the highlights
    return smf3(Y, (o.z - Y) * 0.564f * hl, (o.x - Y) * 0.713f * hl);
}

// Back to code values: Y with its peaking (Yb = its blur), Cb Cr, tape noise, head switching, dropouts, levels.
SM_FN SMf3 cr_signal_out(float Y, float Yb, float Cb, float Cr, int x, int y, SM_CREF(CreativeParams) p) {
    Y = Y + p.sharp * (Y - Yb);
    if (p.kind == CR_KIND_ANALOG) {
        const int line = y / 2;
        Y += p.noise * (1.0f + 0.3f * Y) * (0.6f * cr_hash(x, y, p.frame * 3 + 11)
                                            + 0.4f * cr_value((float)x * 0.35f, (float)y, p.frame * 3 + 12));
        Cb += p.chromaNoise * cr_value((float)x * 0.06f, (float)line * 0.5f, p.frame * 3 + 13);
        Cr += p.chromaNoise * cr_value((float)x * 0.06f + 40.0f, (float)line * 0.5f, p.frame * 3 + 14);
        if ((float)y < p.headLines) {   // the switching point, at the bottom (row 0): a noisy band
            Y += p.noise * 3.0f * cr_hash(x / 3, y, p.frame + 31);
            Cb *= 0.4f;
            Cr *= 0.4f;
        }
        if (p.dropouts > 0.0f && cr_hash01(y, p.frame, 41) < p.dropouts) {   // a streak on this line
            const float start = cr_hash01(y, p.frame, 43) * (float)p.W;
            const float len = (0.01f + 0.08f * cr_hash01(y, p.frame, 47)) * (float)p.W;
            if ((float)x >= start && (float)x < start + len) {
                Y = cr_hash(y, p.frame, 49) > -0.3f ? 0.92f : 0.05f;
                Cb = 0.0f;
                Cr = 0.0f;
            }
        }
    }
    SMf3 d = smf3(Y + 1.402f * Cr, Y - 0.344136f * Cb - 0.714136f * Cr, Y + 1.772f * Cb);
    if (p.levels > 0.0f)   // 8-bit (or coarser) banding
        d = smf3(CR_FLOOR(d.x * p.levels + 0.5f) / p.levels, CR_FLOOR(d.y * p.levels + 0.5f) / p.levels,
                 CR_FLOOR(d.z * p.levels + 0.5f) / p.levels);
    return smf3(sm_clamp(d.x, 0.0f, 1.0f), sm_clamp(d.y, 0.0f, 1.0f), sm_clamp(d.z, 0.0f, 1.0f));
}

// ---- passes, one output value each (the CPU loops and the Metal kernels call the same functions)

// 1D Gaussian of a w x h plane along x (axis 0) or y (1) at (x, y), read shift px earlier along the axis; edges clamp.
SM_FN float cr_blur_at(CR_PLANE in, int w, int h, int x, int y, int axis, float sigma, int shift) {
    const int n = axis == 0 ? w : h;
    const int at = axis == 0 ? x : y;
    const int stride = axis == 0 ? 1 : w, base = axis == 0 ? y * w : x;
    if (sigma < 0.3f) return in[base + cr_clamp_i(at - shift, n) * stride];
    // symmetric taps; the Gaussian weights by recurrence, w(d + 1) = w(d) q(d), q(d + 1) = q(d) r: two exponentials
    const int r = (int)CR_CEIL(3.0f * sigma), c = at - shift;
    const float k = -0.5f / (sigma * sigma);
    const float rr = SM_EXP(2.0f * k);
    float q = SM_EXP(k), wgt = 1.0f;
    float sum = in[base + cr_clamp_i(c, n) * stride], wsum = 1.0f;
    for (int d = 1; d <= r; ++d) {
        wgt *= q;
        q *= rr;
        sum += wgt * (in[base + cr_clamp_i(c + d, n) * stride] + in[base + cr_clamp_i(c - d, n) * stride]);
        wsum += 2.0f * wgt;
    }
    return sum / wsum;
}

// Mean of the s x s block of grid cell (i, j): record c of the planes E, (c = 3) the halation source, or (c = 4) the
// luminance of a picture.
SM_FN float cr_down_at(CR_PLANE E0, CR_PLANE E1, CR_PLANE E2, int i, int j, int s, int c, SM_CREF(CreativeParams) p) {
    float sum = 0.0f;
    int n = 0;
    for (int y = j * s; y < j * s + s && y < p.H; ++y)
        for (int x = i * s; x < i * s + s && x < p.W; ++x) {
            const int k = y * p.W + x;
            const SMf3 v = smf3(E0[k], E1[k], E2[k]);
            sum += c == 3 ? cr_halation_source(v, p) : (c == 4 ? 0.2126f * v.x + 0.7152f * v.y + 0.0722f * v.z : cr_at(v, c));
            ++n;
        }
    return n > 0 ? sum / (float)n : 0.0f;
}

// The records of (x, y) with their glows.
SM_FN SMf3 cr_glow_at(CR_PLANE E0, CR_PLANE E1, CR_PLANE E2, CR_PLANE hal, CR_PLANE b0, CR_PLANE b1, CR_PLANE b2,
                      int x, int y, SM_CREF(CreativeParams) p) {
    const int k = y * p.W + x;
    const SMf3 e = smf3(E0[k], E1[k], E2[k]);
    if (p.bloom <= 0.0f && p.halAmount <= 0.0f) return e;
    const SMf3 blur = p.bloom > 0.0f ? smf3(cr_grid(b0, p.bW, p.bH, p.bS, x, y), cr_grid(b1, p.bW, p.bH, p.bS, x, y),
                                            cr_grid(b2, p.bW, p.bH, p.bS, x, y))
                                     : e;
    const float h = p.halAmount > 0.0f ? cr_grid(hal, p.hW, p.hH, p.hS, x, y) : 0.0f;
    return cr_glow(e, blur, h, p);
}

// Output of a signal chain at (x, y): luma and its blur, chroma averaged over its block (line pairs for colour-under
// tape, 4:1:1 or 4:2:0 for digital).
SM_FN SMf3 cr_signal_at(CR_PLANE Y, CR_PLANE Yb, CR_PLANE Cb, CR_PLANE Cr, int x, int y, SM_CREF(CreativeParams) p) {
    const int bx = p.chromaBlockX, by = p.chromaBlockY;
    const int x0 = (x / bx) * bx, y0 = (y / by) * by;
    float cb = 0.0f, cr = 0.0f;
    int n = 0;
    for (int j = y0; j < y0 + by && j < p.H; ++j)
        for (int i = x0; i < x0 + bx && i < p.W; ++i) {
            cb += Cb[j * p.W + i];
            cr += Cr[j * p.W + i];
            ++n;
        }
    const int k = y * p.W + x;
    return cr_signal_out(Y[k], Yb[k], cb / (float)n, cr / (float)n, x, y, p);
}

// ---- digital cameras, emulated (research DIGITALI_OTTICHE.md A.0): the picture at the sensor's own size (box average,
// or one sample per pixel for line-skipping and sensors without an optical low-pass: aliasing), the ISP's unsharp
// mask, the codec's 8x8 DCT with the JPEG tables (chroma subsampled first), then scaled back up as a player does.
// Quantisation rounds softly (a steep continuous step) so the CPU and the GPU agree.

SM_CONST float CR_DCT[64] = {
    0.353553391f, 0.353553391f, 0.353553391f, 0.353553391f, 0.353553391f, 0.353553391f, 0.353553391f, 0.353553391f,
    0.490392640f, 0.415734806f, 0.277785117f, 0.097545161f, -0.097545161f, -0.277785117f, -0.415734806f, -0.490392640f,
    0.461939766f, 0.191341716f, -0.191341716f, -0.461939766f, -0.461939766f, -0.191341716f, 0.191341716f, 0.461939766f,
    0.415734806f, -0.097545161f, -0.490392640f, -0.277785117f, 0.277785117f, 0.490392640f, 0.097545161f, -0.415734806f,
    0.353553391f, -0.353553391f, -0.353553391f, 0.353553391f, 0.353553391f, -0.353553391f, -0.353553391f, 0.353553391f,
    0.277785117f, -0.490392640f, 0.097545161f, 0.415734806f, -0.415734806f, -0.097545161f, 0.490392640f, -0.277785117f,
    0.191341716f, -0.461939766f, 0.461939766f, -0.191341716f, -0.191341716f, 0.461939766f, -0.461939766f, 0.191341716f,
    0.097545161f, -0.277785117f, 0.415734806f, -0.490392640f, 0.490392640f, -0.415734806f, 0.277785117f, -0.097545161f };
SM_CONST float CR_JPEG_Y[64] = {
    16.0f, 11.0f, 10.0f, 16.0f, 24.0f, 40.0f, 51.0f, 61.0f,
    12.0f, 12.0f, 14.0f, 19.0f, 26.0f, 58.0f, 60.0f, 55.0f,
    14.0f, 13.0f, 16.0f, 24.0f, 40.0f, 57.0f, 69.0f, 56.0f,
    14.0f, 17.0f, 22.0f, 29.0f, 51.0f, 87.0f, 80.0f, 62.0f,
    18.0f, 22.0f, 37.0f, 56.0f, 68.0f, 109.0f, 103.0f, 77.0f,
    24.0f, 35.0f, 55.0f, 64.0f, 81.0f, 104.0f, 113.0f, 92.0f,
    49.0f, 64.0f, 78.0f, 87.0f, 103.0f, 121.0f, 120.0f, 101.0f,
    72.0f, 92.0f, 95.0f, 98.0f, 112.0f, 100.0f, 103.0f, 99.0f };
SM_CONST float CR_JPEG_C[64] = {
    17.0f, 18.0f, 24.0f, 47.0f, 99.0f, 99.0f, 99.0f, 99.0f,
    18.0f, 21.0f, 26.0f, 66.0f, 99.0f, 99.0f, 99.0f, 99.0f,
    24.0f, 26.0f, 56.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f,
    47.0f, 66.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f,
    99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f,
    99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f,
    99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f,
    99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f };

// Plane P (W x H, the picture's) averaged over the footprint of pixel (i, j) of a w x h plane.
SM_FN float cr_dig_down(CR_PLANE P, int i, int j, int w, int h, SM_CREF(CreativeParams) p) {
    const float sx = (float)p.W / (float)w, sy = (float)p.H / (float)h;
    if (p.dAlias != 0)
        return P[cr_clamp_i((int)(((float)j + 0.5f) * sy), p.H) * p.W + cr_clamp_i((int)(((float)i + 0.5f) * sx), p.W)];
    const int x0 = (int)CR_FLOOR((float)i * sx), y0 = (int)CR_FLOOR((float)j * sy);
    const int x1 = cr_max_i(x0 + 1, (int)CR_FLOOR((float)(i + 1) * sx)), y1 = cr_max_i(y0 + 1, (int)CR_FLOOR((float)(j + 1) * sy));
    float s = 0.0f;
    int n = 0;
    for (int y = y0; y < y1 && y < p.H; ++y)
        for (int x = x0; x < x1 && x < p.W; ++x) {
            s += P[y * p.W + x];
            ++n;
        }
    return n > 0 ? s / (float)n : P[cr_clamp_i(y0, p.H) * p.W + cr_clamp_i(x0, p.W)];
}

// The ISP's unsharp mask on the sensor-size luma (3x3 mean as the blur).
SM_FN float cr_dig_sharp(CR_PLANE Y, int i, int j, SM_CREF(CreativeParams) p) {
    float s = 0.0f;
    for (int b = -1; b <= 1; ++b)
        for (int a = -1; a <= 1; ++a) s += Y[cr_clamp_i(j + b, p.dH) * p.dW + cr_clamp_i(i + a, p.dW)];
    const float v = Y[j * p.dW + i];
    return v + p.dSharp * (v - s * (1.0f / 9.0f));
}

// Block (bx, by) of a w x h plane through the codec: DCT, quantisation by the JPEG table (chroma: its own) times the
// quality's scale, inverse DCT. Values in code units (x255, as JPEG); edge blocks read the edge again.
SM_FN void cr_dig_dct(CR_PLANE in, CR_OUT out, int w, int h, int bx, int by, int chroma, SM_CREF(CreativeParams) p) {
    float f[64], t[64];
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            f[y * 8 + x] = 255.0f * in[cr_clamp_i(by * 8 + y, h) * w + cr_clamp_i(bx * 8 + x, w)];
    for (int y = 0; y < 8; ++y)   // rows
        for (int u = 0; u < 8; ++u) {
            float s = 0.0f;
            for (int x = 0; x < 8; ++x) s += CR_DCT[u * 8 + x] * f[y * 8 + x];
            t[y * 8 + u] = s;
        }
    for (int v = 0; v < 8; ++v)   // columns, then the quantiser
        for (int u = 0; u < 8; ++u) {
            float s = 0.0f;
            for (int y = 0; y < 8; ++y) s += CR_DCT[v * 8 + y] * t[y * 8 + u];
            const float q = SM_MAX((chroma != 0 ? CR_JPEG_C[v * 8 + u] : CR_JPEG_Y[v * 8 + u]) * p.dQ, 1.0f);
            const float a = s / q, n = CR_FLOOR(a);
            f[v * 8 + u] = (n + cr_smooth(0.35f, 0.65f, a - n)) * q;
        }
    for (int v = 0; v < 8; ++v)   // inverse: columns
        for (int u = 0; u < 8; ++u) {
            float s = 0.0f;
            for (int k = 0; k < 8; ++k) s += CR_DCT[k * 8 + v] * f[k * 8 + u];
            t[v * 8 + u] = s;
        }
    for (int y = 0; y < 8; ++y)   // rows
        for (int x = 0; x < 8; ++x) {
            const int gx = bx * 8 + x, gy = by * 8 + y;
            if (gx >= w || gy >= h) continue;
            float s = 0.0f;
            for (int u = 0; u < 8; ++u) s += CR_DCT[u * 8 + x] * t[y * 8 + u];
            out[gy * w + gx] = s * (1.0f / 255.0f);
        }
}

// Plane P (w x h) read bilinearly at its own coordinates (u, v), edges clamped.
SM_FN float cr_dig_sample(CR_PLANE P, int w, int h, float u, float v) {
    u = sm_clamp(u, 0.0f, (float)(w - 1));
    v = sm_clamp(v, 0.0f, (float)(h - 1));
    const int i = cr_min_i((int)CR_FLOOR(u), w - 1), j = cr_min_i((int)CR_FLOOR(v), h - 1);
    const int i1 = cr_min_i(i + 1, w - 1), j1 = cr_min_i(j + 1, h - 1);
    const float a = u - (float)i, b = v - (float)j;
    const float top = P[j * w + i] + (P[j * w + i1] - P[j * w + i]) * a;
    const float bot = P[j1 * w + i] + (P[j1 * w + i1] - P[j1 * w + i]) * a;
    return top + (bot - top) * b;
}

// Output pixel (x, y): the sensor-size luma and the subsampled colour scaled up, then code values.
SM_FN SMf3 cr_dig_out(CR_PLANE Y, CR_PLANE Cb, CR_PLANE Cr, int x, int y, SM_CREF(CreativeParams) p) {
    const float u = ((float)x + 0.5f) * (float)p.dW / (float)p.W - 0.5f;
    const float v = ((float)y + 0.5f) * (float)p.dH / (float)p.H - 0.5f;
    const float uc = ((float)x + 0.5f) * (float)p.cW / (float)p.W - 0.5f;
    const float vc = ((float)y + 0.5f) * (float)p.cH / (float)p.H - 0.5f;
    const float Yv = cr_dig_sample(Y, p.dW, p.dH, u, v);
    return cr_signal_out(Yv, Yv, cr_dig_sample(Cb, p.cW, p.cH, uc, vc), cr_dig_sample(Cr, p.cW, p.cH, uc, vc), x, y, p);
}

// ---- analog tape, emulated: a signal along each line, causal filters, one cause for many symptoms
// (docs/CREATIVE.md, research VIDEO_EMULAZIONE.md). Rows are bottom-up (OFX): frame line l counts from the top; its
// field is l & 1 and its field line l >> 1. Noise, timebase and dropouts belong to the field line of their field.

SM_FN int cr_frame_line(int y, SM_CREF(CreativeParams) p) {
    const int lines = 2 * p.fieldLines;
    return cr_clamp_i((int)((float)(p.H - 1 - y) * (float)lines / (float)p.H), lines);
}
// About unit-variance Gaussian noise (three uniform hashes).
SM_FN float cr_gauss(int x, int y, int z) { return cr_hash(x, y, z) + cr_hash(x, y, z + 1) + cr_hash(x, y, z + 2); }

// Horizontal displacement (px, + to the right) of output row y: what the TV's line lock leaves of the tape's timebase
// error. A slow wave, the scrape of the tape, the head switch near the bottom of the field (a step the lock recovers
// over a few lines) and the flagging at the top of the next field, all from the same field's error.
SM_FN float cr_tape_jitter(int r, int F, SM_CREF(CreativeParams) p) {
    return p.tbWave * cr_value((float)r / p.tbWaveLines, (float)F * 0.37f, 901) + p.tbScrape * cr_gauss(r, F, 905);
}
SM_FN float cr_tape_shift(int y, SM_CREF(CreativeParams) p) {
    const int l = cr_frame_line(y, p), F = p.frame * 2 + (l & 1), r = l >> 1;
    float d = cr_tape_jitter(r, F, p);
    const float fromBottom = (float)(p.fieldLines - 1 - r);
    if (p.headStep != 0.0f && fromBottom < p.headRows)
        d += p.headStep * (1.0f + 0.2f * cr_hash(F, 0, 911)) * SM_EXP(-(p.headRows - fromBottom) / p.headTau);
    if (p.flagAmp != 0.0f && (float)r < p.flagRows)
        d += p.flagAmp * (1.0f + 0.3f * cr_hash(F, 1, 913)) * SM_EXP(-3.0f * (float)r / p.flagRows);
    return d;
}

// One output row through the recorder and the player (serial along the line). Luma: pre-emphasis, the FM clip,
// FM noise (white, differentiated: it grows with frequency), de-emphasis (its exact inverse, so only the clip and the
// noise remain), the playback low-pass (two biquads, causal: overshoot after the edge) and the crispener. Colour:
// three one-pole low-passes, AM and phase noise, the hue turned by the same timebase error, saturated colour
// compressed. Y Cb Cr in, out to (Yo, Cbo, Cro) of the same layout.
SM_FN void cr_tape_row(CR_PLANE Y, CR_PLANE Cb, CR_PLANE Cr, CR_OUT Yo, CR_OUT Cbo, CR_OUT Cro, int y,
                       SM_CREF(CreativeParams) p) {
    const int l = cr_frame_line(y, p), F = p.frame * 2 + (l & 1), r = l >> 1;
    const int row = y * p.W;
    // the colour circuit locks to each line's burst: of the timebase only the fast jitter turns the hue
    const float phi = p.hueGain * cr_tape_jitter(r, F, p) + p.huePhase * cr_gauss(r, F, 921);
    const float cs = CR_COS(phi), sn = CR_SIN(phi);
    float se = Y[row], sd = Y[row];
    float x1a = Y[row], x2a = Y[row], y1a = Y[row], y2a = Y[row], x1b = Y[row], x2b = Y[row], y1b = Y[row], y2b = Y[row];
    float sc = Y[row], wPrev = cr_gauss(-1, r, F * 7 + 931);
    float u1 = Cb[row], u2 = Cb[row], u3 = Cb[row], v1 = Cr[row], v2 = Cr[row], v3 = Cr[row];
    float nu = 0.0f, nv = 0.0f;
    for (int x = 0; x < p.W; ++x) {
        const int k = row + x;
        // luma
        const float e = Y[k];
        se += p.emphK * (e - se);
        float m = e + p.emphG * (e - se);
        m = sm_clamp(m, p.clipLo, p.clipHi);
        const float w = cr_gauss(x, r, F * 7 + 931);
        m += p.fmNoise * (0.6f + 0.4f * sm_clamp(e, 0.0f, 1.2f)) * (w - wPrev);
        wPrev = w;
        const float dd = (m + p.emphG * (1.0f - p.emphK) * sd) / (1.0f + p.emphG - p.emphG * p.emphK);
        sd += p.emphK * (dd - sd);
        const float a = p.lp[0] * dd + p.lp[1] * x1a + p.lp[2] * x2a - p.lp[3] * y1a - p.lp[4] * y2a;
        x2a = x1a; x1a = dd; y2a = y1a; y1a = a;
        const float b = p.lp[5] * a + p.lp[6] * x1b + p.lp[7] * x2b - p.lp[8] * y1b - p.lp[9] * y2b;
        x2b = x1b; x1b = a; y2b = y1b; y1b = b;
        sc += p.crispK * (b - sc);
        Yo[k] = b + p.crisp * (b - sc);
        // colour; a striped tube or a mosaic chip reads fine luma detail as colour (its carrier), which the
        // colour-under then smooths into slow blotches
        float cbIn = Cb[k], crIn = Cr[k];
        if (p.falseColour > 0.0f) {
            const float hp = e - 0.5f * (Y[row + (x > 0 ? x - 1 : 0)] + Y[row + (x + 1 < p.W ? x + 1 : x)]);
            const float ph = 6.2831853f * (float)x / p.fcPeriod + 3.1415927f * (float)((r + F) & 1);
            cbIn += p.falseColour * hp * CR_COS(ph);
            crIn += p.falseColour * hp * CR_SIN(ph);
        }
        u1 += p.chromaK * (cbIn - u1); u2 += p.chromaK * (u1 - u2); u3 += p.chromaK * (u2 - u3);
        v1 += p.chromaK * (crIn - v1); v2 += p.chromaK * (v1 - v2); v3 += p.chromaK * (v2 - v3);
        nu += p.chromaK * (cr_gauss(x, r, F * 7 + 941) - nu);
        nv += p.chromaK * (cr_gauss(x, r, F * 7 + 951) - nv);
        const float am = 1.0f + p.chromaAM * nu;
        float cu = (u3 * cs - v3 * sn) * am + p.chromaSigmaN * nu, cv = (u3 * sn + v3 * cs) * am + p.chromaSigmaN * nv;
        const float mag = SM_SQRT(cu * cu + cv * cv), lim = 1.0f / SM_SQRT(1.0f + mag * mag / (p.chromaLimit * p.chromaLimit));
        Cbo[k] = cu * lim;
        Cro[k] = cv * lim;
    }
}

// Linear read of row y of a plane at the fractional x; outside the line on the left: blanking (0).
SM_FN float cr_row_at(CR_PLANE P, int y, float x, SM_CREF(CreativeParams) p) {
    if (x < -1.0f) return 0.0f;
    const float fx = CR_FLOOR(x);
    const int i = (int)fx;
    const float t = x - fx;
    const float a = i < 0 ? 0.0f : P[y * p.W + cr_clamp_i(i, p.W)], b = P[y * p.W + cr_clamp_i(i + 1, p.W)];
    return a + (b - a) * t;
}

// The player's output at (x, y), as the TV shows it: the line displaced by the timebase, the colour later still and
// averaged with the field's previous line (comb), dropouts replaced by the line above (the compensator) and colourless,
// the noisy band of the head switch; then R'G'B' code values.
SM_FN SMf3 cr_tape_out(CR_PLANE Yp, CR_PLANE Cbp, CR_PLANE Crp, int x, int y, SM_CREF(CreativeParams) p) {
    const int l = cr_frame_line(y, p), F = p.frame * 2 + (l & 1), r = l >> 1;
    const float xs = (float)x - cr_tape_shift(y, p);
    int src = y;
    bool drop = false;
    if (p.dropDensity > 0.0f) {   // defects on the tape: they live some fields and slide up the picture
        const int epoch = (int)CR_FLOOR((float)F / p.dropLife);
        const int cell = (int)CR_FLOOR((float)r + p.dropSlide * (float)(F - epoch * (int)p.dropLife));
        if (cr_hash01(cell, epoch, 961) < p.dropDensity) {
            const float h = cr_hash01(cell, epoch, 963);
            const float start = cr_hash01(cell, epoch, 965) * (float)p.W, len = (0.004f + 0.08f * h * h * h) * (float)p.W;
            drop = xs >= start && xs < start + len;
        }
    }
    if (drop) src = cr_clamp_i(y + p.docRows, p.H);
    float Yv = cr_row_at(Yp, src, xs, p);
    const float xc = xs - p.delayPx;
    const int up = cr_clamp_i(y + p.combRows, p.H);
    float cb = cr_row_at(Cbp, y, xc, p), cr = cr_row_at(Crp, y, xc, p);
    if (p.combRows > 0) {
        cb = 0.5f * (cb + cr_row_at(Cbp, up, xc, p));
        cr = 0.5f * (cr + cr_row_at(Crp, up, xc, p));
    }
    if (drop) {
        cb = 0.0f;
        cr = 0.0f;
    }
    if ((float)(p.fieldLines - 1 - r) < 1.5f && p.headNoise > 0.0f) {   // the switching lines themselves
        Yv += p.headNoise * cr_gauss(x / 2, r, F + 971);
        cb *= 0.3f;
        cr *= 0.3f;
    }
    const SMf3 d = smf3(Yv + 1.402f * cr, Yv - 0.344136f * cb - 0.714136f * cr, Yv + 1.772f * cb);
    return smf3(sm_clamp(d.x, 0.0f, 1.0f), sm_clamp(d.y, 0.0f, 1.0f), sm_clamp(d.z, 0.0f, 1.0f));
}

// ---- viewing medium: the finished picture (planes M, display-linear Rec.709) as a screen or a print shows it

// The picture to show at (x, y): the output's code values, decoded (Gamma 2.4); an LCD still shows part of the previous
// picture (prev, the same layout; read only when mGhost > 0).
SM_FN SMf3 cr_medium_in(CR_PLANE dst, CR_PLANE prev, int x, int y, SM_CREF(CreativeParams) p) {
    const int d = (y * p.dstRowPixels + x) * 4;
    SMf3 c = smf3(sm_decode(dst[d], CR_GAMMA24), sm_decode(dst[d + 1], CR_GAMMA24), sm_decode(dst[d + 2], CR_GAMMA24));
    if (p.mGhost > 0.0f) {
        const float g = p.mGhost;
        c = smf3(c.x + (sm_decode(prev[d], CR_GAMMA24) - c.x) * g, c.y + (sm_decode(prev[d + 1], CR_GAMMA24) - c.y) * g,
                 c.z + (sm_decode(prev[d + 2], CR_GAMMA24) - c.z) * g);
    }
    return c;
}

// The picture at the fractional pixel (u, v), bilinear, edges clamped.
SM_FN SMf3 cr_pic(CR_PLANE M0, CR_PLANE M1, CR_PLANE M2, float u, float v, SM_CREF(CreativeParams) p) {
    u = sm_clamp(u, 0.0f, (float)(p.W - 1));
    v = sm_clamp(v, 0.0f, (float)(p.H - 1));
    const int i0 = (int)u, j0 = (int)v;
    const int i1 = i0 + 1 < p.W ? i0 + 1 : p.W - 1, j1 = j0 + 1 < p.H ? j0 + 1 : p.H - 1;
    const float fx = u - (float)i0, fy = v - (float)j0;
    const int a = j0 * p.W + i0, b = j0 * p.W + i1, c = j1 * p.W + i0, d = j1 * p.W + i1;
    const float w00 = (1.0f - fx) * (1.0f - fy), w10 = fx * (1.0f - fy), w01 = (1.0f - fx) * fy, w11 = fx * fy;
    return smf3(M0[a] * w00 + M0[b] * w10 + M0[c] * w01 + M0[d] * w11, M1[a] * w00 + M1[b] * w10 + M1[c] * w01 + M1[d] * w11,
                M2[a] * w00 + M2[b] * w10 + M2[c] * w01 + M2[d] * w11);
}

// A tube's raster: the picture as mLines scan lines (each read as the signal carries it, a little soft along the line),
// each a Gaussian beam whose width grows with its brightness, so bright lines fill the gaps and dark lines thin out;
// a flat field keeps its mean.
SM_FN SMf3 cr_scan(CR_PLANE M0, CR_PLANE M1, CR_PLANE M2, float sx, float sy, SM_CREF(CreativeParams) p) {
    const float H = (float)p.H, L = (float)p.mLines;
    const float ly = (sy + 0.5f) / H * L - 0.5f, l0 = CR_FLOOR(ly), f = ly - l0;
    const float y0 = (l0 + 0.5f) / L * H - 0.5f, y1 = (l0 + 1.5f) / L * H - 0.5f, q = 0.25f * H / L, h = p.mSoft;
    const SMf3 a0 = cr_pic(M0, M1, M2, sx - h, y0 - q, p), a1 = cr_pic(M0, M1, M2, sx + h, y0 + q, p);
    const SMf3 b0 = cr_pic(M0, M1, M2, sx - h, y1 - q, p), b1 = cr_pic(M0, M1, M2, sx + h, y1 + q, p);
    float o[3];
    for (int k = 0; k < 3; ++k) {
        const float ca = 0.5f * (cr_at(a0, k) + cr_at(a1, k)), cb = 0.5f * (cr_at(b0, k) + cr_at(b1, k));
        const float sa = p.mBeamMin + (p.mBeamMax - p.mBeamMin) * SM_SQRT(SM_MAX(ca, 0.0f));
        const float sb = p.mBeamMin + (p.mBeamMax - p.mBeamMin) * SM_SQRT(SM_MAX(cb, 0.0f));
        // each beam of area 1; the lines beyond (l0 - 1, l0 + 2) taken as like their neighbours
        const float ia = 0.5f / (sa * sa), ib = 0.5f / (sb * sb), g = 1.0f - f;
        o[k] = 0.39894228f * (ca / sa * (SM_EXP(-f * f * ia) + SM_EXP(-(1.0f + f) * (1.0f + f) * ia))
                              + cb / sb * (SM_EXP(-g * g * ib) + SM_EXP(-(1.0f + g) * (1.0f + g) * ib)));
    }
    return smf3(o[0], o[1], o[2]);
}

// The phosphors behind the mask: red, green and blue side by side (mean 1 over a triad); a slot mask also breaks
// the columns into short slots, offset by half on every other triad.
SM_FN SMf3 cr_phosphor(SMf3 c, int x, int y, SM_CREF(CreativeParams) p) {
    const float t = (float)x / p.mPitch;
    float m[3];
    for (int k = 0; k < 3; ++k) m[k] = 1.0f + p.mMask * CR_COS(6.2831853f * (t - (float)k / 3.0f));
    float s = 1.0f;
    if (p.mMaskType == 1) {
        const float col = CR_FLOOR(t);
        const float yy = (float)y / (1.5f * p.mPitch) + 0.5f * (col - 2.0f * CR_FLOOR(0.5f * col));
        const float bar = 0.5f + 0.5f * CR_COS(6.2831853f * yy);
        s = (1.0f - p.mMask * bar * bar * bar * bar) / (1.0f - p.mMask * 0.2734375f);
    }
    return smf3(c.x * m[0] * s, c.y * m[1] * s, c.z * m[2] * s);
}

// A handheld console's LCD: the picture cropped to the screen's aspect at the centre, read once per LCD pixel,
// brought to four shades with a 4x4 ordered dither, the lighter unlit panel between the pixels; around it the
// bezel and the case. Code values.
SM_FN SMf3 cr_lcd(CR_PLANE M0, CR_PLANE M1, CR_PLANE M2, int x, int y, SM_CREF(CreativeParams) p) {
    const float W = (float)p.W, H = (float)p.H, cols = (float)p.mCols, rows = (float)p.mRows;
    const float x0 = 0.5f * (W - p.mScreenW), y0 = 0.5f * (H - p.mScreenH);
    const float lx = ((float)x + 0.5f - x0) / p.mScreenW * cols, ly = ((float)y + 0.5f - y0) / p.mScreenH * rows;
    if (lx < 0.0f || ly < 0.0f || lx >= cols || ly >= rows) {
        const float fx = (float)x + 0.5f, fy = (float)y + 0.5f, b = p.mBezel;
        const bool bezel = fx >= x0 - b && fx < x0 + p.mScreenW + b && fy >= y0 - b && fy < y0 + p.mScreenH + b;
        return bezel ? smf3(p.mBezelTint[0], p.mBezelTint[1], p.mBezelTint[2])
                     : smf3(p.mBodyTint[0], p.mBodyTint[1], p.mBodyTint[2]);
    }
    const int i = (int)lx, j = (int)ly;
    const float cw = SM_MIN(W, H * cols / rows);   // the source width the screen shows
    const float sx = 0.5f * W + (((float)i + 0.5f) / cols - 0.5f) * cw - 0.5f, sy = ((float)j + 0.5f) / rows * H - 0.5f;
    const float q = 0.25f * H / rows;
    const SMf3 a = cr_pic(M0, M1, M2, sx - q, sy - q, p), b = cr_pic(M0, M1, M2, sx + q, sy - q, p);
    const SMf3 c = cr_pic(M0, M1, M2, sx - q, sy + q, p), d = cr_pic(M0, M1, M2, sx + q, sy + q, p);
    const float lin = 0.25f * (0.2126f * (a.x + b.x + c.x + d.x) + 0.7152f * (a.y + b.y + c.y + d.y)
                               + 0.0722f * (a.z + b.z + c.z + d.z));
    const float Y = sm_encode(sm_clamp(lin, 0.0f, 1.0f), CR_GAMMA24);   // shades are spaced in lightness
    const int bx = i & 3, by = j & 3, bxy = bx ^ by;
    const int bayer = (bxy & 1) * 8 + (by & 1) * 4 + (bxy & 2) + ((by & 2) >> 1);   // 0..15
    const float th = ((float)bayer + 0.5f) / 16.0f - 0.5f;
    const int lev = (int)sm_clamp(CR_FLOOR(Y * 3.0f + 0.5f + p.mDither * th), 0.0f, 3.0f);
    const float gx = lx - (float)i, gy = ly - (float)j;
    const float gap = p.mGrid * SM_MAX(cr_smooth(0.82f, 0.96f, gx), cr_smooth(0.82f, 0.96f, gy));
    float o[3];
    for (int k = 0; k < 3; ++k) o[k] = p.mPalette[lev * 3 + k] + (p.mUnlit[k] - p.mPalette[lev * 3 + k]) * gap;
    return smf3(o[0], o[1], o[2]);
}

// The medium at (x, y), as code values (Gamma 2.4): geometry (curved glass, unsteadiness), the screen's edge, the
// raster or the lens, phosphors, the glow of glass or lens (grid of the picture's luminance; read only when mGlow > 0),
// paper fibres, light falloff and flicker, then black (flare, Dmax) and white (lamp, paper).
// The device's own response to a Rec.709 picture c (display-linear, 1 = reference white): colourfulness (mSat; the
// Hunt effect of a dimmer screen, or a set turned up), a gamut that runs out (mChroma: paper), then the tone of its
// light. mTone is the device's exponent on the signal over the reference 2.4, times the surround's (Bartleson-
// Breneman: a dark room flattens what it shows); mCutoff the black of the device itself, BT.1886-style: it lifts
// the shadows, not the whites. Identity at mSat 1, mChroma 0, mTone 1, mCutoff 0.
SM_FN SMf3 cr_device(SMf3 c, SM_CREF(CreativeParams) p) {
    const float Y = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
    float k = p.mSat;
    if (p.mChroma > 0.0f) {   // chroma relative to Y, compressed softly from 60% of the limit
        const float d = SM_MAX(SM_MAX(SM_ABS(c.x - Y), SM_ABS(c.y - Y)), SM_ABS(c.z - Y)) * k / SM_MAX(Y, 1e-4f);
        const float t = 0.6f * p.mChroma;
        if (d > t) k *= (t + (d - t) / (1.0f + (d - t) / (p.mChroma - t))) / d;
    }
    float o[3] = { Y + k * (c.x - Y), Y + k * (c.y - Y), Y + k * (c.z - Y) };
    if (p.mTone != 1.0f || p.mCutoff > 0.0f) {
        const float g = 2.4f * p.mTone, lb = SM_POW(p.mCutoff, 1.0f / g);
        const float a = SM_POW(1.0f - lb, g), b = lb / (1.0f - lb);
        for (int i = 0; i < 3; ++i) {
            const float v = SM_POW(SM_MAX(o[i], 0.0f), 1.0f / 2.4f);   // the signal the device receives
            o[i] = a * SM_POW(SM_MAX(v + b, 0.0f), g);
        }
    }
    return smf3(o[0], o[1], o[2]);
}

SM_FN SMf3 cr_medium_at(CR_PLANE M0, CR_PLANE M1, CR_PLANE M2, CR_PLANE glow, int x, int y, SM_CREF(CreativeParams) p) {
    if (p.medium == CR_MEDIUM_LCD) return cr_lcd(M0, M1, M2, x, y, p);
    const float W = (float)p.W, H = (float)p.H;
    const float u = ((float)x + 0.5f) / W * 2.0f - 1.0f, v = ((float)y + 0.5f) / H * 2.0f - 1.0f;
    const float a2 = (W / H) * (W / H);
    const float r2 = (u * u * a2 + v * v) / (a2 + 1.0f);   // 0 at the centre, 1 at the corners
    const float g = 1.0f + p.mCurve * r2;                  // curved glass: the picture bulges, its edges recede
    const float cu = u * g, cv = v * g, zu = cu / p.mZoom, zv = cv / p.mZoom;   // overscan: the edges hidden
    const float qx = SM_ABS(cu) * 0.5f * W - (0.5f * W - p.mCorner), qy = SM_ABS(cv) * 0.5f * H - (0.5f * H - p.mCorner);
    const float ox = SM_MAX(qx, 0.0f), oy = SM_MAX(qy, 0.0f);
    const float edge = SM_SQRT(ox * ox + oy * oy) + SM_MIN(SM_MAX(qx, qy), 0.0f) - p.mCorner;   // px, < 0 inside
    const float inside = 1.0f - cr_smooth(-p.mEdge, 0.0f, edge);
    if (inside <= 0.0f) return smf3(0.0f, 0.0f, 0.0f);
    const float sx = (zu + 1.0f) * 0.5f * W - 0.5f + p.mShiftX, sy = (zv + 1.0f) * 0.5f * H - 0.5f + p.mShiftY;
    SMf3 c;
    if (p.mLines > 0) {
        c = cr_phosphor(cr_scan(M0, M1, M2, sx, sy, p), x, y, p);
    } else if (p.mSoft > 0.0f) {   // a projection lens, softer towards the edges
        const float h = p.mSoft * (0.4f + r2);
        const SMf3 a = cr_pic(M0, M1, M2, sx - h, sy - h, p), b = cr_pic(M0, M1, M2, sx + h, sy - h, p);
        const SMf3 d = cr_pic(M0, M1, M2, sx - h, sy + h, p), e = cr_pic(M0, M1, M2, sx + h, sy + h, p);
        c = smf3(0.25f * (a.x + b.x + d.x + e.x), 0.25f * (a.y + b.y + d.y + e.y), 0.25f * (a.z + b.z + d.z + e.z));
    } else {
        c = cr_pic(M0, M1, M2, sx, sy, p);
    }
    if (p.mGlow > 0.0f) {   // light scattered in the glass or the lens around bright areas (neutral)
        const float h = p.mGlow * cr_grid(glow, p.mGW, p.mGH, p.mGS, (int)sm_clamp(sx, 0.0f, W - 1.0f),
                                          (int)sm_clamp(sy, 0.0f, H - 1.0f));
        c = smf3(c.x * (1.0f - p.mGlow) + h, c.y * (1.0f - p.mGlow) + h, c.z * (1.0f - p.mGlow) + h);
    }
    c = cr_device(c, p);
    float light = p.mGain * (1.0f - p.mFalloff * r2) * inside;
    if (p.mTexture > 0.0f) {   // paper fibres: still, the same on every frame
        const float t = (1.5f * H / 1080.0f) + 0.5f;
        light *= 1.0f + p.mTexture * (0.020f * cr_value((float)x / t, (float)y / t, 977)
                                      + 0.010f * cr_value((float)x / (6.0f * t), (float)y / (6.0f * t), 978));
    }
    float o[3];
    for (int k = 0; k < 3; ++k)
        o[k] = sm_encode(sm_clamp(p.mBlack * inside + (p.mWhite[k] - p.mBlack) * cr_at(c, k) * light, 0.0f, 1.0f),
                         CR_GAMMA24);
    return smf3(o[0], o[1], o[2]);
}

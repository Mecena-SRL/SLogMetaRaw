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
#define CR_FLOOR floor
#define CR_CEIL ceil
#define CR_SIN precise::sin
#define CR_COS precise::cos
typedef uint CrU;
#else
#define CR_PLANE const float*
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
    // duplicates and print: grain of the intermediates (stops), of the print (density)
    float genGrain, genSize, printGrain, printGrainSize, genSoft;   // genSoft: the duplicates' blur (px)
    // viewing medium, on the finished picture (display-linear Rec.709)
    int medium, mLines, mMaskType, mCols, mRows;
    float mCurve, mBeamMin, mBeamMax, mMask, mPitch, mFalloff, mGain, mBlack, mWhite[3], mShiftX, mShiftY, mSoft;
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
        return cr_tap(src, cx + (u - cx) * p.recScale[k] + p.recX[k], cy + (v - cy) * p.recScale[k] + p.recY[k], p);
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

// The three records at (x, y). When they share frame and place (p.shared), the source is read and decoded once.
SM_FN SMf3 cr_records(CR_PLANE s0, CR_PLANE s1, CR_PLANE s2, int x, int y, SM_CREF(CreativeParams) p) {
    const SMf3 c0 = cr_source(s0, 0, x, y, p);
    const SMf3 c1 = p.shared != 0 ? c0 : cr_source(s1, 1, x, y, p);
    const SMf3 c2 = p.shared != 0 ? c0 : cr_source(s2, 2, x, y, p);
    return smf3(cr_expose(c0, 0, x, y, p), cr_expose(c1, 1, x, y, p), cr_expose(c2, 2, x, y, p));
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
SM_FN SMf3 cr_medium_at(CR_PLANE M0, CR_PLANE M1, CR_PLANE M2, CR_PLANE glow, int x, int y, SM_CREF(CreativeParams) p) {
    if (p.medium == CR_MEDIUM_LCD) return cr_lcd(M0, M1, M2, x, y, p);
    const float W = (float)p.W, H = (float)p.H;
    const float u = ((float)x + 0.5f) / W * 2.0f - 1.0f, v = ((float)y + 0.5f) / H * 2.0f - 1.0f;
    const float a2 = (W / H) * (W / H);
    const float r2 = (u * u * a2 + v * v) / (a2 + 1.0f);   // 0 at the centre, 1 at the corners
    const float g = 1.0f + p.mCurve * r2;                  // curved glass: the picture bulges, its edges recede
    const float cu = u * g, cv = v * g;
    const float qx = SM_ABS(cu) * 0.5f * W - (0.5f * W - p.mCorner), qy = SM_ABS(cv) * 0.5f * H - (0.5f * H - p.mCorner);
    const float ox = SM_MAX(qx, 0.0f), oy = SM_MAX(qy, 0.0f);
    const float edge = SM_SQRT(ox * ox + oy * oy) + SM_MIN(SM_MAX(qx, qy), 0.0f) - p.mCorner;   // px, < 0 inside
    const float inside = 1.0f - cr_smooth(-p.mEdge, 0.0f, edge);
    if (inside <= 0.0f) return smf3(0.0f, 0.0f, 0.0f);
    const float sx = (cu + 1.0f) * 0.5f * W - 0.5f + p.mShiftX, sy = (cv + 1.0f) * 0.5f * H - 0.5f + p.mShiftY;
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

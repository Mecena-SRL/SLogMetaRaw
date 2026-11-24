// SPDX-License-Identifier: GPL-3.0-or-later
// The Noise pipeline on the CPU, free of OpenFX so the tests can run it on their own.
//
// The picture stays in the encoding it arrives in (camera log code values). The noise profile says how much noise
// to expect at each brightness, per wavelet band, for luminance and colour: the filter takes that much and no more.
//   1. temporal (optional): neighbour frames aligned by block matching, averaged with a weight per pixel that drops
//      where the patches differ more than the noise explains (motion, occlusion, cuts)
//   2. spatial: opponent channels (luma + two colour differences), à trous B3 wavelet in 5 bands, each coefficient
//      shrunk by the non-negative garrote against the band's expected noise at that pixel's brightness
#pragma once
#include <cstddef>
#include <functional>
#include <vector>

// parallel(n, fn) runs fn(begin, end) over [0, n), split among threads; fn must only write its own rows.
using NoiseParallel = std::function<void(int n, const std::function<void(int begin, int end)>& fn)>;

const int kNoiseLevels = 5;   // wavelet bands: 1, 2, 4, 8, 16 px (fine to coarse)
const int kNoiseBins = 16;    // brightness bins of a profile, over the mean code value 0..1

// Expected noise, as standard deviation in code values (0..1), of each band of the opponent channels
// (Y = (R+G+B)/√3, C1 = (R-B)/√2, C2 = (R-2G+B)/√6, orthonormal) for each brightness bin.
struct NoiseProfile
{
    float luma[kNoiseBins][kNoiseLevels] = {};
    float chroma[kNoiseBins][kNoiseLevels] = {};
    float temporalCorr = 0.0f;   // share of the noise that repeats in the next frame (Long GOP), 0..1
    bool valid = false;
};

enum NoiseView { kViewOutput = 0, kViewRemoved = 1, kViewNoiseMap = 2, kViewTemporal = 3 };

struct NoiseControls
{
    float luma = 1.0f, chroma = 1.0f;           // strength: 0 keeps the channel, 1 = threshold 1.2 σ of the band (neighbourhood energy)
    float fine = 1.0f, medium = 1.0f, coarse = 1.0f;   // per band group: levels 0-1, 2, 3-4
    float shadows = 0.0f;                       // extra strength towards the blacks, 0..1
    float preserve = 0.0f;                      // share of the removed fine luma put back, 0..1
    float skin = 0.0f;                          // skin tones keep this share of their fine luma texture, 0..1
    float temporal = 1.0f;                      // temporal strength, 0 = only the current frame
    int view = kViewOutput;
    int box[4] = { -1, -1, -1, -1 };            // sample box drawn on the output, pixels x0 y0 x1 y1 (x0 < 0: none)
};

// The outline of the sample box: yellow and black dashes, t pixels wide. Same rule in Noise.metal (nz_final).
inline bool noiseBoxEdge(const int (&box)[4], int x, int y, int t, float (&rgb)[3])
{
    if (box[0] < 0) return false;
    const bool outer = x >= box[0] - t && x <= box[2] + t && y >= box[1] - t && y <= box[3] + t;
    const bool inner = x > box[0] && x < box[2] && y > box[1] && y < box[3];
    if (!outer || inner) return false;
    const bool dash = ((x + y) >> 3) & 1;
    rgb[0] = dash ? 1.0f : 0.0f;
    rgb[1] = dash ? 0.85f : 0.0f;
    rgb[2] = 0.0f;
    return true;
}

// One frame of float RGBA, rows of rowFloats floats (>= 4 * width).
struct NoiseFrame
{
    const float* px = nullptr;
    size_t rowFloats = 0;
};

struct NoiseScratch
{
    std::vector<float> plane[3], acc[3], cur, next, band, guide, gc1, gc2, ratio, wsum, wsq, lumaC, boost, skin;   // full size
    std::vector<float> fused;                                                              // RGBA
    std::vector<float> yN, pyr[3], pyrN[3];                                                // motion search
    std::vector<int> vx, vy;
};

// Non-negative garrote of coefficient w on the mean energy e of its neighbourhood, threshold t.
inline float noiseShrink(float w, float e, float t)
{
    const float t2 = t * t;
    return e > t2 ? w * (1.0f - t2 / e) : 0.0f;
}

// Brightness bin of a mean code value, with the fraction towards the next bin.
void noiseBin(float mean, int& bin, float& frac);

// Temporal + spatial denoise of frames[center] into dst. frames holds the window in time order (the centre
// included); a null px marks a neighbour the host could not give. Alpha is copied from the centre frame.
void noiseRenderCPU(int W, int H, const NoiseFrame* frames, int count, int center, const NoiseProfile& profile,
                    const NoiseControls& c, float* dst, size_t dstRowFloats, const NoiseParallel& parallel,
                    NoiseScratch& s);

// Measure the profile of one frame: per brightness bin and band, the noise of the flattest blocks.
// Bins with too few flat blocks are filled from their neighbours; false when the frame has too little flat area.
bool noiseMeasure(int W, int H, NoiseFrame frame, const NoiseParallel& parallel, NoiseScratch& s, NoiseProfile& out);

// The noise of a flat area the user picked (the sample box, like Neat Video): per band, luma and colour, at the area's
// brightness. False when the box is too small or not flat (more texture than noise).
struct NoiseSample
{
    float level = 0.0f;                         // mean brightness of the box (guide scale, 0..1)
    float luma[kNoiseLevels] = {}, chroma[kNoiseLevels] = {};
    bool valid = false;
};
bool noiseSample(int W, int H, NoiseFrame frame, int x0, int y0, int x1, int y1, const NoiseParallel& parallel,
                 NoiseScratch& s, NoiseSample& out);

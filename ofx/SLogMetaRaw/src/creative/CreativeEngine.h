// SPDX-License-Identifier: GPL-3.0-or-later
// The Creative pipeline on the CPU, free of OpenFX so the tests can run it on their own. The Metal kernels
// (metal/CreativeKernels.mm) run the same passes, calling the same functions of math/Creative.h:
//   1. record   source frames -> record planes E (one frame per record: sequential plates read other times), then
//               the video camera's sensor (a tube's memory of the frames before, a CCD's smear down the column)
//   2. glow     E -> halation grid, diffusion grids (box down, Gaussian on the grid)
//   3. output   process: per pixel, curve and image; signal chains: Y Cb Cr planes, their filters, then per pixel
//   3b. transfer the finished picture copied (telecine, tape dub, kinescope, film-out): steps 1-3 again on it
//   4. medium   the finished picture through a projector, a tube, an LCD or on paper (when one is chosen)
#pragma once
#include <cstddef>
#include <functional>
#include <vector>

#include "../../gen/DevelopMath.h"

// parallel(n, fn) runs fn(begin, end) over [0, n), split among threads; fn must only write its own rows.
using CreativeParallel = std::function<void(int n, const std::function<void(int begin, int end)>& fn)>;

struct CreativeScratch
{
    std::vector<float> E[3], V[3], T[4];   // full size: records; Y Cb Cr; filtered luma, its blur, Cb, Cr
    std::vector<float> hal, bl[3], tmp, M;   // M: the finished picture, display-linear, for the viewing medium
    std::vector<float> prev;                 // the previous frame's picture, for an LCD's slow response
    std::vector<float> S;                    // a CCD's smear per column (3 x W)
    std::vector<float> D;                    // a digital camera's planes at its sensor's size
    std::vector<float> xfer;                 // the picture before its transfer (a copy onto tape or film)
    std::vector<float> L[3], St, gl;         // the lens: soft records, streak per line, veiling glare grid
};

// The previous frame, for an LCD's slow response (CreativeParams::mGhost): its own parameters and source frames.
struct CreativePrevious
{
    CreativeParams p;
    const float* src[3];
};

// The frames before this one a pickup tube still remembers (CreativeParams::history of them): 1, 2, 3 frames back.
struct CreativeHistory
{
    const float* src[3];
};

// src[k]: the frame record k is exposed from (RGBA, rows of srcRowFloats floats; the same pointer when the records
// are simultaneous). Alpha comes from src[0]. prev: the previous frame when the medium needs it, else null;
// history: the frames before this one when p.history > 0; transfer: the second pass of a copy
// (creativePrepareTransfer), which then also carries the viewing medium.
void creativeRenderCPU(const CreativeParams& p, const float* const src[3], size_t srcRowFloats, float* dst,
                       size_t dstRowFloats, const CreativeParallel& parallel, CreativeScratch& s,
                       const CreativePrevious* prev = nullptr, const CreativeHistory* history = nullptr,
                       const CreativeParams* transfer = nullptr);

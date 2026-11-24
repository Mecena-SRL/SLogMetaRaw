// SPDX-License-Identifier: GPL-3.0-or-later
// The Creative pipeline on the CPU, free of OpenFX so the tests can run it on their own. The Metal kernels
// (metal/CreativeKernels.mm) run the same passes, calling the same functions of math/Creative.h:
//   1. record   source frames -> record planes E (one frame per record: sequential plates read other times)
//   2. glow     E -> halation grid, diffusion grids (box down, Gaussian on the grid)
//   3. output   process: per pixel, curve and image; signal chains: Y Cb Cr planes, their filters, then per pixel
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
    std::vector<float> hal, bl[3], tmp;
};

// src[k]: the frame record k is exposed from (RGBA, rows of srcRowFloats floats; the same pointer when the records
// are simultaneous). Alpha comes from src[0].
void creativeRenderCPU(const CreativeParams& p, const float* const src[3], size_t srcRowFloats, float* dst,
                       size_t dstRowFloats, const CreativeParallel& parallel, CreativeScratch& s);

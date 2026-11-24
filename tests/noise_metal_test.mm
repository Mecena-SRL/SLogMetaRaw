// SPDX-License-Identifier: GPL-3.0-or-later
// The Noise node's Metal kernels against noiseRenderCPU on synthetic frames (spatial, temporal, views, skin, sample box).
// Prints "MAXDIFF x" (code values) and, with --time, the GPU time of a UHD frame (spatial, then ±2).
#import <Metal/Metal.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <random>
#include <vector>

#include "metal/MetalKernels.h"
#include "src/noise/NoiseEngine.h"

static void serial(int n, const std::function<void(int, int)>& fn) { fn(0, n); }

// A scene in code values, moved by (dx, dy), with white noise: gradient, wall, bright box, texture, skin-like patch.
static std::vector<float> scene(int W, int H, int rowPixels, float dx, float dy, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> n(0.0f, 0.02f);
    std::vector<float> img((size_t)rowPixels * H * 4, 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float fx = x - dx, fy = y - dy;
            float v = 0.15f + 0.6f * fx / W;
            if (fy > H * 0.55f) v = 0.2f;
            if (fx > W * 0.3f && fx < W * 0.5f && fy > H * 0.2f && fy < H * 0.45f) v = 0.75f;
            v += 0.04f * sinf(fx * 0.35f) * cosf(fy * 0.3f);
            const bool skin = fx > W * 0.6f && fx < W * 0.8f && fy > H * 0.1f && fy < H * 0.4f;
            const float tint[3] = { skin ? 1.15f : 1.0f, skin ? 0.95f : 0.95f, skin ? 0.8f : 0.85f };
            float* p = &img[((size_t)y * rowPixels + x) * 4];
            for (int c = 0; c < 3; ++c) p[c] = v * tint[c] + n(rng);
            p[3] = 0.25f + 0.5f * (x & 1);
        }
    return img;
}

static NoiseProfile flatProfile(float sigma)
{
    NoiseProfile p;
    const float white[kNoiseLevels] = { 0.889f, 0.200f, 0.086f, 0.041f, 0.020f };
    for (int b = 0; b < kNoiseBins; ++b)
        for (int j = 0; j < kNoiseLevels; ++j) {
            const float k = 1.0f + 0.5f * (kNoiseBins - 1 - b) / (kNoiseBins - 1);   // more in the shadows
            p.luma[b][j] = sigma * white[j] * k;
            p.chroma[b][j] = sigma * white[j] * k;
        }
    p.temporalCorr = 0.1f;
    p.valid = true;
    return p;
}

int main(int argc, char** argv)
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) { printf("NO_DEVICE\n"); return 2; }
    id<MTLCommandQueue> queue = [device newCommandQueue];
    const bool timing = argc > 1 && !strcmp(argv[1], "--time");
    const int W = timing ? 3840 : 203, H = timing ? 2160 : 131, row = W + 5;   // odd sizes, padded rows
    const NoiseProfile profile = flatProfile(0.02f);
    std::vector<std::vector<float>> imgs;
    std::vector<id<MTLBuffer>> bufs;
    for (int k = 0; k < 5; ++k) {
        imgs.push_back(scene(W, H, row, 3.0f * (k - 2), 2.0f * (k - 2), 40 + k));
        bufs.push_back([device newBufferWithBytes:imgs.back().data() length:imgs.back().size() * 4
                                          options:MTLResourceStorageModeShared]);
    }
    id<MTLBuffer> out = [device newBufferWithLength:imgs[0].size() * 4 options:MTLResourceStorageModeShared];
    auto sync = [&]() {
        id<MTLCommandBuffer> c = [queue commandBuffer];
        [c commit];
        [c waitUntilCompleted];
    };
    if (timing) {
        for (int radius : { 0, 1, 2 }) {
            std::vector<NoiseFrame> gf;
            for (int k = 2 - radius; k <= 2 + radius; ++k)
                gf.push_back({ (const float*)(__bridge void*)bufs[k], (size_t)row * 4 });
            NoiseControls c;
            c.skin = 0.3f;
            c.preserve = 0.1f;
            if (!RunNoiseKernels((__bridge void*)queue, W, H, gf.data(), (int)gf.size(), radius, profile, c,
                                 (float*)(__bridge void*)out, row)) {
                printf("KERNEL_FAILED\n");
                return 1;
            }
            sync();
            // the median of 7 frames, each waited for: the CI runners share their GPU, a batch mean wanders
            std::vector<double> ms;
            for (int r = 0; r < 7; ++r) {
                const auto t0 = std::chrono::steady_clock::now();
                RunNoiseKernels((__bridge void*)queue, W, H, gf.data(), (int)gf.size(), radius, profile, c,
                                (float*)(__bridge void*)out, row);
                sync();
                ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            }
            std::sort(ms.begin(), ms.end());
            printf("UHD radius %d: %.1f ms (min %.1f)\n", radius, ms[3], ms[0]);
        }
        return 0;
    }
    double worst = 0.0;
    NoiseScratch scratch;
    std::vector<float> cpu(imgs[0].size());
    for (int k = 0; k < 10; ++k) {
        NoiseControls c;
        int radius = 0;
        switch (k) {
        case 0: c.temporal = 0.0f; break;
        case 1: c.temporal = 0.0f; c.skin = 0.6f; c.preserve = 0.3f; c.shadows = 0.5f; break;
        case 2: radius = 2; break;
        case 3: radius = 1; c.temporal = 0.5f; c.skin = 0.3f; break;
        case 4: radius = 2; c.view = kViewRemoved; break;
        case 5: radius = 2; c.view = kViewNoiseMap; break;
        case 6: radius = 2; c.view = kViewTemporal; break;
        case 7: radius = 2; c.luma = 0.0f; c.chroma = 2.0f; c.fine = 0.5f; c.coarse = 2.0f; break;
        case 8: radius = 2; c.luma = 0.0f; c.chroma = 0.0f; break;
        case 9: radius = 1; c.box[0] = 10; c.box[1] = 7; c.box[2] = W / 2; c.box[3] = H / 2; break;   // sample box
        }
        std::vector<NoiseFrame> cf, gf;
        for (int f = 2 - radius; f <= 2 + radius; ++f) {
            const bool missing = k == 3 && f == 1;   // a neighbour the host could not give
            cf.push_back({ missing ? nullptr : imgs[f].data(), (size_t)row * 4 });
            gf.push_back({ missing ? nullptr : (const float*)(__bridge void*)bufs[f], (size_t)row * 4 });
        }
        std::fill(cpu.begin(), cpu.end(), 0.0f);
        noiseRenderCPU(W, H, cf.data(), (int)cf.size(), radius, profile, c, cpu.data(), (size_t)row * 4, serial, scratch);
        memset(out.contents, 0, out.length);
        if (!RunNoiseKernels((__bridge void*)queue, W, H, gf.data(), (int)gf.size(), radius, profile, c,
                             (float*)(__bridge void*)out, row)) {
            printf("KERNEL_FAILED %d\n", k);
            return 1;
        }
        sync();
        const float* gpu = (const float*)out.contents;
        double here = 0.0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int ch = 0; ch < 4; ++ch) {
                    const size_t i = ((size_t)y * row + x) * 4 + ch;
                    if (ch == 3 && gpu[i] != imgs[2][i]) { printf("ALPHA_MISMATCH %d\n", k); return 1; }
                    here = std::max(here, fabs((double)cpu[i] - gpu[i]));
                }
        fprintf(stderr, "case %d: %.8f\n", k, here);
        worst = std::max(worst, here);
    }
    printf("MAXDIFF %.8f\n", worst);
    return 0;
}

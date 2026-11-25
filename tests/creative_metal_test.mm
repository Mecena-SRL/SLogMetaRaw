// SPDX-License-Identifier: GPL-3.0-or-later
// The Creative node's Metal kernels against its CPU pipeline on a synthetic clip, every process.
// Prints "MAXDIFF x" (code values; a difference negligible in light is not counted: near black the display curve
// turns a last-bit difference of the transcendental functions into a large code step, as in the other Metal tests).
#import <Metal/Metal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "gen/DevelopMath.h"
#include "metal/MetalKernels.h"
#include "src/creative/CreativeEngine.h"
#include "src/creative/CreativeModel.h"

// frame f of a moving scene: a ramp, coloured bars that slide, a bright window and a NaN
static void frame(std::vector<float>& img, int W, int H, int f)
{
    img.assign((size_t)W * H * 4, 1.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float ev = -6.0f + 11.0f * x / W;
            if (x > W * 0.6f && x < W * 0.8f && y > H * 0.55f && y < H * 0.85f) ev = 5.0f;
            ev += 0.1f * sinf(x * 1.7f) * cosf(y * 1.3f);
            const bool bar = ((x + 4 * f) / 12) % 2;
            const float tint[3] = { bar ? 1.0f : 0.6f, 0.85f, bar ? 0.6f : 1.0f };
            for (int c = 0; c < 3; ++c) img[((size_t)y * W + x) * 4 + c] = sm_encode(0.18f * exp2f(ev) * tint[c], 9);
            img[((size_t)y * W + x) * 4 + 3] = 0.5f + 0.5f * (float)((x + y) % 2);
        }
    img[(size_t)(5 * W + 7) * 4] = NAN;
}

int main()
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) { printf("NO_DEVICE\n"); return 2; }
    id<MTLCommandQueue> queue = [device newCommandQueue];
    const int W = 320, H = 180;
    std::vector<float> img[3];
    id<MTLBuffer> in[3];
    for (int f = 0; f < 3; ++f) {
        frame(img[f], W, H, f);
        in[f] = [device newBufferWithBytes:img[f].data() length:img[f].size() * 4 options:MTLResourceStorageModeShared];
    }
    id<MTLBuffer> out = [device newBufferWithLength:img[0].size() * 4 options:MTLResourceStorageModeShared];
    CreativeParallel serial = [](int n, const std::function<void(int, int)>& fn) { fn(0, n); };
    double worst = 0.0;
    for (int k = 0; k < kCrProcessCount + 10; ++k) {
        CreativeControls c;
        c.process = k % kCrProcessCount;
        c.interval = 1;
        c.registration = 4;
        c.bloom = k % 2 ? 0.6 : 0.0;
        c.halation = 2;
        c.dust = 0.6;
        c.scratches = 0.6;
        c.weave = 0.8;
        c.aberration = 0.7;
        c.vignette = 1;
        c.shadowAmount = 0.5;
        c.highAmount = 0.4;
        c.age = 0.4;
        c.silver = 0.5;
        c.generation = k % kCrGenerationCount;
        c.printGrain = 2;
        c.medium = k % kCrMediumCount;   // the console's four shades are thresholds: a rounding apart flips one
        if (c.medium == 6 || c.medium >= 12) c.medium = 3;   // the home consoles too: palettes and bit depths
        if (k == kCrProcessCount) { c.process = kCrColourNegative; c.output = 1; c.print = 0; }
        if (k == kCrProcessCount + 1) { c.process = kCrAnalog; c.analog = 2; c.wear = 1; c.heads = 1; }
        if (k == kCrProcessCount + 2) { c.process = kCrAnalog; c.analog = 1; c.sensor = 5; c.comet = 2; }   // tube
        if (k == kCrProcessCount + 3) { c.process = kCrAnalog; c.analog = 9; c.sensor = 7; c.smear = 3; }   // CCD
        if (k == kCrProcessCount + 4) { c.device = kCrDevPhone; c.phoneCamera = 5; c.compression = 2; }   // codec, small
        if (k == kCrProcessCount + 5) { c.device = kCrDevCinema; c.cinemaCamera = 2; }   // line skipping
        if (k == kCrProcessCount + 6) { c.process = kCrColourNegative; c.transfer = 1; c.medium = 3; }   // to VHS
        if (k == kCrProcessCount + 7) { c.process = kCrAnalog; c.analog = 1; c.transfer = 4; }   // kinescope
        if (k == kCrProcessCount + 8) { c.lens = 6; c.lensLoaded = 6; c.streak = 1; c.glare = 4; }   // anamorphic
        if (k == kCrProcessCount + 9) { c.process = kCrAnalog; c.lens = 13; }   // Petzval: soft, swirl, veil
        double t[3];
        creativeRecordTimes(c, t);
        const float* cpuIn[3];
        const float* gpuIn[3];
        for (int r = 0; r < 3; ++r) {
            const int f = std::min(2, std::max(0, (int)lround(t[r])));
            cpuIn[r] = img[f].data();
            gpuIn[r] = (const float*)(__bridge void*)in[f];
        }
        // a pickup tube's frames before: the other two pictures
        CreativeHistory hist = { { img[1].data(), img[2].data(), img[1].data() } };
        const float* gpuHist[3] = { (const float*)(__bridge void*)in[1], (const float*)(__bridge void*)in[2],
                                    (const float*)(__bridge void*)in[1] };
        CreativeParams p;
        creativePrepare(p, c, W, H, 11, 8, 9, 3);
        std::vector<float> cpu(img[0].size());
        CreativeScratch scratch;
        CreativeParams copy;
        const bool transfer = creativePrepareTransfer(copy, c, W, H, 11);
        creativeRenderCPU(p, cpuIn, (size_t)W * 4, cpu.data(), (size_t)W * 4, serial, scratch, nullptr, &hist,
                          transfer ? &copy : nullptr);
        p.rowPixels = p.dstRowPixels = W;
        copy.rowPixels = copy.dstRowPixels = W;
        if (!RunCreativeKernels((__bridge void*)queue, p, gpuIn, (float*)(__bridge void*)out, nullptr, nullptr,
                                gpuHist, transfer ? &copy : nullptr)) {
            printf("KERNEL_FAILED %d\n", k);
            return 1;
        }
        id<MTLCommandBuffer> sync = [queue commandBuffer];
        [sync commit];
        [sync waitUntilCompleted];
        const float* gpu = (const float*)out.contents;
        double here = 0.0;
        size_t at = 0;
        const int outGamma = p.output == CR_OUT_LOG ? 9 : CR_GAMMA24;   // the encoding written
        for (size_t i = 0; i < cpu.size(); ++i) {
            if (i % 4 == 3) { if (gpu[i] != img[0][i]) { printf("ALPHA_MISMATCH %d\n", k); return 1; } continue; }
            if (std::isnan(cpu[i]) && std::isnan(gpu[i])) continue;
            if (fabsf(sm_decode(cpu[i], outGamma) - sm_decode(gpu[i], outGamma)) <= 2e-6f) continue;
            const double d = fabs((double)cpu[i] - gpu[i]);
            if (d > here) { here = d; at = i; }
        }
        if (here > 2e-4)
            fprintf(stderr, "case %d (process %d): %.6f at x %zu y %zu (cpu %.6f gpu %.6f)\n", k, c.process, here,
                    (at / 4) % W, (at / 4) / W, cpu[at], gpu[at]);
        worst = std::max(worst, here);
    }
    printf("MAXDIFF %.8f\n", worst);
    return 0;
}

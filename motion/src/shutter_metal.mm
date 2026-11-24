// SPDX-License-Identifier: GPL-3.0-or-later
// Metal driver of the shared motion blur engine. The kernels below are thin
// wrappers around blur_core.inc, the same code the CPU path runs and the tests
// verify on every platform.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "blur.hpp"
#include <cstdio>
#include <mutex>
#include <memory>
#include <string>
namespace motion {
namespace {
#define GM_SOURCE(...) #__VA_ARGS__
const char* coreSource =
#include "blur_core.inc"
;
#undef GM_SOURCE
const char* prelude = R"MSL(
#include <metal_stdlib>
using namespace metal;
#define GM_DEV device
#define GM_DEVC device const
#define GM_CREF(T) constant T&
)MSL";
const char* kernelSource = R"MSL(
kernel void kPrepare(device const float* src [[buffer(0)]], device float* lin [[buffer(1)]], device float* luma [[buffer(2)]],
                     constant GmParams& p [[buffer(3)]], uint2 pos [[thread_position_in_grid]]) {
    if (int(pos.x) >= p.width || int(pos.y) >= p.height) return;
    gm_prepare(src, lin, luma, p.width, p.curve, int(pos.x), int(pos.y));
}
kernel void kReduce(device float* pyr [[buffer(0)]], constant GmGeometry& g [[buffer(1)]], constant int& level [[buffer(2)]],
                    uint2 pos [[thread_position_in_grid]]) {
    if (int(pos.x) >= g.widths[level] || int(pos.y) >= g.heights[level]) return;
    gm_reduce(pyr, g, level, int(pos.x), int(pos.y));
}
kernel void kFlow(device const float* A [[buffer(0)]], device const float* B [[buffer(1)]], device const float* coarse [[buffer(2)]],
                  device float* raw [[buffer(3)]], constant GmGeometry& g [[buffer(4)]], constant GmCamera& cam [[buffer(5)]],
                  constant int& level [[buffer(6)]], uint2 pos [[thread_position_in_grid]]) {
    int fw = g.fieldW[level];
    if (int(pos.x) >= fw || int(pos.y) >= g.fieldH[level]) return;
    GmVec v = gm_flowNode(A, B, coarse, g, cam, level, int(pos.x), int(pos.y));
    int k = g.fieldOffsets[level] + 2 * (int(pos.y) * fw + int(pos.x));
    raw[k] = v.x; raw[k + 1] = v.y;
}
kernel void kSmooth(device const float* A [[buffer(0)]], device const float* B [[buffer(1)]], device const float* raw [[buffer(2)]],
                    device float* smooth [[buffer(3)]], constant GmGeometry& g [[buffer(4)]], constant int& level [[buffer(5)]],
                    uint2 pos [[thread_position_in_grid]]) {
    int fw = g.fieldW[level];
    if (int(pos.x) >= fw || int(pos.y) >= g.fieldH[level]) return;
    GmVec v = gm_smoothNode(A, B, raw, g, level, int(pos.x), int(pos.y));
    int k = g.fieldOffsets[level] + 2 * (int(pos.y) * fw + int(pos.x));
    smooth[k] = v.x; smooth[k + 1] = v.y;
}
kernel void kCross(device const float* C [[buffer(0)]], device const float* P [[buffer(1)]], device const float* N [[buffer(2)]],
                   device const float* fieldP [[buffer(3)]], device const float* fieldN [[buffer(4)]],
                   device float* outP [[buffer(5)]], device float* outN [[buffer(6)]], constant GmGeometry& g [[buffer(7)]],
                   uint2 pos [[thread_position_in_grid]]) {
    int fw = g.fieldW[0];
    if (int(pos.x) >= fw || int(pos.y) >= g.fieldH[0]) return;
    GmPair r = gm_crossNode(C, P, N, fieldP, fieldN, g, int(pos.x), int(pos.y));
    int k = 2 * (int(pos.y) * fw + int(pos.x));
    outP[k] = r.p.x; outP[k + 1] = r.p.y; outN[k] = r.n.x; outN[k + 1] = r.n.y;
}
kernel void kTile(device const float* in [[buffer(0)]], device float* out [[buffer(1)]], constant GmParams& p [[buffer(2)]],
                  constant int& side [[buffer(3)]], constant int& horizontal [[buffer(4)]], uint2 pos [[thread_position_in_grid]]) {
    if (int(pos.x) >= p.fieldW || int(pos.y) >= p.fieldH) return;
    GmVec v = gm_tileMax(in, p, side, int(pos.x), int(pos.y), horizontal);
    int k = 2 * (int(pos.y) * p.fieldW + int(pos.x));
    out[k] = v.x; out[k + 1] = v.y;
}
kernel void kIntegrate(device const float* src [[buffer(0)]], device const float* linC [[buffer(1)]],
                       device const float* linP [[buffer(2)]], device const float* linN [[buffer(3)]],
                       device const float* lumaC [[buffer(4)]], device const float* lumaP [[buffer(5)]], device const float* lumaN [[buffer(6)]],
                       device const float* fieldP [[buffer(7)]], device const float* fieldN [[buffer(8)]],
                       device const float* tileP [[buffer(9)]], device const float* tileN [[buffer(10)]],
                       constant GmParams& p [[buffer(11)]], device float* out [[buffer(12)]], uint2 pos [[thread_position_in_grid]]) {
    if (int(pos.x) >= p.width || int(pos.y) >= p.height) return;
    gm_integrate(src, linC, linP, linN, lumaC, lumaP, lumaN, fieldP, fieldN, tileP, tileN, p, int(pos.x), int(pos.y),
                 out + 4 * (int(pos.y) * p.width + int(pos.x)));
}
)MSL";
enum Kernel { Prepare, Reduce, Flow, Smooth, Cross, Tile, Integrate, KernelCount };
enum Slot { Src0, Src1, Src2, Lin0, Lin1, Lin2, Luma0, Luma1, Luma2, Raw0, Raw1, Field0, Field1, Cross0, Cross1, Tmp0, Tmp1, Tile0, Tile1, Out, SlotCount };
struct Scratch {
    id<MTLBuffer> slots[SlotCount]{};
    int width = 0, height = 0;
    bool draft = false;
};
struct Engine {
    std::mutex cacheMutex;
    std::unique_ptr<Scratch> idle;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLComputePipelineState> kernels[KernelCount]{};
    bool ready = false;
    Engine() {
        if (!device || !queue) return;
        NSError* error = nil;
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.fastMathEnabled = NO;
        std::string source = std::string(prelude) + coreSource + "\n" + kernelSource;
        id<MTLLibrary> lib = [device newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()] options:options error:&error];
        if (!lib) { std::fprintf(stderr, "[Mecena Metal] %s\n", error ? error.localizedDescription.UTF8String : "library"); return; }
        NSArray<NSString*>* names = @[@"kPrepare", @"kReduce", @"kFlow", @"kSmooth", @"kCross", @"kTile", @"kIntegrate"];
        for (int i = 0; i < KernelCount; ++i) {
            kernels[i] = [device newComputePipelineStateWithFunction:[lib newFunctionWithName:names[i]] error:&error];
            if (!kernels[i]) { std::fprintf(stderr, "[Mecena Metal] %s\n", error ? error.localizedDescription.UTF8String : "pipeline"); return; }
        }
        ready = true;
    }
};
Engine& engine() { static Engine e; return e; }
void dispatch(id<MTLComputeCommandEncoder> enc, id<MTLComputePipelineState> pipeline, int w, int h) {
    [enc setComputePipelineState:pipeline];
    NSUInteger x = pipeline.threadExecutionWidth;
    NSUInteger y = std::max<NSUInteger>(1, std::min<NSUInteger>(8, pipeline.maxTotalThreadsPerThreadgroup / x));
    [enc dispatchThreadgroups:MTLSizeMake((w + x - 1) / x, (h + y - 1) / y, 1) threadsPerThreadgroup:MTLSizeMake(x, y, 1)];
}
}
bool metalBlur(const blur::Job& job, Image dst, const int* win) {
    @autoreleasepool {
        Engine& E = engine();
        if (!E.ready || (!job.prev && !job.next)) return false;
        const Image& c = job.center;
        if (c.w != dst.w || c.h != dst.h || c.x0 != dst.x0 || c.y0 != dst.y0) return false;
        const int w = c.w, h = c.h;
        const auto g = blur::geometry(w, h, job.draft);
        const size_t pixels = size_t(w) * h, bytes = pixels * 16;
        const size_t pyramidBytes = size_t(blur::pyramidSize(g)) * 4, fieldBytes = size_t(blur::fieldSize(g)) * 4;
        const size_t level0Bytes = size_t(g.fieldW[0]) * g.fieldH[0] * 8;
        std::unique_ptr<Scratch> scratch;
        { std::lock_guard<std::mutex> lock(E.cacheMutex); scratch = std::move(E.idle); }
        if (!scratch || scratch->width != w || scratch->height != h || scratch->draft != job.draft) {
            scratch = std::make_unique<Scratch>(); scratch->width = w; scratch->height = h; scratch->draft = job.draft;
        }
        auto acquire = [&](int slot, size_t length) -> id<MTLBuffer> {
            if ([scratch->slots[slot] length] < length)
                scratch->slots[slot] = [E.device newBufferWithLength:length options:MTLResourceStorageModeShared];
            return scratch->slots[slot];
        };
        const Image* frames[3] = {&c, job.prev, job.next};
        id<MTLBuffer> src[3]{}, lin[3]{}, luma[3]{};
        for (int i = 0; i < 3; ++i) {
            if (!frames[i]) continue;
            src[i] = acquire(Src0 + i, bytes); lin[i] = acquire(Lin0 + i, bytes); luma[i] = acquire(Luma0 + i, pyramidBytes);
            if (!src[i] || !lin[i] || !luma[i]) return false;
            const Image& im = *frames[i];
            if (im.w != w || im.h != h) return false;
            auto packed = static_cast<char*>([src[i] contents]);
            for (int y = 0; y < h; ++y) std::memcpy(packed + size_t(y) * w * 16, im.at(im.x0, im.y0 + y), size_t(w) * 16);
        }
        gmc::GmParams p = blur::params(g, job, 1);
        id<MTLCommandBuffer> cmd = [E.queue commandBuffer];
        if (!cmd) return false;
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        if (!enc) return false;
        for (int i = 0; i < 3; ++i) {
            if (!frames[i]) continue;
            [enc setBuffer:src[i] offset:0 atIndex:0]; [enc setBuffer:lin[i] offset:0 atIndex:1];
            [enc setBuffer:luma[i] offset:0 atIndex:2]; [enc setBytes:&p length:sizeof(p) atIndex:3];
            dispatch(enc, E.kernels[Prepare], w, h);
            for (int l = 1; l < g.levels; ++l) {
                [enc memoryBarrierWithScope:MTLBarrierScopeBuffers];
                [enc setBuffer:luma[i] offset:0 atIndex:0]; [enc setBytes:&g length:sizeof(g) atIndex:1];
                [enc setBytes:&l length:sizeof(l) atIndex:2];
                dispatch(enc, E.kernels[Reduce], g.widths[l], g.heights[l]);
            }
        }
        id<MTLBuffer> fields[2]{};
        for (int side = 0; side < 2; ++side) {
            if (!frames[side + 1]) continue;
            id<MTLBuffer> raw = acquire(Raw0 + side, fieldBytes), smooth = acquire(Field0 + side, fieldBytes);
            if (!raw || !smooth) { [enc endEncoding]; return false; }
            std::memset([smooth contents], 0, fieldBytes);
            const auto cam = blur::cameraOf(job.camera, side);
            for (int l = g.levels - 1; l >= 0; --l) {
                [enc memoryBarrierWithScope:MTLBarrierScopeBuffers];
                [enc setBuffer:luma[0] offset:0 atIndex:0]; [enc setBuffer:luma[side + 1] offset:0 atIndex:1];
                [enc setBuffer:smooth offset:0 atIndex:2]; [enc setBuffer:raw offset:0 atIndex:3];
                [enc setBytes:&g length:sizeof(g) atIndex:4]; [enc setBytes:&cam length:sizeof(cam) atIndex:5];
                [enc setBytes:&l length:sizeof(l) atIndex:6];
                dispatch(enc, E.kernels[Flow], g.fieldW[l], g.fieldH[l]);
                [enc memoryBarrierWithScope:MTLBarrierScopeBuffers];
                [enc setBuffer:luma[0] offset:0 atIndex:0]; [enc setBuffer:luma[side + 1] offset:0 atIndex:1];
                [enc setBuffer:raw offset:0 atIndex:2]; [enc setBuffer:smooth offset:0 atIndex:3];
                [enc setBytes:&g length:sizeof(g) atIndex:4]; [enc setBytes:&l length:sizeof(l) atIndex:5];
                dispatch(enc, E.kernels[Smooth], g.fieldW[l], g.fieldH[l]);
            }
            fields[side] = smooth;
        }
        if (fields[0] && fields[1]) {
            id<MTLBuffer> outP = acquire(Cross0, level0Bytes), outN = acquire(Cross1, level0Bytes);
            if (!outP || !outN) { [enc endEncoding]; return false; }
            [enc memoryBarrierWithScope:MTLBarrierScopeBuffers];
            [enc setBuffer:luma[0] offset:0 atIndex:0]; [enc setBuffer:luma[1] offset:0 atIndex:1]; [enc setBuffer:luma[2] offset:0 atIndex:2];
            [enc setBuffer:fields[0] offset:0 atIndex:3]; [enc setBuffer:fields[1] offset:0 atIndex:4];
            [enc setBuffer:outP offset:0 atIndex:5]; [enc setBuffer:outN offset:0 atIndex:6];
            [enc setBytes:&g length:sizeof(g) atIndex:7];
            dispatch(enc, E.kernels[Cross], g.fieldW[0], g.fieldH[0]);
            fields[0] = outP; fields[1] = outN;
        }
        [enc endEncoding];
        [cmd commit]; [cmd waitUntilCompleted];
        if (cmd.status != MTLCommandBufferStatusCompleted) return false;
        // The tile radius depends on the longest motion found.
        const int nodes = 2 * g.fieldW[0] * g.fieldH[0];
        int reach = 1;
        for (int side = 0; side < 2; ++side) {
            if (!fields[side]) continue;
            const float* f = static_cast<const float*>([fields[side] contents]);
            reach = std::max(reach, blur::reachOf(std::vector<float>(f, f + nodes), nodes, job.exposure, g.grid));
        }
        p = blur::params(g, job, reach);
        cmd = [E.queue commandBuffer];
        if (!cmd) return false;
        enc = [cmd computeCommandEncoder];
        if (!enc) return false;
        id<MTLBuffer> tiles[2]{};
        for (int side = 0; side < 2; ++side) {
            if (!fields[side]) continue;
            id<MTLBuffer> tmp = acquire(Tmp0 + side, level0Bytes), tile = acquire(Tile0 + side, level0Bytes);
            if (!tmp || !tile) { [enc endEncoding]; return false; }
            for (int pass = 0; pass < 2; ++pass) {
                int horizontal = pass == 0;
                if (pass) [enc memoryBarrierWithScope:MTLBarrierScopeBuffers];
                [enc setBuffer:(pass ? tmp : fields[side]) offset:0 atIndex:0]; [enc setBuffer:(pass ? tile : tmp) offset:0 atIndex:1];
                [enc setBytes:&p length:sizeof(p) atIndex:2]; [enc setBytes:&side length:sizeof(side) atIndex:3];
                [enc setBytes:&horizontal length:sizeof(horizontal) atIndex:4];
                dispatch(enc, E.kernels[Tile], g.fieldW[0], g.fieldH[0]);
            }
            tiles[side] = tile;
        }
        id<MTLBuffer> out = acquire(Out, bytes);
        if (!out) { [enc endEncoding]; return false; }
        auto pick = [](id<MTLBuffer> a, id<MTLBuffer> b) { return a ? a : b; };
        [enc memoryBarrierWithScope:MTLBarrierScopeBuffers];
        [enc setBuffer:src[0] offset:0 atIndex:0]; [enc setBuffer:lin[0] offset:0 atIndex:1];
        [enc setBuffer:pick(lin[1], lin[0]) offset:0 atIndex:2]; [enc setBuffer:pick(lin[2], lin[0]) offset:0 atIndex:3];
        [enc setBuffer:luma[0] offset:0 atIndex:4];
        [enc setBuffer:pick(luma[1], luma[0]) offset:0 atIndex:5]; [enc setBuffer:pick(luma[2], luma[0]) offset:0 atIndex:6];
        [enc setBuffer:pick(fields[0], fields[1]) offset:0 atIndex:7]; [enc setBuffer:pick(fields[1], fields[0]) offset:0 atIndex:8];
        [enc setBuffer:pick(tiles[0], tiles[1]) offset:0 atIndex:9]; [enc setBuffer:pick(tiles[1], tiles[0]) offset:0 atIndex:10];
        [enc setBytes:&p length:sizeof(p) atIndex:11]; [enc setBuffer:out offset:0 atIndex:12];
        dispatch(enc, E.kernels[Integrate], w, h);
        [enc endEncoding];
        [cmd commit]; [cmd waitUntilCompleted];
        if (cmd.status != MTLCommandBufferStatusCompleted) return false;
        int x = std::max(win[0], dst.x0), end = std::min(win[2], dst.x0 + dst.w);
        auto packed = static_cast<const float*>([out contents]);
        if (end > x)
            for (int y = std::max(win[1], dst.y0); y < std::min(win[3], dst.y0 + dst.h); ++y)
                std::memcpy(dst.at(x, y), packed + 4 * (size_t(y - dst.y0) * dst.w + x - dst.x0), size_t(end - x) * 16);
        { std::lock_guard<std::mutex> lock(E.cacheMutex); if (!E.idle) E.idle = std::move(scratch); }
        return true;
    }
}
}

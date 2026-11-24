// SPDX-License-Identifier: GPL-3.0-or-later
// The Creative node on Metal: the passes of creativeRenderCPU (src/creative/CreativeEngine.cpp) in the same order,
// kernels in metal/kernels/Creative.metal. Nothing is read back: the whole frame is one command buffer.
#include "MetalKernels.h"

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "MetalContext.h"
#include "../gen/MetalSource.inc"

namespace {

// Same layout as in Creative.metal.
struct CrBlur { int w, h, axis, shift; float sigma; };
struct CrGrid { int s, w, h, c; };

struct Scratch
{
    std::map<std::string, id<MTLBuffer>> planes;
};

// Working planes per frame size, back in the pool when the command buffer completes; two sizes, two frames each.
class Pool
{
public:
    std::shared_ptr<Scratch> acquire(id<MTLDevice> device, const std::string& key)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto& recent = m_Recent[device];
        recent.erase(std::remove(recent.begin(), recent.end(), key), recent.end());
        recent.push_back(key);
        while (recent.size() > 2) {
            m_Free[device].erase(recent.front());
            recent.erase(recent.begin());
        }
        auto& list = m_Free[device][key];
        if (list.empty()) return std::make_shared<Scratch>();
        std::shared_ptr<Scratch> s = list.back();
        list.pop_back();
        return s;
    }
    void release(id<MTLDevice> device, const std::string& key, const std::shared_ptr<Scratch>& s)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto& recent = m_Recent[device];
        if (std::find(recent.begin(), recent.end(), key) == recent.end()) return;
        auto& list = m_Free[device][key];
        if (list.size() < 2) list.push_back(s);
    }
    void clear()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Free.clear();
        m_Recent.clear();
    }

private:
    std::mutex m_Mutex;
    std::map<id<MTLDevice>, std::map<std::string, std::vector<std::shared_ptr<Scratch>>>> m_Free;
    std::map<id<MTLDevice>, std::vector<std::string>> m_Recent;
};

Pool& pool()
{
    static Pool* p = new Pool();   // never destroyed: completion handlers may run after unload starts
    return *p;
}

class Encoder
{
public:
    Encoder(id<MTLDevice> device, id<MTLComputeCommandEncoder> enc) : m_Device(device), m_Enc(enc) {}
    bool ok = true;

    id<MTLComputePipelineState> use(const char* name)
    {
        id<MTLComputePipelineState> ps = metalPipeline(m_Device, kMetalCreative, name);
        if (!ps) ok = false;
        else [m_Enc setComputePipelineState:ps];
        return ps;
    }
    void grid(id<MTLComputePipelineState> ps, int w, int h)
    {
        if (!ps || w <= 0 || h <= 0) return;
        const NSUInteger tw = ps.threadExecutionWidth;
        const NSUInteger th = std::min<NSUInteger>(16, std::max<NSUInteger>(1, ps.maxTotalThreadsPerThreadgroup / tw));
        [m_Enc dispatchThreads:MTLSizeMake(w, h, 1) threadsPerThreadgroup:MTLSizeMake(tw, th, 1)];
    }
    void buf(id<MTLBuffer> b, int i, size_t floats = 0) { [m_Enc setBuffer:b offset:floats * sizeof(float) atIndex:i]; }
    template <typename T> void bytes(const T& v, int i) { [m_Enc setBytes:&v length:sizeof(T) atIndex:i]; }

    // 1D Gaussian of the plane at (in, inAt) into (out, outAt)
    void blur(id<MTLBuffer> in, size_t inAt, id<MTLBuffer> out, size_t outAt, int w, int h, int axis, float sigma, int shift)
    {
        const CrBlur b = { w, h, axis, shift, sigma };
        id<MTLComputePipelineState> ps = use("cr_blur_k");
        buf(in, 0, inAt); buf(out, 1, outAt); bytes(b, 2); grid(ps, w, h);
    }
    // grid of scale s from the record planes (record c, 3 = halation source), then its separable Gaussian
    void glow(id<MTLBuffer> X, id<MTLBuffer> out, size_t outAt, id<MTLBuffer> tmp, int s, int w, int h, int c,
              float sigma, const CreativeParams& p)
    {
        const CrGrid g = { s, w, h, c };
        id<MTLComputePipelineState> ps = use("cr_down_k");
        buf(X, 0); buf(out, 1, outAt); bytes(g, 2); bytes(p, 3); grid(ps, w, h);
        blur(out, outAt, tmp, 0, w, h, 0, sigma, 0);
        blur(tmp, 0, out, outAt, w, h, 1, sigma, 0);
    }

private:
    id<MTLDevice> m_Device;
    id<MTLComputeCommandEncoder> m_Enc;
};

}  // namespace

bool RunCreativeKernels(void* p_CmdQ, const CreativeParams& p_Params, const float* const p_Inputs[3], float* p_Output)
{
    const CreativeParams& p = p_Params;
    if (p.W <= 0 || p.H <= 0) return false;
    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)p_CmdQ;
    id<MTLDevice> device = queue.device;
    id<MTLBuffer> src[3];
    for (int k = 0; k < 3; ++k) src[k] = (__bridge id<MTLBuffer>)(void*)p_Inputs[k];
    id<MTLBuffer> dst = (__bridge id<MTLBuffer>)(void*)p_Output;
    const size_t N = (size_t)p.W * p.H;
    const std::string key = std::to_string(p.W) + "x" + std::to_string(p.H);
    std::shared_ptr<Scratch> s = pool().acquire(device, key);
    bool allocated = true;
    auto P = [&](const char* name, size_t floats) {
        __strong id<MTLBuffer>& b = s->planes[name];
        floats = std::max<size_t>(floats, 1);
        if (!b || b.length < floats * sizeof(float))
            b = [device newBufferWithLength:floats * sizeof(float) options:MTLResourceStorageModePrivate];
        if (!b) allocated = false;
        return b;
    };
    const bool signal = p.kind != CR_KIND_PROCESS;
    const size_t G = (size_t)p.bW * p.bH, Gh = (size_t)p.hW * p.hH;
    id<MTLBuffer> E = P("E", 3 * N);
    id<MTLBuffer> hal = p.halAmount > 0.0f ? P("hal", Gh) : E;
    id<MTLBuffer> bl = p.bloom > 0.0f ? P("bl", 3 * G) : E;
    id<MTLBuffer> tmp = (p.halAmount > 0.0f || p.bloom > 0.0f) ? P("tmp", std::max(G, Gh)) : E;

    id<MTLCommandBuffer> commands = [queue commandBuffer];
    commands.label = @"SLogMetaRawCreative";
    id<MTLComputeCommandEncoder> enc = [commands computeCommandEncoder];
    if (!enc) return false;
    Encoder e(device, enc);

    // 1. records
    id<MTLComputePipelineState> ps = e.use("cr_record_k");
    e.buf(src[0], 0); e.buf(src[1], 1); e.buf(src[2], 2); e.buf(E, 3); e.bytes(p, 4); e.grid(ps, p.W, p.H);

    // 2. glow
    if (p.halAmount > 0.0f) e.glow(E, hal, 0, tmp, p.hS, p.hW, p.hH, 3, p.halSigma, p);
    if (p.bloom > 0.0f)
        for (int c = 0; c < 3; ++c) e.glow(E, bl, c * G, tmp, p.bS, p.bW, p.bH, c, p.bloomSigma, p);

    // 3. output
    if (!signal) {
        s->planes.erase("V");   // planes of a signal chain of the same size, not needed here
        s->planes.erase("T");
        ps = e.use("cr_process_k");
        e.buf(src[0], 0); e.buf(dst, 1); e.buf(E, 2); e.buf(hal, 3); e.buf(bl, 4); e.bytes(p, 5); e.grid(ps, p.W, p.H);
    } else {
        // V = Y Cb Cr; T = filtered luma (0), its blur (1), Cb (2), Cr (3); the same steps as the CPU
        id<MTLBuffer> V = P("V", 3 * N), T = P("T", 4 * N);
        ps = e.use("cr_signal_k");
        e.buf(E, 0); e.buf(hal, 1); e.buf(bl, 2); e.buf(V, 3); e.bytes(p, 4); e.grid(ps, p.W, p.H);
        size_t Y = 0, Yb = N;   // offsets into T, or V for Yb when the luma is filtered along the lines only
        id<MTLBuffer> YbBuf = T;
        e.blur(V, 0, T, 0, p.W, p.H, 0, p.lumaSigma, 0);
        if (p.lumaAxes == 2) {
            e.blur(T, 0, T, N, p.W, p.H, 1, p.lumaSigma, 0);
            Y = N;
            Yb = 0;
        }
        e.blur(T, Y, V, 0, p.W, p.H, 0, p.ringSigma, 0);
        if (p.lumaAxes == 2) e.blur(V, 0, T, Yb, p.W, p.H, 1, p.ringSigma, 0);
        else { YbBuf = V; Yb = 0; }
        e.blur(V, N, T, 2 * N, p.W, p.H, 0, p.chromaSigma, p.chromaDelay);
        e.blur(V, 2 * N, T, 3 * N, p.W, p.H, 0, p.chromaSigma, p.chromaDelay);
        ps = e.use("cr_signal_out_k");
        e.buf(src[0], 0); e.buf(dst, 1); e.buf(T, 2, Y); e.buf(YbBuf, 3, Yb); e.buf(T, 4, 2 * N); e.buf(T, 5, 3 * N);
        e.bytes(p, 6); e.grid(ps, p.W, p.H);
    }
    // 4. viewing medium
    if (p.medium != CR_MEDIUM_NONE) {
        id<MTLBuffer> M = P("M", 3 * N);
        ps = e.use("cr_medium_in_k");
        e.buf(dst, 0); e.buf(M, 1); e.bytes(p, 2); e.grid(ps, p.W, p.H);
        ps = e.use("cr_medium_k");
        e.buf(M, 0); e.buf(dst, 1); e.bytes(p, 2); e.grid(ps, p.W, p.H);
    } else {
        s->planes.erase("M");
    }
    [enc endEncoding];
    if (!e.ok || !allocated) return false;
    [commands addCompletedHandler:^(id<MTLCommandBuffer>) { pool().release(device, key, s); }];
    [commands commit];
    return true;
}

void DropCreativeScratch() { pool().clear(); }

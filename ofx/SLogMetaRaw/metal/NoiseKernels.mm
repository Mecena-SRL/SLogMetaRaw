// SPDX-License-Identifier: GPL-3.0-or-later
// The Noise node on Metal: the passes of noiseRenderCPU (src/noise/NoiseEngine.cpp) in the same order, kernels in
// metal/kernels/Noise.metal. Nothing is read back: the whole frame is encoded in one command buffer.
#include "MetalKernels.h"

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "MetalContext.h"
#include "../gen/MetalSource.inc"

namespace {

// Same layout as in Noise.metal.
struct NzTables { float luma[kNoiseBins][kNoiseLevels]; float chroma[kNoiseBins][kNoiseLevels]; };
struct NzArgs {
    int W, H, rowPixels, dstRowPixels;
    int w, h, cw, ch;
    int step, x0, x1;
    int nbx, nby;
    int isLuma, skinOn, view, used;
    float gj, preserve, skin, shadows, inv, rho, scale;
};

const float kThresholdGPU = 1.2f;   // NoiseEngine.cpp kThreshold
const int kBlockGPU = 16;

// Working planes of one frame size; back to the pool in the command buffer's completion handler.
struct Scratch
{
    std::map<std::string, id<MTLBuffer>> planes;
};

class Pool
{
public:
    std::shared_ptr<Scratch> acquire(id<MTLDevice> device, const std::string& key)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto& byKey = m_Free[device];
        auto& recent = m_Recent[device];
        recent.erase(std::remove(recent.begin(), recent.end(), key), recent.end());
        recent.push_back(key);
        while (recent.size() > 2) {
            byKey.erase(recent.front());
            recent.erase(recent.begin());
        }
        auto& list = byKey[key];
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
        if (list.size() < 2) list.push_back(s);   // the planes are big: two frames in flight at most
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
        id<MTLComputePipelineState> ps = metalPipeline(m_Device, kMetalNoise, name);
        if (!ps) ok = false;
        else [m_Enc setComputePipelineState:ps];
        return ps;
    }
    void grid2(id<MTLComputePipelineState> ps, int w, int h)
    {
        if (!ps || w <= 0 || h <= 0) return;
        const NSUInteger tw = ps.threadExecutionWidth;
        const NSUInteger th = std::min<NSUInteger>(16, std::max<NSUInteger>(1, ps.maxTotalThreadsPerThreadgroup / tw));
        [m_Enc dispatchThreads:MTLSizeMake(w, h, 1) threadsPerThreadgroup:MTLSizeMake(tw, th, 1)];
    }
    void grid1(id<MTLComputePipelineState> ps, int n)
    {
        if (!ps || n <= 0) return;
        [m_Enc dispatchThreads:MTLSizeMake(n, 1, 1) threadsPerThreadgroup:MTLSizeMake(ps.threadExecutionWidth, 1, 1)];
    }
    void buf(id<MTLBuffer> b, int i) { [m_Enc setBuffer:b offset:0 atIndex:i]; }
    template <typename T> void bytes(const T& v, int i) { [m_Enc setBytes:&v length:sizeof(T) atIndex:i]; }

    void fill(id<MTLBuffer> b, float v, int n)
    {
        id<MTLComputePipelineState> ps = use("nz_fill");
        buf(b, 0); bytes(v, 1); bytes(n, 2); grid1(ps, n);
    }
    // one à trous level of in into out, tmp of the same size (out may be in only through tmp: never the same)
    void smooth(id<MTLBuffer> in, id<MTLBuffer> out, id<MTLBuffer> tmp, NzArgs a, int level)
    {
        a.step = 1 << level;
        a.x0 = std::min(2 * a.step, a.W);
        a.x1 = std::max(a.x0, a.W - 2 * a.step);
        id<MTLComputePipelineState> ps = use("nz_smooth_h");
        buf(in, 0); buf(tmp, 1); bytes(a, 2); grid2(ps, a.W, a.H);
        ps = use("nz_smooth_v");
        buf(tmp, 0); buf(out, 1); bytes(a, 2); grid2(ps, a.W, a.H);
    }
    // the luma pyramid of an RGBA frame: full, 1/2, 1/4
    void pyramid(id<MTLBuffer> frame, int rowPixels, NzArgs a, __strong id<MTLBuffer> (&pyr)[3], int (&w)[3], int (&h)[3])
    {
        a.rowPixels = rowPixels;
        id<MTLComputePipelineState> ps = use("nz_luma");
        buf(frame, 0); buf(pyr[0], 1); bytes(a, 2); grid2(ps, a.W, a.H);
        for (int l = 1; l < 3; ++l) {
            a.w = w[l - 1]; a.h = h[l - 1]; a.cw = w[l]; a.ch = h[l];
            ps = use("nz_reduce");
            buf(pyr[l - 1], 0); buf(pyr[l], 1); bytes(a, 2); grid2(ps, a.cw, a.ch);
        }
    }

private:
    id<MTLDevice> m_Device;
    id<MTLComputeCommandEncoder> m_Enc;
};

}  // namespace

bool RunNoiseKernels(void* p_CmdQ, int W, int H, const NoiseFrame* frames, int count, int center,
                     const NoiseProfile& profile, const NoiseControls& c, float* p_Output, int dstRowPixels)
{
    if (W <= 0 || H <= 0 || center < 0 || center >= count || !frames[center].px) return false;
    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)p_CmdQ;
    id<MTLDevice> device = queue.device;
    id<MTLBuffer> src = (__bridge id<MTLBuffer>)(void*)frames[center].px;
    id<MTLBuffer> dst = (__bridge id<MTLBuffer>)(void*)p_Output;
    const int N = W * H;
    const std::string key = std::to_string(W) + "x" + std::to_string(H);
    std::shared_ptr<Scratch> s = pool().acquire(device, key);
    bool allocated = true;
    auto P = [&](const char* name, size_t floats) {
        __strong id<MTLBuffer>& b = s->planes[name];
        floats = std::max<size_t>(floats, 1);
        if (!b || b.length < floats * sizeof(float) || (floats <= 1 && b.length > sizeof(float)))
            b = [device newBufferWithLength:floats * sizeof(float) options:MTLResourceStorageModePrivate];
        if (!b) allocated = false;
        return b;
    };

    NzTables T;
    for (int b = 0; b < kNoiseBins; ++b)
        for (int j = 0; j < kNoiseLevels; ++j) {
            T.luma[b][j] = profile.luma[b][j];
            T.chroma[b][j] = profile.chroma[b][j];
        }
    NzArgs a = {};
    a.W = W;
    a.H = H;
    a.rowPixels = (int)(frames[center].rowFloats / 4);
    a.dstRowPixels = dstRowPixels;
    a.view = c.view;

    id<MTLCommandBuffer> commands = [queue commandBuffer];
    commands.label = @"SLogMetaRawNoise";
    id<MTLComputeCommandEncoder> enc = [commands computeCommandEncoder];
    if (!enc) return false;
    Encoder e(device, enc);
    id<MTLComputePipelineState> ps;

    id<MTLBuffer> pl[3] = { P("pl0", N), P("pl1", N), P("pl2", N) };
    id<MTLBuffer> cur = P("cur", N), next = P("next", N), tmp = P("tmp", N);
    id<MTLBuffer> guide = P("guide", N), boost = P("boost", N), ratio = P("ratio", N), wsum = P("wsum", N);
    id<MTLBuffer> acc[3] = { P("acc0", N), P("acc1", N), P("acc2", N) };

    // ---- 1. temporal
    int used = 0;
    for (int n = 0; n < count; ++n)
        if (n != center && frames[n].px) ++used;
    if (used > 0 && c.temporal > 0.0f) {
        int w[3] = { W, 0, 0 }, h[3] = { H, 0, 0 };
        for (int l = 1; l < 3; ++l) {
            w[l] = std::max(1, w[l - 1] / 2);
            h[l] = std::max(1, h[l - 1] / 2);
        }
        id<MTLBuffer> pc[3] = { P("pc0", N), P("pc1", (size_t)w[1] * h[1]), P("pc2", (size_t)w[2] * h[2]) };
        id<MTLBuffer> pn[3] = { P("pn0", N), P("pn1", (size_t)w[1] * h[1]), P("pn2", (size_t)w[2] * h[2]) };
        const int nbx = (W + kBlockGPU - 1) / kBlockGPU, nby = (H + kBlockGPU - 1) / kBlockGPU;
        id<MTLBuffer> vx = P("vx", (size_t)nbx * nby), vy = P("vy", (size_t)nbx * nby);   // ints, same size
        id<MTLBuffer> sig = boost;   // boost is computed after the temporal pass
        e.pyramid(src, a.rowPixels, a, pc, w, h);
        NzArgs q = a;
        q.w = w[2];
        q.h = h[2];
        ps = e.use("nz_sigma");
        e.buf(pc[2], 0); e.buf(sig, 1); e.bytes(q, 2); e.bytes(T, 3); e.grid2(ps, W, H);
        for (int k = 0; k < 3; ++k) e.fill(acc[k], 0.0f, N);
        e.fill(wsum, 0.0f, N);
        e.fill(ratio, 0.0f, N);   // Σw² until nz_fuse
        q.inv = 2.0f / std::max(c.temporal, 0.05f);
        q.nbx = nbx;
        q.nby = nby;
        for (int n = 0; n < count; ++n) {
            if (n == center || !frames[n].px) continue;
            id<MTLBuffer> nb = (__bridge id<MTLBuffer>)(void*)frames[n].px;
            const int nbRow = (int)(frames[n].rowFloats / 4);
            e.pyramid(nb, nbRow, a, pn, w, h);
            NzArgs m = q;
            m.w = w[1]; m.h = h[1]; m.cw = w[2]; m.ch = h[2];
            ps = e.use("nz_search_blocks");
            e.buf(pc[0], 0); e.buf(pc[1], 1); e.buf(pc[2], 2); e.buf(pn[0], 3); e.buf(pn[1], 4); e.buf(pn[2], 5);
            e.buf(vx, 6); e.buf(vy, 7); e.bytes(m, 8); e.grid2(ps, nbx, nby);
            ps = e.use("nz_diff2");
            e.buf(pc[0], 0); e.buf(pn[0], 1); e.buf(vx, 2); e.buf(vy, 3); e.buf(cur, 4); e.bytes(q, 5);
            e.grid2(ps, W, H);
            ps = e.use("nz_rowsum3");
            e.buf(cur, 0); e.buf(next, 1); e.bytes(q, 2); e.grid2(ps, W, H);
            NzArgs r = q;
            r.rowPixels = nbRow;
            ps = e.use("nz_accumulate");
            e.buf(next, 0); e.buf(sig, 1); e.buf(nb, 2); e.buf(vx, 3); e.buf(vy, 4); e.buf(acc[0], 5);
            e.buf(acc[1], 6); e.buf(acc[2], 7); e.buf(wsum, 8); e.buf(ratio, 9); e.bytes(r, 10); e.grid2(ps, W, H);
        }
        q.rho = std::min(std::max(profile.temporalCorr, 0.0f), 1.0f);
        ps = e.use("nz_fuse");
        e.buf(src, 0); e.buf(acc[0], 1); e.buf(acc[1], 2); e.buf(acc[2], 3); e.buf(wsum, 4); e.buf(ratio, 5);
        e.buf(pl[0], 6); e.buf(pl[1], 7); e.buf(pl[2], 8); e.bytes(q, 9); e.grid2(ps, W, H);
    } else {
        s->planes.erase("pc0"); s->planes.erase("pn0");   // the big temporal planes, not needed by this node
        e.fill(ratio, 1.0f, N);
        e.fill(wsum, 0.0f, N);
        ps = e.use("nz_opponent");
        e.buf(src, 0); e.buf(pl[0], 1); e.buf(pl[1], 2); e.buf(pl[2], 3); e.bytes(a, 4); e.grid2(ps, W, H);
    }

    // ---- 2. spatial
    // guide: the luma smoothed to the 8 px band, as a mean code value
    e.smooth(pl[0], cur, tmp, a, 0);
    e.smooth(cur, next, tmp, a, 1);
    e.smooth(next, guide, tmp, a, 2);
    {
        const float k = 0.57735027f;
        ps = e.use("nz_scale");
        e.buf(guide, 0); e.bytes(k, 1); e.bytes(N, 2); e.grid1(ps, N);
    }
    const bool skin = c.skin > 0.0f && c.luma > 0.0f;
    id<MTLBuffer> skinW = P("skin", skin ? (size_t)N : 1);
    // the colour of the neighbourhood for the skin weight, in acc1 and acc2 (free until the channel loop)
    if (skin) {
        for (int k = 1; k < 3; ++k) {
            e.smooth(pl[k], cur, tmp, a, 0);
            e.smooth(cur, acc[k], tmp, a, 1);
        }
    }
    {
        NzArgs b = a;
        b.shadows = c.shadows;
        b.skin = c.skin;
        b.skinOn = skin ? 1 : 0;
        ps = e.use("nz_boost");
        e.buf(guide, 0); e.buf(ratio, 1); e.buf(acc[1], 2); e.buf(acc[2], 3); e.buf(boost, 4); e.buf(skinW, 5);
        e.bytes(b, 6); e.grid1(ps, N);
    }
    const float group[kNoiseLevels] = { c.fine, c.fine, c.medium, c.coarse, c.coarse };
    for (int k = 0; k < 3; ++k) {
        const bool isLuma = k == 0;
        const float strength = (isLuma ? c.luma : c.chroma) * kThresholdGPU;
        e.fill(acc[k], 0.0f, N);
        id<MTLBuffer> x = pl[k], y = cur;   // x: the current approximation; y: the next one
        for (int j = 0; j < kNoiseLevels; ++j) {
            e.smooth(x, y, tmp, a, j);
            NzArgs b = a;
            b.step = 1 << j;
            b.gj = group[j] * strength;
            b.isLuma = isLuma ? 1 : 0;
            b.preserve = c.preserve;
            b.skinOn = skin ? 1 : 0;
            ps = e.use("nz_shrink");
            e.buf(x, 0); e.buf(y, 1); e.buf(guide, 2); e.buf(boost, 3); e.buf(skinW, 4); e.buf(acc[k], 5);
            e.bytes(b, 6); e.bytes(T, 7); e.bytes(j, 8); e.grid2(ps, W, H);
            // the next level reads y; x becomes free (plane k is no longer needed after its first level)
            id<MTLBuffer> old = x;
            x = y;
            y = (old == pl[k]) ? next : old;
        }
        ps = e.use("nz_add");
        e.buf(acc[k], 0); e.buf(x, 1); e.bytes(N, 2); e.grid1(ps, N);
    }

    // ---- 3. back to RGB, or a view
    NzArgs f = a;
    f.used = used;
    ps = e.use("nz_final");
    e.buf(src, 0); e.buf(dst, 1); e.buf(acc[0], 2); e.buf(acc[1], 3); e.buf(acc[2], 4); e.buf(guide, 5);
    e.buf(ratio, 6); e.buf(wsum, 7); e.bytes(f, 8); e.bytes(T, 9); e.grid2(ps, W, H);
    [enc endEncoding];
    if (!e.ok || !allocated) return false;
    [commands addCompletedHandler:^(id<MTLCommandBuffer>) { pool().release(device, key, s); }];
    [commands commit];
    return true;
}

void DropNoiseScratch() { pool().clear(); }

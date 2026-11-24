// SPDX-License-Identifier: GPL-3.0-or-later
#include "CreativeEngine.h"

namespace {

using Plane = std::vector<float>;

void ensure(Plane& v, size_t n)
{
    if (v.size() < n) v.resize(n);
}

// 1D Gaussian of a w x h plane into out (a different plane).
void blur1(const CreativeParallel& par, const float* in, float* out, int w, int h, int axis, float sigma, int shift)
{
    par(h, [&](int b, int e) {
        for (int y = b; y < e; ++y)
            for (int x = 0; x < w; ++x) out[(size_t)y * w + x] = cr_blur_at(in, w, h, x, y, axis, sigma, shift);
    });
}

// Grid of scale gs from the record planes (record c, 3 = halation source), then its separable Gaussian.
void glowGrid(const CreativeParallel& par, CreativeScratch& s, Plane& grid, int gs, int gw, int gh, int c, float sigma,
              const CreativeParams& p)
{
    ensure(grid, (size_t)gw * gh);
    ensure(s.tmp, (size_t)gw * gh);
    float* g = grid.data();
    par(gh, [&](int b, int e) {
        for (int j = b; j < e; ++j)
            for (int i = 0; i < gw; ++i)
                g[(size_t)j * gw + i] = cr_down_at(s.E[0].data(), s.E[1].data(), s.E[2].data(), i, j, gs, c, p);
    });
    blur1(par, g, s.tmp.data(), gw, gh, 0, sigma, 0);
    blur1(par, s.tmp.data(), g, gw, gh, 1, sigma, 0);
}

template <typename F>
void writeOut(const CreativeParallel& par, const CreativeParams& p, const float* alpha, size_t srcRowFloats, float* dst,
              size_t dstRowFloats, F pixel)
{
    par(p.H, [&](int b, int e) {
        for (int y = b; y < e; ++y)
            for (int x = 0; x < p.W; ++x) {
                const SMf3 o = pixel(x, y);
                float* d = dst + (size_t)y * dstRowFloats + (size_t)x * 4;
                d[0] = o.x;
                d[1] = o.y;
                d[2] = o.z;
                d[3] = alpha[(size_t)y * srcRowFloats + (size_t)x * 4 + 3];
            }
    });
}

void signalChain(const CreativeParallel& par, const CreativeParams& q, CreativeScratch& s, const float* const E[3],
                 const float* hal, const float* const bl[3], const float* alpha, size_t srcRowFloats, float* dst,
                 size_t dstRowFloats);

}  // namespace

void creativeRenderCPU(const CreativeParams& p, const float* const src[3], size_t srcRowFloats, float* dst,
                       size_t dstRowFloats, const CreativeParallel& par, CreativeScratch& s)
{
    const int W = p.W, H = p.H;
    const size_t N = (size_t)W * H;
    CreativeParams q = p;
    q.rowPixels = (int)(srcRowFloats / 4);
    q.dstRowPixels = (int)(dstRowFloats / 4);

    // 1. records
    for (int k = 0; k < 3; ++k) ensure(s.E[k], N);
    par(H, [&](int b, int e) {
        for (int y = b; y < e; ++y)
            for (int x = 0; x < W; ++x) {
                const SMf3 e = cr_records(src[0], src[1], src[2], x, y, q);
                const size_t i = (size_t)y * W + x;
                s.E[0][i] = e.x;
                s.E[1][i] = e.y;
                s.E[2][i] = e.z;
            }
    });

    // 2. glow
    if (q.halAmount > 0.0f) glowGrid(par, s, s.hal, q.hS, q.hW, q.hH, 3, q.halSigma, q);
    if (q.bloom > 0.0f)
        for (int c = 0; c < 3; ++c) glowGrid(par, s, s.bl[c], q.bS, q.bW, q.bH, c, q.bloomSigma, q);
    const float* E[3] = { s.E[0].data(), s.E[1].data(), s.E[2].data() };
    const float* hal = q.halAmount > 0.0f ? s.hal.data() : E[0];   // unused grids: never read
    const float* bl[3];
    for (int c = 0; c < 3; ++c) bl[c] = q.bloom > 0.0f ? s.bl[c].data() : E[c];

    // 3. output
    if (q.kind == CR_KIND_PROCESS) {
        writeOut(par, q, src[0], srcRowFloats, dst, dstRowFloats, [&](int x, int y) {
            return cr_process(cr_glow_at(E[0], E[1], E[2], hal, bl[0], bl[1], bl[2], x, y, q), x, y, q);
        });
    } else {
        signalChain(par, q, s, E, hal, bl, src[0], srcRowFloats, dst, dstRowFloats);
    }

    // 4. viewing medium: the finished picture, decoded into M, shown through the screen or on paper
    if (q.medium == CR_MEDIUM_NONE) return;
    ensure(s.M, 3 * N);
    float* M = s.M.data();
    par(H, [&](int b, int e) {
        for (int y = b; y < e; ++y)
            for (int x = 0; x < W; ++x) {
                const SMf3 v = cr_medium_in(dst, x, y, q);
                const size_t k = (size_t)y * W + x;
                M[k] = v.x;
                M[N + k] = v.y;
                M[2 * N + k] = v.z;
            }
    });
    par(H, [&](int b, int e) {
        for (int y = b; y < e; ++y)
            for (int x = 0; x < W; ++x) {
                const SMf3 o = cr_medium_at(M, M + N, M + 2 * N, x, y, q);
                float* d = dst + (size_t)y * dstRowFloats + (size_t)x * 4;
                d[0] = o.x;
                d[1] = o.y;
                d[2] = o.z;
            }
    });
}

namespace {

// Signal chains: Y Cb Cr planes, their filters, then per pixel.
void signalChain(const CreativeParallel& par, const CreativeParams& q, CreativeScratch& s, const float* const E[3],
                 const float* hal, const float* const bl[3], const float* alpha, size_t srcRowFloats, float* dst,
                 size_t dstRowFloats)
{
    const int W = q.W, H = q.H;
    const size_t N = (size_t)W * H;
    for (int c = 0; c < 3; ++c) ensure(s.V[c], N);
    for (int c = 0; c < 4; ++c) ensure(s.T[c], N);
    par(H, [&](int b, int e) {
        for (int y = b; y < e; ++y)
            for (int x = 0; x < W; ++x) {
                const SMf3 v = cr_signal(cr_glow_at(E[0], E[1], E[2], hal, bl[0], bl[1], bl[2], x, y, q), x, y, q);
                const size_t k = (size_t)y * W + x;
                s.V[0][k] = v.x;
                s.V[1][k] = v.y;
                s.V[2][k] = v.z;
            }
    });
    // luma: its bandwidth (along the line; both axes for a digital camera's optics), then the peaking's blur
    float* Y = s.T[0].data();
    float* Yb = s.T[1].data();
    blur1(par, s.V[0].data(), Y, W, H, 0, q.lumaSigma, 0);
    if (q.lumaAxes == 2) {
        blur1(par, Y, Yb, W, H, 1, q.lumaSigma, 0);
        Y = s.T[1].data();
        Yb = s.T[0].data();
    }
    blur1(par, Y, s.V[0].data(), W, H, 0, q.ringSigma, 0);
    if (q.lumaAxes == 2) blur1(par, s.V[0].data(), Yb, W, H, 1, q.ringSigma, 0);
    else Yb = s.V[0].data();
    // chroma: its bandwidth and delay along the line
    blur1(par, s.V[1].data(), s.T[2].data(), W, H, 0, q.chromaSigma, q.chromaDelay);
    blur1(par, s.V[2].data(), s.T[3].data(), W, H, 0, q.chromaSigma, q.chromaDelay);
    const float* Yc = Y;
    const float* Ybc = Yb;
    writeOut(par, q, alpha, srcRowFloats, dst, dstRowFloats,
             [&](int x, int y) { return cr_signal_at(Yc, Ybc, s.T[2].data(), s.T[3].data(), x, y, q); });
}

}  // namespace

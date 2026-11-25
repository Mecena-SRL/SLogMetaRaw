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

// Grid of scale gs from the planes X (record c, 3 = halation source, 4 = luminance), then its separable Gaussian.
void glowGrid(const CreativeParallel& par, CreativeScratch& s, const float* const X[3], Plane& grid, int gs, int gw,
              int gh, int c, float sigma, const CreativeParams& p)
{
    ensure(grid, (size_t)gw * gh);
    ensure(s.tmp, (size_t)gw * gh);
    float* g = grid.data();
    par(gh, [&](int b, int e) {
        for (int j = b; j < e; ++j)
            for (int i = 0; i < gw; ++i)
                g[(size_t)j * gw + i] = cr_down_at(X[0], X[1], X[2], i, j, gs, c, p);
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

// Steps 1-3: the finished picture into dst.
void renderPicture(const CreativeParams& p, const float* const src[3], size_t srcRowFloats, float* dst,
                   size_t dstRowFloats, const CreativeParallel& par, CreativeScratch& s, const CreativeHistory* hist)
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
    const float* E[3] = { s.E[0].data(), s.E[1].data(), s.E[2].data() };
    if (q.sensor != 0) {   // the video camera's sensor: a CCD's smear by column, then every pixel
        if (!hist) q.history = 0;
        ensure(s.S, 3 * (size_t)W);
        float* S = s.S.data();
        if (q.sensor == CR_SENSOR_CCD)
            par(W, [&](int b, int e) {
                for (int x = b; x < e; ++x) cr_smear_col(E[0], E[1], E[2], S, x, q);
            });
        const float* h[3];
        for (int k = 0; k < 3; ++k) h[k] = q.history > k ? hist->src[k] : src[0];
        par(H, [&](int b, int e) {
            for (int y = b; y < e; ++y)
                for (int x = 0; x < W; ++x) {
                    const size_t i = (size_t)y * W + x;
                    const SMf3 v = cr_sensor(smf3(s.E[0][i], s.E[1][i], s.E[2][i]), h[0], h[1], h[2], S, x, y, q);
                    s.E[0][i] = v.x;
                    s.E[1][i] = v.y;
                    s.E[2][i] = v.z;
                }
        });
    }
    if (q.genSoft > 0.0f) {   // the duplicates' loss of sharpness
        ensure(s.tmp, N);
        for (int k = 0; k < 3; ++k) {
            blur1(par, E[k], s.tmp.data(), W, H, 0, q.genSoft, 0);
            blur1(par, s.tmp.data(), s.E[k].data(), W, H, 1, q.genSoft, 0);
        }
    }

    // 2. glow
    if (q.halAmount > 0.0f) glowGrid(par, s, E, s.hal, q.hS, q.hW, q.hH, 3, q.halSigma, q);
    if (q.bloom > 0.0f)
        for (int c = 0; c < 3; ++c) glowGrid(par, s, E, s.bl[c], q.bS, q.bW, q.bH, c, q.bloomSigma, q);
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

}

}  // namespace

void creativeRenderCPU(const CreativeParams& p, const float* const src[3], size_t srcRowFloats, float* dst,
                       size_t dstRowFloats, const CreativeParallel& par, CreativeScratch& s, const CreativePrevious* prev,
                       const CreativeHistory* history)
{
    const int W = p.W, H = p.H;
    const size_t N = (size_t)W * H;
    CreativeParams q = p;
    q.rowPixels = (int)(srcRowFloats / 4);
    q.dstRowPixels = (int)(dstRowFloats / 4);
    // an LCD's slow response: the previous frame's picture first (no medium), in the layout of dst
    const bool ghost = q.medium != CR_MEDIUM_NONE && q.mGhost > 0.0f && prev != nullptr;
    if (!ghost) q.mGhost = 0.0f;
    if (ghost) {
        ensure(s.prev, dstRowFloats * (size_t)H);
        CreativeParams pp = prev->p;
        pp.medium = CR_MEDIUM_NONE;
        renderPicture(pp, prev->src, srcRowFloats, s.prev.data(), dstRowFloats, par, s, nullptr);
    }
    renderPicture(q, src, srcRowFloats, dst, dstRowFloats, par, s, history);

    // 4. viewing medium: the finished picture, decoded into M, shown through the screen or on paper
    if (q.medium == CR_MEDIUM_NONE) return;
    ensure(s.M, 3 * N);
    float* M = s.M.data();
    par(H, [&](int b, int e) {
        for (int y = b; y < e; ++y)
            for (int x = 0; x < W; ++x) {
                const SMf3 v = cr_medium_in(dst, ghost ? s.prev.data() : dst, x, y, q);
                const size_t k = (size_t)y * W + x;
                M[k] = v.x;
                M[N + k] = v.y;
                M[2 * N + k] = v.z;
            }
    });
    const float* Mp[3] = { M, M + N, M + 2 * N };
    if (q.mGlow > 0.0f) glowGrid(par, s, Mp, s.hal, q.mGS, q.mGW, q.mGH, 4, q.mGlowSigma, q);
    const float* glow = q.mGlow > 0.0f ? s.hal.data() : M;   // unused: never read
    par(H, [&](int b, int e) {
        for (int y = b; y < e; ++y)
            for (int x = 0; x < W; ++x) {
                const SMf3 o = cr_medium_at(M, M + N, M + 2 * N, glow, x, y, q);
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
    if (q.tape != 0) {   // the tape, line by line: record and playback filters, then the picture the TV shows
        float* T0 = s.T[0].data();
        float* T2 = s.T[2].data();
        float* T3 = s.T[3].data();
        par(H, [&](int b, int e) {
            for (int y = b; y < e; ++y)
                cr_tape_row(s.V[0].data(), s.V[1].data(), s.V[2].data(), T0, T2, T3, y, q);
        });
        writeOut(par, q, alpha, srcRowFloats, dst, dstRowFloats,
                 [&](int x, int y) { return cr_tape_out(T0, T2, T3, x, y, q); });
        return;
    }
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

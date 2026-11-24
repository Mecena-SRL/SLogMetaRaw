// SPDX-License-Identifier: GPL-3.0-or-later
// CPU driver of the shared motion blur engine (blur_core.inc). The Metal driver
// in shutter_metal.mm runs the same functions as GPU kernels.
#pragma once
#include "camera.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <thread>
#include <vector>

namespace gmc {
inline float min(float a, float b) { return a < b ? a : b; }
inline float max(float a, float b) { return a > b ? a : b; }
inline int min(int a, int b) { return a < b ? a : b; }
inline int max(int a, int b) { return a > b ? a : b; }
inline float clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline int clamp(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
inline float abs(float v) { return std::fabs(v); }
inline int abs(int v) { return v < 0 ? -v : v; }
inline float sqrt(float v) { return std::sqrt(v); }
inline float floor(float v) { return std::floor(v); }
inline float ceil(float v) { return std::ceil(v); }
inline float pow(float a, float b) { return std::pow(a, b); }
inline float exp2(float v) { return std::exp2(v); }
inline float log2(float v) { return std::log2(v); }
inline float log10(float v) { return std::log10(v); }
#define GM_DEV
#define GM_DEVC const
#define GM_CREF(T) const T&
#define GM_SOURCE(...) __VA_ARGS__
#include "blur_core.inc"
#undef GM_SOURCE
#undef GM_CREF
#undef GM_DEVC
#undef GM_DEV
}

namespace blur {
using motion::Image;
// exposure: added shutter, in units of the interval between the centre and
// each neighbour. 0.5 adds half of that interval, centred on the picture.
struct Job {
    Image center;
    const Image* prev = nullptr;
    const Image* next = nullptr;
    float exposure = 0;
    int curve = 0;
    bool draft = false;
    bool curved = false;
    const motion::CameraPair* camera = nullptr; // [prev,next] or null
};

inline gmc::GmGeometry geometry(int w, int h, bool draft) {
    gmc::GmGeometry g{};
    g.width = w; g.height = h; g.grid = draft ? 8 : 4; g.search = 4; g.iterations = draft ? 3 : 6;
    int pixels = 0, nodes = 0;
    for (int l = 0; l < 8; ++l) {
        g.widths[l] = w; g.heights[l] = h; g.offsets[l] = pixels; pixels += w * h;
        g.fieldW[l] = (w - 1) / g.grid + 2; g.fieldH[l] = (h - 1) / g.grid + 2;
        g.fieldOffsets[l] = nodes; nodes += 2 * g.fieldW[l] * g.fieldH[l];
        ++g.levels;
        if (std::min(w, h) < 48) break;
        w = (w + 1) / 2; h = (h + 1) / 2;
    }
    return g;
}
inline int pyramidSize(const gmc::GmGeometry& g) { return g.offsets[g.levels - 1] + g.widths[g.levels - 1] * g.heights[g.levels - 1]; }
inline int fieldSize(const gmc::GmGeometry& g) { int l = g.levels - 1; return g.fieldOffsets[l] + 2 * g.fieldW[l] * g.fieldH[l]; }
inline gmc::GmCamera cameraOf(const motion::CameraPair* pair, int side) {
    gmc::GmCamera c{};
    if (pair && pair[side].valid()) std::copy(pair[side].forward.h, pair[side].forward.h + 9, c.h);
    return c;
}
// Longest motion in pixels -> tile radius in nodes for the requested exposure.
inline int reachOf(const std::vector<float>& field, int count, float exposure, int grid) {
    float longest = 0;
    for (int i = 0; i + 1 < count; i += 2) longest = std::max(longest, field[i] * field[i] + field[i + 1] * field[i + 1]);
    float pixels = .5f * exposure * std::sqrt(longest);
    return std::clamp(int(std::ceil(pixels / grid)) + 1, 1, 24);
}
inline gmc::GmParams params(const gmc::GmGeometry& g, const Job& job, int reach) {
    gmc::GmParams p{};
    p.width = g.width; p.height = g.height; p.grid = g.grid; p.fieldW = g.fieldW[0]; p.fieldH = g.fieldH[0];
    p.hasPrev = job.prev != nullptr; p.hasNext = job.next != nullptr; p.curve = job.curve;
    p.maxSamples = job.draft ? 24 : 256; p.curved = job.curved; p.reach = reach; p.exposure = job.exposure;
    p.camP = cameraOf(job.camera, 0); p.camN = cameraOf(job.camera, 1);
    return p;
}

template <class F> void parallelRows(int rows, const F& body) {
    unsigned n = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
    if (rows < 32 || n == 1) { for (int y = 0; y < rows; ++y) body(y); return; }
    std::atomic<int> next{0};
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < n; ++t) pool.emplace_back([&] { for (int y; (y = next++) < rows;) body(y); });
    for (auto& t : pool) t.join();
}

// Returns false when aborted.
inline bool render(const Job& job, Image dest, const int* win, const std::function<bool()>& aborted = {}) {
    const Image& c = job.center;
    const int w = c.w, h = c.h;
    const auto g = geometry(w, h, job.draft);
    const Image* frames[3] = {&c, job.prev, job.next};
    std::vector<float> src(size_t(w) * h * 4), lin[3], luma[3];
    for (int y = 0; y < h; ++y) std::memcpy(src.data() + size_t(y) * w * 4, c.at(c.x0, c.y0 + y), size_t(w) * 16);
    for (int i = 0; i < 3; ++i) {
        if (!frames[i]) continue;
        lin[i].resize(size_t(w) * h * 4); luma[i].resize(pyramidSize(g));
        std::vector<float> packed(size_t(w) * h * 4);
        const Image& im = *frames[i];
        for (int y = 0; y < h; ++y) std::memcpy(packed.data() + size_t(y) * w * 4, im.at(im.x0, im.y0 + y), size_t(w) * 16);
        parallelRows(h, [&](int y) { for (int x = 0; x < w; ++x) gmc::gm_prepare(packed.data(), lin[i].data(), luma[i].data(), w, job.curve, x, y); });
        for (int l = 1; l < g.levels; ++l)
            parallelRows(g.heights[l], [&](int y) { for (int x = 0; x < g.widths[l]; ++x) gmc::gm_reduce(luma[i].data(), g, l, x, y); });
    }
    if (aborted && aborted()) return false;
    std::vector<float> fields[2], tiles[2];
    int reach = 1;
    for (int side = 0; side < 2; ++side) {
        if (!frames[side + 1]) continue;
        const auto cam = cameraOf(job.camera, side);
        std::vector<float> raw(fieldSize(g)), smooth(fieldSize(g));
        const float* A = luma[0].data(); const float* B = luma[side + 1].data();
        for (int l = g.levels - 1; l >= 0; --l) {
            const int fw = g.fieldW[l];
            parallelRows(g.fieldH[l], [&](int j) {
                for (int i = 0; i < fw; ++i) {
                    auto v = gmc::gm_flowNode(A, B, smooth.data(), g, cam, l, i, j);
                    raw[g.fieldOffsets[l] + 2 * (j * fw + i)] = v.x; raw[g.fieldOffsets[l] + 2 * (j * fw + i) + 1] = v.y;
                }
            });
            parallelRows(g.fieldH[l], [&](int j) {
                for (int i = 0; i < fw; ++i) {
                    auto v = gmc::gm_smoothNode(A, B, raw.data(), g, l, i, j);
                    smooth[g.fieldOffsets[l] + 2 * (j * fw + i)] = v.x; smooth[g.fieldOffsets[l] + 2 * (j * fw + i) + 1] = v.y;
                }
            });
            if (aborted && aborted()) return false;
        }
        reach = std::max(reach, reachOf(smooth, 2 * g.fieldW[0] * g.fieldH[0], job.exposure, g.grid));
        fields[side] = std::move(smooth);
    }
    const int fw = g.fieldW[0], fh = g.fieldH[0];
    const auto p = params(g, job, reach);
    if (!fields[0].empty() && !fields[1].empty()) {
        std::vector<float> cp(fields[0].size()), cn(fields[1].size());
        parallelRows(fh, [&](int j) {
            for (int i = 0; i < fw; ++i) {
                auto r = gmc::gm_crossNode(luma[0].data(), luma[1].data(), luma[2].data(), fields[0].data(), fields[1].data(), g, i, j);
                int k = 2 * (j * fw + i);
                cp[k] = r.p.x; cp[k + 1] = r.p.y; cn[k] = r.n.x; cn[k + 1] = r.n.y;
            }
        });
        std::copy(cp.begin(), cp.begin() + 2 * fw * fh, fields[0].begin());
        std::copy(cn.begin(), cn.begin() + 2 * fw * fh, fields[1].begin());
    }
    for (int side = 0; side < 2; ++side) {
        if (fields[side].empty()) continue;
        std::vector<float> tmp(size_t(2) * fw * fh);
        tiles[side].resize(tmp.size());
        for (int pass = 0; pass < 2; ++pass) {
            const float* in = pass ? tmp.data() : fields[side].data();
            float* outp = pass ? tiles[side].data() : tmp.data();
            parallelRows(fh, [&](int j) {
                for (int i = 0; i < fw; ++i) {
                    auto v = gmc::gm_tileMax(in, p, side, i, j, pass == 0);
                    outp[2 * (j * fw + i)] = v.x; outp[2 * (j * fw + i) + 1] = v.y;
                }
            });
        }
    }
    auto ptr = [](std::vector<float>& v, std::vector<float>& fallback) { return v.empty() ? fallback.data() : v.data(); };
    const float *lc = lin[0].data(), *lp = ptr(lin[1], lin[0]), *ln = ptr(lin[2], lin[0]);
    const float *yc = luma[0].data(), *yp = ptr(luma[1], luma[0]), *yn = ptr(luma[2], luma[0]);
    const float *fp = ptr(fields[0], fields[1]), *fn = ptr(fields[1], fields[0]);
    const float *tp = ptr(tiles[0], tiles[1]), *tn = ptr(tiles[1], tiles[0]);
    const int x0 = std::max(win[0], dest.x0), x1 = std::min(win[2], dest.x0 + dest.w);
    const int y0 = std::max(win[1], dest.y0), y1 = std::min(win[3], dest.y0 + dest.h);
    std::atomic<bool> stop{false};
    parallelRows(std::max(0, y1 - y0), [&](int row) {
        if (stop) return;
        if (aborted && (row & 15) == 0 && aborted()) { stop = true; return; }
        const int y = y0 + row;
        for (int x = x0; x < x1; ++x) {
            float* d = dest.at(x, y);
            const int u = x - c.x0, v = y - c.y0;
            if (u < 0 || v < 0 || u >= w || v >= h) { std::fill(d, d + 4, 0.f); continue; }
            float o[4];
            gmc::gm_integrate(src.data(), lc, lp, ln, yc, yp, yn, fp, fn, tp, tn, p, u, v, o);
            std::copy(o, o + 4, d);
        }
    });
    return !stop;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoiseEngine.h"

#include <algorithm>
#include <cmath>

namespace {

const float kInvSqrt2 = 0.70710678f, kInvSqrt3 = 0.57735027f, kInvSqrt6 = 0.40824829f;
// à trous B3 spline: 1 4 6 4 1 / 16, taps 2^level apart
const float kB3[5] = { 1.0f / 16.0f, 4.0f / 16.0f, 6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f };
// a threshold of 1.5 σ at strength 1: most of the noise of a band, little of a texture above it
const float kThreshold = 1.5f;

inline int mirror(int i, int n)
{
    if (n == 1) return 0;
    while (i < 0 || i >= n) {
        if (i < 0) i = -i;
        if (i >= n) i = 2 * n - 2 - i;
    }
    return i;
}

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// One à trous smoothing of src into dst (separable, mirrored edges), tmp of the same size.
void smooth(const std::vector<float>& src, std::vector<float>& dst, std::vector<float>& tmp, int W, int H, int level,
            const NoiseParallel& parallel)
{
    const int step = 1 << level;
    const int x0 = std::min(2 * step, W), x1 = std::max(x0, W - 2 * step);   // taps inside the row
    parallel(H, [&](int ya, int yb) {
        for (int y = ya; y < yb; ++y) {
            const float* s = &src[(size_t)y * W];
            float* t = &tmp[(size_t)y * W];
            auto edge = [&](int x) {
                float v = 0.0f;
                for (int k = 0; k < 5; ++k) v += kB3[k] * s[mirror(x + (k - 2) * step, W)];
                t[x] = v;
            };
            for (int x = 0; x < x0; ++x) edge(x);
            for (int x = x0; x < x1; ++x)
                t[x] = kB3[0] * (s[x - 2 * step] + s[x + 2 * step]) + kB3[1] * (s[x - step] + s[x + step]) + kB3[2] * s[x];
            for (int x = x1; x < W; ++x) edge(x);
        }
    });
    parallel(H, [&](int ya, int yb) {
        for (int y = ya; y < yb; ++y) {
            const float* r0 = &tmp[(size_t)mirror(y - 2 * step, H) * W];
            const float* r1 = &tmp[(size_t)mirror(y - step, H) * W];
            const float* r2 = &tmp[(size_t)y * W];
            const float* r3 = &tmp[(size_t)mirror(y + step, H) * W];
            const float* r4 = &tmp[(size_t)mirror(y + 2 * step, H) * W];
            float* d = &dst[(size_t)y * W];
            for (int x = 0; x < W; ++x) d[x] = kB3[0] * (r0[x] + r4[x]) + kB3[1] * (r1[x] + r3[x]) + kB3[2] * r2[x];
        }
    });
}

void resize(std::vector<float>& v, size_t n)
{
    if (v.size() != n) v.assign(n, 0.0f);
}

// Opponent channels of an RGBA frame into s.plane (orthonormal: white noise keeps its σ in each).
void toOpponent(int W, int H, const NoiseFrame& f, NoiseScratch& s, const NoiseParallel& parallel)
{
    parallel(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* row = f.px + (size_t)y * f.rowFloats;
            for (int x = 0; x < W; ++x) {
                const float r = row[4 * x], g = row[4 * x + 1], b = row[4 * x + 2];
                const size_t i = (size_t)y * W + x;
                s.plane[0][i] = (r + g + b) * kInvSqrt3;
                s.plane[1][i] = (r - b) * kInvSqrt2;
                s.plane[2][i] = (r - 2.0f * g + b) * kInvSqrt6;
            }
        }
    });
}

// Brightness guide: the luma smoothed to the 8 px band, as a mean code value.
void buildGuide(int W, int H, NoiseScratch& s, const NoiseParallel& parallel)
{
    s.cur = s.plane[0];
    for (int j = 0; j < 3; ++j) {
        smooth(s.cur, s.next, s.guide, W, H, j, parallel);
        s.cur.swap(s.next);
    }
    s.guide.swap(s.cur);
    for (float& v : s.guide) v *= kInvSqrt3;
}

// Pixel noise of the mean code value at a bin: the bands together, as white noise would add up to 1.
float pixelSigma(const NoiseProfile& p, int bin, float frac)
{
    const int b1 = std::min(bin + 1, kNoiseBins - 1);
    float e = 0.0f;
    for (int j = 0; j < kNoiseLevels; ++j) {
        const float v = p.luma[bin][j] + frac * (p.luma[b1][j] - p.luma[bin][j]);
        e += v * v;
    }
    return std::sqrt(e) * (1.0f / 0.917f) * kInvSqrt3;
}

// Skin tones in the opponent plane of log code values: warm hue, low saturation, mid brightness. A heuristic.
float skinWeight(float m, float c1, float c2)
{
    if (m < 0.15f || m > 0.85f) return 0.0f;
    const float sat = std::sqrt(c1 * c1 + c2 * c2);
    if (sat < 0.01f || sat > 0.3f) return 0.0f;
    const float hue = std::atan2(c2, c1) * 57.2957795f;   // 0 = red-blue axis; skin about 0..30 degrees
    const float dh = std::fabs(hue - 12.0f);
    const float h = clampf((30.0f - dh) / 12.0f, 0.0f, 1.0f);
    const float lo = clampf((sat - 0.01f) / 0.02f, 0.0f, 1.0f), hi = clampf((0.3f - sat) / 0.1f, 0.0f, 1.0f);
    return h * lo * hi;
}

// ---------------------------------------------------------------- temporal

const int kBlock = 16;   // motion vectors per 16x16 block of the full frame

inline float lumaAt(const NoiseFrame& f, int x, int y)
{
    const float* p = f.px + (size_t)y * f.rowFloats + 4 * (size_t)x;
    return (p[0] + p[1] + p[2]) * (1.0f / 3.0f);
}

// Mean-luma pyramid: level 0 is read from the frame, levels 1 and 2 are 2x2 box reductions.
void buildPyramid(int W, int H, const NoiseFrame& f, std::vector<float> (&pyr)[3], int (&w)[3], int (&h)[3],
                  const NoiseParallel& parallel)
{
    w[0] = W;
    h[0] = H;
    resize(pyr[0], (size_t)W * H);
    parallel(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < W; ++x) pyr[0][(size_t)y * W + x] = lumaAt(f, x, y);
    });
    for (int l = 1; l < 3; ++l) {
        w[l] = std::max(1, w[l - 1] / 2);
        h[l] = std::max(1, h[l - 1] / 2);
        resize(pyr[l], (size_t)w[l] * h[l]);
        const int pw = w[l - 1], ph = h[l - 1], cw = w[l];
        const std::vector<float>& src = pyr[l - 1];
        std::vector<float>& dst = pyr[l];
        parallel(h[l], [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y)
                for (int x = 0; x < cw; ++x) {
                    const int xa = std::min(2 * x, pw - 1), xb = std::min(2 * x + 1, pw - 1);
                    const int ya = std::min(2 * y, ph - 1), yb = std::min(2 * y + 1, ph - 1);
                    dst[(size_t)y * cw + x] = 0.25f * (src[(size_t)ya * pw + xa] + src[(size_t)ya * pw + xb]
                                                       + src[(size_t)yb * pw + xa] + src[(size_t)yb * pw + xb]);
                }
        });
    }
}

// Sum of absolute differences of a size x size window centred on (cx, cy), the neighbour displaced by (dx, dy).
float sad(const std::vector<float>& a, const std::vector<float>& b, int w, int h, int cx, int cy, int dx, int dy, int size)
{
    float sum = 0.0f;
    const int half = size / 2, x0 = cx - half, y0 = cy - half;
    if (x0 >= 0 && y0 >= 0 && x0 + size <= w && y0 + size <= h && x0 + dx >= 0 && y0 + dy >= 0 && x0 + dx + size <= w
        && y0 + dy + size <= h) {   // inside both: no clamping
        for (int y = 0; y < size; ++y) {
            const float* pa = &a[(size_t)(y0 + y) * w + x0];
            const float* pb = &b[(size_t)(y0 + y + dy) * w + x0 + dx];
            for (int x = 0; x < size; ++x) sum += std::fabs(pa[x] - pb[x]);
        }
        return sum;
    }
    for (int y = y0; y < y0 + size; ++y) {
        const int ya = clampi(y, 0, h - 1), yb = clampi(y + dy, 0, h - 1);
        for (int x = x0; x < x0 + size; ++x) {
            const int xa = clampi(x, 0, w - 1), xb = clampi(x + dx, 0, w - 1);
            sum += std::fabs(a[(size_t)ya * w + xa] - b[(size_t)yb * w + xb]);
        }
    }
    return sum;
}

// Best displacement around (px, py) within ±r; a small cost per pixel of motion favours the still answer in flat,
// noisy areas, where every candidate is as good as the others.
void search(const std::vector<float>& a, const std::vector<float>& b, int w, int h, int cx, int cy, int size, int r,
            float bias, int& vx, int& vy)
{
    const int px = vx, py = vy;
    float best = 1e30f;
    for (int dy = py - r; dy <= py + r; ++dy)
        for (int dx = px - r; dx <= px + r; ++dx) {
            const float cost = sad(a, b, w, h, cx, cy, dx, dy, size) + bias * (float)(std::abs(dx) + std::abs(dy));
            if (cost < best) {
                best = cost;
                vx = dx;
                vy = dy;
            }
        }
}

}  // namespace

void noiseBin(float mean, int& bin, float& frac)
{
    const float pos = clampf(mean, 0.0f, 1.0f) * (kNoiseBins - 1);
    bin = std::min((int)pos, kNoiseBins - 2);
    frac = pos - (float)bin;
}

void noiseRenderCPU(int W, int H, const NoiseFrame* frames, int count, int center, const NoiseProfile& profile,
                    const NoiseControls& c, float* dst, size_t dstRowFloats, const NoiseParallel& parallel,
                    NoiseScratch& s)
{
    const size_t N = (size_t)W * H;
    for (int k = 0; k < 3; ++k) {
        resize(s.plane[k], N);
        resize(s.acc[k], N);
    }
    resize(s.cur, N);
    resize(s.next, N);
    resize(s.guide, N);
    resize(s.ratio, N);
    resize(s.wsum, N);
    const NoiseFrame& cf = frames[center];

    // ---- 1. temporal: the centre frame and its aligned neighbours, weighted per pixel
    int used = 0;
    for (int n = 0; n < count; ++n)
        if (n != center && frames[n].px) ++used;
    NoiseFrame fused = cf;
    std::vector<float>& fusedRGBA = s.fused;
    if (used > 0 && c.temporal > 0.0f) {
        int w[3], h[3], wn[3], hn[3];
        buildPyramid(W, H, cf, s.pyr, w, h, parallel);
        // noise of the mean code value at each pixel, from the profile at its brightness
        std::vector<float>& sig = s.lumaC;
        resize(sig, N);
        parallel(H, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y)
                for (int x = 0; x < W; ++x) {
                    int bin;
                    float frac;
                    noiseBin(s.pyr[2][(size_t)std::min(y / 4, h[2] - 1) * w[2] + std::min(x / 4, w[2] - 1)], bin, frac);
                    sig[(size_t)y * W + x] = std::max(pixelSigma(profile, bin, frac), 1e-5f);
                }
        });
        std::vector<float>& wsq = s.wsq;
        resize(wsq, N);
        std::fill(wsq.begin(), wsq.end(), 0.0f);
        for (int k = 0; k < 3; ++k) std::fill(s.acc[k].begin(), s.acc[k].end(), 0.0f);
        std::fill(s.wsum.begin(), s.wsum.end(), 0.0f);
        const int nbx = (W + kBlock - 1) / kBlock, nby = (H + kBlock - 1) / kBlock;
        s.vx.assign((size_t)nbx * nby, 0);
        s.vy.assign((size_t)nbx * nby, 0);
        const float inv = 2.0f / std::max(c.temporal, 0.05f);
        for (int n = 0; n < count; ++n) {
            const NoiseFrame& nf = frames[n];
            if (n == center || !nf.px) continue;
            buildPyramid(W, H, nf, s.pyrN, wn, hn, parallel);
            // coarse to fine: ±6 at 1/4 (±24 px), then ±1 at 1/2 and at full size
            parallel(nby, [&](int b0, int b1) {
                for (int by = b0; by < b1; ++by)
                    for (int bx = 0; bx < nbx; ++bx) {
                        const int cx = bx * kBlock + kBlock / 2, cy = by * kBlock + kBlock / 2;
                        int vx = 0, vy = 0;
                        search(s.pyr[2], s.pyrN[2], w[2], h[2], cx / 4, cy / 4, 8, 6, 0.02f, vx, vy);
                        vx *= 2;
                        vy *= 2;
                        search(s.pyr[1], s.pyrN[1], w[1], h[1], cx / 2, cy / 2, 12, 1, 0.01f, vx, vy);
                        vx *= 2;
                        vy *= 2;
                        search(s.pyr[0], s.pyrN[0], w[0], h[0], cx, cy, kBlock, 1, 0.01f, vx, vy);
                        s.vx[(size_t)by * nbx + bx] = vx;
                        s.vy[(size_t)by * nbx + bx] = vy;
                    }
            });
            // squared luma difference of each pixel with its match, then its 3x3 mean: the patch distance
            auto vecOf = [&](int x, int y, int& dx, int& dy) {
                const size_t bi = (size_t)std::min(y / kBlock, nby - 1) * nbx + std::min(x / kBlock, nbx - 1);
                dx = s.vx[bi];
                dy = s.vy[bi];
            };
            parallel(H, [&](int y0, int y1) {
                for (int y = y0; y < y1; ++y)
                    for (int x = 0; x < W; ++x) {
                        int dx, dy;
                        vecOf(x, y, dx, dy);
                        const int qx = clampi(x + dx, 0, W - 1), qy = clampi(y + dy, 0, H - 1);
                        const float e = s.pyr[0][(size_t)y * W + x] - s.pyrN[0][(size_t)qy * W + qx];
                        s.cur[(size_t)y * W + x] = e * e;
                    }
            });
            parallel(H, [&](int y0, int y1) {
                for (int y = y0; y < y1; ++y) {
                    const float* r = &s.cur[(size_t)y * W];
                    float* t = &s.next[(size_t)y * W];
                    for (int x = 0; x < W; ++x) t[x] = r[std::max(x - 1, 0)] + r[x] + r[std::min(x + 1, W - 1)];
                }
            });
            parallel(H, [&](int y0, int y1) {
                for (int y = y0; y < y1; ++y) {
                    const float* ra = &s.next[(size_t)std::max(y - 1, 0) * W];
                    const float* rb = &s.next[(size_t)y * W];
                    const float* rc = &s.next[(size_t)std::min(y + 1, H - 1) * W];
                    for (int x = 0; x < W; ++x) {
                        const size_t i = (size_t)y * W + x;
                        const float d = ra[x] + rb[x] + rc[x];
                        const float q = d / 9.0f / (2.0f * sig[i] * sig[i]);
                        const float wt = std::exp(-std::max(q - 1.5f, 0.0f) * inv);
                        if (wt < 1e-3f) continue;
                        int dx, dy;
                        vecOf(x, y, dx, dy);
                        const int qx = clampi(x + dx, 0, W - 1), qy = clampi(y + dy, 0, H - 1);
                        const float* p = nf.px + (size_t)qy * nf.rowFloats + 4 * (size_t)qx;
                        for (int k = 0; k < 3; ++k) s.acc[k][i] += wt * p[k];
                        s.wsum[i] += wt;
                        wsq[i] += wt * wt;
                    }
                }
            });
        }
        fusedRGBA.resize(N * 4);
        const float rho = clampf(profile.temporalCorr, 0.0f, 1.0f);
        parallel(H, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                const float* row = cf.px + (size_t)y * cf.rowFloats;
                for (int x = 0; x < W; ++x) {
                    const size_t i = (size_t)y * W + x;
                    const float norm = 1.0f / (1.0f + s.wsum[i]);
                    for (int k = 0; k < 3; ++k) fusedRGBA[4 * i + k] = (row[4 * x + k] + s.acc[k][i]) * norm;
                    fusedRGBA[4 * i + 3] = row[4 * x + 3];
                    // what is left of the noise: sqrt(Σw²)/Σw for independent frames, less gain where it repeats
                    const float r2 = (1.0f + wsq[i]) * norm * norm;
                    s.ratio[i] = std::sqrt(rho + (1.0f - rho) * r2);
                }
            }
        });
        fused.px = fusedRGBA.data();
        fused.rowFloats = (size_t)W * 4;
    } else {
        std::fill(s.ratio.begin(), s.ratio.end(), 1.0f);
        std::fill(s.wsum.begin(), s.wsum.end(), 0.0f);
    }

    // ---- 2. spatial: opponent channels, five bands, garrote against the expected noise of each
    toOpponent(W, H, fused, s, parallel);
    buildGuide(W, H, s, parallel);
    const bool skin = c.skin > 0.0f && c.luma > 0.0f;
    if (skin) {   // colour of the neighbourhood, for the skin weight
        resize(s.gc1, N);
        resize(s.gc2, N);
        for (int k = 1; k < 3; ++k) {
            std::vector<float>& out = k == 1 ? s.gc1 : s.gc2;
            s.cur = s.plane[k];
            for (int j = 0; j < 2; ++j) {
                smooth(s.cur, s.next, out, W, H, j, parallel);
                s.cur.swap(s.next);
            }
            out.swap(s.cur);
        }
    }
    // per pixel, once: what multiplies every threshold (shadow boost, temporal leftover) and the skin weight
    resize(s.boost, N);
    if (skin) resize(s.skin, N);
    parallel(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < W; ++x) {
                const size_t i = (size_t)y * W + x;
                const float m = s.guide[i];
                s.boost[i] = (1.0f + c.shadows * clampf((0.35f - m) / 0.25f, 0.0f, 1.0f)) * s.ratio[i];
                if (skin) s.skin[i] = c.skin * skinWeight(m, s.gc1[i], s.gc2[i]);
            }
    });
    const float group[kNoiseLevels] = { c.fine, c.fine, c.medium, c.coarse, c.coarse };
    for (int k = 0; k < 3; ++k) {
        const bool isLuma = k == 0;
        const float strength = (isLuma ? c.luma : c.chroma) * kThreshold;
        std::vector<float>& acc = s.acc[k];
        std::fill(acc.begin(), acc.end(), 0.0f);
        s.cur = s.plane[k];
        for (int j = 0; j < kNoiseLevels; ++j) {
            smooth(s.cur, s.next, s.plane[k], W, H, j, parallel);   // plane[k] is free after the copy: scratch
            const float gj = group[j] * strength;
            const float (*tab)[kNoiseLevels] = isLuma ? profile.luma : profile.chroma;
            parallel(H, [&](int y0, int y1) {
                for (int y = y0; y < y1; ++y)
                    for (int x = 0; x < W; ++x) {
                        const size_t i = (size_t)y * W + x;
                        const float wv = s.cur[i] - s.next[i];
                        float out = wv;
                        if (gj > 0.0f) {
                            int bin;
                            float frac;
                            noiseBin(s.guide[i], bin, frac);
                            const float sig = tab[bin][j] + frac * (tab[bin + 1][j] - tab[bin][j]);
                            const float t = gj * sig * s.boost[i];
                            const float w2 = wv * wv, t2 = t * t;
                            out = w2 > t2 ? wv * (1.0f - t2 / w2) : 0.0f;
                            if (isLuma && j == 0 && c.preserve > 0.0f) out += c.preserve * (wv - out);
                            if (skin && isLuma && j < 2) out += s.skin[i] * (wv - out);
                        }
                        acc[i] += out;
                    }
            });
            s.cur.swap(s.next);
        }
        // residual + kept bands
        for (size_t i = 0; i < N; ++i) acc[i] += s.cur[i];
    }

    // ---- 3. back to RGB, or a view
    parallel(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* src = cf.px + (size_t)y * cf.rowFloats;
            float* out = dst + (size_t)y * dstRowFloats;
            for (int x = 0; x < W; ++x) {
                const size_t i = (size_t)y * W + x;
                const float Y = s.acc[0][i] * kInvSqrt3, c1 = s.acc[1][i], c2 = s.acc[2][i];
                float rgb[3] = { Y + c1 * kInvSqrt2 + c2 * kInvSqrt6, Y - 2.0f * c2 * kInvSqrt6,
                                 Y - c1 * kInvSqrt2 + c2 * kInvSqrt6 };
                if (c.view == kViewRemoved) {
                    for (int k = 0; k < 3; ++k) rgb[k] = 0.5f + 4.0f * (src[4 * x + k] - rgb[k]);
                } else if (c.view == kViewNoiseMap) {
                    int bin;
                    float frac;
                    noiseBin(s.guide[i], bin, frac);
                    const float v = clampf(pixelSigma(profile, bin, frac) * s.ratio[i] / 0.02f, 0.0f, 1.0f);
                    rgb[0] = rgb[1] = rgb[2] = v;
                } else if (c.view == kViewTemporal) {
                    const float v = used > 0 ? clampf(s.wsum[i] / (float)used, 0.0f, 1.0f) : 0.0f;
                    rgb[0] = rgb[1] = rgb[2] = v;
                }
                out[4 * x] = rgb[0];
                out[4 * x + 1] = rgb[1];
                out[4 * x + 2] = rgb[2];
                out[4 * x + 3] = src[4 * x + 3];
            }
        }
    });
}

bool noiseMeasure(int W, int H, NoiseFrame frame, const NoiseParallel& parallel, NoiseScratch& s, NoiseProfile& out)
{
    const size_t N = (size_t)W * H;
    for (int k = 0; k < 3; ++k) resize(s.plane[k], N);
    resize(s.cur, N);
    resize(s.next, N);
    resize(s.guide, N);
    resize(s.lumaC, N);
    toOpponent(W, H, frame, s, parallel);
    buildGuide(W, H, s, parallel);
    out = NoiseProfile();
    const int kSize[kNoiseLevels] = { 8, 8, 16, 16, 16 };
    // energies of the bands per block, per brightness bin: luma, chroma (C1 and C2 together)
    std::vector<float> energy[2][kNoiseLevels][kNoiseBins];
    for (int k = 0; k < 3; ++k) {
        s.cur = s.plane[k];
        for (int j = 0; j < kNoiseLevels; ++j) {
            smooth(s.cur, s.next, s.lumaC, W, H, j, parallel);
            const int b = kSize[j], nbx = W / b, nby = H / b;
            const int edge = (4 << j) / b + 1;   // blocks the mirrored border reaches at this band: skipped
            for (int by = edge; by < nby - edge; ++by)
                for (int bx = edge; bx < nbx - edge; ++bx) {
                    double e = 0.0, m = 0.0;
                    for (int y = by * b; y < by * b + b; ++y)
                        for (int x = bx * b; x < bx * b + b; ++x) {
                            const size_t i = (size_t)y * W + x;
                            const float wv = s.cur[i] - s.next[i];
                            e += (double)wv * wv;
                            m += s.guide[i];
                        }
                    m /= (double)b * b;
                    if (m < 0.02 || m > 0.97) continue;   // clipped: no noise to see
                    int bin;
                    float frac;
                    noiseBin((float)m, bin, frac);
                    if (frac > 0.5f) ++bin;
                    energy[k == 0 ? 0 : 1][j][bin].push_back((float)(e / ((double)b * b)));
                }
            s.cur.swap(s.next);
        }
    }
    // the flattest blocks: the 15th percentile, corrected for where it falls in the spread of a pure noise block
    int valid = 0;
    bool has[kNoiseBins] = {};
    bool coarse[kNoiseBins][kNoiseLevels] = {};   // measured at that band (the coarse ones need more area)
    for (int bin = 0; bin < kNoiseBins; ++bin) {
        bool ok = true;
        for (int j = 0; j < kNoiseLevels; ++j) {
            bool here = true;
            for (int ch = 0; ch < 2; ++ch) {
                std::vector<float>& v = energy[ch][j][bin];
                if (v.size() < 4) { here = false; break; }
                const size_t p = v.size() * 15 / 100;
                std::nth_element(v.begin(), v.begin() + p, v.end());
                const float side = (float)kSize[j] / (float)(1 << j);
                const float dof = std::max(4.0f, side * side * 0.5f);
                const float corr = std::max(0.3f, 1.0f - 1.04f * std::sqrt(2.0f / dof));
                (ch == 0 ? out.luma : out.chroma)[bin][j] = std::sqrt(std::max(v[p], 0.0f) / corr);
            }
            coarse[bin][j] = here;
            if (j < 3 && !here) ok = false;   // the three finer bands make a measured bin
        }
        if (ok) {
            has[bin] = true;
            ++valid;
        }
    }
    // a measured bin without its coarsest bands: the ratio of the bins that have them, else that of white noise
    const float white[kNoiseLevels] = { 0.889f, 0.200f, 0.086f, 0.041f, 0.020f };
    for (int j = 3; j < kNoiseLevels; ++j)
        for (int ch = 0; ch < 2; ++ch) {
            float (*t)[kNoiseLevels] = ch == 0 ? out.luma : out.chroma;
            std::vector<float> ratios;
            for (int bin = 0; bin < kNoiseBins; ++bin)
                if (has[bin] && coarse[bin][j] && t[bin][j - 1] > 0.0f) ratios.push_back(t[bin][j] / t[bin][j - 1]);
            float ratio = white[j] / white[j - 1];
            if (!ratios.empty()) {
                std::nth_element(ratios.begin(), ratios.begin() + ratios.size() / 2, ratios.end());
                ratio = ratios[ratios.size() / 2];
            }
            for (int bin = 0; bin < kNoiseBins; ++bin)
                if (has[bin] && !coarse[bin][j]) t[bin][j] = t[bin][j - 1] * ratio;
        }
    if (valid < 3) return false;
    // no plausible noise grows from one band to the next coarser one past the second: what does is picture
    for (int bin = 0; bin < kNoiseBins; ++bin)
        for (int j = 2; j < kNoiseLevels; ++j) {
            out.luma[bin][j] = std::min(out.luma[bin][j], out.luma[bin][j - 1]);
            out.chroma[bin][j] = std::min(out.chroma[bin][j], out.chroma[bin][j - 1]);
        }
    // empty bins: linear between measured neighbours, constant past the ends
    for (int bin = 0; bin < kNoiseBins; ++bin) {
        if (has[bin]) continue;
        int lo = bin - 1, hi = bin + 1;
        while (lo >= 0 && !has[lo]) --lo;
        while (hi < kNoiseBins && !has[hi]) ++hi;
        for (int j = 0; j < kNoiseLevels; ++j)
            for (int ch = 0; ch < 2; ++ch) {
                float (*t)[kNoiseLevels] = ch == 0 ? out.luma : out.chroma;
                if (lo < 0) t[bin][j] = t[hi][j];
                else if (hi >= kNoiseBins) t[bin][j] = t[lo][j];
                else t[bin][j] = t[lo][j] + (t[hi][j] - t[lo][j]) * (float)(bin - lo) / (float)(hi - lo);
            }
    }
    out.valid = true;
    return true;
}

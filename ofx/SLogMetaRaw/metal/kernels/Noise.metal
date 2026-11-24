// SPDX-License-Identifier: GPL-3.0-or-later
// The Noise node on the GPU: the passes of src/noise/NoiseEngine.cpp, with the same sums in the same order.
// Compiled at runtime after kMetalPrelude (metal_stdlib + DevelopMath.h; nothing of it is used here).

#define NZ_BINS 16
#define NZ_LEVELS 5
#define NZ_BLOCK 16

struct NzTables {
    float luma[NZ_BINS][NZ_LEVELS];
    float chroma[NZ_BINS][NZ_LEVELS];
};

// One struct for every pass: each reads the fields it needs.
struct NzArgs {
    int W, H;            // frame
    int rowPixels;       // stride of the RGBA frame read by the pass (pixels)
    int dstRowPixels;    // stride of the output frame
    int w, h, cw, ch;    // pyramid: plane read and plane written
    int step, x0, x1;    // à trous level: tap spacing, interior span of the horizontal pass
    int nbx, nby;        // motion blocks
    int isLuma, skinOn, view, used;
    float gj, preserve, skin, shadows, inv, rho, scale;
};

constant float NZ_INV_SQRT2 = 0.70710678f;
constant float NZ_INV_SQRT3 = 0.57735027f;
constant float NZ_INV_SQRT6 = 0.40824829f;
constant float NZ_B0 = 1.0f / 16.0f;
constant float NZ_B1 = 4.0f / 16.0f;
constant float NZ_B2 = 6.0f / 16.0f;

inline int nz_mirror(int i, int n)
{
    if (n == 1) return 0;
    while (i < 0 || i >= n) {
        if (i < 0) i = -i;
        if (i >= n) i = 2 * n - 2 - i;
    }
    return i;
}

inline int nz_clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float nz_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline void nz_bin(float mean, thread int& bin, thread float& frac)
{
    const float pos = nz_clampf(mean, 0.0f, 1.0f) * (float)(NZ_BINS - 1);
    bin = min((int)pos, NZ_BINS - 2);
    frac = pos - (float)bin;
}

inline float nz_pixel_sigma(constant NzTables& T, int bin, float frac)
{
    const int b1 = min(bin + 1, NZ_BINS - 1);
    float e = 0.0f;
    for (int j = 0; j < NZ_LEVELS; ++j) {
        const float v = T.luma[bin][j] + frac * (T.luma[b1][j] - T.luma[bin][j]);
        e += v * v;
    }
    return sqrt(e) * (1.0f / 0.917f) * NZ_INV_SQRT3;
}

inline float nz_skin_weight(float m, float c1, float c2)
{
    if (m < 0.15f || m > 0.85f) return 0.0f;
    const float sat = sqrt(c1 * c1 + c2 * c2);
    if (sat < 0.01f || sat > 0.3f) return 0.0f;
    const float hue = atan2(c2, c1) * 57.2957795f;
    const float dh = fabs(hue - 12.0f);
    const float h = nz_clampf((30.0f - dh) / 12.0f, 0.0f, 1.0f);
    const float lo = nz_clampf((sat - 0.01f) / 0.02f, 0.0f, 1.0f), hi = nz_clampf((0.3f - sat) / 0.1f, 0.0f, 1.0f);
    return h * lo * hi;
}

// ---------------------------------------------------------------- temporal

kernel void nz_luma(const device float* src [[buffer(0)]], device float* out [[buffer(1)]],
                    constant NzArgs& a [[buffer(2)]], uint2 id [[thread_position_in_grid]])
{
    if ((int)id.x >= a.W || (int)id.y >= a.H) return;
    const int p = ((int)id.y * a.rowPixels + (int)id.x) * 4;
    out[(int)id.y * a.W + (int)id.x] = (src[p] + src[p + 1] + src[p + 2]) * (1.0f / 3.0f);
}

kernel void nz_reduce(const device float* src [[buffer(0)]], device float* dst [[buffer(1)]],
                      constant NzArgs& a [[buffer(2)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.cw || y >= a.ch) return;
    const int xa = min(2 * x, a.w - 1), xb = min(2 * x + 1, a.w - 1);
    const int ya = min(2 * y, a.h - 1), yb = min(2 * y + 1, a.h - 1);
    dst[y * a.cw + x] = 0.25f * (src[ya * a.w + xa] + src[ya * a.w + xb] + src[yb * a.w + xa] + src[yb * a.w + xb]);
}

// Noise of the mean code value at each pixel, from the profile at the brightness of the 1/4 pyramid.
kernel void nz_sigma(const device float* p2 [[buffer(0)]], device float* sig [[buffer(1)]],
                     constant NzArgs& a [[buffer(2)]], constant NzTables& T [[buffer(3)]],
                     uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    int bin;
    float frac;
    nz_bin(p2[min(y / 4, a.h - 1) * a.w + min(x / 4, a.w - 1)], bin, frac);
    sig[y * a.W + x] = max(nz_pixel_sigma(T, bin, frac), 1e-5f);
}

inline float nz_sad(const device float* A, const device float* B, int w, int h, int cx, int cy, int dx, int dy,
                    int size)
{
    float sum = 0.0f;
    const int hs = size / 2, x0 = cx - hs, y0 = cy - hs;
    if (x0 >= 0 && y0 >= 0 && x0 + size <= w && y0 + size <= h && x0 + dx >= 0 && y0 + dy >= 0 && x0 + dx + size <= w
        && y0 + dy + size <= h) {
        for (int y = 0; y < size; ++y) {
            const device float* pa = A + (y0 + y) * w + x0;
            const device float* pb = B + (y0 + y + dy) * w + x0 + dx;
            for (int x = 0; x < size; ++x) sum += fabs(pa[x] - pb[x]);
        }
        return sum;
    }
    for (int y = y0; y < y0 + size; ++y) {
        const int ya = nz_clampi(y, 0, h - 1), yb = nz_clampi(y + dy, 0, h - 1);
        for (int x = x0; x < x0 + size; ++x) {
            const int xa = nz_clampi(x, 0, w - 1), xb = nz_clampi(x + dx, 0, w - 1);
            sum += fabs(A[ya * w + xa] - B[yb * w + xb]);
        }
    }
    return sum;
}

inline void nz_search(const device float* A, const device float* B, int w, int h, int cx, int cy, int size, int r,
                      float bias, thread int& vx, thread int& vy)
{
    const int px = vx, py = vy;
    float best = 1e30f;
    for (int dy = py - r; dy <= py + r; ++dy)
        for (int dx = px - r; dx <= px + r; ++dx) {
            const float cost = nz_sad(A, B, w, h, cx, cy, dx, dy, size) + bias * (float)(abs(dx) + abs(dy));
            if (cost < best) {
                best = cost;
                vx = dx;
                vy = dy;
            }
        }
}

// One motion vector per 16x16 block: ±6 at 1/4 (±24 px), then ±1 at 1/2 and at full size.
// The plane sizes: W x H, w x h (1/2), cw x ch (1/4).
kernel void nz_search_blocks(const device float* c0 [[buffer(0)]], const device float* c1 [[buffer(1)]],
                             const device float* c2 [[buffer(2)]], const device float* n0 [[buffer(3)]],
                             const device float* n1 [[buffer(4)]], const device float* n2 [[buffer(5)]],
                             device int* vxs [[buffer(6)]], device int* vys [[buffer(7)]],
                             constant NzArgs& a [[buffer(8)]], uint2 id [[thread_position_in_grid]])
{
    const int bx = (int)id.x, by = (int)id.y;
    if (bx >= a.nbx || by >= a.nby) return;
    const int cx = bx * NZ_BLOCK + NZ_BLOCK / 2, cy = by * NZ_BLOCK + NZ_BLOCK / 2;
    int vx = 0, vy = 0;
    nz_search(c2, n2, a.cw, a.ch, cx / 4, cy / 4, 8, 6, 0.02f, vx, vy);
    vx *= 2;
    vy *= 2;
    nz_search(c1, n1, a.w, a.h, cx / 2, cy / 2, 12, 1, 0.01f, vx, vy);
    vx *= 2;
    vy *= 2;
    nz_search(c0, n0, a.W, a.H, cx, cy, NZ_BLOCK, 1, 0.01f, vx, vy);
    vxs[by * a.nbx + bx] = vx;
    vys[by * a.nbx + bx] = vy;
}

inline int nz_block(constant NzArgs& a, int x, int y)
{
    return min(y / NZ_BLOCK, a.nby - 1) * a.nbx + min(x / NZ_BLOCK, a.nbx - 1);
}

// Squared luma difference of each pixel with its match.
kernel void nz_diff2(const device float* c0 [[buffer(0)]], const device float* n0 [[buffer(1)]],
                     const device int* vxs [[buffer(2)]], const device int* vys [[buffer(3)]],
                     device float* out [[buffer(4)]], constant NzArgs& a [[buffer(5)]],
                     uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    const int b = nz_block(a, x, y);
    const int qx = nz_clampi(x + vxs[b], 0, a.W - 1), qy = nz_clampi(y + vys[b], 0, a.H - 1);
    const float e = c0[y * a.W + x] - n0[qy * a.W + qx];
    out[y * a.W + x] = e * e;
}

kernel void nz_rowsum3(const device float* in [[buffer(0)]], device float* out [[buffer(1)]],
                       constant NzArgs& a [[buffer(2)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    const device float* r = in + y * a.W;
    out[y * a.W + x] = r[max(x - 1, 0)] + r[x] + r[min(x + 1, a.W - 1)];
}

// The 3x3 patch distance against the expected noise gives the neighbour's weight at each pixel.
// wsq is kept in the ratio plane until nz_fuse.
kernel void nz_accumulate(const device float* rows [[buffer(0)]], const device float* sig [[buffer(1)]],
                          const device float* nb [[buffer(2)]], const device int* vxs [[buffer(3)]],
                          const device int* vys [[buffer(4)]], device float* acc0 [[buffer(5)]],
                          device float* acc1 [[buffer(6)]], device float* acc2 [[buffer(7)]],
                          device float* wsum [[buffer(8)]], device float* wsq [[buffer(9)]],
                          constant NzArgs& a [[buffer(10)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    const int i = y * a.W + x;
    const float d = rows[max(y - 1, 0) * a.W + x] + rows[i] + rows[min(y + 1, a.H - 1) * a.W + x];
    const float q = d / 9.0f / (2.0f * sig[i] * sig[i]);
    const float wt = exp(-max(q - 1.5f, 0.0f) * a.inv);
    if (wt < 1e-3f) return;
    const int b = nz_block(a, x, y);
    const int qx = nz_clampi(x + vxs[b], 0, a.W - 1), qy = nz_clampi(y + vys[b], 0, a.H - 1);
    const int p = (qy * a.rowPixels + qx) * 4;
    acc0[i] += wt * nb[p];
    acc1[i] += wt * nb[p + 1];
    acc2[i] += wt * nb[p + 2];
    wsum[i] += wt;
    wsq[i] += wt * wt;
}

// The weighted mean of the window, straight into the opponent planes; the leftover noise ratio in place of wsq.
kernel void nz_fuse(const device float* src [[buffer(0)]], const device float* acc0 [[buffer(1)]],
                    const device float* acc1 [[buffer(2)]], const device float* acc2 [[buffer(3)]],
                    const device float* wsum [[buffer(4)]], device float* ratio [[buffer(5)]],
                    device float* p0 [[buffer(6)]], device float* p1 [[buffer(7)]], device float* p2 [[buffer(8)]],
                    constant NzArgs& a [[buffer(9)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    const int i = y * a.W + x, p = (y * a.rowPixels + x) * 4;
    const float norm = 1.0f / (1.0f + wsum[i]);
    const float r = (src[p] + acc0[i]) * norm, g = (src[p + 1] + acc1[i]) * norm, b = (src[p + 2] + acc2[i]) * norm;
    p0[i] = (r + g + b) * NZ_INV_SQRT3;
    p1[i] = (r - b) * NZ_INV_SQRT2;
    p2[i] = (r - 2.0f * g + b) * NZ_INV_SQRT6;
    const float r2 = (1.0f + ratio[i]) * norm * norm;
    ratio[i] = sqrt(a.rho + (1.0f - a.rho) * r2);
}

// ---------------------------------------------------------------- spatial

kernel void nz_opponent(const device float* src [[buffer(0)]], device float* p0 [[buffer(1)]],
                        device float* p1 [[buffer(2)]], device float* p2 [[buffer(3)]],
                        constant NzArgs& a [[buffer(4)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    const int i = y * a.W + x, p = (y * a.rowPixels + x) * 4;
    const float r = src[p], g = src[p + 1], b = src[p + 2];
    p0[i] = (r + g + b) * NZ_INV_SQRT3;
    p1[i] = (r - b) * NZ_INV_SQRT2;
    p2[i] = (r - 2.0f * g + b) * NZ_INV_SQRT6;
}

kernel void nz_fill(device float* out [[buffer(0)]], constant float& v [[buffer(1)]], constant int& n [[buffer(2)]],
                    uint id [[thread_position_in_grid]])
{
    if ((int)id < n) out[id] = v;
}

kernel void nz_scale(device float* out [[buffer(0)]], constant float& k [[buffer(1)]], constant int& n [[buffer(2)]],
                     uint id [[thread_position_in_grid]])
{
    if ((int)id < n) out[id] *= k;
}

kernel void nz_add(device float* out [[buffer(0)]], const device float* in [[buffer(1)]],
                   constant int& n [[buffer(2)]], uint id [[thread_position_in_grid]])
{
    if ((int)id < n) out[id] += in[id];
}

// À trous B3, horizontal: the taps inside the row in pairs, at the edges one by one (mirrored), as on the CPU.
kernel void nz_smooth_h(const device float* src [[buffer(0)]], device float* tmp [[buffer(1)]],
                        constant NzArgs& a [[buffer(2)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    const device float* s = src + y * a.W;
    const int st = a.step;
    float v;
    if (x >= a.x0 && x < a.x1) {
        v = NZ_B0 * (s[x - 2 * st] + s[x + 2 * st]) + NZ_B1 * (s[x - st] + s[x + st]) + NZ_B2 * s[x];
    } else {
        const float k[5] = { NZ_B0, NZ_B1, NZ_B2, NZ_B1, NZ_B0 };
        v = 0.0f;
        for (int t = 0; t < 5; ++t) v += k[t] * s[nz_mirror(x + (t - 2) * st, a.W)];
    }
    tmp[y * a.W + x] = v;
}

kernel void nz_smooth_v(const device float* tmp [[buffer(0)]], device float* dst [[buffer(1)]],
                        constant NzArgs& a [[buffer(2)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    const int st = a.step;
    const float r0 = tmp[nz_mirror(y - 2 * st, a.H) * a.W + x], r1 = tmp[nz_mirror(y - st, a.H) * a.W + x];
    const float r2 = tmp[y * a.W + x];
    const float r3 = tmp[nz_mirror(y + st, a.H) * a.W + x], r4 = tmp[nz_mirror(y + 2 * st, a.H) * a.W + x];
    dst[y * a.W + x] = NZ_B0 * (r0 + r4) + NZ_B1 * (r1 + r3) + NZ_B2 * r2;
}

// What multiplies every threshold (shadow boost, temporal leftover) and the skin weight, once per pixel.
kernel void nz_boost(const device float* guide [[buffer(0)]], const device float* ratio [[buffer(1)]],
                     const device float* gc1 [[buffer(2)]], const device float* gc2 [[buffer(3)]],
                     device float* boost [[buffer(4)]], device float* skin [[buffer(5)]],
                     constant NzArgs& a [[buffer(6)]], uint id [[thread_position_in_grid]])
{
    const int i = (int)id;
    if (i >= a.W * a.H) return;
    const float m = guide[i];
    boost[i] = (1.0f + a.shadows * nz_clampf((0.35f - m) / 0.25f, 0.0f, 1.0f)) * ratio[i];
    if (a.skinOn) skin[i] = a.skin * nz_skin_weight(m, gc1[i], gc2[i]);
}

inline float nz_coef(const device float* cur, const device float* next, int i) { return cur[i] - next[i]; }

// One band of one channel: each coefficient shrunk against the mean energy of its 3x3 neighbourhood (one band step
// apart), threshold from the profile at the pixel's brightness; the result added to acc.
kernel void nz_shrink(const device float* cur [[buffer(0)]], const device float* next [[buffer(1)]],
                      const device float* guide [[buffer(2)]], const device float* boost [[buffer(3)]],
                      const device float* skin [[buffer(4)]], device float* acc [[buffer(5)]],
                      constant NzArgs& a [[buffer(6)]], constant NzTables& T [[buffer(7)]],
                      constant int& level [[buffer(8)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    const int i = y * a.W + x;
    const float wv = nz_coef(cur, next, i);
    float out = wv;
    if (a.gj > 0.0f) {
        int bin;
        float frac;
        nz_bin(guide[i], bin, frac);
        const float t0 = a.isLuma ? T.luma[bin][level] : T.chroma[bin][level];
        const float t1 = a.isLuma ? T.luma[bin + 1][level] : T.chroma[bin + 1][level];
        const float sig = t0 + frac * (t1 - t0);
        const float t = a.gj * sig * boost[i];
        const int st = a.step;
        const int xa = nz_mirror(x - st, a.W), xb = nz_mirror(x + st, a.W);
        const int rows[3] = { nz_mirror(y - st, a.H) * a.W, y * a.W, nz_mirror(y + st, a.H) * a.W };
        float e = 0.0f;
        for (int r = 0; r < 3; ++r) {
            const float va = nz_coef(cur, next, rows[r] + xa), vc = nz_coef(cur, next, rows[r] + x);
            const float vb = nz_coef(cur, next, rows[r] + xb);
            e += va * va + vc * vc + vb * vb;
        }
        e = e * (1.0f / 9.0f);
        const float t2 = t * t;
        out = e > t2 ? wv * (1.0f - t2 / e) : 0.0f;
        if (a.isLuma && level == 0 && a.preserve > 0.0f) out += a.preserve * (wv - out);
        if (a.skinOn && a.isLuma && level < 2) out += skin[i] * (wv - out);
    }
    acc[i] += out;
}

// Back to RGB, or a view; alpha from the source.
kernel void nz_final(const device float* src [[buffer(0)]], device float* dst [[buffer(1)]],
                     const device float* acc0 [[buffer(2)]], const device float* acc1 [[buffer(3)]],
                     const device float* acc2 [[buffer(4)]], const device float* guide [[buffer(5)]],
                     const device float* ratio [[buffer(6)]], const device float* wsum [[buffer(7)]],
                     constant NzArgs& a [[buffer(8)]], constant NzTables& T [[buffer(9)]],
                     uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= a.W || y >= a.H) return;
    const int i = y * a.W + x, p = (y * a.rowPixels + x) * 4, o = (y * a.dstRowPixels + x) * 4;
    const float Y = acc0[i] * NZ_INV_SQRT3, c1 = acc1[i], c2 = acc2[i];
    float rgb[3] = { Y + c1 * NZ_INV_SQRT2 + c2 * NZ_INV_SQRT6, Y - 2.0f * c2 * NZ_INV_SQRT6,
                     Y - c1 * NZ_INV_SQRT2 + c2 * NZ_INV_SQRT6 };
    if (a.view == 1) {
        for (int k = 0; k < 3; ++k) rgb[k] = 0.5f + 4.0f * (src[p + k] - rgb[k]);
    } else if (a.view == 2) {
        int bin;
        float frac;
        nz_bin(guide[i], bin, frac);
        const float v = nz_clampf(nz_pixel_sigma(T, bin, frac) * ratio[i] / 0.02f, 0.0f, 1.0f);
        rgb[0] = rgb[1] = rgb[2] = v;
    } else if (a.view == 3) {
        const float v = a.used > 0 ? nz_clampf(wsum[i] / (float)a.used, 0.0f, 1.0f) : 0.0f;
        rgb[0] = rgb[1] = rgb[2] = v;
    }
    dst[o] = rgb[0];
    dst[o + 1] = rgb[1];
    dst[o + 2] = rgb[2];
    dst[o + 3] = src[p + 3];
}

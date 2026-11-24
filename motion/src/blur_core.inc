// SPDX-License-Identifier: GPL-3.0-or-later
// Motion blur engine shared verbatim by the CPU path (blur.hpp) and Metal
// (shutter_metal.mm). The body is one macro argument: C++ expands it as code,
// Metal receives it as a string through #__VA_ARGS__. Inside GM_SOURCE use no
// preprocessor directives, no double precision, and only same-typed calls to
// min/max/clamp so both compilers accept exactly the same program.
//
// Pipeline for each neighbour (previous and next picture):
//   prepare   linear RGB + perceptual luma
//   reduce    luma pyramid
//   flowNode  per pyramid level: candidate propagation (coarse, camera, zero,
//             brute force at the top) followed by mean-normalised Lucas-Kanade
//   smooth    robust 3x3 median that is only accepted where the image agrees
//   tileH/V   largest motion that can reach each node within the exposure
//   integrate per pixel: time integral of the light that crosses the pixel,
//             including moving surfaces that sweep over it from outside
GM_SOURCE(
struct GmVec { float x, y; };
struct GmColor { float r, g, b, ok; };
struct GmPair { GmVec p, n; };
struct GmGeometry {
    int width, height, levels, grid, search, iterations, pad0, pad1;
    int widths[8], heights[8], offsets[8];
    int fieldW[8], fieldH[8], fieldOffsets[8];
};
struct GmCamera { float h[12]; };
struct GmParams {
    int width, height, grid, fieldW, fieldH;
    int hasPrev, hasNext, curve, maxSamples, curved, reach, level;
    float exposure, pad0, pad1, pad2;
    GmCamera camP, camN; // dominant (camera) motion to each neighbour, or zeros
};

inline GmVec gmv(float x, float y) { GmVec r; r.x = x; r.y = y; return r; }
inline float gm_len(GmVec v) { return sqrt(v.x * v.x + v.y * v.y); }
inline float gm_len2(GmVec v) { return v.x * v.x + v.y * v.y; }

inline float gm_decode(float v, int c) {
    if (c == 1) return v >= 171.2102946929f / 1023.f ? .19f * pow(10.f, (v * 1023.f - 420.f) / 261.5f) - .01f
                                                     : (v * 1023.f - 95.f) * .01125f / (171.2102946929f - 95.f);
    if (c == 2) return v <= .02740668f ? v / 10.44426855f : exp2(v / .07329248f - 7.f) - .0075f;
    return v;
}
inline float gm_encode(float v, int c) {
    if (c == 1) return v >= .01125f ? (420.f + 261.5f * log10((v + .01f) / .19f)) / 1023.f
                                    : (95.f + v * (171.2102946929f - 95.f) / .01125f) / 1023.f;
    if (c == 2) return v <= .00262409f ? v * 10.44426855f : (log2(v + .0075f) + 7.f) * .07329248f;
    return v;
}
inline float gm_perceptual(float r, float g, float b) {
    float l = .25f * r + .5f * g + .25f * b;
    return .125f * log2(max(l, 0.f) + .02f);
}

inline float gm_tex(GM_DEVC float* p, int off, int w, int h, int x, int y) {
    x = clamp(x, 0, w - 1); y = clamp(y, 0, h - 1);
    return p[off + y * w + x];
}
inline float gm_bil(GM_DEVC float* p, int off, int w, int h, float x, float y) {
    x = clamp(x, 0.f, float(w - 1)); y = clamp(y, 0.f, float(h - 1));
    int ix = int(x), iy = int(y);
    float fx = x - float(ix), fy = y - float(iy);
    int ix1 = min(ix + 1, w - 1), iy1 = min(iy + 1, h - 1);
    float a = p[off + iy * w + ix], b = p[off + iy * w + ix1];
    float c = p[off + iy1 * w + ix], d = p[off + iy1 * w + ix1];
    float top = a + (b - a) * fx, bottom = c + (d - c) * fx;
    return top + (bottom - top) * fy;
}
inline GmVec gm_node(GM_DEVC float* f, int off, int fw, int fh, int i, int j) {
    i = clamp(i, 0, fw - 1); j = clamp(j, 0, fh - 1);
    int k = off + 2 * (j * fw + i);
    return gmv(f[k], f[k + 1]);
}
inline GmVec gm_field(GM_DEVC float* f, int off, int fw, int fh, float x, float y) {
    x = clamp(x, 0.f, float(fw - 1)); y = clamp(y, 0.f, float(fh - 1));
    int ix = int(x), iy = int(y);
    float fx = x - float(ix), fy = y - float(iy);
    GmVec a = gm_node(f, off, fw, fh, ix, iy), b = gm_node(f, off, fw, fh, ix + 1, iy);
    GmVec c = gm_node(f, off, fw, fh, ix, iy + 1), d = gm_node(f, off, fw, fh, ix + 1, iy + 1);
    float tx = a.x + (b.x - a.x) * fx, ty = a.y + (b.y - a.y) * fx;
    float bx = c.x + (d.x - c.x) * fx, by = c.y + (d.y - c.y) * fx;
    return gmv(tx + (bx - tx) * fy, ty + (by - ty) * fy);
}
inline GmVec gm_cameraShift(GM_CREF(GmCamera) c, int width, int height, float x, float y) {
    if (c.h[8] == 0.f) return gmv(0.f, 0.f);
    float sx = .5f * float(max(1, width - 1)), sy = .5f * float(max(1, height - 1));
    float u = clamp(x / sx - 1.f, -1.f, 1.f), v = clamp(y / sy - 1.f, -1.f, 1.f);
    float z = c.h[6] * u + c.h[7] * v + 1.f;
    return gmv(sx * ((c.h[0] * u + c.h[1] * v + c.h[2]) / z - u), sy * ((c.h[3] * u + c.h[4] * v + c.h[5]) / z - v));
}

// Patch of 8x8 taps; q is the tap spacing at the current level.
inline int gm_tap(int i, int q) { return (i - 4) * q + q / 2; }
inline float gm_cost(GM_DEVC float* A, GM_DEVC float* B, int off, int w, int h, int px, int py, int q, GmVec u) {
    float d[64];
    float mean = 0.f;
    for (int j = 0; j < 8; ++j) for (int i = 0; i < 8; ++i) {
        int x = px + gm_tap(i, q), y = py + gm_tap(j, q);
        float e = gm_bil(B, off, w, h, float(x) + u.x, float(y) + u.y) - gm_tex(A, off, w, h, x, y);
        d[j * 8 + i] = e; mean += e;
    }
    mean *= 1.f / 64.f;
    float cost = 0.f;
    for (int k = 0; k < 64; ++k) cost += abs(d[k] - mean);
    return cost;
}
// Repeating textures (tiles, strings, stripes) match several vectors equally
// well. Prefer the shortest one between equal costs.
inline float gm_score(float cost, GmVec u) { return cost + .0015f * (abs(u.x) + abs(u.y)); }
inline GmVec gm_lucasKanade(GM_DEVC float* A, GM_DEVC float* B, int off, int w, int h, int px, int py, int q, GmVec u, int iterations) {
    float gx[64], gy[64];
    float xx = 0.f, xy = 0.f, yy = 0.f;
    for (int j = 0; j < 8; ++j) for (int i = 0; i < 8; ++i) {
        int x = px + gm_tap(i, q), y = py + gm_tap(j, q), k = j * 8 + i;
        gx[k] = .5f * (gm_tex(A, off, w, h, x + 1, y) - gm_tex(A, off, w, h, x - 1, y));
        gy[k] = .5f * (gm_tex(A, off, w, h, x, y + 1) - gm_tex(A, off, w, h, x, y - 1));
        xx += gx[k] * gx[k]; xy += gx[k] * gy[k]; yy += gy[k] * gy[k];
    }
    float ridge = 1.e-9f + .002f * (xx + yy);
    xx += ridge; yy += ridge;
    float det = xx * yy - xy * xy;
    if (!(det > 1.e-14f)) return u;
    float limit = float(2 * q);
    for (int it = 0; it < iterations; ++it) {
        float d[64];
        float mean = 0.f;
        for (int j = 0; j < 8; ++j) for (int i = 0; i < 8; ++i) {
            int x = px + gm_tap(i, q), y = py + gm_tap(j, q);
            float e = gm_bil(B, off, w, h, float(x) + u.x, float(y) + u.y) - gm_tex(A, off, w, h, x, y);
            d[j * 8 + i] = e; mean += e;
        }
        mean *= 1.f / 64.f;
        float bx = 0.f, by = 0.f;
        for (int k = 0; k < 64; ++k) { float r = d[k] - mean; bx += gx[k] * r; by += gy[k] * r; }
        float dx = clamp((yy * bx - xy * by) / det, -limit, limit);
        float dy = clamp((xx * by - xy * bx) / det, -limit, limit);
        u.x -= dx; u.y -= dy;
        if (abs(dx) + abs(dy) < .005f) break;
    }
    return u;
}
// Flow from picture A (centre) to picture B (neighbour) for one node of one level.
inline GmVec gm_flowNode(GM_DEVC float* A, GM_DEVC float* B, GM_DEVC float* coarse, GM_CREF(GmGeometry) g,
                         GM_CREF(GmCamera) cam, int level, int i, int j) {
    int w = g.widths[level], h = g.heights[level], off = g.offsets[level];
    int px = min(i * g.grid, w - 1), py = min(j * g.grid, h - 1);
    int q = max(1, g.grid / 4);
    float scale = exp2(float(level));
    GmVec camera = gm_cameraShift(cam, g.width, g.height, (float(px) + .5f) * scale - .5f, (float(py) + .5f) * scale - .5f);
    camera.x /= scale; camera.y /= scale;
    GmVec best = gmv(0.f, 0.f);
    float bestCost = gm_cost(A, B, off, w, h, px, py, q, best);
    if (cam.h[8] != 0.f) {
        float c = gm_score(gm_cost(A, B, off, w, h, px, py, q, camera), camera);
        if (c < bestCost) { bestCost = c; best = camera; }
    }
    if (level + 1 < g.levels) {
        int cl = level + 1, fw = g.fieldW[cl], fh = g.fieldH[cl], foff = g.fieldOffsets[cl];
        float cx = .5f * float(px) / float(g.grid), cy = .5f * float(py) / float(g.grid);
        for (int k = 0; k < 5; ++k) {
            float ox = k == 1 ? -1.f : (k == 2 ? 1.f : 0.f), oy = k == 3 ? -1.f : (k == 4 ? 1.f : 0.f);
            GmVec v = gm_field(coarse, foff, fw, fh, cx + ox, cy + oy);
            v.x *= 2.f; v.y *= 2.f;
            float c = gm_score(gm_cost(A, B, off, w, h, px, py, q, v), v);
            if (c < bestCost) { bestCost = c; best = v; }
        }
    } else {
        // Top of the pyramid: exhaustive integer search around zero and the camera.
        for (int s = 0; s < 2; ++s) {
            if (s == 1 && cam.h[8] == 0.f) break;
            float bx = s == 0 ? 0.f : floor(camera.x + .5f), by = s == 0 ? 0.f : floor(camera.y + .5f);
            for (int v = -g.search; v <= g.search; ++v) for (int u = -g.search; u <= g.search; ++u) {
                GmVec cand = gmv(bx + float(u), by + float(v));
                float c = gm_score(gm_cost(A, B, off, w, h, px, py, q, cand), cand);
                if (c < bestCost) { bestCost = c; best = cand; }
            }
        }
    }
    GmVec refined = gm_lucasKanade(A, B, off, w, h, px, py, q, best, g.iterations);
    if (gm_score(gm_cost(A, B, off, w, h, px, py, q, refined), refined) <= bestCost) best = refined;
    return best;
}
inline float gm_median9(float v0, float v1, float v2, float v3, float v4, float v5, float v6, float v7, float v8) {
    float a[9];
    a[0] = v0; a[1] = v1; a[2] = v2; a[3] = v3; a[4] = v4; a[5] = v5; a[6] = v6; a[7] = v7; a[8] = v8;
    for (int i = 1; i < 9; ++i) {
        float x = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > x) { a[j + 1] = a[j]; --j; }
        a[j + 1] = x;
    }
    return a[4];
}
// Remove isolated wrong matches without erasing thin structures that the
// picture itself supports (strings, fingers, hair).
inline GmVec gm_smoothNode(GM_DEVC float* A, GM_DEVC float* B, GM_DEVC float* raw, GM_CREF(GmGeometry) g, int level, int i, int j) {
    int fw = g.fieldW[level], fh = g.fieldH[level], foff = g.fieldOffsets[level];
    GmVec n0 = gm_node(raw, foff, fw, fh, i - 1, j - 1), n1 = gm_node(raw, foff, fw, fh, i, j - 1), n2 = gm_node(raw, foff, fw, fh, i + 1, j - 1);
    GmVec n3 = gm_node(raw, foff, fw, fh, i - 1, j), own = gm_node(raw, foff, fw, fh, i, j), n5 = gm_node(raw, foff, fw, fh, i + 1, j);
    GmVec n6 = gm_node(raw, foff, fw, fh, i - 1, j + 1), n7 = gm_node(raw, foff, fw, fh, i, j + 1), n8 = gm_node(raw, foff, fw, fh, i + 1, j + 1);
    GmVec med = gmv(gm_median9(n0.x, n1.x, n2.x, n3.x, own.x, n5.x, n6.x, n7.x, n8.x),
                    gm_median9(n0.y, n1.y, n2.y, n3.y, own.y, n5.y, n6.y, n7.y, n8.y));
    if (abs(med.x - own.x) + abs(med.y - own.y) < .25f) return own;
    int w = g.widths[level], h = g.heights[level], off = g.offsets[level];
    int px = min(i * g.grid, w - 1), py = min(j * g.grid, h - 1);
    int q = max(1, g.grid / 4);
    float costOwn = gm_score(gm_cost(A, B, off, w, h, px, py, q, own), own);
    float costMed = gm_score(gm_cost(A, B, off, w, h, px, py, q, med), med);
    return costMed <= costOwn * 1.08f + 1.e-5f ? med : own;
}
// Level 0, both neighbours known: a node whose patch is being covered or
// uncovered matches only on one side. Continue the visible side's motion.
inline GmPair gm_crossNode(GM_DEVC float* C, GM_DEVC float* P, GM_DEVC float* N, GM_DEVC float* fieldP, GM_DEVC float* fieldN,
                           GM_CREF(GmGeometry) g, int i, int j) {
    int w = g.widths[0], h = g.heights[0], fw = g.fieldW[0], fh = g.fieldH[0];
    int px = min(i * g.grid, w - 1), py = min(j * g.grid, h - 1), q = max(1, g.grid / 4);
    GmPair r;
    r.p = gm_node(fieldP, 0, fw, fh, i, j); r.n = gm_node(fieldN, 0, fw, fh, i, j);
    float ep = gm_cost(C, P, 0, w, h, px, py, q, r.p), en = gm_cost(C, N, 0, w, h, px, py, q, r.n);
    if (en > 2.f * ep + .1f) {
        GmVec alt = gmv(-r.p.x, -r.p.y);
        if (gm_cost(C, N, 0, w, h, px, py, q, alt) < en) r.n = alt;
    } else if (ep > 2.f * en + .1f) {
        GmVec alt = gmv(-r.n.x, -r.n.y);
        if (gm_cost(C, P, 0, w, h, px, py, q, alt) < ep) r.p = alt;
    }
    return r;
}
// Separable selection, in a (2R+1)^2 node window, of the vector that departs
// most from the dominant camera motion. Surfaces moving against the camera
// motion (hands, faces, instruments, a tracked subject) are usually in front.
inline float gm_deviation(GmVec v, GM_CREF(GmCamera) cam, GM_CREF(GmParams) p, float x, float y) {
    GmVec c = gm_cameraShift(cam, p.width, p.height, x, y);
    return gm_len2(gmv(v.x - c.x, v.y - c.y));
}
inline GmVec gm_tileMax(GM_DEVC float* f, GM_CREF(GmParams) p, int side, int i, int j, int horizontal) {
    int fw = p.fieldW, fh = p.fieldH;
    float x = float(min(i * p.grid, p.width - 1)), y = float(min(j * p.grid, p.height - 1));
    GmVec best = gm_node(f, 0, fw, fh, i, j);
    float score = side == 0 ? gm_deviation(best, p.camP, p, x, y) : gm_deviation(best, p.camN, p, x, y);
    for (int k = -p.reach; k <= p.reach; ++k) {
        if (k == 0) continue;
        GmVec v = horizontal != 0 ? gm_node(f, 0, fw, fh, i + k, j) : gm_node(f, 0, fw, fh, i, j + k);
        float l = side == 0 ? gm_deviation(v, p.camP, p, x, y) : gm_deviation(v, p.camN, p, x, y);
        if (l > score) { score = l; best = v; }
    }
    return best;
}
// Per-pixel flow. Each node near the pixel proposes its pair of vectors (to the
// previous and to the next picture); proposals are weighted by how well they
// explain a 3x3 neighbourhood of THIS pixel in both pictures. A background
// pixel next to a moving hand rejects the hand's vectors and vice versa, so
// motion boundaries follow the image instead of the node grid. Where one side
// is occluded the other side still decides.
inline float gm_patchError(GM_DEVC float* A, GM_DEVC float* B, int w, int h, int x, int y, GmVec v) {
    float e = 0.f;
    for (int j = -1; j <= 1; ++j) for (int i = -1; i <= 1; ++i)
        e += abs(gm_bil(B, 0, w, h, float(x + i) + v.x, float(y + j) + v.y) - gm_tex(A, 0, w, h, x + i, y + j));
    return e;
}
inline GmPair gm_dense(GM_DEVC float* fieldP, GM_DEVC float* fieldN, int fw, int fh, int grid,
                       GM_DEVC float* lumaC, GM_DEVC float* lumaP, GM_DEVC float* lumaN,
                       int hasPrev, int hasNext, int w, int h, int x, int y, GM_CREF(GmCamera) camP, GM_CREF(GmCamera) camN) {
    int i0 = x / grid, j0 = y / grid;
    float sw = 0.f, spx = 0.f, spy = 0.f, snx = 0.f, sny = 0.f;
    float radius = 1.5f * float(grid);
    for (int dj = -1; dj <= 2; ++dj) for (int di = -1; di <= 2; ++di) {
        int i = i0 + di, j = j0 + dj;
        if (i < 0 || j < 0 || i >= fw || j >= fh) continue;
        float nx = float(min(i * grid, w - 1)), ny = float(min(j * grid, h - 1));
        float tx = 1.f - abs(float(x) - nx) / radius, ty = 1.f - abs(float(y) - ny) / radius;
        if (tx <= 0.f || ty <= 0.f) continue;
        GmVec vp = hasPrev != 0 ? gm_node(fieldP, 0, fw, fh, i, j) : gmv(0.f, 0.f);
        GmVec vn = hasNext != 0 ? gm_node(fieldN, 0, fw, fh, i, j) : gmv(0.f, 0.f);
        if (hasPrev == 0) vp = gmv(-vn.x, -vn.y);
        if (hasNext == 0) vn = gmv(-vp.x, -vp.y);
        float ep = hasPrev != 0 ? gm_patchError(lumaC, lumaP, w, h, x, y, vp) : -1.f;
        float en = hasNext != 0 ? gm_patchError(lumaC, lumaN, w, h, x, y, vn) : -1.f;
        float cost = ep < 0.f ? en : (en < 0.f ? ep : min(ep, en) + .25f * max(ep, en));
        float e = .012f + cost;
        float wt = tx * ty / (e * e);
        sw += wt; spx += wt * vp.x; spy += wt * vp.y; snx += wt * vn.x; sny += wt * vn.y;
    }
    GmPair r;
    r.p = gmv(0.f, 0.f); r.n = gmv(0.f, 0.f);
    if (sw > 0.f) { r.p = gmv(spx / sw, spy / sw); r.n = gmv(snx / sw, sny / sw); }
    // Occlusion: a surface about to be covered (or just uncovered) has no match
    // on one side. Continue the visible side's motion through the centre.
    if (hasPrev != 0 && hasNext != 0) {
        float ep = gm_patchError(lumaC, lumaP, w, h, x, y, r.p), en = gm_patchError(lumaC, lumaN, w, h, x, y, r.n);
        float ap = gm_patchError(lumaC, lumaP, w, h, x, y, gmv(-r.n.x, -r.n.y));
        float an = gm_patchError(lumaC, lumaN, w, h, x, y, gmv(-r.p.x, -r.p.y));
        if (en > 2.f * ep + .03f && an < en) r.n = gmv(-r.p.x, -r.p.y);
        else if (ep > 2.f * en + .03f && ap < ep) r.p = gmv(-r.n.x, -r.n.y);
        else if (camP.h[8] != 0.f && camN.h[8] != 0.f) {
            // Next to a moving subject the background is covered on one side
            // and a repetitive background can still match somewhere wrong.
            // When one side follows the camera and the other is unrelated,
            // the camera-following side is the background: mirror its residual.
            GmVec cp = gm_cameraShift(camP, w, h, float(x), float(y)), cn = gm_cameraShift(camN, w, h, float(x), float(y));
            GmVec rp = gmv(r.p.x - cp.x, r.p.y - cp.y), rn = gmv(r.n.x - cn.x, r.n.y - cn.y);
            float tol = 1.5f + .08f * max(gm_len(cp), gm_len(cn));
            float lp = gm_len(rp), ln = gm_len(rn);
            if (lp < tol && ln > 3.f * tol && ln > 2.f * lp) r.n = gmv(cn.x - rp.x, cn.y - rp.y);
            else if (ln < tol && lp > 3.f * tol && lp > 2.f * ln) r.p = gmv(cp.x - rn.x, cp.y - rn.y);
        }
    }
    return r;
}
inline GmColor gm_sample(GM_DEVC float* img, int w, int h, float x, float y, float alpha) {
    x = clamp(x, 0.f, float(w - 1)); y = clamp(y, 0.f, float(h - 1));
    int ix = int(x), iy = int(y);
    float fx = x - float(ix), fy = y - float(iy);
    GmColor c; c.r = 0.f; c.g = 0.f; c.b = 0.f; c.ok = 1.f;
    for (int j = 0; j < 2; ++j) for (int i = 0; i < 2; ++i) {
        float wt = (i == 1 ? fx : 1.f - fx) * (j == 1 ? fy : 1.f - fy);
        if (wt <= 0.f) continue;
        int k = 4 * (min(iy + j, h - 1) * w + min(ix + i, w - 1));
        if (!(abs(img[k + 3] - alpha) <= 1.e-5f)) c.ok = 0.f;
        c.r += wt * img[k]; c.g += wt * img[k + 1]; c.b += wt * img[k + 2];
    }
    return c;
}
// 1 when two observations of the same surface point agree, 0 when they are
// clearly different surfaces.
inline float gm_agree(GmColor a, GmColor b) {
    float diff = max(abs(a.r - b.r), max(abs(a.g - b.g), abs(a.b - b.b)));
    float peak = max(.03f, max(max(abs(a.r), abs(b.r)), max(max(abs(a.g), abs(b.g)), max(abs(a.b), abs(b.b)))));
    return clamp(1.f - diff / (.02f + .35f * peak), 0.f, 1.f);
}
inline GmColor gm_mix(GmColor a, GmColor b, float t) {
    GmColor r; r.r = a.r + (b.r - a.r) * t; r.g = a.g + (b.g - a.g) * t; r.b = a.b + (b.b - a.b) * t; r.ok = a.ok;
    return r;
}
inline void gm_prepare(GM_DEVC float* src, GM_DEV float* lin, GM_DEV float* luma, int w, int curve, int x, int y) {
    int i = y * w + x;
    float r = gm_decode(src[4 * i], curve), gg = gm_decode(src[4 * i + 1], curve), b = gm_decode(src[4 * i + 2], curve);
    float a = src[4 * i + 3];
    lin[4 * i] = r; lin[4 * i + 1] = gg; lin[4 * i + 2] = b; lin[4 * i + 3] = a;
    luma[i] = gm_perceptual(r, gg, b);
}
inline void gm_reduce(GM_DEV float* pyr, GM_CREF(GmGeometry) g, int level, int x, int y) {
    int pw = g.widths[level - 1], ph = g.heights[level - 1], poff = g.offsets[level - 1];
    float s = gm_tex(pyr, poff, pw, ph, 2 * x, 2 * y) + gm_tex(pyr, poff, pw, ph, 2 * x + 1, 2 * y)
            + gm_tex(pyr, poff, pw, ph, 2 * x, 2 * y + 1) + gm_tex(pyr, poff, pw, ph, 2 * x + 1, 2 * y + 1);
    pyr[g.offsets[level] + y * g.widths[level] + x] = .25f * s;
}
// Integrate the light reaching pixel (x,y) over the added exposure.
// t in [-exposure/2, exposure/2] is measured in neighbour intervals.
inline void gm_integrate(GM_DEVC float* src, GM_DEVC float* linC, GM_DEVC float* linP, GM_DEVC float* linN,
                         GM_DEVC float* lumaC, GM_DEVC float* lumaP, GM_DEVC float* lumaN,
                         GM_DEVC float* fieldP, GM_DEVC float* fieldN, GM_DEVC float* tileP, GM_DEVC float* tileN,
                         GM_CREF(GmParams) p, int x, int y, GM_DEV float* out) {
    int w = p.width, h = p.height, idx = 4 * (y * w + x);
    out[0] = src[idx]; out[1] = src[idx + 1]; out[2] = src[idx + 2]; out[3] = src[idx + 3];
    if (p.hasPrev == 0 && p.hasNext == 0) return;
    float alpha = src[idx + 3];
    GmVec vP = gmv(0.f, 0.f), vN = gmv(0.f, 0.f), mP = gmv(0.f, 0.f), mN = gmv(0.f, 0.f);
    int ti = clamp((x + p.grid / 2) / p.grid, 0, p.fieldW - 1), tj = clamp((y + p.grid / 2) / p.grid, 0, p.fieldH - 1);
    GmPair flow = gm_dense(fieldP, fieldN, p.fieldW, p.fieldH, p.grid, lumaC, lumaP, lumaN, p.hasPrev, p.hasNext, w, h, x, y, p.camP, p.camN);
    vP = flow.p; vN = flow.n;
    if (p.hasPrev != 0) mP = gm_node(tileP, 0, p.fieldW, p.fieldH, ti, tj);
    if (p.hasNext != 0) mN = gm_node(tileN, 0, p.fieldW, p.fieldH, ti, tj);
    if (p.hasPrev == 0) mP = gmv(-mN.x, -mN.y);
    if (p.hasNext == 0) { vN = gmv(-vP.x, -vP.y); mN = gmv(-mP.x, -mP.y); }
    float halfExposure = .5f * p.exposure;
    float reach = halfExposure * max(max(gm_len(vP), gm_len(vN)), max(gm_len(mP), gm_len(mN)));
    if (reach < .025f) return;
    if (p.hasPrev != 0 && p.hasNext != 0 && halfExposure * max(gm_len(mP), gm_len(mN)) < .5f) {
        bool still = true;
        for (int dx = -1; dx <= 1; ++dx) {
            int k = 4 * (y * w + clamp(x + dx, 0, w - 1));
            for (int c = 0; c < 4; ++c) still = still && linC[k + c] == linP[k + c] && linC[k + c] == linN[k + c];
        }
        if (still) return;
    }
    GmColor self = gm_sample(linC, w, h, float(x), float(y), alpha);
    // A turnaround or acceleration inside the exposure: fit a parabola when
    // both neighbours really show this surface where the vectors point.
    bool curved = false;
    if (p.curved != 0 && p.hasPrev != 0 && p.hasNext != 0) {
        float a2 = gm_len2(vP), b2 = gm_len2(vN);
        curved = max(a2, b2) < 4.f * min(a2, b2);
        if (curved && vP.x * vN.x + vP.y * vN.y >= 0.f) {
            GmColor e = gm_sample(linP, w, h, float(x) + vP.x, float(y) + vP.y, alpha);
            GmColor l = gm_sample(linN, w, h, float(x) + vN.x, float(y) + vN.y, alpha);
            curved = e.ok > 0.f && l.ok > 0.f && gm_agree(self, e) > .5f && gm_agree(self, l) > .5f;
        }
    }
    int n = clamp(int(ceil(3.f * reach)) + 1, 2, p.maxSamples);
    float sr = 0.f, sg = 0.f, sb = 0.f;
    for (int k = 0; k < n; ++k) {
        float t = ((float(k) + .5f) / float(n) - .5f) * p.exposure;
        float f = abs(t);
        bool before = t < 0.f;
        GmVec vs = before ? vP : vN, ms = before ? mP : mN;
        bool side = before ? p.hasPrev != 0 : p.hasNext != 0;
        GM_DEVC float* other = before ? linP : linN;
        GM_DEVC float* otherField = before ? fieldP : fieldN;
        GmVec d = gmv(f * vs.x, f * vs.y);
        if (curved && f <= 1.f) d = gmv(.5f * (t * (vN.x - vP.x) + t * t * (vP.x + vN.x)), .5f * (t * (vN.y - vP.y) + t * t * (vP.y + vN.y)));
        float px = float(x) - d.x, py = float(y) - d.y;
        GmColor s = gm_sample(linC, w, h, px, py, alpha);
        if (s.ok <= 0.f) s = self;
        if (side && f <= 1.f) {
            GmColor o = gm_sample(other, w, h, px + vs.x, py + vs.y, alpha);
            if (o.ok > 0.f) s = gm_mix(s, o, f * gm_agree(s, o));
        }
        // A faster surface nearby (a hand, a string, a passing object) may
        // sweep across this pixel during the exposure, even if it does not
        // cover the pixel in the centre picture.
        GmVec dm = gmv(f * ms.x, f * ms.y);
        bool otherLayer = gm_len(gmv(ms.x - vs.x, ms.y - vs.y)) > .2f * gm_len(ms) + 1.f;
        float fx = float(x), fy = float(y);
        bool front = before ? gm_deviation(ms, p.camP, p, fx, fy) > gm_deviation(vs, p.camP, p, fx, fy)
                            : gm_deviation(ms, p.camN, p, fx, fy) > gm_deviation(vs, p.camN, p, fx, fy);
        if (otherLayer && front && abs(dm.x - d.x) + abs(dm.y - d.y) > .5f) {
            float zx = float(x) - dm.x, zy = float(y) - dm.y;
            GmVec vz = gm_field(side ? otherField : (before ? fieldN : fieldP), 0, p.fieldW, p.fieldH, zx / float(p.grid), zy / float(p.grid));
            if (!side) vz = gmv(-vz.x, -vz.y);
            float cover = clamp(1.f - gm_len(gmv(vz.x - ms.x, vz.y - ms.y)) / (.2f * gm_len(ms) + 1.f), 0.f, 1.f);
            if (cover > 0.f) {
                GmColor c2 = gm_sample(linC, w, h, zx, zy, alpha);
                float visible = c2.ok;
                if (side && visible > 0.f) {
                    GmColor e = gm_sample(other, w, h, zx + ms.x, zy + ms.y, alpha);
                    visible = e.ok > 0.f ? gm_agree(c2, e) : 0.f;
                }
                s = gm_mix(s, c2, cover * visible);
            }
        }
        sr += s.r; sg += s.g; sb += s.b;
    }
    float inv = 1.f / float(n);
    out[0] = gm_encode(sr * inv, p.curve); out[1] = gm_encode(sg * inv, p.curve); out[2] = gm_encode(sb * inv, p.curve);
    out[3] = alpha;
}
)

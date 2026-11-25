// SPDX-License-Identifier: GPL-3.0-or-later
// The Creative node on the GPU: the passes of src/creative/CreativeEngine.cpp, each calling the same function of
// math/Creative.h. Compiled at runtime after kMetalPrelude (metal_stdlib + DevelopMath.h).

struct CrBlur { int w, h, axis, shift; float sigma; };
struct CrGrid { int s, w, h, c; };

// The three records, each from its own frame (the same buffer when the records are simultaneous).
kernel void cr_record_k(const device float* s0 [[buffer(0)]], const device float* s1 [[buffer(1)]],
                        const device float* s2 [[buffer(2)]], device float* E [[buffer(3)]],
                        constant CreativeParams& p [[buffer(4)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= p.W || y >= p.H) return;
    const int N = p.W * p.H, k = y * p.W + x;
    const SMf3 e = cr_records(s0, s1, s2, x, y, p);
    E[k] = e.x;
    E[N + k] = e.y;
    E[2 * N + k] = e.z;
}

// The video camera's sensor, in place on E: a CCD's smear per column first (one thread per column), then every pixel
// (a tube reads the frames before, h0..h2).
kernel void cr_smear_k(const device float* E [[buffer(0)]], device float* S [[buffer(1)]],
                       constant CreativeParams& p [[buffer(2)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x;
    if (x >= p.W) return;
    const int N = p.W * p.H;
    cr_smear_col(E, E + N, E + 2 * N, S, x, p);
}

kernel void cr_sensor_k(device float* E [[buffer(0)]], const device float* h0 [[buffer(1)]],
                        const device float* h1 [[buffer(2)]], const device float* h2 [[buffer(3)]],
                        const device float* S [[buffer(4)]], constant CreativeParams& p [[buffer(5)]],
                        uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= p.W || y >= p.H) return;
    const int N = p.W * p.H, k = y * p.W + x;
    const SMf3 v = cr_sensor(smf3(E[k], E[N + k], E[2 * N + k]), h0, h1, h2, S, x, y, p);
    E[k] = v.x;
    E[N + k] = v.y;
    E[2 * N + k] = v.z;
}

kernel void cr_down_k(const device float* E [[buffer(0)]], device float* out [[buffer(1)]],
                      constant CrGrid& g [[buffer(2)]], constant CreativeParams& p [[buffer(3)]],
                      uint2 id [[thread_position_in_grid]])
{
    const int i = (int)id.x, j = (int)id.y;
    if (i >= g.w || j >= g.h) return;
    const int N = p.W * p.H;
    out[j * g.w + i] = cr_down_at(E, E + N, E + 2 * N, i, j, g.s, g.c, p);
}

kernel void cr_blur_k(const device float* in [[buffer(0)]], device float* out [[buffer(1)]],
                      constant CrBlur& b [[buffer(2)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= b.w || y >= b.h) return;
    out[y * b.w + x] = cr_blur_at(in, b.w, b.h, x, y, b.axis, b.sigma, b.shift);
}

// Process: the output of every pixel. Unused grids are bound to E (never read).
kernel void cr_process_k(const device float* src [[buffer(0)]], device float* dst [[buffer(1)]],
                         const device float* E [[buffer(2)]], const device float* hal [[buffer(3)]],
                         const device float* bl [[buffer(4)]], constant CreativeParams& p [[buffer(5)]],
                         uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= p.W || y >= p.H) return;
    const int N = p.W * p.H, G = p.bW * p.bH;
    const SMf3 e = cr_glow_at(E, E + N, E + 2 * N, hal, bl, bl + G, bl + 2 * G, x, y, p);
    const SMf3 o = cr_process(e, x, y, p);
    const int d = (y * p.dstRowPixels + x) * 4;
    dst[d] = o.x;
    dst[d + 1] = o.y;
    dst[d + 2] = o.z;
    dst[d + 3] = src[(y * p.rowPixels + x) * 4 + 3];
}

// Signal chains: Y Cb Cr planes of every pixel, glows included.
kernel void cr_signal_k(const device float* E [[buffer(0)]], const device float* hal [[buffer(1)]],
                        const device float* bl [[buffer(2)]], device float* V [[buffer(3)]],
                        constant CreativeParams& p [[buffer(4)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= p.W || y >= p.H) return;
    const int N = p.W * p.H, G = p.bW * p.bH, k = y * p.W + x;
    const SMf3 v = cr_signal(cr_glow_at(E, E + N, E + 2 * N, hal, bl, bl + G, bl + 2 * G, x, y, p), x, y, p);
    V[k] = v.x;
    V[N + k] = v.y;
    V[2 * N + k] = v.z;
}

kernel void cr_signal_out_k(const device float* src [[buffer(0)]], device float* dst [[buffer(1)]],
                            const device float* Y [[buffer(2)]], const device float* Yb [[buffer(3)]],
                            const device float* Cb [[buffer(4)]], const device float* Cr [[buffer(5)]],
                            constant CreativeParams& p [[buffer(6)]], uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= p.W || y >= p.H) return;
    const SMf3 o = cr_signal_at(Y, Yb, Cb, Cr, x, y, p);
    const int d = (y * p.dstRowPixels + x) * 4;
    dst[d] = o.x;
    dst[d + 1] = o.y;
    dst[d + 2] = o.z;
    dst[d + 3] = src[(y * p.rowPixels + x) * 4 + 3];
}

// Analog tape: one thread per output row (the filters run along the line), then every pixel.
kernel void cr_tape_row_k(const device float* V [[buffer(0)]], device float* T [[buffer(1)]],
                          constant CreativeParams& p [[buffer(2)]], uint2 id [[thread_position_in_grid]])
{
    const int y = (int)id.x;
    if (y >= p.H) return;
    const int N = p.W * p.H;
    cr_tape_row(V, V + N, V + 2 * N, T, T + 2 * N, T + 3 * N, y, p);
}

kernel void cr_tape_out_k(const device float* src [[buffer(0)]], device float* dst [[buffer(1)]],
                          const device float* T [[buffer(2)]], constant CreativeParams& p [[buffer(3)]],
                          uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= p.W || y >= p.H) return;
    const int N = p.W * p.H;
    const SMf3 o = cr_tape_out(T, T + 2 * N, T + 3 * N, x, y, p);
    const int d = (y * p.dstRowPixels + x) * 4;
    dst[d] = o.x;
    dst[d + 1] = o.y;
    dst[d + 2] = o.z;
    dst[d + 3] = src[(y * p.rowPixels + x) * 4 + 3];
}

// Viewing medium: the finished picture decoded into M, then shown through the screen or on paper (alpha kept).
kernel void cr_medium_in_k(const device float* dst [[buffer(0)]], const device float* prev [[buffer(1)]],
                           device float* M [[buffer(2)]], constant CreativeParams& p [[buffer(3)]],
                           uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= p.W || y >= p.H) return;
    const int N = p.W * p.H, k = y * p.W + x;
    const SMf3 v = cr_medium_in(dst, prev, x, y, p);
    M[k] = v.x;
    M[N + k] = v.y;
    M[2 * N + k] = v.z;
}

kernel void cr_medium_k(const device float* M [[buffer(0)]], const device float* glow [[buffer(1)]],
                        device float* dst [[buffer(2)]], constant CreativeParams& p [[buffer(3)]],
                        uint2 id [[thread_position_in_grid]])
{
    const int x = (int)id.x, y = (int)id.y;
    if (x >= p.W || y >= p.H) return;
    const int N = p.W * p.H;
    const SMf3 o = cr_medium_at(M, M + N, M + 2 * N, glow, x, y, p);
    const int d = (y * p.dstRowPixels + x) * 4;
    dst[d] = o.x;
    dst[d + 1] = o.y;
    dst[d + 2] = o.z;
}

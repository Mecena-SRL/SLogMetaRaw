// SPDX-License-Identifier: GPL-3.0-or-later
// The Noise pipeline on synthetic frames: prints name=value lines that tests/test_noise.py checks.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "src/noise/NoiseEngine.h"
#include "src/noise/NoiseProfiles.h"

static const int W = 192, H = 128;

static void serial(int n, const std::function<void(int, int)>& fn) { fn(0, n); }

// A clean scene in code values: a gradient, a dark wall, hard edges, fine texture and colour.
static float clean(int x, int y, int c, float dx = 0.0f, float dy = 0.0f)
{
    const float fx = x - dx, fy = y - dy;
    float v = 0.15f + 0.6f * fx / W;
    if (fy > H * 0.55f) v = 0.2f;
    if (fx > W * 0.3f && fx < W * 0.5f && fy > H * 0.2f && fy < H * 0.45f) v = 0.75f;
    v += 0.04f * std::sin(fx * 0.35f) * std::cos(fy * 0.3f);   // texture of about 18 px
    const float tint[3] = { 1.0f, 0.95f, 0.85f };
    return v * tint[c];
}

struct Img
{
    std::vector<float> px;
    NoiseFrame frame() const { return { px.data(), (size_t)W * 4 }; }
};

static Img make(float sigma, unsigned seed, float dx = 0.0f, float dy = 0.0f, bool flat = false)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> n(0.0f, 1.0f);
    Img im;
    im.px.resize((size_t)W * H * 4);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (int c = 0; c < 4; ++c) {
                float v = c == 3 ? 1.0f : (flat ? 0.1f + 0.8f * x / W : clean(x, y, c, dx, dy));
                if (c < 3) v += sigma * n(rng);
                im.px[((size_t)y * W + x) * 4 + c] = v;
            }
    return im;
}

static double rmse(const std::vector<float>& a, float dx = 0.0f, float dy = 0.0f, int margin = 8)
{
    double e = 0.0;
    long n = 0;
    for (int y = margin; y < H - margin; ++y)
        for (int x = margin; x < W - margin; ++x)
            for (int c = 0; c < 3; ++c) {
                const double d = a[((size_t)y * W + x) * 4 + c] - clean(x, y, c, dx, dy);
                e += d * d;
                ++n;
            }
    return std::sqrt(e / n);
}

int main()
{
    NoiseScratch s;
    const float sigma = 0.02f;

    // 1. measure: a smooth ramp with white noise of known σ
    const Img flat = make(sigma, 1, 0, 0, true);
    NoiseProfile measured;
    const bool ok = noiseMeasure(W, H, flat.frame(), serial, s, measured);
    printf("measure_ok=%d\n", ok ? 1 : 0);
    printf("measured_luma0=%.5f\n", measured.luma[8][0]);   // white noise: 0.889 σ
    printf("measured_chroma0=%.5f\n", measured.chroma[8][0]);
    printf("expected_band0=%.5f\n", 0.889f * sigma);
    const std::string text = noiseProfileToText(measured);
    const NoiseProfile back = noiseProfileFromText(text);
    printf("text_roundtrip=%d\n", back.valid && std::fabs(back.luma[8][0] - measured.luma[8][0]) < 1e-6f ? 1 : 0);
    printf("text_rejects_garbage=%d\n", noiseProfileFromText("np1,abc").valid ? 0 : 1);

    // 2. spatial: noisy scene, measured profile
    const Img noisy = make(sigma, 2);
    std::vector<float> out((size_t)W * H * 4);
    NoiseControls c;
    c.temporal = 0.0f;
    NoiseFrame f = noisy.frame();
    noiseRenderCPU(W, H, &f, 1, 0, measured, c, out.data(), (size_t)W * 4, serial, s);
    printf("noisy_rmse=%.5f\n", rmse(noisy.px));
    printf("spatial_rmse=%.5f\n", rmse(out));
    printf("alpha_kept=%d\n", out[3] == noisy.px[3] ? 1 : 0);

    // 3. strength 0: the picture comes back
    NoiseControls zero;
    zero.luma = zero.chroma = 0.0f;
    zero.temporal = 0.0f;
    noiseRenderCPU(W, H, &f, 1, 0, measured, zero, out.data(), (size_t)W * 4, serial, s);
    double maxd = 0.0;
    for (size_t i = 0; i < out.size(); ++i) maxd = std::max(maxd, (double)std::fabs(out[i] - noisy.px[i]));
    printf("zero_maxdiff=%.8f\n", maxd);

    // 4. temporal, still scene: five frames with independent noise, spatial off
    std::vector<Img> still;
    for (int k = 0; k < 5; ++k) still.push_back(make(sigma, 10 + k));
    std::vector<NoiseFrame> fr;
    for (auto& im : still) fr.push_back(im.frame());
    NoiseControls tonly;
    tonly.luma = tonly.chroma = 0.0f;
    noiseRenderCPU(W, H, fr.data(), 5, 2, measured, tonly, out.data(), (size_t)W * 4, serial, s);
    printf("still_noisy_rmse=%.5f\n", rmse(still[2].px));
    printf("still_temporal_rmse=%.5f\n", rmse(out));

    // 5. temporal, moving scene: 3 px right and 2 px down per frame; the result must sit on the centre frame
    std::vector<Img> moving;
    for (int k = 0; k < 5; ++k) moving.push_back(make(sigma, 20 + k, 3.0f * (k - 2), 2.0f * (k - 2)));
    fr.clear();
    for (auto& im : moving) fr.push_back(im.frame());
    noiseRenderCPU(W, H, fr.data(), 5, 2, measured, tonly, out.data(), (size_t)W * 4, serial, s);
    printf("moving_noisy_rmse=%.5f\n", rmse(moving[2].px));
    printf("moving_temporal_rmse=%.5f\n", rmse(out));

    // 6. a cut: neighbours from another picture must not leak in
    std::vector<Img> cut = { make(sigma, 30, 40, 30), make(sigma, 31, 40, 30), make(sigma, 32), make(sigma, 33),
                             make(sigma, 34) };
    for (int k = 0; k < 2; ++k)   // another scene: inverted
        for (size_t i = 0; i < cut[k].px.size(); ++i)
            if (i % 4 != 3) cut[k].px[i] = 1.0f - cut[k].px[i];
    fr.clear();
    for (auto& im : cut) fr.push_back(im.frame());
    noiseRenderCPU(W, H, fr.data(), 5, 2, measured, tonly, out.data(), (size_t)W * 4, serial, s);
    printf("cut_temporal_rmse=%.5f\n", rmse(out));

    // 7. both together
    NoiseControls all;
    fr.clear();
    for (auto& im : moving) fr.push_back(im.frame());
    noiseRenderCPU(W, H, fr.data(), 5, 2, measured, all, out.data(), (size_t)W * 4, serial, s);
    printf("full_rmse=%.5f\n", rmse(out));

    // 8. the model profiles of the five cameras: finite, growing towards the blacks
    struct Case { const char* name; int gamma; const char* model; const char* codec; double iso, mbps; int depth;
                  const char* chroma; int w, h; double fps; };
    const Case cases[] = {
        { "fx6_xavci", 9, "ILME-FX6V", "intra", 800, 240, 10, "4:2:2", 3840, 2160, 23.976 },
        { "fx6_xavcl_high", 9, "ILME-FX6V", "long8", 12800, 150, 8, "4:2:0", 3840, 2160, 25 },
        { "fx6_raw", 9, "ILME-FX6V", "raw", 800, 0, 12, "", 4096, 2160, 25 },
        { "fx30_hs", 9, "ILME-FX30", "long", 2500, 140, 10, "4:2:0", 3840, 2160, 25 },
        { "fx30_120", 9, "ILME-FX30", "long", 800, 280, 10, "4:2:2", 3840, 2160, 119.88 },
        { "iphone_prores", 12, "iPhone 17 Pro", "intra", 400, 0, 10, "4:2:2", 3840, 2160, 30 },
        { "iphone_hevc", 12, "iPhone 17 Pro", "long", 100, 60, 10, "4:2:0", 3840, 2160, 30 },
        { "action4", 13, "OsmoAction4", "long", 1600, 130, 10, "4:2:0", 3840, 2160, 30 },
        { "action5", 13, "OsmoAction5 Pro", "long", 100, 120, 10, "4:2:0", 3840, 2160, 60 },
    };
    for (const Case& k : cases) {
        NoiseSource src;
        src.gamma = k.gamma;
        src.model = k.model;
        src.codec = k.codec;
        src.iso = k.iso;
        src.bitrate = k.mbps;
        src.bitDepth = k.depth;
        src.chroma = k.chroma;
        src.width = k.w;
        src.height = k.h;
        src.fps = k.fps;
        const NoiseModel m = noiseModel(src, k.w, 1.0);
        bool finite = m.profile.valid;
        for (int b = 0; b < kNoiseBins; ++b)
            for (int j = 0; j < kNoiseLevels; ++j)
                finite = finite && std::isfinite(m.profile.luma[b][j]) && m.profile.luma[b][j] >= 0.0f;
        printf("model_%s_finite=%d\n", k.name, finite ? 1 : 0);
        printf("model_%s_dark=%.5f\n", k.name, m.profile.luma[3][0] + m.profile.luma[3][1]);
        printf("model_%s_grey=%.5f\n", k.name, m.profile.luma[8][0] + m.profile.luma[8][1]);
        printf("model_%s_class=%d\n", k.name, (int)m.cls);
        printf("model_%s_radius=%d\n", k.name, m.radius);
        printf("model_%s_info=%s\n", k.name, m.info.c_str());
    }
    printf("OK\n");
    return 0;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoiseProfiles.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "../../gen/DevelopMath.h"

namespace {

std::string squash(const std::string& s)   // upper case, no spaces: "OsmoAction5 Pro" -> "OSMOACTION5PRO"
{
    std::string out;
    for (char ch : s)
        if (ch != ' ' && ch != '-' && ch != '_') out += (char)std::toupper((unsigned char)ch);
    return out;
}

// What the camera's own processing does before the file: the noise reduction it always applies and whether its
// ISO is a real gain (phones, action cameras) or, in Cine EI, metadata over a native gain (Sony).
struct Body
{
    const char* key;          // in the squashed model name
    const char* name;
    double native;            // ISO of the lowest gain, for a real-gain ISO
    bool realGain;            // ISO above native amplifies the recorded signal and its noise
    bool processed;           // heavy in-camera noise reduction (ISP): smeared fine grain, blotchy colour
};
const Body kBodies[] = {
    { "FX6", "Sony FX6", 800, false, false },
    { "FX30", "Sony FX30", 800, false, false },
    { "IPHONE17PRO", "iPhone 17 Pro", 100, true, true },          // native ISO estimated (CineD ~101)
    { "IPHONE", "iPhone", 55, true, true },
    { "OSMOACTION5", "DJI Osmo Action 5 Pro", 100, true, true },
    { "OSMOACTION4", "DJI Osmo Action 4", 100, true, true },
    { "ACTION", "DJI Osmo Action", 100, true, true },
};

// Where each camera's noise shows, in stops below grey at the native ISO with its internal codec (start: begins,
// full: everywhere): CineD latitude and IMATEST SNR = 2 tests, the ARRI per-EI table (from the Maschera rumore of
// Log MetaRaw 3.2, sources in docs/DENOISE.md). gamma = transfer code of math/Transfer.h; a model row also needs it.
struct Camera
{
    const char* model;     // in the squashed model name; nullptr = any camera on that curve
    int gamma;
    const char* name;
    double start, full;
    double low, high;      // native ISOs; low 0 = unknown, high 0 = a single one
    double switchAt;       // EI from which the camera reads with the high native gain
    const char* source;
};
const Camera kCameras[] = {
    { "FX6",    9,  "Sony FX6",          3.5, 5.0, 800, 12800, 12800, "misurata" },
    { "FX30",   9,  "Sony FX30",         3.0, 4.5, 800, 2500,  2500,  "stimata" },
    { "FX3",    9,  "Sony FX3",          3.0, 4.5, 800, 12800, 12800, "stimata (sensore A7S III)" },
    { "7SM3",   9,  "Sony A7S III",      3.0, 4.5, 640, 12800, 12800, "misurata" },
    { "7M4",    9,  "Sony A7 IV",        3.0, 4.5, 800, 0,     0,     "misurata" },
    { "GH6",    11, "Panasonic GH6",     2.5, 4.0, 800, 2000,  2000,  "misurata" },
    { "S5",     11, "Panasonic S5",      3.0, 4.5, 640, 4000,  4000,  "misurata" },
    { "POCKET", 23, "Blackmagic Pocket", 3.5, 5.0, 400, 3200,  1250,  "stimata" },
    { nullptr,  7,  "S-Log",             3.0, 4.5, 800, 0,     0,     "stimata" },
    { nullptr,  8,  "S-Log2",            3.0, 4.5, 800, 0,     0,     "stimata" },
    { nullptr,  9,  "S-Log3",            3.0, 4.5, 800, 0,     0,     "stimata" },
    { nullptr,  11, "V-Log",             3.0, 4.5, 640, 4000,  4000,  "stimata" },
    { nullptr,  12, "Apple Log",         1.5, 3.0, 55,  0,     0,     "derivata (iPhone 15 Pro)" },
    { nullptr,  13, "D-Log M",           2.0, 3.0, 100, 0,     0,     "stimata" },
    { nullptr,  14, "D-Log",             2.0, 3.0, 100, 0,     0,     "stimata" },
    { nullptr,  15, "F-Log",             3.5, 5.0, 640, 0,     0,     "stimata" },
    { nullptr,  16, "F-Log2",            3.5, 5.0, 1250, 0,    0,     "stimata" },
    { nullptr,  17, "N-Log",             3.5, 5.0, 800, 0,     0,     "stimata" },
    { nullptr,  18, "C-Log2",            3.5, 5.0, 800, 0,     0,     "stimata" },
    { nullptr,  19, "C-Log3",            3.5, 5.0, 800, 0,     0,     "stimata" },
    { nullptr,  20, "LogC3",             5.3, 6.9, 800, 0,     0,     "derivata (ARRI ALEV-3)" },
    { nullptr,  21, "LogC4",             6.1, 7.7, 800, 0,     0,     "derivata (ARRI ALEV-4)" },
    { nullptr,  22, "Log3G10",           3.5, 5.0, 800, 0,     0,     "stimata" },
    { nullptr,  23, "BMD Film Gen5",     3.5, 5.0, 400, 3200,  1250,  "stimata" },
    { nullptr,  24, "HLG",               2.0, 3.5, 0,   0,     0,     "stimata" },
};
const Camera kGenericCamera = { nullptr, -1, "camera log", 3.0, 4.5, 0, 0, 0, "stimata" };

const Camera& findCamera(int gamma, const std::string& key)
{
    for (const Camera& c : kCameras)
        if (c.model && c.gamma == gamma && key.find(c.model) != std::string::npos) return c;
    for (const Camera& c : kCameras)
        if (!c.model && c.gamma == gamma) return c;
    return kGenericCamera;
}

// What the recording does to the start of the noise, in stops (< 0: it shows earlier)
double codecShift(const std::string& codec)
{
    if (codec == "raw") return -0.5;     // FX6 ProRes RAW vs XAVC-I: 0.3-0.7 stop less at SNR = 2, no in-camera NR
    if (codec == "long8") return -0.5;   // estimate: blocks and banding
    return 0.0;
}

// Band shapes: the noise of each à trous band for a unit pixel noise. White noise gives 0.89 / 0.20 / 0.086 /
// 0.041 / 0.020; demosaicing, compression and in-camera noise reduction move it from the fine bands to the
// coarser ones (estimates, refined by Misura on real clips).
struct Shape { float luma[kNoiseLevels], chroma[kNoiseLevels], temporalCorr; int radius; const char* name; };
const Shape kShapes[] = {
    { { 0.80f, 0.32f, 0.14f, 0.06f, 0.03f }, { 0.75f, 0.40f, 0.20f, 0.09f, 0.04f }, 0.00f, 2, "RAW" },
    { { 0.80f, 0.32f, 0.14f, 0.06f, 0.03f }, { 0.70f, 0.40f, 0.20f, 0.09f, 0.04f }, 0.05f, 2, "Intra" },
    { { 0.55f, 0.32f, 0.16f, 0.07f, 0.035f }, { 0.45f, 0.40f, 0.22f, 0.10f, 0.05f }, 0.25f, 2, "Long GOP" },
    { { 0.35f, 0.28f, 0.16f, 0.08f, 0.04f }, { 0.30f, 0.35f, 0.25f, 0.12f, 0.06f }, 0.45f, 1, "Long GOP compresso" },
    { { 0.30f, 0.25f, 0.16f, 0.08f, 0.04f }, { 0.25f, 0.30f, 0.25f, 0.13f, 0.07f }, 0.40f, 1, "elaborato in camera" },
};

double num(const std::map<std::string, std::string>& r, const char* k)
{
    auto it = r.find(k);
    if (it == r.end()) return 0.0;
    const double v = atof(it->second.c_str());
    return std::isfinite(v) && v > 0.0 ? v : 0.0;
}

std::string str(const std::map<std::string, std::string>& r, const char* k)
{
    auto it = r.find(k);
    return it == r.end() ? std::string() : it->second;
}

std::string fmt(const char* f, double v)
{
    char buf[48];
    snprintf(buf, sizeof(buf), f, v);
    std::string s = buf;
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

// Noise (standard deviation) of the recorded linear signal y, grey = 0.18: Poisson-Gaussian σ² = a·y + b, fixed by the
// latitude table of the mask: SNR 4 where the noise starts to show, 1.5 where it is full.
void poissonGaussian(double start, double full, double& a, double& b)
{
    const double y1 = 0.18 * std::exp2(-start), y2 = 0.18 * std::exp2(-full);
    const double v1 = y1 * y1 / 16.0, v2 = y2 * y2 / 2.25;
    a = (v1 - v2) / (y1 - y2);
    if (a < 1e-7) a = 1e-7;   // the table would ask for less shot noise than none: keep a little
    b = std::max(v2 - a * y2, 1e-10);
}

}  // namespace

NoiseSource noiseSourceFromRecord(const std::map<std::string, std::string>& r)
{
    NoiseSource s;
    auto it = r.find("cam_gamma");
    s.gamma = it != r.end() && !it->second.empty() ? atoi(it->second.c_str()) : -1;
    s.model = str(r, "model");
    s.codec = str(r, "noise_codec");
    s.iso = num(r, "noise_iso") > 0.0 ? num(r, "noise_iso") : num(r, "shot_ei");
    s.bitrate = num(r, "noise_bitrate");
    s.bitDepth = (int)num(r, "noise_bit_depth");
    s.chroma = str(r, "noise_chroma");
    s.width = (int)num(r, "noise_width");
    s.height = (int)num(r, "noise_height");
    s.fps = num(r, "noise_fps");
    return s;
}

NoiseModel noiseModel(const NoiseSource& src, int frameWidth, double level)
{
    NoiseModel m;
    const std::string key = squash(src.model);
    const Body* body = nullptr;
    for (const Body& b : kBodies)
        if (!key.empty() && key.find(b.key) != std::string::npos) { body = &b; break; }
    const int gamma = src.gamma >= 0 ? src.gamma : 9;
    const Camera& cam = findCamera(gamma, key);
    const double shift = codecShift(src.codec);

    // ---- class of the recording
    double bpp = 0.0;
    if (src.bitrate > 0.0 && src.width > 0 && src.height > 0 && src.fps > 0.0)
        bpp = src.bitrate * 1e6 / ((double)src.width * src.height * src.fps);
    if (src.codec == "raw") m.cls = kClassRaw;
    else if (body && body->processed) m.cls = kClassProcessed;
    else if (src.codec == "intra") m.cls = kClassIntra;
    else if (src.codec == "long8" || src.bitDepth == 8 || (bpp > 0.0 && bpp < 0.4)) m.cls = kClassLongLow;
    else if (src.codec == "long") m.cls = kClassLongHigh;
    else m.cls = kClassIntra;   // unknown: the shape of an uncompressed recording, the safest for detail
    const Shape& shape = kShapes[m.cls];
    m.radius = shape.radius;

    // ---- noise of the recorded signal
    double a, b;
    // the codec offset of the mask (RAW / 8 bit: noise shows earlier) moves both points
    poissonGaussian(cam.start + shift, cam.full + shift, a, b);
    double gain = 1.0;
    std::string isoText;
    if (body && body->realGain) {
        // phones, action cameras: ISO above the lowest one is gain on the recorded signal and its noise
        const double iso = src.iso > 0.0 ? src.iso : body->native;
        gain = std::max(1.0, iso / body->native);
        isoText = "ISO " + fmt("%.0f", iso) + " (guadagno ×" + fmt("%.1f", gain) + ")";
        // the in-camera noise reduction takes part of the noise, more of it as the gain grows (estimate)
        if (body->processed) gain = 0.6 * std::pow(gain, 0.6);
    } else {
        // Cine EI: the file holds the native gain, the EI only says how to expose it in post
        const double ei = src.iso > 0.0 ? src.iso : 800.0;
        const bool high = cam.high > 0.0 && ei >= cam.switchAt;
        if (high) b *= 2.0;   // the high native gain costs about half a stop of shadows (FX6, A7S III, S5 II)
        const double native = high ? cam.high : (cam.low > 0.0 ? cam.low : ei);
        isoText = "EI " + fmt("%.0f", ei) + ", nativo " + fmt("%.0f", native) + (high ? " (alto)" : "");
    }
    // FX30 at 100/120 fps reads a 1.6x crop without the 6K oversampling: its pixels keep more noise (estimate)
    double crop = 1.0;
    if (key.find("FX30") != std::string::npos && src.fps >= 99.0) crop = 1.4;
    // a source larger than the frames the plugin gets is averaged down: its noise drops about by the ratio
    double scale = 1.0;
    if (src.width > 0 && frameWidth > 0 && src.width > frameWidth * 1.05) scale = (double)frameWidth / src.width;
    const double k = std::max(level, 0.0) * crop * scale;

    const bool chroma420 = src.chroma == "4:2:0" || (src.chroma.empty() && m.cls >= kClassLongLow);
    for (int bin = 0; bin < kNoiseBins; ++bin) {
        const float x = (float)bin / (float)(kNoiseBins - 1);
        double y = sm_decode(x, gamma);
        if (!(y > 1e-5)) y = 1e-5;
        // σ in code values: the recorded noise through the slope of the curve at that level
        const double h = std::max(1e-5, 0.02 * y);
        const double slope = std::fabs((double)sm_encode((float)(y + h), gamma) - (double)sm_encode((float)std::max(y - h, 1e-6), gamma))
                             / (y + h - std::max(y - h, 1e-6));
        const double sig = std::sqrt(gain * a * y + gain * gain * b) * slope * k;
        for (int j = 0; j < kNoiseLevels; ++j) {
            m.profile.luma[bin][j] = (float)(sig * 1.2 * shape.luma[j]);
            float c = shape.chroma[j];
            if (chroma420) c *= j == 0 ? 0.7f : (j == 1 ? 1.1f : 1.0f);   // halved colour resolution: coarser blotches
            m.profile.chroma[bin][j] = (float)(sig * c);
        }
    }
    m.profile.temporalCorr = shape.temporalCorr;
    m.profile.valid = true;

    std::string what = body ? body->name : cam.name;
    std::string fmtText = shape.name;
    if (src.bitDepth > 0) fmtText += " " + std::to_string(src.bitDepth) + " bit";
    if (!src.chroma.empty()) fmtText += " " + src.chroma;
    if (bpp > 0.0) fmtText += ", " + fmt("%.2f", bpp) + " bit/pixel";
    m.info = what + " · " + fmtText + " · " + isoText + (crop > 1.0 ? " · crop 120p" : "")
             + (scale < 1.0 ? " · ridotto a " + fmt("%.0f", scale * 100.0) + "%" : "") + " · " + cam.source;
    return m;
}

std::string noiseProfileToText(const NoiseProfile& p)
{
    if (!p.valid) return "";
    std::ostringstream o;
    o << "np1";
    char buf[32];
    for (int bin = 0; bin < kNoiseBins; ++bin)
        for (int j = 0; j < kNoiseLevels; ++j) {
            snprintf(buf, sizeof(buf), ",%.6g,%.6g", p.luma[bin][j], p.chroma[bin][j]);
            o << buf;
        }
    return o.str();
}

NoiseProfile noiseProfileFromText(const std::string& text)
{
    NoiseProfile p;
    if (text.compare(0, 3, "np1") != 0) return p;
    const char* s = text.c_str() + 3;
    for (int bin = 0; bin < kNoiseBins; ++bin)
        for (int j = 0; j < kNoiseLevels; ++j)
            for (int ch = 0; ch < 2; ++ch) {
                if (*s != ',') return NoiseProfile();
                char* end = nullptr;
                const double v = strtod(s + 1, &end);
                if (end == s + 1 || !std::isfinite(v) || v < 0.0 || v > 1.0) return NoiseProfile();
                (ch == 0 ? p.luma : p.chroma)[bin][j] = (float)v;
                s = end;
            }
    if (*s != '\0') return NoiseProfile();
    p.valid = true;
    return p;
}

NoiseSampleFix noiseSampleFix(const NoiseProfile& expected, const NoiseSample& measured)
{
    NoiseSampleFix f;
    if (!measured.valid) return f;
    int bin;
    float frac;
    noiseBin(measured.level, bin, frac);
    const int next = std::min(bin + 1, kNoiseBins - 1);
    for (int ch = 0; ch < 2; ++ch) {
        const float (*t)[kNoiseLevels] = ch == 0 ? expected.luma : expected.chroma;
        const float* m = ch == 0 ? measured.luma : measured.chroma;
        float* r = ch == 0 ? f.luma : f.chroma;
        float last = 1.0f;
        for (int j = 0; j < kNoiseLevels; ++j) {
            const float e = t[bin][j] + (t[next][j] - t[bin][j]) * frac;
            // a band the box was too small for keeps the correction of the finer one
            if (m[j] > 0.0f && e > 1e-6f) last = std::min(5.0f, std::max(0.2f, m[j] / e));
            r[j] = last;
        }
    }
    f.level = measured.level;
    f.valid = true;
    return f;
}

void applySampleFix(NoiseProfile& p, const NoiseSampleFix& f)
{
    if (!f.valid) return;
    for (int bin = 0; bin < kNoiseBins; ++bin)
        for (int j = 0; j < kNoiseLevels; ++j) {
            p.luma[bin][j] *= f.luma[j];
            p.chroma[bin][j] *= f.chroma[j];
        }
}

std::string noiseSampleToText(const NoiseSampleFix& f)
{
    if (!f.valid) return "";
    std::ostringstream o;
    char buf[32];
    snprintf(buf, sizeof(buf), "ns1,%.4g", f.level);
    o << buf;
    for (int j = 0; j < kNoiseLevels; ++j) {
        snprintf(buf, sizeof(buf), ",%.4g,%.4g", f.luma[j], f.chroma[j]);
        o << buf;
    }
    return o.str();
}

NoiseSampleFix noiseSampleFromText(const std::string& text)
{
    NoiseSampleFix f;
    if (text.compare(0, 3, "ns1") != 0) return f;
    const char* s = text.c_str() + 3;
    auto next = [&](float lo, float hi, float& out) {
        if (*s != ',') return false;
        char* end = nullptr;
        const double v = strtod(s + 1, &end);
        if (end == s + 1 || !std::isfinite(v) || v < lo * 0.999 || v > hi * 1.001) return false;
        out = std::min(hi, std::max(lo, (float)v));
        s = end;
        return true;
    };
    if (!next(0.0f, 1.0f, f.level)) return NoiseSampleFix();
    for (int j = 0; j < kNoiseLevels; ++j)
        if (!next(0.2f, 5.0f, f.luma[j]) || !next(0.2f, 5.0f, f.chroma[j])) return NoiseSampleFix();
    if (*s != '\0') return NoiseSampleFix();
    f.valid = true;
    return f;
}

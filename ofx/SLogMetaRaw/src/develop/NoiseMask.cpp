// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoiseMask.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace {

struct Row { const char* model; int gamma; NoiseCamera c; };

// gamma = transfer code of math/Transfer.h. A model row also needs its curve: a phone app may record another one.
// start / full: CineD latitude and IMATEST SNR = 2 tests, the ARRI per-EI table (docs/NOISE.md).
const Row kRows[] = {
    { "FX6",    9,  { -1, "Sony FX6",        3.5, 5.0, 800, 12800, 12800, "misurata", 0, "" } },
    { "FX30",   9,  { -1, "Sony FX30",       3.0, 4.5, 800, 2500,  2500,  "stimata",  0, "" } },
    { "FX3",    9,  { -1, "Sony FX3",        3.0, 4.5, 800, 12800, 12800, "stimata (sensore A7S III)", 0, "" } },
    { "7SM3",   9,  { -1, "Sony A7S III",    3.0, 4.5, 640, 12800, 12800, "misurata", 0, "" } },
    { "7M4",    9,  { -1, "Sony A7 IV",      3.0, 4.5, 800, 0,     0,     "misurata", 0, "" } },
    { "GH6",    11, { -1, "Panasonic GH6",   2.5, 4.0, 800, 2000,  2000,  "misurata", 0, "" } },
    { "S5",     11, { -1, "Panasonic S5",    3.0, 4.5, 640, 4000,  4000,  "misurata", 0, "" } },
    { "POCKET", 23, { -1, "Blackmagic Pocket", 3.5, 5.0, 400, 3200, 1250, "stimata", 0, "" } },
    { nullptr,  7,  { -1, "S-Log",           3.0, 4.5, 800, 0,     0,     "stimata",  0, "" } },
    { nullptr,  8,  { -1, "S-Log2",          3.0, 4.5, 800, 0,     0,     "stimata",  0, "" } },
    { nullptr,  9,  { -1, "S-Log3",          3.0, 4.5, 800, 0,     0,     "stimata",  0, "" } },
    { nullptr,  11, { -1, "V-Log",           3.0, 4.5, 640, 4000,  4000,  "stimata",  0, "" } },
    { nullptr,  12, { -1, "Apple Log",       1.5, 3.0, 55,  0,     0,     "derivata (iPhone 15 Pro)", 0, "" } },
    { nullptr,  13, { -1, "D-Log M",         2.0, 3.0, 100, 0,     0,     "stimata",  0, "" } },
    { nullptr,  14, { -1, "D-Log",           2.0, 3.0, 100, 0,     0,     "stimata",  0, "" } },
    { nullptr,  15, { -1, "F-Log",           3.5, 5.0, 640, 0,     0,     "stimata",  0, "" } },
    { nullptr,  16, { -1, "F-Log2",          3.5, 5.0, 1250, 0,    0,     "stimata",  0, "" } },
    { nullptr,  17, { -1, "N-Log",           3.5, 5.0, 800, 0,     0,     "stimata",  0, "" } },
    { nullptr,  18, { -1, "C-Log2",          3.5, 5.0, 800, 0,     0,     "stimata",  0, "" } },
    { nullptr,  19, { -1, "C-Log3",          3.5, 5.0, 800, 0,     0,     "stimata",  0, "" } },
    { nullptr,  20, { -1, "LogC3",           5.3, 6.9, 800, 0,     0,     "derivata (ARRI ALEV-3)", 0, "" } },
    { nullptr,  21, { -1, "LogC4",           6.1, 7.7, 800, 0,     0,     "derivata (ARRI ALEV-4)", 0, "" } },
    { nullptr,  22, { -1, "Log3G10",         3.5, 5.0, 800, 0,     0,     "stimata",  0, "" } },
    { nullptr,  23, { -1, "BMD Film Gen5",   3.5, 5.0, 400, 3200,  1250,  "stimata",  0, "" } },
    { nullptr,  24, { -1, "HLG",             2.0, 3.5, 0,   0,     0,     "stimata",  0, "" } },
};
const NoiseCamera kGeneric = { -1, "camera log", 3.0, 4.5, 0, 0, 0, "stimata", 0, "" };

std::string upper(std::string s)
{
    for (char& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

std::string stops(double v)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", v);
    std::string s = buf;
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

}  // namespace

NoiseCamera noiseCamera(int p_Gamma, const std::string& p_Model, const std::string& p_Codec)
{
    const std::string model = upper(p_Model);
    NoiseCamera c = kGeneric;
    bool byModel = false;
    for (const Row& r : kRows) {
        if (r.gamma != p_Gamma) continue;
        if (r.model && model.find(r.model) != std::string::npos) { c = r.c; byModel = true; break; }
    }
    if (!byModel) {
        for (const Row& r : kRows) {
            if (!r.model && r.gamma == p_Gamma) { c = r.c; break; }
        }
    }
    // FX6 ProRes RAW vs XAVC-I: 0.3-0.7 stop less range at SNR = 2 without the in-camera noise reduction
    if (p_Codec == "raw") { c.codec = -0.5; c.codecName = "RAW"; }
    else if (p_Codec == "long8") { c.codec = -0.5; c.codecName = "8 bit"; }   // stima: blocchi e banding
    else if (p_Codec == "intra") c.codecName = "Intra / ProRes";
    else if (p_Codec == "long") c.codecName = "Long GOP 10 bit";
    c.gamma = p_Gamma;
    return c;
}

void setNoiseMask(DevelopParams& p, const NoiseCamera& c, int p_Mode, bool p_View, double p_Start, double p_Fade,
                  double p_ShotEI, double p_EI, std::string* p_Info)
{
    p.nmOn = 0;
    if (p_Mode != 1 && p_Mode != 2) return;
    const double shot = p_ShotEI > 0.0 ? p_ShotEI : 800.0;
    const double ei = p_EI > 0.0 ? p_EI : shot;
    // the high native gain costs about half a stop of shadows (FX6, A7S III, S5 II), not its distance from the low one
    const bool high = c.high > 0.0 && shot >= c.switchAt;
    const double native = high ? c.high : c.low > 0.0 ? c.low : shot;
    const double start = (p_Mode == 2 ? p_Start : c.start) - (high ? 0.5 : 0.0) + c.codec;
    const double fade = std::max(0.25, p_Mode == 2 ? p_Fade : c.full - c.start);
    // every stop of EI over the native one brings the noise a stop closer to grey (ARRI: 6.9 / 5.9 / 4.9 at 800-3200)
    const double below = std::max(0.0, start - std::log2(ei / native));
    p.nmOn = p_View ? 2 : 1;
    p.nmEdge = (float)-below;
    p.nmInvFade = (float)(1.0 / fade);
    if (p_Info) {
        char iso[64];
        snprintf(iso, sizeof(iso), "nativo %g%s, EI %g", native, high ? " (alto)" : c.low > 0.0 ? "" : " (= ripresa)", ei);
        *p_Info = (below > 0.05 ? "da -" + stops(below) : std::string("dal grigio")) + " a -" + stops(below + fade) + " stop sotto il grigio · " + c.name + ", "
                + iso + (*c.codecName ? std::string(", ") + c.codecName : "") + " · "
                + (p_Mode == 2 ? "personalizzata" : c.source);
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "ColourSpaces.h"

const char* const kSpaceNames[kSpaceCount] = { "DaVinci WG", "Rec.709", "Rec.2020", "P3 D65", "P3 D60", "P3 DCI",
                                               "S-Gamut", "S-Gamut3", "S-Gamut3.Cine", "ACES AP0", "ACES AP1",
                                               "V-Gamut", "Apple Wide Gamut", "D-Gamut", "Cinema Gamut", "ARRI WG3",
                                               "ARRI WG4", "RED WG", "BMD WG Gen5", "DJI D-Log M Gamut" };
const char* const kGammaNames[kGammaNameCount] = { "DaVinci Intermediate", "Linear", "Gamma 2.2", "Gamma 2.4",
                                               "Gamma 2.6", "Rec.709", "sRGB", "SLog", "SLog2", "SLog3", "ACEScct",
                                               "V-Log", "Apple Log", "D-Log M", "D-Log", "F-Log", "F-Log2", "N-Log",
                                               "C-Log2", "C-Log3", "LogC3", "LogC4", "Log3G10", "BMD Film Gen5", "HLG",
                                               "Cineon" };
const char* const kLevelNames[2] = { "Video (64-940)", "Full (0-1023)" };
const int kNodeInputPairs[kCreativeInputCount][2] = { { 0, 0 }, { 0, 0 }, { 8, 9 }, { 7, 9 }, { 6, 8 }, { 10, 10 },
                                                       { 1, 5 }, { 10, 1 }, { 9, 1 }, { 1, kGammaCineon }, { 0, 1 },
                                                       { 2, 1 }, { 11, 11 }, { 12, 12 }, { 19, 13 }, { 13, 14 },
                                                       { 2, 15 }, { 2, 16 }, { 2, 17 }, { 14, 18 }, { 14, 19 },
                                                       { 15, 20 }, { 16, 21 }, { 17, 22 }, { 18, 23 }, { 7, 8 } };
const char* const kNodeInputNames[kCreativeInputCount] = {
    "Automatico", "DaVinci WG/Intermediate", "S-Gamut3.Cine/S-Log3", "S-Gamut3/S-Log3", "S-Gamut/S-Log2",
    "ACES AP1/ACEScct", "Rec.709", "ACES AP1/ACEScg (lineare)", "ACES AP0/ACES2065-1 (lineare)", "Rec.709/Cineon",
    "DaVinci WG (lineare)", "Rec.2020 (lineare)", "V-Gamut/V-Log (Panasonic)", "Apple Wide Gamut/Apple Log (iPhone)",
    "D-Gamut/D-Log M (DJI)", "D-Gamut/D-Log (DJI)", "F-Gamut/F-Log (Fujifilm)", "F-Gamut/F-Log2 (Fujifilm)",
    "N-Gamut/N-Log (Nikon)", "Cinema Gamut/C-Log2 (Canon)", "Cinema Gamut/C-Log3 (Canon)", "ARRI WG3/LogC3",
    "ARRI WG4/LogC4", "REDWideGamutRGB/Log3G10", "Blackmagic WG Gen5/Film Gen5", "S-Gamut3/S-Log2" };

bool mapColourspace(const std::string& cs, int& space, int& gamma)
{
    struct Entry { const char* name; int space; int gamma; };
    static const Entry table[] = {
        { "davinci_intermediate_widegamut", 0, 0 }, { "lin_davinci_widegamut", 0, 1 },
        { "slog3_sgamut3cine", 8, 9 }, { "slog3_sgamut3", 7, 9 },
        { "slog3_venice_sgamut3cine", 8, 9 }, { "slog3_venice_sgamut3", 7, 9 },
        { "lin_sgamut3cine", 8, 1 }, { "lin_sgamut3", 7, 1 },
        { "ACEScct", 10, 10 }, { "ACEScg", 10, 1 }, { "ACES2065-1", 9, 1 },
        { "lin_rec709_srgb", 1, 1 }, { "lin_rec2020", 2, 1 }, { "lin_p3d65", 3, 1 },
        { "g24_rec709_tx", 1, 3 }, { "g22_rec709_tx", 1, 2 }, { "srgb_tx", 1, 6 },
        { "rec1886_rec709_display", 1, 3 }, { "camera_rec709", 1, 5 },
    };
    for (const Entry& e : table) {
        if (cs == e.name) { space = e.space; gamma = e.gamma; return true; }
    }
    return false;
}

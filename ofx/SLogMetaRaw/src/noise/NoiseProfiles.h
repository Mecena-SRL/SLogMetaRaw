// SPDX-License-Identifier: GPL-3.0-or-later
// Noise profile of a recording: camera, curve, ISO and format -> expected noise per brightness and band.
// Sources and confidence of every number: docs/DENOISE.md (built on docs/NOISE.md).
#pragma once
#include <map>
#include <string>

#include "NoiseEngine.h"

// What the clip record (slogmetaraw/plugin_cache.py) says about the recording; zero / empty = unknown.
struct NoiseSource
{
    int gamma = -1;               // transfer code of the input (math/Transfer.h)
    std::string model;            // camera model as the file names it
    std::string codec;            // raw, intra, long, long8 (plugin_cache.noise_codec)
    double iso = 0.0;             // shot ISO or EI
    double bitrate = 0.0;         // Mbps
    int bitDepth = 0;
    std::string chroma;           // 4:2:0, 4:2:2, 4:4:4
    int width = 0, height = 0;
    double fps = 0.0;
};

NoiseSource noiseSourceFromRecord(const std::map<std::string, std::string>& record);

// The recording's class: how the noise reaches the plugin.
enum NoiseClass { kClassRaw, kClassIntra, kClassLongHigh, kClassLongLow, kClassProcessed };

struct NoiseModel
{
    NoiseProfile profile;
    NoiseClass cls = kClassIntra;
    int radius = 2;               // automatic temporal radius
    std::string info;             // what the profile was built from, for the panel
};

// Build the expected profile. frameWidth is the width of the frames the plugin receives (timeline or proxy):
// a source scaled down averages its noise. level multiplies the noise (the panel's Livello rumore).
NoiseModel noiseModel(const NoiseSource& src, int frameWidth, double level);

// A measured profile as text for a hidden parameter, and back. Empty / malformed text -> invalid profile.
std::string noiseProfileToText(const NoiseProfile& p);
NoiseProfile noiseProfileFromText(const std::string& text);

// The sample box's correction of a camera profile: per band, measured noise / expected noise at the box's
// brightness, luma and colour. Applied to every brightness: the camera curve keeps its shape, the sample its level.
struct NoiseSampleFix
{
    float luma[kNoiseLevels] = {}, chroma[kNoiseLevels] = {};
    float level = 0.0f;   // brightness of the box, for the panel
    bool valid = false;
};
NoiseSampleFix noiseSampleFix(const NoiseProfile& expected, const NoiseSample& measured);
void applySampleFix(NoiseProfile& p, const NoiseSampleFix& f);
std::string noiseSampleToText(const NoiseSampleFix& f);
NoiseSampleFix noiseSampleFromText(const std::string& text);

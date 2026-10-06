// SPDX-License-Identifier: GPL-3.0-or-later
// The zone controls shared by both nodes: exposure per zone in stops, like Resolve's HDR palette.
#pragma once
#include <string>

#include "ofxsImageEffect.h"

const int kZoneCount = 4;
struct ZoneDef
{
    const char* name;        // Black, Shadow, Light, Specular
    int up;                  // 1 = acts on tones above its edge, 0 = below
    double range, falloff;   // defaults: edge in stops from 18% grey, transition width in stops
};
extern const ZoneDef kZones[kZoneCount];

// <prefix><Zone>Exp / Sat / Range / Falloff and <prefix>Pivot. withSat: the Develop has Sat, the Detail not.
void defineZoneParams(OFX::ImageEffectDescriptor& d, OFX::PageParamDescriptor* page, OFX::GroupParamDescriptor* group,
                      const std::string& prefix, bool withSat);
std::string zoneParamName(const std::string& prefix, int zone, const char* field);

struct ZoneValues
{
    double pivot = 0.0;
    double exp[kZoneCount] = {}, sat[kZoneCount] = {}, range[kZoneCount] = {}, falloff[kZoneCount] = {};
};
// The zone parameters, looked up once at construction: no name lookups while rendering.
struct ZoneParamSet
{
    OFX::DoubleParam* pivot = nullptr;
    OFX::DoubleParam *exp[kZoneCount] = {}, *sat[kZoneCount] = {}, *range[kZoneCount] = {}, *falloff[kZoneCount] = {};
    void bind(OFX::ImageEffect& effect, const std::string& prefix);
    ZoneValues read(double time) const;   // missing parameters keep their defaults
};
// Back to the defaults, keys removed ("Azzera zone").
void resetZoneParams(OFX::ImageEffect& effect, const std::string& prefix);

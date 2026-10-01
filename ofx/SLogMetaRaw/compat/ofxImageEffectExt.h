// SPDX-License-Identifier: GPL-3.0-or-later
// Stand-in for the header of the DaVinci Resolve SDK, so the plugin builds against the upstream OpenFX
// SDK on Linux and Windows. Only the pieces S-Log MetaRaw uses; the names match the host's.
#pragma once
#include "ofxImageEffect.h"

#ifndef kOfxImageEffectPropSrcFilePath
#define kOfxImageEffectPropSrcFilePath "OfxImageEffectPropSrcFilePath"
#endif
#ifndef kOfxImageEffectPropNoSpatialAwareness
#define kOfxImageEffectPropNoSpatialAwareness "OfxImageEffectPropNoSpatialAwareness"
#endif

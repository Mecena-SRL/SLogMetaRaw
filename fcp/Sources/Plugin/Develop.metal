// SPDX-License-Identifier: GPL-3.0-or-later
// Log MetaRaw Develop for Final Cut Pro. The colour maths is the one of the DaVinci Resolve node
// (ofx/SLogMetaRaw/gen/DevelopMath.h, generated from math/*.h): this file only moves pixels in and out.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "../../../ofx/SLogMetaRaw/gen/DevelopMath.h"
#include "DevelopShaderTypes.h"

typedef struct {
    float4 clipSpacePosition [[position]];
    float2 textureCoordinate;
} RasterizerData;

vertex RasterizerData
developVertex(uint vertexID [[vertex_id]],
              constant LMRVertex2D* vertexArray [[buffer(LMRVertices)]],
              constant vector_uint2* viewportSizePointer [[buffer(LMRViewportSize)]])
{
    RasterizerData out;
    const float2 pixelSpacePosition = vertexArray[vertexID].position.xy;
    const float2 viewportSize = float2(*viewportSizePointer);
    out.clipSpacePosition.xy = pixelSpacePosition / (viewportSize / 2.0);
    out.clipSpacePosition.z = 0.0;
    out.clipSpacePosition.w = 1.0;
    out.textureCoordinate = vertexArray[vertexID].textureCoordinate;
    return out;
}

// Final Cut hands an effect the clip's code values raised to 1.961 (Apple's Rec.709 curve, measured on grey ramps):
// undo it so the maths sees the code values the Resolve node sees, and redo it on the way out so Final Cut
// shows and exports the code values the node writes. Sign kept: wide-gamut conversions can go below zero.
static inline float lmr_pow(float v, float e) { return copysign(precise::pow(fabs(v), e), v); }

fragment float4
developFragment(RasterizerData in [[stage_in]],
                texture2d<float> source [[texture(LMRSourceImage)]],
                constant DevelopParams& params [[buffer(LMRDevelopParamsIndex)]],
                constant float& hostGamma [[buffer(LMRHostGammaIndex)]])
{
    constexpr sampler s(mag_filter::nearest, min_filter::nearest, address::clamp_to_edge);
    const float4 c = source.sample(s, in.textureCoordinate);
    const float g = hostGamma;
    const SMf3 code = g > 0.0f ? smf3(lmr_pow(c.r, 1.0f / g), lmr_pow(c.g, 1.0f / g), lmr_pow(c.b, 1.0f / g))
                               : smf3(c.r, c.g, c.b);
    const SMf3 o = sm_develop(code, params);
    return g > 0.0f ? float4(lmr_pow(o.x, g), lmr_pow(o.y, g), lmr_pow(o.z, g), c.a) : float4(o.x, o.y, o.z, c.a);
}

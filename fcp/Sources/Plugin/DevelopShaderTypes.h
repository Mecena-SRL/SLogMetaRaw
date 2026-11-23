// SPDX-License-Identifier: GPL-3.0-or-later
// Indices shared by DevelopEffect.swift and Develop.metal.
#ifndef DevelopShaderTypes_h
#define DevelopShaderTypes_h

#import <simd/simd.h>

typedef enum LMRVertexInput {
    LMRVertices     = 0,
    LMRViewportSize = 1
} LMRVertexInput;

typedef enum LMRTextureInput {
    LMRSourceImage = 0
} LMRTextureInput;

typedef enum LMRFragmentInput {
    LMRDevelopParamsIndex = 0,
    LMRHostGammaIndex = 1     // float: Final Cut's code value -> pixel exponent (1.961), 0 = pixels are code values
} LMRFragmentInput;

typedef struct LMRVertex2D {
    vector_float2 position;
    vector_float2 textureCoordinate;
} LMRVertex2D;

#endif /* DevelopShaderTypes_h */

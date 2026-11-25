// SPDX-License-Identifier: GPL-3.0-or-later
// Kernels run on the host's queue and are never waited for (ofxGPURender.h). The pixel pointers
// Resolve hands over are MTLBuffer handles. A false return means nothing was encoded.
#pragma once
#include <cstddef>

#include "../gen/DevelopMath.h"
#include "../src/noise/NoiseEngine.h"

// The Creative node: p_Inputs[k] is the frame of record k (MTLBuffer handles); p_Params.rowPixels and dstRowPixels
// are the strides of the input and output buffers.
// p_PrevParams, p_PrevInputs: the previous frame for an LCD's slow response (CreativeParams::mGhost), or null.
// p_History: the frames 1, 2, 3 before this one for a pickup tube (the first p_Params.history of them), or null.
// p_Transfer: the second pass of a copy (creativePrepareTransfer), which then carries the viewing medium, or null.
bool RunCreativeKernels(void* p_CmdQ, const CreativeParams& p_Params, const float* const p_Inputs[3], float* p_Output,
                        const CreativeParams* p_PrevParams = nullptr, const float* const* p_PrevInputs = nullptr,
                        const float* const* p_History = nullptr, const CreativeParams* p_Transfer = nullptr);
bool RunDevelopKernel(void* p_CmdQ, int p_Width, int p_Height, int p_RowPixels, const DevelopParams& p_Params,
                      const float* p_Input, float* p_Output);
// Pass-through: the first p_Bytes of the input buffer into the output buffer.
bool RunCopy(void* p_CmdQ, size_t p_Bytes, const float* p_Input, float* p_Output);
// Frees the Creative working planes kept between frames (the last Creative node is gone).
void DropCreativeScratch();
// The Noise node: frames as noiseRenderCPU takes them (px are MTLBuffer handles, rowFloats = 4 x row pixels).
bool RunNoiseKernels(void* p_CmdQ, int p_Width, int p_Height, const NoiseFrame* p_Frames, int p_Count, int p_Center,
                     const NoiseProfile& p_Profile, const NoiseControls& p_Controls, float* p_Output,
                     int p_DstRowPixels);
// Frees the Noise working planes kept between frames (the last Noise node is gone).
void DropNoiseScratch();

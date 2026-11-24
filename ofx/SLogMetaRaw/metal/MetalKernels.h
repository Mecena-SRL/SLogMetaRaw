// SPDX-License-Identifier: GPL-3.0-or-later
// Kernels run on the host's queue and are never waited for (ofxGPURender.h). The pixel pointers
// Resolve hands over are MTLBuffer handles. A false return means nothing was encoded.
#pragma once
#include <cstddef>

#include "../gen/DevelopMath.h"
#include "../src/noise/NoiseEngine.h"

bool RunDetailKernels(void* p_CmdQ, const DetailParams& p_Params, int p_RowPixels, const float* p_Input,
                      float* p_Output);
bool RunDevelopKernel(void* p_CmdQ, int p_Width, int p_Height, int p_RowPixels, const DevelopParams& p_Params,
                      const float* p_Input, float* p_Output);
// Pass-through: the first p_Bytes of the input buffer into the output buffer.
bool RunCopy(void* p_CmdQ, size_t p_Bytes, const float* p_Input, float* p_Output);
// Frees the Detail working planes kept between frames (the last Detail node is gone).
void DropDetailScratch();
// The Noise node: frames as noiseRenderCPU takes them (px are MTLBuffer handles, rowFloats = 4 x row pixels).
bool RunNoiseKernels(void* p_CmdQ, int p_Width, int p_Height, const NoiseFrame* p_Frames, int p_Count, int p_Center,
                     const NoiseProfile& p_Profile, const NoiseControls& p_Controls, float* p_Output,
                     int p_DstRowPixels);
// Frees the Noise working planes kept between frames (the last Noise node is gone).
void DropNoiseScratch();

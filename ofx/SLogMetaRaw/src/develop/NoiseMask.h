// SPDX-License-Identifier: GPL-3.0-or-later
// Maschera rumore: where the selected EI leaves the shadows noisy, for each camera and recording.
// Sources and confidence of every number: docs/NOISE.md.
#pragma once
#include <string>

#include "../../gen/DevelopMath.h"

struct NoiseCamera
{
    int gamma;             // the curve it was chosen for
    const char* name;      // "Sony FX6", or the curve when the model is not in the table
    double start, full;    // stops below grey where the noise begins and is full, at the native ISO, intra codec
    double low, high;      // native ISOs; low 0 = unknown (the shot ISO stands in), high 0 = a single one
    double switchAt;       // shot ISO from which the camera reads with the high native gain
    const char* source;    // misurata, derivata, stimata
    double codec;          // stops the recording moves the start: < 0 = noise shows earlier
    const char* codecName;
};

// The row for the clip: the model first, then the curve, then a generic log camera.
NoiseCamera noiseCamera(int p_Gamma, const std::string& p_Model, const std::string& p_Codec);

// mode 0 off, 1 automatic, 2 custom (p_Start and p_Fade typed, the native ISOs and the EI still apply).
// Fills nmOn, nmEdge, nmInvFade; p_Info says where the mask starts and why.
void setNoiseMask(DevelopParams& p, const NoiseCamera& c, int p_Mode, bool p_View, double p_Start, double p_Fade,
                  double p_ShotEI, double p_EI, std::string* p_Info);

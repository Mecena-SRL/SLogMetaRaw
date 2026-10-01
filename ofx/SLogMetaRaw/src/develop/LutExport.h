// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

#include "../../gen/DevelopMath.h"

// A .cube 3D LUT of the node: the same sm_develop the render runs, sampled on a size^3 grid over the
// signal entering the node (its own encoding, 0-1). The output is not clamped: a linear output can
// leave 0-1.
const int kLutSizes[] = { 17, 33, 65 };
const int kLutSizeCount = 3;
const int kLutDefaultSize = 1;   // index in kLutSizes: 33

// Writes the file; false (and a reason) on a bad size or an unwritable path.
bool writeCubeLut(const DevelopParams& p, int size, const std::string& title, const std::string& path,
                  std::string& error);

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

// Sizes of the gamut and transfer tables of DevelopMath.h: codes read from files are checked against them.
// kGammaCount: the curves of the Develop node's menus. Cineon (code 25) comes after them: only an input of the
// Creative node, never a camera or an output.
const int kSpaceCount = 20, kGammaCount = 25, kGammaCineon = 25, kGammaNameCount = 26;
extern const char* const kSpaceNames[kSpaceCount];
extern const char* const kGammaNames[kGammaNameCount];
extern const char* const kLevelNames[2];          // index = data level code, shared with datalevel.py
// A saved code outside its table (e.g. from a newer build) reads as unknown.
inline int validCode(int v, int count) { return v >= 0 && v < count ? v : -1; }

// "Ingresso nodo" choice: 0 = automatic, then fixed (gamut, transfer) pairs. Noise and Develop list the first
// kNodeInputCount; the Creative node, which may sit after a grade, lists them all (indices are permanent: add at the
// end). Rec.709 reads through the BT.709 curve, so a graded picture's grey (code 0.41) is the film's 18%.
const int kNodeInputCount = 6, kCreativeInputCount = 26;
extern const int kNodeInputPairs[kCreativeInputCount][2];
extern const char* const kNodeInputNames[kCreativeInputCount];

// OFX native colourspace name -> codes of DevelopMath.h. False when the name is unknown.
bool mapColourspace(const std::string& cs, int& space, int& gamma);

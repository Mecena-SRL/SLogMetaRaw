// SPDX-License-Identifier: GPL-3.0-or-later
// The Creative node's processes as data: what each control means for the engine (math/Creative.h). Free of OpenFX,
// so the tests use it directly. Every number is a parametric estimate from the research in docs/CREATIVE.md (marked
// there as documented or estimated); none comes from a product's profile.
#pragma once
#include "../../gen/DevelopMath.h"

// "Processo": the order is permanent (Resolve saves the index); append only.
enum CreativeProcess {
    kCrColourNegative, kCrReversal, kCrBlackWhite, kCrTechni3, kCrTechni2, kCrAutochrome, kCrDufay, kCrThreePlates,
    kCrKinemacolor, kCrDaguerreotype, kCrCollodion, kCrAlbumen, kCrCyanotype, kCrPlatinum, kCrAnalog, kCrDigital,
    kCrProcessCount
};
extern const char* const kCrProcessNames[kCrProcessCount];
extern const char* const kCrProcessInfo[kCrProcessCount];

// The variants of each process (one choice parameter each, shown with its process). Append only.
const int kCrNegativeCount = 14, kCrReversalCount = 3, kCrBWStockCount = 3, kCrSensitivityCount = 4, kCrFilterCount = 5,
          kCrTechniEraCount = 3, kCrCollodionCount = 2, kCrPaperCount = 2, kCrPrintCount = 5, kCrFormatCount = 12,
          kCrAnalogCount = 11, kCrDigitalCount = 10;
extern const char* const kCrNegativeNames[kCrNegativeCount];
extern const char* const kCrReversalNames[kCrReversalCount];
extern const char* const kCrBWStockNames[kCrBWStockCount];
extern const char* const kCrSensitivityNames[kCrSensitivityCount];
extern const char* const kCrFilterNames[kCrFilterCount];
extern const char* const kCrTechniEraNames[kCrTechniEraCount];
extern const char* const kCrCollodionNames[kCrCollodionCount];
extern const char* const kCrPaperNames[kCrPaperCount];
extern const char* const kCrPrintNames[kCrPrintCount];
extern const char* const kCrFormatNames[kCrFormatCount];
extern const char* const kCrAnalogNames[kCrAnalogCount];
extern const char* const kCrDigitalNames[kCrDigitalCount];

// What the panel holds, in the panel's units. Defaults: a node that does nothing surprising.
struct CreativeControls
{
    int process = kCrColourNegative, output = 0;
    int negative = 0, reversal = 0, bwStock = 0, sensitivity = 3, filter = 0, techniEra = 0, collodion = 0, paper = 1,
        print = 1, format = 2, analog = 1, digital = 0;
    // shot and development
    double exposure = 0, temperature = 0, tint = 0, push = 0, silver = 0, flash = 0, age = 0;
    // print
    double printR = 0, printG = 0, printB = 0, printContrast = 1;
    // texture and optics (amounts are multiples of the process's own, 1 = as it was)
    double grain = 1, grainSize = 1, halation = 1, bloom = 0, bloomRadius = 2, aberration = 0, vignette = 0,
           weave = 0, breath = 0, dust = 0, scratches = 0;
    // separations, screens, plates
    double registration = 0, interval = 0, keyImage = -1, dyeDensity = 1;   // keyImage -1: the era's own
    double mosaicScale = 1, clump = 0.5, viewLight = 0.85;
    double viewAngle = 0.9, uv = 0.5, oxidation = 0.3, border = 0.4, toning = 0.5;
    // analog video and digital cameras (multiples of the format's own, 1 = typical)
    double wear = 0.15, heads = 0.3, instability = 0.25, noise = 1, colourBleed = 1, sharpness = 0;
    double range = 0, highlights = 0.5, compression = 1;
    // colour
    double shadowHue = 200, shadowAmount = 0, highHue = 40, highAmount = 0, balance = 0, saturation = 1, density = 0;
};

// Time offsets (frames) of the three records' exposures: sequential plates and two-colour cameras read other frames.
void creativeRecordTimes(const CreativeControls& c, double out[3]);

// The engine's parameters for a W x H frame (the whole frame: no tiles), frame number `frame`, input encoding
// (space, gamma) and source pixel aspect.
void creativePrepare(CreativeParams& p, const CreativeControls& c, int W, int H, int frame, int space, int gamma);

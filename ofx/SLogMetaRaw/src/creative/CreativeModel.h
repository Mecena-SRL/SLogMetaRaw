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
const int kCrNegativeCount = 20, kCrReversalCount = 6, kCrBWStockCount = 6, kCrSensitivityCount = 4, kCrFilterCount = 5,
          kCrTechniEraCount = 3, kCrCollodionCount = 2, kCrPaperCount = 2, kCrPrintCount = 9, kCrFormatCount = 12,
          kCrAnalogCount = 11, kCrDigitalCount = 10, kCrGenerationCount = 4, kCrMediumCount = 8;
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
extern const char* const kCrGenerationNames[kCrGenerationCount];
extern const char* const kCrMediumNames[kCrMediumCount];
extern const char* const kCrMediumInfo[kCrMediumCount];

// What the panel holds, in the panel's units. Defaults: a node that does nothing surprising.
struct CreativeControls
{
    int process = kCrColourNegative, output = 0;
    int negative = 0, reversal = 0, bwStock = 0, sensitivity = 3, filter = 0, techniEra = 0, collodion = 0, paper = 1,
        print = 1, format = 2, analog = 1, digital = 0, generation = 0, medium = 0;
    // shot and development
    double exposure = 0, temperature = 0, tint = 0, push = 0, silver = 0, flash = 0, age = 0;
    // print
    double printR = 0, printG = 0, printB = 0, printContrast = 1, printGrain = 1;
    // viewing medium: how much of it (0 = the plain picture, 1 = as it is), then the device's response in physical
    // units, loaded from the medium's preset when it is chosen (creativeLoadMedium). mediumLoaded: the medium whose
    // values the panel holds; any other (a grade saved before 4.1.1, a keyframed medium) renders its preset.
    double mediumAmount = 1;
    int mediumRoom = 1, mediumLoaded = -1;   // room: 0 dark, 1 dim, 2 lit
    double mediumGamma = 2.4, mediumContrast = 2000, mediumAmbient = 0, mediumNits = 100, mediumKelvin = 6500,
           mediumColour = 100, mediumOverscan = 0;
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

extern const char* const kCrRoomNames[3];
// The preset of medium m written into c's response fields (gamma, contrast, ambient light, peak, white, colour,
// overscan, room), and c.mediumLoaded = m.
void creativeLoadMedium(CreativeControls& c, int m);
// Which response rows mean something for medium m: tone (gamma, contrast, light, peak, room), white, overscan.
struct CrMediumRows { bool tone, white, overscan; };
CrMediumRows creativeMediumRows(int m);

// Whether the picture of the previous frame is needed too (an LCD's slow response).
bool creativeNeedsPrevious(const CreativeControls& c);

// Time offsets (frames) of the three records' exposures: sequential plates and two-colour cameras read other frames.
void creativeRecordTimes(const CreativeControls& c, double out[3]);

// The engine's parameters for a W x H frame (the whole frame: no tiles), frame number `frame`, input encoding
// (space, gamma) and source pixel aspect.
void creativePrepare(CreativeParams& p, const CreativeControls& c, int W, int H, int frame, int space, int gamma);

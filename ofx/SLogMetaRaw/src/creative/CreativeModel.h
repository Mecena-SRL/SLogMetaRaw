// SPDX-License-Identifier: GPL-3.0-or-later
// The Creative node's processes as data: what each control means for the engine (math/Creative.h). Free of OpenFX,
// so the tests use it directly. Every number is a parametric estimate from the research in docs/CREATIVE.md (marked
// there as documented or estimated); none comes from a product's profile.
#pragma once
#include "../../gen/DevelopMath.h"

#include <string>

#include "CreativePresets.h"

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
          kCrTechniEraCount = 3, kCrCollodionCount = 2, kCrPaperCount = 2, kCrPrintCount = 9, kCrFormatCount = 15,
          kCrAnalogCount = 11, kCrDigitalCount = 10, kCrGenerationCount = 4, kCrMediumCount = 12,
          kCrSensorCount = 10, kCrTransferCount = 6;
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
// The video camera's sensor (analog video): pickup tubes, CCD, MOS. Append only.
extern const char* const kCrSensorNames[kCrSensorCount];
// "Riversato su": what the finished picture was copied onto before it was seen (telecine, tape dub, kinescope,
// film-out). Append only.
extern const char* const kCrTransferNames[kCrTransferCount];

// "Dispositivo": what took the picture. Manual = the process chosen by hand (every process); the others choose a
// model, and film a manufacturer and stock, which set the process and its variant. Append only.
enum CreativeDevice { kCrDevManual, kCrDevFilm, kCrDevVideo, kCrDevCinema, kCrDevPhone, kCrDeviceCount };
extern const char* const kCrDeviceNames[kCrDeviceCount];

// What the panel holds, in the panel's units. Defaults: a node that does nothing surprising.
struct CreativeControls
{
    int process = kCrColourNegative, output = 0;
    // device and its preset: model per family, manufacturer, stock per manufacturer. deviceLoaded: the model whose
    // values the texture rows (or the tape rows) hold, as creativeDeviceKey; any other renders the model's preset.
    int device = kCrDevManual, filmCamera = 17, videoCamera = 5, cinemaCamera = 0, phoneCamera = 0, maker = 0;
    int stock[kCrMakerCount] = {};
    int deviceLoaded = -1;
    // what the film applies (0 off): its curve and contrast, its colour, its grain and texture. Tone off = the
    // picture's own light, back in the input encoding: with colour off too, only the film's grain on the video.
    int useTone = 1, useColour = 1, useTexture = 1;
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
    // unsteadiness, each from its own machine (0 off): the camera's (weave, above), each printing step's, the
    // projector's (multiple of its own, 1 = typical)
    int camWeave = 1, projWeave = 1;
    // a hair or a crumb of emulsion caught in the camera's gate: the share of shots that have one (0 none)
    double hair = 0;
    double printWeave = 1, projectorWeave = 1;
    // separations, screens, plates
    double registration = 0, interval = 0, keyImage = -1, dyeDensity = 1;   // keyImage -1: the era's own
    double mosaicScale = 1, clump = 0.5, viewLight = 0.85;
    double viewAngle = 0.9, uv = 0.5, oxidation = 0.3, border = 0.4, toning = 0.5;
    // analog video and digital cameras (multiples of the format's own, 1 = typical)
    double wear = 0.15, heads = 0.3, instability = 0.25, noise = 1, colourBleed = 1, sharpness = 0;
    double range = 0, highlights = 0.5, compression = 1;
    // the video camera's sensor (0 none: the signal as it is), its tube's lag and comet tails, its CCD's smear
    // (multiples of the sensor's own, 1 = typical)
    int sensor = 0;
    double lag = 1, comet = 1, smear = 1;
    // the copy the picture went through before it was seen (0 none)
    int transfer = 0;
    // the lens (0: the camera's own) and what it loads: distortion (%), edge softness, swirl, veiling glare (%),
    // anamorphic streak; lensLoaded: the lens whose values the rows hold
    int lens = 0, lensLoaded = -1;
    double distortion = 0, edgeSoft = 0, swirl = 0, glare = 0, streak = 0;
    // colour
    double shadowHue = 200, shadowAmount = 0, highHue = 40, highAmount = 0, balance = 0, saturation = 1, density = 0;
};

extern const char* const kCrRoomNames[3];
// The preset of medium m written into c's response fields (gamma, contrast, ambient light, peak, white, colour,
// overscan, room), and c.mediumLoaded = m.
void creativeLoadMedium(CreativeControls& c, int m);
// Which response rows mean something for medium m: tone (gamma, contrast, light, peak, room), white, overscan, and
// whether it moves the film (a projector).
struct CrMediumRows { bool tone, white, overscan, weave; };
CrMediumRows creativeMediumRows(int m);

// The device's model as one number (0 for manual), and its preset written into c's rows: format, weave, flicker and
// lens for a film camera, tape and its faults for a video camera; c.deviceLoaded = the key.
int creativeDeviceKey(const CreativeControls& c);
void creativeLoadDevice(CreativeControls& c);
// The controls as the engine reads them: the device's process and variant (stock, camera row), the model's preset
// when the rows do not hold it. Manual: c as it is.
CreativeControls creativeResolve(const CreativeControls& c);
// Film unsteadiness in pixels at frame: out[0..1] the camera's (x, y), [2..3] the printer's (the picture moves on the
// film), [4..5] the projector's (the whole film moves on the screen, at full medium intensity).
void creativeWeave(const CreativeControls& c, int W, int H, int frame, double out[6]);
// The chosen stock (nullptr unless a film device) and whether it was made in the chosen format.
const CrStock* creativeStock(const CreativeControls& c);
bool creativeStockFits(const CreativeControls& c);
// What the panel says about the device: the model, its formats, the stock (year, kind, gauges) and any mismatch.
std::string creativeDeviceInfo(const CreativeControls& c);
// Whether the output is the input encoding (Log output, or the film's tone off): no print, no viewing medium.
bool creativeLogOut(const CreativeControls& resolved);
// Whether the toggles apply (film processes: colour negative, reversal, black and white).
bool creativeToggles(int process);

// Whether the picture of the previous frame is needed too (an LCD's slow response).
bool creativeNeedsPrevious(const CreativeControls& c);
// How many frames before this one the sensor reads (a pickup tube's charge carries over): 0 to 3.
int creativeHistory(const CreativeControls& c);
// Whether sensor s is a pickup tube, a CCD.
bool creativeSensorTube(int s);
bool creativeSensorCCD(int s);

// Time offsets (frames) of the three records' exposures: sequential plates and two-colour cameras read other frames.
void creativeRecordTimes(const CreativeControls& c, double out[3]);

// The engine's parameters for a W x H frame (the whole frame: no tiles), frame number `frame`, input encoding
// (space, gamma) and source pixel aspect.
// The second pass of a transfer (c.transfer): the parameters that take the finished picture (Rec.709 Gamma 2.4) through
// the copy, the viewing medium included; false when there is none (or the output is the input encoding).
// The lens l's preset written into c's rows (distortion, softness, swirl, glare, streak, vignetting, aberration,
// diffusion), and c.lensLoaded = l.
void creativeLoadLens(CreativeControls& c, int l);
bool creativePrepareTransfer(CreativeParams& p, const CreativeControls& c, int W, int H, int frame);
// history: how many of the frames before this one the host passes (creativeHistory or fewer, at the clip's start).
void creativePrepare(CreativeParams& p, const CreativeControls& c, int W, int H, int frame, int space, int gamma,
                     int history = 0);

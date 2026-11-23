// SPDX-License-Identifier: GPL-3.0-or-later
// Bridge between the FxPlug panel (Swift) and the per-pixel maths of the OpenFX plugin (C++).
// Plain Objective-C so the Swift bridging header can import it; the C++ stays in DevelopHost.mm.
#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C" {
#endif

// The panel as the user sees it. Codes follow ofx/SLogMetaRaw/src/common/ColourSpaces.cpp:
// space = index in kSpaceNames, gamma = index in kGammaNames (no "Automatico" entry here).
typedef struct LMRDevelopControls {
    double shotEI, shotTemp, shotTint;     // as shot: the zero of EI and White Balance
    double ei, temp, tint;                 // chosen
    int inSpace, inGamma;                  // what enters the effect
    int outSpace, outGamma;                // output: a code, or -1 = the project's encoding (projSpace / projGamma)
    int projSpace, projGamma;              // the Final Cut project's working space and curve
    int rendering;                         // 0 none, 1 ACES 2.0, 2 Luminance, 3 Filmic, 4 ACES 1.3, 5 OpenDRT
    int gamutCompress;                     // 0 none, 1 ACES reference
    int gamutMapping;                      // 0 none, 1 saturation compression
    double highlightVibrance;              // 0..100, only with a rendering
    double contrast, highlights, shadows, whites, blacks, vibrance, saturation;   // -100..100
    int fcExposure, fcTemperature, fcTint;  // false colour views, as in the Resolve node
    int softClip;                           // Soft Clip
    double softClipLevel;                   // Bianco (stop), 2.5 by default
} LMRDevelopControls;

// DevelopParams as bytes, the render-time input of the Metal kernel (same layout in C++ and Metal).
NSData* LMRBuildDevelopParams(const LMRDevelopControls* controls);

// Size in bytes of that data, for the check at render time.
size_t LMRDevelopParamsSize(void);

#ifdef __cplusplus
}
#endif

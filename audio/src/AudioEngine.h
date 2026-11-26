// SPDX-License-Identifier: GPL-3.0-or-later
// Log MetaRaw Audio: the period recording and mix chain, host-independent (the VST3/AU wrappers only move samples
// and parameters). One engine serves both plugins: «Ripresa» runs microphone → preamp → medium, «Mix e supporto»
// runs desk → medium → transfers → listening; a stage that is off or set to «Nessuno» passes samples untouched.
#pragma once
#include "AudioPresets.h"

#include <cstdint>
#include <vector>

// Stages with an on/off switch.
enum AuStage { kStMic, kStPre, kStDesk, kStMedium, kStWow, kStNoise, kStNR, kStTransfer, kStListen, kStages };

struct AuSettings
{
    int family = 0, model = 0;   // the recorder (Ripresa); its medium, microphone and NR are the defaults below
    int mic = 0;                 // kAuMics, 0 = the device's own
    int medium = -1;             // kAuMedia, -1 = the device's
    int nr = -1;                 // kAuNR, -1 = the device's
    int desk = 0;                // kAuConsoles
    int transfer = 0, copies = 1;  // re-recorded copies onto kAuMedia[transfer] (0 = none)
    int listen = 0;              // kAuListen
    bool useDevice = true;       // false: the chain starts at the desk (Mix e supporto)
    bool on[kStages] = { true, true, true, true, true, true, true, true, true };
    float drive = 1, noiseDb = 0, wow = 1;  // trims: saturation ×, noise offset (dB), wow and flutter ×
    float mix = 1, outDb = 0;    // dry/wet and output level
    // The machine's own controls (the front panel): a low cut (second-order high-pass, Hz, below 10 = off), the
    // limiter or automatic level control, and the tone (shelves at 100 Hz and 10 kHz, dB): the desk's equaliser, or a
    // recorder's playback tone.
    float lowCutHz = 0, lowDb = 0, highDb = 0;
    bool limit = false, alc = false;   // a peak limiter; automatic level control (slow, rides the level)
    uint32_t seed = 1;
};

// A resolved, sample-rate-ready chain; reset() clears all state (start of playback, a jump in the timeline).
class AuEngine
{
public:
    // Builds the chain; called again with new settings it keeps the running state of every block whose shape is
    // unchanged (no click when a knob moves). Allocates: call it outside the audio thread or between blocks.
    void prepare(double sampleRate, int channels, const AuSettings& s);
    void reset();
    // In place, planar float, any block size.
    void process(float* const* ch, int frames);

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float run(float x, float& z1, float& z2) const
        {
            float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

private:
    // One stage of the chain: filters and nonlinearity, plus a medium's moving parts when it is one.
    struct Block
    {
        std::vector<Biquad> eq;                  // run in order
        float drive = 0, asym = 0, comp = 0;     // saturation, even harmonics, level-riding (0..1)
        bool mono = false;
        float wowA = 0, wowW = 0, flutA = 0, flutW = 0;           // delay swing (s) and rate (rad/s)
        float hiss = 0, hum = 0, rumble = 0, crackle = 0, dropout = 0;  // linear levels; events per sample
        Biquad hissLp;
        // state, per channel where it is a vector
        std::vector<float> z, env, line, tail, hissZ;
        uint64_t pos = 0;   // samples since reset: the medium's clock, shared by its channels
        float drop = 1, dropTo = 1, rumbleZ = 0;
        int dropLeft = 0;
        uint32_t rng = 1;
        float random();   // uniform -1..1, its own sequence: the output does not depend on the host's block size
    };

    void add(Block b);
    void fresh(Block& b, size_t index) const;
    void run(Block& b, float* const* ch, int frames);

    double fs_ = 48000;
    int channels_ = 0;
    uint32_t seed_ = 1;
    float wet_ = 1, out_ = 1;
    std::vector<Block> blocks_;
    std::vector<float> dry_;
};

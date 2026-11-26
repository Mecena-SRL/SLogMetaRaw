// SPDX-License-Identifier: GPL-3.0-or-later
// How each level meter moves, sample by sample in the processor (the panel only draws the reading). Readings are in
// dB against a steady sine of RMS 1: every meter reads such a sine at its RMS, so 0 VU and PPM 4 fall on the same tone,
// as in a studio aligned to one reference. Where the numbers come from:
//   VU          ANSI C16.5 / IEC 60268-17: a sine step reaches 99 % of its reading in 300 ms and overshoots by
//               1 to 1.5 %; full-wave average. A second-order movement, ζ = 0.80 (1.5 % overshoot) and ωn = 13.13 rad/s
//               (99 % at 0.300 s), integrated on the rectified signal.                          [standard]
//   PPM         IEC 60268-10 type IIa (BBC): a 10 ms burst of 5 kHz reads 2 dB below its steady value; the needle
//               falls 24 dB in 2.8 s. Full-wave peak rectifier charging with τ = 2.76 ms (the value that gives the
//               -2 dB), linear fall in dB.                                                     [standard]
//   Modulometer Nagra peak meter. The manuals give the scale, not the times: modelled like IEC type I (10 ms burst
//               -1 dB: τ = 1.69 ms; fall 20 dB in 1.5 s), the quasi-peak class of the period's European meters.
//                                                                                              [deduced]
// The peak meters are calibrated as real ones were, on a steady tone: prepare() measures a 1 kHz sine and scales.
//   Indicator   small consumer needle, average, about 100 ms, no overshoot.                   [deduced]
//   Eye         indicator tube driven through an RC: "an averaged value, similar to a VU's damping" (Sams, Tape
//               recorders, how they work): 20 ms charge, 250 ms discharge.                    [source + deduced]
//   Neon        "will not light until its applied voltage reaches a certain point" (Sams): peak, fast, a short glow.
//   Led         peak ladder: 1 ms rise, falls 20 dB in 1.5 s; the panel holds the peak 0.8 s. [deduced]
//   Automatic   the lamp of automatic level control: like the eye.                            [deduced]
#pragma once
#include "AuLooks.h"

#include <algorithm>
#include <cmath>

class AuBallistics
{
public:
    void prepare(double fs, AuMeter meter)
    {
        meter_ = meter;
        dt_ = 1 / fs;
        auto charge = [&](double tau) { return 1 - std::exp(-dt_ / tau); };
        auto fall = [&](double db, double seconds) { return std::pow(10.0, -db / seconds / 20 * dt_); };
        switch (meter) {
        case AuMeter::PPM: att_ = charge(0.00276), rel_ = fall(24, 2.8); break;
        case AuMeter::Modulometer: att_ = charge(0.00169), rel_ = fall(20, 1.5); break;
        case AuMeter::Led: att_ = charge(0.001), rel_ = fall(20, 1.5); break;
        case AuMeter::Neon: att_ = charge(0.001), rel_ = fall(20, 0.2); break;
        case AuMeter::Indicator: att_ = rel_ = charge(0.1); break;
        case AuMeter::Eye:
        case AuMeter::Automatic: att_ = charge(0.02), rel_ = charge(0.25); break;
        default: break;
        }
        cal_ = 1;
        if (meter == AuMeter::PPM || meter == AuMeter::Modulometer || meter == AuMeter::Led || meter == AuMeter::Neon) {
            reset();   // a steady sine of RMS 1 must read 0 dB
            float x[480];
            for (int block = 0; block < 50; ++block) {
                for (int i = 0; i < 480; ++i) x[i] = float(std::sqrt(2.0) * std::sin(2 * 3.141592653589793 * 1000 * (block * 480 + i) * dt_));
                run(x, 480);
            }
            cal_ = 1 / std::max(y_, 1e-3);
        }
        reset();
    }
    void reset() { y_ = v_ = 0; }

    // One block of one channel.
    void run(const float* x, int n)
    {
        constexpr double kAvg = 1.1107207345;   // π / (2√2): the average of |sine| read as its RMS
        constexpr double kPeak = 0.7071067812;  // 1 / √2: the peak of a sine read as its RMS
        constexpr double kZeta = 0.8007, kWn = 13.126;
        switch (meter_) {
        case AuMeter::VU:   // the movement: y'' = ωn² (u - y) - 2 ζ ωn y', semi-implicit Euler
            for (int i = 0; i < n; ++i) {
                const double u = std::fabs(x[i]) * kAvg;
                v_ += (kWn * kWn * (u - y_) - 2 * kZeta * kWn * v_) * dt_;
                y_ += v_ * dt_;
            }
            break;
        case AuMeter::PPM:
        case AuMeter::Modulometer:
        case AuMeter::Led:
        case AuMeter::Neon:
            for (int i = 0; i < n; ++i) {
                const double e = std::fabs(x[i]) * kPeak;
                y_ = e > y_ ? y_ + (e - y_) * att_ : y_ * rel_;
            }
            break;
        case AuMeter::None: break;
        default:   // averaging: indicator needle, magic eye, automatic level lamp
            for (int i = 0; i < n; ++i) {
                const double u = std::fabs(x[i]) * kAvg;
                y_ += (u - y_) * (u > y_ ? att_ : rel_);
            }
        }
    }
    double db() const { return 20 * std::log10(std::max(y_ * cal_, 1e-6)); }

private:
    AuMeter meter_ = AuMeter::VU;
    double dt_ = 1 / 48000.0, att_ = 0, rel_ = 1, y_ = 0, v_ = 0, cal_ = 1;
};

// A reading travels to the panel as a read-only parameter, 0..1 over -60..+20 dB.
inline double auMeterToNorm(double db) { return std::clamp((db + 60) / 80, 0.0, 1.0); }
inline double auMeterFromNorm(double n) { return n * 80 - 60; }

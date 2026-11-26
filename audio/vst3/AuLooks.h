// SPDX-License-Identifier: GPL-3.0-or-later
// The front of every recorder and desk as data: plate colours and the level meter it had, with its scale and
// ballistics. Original drawings after period photographs and descriptions [gen], no makers' marks. One row per row of
// the preset tables (AudioPresets.h), in the same order: append only.
#pragma once
#include "AudioPresets.h"

#include <cstdint>

// The meter a machine had. How each one moves is in AuBallistics (AuMeters.h), with its source.
enum class AuMeter : uint8_t {
    VU,           // volume indicator: average, 99 % in 300 ms, 1-1.5 % overshoot; -20..+3, 0 VU at 71 % of the arc
    PPM,          // peak programme meter (BBC, IEC 60268-10 IIa): 10 ms burst -2 dB, falls 24 dB in 2.8 s; marks 1..7
    Modulometer,  // the Nagra's peak meter, scale in dB with an overload segment
    Indicator,    // a small consumer level needle: no numbers, a black and a red arc
    Eye,          // the "magic eye" indicator tube: a green fan that closes as the level rises (averaging)
    Led,          // a ladder of lamps, green to red, peak with a short hold
    Automatic,    // no meter: automatic level control, a lamp that lights with the sound
    None,         // nothing at all (the acoustic phonograph)
    Neon,         // a neon lamp that strikes above a set voltage: a peak indicator (wire recorders)
};

// A front: body gradient (top, bottom), name plate, its lettering, meter face, knob caps (0xRRGGBB), brushed metal,
// the meter and how many (1: one for both channels, 2: left and right, 4: one per track or output, 3 and 4 idle
// on a stereo signal).
struct AuLook
{
    uint32_t body, body2, plate, ink, face, cap;
    bool brushed;
    AuMeter meter;
    int meters;
};

inline constexpr AuLook kAuReelPortableLooks[] = {
    { 0x8E8A80, 0x66625A, 0x2A2622, 0xE6DCC8, 0x000000, 0x3A2E26, false, AuMeter::Eye, 1 },          // Geloso G.257
    { 0xC2C4C6, 0x8A8C8F, 0x1C1C1E, 0xEEEEEA, 0xF2F0EA, 0x202020, true, AuMeter::Modulometer, 1 },  // Nagra III
    { 0x5A5C60, 0x323438, 0xB8BABE, 0x1A1A1A, 0xF0EEE6, 0x202224, false, AuMeter::Indicator, 1 },    // Uher 4000
    { 0xCACCCE, 0x929497, 0x1C1C1E, 0xEEEEEA, 0xF4F2EC, 0x1A1A1A, true, AuMeter::Modulometer, 1 },  // Nagra 4.2
};
inline constexpr AuLook kAuReelStudioLooks[] = {
    { 0x8C9096, 0x5E6268, 0x2C2E30, 0xE8E4D8, 0xF0DFB0, 0x1E1E1E, false, AuMeter::VU, 1 },           // Ampex 350
    { 0xC8C6C0, 0x9C9A94, 0x505860, 0xF0F0EC, 0xF0E8D0, 0x3A3C3E, false, AuMeter::VU, 4 },           // Studer J37
    { 0xB4B6B8, 0x7C7E81, 0x161616, 0xEEEEEA, 0xE8DDBE, 0x1C1C1C, true, AuMeter::VU, 2 },            // Revox A77
    { 0xA8A49A, 0x7A766C, 0x2E3236, 0xF0F0EC, 0xF2EEE2, 0x2A2A2A, false, AuMeter::VU, 2 },           // Studer A80
};
inline constexpr AuLook kAuCassetteLooks[] = {
    { 0x5E6064, 0x3A3C40, 0xC0C2C4, 0x222222, 0xE8E8E0, 0x1E1E1E, false, AuMeter::Indicator, 1 },   // Philips EL 3300
    { 0x3A3A3C, 0x1C1C1E, 0xC8C8C8, 0x111111, 0xE8E0C8, 0x6A6A6A, false, AuMeter::VU, 4 },          // Portastudio 144
    { 0x2E2E30, 0x141416, 0x8E9094, 0x0E0E0E, 0x000000, 0x8C8E92, true, AuMeter::Led, 1 },          // WM-D6C
    { 0xA6A8AC, 0x6C6E72, 0x161616, 0xEEEEEA, 0xEDEFF0, 0x2A2A2C, true, AuMeter::VU, 2 },           // TC-D5M
};
inline constexpr AuLook kAuCameraLooks[] = {
    { 0x2C2A28, 0x151413, 0xB09060, 0x1A1208, 0xEAE0C6, 0x101010, false, AuMeter::VU, 1 },          // Auricon
    { 0x3E3E40, 0x202022, 0xD8D2C0, 0x202020, 0x000000, 0x1A1A1A, false, AuMeter::Automatic, 1 },   // Ektasound 130
    { 0x303032, 0x18181A, 0xBEC0C2, 0x101010, 0xE6E6E0, 0x1A1A1A, true, AuMeter::Indicator, 1 },    // Canon 514XL-S
    { 0x9A9EA4, 0x686C72, 0x2A2C30, 0xF0F0EC, 0x000000, 0x2A2C30, false, AuMeter::Automatic, 1 },   // JVC GR-C1
    { 0x2A2A2C, 0x141416, 0x6A6C70, 0xF0F0EC, 0x000000, 0x1A1A1A, false, AuMeter::Automatic, 1 },   // CCD-V8
};
inline constexpr AuLook kAuEarlyLooks[] = {
    { 0x7C4E2A, 0x4A2A12, 0xC6A25E, 0x32200C, 0x000000, 0xB08A48, false, AuMeter::None, 1 },        // Edison
    { 0x8A6A4A, 0x58422E, 0xD8C8A0, 0x3A2A18, 0x000000, 0x3A2A1A, false, AuMeter::Neon, 1 },        // Webster-Chicago
};
inline constexpr const AuLook* kAuFamilyLooks[] = {
    kAuReelPortableLooks, kAuReelStudioLooks, kAuCassetteLooks, kAuCameraLooks, kAuEarlyLooks,
};
static_assert(std::size(kAuReelPortableLooks) == std::size(kAuReelPortable));
static_assert(std::size(kAuReelStudioLooks) == std::size(kAuReelStudio));
static_assert(std::size(kAuCassetteLooks) == std::size(kAuCassetteDecks));
static_assert(std::size(kAuCameraLooks) == std::size(kAuCameras));
static_assert(std::size(kAuEarlyLooks) == std::size(kAuEarly));
static_assert(std::size(kAuFamilyLooks) == std::size(kAuFamilies));

inline constexpr AuLook kAuDeskLooks[] = {
    { 0x4A4C50, 0x2C2E32, 0x1E1E20, 0xD6D2C8, 0xE8E2CE, 0x343436, false, AuMeter::VU, 2 },          // none
    { 0x9C968A, 0x6E685E, 0x1E2226, 0xDCE0E2, 0xF0E0B0, 0x282C30, false, AuMeter::VU, 1 },          // RCA BC-2B
    { 0x7A8276, 0x545C52, 0xE2DECE, 0x1E201C, 0xEEE6D2, 0x383C36, false, AuMeter::VU, 4 },          // EMI REDD.51
    { 0x64708A, 0x404C5C, 0xCAC6BA, 0x1C1E22, 0xEEE6D4, 0x782C28, false, AuMeter::VU, 2 },          // Neve 8078
    { 0x3E4044, 0x222326, 0xD6D6D6, 0x18181A, 0xE6E8E8, 0x5C6064, false, AuMeter::VU, 2 },          // SSL 4000 E
    { 0x383838, 0x1E1E20, 0xDEDEDE, 0x141416, 0x000000, 0x282828, true, AuMeter::Led, 2 },          // Mackie CR-1604
};
static_assert(std::size(kAuDeskLooks) == std::size(kAuConsoles));

inline const AuLook& auDeviceLook(int family, int model) { return kAuFamilyLooks[family][model]; }

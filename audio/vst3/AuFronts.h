// SPDX-License-Identifier: GPL-3.0-or-later
// The front panel of every recorder and desk as data: which controls it had, where, with the words engraved on them
// and their positions, and what each one does in the plugin. One front per row of the preset tables (AudioPresets.h),
// same order. Original drawings without makers' marks; the layout and the labels follow the manuals where we have
// them (docs/audio/AUDIO.md, «Frontali e comandi»: the source of each machine and what is deduced).
//
// A control is Live (moves a parameter), Mech (a mechanical control of the real machine with no bearing on the sound:
// it moves and latches, nothing more) or Idle (a channel or input the plugin does not feed: drawn dimmed).
#pragma once
#include "AuParams.h"

#include <cstdint>

enum class AuWidget : uint8_t {
    Knob,      // a continuous pot with a dial
    Selector,  // a pointer knob with named positions around it
    Coin,      // a selector turned with a coin in its slot (Nagra III)
    Lever,     // a bat toggle, positions top to bottom
    Slide,     // a slide switch, positions left to right
    Keys,      // a row of piano keys or push-buttons, one latched at a time
    Fader,     // a straight fader
    Quadrant,  // a quadrant fader (an arc, the lever pivoting below)
    Button,    // a push-button that acts while held
};
enum class AuUse : uint8_t { Live, Mech, Idle };

// What a position sets, besides parameters (an AuParamId ≥ 0): the medium by its row in kAuMedia, the noise reduction
// by its row in kAuNR (the panel turns them into the menu's plain values), what the meter reads, or a reference tone.
constexpr int kSetNone = -1, kSetMedium = -2, kSetNR = -3, kSetMeter = -4, kSetTone = -5;
enum AuMeterSource : int {
    kReadIn,       // the signal going to tape (record, test): input level × record level
    kReadOut,      // what comes back (playback, tape monitor): the plugin's output
    kReadBattery,  // a fresh battery, steady
    kReadFixed,    // a setting the machine measures and the plugin does not have (bias, erase, pilot): at its mark
    kReadZero,     // nothing to read in this position
};

struct AuSet
{
    int target = kSetNone;
    float value = 0;
};
struct AuPos
{
    const char* label;
    AuSet a = {}, b = {}, c = {}, d = {};
    uint32_t colour = 0;   // a key's own colour (0: the front's)
};

struct AuControl
{
    AuWidget kind;
    int16_t x, y, w, h;
    const char* label;
    AuUse use;
    int param;               // continuous: the parameter, plain = from + (to - from) × position
    float from, to;
    const char* dial;        // continuous: the numbers on the dial, '|' apart, evenly spread
    const AuPos* pos;        // discrete: the positions, in order (left to right, top to bottom, anticlockwise)
    int count, def;          // how many, and the one the machine starts in
    uint32_t cap;            // the knob's colour (0: the front's)
};

struct AuFront
{
    int16_t mx, my, mw, mh;  // the meter (or meters) in the front
    int16_t nx, ny;          // the name plate
    AuMeterSource reads;     // what the meter reads before any control says otherwise
    const AuControl* controls;
    int count;
};

constexpr AuControl auKnob(int x, int y, int d, const char* label, int param, float from, float to, const char* dial,
                           AuUse use = AuUse::Live, uint32_t cap = 0)
{
    return { AuWidget::Knob, int16_t(x), int16_t(y), int16_t(d), int16_t(d), label, use, param, from, to, dial,
             nullptr, 0, 0, cap };
}
template <size_t N>
constexpr AuControl auPick(AuWidget kind, int x, int y, int w, int h, const char* label, const AuPos (&pos)[N],
                           int def, AuUse use = AuUse::Live, uint32_t cap = 0)
{
    return { kind, int16_t(x), int16_t(y), int16_t(w), int16_t(h), label, use, -1, 0, 0, nullptr, pos, int(N), def, cap };
}
constexpr AuControl auFader(AuWidget kind, int x, int y, int w, int h, const char* label, int param, float from,
                            float to, const char* dial, AuUse use = AuUse::Live)
{
    return { kind, int16_t(x), int16_t(y), int16_t(w), int16_t(h), label, use, param, from, to, dial, nullptr, 0, 0, 0 };
}
#define AU_FRONT(mx, my, mw, mh, nx, ny, reads, controls) { mx, my, mw, mh, nx, ny, reads, controls, int(std::size(controls)) }

// ---- positions shared by several machines

inline constexpr AuSet kRun = { kPBypass, 0 }, kStop = { kPBypass, 1 };
inline constexpr AuSet kReadsIn = { kSetMeter, kReadIn }, kReadsOut = { kSetMeter, kReadOut };
inline constexpr AuSet kTapeOn = { kPOnMedium, 1 }, kTapeOff = { kPOnMedium, 0 };
inline constexpr AuPos kOnOff[] = { { "OFF", kStop }, { "ON", kRun } };
inline constexpr AuPos kOffOnMech[] = { { "OFF" }, { "ON" } };
inline constexpr AuPos kInputTape[] = { { "INPUT", kTapeOff, kReadsIn }, { "TAPE", kTapeOn, kReadsOut } };

// ================================================================== Registratori a bobine portatili

// Geloso G.257 (1961): the record indicator on the left, five coloured keys on the right (green play, grey stop, white
// fast forward, yellow rewind, red record), the red volume knob and the power wheel in front.
inline constexpr AuPos kGelosoKeys[] = {
    { "ASCOLTO", kRun, kReadsOut, {}, {}, 0x3C8C4A }, { "ARRESTO", kStop, {}, {}, {}, 0x8C8C88 },
    { "AVANTI", {}, {}, {}, {}, 0xE8E6DE }, { "INDIETRO", {}, {}, {}, {}, 0xD8B640 },
    { "REGISTRAZIONE", kRun, kReadsIn, {}, {}, 0xB82A22 },
};
inline constexpr AuPos kGelosoPower[] = { { "SPENTO", kStop }, { "ACCESO", kRun } };
inline constexpr AuControl kGelosoControls[] = {
    auKnob(206, 150, 64, "VOLUME", kPRecLevel, -24, 12, "0|1|2|3|4|5|6|7|8|9|10", AuUse::Live, 0xB02A22),
    auPick(AuWidget::Keys, 300, 150, 330, 44, "", kGelosoKeys, 0),
    auPick(AuWidget::Slide, 470, 214, 120, 22, "RETE", kGelosoPower, 1),
};

// Nagra III (manual, 1963): speed and equalisation selector turned with a coin, microphone level, the modulation level
// meter (overload segment 0..+2 dB, battery scale below), the zero-level reference button, line input and playback
// volume, the six-position function selector, the transport control.
inline constexpr AuPos kNagra3Speed[] = {
    { "38,1", { kSetMedium, 1 } }, { "19,05 CCIR", { kSetMedium, 2 } }, { "19,05 AMPEX", { kSetMedium, 2 } },
    { "9,525", { kSetMedium, 3 } },
};
inline constexpr AuPos kNagra3Function[] = {
    { "PLAY+BATT.", kRun, kTapeOn, { kSetMeter, kReadBattery } }, { "HI-FI PLAY", kRun, kTapeOn, kReadsOut },
    { "STOP", kStop }, { "TEST", kRun, kTapeOff, kReadsIn },
    { "HI-FI REC", kRun, kTapeOn, kReadsIn, { kPAuto, 0 } }, { "AUTO REC", kRun, kTapeOn, kReadsIn, { kPAuto, 1 } },
};
inline constexpr AuPos kNagra3Transport[] = { { "REW" }, { "0" }, { "EN-ON" } };
inline constexpr AuPos kRefButton[] = { { "REF", { kSetTone, 0 } } };          // the zero-level whistle reads 0 dB
inline constexpr AuPos kRefOscillator[] = { { "REF", { kSetTone, -8 } } };     // 4.2: "the modulometer shows -8 dB"
inline constexpr AuControl kNagra3Controls[] = {
    auPick(AuWidget::Coin, 34, 44, 60, 60, "SPEED", kNagra3Speed, 1),
    auPick(AuWidget::Selector, 20, 150, 70, 70, "TAPE", kNagra3Transport, 2, AuUse::Mech),
    auKnob(116, 156, 56, "MICRO", kPRecLevel, -24, 12, "0|2|4|6|8|10"),
    auPick(AuWidget::Button, 420, 60, 36, 36, "ZERO LEVEL", kRefButton, 0),
    auKnob(404, 166, 48, "LINE · VOLUME", kPOut, -24, 12, "0|2|4|6|8|10"),
    auPick(AuWidget::Selector, 480, 92, 116, 116, "", kNagra3Function, 4),
};

// Uher 4000 Report-L: the four-speed gear selector at the top right (its top position switches off), the level meter
// (pull the volume knob: battery; pull the tone knob: lamp), record level, piano keys and the pause lever.
inline constexpr AuPos kUherSpeed[] = {
    { "0", kStop }, { "2,4", kRun, { kSetMedium, 24 } }, { "4,75", kRun, { kSetMedium, 4 } },
    { "9,5", kRun, { kSetMedium, 3 } }, { "19", kRun, { kSetMedium, 2 } },
};
inline constexpr AuPos kUherKeys[] = {
    { "RÜCKLAUF" }, { "START", kRun, kReadsOut }, { "VORLAUF" }, { "HALT", kStop }, { "AUFNAHME", kRun, kReadsIn, {}, {}, 0xB02A22 },
};
inline constexpr AuPos kUherBattery[] = { { "PEGEL", kReadsIn }, { "BATT.", { kSetMeter, kReadBattery } } };
inline constexpr AuPos kUherPause[] = { { "PAUSE" }, { "" } };
inline constexpr AuControl kUherControls[] = {
    auKnob(220, 40, 58, "AUSSTEUERUNG", kPRecLevel, -24, 12, "0|2|4|6|8|10"),
    auKnob(306, 40, 50, "LAUTSTÄRKE", kPOut, -24, 12, "0|10"),
    auPick(AuWidget::Lever, 314, 118, 34, 40, "", kUherBattery, 0),
    auKnob(384, 40, 50, "KLANG", kPHigh, -15, 0, "−|+"),
    auPick(AuWidget::Selector, 520, 26, 100, 100, "", kUherSpeed, 4),
    auPick(AuWidget::Keys, 196, 186, 360, 42, "", kUherKeys, 1),
    auPick(AuWidget::Lever, 586, 176, 34, 46, "", kUherPause, 1, AuUse::Mech),
};

// Nagra 4.2 (manual, 1991): manual / automatic level, filter, modulometer and its eleven-position mode selector,
// tape / direct, speed and equalisation, two microphone levels, reference oscillator, line and playback, the main
// function selector.
inline constexpr AuPos kNagra42Auto[] = { { "MAN.", { kPAuto, 0 } }, { "AUTO 1", { kPAuto, 1 } }, { "AUTO 2", { kPAuto, 1 } } };
inline constexpr AuPos kNagra42Filter[] = {   // the loss at 50 Hz engraved on each position, as a second-order cut
    { "LFA 2", { kPLowCut, 76 } }, { "LFA 1", { kPLowCut, 55 } }, { "FLAT", { kPLowCut, 0 } }, { "HP 1", { kPLowCut, 87 } },
    { "HP1+LFA1", { kPLowCut, 111 } }, { "HP 2", { kPLowCut, 158 } },
};
inline constexpr AuPos kNagra42Meter[] = {
    { "X", { kSetMeter, kReadZero } }, { "RX", { kSetMeter, kReadZero } }, { "SYNCH", { kSetMeter, kReadFixed } },
    { "PILOT", { kSetMeter, kReadZero } }, { "PILOT FREQ", { kSetMeter, kReadFixed } }, { "LEVEL", kReadsIn },
    { "BATT. RES.", { kSetMeter, kReadBattery } }, { "VOLT/CELL", { kSetMeter, kReadBattery } },
    { "COMPR.", { kSetMeter, kReadZero } }, { "MOT", { kSetMeter, kReadFixed } }, { "BIAS", { kSetMeter, kReadFixed } },
};
inline constexpr AuPos kNagra42Speed[] = {
    { "3¾ I", { kSetMedium, 3 } }, { "3¾ II", { kSetMedium, 3 } }, { "7½ I", { kSetMedium, 2 } },
    { "7½ II", { kSetMedium, 2 } }, { "15 I", { kSetMedium, 1 } }, { "15 II", { kSetMedium, 1 } },
};
inline constexpr AuPos kNagra42Function[] = {
    { "STOP", kStop }, { "TEST", kRun, kTapeOff, kReadsIn }, { "RECORD", kRun, kTapeOn, kReadsIn, { kPLimit, 1 } },
    { "REC. NO LIM.", kRun, kTapeOn, kReadsIn, { kPLimit, 0 } }, { "PLAYBACK", kRun, kTapeOn, kReadsOut },
    { "PLAYBACK LS", kRun, kTapeOn, kReadsOut },
};
inline constexpr AuPos kNagra42Monitor[] = { { "TAPE", kTapeOn, kReadsOut }, { "DIRECT", kTapeOff, kReadsIn } };
inline constexpr AuControl kNagra42Controls[] = {
    auPick(AuWidget::Selector, 30, 36, 56, 56, "", kNagra42Auto, 0),
    auPick(AuWidget::Selector, 126, 36, 56, 56, "FILTER", kNagra42Filter, 2),
    auPick(AuWidget::Selector, 438, 30, 84, 84, "", kNagra42Meter, 5),
    auPick(AuWidget::Lever, 592, 36, 30, 42, "", kNagra42Monitor, 1),
    auPick(AuWidget::Coin, 34, 178, 50, 50, "SPEED", kNagra42Speed, 4),
    auKnob(116, 186, 44, "LEVEL 1", kPRecLevel, -24, 12, "0|2|4|6|8|10"),
    auKnob(186, 186, 44, "LEVEL 2", kPRecLevel, -24, 12, "0|2|4|6|8|10", AuUse::Idle),
    auPick(AuWidget::Button, 252, 196, 26, 26, "REF.", kRefOscillator, 0),
    auKnob(308, 186, 44, "LINE · PB", kPOut, -24, 12, "0|10"),
    auPick(AuWidget::Selector, 448, 124, 118, 118, "", kNagra42Function, 4),
};

// ================================================================== Registratori a bobine da studio

// Ampex 350/351 (manual of the 351, 1959): record level, playback level, the 4-inch VU, the meter and output switch
// (playback level, record level, bias, erase: what is heard follows it), input transfer, equalisation, power, record
// button; on the transport: speed, reel size, rewind, fast forward, stop, play.
inline constexpr AuPos kAmpexMeter[] = {
    { "PLAYBACK LEVEL", kTapeOn, kReadsOut }, { "RECORD LEVEL", kTapeOff, kReadsIn },
    { "BIAS", kTapeOn, { kSetMeter, kReadFixed } }, { "ERASE", kTapeOn, { kSetMeter, kReadFixed } },
};
inline constexpr AuPos kAmpexInput[] = { { "MICROPHONE", { kPOnMic, 1 } }, { "BAL. BRIDGE", { kPOnMic, 0 } }, { "UNBAL. BRIDGE", { kPOnMic, 0 } } };
inline constexpr AuPos kAmpexEq[] = { { "LOW SPEED" }, { "HIGH SPEED" } };
inline constexpr AuPos kAmpexSpeed[] = { { "LOW", { kSetMedium, 2 } }, { "HIGH", { kSetMedium, 1 } } };
inline constexpr AuPos kAmpexReel[] = { { "LARGE" }, { "SMALL" } };
inline constexpr AuPos kAmpexKeys[] = { { "REWIND" }, { "FAST FWD" }, { "STOP", kStop }, { "PLAY", kRun } };
inline constexpr AuControl kAmpexControls[] = {
    auKnob(30, 32, 56, "RECORD LEVEL", kPRecLevel, -24, 12, "0|10"),
    auKnob(116, 32, 56, "PLAYBACK LEVEL", kPOut, -24, 12, "0|10"),
    auPick(AuWidget::Selector, 500, 44, 80, 80, "METER & OUTPUT", kAmpexMeter, 0),
    auPick(AuWidget::Lever, 436, 176, 30, 42, "POWER", kOnOff, 1),
    auPick(AuWidget::Selector, 76, 136, 56, 56, "INPUT TRANSFER", kAmpexInput, 0),
    auPick(AuWidget::Lever, 170, 140, 30, 42, "EQUALIZATION", kAmpexEq, 1, AuUse::Mech),
    auPick(AuWidget::Lever, 512, 176, 30, 42, "TAPE SPEED", kAmpexSpeed, 1),
    auPick(AuWidget::Lever, 588, 176, 30, 42, "REEL SIZE", kAmpexReel, 0, AuUse::Mech),
    auPick(AuWidget::Keys, 216, 204, 200, 34, "", kAmpexKeys, 3),
};

// Studer J37: a four-track machine, a VU meter per track over the transport; record and replay level, input or tape,
// two speeds. The labels are not in a manual we could read: deduced from photographs.
inline constexpr AuPos kJ37Speed[] = { { "19", { kSetMedium, 2 } }, { "38", { kSetMedium, 1 } } };
inline constexpr AuPos kJ37Tracks[] = { { "1" }, { "2" }, { "3" }, { "4" } };
inline constexpr AuPos kJ37Keys[] = { { "◀◀" }, { "▶▶" }, { "STOP", kStop }, { "START", kRun }, { "AUFNAHME", kRun, {}, {}, {}, 0xB02A22 } };
inline constexpr AuControl kJ37Controls[] = {
    auKnob(26, 138, 46, "AUFNAHME", kPRecLevel, -24, 12, "0|10"),
    auKnob(100, 138, 46, "WIEDERGABE", kPOut, -24, 12, "0|10"),
    auPick(AuWidget::Lever, 176, 144, 30, 40, "", kInputTape, 1),
    auPick(AuWidget::Lever, 232, 144, 30, 40, "cm/s", kJ37Speed, 1),
    auPick(AuWidget::Keys, 290, 142, 200, 28, "SPUR", kJ37Tracks, 0, AuUse::Mech),
    auPick(AuWidget::Keys, 290, 204, 336, 34, "", kJ37Keys, 3),
};

// Revox A77 (service guide): per channel an input selector (MIC HI, MIC LO, AUX) and a record level, the two VU meters
// between them; speed, power, monitor (input or tape), IEC/NAB, mono/stereo, volume and balance; the transport keys.
inline constexpr AuPos kA77Input[] = { { "MIC LO", { kPOnMic, 1 } }, { "MIC HI", { kPOnMic, 1 } }, { "AUX", { kPOnMic, 0 } } };
inline constexpr AuPos kA77Speed[] = { { "3¾", { kSetMedium, 3 } }, { "7½", { kSetMedium, 2 } } };
inline constexpr AuPos kA77Eq[] = { { "IEC" }, { "NAB" } };
inline constexpr AuPos kA77Mono[] = { { "MONO" }, { "STEREO" } };
inline constexpr AuPos kA77Keys[] = { { "REW" }, { "FFW" }, { "PLAY", kRun }, { "STOP", kStop }, { "REC", kRun, {}, {}, {}, 0xB02A22 } };
inline constexpr AuControl kA77Controls[] = {
    auPick(AuWidget::Selector, 22, 26, 76, 76, "INPUT 1", kA77Input, 0),
    auKnob(30, 130, 58, "LEVEL 1", kPRecLevel, -24, 12, "0|10"),
    auPick(AuWidget::Selector, 546, 26, 76, 76, "INPUT 2", kA77Input, 0),
    auKnob(554, 130, 58, "LEVEL 2", kPRecLevel, -24, 12, "0|10"),
    auPick(AuWidget::Lever, 140, 186, 28, 32, "", kA77Speed, 1),
    auPick(AuWidget::Lever, 192, 186, 28, 32, "", kA77Eq, 0, AuUse::Mech),
    auPick(AuWidget::Lever, 244, 186, 28, 32, "MONITOR", kInputTape, 1),
    auPick(AuWidget::Lever, 296, 186, 28, 32, "", kA77Mono, 1, AuUse::Mech),
    auKnob(352, 182, 36, "VOLUME", kPOut, -24, 12, "0|10"),
    auKnob(414, 182, 36, "BALANCE", -1, 0, 0, "L|R", AuUse::Mech),
    auPick(AuWidget::Lever, 476, 186, 28, 32, "POWER", kOnOff, 1),
    auPick(AuWidget::Keys, 150, 234, 340, 20, "", kA77Keys, 2),
};

// Studer A80: the meter panel with its two VU meters over the channel units (input and output level, the INPUT, SYNC,
// REPRO keys, ready or safe), speed and equalisation, the transport keys. Deduced from photographs.
inline constexpr AuPos kA80Mode[] = { { "INPUT", kTapeOff, kReadsIn }, { "SYNC", kTapeOn, kReadsOut }, { "REPRO", kTapeOn, kReadsOut } };
inline constexpr AuPos kA80Ready[] = { { "SAFE" }, { "READY", {}, {}, {}, {}, 0xC8A030 } };
inline constexpr AuPos kA80Speed[] = { { "19", { kSetMedium, 2 } }, { "38", { kSetMedium, 1 } } };
inline constexpr AuPos kA80Eq[] = { { "CCIR" }, { "NAB" } };
inline constexpr AuPos kA80Keys[] = { { "◀◀" }, { "▶▶" }, { "PLAY", kRun }, { "STOP", kStop }, { "REC", kRun, {}, {}, {}, 0xB02A22 } };
inline constexpr AuControl kA80Controls[] = {
    auKnob(24, 26, 50, "INPUT", kPRecLevel, -24, 12, "−|+"),
    auKnob(24, 96, 50, "OUTPUT", kPOut, -24, 12, "−|+"),
    auPick(AuWidget::Keys, 100, 150, 150, 26, "", kA80Mode, 2),
    auPick(AuWidget::Keys, 290, 150, 100, 26, "", kA80Ready, 1, AuUse::Mech),
    auPick(AuWidget::Keys, 528, 150, 96, 26, "cm/s", kA80Speed, 1),
    auPick(AuWidget::Lever, 584, 26, 34, 46, "", kA80Eq, 0, AuUse::Mech),
    auPick(AuWidget::Keys, 270, 208, 354, 30, "", kA80Keys, 2),
};

// ================================================================== Registratori a cassetta

// Philips EL 3300 (1963): the first Compact Cassette recorder; one slide for the tape, the record key, the level wheel
// and a small needle that is also the battery check. Deduced from photographs.
inline constexpr AuPos kEL3300Slide[] = { { "◀◀" }, { "■", kStop }, { "▶", kRun, kReadsOut }, { "▶▶" } };
inline constexpr AuPos kEL3300Rec[] = { { "", kReadsOut }, { "●", kReadsIn, {}, {}, {}, 0xB02A22 } };
inline constexpr AuControl kEL3300Controls[] = {
    auKnob(240, 40, 70, "", kPRecLevel, -24, 12, "0|10"),
    auPick(AuWidget::Slide, 340, 60, 220, 30, "", kEL3300Slide, 2),
    auPick(AuWidget::Keys, 580, 54, 44, 40, "", kEL3300Rec, 0),
};

// TASCAM Portastudio 144 (owner's manual): four input channels (MIC/LINE-TAPE, TRIM, AUX SEND, EQUALIZER bass and
// treble, PAN, fader, record select), the master fader, pitch, buss monitor, cue and remix, four lit VU meters.
// The plugin feeds channel 1; the meters of tracks 3 and 4 rest.
inline constexpr AuPos kMicTape[] = { { "MIC/LINE" }, { "TAPE" } };
inline constexpr AuPos kDbx[] = { { "dbx ON", { kPOnNR, 1 } }, { "OFF", { kPOnNR, 0 } } };
inline constexpr AuPos kPortaKeys[] = { { "◀◀" }, { "▶▶" }, { "PLAY", kRun }, { "STOP", kStop }, { "REC", kRun, {}, {}, {}, 0xB02A22 } };
#define AU_PORTA_STRIP(x, use, fader)                                                         \
    auPick(AuWidget::Lever, x + 4, 112, 24, 26, "", kMicTape, 0, AuUse::Mech),                \
    auKnob(x + 4, 158, 24, "", kPRecLevel, -24, 12, "|", use),                                      \
    auKnob(x + 4, 190, 24, "", kPHigh, -12, 12, "|", use),                                    \
    auKnob(x + 4, 222, 24, "", kPLow, -12, 12, "|", use),                                     \
    auFader(AuWidget::Fader, x + 42, 110, 22, 136, "", fader, -24, 12, "10|8|6|4|2|0", use)
inline constexpr AuControl kPortaControls[] = {
    AU_PORTA_STRIP(160, AuUse::Live, kPOut), AU_PORTA_STRIP(236, AuUse::Idle, kPOut),
    AU_PORTA_STRIP(312, AuUse::Idle, kPOut), AU_PORTA_STRIP(388, AuUse::Idle, kPOut),
    auFader(AuWidget::Fader, 478, 110, 26, 136, "MASTER", kPOut, -24, 12, "10|8|6|4|2|0"),
    auPick(AuWidget::Lever, 530, 116, 28, 36, "", kDbx, 0),
    auKnob(588, 112, 36, "PITCH", -1, 0, 0, "−|+", AuUse::Mech),
    auKnob(530, 196, 36, "CUE", -1, 0, 0, "|", AuUse::Mech),
    auKnob(588, 196, 36, "BUSS", -1, 0, 0, "|", AuUse::Mech),
    auPick(AuWidget::Keys, 16, 204, 124, 36, "", kPortaKeys, 2),
};
#undef AU_PORTA_STRIP

// Sony WM-D6C (operating instructions): five-LED peak and battery indicator with its selector, DOLBY NR (B, C), TAPE
// selector, REC LEVEL, MIC ATT, the keys, HEADPHONE VOLUME, SPEED TUNE.
inline constexpr AuPos kD6Indicator[] = { { "PEAK", kReadsIn }, { "BATT", { kSetMeter, kReadBattery } } };
inline constexpr AuPos kDolbyBC[] = { { "OFF", { kPOnNR, 0 } }, { "B", { kPOnNR, 1 }, { kSetNR, 2 } }, { "C", { kPOnNR, 1 }, { kSetNR, 3 } } };
inline constexpr AuPos kCassetteType[] = { { "NORM", { kSetMedium, 5 } }, { "CrO₂", { kSetMedium, 6 } }, { "METAL", { kSetMedium, 7 } } };
inline constexpr AuPos kMicAtt[] = { { "0" }, { "−20" } };
inline constexpr AuPos kD6Keys[] = {
    { "REC", kRun, kReadsIn, {}, {}, 0xB02A22 }, { "II" }, { "CUE" }, { "REVIEW" }, { "PLAY", kRun, kReadsOut }, { "■", kStop },
};
inline constexpr AuControl kD6Controls[] = {
    auPick(AuWidget::Slide, 200, 34, 100, 22, "", kD6Indicator, 0),
    auPick(AuWidget::Slide, 330, 34, 120, 22, "DOLBY NR", kDolbyBC, 1),
    auPick(AuWidget::Slide, 480, 34, 140, 22, "TAPE", kCassetteType, 1),
    auKnob(30, 140, 64, "REC LEVEL", kPRecLevel, -24, 12, "0|2|4|6|8|10"),
    auPick(AuWidget::Slide, 120, 160, 70, 22, "MIC ATT", kMicAtt, 0, AuUse::Mech),
    auPick(AuWidget::Keys, 214, 136, 330, 38, "", kD6Keys, 4),
    auKnob(568, 130, 50, "VOLUME", kPOut, -24, 12, "0|10"),
    auKnob(568, 192, 40, "SPEED TUNE", -1, 0, 0, "−|+", AuUse::Mech),
};

// Sony TC-D5M: two needle meters, record level, DOLBY NR, TAPE, LIMITER, MIC/LINE, the keys. Deduced from photographs.
inline constexpr AuPos kDolbyOnOff[] = { { "OFF", { kPOnNR, 0 } }, { "ON", { kPOnNR, 1 } } };
inline constexpr AuPos kLimiter[] = { { "OFF", { kPLimit, 0 } }, { "ON", { kPLimit, 1 } } };
inline constexpr AuPos kMicLine[] = { { "MIC", { kPOnMic, 1 } }, { "LINE", { kPOnMic, 0 } } };
inline constexpr AuPos kD5Keys[] = {
    { "REC", kRun, kReadsIn, {}, {}, 0xB02A22 }, { "◀◀" }, { "▶", kRun, kReadsOut }, { "▶▶" }, { "■", kStop }, { "II" },
};
inline constexpr AuControl kD5Controls[] = {
    auKnob(30, 30, 60, "REC LEVEL", kPRecLevel, -24, 12, "0|2|4|6|8|10"),
    auPick(AuWidget::Lever, 30, 130, 34, 46, "DOLBY NR", kDolbyOnOff, 1),
    auPick(AuWidget::Lever, 96, 130, 34, 46, "LIMITER", kLimiter, 0),
    auPick(AuWidget::Lever, 162, 130, 34, 46, "", kMicLine, 0),
    auPick(AuWidget::Slide, 450, 140, 170, 22, "TAPE", kCassetteType, 2),
    auPick(AuWidget::Keys, 250, 204, 374, 34, "", kD5Keys, 2),
    auKnob(130, 30, 48, "VOLUME", kPOut, -24, 12, "0|10"),
};

// ================================================================== Cinepresa e videocamera sonora

// Auricon Cine-Voice: the camera and its recording amplifier (microphone gain, volume meter, battery test, headphone).
// No manual read: deduced from the amplifiers' description (inputs, headphone, meter, battery test).
inline constexpr AuPos kBatteryTest[] = { { "BATT. TEST", { kSetMeter, kReadBattery } } };
inline constexpr AuPos kCameraRun[] = { { "OFF", kStop }, { "RUN", kRun } };
inline constexpr AuControl kAuriconControls[] = {
    auKnob(300, 40, 66, "GAIN", kPRecLevel, -24, 12, "0|10"),
    auKnob(300, 140, 48, "PHONES", kPOut, -24, 12, "0|10"),
    auPick(AuWidget::Button, 590, 60, 34, 34, "", kBatteryTest, 0),
    auPick(AuWidget::Lever, 590, 150, 34, 46, "MOTOR", kCameraRun, 1),
};

// Kodak Ektasound 130: automatic level, nothing to turn; the trigger. Deduced.
inline constexpr AuPos kTrigger[] = { { "", kStop }, { "●", kRun } };
inline constexpr AuControl kEktaControls[] = { auPick(AuWidget::Keys, 520, 150, 60, 50, "", kTrigger, 1) };

// Canon 514XL-S: automatic or manual sound level, the manual level, the needle; the trigger. Deduced.
inline constexpr AuPos kAutoManual[] = { { "AUTO", { kPAuto, 1 } }, { "MANUAL", { kPAuto, 0 } } };
inline constexpr AuControl kCanonControls[] = {
    auPick(AuWidget::Lever, 260, 60, 34, 46, "REC LEVEL", kAutoManual, 1),
    auKnob(320, 52, 60, "", kPRecLevel, -24, 12, "0|10"),
    auPick(AuWidget::Keys, 520, 150, 60, 50, "", kTrigger, 1),
};

// JVC GR-C1 and Sony CCD-V8: automatic level, the record start/stop button. Deduced.
inline constexpr AuPos kRecStartStop[] = { { "STANDBY", kStop }, { "REC", kRun, {}, {}, {}, 0xB02A22 } };
inline constexpr AuControl kCamcorderControls[] = { auPick(AuWidget::Keys, 440, 150, 150, 50, "", kRecStartStop, 1) };

// ================================================================== Prima del nastro

// Edison phonograph: no meter, no knob; the start lever and the speed screw of the governor.
inline constexpr AuPos kEdisonLever[] = { { "STOP", kStop }, { "START", kRun } };
inline constexpr AuControl kEdisonControls[] = {
    auPick(AuWidget::Lever, 300, 130, 34, 46, "", kEdisonLever, 1),
    auKnob(380, 130, 40, "SPEED", -1, 0, 0, "−|+", AuUse::Mech),
};

// Webster-Chicago 80/180 (service instructions): record-listen switch, microphone volume, tone, on-off, the run-rewind
// lever, output selector; a neon lamp shows the recording level.
inline constexpr AuPos kWebsterMode[] = { { "RECORD", kReadsIn }, { "LISTEN", kReadsOut } };
inline constexpr AuPos kWebsterRun[] = { { "STOP" }, { "RUN" }, { "REWIND" } };
inline constexpr AuPos kWebsterOutput[] = { { "1" }, { "2" }, { "3" } };
inline constexpr AuControl kWebsterControls[] = {
    auPick(AuWidget::Lever, 200, 40, 34, 46, "", kWebsterMode, 0),
    auKnob(260, 34, 60, "MICROPHONE VOLUME", kPRecLevel, -24, 12, "0|10"),
    auKnob(360, 34, 60, "TONE", kPHigh, -15, 0, "−|+"),
    auPick(AuWidget::Lever, 460, 40, 34, 46, "", kOnOff, 1),
    auPick(AuWidget::Selector, 200, 140, 90, 90, "RUN-REWIND", kWebsterRun, 1, AuUse::Mech),
    auPick(AuWidget::Selector, 340, 150, 74, 74, "OUTPUT", kWebsterOutput, 0, AuUse::Mech),
};

// Perfectone EP 6 (1959) [dedotto: no manual found; the controls a synchronous film recorder of its kind had]: the VU
// on the left, record level, the pilot check, playback volume, the function selector, the meter switch (level or
// battery). One speed, 19,05 cm/s full track: no speed selector.
inline constexpr AuPos kEP6Function[] = {
    { "STOP", kStop }, { "TEST", kRun, kTapeOff, kReadsIn }, { "ENREG.", kRun, kTapeOn, kReadsIn },
    { "LECTURE", kRun, kTapeOn, kReadsOut },
};
inline constexpr AuPos kEP6Meter[] = { { "MODUL.", kReadsIn }, { "PILOTE", { kSetMeter, kReadFixed } },
                                       { "BATT.", { kSetMeter, kReadBattery } } };
inline constexpr AuPos kEP6Limit[] = { { "OFF", { kPLimit, 0 } }, { "ON", { kPLimit, 1 } } };
inline constexpr AuControl kEP6Controls[] = {
    auKnob(240, 40, 60, "NIVEAU", kPRecLevel, -24, 12, "0|2|4|6|8|10"),
    auKnob(330, 40, 50, "ÉCOUTE", kPOut, -24, 12, "0|10"),
    auPick(AuWidget::Selector, 420, 30, 100, 100, "", kEP6Function, 2),
    auPick(AuWidget::Selector, 340, 140, 80, 80, "MESURE", kEP6Meter, 0),
    auPick(AuWidget::Lever, 560, 50, 34, 46, "LIMITEUR", kEP6Limit, 0),
};

inline constexpr AuFront kAuReelPortableFronts[] = {
    AU_FRONT(26, 40, 160, 160, 300, 30, kReadOut, kGelosoControls),
    AU_FRONT(170, 24, 230, 120, 186, 172, kReadIn, kNagra3Controls),
    AU_FRONT(24, 30, 170, 110, 24, 150, kReadIn, kUherControls),
    AU_FRONT(196, 16, 220, 120, 206, 140, kReadOut, kNagra42Controls),
};
inline constexpr AuFront kAuReelStudioFronts[] = {
    AU_FRONT(220, 16, 200, 150, 14, 206, kReadOut, kAmpexControls),
    AU_FRONT(20, 12, 610, 104, 20, 206, kReadOut, kJ37Controls),
    AU_FRONT(130, 60, 384, 102, 222, 8, kReadOut, kA77Controls),
    AU_FRONT(150, 14, 340, 116, 20, 204, kReadOut, kA80Controls),
};
inline constexpr AuFront kAuCassetteFronts[] = {
    AU_FRONT(30, 30, 180, 120, 30, 170, kReadOut, kEL3300Controls),
    AU_FRONT(232, 10, 400, 84, 16, 14, kReadOut, kPortaControls),
    AU_FRONT(30, 34, 150, 60, 260, 196, kReadIn, kD6Controls),
    AU_FRONT(200, 20, 340, 110, 24, 200, kReadOut, kD5Controls),
};
inline constexpr AuFront kAuCameraFronts[] = {
    AU_FRONT(380, 30, 200, 120, 20, 30, kReadOut, kAuriconControls),
    AU_FRONT(300, 60, 160, 120, 20, 30, kReadOut, kEktaControls),
    AU_FRONT(400, 30, 180, 100, 20, 30, kReadIn, kCanonControls),
    AU_FRONT(260, 60, 160, 120, 20, 30, kReadOut, kCamcorderControls),
    AU_FRONT(260, 60, 160, 120, 20, 30, kReadOut, kCamcorderControls),
};
inline constexpr AuFront kAuEarlyFronts[] = {
    AU_FRONT(30, 40, 230, 90, 30, 150, kReadOut, kEdisonControls),
    AU_FRONT(40, 40, 120, 120, 430, 196, kReadIn, kWebsterControls),
};
inline constexpr AuFront kAuReelSyncFronts[] = {
    AU_FRONT(24, 30, 190, 120, 24, 180, kReadIn, kEP6Controls),
};
inline constexpr const AuFront* kAuFamilyFronts[] = {
    kAuReelPortableFronts, kAuReelStudioFronts, kAuCassetteFronts, kAuCameraFronts, kAuEarlyFronts, kAuReelSyncFronts,
};
static_assert(std::size(kAuReelPortableFronts) == std::size(kAuReelPortable));
static_assert(std::size(kAuReelStudioFronts) == std::size(kAuReelStudio));
static_assert(std::size(kAuCassetteFronts) == std::size(kAuCassetteDecks));
static_assert(std::size(kAuCameraFronts) == std::size(kAuCameras));
static_assert(std::size(kAuEarlyFronts) == std::size(kAuEarly));
static_assert(std::size(kAuReelSyncFronts) == std::size(kAuReelSync));

// ================================================================== Banchi di missaggio

// No desk: the plugin's own controls.
inline constexpr AuControl kNoDeskControls[] = {
    auKnob(30, 40, 64, "SATURAZIONE", kPDrive, 0, 3, "0|1|2|3"),
    auKnob(120, 40, 64, "DRY/WET", kPMix, 0, 100, "0|50|100"),
    auKnob(210, 40, 64, "USCITA", kPOut, -24, 12, "−24|0|+12"),
};

// RCA BC-2B (instruction book IB-24748): eight mixer attenuators with their key switches (audition, off, program), the
// master gain, monitor gain, one 4-inch VU with the type B scale. The plugin's bus is mixer 1.
inline constexpr AuPos kRcaKey[] = { { "AUD", kRun }, { "", kStop }, { "PGM", kRun } };
#define AU_RCA_CHANNEL(x, label, use)                                                \
    auPick(AuWidget::Lever, x + 13, 140, 26, 30, "", kRcaKey, 2, use),               \
    auKnob(x + 2, 190, 48, label, kPRecLevel, -24, 12, "|", use, 0x1A1A1A)
inline constexpr AuControl kRcaControls[] = {
    AU_RCA_CHANNEL(16, "MIC 1", AuUse::Live), AU_RCA_CHANNEL(92, "MIC 2", AuUse::Idle),
    AU_RCA_CHANNEL(168, "MIC 3", AuUse::Idle), AU_RCA_CHANNEL(244, "MIC 4", AuUse::Idle),
    AU_RCA_CHANNEL(320, "TT 1", AuUse::Idle), AU_RCA_CHANNEL(396, "TT 2", AuUse::Idle),
    AU_RCA_CHANNEL(472, "NET", AuUse::Idle), AU_RCA_CHANNEL(548, "REM", AuUse::Idle),
    auKnob(462, 40, 50, "MON GAIN", -1, 0, 0, "|", AuUse::Mech, 0x1A1A1A),
    auKnob(548, 30, 64, "MASTER", kPOut, -24, 12, "|", AuUse::Live, 0x1A1A1A),
};
#undef AU_RCA_CHANNEL

// EMI REDD.51 (descriptions of the desk): fourteen Painton quadrant faders (eight microphones, two auxiliaries, four
// main), the channel's Pop/Classic equaliser (bass shelf at 100 Hz, treble at 10 kHz), four large VU meters.
inline constexpr AuPos kReddEq[] = { { "CLASSIC" }, { "POP" } };
#define AU_REDD_FADER(i, label, param, use) \
    auFader(AuWidget::Quadrant, 146 + 34 * i, 132, 32, 106, label, param, -24, 12, "", use)
inline constexpr AuControl kReddControls[] = {
    auKnob(22, 142, 40, "TREBLE", kPHigh, 0, 10, "0|10"),
    auKnob(84, 142, 40, "BASS", kPLow, 0, 10, "0|10"),
    auPick(AuWidget::Lever, 56, 208, 28, 30, "", kReddEq, 0, AuUse::Mech),
    AU_REDD_FADER(0, "1", kPRecLevel, AuUse::Live), AU_REDD_FADER(1, "2", kPRecLevel, AuUse::Idle),
    AU_REDD_FADER(2, "3", kPRecLevel, AuUse::Idle), AU_REDD_FADER(3, "4", kPRecLevel, AuUse::Idle),
    AU_REDD_FADER(4, "5", kPRecLevel, AuUse::Idle), AU_REDD_FADER(5, "6", kPRecLevel, AuUse::Idle),
    AU_REDD_FADER(6, "7", kPRecLevel, AuUse::Idle), AU_REDD_FADER(7, "8", kPRecLevel, AuUse::Idle),
    AU_REDD_FADER(8, "AUX", kPRecLevel, AuUse::Idle), AU_REDD_FADER(9, "AUX", kPRecLevel, AuUse::Idle),
    AU_REDD_FADER(10, "A", kPOut, AuUse::Live), AU_REDD_FADER(11, "B", kPOut, AuUse::Idle),
    AU_REDD_FADER(12, "C", kPOut, AuUse::Idle), AU_REDD_FADER(13, "D", kPOut, AuUse::Idle),
};
#undef AU_REDD_FADER

// Neve 8078: a channel of the console (stepped microphone gain, high-pass filter, high and low shelves), its fader and
// the two VU meters of the mix bus. Deduced from photographs of the 31105 module.
inline constexpr AuPos kNeveHp[] = {
    { "OFF", { kPLowCut, 0 } }, { "45", { kPLowCut, 45 } }, { "70", { kPLowCut, 70 } }, { "160", { kPLowCut, 160 } },
    { "360", { kPLowCut, 360 } },
};
inline constexpr AuControl kNeveControls[] = {
    auKnob(20, 26, 66, "MIC GAIN", kPRecLevel, -24, 12, "−70|−50|−30|−20", AuUse::Live, 0xA8302A),
    auPick(AuWidget::Selector, 110, 26, 70, 70, "HIGH PASS", kNeveHp, 0),
    auKnob(200, 26, 54, "HF", kPHigh, -15, 15, "−|+", AuUse::Live, 0x3A5A8C),
    auKnob(200, 110, 54, "LF", kPLow, -15, 15, "−|+", AuUse::Live, 0x3A5A8C),
    auKnob(110, 120, 50, "MF", -1, 0, 0, "−|+", AuUse::Mech, 0x3A5A8C),
    auFader(AuWidget::Fader, 286, 26, 28, 210, "", kPOut, -24, 12, "+10|0|−10|−20|−30|−40"),
};

// SSL 4000 E: a channel strip (input trim, filters, equaliser, dynamics), its fader, the VU meters. Deduced.
inline constexpr AuControl kSslControls[] = {
    auKnob(20, 26, 50, "TRIM", kPRecLevel, -24, 12, "−|+"),
    auKnob(84, 26, 50, "HPF", kPLowCut, 0, 350, "OUT|350"),
    auKnob(20, 110, 50, "HF", kPHigh, -15, 15, "−15|+15", AuUse::Live, 0xB8302A),
    auKnob(84, 110, 50, "LF", kPLow, -15, 15, "−15|+15", AuUse::Live, 0x6A4430),
    auKnob(150, 26, 50, "RATIO", -1, 0, 0, "1|∞", AuUse::Mech),
    auPick(AuWidget::Keys, 150, 116, 60, 26, "DYNAMICS", kLimiter, 0),
    auFader(AuWidget::Fader, 240, 26, 30, 210, "", kPOut, -24, 12, "+10|0|−10|−20|−30|−40"),
};

// Mackie CR-1604: a channel (trim, six aux sends, high, mid and low equaliser, pan, mute, solo, fader) and the
// twelve-step LED meters of the main mix.
inline constexpr AuPos kMute[] = { { "", kRun }, { "MUTE", kStop } };
inline constexpr AuPos kSolo[] = { { "" }, { "SOLO" } };
inline constexpr AuControl kMackieControls[] = {
    auKnob(20, 20, 40, "TRIM", kPRecLevel, -24, 12, "U|+60"),
    auKnob(70, 20, 30, "AUX 1", -1, 0, 0, "|", AuUse::Mech), auKnob(110, 20, 30, "AUX 2", -1, 0, 0, "|", AuUse::Mech),
    auKnob(70, 66, 30, "AUX 3", -1, 0, 0, "|", AuUse::Mech), auKnob(110, 66, 30, "AUX 4", -1, 0, 0, "|", AuUse::Mech),
    auKnob(150, 20, 30, "AUX 5", -1, 0, 0, "|", AuUse::Mech), auKnob(150, 66, 30, "AUX 6", -1, 0, 0, "|", AuUse::Mech),
    auKnob(20, 112, 40, "HI", kPHigh, -15, 15, "−|+", AuUse::Live, 0x2E8A4A),
    auKnob(70, 112, 40, "MID", -1, 0, 0, "−|+", AuUse::Mech, 0x2E8A4A),
    auKnob(120, 112, 40, "LOW", kPLow, -15, 15, "−|+", AuUse::Live, 0x2E8A4A),
    auKnob(20, 176, 40, "PAN", -1, 0, 0, "L|R", AuUse::Mech),
    auPick(AuWidget::Keys, 72, 184, 46, 24, "", kMute, 0),
    auPick(AuWidget::Keys, 124, 184, 46, 24, "", kSolo, 0, AuUse::Mech),
    auFader(AuWidget::Fader, 200, 20, 28, 216, "", kPOut, -24, 12, "+10|U|−10|−20|−30|−50"),
    auFader(AuWidget::Fader, 250, 20, 28, 216, "MAIN", kPOut, -24, 12, "+10|U|−10|−20|−30|−50"),
};

inline constexpr AuFront kAuDeskFronts[] = {
    AU_FRONT(300, 26, 320, 120, 30, 150, kReadOut, kNoDeskControls),
    AU_FRONT(232, 12, 210, 110, 16, 20, kReadOut, kRcaControls),
    AU_FRONT(232, 12, 400, 104, 16, 16, kReadOut, kReddControls),
    AU_FRONT(336, 26, 290, 120, 360, 180, kReadOut, kNeveControls),
    AU_FRONT(300, 26, 320, 120, 310, 170, kReadOut, kSslControls),
    AU_FRONT(320, 26, 300, 110, 320, 170, kReadOut, kMackieControls),
};
static_assert(std::size(kAuDeskFronts) == std::size(kAuConsoles));

inline const AuFront& auFront(const double* v, bool mix)
{
    if (mix) return kAuDeskFronts[std::clamp(auPlain(v, kPDesk), 0, int(std::size(kAuDeskFronts)) - 1)];
    int family, model;
    auDeviceSplit(auPlain(v, kPDevice), family, model);
    return kAuFamilyFronts[family][model];
}

// The parameter values a position stands for: each target with its plain value (kSetMedium and kSetNR turned into the
// menus' values: in Ripresa 0 is the recorder's own). Meter and tone targets are the panel's, left out.
inline std::vector<std::pair<AuParamId, double>> auPositionValues(const AuPos& p, const double* v, bool mix)
{
    std::vector<std::pair<AuParamId, double>> out;
    const AuDevice& dev = auDevice(v);
    for (const AuSet& s : { p.a, p.b, p.c, p.d }) {
        if (s.target >= 0) out.push_back({ AuParamId(s.target), s.value });
        else if (s.target == kSetMedium) out.push_back({ kPMedium, !mix && int(s.value) == dev.medium ? 0.0 : s.value });
        else if (s.target == kSetNR) out.push_back({ kPNR, mix ? s.value : int(s.value) == dev.nr ? 0.0 : s.value + 1 });
    }
    return out;
}
// What the meter reads in a position, -1 if it does not say.
inline int auPositionReads(const AuPos& p)
{
    for (const AuSet& s : { p.a, p.b, p.c, p.d })
        if (s.target == kSetMeter) return int(s.value);
    return -1;
}

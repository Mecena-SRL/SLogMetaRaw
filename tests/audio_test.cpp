// SPDX-License-Identifier: GPL-3.0-or-later
// Log MetaRaw Audio engine (audio/src): every preset renders finite and deterministic, switches really bypass, and
// each stage does what its table says; the menus offer only what fits together. Run by tests/test_audio.py; prints one line per failure, exit 1 on any.
#include "AuFronts.h"
#include "AuMeters.h"
#include "AuParams.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr double kFs = 48000;
int failures = 0;

void check(bool ok, const char* what, const char* name = "")
{
    if (!ok) {
        std::printf("FAIL %s %s\n", what, name);
        ++failures;
    }
}

using Buffer = std::vector<std::vector<float>>;

Buffer sine(double hz, float amp, int frames, int channels = 2)
{
    Buffer b(channels, std::vector<float>(frames));
    for (int c = 0; c < channels; ++c)
        for (int f = 0; f < frames; ++f) b[c][f] = amp * float(std::sin(2 * 3.14159265358979 * hz * f / kFs));
    return b;
}

// Runs the buffer through a fresh engine in host-like blocks of `block` frames.
Buffer render(const AuSettings& s, Buffer b, int block = 512)
{
    AuEngine e;
    e.prepare(kFs, int(b.size()), s);
    int frames = int(b[0].size());
    std::vector<float*> ptr(b.size());
    for (int f = 0; f < frames; f += block) {
        for (size_t c = 0; c < b.size(); ++c) ptr[c] = b[c].data() + f;
        e.process(ptr.data(), std::min(block, frames - f));
    }
    return b;
}

double rms(const std::vector<float>& x, int from = 0)
{
    double s = 0;
    for (size_t i = from; i < x.size(); ++i) s += double(x[i]) * x[i];
    return std::sqrt(s / double(x.size() - from));
}

// Steady-state gain in dB of a sine through the settings (noise and wow off).
double gainDb(AuSettings s, double hz)
{
    s.on[kStNoise] = s.on[kStWow] = false;
    Buffer y = render(s, sine(hz, 0.05f, 24000));
    return 20 * std::log10(rms(y[0], 12000) / (0.05 / std::sqrt(2.0)));
}

AuSettings bare()   // every stage off
{
    AuSettings s;
    for (bool& o : s.on) o = false;
    return s;
}

bool sane(const Buffer& b)
{
    for (auto& ch : b)
        for (float v : ch)
            if (!std::isfinite(v) || std::fabs(v) > 4) return false;
    return true;
}

}  // namespace

int main()
{
    // Tables: every device points at a real microphone and medium, and takes the medium it starts from.
    for (const AuFamily& fam : kAuFamilies)
        for (size_t i = 0; i < fam.count; ++i) {
            const AuDevice& d = fam.devices[i];
            check(d.mic > 0 && d.mic < int(std::size(kAuMics)), "device mic", d.name);
            check(d.medium > 0 && d.medium < int(std::size(kAuMedia)), "device medium", d.name);
            check((d.media >> d.medium) & 1, "device takes its medium", d.name);
            check(d.nr >= 0 && d.nr < int(std::size(kAuNR)), "device nr", d.name);
        }

    // Era setups (shared/EraSetups.h): every name exists, and what a setup sets survives the menus' coherence rules.
    for (int s = 1; s < kEraSetupCount; ++s)
        for (bool mix : { false, true }) {
            bool ok = false;
            const auto set = auSetupValues(s, mix, &ok);
            check(ok, "era setup names", kEraSetups[s].name);
            double v[kPCount];
            for (int i = 0; i < kPCount; ++i) v[i] = auDefault(AuParamId(i), mix);
            for (auto [id, plain] : set) v[id] = plain;
            auCoerce(v, mix);
            for (auto [id, plain] : set) check(v[id] == plain, "era setup coherent", kEraSetups[s].name);
        }
    {
        const auto take = auSetupValues(2, false), mix = auSetupValues(2, true);   // À bout de souffle
        check(!take.empty() && take[0].second == auDeviceNamed("Ampex 350"), "Breathless: post-sync in studio");
        check(mix.size() == 5 && mix[3].second == auRowNamed(kAuMedia, "Banda ottica 35 mm (Academy)"),
              "Breathless: optical print");
        check(auSetupValues(1, false).empty() && auSetupValues(1, true).empty(), "Lumière: silent");
    }

    // All switches off: the samples pass untouched (bit for bit).
    {
        Buffer in = sine(440, 0.5f, 4800);
        check(render(bare(), in) == in, "bypass");
    }

    // Every recorder, medium, desk and speaker renders finite, bounded and deterministic, whatever the block size.
    Buffer music = sine(220, 0.7f, 9600);
    for (int i = 0; i < 9600; ++i) music[1][i] = 0.5f * float(std::sin(i * 0.31)) * music[0][i];
    for (int fam = 0; fam < int(std::size(kAuFamilies)); ++fam)
        for (int m = 0; m < int(kAuFamilies[fam].count); ++m) {
            AuSettings s;
            s.family = fam;
            s.model = m;
            Buffer a = render(s, music), b = render(s, music, 97);
            check(sane(a), "device finite", kAuFamilies[fam].devices[m].name);
            check(a == b, "device deterministic", kAuFamilies[fam].devices[m].name);
        }
    for (int m = 1; m < int(std::size(kAuMedia)); ++m) {
        AuSettings s;
        s.useDevice = false;
        s.medium = m;
        s.transfer = m;
        s.copies = 2;
        Buffer a = render(s, music), b = render(s, music, 333);
        check(sane(a), "medium finite", kAuMedia[m].name);
        check(a == b, "medium deterministic", kAuMedia[m].name);
    }
    for (int d = 1; d < int(std::size(kAuConsoles)); ++d)
        for (int l = 1; l < int(std::size(kAuListen)); ++l) {
            AuSettings s;
            s.useDevice = false;
            s.medium = 0;
            s.desk = d;
            s.listen = l;
            check(sane(render(s, music)), "desk and speaker finite", kAuConsoles[d].name);
        }

    // A telephone line passes speech, not bass or air.
    {
        AuSettings s = bare();
        s.on[kStMedium] = true;
        s.useDevice = false;
        s.medium = 23;
        check(gainDb(s, 1000) > -6, "telephone 1 kHz");
        check(gainDb(s, 100) < -20, "telephone 100 Hz");
        check(gainDb(s, 9000) < -20, "telephone 9 kHz");
    }
    // A crystal microphone rings around 3 kHz.
    {
        AuSettings s = bare();
        s.on[kStMic] = true;
        s.mic = 3;
        check(gainDb(s, 3000) - gainDb(s, 600) > 4, "piezo resonance");
    }
    // Hiss: slower tape is noisier, Dolby takes some of it away.
    {
        Buffer silence(2, std::vector<float>(48000));
        auto noise = [&](int medium, int nr) {
            AuSettings s = bare();
            s.on[kStMedium] = s.on[kStNoise] = s.on[kStNR] = true;
            s.useDevice = false;
            s.medium = medium;
            s.nr = nr;
            return 20 * std::log10(rms(render(s, silence)[0]) + 1e-12);
        };
        double fast = noise(1, 0), slow = noise(4, 0), cassette = noise(5, 0), dolby = noise(5, 2);
        check(slow > fast + 12, "slow tape hisses more");
        check(cassette - dolby > 6, "Dolby B lowers hiss");
        check(slow < -30 && fast > -90, "hiss in range");
    }
    // Wow and flutter move the pitch: zero-crossing intervals spread.
    {
        auto spread = [&](bool wow) {
            AuSettings s = bare();
            s.on[kStMedium] = true;
            s.on[kStWow] = wow;
            s.useDevice = false;
            s.medium = 4;   // 4.75 cm/s
            Buffer y = render(s, sine(1000, 0.1f, 96000));
            std::vector<double> cross;
            for (int i = 4800; i < 96000; ++i)
                if (y[0][i - 1] < 0 && y[0][i] >= 0) cross.push_back(i - y[0][i] / (y[0][i] - y[0][i - 1]));
            double mean = (cross.back() - cross.front()) / (cross.size() - 1), var = 0;
            for (size_t i = 1; i < cross.size(); ++i) var += std::pow(cross[i] - cross[i - 1] - mean, 2);
            return std::sqrt(var / (cross.size() - 1)) / mean;
        };
        double still = spread(false), moving = spread(true);
        check(moving > 0.002 && moving > 5 * still, "wow moves the pitch");
    }
    // A mono medium folds the channels together.
    {
        AuSettings s = bare();
        s.on[kStMedium] = true;
        s.useDevice = false;
        s.medium = 18;
        Buffer in = sine(500, 0.3f, 4800);
        for (float& v : in[1]) v = 0;
        Buffer y = render(s, in);
        check(std::fabs(rms(y[0]) - rms(y[1])) < 1e-6, "optical track is mono");
    }
    // Every further copy loses more top end.
    {
        AuSettings s = bare();
        s.on[kStTransfer] = true;
        s.useDevice = false;
        s.transfer = 13;   // VHS SP
        s.copies = 1;
        double one = gainDb(s, 8000);
        s.copies = 3;
        check(gainDb(s, 8000) < one - 3, "copies lose treble");
    }
    // Dry/wet at zero gives the input back; the output trim is a plain gain.
    {
        AuSettings s;
        s.mix = 0;
        Buffer in = sine(300, 0.2f, 4800), y = render(s, in);
        double err = 0;
        for (int i = 0; i < 4800; ++i) err = std::max(err, double(std::fabs(y[0][i] - in[0][i])));
        check(err < 1e-6, "mix 0 is dry");
        AuSettings g = bare();
        g.outDb = -6;
        check(std::fabs(gainDb(g, 1000) + 6) < 0.05, "output trim");
    }

    // Preparing again mid-stream with the same chain shape continues the sound instead of restarting it.
    {
        AuSettings s;
        Buffer whole = render(s, music), split = music;
        AuEngine e;
        e.prepare(kFs, 2, s);
        float* half[2] = { split[0].data(), split[1].data() };
        e.process(half, 4800);
        e.prepare(kFs, 2, s);
        float* rest[2] = { split[0].data() + 4800, split[1].data() + 4800 };
        e.process(rest, 4800);
        check(split == whole, "prepare keeps the running state");
    }

    // What goes with what (AuParams.h): every recorder's own parts fit it, the menus never offer a mismatch, and a
    // changed parent drags its children back into range.
    {
        auto has = [](const std::vector<int>& list, int x) { return std::find(list.begin(), list.end(), x) != list.end(); };
        double v[kPCount];
        auto reset = [&](bool mix) {
            for (int i = 0; i < kPCount; ++i) v[i] = auDefault(AuParamId(i), mix);
        };
        for (int fam = 0; fam < int(std::size(kAuFamilies)); ++fam)
            for (int m = 0; m < int(kAuFamilies[fam].count); ++m) {
                const AuDevice& d = kAuFamilies[fam].devices[m];
                reset(false);
                v[kPDevice] = auDeviceIndex(fam, m);
                check(kAuMics[d.mic].year <= d.year + kAuDeviceLife, "device mic in its era", d.name);
                check(auNRFits(d.nr, d.medium, d.year + kAuDeviceLife), "device nr fits its medium", d.name);
                check(auCoerce(v, false).empty(), "device defaults fit", d.name);
                for (int med : auAllowed(kPMedium, v, false))
                    check(med == 0 || ((d.media >> med) & 1), "medium menu only the device's", d.name);
                for (int mic : auAllowed(kPMic, v, false))
                    check(mic == 0 || (mic != 1 && kAuMics[mic].year <= d.year + kAuDeviceLife), "mic menu in era", d.name);
            }
        reset(false);
        v[kPDevice] = auDeviceIndex(0, 1);   // Nagra III: reels only, never a VHS
        check(!has(auAllowed(kPMedium, v, false), 13), "no VHS in a Nagra");
        check(auAllowed(kPMic, v, false).size() > 3, "Nagra offers microphones");
        v[kPMedium] = 1;
        check(has(auAllowed(kPNR, v, false), 2) && !has(auAllowed(kPNR, v, false), 3), "reel takes Dolby A, not B");
        v[kPNR] = 2;   // Dolby A
        v[kPDevice] = auDeviceIndex(2, 2);   // → Walkman: medium and NR fall back to its own
        auto moved = auCoerce(v, false);
        check(v[kPMedium] == 0 && v[kPNR] == 0 && moved.size() == 2, "coerce on recorder change");
        v[kPDevice] = auDeviceIndex(4, 0);   // Edison: only its horn
        check(auAllowed(kPMic, v, false) == std::vector<int> { 0 }, "phonograph keeps its horn");

        reset(true);
        check(auCoerce(v, true).empty(), "mix defaults fit");
        v[kPDesk] = 1;   // RCA 1950: no VHS, no digital master
        check(!has(auAllowed(kPMedium, v, true), 13) && !has(auAllowed(kPMedium, v, true), 0), "1950 desk media");
        v[kPDesk] = 5;   // Mackie 1993: no wire, cylinder or 78
        for (int m : { 8, 9, 10 }) check(!has(auAllowed(kPMedium, v, true), m), "1993 desk media");
        v[kPMedium] = 13;   // VHS master: transfers onto what was still in use, no cylinder
        check(!has(auAllowed(kPTransfer, v, true), 9) && has(auAllowed(kPTransfer, v, true), 5), "transfer era");
        check(!has(auAllowed(kPNR, v, true), 1), "no Dolby A on VHS");
        v[kPDesk] = 1;
        auto fixed = auCoerce(v, true);
        check(!fixed.empty() && v[kPMedium] != 13, "coerce on desk change");
    }

    // The front panels' own controls: low cut, limiter, automatic level, tone.
    {
        AuSettings s = bare();
        s.useDevice = true;
        double flat = gainDb(s, 50);
        s.lowCutHz = 158;   // Nagra 4.2, HP 2: -20 dB at 50 Hz
        check(std::fabs(gainDb(s, 50) - flat + 20) < 1, "filter HP 2 is -20 dB at 50 Hz");
        check(gainDb(s, 1000) > -0.5, "filter leaves 1 kHz");
        s = bare();
        s.highDb = -12;
        check(gainDb(s, 15000) < -8 && gainDb(s, 500) > -1, "tone cuts the treble");
        s = bare();
        s.lowDb = 10;   // REDD.51 bass: +10 dB shelf at 100 Hz
        check(gainDb(s, 30) > 8 && gainDb(s, 2000) < 1, "bass shelf");
        AuSettings loud = bare();
        Buffer in = sine(1000, 0.9f, 24000);
        double open = rms(render(loud, in)[0], 12000);
        loud.limit = true;
        double limited = rms(render(loud, in)[0], 12000);
        loud.limit = false, loud.alc = true;
        double riding = rms(render(loud, in)[0], 12000);
        check(limited < open * 0.7 && riding < limited, "limiter and automatic level lower a loud tone");
    }

    // Meter ballistics (AuMeters.h) against their standards.
    {
        auto reading = [](AuMeter m, const std::vector<float>& x, int upTo) {
            AuBallistics b;
            b.prepare(kFs, m);
            b.run(x.data(), upTo);
            return b.db();
        };
        const float a = float(std::sqrt(2.0));   // a sine of RMS 1 reads 0 dB on every meter
        std::vector<float> tone = sine(1000, a, int(kFs * 3), 1)[0];
        check(std::fabs(reading(AuMeter::VU, tone, int(kFs * 2.5))) < 0.15, "VU reads a sine at its RMS");
        check(std::fabs(reading(AuMeter::PPM, tone, int(kFs * 1))) < 0.1, "PPM reads a sine at its RMS");
        double peak = -100, at99 = -1;   // VU: 99 % (−0.087 dB) at 300 ms, overshoot 1 to 1.5 %
        for (int ms = 10; ms <= 1500; ms += 5) {
            const double db = reading(AuMeter::VU, tone, int(kFs * ms / 1000));
            peak = std::max(peak, db);
            if (at99 < 0 && db >= 20 * std::log10(0.99)) at99 = ms;
        }
        check(at99 >= 270 && at99 <= 330, "VU 99 % in 300 ms");
        check(peak > 20 * std::log10(1.005) && peak < 20 * std::log10(1.02), "VU overshoot about 1.5 %");
        std::vector<float> burst = sine(5000, a, int(kFs * 0.01), 1)[0];   // PPM IIa: a 10 ms burst of 5 kHz, 2 dB low
        check(std::fabs(reading(AuMeter::PPM, burst, int(kFs * 0.01)) + 2) < 0.3, "PPM 10 ms burst -2 dB");
        std::vector<float> fall = tone;   // and falls 24 dB in 2.8 s
        std::fill(fall.begin() + int(kFs), fall.end(), 0.0f);
        fall.resize(size_t(kFs * 3.8));
        check(std::fabs(reading(AuMeter::PPM, fall, int(kFs * 3.8)) + 24) < 1, "PPM falls 24 dB in 2.8 s");
    }

    // Every front fits its machine: positions name media and noise reductions the machine takes, controls drive
    // parameters the plugin has.
    {
        double v[kPCount];
        for (int fam = 0; fam < int(std::size(kAuFamilies)); ++fam)
            for (int m = 0; m < int(kAuFamilies[fam].count); ++m) {
                const AuDevice& d = kAuFamilies[fam].devices[m];
                const AuFront& f = kAuFamilyFronts[fam][m];
                for (int i = 0; i < kPCount; ++i) v[i] = auDefault(AuParamId(i), false);
                v[kPDevice] = auDeviceIndex(fam, m);
                auto params = auParams(false);
                for (int c = 0; c < f.count; ++c) {
                    const AuControl& k = f.controls[c];
                    check(k.pos || k.param < 0 || std::find(params.begin(), params.end(), k.param) != params.end(),
                          "front pot drives a parameter", d.name);
                    for (int p = 0; p < k.count; ++p)
                        for (const AuSet& set : { k.pos[p].a, k.pos[p].b, k.pos[p].c, k.pos[p].d }) {
                            if (set.target == kSetMedium) check((d.media >> int(set.value)) & 1, "front speed fits", d.name);
                            if (set.target == kSetNR) check(auNRFits(int(set.value), d.medium, d.year + kAuDeviceLife), "front NR fits", d.name);
                            if (set.target >= 0)
                                check(std::find(params.begin(), params.end(), set.target) != params.end(), "front switch param", d.name);
                        }
                    check(!k.pos || (k.def >= 0 && k.def < k.count), "front default position", d.name);
                }
            }
        auto params = auParams(true);
        for (const AuFront& f : kAuDeskFronts)
            for (int c = 0; c < f.count; ++c) {
                const AuControl& k = f.controls[c];
                check(k.pos || k.param < 0 || std::find(params.begin(), params.end(), k.param) != params.end(), "desk pot");
                for (int p = 0; p < k.count; ++p)
                    for (const AuSet& set : { k.pos[p].a, k.pos[p].b, k.pos[p].c, k.pos[p].d })
                        if (set.target >= 0) check(std::find(params.begin(), params.end(), set.target) != params.end(), "desk switch");
            }
    }


    // The record level drives the medium against its hiss and is given back on playback: loudness stays, the hiss
    // falls relative to the signal, saturation grows.
    {
        auto take = [&](float rec, bool tone) {
            AuSettings s = bare();
            s.on[kStMedium] = s.on[kStNoise] = true;
            s.useDevice = false;
            s.medium = 5;   // cassette type I
            s.recDb = rec;
            Buffer in = tone ? sine(1000, 0.05f, 48000) : Buffer(2, std::vector<float>(48000));
            return 20 * std::log10(rms(render(s, in)[0], 12000) + 1e-12);
        };
        check(std::fabs(take(-12, true) - take(6, true)) < 1, "record level keeps the loudness");
        check(take(-12, false) > take(6, false) + 15, "low record level: more hiss");
        AuSettings s = bare();
        s.on[kStMedium] = true;
        s.useDevice = false;
        s.medium = 5;
        s.recDb = 12;
        Buffer hot = render(s, sine(1000, 0.5f, 24000));
        s.recDb = -12;
        Buffer cold = render(s, sine(1000, 0.5f, 24000));
        check(rms(hot[0], 12000) < rms(cold[0], 12000) * 0.9, "high record level saturates");
    }
    // Automation: a level, the output and dry/wet glide to a new value instead of stepping.
    {
        AuSettings s = bare();
        Buffer x(2, std::vector<float>(9600, 0.5f));
        AuEngine e;
        e.prepare(kFs, 2, s);
        float* a[2] = { x[0].data(), x[1].data() };
        e.process(a, 4800);
        s.outDb = -12;
        s.recDb = 6;
        s.mix = 0.5f;
        e.prepare(kFs, 2, s);
        float* b[2] = { x[0].data() + 4800, x[1].data() + 4800 };
        e.process(b, 4800);
        double step = 0;
        for (int i = 1; i < 9600; ++i) step = std::max(step, double(std::fabs(x[0][i] - x[0][i - 1])));
        check(step < 0.01 && std::fabs(x[0][9599] - 0.5 * std::pow(10, -12 / 20.0)) < 1e-3, "automation glides");
    }
    // The guide: every source and programme points at machines that exist, microphones that exist, a target per
    // meter; the level it suggests brings the measured peaks to the target.
    {
        for (const AuSource& src : kAuSources) {
            for (int m : src.mics) {
                if (m < 0) break;
                check(m > 0 && m < int(std::size(kAuMics)), "guide mic", src.name);
            }
            for (const char* r : src.recorders)
                if (r) check(auDeviceNamed(r) >= 0, "guide recorder", r);
        }
        for (const AuProgram& p : kAuPrograms) {
            for (const char* d : p.desks)
                if (d) check(auDeskNamed(d) >= 0, "guide desk", d);
            for (const char* r : p.recorders)
                if (r) check(auDeviceNamed(r) >= 0, "guide recorder", r);
        }
        double v[kPCount];
        for (int i = 0; i < kPCount; ++i) v[i] = auDefault(AuParamId(i), false);
        v[kPDevice] = auDeviceIndex(0, 3);   // Nagra 4.2: modulometer
        v[kPSource] = 1;                     // speech: peaks at -2 dB on the modulometer
        check(auGuideLevel(v, false, -14) == 12, "guide level");
        check(auGuideMic(v, 1) == 0, "guide mic for speech on a Nagra 4.2: its own shotgun");
        check(!auGuideText(v, false, -14).empty(), "guide text");
        v[kPSource] = 1;   // Cinema · Presa diretta: mono, limiter, 19 cm/s (the Nagra's own), the shotgun
        for (auto [id, plain] : auPresetValues(v, false)) v[id] = plain;
        check(v[kPInput] == 1 && v[kPLimit] == 1 && v[kPMedium] == 0 && v[kPMic] == 0 && v[kPLowCut] == 80,
              "preset of use sets the Nagra for cinema");
        v[kPSource] = 10;   // Sala di registrazione · Voce: the quietest speed
        for (auto [id, plain] : auPresetValues(v, false)) v[id] = plain;
        check(v[kPMedium] == 1, "music preset takes 38 cm/s");
        v[kPDevice] = auDeviceIndex(1, 0);   // Ampex 350: a U 47 on the cardioid position
        v[kPSource] = 10;
        for (auto [id, plain] : auPresetValues(v, false)) v[id] = plain;
        check(v[kPMic] == 0 && v[kPMicPattern] == 1, "voice preset: the Ampex's own U 47, cardioid");
        check(auEntryName(kPMicPattern, 2, v, false) == "Omnidirezionale", "U 47 pattern names");
        check(std::string(kAuSources[10].name) == "Voce" && auListNames(kPSource, false)[10] == "Sala di registrazione · Voce",
              "presets by category");
        v[kPDevice] = auDeviceIndex(4, 0);   // phonograph: the horn only
        check(auGuideMic(v, 2) == -1 || auGuideMic(v, 2) == 0, "guide keeps the horn");
        check(auMeterReading(AuMeter::PPM, 8) == "PPM 6", "PPM marks");
        for (int i = 0; i < kPCount; ++i) v[i] = auDefault(AuParamId(i), true);
        v[kPSource] = 4;
        check(!auGuideText(v, true, kAuNoMeasure).empty(), "mix guide text");
    }


    // The desk channel: a mono input sums the sides, the pan places it at constant power, the fader is a gain; the
    // channel's desk colours the track in Ripresa.
    {
        AuSettings s = bare();
        Buffer in = sine(1000, 0.2f, 24000);
        for (float& x : in[1]) x = 0;
        s.input = 1;
        Buffer y = render(s, in);
        check(std::fabs(rms(y[0]) - rms(y[1])) < 1e-6 && std::fabs(rms(y[0]) - 0.1 / std::sqrt(2.0)) < 1e-3, "mono sums the sides");
        s.input = 2;
        s.pan = -1;
        y = render(s, in);
        check(rms(y[1], 12000) < 1e-6 && std::fabs(20 * std::log10(rms(y[0], 12000) / (0.2 / std::sqrt(2.0))) - 3.01) < 0.05,
              "pan hard left, +3 dB at the side");
        s.pan = 0;
        s.faderDb = -6;
        check(std::fabs(gainDb(s, 1000) + 6) < 0.05, "channel fader");
        check(auDefault(kPDesk, false) == 0 && auDefault(kPInput, false) == 0, "a Ripresa channel starts plain");
        AuSettings r;   // Ripresa with a Neve channel before the recorder
        r.desk = 3;
        Buffer m = render(r, sine(220, 0.5f, 9600));
        check(sane(m) && m != render(AuSettings {}, sine(220, 0.5f, 9600)), "channel desk colours the track");
    }


    // The microphone's switches, the medium's standard equalisation and its wear.
    {
        AuSettings s = bare();
        s.on[kStMic] = true;
        s.mic = 10;   // MD 421
        s.micFilter = 0;
        const double m = gainDb(s, 60);
        s.micFilter = 4;   // S
        check(gainDb(s, 60) < m - 8, "MD 421 on S cuts the bass");
        s.mic = 7;   // U 47: omni has no proximity lift
        s.micPattern = 0;
        const double cardioid = gainDb(s, 60);
        s.micPattern = 1;
        check(gainDb(s, 60) < cardioid - 1, "U 47 omni: no proximity effect");

        AuSettings t = bare();
        t.on[kStMedium] = true;
        t.useDevice = false;
        t.medium = 5;   // cassette type I, 120 µs
        auto level = [&](double hz, float amp) {
            AuSettings u = t;
            u.on[kStNoise] = u.on[kStWow] = false;
            Buffer y = render(u, sine(hz, amp, 24000));
            return 20 * std::log10(rms(y[0], 12000) / (amp / std::sqrt(2.0)));
        };
        const double lowHf = level(8000, 0.02f), hotHf = level(8000, 0.6f), lowMid = level(1000, 0.02f),
                     hotMid = level(1000, 0.6f);
        check(lowHf - hotHf > (lowMid - hotMid) + 1.5, "pre-emphasis: the highs saturate first");

        auto worn = [&](float age, bool tone) {
            AuSettings u = t;
            u.on[kStNoise] = true;
            u.on[kStWow] = false;
            u.age = age;
            Buffer in = tone ? sine(9000, 0.05f, 48000) : Buffer(2, std::vector<float>(48000));
            return 20 * std::log10(rms(render(u, in)[0], 12000) + 1e-12);
        };
        check(worn(1, false) > worn(0, false) + 5, "worn medium hisses more");
        check(worn(1, true) < worn(0, true) - 3, "worn medium is duller");
    }


    // The acquisition: every player and converter renders finite; a cheap one clips, has its bits, loses the highs.
    {
        for (int k = 1; k < int(std::size(kAuCaptures)); ++k) {
            AuSettings s;
            s.useDevice = false;
            s.capture = k;
            check(sane(render(s, music)) && render(s, music) == render(s, music, 211), "capture finite and deterministic", kAuCaptures[k].name);
        }
        AuSettings s = bare();
        s.useDevice = false;
        s.capture = 7;   // 8-bit recorder: clips at -1 dBFS, 8 bits
        s.on[kStNoise] = false;
        Buffer y = render(s, sine(500, 1.2f, 4800));
        float top = 0;
        for (float x : y[0]) top = std::max(top, std::fabs(x));
        check(top <= std::pow(10.0f, -1 / 20.0f) + 1e-6f, "capture clips");
        s.capture = 2;   // VHS + USB stick
        check(gainDb(s, 14000) < gainDb(s, 1000) - 3, "capture loses the highs");
    }

    if (!failures) std::printf("audio_test OK\n");
    return failures ? 1 : 0;
}

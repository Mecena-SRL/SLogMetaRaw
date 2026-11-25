// SPDX-License-Identifier: GPL-3.0-or-later
// Log MetaRaw Audio engine (audio/src): every preset renders finite and deterministic, switches really bypass, and
// each stage does what its table says; the menus offer only what fits together. Run by tests/test_audio.py; prints one line per failure, exit 1 on any.
#include "AuParams.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

    if (!failures) std::printf("audio_test OK\n");
    return failures ? 1 : 0;
}

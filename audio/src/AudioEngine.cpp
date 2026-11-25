// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioEngine.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kQ = 0.70710678118654752;   // Butterworth
constexpr float kSilent = -199;   // dBFS at or below: the noise is absent

float dbToGain(float db) { return db <= kSilent ? 0.0f : std::pow(10.0f, db / 20.0f); }

template <class T> const T& pick(const T* table, size_t count, int i)
{
    return table[std::clamp(i, 0, int(count) - 1)];
}

// RBJ cookbook biquads, normalised by a0.
using Biquad = AuEngine::Biquad;
Biquad normalise(double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { float(b0 / a0), float(b1 / a0), float(b2 / a0), float(a1 / a0), float(a2 / a0) };
}
Biquad lowpass(double fs, double f, double q)
{
    double w = 2 * kPi * f / fs, c = std::cos(w), a = std::sin(w) / (2 * q);
    return normalise((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
}
Biquad highpass(double fs, double f, double q)
{
    double w = 2 * kPi * f / fs, c = std::cos(w), a = std::sin(w) / (2 * q);
    return normalise((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
}
Biquad peak(double fs, double f, double db, double q)
{
    double A = std::pow(10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos(w), a = std::sin(w) / (2 * q);
    return normalise(1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
}
Biquad shelf(double fs, double f, double db, bool high)
{
    double A = std::pow(10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos(w), s = std::sin(w);
    double a = s / std::sqrt(2.0) * std::sqrt(A), sg = high ? 1 : -1;   // slope 1
    double b0 = A * ((A + 1) + sg * (A - 1) * c + 2 * std::sqrt(A) * a);
    double b1 = -2 * sg * A * ((A - 1) + sg * (A + 1) * c);
    double b2 = A * ((A + 1) + sg * (A - 1) * c - 2 * std::sqrt(A) * a);
    double a0 = (A + 1) - sg * (A - 1) * c + 2 * std::sqrt(A) * a;
    double a1 = 2 * sg * ((A - 1) - sg * (A + 1) * c);
    double a2 = (A + 1) - sg * (A - 1) * c - 2 * std::sqrt(A) * a;
    return normalise(b0, b1, b2, a0, a1, a2);
}

// Band edges (0 = open), `order` cascaded Butterworth sections each; edges past the sample rate's reach are dropped.
void band(std::vector<Biquad>& eq, double fs, float hp, float lp, int order)
{
    for (int i = 0; i < order; ++i) {
        if (hp > 0) eq.push_back(highpass(fs, hp, kQ));
        if (lp > 0 && lp < 0.45 * fs) eq.push_back(lowpass(fs, lp, kQ));
    }
}
void bell(std::vector<Biquad>& eq, double fs, float hz, float db, float q)
{
    if (db != 0 && hz < 0.45 * fs) eq.push_back(peak(fs, hz, db, q));
}

// Hiss: uniform noise through a low-pass at `lp`, scaled so `db` is its RMS level after the filter.
void hiss(AuEngine::Biquad& filter, float& level, double fs, float lp, float db)
{
    double edge = std::min(0.45 * fs, double(lp > 0 ? lp : 20000));
    filter = lowpass(fs, edge, kQ);
    level = dbToGain(db) * float(std::sqrt(3 * 0.5 * fs / (1.11 * edge)));   // 1.11: the filter's noise bandwidth
}

}  // namespace

float AuEngine::Block::random()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return float(rng) * (2.0f / 4294967295.0f) - 1.0f;
}

void AuEngine::add(Block b)
{
    if (!b.eq.empty() || b.drive > 0 || b.comp > 0 || b.mono || b.wowA > 0 || b.flutA > 0 || b.hiss > 0 ||
        b.hum > 0 || b.rumble > 0 || b.crackle > 0 || b.dropout > 0)
        blocks_.push_back(std::move(b));
}

void AuEngine::prepare(double sampleRate, int channels, const AuSettings& s)
{
    // A knob turned while playing keeps the sound running: blocks of the same shape keep their state.
    const bool same = sampleRate == fs_ && channels == channels_ && (s.seed ? s.seed : 1) == seed_;
    std::vector<Block> old;
    old.swap(blocks_);
    fs_ = sampleRate;
    channels_ = channels;
    seed_ = s.seed ? s.seed : 1;   // after `same`: compared with the previous seed
    wet_ = std::clamp(s.mix, 0.0f, 1.0f);
    out_ = dbToGain(s.outDb);
    const double fs = fs_;
    const float drive = std::max(0.0f, s.drive);
    const bool noise = s.on[kStNoise];

    const AuFamily& fam = pick(kAuFamilies, std::size(kAuFamilies), s.family);
    const AuDevice& dev = pick(fam.devices, fam.count, s.model);

    if (s.useDevice && s.on[kStMic]) {
        const AuMic& m = pick(kAuMics, std::size(kAuMics), s.mic > 0 ? s.mic : dev.mic);
        Block b;
        band(b.eq, fs, m.hp, m.lp, 1);
        bell(b.eq, fs, m.peakHz, m.peakDb, m.peakQ);
        if (m.prox != 0) b.eq.push_back(shelf(fs, 150, m.prox, false));
        b.drive = m.drive * drive;
        add(std::move(b));
    }
    if (s.useDevice && s.on[kStPre]) {
        Block b;
        b.drive = dev.preDrive * drive;
        b.asym = dev.preAsym;
        if (b.asym > 0) b.eq.push_back(highpass(fs, 10, kQ));   // the valves' DC
        if (noise) b.hum = dbToGain(dev.humDb + s.noiseDb);
        add(std::move(b));
    }
    if (s.on[kStDesk] && s.desk > 0) {
        const AuConsole& c = pick(kAuConsoles, std::size(kAuConsoles), s.desk);
        Block b;
        band(b.eq, fs, c.hp, c.lp, 1);
        if (c.lowDb != 0) b.eq.push_back(shelf(fs, 100, c.lowDb, false));
        if (c.highDb != 0 && 8000 < 0.45 * fs) b.eq.push_back(shelf(fs, 8000, c.highDb, true));
        b.drive = c.drive * drive;
        b.asym = c.asym;
        b.comp = c.comp;
        if (noise) hiss(b.hissLp, b.hiss, fs, c.lp, c.noiseDb + s.noiseDb);
        add(std::move(b));
    }

    auto medium = [&](int index, float hissCut) {
        const AuMedium& m = pick(kAuMedia, std::size(kAuMedia), index);
        Block b;
        b.drive = m.drive * drive;
        b.asym = m.asym;
        b.comp = m.comp;
        b.mono = m.mono;
        band(b.eq, fs, m.hp, m.lp, 2);
        bell(b.eq, fs, m.bumpHz, m.bumpDb, 1);
        if (s.on[kStWow]) {
            float w = std::max(0.0f, s.wow);
            b.wowW = float(2 * kPi * m.wowHz);
            b.flutW = float(2 * kPi * m.flutterHz);
            // a delay swinging by A at ω changes the speed by A·ω: the peak error in % sets A
            b.wowA = m.wow * 0.01f * w / b.wowW;
            b.flutA = m.flutter * 0.01f * w / b.flutW;
        }
        if (noise) {
            hiss(b.hissLp, b.hiss, fs, m.lp, m.hissDb + s.noiseDb - hissCut);
            b.hum = dbToGain(m.humDb + s.noiseDb);
            b.rumble = dbToGain(m.rumbleDb + s.noiseDb);
            b.crackle = float(m.crackle / fs);
            b.dropout = float(m.dropout / fs);
        }
        add(std::move(b));
    };
    int med = s.medium >= 0 ? s.medium : (s.useDevice ? dev.medium : 0);
    int nr = s.nr >= 0 ? s.nr : (s.useDevice ? dev.nr : 0);
    float cut = s.on[kStNR] ? pick(kAuNR, std::size(kAuNR), nr).hissCutDb : 0;
    if (s.on[kStMedium] && med > 0) medium(med, cut);
    if (s.on[kStTransfer] && s.transfer > 0)
        for (int i = 0; i < std::clamp(s.copies, 1, 8); ++i) medium(s.transfer, 0);

    if (s.on[kStListen] && s.listen > 0) {
        const AuListen& l = pick(kAuListen, std::size(kAuListen), s.listen);
        Block b;
        band(b.eq, fs, l.hp, l.lp, 1);
        bell(b.eq, fs, l.peakHz, l.peakDb, l.peakQ);
        b.drive = l.drive * drive;
        b.mono = l.mono;
        add(std::move(b));
    }
    for (size_t i = 0; i < blocks_.size(); ++i) {
        Block& b = blocks_[i];
        if (same && i < old.size() && old[i].eq.size() == b.eq.size() &&
            old[i].line.empty() == !(b.wowA > 0 || b.flutA > 0)) {
            const Block& o = old[i];
            b.z = o.z, b.env = o.env, b.line = o.line, b.tail = o.tail, b.hissZ = o.hissZ;
            b.pos = o.pos, b.rumbleZ = o.rumbleZ, b.drop = o.drop, b.dropTo = o.dropTo, b.dropLeft = o.dropLeft;
            b.rng = o.rng;
        } else {
            fresh(b, i);
        }
    }
}

void AuEngine::fresh(Block& b, size_t index) const
{
    const int n = channels_;
    size_t line = 16;
    while (line < fs_ * 0.05) line <<= 1;   // 50 ms covers the widest wow
    b.z.assign(b.eq.size() * 2 * n, 0);
    b.env.assign(n, 0);
    b.line.assign(b.wowA > 0 || b.flutA > 0 ? line * n : 0, 0);
    b.tail.assign(n, 0);
    b.hissZ.assign(2 * n, 0);
    b.pos = 0;
    b.rumbleZ = 0;
    b.drop = b.dropTo = 1;
    b.dropLeft = 0;
    b.rng = seed_ * 2654435761u + uint32_t(index + 1) * 40503u;
    if (!b.rng) b.rng = 1;
}

void AuEngine::reset()
{
    for (size_t i = 0; i < blocks_.size(); ++i) fresh(blocks_[i], i);
}

void AuEngine::run(Block& b, float* const* ch, int frames)
{
    const int n = channels_;
    const float att = 1 - std::exp(-1 / float(0.005 * fs_)), rel = 1 - std::exp(-1 / float(0.15 * fs_));
    const float tailK = std::exp(-1 / float(0.0004 * fs_)), dropK = 1 - std::exp(-1 / float(0.003 * fs_));
    const float rumbleK = 1 - std::exp(float(-2 * kPi * 25 / fs_));
    const float rumbleNorm = std::sqrt(3 * (2 - rumbleK) / rumbleK);   // unit RMS after the one-pole
    const size_t mask = b.line.empty() ? 0 : b.line.size() / n - 1;
    const float base = b.wowA + b.flutA + float(2 / fs_);   // the centre of the swing, never negative
    const size_t nq = b.eq.size();
    const float g = b.drive;

    for (int f = 0; f < frames; ++f) {
        // what the medium shares across channels: speed, mains, rumble, clicks, dropouts
        const uint64_t w = b.pos++;
        const double t = double(w) / fs_;
        float delay = 0, hum = 0, rumble = 0, click = 0;
        if (mask) {
            double fl = b.flutW * t;
            delay = base + b.wowA * float(std::sin(b.wowW * t)) + b.flutA * float(std::sin(fl + 0.7 * std::sin(0.37 * fl)));
        }
        if (b.hum > 0) {
            double w = 2 * kPi * 50 * t;
            hum = b.hum * float(std::sin(w) + 0.5 * std::sin(3 * w) + 0.25 * std::sin(5 * w));
        }
        if (b.rumble > 0) {   // turntable rumble: white noise through a 25 Hz one-pole, the same on both sides
            b.rumbleZ += rumbleK * (b.random() - b.rumbleZ);
            rumble = b.rumble * rumbleNorm * b.rumbleZ;
        }
        if (b.crackle > 0 && b.random() * 0.5f + 0.5f < b.crackle) click = 0.3f * b.random();
        if (b.dropout > 0) {
            if (b.dropLeft > 0 && --b.dropLeft == 0) b.dropTo = 1;
            else if (b.dropLeft == 0 && b.random() * 0.5f + 0.5f < b.dropout) {
                b.dropLeft = int(fs_ * (0.09 + 0.06 * b.random()));
                b.dropTo = 0.42f + 0.18f * b.random();
            }
            b.drop += dropK * (b.dropTo - b.drop);
        }
        if (b.mono && n > 1) {
            float sum = 0;
            for (int c = 0; c < n; ++c) sum += ch[c][f];
            for (int c = 0; c < n; ++c) ch[c][f] = sum / n;
        }
        for (int c = 0; c < n; ++c) {
            float x = ch[c][f];
            if (b.comp > 0) {   // a level rider: 18 dB threshold, ratio up to 5:1, 5 ms / 150 ms
                float& e = b.env[c];
                float p = x * x;
                e += (p > e ? att : rel) * (p - e);
                float lvl = 10 * std::log10(e + 1e-12f), over = lvl + 18;
                if (over > 0) x *= std::pow(10.0f, -over * (1 - 1 / (1 + 4 * b.comp)) / 20);
            }
            if (g > 1e-3f) x = std::tanh(g * (x + b.asym * x * x)) / g;
            float* z = &b.z[c * nq * 2];
            for (size_t i = 0; i < nq; ++i) x = b.eq[i].run(x, z[2 * i], z[2 * i + 1]);
            if (mask) {
                float* line = &b.line[c * (mask + 1)];
                line[w & mask] = x;
                double rd = double(w) - double(delay) * fs_;
                size_t i0 = size_t(std::floor(rd));
                float fr = float(rd - std::floor(rd));
                x = line[i0 & mask] * (1 - fr) + line[(i0 + 1) & mask] * fr;
            }
            if (b.hiss > 0) x += b.hiss * b.hissLp.run(b.random(), b.hissZ[2 * c], b.hissZ[2 * c + 1]);
            float& tl = b.tail[c];
            tl = tl * tailK + click * (c ? 0.8f : 1.0f);
            x = (x + hum + rumble + tl) * b.drop;
            ch[c][f] = x;
        }
    }
}

void AuEngine::process(float* const* ch, int frames)
{
    const int n = channels_;
    if (wet_ < 1) {
        dry_.resize(size_t(n) * frames);
        for (int c = 0; c < n; ++c) std::copy(ch[c], ch[c] + frames, dry_.begin() + size_t(c) * frames);
    }
    for (Block& b : blocks_) run(b, ch, frames);
    for (int c = 0; c < n; ++c)
        for (int f = 0; f < frames; ++f) {
            float y = ch[c][f];
            if (wet_ < 1) y = dry_[size_t(c) * frames + f] + wet_ * (y - dry_[size_t(c) * frames + f]);
            ch[c][f] = y * out_;
        }
}

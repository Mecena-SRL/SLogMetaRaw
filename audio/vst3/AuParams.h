// SPDX-License-Identifier: GPL-3.0-or-later
// The two plugins' controls as data: one table says what each parameter is, one per plugin says which ones it shows
// and in which rows of the panel. Parameter ids are permanent (hosts save automation by id): append only.
#pragma once
#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

enum AuParamId : int {
    kPBypass, kPDevice, kPMic, kPMedium, kPNR, kPDesk, kPTransfer, kPCopies, kPListen,
    kPOnMic, kPOnPre, kPOnDesk, kPOnMedium, kPOnWow, kPOnNoise, kPOnNR, kPOnTransfer, kPOnListen,
    kPDrive, kPNoise, kPWow, kPMix, kPOut, kPMeterRef, kPMeterL, kPMeterR, kPCount
};

enum class AuKind { List, Toggle, Range, Meter };   // Meter: read-only level the processor reports to the panel

struct AuParamDef
{
    AuParamId id;
    const char* name;
    AuKind kind;
    double min = 0, max = 1, def = 0;   // plain values; for a list, the default index
    const char* unit = "";
};

// The recorders as one list, grouped by family: «Famiglia · Modello».
inline std::vector<std::string> auDeviceNames()
{
    std::vector<std::string> out;
    for (const AuFamily& f : kAuFamilies)
        for (size_t i = 0; i < f.count; ++i) out.push_back(std::string(f.name) + " · " + f.devices[i].name);
    return out;
}
inline int auDeviceIndex(int family, int model)   // flat list index of (family, model)
{
    int flat = 0;
    for (int f = 0; f < int(std::size(kAuFamilies)); ++f)
        for (int m = 0; m < int(kAuFamilies[f].count); ++m, ++flat)
            if (f == family && m == model) return flat;
    return 0;
}
inline void auDeviceSplit(int flat, int& family, int& model)
{
    family = model = 0;
    for (int f = 0; f < int(std::size(kAuFamilies)); ++f) {
        if (flat < int(kAuFamilies[f].count)) {
            family = f, model = flat;
            return;
        }
        flat -= int(kAuFamilies[f].count);
    }
}

template <class T, size_t N> std::vector<std::string> auNames(const T (&table)[N], size_t from = 0,
                                                              const char* first = nullptr)
{
    std::vector<std::string> out;
    if (first) out.emplace_back(first);
    for (size_t i = from; i < N; ++i) out.emplace_back(table[i].name);
    return out;
}

// The entries of a list parameter, for one plugin. In Ripresa microphone, medium and noise reduction start with
// «Di serie / Del registratore» (the recorder's own); in Mix e supporto they are the tables as they are.
inline std::vector<std::string> auListNames(AuParamId id, bool mix)
{
    switch (id) {
    case kPDevice: return auDeviceNames();
    case kPMic: return auNames(kAuMics);
    case kPMedium: return mix ? auNames(kAuMedia) : auNames(kAuMedia, 1, "Del registratore");
    case kPNR: return mix ? auNames(kAuNR) : auNames(kAuNR, 0, "Del registratore");
    case kPDesk: return auNames(kAuConsoles);
    case kPTransfer: return auNames(kAuMedia, 1, "Nessuno");
    case kPListen: return auNames(kAuListen);
    default: return {};
    }
}

inline const AuParamDef& auParam(AuParamId id)
{
    static const AuParamDef defs[kPCount] = {
        { kPBypass, "Bypass", AuKind::Toggle },
        { kPDevice, "Registratore", AuKind::List },
        { kPMic, "Microfono", AuKind::List },
        { kPMedium, "Supporto", AuKind::List },
        { kPNR, "Riduzione rumore", AuKind::List },
        { kPDesk, "Banco", AuKind::List, 0, 1, 3 },
        { kPTransfer, "Riversaggio su", AuKind::List },
        { kPCopies, "Copie", AuKind::Range, 1, 8, 1 },
        { kPListen, "Ascolto", AuKind::List },
        { kPOnMic, "Microfono attivo", AuKind::Toggle, 0, 1, 1 },
        { kPOnPre, "Preamplificatore", AuKind::Toggle, 0, 1, 1 },
        { kPOnDesk, "Banco attivo", AuKind::Toggle, 0, 1, 1 },
        { kPOnMedium, "Supporto attivo", AuKind::Toggle, 0, 1, 1 },
        { kPOnWow, "Wow e flutter attivi", AuKind::Toggle, 0, 1, 1 },
        { kPOnNoise, "Rumori attivi", AuKind::Toggle, 0, 1, 1 },
        { kPOnNR, "Riduzione rumore attiva", AuKind::Toggle, 0, 1, 1 },
        { kPOnTransfer, "Riversaggio attivo", AuKind::Toggle, 0, 1, 1 },
        { kPOnListen, "Ascolto attivo", AuKind::Toggle, 0, 1, 1 },
        { kPDrive, "Saturazione", AuKind::Range, 0, 3, 1, "×" },
        { kPNoise, "Rumori", AuKind::Range, -30, 20, 0, "dB" },
        { kPWow, "Wow e flutter", AuKind::Range, 0, 4, 1, "×" },
        { kPMix, "Dry/wet", AuKind::Range, 0, 100, 100, "%" },
        { kPOut, "Uscita", AuKind::Range, -24, 12, 0, "dB" },
        { kPMeterRef, "Taratura meter", AuKind::Range, -24, -6, -18, "dBFS" },   // the level that reads 0 VU
        { kPMeterL, "Meter sinistro", AuKind::Meter },
        { kPMeterR, "Meter destro", AuKind::Meter },
    };
    return defs[id];
}

// The default plain value: Mix e supporto starts on ¼" tape at 38 cm/s, everything else on its table's default.
inline double auDefault(AuParamId id, bool mix)
{
    return id == kPMedium && mix ? 1 : auParam(id).def;
}

// ---- What goes with what: each menu offers only what its parents allow (recorder → microphone → medium → noise
// reduction; desk → master medium → noise reduction → transfer). A device stays in service kAuDeviceLife years, a desk
// kAuDeskLife: later microphones and reducers fit within that span, a master medium must be in use while the desk is.
constexpr int kAuDeviceLife = 15, kAuDeskLife = 25;

inline const AuDevice& auDevice(const double* v)
{
    int f, m;
    auDeviceSplit(int(v[kPDevice]), f, m);
    return kAuFamilies[f].devices[m];
}
inline int auPlain(const double* v, AuParamId id) { return int(std::lround(v[id])); }

// Noise reduction nr on medium: Dolby A on reels, B and C on cassettes, dbx on both; none fits everything.
inline bool auNRFits(int nr, int medium, int maxYear)
{
    if (nr == 0) return true;
    if (kAuNR[nr].year > maxYear) return false;
    const bool reel = (kAuReel >> medium) & 1, cassette = (kAuCassette >> medium) & 1;
    return nr == 4 ? reel || cassette : nr == 1 ? reel : cassette;
}

// The master medium in Mix e supporto, the medium actually recorded on in Ripresa.
inline int auMediumOf(const double* v, bool mix)
{
    return mix || auPlain(v, kPMedium) > 0 ? auPlain(v, kPMedium) : auDevice(v).medium;
}

// The plain values a list parameter may take given the others, in menu order. In Ripresa the first entry is always
// the recorder's own (plain 0) and the alternatives follow; other lists are filtered tables.
inline std::vector<int> auAllowed(AuParamId id, const double* v, bool mix)
{
    std::vector<int> out;
    const int n = int(auListNames(id, mix).size());
    const AuDevice& dev = auDevice(v);
    const AuConsole& desk = kAuConsoles[mix ? auPlain(v, kPDesk) : 0];
    const int medium = auMediumOf(v, mix);
    const int maxYear = mix ? (desk.year ? desk.year + kAuDeskLife : 9999) : dev.year + kAuDeviceLife;
    auto keep = [&](bool ok, int plain) { if (ok) out.push_back(plain); };
    switch (id) {
    case kPMic:   // the horn only on the phonograph, the phonograph only with its horn
        out.push_back(0);
        for (int m = 2; m < n && dev.mic != 1; ++m) keep(m != dev.mic && kAuMics[m].year <= maxYear, m);
        break;
    case kPMedium:
        if (!mix) {
            out.push_back(0);
            for (int m = 1; m < n; ++m) keep(m != dev.medium && ((dev.media >> m) & 1), m);
        } else
            for (int m = 0; m < n; ++m)
                keep(!desk.year || (kAuMedia[m].to >= desk.year && kAuMedia[m].from <= desk.year + kAuDeskLife), m);
        break;
    case kPNR:
        if (!mix) out.push_back(0);
        for (int k = 0; k < int(std::size(kAuNR)); ++k)
            keep((mix || k != dev.nr) && auNRFits(k, medium, maxYear), k + (mix ? 0 : 1));
        break;
    case kPTransfer:   // a copy onto anything still in use when the master was made
        out.push_back(0);
        for (int m = 1; m < n; ++m) keep(kAuMedia[m].to >= kAuMedia[medium].from, m);
        break;
    default:
        for (int i = 0; i < n; ++i) out.push_back(i);
    }
    return out;
}

// After a parent changed: moves every child whose value no longer fits to its first allowed entry, parents first.
// Returns the ids it changed.
inline std::vector<AuParamId> auCoerce(double* v, bool mix)
{
    std::vector<AuParamId> changed;
    const std::vector<AuParamId> order = mix ? std::vector { kPMedium, kPNR, kPTransfer }
                                             : std::vector { kPMic, kPMedium, kPNR };
    for (AuParamId id : order) {
        auto ok = auAllowed(id, v, mix);
        if (std::find(ok.begin(), ok.end(), auPlain(v, id)) == ok.end()) {
            v[id] = ok.front();
            changed.push_back(id);
        }
    }
    return changed;
}

// The name a menu shows for one entry: in Ripresa the recorder's own says what it is.
inline std::string auEntryName(AuParamId id, int plain, const double* v, bool mix)
{
    if (!mix && plain == 0) {
        const AuDevice& dev = auDevice(v);
        if (id == kPMic) return std::string("Di serie · ") + kAuMics[dev.mic].name;
        if (id == kPMedium) return std::string("Del registratore · ") + kAuMedia[dev.medium].name;
        if (id == kPNR) return std::string("Del registratore · ") + kAuNR[dev.nr].name;
    }
    return auListNames(id, mix)[size_t(plain)];
}

// A panel row: an optional stage switch on the left, then an optional control.
struct AuRow
{
    const char* label;
    int toggle, control;   // parameter ids, -1 = none
};

inline const std::vector<AuRow>& auRows(bool mix)
{
    static const std::vector<AuRow> ripresa = {
        { "Registratore", -1, kPDevice },
        { "Microfono", kPOnMic, kPMic },
        { "Preamplificatore", kPOnPre, -1 },
        { "Supporto", kPOnMedium, kPMedium },
        { "Riduzione rumore", kPOnNR, kPNR },
        { "Wow e flutter", kPOnWow, kPWow },
        { "Rumori", kPOnNoise, kPNoise },
    };
    static const std::vector<AuRow> mixRows = {
        { "Banco", kPOnDesk, kPDesk },
        { "Supporto", kPOnMedium, kPMedium },
        { "Riduzione rumore", kPOnNR, kPNR },
        { "Wow e flutter", kPOnWow, kPWow },
        { "Rumori", kPOnNoise, kPNoise },
        { "Riversaggio su", kPOnTransfer, kPTransfer },
        { "Copie", -1, kPCopies },
        { "Ascolto", kPOnListen, kPListen },
    };
    return mix ? mixRows : ripresa;
}

// The knobs on the front plate, under the device's name, and the two meters beside them.
inline const std::vector<AuParamId>& auKnobs()
{
    static const std::vector<AuParamId> knobs = { kPDrive, kPMix, kPOut, kPMeterRef };
    return knobs;
}

// The parameters a plugin has: bypass, everything its rows show, the knobs and the meters.
inline std::vector<AuParamId> auParams(bool mix)
{
    std::vector<AuParamId> out = { kPBypass };
    for (const AuRow& r : auRows(mix)) {
        if (r.toggle >= 0) out.push_back(AuParamId(r.toggle));
        if (r.control >= 0) out.push_back(AuParamId(r.control));
    }
    for (AuParamId k : auKnobs()) out.push_back(k);
    out.push_back(kPMeterL);
    out.push_back(kPMeterR);
    return out;
}

// How the meters read: a peak programme meter (fast rise, slow fall) on the portable reels, whose modulometers are
// one, and on the EMI desk; a VU (average, about 300 ms) everywhere else.
inline bool auMeterPeak(const double* v, bool mix)
{
    if (mix) return auPlain(v, kPDesk) == 2;
    int family, model;
    auDeviceSplit(auPlain(v, kPDevice), family, model);
    return family == 0;
}

// Plain parameter values (index for lists, 0/1 for switches) → the engine's settings.
inline AuSettings auSettings(const double* v, bool mix, uint32_t seed)
{
    AuSettings s;
    s.seed = seed;
    s.useDevice = !mix;
    auDeviceSplit(int(v[kPDevice]), s.family, s.model);
    s.mic = int(v[kPMic]);
    s.medium = mix ? int(v[kPMedium]) : (v[kPMedium] > 0 ? int(v[kPMedium]) : -1);
    s.nr = int(v[kPNR]) - (mix ? 0 : 1);
    s.desk = mix ? int(v[kPDesk]) : 0;
    s.transfer = mix ? int(v[kPTransfer]) : 0;
    s.copies = int(v[kPCopies] + 0.5);
    s.listen = mix ? int(v[kPListen]) : 0;
    const AuParamId on[kStages] = { kPOnMic, kPOnPre, kPOnDesk, kPOnMedium, kPOnWow,
                                    kPOnNoise, kPOnNR, kPOnTransfer, kPOnListen };
    for (int i = 0; i < kStages; ++i) s.on[i] = v[on[i]] > 0.5;
    s.drive = float(v[kPDrive]);
    s.noiseDb = float(v[kPNoise]);
    s.wow = float(v[kPWow]);
    s.mix = float(v[kPMix] / 100);
    s.outDb = float(v[kPOut]);
    return s;
}

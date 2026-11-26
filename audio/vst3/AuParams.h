// SPDX-License-Identifier: GPL-3.0-or-later
// The two plugins' controls as data: one table says what each parameter is, one per plugin says which ones it shows
// and in which rows of the panel. Parameter ids are permanent (hosts save automation by id): append only.
#pragma once
#include "AudioEngine.h"
#include "AuGuide.h"
#include "AuLooks.h"
#include "../../shared/EraSetups.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

enum AuParamId : int {
    kPBypass, kPDevice, kPMic, kPMedium, kPNR, kPDesk, kPTransfer, kPCopies, kPListen,
    kPOnMic, kPOnPre, kPOnDesk, kPOnMedium, kPOnWow, kPOnNoise, kPOnNR, kPOnTransfer, kPOnListen,
    kPDrive, kPNoise, kPWow, kPMix, kPOut, kPMeterRef, kPMeterL, kPMeterR,
    kPLowCut, kPLow, kPHigh, kPLimit, kPAuto, kPMeterInL, kPMeterInR,
    kPRecLevel, kPSource, kPInput, kPPan, kPFader, kPMicFilter, kPMicPattern, kPAge, kPCapture, kPEraSetup, kPCount
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
    case kPSource: return mix ? auPresetNames(kAuPrograms) : auPresetNames(kAuSources);
    case kPCapture: return auNames(kAuCaptures);
    case kPEraSetup: {
        std::vector<std::string> out;
        for (const EraSetup& e : kEraSetups) out.emplace_back(e.name);
        return out;
    }
    case kPInput: return { "Stereo", "Mono (somma L+R)", "Mono dal sinistro", "Mono dal destro" };
    case kPMicFilter:
    case kPMicPattern: return { "Di fabbrica", "1", "2", "3", "4", "5" };
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
        { kPLowCut, "Filtro passa-alto", AuKind::Range, 0, 400, 0, "Hz" },   // 0 = flat; the front's filter switch
        { kPLow, "Bassi", AuKind::Range, -15, 15, 0, "dB" },                 // shelf at 100 Hz: desk EQ, recorder tone
        { kPHigh, "Acuti", AuKind::Range, -15, 15, 0, "dB" },                // shelf at 10 kHz
        { kPLimit, "Limitatore", AuKind::Toggle },
        { kPAuto, "Livello automatico", AuKind::Toggle },
        { kPMeterInL, "Meter ingresso sinistro", AuKind::Meter },            // what the meter reads in record/test
        { kPMeterInR, "Meter ingresso destro", AuKind::Meter },
        // the record level: gain onto the medium, given back on playback (hiss and saturation move, loudness does not)
        { kPRecLevel, "Livello di registrazione", AuKind::Range, -24, 12, 0, "dB" },
        { kPSource, "Cosa registri", AuKind::List },   // the guide (AuGuide.h): no effect on the sound
        // the desk channel (Ripresa) or the master's input (Mix e supporto)
        { kPInput, "Ingresso", AuKind::List },
        { kPPan, "Panpot", AuKind::Range, -100, 100, 0, "L/R" },
        { kPFader, "Fader del canale", AuKind::Range, -60, 10, 0, "dB" },
        // the microphone's own switches: 0 = as it left the factory, then its positions (kAuMicFilters, kAuMicPatterns)
        { kPMicFilter, "Filtro del microfono", AuKind::List },
        { kPMicPattern, "Direttività del microfono", AuKind::List },
        { kPAge, "Usura del supporto", AuKind::Range, 0, 100, 0, "%" },   // 0 = new, as it sounded then
        { kPCapture, "Acquisizione", AuKind::List },   // the copy played back and digitised (kAuCaptures)
        { kPEraSetup, "Setup d'epoca", AuKind::List },   // shared/EraSetups.h: loads the chain, then stays a label
    };
    return defs[id];
}

// The default plain value: Mix e supporto starts on ¼" tape at 38 cm/s, everything else on its table's default.
inline double auDefault(AuParamId id, bool mix)
{
    if (id == kPDesk && !mix) return 0;   // a Ripresa channel starts on no desk (older projects sound as they did)
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
    case kPMicFilter:
    case kPMicPattern: {   // the positions the microphone in use has
        out.push_back(0);
        const int mic = auPlain(v, kPMic) > 0 ? auPlain(v, kPMic) : dev.mic;
        if (const AuMicSwitch* sw = auMicSwitch(id == kPMicFilter, mic))
            for (int i = 0; i < sw->count; ++i) out.push_back(i + 1);
        break;
    }
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
                                             : std::vector { kPMic, kPMicFilter, kPMicPattern, kPMedium, kPNR };
    for (AuParamId id : order) {
        auto ok = auAllowed(id, v, mix);
        if (std::find(ok.begin(), ok.end(), auPlain(v, id)) == ok.end()) {
            v[id] = ok.front();
            changed.push_back(id);
        }
    }
    return changed;
}

// The row of `name` in a table, -1 if absent.
template <class T, size_t N> int auRowNamed(const T (&table)[N], const char* name)
{
    for (size_t i = 0; i < N; ++i)
        if (name && !std::strcmp(table[i].name, name)) return int(i);
    return -1;
}

// What era setup s (shared/EraSetups.h) sets in this plugin, before auCoerce. Ripresa: the recorder, its microphone
// and its medium (the recorder's own when the row leaves them empty), only if the row names a recorder. Mix e
// supporto: desk, master medium, transfer and listening (none when empty). ok: every name the row gives exists.
inline std::vector<std::pair<AuParamId, double>> auSetupValues(int s, bool mix, bool* ok = nullptr)
{
    std::vector<std::pair<AuParamId, double>> out;
    if (ok) *ok = true;
    if (s <= 0 || s >= kEraSetupCount) return out;
    const EraSetup& e = kEraSetups[s];
    auto named = [&](int row, const char* name) {   // the row, 0 for no name; a missing name fails the check
        if (name && row < 0 && ok) *ok = false;
        return name && row >= 0 ? row : 0;
    };
    const int device = e.recorder ? auDeviceNamed(e.recorder) : -1;
    const int mic = named(auRowNamed(kAuMics, e.mic), e.mic), tape = named(auRowNamed(kAuMedia, e.tape), e.tape);
    if (!mix) {
        if (e.recorder && device < 0 && ok) *ok = false;
        if (device < 0) return out;
        int f, m;
        auDeviceSplit(device, f, m);
        const AuDevice& dev = kAuFamilies[f].devices[m];
        out = { { kPDevice, device }, { kPMic, mic == dev.mic ? 0 : mic }, { kPMedium, tape == dev.medium ? 0 : tape },
                { kPNR, 0 }, { kPDesk, 0 } };
        return out;
    }
    const int desk = named(auRowNamed(kAuConsoles, e.desk), e.desk);
    const int transfer = named(auRowNamed(kAuMedia, e.audioTransfer), e.audioTransfer);
    const int listen = named(auRowNamed(kAuListen, e.listen), e.listen);
    int master = tape;
    if (!e.tape && device >= 0) {
        int f, m;
        auDeviceSplit(device, f, m);
        master = kAuFamilies[f].devices[m].medium;
    }
    if (!e.desk && !e.tape && !e.recorder && !e.audioTransfer && !e.listen) return out;   // a silent era
    out = { { kPDesk, desk }, { kPMedium, master }, { kPNR, 0 }, { kPTransfer, transfer }, { kPListen, listen } };
    return out;
}

// The name a menu shows for one entry: in Ripresa the recorder's own says what it is.
inline std::string auEntryName(AuParamId id, int plain, const double* v, bool mix)
{
    if (id == kPMicFilter || id == kPMicPattern) {
        const int mic = auPlain(v, kPMic) > 0 ? auPlain(v, kPMic) : auDevice(v).mic;
        const AuMicSwitch* sw = auMicSwitch(id == kPMicFilter, mic);
        if (!sw) return plain == 0 ? "Il microfono non ne ha" : auListNames(id, mix)[size_t(plain)];
        if (plain == 0) return std::string("Di fabbrica · ") + sw->pos[sw->def].name;
        return plain <= sw->count ? sw->pos[plain - 1].name : auListNames(id, mix)[size_t(plain)];
    }
    if (!mix && plain == 0) {
        const AuDevice& dev = auDevice(v);
        if (id == kPMic) return std::string("Di serie · ") + kAuMics[dev.mic].name;
        if (id == kPMedium) return std::string("Del registratore · ") + kAuMedia[dev.medium].name;
        if (id == kPNR) return std::string("Del registratore · ") + kAuNR[dev.nr].name;
    }
    return auListNames(id, mix)[size_t(plain)];
}

// A panel row: an optional stage switch on the left, then an optional control. Half rows go two to a line; a header
// opens a group (the panel reads top to bottom: the machine, the preset of use, the choices from microphone to
// medium, the trims).
enum class AuRowKind : uint8_t { Full, Half, Header };
struct AuRow
{
    const char* label;
    int toggle, control;   // parameter ids, -1 = none
    AuRowKind kind = AuRowKind::Full;
};

inline const std::vector<AuRow>& auRows(bool mix)
{
    using K = AuRowKind;
    static const std::vector<AuRow> ripresa = {
        { "Setup d'epoca", -1, kPEraSetup },
        { "Registratore", -1, kPDevice },
        { "Preset d'uso", -1, kPSource },
        { "Dal microfono al supporto", -1, -1, K::Header },
        { "Microfono", kPOnMic, kPMic },
        { "Filtro", -1, kPMicFilter, K::Half },
        { "Direttività", -1, kPMicPattern, K::Half },
        { "Ingresso", -1, kPInput, K::Half },
        { "Preamplificatore", kPOnPre, -1, K::Half },
        { "Banco (canale)", kPOnDesk, kPDesk },
        { "Supporto", kPOnMedium, kPMedium },
        { "Riduzione rumore", kPOnNR, kPNR },
        { "Regolazioni", -1, -1, K::Header },
        { "Livello reg.", -1, kPRecLevel, K::Half },
        { "Usura supporto", -1, kPAge, K::Half },
        { "Rumori", kPOnNoise, kPNoise, K::Half },
        { "Wow e flutter", kPOnWow, kPWow, K::Half },
        { "Panpot", -1, kPPan, K::Half },
        { "Fader canale", -1, kPFader, K::Half },
        { "Saturazione", -1, kPDrive, K::Half },
        { "Dry/wet", -1, kPMix, K::Half },
        { "Uscita", -1, kPOut, K::Half },
        { "Taratura meter", -1, kPMeterRef, K::Half },
    };
    static const std::vector<AuRow> mixRows = {
        { "Setup d'epoca", -1, kPEraSetup },
        { "Banco", kPOnDesk, kPDesk },
        { "Preset di missaggio", -1, kPSource },
        { "Dal banco al supporto", -1, -1, K::Header },
        { "Ingresso", -1, kPInput, K::Half },
        { "Copie", -1, kPCopies, K::Half },
        { "Supporto", kPOnMedium, kPMedium },
        { "Riduzione rumore", kPOnNR, kPNR },
        { "Riversaggio su", kPOnTransfer, kPTransfer },
        { "Acquisizione", -1, kPCapture },
        { "Ascolto", kPOnListen, kPListen },
        { "Regolazioni", -1, -1, K::Header },
        { "Livello master", -1, kPRecLevel, K::Half },
        { "Usura supporto", -1, kPAge, K::Half },
        { "Rumori", kPOnNoise, kPNoise, K::Half },
        { "Wow e flutter", kPOnWow, kPWow, K::Half },
        { "Saturazione", -1, kPDrive, K::Half },
        { "Dry/wet", -1, kPMix, K::Half },
        { "Uscita", -1, kPOut, K::Half },
        { "Taratura meter", -1, kPMeterRef, K::Half },
    };
    return mix ? mixRows : ripresa;
}

// The parameters a plugin has: bypass, everything its rows show, the fronts' own controls and the meters. The rows
// end with the plugin's trims (saturation, dry/wet, output, meter calibration): every machine has them, whatever its
// front.
inline std::vector<AuParamId> auParams(bool mix)
{
    std::vector<AuParamId> out = { kPBypass };
    for (const AuRow& r : auRows(mix)) {
        if (r.toggle >= 0) out.push_back(AuParamId(r.toggle));
        if (r.control >= 0) out.push_back(AuParamId(r.control));
    }
    for (AuParamId k : { kPLowCut, kPLow, kPHigh, kPLimit, kPAuto }) out.push_back(k);   // the fronts' own controls
    for (AuParamId k : { kPMeterL, kPMeterR, kPMeterInL, kPMeterInR }) out.push_back(k);
    return out;
}

// The front the panel shows: the chosen recorder's in Ripresa, the desk's in Mix e supporto (AuLooks.h).
inline const AuLook& auLook(const double* v, bool mix)
{
    if (mix) return kAuDeskLooks[std::clamp(auPlain(v, kPDesk), 0, int(std::size(kAuDeskLooks)) - 1)];
    int family, model;
    auDeviceSplit(auPlain(v, kPDevice), family, model);
    return auDeviceLook(family, model);
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
    s.desk = int(v[kPDesk]);   // Ripresa: the channel's desk; Mix e supporto: the bus
    s.input = int(v[kPInput]);
    s.pan = mix ? 0 : float(v[kPPan] / 100);
    s.faderDb = mix ? 0 : float(v[kPFader]);
    s.micFilter = auPlain(v, kPMicFilter) - 1;   // -1: as it left the factory
    s.micPattern = auPlain(v, kPMicPattern) - 1;
    s.age = float(v[kPAge] / 100);
    s.capture = mix ? auPlain(v, kPCapture) : 0;
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
    s.lowCutHz = float(v[kPLowCut]);
    s.lowDb = float(v[kPLow]);
    s.highDb = float(v[kPHigh]);
    s.limit = v[kPLimit] > 0.5;
    s.recDb = float(v[kPRecLevel]);
    s.alc = v[kPAuto] > 0.5 || (!mix && auLook(v, mix).meter == AuMeter::Automatic);   // built in, no switch
    return s;
}

// ---- The guide («Cosa registri»): what to aim for on this machine's meter, and the settings it suggests.

inline std::string auSignedDb(double db)   // «−3», «+9», «0»
{
    const long r = std::lround(db);
    return r == 0 ? "0" : (r < 0 ? "−" : "+") + std::to_string(std::labs(r));
}
// A level as the machine's meter shows it: «−3 VU», «PPM 6», «0 dB».
inline std::string auMeterReading(AuMeter meter, double db)
{
    if (meter == AuMeter::PPM) {
        const double mark = std::clamp(4 + db / 4, 0.0, 7.0);
        return "PPM " + (std::fabs(mark - std::round(mark)) < 0.13 ? std::to_string(std::lround(mark))
                                                                     : std::to_string(int(mark)) + "½");
    }
    if (meter == AuMeter::VU) return auSignedDb(db) + " VU";
    return auSignedDb(db) + " dB";
}

// The microphone a preset of use picks on this recorder, as the menu's plain value (-1: none of its choices fits).
inline int auGuideMic(const double* v, int source)
{
    const AuSource& s = kAuSources[std::clamp(source, 0, int(std::size(kAuSources)) - 1)];
    const AuDevice& dev = auDevice(v);
    const std::vector<int> ok = auAllowed(kPMic, v, false);
    for (int m : s.mics) {
        if (m < 0) break;
        const int plain = m == dev.mic ? 0 : m;
        if (std::find(ok.begin(), ok.end(), plain) != ok.end()) return plain;
    }
    return -1;
}

inline AuLoud auPresetLoud(const double* v, bool mix)
{
    return mix ? kAuPrograms[std::clamp(auPlain(v, kPSource), 0, int(std::size(kAuPrograms)) - 1)].loud
               : kAuSources[std::clamp(auPlain(v, kPSource), 0, int(std::size(kAuSources)) - 1)].loud;
}

// The record level that brings the measured peaks (dB on the machine's meter, before the record level) to the target.
inline double auGuideLevel(const double* v, bool mix, double measured)
{
    const AuParamDef& d = auParam(kPRecLevel);
    return std::clamp(std::round(auTargetPeak(auLook(v, mix).meter, auPresetLoud(v, mix)) - measured), d.min, d.max);
}

constexpr double kAuNoMeasure = -1000;   // nothing measured yet (no signal in the last seconds)

// What a preset of use sets on this machine, in order (each value fits the ones before it): microphone, its
// switches, input, filter, limiter, speed or cassette. Mix: the master's input.
inline std::vector<std::pair<AuParamId, double>> auPresetValues(const double* values, bool mix)
{
    std::vector<std::pair<AuParamId, double>> out;
    const int source = auPlain(values, kPSource);
    if (source <= 0) return out;
    std::vector<double> v(values, values + kPCount);
    auto set = [&](AuParamId id, double plain) {
        v[id] = plain;
        out.push_back({ id, plain });
    };
    if (mix) {
        set(kPInput, kAuPrograms[std::clamp(source, 0, int(std::size(kAuPrograms)) - 1)].input);
        return out;
    }
    const AuSource& s = kAuSources[std::clamp(source, 0, int(std::size(kAuSources)) - 1)];
    if (int mic = auGuideMic(v.data(), source); mic >= 0) set(kPMic, mic);
    auto position = [&](AuParamId id, const char* const* names) {   // the first named position the microphone has
        double plain = 0;
        for (int p : auAllowed(id, v.data(), false))
            for (int k = 0; k < 3 && names[k] && p > 0 && plain == 0; ++k)
                if (auEntryName(id, p, v.data(), false) == names[k]) plain = p;
        set(id, plain);
    };
    position(kPMicFilter, s.filter);
    position(kPMicPattern, s.pattern);
    set(kPInput, s.input);
    set(kPLowCut, s.lowCut);
    set(kPLimit, s.limit ? 1 : 0);
    if (s.speed != AuSpeed::Keep) {   // the quietest medium the machine takes, or the next (the usual for speech)
        const AuDevice& dev = auDevice(v.data());
        std::vector<int> media = auAllowed(kPMedium, v.data(), false);
        auto hiss = [&](int plain) { return kAuMedia[plain == 0 ? dev.medium : plain].hissDb; };
        std::stable_sort(media.begin(), media.end(), [&](int x, int y) { return hiss(x) < hiss(y); });
        set(kPMedium, media[s.speed == AuSpeed::Standard && media.size() > 1 ? 1 : 0]);
    }
    return out;
}

// What the guide says, in a few lines. `measured`: the peaks of the last seconds on this meter, without the record
// level (kAuNoMeasure if silent).
inline std::string auGuideText(const double* v, bool mix, double measured)
{
    const int source = auPlain(v, kPSource);
    if (source <= 0) return mix ? "Scegli un preset di missaggio: imposta l'ingresso e ti dico a che livello portare "
                                  "le punte sul meter del banco, con quali banchi e macchine si faceva."
                                : "Scegli un preset d'uso: imposta il registratore come in quella presa (microfono e "
                                  "suoi commutatori, filtro, limitatore, velocità) e ti dico a che livello registrare.";
    const AuMeter meter = auLook(v, mix).meter;
    const double target = auTargetPeak(meter, auPresetLoud(v, mix));
    std::string t;
    if (meter == AuMeter::None) t = "Nessun meter: si regolava la distanza dalla tromba. ";
    else if (meter == AuMeter::Neon) t = "Le punte devono appena accendere la lampada al neon. ";
    else t = "Punte a " + auMeterReading(meter, target) + (mix ? " sul meter del banco. " : " su questo meter. ");
    if (measured > kAuNoMeasure / 2)
        t += "Ora arrivano a " + auMeterReading(meter, measured + v[kPRecLevel]) + ": «Regola livello» lo porta a " +
             auSignedDb(auGuideLevel(v, mix, measured)) + " dB.\n";
    else
        t += "Fai suonare la sorgente qualche secondo: misuro le punte e regolo il livello.\n";
    auto names = [](const char* const* list, int count, bool desks) {
        std::string out;
        for (int i = 0; i < count && list[i]; ++i) {
            const int k = desks ? auDeskNamed(list[i]) : auDeviceNamed(list[i]);
            if (k < 0) continue;
            int f = 0, m = 0;
            if (!desks) auDeviceSplit(k, f, m);
            out += (out.empty() ? "" : ", ") + std::string(desks ? kAuConsoles[k].name : kAuFamilies[f].devices[m].name);
        }
        return out;
    };
    auto capital = [](std::string tip) {
        if (!tip.empty()) tip[0] = char(std::toupper(static_cast<unsigned char>(tip[0])));
        return tip;
    };
    if (mix) {
        const AuProgram& p = kAuPrograms[source];
        return t + "Banchi d'uso: " + names(p.desks, 3, true) + ". Per le tracce: " + names(p.recorders, 3, false) +
               ".\n" + capital(p.tip) + ".";
    }
    const AuSource& s = kAuSources[source];
    return t + "Macchine d'uso: " + names(s.recorders, 4, false) + ".\n" + capital(s.tip) + ".";
}

// The «?»: the story of the machine on show.
inline const char* auStory(const double* v, bool mix)
{
    if (mix) return kAuDeskStories[std::clamp(auPlain(v, kPDesk), 0, int(std::size(kAuDeskStories)) - 1)];
    return kAuDeviceStories[std::clamp(auPlain(v, kPDevice), 0, int(std::size(kAuDeviceStories)) - 1)];
}

// SPDX-License-Identifier: GPL-3.0-or-later
// The two plugins' controls as data: one table says what each parameter is, one per plugin says which ones it shows
// and in which rows of the panel. Parameter ids are permanent (hosts save automation by id): append only.
#pragma once
#include "AudioEngine.h"

#include <string>
#include <vector>

enum AuParamId : int {
    kPBypass, kPDevice, kPMic, kPMedium, kPNR, kPDesk, kPTransfer, kPCopies, kPListen,
    kPOnMic, kPOnPre, kPOnDesk, kPOnMedium, kPOnWow, kPOnNoise, kPOnNR, kPOnTransfer, kPOnListen,
    kPDrive, kPNoise, kPWow, kPMix, kPOut, kPCount
};

enum class AuKind { List, Toggle, Range };

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
    };
    return defs[id];
}

// The default plain value: Mix e supporto starts on ¼" tape at 38 cm/s, everything else on its table's default.
inline double auDefault(AuParamId id, bool mix)
{
    return id == kPMedium && mix ? 1 : auParam(id).def;
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
        { "Saturazione", -1, kPDrive },
        { "Dry/wet", -1, kPMix },
        { "Uscita", -1, kPOut },
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
        { "Saturazione", -1, kPDrive },
        { "Dry/wet", -1, kPMix },
        { "Uscita", -1, kPOut },
    };
    return mix ? mixRows : ripresa;
}

// The parameters a plugin has: bypass plus everything its rows show.
inline std::vector<AuParamId> auParams(bool mix)
{
    std::vector<AuParamId> out = { kPBypass };
    for (const AuRow& r : auRows(mix)) {
        if (r.toggle >= 0) out.push_back(AuParamId(r.toggle));
        if (r.control >= 0) out.push_back(AuParamId(r.control));
    }
    return out;
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

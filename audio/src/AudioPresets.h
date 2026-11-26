// SPDX-License-Identifier: GPL-3.0-or-later
// Log MetaRaw Audio: the presets as data, twin of the Creative's tables. A name says which sound a row aims at; every
// number is a parametric estimate from published specifications and period descriptions (docs/audio/AUDIO.md), not a
// measured profile of that product. Order is permanent within each list (a saved project keeps the index): append only.
#pragma once
#include <cstddef>
#include <iterator>

// A microphone: band edges (Hz, 0 = open), one resonance, proximity (low shelf at 150 Hz, dB) and capsule drive.
struct AuMic
{
    const char* name;
    float hp, lp, peakHz, peakDb, peakQ, prox, drive;
    int year;   // introduced
};

inline constexpr AuMic kAuMics[] = {
    { "Di serie del registratore", 0, 0, 1000, 0, 1, 0, 0, 0 },                    // the device row picks it
    { "Tromba acustica (registrazione a cilindro)", 300, 3500, 1200, 9, 3, 0, 0.3f, 1877 },
    { "Microfono a carbone", 300, 3400, 1500, 6, 1.5f, 0, 0.8f, 1878 },
    { "Piezoelettrico a cristallo", 150, 6000, 3000, 8, 2, 0, 0.2f, 1935 },
    { "RCA 44-BX (nastro, 1932)", 30, 11000, 2000, -1, 0.7f, 4, 0, 1932 },
    { "RCA 77-DX (nastro, 1954)", 35, 13000, 3000, 1, 0.7f, 3, 0, 1954 },
    { "Shure 55 Unidyne (1939)", 60, 10000, 5000, 4, 1, 2, 0.1f, 1939 },
    { "Neumann U 47 (valvole, 1949)", 30, 16000, 9000, 3, 0.8f, 2, 0.15f, 1949 },
    { "AKG C12 (valvole, 1953)", 30, 18000, 12000, 5, 0.9f, 1.5f, 0.1f, 1953 },
    { "Electro-Voice 635A (1955)", 80, 12000, 4000, 2, 0.8f, 0, 0, 1955 },
    { "Sennheiser MD 421 (1960)", 50, 17000, 5000, 3, 1, 2, 0, 1960 },
    { "Shure SM58 (1966)", 80, 15000, 5000, 5, 1, 3, 0, 1966 },
    { "Sennheiser MKH 416 (fucile, 1975)", 60, 20000, 9000, 5, 1, 0, 0, 1975 },
    { "Dinamico economico di serie", 150, 8000, 3000, 4, 1, 0, 0.1f, 1955 },
    { "Electret di videocamera", 120, 12000, 4000, 3, 1, 0, 0, 1975 },
};

// The switches a microphone really had (from its data sheet): a bass filter and a polar pattern. A filter position
// cuts the lows with a shelf at 100 Hz; a pattern scales the proximity effect (omni has none, figure of eight the
// most). Positions in the order engraved on the microphone; `def` is how it left the factory. [dedotto: the amounts]
struct AuMicPos
{
    const char* name;
    float value;   // filter: dB at 100 Hz; pattern: proximity ×
};
struct AuMicSwitch
{
    int mic;   // row of kAuMics
    AuMicPos pos[5];
    int count, def;
};
inline constexpr AuMicSwitch kAuMicFilters[] = {
    { 5, { { "M (musica)", 0 }, { "V1 (voce)", -6 }, { "V2 (voce, più taglio)", -12 } }, 3, 0 },   // RCA 77-DX
    { 10, { { "M", 0 }, { "2", -3 }, { "3", -6 }, { "4", -10 }, { "S", -15 } }, 5, 0 },               // MD 421
};
inline constexpr AuMicSwitch kAuMicPatterns[] = {
    { 5, { { "Bidirezionale", 1.3f }, { "Unidirezionale", 1 }, { "Non direzionale", 0 } }, 3, 1 },   // RCA 77-DX
    { 7, { { "Cardioide", 1 }, { "Omnidirezionale", 0 } }, 2, 0 },                                   // U 47
    { 8, { { "Omni", 0 }, { "Subcardioide", 0.5f }, { "Cardioide", 1 }, { "Ipercardioide", 1.15f }, { "Figura a 8", 1.3f } }, 5, 2 },  // C12
};
template <size_t N> constexpr const AuMicSwitch* auMicSwitch(const AuMicSwitch (&table)[N], int mic)
{
    for (const AuMicSwitch& s : table)
        if (s.mic == mic) return &s;
    return nullptr;
}
inline const AuMicSwitch* auMicSwitch(bool filter, int mic)
{
    return filter ? auMicSwitch(kAuMicFilters, mic) : auMicSwitch(kAuMicPatterns, mic);
}

// A recording or transfer medium. Band edges and the head bump (Hz, dB); drive is the saturation of a full-level
// signal (0 = clean); noise floors in dBFS (-200 = none); wow and flutter as peak speed error in % with their rates;
// crackle and dropout as events per second; comp the medium's own limiting (0..1); mono sums the channels.
struct AuMedium
{
    const char* name;
    float hp, lp, bumpHz, bumpDb, drive, asym, hissDb, humDb, rumbleDb, crackle;
    float wow, wowHz, flutter, flutterHz, dropout, comp;
    bool mono;
    int from, to;   // years in use (9999 = still)
};

inline constexpr AuMedium kAuMedia[] = {
    { "Nessuno (digitale)", 0, 0, 100, 0, 0, 0, -200, -200, -200, 0, 0, 0.5f, 0, 8, 0, 0, false, 1982, 9999 },
    { "Nastro ¼\" · 38 cm/s", 25, 20000, 50, 1.5f, 0.6f, 0.05f, -68, -200, -200, 0, 0.02f, 0.6f, 0.03f, 9, 0.002f, 0, false, 1950, 9999 },
    { "Nastro ¼\" · 19 cm/s", 30, 16000, 70, 2, 0.8f, 0.05f, -62, -200, -200, 0, 0.05f, 0.6f, 0.06f, 9, 0.004f, 0, false, 1948, 9999 },
    { "Nastro ¼\" · 9,5 cm/s", 40, 12000, 90, 2.5f, 1, 0.06f, -55, -200, -200, 0, 0.1f, 0.6f, 0.12f, 8, 0.01f, 0, false, 1950, 2000 },
    { "Nastro ¼\" · 4,75 cm/s", 60, 6500, 110, 3, 1.2f, 0.08f, -48, -200, -200, 0, 0.25f, 0.5f, 0.3f, 7, 0.03f, 0, false, 1955, 1990 },
    { "Cassetta Tipo I (ferro)", 40, 12500, 80, 1.5f, 1.1f, 0.06f, -52, -200, -200, 0, 0.12f, 0.8f, 0.15f, 10, 0.02f, 0, false, 1963, 9999 },
    { "Cassetta Tipo II (cromo)", 40, 14500, 80, 1.5f, 1, 0.05f, -55, -200, -200, 0, 0.12f, 0.8f, 0.15f, 10, 0.02f, 0, false, 1970, 9999 },
    { "Cassetta Tipo IV (metallo)", 35, 16000, 80, 1.5f, 0.9f, 0.05f, -57, -200, -200, 0, 0.1f, 0.8f, 0.12f, 10, 0.015f, 0, false, 1979, 9999 },
    { "Filo d'acciaio", 150, 5000, 300, 2, 1.4f, 0.15f, -40, -200, -200, 0.5f, 0.5f, 0.7f, 0.6f, 6, 0.1f, 0.2f, true, 1946, 1955 },
    { "Cilindro di cera", 250, 4000, 1500, 4, 1.2f, 0.2f, -34, -200, -48, 3, 0.8f, 0.9f, 0.4f, 5, 0, 0.3f, true, 1888, 1929 },
    { "Disco 78 giri (gommalacca)", 80, 6000, 2500, 2, 0.9f, 0.1f, -38, -200, -50, 6, 0.3f, 1.3f, 0.1f, 12, 0, 0, true, 1898, 1960 },
    { "Vinile 45 giri", 30, 15000, 100, 0.5f, 0.4f, 0.08f, -60, -200, -58, 1.5f, 0.08f, 0.75f, 0.05f, 12, 0, 0, false, 1949, 9999 },
    { "Vinile 33 giri LP", 25, 16000, 100, 0.5f, 0.4f, 0.08f, -62, -200, -60, 0.8f, 0.07f, 0.55f, 0.04f, 12, 0, 0, false, 1948, 9999 },
    { "VHS lineare SP", 100, 10000, 200, 1, 1, 0.06f, -45, -60, -200, 0, 0.2f, 0.4f, 0.3f, 13, 0.05f, 0.2f, true, 1976, 2008 },
    { "VHS lineare LP", 100, 6000, 200, 1.5f, 1.1f, 0.06f, -40, -58, -200, 0, 0.3f, 0.4f, 0.5f, 13, 0.08f, 0.2f, true, 1977, 2008 },
    { "VHS Hi-Fi", 20, 20000, 100, 0, 0.3f, 0.02f, -75, -55, -200, 0, 0.02f, 0.4f, 0.02f, 13, 0.02f, 0.3f, false, 1983, 2008 },
    { "Video8 AFM", 20, 15000, 100, 0, 0.3f, 0.02f, -70, -60, -200, 0, 0.03f, 0.4f, 0.03f, 13, 0.02f, 0.3f, true, 1985, 2007 },
    { "U-matic", 50, 12000, 150, 1, 0.8f, 0.05f, -48, -62, -200, 0, 0.1f, 0.5f, 0.15f, 12, 0.03f, 0.1f, true, 1971, 2000 },
    { "Banda ottica 35 mm (Academy)", 50, 7500, 100, 0, 0.9f, 0.12f, -50, -200, -200, 0.5f, 0.05f, 1, 0.1f, 24, 0, 0.1f, true, 1927, 9999 },
    { "Banda ottica 16 mm", 100, 6000, 100, 0, 1, 0.12f, -42, -200, -200, 0.8f, 0.1f, 1, 0.2f, 24, 0, 0.1f, true, 1935, 2000 },
    { "Pista magnetica Super 8", 70, 8000, 150, 2, 1.1f, 0.08f, -45, -200, -200, 0.3f, 0.3f, 1.5f, 0.4f, 18, 0.05f, 0, true, 1973, 1990 },
    { "Radio AM", 100, 4500, 100, 0, 0.6f, 0.05f, -45, -55, -200, 0.5f, 0, 0.5f, 0, 8, 0, 0.8f, true, 1920, 9999 },
    { "Radio FM", 30, 15000, 100, 0, 0.3f, 0.02f, -65, -200, -200, 0, 0, 0.5f, 0, 8, 0, 0.6f, false, 1945, 9999 },
    { "Telefono fisso", 300, 3400, 1000, 2, 1.5f, 0.15f, -45, -200, -200, 0.2f, 0, 0.5f, 0, 8, 0, 0.5f, true, 1900, 9999 },
    { "Nastro ¼\" · 2,4 cm/s", 80, 4500, 130, 3.5f, 1.4f, 0.1f, -44, -200, -200, 0, 0.4f, 0.5f, 0.45f, 6, 0.05f, 0, false, 1961, 1990 },
};

// The medium's standard equalisation: the playback high-frequency time constant (µs; 0 = none), the record side
// pre-emphasising the same amount. The highs reach the medium boosted, so they saturate first, as on the real thing:
// CCIR/IEC 35 µs at 38 cm/s, 70 µs at 19, 90 µs at 9,5, 120 µs at 4,75 and 2,4 and on type I cassettes, 70 µs on
// type II and IV, RIAA 75 µs on vinyl, 50 µs on European FM. [standard]
inline constexpr float kAuMediumEqUs[] = {
    0, 35, 70, 90, 120,   // digital, ¼" 38, 19, 9,5, 4,75
    120, 70, 70,          // cassette I, II, IV
    0, 0, 0, 75, 75,      // wire, wax, shellac (curves of the period vary), vinyl 45, 33
    0, 0, 0, 0, 0,        // VHS linear SP, LP, Hi-Fi, Video8, U-matic
    0, 0, 0,              // optical 35, 16, Super 8 stripe
    0, 50, 0,             // AM, FM, telephone
    120,                  // ¼" 2,4
};
static_assert(std::size(kAuMediumEqUs) == std::size(kAuMedia), "one equalisation per medium");

// A noise reducer: how much it lowers the medium's hiss (dB). Modelled as the hiss it removes, not as a compander.
struct AuNoiseReduction
{
    const char* name;
    float hissCutDb;
    int year;
};

inline constexpr AuNoiseReduction kAuNR[] = {
    { "Nessuna", 0, 0 }, { "Dolby A", 10, 1966 }, { "Dolby B", 9, 1968 }, { "Dolby C", 18, 1980 }, { "dbx", 28, 1971 },
};

// A recorder or camera: its built-in microphone (index in kAuMics), the media it takes (bit i = kAuMedia[i]), the one
// it starts from, the preamplifier (drive, asym: even harmonics of valves), mains hum (dBFS), year and noise reduction.
struct AuDevice
{
    const char* name;
    int mic;
    unsigned long media;
    int medium;
    float preDrive, preAsym, humDb;
    int year;
    int nr = 0;
};
constexpr unsigned long auBit(int i) { return 1ul << i; }
constexpr unsigned long kAuReel = auBit(1) | auBit(2) | auBit(3) | auBit(4);
constexpr unsigned long kAuCassette = auBit(5) | auBit(6) | auBit(7);

inline constexpr AuDevice kAuReelPortable[] = {
    { "Geloso G.257 (1961)", 3, auBit(4), 4, 0.9f, 0.3f, -54, 1961 },
    { "Nagra III (1958)", 9, auBit(1) | auBit(2) | auBit(3), 2, 0.4f, 0.05f, -200, 1958 },
    { "Uher 4000 Report-L (1965)", 13, auBit(2) | auBit(3) | auBit(4) | auBit(24), 3, 0.6f, 0.05f, -200, 1965 },
    { "Nagra 4.2 (1971)", 12, auBit(1) | auBit(2) | auBit(3), 2, 0.3f, 0.02f, -200, 1971 },
};
inline constexpr AuDevice kAuReelStudio[] = {
    { "Ampex 350 (valvole, 1953)", 7, auBit(1) | auBit(2), 1, 0.6f, 0.25f, -72, 1953 },
    { "Studer J37 (valvole, 1964)", 7, auBit(1) | auBit(2), 1, 0.7f, 0.25f, -72, 1964 },
    { "Revox A77 (1967)", 10, auBit(2) | auBit(3), 2, 0.5f, 0.05f, -200, 1967 },
    { "Studer A80 (1970)", 10, auBit(1) | auBit(2), 1, 0.4f, 0.03f, -200, 1970 },
};
inline constexpr AuDevice kAuCassetteDecks[] = {
    { "Philips EL 3300 (1963)", 13, auBit(5), 5, 0.8f, 0.05f, -60, 1963 },
    { "Tascam Portastudio 144 (1979)", 11, kAuCassette, 5, 0.6f, 0.05f, -200, 1979, 4 },
    { "Sony Walkman Pro WM-D6C (1984)", 13, kAuCassette, 6, 0.4f, 0.03f, -200, 1984, 2 },
    { "Sony TC-D5M (1980)", 13, kAuCassette, 7, 0.4f, 0.03f, -200, 1980, 2 },
};
inline constexpr AuDevice kAuCameras[] = {
    { "Auricon Cine-Voice 16 mm (1949)", 9, auBit(19), 19, 0.8f, 0.25f, -58, 1949 },
    { "Kodak Ektasound 130 Super 8 (1973)", 13, auBit(20), 20, 0.7f, 0.05f, -200, 1973 },
    { "Canon 514XL-S Super 8 (1975)", 13, auBit(20), 20, 0.6f, 0.05f, -200, 1975 },
    { "JVC GR-C1 VHS-C (1984)", 14, auBit(13) | auBit(14) | auBit(15), 13, 0.5f, 0.03f, -200, 1984 },
    { "Sony Handycam CCD-V8 Video8 (1985)", 14, auBit(16), 16, 0.4f, 0.03f, -200, 1985 },
};
inline constexpr AuDevice kAuEarly[] = {
    { "Fonografo Edison a cilindro (1900)", 1, auBit(9), 9, 0, 0, -200, 1900 },
    { "Webster-Chicago 180 a filo (1948)", 3, auBit(8), 8, 1, 0.3f, -50, 1948 },
};

struct AuFamily
{
    const char* name;
    const AuDevice* devices;
    size_t count;
};
#define AU_FAMILY(name, table) { name, table, std::size(table) }
inline constexpr AuFamily kAuFamilies[] = {
    AU_FAMILY("Registratore a bobine portatile", kAuReelPortable),
    AU_FAMILY("Registratore a bobine da studio", kAuReelStudio),
    AU_FAMILY("Registratore a cassetta", kAuCassetteDecks),
    AU_FAMILY("Cinepresa e videocamera sonora", kAuCameras),
    AU_FAMILY("Prima del nastro", kAuEarly),
};
#undef AU_FAMILY

// A mixing desk on the bus: transformer and valve drive, its band, shelves (100 Hz, 8 kHz), bus compression, noise.
struct AuConsole
{
    const char* name;
    float hp, lp, drive, asym, lowDb, highDb, comp, noiseDb;
    int year;
};

inline constexpr AuConsole kAuConsoles[] = {
    { "Nessuno", 0, 0, 0, 0, 0, 0, 0, -200, 0 },
    { "RCA BC-2B (broadcast a valvole, 1950)", 40, 12000, 0.8f, 0.3f, 0.5f, -1, 0.3f, -70, 1950 },
    { "EMI REDD.51 (valvole, 1964)", 30, 16000, 0.7f, 0.25f, 1, 1, 0.2f, -74, 1964 },
    { "Neve 8078 (1978)", 20, 20000, 0.6f, 0.1f, 1.5f, 1.5f, 0.3f, -82, 1978 },
    { "SSL 4000 E (1979)", 20, 22000, 0.4f, 0.02f, 0, 1, 0.6f, -86, 1979 },
    { "Mackie CR-1604 (1993)", 20, 20000, 0.3f, 0.01f, 0, 0, 0, -80, 1993 },
};

// Where the mix is heard: loudspeaker band, cabinet resonance, cone drive.
struct AuListen
{
    const char* name;
    float hp, lp, peakHz, peakDb, peakQ, drive;
    bool mono;
};

inline constexpr AuListen kAuListen[] = {
    { "Monitor (nessuno)", 0, 0, 1000, 0, 1, 0, false },
    { "Radio a valvole (anni '50)", 120, 5500, 1000, 3, 0.8f, 0.5f, true },
    { "Radiolina a transistor", 300, 4000, 2000, 5, 1, 0.8f, true },
    { "Juke-box Wurlitzer 1015 (1946)", 60, 7000, 120, 4, 1.2f, 0.4f, true },
    { "Televisore CRT (anni '70)", 150, 7000, 2500, 3, 1, 0.3f, true },
    { "Sala cinematografica (curva X)", 40, 10000, 1000, 0, 1, 0, false },
};

// The acquisition: how the analogue copy comes back into the computer, the machine that plays it and the converter
// that digitises it. Band (Hz), the playback head's azimuth error (dB lost at 10 kHz, and the time slip between the
// channels in µs), ground-loop hum and converter noise (dBFS), the converter's bits (0 = clean 24-bit) and where it
// clips (dBFS, a hard edge), the player's own wow (% peak). [dedotto: typical of each kind, not a measured unit]
struct AuCapture
{
    const char* name;
    float hp, lp, azimuthDb, slipUs, humDb, noiseDb;
    int bits;
    float clipDb, wow;
    int year;
};

inline constexpr AuCapture kAuCaptures[] = {
    { "Nessuna (passaggio diretto)", 0, 0, 0, 0, -200, -200, 0, 0, 0, 0 },
    { "Piastra a cassetta di casa + scheda audio", 30, 15000, 3, 25, -72, -84, 16, -0.5f, 0.08f, 1995 },
    { "Videoregistratore VHS + chiavetta di cattura USB", 40, 14000, 1, 10, -60, -70, 16, -3, 0.12f, 2005 },
    { "Giradischi + preamplificatore RIAA + convertitore", 20, 18000, 0, 0, -66, -88, 16, -1, 0.06f, 2000 },
    { "Registratore a bobine allineato + convertitore da studio", 20, 20000, 0.5f, 5, -90, -110, 0, -0.1f, 0.03f, 2010 },
    { "Lettore ottico del proiettore + scheda audio", 60, 9000, 0, 0, -58, -80, 16, -1, 0.15f, 1998 },
    { "Sintonizzatore radio + registrazione al computer", 30, 15000, 0, 0, -70, -82, 16, -1, 0, 2000 },
    { "Registratore digitale portatile (8 bit, anni '90)", 60, 7000, 0, 0, -200, -48, 8, -1, 0, 1995 },
};


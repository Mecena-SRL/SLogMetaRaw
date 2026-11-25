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
};

inline constexpr AuMic kAuMics[] = {
    { "Di serie del registratore", 0, 0, 1000, 0, 1, 0, 0 },                    // the device row picks it
    { "Tromba acustica (registrazione a cilindro)", 300, 3500, 1200, 9, 3, 0, 0.3f },
    { "Microfono a carbone", 300, 3400, 1500, 6, 1.5f, 0, 0.8f },
    { "Piezoelettrico a cristallo", 150, 6000, 3000, 8, 2, 0, 0.2f },
    { "RCA 44-BX (nastro, 1932)", 30, 11000, 2000, -1, 0.7f, 4, 0 },
    { "RCA 77-DX (nastro, 1954)", 35, 13000, 3000, 1, 0.7f, 3, 0 },
    { "Shure 55 Unidyne (1939)", 60, 10000, 5000, 4, 1, 2, 0.1f },
    { "Neumann U 47 (valvole, 1949)", 30, 16000, 9000, 3, 0.8f, 2, 0.15f },
    { "AKG C12 (valvole, 1953)", 30, 18000, 12000, 5, 0.9f, 1.5f, 0.1f },
    { "Electro-Voice 635A (1955)", 80, 12000, 4000, 2, 0.8f, 0, 0 },
    { "Sennheiser MD 421 (1960)", 50, 17000, 5000, 3, 1, 2, 0 },
    { "Shure SM58 (1966)", 80, 15000, 5000, 5, 1, 3, 0 },
    { "Sennheiser MKH 416 (fucile, 1975)", 60, 20000, 9000, 5, 1, 0, 0 },
    { "Dinamico economico di serie", 150, 8000, 3000, 4, 1, 0, 0.1f },
    { "Electret di videocamera", 120, 12000, 4000, 3, 1, 0, 0 },
};

// A recording or transfer medium. Band edges and the head bump (Hz, dB); drive is the saturation of a full-level
// signal (0 = clean); noise floors in dBFS (-200 = none); wow and flutter as peak speed error in % with their rates;
// crackle and dropout as events per second; comp the medium's own limiting (0..1); mono sums the channels.
struct AuMedium
{
    const char* name;
    float hp, lp, bumpHz, bumpDb, drive, asym, hissDb, humDb, rumbleDb, crackle;
    float wow, wowHz, flutter, flutterHz, dropout, comp;
    bool mono;
};

inline constexpr AuMedium kAuMedia[] = {
    { "Nessuno (digitale)", 0, 0, 100, 0, 0, 0, -200, -200, -200, 0, 0, 0.5f, 0, 8, 0, 0, false },
    { "Nastro ¼\" · 38 cm/s", 25, 20000, 50, 1.5f, 0.6f, 0.05f, -68, -200, -200, 0, 0.02f, 0.6f, 0.03f, 9, 0.002f, 0, false },
    { "Nastro ¼\" · 19 cm/s", 30, 16000, 70, 2, 0.8f, 0.05f, -62, -200, -200, 0, 0.05f, 0.6f, 0.06f, 9, 0.004f, 0, false },
    { "Nastro ¼\" · 9,5 cm/s", 40, 12000, 90, 2.5f, 1, 0.06f, -55, -200, -200, 0, 0.1f, 0.6f, 0.12f, 8, 0.01f, 0, false },
    { "Nastro ¼\" · 4,75 cm/s", 60, 6500, 110, 3, 1.2f, 0.08f, -48, -200, -200, 0, 0.25f, 0.5f, 0.3f, 7, 0.03f, 0, false },
    { "Cassetta Tipo I (ferro)", 40, 12500, 80, 1.5f, 1.1f, 0.06f, -52, -200, -200, 0, 0.12f, 0.8f, 0.15f, 10, 0.02f, 0, false },
    { "Cassetta Tipo II (cromo)", 40, 14500, 80, 1.5f, 1, 0.05f, -55, -200, -200, 0, 0.12f, 0.8f, 0.15f, 10, 0.02f, 0, false },
    { "Cassetta Tipo IV (metallo)", 35, 16000, 80, 1.5f, 0.9f, 0.05f, -57, -200, -200, 0, 0.1f, 0.8f, 0.12f, 10, 0.015f, 0, false },
    { "Filo d'acciaio", 150, 5000, 300, 2, 1.4f, 0.15f, -40, -200, -200, 0.5f, 0.5f, 0.7f, 0.6f, 6, 0.1f, 0.2f, true },
    { "Cilindro di cera", 250, 4000, 1500, 4, 1.2f, 0.2f, -34, -200, -48, 8, 0.8f, 0.9f, 0.4f, 5, 0, 0.3f, true },
    { "Disco 78 giri (gommalacca)", 80, 6000, 2500, 2, 0.9f, 0.1f, -38, -200, -50, 6, 0.3f, 1.3f, 0.1f, 12, 0, 0, true },
    { "Vinile 45 giri", 30, 15000, 100, 0.5f, 0.4f, 0.08f, -60, -200, -58, 1.5f, 0.08f, 0.75f, 0.05f, 12, 0, 0, false },
    { "Vinile 33 giri LP", 25, 16000, 100, 0.5f, 0.4f, 0.08f, -62, -200, -60, 0.8f, 0.07f, 0.55f, 0.04f, 12, 0, 0, false },
    { "VHS lineare SP", 100, 10000, 200, 1, 1, 0.06f, -45, -60, -200, 0, 0.2f, 0.4f, 0.3f, 13, 0.05f, 0.2f, true },
    { "VHS lineare LP", 100, 6000, 200, 1.5f, 1.1f, 0.06f, -40, -58, -200, 0, 0.3f, 0.4f, 0.5f, 13, 0.08f, 0.2f, true },
    { "VHS Hi-Fi", 20, 20000, 100, 0, 0.3f, 0.02f, -75, -55, -200, 0, 0.02f, 0.4f, 0.02f, 13, 0.02f, 0.3f, false },
    { "Video8 AFM", 20, 15000, 100, 0, 0.3f, 0.02f, -70, -60, -200, 0, 0.03f, 0.4f, 0.03f, 13, 0.02f, 0.3f, true },
    { "U-matic", 50, 12000, 150, 1, 0.8f, 0.05f, -48, -62, -200, 0, 0.1f, 0.5f, 0.15f, 12, 0.03f, 0.1f, true },
    { "Banda ottica 35 mm (Academy)", 50, 7500, 100, 0, 0.9f, 0.12f, -50, -200, -200, 2, 0.05f, 1, 0.1f, 24, 0, 0.1f, true },
    { "Banda ottica 16 mm", 100, 6000, 100, 0, 1, 0.12f, -42, -200, -200, 3, 0.1f, 1, 0.2f, 24, 0, 0.1f, true },
    { "Pista magnetica Super 8", 70, 8000, 150, 2, 1.1f, 0.08f, -45, -200, -200, 0.3f, 0.3f, 1.5f, 0.4f, 18, 0.05f, 0, true },
    { "Radio AM", 100, 4500, 100, 0, 0.6f, 0.05f, -45, -55, -200, 0.5f, 0, 0.5f, 0, 8, 0, 0.8f, true },
    { "Radio FM", 30, 15000, 100, 0, 0.3f, 0.02f, -65, -200, -200, 0, 0, 0.5f, 0, 8, 0, 0.6f, false },
    { "Telefono fisso", 300, 3400, 1000, 2, 1.5f, 0.15f, -45, -200, -200, 0.2f, 0, 0.5f, 0, 8, 0, 0.5f, true },
};

// A noise reducer: how much it lowers the medium's hiss (dB). Modelled as the hiss it removes, not as a compander.
struct AuNoiseReduction
{
    const char* name;
    float hissCutDb;
};

inline constexpr AuNoiseReduction kAuNR[] = {
    { "Nessuna", 0 }, { "Dolby A", 10 }, { "Dolby B", 9 }, { "Dolby C", 18 }, { "dbx", 28 },
};

// A recorder or camera: its built-in microphone (index in kAuMics), the media it takes (bit i = kAuMedia[i]), the one
// it starts from, the preamplifier (drive, asym: even harmonics of valves), mains hum (dBFS) and noise reduction.
struct AuDevice
{
    const char* name;
    int mic;
    unsigned long media;
    int medium;
    float preDrive, preAsym, humDb;
    int nr = 0;
};
constexpr unsigned long auBit(int i) { return 1ul << i; }
constexpr unsigned long kAuReel = auBit(1) | auBit(2) | auBit(3) | auBit(4);
constexpr unsigned long kAuCassette = auBit(5) | auBit(6) | auBit(7);

inline constexpr AuDevice kAuReelPortable[] = {
    { "Geloso G.257 (anni '50)", 3, auBit(3) | auBit(4), 4, 0.9f, 0.3f, -54 },
    { "Nagra III (1958)", 9, auBit(1) | auBit(2) | auBit(3), 2, 0.4f, 0.05f, -200 },
    { "Uher 4000 Report-L (1965)", 13, auBit(2) | auBit(3) | auBit(4), 3, 0.6f, 0.05f, -200 },
    { "Nagra 4.2 (1971)", 12, auBit(1) | auBit(2), 2, 0.3f, 0.02f, -200 },
};
inline constexpr AuDevice kAuReelStudio[] = {
    { "Ampex 350 (valvole, 1953)", 7, auBit(1) | auBit(2), 1, 0.6f, 0.25f, -72 },
    { "Studer J37 (valvole, 1964)", 7, auBit(1) | auBit(2), 1, 0.7f, 0.25f, -72 },
    { "Revox A77 (1967)", 10, auBit(2) | auBit(3), 2, 0.5f, 0.05f, -200 },
    { "Studer A80 (1970)", 10, auBit(1) | auBit(2), 1, 0.4f, 0.03f, -200 },
};
inline constexpr AuDevice kAuCassetteDecks[] = {
    { "Philips EL 3300 (1963)", 13, auBit(5), 5, 0.8f, 0.05f, -60 },
    { "Tascam Portastudio 144 (1979)", 11, kAuCassette, 5, 0.6f, 0.05f, -200, 4 },
    { "Sony Walkman Pro WM-D6C (1984)", 13, kAuCassette, 6, 0.4f, 0.03f, -200, 2 },
    { "Sony TC-D5M (1980)", 13, kAuCassette, 7, 0.4f, 0.03f, -200, 2 },
};
inline constexpr AuDevice kAuCameras[] = {
    { "Auricon Cine-Voice 16 mm (1949)", 9, auBit(19), 19, 0.8f, 0.25f, -58 },
    { "Kodak Ektasound 130 Super 8 (1973)", 13, auBit(20), 20, 0.7f, 0.05f, -200 },
    { "Canon 514XL-S Super 8 (1975)", 13, auBit(20), 20, 0.6f, 0.05f, -200 },
    { "JVC GR-C1 VHS-C (1984)", 14, auBit(13) | auBit(14) | auBit(15), 13, 0.5f, 0.03f, -200 },
    { "Sony Handycam CCD-V8 Video8 (1985)", 14, auBit(16), 16, 0.4f, 0.03f, -200 },
};
inline constexpr AuDevice kAuEarly[] = {
    { "Fonografo Edison a cilindro (1900)", 1, auBit(9), 9, 0, 0, -200 },
    { "Webster-Chicago 180 a filo (1948)", 3, auBit(8), 8, 1, 0.3f, -50 },
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
};

inline constexpr AuConsole kAuConsoles[] = {
    { "Nessuno", 0, 0, 0, 0, 0, 0, 0, -200 },
    { "RCA BC-2B (broadcast a valvole, 1950)", 40, 12000, 0.8f, 0.3f, 0.5f, -1, 0.3f, -70 },
    { "EMI REDD.51 (valvole, 1964)", 30, 16000, 0.7f, 0.25f, 1, 1, 0.2f, -74 },
    { "Neve 8078 (1978)", 20, 20000, 0.6f, 0.1f, 1.5f, 1.5f, 0.3f, -82 },
    { "SSL 4000 E (1979)", 20, 22000, 0.4f, 0.02f, 0, 1, 0.6f, -86 },
    { "Mackie CR-1604 (1993)", 20, 20000, 0.3f, 0.01f, 0, 0, 0, -80 },
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

// SPDX-License-Identifier: GPL-3.0-or-later
// The Creative node's presets as data: device families, their models (real commercial names), the film
// manufacturers and their stocks. A name says which look a row aims at; every number is a parametric estimate from
// the research (docs/CREATIVE.md, "Preset"), not a measured profile of that product. Order is permanent within each
// list (Resolve saves the index); append only.
#pragma once
#include <array>
#include <cstddef>
#include <iterator>

// Gauges a stock was made in (bits).
enum : unsigned { kG8 = 1, kGS8 = 2, kG95 = 4, kG16 = 8, kG35 = 16, kG65 = 32, kG135 = 64, kG120 = 128 };

// A film stock: the engine row it starts from (a process's variant) and what sets it apart. rms 0 = the row's grain;
// contrast multiplies the curve's slope; cast in stops per layer; sens: the B/W spectral sensitivity row.
struct CrStock
{
    const char* name;
    int process, row;
    unsigned gauges;
    float rms = 0, sat = 1, contrast = 1, hal = 1;
    float cast[3] = { 0, 0, 0 };
    int sens = 3;
};
enum { kStNeg, kStRev, kStBW };   // which process: colour negative, colour reversal, black and white

#define CR_FUJI { -0.02f, 0.015f, 0.01f }   // cooler greens, cyan-green shadows [gen]
#define CR_EAST { 0.0f, 0.03f, -0.02f }     // green-cyan Soviet and East German casts [gen]
#define CR_AGFA { 0.03f, 0.02f, -0.04f }    // olive greens, orange reds [gen]

inline constexpr CrStock kCrKodak[] = {
    { "Vision3 500T 5219 (2007)", kStNeg, 1, kGS8 | kG16 | kG35 | kG65, 11 },
    { "Vision3 200T 5213 (2010)", kStNeg, 15, kGS8 | kG16 | kG35 | kG65, 8.25f },
    { "Vision3 250D 5207 (2010)", kStNeg, 0, kG16 | kG35 | kG65, 7.7f },
    { "Vision3 50D 5203 (2011)", kStNeg, 0, kGS8 | kG16 | kG35 | kG65, 6 },
    { "Vision2 500T 5218 (2002)", kStNeg, 1, kGS8 | kG16 | kG35 | kG65, 11.5f, 1, 1.02f },
    { "Vision 800T 5289 (1998)", kStNeg, 1, kG16 | kG35, 15.4f, 0.97f, 0.95f },
    { "Vision Expression 500T 5284 (2001)", kStNeg, 3, kG16 | kG35, 11, 0.95f, 0.9f, 1, { 0.01f, 0, -0.01f } },
    { "Primetime 640T 5620 (1995)", kStNeg, 3, kG16 | kG35, 14, 0.95f, 0.85f },
    { "EXR 500T 5298 (1994)", kStNeg, 1, kG16 | kG35, 13, 1.05f, 1.03f },
    { "EXR 200T 5293 (1992)", kStNeg, 15, kG16 | kG35, 9, 1.08f },
    { "EXR 50D 5245 (1989)", kStNeg, 14, kG16 | kG35, 5.5f, 1.1f },
    { "High Speed 5294 (1983)", kStNeg, 19, kG16 | kG35, 16 },
    { "Eastmancolor II 5247 (1974)", kStNeg, 19, kG16 | kG35 | kG65, 15, 0.95f, 1.02f, 1, { 0.02f, 0.01f, -0.02f } },
    { "Eastmancolor 5254 (1968)", kStNeg, 6, kG16 | kG35 | kG65, 17.6f, 0.95f, 0.95f },
    { "Eastmancolor 5248 (1952)", kStNeg, 9, kG35, 20 },
    { "Eastmancolor 5247 (1950)", kStNeg, 9, kG35, 22, 0.9f, 1.05f, 1, { 0.03f, 0.01f, -0.02f } },
    { "Verita 200D 5206 (2026)", kStNeg, 0, kG16 | kG35, 8 },
    { "Kodachrome 40 Super 8 (1974)", kStRev, 2, kGS8 | kG16, 11 },
    { "Kodachrome II (1961)", kStRev, 2, kG8 | kGS8 | kG16 | kG135, 10 },
    { "Kodachrome (1935)", kStRev, 2, kG8 | kG16 | kG135, 12, 0.95f, 1.05f },
    { "Kodachrome 25 (foto)", kStRev, 2, kG135, 9 },
    { "Kodachrome 64 (foto)", kStRev, 2, kG135 | kG120, 10 },
    { "Kodachrome 200 (foto)", kStRev, 2, kG135, 17 },
    { "Ektachrome 100D 7294 (2018)", kStRev, 4, kGS8 | kG16, 8 },
    { "Ektachrome 100D 7285 (1999)", kStRev, 4, kGS8 | kG16 | kG35, 9 },
    { "Ektachrome 64T 7280 (2005)", kStRev, 4, kGS8, 10, 1, 1, 1, { -0.01f, 0, 0.02f } },
    { "Ektachrome 160 Super 8 (1971)", kStRev, 3, kGS8, 16 },
    { "Ektachrome EF 7241 (1966)", kStRev, 1, kG16, 14, 1.05f, 1.05f },
    { "Ektachrome Commercial 7252 (1970)", kStRev, 1, kG16, 9, 0.95f, 0.8f },
    { "Ektachrome E100 (foto)", kStRev, 4, kG135 | kG120, 8, 1.1f },
    { "Ektachrome 100VS (foto)", kStRev, 0, kG135 | kG120, 11 },
    { "Portra 160 (foto)", kStNeg, 17, kG135 | kG120, 7 },
    { "Portra 400 (foto)", kStNeg, 16, kG135 | kG120, 10, 0.95f, 0.97f },
    { "Portra 800 (foto)", kStNeg, 18, kG135 | kG120, 14, 0.95f },
    { "Ektar 100 (foto)", kStNeg, 4, kG135 | kG120, 5, 1.2f },
    { "Gold 200 (foto)", kStNeg, 6, kG135, 10 },
    { "Ultramax 400 (foto)", kStNeg, 6, kG135, 12, 1.1f },
    { "Kodacolor II (1972, 110 e 126)", kStNeg, 6, kG135, 14, 1.1f },
    { "Double-X 5222 (1959)", kStBW, 0, kG16 | kG35 | kG65, 16 },
    { "Plus-X 5231 (1956)", kStBW, 0, kG16 | kG35, 12 },
    { "Super-XX 1232 (1938)", kStBW, 0, kG35, 16, 1, 0.95f, 1, { 0, 0, 0 }, 2 },
    { "4-X 5224 (1964)", kStBW, 1, kG16 | kG35, 19 },
    { "Tri-X invertibile 7266 (1955)", kStBW, 1, kG8 | kGS8 | kG16, 17, 1, 1.25f },
    { "Plus-X invertibile 7276 (1955)", kStBW, 0, kG8 | kG16, 12, 1, 1.25f },
    { "Tri-X 400 (foto)", kStBW, 1, kG135 | kG120, 17 },
    { "T-Max 100 (foto)", kStBW, 2, kG135 | kG120, 8 },
    { "T-Max 400 (foto)", kStBW, 2, kG135 | kG120, 10 } };

inline constexpr CrStock kCrFuji[] = {
    { "Eterna 500 8573 (2004)", kStNeg, 3, kG16 | kG35, 9, 1, 0.95f, 1, CR_FUJI },
    { "Eterna 250D 8563 (2006)", kStNeg, 3, kG16 | kG35, 7, 1, 0.95f, 1, CR_FUJI },
    { "Eterna Vivid 500 8547 (2009)", kStNeg, 4, kG16 | kG35, 10, 1.15f, 1, 1, CR_FUJI },
    { "Eterna Vivid 160 8543 (2007)", kStNeg, 4, kG16 | kG35, 7, 1.15f, 1, 1, CR_FUJI },
    { "Reala 500D 8592 (2001)", kStNeg, 0, kG16 | kG35, 9, 1, 0.97f, 1, { -0.01f, 0.005f, 0.005f } },
    { "F-64D 8522 (anni '90)", kStNeg, 14, kG16 | kG35, 5, 1.1f, 1, 1, CR_FUJI },
    { "F-500 8572 (anni '90)", kStNeg, 1, kG16 | kG35, 13, 1, 1, 1, CR_FUJI },
    { "Fujicolor AX 8514 (anni '80)", kStNeg, 1, kG16 | kG35, 15, 1, 1, 1, CR_FUJI },
    { "Fujicolor 8517 (1977)", kStNeg, 19, kG16 | kG35, 14, 1, 1, 1, CR_FUJI },
    { "Fujichrome R25 Single-8 (anni '70)", kStRev, 1, kGS8, 10, 1, 1, 1, CR_FUJI },
    { "Velvia 50 (foto)", kStRev, 0, kG135 | kG120, 9, 1.2f, 1.1f },
    { "Provia 100F (foto)", kStRev, 1, kG135 | kG120, 8 },
    { "Astia 100F (foto)", kStRev, 1, kG135 | kG120, 7, 0.95f, 0.85f },
    { "Pro 400H (foto)", kStNeg, 5, kG135 | kG120, 9, 0.9f, 1, 1, { -0.02f, 0.02f, 0.01f } },
    { "Superia 400 (foto)", kStNeg, 16, kG135, 12, 1, 1, 1, CR_FUJI } };

inline constexpr CrStock kCrAgfa[] = {
    { "Agfacolor Neu invertibile (1936)", kStRev, 1, kG16 | kG135, 14, 0.8f, 0.95f, 1, CR_AGFA },
    { "Agfacolor negativo cinema (1939)", kStNeg, 9, kG35, 16, 0.8f, 1, 1, CR_AGFA },
    { "Agfacolor CT18 / CK17 (1952-1977)", kStRev, 4, kG8 | kGS8 | kG16 | kG135, 11, 0.85f },
    { "Agfa Moviechrome 40 (1974)", kStRev, 3, kGS8, 13, 0.95f },
    { "Agfa XT 320 (1990)", kStNeg, 3, kG16 | kG35, 10 },
    { "Agfa XTR 250 (1990)", kStNeg, 3, kG16 | kG35, 9 },
    { "Agfa Ultra 100 (foto)", kStNeg, 4, kG135, 5, 1.3f },
    { "Agfa Vista 200 (foto)", kStNeg, 6, kG135, 8 },
    { "Agfa APX 100 (foto)", kStBW, 0, kG135 | kG120, 10 },
    { "Agfa Scala 200x (invertibile)", kStBW, 1, kG135 | kG120, 12, 1, 1.25f } };

inline constexpr CrStock kCrOrwo[] = {
    { "ORWOcolor NC 3 (1972)", kStNeg, 11, kG16 | kG35 | kG65, 14 },
    { "ORWOcolor NC 1 (1964)", kStNeg, 11, kG16 | kG35 | kG65, 16, 0.95f, 0.95f },
    { "Original Wolfen NC 500 (2022)", kStNeg, 1, kG35, 13, 0.95f },
    { "ORWOchrom UT 15 / UK 17", kStRev, 1, kG8 | kGS8 | kG16, 14, 0.8f, 1, 1, CR_EAST },
    { "ORWO UP 21 (invertibile)", kStBW, 0, kG8 | kG95 | kG16, 14, 1, 1.25f },
    { "ORWO UN 54", kStBW, 0, kG16 | kG35, 11, 1, 0.95f, 1.4f },
    { "ORWO N 74 / N 75", kStBW, 1, kG16 | kG35, 15 } };

inline constexpr CrStock kCrSvema[] = {
    { "Sovcolor (anni '50)", kStNeg, 10, kG35, 16 },
    { "Svema DS-4 (anni '60-'70)", kStNeg, 12, kG16 | kG35, 15 },
    { "Svema LN-8 (anni '80)", kStNeg, 12, kG16 | kG35, 13 },
    { "Svema CO-32D (invertibile)", kStRev, 1, kG8 | kGS8 | kG16, 15, 0.75f, 0.9f, 1, CR_EAST },
    { "Svema OCh-180 (invertibile B/N)", kStBW, 1, kG8 | kGS8 | kG16, 18, 1, 1.2f },
    { "Tasma A-2Sh (negativo B/N)", kStBW, 0, kG16 | kG35, 15 },
    { "Svema Foto 400 (foto)", kStBW, 1, kG135, 18 } };

inline constexpr CrStock kCrIlford[] = {
    { "HP5 Plus", kStBW, 1, kG8 | kG16 | kG135 | kG120, 18, 1, 0.92f },
    { "FP4 Plus", kStBW, 0, kG8 | kG16 | kG135 | kG120, 11 },
    { "Pan F Plus 50", kStBW, 3, kG135 | kG120 },
    { "Delta 100", kStBW, 2, kG135 | kG120, 8 },
    { "Delta 3200", kStBW, 4, kG135 | kG120 },
    { "XP2 Super (cromogenica)", kStBW, 2, kG135 | kG120, 9, 1, 0.9f },
    { "HP (1935)", kStBW, 1, kG135 | kG120, 20, 1, 1, 1, { 0, 0, 0 }, 2 } };

inline constexpr CrStock kCrFerrania[] = {
    { "Pancro 30 / P30 (Pasolini, Fellini)", kStBW, 0, kG16 | kG35 | kG135, 13, 1, 1.15f },
    { "Ferraniacolor (1952)", kStNeg, 13, kG35, 16, 0.85f, 1, 1, { 0.04f, 0.01f, -0.03f } },
    { "Pathéscope Colour 9,5 mm (anni '50)", kStRev, 1, kG95, 16, 0.85f, 1, 1, { 0.03f, 0.01f, -0.02f } },
    { "Solaris FG 200 (foto)", kStNeg, 6, kG135, 11 },
    { "Scotch Chrome 1000 (3M, foto)", kStRev, 5, kG135, 30, 0.7f },
    { "P30 Alpha (2017, foto)", kStBW, 0, kG135 | kG120, 13, 1, 1.15f } };

inline constexpr CrStock kCrGevaert[] = {
    { "Gevacolor (1947)", kStNeg, 13, kG35, 15 },
    { "Gevapan 30", kStBW, 0, kG16 | kG35, 13 },
    { "Gevapan 36 (rapida)", kStBW, 1, kG16 | kG35, 17 } };

inline constexpr CrStock kCrAnsco[] = {
    { "Anscocolor invertibile (anni '50)", kStRev, 1, kG16 | kG35, 14, 0.85f, 0.9f, 1, { 0, 0.01f, 0.02f } },
    { "Ansco Color Negative 844 (1956)", kStNeg, 9, kG35, 18 },
    { "Anscochrome 500 (anni '60)", kStRev, 1, kG16, 22, 0.9f },
    { "Ansco Supreme 453", kStBW, 0, kG35, 14 } };

inline constexpr CrStock kCrDupont[] = {
    { "Superior 2 (noir anni '40)", kStBW, 0, kG16 | kG35, 16, 1, 0.95f, 1, { 0, 0, 0 }, 2 },
    { "Superior 4 (rapida)", kStBW, 1, kG16 | kG35, 20 },
    { "Rapid Reversal 931", kStBW, 1, kG16, 17, 1, 1.25f } };

inline constexpr CrStock kCrCinestill[] = {
    { "CineStill 800T", kStNeg, 2, kG135 | kG120, 12, 1, 1.12f },
    { "CineStill 50D", kStNeg, 2, kG135 | kG120, 6, 1, 1.12f },
    { "CineStill 400D", kStNeg, 2, kG135 | kG120, 10, 1, 1.12f },
    { "CineStill BwXX", kStBW, 0, kG135 | kG120, 15 } };

inline constexpr CrStock kCrLomo[] = {
    { "Lomography Color 400", kStNeg, 6, kG135 | kG120, 13, 1.2f, 1.08f },
    { "LomoChrome Metropolis", kStNeg, 3, kG135 | kG120, 12, 0.55f, 1.15f },
    { "Berlin Kino 400", kStBW, 1, kG135 | kG120, 16 } };

inline constexpr CrStock kCrKonica[] = {
    { "Centuria Super 200", kStNeg, 5, kG135, 8, 0.95f },
    { "Impresa 50", kStNeg, 14, kG135, 4 } };

#undef CR_FUJI
#undef CR_EAST
#undef CR_AGFA

// The manufacturers: their stock lists, each its own choice (name permanent), shown with its manufacturer.
struct CrMaker { const char* name; const char* param; const CrStock* stocks; int count; };
inline constexpr CrMaker kCrMakers[] = {
    { "Kodak / Eastman Kodak", "stockKodak", kCrKodak, (int)std::size(kCrKodak) },
    { "Fujifilm", "stockFuji", kCrFuji, (int)std::size(kCrFuji) },
    { "Agfa / Agfa-Gevaert", "stockAgfa", kCrAgfa, (int)std::size(kCrAgfa) },
    { "ORWO (Wolfen)", "stockOrwo", kCrOrwo, (int)std::size(kCrOrwo) },
    { "Svema e Tasma (URSS)", "stockSvema", kCrSvema, (int)std::size(kCrSvema) },
    { "Ilford", "stockIlford", kCrIlford, (int)std::size(kCrIlford) },
    { "Ferrania (e 3M)", "stockFerrania", kCrFerrania, (int)std::size(kCrFerrania) },
    { "Gevaert", "stockGevaert", kCrGevaert, (int)std::size(kCrGevaert) },
    { "Ansco", "stockAnsco", kCrAnsco, (int)std::size(kCrAnsco) },
    { "DuPont", "stockDupont", kCrDupont, (int)std::size(kCrDupont) },
    { "CineStill", "stockCinestill", kCrCinestill, (int)std::size(kCrCinestill) },
    { "Lomography", "stockLomo", kCrLomo, (int)std::size(kCrLomo) },
    { "Konica", "stockKonica", kCrKonica, (int)std::size(kCrKonica) } };
const int kCrMakerCount = (int)std::size(kCrMakers);

// Film cameras: the native format (kCrFormatNames index), the formats it takes (bits of format indices), and what it
// loads into the texture rows: gate weave and flicker (0..1, multiples of the format's own), the lens's vignetting
// (stops), chromatic aberration and diffusion. Weave classes from the research: double pin 0.1-0.15, single pin
// 0.25-0.35, claw 0.4-0.6, amateur claw 0.7, cartridge 0.85-0.9, hand crank 1.
struct CrCamera { const char* name; int format; unsigned formats; float weave, breath, vignette, aberration, bloom; };
#define CR_F(i) (1u << (i))
inline constexpr CrCamera kCrCameras[] = {
    { "Pathé-Baby (9,5 mm, 1922, a manovella)", 8, CR_F(8), 1.0f, 0.6f, 1.0f, 0.4f, 0.15f },
    { "Kodak Brownie Movie Camera (8 mm, 1951)", 7, CR_F(7), 0.8f, 0.3f, 0.8f, 0.3f, 0.1f },
    { "Bolex H16 Reflex (16 mm, 1956)", 1, CR_F(1), 0.6f, 0.15f, 0.4f, 0.15f, 0 },
    { "Canon 1014 XL-S (Super 8, 1979)", 0, CR_F(0), 0.9f, 0.2f, 0.6f, 0.25f, 0.05f },
    { "Nizo S800 (Super 8, 1972)", 0, CR_F(0), 0.85f, 0.2f, 0.5f, 0.2f, 0 },
    { "Beaulieu 4008 ZM (Super 8, 1971)", 0, CR_F(0), 0.8f, 0.15f, 0.4f, 0.15f, 0 },
    { "Krasnogorsk-3 (16 mm, URSS 1971)", 1, CR_F(1) | CR_F(6), 0.65f, 0.2f, 0.7f, 0.35f, 0.1f },
    { "Arriflex 16ST (16 mm, 1952)", 1, CR_F(1), 0.5f, 0.1f, 0.4f, 0.15f, 0 },
    { "Éclair NPR (16 mm, 1960)", 1, CR_F(1), 0.4f, 0.1f, 0.35f, 0.1f, 0 },
    { "Arriflex 16SR (16 e Super 16, 1975)", 6, CR_F(1) | CR_F(6), 0.3f, 0.05f, 0.3f, 0.1f, 0 },
    { "Aaton XTR (Super 16, 1990)", 6, CR_F(1) | CR_F(6), 0.35f, 0.05f, 0.3f, 0.1f, 0 },
    { "Arriflex 416 (Super 16, 2006)", 6, CR_F(6), 0.15f, 0.03f, 0.2f, 0.05f, 0 },
    { "Mitchell BNC (35 mm Academy, 1934)", 12, CR_F(12) | CR_F(2), 0.15f, 0.05f, 0.5f, 0.2f, 0.1f },
    { "Arriflex 35 IIC (35 mm, 1963)", 12, CR_F(12) | CR_F(2) | CR_F(10), 0.45f, 0.05f, 0.4f, 0.15f, 0 },
    { "Konvas-1M (35 mm, URSS)", 12, CR_F(12) | CR_F(2), 0.5f, 0.1f, 0.6f, 0.3f, 0.15f },
    { "Arriflex 35BL (35 mm, 1972)", 2, CR_F(2) | CR_F(12) | CR_F(10), 0.25f, 0.03f, 0.3f, 0.1f, 0 },
    { "Panavision Panaflex (35 mm anamorfico, 1972)", 9, CR_F(9) | CR_F(2) | CR_F(12), 0.15f, 0.03f, 0.6f, 0.3f,
      0.1f },
    { "Arriflex 435 (Super 35, 1995)", 2, CR_F(2) | CR_F(12) | CR_F(10), 0.1f, 0.02f, 0.2f, 0.05f, 0 },
    { "Mitchell VistaVision (1954)", 11, CR_F(11), 0.15f, 0.03f, 0.3f, 0.1f, 0 },
    { "Todd-AO (65 mm, 1955)", 3, CR_F(3), 0.15f, 0.03f, 0.5f, 0.2f, 0.05f },
    { "Panavision System 65 (65 mm, 1991)", 3, CR_F(3), 0.1f, 0.02f, 0.3f, 0.1f, 0 },
    { "IMAX MSM 9802 (IMAX 15/70, 1990)", 4, CR_F(4), 0.1f, 0.02f, 0.3f, 0.1f, 0 },
    { "Leica M6 (135, 1984)", 13, CR_F(13), 0, 0.3f, 0.5f, 0.1f, 0 },
    { "Canon AE-1 (135, 1976)", 13, CR_F(13), 0, 0.3f, 0.4f, 0.15f, 0 },
    { "Hasselblad 500C/M (120 6x6, 1970)", 14, CR_F(14), 0, 0.3f, 0.4f, 0.05f, 0 } };
#undef CR_F
const int kCrCameraCount = (int)std::size(kCrCameras);

// Analog video cameras and recorders: the tape (kCrAnalogNames index) and its wear, head switching and timebase.
// sensor: kCrSensorNames (CreativeModel.h); broadcast ENG cameras of the 1970s paired with their recorder.
struct CrVideoCamera { const char* name; int tape; float wear, heads, instability; int sensor; };
inline constexpr CrVideoCamera kCrVideoCameras[] = {
    { "Camera da studio a tubi Plumbicon (broadcast, anni '70)", 0, 0, 0, 0, 1 },
    { "Sony U-matic BVU-110 (ENG, 1976)", 7, 0.2f, 0.3f, 0.3f, 1 },
    { "Ampex VPR-2 (1 pollice tipo C, 1977)", 10, 0.1f, 0, 0.1f, 1 },
    { "Sony Betacam SP BVW-400 (1986)", 9, 0.05f, 0, 0.05f, 3 },
    { "Sony Betamovie BMC-100 (Betamax, 1983)", 4, 0.2f, 0.3f, 0.25f, 4 },
    { "Panasonic NV-M7 (VHS, 1987)", 1, 0.2f, 0.35f, 0.3f, 7 },
    { "JVC GR-C1 (VHS-C, 1984)", 1, 0.25f, 0.4f, 0.35f, 4 },
    { "JVC GR-S707 (S-VHS, 1989)", 3, 0.15f, 0.3f, 0.25f, 7 },
    { "Sony Handycam CCD-V8 (Video8, 1985)", 5, 0.2f, 0.3f, 0.25f, 7 },
    { "Sony Handycam CCD-TR (Hi8, 1989)", 6, 0.15f, 0.25f, 0.2f, 7 },
    { "Panasonic WV-3110 con VHS portatile (vidicon, 1979)", 1, 0.25f, 0.4f, 0.35f, 5 },
    { "Panasonic PK-958 con VHS portatile (Newvicon, 1982)", 1, 0.2f, 0.35f, 0.3f, 6 },
    { "Ikegami HL-79E con U-matic BVU (Plumbicon, 1984)", 7, 0.15f, 0.25f, 0.25f, 2 } };
const int kCrVideoCameraCount = (int)std::size(kCrVideoCameras);

// Digital cameras by family: the engine's camera row (kCrDigitalNames index: curve, colour, noise) and the device's
// own pipeline (research DIGITALI_OTTICHE.md A.7, estimates): the sensor's lines (0: the picture's own), the codec's
// JPEG-like quality (0: no visible codec), chroma subsampling (0 4:4:4, 1 4:2:2, 2 4:2:0, 3 4:1:1), the ISP's
// sharpening, a noise multiple, aliasing (line skipping, no optical low-pass). Append only.
struct CrDigitalCamera { const char* name; int row, lines, quality, sub; float sharp, noise; int alias; };
inline constexpr CrDigitalCamera kCrCinemaCameras[] = {
    { "ARRI Alexa (2010)", 0, 0, 0, 0, 0, 1, 0 },
    { "Sony HDW-F900 CineAlta (2000)", 2, 1080, 75, 3, 0.3f, 1, 0 },
    { "Canon EOS 5D Mark II (2008)", 1, 720, 80, 2, 0.3f, 1, 1 },
    { "Panasonic AG-DVX100 (MiniDV 24p, 2002)", 4, 480, 70, 3, 0.4f, 1, 0 },
    { "Sony DCR-VX1000 (MiniDV 3CCD, 1995)", 3, 480, 70, 3, 0.7f, 1, 0 },
    { "RED One (2007)", 0, 2160, 85, 0, 0.15f, 1.6f, 0 },
    { "Canon EOS C300 (2012)", 0, 1080, 80, 1, 0, 0.8f, 0 },
    { "Blackmagic Pocket Cinema Camera (2013)", 0, 1080, 0, 1, 0, 1.3f, 1 },
    { "Panasonic Lumix GH2 (2010)", 1, 1080, 72, 2, 0.5f, 1.1f, 0 } };
inline constexpr CrDigitalCamera kCrPhoneCameras[] = {
    { "Apple iPhone 3GS (2009)", 6, 480, 55, 2, 0.6f, 1, 0 },
    { "Canon PowerShot A95 (2004)", 5, 480, 60, 1, 0.5f, 1, 0 },
    { "Logitech QuickCam (webcam, anni 2000)", 7, 240, 35, 2, 0.3f, 1, 0 },
    { "GoPro HD Hero (2010)", 8, 1080, 60, 2, 0.7f, 1, 0 },
    { "Apple iPhone 15 Pro (HDR)", 9, 2160, 90, 2, 0.4f, 1, 0 },
    { "Motorola RAZR V3 (2004)", 7, 144, 20, 2, 0.3f, 1.3f, 0 },
    { "Sony Ericsson K800i (2006)", 7, 144, 28, 2, 0.4f, 1.1f, 0 },
    { "Nokia N95 (2007)", 6, 480, 45, 2, 0.5f, 1, 0 },
    { "Flip UltraHD (2009)", 8, 720, 55, 2, 0.5f, 1, 0 },
    { "Kodak Zi8 (2009)", 8, 1080, 50, 2, 0.5f, 1.3f, 0 } };
const int kCrCinemaCameraCount = (int)std::size(kCrCinemaCameras), kCrPhoneCameraCount = (int)std::size(kCrPhoneCameras);

// The names of a table's rows, for its choice parameter.
template <class T, std::size_t N> constexpr std::array<const char*, N> crNames(const T (&t)[N])
{
    std::array<const char*, N> a {};
    for (std::size_t i = 0; i < N; ++i) a[i] = t[i].name;
    return a;
}
inline constexpr auto kCrCameraNames = crNames(kCrCameras);
inline constexpr auto kCrVideoCameraNames = crNames(kCrVideoCameras);
inline constexpr auto kCrCinemaCameraNames = crNames(kCrCinemaCameras);
inline constexpr auto kCrPhoneCameraNames = crNames(kCrPhoneCameras);
inline constexpr auto kCrMakerNames = crNames(kCrMakers);

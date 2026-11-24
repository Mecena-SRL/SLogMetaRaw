// SPDX-License-Identifier: GPL-3.0-or-later
#include "CreativeModel.h"

#include <algorithm>
#include <cmath>

const char* const kCrProcessNames[kCrProcessCount] = {
    "Negativo colore e stampa", "Invertibile colore (diapositiva)", "Bianco e nero", "Technicolor tre strisce",
    "Technicolor due colori", "Autochrome", "Dufaycolor (rete)", "Tre lastre (Prokudin-Gorskij)", "Kinemacolor",
    "Dagherrotipo", "Collodio umido", "Carta salata e albumina", "Cianotipia", "Platino e palladio", "Video analogico",
    "Digitale d'epoca" };
const char* const kCrProcessInfo[kCrProcessCount] = {
    "Tre strati: curva del negativo, interimage, grana, stampa positiva con le sue luci; uscita Log = scansione del negativo",
    "Positivo diretto: poca latitudine, neri densi, colori puri",
    "Un solo record con la sensibilità spettrale scelta e il filtro di presa, poi la stampa",
    "Tre negativi B/N dal prisma (verde; bipack blu e rosso), matrici CMY per imbibizione, chiave K fino al 1944",
    "Due record (rosso, verde) e due coloranti: incarnati rosa-arancio, cieli turchese, niente gialli né viola",
    "Grani di fecola tinti arancio, verde e viola su lastra invertibile: mosaico additivo, colori tenui",
    "Rete regolare a 23° di linee rosse e quadrati verdi e blu: mosaico additivo",
    "Tre pose in sequenza dietro filtri blu, verde e rosso: ciò che si muove fra le pose si separa in colori",
    "Due pose alternate rosso e verde, proiettate additive: bianco giallo-crema, frange sul movimento",
    "Argento su rame, sensibile solo al blu e all'UV: specchio, immagine rovesciata, ossidazione dai bordi",
    "Lastra blu-sensibile su vetro nero (ambrotipo) o ferro laccato (ferrotipo): i rossi vengono neri",
    "Stampa ad annerimento diretto da negativo blu-sensibile: toni caldi bruni, virati all'oro verso il viola",
    "Blu di Prussia su carta, da negativo: stampa ripida, alte luci che si lavano, pennellata",
    "Metallo nobile nelle fibre: scala lunghissima, platino neutro freddo, palladio bruno caldo",
    "Segnale video su nastro: luma e colore con banda stretta, colore in ritardo, rumore, testine, dropout",
    "Camere digitali d'epoca: gamma dinamica, spalla, matrice, nitidezza elettronica, rumore, sottocampionamento" };

const char* const kCrNegativeNames[kCrNegativeCount] = {
    "Cinema lenta, pulita", "Cinema veloce tungsteno", "Veloce senza remjet (alone forte)", "Fredda a basso contrasto",
    "Vivida ad alto contrasto", "Ritratto morbida", "Amatoriale calda", "Amatoriale fredda-verde",
    "Archivio Est Europa anni '70", "Monopack anni '50 (assorbimenti forti)", "Sovcolor anni '50",
    "ORWO NC anni '70", "Svema anni '70-'80", "Gevacolor anni '50" };
const char* const kCrReversalNames[kCrReversalCount] = { "Ultra-satura", "Neutra", "Coloranti aggiunti in sviluppo" };
const char* const kCrBWStockNames[kCrBWStockCount] = { "Cinema classica", "Veloce da reportage", "Tabulare finissima" };
const char* const kCrSensitivityNames[kCrSensitivityCount] = { "Blu (prima del 1880)", "Ortocromatica",
                                                               "Pancromatica precoce", "Pancromatica moderna" };
const char* const kCrFilterNames[kCrFilterCount] = { "Nessuno", "Giallo", "Arancio", "Rosso", "Verde" };
const char* const kCrTechniEraNames[kCrTechniEraCount] = { "1935-1944 con chiave K", "1945-1955",
                                                           "Imbibizione da monopack (1955-1975)" };
const char* const kCrCollodionNames[kCrCollodionCount] = { "Ambrotipo (vetro su nero)", "Ferrotipo (ferro laccato)" };
const char* const kCrPaperNames[kCrPaperCount] = { "Carta salata", "Albumina" };
const char* const kCrPrintNames[kCrPrintCount] = { "Nessuna (scansione del negativo)", "Positivo standard",
                                                   "Positivo a contrasto alto", "Positivo freddo", "Positivo morbido" };
const char* const kCrFormatNames[kCrFormatCount] = { "Super 8", "16 mm", "Super 35", "65 mm (Todd-AO, Panavision 70)",
                                                     "IMAX 15/70", "Lastra 9x12", "Super 16", "8 mm",
                                                     "9,5 mm Pathé-Baby", "35 mm anamorfico", "Techniscope",
                                                     "VistaVision" };
const char* const kCrAnalogNames[kCrAnalogCount] = { "Broadcast", "VHS SP", "VHS LP", "S-VHS", "Betamax", "Video8",
                                                     "Hi8", "U-matic", "U-matic SP", "Betacam SP", "1 pollice tipo C" };
const char* const kCrDigitalNames[kCrDigitalCount] = {
    "Cinema digitale moderno", "Reflex video 2009", "Cinema HD a CCD 2002", "CCD 3 sensori anni 2000 (MiniDV)",
    "MiniDV cine 24p", "Compatta digitale 2004", "Telefono 2008", "Webcam anni 2000", "Action cam 2010",
    "Telefono smart HDR" };

namespace {

const double kPi = 3.14159265358979323846;

// ---- tables (sources and status: docs/CREATIVE.md)

// Colour negatives: gamma per layer, toe and shoulder (stops), interimage, grain (RMS-like, 35 mm), halation,
// saturation, cast (stops per layer), unwanted dye absorptions (0 modern masked, 1 unmasked 1950s).
struct Negative { float g[3], toe, shoulder, iie, rms, hal, sat, cast[3], dirty; };
const Negative kNegatives[kCrNegativeCount] = {
    { { 0.60f, 0.60f, 0.61f }, 3.0f, 7.0f, 1.0f, 6, 1.0f, 1.00f, { 0, 0, 0 }, 0 },
    { { 0.58f, 0.59f, 0.60f }, 2.5f, 6.5f, 1.0f, 11, 1.2f, 1.00f, { 0.03f, 0, -0.03f }, 0 },
    { { 0.62f, 0.63f, 0.64f }, 2.5f, 6.0f, 1.0f, 13, 4.5f, 1.00f, { 0.02f, 0, -0.02f }, 0 },
    { { 0.55f, 0.56f, 0.57f }, 3.5f, 7.0f, 0.8f, 8, 1.0f, 0.85f, { -0.03f, 0, 0.03f }, 0 },
    { { 0.66f, 0.66f, 0.65f }, 2.0f, 5.5f, 1.4f, 8, 1.0f, 1.25f, { 0, 0, 0 }, 0 },
    { { 0.56f, 0.57f, 0.58f }, 3.0f, 8.0f, 0.8f, 9, 0.8f, 0.90f, { 0.02f, 0, -0.01f }, 0 },
    { { 0.64f, 0.62f, 0.60f }, 2.0f, 5.0f, 1.1f, 12, 1.0f, 1.05f, { 0.05f, 0.02f, -0.05f }, 0 },
    { { 0.62f, 0.64f, 0.62f }, 2.0f, 5.0f, 1.1f, 12, 1.0f, 1.00f, { -0.02f, 0.04f, 0 }, 0 },
    { { 0.50f, 0.55f, 0.58f }, 3.0f, 5.0f, 0.5f, 16, 1.5f, 0.70f, { 0, 0.03f, -0.04f }, 0.6f },
    { { 0.60f, 0.60f, 0.62f }, 2.5f, 5.5f, 0.7f, 14, 1.5f, 0.90f, { 0, 0, 0 }, 1.0f },
    // the Agfa family of the East: unmasked, overlapping, slow, low print density (FORMATI_E_PELLICOLE_RARE.md 1.8)
    { { 0.58f, 0.60f, 0.55f }, 3.0f, 4.5f, 0.5f, 16, 1.25f, 0.72f, { 0.02f, 0.03f, -0.03f }, 1.10f },
    { { 0.60f, 0.62f, 0.58f }, 2.8f, 5.0f, 0.6f, 14, 1.0f, 0.78f, { 0, 0.02f, -0.02f }, 0.95f },
    { { 0.58f, 0.62f, 0.60f }, 2.6f, 5.0f, 0.7f, 13, 1.0f, 0.82f, { -0.01f, 0.03f, -0.01f }, 0.80f },
    { { 0.60f, 0.60f, 0.55f }, 3.0f, 4.6f, 0.6f, 15, 1.1f, 0.78f, { 0.03f, 0.01f, -0.03f }, 1.00f } };

// Reversals: gamma (positive), toe, shoulder, interimage, grain, saturation.
struct Reversal { float g, toe, shoulder, iie, rms, sat; };
const Reversal kReversals[kCrReversalCount] = { { 1.80f, 1.0f, 2.0f, 1.8f, 8, 1.35f },
                                                { 1.60f, 1.3f, 2.5f, 1.3f, 9, 1.05f },
                                                { 1.75f, 1.0f, 2.0f, 1.2f, 9, 1.15f } };

// Black and white: gamma, toe, shoulder, grain.
struct BWStock { float g, toe, shoulder, rms; };
const BWStock kBWStocks[kCrBWStockCount] = { { 0.67f, 2.5f, 6.0f, 14 }, { 0.58f, 2.5f, 6.0f, 17 }, { 0.60f, 2.5f, 6.5f, 7 } };
// Spectral sensitivity as Rec.709 weights (sum 1), and the taking filters' transmissions.
const float kSensitivity[kCrSensitivityCount][3] = { { 0.00f, 0.12f, 0.88f }, { 0.04f, 0.52f, 0.44f },
                                                     { 0.18f, 0.46f, 0.36f }, { 0.27f, 0.48f, 0.25f } };
const float kFilters[kCrFilterCount][3] = { { 1, 1, 1 }, { 1.0f, 0.95f, 0.35f }, { 1.0f, 0.6f, 0.1f },
                                            { 1.0f, 0.15f, 0.03f }, { 0.3f, 1.0f, 0.3f } };

// Prints: contrast of the positive curve (1 = scan), cast (stops), black density.
struct Print { float contrast, cast[3], dmax; };
const Print kPrints[kCrPrintCount] = { { 1.00f, { 0, 0, 0 }, 2.0f }, { 1.45f, { 0, 0, 0 }, 3.8f },
                                       { 1.65f, { 0, 0, 0 }, 4.1f }, { 1.45f, { -0.04f, 0, 0.05f }, 3.7f },
                                       { 1.30f, { 0.01f, 0, -0.01f }, 3.5f } };

// Film formats: grain size and amount, halation radius, gate weave (share of the width), relative to Super 35.
struct Format { float grainSize, grainAmount, halRadius, weave; };
const Format kFormats[kCrFormatCount] = { { 2.6f, 1.6f, 2.2f, 0.008f }, { 1.7f, 1.3f, 1.5f, 0.0035f },
                                          { 1.0f, 1.0f, 1.0f, 0.0012f }, { 0.6f, 0.65f, 0.55f, 0.0006f },
                                          { 0.4f, 0.5f, 0.4f, 0.0004f }, { 0.35f, 0.45f, 0.4f, 0.0f },
                                          { 1.55f, 1.25f, 1.4f, 0.003f }, { 2.9f, 1.75f, 2.4f, 0.010f },
                                          { 2.0f, 1.4f, 1.8f, 0.006f }, { 1.08f, 1.04f, 1.08f, 0.0012f },
                                          { 1.3f, 1.1f, 1.3f, 0.0015f }, { 0.76f, 0.87f, 0.76f, 0.0008f } };

// Analog formats: luma MHz, peaking, chroma kHz, Y/C delay ns, luma SNR dB, jitter ns (random), slow wave ns,
// head-switching lines (of 480), colour-under (chroma shared by line pairs), dropouts per line at full wear.
struct Tape { float lumaMHz, peaking, chromaKHz, delayNs, snr, jitterNs, waveNs, headLines; int colourUnder; float drop; };
const Tape kTapes[kCrAnalogCount] = {
    { 4.6f, 0.10f, 1300, 10, 50, 0, 0, 0, 0, 0.0f },     { 3.0f, 0.60f, 400, 150, 40, 20, 250, 5, 1, 0.05f },
    { 2.6f, 0.70f, 350, 200, 38, 25, 300, 5, 1, 0.06f }, { 5.0f, 0.50f, 400, 120, 42, 20, 250, 5, 1, 0.06f },
    { 3.0f, 0.50f, 400, 150, 41, 20, 250, 5, 1, 0.05f }, { 3.4f, 0.50f, 400, 150, 40, 20, 200, 5, 1, 0.07f },
    { 5.0f, 0.50f, 400, 120, 42, 20, 200, 5, 1, 0.08f }, { 3.3f, 0.40f, 500, 80, 43, 10, 100, 4, 1, 0.04f },
    { 4.0f, 0.30f, 600, 25, 45, 5, 30, 4, 1, 0.04f },    { 4.5f, 0.20f, 1500, 20, 47, 2, 10, 0, 0, 0.02f },
    { 4.6f, 0.20f, 1300, 10, 43, 2, 10, 0, 0, 0.02f } };

// Digital cameras: stops above grey before the clip, knee (0 hard .. 1 soft), saturation, highlight desaturation,
// sharpening amount and radius (px at 1080), noise luma and chroma (stops), contrast, banding (0 none .. 1),
// chroma block (4:1:1 = 4x1, 4:2:0 = 2x2; in source pixels) and source lines (480 SD, 1080 HD).
struct Camera { float clip, knee, sat, highDesat, sharp, radius, noise, chroma, contrast, banding; int cbx, cby, lines; };
const Camera kCameras[kCrDigitalCount] = {
    { 6.5f, 0.85f, 1.00f, 0.6f, 0.05f, 0.6f, 0.02f, 0.01f, 1.15f, 0.0f, 1, 1, 1080 },
    { 3.5f, 0.25f, 1.05f, 0.2f, 0.10f, 1.0f, 0.03f, 0.02f, 1.30f, 0.5f, 2, 2, 1080 },
    { 2.5f, 0.10f, 1.15f, 0.1f, 0.50f, 1.2f, 0.02f, 0.03f, 1.35f, 0.2f, 3, 1, 1080 },
    { 2.0f, 0.05f, 1.20f, 0.1f, 0.70f, 1.3f, 0.05f, 0.03f, 1.40f, 0.4f, 4, 1, 480 },
    { 2.5f, 0.35f, 0.90f, 0.3f, 0.50f, 1.3f, 0.05f, 0.03f, 1.20f, 0.4f, 4, 1, 480 },
    { 2.5f, 0.05f, 1.25f, 0.0f, 0.70f, 1.0f, 0.06f, 0.09f, 1.40f, 0.3f, 2, 2, 768 },
    { 1.5f, 0.02f, 0.85f, 0.0f, 0.80f, 1.5f, 0.10f, 0.12f, 1.45f, 0.6f, 2, 2, 480 },
    { 1.5f, 0.02f, 0.80f, 0.0f, 0.30f, 1.0f, 0.12f, 0.14f, 1.35f, 0.7f, 2, 2, 240 },
    { 2.5f, 0.05f, 1.30f, 0.0f, 0.90f, 0.8f, 0.07f, 0.06f, 1.40f, 0.5f, 2, 2, 1080 },
    { 5.0f, 1.00f, 1.15f, 0.2f, 0.80f, 0.6f, 0.02f, 0.01f, 1.20f, 0.1f, 2, 2, 1080 } };

// ---- helpers

double clampd(double v, double a, double b) { return std::min(std::max(v, a), b); }

void mat(const float* a, const float* b, float* out)   // out = a * b (3x3)
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out[i * 3 + j] = (float)((double)a[i * 3] * b[j] + (double)a[i * 3 + 1] * b[3 + j] + (double)a[i * 3 + 2] * b[6 + j]);
}

// from gamut a to gamut b (space codes)
void convert(int a, int b, float* out) { mat(SM_XYZ_TO_RGB + b * 9, SM_RGB_TO_XYZ + a * 9, out); }

// record k's weights given over linear Rec.709, as weights over linear Rec.2020: w2020 = w709 * M(2020 -> 709)
void weights709(CreativeParams& p, int k, const float w[3])
{
    float m[9];
    convert(2, 1, m);
    for (int j = 0; j < 3; ++j) p.recW[k * 3 + j] = w[0] * m[j] + w[1] * m[3 + j] + w[2] * m[6 + j];
}

// a colour given in linear Rec.709, in linear Rec.2020
void colour709(const float c[3], float* out)
{
    float m[9];
    convert(1, 2, m);
    for (int i = 0; i < 3; ++i) out[i] = m[i * 3] * c[0] + m[i * 3 + 1] * c[1] + m[i * 3 + 2] * c[2];
}

void set3(float* d, float a, float b, float c) { d[0] = a; d[1] = b; d[2] = c; }

// split-tone vector of a hue (degrees) and an amount, in stops
void hueVector(double hue, double amount, float* out)
{
    const double h = hue * kPi / 180.0;
    for (int k = 0; k < 3; ++k) out[k] = (float)(0.35 * amount * cos(h - 2.0 * kPi * k / 3.0));
}

// a sigma (full-size px) on a grid fine enough for it: scale s with sigma / s <= 6
void glowGrid(int W, int H, float sigma, int& s, int& w, int& h, float& onGrid)
{
    s = 1;
    while (sigma / (float)s > 6.0f && s < 64) s *= 2;
    w = (W + s - 1) / s;
    h = (H + s - 1) / s;
    onGrid = sigma / (float)s;
}

}  // namespace

void creativeRecordTimes(const CreativeControls& c, double out[3])
{
    const double d = clampd(c.interval, 0, 48);
    out[0] = out[1] = out[2] = 0;
    if (c.process == kCrThreePlates) {   // blue first, then green, then red
        out[2] = 0;
        out[1] = d;
        out[0] = 2 * d;
    } else if (c.process == kCrKinemacolor) {
        out[1] = d;
    }
}

void creativePrepare(CreativeParams& p, const CreativeControls& c, int W, int H, int frame, int space, int gamma)
{
    p = CreativeParams();
    p.W = W;
    p.H = H;
    p.rowPixels = p.dstRowPixels = W;
    p.space = space;
    p.gamma = gamma;
    p.frame = frame;
    const int proc = c.process >= 0 && c.process < kCrProcessCount ? c.process : kCrColourNegative;
    convert(space, 2, p.toRef);
    convert(2, space, p.fromRef);
    convert(2, 1, p.toOut);
    const float k1080 = (float)H / 1080.0f;

    // defaults: three records = the Rec.2020 channels, a neutral curve and print, ideal dyes
    p.kind = CR_KIND_PROCESS;
    p.image = CR_IMAGE_SUB;
    p.records = 3;
    for (int k = 0; k < 3; ++k) {
        p.recW[k * 4] = 1.0f;
        p.recScale[k] = 1.0f;
        p.recGain[k] = 1.0f;
        p.slope[k] = 1.0f;
        p.printContrast[k] = 1.45f;
        p.fade[k] = 1.0f;
        p.grainLayer[k] = 1.0f;
        p.dye[k * 4] = 1.0f;
        p.area[k] = 1.0f / 3.0f;
        p.filt[k * 4] = 1.0f;
        p.tone[k] = 1.0f;
        p.paper[k] = 1.0f;
    }
    set3(p.grainLayer, 0.9f, 0.85f, 1.25f);
    p.shoulder = 7.0f;
    p.toe = 4.0f;
    p.interlayer = 1.0f;
    p.saturation = (float)clampd(c.saturation, 0, 2);
    p.density = (float)clampd(c.density, 0, 1);
    p.dmax = 3.0f;
    p.dustSign = 1.0f;
    p.output = CR_OUT_REC709;
    p.chromaBlockX = p.chromaBlockY = 1;
    p.lumaAxes = 1;
    p.viewLight = 1.0f;

    // shot: exposure, white balance as record gains
    const double t = clampd(c.temperature, -1, 1), g = clampd(c.tint, -1, 1);
    p.exposure = (float)exp2(clampd(c.exposure, -6, 6));
    const float wb[3] = { (float)exp2(0.25 * t), (float)exp2(-0.2 * g), (float)exp2(-0.25 * t) };
    p.flash = (float)(clampd(c.flash, 0, 0.3) * 0.25);
    p.vignette = (float)clampd(c.vignette, -1, 3);

    // film texture defaults (per process below)
    const Format& fmt = kFormats[c.format >= 0 && c.format < kCrFormatCount ? c.format : 2];
    float rms = 10.0f, hal = 1.0f, halThreshold = 3.0f;
    float printCast[3] = { 0, 0, 0 };
    float printContrast = 1.45f;
    const double push = clampd(c.push, -2, 3), silver = clampd(c.silver, 0, 1), age = clampd(c.age, 0, 1);
    bool film = true;          // grain, halation, weave and the print's lights apply
    bool pushes = true;        // the development (push/pull) applies

    switch (proc) {
    case kCrColourNegative: {
        const Negative& n = kNegatives[c.negative >= 0 && c.negative < kCrNegativeCount ? c.negative : 0];
        for (int k = 0; k < 3; ++k) {
            p.slope[k] = n.g[k] / 0.6f;
            p.offset[k] = n.cast[k];
        }
        p.toe = n.toe;
        p.shoulder = n.shoulder;
        p.interlayer = 0.85f + 0.25f * n.iie;
        p.saturation *= n.sat;
        rms = n.rms;
        hal = n.hal;
        if (c.negative == 2) halThreshold = 2.5f;
        if (n.dirty > 0.0f) {   // unwanted absorptions, unmasked
            const float d[9] = { 1.0f, 0.32f * n.dirty, 0.12f * n.dirty, 0.12f * n.dirty, 1.0f, 0.40f * n.dirty,
                                 0.02f * n.dirty, 0.12f * n.dirty, 1.0f };
            for (int i = 0; i < 9; ++i) p.dye[i] = d[i];
        }
        const Print& pr = kPrints[c.print >= 0 && c.print < kCrPrintCount ? c.print : 1];
        printContrast = pr.contrast;
        for (int k = 0; k < 3; ++k) printCast[k] = pr.cast[k];
        p.dmax = pr.dmax;
        p.dustSign = c.print == 0 ? 1.0f : -1.0f;   // dust on the negative prints white
        if (c.output == CR_OUT_LOG) p.output = CR_OUT_LOG;
        break;
    }
    case kCrReversal: {
        const Reversal& r = kReversals[c.reversal >= 0 && c.reversal < kCrReversalCount ? c.reversal : 0];
        p.toe = r.toe;
        p.shoulder = r.shoulder;
        p.interlayer = 0.85f + 0.25f * r.iie;
        p.saturation *= r.sat;
        printContrast = r.g / 0.885f * 0.8f;
        rms = r.rms;
        hal = 0.5f;
        p.dmax = 3.6f;
        break;
    }
    case kCrBlackWhite: {
        const BWStock& b = kBWStocks[c.bwStock >= 0 && c.bwStock < kCrBWStockCount ? c.bwStock : 0];
        const float* sens = kSensitivity[c.sensitivity >= 0 && c.sensitivity < kCrSensitivityCount ? c.sensitivity : 3];
        const float* f = kFilters[c.filter >= 0 && c.filter < kCrFilterCount ? c.filter : 0];
        float w[3] = { sens[0] * f[0], sens[1] * f[1], sens[2] * f[2] };
        const float sum = std::max(w[0] + w[1] + w[2], 1e-3f);   // the filter factor: exposure compensated
        for (float& v : w) v /= sum;
        weights709(p, 0, w);
        p.records = 1;
        p.image = CR_IMAGE_MONO;
        p.slope[0] = b.g / 0.6f;
        p.toe = b.toe;
        p.shoulder = b.shoulder;
        p.uv = c.sensitivity == 0 ? (float)clampd(c.uv, 0, 1) : 0.0f;
        printContrast = 1.5f;
        rms = b.rms;
        hal = 1.2f;
        set3(p.paper, 0.95f, 0.95f, 0.95f);
        p.dustSign = -1.0f;
        break;
    }
    case kCrTechni3: {
        const int era = c.techniEra >= 0 && c.techniEra < kCrTechniEraCount ? c.techniEra : 0;
        const float strip[3][3] = { { 0.92f, 0.08f, 0.00f }, { 0.08f, 0.84f, 0.08f }, { 0.00f, 0.06f, 0.94f } };
        const float mono[3][3] = { { 0.80f, 0.18f, 0.02f }, { 0.12f, 0.75f, 0.13f }, { 0.02f, 0.18f, 0.80f } };
        for (int k = 0; k < 3; ++k) {
            weights709(p, k, era == 2 ? mono[k] : strip[k]);
            p.slope[k] = 0.65f / 0.6f;
        }
        const float acid[9] = { 1.00f, 0.22f, 0.08f, 0.08f, 1.00f, 0.28f, 0.01f, 0.07f, 1.00f };
        const float dd = (float)clampd(c.dyeDensity, 0.6, 1.4);
        for (int i = 0; i < 9; ++i) p.dye[i] = acid[i] * dd;
        p.keyImage = (float)(c.keyImage >= 0 ? clampd(c.keyImage, 0, 1) : (era == 0 ? 0.5 : 0.0)) * 0.5f;
        p.toe = 2.0f;
        p.shoulder = 5.5f;
        p.interlayer = 1.0f;   // three separate films: no interimage
        printContrast = 1.5f;
        rms = 10.0f;
        p.dmax = 2.6f;
        break;
    }
    case kCrTechni2: {
        const float r[3] = { 0.90f, 0.10f, 0.00f }, gr[3] = { 0.05f, 0.70f, 0.25f }, none[3] = { 0, 0, 0 };
        weights709(p, 0, r);
        weights709(p, 1, gr);
        weights709(p, 2, none);
        const float dd = (float)clampd(c.dyeDensity, 0.6, 1.4);
        const float d[9] = { 1.00f * dd, 0.30f * dd, 0.45f * dd, 0.04f * dd, 0.85f * dd, 1.00f * dd, 0, 0, 0 };
        for (int i = 0; i < 9; ++i) p.dye[i] = d[i];
        p.slope[0] = p.slope[1] = 0.7f / 0.6f;
        p.interlayer = 1.0f;
        printContrast = 1.55f;
        rms = 12.0f;
        p.dmax = 2.6f;
        break;
    }
    case kCrAutochrome:
    case kCrDufay: {
        const bool grains = proc == kCrAutochrome;
        // taking and viewing filters (linear Rec.709): broad and overlapping for the starch grains
        const float ag[3][3] = { { 0.62f, 0.10f, 0.02f }, { 0.06f, 0.48f, 0.08f }, { 0.05f, 0.04f, 0.50f } };
        const float dg[3][3] = { { 0.70f, 0.08f, 0.05f }, { 0.08f, 0.55f, 0.15f }, { 0.03f, 0.08f, 0.60f } };
        const float(*f)[3] = grains ? ag : dg;
        for (int k = 0; k < 3; ++k) {
            const float sum = f[k][0] + f[k][1] + f[k][2];
            const float w[3] = { f[k][0] / sum, f[k][1] / sum, f[k][2] / sum };
            weights709(p, k, w);
            colour709(f[k], p.filt + k * 3);
        }
        if (grains) set3(p.area, 0.30f, 0.29f, 0.29f);   // 12 % carbon black between the grains
        else set3(p.area, 0.421f, 0.265f, 0.314f);
        p.image = CR_IMAGE_ADD;
        p.mosaic = grains ? CR_MOSAIC_GRAINS : CR_MOSAIC_RESEAU;
        p.mosaicTexture = 1.0f;
        p.mosaicScale = grains ? (float)(clampd(c.mosaicScale, 0.25, 4) * 2.5 * H / 2160.0)
                               : (float)(clampd(c.mosaicScale, 0.25, 4) * W / 500.0);
        p.mosaicScale = std::max(p.mosaicScale, 1.0f);
        p.clump = (float)clampd(c.clump, 0, 1);
        p.viewLight = (float)clampd(c.viewLight, 0.1, 1.2);
        p.interlayer = 1.0f;
        p.toe = 1.5f;
        p.shoulder = 3.0f;
        printContrast = 1.4f;
        rms = 6.0f;
        hal = 0.6f;
        pushes = false;
        break;
    }
    case kCrThreePlates:
    case kCrKinemacolor: {
        const bool plates = proc == kCrThreePlates;
        const float pg[3][3] = { { 0.88f, 0.12f, 0.00f }, { 0.06f, 0.82f, 0.12f }, { 0.02f, 0.16f, 0.82f } };
        const float kg[3][3] = { { 0.92f, 0.08f, 0.00f }, { 0.05f, 0.75f, 0.20f }, { 0, 0, 0 } };
        const float kf[3][3] = { { 0.85f, 0.08f, 0.03f }, { 0.10f, 0.75f, 0.40f }, { 0, 0, 0 } };
        for (int k = 0; k < 3; ++k) {
            weights709(p, k, plates ? pg[k] : kg[k]);
            if (plates) {
                p.filt[k * 4] = 1.0f;
                for (int j = 0; j < 3; ++j) if (j != k) p.filt[k * 3 + j] = 0.0f;
            } else {
                colour709(kf[k], p.filt + k * 3);
            }
        }
        if (plates) {
            float id[9];
            convert(1, 2, id);   // the Rec.709 primaries as the viewing filters
            for (int i = 0; i < 9; ++i) p.filt[(i % 3) * 3 + i / 3] = id[i];
        }
        set3(p.area, plates ? 1.0f / 3.0f : 0.5f, plates ? 1.0f / 3.0f : 0.5f, plates ? 1.0f / 3.0f : 0.0f);
        p.image = CR_IMAGE_ADD;
        p.interlayer = 1.0f;
        printContrast = plates ? 1.3f : 1.6f;
        rms = plates ? 5.0f : 12.0f;
        pushes = plates ? false : true;
        break;
    }
    case kCrDaguerreotype:
    case kCrCollodion: {
        const bool dag = proc == kCrDaguerreotype;
        const float w[3] = { 0.0f, dag ? 0.06f : 0.12f, dag ? 0.94f : 0.88f };
        weights709(p, 0, w);
        p.records = 1;
        p.image = CR_IMAGE_MONO;
        p.uv = (float)clampd(c.uv, 0, 1);
        p.slope[0] = dag ? 1.4f : 1.2f;
        p.toe = 2.0f;
        p.shoulder = 3.5f;
        printContrast = 1.25f;
        set3(p.tone, 0, 0, 0);   // the image is the deposit's own reflectance, over a dark support or mirror
        p.mirror = 1.0f;
        const double gold = clampd(c.toning, 0, 1);
        if (dag) {
            set3(p.paper, (float)(0.78 + 0.03 * gold), (float)(0.76 - 0.02 * gold), (float)(0.72 - 0.10 * gold));
            const float env = (float)(0.02 + 0.88 * (1.0 - clampd(c.viewAngle, -1, 1)) * 0.5);   // the room seen in the mirror
            set3(p.mirrorTint, env * 0.95f, env * 0.95f, env);
            p.flipX = 1;
            p.oxidation = (float)clampd(c.oxidation, 0, 1);
            printContrast += (float)(0.2 * gold);
        } else {
            set3(p.paper, 0.80f, 0.75f, 0.62f);
            if (c.collodion == 1) set3(p.mirrorTint, 0.05f, 0.035f, 0.025f);
            else set3(p.mirrorTint, 0.03f, 0.03f, 0.035f);
            p.border = (float)clampd(c.border, 0, 1);
            set3(p.borderTint, p.mirrorTint[0], p.mirrorTint[1], p.mirrorTint[2]);
        }
        rms = 3.0f;
        hal = dag ? 0.0f : 0.8f;
        film = false;
        pushes = false;
        break;
    }
    case kCrAlbumen:
    case kCrCyanotype:
    case kCrPlatinum: {
        const float blue[3] = { 0.0f, 0.12f, 0.88f }, pan[3] = { 0.27f, 0.48f, 0.25f };
        weights709(p, 0, proc == kCrAlbumen ? blue : pan);
        p.records = 1;
        p.image = CR_IMAGE_MONO;
        p.uv = proc == kCrAlbumen ? (float)clampd(c.uv, 0, 1) : 0.0f;
        const double tone = clampd(c.toning, 0, 1);
        if (proc == kCrAlbumen) {
            static const float kSalt[3] = { 0.70f, 0.95f, 1.10f }, kAlbumen[3] = { 0.80f, 0.97f, 1.05f };
            const float* k0 = c.paper == 0 ? kSalt : kAlbumen;
            for (int k = 0; k < 3; ++k) p.tone[k] = (float)(k0[k] + ((k == 0 ? 0.88 : (k == 1 ? 1.0 : 1.02)) - k0[k]) * tone);
            set3(p.paper, 0.93f, 0.90f, 0.82f);
            printContrast = 1.1f;
            p.dmax = c.paper == 0 ? 1.4f : 1.9f;
            p.slope[0] = 1.0f;
            p.shoulder = 4.0f;
        } else if (proc == kCrCyanotype) {
            for (int k = 0; k < 3; ++k) p.tone[k] = (float)((k == 0 ? 1.0 : (k == 1 ? 0.62 : 0.22)) * (1 - tone)
                                                           + (k == 0 ? 0.9 : (k == 1 ? 1.0 : 1.1)) * tone);
            set3(p.paper, 0.92f, 0.91f, 0.86f);
            printContrast = 1.6f;
            p.dmax = 1.6f;
            p.border = (float)clampd(c.border, 0, 1);
            set3(p.borderTint, 0.92f, 0.91f, 0.86f);
        } else {
            for (int k = 0; k < 3; ++k) p.tone[k] = (float)((k == 0 ? 0.98 : (k == 1 ? 1.0 : 1.04)) * (1 - tone)
                                                           + (k == 0 ? 0.78 : (k == 1 ? 0.95 : 1.12)) * tone);
            set3(p.paper, 0.94f, 0.92f, 0.86f);
            printContrast = 0.9f;
            p.dmax = 1.5f;
            p.toe = 6.0f;
            p.border = (float)clampd(c.border, 0, 1);
            set3(p.borderTint, 0.94f, 0.92f, 0.86f);
        }
        rms = 2.0f;
        hal = 0.0f;
        film = false;
        pushes = false;
        break;
    }
    case kCrAnalog: {
        const Tape& tp = kTapes[c.analog >= 0 && c.analog < kCrAnalogCount ? c.analog : 1];
        p.kind = CR_KIND_ANALOG;
        const float pxus = 36.5f * k1080;   // pixels per microsecond of a 16:9 line at this height
        const double bleed = clampd(c.colourBleed, 0, 2), wear = clampd(c.wear, 0, 1);
        p.lumaSigma = 0.1325f * pxus / tp.lumaMHz;
        p.ringSigma = 2.5f * p.lumaSigma + 0.5f;
        p.sharp = (float)(tp.peaking * (1.0 + clampd(c.sharpness, -1, 1)));
        p.chromaSigma = (float)(0.1325 * pxus / (tp.chromaKHz / 1000.0) * pow(std::max(bleed, 0.1), 0.8));
        p.chromaDelay = (int)lround(tp.delayNs / 1000.0 * pxus * bleed);
        const double snr = tp.snr - 20.0 * log10(std::max(clampd(c.noise, 0, 2), 1e-3)) - 8.0 * wear;
        p.noise = c.noise > 0 ? (float)pow(10.0, -snr / 20.0) * 2.0f : 0.0f;
        p.chromaNoise = p.noise * (float)(0.6 * std::max(bleed, 0.3));
        const double inst = clampd(c.instability, 0, 1);
        p.jitter = (float)((tp.jitterNs + tp.waveNs * 0.3) / 1000.0 * pxus * 2.0 * inst);
        const double heads = clampd(c.heads, 0, 1);
        p.headLines = tp.headLines > 0 ? (float)((2.0 + 8.0 * heads) * H / 480.0 * tp.headLines / 5.0) : 0.0f;
        p.headShift = tp.headLines > 0 ? (float)((0.5 + 4.0 * heads) * pxus * 0.5) : 0.0f;
        p.dropouts = (float)(tp.drop * wear * wear);
        p.chromaBlockY = tp.colourUnder ? std::max(1, (int)lround(2.0 * H / 480.0)) : 1;
        p.saturation *= (float)(1.0 - 0.15 * wear);
        p.contrast = 1.15f;
        p.clip = 3.0f;
        p.knee = 0.3f;
        film = false;
        pushes = false;
        break;
    }
    case kCrDigital: {
        const Camera& cam = kCameras[c.digital >= 0 && c.digital < kCrDigitalCount ? c.digital : 0];
        p.kind = CR_KIND_DIGITAL;
        p.lumaAxes = 2;
        const float src = (float)H / (float)cam.lines;   // output pixels per source pixel
        p.lumaSigma = cam.lines < H ? 0.45f * src : 0.0f;
        p.ringSigma = cam.radius * k1080;
        p.sharp = (float)(cam.sharp * clampd(1.0 + c.sharpness, 0, 2));
        p.clip = (float)(cam.clip + clampd(c.range, -3, 3));
        p.knee = (float)(0.05 + 1.2 * clampd(cam.knee + (c.highlights - 0.5), 0, 1));
        p.contrast = cam.contrast;
        p.highDesat = cam.highDesat;
        p.saturation *= cam.sat;
        p.noise = (float)(cam.noise * clampd(c.noise, 0, 2));
        p.chromaNoise = (float)(cam.chroma * clampd(c.noise, 0, 2));
        const double comp = clampd(c.compression, 0, 2);
        p.chromaBlockX = comp > 0 ? std::max(1, (int)lround(cam.cbx * src)) : 1;
        p.chromaBlockY = comp > 0 ? std::max(1, (int)lround(cam.cby * src)) : 1;
        p.levels = cam.banding * comp > 0 ? (float)(255.0 / (1.0 + 3.0 * cam.banding * comp)) : 0.0f;
        film = false;
        pushes = false;
        break;
    }
    default:
        break;
    }

    // development: push/pull and silver kept, age (film processes)
    if (pushes) {
        const bool bw = proc == kCrBlackWhite;
        for (int k = 0; k < 3; ++k) {
            p.slope[k] *= (float)(1.0 + (bw ? 0.20 : 0.12) * push);
            p.offset[k] += (float)(0.15 * push + (k == 0 ? 0.02 : (k == 2 ? -0.02 : 0.0)) * push);   // shadow speed, crossover
        }
        if (push > 0) p.flash += (float)(0.01 * push);
        p.shoulder -= (float)(0.15 * push);
        rms *= (float)pow(1.28, push);
        p.silver = (float)(silver * 0.9);
        printContrast *= (float)(1.0 + 0.4 * silver);
        rms *= (float)(1.0 + 0.3 * silver);
    }
    if (age > 0) {
        if (p.image == CR_IMAGE_SUB) set3(p.fade, (float)(1 - 0.6 * age), (float)(1 - 0.1 * age), (float)(1 - 0.4 * age));
        set3(p.stain, (float)(0.02 * age), (float)(0.04 * age), (float)(0.14 * age));
        for (int k = 0; k < 3; ++k) p.slope[k] *= (float)(1.0 - 0.15 * age);
        rms *= (float)(1.0 + 0.3 * age);
    }

    // the print's (or the positive's) lights and contrast: 1 printer point = 0.025 log E = 0.083 stop
    const float pts[3] = { (float)clampd(c.printR, -24, 24), (float)clampd(c.printG, -24, 24), (float)clampd(c.printB, -24, 24) };
    for (int k = 0; k < 3; ++k) {
        p.printOffset[k] = printCast[k] + 0.083f * pts[k];
        p.printContrast[k] = printContrast * (float)clampd(c.printContrast, 0.5, 2);
    }
    p.contrast *= (float)clampd(c.printContrast, 0.5, 2);
    for (int k = 0; k < 3; ++k) p.recGain[k] *= wb[k];
    if (p.records == 1 || p.kind != CR_KIND_PROCESS || p.image == CR_IMAGE_ADD) {
        // white balance acts on the light before the records' weights
        for (int k = 0; k < 3; ++k) p.recGain[k] = 1.0f;
        for (int k = 0; k < 3; ++k)
            for (int j = 0; j < 3; ++j) p.recW[k * 3 + j] *= wb[j];
    }

    // grain, halation, diffusion
    p.grainAmount = (float)(rms * 0.008 * fmt.grainAmount * clampd(c.grain, 0, 3)) * (film ? 1.0f : 0.5f);
    p.grainSize = (float)std::max(0.5, 1.1 * fmt.grainSize * clampd(c.grainSize, 0.25, 4) * k1080);
    p.grainColor = p.records == 1 ? 0.0f : 0.6f;
    if (p.kind != CR_KIND_PROCESS) p.grainAmount = 0.0f;
    p.halAmount = (float)(hal * 0.12 * clampd(c.halation, 0, 3));
    if (p.kind != CR_KIND_PROCESS) p.halAmount = 0.0f;
    p.halThreshold = CR_GREY * (float)exp2(halThreshold);
    set3(p.halTint, 1.0f, 0.30f, 0.06f);
    if (p.records == 1) set3(p.halTint, 0.6f, 0.6f, 0.6f);
    if (p.halAmount > 0.0f) {
        p.halSigma = 10.0f * fmt.halRadius * k1080;
        glowGrid(W, H, p.halSigma, p.hS, p.hW, p.hH, p.halSigma);
    }
    p.bloom = (float)clampd(c.bloom, 0, 1) * 0.5f;
    if (p.bloom > 0.0f) {
        p.bloomSigma = (float)(clampd(c.bloomRadius, 0.2, 10) * H / 100.0);
        glowGrid(W, H, p.bloomSigma, p.bS, p.bW, p.bH, p.bloomSigma);
    }

    // where the records are read: gate weave, breath, aberration, registration of separate records
    const double wv = film ? clampd(c.weave, 0, 1) * fmt.weave * W : 0.0;
    p.weaveX = (float)(wv * (0.7 * sin(frame * 0.9 + 1.3) + 0.3 * cr_hash(frame, 1, 71)));
    p.weaveY = (float)(0.6 * wv * (0.7 * sin(frame * 0.63 + 0.2) + 0.3 * cr_hash(frame, 2, 72)));
    const double br = clampd(c.breath, 0, 1);
    if (br > 0) {
        p.exposure *= (float)exp2(0.06 * br * cr_hash(frame, 3, 73));
        for (int k = 0; k < 3; ++k) p.recGain[k] *= (float)exp2(0.03 * br * cr_hash(frame, 4 + k, 74));
    }
    const double ca = clampd(c.aberration, 0, 1) * 0.0015;
    p.recScale[0] = (float)(1.0 + ca);
    p.recScale[2] = (float)(1.0 - ca);
    const double reg = clampd(c.registration, 0, 20) * k1080;
    if (reg > 0 && p.records > 1) {   // each strip or plate off by its own fixed amount, slowly breathing
        const float dir[3][2] = { { 0.8f, 0.3f }, { -0.2f, -0.6f }, { -0.7f, 0.5f } };
        for (int k = 0; k < 3; ++k) {
            const float osc = 1.0f + 0.2f * (float)sin(frame * 0.37 + k * 2.1);
            p.recX[k] = (float)(reg * dir[k][0] * osc);
            p.recY[k] = (float)(reg * dir[k][1] * osc);
        }
    }
    p.dust = (float)clampd(c.dust, 0, 1);
    p.scratches = film ? (float)clampd(c.scratches, 0, 1) : 0.0f;
    p.moved = (p.weaveX != 0 || p.weaveY != 0 || p.jitter > 0 || p.headShift > 0 || p.flipX || ca > 0 || reg > 0) ? 1 : 0;

    // colour
    float sh[3], hi[3];
    hueVector(c.shadowHue, clampd(c.shadowAmount, 0, 1), sh);
    hueVector(c.highHue, clampd(c.highAmount, 0, 1), hi);
    for (int k = 0; k < 3; ++k) {
        p.splitShadow[k] = sh[k];
        p.splitHigh[k] = hi[k];
    }
    p.splitBalance = (float)clampd(c.balance, -3, 3);
}

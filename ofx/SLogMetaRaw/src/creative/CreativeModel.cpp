// SPDX-License-Identifier: GPL-3.0-or-later
#include "CreativeModel.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <cmath>
#include <vector>

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
    "ORWO NC anni '70", "Svema anni '70-'80", "Gevacolor anni '50", "Diurna 50, grana finissima",
    "Tungsteno 200, media", "Fotografica 400 (negativo still)", "Fotografica ritratto 160", "Amatoriale 800, notturna",
    "Negativo anni '80 (prima dei tabulari)" };
const char* const kCrReversalNames[kCrReversalCount] = { "Ultra-satura", "Neutra", "Coloranti aggiunti in sviluppo",
                                                          "Super 8 tungsteno anni '70", "Fredda moderna da proiezione",
                                                          "Scaduta (contrasto basso)" };
const char* const kCrBWStockNames[kCrBWStockCount] = { "Cinema classica", "Veloce da reportage", "Tabulare finissima",
                                                       "Lenta 25, grana finissima", "Spinta a 3200",
                                                       "Positiva usata in ripresa (contrasto alto)" };
const char* const kCrSensitivityNames[kCrSensitivityCount] = { "Blu (prima del 1880)", "Ortocromatica",
                                                               "Pancromatica precoce", "Pancromatica moderna" };
const char* const kCrFilterNames[kCrFilterCount] = { "Nessuno", "Giallo", "Arancio", "Rosso", "Verde" };
const char* const kCrTechniEraNames[kCrTechniEraCount] = { "1935-1944 con chiave K", "1945-1955",
                                                           "Imbibizione da monopack (1955-1975)" };
const char* const kCrCollodionNames[kCrCollodionCount] = { "Ambrotipo (vetro su nero)", "Ferrotipo (ferro laccato)" };
const char* const kCrPaperNames[kCrPaperCount] = { "Carta salata", "Albumina" };
const char* const kCrPrintNames[kCrPrintCount] = { "Nessuna (scansione del negativo)", "Positivo standard",
                                                   "Positivo a contrasto alto", "Positivo freddo", "Positivo morbido",
                                                   "Positivo caldo anni '70", "Positivo Est Europa (poca densità)",
                                                   "Positivo a basso contrasto per telecinema",
                                                   "Positivo con argento trattenuto (bypass in stampa)" };
const char* const kCrGenerationNames[kCrGenerationCount] = { "Copia campione dal negativo",
                                                             "Copia di distribuzione (interpositivo e internegativo)",
                                                             "Riduzione 16 mm da 35 mm",
                                                             "Controtipo di controtipo (archivio)" };
const char* const kCrMediumNames[kCrMediumCount] = { "Nessuno (monitor Rec.709)", "Proiezione in sala (xeno)",
                                                     "Proiettore domestico 16 mm (alogena)",
                                                     "Televisore a tubo PAL (576 righe)", "Televisore a tubo NTSC (480 righe)",
                                                     "Monitor broadcast a tubo", "Console portatile LCD verde (160x144)",
                                                     "Stampa fotografica su carta",
                                                     "Proiettore Super 8 (alogena, 18 fps)",
                                                     "Televisore Trinitron (anni '90)", "Televisore al plasma (anni 2000)",
                                                     "Primo schermo LCD (anni 2000)",
                                                     "Console 8 bit: NES (1985) su tubo, composito",
                                                     "Console 8 bit: Master System (1986) su tubo, RGB",
                                                     "Console 16 bit: Mega Drive (1988) su tubo, RGB",
                                                     "Console 16 bit: Super Nintendo (1990) su tubo, RGB",
                                                     "Console 32 bit: PlayStation (1994) su tubo, RGB",
                                                     "Console 32 bit: Saturn (1994) su tubo, RGB" };
const char* const kCrScanViewNames[3] = { "Il quadro", "Il quadro con i bordi del mascherino",
                                          "Da bordo a bordo, con le perforazioni" };
const char* const kCrSoundNames[7] = { "Secondo epoca e formato", "Nessuna (copia muta)",
                                       "Ottica a densità variabile (anni '30-'40)", "Ottica ad area variabile, mono",
                                       "Ottica stereo (Dolby Stereo, dal 1976)",
                                       "Stereo + Dolby Digital, DTS, SDDS (dal 1993)",
                                       "Pista magnetica (Super 8, 16 mm)" };
const char* const kCrBaseNames[5] = { "Secondo epoca e formato", "Nitrato di cellulosa (35 mm, fino al 1951)",
                                     "Diacetato (pellicola di sicurezza, 16, 9,5 e 8 mm dal 1909)",
                                     "Triacetato (dal 1948, negativo di ripresa)", "Poliestere (Estar, dal 1955)" };
const char* const kCrGateNames[3] = { "Squadrato", "Angoli arrotondati", "Limato, aperto fino alle perforazioni" };
const char* const kCrSensorNames[kCrSensorCount] = { "Nessuno (il segnale com'è)", "3 tubi Plumbicon (anni '70)",
                                                     "3 tubi Plumbicon con bias light e ACT (anni '80)",
                                                     "3 tubi Saticon", "1 tubo Saticon a strisce (consumer)",
                                                     "1 tubo vidicon a strisce (consumer)", "1 tubo Newvicon a strisce",
                                                     "1 CCD a mosaico (consumer)", "3 CCD", "1 MOS a mosaico" };
const char* const kCrTransferNames[kCrTransferCount] = { "Nessuno", "Telecinema su VHS (casa)",
                                                         "Telecinema su Betacam SP (televisione)",
                                                         "Copia VHS di terza generazione",
                                                         "Kinescopio su 16 mm bianco e nero (anni '50)",
                                                         "Film-out su 35 mm (negativo e stampa)" };
const char* const kCrMediumInfo[kCrMediumCount] = {
    "",
    "Sala: 48 nit al buio, che appiattisce il contrasto percepito (2,4/2,6); xenon, luce che cade del 20% agli angoli, "
    "nero sollevato dalla luce diffusa, leggero sfarfallio e instabilità",
    "Proiettore di casa: lampada a 3200 K, circa 15 nit (meno colore percepito), nero della stanza, angoli scuri, "
    "sfarfallio a 18 fotogrammi, fuoco morbido ai bordi",
    "Tubo PAL di casa: gamma 2,5, nero del tubo e riflessi della stanza, bianco più freddo (7500 K), colore un po' "
    "alto, overscan del 6%; vetro curvo, 576 righe che si allargano nelle luci, maschera a fessure",
    "Tubo NTSC di casa: come il PAL, ma bianco a 9300 K (televisori giapponesi), colore spinto, overscan del 7%, 480 "
    "righe più spesse",
    "Monitor da regia: gamma 2,4 (BT.1886), D65, nero profondo, colore esatto, immagine intera; vetro quasi piatto, "
    "righe sottili, fosfori a strisce",
    "LCD riflettente 160x144 a quattro toni di verde, retinatura ordinata, griglia fra i pixel, cornice, scia",
    "Carta: bianco della carta, nero massimo (Dmax 2,1), gamut più piccolo che comprime i colori saturi, fibre, luce "
    "della lampada che cala ai bordi",
    "Proiettore Super 8 di casa: alogena a 3100 K, macchia calda al centro, sfarfallio a 18 fotogrammi con otturatore "
    "a tre pale, la griffa che fa saltare il quadro, obiettivo economico morbido, angoli del finestrino arrotondati",
    "Trinitron: griglia ad apertura (strisce continue), più luce, vetro più scuro, i due fili di smorzamento a un "
    "terzo e a due terzi dell'altezza",
    "Plasma: nero profondo, gamma circa 2,3, dithering a impulsi nelle ombre che cambia a ogni fotogramma",
    "Primo LCD: nero grigio-bluastro, pixel lenti che lasciano la scia, bianco freddo, 6 bit con bande morbide",
    "NES: 256x240 pixel, la tavolozza fissa di 54 colori che il chip genera come onda NTSC (ogni colore scelto fra "
    "quelli), uscita composita morbida; 240 righe progressive con le righe nere fra l'una e l'altra, televisore di casa",
    "Master System: 256x192 pixel, 64 colori (2 bit per canale), bordo nero sopra e sotto nelle 240 righe; RGB, tubo "
    "di casa",
    "Mega Drive: 320x224 pixel, 512 colori (3 bit per canale, con i livelli non lineari del suo DAC); RGB, 240 righe "
    "con le righe nere, tubo di casa",
    "Super Nintendo: 256x224 pixel, 32768 colori (5 bit per canale), pixel più larghi del Mega Drive; RGB, 240 righe "
    "con le righe nere, tubo di casa",
    "PlayStation: 320x240 pixel, 5 bit per canale con la retinatura ordinata 4x4 della sua GPU, che si vede nelle "
    "sfumature; RGB, 240 righe con le righe nere, tubo di casa",
    "Saturn: 320x224 pixel, 5 bit per canale senza retinatura (le sfumature fanno bande); RGB, 240 righe con le righe "
    "nere, tubo di casa" };
const char* const kCrFormatNames[kCrFormatCount] = { "Super 8", "16 mm", "Super 35", "65 mm (Todd-AO, Panavision 70)",
                                                     "IMAX 15/70", "Lastra 9x12", "Super 16", "8 mm",
                                                     "9,5 mm Pathé-Baby", "35 mm anamorfico", "Techniscope",
                                                     "VistaVision", "35 mm Academy", "135 (24x36, foto)",
                                                     "120 (6x6, foto)" };
const char* const kCrDeviceNames[kCrDeviceCount] = { "Processo a mano (tutti i procedimenti)",
                                                     "Cinepresa o fotocamera a pellicola", "Videocamera analogica",
                                                     "Cinema e video digitale", "Telefono, compatta e webcam" };
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
    { { 0.60f, 0.60f, 0.55f }, 3.0f, 4.6f, 0.6f, 15, 1.1f, 0.78f, { 0.03f, 0.01f, -0.03f }, 1.00f },
    // generic modern stocks by speed and use (PELLICOLE_E_SVILUPPO.md): grain grows about with the square root of speed
    { { 0.62f, 0.62f, 0.63f }, 3.0f, 7.5f, 1.1f, 4.5f, 0.9f, 1.05f, { 0, 0, 0 }, 0 },
    { { 0.60f, 0.60f, 0.61f }, 3.0f, 7.0f, 1.0f, 8, 1.0f, 1.00f, { 0.01f, 0, -0.01f }, 0 },
    { { 0.60f, 0.62f, 0.62f }, 2.5f, 6.5f, 1.2f, 12, 1.0f, 1.10f, { 0.02f, 0.01f, -0.03f }, 0 },
    { { 0.55f, 0.56f, 0.57f }, 3.5f, 8.0f, 0.9f, 7, 0.9f, 0.92f, { 0.02f, 0, -0.02f }, 0 },
    { { 0.58f, 0.60f, 0.62f }, 2.0f, 5.5f, 1.0f, 18, 1.2f, 1.00f, { 0.03f, 0.02f, -0.04f }, 0 },
    { { 0.62f, 0.63f, 0.62f }, 2.5f, 6.0f, 1.0f, 14, 1.1f, 1.00f, { 0.02f, 0.01f, -0.02f }, 0.2f } };

// Reversals: gamma (positive), toe, shoulder, interimage, grain, saturation.
struct Reversal { float g, toe, shoulder, iie, rms, sat; };
const Reversal kReversals[kCrReversalCount] = { { 1.80f, 1.0f, 2.0f, 1.8f, 8, 1.35f },
                                                { 1.60f, 1.3f, 2.5f, 1.3f, 9, 1.05f },
                                                { 1.75f, 1.0f, 2.0f, 1.2f, 9, 1.15f },
                                                { 1.70f, 1.0f, 2.0f, 1.2f, 12, 1.10f },
                                                { 1.65f, 1.2f, 2.4f, 1.4f, 7, 1.20f },
                                                { 1.35f, 1.5f, 2.0f, 0.9f, 11, 0.85f } };

// Black and white: gamma, toe, shoulder, grain.
struct BWStock { float g, toe, shoulder, rms; };
const BWStock kBWStocks[kCrBWStockCount] = { { 0.67f, 2.5f, 6.0f, 14 }, { 0.58f, 2.5f, 6.0f, 17 }, { 0.60f, 2.5f, 6.5f, 7 },
                                             { 0.70f, 2.5f, 6.5f, 5 },  { 0.55f, 2.0f, 5.5f, 24 }, { 1.00f, 1.5f, 4.0f, 6 } };
// Spectral sensitivity as Rec.709 weights (sum 1), and the taking filters' transmissions.
const float kSensitivity[kCrSensitivityCount][3] = { { 0.00f, 0.12f, 0.88f }, { 0.04f, 0.52f, 0.44f },
                                                     { 0.18f, 0.46f, 0.36f }, { 0.27f, 0.48f, 0.25f } };
const float kFilters[kCrFilterCount][3] = { { 1, 1, 1 }, { 1.0f, 0.95f, 0.35f }, { 1.0f, 0.6f, 0.1f },
                                            { 1.0f, 0.15f, 0.03f }, { 0.3f, 1.0f, 0.3f } };

// Prints: contrast of the positive curve (1 = scan), cast (stops), black density.
// Prints: contrast of the positive curve (1 = scan), cast (stops), black density, silver kept in the print.
struct Print { float contrast, cast[3], dmax, silver; };
const Print kPrints[kCrPrintCount] = { { 1.00f, { 0, 0, 0 }, 2.0f, 0 },          { 1.45f, { 0, 0, 0 }, 3.8f, 0 },
                                       { 1.65f, { 0, 0, 0 }, 4.1f, 0 },          { 1.45f, { -0.04f, 0, 0.05f }, 3.7f, 0 },
                                       { 1.30f, { 0.01f, 0, -0.01f }, 3.5f, 0 }, { 1.40f, { 0.03f, 0, -0.04f }, 3.6f, 0 },
                                       { 1.30f, { 0.01f, 0.02f, -0.02f }, 3.0f, 0 }, { 1.10f, { 0, 0, 0 }, 3.2f, 0 },
                                       { 1.50f, { 0.25f, 0.25f, 0.25f }, 4.4f, 0.3f } };   // printed lighter, as labs did

// Generations between the camera negative and the print: grain each duplicate adds (share of the camera's, in its
// size), the contrast and saturation they accumulate, the print's grain size (a 16 mm print is enlarged more) and
// the sharpness each optical or contact step loses (Gaussian sigma, px at 1080).
struct Generation { float grain, size, contrast, sat, printSize, soft; };
const Generation kGenerations[kCrGenerationCount] = { { 0.00f, 1.0f, 1.00f, 1.00f, 1.0f, 0.0f },
                                                      { 0.35f, 0.8f, 1.05f, 0.95f, 1.0f, 0.6f },
                                                      { 0.45f, 1.6f, 1.08f, 0.92f, 1.8f, 1.3f },
                                                      { 0.80f, 1.0f, 1.15f, 0.85f, 1.2f, 1.0f } };

// Viewing media (VIDEO_ANALOGICO.md, PELLICOLE_E_SVILUPPO.md): kind, scan lines, glass curvature, beam sigma (in
// lines) dark and bright, phosphor mask amount and type (0 stripes, 1 slots) and triads across the width, light
// falloff to the corners, flicker, black (flare or paper Dmax), white (lamp or paper), unsteadiness (share of the
// height), lens softness (px at 1080), corner radius and edge softness (share of the height), glow of the glass or
// the lens around bright areas (share of the light, radius as share of the height), and an LCD's slow response
// (share of the previous frame still on the screen).
// Then the response, in the panel's units (creativeLoadMedium): device gamma, contrast of the device alone (x:1),
// ambient light on the screen or paper (share of white), peak luminance (nit), white (K), colour (%, the set's
// knob), overscan (%), room (0 dark, 1 dim, 2 lit) and a chroma limit (0 none: paper's gamut). Estimates [gen] from
// docs/CREATIVE.md: CRTs 2.35-2.55 gamma and 100-200 nit, consumer sets cooler (PAL ~7500 K, Japanese NTSC 9300 K)
// with colour turned up and 5-8% overscan; cinema 48 nit (14 fL) in the dark, xenon ~6000 K; home projection ~15 nit
// under a 3200 K lamp; photo paper Dmax ~2.1 under ~130 nit of room light.
struct Medium { int kind, lines; float curve, beamMin, beamMax, mask; int maskType; float triads, falloff, flicker,
                white[3], unsteady, soft, corner, edge, glow, glowRadius, ghost;
                float gamma, contrast, ambient, nits, kelvin, colour, overscan; int room; float chroma;
                float damper, plasma, levels, blackTint[3];   // blackTint 0: neutral
                int console; };                               // kConsoles, 0: none
const Medium kMedia[kCrMediumCount] = {
    { CR_MEDIUM_NONE, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 1, 1, 1 }, 0, 0, 0, 0, 0, 0, 0,
      2.4f, 2000, 0, 100, 6500, 100, 0, 1, 0 },
    { CR_MEDIUM_SCREEN, 0, 0, 0, 0, 0, 0, 0, 0.20f, 0.012f, { 1, 1, 1 }, 0.0012f, 0, 0.010f, 0.003f, 0.015f, 0.02f, 0,
      2.4f, 2000, 0.004f, 48, 6000, 100, 0, 0, 0 },
    { CR_MEDIUM_SCREEN, 0, 0, 0, 0, 0, 0, 0, 0.50f, 0.050f, { 1, 1, 1 }, 0.004f, 1.2f, 0.03f, 0.006f, 0.030f, 0.03f, 0,
      2.4f, 300, 0.012f, 15, 3200, 100, 0, 0, 0 },
    { CR_MEDIUM_SCREEN, 576, 0.06f, 0.22f, 0.45f, 0.35f, 1, 640, 0.12f, 0, { 1, 1, 1 }, 0, 0.8f, 0.05f, 0.004f, 0.060f,
      0.015f, 0, 2.5f, 300, 0.020f, 120, 7500, 105, 6, 2, 0 },
    { CR_MEDIUM_SCREEN, 480, 0.06f, 0.22f, 0.45f, 0.35f, 1, 560, 0.12f, 0, { 1, 1, 1 }, 0, 0.9f, 0.05f, 0.004f, 0.060f,
      0.015f, 0, 2.5f, 300, 0.020f, 150, 9300, 112, 7, 2, 0 },
    { CR_MEDIUM_SCREEN, 576, 0.02f, 0.18f, 0.38f, 0.25f, 0, 900, 0.06f, 0, { 1, 1, 1 }, 0, 0.4f, 0.02f, 0.002f, 0.030f,
      0.010f, 0, 2.4f, 2000, 0.004f, 100, 6500, 100, 0, 1, 0 },
    { CR_MEDIUM_LCD, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 1, 1, 1 }, 0, 0, 0, 0, 0, 0, 0.45f,
      2.4f, 2000, 0, 100, 6500, 100, 0, 2, 0 },
    { CR_MEDIUM_PAPER, 0, 0, 0, 0, 0, 0, 0, 0.08f, 0, { 0.90f, 0.89f, 0.86f }, 0, 0, 0, 0, 0, 0, 0,
      2.4f, 125, 0, 130, 6500, 92, 0, 2, 0.55f },
    // Super 8 at home (research DIGITALI_OTTICHE.md C.7, [gen]): a hot halogen lamp, strong hot spot, 18 fps on a
    // three-blade shutter, a lot of jitter, a soft cheap lens, the gate's rounded corners
    { CR_MEDIUM_SCREEN, 0, 0, 0, 0, 0, 0, 0, 0.70f, 0.080f, { 1, 1, 1 }, 0.008f, 1.8f, 0.06f, 0.008f, 0.040f, 0.03f, 0,
      2.4f, 200, 0.015f, 12, 3100, 95, 0, 0, 0 },
    // Trinitron (C.4): aperture grille (continuous stripes), brighter, darker glass, the two damper wires
    { CR_MEDIUM_SCREEN, 576, 0.03f, 0.20f, 0.42f, 0.30f, 0, 700, 0.10f, 0, { 1, 1, 1 }, 0, 0.6f, 0.03f, 0.003f, 0.050f,
      0.012f, 0, 2.4f, 500, 0.010f, 150, 7000, 105, 4, 2, 0, 0.25f },
    // plasma (C.5): deep black, pulse dithering in the shadows, gamma about 2.3
    { CR_MEDIUM_SCREEN, 0, 0, 0, 0, 0, 0, 0, 0.02f, 0, { 1, 1, 1 }, 0, 0, 0, 0.002f, 0, 0, 0,
      2.3f, 3000, 0.004f, 150, 6500, 102, 2, 2, 0, 0, 1 },
    // an early LCD (C.6): grey-blue black, slow pixels, cold white, 6 bits
    { CR_MEDIUM_SCREEN, 0, 0, 0, 0, 0, 0, 0, 0.06f, 0, { 1, 1, 1 }, 0, 0, 0, 0.002f, 0, 0, 0.35f,
      2.2f, 500, 0.012f, 200, 8000, 95, 0, 2, 0, 0, 0, 64, { 0.85f, 0.95f, 1.25f } },
    // home consoles on the PAL set of the house (its response), in their 240 progressive lines: one line per row of
    // the console, so the dark gaps between lines show. The NES only had composite (softer along the line); the
    // others went out in RGB over SCART in Europe. kConsoles holds the frame and the colours.
    { CR_MEDIUM_SCREEN, 240, 0.06f, 0.22f, 0.45f, 0.35f, 1, 640, 0.12f, 0, { 1, 1, 1 }, 0, 1.4f, 0.05f, 0.004f, 0.060f,
      0.015f, 0, 2.5f, 300, 0.020f, 120, 7500, 105, 6, 2, 0, 0, 0, 0, { 0, 0, 0 }, 1 },
    { CR_MEDIUM_SCREEN, 240, 0.06f, 0.22f, 0.45f, 0.35f, 1, 640, 0.12f, 0, { 1, 1, 1 }, 0, 0.5f, 0.05f, 0.004f, 0.060f,
      0.015f, 0, 2.5f, 300, 0.020f, 120, 7500, 105, 6, 2, 0, 0, 0, 0, { 0, 0, 0 }, 2 },
    { CR_MEDIUM_SCREEN, 240, 0.06f, 0.22f, 0.45f, 0.35f, 1, 640, 0.12f, 0, { 1, 1, 1 }, 0, 0.5f, 0.05f, 0.004f, 0.060f,
      0.015f, 0, 2.5f, 300, 0.020f, 120, 7500, 105, 6, 2, 0, 0, 0, 0, { 0, 0, 0 }, 3 },
    { CR_MEDIUM_SCREEN, 240, 0.06f, 0.22f, 0.45f, 0.35f, 1, 640, 0.12f, 0, { 1, 1, 1 }, 0, 0.5f, 0.05f, 0.004f, 0.060f,
      0.015f, 0, 2.5f, 300, 0.020f, 120, 7500, 105, 6, 2, 0, 0, 0, 0, { 0, 0, 0 }, 4 },
    { CR_MEDIUM_SCREEN, 240, 0.06f, 0.22f, 0.45f, 0.35f, 1, 640, 0.12f, 0, { 1, 1, 1 }, 0, 0.5f, 0.05f, 0.004f, 0.060f,
      0.015f, 0, 2.5f, 300, 0.020f, 120, 7500, 105, 6, 2, 0, 0, 0, 0, { 0, 0, 0 }, 5 },
    { CR_MEDIUM_SCREEN, 240, 0.06f, 0.22f, 0.45f, 0.35f, 1, 640, 0.12f, 0, { 1, 1, 1 }, 0, 0.5f, 0.05f, 0.004f, 0.060f,
      0.015f, 0, 2.5f, 300, 0.020f, 120, 7500, 105, 6, 2, 0, 0, 0, 0, { 0, 0, 0 }, 6 } };

// Home consoles (emulator and hardware references, [gen] where noted): frame, lines, colours (CreativeParams.cKind)
// and the PlayStation's dither. Mega Drive DAC levels as measured on the hardware (normal mode, in 8-bit code values,
// as emulators use them); Master System 2 bits, SNES, PlayStation and Saturn 5 bits per channel, even steps.
struct Console { int cols, rows, kind, n, dither; float levels[8]; };
const Console kConsoles[] = { { 0, 0, 0, 0, 0, { 0 } },
                              { 256, 240, 1, 64, 0, { 0 } },   // NES: the palette, built by nesPalette
                              { 256, 192, 2, 4, 0, { 0 } },
                              { 320, 224, 3, 8, 0, { 0, 52, 87, 116, 144, 172, 206, 255 } },
                              { 256, 224, 2, 32, 0, { 0 } },
                              { 320, 240, 2, 32, 1, { 0 } },
                              { 320, 224, 2, 32, 0, { 0 } } };

// The NES palette as the 2C02 makes it: no colour table, a square wave between two voltages (per brightness level)
// whose phase is the hue, decoded by the television. Levels of the 2C02 as measured (volts, black 0.518, white
// 1.962); hue 0 grey at the high level, 13 grey at the low, 14 and 15 black; 12 samples per colour cycle, the burst
// at hue 8 (phase offset 4 samples puts 0x16 on red, 0x28 on yellow, 0x21 on light blue, as on the hardware), FCC
// YIQ to RGB. The result is a code value, as the set shows it.
static void nesPalette(float out[192])
{
    const double lo[4] = { 0.350, 0.518, 0.962, 1.550 }, hi[4] = { 1.094, 1.506, 1.962, 1.962 };
    const double black = 0.518, white = 1.962;
    for (int c = 0; c < 64; ++c) {
        const int h = c & 15, l = (c >> 4) & 3;
        double y = 0, i = 0, q = 0;
        if (h < 14) {
            const double vh = h == 13 ? lo[l] : hi[l], vl = h == 0 ? hi[l] : lo[l];
            for (int s = 0; s < 12; ++s) {
                const double v = ((h + s) % 12 < 6 ? vh : vl) - black, a = 3.14159265358979 / 6.0 * (s + 4);
                const double n = v / (white - black);
                y += n / 12.0;
                i += n * std::cos(a) / 6.0;
                q += n * std::sin(a) / 6.0;
            }
        }
        const double rgb[3] = { y + 0.946882 * i + 0.623557 * q, y - 0.274788 * i - 0.635691 * q,
                                y - 1.108545 * i + 1.709007 * q };
        for (int k = 0; k < 3; ++k) out[c * 3 + k] = (float)std::min(std::max(rgb[k], 0.0), 1.0);
    }
}

// The film strip of each format (kCrFormatNames order), in mm (SMPTE and ISO dimensions; [gen] where marked): travel
// across the output (1: horizontal, stills and VistaVision, IMAX) or down; film width; the camera aperture across and
// along; its centre's offset across from the film's centre (towards the side away from a single row of perforations);
// frame pitch; perforations: sides (0 none, 1 one side, 2 both, 3 between the frames), size across and along, corner
// radius, centre across from the film's centre, pitch, offset along from the frame's centre. BH (negative) 2.794 x
// 1.854 with curved sides, KS 2.794 x 1.981, 16 mm 1.829 x 1.270, Super 8 0.914 x 1.143.
struct FilmStrip { int travelH; float filmA, apA, apL, apOff, pitch; int sides; float perfA, perfL, perfR, perfX,
                   perfPitch, perfPhase; };
const FilmStrip kStrips[kCrFormatCount] = {
    { 0, 7.976f, 5.79f, 4.01f, 0.38f, 4.234f, 1, 0.914f, 1.143f, 0.13f, 3.02f, 4.234f, 0 },   // Super 8 [gen phase]
    { 0, 16.0f, 10.26f, 7.49f, 0, 7.62f, 2, 1.829f, 1.27f, 0.25f, 6.2f, 7.62f, 3.81f },        // 16 mm double perf
    { 0, 35.0f, 24.89f, 18.66f, 0, 19.0f, 2, 2.794f, 1.854f, 0.9f, 14.09f, 4.75f, 2.375f },    // Super 35, 4 perf
    { 0, 65.0f, 52.48f, 23.01f, 0, 23.75f, 2, 2.794f, 1.981f, 0.5f, 29.9f, 4.75f, 0 },         // 65 mm, 5 perf
    { 1, 70.0f, 52.63f, 70.41f, 0, 71.25f, 2, 2.794f, 1.981f, 0.5f, 32.1f, 4.75f, 0 },         // IMAX 15/70
    { 1, 90.0f, 86.0f, 116.0f, 0, 10000.0f, 0, 0, 0, 0, 0, 1, 0 },                              // plate 9x12
    { 0, 16.0f, 12.52f, 7.41f, 1.0f, 7.62f, 1, 1.829f, 1.27f, 0.25f, 6.2f, 7.62f, 3.81f },     // Super 16
    { 0, 8.0f, 4.88f, 3.68f, 1.13f, 3.81f, 1, 1.829f, 1.27f, 0.25f, 2.23f, 3.81f, 1.905f },    // 8 mm
    { 0, 9.5f, 8.5f, 6.5f, 0, 7.54f, 3, 2.0f, 1.0f, 0.2f, 0, 7.54f, 3.77f },                    // 9.5 mm [gen perf]
    { 0, 35.0f, 21.95f, 18.59f, 1.3f, 19.0f, 2, 2.794f, 1.854f, 0.9f, 14.09f, 4.75f, 2.375f }, // anamorphic
    { 0, 35.0f, 22.0f, 9.47f, 1.3f, 9.5f, 2, 2.794f, 1.854f, 0.9f, 14.09f, 4.75f, 2.375f },    // Techniscope 2 perf
    { 1, 35.0f, 25.17f, 37.72f, 0, 38.0f, 2, 2.794f, 1.854f, 0.9f, 14.09f, 4.75f, 2.375f },    // VistaVision 8 perf
    { 0, 35.0f, 22.0f, 16.03f, 1.3f, 19.0f, 2, 2.794f, 1.854f, 0.9f, 14.09f, 4.75f, 2.375f },  // Academy [gen offset]
    { 1, 35.0f, 24.0f, 36.0f, 0, 38.0f, 2, 2.794f, 1.981f, 0.5f, 14.09f, 4.75f, 2.375f },      // 135, 8 perf
    { 1, 61.5f, 56.0f, 56.0f, 0, 62.0f, 0, 0, 0, 0, 0, 1, 0 } };                                // 120 6x6

// A white of Kelvin k on a D65 screen, as Rec.709 gains (largest 1), with the eye adapted most of the way: a 3200 K
// lamp reads warm, not orange (adaptation share [gen]). Planckian locus: Krystek 1985 (as in math/HostOnly.h).
static void kelvinWhite(double k, float out[3])
{
    auto rgb = [](double t, double o[3]) {
        t = std::min(std::max(t, 1500.0), 15000.0);
        const double u = (0.860117757 + 1.54118254e-4 * t + 1.28641212e-7 * t * t)
                         / (1.0 + 8.42420235e-4 * t + 7.08145163e-7 * t * t);
        const double v = (0.317398726 + 4.22806245e-5 * t + 4.20481691e-8 * t * t)
                         / (1.0 - 2.89741816e-5 * t + 1.61456053e-7 * t * t);
        const double d = 2.0 * u - 8.0 * v + 4.0, x = 3.0 * u / d, y = 2.0 * v / d;
        const double X = x / y, Z = (1.0 - x - y) / y;
        o[0] = 3.2404542 * X - 1.5371385 - 0.4985314 * Z;
        o[1] = -0.9692660 * X + 1.8760108 + 0.0415560 * Z;
        o[2] = 0.0556434 * X - 0.2040259 + 1.0572252 * Z;
    };
    double w[3], d65[3];
    rgb(k, w);
    rgb(6504, d65);
    double g[3], top = 0;
    for (int i = 0; i < 3; ++i) {
        g[i] = std::pow(std::max(w[i] / d65[i], 1e-3), 0.4);   // 40% of the cast left after adaptation
        top = std::max(top, g[i]);
    }
    for (int i = 0; i < 3; ++i) out[i] = (float)(g[i] / top);
}


// Film formats: grain size and amount, halation radius, gate weave (share of the width), relative to Super 35.
struct Format { float grainSize, grainAmount, halRadius, weave; };
const Format kFormats[kCrFormatCount] = { { 2.6f, 1.6f, 2.2f, 0.008f }, { 1.7f, 1.3f, 1.5f, 0.0035f },
                                          { 1.0f, 1.0f, 1.0f, 0.0012f }, { 0.6f, 0.65f, 0.55f, 0.0006f },
                                          { 0.4f, 0.5f, 0.4f, 0.0004f }, { 0.35f, 0.45f, 0.4f, 0.0f },
                                          { 1.55f, 1.25f, 1.4f, 0.003f }, { 2.9f, 1.75f, 2.4f, 0.010f },
                                          { 2.0f, 1.4f, 1.8f, 0.006f }, { 1.08f, 1.04f, 1.08f, 0.0012f },
                                          { 1.3f, 1.1f, 1.3f, 0.0015f }, { 0.76f, 0.87f, 0.76f, 0.0008f },
                                          { 1.1f, 1.05f, 1.1f, 0.0013f },  { 0.82f, 0.9f, 0.8f, 0.0f },
                                          { 0.54f, 0.74f, 0.55f, 0.0f } };

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

// The same formats as the player emulates them (VIDEO_EMULAZIONE.md §4.4, values to calibrate): playback low-pass
// at -3 dB (MHz) and its Q pair (Butterworth or Bessel, 4th order), pre-emphasis time constant (µs, 0 = none) and HF
// gain, white clip (1 = white, black clip at 1 - clip), colour low-pass (kHz) and its delay (ns), unweighted luma
// SNR (dB), colour-under frequency (kHz, 0 = colour on its own channel: no hue noise), crispener, comb (field lines),
// timebase after the TV's lock: slow wave and scrape (ns), head switch step (µs), flagging (ns), dropouts per field
// line at the format's typical wear.
struct TapeEmu { float lpMHz, q1, q2, emphUs, emphG, clip, chromaKHz, delayNs, snr, fcuKHz, crisp; int comb;
                 float waveNs, scrapeNs, headUs, flagNs, drop; };
const TapeEmu kTapeEmu[kCrAnalogCount] = {
    { 5.0f, 0.5219f, 0.8055f, 0, 0, 2.0f, 1300, 0, 52, 0, 0.1f, 0, 0, 0, 0, 0, 0 },                 // broadcast live
    { 2.1f, 0.5412f, 1.3066f, 1.3f, 4, 1.6f, 400, 220, 39, 627, 0.45f, 1, 60, 8, 1.5f, 80, 0.004f },  // VHS SP
    { 1.8f, 0.5412f, 1.3066f, 1.3f, 4, 1.6f, 350, 260, 37, 627, 0.5f, 1, 80, 12, 2.0f, 100, 0.006f }, // VHS LP
    { 4.2f, 0.5412f, 1.3066f, 1.3f, 4, 2.1f, 400, 160, 42, 627, 0.3f, 1, 50, 8, 1.2f, 70, 0.004f },   // S-VHS
    { 2.3f, 0.5412f, 1.3066f, 1.3f, 2.5f, 1.6f, 400, 200, 40, 688, 0.4f, 1, 50, 8, 1.2f, 70, 0.004f },// Betamax
    { 2.5f, 0.5412f, 1.3066f, 1.3f, 4, 1.6f, 400, 200, 40, 743, 0.4f, 1, 40, 6, 1.0f, 60, 0.006f },   // Video8
    { 4.0f, 0.5412f, 1.3066f, 0.47f, 4, 2.2f, 450, 160, 42, 743, 0.3f, 1, 40, 6, 1.0f, 60, 0.008f },  // Hi8
    { 3.0f, 0.5412f, 1.3066f, 0.6f, 3, 1.8f, 500, 60, 44, 686, 0.2f, 1, 30, 5, 0.8f, 40, 0.003f },    // U-matic
    { 3.8f, 0.5412f, 1.3066f, 0.8f, 3, 2.0f, 600, 40, 45, 686, 0.2f, 1, 25, 4, 0.6f, 30, 0.002f },    // U-matic SP
    { 4.5f, 0.5219f, 0.8055f, 0.3f, 2, 2.5f, 1500, 20, 48, 0, 0.1f, 0, 2, 1, 0, 0, 0.001f },          // Betacam SP
    { 4.6f, 0.5412f, 1.3066f, 0.24f, 3, 2.5f, 1300, 10, 45, 0, 0.1f, 0, 2, 1, 0, 0, 0.002f } };       // 1" type C

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

double smoothstepd(double a, double b, double x)
{
    const double t = clampd((x - a) / (b - a), 0, 1);
    return t * t * (3 - 2 * t);
}

// split-tone vector of a hue (degrees) and an amount, in stops
void hueVector(double hue, double amount, float* out)
{
    const double h = hue * kPi / 180.0;
    for (int k = 0; k < 3; ++k) out[k] = (float)(0.35 * amount * cos(h - 2.0 * kPi * k / 3.0));
}

// a sigma (full-size px) on a grid fine enough for it: scale s with sigma / s <= 3
void glowGrid(int W, int H, float sigma, int& s, int& w, int& h, float& onGrid)
{
    s = 1;
    while (sigma / (float)s > 3.0f && s < 64) s *= 2;   // a wide glow is smooth: 3 cells of sigma suffice
    w = (W + s - 1) / s;
    h = (H + s - 1) / s;
    onGrid = sigma / (float)s;
}

// The tape's line-by-line emulation for a W x H frame (cr_tape_row, cr_tape_out): filters in pixels of this width
// (a 52 µs active line), FM noise calibrated so a grey line comes out with the format's SNR.
void tapeEmulation(CreativeParams& p, const TapeEmu& te, int W, int H, double bleed, double wear, double inst,
                   double heads, double noise, double sharp)
{
    const double pxus = std::max(W, 8) / 52.0;   // samples per µs
    const double tau = 2.0 * kPi;
    p.tape = 1;
    p.fieldLines = 288;
    p.emphG = te.emphUs > 0 ? te.emphG : 0.0f;
    p.emphK = te.emphUs > 0 ? (float)(1.0 - exp(-1.0 / (te.emphUs * pxus))) : 1.0f;
    p.clipHi = te.clip;
    p.clipLo = 1.0f - te.clip;
    const double fc = std::min(te.lpMHz / pxus, 0.45);   // cycles per sample
    const double q[2] = { te.q1, te.q2 };
    for (int s = 0; s < 2; ++s) {   // RBJ low-pass biquads
        const double w0 = tau * fc, al = sin(w0) / (2.0 * q[s]), cw = cos(w0), a0 = 1.0 + al;
        p.lp[s * 5 + 0] = (float)((1.0 - cw) / 2.0 / a0);
        p.lp[s * 5 + 1] = (float)((1.0 - cw) / a0);
        p.lp[s * 5 + 2] = (float)((1.0 - cw) / 2.0 / a0);
        p.lp[s * 5 + 3] = (float)(-2.0 * cw / a0);
        p.lp[s * 5 + 4] = (float)((1.0 - al) / a0);
    }
    p.crispK = (float)(1.0 - exp(-tau * std::min(2.5 / pxus, 0.45)));
    p.crisp = (float)(te.crisp * (1.0 + sharp));
    // colour: three one-poles, -3 dB at chromaKHz together (each at 1 / 0.5098 of it), narrower with more bleed
    const double cfc = te.chromaKHz / 1000.0 / std::max(bleed, 0.1) / 0.5098 / pxus;
    p.chromaK = (float)(1.0 - exp(-tau * std::min(cfc, 0.45)));
    p.delayPx = (float)(te.delayNs / 1000.0 * pxus * bleed);
    const int rowsPerLine = std::max(1, (int)lround((double)H / (2.0 * p.fieldLines)));
    p.combRows = te.comb ? 2 * rowsPerLine : 0;   // the same field's previous line
    p.docRows = 2 * rowsPerLine;
    // rad per px of timebase error, of which the burst lock (APC) leaves about a third
    p.hueGain = te.fcuKHz > 0 ? (float)(0.3 * tau * te.fcuKHz / 1000.0 / pxus) : 0.0f;
    p.huePhase = te.fcuKHz > 0 ? (float)(0.03 + 0.12 * wear) : 0.0f;
    p.chromaLimit = 0.45f;
    // timebase (px): ns -> px
    p.tbWave = (float)(te.waveNs / 1000.0 * pxus * inst);
    p.tbWaveLines = 9.0f;
    p.tbScrape = (float)(te.scrapeNs / 1000.0 * pxus * inst * 0.6);
    p.headStep = (float)(te.headUs * pxus * heads);
    p.headRows = 7.0f;   // 6.5 H before the vertical sync
    p.headTau = 2.5f;
    p.flagAmp = (float)(te.flagNs / 1000.0 * pxus * inst);
    p.flagRows = 14.0f;
    p.headNoise = te.headUs > 0 ? (float)(0.08 * heads) : 0.0f;
    p.dropDensity = (float)(te.drop * (0.3 + 6.0 * wear * wear));
    p.dropSlide = 1.5f;
    p.dropLife = 6.0f;
    // FM noise: a flat grey line through the same filters, then the amplitude that gives the SNR (minus the wear's)
    const double snr = te.snr - 8.0 * wear;
    p.fmNoise = 0.0f;
    if (noise > 0) {
        CreativeParams t = p;
        const int n = 2048;
        t.W = n;
        t.H = 1;
        t.fieldLines = 1;
        t.fmNoise = 1.0f;
        t.huePhase = t.hueGain = 0.0f;
        t.chromaSigmaN = t.chromaAM = 0.0f;
        t.tbWave = t.tbScrape = t.headStep = t.flagAmp = 0.0f;
        std::vector<float> in(n, 0.5f), zero(n, 0.0f), yo(n), cbo(n), cro(n);
        cr_tape_row(in.data(), zero.data(), zero.data(), yo.data(), cbo.data(), cro.data(), 0, t);
        double sum = 0, sq = 0;
        for (int i = 256; i < n; ++i) {
            sum += yo[i];
            sq += yo[i] * yo[i];
        }
        const double m = sum / (n - 256), sigma = sqrt(std::max(sq / (n - 256) - m * m, 1e-12));
        p.fmNoise = (float)(pow(10.0, -snr / 20.0) / sigma * noise);
    }
    // colour noise: about 3 dB under the luma's, after one of its low-passes
    const double k = p.chromaK, gain = sqrt(k / (2.0 - k));
    p.chromaSigmaN = (float)(pow(10.0, -(snr - 3.0) / 20.0) / std::max(gain, 1e-3) * 0.5 * noise);
    p.chromaAM = (float)((0.02 + 0.1 * wear) / std::max(gain, 1e-3) * noise);
}


// The sensors (kCrSensorNames). Tubes: lag K and beam capacity B per channel (units of the peak white's charge per
// field), target gamma, bias light, the overload kept (multiple of B; an ACT gun clamps it), green highlights of a
// striped tube; CCD: smear (share of the light above the full well spread down the column), one chip or three; MOS:
// fixed pattern; false colour of stripes or mosaic and its carrier (MHz). Research VIDEO_EMULAZIONE.md 1.1-1.2:
// lag and capacities from the data books [doc], the rest estimates to calibrate. The smear is far above a real
// chip's (-80 dB and less): the picture's highlights arrive clipped, so it stands for the sun and the headlights.
struct SensorDef { int type; float K[3], B[3], gamma, bias, qmax, green, smear, mono, fpn, fc, fcMHz; };
const SensorDef kSensors[kCrSensorCount] = {
    { 0, { 0, 0, 0 }, { 1, 1, 1 }, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { CR_SENSOR_TUBE, { 0.023f, 0.02f, 0.028f }, { 1.6f, 2, 2 }, 0.95f, 0, 12, 0, 0, 0, 0, 0, 1 },
    { CR_SENSOR_TUBE, { 0.018f, 0.016f, 0.022f }, { 2, 2, 2 }, 0.95f, 0.012f, 1, 0, 0, 0, 0, 0, 1 },
    { CR_SENSOR_TUBE, { 0.018f, 0.017f, 0.018f }, { 3.5f, 3.5f, 3.5f }, 1, 0, 8, 0, 0, 0, 0, 0, 1 },
    { CR_SENSOR_TUBE, { 0.045f, 0.04f, 0.045f }, { 3, 3, 3 }, 1, 0, 8, 0.15f, 0, 0, 0, 0.8f, 3.58f },
    { CR_SENSOR_TUBE, { 0.08f, 0.07f, 0.08f }, { 1.5f, 1.5f, 1.5f }, 0.65f, 0, 12, 0.6f, 0, 0, 0, 1.0f, 3.58f },
    { CR_SENSOR_TUBE, { 0.035f, 0.03f, 0.035f }, { 2, 2, 2 }, 1, 0, 10, 0.2f, 0, 0, 0, 0.6f, 5.0f },
    { CR_SENSOR_CCD, { 0, 0, 0 }, { 1, 1, 1 }, 1, 0, 0, 0, 0.12f, 1, 0, 0.5f, 4.8f },
    { CR_SENSOR_CCD, { 0, 0, 0 }, { 1, 1, 1 }, 1, 0, 0, 0, 0.04f, 0, 0, 0, 1 },
    { CR_SENSOR_MOS, { 0, 0, 0 }, { 1, 1, 1 }, 1, 0, 0, 0, 0, 0, 0.02f, 0.5f, 4.8f } };

}  // namespace

const char* const kCrRoomNames[3] = { "Buia", "Penombra", "Illuminata" };

void creativeLoadMedium(CreativeControls& c, int m)
{
    const Medium& md = kMedia[m >= 0 && m < kCrMediumCount ? m : 0];
    c.mediumGamma = md.gamma;
    c.mediumContrast = md.contrast;
    c.mediumAmbient = md.ambient * 100.0;
    c.mediumNits = md.nits;
    c.mediumKelvin = md.kelvin;
    c.mediumColour = md.colour;
    c.mediumOverscan = md.overscan;
    c.mediumRoom = md.room;
    c.mediumLoaded = m;
}

CrMediumRows creativeMediumRows(int m)
{
    const Medium& md = kMedia[m >= 0 && m < kCrMediumCount ? m : 0];
    const bool on = md.kind != CR_MEDIUM_NONE && md.kind != CR_MEDIUM_LCD;   // the console's greens are its own
    return { on, on, on && md.lines > 0, md.unsteady > 0 };
}

namespace {

int pick(int i, int n) { return i >= 0 && i < n ? i : 0; }

// The gauge (stock bits) a format belongs to: the 35 mm frames share their stock, so do 65 mm and IMAX.
unsigned gaugeOf(int format)
{
    static const unsigned g[kCrFormatCount] = { kGS8, kG16, kG35, kG65, kG65, 0, kG16, kG8, kG95, kG35, kG35, kG35,
                                                kG35, kG135, kG120 };
    return format >= 0 && format < kCrFormatCount ? g[format] : 0;
}

std::string gaugeList(unsigned bits)
{
    static const char* const names[] = { "8 mm", "Super 8", "9,5 mm", "16 mm", "35 mm", "65 mm", "135", "120" };
    std::string out;
    for (int i = 0; i < 8; ++i)
        if (bits & (1u << i)) out += (out.empty() ? "" : ", ") + std::string(names[i]);
    return out;
}

}  // namespace

int creativeDeviceKey(const CreativeControls& c)
{
    switch (c.device) {
    case kCrDevFilm: return 1000 + pick(c.filmCamera, kCrCameraCount);
    case kCrDevVideo: return 2000 + pick(c.videoCamera, kCrVideoCameraCount);
    case kCrDevCinema: return 3000 + pick(c.cinemaCamera, kCrCinemaCameraCount);
    case kCrDevPhone: return 4000 + pick(c.phoneCamera, kCrPhoneCameraCount);
    default: return 0;
    }
}

void creativeLoadDevice(CreativeControls& c)
{
    if (c.device == kCrDevFilm) {
        const CrCamera& m = kCrCameras[pick(c.filmCamera, kCrCameraCount)];
        c.format = m.format;
        c.weave = m.weave;
        c.breath = m.breath;
        c.vignette = m.vignette;
        c.aberration = m.aberration;
        c.bloom = m.bloom;
        c.hair = m.hair;
    } else if (c.device == kCrDevVideo) {
        const CrVideoCamera& m = kCrVideoCameras[pick(c.videoCamera, kCrVideoCameraCount)];
        c.analog = m.tape;
        c.wear = m.wear;
        c.heads = m.heads;
        c.instability = m.instability;
        c.sensor = m.sensor;
    }
    c.deviceLoaded = creativeDeviceKey(c);
}

const CrStock* creativeStock(const CreativeControls& c)
{
    if (c.device != kCrDevFilm) return nullptr;
    const CrMaker& m = kCrMakers[pick(c.maker, kCrMakerCount)];
    return &m.stocks[pick(c.stock[pick(c.maker, kCrMakerCount)], m.count)];
}

bool creativeStockFits(const CreativeControls& c)
{
    const CrStock* st = creativeStock(c);
    return !st || (st->gauges & gaugeOf(c.format)) != 0;
}

int creativeGaugeList(const CreativeControls& c)
{
    const unsigned g = gaugeOf(c.format);
    for (int l = 0; l < kCrGaugeListCount; ++l)
        if (kCrGaugeLists[l].gauge == g) return l;
    return -1;
}

int creativePickIndex(int l, int maker, int stock)
{
    if (l < 0 || l >= kCrGaugeListCount) return -1;
    for (int i = 0; i < kCrGaugeLists[l].count; ++i)
        if (kCrGaugeLists[l].picks[i].maker == maker && kCrGaugeLists[l].picks[i].stock == stock) return i;
    return -1;
}

std::string creativePickLabel(int l, int i)
{
    if (l < 0 || l >= kCrGaugeListCount || i < 0 || i >= kCrGaugeLists[l].count) return "";
    const CrPick& pk = kCrGaugeLists[l].picks[i];
    std::string maker = kCrMakers[pk.maker].name, name = kCrMakers[pk.maker].stocks[pk.stock].name;
    const size_t cut = maker.find_first_of("/(");
    if (cut != std::string::npos) maker = maker.substr(0, maker.find_last_not_of(' ', cut - 1) + 1);
    const unsigned g = kCrGaugeLists[l].gauge;
    const bool kodak = pk.maker == 0, fuji = pk.maker == 1;
    if ((kodak && (g & (kG8 | kGS8 | kG16))) || (fuji && (g & kG16)))
        for (size_t k = 0; k + 4 <= name.size(); ++k) {   // the first four-digit code: 5219 -> 7219, 8573 -> 8673
            const bool four = std::isdigit((unsigned char)name[k]) && std::isdigit((unsigned char)name[k + 1])
                              && std::isdigit((unsigned char)name[k + 2]) && std::isdigit((unsigned char)name[k + 3])
                              && (k == 0 || !std::isdigit((unsigned char)name[k - 1]))
                              && (k + 4 == name.size() || !std::isdigit((unsigned char)name[k + 4]));
            if (!four) continue;
            if (kodak && name[k] == '5') name[k] = '7';
            if (fuji && name[k] == '8' && name[k + 1] == '5') name[k + 1] = '6';
            break;
        }
    return maker + " · " + name;
}

int creativeBase(const CreativeControls& c)
{
    if (c.base >= 1 && c.base <= 4) return c.base;
    // by era and gauge: nitrate only in 35 mm and wider until the 1951 switch, small gauges always on safety film
    // (diacetate until triacetate took over at the end of the 1940s); camera film never on polyester [gen dates]
    int year = 0;
    if (const CrStock* st = creativeStock(c)) {
        const char* n = std::strchr(st->name, '(');
        while (n && !year) {
            if (std::isdigit((unsigned char)n[1]) && std::isdigit((unsigned char)n[4])) year = std::atoi(n + 1);
            n = std::strchr(n + 1, '(');
        }
    }
    if (year == 0) return 3;
    const unsigned g = gaugeOf(c.format);
    if ((g & (kG35 | kG65 | kG135 | kG120)) && year < 1951) return 1;
    return year < 1948 ? 2 : 3;
}

void creativeLoadLens(CreativeControls& c, int l)
{
    const CrLens& m = kCrLenses[pick(l, kCrLensCount)];
    c.distortion = m.distortion;
    c.edgeSoft = m.soft;
    c.swirl = m.swirl;
    c.glare = m.glare;
    c.streak = m.streak;
    c.vignette = m.vignette;
    c.aberration = m.aberration;
    c.bloom = m.bloom;
    c.lensLoaded = l;
}

static CreativeControls resolveDevice(const CreativeControls& c);

CreativeControls creativeResolve(const CreativeControls& c)
{
    CreativeControls r = resolveDevice(c);
    if (r.lens > 0 && r.lens < kCrLensCount && r.lensLoaded != r.lens) creativeLoadLens(r, r.lens);
    return r;
}

static CreativeControls resolveDevice(const CreativeControls& c)
{
    CreativeControls r = c;
    if (c.device <= kCrDevManual || c.device >= kCrDeviceCount) {
        r.device = kCrDevManual;
        return r;
    }
    if (r.deviceLoaded != creativeDeviceKey(r)) creativeLoadDevice(r);
    if (const CrStock* st = creativeStock(r)) {
        static const int proc[3] = { kCrColourNegative, kCrReversal, kCrBlackWhite };
        r.process = proc[st->process];
        if (st->process == kStNeg) r.negative = st->row;
        else if (st->process == kStRev) r.reversal = st->row;
        else {
            r.bwStock = st->row;
            r.sensitivity = st->sens;
        }
    } else if (r.device == kCrDevVideo) {
        r.process = kCrAnalog;
    } else {
        r.process = kCrDigital;
        r.digital = r.device == kCrDevCinema ? kCrCinemaCameras[pick(r.cinemaCamera, kCrCinemaCameraCount)].row
                                             : kCrPhoneCameras[pick(r.phoneCamera, kCrPhoneCameraCount)].row;
    }
    return r;
}

std::string creativeDeviceInfo(const CreativeControls& c0)
{
    const CreativeControls c = creativeResolve(c0);
    if (c.device == kCrDevVideo)
        return std::string("Nastro: ") + kCrAnalogNames[pick(c.analog, kCrAnalogCount)] + ". Sensore: "
               + kCrSensorNames[pick(c.sensor, kCrSensorCount)];
    if (c.device == kCrDevCinema || c.device == kCrDevPhone) {
        const CrDigitalCamera& m = c.device == kCrDevCinema ? kCrCinemaCameras[pick(c.cinemaCamera, kCrCinemaCameraCount)]
                                                            : kCrPhoneCameras[pick(c.phoneCamera, kCrPhoneCameraCount)];
        static const char* const subs[4] = { "4:4:4", "4:2:2", "4:2:0", "4:1:1" };
        std::string r = std::string("Sensore: ") + (m.lines > 0 ? std::to_string(m.lines) + " righe" : "piena risoluzione");
        if (m.alias) r += " con aliasing";
        r += m.quality > 0 ? ". Codec: qualità " + std::to_string(m.quality) + ", colore " + subs[std::max(0, std::min(m.sub, 3))]
                           : ". Codec: invisibile";
        return r + ". Curva e colore: " + kCrDigitalNames[pick(c.digital, kCrDigitalCount)];
    }
    const CrStock* st = creativeStock(c);
    if (!st) return "";
    const CrCamera& cam = kCrCameras[pick(c.filmCamera, kCrCameraCount)];
    std::string formats;
    for (int f = 0; f < kCrFormatCount; ++f)
        if (cam.formats & (1u << f)) formats += (formats.empty() ? "" : ", ") + std::string(kCrFormatNames[f]);
    static const char* const kind[3] = { "negativo colore", "invertibile colore", "bianco e nero" };
    std::string text = "Formati della cinepresa: " + formats + " · " + st->name + ": " + kind[st->process]
                       + ", prodotta in " + gaugeList(st->gauges);
    const int f = pick(c.format, kCrFormatCount);
    if (!(cam.formats & (1u << f))) text += " · la cinepresa non monta il " + std::string(kCrFormatNames[f]);
    if (!creativeStockFits(c)) text += " · mai prodotta in " + std::string(kCrFormatNames[f]) + ": resa come se lo fosse";
    return text;
}

bool creativeToggles(int process)
{
    return process == kCrColourNegative || process == kCrReversal || process == kCrBlackWhite;
}

bool creativeLogOut(const CreativeControls& r)
{
    return (r.process == kCrColourNegative && r.output == CR_OUT_LOG) || (creativeToggles(r.process) && !r.useTone);
}

bool creativeNeedsPrevious(const CreativeControls& c0)
{
    const CreativeControls c = creativeResolve(c0);
    return c.medium > 0 && c.medium < kCrMediumCount && kMedia[c.medium].ghost > 0.0f && c.mediumAmount > 0.0
           && c.transfer <= 0 && !creativeLogOut(c);   // a copy's pass shows no previous picture
}

bool creativePrepareTransfer(CreativeParams& p, const CreativeControls& c0, int W, int H, int frame)
{
    const CreativeControls c = creativeResolve(c0);
    if (c.transfer <= 0 || c.transfer >= kCrTransferCount || creativeLogOut(c)) return false;
    // the copy sees the finished picture as a scene: its grey (display 0.115) back to 0.18
    CreativeControls t;
    t.exposure = std::log2(0.18 / 0.115);
    switch (c.transfer) {
    case 1: t.process = kCrAnalog; t.analog = 1; t.wear = 0.1; t.heads = 0.3; t.instability = 0.25; break;
    case 2: t.process = kCrAnalog; t.analog = 9; t.wear = 0.05; t.heads = 0; t.instability = 0.05; break;
    case 3:   // a copy of a copy: every generation's noise, bleed and timebase on top of each other
        t.process = kCrAnalog; t.analog = 1; t.wear = 0.6; t.heads = 0.5; t.instability = 0.5; t.noise = 1.6;
        t.colourBleed = 1.5; break;
    case 4: t.process = kCrBlackWhite; t.bwStock = 0; t.format = 1; t.weave = 0.3; t.grain = 1.2; break;
    default: t.process = kCrColourNegative; t.negative = 0; t.print = 1; t.format = 2; t.grain = 0.7; break;
    }
    // what the copy is seen on: the original's medium and its response
    t.medium = c.medium;
    t.mediumAmount = c.mediumAmount;
    t.mediumLoaded = c.mediumLoaded;
    t.mediumRoom = c.mediumRoom;
    t.mediumGamma = c.mediumGamma;
    t.mediumContrast = c.mediumContrast;
    t.mediumAmbient = c.mediumAmbient;
    t.mediumNits = c.mediumNits;
    t.mediumKelvin = c.mediumKelvin;
    t.mediumColour = c.mediumColour;
    t.mediumOverscan = c.mediumOverscan;
    creativePrepare(p, t, W, H, frame, 1, CR_GAMMA24);   // Rec.709, Gamma 2.4: the first pass's output
    return true;
}

bool creativeSensorTube(int s) { return kSensors[pick(s, kCrSensorCount)].type == CR_SENSOR_TUBE; }
bool creativeSensorCCD(int s) { return kSensors[pick(s, kCrSensorCount)].type == CR_SENSOR_CCD; }

int creativeHistory(const CreativeControls& c0)
{
    const CreativeControls c = creativeResolve(c0);
    return c.process == kCrAnalog && creativeSensorTube(c.sensor) && (c.lag > 0 || c.comet > 0) ? 3 : 0;
}

void creativeRecordTimes(const CreativeControls& c0, double out[3])
{
    const CreativeControls c = creativeResolve(c0);
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

// Film unsteadiness, frame by frame, without a rhythm (research: CREATIVE.md, "Oscillazione"). Value noise with
// knots every `period` frames, Catmull-Rom between them: a drift that wanders, not a sine; RMS about 0.6.
static double weaveDrift(double frame, double period, int seed)
{
    const double t = frame / period, f = t - std::floor(t);
    const int i = (int)std::floor(t);
    const double p0 = cr_hash(i - 1, seed, 911), p1 = cr_hash(i, seed, 911), p2 = cr_hash(i + 1, seed, 911),
                 p3 = cr_hash(i + 2, seed, 911);
    return 0.5 * ((2 * p1) + (-p0 + p2) * f + (2 * p0 - 5 * p1 + 4 * p2 - p3) * f * f
                  + (-p0 + 3 * p1 - 3 * p2 + p3) * f * f * f);
}
static double weaveKick(int frame, int seed) { return cr_hash(frame, seed, 913); }   // one frame's own error

void creativeWeave(const CreativeControls& c0, int W, int H, int frame, double out[6])
{
    const CreativeControls c = creativeResolve(c0);
    for (int k = 0; k < 6; ++k) out[k] = 0;
    const bool toggles = creativeToggles(c.process);
    const bool texture = !toggles || c.useTexture;
    const unsigned bit = 1u << (c.process >= 0 && c.process < kCrProcessCount ? c.process : 0);
    const unsigned moving = (1u << kCrColourNegative) | (1u << kCrReversal) | (1u << kCrBlackWhite) | (1u << kCrTechni3)
                            | (1u << kCrTechni2) | (1u << kCrAutochrome) | (1u << kCrDufay) | (1u << kCrThreePlates)
                            | (1u << kCrKinemacolor);
    const Format& fmt = kFormats[c.format >= 0 && c.format < kCrFormatCount ? c.format : 2];
    if (texture && (bit & moving) && c.camWeave) {
        // the camera: the claw leaves the film a little off at every frame (mostly vertical, uncorrelated), the side
        // rail lets it wander sideways over about half a second. The table's value is the peak; RMS = peak / 2.5.
        const double s = clampd(c.weave, 0, 1) * fmt.weave * W / 2.5;
        out[0] = s * (0.85 * weaveDrift(frame, 14, 1) + 0.35 * weaveKick(frame, 2));
        out[1] = s * (0.75 * weaveKick(frame, 3) + 0.40 * weaveDrift(frame, 8, 4));
    }
    const bool printed = (bit & ((1u << kCrColourNegative) | (1u << kCrBlackWhite)))
                         && !(c.process == kCrColourNegative && (c.print == 0 || creativeLogOut(c)));
    if (texture && printed && c.printWeave > 0) {
        // each printing step (contact printer: the two films slip a little on the sprocket) adds a slow drift
        static const int steps[kCrGenerationCount] = { 1, 3, 3, 5 };
        const int g = c.generation >= 0 && c.generation < kCrGenerationCount ? c.generation : 0;
        const double s = clampd(c.printWeave, 0, 3) * 0.00015 * W * std::sqrt((double)steps[g])
                         * std::max(fmt.weave / 0.0012, 1.0);
        out[2] = s * weaveDrift(frame, 20, 5);
        out[3] = s * weaveDrift(frame, 11, 6);
    }
    if (c.medium > 0 && c.medium < kCrMediumCount && c.projWeave && !creativeLogOut(c)) {
        // the projector: a 35 mm intermittent sprocket turns a quarter per frame, so a worn tooth repeats its jump
        // every 4 frames; the claw of a 16 mm or Super 8 projector errs at every frame. Sideways the film wanders
        // between the guide rails over a couple of seconds.
        const Medium& md = kMedia[c.medium];
        const double s = md.unsteady * clampd(c.projectorWeave, 0, 3) * H / 1.5;   // the table: peak
        if (s > 0) {
            if (c.medium == 1) {
                out[4] = s * (0.6 * weaveDrift(frame, 40, 7) + 0.2 * weaveKick(frame, 8));
                out[5] = s * (0.55 * cr_hash(((frame % 4) + 4) % 4, 9, 915) + 0.35 * weaveKick(frame, 10));
            } else {
                out[4] = s * (0.7 * weaveDrift(frame, 30, 7) + 0.3 * weaveKick(frame, 8));
                out[5] = s * (0.8 * weaveKick(frame, 10) + 0.3 * weaveDrift(frame, 6, 11));
            }
        }
    }
}

// Something caught in the camera's gate, for the shot this frame belongs to (shots of 96 frames, [gen]): in a share
// `amount` of them a hair (thin, long, curving in from an edge of the aperture, now and then curled), a fibre of
// lint (two finer strands from one root) or a crumb of emulsion scraped off the film (short, thick); it trembles
// with the film's passage, the free end most, and stays for the whole shot.
// The camera aperture's height in mm per format (kCrFormatNames order; SMPTE and makers' figures, plates and stills
// their picture height): a hair is the same 60-90 um everywhere, huge on Super 8, a thread on 65 mm.
static const double kGateMM[kCrFormatCount] = { 4.01, 7.49, 18.66, 23.01, 52.63, 90.0, 7.41, 3.68,
                                                6.5, 18.59, 9.47, 25.17, 16.03, 24.0, 56.0 };

static void creativeHair(CreativeParams& p, double amount, int W, int H, int frame, int format, float printSoft)
{
    const double mm = H / kGateMM[format >= 0 && format < kCrFormatCount ? format : 2];   // pixels per mm
    p.hair = 0;
    p.hairStrands = 0;
    if (amount <= 0) return;
    const int shot = (int)std::floor(frame / 96.0);
    if (cr_hash01(shot, 1, 920) >= amount) return;
    const float kind = cr_hash01(shot, 2, 920);
    const bool crumb = kind < 0.3f, lint = !crumb && kind < 0.5f;
    const bool curl = !crumb && !lint && cr_hash01(shot, 9, 920) < 0.3f;
    const int side = (int)(cr_hash01(shot, 3, 920) * 4.0f) & 3;   // left, top, right, bottom
    const double along = 0.1 + 0.8 * cr_hash01(shot, 4, 920);
    // a hair: 60-90 um thick, 0.6-2.5 mm into the aperture; lint: 15-35 um, 0.4-1.5 mm; a crumb: 0.1-0.25 mm thick,
    // 0.15-0.5 mm long
    const double u5 = cr_hash01(shot, 5, 920), u6 = cr_hash01(shot, 6, 920);
    const double len = mm * (crumb ? 0.15 + 0.35 * u5 : lint ? 0.4 + 1.1 * u5 : 0.6 + 1.9 * u5);
    const double thick = crumb ? 0.10 + 0.15 * u6 : lint ? 0.015 + 0.02 * u6 : 0.06 + 0.03 * u6;
    p.hairW = (float)(0.5 * mm * thick);
    p.hairTaper = crumb ? 0.7f : lint ? 0.5f : 0.35f;
    p.hair = crumb ? 0.9f : lint ? 0.75f : 0.97f;
    p.hairSeed = 930 + (shot & 1023) * 4;
    p.hairStrands = lint ? 2 : 1;
    // its shadow: the light's cone past the fibre 20-150 um in front of the emulsion (about f/2.8), the emulsion's
    // own spread and the scan's (about 11 and 8 um), the prints' blur
    const double gap = 0.02 + 0.13 * cr_hash01(shot, 11, 920), cone = gap / 5.6;
    const double blur = mm * std::sqrt(cone * cone + 0.011 * 0.011 + 0.008 * 0.008);
    p.hairBlur = (float)std::sqrt(blur * blur + (double)printSoft * printSoft);
    const double pi = 3.14159265358979;
    const double base = (side == 0 ? 0.0 : side == 1 ? 0.5 : side == 2 ? 1.0 : 1.5) * pi;
    float box[4] = { 1e9f, 1e9f, -1e9f, -1e9f };
    for (int s = 0; s < p.hairStrands; ++s) {
        // from just outside the edge, inward, the bend changing smoothly along it (a curl turns much more)
        double x = side == 0 ? -0.1 * mm : side == 2 ? W + 0.1 * mm : along * W;
        double y = side == 1 ? -0.1 * mm : side == 3 ? H + 0.1 * mm : along * H;
        double a = base + 0.6 * cr_hash(shot, 7 + 40 * s, 920);
        const double bend = (crumb ? 0.25 : curl ? 0.9 : 0.35) * cr_hash(shot, 8 + 40 * s, 920);
        const double step = len * (s == 0 ? 1.0 : 0.7 + 0.3 * cr_hash01(shot, 12, 920)) / 8.0;
        const double phase = 6.283 * cr_hash01(shot, 13 + s, 920), rate = 0.25 + 0.3 * cr_hash01(shot, 14, 920);
        for (int i = 0; i < 9; ++i) {
            // the free end trembles more than the root: a slow sway and a jolt at every frame, up to 25 um
            const double reach = std::pow(i / 8.0, 1.5) * 0.025 * mm;
            const double sway = std::sin(frame * rate + phase + 0.5 * i);
            const float px = (float)(x + reach * (0.6 * sway * std::cos(a + 1.57) + 0.4 * cr_hash(frame, 10 + i + 20 * s, 921)));
            const float py = (float)(y + reach * (0.6 * sway * std::sin(a + 1.57) + 0.4 * cr_hash(frame, 30 + i + 20 * s, 921)));
            p.hairPt[18 * s + 2 * i] = px;
            p.hairPt[18 * s + 2 * i + 1] = py;
            box[0] = std::min(box[0], px);
            box[1] = std::min(box[1], py);
            box[2] = std::max(box[2], px);
            box[3] = std::max(box[3], py);
            x += std::cos(a) * step;
            y += std::sin(a) * step;
            a += bend * (1.0 + 1.2 * cr_value((float)i * 0.6f, (float)s, 925 + (shot & 255))) + 0.08 * cr_hash(shot, 50 + i + 10 * s, 920);
        }
    }
    const float m = p.hairW + 3.0f * p.hairBlur + 2.0f;
    p.hairBox[0] = box[0] - m;
    p.hairBox[1] = box[1] - m;
    p.hairBox[2] = box[2] + m;
    p.hairBox[3] = box[3] + m;
}

// The maker's edge print on the scanned strip [gen layout, sizes and spacing]: the brand as it reads on the film, the
// product code of the gauge (or the stock's name), the base (NITRATE FILM, SAFETY FILM, ESTAR) and a running number.
// From 1990 Kodak and Fuji motion picture negative carries KeyKode (maker and emulsion letters, roll, footage, a bar
// code); before, a key number and the date marks (dot, triangle, square). Photo film numbers its frames instead.
// Letters about 0.9 mm high in the margin outside the perforations, repeated along the whole strip (on real film the
// product line and the key number come every half foot or foot: here closer, so a single frame shows them).
static int edgeGlyph(char ch)
{
    static const char* const order = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+.o^#>/";
    const char* at = std::strchr(order, ch);
    return ch && at ? int(at - order) : 0;
}

static void creativeEdgePrint(CreativeParams& p, const CreativeControls& c, const FilmStrip& fs, int frame, bool print)
{
    p.edgeN = p.barN = 0;
    const CrStock* st = creativeStock(c);
    const unsigned gauge = gaugeOf(c.format);
    if (!gauge) return;   // a glass plate
    static const char* const brands[] = { "EASTMAN", "FUJI", "AGFA", "ORWO", "SVEMA", "ILFORD", "FERRANIA",
                                          "GEVAERT", "ANSCO", "DU PONT", "KODAK", "LOMOGRAPHY", "KONICA" };
    const int maker = c.device == kCrDevFilm ? pick(c.maker, kCrMakerCount) : 0;
    std::string name = st ? st->name : (c.process == kCrBlackWhite ? "Double-X 5222 (1959)" : "5247 (1974)");
    int year = 0;
    for (size_t k = name.find('('); k != std::string::npos && !year; k = name.find('(', k + 1))
        if (k + 4 < name.size() && std::isdigit((unsigned char)name[k + 1])) year = std::atoi(name.c_str() + k + 1);
    if (!year) year = 1975;
    std::string code;   // the product code as the gauge has it, else the stock's first word
    for (size_t k = 0; k + 4 <= name.size() && code.empty(); ++k)
        if (std::isdigit((unsigned char)name[k]) && std::isdigit((unsigned char)name[k + 3])
            && (k == 0 || !std::isalnum((unsigned char)name[k - 1])) && std::isdigit((unsigned char)name[k + 1])
            && std::isdigit((unsigned char)name[k + 2]) && (k + 4 == name.size() || !std::isdigit((unsigned char)name[k + 4])))
            code = name.substr(k, 4);
    if (!code.empty() && (gauge & (kG8 | kGS8 | kG16))) {
        if (maker == 0 && code[0] == '5') code[0] = '7';
        if (maker == 1 && code[0] == '8' && code[1] == '5') code[1] = '6';
    }
    if (code.empty()) code = name.substr(0, name.find_first_of(" ("));
    for (char& ch : code) ch = (char)std::toupper((unsigned char)ch);
    for (char& ch : code) if (!std::isalnum((unsigned char)ch)) ch = '-';
    std::string brand = brands[maker];
    if (maker == 0 && year >= 1990) brand = "KODAK";
    if (maker == 10) brand = "KODAK";   // CineStill is Kodak stock
    const int base = creativeBase(c);
    const bool photo = (gauge & (kG135 | kG120)) != 0;
    const bool keykode = !print && !photo && year >= 1990 && (maker == 0 || maker == 1) && (gauge & (kG16 | kG35 | kG65));
    const char* const support = base == 1 ? "NITRATE FILM" : base == 4 ? "ESTAR" : "SAFETY FILM";
    std::string text;
    int digits = 0, step = 1;
    if (print) {   // a print shows its own stock's code, dark on the clear edge (Eastman, then Kodak) [gen codes]
        const bool fuji = maker == 1 && year >= 1999;
        const char* pc = c.process == kCrBlackWhite ? (year < 1990 ? "5302" : "2302")
                         : fuji ? "3510" : year < 1974 ? "5385" : year < 1983 ? "5381" : year < 1998 ? "5384" : "2383";
        const char* pb = fuji ? "FUJI" : year < 1990 ? "EASTMAN" : "KODAK";
        const char* ps = year < 1951 ? "NITRATE FILM" : year < 1998 ? "SAFETY FILM" : "";
        text = std::string(pc) + " " + pb + "  " + ps + (*ps ? "  " : "")
               + (char)('A' + (unsigned)(cr_hash01(frame / 96, 6, 995) * 26)) + std::to_string(year % 10) + " ";
    } else if (photo) {
        text = brand + " " + code + "  ";
        digits = 2;   // the frame's number
    } else if (keykode) {
        static const char* const em = "ABCDEFGHJKLMNPRSTUVWXYZ";
        const int shot = frame / 96;
        text = brand + " " + code + "  " + support + "  " + (maker == 0 ? "K" : "F") + em[(unsigned)year % 23]
               + std::to_string(10 + (unsigned)(cr_hash01(shot, 1, 995) * 89)) + " "
               + std::to_string(100000 + (unsigned)(cr_hash01(shot, 2, 995) * 899999)) + " ";
        digits = 4;   // footage
    } else {
        static const char* const marks[] = { "o", "^", "#", "o^", "o#", "^#", "o^#" };
        text = brand + " " + code + "  " + support + "  " + (year < 1990 && maker == 0 ? marks[year % 7] : "")
               + (year < 1990 && maker == 0 ? " " : "")
               + (char)('A' + (unsigned)(cr_hash01(frame / 96, 3, 995) * 26)) + std::to_string(year % 10) + " ";
        digits = 6;
    }
    for (size_t i = 0; i < text.size() && p.edgeN < 48; ++i)
        p.edgeGlyph[p.edgeN++] = edgeGlyph(text[i]);
    for (int d = digits - 1; d >= 0 && p.edgeN < 48; --d) p.edgeGlyph[p.edgeN++] = 64 + d;
    if (photo && p.edgeN + 5 <= 48) {   // "12  >12A"
        p.edgeGlyph[p.edgeN++] = 0;
        p.edgeGlyph[p.edgeN++] = edgeGlyph('>');
        p.edgeGlyph[p.edgeN++] = 65;
        p.edgeGlyph[p.edgeN++] = 64;
        p.edgeGlyph[p.edgeN++] = edgeGlyph('A');
    }
    // the margin: outside the perforations, else beyond the picture
    const float half = 0.5f * fs.filmA;
    float inner = fs.sides == 1 || fs.sides == 2 ? fs.perfX + 0.5f * fs.perfA : 0.5f * fs.apA + std::fabs(fs.apOff);
    int side = 1;
    if (fs.sides == 1) side = -1;   // one row of perforations, on the side away from the picture's offset
    const float margin = half - inner;
    if (margin < 0.25f) { p.edgeN = 0; return; }
    p.edgeCH = std::min(0.1f, 0.6f * margin / 7.0f);
    p.edgeCW = 0.85f * p.edgeCH;
    p.edgeSide = side;
    p.edgeAcross = side * (inner + 0.5f * margin);   // cr_film's across is from the film's centre
    p.edgeStart = 0;
    p.barN = keykode ? 52 : 0;
    p.barW = 0.6f * p.edgeCW;
    p.barStart = p.edgeN * 6 * p.edgeCW + 4 * p.edgeCW;
    const double len = p.barStart + p.barN * p.barW + 30 * p.edgeCW;
    // repeats a whole number of frames apart for photo film (one number to a frame), else a whole number of
    // perforations; the strip moves one pitch per frame, so the print runs past the gate with the film
    const double unit = photo ? fs.pitch : (fs.perfPitch > 0 ? fs.perfPitch : fs.pitch);
    p.edgeRepeat = float(photo ? fs.pitch : unit * std::ceil(len / unit));
    const double at = double(frame) * fs.pitch;
    const double k = std::floor(at / p.edgeRepeat);
    p.edgePhase = -float(side) * (float(at - k * p.edgeRepeat) + 0.3f * fs.pitch);
    p.edgeStep = step;
    const int roll = int(cr_hash01(frame / 96, 4, 995) * 800000) + 100000;   // a roll's own numbers
    p.edgeNum = photo ? 1 + int(std::fmod(k, 36.0)) : keykode ? int(std::fmod(k + 1200, 1e4)) : int(std::fmod(roll + k, 1e6));
    p.edgeExp = 1.5f;   // a strong latent exposure: dense on the negative, clear on a reversal or a print
}

// The gauge a format's prints carry their sound on: 35 (the 35 mm frames printed 4-perf), 16 (16 mm), 8 (Super 8 and
// 8 mm, a magnetic stripe), 0 none (Super 16 has no room for a track, the wide and still formats are left out).
int creativeSoundGauge(int format)
{
    switch (format) {
    case 2: case 9: case 10: case 12: return 35;
    case 1: return 16;
    case 0: case 7: return 8;
    default: return 0;
    }
}

// The stock's year, from its name ("... (1974)").
static int creativeYear(const CreativeControls& c)
{
    const CrStock* st = creativeStock(c);
    const std::string name = st ? st->name : (c.process == kCrBlackWhite ? "Double-X 5222 (1959)" : "5247 (1974)");
    for (size_t k = name.find('('); k != std::string::npos; k = name.find('(', k + 1))
        if (k + 4 < name.size() && std::isdigit((unsigned char)name[k + 1])) return std::atoi(name.c_str() + k + 1);
    return 1975;
}

// The soundtrack the scanned strip shows (kCrSoundNames index, resolved: 1 none ... 6 magnetic), from the control and,
// by era and gauge, from the stock's year: variable density until the mid-1950s, then variable area (from 1928 the
// two coexisted), Dolby Stereo from 1976, the digital ones from 1993 (SDDS made until 2004). Optical only on a print.
static int creativeSoundOf(const CreativeControls& c, bool print, int year)
{
    const int g = creativeSoundGauge(c.format);
    int k = pick(c.printSound, 7);
    if (k == 0) {
        if (!print || g == 8 || g == 0) return 1;
        if (g == 16) return year >= 1933 ? 3 : 1;
        k = year < 1928 ? 1 : year < 1955 ? 2 : year < 1976 ? 3 : year < 1993 ? 4 : 5;
    }
    if (k == 6) return g == 8 || g == 16 ? 6 : 1;
    if (k == 1 || !print || g != 35 && g != 16) return 1;
    return g == 16 ? std::min(k, 3) : k;   // 16 mm: one optical track, no digital
}

// The print and its soundtrack on the strip (research COLONNA_SONORA.md, SMPTE 40, 41, 139, 164, 176, ST 203, RP 115,
// ISO 17266; mm from the guided edge, here from the film's centre): 35 mm track 4.90-7.44 mm, modulated 5.21-7.14,
// DTS 7.42-7.67, Dolby Digital blocks 2.41 mm in the perforation column, SDDS 0.295-1.831 mm from both edges; 16 mm
// track 13.46-15.49 mm from the perforated edge, magnetic 14.55 (SMPTE 112); Super 8 stripe 7.24-7.92 mm, balance
// 0.08-0.38. The printer leaves the perforations and the edges unexposed (Pritchard): the clear stock, its own edge
// print dark. Cyan dye tracks from 2004.
static void creativeSoundtrack(CreativeParams& p, const CreativeControls& c, const FilmStrip& fs, int frame, int sound,
                               bool print, int year)
{
    p.printClear = 0;
    p.sndKind = p.sndN = p.sndDigital = p.sndDye = 0;
    p.magW = p.balW = 0;
    const int g = creativeSoundGauge(c.format);
    const float half = 0.5f * fs.filmA;
    // the film's travel, wrapped every 1000 frames (whole perforations) so a float keeps microns
    p.sndPhase = float(std::fmod(double(frame) * fs.pitch, 1000.0 * fs.pitch));
    p.sndSeed = int(cr_hash01(frame / 2400, 5, 995) * 1000);   // a reel's own sound
    if (sound == 6) {   // the magnetic stripe, on the edge away from the perforations, its balance stripe beside them
        if (g == 8 && c.format == 0) {
            p.magAcross = half - 0.4f; p.magW = 0.69f; p.balAcross = -(half - 0.23f); p.balW = 0.30f;
        } else if (g == 8) {   // 8 mm [gen]: a narrower stripe beside the picture
            p.magAcross = half - 0.25f; p.magW = 0.4f; p.balAcross = -(half - 0.23f); p.balW = 0.30f;
        } else {   // 16 mm: over the optical track's place [gen balance]
            p.magAcross = 14.55f - half; p.magW = 2.41f; p.balAcross = -(half - 0.4f); p.balW = 0.6f;
        }
    }
    if (!print) return;
    p.printClear = 1;
    // the printer's light: over the camera's full aperture (black where the negative is clear) and the track's area
    const bool s35 = g == 35, track = sound >= 2 && sound <= 5;
    p.printLo = s35 ? (track ? 4.88f - half : -12.45f) : -5.13f;
    p.printHi = s35 ? 12.45f : (track ? 15.49f - half : 5.13f);
    if (sound <= 1 || sound == 6) return;
    p.sndKind = sound == 2 ? 1 : 2;
    p.sndN = sound >= 4 ? 2 : 1;
    if (g == 35) {   // the guided edge, the track's side, at -half
        p.sndAcross = 6.17f - half;
        p.sndW = 2.54f;
        p.sndMod = 1.93f;
        if (sound == 5) {
            p.sndDigital = 1 | 2 | (year < 2008 ? 4 : 0);   // SDDS left the prints after its decoders stopped [gen]
            p.ddAcross = -fs.perfX;
            p.ddSize = 2.41f;
            p.dtsAcross = 7.54f - half;
            p.dtsW = 0.25f;
            p.sddsIn = half - 1.831f;
            p.sddsOut = half - 0.295f;
            if (p.sndDigital & 4) p.edgeN = p.barN = 0;   // SDDS takes the edges
        }
    } else {   // 16 mm: the second row of perforations' place, the perforated edge at -half
        p.sndAcross = 14.475f - half;
        p.sndW = 2.03f;
        p.sndMod = 1.80f;
    }
    p.sndDye = c.process == kCrColourNegative && year >= 2004 ? 1 : 0;
}

void creativePrepare(CreativeParams& p, const CreativeControls& c0, int W, int H, int frame, int space, int gamma,
                     int history)
{
    CreativeControls c = creativeResolve(c0);
    const bool toggles = creativeToggles(c.process);
    const bool useTone = !toggles || c.useTone, useColour = !toggles || c.useColour, useTexture = !toggles || c.useTexture;
    if (!useTexture) {   // the film's grain, glows, lens and gate: none
        c.grain = c.printGrain = c.halation = c.bloom = c.aberration = c.vignette = 0;
        c.weave = c.hair = c.breath = c.dust = c.scratches = 0;
        c.distortion = c.edgeSoft = c.swirl = c.glare = c.streak = 0;
        c.lens = 0;
    }
    if (!useTone) c.push = c.silver = c.flash = 0;   // the development's light and contrast
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
    float printContrast = 1.45f, printSilver = 0.0f;
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
        printSilver = pr.silver;
        p.dustSign = c.print == 0 ? 1.0f : -1.0f;   // dust on the negative prints white
        if (c.output == CR_OUT_LOG) p.output = CR_OUT_LOG;
        if (c.output == CR_OUT_NEGATIVE) {   // the negative on a white light: base (Status M, Vision-like) [gen]
            p.output = CR_OUT_NEGATIVE;
            set3(p.negBase, 0.24f, 0.62f, 0.92f);
            p.negGamma = 0.6f * 0.30103f;   // a camera negative's gamma, per stop
        }
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
        const TapeEmu& te = kTapeEmu[c.analog >= 0 && c.analog < kCrAnalogCount ? c.analog : 1];
        p.kind = CR_KIND_ANALOG;
        const double bleed = clampd(c.colourBleed, 0, 2), wear = clampd(c.wear, 0, 1);
        const double inst = clampd(c.instability, 0, 1) * 4.0, heads = clampd(c.heads, 0, 1) / 0.3;
        tapeEmulation(p, te, W, H, bleed, wear, inst, heads, clampd(c.noise, 0, 2), clampd(c.sharpness, -1, 1));
        p.chromaBlockY = tp.colourUnder ? std::max(1, (int)lround(2.0 * H / 480.0)) : 1;   // unused by the tape
        p.saturation *= (float)(1.0 - 0.15 * wear);
        p.contrast = 1.15f;
        p.clip = 3.0f;
        p.knee = 0.3f;
        // the camera's sensor, before its curve
        const SensorDef& sn = kSensors[pick(c.sensor, kCrSensorCount)];
        p.sensor = sn.type;
        if (sn.type == CR_SENSOR_TUBE) {
            const double lag = clampd(c.lag, 0, 3);
            for (int k = 0; k < 3; ++k) {
                p.tubeK[k] = (float)(sn.K[k] * lag);
                p.tubeB[k] = sn.B[k];
            }
            p.tubeGamma = sn.gamma;
            p.tubeBias = sn.bias;
            p.tubeQmax = (float)(sn.qmax * clampd(c.comet, 0, 3));
            p.tubeGreen = sn.green;
            p.history = std::max(0, std::min(history, creativeHistory(c)));
        }
        p.smear = (float)(sn.smear * clampd(c.smear, 0, 3));
        p.smearWell = 1.0f;
        p.smearMono = sn.mono;
        if (sn.type == CR_SENSOR_CCD && p.smear <= 0.0f) p.sensor = 0;
        p.fpn = sn.fpn;
        p.falseColour = sn.fc;
        p.fcPeriod = (float)(W / 52.0 / sn.fcMHz);
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
        // a modelled device: its own sensor size, sharpening and codec instead of the row's blurs
        const CrDigitalCamera* dev = c.device == kCrDevCinema ? &kCrCinemaCameras[pick(c.cinemaCamera, kCrCinemaCameraCount)]
                                     : (c.device == kCrDevPhone ? &kCrPhoneCameras[pick(c.phoneCamera, kCrPhoneCameraCount)]
                                                                : nullptr);
        if (dev) {
            p.dig = 1;
            const double aspect = (double)W / H;
            p.dH = dev->lines > 0 && dev->lines < H ? dev->lines : H;
            p.dW = p.dH == H ? W : std::max(1, (int)lround(p.dH * aspect));
            p.dAlias = dev->alias;
            static const int subX[4] = { 1, 2, 2, 4 }, subY[4] = { 1, 1, 2, 1 };
            const int sb = std::max(0, std::min(dev->sub, 3));
            p.cW = (p.dW + subX[sb] - 1) / subX[sb];
            p.cH = (p.dH + subY[sb] - 1) / subY[sb];
            p.dSharp = (float)(dev->sharp * clampd(1.0 + c.sharpness, 0, 2));
            const double qual = dev->quality;
            p.dQ = qual > 0 ? (float)((qual < 50 ? 50.0 / qual : 2.0 - qual / 50.0) * comp) : 0.0f;
            // noise: the device's, made at the picture's size and averaged over each sensor pixel
            const double fp = dev->alias ? 1.0 : (double)W / p.dW;
            p.noise = (float)(p.noise * dev->noise * fp);
            p.chromaNoise = (float)(p.chromaNoise * dev->noise * fp);
            p.sharp = 0.0f;
            p.chromaBlockX = p.chromaBlockY = 1;
        }
        film = false;
        pushes = false;
        break;
    }
    default:
        break;
    }

    // the stock: its grain, saturation, contrast, halation and cast over the variant it starts from
    if (const CrStock* st = creativeStock(c)) {
        if (st->rms > 0) rms = st->rms;
        p.saturation *= st->sat;
        hal *= st->hal;
        for (int k = 0; k < 3; ++k) {
            if (proc != kCrReversal) p.slope[k] *= st->contrast;
            p.offset[k] += st->cast[k];
        }
        if (proc == kCrReversal) printContrast *= st->contrast;
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
    p.silver = std::min(1.0f, p.silver + printSilver);
    if (age > 0) {
        if (p.image == CR_IMAGE_SUB) set3(p.fade, (float)(1 - 0.6 * age), (float)(1 - 0.1 * age), (float)(1 - 0.4 * age));
        set3(p.stain, (float)(0.02 * age), (float)(0.04 * age), (float)(0.14 * age));
        for (int k = 0; k < 3; ++k) p.slope[k] *= (float)(1.0 - 0.15 * age);
        rms *= (float)(1.0 + 0.3 * age);
    }

    // the support: its own colour in what is seen through it (reversal, print, the negative as it is) and its decay
    if (proc == kCrColourNegative || proc == kCrReversal || proc == kCrBlackWhite) {
        // a fresh base is clear: it yellows as it decays (nitrate to amber, acetate a little), polyester stays [gen]
        static const float baseD[5][3] = { { 0, 0, 0 }, { 0.04f, 0.08f, 0.20f }, { 0.01f, 0.02f, 0.05f },
                                           { 0.01f, 0.02f, 0.05f }, { 0, 0, 0 } };
        const int b = creativeBase(c);
        p.decayKind = b == 1 ? 1 : (b == 4 ? 0 : 2);
        p.decay = (float)clampd(c.baseDecay, 0, 1);
        for (int k = 0; k < 3; ++k) {
            p.stain[k] += baseD[b][k] * p.decay;
            p.negBase[k] += baseD[b][k] * p.decay;
        }
    }
    // the generations between negative and print: duplicates add contrast, lose saturation, add their grain
    const Generation& gen = kGenerations[c.generation >= 0 && c.generation < kCrGenerationCount ? c.generation : 0];
    const bool printed = (proc == kCrColourNegative && c.print != 0 && p.output == CR_OUT_REC709) || proc == kCrBlackWhite;
    if (printed) {
        printContrast *= gen.contrast;
        p.saturation *= gen.sat;
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
    if (printed && p.kind == CR_KIND_PROCESS) {
        p.genGrain = (float)(0.08 * fmt.grainAmount * gen.grain * clampd(c.grain, 0, 3));
        p.genSize = (float)std::max(0.5, 1.1 * fmt.grainSize * gen.size * clampd(c.grainSize, 0.25, 4) * k1080);
        p.printGrain = (float)(0.010 * fmt.grainAmount * sqrt(gen.printSize) * clampd(c.printGrain, 0, 3));
        p.printGrainSize = (float)std::max(0.5, 0.8 * fmt.grainSize * gen.printSize * k1080);
        // each duplicate is a little less sharp: the records are blurred, and with them the camera's grain (added
        // later, so its size grows and its amplitude falls as a blurred noise's would)
        p.genSoft = gen.soft * k1080;
        if (p.genSoft > 0.0f) {
            const float wide = std::sqrt(p.grainSize * p.grainSize + 4.0f * p.genSoft * p.genSoft);
            p.grainAmount *= p.grainSize / wide;
            p.grainSize = wide;
        }
    }
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
    double wv[6];
    creativeWeave(c0, W, H, frame, wv);
    if (p.decayKind == 2 && p.decay > 0)   // a shrunk acetate base: printer and projector hold it less steadily
        for (int k = 2; k < 6; ++k) wv[k] *= 1.0 + 1.5 * p.decay;
    p.weaveX = film ? (float)(wv[0] + wv[2]) : 0.0f;   // camera and printer: the picture moves on the film
    p.weaveY = film ? (float)(wv[1] + wv[3]) : 0.0f;
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
    // the lens: distortion and the glass's colour where the records are read, then its own pass on them (soft
    // edges, swirl, veil, streak)
    const CrLens& ln = kCrLenses[pick(c.lens, kCrLensCount)];
    for (int k = 0; k < 3; ++k) {
        p.lTint[k] = c.lens > 0 ? ln.tint[k] : 1.0f;
        p.lGlareTint[k] = ln.glareTint[k];
        p.lStreakTint[k] = ln.streakTint[k];
    }
    const double dist = clampd(c.distortion, -10, 10) / 100.0;
    p.lDistX = (float)dist;
    p.lDistY = (float)(dist * (c.lens > 0 ? ln.distV : 1.0f));
    p.lSoft = (float)clampd(c.edgeSoft, 0, 3);
    p.lSwirl = (float)clampd(c.swirl, 0, 3);
    p.lGlare = (float)(clampd(c.glare, 0, 20) / 100.0);
    p.lStreak = (float)clampd(c.streak, 0, 3);
    p.lens = (p.lSoft > 0 || p.lSwirl > 0 || p.lGlare > 0 || p.lStreak > 0) ? 1 : 0;
    if (p.lGlare > 0.0f) {   // the veil: a quarter of the height around each point, a share of its mean light
        p.lgSigma = 0.25f * (float)H;
        glowGrid(W, H, p.lgSigma, p.lgS, p.lgW, p.lgH, p.lgSigma);
    }
    p.lStreakLen = 0.12f * (float)W;
    p.lStreakThr = CR_GREY * 4.0f;   // two stops over grey
    // each side of the streak carries the share lStreak of the light above the threshold: a wide bright area makes a
    // faint band, a very bright point a visible line
    p.lStreak *= (float)(1.0 - std::exp(-1.0 / std::max((double)p.lStreakLen, 1.0)));

    p.dust = (float)clampd(c.dust, 0, 1);
    if (film) creativeHair(p, clampd(c.hair, 0, 1), W, H, frame, c.format, p.genSoft);
    p.scratches = film ? (float)clampd(c.scratches, 0, 1) : 0.0f;
    p.moved = (p.weaveX != 0 || p.weaveY != 0 || p.jitter > 0 || p.headShift > 0 || p.flipX || ca > 0 || reg > 0
               || p.lDistX != 0 || p.lDistY != 0) ? 1 : 0;
    const bool strip = c.scanView > 0 && (proc == kCrColourNegative || proc == kCrReversal || proc == kCrBlackWhite);
    if (strip) {   // the scanned strip: the frame on the film, its gate, the perforations and the frames beside it
        FilmStrip fs = kStrips[pick(c.format, kCrFormatCount)];
        // a print (35 mm and 16 mm, Rec.709 through a positive): positive stock, its perforations, its soundtrack
        const int year = creativeYear(c), sg = creativeSoundGauge(c.format);
        const bool print = printed && p.output == CR_OUT_REC709 && (sg == 35 || sg == 16);
        const int sound = creativeSoundOf(c, print, year);
        if (print && sg == 35) {
            fs.perfL = 1.981f;   // Kodak Standard (KS-1870) perforations
            fs.perfR = 0.5f;
            if (sound >= 2 && sound <= 5 && (c.format == 2 || c.format == 10)) {   // printed beside the track
                fs.apA = c.format == 10 ? 21.95f : 22.0f;   // Super 35 reduced to Academy, Techniscope to 4-perf scope
                fs.apL = c.format == 10 ? 18.59f : 16.03f;
                fs.apOff = 1.3f;
                fs.pitch = 19.0f;
            }
        }
        if ((print && sg == 16 && sound >= 2) || (sg == 16 && sound == 6)) fs.sides = 1;   // single perforation
        p.strip = 1;
        p.travelH = fs.travelH;
        p.gate = pick(c.gateMask, 3);
        p.apW = fs.travelH ? fs.apL : fs.apA;
        p.apH = fs.travelH ? fs.apA : fs.apL;
        p.filmA = fs.filmA;
        p.aOff = -fs.apOff;
        p.pitch = fs.pitch;
        p.gateR = 0.08f * std::min(fs.apA, fs.apL);   // a rounded gate, as on amateur cameras [gen]
        p.perfSides = fs.sides;
        p.perfA = fs.perfA;
        p.perfL = fs.perfL;
        p.perfR = fs.perfR;
        p.perfX = fs.perfX;
        p.perfPitch = fs.perfPitch;
        p.perfPhase = fs.perfPhase;
        p.sQ = std::max((float)W / p.apW, (float)H / p.apH);
        // the overscan: how much beyond the frame (or the film's width, edge to edge) the scan takes in
        const float over = (float)clampd(c.scanOverscan, 0, 2);
        float vw = (1.0f + over) * p.apW, vh = (1.0f + over) * p.apH;   // the frame with its edges
        if (c.scanView >= 2) {   // edge to edge: the film's width and a bit of the frames beside
            const float across = (1.0f + 0.7f * over) * fs.filmA, along = (1.05f + over) * fs.pitch;
            vw = fs.travelH ? std::min(along, (1.5f + over) * p.apW) : across;
            vh = fs.travelH ? across : std::min(along, (1.5f + over) * p.apH);
            if (fs.travelH) p.sCY = p.aOff;
            else p.sCX = p.aOff;
        }
        p.sMM = std::max(vw / (float)W, vh / (float)H);
        p.moved = 1;
        creativeEdgePrint(p, c, fs, frame, print);
        creativeSoundtrack(p, c, fs, frame, sound, print, year);
        // the scanner's light [gen]: a little fall-off, its optics' glow around the openings, the base piping the
        // light along its cut edges (polyester most, it is clearer), the perforations' punching tolerance
        const int b = creativeBase(c);
        p.lampFall = 0.05f;
        p.glow = 0.10f;
        p.glowR = std::max(0.04f, 5.0f * p.sMM);
        p.pipe = b == 4 ? 0.45f : b == 1 ? 0.2f : 0.28f;
        p.pipeW = std::max(0.015f, 1.0f * p.sMM);
        p.perfTol = 0.004f;
    }
    double when[3];
    creativeRecordTimes(c, when);
    p.shared = (std::lround(when[0]) == std::lround(when[1]) && std::lround(when[1]) == std::lround(when[2])
                && ca <= 0 && reg <= 0) ? 1 : 0;   // one frame at one place: decode the source once

    // colour
    float sh[3], hi[3];
    hueVector(c.shadowHue, clampd(c.shadowAmount, 0, 1), sh);
    hueVector(c.highHue, clampd(c.highAmount, 0, 1), hi);
    for (int k = 0; k < 3; ++k) {
        p.splitShadow[k] = sh[k];
        p.splitHigh[k] = hi[k];
    }
    p.splitBalance = (float)clampd(c.balance, -3, 3);

    // only some of the film: without its colour the records are the picture's own channels in equal layers (a black
    // and white stock keeps the picture's colour); without its tone the curve is a straight line, back in the input
    // encoding (no print, no medium)
    if (!useColour) {
        if (p.records == 1) {
            p.records = 3;
            p.image = CR_IMAGE_SUB;
        }
        for (int i = 0; i < 9; ++i) p.recW[i] = p.dye[i] = i % 4 == 0 ? 1.0f : 0.0f;
        for (int k = 0; k < 3; ++k) p.recGain[k] = wb[k];
        p.uv = 0;
        p.interlayer = 1.0f;
        p.saturation = (float)clampd(c.saturation, 0, 2);
        const float slope = (p.slope[0] + p.slope[1] + p.slope[2]) / 3.0f, offset = (p.offset[0] + p.offset[1] + p.offset[2]) / 3.0f;
        const float cast = (printCast[0] + printCast[1] + printCast[2]) / 3.0f;
        const float fade = (p.fade[0] + p.fade[1] + p.fade[2]) / 3.0f, stain = (p.stain[0] + p.stain[1] + p.stain[2]) / 3.0f;
        for (int k = 0; k < 3; ++k) {
            p.slope[k] = slope;
            p.offset[k] = offset;
            p.printOffset[k] = cast + 0.083f * pts[k];
            p.fade[k] = fade;
            p.stain[k] = stain;
        }
    }
    if (!useTone) {
        for (int k = 0; k < 3; ++k) {
            p.slope[k] = 1.0f;
            p.offset[k] = 0.0f;
        }
        p.toe = p.shoulder = 1000.0f;
        p.output = CR_OUT_LOG;
    }

    // viewing medium: on the finished picture (not on a Log scan, which is graded afterwards)
    const Medium& md = kMedia[c.medium >= 0 && c.medium < kCrMediumCount ? c.medium : 0];
    const float ma = (float)clampd(c.mediumAmount, 0, 1);
    if (md.kind == CR_MEDIUM_NONE || p.output != CR_OUT_REC709) return;
    p.medium = md.kind;
    p.mGain = 1.0f + md.flicker * ma * cr_hash(frame, 5, 81);
    CreativeControls r = c;   // the response: the panel's when it holds this medium's values, else the preset's
    if (c.mediumLoaded != c.medium) creativeLoadMedium(r, c.medium);
    const double nits = clampd(r.mediumNits, 1, 1000);
    // perception: a dark room flattens contrast (Bartleson-Breneman, 2.4 / 2.6 in the dark, [gen] 1.06 lit); a dim
    // screen shows less contrast (Stevens) and less colour (Hunt) than the 100 nit reference
    const double room[3] = { 0.92, 1.0, 1.06 };
    const int lit = r.mediumRoom >= 0 && r.mediumRoom < 3 ? r.mediumRoom : 1;
    const double tone = clampd(r.mediumGamma, 1.5, 3.5) / 2.4 * room[lit] * std::pow(nits / 100.0, 0.04);
    const double sat = clampd(r.mediumColour, 0, 200) / 100.0 * clampd(std::pow(nits / 100.0, 0.1), 0.7, 1.2);
    p.mTone = (float)(1.0 + (tone - 1.0) * ma);
    p.mCutoff = (float)(ma / clampd(r.mediumContrast, 10, 100000));
    p.mSat = (float)(1.0 + (sat - 1.0) * ma);
    p.mChroma = md.chroma > 0.0f && ma > 0.0f ? md.chroma / ma : 0.0f;   // half the effect = twice the gamut
    p.mZoom = (float)(1.0 + clampd(r.mediumOverscan, 0, 20) / 100.0 * ma);
    p.mBlack = (float)(clampd(r.mediumAmbient, 0, 20) / 100.0 * ma);
    float white[3];
    kelvinWhite(r.mediumKelvin, white);
    for (int k = 0; k < 3; ++k) p.mWhite[k] = 1.0f + (md.white[k] * white[k] - 1.0f) * ma;
    p.mFalloff = md.falloff * ma;
    p.mShiftX = (float)(wv[4] * ma);   // the projector: the whole film moves on the screen
    p.mShiftY = (float)(wv[5] * ma);
    p.mSoft = md.soft * ma * k1080;
    p.mCorner = std::min(md.corner * ma * H, 0.25f * std::min(W, H));
    p.mEdge = std::max(0.5f, md.edge * ma * H);
    if (md.lines > 0) {
        // lines and phosphors need pixels to show: below 3 px per line (or per triad) they would only beat
        const float lines = ma * (float)smoothstepd(1.5, 3.0, (double)H / md.lines);
        const float pitch = (float)W / md.triads;
        p.mLines = md.lines;
        p.mCurve = md.curve * ma;
        p.mBeamMin = 0.6f + (md.beamMin - 0.6f) * lines;   // 0.6 lines: a beam wide enough to leave no gaps
        p.mBeamMax = 0.6f + (md.beamMax - 0.6f) * lines;
        p.mMask = md.mask * ma * (float)smoothstepd(2.0, 3.0, pitch);
        p.mMaskType = md.maskType;
        p.mPitch = std::max(3.0f, (float)W / md.triads);
    }
    if (md.kind == CR_MEDIUM_PAPER) p.mTexture = ma;
    if (md.glow > 0.0f && ma > 0.0f) {
        p.mGlow = md.glow * ma;
        glowGrid(W, H, md.glowRadius * H, p.mGS, p.mGW, p.mGH, p.mGlowSigma);
    }
    p.mGhost = md.ghost * ma;
    p.mDamper = md.damper * ma;
    p.mPlasma = md.plasma * ma;
    p.mLevels = md.levels > 1.0f && ma > 0.0f ? md.levels / ma : 0.0f;   // half the effect: twice the levels
    for (int k = 0; k < 3; ++k)
        p.mBlackTint[k] = md.blackTint[0] + md.blackTint[1] + md.blackTint[2] > 0.0f
                              ? 1.0f + (md.blackTint[k] - 1.0f) * ma : 1.0f;
    if (md.console > 0) {   // a console's frame: full at any amount, as the LCD console (it is the device)
        const Console& k = kConsoles[md.console];
        p.cKind = k.kind;
        p.cCols = k.cols;
        p.cRows = k.rows;
        p.cLines = md.lines;
        p.cN = k.n;
        p.cDither = k.dither;
        for (int l = 0; l < 8; ++l) p.cLevels[l] = k.levels[l] / 255.0f;
        if (k.kind == 1) nesPalette(p.cPal);
        p.mLines = md.lines;   // the console's lines even where the tube's would be too fine to show
    }
    if (md.kind == CR_MEDIUM_LCD) {   // 160 x 144, four shades of green (emulator values, [gen])
        p.mCols = 160;
        p.mRows = 144;
        p.mScreenH = 0.86f * H;
        p.mScreenW = p.mScreenH * 160.0f / 144.0f;
        if (p.mScreenW > 0.95f * W) {
            p.mScreenW = 0.95f * W;
            p.mScreenH = p.mScreenW * 144.0f / 160.0f;
        }
        p.mBezel = 0.05f * H;
        p.mDither = ma;
        p.mGrid = 0.35f * ma * (float)smoothstepd(3.0, 6.0, p.mScreenH / 144.0);   // the grid needs pixels to show
        const int hex[4] = { 0x0f380f, 0x306230, 0x8bac0f, 0x9bbc0f };
        for (int l = 0; l < 4; ++l)
            for (int k = 0; k < 3; ++k) p.mPalette[l * 3 + k] = (float)((hex[l] >> (16 - 8 * k)) & 0xff) / 255.0f;
        set3(p.mUnlit, 0.66f, 0.77f, 0.12f);
        set3(p.mBezelTint, 0.33f, 0.33f, 0.38f);
        set3(p.mBodyTint, 0.78f, 0.77f, 0.74f);
    }
}

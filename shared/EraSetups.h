// SPDX-License-Identifier: GPL-3.0-or-later
// Era setups: one list shared by the Creative (video) and Log MetaRaw Audio, so the same row in both plugins loads one
// coherent chain, from the camera and the stock to the recorder and the print. Every field names a row of the
// plugins' own tables (CreativePresets.h, CreativeModel.cpp, AudioPresets.h) by its exact name. A setup is the whole
// chain: nullptr is the device's own (format, sensor, microphone, medium) or none (lens, copies, transfer, viewing,
// desk). A row with no camera leaves the Creative as it is, one with no recorder the Ripresa (see auSetupValues). Sources, and which choices are documented and which are deduced: docs/SETUPS.md. Order is
// permanent (a saved project keeps the index): append only.
#pragma once
#include <iterator>

struct EraSetup
{
    const char* name;   // «Epoca · Opera (autore, anno)»
    // Video: a film camera (kCrCameras) or a video camera (kCrVideoCameras); then what changes on the camera's own.
    const char* camera;
    const char* format;   // kCrFormatNames; nullptr = the camera's own
    const char* stock;    // film stock (any manufacturer's list)
    float push;           // development, stops
    const char* lens;     // kCrLenses
    const char* sensor;   // kCrSensorNames; nullptr = the video camera's own
    const char* generation, *transfer, *medium;
    // Audio. Ripresa: recorder (any family), its microphone and medium (nullptr: its own), only with a recorder. Mix
    // e supporto: the desk, the master medium (tape; nullptr: the recorder's), the transfer and the listening.
    const char* recorder, *mic, *tape, *desk, *audioTransfer, *listen;
};

inline constexpr EraSetup kEraSetups[] = {
    { "Nessuno (a mano)" },
    { "Pre-cinema · Cinématographe Lumière (1895)", "Cinématographe Lumière (35 mm, 1895, a manovella)", nullptr,
      "Pellicola Lumière (1895)", 0, nullptr, nullptr, "Copia campione dal negativo", nullptr, nullptr },
    { "Nouvelle Vague · À bout de souffle (Godard, 1960)", "Éclair Cameflex CM3 (35 mm, 1947)", nullptr,
      "HPS (1954, foto)", 1, nullptr, nullptr, "Copia di distribuzione (interpositivo e internegativo)", nullptr,
      "Proiezione in sala (xeno)", "Ampex 350 (valvole, 1953)", "Neumann U 47 (valvole, 1949)",
      "Nastro ¼\" · 38 cm/s", nullptr, "Banda ottica 35 mm (Academy)", "Sala cinematografica (curva X)" },
    { "Nouvelle Vague · Vivre sa vie (Godard, 1962)", "Mitchell BNC (35 mm Academy, 1934)", nullptr,
      "Double-X 5222 (1959)", 0, nullptr, nullptr, "Copia di distribuzione (interpositivo e internegativo)", nullptr,
      "Proiezione in sala (xeno)", "Perfectone EP 6 (1959)", nullptr, "Nastro ¼\" · 19 cm/s",
      nullptr, "Banda ottica 35 mm (Academy)", "Sala cinematografica (curva X)" },
    { "Nouvelle Vague · Le Mépris (Godard, 1963)", "Mitchell BNC (35 mm Academy, 1934)", "35 mm anamorfico",
      "Eastmancolor 5251 (1962)", 0, "Franscope (anamorfico, anni '50-'60)", nullptr,
      "Copia di distribuzione (interpositivo e internegativo)", nullptr, "Proiezione in sala (xeno)",
      "Ampex 350 (valvole, 1953)", "Neumann U 47 (valvole, 1949)", "Nastro ¼\" · 38 cm/s", nullptr,
      "Banda ottica 35 mm (Academy)", "Sala cinematografica (curva X)" },
    { "Nouvelle Vague · Pierrot le fou (Godard, 1965)", "Arriflex 35 IIC (35 mm, 1963)", "Techniscope",
      "Eastmancolor 5251 (1962)", 0, nullptr, nullptr, "Copia di distribuzione (interpositivo e internegativo)",
      nullptr, "Proiezione in sala (xeno)", "Nagra III (1958)", nullptr, "Nastro ¼\" · 19 cm/s", nullptr,
      "Banda ottica 35 mm (Academy)", "Sala cinematografica (curva X)" },
    { "Godard e il video · Numéro deux (1975)", "Sony U-matic BVU-110 (ENG, 1976)", nullptr, nullptr, 0, nullptr,
      nullptr, nullptr, "Film-out su 35 mm (negativo e stampa)", "Proiezione in sala (xeno)", nullptr, nullptr,
      "U-matic", nullptr, "Banda ottica 35 mm (Academy)", "Sala cinematografica (curva X)" },
    { "Pasolini · Accattone (1961)", "Mitchell BNC (35 mm Academy, 1934)", nullptr,
      "Pancro 30 / P30 (Pasolini, Fellini)", 0, nullptr, nullptr,
      "Copia di distribuzione (interpositivo e internegativo)", nullptr, "Proiezione in sala (xeno)",
      "Ampex 350 (valvole, 1953)", "Neumann U 47 (valvole, 1949)", "Nastro ¼\" · 38 cm/s", nullptr,
      "Banda ottica 35 mm (Academy)", "Sala cinematografica (curva X)" },
    { "Pasolini · Il Vangelo secondo Matteo (1964)", "Mitchell BNC (35 mm Academy, 1934)", nullptr,
      "Pancro 30 / P30 (Pasolini, Fellini)", 0, "Angénieux 25-250 (zoom, anni '60-'80)", nullptr,
      "Copia di distribuzione (interpositivo e internegativo)", nullptr, "Proiezione in sala (xeno)",
      "Ampex 350 (valvole, 1953)", "Neumann U 47 (valvole, 1949)", "Nastro ¼\" · 38 cm/s", nullptr,
      "Banda ottica 35 mm (Academy)", "Sala cinematografica (curva X)" },
    { "Televisione · TV di Stato agli esordi (studio, 1954)", "Camera da studio a tubi Plumbicon (broadcast, anni '70)",
      nullptr, nullptr, 0, nullptr, "Nessuno (il segnale com'è)", nullptr,
      "Kinescopio su 16 mm bianco e nero (anni '50)", "Televisore a tubo PAL (576 righe)", nullptr,
      nullptr, "Banda ottica 16 mm", "RCA BC-2B (broadcast a valvole, 1950)", nullptr, "Radio a valvole (anni '50)" },
    { "Televisione · TV commerciale (anni '80)", "Ikegami HL-79E con U-matic BVU (Plumbicon, 1984)", nullptr,
      nullptr, 0, nullptr, nullptr, nullptr, nullptr, "Televisore a tubo PAL (576 righe)", nullptr,
      nullptr, "U-matic", nullptr, nullptr, "Televisore CRT (anni '70)" },
};
inline constexpr int kEraSetupCount = (int)std::size(kEraSetups);

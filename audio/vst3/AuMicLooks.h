// SPDX-License-Identifier: GPL-3.0-or-later
// How each microphone of kAuMics looks in the panel: its silhouette, its finish and how it is drawn. A long microphone
// (shotgun, stick, horn) lies under the recorder's front; a short, stout one stands beside it. Original drawings from
// the outline of each type, without makers' marks; the switches drawn on them are the real ones (kAuMicFilters,
// kAuMicPatterns) and move the plugin's. One row per microphone, same order as kAuMics.
#pragma once
#include "AudioPresets.h"
#include <cstdint>
#include <iterator>

enum class AuMicShape : uint8_t {
    Same,       // «Di serie»: the recorder's own, drawn from its row
    Horn,       // the recording horn of a phonograph
    Carbon,     // a carbon button on a candlestick
    Crystal,    // a round crystal capsule on a short stem
    Ribbon44,   // the large lozenge of the RCA 44
    Ribbon77,   // the capsule-shaped ribbon of the RCA 77
    Unidyne,    // the ribbed grille of the Shure 55
    Bottle,     // a valve condenser: cylinder, grille dome on top (U 47, C12)
    Stick,      // a slim dynamic stick with a round nose (EV 635A, cheap dynamics)
    Ball,       // a handheld with its ball grille (SM58)
    Shotgun,    // a long interference tube with its slots (MKH 416)
    Block,      // the stout ribbed body of the MD 421
    Electret,   // the little capsule on a camera
    Suspended,  // a round head in its elastic ring, for the boom (AKG D 25)
};

// body: the main metal or paint; trim: rings, yoke, handle; grille: the mesh. length: how long it is against its width
// (a lying microphone is drawn that much longer than tall). kind: what it is, for the card beside it.
struct AuMicLook
{
    AuMicShape shape;
    bool lying;
    uint32_t body, trim, grille;
    float length;
    const char* kind;
};

inline constexpr AuMicLook kAuMicLooks[] = {
    { AuMicShape::Same, false, 0, 0, 0, 1, "" },
    { AuMicShape::Horn, true, 0x8C6A3C, 0x3A2A16, 0x1C140C, 6, "Tromba acustica: nessuna elettricità, il suono muove la puntina" },
    { AuMicShape::Carbon, false, 0x2A2A2A, 0x8A7A5A, 0x6A6A66, 1, "A carbone: il microfono del telefono e delle prime radio" },
    { AuMicShape::Crystal, false, 0x7A7C80, 0x3C3C3E, 0x9A9C9E, 1, "Piezoelettrico a cristallo: economico, medi in evidenza" },
    { AuMicShape::Ribbon44, false, 0x9A9890, 0x3A3A3A, 0x6C6C68, 1, "A nastro, bidirezionale: le voci radio degli anni '30 e '40" },
    { AuMicShape::Ribbon77, false, 0xA8A49A, 0x3A3A3A, 0x707070, 1, "A nastro, direttività regolabile, filtro voce/musica" },
    { AuMicShape::Unidyne, false, 0xC8C8C6, 0x5A5A5A, 0xD8D8D4, 1, "Dinamico cardioide: il microfono dei palchi anni '50" },
    { AuMicShape::Bottle, false, 0xB8B4A8, 0x6A665E, 0x9A968C, 1, "A condensatore a valvole: direttività cardioide od omni" },
    { AuMicShape::Bottle, false, 0x8C8C88, 0x3C3C3A, 0xA0A09C, 1, "A condensatore a valvole: cinque figure polari" },
    { AuMicShape::Stick, true, 0xB0B0AE, 0x4A4A48, 0xC4C4C0, 4.5f, "Dinamico omnidirezionale: il microfono dei reporter" },
    { AuMicShape::Block, false, 0x2C2C2E, 0x9A9A9C, 0x3A3A3C, 1, "Dinamico cardioide con filtro dei bassi a cinque posizioni" },
    { AuMicShape::Ball, true, 0x3A3C40, 0x2A2A2C, 0xB8B8B6, 4, "Dinamico cardioide: il microfono da palco per eccellenza" },
    { AuMicShape::Shotgun, true, 0x5C5E60, 0x2A2A2C, 0x3A3A3C, 9, "A condensatore, «fucile» supercardioide: la presa diretta" },
    { AuMicShape::Stick, true, 0x2A2A2A, 0x6A6A6A, 0x5A5A5A, 4, "Dinamico economico: quello in dotazione ai registratori" },
    { AuMicShape::Electret, false, 0x1E1E20, 0x3A3A3C, 0x2C2C2E, 1, "Electret incorporato nella videocamera" },
    { AuMicShape::Suspended, false, 0x7C7E82, 0x2A2A2C, 0x9C9EA0, 1, "Dinamico cardioide da giraffa, in sospensione elastica" },
};
static_assert(std::size(kAuMicLooks) == std::size(kAuMics), "one look per microphone");

// SPDX-License-Identifier: GPL-3.0-or-later
// The guides of the panel, as data. The «?» on the plate tells the story of the machine on show (one entry per row of
// the preset tables, same order); «Cosa registri» says how hard to record a source on this machine's meter, which
// microphone suits it and which recorders were used for it. Levels are period practice, not a standard: peaks at
// about 0 VU on a VU (speech a little lower, drums lower still because the needle under-reads them), PPM 5 for speech
// and PPM 6 for music on a peak meter (BBC practice), 0 dB for the peaks on the Nagra modulometer. [prassi]
#pragma once
#include "AudioPresets.h"
#include "AuLooks.h"

#include <cstring>
#include <iterator>

// ---- the «?»: one story per recorder (family by family, in table order) and per desk

inline constexpr const char* kAuDeviceStories[] = {
    // Registratore a bobine portatile
    "Registratore domestico italiano, costruito a Milano e diffusissimo negli anni '60 in case, scuole e uffici. "
    "Una sola velocità lenta, microfono piezoelettrico in dotazione e un indicatore di registrazione al posto del "
    "meter: suono stretto, fruscio evidente, il nastro delle voci di famiglia.",
    "Il primo registratore portatile professionale a transistor, progettato in Svizzera da Stefan Kudelski. "
    "Adottato da radio e televisioni, con il pilota di sincronismo divenne dagli anni '60 lo strumento della presa "
    "diretta al cinema. Modulometro di picco, selettore di funzione a sei posizioni, velocità scelte con una moneta.",
    "Registratore a bobine portatile tedesco, a pile, compatto e robusto: per vent'anni la macchina dei radiocronisti "
    "e delle interviste sul campo. Quattro velocità, ingresso microfonico e meter a lancetta che fa anche da "
    "indicatore della batteria.",
    "L'erede del Nagra III, a mono traccia con pilotone: dagli anni '70 al digitale è lo standard della presa diretta "
    "nel cinema di tutto il mondo. Limitatore, filtri di ingresso, oscillatore di riferimento e modulometro di picco "
    "a norma.",
    // Registratore a bobine da studio
    "Registratore americano a valvole, la macchina di studi e radio degli anni '50: nastro da ¼\" veloce, "
    "VU meter, costruzione da broadcast. Il suono del rock'n'roll e del jazz di quegli anni.",
    "Registratore svizzero a valvole a quattro piste su nastro da 1\": negli studi europei degli anni '60 permise le "
    "prime sovraincisioni stratificate del pop. Qui è simulata una pista, con il VU di ognuna.",
    "Registratore svizzero semiprofessionale a transistor, presente in innumerevoli studi casalinghi, radio locali e "
    "scuole. Due velocità, due VU, ingressi miscelabili: affidabile e quasi trasparente.",
    "Registratore professionale svizzero a transistor, dal ¼\" ai 24 piste su 2\": standard di studi e "
    "broadcaster europei negli anni '70 e '80. Pulito, preciso, con VU o picchi secondo la versione.",
    // Registratore a cassetta
    "Il primo registratore a cassetta compatta, presentato da Philips nel 1963: a pile, un solo comando a leva per "
    "le funzioni, pensato per dettare e registrare la voce. Banda stretta, fruscio alto, un piccolo indicatore di "
    "livello.",
    "Il primo multitraccia a cassetta tutto in uno: quattro piste, mixer e meter in una valigetta. Rese possibile "
    "registrare dischi in camera da letto; un album celebre del 1982 nacque su questa macchina.",
    "Registratore portatile a cassetta professionale, usato da giornalisti, documentaristi e appassionati di "
    "registrazioni sul campo. Riduzione del rumore, nastri metal, meter a LED, controllo di livello manuale.",
    "Registratore portatile a cassetta per reportage e radio, compatto e robusto. Ingressi microfonici, riduzione del "
    "rumore, VU: la cassetta portata alla qualità professionale.",
    // Cinepresa e videocamera sonora
    "Cinepresa da 16 mm che registrava il suono otticamente sulla pellicola, nella macchina stessa: dagli anni '50 "
    "il mezzo di cinegiornali e notiziari televisivi. Banda stretta, la grana del suono ottico.",
    "Una delle prime cineprese Super 8 sonore: il suono andava sulla pista magnetica della pellicola in cassetta. "
    "Microfono a mano, livello automatico: il film di famiglia con la voce.",
    "Cinepresa Super 8 sonora per amatori evoluti, con zoom e registrazione sulla pista magnetica. Il suono "
    "risente della pista stretta e del trascinamento della pellicola.",
    "Videocamera VHS-C compatta degli anni '80: microfono incorporato, livello automatico, audio lineare con "
    "fruscio e trascinamento del nastro. Il suono dei video di famiglia.",
    "Tra le prime videocamere Video8 Sony: audio in modulazione di frequenza sul nastro, più pulito del VHS "
    "lineare, con microfono electret incorporato e livello automatico.",
    // Prima del nastro
    "Il fonografo a cilindro: niente elettricità, il suono entra nella tromba e muove direttamente la puntina che "
    "incide la cera. Si registrava gridando o suonando vicino alla tromba; il livello si regolava spostandosi.",
    "Registratore a filo d'acciaio americano del dopoguerra, prima che il nastro arrivasse nelle case: il filo "
    "sottile dà poca banda, fruscio e cadute di livello. L'indicatore è una lampadina al neon.",
};

inline constexpr const char* kAuDeskStories[] = {
    "Senza banco: il segnale va dritto al supporto. Restano i comandi del plugin, saturazione, dry/wet e uscita.",
    "Banco broadcast americano a valvole: le stazioni radio degli anni '50 mandavano in onda da qui microfoni, "
    "giradischi e linee. Attenuatori rotativi con le levette di ascolto e messa in onda, un VU con scala in "
    "percento di modulazione.",
    "Banco a valvole costruito in casa da EMI per i propri studi londinesi: con esso furono registrati gran parte "
    "dei dischi beat della metà degli anni '60. Fader a quadrante, equalizzazione Pop e Classic, quattro VU.",
    "Grande banco inglese a moduli discreti in classe A, con trasformatori: il suono pieno e caldo di molti dischi "
    "rock degli anni '70 e '80. Guadagno microfonico a scatti, equalizzatore a più bande, fader lungo.",
    "Banco inglese che portò su ogni canale compressore, gate e automazione computerizzata dei fader: il suono "
    "compatto e brillante del pop degli anni '80. Equalizzatore parametrico, VU.",
    "Mixer compatto a sedici canali che negli anni '90 portò il missaggio professionale negli studi casalinghi e "
    "sui palchi: preamplificatori puliti, equalizzatore a tre bande, meter a LED.",
};

static_assert(std::size(kAuDeviceStories) == std::size(kAuReelPortable) + std::size(kAuReelStudio) +
                  std::size(kAuCassetteDecks) + std::size(kAuCameras) + std::size(kAuEarly),
              "one story per recorder");
static_assert(std::size(kAuDeskStories) == std::size(kAuConsoles), "one story per desk");

// ---- «Cosa registri»

enum class AuLoud : uint8_t { Speech, Music, Percussive };

// What is in front of the microphone (Ripresa). Microphones are rows of kAuMics, best first; recorders are names as
// they appear in the device table, best first; the panel offers the first that fits the machine and its period.
struct AuSource
{
    const char* name;
    AuLoud loud;
    int lowCut;             // Hz, 0 = none
    bool limit;             // the limiter on (live, unpredictable sources)
    int mics[5];            // kAuMics rows, -1 ends
    const char* recorders[4];   // nullptr ends
    const char* tip;
};

inline constexpr AuSource kAuSources[] = {
    { "Nessuna guida", AuLoud::Music, 0, false, { -1 }, { nullptr }, "" },
    { "Voce parlata (dialogo, intervista)", AuLoud::Speech, 80, true, { 12, 10, 9, 5, 4 },
      { "Nagra 4.2", "Nagra III", "Uher", "TC-D5M" }, "microfono vicino alla bocca, fuori asse dal fiato; filtro contro il vento e i passi" },
    { "Voce cantata", AuLoud::Music, 60, false, { 7, 8, 5, 11, 6 },
      { "Studer A80", "Studer J37", "Ampex 350", "Revox A77" }, "a un palmo dal microfono, più lontano negli acuti forti" },
    { "Coro o gruppo vocale", AuLoud::Music, 40, false, { 8, 7, 4, -1 },
      { "Studer A80", "Ampex 350", "Revox A77", nullptr }, "un microfono davanti al gruppo, all'altezza delle teste" },
    { "Chitarra acustica", AuLoud::Music, 60, false, { 8, 7, 10, -1 },
      { "Revox A77", "Studer A80", "Portastudio", nullptr }, "verso il dodicesimo tasto, a 20-30 cm" },
    { "Chitarra elettrica (amplificatore)", AuLoud::Music, 80, false, { 10, 11, 4, 9, -1 },
      { "Studer J37", "Studer A80", "Portastudio", nullptr }, "vicino al cono, a metà tra centro e bordo" },
    { "Basso (amplificatore)", AuLoud::Music, 30, false, { 7, 10, 9, -1 },
      { "Studer A80", "Studer J37", "Portastudio", nullptr }, "davanti al cono; niente filtro, il basso è tutto sotto" },
    { "Grancassa", AuLoud::Percussive, 30, false, { 10, 7, 9, -1 },
      { "Studer A80", "Studer J37", "Portastudio", nullptr }, "nel foro della pelle risonante" },
    { "Rullante e tom", AuLoud::Percussive, 80, false, { 11, 10, -1 },
      { "Studer A80", "Studer J37", "Portastudio", nullptr }, "a due dita dal bordo, puntato al centro della pelle" },
    { "Batteria intera (panoramici)", AuLoud::Percussive, 60, false, { 8, 7, 4, -1 },
      { "Studer A80", "Studer J37", "Ampex 350", "Portastudio" }, "sopra la batteria, un metro sopra i piatti" },
    { "Pianoforte", AuLoud::Music, 40, false, { 7, 8, 4, -1 },
      { "Studer A80", "Ampex 350", "Revox A77", nullptr }, "a coda aperto, sopra le corde a metà" },
    { "Fiati (ottoni, sassofono)", AuLoud::Music, 60, false, { 4, 5, 10, 7, -1 },
      { "Ampex 350", "Studer J37", "Studer A80", nullptr }, "fuori asse dalla campana, il nastro regge gli ottoni" },
    { "Archi", AuLoud::Music, 40, false, { 8, 4, 7, -1 },
      { "Studer A80", "Ampex 350", "Revox A77", nullptr }, "sopra lo strumento, un metro circa" },
    { "Orchestra o banda", AuLoud::Music, 30, false, { 8, 7, 4, -1 },
      { "Studer A80", "Ampex 350", "Nagra 4.2", nullptr }, "dietro il direttore, in alto" },
    { "Ambiente e rumori di scena", AuLoud::Speech, 40, false, { 12, 8, 13, -1 },
      { "Nagra 4.2", "Nagra III", "TC-D5M", "WM-D6C" }, "fermo e lontano dal passaggio, a mano o su asta" },
    { "Concerto dal vivo", AuLoud::Percussive, 60, true, { 11, 10, 12, -1 },
      { "Nagra 4.2", "Studer A80", "Portastudio", "WM-D6C" }, "dal mixer di sala o con una coppia in platea" },
};

// The kind of programme on the master (Mix e supporto), and the desks it was made on.
struct AuProgram
{
    const char* name;
    AuLoud loud;
    const char* desks[3];      // names as in kAuConsoles, best first; nullptr ends
    const char* recorders[3];  // in Ripresa, for the tracks
    const char* tip;
};

inline constexpr AuProgram kAuPrograms[] = {
    { "Nessuna guida", AuLoud::Music, { nullptr }, { nullptr }, "" },
    { "Parlato, radiodramma, notiziario", AuLoud::Speech, { "RCA", "Neve", nullptr }, { "Nagra 4.2", "Uher", "Ampex 350" },
      "voce in primo piano, musica di commento sotto di 10-15 dB" },
    { "Rock'n'roll, jazz anni '50", AuLoud::Music, { "RCA", nullptr }, { "Ampex 350", "Revox A77", nullptr },
      "pochi microfoni, la stanza fa il suono" },
    { "Beat e pop anni '60", AuLoud::Music, { "REDD", "RCA", nullptr }, { "Studer J37", "Ampex 350", nullptr },
      "quattro piste: si riversa per liberarne" },
    { "Rock anni '70", AuLoud::Percussive, { "Neve", "SSL", nullptr }, { "Studer A80", nullptr },
      "batteria a molti microfoni, basso diretto" },
    { "Pop anni '80", AuLoud::Percussive, { "SSL", "Neve", nullptr }, { "Studer A80", "Portastudio", nullptr },
      "compressione su ogni canale, riverbero ampio" },
    { "Registrazione casalinga, demo", AuLoud::Music, { "Mackie", nullptr }, { "Portastudio", "Revox A77", "WM-D6C" },
      "quattro piste a cassetta, si registra a livello alto contro il fruscio" },
    { "Classica, orchestra", AuLoud::Music, { "Neve", "REDD", nullptr }, { "Studer A80", "Nagra 4.2", nullptr },
      "pochi microfoni lontani; il livello resta fermo, la dinamica è dell'orchestra" },
    { "Colonna sonora di film", AuLoud::Speech, { "Neve", "RCA", nullptr }, { "Nagra 4.2", "Nagra III", nullptr },
      "dialoghi, musica ed effetti su piste separate, poi il missaggio" },
};

// Where the peaks should reach on a meter of this kind, in dB on its own scale (0 = the alignment tone).
inline double auTargetPeak(AuMeter meter, AuLoud loud)
{
    switch (meter) {
    case AuMeter::PPM: return loud == AuLoud::Speech ? 4 : 8;   // PPM 5, PPM 6 (4 dB a mark, PPM 4 = 0)
    case AuMeter::Modulometer: return loud == AuLoud::Speech ? -2 : 0;
    case AuMeter::Led: return loud == AuLoud::Speech ? 0 : 3;
    case AuMeter::Neon: return 0;   // the lamp just flickers on the peaks
    default: return loud == AuLoud::Speech ? -3 : loud == AuLoud::Music ? 0 : -5;   // VU and the averaging meters
    }
}

// How the target reads on the meter: «0 VU», «PPM 6», «0 dB».
inline const char* auMeterUnit(AuMeter meter)
{
    switch (meter) {
    case AuMeter::PPM: return "PPM";
    case AuMeter::VU: return "VU";
    case AuMeter::Neon: return "neon";
    case AuMeter::Eye: return "occhio";
    case AuMeter::Indicator: return "indicatore";
    case AuMeter::Automatic: return "automatico";
    default: return "dB";
    }
}

// The flat index of the first recorder whose name contains `name`, -1 if none.
inline int auDeviceNamed(const char* name)
{
    if (!name) return -1;
    int flat = 0;
    for (const AuFamily& f : kAuFamilies)
        for (size_t i = 0; i < f.count; ++i, ++flat)
            if (std::strstr(f.devices[i].name, name)) return flat;
    return -1;
}
inline int auDeskNamed(const char* name)
{
    if (!name) return -1;
    for (int i = 1; i < int(std::size(kAuConsoles)); ++i)
        if (std::strstr(kAuConsoles[i].name, name)) return i;
    return -1;
}

inline int auDeviceCount()
{
    int n = 0;
    for (const AuFamily& f : kAuFamilies) n += int(f.count);
    return n;
}

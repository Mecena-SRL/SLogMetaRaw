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
#include <string>
#include <vector>

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

// The presets of use (Ripresa): what the take is for. Choosing one sets the recorder as an engineer of the period
// would have: microphone (rows of kAuMics, the first that fits the recorder and its years), its switches (position
// names, the first the microphone has), filter, limiter, input, tape speed. The record level comes from the measure.
// Recorders are names as in the device table: the machines used for it, for the advice. [dedotto: period practice]
enum class AuSpeed : uint8_t { Keep, Fastest, Standard };   // Standard: the second fastest (19 cm/s on a Nagra)
struct AuSource
{
    const char* cat;             // the category the menu groups it under
    const char* name;
    AuLoud loud;
    int lowCut;                  // Hz, 0 = none
    bool limit;                  // the limiter on (live, unpredictable sources)
    int mics[5];                 // kAuMics rows, -1 ends
    const char* filter[3];       // the microphone's bass filter position, by name (nullptr ends)
    const char* pattern[3];      // its polar pattern, by name
    int input;                   // 0 stereo, 1 mono
    AuSpeed speed;
    const char* recorders[4];    // nullptr ends
    const char* tip;
};

#define AU_CARD { "Cardioide", "Unidirezionale" }
#define AU_OMNI { "Omni", "Omnidirezionale", "Non direzionale" }
#define AU_EIGHT { "Figura a 8", "Bidirezionale" }
#define AU_MUSIC { "M", "M (musica)" }
#define AU_SPEECH { "3", "V1 (voce)" }
#define AU_WIND { "4", "V2 (voce, più taglio)" }
inline constexpr AuSource kAuSources[] = {
    { "", "Nessun preset", AuLoud::Music, 0, false, { -1 }, {}, {}, 0, AuSpeed::Keep, { nullptr }, "" },
    // Cinema
    { "Cinema", "Presa diretta", AuLoud::Speech, 80, true, { 12, 10, 5, -1 }, AU_SPEECH, AU_CARD, 1, AuSpeed::Standard,
      { "Nagra 4.2", "Nagra III", "Auricon", nullptr }, "fucile sull'asta appena fuori quadro, puntato alla bocca; pilota per il sincrono" },
    { "Cinema", "Documentario", AuLoud::Speech, 80, true, { 12, 10, 9, 13, -1 }, AU_SPEECH, AU_CARD, 1, AuSpeed::Standard,
      { "Nagra 4.2", "Nagra III", "Uher", "TC-D5M" }, "il microfono segue chi parla; l'ambiente si registra a parte, un minuto per scena" },
    { "Cinema", "Ambiente e rumori di scena", AuLoud::Speech, 40, false, { 12, 8, 13, -1 }, AU_MUSIC, AU_OMNI, 0, AuSpeed::Standard,
      { "Nagra 4.2", "Nagra III", "TC-D5M", "WM-D6C" }, "fermi e lontani dal passaggio; si registra in silenzio di troupe" },
    { "Cinema", "Super 8 e film di famiglia", AuLoud::Speech, 0, false, { 13, 14, -1 }, {}, {}, 1, AuSpeed::Keep,
      { "Ektasound", "Canon 514XL-S", nullptr }, "il microfono a mano verso chi parla, il livello è automatico" },
    // Televisione
    { "Televisione", "Servizio esterno e intervista", AuLoud::Speech, 100, true, { 9, 10, 12, 6, -1 }, AU_WIND, AU_CARD, 1,
      AuSpeed::Standard, { "Nagra 4.2", "Auricon", "Uher", nullptr }, "microfono a mano a un palmo dalla bocca, il filtro toglie vento e maneggio" },
    { "Televisione", "Studio, conduttore e ospiti", AuLoud::Speech, 80, false, { 5, 10, 11, 7, -1 }, AU_SPEECH, AU_CARD, 1,
      AuSpeed::Standard, { "Ampex 350", "Studer A80", nullptr }, "microfono da tavolo o a giraffa, voce piena e vicina" },
    { "Televisione", "Videocamera di famiglia", AuLoud::Speech, 0, false, { 14, -1 }, {}, {}, 1, AuSpeed::Keep,
      { "JVC", "CCD-V8", nullptr }, "il microfono della videocamera, livello automatico" },
    // Radio
    { "Radio", "Intervista e radiocronaca", AuLoud::Speech, 100, true, { 9, 10, 6, 11, -1 }, AU_WIND, AU_CARD, 1,
      AuSpeed::Standard, { "Uher", "Nagra III", "WM-D6C", "TC-D5M" }, "microfono a mano tra chi chiede e chi risponde" },
    { "Radio", "Annunci e radiodramma in studio", AuLoud::Speech, 60, false, { 4, 5, 7, 10, -1 }, AU_SPEECH, AU_EIGHT, 1,
      AuSpeed::Fastest, { "Ampex 350", "Studer A80", "Revox A77", nullptr }, "a un palmo, di lato al fiato; la voce calda del nastro" },
    // Sala di registrazione
    { "Sala di registrazione", "Voce", AuLoud::Music, 40, false, { 7, 8, 5, 11, 6 }, AU_MUSIC, AU_CARD, 1, AuSpeed::Fastest,
      { "Studer A80", "Studer J37", "Ampex 350", "Revox A77" }, "a un palmo dal microfono, più lontano nei passaggi forti" },
    { "Sala di registrazione", "Cori", AuLoud::Music, 40, false, { 8, 7, 4, 5, -1 }, AU_MUSIC, AU_OMNI, 1, AuSpeed::Fastest,
      { "Studer A80", "Ampex 350", "Revox A77", nullptr }, "un microfono davanti al coro, alto, a distanza pari alla larghezza del gruppo" },
    { "Sala di registrazione", "Chitarra acustica", AuLoud::Music, 60, false, { 8, 7, 10, -1 }, AU_MUSIC, AU_CARD, 1,
      AuSpeed::Fastest, { "Revox A77", "Studer A80", "Portastudio", nullptr }, "verso il dodicesimo tasto, a 20-30 cm" },
    { "Sala di registrazione", "Chitarra elettrica", AuLoud::Music, 80, false, { 10, 11, 4, 9, -1 }, AU_MUSIC, {}, 1,
      AuSpeed::Fastest, { "Studer J37", "Studer A80", "Portastudio", nullptr }, "vicino al cono dell'amplificatore, a metà tra centro e bordo" },
    { "Sala di registrazione", "Basso", AuLoud::Music, 0, false, { 7, 10, 9, -1 }, AU_MUSIC, AU_CARD, 1, AuSpeed::Fastest,
      { "Studer A80", "Studer J37", "Portastudio", nullptr }, "davanti al cono; niente filtro, il basso è tutto sotto" },
    { "Sala di registrazione", "Batteria", AuLoud::Percussive, 40, false, { 8, 7, 4, -1 }, AU_MUSIC, AU_CARD, 0,
      AuSpeed::Fastest, { "Studer A80", "Studer J37", "Ampex 350", "Portastudio" },
      "panoramici un metro sopra i piatti; grancassa e rullante, se servono, su tracce a parte" },
    { "Sala di registrazione", "Pianoforte", AuLoud::Music, 30, false, { 7, 8, 4, -1 }, AU_MUSIC, AU_CARD, 0, AuSpeed::Fastest,
      { "Studer A80", "Ampex 350", "Revox A77", nullptr }, "a coda aperta, sopra le corde a metà" },
    { "Sala di registrazione", "Fiati", AuLoud::Music, 60, false, { 4, 5, 10, 7, -1 }, AU_MUSIC, AU_EIGHT, 1, AuSpeed::Fastest,
      { "Ampex 350", "Studer J37", "Studer A80", nullptr }, "fuori asse dalla campana; il microfono a nastro regge gli ottoni" },
    { "Sala di registrazione", "Archi", AuLoud::Music, 40, false, { 8, 4, 7, -1 }, AU_MUSIC, AU_CARD, 1, AuSpeed::Fastest,
      { "Studer A80", "Ampex 350", "Revox A77", nullptr }, "sopra lo strumento, a un metro circa" },
    { "Sala di registrazione", "Orchestra", AuLoud::Music, 30, false, { 8, 7, 4, -1 }, AU_MUSIC, AU_OMNI, 0, AuSpeed::Fastest,
      { "Studer A80", "Ampex 350", "Nagra 4.2", nullptr }, "dietro e sopra il direttore; la dinamica è dell'orchestra, il livello non si tocca" },
    // Concerto
    { "Concerto dal vivo", "Dal mixer di sala", AuLoud::Percussive, 60, true, { 11, 10, -1 }, AU_MUSIC, AU_CARD, 0,
      AuSpeed::Fastest, { "Studer A80", "Revox A77", "Portastudio", nullptr }, "l'uscita del mixer di sala sul registratore, livello con margine" },
    { "Concerto dal vivo", "Coppia in platea", AuLoud::Percussive, 60, true, { 8, 12, 10, -1 }, AU_MUSIC, AU_CARD, 0,
      AuSpeed::Fastest, { "Nagra 4.2", "WM-D6C", "TC-D5M", nullptr }, "due microfoni alti al centro della sala, lontano dagli altoparlanti" },
    // Casa
    { "In casa", "Voci di famiglia", AuLoud::Speech, 0, false, { 3, 13, -1 }, {}, {}, 1, AuSpeed::Keep,
      { "Geloso", "EL 3300", "Revox A77", nullptr }, "il microfono sul tavolo in mezzo a chi parla" },
    { "In casa", "Provino musicale", AuLoud::Music, 40, false, { 11, 13, 10, -1 }, AU_MUSIC, {}, 1, AuSpeed::Fastest,
      { "Portastudio", "Revox A77", "Geloso", nullptr }, "una pista per strumento; livelli alti contro il fruscio" },
};
#undef AU_CARD
#undef AU_OMNI
#undef AU_EIGHT
#undef AU_MUSIC
#undef AU_SPEECH
#undef AU_WIND

// The presets of the mix (Mix e supporto): the kind of programme, the desks it was made on, the master's input.
struct AuProgram
{
    const char* cat;
    const char* name;
    AuLoud loud;
    int input;                 // 0 stereo, 1 mono
    const char* desks[3];      // names as in kAuConsoles, best first; nullptr ends
    const char* recorders[3];  // in Ripresa, for the tracks
    const char* tip;
};

inline constexpr AuProgram kAuPrograms[] = {
    { "", "Nessun preset", AuLoud::Music, 0, { nullptr }, { nullptr }, "" },
    { "Cinema", "Missaggio del film", AuLoud::Speech, 1, { "Neve", "RCA", nullptr }, { "Nagra 4.2", "Nagra III", nullptr },
      "dialoghi, musica ed effetti su piste separate, poi il missaggio in mono" },
    { "Cinema", "Documentario", AuLoud::Speech, 1, { "Neve", "RCA", nullptr }, { "Nagra 4.2", "Uher", nullptr },
      "voce in primo piano, musica di commento sotto di 10-15 dB" },
    { "Televisione", "Programma di studio", AuLoud::Speech, 1, { "Neve", "SSL", "RCA" }, { "Ampex 350", "Studer A80", nullptr },
      "voci al centro e vicine, applausi e musica sotto" },
    { "Televisione", "Telegiornale e servizi", AuLoud::Speech, 1, { "RCA", "Neve", nullptr }, { "Nagra 4.2", "Auricon", nullptr },
      "la voce del giornalista sopra l'ambiente del servizio" },
    { "Radio", "Radiodramma e varietà", AuLoud::Speech, 1, { "RCA", "REDD", nullptr }, { "Ampex 350", "Studer A80", nullptr },
      "voci, effetti e musica dal vivo in uno studio solo" },
    { "Radio", "Programma musicale", AuLoud::Music, 1, { "RCA", "Neve", nullptr }, { "Ampex 350", "Revox A77", nullptr },
      "dischi e nastri in onda, il livello costante del broadcast" },
    { "Sala di registrazione", "Musica anni '50", AuLoud::Music, 1, { "RCA", nullptr }, { "Ampex 350", "Revox A77", nullptr },
      "pochi microfoni, la stanza fa il suono; tutto in mono" },
    { "Sala di registrazione", "Beat e pop anni '60", AuLoud::Music, 0, { "REDD", "RCA", nullptr }, { "Studer J37", "Ampex 350", nullptr },
      "quattro piste: si riversa per liberarne" },
    { "Sala di registrazione", "Rock anni '70", AuLoud::Percussive, 0, { "Neve", "SSL", nullptr }, { "Studer A80", nullptr },
      "batteria a molti microfoni, basso diretto" },
    { "Sala di registrazione", "Pop anni '80", AuLoud::Percussive, 0, { "SSL", "Neve", nullptr }, { "Studer A80", "Portastudio", nullptr },
      "compressione su ogni canale, riverbero ampio" },
    { "Sala di registrazione", "Classica", AuLoud::Music, 0, { "Neve", "REDD", nullptr }, { "Studer A80", "Nagra 4.2", nullptr },
      "pochi microfoni lontani; il livello resta fermo" },
    { "In casa", "Registrazione casalinga", AuLoud::Music, 0, { "Mackie", nullptr }, { "Portastudio", "Revox A77", "WM-D6C" },
      "quattro piste a cassetta, livelli alti contro il fruscio" },
};

// The menu's entries: «Categoria · Preset», as the host lists them.
template <class T, size_t N> std::vector<std::string> auPresetNames(const T (&table)[N])
{
    std::vector<std::string> out;
    for (const T& p : table) out.push_back(*p.cat ? std::string(p.cat) + " · " + p.name : std::string(p.name));
    return out;
}

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

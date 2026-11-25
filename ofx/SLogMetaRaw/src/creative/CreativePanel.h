// SPDX-License-Identifier: GPL-3.0-or-later
// The Creative panel as data: every control, its group and the processes that show it. The factory describes it, the
// effect reads it and shows each row only with its processes. Names are permanent (Resolve saves by name).
#pragma once
#include <cstring>
#include <initializer_list>

#include "CreativeModel.h"

#define CR_BIT(p) (1u << (p))
const unsigned kCrAll = (1u << kCrProcessCount) - 1;
const unsigned kCrSignal = CR_BIT(kCrAnalog) | CR_BIT(kCrDigital);
const unsigned kCrPhoto = kCrAll & ~kCrSignal;                               // film and plates: every process kind
const unsigned kCrFilm = CR_BIT(kCrColourNegative) | CR_BIT(kCrReversal) | CR_BIT(kCrBlackWhite) | CR_BIT(kCrTechni3)
                         | CR_BIT(kCrTechni2) | CR_BIT(kCrAutochrome) | CR_BIT(kCrDufay) | CR_BIT(kCrThreePlates)
                         | CR_BIT(kCrKinemacolor);                           // moving film: gate, scratches
const unsigned kCrDeveloped = CR_BIT(kCrColourNegative) | CR_BIT(kCrReversal) | CR_BIT(kCrBlackWhite)
                              | CR_BIT(kCrTechni3) | CR_BIT(kCrTechni2) | CR_BIT(kCrKinemacolor);   // push, silver
const unsigned kCrHalo = kCrFilm | CR_BIT(kCrCollodion);
const unsigned kCrSeparate = CR_BIT(kCrTechni3) | CR_BIT(kCrTechni2) | CR_BIT(kCrThreePlates) | CR_BIT(kCrKinemacolor);
const unsigned kCrScreen = CR_BIT(kCrAutochrome) | CR_BIT(kCrDufay);
const unsigned kCrPrinted = CR_BIT(kCrColourNegative) | CR_BIT(kCrBlackWhite);   // a negative printed on a positive
const unsigned kCrColourPrint = CR_BIT(kCrColourNegative) | CR_BIT(kCrTechni3) | CR_BIT(kCrTechni2);   // printer lights
const unsigned kCrToned = CR_BIT(kCrDaguerreotype) | CR_BIT(kCrAlbumen) | CR_BIT(kCrCyanotype) | CR_BIT(kCrPlatinum);

enum CreativeGroup { kGrProcess, kGrShot, kGrPrint, kGrTexture, kGrSeparation, kGrScreen, kGrPlate, kGrSignal,
                     kGrColour, kGrView, kGrCount };
struct CrGroupDef { const char* name; const char* label; bool open; };
static const CrGroupDef kCrGroups[kGrCount] = {
    { "processGroup", "Dispositivo e pellicola", true },       { "shotGroup", "Ripresa e sviluppo", true },
    { "printGroup", "Stampa e copie", true },          { "textureGroup", "Grana e ottica", true },
    { "separationGroup", "Separazioni", true }, { "screenGroup", "Mosaico", true },
    { "plateGroup", "Lastra e carta", true },   { "signalGroup", "Segnale", true },
    { "colourGroup", "Colore", false },        { "viewGroup", "Proiezione e visione", true } };

static const char* const kCrOutputNames[3] = { "Rec.709 Gamma 2.4 (stampa)", "Log d'ingresso (scansione del negativo)",
                                               "Il negativo com'è (maschera arancione)" };

struct CrChoiceDef
{
    const char* name;
    const char* label;
    int CreativeControls::*field;
    const char* const* options;
    int count;
    unsigned shows;
    const char* hint;
    int group = kGrProcess;
};
static const CrChoiceDef kCrChoices[] = {
    { "device", "Dispositivo", &CreativeControls::device, kCrDeviceNames, kCrDeviceCount, kCrAll,
      "Che cosa ha ripreso l'immagine. Poi il modello (nomi veri), il formato, il produttore e la pellicola: ogni "
      "scelta carica il suo aspetto (curva, colore, grana, instabilità, ottica) nei comandi sotto, che restano "
      "modificabili. A mano: tutti i procedimenti, Technicolor e lastre compresi" },
    { "process", "Processo", &CreativeControls::process, kCrProcessNames, kCrProcessCount, kCrAll,
      "Il procedimento: come la luce diventa immagine. Ognuno ha i suoi comandi qui sotto" },
    { "filmCamera", "Cinepresa", &CreativeControls::filmCamera, kCrCameraNames.data(), kCrCameraCount, kCrAll,
      "La macchina: carica formato, instabilità del quadro, sfarfallio e carattere dell'obiettivo (vignettatura, "
      "aberrazione, diffusione)" },
    { "videoCamera", "Videocamera", &CreativeControls::videoCamera, kCrVideoCameraNames.data(), kCrVideoCameraCount,
      kCrAll, "La camera e il suo registratore: carica il sensore (tubo, CCD), il nastro, l'usura, il cambio testine e "
      "l'instabilità" },
    { "cinemaCamera", "Camera", &CreativeControls::cinemaCamera, kCrCinemaCameraNames.data(), kCrCinemaCameraCount,
      kCrAll, "Gamma dinamica, spalla, colore, nitidezza, rumore e sottocampionamento di quella camera" },
    { "phoneCamera", "Modello", &CreativeControls::phoneCamera, kCrPhoneCameraNames.data(), kCrPhoneCameraCount,
      kCrAll, "Il sensore, l'elaborazione e la compressione di quel telefono o di quella camera" },
    { "format", "Formato", &CreativeControls::format, kCrFormatNames, kCrFormatCount, kCrPhoto,
      "Le dimensioni del fotogramma: più piccolo = grana, alone e instabilità più grandi sull'immagine. La "
      "cinepresa carica il suo; sotto, i formati che monta" },
    { "maker", "Produttore", &CreativeControls::maker, kCrMakerNames.data(), kCrMakerCount, kCrAll,
      "Chi ha fatto la pellicola: sotto, le sue pellicole" },
    { "negative", "Pellicola", &CreativeControls::negative, kCrNegativeNames, kCrNegativeCount,
      CR_BIT(kCrColourNegative), "Il negativo: contrasto per strato, piede e spalla, interimage, grana, alone, coloranti" },
    { "print", "Positivo", &CreativeControls::print, kCrPrintNames, kCrPrintCount, CR_BIT(kCrColourNegative),
      "La pellicola positiva su cui si stampa il negativo: contrasto, dominante, nero. Nessuna = il negativo "
      "scansionato, piatto", kGrPrint },
    { "output", "Uscita", &CreativeControls::output, kCrOutputNames, 3, CR_BIT(kCrColourNegative),
      "Rec.709: la stampa vista in proiezione. Log: la scansione del negativo, riscritta nella codifica d'ingresso, "
      "per gradare dopo come un DI. Il negativo com'è: la pellicola sviluppata vista in luce, a colori complementari "
      "sulla maschera arancione, con la sua grana" },
    { "reversal", "Pellicola", &CreativeControls::reversal, kCrReversalNames, kCrReversalCount, CR_BIT(kCrReversal),
      "L'invertibile: un solo passaggio, il positivo è la pellicola stessa" },
    { "bwStock", "Pellicola", &CreativeControls::bwStock, kCrBWStockNames, kCrBWStockCount, CR_BIT(kCrBlackWhite),
      "Contrasto e grana dell'emulsione" },
    { "sensitivity", "Sensibilità", &CreativeControls::sensitivity, kCrSensitivityNames, kCrSensitivityCount,
      CR_BIT(kCrBlackWhite), "A quali colori è sensibile l'emulsione: blu = cieli bianchi e rossi neri; "
      "ortocromatica = rossi scuri; pancromatica = tutti" },
    { "filter", "Filtro di presa", &CreativeControls::filter, kCrFilterNames, kCrFilterCount, CR_BIT(kCrBlackWhite),
      "Il filtro davanti all'obiettivo, compensato in esposizione: giallo-rosso scuriscono il cielo, verde schiarisce "
      "le foglie" },
    { "techniEra", "Epoca", &CreativeControls::techniEra, kCrTechniEraNames, kCrTechniEraCount, CR_BIT(kCrTechni3),
      "Fino al 1944 la stampa aveva una chiave nera (K); dal 1955 le matrici vengono da un negativo monopack" },
    { "collodion", "Supporto", &CreativeControls::collodion, kCrCollodionNames, kCrCollodionCount,
      CR_BIT(kCrCollodion), "Vetro su fondo nero o ferro laccato: il nero dietro l'argento" },
    { "paper", "Carta", &CreativeControls::paper, kCrPaperNames, kCrPaperCount, CR_BIT(kCrAlbumen),
      "Salata: opaca, nelle fibre. Albumina: lucida, neri più profondi" },
    { "analog", "Formato video", &CreativeControls::analog, kCrAnalogNames, kCrAnalogCount, CR_BIT(kCrAnalog),
      "Banda del luma e del colore, ritardo del colore, rumore, testine e dropout del formato" },
    { "sensor", "Sensore", &CreativeControls::sensor, kCrSensorNames, kCrSensorCount, CR_BIT(kCrAnalog),
      "Che cosa legge la luce nella camera. Tubi: l'immagine ricorda i quadri prima (scia nelle ombre, code di "
      "cometa dietro le luci forti), a strisce con falso colore sui dettagli fini. CCD: striscia verticale sopra e "
      "sotto le luci fortissime. MOS: un disegno fisso di righe. Nessuno = il segnale com'è", kGrSignal },
    { "digital", "Camera", &CreativeControls::digital, kCrDigitalNames, kCrDigitalCount, CR_BIT(kCrDigital),
      "Gamma dinamica, spalla, colore, nitidezza, rumore e sottocampionamento della camera" },
    { "generation", "Generazione", &CreativeControls::generation, kCrGenerationNames, kCrGenerationCount, kCrPrinted,
      "Quante copie separano la stampa dal negativo: ogni duplicato aggiunge grana e contrasto e toglie colore. "
      "Le copie viste in sala erano di solito di distribuzione", kGrPrint },
    { "lens", "Ottica", &CreativeControls::lens, kCrLensNames.data(), kCrLensCount, kCrAll,
      "Un obiettivo d'epoca: carica distorsione, bordi morbidi, vortice, velo, striscia anamorfica, vignettatura, "
      "aberrazione e diffusione nei comandi qui sotto, poi si possono cambiare; porta anche il colore del suo vetro e "
      "dei suoi riflessi. Nessuna = quella della camera", kGrTexture },
    { "transfer", "Riversato su", &CreativeControls::transfer, kCrTransferNames, kCrTransferCount, kCrAll,
      "La copia che se ne faceva: un telecinema su cassetta, una copia VHS di copia, un kinescopio su pellicola, un "
      "film-out. Il risultato passa per quel supporto e ne prende i difetti, grana sopra grana. Nessuno = l'originale. "
      "Non vale con Uscita Log", kGrView },
    { "medium", "Proiettato o visto su", &CreativeControls::medium, kCrMediumNames, kCrMediumCount, kCrAll,
      "Dove si guarda il risultato, dopo l'uscita: sala, proiettore 16 mm o Super 8, televisore a tubo o Trinitron, "
      "monitor da regia, plasma, primo LCD, console portatile, carta. Carica nei comandi qui sotto gamma, contrasto, "
      "luce, bianco e colore di quel "
      "supporto: poi si possono cambiare. Nessuno = il monitor Rec.709. Non vale con Uscita Log", kGrView },
    { "scanView", "Inquadratura della scansione", &CreativeControls::scanView, kCrScanViewNames, 3,
      CR_BIT(kCrColourNegative) | CR_BIT(kCrReversal) | CR_BIT(kCrBlackWhite),
      "Quanto della pellicola si vede nella scansione. Il quadro: solo l'immagine. Con i bordi: il bordo del "
      "mascherino e le interlinee. Da bordo a bordo: tutta la larghezza della pellicola con le perforazioni (luce "
      "bianca dello scanner) e i quadri vicini, con le misure vere del formato", kGrView },
    { "base", "Supporto della pellicola", &CreativeControls::base, kCrBaseNames, 5,
      CR_BIT(kCrColourNegative) | CR_BIT(kCrReversal) | CR_BIT(kCrBlackWhite),
      "Il materiale della pellicola. Nitrato: solo 35 mm fino al 1951, ingiallisce e si decompone a macchie ambrate. "
      "Acetato (di sicurezza): con la sindrome dell'aceto si ritira, perde stabilità e fa cristalli. Poliestere: "
      "stabile, usato per le copie. Secondo epoca e formato lo sceglie dall'anno della pellicola", kGrShot },
    { "gateMask", "Mascherino della cinepresa", &CreativeControls::gateMask, kCrGateNames, 3,
      CR_BIT(kCrColourNegative) | CR_BIT(kCrReversal) | CR_BIT(kCrBlackWhite),
      "La forma del finestrino che delimita il quadro sulla pellicola, visibile nella scansione con i bordi: "
      "squadrato, ad angoli arrotondati, o limato e aperto fino alle perforazioni (il quadro le circonda)",
      kGrShot },
    { "mediumRoom", "Stanza", &CreativeControls::mediumRoom, kCrRoomNames, 3, kCrAll,
      "Dove lo si guarda: al buio il contrasto sembra minore (sala), con la luce accesa maggiore", kGrView } };
const int kCrChoiceCount = sizeof(kCrChoices) / sizeof(kCrChoices[0]);

struct CrSliderDef
{
    const char* name;
    const char* label;
    double CreativeControls::*field;   // its default is CreativeControls()'s
    double lo, hi, inc;
    int group;
    unsigned shows;
    const char* hint;
};
static const CrSliderDef kCrSliders[] = {
    // shot and development
    { "exposure", "Esposizione (stop)", &CreativeControls::exposure, -6, 6, 0.05, kGrShot, kCrAll,
      "La luce che arriva al materiale: sotto = ombre nel piede, sopra = luci nella spalla" },
    { "temperature", "Temperatura", &CreativeControls::temperature, -1, 1, 0.01, kGrShot, kCrAll,
      "Luce più calda (positivo) o più fredda di quella per cui il materiale è bilanciato" },
    { "tint", "Tinta", &CreativeControls::tint, -1, 1, 0.01, kGrShot, kCrAll, "Verde (negativo) o magenta" },
    { "push", "Sviluppo (stop)", &CreativeControls::push, -2, 3, 0.1, kGrShot, kCrDeveloped,
      "Spinto (positivo): più contrasto, più grana, ombre più dense. Tirato: più morbido" },
    { "baseDecay", "Degrado del supporto", &CreativeControls::baseDecay, 0, 1, 0.01, kGrShot,
      CR_BIT(kCrColourNegative) | CR_BIT(kCrReversal) | CR_BIT(kCrBlackWhite),
      "Quanto il supporto si è decomposto. Nitrato: l'immagine sbiadisce verso l'ambra a macchie che crescono dai "
      "bordi. Acetato: cristalli, e la pellicola ritirata oscilla di più in stampa e in proiezione" },
    { "scanOverscan", "Overscan della scansione", &CreativeControls::scanOverscan, 0, 2, 0.01, kGrView,
      CR_BIT(kCrColourNegative) | CR_BIT(kCrReversal) | CR_BIT(kCrBlackWhite),
      "Quanto margine oltre il quadro prende la scansione (con i bordi) o oltre la larghezza della pellicola (da bordo "
      "a bordo): 0 = stretto, 1 = il doppio; con valori alti si vedono più quadri vicini" },
    { "silver", "Argento trattenuto", &CreativeControls::silver, 0, 1, 0.01, kGrShot, kCrDeveloped,
      "Salto della sbianca (bleach bypass): l'argento resta con i coloranti, neri densi e colori spenti" },
    { "flash", "Preflash", &CreativeControls::flash, 0, 0.3, 0.005, kGrShot, kCrPhoto,
      "Una luce uniforme prima o dopo la posa: apre le ombre e abbassa il contrasto" },
    { "age", "Età", &CreativeControls::age, 0, 1, 0.01, kGrShot, kCrPhoto,
      "Decadimento: coloranti che sbiadiscono a ritmi diversi (il ciano per primo), base che ingiallisce" },
    // print
    { "printR", "Luci di stampa R", &CreativeControls::printR, -24, 24, 1, kGrPrint, kCrColourPrint,
      "Punti della stampatrice: 1 punto = 0,025 log E. Più rosso = stampa più ciano" },
    { "printG", "Luci di stampa G", &CreativeControls::printG, -24, 24, 1, kGrPrint, kCrColourPrint,
      "Più verde = stampa più magenta" },
    { "printB", "Luci di stampa B", &CreativeControls::printB, -24, 24, 1, kGrPrint, kCrColourPrint,
      "Più blu = stampa più gialla" },
    { "printContrast", "Contrasto", &CreativeControls::printContrast, 0.5, 2, 0.01, kGrPrint, kCrAll,
      "Il contrasto del positivo (o del segnale), moltiplicato su quello del processo" },
    { "printWeave", "Oscillazione della stampa", &CreativeControls::printWeave, 0, 3, 0.01, kGrPrint, kCrPrinted,
      "Ogni passaggio in stampatrice a contatto aggiunge una deriva lenta, le due pellicole scivolano un poco sul "
      "rocchetto: una copia campione ne ha uno, una copia di distribuzione tre. 1 = tipica" },
    { "printGrain", "Grana della stampa", &CreativeControls::printGrain, 0, 3, 0.01, kGrPrint, kCrPrinted,
      "La grana propria del positivo, nelle sue densità, più fine di quella del negativo: 1 = com'era" },
    // texture and optics
    { "grain", "Grana", &CreativeControls::grain, 0, 3, 0.01, kGrTexture, kCrPhoto,
      "Quantità rispetto a quella del materiale e del formato: 1 = com'era" },
    { "grainSize", "Dimensione grana", &CreativeControls::grainSize, 0.25, 4, 0.01, kGrTexture, kCrPhoto,
      "Rispetto a quella del materiale e del formato" },
    { "halation", "Alone", &CreativeControls::halation, 0, 3, 0.01, kGrTexture, kCrHalo,
      "La luce che attraversa l'emulsione e torna dal supporto: rosso attorno alle luci forti" },
    { "bloom", "Diffusione", &CreativeControls::bloom, 0, 1, 0.01, kGrTexture, kCrAll,
      "Un filtro di diffusione davanti all'obiettivo: le luci si allargano sulle zone vicine" },
    { "bloomRadius", "Raggio diffusione (%)", &CreativeControls::bloomRadius, 0.2, 10, 0.1, kGrTexture, kCrAll,
      "Raggio in percentuale dell'altezza" },
    { "aberration", "Aberrazione cromatica", &CreativeControls::aberration, 0, 1, 0.01, kGrTexture, kCrAll,
      "Rosso e blu a ingrandimenti diversi verso i bordi, come un obiettivo d'epoca" },
    { "vignette", "Vignettatura (stop)", &CreativeControls::vignette, -1, 3, 0.05, kGrTexture, kCrAll,
      "Caduta di luce agli angoli, prima della curva" },
    { "distortion", "Distorsione (%)", &CreativeControls::distortion, -10, 10, 0.1, kGrTexture, kCrAll,
      "Barilotto agli angoli (negativo = cuscinetto); gli anamorfici la piegano soprattutto in orizzontale" },
    { "edgeSoft", "Bordi morbidi", &CreativeControls::edgeSoft, 0, 3, 0.01, kGrTexture, kCrAll,
      "Il fuoco che cala verso gli angoli (campo curvo, astigmatismo); 1 = forte come un Petzval" },
    { "swirl", "Vortice", &CreativeControls::swirl, 0, 3, 0.01, kGrTexture, kCrAll,
      "La sfocatura dei bordi allungata in cerchio attorno al centro (Helios, Petzval); dedotta senza profondità, "
      "quindi anche sui soggetti ai bordi" },
    { "glare", "Velo (%)", &CreativeControls::glare, 0, 20, 0.1, kGrTexture, kCrAll,
      "La luce diffusa nell'obiettivo che solleva i neri vicino alle zone chiare, nel colore del suo trattamento" },
    { "streak", "Striscia anamorfica", &CreativeControls::streak, 0, 3, 0.01, kGrTexture, kCrAll,
      "Le luci forti si allungano in orizzontale, nel colore della serie (blu Panavision, viola LOMO, ambra Kowa)" },
    { "weave", "Ampiezza in ripresa", &CreativeControls::weave, 0, 1, 0.01, kGrTexture, kCrFilm,
      "La griffa della cinepresa lascia la pellicola un poco fuori posto a ogni fotogramma (soprattutto in verticale) "
      "e il pattino laterale la fa derivare di lato, lentamente: l'immagine si muove sulla pellicola. La cinepresa "
      "carica la sua: 0,1 a perni di registro, 0,5 a sola griffa, 0,85 il caricatore Super 8, 1 a manovella" },
    { "hair", "Pelo nel finestrino", &CreativeControls::hair, 0, 1, 0.01, kGrTexture, kCrFilm,
      "Un pelo o un grumo di emulsione raschiato dalla pellicola che resta nel finestrino della cinepresa: scuro, "
      "nitido, entra da un bordo, trema un poco e dura tutta l'inquadratura. Quota delle inquadrature che ne hanno "
      "uno: la cinepresa carica la sua (caricatori Super 8 e cineprese amatoriali più spesso, cineprese da studio "
      "pulite a ogni caricatore quasi mai)" },
    { "breath", "Sfarfallio", &CreativeControls::breath, 0, 1, 0.01, kGrTexture, kCrPhoto,
      "Densità e colore che cambiano un poco da un fotogramma all'altro" },
    { "dust", "Polvere", &CreativeControls::dust, 0, 1, 0.01, kGrTexture, kCrPhoto,
      "Granelli e peli, diversi a ogni fotogramma: bianchi se sul negativo, neri se sul positivo" },
    { "scratches", "Graffi", &CreativeControls::scratches, 0, 1, 0.01, kGrTexture, kCrFilm,
      "Righe verticali lasciate dal passaggio nella macchina" },
    // separations
    { "registration", "Registro (px a 1080)", &CreativeControls::registration, 0, 20, 0.1, kGrSeparation, kCrSeparate,
      "Ogni record (striscia, matrice o lastra) spostato del suo errore: frange colorate sui bordi" },
    { "interval", "Intervallo fra le pose (fotogrammi)", &CreativeControls::interval, 0, 48, 1, kGrSeparation,
      CR_BIT(kCrThreePlates) | CR_BIT(kCrKinemacolor),
      "Le pose sono in sequenza: ciò che si muove fra una e l'altra si separa nei colori. 0 = simultanee" },
    { "keyImage", "Chiave K (-1 = dell'epoca)", &CreativeControls::keyImage, -1, 1, 0.01, kGrSeparation,
      CR_BIT(kCrTechni3), "Immagine argento nera stampata sotto le tre matrici: contorni e neri più profondi" },
    { "dyeDensity", "Densità coloranti", &CreativeControls::dyeDensity, 0.6, 1.4, 0.01, kGrSeparation,
      CR_BIT(kCrTechni3) | CR_BIT(kCrTechni2), "Quanta tinta prendono le matrici nell'imbibizione" },
    // screens
    { "mosaicScale", "Dimensione mosaico", &CreativeControls::mosaicScale, 0.25, 4, 0.01, kGrScreen, kCrScreen,
      "Grani di fecola o maglia della rete, rispetto alla lastra vera" },
    { "clump", "Ammassi", &CreativeControls::clump, 0, 1, 0.01, kGrScreen, CR_BIT(kCrAutochrome),
      "I grani dello stesso colore che si raggruppano in macchie" },
    { "viewLight", "Luce di visione", &CreativeControls::viewLight, 0.1, 1.2, 0.01, kGrScreen, kCrScreen,
      "Quanto è forte la luce dietro la lastra: il mosaico assorbe due terzi della luce" },
    // plates and papers
    { "viewAngle", "Angolo di visione", &CreativeControls::viewAngle, -1, 1, 0.01, kGrPlate, CR_BIT(kCrDaguerreotype),
      "Lo specchio riflette la stanza: da 1 (positivo) a -1 (negativo, l'immagine si rovescia)" },
    { "uv", "Ultravioletto", &CreativeControls::uv, 0, 1, 0.01, kGrPlate,
      CR_BIT(kCrBlackWhite) | CR_BIT(kCrDaguerreotype) | CR_BIT(kCrCollodion) | CR_BIT(kCrAlbumen),
      "Quanto UV c'è nella luce: cieli e luci bluastre si bruciano prima. B/N: solo con sensibilità Blu" },
    { "oxidation", "Ossidazione", &CreativeControls::oxidation, 0, 1, 0.01, kGrPlate, CR_BIT(kCrDaguerreotype),
      "L'alone iridescente che entra dai bordi della lastra" },
    { "border", "Bordo", &CreativeControls::border, 0, 1, 0.01, kGrPlate,
      CR_BIT(kCrCollodion) | CR_BIT(kCrCyanotype) | CR_BIT(kCrPlatinum),
      "Il bordo irregolare dove il collodio o la soluzione non è arrivata" },
    { "toning", "Viraggio", &CreativeControls::toning, 0, 1, 0.01, kGrPlate, kCrToned,
      "Oro (dagherrotipo, albumina), tannino (cianotipia), palladio (platino): il colore dell'immagine" },
    // signal
    { "lag", "Scia del tubo", &CreativeControls::lag, 0, 3, 0.01, kGrSignal, CR_BIT(kCrAnalog),
      "Quanto il tubo trattiene i quadri prima: si vede nelle ombre e nei mezzitoni, poco nelle luci. 1 = quello "
      "del tubo" },
    { "comet", "Code di cometa", &CreativeControls::comet, 0, 3, 0.01, kGrSignal, CR_BIT(kCrAnalog),
      "La scia satura dietro le luci forti in movimento, che il fascio non riesce a scaricare. 1 = quella del tubo" },
    { "smear", "Smear del CCD", &CreativeControls::smear, 0, 3, 0.01, kGrSignal, CR_BIT(kCrAnalog),
      "La striscia verticale che le luci fortissime (sole, fari, lampadine) lasciano su tutta la colonna. 1 = "
      "quella del sensore" },
    { "wear", "Usura del nastro", &CreativeControls::wear, 0, 1, 0.01, kGrSignal, CR_BIT(kCrAnalog),
      "Nastro copiato e ripassato: più rumore, meno colore, dropout" },
    { "heads", "Cambio testine", &CreativeControls::heads, 0, 1, 0.01, kGrSignal, CR_BIT(kCrAnalog),
      "La banda disturbata in fondo al quadro dove una testina passa all'altra" },
    { "instability", "Instabilità", &CreativeControls::instability, 0, 1, 0.01, kGrSignal, CR_BIT(kCrAnalog),
      "Le righe ondeggiano: tensione del nastro e base dei tempi" },
    { "colourBleed", "Sbavatura colore", &CreativeControls::colourBleed, 0, 2, 0.01, kGrSignal, CR_BIT(kCrAnalog),
      "Banda e ritardo del colore, rispetto a quelli del formato" },
    { "noise", "Rumore", &CreativeControls::noise, 0, 2, 0.01, kGrSignal, kCrSignal,
      "Rispetto a quello del formato o della camera: 1 = tipico" },
    { "sharpness", "Nitidezza elettronica", &CreativeControls::sharpness, -1, 1, 0.01, kGrSignal, kCrSignal,
      "Il peaking della camera: contorni chiari attorno ai bordi" },
    { "range", "Gamma dinamica (stop)", &CreativeControls::range, -3, 3, 0.05, kGrSignal, CR_BIT(kCrDigital),
      "Sposta il punto di clip della camera" },
    { "highlights", "Spalla", &CreativeControls::highlights, 0, 1, 0.01, kGrSignal, CR_BIT(kCrDigital),
      "0 = clip netto, 1 = knee morbido" },
    { "compression", "Compressione", &CreativeControls::compression, 0, 2, 0.01, kGrSignal, CR_BIT(kCrDigital),
      "Sottocampionamento del colore e banding del codec" },
    // colour
    { "shadowHue", "Ombre: tinta", &CreativeControls::shadowHue, 0, 360, 1, kGrColour, kCrAll, "Gradi" },
    { "shadowAmount", "Ombre: quantità", &CreativeControls::shadowAmount, 0, 1, 0.01, kGrColour, kCrAll, nullptr },
    { "highHue", "Luci: tinta", &CreativeControls::highHue, 0, 360, 1, kGrColour, kCrAll, "Gradi" },
    { "highAmount", "Luci: quantità", &CreativeControls::highAmount, 0, 1, 0.01, kGrColour, kCrAll, nullptr },
    { "balance", "Bilanciamento (stop)", &CreativeControls::balance, -3, 3, 0.05, kGrColour, kCrAll,
      "Dove passano le ombre alle luci, in stop dal grigio" },
    { "saturation", "Saturazione", &CreativeControls::saturation, 0, 2, 0.01, kGrColour, kCrAll, nullptr },
    { "density", "Densità colore", &CreativeControls::density, 0, 1, 0.01, kGrColour, kCrAll,
      "Colori saturi più scuri, come i coloranti veri" },
    // viewing medium
    { "mediumAmount", "Intensità del supporto", &CreativeControls::mediumAmount, 0, 1, 0.01, kGrView, kCrAll,
      "1 = com'è davvero, 0 = il monitor Rec.709. Vale per tutto il supporto: risposta, geometria, righe, fosfori, "
      "retinatura, fibre" },
    { "mediumGamma", "Gamma del supporto", &CreativeControls::mediumGamma, 1.8, 2.8, 0.01, kGrView, kCrAll,
      "Come il supporto trasforma il segnale in luce: 2,4 = il monitor di riferimento (BT.1886), un tubo di casa "
      "2,5 (ombre più chiuse)" },
    { "mediumContrast", "Contrasto (x:1)", &CreativeControls::mediumContrast, 20, 10000, 10, kGrView, kCrAll,
      "Bianco diviso nero del supporto da solo: alza le ombre, non i bianchi. Tubo di casa 300, proiettore di casa "
      "300, carta 125 (Dmax 2,1), sala e monitor da regia 2000" },
    { "mediumAmbient", "Luce della stanza (%)", &CreativeControls::mediumAmbient, 0, 10, 0.1, kGrView, kCrAll,
      "Luce che cade sullo schermo e torna indietro: un velo uniforme su tutta l'immagine" },
    { "mediumNits", "Luminanza (nit)", &CreativeControls::mediumNits, 5, 400, 1, kGrView, kCrAll,
      "Luce del bianco. Più bassa del monitor (100) = meno colore e meno contrasto percepiti: sala 48, "
      "proiettore di casa 15, tubo 120-150" },
    { "mediumKelvin", "Bianco (K)", &CreativeControls::mediumKelvin, 3000, 10000, 50, kGrView, kCrAll,
      "Il bianco del supporto, con l'occhio adattato in gran parte: lampada 3200, xenon 6000, PAL 7500, NTSC "
      "giapponese 9300, monitor 6500" },
    { "mediumColour", "Colore (%)", &CreativeControls::mediumColour, 50, 150, 1, kGrView, kCrAll,
      "La manopola del colore del televisore: i televisori di casa erano spesso un po' alti" },
    { "projectorWeave", "Ampiezza in proiezione", &CreativeControls::projectorWeave, 0, 3, 0.01, kGrView, kCrAll,
      "Il proiettore muove tutta la pellicola sullo schermo, bordi compresi. In sala il rocchetto a croce di Malta "
      "gira di un quarto a fotogramma: un dente consumato ripete il suo salto ogni 4 fotogrammi. I proiettori 16 mm "
      "e Super 8 a griffa sbagliano a ogni fotogramma. Di lato la pellicola ondeggia fra i pattini. 1 = tipica" },
    { "mediumOverscan", "Overscan (%)", &CreativeControls::mediumOverscan, 0, 10, 0.5, kGrView, kCrAll,
      "Il tubo di casa ingrandisce l'immagine e ne nasconde i bordi; il monitor da regia la mostra intera" } };
const int kCrSliderCount = sizeof(kCrSliders) / sizeof(kCrSliders[0]);

// What the film applies, as check boxes (film processes only). Off: see CreativeControls.
struct CrToggleDef { const char* name; const char* label; int CreativeControls::*field; const char* hint; };
static const CrToggleDef kCrToggles[] = {
    { "useTone", "Curva e contrasto della pellicola", &CreativeControls::useTone,
      "Spento: la luce e il contrasto restano quelli del video, che esce nella sua codifica (niente stampa né "
      "supporto di visione)" },
    { "useColour", "Colore della pellicola", &CreativeControls::useColour,
      "Spento: i colori restano quelli del video; la pellicola dà solo la sua luminosità (e una B/N tiene il colore)" },
    { "useTexture", "Grana e texture", &CreativeControls::useTexture,
      "Grana, alone, diffusione, obiettivo, instabilità, polvere e graffi. Solo questo acceso: la grana di quella "
      "pellicola su quel formato sul tuo video, nient'altro" } };
const int kCrToggleCount = sizeof(kCrToggles) / sizeof(kCrToggles[0]);

// Switches that belong to a group, shown with their processes like the sliders (default on).
struct CrCheckDef { const char* name; const char* label; int CreativeControls::*field; int group; unsigned shows;
                    const char* hint; };
static const CrCheckDef kCrChecks[] = {
    { "camWeave", "Oscillazione in ripresa", &CreativeControls::camWeave, kGrTexture, kCrFilm,
      "Spento: il fotogramma resta fermo sulla pellicola, come con una cinepresa perfetta" },
    { "projWeave", "Oscillazione in proiezione", &CreativeControls::projWeave, kGrView, kCrAll,
      "Spento: la pellicola resta ferma sullo schermo, come in un proiettore perfetto" } };
const int kCrCheckCount = sizeof(kCrChecks) / sizeof(kCrChecks[0]);

inline bool crIs(const char* name, std::initializer_list<const char*> names)
{
    for (const char* n : names)
        if (!std::strcmp(name, n)) return true;
    return false;
}

// Rows that depend on another choice, hidden while they do nothing. c: the controls as the engine reads them
// (creativeResolve). The device hides the process and the variants its model or stock sets; each family shows its
// model; the film's toggles hide what they switch off. No print (scan or input encoding out) has no generations or
// print grain; that output has no medium; no medium has no intensity or response, and each medium shows only the
// response rows it has (the console none, overscan only on tubes).
inline bool crRowIdle(const char* name, const CreativeControls& c)
{
    const bool film = c.device == kCrDevFilm, manual = c.device == kCrDevManual;
    if (!std::strcmp(name, "process")) return !manual;
    if (!std::strcmp(name, "filmCamera") || !std::strcmp(name, "maker")) return !film;
    if (film && !std::strcmp(name, "format")) {   // a camera made for one format: nothing to choose (unless off it)
        const unsigned f = kCrCameras[c.filmCamera >= 0 && c.filmCamera < kCrCameraCount ? c.filmCamera : 0].formats;
        const bool in = c.format >= 0 && c.format < kCrFormatCount && (f & (1u << c.format)) != 0;
        return (f & (f - 1)) == 0 && in;
    }
    if (!std::strcmp(name, "videoCamera")) return c.device != kCrDevVideo;
    if (!std::strcmp(name, "cinemaCamera")) return c.device != kCrDevCinema;
    if (!std::strcmp(name, "phoneCamera")) return c.device != kCrDevPhone;
    if (crIs(name, { "negative", "reversal", "bwStock", "sensitivity" })) return film;
    if (!std::strcmp(name, "digital")) return !manual;
    const bool toggles = creativeToggles(c.process);
    if (crIs(name, { "useTone", "useColour", "useTexture" })) return !toggles;
    const bool tone = !toggles || c.useTone, colour = !toggles || c.useColour, texture = !toggles || c.useTexture;
    if (!tone && crIs(name, { "output", "print", "push", "silver", "flash", "printR", "printG", "printB",
                              "printContrast", "printGrain", "generation" }))
        return true;
    if (!colour && !std::strcmp(name, "filter")) return true;   // the records are the picture's own channels
    if (!texture && crIs(name, { "lens", "distortion", "edgeSoft", "swirl", "glare", "streak" })) return true;
    if (!texture && crIs(name, { "grain", "grainSize", "halation", "bloom", "bloomRadius", "aberration", "vignette",
                                 "weave", "hair", "breath", "dust", "scratches", "printGrain" }))
        return true;
    if (!texture && crIs(name, { "camWeave", "printWeave" })) return true;
    if (!std::strcmp(name, "weave") && !c.camWeave) return true;
    if (crIs(name, { "lag", "comet" })) return !creativeSensorTube(c.sensor);
    if (!std::strcmp(name, "smear")) return !creativeSensorCCD(c.sensor);
    const bool log = creativeLogOut(c);
    const bool scan = c.process == kCrColourNegative && (c.print == 0 || log || c.output == 2);
    if (crIs(name, { "generation", "printGrain", "printWeave" })) return scan;
    const bool negView = c.process == kCrColourNegative && c.output == 2;   // the negative as it is: no print
    if (!std::strcmp(name, "medium") || !std::strcmp(name, "transfer")) return log || negView;
    if (!std::strcmp(name, "gateMask")) return c.scanView == 0;
    if (!std::strcmp(name, "scanOverscan")) return c.scanView == 0;
    if (!std::strcmp(name, "baseDecay")) return creativeBase(c) == 4;   // polyester does not decay
    if (negView && crIs(name, { "print", "printR", "printG", "printB", "printContrast" })) return true;
    const bool none = log || negView || c.medium <= 0 || c.medium >= kCrMediumCount;
    if (!std::strcmp(name, "mediumAmount") || !std::strcmp(name, "mediumInfo")) return none;
    const CrMediumRows rows = creativeMediumRows(c.medium);
    if (!std::strcmp(name, "mediumKelvin")) return none || !rows.white;
    if (!std::strcmp(name, "mediumOverscan")) return none || !rows.overscan;
    if (!std::strcmp(name, "projWeave")) return none || !rows.weave;
    if (!std::strcmp(name, "projectorWeave")) return none || !rows.weave || !c.projWeave;
    if (!std::strncmp(name, "medium", 6) && std::strcmp(name, "medium") != 0) return none || !rows.tone;
    return false;
}

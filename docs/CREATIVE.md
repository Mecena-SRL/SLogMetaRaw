# Log MetaRaw Creative

Il secondo nodo del bundle (lo slot e l'identificatore `com.slogmetaraw.SLogMetaRawDetail` del vecchio Detail, sostituito
per intero il 2026-11-24). Sviluppo creativo: un solo motore rifà **il procedimento** fotografico o elettronico, non
un'imitazione del suo risultato. Nessun profilo, LUT o dato di prodotto: modelli parametrici nostri, con numeri stimati
dalle ricerche in [`docs/creative/`](creative/).

## Dove va

Dopo Log MetaRaw. Decodifica in luce lineare la codifica che riceve (Ingresso (IDT): dichiarato, da Resolve, dalla clip,
presunto S-Gamut3.Cine/S-Log3; uno spazio non riconosciuto rende il nodo neutro) e la porta in Rec.2020 lineare. L'uscita è
Rec.709 · Gamma 2.4, la stampa vista in proiezione; con il Negativo colore e *Uscita Log* è la scansione del negativo
riscritta nella codifica d'ingresso, da gradare come un DI. Spaziale: Generate LUT lo esclude. Temporale solo per le pose
in sequenza (Tre lastre, Kinemacolor), che leggono i fotogrammi successivi.

## Il motore (`math/Creative.h`, CPU `src/creative/CreativeEngine.cpp`, Metal `metal/kernels/Creative.metal`)

| Passo | Cosa fa |
|---|---|
| Record | fino a tre (strati, strisce, lastre): pesi spettrali su Rec.2020 lineare, letti ognuno nel suo punto (registro, aberrazione radiale, instabilità del quadro) e nel suo istante; esposizione, preflash, vignettatura. Se i tre leggono lo stesso fotogramma nello stesso punto la sorgente si decodifica una volta. Con le copie (Generazione) i record si sfocano come i duplicati (σ 0,6–1,3 px a 1080) e la grana del negativo si allarga e cala come un rumore sfocato |
| Alone e diffusione | su griglie ridotte (box, gaussiana separabile con σ/s ≤ 3 e pesi per ricorrenza, ritorno bilineare): la luce che torna dal supporto nei record, il filtro di diffusione |
| Curva | per record, in stop dal grigio: pendenza e offset H&D (sviluppo), piede e spalla softplus, interimage, grana del negativo e dei duplicati (value noise per strato, semi e grandezze propri), polvere e graffi, luci di stampa, curva del positivo (Naka-Rushton), grana della stampa nelle sue densità (RMS ∝ √D) |
| Immagine | sottrattiva (densità dei coloranti per una matrice di assorbimenti, chiave K, argento trattenuto, sbiadimento, macchia), additiva (filtri per mosaico di grani o rete) o monocroma (carta · 10^(−D·K), specchio, ossidazione, bordo) |
| Supporto di visione | facoltativo, sull'immagine finita (mai sull'uscita Log), in luce lineare Rec.709: proiezione (caduta agli angoli, sfarfallio, instabilità, flare, lampada, fuoco ai bordi), tubo (vetro curvo, righe gaussiane larghe secondo la luminosità che conservano la media, fosfori a strisce o fessure, angoli arrotondati), bagliore del vetro o dell'obiettivo (griglia ridotta della luminanza, energia conservata), LCD della console (scia: il 45 % del fotogramma precedente, renderizzato anch'esso; ritaglio 10:9, 160x144, quattro verdi con retino Bayer 4x4, griglia, cornice), carta (bianco, Dmax 2,1, fibre). Righe e fosfori si attenuano sotto 3 px per riga o per triade |
| Segnale | video analogico e digitale: Y Cb Cr, banda del luma (lungo la riga; 2D per le ottiche digitali), peaking, banda e ritardo del colore, blocchi di colore, rumore, testine, dropout, clip, knee, banding |

Il rumore è un hash intero deterministico: stessi bit su CPU e Metal, a ogni render, diverso per fotogramma. I raggi
seguono l'altezza del fotogramma: stesso aspetto in proxy e a piena risoluzione.

## I dati (`src/creative/CreativeModel.cpp`, pannello `src/creative/CreativePanel.h`)

Ogni processo è un caso di `creativePrepare` e righe di tabelle: negativi (20), invertibili (6), B/N (6) con sensibilità e
filtri, positivi (9), generazioni (4), formati (12, da 8 mm a IMAX 15/70), nastri (11), camere (10), supporti di visione (8). Le tabelle crescono solo in coda: Resolve
salva l'indice delle scelte. Il pannello è una tabella di righe con il gruppo e i processi che le mostrano; i nomi OFX
sono permanenti (`tests/golden/params_detail.txt`).

| Processo | Fonte delle scelte | Stato dei numeri |
|---|---|---|
| Negativo colore, invertibile, B/N, sviluppo, stampa | [PELLICOLE_E_SVILUPPO](creative/PELLICOLE_E_SVILUPPO.md) | stime da curve pubblicate e letteratura; da tarare |
| Pellicole dell'Est, formati grandi e ridotti | [FORMATI_E_PELLICOLE_RARE](creative/FORMATI_E_PELLICOLE_RARE.md) | stime [gen]; date e sensibilità sovietiche in parte non verificate |
| Technicolor, Autochrome, Dufay, tre lastre, Kinemacolor, dagherrotipo, collodio, stampe antiche | [PROCESSI_STORICI](creative/PROCESSI_STORICI.md) | strutture documentate, pesi e coloranti stimati |
| Video analogico | [VIDEO_ANALOGICO](creative/VIDEO_ANALOGICO.md) | bande e ritardi da specifiche di formato; rumore e difetti stimati |
| Camere digitali d'epoca | [CAMERE_DIGITALI](creative/CAMERE_DIGITALI.md) | categorie generiche, non camere precise; stimati |
| Generazioni e grana della stampa | [PELLICOLE_E_SVILUPPO](creative/PELLICOLE_E_SVILUPPO.md) | catena negativo → interpositivo → internegativo → copia documentata; quantità stimate |
| Supporti di visione | [VIDEO_ANALOGICO](creative/VIDEO_ANALOGICO.md), SMPTE 196M per la caduta di luce in sala | righe 576/480 e BT.1886 (il tubo è la curva 2.4) documentati; fasci, maschere, curvatura, flare stimati; i quattro verdi della console sono i valori usati dagli emulatori [gen] |
| Panoramica dei plugin del mercato | [RICERCA_PLUGIN_CREATIVI](creative/RICERCA_PLUGIN_CREATIVI.md) | solo per capire cosa offrire; nessun dato copiato |

## Verifiche

- `tests/test_creative.py`: ogni processo dà un'immagine finita in [0, 1]; la scala dei toni sale con la luce; Esposizione
  alza il grigio; il B/N resta neutro; le tre lastre separano ciò che si muove e con Intervallo 0 leggono solo il
  fotogramma corrente; l'uscita Log è più piatta della stampa e attorno al codice del grigio; stesso risultato con 1 e 4
  thread, diverso fra fotogrammi; il VHS sbava il colore più del broadcast; ogni pellicola, positivo e generazione sale
  con la luce; duplicati e grana della stampa aumentano la grana; ogni supporto cambia l'immagine e a intensità 0 la
  lascia com'è (salvo la console); l'uscita Log ignora il supporto; il tubo disegna righe e conserva la media; la
  console usa solo i suoi quattro verdi e tiene parte del fotogramma precedente; la carta ha il suo bianco e il suo nero;
  le copie sfocano i bordi; sala e tubo fanno bagliore attorno alle luci.
- Pannello: oltre al processo, alcune righe si nascondono quando un'altra scelta le rende inutili (`crRowIdle`):
  senza stampa niente Generazione e grana della stampa, con uscita Log niente supporto, senza supporto niente intensità.
  Le luci di stampa R G B compaiono solo dove si stampa a colori (negativo, Technicolor).
- `tests/test_metal.py::CreativeMetal`: parità CPU/Metal per ogni processo (solo nella CI macOS).
- `tests/test_ofx_host.py`: pannello, nodo Detail salvato che si apre come Creative, comandi visibili per processo.

## Aperto

- Non provato in Resolve; Metal non eseguito fuori dalla CI.
- Tutti i pesi spettrali, le matrici dei coloranti e le curve sono stime: vanno tarati su riprese vere (carte colore,
  scale di grigi), non su LUT altrui.
- Non modellati: la persistenza dei fosfori del tubo (pochi millisecondi, invisibile a 24-60 fps). La scia dell'LCD
  raddoppia il calcolo (rende anche il fotogramma prima) solo con la console scelta; Metal non è nei test di parità
  per la console (le quattro soglie amplificano differenze d'arrotondamento).
- Rimandati: grana ellittica e ottiche anamorfiche, compressione DCT a blocchi e smear dei CCD, infrarosso e falso
  colore, Polaroid, lith, redscale, variabilità di lotto delle pellicole dell'Est.

# Log MetaRaw Creative

Il secondo nodo del bundle (lo slot e l'identificatore `com.slogmetaraw.SLogMetaRawDetail` del vecchio Detail, sostituito
per intero il 2026-11-24). Sviluppo creativo: un solo motore rifà **il procedimento** fotografico o elettronico, non
un'imitazione del suo risultato. Nessun profilo, LUT o dato di prodotto: modelli parametrici nostri, con numeri stimati
dalle ricerche in [`docs/creative/`](creative/).

## Dove va

Dopo Log MetaRaw, o dopo un grade. Decodifica in luce lineare la codifica che riceve (Ingresso (IDT): dichiarato, da Resolve,
dalla clip, presunto S-Gamut3.Cine/S-Log3; dichiarabili anche Rec.709 con la curva BT.709 (grigio 0,41 = 18%), DaVinci WG,
ACEScct, ACEScg, ACES2065-1 e Cineon (nero 95, bianco 685, 300 code per decade); uno spazio non riconosciuto rende il nodo neutro) e la porta in Rec.2020 lineare. L'uscita è
Rec.709 · Gamma 2.4, la stampa vista in proiezione; con il Negativo colore e *Uscita Log* è la scansione del negativo
riscritta nella codifica d'ingresso, da gradare come un DI. Spaziale: Generate LUT lo esclude. Temporale solo per le pose
in sequenza (Tre lastre, Kinemacolor), che leggono i fotogrammi successivi.

## Il motore (`math/Creative.h`, CPU `src/creative/CreativeEngine.cpp`, Metal `metal/kernels/Creative.metal`)

| Passo | Cosa fa |
|---|---|
| Record | fino a tre (strati, strisce, lastre): pesi spettrali su Rec.2020 lineare, letti ognuno nel suo punto (registro, aberrazione radiale, instabilità del quadro) e nel suo istante; esposizione, preflash, vignettatura. Se i tre leggono lo stesso fotogramma nello stesso punto la sorgente si decodifica una volta. Con le copie (Generazione) i record si sfocano come i duplicati (σ 0,6–1,3 px a 1080) e la grana del negativo si allarga e cala come un rumore sfocato |
| Ottica | facoltativa, sui record (ricerca `creative-ricerca/v2/DIGITALI_OTTICHE.md` B.14-B.15): distorsione di Brown a barilotto per asse (gli angoli restano, anamorfici soprattutto in orizzontale) e colore del vetro dove i record si leggono; bordi morbidi e vortice come sfocatura ellittica allungata lungo il cerchio attorno al centro (16 campioni a spirale aurea, `cr_lens_soft`), senza profondità quindi anche sui soggetti ai bordi; velo (veiling glare) = quota della luminanza media locale su una griglia di raggio un quarto dell'altezza, nel colore del trattamento; striscia anamorfica per riga con decadimento esponenziale (lunghezza 12% della larghezza) delle luci oltre due stop sul grigio (`cr_streak_row`). 14 ottiche, dalla Cooke Speed Panchro al Petzval: caricano anche vignettatura, aberrazione e diffusione. Valori stimati [gen], da tarare; niente bokeh né ghost |
| Alone e diffusione | su griglie ridotte (box, gaussiana separabile con σ/s ≤ 3 e pesi per ricorrenza, ritorno bilineare): la luce che torna dal supporto nei record, il filtro di diffusione |
| Curva | per record, in stop dal grigio: pendenza e offset H&D (sviluppo), piede e spalla softplus, interimage, grana del negativo e dei duplicati (value noise per strato, semi e grandezze propri), polvere (granelli dai bordi irregolari e fibre, densità e nitidezza variabili, `cr_speck`) e graffi (spessore e intensità variabili lungo la riga), luci di stampa, curva del positivo (Naka-Rushton), grana della stampa nelle sue densità (RMS ∝ √D) |
| Immagine | sottrattiva (densità dei coloranti per una matrice di assorbimenti, chiave K, argento trattenuto, sbiadimento, macchia), additiva (filtri per mosaico di grani o rete) o monocroma (carta · 10^(−D·K), specchio, ossidazione, bordo) |
| Supporto di visione | facoltativo, sull'immagine finita (mai sull'uscita Log), in luce lineare Rec.709: proiezione (caduta agli angoli, sfarfallio, instabilità, flare, lampada, fuoco ai bordi), tubo (vetro curvo, righe gaussiane larghe secondo la luminosità che conservano la media, fosfori a strisce o fessure, angoli arrotondati), bagliore del vetro o dell'obiettivo (griglia ridotta della luminanza, energia conservata), LCD della console (scia: il 45 % del fotogramma precedente, renderizzato anch'esso; ritaglio 10:9, 160x144, quattro verdi con retino Bayer 4x4, griglia, cornice), carta (bianco, Dmax 2,1, fibre). Righe e fosfori si attenuano sotto 3 px per riga o per triade. Poi la risposta del dispositivo (`cr_device`): colore × effetto Hunt ((nit/100)^0,1), limite di gamut della carta, gamma del dispositivo × stanza (buia 0,92, penombra 1, illuminata 1,06; Bartleson-Breneman) × Stevens ((nit/100)^0,04), nero proprio BT.1886 (1/contrasto), luce della stanza additiva, bianco in K (locus di Planck, 40 % della dominante dopo l'adattamento), overscan. Valori del supporto (stime [gen]): sala 2,4 / 2000:1 / 0,4 % / 48 nit / 6000 K / buia; proiettore di casa 2,4 / 300:1 / 1,2 % / 15 nit / 3200 K / buia; tubo PAL 2,5 / 300:1 / 2 % / 120 nit / 7500 K / colore 105 % / overscan 6 % / illuminata; tubo NTSC uguale ma 150 nit, 9300 K, colore 112 %, overscan 7 %; monitor da regia 2,4 / 2000:1 / 0,4 % / 100 nit / 6500 K / penombra; carta 125:1 (Dmax 2,1) / 130 nit / colore 92 % / gamut 0,55. Scegliere il supporto carica questi valori nel pannello (`creativeLoadMedium`, `mediumLoaded`): un grade che non li ha usa quelli del supporto Supporti aggiunti (ricerca `DIGITALI_OTTICHE.md` C.4-C.7, [gen]): proiettore Super 8 (alogena 3100 K, hot spot forte, sfarfallio a 18 fps, jitter, angoli del quadro arrotondati), Trinitron (griglia ad apertura, due fili di smorzamento scuri a un terzo e due terzi), plasma (dithering a impulsi nelle ombre, diverso a ogni fotogramma), primo LCD (nero bluastro, pixel lenti, bianco freddo, 6 bit con bande arrotondate in modo continuo). Console di casa sul tubo PAL (`cr_console` davanti a `cr_scan`, tabella `kConsoles`): il centro 4:3 dell'immagine ritagliato al quadro della console, letto una volta per pixel (media di 4 campioni), nero intorno; 240 righe progressive, una per riga della console, quindi le righe nere fra l'una e l'altra; il quadro c'è anche a quantità 0 (è il dispositivo). NES 256x240, composito (più morbido lungo la riga): tavolozza generata come il 2C02, onda quadra fra due tensioni per livello (0,350/1,094, 0,518/1,506, 0,962/1,962, 1,550/1,962 V, nero 0,518, bianco 1,962), fase = tinta, 12 campioni, burst sulla tinta 8, YIQ FCC (`nesPalette`), colore più vicino pesato 2:4:3; non simula i limiti di tavolozza per riquadro. Master System 256x192 (bordo nelle 240 righe), 2 bit per canale. Mega Drive 320x224, 3 bit con i livelli del DAC misurati (0, 52, 87, 116, 144, 172, 206, 255). Super Nintendo 256x224 e Saturn 320x224, 5 bit. PlayStation 320x240, 5 bit con la matrice di retinatura 4x4 della GPU (-4..3, in unità a 8 bit). Uscita RGB via SCART per tutte tranne il NES [gen] |
| Oscillazione | tre macchine separate, calcolate sull'host per fotogramma (`creativeWeave`), senza ritmo: rumore a nodi (Catmull-Rom fra valori hash) più l'errore del singolo fotogramma. Cinepresa (spunta *Oscillazione in ripresa*): la griffa sbaglia a ogni fotogramma soprattutto in verticale (poca memoria), il pattino laterale fa derivare di lato su ~14 fotogrammi; ampiezza = valore della cinepresa × quella del formato, letta come picco (RMS = picco/2,5). Stampatrice a contatto: deriva lenta per passaggio (1 copia campione, 3 distribuzione e riduzione, 5 controtipo; √passaggi), solo se si stampa. Cinecamera e stampatrice muovono l'immagine sulla pellicola. Proiettore (spunta *Oscillazione in proiezione*, solo sui supporti che proiettano): muove tutta la pellicola sullo schermo; in sala il rocchetto intermittente a croce di Malta gira di un quarto a fotogramma e ripete il salto del dente ogni 4 fotogrammi, con l'ondeggiare laterale fra i pattini (~40 fotogrammi); 16 mm e Super 8 a griffa: errore a ogni fotogramma. Misure di riferimento (sayyes2analog, Image stability): ARRI 16SR meglio di 1/1250 dell'altezza, ARRI 35III e Aaton 35-II 1/1000, Aaton 35-III 1/2000, Konvas 0,02 mm, Kinor 0,01-0,015 mm; il Super 8 a caricatore e i proiettori sono stime [gen]. Prima della 4.3 era una sinusoide di periodo fisso (~7 fotogrammi) |
| Pelo nel finestrino | host per fotogramma (`creativeHair`): inquadrature di 96 fotogrammi, presenza se hash < valore della cinepresa (`CrCamera.hair`, 0 nelle fotocamere). 30% briciola d'emulsione (0,15-0,5 mm, larga 0,1-0,25 mm), 20% fibra di lanugine (due fili da una radice, 0,4-1,5 mm, 15-35 µm), altrimenti pelo (0,6-2,5 mm, 60-90 µm; 30% arricciato): misure assolute sull'altezza del finestrino del formato (`kGateMM`). Entra 0,1 mm oltre un bordo, 9 punti con curvatura che varia con continuità; si assottiglia verso l'estremo libero, che oscilla (lento + scatto per fotogramma) fino a 25 µm. Nel kernel (`cr_hair`): sezione tonda, densità 0,88-1 lungo il filo, ombra sfocata da cono di luce (fibra 20-150 µm davanti all'emulsione, f/2,8), emulsione (11 µm), scansione (8 µm) e sfocatura dei duplicati (`hairBlur`, px); toglie luce prima della curva H&D. Valori per cinepresa e distanze [gen] |
| Nastro analogico | emulato riga per riga (`cr_tape_row`, `cr_tape_out`; ricerca `creative-ricerca/v2/VIDEO_EMULAZIONE.md`): riga attiva di 52 µs, 288 righe per campo, ogni riga del quadro nel suo campo. Luma: pre-enfasi (un polo, τ 1,3 µs VHS, guadagno ×5), clip FM (bianco 160 %, nero −60 %), rumore FM bianco derivato (cresce con la frequenza), de-enfasi inversa esatta, passa-basso di riproduzione a due biquad (Butterworth o Bessel del 4° ordine, −3 dB 2,1 MHz VHS SP … 4,6 MHz 1"), crispener causale. Colore: tre poli (−3 dB 400 kHz VHS), ritardo Y/C, rumore AM e di fase, tinta ruotata dal jitter rimasto dopo l'aggancio al burst (un terzo), comb con la riga precedente del campo, saturazione compressa. Timebase dopo l'aggancio del TV: onda lenta, scrape, gradino del cambio testina a 7 righe dal fondo recuperato in ~2,5 righe, flagging in alto; a sinistra della riga spostata il blanking nero. Dropout in coordinate nastro (vita 6 campi, salgono di 1,5 righe per campo), coperti dalla riga sopra del campo e senza colore. Il rumore FM si tara in `creativePrepare` su una riga grigia: S/R VHS SP 39 dB, LP 37, S-VHS e Hi8 42, Betamax e Video8 40, U-matic 44-45, Betacam SP 48. Valori stimati [gen] da tarare; interlacciamento del movimento non ancora modellato |
| Sensore della videocamera | `cr_sensor` sui record, prima della curva della camera (ricerca VIDEO_EMULAZIONE.md §1.1-1.2). Tubo: modello per elemento di Philips 1963 in unità di carica del bianco per campo (luce → carica, fascio di capacità B a corrente piena fino al catodo, poi coda esponenziale K = C·V_T; quello che il fascio non riprende è la scia, il sovraccarico oltre B la coda di cometa, tenuto fino a qmax·B). Stato ricostruito a ogni quadro dai 3 quadri prima (2 campi ciascuno, il più vecchio tenuto 4 campi), quindi senza memoria nel plugin. K e B per canale dai data book (Plumbicon K 0,02, B 2, rosso più basso = scia rossa; Saticon B 3,5; vidicon K 0,08, gamma del target 0,65 corretta a metà; Newvicon K 0,03), bias light 1,2 % e ACT (qmax 1) nel Plumbicon anni '80. Tubi a strisce: alte luci verdi dove resta carica, falso colore = passa-alto del luma × portante (3,58 MHz, Newvicon 5) sommato al colore prima del colour-under, che lo riduce a chiazze lente. CCD: somma per colonna della luce sopra il pozzo pieno (`cr_smear_col`, un thread per colonna) su tutta la colonna; un chip la legge rosata. MOS: rumore a disegno fisso di righe e colonne. Smear molto più forte di un CCD vero (−80 dB): le luci arrivano già tagliate, quindi vale per sole e fari. Stime da tarare |
| Segnale | digitale: Y Cb Cr, banda del luma (lungo la riga; 2D per le ottiche digitali), peaking, banda e ritardo del colore, blocchi di colore, rumore, testine, dropout, clip, knee, banding |
| Riversaggio | facoltativo, mai con l'uscita Log: seconda passata dell'intero motore (`creativePrepareTransfer`) sull'immagine finita, letta come Rec.709 Gamma 2.4 con il grigio del display (0,115) riportato a 0,18; il supporto di visione si applica alla copia. Telecinema VHS e Betacam SP, VHS di terza generazione (usura, rumore 1,6, sbavatura 1,5), kinescopio 16 mm bianco e nero, film-out su negativo 35 mm con stampa. Valori stimati, da tarare |

Il rumore è un hash intero deterministico: stessi bit su CPU e Metal, a ogni render, diverso per fotogramma. I raggi
seguono l'altezza del fotogramma: stesso aspetto in proxy e a piena risoluzione.

## Preset (`src/creative/CreativePresets.h`)

Il pannello parte dal **Dispositivo**: Processo a mano (tutti i processi, il pannello di sempre e il default, così i
grade salvati prima non cambiano), cinepresa o fotocamera a pellicola, videocamera analogica, cinema digitale, telefono
e compatta. Ogni famiglia ha i suoi modelli con nome commerciale, scelti diversi tra loro; il nome dice a quale look si
mira, i numeri sono stime nostre dalle ricerche (`creative-ricerca/v2`: cineprese, formati, pellicole, digitali, video),
non profili misurati.

- Cinepresa (25): formato nativo e formati montati, instabilità (doppio pin 0,1 … manovella 1), sfarfallio, vignettatura,
  aberrazione e diffusione dell'obiettivo. Sceglierla li carica nei comandi (`creativeLoadDevice`, `deviceLoaded`); un
  grade che non li ha usa quelli del modello.
- Produttore (13) e pellicola (118): processo (negativo, invertibile, B/N), variante del motore da cui parte, formati in cui
  è esistita, grana (RMS; Vision3 relativa al 500T 5219: 50D 0,55, 250D 0,70, 200T 0,75, 800T 1,4, 5254 1,6, 5247 2,0),
  saturazione, contrasto, alone (CineStill senza rem-jet), dominante (Fuji più fredda, Est verde-ciano, Agfa oliva),
  sensibilità per le B/N antiche. `creativeResolve` ne ricava processo e variante; `creativeDeviceInfo` scrive i formati
  della cinepresa, dove è esistita la pellicola e i casi fuori formato (resi lo stesso).
- Scansione della pellicola (dalla 4.3, `cr_strip`, `cr_strip_light`, tabella `kStrips`): *Inquadratura della
  scansione* il quadro (com'era), il quadro con i bordi del mascherino (vista 1,12 volte il quadro) o da bordo a bordo
  (larghezza della pellicola × 1,08, lungo il trascinamento 1,15 passi: si vedono i quadri vicini, con la stessa
  immagine). Misure in mm per formato: pellicola, finestrino, scostamento del quadro, passo, perforazioni (BH
  2,794×1,854 a lati curvi, KS 2,794×1,981, 16 mm 1,829×1,270, Super 8 0,914×1,143; 16 mm a doppia perforazione,
  Super 16 e Super 8 da un lato, 9,5 mm al centro fra i quadri; foto, VistaVision e IMAX in orizzontale). Fuori dal
  finestrino la pellicola non è esposta: il processo la rende come Dmin (arancione nel negativo com'è, nero nella
  stampa e nell'invertibile); fuori dalla pellicola e nelle perforazioni la luce bianca dello scanner. L'oscillazione
  della cinepresa e della stampa sposta il quadro sulla pellicola, le perforazioni restano ferme. *Mascherino della
  cinepresa*: squadrato, arrotondato (raggio 8 % del lato corto [gen]) o limato aperto fino alle perforazioni (bordo
  irregolare di qualche centesimo di mm, l'immagine circonda le perforazioni). Fasi delle perforazioni del Super 8 e
  misure della perforazione del 9,5 mm e dello scostamento Academy: [gen]. Limiti: i quadri vicini ripetono il
  fotogramma corrente; vignettatura e pelo restano riferiti all'inquadratura, non al quadro.
  Dalla 4.3.1: *Overscan della scansione* (`scanOverscan`, 0-2, predefinito 0,1): margine oltre il quadro (con i bordi)
  o oltre la larghezza della pellicola (da bordo a bordo, ×0,7). Scritte sul bordo (`creativeEdgePrint` host,
  `cr_edge_print` kernel): font 5×7 a punti che si fondono (`kCrFont`), lettere alte ~0,7 mm (7 righe da 0,1 mm, meno
  se il margine è stretto) nel margine fuori dalle perforazioni (dalla parte della fila unica, letta al contrario),
  esposizione latente 1,5: scure sul negativo, chiare sull'invertibile; sulla copia (`printClear`) il testo è quello
  della pellicola da stampa (5385/5381/5384/2383, 5302/2302 in b/n, FUJI 3510 dal 1999 [gen]), scuro sul bordo chiaro. Testo: marca
  (EASTMAN prima del 1990, KODAK dopo; CineStill = KODAK), codice del formato (regola 5xxx→7xxx, 85xx→86xx come nelle
  liste per formato) o prima parola del nome, NITRATE FILM / SAFETY FILM / ESTAR da `creativeBase`, poi: dal 1990
  Kodak e Fuji in 16/35/65 il KeyKode (lettere, numeri del rotolo, metraggio a 4 cifre e 52 moduli di codice a barre);
  prima i segni della data Kodak (punto, triangolo, quadrato) e un numero di 6 cifre; 135 e 120 il numero del
  fotogramma con «>nnA». Si ripete appena finita (multiplo del passo delle perforazioni) e scorre col film di un passo
  per fotogramma: più fitta del vero (mezzo piede o un piede) perché si veda in un quadro solo [gen]. Luce
  (`cr_strip_light`): calo verso gli angoli 5 %, alone delle ottiche 10 % con raggio max(0,04 mm, 5 px), riga del
  taglio della base 20 % nitrato, 28 % acetato, 45 % poliestere, larga max(0,015 mm, 1 px); tolleranza delle
  perforazioni ±4 µm per foro [gen].
  Copia e colonna sonora (dalla 4.3.1, `creativeSoundtrack` host, `cr_sound`/`cr_margin`/`cr_mag` kernel; ricerca su
  SMPTE 40, 41, 112, 139, 164, 176, 195, ST 203, RP 115, ISO 17266, Pritchard; mm dal bordo guida): la stampatrice
  espone solo il quadro intero della cinepresa e la zona della pista (35 mm 4,88-7,82), il resto della copia resta
  chiaro con le scritte scure della pellicola da stampa; perforazioni KS 2,794×1,981. Pista ottica 4,90-7,44 mm,
  modulata 5,21-7,14 (due piste stereo affiancate); DTS 7,42-7,67; Dolby Digital 2,41 mm quadrato (76×76 bit, logo
  DD al centro) in ogni intervallo tra le perforazioni lato pista; SDDS 0,295-1,831 mm da entrambi i bordi (al posto
  delle scritte). Con la pista Super 35 e Techniscope si stampano Academy (22×16,03, centro +1,3 mm) o 4 perf scope.
  16 mm: perforazione singola, pista 13,46-15,49 mm dal bordo perforato; magnetica 14,55 mm, larga 2,41. Super 8:
  pista magnetica 7,24-7,92 mm, bilanciamento 0,08-0,38. Epoca (`printSound` = 0, anno della pellicola): muta prima
  del 1928, densità variabile fino al 1954, area variabile mono, Dolby Stereo dal 1976, digitali dal 1993 (SDDS fino al
  2007 [gen]); 16 mm area variabile dal 1933. Pista ciano (solo lo strato sensibile al rosso) sulle copie a colori dal
  2004; i digitali sono solo colorante (ciano scuro). Il suono (inviluppo di frasi e portante, rumore) è [gen]; la
  riduzione di rumore lascia una riga sottile nelle pause.
- Il negativo com'è (*Uscita*, dalla 4.3, `cr_negative`): densità per strato = gamma del negativo 0,6 per log E (0,18
  per stop) da un piede morbido 4,5 stop sotto il grigio, coloranti complementari con il poco di assorbimento indesiderato
  che la maschera lascia (5-10 % [gen]), sulla base arancione (Status M 0,24 / 0,62 / 0,92, tipo Vision [gen]), vista
  in trasparenza. La grana è quella del negativo, in densità; niente stampa, niente supporto di visione.
- Supporto della pellicola (dalla 4.3, `creativeBase`, `cr_base`): nitrato, diacetato, triacetato, poliestere, oppure
  secondo epoca e formato dall'anno della pellicola (nitrato solo 35 mm e più larghi fino al 1951; i formati ridotti
  sempre di sicurezza, diacetato fino al 1948 e poi triacetato; il negativo di ripresa mai su poliestere [gen date]).
  Un supporto nuovo è trasparente: degradandosi ingiallisce, e la sua densità si aggiunge a ciò che si vede attraverso
  (invertibile, stampa, negativo com'è: nitrato fino a 0,04/0,08/0,20, acetato 0,01/0,02/0,05, poliestere niente
  [gen]); senza degrado l'immagine non cambia. *Degrado del supporto*: nitrato, l'immagine sbiadisce verso il
  centro e vira all'ambra a macchie che crescono dai bordi e restano da un quadro all'altro; acetato (sindrome
  dell'aceto), cristalli diversi a ogni quadro e la pellicola ritirata oscilla fino a 2,5 volte di più in stampa e in
  proiezione; poliestere: niente. Riversaggio: il supporto della copia non si sceglie (kinescopio su acetato, film-out
  su poliestere).
- Pellicola per formato (dalla 4.3): il pannello mostra prima la lista del calibro del formato scelto (`kCrGaugeLists`:
  8, Super 8, 9,5, 16, 35, 65, 135, 120), con le pellicole di tutti i produttori fatte in quel calibro e il codice di
  quel calibro (`creativePickLabel`: Kodak 5xxx → 7xxx in 16 mm e 8 mm, Fuji 85xx → 86xx in 16 mm). È una vista:
  sceglierne una scrive produttore e pellicola (`pickStock`), che restano lo stato salvato, quindi i grade di prima non
  cambiano; cambiare cinepresa, formato o produttore riallinea la lista (`syncPick`). Le liste sono fisse e si
  allungano solo in fondo (Resolve salva l'indice); un test verifica che ogni pellicola compaia una volta in ogni
  calibro in cui è esistita. La spunta *Tutte le pellicole, per produttore* riporta i listini completi, che compaiono
  anche quando la pellicola scelta non è mai esistita in quel calibro. Una cinepresa di un solo formato nasconde il
  formato.
- Formati nuovi: 35 mm Academy, 135 e 120 (grana e dimensione ∝ M^0,32 e M^0,65, M = √(area S35/area)).
- Videocamere (13): nastro (con usura, testine, instabilità) e sensore (tubi a 3 o a 1 tubo a strisce, CCD a 1 o 3
  chip).
- Cinema digitale (9) e telefoni e tascabili (10): la riga delle camere per curva, colore e rumore, più la catena del
  modello (ricerca DIGITALI_OTTICHE.md A.0 e A.7, stime): righe del sensore (0 = quelle del quadro), aliasing (salto di
  righe, niente filtro ottico: un campione per pixel), nitidezza dell'ISP (maschera di contrasto 3x3), qualità del
  codec come JPEG (scala delle tabelle: Q < 50 → 50/Q, sopra 2 − Q/50, per *Compressione*), sottocampionamento del
  colore. Motore: `cr_dig_down`, `cr_dig_sharp`, `cr_dig_dct` (un thread per blocco 8x8, quantizzazione arrotondata con
  un gradino continuo per la parità CPU/GPU), `cr_dig_out` (bilineare). Non ancora: rolling shutter, esposizione e
  bilanciamento automatici, GOP e macroblocchi in movimento (servono più quadri).
- Interruttori (solo pellicole): *Curva e contrasto* spento = curva retta e uscita nella codifica d'ingresso (niente
  stampa né supporto); *Colore* spento = record uguali ai canali del video, strati uguali, niente dominanti (una B/N tiene
  il colore del video); *Grana e texture* spento = niente grana, alone, diffusione, ottica, instabilità, polvere, graffi.
  Tutti e tre spenti = il video com'è (verificato); solo grana = la grana dello stock e del formato sul video.

## I dati (`src/creative/CreativeModel.cpp`, pannello `src/creative/CreativePanel.h`)

Ogni processo è un caso di `creativePrepare` e righe di tabelle: negativi (20), invertibili (6), B/N (6) con sensibilità e
filtri, positivi (9), generazioni (4), formati (15, da 8 mm a IMAX 15/70, con 135 e 120), nastri (11), camere (10), supporti di visione (8). Le tabelle crescono solo in coda: Resolve
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
  le copie sfocano i bordi; sala e tubo fanno bagliore attorno alle luci; con tutto spento la pellicola restituisce il
  video; la sola grana conserva i valori e cresce con lo stock (50D < 500T < 5247) e col formato piccolo; Double-X è B/N
  e senza colore tiene quello del video; un modello digitale o video rende come la sua riga; sul nastro la parte a
  sinistra di un bordo resta piatta e un gradino grande sale più lentamente di uno piccolo (clip FM), il rumore è
  correlato lungo la riga e indipendente fra righe, il colore arriva in ritardo. Oscillazione: di lato la cinepresa
  deriva (correlazione a 1 fotogramma > 0,75), in verticale salta (< 0,5), nessun periodo; il Super 8 si muove 20 volte
  l'Arriflex 435; la sala ripete il salto ogni 4 fotogrammi, il Super 8 a griffa no e si muove di più; ogni passaggio di
  stampa aggiunge deriva; spunte spente e negativo scansionato = fermo.
- Pannello: il dispositivo nasconde il processo e le varianti che il modello o lo stock impostano; gli interruttori
  nascondono ciò che spengono. Oltre al processo, alcune righe si nascondono quando un'altra scelta le rende inutili (`crRowIdle`):
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
- Rimandati: grana ellittica, bokeh e ghost delle ottiche, desqueeze anamorfico, infrarosso e falso colore, Polaroid, lith, redscale, variabilità di lotto delle pellicole dell'Est.

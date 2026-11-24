# Simulazione di "look" di camere digitali: ricerca per un plugin OFX originale

Data: 2026-11-24. Stato: ricerca. Nessun codice è stato scritto o modificato.

## Legenda e perimetro

- **[doc]**: il fatto viene da una fonte consultata in questa ricerca; il numero tra parentesi rimanda all'elenco delle fonti in fondo.
- **[gen]**: conoscenza tecnica generale non verificata qui su una fonte primaria. Va controllata prima di diventare una costante nel codice.
- **[prop]**: proposta di progetto, cioè la nostra scelta. Non è un fatto.

Il plugin implementa **modelli parametrici propri**. Non copia LUT, profili o tabelle proprietarie (LUT ARRI K1S1/REVEAL, profili FilmConvert, LUT DJI e simili). Si possono usare legittimamente:

1. curve log e primarie di gamut **pubblicate** dai produttori in white paper o documenti tecnici;
2. formati e standard pubblici (DV, HDV, JPEG, MPEG-2, H.264, Rec.709);
3. descrizioni qualitative del comportamento pubblicate dai produttori o dalla stampa tecnica;
4. dati scientifici sulla sensibilità spettrale delle camere (database accademici con licenza da verificare).

Nota di metodo, in linea con le regole del repository: le tabelle dei parametri degli archetipi (sezione 5) sono **un modello scritto a mano**. Sono plausibili e descritte con riferimenti, ma non vengono da misure. Vanno tarate su materiale reale e verificate con test prima di presentarle come "fedeli".

---

## 1. Matching e simulazione di camere

### 1.1 Come lavorano gli strumenti commerciali

**FilmConvert CineMatch.** FilmConvert dichiara due passaggi [doc, 9]:

1. un *sensor match*, che porta i dati della camera sorgente a come la camera di destinazione avrebbe registrato la stessa scena;
2. la riapplicazione della *color science* (gamma/gamut) della camera di destinazione.

I profili vengono costruiti riprendendo ogni camera "nelle stesse condizioni". Il metodo di misura non è pubblicato [doc, 9]. FilmConvert sottolinea la differenza da una Color Space Transform (CST): una CST è una conversione matematica tra spazi colore e ignora le differenze fra sensori, per esempio fra una VENICE e una FX3 che condividono S-Log3/S-Gamut3 [doc, 9].

Conseguenza per noi: la "firma" di una camera non sta nella curva log, che è solo una codifica. Sta (a) nella risposta spettrale del sensore e nella matrice che la porta nel gamut nativo e (b) nella trasformazione di visualizzazione (tone map, gamut map, look).

**Colourlab Ai.** È un'app macOS che usa computer vision e machine learning per abbinare inquadrature [doc, 10]. L'esito viene scritto come parametri ASC CDL (lift/gamma/gain, printer points). Ci sono diversi modelli "percettivi" (Dailies, Final, Theatrical, Balance, RGB, Perceptual 1/2). Il round-trip verso Resolve produce 4 nodi: trasformazione d'ingresso, nodo CDL di match, LUT di show opzionale, trasformazione d'uscita [doc, 10]. L'algoritmo interno non è pubblicato [doc, 10].

Lezione per noi: un buon risultato si ottiene anche con **pochi parametri primari interpretabili** applicati in uno spazio di lavoro coerente. È un argomento a favore di controlli semplici.

### 1.2 Cosa distingue davvero la color science dei produttori

Struttura comune, a cui si aggiungono le specificità per produttore:

| Componente | Dove agisce | Effetto visibile |
|---|---|---|
| Sensibilità spettrale CFA (filtri R, G, B) | sensore | separazione delle tinte (tessuti, fogliame, pelle), metamerismo sotto luci LED/fluorescenti |
| Matrice 3x3 camera RGB → gamut nativo (spesso dipendente dalla temperatura colore) | camera | saturazione e tinta "di base" dei primari |
| Codifica log + gamut | camera, file | solo contenitore (reversibile), non è un look |
| Tone map di visualizzazione (curva S, roll-off delle alte luci, toe) | LUT o DRT | contrasto, morbidezza delle alte luci, nero |
| Gamut mapping / compressione della saturazione con l'esposizione | LUT o DRT | come i colori saturi "sbiancano" nelle alte luci |
| Correzioni dipendenti dalla tinta (3D LUT) | look | pelle, cieli, verdi |
| Demosaicing, sharpening, NR, codec | camera | texture, aliasing, rumore |

Esempio ARRI, dalla documentazione del produttore:

- **ALEXA (SUP 3.x, LogC3)** [doc, 4]: 14 stop di gamma dinamica (rapporto lineare oltre 15.000:1). A EI 400 ci sono circa 6,5 stop sopra il grigio medio; ogni raddoppio di EI sposta uno stop dalle ombre alle alte luci. Il nero LogC è a 95/1023 e il massimo vale 917/1023 a EI 400 e 976/1023 a EI 800. La matrice cambia con la temperatura colore, così la luce mantiene parte del suo carattere. La tone map per il monitor comprime ombre e alte luci. In SUP 3.x i colori fuori gamut sono mappati "più dolcemente", restano più saturi al crescere dell'esposizione e passano alla sovraesposizione in modo più graduale. Sono ridotti i verdi clippati sul fogliame saturo in luce forte. In P3 la curva è più morbida e la saturazione complessiva più bassa [doc, 4].
- **REVEAL (ALEXA 35/265)** [doc, 1]: catena di debayer ADA-7, motore colore ACE4, gamut AWG4 e curva LogC4. ARRI dichiara "colori brillanti saturi molto migliorati", "resa piacevole e naturale di tutti gli incarnati" e coerenza tra SDR e HDR. AWG4 è descritto come più grande di Rec.2020 ma con pochi "colori virtuali" e contenuto in ACES [doc, 1]. La pagina ARRI su REVEAL **non** cita K1S1 né il roll-off [doc, 1].
- **K1S1** [gen]: è la LUT di visualizzazione Rec.709 "classica" di ALEXA (LogC3 → Rec.709), descritta come look neutro con alte luci morbide. È proprietaria e non va copiata. Si può emulare il *comportamento* con una nostra curva: toe morbido, spalla lunga e desaturazione progressiva nelle alte luci.

Tendenze per produttore [gen], descritte in modo prudente perché dipendono da modello, firmware e LUT:

- **ARRI**: alte luci con spalla lunga, pelle calda e "compatta" (bassa varianza di tinta), saturazione contenuta nelle alte luci, rumore fine e organico, sharpening interno basso.
- **RED (IPP2)**: pipeline scena-riferita con un output transform separato e controlli di contrasto e roll-off delle alte luci selezionabili [gen; descrizione IPP2 in 13]. Tradizionalmente più contrasto e verdi/gialli più vivaci. La resa varia molto tra sensori (MX, Dragon, Helium, Monstro, V-Raptor).
- **Sony (S-Log3/S-Gamut3.Cine)**: con i LUT di fabbrica la resa è spesso percepita come più "fredda" o verde-ciano nei cieli e nel fogliame. Venice ha una matrice dedicata (colour-science contiene primarie specifiche "Venice S-Gamut3") [doc, 14].
- **Canon**: incarnati caldi e rosati, rossi saturi e una resa "piacevole" pronta per la consegna.
- **Blackmagic (Gen 5)**: Film Gen 5 con molta latitudine. Le LUT di fabbrica sono abbastanza neutre e sul materiale compresso sono frequenti problemi di saturazione nei rossi.

### 1.3 Curve log e gamut pubblicati (utilizzabili come contenitori)

Servono per **leggere** materiale reale o per **scrivere** un'uscita "come se" fosse di quella camera. Non sono un look.

**Curve di codifica (lineare di scena → valore di codice normalizzato 0–1):**

| Curva | Formula pubblicata (sintesi) | Fonte |
|---|---|---|
| ARRI LogC3 (EI 800) | `E > cut: c·log10(a·x+b)+d; altrimenti e·x+f` con cut=0,010591, a=5,555556, b=0,052272, c=0,247190, d=0,385537, e=5,367655, f=0,092809 | [doc, 4][doc, 14] |
| ARRI LogC4 | `x ≥ t: (log2(a·x+64)−6)/14·b+c; altrimenti (x−t)/s`, a=(2^18−16)/117,45, b=(1023−95)/1023, c=95/1023, s=7·ln2·2^(7−14c/b)/(a·b), t=(2^(14(−c/b)+6)−64)/a | [doc, 2] |
| RED Log3G10 v2 | `y = sign(x+0,01)·0,224282·log10(|x+0,01|·155,975327+1)` | [doc, 14] |
| Sony S-Log3 | `x ≥ 0,01125: (420+log10((x+0,01)/0,19)·261,5)/1023; altrimenti (x·(171,2102946929−95)/0,01125+95)/1023` | [doc, 5] |
| Canon Log 2 (v1.2) | `0,24136077·log10(x·87,09937546+1)+0,092864125` (ramo positivo) | [doc, 14] |
| Canon Log 3 (v1.2) | tre rami: lineare `1,9754798·x+0,12512219` al centro, log `0,36726845·log10(x·14,98325+1)+0,12240537` in alto | [doc, 14] |
| Panasonic V-Log | `x < 0,01: 5,6·x+0,125; altrimenti 0,241514·log10(x+0,00873)+0,598206` | [doc, 6] |
| BMD Film Gen 5 | `x < 0,005: 8,283605932·x+0,092465753; altrimenti 0,086928761·ln(x+0,005494072)+0,530013339` | [doc, 14] (cita il documento BMD "Generation 5 Color Science") |
| DaVinci Intermediate | `L > 0,00262409: (log2(L+0,0075)+7)·0,07329248; altrimenti L·10,44426855` | [doc, 7] |
| Apple Log | costanti R0=−0,05641088, Rt=0,01, σ=47,28711236, β=0,00964052, γ=0,08550479, δ=0,69336945 (white paper Apple 2023, IDT ACES) | [doc, 14] |
| DJI D-Log (Zenmuse X7, 2017) | ramo log `0,256663·log10(0,9892·x+0,0108)+0,584555` | [doc, 14] (cita il white paper DJI 2017) |
| DJI **D-Log M** | **nessuna formula pubblica trovata**; DJI distribuisce solo LUT | incerto |

Gamme dinamiche dichiarate da Canon [doc, 8]: Canon Log 800% (≈3 stop sopra il grigio, secondo la nostra conversione), Canon Log 3 1600% (≈4 stop), Canon Log 2 6400% (≈6 stop). Il documento ha un'incoerenza interna sul riferimento, quindi vanno usati come ordine di grandezza.

**Primarie di gamut (CIE 1931 xy, bianco D65 salvo diversa indicazione):**

| Gamut | R (x, y) | G (x, y) | B (x, y) | Fonte |
|---|---|---|---|---|
| ARRI Wide Gamut 3 | 0,6840, 0,3130 | 0,2210, 0,8480 | 0,0861, −0,1020 | [doc, 14] |
| ARRI Wide Gamut 4 | 0,7347, 0,2653 | 0,1424, 0,8576 | 0,0991, −0,0308 | [doc, 2] |
| REDWideGamutRGB | 0,780308, 0,304253 | 0,121595, 1,493994 | 0,095612, −0,084589 | [doc, 14] |
| S-Gamut3 (= S-Gamut) | 0,7300, 0,2800 | 0,1400, 0,8550 | 0,1000, −0,0500 | [doc, 14] |
| S-Gamut3.Cine | 0,7660, 0,2750 | 0,2250, 0,8000 | 0,0890, −0,0870 | [doc, 14] |
| Canon Cinema Gamut | 0,7400, 0,2700 | 0,1700, 1,1400 | 0,0800, −0,1000 | [doc, 14] |
| Panasonic V-Gamut | 0,730, 0,280 | 0,165, 0,840 | 0,100, −0,030 | [doc, 6] |
| Blackmagic Wide Gamut (Gen 4/5) | 0,7177, 0,3171 | 0,2280, 0,8616 | 0,1006, −0,0820 | [doc, 14] |
| DaVinci Wide Gamut | 0,8000, 0,3130 | 0,1682, 0,9877 | 0,0790, −0,1155 | [doc, 7] |
| DJI D-Gamut | 0,71, 0,31 | 0,21, 0,88 | 0,09, −0,08 | [doc, 14] |
| Apple Log | gamut Rec.2020 (0,708/0,292; 0,170/0,797; 0,131/0,046) | | | [gen] (Apple Log usa primarie BT.2020) |

I valori presi da colour-science (pacchetto open source BSD-3, versione 0.4.7) riportano i documenti dei produttori. Prima di scolpirli nel codice conviene incrociarli con il white paper originale.

### 1.4 Sensibilità spettrale: perché una matrice 3x3 basta "quasi"

- **Jiang, Liu, Gu, Süsstrunk (WACV 2013)** [doc, 3] hanno misurato le funzioni di sensibilità spettrale di **28 camere** (reflex professionali, compatte, industriali, telefoni) con monocromatore e spettrometro. La PCA per canale mostra che il **primo componente spiega oltre il 95%** della varianza e i primi due **oltre il 97%**: lo spazio delle sensibilità è di fatto **bidimensionale** e convesso [doc, 3]. Propongono anche due metodi per stimare la sensibilità di una camera da una sola foto di un color checker. Il database e il codice sono stati pubblicati dal gruppo RIT (sito citato nell'articolo) [doc, 3]. **La licenza d'uso va verificata** prima di includere dati nel prodotto.
- Implicazione [gen]: le camere differiscono per poche "direzioni" spettrali. Una **matrice 3x3** (eventualmente dipendente dall'illuminante) cattura la parte principale della differenza sotto luce a spettro continuo. Il residuo è non lineare e dipende dalla tinta (pelle, verdi, luci LED a banda stretta). Si approssima con correzioni hue-dependent (6–12 settori di tinta) o con una piccola LUT 3D *generata dal nostro modello*.
- Condizione di Luther [gen]: un sensore sarebbe colorimetrico se le sue sensibilità fossero una combinazione lineare delle funzioni del CIE observer. Nessun sensore reale la soddisfa, e l'errore residuo è la "firma" metamerica della camera.
- Approccio legittimo per noi [prop]: costruire 3–5 **set di sensibilità sintetiche** parametriche (gaussiane o skew-gaussiane con picco R/G/B e larghezza come parametri) e calcolare offline matrici e residui su un set di riflettanze pubbliche (ColorChecker, pelle). Tutto deriva dai nostri parametri, senza dati proprietari.

### 1.5 Scomposizione di un "look di camera"

| Strato | Peso percepito [gen] | Modello nel plugin [prop] |
|---|---|---|
| Tone curve (toe, contrasto, spalla) | molto alto | curva parametrica in log: pivot, pendenza, toe, shoulder |
| Clipping delle alte luci (duro o morbido, per canale o luminanza) | alto, è il segno "digitale" | soglia, morbidezza, modalità per canale o con preservazione della tinta |
| Matrice primaria | alto | 3x3 da "rotazione primari" e "saturazione primari" |
| Saturazione in funzione della luminanza | medio-alto | desaturazione nelle alte luci e nelle ombre |
| Correzioni per tinta (pelle, cieli, verdi) | medio | 6 settori: hue shift, saturazione |
| White balance e tinta residua | medio | errore WB intenzionale (K, verde/magenta) |
| Rumore (luma/croma, grana spaziale, pattern fisso) | medio | modello shot + read noise per canale, dimensione grana |
| Sharpening (raggio, quantità, aloni) | medio, decisivo per il "video" | unsharp mask con raggio 0,5–3 px e overshoot |
| Risoluzione, aliasing, moiré | medio | downscale e line-skip senza filtro |
| Codec (blocchi, sottocampionamento croma, banding) | alto per i look consumer | DCT 8x8 con quantizzazione, sottocampionamento croma, quantizzazione 8 bit |
| Artefatti ottici e sensore (smear, blooming, fringing, vignettatura) | caratterizzante | moduli dedicati |

---

## 2. Look digitali storici e consumer

### 2.1 Camcorder 3CCD MiniDV (fine anni '90, primi 2000)

- **Sony DCR-VX1000 (1995)** [doc, 11]: 3 CCD da 1/3", circa 410.000 pixel, risoluzione orizzontale analogica oltre 530 linee (circa il doppio del VHS). La qualità era paragonata al Betacam analogico. È stata usata per i video di skate di fine anni '90 e primi 2000, *The Idiots* (von Trier) e *Bamboozled* (Spike Lee) [doc, 11]. VX2000 (2000) e VX2100 (2003) le succedono. La DSR-PD150 è stata usata per *Inland Empire* (Lynch, 2006) [doc, 11].
- **Panasonic AG-DVX100 (2002)** [doc, 12]: 3CCD, circa 410.000 pixel totali e 380.000 effettivi. Riprende a 24p e 30p progressivo, con 24p in due modalità di pulldown (2:3 e 2:3:3:2 "24pA"), oppure 60i/50i. Ha una **"Cine-Like Gamma"** per una tonalità simile alla pellicola (il nome Panasonic è "Cine-Like", non "CineGamma"). ND da circa 3 e circa 6 stop. Il primo modello era solo 4:3 [doc, 12].
- **Formato DV** [doc, 15][doc, 16][doc, 17]: 720x480 (60 Hz) o 720x576 (50 Hz), 704 pixel attivi. Croma **4:1:1 in NTSC, 4:2:0 in PAL**. Circa **25 Mbit/s** video (circa 36 Mbit/s totali con audio e correzione d'errore), compressione **intra-frame DCT a 8x8**, rapporto **nominale 5:1**, 8 bit. Le tabelle di quantizzazione sono ottimizzate localmente dentro il fotogramma (a differenza del JPEG a tabella fissa). È interlacciato (DVCPRO Progressive a parte) [doc, 15][doc, 16][doc, 17].
- Tratti visivi [gen]:
  - gamma dinamica utile di circa 6–8 stop, con clip duro a circa 100–109 IRE (knee regolabile nei modelli pro) e alte luci che virano al bianco o al ciano;
  - colori saturi e "puliti" grazie al prisma 3CCD (separazione spettrale buona e niente demosaicing), con rossi che sbavano per la 4:1:1 (croma orizzontale a 180 campioni per riga in NTSC);
  - **sharpening elettronico forte** (detail/aperture) con aloni chiari di 1–2 px sui contorni;
  - rumore di luma fine con guadagno a +9/+18 dB;
  - **smear verticale** sulle luci forti (CCD interline);
  - mosquito noise attorno ai bordi e blocchi 8x8 sulle scene complesse e nel movimento;
  - interlacciamento con combing nel movimento se de-interlacciato male.

### 2.2 Smear e blooming CCD

- Lo **smear** è tipico dei CCD interline transfer. Nasce nei registri verticali (VCCD) schermati da fotoni ed elettroni dispersi e produce una **colonna luminosa sopra e sotto** la sorgente forte. La schermatura dipende dalla lunghezza d'onda, quindi lo smear è molto peggiore oltre circa 700 nm [doc, 18]. I CMOS non hanno smear [doc, 18].
- Il **blooming** è la carica che trabocca nei pixel vicini attorno alle sorgenti molto forti. Riguarda entrambe le tecnologie, con un vantaggio per il CMOS [doc, 18].
- Modello [prop]: per ogni colonna, `smear(col) = k · somma(max(0, L − soglia))` lungo la colonna (in linea, prima del clip). Si somma su tutta la colonna con un'attenuazione leggera e una tinta (spesso tendente al rosa o al magenta per il contributo IR [gen]). Il blooming è una diffusione locale anisotropa, prevalente in verticale [gen], applicata solo all'eccesso sopra saturazione.

### 2.3 Compatte digitali CCD 2000–2006

- **Purple fringing** [doc, 19]: alone viola o magenta sui bordi scuri contro aree luminose (rami contro il cielo). Le cause citate sono l'aberrazione cromatica (la principale), il bloom dei CCD sovraesposti, l'elaborazione e l'interpolazione ed eventualmente UV/IR. È favorito da cieli chiari o velati [doc, 19].
- Tratti [gen]:
  - sensori da 1/2,5"–1/1,8", 3–8 MP;
  - gamma dinamica JPEG di circa 7–8,5 stop con alte luci che clippano presto e per canale (cieli ciano, pelle che vira al giallo in sovraesposizione);
  - saturazione "consumer" alta su blu e verdi, contrasto alto;
  - JPEG 4:2:2 o 4:2:0 con blocchi 8x8 e ringing alle qualità "Normal";
  - NR moderata, con il rumore croma a macchie a ISO 200–400 come segno distintivo;
  - auto WB spesso freddo o blu all'aperto e arancio sotto tungsteno;
  - vignettatura moderata e distorsione a barilet in grandangolo;
  - flash diretto: soggetto piatto, sfondo nero, occhi rossi.
- dpreview misurava la gamma dinamica nelle recensioni dell'epoca (tutorial "Measuring Dynamic Range") [doc, 20]. Non abbiamo estratto numeri specifici, quindi i valori di DR qui sopra restano [gen].

### 2.4 Telefoni con fotocamera 2003–2010 e webcam

- Il primo iPhone (2007) aveva una camera da 2 MP, molto inferiore alle compatte di pari prezzo. La qualità diventa una priorità solo dopo il 2010 [doc, 21].
- I pixel dei telefoni misurano circa 1,5 µm o meno, contro circa 4 µm delle reflex. Ne seguono meno carica, gamma dinamica minore (10 bit contro 12–14 bit raw) e più guadagno, quindi più rumore. Il denoise diventa centrale [doc, 21].
- Pipeline ISP classica [doc, 21]: livello del nero e pixel difettosi → correzione dello shading della lente (maschera calibrata) → demosaic → WB → matrice colore (spesso più 3D LUT) → tone map 1D verso 8–10 bit → denoise → sharpening → resize e conversione sRGB → JPEG.
- Lo shading colore, cioè la dominante che cambia dal centro ai bordi, è oggetto di molti brevetti di correzione [doc, 22]. La sua presenza residua è un tratto dei telefoni economici [gen].
- Tratti [gen]:
  - sensori da 0,3 a 3 MP (2003–2007), ottica fissa in plastica con poca nitidezza ai bordi;
  - **NR "ad acquerello"** che cancella la texture fine;
  - vignettatura forte (−1/−2 stop ai bordi) con dominante verde, magenta o ciano agli angoli;
  - DR di circa 5–7 stop;
  - JPEG pesante;
  - video spesso 176x144 o 320x240 a 15 fps, in 3GP/MPEG-4 o H.263 con blocchi enormi;
  - **rolling shutter** marcato (skew nei panning, effetto "jello");
  - AWB instabile;
  - webcam: esposizione automatica che "respira", 15–30 fps irregolari e rumore croma forte in interni.

### 2.5 Digital Betacam (riferimento alto)

Digital Betacam [doc, 23]: componente **10 bit 4:2:2**, circa **90 Mbit/s**, compressione DCT leggera (rapporto circa 2:1 [gen]). Look: pulito, quasi senza artefatti di codec, gamma video broadcast Rec.601 con knee. È utile come riferimento "SD di qualità" contro cui misurare il degrado del DV [prop].

### 2.6 HDV (MPEG-2 long GOP)

- HDV [doc, 24]: 1080i a **1440x1080** con pixel non quadrati (PAR 1,33) e **25 Mbit/s**; 720p a 1280x720 e 19,7 Mbit/s. Usa MPEG-2 con compressione temporale.
- Tratti [gen]:
  - GOP di circa 12–15 fotogrammi (1080i) e 6 (720p);
  - croma 4:2:0;
  - **pulsazione del GOP** (la qualità risale a ogni I-frame);
  - macroblocchi 16x16 che "esplodono" su acqua, coriandoli, fogliame mosso o flash;
  - risoluzione orizzontale ridotta;
  - CCD o CMOS da 1/3" con sharpening forte.

### 2.7 Cinema HD CCD di prima generazione (Sony HDW-F900)

- La HDW-F900 usava **3 CCD da 2/3"** e registrava 1080p/1080i su nastro **HDCAM**. È stata usata per *Star Wars: Episodio II* e per *Arca russa* (in HD non compresso, un piano sequenza di 86 minuti). La F950 è servita per l'*Episodio III* [doc, 25].
- Tratti [gen]:
  - HDCAM a 1440 campioni orizzontali, croma 3:1:1, 8 bit, circa 140 Mbit/s;
  - gamma dinamica di circa 8–10 stop con **clip delle alte luci netto** (knee video) e cieli bruciati;
  - immagine "nitida-video" con poca profondità di campo (sensore 2/3");
  - colori saturi un po' cartoon, con sharpening di dettaglio visibile;
  - rumore basso ma neri un po' "sporchi" di rumore croma.
- Lo sfondo storico di questi tratti è nel racconto di RedShark sull'uso della F900 [doc, 26]. I numeri sopra restano [gen] finché non vengono verificati sulla scheda Sony.

### 2.8 Canon 5D Mark II (2008): reflex video

- Video **1920x1080 a 30 fps fisso** (i 24/25p sono arrivati con un firmware successivo [gen]), **H.264 in MOV, circa 38,6 Mbit/s** [doc, 27].
- **Moiré e aliasing**: sul test di zone plate il moiré è "notevole". L'ipotesi più diffusa è la lettura di una riga ogni tre con ridimensionamento orizzontale a 1920. Ne seguono contorni seghettati e scintillanti sulle diagonali e sulle linee quasi orizzontali (insegne, orizzonti, edifici), mentre volti e verticali reggono [doc, 28].
- Tratti [gen]:
  - 8 bit 4:2:0 con **banding** nei cieli e nelle sfumature, soprattutto dopo una correzione colore;
  - **macroblocchi H.264** nelle ombre e sul rumore;
  - rolling shutter;
  - profondità di campo full frame (il motivo del successo);
  - Picture Style "Neutral/Faithful" a contrasto ridotto. La comunità usava profili flat (per esempio Technicolor CineStyle) per recuperare latitudine al prezzo di più banding [doc, 29].
- È esistito un problema di banding riconosciuto da Canon sulle foto in certe condizioni [doc, 30]. Riguarda le foto ed è distinto dal banding da 8 bit nel video.

### 2.9 GoPro dei primi anni

GoPro HD Hero (2009) [doc, 31]: sensore CMOS (foto da 5 MP), ottica fissa f/2.8, **FOV di 170°** in WVGA, 720p e 960p e **127° in 1080p**, sempre con distorsione fisheye. Modalità da 8 a 15 Mbit/s. **Rolling shutter** ("jello") visibile nei veicoli non sospesi, rumore in poca luce, sovraesposizione in scene luminose e immagine giudicata "nitida" nonostante il bitrate modesto [doc, 31].

Tratti [gen]: sharpening alto con aloni, saturazione spinta, cieli che clippano al bianco o al ciano, vignettatura e aberrazione cromatica ai bordi.

### 2.10 Telefoni dell'era "smart HDR" (2018 in poi)

- Le pipeline multi-frame (per esempio HDR+) uniscono 2–8 fotogrammi raw volutamente sottoesposti. Il rumore cala circa con la radice quadrata del numero di fotogrammi e diventa spazialmente variabile [doc, 21].
- Il **tone mapping locale** separa base e dettaglio per aumentare il contrasto locale. Nelle scene notturne la scelta estetica dichiarata è "ombre vicine al nero e saturazione aumentata" [doc, 21].
- Il caso "Beautygate" dell'iPhone XS (2018): utenti e stampa hanno lamentato pelle levigata nei selfie, attribuita all'HDR multi-frame e alla riduzione rumore più che a un filtro bellezza. Apple ha poi modificato il comportamento via software [doc, 32].
- Tratti [gen]:
  - alte luci "trattenute" e ombre sollevate (look piatto e uniforme);
  - aloni sui contorni cielo/edifici da tone mapping locale;
  - texture della pelle levigata;
  - sharpening a raggio piccolo molto evidente;
  - saturazione alta sui cieli;
  - assenza quasi totale di clip duro.

### 2.11 Super 8 e DV a confronto (punti chiave)

[gen]

| Aspetto | Super 8 | DV |
|---|---|---|
| Alte luci | spalla morbida | clip duro con alone di sharpening |
| Rumore | grana casuale per fotogramma, a struttura organica | rumore fine e regolare più blocchi di codec |
| Moto | 18/24 fps con otturatore a 180°: mosso e flicker | 60i: moto fluido "video", o 24p con pulldown |
| Colore | dominanti di emulsione, nero che sale con la perdita di densità | colore sottocampionato e sbavato orizzontalmente (NTSC 4:1:1) |
| Bordi | morbidi | duri e accentuati |

---

## 3. Sintesi dei tratti con numeri

Valori in gran parte **[gen]**, salvo quelli marcati [doc] nelle sezioni precedenti. Formato e risoluzione per DV, HDV, 5D II e GoPro sono [doc].

| Look | DR utile (stop) | Clipping | Matrice e colore | Rumore | Sharpening (raggio / quantità) | Compressione / croma |
|---|---|---|---|---|---|---|
| 3CCD MiniDV | 6–8 | duro, knee a ~100 IRE, per canale | saturo, rossi forti, verdi puliti | fine, luma, guadagno a gradini | 1–1,5 px / alto, aloni chiari | DCT 8x8 intra, 25 Mbit/s, 4:1:1 (NTSC) o 4:2:0 (PAL) [doc] |
| DVX100 Cine-Like | 7–8 | knee più morbido, toe sollevato | meno saturo, più caldo | come sopra | medio | come DV [doc] |
| Digital Betacam | 8–9 (video) | knee broadcast | neutro Rec.601 | basso | basso-medio | 10 bit 4:2:2, 90 Mbit/s [doc] |
| HD CCD broadcast 2002 | 8–10 | duro, cieli bruciati | saturo, "video" | basso | medio-alto | HDCAM 1440, 3:1:1 (da verificare) |
| HDV | 7–8 | duro | saturo | medio | alto | MPEG-2 long GOP, 1440x1080, 25 Mbit/s [doc], 4:2:0 |
| Compatta CCD 2004 | 7–8,5 | precoce, per canale (ciano/giallo) | blu e verdi spinti | croma a macchie | 0,8–1,2 px / alto | JPEG 8x8, 4:2:2/4:2:0, q 70–90 |
| Telefono 2005–2008 | 5–7 | precoce, bruciato | spento o con dominanti, shading colore | forte, NR ad acquerello | 1–2 px / alto su un'immagine morbida | JPEG q 50–75; video 3GP/H.263 da 176x144 a 320x240 |
| Webcam 2005 | 5–6 | esposizione che pompa | dominanti | croma forte | basso | MJPEG o H.263, 15 fps |
| Reflex video 2009 | 9–11 (profilo neutro) | duro nel JPEG/video | Canon caldo | basso (FF) | basso; aliasing da line-skip [doc] | H.264 ~38,6 Mbit/s [doc], 8 bit 4:2:0 |
| Action cam 2010 | 7–8 | bianco/ciano | saturo | medio in interni | alto | H.264 8–15 Mbit/s [doc]; fisheye 127–170° [doc] |
| Telefono smart HDR | 10–12 (multi-frame) | quasi nessuno (tone map) | cieli saturi, pelle levigata | basso con NR forte | 0,5–1 px / alto | HEVC/H.264, 10 bit HDR |
| Cinema digitale moderno | 14–17 | spalla lunga, desaturazione progressiva [doc ARRI] | naturale, pelle compatta | fine, organico | quasi nullo | raw o ProRes 4444/422 HQ |

---

## 4. Ordine di elaborazione raccomandato

Proposta [prop], in luce lineare di scena dove possibile (coerente con la pipeline ISP [doc, 21]):

1. **Ingresso** da log o display a lineare di scena (curve pubblicate, sezione 1.3) e gamut di lavoro (per esempio AP1 o linear Rec.2020).
2. **Ottica**: vignettatura e shading colore radiale, aberrazione cromatica laterale, distorsione (barilet o fisheye).
3. **Sensore, componente lineare**: esposizione e guadagno (dB), poi **smear e blooming** calcolati sull'eccesso prima del clip.
4. **Spettro → matrice**: matrice 3x3 "camera" (rotazione e saturazione dei primari, separazione), più WB ed errore WB intenzionale.
5. **Rumore del sensore**: shot noise (∝√L) più read noise, rumore croma, pattern fisso o banding di riga. Va prima della tone curve, così cresce nelle ombre in modo realistico.
6. **Clip del sensore**: soglia in stop sopra il grigio, morbidezza, modalità per canale (dominanti, segno consumer) o con preservazione della tinta.
7. **Risoluzione**: downscale con o senza filtro antialias, line-skip (moiré), demosaic semplificato e fringing viola sui bordi ad alto contrasto.
8. **Tone curve di visualizzazione**: toe, contrasto, spalla/knee e desaturazione in funzione della luminanza.
9. **Correzioni per tinta**: pelle, cielo, verde e saturazione globale.
10. **Elaborazione ISP**: denoise ("acquerello") poi sharpening (raggio, quantità, soglia, overshoot), nell'ordine del pipeline mobile [doc, 21].
11. **Codifica display** (Rec.709 o sRGB) e quantizzazione a 8 bit con banding opzionale.
12. **Codec**: sottocampionamento croma (4:1:1, 4:2:0, 3:1:1), DCT a blocchi 8x8 con quantizzazione, deblocking assente o leggero, pulsazione GOP opzionale e macroblocchi in movimento.
13. **Tempo** (opzionale): rolling shutter (skew per riga da vettori di moto, costoso), interlacciamento e cadenza.
14. **Uscita** nel gamut e nella curva di destinazione.

Note di implementazione OFX [prop]:

- I passi 1–9 sono per pixel o separabili. Il 7 e il 10 sono filtri locali. Il 12 è per blocchi e richiede tile allineati a multipli di 8/16 rispetto all'origine del fotogramma, così i blocchi non "nuotano" tra le tile.
- Il rumore e la pulsazione GOP devono essere **deterministici rispetto al tempo** (seed = frame + parametro).
- Il rolling shutter reale richiede il moto. Conviene trattarlo come effetto separato o rinunciarvi nella prima versione.

---

## 5. Proposta per il plugin: archetipi e controlli

### 5.1 Archetipi (nomi senza marchi)

Valori iniziali [prop] scritti a mano, da tarare. Legenda delle colonne:

- DR = stop sopra e sotto il grigio prima del clip;
- Spalla = morbidezza del clip, da 0 duro a 1 molto morbido;
- Sat = saturazione globale relativa;
- Sat AL = desaturazione nelle alte luci, 0–1;
- Sharp = quantità / raggio in px a 1080;
- Rumore = luma / croma, 0–1;
- Codec = forza 0–1 e tipo;
- Smear = 0–1.

| # | Nome UI (it) | DR (sopra/sotto) | Spalla | Sat | Sat AL | Hue / colore | Sharp | Rumore | Codec | Smear | Extra |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cinema digitale moderno | +7 / −7,5 | 0,85 | 1,00 | 0,6 | pelle compatta, verdi leggermente gialli | 0,05 / 0,6 | 0,15 / 0,05 | 0 | 0 | — |
| 2 | Cinema digitale "nitido" | +6 / −6 | 0,55 | 1,10 | 0,3 | verdi e gialli vivaci, contrasto alto | 0,15 / 0,7 | 0,1 / 0,05 | 0 | 0 | — |
| 3 | Mirrorless log anni 2020 | +6 / −6,5 | 0,6 | 1,00 | 0,4 | cieli verso il ciano, pelle leggermente verde | 0,2 / 0,8 | 0,2 / 0,1 | 0,1 (H.264/HEVC 10 bit) | 0 | — |
| 4 | Reflex video 2009 | +3,5 / −6 | 0,25 | 1,05 | 0,2 | caldo, rossi saturi | 0,1 / 1,0 | 0,15 / 0,1 | 0,45 (H.264 8 bit) | 0 | moiré da line-skip 0,7, banding 0,5 |
| 5 | Cinema HD CCD 2002 | +2,5 / −5,5 | 0,1 | 1,15 | 0,1 | primari saturi, cieli che bruciano al ciano | 0,5 / 1,2 | 0,1 / 0,15 | 0,3 (DCT 3:1:1) | 0,3 | — |
| 6 | Broadcast SD di qualità | +2,5 / −5 | 0,3 (knee) | 1,00 | 0,2 | neutro Rec.601 | 0,4 / 1,2 | 0,1 / 0,05 | 0,1 (4:2:2) | 0,25 | interlacciato opzionale |
| 7 | CCD 3 sensori anni 2000 (MiniDV) | +2 / −5 | 0,05 | 1,20 | 0,1 | rossi forti, verdi puliti | 0,7 / 1,3 | 0,25 / 0,15 | 0,5 (DV 4:1:1/4:2:0) | 0,5 | aloni, mosquito |
| 8 | MiniDV "cine" 24p | +2,5 / −5 | 0,35 | 0,90 | 0,3 | caldo, toe sollevato | 0,5 / 1,3 | 0,25 / 0,15 | 0,5 (DV) | 0,5 | 24p |
| 9 | HD consumer a nastro (MPEG-2) | +2,5 / −5 | 0,1 | 1,15 | 0,1 | saturo | 0,8 / 1,0 | 0,3 / 0,2 | 0,55 (long GOP) | 0,2 | pulsazione GOP |
| 10 | Compatta digitale 2004 | +2,5 / −5 | 0,05 per canale | 1,25 | 0,0 | blu e verdi spinti, clip ciano/giallo | 0,7 / 1,0 | 0,3 / 0,45 | 0,35 (JPEG q≈80) | 0,35 | fringing viola 0,5, WB freddo |
| 11 | Telefono 2008 | +1,5 / −4,5 | 0,0 | 0,85 | 0,0 | dominanti, shading colore agli angoli | 0,8 / 1,5 | 0,5 / 0,6 + NR 0,7 | 0,6 (JPEG q≈60) | 0 | vignettatura −1,5 stop, risoluzione 0,4 |
| 12 | Webcam anni 2000 | +1,5 / −4 | 0,0 | 0,80 | 0,0 | verde-giallo in interni | 0,3 / 1,0 | 0,6 / 0,7 | 0,7 (MJPEG/H.263) | 0 | 15 fps, esposizione che pompa |
| 13 | Action cam 2010 | +2,5 / −5 | 0,05 | 1,30 | 0,0 | saturo, cieli ciano | 0,9 / 0,8 | 0,35 / 0,3 | 0,5 (H.264 15 Mbit/s) | 0 | fisheye 0,8, CA ai bordi |
| 14 | Telefono smart HDR | +5 / −6 (tone map locale) | 1,0 | 1,15 | 0,2 | cieli saturi, pelle levigata | 0,8 / 0,6 | 0,1 / 0,05 + NR 0,6 | 0,15 (HEVC) | 0 | contrasto locale 0,6, aloni 0,3 |
| 15 | Videocamera di sorveglianza CCD (opzionale) | +2 / −4 | 0,0 | 0,6 | 0,0 | dominante verde, IR | 0,6 / 1,5 | 0,5 / 0,3 | 0,7 | 0,8 | interlacciato |

### 5.2 Controlli semplici (pannello principale)

Proposta di 8 controlli macro più 1 menu. Ogni macro pilota diversi parametri interni dell'archetipo e scala attorno ai valori del preset (0 = valore del preset).

| Controllo UI | Tipo / range | Default | Cosa fa internamente |
|---|---|---|---|
| **Camera** | menu (archetipi 1–15) | 1 | carica il set di parametri |
| **Quantità** | 0–100% | 100 | interpolazione tra neutro (passthrough) e archetipo, su tutti i moduli |
| **Gamma dinamica** | −3…+3 stop | 0 | sposta il punto di clip e ricentra il toe |
| **Alte luci** | 0–100 (duro → morbido) | preset | morbidezza della spalla, desaturazione nelle alte luci, clip per canale o con preservazione della tinta |
| **Colore** | −100…+100 | 0 | saturazione e intensità della matrice e delle correzioni per tinta (0 = matrice del preset) |
| **Temperatura errore** | −100…+100 | 0 | errore di WB (freddo/caldo) e tinta verde/magenta |
| **Nitidezza elettronica** | 0–200% | 100 | quantità di sharpening con raggio dal preset; oltre 100 compaiono gli aloni |
| **Rumore** | 0–200% | 100 | rumore luma e croma insieme, più NR "acquerello" per i preset telefono |
| **Compressione** | 0–200% | 100 | quantizzazione DCT, sottocampionamento croma e banding |

Pannello avanzato [prop], con gruppi chiusi per default:

- **Ottica**: vignettatura (stop), shading colore, fisheye, aberrazione cromatica, fringing viola.
- **Sensore**: smear (quantità, soglia, tinta), blooming, risoluzione e line-skip, guadagno (dB).
- **Colore fine**: rotazione dei primari R/G/B (±15°), saturazione per primario, tinta e saturazione della pelle, del cielo e del verde.
- **Codec**: tipo (JPEG, DV, MPEG-2 GOP, H.264, nessuno), croma (4:4:4, 4:2:2, 4:2:0, 4:1:1, 3:1:1), dimensione dei blocchi (8/16), periodo e intensità della pulsazione GOP, seed.
- **Tempo**: interlacciamento e combing, cadenza (24/25/30/15).

Regole del repository da rispettare quando si implementa [prop]:

- i nomi dei controlli OFX sono permanenti, quindi vanno scelti con cura prima della prima release;
- ogni cambio del pannello aggiorna di proposito `tests/golden/params_*.txt`;
- prima di creare widget nuovi si usano gli helper `define*` esistenti in `ParamDefs.h`.

### 5.3 Verifica proposta

[prop]

- Test numerici per le curve log contro i valori di riferimento pubblicati (per esempio grigio 18% in S-Log3 → 420/1023; LogC4 e V-Log con i valori di test dei white paper).
- Test del codec: blocchi allineati alla griglia 8x8 del fotogramma con tile diverse (output identico con tile 1 e N).
- Rumore deterministico rispetto a frame e seed.
- Taratura visiva degli archetipi su materiale reale di archivio, possedendo i diritti sul materiale. Le misure (DR effettiva, curva, matrice stimata con un color checker) vanno registrate e non presentate come dati del produttore.

---

## 6. Incertezze e lavoro aperto

- **D-Log M (DJI)**: non è stata trovata una formula pubblica. Va trattata come "non supportata" o stimata solo da materiale nostro, dichiarandolo.
- **K1S1 / REVEAL**: si conosce solo il comportamento qualitativo. Nessun numero pubblico sulla spalla, che va modellata con una nostra curva.
- **HDCAM** (3:1:1, circa 140 Mbit/s, 8 bit) e **Digital Betacam** (circa 2:1): vanno verificati su schede Sony o SMPTE.
- **Database di Jiang et al.**: va verificata la licenza prima di includere dati. In alternativa si usano sensibilità sintetiche parametriche.
- I valori di DR, sharpening e rumore degli archetipi sono **stime [gen]** da validare.
- Durante la ricerca il budget di accesso web si è esaurito. Non sono state lette direttamente le schede Sony F900, il white paper DJI 2017 e quello Apple Log, citati qui tramite colour-science.

---

## Fonti

1. ARRI, *REVEAL Color Science*: https://www.arri.com/en/learn-help/learn-help-camera-system/image-science/reveal-color-science
2. ARRI, *LogC4 Specification* (PDF): https://www.afcinema.com/IMG/pdf/9/2/e/arri_logc4_specification.pdf
3. Jiang, Liu, Gu, Süsstrunk, *What is the Space of Spectral Sensitivity Functions for Digital Color Cameras?*, WACV 2013: https://infoscience.epfl.ch/record/182538 (PDF: https://infosciences.epfl.ch/record/182538/files/egpaper_final.pdf)
4. ARRI, *ALEXA Color Processing White Paper*: https://www.samys.com/images/pdf/ALEXA-Color-Processing-White-Paper.pdf ; costanti LogC3 anche in colour-science: https://colour.readthedocs.io/en/latest/_modules/colour/models/rgb/transfer_functions/arri.html
5. Sony, *S-Gamut3/S-Log3 technical summary* (copia PDF): https://wikis.utexas.edu/download/attachments/148027475/Sony_Gamut_S-Log3.pdf?api=v2
6. Panasonic, *VARICAM V-Log/V-Gamut Reference*: https://resource.holdan.co.uk/Panasonic/specs/Panasonic_VARICAM_V-Log_V-Gamut.pdf
7. Blackmagic Design, *DaVinci Resolve 17 Wide Gamut Intermediate*: https://documents.blackmagicdesign.com/at/InformationNotes/DaVinci_Resolve_17_Wide_Gamut_Intermediate.pdf
8. Canon, *White paper: Canon Log gamma curves*: https://www.usa.canon.com/content/dam/canon-assets/white-papers/pro/white-paper-canon-log-gamma-curves.pdf
9. FilmConvert, *How does CineMatch compare to Color Space Transform in Resolve?*: https://www.filmconvert.com/blog/?p=1912 ; prodotto: https://www.filmconvert.com/cinematch
10. Mixing Light, *Computer-Assisted Shot Matching with Colourlab AI – Part 1*: https://mixinglight.com/color-grading-tutorials/computer-assisted-shot-matching-with-colourlab-ai-part-1
11. Wikipedia, *Sony DCR-VX1000*: https://en.wikipedia.org/wiki/DCR-VX1000
12. Wikipedia, *Panasonic AG-DVX100*: https://en.wikipedia.org/wiki/DVX100 ; scheda Panasonic: https://www.fullcompass.com/common/files/11042-AGDVX100BPSDataSpecificationsSheet.pdf
13. RED, *IPP2 Overview*: https://support.red.com/hc/en-us/articles/115004913827-IPP2-Overview
14. colour-science (BSD-3), pacchetto 0.4.7: dataset di gamut (arri, red, sony, canon_cinema_gamut, blackmagic_design, dji_d_gamut) e funzioni di trasferimento (red, canon, blackmagic_design, apple_log_profile, dji_d_log): https://colour.readthedocs.io/ ; per esempio https://colour.readthedocs.io/en/latest/_modules/colour/models/rgb/transfer_functions/red.html , https://colour.readthedocs.io/en/latest/_modules/colour/models/rgb/transfer_functions/blackmagic_design.html , https://colour.readthedocs.io/en/v0.4.5/_modules/colour/models/rgb/transfer_functions/apple_log_profile.html
15. Library of Congress, *DV formats*: https://loc.gov/preservation/digital/formats/fdd/fdd000183.shtml
16. Wikipedia, *DV (video format)*: https://en.wikipedia.org/wiki/DV25
17. Eskil (KTH), *DV format overview*: https://people.kth.se/~eskil/DV/dvformt.html ; standard: SMPTE ST 314 https://pub.smpte.org/doc/st314/20050905-pub/st0314-2005_stable2013.pdf
18. Adimec, *CCD versus CMOS: blooming and smear performance*: https://www.adimec.com/ccd-versus-cmos-blooming-and-smear-performance/ ; FLIR/Teledyne, *Vertical bleeding or smearing*: https://flir.com/support-center/iis/machine-vision/knowledge-base/vertical-bleeding-or-smearing-from-a-saturated-portion-of-an-image
19. Wikipedia, *Purple fringing*: https://en.wikipedia.org/wiki/Purple_fringing
20. dpreview, *Measuring Dynamic Range*: https://www.dpreview.com/articles/4662401783/dynamicrange
21. Delbracio, Kelly, Brown, Milanfar, *Mobile Computational Photography: A Tour* (2021): https://arxiv.org/pdf/2102.09000
22. Esempio di brevetto sulla correzione dello shading colore: https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/9270959 ; flat-field per il modulo camera Raspberry Pi: https://arxiv.org/pdf/1911.13295
23. MediaCollege, *Digital Betacam*: https://mediacollege.com/video/format/beta/digital-betacam.html
24. MediaCollege, *HDV*: https://mediacollege.com/video/format/dv/hdv.html
25. Wikipedia/Wikiwand, *Sony HDW-F900 / CineAlta*: https://wikiwand.com/en/articles/Sony_HDW-F900 ; https://en.wikipedia.org/wiki/CineAlta
26. RedShark News, *How George Lucas pioneered the use of digital video... HDW-F900*: https://www.redsharknews.com/technology-computing/item/2990-how-george-lucas-pioneered-the-use-of-digital-video-in-feature-films-with-the-sony-hdw-f900
27. dpreview, *Canon EOS 5D Mark II In-depth Review*: https://www.dpreview.com/previews/canoneos5dmarkii
28. TechCrunch, *For image quality buffs, DSLR video is off the table* (2008): https://techcrunch.com/2008/12/02/for-image-quality-buffs-dslr-video-is-off-the-table/
29. Vincent Laforet, *Setting up your Canon 5D MKII*: https://blog.vincentlaforet.com/setting-up-your-canon-5d-mkii-and-mkiii ; DVinfo, *Technicolor CineStyle*: https://www.dvinfo.net/forum/canon-eos-full-frame-hd/495723-music-video-shot-using-technicolor-cinestyle.html
30. Reviewed, *Canon acknowledges 5D Mark II banding issue*: https://reviewed.com/cameras/news/canon-acknowledges-canon-eos-5d-mark-ii-banding-issue-19629
31. Sound On Sound, *GoPro HD Hero review*: https://www.soundonsound.com/reviews/gopro-hd-hero
32. MacRumors, *iPhone XS users complain about skin-smoothing selfie camera*: https://www.macrumors.com/2018/09/28/iphone-xs-skin-smoothing-selfie-camera/ ; PetaPixel, *Debunking the "Beauty Filter"*: https://petapixel.com/2018/10/02/iphone-xs-a-look-at-the-new-camera-and-debunking-the-beauty-filter

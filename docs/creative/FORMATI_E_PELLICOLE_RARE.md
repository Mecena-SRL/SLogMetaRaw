# Formati speciali e pellicole rare — ricerca per l'emulazione parametrica

Documento di ricerca (nessuna modifica al codice). Obiettivo: fatti fisici e tecnici per costruire **modelli parametrici
originali** (nessun LUT o profilo copiato) di pellicole non-Kodak/non occidentali, formati grandi e piccoli, e
materiali speciali in bianco e nero, infrarosso e istantanea.

Integra (non ripete) `PELLICOLE_E_SVILUPPO.md` (Kodak/Fuji moderne, ECN-2/C-41/E-6) e `PROCESSI_STORICI.md`
(Technicolor, Autochrome, dagherrotipo).

## Legenda di affidabilità

- **[doc]** fatto letto in una fonte durante questa ricerca (fonte indicata tra parentesi, URL nella sezione Fonti).
- **[gen]** conoscenza generale o ricostruzione non verificata in questa sessione: da controllare prima di usarla come
  riferimento. Tutti i **valori di partenza dei modelli** sono [gen] per definizione: sono scelte di progetto, non misure.
- **[deriv]** calcolo fatto qui a partire da dati [doc] (es. rapporti di ingrandimento).

Limite della ricerca: il budget di consultazione web si è esaurito prima di coprire Lucky, Foton, Forte, Konica e le
schede GOST sovietiche (DS-5, LN-7, LN-8). Per queste voci il testo è quasi tutto [gen]: vanno verificate.

## 0. Convenzioni del motore usate nelle proposte

Per non ripetere ogni volta lo schema, ogni proposta usa questi blocchi (nomi indicativi, non nomi di controlli OFX):

| Blocco | Significato | Unità |
|---|---|---|
| `S` (sensibilità dei record) | matrice 3×3 (o 3×4 con un canale pseudo-IR) che pesa R,G,B lineari in ingresso per ottenere l'esposizione di ciascun record (strato) | adimensionale, righe normalizzate a 1 |
| `HD[r]` | curva caratteristica per record: `gamma` (pendenza nella parte lineare), `toe` e `shoulder` (ampiezza in stop della zona di raccordo), `dmin`, `dmax` | stop, densità |
| `D` (assorbimento dei coloranti) | matrice 3×3: quanta densità rossa/verde/blu produce ciascun colorante (le componenti fuori diagonale sono gli "assorbimenti indesiderati") | densità |
| `mask` | quota di correzione delle assorbimenti indesiderati (0 = negativo non mascherato, 1 = maschera completa tipo Eastman) | 0..1 |
| `grain` | ampiezza RMS e dimensione del grano **relative al 35 mm Super 35 4-perf** (`k` = fattore di scala lineare, vedi §2.0) | rel. |
| `halation` | intensità (0..1), raggio in mm **sul film** (poi convertito in pixel col formato), tinta (tipicamente rossa: lo strato sensibile al rosso è il più profondo) | mm, rel. |
| `squeeze` | fattore anamorfico: rende ellittici grano, alone, bokeh e flare | rapporto |
| `frame` | bordi, giunzioni, gate weave, flicker | px, % |

Principio generale: **grano, alone e nitidezza si fissano in unità fisiche sul fotogramma** (µm/mm) e si convertono in
pixel tramite le dimensioni del formato. Così cambiare formato cambia l'aspetto in modo coerente, senza preset separati.

---

## 1. Pellicole non-Kodak e non occidentali

### 1.1 Famiglia Agfacolor e i suoi eredi: la chiave per capire l'Est

Quasi tutto il colore dell'Europa orientale discende da **Agfacolor Neu (1936)**, sviluppato a **Wolfen**: il primo film
a colori moderno con copulanti incorporati negli strati, più semplice da trattare del Kodachrome [doc, Wikipedia ORWO].
I copulanti Agfa sono **idrosolubili con coda grassa** (ancorati nello strato); i negativi Agfa Wolfen 1939–1964
**non erano mascherati** [doc, filmportal "Kine-Farbnegativfilme"]. La maschera colorata (che dà al negativo Kodak il
tipico arancio) è una correzione introdotta da Eastman Color [doc, idem].

Conseguenza fisica per il modello: senza maschera, gli assorbimenti indesiderati dei coloranti (soprattutto magenta che
assorbe anche nel blu, ciano che assorbe nel verde) passano nella stampa e **sporcano e desaturano** i colori, specie
blu, verdi e incarnati. Questo, più che la "tinta", è il marchio Agfa/ORWO/Sovcolor [gen, coerente con [doc]].

Cronologia dei negativi cine Agfa Wolfen [doc, filmportal, tabella 3]:

| Tipo | Luce | Sensibilità | Anni |
|---|---|---|---|
| Agfacolor U | universale | 10/10 °DIN (~ISO 8) | 1939–40 |
| Typ B / Typ G | luce diurna / artificiale | 10/10 °DIN | 1940–41 |
| Typ B2 / G2 | idem | 15/10 °DIN (~ISO 25), guadagno con "effetto oro" | 1941–45 |
| Typ B / G | idem | 13/10 °DIN (sensibilità ridotta nel dopoguerra) | 1945–47 |
| Typ B / G migliorati | idem | 13/10 °DIN | 1947–54 |
| Typ 333 / 334 | diurna / artificiale | 17/10 °DIN (~ISO 40) | 1954–62 |
| Typ 432 | artificiale | 17/10 °DIN | 1962–64, rinominato **Orwocolor NC 1** nel 1964 |

Tutte pellicole **lente** (ISO 8–40): richiedevano molta luce, quindi diaframmi aperti e poca profondità di campo in
interni, oppure set illuminatissimi e piatti [gen, deduzione dalla sensibilità].

### 1.2 ORWO / Agfa Wolfen (DDR) e il colore DEFA

- Dal 1964 il marchio Agfa a Wolfen diventa **ORWO** (Original Wolfen); i negativi cine diventano la serie **Orwocolor
  NC** (NC 1 dal Typ 432) [doc, filmportal]. Le versioni successive (NC 2, NC 3, sensibilità intorno a 17–20 °DIN) [gen].
- Invertibile per amatori e documentario: **Orwocolor UT** (UT 18, UT 21) [gen]; diapositive **Orwochrom** negli anni '80
  [doc, Wikipedia ORWO].
- Stampa: positivo colore Orwocolor (sigla PC) [gen]. **Attenzione: PF2 è un positivo bianco e nero**, e **UN 54** è un
  negativo pancromatico B/N ISO 100/21° [doc, Wikipedia ORWO]: non sono materiali a colori.
- ORWO iniziò a passare dal processo Agfacolor al **C-41** ma non completò la conversione prima della fine della DDR
  [doc, Wikipedia ORWO]. Per il cinema, il trattamento restò del tipo Agfa (sviluppatore cromogeno diverso da CD-3/CD-4,
  copulanti nello strato) [gen].
- Oggi ORWO produce **NC 500** (ECN-2, 2022) e NC 400 [doc, Wikipedia ORWO]: materiali moderni, non storici.

Aspetto tipico dei film DEFA a colori anni '60–'80 [gen]: palette pastello e "lattiginosa", neri poco profondi (dmax di
stampa basso), rossi tendenti al carminio-arancio, verdi spenti verso l'oliva, cieli ciano chiari, incarnati rosati-
magenta; contrasto di stampa moderato; variabilità tra lotti visibile tra scene (dominanti che cambiano a ogni
inquadratura).

### 1.3 URSS: Shostka (Svema), Kazan (Tasma), Sovcolor

- **Shostka** (Ucraina): impianto fondato nel 1928 come joint venture con Lumière (la stessa fonte riporta inizio
  produzione 1931) [doc, Wikipedia Svema]. Dal **1946** produzione di pellicola grezza con **attrezzature Agfa** prese
  come riparazione di guerra (G. Koshofer, citato in [doc, Cinema Riot]).
- **Sovcolor**: nasce dalle grandi scorte di Agfacolor sequestrate a fine guerra; le prime copie stampate a Berlino Est
  erano di fatto Agfacolor con altro nome [doc, Wikipedia Agfacolor; Cinema Riot]. Usato in tutto il blocco orientale;
  in Polonia lo stesso materiale si chiamava **Polcolor** (dal 1954) [doc, Wikipedia Agfacolor].
- Qualità: colori variabili tra lotti, con **errori di continuità** quando la tinta cambia tra scene; le produzioni di
  prestigio usavano Eastman Kodak importata [doc, Wikipedia Agfacolor].
- **Svema** (marchio di Shostka) arriva dopo, ispirandosi a Eastmancolor con modifiche chimiche minime; tinte più
  distinte e contrasto più sano rispetto al primo Sovcolor, ma **palette pastello, colori desaturati, contrasto
  "lattiginoso"**, difetti di fabbricazione, fluttuazioni di colore e **lotti con uno strato di colore mancante**; molti
  registi negli anni '60 tornarono al B/N per evitarli [doc, Cinema Riot].
- Fine produzione colore a Shostka nel **1995**; B/N cine fino agli anni 2010; fallimento 2015 [doc, Wikipedia Svema].
- Sigle note [doc, Wikipedia Svema]: **DS-4** negativo colore ISO 50, **DS-5M** negativo colore 35 mm cine, **LN-9**
  negativo colore 35 mm cine, **TsNL 65** negativo ISO 80, CO-32D e CO-50D invertibili (8 mm), CND 64.
- Nomenclatura [gen]: Д = дневной (luce diurna), Л = лампы (luce artificiale), Н = негатив; ЦН = negativo colore,
  ЦО = invertibile colore, ЦП = positivo colore (es. ЦП-8 per la stampa). **DS-5, LN-7, LN-8, DS-8**: famiglia di
  negativi diurni/artificiali dagli anni '60 agli '80; sensibilità tipiche 32–90 unità GOST (≈ ISO 40–100), LN-8 tra i
  più rapidi [gen, da verificare su schede GOST].
- Scale di sensibilità GOST [doc, Wikipedia Svema]: prima del 1987 GOST 32 ≈ ISO 40, 65 ≈ ISO 80, 130 ≈ ISO 160,
  250 ≈ ISO 320; dal 1987 la scala GOST coincide con ASA.
- **Kazan (Tasma)**: specializzata in B/N, aerofotografia, pellicole tecniche e **infrarosse** (serie "И-" con
  sensibilizzazione fino a 760–1000 nm circa) [gen, non verificato].

**Kodak nel cinema sovietico** [doc, Cinema Riot]: Goskino comprava piccoli lotti Kodak per progetti favoriti (scene di
*Guerra e pace*, girato anche in 65 mm). *Va' e vedi* mescola Svema e Kodak: "alberi e cieli pastello di Svema contro il
verde smeraldo profondo di Kodak". *Solaris* usa Eastman 5254 per alcune sequenze (incarnati più caldi, contrasto e
colori più profondi); finita la Kodak, Tarkovskij preferì virare/colorare il B/N anziché mescolare Sovcolor.

**Stalker (1979)** [doc, Cinema Riot]: la prima versione, girata su **Eastman 100T 5247**, tornò dal laboratorio
sviluppata male con una profonda dominante verde e fu rigirata da capo. Nel film finito il mondo esterno è
monocromo con viraggio seppia/bruno, la Zona è a colori [gen, osservazione nota]. Per il motore è un caso di **due
regimi nella stessa opera**: un B/N con viraggio (curva B/N + tinta di stampa bruno-verde, non un colore desaturato) e
un colore a bassa saturazione, verdi muschiosi e dominante calda.

### 1.4 Agfa-Gevaert: Gevacolor e XT

- **Gevacolor**: Gevaert (Belgio) dal **1947**, affiliato ad Agfacolor; due tipi 35 mm (diurna e artificiale); diffuso
  negli anni '50 anche perché adatto alle riprese in esterni; nel **1964** fusione in Agfa-Gevaert; produzione fino agli
  anni '80 [doc, Wikipedia Gevacolor]. Usi: primo film *La maison du printemps* (1949), *Jedda* (Australia), primo
  lungometraggio a colori ungherese *Lúdas Matyi* (1950), *Shahenshah* (India, 1953), *L'albero degli zoccoli* (1978)
  indicato come ultimo [doc, idem]. Esiste una positiva Gevacolor tipo 953 (articolo SMPTE) [doc, titolo SMPTE].
- Aspetto [gen]: simile ad Agfacolor (non mascherato nelle prime versioni), toni caldi e terrosi, verdi meno saturi di
  Eastman, contrasto morbido.
- **Agfa XT 100 / XT 125 / XT 320** (anni '80): negativi cine compatibili ECN-2, già mascherati; più "moderni", grana
  morbida, colori meno saturi e più neutri di Kodak coeva [gen].

### 1.5 Ferrania (Italia)

- Ferrania produsse una diapositiva a colori già nel 1942–44 e di nuovo nel 1951 [doc, Wikipedia Gevacolor, citando
  Roosens]. **Ferraniacolor** (anni '50) fu usato per lungometraggi italiani (es. *Totò a colori*, 1952) e da cineamatori
  [gen; ruolo nel passo ridotto: doc, Frames Cinema Journal "Small-Gauge Colour Visions"].
- Aspetto Ferraniacolor [gen]: materiale invertibile/positivo di derivazione Agfa, colori "dipinti" e caldi, rossi
  vivaci, verdi gialli, contrasto alto in stampa per invertibile, forte variabilità.
- B/N: **Ferrania P30** (80 ASA) è associata al cinema italiano degli anni '50–'60: grana fine, neri pieni, contrasto
  brillante [gen]. **Solaris** è invece una famiglia di negativi colore fotografici C-41 (anni '90–2000) prodotti da
  Ferrania/3M-Imation: colori saturi e caldi, grana più evidente dei concorrenti [gen].

### 1.6 Giappone: Fuji e Konica/Sakura

- Primo lungometraggio a colori giapponese: *Carmen torna a casa* (1951) su **Fujicolor invertibile** [gen]. Poi
  negativi Fujicolor (serie 85xx) documentati negli SMPTE Journal 1979–1989: "Fujicolor negative with improved
  characteristic and hot-process positive" (1979), negative higher speed (1981), F-series (1989) [doc, titoli SMPTE].
  La sigla **8517** non è stata verificata in questa sessione [gen].
- Firma Fuji storica [gen]: verdi più freddi e bluastri, magenta e rossi più puliti, incarnati meno caldi di Kodak; nella
  stampa Fuji neri leggermente più blu-ciano.
- **Konica / Sakura**: Sakuracolor (1940) primo film fotografico a colori giapponese [gen]; produzione cine molto
  marginale rispetto a Fuji. Firma fotografica Konica: toni caldi e pastello, verdi giallastri [gen].

### 1.7 Cina, India, Polonia, Cecoslovacchia, Ungheria

| Produttore | Fatti | Aspetto [gen] |
|---|---|---|
| **China Lucky Film** (Baoding) | fondata 1958; accordo ventennale con Kodak nel 2003 (20% del capitale, 45 M$ + 54,5 M$ per le linee), Kodak esce nel 2007 [doc, Wikipedia]. Pellicole colore, B/N, raggi X. Nuovo colore C200 annunciato 2024 [doc, Kosmofoto/Digital Camera World, titoli] | cine cinese anni '60–'80 spesso su stampe locali derivate da tecnologia ORWO/Sovcolor: colori pastello e caldi, variabilità; Lucky SHD B/N a grana piuttosto evidente |
| **Hindustan Photo Films** (Ooty, India) | positiva B/N e colore per il cinema indiano (marchio Indu) [gen] | stampe a basso dmax, sbiadimento rapido |
| **Foton** (Varsavia) | B/N fotografico; il colore polacco cine era Polcolor (= Sovcolor/ORWO) [doc per Polcolor] | B/N morbido, grana media |
| **Foma** (Hradec Králové) | Fomapan B/N; esiste ancora un invertibile cine **Fomapan R 100** [gen] | grana visibile, toe morbido, alte luci che si comprimono presto |
| **Forte** (Vác, Ungheria) | B/N fotografico, chiusa nel 2007 [gen] | toni caldi su carta, grana media |

### 1.8 Proposta di modello: famiglia "Agfa-Est" (Sovcolor / ORWO / Svema / Gevacolor)

Un solo modello parametrico con quattro preset, non quattro motori.

| Parametro | Sovcolor anni '50 | ORWO NC anni '70 | Svema anni '70–'80 | Gevacolor anni '50 |
|---|---|---|---|---|
| `S` sovrapposizione spettrale (fuori diagonale) | 0,18 | 0,14 | 0,12 | 0,15 |
| `HD gamma` neg (B,G,R) | 0,55 / 0,60 / 0,58 | 0,58 / 0,62 / 0,60 | 0,60 / 0,62 / 0,58 | 0,55 / 0,60 / 0,60 |
| toe / shoulder (stop) | 2,0 / 1,5 | 1,8 / 2,0 | 1,6 / 2,0 | 2,0 / 1,6 |
| `mask` | 0 | 0 | 0,3 | 0 |
| `D` assorbimento indesiderato magenta→blu | 0,35 | 0,30 | 0,25 | 0,32 |
| `D` ciano→verde | 0,25 | 0,22 | 0,20 | 0,22 |
| dmax stampa (densità) | 2,4 | 2,6 | 2,7 | 2,5 |
| grano (rel. 35 mm moderno) | 1,8 | 1,5 | 1,4 | 1,6 |
| alone (intensità, raggio mm) | 0,25 / 0,20 | 0,20 / 0,18 | 0,20 / 0,18 | 0,22 / 0,20 |
| variabilità tra scene (σ di bilanciamento, densità) | 0,06 | 0,04 | 0,05 | 0,04 |

Tutti [gen]: punti di partenza da tarare su materiale reale osservato (non su LUT altrui).

Elementi specifici:
- **Variabilità di lotto**: un offset di bilanciamento colore per "scena" (seme legato a un ID di inquadratura o a un
  controllo manuale), più una deriva lenta nel tempo (σ ≈ 0,01 densità/s). Il difetto "strato mancante" va come opzione
  esplicita (un record con gamma ×0,3), non casuale.
- **Mottling / disuniformità di emulsione**: rumore a bassa frequenza (scala 5–15% della larghezza fotogramma) sulla
  densità di un singolo record, ampiezza 0,01–0,03 [gen].

Controlli intuitivi (2–4):
1. **Epoca** (1950 → 1985): interpola i preset (sovrapposizione, mask, dmax, grano).
2. **Pastello** (0..1, default 0,5): agisce su assorbimenti indesiderati + dmax di stampa insieme.
3. **Instabilità di lotto** (0..1, default 0,3): scala σ di variabilità e mottling.
4. **Origine stampa** (Agfa / Kodak): riusa la stampa moderna esistente per simulare il "negativo Est stampato su Kodak".

---

## 2. Formati grandi e speciali

### 2.0 Scala del grano, dell'alone e della nitidezza in funzione del formato

A parità di immagine finale (stessa larghezza in pixel), una caratteristica di dimensione fisica fissa sul film
(grano, raggio di alone, MTF) appare più grande quanto più piccolo è il fotogramma. Definiamo:

`k = L_ref / L_fmt`, con `L_ref` = 24,89 mm (larghezza Super 35 4-perf) e `L_fmt` = larghezza utile del formato.

Per i formati **anamorfici** con fattore `q`: la scala orizzontale è `k`, quella verticale è `k / q`. Il grano diventa
**ellittico**, allungato orizzontalmente di un fattore `q` dopo la de-compressione; lo stesso vale per alone e bokeh
(che però sul set nasce già ovale, vedi §2.3).

| Formato | Fotogramma utile (mm) | Perf | AR | Squeeze | fps | k orizz. | k vert. |
|---|---|---|---|---|---|---|---|
| Super 35 4-perf (rif.) | 24,89 × 18,66 [gen] | 4 | 1,33 | 1 | 24 | 1,00 | 1,00 |
| 35 mm anamorfico | ≈22 × 18,6 [doc, Wikipedia CinemaScope] | 4 | 2,39 | 2 | 24 | 1,13 | 0,57 |
| CinemaScope 55 | ≈46 × 36 [doc, idem] | 8 | 2,55 | 2 | 24 | 0,54 | 0,27 |
| Techniscope | ≈22,0 × 9,5 [gen] | 2 | 2,33 | 1 (2× in stampa) | 24 | 1,13 | 1,13 |
| VistaVision | ≈37,7 × 24,9 [gen] | 8 orizz. | 1,5 | 1 | 24 | 0,66 | 0,66 |
| Technirama | 38,0 × 25,2 (1,496 × 0,992") [doc, Widescreen Museum] | 8 orizz. | 2,25 | 1,5 | 24 | 0,65 | 0,44 |
| 65 mm 5-perf (Todd-AO, Super Panavision 70) | ≈52,5 × 23,0 (2,066 × 0,906") [gen] | 5 | 2,20 | 1 | 24 (Todd-AO 30) | 0,47 | 0,47 |
| Ultra Panavision 70 / MGM Camera 65 | 52,6 × 23,0 (2,072 × 0,906") [doc, Widescreen Museum] | 5 | 2,76 | 1,25 | 24 | 0,47 | 0,38 |
| IMAX 15/70 | 70,41 × 52,63 (o 69,6 × 48,6) [doc, cinematography.net] | 15 orizz. | 1,43 | 1 | 24 | 0,35 | 0,35 |
| Cinerama (ogni pannello) | 25,3 × 28,3 (0,996 × 1,116") [doc, Widescreen Museum] | 6 | 2,59 totale | 1 | 26 | 0,98 per pannello, ≈0,33 sull'insieme | idem |
| 28 mm Pathé KOK | ≈19 × 14 [gen] | — | 1,33 | 1 | 16–18 | ≈1,3 | ≈1,3 |
| 17,5 mm (Pathé Rural, ecc.) | ≈13–14 × 10 [gen, molto incerto] | — | 1,33 | 1 | 16–24 | ≈1,9 | ≈1,9 |
| 16 mm | ≈10,26 × 7,49 [gen] | 1 | 1,37 | 1 | 24 (16–18 amatoriale) | 2,43 | 2,43 |
| Super 16 | ≈12,52 × 7,41 [gen] | 1 | 1,66 | 1 | 24/25 | 1,99 | 1,99 |
| 9,5 mm Pathé-Baby | ≈8,5 × 6,5, perforazione centrale [gen] | 1 centrale | 1,31 | 1 | 14–16 | 2,93 | 2,93 |
| Super 8 | ≈5,79 × 4,01 [gen]; Kvarts sovietica 5,69 × 4,22 [doc, filmkorn] | 1 | 1,36 | 1 | 18/24 | 4,30 | 4,30 |
| Regular / Standard 8 | ≈4,8 × 3,5 [gen] | 1 (bordo) | 1,33 | 1 | 16/18 | 5,19 | 5,19 |

Note:
- I valori `k` sono [deriv] dai dati di riga. IMAX ha area circa **2,7 volte** il 35 mm a fotogramma intero
  [doc, cinematography.net] (qui confrontato col Super 35 si ottiene ≈7,9 volte in area: le due cifre usano riferimenti
  diversi; la fonte non chiarisce quale).
- Per il **grano** basta moltiplicare la dimensione del grano in pixel per `k` (e l'ampiezza RMS percepita all'incirca per
  `k`, perché la granulosità RMS a parità di apertura di misura cresce con l'ingrandimento) [gen, regola pratica].
- Per l'**alone**: raggio fisico dell'ordine di 2× lo spessore del supporto (supporto triacetato ≈ 0,12–0,14 mm, quindi
  raggio ≈ 0,2–0,3 mm) [gen]; in pixel: `r_px = r_mm × W_px / L_fmt`.
- Per la **nitidezza**: MTF dell'emulsione in cicli/mm convertita in cicli/immagine moltiplicando per `L_fmt`; più lente e
  proiezione (la catena reale per IMAX e 70 mm era limitata dalle ottiche quanto dall'emulsione) [gen].

### 2.1 Todd-AO e Dimension 150

- 65 mm negativo, 70 mm copie, **5 perforazioni** verticali, **2,20:1**, sei piste magnetiche sulla copia 70 mm
  [doc, Wikipedia Todd-AO].
- **30 fps** nel sistema originale: solo *Oklahoma!* e *Il giro del mondo in 80 giorni*; poi tutti i film a 24 fps
  [doc, idem; 30 fps anche in [doc, ASC]].
- Ottiche con campo di **128° ("bug-eye"), 64°, 48°, 37°** [doc, Wikipedia]. Il 128° produce forte curvatura delle linee
  ai bordi (distorsione a barilotto molto marcata, corretta in parte da stampe ottiche per schermi curvi) [gen].
- Schermi per lo più **moderatamente curvi**; la variante **Dimension 150** usava schermi profondamente curvi; due film:
  *La Bibbia* e *Patton* [doc, Wikipedia]. Sale "Cinestage" profondamente curve: Rivoli (NY), Cinestage (Chicago), Hunt's
  Cinestage (Columbus) [doc].
- Aspetto: grano quasi invisibile (k ≈ 0,47), nitidezza alta ma morbida ai bordi per le ottiche grandangolari estreme,
  colore Eastman dell'epoca [gen].

Modello: `k = 0,47`; distorsione radiale opzionale `k1` (barilotto) 0 → −0,15 per il "128°"; vignettatura 0,3–0,6 stop
ai bordi; **cadenza 30 fps** come attributo di metadati (il motore non cambia velocità ma può modulare il flicker:
a 30 fps con otturatore a due pale ≈ 60 Hz, meno percepibile) [gen].

### 2.2 Cinerama e Kinopanorama

- Tre negativi 35 mm affiancati, ciascuno alto **6 perforazioni**; apertura negativo per pannello **0,996" × 1,116"**,
  copia 0,985" × 1,088"; **26 fps** (24 fps negli ultimi due film); tre obiettivi **27 mm**; schermo curvo a **146°**;
  rapporto massimo **2,59:1** (con riserva di composizione a ≈2,65:1 per limiti architettonici) [doc, Widescreen Museum].
  Campo 146° × 55,5° [doc, ASC]. Lo schermo era fatto di **1000–2000+ strisce verticali** larghe 3/4" sovrapposte per
  evitare riflessi incrociati [doc, Widescreen Museum].
- Giunzioni tra pannelli: le tre proiezioni si sovrapponevano con bordi sfumati da lamelle oscillanti nei proiettori
  ("jigolo"); le giunzioni restano visibili come **bande verticali** con differenze di densità e colore, piccoli
  disallineamenti verticali e **instabilità indipendente** dei tre pannelli; oggetti vicini alla camera si "spezzano"
  per parallasse lungo la giunzione [gen, noto dal restauro "smilebox"].
- **Kinopanorama** (URSS): equivalente sovietico a tre film, sviluppato 1956–57, prima proiezione 28 febbraio 1958; funziona
  quasi come Cinerama, con differenze di perforazione (KS 1866 contro BH 1866); camera PSO-1960 con ottiche
  intercambiabili 27–100 mm (Cinerama usava focale fissa); 12 film a tre pellicole fino al 1966, poi 8 in **Kinopanorama 70**
  a negativo singolo [doc, Wikipedia Kinopanorama]. Cadenza [gen: probabilmente 25 fps, da verificare].

Modello "tri-strip":
- Dividere l'immagine in 3 pannelli con larghezza 1/3 ciascuno (o pesata secondo il campo 146°); **ogni pannello ha la sua
  istanza** di grano, gate weave (σ indipendente), bilanciamento (σ 0,02 densità) e luminosità (±3%).
- Giunzione: banda di larghezza 0,5–1,5% della larghezza totale con miscela incrociata; offset verticale casuale
  0–0,3% dell'altezza; **opzione "jigolo"**: la banda trema orizzontalmente (σ 0,1%) a frequenza della cadenza.
- Curvatura dello schermo: rimappatura cilindrica opzionale (visualizzazione "smilebox": bordo superiore e inferiore curvi
  verso l'alto) con intensità 0..1.
- Controlli: **Visibilità giunzioni** (0..1, default 0,4), **Disallineamento pannelli** (0..1, default 0,3),
  **Smilebox** (0..1, default 0), **Cadenza** (24/25/26, metadato per il flicker).

### 2.3 CinemaScope e le ottiche anamorfiche

- Compressione **2:1**; rapporti **2,66:1** (apertura muta piena), **2,55:1** (con piste magnetiche), **2,35:1**
  (con colonna ottica), poi **2,39:1** [doc, Wikipedia CinemaScope]. Produzione Fox 1953–1967; brevetto Hypergonar di
  Chrétien del 1926 [doc].
- **"Mumps"**: le prime ottiche perdevano compressione alle brevi distanze, allargando i volti nei primi piani; le ottiche
  Panavision con doppi elementi anamorfici rotanti mantenevano 2× costante e risolsero il problema [doc, idem]. Ottiche
  combinate Bausch & Lomb: 35, 40, 50, 75, 100, 152 mm, poi 25 mm [doc].
- Caratteristiche ottiche anamorfiche (cilindro frontale) [gen]: **bokeh ovale verticale** (rapporto altezza/larghezza ≈ q
  sul negativo de-compresso), **flare orizzontali** a striscia (tipicamente bluastri con trattamenti antiriflesso
  dell'epoca), **distorsione a barilotto orizzontale** e caduta di nitidezza e "bending" delle verticali ai bordi,
  **breathing** e alterazione della compressione con la messa a fuoco, astigmatismo che rende il fuori fuoco "a
  pennellata", vignettatura ovale.
- **CinemaScope 55** (55,625 mm, negativo ≈46 × 36 mm, 8 perf negativo / 6 perf stampa, 1,275:1 → 2,55:1) [doc].

Modello anamorfico (applicabile anche a Technirama, Ultra Panavision 70 e 35 mm anamorfico moderno):

| Parametro | 2× anni '50 | 2× Panavision anni '70 | 1,5× Technirama | 1,25× UP70 |
|---|---|---|---|---|
| `squeeze q` | 2,0 | 2,0 | 1,5 | 1,25 |
| bokeh (rapporto altezza/larghezza) | 1,8 | 1,9 | 1,4 | 1,2 |
| flare orizzontale (intensità, lunghezza in % larghezza) | 0,5 / 40% | 0,35 / 60% | 0,3 / 30% | 0,2 / 25% |
| tinta flare | blu-ciano | blu | blu-ambra | neutra |
| distorsione barilotto orizz. `k1x` | −0,06 | −0,03 | −0,03 | −0,02 |
| caduta MTF ai bordi (fattore) | 0,5 | 0,7 | 0,75 | 0,8 |
| "mumps" (allargamento al centro nei primi piani) | 0 → +12% | 0 | 0 | 0 |

Tutti [gen]. Controlli: **Fattore anamorfico** (1,25 / 1,33 / 1,5 / 2), **Carattere ottica** (vintage → moderna:
interpola le righe), **Flare** (0..1), **Mumps** (0..1, solo se "vintage" e q = 2).

### 2.4 VistaVision, Technirama, Super/Ultra Panavision 70, Techniscope

- **VistaVision** (Paramount, 1954): 35 mm in **orizzontale**, fotogramma di **8 perforazioni** in larghezza, riduzione in
  stampa, 1,66:1 indicato da ASC [doc, ASC]; 1,50:1 nativo, poi ritaglio [doc, lista Wikipedia]. Su *Bianco Natale*
  furono provate copie con compressione 1,5:1 [doc, Widescreen Museum]. Aspetto: grana molto fine dopo riduzione
  ottica (la stampa per riduzione "integra" il grano), alta nitidezza [gen]. `k = 0,66`; ulteriore **attenuazione del
  grano 0,8** se la catena è "negativo grande → stampa ridotta" [gen].
- **Technirama**: 8 perf orizzontali, apertura 1,496" × 0,992", **1,5×**, max 2,25:1; copie 35 mm anamorfiche 2× (2,35:1)
  e **Super Technirama 70** sferico 2,21:1 [doc, Widescreen Museum].
- **Ultra Panavision 70 / MGM Camera 65**: 65 mm 5 perf, **1,25×**, apertura 2,072" × 0,906", max **2,76:1** (consigliato
  2,55), copia 70 mm 1,912" × 0,870"; riduzione 35 mm 2× a 2,35:1 [doc, Widescreen Museum]. Riutilizzato in *The Hateful
  Eight* [doc, ASC blog, titolo].
- **Super Panavision 70**: 65 mm sferico, 2,20:1 [gen].
- **Techniscope** (Technicolor Italia, 1960): **2 perforazioni**, sferico, fotogramma metà altezza dell'anamorfico; il
  rapporto anamorfico 2:1 (allungamento verticale, equivalente a una compressione orizzontale) era applicato in stampa
  per ottenere una copia anamorfica standard [doc, Widescreen Museum]; ≈2,33–2,39:1
  [doc lista Wikipedia, gen]. Usato nei western italiani: **grana circa doppia in area** rispetto al 4-perf anamorfico,
  perché l'area è metà; profondità di campo maggiore (ottiche sferiche corte) [gen]. Nel motore: `k = 1,13` su entrambi
  gli assi (contro k vert. 0,57 dell'anamorfico 2×), grano rotondo, bokeh rotondo, niente flare anamorfici.

### 2.5 IMAX 15/70 e DMR

- 65 mm negativo / 70 mm copia, **15 perforazioni orizzontali**, passo 0,1870", 24 fps, ≈334 piedi/min; fotogramma
  **70,41 × 52,63 mm** (J. Houston) o 69,6 × 48,6 mm (D. Case); area ≈2,7× il 35 mm a fotogramma intero; scansioni
  comunemente 6K×8K (anche 4K×3K sufficienti per riprese dal vero; 6K per CGI e titoli) [doc, cinematography.net].
- Aspetto: grano praticamente invisibile in proiezione (k ≈ 0,35); il limite reale è ottica e profondità di campo
  ridottissima su un formato grande [gen]. Il 1,43:1 è il rapporto nativo [doc, lista Wikipedia].
- **IMAX DMR** (Digital Media Remastering): processo di ricampionamento, riduzione del grano e affilatura per gonfiare
  35 mm/digitale a IMAX [gen]. Nel motore: un preset "DMR" = riduzione del grano (0,5×) + unsharp moderato
  (raggio 1,5 px, quantità 0,4), da mostrare come elaborazione, non come pellicola.

### 2.6 Passo ridotto: Super 16, 16, Super 8, 8 mm, 9,5 mm, 17,5 mm, 28 mm

Fatti principali:
- Super 8: 1 perforazione, ≈1,33:1 [doc, lista Wikipedia]; fotogramma normale delle camere sovietiche Kvarts
  4,22 × 5,69 mm e formato largo 4,22 × 10,8 mm [doc, filmkorn]. Cadenze 18 e 24 fps; 50 piedi a 18 fps ≈ 3 min 20 s
  [gen].
- Regular/Standard 8: pellicola 16 mm esposta due volte e tagliata; perforazioni grandi sul bordo, 16–18 fps [gen].
- 9,5 mm Pathé-Baby (1922): perforazione **centrale tra i fotogrammi**, quindi fotogramma quasi a tutta larghezza
  (efficienza superiore all'8 mm); 14–16 fps; danni tipici: graffi e rotture che partono dalla perforazione centrale [gen].
- 17,5 mm: 35 mm tagliato a metà (Pathé Rural, 1926, e precedenti "Biokam") [gen].
- 28 mm Pathé KOK (1912): primo formato amatoriale su supporto di sicurezza (non nitrato) [gen].
- Super 16: Super 16 ricavato allargando il fotogramma sul lato della colonna (perforazione singola), ≈1,66–1,85:1 [doc
  lista Wikipedia per AR].

Difetti di proiezione e ripresa [gen]:

| Formato | Gate weave tipico (σ, % altezza) | Flicker | Altro |
|---|---|---|---|
| 35 mm professionale | 0,05–0,1% | trascurabile a 48 Hz | polvere bianca (in stampa), righe |
| 16 / Super 16 | 0,1–0,2% | basso | grano dominante |
| Super 8 | 0,3–0,8% (la pressione nel caricatore in plastica è irregolare) | medio; esposimetro automatico che "respira" | bordi arrotondati del fotogramma, fuori fuoco ai bordi, salti di esposizione a inizio ripresa |
| 8 mm / 9,5 mm | 0,5–1,0% | alto a 16 fps (otturatore a tre pale ≈ 48 Hz in proiezione) | 9,5: graffio centrale |

Bordi del fotogramma: Super 8 e 8 mm mostrano un bordo **arrotondato** e spesso irregolare del finestrino (raggio angoli
≈ 3–6% dell'altezza) e in scansione "overscan" la perforazione a sinistra [gen].

Controlli "passo ridotto": **Formato** (elenco: determina k, bordo, weave), **Usura** (0..1: graffi, polvere, perdita di
fuoco ai bordi), **Cadenza** (16/18/24, guida flicker e weave), **Mostra bordo** (spento / finestrino / overscan con
perforazioni).

### 2.7 Look del nitrato

- Supporto nitrato di cellulosa (fino al 1951 circa per il 35 mm professionale) [gen]. Le copie nitrato in B/N sono
  ricordate per neri molto profondi e "luminosità argentea": emulsioni ricche d'argento, dmax di stampa elevato (≥2,5–3),
  supporto molto trasparente [gen]. Degrado: ingiallimento/imbrunimento del supporto, macchie ambrate, perdita di
  immagine "a bolle" e bordi appiccicosi negli stadi avanzati [gen].
- Modello: B/N con dmax 3,0, toe corto (1 stop), gamma di stampa 2,0–2,3, alone ridotto (strato antialone già presente
  ma supporto diverso); degrado come tinta ambra a bassa frequenza (opzionale).

### 2.8 Proposta di controlli per l'intero blocco "formato"

1. **Formato** (elenco della tabella 2.0): imposta `k`, `q`, AR, bordi, fps di riferimento.
2. **Ottica** (sferica moderna / sferica vintage / anamorfica vintage / anamorfica moderna / grandangolo estremo).
3. **Proiezione** (0..1): gate weave, flicker, oscillazione di luminosità, polvere; valori scalati per formato.
4. **Giunzioni** (solo Cinerama/Kinopanorama): visibilità e disallineamento.

---

## 3. Bianco e nero speciale, infrarosso, istantanea

### 3.1 Infrarosso B/N (Kodak HIE e simili)

- Kodak High-Speed Infrared: sensibile al visibile (verde ridotto), **infrarosso fino a 900 nm**, un po' di UV; **nessuno
  strato antialone e nessun supporto grigio** (base completamente trasparente); caricamento al buio totale; supporto in
  poliestere che si graffia facilmente; fuori produzione a fine **2007** [doc, Wikipedia]. Senza filtro rosso l'effetto IR
  si vede poco; filtro rosso 25 tipico, 87/89B per IR più puro [gen].
- **Effetto Wood**: la vegetazione sana riflette fortemente il vicino infrarosso (riflettanza fogliare ≈ 40–60% oltre
  700 nm contro ≈ 10–15% nel verde visibile) e appare **bianca**; cielo blu sereno e acqua **neri** (poco IR diffuso);
  pelle **lattea e senza pori**, occhi scuri, labbra chiare (l'IR penetra nella pelle) [gen].
- **Alone** fortissimo attorno alle alte luci per l'assenza di antialone, con bagliore diffuso tipo "sogno" [doc per
  assenza di antialone; aspetto gen].
- Grano grosso e visibile [gen]. Fuoco: l'IR mette a fuoco più lontano (indice IR sugli obiettivi) → leggera morbidezza
  [gen].

Il motore ha solo RGB: serve un **canale pseudo-NIR** stimato. Proposta [gen]:

```
veg = clamp((G - max(R,B)) / (G + 1e-4), 0, 1) ^ 0.7        # quanto "verde clorofilla"
sky = clamp((B - max(R,G)) / (B + 1e-4), 0, 1)              # quanto "blu cielo"
NIR = 0.55*R + 0.35*G + 0.10*B + A_veg*veg*G - A_sky*sky*B
```

Valori di partenza: `A_veg = 2,5`, `A_sky = 0,8`. Esposizione del record: `E = w_nir*NIR + (1-w_nir)*(0,7R + 0,25G + 0,05B)`
con `w_nir` = 0,6 (filtro 25), 0,85 (87), 0,95 (89B/R72). Curva: gamma 0,9, toe 1,5 stop, spalla 1 stop.
Alone: intensità 0,6, raggio 0,5 mm sul film, kernel con coda lunga (somma di due gaussiane, 0,3 e 1,2 mm), applicato
alle densità alte prima della curva. Grano: 1,8× il riferimento.

Controlli: **Filtro** (giallo 12 / rosso 25 / 87 / 89B), **Effetto Wood** (scala A_veg, 0..2, default 1),
**Bagliore** (alone, 0..1, default 0,6), **Cielo scuro** (A_sky, 0..1, default 0,5).

### 3.2 Aerochrome e Ektachrome Infrared (falso colore)

- **Aerochrome III 1443**: invertibile in falso colore sensibile all'IR fino a ≈900 nm; ISO 400 a livello del mare, ridotto
  di circa 1 stop ogni 2000 piedi sopra i 5000 piedi; Kodak raccomanda **filtro giallo Wratten 12** (anche arancio 21 e
  rosso 25/26); trattamento **E-6** o **AR-5**; minilab con luci IR interne lo velano; fogliame **rosa, porpora o rosso
  intenso**, acqua e cielo anche neri, cielo coperto bianco [doc, EMULSIVE].
- Struttura [gen, coerente con la letteratura ASPRS/USGS]: tre strati sensibili a **verde, rosso e infrarosso** (il filtro
  giallo elimina il blu, a cui tutti gli strati sono sensibili). In stampa invertibile: verde → **blu**, rosso →
  **verde**, IR → **rosso**. Erba e alberi (molto IR) diventano rosso-magenta, la pelle verdastra-giallognola, il
  cielo blu diventa scuro/blu intenso, oggetti rossi diventano giallo-verdi.
- **Kodak Ektachrome Infrared (EIR)**: versione 35 mm della stessa idea, E-6/AR-5, fuori produzione intorno al 2009 [gen].

Modello: mappatura di canale dopo la stima pseudo-NIR (§3.1):
`R_out = HD_r(NIR)`, `G_out = HD_g(R)`, `B_out = HD_b(G)`, poi assorbimenti dei coloranti tipo invertibile E-6 (D con
fuori diagonale 0,10–0,15) e gamma invertibile 1,6–1,8 [gen]. Il filtro cambia la miscela: con giallo 12 una parte del
blu residuo (0,1) entra in tutti i record; con 25 i record verdi si svuotano (fogliame più rosso, cielo quasi nero).

Controlli: **Filtro** (12 / 21 / 25), **Intensità IR** (A_veg, default 1,2), **Saturazione falso colore** (0..1,
default 0,7), **Età/lotto** (spostamento verso il magenta e minore separazione, tipico del materiale scaduto, 0..1).

### 3.3 Lith e copia ad alto contrasto

- Pellicole **lith** (ortocromatiche, sviluppo "infettivo"): gamma molto alto (> 8–10), quasi solo bianco e nero; con
  sviluppatore lith su carta si ottengono ombre dure e alte luci morbide e calde (rosa-salmone) [gen].
- Copia ad alto contrasto (duplicazione titoli, contatti): gamma 2–4 [gen].

Modello: curva con gamma 6–12 e toe/spalla 0,3 stop; sensibilità ortocromatica (`S` = 0,05R + 0,55G + 0,40B: il rosso
diventa nero); bordo "infettivo": l'immagine si sviluppa a macchia d'olio → filtro di soglia con dilatazione 1–2 px e
frange (effetto Eberhard/bordo, sovra-densità 0,1 nelle zone vicine a forti transizioni). Per "lith print": dividere
ombre (gamma alta, tinta neutra-fredda) e alte luci (gamma 0,6, tinta calda R+0,08 G+0,02).
Controlli: **Durezza** (gamma 2 → 12), **Soglia** (stop, −2..+2), **Lith print** (0..1), **Calore alte luci** (0..1).

### 3.4 Polaroid e istantanee

Fatti:
- Color 600: ISO 640, SX-70: ISO 160, Instax: ISO 800; Polaroid 600 si sviluppa in ≈15 minuti (Instax circa la metà),
  monocromo e Duochrome ≈10 min; gamma dinamica ≈ **3,5 stop**, alto contrasto; dominanti **rosa nelle alte luci e
  verde nelle ombre**; il **freddo** indebolisce lo sviluppo e dà forti dominanti **turchesi**; proteggere dalla luce durante
  lo sviluppo dà più contrasto; la luce solare dopo lo sviluppo degrada le alte luci [doc, analog.cafe].
- Fuji **FP-100C** (peel-apart, fuori produzione 2016 [gen]): ISO 100 [gen]; il limite inferiore di temperatura di ripresa
  fu esteso da 15 °C a **10 °C**, variazioni di prestazione con la temperatura di sviluppo ridotte, scolorimento alla luce
  ridotto del ≈75% rispetto al tipo precedente [doc, brochure Fujifilm]. Tempo di sviluppo standard ≈ 90 s a 25 °C [gen].
- Chimica [gen]: **diffusione di coloranti** ("dye developers" in SX-70: molecole che sono contemporaneamente colorante e
  sviluppatore), reagente viscoso contenuto in una "busta" alla base della foto e spalmato dai rulli; l'**opacizzante**
  protegge l'immagine dalla luce nei primi minuti (l'immagine "emerge" dal blu-verde scuro). Nelle peel-apart il negativo
  si separa dal positivo; il negativo Fuji può essere recuperato (sbiancato) come immagine a sé.
- Bordi [gen]: la stesura del reagente dai rulli lascia **disuniformità vicino al bordo d'ingresso** (lato busta) e
  **mancanza di reagente** verso l'angolo opposto (zone non sviluppate, chiare o con "frange"); nelle peel-apart bordi
  irregolari con segni dei rulli e sbavature di chimica; riquadro bianco caratteristico (SX-70/600: 79 × 79 mm di
  immagine con bordo inferiore largo; Instax mini 46 × 62 mm) [gen].
- Polaroid moderne (Impossible Project dal 2008, poi Polaroid): chimica nuova, più sensibile alla temperatura e alla luce,
  colori meno stabili nelle prime ore [gen].

Modello "istantanea":

| Parametro | SX-70 anni '70 | Polaroid 600 moderna | Peel-apart FP-100C | Instax |
|---|---|---|---|---|
| gamma | 1,3 | 1,5 | 1,1 | 1,2 |
| latitudine (stop tra toe e spalla) | 4,5 | 3,5 [doc] | 5,5 | 5,0 |
| bilanciamento alte luci (R,G,B offset) | +0,02, 0, −0,03 | +0,03, −0,02, 0 (rosa) | 0, 0, +0,01 | −0,01, 0, +0,02 |
| bilanciamento ombre | −0,01, +0,01, +0,02 | −0,02, +0,03, +0,01 (verde) | 0, +0,01, +0,02 | 0, 0, +0,02 |
| saturazione | 0,85 | 0,9 | 1,0 | 1,05 |
| nero massimo (dmax effettivo) | 1,6 | 1,7 | 1,9 | 2,0 |
| grano | quasi nullo (0,2) | 0,2 | 0,3 | 0,2 |

Temperatura di sviluppo `T` (°C), effetto [gen, direzione coerente con [doc]]: sotto 13 °C → densità −15%, contrasto −20%,
spostamento turchese (G+0,03, B+0,05, R−0,04); sopra 28 °C → alte luci gialle-ambra (R+0,04, B−0,05), contrasto +10%,
perdita di dettaglio nelle alte luci. Tempo di sviluppo (peel-apart): `t/t_std` < 1 → immagine slavata e fredda;
> 1 → più contrasto e caldo.

Bordo: maschera parametrica con fronte di reagente (profilo irregolare a bassa frequenza, ampiezza 1–3% del lato),
zona non sviluppata all'angolo opposto alla busta (probabilità 0..1), striature parallele alla direzione dei rulli.

Controlli: **Tipo** (SX-70 / 600 / peel-apart / Instax), **Temperatura** (5–35 °C, default 21), **Tempo di sviluppo**
(0,5–2×, default 1), **Difetti di stesura** (0..1, default 0,2), **Bordo** (spento / cornice / cornice con sbavature).

### 3.5 Redscale

- Pellicola colore negativa caricata al contrario ed esposta **attraverso il supporto**: la luce attraversa prima
  supporto e (dove presente) antialone, poi lo strato rosso, verde e infine quello blu; quasi nessuna informazione nel
  record blu, dominante rosso-arancio-gialla, contrasto alto, grano grosso; servono 1–3 stop di sovraesposizione
  (più esposizione → più giallo) [gen].

Modello: invertire l'ordine degli strati nella matrice `S` e attenuare la luce che arriva a ciascun record con un
fattore di trasmissione cumulativo: record R ×0,35 (base + antialone), G ×0,15, B ×0,04 [gen]; alone forte (la luce
riflessa dal retro attraversa la pellicola: intensità 0,5); grano 1,5×. Controllo principale **Sovraesposizione**
(0–4 stop, default 2): sposta da rosso cupo (0–1) a arancio (2) a giallo-pesca (3–4).

---

## 4. Riepilogo dei modelli proposti

| Modello | Blocchi principali | Controlli (max 4) |
|---|---|---|
| Agfa-Est (Sovcolor/ORWO/Svema/Gevacolor) | `S` sovrapposto, `mask` 0–0,3, `D` sporco, dmax basso, variabilità lotto | Epoca, Pastello, Instabilità, Origine stampa |
| Formato | `k`, `q`, AR, bordo, weave, flicker | Formato, Ottica, Proiezione, Giunzioni |
| Anamorfico | squeeze su grano/alone/bokeh, flare, distorsione, mumps | Fattore, Carattere, Flare, Mumps |
| Tri-strip (Cinerama/Kinopanorama) | 3 istanze indipendenti, giunzioni, smilebox | Giunzioni, Disallineamento, Smilebox, Cadenza |
| IR B/N | pseudo-NIR, alone senza antialone | Filtro, Wood, Bagliore, Cielo scuro |
| Falso colore IR | NIR→R, R→G, G→B | Filtro, Intensità IR, Saturazione, Età |
| Lith / alto contrasto | gamma 6–12, ortocromatico, bordo infettivo | Durezza, Soglia, Lith print, Calore |
| Istantanea | latitudine ridotta, dominanti per zona, temperatura, bordo chimico | Tipo, Temperatura, Tempo, Difetti |
| Redscale | ordine strati invertito, trasmissione cumulativa | Sovraesposizione |

Ordine di lavoro suggerito [gen]: (1) blocco Formato con `k` e `q` (riutilizza grano e alone esistenti, guadagno grande
con poco codice); (2) anamorfico; (3) Agfa-Est; (4) IR e falso colore (richiedono il canale pseudo-NIR); (5) istantanea.

## 5. Cosa resta incerto

- Schede tecniche GOST di DS-5, LN-7, LN-8, DS-8 (sensibilità esatte, anni): non trovate online in questa sessione.
- Sigla Fujicolor **8517**: non verificata.
- Cadenza di Kinopanorama, dimensioni esatte di 17,5 mm e 28 mm, 9,5 mm.
- Tutti i valori numerici dei modelli sono scelte di partenza da tarare per osservazione su materiale di riferimento
  (scansioni di copie d'epoca, non LUT commerciali).
- Il canale pseudo-NIR è un'approssimazione: non può distinguere un oggetto verde dipinto da una foglia (fa "bianco"
  anche la vernice verde). Va dichiarato come tale nell'interfaccia e nella documentazione.

## Fonti

Consultate in questa ricerca (contenuto letto):
- Wikipedia, Svema: https://en.wikipedia.org/wiki/Svema
- Wikipedia, ORWO: https://en.wikipedia.org/wiki/ORWO
- Wikiwand / Wikipedia, Sovcolor (Agfacolor): https://www.wikiwand.com/en/Sovcolor
- filmportal.de, "Kine-Farbnegativfilme" (tabella Agfa Wolfen 1939–1964): https://www.filmportal.de/sites/default/files/Kine-Farbnegativfilme.pdf
- Cinema Riot, "Soviet Kodak Moments": https://cinemariot.substack.com/p/soviet-kodak-moments
- Wikipedia, Gevacolor: https://en.wikipedia.org/wiki/Gevacolor
- Wikipedia, China Lucky Film: https://en.wikipedia.org/wiki/China_Lucky_Film
- American WideScreen Museum, Cinerama specs: https://www.widescreenmuseum.com/widescreen/cineramaspecs.htm
- American WideScreen Museum, Ultra Panavision 70 specs: https://www.widescreenmuseum.com/widescreen/ultrapanavisionspecs.htm
- American WideScreen Museum, Technirama specs: https://www.widescreenmuseum.com/widescreen/techniramaspecs.htm
- American WideScreen Museum, Film dimensions: https://www.widescreenmuseum.com/widescreen/filmdims.htm
- ASC, "Summary of current widescreen systems of photography": https://theasc.com/article/summary-of-current-widescreen-systems-of-photography/
- Wikipedia, Todd-AO: https://en.wikipedia.org/wiki/Todd-AO
- Wikipedia, CinemaScope: https://en.wikipedia.org/wiki/CinemaScope
- Wikipedia, Kinopanorama: https://en.wikipedia.org/wiki/Kinopanorama
- cinematography.net, IMAX 15-perf 70 mm tech specs: https://cinematography.net/edited-pages/IMAX15perf_70mmTechSpecs.htm
- Wikipedia, List of film formats: https://en.wikipedia.org/wiki/List_of_film_formats
- filmkorn.org, 8 mm film formats: https://www.filmkorn.org/super8data/database/articles_list/8mm_formats.htm
- Wikipedia, Kodak High-Speed Infrared: https://en.wikipedia.org/wiki/Kodak_High-Speed_Infrared
- EMULSIVE, Kodak Aerochrome III 1443 review: https://emulsive.org/reviews/film-review-kodak-aerochrome-iii-infrared-film-1443
- analog.cafe, Polaroid Color 600 review: https://analog.cafe/r/polaroid-600-colour-film-review-kxll
- Fujifilm FP-100C brochure (Freestyle): https://www.freestylephoto.com/static/pdf/product_pdfs/fujifilm/Fuji_FP-100C_Brochure.pdf

Individuate ma non lette (utili per la verifica dei punti [gen]):
- filmcolors.org, Gevacolor Negative: https://filmcolors.org/timeline-entry/1308/
- filmcolors.org, Fujicolor: https://filmcolors.org/timeline-entry/1367/
- M. Talbert, Early Gevaert colour materials: https://photomemorabilia.co.uk/Colour_Darkroom/Early_Gevaert.html
- SMPTE Journal, "The Gevacolor Positive Film Type 953": https://journal.smpte.org/periodicals/Journal%20of%20the%20SMPTE/73/1/8/
- SMPTE Journal, "Fujicolor Negative Films — F-Series": https://journal.smpte.org/periodicals/SMPTE%20Journal/98/11/18/
- SMPTE Journal, "Two New Types of Fujicolor Films…": https://journal.smpte.org/periodicals/SMPTE%20Journal/88/7/4/
- Frames Cinema Journal, "Small-Gauge Colour Visions" (Ferraniacolor e amatori): https://framescinemajournal.com/article/small-gauge-colour-visions-the-role-of-amateur-filmmakers-in-italys-transition-from-black-and-white-to-colour/
- Wikipedia, Ferrania: https://en.wikipedia.org/wiki/Ferrania
- ASPRS 1977, infrared colour film: https://asprs.org/wp-content/uploads/pers/1977journal/jan/1977_jan_61-72.pdf
- USGS 1969 report (colour IR): https://pubs.usgs.gov/of/1969/0197/report.pdf
- Fstoppers, storia di Aerochrome: https://fstoppers.com/film/incredible-true-story-how-and-why-kodak-aerochrome-was-created-534094
- Harvard Business School, SX-70 R&D: https://www.library.hbs.edu/hc/polaroid/instant-photography/research-and-development-project-code-sx-70
- in70mm.com, Kinopanorama 2003: https://www.in70mm.com/news/2003/kinopanorama/index.htm
- Kosmofoto, Lucky colour film 2024: https://kosmofoto.com/2024/05/chinas-lucky-film-may-bring-back-colour-film-production-by-the-end-of-2024
- SMPTE ST 417 (proiezione 70 mm): https://pub.smpte.org/latest/st417/st0417-2005_stable2015.pdf

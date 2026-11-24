# Video analogico e formati a nastro: meccanismi, numeri, mappatura per un plugin OFX

Ricerca preliminare per un plugin OFX originale che simula il video analogico modellando la catena del segnale.
Nessuna modifica al codice. Data: 2026-11-24.

**Legenda di affidabilità**
- **[doc]** dato letto in una fonte consultata durante questa ricerca. La fonte è indicata tra parentesi; gli URL sono nell'elenco finale.
- **[code]** valore letto direttamente nel sorgente di un progetto open source (ntsc-rs, composite-video-simulator, vhs-decode). Sono valori di implementazione, a volte tarati "a occhio" dagli autori, e non specifiche del formato.
- **[gen]** conoscenza tecnica generale non verificata in questa sessione. Va controllata prima di trattarla come specifica.
- **[deriv]** calcolo nostro, ottenuto dai dati precedenti con la formula indicata.

Limite della ricerca: il budget di accesso web si è esaurito prima di consultare le specifiche SMPTE/IEC e le pagine
sugli shader RetroArch. Le parti marcate [gen] su Quad, MII, D1/D2, SECAM e RetroArch non sono quindi verificate.

---

## 1. Il segnale composito di base

### 1.1 Grandezze fondamentali

| Grandezza | NTSC (M) | PAL (B/G/I/D) | Note |
|---|---|---|---|
| Righe / campi | 525 / 59,94 Hz, 262,5 righe per campo | 625 / 50 Hz | NTSC [doc Wikipedia NTSC] |
| Frequenza di riga fH | 15 734 Hz | 15 625 Hz | [doc Wikipedia NTSC], PAL [gen] |
| Sottoportante colore fsc | 315/88 MHz = 3,579545 MHz ±10 Hz | 4,43361875 MHz ±5 Hz | [doc Wikipedia NTSC, PAL] |
| Relazione fsc/fH | 227,5 fH (mezza riga di offset) | 283,75 fH + 25 Hz (offset di un quarto di riga) | [gen] |
| Banda luma di trasmissione | 4,2 MHz | B/G 5,0; I 5,5; D/K 6,0 MHz | NTSC: banda laterale 4,2 MHz [doc Wikipedia NTSC]; PAL [doc Wikipedia PAL] |
| Banda crominanza | I ≈ 1,3 MHz, Q ≈ 0,4 MHz | U, V ≈ 1,3 MHz ciascuno | [doc Wikipedia NTSC, PAL] |
| Riga attiva | ≈ 52,66 µs | ≈ 52 µs | [gen]. PAL: sincronismo 4,7 µs, intervallo di sincronismo totale 12,05 µs [doc Wikipedia PAL] |
| Burst | ≥ 8 cicli sul back porch, 40 IRE p-p | ~10 cicli, ±45° alternato (swinging burst) | numero di cicli NTSC [doc]; swinging burst PAL [doc]; ampiezze [gen] |
| Nero | 7,5 IRE di setup (NTSC-USA); 0 IRE in NTSC-J | coincide con il blanking (0 mV) | il nero a 7,5 IRE compare nella tabella delle barre [doc Wikipedia NTSC]; NTSC-J [gen] |
| Sincronismo | −40 IRE (286 mV) | −300 mV | [gen] |
| Bianco | 100 IRE = 714 mV sopra il blanking | 700 mV | [gen] |
| Picco composito legale | ~120 IRE (limite di trasmissione FCC) | ~133% (≈ 930 mV) | [gen] |
| Digitale Rec.601 | Y 16–235, Cb/Cr 16–240, luma campionata a 13,5 MHz | uguale | [gen]; la luma a 13,5 MHz è confermata per il DV [doc Wikipedia DV] |

**Conversione MHz → linee TV (TVL, per altezza di quadro).** Con riga attiva T ≈ 52,6 µs e formato 4:3:
TVL ≈ 2 · B · T · (3/4) ≈ **79 TVL per MHz** in NTSC e **78 TVL per MHz** in PAL [deriv]. Verifica: VHS a 3 MHz
→ 237 TVL, in linea con i 240 TVL dichiarati [doc Wikipedia VHS].

### 1.2 Spazi colore
- NTSC usa Y'IQ: I è l'asse arancio–ciano a larghezza di banda piena (1,3 MHz), Q l'asse viola–verde a banda ridotta
  (0,4 MHz) [doc Wikipedia NTSC]. In pratica molti encoder NTSC usano assi U/V con banda uguale (~1,3 MHz) [gen].
  Anche ntsc-rs usa 1,3 MHz per I ma 0,6 MHz per Q, e nel sorgente l'autore osserva che non ha trovato una fonte per
  il valore di 0,4 MHz [code ntsc-rs].
- PAL usa Y'UV. Il componente V inverte la fase a ogni riga [doc Wikipedia PAL].
- Tutta l'elaborazione analogica avviene su segnali con correzione gamma (Y' e non Y lineare) [gen]. Il plugin deve
  quindi lavorare sui valori display-referred Rec.601/709, non in luce lineare.

### 1.3 Artefatti della codifica composita

**Dot crawl / cross-luminance.** Il decoder separa male la crominanza dalla luma: la sottoportante resta nella luma
come trama di punti. In NTSC, con 227,5 fH, la fase si inverte a ogni riga e a ogni quadro: ne risulta una scacchiera
che "striscia" lungo i bordi verticali colorati, con un ciclo di 2 quadri (4 campi) [gen]. Era già visibile come
"puntini striscianti" sui primi televisori in bianco e nero, che non filtravano la crominanza [doc Wikipedia NTSC]. In
PAL l'offset di un quarto di riga produce una trama diagonale con ciclo di 4 quadri (8 campi) [gen].

**Cross-colour / rainbow.** È il caso opposto: dettagli di luma vicini a fsc (righe fini, tessuti, griglie a circa
3–4 MHz) vengono decodificati come crominanza e diventano arcobaleni di falsi colori. Con il decoder a notch sono
fermi o lampeggianti; con il comb restano sui dettagli diagonali [gen].

**Notch e comb.**
- *Notch* (trappola a fsc nella luma, passa-banda nella crominanza): costa poco, ma toglie il dettaglio luma tra circa
  3 e 4,2 MHz (risoluzione effettiva ≈ 3 MHz, 240–260 TVL) e lascia molto cross-colour [gen].
- *Comb 1H in NTSC*: somma o sottrae la riga precedente. Elimina il dot crawl sulle zone verticalmente uniformi, ma
  dimezza la risoluzione verticale della crominanza e lascia "hanging dots" sui bordi orizzontali di colore [gen].
- *Comb 2H/3 righe* (in PAL serve una linea di ritardo da 2 righe per il quarto di riga di offset), *2D adattivo* e
  *3D con compensazione di moto* sono progressivamente migliori [gen].
- ntsc-rs offre Box, Notch, 1-line comb, 2-line comb, 2D e 2D adattivo; il default è Notch [code ntsc-rs].

**Errori di tinta NTSC ("Never Twice the Same Color").** In NTSC un errore di fase φ tra burst e crominanza ruota
direttamente la tinta. Le cause sono ghosting, fase differenziale e trasmissione; per questo i televisori hanno il
controllo Tint/Hue [doc Wikipedia NTSC]. La *fase differenziale* (fase che dipende dal livello di luma) e il
*guadagno differenziale* (saturazione che dipende dalla luma) fanno sì che zone chiare e scure dello stesso colore
virino in modo diverso: valori da pochi gradi a circa 10° nei videoregistratori, e meno del 3% di DG nel BVU-870 SP
[doc BVU-870]. Gli altri valori sono [gen].

**PAL: alternanza di fase e barre di Hanover.** Con la linea di ritardo (PAL-D) il decoder media la riga corrente con
la precedente: un errore di fase φ diventa una perdita di saturazione proporzionale a cos φ invece di uno spostamento
di tinta [doc Wikipedia PAL]. Il costo è la risoluzione verticale della crominanza dimezzata [doc Wikipedia PAL]. Senza
linea di ritardo (PAL-S, "poor man's PAL") righe alterne hanno tinta +φ e −φ, e il risultato sono le *barre di
Hanover*: un pettine di righe orizzontali, più visibile in UHF [doc Wikipedia PAL].

**SECAM** [gen]. La crominanza è in FM, con Db e Dr trasmessi su righe alterne (sottoportanti a riposo 4,25 e
4,40625 MHz) e ricostruiti con una linea di ritardo: la risoluzione verticale del colore è sempre dimezzata. Artefatto
tipico è il *"fuoco SECAM"*, cioè striature colorate brillanti e rumore a scintilla sui bordi orizzontali saturi,
perché il rumore FM cresce dove la deviazione è grande. Il filtro "a campana" (cloche) di pre-enfasi in RF dà alone
sui bordi colorati. Nessun errore di tinta. ME-SECAM registra la crominanza FM sul nastro come il colour-under PAL ma
senza rotazione di fase [code vhs-decode, commento in vhs.py].

---

## 2. Formati a nastro

### 2.1 Principio comune dei formati "colour-under"
VHS, Betamax, Video8/Hi8 e U-matic separano Y e C. La **luma** è modulata in frequenza (FM) tra la frequenza del
sincronismo (sync tip) e quella del bianco di picco, con pre-enfasi, e registrata con testine elicoidali. La
**crominanza** è convertita verso il basso (colour-under), a circa 0,6–0,9 MHz, e registrata in AM/QAM sotto lo
spettro FM. L'FM della luma fa anche da bias per la crominanza [doc Sencore TT189; Wikipedia VHS].

Conseguenze da modellare:
1. La **banda della crominanza** è circa 300–500 kHz: circa 40 elementi di colore per riga [doc Wikipedia VHS].
2. La **crominanza ha rumore proprio** (AM e PM), indipendente da quello della luma, a bassa frequenza orizzontale.
3. Per cancellare la diafonia tra tracce adiacenti la crominanza è ruotata di fase riga per riga (VHS: rotazione di
   90°, PS/PI). In riproduzione serve un **comb 1H** sulla crominanza, che media verticalmente il colore di righe
   adiacenti. composite-video-simulator lo commenta così: i deck VHS sommano la crominanza della riga precedente con
   una linea di ritardo [code composite-video-simulator]. Le tracce di luma si separano invece con l'azimuth delle
   testine, ±6° nel VHS [doc Wikipedia VHS].
4. **Timebase**: in riproduzione la velocità testina/nastro non è perfettamente costante, da cui jitter orizzontale,
   skew al cambio testina e flagging in alto. La crominanza ha un circuito AFC/APC che corregge in parte la fase; la
   luma no, salvo TBC [gen].
5. **Cambio testina**: il tamburo ha 2 testine a 180° e ogni testina legge un campo. La commutazione è messa poco
   prima del sincronismo verticale, quindi qualche riga in fondo al quadro attivo, normalmente nascosta
   nell'overscan [gen]. composite-video-simulator la mette a 4,5 righe NTSC sopra il vsync, con un piccolo errore di
   fase [code].

### 2.2 Tabella dei formati analogici

Frequenze FM in MHz (sync tip → bianco di picco). CU = colour-under. TVL = linee TV orizzontali.

| Formato | FM luma NTSC | FM luma PAL | Deviazione | CU NTSC / PAL | Luma | Note e fonti |
|---|---|---|---|---|---|---|
| **VHS** | 3,4 → 4,4 [doc Sencore] | 3,8 → 4,8 [code vhs-decode] | 1,0 MHz | 629 kHz (40 fH) [doc Sencore] / 626,95 kHz (40 fH + 1953 Hz) [code vhs-decode] | ~3 MHz, 240 TVL (HQ 250) [doc Wikipedia VHS]; LP ~230 TVL [doc] | crominanza ~300 kHz in banda base [doc Wikipedia VHS]; composite-video-simulator stima 400 kHz [code] |
| **VHS-C** | come VHS | come VHS | 1,0 | come VHS | come VHS | stessa codifica, cassetta compatta [doc Sencore] |
| **S-VHS / S-VHS-C** | 5,4 → 7,0 [doc Sencore] | 5,4 → 7,0 [code vhs-decode] | 1,6 | 629 kHz (come VHS) | >5 MHz, ~400–420 TVL [doc Wikipedia VHS; Sencore 400] | crominanza come VHS [gen]; white clip al 210% del sync tip (~8,76 MHz) [code vhs-decode]; S/N ~45 dB [doc videouniversity] |
| **Betamax** (Beta II/III) | 3,6 → 4,8; Beta I deviazione 1,3 [doc Sencore] | sync 3,8 → bianco 5,2 [code vhs-decode, da Television 1983] | 1,2 | 688 kHz (43,75 fH) [doc Sencore; code] / PAL: frequenze diverse sulle tracce A/B per avere 90° di offset [code vhs-decode] | 240 TVL [doc Sencore] | |
| **Super Beta** | 4,4 → 5,6 [doc Sencore] | – | 1,2 | 688 kHz | Sencore indica 400 TVL. Il valore sembra alto: si citano spesso ~290 TVL [gen] | da verificare |
| **Video8** | 4,2 → 5,4 [doc Sencore; code] | 4,2 → 5,4 | 1,2 | 743,4 kHz (47,25 fH) / 732,4 kHz (46,875 fH) [code vhs-decode] | 272 TVL [doc Sencore] | |
| **Hi8** | 5,7 → 7,7 [doc Sencore; code] | 5,7 → 7,7 | 2,0 | come Video8 | 432 TVL [doc Sencore], in genere ~400 | white clip 220% (~10,1 MHz?) [code, con dubbio dell'autore] |
| **U-matic low band** | 3,8 → 5,4 [doc Sencore] | 3,8 → 5,4 [code vhs-decode] | 1,6 | 688 kHz / 685,5 kHz [code] | 250 TVL [doc Sencore]; 260 TVL [doc BVU-870] | de-enfasi τ ≈ 600 ns [code] |
| **U-matic high band (BVU, PAL)** | – | 4,8 → 6,4 [code vhs-decode] | 1,6 | 923,8 kHz (43,875 fH) [code] | ~300 TVL [gen] | τ ≈ 800 ns [code] |
| **U-matic SP** | 5,0 → 6,6 [doc Sencore] | 5,6 → 7,2 [code vhs-decode] | 1,6 | 688 kHz / 923,8 kHz (PAL) | 330 TVL [doc Sencore]; 300 TVL PAL [doc BVU-870] | **BVU-870 SP: S/N Y > 49 dB, colore AM > 46 dB, PM > 41 dB, K 2T < 3%, DG < 3%, ritardo Y/C < 25 ns** [doc] |
| **Betacam** (ossido) | Y 4,4 → 6,4; C 4,0 → 5,7 [doc palsite] | uguale | 2,0 | nessun CU: componenti R-Y/B-Y compressi in tempo (CTDM) su una traccia dedicata [doc Wikipedia; palsite] | 300 TVL luma, 120 TVL crominanza [doc Wikipedia Betacam] | velocità nastro 10,15 cm/s [doc]; S/N > 46 dB [doc palsite] |
| **Betacam SP** | Y 6,8 → 8,8; C 5,6 → 7,3 [doc palsite] | uguale | 2,0 | CTDM | 340 TVL [doc Wikipedia]; ~4,5 MHz [doc videouniversity] | S/N Y > 51 dB, C > 53 dB [doc videouniversity]; crominanza ~1,5 MHz [doc videouniversity, attribuzione incerta: tabella confusa nella fonte]; niente cross-colour né cross-luminance perché le tracce sono separate [doc palsite] |
| **MII** (Panasonic) | ? | ? | ? | componente, CTDM | 5,0 MHz [doc videouniversity] | S/N Y > 47, C > 50 dB [doc videouniversity]; frequenze FM non verificate |
| **1" Type C** | 7,06 → 10,0 [code vhs-decode] | 7,16 → 8,90 [code vhs-decode] | ~2,9 / 1,74 | nessuno: composito diretto in FM | piena banda broadcast (4,2 / 5,5 MHz) [gen]; "EBU C-format 5,5 MHz, 43 dB" [doc videouniversity] | non segmentato, un campo per traccia; serve un TBC; de-enfasi τ 240 ns (NTSC) e 180 ns (PAL) [code] |
| **2" Quad** | high band ≈ 7,06 → 10,0; low band ≈ 5,0 → 6,8 [gen] | high band ≈ 7,16 → 8,9 [gen] | | composito diretto | broadcast piena [gen] | 4 testine trasversali, ~16 righe per passata NTSC, ~20 in PAL [gen]: vedi artefatti |

**SNR tipici dei formati consumer [gen].** Valori pesati da scheda tecnica: VHS SP ≈ 43–45 dB (luma); crominanza AM
≈ 40–43 dB; crominanza PM ≈ 35–40 dB; LP/EP da 2 a 5 dB peggio. Video8 ≈ 43–45, Hi8 ≈ 45–47, S-VHS ≈ 45–47 (l'unico
verificato è S-VHS 45 dB [doc]). I valori pesati sono di solito 6–10 dB migliori del rumore non pesato che si vede
davvero [gen]. Per la simulazione conviene partire dai valori non pesati, quindi VHS SP ≈ 38–40 dB.

### 2.3 Artefatti e guasti per famiglia

**VHS / S-VHS / VHS-C / Betamax / Video8 / Hi8 (consumer colour-under)** [gen salvo dove indicato]
- Luma morbida (240–270 TVL), con ringing e alone dovuti allo *sharpening* di riproduzione. composite-video-simulator
  aggiunge sempre uno sharpening in uscita e commenta che i deck VHS tendono ad accentuare la nitidezza [code].
- Crominanza: "sbavatura" orizzontale (~40 elementi per riga), ritardo Y/C (il colore slitta a destra), media
  verticale tra righe (comb 1H), rumore di crominanza a macchie orizzontali, perdita di colore su righe isolate
  (*chroma loss*, il killer del colore scatta su un dropout) [code: chroma_loss in ntsc-rs e c-v-s].
- Rumore luma "a strisce": il rumore FM demodulato ha spettro triangolare (cresce con la frequenza). Dopo la de-enfasi
  resta un rumore fine a grana orizzontale, correlato lungo la riga e scorrelato tra righe e campi.
- *Comet tails* e striature bianche o nere dopo transizioni forti. La pre-enfasi crea overshoot che supera i limiti di
  clip FM (white clip e dark clip): l'FM perde la portante e la demodulazione dà spike invertiti. vhs-decode prevede
  un "high frequency boost" proprio per ridurre le striature dovute alla perdita di ampiezza attorno alle transizioni
  ripide [code vhs-decode, commento].
- Dropout: particelle di ossido mancanti o sporco. Danno strisce orizzontali bianche (perdita di portante → rumore
  saturato) o nere. Il **DOC** (dropout compensator) le sostituisce con la riga precedente ritardata di 1H, quindi
  segmenti "copiati" dalla riga sopra. Durata tipica 2–30 µs, a volte una riga intera.
- Cambio testina in basso: 3–8 righe spostate orizzontalmente, di solito verso destra, con rumore e un gradino a
  metà riga.
- Errori di tracking: una banda orizzontale di rumore (snow) che scorre verticalmente, con la luma che "strappa".
  Con tracking pessimo compaiono due bande e scatti verticali.
- Skew e flagging: le prime righe in alto si piegano lateralmente (tensione del nastro, TV con AFC orizzontale lenta).
- Instabilità orizzontale e righe ondulate (*edge wave*): ntsc-rs la modella come spostamento di riga con rumore fBm
  [code].
- Pausa e ricerca (pausa senza testine dedicate): barre di rumore fisse, quadro che salta, campo ripetuto e
  movimento a scatti.
- Nastro piegato o stropicciato: bande orizzontali di dropout massiccio, ripetute a ogni giro, con perdita di colore
  e salto di sincronismo. Usura da riavvolgimento e da pausa: dropout concentrati all'inizio del nastro.
- Limitatore del bianco e AGC: il registratore comprime le alte luci (il white clip FM taglia i super-bianchi);
  l'AGC d'ingresso pompa con le scene molto chiare o scure.
- Deriva di saturazione: l'ACC del colore oscilla e la saturazione respira; i nastri vecchi perdono il colore.
- Specifico 8 mm: niente linear audio (AFM/PCM), quindi niente hum; dropout più frequenti su ME/MP sottili.
- Betamax PAL: CU diverso sulle due tracce [code vhs-decode].

**U-matic** [gen]: come VHS ma con più banda. I tipici difetti broadcast sono il ritardo Y/C (BVU-870 < 25 ns [doc]),
il moiré di crominanza, il flagging in alto e i dropout da *shedding*: la sindrome sticky-shed rende il nastro
appiccicoso e produce bande di rumore e intasamento delle testine, con il quadro che sparisce a tratti.

**Betacam / SP** [doc palsite + gen]: componente, quindi niente cross-colour. I difetti tipici sono il ritardo Y/C
dovuto all'espansione CTDM, piccoli spostamenti orizzontali tra crominanza R-Y e B-Y, rumore ridotto, dropout corti
e un "tremolio" a bassa frequenza dovuto al TBC.

**1" Type C / 2" Quad** [gen]
- Type C: composito pieno, quindi dot crawl e cross-colour sono quelli del broadcast. Dropout corti mascherati dal
  DOC; il cambio testina avviene nel vertical blanking, ma un salto di righe nella zona del sincronismo produce un
  piccolo flagging in alto.
- Quad: **banding a segmenti**. Ognuna delle 4 testine scrive circa 16 righe (NTSC) di un campo, quindi le differenze
  tra testine (guadagno, risposta in frequenza, fase) producono bande orizzontali di circa 16 righe con luminosità,
  nitidezza o colore leggermente diversi ("venetian blinds"). Gli *errori di quadratura* (angolo fra testine) danno
  gradini orizzontali periodici; lo *scalloping* curva le bande. Con il chroma fuori fase fra testine la tinta cambia
  a bande.

### 2.4 Formati digitali a nastro (primi)

| Formato | Codifica | Campionamento | Bitrate / compressione | Artefatti tipici |
|---|---|---|---|---|
| **D1** | componente non compresso | 4:2:2, 8 bit, 13,5 / 6,75 MHz (luma utile ~5,75 MHz) [gen] | ~172 Mbit/s di video attivo [gen] | quasi trasparente. Restano: filtri anti-alias a 5,75 MHz (ringing di Gibbs lieve), quantizzazione 8 bit con banding nei gradienti, occultamento dei blocchi con errore |
| **D2 / D3** | **composito** digitale 4fsc | 8 bit a 14,3 MHz (NTSC) o 17,7 MHz (PAL) [gen] | non compresso | conserva tutti gli artefatti compositi (dot crawl, cross-colour) ma non aggiunge rumore di generazione |
| **Digital Betacam** | componente DCT | 4:2:2, 10 bit, 720×486 o 576 [doc Wikipedia Betacam] | 90 Mbit/s [doc]; ~2:1 [gen] | praticamente trasparente; sui dettagli fini lievissimo mosquito |
| **Betacam SX** | MPEG-2 4:2:2P@ML, sequenza IBIB | 4:2:2, 8 bit | ~18 Mbit/s, ~10:1 [doc Wikipedia Betacam] | blocchi e mosquito su movimento rapido |
| **DV / MiniDV** | DCT intra, 8×8 | 13,5 MHz, 720 px (704 utili) [doc]; **4:1:1 in 525/60, 4:2:0 in 625/50** [doc Wikipedia DV] | 25 Mbit/s video [doc]; ~5:1 [gen] | vedi sotto |
| **DVCAM** | come DV | come DV | 25 Mbit/s, traccia da 15 µm [doc] | come DV, meno dropout |
| **DVCPRO (25)** | DV variante | **4:1:1 anche in PAL** [doc] | 25 Mbit/s, traccia da 18 µm (meno dropout) [doc] | crominanza orizzontale a gradini anche in PAL |
| **DVCPRO50** | DV variante | 4:2:2 [doc] | 50 Mbit/s [doc] | quasi trasparente |
| **Digital8** | DV su nastro Hi8 | 4:1:1 (525) / 4:2:0 (625) [doc] | 25 Mbit/s [doc] | come DV |

**Meccanismo DV da simulare** [gen salvo dove indicato]:
1. Sottocampionamento della crominanza. **4:1:1** = 180 campioni di crominanza per riga, cioè blocchi orizzontali di
   4 px a 720. Su 1440 px diventano 8 px e su 2880 px 16 px. Molti decoder fanno l'upsampling a ripetizione, da cui
   gradini netti sui bordi rossi e blu. **4:2:0 PAL DV**: crominanza a metà risoluzione verticale. Sull'interlacciato
   produce il *chroma bug* (colore mescolato tra i campi sui bordi orizzontali).
2. DCT 8×8 con quantizzazione adattiva per segmento. Un macroblocco è fatto di 4 blocchi luma e 2 di crominanza
   (32×8 luma in 4:1:1, 16×16 in 4:2:0); 27 macroblocchi formano un superblocco e 5 macroblocchi rimescolati per
   tutto il quadro formano un segmento a bitrate fisso. Gli artefatti sono mosquito noise attorno ai bordi, leggero
   ammorbidimento dei dettagli fini e blocchi su texture ad alta entropia (fogliame, acqua, rumore).
3. *DCT 2-4-8*: con moto forte il codificatore passa a una DCT 2×4×8 per campo, cioè maggiore perdita verticale sui
   movimenti.
4. Errori di nastro: i blocchi non correggibili sono sostituiti con quelli del quadro precedente ("blocchetti" sparsi
   per il quadro, perché i segmenti sono rimescolati). Con errori gravi il quadro si congela o compaiono macroblocchi
   verdi o grigi. Non ci sono "dropout analogici" a strisce: l'errore digitale è **a blocchi e sparso**.
5. Niente rumore di generazione, ma la ricompressione ripetuta (DV→NLE→DV) ammorbidisce i dettagli fini e accumula
   mosquito.

---

## 3. Implementazioni open source di riferimento (algoritmi e default)

### 3.1 composite-video-simulator (joncampbell123) [code]
Progetto C++ con FFmpeg 3.x, scritto per l'accuratezza e non per la velocità. Il programma principale è
`ffmpeg_ntsc.cpp`.
- Lavora su campi, con una frequenza di campionamento implicita di **4×fsc = 14,318 MHz** per pixel: tutti i filtri
  sono `setFilter(315000000*4/88, cutoff)`. Il formato di lavoro è quindi circa 4fsc con circa 760 px attivi.
- **Encode**: lowpass della crominanza in ingresso, modulazione in quadratura su fsc con fase per riga e per campo
  (4 campi di framing del colore), `subcarrier_amplitude = 50`.
- **Preset VHS (`-vhs`)**: head switching attivo, chroma phase noise 4, chroma noise 16, chroma loss 4 (per 100 000
  righe), video noise 4 (base 2).
- **Velocità nastro**:

| Velocità | luma_cut | chroma_cut | chroma_delay (campioni a 4fsc) | phase noise / chroma noise / chroma loss / video noise |
|---|---|---|---|---|
| SP | 2,4 MHz ("3,0 MHz × 80%") | 320 kHz ("400 kHz × 80%") | 9 (≈ 630 ns) | 4 / 16 / 4 / 4 |
| LP | 1,9 MHz | 300 kHz | 12 (≈ 840 ns) | 5 / 19 / 6 / 5 |
| EP | 1,4 MHz | 280 kHz | 14 (≈ 980 ns) | 6 / 22 / 8 / 6 |

- **Filtro luma VHS**: 3 lowpass del primo ordine in cascata a `luma_cut`, più `+1,6 × highpass(luma_cut)`, che
  simula la pre-enfasi residua e dà overshoot.
- **Filtro crominanza**: 3 lowpass in cascata a `chroma_cut`. Il risultato è scritto spostato di `chroma_delay`
  campioni: crominanza e luma non sono più allineate.
- **Comb verticale** `vhs_chroma_vert_blend = true`: media (riga precedente + corrente)/2 per U e V, come la linea di
  ritardo VHS.
- **Sharpening di uscita**: `Y = s + (s − lp³(s, 4·luma_cut)) · vhs_out_sharpen · 2`.
- **Cambio testina**: punto a `1 − (4,5+0,01)/262,5` del campo, rumore di fase 1/500 di riga.
- **Ricombinazione**: se non c'è uscita S-Video, la crominanza è rimodulata nella luma e poi separata di nuovo, cioè
  passa per l'uscita composita del VCR.
- **Preset CATV** (pre-enfasi del composito): scala 7/15/25/40 con taglio fsc, fsc, 2·fsc, 4·fsc.
- Simula anche l'audio lineare (SP 100 Hz–10 kHz, S/N 42 dB; il ronzio dei sincronismi a −42 dBFS) e l'audio Hi-Fi.

### 3.2 ntsc-rs (valadaptive) [code]
Porting Rust di ntscQT, che a sua volta è porting Python di composite-video-simulator. L'autore dichiara che non è un
porting esatto [doc README]. Ha già un plugin OpenFX e uno After Effects. Campionamento implicito anche qui a 4fsc,
con `horizontal_scale` per altre risoluzioni.

**Ordine della pipeline** (`apply_effect_to_yiq_field`):
1. filtro luma in ingresso (default *Notch*);
2. lowpass della crominanza in ingresso (*Full*: I 1,3 MHz, Q 0,6 MHz; *Light*: 2,6 MHz);
3. modulazione della crominanza nella luma (composito);
4. *composite sharpening* (pre-enfasi, highpass a fsc/2, scala 1,0);
5. rumore composito (fBm: frequenza 0,5, intensità 0,05, dettaglio 1);
6. snow (intensità 0,00025, anisotropia 0,5);
7. head switching (altezza 8 righe, offset 3, spostamento orizzontale 72 px a scala 1; il profilo dello spostamento
   cresce con potenza 1,5 verso il basso; *mid-line* posizione 0,95, jitter 0,03, con un transiente di 16 px che
   decade al cubo);
8. tracking noise (altezza 12 righe, wave 15, snow 0,025, anisotropia 0,25, noise 0,25; intensità che cresce verso il
   basso, linearmente per lo spostamento e al quadrato per neve e rumore; spostamento con rumore simplex a frequenza
   0,5 righe);
9. demodulazione (default *Notch*);
10. luma smear 0,5;
11. ringing (notch con frequenza 0,45, potenza 4, intensità 4);
12. rumore luma 0,01 e rumore di crominanza (fBm: frequenza 0,05, intensità 0,1, dettaglio 2, cioè macchie larghe);
13. errore di fase della crominanza 0 e rumore di fase 0,001;
14. chroma delay orizzontale e verticale 0;
15. **VHS**: edge wave (intensità 0,5, velocità 4, frequenza 0,05, dettaglio 2, applicato a Y, I e Q); filtri del
    nastro, con default **LP**:

| Velocità | luma_cut | chroma_cut | chroma_delay |
|---|---|---|---|
| SP | 2,4 MHz | 320 kHz | 4 campioni |
| LP | 1,9 MHz | 300 kHz | 5 |
| EP | 1,4 MHz | 280 kHz | 6 |

    Dopo i filtri: `+1,6 × highpass`, chroma loss 0,000025 per riga (distribuzione geometrica), sharpen (intensità
    0,25, frequenza 1,0; filtro a `luma_cut × freq`, guadagno `−intensity × 2 × freq`). L'autore nota nel sorgente
    che "questo effetto è molto più spalmato del vero VHS" e che serve un filtro migliore;
16. chroma vertical blend (default **off**);
17. lowpass della crominanza in uscita (*Full*).

Altri default: campo *InterleavedUpper* (i campi sono elaborati separatamente con numeri di quadro 2n e 2n+1), filtro
*Butterworth* (alternativa *ConstantK*). Il ritardo della crominanza è stato quasi dimezzato rispetto a
composite-video-simulator (4 contro 9 campioni). Non ho verificato se i blocchi VHS e head switching siano attivi di
default.

### 3.3 ntscQT (JargeZ) [doc README ntsc-rs; dettagli gen]
GUI PyQt su `zhuker/ntsc` (porting Python di composite-video-simulator). Ha gli stessi parametri e ha introdotto edge
wave, ringing alternativo e preset salvabili. È lento (Python + OpenCV) ed è il precursore diretto di ntsc-rs.

### 3.4 Shader RetroArch e affini [gen, non verificato in questa sessione]
- **blargg nes_ntsc / snes_ntsc**: kernel precalcolati che simulano encode e decode NTSC per pixel sorgente
  (2 o 3 fasi). Parametri da −1 a +1: hue, saturation, contrast, brightness, sharpness, gamma, *resolution*,
  *artifacts*, *fringing*, *bleed*, *merge_fields*. Preset: composite (tutto a 0), S-Video (sharpness 0,2,
  resolution 0,2, artifacts −1, fringing −1), RGB (sharpness 0,2, resolution 0,7, artifacts/fringing/bleed −1),
  monochrome.
- **Shader NTSC "maister"** (2 passi): modula a 2 o 3 fasi su una texture 4× orizzontale, poi decode con FIR; varianti
  composite e S-Video.
- **guest-advanced-ntsc**, **ntsc-adaptive**: decoder adattivi con controllo del ricampionamento, qui senza nastro.
- Shader "VHS" generici (Godot, Shadertoy): di solito blur gaussiano della crominanza, offset RGB, rumore e bande
  scorrevoli. Sono esteticamente utili ma non modellano la catena.

### 3.5 vhs-decode / ld-decode [code]
Fa l'opposto, cioè decodifica l'RF grezzo dei nastri, ed è la fonte più ricca di parametri reali: frequenze FM e
colour-under per sistema (tabelle sopra), de-enfasi FM τ ≈ 1,3 µs per VHS, Betamax e Video8, 600 ns per U-matic low
band, 800 ns per high band e SP, 240/180 ns per Type C NTSC e PAL. Filtro luma dopo la demodulazione: VHS PAL
3,4 MHz, NTSC 6,6 MHz (con passa-banda RF), Betamax e Video8 3,5 MHz, U-matic 4,0–4,2 MHz, Type C 4,2 e 5,2 MHz.
Molti valori sono marcati "da ritoccare" dagli stessi autori.

---

## 4. Meccanismi da simulare e modello numerico

### 4.1 Griglia di lavoro e conversione in pixel
**Proposta.** Elaborare ogni campo su una griglia "virtuale" a righe native (240/243 righe attive NTSC, 288 PAL per
campo) con campionamento orizzontale proporzionale al tempo (px/µs). Poi ricostruire il quadro interlacciato e
scalarlo all'uscita. In alternativa, lavorare direttamente all'altezza di uscita con kernel scalati, che è più
semplice in OFX ma rende meno credibili gli artefatti per riga.

Conversioni per un 4:3 inscritto (larghezza attiva 1440 a 1080p, 2880 a 2160p) [deriv]:

| Grandezza | 1080 (1440 px attivi) | 2160 (2880 px attivi) | Per 16:9 stirato (1920/3840) |
|---|---|---|---|
| px per µs (NTSC 52,66 µs) | 27,3 | 54,7 | 36,5 / 72,9 |
| px per µs (PAL 52 µs) | 27,7 | 55,4 | 36,9 / 73,8 |
| 100 ns di ritardo | 2,7 px | 5,5 px | 3,6 / 7,3 |
| altezza di una riga sorgente NTSC (480 visibili) | 2,25 px | 4,5 px | – |
| altezza di una riga sorgente PAL (576) | 1,875 px | 3,75 px | – |

**Larghezza del filtro.** Per un passa-basso gaussiano con −3 dB alla frequenza B (MHz):
σ_px = 0,1325 · (px/µs) / B. A 1080 (27,3 px/µs) σ ≈ **3,62 / B** px; a 2160 σ ≈ **7,23 / B** px [deriv]. Se il taglio
è definito a −6 dB il coefficiente è 0,187 invece di 0,1325. Un filtro gaussiano non dà ringing: per avere l'overshoot
tipico servono IIR (Butterworth, come ntsc-rs) o peaking esplicito.

| Banda | σ a 1080 | σ a 2160 | Uso |
|---|---|---|---|
| 5,5 MHz (PAL I, Type C) | 0,66 px | 1,3 px | broadcast PAL |
| 5,0 MHz (S-VHS, MII) | 0,72 | 1,45 | |
| 4,5 MHz (Betacam SP) | 0,80 | 1,6 | |
| 4,2 MHz (NTSC broadcast) | 0,86 | 1,72 | |
| 3,0 MHz (VHS SP reale) | 1,21 | 2,41 | |
| 2,4 MHz (VHS secondo c-v-s) | 1,51 | 3,0 | |
| 1,5 MHz (crominanza Betacam SP) | 2,4 | 4,8 | |
| 1,3 MHz (I, U/V broadcast) | 2,8 | 5,6 | |
| 0,6 MHz (Q in ntsc-rs) | 6,0 | 12,1 | |
| 0,4 MHz (Q, crominanza VHS) | 9,0 | 18,1 | |
| 0,3 MHz (crominanza VHS LP) | 12,1 | 24,1 | |

Per sorgenti a 720 px (risoluzione SD nativa) i valori vanno divisi per 2 (13,5 px/µs).

### 4.2 Catalogo dei meccanismi con parametri

| Meccanismo | Modello proposto | Valori tipici [gen salvo indicazione] | In pixel (1080 / 2160) |
|---|---|---|---|
| Banda della luma | IIR del 2°–4° ordine (Butterworth) o 3 poli in cascata come c-v-s | vedi tabella 4.1 | σ eq. 0,7–1,5 / 1,4–3 px |
| Peaking / edge enhancement | `Y += k·(Y − LP(Y, f_p))`, con f_p ≈ 1–1,5 × taglio luma; più un eventuale peaking a banda (BPF a ~2 MHz) | k = 0,3–1,0 per VHS (c-v-s usa +1,6 × highpass nel registratore e 0,5 in uscita) [code] | alone 2–6 / 4–12 px |
| Banda della crominanza | IIR 3 poli o gaussiana su U/V (I/Q) | VHS 300–500 kHz; Betacam SP ~1,5 MHz | σ 7–12 / 14–24 px (VHS) |
| Ritardo Y/C | spostamento orizzontale della crominanza (frazionario) | broadcast < 25 ns [doc BVU-870]; VHS 100–300 ns (ntsc-rs 280 ns SP; c-v-s 630 ns SP) [code] | 2,7–8 / 5,5–16 px (VHS) |
| Comb verticale della crominanza | `C_n = (C_n + C_{n−1})/2` nel campo | VHS sempre; PAL-D nel decoder | 1 riga di campo = 2 righe di quadro |
| Rumore FM della luma | rumore bianco filtrato: spettro ∝ f (triangolare) poi de-enfasi; scorrelato per riga e campo; ampiezza crescente con il livello del segnale (CNR peggiore a frequenze RF alte, cioè sui bianchi) | VHS non pesato 38–42 dB → σ ≈ 0,8–1,3 % della scala 0–100 IRE (σ = 10^(−SNR/20)) | grana orizzontale di 1–3 / 2–6 px, alta 1 riga |
| Rumore di crominanza | rumore a bassa frequenza orizzontale (fBm o rumore bianco filtrato alla banda della crominanza), con componente AM (saturazione) e PM (tinta) | VHS AM ≈ 40, PM ≈ 35–38 dB; ntsc-rs: frequenza 0,05, intensità 0,1 [code] | macchie lunghe 10–40 / 20–80 px, alte 1–2 righe |
| Speckle / comet tail | spike sopra soglia FM: dove la pre-enfasi del bordo supera il clip bianco o nero, sostituzione con rumore saturato o inversione | soglia ~ 170–210 % della deviazione [code vhs-decode per S-VHS 210 %] | 3–15 / 6–30 px |
| Dropout | processo di Poisson per riga: durata log-normale (mediana 5 µs, coda fino a 1H), livello bianco o nero o rumore; con DOC si copia la riga precedente | nastro buono 0,1–1 per campo; usurato 5–30 per campo | 15–300 / 30–600 px |
| Jitter orizzontale (timebase) | spostamento per riga: componente casuale (σ 5–30 ns) più componente lenta (rumore 1/f, ±100–500 ns) correlata nel campo; la crominanza è corretta in parte dall'APC | VCR senza TBC | σ 0,15–0,8 px casuale; ±3–14 px lento (1080) |
| Skew / flagging in alto | spostamento orizzontale che decresce esponenzialmente dalla prima riga, per circa 5–20 righe | 0,5–3 µs alla prima riga | 14–80 / 27–160 px |
| Cambio testina | ultime N righe del campo spostate orizzontalmente (profilo che cresce verso il basso), gradino a metà riga con transiente, rumore | N = 3–8 righe (c-v-s: 4,5 righe sopra il vsync) [code]; ntsc-rs 8 righe e 72 px [code] | 7–18 / 14–36 px di altezza; spostamento 30–150 / 60–300 px |
| Errore di tracking | banda di rumore alta 5–30 righe che scorre in verticale (velocità ∝ errore di tracking), con spostamento orizzontale ondulato e snow; ai lati della banda, perdita di colore | ntsc-rs 12 righe, wave 15 [code] | 11–70 / 22–135 px di altezza |
| Edge wave / righe ondulate | spostamento per riga con rumore fBm lento nel tempo | ntsc-rs: intensità 0,5, velocità 4, frequenza 0,05 [code] | ±1–4 / 2–8 px |
| Pausa VCR | campo ripetuto, barra di rumore fissa in posizione (dipende dalla fase del tamburo), jitter verticale di ±1 riga | – | – |
| Generazioni | ripetere encode → nastro → decode N volte; oppure in forma analitica: σ_filtro·√N, rumore +10·log10(N) dB (rumore uguale per generazione), ritardo Y/C × N, peaking cumulativo (alone che si allarga), saturazione −3/−5 % per generazione | dopo 3 generazioni VHS la luma è ≈ 180–200 TVL | – |
| Interlacciamento | elaborare i campi separatamente con tempo t+½ quadro; combing sul movimento; ordine dei campi (NTSC DV: bottom first; broadcast analogico: top first nella maggior parte dei sistemi) | – | 1 riga di campo = 2,25 / 4,5 px (NTSC) |
| Overscan / blanking | maschera nera dei bordi, mezze righe di inizio e fine campo, righe VBI (opzionale: linea 21 / teletext come pattern) | overscan 5–10 % per lato | – |
| White limiter / AGC | soft-clip dei bianchi oltre circa 100–105 IRE; AGC: guadagno che segue la luminanza media con costante di tempo 0,3–2 s e un po' di overshoot | – | – |
| Errore di tinta e saturazione | NTSC: rotazione UV di φ più φ·(dipendenza dalla luma) per la fase differenziale; PAL-D: saturazione × cos φ; PAL-S: ±φ su righe alterne (Hanover) | φ 0–10°; DG 0–5 % | – |
| Dot crawl / cross-colour | vera modulazione su fsc con la fase giusta per riga e quadro, poi decode notch o comb | fsc a 720 px ≈ 4 px per ciclo (NTSC) | ciclo di 7,6 / 15,3 px a 1080 / 2160 (NTSC); 6,2 / 12,5 (PAL) [deriv] |
| CRT (opzionale) | scanline (profilo gaussiano per riga, larghezza dipendente dalla luminanza), maschera (aperture/shadow/slot), bloom (blur sulle alte luci), curvatura, persistenza dei fosfori | – | – |

**Nota sulla forma del rumore FM** [gen]. Dopo la demodulazione FM la densità spettrale del rumore cresce
linearmente con la frequenza (rumore "triangolare"). La pre-enfasi in registrazione e la de-enfasi in riproduzione
(τ ≈ 1,3 µs VHS [code]) la riportano quasi piatta fino a circa 1 MHz, poi resta accentuata in alto fino al taglio
luma. Visivamente diventa una grana fine allungata in orizzontale, perché ogni riga è indipendente, e non segue
1/f in 2D. Il rumore lento 1/f appartiene invece alla timebase, al pompaggio dell'AGC e alla deriva della
saturazione. Perché il rumore cresca nelle alte luci: le frequenze FM più alte (bianco) hanno meno uscita dalla
testina, quindi un CNR peggiore. Nella percezione però il rumore si nota di più nei mezzitoni e nelle ombre, per
via della gamma. Propongo una dipendenza lieve, con fattore 1 + 0,3·Y'.

---

## 5. Proposta per il plugin

### 5.1 Controlli semplici (pannello principale)

| Controllo | Tipo e intervallo | Default | Cosa muove |
|---|---|---|---|
| **Formato** | scelta: Broadcast, VHS SP, VHS LP, VHS EP, S-VHS, Betamax, Video8, Hi8, U-matic, U-matic SP, Betacam SP, 1" Type C, Quad, D2, DV, DVCPRO, Digital Betacam | VHS SP | preset completo della tabella 5.2 |
| **Sistema** | NTSC / PAL / SECAM | PAL | fsc, righe, fase, decoder di colore (Hanover o tinta), setup 7,5 IRE |
| **Collegamento** | Composito / S-Video / Componente / RF | Composito | se c'è dot crawl e cross-colour; RF aggiunge rumore e ghosting |
| **Generazioni** | 1–8 (float, interpolato) | 1 | ripetizioni della catena (vedi 4.2) |
| **Usura** | 0–100 % | 15 % | frequenza e durata dei dropout, chroma loss, rumore +0…+8 dB, deriva di saturazione, flagging |
| **Tracking** | −100…+100 (0 = ottimo) | 0 | \|x\| > 10: compare la banda; altezza ∝ \|x\|; il segno dà la direzione di scorrimento |
| **Testine** | 0–100 % | 30 % | altezza e spostamento del cambio testina, transiente a metà riga, jitter verticale di campo |
| **Instabilità** | 0–100 % | 25 % | jitter per riga, edge wave, skew in alto; *TBC* (on/off) lo annulla quasi del tutto |
| **Rumore** | 0–200 % del nominale del formato | 100 % | rumore FM della luma, snow |
| **Colore sbavato** | 0–200 % | 100 % | banda della crominanza (inversa), ritardo Y/C, rumore di crominanza |
| **Nitidezza** | −100…+100 | 0 = valore nominale del formato | guadagno del peaking: +100 = alone forte, −100 = morbido senza ringing |
| **Saturazione / Tinta** | 0–200 % / ±30° | 100 % / 0 | errore statico; più "deriva" con l'Usura |
| **Campi** | Progressivo / Interlacciato TFF / BFF | dal Formato | elaborazione per campo |
| **Seme** | intero | 0 | determinismo per quadro |
| **Display** (gruppo chiuso) | Off / TV CRT / Monitor broadcast | Off | scanline, maschera, bloom, curvatura |

Gruppo **Avanzate** (chiuso), per chi vuole i parametri fisici: banda luma in MHz, banda crominanza in kHz, ritardo
Y/C in ns, SNR luma e crominanza in dB, decoder (Notch/Comb 1H/Comb 2D/3D), righe di cambio testina, dropout per
campo, DOC on/off, white clip in IRE, AGC on/off.

Regola: i controlli semplici sono **moltiplicatori relativi** rispetto al preset del Formato, quindi lo stesso 100 %
dà "un VHS tipico" su qualsiasi formato. Le Avanzate, se toccate, sovrascrivono il valore assoluto.

### 5.2 Mappatura Formato → parametri fisici (nominali al 100 %)

Valori proposti dalla tabella 2.2 e da [gen]. Sono **da validare** confrontandoli con catture reali.

| Formato | Banda Y (MHz) | Peaking k | Banda C (kHz) | Ritardo Y/C (ns) | SNR Y non pesato (dB) | SNR C AM/PM (dB) | Comb C 1H | Jitter casuale (ns rms) / lento (ns) | Cambio testina (righe) | Dropout per campo (usura 0→100 %) |
|---|---|---|---|---|---|---|---|---|---|---|
| Broadcast NTSC/PAL | 4,2 / 5,0–5,5 | 0,1 | 1300 (Q 400) | 0–25 | 48–52 | 50 / 48 | solo decoder | 0 / 0 | 0 | 0 |
| VHS SP | 3,0 | 0,6 | 400 | 150 | 40 | 40 / 36 | sì | 20 / 250 | 5 | 0,3 → 25 |
| VHS LP | 2,6 | 0,7 | 350 | 200 | 38 | 38 / 34 | sì | 25 / 300 | 5 | 0,5 → 30 |
| VHS EP | 2,2 | 0,8 | 300 | 250 | 36 | 36 / 32 | sì | 30 / 350 | 5 | 0,8 → 40 |
| S-VHS | 5,0 | 0,5 | 400 | 120 | 42 | 40 / 36 | sì | 20 / 250 | 5 | 0,5 → 30 |
| Betamax | 3,0 | 0,5 | 400 | 150 | 41 | 40 / 36 | sì | 20 / 250 | 5 | 0,3 → 25 |
| Video8 | 3,4 | 0,5 | 400 | 150 | 40 | 39 / 35 | sì | 20 / 200 | 5 | 0,6 → 35 |
| Hi8 | 5,0 | 0,5 | 400 | 120 | 42 | 40 / 36 | sì | 20 / 200 | 5 | 0,8 → 40 |
| U-matic LB | 3,3 | 0,4 | 500 | 80 | 43 | 43 / 38 | sì | 10 / 100* | 4 | 0,2 → 20 |
| U-matic SP | 4,0 | 0,3 | 600 | 25 [doc] | 45 (49 pesato [doc]) | 43 / 38 (46 / 41 pesati [doc]) | sì | 5 / 30* | 4 | 0,2 → 20 |
| Betacam SP | 4,5 [doc] | 0,2 | 1500 [doc?] | 20 | 47 (>51 pesato [doc]) | 49 | no | 2 / 10* | 0 (VBI) | 0,1 → 10 |
| 1" Type C | 4,2 / 5,5 | 0,2 | 1300 (composito) | 10 | 43 [doc] | 43 | no | 2 / 10* | 0 (VBI) | 0,1 → 10 |
| Quad | 4,2 / 5,5 | 0,2 | 1300 | 10 | 40 | 40 | no | 2 / 10* + banding | 0 | 0,2 → 15 |
| D2 | 4,2 / 5,5 (composito 4fsc) | 0 | 1300 | 0 | ∞ (quantizzazione 8 bit) | – | no | 0 | 0 | errori a blocchi |
| DV / DVCPRO | 5,75 | 0 | 4:1:1 o 4:2:0 | 0 | ∞ + DCT | – | no | 0 | 0 | errori a blocchi |
| Digital Betacam | 5,75 | 0 | 4:2:2 | 0 | ∞ | – | no | 0 | 0 | quasi nessuno |

\* Formati professionali: si suppone un TBC attivo; con TBC spento si usano i valori del consumer.

Formule dei controlli semplici (proposta):
- **Rumore R** (0–2): SNR_eff = SNR_nom − 20·log10(R) − 8·Usura + 10·log10(Generazioni).
  σ = 10^(−SNR_eff/20) sulla scala 0–1 di Y'.
- **Colore sbavato S** (0–2): B_C = B_C,nom / max(S, 0,1)^0,8; ritardo = ritardo_nom · S; rumore di crominanza
  +6·(S−1) dB.
- **Nitidezza N** (−1…+1): k = k_nom · (1 + N) per N ≥ 0; per N < 0, k = k_nom · (1 + N) e banda luma × (1 + 0,3·N).
- **Testine T** (0–1): righe = round(2 + 8·T); spostamento = 0,5 + 4·T µs; probabilità del transiente a metà riga = T.
- **Instabilità I** (0–1): jitter casuale = J_nom · 2·I; lento = L_nom · 2·I; skew in alto = 2·I µs su 12 righe;
  con TBC tutto × 0,05.
- **Tracking X** (−1…+1): banda visibile se |X| > 0,1; altezza = 4 + 40·|X| righe; velocità di scorrimento =
  0,5·sign(X)·|X| quadri per secondo; snow nella banda ∝ |X|².
- **Usura U** (0–1): frequenza dropout = D0·(D100/D0)^U; durata mediana 3 + 10·U µs; probabilità di chroma loss per
  riga 2·10⁻⁵ + 5·10⁻⁴·U; oscillazione della saturazione ±(2 + 10·U) % a 0,2–1 Hz.

### 5.3 Ordine di elaborazione proposto

1. **Ingresso**: da lineare a Y'CbCr display-referred (Rec.709 o 601), clamp alle gamme legali, setup a 7,5 IRE se
   NTSC-USA.
2. **Rasterizzazione**: ricampionamento orizzontale sulla griglia temporale (px/µs del formato, con oversampling ×2)
   e separazione in campi (righe pari e dispari; il secondo campo prende il tempo t + ½ quadro se il clip è
   progressivo ad alto frame rate).
3. **Encoder** (sorgente broadcast o camera): lowpass di I/Q o U/V a 1,3 / 0,4 MHz.
4. **Collegamento in ingresso al registratore**: se Composito, modulazione su fsc con fase per riga e quadro corretta
   e separazione Y/C del registratore (notch o comb: qui nasce il cross-colour "registrato").
5. **Registrazione**
   - luma: AGC e white limiter → pre-enfasi → clip FM (bianco e nero) con spike → banda luma del formato;
   - crominanza: banda colour-under, ritardo Y/C, ACC.
6. **Nastro**: rumore FM sulla luma (spettro triangolare con de-enfasi), rumore di crominanza AM e PM, maschera dei
   dropout (generata qui e usata anche al passo 7), banda di tracking, errori di timebase (stesso spostamento per Y e
   C; la crominanza riceve inoltre un errore di fase proporzionale al jitter residuo).
7. **Riproduzione**: comb 1H sulla crominanza (formati colour-under), DOC (riga precedente nelle zone di dropout),
   chroma loss e killer del colore, peaking e sharpening, cambio testina, skew in alto, edge wave, AGC.
8. **Uscita del VCR** (Composito / S-Video / Componente): eventuale rimodulazione su fsc.
9. **Generazioni**: ripetere 4–8 per le generazioni intere, oppure interpolare l'ultima con la forma analitica.
10. **Ricevitore / decoder**: separazione Y/C (notch, comb o 3D), demodulazione, errori di tinta (NTSC) o
    Hanover / saturazione (PAL), SECAM se scelto, ritardo di luma del TV.
11. **Ricostruzione**: campi → quadro (o "bob" per output a 50/60p), ricampionamento all'uscita, conversione in RGB e
    poi in lineare se l'host lo chiede.
12. **Display CRT** (opzionale): scanline, maschera, bloom, curvatura, persistenza.
13. **Formati digitali**: dopo il passo 3 si salta il nastro analogico e si fa il sottocampionamento della crominanza
    (4:1:1, 4:2:0, 4:2:2) → DCT 8×8 con quantizzazione dal preset (mosquito) → errori a blocchi → upsampling della
    crominanza (con nearest per ottenere il gradino tipico).

### 5.4 Note di implementazione
- **Determinismo**: il seme si combina con numero di quadro, campo e riga (come ntsc-rs con SplitMix64). Così il
  rendering di un singolo quadro in OFX è riproducibile e indipendente dall'ordine.
- **Indipendenza dalla risoluzione**: tutti i parametri spaziali sono espressi in µs e in righe sorgente, poi
  convertiti con la tabella 4.1. Un'anteprima a ½ o ¼ di risoluzione deve dare lo stesso aspetto.
- **Filtri**: IIR ricorsivi lungo la riga (economici, con fase non lineare: è lì il "sapore" analogico). Per la
  crominanza molto stretta a 2160p, IIR o box multipassata evitano kernel di oltre 100 tap.
- **Verifica**: confrontare con catture reali (barre colore, multiburst, zone plate) i valori MHz → TVL, e usare un
  multiburst sintetico come test di regressione (golden).

---

## 6. Cosa è verificato e cosa no

- **Verificato da fonte** [doc]: valori NTSC e PAL di base; frequenze FM e colour-under della tabella Sencore (VHS,
  S-VHS, Beta, Super Beta, U-matic, U-matic SP, 8 mm, Hi8); Betacam e SP (palsite, Wikipedia); BVU-870; DV, DVCAM,
  DVCPRO, Digital8 (Wikipedia); S/N di Betacam SP, MII e S-VHS (videouniversity, tabella in parte confusa).
- **Letto nel codice** [code]: tutti i default di ntsc-rs e composite-video-simulator; frequenze PAL e de-enfasi da
  vhs-decode.
- **Non verificato** [gen]: Quad, MII FM, D1/D2/D3, SECAM, preset blargg/RetroArch, SNR consumer, valori di jitter,
  dropout e head switching reali. La tabella 5.2 e le formule 5.2 sono **proposte**, da tarare.

## 7. Fonti

- Sencore Tech Tip TT189, "Comparison of VCR Formats": https://xdevs.com/doc/Sencore/Sencore_Tech_Tips/TT189p%20-%20Comparison%20Of%20VCR%20Formats.pdf
- Sencore Tech Tip TT135, VCR FM modulator frequencies: https://xdevs.com/doc/Sencore/Sencore_Tech_Tips/TT135p%20-%20How%20To%20Set%20VCR%20FM%20Modulator%20Frequencies.pdf
- Wikipedia, VHS: https://en.wikipedia.org/wiki/VHS
- Wikipedia, Betacam: https://en.wikipedia.org/wiki/Betacam
- Wikipedia, DV: https://en.wikipedia.org/wiki/DV_(video_format)
- Wikipedia, NTSC: https://en.wikipedia.org/wiki/NTSC
- Wikipedia, PAL: https://en.wikipedia.org/wiki/PAL
- Wikipedia, Hi8 / 8 mm: https://en.wikipedia.org/wiki/Hi8
- Betacam format (palsite): https://betacam.palsite.com/format.html
- Sony BVU-870 spec (palsite): https://chat.palsite.com/umatic/bvu870spec.shtml
- M. Iisakkila, "Video recording formats": https://www.videouniversity.com/articles/video-recording-formats/
- ntsc-rs (codice: crates/ntscrs/src/ntsc.rs, settings/standard.rs): https://github.com/valadaptive/ntsc-rs
- composite-video-simulator (ffmpeg_ntsc.cpp): https://github.com/joncampbell123/composite-video-simulator
- ntscQT: https://github.com/JargeZ/ntscqt · ntsc (Python): https://github.com/zhuker/ntsc
- vhs-decode (vhsdecode/format_defs/*.py) e wiki: https://github.com/oyvindln/vhs-decode · https://github.com/oyvindln/vhs-decode/wiki/Technical-Breakdowns
- CVBS presets (riferimento sui parametri dei sistemi): https://simoninns.github.io/cvbs-file-format-specification/video-standard-presets/
- ITU-R BT.470-6 (sistemi TV convenzionali, da consultare): https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.470-6-199811-S!!PDF-E.pdf
- SMPTE ST 230 / ST 251 / RP 84/86/87 (specifiche dei formati, da consultare): https://pub.smpte.org/
- Wiki degli emulatori sui filtri NTSC (non letta, budget esaurito): https://emulation.gametechwiki.com/index.php/NTSC_filters
- Forum libretro "NTSC shaders redux" (non letto): https://forums.libretro.com/t/ntsc-shaders-redux/39831

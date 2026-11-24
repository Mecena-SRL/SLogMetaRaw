# Pellicole e processi di sviluppo: dati fisici per modelli parametrici

Ricerca per un plugin OFX originale di emulazione pellicola. Scopo: raccogliere dati tecnici **pubblicati**
(curve, densità, gamma, sensibilità spettrali, coloranti, grana, risoluzione, alonatura) per costruire
modelli **parametrici nostri**. Nessuna LUT, profilo o dato proprietario va copiato: i datasheet servono
come ordini di grandezza e come verifica di plausibilità.

**Legenda delle fonti**
- **[doc]** = dato letto in un documento tecnico o articolo citato (la fonte è indicata tra parentesi; elenco URL in fondo).
- **[gen]** = conoscenza generale di fotochimica/sensitometria, ragionevole ma **non verificata** in questa ricerca:
  da trattare come ipotesi di partenza da calibrare.
- **[prop]** = proposta progettuale nostra (mappature, archetipi), non un fatto.

**Avvertenza importante.** I datasheet moderni Kodak e Fuji per pellicole cinema (Vision3, 2383, Eterna)
riportano curve caratteristiche, granularità RMS, MTF e sensibilità spettrali **solo come grafici**, senza
valori numerici in tabella [doc] (Kodak 7219 TECHNICAL DATA; Kodak 2383 data sheet). I numeri di gamma per
quelle pellicole che compaiono qui sotto sono quindi **[gen]**: letture tipiche di letteratura e di
laboratorio, da verificare digitalizzando le curve dei PDF (operazione lecita a scopo di misura, ma i
nostri modelli devono restare parametrici).

---

## 1. Fondamenti fisici da modellare

### 1.1 Catena negativo → positivo e gamma di sistema

| Elemento | Gamma tipica | Note | Fonte |
|---|---|---|---|
| Negativo colore cinema (ECN-2) | 0,55–0,65 | Vision3 ~0,60 sul tratto rettilineo, curve per canale quasi parallele | [gen] |
| Negativo b/n cinema Double-X 5222 | 0,65–0,70 (control gamma) | EI 250 D / 200 T sviluppato a gamma 0,65 | [doc] Kodak Double-X TD |
| Negativo b/n ORWO UN54 | gradiente medio 0,65 | ISO 100 a G=0,65 in D-96 | [doc] ORWO UN54 datasheet |
| Negativo b/n still (Tri-X) | contrast index 0,56 | tempi di partenza Kodak tarati su CI 0,56 | [doc] Kodak F-4017 |
| Intermedi (IP/IN, es. 5254, 2242) | ~1,0 | duplicazione "unitaria"; aim Fuji ETERNA-CI = Dmin+1,0 ±0,1 per R,G,B | [gen] / [doc] Fuji manual |
| Positivo b/n 2302 | 2,4–2,6 (control gamma, Status A) | sviluppo D-97, gamma come parametro di controllo | [doc] Kodak 2302 TI |
| Positivo colore 2383/3513 | ~2,6–3,0 a metà scala | forte spalla in alto e piede lungo nelle ombre | [gen] |
| Sistema proiettato (neg × print) | ~1,5–1,8 | > 1 per compensare flare di sala e visione al buio | [gen] |

Modello minimo [prop]: `logE scena → D_neg = curva_neg(logE)` (gamma ~0,6, piede e spalla) →
`logE_print = −D_neg + offset_printer_lights` → `D_print = curva_print(logE_print)` (gamma ~2,7) →
`luminanza = 10^(−D_print)`. Questa doppia curva produce da sola la caratteristica "compressione delle alte
luci morbida + neri profondi" del cinema: la spalla del negativo è lunghissima (Vision3 ha **~2 stop di
latitudine in più nelle alte luci**: Kodak ha esteso la scala sensitometrica pubblicata da 0–4 a 0–5 logE
[doc] Kodak 7219 TD), mentre la forma finale è dominata dal **piede del positivo** (alte luci) e dalla
**spalla del positivo** (neri).

Riferimenti utili:
- LAD (Laboratory Aim Density): la patch LAD corrisponde a **densità visiva 1,0 sul positivo** [doc] Kodak 7219 TD.
- Aim Fuji per positivo F-CP/ETERNA-CP (Status A R/G/B): **1,10 / 1,05 / 1,05** [doc] Fuji MP film manual.
- Densità di riferimento di un negativo colore still esposto correttamente: cartoncino grigio 18% → **0,77–0,87**
  sopra base; fronte carnagione chiara 1,08–1,18; carnagione scura 0,93–1,03 [doc] Kodak E-4050 Portra 400.
- Dmin negativo colore cinema: la maschera arancio porta la base a densità Status M circa R 0,2 / G 0,6 / B 0,8 [gen];
  Kodak traccia le curve con riferimento "0,2 > D-min" [doc] Kodak 7219 TD.
- Positivo colore: Dmin ~0,05–0,10, Dmax ~4,0 (Status A) [gen].

### 1.2 Struttura a strati, ordine e conseguenze

Ordine tipico dall'alto (lato obiettivo) verso la base [gen]: strato protettivo → strato sensibile al blu
(forma giallo) → filtro giallo (argento colloidale Carey Lea, rimosso in sbianca) → strato sensibile al verde
(forma magenta) → strato sensibile al rosso (forma ciano) → base → anti-alone (remjet nel cinema, o strato
colorato/base grigia nelle still). Ogni strato è spesso diviso in sottostrati veloce/lento per estendere la
latitudine.

Conseguenze modellabili:
- **Alonatura rossa**: la luce che attraversa tutto e si riflette dalla base rientra per primo nello strato
  rosso-sensibile, quindi il bagliore è rosso/arancio [doc] Wikipedia "Anti-halation backing".
- **Redscale** (pellicola caricata al contrario): la luce colpisce prima lo strato rosso, poi la maschera e il
  filtro giallo bloccano il blu → immagine rosso-arancio-giallo, perdita 1–3 stop [gen].
- **Nitidezza per strato**: lo strato rosso, più profondo, riceve luce più diffusa → MTF ciano < magenta < giallo
  in molte pellicole [gen].

### 1.3 Assorbimenti indesiderati, maschera arancio e accoppiatori colorati

I coloranti reali non sono filtri ideali [gen]:
- ciano: assorbe soprattutto il rosso (picco ~650–690 nm) ma anche verde e un po' di blu;
- magenta: picco ~540–560 nm, ma assorbe in modo apprezzabile nel blu;
- giallo: picco ~440–460 nm, è il più "pulito".

Senza correzione, ogni colorante aggiunge densità nei canali sbagliati → desaturazione e virate. Nei negativi
colore si usano **accoppiatori colorati (masking couplers)**: l'accoppiatore magenta non reagito è giallo,
quello ciano è rosa-arancio; dove si forma colorante il colore dell'accoppiatore sparisce, dove non si forma
resta. Il risultato è una densità di correzione inversamente proporzionale all'immagine: la **maschera arancio**
uniforme che si toglie in stampa con le luci di stampa [gen]. I positivi (2383) e gli invertibili (E-6, K-14)
non hanno maschera [gen].

Modello [prop]: matrice 3×3 di densità `D_out = M · D_coloranti`, con diagonale 1 e fuori diagonale
0,05–0,25 (es. magenta→blu 0,20–0,30, ciano→verde 0,10–0,20, ciano→blu 0,05–0,10, giallo→verde 0,03–0,08)
[gen]. Per un negativo mascherato la matrice effettiva vista dal positivo è quasi diagonale; per un invertibile
la si lascia "sporca" e si compensa con l'interimage.

### 1.4 Effetti interimage / interlayer (DIR) e saturazione

Gli accoppiatori **DIR** (Development Inhibitor Releasing) liberano un inibitore proporzionale allo sviluppo
locale; l'inibitore diffonde negli strati vicini e riduce lo sviluppo lì [gen]. Effetti:
1. **Saturazione**: dove uno strato si sviluppa molto (colore saturo) gli altri vengono frenati → separazione
   cromatica aumentata. Equivale a una matrice in spazio log-densità con termini fuori diagonale **negativi**
   (−0,05…−0,25), cioè un aumento di saturazione di ~1,1–1,4× nel dominio delle densità [gen].
2. **Nitidezza (acutanza)**: l'inibizione intra-strato a breve raggio crea bordi accentuati, simili agli
   effetti di adiacenza (vedi 1.7) [gen].
3. **Neutri più puri** e **grana ridotta** in alcune generazioni (T-Grain + DIR) [gen].

Gli invertibili (Velvia) usano interimage molto forte per compensare l'assenza di maschera: da qui
saturazione alta e verdi/blu "spinti" [gen]. Il manuale Fuji descrive ETERNA Vivid 160 come "high color
saturation, high contrast" [doc] Fuji MP film manual; Velvia 50 come "ultra-high saturation" con grigi
neutri e ombre profonde [doc] Fuji Velvia 50 datasheet.

### 1.5 Alonatura (halation), remjet e Cinestill

- **Remjet**: strato nero a base di carbone in legante alcali-solubile sul dorso delle pellicole cinema; assorbe la
  luce che attraversa l'emulsione e funge da antistatico; si rimuove in pre-bagno alcalino e lavaggio a spruzzo
  (ECN-2, K-14) [doc] Wikipedia "Anti-halation backing"; K-14 process. Le Vision3 5219/7219 hanno base acetato
  con remjet [doc] Kodak 7219 TD.
- Anche con anti-alone, le alte luci più forti mostrano un leggero alone rosso [doc] Wikipedia.
- **Cinestill 800T** è Vision3 500T con remjet rimosso in fabbrica per poter usare C-41: senza remjet, attorno alle
  sorgenti puntiformi compare un alone rosso-arancio marcato [gen] (fonti di settore, non datasheet).
- **Geometria** [gen]: il raggio dell'alone ≈ 2 × spessore base (andata e ritorno); base acetato cinema ~0,13–0,14 mm
  → raggio ~0,25–0,30 mm sul film. Su Super 35 (24,9 mm) a 4096 px: ~40–50 px; su 16 mm (10,3 mm) a 2048 px: ~50–60 px
  (in proporzione al quadro è 2,4× più grande). Il profilo è un anello morbido/esponenziale, non una gaussiana pura.
- **Soglia**: l'alone compare solo oltre una certa esposizione (alte luci speculari, +3…+6 stop sopra il grigio medio)
  perché è una frazione dell'1–5 % della luce trasmessa [gen].
- **Colore**: dominante nel canale rosso (strato ciano → nel positivo appare rosso), meno nel verde, quasi nullo nel blu.
  Valori iniziali [prop]: peso R 1,0 / G 0,25–0,4 / B 0,05.

### 1.6 Grana

- La granularità RMS (σD×1000) si misura con apertura 48 µm a densità netta 1,0 [doc] Kodak Double-X TD; Fuji Velvia
  datasheet; Kodak 7219 TD. I fotografici Kodak recenti usano invece il **Print Grain Index** (soglia visiva 25, JND = 4
  unità, visione a 14 pollici) e non è convertibile in RMS [doc] Kodak E-4050.
- Dimensioni [doc] (Vitale, "Film grain, resolution and perception"): alogenuri 0,2–2 µm (media ~0,6 µm); nuvole di
  colorante 1–6 µm (fino a ~10 µm in grumi); grana percepita 6–30 µm.
- **Grana vs densità** [gen]: nel negativo colore σD cresce con la densità (più colorante, più nuvole), ma l'immagine
  finale dipende dalla pendenza della curva del positivo: la grana è più visibile nei **toni medi e medio-scuri** del
  positivo, ridotta nelle alte luci (piede del print) e nei neri profondi (spalla del print). Modello [prop]:
  `σ_out(x) = σ_neg(D_neg) × |dD_print/dlogE|`, con `σ_neg(D) = σ0 × (0,6 + 0,4·D)`.
- Negli invertibili (E-6) la grana è massima nelle **ombre** (la densità alta coincide con lo scuro) [gen].
- **Grana vs formato** [gen]: a parità di pellicola la grana relativa al quadro scala con l'ingrandimento:
  Super 35 = 1×, 16 mm ≈ 2,4×, Super 8 ≈ 4,4×, 65 mm ≈ 0,5×, 120 (6×6) ≈ 0,4× rispetto a 24×36.
- **Struttura**: grana colore = nuvole morbide, decorrelate per strato (correlazione inter-canale 0,2–0,5); grana b/n =
  più netta, un solo canale [gen].
- **Frequenza**: lo spettro della grana non è bianco; ha un taglio dovuto alla dimensione delle nuvole (filtro passa-basso
  1–3 px a 4K per 35 mm) [gen].

### 1.7 Acutanza ed effetti di adiacenza (Eberhard, Mackie)

Durante lo sviluppo, il rivelatore si esaurisce nelle zone dense e i prodotti di reazione (bromuro, inibitori)
diffondono; al confine tra zona densa e zona chiara si formano un **bordo più scuro** (lato denso) e un **bordo più
chiaro** (lato rado): effetto Eberhard/bordo; con sviluppo poco agitato può comparire una linea marcata (**linea di
Mackie**) [gen; voci Wikipedia "Eberhard effect", "Mackie line"]. Raggio tipico 50–200 µm [gen]. È più forte con
rivelatori diluiti, poca agitazione, sviluppo da fermo (stand) e rivelatori a bassa solfito (Rodinal) [gen].

Modello [prop]: unsharp mask **in spazio densità** (non in luce), con raggio 0,1–0,4 % della larghezza del quadro e
guadagno 0–0,5, applicato prima della curva del positivo.

### 1.8 Risoluzione e MTF

| Pellicola | Risolvenza 1000:1 | Risolvenza 1,6:1 | Fonte |
|---|---|---|---|
| Fuji Acros 100 | 200 l/mm | 60 l/mm | [doc] Fuji Acros datasheet |
| Fuji Velvia 50 | 160 l/mm | 80 l/mm | [doc] Fuji Velvia 50 datasheet |
| Kodak Double-X 5222 | 100 l/mm | 32 l/mm | [doc] Kodak Double-X TD |

Risolvenza nativa "al 30 % di contrasto" [doc] Vitale: Tri-X 400 ~65 lp/mm; Portra 160NC ~73–80; Velvia ~80;
Provia 100F ~55; Technical Pan ~142; Panatomic-X ~170. ORWO UN54: fattore di modulazione m30 ≥ 0,80 [doc] ORWO.
Modello [prop]: MTF per canale come gaussiana + leggera sovra-risposta (DIR) alle basse frequenze (picco 1,05–1,15 a
5–15 cicli/mm), con frequenza al 50 % ~40–80 c/mm per negativi moderni [gen].

### 1.9 Reciprocità

| Pellicola | Regime senza correzione | Oltre | Fonte |
|---|---|---|---|
| Vision3 500T | 1/1000–1 s | — | [doc] Kodak 7219 TD |
| Fuji Eterna 250/500/Vivid | 1/1000–1/10 s | a 1 s +1/3 stop | [doc] Fuji MP manual |
| Portra 400, Ektar 100 | 1/10.000–1 s | "fare prove" | [doc] Kodak E-4050, E-4046 |
| Double-X 5222 | 1/10.000–1 s | — | [doc] Kodak Double-X TD |
| Velvia 50 | fino a 1 s | 4 s: +1/3 stop + 5M; 8 s: +1/2 + 7,5M; 16 s: +2/3 + 10M; 32 s: +1 + 12,5M; 64 s sconsigliato | [doc] Fuji Velvia 50 |
| Acros 100 | < 120 s | correzione oltre 120 s | [doc] Fuji Acros |
| Tri-X 400 | 1/1000–1/10 s | 1 s: +1 stop (2 s), sviluppo −10 %; 10 s: +2 stop (50 s), −20 %; 100 s: +3 stop (1200 s), −30 % | [doc] Kodak F-4017 |

Modello [prop]: legge di Schwarzschild `E_eff = I·t^p`, p = 1 entro il regime, p 0,75–0,9 oltre; per il colore p
diverso per strato → virata (Velvia verso il verde, da cui la correzione magenta).

### 1.10 Crossover cromatico

Quando le tre curve R,G,B del negativo non sono parallele (gamma diverse o piede/spalla diversi), un grigio cambia
colore con l'esposizione: es. ombre ciano e alte luci calde [gen]. È esattamente ciò che fa il push di ECN-2:
"contrast mismatches between layers, most strongly in the red and green layers, producing reddish-yellow highlights
and cyan-blue shadows"; il pull invece dà "ombre gialle e alte luci blu" [doc] Kodak Push/Pull guide. Modello
[prop]: gamma per canale + offset del piede per canale; crossover = differenza di gamma (0,00–0,06) tra i canali.

---

## 2. Processi di sviluppo

### 2.1 Processi standard

| Processo | Per | Dati chiave | Fonte |
|---|---|---|---|
| ECN-2 | negativi colore cinema | rivelatore 3:00 a 41,1 °C (106 °F); pre-bagno e rimozione remjet | [doc] Kodak Push/Pull guide |
| ECP-2 (2B/2E) | positivi colore | 2383 in ECP-2E; Fuji CP in ECP-2B; colonna sonora analogica a densità 1,1–1,6 a 800 nm | [doc] Kodak 2383; Fuji manual |
| C-41 | negativi colore still | rivelatore 3:15 a 37,8 °C | [gen] |
| E-6 | invertibili colore | 1° rivelatore b/n ~6 min a 38 °C, inversione, rivelatore cromogeno, sbianca, fix | [gen]; Ektachrome 100D "E-6, cine machine only" [doc] Kodak 100D sell sheet |
| K-14 | Kodachrome | emulsione **senza accoppiatori**: i coloranti si formano in 3 rivelatori cromogeni separati, ciascuno con il suo accoppiatore | [doc] Wikipedia K-14 |
| D-96 / D-97 | b/n cinema neg / positivo | Double-X ~7 min a 21 °C in D-96 [doc]; 2302 in D-97 a gamma 2,4–2,6 [doc] | Kodak |

**K-14 in dettaglio** [doc] Wikipedia K-14: rimozione remjet → 1° rivelatore PQ (immagine negativa argento; il filtro
giallo diventa opaco) → lavaggio → **riesposizione rossa dal dorso** → rivelatore ciano → lavaggio →
**riesposizione blu dall'alto** (il filtro giallo opaco protegge lo strato magenta) → rivelatore giallo → lavaggio →
rivelatore magenta con **agente velante chimico** → condizionatore → sbianca ferro-EDTA → fix. Conseguenze di look
[gen]: coloranti più stabili e puri (scelti liberamente perché non devono stare nell'emulsione), emulsione sottile e
quindi grande nitidezza; Kodachrome 25 RMS 9, Kodachrome 64 RMS 10 [doc] Vitale; rossi profondi, blu-cielo scuri,
gialli caldi, ombre dense e contrasto alto [gen].

### 2.2 Push / pull

**ECN-2 (Kodak)** [doc] Push/Pull guide, 41,1 °C:

| Condizione | Tempo | Δ rispetto a 180 s |
|---|---|---|
| Pull 1 | 2:30 | −17 % |
| Normale | 3:00 | — |
| Push 1 | 3:40 | +22 % |
| Push 2 | 4:40 | +56 % |

Kodak: il push non è raccomandato per guadagnare sensibilità; push + sottoesposizione dà perdita netta e "ombre
fumose"; il push aumenta molto la grana, il pull la migliora; il push può ridurre leggermente la nitidezza [doc].

**B/N (Tri-X 400, tank piccolo, agitazione ogni 30 s)** [doc] Kodak F-4017:

| Rivelatore | EI 400, 20 °C | EI 1600 (push 2), 20 °C | EI 3200 (push 3), 20 °C |
|---|---|---|---|
| D-76 | 6¾ min | 9½ min | 11 min |
| HC-110 (B) | 3¾ min | — | — |
| XTOL | 7 min | 9¾ min | 11½ min |

Con 1 stop di sottoesposizione Kodak indica sviluppo normale con lieve perdita nelle ombre [doc]. Tri-X RMS 17
[doc].

**Effetti fisici generali del push** [gen], da usare come mappature iniziali:
- gamma: +10–15 % per stop nel colore; +15–25 % per stop nel b/n (CI 0,56 → ~0,70 a push 2);
- velo (fog/Dmin): +0,02–0,05 per stop;
- sensibilità reale in ombra: +0,3…0,5 stop per stop di push (non +1);
- grana: ×1,2–1,35 per stop;
- colore: divergenza delle gamma R/G → crossover (vedi 1.10);
- spalla: le alte luci si "bloccano" prima (densità massima raggiunta prima).
- Pull: gamma −10 % per stop, grana ×0,85, ombre più vuote, colori più tenui.

**E-6 push** [gen]: allungamento del 1° rivelatore (circa +2 min per stop); aumenta Dmin (neri meno profondi, con
dominanti), riduce Dmax, aumenta contrasto apparente nei medi. Velvia 50 tollera da −½ a +1 stop con poca variazione
cromatica [doc] Fuji Velvia 50.

### 2.3 Rivelatori b/n e loro firma [gen]

| Rivelatore | Carattere | Grana | Acutanza / bordi | Curva |
|---|---|---|---|---|
| D-76 / ID-11 (stock) | riferimento, solvente (solfito) | fine, morbida | media | pieno uso della velocità |
| D-76 1+1 | meno solvente | leggermente più netta | più alta | simile |
| HC-110 | liquido, versatile | media | media | spalla leggermente compressa con diluizioni alte |
| Rodinal (1+25 / 1+50) | non solvente, basso solfito | netta, evidente | **alta** (bordi marcati) | perdita ~⅓ stop, alte luci ben separate |
| Rodinal 1+100 stand (60 min) | compensatore | netta | **molto alta**, possibili linee di Mackie / bromide drag | spalla molto morbida, ombre sostenute |
| XTOL | ascorbato, alta resa di velocità | fine | buona | piede pieno |

### 2.4 Cross-processing [gen]

| Combinazione | Risultato tipico | Parametri da modellare |
|---|---|---|
| E-6 in C-41 (dia → negativo) | negativo senza maschera arancio, contrasto molto alto; dopo inversione: colori saturi, alte luci bruciate, dominanti giallo-verdi o ciano a seconda dell'emulsione | gamma neg ×1,5–2; matrice senza correzione maschera; crossover forte; Dmin quasi neutro |
| C-41 in E-6 (negativo → positivo) | positivo con maschera arancio impressa: immagine piatta, pastello, dominante arancio-gialla nelle alte luci e blu-ciano nelle ombre | gamma ~0,6–0,8 nel positivo; tinta arancio additiva; neri sollevati |
| ECN-2 in C-41 | (Cinestill / Vision3 in C-41) funziona: contrasto un po' più alto, colori leggermente diversi; senza remjet → alonatura | gamma +5–10 %, alonatura forte |

### 2.5 Ritenzione d'argento: bleach bypass, skip bleach, ENR, CCE, ACE, NEC

Meccanismo [doc] Kodak "Processing techniques": parte o tutto l'argento dell'immagine, normalmente rimosso da sbianca
e fissaggio, resta insieme ai coloranti e aggiunge grigio/nero. Si può applicare al negativo, all'IP, al DN o alla
copia [doc].
- **Sul negativo**: alte luci più dense (nel positivo più chiare/bruciate), contrasto più alto, grana forse
  maggiore [doc].
- **Sul positivo**: ombre più scure e ricche, contrasto più alto, meno dettaglio in ombra (blocked-in),
  colori desaturati; fortemente dipendente dalla scena [doc].
- Funziona meglio con circa **1 stop di sottoesposizione**; più grana, meno latitudine [doc] Wikipedia
  "Bleach bypass". Nella pratica cinema spesso applicato all'internegativo [doc] Wikipedia.

Misure di argento ritenuto (densità infrarossa a ~1000 nm, dove i coloranti sono trasparenti) [doc] ASC "Soup du
jour" (nov. 1998):

| Trattamento | Lettura IR | Note |
|---|---|---|
| Copia normale (Deluxe) | 58–60 | argento residuo nullo, la lettura è base/coloranti |
| ENR (Technicolor) | 40 / 60 / 80 ... (densità ×100, non percentuali) | bagno b/n aggiuntivo tra sbianca e fissaggio, rigenera argento soprattutto in ombra; più contrasto, neri più profondi, lieve effetto bordo, desaturazione regolabile |
| ACE (Deluxe) | 30–60 % | scalabile; neri profondi con colori più fedeli; ~50 % usato in alcuni film |
| CCE (Deluxe) | 180–190 (~75 % argento) | più contrasto, più grana, neri molto chiusi, desaturazione e "grit", più dettaglio in ombra dello skip bleach |
| Skip bleach totale | ~240 (100 % argento, ~4× normale) | volti "vecchi e duri" |
| NEC (LTC, Parigi) | trattamento sull'interpositivo | punta più su contrasto e resa tonale che sul nero |

Modello [prop]: `D_tot(λ) = D_coloranti(λ) + k · D_argento`, con `D_argento = f · (D_vis_coloranti)^α`
(spettralmente neutra; α ≈ 1,0–1,2 per ENR che privilegia le ombre del positivo). Con f = 0…1:
- desaturazione: la componente neutra si somma uguale ai tre canali → saturazione ridotta di ~30–50 % a f = 1;
- contrasto: gamma del positivo ×(1 + 0,3…0,5 f);
- grana: σ ×(1 + 0,3 f) (la grana d'argento è più fine ma più "dura" delle nuvole di colorante);
- se applicato al **negativo**: alte luci del positivo più chiare (spalla neg più densa), effetto opposto.

### 2.6 Flashing (pre/post-esposizione)

Un "flash del 10 %" aggiunge **0,10 di densità sopra il Dmin** in ciascun colore [doc] ASC "Soup du jour". Sul
negativo solleva le ombre del negativo → nel positivo ombre più aperte e neri meno profondi, contrasto ridotto
nelle basse luci; sul positivo abbassa le alte luci. Un flash colorato tinge le ombre (neg) o le alte luci (print)
[gen]. Modello [prop]: `E_neg' = E_neg + flash · E_grigio18 · c_RGB`, con flash 0–30 %.

### 2.7 Pellicola scaduta [gen]

- Velo: Dmin +0,05…0,30, più forte nello strato **blu-sensibile** (in alto, più esposto a radiazioni cosmiche e
  calore) → negativo con velo giallo → positivo con ombre blu/ciano o magenta lavato.
- Perdita di sensibilità: regola pratica ~1 stop per decennio per pellicole ≤ 400 ISO conservate a temperatura
  ambiente; di più per alte sensibilità; poco se congelate.
- Perdita di contrasto (gamma −10…−30 %), crossover irregolare, maggiore grana apparente (velo + sovraesposizione
  compensativa).
- Kodak raccomanda per Vision3 conservazione a 13 °C (< 6 mesi) o −18…−23 °C (> 6 mesi) [doc] Kodak 7219 TD; Fuji:
  ~1 anno a ≤ 10 °C [doc] Fuji MP manual.

### 2.8 Redscale [gen]

Esposizione attraverso la base: la luce attraversa base → strato rosso → (maschera, filtro giallo) → verde → blu.
Rosso pieno, verde ridotto, blu quasi assente: immagini rosso-arancio a 1–2 stop di sovraesposizione, giallo pallido a
+3 stop. Sensibilità effettiva −1…−3 stop; nitidezza ridotta (diffusione nella base).

---

## 3. Schede per famiglia di pellicole

Dati numerici disponibili e firma visiva. I giudizi di look sono [gen] salvo indicazione.

### 3.1 Negativi cinema colore

| Pellicola | EI | Dati [doc] | Firma visiva [gen] |
|---|---|---|---|
| Kodak Vision3 50D (5203) | 50 D | ECN-2, remjet | grana finissima, colori puliti, tono pieno; base di riferimento "pulita" |
| Kodak Vision3 250D (5207) | 250 D | ECN-2 | grana fine, latitudine ampia, leggermente più caldo del 50D |
| Kodak Vision3 200T (5213) | 200 T | ECN-2 | look morbido, alte luci gentili, ottimo in interni |
| Kodak Vision3 500T (5219) | 500 T / 320 D (85) | reciprocità ok 1/1000–1 s; +2 stop di latitudine in alto; Status M; ECN-2 [doc 7219 TD] | grana visibile ma organica, ombre con leggero viraggio ciano-verde se sottoesposto, alonatura rossa sulle luci; pelle calda |
| Fuji Eterna 250/400/500 | 250/400/500 T | Eterna 500: "ultra-fine grain, broad latitude"; Eterna 400: "enhanced shadow detail, natural colour" [doc Fuji manual] | palette più fredda e meno satura del Kodak, verdi tendenti al ciano, contrasto basso, ombre "aperte" |
| Fuji Eterna Vivid 160/250/500 | 160 T | "high color saturation, high contrast" [doc] | contrasto e saturazione alti, pelle meno arrossata |
| Fuji Reala 500D (8592) | 500 D | **quarto strato** sensibile al ciano-blu [doc Fuji manual] | resa più fedele sotto fluorescenti/luce mista (meno verde), colori naturali |
| Agfa XT/XTS (anni '80–'90) | 100–400 | — | colori tenui, tendenza fredda, grana più grossa del Kodak coevo |
| Svema DS-4, DS-5M, LN-9; CND 64 | 50–80 ISO | stock colore cinema sovietici, prodotti fino al 1995 [doc Wikipedia Svema] | basso contrasto o contrasto irregolare, saturazione scarsa, dominanti verde-giallo, velo alto, grana grossa; crossover marcato |
| ORWO colore (ORWOcolor NC) | — | — | contrasto basso, palette verdastra-magenta, coloranti instabili (sbiadimento verso magenta nelle copie) |

### 3.2 Negativi e invertibili cinema b/n e invertibile colore

| Pellicola | Dati [doc] | Firma [gen] |
|---|---|---|
| Eastman Double-X 5222 | EI 250 D / 200 T; D-96 ~7 min a 21 °C; gamma 0,65–0,70; RMS 14; 100/32 l/mm; pancromatica [Kodak Double-X TD] | grana evidente ma bella, mezzitoni ricchi, alte luci che si aprono, rossi un po' scuri |
| ORWO UN54 | ISO 100 a gradiente 0,65 (D-96); 1,1 in inversione; RMS 12; m30 ≥ 0,80; base grigia 135 µm [ORWO] | tonalità classica, grana medio-fine, base grigia = anti-alone |
| ORWO N74 | (ISO 400) | più grana, contrasto più alto del 5222 in push |
| Kodak Ektachrome 100D (5294/7294) | EI 100 D / 25 T; E-6 solo macchine cine; "dense blacks, rich deep color, neutral gray scale, true-to-life skin"; saturazione moderata [Kodak sell sheet] | proiettabile, contrasto alto (gamma ~1,6–1,8 come invertibile), blu limpidi |
| Svema/Tasma b/n | — | Tasma: pellicole tecniche/cine b/n; contrasto variabile, grana grossa [gen] |

### 3.3 Intermedi e positivi

| Pellicola | Dati | Firma |
|---|---|---|
| Kodak 2383 | ECP-2E, base ESTAR 120 µm; perdita coloranti < 10 % in decenni [doc Kodak 2383] | il "look cinema" di riferimento: neri profondi, spalla dura, pelle calda, ciano-verdi nelle ombre [gen] |
| Kodak 2393 | premium, Dmax maggiore [gen] | più contrasto e neri più ricchi di 2383 [gen] |
| Fuji 3513 / Eterna-CP | ECP-2B; aim 1,10/1,05/1,05 Status A [doc Fuji manual] | leggermente più freddo e meno contrastato della 2383, verdi più "puliti" [gen] |
| Eastman 2302 b/n | D-97, gamma 2,4–2,6, solo blu-sensibile [doc 2302 TI] | neri pieni, alte luci brillanti |
| 5203/5254/2242 intermedi | gamma ~1 [gen]; aim Dmin + 1,0 (Fuji CI) [doc] | ogni generazione aggiunge un po' di contrasto, grana e perdita di nitidezza [gen] |

### 3.4 Fotografiche (still)

| Pellicola | Dati [doc] | Firma [gen] |
|---|---|---|
| Kodak Portra 400 | PGI 135: 37 (10×15), 59 (20×25), 89 (40×50); reciprocità ok 1/10.000–1 s [E-4050] | pelle morbida e calda, saturazione contenuta, latitudine enorme in sovraesposizione, verdi verso il giallo-oliva |
| Kodak Portra 160 | Portra 160NC ~73–80 lp/mm [Vitale] | ancora più tenue e fine |
| Kodak Ektar 100 | PGI 135: < 25 (10×15), 38 (20×25), 66 (40×50); "high saturation, ultra-vivid" [E-4046] | rossi e blu molto saturi, pelle tende al rosso, contrasto alto, poca latitudine in sottoesposizione |
| Kodak Gold 200 / UltraMax 400 | — | caldo-giallo, verdi saturi, contrasto medio, grana visibile |
| Fuji Superia 200/400 | quarto strato in alcune versioni [gen] | verdi freddi/ciano, magenta nei rossi, ombre verdastre |
| Fuji Velvia 50 | ISO 50; RMS 9; 160/80 l/mm; push −½…+1; reciprocità vedi 1.9 [Fuji] | saturazione estrema, verdi brillanti, blu profondi, contrasto alto, ombre chiuse |
| Fuji Provia 100F | RMS ~8–9; ~55 lp/mm a 30 % [Vitale] | neutra, contrasto medio-alto, la dia "standard" |
| Kodachrome 25/64 | RMS 9/10; ~50 lp/mm a 30 % [Vitale] | rossi profondi, gialli caldi, blu del cielo scuri, neri densi, nitidezza alta |
| Kodak Tri-X 400 | RMS 17; CI 0,56; push fino a EI 3200 [F-4017]; ~65 lp/mm a 30 % [Vitale] | grana evidente e "classica", mezzitoni robusti, spalla morbida |
| Ilford HP5 Plus | — | più morbido di Tri-X, grana leggermente più grande, ottima in push |
| Ilford Delta 100/400/3200 | — | tabulare: grana fine e regolare, tonalità lineare, meno "carattere" |
| Fuji Acros 100 (II) | RMS 7; 200/60 l/mm; reciprocità ok < 120 s [Fuji] | grana finissima, toni puliti, cieli leggermente scuri (sensibilità ortopancromatica) |
| Cinestill 800T | Vision3 500T senza remjet, per C-41 | alonatura rossa intensa, tungsteno (dominante blu in luce diurna), grana di 500T in push |
| Foma (Fomapan 100/200/400) | — | grana tradizionale, contrasto alto, toni medi un po' compressi |
| Ferrania (P30, Solaris) | — | P30: b/n ad alto contenuto d'argento, contrasto elevato, neri densi; Solaris: colore caldo-saturo, grana media |

---

## 4. Proposta di controlli per il plugin [prop]

Principio: **pochi controlli intuitivi, ciascuno collegato a parametri fisici documentati**. I valori sono
punti di partenza da calibrare su riferimenti reali (test chart fotografate e scansionate da noi).

### 4.1 Controlli principali

| Controllo | Range | Default | Parametri pilotati (mappatura) |
|---|---|---|---|
| **Archetipo** (menu) | lista §5 | "Negativo cinema neutro" | curve neg per canale, matrice coloranti, interimage, grana base, MTF, alonatura |
| **Esposizione** (stop) | −4…+4 | 0 | offset logE; la latitudine viene dalla forma delle curve (non clip) |
| **Sviluppo push/pull** (stop) | −2…+3 | 0 | per stop: gamma ×(1 + 0,12 s) colore / ×(1 + 0,20 s) b/n; Dmin +0,03 s (solo s > 0); grana ×1,28^s; sens. ombre +0,4 s stop; crossover R/G ±0,02 s (alte luci rosso-gialle, ombre ciano-blu per s > 0, invertito per s < 0, come da guida Kodak); spalla anticipata di 0,15 s stop |
| **Ritenzione argento %** | 0…100 | 0 | f = %/100; desat 0,45 f; gamma print ×(1 + 0,4 f); grana ×(1 + 0,3 f); Dmax +0,3 f; punto di applicazione: menu Negativo / Positivo |
| **Stampa** (menu) | Nessuna (scan) / Positivo standard / Positivo premium / Positivo freddo / b/n | Positivo standard | curva print (gamma 2,6 / 3,0 / 2,5 / 2,5), Dmax 3,8 / 4,1 / 3,7 / 3,5 |
| **Luci di stampa R/G/B** (punti) | −12…+12 | 0 | 1 punto = 0,025 logE (convenzione laboratorio, [gen]) sull'esposizione del positivo per canale |
| **Grana** | 0…200 % | 100 % | scala σ0 dell'archetipo; dipendenza da densità e formato automatica |
| **Formato** (menu) | Super 8 / 16 mm / Super 35 / 65 mm / 135 / 120 | Super 35 | scala grana, alone e MTF col fattore di ingrandimento (§1.6) |
| **Alonatura** | 0…200 % | archetipo | intensità; raggio da formato; soglia +3 stop; "Senza remjet" come preset (×4 intensità) |
| **Saturazione da interimage** | 0…200 % | 100 % | scala dei termini fuori diagonale negativi della matrice DIR |
| **Effetto bordo (sviluppo)** | 0…100 % | archetipo | guadagno unsharp in densità 0–0,5, raggio 0,2 % del quadro |
| **Flash** | 0…30 % (+ tinta) | 0 | +0,01 D per % sopra Dmin sul negativo (10 % = 0,10 D, da ASC) |
| **Età / scaduta** (anni) | 0…40 | 0 | velo blu-strato +0,008/anno (max 0,3), sensibilità −0,1 stop/anno (max −3), gamma −0,7 %/anno, grana +1 %/anno |
| **Processo alternativo** (menu) | Standard / Cross E-6→C-41 / Cross C-41→E-6 / Redscale | Standard | sostituisce matrice e curve secondo §2.4 e §2.8 |

### 4.2 Ordine di elaborazione suggerito [prop]

1. Ingresso scena lineare (da LogC/S-Log/V-Log ecc., già gestiti altrove nel prodotto) → **logE**.
2. Sensibilità spettrale: matrice 3×3 camera → strati (con quarto strato opzionale per archetipi "fedeli").
3. Reciprocità/età/flash → esposizione efficace per strato.
4. **Alonatura** (in esposizione lineare, prima della curva: è luce che ri-espone).
5. MTF/diffusione per strato (blur in esposizione).
6. Curve negativo per canale (gamma, piede, spalla, Dmin, push).
7. Interimage DIR (matrice in densità) + **effetto bordo** (unsharp in densità).
8. **Grana** in densità del negativo (dipendente da D e formato).
9. Coloranti + assorbimenti indesiderati + maschera; argento ritenuto (se sul negativo).
10. Luci di stampa → curva positivo; argento ritenuto (se sul positivo).
11. Uscita (display o di nuovo log per il grading).

### 4.3 Note di implementazione

- Lavorare in **densità/log** per curve, interimage, grana ed effetti di bordo; in **lineare** per alonatura e
  diffusione ottica.
- Grana: rumore precalcolato tileable a più ottave per canale con correlazione inter-canale 0,3; seme legato al
  frame (grana che "vive"); niente grana sotto Dmin.
- Controlli OFX: nomi permanenti (vincolo del progetto); gli archetipi vanno identificati da ID stabili, non da
  etichette tradotte.

---

## 5. Archetipi generici (senza marchi) [prop]

Valori iniziali: **γR/γG/γB** = gamma del negativo per canale (per gli invertibili: gamma del positivo diretto);
**piede/spalla** = ampiezza della transizione in stop (piede: sotto il grigio medio, dove la pendenza scende al
50 %; spalla: sopra il grigio medio, dove inizia la compressione); **IIE** = forza interimage (1,0 = negativo cinema
moderno, equivalente a ~ −0,15 fuori diagonale); **σ** = grana RMS-like (scala tipo RMS diffusa a D = 1, formato
35 mm); **H** = alonatura (0 = assente, 1 = moderna con remjet, 4+ = senza anti-alone); **Sat** = saturazione percepita
attesa; ispirazione indicata solo per orientamento interno.

| # | Archetipo | γR | γG | γB | Piede (stop) | Spalla (stop sopra 18 %) | IIE | σ | H | Altre caratteristiche |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Negativo cinema lento, pulito | 0,60 | 0,60 | 0,61 | 3,0 | +7 | 1,0 | 6 | 1 | neutro, grana finissima |
| 2 | Negativo cinema veloce tungsteno | 0,58 | 0,59 | 0,60 | 2,5 | +6,5 | 1,0 | 11 | 1,2 | ombre leggermente ciano-verdi in sotto; pelle calda |
| 3 | Negativo veloce senza remjet | 0,62 | 0,63 | 0,64 | 2,5 | +6 | 1,0 | 13 | 4,5 | alone rosso-arancio forte, soglia bassa (+2,5 stop) |
| 4 | Negativo cinema freddo a basso contrasto | 0,55 | 0,56 | 0,57 | 3,5 | +7 | 0,8 | 8 | 1 | verdi verso ciano, ombre aperte, saturazione 0,85 |
| 5 | Negativo vivido ad alto contrasto | 0,66 | 0,66 | 0,65 | 2,0 | +5,5 | 1,4 | 8 | 1 | saturazione 1,25, nero più denso |
| 6 | Negativo ritratto morbido | 0,56 | 0,57 | 0,58 | 3,0 | +8 | 0,8 | 9 | 0,8 | rossi/arancio desaturati, pelle uniforme, verdi oliva |
| 7 | Negativo amatoriale caldo | 0,64 | 0,62 | 0,60 | 2,0 | +5 | 1,1 | 12 | 1 | dominante giallo calda in alte luci, verdi saturi |
| 8 | Negativo amatoriale freddo-verde | 0,62 | 0,64 | 0,62 | 2,0 | +5 | 1,1 | 12 | 1 | ombre verdi, rossi verso magenta |
| 9 | Invertibile ultra-saturo | 1,80 | 1,85 | 1,80 | 1,0 | +2 | 1,8 | 8 | 0,5 | matrice "sporca" compensata da IIE forte; ombre chiuse; verdi/blu spinti |
| 10 | Invertibile neutro | 1,60 | 1,60 | 1,62 | 1,3 | +2,5 | 1,3 | 9 | 0,5 | grigi puliti, blu limpidi |
| 11 | Invertibile a coloranti aggiunti in sviluppo | 1,75 | 1,70 | 1,65 | 1,0 | +2 | 1,2 | 9 | 0,6 | MTF alta, rossi profondi, gialli caldi, cieli scuri, neri densi |
| 12 | Colore d'archivio a basso contrasto (Est Europa anni '70–'80) | 0,50 | 0,55 | 0,58 | 3,0 | +5 | 0,5 | 16 | 1,5 | velo +0,15 blu, crossover marcato, dominante verde-gialla, saturazione 0,7 |
| 13 | B/N cinema classico | 0,67 | 0,67 | 0,67 | 2,5 | +6 | — | 14 | 1,2 | pancromatico, rossi leggermente scuri, effetto bordo 0,2 |
| 14 | B/N still veloce "da reportage" | 0,58 | 0,58 | 0,58 | 2,5 | +6 | — | 17 | 1 | spalla morbida, grana evidente; ottimo con push |
| 15 | B/N tabulare finissimo | 0,60 | 0,60 | 0,60 | 2,5 | +6,5 | — | 7 | 0,8 | grana minima, tono lineare, cieli scuri (sensibilità rossa ridotta) |

Note sui valori: σ per i b/n sono ricavati dagli ordini di grandezza documentati (Double-X 14, UN54 12, Tri-X 17,
Acros 7 [doc]); per i colori sono stime coerenti con Velvia 9 / Kodachrome 9–10 / Provia ~8–9 [doc] e con
l'osservazione che i negativi cinema moderni a bassa sensibilità sono tra i più fini [gen]. Le gamma dei negativi
colore e le spalle sono [gen]: da calibrare digitalizzando le curve pubblicate. Gli archetipi b/n senza IIE usano
la sola curva di luminanza dopo una miscela spettrale (es. pancromatico R 0,30 / G 0,55 / B 0,15; ortopancromatico R 0,20 /
G 0,60 / B 0,20) [prop].

Associazione stampa consigliata [prop]: archetipi 1–8 e 12 → "Positivo standard" (o "Nessuna" per un look da
scansione); 9–11 → "Nessuna" (già positivi); 13–15 → "b/n" con gamma 2,5 (da 2302 [doc]) o "Nessuna".

---

## 6. Cosa resta incerto / da verificare

- Gamma per canale, Dmin e Dmax numerici di Vision3, Eterna, 2383, 3513: **non presenti in testo** nei datasheet;
  vanno misurati sui grafici (digitalizzazione) o con nostre esposizioni di prova.
- Valori RMS dei negativi colore cinema moderni: solo grafici; per i fotografici Kodak recenti c'è solo PGI.
- Le mappature push/pull, scaduta, cross-processing e i parametri degli archetipi sono **ipotesi progettuali**
  coerenti con la letteratura, non dati misurati.
- Ilford, Foma, Ferrania, Agfa, Svema: non sono stati consultati datasheet in questa ricerca; le descrizioni sono [gen].
- Geometria dell'alone (raggio da spessore base) è un modello semplificato: andrebbe confrontato con fotografie di
  sorgenti puntiformi su pellicola reale senza remjet.

---

## 7. Fonti

Datasheet e documenti tecnici dei produttori:
- Kodak VISION3 500T 7219 Technical Data — https://www.kodak.com/content/products-brochures/Film/VISION3-500T-Color-Negative-Film-7219-TECHNICAL-DATA.pdf
- Kodak VISION3 500T 5219/7219 brochure — https://www.kodak.com/content/products-brochures/motion-picture/KODAK-VISION3-500T-5219-7219-brochure.pdf
- Kodak VISION Color Print Film 2383/3383 data sheet — https://www.kodak.com/content/products-brochures/Film/KODAK-VISION-Color-Print-Film-2383-3383-data-sheet.pdf
- Kodak Black-and-White Print Film 2302/3302 technical information — https://www.kodak.com/content/products-brochures/motion-picture/Kodak-black-and-white-print-film-2302-3302-technical-information.pdf
- Eastman Double-X Technical Data — https://www.kodak.com/content/products-brochures/Film/DOUBLEX-Technical-Data-EN.pdf
- Kodak Ektachrome 100D sell sheet — https://www.kodak.com/content/products-brochures/Film/EKTACHROME-100D-sell-sheet-EN.pdf
- Kodak Processing Techniques (skip bleach) — https://www.kodak.com/en/motion/page/processing-techniques/
- Kodak Processing Manuals (H-24 moduli) — https://www.kodak.com/en/motion/page/processing-manuals/
- Eastman Kodak motion push/pull processing guide — https://flicfilm.ca/wp-content/uploads/2024/04/Eastman-Kodak-Motioin-Push-Pull-processing-guide.pdf
- Kodak Portra 400 E-4050 — https://www.freestylephoto.com/static/pdf/product_pdf/kodak/e4050_Kodak_Portra_400.pdf
- Kodak Ektar 100 E-4046 — https://www.kodakprofessional.com/sites/default/files/2025-07/e4046.pdf
- Kodak Tri-X 320/400 F-4017 — https://kodakprofessional.com/sites/default/files/wysiwyg/film/f4017_trix_320400.pdf
- Fujifilm Motion Picture Film manual — https://www.theodoropoulos.info/attachments/076_fujifilm_motion_picture_film_manual.pdf
- Fujichrome Velvia 50 datasheet — https://asset.fujifilm.com/www/jp/files/2024-04/7d7888aab11c548b2acd50f0fee9e504/datasheet_velvia50_01.pdf
- Fujifilm Neopan Acros 100 datasheet (AF3-083E) — https://juneleung.github.io/filmdoc/fuji/Acros-120_AF3-083E.pdf
- ORWO UN54 datasheet — https://www.digitaltruth.com/products/orwo_tech/ORWO-UN54-datasheet.pdf

Articoli e riferimenti:
- American Cinematographer, "Soup du Jour" (nov. 1998), processi a ritenzione d'argento — https://theasc.com/magazine/nov98/soupdujour/pg2.htm
- T. Vitale, "Film grain, resolution and perception" — https://cool.culturalheritage.org/videopreservation/library/film_grain_resolution_and_perception_v24.pdf
- Wikipedia, Anti-halation backing (Remjet) — https://en.wikipedia.org/wiki/Remjet
- Wikipedia, K-14 process — https://en.wikipedia.org/wiki/K-14_process
- Wikipedia, Bleach bypass — https://en.wikipedia.org/wiki/Bleach_bypass
- Wikipedia, Svema — https://en.wikipedia.org/wiki/Svema
- Wikipedia, Eberhard effect — https://wikipedia.com/wiki/Eberhard_effect
- Wikipedia, Mackie line — https://en.wikipedia.org/wiki/Mackie_line
- ASC, "Kodak Enhances Vision3" — https://theasc.com/article/kodak-enhances-vision3/

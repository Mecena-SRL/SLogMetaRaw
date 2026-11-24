# Processi fotografici e cinematografici storici: fisica, artefatti e modelli implementabili

Ricerca per un plugin OFX di look originale che emuli i processi storici modellandone il funzionamento reale, non
con LUT o preset estetici. Ingresso: RGB scene-linear da camera moderna. Uscita: immagine come apparirebbe il
supporto storico osservato o proiettato.

**Legenda delle affermazioni**
- **[doc: N]**: fatto trovato in una fonte consultata in questa ricerca (numero nell'elenco fonti in fondo).
- **[gen]**: conoscenza generale non verificata in questa sessione. Va controllata prima di trattarla come dato.
- **[mod]**: proposta di modello o coefficiente nostro, cioè una stima ingegneristica e non un dato storico. Va
  tarata su campioni reali (scansioni di originali, curve pubblicate).

Nessun coefficiente [mod] è "estratto": sono punti di partenza da fittare.

---

## 0. Architettura comune proposta

Quasi tutti i processi condividono la stessa catena fisica. Conviene implementarla una volta sola e
parametrizzarla per processo.

1. **Registrazione spettrale.** Per ogni strato o lastra k, l'esposizione è `E_k = t · ∫ S_k(λ) · F_k(λ) · L(λ) dλ`.
   Qui S_k è la sensibilità dell'emulsione, F_k il filtro di presa e L la radianza della scena.
   - *Via rapida* [mod]: `E_k = Σ_c w_kc · RGB_c`, con pesi w (tabelle sotto) sull'RGB lineare (primarie
     Rec.709/sRGB lineari; per ACES AP0/AP1 va prima convertito).
   - *Via accurata* [gen]: si ricostruisce uno spettro plausibile da RGB con un upsampling spettrale (es.
     Jakob & Hanika 2019, "A Low-Dimensional Function Space for Efficient Spectral Upsampling", con LUT 3D
     precalcolata di coefficienti sigmoidi), poi si integra con curve S_k(λ) tabulate a 5–10 nm. Ha due vantaggi.
     Rende correttamente i metamerismi: un rosso saturo e un arancio con lo stesso RGB-luma reagiscono in modo
     diverso a una emulsione cieca al rosso. Inoltre permette di usare direttamente le curve pubblicate. La LUT
     può essere precalcolata offline in un unico passaggio, una per processo: `RGB → E_k`, una 3D-LUT 33³ per
     ogni record.
2. **Curva caratteristica (H&D).** `D = Dmin + (Dmax − Dmin) · σ(γ' · (log10 E − log10 E0))`, con σ sigmoide
   logistica. Va tarata in modo che la pendenza nel tratto rettilineo sia γ. Il piede e la spalla si controllano
   con l'asimmetria (curva logistica generalizzata) [mod].
3. **Stampa o inversione.** Per i processi negativo→positivo si applica una seconda curva H&D, con esposizione
   `10^(−D_neg)`. Per i positivi diretti (dagherrotipo, ambrotipo, ferrotipo, autochrome) c'è una sola curva,
   eventualmente invertita.
4. **Formazione del colore.**
   - *Sottrattiva*: `T_c = 10^(−Σ_j D_j · A_jc)`, dove A è la matrice di assorbimento dei coloranti, con gli
     assorbimenti indesiderati fuori diagonale. La radianza vista è `T_c · L_proiettore,c`.
   - *Additiva*: `RGB_out = Σ_k T_k · P_k`, dove P_k è il colore (in lineare) del filtro di visione k, pesato
     per l'area che occupa.
   - *Monocroma tonata*: `RGB_out = carta_c · 10^(−D · K_c)`, con K il vettore di assorbimento spettrale del
     materiale d'immagine (argento colloidale, oro, platino, blu di Prussia...).
5. **Artefatti spaziali e temporali**: misregistrazione, texture del supporto, mosaici, danni e invecchiamento.
6. **Invecchiamento**: perdita selettiva di densità per colorante, ingiallimento del supporto (Dmin che cresce
   in blu), riduzione del contrasto.

Lavorare in densità (log) e non in valori RGB è ciò che rende credibili l'invecchiamento e i coloranti. È anche
il punto in cui l'approccio si distingue da un grading.

---

## 1. Sensibilità spettrali di base (tabella riassuntiva)

| Emulsione | Intervallo utile | Picco | Fonte |
|---|---|---|---|
| AgI (dagherrotipo, ioduro puro) | UV–437 nm | 420–425 nm | [doc: 1] |
| AgI:AgBr 2:1 (collodio) | 380–527 nm | 425 nm | [doc: 1] |
| AgI:AgBr 1:1 | 380–530 nm | 440 nm | [doc: 1] |
| AgBr | 390–500 nm | 450 nm | [doc: 1] |
| Collodio umido, pratica | fino a ~520–530 nm | blu | [doc: 1][doc: 2] |
| Ortocromatica | blu + verde + giallo-verde, quasi nulla verso arancio e rosso | — | [doc: 3] |
| Pancromatica convenzionale | ~380–650 nm | — | [doc: 3] |
| Pan tipo B e tipo C (Kodak) | B ha meno rosso di C | — | [doc: 3] |

Nota [doc: 1]: secondo Mokhtari et al., aggiungere bromuro (dal 1853) estende la sensibilità del collodio fino a
~530 nm, ma non oltre. Quindi il rosso non viene registrato.

**Pesi RGB lineari (Rec.709) proposti** [mod]. Sono normalizzati a somma 1 per un grigio neutro. Servono per la
via rapida e vanno fittati integrando le curve sopra su spettri di riflettanza tipici (ColorChecker).

| Record | wR | wG | wB | Note |
|---|---|---|---|---|
| Dagherrotipo (AgI puro) | 0.00 | 0.04 | 0.96 | il verde quasi non conta |
| Dagherrotipo accelerato (Br/Cl) | 0.00 | 0.10 | 0.90 | |
| Collodio / calotipo | 0.00 | 0.12 | 0.88 | più un termine "UV" (sotto) |
| Ortocromatico precoce (eritrosina) | 0.02 | 0.40 | 0.58 | |
| Ortocromatico tardo | 0.04 | 0.52 | 0.44 | |
| Pancromatico precoce (tipo B) | 0.18 | 0.46 | 0.36 | blu ancora sovrarappresentato |
| Pancromatico moderno (tipo C) | 0.27 | 0.48 | 0.25 | |
| Luminanza visiva (riferimento) | 0.2126 | 0.7152 | 0.0722 | |

**Termine UV** [mod]. Le camere moderne non registrano l'UV, mentre le emulsioni blu-sensibili sì, e molto. Il
cielo, che ha forte componente UV e blu diffuso, diventa quindi bianco anche con poca esposizione. Si può
approssimare con `E += k_uv · max(0, B − 0.5·G)`, k_uv tra 0 e 0.5, attivato soprattutto nelle zone di cielo
(alta luminanza, bassa saturazione verso il blu). Il controllo si chiamerebbe "Sensibilità UV".

**Conseguenze visive** [doc: 3, 2][gen]:
- Con emulsione blu-sensibile o ortocromatica, i rossi diventano quasi neri: labbra scure, mattoni e uniformi
  rosse nere, lentiggini e couperose accentuate perché l'emoglobina assorbe nel blu-verde.
- Il cielo blu diventa bianco e piatto, senza nuvole se non con mascherini o stampa combinata [gen].
- I gialli (fiori, pomodori) vengono scuri [doc: 3].
- Gli occhi azzurri vengono chiarissimi, quasi vuoti [gen].
- Con la pancromatica la pelle si schiarisce e si ammorbidisce. Il rosso estremo e l'IR riducono rughe e
  imperfezioni [doc: 3].

---

## 2. Dagherrotipo (1839–1860 circa)

### Come funziona
- Lastra di rame placcato argento, prima laminata, poi argentata galvanicamente, lucidata a specchio.
  Sensibilizzata con vapori di iodio (AgI). Le lastre più rapide hanno un trattamento con bromo o cloro e poi
  altro iodio. Il colore dello strato segue i cicli di interferenza giallo → arancio → rosso → blu → giallo. La
  sensibilità è massima al giallo e il contrasto cala con lo spessore. Si usava di solito al secondo o terzo
  giallo [doc: 4].
- Sviluppo a vapori di mercurio a ~60 °C, con la lastra a ~45°, per ~20 minuti [doc: 4]. Fissaggio in
  tiosolfato, viraggio all'oro opzionale [doc: 4].
- L'immagine è fatta di nanoparticelle di amalgama argento-mercurio che diffondono la luce. Le proprietà ottiche,
  la risoluzione e la gamma dinamica dipendono dalla diffusione (anche plasmonica) di queste nanostrutture
  metalliche. Le particelle hanno grande variabilità di dimensione, forma e composizione [doc: 5].
- Le alte luci sono zone dense di particelle bianco-diffondenti. Le ombre sono argento nudo speculare, che appare
  scuro solo perché riflette un ambiente scuro, come il velluto nero dell'astuccio [doc: 4][gen]. Inclinando la
  lastra verso una superficie chiara l'immagine si inverte in negativo [gen].
- L'immagine è invertita lateralmente (speculare), a meno di uno specchio davanti all'obiettivo, che però allunga
  l'esposizione [doc: 4].
- Esposizioni [gen]: nel 1839 da molti minuti fino a ~15 minuti in pieno sole. Dal 1840–41, con accelerazione
  bromo/cloro e obiettivo Petzval (f/3.6 circa), da secondi a decine di secondi per i ritratti. Si usavano
  poggiatesta, da cui le espressioni rigide.
- Il colorito a mano sulle guance (carminio, oro sui gioielli) è documentato: oro, carminio, giallo cromo,
  oltremare [doc: 4].

### Artefatti
- [gen] Ossidazione o solforazione (Ag2S): aloni iridescenti dal bordo verso l'interno (blu, magenta, ambra)
  dovuti a film sottili con interferenza; spesso un anello a ridosso del passe-partout.
- Velatura bianca ("bloom") e cristalli sotto il vetro [gen].
- Graffi di lucidatura direzionali, sottili e paralleli, che si vedono sullo specchio [doc: 4 menziona la
  lucidatura][gen per l'aspetto].
- Movimento: soggetti mossi semitrasparenti o assenti (strade "vuote") [gen].
- Solarizzazione e sovraesposizione: alte luci bluastre o "bruciate" in negativo [gen].
- Vignettatura e campo curvo del Petzval, con sfocato "swirl" [gen].

### Modello [mod]
1. `E = exp · (0.0·R + 0.06·G + 0.94·B + k_uv·UV)`, con eventuale integrazione temporale (sezione 2b).
2. Densità di particelle `ρ = σ(γ·(log10 E − log10 E0))`, con γ circa 1.2–1.6. Nessuna stampa: è un positivo
   diretto.
3. Riflettanza osservata: `L = ρ · ρ_diff · C_amalgama + (1 − ρ) · R_spec · L_env(θ)`. Qui ρ_diff ≈ 0.6–0.8,
   C_amalgama è un bianco leggermente caldo-grigio (viraggio oro: più caldo, più contrasto) e R_spec ≈ 0.9 è la
   riflettanza dell'argento. `L_env(θ)` è la luminanza dell'ambiente riflesso, che dipende dall'angolo.
   - Con L_env ≈ 0.02 si ha un positivo normale.
   - Con L_env > ρ_diff·C si ha un negativo.
   - Con valori intermedi si ha un'immagine slavata ("fantasma"). Così positivo e negativo emergono dallo stesso
     modello fisico.
4. Speculare: flip orizzontale (attivo di default).
5. Ossidazione: campo `d` = distanza normalizzata dal bordo della finestra (passe-partout ovale o rettangolare),
   più rumore a bassa frequenza. Spessore del film `h = h_max · smoothstep(r0, 0, d + n)`. Colore da
   interferenza di film sottile, `I(λ) ∝ 1 − cos(4π·n·h/λ)` integrato su tre lunghezze d'onda RGB (circa 610,
   550 e 465 nm), con n ≈ 2.5 (indice Ag2S, [gen]). Si moltiplica la componente speculare per questo colore.
6. Texture: graffi direzionali (rumore anisotropo allungato, ampiezza bassa) solo nella componente speculare.

### Controlli
| Controllo | Fisica | Intervallo |
|---|---|---|
| Angolo di osservazione | L_env: dal positivo al negativo | −1 (negativo) … 0 … +1 (positivo pieno), default 0.8 |
| Accelerazione (bromo) | pesi spettrali e velocità | 0 = 1839 (solo AgI), 1 = 1845 |
| Durata della posa | finestra di integrazione temporale (2b) | 0–60 s equivalenti (in fotogrammi) |
| Ossidazione | spessore massimo dell'alone e penetrazione | 0–1 |
| Viraggio oro | tono e contrasto | 0–1 |

### 2b. Pose lunghe come modello temporale [mod]
Le esposizioni da secondi a minuti sono il cuore del look: integrare N fotogrammi (media in lineare) con un
fattore di "presenza". Chi si muove viene rappresentato pesato dal tempo di permanenza. In OFX basta richiedere
il clip a t−N…t (limite pratico 16–64 frame, con decimazione temporale). Si può aggiungere un micro-tremolio
residuo da poggiatesta, cioè un blur direzionale minimo sui volti.

---

## 3. Calotipo e talbotipo (1841 circa – anni 1850)

- [gen] Negativo su carta sensibilizzata con ioduro d'argento e "gallo-nitrato", sviluppo latente. È
  blu-sensibile, con la stessa famiglia spettrale di AgI/AgBr [doc: 1 per gli alogenuri].
- Il negativo è di carta: le fibre e la "formazione" della carta (nuvole di densità) si stampano nel positivo
  (carta salata), insieme a eventuali cerature [gen].
- Risoluzione bassa, aspetto morbido e "pittorico", toni bruno-rossastri della stampa salata [gen].

### Modello [mod]
1. Registrazione blu-sensibile (pesi del collodio) con γ negativo ≈ 0.8–1.0.
2. Texture fibra *nel negativo*. Si somma alla densità del negativo un campo
   `D_fibra = a · (rumore anisotropo allungato, lunghezza 20–80 px, larghezza 1–3 px, orientazione casuale)`
   più `b · rumore Perlin a 2–3 ottave` (formazione), con a ≈ 0.03–0.12 D e b ≈ 0.05–0.2 D. Poi si applica una
   leggera diffusione (blur ≈ 1–2 px a 2K) che rappresenta la luce che attraversa la carta durante la stampa a
   contatto.
3. Stampa su carta salata (sezione 5) con la stessa texture in parte presente anche nella carta positiva.

### Controlli
- **Fibra del negativo** 0–1
- **Cera** 0–1: la cera riduce la diffusione e quindi la texture
- **Diffusione di stampa** 0–5 px
- **Tono della carta salata** (vedi sezione 5)

---

## 4. Collodio umido: ambrotipo, ferrotipo, negativo su vetro (1851–1880 circa)

### Come funziona
- Lastra di vetro (ambrotipo, negativo) o di ferro laccato nero (ferrotipo). Collodio iodurato e bromurato,
  sensibilizzato in nitrato d'argento, esposto e sviluppato *bagnato* entro ~20 minuti [doc: 1]. Nel test
  moderno del 2021 la preparazione, l'esposizione e lo sviluppo stavano sotto i 30 minuti [doc: 1].
- Spettro utile ~380–530 nm, picco nel blu [doc: 1]. In pratica nulla sopra ~520 nm [doc: 2].
- Sensibilità [gen]: equivalente ISO tra ~0.5 e 5 a seconda della chimica, quindi pose da 2 a 15 s in studio
  luminoso, meno all'aperto.
- Ambrotipo: negativo sottoesposto e sbiancato (argento chiaro), visto su fondo nero. L'argento grigio-crema
  diffonde ed è percepito come positivo [gen]. Ferrotipo: stesso principio su lacca nera o marrone, con neri
  meno profondi e toni caldi [gen].
- Il collodio è anche il materiale di Maxwell/Sutton (1861), vedi sezione 9.

### Artefatti caratteristici [gen salvo dove indicato]
- **Bordi di colata**: il collodio viene versato da un angolo, quindi uno spessore irregolare e densità
  disomogenea verso un bordo. Spesso un angolo resta scoperto, dove la lastra era tenuta col dito o col piede di
  colata.
- **Comete**: punti di polvere o particelle che durante lo sviluppo trascinano una scia ("coda").
- **Oyster lines**: linee concentriche come gusci d'ostrica, dovute all'asciugatura disomogenea del bagno
  d'argento sul bordo.
- **Velo d'argento** e macchie di sviluppo a forma di fronte di flusso, dove lo sviluppatore è stato versato.
- **Vignettatura** forte e sfocato "a vortice" dei Petzval. Messa a fuoco molto stretta per via delle aperture
  grandi.
- Il rosso diventa nero e il cielo bianco. Le vene e le lentiggini si accentuano. Gli occhi azzurri vengono
  chiari [doc: 2 sul taglio a 520 nm; resto gen].
- Ferrotipo: tono caldo, alte luci color crema e mai bianche, Dmax limitato dal fondo laccato.

### Modello [mod]
1. `E = exp · (0.00·R + 0.12·G + 0.88·B + k_uv·UV)`.
2. Curva: γ ≈ 1.0–1.5. Per l'ambrotipo la densità d'argento diventa riflettanza diffusa
   `ρ = ρ_max · σ(...)`, con ρ_max ≈ 0.55–0.7. Il fondo ha riflettanza ~0.02–0.04 (vernice nera) per
   l'ambrotipo e ~0.04–0.08 (lacca) per il ferrotipo.
3. Colore: alte luci `C_hi` = crema (lineare circa 0.80, 0.75, 0.62). Fondo ambrotipo neutro-freddo. Fondo
   ferrotipo bruno (circa 0.05, 0.035, 0.025) [mod].
4. Artefatti procedurali:
   - Mappa di spessore `h(x,y)` = gradiente da un angolo più rumore a bassa frequenza. Modula sensibilità e
     ρ_max (±20%).
   - Comete: N punti casuali con scia orientata nella direzione di colata (lunghezza 10–60 px, densità
     decrescente).
   - Oyster lines: isolinee di `h` (frazione armonica `sin(2π·h·k)`) solo vicino al bordo di uscita.
   - Bordo: maschera di copertura irregolare (rumore frattale sul contorno) e angolo scoperto, dove si vede il
     supporto (vetro nero o ferro).
5. Ottica (opzionale, condivisa con altri processi): vignettatura e swirl.

### Controlli
| Controllo | Fisica | Intervallo |
|---|---|---|
| Supporto | ambrotipo / ferrotipo / negativo su vetro | scelta |
| Sensibilità UV | quanto bianco va il cielo | 0–1 |
| Colata | irregolarità di spessore, bordi, angolo scoperto | 0–1 |
| Difetti (comete e velo) | densità degli artefatti | 0–1 |
| Esposizione (stop) | posa reale, in stop | −3…+3 |

---

## 5. Carta salata e albumina (1840–1900 circa)

### Come funziona [gen]
- Carte ad annerimento diretto (printing-out). Il cloruro d'argento si annerisce sotto la luce del sole in
  contatto con il negativo, senza sviluppo. Poi si vira (oro) e si fissa.
- **Auto-mascheratura**: l'argento che si forma nelle ombre scherma l'emulsione sottostante. Le ombre "rallentano"
  mentre le alte luci continuano a crescere. Ne viene una scala tonale lunghissima con spalla morbida, che accetta
  negativi con intervallo di densità di ~1.8–2.2.
- Carta salata: opaca, immagine *dentro* le fibre, Dmax ≈ 1.1–1.4, tono rosso-bruno (non virata) o
  bruno-violaceo (virata all'oro).
- Albumina: strato di albume lucido, immagine *in superficie*, Dmax ≈ 1.6–2.0, toni bruno-porpora o
  bruno-cioccolato con viraggio all'oro.
- **Invecchiamento**: ingiallimento dell'albumina, che colpisce soprattutto le alte luci con Dmin che cresce in
  blu. Sbiadimento delle alte e medie luci, perché l'argento fine è facile da ossidare e solforare. Le ombre
  virano verso giallo-bruno e il contrasto si perde. Le superfici di albumina sviluppano una crettatura fine.
  Specchiatura d'argento ai bordi delle ombre.

### Modello [mod]
1. Dalla scena al negativo virtuale: pesi blu-sensibili (collodio) o ortocromatici a seconda dell'epoca. Curva
   negativa γ_n ≈ 1.0–1.3, densità del negativo `Dn`.
2. Stampa ad annerimento diretto con auto-mascheratura, risolta numericamente in pochi passi per pixel:
   `dD/dt = k · I · 10^(−Dn) · 10^(−β·D) · (Dmax − D)/Dmax`, con β ≈ 0.5–1.0. Integrata per un tempo T dà
   un'ottima approssimazione della curva tipica: lunga e con spalla morbida. In alternativa si precalcola una
   LUT 1D `Dn → D`.
3. Colore: `RGB = carta · 10^(−D · K)`. Vettori K normalizzati (assorbimento relativo R, G, B) [mod]:
   - salata non virata: K ≈ (0.70, 0.95, 1.10), rosso-bruno
   - salata virata oro: K ≈ (0.85, 1.00, 1.05), bruno-viola
   - albumina virata: K ≈ (0.88, 1.00, 1.02), con lucentezza superficiale (micro-contrasto) maggiore
4. Invecchiamento, con parametro età `a` tra 0 e 1:
   - `D_eff = D · (1 − a·f(D))`, con f maggiore per D bassi e medi (sbiadimento delle alte luci).
   - Dmin carta: `+a·(0.02, 0.06, 0.18)` di densità (ingiallimento).
   - K ruota verso il giallo-bruno: K_aged = mix(K, (0.6, 0.85, 1.25), a).
   - Albumina: rete di crettatura (Voronoi con bordi sottili) a densità molto bassa.

### Controlli
- **Tipo di carta**: salata / albumina
- **Viraggio oro** 0–1
- **Età** 0–1
- **Intervallo del negativo** (contrasto) 1.2–2.4 D

---

## 6. Cianotipia (1842–)

### Come funziona
- Ferro(III) ammonio citrato più ferricianuro di potassio. La luce riduce Fe³⁺ a Fe²⁺, che forma blu di Prussia
  insolubile [gen]. È sensibile all'UV e al blu (circa 300–450 nm [gen]). Si esponeva al sole o sotto lampada UV
  per ~20–30 minuti, fino a un'immagine visibilmente "chiusa", cioè con forte sovraesposizione voluta
  [doc: 6].
- Il processo tradizionale accetta negativi con intervallo di densità di solo ~1.0–1.2, circa metà dei ~2.4 della
  "New Cyanotype" e di altri processi al ferro [doc: 6].
- Circa metà della scala tonale si perde nel lavaggio, perché il blu di Prussia delle alte luci se ne va. Le ombre
  appaiono invertite (solarizzate) prima dell'asciugatura. Le alte luci sembrano verdi per il sensibilizzatore
  giallo residuo. L'ossidazione all'aria riporta le ombre al blu pieno [doc: 6].
- Sensibilità agli alcali: l'acqua di lavaggio non deve essere alcalina. Gli alcali sbiancano il blu di Prussia
  [doc: 6]. [gen] Sbiadisce alla luce e si rigenera al buio, in parte.

### Modello [mod]
1. Per "stampa da negativo" si usa una luminanza, o i pesi ortocromatici per un look da negativo d'epoca. Il
   negativo virtuale è compresso nell'intervallo ~1.1 D.
2. Curva di stampa ripida e corta (γ_print ≈ 1.6–2.2 rispetto al log dell'esposizione), con spalla netta e
   perdita delle alte luci: soglia sotto la quale il pigmento si lava via (`D = max(0, D − D_wash)`, D_wash
   ≈ 0.08–0.15).
3. Colore Beer-Lambert del blu di Prussia: K ≈ (1.00, 0.62, 0.22) (assorbe nel rosso, trasmette il blu). Dmax
   ≈ 1.4–1.8 [mod]. Carta: bianco caldo. Viraggi (tè/tannino più carbonato): K verso il bruno, (0.9, 1.0, 1.1).
4. Residuo giallo nelle alte luci: `+ (1 − ρ) · r_y · (0, 0.02, 0.08)` di densità.
5. Texture della carta e bordo pennellato del sensibilizzatore: maschera irregolare con pennellate, dove fuori
   maschera c'è carta bianca.

### Controlli
- **Esposizione UV** (−2…+2 stop): con sovraesposizione si "chiude" e le ombre si fondono
- **Lavaggio**: perdita delle alte luci 0–1
- **Viraggio** (blu → nero-violaceo → bruno) 0–1
- **Pennellata del bordo** 0–1

---

## 7. Platino e palladio, gomma bicromatata, carbone, Woodburytype

### Platino e palladio [gen]
- Sali di ferro più sale di platino o palladio, immagine di metallo nobile *nelle fibre* (opaca). Scala molto
  lunga: accetta negativi con intervallo ~1.8–2.4. Dmax tipico ~1.4–1.6 (carta non patinata). Straordinaria
  separazione delle ombre e permanenza quasi illimitata.
- Tono: platino neutro-freddo, nero-grigio. Palladio bruno caldo. Le miscele stanno nel mezzo. Il contrasto si
  regolava con clorato o dicromato.
- Effetti: superficie opaca senza bronzatura, nessuna lucentezza. Bordo pennellato. Una leggera "ombra" di
  diffusione nelle fibre.
- **Modello** [mod]: curva di stampa logistica lunga, γ ≈ 0.6–0.8 sul log del negativo, piede molto lungo.
  K_Pt ≈ (0.98, 1.0, 1.04), K_Pd ≈ (0.78, 0.95, 1.12). Blur di diffusione nella fibra di 0.5–1.5 px.
  Controlli: **Platino ↔ Palladio** (0–1), **Contrasto (dicromato)** (0–1), **Carta** (liscia/ruvida),
  **Pennellata** (0–1).

### Gomma bicromatata [gen]
- Pigmento in gomma arabica più dicromato. La luce indurisce. Lo sviluppo in acqua toglie la gomma non indurita.
  Ogni strato ha Dmax basso (~0.3–0.7) e scala corta. Si sovrappongono 2–6 strati, spesso di colori diversi (anche
  CMY da separazioni, quindi tricromia a gomma).
- Artefatti: macchia di pigmento nelle alte luci, segni di pennello, grana della carta, distacchi ai bordi,
  registro imperfetto tra strati.
- **Modello** [mod]: N strati con pigmento P_i (vettore K), curva corta σ con γ alto, `D_i = d_i · σ(...)`.
  Somma di densità. Rumore di pennellata su d_i. Spostamento casuale di registro per strato (0–3 px). Macchia di
  Dmin per strato (0.02–0.08). Controlli: **Numero di strati** (1–6), **Pigmento** (palette), **Macchia** (0–1),
  **Registro** (px).

### Stampa al carbone [gen]
- Tessuto di pigmento (nerofumo o altri) in gelatina bicromatata, esposto a contatto, trasferito e sviluppato in
  acqua calda. Rilievo di gelatina proporzionale alla densità. Dmax alto (~2.0 o più), lucido nelle ombre,
  colore a piacere (nero, seppia, rosso), permanente. Anche il trasferimento invertiva i lati.
- **Modello** [mod]: curva lunga e lineare, Dmax 2.0–2.4, K del pigmento. "Rilievo": micro-normali da `∂D`,
  per una lieve lucentezza differenziale nelle ombre. Controlli: **Pigmento**, **Rilievo** (0–1), **Dmax**.

### Woodburytype (1864–1900 circa) [gen]
- Rilievo di gelatina bicromatata pressato in piombo (stampo). Lo stampo, riempito di gelatina pigmentata, viene
  pressato su carta. Lo spessore del pigmento dà la densità. Tono continuo vero, senza retino né grana, colori
  tipicamente bruno-porpora, superficie con lieve lucido. Bordi rifilati perché si stampava con eccesso.
- **Modello** [mod]: `D = k · spessore`, con spessore lineare nel log dell'esposizione (curva quasi lineare,
  γ ≈ 1), nessuna grana. Pigmento K ≈ (0.80, 1.0, 1.08). Micro-rilievo come nel carbone. Controlli:
  **Pigmento**, **Spessore massimo** (Dmax 1.4–2.0), **Lucentezza**.

---

## 8. Ortocromatico e pancromatico in bianco e nero (1880 circa – oggi)

### Fatti
- Ortocromatico: sensibile a blu e verde e in parte al giallo, quasi cieco ad arancio e rosso. Gli oggetti rossi
  diventano quasi neri [doc: 3].
- Pancromatico convenzionale fino a ~650 nm. I primi pancromatici risalgono ai primi del Novecento, la
  diffusione amatoriale agli anni '30. Il tipo B (meno rosso) va distinto dal tipo C [doc: 3].
- [gen] Il cinema muto girava su ortocromatico fino al 1926–28 circa, da cui il trucco con fondotinta giallastri
  o verdastri e rossetti marrone o blu per compensare. Gli occhi azzurri venivano quasi bianchi, il cielo bianco.
  Dal 1928 il pancromatico (Kodak Type II) e le luci al tungsteno hanno cambiato trucco e illuminazione.
  [doc parziale: 7, 8]
- [gen] Gamma da negativo cinematografico circa 0.6–0.7, stampa circa 2.5–2.8. Il prodotto, ~1.5–1.8 in
  proiezione, accentua il contrasto. Il grano dei negativi ortocromatici era fine perché la sensibilità era
  bassa.

### Modello [mod]
- Record singolo con i pesi della tabella in sezione 1, oppure, meglio, interpolazione continua tra curve
  spettrali. Il controllo "Estensione spettrale" fa scorrere il taglio rosso da 500 nm (blu-sensibile) a 590 nm
  (orto) a 650 nm (pan B) a 700 nm (pan C) a 720 nm e oltre (rosso esteso). Con la via rapida si interpola tra le
  righe della tabella.
- Filtri di presa (giallo K2, arancio, rosso 25, verde X1): moltiplicazione per la trasmissione del filtro sui
  canali prima della somma pesata, più una compensazione di esposizione automatica (fattore filtro).
- Negativo, poi stampa, con due curve H&D. Grana come rumore dipendente dalla densità, dimensione legata alla
  sensibilità.

### Controlli
- **Sensibilità spettrale** (taglio): 500–720 nm, con preset Blu / Orto / Pan B / Pan C
- **Filtro di presa**: nessuno / giallo / arancio / rosso / verde
- **Contrasto di stampa** (γ di stampa) 1.8–3.2
- **Grana** 0–1

---

## 9. Sintesi additiva: Maxwell 1861

- Maxwell proiettò il 17 maggio 1861 alla Royal Institution tre diapositive (contatti dai negativi al collodio di
  Sutton), ciascuna attraverso il filtro con cui era stata presa, e le sovrappose. Le lastre originali sono al
  Cavendish Laboratory [doc: 1].
- Sutton usò filtri a soluzioni coloranti, descritte in modo vago. Il collodio non registra sopra ~530 nm, quindi
  la "separazione rossa" non poteva registrare il rosso. Evans (1961) propose che fosse UV riflesso dai coloranti
  rossi del nastro. Mokhtari et al. sostengono invece che tra ~290 e 470 nm i filtri colorati si comportino quasi
  come filtri neutri: le tre lastre differirebbero soprattutto per *esposizione*, non per contenuto spettrale
  [doc: 1].
- **Modello** [mod], "Maxwell 1861": tre record tutti blu-sensibili (pesi del collodio) con fattori di esposizione
  diversi e un piccolo contributo cromatico residuo. `R_rec = 0.4·E_blu + ε_R·(R_scena nel filtro)` ecc., poi
  sintesi additiva con primarie da proiezione sature. Il risultato è un'immagine dai colori imprecisi, con
  dominanti e il rosso quasi assente, coerente con l'esperimento. Controlli: **Fedeltà cromatica** (0 = ipotesi
  "solo esposizione", 1 = separazione ideale), **Registro** (px), **Bilanciamento delle lastre**.

---

## 10. Prokudin-Gorskij (1905–1915 circa): tre esposizioni sequenziali

### Fatti
- Lastra di vetro singola, oblunga, ~9 × 24 cm (formato Miethe-Bermpohl), con tre esposizioni in sequenza
  attraverso filtri blu, verde e rosso [doc: 9, 10]. La Library of Congress la descrive come circa 3 × 9 pollici,
  in verticale [doc: 11].
- La camera Miethe-Bermpohl (in commercio dal 1903) esponeva in ordine **blu → verde → rosso**. La camera
  personale di Prokudin-Gorskij non è documentata [doc: 9].
- Le pose andavano da 1 a 3 s secondo una lettera a Tolstoj, 6 s in una giornata di sole secondo un ricordo
  successivo. La sequenza completa (tre pose più due spostamenti del portalastre) secondo Agüera y Arcas
  superava il minuto. Dentro gli edifici si poteva arrivare a dieci minuti o più [doc: 9, 12].
- Miethe aveva migliorato molto la pancromaticità delle emulsioni per questo metodo [doc: 9]. [gen] La
  sensibilizzazione era probabilmente con etil-rosso o altri sensibilizzatori; il record rosso restava il più
  lento.
- Artefatti documentati [doc: 12]:
  - movimento di soggetto o camera tra la prima e la terza posa, da cui "ghosting" con sfrangiature colorate
    (teste di cammello, bambini, tappeti, acqua, fumo e nuvole con lampi di colore);
  - lastre crepate, muffe, graffi, polvere, impronte, degrado dell'emulsione (nomi da conservatori: "butterfly
    wing", "fern life", "frost on a windowpane");
  - zone piatte di colore e "cunei rosa".
  Le versioni moderne differiscono molto tra loro: il "colore vero" non è univoco [doc: 12].
- Nelle ricomposizioni di Agüera y Arcas si vedono bande di colore spesse ai bordi del fotogramma, che si
  sfilacciano in fili rossi, blu e verdi [doc: 12].

### Modello [mod]
1. **Tre esposizioni nel tempo**: `B_rec = f_B(frame(t))`, `G_rec = f_G(frame(t + Δ))`,
   `R_rec = f_R(frame(t + 2Δ))`, con Δ in fotogrammi (0–48). In OFX si chiedono i fotogrammi al clip sorgente.
   Per le immagini fisse o i clip statici si simula il movimento con un'opzione "Vento" (flusso procedurale
   applicato alle zone ad alta frequenza come foglie e acqua).
   - Ogni record integra a sua volta la propria posa (1–6 s, cioè N fotogrammi). Acqua e nuvole diventano così
     morbide *e* colorate in modo diverso per record.
2. **Filtri e pesi** [mod], cioè filtri tricromatici stretti su un'emulsione pancromatica precoce:
   - B: (0.02, 0.16, 0.82)
   - G: (0.06, 0.82, 0.12)
   - R: (0.88, 0.12, 0.00)
   Ogni record ha il suo fattore di esposizione, con l'errore di bilanciamento come parametro.
3. **Curva per lastra**: γ ≈ 1.0, con densità di velo e Dmax che variano per lastra (±0.1 D), più vignettatura
   indipendente.
4. **Misregistrazione** (spostamento meccanico del portalastre): trasformazione affine per record, con
   traslazione di 0–20 px, rotazione di 0–0.3° e scala di 1 ± 0.002 (lastre diverse, messa a fuoco).
   Bordo: ogni record ha la sua cornice. L'unione produce le "bande colorate" ai margini (si possono mostrare
   o ritagliare).
5. **Ricomposizione**: additiva sulle primarie del proiettore, oppure "stampa CMY moderna" (le restituzioni LoC).
   Variante "proiezione d'epoca": saturazione più bassa e toni smorzati, perché le stampe dell'epoca erano più
   tenui delle restituzioni moderne [doc: 12].
6. **Danni**: crepe (linee frattali che separano blocchi con leggero spostamento), muffa (macchie frattali con
   densità e colore *in un solo record*, che è la causa dei colori "falsi" locali), polvere e graffi per record.
   I difetti in un solo canale producono macchie colorate saturo-complementari, molto caratteristiche.

### Controlli
| Controllo | Fisica | Intervallo |
|---|---|---|
| Intervallo tra le lastre | tempo tra le pose (fotogrammi) | 0–48 |
| Registrazione delle lastre | errore di allineamento | 0–20 px (più rotazione derivata) |
| Bilanciamento delle lastre | errore di esposizione per record | ±1 stop (tre sotto-cursori o "casuale") |
| Danni alla lastra | crepe, muffa, polvere per canale | 0–1 |
| Stampa d'epoca / moderna | ricomposizione | scelta |

---

## 11. Autochrome Lumière (1907–1930 circa; su pellicola dal 1931)

### Fatti
- Grani di fecola di patata tinti **arancio-rosso, verde e blu-violetto**, mescolati in modo che a occhio nudo il
  mosaico sembri grigio. Uno strato solo, circa 4 milioni di grani per pollice quadrato, interstizi riempiti di
  nerofumo, rullatura ad alta pressione per appiattire i grani e aumentare la trasmissione [doc: 13].
- Dimensione dei grani: 5–10 µm secondo una fonte [doc: 13], 10–20 µm secondo una ricetta del Met [doc: 14].
  Conviene parametrizzare.
- Coloranti (ricetta riportata) [doc: 14]: arancio = eritrosina + rosa bengala + tartrazina; verde = blu patent
  + tartrazina; blu-violetto = cristalvioletto + verde malachite. Colori e proporzioni variavano tra lotti di
  produzione [doc: 15].
- Emulsione pancromatica in bianco e nero sopra il mosaico, protetta da gommalacca. Filtro arancio-giallo in
  camera per tagliare l'UV e ridurre l'eccesso di sensibilità al blu e violetto. Lato vetro verso l'obiettivo.
  Pose molto più lunghe del bianco e nero, quindi treppiede [doc: 13].
- Inversione: sviluppo a negativo senza fissaggio, rimozione dell'argento negativo, riesposizione e sviluppo
  dell'alogenuro residuo, da cui un positivo diretto trasparente [doc: 13].
- Forte perdita di luce attraverso il mosaico [doc: 13]. [gen] Trasmissione complessiva del bianco dell'ordine di
  pochi per cento, che rende le immagini scure se non molto illuminate. Pose dell'ordine di 1 s o più al sole a
  f/8, circa 40–60 volte un bianco e nero coevo.
- [gen] I grani dello stesso colore si raggruppano casualmente in "grumi" di molti grani: la texture visibile è
  più grossa del singolo grano e ha un aspetto puntinista. Colori pastello, rossi caldi, verdi ottimi. Neri non
  profondi, gialli scuriti.

### Implicazioni di scala [mod]
4·10⁶ grani/in² ≈ 6200 grani/mm², quindi un passo medio di ~12.7 µm. Su una lastra 9 × 12 cm la larghezza è
90 mm, circa 7000 grani. A 3840 px il singolo grano è sotto il pixel (~0.55 px). La texture percepita viene dai
grumi (3–10 grani, cioè 2–6 px a 4K). Il controllo deve essere "Formato della lastra" o "Scala del mosaico",
non "dimensione del grano in px".

### Modello [mod]
1. **Mosaico procedurale**: punti Poisson-disk con raggio r (diametro 5–20 µm in scala della lastra), più
   tessellazione di Voronoi con bordo nero di spessore s (nerofumo, frazione d'area 5–15%). Ogni cella riceve un
   colore da un campo di rumore a bassa frequenza: tre classi con probabilità p_o, p_g, p_v (circa 0.33 ciascuna,
   con varianza di lotto) e un parametro di aggregazione (correlazione spaziale delle etichette) per ottenere i
   grumi. A risoluzione sub-pixel si renderizza con supersampling 4×4, oppure si calcola analiticamente la
   frazione d'area per pixel.
2. **Ripresa**: per ogni cella k, `E = Σ_c RGB_c · Tf_k,c · Tcam_c` (Tcam è il filtro giallo-arancio di
   compensazione), poi una curva H&D di inversione su emulsione pancromatica precoce: `T_argento = 1 − σ(...)`.
   Trasmissioni Tf dei grani in lineare [mod]:
   - arancio-rosso ≈ (0.62, 0.10, 0.02)
   - verde ≈ (0.06, 0.48, 0.08)
   - blu-violetto ≈ (0.05, 0.04, 0.50)
   Larghe e sovrapposte: da qui la desaturazione tipica.
3. **Visione**: `RGB_out(px) = Σ_celle area · T_argento · Tf_k · L_luce`. La somma include il nero degli
   interstizi. Normalizzazione al bianco per compensare la bassa trasmissione, ma con un controllo "Luce di
   visione" che lascia l'immagine scura come sul visore o al proiettore.
4. **Vernice e invecchiamento**: ingiallimento della vernice (+Dmin in blu), sbiadimento dei grani
   (la desaturazione viola sbiadisce prima [gen]), crepe del vernice-mosaico.
5. Grana d'argento dell'emulsione: secondario rispetto al mosaico.

### Controlli
| Controllo | Fisica | Intervallo |
|---|---|---|
| Scala del mosaico | formato della lastra / grandezza del grano | 9×12 cm … 13×18 … "ravvicinato" (2–20 µm) |
| Aggregazione dei grani | correlazione dei colori, grumi puntinisti | 0–1 |
| Purezza dei coloranti | sovrapposizione delle trasmissioni | 0–1 |
| Luce di visione | trasmissione assoluta | 0–1 (1 = normalizzata) |
| Invecchiamento della vernice | ingiallimento e sbiadimento | 0–1 |

---

## 12. Dufaycolor, Finlay, Paget (reti regolari)

### Dufaycolor (cinema 1932, foto 1935 – fine anni '50)
- Réseau di **linee rosse** alternate a file di **quadrati verdi e blu**, sotto un'emulsione pancromatica, sullo
  stesso lato del supporto. Circa un milione di elementi per pollice quadrato. Assorbe gran parte della luce di
  proiezione [doc: 16].
- Dati quantitativi (Hubička et al. 2024) [doc: 15]:
  - 19–25 linee/mm (480–635 lpi)
  - lato degli elementi blu e verdi ≈ 28.9 µm
  - copertura d'area: rosso 42.1%, verde 26.5%, blu 31.4%
  - linee a ~23° rispetto al bordo della pellicola
  - fabbricazione: base di collodio tinta in blu, stampa di linee, sbiancamento e tintura verde, poi seconda
    stampa ortogonale con sbiancamento e tintura rossa
- Cromaticità degli elementi (Cornwell-Clyne 1951) [doc: 15]:

| Elemento | x | y | Y | λ dominante |
|---|---|---|---|---|
| Rosso | 0.633 | 0.365 | 17.7% | 601.7 nm |
| Verde | 0.233 | 0.647 | 43% | 549.6 nm |
| Blu | 0.14 (corretto: 0.164) | 0.089 (corretto: 0.164) | 3.7% (corretto: 8.7%) | 466.0 nm |

  Gli autori propongono la correzione del blu per ottenere un insieme neutro. Illuminante di riferimento B o
  D65 [doc: 15]. La scelta dei dati storici conta meno della nitidezza della scansione: ΔE2000 medio < 1.18 tra
  le diverse specifiche [doc: 15].
- **Scala nel fotogramma** [mod]: un fotogramma 35 mm muto o sonoro è largo ~22 mm, quindi ~420–550 linee per
  larghezza. A 4K sono 7–9 px per linea, cioè un reticolo *visibile*. Per il 16 mm la larghezza è ~10 mm, circa
  200–250 linee, 15–19 px per linea a 4K.

### Modello Dufaycolor [mod, primarie da doc]
1. Reticolo procedurale ruotato di 23°: righe rosse (spessore che dà il 42.1% d'area) alternate a righe di
   quadrati G e B alternati.
2. Ripresa: ogni elemento filtra la scena. Le trasmissioni si derivano dalle xyY sopra, convertite in XYZ e poi
   in RGB lineare (D65). Ne viene una trasmissione larga; l'emulsione pancromatica fa da record unico.
3. Inversione (reversal) con H&D, poi visione con le stesse primarie e area. Poiché il bianco del reticolo ha Y
   medio ≈ 0.421·0.177 + 0.265·0.43 + 0.314·0.087 ≈ 0.217, l'immagine proiettata ha circa il 22% di trasmissione
   del bianco [mod, calcolato da doc: 15].
4. Moiré, dato che il reticolo viene ricampionato: renderizzare l'area esatta per pixel (box filter analitico) e
   lasciare un controllo per il "moiré di scansione" voluto.

### Finlay (anni '29–'50) e Paget (1913 circa) [gen]
- Reti regolari a scacchiera (quadrati rossi, verdi e blu) su lastra separata: lo schermo di presa e lo schermo
  di visione erano distinti e andavano allineati. Ne venivano moiré e frange se disallineati. Paget aveva
  dimensioni simili, ~ 50–100 elementi/mm [gen].
- **Modello**: come il Dufay, con geometria a scacchiera e un parametro "Disallineamento dello schermo di visione"
  (offset e rotazione tra schermo di presa e di visione), che genera moiré e dominanti locali.

### Controlli (rete)
- **Tipo di rete**: Dufay / Finlay / Paget
- **Formato**: 35 mm / 16 mm / lastra (scala del reticolo)
- **Luce di visione** 0–1
- **Allineamento dello schermo** (Finlay/Paget): 0–2 px e 0–1°
- **Sbiadimento del réseau** 0–1

---

## 13. Kinemacolor e Prizma (bicromia additiva sequenziale)

### Kinemacolor (1906–1915)
- Ideato da G. A. Smith (1906), brevettato nel 1907, in uso commerciale 1909–1915, prima proiezione pubblica il
  26 febbraio 1909, brevetto revocato nell'aprile 1915 [doc: 17].
- Ripresa a **32 fotogrammi/s** (il doppio dei 16 usuali), con proiezione confermata a 30–32 fps. Filtro rotante
  in alluminio a quattro settori: due aperti, uno di gelatina rossa e uno verde. Pellicola pancromatica in bianco
  e nero. Gli operatori sceglievano filtri rosso/arancio e blu/verde secondo il soggetto [doc: 17].
- Filtri di proiezione fissi: rosso equivalente a "25 Sunset Red" (picco di trasmissione sopra 610 nm), verde
  equivalente a "122 Fern Green" (picco ~510–540 nm) [doc: 17].
- Artefatti: affaticamento visivo, parallasse temporale tra fotogrammi [doc: 17]. [gen] Frange rosse e verdi sui
  bordi in movimento ("color fringing"). Assenza di blu e viola: il cielo blu diventa bianco o verdastro, il viola
  diventa rosso scuro.

### Prizma [gen]
- Prizma Color (W. Van Doren Kelley), 1913–1917: additivo a filtro rotante, poi Prizma II (dal 1918), sottrattivo
  bicromo su pellicola a doppia emulsione, con un lato tinto rosso-arancio e l'altro blu-verde.

### Modello Kinemacolor [mod]
1. Due record a istanti diversi: `R_rec = f(frame(t))` con il filtro rosso, pesi (0.92, 0.08, 0.00). Poi
   `G_rec = f(frame(t + 1/32 s))` con il filtro verde, pesi (0.05, 0.75, 0.20). A 24 fps l'istante intermedio si
   ottiene con un'interpolazione a flusso ottico, oppure in modo semplice con `frame(t+1)` scalato dal controllo
   "Sfasamento temporale" (0–1 fotogrammi).
2. Due curve H&D (reversal o stampa positiva), con γ alto ~1.6–2.0.
3. Proiezione: `RGB_out = R_rec · P_rosso + G_rec · P_verde`, con P_rosso ≈ (0.85, 0.08, 0.03) e P_verde ≈ (0.10,
   0.75, 0.40) in lineare [mod da doc: 17 per le lunghezze d'onda]. Il bianco risultante è giallo-crema; va
   bilanciato con un controllo di compensazione.
4. Opzione "sfarfallio": modulare l'uscita alternando i due record ogni mezzo fotogramma. Resta sconsigliata per
   la fotosensibilità.

### Controlli
- **Sfasamento temporale** 0–1 fotogrammi
- **Filtro di presa**: rosso-arancio ↔ rosso profondo, verde-blu ↔ verde
- **Bianco di proiezione** (compensazione) 0–1
- **Contrasto di stampa** 1.2–2.2

---

## 14. Technicolor bicromo: processi 1, 2, 3 (1916–1933)

- **Processo 1** (1916): additivo bicromo. Prisma divisore dietro l'obiettivo, due fotogrammi consecutivi esposti
  dietro filtri rosso e verde. Ripresa e proiezione a velocità doppia, proiettore a due finestre con allineamento
  continuo [doc: 18].
- **Processo 2** (1922): sottrattivo. I due record stampati su strisce separate, *virati* arancio-rosso (immagini
  prese col verde) e ciano-verde (immagini prese col rosso), poi incollati dorso a dorso. Il viraggio sostituisce
  l'argento con materiale colorato: alte luci limpide, ombre fortemente colorate. Problemi: incurvamento
  ("cupping") con perdita di fuoco imprevedibile, graffi vistosamente colorati, giunte deboli [doc: 18].
- **Processo 3** (1928): imbibizione (dye transfer) bicroma. Il fotogramma preso col rosso è tinto ciano-verde,
  quello preso col verde arancio-rosso. Matrici di gelatina indurita in rilievo, trasferimento su pellicola
  "blank" mordenzata. Le copie sonore usavano una blank in bianco e nero con colonna e linee di fotogramma
  prestampate [doc: 18]. Ogni passaggio teneva matrice e blank in contatto circa due minuti, su nastro a perni per
  il registro [doc: 19].

### Modello bicromo sottrattivo [mod]
1. Record rosso `Rr = (0.90, 0.10, 0.00)·RGB` e record verde `Gr = (0.05, 0.70, 0.25)·RGB`. Il filtro verde di
   presa passava anche parte del blu [gen].
2. Negativo (γ ~0.7), poi matrice/positivo (γ ~2.2): densità di colorante `D_cg` dal record rosso (più rosso
   nella scena, meno ciano-verde) e `D_or` dal record verde.
3. Coloranti, matrice di assorbimento A (righe = colorante, colonne = R, G, B) [mod]:
   - ciano-verde: (1.00, 0.30, 0.45)
   - arancio-rosso: (0.04, 0.85, 1.00)
4. `T = 10^(−(D_cg·A_cg + D_or·A_or))`. Il risultato: incarnati rosa-arancio, cieli ciano-turchese, nessun giallo
   puro né viola, verdi tendenti al turchese. È il look storico dei musical 1929–1932 [gen].
5. Processo 2: "cupping", con blur variabile nel tempo a bassa frequenza (perdita di fuoco a ondate); graffi
   colorati su uno strato soltanto (ciano o arancio a seconda del lato).

### Controlli
- **Processo**: 1 (additivo) / 2 (incollato) / 3 (imbibizione)
- **Densità dei coloranti** 0.5–1.5
- **Registro** tra i due record 0–4 px
- **Incurvamento** (solo P2) 0–1

---

## 15. Technicolor tricromo, processo 4 (1932–1955) e dye transfer (1952–1975 / 1997–2002)

### Camera e registrazione
- Camera costruita da Mitchell con divisore di fascio: superficie semiriflettente in un cubo prismatico diviso.
  Circa **1/3 della luce** passa dritta alla striscia con filtro verde. I **2/3 riflessi** attraversano un
  **filtro magenta** e raggiungono due pellicole a contatto emulsione-contro-emulsione (bipack): davanti
  un'ortocromatica cieca al rosso che registra il **blu**, con uno strato **rosso-arancio** sulla sua emulsione
  che blocca il blu verso la pellicola posteriore, una pancromatica che registra il **rosso** [doc: 18, 20, 21].
- La superficie riflettente del prisma era all'inizio dorata a puntini ("gold-flecked"), poi argentata. Lo strato
  rosso-arancio si scioglieva nello sviluppo. Rosso e blu, attraversando la base della pellicola anteriore, sono
  *leggermente meno nitidi* del verde, anche se la differenza è minima. Il record rosso veniva ribaltato
  otticamente nella stampa delle matrici [doc: 21].
- Sensibilità molto bassa, circa **ASA 5**, più la perdita del divisore: illuminazione molto superiore al bianco e
  nero, con temperature sopra i 100 °F sul set di *Il mago di Oz* [doc: 18]. Lampade ad arco al carbone a fiamma
  bianca. Intervallo di contrasto ~1:32 per le scene dipinte, ~1:64 per la proiezione. Celluloide Kodak a basso
  ritiro (~¼ dell'1%) [doc: 22]. Cinque obiettivi Taylor-Hobson, 35–140 mm [doc: 22].

### Stampa
- Tre negativi in bianco e nero, poi tre matrici di gelatina in rilievo (positivi). Ogni matrice viene tinta e
  pressata sulla blank: **ciano** dal record rosso, **magenta** dal verde, **giallo** dal blu [doc: 18, 19].
  Ogni soluzione colorante era una miscela di più coloranti. Le matrici erano riutilizzabili fino a circa 50
  volte [doc: 19].
- **Immagine chiave K**: le prime copie a tre strisce erano prestampate con un'immagine in bianco e nero al
  **50%** dal record verde. Copriva le frange di colore e aumentava il contrasto, ma riduceva la saturazione. È
  stata eliminata nel **1944** [doc: 18].
- Ultimo film americano a tre strisce: *Foxfire* (1955). Ultimo britannico: *The Feminine Touch* (1956)
  [doc: 18, 19]. Dal 1952 le matrici si ricavavano direttamente da negativi Eastmancolor monopack [doc: 18]. Il
  dye transfer è andato avanti circa 20 anni con negativi monopack. Il laboratorio di Hollywood ha chiuso nel
  1975, quello britannico nel 1978 (venduto a Pechino, attivo fino al 1993), quello italiano nel 1980. Revival nel
  1997, chiuso nel 2002 [doc: 18, 19].
- Coloranti acidi stabili, considerati d'archivio, molto più stabili dei cromogeni Eastmancolor [doc: 18, 23].
  Le copie possono apparire più morbide delle cromogene moderne. Colonna sonora in emulsione in bianco e nero
  [doc: 24].

### Perché "sembra Technicolor" [gen + dedotto]
- Separazione spettrale con filtri stretti e indipendenti: nessun crosstalk tra strati come nel monopack, dove
  gli strati si sovrappongono spettralmente e i coloranti hanno assorbimenti indesiderati non mascherabili. Le
  curve di ogni record si regolano separatamente (gamma per matrice). Il risultato è una saturazione altissima
  dei primari e una buona purezza dei gialli e dei rossi.
- I coloranti di imbibizione, pur con assorbimenti secondari, permettevano densità massime alte e neri profondi,
  anche grazie al K prima del 1944.
- Le scenografie e i costumi erano disegnati da Color Advisory (Natalie Kalmus) per il processo [gen].
- Le illuminazioni molto forti e la profondità di campo ridotta hanno dato il loro contributo [dedotto].

### Modello Technicolor 3-strip [mod]
1. **Separazioni** (pesi RGB lineari):
   - Verde (filtro verde, pancromatica): `Gs = (0.08, 0.84, 0.08)·RGB`
   - Blu (magenta più ortocromatica): `Bs = (0.00, 0.06, 0.94)·RGB`
   - Rosso (magenta più strato rosso-arancio più pancromatica): `Rs = (0.92, 0.08, 0.00)·RGB`
   Bipack: blur gaussiano su Rs e Bs di σ ≈ 0.4–1.0 px a 2K (diffusione attraverso la base) [mod da doc: 21
   qualitativo].
2. **Negativi**: γ_neg ≈ 0.6–0.7, curve per record con piede e velo propri. Una latitudine ridotta (ASA 5,
   1:64) suggerisce un piede corto.
3. **Matrici e stampa**: γ_mat per record (default circa 1.0 ciascuno, regolabili), prodotto complessivo γ_proiez
   ≈ 1.6–1.8. Densità di colorante `D_c`, `D_m`, `D_y` ∈ [0, Dmax_j], con Dmax_c ≈ 2.6, Dmax_m ≈ 2.8,
   Dmax_y ≈ 2.4 [mod].
4. **Matrice di assorbimento dei coloranti** (righe C, M, Y; colonne R, G, B, densità relative) [mod, valori
   tipici di coloranti acidi]:
   ```
   C: (1.00, 0.22, 0.08)
   M: (0.08, 1.00, 0.28)
   Y: (0.01, 0.07, 1.00)
   ```
   Per il monopack Eastmancolor anni '50 gli indesiderati sono maggiori: C (1.00, 0.32, 0.12), M (0.12, 1.00,
   0.40), Y (0.02, 0.12, 1.00) [mod]. Questa differenza è parte del confronto "Technicolor vs Eastmancolor" e il
   plugin può esporla.
5. **Chiave K** (prima del 1944): `D_k = 0.5 · f(Gs)` in densità neutra (argento: (1, 1, 1)) sommata agli
   altri. Con K = 0.5 si hanno contrasto e nero più forti, colori meno saturi, frange mascherate.
6. **Registro** di imbibizione: traslazione per colorante (0–3 px a 2K), più "breathing" lento nel tempo
   (sinusoide o rumore a bassa frequenza per fotogramma). Le frange appaiono sui bordi ad alto contrasto, in
   colori primari sottrattivi (ciano/rosso, magenta/verde).
7. **Proiezione**: `T = 10^(−(D·A))`, per lo spettro della lampada ad arco o allo xeno (bianco leggermente
   blu). Conversione all'RGB di uscita.
8. Variante "dye transfer da monopack" (1952–1975, 1997–2002): al posto delle tre strisce, le separazioni si
   fanno con la sensibilità del negativo Eastmancolor (più larga e sovrapposta, crosstalk): `Rs = (0.80, 0.18,
   0.02)`, `Gs = (0.12, 0.75, 0.13)`, `Bs = (0.02, 0.18, 0.80)` [mod]. Senza K.

### Controlli
| Controllo | Fisica | Intervallo |
|---|---|---|
| Epoca | 1935–1944 con chiave K / 1945–1955 / dye transfer da monopack | scelta |
| Chiave K | opacità dell'immagine argento dal record verde | 0–100% (default 50% per la prima epoca) |
| Densità dei coloranti | scala Dmax per C, M, Y (globale più tre fini) | 0.6–1.4 |
| Registro delle matrici | errore di imbibizione | 0–4 px, più "oscillazione" 0–1 |
| Contrasto delle separazioni | γ_matrice per record | 0.7–1.4 |

---

## 16. Eastmancolor anni '50–'70 e il suo sbiadimento

- Kodak introduce il negativo colore Eastmancolor 35 mm nel 1950. *Royal Journey* (1951) è il primo
  lungometraggio [doc: 18].
- Al buio, per i negativi e le copie Eastmancolor 1950–1981, **ciano e giallo sbiadiscono più rapidamente del
  magenta**. Ne risultano dominanti forti, perdita di contrasto e dettaglio nelle alte luci [doc: 23]. Nelle copie
  Eastman di solito sbiadisce prima il ciano, poi il giallo, e il magenta è il più stabile: le copie sbiadite
  appaiono rosse o magenta. Le pellicole di altre marche sbiadiscono verso colori diversi [doc: 25].
- La temperatura di conservazione è il fattore critico, l'umidità conta meno. Il degrado della base (sindrome
  dell'aceto, nitrato) abbassa il pH e accelera lo sbiadimento [doc: 23]. Dalla metà degli anni '80 le
  cromogene raggiungono la massima stabilità [doc: 23]. [gen] Kodak "LPP" (Low Fade Print) dal 1982 circa.

### Modello [mod]
1. Base: catena monopack (separazioni con crosstalk, γ negativo 0.6, stampa 2.6–3.0, matrice degli assorbimenti
   indesiderati "monopack" della sezione 15).
2. **Sbiadimento**, con età `a` tra 0 e 1: `D_c' = D_c · (1 − a·0.85)`, `D_y' = D_y · (1 − a·0.55)`,
   `D_m' = D_m · (1 − a·0.12)`. È non lineare: lo sbiadimento colpisce di più le densità basse e medie (alte
   luci), `f(D) = 1 − exp(−D/D0)`.
3. Macchia gialla di Dmin (aumento di D_y in Dmin, base ingiallita) `+a·0.08`, e perdita di Dmax complessiva.
4. Sbiadimento disuniforme lungo la bobina e ai bordi: rumore a bassa frequenza su `a` (opzionale).

### Controlli
- **Età** (0–1, oppure 0–60 anni con legge di Arrhenius semplificata)
- **Temperatura di conservazione** (5–30 °C, modula la velocità)
- **Ciano / Giallo / Magenta** (pesi relativi dello sbiadimento, per stock diversi)
- **Restauro digitale** (0–1): ricostruisce il bilanciamento perdendo latitudine, simulando un recupero tipo
  Pritchard [doc: 25]

---

## 17. Agfacolor, Sovcolor, Ferraniacolor, Gevacolor [gen]

- **Agfacolor Neu** (1936, invertibile; negativo-positivo dal 1939–40): primo monopack con *copulanti
  incorporati* negli strati (copulanti "sostantivi" con catene grasse anti-diffusione), a differenza del
  Kodachrome che li aggiungeva nello sviluppo. Dopo la guerra i brevetti diventano liberi e la tecnologia passa a:
  - **Sovcolor** (URSS, da impianti e know-how Agfa Wolfen) e Orwocolor (DDR, Agfa Wolfen dal 1964)
  - **Ferraniacolor** (Italia, primi anni '50)
  - **Gevacolor** (Belgio, Gevaert, fine anni '40)
  - Anscocolor (USA)
- Look tipico: saturazione più bassa e colori più "pastello" o "terrosi" del Technicolor. Verdi freddi, a volte
  cianotici. Incarnati bruno-arancio. Contrasto morbido. Grana evidente. Sbiadimento importante, spesso verso
  magenta o bruno, anche se le direzioni variano per stock e periodo [doc: 23 sul fatto che lo sbiadimento
  dipende dai copulanti].
- **Modello** [mod]: stessa catena monopack, con crosstalk spettrale *maggiore* (sensibilità degli strati più
  sovrapposte: `Rs = (0.74, 0.22, 0.04)`, `Gs = (0.14, 0.70, 0.16)`, `Bs = (0.03, 0.22, 0.75)`). Assorbimenti
  indesiderati più forti, senza mascheramento (niente maschera arancione negli Agfa negativi d'epoca [gen]).
  Contrasto di stampa 2.4. Grana maggiore. Direzione di sbiadimento selezionabile.
- **Controlli**: **Marca** (preset), **Purezza** (crosstalk 0–1), **Grana**, **Età**.

---

## 18. Elementi trasversali da implementare una volta

1. **Motore di densità**: curva H&D parametrica con piede e spalla, Beer-Lambert con matrice di coloranti,
   modalità additiva e monocroma tonata.
2. **Motore spettrale**: pesi (via rapida) o upsampling RGB→spettro con LUT precalcolate per record. Per un
   plugin real-time conviene precalcolare offline, per ogni processo, una LUT 3D `RGB_lin → (E_1, E_2, E_3)` in
   log, con un dominio esteso tramite shaper log2.
3. **Motore temporale**: integrazione di N fotogrammi (pose lunghe) e campionamento di fotogrammi sfalsati per
   record (Prokudin-Gorskij, Kinemacolor). In OFX servono la dichiarazione dei frame richiesti
   (`getFramesNeeded`) e una cache.
4. **Motore di registro**: affine per record, più oscillazione lenta, più blur per record (bipack).
5. **Motore di supporto**: texture di carta e fibra, mosaici Voronoi/Poisson (Autochrome), reticoli (Dufay,
   Finlay), bordi di colata e pennellata, crepe, muffe, polvere per canale.
6. **Motore di invecchiamento**: sbiadimento per colorante dipendente dalla densità, Dmin crescente,
   ingiallimento, tarnish a film sottile.
7. **Risoluzione-indipendenza**: tutte le dimensioni fisiche (grani, linee, blur) vanno in µm sul supporto
   virtuale, convertite in px in base al formato scelto e alla risoluzione del progetto. Altrimenti il look cambia
   tra proxy e render.

### Verifica (per la regola "verificare sul prodotto reale")
- Rendering di un ColorChecker sintetico (spettri noti) e confronto con:
  - le cromaticità Dufay pubblicate [doc: 15];
  - l'inversione dei rossi in orto e collodio, verificabile anche con foto reali tramite filtro BG25 come in
    [doc: 2].
- Test di regressione: immagini di riferimento per ogni processo, tolleranza in ΔE.
- Le curve spettrali reali (Mokhtari et al. [doc: 1], datasheet Kodak/Ilford [doc: 3]) vanno digitalizzate e
  usate per fittare i pesi [mod] di questo documento. Finché non è fatto, i pesi restano stime.

---

## Stato delle affermazioni

**Documentato in questa ricerca** (fonti 1–25):
- sensibilità degli alogenuri e limite a 530 nm del collodio;
- processo del dagherrotipo (iodio/bromo, cicli di colore, mercurio a 60 °C per 20 min, specchio e inversione,
  ritocchi) e natura plasmonica delle particelle;
- Prokudin-Gorskij: lastra 9×24, ordine B-G-R, pose 1–6 s, sequenza > 1 min, artefatti;
- Autochrome: grani, nerofumo, filtro di compensazione, inversione, coloranti;
- Dufaycolor: linee/mm, aree, cromaticità;
- Kinemacolor: 32 fps, filtri;
- Technicolor 1–4: prisma 1/3–2/3, bipack, ASA 5, chiave K 50% fino al 1944, date;
- sbiadimento Eastmancolor (ciano poi giallo);
- cianotipia: intervallo 1.0–1.2, perdita nel lavaggio.

**Conoscenza generale non verificata** [gen]:
- tempi di posa del dagherrotipo e del collodio;
- equivalenti ISO;
- Dmax di carta salata, albumina, platino e carbone;
- aspetto e meccanismi degli artefatti del collodio;
- Prizma, Finlay, Paget;
- famiglia Agfa e successori;
- trucco del cinema muto.

**Solo proposta** [mod]: tutti i pesi RGB, le matrici dei coloranti, i vettori K, i modelli di invecchiamento,
gli intervalli dei controlli.

---

## Fonti

1. Mokhtari et al., "Maxwell's disappointment and Sutton's accident" (UWE repository).
   https://uwe-repository.worktribe.com/OutputFile/10253233
2. PetaPixel, "Recreating the Look of 1850s Tintypes in Digital with Math and Science" (2019).
   https://petapixel.com/2019/04/05/recreating-the-look-of-1850s-tintypes-in-digital-with-math-and-science
3. 35mmc, "Spectral Sensitivity of B&W Film – A Deep Dive into Orthochromatic, Panchromatic and All the Rest"
   (2023). https://www.35mmc.com/02/05/2023/spectral-sensitivity-of-bw-film-a-deep-dive-into-orthochromatic-panchromatic-and-all-the-rest/
4. Luminous-Lint, "Daguerreotypes – technical details".
   https://luminous-lint.com/phoenix/techniques/details/daguerreotypes/_notes_technical_details/
5. Princeton Graphic Arts, sintesi di "How light is scattered by the metallic nanoparticles on the surface of a
   daguerreotype..." (PNAS 2019, doi:10.1073/pnas.1904331116).
   https://graphicarts.princeton.edu/2019/06/20/how-light-is-scattered-by-the-metallic-nanoparticles-on-the-surface-of-a-daguerreotype-determines-the-characteristics-of-its-image-such-as-shade-and-color/
6. Mike Ware, "The Traditional Cyanotype Process". https://mikeware.co.uk/mikeware/Traditional_Cyanotype.html
7. Silent Film Music, "Orthochromatic film and facial makeup in silent movies".
   https://silentfilmmusic.com/orthochromatic-film-facial-makeup-silent-movies/ (non letta per intero)
8. Cosmetics and Skin, "Early movie make-up". https://cosmeticsandskin.com/cdc/early-movie.php (non letta per
   intero)
9. Wikipedia, "Sergey Prokudin-Gorsky". https://en.wikipedia.org/wiki/Sergey_Prokudin-Gorsky
10. Library of Congress, Prokudin-Gorskii Collection, "Method".
    https://www.loc.gov/pictures/collection/prok/method.html
11. Library of Congress, "The Empire That Was Russia – Making the color composites".
    https://www.loc.gov/exhibits/empire/making.html
12. The Public Domain Review, "In Search of True Color: Sergei Prokudin-Gorsky's Flawed Images".
    https://publicdomainreview.org/essay/in-search-of-true-color/
13. Wikipedia, "Autochrome Lumière". https://en.wikipedia.org/wiki/Autochrome_Lumi%C3%A8re
14. Photrio, "Autochrome Recipe from Metropolitan Museum of Art".
    https://www.photrio.com/forum/threads/autochrome-recipe-from-metropolitan-museum-of-art.57136/
15. Hubička, Kimrová, Konečný, "Color-Screen: reconstruction of Dufaycolor colours" (arXiv 2501.06216, 2024).
    https://arxiv.org/pdf/2501.06216
16. Wikipedia, "Dufaycolor". https://en.wikipedia.org/wiki/Dufaycolor
17. Wikipedia, "Kinemacolor". https://en.wikipedia.org/wiki/Kinemacolor
18. Wikipedia, "Technicolor". https://en.wikipedia.org/wiki/Technicolor
19. George Eastman Museum, "Dye-Transfer Printing".
    https://www.eastman.org/technicolor/technology/dye-transfer-printing
20. George Eastman Museum, "Three-Strip Camera". https://www.eastman.org/technicolor/technology/three-strip-camera
21. American WideScreen Museum, "Technicolor" (three-strip camera).
    https://widescreenmuseum.com/oldcolor/technicolor7.htm
22. Technicolor / Academy Technicians Branch, "The Three-Color Camera – The Three-Color Process" (1935, PDF,
    George Eastman Museum). https://www.eastman.org/sites/default/files/technicolor/pdfs/Three-ColorCamera_TheThreeColorProcess.pdf
23. FilmCare (Image Permanence Institute), "Dye fade". https://filmcare.org/vd_dyefade.php
24. FilmCare, "Identifying Technicolor". https://filmcare.org/id_technicolor.php
25. Brian Pritchard, "Fading of motion picture film".
    https://brianpritchard.com/wp/british-motion-picture-laboratories/fading-of-motion-picture-film/

Fonti trovate ma non consultate, utili per il passo successivo:
- National Film Preservation Foundation, "Color dye fading".
  https://www.filmpreservation.org/preservation-basics/color-dye-fading
- Getty, "Atlas of Analytical Signatures – Cyanotype".
  https://www.getty.edu/conservation/publications_resources/pdf_publications/pdf/atlas_cyanotype.pdf
- filmcolors.org (timeline di Kinemacolor, Technicolor III e IV, Dufaycolor). https://filmcolors.org/
- Physics World, "Daguerreotypes reveal their plasmonic secrets".
  https://physicsworld.com/a/daguerreotypes-reveal-their-plasmonic-secrets/
- JAIC, studi sulla stabilità dei coloranti. https://cool.culturalheritage.org/jaic/articles/jaic31-02-005.html

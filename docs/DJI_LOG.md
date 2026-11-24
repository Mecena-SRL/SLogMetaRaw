# DJI: profili log per camera (ricerca, 24/11/2026)

Ricerca per implementare l'ingresso DJI. Ogni dato porta la sua natura: **ufficiale** (documento DJI o Blackmagic),
**misurato** (terzi, con metodo dichiarato), **dedotto** (calcolato qui da dati altrui), **non verificato**.
Nessuna camera DJI è stata tarata in questa ricerca (una clip Action 5 Pro è stata solo analizzata, §3b); nessun
codice è cambiato.

## 1. Quattro famiglie, non una

| Profilo | Natura | Formula pubblicata | Gamut | In Resolve (CST/RCM) |
|---|---|---|---|---|
| **D-Log** | log vero, 10 bit | **sì**, white paper DJI 2017 (X5S/X7) e 2022 (X9) | D-Gamut (ufficiale) | sì: *DJI D-Log* / *DJI D-Gamut* |
| **D-Log M** | "log leggero" consumer, 10 bit | **no**, solo LUT → Rec.709 per camera | non pubblicato | **no** (né in 21.1) |
| **D-Log2** | log nuovo (Osmo Pocket 4P, 2026), 10 bit | non trovato il documento; costanti esatte in un IDT pubblico | D-Gamut2 | **sì da 21.1**: *DJI D-Log2* / *D-Gamut2* (nomi esatti del menu da verificare) |
| D-Cinelike | curva "flat" video, non log | no | non pubblicato (presumibilmente Rec.709) | no |
| HLG | BT.2100 HLG | standard | Rec.2020 | sì |

## 2. D-Log / D-Gamut (ufficiale)

Fonti: [white paper 2017](https://dl.djicdn.com/downloads/zenmuse+x7/20171010/D-Log_D-Gamut_Whitepaper.pdf),
[white paper X9 2022](https://dl.djicdn.com/downloads/DJI_Ronin_4D/X9_D_Log_D_Gamut_Whitepaper.pdf) (formula identica),
[colour-science](https://github.com/colour-science/colour) (stessa formula).

```
lin → log:  x <= 0.0078 : 6.025 x + 0.0929        altrimenti  0.256663 log10(0.9892 x + 0.0108) + 0.584555
log → lin:  y <= 0.14   : (y - 0.0929) / 6.025    altrimenti  (10^(3.89616 y - 2.27752) - 0.0108) / 0.9892
```
- Codici 10 bit all'EI nativo: 0% = 95, **18% = 408 (0.3988, range Full)**, 90% = 586. 2017: 7,8 stop sopra il
  grigio fino a 1.0, "circa 15 stop". X9: "14 stop", clip 948 all'EI nativo, più basso sotto il nativo
  (EI 200 → 789); sopra il nativo stessa curva + guadagno analogico (si perde luce alta).
- EI nativi X9: 6K 800/5000, 8K 800/4000. X5S: EI 1600.
- D-Gamut, bianco D65: R 0.71/0.31, G 0.21/0.88, B 0.09/−0.08. Matrice ufficiale D-Gamut → Rec.709:
  `1.6746 −0.5797 −0.0949 / −0.0981 1.3340 −0.2359 / −0.0410 −0.2430 1.2840`.
- Il plugin (curva 14, gamut 13) coincide con il white paper. **Range**: il paper non lo dichiara; i codici sono Full.
  Sui file Mavic/Pocket 4 non è verificato se il flag VUI dica Full o Video.

## 3. D-Log M (nessun dato ufficiale)

DJI non ha pubblicato curva né gamut; le richieste sul forum DJI (Pocket 3, 2023-24) restano senza risposta.
Esistono solo LUT D-Log M → Rec.709, **diverse per camera** ([elenco DJI](https://www.dji.com/lut)).
Resolve: le prove della comunità con CST *D-Gamut/D-Log* danno immagine troppo contrastata e satura (D-Log M ha circa il
doppio dei codici per stop rispetto a D-Log).

**Misura più solida trovata** ([jens-krause/dlogm-dctl](https://github.com/jens-krause/dlogm-dctl), v1.3 05/11/2026):
ColorChecker, serie di tempi a ISO fisso, spline libera, residuo ~0,005. Due camere, due curve.

| Codice (Video normalizzato) | Action 6 (stop vs 18%) | Osmo 360 II | Fit Freeman (nel plugin) |
|---|---|---|---|
| 0.10 | −3.80 | −4.20 | −3.78 |
| 0.20 | −2.08 | −2.11 | −1.90 |
| **0.414** | **0** | **0** | +0.10 |
| 0.60 | +1.19 | +1.19 | +1.35 |
| 0.80 | +2.51 | +2.84 | +2.54 |
| 0.90 | +3.23 | +3.68 | +3.11 |
| 1.00 | +4.08 | +4.33 | +3.67 |

(Colonne Action 6 e 360 II lette dalle tabelle della DCTL; colonna Freeman calcolata qui da `sm_dlogm_dec`.)
- Grigio 18% a **0.414** in entrambe (come la LUT DJI); Freeman lo mette a **0.400**: −0,1 stop nei medi.
- Nero = codice 0 (tappo); lineare sotto ~0,03–0,05; oltre 1.0 prosegue in log.
- Luci: Freeman comprime le alte luci (−0,4 stop a 1.0 rispetto all'Action 6, −0,7 rispetto al 360 II).
- La 360 II ha più margine nelle luci dell'Action 6: **la curva D-Log M non è unica fra camere**.
- Matrice Action 6 ricavata dalla LUT ufficiale (344 nodi non clippati, residuo 0,004), nativo → Rec.709 lineare:
  `1.29666 −0.20659 −0.09007 / −0.09630 1.14054 −0.04424 / −0.07450 −0.15300 1.22751`.
  Primarie **dedotte** qui: R 0.771/0.319, G 0.283/0.730, B 0.123/0.028, D65 (righe a somma 1).
  Freeman (nel plugin): R 0.751/0.276, G 0.234/0.697, B 0.163/−0.016. La 360 II ha una matrice propria (allineata
  all'Action 6 sulla stessa tavola).
- Il fit di Freeman viene da footage Mini 4 Pro: forse giusto per i droni, non dimostrato per le action.

Gamma dinamica D-Log M (non ufficiale): Mavic 4 Pro con auto ISO 17,7 stop misurati da terzi (Xyla 21, contro 14,7 in
D-Log) per una fusione a doppio guadagno non dichiarata da DJI ([DroneXL](https://dronexl.co/2025/06/28/dji-mavic-4-pro-dual-gain-iso-fusion)).

## 3b. D-Log M: analisi numerica (24/11/2026)

Calcoli fatti qui (script non nel repository) su: tabelle misurate Action 6 / 360 II (§3), fit Freeman del plugin, LUT
ufficiale DJI e LUT CineMatch fornite da Ivan, una clip Action 5 Pro D-Log M fornita da Ivan. Codici normalizzati sul
range Video, rms in codici su 1023.

**Che curva è.** Pendenza in codici per stop (dalla tabella Action 6):

| Codice | 0.10 | 0.20 | 0.30 | 0.414 | 0.60 | 0.80 | 0.95 |
|---|---|---|---|---|---|---|---|
| Action 6 | 0.048 | 0.069 | 0.107 | 0.148 | 0.156 | 0.150 | 0.117 |
| Freeman | 0.057 | 0.075 | 0.110 | 0.136 | 0.161 | 0.173 | 0.178 |

- Una log vera ha pendenza costante sopra il piede (D-Log: 0.077 ovunque). D-Log M ne ha circa **il doppio (0.15) nei
  medi**, un **piede lungo** sotto 0.3 e una **spalla** sopra 0.8: è una curva a S con il centro logaritmico, non una
  log. Spiega il giudizio della comunità ("not a true log", ACES Central 2024) e perché il CST *D-Log* raddoppia il
  contrasto.
- Freeman non ha spalla (pendenza crescente fino a 1.0): da qui l'errore nelle luci della §3.
- Modelli analitici contro la tabella Action 6 (codici 0.04–1.0): `a·log2(lin+b)+c` rms 11,4; HLG BT.2100 scalato
  rms 8,4; Freeman 17,8; D-Log scalato 117. Sulla 360 II: 15,7 / 15,1 / 36,9 / 103. **Nessuna forma chiusa scende
  sotto il rumore di misura (~5)**: serve una tabella (come fa la DCTL di riferimento) o una spline.
- HLG è la forma chiusa più vicina: spiega perché in Resolve "input HLG" funziona meglio di D-Log.

**Le LUT DJI non sono per camera.** I file "Osmo Pocket 3" e "Osmo Action 5 Pro" (copie in un repository pubblico; la Pocket 3 riporta l'URL CDN DJI) e la
LUT fornita da Ivan sono **lo stesso file** (sha256 `b1816285…`), con intestazione `Mavic 3 Pro, D-Log M, 2023-03-24`.
DJI dunque distribuisce la LUT Mavic 3 Pro anche per Pocket 3 e Action 5 Pro; la Action 6 ha una LUT propria
(`OA6 D-LogM to Rec.709 v11.17`). Conseguenza: una curva o un gamut "confermati dalla LUT" valgono per il Mavic 3 Pro,
non per la camera che ha girato la clip.

**Cosa fa la LUT 2023.** Asse neutro: grigio 0.414 → uscita 0.397 (−0,7 stop di display a gamma 2.4); il 18% in OETF
Rec.709 (0.409) arriva da codice 0.424; nessun clip prima di 1.0. Separabilità (curva per canale + matrice in lineare +
curva per canale), provata con sei decodifiche:

| Decodifica | rms, nodi poco saturi (spread ≤ 0.13) | R x,y ricavato |
|---|---|---|
| Action 6 misurata | 18,8 | 0.993, 0.323 |
| Freeman | 18,1 | 0.978, 0.326 |
| HLG | 18,6 | 0.980, 0.321 |
| gamma 2.4 | 19,8 | 0.962, 0.328 |
| D-Log | 68,0 | 0.952, 0.305 |

Allargando ai nodi con spread ≤ 0.26 il residuo sale a ~45 e il rosso ricavato scende a x 0.92; su tutti i nodi a x
0.74. Quindi: (1) la LUT 2023 **non è matrice + curva**, contiene un look non lineare (tinta e saturazione); (2) le
primarie che se ne ricavano **dipendono dai nodi scelti** e non sono fisiche (x > 0.9): anche quelle di
`tools/lut_gamut.py` vanno lette così; (3) la LUT esclude D-Log ma **non distingue** fra Freeman, Action 6 e HLG.
La matrice Action 6 della §3 viene invece dalla LUT OA6, che il suo autore trova separabile (residuo 0,004).

**Clip Action 5 Pro (fornita da Ivan).** HEVC 10 bit 4:2:0, VUI dichiarato BT.709 / BT.709 / BT.709 e range Video;
ISO 1600, 1/53 s, 5600 K. Y minimo 28 (sotto il nero 64: DJI usa il footroom, il nero non è tagliato), 0,1° percentile
~85, 99,9° 731, massimo 837: la scena non tocca il clip, quindi questa clip non misura il bianco.

## 3c. CineMatch (FilmConvert)

- Metodo dichiarato: ogni camera è profilata "under identical shooting conditions"; il match "allinea i dati del
  sensore" della sorgente a quelli della destinazione e poi riapplica la scienza colore della destinazione. Uscite
  Rec.709, Rec.2100 HLG o PQ; controlli esposizione, bilanciamento, tinta. FilmConvert **non pubblica** procedura,
  formato dei profili, spazio di lavoro né decodifica di D-Log M ([pagina CineMatch](https://www.filmconvert.com/plugin/cinematch),
  [blog 2020](https://www.filmconvert.com/blog/introducing-cinematch-camera-matching-made-easy/)).
- Camere DJI nell'elenco FilmConvert: Action 4, 5 Pro, 6; Air 2S, Air 3; Mavic 2 Pro, 3 Cine, 4 Pro; Mini 3 Pro,
  4 Pro, 5 Pro; Pocket 3, 4, 4P; Phantom 4 / 4 Pro; Ronin 4D; Zenmuse X3–X7 (senza dire quali profili).
- **Misura nostra**: la LUT CineMatch esportata il 15/11/2026 (impostazioni di esportazione non note) coincide con la
  LUT DJI 2023: differenza media 0,0007, rms 0,0009, massimo 0,017, solo il 2% dei nodi oltre 0,005, assi neutri
  entro 0,0006. È la LUT DJI ricampionata: per D-Log M → Rec.709 CineMatch **non aggiunge** una curva o un gamut
  propri, almeno in questa esportazione.
- Lezione per il plugin: il valore di CineMatch sta nel profilo per camera e nei controlli in un dominio "sensore"; su
  D-Log M la sua base è la stessa LUT DJI che abbiamo. Un vantaggio reale si ottiene solo con misure nostre per camera.

## 3d. Metodo di taratura proposto

Per ogni famiglia (action, 360, Pocket, droni): ISO fisso, WB fisso, nitidezza e riduzione rumore al minimo;
1. **Curva**: serie di tempi a luce costante (cartoncino grigio + scala di grigi), 1/3 di stop fra le prese su 12+
   stop, tempo letto dal `djmd`; stima della curva come spline monotona su tutti i patch (metodo di Debevec–Malik), nero
   da una presa col tappo, grigio 18% come ancora. Verifica: una seconda serie indipendente deve cadere entro 0,1 stop.
2. **Gamut**: ColorChecker sotto illuminante misurato; matrice nativo → XYZ ai minimi quadrati (righe vincolate al
   bianco) sui patch linearizzati, con i valori Lab di riferimento X-Rite; errore in ΔE2000 per patch.
3. **Confronto**: CST di Resolve con la nostra curva contro la LUT DJI della stessa camera, sulle stesse clip.

## 4. D-Log2 / D-Gamut2 (Osmo Pocket 4P, 2026)

- **Resolve 21.1 lo supporta**: "DJI's D-Gamut2 and D-Log2" ([Blackmagic, 08/09/2026](https://www.blackmagicdesign.com/media/partial/release/20260908-03)).
- White paper DJI non trovato. IDT pubblico ([AJ3DVFX/dji-dlog2-to-aces-idt](https://github.com/AJ3DVFX/dji-dlog2-to-aces-idt), MIT,
  fonte delle costanti non dichiarata) con costanti a 15 cifre e grigio esattamente a 312/1023:

```
log → lin:  y < 0.148314799066323 : (y - 0.148314799066323) / 2.960935254549225 + 0.028961695254132
            y < 0.304985337243402 : 2^((y - 0.304985337243402) / 0.059439398321493 + log2(0.18))
            altrimenti            : 475 / (2^16.285770761945304 - 1) * (2^(16.285770761945304 y) - 1)
D-Gamut2 → AP0:  0.6788 0.2163 0.1049 / 0.0437 0.9329 0.0234 / -0.0031 0.0232 0.9799
```
- Verifiche fatte qui: segmenti continui (valore e pendenza a 0.1483), lineare 0 a codice **64/1023** (nero legale su
  scala 0–1023), grigio 0.305 (≈ 31 IRE, come misurato da Gamut.io), massimo 475 = **+11,4 stop** sopra il grigio
  (Gamut.io misura ~10 stop prima del clip, clip ~70 IRE a ISO 100 e ~94 IRE a ISO 1600).
- Primarie **dedotte** dalla matrice (bianco ACES, quindi già adattate): R 0.711/0.292, G 0.210/0.766, B 0.093/−0.016.
  Vicine a D-Gamut ma non uguali. Il bianco nativo di D-Gamut2 non è noto.
- Solo obiettivo 1× della 4P, non in slow motion; ISO base 100, niente auto ISO. DJI dichiara 17 stop (sensore LOFIC).
- Da confrontare con la CST di Resolve 21.1 prima di adottarle (§7).

## 5. Camere

Fonti: pagine specifiche DJI lette il 24/11/2026; dove bloccate, [4filmmakers 11/2026](https://www.4filmmakers.co/news/dji-d-log-vs-d-log-m-vs-hlg/)
e [DroneDJ 2024](https://dronedj.com/2024/06/27/dji-drone-camera-d-log/) (non verificato). Schema `djmd` da
[exiftool DJI.pm 1.21](https://github.com/exiftool/exiftool/blob/master/lib/Image/ExifTool/DJI.pm).

| Camera | Profili log | Bit / codec | ISO in log | Schema `djmd` |
|---|---|---|---|---|
| Inspire 3 (X9-8K Air) | D-Log, RAW | ProRes RAW, CinemaDNG | EI 200–6400 | — |
| Ronin 4D (X9-6K / 8K) | D-Log, HLG | ProRes RAW/4444 XQ/422 HQ, H.264 10 bit 4:2:0 | 6K: EI nativi 800/5000; 8K: 800/4000 (white paper), 320/1600 con DRE spento (pagina DJI) | — |
| Inspire 2 (X5S, X7) | D-Log | CinemaDNG, ProRes | EI 1600 (X5S) | — |
| Mavic 4 Pro (3 camere) | D-Log, D-Log M, HLG | H.265 10 bit 4:2:0; H.264 ALL-I 4:2:2 | D-Log 400–6400, D-Log M 100–6400 (Hasselblad) | `dvtm_Mavic4` |
| Mavic 3 Pro | D-Log (solo Hasselblad), D-Log M, HLG | H.265 10 bit 4:2:0; ProRes 4:2:2 (Cine) | D-Log 400–1600, D-Log M 100–1600 | `dvtm_wm261` |
| Mavic 3 / Cine / Classic | D-Log, D-Log M (firmware) | H.264/H.265, ProRes (Cine) | — | `dvtm_wm265e`, `dvtm_wm260` |
| Mavic 2 Pro, Air 2S | "Dlog-M" 2018 (LUT propria) | H.265 10 bit | — | — |
| Air 3, Air 3S | D-Log M, HLG | H.264/H.265 10 bit 4:2:0 | 100–3200 (3S) | `dvtm_Air3`, `dvtm_Air3s` |
| Mini 5 Pro | D-Log M, HLG | H.265 10 bit 4:2:0 | 100–3200 | `dvtm_Mini5Pro` |
| Mini 4 Pro | D-Log M, HLG | H.265 10 bit 4:2:0 | 100–1600 | `dvtm_Mini4_Pro` |
| Avata 2, Flip, O4 Air Unit | D-Log M | H.264/H.265 | Flip 100–1600 | `dvtm_AVATA2` |
| Neo, Neo 2 | nessun log | — | — | `dvtm_dji_neo`, `dvtm_NEO2` |
| Osmo Action 4 | D-Log M | H.264/H.265 10 bit | — | `dvtm_ac203` |
| Osmo Action 5 Pro | D-Log M, HLG | HEVC 10 bit | — | `dvtm_ac204` (provato nel plugin) |
| Osmo Action 6 | D-Log M | HEVC 10 bit | max 6400 in D-Log M | `dvtm_ac206` |
| Osmo 360, 360 II | D-Log M | HEVC 10 bit | — | `dvtm_oq101` |
| Osmo Nano | D-Log M | 10 bit | — | non in exiftool |
| Osmo Pocket 3 | D-Log M | H.264/H.265 10 bit | 50–6400 | `dvtm_PP-101` |
| Osmo Pocket 4 | D-Log | H.265 10 bit | — | non in exiftool |
| Osmo Pocket 4P | D-Log2 (1×), D-Log (tele) | H.265 10 bit | base 100 (D-Log2) | non in exiftool |
| Mini 3 Pro, Mavic Air 2, Mavic 2 Zoom, Osmo Action 1-3, Pocket 1-2 (DroneDJ) | D-Cinelike | 8/10 bit | — | — |

## 6. Il plugin oggi e le lacune

Stato (`slogmetaraw/dji.py`, `math/Transfer.h`, `docs/CAMERAS.md`): curva 13 D-Log M = Freeman, 14 D-Log = white paper;
gamut 13 D-Gamut, 19 D-Log M stimato; lettore `djmd` con percorsi della Action 5 Pro.

1. **D-Log2 / D-Gamut2 assenti.** Né curva né gamut né codice del modo colore. Resolve 21.1 li ha: la priorità è la
   coerenza con la CST di Resolve.
2. **D-Log M: una curva per tutte le camere.** Le misure Action 6 / 360 II mettono il grigio a 0.414 (Freeman 0.400)
   e le luci fino a +0,7 stop oltre Freeman. Va scelta una curva per famiglia (action, 360, droni), non una sola.
3. **Gamut D-Log M**: tre stime diverse (Freeman; `lut_gamut.py` sulla LUT "Action 5 Pro", che è la LUT Mavic 3
   Pro 2023 e non è separabile, §3b; matrice Action 6 dalla LUT OA6). Nessuna misura colorimetrica assoluta.
   Anche la frase di `docs/CAMERAS.md` "curva confermata dalla LUT DJI" va ridimensionata: la LUT non distingue
   Freeman da Action 6 o HLG.
4. **Percorsi `djmd` dei droni diversi da quelli delle action** (exiftool): ISO a 3-2-7-1 (Mini 4 Pro, Air 3), 3-2-9-1
   (Pocket 3, Mavic 3 Pro, Mavic 4), 3-2-2-1 (Avata 2, Neo, Mavic 3); Kelvin a 3-2-32-1 (Mini 4 Pro, Air 3) o 3-2-24-1
   (Pocket 3, Mavic 4); su Mavic 3 il 3-2-6-1 è lo zoom. Oggi `FRAME_FIELDS` usa quelli dell'Action 4/5: sui droni
   l'ISO viene perso e il Kelvin può essere sbagliato se il valore cade nell'intervallo ammesso. Il percorso del modo
   colore 2-4-1 è provato solo sulla Action 5 Pro.
5. **`profile_source 'tarato'`** vale per ogni schema `dvtm_ac20*`, quindi anche Action 6, la cui curva misurata è
   diversa da Freeman.
6. **Range D-Log sui file Mavic**: non verificato.

## 7. Proposta (non applicata)

In ordine; ogni passo va fatto con test e aggiornando l'Atlante.

1. **Curva D-Log2 e gamut D-Gamut2** con le costanti del §4 (curva 25, gamut 20, in coda), poi confronto numerico
   con la CST di Resolve 21.1 su una rampa: se coincidono, le costanti sono confermate.
2. **Tabella schema → percorsi** in `dji.py` (dalle tabelle di exiftool) invece dei percorsi fissi della Action.
3. **D-Log M per famiglia**: tenere Freeman per i droni; per Action 6 / 360 una curva misurata (tabelle a 513 punti o
   un fit), dopo aver controllato la licenza del repository di origine (oggi non dichiarata nel README).
4. **Misura nostra** col metodo della §3d su ogni famiglia di camere. Servono le clip.
5. Modo colore D-Log2 nel `djmd` della Pocket 4P: serve una clip.

# Creative: le pellicole dalle schede tecniche

Le pellicole colore che hanno una scheda tecnica pubblicata (o una sorella misurata della stessa casa e della
stessa generazione) passano per una **catena spettrale**: la luce della scena espone i tre strati con le loro
sensibilità spettrali, ogni strato si sviluppa sulla sua curva caratteristica, i coloranti del negativo filtrano la luce
della stampatrice come la vedono gli strati della pellicola positiva, la positiva si sviluppa sulla sua curva e i suoi
coloranti filtrano la lampada del proiettore, vista dall'occhio. Tutto da misure, niente curve inventate. Le altre
pellicole (storiche, bianco e nero, processi antichi) restano sul modello parametrico di [`CREATIVE.md`](../CREATIVE.md).

Legenda: **[doc]** misura dalla scheda del produttore; **[ded]** dedotto (una scelta nostra, motivata); **[gen]** stima
generica da tarare.

## Fonti

- Schede tecniche Kodak e Fujifilm (sensibilità spettrale, curve caratteristiche Status M/A, densità spettrale dei
  coloranti, base) [doc], nella digitalizzazione del progetto **spektrafilm** di Andrea Volpato
  (<https://github.com/andreavolpato/agx-emulsion>, profili v0.3.2, commit `3bb2c2d` del 2026-06-07, licenza
  **CC BY-SA 4.0**; i dati originali sono dei produttori). Nel repository non c'è nessun profilo di spektrafilm: lo
  script `tools/creative/datasheets.py` li legge e scrive `ofx/SLogMetaRaw/src/creative/CreativeSheets.h`, che ne è un
  derivato (stessa licenza compatibile con la GPL 3).
- Osservatore CIE 1931 2°, illuminanti CIE D50/D55/D65, lampada a incandescenza e xeno Kinoton 75P, riflettanze di
  addestramento di rawtoaces (190 campioni, Academy) e ColorChecker BabelColor: dalla libreria colour-science.
- Filtro anticalore Schott KG3 della stampatrice (curva dal catalogo Schott, nei dati di spektrafilm).
- Mallett e Yuksel 2019, *Spectral Primary Decomposition*: provata e scartata (vedi sotto).

## Il metodo (`tools/creative/datasheets.py`, `math/Creative.h` `cr_sheet_*`)

| Passo | Cosa si fa | Natura |
|---|---|---|
| Ripresa | la scena è illuminata dalla luce per cui la pellicola è bilanciata (3200 K per le T, D55 per le D: come una cinepresa filtrata a regola, 85 o 80A), il video l'ha bilanciata sul bianco (CAT02 verso D65). Esposizione di ogni strato = ∫ luce · riflettanza · sensibilità. Il passaggio dal video (Rec.2020 lineare, tre numeri) allo spettro non è univoco: come per un IDT dell'Academy si adatta una matrice 3×3 sulle 190 riflettanze di addestramento di rawtoaces (minimi quadrati relativi, bianco esatto), verificata sul ColorChecker | [doc] sensibilità, [ded] il metodo |
| Sviluppo | curva caratteristica di ogni strato (densità sopra il minimo contro log₁₀ dell'esposizione, grigio 18 % a 0), 32 campioni a passi di 0,2 log | [doc] |
| Texture | grana, polvere, graffi, interimage e i comandi di colore lavorano sulle densità, in "stop" di un negativo di gamma 0,6 attorno al grigio: gli stessi passaggi di prima | [gen] come prima |
| Stampa | i coloranti e la base del negativo (densità spettrali 400-740 nm ogni 10 nm) filtrano la lampada della stampatrice (3400 K dietro un KG3) e gli strati della positiva la prendono con le loro sensibilità: integrale spettrale vero, per pixel. Le luci di stampa le fissa il laboratorio sul grigio del negativo: il grigio esce neutro a 0,115 del bianco (la mira LAD di Kodak, circa 1,0 Status A). Punti di stampa: 1 punto = 0,025 log E, più rosso = stampa più ciano | [doc] spettri, [ded] la mira |
| Positiva | curva della 2383 (Positivo standard e varianti) o della 2393 Premier (Positivo a contrasto alto) | [doc] |
| Proiezione | coloranti e base della positiva sulla lampada allo xeno (Kinoton 75P), occhio adattato al bianco del quadro vuoto (CAT02), Rec.2020 lineare → Rec.709 Gamma 2.4 | [doc] spettri, [ded] l'adattamento |
| Invertibile | stessa ripresa e curva (che scende), poi i suoi coloranti visti su D50 | [doc], [ded] la luce |
| Negativo com'è | i coloranti e la base arancione su un piano luminoso D65, senza adattamento: si vede la maschera | [doc] |

Il campionamento a 10 nm è controllato contro i dati a 5 nm: esposizione della stampa entro 0,002 log in media,
0,025 log al massimo. Le varianti di stampa (fredda, calda, morbida, Est Europa, per telecinema, bypass) sono la 2383
con il contrasto e la dominante della variante [ded].

### Perché non la decomposizione di Mallett

Mallett-Yuksel ricostruisce riflettanze lisce dai tre valori sRGB: dentro sRGB va bene, fuori (DaVinci WG, ACES,
gamut delle camere) i coefficienti diventano negativi e uno strato riceve **luce negativa**, che nessuna pellicola può
ricevere. Sul set di addestramento sbaglia di 0,3-0,5 stop in media e fino a 12 stop sui colori saturi; la matrice
adattata sulle riflettanze reali sbaglia 0,1 stop in media (vedi tabella). Una luce monocromatica resta comunque fuori
portata di qualunque metodo a tre numeri: il video ha già perso lo spettro (metamerismo).

### Errori della ripresa, in stop (media / massimo)

```
kodak_vision3_500t     shoot: training 0.11/1.09, checker 0.07/0.58 stops (mean/max); 10 nm printing 0.001/0.009 log
kodak_vision3_200t     shoot: training 0.11/1.47, checker 0.08/0.55 stops (mean/max); 10 nm printing 0.001/0.007 log
kodak_vision3_250d     shoot: training 0.14/1.55, checker 0.09/0.81 stops (mean/max); 10 nm printing 0.001/0.007 log
kodak_vision3_50d      shoot: training 0.12/1.36, checker 0.09/0.75 stops (mean/max); 10 nm printing 0.002/0.021 log
kodak_verita_200d      shoot: training 0.11/1.10, checker 0.08/0.65 stops (mean/max); 10 nm printing 0.002/0.015 log
kodak_portra_160       shoot: training 0.08/0.75, checker 0.06/0.41 stops (mean/max); 10 nm printing 0.002/0.025 log
kodak_portra_400       shoot: training 0.11/0.94, checker 0.08/0.61 stops (mean/max); 10 nm printing 0.002/0.023 log
kodak_portra_800       shoot: training 0.13/1.38, checker 0.09/0.78 stops (mean/max); 10 nm printing 0.002/0.020 log
kodak_ektar_100        shoot: training 0.13/1.31, checker 0.09/0.73 stops (mean/max); 10 nm printing 0.002/0.022 log
kodak_gold_200         shoot: training 0.13/1.35, checker 0.09/0.82 stops (mean/max); 10 nm printing 0.002/0.023 log
kodak_ultramax_400     shoot: training 0.14/1.35, checker 0.09/0.80 stops (mean/max); 10 nm printing 0.002/0.021 log
fujifilm_c200          shoot: training 0.08/0.89, checker 0.06/0.51 stops (mean/max); 10 nm printing 0.002/0.019 log
fujifilm_pro_400h      shoot: training 0.08/1.01, checker 0.06/0.38 stops (mean/max); 10 nm printing 0.001/0.006 log
fujifilm_xtra_400      shoot: training 0.08/0.83, checker 0.06/0.40 stops (mean/max); 10 nm printing 0.002/0.019 log
fujifilm_provia_100f   shoot: training 0.13/1.40, checker 0.09/0.82 stops (mean/max)
fujifilm_velvia_100    shoot: training 0.12/1.33, checker 0.09/0.79 stops (mean/max)
kodak_ektachrome_100   shoot: training 0.14/1.50, checker 0.10/0.81 stops (mean/max)
kodak_kodachrome_64    shoot: training 0.15/1.45, checker 0.10/0.83 stops (mean/max)
```

## In ogni spazio colore

La pellicola vede la luce della scena: il nodo decodifica la codifica d'ingresso in luce lineare (S-Log3, DaVinci
Intermediate, ACEScct, ACES2065-1, LogC3/4, V-Log, Apple Log, D-Log M, F-Log, C-Log, Log3G10, Film Gen5, Cineon,
Rec.709) e la porta in Rec.2020 adattato a D65. La stessa scena dà la stessa immagine da ogni codifica (test
`test_a_measured_stock_sees_the_scene_alike_from_every_encoding`: entro 0,004 in codice). Il menu *Ingresso (IDT)*
elenca ora tutte le coppie gamut/curva che il nodo sa leggere; *Automatico* resta la scelta normale.

Limiti: un colore fuori dal gamut della pellicola (un LED blu, un verde laser) non può essere ricostruito come spettro;
dove la matrice darebbe un'esposizione negativa lo strato non riceve luce (resta al suo minimo). Le alte luci oltre
la fine della curva misurata (circa 11 stop sopra il grigio) restano sull'ultimo valore della scheda.

## Ogni pellicola

| Produttore | Pellicola | Catena | Dati |
|---|---|---|---|
| Kodak / Eastman Kodak | Vision3 500T 5219 (2007) | spettrale | misurata (scheda Vision3 500T) |
| Kodak / Eastman Kodak | Vision3 200T 5213 (2010) | spettrale | misurata (scheda Vision3 200T) |
| Kodak / Eastman Kodak | Vision3 250D 5207 (2010) | spettrale | misurata (scheda Vision3 250D) |
| Kodak / Eastman Kodak | Vision3 50D 5203 (2011) | spettrale | misurata (scheda Vision3 50D) |
| Kodak / Eastman Kodak | Vision2 500T 5218 (2002) | spettrale | [ded] sorella misurata: Vision3 500T |
| Kodak / Eastman Kodak | Vision 800T 5289 (1998) | spettrale | [ded] sorella misurata: Vision3 500T |
| Kodak / Eastman Kodak | Vision Expression 500T 5284 (2001) | spettrale | [ded] sorella misurata: Vision3 500T |
| Kodak / Eastman Kodak | Primetime 640T 5620 (1995) | spettrale | [ded] sorella misurata: Vision3 500T |
| Kodak / Eastman Kodak | EXR 500T 5298 (1994) | spettrale | [ded] sorella misurata: Vision3 500T |
| Kodak / Eastman Kodak | EXR 200T 5293 (1992) | spettrale | [ded] sorella misurata: Vision3 200T |
| Kodak / Eastman Kodak | EXR 50D 5245 (1989) | spettrale | [ded] sorella misurata: Vision3 50D |
| Kodak / Eastman Kodak | High Speed 5294 (1983) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Eastmancolor II 5247 (1974) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Eastmancolor 5254 (1968) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Eastmancolor 5248 (1952) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Eastmancolor 5247 (1950) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Verita 200D 5206 (2026) | spettrale | misurata (scheda Verita 200D) |
| Kodak / Eastman Kodak | Kodachrome 40 Super 8 (1974) | spettrale | [ded] sorella misurata: Kodachrome 64 |
| Kodak / Eastman Kodak | Kodachrome II (1961) | spettrale | [ded] sorella misurata: Kodachrome 64 |
| Kodak / Eastman Kodak | Kodachrome (1935) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Kodachrome 25 (foto) | spettrale | [ded] sorella misurata: Kodachrome 64 |
| Kodak / Eastman Kodak | Kodachrome 64 (foto) | spettrale | misurata (scheda Kodachrome 64) |
| Kodak / Eastman Kodak | Kodachrome 200 (foto) | spettrale | [ded] sorella misurata: Kodachrome 64 |
| Kodak / Eastman Kodak | Ektachrome 100D 7294 (2018) | spettrale | misurata (scheda Ektachrome E100) |
| Kodak / Eastman Kodak | Ektachrome 100D 7285 (1999) | spettrale | [ded] sorella misurata: Ektachrome E100 |
| Kodak / Eastman Kodak | Ektachrome 64T 7280 (2005) | spettrale | [ded] sorella misurata: Ektachrome E100 |
| Kodak / Eastman Kodak | Ektachrome 160 Super 8 (1971) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Ektachrome EF 7241 (1966) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Ektachrome Commercial 7252 (1970) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Ektachrome E100 (foto) | spettrale | misurata (scheda Ektachrome E100) |
| Kodak / Eastman Kodak | Ektachrome 100VS (foto) | spettrale | [ded] sorella misurata: Ektachrome E100 |
| Kodak / Eastman Kodak | Portra 160 (foto) | spettrale | misurata (scheda Portra 160) |
| Kodak / Eastman Kodak | Portra 400 (foto) | spettrale | misurata (scheda Portra 400) |
| Kodak / Eastman Kodak | Portra 800 (foto) | spettrale | misurata (scheda Portra 800) |
| Kodak / Eastman Kodak | Ektar 100 (foto) | spettrale | misurata (scheda Ektar 100) |
| Kodak / Eastman Kodak | Gold 200 (foto) | spettrale | misurata (scheda Gold 200) |
| Kodak / Eastman Kodak | Ultramax 400 (foto) | spettrale | misurata (scheda Ultramax 400) |
| Kodak / Eastman Kodak | Kodacolor II (1972, 110 e 126) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Double-X 5222 (1959) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Plus-X 5231 (1956) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Super-XX 1232 (1938) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | 4-X 5224 (1964) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Tri-X invertibile 7266 (1955) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Plus-X invertibile 7276 (1955) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Tri-X 400 (foto) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | T-Max 100 (foto) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | T-Max 400 (foto) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Kodak / Eastman Kodak | Eastmancolor 5251 (1962) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Fujifilm | Eterna 500 8573 (2004) | spettrale | [ded] sorella misurata: Pro 400H |
| Fujifilm | Eterna 250D 8563 (2006) | spettrale | [ded] sorella misurata: Pro 400H |
| Fujifilm | Eterna Vivid 500 8547 (2009) | spettrale | [ded] sorella misurata: Pro 400H |
| Fujifilm | Eterna Vivid 160 8543 (2007) | spettrale | [ded] sorella misurata: Pro 400H |
| Fujifilm | Reala 500D 8592 (2001) | spettrale | [ded] sorella misurata: Pro 400H |
| Fujifilm | F-64D 8522 (anni '90) | spettrale | [ded] sorella misurata: Fujicolor C200 |
| Fujifilm | F-500 8572 (anni '90) | spettrale | [ded] sorella misurata: Superia X-TRA 400 |
| Fujifilm | Fujicolor AX 8514 (anni '80) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Fujifilm | Fujicolor 8517 (1977) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Fujifilm | Fujichrome R25 Single-8 (anni '70) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Fujifilm | Velvia 50 (foto) | spettrale | [ded] sorella misurata: Velvia 100 |
| Fujifilm | Provia 100F (foto) | spettrale | misurata (scheda Provia 100F) |
| Fujifilm | Astia 100F (foto) | spettrale | [ded] sorella misurata: Provia 100F |
| Fujifilm | Pro 400H (foto) | spettrale | misurata (scheda Pro 400H) |
| Fujifilm | Superia 400 (foto) | spettrale | [ded] sorella misurata: Superia X-TRA 400 |
| Agfa / Agfa-Gevaert | Agfacolor Neu invertibile (1936) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Agfa / Agfa-Gevaert | Agfacolor negativo cinema (1939) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Agfa / Agfa-Gevaert | Agfacolor CT18 / CK17 (1952-1977) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Agfa / Agfa-Gevaert | Agfa Moviechrome 40 (1974) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Agfa / Agfa-Gevaert | Agfa XT 320 (1990) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Agfa / Agfa-Gevaert | Agfa XTR 250 (1990) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Agfa / Agfa-Gevaert | Agfa Ultra 100 (foto) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Agfa / Agfa-Gevaert | Agfa Vista 200 (foto) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Agfa / Agfa-Gevaert | Agfa APX 100 (foto) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Agfa / Agfa-Gevaert | Agfa Scala 200x (invertibile) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| ORWO (Wolfen) | ORWOcolor NC 3 (1972) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| ORWO (Wolfen) | ORWOcolor NC 1 (1964) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| ORWO (Wolfen) | Original Wolfen NC 500 (2022) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| ORWO (Wolfen) | ORWOchrom UT 15 / UK 17 | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| ORWO (Wolfen) | ORWO UP 21 (invertibile) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| ORWO (Wolfen) | ORWO UN 54 | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| ORWO (Wolfen) | ORWO N 74 / N 75 | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Svema e Tasma (URSS) | Sovcolor (anni '50) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Svema e Tasma (URSS) | Svema DS-4 (anni '60-'70) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Svema e Tasma (URSS) | Svema LN-8 (anni '80) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Svema e Tasma (URSS) | Svema CO-32D (invertibile) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Svema e Tasma (URSS) | Svema OCh-180 (invertibile B/N) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Svema e Tasma (URSS) | Tasma A-2Sh (negativo B/N) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Svema e Tasma (URSS) | Svema Foto 400 (foto) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ilford | HP5 Plus | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ilford | FP4 Plus | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ilford | Pan F Plus 50 | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ilford | Delta 100 | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ilford | Delta 3200 | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ilford | XP2 Super (cromogenica) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ilford | HP (1935) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ilford | HPS (1954, foto) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ferrania (e 3M) | Pancro 30 / P30 (Pasolini, Fellini) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ferrania (e 3M) | Ferraniacolor (1952) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Ferrania (e 3M) | Pathéscope Colour 9,5 mm (anni '50) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Ferrania (e 3M) | Solaris FG 200 (foto) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Ferrania (e 3M) | Scotch Chrome 1000 (3M, foto) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Ferrania (e 3M) | P30 Alpha (2017, foto) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Gevaert | Gevacolor (1947) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Gevaert | Gevapan 30 | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Gevaert | Gevapan 36 (rapida) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Ansco | Anscocolor invertibile (anni '50) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Ansco | Ansco Color Negative 844 (1956) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Ansco | Anscochrome 500 (anni '60) | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Ansco | Ansco Supreme 453 | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| DuPont | Superior 2 (noir anni '40) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| DuPont | Superior 4 (rapida) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| DuPont | Rapid Reversal 931 | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| CineStill | CineStill 800T | spettrale | [ded] sorella misurata: Vision3 500T |
| CineStill | CineStill 50D | spettrale | [ded] sorella misurata: Vision3 50D |
| CineStill | CineStill 400D | spettrale | [ded] sorella misurata: Vision3 250D |
| CineStill | CineStill BwXX | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Lomography | Lomography Color 400 | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Lomography | LomoChrome Metropolis | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Lomography | Berlin Kino 400 | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |
| Konica | Centuria Super 200 | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Konica | Impresa 50 | modello | [gen] parametri stimati (docs/CREATIVE.md) |
| Lumière (Lione) | Pellicola Lumière (1895) | modello B/N | [gen] parametri stimati (docs/CREATIVE.md) |

Cosa vuol dire "sorella misurata" [ded]: stessa casa, stessa famiglia di coloranti e di sensibilizzazione (Vision,
Vision2 ed EXR sulle Vision3 della stessa velocità; Kodachrome K-12 e K-14 sulla 64; Ektachrome cinema sulla E100;
Eterna e Reala sulla Pro 400H, l'altra Fuji a quarto strato; CineStill: Kodak cinema senza remjet). Sopra la
misura si mettono contrasto, saturazione e dominante della pellicola (stime [gen] in `CreativePresets.h`). La Ektachrome
100D 7294 è la stessa emulsione della E100 fotografica (Kodak, 2018). Una pellicola misurata usa solo la scheda: le sue
stime di contrasto, saturazione e dominante non si applicano; grana (RMS), alone e interimage restano quelle della riga.

## Cosa manca

- Bianco e nero: le sensibilità (blu, ortocromatica, pancromatica) e le curve sono ancora stime [gen]; servono le
  schede digitalizzate (Double-X 5222, Tri-X, HP5, FP4, P30).
- Pellicole storiche (Eastmancolor anni '50-'70, Agfacolor, Sovcolor, ORWO, Ferraniacolor, Gevacolor, Ansco): nessuna
  scheda spettrale digitalizzata trovata; restano sul modello con i coloranti non mascherati.
- Luce sbagliata per la pellicola (una T al sole senza 85): oggi si assume sempre la luce giusta; il cambio di
  temperatura del pannello resta una correzione creativa.
- Interimage (DIR) misurato: le curve sono neutre, l'effetto fra strati resta il parametro della riga.
- Non provato in Resolve; la parità Metal si verifica solo nella CI macOS.

# S-Log MetaRaw Detail: recupero locale, Texture, Clarity, Dehaze

Il secondo nodo del bundle. Fa quello che un nodo puntuale non può fare: **recuperare luci e
ombre per aree, conservando la texture**. È la parte "alla Lightroom" dei toni. Lavora in luce
lineare: decodifica quello che riceve e lo riscrive nella stessa codifica, cambiando solo un
guadagno per pixel (uguale su R, G e B, quindi la tinta non cambia).

## Dove metterlo, e in che spazio colore

```
S-Log MetaRaw  →  S-Log MetaRaw Detail  →  resto del grade  →  CST / LUT / DRT d'uscita
```

**In e Out sono lo stesso spazio.** Il nodo decodifica in luce lineare quello che riceve e lo
riscrive nella stessa codifica, cambiando solo un guadagno per pixel. Per questo vuole in
ingresso una codifica **log di scena**: S-Gamut3.Cine/S-Log3, S-Gamut3/S-Log3, S-Gamut/S-Log2,
DaVinci WG/Intermediate o ACES AP1/ACEScct (le scelte di Sviluppo e spazio colore › Ingresso (IDT)). Rec.709,
Gamma 2.4 o sRGB non sono fra queste: sono già immagini per il monitor, con le alte luci tagliate
al bianco, e non resta niente da recuperare.

| Configurazione | Nodo principale (Color Space · Gamma) | Detail (Ingresso (IDT)) | Uscita |
|---|---|---|---|
| **Consigliata**, timeline YRGB non gestita | DaVinci WG · DaVinci Intermediate | **DaVinci WG/Intermediate**, da dichiarare: in una timeline non gestita Automatico non la vede | grade in DWG, poi per ultimo un CST DaVinci WG/Intermediate → Rec.709 · Gamma 2.4 con tone mapping DaVinci |
| Tutto in log | Timeline | Automatico (usa la codifica di camera della clip) | CST S-Gamut3.Cine/S-Log3 → Rec.709 o DWG |
| Progetto color managed (timeline DWG) | Timeline | Automatico (lo spazio lo dice Resolve) | la fa il progetto |
| **Da evitare** | Rec.709 | — | Automatico non sa della conversione e decodifica il Rec.709 come S-Log3 (o come la timeline): il risultato è sbagliato, e le luci sono comunque già tagliate |

- **Destinazione (ODT)** dichiara la trasformazione che viene dopo (il nodo non la fa), con bianco e nero in stop dal
  grigio: Rec.709 con CST senza tone mapping +2,5 / −4,5 (7 stop in tutto), Rec.709 con DRT ACES/AgX/DaVinci
  +4,5 / −6, HDR PQ 1000 nit +6,5 / −7, Scena DaVinci WG/Intermediate (ODT più avanti) +9 / −9. Con «Dal Bianco
  (manuale)», il default, vale Avanzate › Bianco come prima (nero −4,5): con un tone mapping dopo portalo a 4-5 stop.
  Bianco e nero sono anche la zona X e la zona 0 della Zonale e della vista zone.
- Il Rec.709 del nodo principale è un CST senza tone mapping: va bene per un risultato veloce, non
  come base per il Detail né per un grade spinto.
- È **spaziale**: guarda i pixel vicini. Per questo Generate LUT lo esclude, e con lui tutte
  le altre correzioni del suo nodo (manuale di Resolve: gli OFX non compatibili con i LUT
  saltano tutto il nodo). Se esporti LUT, tienilo in un **nodo dedicato**.
- Ingresso **Automatico**: chiede lo spazio colore a Resolve. In una timeline YRGB senza gestione
  del colore Resolve non lo dice: il nodo usa allora la codifica di camera della clip (dal
  record di S-Log MetaRaw), o in mancanza S-Gamut3.Cine/S-Log3 "presunto". La riga in cima dice
  sempre quale ha scelto e perché: se il nodo principale converte, controllala.

## I controlli

| Gruppo | Controllo | Cosa fa |
|---|---|---|
| Sviluppo e spazio colore | **Modalità** | Zonale (Ansel Adams) o Split grade (stampa), vedi sotto; una riga dice cosa fa |
| | **Ingresso (IDT)** | la codifica che entra nel nodo (prima in Avanzate, stesso nome salvato) |
| | **Destinazione (ODT)** | bianco e nero dell'uscita: Dal Bianco (manuale), Rec.709 CST, Rec.709 DRT, PQ 1000, Scena DWG |
| Gamma dinamica | **Local Contrast** (−100…+50) | contrasto delle grandi aree attorno al grigio, a dettaglio intatto |
| | **Local Highlights** | in negativo comprime le grandi aree luminose con la stessa spalla del nodo principale, ma tiene la texture e la rinforza dove la base è compressa: il cielo si scurisce, le nuvole restano definite. A −100 il massimo registrato arriva sul Bianco di Avanzate. In positivo più stacco per aree |
| | **Local Shadows** | apre (+) o chiude (−) le ombre per aree, 100 = 2 stop |
| | Vista: guadagno | quanto il nodo alza (arancio) o abbassa (blu) ogni zona; grigio = invariato |
| | Vista: base | la base edge-aware su cui lavorano i toni locali, senza dettaglio |
| | Vista: zone, Vista: zone in grigi | la zona 0–X di ogni area dopo il nodo, in undici colori o undici grigi, sulla scala della Destinazione. In basso la legenda 0–X; in Zonale (Sviluppo) due tacche grigie dove sono stati letti Ombra e Luce e due bianche dove arrivano. Le zone 0 e X, senza dettaglio, sono tratteggiate |
| Presenza | **Texture** | dettaglio fine (pelle, tessuti, foglie); in negativo leviga; la grana sotto la soglia rumore non sale |
| | **Clarity** | contrasto locale a media scala, pesato sui mezzitoni |
| | **Dehaze** | toglie (+) o aggiunge (−) velo; il velo è quello che dichiari in Avanzate › Velo |
| | Azzera dettaglio | riporta a zero tutto il nodo |
| Avanzate | Preserva dettaglio (100 %) | 100 = le zone muovono solo le aree; 0 = come il nodo principale |
| | Raggio (4 % dell'altezza) | dimensione delle aree |
| | Soglia bordi (0,5 EV) | salto che conta come bordo, per la base e per Local Highlights: più bassa = meno aloni, meno recupero |
| | Soglia rumore (0,04 EV) | sotto questa ampiezza il dettaglio è rumore: Texture non lo alza |
| | Clarity: centro | il tono dove Clarity lavora di più |
| | Bianco (2,5 stop) | dove Local Highlights −100 porta il massimo registrato, con Destinazione «Dal Bianco»; con un DRT dopo, 4–5 |
| Velo | Livello (EV), Colore | luminanza e colore del velo; mai stimati fotogramma per fotogramma |

### Modalità Split grade: due esposizioni

La stampa a due esposizioni: una morbida (grado 0, filtro 00) che porta le luci e una dura (grado 5) che porta le
ombre; l'ordine non conta. Fasi: **1 · Esposizioni**, **2 · Locale**, **3 · Presenza**, o un nodo; la Guida segue.
Ombra e Luce e **Misura** sono quelli della Zonale. **Luci morbide** e **Ombre dure** sono in stop (più stop = più
scuro): a 0 le luci cadono sulla zona VIII e le ombre sulla III. Il **Grado** (da 0 morbida a 5 dura) esce dalla
pendenza della curva, 2,5 per la pendenza dell'immagine stessa e 2,5 gradi per ogni raddoppio: è un'ipotesi, da
verificare su stampe vere. In Locale, Local Highlights brucia le luci e Local Shadows schiarisce le ombre.

**Unione.** *Curva*: una sola curva, la pendenza esce dal rapporto fra le due esposizioni (perno scorrevole). *Due
strati*: la carta ha uno strato morbido (grado 0, pendenza 0,5) e uno duro (grado 5, pendenza 2), ognuno una retta
ancorata alla sua esposizione, e a ogni luce prende la più bassa con un raccordo di mezzo stop: ombre dure, luci
morbide, senza una pendenza intermedia. I gradi dei due strati sono fissi (ipotesi, da verificare su stampe).

**Provino.** Come il provino a strisce della camera oscura: **Provino** mostra sull'immagine una griglia di celle,
ognuna la stessa immagine con esposizioni diverse (più scuro = più stop), con la cella attuale incorniciata di chiaro.
*Strisce*: sette passi della morbida (riga alta) e della dura (riga bassa). *Matrice*: morbida da sinistra a destra,
dura dall'alto in basso. *Unioni a confronto*: Curva a sinistra e Due strati a destra, con le stesse esposizioni.
**Provino: passo** sono gli stop fra due celle. Porta **Provino: cella** su una cella e premi **Usa la cella**: le
esposizioni (o l'unione) di quella cella diventano quelle del nodo.

### Modalità Unito: misura, sviluppo e stampa

Split grade con la misura automatica e il ragionamento del BTZS: **Misura automatica** (nel gruppo Misura, vale in ogni
modalità) legge tutta l'immagine e prende come ombra e luce il 2° e il 98° percentile della luminanza; Ombra, Luce e
Misura le correggono. Il **soggetto** (luce meno ombra, in stop) è confrontato con la **scala della carta**, la
distanza fra le zone III e VIII sulla Destinazione (spostata da Luci morbide e Ombre dure). Se il soggetto è più ampio o
più stretto della carta, lo **sviluppo** del negativo (una retta attorno all'ombra, N− o N+) lo porta alla carta fin
dove può (pendenza da 0,5 a 2); la **stampa** a due esposizioni (Unione: Curva o Due strati) dà il resto. Il Grado
dice soggetto, carta, sviluppo e stampa. Fasi e Guida come nello Split grade; il Provino è dello Split grade. I
percentili e i limiti dello sviluppo sono scelte di questa implementazione, non formule del BTZS.

### Modalità Zonale: tre fasi, tre nodi

Il sistema zonale di Ansel Adams: esponi per le ombre, sviluppa per le luci, poi stampa. Zona V è il grigio 18 %,
zona 0 il nero e zona X il bianco della Destinazione, con cinque zone per lato: le zone dipendono dall'uscita. Con un
Rec.709 CST (7 stop) sopra il grigio una zona vale mezzo stop, sotto 0,9; con la Scena DWG (18 stop) quasi due.

Come in camera oscura il lavoro è diviso in fasi, una per nodo. Scegli Modalità **Zonale**, poi **Fase**; la sezione
**Guida**, in fondo al pannello, cambia con la fase: lo scopo, tre passi e cosa fare dopo.

```
S-Log MetaRaw → Detail · 1 Sviluppo → Detail · 2 Stampa → Detail · 3 Presenza → CST / DRT d'uscita
```

**Guida rapida**

1. Aggiungi un Detail dopo S-Log MetaRaw, Modalità Zonale: un nodo nuovo parte da **1 · Sviluppo**. Imposta la
   Destinazione (decide le zone).
2. Metti **Ombra** e **Luce** sul viewer, premi **Misura zone**, poi scegli **Ombra sulla zona** (esposizione, di
   solito III) e **Luci sulla zona** (sviluppo, di solito VIII). Vista zone ti mostra dove cade ogni area.
3. Copia il nodo, incollalo subito dopo e scegli **2 · Stampa**: schiarisci le ombre e brucia le luci per aree, brucia
   i bordi.
4. Incolla ancora dopo e scegli **3 · Presenza**: Texture, Clarity, Dehaze.

Copiando il nodo la Destinazione passa con lui; i comandi delle altre fasi restano salvati ma non agiscono.
**Un nodo (tutte le fasi)** fa tutto insieme: è la scelta dei nodi Zonale salvati prima delle fasi.

| Fase | Controllo | Cosa fa |
|---|---|---|
| 1 Sviluppo | Ombra, Luce | i due punti sull'immagine: un'ombra e una luce in cui vuoi ancora dettaglio |
| | Misura zone | legge i due punti; l'ombra resta sulla zona scelta, la luce va dove cade con sviluppo normale (N 0) |
| | Ombra sulla zona (III) | esposizione: l'ombra misurata arriva esattamente lì |
| | Luci sulla zona (VIII) | sviluppo: la retta ruota attorno all'ombra (N+ espande, N− contrae) perché la luce arrivi lì |
| 2 Stampa | Local Shadows, Highlights, Contrast | schiarire e bruciare per aree, a dettaglio intatto  |
| | Brucia i bordi, Ampiezza bordi | scurisce i margini, fino a 2 stop, nella fascia esterna scelta |
| 3 Presenza | Texture, Clarity, Dehaze | il dettaglio, pieno nelle zone II–VIII e sfumato verso 0 e X |

**Lo Sviluppo è una curva sul singolo pixel**: non guarda i vicini, quindi non fa aloni e regge qualsiasi bordo.
Sotto l'ombra c'è un piede e sopra la luce una spalla, esponenziali, con la pendenza della retta dove cominciano: le
ombre profonde scendono morbide verso la zona 0, le luci estreme verso la X, senza tagli. La pendenza va da 0,25 a 4:
oltre, la luce si ferma prima della sua zona e la riga Zone lo dice. Il lavoro per aree (con la base e i suoi possibili
aloni) resta tutto nella Stampa, dove lo controlli tu.

La riga Zone mostra le zone misurate e quelle ottenute, lo sviluppo N± e l'estensione dell'uscita. Verificato su
campi uniformi e sul modello: ombra e luce arrivano esatte, la curva è monotona e senza salti di pendenza, lo Sviluppo
è puntuale, ogni fase ignora i comandi delle altre (`tests/test_detail.py`).

Da provare in Resolve: comandi che compaiono e spariscono al cambio di modalità e di fase, i punti sul viewer, la
lettura dell'immagine quando premi Misura, Metal (la parità CPU è verificata qui; quella Metal gira sulla CI macOS).

## Come funziona

1. `L = log2(N/0,18)` della norma di ogni pixel (la stessa del nodo principale).
2. Una **griglia di lavoro** alta fra 382 e 764 righe (riduzione a tenda), con i raggi relativi
   all'altezza: **lo stesso look a piena risoluzione, in proxy e nel viewer**.
3. **Base** B: guided filter auto-guidato di L sulla griglia (He et al.), applicato a piena
   risoluzione a una guida da cui è tolto il rumore a scala di pixel. B segue i bordi forti e
   ignora la grana.
4. **Toni locali**: Local Contrast e Local Shadows usano la stessa curva a zone
   del nodo principale, applicata a B. Il dettaglio `L − B` passa con pendenza 1: le aree si
   spostano, la texture no. Le aperture svaniscono fra −6,5 e −9,5 stop, così il pavimento del
   rumore resta dov'era.
5. **Local Highlights** separa dettaglio e bordi per ampiezza, come i filtri Laplaciani locali di
   Lightroom. `Dt = lim(L − B)` vale `L − B` fino a 0,7 × Soglia bordi e satura entro
   1,3 × Soglia bordi: oltre, il salto è un bordo e passa per la curva. La spalla del nodo
   principale (qui parte da −0,5 stop) si applica a `X = L − Dt`; dove la base è compressa il
   dettaglio sale di `0,6 · (1 − T′) · Db`, con `Db` limitato a 0,5 × Soglia bordi (sui bordi
   spinge poco) e un cancello che lascia fuori il rumore (3 volte il rumore del codice S-Log3).
   Vicino al massimo registrato la base è il clip del sensore, senza dettaglio: lì la separazione
   sfuma e il pixel segue la curva. Dove comprime, i colori vanno verso il bianco come nel nodo
   principale (Oklab, a tinta costante): una finestra bruciata non diventa una lastra color crema.
   Local Highlights in negativo non schiarisce mai un pixel, e un pixel più chiaro resta sempre
   più chiaro.
6. **Clarity**: differenza di due guided filter (0,5 % e 2,5 % dell'altezza), pesata sui
   mezzitoni.
7. **Texture**: differenza di gaussiane (0,05 % e 0,4 % dell'altezza), con un cancello che
   lascia passare solo ciò che supera il rumore del codice S-Log3 a quella luminanza.
8. Il dettaglio aggiunto è limitato in modo morbido a ±1,5 stop.
9. **Dehaze**: canale scuro morbido rispetto al velo dichiarato, trasmissione raffinata con un
   guided filter congiunto, mai sotto 0,4; sopra il livello del velo non agisce.

Nessuna statistica per fotogramma: niente flicker.

## Limiti dichiarati

- Local Shadows a ±100: su un gradino di 1 stop l'alone arriva a circa il 12 %
  del gradino, il 4-6 % su gradini di 2-3 stop; Clarity +100 circa il 7 %.
- Local Highlights −100, Soglia bordi 0,5: sul lato scuro di un bordo l'alone resta sotto il 3 %
  del gradino (0,08 stop su 3 stop). Sul lato chiaro una fascia larga come il raggio viene
  compressa meno del resto: il 39-52 % della compressione su gradini di 2-3 stop, come nella 2.0
  di prova. Con Soglia bordi 0,25 scende al 21-32 %.
- Texture tenuta da Local Highlights −100 su un'area a +4 stop: ×1,3 per dettagli di 0,1-0,3
  stop, ×1,2 a 0,5, ×0,96 a 0,8. Il nodo principale, puntuale, a −100 ne tiene ×0,18.
- Se vedi un alone: abbassa Soglia bordi.
- Vicino ai bordi forti la grana si amplifica di 1,25-1,7 volte con le zone espansive.
- Il nodo non conosce l'EI: le soglie sono ancorate alla codifica S-Log3, e Local Highlights −100
  porta sul Bianco il massimo di un'esposizione all'EI di ripresa (+6 stop). Se nel nodo principale
  cambi molto l'EI, correggi il Bianco del Detail di conseguenza.

## Prestazioni

Su GPU (Metal) un fotogramma UHD costa 6-9 ms con toni, Clarity e zone, 15-18 ms con Texture o
Dehaze (Apple M-series). La CPU è il ripiego e costa diverse decine di millisecondi.

## Verifica

Il modello di riferimento è `tests/model/detail.py`. La pipeline CPU (`src/detail`) lo segue
entro 2e-4 EV su tutti i controlli, ed è bit-identica fra 1 e N thread; il Metal segue la CPU
entro 1e-5.

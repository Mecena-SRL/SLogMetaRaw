# Proposta: plugin Log MetaRaw Noise (riduzione rumore video)

Stato: proposta iniziale. La prima versione implementata (CPU, spaziale + temporale, profili per camera e formato) è
descritta in `docs/DENOISE.md`; questa pagina resta come storia del progetto.

## 1. Obiettivo

Un quarto nodo del pacchetto (Log MetaRaw, Detail, Stabilize/Shutter, **Noise**) che toglie il rumore sapendo già
**quanto** rumore aspettarsi e **dove**: camera, ISO nativo attivo, EI scelto, codec, dai metadata letti dallo script.
Neat Video deve misurare un profilo su una zona piatta; noi lo abbiamo in anticipo dalla tabella per camera. Così la
maschera smette di essere un alpha da portare a mano nel key di un altro nodo (passaggio mai provato in Resolve): diventa
un peso interno al denoiser.

Non obiettivi della prima versione: reti neurali, rimozione di banding/macroblocchi del codec, deflicker.

## 2. Stato dell'arte (sintesi)

| Famiglia | Idea | Qualità | Costo GPU | Adatta a noi |
|---|---|---|---|---|
| Bilaterale / guidato | media pesata da distanza e somiglianza | bassa, «plastica» | minimo | solo come base |
| Wavelet (à trous, soglia per scala) | rumore separato per frequenza, soglia da σ | buona su rumore fine, buona per chroma | basso, multipass | **sì, spaziale V1** |
| Non-Local Means (Buades 2005) | media di patch simili nell'area | buona, conserva bordi | medio-alto (ricerca patch) | variante ridotta |
| BM3D / VBM4D | gruppi di blocchi simili, filtro 3D collaborativo | riferimento classico | alto, difficile su GPU | no (costo) |
| Temporale ricorsivo / a finestra | media lungo il tempo dei pixel fermi | molto alta su scene ferme | basso | sì, con compensazione |
| Temporale **con compensazione del movimento** | allinea i fotogrammi vicini (flusso ottico o block matching) poi media con pesi per patch | la migliore senza ML | medio | **sì, V2** |
| Reti (FastDVDnet, ViDeNN, Resolve Neural Engine) | denoiser addestrato | la migliore | alto, pesi da distribuire | no: parità CPU/Metal e licenze |

Neat Video (per quanto documentato pubblicamente): **profilo di rumore** per livello di luminanza e per canale, misurato
su un'area uniforme o automaticamente; filtro **temporale** con compensazione del movimento su una finestra di
fotogrammi (raggio impostabile) seguito da un filtro **spaziale** per frequenze (alte/medie/basse) e per canale
(luminanza/crominanza separati); recupero dettaglio e nitidezza finale. Il valore sta nel profilo: la forza segue il
rumore atteso, non un cursore globale. Dettagli interni non pubblici: non li replichiamo, ne seguiamo l'impostazione.

Resolve integrato: Temporal NR (motion estimation, frame 1-5) + Spatial NR (Better/Enhanced), Studio; UltraNR con
Neural Engine. Riferimento di qualità per i confronti.

## 3. Modello del rumore (cosa ci dà la maschera)

Oggi la maschera usa una soglia in stop: `soglia = D_camera − 0,5·[nativo alto] + codec − log2(EI/nativo)`. Per un
denoiser serve una **σ attesa per livello**, non una soglia. Proposta:

```
Y        = luminanza scena-lineare (dopo IDT, prima del tone mapping), grigio = 0,18
σ(Y)     = sqrt(a·Y + b)                         rumore Poisson-gaussiano
SNR(s)   = Y/σ(Y) con s = log2(Y/0,18)
vincolo  : SNR = 1 a −(soglia + sfumatura) stop  (pieno della maschera)
           SNR ≈ 4 a −soglia stop               (inizio della maschera)
```

Due punti fissano `a` (shot noise) e `b` (read noise): stesse costanti `D` della tabella, stesse regole di nativo, codec
ed EI. La maschera attuale resta il caso limite (σ alta = maschera 1). Correzione per risoluzione: un clip 6K in
timeline HD ha σ ridotta di circa `log2(k)` stop (k = rapporto lineare), dal report di ricerca.

**Profilo misurato** (opzionale, V3): come Neat, stima σ per fascia di luminanza da zone piatte del fotogramma corrente
(MAD dei dettagli wavelet fini). Serve per le camere con D stimata (C70, Z8, X-H2S, Komodo, Pocket, DJI, iPhone) e per i
codec con NR interna, dove il rumore reale è a blocchi e la varianza sottostimata. Il profilo misurato si salva nel record
della clip (`plugin_cache.py`) e si può riportare in tabella dopo la taratura con una scala di esposizioni su un grigio.

## 4. Algoritmo proposto

Dominio di lavoro: **scena-lineare con stabilizzazione della varianza** (trasformata di Anscombe generalizzata con
`a`, `b`): il rumore diventa circa uniforme (σ = 1) e un'unica soglia vale per tutti i livelli. Separazione in
luminanza + due differenze di colore (opponenti), perché la crominanza tollera un filtro molto più forte.

**V1 – spaziale (un fotogramma)**
1. Ingresso log → lineare (stesse IDT del Develop, `math/`), Anscombe.
2. Wavelet à trous (B3 spline), 5 livelli: costo lineare, multipass come Detail.
3. Soglia morbida per livello e canale: `k·σ_livello`, con `k` = Forza luma / Forza chroma.
4. Peso per pixel = maschera (o σ locale): dove la camera è pulita il filtro si spegne da solo.
5. Ricostruzione, Anscombe inversa, lineare → log: **uscita nello stesso spazio dell'ingresso**, così il nodo va prima del
   Develop e non cambia nient'altro della catena.
6. Forza 0 ⇒ uscita identica all'ingresso bit per bit (test come per il nodo maschera).

**V2 – temporale con compensazione del movimento**
1. Finestra di ±1 o ±2 fotogrammi (`getFramesNeeded`, `setTemporalClipAccess(true)`; oggi è `false` in
   `common/ParamDefs.cpp`).
2. Stima del movimento: piramide 3-4 livelli, block matching 8×8 sulla luminanza stabilizzata, raffinamento
   sub-pixel; vettori per blocco interpolati.
3. Fusione: per ogni pixel media pesata dei fotogrammi allineati, peso `exp(−d²/h²)` con `d` = distanza tra patch 5×5 e
   `h` legato a σ = 1 (dominio stabilizzato). Occlusioni e errori di flusso pesano da soli quasi 0: si torna allo
   spaziale.
4. Spaziale V1 dopo il temporale, con forza ridotta (resta meno rumore).

**V3 – opzionale**: profilo misurato; NLM spaziale ridotto (ricerca 7×7) come modalità «Qualità»; recupero dettaglio
(reiniezione di una frazione dell'alta frequenza rimossa).

## 5. Fattibilità OFX / Resolve / GPU

- **Accesso temporale**: lo standard OFX lo prevede (`kOfxImageEffectPropTemporalClipAccess`, `getFramesNeeded`) e
  Resolve lo concede ai plugin (Neat Video OFX lavora in temporale in Resolve). Da verificare su un nostro build: costo
  di cache e comportamento con velocità clip, tagli e transizioni (al taglio i vicini sono un'altra inquadratura: il peso
  per patch lo gestisce, ma va provato).
- **GPU**: oggi CPU + Metal con matematica condivisa in `math/` e parità bit/float verificata in CI. Wavelet e fusione
  temporale sono kernel per pixel/blocco: entrano nello stesso schema (come i passaggi del Detail). CUDA/OpenCL non ci
  sono: su Windows/Linux il nodo girerebbe su CPU, accettabile per V1, lento per V2 in 4K. Aggiungere CUDA è un lavoro a
  parte da decidere.
- **Memoria**: 4K float RGBA = 128 MB per fotogramma; V2 a ±2 tiene 5 fotogrammi + piramidi ≈ 0,8 GB di GPU.
- **Tile**: il pacchetto lavora senza tile (`setSupportsTiles(false)`); wavelet e flusso hanno bisogno di bordi, va
  bene così.
- **Prestazioni attese (stima, da misurare)**: V1 Metal 4K pochi ms per fotogramma; V2 decine di ms.

## 6. Pannello (proposta, nomi da fissare prima del primo rilascio)

I nomi OFX sono permanenti: questa lista va approvata prima di scriverla.

| Gruppo | Controllo | Note |
|---|---|---|
| Profilo | Profilo: Automatico (camera) / Misurato / Manuale | Automatico = tabella `docs/NOISE.md` |
| | EI, Camera (letti dal record, come nel Develop) | stessa sorgente dati |
| | Rumore manuale (stop sotto il grigio) | solo Manuale |
| Temporale | Fotogrammi: Spento / ±1 / ±2 | V2 |
| | Soglia movimento | |
| Spaziale | Forza luminanza, Forza crominanza | |
| | Dettagli fini / medi / grossi | pesi per livello wavelet |
| | Solo zone rumorose (usa la maschera) | sì/no, sfumatura |
| Vista | Uscita / Rumore rimosso / Maschera / σ attesa | controllo come *Vedi maschera* |

## 7. Posizione nella catena

`Ingresso log → Noise → Log MetaRaw (sviluppo) → Detail → …`. Il denoise lavora prima di ogni guadagno: sfrutta il modello
del sensore e non amplifica il rumore con curve e saturazione. Il nodo maschera della 3.2.1 resta per chi usa Neat Video.

## 8. Verifiche previste (prima di dichiarare una versione)

- Forza 0 ⇒ identità bit per bit; CPU = Metal entro arrotondamento float.
- Rumore sintetico Poisson-gaussiano su immagini di prova: σ stimata = σ imposta (profilo misurato), PSNR/SSIM migliori
  dell'ingresso, nessun aumento sopra la soglia della maschera.
- Scena ferma: V2 riduce σ circa di `sqrt(2r+1)`; scena in movimento: nessuna scia (test con traslazione nota).
- Clip reali del progetto (Sony, Panasonic, iPhone, DJI) in Resolve, confronto con Temporal NR di Resolve e Neat Video.

## 9. Domande aperte

1. Nodo separato o modalità del Develop? Proposta: nodo separato (prima del Develop, temporale, costo diverso).
2. CUDA per Windows/Linux in V2?
3. Nomi del pannello (sezione 6).
4. Taratura delle costanti stimate: serve una scala di esposizioni su grigio per camera (già aperto in `docs/NOISE.md`).

## Fonti

- A. Buades, B. Coll, J.-M. Morel, «A non-local algorithm for image denoising», CVPR 2005.
- K. Dabov et al., «Image denoising by sparse 3D transform-domain collaborative filtering» (BM3D), IEEE TIP 2007;
  M. Maggioni et al., VBM4D, IEEE TIP 2012.
- M. Mäkitalo, A. Foi, «Optimal inversion of the generalized Anscombe transformation for Poisson-Gaussian noise»,
  IEEE TIP 2013.
- J.-L. Starck, F. Murtagh, wavelet à trous e soglie per scala (Image Processing and Data Analysis, 1998).
- M. Tassano et al., «FastDVDnet», CVPR 2020 (riferimento ML, non adottato).
- Neat Video, documentazione utente (profilo di rumore, filtro temporale e spaziale).
- OpenFX Programming Reference: temporal clip access, `getFramesNeeded`.
- Ricerca del progetto: `docs/NOISE.md` e report «Rumore video per camera».

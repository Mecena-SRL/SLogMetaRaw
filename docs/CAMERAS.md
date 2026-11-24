# Camere e profili log

Log MetaRaw legge i metadata di ogni clip e sceglie la curva e il gamut con cui il nodo decodifica il segnale in luce
lineare. Il nome del progetto era *S-Log MetaRaw*: dopo la 3.0.0 il nome visibile è **Log MetaRaw**; gli identificativi
interni (pacchetto `slogmetaraw`, id OFX `com.slogmetaraw.*`, cartelle di installazione) restano gli stessi, così i
progetti Resolve salvati continuano ad aprirsi.

## Come viene letto il profilo

Ogni lettura dichiara quanto è affidabile (campo `profile_source`, riga *Lettura del profilo* nello script, campo
*Colore* nel nodo):

| Lettura | Significato |
|---|---|
| **tarata** | un lettore specifico, provato su clip reali di quella camera |
| **compatibile** | il nome del profilo (V-Log, F-Log2, N-Log, C-Log3, LogC, HLG…) è scritto in un campo qualsiasi dei metadata QuickTime (chiavi `mdta`, atomi `udta`, box `logs`) |
| **stimata** | nessun profilo nei metadata: decide la descrizione colore del codec (VUI H.264/HEVC, box `colr`, estensione MPEG-2); Rec.709 se tace |

Una lettura *stimata* senza marca né modello (registratori esterni, DV, file sconosciuti) non viene usata dal nodo: resta
la via *Spazio colore › Senza metadata › Sblocca controlli*, e lo script non corregge il Data Level.

## Camere tarate (provate su clip reali)

| Camera | Dove sono i metadata | Cosa si legge |
|---|---|---|
| Sony XAVC (FX, Alpha, handycam) | traccia `rtmd` + NRT XML | tutto, come prima |
| Panasonic DC-S5 (MOV) | chiave QuickTime `com.panasonic.Semi-Pro.metadata.xml` | V-Log / V-Gamut, ISO, Kelvin, modello, TC |
| iPhone 17 Pro con Blackmagic Camera | chiavi `com.blackmagic-design.camera.*`, box `logs` nella sample entry (`com.apple.apple-wide-gamut.apple-log`) | Apple Log 2 (curva Apple Log + Apple Wide Gamut), ISO, Kelvin, tint, shutter angle, diaframma, lente |
| DJI Osmo Action 5 Pro | traccia `djmd` (protobuf `dvtm_ac204.proto`, un messaggio per fotogramma) | modo colore (campo 2-4-1: 19 = D-Log M, 2 = D-Log, 9 = HLG), ISO 3-2-3-1, shutter 3-2-4-1, Kelvin 3-2-6-1, modello, seriale, firmware; variazioni campionate lungo la clip |

L'Osmo Action 4 (`dvtm_ac203.proto`) ha gli stessi percorsi secondo le tabelle DJI di exiftool: non provata su una clip.
Il percorso del modo colore (2-4-1) è stato letto sulla clip della Action 5 Pro; DJI non lo documenta.
Un altro modello DJI con la traccia `djmd` ma uno schema diverso perde solo i campi che non tornano: un valore di tipo
inatteso (testo al posto di un numero e viceversa) o fuori scala (ISO oltre 1 000 000, Kelvin fuori da 1000-50000) viene
scartato, il resto della clip si legge (`tests/test_fuzz_cameras.py`). Ogni messaggio è letto fino a 64 KB.

## Curve e gamut del plugin

Codici di `math/Transfer.h` e `tools/gamut_matrices.py` (gli stessi in `slogmetaraw/camera.py`). Range: su quale scala
di code value è scritta la formula (`datalevel.FULL_SCALE_GAMMAS`).

| Codice | Curva | Range | Fonte |
|---|---|---|---|
| 11 | V-Log | Full | Panasonic V-Log/V-Gamut reference manual (via colour-science) |
| 12 | Apple Log | Video | Apple Log Profile white paper, 2023; Apple Log 2 usa la stessa curva |
| 13 | D-Log M | Video | **nessuna formula DJI**: tabella a 17 nodi della curva Osmo Action 6 misurata con ColorChecker (`jens-krause/dlogm-dctl` v1.3), grigio 18% a 0.414; errore 0,8/1023 rms dalla misura |
| 14 | D-Log | Full | DJI D-Log/D-Gamut white paper, 2017 |
| 15, 16 | F-Log, F-Log2 | Full | Fujifilm F-Log/F-Log2 data sheet |
| 17 | N-Log | Full | Nikon N-Log specification, 2018 |
| 18, 19 | C-Log2, C-Log3 | Video | Canon Log white paper v1.2 (riflettanza / 0.9) |
| 20 | LogC3 (EI 800) | Full | ARRI ALEXA LogC curve |
| 21 | LogC4 | Full | ARRI LogC4 specification, 2022 |
| 22 | Log3G10 | Full | RED white paper (IPP2) |
| 23 | BMD Film Gen 5 | Full | Blackmagic Generation 5 Color Science, 2021 |
| 24 | HLG | Video | BT.2100 inversa dell'OETF, scalata in modo che il 38% del segnale sia il grigio 18% (BT.2408) |

| Codice | Gamut | Fonte |
|---|---|---|
| 11 | V-Gamut | Panasonic |
| 12 | Apple Wide Gamut | Apple (primarie nel builtin OpenColorIO `AppleCameras.cpp`) |
| 13 | D-Gamut | DJI white paper |
| 14 | Cinema Gamut | Canon |
| 15, 16 | ARRI Wide Gamut 3, 4 | ARRI |
| 17 | REDWideGamutRGB | RED |
| 18 | Blackmagic Wide Gamut Gen 5 | Blackmagic |
| 19 | DJI D-Log M Gamut | **dedotto**: matrice nativo → Rec.709 della Action 6 ricavata dalla LUT DJI OA6 (`jens-krause/dlogm-dctl`); DJI non pubblica il gamut |

Il punto di grigio 18% di ogni curva (codifica di 0.18) coincide con i valori pubblicati dai produttori: V-Log 0.423,
Apple Log 0.488, F-Log 0.459, N-Log 0.364, C-Log2 0.398, C-Log3 0.343, LogC3 0.391, LogC4 0.278, Gen 5 0.384.

## D-Log M: stato e prossimo passo

DJI distribuisce solo LUT D-Log M → Rec.709; la curva e il gamut non sono pubblicati e DaVinci Resolve 21.1 non ha
D-Log M nel Color Space Transform. La LUT è usata solo come **riferimento** per capire la codifica, mai applicata:

- **curva** (dal 24/11/2026): D-Log M non è una log ma una curva a S; nessuna formula chiusa la segue entro il rumore
  di misura, quindi il plugin usa una tabella a 17 nodi (log2 della luce ai codici i/16) della curva Action 6 misurata
  con ColorChecker. Sostituisce il fit di Freeman (grigio 0.400, nessuna spalla: −0,4 stop a codice 1.0);
- **gamut**: primarie R 0.771/0.319, G 0.283/0.730, B 0.123/0.028, D65, dalla matrice che la LUT OA6 implica (LUT
  separabile, residuo 0,004). Sostituisce le primarie di Freeman. La LUT "Action 5 Pro" / "Pocket 3" è la LUT Mavic 3
  Pro 2023, non separabile: `tools/lut_gamut.py` su quella dà primarie instabili;
- **limite**: misura di una sola camera (Action 6). Per Action 4/5 Pro, Pocket 3 e droni è la stima migliore
  disponibile, non una taratura (analisi in `docs/DJI_LOG.md`).

Ricerca completa per camera (D-Log, D-Log M, D-Log2, percorsi `djmd` dei droni, lacune): `docs/DJI_LOG.md`.

Resta da verificare: una clip della Osmo Action 4 e una serie di esposizioni di un ColorChecker (o cartoncino grigio) a
±1 stop, per fissare grigio e matrice su una misura e non su una LUT. I file LUT non sono nel repository.

## Verifica su clip reali (3.1.1)

Un fotogramma per camera decodificato fuori da Resolve (code value del file, matrice YCbCr dichiarata) e passato per la
matematica del nodo:

| Camera | Range del file | Nero misurato | Esito |
|---|---|---|---|
| Panasonic DC-S5, V-Log, H.264 4:2:2 | Full (flag VUI) | Y min 136 (V-Log: 128) | curva e matrice V-Gamut uguali al reference manual Panasonic (0.423 / 128 / 602; matrice a 6 cifre); WB di ripresa 4000 K sotto luce diurna: muri a xy 0.295, 0.331 (≈ 7700 K rispetto a D65), immagine fredda per scelta di ripresa, non per la decodifica |
| iPhone, Apple Log | Video | Y min 119 | neutri e incarnati plausibili |
| DJI Osmo Action 5 Pro, D-Log M | Video | Y min 46 | muro grigio a xy 0.320, 0.336, Y 0.18 (con la curva e il gamut di Freeman, prima del 24/11/2026) |

Con la S5 conta il Data Level: il file è Full; se Resolve lo decodifica Video il bianco 90% sale di +0,37 stop, il nero
va sotto zero e la saturazione cresce. Lo script imposta Data Level = Full; con Data Level su Auto il nodo non corregge.
Lo script non imposta ancora *Input Color Space* per le camere non Sony (in RCM la clip resta Rec.709 per Resolve):
i nomi Resolve di V-Log, Apple Log e D-Log M non sono stati verificati.

## Aggiungere una camera

1. Un lettore in `slogmetaraw/vendors.py` (chiavi QuickTime/XML) o un modulo come `dji.py` (tracce per fotogramma) che
   riempia le chiavi normalizzate (`manufacturer`, `model`, `iso`, `white_balance_k`, `capture_gamma`,
   `color_primaries`, `profile_source`).
2. Se la curva è nuova: codice in coda a `math/Transfer.h`, `tests/develop_model.py`, `ColourSpaces.cpp`,
   `camera.GAMMA_CODE`; gamut in coda a `tools/gamut_matrices.py`; `python3 tools/build_math.py`; scala in
   `datalevel.FULL_SCALE_GAMMAS`; golden `params_develop.txt` aggiornato di proposito.
3. Un test con un file sintetico in `tests/test_cameras.py` (mai clip reali né dati personali nel repository).

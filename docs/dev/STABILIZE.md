# Dati di movimento e nodo Stabilize — ricerca e piano

Stato: **fase 1 implementata** (acquisizione profonda Sony), il resto è piano.

## Cosa registrano le camere

| Camera | Dati | Dove | Stato in Log MetaRaw |
|---|---|---|---|
| Sony con IMU (FX30, FX3, FX6, a7 IV, a7S III, ZV-E1…) | giroscopio + accelerometro (i16 x/y/z, scala f32, ~1–2 kHz), tempo di lettura del sensore, fuoco/zoom per fotogramma, IBIS e OSS | traccia `rtmd` (MP4) o ANC ST 436 (MXF), tag 0xe4xx/0xe5xx | **letto** (`slogmetaraw/motion.py`) |
| Sony senza IMU (es. HDR-CX405, a6300) | solo fuoco/diaframma per fotogramma | `rtmd` | nessun giroscopio: `motion.read` → None |
| DJI Osmo Action 4/5, Pocket 3 | quaternioni di assetto per ogni lettura IMU (non giroscopio grezzo) | traccia `djmd`, protobuf | da fare, dopo la correzione della lettura Action 4 (altro thread) |
| GoPro | GYRO, ACCL con SCAL, orientamento da ORIN/MTRX (dipende dal modello) | traccia GPMF `gpmd` | non previsto ora; Gyroflow lo legge già |
| iPhone + Blackmagic Camera | nessun dato di movimento nella clip di prova (`iPhoneA001_…_C018.mov`: solo video, audio, timecode) | — | serve una clip con registrazione del movimento, se l'app la offre |
| Panasonic S5 | nessun giroscopio nel file | — | niente da leggere |

Fonti: telemetry-parser (`src/sony/rtmd_tags.rs`, `src/util.rs`, `src/gyroflow/gcsv.rs`, MIT/Apache, AdrianEddy),
gpmf-parser (README, GoPro). Clip di prova del progetto: nessuna ha il giroscopio, i test usano clip sintetiche.

## Fase 1 (fatta): acquisizione profonda

- Spunta **Acquisizione profonda (IMU)** nella finestra; `--deep` da riga di comando. Il lettore figlio parte con
  `--helper --deep`; limite 180 s per clip (150 s di lettura, poi risultato parziale).
- Per ogni fotogramma: giroscopio e accelerometro (interi grezzi), più fuoco, focale, diaframma.
- File accanto alla scheda del plugin (`<cache>/<fnv1a64(percorso)>`):
  - `.gcsv`: log IMU Gyroflow (`tscale` 1 µs, `gscale` → °/s, `ascale` → g, orientamento convertito come
    telemetry-parser: X↔Y, Z invertito). Letture distribuite uniformemente nel fotogramma.
  - `.lens.csv`: `frame,focus_m,focal_mm,iris_f`.
- Resolve: lo scripting non accetta dati di giroscopio su una clip; i dati restano nei file, letti dal futuro nodo.

Da verificare su una clip Sony reale con giroscopio: orientamento, unità (0xe438), confronto con Gyroflow.

## Fase 2: un solo pacchetto con quattro nodi

Log MetaRaw (sviluppo base), Detail (colore), **Stabilize**, **Shutter**: stesso bundle OFX, stessa scheda per clip.
Il plugin di stabilizzazione sviluppato a parte va portato qui; serve il suo sorgente (repository o cartella).

Piano Stabilize (da confermare dopo aver visto il plugin esistente):
1. Legge `<hash>.gcsv` con lo stesso hash di `Files.cpp`; integra il giroscopio in quaternioni.
2. Smorzamento (finestra temporale regolabile), orizzonte opzionale dall'accelerometro.
3. Correzione rolling shutter con `frame_readout_time`, per riga.
4. Ritaglio/zoom automatico minimo; obiettivo dal `.lens.csv` (focale) e dalla distorsione Sony quando c'è.
5. Sincronizzazione: offset regolabile; prima versione senza analisi ottica.

Shutter: da definire con il plugin esistente.

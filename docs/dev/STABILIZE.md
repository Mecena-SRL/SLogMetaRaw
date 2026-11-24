# Dati di movimento, Stabilize e Shutter in un solo pacchetto

Stato: fasi 1 e 2 fatte (acquisizione profonda, un solo pacchetto macOS), fase 3 da fare.

## Decisione (24/11/2026, Ivan)

Un solo database per i dati di movimento: **MecenaClipDB**, scritto da `mecena-clipdb` di Mecena Motion
(repo `Mecena-SRL/GyroMotion`). Log MetaRaw non ha un parser IMU suo: la spunta del script lancia lo strumento. Prima
di questa scelta un lettore Sony scritto qui (rtmd, MP4 e MXF) era stato provato su clip reali (FX6 MXF 50p, 40
letture per fotogramma a 2000 Hz; FX30 MP4 25p, 80): accelerometro a riposo 0,998 e 1,001 g. È stato tolto perché
`mecena-clipdb` legge le stesse camere e anche le altre.

## Cosa registrano le camere

| Camera | Dati | Letti da mecena-clipdb (Gyroflow) |
|---|---|---|
| Sony con IMU (FX6, FX30, FX3, a7 IV…) | giroscopio + accelerometro a 2000 Hz, readout, fuoco/zoom per fotogramma | sì |
| Sony senza IMU (HDR-CX405, a6300) | solo obiettivo | nessun dato di movimento → avviso |
| DJI Osmo Action / Pocket | quaternioni di assetto (`djmd`) | sì |
| GoPro | GPMF (GYRO, ACCL, CORI) | sì |
| iPhone + Blackmagic Camera | nessun dato di movimento nella clip di prova | — (Shutter: stabilizzazione ottica leggera) |
| Panasonic S5 | nessun giroscopio nel file | — |

## Fase 1 (fatta): acquisizione profonda

- Spunta **Acquisizione profonda (IMU)**; `--deep` da riga di comando. Il lettore figlio parte con `--helper --deep`;
  dopo `read_clip` esegue `mecena-clipdb scan <database> <clip>` (limite 150 s; la finestra aspetta 180 s).
- Strumento: `MECENA_CLIPDB_BIN`, poi il bundle `/Library/OFX/Plugins/MecenaMotion.ofx.bundle/Contents/Resources/`,
  poi il `PATH`. Database: `MECENA_CLIPDB`, poi il primo registrato in `clipdb.json`, poi `~/Movies/MecenaClipDB`.
  È la stessa regola dello script «Mecena ClipDB».
- Nei Dettagli compare la sezione MOVIMENTO, con il riepilogo e la cartella della clip. Se Mecena Motion manca o la
  clip non ha dati di movimento, la clip si legge come prima e compare un avviso.
- Resolve: gli script non possono assegnare dati di giroscopio a una clip, quindi i dati restano nel database.

## Fase 2: un solo pacchetto (fatta, da provare su Mac)

- Sorgenti di GyroMotion copiati in `motion/` senza storia (scelta di Ivan: verranno riscritti molto). Stabilize è un
  fork di gyroflow-ofx (Rust, core Gyroflow con le patch di `motion/release/*.patch`), Shutter è in C++/Metal.
  Entrambi stanno in un solo binario OFX, solo macOS.
- La categoria OpenFX è **Log MetaRaw** (`set_grouping` in `motion/vendor/openfx/src/gyroflow.rs`,
  `kOfxImageEffectPluginPropGrouping` in `motion/src/plugin.cpp`). Gli identificativi non cambiano.
- Build: `motion/release/build-mac.sh`. La CI la lancia in `.github/workflows/motion.yml` quando cambia `motion/`.
  `packaging/build_installer.sh` la lancia e mette il bundle nel pkg come secondo componente (`it.mecena.motion.pkg`,
  la stessa ricevuta del pkg separato), con gli script `motion/release/installer/{pre,post}install`. La versione di
  Motion viene da `motion/release/Info.plist`.
- Il disinstallatore toglie bundle, script e `mecena-clipdb`, e lascia il MecenaClipDB.
- Restano da fare: Linux e Windows (il core Gyroflow compila anche lì, Shutter ha già il percorso CPU) e una
  versione unica per i due prodotti.

## Fase 3: DJI Action 4

La lettura dei metadata della Osmo Action 4 la sistema un altro thread. Per i dati di movimento basta mecena-clipdb:
va controllata una clip reale della Action 4.

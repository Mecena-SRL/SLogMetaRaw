# Graphify — mappa strutturale (dev, non pubblicata)

Grafo di navigazione del repository: moduli, dipendenze fra loro, punto di ingresso. Rispecchia
`main` alla 2.4.0. Va rigenerato quando cambia la struttura dei moduli, non a
ogni commit.

## Python — `slogmetaraw/` (lettura metadata, CLI, script Resolve)

```
__main__.py ──> extract.py ──> codec.py
   │                 ├──────> datalevel.py
   │                 ├──────> mp4.py
   │                 ├──────> mxf.py
   │                 ├──────> nrt.py
   │                 └──────> rtmd.py
   │
ui.py ──> i18n.py
   ├────> osx_utils.py
   ├────> paths.py
   ├────> plugin_cache.py ──> camera.py
   │                     ├──> datalevel.py
   │                     ├──> paths.py
   │                     └──> resolve_io.py ──> datalevel.py
   └────> update.py ──> paths.py

connect.py            (standalone: apre/attiva Resolve e scrive una clip via scripting server)
resolve_script/SLogMetaRaw.py   (entry point GUI eseguito dentro Resolve, usa ui.py;
                                  duplica la logica di paths.py perché gira prima che il
                                  pacchetto sia importabile)
```

- **Entry point CLI/plugin**: `__main__.py` — modalità `--cache`, `--to-resolve`,
  `--update-check`, `--helper` (contratto con il plugin OFX C++, vedi sotto).
- **Entry point GUI**: `resolve_script/SLogMetaRaw.py` → `slogmetaraw.ui`.
- `extract.py` è l'hub di lettura: instrada MP4/MXF ai parser di formato e normalizza l'output.
- `plugin_cache.py` e `resolve_io.py` sono il ponte verso il nodo OFX (cache su disco, scrittura
  CSV per il fusion/script panel).
- `paths.py` è la sola fonte delle cartelle di stato per sistema (supporto/log); il plugin C++
  usa le stesse cartelle da `common/Files.cpp`, non importandolo ma rispecchiandole a mano.

## C++ — `ofx/SLogMetaRaw/src/` (plugin OFX per DaVinci Resolve)

```
Plugin.cpp  (registra le due factory OFX)
  ├──> develop/DevelopFactory.h ──> DevelopEffect, DevelopProcessor, DevelopSync,
  │                                 BuildParams, ToneParams, ClipMeta, LutExport (DevelopLut.cpp)
  └──> detail/DetailFactory.h   ──> DetailEffect, DetailPasses

common/  (condiviso dai due nodi)
  ClipCache, ColourSpaces, Files, FlatJson, ImageLayout, ParamDefs, Update, UpdateBadge,
  Version, ZoneParams, Child (API di processo figlio: Child.cpp su POSIX, ChildWin.cpp su Windows)
```

- **Develop** (nodo principale): legge i metadata via `common/Child.h` → `python3 -m
  slogmetaraw --cache/--to-resolve`, li cachea (`ClipCache`), costruisce i parametri
  (`BuildParams`, `ToneParams`) e sincronizza col pannello (`DevelopSync`: migrazione delle impostazioni,
  identità della clip, ingresso del nodo `nodeEncoding`, contagocce `pickWhite`; `ToneParams`: visibilità dei
  controlli di Toni e Zone). Pannello a gruppi annidati: Ripresa, Spazio colore (Ingresso, Senza metadata,
  Uscita), Toni, Zone, Dati di ripresa.
- **Detail** (secondo nodo): recupero locale/texture; non legge metadata clip, lavora sui pixel
  in ingresso (`DetailPasses`, GPU Metal + fallback CPU). Modalità Zonale e Split grade (Standard e Cinematografico eliminati); la
  Zonale in fasi (`zonalPhase`): Sviluppo puntuale (`dt_zonal_curve`), Stampa per aree + `dt_edge_burn`,
  Presenza. Parametri: `math/DetailHost.h::dt_prepare`, per pixel: `math/Detail.h::dt_gain`.
- **Esporta LUT** (`develop/DevelopLut.cpp` + `LutExport.{h,cpp}`): salva un .cube 3D campionando
  `sm_develop` (lo stesso `math/` del render) sulla griglia size³; `tools/export_lut.py` è il
  prototipo Python equivalente, usato per verificare l'output senza ricompilare il plugin.
- `common/Update*` interroga GitHub Releases (stesso schema JSON di `slogmetaraw/update.py`, letto
  anche lato Python per il badge nello script window).
- `common/Child.h` dichiara l'API, `Child.cpp`/`ChildWin.cpp` la implementano per piattaforma
  (spawn, redirect, timeout del processo Python).

## Packaging e release (non runtime)

```
packaging/linux/    build_plugin.sh, build_package.sh, build_packages.py (.deb/.rpm/.run/.tar.gz),
                     check_abi.py, install.sh, smoke_in_container.sh
packaging/windows/  SLogMetaRaw.iss, build_package.ps1, install.ps1
tools/               check_version.py, build_math.py (genera gen/DevelopMath.h da math/)
```

`tools/atlas/` (Atlante della logica, strumento di sviluppo): estrae dal codice Python funzioni, chiamate,
import, binding UI e test e li unisce al modello dei comportamenti; vedi `docs/dev/CONFIGURATORI.md`.

Non fanno parte del grafo di runtime del plugin: producono gli installer e verificano l'ABI/la
versione prima di una release. Non pubblicati ancora nelle release notes.

## Confini e contratti da non rompere silenziosamente

- Formato JSON flat di `__main__.py --cache`/`--to-resolve`/`--update-check`: consumato da
  `common/Child.cpp`/`ChildWin.cpp` (C++) — cambiare le chiavi richiede aggiornare entrambi i lati.
- `camera.py` — i codici gamut/gamma devono restare identici a
  `ofx/SLogMetaRaw/math/` (compilato in `gen/DevelopMath.h` da `tools/build_math.py`).
- `datalevel.py` è importato sia da Python (`extract`, `plugin_cache`, `resolve_io`) sia
  concettualmente specchiato in `common/` lato C++ per le stesse scale di codice.
- `paths.py` e le cartelle di `common/Files.cpp` devono restare le stesse per sistema: cache e log
  scritti da un lato devono restare leggibili dall'altro.
- `gen/DevelopMath.h` è generato da `math/` via `tools/build_math.py`: non va mai editato a mano.

_Ultimo aggiornamento: 01/11/2026 — verificata la mappa contro main alla 2.4.0 (nessun modulo nuovo o rimosso:
cambiano il pannello Develop e `math/HostOnly.h`, che ora risolve Temperatura e Tint da un colore); aggiunto
`tools/atlas/`. La freschezza di questa mappa (versione e moduli citati) è controllata
dall'Atlante, vista Manutenzione._

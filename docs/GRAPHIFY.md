# Graphify — mappa strutturale (dev, non pubblicata)

Grafo di navigazione del repository: moduli, dipendenze fra loro, punto di ingresso. Riflette
la 2.2.1 (`main`). Va rigenerato quando cambia la struttura dei moduli, non a ogni commit.

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

connect.py             (standalone: server di scripting Resolve, scrittura di una singola clip —
                         usato da `--to-resolve`)
resolve_script/SLogMetaRaw.py   (entry point GUI eseguito dentro Resolve, usa ui.py)
```

- **Entry point CLI/plugin**: `__main__.py` — modalità `--cache`, `--to-resolve`,
  `--update-check`, `--helper` (contratto con il plugin OFX C++, vedi sotto).
- **Entry point GUI**: `resolve_script/SLogMetaRaw.py` → `slogmetaraw.ui`.
- `extract.py` è l'hub di lettura: instrada MP4/MXF ai parser di formato e normalizza l'output.
- `plugin_cache.py` e `resolve_io.py` sono il ponte verso il nodo OFX (cache su disco, scrittura
  CSV per il fusion/script panel).
- `paths.py` fissa le cartelle di stato per OS (cache, log); lo stesso contratto di cartelle è
  rispecchiato lato C++ in `common/Files.cpp`.

## C++ — `ofx/SLogMetaRaw/src/` (plugin OFX per DaVinci Resolve)

```
Plugin.cpp  (registra le due factory OFX)
  ├──> develop/DevelopFactory.h ──> DevelopEffect, DevelopProcessor, DevelopSync,
  │                                 BuildParams, ToneParams, ClipMeta, DevelopLut, LutExport
  └──> detail/DetailFactory.h   ──> DetailEffect, DetailPasses

common/  (condiviso dai due nodi)
  ClipCache, ColourSpaces, Files, FlatJson, ImageLayout, ParamDefs, Update, UpdateBadge,
  Version, ZoneParams (toni/zone, usato da detail e develop/ToneParams), Child (spawn del
  processo Python __main__.py)
```

- **Develop** (nodo principale): legge i metadata via `common/Child.h` → `python3 -m
  slogmetaraw --cache/--to-resolve`, li cachea (`ClipCache`), costruisce i parametri
  (`BuildParams`, `ToneParams`) e sincronizza col pannello (`DevelopSync`). `DevelopLut` +
  `LutExport` scrivono la LUT .cube dal pulsante "Esporta LUT".
- **Detail** (secondo nodo): recupero locale/texture; non legge metadata clip, lavora sui pixel
  in ingresso (`DetailPasses`, GPU Metal + fallback CPU).
- `common/Update*` interroga GitHub Releases (stesso schema JSON di `slogmetaraw/update.py`, letto
  anche lato Python per il badge nello script window).

## Confini e contratti da non rompere silenziosamente

- Formato JSON flat di `__main__.py --cache`/`--to-resolve`/`--update-check`: consumato da
  `common/Child.cpp` (C++) — cambiare le chiavi richiede aggiornare entrambi i lati.
- `camera.py` — i codici gamut/gamma devono restare identici a
  `ofx/SLogMetaRaw/gen/DevelopMath.h`.
- `datalevel.py` è importato sia da Python (`extract`, `plugin_cache`, `resolve_io`) sia
  concettualmente specchiato in `common/` lato C++ per le stesse scale di codice.

_Ultimo aggiornamento: manutenzione 2.2.1, 05/10/2026 — aggiunti `paths.py`, `DevelopLut`/
`LutExport`, corretto il ruolo di `connect.py` e il percorso di `DevelopMath.h`._

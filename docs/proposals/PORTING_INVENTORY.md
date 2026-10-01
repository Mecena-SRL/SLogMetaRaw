# Inventario per la portabilità — #26 (#10 Resolve 20, #11 Windows, #12 Linux)

Stato: inventario dal codice, **non** verificato su Windows, Linux o Resolve 20. Dove scrivo "non verificato" è
un'ipotesi dal sorgente.

## Cosa è già portabile
- `ofx/SLogMetaRaw/math/*.h`: tutto il calcolo per pixel (develop, tone, detail, false color) è C++ puro, compilabile
  su Linux (`tests/ofx_math_test.cpp`, `detail_test.cpp`: già compilati con g++/clang++ nei test). Il kernel Metal è una
  traduzione generata dallo stesso sorgente (`tools/build_math.py`), la parità è già verificata.
- Il percorso CPU del plugin esiste ed è il riferimento della parità (`DevelopProcessor::multiThreadProcessImages`,
  `DetailPasses.cpp`).
- Il pacchetto Python `slogmetaraw` (lettura di MP4/MXF/NRT) è puro Python: gira su qualunque sistema.

## Dipendenze da macOS nel plugin C++ (da sostituire o isolare)
| File | Cosa | Per portare |
|---|---|---|
| `src/common/Files.cpp` | `CoreFoundation` per la normalizzazione NFC; `~/Library/Application Support`; `st_flags & SF_DATALESS` (segnaposto iCloud) | NFC con un'implementazione portabile; cartelle per sistema (XDG su Linux, `%APPDATA%` su Windows); `SF_DATALESS` solo su macOS |
| `src/common/ClipCache.cpp` | `st_mtimespec` (su Linux è `st_mtim`) | macro per sistema |
| `src/common/Child.cpp` | `posix_spawn` con `POSIX_SPAWN_CLOEXEC_DEFAULT` (estensione Apple); percorsi fissi `/Applications/DaVinci Resolve/...`; log in `~/Library/Logs` | Linux: `posix_spawn` senza quel flag e chiusura esplicita dei descrittori; Windows: `CreateProcess` + pipe, riscrittura quasi completa |
| `src/develop/DevelopEffect.cpp`, `DevelopProcessor.h`, `detail/DetailEffect.cpp`, `common/ParamDefs.cpp` | già protetti da `#ifdef __APPLE__` per Metal | su altri sistemi resta il CPU: nessuna modifica per far compilare |
| `metal/*.mm`, `metal/kernels/*` | Metal | restano solo macOS; per CUDA/AMD/Intel serve una backend GPU nuova (non esiste) |
| `Makefile` | solo macOS (`-arch`, `-framework`, `codesign`) | Makefile o CMake per Linux |
| `packaging/` e `install.sh` | `.dmg`/`.pkg`, percorsi del Resolve macOS | installer per sistema |

## Dipendenze da macOS nello script Python (da isolare)
`osx_utils.py` (CoreGraphics, `afplay`, `osascript`), `connect.py` e `ui.py` (`/sbin/ifconfig`, `/sbin/mount`,
`/usr/bin/osascript`), percorsi `~/Library/...` in `ui.py`, `resolve_script/SLogMetaRaw.py`, `update.py`, `plugin_cache.py`,
`i18n.py` (`/usr/bin/defaults read AppleLanguages`). Su Linux/Windows la finestra dello script (UIManager di Resolve) è
la stessa API: **non verificato** che si comporti uguale.

## Resolve 20 (#10)
- Il codice dichiara 21.1 come versione testata. Le chiamate di scripting usate (`GetMediaPool`, `GetClipProperty`,
  `SetClipProperty`, `SetMetadata`, `SetThirdPartyMetadata`, `UIManager`) esistono da versioni precedenti alla 20: **non
  verificato**.
- I nomi degli spazi colore in ingresso sono "verificati sulla 21.1" (`resolve_io.RESOLVE_COLOR_SPACES`): potrebbero
  chiamarsi diversamente prima. Questo è il rischio concreto.
- Le note dell'interfaccia (`ui.py`) citano comportamenti della 21.x (chiusura finestra, proxy visibile dopo `Hide()`).
- Il plugin usa le API OpenFX 1.4 con gestione colore (`kOfxImageEffectPropColourManagementStyle`). Resolve 20 le
  supporta, **non verificato**.
- Per la verifica serve un Resolve 20 su macOS: stesso Mac, versione diversa. Il lavoro sul codice è probabilmente
  piccolo, il lavoro grosso è provare.

## Ordine suggerito
1. **Linux solo CPU, senza Resolve**: far compilare il plugin e i test su Linux (isolare `Files.cpp`, `ClipCache.cpp`,
   `Child.cpp`, un Makefile Linux). Verificabile con i test esistenti, senza hardware. È il passo che sblocca CI più
   forte (parità CPU) e il porting.
2. Resolve 20 su macOS (stesso hardware, prova manuale).
3. Linux in Resolve (serve macchina Linux con Resolve Studio e GPU).
4. Windows (riscrittura di `Child.cpp`, installer, CUDA/AMD/Intel: lavoro grosso).

## Non fattibile da qui
Provare su Windows/Linux/Resolve 20, nuovi backend GPU, installer. Il passo 1 sì, ma è un cambio di più file del plugin che
non posso compilare fino in fondo senza gli header OpenFX (non presenti in questa sessione), quindi lo farei solo
dopo il tuo via libera.

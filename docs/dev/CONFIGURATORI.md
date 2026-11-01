# Configuratori di sviluppo: inventario, fonti, schema

Strumenti dell'ambiente di sviluppo, esterni all'app. Il programma (script Resolve + plugin OFX) è l'oggetto osservato.
Prima sessione di allineamento: 01/11/2026.

## 1. Inventario delle capacità

| Capacità | Dove | Stato | Note |
|---|---|---|---|
| Mappa moduli (Graphify) | `docs/GRAPHIFY.md` | **funzionante, manuale** | Disegnata a mano. Freschezza (versione, moduli citati) controllata dall'Atlante. Nessuno strumento graphify/codegraph installato nel repo. |
| Estrazione strutturale Python | `tools/atlas/atlas.py` | **funzionante** | AST: funzioni, righe, impronte, chiamate, import, binding UI `win.On.X.Y`, test. |
| Estrazione C++ (plugin OFX) | — | **mancante** | Il plugin è citato solo a livello di file (esistenza controllata). |
| Atlante dei comportamenti | `tools/atlas/` | **funzionante, primo incremento** | 2 comportamenti reali modellati; testo e collegamenti manuali, riferimenti estratti e controllati. |
| Verifica dei comportamenti | `tests/` (unittest) | **funzionante** | L'Atlante cita i test e con `--run-tests` ne registra l'esito. |
| Parametri plugin (golden) | `tests/golden/params_*.txt` | **funzionante** | Fonte verificata dei parametri OFX: base possibile per il catalogo dei controlli del plugin. |
| Design System / catalogo componenti | — | **mancante** | La finestra usa i widget UIManager di Resolve (`ui.py`), il plugin i parametri OFX (`ParamDefs.cpp`). Nessun catalogo. |
| Configuratore UI / navigazione | — | **mancante** | `tools/render_launcher.py`, `packaging/guide/*.html` sono materiali di prodotto, non configuratori. |
| Grafici dimostrativi | `tools/make_tone_chart.py`, `make_falsecolor_chart.py`, `docs/*.png` | **dimostrativo** | Immagini generate da `math/` per la documentazione utente; non collegate a un modello. |
| Bibliotecario / contesto selettivo | — | **parziale** | Pulsante «Copia contesto per l'agente» nell'Atlante (elemento, livello, relazioni, fonti, nota). Nessun indice di decisioni/skill. |
| JEV | — | non presente | Facoltativo. |

## 2. Fonti autorevoli

| Informazione | Fonte autorevole | Copie derivate |
|---|---|---|
| Comportamento effettivo | codice in `slogmetaraw/`, `resolve_script/`, `ofx/` | spiegazioni dell'Atlante (`model.json`, manuali) |
| Struttura (funzioni, chiamate, eventi UI) | codice, estratto a ogni generazione | `build/atlas/atlas.json` (non committato) |
| Verifica | `tests/` + CI | esiti registrati in `build/atlas/` con `--run-tests` |
| Comportamenti, collegamenti, proposte | `tools/atlas/model.json` | pagina generata |
| Disposizione grafica | `model.json › view` (condivisa); spostamenti locali nel browser | — |
| Mappa moduli | `docs/GRAPHIFY.md` | — |
| Versione | `slogmetaraw/__init__.py` | README, RELEASE_NOTES (controllati da `tools/check_version.py`) |

Nessuna copia indipendente: la pagina si rigenera; righe e chiamate non si scrivono mai a mano.

## 3. Schema minimo condiviso (`slogmetaraw-atlas/1`)

Identità stabili, uguali in tutti i configuratori futuri:
- programma `prog`; sottosistema `sub.<nome>`; comportamento `beh:<area>.<nome>`; blocco e collegamento
  `<comportamento>/<id locale>`; proposta `<comportamento>/P-NNN`;
- funzione `<file>::<nome qualificato>` (es. `slogmetaraw/ui.py::main.show_details`); evento UI
  `binding:<Widget>.<Evento>`; test `tests/<file>::<Classe>.<metodo>`; modulo `mod:<file>`.

Catena prevista: regola → componente → schermata → azione → comportamento → modulo → implementazione → verifica.
Oggi esistono: azione (`binding:`), comportamento, modulo, implementazione (`::`), verifica (test).

Elementi:
- blocco: `kind` (event, function, decision, data, persistence, process, async, external, end), `label`, `text`,
  `basis`, `refs`;
- collegamento: `from` → `to` (direzione), `kind` (trigger, call, request, response, data, async, error,
  persistence, decision, control), `label` (significato). Scambi bidirezionali = due collegamenti (richiesta e
  risposta). I `call` sono confermati solo se la chiamata è nell'AST (`evidence: estratto|dedotto`);
- `basis`: intenzione · struttura estratta · dedotto · osservato · verificato;
- `refs`: `code` | `binding` | `test` | `file`; stato calcolato: estratto, mancante, non revisionato, modificato,
  presente, superato/fallito;
- `fingerprints`: impronta (sha1 del sorgente normalizzato) del codice citato al momento della revisione. Codice
  cambiato → «modificato» (da rivedere), non un errore;
- `proposals`: problema, modifica, impatto, verifica, stato (proposta · applicata), mostrate separate dallo stato attuale.

Revisione: ogni generazione registra commit, ramo, versione e file con modifiche locali (inclusi nell'estrazione).

## 4. Atlante: come aprirlo

```
python3 tools/atlas/atlas.py --run-tests
# apri build/atlas/index.html nel browser (file autonomo, nessuna rete)
```
- Albero a sinistra: programma → sottosistema → comportamento → blocco → funzione; ricerca.
- Diagramma: corsie per attore (utente, thread UI, thread lettore, processo, disco, Resolve), collegamenti con verso
  e significato; trascinare un blocco cambia solo la vista (bordo tratteggiato, «Ripristina» in Manutenzione).
- Pannello: spiegazione, base, fonti con righe estratte e link al commit, ingressi/uscite, chiamate estratte,
  proposte, nota e «Copia contesto per l'agente».
- Modalità Naviga/Annota; vista Manutenzione: riferimenti rotti, codice da rivedere, moduli non assegnati,
  freschezza di Graphify, cosa è e non è estratto.
- Stato dell'editor (rami aperti, ricerca, scroll dell'albero e di ogni diagramma, selezione, modalità, note,
  spostamenti) conservato nel browser, separato dal modello.

## 5. Percorso dimostrato (01/11/2026)

Scenario «Cosa succede quando seleziono una clip?» → diagramma `beh:ui.details.sections` → spiegazione → codice
(`show_details`) → discussione: le sezioni dei Dettagli tornavano ai default a ogni selezione o nuova lettura →
proposta P-001 → modifica (`_remember_detail_sections`, `_section_open`) con test che fallisce prima e passa dopo
(`test_details_sections_stay_as_the_user_left_them`) → modello riestratto e impronte registrate (`--accept`).
P-002 (riselezionare la clip dopo una nuova lettura) resta proposta: serve un'API verificata del Tree di Resolve.

## 6. Limiti noti

- Testi, decisioni e collegamenti sono scritti a mano: i riferimenti sono controllati, il contenuto no.
- C++ non estratto; i comportamenti del plugin non sono ancora modellati.
- La lettura di `Expanded` dagli elementi del Tree di Resolve è supposta, non provata in Resolve.
- Le note restano nel browser di chi le scrive; per discuterle si copia il contesto nella conversazione.

## 7. Prossimi incrementi

1. Catalogo dei controlli del plugin dai parametri OFX (`ParamDefs.cpp` / `tests/golden`), primo nucleo del Design
   System con identità `param:<nodo>.<nome>`.
2. Mappa della finestra dello script (widget di `ui.py` estratti dall'AST) come primo configuratore UI, collegata ai
   `binding:` già presenti.
3. Comportamenti successivi: «Scrivi in Resolve», «Copia su clip senza metadata», lettura `--cache` dal plugin.
4. Estrazione C++ minima (funzioni e chiamate) per collegare i comportamenti del plugin.

# Configuratori di sviluppo: inventario, fonti, schema

Strumenti dell'ambiente di sviluppo, esterni all'app. Il programma (script Resolve + plugin OFX) è l'oggetto osservato.
Prima sessione di allineamento: 01/11/2026.

## 1. Inventario delle capacità

| Capacità | Dove | Stato | Note |
|---|---|---|---|
| Mappa moduli (Graphify) | `docs/GRAPHIFY.md` | **funzionante, manuale** | Disegnata a mano. Freschezza (versione, moduli citati) controllata dall'Atlante. Nessuno strumento graphify/codegraph installato nel repo. |
| Estrazione strutturale Python | `tools/atlas/atlas.py` | **funzionante** | AST: funzioni, righe, impronte, chiamate, import, binding UI `win.On.X.Y`, test. |
| Estrazione C++ (plugin OFX) | `tools/atlas/atlas.py` (`cpp_symbol`) | **parziale** | Ricerca testuale delle definizioni (righe, impronta), non un parser: niente grafo delle chiamate C++. |
| Atlante dei comportamenti | `tools/atlas/` | **funzionante** | 3 comportamenti: Leggi metadata, Seleziona clip → Dettagli, Detail: colore e passi. Testo e collegamenti manuali, riferimenti estratti e controllati. |
| Verifica dei comportamenti | `tests/` (unittest) | **funzionante** | L'Atlante cita i test e con `--run-tests` ne registra l'esito. |
| Parametri plugin (golden) | `tests/golden/params_*.txt` | **funzionante** | Fonte verificata dei parametri OFX: base possibile per il catalogo dei controlli del plugin. |
| Design System / catalogo componenti | vista Interfaccia › Catalogo | **primo nucleo, estratto** | Helper `define*` di `ParamDefs.h` e tipi UIManager, con utilizzi. Mancano fondamenti grafici, varianti e regole d'uso scritte. |
| Configuratore UI / navigazione | vista Interfaccia | **funzionante, primo incremento** | Anteprima di finestra script, pannello Develop e Detail dalle fonti; selezione, note, proposte di modifica (testo, tooltip, default) esportabili per l'agente. L'anteprima è una rappresentazione, non uno screenshot. Le proposte non modificano il codice da sole. |
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

Altre identità: widget `ui:script.<ID>`; controllo OFX `param:<develop|detail>.<nome>` (nome permanente: Resolve
salva i nodi per nome); componente `comp:ofx.<helper>` / `comp:script.<Tipo>`; funzione C++ `<file>::<nome>`.

Catena prevista: regola → componente → schermata → azione → comportamento → modulo → implementazione → verifica.
Oggi esistono: componente (`comp:`), schermata/controllo (`ui:`, `param:`), azione (`binding:`), comportamento,
modulo, implementazione (`::`), verifica (test, golden). Manca la regola (fondamenti del Design System).

Elementi:
- blocco: `kind` (event, function, decision, data, persistence, process, async, external, end), `label`, `text`,
  `basis`, `refs`;
- collegamento: `from` → `to` (direzione), `kind` (trigger, call, request, response, data, async, error,
  persistence, decision, control), `label` (significato). Scambi bidirezionali = due collegamenti (richiesta e
  risposta). I `call` sono confermati solo se la chiamata è nell'AST (`evidence: estratto|dedotto`);
- `basis`: intenzione · struttura estratta · dedotto · osservato · verificato;
- `refs`: `code` | `cpp` | `param` | `binding` | `test` | `file`; stato calcolato: estratto, mancante, non revisionato, modificato,
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
- Vista Interfaccia: albero e anteprima delle tre superfici e catalogo componenti; selezionando un controllo si
  vedono proprietà, definizione nel codice, gestori, comportamenti dell'Atlante, componente; «Proposta di modifica»
  mostra il valore proposto tratteggiato accanto allo stato attuale e «Copia proposte per l'agente» produce la
  richiesta tracciabile (fonte, valore attuale → proposto, vincoli).
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
- Detail Cinematografico: visibilità dinamica dei comandi, punti sul viewer e Misura scritti secondo lo standard
  OFX, non ancora provati in Resolve.
- Le note restano nel browser di chi le scrive; per discuterle si copia il contesto nella conversazione.

## 7. Prossimi incrementi

1. Fatto (01/11/2026): catalogo componenti e vista Interfaccia (script, Develop, Detail).
2. Modalità di sviluppo del Detail (P-003 nell'Atlante, in corso). Fatto (01/11/2026): menu Modalità, Ingresso
   (IDT), Destinazione (ODT), modalità Cinematografico (rapporto di luce misurato sul viso); modello, parità CPU e
   golden aggiornati, kernel CPU/Metal invariati. Da provare in Resolve: `setIsSecret` dal vivo, punti Double2D sul
   viewer, `fetchImage` durante Misura. Fatto anche: Zonale e vista zone (kernel CPU e Metal: `view` 3/4,
   `zoneWhite`/`zoneBlack` in `DetailParams`), Destinazione Scena DWG. Da fare: Stampa, Recupero HDR→SDR.
3. Comportamenti successivi: «Scrivi in Resolve», «Copia su clip senza metadata», lettura `--cache` dal plugin.
4. Estrazione C++ minima (funzioni e chiamate) per collegare i comportamenti del plugin.

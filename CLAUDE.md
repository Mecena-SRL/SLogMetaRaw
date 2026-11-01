# Istruzioni permanenti per gli agenti (S-Log MetaRaw)

Letto da Claude Code a ogni sessione in questo repository. L'orchestratore trasmette agli agenti le parti pertinenti.

## Repository pubblico
Non committare segreti, token, dati personali, email, percorsi locali (`/Users/...`, `/home/...`) o materiale privato.
Vale anche per le pagine generate (Atlante) e per i messaggi di commit e PR.

## Metodo di sviluppo
I configuratori sono strumenti di sviluppo esterni all'app (finestre accanto a conversazione e codice). Non vanno
integrati nell'interfaccia del programma salvo richiesta esplicita. Stato, fonti e schema: `docs/dev/CONFIGURATORI.md`.

Quattro viste collegate da identità stabili:
1. Design System — catalogo componenti estratto (vista Interfaccia › Catalogo); fondamenti e regole d'uso mancanti.
2. Interfaccia — vista Interfaccia dell'Atlante: finestra script, pannelli Develop e Detail dalle fonti.
3. Atlante della logica — `tools/atlas/` (primo incremento, vedi sotto).
4. Manutenzione (Graphify, indici, decisioni, skill) — `docs/GRAPHIFY.md` + vista Manutenzione dell'Atlante.

Regole:
- Ogni modifica al codice aggiorna o invalida esplicitamente le viste interessate nella stessa modifica.
- Distinguere sempre: intenzione, struttura estratta, relazione dedotta, comportamento osservato, verificato da test.
  Non presentare un modello scritto a mano come estratto automaticamente.
- Spostare un blocco nel diagramma cambia solo la vista; cambiare una responsabilità o un flusso è una proposta di
  modifica al programma: va registrata (`proposals` in `tools/atlas/model.json`), applicata con test, poi il modello
  va riestratto e revisionato.
- Prima di creare un componente UI cercare una soluzione esistente (catalogo: helper `define*` di `ParamDefs.h`,
  widget UIManager); un helper nuovo in `ParamDefs.h` compare da solo nel catalogo, va documentato nella stessa modifica.
- Controlli OFX: i nomi sono permanenti; un cambio al pannello aggiorna `tests/golden/params_*.txt` di proposito.
- Verificare sul prodotto reale: diagrammi e mock non sostituiscono build, controlli e test.
- JEV è facoltativo: non certifica correttezza né autorizza operazioni.

## Atlante della logica
```
python3 tools/atlas/atlas.py --run-tests   # genera build/atlas/index.html (aprire nel browser)
python3 tools/atlas/atlas.py --check       # riferimenti rotti → exit 1 (anche in tests/test_atlas.py)
python3 tools/atlas/atlas.py --accept ID   # dopo aver rivisto un comportamento il cui codice è cambiato
```
Se modifichi una funzione citata da `tools/atlas/model.json`: rileggi il comportamento, correggi testo/collegamenti se
il funzionamento è cambiato, poi `--accept`. Se la rinomini o la rimuovi, aggiorna il riferimento (il test fallisce).

## Inizio e fine sessione
- Inizio: `python3 tools/atlas/atlas.py --check` e vista Manutenzione (freschezza di `docs/GRAPHIFY.md` rispetto a
  versione e moduli, codice da rivedere, modifiche locali). Aggiornare prima di usarli per decidere.
- Fine: aggiornare `docs/GRAPHIFY.md` se cambia la struttura dei moduli, il modello dell'Atlante se cambia un
  comportamento modellato, `RELEASE_NOTES.md` per le modifiche visibili; registrare decisioni e lavoro aperto in
  `docs/dev/CONFIGURATORI.md`.

## Controlli prima di una PR
```
python3 tools/check_version.py
python3 -m unittest discover -s tests      # compila anche plugin e helper C++ con CMake
python3 tools/atlas/atlas.py --check
```

## PR e merge
Aperta una PR, procedere al merge dopo aver verificato diff, controlli richiesti (CI linux, macos, windows,
linux-packages) e compatibilità con `main`. Conflitti meccanici: risolverli e ripetere le verifiche. Conflitti
funzionali o intenzioni ambigue: segnalarli e sospendere quel merge. Niente force push, niente aggiramento delle
protezioni.

## Rapporto finale
Distinguere ciò che è implementato e verificato, ciò che resta incerto e ciò che è solo pianificato.

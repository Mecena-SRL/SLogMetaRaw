# Proposta #7 — Copiare i metadata Sony su una clip senza metadata propri

Stato: proposta di design, nessun codice. Da decidere: i punti in fondo.

## Come funziona oggi (verificato nel codice)
- Il plugin trova i metadata della clip da un **record JSON** in `~/Library/Application Support/SLogMetaRaw/cache/`, il cui
  nome è l'hash FNV-1a del percorso canonico del file (`plugin_cache.py`, `src/common/Files.cpp`).
- Il record vale solo per quel file: contiene `file_size` e `file_mtime_ns`, e il plugin scarta un record che non coincide
  (`ClipCache.cpp`, `recordMatchesFile`).
- Se manca il record, il plugin avvia il lettore Python solo per `.mp4`/`.mxf` (`isSonyContainer`): un ProRes esterno
  si ferma lì con "Formato non Sony".
- *Avanzate › Sblocca controlli senza metadata* sblocca i controlli ma non fornisce valori: Kelvin, tint ed EI di ripresa
  restano quelli di default.

## Idea
Il meccanismo non richiede modifiche al plugin: **scrivere un record per il percorso della clip di destinazione**, copiando
dalla clip Sony i campi che servono (`shot_temp`, `shot_tint`, `shot_ei`, `cam_space`, `cam_gamma`, dati di lente/otturatore
mostrati sola lettura) e mettendo `file_size`/`file_mtime_ns` della **destinazione**, più `copied_from` (percorso della
sorgente) e `supported = 1`. Il nodo la tratta come una clip normale e i pannelli mostrano i dati copiati.

Il sistema ha già tutti i pezzi: `plugin_cache.build_record` costruisce il record, `write_cache` lo scrive in modo atomico.

## Dove si fa
Nella finestra dello script (non nel nodo): il nodo non ha un modo di scegliere un'altra clip del Media Pool, lo script sì.
Flusso: selezioni nel Media Pool la clip Sony (sorgente) e la clip esterna (destinazione) → *Copia metadata Sony su clip
selezionata*.

## Le due domande dell'issue
1. **Come si indica la sorgente.** Proposta: selezione manuale di due clip (sorgente = clip Sony, destinazione = l'altra).
   Il Timecode è un aiuto, non la regola: un registratore esterno spesso ha TC diverso. Un abbinamento automatico per
   TC può venire dopo, come suggerimento da confermare.
2. **Una tantum o agganciata.** Proposta: **una tantum**, con `copied_from` salvato per poter rifare la copia con un
   pulsante *Aggiorna da sorgente*. Un aggancio vivo richiederebbe che il plugin ricontrolli la sorgente a ogni frame o
   clip, con costi e fallimenti (sorgente spostata) difficili da spiegare. Una tantum è prevedibile e reversibile.

## Rischi
- Il record viene scartato se la clip di destinazione cambia (`file_size`/`mtime`): ricopiare, nessun danno.
- Il valore ISO/EI di un ProRes esterno può non coincidere con quello della camera (guadagno diverso): il nodo usa l'EI per
  l'esposizione relativa. Va detto nell'interfaccia.
- Il data level del ProRes (Video/Full) può differire da quello Sony: la copia **non** deve portare `level_*` della
  sorgente, ma lasciare che sia l'utente a decidere (oggi quei campi dipendono dal file).
- Se il ProRes esterno è già in un altro spazio colore (non S-Log), copiare `cam_space`/`cam_gamma` darebbe un risultato
  sbagliato: l'interfaccia deve mostrare lo spazio copiato e permettere di scegliere.

## Passi
1. `plugin_cache.copy_record(source_clip, target_clip)`, con test (unit test su file temporanei).
2. Pulsante e testi nelle 5 lingue (`i18n.py`).
3. Una riga nel README e nella wiki (sezione *Special cases*).
Dopo la tua scelta su 1 e 2, è lavoro piccolo e testabile su Linux per il record; il pulsante va provato in Resolve.

## Aperto, a te
- Selezione manuale di due clip: va bene come primo passo?
- Una tantum con *Aggiorna da sorgente*: va bene?

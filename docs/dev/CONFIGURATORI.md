# Configuratori di sviluppo: inventario, fonti, schema

Strumenti dell'ambiente di sviluppo, esterni all'app. Il programma (script Resolve + plugin OFX) è l'oggetto osservato.
Prima sessione di allineamento: 01/11/2026.

## 1. Inventario delle capacità

| Capacità | Dove | Stato | Note |
|---|---|---|---|
| Mappa moduli (Graphify) | `docs/GRAPHIFY.md` | **funzionante, manuale** | Disegnata a mano. Freschezza (versione, moduli citati) controllata dall'Atlante. Nessuno strumento graphify/codegraph installato nel repo. |
| Estrazione strutturale Python | `tools/atlas/atlas.py` | **funzionante** | AST: funzioni, righe, impronte, chiamate, import, binding UI `win.On.X.Y`, test. |
| Estrazione C++ (plugin OFX) | `tools/atlas/atlas.py` (`cpp_symbol`) | **parziale** | Ricerca testuale delle definizioni (righe, impronta), non un parser: niente grafo delle chiamate C++. |
| Atlante dei comportamenti | `tools/atlas/` | **funzionante** | 3 comportamenti: Leggi metadata, Seleziona clip → Dettagli, Creative: dal processo ai pixel. Testo e collegamenti manuali, riferimenti estratti e controllati. |
| Verifica dei comportamenti | `tests/` (unittest) | **funzionante** | L'Atlante cita i test e con `--run-tests` ne registra l'esito. |
| Parametri plugin (golden) | `tests/golden/params_*.txt` | **funzionante** | Fonte verificata dei parametri OFX: base possibile per il catalogo dei controlli del plugin. |
| Design System / catalogo componenti | vista Interfaccia › Catalogo | **primo nucleo, estratto** | Helper `define*` di `ParamDefs.h` e tipi UIManager, con utilizzi. Mancano fondamenti grafici, varianti e regole d'uso scritte. |
| Configuratore UI / navigazione | vista Interfaccia | **funzionante, primo incremento** | Anteprima di finestra script, pannelli Develop, Creative e Noise dalle fonti; selezione, note, proposte di modifica (testo, tooltip, default) esportabili per l'agente. L'anteprima è una rappresentazione, non uno screenshot. Le proposte non modificano il codice da sole. |
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

Altre identità: widget `ui:script.<ID>`; controllo OFX `param:<develop|creative|noise>.<nome>` (nome permanente: Resolve
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
   Pannello Develop (01/11/2026, non rilasciato): riorganizzato sulle simulazioni dell'Inspector. Decisioni:
   gruppo Ripresa con false color sopra Temperatura e sotto Tint/Esposizione, **Punto bianco** come parametro colore
   col contagocce di Resolve (la formula `sm_wb_from_xyz` è provata sul render), **Spazio colore** al posto di
   Avanzate (Ingresso, Senza metadata, Uscita), Toni ridotti a ciò che Resolve non ha (gli altri compaiono se usati o
   con «Mostra tutti i controlli»), Dati di ripresa invariati. Da provare in Resolve: sottogruppi annidati,
   spazio del colore campionato dal contagocce, controlli nascosti che riappaiono, scritte corte, rombi keyframe.
   Persistenza: `settingsVersion` 6, identità della clip (nome + dimensione) per il rilink, test di compatibilità con
   `tests/golden/released/`. Aperto: icone sulle caselle non disegnate da Resolve (le tre `fc_*.png` restano assegnate),
   screenshot del wiki da rifare.
   Detail, tre modalità (02/11/2026): Zonale (Adams), Split grade (autonoma, con provino a strisce, matrice
   e confronto fra tre unioni delle due esposizioni) e Unito (Split grade + Auto zone + BTZS, una sola Misura). Standard e
   Cinematografico eliminati con codice, parametri e test, senza migrazione (settingsVersion 3: i nodi salvati diventano
   Zonale). Proposta e fonti: Claude Doc «Detail: Stampa split grade, BTZS e Auto zone». Da fare in PR successive: Split grade,
   Misura automatica e Unito, provino.
   Split grade, motore a curva (02/11/2026, non rilasciato): seconda modalità, con Fase propria (Esposizioni, Locale,
   Presenza), Guida per modalità e fase, Luci morbide e Ombre dure in stop sulla stessa curva dello Sviluppo e Grado
   letto dalla pendenza (2,5 + 2,5·log2 k: ipotesi da verificare su stampe vere). Misura e punti sul viewer sono
   condivisi con la Zonale.
   Provino e Due strati (02/11/2026, non rilasciato): Unione scelta fra Curva (pendenza dal rapporto) e Due strati (due
   rette di grado 5 e 0, la più bassa con raccordo di mezzo stop); Provino su vista (strisce, matrice, unioni) con cella
   attuale incorniciata e «Usa la cella». Celle e cornici in interi (parità CPU/modello); in `DetailParams` ora
   `SMZdCurve zd` e `pv[25]`. Da provare in Resolve; Metal non eseguito. 
   Unito (02/11/2026, non rilasciato): terza modalità. Misura automatica = 2° e 98° percentile della L (campioni a passo,
   `dt_auto_zone`, nel gruppo Misura per tutte le modalità); sviluppo come retta attorno all'ombra con pendenza in
   [0,5, 2] (`SM_DT_UNITO_K_*`) poi curva di stampa (Curva o Due strati) sul resto: due stadi in `dt_gain` (`zd1`, `zd`).
   Il provino non vale in Unito. Ipotesi non verificate: percentili, limiti di sviluppo, gradi; il tono medio (key) non
   è usato. Metal non eseguito; `dt_auto_zone` provato solo nel modello Python.
   Detail, Zonale in tre fasi (02/11/2026, non rilasciato): dopo la ricerca sul sistema zonale e una prova sul prodotto
   (troppi artefatti), il lavoro è diviso come in camera oscura, un nodo per fase in serie: Sviluppo (curva puntuale,
   ruota attorno all'ombra, piede e spalla), Stampa (toni per aree + bordi bruciati), Presenza. Decisioni: tre nodi,
   N± attorno all'ombra, sezione Guida in fondo al pannello (scopo, tre passi, poi), testo per fase. Presenza limitata alle zone II–VIII (sfumata) e Vista zone con legenda, tacche e tratteggio (02/11/2026; Metal non eseguito). Il ridisegno del pannello Detail (simulazione con Vista e Soggetto
   come scelte) è in pausa. Aperto: diagnosi degli artefatti su fotogrammi reali; Cinematografico e Standard usano
   ancora la curva sulla base (per aree).
3. Comportamenti successivi: «Scrivi in Resolve», «Copia su clip senza metadata», lettura `--cache` dal plugin.
4. Estrazione C++ minima (funzioni e chiamate) per collegare i comportamenti del plugin.

- Lettura multi-camera (15/11/2026, non rilasciato): nome visibile Log MetaRaw (id interni invariati), lettori
  `vendors.py` e `dji.py`, 14 curve e 9 gamut in coda alle tabelle (codici 11+, l'ordine è permanente). Decisioni: una
  lettura *stimata* senza marca né modello non guida il nodo (`camera_found` 0); D-Log M: curva di Freeman confermata dalla LUT DJI,
  gamut di Freeman tenuto per scelta (`tools/lut_gamut.py`, vedi `docs/CAMERAS.md`). Aperto: clip Action 4 e ColorChecker a ±1 stop, prova in Resolve dei nuovi menu, nomi Resolve dell'Input Color Space per V-Log, Apple Log, D-Log M.
- Uscita del Develop, tone mapping e gamut (15/11/2026, 3.1.1 non rilasciata): *Rendering display* diventa **Tone
  mapping** (stesso parametro `rendering`), con **Compressione gamut** (`gamutCompress`: Nessuna, ACES riferimento,
  ACES parametrica con sette cursori `gamut*` visibili solo in parametrica) e **Mappatura gamut** (`gamutMapping`:
  Nessuna, Compressione saturazione), sul modello del Color Space Transform. Decisioni: la coppia che ogni rendering
  aveva incorporata è migrata (`settingsVersion` 7) e proposta quando l'utente sceglie un rendering, i nodi nuovi
  partono senza; con un rendering l'uscita è forzata a una codifica di schermo (Rec.709 = Gamma 2.4). Aperto: provare
  in Resolve (Metal, visibilità dei cursori), confronto con OpenColorIO da rifare con PyOpenColorIO installato (in CI
  i test ACES contro OCIO sono saltati), nomi Resolve di *Input Color Space* per V-Log / Apple Log / D-Log M.
- False color dopo la prima prova su Mac (15/11/2026): verifica statica del percorso Metal (struttura parametri solo
  int/float, 336 byte uguali in C++ e Metal; nessun letterale double, nessuna divisione intera, puntatori `constant`
  con `SM_CPTR`/`SM_CREF`) senza difetti trovati; il kernel resta non eseguito. Corretto il clip della vista
  esposizione sulle curve corte (`sm_fc_headroom`), visto su un fotogramma reale di C0001 (Rec.709). Aperto: dettagli
  del problema segnalato in Resolve; toggle false color attivi ma senza effetto con *Camera metadata* o senza
  metadata (nodo trasparente); vista zone con i valori iniziali quasi tutta tinta (Ombre e Luci coprono ogni stop).
- 24/11/2026, ricerca DJI (`docs/DJI_LOG.md`, solo documentazione): aperti D-Log2/D-Gamut2 (in Resolve 21.1),
  D-Log M per famiglia di camere (misure Action 6 / 360 II diverse da Freeman), percorsi `djmd` per schema dei droni.

Maschera rumore (24/11/2026): gruppo nuovo del Develop, alpha con le zone rumorose per camera, ISO nativo, codec ed
EI scelto (`develop/NoiseMask.cpp`, `docs/NOISE.md`, entrambi rimossi in 4.0.0); RGB invariato, verificato dai test. Da provare in Resolve:
alpha del nodo come key del nodo Neat Video, Metal non eseguito. Aperto: tarare le costanti stimate (C70, Z8,
X-H2S, Komodo, Pocket, FX3/FX30, DJI) con una scala di esposizioni su un grigio.
Rimossa nella 4.0.0 su richiesta di Ivan (in Resolve l'immagine restava strana anche col nodo maschera della 3.2.1,
causa non diagnosticata): tabella, modello e fonti passano al plugin Noise. Resta `noise_codec` nel record della clip.
Plugin Noise (24/11/2026): nodo `noise/` su CPU e GPU Metal (`docs/DENOISE.md`, proposta iniziale in
`docs/proposals/NOISE_PLUGIN.md`), verificato su fotogrammi sintetici (`tests/test_noise.py`) e GPU contro CPU
(`tests/noise_metal_test.mm`, CI macOS). Spaziale con soglia sul vicinato 3×3 (NeighShrink). Provato e scartato:
secondo stadio Wiener su pilota (−5% di errore al doppio del costo). Aperto: prova in Resolve (accesso temporale su
GPU, cache, Salta ovunque, Misura in modalità Metal), taratura dei profili sui campioni di Ivan, CUDA, CPU più rapida.
Noise 4.0.2 (scelta di Ivan: «zona campione + automatico»): profilo automatico dai metadata, corretto per banda da un
riquadro tra due punti Double2D (Resolve non permette a un OFX di disegnare un riquadro trascinabile; il contorno lo
disegna il nodo con *Mostra campione*). Pannello: Profilo, campione, Forza e Vista in vista, il resto in Avanzate
chiuso. Aperto: provare in Resolve il trascinamento degli angoli e i tempi GPU reali.
Noise 4.0.3 (segnalazione di Ivan su FX6): con uno spazio colore di Resolve non in tabella il nodo restava neutro;
ora segue la regola di Develop (codifica camera dal record). Rileggi metadata in cima al pannello, come Develop.
Il pannello della segnalazione era quello della 4.0.0 (Temporale, nessun tasto): Ivan non aveva ancora installato
la 4.0.2. Aperto: il nome che Resolve dà sul suo progetto (la riga Noise lo mostra se resta neutro).
Guide 3.2.0: `packaging/guide/guida.html` e `guide-en.html` illustrate, 10 pagine, 11 dalla 4.2.0 con la pagina Audio (limite in
`packaging/build_installer.sh`), figure da `tools/make_guide_figures.py` sui modelli in `tests/`. Stampa su macOS non
verificata (font diverso da Linux).
Acquisizione profonda (24/11/2026, non rilasciata): spunta nella finestra e `--deep`; `slogmetaraw/clipdb.py` lancia
`mecena-clipdb` di Mecena Motion (repo Mecena-SRL/GyroMotion) e riporta la voce del MecenaClipDB. Scelta di Ivan:
un solo database, nessun parser IMU in Log MetaRaw (il lettore Sony provato su FX6/FX30 è stato tolto). Piano di
integrazione Stabilize/Motion Blur: `docs/dev/STABILIZE.md`. Fatto anche: sorgenti di GyroMotion in `motion/` (copia senza storia, scelta di Ivan), categoria «Log MetaRaw», pkg
macOS unico che compila Motion. Non provato su Mac. Aperto: DJI Action 4, Linux/Windows per Motion.
Creative (24/11/2026, rilasciato nella 4.1.0): scelta di Ivan, il Detail (Zonale, Split grade, Texture, Clarity, Dehaze, P-003)
è sostituito per intero dal nodo Creative nello stesso slot (P-004 nell'Atlante). Ricerche in `docs/creative/`, motore e
tabelle in `docs/CREATIVE.md`. Verificato: test CPU (`tests/test_creative.py`), host OFX, golden del pannello, controllo
sintattico del codice Metal con spazi d'indirizzo simulati. Aperto: prova in Resolve, parità Metal (solo CI macOS),
taratura dei numeri su riprese reali; rimandati grana anamorfica, DCT e smear CCD, infrarosso, Polaroid, lith.
Poi (stesso giorno, richiesta di Ivan): più pellicole, positivi e generazioni di copia con grana dei duplicati e della
stampa, Supporto di visione (sala, proiettore, tubo, monitor, console LCD, carta) come ultimo passo su CPU e Metal,
motore più veloce. Verificato come sopra; aperto: parità Metal del supporto (CI), aspetto su un 4K vero.
Terzo giro: nitidezza dei duplicati, bagliore di vetro e obiettivo, scia dell'LCD (rende anche il fotogramma prima),
righe del pannello nascoste quando non servono (crRowIdle).
Dopo la 4.1.0 (25/11/2026, richiesta di Ivan): ingressi Rec.709 (curva BT.709), ACEScg, ACES2065-1 e Cineon (codice gamma 25,
solo per il Creative: `kCreativeInputCount`, menu di Develop e Noise invariati); supporti con risposta del dispositivo
(`cr_device`) e comandi caricati dal supporto scelto (`creativeLoadMedium`, `mediumLoaded`). Verificato: test CPU e host,
sintassi Metal simulata. Aperto: numeri da tarare, prova in Resolve, se Ivan vuole lo stesso caricamento dei valori anche
per pellicole e processi (oggi i comandi del processo sono multipli: 1 = com'era).

Preset per dispositivo (25/11/2026, richiesta di Ivan: albero dispositivo → modello → formato → produttore → pellicola con
nomi reali, interruttori per usare solo la grana): `src/creative/CreativePresets.h` (tabelle), `creativeResolve`,
`creativeLoadDevice`, `deviceLoaded`, interruttori `useTone`/`useColour`/`useTexture`. Default *Processo a mano*: i grade
salvati non cambiano. Verificato: test CPU e host (518). Aperto: taratura degli stock, prova in Resolve, emulazione vera
del segnale VHS/Betamax (ricerca in `creative-ricerca/v2/VIDEO_EMULAZIONE.md`), digitali e telefoni, riversaggio,
ottiche complete.

Nastro emulato (25/11/2026, fase 2, richiesta di Ivan: «VHS e Beta si sente che è finto»): `cr_tape_row` (per riga,
seriale lungo x: enfasi, clip FM, rumore FM, de-enfasi, biquad, colore), `cr_tape_out` (timebase, comb, dropout, cambio
testina), `tapeEmulation` in `CreativeModel.cpp` con la tabella `kTapeEmu`; kernel Metal `cr_tape_row_k` (un thread per
riga) e `cr_tape_out_k`. Verificato: test CPU (asimmetria dei bordi, clip, grana per riga, ritardo del colore), sintassi
Metal simulata; parità Metal solo in CI macOS. Aperto: camera a tubo e CCD, campi interlacciati nel movimento, taratura.
Sensore della videocamera (25/11/2026, fase 2b): `cr_sensor` (tubo di Philips 1963 con stato dai 3 quadri prima,
CCD smear da `cr_smear_col`, MOS), falso colore in `cr_tape_row`; tabella `kSensors` in `CreativeModel.cpp`, controlli
`sensor`, `lag`, `comet`, `smear`; l'host passa i quadri prima (`creativeHistory`, `CreativeHistory`, kernel Metal
`cr_smear_k`, `cr_sensor_k`). Verificato: test CPU (scia nelle ombre, cometa, smear per colonna, falso colore), sintassi
Metal simulata; parità Metal solo in CI macOS. Aperto: campi interlacciati nel movimento, otturatore a righe del tubo,
taratura su riprese vere.
Digitali per modello (25/11/2026, fase 3): `cr_dig_*` in `math/Creative.h` (sensore, ISP, DCT 8x8 JPEG, uscita),
campi del modello in `CrDigitalCamera` (`CreativePresets.h`), kernel Metal `cr_dig_*_k`. Verificato: test CPU (blocchi
8x8 crescono con la compressione, il RAZR perde il dettaglio fine), sintassi Metal simulata; parità Metal solo in CI
macOS. Aperto: rolling shutter, AE/AWB, GOP; valori da tarare su clip vere.
Riversaggio (25/11/2026, fase 4): controllo `transfer`, `creativePrepareTransfer` in `CreativeModel.cpp` prepara
la copia come seconda passata (CPU `creativeRenderCPU`, Metal `RunCreativeKernels` con `p_Transfer`, buffer `xfer`);
il supporto di visione va sulla copia, niente ghost LCD con la copia. Verificato: test CPU (VHS ammorbidisce e aggiunge
rumore, kinescopio toglie il colore e aggiunge grana, Log ignora la copia), golden; parità Metal solo in CI macOS.
Aperto: taratura, generazioni multiple scelte a mano.
Ottiche (25/11/2026, fase 5): tabella `kCrLenses` (`CreativePresets.h`, 14 ottiche [gen] da `DIGITALI_OTTICHE.md`),
`creativeLoadLens` e `lensLoaded` (come per il supporto), `cr_lens_soft`, `cr_streak_row`, `cr_lens_add` in
`math/Creative.h`, kernel Metal `cr_lens_soft_k`, `cr_streak_k`, `cr_lens_add_k`. Verificato: test CPU (barilotto,
bordi e vortice, striscia, velo, caricamento e vetro), golden; parità Metal solo in CI macOS. Aperto: bokeh, ghost,
desqueeze, vortice a 16 campioni granuloso con raggi grandi (4K), taratura su riprese vere.
Supporti di visione (25/11/2026, dopo la fase 5): quattro righe in coda a `kMedia` (Super 8, Trinitron, plasma, primo
LCD; gli indici di prima restano) e campi `damper`, `plasma`, `levels`, `blackTint` in `cr_medium_at`. Verificato:
test CPU (fili, formicolio che cambia per fotogramma, nero bluastro e bande), golden; parità Metal solo in CI macOS.
Aperto: ABL del plasma, angolo di visione dell'LCD, pulldown 3:2, proiettore DLP.
Release 4.2.0 (25/11/2026): fasi 1–5 del Creative, supporti di visione e Log MetaRaw Audio in una sola release; guide
IT/EN con pagina Audio (11 pagine), README e wiki aggiornati. Aperto: prova in Resolve e Fairlight, taratura.
Release 4.3.0 (25/11/2026): Creative con pannello gerarchico, pellicole per formato, trascinamento, capello, console,
scansione con perforazioni, base e negativo; guide IT/EN, README e wiki aggiornati. Aperto: prova in Resolve.
Motion Blur, qualità (24/11/2026): su FX6 reale (C005C072, 50p 1/200 letto dall'RTMD con `shutter-time.patch`) il
motore CPU produce una scia coerente con la posa (passo 1 e 2 uguali, Metal = CPU in CI). Aggiunta la grana: il
residuo fine del centro, livello stimato nelle zone piatte (`blur::grainLevel`, correlazione tra pixel adiacenti per
escludere la texture), torna dove c'è mosso. Aperto: diagnostica «Verifica ultimo render» di Ivan in Resolve (subframe
e posa letta davvero), velocità su GPU reale.

Log MetaRaw Audio (25/11/2026, rilasciato nella 4.2.0): simulatore di registrazione e mix d'epoca per Fairlight. Decisioni di
Ivan: due plugin («Ripresa» per le tracce, «Mix e supporto» per bus e master) sullo stesso motore, preset con nomi
reali e interruttore per ogni stadio. Fatto: motore e tabelle in `audio/src/`, test; plugin VST3 con pannello (`audio/vst3/`),
CI e installer. Deciso: niente AU (Fairlight carica il VST3 anche su Mac). Menu coerenti (richiesta di Ivan, 25/11/2026):
registratore → microfono → supporto → riduzione, banco → master → riversaggio, filtrati per compatibilità e anno nel
pannello (`auAllowed`, `auCoerce` in `audio/vst3/AuParams.h`). Aperto: prova in Fairlight, taratura. Dettagli: `docs/audio/AUDIO.md`.

# Log MetaRaw Audio — simulatore di registrazione e mix d'epoca

Gemello sonoro del Creative: come si dice «girato in Super 8 su Kodachrome», si dice «tracce registrate col Geloso
G.257 a 4,75 cm/s col suo microfono piezo, mix su EMI REDD.51, stampato su 78 giri, ascoltato da una radio a valvole».

## Formato

OFX non elabora audio: in Resolve serve un plugin per Fairlight. Formato: **VST3** (macOS universale e Windows x64),
con l'SDK VST3 di Steinberg (licenza MIT dalla 3.8, ottobre 2025, scaricato dal CMake al tag `v3.8.1_build_84`) e
VSTGUI per il pannello, senza JUCE. Fairlight carica il VST3 anche su Mac. Per Logic Pro (e GarageBand, le altre app
Mac) ci sono le **Audio Unit v2** `Log MetaRaw Ripresa.component` e `Log MetaRaw Mix e supporto.component` (codici
`aufx LMrp Mcna`, `aufx LMmx Mcna`): il wrapper AU dell'SDK Steinberg attorno a un VST3 con un solo plugin (il wrapper
prende il primo effetto del modulo, da qui `AU_ONLY`), copiato in `Contents/Resources/plugin.vst3`; l'AudioUnitSDK di
Apple (Apache-2.0, tag 1.1.0, C++17) si compila dentro, così basta un generatore CMake qualunque (target
`LogMetaRawAU`). La CI le verifica con `auval -strict`; l'installer Mac le ha come componente a parte. Linux: Resolve non carica plugin VST nativi; lì il plugin si
compila solo per i controlli.

Un solo modulo, `Log MetaRaw Audio.vst3`, contiene i due plugin. Installazione: `/Library/Audio/Plug-Ins/VST3/` su Mac
(dal pkg), `%CommonProgramFiles%\VST3\` su Windows (setup o `install.ps1`). Negli installer è un componente a sé,
spuntato di serie: con «Personalizza» (Mac) o «Scegli i componenti» (Windows) si può lasciare fuori. Log MetaRaw
(script e nodi Color) è sempre installato; su Mac anche Stabilize e Motion Blur sono un componente facoltativo.

```
cmake -S audio/vst3 -B build/audio -DCMAKE_BUILD_TYPE=Release
cmake --build build/audio --config Release --target LogMetaRawAudio   # esegue anche il validatore Steinberg
```

Codice: `audio/vst3/AuParams.h` (parametri e righe del pannello come dati; id permanenti), `audio/vst3/AuPlugin.cpp`
(processore, controller, pannello disegnato dalle righe, factory). Lo stato salvato tiene il registratore come famiglia e
modello, così un modello aggiunto in coda a una famiglia non sposta quelli salvati. Ogni istanza ha il suo seme del
rumore (due tracce non condividono lo stesso soffio).

## Due plugin, un motore

| Plugin | Dove va | Catena |
|---|---|---|
| **Ripresa** | tracce microfono | Microfono → Preamplificatore → Supporto (con wow e flutter, rumori, riduzione rumore) |
| **Mix e supporto** | bus e master | Banco → Supporto → Riversaggi (copie) → Ascolto |

Il motore (`audio/src/AudioEngine.*`) non dipende dall'host: i wrapper spostano solo campioni e parametri. Ogni stadio
ha un interruttore (`AuSettings::on`); uno stadio spento o su «Nessuno» lascia passare i campioni invariati (verificato
bit per bit). Regolazioni comuni: Saturazione (×), Rumore (dB), Wow e flutter (×), Dry/wet, Uscita.

## Preset (`audio/src/AudioPresets.h`)

Tabelle come nel Creative: l'ordine è permanente (il progetto salva l'indice), si aggiunge in coda.

- **Dispositivo › Modello** (Ripresa): bobine portatili (Geloso G.257, Nagra III, Uher 4000 Report-L, Nagra 4.2),
  bobine da studio (Ampex 350, Studer J37, Revox A77, Studer A80), cassetta (Philips EL 3300, Tascam Portastudio 144,
  Walkman Pro WM-D6C, Sony TC-D5M), cinepresa e videocamera (Auricon Cine-Voice 16 mm, Kodak Ektasound 130,
  Canon 514XL-S, JVC GR-C1, Sony CCD-V8), prima del nastro (fonografo Edison, registratore a filo Webster-Chicago).
  Ogni modello porta microfono di serie, supporti compatibili, preamplificatore, ronzio di rete, riduzione rumore.
- **Microfono**: di serie, tromba acustica, carbone, piezo, RCA 44-BX e 77-DX, Shure 55, Neumann U 47, AKG C12,
  EV 635A, MD 421, SM58, MKH 416, electret.
- **Supporto**: nastro ¼" a 38/19/9,5/4,75 cm/s, cassetta Tipo I/II/IV, filo, cilindro, 78/45/33 giri, VHS lineare
  SP/LP e Hi-Fi, Video8, U-matic, banda ottica 35 e 16 mm, pista magnetica Super 8, radio AM/FM, telefono.
- **Riduzione rumore**: Dolby A/B/C, dbx (per ora come riduzione del soffio, non come compander).
- **Banco**: RCA BC-2B, EMI REDD.51, Neve 8078, SSL 4000 E, Mackie CR-1604.
- **Ascolto**: radio a valvole, radiolina a transistor, juke-box Wurlitzer 1015, TV CRT, sala (curva X).

## Frontali e comandi (`audio/vst3/AuFronts.h`, `AuLooks.h`, `AuMeters.h`)

Il pannello si apre sul frontale del modello scelto (in *Mix e supporto*, del banco): disegnato, senza marchi, con i
comandi che quella macchina aveva, dove li aveva, con le scritte e le posizioni dei manuali, e il suo indicatore di
livello con la sua scala e il suo movimento. Sotto il frontale restano i menu e le regolazioni del plugin
(*Saturazione*, *Dry/wet*, *Uscita*, *Taratura meter*), uguali per tutti.

Ogni comando è **attivo** (muove un parametro), **meccanico** (sulla macchina vera non tocca il suono: avvolgimento,
formato bobina, equalizzazione di riproduzione, pitch; si muove e basta) o **non collegato** (un canale o un ingresso
che il plugin non alimenta, disegnato spento). Cosa fanno quelli attivi:

| Comando sul frontale | Nel plugin |
|---|---|
| livello di registrazione, guadagno, trim, fader di canale | *Saturazione* (quanto si spinge nastro e preamplificatore) |
| volume, livello di riproduzione, uscita linea, fader master | *Uscita* |
| velocità (cm/s), tipo di cassetta | il supporto (solo quelli che la macchina accetta) |
| Dolby, dbx | la riduzione rumore e il suo interruttore |
| selettore di funzione (STOP, TEST, RECORD, PLAYBACK…), tasti di trasporto | STOP = bypass; TEST, INPUT, DIRECT = si ascolta senza nastro; RECORD = il meter legge l'ingresso, PLAYBACK il nastro |
| limiter, AUTO, livello automatico | limitatore (Nagra 4.2, TC-D5M, SSL) e controllo automatico (Nagra, Canon; fisso sulle videocamere) |
| filtro (Nagra 4.2 LFA/HP, passa-alto Neve e SSL) | passa-alto del secondo ordine; sul Nagra la frequenza dà l'attenuazione a 50 Hz scritta sul selettore (HP 2 = −20 dB) |
| tono, acuti/bassi, EQ dei banchi | mensole a 100 Hz e 10 kHz |
| selettore del meter (LEVEL, BATT, BIAS…), pulsante di riferimento | cosa legge l'indicatore: ingresso, nastro, batteria, una taratura fissa; il riferimento del Nagra III legge 0 dB, quello del 4.2 −8 dB |

Un comando segue anche i parametri: se l'host o un menu cambiano il supporto, il selettore di velocità si sposta.

### Indicatori e balistica (`AuMeters.h`, calcolata nel processore, campione per campione)

| Indicatore | Scala | Movimento | Fonte | Dove |
|---|---|---|---|---|
| VU | −20…+3, 0 VU al 71 %; sul RCA la scala B (per cento in alto) | media; 99 % in 300 ms, sovraelongazione 1,5 % (ζ 0,80, ωn 13,1 rad/s) | ANSI C16.5, IEC 60268-17 [norma] | Ampex 350, Studer J37 (4) e A80, Revox A77, Portastudio 144 (4), TC-D5M, Auricon, RCA BC-2B, EMI REDD.51 (4), Neve, SSL |
| PPM | tacche 1–7, 4 dB l'una, la 4 al riferimento | picco; 10 ms di 5 kHz leggono −2 dB; scende 24 dB in 2,8 s | IEC 60268-10 IIa [norma] | (disponibile, nessun modello lo usa) |
| Modulometro | dB −30…+5, segmento nero di sovramodulazione 0…+2, scala batteria | picco quasi-istantaneo (10 ms −1 dB), scende 20 dB in 1,5 s | scala dai manuali Nagra; tempi [dedotti] come IEC tipo I | Nagra III, Nagra 4.2 |
| Ago REC | senza numeri, arco nero e rosso, zona batteria | media, circa 100 ms | [dedotto] | Uher 4000, Philips EL 3300, Canon 514XL-S |
| Occhio magico | ventaglio verde | media: carica 20 ms, scarica 250 ms | Sams, *Tape recorders, how they work*; tempi [dedotti] | Geloso G.257 |
| Lampada al neon | si accende sopra lo 0 | picco, rapida | bollettino Webster 79/80; Sams | Webster-Chicago 180 |
| LED | 5 lampade (−20…+4) sul Walkman Pro, 12 per canale (−30…CLIP) sul Mackie | picco 1 ms, scende 20 dB in 1,5 s, picco tenuto 0,8 s | manuale WM-D6C (5 LED picco/batteria); dB [dedotti] | WM-D6C, CR-1604 |
| Livello automatico | una spia | media | [dedotto] | Ektasound 130, JVC GR-C1, CCD-V8 |
| Nessuno | — | — | — | fonografo Edison |

Tutti gli indicatori leggono una sinusoide stabile al suo valore efficace (i picchi sono tarati su un tono a 1 kHz,
come si faceva): 0 VU, la tacca 4 del PPM e lo 0 del modulometro cadono sullo stesso livello, *Taratura meter*
(di serie −18 dBFS). Il processore manda al pannello quattro letture in sola lettura (uscita e ingresso, sinistra e
destra), non salvate.

### Fonti dei frontali

- Dai manuali e dalle schede lette: Nagra III (manuale 1963: selettore di funzione a sei posizioni, velocità ed
  equalizzazione a moneta, pulsante di riferimento), Nagra 4.2 (manuale 1991: funzione, filtro LFA/HP, modulometro a
  undici posizioni, AUTO 1/2, TAPE/DIRECT, oscillatore a −8 dB), Ampex 350 (manuale del 351, 1959: METER AND OUTPUT,
  INPUT TRANSFER, EQUALIZATION, velocità e bobine), Revox A77 (guida di servizio: ingressi MIC HI/LO/AUX, IEC/NAB,
  MONO/STEREO), Uher 4000 Report (selettore a quattro velocità con spento, manopole a trazione batteria/luce), Sony
  WM-D6C (istruzioni: 5 LED picco/batteria, DOLBY NR B/C, TAPE, MIC ATT, SPEED TUNE), Portastudio 144 (manuale:
  MIC/LINE-TAPE, TRIM, EQ, PAN, fader, PITCH, BUSS, CUE, quattro VU), RCA BC-2B (IB-24748: otto attenuatori con chiavi
  AUD/PGM, MASTER, un VU da 4" scala B), EMI REDD.51 (quattordici fader a quadrante, EQ Pop/Classic, quattro VU),
  Webster-Chicago 79/80 (bollettino: RECORD-LISTEN, MICROPHONE VOLUME, TONE, RUN-REWIND, OUTPUT, neon), Geloso G.257
  (schede museali: tasti colorati, manopola rossa, interruttore a rotella; 4,75 cm/s).
- [dedotto] da fotografie e dalla tipologia, senza manuale letto: Studer J37 e A80, Philips EL 3300, Sony TC-D5M,
  Auricon, Ektasound, Canon 514XL-S, JVC GR-C1, CCD-V8, fonografo Edison, Neve 8078, SSL 4000 E, Mackie CR-1604. Le
  scritte di questi vanno confrontate con le macchine vere.
- Cambiati di conseguenza: il Geloso registra solo a 4,75 cm/s; l'Uher ha anche 2,4 cm/s (nuovo supporto); il Nagra
  4.2 anche 9,5 cm/s; il Webster ha la lampada al neon e non l'occhio magico; il REDD.51 quattro VU e non PPM.

## Livello di registrazione, automazione e guide (`AuParams.h`, `AuGuide.h`)

- **Livello di registrazione** (*Ripresa*: dopo il preamplificatore; *Mix e supporto*: «Livello sul master», prima del
  banco): guadagno verso il supporto, restituito in riproduzione. Il volume resta, cambia quanto il supporto è spinto
  contro il suo fruscio: basso = più fruscio, alto = più saturazione. Le manopole di registrazione dei frontali (RECORD
  LEVEL, AUSSTEUERUNG, MIC GAIN, TRIM, fader del REDD…) muovono questo parametro; *Saturazione* resta il carattere.
  Il meter in registrazione o TEST legge ingresso + livello, come sulla macchina.
- **Automazione**: ogni comando che agisce sul suono è un parametro automatizzabile; girarlo nel frontale scrive
  l'automazione (gesto completo). Livelli, uscita e dry/wet scorrono in 10 ms, senza gradini. I comandi solo
  meccanici (Mech) non hanno effetto sul suono e non si automatizzano. Cambiare macchina o supporto durante la
  riproduzione riparte quei blocchi (piccolo scatto possibile).
- **«?»** sulla targhetta: al passaggio del puntatore, la storia della macchina o del banco (`kAuDeviceStories`,
  `kAuDeskStories`, una per riga delle tabelle; le static_assert lo controllano).
- **Cosa registri / Cosa missi**: la sorgente (voce, strumenti, batteria, ambiente, concerto) o il programma (parlato,
  generi per epoca, classica, film). La guida dice dove portare le punte sul meter della macchina, misura le punte
  degli ultimi 8 secondi e propone il livello; consiglia microfono (il primo adatto a quell'epoca e registratore),
  filtro, limitatore, macchine e banchi d'uso. *Applica* imposta livello, microfono, filtro e limitatore.
  Obiettivi di picco [prassi]: VU e indicatori a media circa 0 (parlato −3, percussioni −5, il VU le sottostima);
  PPM 5 parlato e PPM 6 musica (prassi BBC); modulometro Nagra 0 dB (parlato −2); LED +3; neon appena acceso.
  Consigli di microfono e macchine: scelte d'uso comuni, non regole [dedotto].

## Il banco e i canali (fase 2)

Ogni traccia con *Ripresa* è un canale del banco, come in una presa diretta vera: microfono → **ingresso del canale**
(stereo, mono somma, mono dal sinistro o dal destro) → **banco (canale)** (preamplificatore ed equalizzazione del banco
scelto colorano la traccia prima del registratore; la compressione di bus resta alla sezione master) → registratore →
**panpot** (mono a potenza costante, 0 dB al centro e +3 dB ai lati; stereo come bilanciamento) → **fader del canale**.
*Mix e supporto* sul bus è la sezione master: ingresso (stereo o mono), banco, livello sul master, supporto,
riversaggio, ascolto. Una band: una traccia per strumento, ciascuna con il suo microfono e registratore, lo stesso
banco su tutte le tracce e sul bus. Ogni istanza è indipendente: il pannello di *Mix* non vede le tracce. Di serie un
canale di *Ripresa* non ha banco, quindi i progetti salvati suonano come prima.

## Setup d'epoca (`shared/EraSetups.h`, `auSetupValues`)

Primo menu di entrambi i plugin, la stessa lista del Creative: scegliere un film o un'epoca carica la sua catena. In
*Ripresa* registratore, microfono e supporto (quelli del registratore se la riga non li dice), solo se la riga nomina
un registratore; in *Mix e supporto* banco, supporto del master, riversaggio (la banda ottica della copia) e ascolto.
Poi le regole di coerenza (`auCoerce`) e tutto resta modificabile; il menu non agisce sul suono. Fonti e scelte
dedotte: [`../SETUPS.md`](../SETUPS.md).

## Cosa va con cosa (`audio/vst3/AuParams.h`)

Come nel Creative, ogni menu del pannello offre solo ciò che i menu sopra permettono; cambiare un menu riporta i figli
incompatibili alla prima scelta valida.

- **Ripresa**: registratore (diviso per famiglia) → microfono → supporto → riduzione rumore. La prima voce di microfono,
  supporto e riduzione è quella del registratore e dice cos'è («Del registratore · Nastro ¼" · 19 cm/s»); seguono le
  alternative. Supporti: solo quelli che il registratore accetta (un Nagra non registra su VHS). Microfoni: usciti entro
  15 anni dal registratore; la tromba acustica solo sul fonografo, che non ne ha altre.
- **Mix e supporto**: banco → supporto del master → riduzione rumore → riversaggio. Il master deve essere in uso negli
  anni del banco (fino a 25 dopo); il riversaggio va su supporti ancora in uso quando nasce il master; *Copie* si
  attiva solo con un riversaggio.
- **Riduzione rumore**: Dolby A sulle bobine, Dolby B e C sulle cassette, dbx su entrambe, e non prima dell'anno del
  registratore o del banco (più la loro vita).
- Gli anni sono campi delle tabelle (`year`, `from`/`to`). Gli id e i valori salvati non cambiano: un progetto con una
  combinazione oggi esclusa si apre com'era e il menu la mostra in fondo.

**Stato dei valori:** stime parametriche da specifiche pubblicate e descrizioni d'epoca [gen], non misure dei prodotti.
Rumori in dBFS RMS dopo la banda del supporto; wow e flutter come errore di velocità di picco in %.

## Stato

- Implementato e verificato da test (`tests/audio_test.cpp`, `tests/test_audio.py`): motore, tabelle, bypass esatto,
  determinismo indipendente dalla dimensione del blocco, banda del telefono, risonanza piezo, soffio per velocità e
  Dolby, wow misurato sugli attraversamenti dello zero, supporti mono, perdita di acuti per copia, dry/wet e uscita.
- Implementato e verificato: plugin VST3 (validatore Steinberg 94/94 su Linux), pannello aperto e fotografato nell'host
  VST3 di prova dell'SDK (editorhost), job CI `audio` su Linux, macOS e Windows, pkg Mac e setup Windows.
- Incerto: comportamento in Fairlight (mai caricato in Resolve) e in Logic Pro (verificato solo da `auval`), build Mac e Windows finché la CI non passa.
- Implementato e verificato (test in `tests/audio_test.cpp`, pannello fotografato in editorhost): menu filtrati e
  coerenti come sopra. L'host vede ancora le liste complete (automazione), il filtro è nel pannello.
- Implementato e verificato su Linux (validatore 94/94, editorhost): frontali, comandi e indicatori per ogni
  registratore e banco; un selettore provato col mouse (Nagra 4.2: TEST spegne il supporto e sposta TAPE/DIRECT);
  balistiche VU e PPM verificate da test contro le norme.
  Il movimento delle lancette è stato visto con un segnale finto nel pannello: l'host di prova dell'SDK non fa passare
  audio, quindi il meter con audio vero va visto in Fairlight.
- Implementato e verificato da test e in editorhost (26/11/2026): livello di registrazione (volume costante, fruscio
  e saturazione che cambiano), levigatura di livelli/uscita/dry-wet, «?» con la storia, guida «Cosa registri».
  La misura delle punte con audio vero va provata in Fairlight (l'host di prova non fa passare audio).
- Implementato e verificato da test (26/11/2026): canale del banco in *Ripresa* (ingresso mono/stereo, banco prima del
  registratore, panpot, fader), ingresso mono/stereo del master.
- Limiti noti: cambiare un menu durante la riproduzione può dare un piccolo scatto.
- Pianificato: prova in Fairlight e taratura su registrazioni vere (fase 3), ricerca per modello come `docs/creative/`
  (fase 4). Dolby come compander,
  isteresi del nastro, distorsione di tracciamento del vinile, sibilanti della banda ottica.

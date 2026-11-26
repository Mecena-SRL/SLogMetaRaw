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

## Frontale e meter (`audio/vst3/AuPlugin.cpp`)

Il pannello si apre sul frontale del modello scelto (in *Mix e supporto*, del banco), disegnato e non fotografato,
senza marchi: colori e materiali della macchina, targhetta con il nome, quattro manopole (*Saturazione*, *Dry/wet*,
*Uscita*, *Taratura*) e l'indicatore di livello che quella macchina aveva, con la sua scala e la sua balistica. I dati
stanno in `audio/vst3/AuLooks.h`, una riga per ogni registratore e banco, nello stesso ordine delle tabelle [gen]:

| Indicatore | Scala e movimento | Dove |
|---|---|---|
| VU | −20…+3, lineare in tensione (0 VU al 71 %), media, circa 300 ms | Ampex 350 (uno), Studer J37 e A80, Revox A77, Portastudio 144, TC-D5M, Auricon (uno), RCA BC-2B (uno), Neve, SSL |
| PPM | tacche 1–7 a 4 dB, la 4 al riferimento; sale in 10 ms, scende 24 dB in 2,8 s; quadrante nero | EMI REDD.51 |
| Modulometro | −30…+5 dB, picco, discesa più lenta | Nagra III, Nagra 4.2 |
| Indicatore REC | ago senza numeri, arco nero e rosso | Uher 4000, Philips EL 3300, Canon 514XL-S |
| Occhio magico | ventaglio verde che si chiude col livello | Geloso G.257, Webster-Chicago 180 |
| LED | scala −40…+6 dB, verde, giallo, rosso; picco tenuto | Walkman Pro WM-D6C, Mackie CR-1604 |
| Livello automatico | nessun meter, una spia che si accende col suono | Ektasound 130, JVC GR-C1, CCD-V8 |
| Nessuno | — | fonografo Edison |

- *Taratura* è il livello digitale che legge 0 VU (la tacca 4 del PPM, lo 0 del modulometro), da −24 a −6 dBFS, di
  serie −18 dBFS; la lampada rossa si accende oltre il fondo scala.
- Il livello lo misura il processore all'uscita, a ogni blocco (media quadratica per VU e aghi semplici, picco per gli
  altri), e lo
  manda al pannello con due parametri in sola lettura (`Meter sinistro`/`destro`, non salvati, non automatizzabili).

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
- Implementato e verificato su Linux (validatore 94/94, editorhost): frontali e indicatori per ogni registratore e banco, manopole.
  Il movimento delle lancette è stato visto con un segnale finto nel pannello: l'host di prova dell'SDK non fa passare
  audio, quindi il meter con audio vero va visto in Fairlight.
- Limiti noti: cambiare un menu durante la riproduzione può dare un piccolo scatto.
- Pianificato: prova in Fairlight e taratura su registrazioni vere (fase 3), ricerca per modello come `docs/creative/`
  (fase 4). Dolby come compander,
  isteresi del nastro, distorsione di tracciamento del vinile, sibilanti della banda ottica.

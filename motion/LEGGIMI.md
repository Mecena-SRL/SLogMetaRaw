# Mécena Motion 0.11.0

Da novembre 2026 i sorgenti stanno qui, in `motion/` del repository Log MetaRaw (copiati da Mecena-SRL/GyroMotion, senza storia), e l’installer di Log MetaRaw per macOS li compila e installa insieme (`packaging/build_installer.sh`). Gli effetti stanno nella categoria **Log MetaRaw**. Le istruzioni qui sotto valgono anche per la build separata.

Due effetti OpenFX per DaVinci Resolve su Mac. **Stabilize** stabilizza tramite Gyroflow integrato. **Motion Blur** ricostruisce il mosso dall’immagine che riceve. Puoi usare Motion Blur da solo oppure dopo la stabilizzazione.

## Installazione sul Mac

1. Estrai l’archivio sorgenti e apri la cartella **Motion0100**. Da Terminale puoi scrivere `bash `, trascinare **release/build-mac.sh** nella finestra e premere Invio. Servono Rust, gli strumenti da riga di comando di Xcode e Internet solo sulla macchina che compila. La build scarica le dipendenze bloccate e incorpora il catalogo degli obiettivi.
2. In **Motion0100/dist** trovi **MecenaMotion-0.11.0.pkg**. Chiudi Resolve, apri il pkg e segui l’installer: copia **MecenaMotion.ofx.bundle** in **/Library/OFX/Plugins/** e sostituisce la versione precedente. Riavvia Resolve. Gli effetti compaiono in Effects → OpenFX → **Log MetaRaw**, accanto a Log MetaRaw e Detail, ciascuno con la propria icona.
3. Per rimuoverli apri **Disinstalla Mecena Motion.command** nella stessa cartella. In alternativa resta **MecenaMotion-0.11.0-arm64.zip** con il solo bundle da copiare a mano.

Senza firma Apple, al primo avvio del pkg macOS può bloccarlo: clic destro → Apri, oppure Impostazioni di Sistema → Privacy e sicurezza → Apri comunque. Per distribuire un pkg firmato e notarizzato imposta prima della build `MECENA_APP_SIGN` (certificato “Developer ID Application”), `MECENA_INSTALLER_SIGN` (“Developer ID Installer”) e `MECENA_NOTARY_PROFILE` (profilo creato con `xcrun notarytool store-credentials`). Le icone (PNG con il nome dell'identificativo, in **assets/icons**) vengono da `tools/make_icons.swift` del repository; `packaging/build_installer.sh` le copia prima della build.

## Il tuo caso da 50p a 25p

Sulla clip seleziona **Motion Blur → Look · FPS e shutter → Cinema · shutter sulla timeline** e lascia **Shutter desiderato (°)** a **180**. È la scelta predefinita sulle nuove istanze. Per aggiornare un effetto già in timeline selezionala esplicitamente. Il controllo in gradi accetta keyframe. In questo modo gli FPS e lo shutter inseriti nei vecchi preset di ripresa non intervengono nel calcolo.

A velocità reale, 50 fps e 90° corrispondono a una posa di 5 ms. Su una timeline a 25 fps ogni fotogramma dura 40 ms. Il look a 180° ricostruisce una posa di 20 ms, centrata sul fotogramma di timeline (da −10 a +10 ms). Con gli stessi 25 fps, 90° richiedono 10 ms e 360° ne richiedono 40. Lo shutter a zero riproduce il mosso già registrato.

Il filtro confronta il fotogramma centrale con il precedente e il successivo. Su una clip 50p in un flusso OFX 25p prova anche le due posizioni a mezzo frame e le usa se contengono informazione distinta dagli estremi. Se Resolve consegna soltanto 1, 3, 5, per l’uscita centrata sul 3 il filtro misura il moto verso 1 e 5 e integra la luce lungo la traiettoria di ogni pixel per tutta la durata dello shutter scelto. Il percorso include stabilizzazione, ritiming e trasformazioni già presenti all’ingresso dell’effetto.

**Motore del mosso.** Il flusso ottico è denso: piramide fino a 8 livelli, una stima ogni 4 pixel in Alta qualità (8 in Fluida) con propagazione dal livello più grosso, dal moto di camera e da zero, raffinata con Lucas-Kanade su patch normalizzate (robuste al flicker). Ogni pixel sceglie poi il vettore che spiega meglio la propria immagine in entrambi i vicini, così il confine del moto segue il bordo reale di mani, dita, volto o corde. Dove un lato è coperto (lo sfondo che sparisce dietro una mano) il motore usa il lato visibile e il moto della camera. Il mosso del soggetto esce dalla sua sagoma: una mano veloce lascia la scia sullo sfondo, come in una vera esposizione. È in primo piano la superficie che si discosta dal moto della camera, e viene accettata solo se è visibile anche nel fotogramma vicino. La posa già impressa viene sottratta in modo esatto: a 50p 90° (5 ms) verso 25p 180° (20 ms) si aggiungono 15 ms di traiettoria, non 20. Lo stesso codice gira su CPU e Metal (`src/blur_core.inc`).

La posa già impressa viene cercata nei metadati compatibili della ripresa e nei bordi in movimento dell’immagine ricevuta. Se mancano entrambe il motore aggiunge lo shutter completo. Una posa originale più lunga del risultato desiderato limita quanto si può ottenere. Durante un fermo, un taglio o un cambio di direzione rapidissimo l’informazione disponibile può essere parziale. Alle estremità della clip lavora con il solo vicino presente.

## Controllare un fotogramma

- **Verifica ultimo render** aggiorna **Fotogrammi utilizzati**. “Interi OFX · passo 40.00 ms” indica che i vicini usati distano una posa di timeline a 25 fps dal centro. “Intermedi OFX · passo 20.00 ms” indica che l’host ha fornito immagini a metà intervallo. “Guida” indica un contesto temporale più largo accettato per il movimento di camera.
- **Risultato verifica** riporta la posa obiettivo, la provenienza di quella iniziale quando disponibile e **ΔRGB P90**. Il valore è il novantesimo percentile della massima variazione RGB per punto su una griglia del render. Un movimento molto piccolo nello spazio può sfuggire a quella griglia. Con **Vista → Differenza RGB ×8** puoi osservare direttamente ogni pixel modificato. Riporta **Vista** su **Risultato** per l’export.
- Per un confronto con la versione precedente su una clip già elaborata, rigenera la cache del tratto e confronta lo stesso fotogramma al 100% nel viewer. Un frame servito dalla cache può precedere sia il cambio di versione sia l’ultimo aggiornamento del rapporto.

## Colore, prestazioni e retiming

**Valori manuali → Segnale in ingresso** deve corrispondere alla curva presente in quel punto della catena. Per un nodo che riceve DWG/DaVinci Intermediate seleziona **DaVinci Intermediate**. Se entra direttamente il girato Sony seleziona **Sony S-Log3**. Su immagini già trasformate in luce lineare seleziona **Lineare (scene-linear)**. Per altre curve log puoi usare i Color Space Transform di Resolve attorno all’effetto. Motion Blur calcola la media della luce e la riconverte nella curva scelta. La grana del sensore non segue il moto: il nodo misura il rumore nelle zone piatte del fotogramma e, dove aggiunge mosso, restituisce solo quella grana fine (bordi e dettagli restano mossi). Il tag della camera da solo non identifica una trasformazione già applicata.

**Anteprima → Fluida** usa una griglia di 8 pixel e fino a 24 campioni quando l’host segnala un render draft. **Alta qualità** usa sempre la griglia di 4 pixel e fino a 256 campioni, circa tre per ogni pixel di traiettoria. In assenza del flag draft il filtro usa Alta qualità anche in playback. Per un master, se la cache contiene immagini draft, imposta Alta qualità e usa un render fresco.

Su macOS il codice Metal incorporato (lo stesso sorgente del percorso CPU) costruisce le piramidi, calcola il flusso e integra il mosso. L’analisi del moto globale e della posa precede questo passaggio sulla CPU. L’OFX riceve immagini in memoria dell’host. Il motore riusa le pipeline e i buffer Metal e può ripiegare sulla CPU per immagini piccole o quando Metal è indisponibile. La build confronta Metal e CPU sullo stesso fotogramma e scrive **dist/Motion Blur-Metal.txt** con i tempi sintetici Full HD e UHD.

**Compensa lo shutter nella speed ramp** permette di animare **Velocità clip (%)** quando si conosce la curva del retime. Motion Blur misura il moto che entra nell’effetto. Per lavorare dopo la rampa si può applicare Motion Blur all’esterno di una compound che contiene la clip ritimata. La compensazione manuale adatta il tempo di posa letto dalla camera alla velocità locale. Nella modalità Cinema il centro rimane quello richiesto da Resolve.

Per una cadenza creativa diversa da quella di timeline, scegli **Look · FPS e shutter → Manuale** e anima **FPS look** e **Shutter desiderato**. I precedenti preset per ripresa e consegna mantengono i loro indici nei progetti salvati. I preset con cadenza inferiore possono trattenere la posa per più fotogrammi di timeline.

## Stabilizzazione

**MASTER (%)** è il controllo da animare in Edit o Color. A 100 applica la correzione impostata, a 0 azzera la rotazione di stabilizzazione e lo zoom adattivo. Valori intermedi interpolano la rotazione per riga e il campo visivo. La correzione ottica del profilo e l’orientamento del file appartengono alla geometria di base.

**Stile** sceglie il comportamento. Ogni nuova istanza parte da **Cinematografico**. Gli stili usano il motore **Percorso ottimale**, che calcola la traiettoria su tutta la clip minimizzando gli strappi del movimento e resta dentro il **Crop massimo**: dove il crop non basta, riduce la fluidità solo in quel tratto.

| Stile | Cosa fa | Valori principali |
| --- | --- | --- |
| Micromosso · solo tremolio | Toglie vibrazioni e tremolio della mano, lascia il movimento dell’operatore | τ ≈ 0,06 s su tutti gli assi, crop 108% |
| Controllato · limita il movimento | Smorza i movimenti bruschi e ampi, con una leggera tendenza a restare fermo | τ ≈ 0,6 s, inerzia 0,5, crop 120% |
| Cinematografico · attenua i passi | Toglie il rimbalzo dei passi su tilt e rollio, lascia libere le panoramiche | tilt/rollio τ ≈ 0,9 s, pan τ ≈ 0,3 s, crop 125% |
| Crane / gimbal · fluido su tutti gli assi | Movimento lento e morbido, partenze e arresti dolci, orizzonte in piano, crop fisso senza respiro | tilt/rollio τ ≈ 1,7 s, pan τ ≈ 0,6 s, orizzonte 100%, zoom statico, crop 135% |
| Personalizzato | Usa le regolazioni avanzate così come sono | — |

τ è la costante di tempo: il filtro lascia passare i movimenti più lenti di circa 1/(2π·τ) Hz. Dopo aver scelto uno stile puoi rifinire **Fluidità** (accetta keyframe), **Crop massimo** e **Blocco orizzonte** senza perdere lo stile. Modificando una regolazione avanzata lo stile diventa **Personalizzato**. Le istanze salvate prima di questa versione si aprono come **Personalizzato** con il motore **Adattivo** e conservano il comportamento precedente.

**Regolazioni avanzate**:
- **Motore**: Percorso ottimale (più fluido, segue le panoramiche senza ritardo costante) oppure Adattivo (il filtro classico di Gyroflow).
- **Bilanciamento tilt / pan / rollio (%)**: la fluidità di ciascun asse in percentuale della Fluidità. Il rimbalzo dei passi arriva soprattutto sul tilt.
- **Tempo di fluidità massimo (s)**: la costante di tempo a Fluidità 100.
- **Reattività nei movimenti rapidi (s)**: solo Adattivo, evita scatti all’inizio delle panoramiche.
- **Inerzia** e **Tendenza a restare fermo**: solo Percorso ottimale. La prima addolcisce partenze e arresti, la seconda attenua anche i movimenti lenti intenzionali.
- **Resta nel crop massimo**: solo Percorso ottimale.
- **Zoom adattivo** Dinamico o Statico e **Finestra zoom dinamico (s)**: lo statico usa un solo crop per tutta la clip ed elimina il respiro dell’inquadratura.

Limiti: il giroscopio misura solo le rotazioni. La stabilizzazione corregge tilt, pan e rollio; lo spostamento fisico della camera (altezza, passi, avanzamento) resta nell’immagine come traslazione e parallasse. Il rimbalzo dei passi si vede soprattutto come oscillazione del tilt e viene attenuato da lì. Per eliminare anche la traslazione serve una stabilizzazione ottica sull’immagine.

**Crop massimo (zoom %)** imposta il limite richiesto al calcolo adattivo di Gyroflow. A 110% la scala è 1,10× e resta circa il 91% della larghezza. Il limite riguarda lo zoom adattivo e il risultato dipende anche dalla correzione ottica. **Blocco orizzonte (%)** dosa il livellamento. Nel rapporto **Dati rilevati** trovi sorgente telemetrica, dimensioni, fps, conteggi IMU e ottica, prima distanza di fuoco valida e tempo di lettura del sensore quando disponibili. La prima distanza è informativa, non descrive la profondità della scena.

## Lettura automatica della ripresa

Applicare Stabilize a una clip con un originale accessibile. Il percorso proviene dalla proprietà della clip fornita da Resolve. Per forzare una nuova lettura usare **Leggi dati clip**. Il codice invalida la vecchia analisi e rilegge i dati dello stesso originale. Se Resolve non espone un file (per esempio alcuni contenitori compositi), il pannello indica il limite. I campi e la precisione dipendono da camera, modalità, firmware e lente. Il catalogo di calibrazione viene incorporato in compilazione. La patch al core elimina la scansione delle cartelle dei profili e il caricamento dei preset predefiniti dell’app Gyroflow. Per il montaggio serve il solo bundle compilato; la build dei sorgenti acquisisce ancora dipendenze e catalogo sulla macchina di sviluppo.

## Database dei metadati per clip (MecenaClipDB)

**mecena-clipdb** estrae da ogni originale giroscopio, accelerometro, orientamento integrato, posa, readout, dimensioni del sensore, focale, zoom, fuoco e diaframma, con lo stesso parser e le stesse calibrazioni di Stabilize. Il binario si trova in **MecenaMotion.ofx.bundle/Contents/Resources/**. Conviene lanciarlo all’ingest, quando le schede sono ancora collegate:

    /Library/OFX/Plugins/MecenaMotion.ofx.bundle/Contents/Resources/mecena-clipdb scan ~/Movies/MecenaClipDB /Volumes/SCHEDA_A

Una seconda scansione rilegge soltanto le clip nuove o modificate; `--force` le rilegge tutte. Ogni clip occupa una cartella **clips/<id>/** con questi file:

| File | Contenuto |
| --- | --- |
| `clip.json` | camera, ottica, video, posa e angolo shutter, readout, IMU, sensore in px e mm, intervalli di focale, fuoco, diaframma e zoom |
| `gyro.gyroflow` | progetto Gyroflow con i dati movimento incorporati, quello che Stabilize carica |
| `imu.csv` | campioni grezzi di giroscopio (°/s), accelerometro e magnetometro |
| `orientation.csv` | quaternione integrato nel tempo |
| `lens.csv` | per ogni campione ottico: focale, fuoco, f/T-stop, zoom, focale in pixel, centro ottico, sensore e pixel pitch |
| `frames.csv` | tempo e offset di cattura di ogni fotogramma |
| `metadata.json` | metadati del file senza le serie, compresi i dati specifici della camera |

L’elenco si trova in **index.json**. Il formato è descritto in `vendor/common/src/clipdb.rs`.

Stabilize cerca la clip nel database prima di leggere l’originale. I database consultati sono quelli registrati dalla scansione, quelli indicati da `MECENA_CLIPDB` e una cartella **MecenaClipDB** posta accanto agli originali o fino a cinque livelli sopra. La corrispondenza si cerca prima per percorso, poi per nome e dimensione e infine per nome senza i suffissi dei proxy (`_proxy`, `_prx`…). Due schede con la stessa numerazione si distinguono dalle cartelle in comune. Se nessun criterio basta, il plugin non sceglie. Funziona quindi anche con gli originali offline e con i proxy. **Dati rilevati** riporta «DB Mecena» e il riepilogo della clip.

**ProRes RAW e registratori esterni.** Un file di Atomos/Ninja o un intermedio non porta i dati del giroscopio. Li prende dalla clip interna registrata insieme. Importa le clip interne nel Media Pool (anche in un bin a parte) e lancia lo script **Mecena ClipDB**. Lo script abbina ogni file senza dati movimento alla clip interna che ne copre la parte più lunga del timecode, purché ne copra almeno metà. Lo scarto fra i due inizi diventa l’offset in frame. Il timecode deve arrivare al registratore via HDMI/SDI. **Dati rilevati** mostra «dati di C0001.MP4 (collegamento timecode, offset +37 frame)». Per correggere o forzare un abbinamento usa:

    mecena-clipdb link ~/Movies/MecenaClipDB /Volumes/NINJA/RAW_0001.mov /Volumes/SCHEDA_A/C0001.MP4 --offset 37

Un collegamento manuale non viene sovrascritto dallo script. I proxy di Resolve e i file dei registratori non contengono la telemetria: per questo il database resta legato all’originale interno. Il RAW può usare un’area del sensore diversa dall’interno, come il 4.2K RAW e il 4K interno di FX3. Stabilize adatta la geometria per proporzioni, quindi va verificata quando i due formati hanno un crop diverso.

**Regolazioni avanzate → Sorgente movimento.** **Automatica** è la scelta predefinita e usa, in quest’ordine, il database, i collegamenti per timecode e il file della clip. **Solo file della clip** ignora il database e legge sempre la telemetria dall’originale. **Offset sincronismo (frame)** sposta i dati movimento rispetto all’immagine, in frame della clip che li ha registrati. Si somma all’offset del collegamento, vale per ogni sorgente e accetta keyframe.

**Adjustment clip su tutta la timeline.** L’installer pkg copia `Mecena ClipDB.py` in `/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts/Utility/` e collega `mecena-clipdb` in `/usr/local/bin`. Con il solo zip copia lo script a mano da Resources. Con la timeline aperta lancia **Workspace → Scripts → Mecena ClipDB**. Lo script aggiorna il database con gli originali raggiungibili e scrive in **timelines/** la mappa che associa ogni frame di timeline a una clip, al frame sorgente e alla velocità. La mappa diventa quella attiva. Stabilize su un adjustment clip legge, frame per frame, la clip visibile più in alto e la stabilizza con i suoi dati. Con più adjustment clip li distingue per durata. Dopo ogni modifica al montaggio rilancia lo script. Limiti: le speed ramp a velocità variabile non vengono seguite (la velocità costante sì), e le clip ridimensionate o riposizionate sotto l’adjustment clip ricevono una correzione pensata per il quadro intero. Il caso d’uso previsto è una timeline di proxy o originali a quadro pieno.

## Uso previsto in Edit e Color

La categoria **Log MetaRaw** (la stessa di Log MetaRaw e Detail) espone i due effetti, che restano nello stesso binario OFX. Applicare **Stabilize** sulla clip e poi **Motion Blur**. Questa scelta lascia a Resolve il compito di rendere i fotogrammi adiacenti del primo stadio anche durante lo scrubbing o un render fuori ordine. Si può usare ogni effetto singolarmente. La lettura avviene dall’originale indicato da Resolve oppure, per rileggere dopo un relink, tramite il pulsante «Leggi dati clip» che interroga di nuovo Resolve. Il plugin usa il parser telemetrico di Gyroflow, le calibrazioni integrate e i fotogrammi passati dall’host. Il progetto non installa un’app, un interprete, script di menu o un secondo bundle. Non crea master intermedi. I dati della lente dipendono da camera, modalità e originale; una distanza di fuoco non equivale a una mappa di profondità.

**Sony e DJI con dati movimento**: applicare Stabilize e verificare stato, profilo e sincronismo, poi aggiungere Motion Blur. L’adattatore legge il frame sorgente quando Resolve lo espone. Se manca l’informazione, il tempo viene stimato dalla cadenza e dagli intervalli; una rampa arbitraria può produrre disallineamento. I timestamp VFR irregolari e le diverse modalità S&Q richiedono verifica su file reali.

**iPhone o clip senza gyro utilizzabile**: applicare Motion Blur e attivare «Stabilizzazione ottica leggera (senza gyro)». Questa prima modalità stima la traslazione globale da corrispondenze locali, conserva una panoramica a velocità costante e attenua gli scatti tra fotogrammi. I bordi sono estesi per campionamento. Rotazioni, soggetti che occupano gran parte del quadro e forte parallasse sono limiti di questa modalità.

**Retiming**: per rendere esplicito l’ordine, stabilizzare la clip interna e applicare Motion Blur al livello che riceve i fotogrammi già ritimati, ad esempio all’esterno di una compound clip. La prova in Resolve deve usare un riferimento alla stessa cadenza di consegna.

**Colore**: Motion Blur espone lineare, S-Log3 e DaVinci Intermediate. Impostare la curva effettivamente ricevuta dall’OFX. Per altri log usare trasformazioni interne a Resolve attorno al nodo Color per lavorare in scene-linear. Riconoscimento automatico dello spazio in ogni configurazione, virtual production e parità visiva con RSMB restano da validare o sviluppare.

## Prove incluse

La prova `tests/cinema.cpp` richiama le azioni OpenFX di descrizione, richiesta dei frame e render. La scena sintetica combina pan, tilt e roll con una mano articolata in movimento indipendente. La sorgente simula un’acquisizione a 50 fps e 90°. Il riferimento calcola direttamente la luce e la visibilità della stessa scena in 96 istanti distribuiti su 20 ms. In questo test l’host rifiuta tutte le richieste intermedie e presenta solo frame interi a 25 fps.

| Zona | Errore RGB medio senza effetto | Errore RGB medio con Cinema a 180° |
| --- | ---: | ---: |
| Sfondo | 0,04717 | 0,00203 (prima: 0,00822) |
| Interno della mano | 0,05787 | 0,00822 (prima: 0,02041) |
| Contorni e zone che si coprono o scoprono | 0,06565 | 0,03689 (prima: 0,04224) |

I valori sono errori assoluti medi in RGB lineare su un istante della prova. Le verifiche includono intermedi distinti, arrotondati, duplicati e miscelati, 100p su 23,976p a 135°, posa originale diversa, guida remota attraversata da un taglio e immagine ferma. Una mira a righe ripetute riproduce la partenza, l’arresto e l’inversione del moto con una zona ferma accanto. Su CPU l’errore medio in questi tre casi passa rispettivamente da 0,03304 a 0,00840, da 0,03299 a 0,00839 e da 0,01254 a 0,00189. I test preesistenti coprono reverse, ramp, cadenza, bypass e curve log. Sul sistema di sviluppo Linux sono stati eseguiti il motore CPU e l’host di prova. Lo script Mac ripete i test sul binario integrato e controlla Metal. La resa sulla panoramica Sony originale, il confronto con RSMB e i tempi in Resolve su M4 Pro richiedono un riscontro su quelle macchine e immagini.

## Provenienza

Adattatore e codice comune derivati da gyroflow/gyroflow-plugins, commit 1d01ee04ca862b1a73cd8e2a507cfd1e95c03315. Gyroflow core e telemetry-parser sono dipendenze di compilazione bloccate in vendor/openfx/Cargo.lock. La variante elimina il ponte fuscript e l’apertura dell’app esterna, registra i due descrittori in un binario e collega staticamente il codice C++ del mosso. Licenza del progetto GPL-3.0-or-later, header OpenFX con licenza BSD inclusa in SDK. Fonti di riferimento: https://github.com/gyroflow/gyroflow-plugins e https://github.com/gyroflow/gyroflow.

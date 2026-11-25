<p align="center"><img src="assets/icon_1024.png" width="140" alt="Log MetaRaw"></p>

<h1 align="center">Log MetaRaw</h1>
<p align="center"><b>Metadata log delle camere (Sony, Panasonic, Apple, DJI e altre) e controlli di sviluppo in luce di scena per DaVinci Resolve 20 e 21</b><br>
Ivan Mazzone + Claude · <a href="https://github.com/ivan-94m">github.com/ivan-94m</a> · <a href="https://instagram.com/ivan_94m">@ivan_94m</a></p>

<p align="center">
<a href="README.md">English</a> · <b>Italiano</b> · <a href="README.es.md">Español</a> · <a href="README.pt.md">Português</a> · <a href="README.zh.md">简体中文</a>
</p>

---

## Cos'è

Le camere Sony scrivono in ogni file come è stata girata l'inquadratura: bilanciamento in Kelvin, tinta, EI, obiettivo,
diaframma, shutter, profilo colore. Resolve usa questi dati sugli MXF di FX6 e FX9, che hanno il pannello *Camera Raw*.
Sugli MP4 di FX30, FX3, serie a7 o a6000 li ignora.

Log MetaRaw legge questi dati e li usa. È fatto di uno script e cinque nodi, numerati sulle icone nell'ordine della catena:

| | Dove | Cosa fa |
|---|---|---|
| **Script** | Workspace › Scripts › Log MetaRaw | legge i metadata di tutte le clip del progetto e li scrive nel Media Pool |
| Nodo **1 · Log MetaRaw Noise** | Color › OpenFX | riduzione rumore sull'immagine log, con il rumore atteso per camera, ISO e formato; temporale e spaziale, GPU Metal |
| **2 · Stabilize**, **3 · Motion Blur** | Color › OpenFX › Log MetaRaw | stabilizzazione dal giroscopio (Gyroflow integrato) e mosso ricostruito dal flusso ottico; solo Mac Apple silicon |
| Nodo **4 · Log MetaRaw** | Color › OpenFX | sviluppa una clip partendo dai valori di ripresa: bilanciamento, esposizione, spazio colore, toni per zone, false color |
| Nodo **5 · Log MetaRaw Creative** | Color › OpenFX | sviluppo creativo: 16 procedimenti fotografici (negativi e stampe, invertibili, bianco e nero, Technicolor, Autochrome, tre lastre, dagherrotipo, collodio, stampe antiche), video analogico e camere digitali d'epoca; stampe e generazioni di copia; preset per dispositivo e modello (cineprese, videocamere a tubo e CCD, camere digitali e telefoni), 14 ottiche d'epoca, riversaggi (telecinema, kinescopio, film-out) e un supporto di visione (sala, proiettori, televisori a tubo e Trinitron, plasma, primi LCD, console, carta) |
| Plugin **Log MetaRaw Ripresa** e **Mix e supporto** (VST3; Audio Unit su Mac) | Fairlight › Effetti › VST3; Logic Pro › Audio Unit › Mecena SRL | suono d'epoca: sulle tracce registratore e microfono reali (Geloso, Nagra, Uher, Walkman…), su bus e master banco, supporto (nastro, cassetta, vinile, VHS, banda ottica, radio, telefono), riversaggi e ascolto; ogni stadio con interruttore; Mac e Windows |

Nessun file viene transcodificato, nessun originale viene mai scritto.

### Cosa aspettarsi, onestamente

**Non è raw.** Un MP4 log è già demosaicizzato e compresso, a 8 o 10 bit, spesso 4:2:0, con la riduzione rumore della
camera già applicata. Nessun plugin può restituire quello che la camera ha buttato via.

Il nodo applica la scienza colore con rigore. Esposizione e bilanciamento lavorano in luce lineare, partono dai valori
che la camera ha registrato e seguono curve e gamut pubblicati. I toni spostano l'immagine in stop. Lavorata così,
un'immagine log **ha un comportamento che ricorda un file RAW**: il bilanciamento si sposta pulito, l'esposizione si
muove come uno stop di luce, le alte luci si arrotondano invece di rompersi.

Nel lavoro intenso, però, la sola scienza colore non basta. Appena si forzano i limiti della camera, l'assenza di
informazioni nell'immagine si fa sentire: banding nei cieli, rumore nelle ombre alzate, alte luci bruciate che restano
bruciate, colore che si sfalda nei canali compressi. Esponi bene in ripresa. Log MetaRaw ti aiuta a tirare fuori il
meglio da quello che c'è. Non può creare quello che non c'è.

---

## Installazione

1. Scarica `SLogMetaRaw-4.3.1.dmg` da **Releases** e aprilo.
2. Doppio clic su **Installa Log MetaRaw.pkg**. Non è firmato con un certificato Apple: la prima volta usa tasto
   destro › **Apri**. Chiede la password del Mac perché il plugin va in una cartella di sistema.
3. Riavvia DaVinci Resolve.

L'installer cancella anche la cache dei plugin di Resolve (`OFXPluginCacheV2.xml`), che Resolve ricostruisce al
prossimo avvio. Senza, Resolve mostrerebbe ancora il pannello vecchio e non vedrebbe il nodo Creative.

| Installato | Percorso |
|---|---|
| I due nodi (un solo bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| La libreria Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Lo script del menu | `…/DaVinci Resolve/Fusion/Scripts/Utility/Log MetaRaw.py` |

Mentre lavora tiene un piccolo record JSON per clip in `~/Library/Application Support/SLogMetaRaw/cache`. Nel disco ci
sono anche le guide illustrate di undici pagine, in italiano e in inglese.

**Installazione pulita e disinstallazione.** Ogni installazione parte pulita: l'installer sostituisce per intero il
plugin e la libreria precedenti e toglie le installazioni di sviluppo, così non resta nessun file di una versione
vecchia. **Disinstalla Log MetaRaw.command**, nel disco (la prima volta: tasto destro › Apri), rimuove tutte le
versioni installate, 1.x e 2.x comprese, con cache, impostazioni e log. Prima di toccare qualcosa elenca tutto, ti chiede
di chiudere Resolve e ti chiede se cancellare anche i CSV esportati. I metadata già scritti nei progetti di Resolve fanno
parte dei progetti e restano.

**Requisiti:** macOS 12 o successivo, Apple silicon o Intel, e DaVinci Resolve 21 o 20. Resolve 20 non ha un Python
suo: installa Python 3 da [python.org](https://www.python.org/downloads/), che serve comunque al suo menu
Workspace › Scripts. **Testato solo su Resolve Studio 21.1 su macOS; Resolve 20 non ancora provato.**

**Clip:** Sony XAVC in `.MP4` o `.MXF`, più le camere di [docs/CAMERAS.md](docs/CAMERAS.md) (Panasonic V-Log, Apple
Log, DJI D-Log e D-Log M e altre), ognuna con il grado di affidabilità della lettura. Lo script le legge tutte. I nodi
sviluppano **S-Log3** (S-Gamut3.Cine o S-Gamut3), **S-Log2**, **S-Log** (S-Gamut) e le curve di quelle camere. Dalla
3.2.0 DJI D-Log M usa la curva misurata su una Osmo Action 6 ([docs/DJI_LOG.md](docs/DJI_LOG.md)): per le altre camere
DJI è la stima migliore, non una taratura. Con un profilo sconosciuto restano neutri e lo dicono.

---

## In breve

1. Importa il girato. **Workspace › Scripts › Log MetaRaw**: premi **1 · Leggi metadata**, poi **2 · Scrivi in Resolve**.
2. Nella pagina Color metti **Log MetaRaw** come **primo nodo**. Prende EI, Kelvin e tinta della clip. A quei valori non
   cambia niente.
3. Correggi bilanciamento ed esposizione nel nodo. Le viste false color aiutano.
4. Modella i toni con **Toni**. Per il look di una pellicola, di un procedimento antico, di un nastro o di una camera
   d'epoca aggiungi **Log MetaRaw Creative** come nodo successivo.
5. Poi il tuo CST, LUT o DRT.

```
Noise  →  Stabilize  →  Motion Blur  →  Log MetaRaw  →  Log MetaRaw Creative  →  CST / LUT / DRT  →  resto del grade
```

---

## Lo script

La finestra ha una riga di pulsanti, una di opzioni e l'elenco delle clip, e segue la lingua di Resolve (italiano,
inglese, spagnolo, portoghese, cinese semplificato).

- Un menu sceglie le clip: *Tutto il Media Pool* o *Clip selezionate nel Media Pool*.
- **1 · Leggi metadata**: una riga per clip con camera, obiettivo, diaframma, shutter, EI, WB, spazio colore e data
  level. La colonna *Stato* dice `letto`, cosa è cambiato durante la ripresa (diaframma, fuoco…) o perché una clip è stata
  saltata. Clicca una riga per vedere tutto quello che è stato letto, diviso come in Catalyst Browse.
- **2 · Scrivi in Resolve**: riempie i campi del Media Pool (pannello Metadata, colonne, keyword per le smart bin, data
  burn) e corregge i valori che Resolve legge male dagli MXF, come *Camera Aperture* `F53343` sulla FX6.
- **Esporta CSV**: esporta i valori per cui Resolve non ha un campo (EI, tinta, modo WB, distanza di fuoco, gamma di
  ripresa…) nel formato CSV dei metadata di Resolve.
- Opzioni:
  - **Tag per camera** (spento): aggiunge camera, gamma e primarie alle keyword.
  - **Sovrascrivi metadata** (acceso): sostituisce i valori già scritti da Resolve; spento, riempie solo i campi vuoti.
  - **Correggi il Data Level** (acceso): imposta il *Data Level* di ogni clip su Full o Video, quello che serve alla
    sua gamma. Il perché è in [docs/DATA_LEVELS.md](docs/DATA_LEVELS.md).
  - **Imposta anche Input Color Space** (spento): per i progetti color managed.
    Uno script non può riportarlo su *Project*: solo tu, a mano.
- La versione in basso a destra, cliccata, controlla GitHub.

La lettura è veloce perché non decodifica niente: al massimo 24 campioni della traccia metadata per clip, con un tempo
massimo di un secondo. Un volume che smette di rispondere viene saltato una volta sola, con un avviso, invece di bloccare
l'elenco.

---

## Il nodo Log MetaRaw

Il nodo è puntuale: ogni pixel dipende solo da sé stesso. Non crea mai aloni e **Generate LUT** lo può esportare
(consigliati 65 punti).

| Controllo | Cosa fa |
|---|---|
| **Versione** (in cima) | `v4.3.1`. Una volta al giorno chiede a GitHub l'ultima release. Se ce n'è una diventa **🟢 v3.1.0 → 3.x.y** e il clic apre nel browser il download dell'installer del sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Non installa mai niente da solo |
| **Camera** · **Rileggi metadata** | la camera letta. *Rileggi* rilegge la clip, riporta ogni controllo ai valori di camera e scrive i metadata della clip nel Media Pool |
| **Ripresa** · **Decodifica** | *Clip* permette di cambiare i controlli; *Camera metadata* li blocca sui valori di ripresa |
| **Bilanciamento** · **Temperatura** · **Tint** | As shot o preset. Adattamento cromatico Bradford in luce lineare, dal bianco registrato dalla camera |
| **Punto bianco** | il contagocce di Resolve: scegli sull'immagine un punto che deve essere neutro e Temperatura e Tint si regolano da soli, con la stessa matematica del render |
| **Esposizione** (EI) | indice di esposizione: EI doppio = +1 stop |
| **False color** | una casella sopra Temperatura, una sotto Tint, una sotto Esposizione; vedi sotto |
| **Spazio colore** | tre sottogruppi. **Ingresso**: Spazio colore e Gamma in ingresso, Rilevato e *Ripristina da camera*, Data level, Livello e *Ripristina da clip*. **Senza metadata**: *Sblocca controlli* e i riferimenti di ripresa. **Uscita**: **Tone mapping** (Nessuno/CST, ACES 2.0, Luminanza, Filmico, ACES 1.3, OpenDRT), Compressione e Mappatura gamut, Vibrance luci, Spazio colore e Gamma (come un Color Space Transform; *Timeline* non converte), dimensione ed esportazione della LUT |
| **Toni** | Highlights, **Bianco** (stop), Soft Clip, Saturation (−100…+100). *Mostra tutti i controlli* aggiunge Contrast, Shadows, Whites, Blacks, Vibrance e le **Zone** (Black, Shadow, Light, Specular con Exp, Sat, Range e Falloff, Contrast Pivot, false color *Zone*); compaiono da soli se il nodo li usa |
| **Dati di ripresa** | sola lettura: obiettivo, focale, diaframma, fuoco, shutter, EI, WB, fps, ND, LUT di camera |

**Highlights è una spalla filmica.** In negativo comprime le alte luci con una pendenza che cala in modo continuo verso
l'alto, come la pellicola, ACES 2.0 e AgX. A −100 il massimo che la camera ha registrato (circa +6 stop sopra il grigio in
S-Log3) arriva esattamente sul **Bianco**: niente velo grigio, niente clip. Grigio e toni sotto restano fermi, la pelle a +1
stop si sposta al massimo di 0,05 stop. Le luci più compresse vanno dolcemente verso il bianco senza cambiare tinta. In
positivo dà più stacco alle alte luci, con una pendenza limitata. **Bianco** è dove arriva quel massimo (e il tetto di Soft
Clip): 2,5 stop è il bianco Rec.709 con un CST senza tone mapping. Con un DRT dopo il nodo (ACES, AgX, DaVinci) alzalo a
4–5, altrimenti le alte luci vengono compresse due volte.

**Toni per zone.** Shadows, Whites e le Zone sono esposizioni, in stop, su una fascia di toni: Shadows sotto −1 stop,
Whites da +3,5 stop. Blacks è un velo lineare che sposta il nero senza muovere il grigio. Dentro la zona l'immagine si muove
come con un'esposizione, quindi la texture resta, e la compressione sta in una banda di transizione dichiarata. Nessuna
combinazione di cursori può solarizzare. Il costo di un nodo puntuale, detto chiaramente: quello che comprime, lo comprime
anche nella texture. Per questo il recupero locale è un nodo a parte. Formule e ricette:
[docs/TONE_MAPPING.md](docs/TONE_MAPPING.md).

**False color.** Una vista per controllo, accanto al cursore che serve (sopra Temperatura, sotto Tint e sotto Esposizione):
- **Esposizione** usa bande in stop attorno al grigio 18%, nello stile ARRI. Verde è il grigio medio, rosa uno stop sopra
  (l'incarnato), giallo vicino al clip, rosso il clip, blu e viola il fondo.
- **Temperatura** e **Tinta** funzionano come in CineMatch. L'immagine diventa grigia, le dominanti si colorano di
  arancio/blu o verde/magenta, e quelle quasi neutre sono esaltate fino a 8× perché si vedano. Muovi il cursore finché
  quello che deve essere neutro resta grigio. Ogni vista risponde solo al suo cursore.

La vista sostituisce l'immagine: spegnila prima di renderizzare. [docs/FALSE_COLOR.md](docs/FALSE_COLOR.md)

**Data level.** Se Resolve decodifica una clip sulla scala di code value sbagliata, il nodo la corregge prima della curva
log. L'opzione *Correggi il Data Level* dello script la sistema per tutto il progetto.
[docs/DATA_LEVELS.md](docs/DATA_LEVELS.md)

**Tone mapping con uscita di scena (3.2.0).** Con un rendering (ACES 2.0, Filmico, OpenDRT…) uno Spazio colore e
una Gamma di scena come DaVinci WG / DaVinci Intermediate non vengono più sostituiti da Rec.709 / Gamma 2.4: l'immagine
resa per Rec.709 viaggia in quella codifica, per passarla ad altri plugin (Dehancer…), e un CST verso Rec.709 / Gamma 2.4
senza tone mapping la riporta identica. Le uscite di schermo (Rec.709, P3, Rec.2020) non cambiano.

**Senza metadata.** Un ProRes da registratore esterno, o una clip che non si legge, lascia il nodo neutro. Spunta
*Spazio colore › Senza metadata › Sblocca controlli* e scrivi EI, Kelvin e tinta di ripresa: i controlli si attivano e il nodo
parte neutro.

**Velocità.** Aprire un progetto non legge nessun file. Aprire il pannello aspetta al massimo mezzo secondo; un disco
lento finisce in background, con un limite di 15 secondi. *Rileggi* risponde in circa 2 secondi al massimo.

---

## Il nodo Log MetaRaw Creative

Un solo motore che rifà **il procedimento**, non un'imitazione del risultato. La luce di scena, decodificata in lineare
Rec.2020, diventa fino a tre **record** con la loro sensibilità spettrale. Ognuno è esposto sulla curva del materiale e
trasformato in immagine come faceva quel processo: coloranti sottrattivi, filtri additivi, un deposito d'argento o di
platino sulla carta, un segnale video. Ogni processo è una tabella di numeri e il pannello mostra solo i comandi che
contano per quello scelto.

| Processo | Cosa modella |
|---|---|
| **Negativo colore e stampa** | tre strati con contrasto, piede e spalla propri, interimage, grana per strato, alone, stampa positiva con le luci R G B; 14 pellicole, anche dell'Est (Sovcolor, ORWO, Svema, Gevacolor). *Uscita Log* dà la scansione del negativo, nella codifica d'ingresso |
| **Invertibile**, **Bianco e nero** | diapositiva; B/N con sensibilità blu, orto o pan e filtro di presa compensato |
| **Technicolor** tre strisce e due colori | tre negativi dal prisma, matrici per imbibizione, chiave K fino al 1944; solo rosso e verde |
| **Autochrome**, **Dufaycolor** | mosaico additivo di grani di fecola tinti o rete regolare, visto in trasparenza |
| **Tre lastre**, **Kinemacolor** | pose in sequenza: con *Intervallo* > 0 il nodo legge i fotogrammi successivi e ciò che si muove si separa nei colori (Prokudin-Gorskij) |
| **Dagherrotipo**, **Collodio**, carta salata e albumina, cianotipia, platino | depositi sensibili al blu e all'UV, specchio e ossidazione, supporto, bordo, viraggio |
| **Video analogico** | 11 formati, dal broadcast al VHS LP e al Betacam SP: banda di luma e colore, ritardo del colore, peaking, rumore, cambio testine, dropout |
| **Digitale d'epoca** | 10 camere: clip e knee, nitidezza elettronica, rumore, sottocampionamento, banding |

Comandi, per gruppi: **Ripresa e sviluppo** (esposizione, bilanciamento della luce, sviluppo spinto o tirato, argento
trattenuto, preflash, età), **Stampa** (punti della stampatrice, contrasto), **Grana e ottica** (grana, alone, diffusione,
aberrazione, vignettatura, instabilità del quadro, sfarfallio, polvere, graffi; in multipli di quelli del materiale e del
**Formato**, da 8 mm a IMAX 15/70), **Separazioni**, **Mosaico**, **Lastra e carta**, **Segnale**, **Colore** (viraggio di
ombre e luci, saturazione, densità colore).

- Va **subito dopo** Log MetaRaw. Decodifica in luce lineare ciò che riceve ed esce in Rec.709 · Gamma 2.4 (la stampa
  proiettata), o nella codifica d'ingresso con *Uscita Log*.
- È **spaziale**: Generate LUT lo esclude. I raggi seguono l'altezza del fotogramma: stesso aspetto in proxy. Il rumore è
  deterministico, quindi CPU e Metal danno la stessa immagine a ogni render.
- Sostituisce il nodo Detail nello stesso posto: un grade salvato con il Detail si apre come un Creative nuovo.

I numeri sono stime dalle ricerche in [docs/CREATIVE.md](docs/CREATIVE.md), non profili di prodotti: vanno tarati su
riprese vere. Non ancora provato in Resolve; Metal verificato dai test della CI macOS.

**Novità della 4.3.1.**

- **Audio, scelte coerenti**: nei due plugin audio ogni menu propone solo ciò che va con le scelte sopra: prima la
  dotazione del registratore (supporto, microfono, riduzione rumore), poi le alternative compatibili (niente VHS in
  un Nagra); in *Mix e supporto* il banco decide i supporti del master e il master su cosa si può riversare.
- **Pelo nel finestrino, polvere e graffi veri**: il pelo si assottiglia verso la punta, ha densità irregolare e
  un'ombra sfocata come sulla pellicola (di più su Super 8 e sulle copie); a volte è arricciato o è una fibra a due
  fili. La polvere ha bordi irregolari e granelli di ogni misura; i graffi cambiano spessore e intensità lungo la riga.
- **Scansione con le scritte del produttore**: da bordo a bordo il margine porta la marca (EASTMAN prima del 1990,
  poi KODAK; FUJI, AGFA, ORWO…), il codice della pellicola nel formato, NITRATE FILM, SAFETY FILM o ESTAR, un numero
  progressivo e dal 1990 il KeyKode con il codice a barre; le pellicole foto numerano i fotogrammi. La luce dello
  scanner fa alone intorno alle perforazioni e lungo il taglio della base. Nuovo comando *Overscan della scansione*.
- **Colonna sonora sulle copie**: da bordo a bordo una copia 35 mm o 16 mm mostra la colonna sonora nelle posizioni
  SMPTE: pista ottica (densità variabile, area variabile, Dolby Stereo), dal 1993 Dolby Digital tra le perforazioni,
  DTS e SDDS, dal 2004 la pista ciano; Super 8 e 16 mm possono avere la pista magnetica. Nuovo menu *Colonna sonora
  della copia* (secondo l'epoca o a mano).

**Novità della 4.3.0.**

- **Pellicole per formato**: con una cinepresa scelta compaiono solo le pellicole fatte nel suo formato, di ogni
  produttore e con il codice di quel formato (in 16 mm il 500T è 7219); i listini completi restano a una spunta.
- **Scansione**: il quadro, il quadro con i bordi del mascherino o la pellicola da bordo a bordo con perforazioni,
  interlinee e quadri vicini, con le misure vere di ogni formato. Mascherino squadrato, arrotondato o limato aperto.
- **Il negativo com'è**: il negativo colore in trasparenza, complementare sulla maschera arancione, con la sua grana.
- **Supporto della pellicola**: nitrato, acetato o poliestere (o secondo epoca e formato) e il suo degrado: macchie
  ambrate del nitrato, cristalli e instabilità dell'acetato.
- **Oscillazione realistica** in ripresa, in stampa e in proiezione, ognuna con la sua spunta; **pelo nel finestrino**
  secondo la cinepresa.
- **Console** come supporto di visione: NES, Master System, Mega Drive, Super Nintendo, PlayStation e Saturn sul
  televisore a tubo.
- Corretto il crash scegliendo i supporti di visione aggiunti nella 4.2.0.

**Novità della 4.2.0.**

- **Dispositivo › modello**: si parte dalla macchina (cinepresa, videocamera, cinema digitale, telefono), che carica
  formato, nastro, sensore e difetti; poi produttore e pellicola, con nomi veri. Gli interruttori *Tono*, *Colore* e
  *Texture* permettono di usare per esempio solo la grana sul proprio video.
- **Video emulato**: nastro VHS e Betacam rifatto riga per riga (rumore FM, colore in ritardo, testine, dropout) e
  sensore della camera: tubi con scia e code di cometa, smear del CCD, falso colore dei tubi a strisce.
- **Digitali per modello**: dimensione del sensore e aliasing, nitidezza interna, blocchi 8x8 del codec e colore
  sottocampionato (dal Motorola RAZR all'ARRI Alexa).
- **Riversato su**: la copia su VHS, Betacam, VHS di terza generazione, kinescopio 16 mm o film-out 35 mm, con la grana
  che si somma.
- **Ottica**: 14 obiettivi (Cooke, Zeiss Super Speed, Canon K35, Panavision, LOMO, Helios, Petzval…) con distorsione,
  bordi morbidi, vortice, velo e striscia anamorfica.
- **Supporti di visione nuovi**: proiettore Super 8, Trinitron, plasma, primo LCD.

---

## I nodi Noise, Stabilize e Motion Blur (4.0.0)

Lavorano sull'immagine della camera, prima di **Log MetaRaw**.

- **Log MetaRaw Noise** sa quanto rumore aspettarsi e dove. Dal record dello script legge la camera, l'ISO (o l'EI e
  l'ISO nativo), il codec, i bit per pixel e il 4:2:0. Ha profili per Sony FX6 e FX30, iPhone 17 Pro e DJI Osmo
  Action 4 e 5 Pro. Il tasto **Rileggi metadata** rilegge questi dati dal file (all'apertura del pannello partono da soli se mancano). Per tararlo sul rumore vero spunta **Mostra campione**, porta i due angoli del riquadro su una zona piatta e premi **Usa il campione** (come Neat Video); bande, ombre e pelle sono nel gruppo **Avanzate**.
  - *Temporale*: da ±1 a ±3 fotogrammi vicini, allineati a blocchi e pesati pixel per pixel, così le parti in movimento
    e i tagli non si impastano.
  - *Spaziale*: luminanza e colore separati, in 5 bande wavelet. Ogni coefficiente è confrontato con l'energia del suo
    vicinato, che tiene la trama.
  - Viste del rumore rimosso, protezione di dettaglio e pelle, e **Salta Noise ovunque** per spegnere tutti i nodi
    Noise mentre monti.
  - Su Mac gira tutto su GPU Metal, e i test verificano che dia lo stesso risultato della CPU. Altrove gira su CPU.
  - I profili sono stime finché non vengono tarati su campioni reali. [docs/DENOISE.md](docs/DENOISE.md)
- **Stabilize** legge il giroscopio della clip (Gyroflow integrato). Ha stili dal micromosso al crane e un **MASTER**
  animabile. **Motion Blur** ricostruisce l'otturatore dal flusso ottico, per esempio da 50p a 90° a 25p a 180°, e lascia la grana del sensore nelle zone mosse.
  Vengono entrambi da Mecena Motion (`motion/`) e li installa il pacchetto macOS, solo Apple silicon.
- **Acquisizione profonda**, una spunta nella finestra dello script: salva il giroscopio e l'obiettivo di ogni
  fotogramma nel MecenaClipDB, per Stabilize.

Nessuno dei tre nodi è ancora stato provato in Resolve su clip reali.

## Log MetaRaw Audio (VST3, 4.2.0)

Il gemello sonoro del Creative, per **Fairlight** (Mac e Windows): due plugin nello stesso modulo
`Log MetaRaw Audio.vst3`.

- **Ripresa**, sulle tracce: registratore (Geloso G.257, Nagra, Uher, Studer, Revox, Walkman, Auricon, fonografo Edison,
  registratore a filo…), microfono (di serie o RCA 44, U 47, C12, SM58, MKH 416…), supporto e riduzione rumore.
- **Mix e supporto**, su bus e master: banco (RCA, EMI REDD.51, Neve, SSL, Mackie), supporto (nastro, cassetta, 78/45/33
  giri, VHS, banda ottica, radio, telefono), riversaggi e ascolto (radio a valvole, juke-box, TV, sala).
- Ogni stadio ha un interruttore; Saturazione, Rumore, Wow e flutter, Dry/wet e Uscita regolano il risultato.
- Negli installer è un componente a parte, spuntato di serie. Linux: Resolve non carica VST3 nativi.

I valori sono stime da specifiche e descrizioni d'epoca ([docs/audio/AUDIO.md](docs/audio/AUDIO.md)); mai provato in
Fairlight.

## Aggiornamenti e privacy

- I **nodi** chiedono a GitHub l'ultima release di questo progetto al massimo una volta al giorno, in background. La
  richiesta porta solo la versione del programma (`User-Agent: SLogMetaRaw/4.3.1`). Per disattivarlo crea il file vuoto
  `~/Library/Application Support/SLogMetaRaw/no_update_check`.
- Lo **script** controlla solo quando clicchi la sua versione.
- Un clic apre soltanto un link di download delle release GitHub di questo progetto. Niente viene installato senza di te.

---

## Come funziona, in breve

- **Metadata.** I parser sono in Python puro, senza dipendenze. Leggono i metadata di acquisizione SMPTE RDD 18 fotogramma
  per fotogramma (traccia `rtmd` degli MP4, pacchetti ST 436 degli MXF), l'XML NonRealTimeMeta di Sony, l'SPS H.264/HEVC e
  il descrittore immagine MXF. Una clip di più gigabyte costa circa 100 KB di lettura.
- **Dallo script al nodo.** Ogni clip ha un record JSON nella cache. Il nodo trova il suo file dal percorso sorgente che
  Resolve gli passa, legge il record e, se manca, lo crea in background.
- **Matematica del colore.** La catena è: decodifica della curva di camera, rapporto di EI, bilanciamento Bradford dal
  luogo di Planck (con la tinta perpendicolare in CIE 1960 uv), poi toni con un solo guadagno per pixel da una norma dei
  suoi canali (la cromaticità resta), poi colore, poi uscita. Tutto in luce lineare, con curve e gamut pubblicati da Sony.
- **Una matematica, tre implementazioni.** La matematica sta in `ofx/SLogMetaRaw/math` e si compila sia come C++ sia come
  Metal. Un modello Python di riferimento le verifica: CPU e GPU concordano entro 2·10⁻⁴, con l'FMA disattivato su ogni
  percorso.
- **Test.** 520 test automatici: parser, modelli di riferimento, C++, Metal, un host OpenFX in miniatura che carica i due
  nodi come fa Resolve, e i file golden dei pannelli.

---

## Compilare dal sorgente

```bash
make -C ofx/SLogMetaRaw                     # plugin universale, arm64 + x86_64 (serve Xcode)
make -C ofx/SLogMetaRaw test-bins           # binari di test
python3 -m unittest discover tests          # i test
./install.sh --dev                          # script e plugin puntati su questa cartella
./packaging/build_installer.sh              # dist/SLogMetaRaw-<versione>.dmg
python3 -m slogmetaraw clip.MP4             # lettura stile Catalyst da riga di comando
```

```
slogmetaraw/         parser, scrittura in Resolve, finestra dello script, controllo aggiornamenti
resolve_script/      launcher per Workspace › Scripts
ofx/SLogMetaRaw/     math/ (condivisa CPU/Metal), src/ (common, develop, detail, noise), metal/
motion/              Mecena Motion: Stabilize, Motion Blur, mecena-clipdb (Rust + C++, macOS)
tools/               generatori di matrici, icone e grafici
tests/               test e modelli di riferimento (le clip di prova non sono nel repository)
packaging/           installer, guide, disinstallatore
docs/                TONE_MAPPING, CREATIVE, DENOISE, FALSE_COLOR, DATA_LEVELS
```

### Linux e Windows (render su CPU, sperimentale)

Il plugin si compila per Linux e Windows con CMake. Questi port renderizzano su CPU (ancora senza Metal, CUDA o
OpenCL) e non sono stati provati dentro DaVinci Resolve: la build supportata resta quella macOS.

**Installazione su Linux** (scarica da **Releases** il file adatto alla tua distribuzione):

| File | Per |
|---|---|
| `slogmetaraw_<versione>_amd64.deb` | Ubuntu, Debian, Mint: `sudo apt install ./slogmetaraw_*.deb` |
| `slogmetaraw-<versione>-1.x86_64.rpm` | Rocky, Alma, RHEL, CentOS, Fedora: `sudo dnf install ./slogmetaraw-*.rpm` |
| `SLogMetaRaw-<versione>-linux-x86_64.run` | qualsiasi distribuzione: `sh SLogMetaRaw-*.run` (`--user` senza sudo, `--uninstall`) |
| `SLogMetaRaw-<versione>-linux-x86_64.tar.gz` | lo stesso, scompattato: lancia `install.sh` |

Il plugin va in `/usr/OFX/Plugins`; la libreria Python in `/usr/lib/slogmetaraw` (.deb/.rpm) o in
`~/.local/share/SLogMetaRaw` (.run, .tar.gz). Il pacchetto mette lo script di menu in
`/opt/resolve/Fusion/Scripts/Utility` se Resolve e li; altrimenti lancia una volta `slogmetaraw-setup` (per il tuo
utente). Riavvia Resolve. Basta Python 3.6 (anche su Rocky/RHEL 8). Il binario e costruito contro glibc 2.28: si carica
su Rocky/Alma/RHEL 8+, Ubuntu 20.04+ e Debian 10+.

```bash
# compilalo tu (cmake, un compilatore C++17, python3; l'SDK OpenFX viene scaricato, oppure imposta OFX_SDK_DIR)
./packaging/linux/build_package.sh          # dist/: .tar.gz, .run, .deb, .rpm
# Windows (PowerShell): servono cmake, Visual Studio Build Tools, python; Inno Setup e facoltativo
powershell -File packaging\windows\build_package.ps1   # dist\...-windows-x64.zip (+ setup .exe con Inno Setup)
```

Lo stato e in `~/.local/share/SLogMetaRaw` su Linux e `%APPDATA%\SLogMetaRaw` su Windows. `.github/workflows/`
costruisce i pacchetti e a ogni modifica installa .deb/.rpm su Rocky 8/9, Ubuntu 20.04-24.04 e Debian 12; un tag `v*` li
allega a una release in bozza.

---

## Limiti noti

- Il pannello Camera Raw di Resolve e la stabilizzazione giroscopica di Resolve non si sbloccano per gli MP4 (per il giroscopio c'è il nodo Stabilize): vivono dentro i
  decoder di Resolve. Log MetaRaw ricostruisce i controlli del colore, non apre una porta dentro Resolve.
- Alcune camere (fra cui l'a6300) non registrano i Kelvin: in quel caso sono stimati dal preset di luce e segnalati.
- S-Log2 segue il documento Sony. La curva S-Log2 di Resolve differisce di circa 0,15 stop.
- Da verificare su più file: XAVC HS (HEVC), HLG, S-Cinetone, zoom motorizzati.
- **Progetto hobbistico e indipendente**, distribuito così com'è, senza garanzie e senza responsabilità per l'uso
  professionale. Il codice è aperto: leggilo, provalo, modificalo.

---

## Crediti e licenza

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m).
Scritto con Claude (Anthropic): la 1.0 il 19 settembre 2026, la 1.1.0 il 22 settembre, la 2.0.0 il 23 settembre 2026, la 3.2.0 e la 4.0.0 il 24 novembre 2026.
La storia completa è in [RELEASE_NOTES.md](RELEASE_NOTES.md).

Riferimenti per i tag Sony: SMPTE RDD 18, [ExifTool](https://exiftool.org) e
[telemetry-parser](https://github.com/AdrianEddy/telemetry-parser) di AdrianEddy (MIT), da cui viene parte della tabella
dei tag.

Rendering e dati delle camere: l'Output Transform ACES 2.0 è un port di OpenColorIO 2.6 e ACES 1.3 è verificato
sulle sue trasformazioni integrate (ACES: Academy of Motion Picture Arts and Sciences); [OpenDRT](https://github.com/jedypod/open-display-transform)
v1.1.0 è di Jed Smith (GPL-3.0), portato dalla sua DCTL. Curva e gamut DJI D-Log M vengono dalle misure della Osmo Action
6 in [jens-krause/dlogm-dctl](https://github.com/jens-krause/dlogm-dctl) v1.3 e dalla LUT DJI della Action 6, usata solo come
riferimento e non distribuita; fino alla 3.1.x la curva era il fit di Thatcher Freeman.

Stabilize si basa su [Gyroflow](https://github.com/gyroflow/gyroflow) di AdrianEddy e contributori (GPL-3.0) e sul suo
catalogo degli obiettivi. Il filtro spaziale del nodo Noise segue NeighShrink (Chen, Bui e Krzyżak, 2004).

[GNU GPL v3.0 o successiva](LICENSE). L'SDK OpenFX è © The Open Effects Association (BSD-3). L'SDK VST3 è © Steinberg Media Technologies (MIT); VST è un marchio di Steinberg. Sony, XAVC e Catalyst sono
marchi di Sony Group Corporation; DaVinci Resolve è un marchio di Blackmagic Design. Progetto indipendente, non affiliato
né approvato da nessuna delle due aziende.

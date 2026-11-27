# Log MetaRaw — Guida utente

[English](Wiki-Software) · **Italiano** · [Español](Wiki-Software-es) · [Português](Wiki-Software-pt) · [简体中文](Wiki-Software-zh) · [Home](Home-it)

> Versione **4.4.0** · macOS 12+ · DaVinci Resolve 20 / 21 · Sony XAVC `.MP4` / `.MXF`
> Testato solo su DaVinci Resolve Studio 21.1 su macOS.

Log MetaRaw legge i dati di ripresa che le camere Sony scrivono in ogni file (Kelvin, tinta, EI, obiettivo, diaframma, shutter, profilo colore) e li usa dentro DaVinci Resolve. Resolve lo fa solo per i file MXF di FX6/FX9; per gli MP4 di FX30, FX3, serie a7 e a6000 ignora questi dati.

**Indice**

1. [Cosa è — e cosa non è](#1-cosa-è-e-cosa-non-è)
2. [Installazione](#2-installazione)
3. [Avvio rapido](#3-avvio-rapido)
4. [Lo script](#4-lo-script)
5. [Il nodo Log MetaRaw](#5-il-nodo-s-log-metaraw)
6. [Falso colore](#6-falso-colore)
7. [Toni e Zone](#7-toni-e-zone)
8. [Il nodo Log MetaRaw Creative](#8-il-nodo-log-metaraw-creative)
9. [Pipeline: dove va ogni nodo](#9-pipeline-dove-va-ogni-nodo)
10. [Ricette](#10-ricette)
11. [Casi speciali](#11-casi-speciali)
12. [Aggiornamenti e privacy](#12-aggiornamenti-e-privacy)
13. [Disinstallazione](#13-disinstallazione)
14. [Risoluzione problemi / FAQ](#14-risoluzione-problemi--faq)
15. [Linux e Windows (sperimentale)](#15-linux-e-windows-sperimentale)
16. [Limiti noti](#16-limiti-noti)

---

## 1. Cosa è — e cosa non è

Tre strumenti, un'unica installazione:

| Strumento | Dove in Resolve | Cosa fa |
|---|---|---|
| **Script** | Workspace › Scripts › Log MetaRaw | Legge i metadata di ogni clip e li scrive nel Media Pool. Corregge il Data Level di ogni clip. |
| Nodo **Log MetaRaw** | Color › OpenFX — **primo nodo** | Sviluppa una clip dai valori di ripresa: bilanciamento del bianco, esposizione, spazio colore, toni per zone, falso colore. |
| Nodo **5 · Log MetaRaw Creative** | Color › OpenFX | sviluppo creativo: 16 procedimenti fotografici (negativi e stampe, invertibili, bianco e nero, Technicolor, Autochrome, tre lastre, dagherrotipo, collodio, stampe antiche), video analogico e camere digitali d'epoca; stampe e generazioni di copia; preset per dispositivo e modello (cineprese, videocamere a tubo e CCD, camere digitali e telefoni), 14 ottiche d'epoca, riversaggi (telecinema, kinescopio, film-out) e un supporto di visione (sala, proiettori, televisori a tubo e Trinitron, plasma, primi LCD, console, carta) |

I file originali non vengono **mai modificati**: nessuna transcodifica, nessun rewrap.

**Non è raw.** Un MP4 log è già demosaicizzato e compresso (8 o 10 bit, spesso 4:2:0, con la riduzione rumore della camera già applicata). Il nodo applica la scienza colore con rigore — esposizione e bilanciamento del bianco in luce lineare, dai valori registrati, con le curve e i gamut pubblicati da Sony — così l'immagine *si comporta* in un modo che ricorda un file raw: il bilanciamento si sposta pulito, l'esposizione si muove come uno stop di luce, le alte luci si arrotondano invece di rompersi. Oltre i limiti della camera, l'assenza di informazioni si fa sentire: banding nei cieli, rumore nelle ombre alzate, le alte luci bruciate restano bruciate. Esponi bene in ripresa.

---

## 2. Installazione

1. Scarica `SLogMetaRaw-4.4.0.dmg` da [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) e aprilo.

   ![La finestra del DMG](images/01-dmg.png)

2. Doppio clic su **Installa Log MetaRaw.pkg**. Il pacchetto non è firmato con un certificato Apple: la prima volta, **tasto destro › Apri**. Chiede la password del Mac perché il plugin va in una cartella di sistema.

   ![Tasto destro › Apri sul pacchetto non firmato](images/02-gatekeeper.png)

3. **Riavvia DaVinci Resolve.**

L'installer cancella anche la cache dei plugin di Resolve (`OFXPluginCacheV2.xml`); Resolve la ricostruisce al prossimo avvio. Senza, Resolve mostrerebbe ancora il pannello vecchio e non vedrebbe il nodo Creative.

Ogni installazione parte pulita: il plugin e la libreria precedenti vengono sostituiti per intero e le installazioni di sviluppo vengono rimosse.

**Cosa va dove**

| Elemento | Percorso |
|---|---|
| I due nodi (un solo bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Libreria Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Script del menu | `…/DaVinci Resolve/Fusion/Scripts/Utility/Log MetaRaw.py` |
| Cache per clip (JSON) | `~/Library/Application Support/SLogMetaRaw/cache` |

**Requisiti:** macOS 12 o successivo, Apple silicon o Intel, DaVinci Resolve 21 o 20 (Resolve 20: installa Python 3 da python.org, non ne ha uno proprio).
**Clip:** Sony XAVC in `.MP4` o `.MXF`, più le camere di [docs/CAMERAS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/CAMERAS.md) (Panasonic V-Log, Apple Log, DJI D-Log e D-Log M e altre), ognuna con il grado di affidabilità della lettura. Lo script le legge tutte. I nodi sviluppano **S-Log3** (S-Gamut3.Cine o S-Gamut3), **S-Log2**, **S-Log** (S-Gamut) e le curve di quelle camere. Dalla 3.2.0 DJI D-Log M usa la curva misurata su una Osmo Action 6 ([docs/DJI_LOG.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DJI_LOG.md)): per le altre camere DJI è la stima migliore, non una taratura. Con un profilo sconosciuto restano neutri e lo dicono.

---

## 3. Avvio rapido

1. Importa il girato. Apri **Workspace › Scripts › Log MetaRaw**, premi **1 · Leggi metadata**, poi **2 · Scrivi in Resolve**.
2. Nella pagina Color, aggiungi **Log MetaRaw** come **primo nodo**. Prende EI, Kelvin e tinta della clip: a quei valori non cambia nulla.
3. Correggi il bilanciamento del bianco e l'esposizione nel nodo, usando le viste falso colore.
4. Modella i toni con **Toni**. Per il look di una pellicola, di un procedimento antico, di un nastro o di una camera
   d'epoca aggiungi **Log MetaRaw Creative** come nodo successivo.
5. Poi il resto del grade e, per ultimo, il tuo CST / LUT / DRT di uscita.

```
Log MetaRaw  →  Log MetaRaw Creative  →  resto del grade  →  CST / LUT / DRT di uscita
```

![Albero dei nodi consigliato](images/08-node-tree.png)

> Copia il nodo su un'altra clip e si azzera con i dati di quella clip.

---

## 4. Lo script

**Workspace › Scripts › Log MetaRaw**

![Menu Workspace › Scripts](images/03-scripts-menu.png)

La finestra ha una riga di pulsanti, una riga di opzioni e l'elenco delle clip. Segue la lingua di Resolve (inglese, italiano, spagnolo, portoghese, cinese semplificato). Lo script legge i file; non li modifica mai.

![Finestra dello script dopo Leggi metadata](images/04-script-window.png)

### Pulsanti

| Controllo | Cosa fa |
|---|---|
| **Menu clip** | *Tutto il Media Pool* o *Clip selezionate nel Media Pool*. |
| **1 · Leggi metadata** | Una riga per clip: camera, obiettivo, diaframma, shutter, EI, WB, spazio colore, data level. La colonna **Stato** dice `letto`, cosa è cambiato durante la ripresa (diaframma, fuoco…) o perché una clip è stata saltata. Clicca una riga per vedere tutto quello che è stato letto, raggruppato come in Catalyst Browse. |
| **2 · Scrivi in Resolve** | Riempie i campi del Media Pool (pannello Metadata, colonne, keyword per le smart bin, Camera Notes, data burn-in) e corregge i valori che Resolve legge male dagli MXF, ad es. *Camera Aperture* `F53343` sulla FX6. |
| **Esporta CSV** | Esporta i valori per cui Resolve non ha un campo (EI, tinta, modo WB, distanza di fuoco, gamma di ripresa…) nel formato CSV dei metadata di Resolve. Importa con **File › Import › Metadata**, con *create custom fields* attivo. |
| **Versione** (in basso a destra) | Clicca per controllare GitHub; diventa verde quando esiste una release più recente. |

![Dettaglio clip, stile Catalyst](images/05-script-clip-detail.png)

### Opzioni

| Opzione | Predefinito | Effetto |
|---|---|---|
| **Tag per camera** | spento | Aggiunge camera, gamma e primarie alle keyword. |
| **Sovrascrivi metadata** | acceso | Sostituisce i valori già scritti da Resolve. Spento: riempie solo i campi vuoti. |
| **Correggi il Data Level** | acceso | Imposta il *Data Level* di ogni clip su **Full** (log) o **Video** (709, Cine, HLG). È la correzione vera: vale per tutto il progetto, scope ed export compresi. |
| **Imposta anche Input Color Space** | spento | Per i progetti color-managed. ⚠️ Uno script non può riportarlo su *Project* — solo tu, a mano. |

Dopo **2 · Scrivi in Resolve**, i dati compaiono nel Media Pool:

![Pannello Metadata del Media Pool compilato](images/06-media-pool-metadata.png)

**Velocità.** Niente viene decodificato: al massimo 24 campioni della traccia metadata per clip, entro un secondo. Un file di più gigabyte costa circa 100 KB di lettura. Un disco che smette di rispondere viene saltato una volta, con un messaggio, invece di bloccare l'elenco. Eseguire lo script subito dopo l'importazione fa sì che i nodi trovino i dati già pronti.

---

## 5. Il nodo Log MetaRaw

**Color › OpenFX › Log MetaRaw** — primo nodo, prima di qualsiasi CST o LUT.

![Libreria OpenFX con entrambi i nodi](images/07-openfx-library.png)

Il nodo è **puntuale**: ogni pixel dipende solo da sé stesso. Non crea mai aloni e **Generate LUT** lo può esportare (consigliati 65 punti). Le impostazioni si salvano per clip.

![Pannello principale del nodo — parte alta](images/09-node-panel-top.png)

| Controllo | Cosa fa |
|---|---|
| **Versione** (in cima) | Mostra `v3.2.0`. Una volta al giorno chiede a GitHub l'ultima release; se ce n'è una legge **🟢 v3.2.0 → 3.x.y** e un clic apre il download dell'installer per il tuo sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Non installa mai nulla da solo. |
| **Camera** · **Rileggi metadata** | La camera che è stata letta. *Rileggi* rilegge la clip, riporta ogni controllo ai valori di camera e scrive i metadata della clip nel Media Pool. Risponde in circa 2 s. |
| **Ripresa** · **Decodifica** | *Clip* permette di cambiare i controlli; *Camera metadata* li blocca sui valori di ripresa |
| **Bilanciamento** · **Temperatura** · **Tint** | As shot o preset. Adattamento cromatico Bradford in luce lineare, dal bianco registrato dalla camera |
| **Punto bianco** | il contagocce di Resolve: scegli sull'immagine un punto che deve essere neutro e Temperatura e Tint si regolano da soli, con la stessa matematica del render |
| **Esposizione** (EI) | indice di esposizione: EI doppio = +1 stop |
| **False color** | una casella sopra Temperatura, una sotto Tint, una sotto Esposizione; vedi sotto |
| **Spazio colore** | tre sottogruppi. **Ingresso**: Spazio colore e Gamma in ingresso, Rilevato e *Ripristina da camera*, Data level, Livello e *Ripristina da clip*. **Senza metadata**: *Sblocca controlli* e i riferimenti di ripresa. **Uscita**: **Tone mapping** (Nessuno/CST, ACES 2.0, Luminanza, Filmico, ACES 1.3, OpenDRT), Compressione e Mappatura gamut, Vibrance luci, Spazio colore e Gamma (come un Color Space Transform; *Timeline* non converte), dimensione ed esportazione della LUT |
| **Toni** | Highlights, **Bianco** (stop), Soft Clip, Saturation (−100…+100). *Mostra tutti i controlli* aggiunge Contrast, Shadows, Whites, Blacks, Vibrance e le **Zone** (Black, Shadow, Light, Specular con Exp, Sat, Range e Falloff, Contrast Pivot, false color *Zone*); compaiono da soli se il nodo li usa |
| **Dati di ripresa** | Sola lettura: obiettivo, focale, diaframma, fuoco, shutter, EI, WB, fps, ND, LUT di camera. |

![Avanzate e Dati di ripresa](images/14-avanzate-dati.png)

**Spazio colore nel dettaglio**

- **Ingresso › Spazio colore / Gamma** — *Automatico* chiede a Resolve o legge la camera; *Ripristina da camera* li riporta lì. **Rilevato** mostra cosa legge il nodo.
- **Ingresso › Data level** — corregge la scala di code value quando Resolve decodifica una clip su quella sbagliata. **Livello** mostra quello letto, *Ripristina da clip* torna ad Automatico. *Correggi il Data Level* dello script la sistema per tutto il progetto.
- **Ingresso › Stato** — quello che il nodo ha rilevato; avvisa anche quando una vista falso colore è attiva.
- **Senza metadata › Sblocca controlli** — vedi [§11](#11-casi-speciali).

> **L'uscita Rec.709** nel nodo con Tone mapping *Nessuno (CST)* è un CST semplice (le alte luci oltre Bianco si clippano), e ogni conversione in uscita esclude il nodo Creative. Preferisci un CST di uscita in fondo all'albero dei nodi.

**Tone mapping con uscita di scena (3.2.0).** Con un rendering (ACES 2.0, Filmico, OpenDRT…) uno Spazio colore e una Gamma di scena come DaVinci WG / DaVinci Intermediate non vengono più sostituiti da Rec.709 / Gamma 2.4: l'immagine resa per Rec.709 viaggia in quella codifica, per passarla ad altri plugin (Dehancer…), e un CST verso Rec.709 / Gamma 2.4 senza tone mapping la riporta identica. Le uscite di schermo (Rec.709, P3, Rec.2020) non cambiano.

---

## 6. Falso colore

Una vista per controllo, posizionata accanto al cursore che serve (sopra Temperatura, sotto Tint e sotto Esposizione). **La vista sostituisce l'immagine — spegnila prima di renderizzare.** (Un plugin OpenFX non può disegnare un overlay sul viewer di Resolve.)

Prima imposta *Decodifica* su **Clip**: in *Camera metadata* i controlli sono bloccati.

### Esposizione

Bande in stop, stile ARRI, attorno al grigio 18%. Tutto il resto diventa grigio.

![Bande di esposizione](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/falsecolor_bands.png)

| Colore | Significato |
|---|---|
| Verde | Grigio medio (18%) |
| Rosa | Uno stop sopra — incarnato |
| Giallo | Vicino al clip |
| Rosso | Clippato |
| Blu / viola | Ombra profonda / nero |

Attiva la vista, punta sul grigio medio o sull'incarnato, muovi **Exposure** finché l'area giusta diventa verde (grigio) o rosa (incarnato).

![Falso colore dell'esposizione nel viewer](images/10-falsecolor-exposure.png)

### Temperatura e Tinta

Funzionano come in CineMatch. L'immagine diventa grigia; le dominanti si mostrano a colori, e quelle quasi neutre sono esaltate fino a 8× perché si vedano.

| Vista | Vedi | Fai |
|---|---|---|
| Temperatura | Blu (dominante freddo) | Alza Color Temp |
| Temperatura | Arancio (dominante caldo) | Abbassa Color Temp |
| Tinta | Verde | Alza Tint |
| Tinta | Magenta | Abbassa Tint |

Scegli una superficie che dovrebbe essere neutra e muovi il cursore finché resta grigia. Alterna Temp e Tint un paio di volte; converge. Ogni vista reagisce solo al proprio cursore.

![Falso colore della temperatura](images/11-falsecolor-temp.png)

Approfondimento: [docs/FALSE_COLOR.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/FALSE_COLOR.md)

---

## 7. Toni e Zone

### Toni

![Gruppo Toni](images/12-toni.png)

Tutti i cursori vanno da −100 a +100, nell'ordine di Camera Raw. Highlights è una spalla filmica; gli altri sono **esposizioni su una fascia di toni, in stop dal grigio 18%**: dentro la zona l'immagine si muove come con un'esposizione, quindi la texture resta. Nessuna combinazione di cursori può solarizzare.

| Cursore | Comportamento |
|---|---|
| **Contrast** | Gira attorno al Pivot. +100 raddoppia la pendenza al pivot, −100 la dimezza, con gli estremi limitati. |
| **Highlights** | Negativo: spalla filmica — a −100 il valore massimo registrato (~+6 stop sopra il grigio in S-Log3) arriva esattamente su **Bianco**, niente velo grigio, niente clip. Il grigio e i toni sotto restano fermi; la pelle a +1 stop si sposta al massimo di 0,05 stop. La tinta resta costante. Positivo: più stacco. |
| **Bianco** (stop) | Dove arriva quel massimo, e il tetto di Soft Clip. **2,5** = bianco Rec.709 attraverso un CST senza tone mapping. **Con un DRT dopo (ACES, AgX, DaVinci) alzalo a 4–5**, altrimenti le alte luci vengono compresse due volte. |
| **Shadows** | Toni sotto −1 stop; 100 = 2 stop. Alza anche il nero — tienilo fermo con Blacks. |
| **Whites** | Da +3,5 stop al clip; 100 = 1 stop. |
| **Blacks** | Velo lineare: sposta il nero (−3 / +1 stop) senza muovere il grigio. |
| **Vibrance** | Attorno alla luminanza; protegge gli incarnati. |
| **Saturation** | Attorno alla luminanza; uguale in ogni spazio colore. |

**Azzera toni** azzera il gruppo.

![Highlights 0 contro −100](images/15-highlights-before-after.png)

![Curva dei toni](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/tone_curve.png)

> **Il costo onesto di un nodo puntuale:** quello che comprime, lo comprime anche nella texture che contiene.

### Zone

![Gruppo Zone](images/13-zone.png)

- Quattro zone, come nella palette HDR di Resolve: **Black, Shadow, Light, Specular** — ciascuna con **Exp** (stop), **Sat**, **Range** (bordo, in stop) e **Falloff** (ampiezza della transizione).
- **Contrast Pivot** — il tono attorno a cui gira Contrast.
- **Soft Clip** / **Soft Clip Color** — ripiega le alte luci sotto un tetto (a Bianco) che non raggiungono mai. *Color* decide se le alte luci ripiegate vanno verso il bianco (pellicola) o mantengono il colore. Contenimento, non recupero.
- **Zone false colour** — colora ogni pixel con la zona che lo muove.
- **Azzera zone** azzera il gruppo.

I bordi delle zone sono portati attraverso Contrast, quindi restano in stop di scena qualunque sia il Contrast che usi.

### Generate LUT

Il nodo è puntuale, quindi Generate LUT lo include. Usa **65 punti**: con i cursori a ±100 l'errore resta entro ~3,5 code value S-Log3. A 33 punti il toe lineare dell'S-Log3 (Shadows +100) arriva a ~10 CV. Alcune Zone estreme (Black o Shadow +3) superano questo limite anche a 65 punti — per quei grade tieni il nodo attivo.

Approfondimento: [docs/TONE_MAPPING.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/TONE_MAPPING.md)

---

## 8. Il nodo Log MetaRaw Creative

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
  Action 4 e 5 Pro. Può anche misurare il rumore da un fotogramma con zone piatte.
  - *Temporale*: da ±1 a ±3 fotogrammi vicini, allineati a blocchi e pesati pixel per pixel, così le parti in movimento
    e i tagli non si impastano.
  - *Spaziale*: luminanza e colore separati, in 5 bande wavelet. Ogni coefficiente è confrontato con l'energia del suo
    vicinato, che tiene la trama.
  - Viste del rumore rimosso, protezione di dettaglio e pelle, e **Salta Noise ovunque** per spegnere tutti i nodi
    Noise mentre monti.
  - Su Mac gira tutto su GPU Metal, e i test verificano che dia lo stesso risultato della CPU. Altrove gira su CPU.
  - I profili sono stime finché non vengono tarati su campioni reali. [docs/DENOISE.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DENOISE.md)
- **Stabilize** legge il giroscopio della clip (Gyroflow integrato). Ha stili dal micromosso al crane e un **MASTER**
  animabile. **Motion Blur** ricostruisce l'otturatore dal flusso ottico, per esempio da 50p a 90° a 25p a 180°.
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

## 9. Pipeline: dove va ogni nodo

### A — Grade in DaVinci Wide Gamut (consigliata)

```
Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → il tuo grade (DWG)
  → CST di uscita       DaVinci WG/Intermediate → Rec.709 · Gamma 2.4, DaVinci tone mapping
```

Con un tone mapping dopo, porta **Bianco a 4–5**. **Creative**, se lo usi, va subito dopo Log MetaRaw ed esce in Rec.709 · Gamma 2.4 (o nella codifica d'ingresso con *Uscita Log*): in un grade DWG mettilo per ultimo, al posto del CST d'uscita.

### B — Tutto in log

```
Log MetaRaw          Color Space/Gamma: Timeline
  → il tuo grade
  → CST / LUT          S-Gamut3.Cine/S-Log3 → la tua uscita
```

### C — Progetto color-managed (DaVinci YRGB Color Managed / ACES)

Lascia Color Space/Gamma su **Timeline** nel nodo principale. Attiva l'opzione dello script *Imposta anche Input Color Space* solo se vuoi che lo script imposti l'ingresso di ogni clip — ricorda che dovrai riportarlo su *Project* a mano.

---

## 10. Ricette

| Problema | Dove | Impostazione |
|---|---|---|
| Cielo o finestre che bruciano | Main › Toni | Highlights −50…−100 (con un DRT dopo: Bianco 4–5) |
| Volto in controluce nell'ombra | Main › Toni | Shadows +30…+60, Blacks −20…−40 |
| Look più "filmico" | Main › Toni | Contrast +20…+30, Highlights −60 |
| Ombre sature e rumorose | Main › Zone | Shadow Sat −30…−50 |
| Riflessi speculari | Main › Zone | Specular Exp −1…−2 |

---

## 11. Casi speciali

**ProRes da un registratore esterno / clip illeggibile.** Il nodo resta neutro. Spunta **Spazio colore › Senza metadata › Sblocca controlli** e scrivi EI, Kelvin e tinta di ripresa: i controlli si attivano e il nodo parte neutro.

**Profili non log** (Cine, HLG, S-Cinetone): i nodi restano neutri e lo dicono in *Stato*.

**Data Level sbagliato.** Se i neri appaiono alzati o schiacciati appena fuori dalla camera, Resolve sta decodificando sulla scala di code value sbagliata. Esegui lo script con *Correggi il Data Level* attivo (correzione per tutto il progetto), oppure usa *Spazio colore › Ingresso › Data level* su una singola clip. Approfondimento: [docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**Camere senza Kelvin** (ad es. a6300): il Kelvin è stimato dal preset di luce e segnalato.

**Prestazioni.** Aprire un progetto non legge nessun file. Aprire il pannello aspetta al massimo mezzo secondo; un disco lento finisce in background con un limite di 15 s.

---

## 12. Aggiornamenti e privacy

- I **nodi** chiedono a GitHub l'ultima release al massimo una volta al giorno, in background. La richiesta porta solo la versione (`User-Agent: SLogMetaRaw/3.0.0`).
- Lo **script** controlla solo quando clicchi la sua versione.
- Un clic apre solo un link di download dalle release GitHub di questo progetto. Niente viene installato senza di te.

![Badge di aggiornamento](images/18-update-badge.png)

**Disattiva il controllo:** crea il file vuoto

```bash
touch ~/Library/Application\ Support/SLogMetaRaw/no_update_check
```

**Aggiornamento dalla 1.x:** i toni della 1.x (Highlights, Shadows, Contrast, Saturation, Color Boost, Color Recovery) non possono essere convertiti e vengono azzerati. Per mantenere il look di una clip già finita, **prima di aggiornare** esegui Generate LUT su quel nodo. Dopo l'aggiornamento, *Stato* mostra cosa c'era (ad es. "Toni 1.1 azzerati (H −40, C +15)"). Exposure, data level, Color Space e Gamma restano invariati.

---

## 13. Disinstallazione

Fai doppio clic su **Disinstalla Log MetaRaw.command** nel DMG (la prima volta: tasto destro › Apri). Lo strumento:

1. elenca tutto quello che rimuoverà (tutte le versioni 1.x e 2.x, il vecchio nome *SonyMeta*, le installazioni di sviluppo, cache, impostazioni, log);
2. chiede di chiudere Resolve;
3. chiede se cancellare anche i CSV esportati;
4. verifica che non resti nulla.

Eseguilo con `--dry-run` per vedere cosa rimuoverebbe senza toccare nulla. I metadata già scritti nei progetti Resolve restano — fanno parte dei progetti.

![Disinstallatore nel Terminale](images/20-uninstaller.png)

---

## 14. Risoluzione problemi / FAQ

**Il nodo Creative non c'è / il pannello sembra vecchio.**
Resolve sta usando la vecchia cache dei plugin. Chiudi Resolve, elimina `OFXPluginCacheV2.xml` (di solito lo fa già l'installer), riavvia.

**Il nodo non fa nulla.**
È normale ai valori di ripresa. Controlla che *Decodifica* sia su **Clip**, e leggi *Spazio colore › Ingresso › Stato*: dice se il profilo non è log o se manca il metadato.

**Il mio render ha il falso colore dentro.**
È rimasta attiva una vista. Spegni tutte le viste falso colore prima di renderizzare.

**Le alte luci sembrano compresse due volte / spente.**
Hai un DRT (ACES, AgX, DaVinci tone mapping) dopo il nodo: alza **Bianco** a 4–5.

**Lo script ha saltato una clip.**
Leggi la colonna *Stato*: dice il perché (file non supportato, disco che non risponde, ecc.).

**"Camera Aperture F53343" sulla FX6.**
Una lettura errata di Resolve sull'MXF; **2 · Scrivi in Resolve** la corregge.

**macOS non apre l'installer.**
Non è firmato: tasto destro › Apri (oppure Impostazioni di Sistema › Privacy e sicurezza › Apri comunque).

---

## 15. Linux e Windows (sperimentale)

Il plugin si compila anche per Linux e Windows con CMake. Queste build renderizzano solo su **CPU** (ancora senza
Metal, CUDA o OpenCL) e non sono state testate dentro DaVinci Resolve: la build supportata resta quella macOS.

**Linux** (scarica da [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases); scegli quello adatto alla tua distribuzione):

| File | Per |
|---|---|
| `slogmetaraw_<versione>_amd64.deb` | Ubuntu, Debian, Mint: `sudo apt install ./slogmetaraw_*.deb` |
| `slogmetaraw-<versione>-1.x86_64.rpm` | Rocky, Alma, RHEL, CentOS, Fedora: `sudo dnf install ./slogmetaraw-*.rpm` |
| `SLogMetaRaw-<versione>-linux-x86_64.run` | qualsiasi distribuzione: `sh SLogMetaRaw-*.run` (`--user` senza sudo, `--uninstall`) |
| `SLogMetaRaw-<versione>-linux-x86_64.tar.gz` | lo stesso, scompattato: esegui `install.sh` dentro |

Il plugin va in `/usr/OFX/Plugins`; la libreria Python in `/usr/lib/slogmetaraw` (.deb/.rpm) o
`~/.local/share/SLogMetaRaw` (.run, .tar.gz). Riavvia Resolve. Basta Python 3.6+. Il binario è compilato contro
glibc 2.28, quindi si carica su Rocky/Alma/RHEL 8+, Ubuntu 20.04+ e Debian 10+.

**Windows:** compilalo tu con `powershell -File packaging\windows\build_package.ps1` (cmake, Visual Studio
Build Tools, python; Inno Setup è facoltativo per l'`.exe`). Lo stato vive in `%APPDATA%\SLogMetaRaw`.

Un tag `v*` pubblicato sul repository fa sì che la CI attacchi automaticamente i pacchetti Linux (e, con un
tentativo migliore, quello Windows) a una release GitHub in **bozza** — vedi [Limiti noti](#16-limiti-noti) per
quello che manca ancora (accelerazione GPU, una pipeline immagine testata ed equivalente a quella macOS).

---

## 16. Limiti noti

- Il pannello Camera Raw di Resolve e la stabilizzazione giroscopica non si possono sbloccare per gli MP4: vivono dentro i decoder di Resolve. Log MetaRaw ricostruisce i controlli del colore, non apre una porta dentro Resolve.
- S-Log2 segue il documento Sony; la curva S-Log2 di Resolve differisce di circa 0,15 stop.
- Ancora da verificare su più file: XAVC HS (HEVC), HLG, S-Cinetone, zoom motorizzati.
- Testato solo su Resolve Studio 21.1 su macOS. Le build Linux e Windows sono solo CPU e non testate dentro Resolve (vedi [§15](#15-linux-e-windows-sperimentale)).
- Progetto hobbistico e indipendente, fornito così com'è, senza garanzie e senza responsabilità per l'uso professionale. Non affiliato né approvato da Sony o Blackmagic Design.

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0+](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [Note di rilascio](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)

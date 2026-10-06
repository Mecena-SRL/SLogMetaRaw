# S-Log MetaRaw — Guida utente

[English](Wiki-Software) · **Italiano** · [Español](Wiki-Software-es) · [Português](Wiki-Software-pt) · [简体中文](Wiki-Software-zh)

> Versione **2.3.0** · macOS 12+ · DaVinci Resolve 20 / 21 · Sony XAVC `.MP4` / `.MXF`
> Testato solo su DaVinci Resolve Studio 21.1 su macOS.

S-Log MetaRaw legge i dati di ripresa che le camere Sony scrivono in ogni file (Kelvin, tinta, EI, obiettivo, diaframma, shutter, profilo colore) e li usa dentro DaVinci Resolve. Resolve lo fa solo per i file MXF di FX6/FX9; per gli MP4 di FX30, FX3, serie a7 e a6000 li ignora.

**Indice**

1. [Cos'è — e cosa non è](#1-cosè--e-cosa-non-è)
2. [Installazione](#2-installazione)
3. [Avvio rapido](#3-avvio-rapido)
4. [Lo script](#4-lo-script)
5. [Il nodo S-Log MetaRaw](#5-il-nodo-s-log-metaraw)
6. [False color](#6-false-color)
7. [Toni e Zone](#7-toni-e-zone)
8. [Il nodo S-Log MetaRaw Detail](#8-il-nodo-s-log-metaraw-detail)
9. [Pipeline: dove va ogni nodo](#9-pipeline-dove-va-ogni-nodo)
10. [Ricette](#10-ricette)
11. [Casi speciali](#11-casi-speciali)
12. [Aggiornamenti e privacy](#12-aggiornamenti-e-privacy)
13. [Disinstallazione](#13-disinstallazione)
14. [Risoluzione problemi / FAQ](#14-risoluzione-problemi--faq)
15. [Limiti noti](#15-limiti-noti)

---

## 1. Cos'è — e cosa non è

Tre strumenti, una sola installazione:

| Strumento | Dove, in Resolve | Cosa fa |
|---|---|---|
| **Script** | Workspace › Scripts › S-Log MetaRaw | Legge i metadata di ogni clip e li scrive nel Media Pool. Corregge il Data Level di ogni clip. |
| Nodo **S-Log MetaRaw** | Color › OpenFX — **primo nodo** | Sviluppa una clip partendo dai valori di ripresa: bilanciamento del bianco, esposizione, spazio colore, toni per zone, false color. |
| Nodo **S-Log MetaRaw Detail** | Color › OpenFX — **subito dopo** | Il nodo creativo: recupero locale di alte luci/ombre, Texture, Clarity, Dehaze. |

I file originali **non vengono mai modificati**: nessuna transcodifica, nessun rewrap.

**Non è raw.** Un MP4 log è già demosaicizzato e compresso (8 o 10 bit, spesso 4:2:0, con la riduzione rumore della camera già applicata). Il nodo applica la scienza colore con rigore — esposizione e bilanciamento del bianco in luce lineare, partendo dai valori registrati, con le curve e i gamut pubblicati da Sony — così l'immagine *si comporta* in un modo che ricorda il raw: il bilanciamento del bianco si sposta pulito, l'esposizione si muove come uno stop di luce, le alte luci si arrotondano invece di rompersi. Spingi oltre i limiti della camera e l'assenza di informazioni si fa sentire: banding nei cieli, rumore nelle ombre alzate, le alte luci bruciate restano bruciate. Esponi bene in ripresa.

---

## 2. Installazione

1. Scarica `SLogMetaRaw-2.3.0.dmg` da [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) e aprilo.

   ![La finestra del DMG](images/01-dmg.png)

2. Doppio clic su **Installa S-Log MetaRaw.pkg**. Il pacchetto non è firmato con un certificato Apple: la prima volta, **tasto destro › Apri**. Chiede la password del Mac perché il plugin va in una cartella di sistema.

   ![Tasto destro › Apri sul pacchetto non firmato](images/02-gatekeeper.png)

3. **Riavvia DaVinci Resolve.**

L'installer cancella anche la cache dei plugin di Resolve (`OFXPluginCacheV2.xml`); Resolve la ricostruisce al prossimo avvio. Senza, Resolve mostrerebbe ancora il pannello vecchio e non vedrebbe il nodo Detail.

Ogni installazione parte pulita: il plugin e la libreria precedenti vengono sostituiti per intero, e le installazioni di sviluppo vengono rimosse.

**Cosa va dove**

| Elemento | Percorso |
|---|---|
| I due nodi (un solo bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Libreria Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Script del menu | `…/DaVinci Resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py` |
| Cache per clip (JSON) | `~/Library/Application Support/SLogMetaRaw/cache` |

**Requisiti:** macOS 12 o successivo, Apple silicon o Intel, DaVinci Resolve 21 o 20 (Resolve 20: installa Python 3 da python.org, non ne ha uno proprio).
**Clip:** Sony XAVC `.MP4` o `.MXF` — lo script le legge tutte. I nodi sviluppano **S-Log3** (S-Gamut3.Cine o S-Gamut3), **S-Log2** e **S-Log** (S-Gamut). Con altri profili (Cine, HLG, S-Cinetone) restano neutri e lo dicono.

---

## 3. Avvio rapido

1. Importa il girato. Apri **Workspace › Scripts › S-Log MetaRaw**, premi **1 · Leggi metadata**, poi **2 · Scrivi in Resolve**.
2. Nella pagina Color, aggiungi **S-Log MetaRaw** come **primo nodo**. Prende l'EI, il Kelvin e la tinta della clip: a quei valori non cambia niente.
3. Correggi il bilanciamento del bianco e l'esposizione nel nodo, usando le viste false color.
4. Modella i toni con **Toni**. Per il recupero locale, Texture, Clarity o Dehaze, aggiungi **S-Log MetaRaw Detail** come nodo successivo.
5. Poi il resto del grade e, per ultimo, il tuo CST / LUT / DRT di uscita.

```
S-Log MetaRaw  →  S-Log MetaRaw Detail  →  resto del grade  →  CST / LUT / DRT di uscita
```

![Albero dei nodi consigliato](images/08-node-tree.png)

> Copia il nodo su un'altra clip e si reimposta da solo con i dati di quella clip.

---

## 4. Lo script

**Workspace › Scripts › S-Log MetaRaw**

![Menu Workspace › Scripts](images/03-scripts-menu.png)

La finestra ha una riga di pulsanti, una riga di opzioni e l'elenco delle clip. Segue la lingua di Resolve (inglese, italiano, spagnolo, portoghese, cinese semplificato). Lo script legge i file; non li modifica mai.

![Finestra dello script dopo Leggi metadata](images/04-script-window.png)

### Pulsanti

| Controllo | Cosa fa |
|---|---|
| **Menu Clip** | *Tutto il Media Pool* o *Clip selezionate nel Media Pool*. |
| **1 · Leggi metadata** | Una riga per clip: camera, obiettivo, diaframma, shutter, EI, WB, spazio colore, data level. La colonna **Stato** dice `letto`, cosa è cambiato durante la ripresa (diaframma, fuoco…), o perché una clip è stata saltata. Clicca una riga per vedere tutto quello che è stato letto, raggruppato come in Catalyst Browse. |
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
| **Imposta anche Input Color Space** | spento | Per i progetti color managed. ⚠️ Uno script non può riportarlo su *Project* — solo tu, a mano. |

Dopo **2 · Scrivi in Resolve**, i dati compaiono nel Media Pool:

![Pannello Metadata del Media Pool compilato](images/06-media-pool-metadata.png)

**Velocità.** Niente viene decodificato: al massimo 24 campioni della traccia metadata per clip, entro un secondo. Un file di più gigabyte costa circa 100 KB di lettura. Un disco che smette di rispondere viene saltato una volta sola, con un messaggio, invece di bloccare l'elenco. Eseguire lo script subito dopo l'importazione fa sì che i nodi trovino i dati già pronti.

---

## 5. Il nodo S-Log MetaRaw

**Color › OpenFX › S-Log MetaRaw** — primo nodo, prima di ogni CST o LUT.

![Libreria OpenFX con entrambi i nodi](images/07-openfx-library.png)

Il nodo è **puntuale**: ogni pixel dipende solo da sé stesso. Non crea mai aloni, e **Generate LUT** lo può esportare (consigliati 65 punti). Le impostazioni sono salvate per clip.

![Pannello del nodo principale — in alto](images/09-node-panel-top.png)

| Controllo | Cosa fa |
|---|---|
| **Versione** (in cima) | Mostra `v2.3.0`. Una volta al giorno chiede a GitHub l'ultima release; se ce n'è una, mostra **🟢 v2.3.0 → 2.x.y** e un clic apre il download dell'installer per il tuo sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Non installa mai niente da solo. |
| **Camera** · **Rileggi metadata** | La camera che è stata letta. *Rileggi* rilegge la clip, riporta ogni controllo ai valori di camera e scrive i metadata della clip nel Media Pool. Risponde entro ~2 s. |
| **Decode Using** | *Clip* permette di cambiare i controlli; *Camera metadata* li blocca sui valori di ripresa (il nodo è trasparente). |
| **White Balance** | As shot, oppure preset (Daylight, Cloudy, Shade, Tungsten, Fluorescent, Flash). Muovere un cursore passa a *Custom*. |
| **Color Temp** · **Tint** | Adattamento cromatico Bradford in luce lineare, dal bianco registrato dalla camera. Temp più alta scalda; Tint positivo va verso il magenta. |
| **Exposure** | In EI: EI doppio = esattamente +1 stop, in luce lineare, prima di ogni curva. |
| **False color** | Viste di temperatura, tinta ed esposizione — vedi [§6](#6-false-color). |
| **Color Space** · **Gamma** | Uscita, come un Color Space Transform. *Timeline* non converte — lascialo lì in un progetto color managed. Per gradare in DWG: *DaVinci WG · DaVinci Intermediate*. |
| **Toni** | Contrast, Highlights, Shadows, Whites, Bianco, Blacks, Vibrance, Saturation — vedi [§7](#7-toni-e-zone). |
| **Zone** (chiuso) | Zone Black, Shadow, Light, Specular; Contrast Pivot; Soft Clip. |
| **Avanzate** | Ingresso del nodo, correzione del data level, stato, *Sblocca controlli senza metadata*. |
| **Dati di ripresa** | Sola lettura: obiettivo, focale, diaframma, fuoco, shutter, EI, WB, fps, ND, LUT di camera. |

![Avanzate e Dati di ripresa](images/14-avanzate-dati.png)

**Avanzate, nel dettaglio**

- **Ingresso nodo** — *Automatico* chiede a Resolve.
- **Data level in ingresso** — corregge la scala di code value quando Resolve decodifica una clip su quella sbagliata. *Correggi il Data Level* dello script la corregge per tutto il progetto.
- **Stato** — quello che il nodo ha rilevato; avvisa anche quando una vista false color è accesa.
- **Sblocca controlli senza metadata** — vedi [§11](#11-casi-speciali).

> **L'uscita Rec.709** nel nodo è un CST *senza* tone mapping (le alte luci oltre Bianco si clippano) ed esclude il nodo Detail. Preferisci un CST di uscita alla fine dell'albero.

---

## 6. False color

Una vista per controllo, posta sopra il cursore a cui serve. **La vista sostituisce l'immagine — spegnila prima di renderizzare.** (Un plugin OpenFX non può disegnare un overlay sul viewer di Resolve.)

Imposta prima *Decode Using* su **Clip**: in *Camera metadata* i controlli sono bloccati.

### Exposure

Bande in stile ARRI, in stop, attorno al grigio 18%. Tutto il resto diventa grigio.

![Bande di esposizione](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/falsecolor_bands.png)

| Colore | Significato |
|---|---|
| Verde | Grigio medio (18%) |
| Rosa | Uno stop sopra — incarnato |
| Giallo | Vicino al clip |
| Rosso | Clippato |
| Blu / viola | Ombra profonda / nero |

Accendi la vista, inquadra il grigio medio o l'incarnato, muovi **Exposure** finché l'area giusta diventa verde (grigio) o rosa (incarnato).

![False color di esposizione nel viewer](images/10-falsecolor-exposure.png)

### Temperatura e Tint

Funzionano come in CineMatch. L'immagine diventa grigia; le dominanti si mostrano a colori, e quelle quasi neutre sono esaltate fino a 8× perché siano visibili.

| Vista | Vedi | Fai |
|---|---|---|
| Temperatura | Blu (dominante fredda) | Alza Color Temp |
| Temperatura | Arancio (dominante calda) | Abbassa Color Temp |
| Tint | Verde | Alza Tint |
| Tint | Magenta | Abbassa Tint |

Scegli una superficie che dovrebbe essere neutra e muovi il cursore finché resta grigia. Alterna Temp e Tint un paio di volte; converge. Ogni vista reagisce solo al proprio cursore.

![False color di temperatura](images/11-falsecolor-temp.png)

Approfondimento: [docs/FALSE_COLOR.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/FALSE_COLOR.md)

---

## 7. Toni e Zone

### Toni

![Gruppo Toni](images/12-toni.png)

Tutti i cursori vanno da −100 a +100, nell'ordine di Camera Raw. Highlights è una spalla filmica; gli altri sono **esposizioni su una fascia di toni, in stop dal grigio 18%**: dentro la fascia l'immagine si muove come con un'esposizione, quindi la texture resta. Nessuna combinazione di cursori può solarizzare.

| Cursore | Comportamento |
|---|---|
| **Contrast** | Ruota attorno al Pivot. +100 raddoppia la pendenza al pivot, −100 la dimezza, con estremi limitati. |
| **Highlights** | Negativo: spalla filmica — a −100 il valore massimo registrato (~+6 stop sopra il grigio in S-Log3) arriva esattamente su **Bianco**, senza velo grigio, senza clip. Il grigio e sotto restano fermi; l'incarnato a +1 stop si sposta al massimo di 0,05 stop. La tinta resta costante. Positivo: più stacco. |
| **Bianco** (stop) | Dove arriva quel massimo, ed è anche il tetto di Soft Clip. **2,5** = bianco Rec.709 attraverso un CST senza tone mapping. **Con un DRT dopo (ACES, AgX, DaVinci) alzalo a 4–5**, altrimenti le alte luci vengono compresse due volte. |
| **Shadows** | Toni sotto −1 stop; 100 = 2 stop. Alza anche il nero — tienilo fermo con Blacks. |
| **Whites** | Da +3,5 stop fino al clip; 100 = 1 stop. |
| **Blacks** | Velo lineare: sposta il nero (−3 / +1 stop) senza muovere il grigio. |
| **Vibrance** | Attorno alla luminanza; protegge gli incarnati. |
| **Saturation** | Attorno alla luminanza; uguale in ogni spazio colore. |

**Azzera toni** azzera il gruppo.

![Highlights 0 contro −100](images/15-highlights-before-after.png)

![Curva dei toni](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/tone_curve.png)

> **Il costo onesto di un nodo puntuale:** qualunque cosa comprima, comprime anche la texture al suo interno. Il recupero che mantiene la texture vive nel nodo Detail.

### Zone

![Gruppo Zone](images/13-zone.png)

- Quattro zone, come nella palette HDR di Resolve: **Black, Shadow, Light, Specular** — ognuna con **Exp** (stop), **Sat**, **Range** (bordo, in stop) e **Falloff** (ampiezza della transizione).
- **Contrast Pivot** — il tono attorno a cui ruota Contrast.
- **Soft Clip** / **Soft Clip Color** — piega le alte luci sotto un tetto (a Bianco) che non raggiungono mai. *Color* decide se le alte luci piegate vanno verso il bianco (pellicola) o mantengono il colore. Contenimento, non recupero.
- **False color Zone** — colora ogni pixel con la zona che lo muove.
- **Azzera zone** azzera il gruppo.

I bordi delle zone sono portati attraverso Contrast, quindi restano in stop di scena qualunque Contrast tu usi.

### Generate LUT

Il nodo è puntuale, quindi Generate LUT lo include. Usa **65 punti**: con i cursori a ±100 l'errore resta entro ~3,5 code value S-Log3. A 33 punti il toe lineare di S-Log3 (Shadows +100) arriva a ~10 CV. Alcune Zone estreme (Black o Shadow +3) superano questo limite anche a 65 — per quei grade tieni il nodo attivo.

Approfondimento: [docs/TONE_MAPPING.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/TONE_MAPPING.md)

---

## 8. Il nodo S-Log MetaRaw Detail

**Color › OpenFX › S-Log MetaRaw Detail** — subito dopo S-Log MetaRaw, prima di ogni CST / LUT / DRT.

Lavora **per aree**, su una base che rispetta i bordi: sposta grandi aree di luce senza appiattire il dettaglio fine — la parte di Highlights/Shadows di Lightroom che un nodo puntuale non può fare.

![Pannello del nodo Detail](images/16-detail-panel.png)

| Gruppo | Controlli |
|---|---|
| **Gamma dinamica** | Local Contrast, Local Highlights, Local Shadows; viste di controllo *gain* e *base*. |
| **Presenza** | **Texture** (dettaglio fine; non alza la grana sotto la soglia rumore), **Clarity** (contrasto locale a media scala), **Dehaze**. |
| **Zone locali** | Le zone del nodo principale, applicate alle aree invece che ai pixel. |
| **Avanzate** | Preservazione del dettaglio, raggio, *Soglia bordi*, soglia rumore, centro di Clarity, **Bianco** di Local Highlights, ingresso del nodo. |
| **Velo** | Livello e colore del velo che Dehaze toglie. Li imposti tu; mai stimati per fotogramma → niente flicker. |

**Azzera dettaglio** azzera il nodo.

**Local Highlights contro Highlights.** Local Highlights comprime le grandi aree luminose e mantiene — anzi rinforza — la texture fine: il cielo si scurisce e le nuvole restano dettagliate. L'Highlights del nodo principale invece ammorbidisce la texture delle alte luci, come la pellicola. Local Highlights ha il suo **Bianco** in Avanzate e non conosce l'EI scelto nel nodo principale: se cambi molto l'EI, regolalo.

![Local Highlights su un cielo, prima/dopo](images/17-detail-before-after.png)

**Regole**

- Decodifica in luce lineare quello che riceve e lo riscrive nella **stessa codifica**. Accetta solo codifiche scene-log: S-Log3, S-Log2, DaVinci WG/Intermediate, ACEScct. **Non** Rec.709, Gamma 2.4 o sRGB.
- È **spaziale**: Generate LUT lo esclude, insieme al resto del suo nodo. Tienilo in un nodo tutto suo.
- I raggi seguono l'altezza del fotogramma: stesso look a piena risoluzione, in proxy e nel viewer. Nessuna statistica per fotogramma → niente flicker.
- Metal: ~6–18 ms per fotogramma UHD su Apple silicon. Il ripiego su CPU è molto più lento.

**Limiti misurati**

- Local Highlights −100: alone sul lato scuro di un bordo < 3% del gradino.
- Local Shadows o le zone locali a ±100: ~12% su un bordo netto di 1 stop, 4–6% su bordi di 2–3 stop. Se lo vedi, abbassa *Soglia bordi*.
- Texture vicino a bordi forti può far crescere la grana di 1,25–1,7×.
- Dehaze ha bisogno di un velo vero da togliere.

Approfondimento: [docs/DETAIL.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DETAIL.md)

---

## 9. Pipeline: dove va ogni nodo

### A — Grade in DaVinci Wide Gamut (consigliato)

```
S-Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → Detail             Ingresso nodo: DaVinci WG/Intermediate
  → il tuo grade (DWG)
  → CST di uscita       DaVinci WG/Intermediate → Rec.709 · Gamma 2.4, DaVinci tone mapping
```

Con il tone mapping dopo, imposta **Bianco a 4–5 in entrambi i nodi**. Su una timeline non color-managed devi dichiarare l'ingresso del nodo Detail.

### B — Tutto in log

```
S-Log MetaRaw          Color Space/Gamma: Timeline
  → Detail             Ingresso nodo: Automatico (usa l'S-Log3 della clip)
  → il tuo grade
  → CST / LUT          S-Gamut3.Cine/S-Log3 → la tua uscita
```

### C — Progetto color managed (DaVinci YRGB Color Managed / ACES)

Lascia Color Space/Gamma su **Timeline** nel nodo principale. Attiva *Imposta anche Input Color Space* dello script solo se vuoi che imposti l'input di ogni clip — ricorda che devi riportarlo su *Project* a mano.

---

## 10. Ricette

| Problema | Dove | Impostazione |
|---|---|---|
| Cielo o finestre bruciate | Main › Toni | Highlights −50…−100 (con un DRT dopo: Bianco 4–5) |
| …e mantenere la texture delle nuvole | Detail › Gamma dinamica | Local Highlights |
| Viso in controluce, in ombra | Main › Toni | Shadows +30…+60, Blacks −20…−40 |
| Look più "pellicola" | Main › Toni | Contrast +20…+30, Highlights −60 |
| Ombre sature e rumorose | Main › Zone | Shadow Sat −30…−50 |
| Riflessi speculari | Main › Zone | Specular Exp −1…−2 |
| Paesaggio piatto e con foschia | Detail › Presenza + Velo | Dehaze; imposta a mano livello/colore del velo |

---

## 11. Casi speciali

**ProRes da un registratore esterno / clip illeggibile.** Il nodo resta neutro. Spunta **Avanzate › Sblocca controlli senza metadata** e scrivi l'EI, il Kelvin e la tinta di ripresa: i controlli si attivano e il nodo parte neutro.

**Profili non log** (Cine, HLG, S-Cinetone): i nodi restano neutri e lo dicono in *Stato*.

**Data Level sbagliato.** Se i neri sembrano alzati o schiacciati già dalla camera, Resolve sta decodificando sulla scala di code value sbagliata. Esegui lo script con *Correggi il Data Level* acceso (correzione per tutto il progetto), oppure usa *Avanzate › Data level in ingresso* su una singola clip. Approfondimento: [docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**Camere senza Kelvin** (ad es. l'a6300): il Kelvin è stimato dal preset di luce e segnalato.

**Prestazioni.** Aprire un progetto non legge nessun file. Aprire il pannello aspetta al massimo mezzo secondo; un disco lento finisce in background con un limite di 15 s.

---

## 12. Aggiornamenti e privacy

- I **nodi** chiedono a GitHub l'ultima release al massimo una volta al giorno, in background. La richiesta porta solo la versione (`User-Agent: SLogMetaRaw/2.3.0`).
- Lo **script** controlla solo quando clicchi la sua versione.
- Un clic apre solo un link di download dalle release GitHub di questo progetto. Niente viene installato senza di te.

![Badge di aggiornamento](images/18-update-badge.png)

**Per disattivare il controllo:** crea il file vuoto

```bash
touch ~/Library/Application\ Support/SLogMetaRaw/no_update_check
```

**Aggiornamento dalla 1.x:** i toni della 1.x (Highlights, Shadows, Contrast, Saturation, Color Boost, Color Recovery) non si possono convertire e vengono azzerati. Per mantenere il look di una clip finita, **prima di aggiornare** esegui Generate LUT su quel nodo. Dopo l'aggiornamento, *Stato* mostra cosa c'era (ad es. "Toni 1.1 azzerati (H −40, C +15)"). Exposure, data level, Color Space e Gamma restano invariati.

---

## 13. Disinstallazione

Doppio clic su **Disinstalla S-Log MetaRaw.command** nel DMG (la prima volta: tasto destro › Apri). Fa questo:

1. elenca tutto quello che rimuoverà (tutte le versioni 1.x e 2.x, il vecchio nome *SonyMeta*, le installazioni di sviluppo, cache, impostazioni, log);
2. ti chiede di chiudere Resolve;
3. ti chiede se cancellare anche i CSV esportati;
4. controlla che non resti nulla.

Eseguilo con `--dry-run` per vedere cosa rimuoverebbe senza toccare nulla. I metadata già scritti nei progetti di Resolve restano — fanno parte dei progetti.

![Disinstallatore nel Terminale](images/20-uninstaller.png)

---

## 14. Risoluzione problemi / FAQ

**Il nodo Detail manca / il pannello sembra vecchio.**
Resolve sta usando la sua vecchia cache dei plugin. Chiudi Resolve, cancella `OFXPluginCacheV2.xml` (di norma lo fa l'installer), riavvia.

**Il nodo non fa niente.**
È voluto, ai valori di ripresa. Controlla che *Decode Using* sia su **Clip**, e leggi *Avanzate › Stato*: dice se il profilo non è log o se mancano i metadata.

**Nel render c'è il false color.**
Una vista è rimasta accesa. Spegni tutte le viste false color prima di renderizzare.

**Le alte luci sembrano compresse due volte / spente.**
Hai un DRT (ACES, AgX, DaVinci tone mapping) dopo il nodo: alza **Bianco** a 4–5 (anche nel nodo Detail).

**Aloni attorno ai bordi con il nodo Detail.**
Abbassa *Avanzate › Soglia bordi*, oppure riduci Local Shadows / le zone locali.

**Lo script ha saltato una clip.**
Leggi la colonna *Stato*: dice il perché (file non supportato, disco che non risponde, ecc.).

**"Camera Aperture F53343" sulla FX6.**
Una lettura errata di Resolve dagli MXF; **2 · Scrivi in Resolve** la corregge.

**macOS non apre l'installer.**
Non è firmato: tasto destro › Apri (oppure Impostazioni di Sistema › Privacy e Sicurezza › Apri comunque).

---

## 15. Limiti noti

- Il pannello Camera Raw di Resolve e la stabilizzazione giroscopica non si possono sbloccare per gli MP4: vivono dentro i decoder di Resolve. S-Log MetaRaw ricostruisce i controlli del colore, non apre una porta dentro Resolve.
- S-Log2 segue il documento Sony; la curva S-Log2 di Resolve differisce di ~0,15 stop.
- Ancora da verificare su più file: XAVC HS (HEVC), HLG, S-Cinetone, zoom motorizzati.
- Testato solo su Resolve Studio 21.1 su macOS.
- Progetto hobbistico e indipendente, fornito così com'è, senza garanzie e senza responsabilità per l'uso professionale. Non affiliato né approvato da Sony o Blackmagic Design.

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0+](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [Note di rilascio](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)

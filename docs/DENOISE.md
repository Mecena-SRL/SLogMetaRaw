# Log MetaRaw Noise (riduzione rumore)

Terzo nodo del pacchetto. Toglie il rumore sapendo quanto aspettarsene e dove: camera, ISO (o EI e ISO nativo),
codec, bit per pixel, campionamento del colore, letti dal record della clip che scrive lo script (`plugin_cache.py`).
Va **prima di Log MetaRaw**, sull'immagine log, e restituisce la stessa codifica: il resto della catena non cambia.
L'alpha esce identico all'ingresso (Resolve usa l'alpha di un plugin come key del suo stesso nodo).

Codice: `ofx/SLogMetaRaw/src/noise/` (`NoiseEngine` motore CPU senza OpenFX, `NoiseProfiles` profili per camera e
formato, `NoiseEffect`/`NoiseFactory` il nodo). Test: `tests/noise_test.cpp`, `tests/test_noise.py`.

## Stato

| | |
|---|---|
| Implementato e verificato dai test | motore spaziale e temporale su fotogrammi sintetici (rumore bianco noto, scena ferma, in movimento, taglio); misura dal fotogramma; forza 0 = immagine identica; alpha invariato; pannello caricato dal mini host OFX |
| Non verificato | comportamento in Resolve (accesso temporale, cache, prestazioni), qualità su clip reali con rumore del codec e NR della camera, protezione pelle (euristica) |
| Stimato, da tarare con i campioni | quasi tutte le costanti dei profili (sotto) |
| Pianificato | kernel Metal/GPU (oggi solo CPU), CUDA, misura su un'area scelta, profili salvati per camera/ISO |

Prestazioni misurate su CPU a 4 thread (container Linux): HD solo spaziale 150 ms, HD con ±2 fotogrammi 380 ms;
UHD con ±2 circa 1,5 s. Su un Mac con più core sarà più rapido, ma resta lento per la riproduzione: per lavorare
usa **Salta Noise ovunque**.

## Come lavora

1. **Profilo**: per 16 livelli di luminosità e 5 bande (1, 2, 4, 8, 16 px) il rumore atteso, separato per
   luminanza e colore. Tre modi:
   - *Automatico*: dal modello della camera e del formato (sotto);
   - *Misurato*: il pulsante **Misura dal fotogramma** legge il fotogramma corrente; per ogni luminosità e banda
     prende i blocchi più uniformi (15° percentile). Serve un fotogramma con zone piatte (cielo, muri, fuori fuoco)
     a più luminosità. Le bande grosse mancanti si ricavano dal rapporto delle altre;
   - *Manuale*: profilo generico della curva, regolato da *Livello rumore*.
2. **Temporale** (fotogrammi ±1…±3, Automatico per formato): per ogni vicino un vettore di movimento per blocco
   16×16 (ricerca su piramide 1/4, 1/2, piena, ±24 px), poi un peso per pixel dalla differenza delle patch 3×3
   rispetto al rumore atteso: dove la differenza è più di quanto il rumore spiega (movimento sbagliato, occlusione,
   taglio) il vicino non conta. Il rumore rimasto (√Σw²/Σw, corretto per la parte che nel Long GOP si ripete tra
   fotogrammi) abbassa la soglia dello spaziale.
3. **Spaziale**: canali opponenti ortonormali (Y, R−B, R−2G+B), wavelet à trous B3 in 5 bande, ogni coefficiente
   ridotto con il *garrote* (0 sotto la soglia, quasi intatto molto sopra) con soglia = 1,5 × forza × rumore atteso
   della banda alla luminosità di quel punto. Così la forza cresce da sola dove la camera è più rumorosa (ombre, ISO
   alti) e luminanza e colore hanno forze separate.
4. **Protezioni**: *Preserva dettaglio* rimette una parte della grana più fine; *Protezione pelle* conserva parte
   della trama fine sui toni pelle (tinta calda e poco satura, rilevata in modo euristico sui valori log).

Viste: Uscita, Rumore rimosso (grigio = niente, ×4), Mappa del rumore atteso, Movimento (bianco = vicini usati).

## Modello per camera e formato

Rumore del segnale registrato (lineare, grigio 0,18): Poisson-gaussiano σ² = a·y + b, fissato dalle soglie della
vecchia Maschera rumore (stop sotto il grigio dove il rumore inizia e dove è pieno, all'ISO nativo): SNR 4 all'inizio,
1,5 al pieno. Poi in code value con la pendenza della curva log a quel livello.

- **Cine EI (Sony)**: il file ha il guadagno nativo; l'EI non cambia il rumore registrato. Il nativo alto
  (FX6 da EI 12800, FX30 da 2500) costa circa mezzo stop di ombre.
- **ISO reale (iPhone, DJI)**: ISO sopra il minimo = guadagno sul segnale e sul rumore; la NR interna ne toglie una
  parte crescente (stima: fattore 0,6 × guadagno^0,6).
- **Codec**: ProRes RAW e 8 bit anticipano il rumore di mezzo stop.
- **Classe del formato** (forma per banda, correlazione temporale, raggio automatico):

| Classe | Quando | Raggio | Rumore che si ripete tra fotogrammi |
|---|---|---|---|
| RAW | ProRes RAW (fourcc aprn/aprh) | ±2 | 0 |
| Intra | XAVC-I/S-I, ProRes, sconosciuto | ±2 | 0,05 |
| Long GOP | 10 bit, ≥ 0,4 bit/pixel | ±2 | 0,25 |
| Long GOP compresso | 8 bit o < 0,4 bit/pixel | ±1 | 0,45 |
| Elaborato in camera | iPhone (non RAW), DJI Osmo Action | ±1 | 0,40 |

  Nel Long GOP compresso e nell'elaborato in camera il rumore fine è già stato tolto dal codec o dall'ISP e resta a
  macchie più larghe: la forma sposta il peso dalle bande fini a quelle medie. Con 4:2:0 il colore ha macchie più
  grosse. Tutte stime, da tarare con *Misura*.
- **Risoluzione**: se la clip è più grande dei fotogrammi che il nodo riceve (timeline HD da un 4K, proxy), il
  rumore scende circa del rapporto.

### Le cinque camere

| Camera | Formati | Note per il denoise (fonti: ricerche di progetto, sotto) |
|---|---|---|
| Sony FX6 | XAVC-I (Intra 10 bit 4:2:2, 240-600 Mbps UHD), XAVC-L (Long GOP 100/150 Mbps UHD, **8 bit** in UHD secondo Alister Chapman), ProRes RAW esterno (16 bit SDI → 12 bit log, Atomos), BRAW via HDMI dal firmware 6 | nativi 800/12800; CineD: XAVC-I 11,7 stop a SNR 2 (800), ProRes RAW 11,4; NR interna minima a 800, non disattivabile a 12800; banding orizzontale a −4/−5 stop |
| Sony FX30 | XAVC S-I (10 bit 4:2:2 fino a 600 Mbps), XAVC S (H.264 8 bit 4:2:0 o 10 bit 4:2:2), XAVC HS (H.265 10 bit, 30-280 Mbps), ProRes RAW via HDMI (4672×2628) | nativi 800/2500; UHD fino a 60p sovracampionato dal 6K; **4K 100/120p crop 1,6× senza sovracampionamento** (profilo ×1,4, stima); misure dalla a6700 (stesso sensore) |
| iPhone 17 Pro | HEVC/H.264, ProRes 422 HQ/422/LT, ProRes RAW e RAW HQ (nuovo), Apple Log 2 | ISO base stimato ~100 (CineD); ISP sempre attivo anche in RAW, esposizione e tone mapping variabili con la scena; telefoto più rumoroso (~2,6 stop meno luce, stima) |
| DJI Osmo Action 4 | H.264/HEVC fino a 130 Mbps, D-Log M 10 bit, 4:2:0 | ISO 100-12800; NR interna regolabile (molti la mettono a −2); ISO per fotogramma nel track djmd |
| DJI Osmo Action 5 Pro | HEVC fino a 100-120 Mbps (fonti discordi), Normal 8/10 bit, HLG, D-Log M 10 bit | ISO 100-51200 in video; SuperNight = NR con AI (aloni); 960 fps = interpolazione |

## In Resolve

1. Nodo **Log MetaRaw Noise** prima del nodo Log MetaRaw (o di CST/LUT), sull'immagine della camera.
2. Lascia Profilo su Automatico se la clip ha il record dello script (la riga *Da* dice camera, formato e ISO usati);
   altrimenti scegli un fotogramma con zone uniformi e premi **Misura dal fotogramma**.
3. Regola *Livello rumore* (quanto rumore il filtro si aspetta), poi Luminanza e Colore. *Vista: Rumore rimosso*
   mostra cosa toglie: deve sembrare grana senza contorni.
4. **Salta Noise ovunque**: spegne tutti i nodi Noise del progetto finché non lo ripremi o riapri Resolve. Prima
   dell'export controlla che la riga Noise non dica SALTATO. Resolve può tenere in cache fotogrammi calcolati prima:
   se dopo la riaccensione un nodo sembra spento, svuota la render cache.

Accesso temporale: i fotogrammi vicini vengono dalla stessa clip; in timeline con clip rallentate o accelerate,
compound o nested i vicini possono essere sbagliati (problema noto anche a Neat Video): in quei casi Fotogrammi:
Spento, o Render in Place prima.

## Fonti

Ricerche di progetto (cartella condivisa del progetto, `rumore-ricerca/`): *Rumore video per camera*, *Rumore codec
e denoiser video*, note *Formati camere per denoise* (Sony FX6/FX30, iPhone 17 Pro, DJI Osmo Action 4/5 Pro).

- Poisson-gaussiano e stabilizzazione: A. Foi et al., «Practical Poissonian-Gaussian noise modeling and fitting for
  single-image raw-data», IEEE TIP 2008; M. Mäkitalo, A. Foi, IEEE TIP 2013 (Anscombe generalizzata).
- Wavelet à trous e soglie per banda: J.-L. Starck, F. Murtagh, *Astronomical Image and Data Analysis*.
- Stima del rumore da blocchi uniformi: N. Ponomarenko et al. / M. Colom, A. Buades, IPOL 2013.
- Temporale con pesi per patch e block matching: S. Hasinoff et al., «Burst photography for HDR and low-light
  imaging on mobile cameras» (HDR+), SIGGRAPH Asia 2016; Neat Video, guida utente (raggio 2 di default).
- Camere: Sony (specifiche FX6, Help Guide FX30), CineD (laboratorio FX6, a6700), xdcam-user (FX30), Apple Support
  (iPhone 17 Pro), DJI (specifiche Osmo Action 4/5 Pro), ExifTool (tag DJI djmd).

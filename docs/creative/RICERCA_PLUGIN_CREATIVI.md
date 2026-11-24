# Ricerca: plugin creativi di look (film, camera matching, video analogico, ottica)

Data: novembre 2026. Scopo: capire funzioni, controlli, ordine di elaborazione e costanti comuni per progettare un
nodo OFX "Creative" originale. Nessun codice, LUT, profilo o nome di stock va copiato: i nomi commerciali qui sotto
servono solo a identificare i prodotti studiati.

**Legenda di affidabilità**
- **[doc]**: da documentazione ufficiale o recensione citata (vedi Fonti).
- **[gen]**: conoscenza tecnica generale (sensitometria, standard video), non verificata su una fonte di questa
  ricerca. Va confermata prima di essere usata come costante.

Molte guide sono parziali o riservate ai membri (Mixing Light). Dove mancano i valori di default lo si dice.

---

## 1. Schede prodotto (21 strumenti)

### Emulazione pellicola completa (negativo → stampa)

**Dehancer Pro** (OFX Resolve, FCP, Premiere/AE, Vegas, Baselight, SCRATCH; app iPad/Mac) [doc]
- Strumenti: Input, **Expand** (adatta la gamma di contrasto del profilo alla sorgente *prima* del profilo, invece di
  correggerne l'uscita), Film Profile, Push/Pull, Film Compression, Film Developer, Print Film, **CMY Color Head**
  (filtri YMC della stampa ottica), Halation, Bloom, Grain, Film Breath, Gate Weave, Film Damage, Vignette (trattata
  come mascheratura in stampa) e Overscan (Film Orientation, Lens Zoom, Offset X/Y). L'ordine completo è solo
  nel diagramma; il testo conferma Input → Expand → Film Profile e la vignettatura in fase di stampa.
- **Grain**: procedurale "3D", generato dal colore e dalla luminanza locali, non sovrapposto. Size 1–10 (1 ≈ emulsione
  sensibile su 35–70 mm, 10 ≈ 8 mm); Amount 0–100 (≈ densità ottica, a 100 opaco); Shadows/Midtones/Highlights; Chroma
  0–100; **Film Resolution** (100 = dettaglio sorgente; valori più bassi sfocano, perché il dettaglio non può essere
  più fine della grana); Film Type Negative/Positive; scala automatica sulla risoluzione della timeline.
- **Halation**: Defringe preliminare, Source Limiter (soglia, default 0), Background Gain, Local Diffusion (raggio),
  Global Diffusion (bagliore secondario dello strato rosso nei mezzitoni), Smoothness, Hue (0 = rosso puro, 100 =
  arancio), Amplify, Blue Comp., Impact.
- **Film Breath**: Period (frame), Exposure, Tonal Contrast, Color (spostamenti sottrattivi casuali), Impact.
- **Gate Weave**: Period, Translation, Rotation, Auto Zoom, Impact.
- Il LUT Generator esclude gli strumenti spaziali.
- Distintivo: Expand, CMY head, Film Breath e un modello di alone a due scale.

**FilmConvert Nitrate** (OFX, Premiere/AE, FCP) [doc]
- Tre passi: **Camera Pack** (oltre 100 profili camera/gamma da test chart), Film Stock (19 stock in 5 gruppi),
  Grain.
- Lavora in Cineon log.
- Con sorgente log: **Film Color** e **Cineon to Print Film** (0 = scansione Cineon, 100 = stampa piena). Con sorgente
  non log: Film Luma e Film Chroma.
- Grana da scansioni reali in 8 formati (da 8 mm a 35 mm full frame), con curva di risposta per stock, regolazione
  per ombre, mezzitoni e luci, Softness e Saturation. Nelle versioni successive sono state aggiunte halation e curve.
- Distintivo: il profilo è legato alla *camera sorgente*, non solo allo spazio colore.

**FilmConvert CineMatch** (OFX, Premiere, FCP) [doc]
- Abbinamento sensore-sensore: ogni camera è profilata in condizioni identiche, poi la sorgente viene mappata sul
  target e si riapplica la scienza colore del target.
- Controlli: esposizione, bilanciamento del bianco, temperatura e tinta in stile camera; HSL per colore sorgente e
  target; falsi colori Middle Grey e Skinstones; generatore di LUT; uscita Rec.709, HLG e PQ.

**FilmConvert Cineprint16** (OFX) [doc, parziale]
- Variante orientata a 16 mm e stampa, con guida di 27 pagine non leggibile qui. Controlli non verificati.

**Video Village Filmbox / Filmbox Pro** (Resolve, Baselight, Livegrade, Colorfront, Premiere/AE, FCP) [doc]
- Pipeline unica in ACEScct AP1, in quest'ordine: Film System → Mode → Source → **Camera** (esposizione e bilanciamento
  *prima* dell'esposizione del negativo) → **Negative** (Color & Tone, Halation, Grain, Gate Weave, Flicker, Aura) →
  **Lab** (push/pull, acutance) → **Print** (dalla stampa a contatto completa alla scansione telecine) → Display (HDR).
- Dati di misura: chart illuminata ad ampio spettro su 20 stop a passi di ½ stop. Il colore è basato su un negativo
  250D stampato a contatto su 2383; gli altri negativi sono definiti come offset. Grana, alone e weave vengono da
  scansioni ArriScan su S35.
- **Halation**: Style, Gauge (che determina il raggio), Radius, Strength, Sharpness, **Aura** (alone molto largo dei
  negativi senza remjet, con contributo del verde regolabile), colore RGB.
- **Grain**: Style/Gauge, Strength, Saturation, Time Offset, Softness, Roughness (aggregazione), **Streaks** (developer
  mottle), Desqueeze, distribuzione tonale su 3 o 9 zone, Channel Mix, Granule Brightness, Dust.
- **Gate Weave**: Style (profilo di movimento di una camera reale) e Strength.

**Cullen Kelly / Steve Yedlin / Mitch Bogdanowicz – Genesis** (OFX Resolve) [doc]
- Ultimo nodo, che fa anche da trasformazione di uscita.
- Input: ACES, DWG (default), ARRI, RED o Sony.
- **Negative Stock**: 42 emulsioni con l'anno di produzione.
- **Development**: Push/Pull fotometrico, **Interlayer Effect** (saturazione che preserva la luminanza), Bleach Bypass
  sul negativo, Neutral Neg Curves, Exposure.
- **Print**: **Printer Points** applicati nello spazio del negativo, 13 stock di stampa, Bleach Bypass della stampa
  (separato da quello del negativo), Neutral Print Curves, Display Target.
- Halation e grana (35, 16 e 65 mm) seguono il modello di Yedlin: rispondono alla luminanza, la grana è colorata e c'è
  un seed.
- Distintivo: trattare la pellicola come un *sistema* e non come una somma di filtri.

**ARRI Film Lab 1.0 → 1.5** (OFX: Resolve, Baselight, Nuke; on-set con Resolve Live e Livegrade) [doc]
- Grain con struttura 50D, 200T o 500T e formati da 8 a 65 mm; Size e Detail ciascuno 0,75–1,5.
- Color: stampa, invertibile, bleach bypass.
- Halation (intensità ed estensione), Gate Weave.
- 8 rapporti anamorfici da 1,25× a 2,0×, che stirano grana e alone.
- Lavora in LogC3, LogC4 e ACES.
- Distintivo: filosofia "KISS" con pochi parametri e una leggera riduzione della MTF. La Look Library va nel nodo
  precedente.

**Colourlab Ai Look Designer 2 + Grainlab** (OFX, Premiere/AE, app) [doc]
- Moduli: Input/Output Transform, RGB Gamut Limits, **Printer Lights** (All + RGB), Lift/Gamma/Gain, **Subtractive
  Color** (CMY), Push/Pull, Temperature, Saturation, Negative Stock con Intensity, stock "Major/Minor" espressivi non
  legati a pellicole reali, Print Stock con Contrast e Intensity, ENR (bleach bypass).
- Strumenti di test: rampe di saturazione e luminosità, falsi colori a zone.

**Magic Bullet Film / Looks** (Maxon Red Giant: Premiere/AE, FCP, Resolve) [doc]
- Magic Bullet Film: My Footage Is (Flat/Video/Log), Temperature, Tint, Exposure, Contrast (±100), Saturation 50–150,
  **Skin Tone** (±100, verso magenta o verde), **Vintage/Modern** (±100), Grain (50%), Vignette, Strength.
- Looks separa Film Negative (22 stock) e Film Print (4).
- **Halation**: Amount, Size, Anamorphic, Falloff, Highlights Only, Radiate, Color, **Emulsion Spread**.
- **Renoiser**: grana per canale con Size e Amount, Log Grain.
- Altri strumenti: Print Bleach Bypass (Silver Retention), 2-Strip Process, **Telecine Net**, Shoulder (Rolloff Start).

**Koji / Koji Advance** (FxFactory: FCP, Premiere/AE) [doc]
- Fuori commercio (pagina "no longer available"). Ha un peso storico ma non è più rilevante nel 2026.

### Strumenti integrati nei programmi

**Resolve Film Look Creator** (ResolveFX, Studio) [doc]
- Color Blend (master 0–1); **3D LUT Compatible** (limita l'effetto alle operazioni di colore); Film Looks
  (Cinematic come default più altri 4).
- Color Settings: Exposure, Contrast, Subtractive Sat, Richness, Highlights, Fade, White Balance, Tint, Skin Bias.
- Split Tone: Amount, Hue Angle, Pivot.
- Textures: Vignette, Halation (Radius, Hue), Bloom, Grain.
- Altri: Flicker, Gate Weave, Film Gate (Ratio, Padding).
- Lavora in spazio scena.

**Resolve Film Grain** [doc]
- Preset 8/16/35 mm, Composite Type, Opacity.
- Texture, Grain Size, Grain Aspect Ratio, Grain Strength, Offset, Symmetry, Softness, Saturation.
- Grana RGB; guadagno per ombre, mezzitoni e luci.

**Resolve Film Halation, Glow, Aperture Diffraction, Film Damage** [doc, parziale]
- Halation e Glow sono separati dalla diffusione.
- Aperture Diffraction modella la PSF del diaframma (raggi a stella). Parametri non verificati qui.

**Resolve Analog Damage** [doc]
- **Telecine Source**: Vignetting, Shutter Weave.
- **Broadcast Signal**: Noise Scale, Signal Noise, Chroma Noise, Detail Loss, **Chroma Detail Loss**, Ghosting,
  **Ghost Offset**, **Chroma Misalignment**.
- **Color Dials**: Brightness, Contrast, Color, Tint.
- **Scan**: H-Shift, V-Shift, **V-Hold/Latch**, Overscan, V-Scale, Vertical Blanking.
- **Scan Lines**: Line Sharpness, Line Frequency, Colored Lines.
- **TV Construction**: Phosphor Brightness, Phosphor Tint, Defocus, Screen Curvature, Edge Mask.
- **VHS**: **Restless Foot** Height, Offset, Jitter.

**Adobe Lumetri – Creative** [gen]
- Look e Intensity; Faded Film, Sharpen, Vibrance, Saturation, Shadow Tint, Highlight Tint, Tint Balance.
- È solo colore, senza strumenti di texture.

### Effetti ottici e analogici

**Boris FX Sapphire** (OFX, AE/Premiere, Avid, FCP) [doc]
- **S_FilmEffect**: Neg Film e Print Film; Input/Output Gamma 2.2; Neg Exposure e Print Exposure; **Print Lights R/G/B
  default 25, range 0–50**; Glow con larghezza per canale; grana con Amp per canale (R 0,9, G 1, B 1,6), Darks/Brights,
  Midtone Pos, Blur per canale (R 1, G 0,9, B 1,2), Grain Hold Frame/Field/3:2; Vignette; Fields.
- **S_VHSDamage**: Downsample a 720; Interlaced Combing; Color Downsample (scala 6,67, altezza relativa 0,6); Color
  Aberration (blur R 0,84, G 0,55, B 1; sharpen 2); Color Bloom; Luma Adjust (Blur Luma con Blur Rel X/Y = 0,05 / 1,
  cioè sfocatura *solo orizzontale*); Tape Noise; Fast Forward a bande; Scanlines; Streaks; Sparkles (dropout); seed.
- Strumenti collegati: S_FilmDamage, S_Grain, S_Halation, S_Glow.

**Maxon Universe VHS** [doc]
- Black Level, Luminance Blur 0–20, Chroma Blur 0–100, Chroma Offset X (default 10), Chroma Weave, offset e blur per
  canale RGB.
- Tape Noise Type: VHS Noise, Bad Tracking, Blank Tape, Vertical Slip ecc.; Pop Line Frequency; Frame Jitter 0–20;
  Interlace Error.
- Tape Style: VHS, Bad VHS, S-VHS, U-matic, Beta.
- Generatore di OSD (SP/LP/SLP, contatore).

**ntsc-rs** (open source, OFX/AE/standalone) [doc]
- È l'unico prodotto che simula il *segnale* composito: modulazione e demodulazione del croma, filtri notch e comb.
- La catena dei parametri (default fra parentesi), nell'ordine dell'elenco ufficiale; la pagina non conferma che sia
  l'ordine di esecuzione:
  1. field;
  2. filtro luma in ingresso (Notch);
  3. passa-basso croma in ingresso (Full);
  4. sharpening composito (1);
  5. rumore composito (0,05);
  6. snow;
  7. sfasamento di riga (180°);
  8. demodulazione croma (Notch o comb a 1–2 righe);
  9. luma smear (0,5);
  10. **head switching** (altezza 8 righe, offset 3, spostamento orizzontale 72, inizio a metà riga, jitter 0,03);
  11. **tracking noise** (altezza 12, onda 15);
  12. **ringing** (frequenza 0,45, power 4, scala 4);
  13. rumore di luma e di croma;
  14. errore e rumore di fase del croma;
  15. ritardo del croma orizzontale e verticale;
  16. VHS (velocità del nastro SP/LP/EP, perdita di croma, sharpen, **edge wave**);
  17. fusione verticale del croma;
  18. passa-basso croma in uscita.

**Scatter** (Video Village) [doc]
- Filtri di diffusione fisici (tipo Pro-Mist), tarati per un 50 mm su S35, con Highlight Colour Recovery.

**Grana da scansione (CineGrain, Grainlab, overlay)**
- Approccio a plate di grana: realistico, ma ripetitivo e indipendente dalla densità dell'immagine.

**PixelTools Film Lab** (PowerGrade Resolve) [doc]
- Look di stampa costruito con nodi nativi: un gruppo per la stampa e il negativo, poi nodi separati per grana,
  halation, gate weave e diffusione, ciascuno in 3 intensità.
- Distintivo: trasparenza totale della catena.

---

## 2. Costanti condivise

1. **Separazione colore/texture.** Quasi tutti separano la parte cromatica (esportabile come LUT) da quella spaziale
   e temporale (grana, alone, weave). Resolve ha "3D LUT Compatible"; Dehancer e Genesis escludono gli strumenti
   spaziali dalla LUT.
2. **Ordine fisico** (ricostruito da Filmbox, Dehancer e Genesis):
   1. trasformazione di ingresso verso uno spazio di lavoro log o scena (ACEScct, DWG, Cineon, LogC);
   2. "Camera": esposizione e bilanciamento sulla luce *prima* del negativo;
   3. ottica: bloom e diffusione, in luce lineare di scena [gen];
   4. negativo: curve, interlayer, **halation** (si forma nell'emulsione, quindi prima della grana);
   5. **grana** nel dominio della densità del negativo;
   6. weave e flicker (meccanica della camera);
   7. laboratorio: push/pull, bleach bypass del negativo;
   8. **stampa**: printer points o testa CMY, stock di stampa, bleach bypass della stampa;
   9. display: trasformazione di uscita e target HDR.

   I danni (polvere, graffi) e la vignettatura di proiezione o telecine vanno dopo la stampa o la scansione.
3. **Printer lights / points.** Si regolano per canale R/G/B. In Sapphire il default è 25 con range 0–50.
   - Convenzione di laboratorio [gen]: 1 punto ≈ 0,025 log E ≈ 1/12 di stop, quindi circa 8 punti per stop.
   - L'alternativa equivalente sono i cursori CMY sottrattivi (Dehancer, Colourlab).
4. **Grana**:
   - Dipende dalla densità e quindi dalla tonalità: controlli per ombre, mezzitoni e luci o a 9 zone.
   - Si scala con il formato: preset 8/16/35/65 mm, cioè con la dimensione del fotogramma rispetto ai pixel.
   - Intensità per canale, con il blu più forte in Sapphire (1,6 contro 0,9 e 1).
   - Ha saturazione propria (nuvole di colorante), softness o size, seed e un offset temporale per clip.
   - ARRI usa una scala di 0,75–1,5 attorno al nominale, Dehancer 1–10.
5. **Halation**:
   - È rossa o arancio e regolabile in tinta (dal rosso all'arancio).
   - Ha soglia o limitatore della sorgente, raggio legato al formato e intensità.
   - Più rara la componente larga ("Aura" o "Global Diffusion").
6. **Gate weave**:
   - Periodo o intervallo in frame, traslazione X/Y, rotazione e zoom automatico per coprire i bordi.
   - Filmbox usa profili di movimento registrati da camere reali.
7. **Flicker / film breath.** Variazioni lente di esposizione, contrasto e colore, con un periodo in frame.
8. **Gate / vignettatura / overscan.** Ritaglio al rapporto della finestra di ripresa, con padding e bordi
   arrotondati.
9. **Blend e strength globali.** Uno 0–1 o 0–100% su tutto il nodo.

---

## 3. Idee distintive da considerare (da reimplementare in modo originale)

- **Expand prima del profilo** (Dehancer): adatta la gamma dinamica della sorgente alla curva della pellicola invece
  di tagliarla dopo.
- **Printer points nello spazio del negativo** e **due bleach bypass separati** per negativo e stampa (Genesis).
- **Interlayer come saturazione che preserva la luminanza** (Genesis): un controllo fisicamente motivato.
- **Stampa variabile con continuità** fra contatto pieno e scansione Cineon (Nitrate "Cineon to Print", Filmbox
  "Print").
- **Aura** come alone gigante dei negativi senza remjet, con un controllo del contributo del verde (Filmbox).
- **Developer mottle / streaks** e **granule brightness** nella grana (Filmbox).
- **Weave registrato da camere reali** al posto del rumore casuale (Filmbox).
- **Anamorfico**: grana e alone stirati secondo il fattore di squeeze (ARRI, Filmbox Desqueeze, MBL Anamorphic).
- **Film Resolution / acutance**: la MTF dello stock viene ridotta in modo coerente con la grana (Dehancer, Filmbox
  Lab, riduzione MTF di ARRI).
- **Skin Tone / Skin Bias** come asse magenta-verde dedicato (MBL, Resolve).
- **Falsi colori di supporto** (zone, grigio medio, incarnati) dentro il plugin (CineMatch, Colourlab, Filmbox).
- **Defringe prima dell'alone** (Dehancer), per evitare che l'aberrazione cromatica del digitale venga amplificata.
- **Tonal distribution a 9 zone con overlay** di falsi colori.
- **Restless foot, V-hold con aggancio e testo OSD** per il video analogico (Resolve, Universe).

---

## 4. Camera matching / simulazione

- **CineMatch**:
  - Profila ogni sensore con le stesse chart e la stessa luce.
  - Riporta la sorgente a una rappresentazione comune "da sensore", poi la porta nella risposta del target e
    riapplica la scienza colore del target.
  - Aggiunge controlli in stile camera (esposizione, bilanciamento in kelvin e tinta) applicati *nel dominio del
    sensore*.
  - Correzioni HSL residue per colore, sorgente e target.
- **Nitrate e Filmbox**: come sopra, ma con la pellicola come target. I Camera Pack sono costruiti con le stesse chart
  riprese su camera e su pellicola; Filmbox caratterizza su Venice con riferimento ad Alexa.
- **Modello tecnico tipico** [gen]:
  1. linearizzazione con la curva inversa log o OETF della camera;
  2. matrice 3×3 dal gamut nativo a XYZ o a uno spazio di lavoro (ottimizzata con i minimi quadrati sulle patch,
     eventualmente in spazio percettivo);
  3. adattamento cromatico (Bradford o CAT02);
  4. correzioni non lineari residue (LUT 3D piccola, polinomio di radice o HSL per tinta);
  5. risposta tonale del target (curva di carattere, spalla e piede);
  6. codifica del target.

  Il bilanciamento "in camera" si applica come guadagni diagonali nello spazio del sensore o del gamut nativo, prima
  della matrice.
- **Raccomandazione di design**: profili sorgente e target come dati (curva, matrice, residuo opzionale), creati con le
  proprie misure o da specifiche pubbliche dei produttori, mai da LUT di terzi.

---

## 5. Video analogico: cosa lo rende credibile

**Parametri di formato**

| Formato | FM luma (sync–bianco) | Croma (color-under) | Risoluzione orizzontale |
|---|---|---|---|
| VHS | 3,4–4,4 MHz | 629 kHz | ~240 TVL [doc] |
| S-VHS | 5,4–7,0 MHz | 629 kHz | ~400 TVL [doc] |
| Betamax | 3,6–4,8 MHz | 688 kHz | 240 TVL [doc] |
| Video8 | 4,2–5,4 MHz | 743 kHz | ~272 TVL [doc] |
| Hi8 | 5,7–7,7 MHz | 743 kHz | 432 TVL [doc] |
| U-matic | — | — | luma ~4 MHz [doc] |
| Betacam SP | — | — | componenti; luma 4,5 MHz, croma 1,5 MHz; S/N > 51 dB [doc] |

- Conversione [gen]: in NTSC circa 80 TVL per MHz, quindi 3 MHz ≈ 240 TVL.
- Banda croma dei formati color-under [gen]: circa 0,4–0,6 MHz, cioè 30–40 linee di colore.
- S/N luma VHS [gen]: circa 43–45 dB.
- In VHS e S-VHS la riduzione della diafonia fra tracce **dimezza la risoluzione verticale del croma** [doc]; ntsc-rs
  la imita con "vertically blend chroma".

**Effetti ordinati per peso percettivo**
1. Croma a banda stretta e *ritardato* o disallineato rispetto alla luma: sbavatura orizzontale di parecchi pixel e
   offset di 1–3 pixel (Universe: Chroma Offset X 10).
2. Luma passa-basso **solo in orizzontale** (Sapphire Blur Rel X 0,05 / Y 1) più **sharpening con ringing e
   overshoot** (lo "sharpness" del videoregistratore). In ntsc-rs ringing e sharpen sono stadi distinti.
3. **Head switching**: banda di circa 5–10 righe in fondo al quadro, spostata orizzontalmente e con jitter
   (ntsc-rs: 8 righe, offset 3).
4. **Tracking noise / edge wave**: ondulazione orizzontale dei bordi riga per riga, che varia nel tempo, più fasce di
   neve.
5. **Dropout**: brevi strisce orizzontali bianche o nere ("sparkles" e "streaks" in Sapphire), random e per riga.
6. **Rumore**: luma fine, croma a bassa frequenza e a macchie, rumore di fase del croma (instabilità della tinta).
7. **Interlacciamento** e field order; downsample a 480 o 576 righe (Sapphire 720 pixel orizzontali).
8. **Jitter di base tempi** (TBC assente): spostamento orizzontale per riga e per campo.
9. Colore: nero sollevato, saturazione, croma "bloom"; LP o EP degradano tutto.
10. Elementi di contesto opzionali: OSD, restless foot, V-hold, ghosting da broadcast, curvatura CRT e fosfori.

Il realismo migliore si ottiene simulando il segnale: codifica YIQ o YUV, filtri 1D lungo la riga con banda in "MHz
equivalenti" convertita in pixel tramite la durata della riga attiva (~52,6 µs [gen]), poi decodifica.

---

## 6. Note di modello fisico (pellicola)

- **Curve caratteristiche** [gen]:
  - Il negativo ha gamma bassa (≈0,55–0,65 in densità per log E) e latitudine ampia.
  - La stampa ha gamma alta (≈2,5–3), con piede e spalla ripidi.
  - Il sistema completo ha un contrasto di circa 1,5–1,8 sulla scena, da cui la "S" del look di stampa.
  - Cineon codifica la densità del negativo a 0,002 D per code value (nero 95, bianco 685 su 10 bit).
  - Modellare le *densità* per strato (C, M, Y), con una matrice di diafonia dei coloranti, prima di passare alla
    stampa.
- **Interlayer / interimage** [gen; Genesis come controllo]:
  - Gli accoppiatori DIR di uno strato inibiscono gli altri.
  - Il risultato è più saturazione e più separazione a parità di luminanza.
  - Si modella con una matrice in densità che sottrae una frazione degli strati adiacenti, rinormalizzata sulla
    luminanza.
- **Halation** [doc]:
  - La luce attraversa l'emulsione, si riflette sulla base o sul fondo della camera e rientra.
  - Lo strato **rosso** è il più profondo e il più colpito, quindi l'alone è rosso; con più energia si aggiunge il verde
    e l'alone diventa arancio.
  - Il blu è quasi assente per via del filtro giallo.
  - Il remjet o strato anti-alone lo riduce molto. Senza remjet compare un alone grande ("Aura").
  - L'alone cresce con la luminosità della sorgente e con il contrasto.
  - Nei formati piccoli è più grande in proporzione: il raggio fisico è circa costante in µm sulla pellicola, quindi in
    pixel scala in modo inversamente proporzionale alla larghezza del fotogramma (16 mm più evidente di 35, 65 quasi
    nullo).
  - Implementazione [gen]:
    1. estrazione delle alte luci in esposizione lineare;
    2. kernel a coda lunga (somma di gaussiane o esponenziale);
    3. iniezione soprattutto nel canale rosso, con un poco nel verde;
    4. ricombinazione *prima* della curva del negativo e della grana.
- **Grana e densità** [doc + gen]:
  - La grana è legata alla densità: la granularità RMS cresce con la densità del negativo fino ai mezzitoni-alte luci.
  - In stampa diventa più visibile nelle ombre della stampa, cioè nelle zone dense del negativo, e attenuata nelle luci.
  - Ogni strato ha una grana propria (il blu, il più veloce, è più grosso).
  - Le "nubi di colorante" sono più morbide dei grani d'argento.
  - Le dimensioni dipendono da stock e formato; si scala sul rapporto fra larghezza del fotogramma e pixel.
- **Gate weave** [doc qualitativo; numeri gen]:
  - Movimento laterale e verticale più una piccola rotazione, con una componente periodica legata alle perforazioni o
    al ciclo di trascinamento e una deriva lenta.
  - Ordini di grandezza [gen], da verificare: camere 35 mm professionali ~0,1–0,2% della larghezza del quadro; 16 mm
    di più; Super 8 e 8 mm amatoriale fino a circa 0,5–1%.
  - In proiezione il weave è maggiore che in ripresa.
  - Scansioni moderne stabilizzate: quasi zero.
  - Esistono raccomandazioni SMPTE sulla stabilità (per esempio RP 105, SMPTE Journal "Image Unsteadiness in 16mm"),
    non lette in dettaglio.
- **Film breath / flicker** [doc]:
  - Le cause sono la stesura non uniforme dell'emulsione, l'otturatore e lo sviluppo.
  - È massimo in Super 8.
  - Si modella con rumore a bassa frequenza (periodo di qualche frame) su esposizione, contrasto e CMY.

---

## Fonti

- https://www.dehancer.com/learn/film-emulation-tools
- https://blog.dehancer.com/articles/how-does-film-grain-work-in-dehancer-ofx-plugin/
- https://blog.dehancer.com/articles/halation/
- https://www.dehancer.com/learn/article/halation
- https://blog.dehancer.com/articles/film-breath-and-gate-weave/
- https://blog.dehancer.com/articles/tools-diagram-of-dehancer-pro-ofx-plugin/
- https://dehancer.com/shop/davinci_resolve/pro/versions
- https://jonnyelwyn.co.uk/film-and-video-editing/understanding-film-emulation-with-filmconvert-nitrate/
- https://www.filmconvert.com/cinematch
- https://www.scribd.com/document/511711482/CinePrint16-User-Guide
- https://videovillage.com/filmbox/technical_faq
- https://videovillage.com/learn/filmbox/full-guide/overview
- https://videovillage.com/learn/filmbox/full-guide/negative/halation
- https://videovillage.com/learn/filmbox/full-guide/negative/grain
- https://videovillage.com/learn/filmbox/full-guide/negative/gate-weave
- https://mixinglight.com/color-grading-tutorials/deep-dive-cullen-kelly-genesis-plugin/
- https://www.provideocoalition.com/review-genesis-film-emulation-plugin/
- https://www.arri.com/filmlab
- https://www.newsshooter.com/2025/12/21/arri-film-lab-review/
- https://digitalproduction.com/2026/10/14/arri-film-lab-1-5/
- https://www.provideocoalition.com/review-look-designer-2-by-colourlab-ai/
- https://help.maxon.net/rg/en-us/Content/html/03-Film-overview-slider-controls.html
- https://help.maxon.net/rg/en-us/Content/html/13-MBL-film-tools.html
- https://prolost.com/blog/filmlooks
- https://fxfactory.com/info/kojiadvance/
- https://mixinglight.com/color-grading-tutorials/film-look-creator-resolvefx-overview-workflow/
- https://blog.frame.io/2024/08/15/what-is-resolves-new-film-look-creator-plugin/
- https://www.steakunderwater.com/VFXPedia/__man/Resolve18-6/DaVinciResolve18_Manual_files/part3405.htm
- https://www.steakunderwater.com/VFXPedia/__man/Resolve18-6/DaVinciResolve18_Manual_files/part3390.htm
- https://mixinglight.com/color-grading-tutorials/how-to-use-aperature-diffraction-resolvefx
- https://borisfx.com/documentation/sapphire/ofx/filmeffect/
- https://borisfx.com/documentation/sapphire/avx/vhsdamage/
- https://borisfx.com/news/sapphire-2025-be-kind-rewind-with-vhs-damage/
- https://help.maxon.net/uni/en-us/Content/html/vhs.html
- https://mltframework.github.io/mlt_web/plugins/FilterOpenfx-wtf-vala%5Entscrs/
- https://ntsc.rs/
- https://digitalproduction.com/2024/01/30/halation-and-diffusion-on-the-hunt-for-the-ultimate-film-look/
- https://www.provideocoalition.com/review-pixeltools-film-lab-powergrade-collection/
- https://help.colourlab.ai/help/grainlab-quickstart
- https://xdevs.com/doc/Sencore/Sencore_Tech_Tips/TT189p%20-%20Comparison%20Of%20VCR%20Formats.pdf
- https://www.videouniversity.com/articles/video-recording-formats/
- https://docs.kdenlive.org/en/effects_and_filters/video_effects/motion/gate_weave.html
- https://journal.smpte.org/periodicals/Journal%20of%20the%20SMPTE/80/10/6/
- https://pub.smpte.org/doc/rp105/20031020-pub/rp0105-2003_stable2016.pdf

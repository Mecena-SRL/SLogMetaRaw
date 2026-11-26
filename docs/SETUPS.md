# Setup d'epoca

Una lista unica, `shared/EraSetups.h`, uguale nel Creative (video) e in Log MetaRaw Audio: la stessa riga nei due
plugin carica la catena reale di un'epoca, di un autore o di un film, dalla cinepresa e dalla pellicola al
registratore e alla banda della copia. Ogni campo nomina per nome esatto una riga delle tabelle dei plugin; il test
`test_every_era_setup_loads_a_coherent_chain` controlla che ogni nome esista. Dopo la scelta tutto resta modificabile.
Un campo vuoto è la dotazione del dispositivo (formato, sensore, microfono, supporto) o nessuno (ottica, copie,
riversaggio, visione, banco). L'ordine è permanente: si aggiunge in coda.

Legenda: **[fonte]** = scritto nella fonte citata; **[ded]** = dedotto da noi (la fonte non lo dice: scelta
coerente con l'epoca e il resto della catena, da sostituire appena si trova una fonte).

Stato: video applicato nel Creative. Audio: righe pronte nella tabella, applicazione nel plugin audio in arrivo
(serve aggiungere Perfectone EP 6 e AKG D 25 alle tabelle audio con il loro frontale).

## Pre-cinema · Cinématographe Lumière (1895)

- Cinématographe: camera, proiettore e stampatrice in uno, a manovella, obiettivo fisso, 35 mm 1,33 con una
  perforazione tonda per lato, circa 15 fotogrammi al secondo [fonte: Wikipedia *Cinematograph*; George Eastman
  Museum, oggetto 51661; ASC, *Lumière Cinématographe comes to the ASC*]. Sviluppo in un secchio di rivelatore e uno
  di fissaggio [fonte: ASC].
- Pellicola: emulsione Lumière; non sensibilizzata al verde e al rosso [ded]. Grana e instabilità della manovella:
  stime [gen].
- Audio: muto, nessuna catena audio.

## Nouvelle Vague · À bout de souffle (Godard, 1960)

- Cinepresa Éclair Cameflex, l'unica che faceva passare la pellicola usata: la griffa tocca solo il bordo della
  perforazione; a mano, anche da una sedia a rotelle [fonte: Wikipedia *Breathless*; TCM, *Behind the camera:
  Breathless*; Coutard, Web of Stories 14460-14461].
- Pellicola fotografica Ilford da 400 ASA, non da cinema, spinta di uno stop a 800 ASA con un rivelatore alla
  parafenilendiammina, sviluppata ai laboratori GTC di Joinville [fonte: Wikipedia; TCM; Coutard 14460]. Wikipedia la
  chiama HP5, che esce solo nel 1976: nel 1959 la 400 ASA di Ilford era la **HPS** (dal 1954; 400 ASA fino alla
  revisione delle scale del 1960, poi 800) [fonte: Wikipedia *Ilford HP*; photomemorabilia, *Ilford film speed
  changes*]. Formato: Academy [ded].
- Suono: tutto post-sincronizzato, la Cameflex era troppo rumorosa per la presa diretta [fonte: Wikipedia; TCM].
  Studio con registratore a valvole e microfono a condensatore, banda ottica 35 mm in copia [ded].

## Nouvelle Vague · Vivre sa vie (Godard, 1962)

- Cinepresa Mitchell, pesante [fonte: Wikipedia *Vivre sa vie*, con richiesta di citazione]. Pellicola: Kodak
  Double-X [ded].
- Suono in presa diretta, dialoghi e rumori, su una sola pista, senza montaggio del suono: un registratore portatile
  sincrono Perfectone su nastro da 6,35 mm, poi riversato su 35 mm perforato; microfoni AKG D 25 o Neumann; fonico
  Guy Villette [fonte: Criterion, *An audacious experiment: the soundtrack of Vivre sa vie*]. Perfectone EP 6:
  19 cm/s, pista intera mono con una metà per il segnale di sincronismo, 30 Hz-13 kHz, VU [fonte: reel-reel.com;
  radiomuseum.org].

## Nouvelle Vague · Le Mépris (Godard, 1963)

- Schermo largo 2,35:1, pellicole molto lente, obiettivi con distorsione a barilotto e a cuscinetto; copie finali su
  pellicola Kodak [fonte: ASC, recensione del DVD di *Contempt*, maggio 2003]. Anamorfico Franscope e cinepresa
  Mitchell [ded]. Negativo Eastmancolor 5251 [ded: è il negativo di *Pierrot le fou* due anni dopo; anno 1962 dall'articolo
  SMPTE «A New Color Negative Film for Better Picture Quality», vol. 71 n. 10, da confermare].
- Suono: post-sincronizzato in studio [ded].

## Nouvelle Vague · Pierrot le fou (Godard, 1965)

- Techniscope (due perforazioni, ottiche normali), Eastmancolor, negativo 5251; cineprese Mitchell e Arriflex;
  missaggio originale su pista magnetica [fonte: Il Cinema Ritrovato, scheda del restauro]. Nessuna ottica d'epoca
  particolare: «obiettivi convenzionali» [fonte: idem].
- Suono: René Levert e Georges Liron [fonte: idem]; registratore Nagra III [ded].

## Godard e il video · Numéro deux (1975)

- Riprese in video con camere fisse nelle stanze di un appartamento, poi rifilmate su 35 mm [fonte: Wikipedia
  *Number Two*]. Camera U-matic portatile [ded]; il riversaggio Film-out su 35 mm rende il passaggio su pellicola.

## Pasolini · Accattone (1961)

- Bianco e nero ad alto contrasto, fotografia di Tonino Delli Colli, formato Academy 1,37 [fonte: Il Cinema
  Ritrovato; BFI, *5 ways to recognise a Pasolini film*]. Pellicola Ferrania P30 e cinepresa Mitchell [ded].
- Suono post-sincronizzato, voci doppiate (Franco Citti doppiato, Monica Vitti per Paola Guidi) [fonte: Wikipedia
  *Accattone*; TCM; BFI].

## Pasolini · Il Vangelo secondo Matteo (1964)

- Bianco e nero, in esterni nel Sud Italia, Delli Colli; «cominciai a usare lo zoom» [fonte: Wikipedia *Il Vangelo
  secondo Matteo*]. Zoom Angénieux, Ferrania P30 e Mitchell [ded].
- Dialoghi in gran parte doppiati da attori professionisti (Enrico Maria Salerno per Irazoqui) [fonte: idem].

## Televisione · TV di Stato agli esordi (studio, 1954)

- Trasmissioni regolari dal 3 gennaio 1954 dagli studi di Milano, in diretta [fonte: Il Post, 3 gennaio 2024;
  Wikipedia *1954 in Italian television*]. 625 righe in bianco e nero, telecamere a tubo image orthicon, registrazione
  su pellicola (kinescopio) [ded: le fonti lette non lo dicono].
- **Approssimato**: il Creative non ha ancora il tubo image orthicon (alone nero attorno alle luci); il setup usa la
  camera da studio senza sensore a tubo e il kinescopio su 16 mm bianco e nero, che rende il bianco e nero e la
  pellicola. Microfono a nastro RCA 77-DX e banco broadcast a valvole [ded].

## Televisione · TV commerciale (anni '80)

- Programmi registrati su videocassetta e spediti alle emittenti [fonte: Wikipedia *Canale 5*]. Camera Ikegami a
  tubi Plumbicon con U-matic BVU, microfono a fucile [ded].

## Da fare

- Fonti per pellicola e cinepresa di *Vivre sa vie*, *Accattone* e *Vangelo*; per l'attrezzatura della TV del 1954 e
  degli anni '80.
- Tubo image orthicon nel Creative (TV 1954).
- Altri setup: muto (Méliès), primo sonoro, neorealismo, Super 8 e video degli anni '70 di Godard (Sonimage).

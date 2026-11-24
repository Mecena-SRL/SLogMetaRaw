# Maschera rumore

Con Maschera accesa (Automatica o Personalizzata) il nodo Develop diventa, dalla 3.2.1, un **nodo maschera**: non
sviluppa, passa l'immagine che riceve identica e scrive solo l'**alpha**, 1 dove la camera, all'EI scelto, produce
rumore visibile e 0 dove è pulita, sfumando. Serve a far lavorare Neat Video o un altro denoiser solo lì. Nella 3.2.0 lo
stesso nodo sviluppava e scriveva l'alpha, ma Resolve usa l'alpha di un plugin come key del suo stesso nodo (uscita =
sviluppata dove l'alpha è 1, ingresso del nodo dove è 0) e l'immagine risultava strana. Il nodo che sviluppa, con
Maschera spenta, non cambia (Rec.709 compreso). Lo verifica `tests/test_ofx_math.py`: l'immagine del nodo maschera è
identica al suo ingresso bit per bit, con ogni tone mapping scelto.

Codice: `src/develop/NoiseMask.cpp` (tabella e soglia), `math/Develop.h` (`sm_noise_mask`, per pixel).

## Come si calcola

Sul pixel dopo Exposure e bilanciamento, `s = log2(Y / 0.18)` (stop dal grigio 18%):

```
nativo   = ISO nativo alto se l'EI di ripresa lo usa, altrimenti quello basso (ignoto: l'EI di ripresa)
inizio   = D_camera − 0,5 (nativo alto) + codec          (stop sotto il grigio all'ISO nativo)
soglia   = max(0, inizio − log2(EI scelto / nativo))
maschera = smoothstep da 0 a −soglia fino a 1 a −(soglia + sfumatura)
```

- Ogni stop di EI scelto sopra il nativo avvicina la soglia di uno stop al grigio. Misurato da ARRI: ALEV-3 a EI
  800/1600/3200 ha il pavimento (SNR = 1) a 6,9/5,9/4,9 stop sotto il grigio.
- Il nativo alto costa circa mezzo stop di ombre (FX6, A7S III, S5 II nei test CineD), non la sua distanza dal basso.
- Codec: ProRes RAW −0,5 stop (FX6: 0,3-0,7 stop di gamma in meno a SNR = 2 senza la riduzione rumore interna);
  8 bit −0,5 (stima: blocchi e banding); Intra, ProRes e Long GOP 10 bit 0. Letto dal record della clip
  (`noise_codec` in `slogmetaraw/plugin_cache.py`); i record scritti prima della 3.2.0 non lo hanno finché non si
  preme *Rileggi metadata*.
- Personalizzata: Inizio e Sfumatura scelti a mano; nativo, codec ed EI si applicano lo stesso.

## Costanti per camera

`D` = inizio / pieno, stop sotto il grigio all'ISO nativo con codec interno. *Misurata*: dai test di laboratorio
(latitudine e IMATEST SNR = 2 di CineD, tabella ARRI); *derivata*: ricavata da una misura vicina; *stimata*: nessuna
misura trovata.

| Camera / curva | D | ISO nativi (passaggio) | Base |
|---|---|---|---|
| Sony FX6 | 3,5 / 5 | 800 / 12800 | misurata |
| Sony FX30 | 3 / 4,5 | 800 / 2500 | stimata |
| Sony FX3 | 3 / 4,5 | 800 / 12800 | stimata (sensore A7S III) |
| Sony A7S III | 3 / 4,5 | 640 / 12800 | misurata |
| Sony A7 IV | 3 / 4,5 | 800 | misurata |
| altre S-Log3 / S-Log2 | 3 / 4,5 | 800 | stimata |
| Panasonic S5, S5 II (e V-Log) | 3 / 4,5 | 640 / 4000 | misurata (V-Log generica stimata) |
| Panasonic GH6 | 2,5 / 4 | 800 / 2000 | misurata |
| ARRI LogC3 (ALEV-3) | 5,3 / 6,9 | EI 800 | derivata |
| ARRI LogC4 (ALEV-4) | 6,1 / 7,7 | EI 800 | derivata |
| Apple Log (iPhone) | 1,5 / 3 | 55 | derivata (iPhone 15 Pro, fonte secondaria) |
| DJI D-Log M, D-Log | 2 / 3 | 100 | stimata |
| Canon C-Log2/3, Fujifilm F-Log/F-Log2, Nikon N-Log, RED Log3G10 | 3,5 / 5 | 800 (F-Log 640, F-Log2 1250) | stimata |
| Blackmagic Film Gen5 (Pocket 6K) | 3,5 / 5 | 400 / 3200 (da 1250) | stimata |
| HLG | 2 / 3,5 | ripresa | stimata |

Non verificati: il punto di passaggio al nativo alto (si usa il nativo alto stesso, tranne Blackmagic), gli ISO
nativi di FX3, X-H2S, Z8, Komodo, la gamma dinamica di C70, Z8/Z6 III, X-H2S, Komodo, Pocket 6K, FX3/FX30, iPhone
16/17 e DJI. Le pagine CineD in inglese non erano leggibili: i numeri vengono dai mirror `cined.com/jp`.

## In Resolve

Usa un secondo nodo Log MetaRaw, copia di quello che sviluppa (stesso EI), messo **prima** di lui così riceve l'immagine
log, con Maschera accesa; collega la sua uscita alpha (key) all'ingresso key del nodo con Neat Video. Il nodo che
sviluppa resta con Maschera *Spenta*. *Vedi maschera* mostra la maschera in grigio per controllarla; la riga *Rilevato*
dice «NODO MASCHERA RUMORE: immagine invariata, solo alpha». Il manuale (18.6, OFX nella pagina Color) dice che
finestra, qualifier o key limitano l'effetto di un plugin OFX. **Non provato**: che l'alpha del nodo maschera arrivi
all'ingresso key del nodo con Neat Video. Se non arriva: un nodo in parallelo con *Vedi maschera* e un qualifier di
luminanza.

## Fonti

- ARRI, dynamic range white paper (tabella per EI): https://www.arri.com/resource/blob/295460/e10ff8a5b3abf26c33f8754379b57442/2022-09-28-arri-dynamic-range-whitepaper-data.pdf
- CineD: [FX6 ProRes RAW vs XAVC-I](https://www.cined.com/jp/sony-fx6-lab-test-external-prores-raw-vs-internal-xavc-intra/),
  [A7S III](https://www.cined.com/jp/sony-a7s-iii-lab-test-does-it-live-up-to-the-hype/),
  [A7 IV](https://www.cined.com/jp/sony-a7-iv-lab-test-rolling-shutter-dynamic-range-and-latitude/),
  [S5](https://www.cined.com/jp/panasonic-lumix-s5-lab-test-rolling-shutter-dynamic-range-and-latitude/),
  [S5 II](https://www.cined.com/jp/panasonic-lumix-s5-ii-lab-test-rolling-shutter-dynamic-range-and-latitude/),
  [GH6](https://www.cined.com/jp/panasonic-lumix-gh6-lab-test-rolling-shutter-dynamic-range-and-latitude/),
  [iPhone 17 Pro](https://www.cined.com/lab-test-of-the-iphone-17-pro-rolling-shutter-dynamic-range-trials-and-exposure-challenges/)
- FX30 ISO nativi: https://www.xdcam-user.com/2022/12/testing-the-sony-ilme-fx30
- A7S III doppio guadagno: https://briansmith.com/dpreview-measures-sony-a7s-iii-dual-gain-iso/
- Pocket 6K: https://www.provideocoalition.com/the-review-of-the-blackmagic-design-pocket-cinema-camera-6k/amp/
- Apple Log: https://prolost.com/applelog

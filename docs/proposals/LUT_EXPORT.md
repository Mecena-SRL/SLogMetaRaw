# Proposta #13 — Esportazione LUT

Stato: proposta + prototipo (`tools/export_lut.py`, test in `tests/test_export_lut.py`). Da decidere: i punti in fondo.

## Cosa esiste già
- Il nodo S-Log MetaRaw è dichiarato **puntuale** (`kPointwiseLutSafe`): dentro Resolve, **Generate 3D LUT** lo include già
  (vedi README). Il Detail è spaziale e resta fuori, per costruzione.
- Tutta la matematica del nodo è per pixel (`sm_develop`, `ofx/SLogMetaRaw/math/Develop.h`): decode → eventuale fix del
  data level → esposizione → bilanciamento del bianco (Bradford) → toni/zone → conversione spazio/gamma. Quindi una LUT 3D
  la rappresenta senza perdite oltre al campionamento.

## Cosa manca
Una LUT **fuori da Resolve**, senza aprire il progetto, a partire dai valori di una clip (Kelvin/tint/EI di ripresa) e dai
controlli scelti. Per esempio da script o da riga di comando, per monitor di riferimento, altri software, altri operatori.

## Prototipo
`tools/export_lut.py` campiona una griglia N³ **con lo stesso codice compilato del nodo** (`tests/ofx_math_test`, compilato
al volo con `c++`/`g++`/`clang++`, funziona anche su Linux) e scrive un `.cube`. Nessuna seconda implementazione della
matematica: la parità è per costruzione. Il test confronta il risultato con il modello Python indipendente
(`tests/develop_model.py`).

```
python3 tools/export_lut.py look.cube --size 33 --out-space Rec.709 --out-gamma Rec.709 \
    --shot-k 5600 --shot-ei 800 --k 4300 --tint 8 --ei 1600 --set toneContrast=25
```

Scelte del prototipo: ingresso = codifica log in ingresso al nodo (S-Gamut3.Cine/S-Log3 per default), indice rosso più
veloce, `DOMAIN` 0–1, valori in uscita non limitati (le alte luci non compresse superano 1: con *Soft Clip* o *Highlights*
rientrano). Lasciati fuori: Detail, viste false color, fix automatico del data level (dipende dalla clip).

## Risposte proposte alle domande dell'issue
| Domanda | Proposta | Perché |
|---|---|---|
| Formato | `.cube` 3D, 33³ di default (65³ opzionale) | lo leggono tutti; 1D da sola non regge WB/saturazione |
| Campionamento | sul segnale log in ingresso, non scene-linear | gli S-Log hanno distribuzione adatta a una griglia uniforme, e coincide con ciò che riceve il nodo |
| Toni | inclusi (sono per pixel) | altrimenti il look non esce dal nodo |
| Detail | **escluso**, dichiarato nel file e nell'interfaccia | è spaziale: nessuna LUT lo rappresenta |

## Passi successivi (nell'ordine)
1. Decidere se basta la CLI o serve un pulsante nello script di Resolve (userebbe i valori letti dalla clip e i controlli
   del nodo corrente: oggi lo script non legge i controlli del nodo, quindi sarebbe lavoro nuovo).
2. Se serve, aggiungere a `slogmetaraw` un comando `--export-lut` che prende i valori dal record della clip
   (`plugin_cache`) invece che dalla riga di comando.
3. Verifica di parità sul plugin vero: confrontare la LUT con **Generate 3D LUT** di Resolve sullo stesso nodo (richiede
   macOS e Resolve).
4. Sul Mac: nessun cambiamento al plugin, quindi nessun rischio per Metal.

## Aperto, a te
- CLI sufficiente, o pulsante nello script?
- 33³ come default va bene?

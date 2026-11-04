# Forum Blackmagic: thread pertinenti e bozze di risposta (3 novembre)

Stato: **non pubblicato**. Rispondere sul forum richiede un account Blackmagic e l'invio di testo a un servizio esterno:
le bozze qui sotto vanno riviste e postate a mano. La ricerca web è parziale (nessun accesso diretto al forum).

## Thread trovati (la gente chiede esattamente ciò che il plugin fa)
| Tema | Thread |
|---|---|
| FX30 in MP4 senza controlli RAW; workaround: transcodifica in XAVC-I MXF con Catalyst Browse | <https://forum.blackmagicdesign.com/viewtopic.php?p=1148411> e <https://forum.blackmagicdesign.com/viewtopic.php?p=1148309> |
| Problemi con la funzione RAW per XAVC-I dell'FX30 | <https://forum.blackmagicdesign.com/viewtopic.php?p=1010692> |
| Importazione XAVC HS 4K | <https://forum.blackmagicdesign.com/viewtopic.php?p=1148472> |
| Metadata Sony FX9 / a7S III | <https://forum.blackmagicdesign.com/viewtopic.php?p=651616> e <https://forum.blackmagicdesign.com/viewtopic.php?p=760663> |
| Importazione metadata (strumento esterno) | <https://forum.blackmagicdesign.com/viewtopic.php?p=709845> |

Sintesi, **riportata dai post degli utenti, non verificata da Blackmagic**: i controlli RAW Sony (18.5+) compaiono solo per
XAVC-I in MXF; gli MP4 di FX30/FX3/a7S III non li ricevono. Le soluzioni citate sono ricodifica o inserimento manuale dei tag.

## Bozza di risposta (EN) per i thread FX30/FX3 in MP4
> There is another route that doesn't transcode anything: S-Log MetaRaw (free, GPL-3, macOS; Linux/Windows experimental)
> reads the acquisition metadata Sony writes into XAVC MP4/MXF (white balance in Kelvin, tint, EI/ISO, lens, S-Log
> profile) into the Media Pool, and a Color-page OpenFX node develops the clip from those values in linear light.
> Original files are never modified. Resolve 20 and 21. https://github.com/Mecena-SRL/SLogMetaRaw
> (Disclosure: I'm one of the authors. It is not raw: it is a careful colour-science treatment of log footage.)

## Bozza di richiesta a Blackmagic (feature request)
> Please consider reading Sony XAVC MP4 acquisition metadata (colour profile, EI/ISO, white balance Kelvin/tint, lens,
> aperture) natively and exposing the existing Sony RAW-style controls for FX30/FX3/a7 series MP4 files, as already done
> for XAVC-I MXF. The data is present in the files' non-real-time metadata; an open-source reference implementation of the
> parsing and of the as-shot development is at https://github.com/Mecena-SRL/SLogMetaRaw (GPL-3), usable as a specification.
> Also useful in the scripting API: an editable per-clip "As Shot" white balance/ISO and a way to attach OpenFX defaults.

Nota: la bozza non promette che Blackmagic integri il codice (GPL-3: la licenza va rispettata in caso di riuso).

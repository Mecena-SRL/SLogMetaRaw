# Wiki corretta e multilingua (da pubblicare)

Questi file sono la versione pronta da copiare nella wiki GitHub del progetto (`<repo>.wiki.git`), che continua ad avere
il problema descritto nella #35 (`Wiki-Software.md` con tre copie sovrapposte e HTML incollato in chiaro, `Home.md` con
link doppi malformati `[[testo](url)](url)`, nessuna traduzione) — confermato di nuovo il 5 ottobre clonando
`https://github.com/Mecena-SRL/SLogMetaRaw.wiki.git`: il problema è ancora lì, non è mai stato pubblicato il fix.

**Contenuto, aggiornato al 5 ottobre:**
- `Home.md` / `Home-it.md` / `Home-es.md` / `Home-pt.md` / `Home-zh.md` — pagina iniziale, con selettore di lingua, una
  sola copia pulita per lingua, URL aggiornati a `Mecena-SRL/SLogMetaRaw`.
- `Wiki-Software.md` — guida utente in inglese, aggiornata alla **2.3.0** (prima era alla 2.0.1), con una sezione nuova
  (§15, "Linux and Windows (experimental)") che non c'era nella versione precedente di questo file.
- `Wiki-Software-it.md` / `-es.md` / `-pt.md` / `-zh.md` — la stessa guida nelle altre 4 lingue del progetto, adattata
  dalle rispettive traduzioni di `README.*.md` più il materiale solo-wiki (ricette, casi speciali, FAQ) tradotto a parte.

**Perché non è ancora pubblicata.** Le wiki GitHub non hanno un'API "contents" come il repository principale: si
modificano solo clonando e pushando `<repo>.wiki.git` con credenziali git dirette. Questa sessione può clonare quel
repository (è pubblico in lettura) ma **non può pusharlo**: il proxy di rete nega l'accesso in scrittura perché
`Mecena-SRL/SLogMetaRaw.wiki` non è nell'elenco dei repository autorizzati per questa sessione (solo
`ivan-94m/SLogMetaRaw`, cioè il repository principale, lo è). Serve una sessione con quel repository nell'elenco, o un
intervento manuale.

**Per pubblicarle:**

```
git clone https://github.com/Mecena-SRL/SLogMetaRaw.wiki.git
cp docs/wiki/*.md SLogMetaRaw.wiki/
cd SLogMetaRaw.wiki && git add -A && git commit -m "Wiki: guida ripulita, multilingua, versione 2.3.0" && git push
```

Dopo il push, chiudere/aggiornare la #35 (il problema che descrive sarà risolto) e segnalare qui se qualcosa non torna
nelle traduzioni — sono state scritte da un'unica sessione automatica, senza revisione madrelingua.

# Wiki corretta (da pubblicare)

`Home.md` e `Wiki-Software.md` sono la versione ripulita delle pagine della wiki GitHub (#35): una sola copia della guida
(prima ce n'erano tre sovrapposte, con HTML incollato), link senza la doppia parentesi `[[testo](url)](url)`, versione 2.1.2,
URL del repository aggiornati a `Mecena-SRL/SLogMetaRaw`.

Le wiki GitHub si modificano solo da `<repo>.wiki.git`; da qui il push è stato negato dal proxy. Per pubblicarle:

```
git clone https://github.com/Mecena-SRL/SLogMetaRaw.wiki.git
cp docs/wiki/Home.md docs/wiki/Wiki-Software.md SLogMetaRaw.wiki/
cd SLogMetaRaw.wiki && git commit -am "Wiki: guida ripulita, link e versione 2.1.2" && git push
```

Non incluso: traduzioni della wiki (README in 5 lingue); il testo della guida resta in inglese. Il numero di versione e le
descrizioni sono quelli della 2.0.1 aggiornati solo nel numero: la guida non è stata riletta riga per riga contro le
novità 2.1.x.

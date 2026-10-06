# Wiki mirror (sorgente da cui si pubblica la wiki GitHub)

Questi file sono la sorgente di verità per `https://github.com/Mecena-SRL/SLogMetaRaw/wiki`, tenuta nel repository
principale perché le wiki GitHub non hanno pull request: vivono in un secondo repository git,
`<repo>.wiki.git`, che non è coperto dalle normali revisioni di codice. Ogni pagina qui dentro corrisponde a una
pagina della wiki con lo stesso nome (senza `.md`).

**Pagine (6 ottobre 2026 — versione 2.3.0, correzione della #35: niente più le tre copie sovrapposte di HTML
incollato, link singoli invece di `[[testo](url)](url)`, 5 lingue come il README):**

| Pagina wiki | File |
|---|---|
| Home | `Home.md` / `Home-it.md` / `Home-es.md` / `Home-pt.md` / `Home-zh.md` |
| User Guide | `Wiki-Software.md` / `Wiki-Software-it.md` / `Wiki-Software-es.md` / `Wiki-Software-pt.md` / `Wiki-Software-zh.md` |

Solo la guida utente è tradotta in tutte le lingue; il testo tecnico dei deep dive (`docs/TONE_MAPPING.md` ecc.) resta
in italiano in ogni versione, come per il resto del progetto.

## Come ripubblicare dopo una modifica qui

```bash
git clone https://github.com/Mecena-SRL/SLogMetaRaw.wiki.git
cp docs/wiki/Home*.md docs/wiki/Wiki-Software*.md SLogMetaRaw.wiki/
cd SLogMetaRaw.wiki && git commit -am "Wiki: <cosa è cambiato>" && git push
```

Aggiornare questi file a ogni release che cambia qualcosa descritto nella guida (versione, requisiti, nuovi
controlli), non solo quando qualcuno segnala un problema sulla wiki.

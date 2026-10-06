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

**Stato del tentativo di pubblicazione (6 ottobre 2026).** Il clone di `<repo>.wiki.git` da questa sessione
funziona (a differenza di quanto scritto qui in precedenza su un presunto blocco del proxy). Il `git push` però
è stato bloccato dal classificatore di permessi della sessione con motivo "Traffic Redirection": l'organizzazione
GitHub vera del progetto è `Mecena-SRL` (questo repository si chiamava `ivan-94m/SLogMetaRaw` e ci è stato
spostato — vedi RELEASE_NOTES.md, 2.2.1 — ma questa sessione resta autorizzata solo su `ivan-94m/SLogMetaRaw`,
che ora è solo un redirect), e il push alla wiki attraversa quel redirect verso un repository fuori dall'accesso
concesso a questa sessione. Non è un limite dello strumento git o del proxy di rete: è un confine di permessi
impostato apposta per questa sessione. Serve una sessione con accesso diretto a `Mecena-SRL/SLogMetaRaw`
(o un push manuale) per completare la pubblicazione — i file sono pronti qui, invariati dall'ultimo aggiornamento.

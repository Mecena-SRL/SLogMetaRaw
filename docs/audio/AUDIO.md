# Log MetaRaw Audio — simulatore di registrazione e mix d'epoca

Gemello sonoro del Creative: come si dice «girato in Super 8 su Kodachrome», si dice «tracce registrate col Geloso
G.257 a 4,75 cm/s col suo microfono piezo, mix su EMI REDD.51, stampato su 78 giri, ascoltato da una radio a valvole».

## Formato

OFX non elabora audio: in Resolve serve un plugin per Fairlight. Destinazione: **VST3** (macOS e Windows) e **AU**
(macOS), scritti con l'SDK VST3 di Steinberg (licenza MIT dalla 3.8, ottobre 2025), senza JUCE. Linux: Resolve non
carica plugin VST nativi, quindi resta escluso finché non lo fa.

## Due plugin, un motore

| Plugin | Dove va | Catena |
|---|---|---|
| **Ripresa** | tracce microfono | Microfono → Preamplificatore → Supporto (con wow e flutter, rumori, riduzione rumore) |
| **Mix e supporto** | bus e master | Banco → Supporto → Riversaggi (copie) → Ascolto |

Il motore (`audio/src/AudioEngine.*`) non dipende dall'host: i wrapper spostano solo campioni e parametri. Ogni stadio
ha un interruttore (`AuSettings::on`); uno stadio spento o su «Nessuno» lascia passare i campioni invariati (verificato
bit per bit). Regolazioni comuni: Saturazione (×), Rumore (dB), Wow e flutter (×), Dry/wet, Uscita.

## Preset (`audio/src/AudioPresets.h`)

Tabelle come nel Creative: l'ordine è permanente (il progetto salva l'indice), si aggiunge in coda.

- **Dispositivo › Modello** (Ripresa): bobine portatili (Geloso G.257, Nagra III, Uher 4000 Report-L, Nagra 4.2),
  bobine da studio (Ampex 350, Studer J37, Revox A77, Studer A80), cassetta (Philips EL 3300, Tascam Portastudio 144,
  Walkman Pro WM-D6C, Sony TC-D5M), cinepresa e videocamera (Auricon Cine-Voice 16 mm, Kodak Ektasound 130,
  Canon 514XL-S, JVC GR-C1, Sony CCD-V8), prima del nastro (fonografo Edison, registratore a filo Webster-Chicago).
  Ogni modello porta microfono di serie, supporti compatibili, preamplificatore, ronzio di rete, riduzione rumore.
- **Microfono**: di serie, tromba acustica, carbone, piezo, RCA 44-BX e 77-DX, Shure 55, Neumann U 47, AKG C12,
  EV 635A, MD 421, SM58, MKH 416, electret.
- **Supporto**: nastro ¼" a 38/19/9,5/4,75 cm/s, cassetta Tipo I/II/IV, filo, cilindro, 78/45/33 giri, VHS lineare
  SP/LP e Hi-Fi, Video8, U-matic, banda ottica 35 e 16 mm, pista magnetica Super 8, radio AM/FM, telefono.
- **Riduzione rumore**: Dolby A/B/C, dbx (per ora come riduzione del soffio, non come compander).
- **Banco**: RCA BC-2B, EMI REDD.51, Neve 8078, SSL 4000 E, Mackie CR-1604.
- **Ascolto**: radio a valvole, radiolina a transistor, juke-box Wurlitzer 1015, TV CRT, sala (curva X).

**Stato dei valori:** stime parametriche da specifiche pubblicate e descrizioni d'epoca [gen], non misure dei prodotti.
Rumori in dBFS RMS dopo la banda del supporto; wow e flutter come errore di velocità di picco in %.

## Stato

- Implementato e verificato da test (`tests/audio_test.cpp`, `tests/test_audio.py`): motore, tabelle, bypass esatto,
  determinismo indipendente dalla dimensione del blocco, banda del telefono, risonanza piezo, soffio per velocità e
  Dolby, wow misurato sugli attraversamenti dello zero, supporti mono, perdita di acuti per copia, dry/wet e uscita.
- Pianificato: wrapper VST3/AU con pannello a menu contestuali (fase 2), build in CI e installer, prova in Fairlight e
  taratura su registrazioni vere (fase 3), ricerca per modello come `docs/creative/` (fase 4). Dolby come compander,
  isteresi del nastro, distorsione di tracciamento del vinile, sibilanti della banda ottica.

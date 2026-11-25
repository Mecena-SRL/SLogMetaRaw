# Log MetaRaw — User Guide

**English** · [Italiano](Wiki-Software-it) · [Español](Wiki-Software-es) · [Português](Wiki-Software-pt) · [简体中文](Wiki-Software-zh) · [Home](Home)

> Version **4.3.1** · macOS 12+ · DaVinci Resolve 20 / 21 · Sony XAVC `.MP4` / `.MXF`
> Tested only on DaVinci Resolve Studio 21.1 on macOS.

Log MetaRaw reads the shooting data Sony cameras write into every file (Kelvin, tint, EI, lens, aperture, shutter, colour profile) and uses it inside DaVinci Resolve. Resolve does this only for FX6/FX9 MXF files; for MP4 from FX30, FX3, a7 and a6000 series it ignores that data.

**Contents**

1. [What it is — and what it is not](#1-what-it-is--and-what-it-is-not)
2. [Install](#2-install)
3. [Quick start](#3-quick-start)
4. [The script](#4-the-script)
5. [The Log MetaRaw node](#5-the-s-log-metaraw-node)
6. [False colour](#6-false-colour)
7. [Tones and Zones](#7-tones-and-zones)
8. [The Log MetaRaw Creative node](#8-the-log-metaraw-creative-node)
9. [Pipelines: where each node goes](#9-pipelines-where-each-node-goes)
10. [Recipes](#10-recipes)
11. [Special cases](#11-special-cases)
12. [Updates and privacy](#12-updates-and-privacy)
13. [Uninstall](#13-uninstall)
14. [Troubleshooting / FAQ](#14-troubleshooting--faq)
15. [Linux and Windows (experimental)](#15-linux-and-windows-experimental)
16. [Known limits](#16-known-limits)

---

## 1. What it is — and what it is not

Three tools, one install:

| Tool | Where in Resolve | What it does |
|---|---|---|
| **Script** | Workspace › Scripts › Log MetaRaw | Reads the metadata of every clip and writes it into the Media Pool. Fixes each clip's Data Level. |
| **Log MetaRaw** node | Color › OpenFX — **first node** | Develops a clip from its as-shot values: white balance, exposure, colour space, tones by zones, false colour. |
| **5 · Log MetaRaw Creative** node | Color › OpenFX | creative development: 16 photographic processes (film negatives and prints, reversal, black and white, Technicolor, Autochrome, three plates, daguerreotype, collodion, early prints), analog video and vintage digital cameras; prints and print generations; presets by device and model (film cameras, tube and CCD video cameras, digital cameras and phones), 14 vintage lenses, transfers (telecine, kinescope, film-out) and a viewing medium (cinema, projectors, CRT and Trinitron TVs, plasma, early LCD, handheld, paper) |

Original files are **never modified**: no transcoding, no rewrapping.

**It is not raw.** A log MP4 is already demosaiced and compressed (8 or 10 bit, often 4:2:0, with in-camera noise reduction). The node applies colour science rigorously — exposure and white balance in linear light, from the recorded values, with Sony's published curves and gamuts — so the image *behaves* in a way that reminds you of raw: white balance shifts cleanly, exposure moves like a stop of light, highlights roll off instead of breaking. Push past the camera's limits and the missing information shows: banding in skies, noise in lifted shadows, clipped highlights stay clipped. Expose well on set.

---

## 2. Install

1. Download `SLogMetaRaw-3.0.0.dmg` from [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) and open it.

   ![The DMG window](images/01-dmg.png)

2. Double-click **Installa Log MetaRaw.pkg**. The package is not signed with an Apple certificate: the first time, **right-click › Open**. It asks for your Mac password because the plugin goes in a system folder.

   ![Right-click › Open on the unsigned package](images/02-gatekeeper.png)

3. **Restart DaVinci Resolve.**

The installer also deletes Resolve's plugin cache (`OFXPluginCacheV2.xml`); Resolve rebuilds it at the next launch. Without that, Resolve would show the old panel and would not see the Creative node.

Every install starts clean: the previous plugin and library are replaced entirely, and development installs are removed.

**What goes where**

| Item | Path |
|---|---|
| Both nodes (one bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Python library | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Menu script | `…/DaVinci Resolve/Fusion/Scripts/Utility/Log MetaRaw.py` |
| Per-clip cache (JSON) | `~/Library/Application Support/SLogMetaRaw/cache` |

**Requirements:** macOS 12 or later, Apple silicon or Intel, DaVinci Resolve 21 or 20 (Resolve 20: install Python 3 from python.org, it has none of its own).
**Clips:** Sony XAVC in `.MP4` or `.MXF`, plus the cameras in [docs/CAMERAS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/CAMERAS.md) (Panasonic V-Log, Apple Log, DJI D-Log and D-Log M and more), each with how reliable its reading is. The script reads any of them. The nodes develop **S-Log3** (S-Gamut3.Cine or S-Gamut3), **S-Log2**, **S-Log** (S-Gamut) and the curves of those cameras. Since 3.2.0 DJI D-Log M uses the curve measured on an Osmo Action 6 ([docs/DJI_LOG.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DJI_LOG.md)): for other DJI cameras it is the best estimate, not a calibration. With an unknown profile they stay neutral and say so.

---

## 3. Quick start

1. Import the footage. Open **Workspace › Scripts › Log MetaRaw**, press **1 · Read metadata**, then **2 · Write to Resolve**.
2. On the Color page, add **Log MetaRaw** as the **first node**. It takes the clip's EI, Kelvin and tint: at those values it changes nothing.
3. Correct white balance and exposure in the node, using the false colour views.
4. Shape the tones with **Toni** (Tones). For the look of a film stock, an early process, a tape or a vintage camera,
   add **Log MetaRaw Creative** as the next node.
5. Then the rest of the grade and, last, your output CST / LUT / DRT.

```
Log MetaRaw  →  Log MetaRaw Creative  →  rest of the grade  →  output CST / LUT / DRT
```

![Recommended node tree](images/08-node-tree.png)

> Copy the node onto another clip and it resets itself with that clip's data.

---

## 4. The script

**Workspace › Scripts › Log MetaRaw**

![Workspace › Scripts menu](images/03-scripts-menu.png)

The window has a row of buttons, a row of options and the clip list. It follows Resolve's language (English, Italian, Spanish, Portuguese, Simplified Chinese). The script reads files; it never modifies them.

![Script window after Read metadata](images/04-script-window.png)

### Buttons

| Control | What it does |
|---|---|
| **Clip menu** | *Whole Media Pool* or *Selected clips in the Media Pool*. |
| **1 · Read metadata** | One row per clip: camera, lens, aperture, shutter, EI, WB, colour space, data level. The **Status** column says `read`, what changed during the take (aperture, focus…), or why a clip was skipped. Click a row to see everything that was read, grouped as in Catalyst Browse. |
| **2 · Write to Resolve** | Fills the Media Pool fields (Metadata panel, columns, keywords for smart bins, Camera Notes, data burn-in) and fixes values Resolve reads wrong from MXF, e.g. *Camera Aperture* `F53343` on the FX6. |
| **Export CSV** | Exports the values Resolve has no field for (EI, tint, WB mode, focus distance, capture gamma…) in Resolve's metadata CSV format. Import with **File › Import › Metadata**, with *create custom fields* on. |
| **Version** (bottom right) | Click to check GitHub; turns green when a newer release exists. |

![Clip detail, Catalyst-style](images/05-script-clip-detail.png)

### Options

| Option | Default | Effect |
|---|---|---|
| **Camera tag** | off | Adds camera, gamma and primaries to the keywords. |
| **Overwrite metadata** | on | Replaces values Resolve already wrote. Off: fills only empty fields. |
| **Correct the Data Level** | on | Sets each clip's *Data Level* to **Full** (log) or **Video** (709, Cine, HLG). This is the real fix: it holds for the whole project, scopes and exports included. |
| **Also set Input Color Space** | off | For colour-managed projects. ⚠️ A script cannot set it back to *Project* — only you can, by hand. |

After **2 · Write to Resolve**, the data shows up in the Media Pool:

![Media Pool Metadata panel filled](images/06-media-pool-metadata.png)

**Speed.** Nothing is decoded: at most 24 samples of the metadata track per clip, within one second. A multi-GB file costs about 100 KB of reading. A disk that stops answering is skipped once, with a message, instead of blocking the list. Running the script right after import means the nodes find the data ready.

---

## 5. The Log MetaRaw node

**Color › OpenFX › Log MetaRaw** — first node, before any CST or LUT.

![OpenFX library with both nodes](images/07-openfx-library.png)

The node is **pointwise**: each pixel depends only on itself. It never makes halos, and **Generate LUT** can export it (65 points recommended). Settings are saved per clip.

> The nodes' panels are in Italian. Labels below come with a translation.

![Main node panel — top](images/09-node-panel-top.png)

| Control | What it does |
|---|---|
| **Version** (top) | Shows `v3.2.0`. Once a day it asks GitHub for the latest release; if there is one it reads **🟢 v3.2.0 → 3.x.y** and a click opens the download of the installer for your system (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). It never installs anything itself. |
| **Camera** · **Rileggi metadata** (re-read) | The camera that was read. *Rileggi* re-reads the clip, resets every control to the camera values and writes the clip's metadata into the Media Pool. Answers within ~2 s. |
| **Ripresa** (shot) · **Decodifica** | *Clip* lets you change the controls; *Camera metadata* locks them to the shot values |
| **Bilanciamento** (balance) · **Temperatura** · **Tint** | As shot or a preset. Bradford chromatic adaptation in linear light, from the white the camera recorded |
| **Punto bianco** (white point) | Resolve's eyedropper: pick a point of the image that should be neutral and Temperatura and Tint set themselves, with the same maths as the render |
| **Esposizione** (EI) | exposure index: double the EI = +1 stop |
| **False color** | a box above Temperatura, one below Tint and one below Esposizione; see below |
| **Spazio colore** (colour space) | three sub-groups. **Ingresso** (input): Spazio colore and Gamma entering the node, Rilevato (detected) and *Ripristina da camera*, Data level, Livello and *Ripristina da clip*. **Senza metadata**: *Sblocca controlli* and the shooting references. **Uscita** (output): **Tone mapping** (Nessuno/CST, ACES 2.0, Luminanza, Filmico, ACES 1.3, OpenDRT), Compressione and Mappatura gamut, Vibrance luci, Spazio colore and Gamma (like a Color Space Transform; *Timeline* does not convert), LUT size and export |
| **Toni** (tones) | Highlights, **Bianco** (stop), Soft Clip, Saturation (−100…+100). *Mostra tutti i controlli* adds Contrast, Shadows, Whites, Blacks, Vibrance and the **Zone** (Black, Shadow, Light, Specular with Exp, Sat, Range and Falloff, Contrast Pivot, *Zone* false color); they appear by themselves when the node uses them |
| **Dati di ripresa** (Shooting data) | Read only: lens, focal length, aperture, focus, shutter, EI, WB, fps, ND, camera LUT. |

![Avanzate and Dati di ripresa](images/14-avanzate-dati.png)

**Spazio colore in detail**

- **Ingresso › Spazio colore / Gamma** (node input) — *Automatico* asks Resolve or reads the camera; *Ripristina da camera* brings both back to it. **Rilevato** shows what the node reads.
- **Ingresso › Data level** — fixes the code-value scale when Resolve decodes a clip on the wrong one. **Livello** shows what was read, *Ripristina da clip* goes back to Automatico. The script's *Correct the Data Level* fixes it project-wide.
- **Ingresso › Stato** — what the node detected; it also warns when a false colour view is on.
- **Senza metadata › Sblocca controlli** — see [§11](#11-special-cases).

> **Rec.709 output** in the node with Tone mapping *Nessuno (CST)* is a plain CST (highlights past Bianco clip), and any output conversion shuts out the Creative node. Prefer an output CST at the end of the tree.

**Tone mapping with a scene output (3.2.0).** With a rendering (ACES 2.0, Filmico, OpenDRT…), a scene Spazio colore and Gamma such as DaVinci WG / DaVinci Intermediate are no longer replaced by Rec.709 / Gamma 2.4: the image rendered for Rec.709 travels in that encoding, to hand it to other plugins (Dehancer…), and a CST to Rec.709 / Gamma 2.4 without tone mapping brings it back identical. Display outputs (Rec.709, P3, Rec.2020) are unchanged.

---

## 6. False colour

One view per control, placed next to the slider it serves (above Temperatura, below Tint and below Esposizione). **The view replaces the image — turn it off before you render.** (An OpenFX plugin cannot draw an overlay on Resolve's viewer.)

Set *Decodifica* to **Clip** first: in *Camera metadata* the controls are locked.

### Exposure

ARRI-style bands in stops around 18% grey. Everything else turns grey.

![Exposure bands](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/falsecolor_bands.png)

| Colour | Meaning |
|---|---|
| Green | Middle grey (18%) |
| Pink | One stop over — skin |
| Yellow | Near clip |
| Red | Clipped |
| Blue / purple | Deep shadow / black |

Turn the view on, aim at middle grey or skin, move **Exposure** until the right area turns green (grey) or pink (skin).

![Exposure false colour in the viewer](images/10-falsecolor-exposure.png)

### Temperature and Tint

They work as in CineMatch. The picture turns grey; casts show in colour, and near-neutral casts are boosted up to 8× so they are visible.

| View | You see | Do |
|---|---|---|
| Temperature | Blue (cool cast) | Raise Color Temp |
| Temperature | Orange (warm cast) | Lower Color Temp |
| Tint | Green | Raise Tint |
| Tint | Magenta | Lower Tint |

Pick a surface that should be neutral and move the slider until it stays grey. Alternate Temp and Tint a couple of times; it converges. Each view reacts only to its own slider.

![Temperature false colour](images/11-falsecolor-temp.png)

Deep dive (Italian): [docs/FALSE_COLOR.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/FALSE_COLOR.md)

---

## 7. Tones and Zones

### Toni (Tones)

![Toni group](images/12-toni.png)

All sliders run −100…+100, in Camera Raw order. Highlights is a film shoulder; the others are **exposures over a zone of tones, in stops from 18% grey**: inside the zone the image moves as with an exposure, so texture is kept. No combination of sliders can solarise.

| Slider | Behaviour |
|---|---|
| **Contrast** | Turns around the Pivot. +100 doubles the slope at the pivot, −100 halves it, with bounded ends. |
| **Highlights** | Negative: film shoulder — at −100 the brightest recorded value (~+6 stops over grey in S-Log3) lands exactly on **Bianco**, no grey veil, no clip. Grey and below stay put; skin at +1 stop moves 0.05 stop at most. Hue is kept constant. Positive: more snap. |
| **Bianco** (white, stops) | Where that maximum lands, and Soft Clip's roof. **2.5** = Rec.709 white through a CST without tone mapping. **With a DRT after (ACES, AgX, DaVinci) raise it to 4–5**, or highlights get compressed twice. |
| **Shadows** | Tones below −1 stop; 100 = 2 stops. Lifts black too — hold it with Blacks. |
| **Whites** | From +3.5 stops to clip; 100 = 1 stop. |
| **Blacks** | Linear veil: moves black (−3 / +1 stop) without moving grey. |
| **Vibrance** | Around luminance; protects skin tones. |
| **Saturation** | Around luminance; same in every colour space. |

**Azzera toni** resets the group.

![Highlights 0 vs −100](images/15-highlights-before-after.png)

![Tone curve](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/tone_curve.png)

> **The honest cost of a pointwise node:** whatever it compresses, it compresses the texture inside it too.

### Zone (Zones)

![Zone group](images/13-zone.png)

- Four zones, as in Resolve's HDR palette: **Black, Shadow, Light, Specular** — each with **Exp** (stops), **Sat**, **Range** (edge, in stops) and **Falloff** (transition width).
- **Contrast Pivot** — the tone Contrast turns around.
- **Soft Clip** / **Soft Clip Color** — folds highlights under a roof (at Bianco) they never reach. *Color* decides whether folded highlights go to white (film) or keep their colour. Containment, not recovery.
- **Zone false colour** — colours each pixel with the zone that moves it.
- **Azzera zone** resets the group.

Zone edges are carried through Contrast, so they stay in scene stops whatever Contrast you use.

### Generate LUT

The node is pointwise, so Generate LUT includes it. Use **65 points**: with ±100 sliders the error stays within ~3.5 S-Log3 code values. At 33 points the S-Log3 linear toe (Shadows +100) reaches ~10 CV. Some extreme Zones (Black or Shadow +3) exceed that even at 65 — keep the node live for those grades.

Deep dive (Italian): [docs/TONE_MAPPING.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/TONE_MAPPING.md)

---

## 8. The Log MetaRaw Creative node

One engine that redoes **the process**, not an imitation of its result. Scene light, decoded to linear Rec.2020,
becomes up to three **records** with their spectral sensitivity. Each is exposed on the material's curve and turned into
a picture the way that process did: subtractive dyes, additive filters, a silver or platinum deposit on paper, a video
signal. Every process is a table of numbers, and the panel shows only the controls that matter for the one you pick.

| Process | What it models |
|---|---|
| **Negativo colore e stampa** | three layers with their own contrast, toe and shoulder, interimage, grain per layer, halation, a positive print with R G B printer lights; 14 stocks, Eastern ones too (Sovcolor, ORWO, Svema, Gevacolor). *Uscita Log* gives the negative scan, in the input encoding |
| **Invertibile**, **Bianco e nero** | slide film; B&W with blue, ortho or pan sensitivity and a compensated taking filter |
| **Technicolor** three-strip and two-colour | three negatives from the prism, dye-transfer matrices, the K key until 1944; red and green only |
| **Autochrome**, **Dufaycolor** | an additive mosaic of dyed starch grains or a regular réseau, seen by transmission |
| **Tre lastre**, **Kinemacolor** | sequential exposures: with *Intervallo* > 0 the node reads the following frames and whatever moves separates into colours (Prokudin-Gorsky) |
| **Dagherrotipo**, **Collodio**, salt and albumen, cyanotype, platinum | blue and UV sensitive deposits, mirror and oxidation, support, border, toning |
| **Video analogico** | 11 formats, broadcast to VHS LP and Betacam SP: luma and chroma bandwidth, chroma delay, peaking, noise, head switching, dropouts |
| **Digitale d'epoca** | 10 cameras: clip and knee, electronic sharpening, noise, chroma subsampling, banding |

Controls, in groups: **Ripresa e sviluppo** (exposure, light balance, push/pull, retained silver, flash, age), **Stampa**
(printer points, contrast), **Grana e ottica** (grain, halation, diffusion, aberration, vignette, gate weave, flicker, dust,
scratches; in multiples of the material's and the **Formato**'s own, 8 mm to IMAX 15/70), **Separazioni**, **Mosaico**,
**Lastra e carta**, **Segnale**, **Colore** (split toning, saturation, colour density).

- Place it **right after** Log MetaRaw. It decodes what it receives to linear light and outputs Rec.709 · Gamma 2.4 (the
  projected print), or the input encoding with *Uscita Log*.
- It is **spatial**, so Generate LUT leaves it out. The radii follow the frame height: the same look in proxy. Noise is
  deterministic, so CPU and Metal give the same picture on every render.
- It replaces the Detail node in the same slot: a grade saved with the Detail opens as a fresh Creative.

The numbers are estimates from the research in [docs/CREATIVE.md](docs/CREATIVE.md), not product profiles: they need
tuning on real footage. Not tried in Resolve yet; Metal is checked by the macOS CI tests.

**New in 4.3.1.**

- **Audio, consistent choices**: in both audio plugins every menu offers only what fits the choices above it: the
  recorder's own medium, microphone and noise reduction first, then compatible ones (no VHS in a Nagra); in *Mix e
  supporto* the desk sets the master's period media and the master what it can be transferred to.
- **Gate hair, dust and scratches look real**: the hair caught in the gate tapers to its tip, has uneven density and a
  shadow blurred as on film (more on Super 8 and on prints); now and then it curls or is a two-strand fibre of lint.
  Dust specks have ragged edges and every size; scratches change width and strength along the line.
- **Scan with the maker's edge print**: edge to edge, the margin carries the brand (EASTMAN before 1990, then KODAK;
  FUJI, AGFA, ORWO…), the stock's code in that gauge, NITRATE FILM, SAFETY FILM or ESTAR, a running number and, from
  1990, KeyKode with its bar code; photo film numbers its frames. The scanner's light glows around the perforations
  and along the base's cut edge. New *Scan overscan* control.
- **Soundtrack on prints**: edge to edge, a 35 mm or 16 mm print shows its soundtrack at the SMPTE positions:
  the optical track (variable density, variable area, Dolby Stereo), from 1993 Dolby Digital between the
  perforations, DTS and SDDS, the cyan track from 2004; Super 8 and 16 mm can carry a magnetic stripe. New *Print
  soundtrack* menu (by era or by hand).

**New in 4.3.0.**

- **Stocks by gauge**: with a camera chosen, only the stocks made in its gauge show, from every maker and with that
  gauge's code (500T is 7219 in 16 mm); the full lists are one tick away.
- **Scan view**: the frame, the frame with the gate's edges, or the film edge to edge with perforations, frame lines
  and the frames beside, at each format's real dimensions. Gate square, rounded or filed open.
- **The negative as it is**: the colour negative on a light, complementary on its orange mask, with its own grain.
- **Film base**: nitrate, acetate or polyester (or by era and gauge) and its decay: nitrate's amber blotches,
  acetate's crystals and unsteadiness.
- **Realistic weave** in the camera, the printer and the projector, each with its own switch; **gate hair** by
  camera.
- **Consoles** as viewing media: NES, Master System, Mega Drive, Super Nintendo, PlayStation and Saturn on a tube TV.
- Fixed the crash when choosing the viewing media added in 4.2.0.

**New in 4.2.0.**

- **Device › model**: start from the machine (film camera, video camera, digital cinema, phone); it loads format, tape,
  sensor and faults; then maker and stock, with real names. The *Tone*, *Colour* and *Texture* switches let you use, for
  instance, only the grain on your own footage.
- **Emulated video**: VHS and Betacam tape rebuilt line by line (FM noise, late colour, head switching, dropouts) and
  the camera's sensor: tubes with lag and comet tails, CCD smear, the false colour of striped tubes.
- **Digital cameras by model**: sensor size and aliasing, in-camera sharpening, the codec's 8x8 blocks and subsampled
  colour (from the Motorola RAZR to the ARRI Alexa).
- **Transferred to**: the copy on VHS, Betacam, third-generation VHS, 16 mm kinescope or 35 mm film-out, grain on grain.
- **Lens**: 14 lenses (Cooke, Zeiss Super Speed, Canon K35, Panavision, LOMO, Helios, Petzval…) with distortion, soft
  edges, swirl, veiling glare and anamorphic streak.
- **New viewing media**: Super 8 projector, Trinitron, plasma, early LCD.

---

## The Noise, Stabilize and Motion Blur nodes (4.0.0)

They work on the camera image, before **Log MetaRaw**.

- **Log MetaRaw Noise** knows how much noise to expect, and where. It reads the camera, the ISO (or the EI and the
  native ISO), the codec, the bits per pixel and the 4:2:0 chroma from the script's record, with tuned profiles for the
  Sony FX6 and FX30, iPhone 17 Pro and DJI Osmo Action 4 and 5 Pro. It can also measure the noise from a frame with
  flat areas.
  - *Temporal*: ±1 to ±3 neighbour frames, aligned by blocks and weighted per pixel, so moving parts and cuts do not
    smear.
  - *Spatial*: luma and colour apart, in 5 wavelet bands. Each coefficient is weighed against the energy of its
    neighbourhood, which keeps texture.
  - Views of the removed noise, detail and skin protection, and **Salta Noise ovunque** to switch every Noise node off
    while you edit.
  - On a Mac it runs entirely on the Metal GPU, and the tests check that it matches the CPU. Elsewhere it runs on the
    CPU.
  - The profiles are estimates until they are tuned on real samples. [docs/DENOISE.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DENOISE.md)
- **Stabilize** reads the clip's gyro (built-in Gyroflow). Its styles go from micro-shake to crane, with a keyframeable
  **MASTER**. **Motion Blur** rebuilds the shutter from optical flow, for example 50p at 90° to 25p at 180°. Both come
  from Mecena Motion (`motion/`) and are installed by the macOS package, Apple silicon only.
- **Deep acquisition**, a tick in the script window: it stores the gyro and the lens of every frame in the MecenaClipDB
  for Stabilize.

None of the three nodes has been tried in Resolve on real clips yet.

## Log MetaRaw Audio (VST3, 4.2.0)

The Creative's twin for sound, in **Fairlight** (Mac and Windows): two plugins in one module, `Log MetaRaw Audio.vst3`.

- **Ripresa** (recording), on tracks: recorder (Geloso G.257, Nagra, Uher, Studer, Revox, Walkman, Auricon, Edison
  phonograph, wire recorder…), microphone (the recorder's own or RCA 44, U 47, C12, SM58, MKH 416…), medium and noise
  reduction.
- **Mix e supporto** (mix and medium), on buses and master: console (RCA, EMI REDD.51, Neve, SSL, Mackie), medium (tape,
  cassette, 78/45/33 rpm, VHS, optical track, radio, telephone), transfers and listening (valve radio, jukebox, TV,
  cinema).
- Every stage has a switch; Saturation, Noise, Wow and flutter, Dry/wet and Output set the result.
- In the installers it is a separate component, ticked by default. Linux: Resolve does not load native VST3.

The values are estimates from period specifications and descriptions ([docs/audio/AUDIO.md](docs/audio/AUDIO.md)); not
yet tried in Fairlight.

## 9. Pipelines: where each node goes

### A — Grade in DaVinci Wide Gamut (recommended)

```
Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → your grade (DWG)
  → output CST         DaVinci WG/Intermediate → Rec.709 · Gamma 2.4, DaVinci tone mapping
```

With tone mapping after, set **Bianco to 4–5**. **Creative**, if you use it, goes right after Log MetaRaw and outputs Rec.709 · Gamma 2.4 (or the input encoding with *Uscita Log*): in a DWG grade put it last, in place of the output CST.

### B — All in log

```
Log MetaRaw          Color Space/Gamma: Timeline
  → your grade
  → CST / LUT          S-Gamut3.Cine/S-Log3 → your output
```

### C — Colour-managed project (DaVinci YRGB Color Managed / ACES)

Leave Color Space/Gamma on **Timeline** in the main node. Enable the script's *Also set Input Color Space* only if you want the script to set each clip's input — remember you must set it back to *Project* by hand.

---

## 10. Recipes

| Problem | Where | Setting |
|---|---|---|
| Sky or windows burning | Main › Toni | Highlights −50…−100 (with a DRT after: Bianco 4–5) |
| Backlit face in shadow | Main › Toni | Shadows +30…+60, Blacks −20…−40 |
| More "film" look | Main › Toni | Contrast +20…+30, Highlights −60 |
| Noisy saturated shadows | Main › Zone | Shadow Sat −30…−50 |
| Specular reflections | Main › Zone | Specular Exp −1…−2 |

---

## 11. Special cases

**ProRes from an external recorder / unreadable clip.** The node stays neutral. Tick **Spazio colore › Senza metadata › Sblocca controlli** and type the shooting EI, Kelvin and tint: the controls come alive and the node starts neutral.

**Non-log profiles** (Cine, HLG, S-Cinetone): the nodes stay neutral and say so in *Stato*.

**Wrong Data Level.** If blacks look lifted or crushed straight out of the camera, Resolve is decoding on the wrong code-value scale. Run the script with *Correct the Data Level* on (project-wide fix), or use *Spazio colore › Ingresso › Data level* on a single clip. Background (Italian): [docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**Cameras without Kelvin** (e.g. a6300): Kelvin is estimated from the light preset and flagged.

**Performance.** Opening a project reads no files. Opening the panel waits at most half a second; a slow disk finishes in the background with a 15 s limit.

---

## 12. Updates and privacy

- The **nodes** ask GitHub for the latest release at most once a day, in the background. The request carries only the version (`User-Agent: SLogMetaRaw/3.0.0`).
- The **script** checks only when you click its version.
- A click only opens a download link from this project's GitHub releases. Nothing is installed without you.

![Update badge](images/18-update-badge.png)

**Turn the check off:** create the empty file

```bash
touch ~/Library/Application\ Support/SLogMetaRaw/no_update_check
```

**Updating from 1.x:** the 1.x tones (Highlights, Shadows, Contrast, Saturation, Color Boost, Color Recovery) cannot be converted and reset to zero. To keep a finished clip's look, **before updating** run Generate LUT on that node. After the update, *Stato* shows what was there (e.g. "Toni 1.1 azzerati (H −40, C +15)"). Exposure, data level, Color Space and Gamma are unchanged.

---

## 13. Uninstall

Double-click **Disinstalla Log MetaRaw.command** in the DMG (first time: right-click › Open). It:

1. lists everything it will remove (all 1.x and 2.x versions, the old *SonyMeta* name, dev installs, cache, settings, logs);
2. asks you to quit Resolve;
3. asks whether to delete exported CSVs too;
4. checks nothing is left.

Run with `--dry-run` to see what it would remove without touching anything. Metadata already written into Resolve projects stays — it is part of the projects.

![Uninstaller in Terminal](images/20-uninstaller.png)

---

## 14. Troubleshooting / FAQ

**The Creative node is missing / the panel looks old.**
Resolve is using its old plugin cache. Quit Resolve, delete `OFXPluginCacheV2.xml` (the installer normally does this), restart.

**The node does nothing.**
That is by design at the as-shot values. Check *Decodifica* is on **Clip**, and read *Spazio colore › Ingresso › Stato*: it says if the profile isn't log or metadata is missing.

**My render has false colour in it.**
A view was left on. Turn off all false colour views before rendering.

**Highlights look compressed twice / dull.**
You have a DRT (ACES, AgX, DaVinci tone mapping) after the node: raise **Bianco** to 4–5.

**The script skipped a clip.**
Read the *Status* column: it says why (unsupported file, disk not answering, etc.).

**"Camera Aperture F53343" on FX6.**
A Resolve misread of MXF; **2 · Write to Resolve** fixes it.

**macOS won't open the installer.**
It is unsigned: right-click › Open (or System Settings › Privacy & Security › Open Anyway).

---

## 15. Linux and Windows (experimental)

The plugin also builds for Linux and Windows with CMake. These ports render on the **CPU only** (no Metal, CUDA or
OpenCL yet) and have not been tested inside DaVinci Resolve itself: the macOS build remains the supported one.

**Linux** (download from [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases); pick what fits your distribution):

| File | For |
|---|---|
| `slogmetaraw_<version>_amd64.deb` | Ubuntu, Debian, Mint: `sudo apt install ./slogmetaraw_*.deb` |
| `slogmetaraw-<version>-1.x86_64.rpm` | Rocky, Alma, RHEL, CentOS, Fedora: `sudo dnf install ./slogmetaraw-*.rpm` |
| `SLogMetaRaw-<version>-linux-x86_64.run` | any distribution: `sh SLogMetaRaw-*.run` (`--user` without sudo, `--uninstall`) |
| `SLogMetaRaw-<version>-linux-x86_64.tar.gz` | the same, unpacked: run `install.sh` inside |

The plugin goes to `/usr/OFX/Plugins`; the Python library to `/usr/lib/slogmetaraw` (.deb/.rpm) or
`~/.local/share/SLogMetaRaw` (.run, .tar.gz). Restart Resolve. Python 3.6+ is enough. The binary is built against
glibc 2.28, so it loads on Rocky/Alma/RHEL 8+, Ubuntu 20.04+ and Debian 10+.

**Windows:** build it yourself with `powershell -File packaging\windows\build_package.ps1` (cmake, Visual Studio
Build Tools, python; Inno Setup is optional for the `.exe`). State lives in `%APPDATA%\SLogMetaRaw`.

A tag `v*` pushed to the repository makes CI attach the Linux packages (and, best-effort, the Windows package) to a
**draft** GitHub Release automatically — see [Known limits](#16-known-limits) for what's still missing (GPU
acceleration, a tested macOS-equivalent image pipeline).

---

## 16. Known limits

- Resolve's Camera Raw panel and gyro stabilisation cannot be unlocked for MP4: they live inside Resolve's decoders. Log MetaRaw rebuilds the colour controls, it doesn't open a door into Resolve.
- S-Log2 follows Sony's document; Resolve's S-Log2 curve differs by ~0.15 stop.
- Still to verify on more files: XAVC HS (HEVC), HLG, S-Cinetone, power zooms.
- Tested only on Resolve Studio 21.1 on macOS. The Linux and Windows builds are CPU-only and untested inside Resolve (see [§15](#15-linux-and-windows-experimental)).
- Independent hobby project, provided as is, with no warranty and no liability for professional use. Not affiliated with or endorsed by Sony or Blackmagic Design.

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0+](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [Release notes](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)
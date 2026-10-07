# S-Log MetaRaw — User Guide

**English** · [Italiano](Wiki-Software-it) · [Español](Wiki-Software-es) · [Português](Wiki-Software-pt) · [简体中文](Wiki-Software-zh) · [Home](Home)

> Version **2.3.1** · macOS 12+ · DaVinci Resolve 20 / 21 · Sony XAVC `.MP4` / `.MXF`
> Tested only on DaVinci Resolve Studio 21.1 on macOS.

S-Log MetaRaw reads the shooting data Sony cameras write into every file (Kelvin, tint, EI, lens, aperture, shutter, colour profile) and uses it inside DaVinci Resolve. Resolve does this only for FX6/FX9 MXF files; for MP4 from FX30, FX3, a7 and a6000 series it ignores that data.

**Contents**

1. [What it is — and what it is not](#1-what-it-is--and-what-it-is-not)
2. [Install](#2-install)
3. [Quick start](#3-quick-start)
4. [The script](#4-the-script)
5. [The S-Log MetaRaw node](#5-the-s-log-metaraw-node)
6. [False colour](#6-false-colour)
7. [Tones and Zones](#7-tones-and-zones)
8. [The S-Log MetaRaw Detail node](#8-the-s-log-metaraw-detail-node)
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
| **Script** | Workspace › Scripts › S-Log MetaRaw | Reads the metadata of every clip and writes it into the Media Pool. Fixes each clip's Data Level. |
| **S-Log MetaRaw** node | Color › OpenFX — **first node** | Develops a clip from its as-shot values: white balance, exposure, colour space, tones by zones, false colour. |
| **S-Log MetaRaw Detail** node | Color › OpenFX — **right after** | The creative node: local highlight/shadow recovery, Texture, Clarity, Dehaze. |

Original files are **never modified**: no transcoding, no rewrapping.

**It is not raw.** A log MP4 is already demosaiced and compressed (8 or 10 bit, often 4:2:0, with in-camera noise reduction). The node applies colour science rigorously — exposure and white balance in linear light, from the recorded values, with Sony's published curves and gamuts — so the image *behaves* in a way that reminds you of raw: white balance shifts cleanly, exposure moves like a stop of light, highlights roll off instead of breaking. Push past the camera's limits and the missing information shows: banding in skies, noise in lifted shadows, clipped highlights stay clipped. Expose well on set.

---

## 2. Install

1. Download `SLogMetaRaw-2.3.1.dmg` from [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) and open it.

   ![The DMG window](images/01-dmg.png)

2. Double-click **Installa S-Log MetaRaw.pkg**. The package is not signed with an Apple certificate: the first time, **right-click › Open**. It asks for your Mac password because the plugin goes in a system folder.

   ![Right-click › Open on the unsigned package](images/02-gatekeeper.png)

3. **Restart DaVinci Resolve.**

The installer also deletes Resolve's plugin cache (`OFXPluginCacheV2.xml`); Resolve rebuilds it at the next launch. Without that, Resolve would show the old panel and would not see the Detail node.

Every install starts clean: the previous plugin and library are replaced entirely, and development installs are removed.

**What goes where**

| Item | Path |
|---|---|
| Both nodes (one bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Python library | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Menu script | `…/DaVinci Resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py` |
| Per-clip cache (JSON) | `~/Library/Application Support/SLogMetaRaw/cache` |

**Requirements:** macOS 12 or later, Apple silicon or Intel, DaVinci Resolve 21 or 20 (Resolve 20: install Python 3 from python.org, it has none of its own).
**Clips:** Sony XAVC `.MP4` or `.MXF` — the script reads them all. The nodes develop **S-Log3** (S-Gamut3.Cine or S-Gamut3), **S-Log2** and **S-Log** (S-Gamut). With other profiles (Cine, HLG, S-Cinetone) they stay neutral and say so.

---

## 3. Quick start

1. Import the footage. Open **Workspace › Scripts › S-Log MetaRaw**, press **1 · Read metadata**, then **2 · Write to Resolve**.
2. On the Color page, add **S-Log MetaRaw** as the **first node**. It takes the clip's EI, Kelvin and tint: at those values it changes nothing.
3. Correct white balance and exposure in the node, using the false colour views.
4. Shape the tones with **Toni**. For local recovery, Texture, Clarity or Dehaze, add **S-Log MetaRaw Detail** as the next node.
5. Then the rest of the grade and, last, your output CST / LUT / DRT.

```
S-Log MetaRaw  →  S-Log MetaRaw Detail  →  rest of the grade  →  output CST / LUT / DRT
```

![Recommended node tree](images/08-node-tree.png)

> Copy the node onto another clip and it resets itself with that clip's data.

---

## 4. The script

**Workspace › Scripts › S-Log MetaRaw**

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

## 5. The S-Log MetaRaw node

**Color › OpenFX › S-Log MetaRaw** — first node, before any CST or LUT.

![OpenFX library with both nodes](images/07-openfx-library.png)

The node is **pointwise**: each pixel depends only on itself. It never makes halos, and **Generate LUT** can export it (65 points recommended). Settings are saved per clip.

> The nodes' panels are in Italian. Labels below come with a translation.

![Main node panel — top](images/09-node-panel-top.png)

| Control | What it does |
|---|---|
| **Version** (top) | Shows `v2.3.1`. Once a day it asks GitHub for the latest release; if there is one it reads **🟢 v2.3.1 → 2.x.y** and a click opens the download of the installer for your system (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). It never installs anything itself. |
| **Camera** · **Rileggi metadata** (re-read) | The camera that was read. *Rileggi* re-reads the clip, resets every control to the camera values and writes the clip's metadata into the Media Pool. Answers within ~2 s. |
| **Decode Using** | *Clip* lets you change the controls; *Camera metadata* locks them to the as-shot values (node is transparent). |
| **White Balance** | As shot, or presets (Daylight, Cloudy, Shade, Tungsten, Fluorescent, Flash). Moving a slider switches it to *Custom*. |
| **Color Temp** · **Tint** | Bradford chromatic adaptation in linear light, from the white the camera recorded. Higher Temp warms; positive Tint goes towards magenta. |
| **Exposure** | In EI: double the EI = exactly +1 stop, in linear light, before any curve. |
| **False color** | Temperature, tint and exposure views — see [§6](#6-false-colour). |
| **Color Space** · **Gamma** | Output, like a Color Space Transform. *Timeline* does not convert — leave it there in a colour-managed project. To grade in DWG: *DaVinci WG · DaVinci Intermediate*. |
| **Toni** (Tones) | Contrast, Highlights, Shadows, Whites, Bianco, Blacks, Vibrance, Saturation — see [§7](#7-tones-and-zones). |
| **Zone** (Zones, closed) | Black, Shadow, Light, Specular zones; Contrast Pivot; Soft Clip. |
| **Avanzate** (Advanced) | Node input, data-level correction, status, *Sblocca controlli senza metadata* (unlock without metadata). |
| **Dati di ripresa** (Shooting data) | Read only: lens, focal length, aperture, focus, shutter, EI, WB, fps, ND, camera LUT. |

![Avanzate and Dati di ripresa](images/14-avanzate-dati.png)

**Avanzate in detail**

- **Ingresso nodo** (node input) — *Automatico* asks Resolve.
- **Data level in ingresso** — fixes the code-value scale when Resolve decodes a clip on the wrong one. The script's *Correct the Data Level* fixes it project-wide.
- **Stato** (status) — what the node detected; it also warns when a false colour view is on.
- **Sblocca controlli senza metadata** — see [§11](#11-special-cases).

> **Rec.709 output** in the node is a CST *without* tone mapping (highlights past Bianco clip) and shuts out the Detail node. Prefer an output CST at the end of the tree.

---

## 6. False colour

One view per control, placed above the slider it serves. **The view replaces the image — turn it off before you render.** (An OpenFX plugin cannot draw an overlay on Resolve's viewer.)

Set *Decode Using* to **Clip** first: in *Camera metadata* the controls are locked.

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

> **The honest cost of a pointwise node:** whatever it compresses, it compresses the texture inside it too. Recovery that keeps texture lives in the Detail node.

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

## 8. The S-Log MetaRaw Detail node

**Color › OpenFX › S-Log MetaRaw Detail** — right after S-Log MetaRaw, before any CST / LUT / DRT.

It works **by areas**, over an edge-aware base: it moves large areas of light without flattening fine detail — the part of Lightroom's Highlights/Shadows a pointwise node cannot do.

![Detail node panel](images/16-detail-panel.png)

| Group | Controls |
|---|---|
| **Gamma dinamica** (dynamic range) | Local Contrast, Local Highlights, Local Shadows; *gain* and *base* check views. |
| **Presenza** (presence) | **Texture** (fine detail; doesn't raise grain below the noise threshold), **Clarity** (mid-scale local contrast), **Dehaze**. |
| **Zone locali** (local zones) | The main node's zones, applied to areas instead of pixels. |
| **Avanzate** (advanced) | Detail preservation, radius, edge threshold (*Soglia bordi*), noise threshold, Clarity centre, **Bianco** for Local Highlights, node input. |
| **Velo** (haze) | Level and colour of the haze Dehaze removes. You set them; never estimated per frame → no flicker. |

**Azzera dettaglio** resets the node.

**Local Highlights vs Highlights.** Local Highlights compresses large bright areas and keeps — even strengthens — fine texture: the sky darkens and the clouds keep their detail. The main node's Highlights softens highlight texture, like film. Local Highlights has its own **Bianco** in Avanzate and does not know the EI chosen in the main node: if you change EI a lot, adjust it.

![Local Highlights on a sky, before/after](images/17-detail-before-after.png)

**Rules**

- It decodes what it receives to linear light and writes it back in the **same encoding**. It accepts only scene-log encodings: S-Log3, S-Log2, DaVinci WG/Intermediate, ACEScct. **Not** Rec.709, Gamma 2.4 or sRGB.
- It is **spatial**: Generate LUT leaves it out, with the rest of its node. Keep it in a node of its own.
- Radii follow frame height: same look at full res, proxy and viewer. No per-frame statistics → no flicker.
- Metal: ~6–18 ms per UHD frame on Apple silicon. The CPU fallback is much slower.

**Measured limits**

- Local Highlights −100: halo on the dark side of an edge < 3% of the step.
- Local Shadows or local zones at ±100: ~12% on a hard 1-stop edge, 4–6% on 2–3 stop edges. If you see it, lower *Soglia bordi*.
- Texture near strong edges can grow grain 1.25–1.7×.
- Dehaze needs a real haze to remove.

Deep dive (Italian): [docs/DETAIL.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DETAIL.md)

---

## 9. Pipelines: where each node goes

### A — Grade in DaVinci Wide Gamut (recommended)

```
S-Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → Detail             Ingresso nodo: DaVinci WG/Intermediate
  → your grade (DWG)
  → output CST         DaVinci WG/Intermediate → Rec.709 · Gamma 2.4, DaVinci tone mapping
```

With tone mapping after, set **Bianco to 4–5 in both nodes**. On an unmanaged timeline you must declare the Detail node's input.

### B — All in log

```
S-Log MetaRaw          Color Space/Gamma: Timeline
  → Detail             Ingresso nodo: Automatico (uses the clip's S-Log3)
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
| …and keep cloud texture | Detail › Gamma dinamica | Local Highlights |
| Backlit face in shadow | Main › Toni | Shadows +30…+60, Blacks −20…−40 |
| More "film" look | Main › Toni | Contrast +20…+30, Highlights −60 |
| Noisy saturated shadows | Main › Zone | Shadow Sat −30…−50 |
| Specular reflections | Main › Zone | Specular Exp −1…−2 |
| Flat, hazy landscape | Detail › Presenza + Velo | Dehaze; set haze level/colour by hand |

---

## 11. Special cases

**ProRes from an external recorder / unreadable clip.** The node stays neutral. Tick **Avanzate › Sblocca controlli senza metadata** and type the shooting EI, Kelvin and tint: the controls come alive and the node starts neutral.

**Non-log profiles** (Cine, HLG, S-Cinetone): the nodes stay neutral and say so in *Stato*.

**Wrong Data Level.** If blacks look lifted or crushed straight out of the camera, Resolve is decoding on the wrong code-value scale. Run the script with *Correct the Data Level* on (project-wide fix), or use *Avanzate › Data level in ingresso* on a single clip. Background (Italian): [docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**Cameras without Kelvin** (e.g. a6300): Kelvin is estimated from the light preset and flagged.

**Performance.** Opening a project reads no files. Opening the panel waits at most half a second; a slow disk finishes in the background with a 15 s limit.

---

## 12. Updates and privacy

- The **nodes** ask GitHub for the latest release at most once a day, in the background. The request carries only the version (`User-Agent: SLogMetaRaw/2.3.1`).
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

Double-click **Disinstalla S-Log MetaRaw.command** in the DMG (first time: right-click › Open). It:

1. lists everything it will remove (all 1.x and 2.x versions, the old *SonyMeta* name, dev installs, cache, settings, logs);
2. asks you to quit Resolve;
3. asks whether to delete exported CSVs too;
4. checks nothing is left.

Run with `--dry-run` to see what it would remove without touching anything. Metadata already written into Resolve projects stays — it is part of the projects.

![Uninstaller in Terminal](images/20-uninstaller.png)

---

## 14. Troubleshooting / FAQ

**The Detail node is missing / the panel looks old.**
Resolve is using its old plugin cache. Quit Resolve, delete `OFXPluginCacheV2.xml` (the installer normally does this), restart.

**The node does nothing.**
That is by design at the as-shot values. Check *Decode Using* is on **Clip**, and read *Avanzate › Stato*: it says if the profile isn't log or metadata is missing.

**My render has false colour in it.**
A view was left on. Turn off all false colour views before rendering.

**Highlights look compressed twice / dull.**
You have a DRT (ACES, AgX, DaVinci tone mapping) after the node: raise **Bianco** to 4–5 (in the Detail node too).

**Halos around edges with the Detail node.**
Lower *Avanzate › Soglia bordi*, or reduce Local Shadows / local zones.

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

- Resolve's Camera Raw panel and gyro stabilisation cannot be unlocked for MP4: they live inside Resolve's decoders. S-Log MetaRaw rebuilds the colour controls, it doesn't open a door into Resolve.
- S-Log2 follows Sony's document; Resolve's S-Log2 curve differs by ~0.15 stop.
- Still to verify on more files: XAVC HS (HEVC), HLG, S-Cinetone, power zooms.
- Tested only on Resolve Studio 21.1 on macOS. The Linux and Windows builds are CPU-only and untested inside Resolve (see [§15](#15-linux-and-windows-experimental)).
- Independent hobby project, provided as is, with no warranty and no liability for professional use. Not affiliated with or endorsed by Sony or Blackmagic Design.

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0+](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [Release notes](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)
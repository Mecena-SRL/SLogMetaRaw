<p align="center"><img src="assets/icon_1024.png" width="140" alt="Log MetaRaw"></p>

<h1 align="center">Log MetaRaw</h1>
<p align="center"><b>Camera log metadata (Sony, Panasonic, Apple, DJI and more) and scene-referred development controls for DaVinci Resolve 20 and 21</b><br>
Ivan Mazzone + Claude · <a href="https://github.com/ivan-94m">github.com/ivan-94m</a> · <a href="https://instagram.com/ivan_94m">@ivan_94m</a></p>

<p align="center">
<b>English</b> · <a href="README.it.md">Italiano</a> · <a href="README.es.md">Español</a> · <a href="README.pt.md">Português</a> · <a href="README.zh.md">简体中文</a>
</p>

---

## What it is

Sony cameras write the shooting setup into every file: white balance in Kelvin, tint, EI, lens, aperture, shutter,
colour profile. Resolve uses that data for the MXF files of an FX6 or FX9, which get a *Camera Raw* panel. For the MP4
files of an FX30, FX3, a7 or a6000-series camera it ignores the data.

Log MetaRaw reads that data and uses it. It is a script and five nodes, numbered in chain order on their icons:

| | Where | What it does |
|---|---|---|
| **Script** | Workspace › Scripts › Log MetaRaw | reads the metadata of every clip in the project and writes it into the Media Pool |
| **1 · Log MetaRaw Noise** node | Color › OpenFX | noise reduction on the log image, with the expected noise of the camera, ISO and format; temporal and spatial, Metal GPU |
| **2 · Stabilize**, **3 · Motion Blur** | Color › OpenFX › Log MetaRaw | gyro stabilisation (built-in Gyroflow) and motion blur rebuilt from optical flow; Apple silicon Macs only |
| **4 · Log MetaRaw** node | Color › OpenFX | develops one clip from its as-shot values: white balance, exposure, colour space, tone by zones, false colour |
| **5 · Log MetaRaw Creative** node | Color › OpenFX | creative development: 16 photographic processes (film negatives and prints, reversal, black and white, Technicolor, Autochrome, three plates, daguerreotype, collodion, early prints), analog video and vintage digital cameras; prints and print generations; presets by device and model (film cameras, tube and CCD video cameras, digital cameras and phones), 14 vintage lenses, transfers (telecine, kinescope, film-out) and a viewing medium (cinema, projectors, CRT and Trinitron TVs, plasma, early LCD, handheld, paper) |
| **Log MetaRaw Ripresa** and **Mix e supporto** plugins (VST3) | Fairlight › Effects › VST3 | period sound: real recorders and microphones on tracks (Geloso, Nagra, Uher, Walkman…), desk, medium (tape, cassette, vinyl, VHS, optical track, radio, telephone), transfers and playback on buses and master; every stage can be switched off; Mac and Windows |

No file is transcoded and no original file is ever written.

### What to expect, honestly

**It is not raw.** A log MP4 is already demosaiced, compressed, 8 or 10 bit, often 4:2:0, with the camera's noise
reduction baked in. No plugin can bring back what the camera threw away.

What the node does is apply colour science carefully. Exposure and white balance act in linear light, starting
from the values the camera recorded and following published curves and gamuts. Tone moves the image in stops. Worked this way, a log image
**behaves in a way that reminds you of a raw file**: white balance shifts cleanly, exposure moves like a stop of light,
and highlights roll off instead of breaking.

On heavy work, colour science alone is not enough. As soon as you push past what the camera captured, the missing
information shows: banding in skies, noise in lifted shadows, clipped highlights that stay clipped, and colour that
comes apart in the compressed channels. Expose well at the camera. Log MetaRaw helps you get the most out of what
is there. It cannot create what is not there.

---

## Install

1. Download `SLogMetaRaw-4.2.0.dmg` from **Releases** and open it.
2. Double-click **Installa Log MetaRaw.pkg**. It is not signed with an Apple certificate: the first time,
   right-click it and choose **Open**. It asks for the Mac password because the plugin goes in a system folder.
3. Restart DaVinci Resolve.

The installer also deletes Resolve's plugin cache (`OFXPluginCacheV2.xml`), which Resolve rebuilds on its next start.
Without that step Resolve would keep showing the old panel and would not list the Creative node.

| Installed | Path |
|---|---|
| The two nodes (one bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| The Python library | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| The menu script | `…/DaVinci Resolve/Fusion/Scripts/Utility/Log MetaRaw.py` |

While it runs it keeps a small JSON record per clip in `~/Library/Application Support/SLogMetaRaw/cache`. The disk image
also holds the illustrated eleven-page guides in Italian and English.

**Clean install and uninstall.** Every install starts clean: the installer replaces the previous plugin and library
entirely and removes developer installs, so no file from an older version stays behind. **Disinstalla S-Log
MetaRaw.command**, on the disk image (the first time: right-click › Open), removes every version ever installed, 1.x and
2.x included, with the cache, settings and logs. It lists everything before touching it, asks you to close Resolve, and
asks before deleting your CSV exports. The metadata already written into Resolve projects is part of the projects and
stays.

**Requirements:** macOS 12 or later, Apple silicon or Intel, and DaVinci Resolve 21 or 20. Resolve 20 has no Python
of its own: install Python 3 from [python.org](https://www.python.org/downloads/), which its Workspace › Scripts menu
needs anyway. **Tested only on Resolve Studio 21.1 on macOS; Resolve 20 not yet tested.**

**Clips:** Sony XAVC in `.MP4` or `.MXF`, plus the cameras in [docs/CAMERAS.md](docs/CAMERAS.md) (Panasonic V-Log,
Apple Log, DJI D-Log and D-Log M and more), each with how reliable its reading is. The script reads any of them. The nodes
develop **S-Log3** (S-Gamut3.Cine or S-Gamut3), **S-Log2**, **S-Log** (S-Gamut) and the curves of those cameras. Since
3.2.0 DJI D-Log M uses the curve measured on an Osmo Action 6 ([docs/DJI_LOG.md](docs/DJI_LOG.md)): for other DJI cameras
it is the best estimate, not a calibration. With an unknown profile they stay neutral and say so.

---

## Quick start

1. Import the footage. **Workspace › Scripts › Log MetaRaw**: press **1 · Read metadata**, then **2 · Write to Resolve**.
2. On the Color page, put **Log MetaRaw** as the **first node**. It picks up the clip's EI, Kelvin and tint. At those
   values it changes nothing.
3. Fix white balance and exposure in the node. The false colour views help.
4. Shape the tones with **Toni** (Tones). For the look of a film stock, an early process, a tape or a vintage camera,
   add **Log MetaRaw Creative** as the next node.
5. Then add your CST, LUT or DRT.

```
Noise  →  Stabilize  →  Motion Blur  →  Log MetaRaw  →  Log MetaRaw Creative  →  CST / LUT / DRT  →  the rest of the grade
```

---

## The script

The window has one row of buttons, one row of options and the clip list. It follows Resolve's language (English,
Italian, Spanish, Portuguese, Simplified Chinese). The nodes' panels are in Italian: below, their labels come with a
translation.

- A menu picks the clips: *Whole Media Pool* or *Selected clips in the Media Pool*.
- **1 · Read metadata**: one row per clip with camera, lens, aperture, shutter, EI, WB, colour space and data level.
  The *Status* column says `read`, what changed during the take (aperture, focus…), or why a clip was
  skipped. Click a row to see everything that was read, grouped as in Catalyst Browse.
- **2 · Write to Resolve**: fills the Media Pool fields (Metadata panel, columns, keywords for smart bins,
  data burn-in) and fixes values Resolve reads wrong from MXF, such as *Camera Aperture* `F53343` on the FX6.
- **Export CSV**: exports the values Resolve has no field for (EI, tint, WB mode, focus distance, capture gamma…) in Resolve's own
  metadata CSV format.
- Options:
  - **Camera tag** (off): adds camera, gamma and primaries to the keywords.
  - **Overwrite metadata** (on): replaces values Resolve already filled in; off, it fills only the empty fields.
  - **Correct the Data Level** (on): sets each clip's *Data Level* to Full or Video, whichever its gamma needs.
    [docs/DATA_LEVELS.md](docs/DATA_LEVELS.md) explains why.
  - **Also set Input Color Space** (off): for colour-managed projects.
    A script cannot put this back to *Project*: only you can, by hand.
- The version at the bottom right checks GitHub when clicked.

Reading is quick because nothing is decoded: at most 24 samples of the metadata track per clip, with a one-second budget.
A volume that stops answering is skipped once, with a message, instead of blocking the list.

---

## The Log MetaRaw node

The node is pointwise: each pixel depends only on itself. It never makes halos, and **Generate LUT** can export it
(65 points recommended).

| Control | What it does |
|---|---|
| **Version** (top) | `v4.2.0`. Once a day it asks GitHub for the latest release. If there is one it reads **🟢 v3.1.0 → 3.x.y**, and a click opens the download of the installer for your system (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`) in the browser. It never installs anything itself |
| **Camera** · **Rileggi metadata** | the camera that was read. *Rileggi* re-reads the clip, sets every control back to the camera values and writes the clip's metadata into the Media Pool |
| **Ripresa** (shot) · **Decodifica** | *Clip* lets you change the controls; *Camera metadata* locks them to the shot values |
| **Bilanciamento** (balance) · **Temperatura** · **Tint** | As shot or a preset. Bradford chromatic adaptation in linear light, from the white the camera recorded |
| **Punto bianco** (white point) | Resolve's eyedropper: pick a point of the image that should be neutral and Temperatura and Tint set themselves, with the same maths as the render |
| **Esposizione** (EI) | exposure index: double the EI = +1 stop |
| **False color** | a box above Temperatura, one below Tint and one below Esposizione; see below |
| **Spazio colore** (colour space) | three sub-groups. **Ingresso** (input): Spazio colore and Gamma entering the node, Rilevato (detected) and *Ripristina da camera*, Data level, Livello and *Ripristina da clip*. **Senza metadata**: *Sblocca controlli* and the shooting references. **Uscita** (output): **Tone mapping** (Nessuno/CST, ACES 2.0, Luminanza, Filmico, ACES 1.3, OpenDRT), Compressione and Mappatura gamut, Vibrance luci, Spazio colore and Gamma (like a Color Space Transform; *Timeline* does not convert), LUT size and export |
| **Toni** (tones) | Highlights, **Bianco** (stop), Soft Clip, Saturation (−100…+100). *Mostra tutti i controlli* adds Contrast, Shadows, Whites, Blacks, Vibrance and the **Zone** (Black, Shadow, Light, Specular with Exp, Sat, Range and Falloff, Contrast Pivot, *Zone* false color); they appear by themselves when the node uses them |
| **Dati di ripresa** (shooting data) | read only: lens, focal length, aperture, focus, shutter, EI, WB, fps, ND, camera LUT |

**Highlights is a film shoulder.** Pulled down, it compresses the highlights with a slope that falls smoothly towards the
top, the way film, ACES 2.0 and AgX do. At −100 the brightest value the camera recorded (about +6 stops over grey in
S-Log3) lands exactly on **Bianco**: no grey veil and no clip. Grey and everything below stay put, and skin at +1 stop moves
by 0.05 stops at most. The most compressed highlights drift gently towards white without changing hue. Pushed up, it gives
the highlights more snap, with a bounded slope. **Bianco** is where that top lands (and Soft Clip's roof): 2.5 stops is
Rec.709 white through a plain CST without tone mapping. With a DRT after the node (ACES, AgX, DaVinci), raise it to 4–5,
or the highlights are compressed twice.

**Tones by zones.** Shadows, Whites and the Zones are exposure changes, in stops, over a range of tones: Shadows below
−1 stop, Whites from +3.5 stops. Blacks is a linear veil that moves black without moving grey. Inside a zone the image moves
like an exposure, so texture is kept, and the compression sits in a declared transition band. No combination of sliders can
solarise. The honest cost of a pointwise node: whatever it compresses, it compresses the texture too. That is why local
recovery is a separate node. Formulas and recipes: [docs/TONE_MAPPING.md](docs/TONE_MAPPING.md).

**False colour.** One view per control, next to the slider it serves (above Temperatura, below Tint and below Esposizione):
- **Exposure** uses ARRI-style bands on the stops around 18% grey. Green is middle grey, pink is one stop over (skin),
  yellow is near clip, red is clipped, and blue and purple are the bottom.
- **Temperature** and **Tint** work as in CineMatch. The picture turns grey, casts show in orange/blue or green/magenta,
  and near-neutral casts are boosted up to 8× so they show. Move the slider until what should be neutral stays grey.
  Each view reacts only to its own slider.

The view replaces the image. Turn it off before you render. [docs/FALSE_COLOR.md](docs/FALSE_COLOR.md)

**Data level.** If Resolve decodes a clip on the wrong code-value scale, the node corrects it before the log curve.
The script's *Correct the Data Level* option fixes it for the whole project. [docs/DATA_LEVELS.md](docs/DATA_LEVELS.md)

**Tone mapping with a scene output (3.2.0).** With a rendering (ACES 2.0, Filmico, OpenDRT…), a scene Spazio colore
and Gamma such as DaVinci WG / DaVinci Intermediate are no longer replaced by Rec.709 / Gamma 2.4: the image rendered for
Rec.709 travels in that encoding, to hand it to other plugins (Dehancer…), and a CST to Rec.709 / Gamma 2.4 without tone
mapping brings it back identical. Display outputs (Rec.709, P3, Rec.2020) are unchanged.

**Without metadata.** A ProRes from an external recorder, or a clip that cannot be read, leaves the node neutral. Tick
*Spazio colore › Senza metadata › Sblocca controlli* and type the shooting EI, Kelvin and tint: the controls come alive, and
the node starts neutral.

**Speed.** Opening a project does not read any file. Opening the panel waits at most half a second; a slow disk
finishes in the background, with a 15-second limit. *Rileggi* answers within about 2 seconds.

---

## The Log MetaRaw Creative node

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
  Sony FX6 and FX30, iPhone 17 Pro and DJI Osmo Action 4 and 5 Pro. The **Rileggi metadata** button re-reads them from the
  file (they are read on their own when the panel opens and they are missing). To tune it on the real noise, tick **Mostra campione**,
  drag the box's two corners onto a flat area and press **Usa il campione** (like Neat Video); bands, shadows and skin
  are in the **Avanzate** group.
  - *Temporal*: ±1 to ±3 neighbour frames, aligned by blocks and weighted per pixel, so moving parts and cuts do not
    smear.
  - *Spatial*: luma and colour apart, in 5 wavelet bands. Each coefficient is weighed against the energy of its
    neighbourhood, which keeps texture.
  - Views of the removed noise, detail and skin protection, and **Salta Noise ovunque** to switch every Noise node off
    while you edit.
  - On a Mac it runs entirely on the Metal GPU, and the tests check that it matches the CPU. Elsewhere it runs on the
    CPU.
  - The profiles are estimates until they are tuned on real samples. [docs/DENOISE.md](docs/DENOISE.md)
- **Stabilize** reads the clip's gyro (built-in Gyroflow). Its styles go from micro-shake to crane, with a keyframeable
  **MASTER**. **Motion Blur** rebuilds the shutter from optical flow, for example 50p at 90° to 25p at 180°, and keeps the sensor grain in the blurred areas. Both come
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

## Updates and privacy

- The **nodes** ask GitHub for this project's latest release at most once a day, in the background. The request
  carries only the program version (`User-Agent: SLogMetaRaw/4.2.0`). To turn it off, create the empty file
  `~/Library/Application Support/SLogMetaRaw/no_update_check`.
- The **script** checks only when you click its version.
- A click only opens a download link from this project's GitHub releases. Nothing is installed without you.

---

## How it works, in short

- **Metadata.** The parsers are pure Python, with no dependencies. They read the per-frame SMPTE RDD 18 acquisition
  metadata (MP4 `rtmd` track, MXF ST 436 packets), Sony's NonRealTimeMeta XML, the H.264/HEVC SPS and the MXF picture
  descriptor. A multi-gigabyte clip costs about 100 KB of reading.
- **From the script to the node.** Each clip gets a JSON record in the cache. The node finds its file through Resolve's
  source path, reads that record, and creates it in the background when it is missing.
- **Colour maths.** The chain is: decode the camera curve, apply the EI ratio, apply Bradford white balance from the Planckian locus
  (with tint perpendicular to it in CIE 1960 uv), then tone with one gain per pixel from a norm of its channels (so
  chromaticity is kept), then colour, then output. Everything is in linear light, with Sony's published curves and gamuts.
- **One maths, three implementations.** The maths lives in `ofx/SLogMetaRaw/math` and compiles both as C++ and as
  Metal. A Python reference model checks both: CPU and GPU agree within 2·10⁻⁴, with FMA disabled on every path.
- **Tests.** 520 automated tests: parsers, reference models, C++, Metal, a miniature OpenFX host that loads both
  nodes the way Resolve does, and golden files of the panels.

---

## Building from source

```bash
make -C ofx/SLogMetaRaw                     # universal plugin, arm64 + x86_64 (needs Xcode)
make -C ofx/SLogMetaRaw test-bins           # test binaries
python3 -m unittest discover tests          # the test suite
./install.sh --dev                          # script and plugin pointing at this folder
./packaging/build_installer.sh              # dist/SLogMetaRaw-<version>.dmg
python3 -m slogmetaraw clip.MP4             # Catalyst-style readout from the command line
```

```
slogmetaraw/         parsers, Resolve writing, the script window, update check
resolve_script/      launcher for Workspace › Scripts
ofx/SLogMetaRaw/     math/ (shared CPU/Metal), src/ (common, develop, detail, noise), metal/
motion/              Mecena Motion: Stabilize, Motion Blur, mecena-clipdb (Rust + C++, macOS)
tools/               generators for matrices, icons and charts
tests/               tests and reference models (the sample clips are not in the repository)
packaging/           installer, guides, uninstaller
docs/                TONE_MAPPING, CREATIVE, DENOISE, FALSE_COLOR, DATA_LEVELS
```

### Linux and Windows (CPU render, experimental)

The plugin builds for Linux and Windows with CMake. These ports render on the CPU (no Metal, CUDA or OpenCL yet) and
have not been tested inside DaVinci Resolve itself: the macOS build is the supported one.

**Linux install** (download from **Releases**; pick what fits your distribution):

| File | For |
|---|---|
| `slogmetaraw_<version>_amd64.deb` | Ubuntu, Debian, Mint: `sudo apt install ./slogmetaraw_*.deb` |
| `slogmetaraw-<version>-1.x86_64.rpm` | Rocky, Alma, RHEL, CentOS, Fedora: `sudo dnf install ./slogmetaraw-*.rpm` |
| `SLogMetaRaw-<version>-linux-x86_64.run` | any distribution: `sh SLogMetaRaw-*.run` (`--user` without sudo, `--uninstall`) |
| `SLogMetaRaw-<version>-linux-x86_64.tar.gz` | the same, unpacked: run `install.sh` inside |

The plugin goes to `/usr/OFX/Plugins`; the Python library to `/usr/lib/slogmetaraw` (.deb/.rpm) or
`~/.local/share/SLogMetaRaw` (.run, .tar.gz). The package puts the menu script in `/opt/resolve/Fusion/Scripts/Utility`
when Resolve is there; otherwise run `slogmetaraw-setup` (for your user) once. Restart Resolve. Python 3.6 or newer is
enough (Rocky/RHEL 8 included). The binary is built against glibc 2.28, so it loads on Rocky/Alma/RHEL 8+, Ubuntu 20.04+
and Debian 10+.

```bash
# build it yourself (cmake, a C++17 compiler, python3; the OpenFX SDK is fetched, or set OFX_SDK_DIR)
./packaging/linux/build_package.sh          # dist/: .tar.gz, .run, .deb, .rpm
# Windows (PowerShell): cmake, Visual Studio Build Tools, python; Inno Setup is optional
powershell -File packaging\windows\build_package.ps1   # dist\...-windows-x64.zip (+ setup .exe with Inno Setup)
```

State lives in `~/.local/share/SLogMetaRaw` on Linux and `%APPDATA%\SLogMetaRaw` on Windows. `.github/workflows/`
builds the packages and installs the .deb/.rpm on Rocky 8/9, Ubuntu 20.04-24.04 and Debian 12 on every change; pushing a
`v*` tag attaches them to a draft release.

---

## Known limits

- Resolve's own Camera Raw panel and gyro stabilisation cannot be unlocked for MP4 (for the gyro there is the Stabilize node). Both live inside Resolve's
  decoders. Log MetaRaw rebuilds the colour controls. It does not open a door into Resolve.
- Some cameras (the a6300 among them) do not record a Kelvin value. It is then estimated from the lighting preset and flagged.
- S-Log2 follows the Sony paper. Resolve's own S-Log2 curve differs by about 0.15 stop.
- Still to be checked on more files: XAVC HS (HEVC), HLG, S-Cinetone, power zooms.
- **An independent hobby project**, distributed as is, with no warranty and no liability for professional use. The
  code is open: read it, test it, change it.

---

## Credits and licence

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m).
Written with Claude (Anthropic): 1.0 on 19 September 2026, 1.1.0 on 22 September, 2.0.0 on 23 September 2026, 3.2.0 and 4.0.0 on 24 November 2026. The full
history is in [RELEASE_NOTES.md](RELEASE_NOTES.md) (in Italian).

Sony tag references: SMPTE RDD 18, [ExifTool](https://exiftool.org) and
[telemetry-parser](https://github.com/AdrianEddy/telemetry-parser) by AdrianEddy (MIT), from which part of the tag table
comes.

Rendering and camera data: the ACES 2.0 Output Transform is ported from OpenColorIO 2.6 and ACES 1.3 is checked
against its built-ins (ACES: Academy of Motion Picture Arts and Sciences); [OpenDRT](https://github.com/jedypod/open-display-transform) v1.1.0 is by
Jed Smith (GPL-3.0), ported from its DCTL. The DJI D-Log M curve and gamut come from the Osmo Action 6 measurements in
[jens-krause/dlogm-dctl](https://github.com/jens-krause/dlogm-dctl) v1.3 and from DJI's Action 6 LUT, used only as a reference and not
distributed; up to 3.1.x the curve was Thatcher Freeman's fit.

Stabilize builds on [Gyroflow](https://github.com/gyroflow/gyroflow) by AdrianEddy and contributors (GPL-3.0) and its
lens catalogue. The Noise node's spatial filter follows NeighShrink (Chen, Bui and Krzyżak, 2004).

[GNU GPL v3.0 or later](LICENSE). The OpenFX SDK is © The Open Effects Association (BSD-3). The VST3 SDK is © Steinberg Media Technologies (MIT); VST is a trademark of Steinberg. Sony, XAVC and Catalyst are
trademarks of Sony Group Corporation; DaVinci Resolve is a trademark of Blackmagic Design. This is an independent project,
not affiliated with or endorsed by either company.

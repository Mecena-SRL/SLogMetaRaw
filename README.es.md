<p align="center"><img src="assets/icon_1024.png" width="140" alt="Log MetaRaw"></p>

<h1 align="center">Log MetaRaw</h1>
<p align="center"><b>Metadatos log de cámaras (Sony, Panasonic, Apple, DJI y otras) y controles de revelado en luz de escena para DaVinci Resolve 20 y 21</b><br>
Ivan Mazzone + Claude · <a href="https://github.com/ivan-94m">github.com/ivan-94m</a> · <a href="https://instagram.com/ivan_94m">@ivan_94m</a></p>

<p align="center">
<a href="README.md">English</a> · <a href="README.it.md">Italiano</a> · <b>Español</b> · <a href="README.pt.md">Português</a> · <a href="README.zh.md">简体中文</a>
</p>

---

## Qué es

Las cámaras Sony escriben en cada archivo cómo se rodó el plano: balance de blancos en Kelvin, tinte, EI, objetivo,
diafragma, obturador, perfil de color. Resolve usa esos datos en los MXF de la FX6 y la FX9, que tienen panel
*Camera Raw*. En los MP4 de una FX30, FX3, serie a7 o a6000 los ignora.

Log MetaRaw lee esos datos y los usa. Es un script y cinco nodos, numerados en sus iconos en el orden de la cadena:

| | Dónde | Qué hace |
|---|---|---|
| **Script** | Workspace › Scripts › Log MetaRaw | lee los metadatos de todos los clips del proyecto y los escribe en el Media Pool |
| Nodo **1 · Log MetaRaw Noise** | Color › OpenFX | reducción de ruido sobre la imagen log, con el ruido esperado por cámara, ISO y formato; temporal y espacial, GPU Metal |
| **2 · Stabilize**, **3 · Motion Blur** | Color › OpenFX › Log MetaRaw | estabilización por giroscopio (Gyroflow integrado) y desenfoque de movimiento a partir del flujo óptico; solo Mac Apple silicon |
| Nodo **4 · Log MetaRaw** | Color › OpenFX | revela un clip a partir de sus valores de rodaje: balance, exposición, espacio de color, tonos por zonas, falso color |
| Nodo **5 · Log MetaRaw Creative** | Color › OpenFX | revelado creativo: 16 procesos fotográficos (negativos y copias, inversibles, blanco y negro, Technicolor, Autochrome, tres placas, daguerrotipo, colodión, copias antiguas), vídeo analógico y cámaras digitales de época; copias y generaciones de copia; un soporte de visionado (sala, proyector doméstico, televisor CRT, consola LCD, papel) |
| Plugins **Log MetaRaw Ripresa** y **Mix e supporto** (VST3) | Fairlight › Efectos › VST3 | sonido de época: grabadoras y micrófonos reales en las pistas, mesa, soporte (cinta, casete, vinilo, VHS, banda óptica, radio, teléfono), copias y escucha en buses y máster; cada etapa se puede desactivar; Mac y Windows |

Ningún archivo se transcodifica y ningún original se escribe nunca.

### Qué esperar, con honestidad

**No es raw.** Un MP4 log ya está demosaicado y comprimido, a 8 o 10 bits, a menudo 4:2:0, con la reducción de ruido de
la cámara ya aplicada. Ningún plugin puede devolver lo que la cámara descartó.

Lo que hace el nodo es aplicar la ciencia del color con rigor. Exposición y balance trabajan en luz lineal, parten de los
valores que la cámara registró y siguen curvas y gamuts publicados. Los tonos mueven la imagen en pasos. Trabajada así,
una imagen log **se comporta de un modo que recuerda a un archivo RAW**: el balance se desplaza limpio, la exposición se
mueve como un paso de luz y las altas luces se redondean en vez de romperse.

En un trabajo intenso, la ciencia del color sola no basta. En cuanto se fuerzan los límites de la cámara, la falta de
información en la imagen se nota: banding en los cielos, ruido en las sombras levantadas, altas luces quemadas que
siguen quemadas, color que se deshace en los canales comprimidos. Expón bien en rodaje. Log MetaRaw te ayuda a sacar
lo mejor de lo que hay. No puede crear lo que no hay.

---

## Instalación

1. Descarga `SLogMetaRaw-4.1.0.dmg` desde **Releases** y ábrelo.
2. Doble clic en **Installa Log MetaRaw.pkg**. No está firmado con un certificado de Apple: la primera vez, clic
   derecho › **Abrir**. Pide la contraseña del Mac porque el plugin va en una carpeta del sistema.
3. Reinicia DaVinci Resolve.

El instalador también borra la caché de plugins de Resolve (`OFXPluginCacheV2.xml`), que Resolve reconstruye en el
siguiente arranque. Sin eso, Resolve seguiría mostrando el panel antiguo y no vería el nodo Creative.

| Instalado | Ruta |
|---|---|
| Los dos nodos (un solo bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| La biblioteca Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| El script del menú | `…/DaVinci Resolve/Fusion/Scripts/Utility/Log MetaRaw.py` |

Mientras funciona guarda un pequeño registro JSON por clip en `~/Library/Application Support/SLogMetaRaw/cache`. El
disco también contiene las guías ilustradas de diez páginas, en italiano e inglés.

**Instalación limpia y desinstalación.** Cada instalación empieza limpia: el instalador sustituye por completo el plugin
y la biblioteca anteriores y quita las instalaciones de desarrollo, así que no queda ningún archivo de una versión vieja.
**Disinstalla Log MetaRaw.command**, en el disco (la primera vez: clic derecho › Abrir), elimina todas las versiones
instaladas, 1.x y 2.x incluidas, con la caché, los ajustes y los registros. Antes de tocar nada lo enumera todo, te pide
cerrar Resolve y te pregunta si borrar también los CSV exportados. Los metadatos ya escritos en los proyectos de Resolve
forman parte de los proyectos y se quedan.

**Requisitos:** macOS 12 o posterior, Apple silicon o Intel, y DaVinci Resolve 21 o 20. Resolve 20 no trae Python
propio: instala Python 3 desde [python.org](https://www.python.org/downloads/), que su menú Workspace › Scripts
necesita de todos modos. **Probado solo en Resolve Studio 21.1 en macOS; Resolve 20 aún no probado.**

**Clips:** Sony XAVC en `.MP4` o `.MXF`, más las cámaras de [docs/CAMERAS.md](docs/CAMERAS.md) (Panasonic V-Log,
Apple Log, DJI D-Log y D-Log M y otras), cada una con la fiabilidad de su lectura. El script los lee todos. Los nodos
revelan **S-Log3** (S-Gamut3.Cine o S-Gamut3), **S-Log2**, **S-Log** (S-Gamut) y las curvas de esas cámaras. Desde la
3.2.0 DJI D-Log M usa la curva medida en una Osmo Action 6 ([docs/DJI_LOG.md](docs/DJI_LOG.md)): para las demás cámaras
DJI es la mejor estimación, no una calibración. Con un perfil desconocido se quedan neutros y lo dicen.

---

## En pocas palabras

1. Importa el material. **Workspace › Scripts › Log MetaRaw**: pulsa **1 · Leer metadatos** y luego
   **2 · Escribir en Resolve**.
2. En la página Color pon **Log MetaRaw** como **primer nodo**. Toma el EI, los Kelvin y el tinte del clip. Con esos
   valores no cambia nada.
3. Corrige balance y exposición en el nodo. Las vistas de falso color ayudan.
4. Da forma a los tonos con **Toni** (Tonos). Para el aspecto de una película, un proceso antiguo, una cinta o una
   cámara de época añade **Log MetaRaw Creative** como nodo siguiente.
5. Después, tu CST, LUT o DRT.

```
Noise  →  Stabilize  →  Motion Blur  →  Log MetaRaw  →  Log MetaRaw Creative  →  CST / LUT / DRT  →  resto del etalonaje
```

---

## El script

La ventana tiene una fila de botones, una de opciones y la lista de clips, y sigue el idioma de Resolve (español,
inglés, italiano, portugués, chino simplificado). Los paneles de los nodos están en italiano: abajo sus etiquetas van
con traducción.

- Un menú elige los clips: *Todo el Media Pool* o *Clips seleccionados en el Media Pool*.
- **1 · Leer metadatos**: una fila por clip con cámara, objetivo, diafragma, obturador, EI, WB, espacio de color y
  data level. La columna *Estado* dice `leído`, qué cambió durante la toma (diafragma, foco…) o por qué
  se saltó un clip. Haz clic en una fila para ver todo lo leído, agrupado como en Catalyst Browse.
- **2 · Escribir en Resolve**: rellena los campos del Media Pool (panel Metadata, columnas, palabras clave para
  smart bins, data burn-in) y corrige valores que Resolve lee mal de los MXF, como *Camera Aperture* `F53343` en la FX6.
- **Exportar CSV**: exporta los valores para los que Resolve no tiene campo (EI, tinte, modo WB, distancia de foco, gamma
  de captura…) en el formato CSV de metadatos de Resolve.
- Opciones:
  - **Etiqueta por cámara** (apagada): añade cámara, gamma y primarios a las palabras clave.
  - **Sobrescribir metadatos** (encendida): sustituye los valores que Resolve ya escribió; apagada, solo rellena los campos
    vacíos.
  - **Corregir el Data Level** (encendida): pone el *Data Level* de cada clip en Full o Video, lo que pida su gamma. El
    porqué está en [docs/DATA_LEVELS.md](docs/DATA_LEVELS.md).
  - **Establecer también Input Color Space** (apagada): para proyectos con gestión de color.
    Un script no puede devolverlo a *Project*: solo tú, a mano.
- La versión abajo a la derecha, al hacer clic, consulta GitHub.

La lectura es rápida porque no decodifica nada: como máximo 24 muestras de la pista de metadatos por clip, con un límite
de un segundo. Un volumen que deja de responder se salta una sola vez, con un aviso, en lugar de bloquear la lista.

---

## El nodo Log MetaRaw

El nodo es puntual: cada píxel depende solo de sí mismo. Nunca crea halos y **Generate LUT** puede exportarlo
(se recomiendan 65 puntos).

| Control | Qué hace |
|---|---|
| **Versión** (arriba) | `v4.1.0`. Una vez al día pregunta a GitHub por la última release. Si la hay muestra **🟢 v3.1.0 → 3.x.y** y el clic abre en el navegador la descarga del instalador de tu sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Nunca instala nada por sí mismo |
| **Camera** · **Rileggi metadata** | la cámara leída. *Rileggi* (releer) vuelve a leer el clip, devuelve cada control a los valores de cámara y escribe los metadatos del clip en el Media Pool |
| **Ripresa** (rodaje) · **Decodifica** | *Clip* permite cambiar los controles; *Camera metadata* los bloquea en los valores de rodaje |
| **Bilanciamento** (balance) · **Temperatura** · **Tint** | As shot o un preset. Adaptación cromática Bradford en luz lineal, desde el blanco que registró la cámara |
| **Punto bianco** (punto blanco) | el cuentagotas de Resolve: elige en la imagen un punto que debe ser neutro y Temperatura y Tint se ajustan solos, con las mismas matemáticas del render |
| **Esposizione** (EI) | índice de exposición: el doble de EI = +1 paso |
| **False color** | una casilla sobre Temperatura, una bajo Tint y una bajo Esposizione; ver abajo |
| **Spazio colore** (espacio de color) | tres subgrupos. **Ingresso** (entrada): Spazio colore y Gamma que entran al nodo, Rilevato (detectado) y *Ripristina da camera*, Data level, Livello y *Ripristina da clip*. **Senza metadata**: *Sblocca controlli* y las referencias de rodaje. **Uscita** (salida): **Tone mapping** (Nessuno/CST, ACES 2.0, Luminanza, Filmico, ACES 1.3, OpenDRT), Compressione y Mappatura gamut, Vibrance luci, Spazio colore y Gamma (como un Color Space Transform; *Timeline* no convierte), tamaño y exportación de la LUT |
| **Toni** (tonos) | Highlights, **Bianco** (stop), Soft Clip, Saturation (−100…+100). *Mostra tutti i controlli* añade Contrast, Shadows, Whites, Blacks, Vibrance y las **Zone** (Black, Shadow, Light, Specular con Exp, Sat, Range y Falloff, Contrast Pivot, false color *Zone*); aparecen solos si el nodo los usa |
| **Dati di ripresa** (datos de rodaje) | solo lectura: objetivo, focal, diafragma, foco, obturador, EI, WB, fps, ND, LUT de cámara |

**Highlights es un hombro de película.** En negativo comprime las altas luces con una pendiente que baja de forma
continua hacia arriba, como la película, ACES 2.0 y AgX. A −100 el valor más alto que registró la cámara (unos +6 pasos
sobre el gris en S-Log3) llega exactamente al **Bianco**: sin velo gris y sin clip. El gris y lo que está debajo no se
mueven, y la piel a +1 paso se desplaza 0,05 pasos como mucho. Las luces más comprimidas van suavemente hacia el blanco sin
cambiar de tono. En positivo da más fuerza a las altas luces, con una pendiente limitada. **Bianco** es donde llega ese
máximo (y el techo de Soft Clip): 2,5 pasos es el blanco de Rec.709 con un CST sin tone mapping. Con un DRT después del nodo
(ACES, AgX, DaVinci), súbelo a 4–5, o las altas luces se comprimen dos veces.

**Tonos por zonas.** Shadows, Whites y las Zonas son exposiciones, en pasos, sobre una franja de tonos: Shadows por debajo
de −1 paso, Whites desde +3,5 pasos. Blacks es un velo lineal que mueve el negro sin mover el gris. Dentro de una zona la
imagen se mueve como con una exposición, así que la textura se conserva, y la compresión está en una banda de transición
declarada. Ninguna combinación de deslizadores puede solarizar. El coste de un nodo puntual, dicho claro: lo que comprime,
lo comprime también en la textura. Por eso la recuperación local es un nodo aparte. Fórmulas y recetas:
[docs/TONE_MAPPING.md](docs/TONE_MAPPING.md).

**Falso color.** Una vista por control, junto al deslizador al que sirve (encima de Temperatura, debajo de Tint y de Esposizione):
- **Exposición** usa bandas en pasos alrededor del gris 18%, al estilo ARRI. Verde es el gris medio, rosa un paso por
  encima (la piel), amarillo cerca del clip, rojo el clip, azul y violeta el fondo.
- **Temperatura** y **Tinte** funcionan como en CineMatch. La imagen se vuelve gris, las dominantes se colorean de
  naranja/azul o verde/magenta, y las casi neutras se amplifican hasta 8× para que se vean. Mueve el deslizador hasta que
  lo que debe ser neutro se quede gris. Cada vista responde solo a su deslizador.

La vista sustituye la imagen: apágala antes de renderizar. [docs/FALSE_COLOR.md](docs/FALSE_COLOR.md)

**Data level.** Si Resolve decodifica un clip en la escala de code values equivocada, el nodo la corrige antes de la
curva log. La opción *Corregir el Data Level* del script lo arregla para todo el proyecto.
[docs/DATA_LEVELS.md](docs/DATA_LEVELS.md)

**Tone mapping con salida de escena (3.2.0).** Con un rendering (ACES 2.0, Filmico, OpenDRT…), un Spazio colore y
una Gamma de escena como DaVinci WG / DaVinci Intermediate ya no se sustituyen por Rec.709 / Gamma 2.4: la imagen
renderizada para Rec.709 viaja en esa codificación, para pasarla a otros plugins (Dehancer…), y un CST a Rec.709 /
Gamma 2.4 sin tone mapping la devuelve idéntica. Las salidas de pantalla (Rec.709, P3, Rec.2020) no cambian.

**Sin metadatos.** Un ProRes de grabador externo, o un clip que no se puede leer, deja el nodo neutro. Marca
*Spazio colore › Senza metadata › Sblocca controlli* y escribe el EI, los Kelvin y el tinte de rodaje: los controles se activan
y el nodo empieza neutro.

**Velocidad.** Abrir un proyecto no lee ningún archivo. Abrir el panel espera como mucho medio segundo; un disco lento
termina en segundo plano, con un límite de 15 segundos. *Rileggi* responde en unos 2 segundos como máximo.

---

## El nodo Log MetaRaw Creative

Un único motor que rehace **el proceso**, no una imitación de su resultado. La luz de escena, decodificada a Rec.2020
lineal, se convierte en hasta tres **registros** con su sensibilidad espectral. Cada uno se expone en la curva del material
y se convierte en imagen como lo hacía ese proceso: colorantes sustractivos, filtros aditivos, un depósito de plata o de
platino sobre papel, una señal de vídeo. Cada proceso es una tabla de números y el panel muestra solo los controles del
proceso elegido.

| Proceso | Qué modela |
|---|---|
| **Negativo color y copia** | tres capas con su contraste, pie y hombro, interimagen, grano por capa, halo, copia positiva con luces R G B; 14 películas, también del Este (Sovcolor, ORWO, Svema, Gevacolor). *Salida Log* da el escaneo del negativo |
| **Inversible**, **Blanco y negro** | diapositiva; B/N con sensibilidad azul, orto o pan y filtro de toma compensado |
| **Technicolor** tres tiras y dos colores | tres negativos del prisma, matrices por imbibición, clave K hasta 1944; solo rojo y verde |
| **Autochrome**, **Dufaycolor** | mosaico aditivo de granos de fécula teñidos o retícula regular, visto por transparencia |
| **Tres placas**, **Kinemacolor** | exposiciones en secuencia: con *Intervallo* > 0 el nodo lee los fotogramas siguientes y lo que se mueve se separa en colores (Prokudin-Gorski) |
| **Daguerrotipo**, **Colodión**, papel salado y albúmina, cianotipia, platino | depósitos sensibles al azul y al UV, espejo y oxidación, soporte, borde, virado |
| **Vídeo analógico** | 11 formatos, de broadcast a VHS LP y Betacam SP: ancho de banda de luma y color, retardo del color, peaking, ruido, cambio de cabezas, dropouts |
| **Digital de época** | 10 cámaras: clip y knee, nitidez electrónica, ruido, submuestreo, banding |

- Va **justo después** de Log MetaRaw. Decodifica a luz lineal y sale en Rec.709 · Gamma 2.4 (la copia proyectada), o en
  la codificación de entrada con *Salida Log*.
- Es **espacial**: Generate LUT lo excluye. Los radios siguen la altura del fotograma. Ruido determinista: CPU y Metal
  dan la misma imagen.
- Sustituye al nodo Detail en el mismo lugar: un etalonaje guardado con el Detail se abre como un Creative nuevo.

Los números son estimaciones de la investigación en [docs/CREATIVE.md](docs/CREATIVE.md), no perfiles de productos: hay
que ajustarlos con material real. Aún no probado en Resolve; Metal verificado por los tests de la CI de macOS.

---

## Los nodos Noise, Stabilize y Motion Blur (4.0.0)

Trabajan sobre la imagen de la cámara, antes de **Log MetaRaw**.

- **Log MetaRaw Noise** sabe cuánto ruido esperar y dónde. Del registro del script lee la cámara, el ISO (o el EI y
  el ISO nativo), el códec, los bits por píxel y el 4:2:0. Tiene perfiles para Sony FX6 y FX30, iPhone 17 Pro y DJI
  Osmo Action 4 y 5 Pro. El botón **Rileggi metadata** los vuelve a leer del archivo (se leen solos al abrir el panel si faltan). Para ajustarlo al ruido real marca **Mostra campione**, lleva las dos esquinas del recuadro a una zona plana y pulsa **Usa il campione** (como Neat Video); bandas, sombras y piel están en el grupo **Avanzate**.
  - *Temporal*: de ±1 a ±3 fotogramas vecinos, alineados por bloques y ponderados píxel a píxel, para que las partes
    en movimiento y los cortes no se empasten.
  - *Espacial*: luminancia y color por separado, en 5 bandas wavelet. Cada coeficiente se compara con la energía de su
    vecindario, lo que conserva la textura.
  - Vistas del ruido eliminado, protección de detalle y piel, y **Salta Noise ovunque** para apagar todos los nodos
    Noise mientras montas.
  - En Mac todo funciona en la GPU Metal, y los tests comprueban que da lo mismo que la CPU. En los demás sistemas
    funciona en la CPU.
  - Los perfiles son estimaciones hasta que se calibren con muestras reales. [docs/DENOISE.md](docs/DENOISE.md)
- **Stabilize** lee el giroscopio del clip (Gyroflow integrado). Tiene estilos del micromovimiento a la grúa y un
  **MASTER** animable. **Motion Blur** reconstruye el obturador a partir del flujo óptico, por ejemplo de 50p a 90° a
  25p a 180°, y conserva el grano del sensor en las zonas movidas. Ambos vienen de Mecena Motion (`motion/`) y los instala el paquete de macOS, solo Apple silicon.
- **Adquisición profunda**, una casilla en la ventana del script: guarda el giroscopio y el objetivo de cada fotograma
  en la MecenaClipDB, para Stabilize.

Ninguno de los tres nodos se ha probado todavía en Resolve con clips reales.

## Actualizaciones y privacidad

- Los **nodos** preguntan a GitHub por la última release de este proyecto como mucho una vez al día, en segundo plano.
  La petición lleva solo la versión del programa (`User-Agent: SLogMetaRaw/4.1.0`). Para desactivarlo crea el archivo
  vacío `~/Library/Application Support/SLogMetaRaw/no_update_check`.
- El **script** solo consulta cuando haces clic en su versión.
- Un clic solo abre un enlace de descarga de las releases de GitHub de este proyecto. Nada se instala sin ti.

---

## Cómo funciona, en breve

- **Metadatos.** Los parsers son Python puro, sin dependencias. Leen los metadatos de adquisición SMPTE RDD 18 fotograma a
  fotograma (pista `rtmd` de los MP4, paquetes ST 436 de los MXF), el XML NonRealTimeMeta de Sony, el SPS H.264/HEVC y el
  descriptor de imagen MXF. Un clip de varios gigabytes cuesta unos 100 KB de lectura.
- **Del script al nodo.** Cada clip tiene un registro JSON en la caché. El nodo encuentra su archivo por la ruta de origen
  que Resolve le da, lee ese registro y, si falta, lo crea en segundo plano.
- **Matemática del color.** La cadena es: decodificación de la curva de cámara, relación de EI, balance Bradford desde el
  locus de Planck (con el tinte perpendicular en CIE 1960 uv), luego tonos con una sola ganancia por píxel a partir de una
  norma de sus canales (la cromaticidad se conserva), luego color, luego salida. Todo en luz lineal, con las curvas y
  gamuts publicados por Sony.
- **Una matemática, tres implementaciones.** La matemática está en `ofx/SLogMetaRaw/math` y se compila como C++ y como
  Metal. Un modelo Python de referencia verifica ambas: CPU y GPU coinciden dentro de 2·10⁻⁴, con FMA desactivado en todas
  las rutas.
- **Tests.** 520 tests automáticos: parsers, modelos de referencia, C++, Metal, un host OpenFX en miniatura que carga los
  dos nodos como lo hace Resolve, y archivos golden de los paneles.

---

## Compilar desde el código

```bash
make -C ofx/SLogMetaRaw                     # plugin universal, arm64 + x86_64 (requiere Xcode)
make -C ofx/SLogMetaRaw test-bins           # binarios de test
python3 -m unittest discover tests          # los tests
./install.sh --dev                          # script y plugin apuntando a esta carpeta
./packaging/build_installer.sh              # dist/SLogMetaRaw-<versión>.dmg
python3 -m slogmetaraw clip.MP4             # lectura estilo Catalyst desde la línea de comandos
```

```
slogmetaraw/         parsers, escritura en Resolve, ventana del script, comprobación de actualizaciones
resolve_script/      lanzador para Workspace › Scripts
ofx/SLogMetaRaw/     math/ (compartida CPU/Metal), src/ (common, develop, detail, noise), metal/
motion/              Mecena Motion: Stabilize, Motion Blur, mecena-clipdb (Rust + C++, macOS)
tools/               generadores de matrices, iconos y gráficos
tests/               tests y modelos de referencia (los clips de prueba no están en el repositorio)
packaging/           instalador, guías, desinstalador
docs/                TONE_MAPPING, CREATIVE, DENOISE, FALSE_COLOR, DATA_LEVELS
```

### Linux y Windows (renderizado por CPU, experimental)

El plugin también compila para Linux y Windows con CMake. Estos ports renderizan por CPU (todavía sin Metal, CUDA ni
OpenCL) y no se han probado dentro de DaVinci Resolve: la build compatible sigue siendo la de macOS.

**Instalación en Linux** (descarga desde **Releases** el archivo adecuado para tu distribución):

| Archivo | Para |
|---|---|
| `slogmetaraw_<versión>_amd64.deb` | Ubuntu, Debian, Mint: `sudo apt install ./slogmetaraw_*.deb` |
| `slogmetaraw-<versión>-1.x86_64.rpm` | Rocky, Alma, RHEL, CentOS, Fedora: `sudo dnf install ./slogmetaraw-*.rpm` |
| `SLogMetaRaw-<versión>-linux-x86_64.run` | cualquier distribución: `sh SLogMetaRaw-*.run` (`--user` sin sudo, `--uninstall`) |
| `SLogMetaRaw-<versión>-linux-x86_64.tar.gz` | lo mismo, sin empaquetar: ejecuta `install.sh` |

El plugin va a `/usr/OFX/Plugins`; la librería Python a `/usr/lib/slogmetaraw` (.deb/.rpm) o a
`~/.local/share/SLogMetaRaw` (.run, .tar.gz). El paquete coloca el script de menú en
`/opt/resolve/Fusion/Scripts/Utility` si Resolve está ahí; si no, ejecuta una vez `slogmetaraw-setup` (para tu
usuario). Reinicia Resolve. Basta con Python 3.6 (incluso en Rocky/RHEL 8). El binario se compila contra glibc 2.28:
se carga en Rocky/Alma/RHEL 8+, Ubuntu 20.04+ y Debian 10+.

```bash
# compílalo tú mismo (cmake, un compilador C++17, python3; el SDK OpenFX se descarga, o fija OFX_SDK_DIR)
./packaging/linux/build_package.sh          # dist/: .tar.gz, .run, .deb, .rpm
# Windows (PowerShell): cmake, Visual Studio Build Tools, python; Inno Setup es opcional
powershell -File packaging\windows\build_package.ps1   # dist\...-windows-x64.zip (+ instalador .exe con Inno Setup)
```

El estado vive en `~/.local/share/SLogMetaRaw` en Linux y `%APPDATA%\SLogMetaRaw` en Windows. `.github/workflows/`
compila los paquetes e instala el .deb/.rpm en Rocky 8/9, Ubuntu 20.04-24.04 y Debian 12 en cada cambio; empujar una
etiqueta `v*` los adjunta a una release en borrador.

---

## Límites conocidos

- El panel Camera Raw de Resolve y su estabilización por giroscopio no se pueden desbloquear para MP4 (para el giroscopio está el nodo Stabilize): viven dentro de los
  decodificadores de Resolve. Log MetaRaw reconstruye los controles de color; no abre una puerta dentro de Resolve.
- Algunas cámaras (entre ellas la a6300) no registran los Kelvin: entonces se estiman a partir del preset de luz y se
  señalan.
- S-Log2 sigue el documento de Sony. La curva S-Log2 de Resolve difiere en unos 0,15 pasos.
- Pendiente de comprobar con más archivos: XAVC HS (HEVC), HLG, S-Cinetone, zooms motorizados.
- **Un proyecto independiente y aficionado**, distribuido tal cual, sin garantía y sin responsabilidad por el uso
  profesional. El código es abierto: léelo, pruébalo, cámbialo.

---

## Créditos y licencia

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m).
Escrito con Claude (Anthropic): la 1.0 el 19 de septiembre de 2026, la 1.1.0 el 22 de septiembre, la 2.0.0 el 23 de
septiembre de 2026, la 3.2.0 y la 4.0.0 el 24 de noviembre de 2026. La historia completa está en [RELEASE_NOTES.md](RELEASE_NOTES.md) (en italiano).

Referencias para las etiquetas Sony: SMPTE RDD 18, [ExifTool](https://exiftool.org) y
[telemetry-parser](https://github.com/AdrianEddy/telemetry-parser) de AdrianEddy (MIT), de donde viene parte de la tabla
de etiquetas.

Rendering y datos de cámara: el Output Transform ACES 2.0 está portado de OpenColorIO 2.6 y ACES 1.3 se comprueba
con sus transformaciones integradas (ACES: Academy of Motion Picture Arts and Sciences); [OpenDRT](https://github.com/jedypod/open-display-transform)
v1.1.0 es de Jed Smith (GPL-3.0), portado de su DCTL. La curva y el gamut DJI D-Log M vienen de las mediciones de la Osmo
Action 6 en [jens-krause/dlogm-dctl](https://github.com/jens-krause/dlogm-dctl) v1.3 y de la LUT DJI de la Action 6, usada solo
como referencia y no distribuida; hasta la 3.1.x la curva era el ajuste de Thatcher Freeman.

Stabilize se basa en [Gyroflow](https://github.com/gyroflow/gyroflow) de AdrianEddy y colaboradores (GPL-3.0) y en su
catálogo de objetivos. El filtro espacial del nodo Noise sigue NeighShrink (Chen, Bui y Krzyżak, 2004).

[GNU GPL v3.0 o posterior](LICENSE). El SDK OpenFX es © The Open Effects Association (BSD-3). Sony, XAVC y Catalyst son
marcas de Sony Group Corporation; DaVinci Resolve es una marca de Blackmagic Design. Proyecto independiente, no afiliado
ni respaldado por ninguna de las dos empresas.

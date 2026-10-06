# S-Log MetaRaw — Guía del usuario

[English](Wiki-Software) · [Italiano](Wiki-Software-it) · **Español** · [Português](Wiki-Software-pt) · [简体中文](Wiki-Software-zh)

> Versión **2.3.0** · macOS 12+ · DaVinci Resolve 20 / 21 · Sony XAVC `.MP4` / `.MXF`
> Probado solo en DaVinci Resolve Studio 21.1 en macOS.

S-Log MetaRaw lee los datos de rodaje que las cámaras Sony escriben en cada archivo (Kelvin, tinte, EI, objetivo, diafragma, obturador, perfil de color) y los usa dentro de DaVinci Resolve. Resolve hace esto solo con los MXF de la FX6/FX9; en los MP4 de las series FX30, FX3, a7 y a6000 ignora esos datos.

**Contenido**

1. [Qué es — y qué no es](#1-qué-es--y-qué-no-es)
2. [Instalación](#2-instalación)
3. [Guía rápida](#3-guía-rápida)
4. [El script](#4-el-script)
5. [El nodo S-Log MetaRaw](#5-el-nodo-s-log-metaraw)
6. [Falso color](#6-falso-color)
7. [Tonos y Zonas](#7-tonos-y-zonas)
8. [El nodo S-Log MetaRaw Detail](#8-el-nodo-s-log-metaraw-detail)
9. [Flujos de trabajo: dónde va cada nodo](#9-flujos-de-trabajo-dónde-va-cada-nodo)
10. [Recetas](#10-recetas)
11. [Casos especiales](#11-casos-especiales)
12. [Actualizaciones y privacidad](#12-actualizaciones-y-privacidad)
13. [Desinstalar](#13-desinstalar)
14. [Solución de problemas / Preguntas frecuentes](#14-solución-de-problemas--preguntas-frecuentes)
15. [Límites conocidos](#15-límites-conocidos)

---

## 1. Qué es — y qué no es

Tres herramientas, una sola instalación:

| Herramienta | Dónde, en Resolve | Qué hace |
|---|---|---|
| **Script** | Workspace › Scripts › S-Log MetaRaw | Lee los metadatos de todos los clips y los escribe en el Media Pool. Corrige el Data Level de cada clip. |
| Nodo **S-Log MetaRaw** | Color › OpenFX — **primer nodo** | Revela un clip a partir de sus valores de rodaje: balance de blancos, exposición, espacio de color, tonos por zonas, falso color. |
| Nodo **S-Log MetaRaw Detail** | Color › OpenFX — **justo después** | El nodo creativo: recuperación local de altas luces/sombras, Texture, Clarity, Dehaze. |

Los archivos originales **nunca se modifican**: sin transcodificación, sin reempaquetado.

**No es raw.** Un MP4 log ya está demosaicado y comprimido (8 o 10 bits, a menudo 4:2:0, con la reducción de ruido de la cámara ya aplicada). El nodo aplica la ciencia del color con rigor — exposición y balance de blancos en luz lineal, a partir de los valores registrados, con las curvas y gamuts publicados por Sony — así que la imagen *se comporta* de un modo que recuerda al raw: el balance de blancos se desplaza limpio, la exposición se mueve como un paso de luz, las altas luces se redondean en vez de romperse. Si fuerzas los límites de la cámara, la falta de información se nota: banding en los cielos, ruido en las sombras levantadas, las altas luces quemadas siguen quemadas. Expón bien en rodaje.

---

## 2. Instalación

1. Descarga `SLogMetaRaw-2.3.0.dmg` desde [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) y ábrelo.

   ![La ventana del DMG](images/01-dmg.png)

2. Haz doble clic en **Installa S-Log MetaRaw.pkg**. El paquete no está firmado con un certificado de Apple: la primera vez, **clic derecho › Abrir**. Pide la contraseña de tu Mac porque el plugin va en una carpeta del sistema.

   ![Clic derecho › Abrir en el paquete sin firmar](images/02-gatekeeper.png)

3. **Reinicia DaVinci Resolve.**

El instalador también borra la caché de plugins de Resolve (`OFXPluginCacheV2.xml`); Resolve la reconstruye en el siguiente arranque. Sin eso, Resolve seguiría mostrando el panel antiguo y no vería el nodo Detail.

Cada instalación empieza limpia: el plugin y la biblioteca anteriores se sustituyen por completo, y se eliminan las instalaciones de desarrollo.

**Qué va dónde**

| Elemento | Ruta |
|---|---|
| Los dos nodos (un solo bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Biblioteca Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Script del menú | `…/DaVinci Resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py` |
| Caché por clip (JSON) | `~/Library/Application Support/SLogMetaRaw/cache` |

**Requisitos:** macOS 12 o posterior, Apple silicon o Intel, DaVinci Resolve 21 o 20 (Resolve 20: instala Python 3 desde python.org, no trae ninguno propio).
**Clips:** Sony XAVC en `.MP4` o `.MXF` — el script los lee todos. Los nodos revelan **S-Log3** (S-Gamut3.Cine o S-Gamut3), **S-Log2** y **S-Log** (S-Gamut). Con otros perfiles (Cine, HLG, S-Cinetone) se quedan neutros y lo indican.

---

## 3. Guía rápida

1. Importa el material. Abre **Workspace › Scripts › S-Log MetaRaw**, pulsa **1 · Leer metadatos** y luego **2 · Escribir en Resolve**.
2. En la página Color, añade **S-Log MetaRaw** como **primer nodo**. Toma el EI, los Kelvin y el tinte del clip: con esos valores no cambia nada.
3. Corrige el balance de blancos y la exposición en el nodo, usando las vistas de falso color.
4. Da forma a los tonos con **Toni**. Para recuperación local, Texture, Clarity o Dehaze, añade **S-Log MetaRaw Detail** como nodo siguiente.
5. Después el resto del etalonaje y, al final, tu CST / LUT / DRT de salida.

```
S-Log MetaRaw  →  S-Log MetaRaw Detail  →  resto del etalonaje  →  CST / LUT / DRT de salida
```

![Árbol de nodos recomendado](images/08-node-tree.png)

> Copia el nodo en otro clip y se reinicia con los datos de ese clip.

---

## 4. El script

**Workspace › Scripts › S-Log MetaRaw**

![Menú Workspace › Scripts](images/03-scripts-menu.png)

La ventana tiene una fila de botones, una fila de opciones y la lista de clips. Sigue el idioma de Resolve (inglés, italiano, español, portugués, chino simplificado). El script lee archivos; nunca los modifica.

![Ventana del script después de Read metadata](images/04-script-window.png)

### Botones

| Control | Qué hace |
|---|---|
| **Menú de clips** | *Todo el Media Pool* o *Clips seleccionados en el Media Pool*. |
| **1 · Leer metadatos** | Una fila por clip: cámara, objetivo, diafragma, obturador, EI, WB, espacio de color, data level. La columna **Estado** dice `leído`, qué cambió durante la toma (diafragma, foco…) o por qué se saltó un clip. Haz clic en una fila para ver todo lo leído, agrupado como en Catalyst Browse. |
| **2 · Escribir en Resolve** | Rellena los campos del Media Pool (panel Metadata, columnas, palabras clave para smart bins, Camera Notes, data burn-in) y corrige valores que Resolve lee mal de los MXF, p. ej. *Camera Aperture* `F53343` en la FX6. |
| **Exportar CSV** | Exporta los valores para los que Resolve no tiene campo (EI, tinte, modo WB, distancia de foco, gamma de captura…) en el formato CSV de metadatos de Resolve. Impórtalo con **File › Import › Metadata**, con *create custom fields* activado. |
| **Versión** (abajo a la derecha) | Haz clic para consultar GitHub; se pone verde cuando hay una versión más reciente. |

![Detalle de clip, al estilo Catalyst](images/05-script-clip-detail.png)

### Opciones

| Opción | Por defecto | Efecto |
|---|---|---|
| **Etiqueta por cámara** | apagada | Añade cámara, gamma y primarios a las palabras clave. |
| **Sobrescribir metadatos** | encendida | Sustituye los valores que Resolve ya escribió. Apagada: solo rellena los campos vacíos. |
| **Corregir el Data Level** | encendida | Pone el *Data Level* de cada clip en **Full** (log) o **Video** (709, Cine, HLG). Es el arreglo real: vale para todo el proyecto, scopes y exportaciones incluidos. |
| **Establecer también Input Color Space** | apagada | Para proyectos con gestión de color. ⚠️ Un script no puede devolverlo a *Project* — solo tú, a mano. |

Después de **2 · Escribir en Resolve**, los datos aparecen en el Media Pool:

![Panel Metadata del Media Pool relleno](images/06-media-pool-metadata.png)

**Velocidad.** No se decodifica nada: como máximo 24 muestras de la pista de metadatos por clip, en un segundo. Un archivo de varios GB cuesta unos 100 KB de lectura. Un disco que deja de responder se salta una sola vez, con un aviso, en lugar de bloquear la lista. Ejecutar el script justo después de importar hace que los nodos encuentren los datos listos.

---

## 5. El nodo S-Log MetaRaw

**Color › OpenFX › S-Log MetaRaw** — primer nodo, antes de cualquier CST o LUT.

![Biblioteca OpenFX con los dos nodos](images/07-openfx-library.png)

El nodo es **puntual**: cada píxel depende solo de sí mismo. Nunca crea halos, y **Generate LUT** puede exportarlo (se recomiendan 65 puntos). Los ajustes se guardan por clip.

> Los paneles de los nodos están en italiano. Las etiquetas de abajo llevan su traducción.

![Panel principal del nodo — arriba](images/09-node-panel-top.png)

| Control | Qué hace |
|---|---|
| **Version** (arriba) | Muestra `v2.3.0`. Una vez al día pregunta a GitHub por la última versión; si la hay, muestra **🟢 v2.3.0 → 2.x.y** y un clic abre la descarga del instalador para tu sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Nunca instala nada por sí mismo. |
| **Camera** · **Rileggi metadata** (releer) | La cámara que se leyó. *Rileggi* vuelve a leer el clip, devuelve cada control a los valores de cámara y escribe los metadatos del clip en el Media Pool. Responde en ~2 s. |
| **Decode Using** | *Clip* permite cambiar los controles; *Camera metadata* los bloquea en los valores de rodaje (el nodo queda transparente). |
| **White Balance** | As shot, o presets (Daylight, Cloudy, Shade, Tungsten, Fluorescent, Flash). Mover un deslizador lo cambia a *Custom*. |
| **Color Temp** · **Tint** | Adaptación cromática Bradford en luz lineal, desde el blanco que registró la cámara. Más Temp calienta; Tint positivo va hacia el magenta. |
| **Exposure** | En EI: el doble de EI = exactamente +1 paso, en luz lineal, antes de cualquier curva. |
| **False color** | Vistas de temperatura, tinte y exposición — ver [§6](#6-falso-color). |
| **Color Space** · **Gamma** | Salida, como un Color Space Transform. *Timeline* no convierte — déjalo ahí en un proyecto con gestión de color. Para etalonar en DWG: *DaVinci WG · DaVinci Intermediate*. |
| **Toni** (Tonos) | Contrast, Highlights, Shadows, Whites, Bianco, Blacks, Vibrance, Saturation — ver [§7](#7-tonos-y-zonas). |
| **Zone** (Zonas, cerrado) | Zonas Black, Shadow, Light, Specular; Contrast Pivot; Soft Clip. |
| **Avanzate** (Avanzado) | Entrada del nodo, corrección del data level, estado, *Sblocca controlli senza metadata* (desbloquear sin metadatos). |
| **Dati di ripresa** (datos de rodaje) | Solo lectura: objetivo, focal, diafragma, foco, obturador, EI, WB, fps, ND, LUT de cámara. |

![Avanzate y Dati di ripresa](images/14-avanzate-dati.png)

**Avanzate en detalle**

- **Ingresso nodo** (entrada del nodo) — *Automatico* pregunta a Resolve.
- **Data level in ingresso** — corrige la escala de code values cuando Resolve decodifica un clip en la equivocada. La opción *Corregir el Data Level* del script lo arregla para todo el proyecto.
- **Stato** (estado) — lo que detectó el nodo; también avisa cuando una vista de falso color está activa.
- **Sblocca controlli senza metadata** — ver [§11](#11-casos-especiales).

> La **salida Rec.709** del nodo es un CST *sin* tone mapping (las altas luces por encima de Bianco se queman) y excluye el nodo Detail. Prefiere un CST de salida al final del árbol.

---

## 6. Falso color

Una vista por control, situada encima del deslizador al que sirve. **La vista sustituye la imagen — apágala antes de renderizar.** (Un plugin OpenFX no puede dibujar una superposición en el visor de Resolve.)

Pon primero *Decode Using* en **Clip**: en *Camera metadata* los controles están bloqueados.

### Exposición

Bandas en pasos al estilo ARRI, alrededor del gris del 18%. Todo lo demás se vuelve gris.

![Bandas de exposición](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/falsecolor_bands.png)

| Color | Significado |
|---|---|
| Verde | Gris medio (18%) |
| Rosa | Un paso por encima — piel |
| Amarillo | Cerca del clip |
| Rojo | Clip (quemado) |
| Azul / violeta | Sombra profunda / negro |

Activa la vista, apunta al gris medio o a la piel, y mueve **Exposure** hasta que la zona correspondiente se ponga verde (gris) o rosa (piel).

![Falso color de exposición en el visor](images/10-falsecolor-exposure.png)

### Temperatura y Tinte

Funcionan como en CineMatch. La imagen se vuelve gris; las dominantes se muestran en color, y las dominantes casi neutras se amplifican hasta 8× para que se vean.

| Vista | Ves | Haz |
|---|---|---|
| Temperature | Azul (dominante fría) | Sube Color Temp |
| Temperature | Naranja (dominante cálida) | Baja Color Temp |
| Tint | Verde | Sube Tint |
| Tint | Magenta | Baja Tint |

Elige una superficie que deba ser neutra y mueve el deslizador hasta que se quede gris. Alterna Temp y Tint un par de veces; converge. Cada vista reacciona solo a su propio deslizador.

![Falso color de temperatura](images/11-falsecolor-temp.png)

Más detalle (en italiano): [docs/FALSE_COLOR.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/FALSE_COLOR.md)

---

## 7. Tonos y Zonas

### Toni (Tonos)

![Grupo Toni](images/12-toni.png)

Todos los deslizadores van de −100 a +100, en el orden de Camera Raw. Highlights es un hombro de película; los demás son **exposiciones sobre una zona de tonos, en pasos desde el gris del 18%**: dentro de la zona la imagen se mueve como con una exposición, así que la textura se conserva. Ninguna combinación de deslizadores puede solarizar.

| Deslizador | Comportamiento |
|---|---|
| **Contrast** | Gira alrededor del Pivot. +100 duplica la pendiente en el pivote, −100 la reduce a la mitad, con los extremos acotados. |
| **Highlights** | Negativo: hombro de película — a −100 el valor más alto registrado (~+6 pasos sobre el gris en S-Log3) llega exactamente a **Bianco**, sin velo gris y sin clip. El gris y lo que está debajo no se mueven; la piel a +1 paso se desplaza 0,05 pasos como mucho. El tono (hue) se mantiene constante. Positivo: más fuerza. |
| **Bianco** (blanco, en pasos) | Dónde llega ese máximo, y el techo de Soft Clip. **2,5** = el blanco de Rec.709 a través de un CST sin tone mapping. **Con un DRT después (ACES, AgX, DaVinci) súbelo a 4–5**, o las altas luces se comprimen dos veces. |
| **Shadows** | Tonos por debajo de −1 paso; 100 = 2 pasos. También levanta el negro — contrólalo con Blacks. |
| **Whites** | Desde +3,5 pasos hasta el clip; 100 = 1 paso. |
| **Blacks** | Velo lineal: mueve el negro (−3 / +1 paso) sin mover el gris. |
| **Vibrance** | Alrededor de la luminancia; protege los tonos de piel. |
| **Saturation** | Alrededor de la luminancia; igual en cualquier espacio de color. |

**Azzera toni** reinicia el grupo.

![Highlights 0 frente a −100](images/15-highlights-before-after.png)

![Curva de tonos](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/tone_curve.png)

> **El coste honesto de un nodo puntual:** lo que comprime, lo comprime también en la textura. La recuperación que conserva la textura vive en el nodo Detail.

### Zone (Zonas)

![Grupo Zone](images/13-zone.png)

- Cuatro zonas, como en la paleta HDR de Resolve: **Black, Shadow, Light, Specular** — cada una con **Exp** (pasos), **Sat**, **Range** (borde, en pasos) y **Falloff** (ancho de la transición).
- **Contrast Pivot** — el tono alrededor del que gira Contrast.
- **Soft Clip** / **Soft Clip Color** — pliega las altas luces bajo un techo (en Bianco) que nunca alcanzan. *Color* decide si las altas luces plegadas van al blanco (película) o conservan su color. Contención, no recuperación.
- **Falso color de Zone** — colorea cada píxel con la zona que lo mueve.
- **Azzera zone** reinicia el grupo.

Los bordes de las Zone se trasladan a través de Contrast, así que se mantienen en pasos de escena sea cual sea el Contrast que uses.

### Generate LUT

El nodo es puntual, así que Generate LUT lo incluye. Usa **65 puntos**: con los deslizadores a ±100 el error se mantiene dentro de ~3,5 code values de S-Log3. Con 33 puntos, el toe lineal de S-Log3 (Shadows +100) llega a ~10 CV. Algunas Zone extremas (Black o Shadow +3) superan eso incluso a 65 — mantén el nodo activo para esos etalonajes.

Más detalle (en italiano): [docs/TONE_MAPPING.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/TONE_MAPPING.md)

---

## 8. El nodo S-Log MetaRaw Detail

**Color › OpenFX › S-Log MetaRaw Detail** — justo después de S-Log MetaRaw, antes de cualquier CST / LUT / DRT.

Trabaja **por áreas**, sobre una base que respeta los bordes: mueve grandes áreas de luz sin aplanar el detalle fino — la parte de Highlights/Shadows de Lightroom que un nodo puntual no puede hacer.

![Panel del nodo Detail](images/16-detail-panel.png)

| Grupo | Controles |
|---|---|
| **Gamma dinamica** (rango dinámico) | Local Contrast, Local Highlights, Local Shadows; vistas de comprobación *gain* y *base*. |
| **Presenza** (presencia) | **Texture** (detalle fino; no sube el grano por debajo del umbral de ruido), **Clarity** (contraste local de escala media), **Dehaze**. |
| **Zone locali** (zonas locales) | Las zonas del nodo principal, aplicadas a áreas en vez de a píxeles. |
| **Avanzate** (avanzado) | Preservación del detalle, radio, umbral de bordes (*Soglia bordi*), umbral de ruido, centro de Clarity, **Bianco** de Local Highlights, entrada del nodo. |
| **Velo** (bruma) | Nivel y color de la bruma que quita Dehaze. Los ajustas tú; nunca se estiman fotograma a fotograma → sin parpadeo. |

**Azzera dettaglio** reinicia el nodo.

**Local Highlights frente a Highlights.** Local Highlights comprime las grandes áreas luminosas y conserva — incluso refuerza — la textura fina: el cielo se oscurece y las nubes mantienen su detalle. El Highlights del nodo principal suaviza la textura de las altas luces, como la película. Local Highlights tiene su propio **Bianco** en Avanzate y no conoce el EI elegido en el nodo principal: si cambias mucho el EI, ajústalo.

![Local Highlights sobre un cielo, antes/después](images/17-detail-before-after.png)

**Reglas**

- Decodifica a luz lineal lo que recibe y lo vuelve a escribir en la **misma codificación**. Solo acepta codificaciones log de escena: S-Log3, S-Log2, DaVinci WG/Intermediate, ACEScct. **No** Rec.709, Gamma 2.4 ni sRGB.
- Es **espacial**: Generate LUT lo deja fuera, junto con el resto de su nodo. Mantenlo en un nodo propio.
- Los radios siguen la altura del fotograma: el mismo look a resolución completa, en proxy y en el visor. Sin estadísticas por fotograma → sin parpadeo.
- Metal: ~6–18 ms por fotograma UHD en Apple silicon. La alternativa por CPU es mucho más lenta.

**Límites medidos**

- Local Highlights −100: halo en el lado oscuro de un borde < 3% del escalón.
- Local Shadows o las zonas locales a ±100: ~12% en un borde nítido de 1 paso, 4–6% en bordes de 2–3 pasos. Si lo ves, baja *Soglia bordi*.
- Texture cerca de bordes fuertes puede aumentar el grano 1,25–1,7×.
- Dehaze necesita una bruma real que quitar.

Más detalle (en italiano): [docs/DETAIL.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DETAIL.md)

---

## 9. Flujos de trabajo: dónde va cada nodo

### A — Etalonar en DaVinci Wide Gamut (recomendado)

```
S-Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → Detail             Ingresso nodo: DaVinci WG/Intermediate
  → tu etalonaje (DWG)
  → CST de salida      DaVinci WG/Intermediate → Rec.709 · Gamma 2.4, tone mapping de DaVinci
```

Con tone mapping después, pon **Bianco en 4–5 en ambos nodos**. En una timeline sin gestión de color debes declarar la entrada del nodo Detail.

### B — Todo en log

```
S-Log MetaRaw          Color Space/Gamma: Timeline
  → Detail             Ingresso nodo: Automatico (usa el S-Log3 del clip)
  → tu etalonaje
  → CST / LUT          S-Gamut3.Cine/S-Log3 → tu salida
```

### C — Proyecto con gestión de color (DaVinci YRGB Color Managed / ACES)

Deja Color Space/Gamma en **Timeline** en el nodo principal. Activa *Establecer también Input Color Space* del script solo si quieres que el script fije la entrada de cada clip — recuerda que debes devolverla a *Project* a mano.

---

## 10. Recetas

| Problema | Dónde | Ajuste |
|---|---|---|
| Cielo o ventanas quemados | Main › Toni | Highlights −50…−100 (con un DRT después: Bianco 4–5) |
| …y conservar la textura de las nubes | Detail › Gamma dinamica | Local Highlights |
| Cara a contraluz en sombra | Main › Toni | Shadows +30…+60, Blacks −20…−40 |
| Look más "de película" | Main › Toni | Contrast +20…+30, Highlights −60 |
| Sombras saturadas y con ruido | Main › Zone | Shadow Sat −30…−50 |
| Reflejos especulares | Main › Zone | Specular Exp −1…−2 |
| Paisaje plano y con bruma | Detail › Presenza + Velo | Dehaze; ajusta a mano el nivel/color de la bruma |

---

## 11. Casos especiales

**ProRes de un grabador externo / clip ilegible.** El nodo se queda neutro. Marca **Avanzate › Sblocca controlli senza metadata** y escribe el EI, los Kelvin y el tinte de rodaje: los controles se activan y el nodo empieza neutro.

**Perfiles que no son log** (Cine, HLG, S-Cinetone): los nodos se quedan neutros y lo indican en *Stato*.

**Data Level equivocado.** Si los negros se ven levantados o aplastados nada más salir de cámara, Resolve está decodificando en la escala de code values equivocada. Ejecuta el script con *Corregir el Data Level* activado (arreglo para todo el proyecto), o usa *Avanzate › Data level in ingresso* en un solo clip. Más contexto (en italiano): [docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**Cámaras sin Kelvin** (p. ej. la a6300): el Kelvin se estima a partir del preset de luz y se señala.

**Rendimiento.** Abrir un proyecto no lee ningún archivo. Abrir el panel espera como mucho medio segundo; un disco lento termina en segundo plano, con un límite de 15 s.

---

## 12. Actualizaciones y privacidad

- Los **nodos** preguntan a GitHub por la última versión como mucho una vez al día, en segundo plano. La petición lleva solo la versión (`User-Agent: SLogMetaRaw/2.3.0`).
- El **script** consulta solo cuando haces clic en su versión.
- Un clic solo abre un enlace de descarga de las releases de GitHub de este proyecto. Nada se instala sin ti.

![Insignia de actualización](images/18-update-badge.png)

**Desactivar la comprobación:** crea el archivo vacío

```bash
touch ~/Library/Application\ Support/SLogMetaRaw/no_update_check
```

**Actualizar desde 1.x:** los tonos de la 1.x (Highlights, Shadows, Contrast, Saturation, Color Boost, Color Recovery) no se pueden convertir y se reinician a cero. Para conservar el look de un clip ya terminado, **antes de actualizar** ejecuta Generate LUT en ese nodo. Después de la actualización, *Stato* muestra lo que había (p. ej. "Toni 1.1 azzerati (H −40, C +15)"). Exposure, data level, Color Space y Gamma no cambian.

---

## 13. Desinstalar

Haz doble clic en **Disinstalla S-Log MetaRaw.command**, en el DMG (la primera vez: clic derecho › Abrir). Hace esto:

1. enumera todo lo que va a eliminar (todas las versiones 1.x y 2.x, el antiguo nombre *SonyMeta*, instalaciones de desarrollo, caché, ajustes, registros);
2. te pide cerrar Resolve;
3. pregunta si también quieres borrar los CSV exportados;
4. comprueba que no queda nada.

Ejecútalo con `--dry-run` para ver qué eliminaría sin tocar nada. Los metadatos ya escritos en los proyectos de Resolve se quedan — forman parte de los proyectos.

![Desinstalador en Terminal](images/20-uninstaller.png)

---

## 14. Solución de problemas / Preguntas frecuentes

**Falta el nodo Detail / el panel se ve antiguo.**
Resolve está usando su caché de plugins antigua. Cierra Resolve, borra `OFXPluginCacheV2.xml` (el instalador normalmente ya lo hace) y reinicia.

**El nodo no hace nada.**
Es así a propósito con los valores de rodaje. Comprueba que *Decode Using* esté en **Clip**, y lee *Avanzate › Stato*: indica si el perfil no es log o si faltan metadatos.

**Mi render tiene falso color.**
Se quedó una vista encendida. Apaga todas las vistas de falso color antes de renderizar.

**Las altas luces se ven comprimidas dos veces / apagadas.**
Tienes un DRT (ACES, AgX, tone mapping de DaVinci) después del nodo: sube **Bianco** a 4–5 (también en el nodo Detail).

**Halos alrededor de los bordes con el nodo Detail.**
Baja *Avanzate › Soglia bordi*, o reduce Local Shadows / las zonas locales.

**El script se saltó un clip.**
Lee la columna *Estado*: dice por qué (archivo no admitido, disco sin responder, etc.).

**"Camera Aperture F53343" en la FX6.**
Es una lectura incorrecta de Resolve del MXF; **2 · Escribir en Resolve** lo corrige.

**macOS no abre el instalador.**
No está firmado: clic derecho › Abrir (o System Settings › Privacy & Security › Open Anyway).

---

## 15. Límites conocidos

- El panel Camera Raw de Resolve y la estabilización por giroscopio no se pueden desbloquear para MP4: viven dentro de los decodificadores de Resolve. S-Log MetaRaw reconstruye los controles de color, no abre una puerta dentro de Resolve.
- S-Log2 sigue el documento de Sony; la curva S-Log2 de Resolve difiere en ~0,15 pasos.
- Pendiente de comprobar con más archivos: XAVC HS (HEVC), HLG, S-Cinetone, zooms motorizados.
- Probado solo en Resolve Studio 21.1 en macOS.
- Proyecto independiente y aficionado, distribuido tal cual, sin garantía ni responsabilidad por el uso profesional. No afiliado ni respaldado por Sony ni Blackmagic Design.

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0+](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [Notas de la versión](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)

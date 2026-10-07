# S-Log MetaRaw — Guía de usuario

[English](Wiki-Software) · [Italiano](Wiki-Software-it) · **Español** · [Português](Wiki-Software-pt) · [简体中文](Wiki-Software-zh) · [Home](Home-es)

> Versión **2.3.1** · macOS 12+ · DaVinci Resolve 20 / 21 · Sony XAVC `.MP4` / `.MXF`
> Probado solo en DaVinci Resolve Studio 21.1 en macOS.

S-Log MetaRaw lee los datos de rodaje que las cámaras Sony escriben en cada archivo (Kelvin, tinte, EI, objetivo, diafragma, obturador, perfil de color) y los usa dentro de DaVinci Resolve. Resolve hace esto solo con los MXF de FX6/FX9; en los MP4 de FX30, FX3, serie a7 y a6000 los ignora.

**Contenido**

1. [Qué es y qué no es](#1-qué-es-y-qué-no-es)
2. [Instalación](#2-instalación)
3. [Inicio rápido](#3-inicio-rápido)
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
14. [Solución de problemas / FAQ](#14-solución-de-problemas--faq)
15. [Linux y Windows (experimental)](#15-linux-y-windows-experimental)
16. [Límites conocidos](#16-límites-conocidos)

---

## 1. Qué es y qué no es

Tres herramientas, una sola instalación:

| Herramienta | Dónde en Resolve | Qué hace |
|---|---|---|
| **Script** | Workspace › Scripts › S-Log MetaRaw | Lee los metadatos de todos los clips y los escribe en el Media Pool. Corrige el Data Level de cada clip. |
| Nodo **S-Log MetaRaw** | Color › OpenFX — **primer nodo** | Revela un clip a partir de sus valores de rodaje: balance de blancos, exposición, espacio de color, tonos por zonas, falso color. |
| Nodo **S-Log MetaRaw Detail** | Color › OpenFX — **justo después** | El nodo creativo: recuperación local de altas luces/sombras, Texture, Clarity, Dehaze. |

Los archivos originales **nunca se modifican**: ni transcodificación ni rewrapping.

**No es raw.** Un MP4 log ya está demosaicado y comprimido (a 8 o 10 bits, a menudo 4:2:0, con la reducción de ruido de la cámara ya aplicada). El nodo aplica la ciencia del color con rigor — exposición y balance de blancos en luz lineal, a partir de los valores registrados, con las curvas y gamuts publicados por Sony — así que la imagen *se comporta* de un modo que recuerda al raw: el balance se desplaza limpio, la exposición se mueve como un paso de luz, y las altas luces se redondean en vez de romperse. En cuanto se fuerzan los límites de la cámara, la información que falta se nota: banding en los cielos, ruido en las sombras levantadas, altas luces quemadas que siguen quemadas. Expón bien en rodaje.

---

## 2. Instalación

1. Descarga `SLogMetaRaw-2.3.1.dmg` desde [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) y ábrelo.

   ![La ventana del DMG](images/01-dmg.png)

2. Haz doble clic en **Installa S-Log MetaRaw.pkg**. El paquete no está firmado con un certificado de Apple: la primera vez, **clic derecho › Abrir**. Pide la contraseña de tu Mac porque el plugin va en una carpeta del sistema.

   ![Clic derecho › Abrir en el paquete sin firmar](images/02-gatekeeper.png)

3. **Reinicia DaVinci Resolve.**

El instalador también borra la caché de plugins de Resolve (`OFXPluginCacheV2.xml`); Resolve la reconstruye en el siguiente arranque. Sin eso, Resolve seguiría mostrando el panel antiguo y no vería el nodo Detail.

Cada instalación empieza limpia: el plugin y la biblioteca anteriores se sustituyen por completo, y las instalaciones de desarrollo se eliminan.

**Qué va dónde**

| Elemento | Ruta |
|---|---|
| Los dos nodos (un solo bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Biblioteca Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Script del menú | `…/DaVinci Resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py` |
| Caché por clip (JSON) | `~/Library/Application Support/SLogMetaRaw/cache` |

**Requisitos:** macOS 12 o posterior, Apple silicon o Intel, DaVinci Resolve 21 o 20 (Resolve 20: instala Python 3 desde python.org, no trae uno propio).
**Clips:** Sony XAVC en `.MP4` o `.MXF` — el script los lee todos. Los nodos revelan **S-Log3** (S-Gamut3.Cine o S-Gamut3), **S-Log2** y **S-Log** (S-Gamut). Con otros perfiles (Cine, HLG, S-Cinetone) se quedan neutros y lo dicen.

---

## 3. Inicio rápido

1. Importa el material. Abre **Workspace › Scripts › S-Log MetaRaw**, pulsa **1 · Leer metadatos** y luego **2 · Escribir en Resolve**.
2. En la página Color, añade **S-Log MetaRaw** como **primer nodo**. Toma el EI, los Kelvin y el tinte del clip: con esos valores no cambia nada.
3. Corrige el balance de blancos y la exposición en el nodo, con ayuda de las vistas de falso color.
4. Da forma a los tonos con **Toni**. Para recuperación local, Texture, Clarity o Dehaze, añade **S-Log MetaRaw Detail** como nodo siguiente.
5. Después, el resto del etalonaje y, al final, tu CST / LUT / DRT de salida.

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

![Ventana del script después de leer los metadatos](images/04-script-window.png)

### Botones

| Control | Qué hace |
|---|---|
| **Menú de clips** | *Todo el Media Pool* o *Clips seleccionados en el Media Pool*. |
| **1 · Leer metadatos** | Una fila por clip: cámara, objetivo, diafragma, obturador, EI, WB, espacio de color, data level. La columna **Estado** dice `leído`, qué cambió durante la toma (diafragma, foco…), o por qué se saltó un clip. Haz clic en una fila para ver todo lo que se leyó, agrupado como en Catalyst Browse. |
| **2 · Escribir en Resolve** | Rellena los campos del Media Pool (panel Metadata, columnas, palabras clave para smart bins, Camera Notes, data burn-in) y corrige valores que Resolve lee mal de los MXF, por ejemplo *Camera Aperture* `F53343` en la FX6. |
| **Exportar CSV** | Exporta los valores para los que Resolve no tiene campo (EI, tinte, modo WB, distancia de foco, gamma de captura…) en el formato CSV de metadatos de Resolve. Impórtalo con **File › Import › Metadata**, con *create custom fields* activado. |
| **Versión** (abajo a la derecha) | Haz clic para consultar GitHub; se pone verde cuando hay una release más nueva. |

![Detalle del clip, al estilo Catalyst](images/05-script-clip-detail.png)

### Opciones

| Opción | Por defecto | Efecto |
|---|---|---|
| **Etiqueta por cámara** | apagada | Añade cámara, gamma y primarios a las palabras clave. |
| **Sobrescribir metadatos** | encendida | Sustituye los valores que Resolve ya escribió. Apagada: solo rellena los campos vacíos. |
| **Corregir el Data Level** | encendida | Pone el *Data Level* de cada clip en **Full** (log) o **Video** (709, Cine, HLG). Es el arreglo de verdad: vale para todo el proyecto, scopes y exportaciones incluidos. |
| **Establecer también Input Color Space** | apagada | Para proyectos con gestión de color. ⚠️ Un script no puede devolverlo a *Project* — solo tú puedes, a mano. |

Después de **2 · Escribir en Resolve**, los datos aparecen en el Media Pool:

![Panel Metadata del Media Pool rellenado](images/06-media-pool-metadata.png)

**Velocidad.** No se decodifica nada: como máximo 24 muestras de la pista de metadatos por clip, dentro de un segundo. Un archivo de varios GB cuesta unos 100 KB de lectura. Un disco que deja de responder se salta una sola vez, con un aviso, en lugar de bloquear la lista. Ejecutar el script justo después de importar hace que los nodos encuentren los datos ya listos.

---

## 5. El nodo S-Log MetaRaw

**Color › OpenFX › S-Log MetaRaw** — primer nodo, antes de cualquier CST o LUT.

![Biblioteca OpenFX con los dos nodos](images/07-openfx-library.png)

El nodo es **puntual**: cada píxel depende solo de sí mismo. Nunca crea halos, y **Generate LUT** puede exportarlo (se recomiendan 65 puntos). Los ajustes se guardan por clip.

> Los paneles de los nodos están en italiano. Las etiquetas de abajo llevan traducción.

![Panel principal del nodo — parte superior](images/09-node-panel-top.png)

| Control | Qué hace |
|---|---|
| **Versión** (arriba) | Muestra `v2.3.1`. Una vez al día pregunta a GitHub por la última release; si la hay, muestra **🟢 v2.3.1 → 2.x.y** y el clic abre la descarga del instalador de tu sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Nunca instala nada por sí mismo. |
| **Camera** · **Rileggi metadata** (releer) | La cámara que se leyó. *Rileggi* vuelve a leer el clip, devuelve cada control a los valores de cámara y escribe los metadatos del clip en el Media Pool. Responde en unos 2 s como máximo. |
| **Decode Using** | *Clip* permite cambiar los controles; *Camera metadata* los bloquea en los valores de rodaje (el nodo queda transparente). |
| **White Balance** | As shot, o presets (Daylight, Cloudy, Shade, Tungsten, Fluorescent, Flash). Mover un deslizador lo cambia a *Custom*. |
| **Color Temp** · **Tint** | Adaptación cromática Bradford en luz lineal, desde el blanco que registró la cámara. Más Temp calienta la imagen; Tint positivo va hacia el magenta. |
| **Exposure** | En EI: el doble de EI equivale exactamente a +1 paso, en luz lineal, antes de cualquier curva. |
| **False color** | Vistas de temperatura, tinte y exposición — ver [§6](#6-falso-color). |
| **Color Space** · **Gamma** | La salida, como un Color Space Transform. *Timeline* no convierte: déjalo así en un proyecto con gestión de color. Para etalonar en DWG: *DaVinci WG · DaVinci Intermediate*. |
| **Toni** (Tonos) | Contrast, Highlights, Shadows, Whites, Bianco, Blacks, Vibrance, Saturation — ver [§7](#7-tonos-y-zonas). |
| **Zone** (Zonas, cerrado) | Zonas Black, Shadow, Light, Specular; Contrast Pivot; Soft Clip. |
| **Avanzate** (Avanzado) | Entrada del nodo, corrección del data level, estado, *Sblocca controlli senza metadata* (desbloquear sin metadatos). |
| **Dati di ripresa** (datos de rodaje) | Solo lectura: objetivo, focal, diafragma, foco, obturador, EI, WB, fps, ND, LUT de cámara. |

![Avanzate y Dati di ripresa](images/14-avanzate-dati.png)

**Avanzate en detalle**

- **Ingresso nodo** (entrada del nodo) — *Automatico* se lo pregunta a Resolve.
- **Data level in ingresso** — corrige la escala de code values cuando Resolve decodifica un clip en la equivocada. La opción *Corregir el Data Level* del script lo arregla para todo el proyecto.
- **Stato** (estado) — lo que detectó el nodo; también avisa cuando hay una vista de falso color activada.
- **Sblocca controlli senza metadata** — ver [§11](#11-casos-especiales).

> La **salida Rec.709** del nodo es un CST *sin* tone mapping (las altas luces por encima de Bianco se cortan) y excluye el nodo Detail. Mejor un CST de salida al final del árbol de nodos.

---

## 6. Falso color

Una vista por cada control, colocada encima del deslizador al que sirve. **La vista sustituye la imagen: apágala antes de renderizar.** (Un plugin OpenFX no puede dibujar una superposición sobre el visor de Resolve.)

Pon primero *Decode Using* en **Clip**: en *Camera metadata* los controles están bloqueados.

### Exposición

Bandas en pasos alrededor del gris del 18%, al estilo ARRI. Todo lo demás se vuelve gris.

![Bandas de exposición](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/falsecolor_bands.png)

| Color | Significado |
|---|---|
| Verde | Gris medio (18%) |
| Rosa | Un paso por encima — piel |
| Amarillo | Cerca del clip |
| Rojo | Clip (quemado) |
| Azul / violeta | Sombra profunda / negro |

Activa la vista, apunta al gris medio o a la piel, y mueve **Exposure** hasta que la zona correspondiente se vuelva verde (gris) o rosa (piel).

![Falso color de exposición en el visor](images/10-falsecolor-exposure.png)

### Temperatura y Tinte

Funcionan como en CineMatch. La imagen se vuelve gris; las dominantes se ven en color, y las casi neutras se amplifican hasta 8× para que se puedan ver.

| Vista | Ves | Haz |
|---|---|---|
| Temperature | Azul (dominante fría) | Sube Color Temp |
| Temperature | Naranja (dominante cálida) | Baja Color Temp |
| Tint | Verde | Sube Tint |
| Tint | Magenta | Baja Tint |

Elige una superficie que debería ser neutra y mueve el deslizador hasta que se quede gris. Alterna Temp y Tint un par de veces; converge. Cada vista responde solo a su propio deslizador.

![Falso color de temperatura](images/11-falsecolor-temp.png)

Para profundizar (en italiano): [docs/FALSE_COLOR.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/FALSE_COLOR.md)

---

## 7. Tonos y Zonas

### Toni (Tonos)

![Grupo Toni](images/12-toni.png)

Todos los deslizadores van de −100 a +100, en el orden de Camera Raw. Highlights es un hombro de película; los demás son **exposiciones sobre una franja de tonos, en pasos desde el gris del 18%**: dentro de la franja la imagen se mueve como con una exposición, así que la textura se conserva. Ninguna combinación de deslizadores puede solarizar.

| Deslizador | Comportamiento |
|---|---|
| **Contrast** | Gira alrededor del Pivot. +100 duplica la pendiente en el pivote, −100 la reduce a la mitad, con los extremos acotados. |
| **Highlights** | Negativo: hombro de película — a −100 el valor más alto registrado (~+6 pasos sobre el gris en S-Log3) llega exactamente a **Bianco**, sin velo gris y sin clip. El gris y lo que está debajo no se mueven; la piel a +1 paso se desplaza 0,05 pasos como mucho. El tono se mantiene constante. Positivo: más fuerza. |
| **Bianco** (blanco, en pasos) | Dónde llega ese máximo, y el techo de Soft Clip. **2,5** = el blanco de Rec.709 a través de un CST sin tone mapping. **Con un DRT después (ACES, AgX, DaVinci) súbelo a 4–5**, o las altas luces se comprimen dos veces. |
| **Shadows** | Tonos por debajo de −1 paso; 100 = 2 pasos. También levanta el negro — sujétalo con Blacks. |
| **Whites** | Desde +3,5 pasos hasta el clip; 100 = 1 paso. |
| **Blacks** | Velo lineal: mueve el negro (−3 / +1 paso) sin mover el gris. |
| **Vibrance** | Alrededor de la luminancia; protege los tonos de piel. |
| **Saturation** | Alrededor de la luminancia; igual en cualquier espacio de color. |

**Azzera toni** reinicia el grupo.

![Highlights 0 frente a −100](images/15-highlights-before-after.png)

![Curva de tonos](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/tone_curve.png)

> **El coste honesto de un nodo puntual:** lo que comprime, lo comprime también en la textura que hay dentro. La recuperación que conserva la textura vive en el nodo Detail.

### Zone (Zonas)

![Grupo Zone](images/13-zone.png)

- Cuatro zonas, como en la paleta HDR de Resolve: **Black, Shadow, Light, Specular** — cada una con **Exp** (pasos), **Sat**, **Range** (borde, en pasos) y **Falloff** (ancho de la transición).
- **Contrast Pivot** — el tono alrededor del cual gira Contrast.
- **Soft Clip** / **Soft Clip Color** — pliega las altas luces bajo un techo (en Bianco) que nunca alcanzan. *Color* decide si las altas luces plegadas van hacia el blanco (película) o conservan su color. Contención, no recuperación.
- **Falso color de Zone** — colorea cada píxel con la zona que lo mueve.
- **Azzera zone** reinicia el grupo.

Los bordes de las zonas se trasladan a través de Contrast, así que se mantienen en pasos de escena sea cual sea el Contrast que uses.

### Generate LUT

El nodo es puntual, así que Generate LUT lo incluye. Usa **65 puntos**: con los deslizadores a ±100 el error se queda dentro de unos 3,5 code values de S-Log3. A 33 puntos, la punta lineal de S-Log3 (Shadows +100) llega a unos 10 CV. Algunas Zone extremas (Black o Shadow a +3) superan eso incluso a 65 puntos — para esos etalonajes, deja el nodo activo en vivo.

Para profundizar (en italiano): [docs/TONE_MAPPING.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/TONE_MAPPING.md)

---

## 8. El nodo S-Log MetaRaw Detail

**Color › OpenFX › S-Log MetaRaw Detail** — justo después de S-Log MetaRaw, antes de cualquier CST / LUT / DRT.

Trabaja **por áreas**, sobre una base que respeta los bordes: mueve grandes áreas de luz sin aplanar el detalle fino — la parte de Highlights/Shadows de Lightroom que un nodo puntual no puede hacer.

![Panel del nodo Detail](images/16-detail-panel.png)

| Grupo | Controles |
|---|---|
| **Gamma dinamica** (rango dinámico) | Local Contrast, Local Highlights, Local Shadows; vistas de comprobación *gain* y *base*. |
| **Presenza** (presencia) | **Texture** (detalle fino; no sube el grano por debajo del umbral de ruido), **Clarity** (contraste local de escala media), **Dehaze**. |
| **Zone locali** (zonas locales) | Las zonas del nodo principal, aplicadas a áreas en lugar de a píxeles. |
| **Avanzate** (avanzado) | Preservación del detalle, radio, umbral de bordes (*Soglia bordi*), umbral de ruido, centro de Clarity, **Bianco** de Local Highlights, entrada del nodo. |
| **Velo** (neblina) | Nivel y color de la neblina que quita Dehaze. Los fijas tú; nunca se estiman fotograma a fotograma → sin parpadeo. |

**Azzera dettaglio** reinicia el nodo.

**Local Highlights frente a Highlights.** Local Highlights comprime las grandes áreas luminosas y conserva — incluso refuerza — la textura fina: el cielo se oscurece y las nubes mantienen su detalle. El Highlights del nodo principal suaviza la textura de las altas luces, como la película. Local Highlights tiene su propio **Bianco** en Avanzate y no conoce el EI elegido en el nodo principal: si cambias mucho el EI, ajústalo.

![Local Highlights sobre un cielo, antes/después](images/17-detail-before-after.png)

**Reglas**

- Decodifica a luz lineal lo que recibe y lo vuelve a escribir en la **misma codificación**. Solo acepta codificaciones scene-log: S-Log3, S-Log2, DaVinci WG/Intermediate, ACEScct. **No** Rec.709, Gamma 2.4 ni sRGB.
- Es **espacial**: Generate LUT lo deja fuera, junto con el resto de su nodo. Tenlo en un nodo propio.
- Los radios siguen la altura del fotograma: mismo aspecto a resolución completa, en proxy y en el visor. Sin estadísticas por fotograma → sin parpadeo.
- Metal: entre ~6 y 18 ms por fotograma UHD en Apple silicon. La alternativa por CPU es mucho más lenta.

**Límites medidos**

- Local Highlights a −100: halo en el lado oscuro de un borde por debajo del 3% del escalón.
- Local Shadows o las zonas locales a ±100: ~12% en un borde nítido de 1 paso, 4–6% en bordes de 2–3 pasos. Si lo ves, baja *Soglia bordi*.
- Texture cerca de bordes fuertes puede aumentar el grano 1,25–1,7×.
- Dehaze necesita una neblina real que quitar.

Para profundizar (en italiano): [docs/DETAIL.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DETAIL.md)

---

## 9. Flujos de trabajo: dónde va cada nodo

### A — Etalonar en DaVinci Wide Gamut (recomendado)

```
S-Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → Detail             Ingresso nodo: DaVinci WG/Intermediate
  → tu etalonaje (DWG)
  → CST de salida       DaVinci WG/Intermediate → Rec.709 · Gamma 2.4, tone mapping de DaVinci
```

Con un tone mapping después, pon **Bianco en 4–5 en ambos nodos**. En una timeline sin gestión de color debes declarar la entrada del nodo Detail.

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
| Paisaje plano y con neblina | Detail › Presenza + Velo | Dehaze; fija el nivel/color de la neblina a mano |

---

## 11. Casos especiales

**ProRes de un grabador externo / clip ilegible.** El nodo se queda neutro. Marca **Avanzate › Sblocca controlli senza metadata** y escribe el EI, los Kelvin y el tinte de rodaje: los controles se activan y el nodo empieza neutro.

**Perfiles que no son log** (Cine, HLG, S-Cinetone): los nodos se quedan neutros y lo dicen en *Stato*.

**Data Level equivocado.** Si los negros se ven levantados o aplastados directamente de cámara, Resolve está decodificando en la escala de code values equivocada. Ejecuta el script con *Corregir el Data Level* activado (arreglo para todo el proyecto), o usa *Avanzate › Data level in ingresso* en un solo clip. Para profundizar (en italiano): [docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**Cámaras sin Kelvin** (por ejemplo la a6300): los Kelvin se estiman a partir del preset de luz y se señalan.

**Rendimiento.** Abrir un proyecto no lee ningún archivo. Abrir el panel espera como mucho medio segundo; un disco lento termina en segundo plano, con un límite de 15 s.

---

## 12. Actualizaciones y privacidad

- Los **nodos** preguntan a GitHub por la última release como mucho una vez al día, en segundo plano. La petición lleva solo la versión (`User-Agent: SLogMetaRaw/2.3.1`).
- El **script** solo consulta cuando haces clic en su versión.
- Un clic solo abre un enlace de descarga de las releases de GitHub de este proyecto. Nada se instala sin ti.

![Aviso de actualización](images/18-update-badge.png)

**Para desactivar la comprobación:** crea el archivo vacío

```bash
touch ~/Library/Application\ Support/SLogMetaRaw/no_update_check
```

**Actualizar desde 1.x:** los tonos de la 1.x (Highlights, Shadows, Contrast, Saturation, Color Boost, Color Recovery) no se pueden convertir y se reinician a cero. Para conservar el look de un clip ya terminado, **antes de actualizar** ejecuta Generate LUT en ese nodo. Después de la actualización, *Stato* muestra lo que había (por ejemplo "Toni 1.1 azzerati (H −40, C +15)"). Exposure, data level, Color Space y Gamma no cambian.

---

## 13. Desinstalar

Haz doble clic en **Disinstalla S-Log MetaRaw.command**, en el DMG (la primera vez: clic derecho › Abrir). Hace esto:

1. enumera todo lo que va a eliminar (todas las versiones 1.x y 2.x, el nombre antiguo *SonyMeta*, las instalaciones de desarrollo, la caché, los ajustes, los registros);
2. te pide que cierres Resolve;
3. te pregunta si quieres borrar también los CSV exportados;
4. comprueba que no queda nada.

Ejecútalo con `--dry-run` para ver qué eliminaría sin tocar nada. Los metadatos ya escritos en los proyectos de Resolve se quedan: forman parte de los proyectos.

![Desinstalador en Terminal](images/20-uninstaller.png)

---

## 14. Solución de problemas / FAQ

**Falta el nodo Detail / el panel se ve viejo.**
Resolve está usando su caché de plugins antigua. Cierra Resolve, borra `OFXPluginCacheV2.xml` (el instalador normalmente lo hace), reinicia.

**El nodo no hace nada.**
Es lo esperado con los valores de rodaje. Comprueba que *Decode Using* está en **Clip**, y lee *Avanzate › Stato*: dice si el perfil no es log o si faltan metadatos.

**Mi render tiene falso color.**
Se quedó una vista encendida. Apaga todas las vistas de falso color antes de renderizar.

**Las altas luces se ven comprimidas dos veces / apagadas.**
Tienes un DRT (ACES, AgX, DaVinci tone mapping) después del nodo: sube **Bianco** a 4–5 (también en el nodo Detail).

**Halos alrededor de los bordes con el nodo Detail.**
Baja *Avanzate › Soglia bordi*, o reduce Local Shadows / las zonas locales.

**El script se saltó un clip.**
Lee la columna *Estado*: dice por qué (archivo no soportado, disco que no responde, etc.).

**«Camera Aperture F53343» en la FX6.**
Una lectura equivocada de Resolve en el MXF; **2 · Escribir en Resolve** lo corrige.

**macOS no abre el instalador.**
No está firmado: clic derecho › Abrir (o Configuración del Sistema › Privacidad y seguridad › Abrir de todas formas).

---

## 15. Linux y Windows (experimental)

El plugin también compila para Linux y Windows con CMake. Estos ports renderizan solo por **CPU** (todavía sin
Metal, CUDA ni OpenCL) y no se han probado dentro de DaVinci Resolve: la build compatible sigue siendo la de macOS.

**Linux** (descarga desde [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) el archivo adecuado para tu distribución):

| Archivo | Para |
|---|---|
| `slogmetaraw_<version>_amd64.deb` | Ubuntu, Debian, Mint: `sudo apt install ./slogmetaraw_*.deb` |
| `slogmetaraw-<version>-1.x86_64.rpm` | Rocky, Alma, RHEL, CentOS, Fedora: `sudo dnf install ./slogmetaraw-*.rpm` |
| `SLogMetaRaw-<version>-linux-x86_64.run` | cualquier distribución: `sh SLogMetaRaw-*.run` (`--user` sin sudo, `--uninstall`) |
| `SLogMetaRaw-<version>-linux-x86_64.tar.gz` | lo mismo, sin empaquetar: ejecuta `install.sh` dentro |

El plugin va a `/usr/OFX/Plugins`; la librería Python a `/usr/lib/slogmetaraw` (.deb/.rpm) o
`~/.local/share/SLogMetaRaw` (.run, .tar.gz). Reinicia Resolve. Basta con Python 3.6+. El binario se compila contra
glibc 2.28, así que se carga en Rocky/Alma/RHEL 8+, Ubuntu 20.04+ y Debian 10+.

**Windows:** compílalo tú mismo con `powershell -File packaging\windows\build_package.ps1` (cmake, Visual Studio
Build Tools, python; Inno Setup es opcional para el `.exe`). El estado vive en `%APPDATA%\SLogMetaRaw`.

Empujar una etiqueta `v*` al repositorio hace que la CI adjunte automáticamente los paquetes de Linux (y, best-effort,
el de Windows) a una **release** de GitHub en borrador — ver [Límites conocidos](#16-límites-conocidos) para lo que
todavía falta (aceleración por GPU, un pipeline de imagen probado y equivalente al de macOS).

---

## 16. Límites conocidos

- El panel Camera Raw de Resolve y la estabilización por giroscopio no se pueden desbloquear para MP4: viven dentro de los decodificadores de Resolve. S-Log MetaRaw reconstruye los controles de color; no abre una puerta dentro de Resolve.
- S-Log2 sigue el documento de Sony; la curva S-Log2 de Resolve difiere en unos 0,15 pasos.
- Pendiente de comprobar con más archivos: XAVC HS (HEVC), HLG, S-Cinetone, zooms motorizados.
- Probado solo en Resolve Studio 21.1 en macOS. Las builds de Linux y Windows son solo por CPU y no se han probado dentro de Resolve (ver [§15](#15-linux-y-windows-experimental)).
- Proyecto independiente y aficionado, distribuido tal cual, sin garantía y sin responsabilidad por el uso profesional. No afiliado ni respaldado por Sony ni por Blackmagic Design.

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0 o posterior](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [Notas de la versión](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)

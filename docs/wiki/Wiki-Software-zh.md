# S-Log MetaRaw — 用户指南

[English](Wiki-Software) · [Italiano](Wiki-Software-it) · [Español](Wiki-Software-es) · [Português](Wiki-Software-pt) · **简体中文**

> 版本 **2.3.0** · macOS 12+ · DaVinci Resolve 20 / 21 · 索尼 XAVC `.MP4` / `.MXF`
> 仅在 macOS 上的 DaVinci Resolve Studio 21.1 中测试过。

S-Log MetaRaw 读取索尼摄影机写入每个文件的拍摄数据（开尔文、色调、EI、镜头、光圈、快门、色彩配置），并在 DaVinci Resolve 中加以使用。Resolve 只对 FX6/FX9 的 MXF 文件这样做；对于 FX30、FX3、a7 和 a6000 系列的 MP4 文件，它会忽略这些数据。

**目录**

1. [这是什么，也不是什么](#1-这是什么也不是什么)
2. [安装](#2-安装)
3. [快速上手](#3-快速上手)
4. [脚本](#4-脚本)
5. [S-Log MetaRaw 节点](#5-s-log-metaraw-节点)
6. [伪色](#6-伪色)
7. [影调与分区](#7-影调与分区)
8. [S-Log MetaRaw Detail 节点](#8-s-log-metaraw-detail-节点)
9. [工作流程：各节点的位置](#9-工作流程各节点的位置)
10. [配方](#10-配方)
11. [特殊情况](#11-特殊情况)
12. [更新与隐私](#12-更新与隐私)
13. [卸载](#13-卸载)
14. [疑难排解 / 常见问题](#14-疑难排解--常见问题)
15. [已知限制](#15-已知限制)

---

## 1. 这是什么，也不是什么

三个工具，一次安装：

| 工具 | 在 Resolve 中的位置 | 作用 |
|---|---|---|
| **脚本** | Workspace › Scripts › S-Log MetaRaw | 读取每个片段的元数据并写入媒体池，并修正每个片段的 Data Level。 |
| **S-Log MetaRaw** 节点 | Color › OpenFX — **第一个节点** | 以拍摄时的数值为起点显影片段：白平衡、曝光、色彩空间、分区影调、伪色。 |
| **S-Log MetaRaw Detail** 节点 | Color › OpenFX — **紧随其后** | 创意节点：局部高光/阴影恢复、Texture、Clarity、Dehaze。 |

原始文件**绝不会被修改**：不转码，不重新封装。

**它不是 RAW。** Log MP4 已经过去马赛克和压缩（8 或 10 位，通常为 4:2:0，并带有机内降噪）。节点严谨地应用色彩科学——曝光和白平衡在线性光中进行，以记录的数值为起点，遵循索尼公开的曲线和色域——因此画面的*表现*会让人联想到 RAW：白平衡移动干净，曝光像一档光那样变化，高光平滑过渡而不是断裂。一旦超出摄影机的极限，缺失的信息就会显现：天空出现色带，提亮的阴影出现噪点，过曝的高光依然过曝。请在拍摄现场正确曝光。

---

## 2. 安装

1. 从 [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) 下载 `SLogMetaRaw-2.3.0.dmg` 并打开。

   ![DMG 窗口](images/01-dmg.png)

2. 双击 **Installa S-Log MetaRaw.pkg**。安装包未使用 Apple 证书签名：第一次请**右键点击 › 打开**。因为插件要安装到系统文件夹，系统会要求输入 Mac 密码。

   ![在未签名的安装包上右键点击 › 打开](images/02-gatekeeper.png)

3. **重启 DaVinci Resolve。**

安装程序还会删除 Resolve 的插件缓存（`OFXPluginCacheV2.xml`）；Resolve 会在下次启动时重建它。否则 Resolve 会继续显示旧面板，也看不到 Detail 节点。

每次安装都从干净状态开始：之前的插件和库会被完整替换，开发版安装也会被移除。

**安装内容及位置**

| 项目 | 路径 |
|---|---|
| 两个节点（同一个 bundle） | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Python 库 | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| 菜单脚本 | `…/DaVinci Resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py` |
| 每个片段的缓存（JSON） | `~/Library/Application Support/SLogMetaRaw/cache` |

**系统要求：** macOS 12 或更高版本，Apple 芯片或 Intel，DaVinci Resolve 21 或 20（Resolve 20 不自带 Python：请从 python.org 安装 Python 3）。
**片段：** 索尼 XAVC `.MP4` 或 `.MXF`——脚本可以全部读取。节点可显影 **S-Log3**（S-Gamut3.Cine 或 S-Gamut3）、**S-Log2** 和 **S-Log**（S-Gamut）。遇到其他配置（Cine、HLG、S-Cinetone）时，节点保持中性并给出提示。

---

## 3. 快速上手

1. 导入素材。打开 **Workspace › Scripts › S-Log MetaRaw**，先点击 **1 · 读取元数据**，再点击 **2 · 写入 Resolve**。
2. 在 Color 页面把 **S-Log MetaRaw** 添加为**第一个节点**。它会取得片段的 EI、开尔文值和色调：在这些数值下，画面不会有任何变化。
3. 借助伪色视图，在节点中校正白平衡和曝光。
4. 用 **Toni** 塑造影调。需要局部恢复、Texture、Clarity 或 Dehaze 时，把 **S-Log MetaRaw Detail** 添加为下一个节点。
5. 然后是调色的其余部分，最后是你的输出 CST / LUT / DRT。

```
S-Log MetaRaw  →  S-Log MetaRaw Detail  →  调色的其余部分  →  输出 CST / LUT / DRT
```

![推荐的节点树](images/08-node-tree.png)

> 把节点复制到另一个片段上，它会用那个片段的数据自我重置。

---

## 4. 脚本

**Workspace › Scripts › S-Log MetaRaw**

![Workspace › Scripts 菜单](images/03-scripts-menu.png)

窗口有一行按钮、一行选项和片段列表。界面语言跟随 Resolve（English、Italiano、Español、Português、简体中文）。脚本只读取文件，绝不修改它们。

![点击读取元数据后的脚本窗口](images/04-script-window.png)

### 按钮

| 控件 | 作用 |
|---|---|
| **片段菜单** | *整个媒体池* 或 *媒体池中选中的片段*。 |
| **1 · 读取元数据** | 每个片段一行：摄影机、镜头、光圈、快门、EI、白平衡、色彩空间、数据电平。**状态**列显示 `已读取`、拍摄过程中变化的参数（光圈、对焦……），或片段被跳过的原因。点击某一行即可查看读取到的全部内容，分组方式与 Catalyst Browse 相同。 |
| **2 · 写入 Resolve** | 填写媒体池字段（元数据面板、列、用于智能素材箱的关键词、Camera Notes、数据叠印），并修正 Resolve 从 MXF 中读错的值，例如 FX6 上的 *Camera Aperture* `F53343`。 |
| **导出 CSV** | 以 Resolve 自己的元数据 CSV 格式导出 Resolve 没有对应字段的数值（EI、色调、白平衡模式、对焦距离、拍摄伽马……）。用 **File › Import › Metadata** 导入，并勾选 *create custom fields*。 |
| **版本**（右下角） | 点击以检查 GitHub；有新版本时会变绿。 |

![片段详情，Catalyst 风格](images/05-script-clip-detail.png)

### 选项

| 选项 | 默认值 | 效果 |
|---|---|---|
| **相机标签** | 关 | 将摄影机、伽马和原色加入关键词。 |
| **覆盖元数据** | 开 | 替换 Resolve 已经写入的值。关闭时只填写空字段。 |
| **修正数据电平** | 开 | 将每个片段的 *Data Level* 设为 **Full**（log）或 **Video**（709、Cine、HLG）。这才是真正的修复：对整个项目、示波器和导出均生效。 |
| **同时设置 Input Color Space** | 关 | 用于色彩管理项目。⚠️ 脚本无法把它改回 *Project*——只能由你手动改回。 |

点击 **2 · 写入 Resolve** 后，数据就会出现在媒体池中：

![填写完毕的媒体池元数据面板](images/06-media-pool-metadata.png)

**速度。** 不解码任何内容：每个片段最多读取元数据轨道的 24 个样本，耗时不超过一秒。一个数 GB 的文件大约只需读取 100 KB。停止响应的磁盘只会被跳过一次并给出提示，而不会卡住整个列表。导入后立即运行脚本，可以让节点一开始就有数据可用。

---

## 5. S-Log MetaRaw 节点

**Color › OpenFX › S-Log MetaRaw** — 第一个节点，位于任何 CST 或 LUT 之前。

![同时包含两个节点的 OpenFX 库](images/07-openfx-library.png)

该节点是**逐点处理**的：每个像素只取决于它自身。它不会产生光晕，**Generate LUT** 可以将其导出（建议 65 点）。设置按片段保存。

> 节点面板为意大利语。下文的控件名称后附有中文说明。

![主节点面板——上半部分](images/09-node-panel-top.png)

| 控件 | 作用 |
|---|---|
| **版本**（顶部） | 显示 `v2.3.0`。每天最多向 GitHub 查询一次最新版本；若有新版本，会显示 **🟢 v2.3.0 → 2.x.y**，点击即可打开适合你系统的安装程序下载（`.dmg`、`.exe`、`.deb`/`.rpm`/`.run`）。它自己从不安装任何东西。 |
| **Camera** · **Rileggi metadata**（重新读取） | 读取到的摄影机。*Rileggi* 会重新读取片段，把所有控件恢复为摄影机数值，并把该片段的元数据写入媒体池。约 2 秒内响应。 |
| **Decode Using** | *Clip* 可以修改控件；*Camera metadata* 把控件锁定为拍摄值（节点透明）。 |
| **White Balance** | As shot 或预设（Daylight、Cloudy、Shade、Tungsten、Fluorescent、Flash）。移动任一滑块会自动切换为 *Custom*。 |
| **Color Temp** · **Tint** | 从摄影机记录的白点出发，在线性光中进行 Bradford 色适应。Temp 越高画面越暖；Tint 为正则偏向品红。 |
| **Exposure** | 以 EI 为单位：EI 翻倍，在任何曲线之前的线性光中正好等于 +1 档。 |
| **False color** | 色温、色调和曝光视图——见 [§6](#6-伪色)。 |
| **Color Space** · **Gamma** | 输出，类似 Color Space Transform。*Timeline* 不做转换——在色彩管理项目中请保持此项不变。要在 DWG 中调色：选择 *DaVinci WG · DaVinci Intermediate*。 |
| **Toni**（影调） | Contrast、Highlights、Shadows、Whites、**Bianco**、Blacks、Vibrance、Saturation——见 [§7](#7-影调与分区)。 |
| **Zone**（分区，默认收起） | Black、Shadow、Light、Specular 分区；Contrast Pivot；Soft Clip。 |
| **Avanzate**（高级） | 节点输入、数据电平校正、状态、**Sblocca controlli senza metadata**（无元数据时解锁控件）。 |
| **Dati di ripresa**（拍摄数据） | 只读：镜头、焦距、光圈、对焦、快门、EI、白平衡、帧率、ND、机内 LUT。 |

![Avanzate 和 Dati di ripresa](images/14-avanzate-dati.png)

**Avanzate 详解**

- **Ingresso nodo**（节点输入）——*Automatico* 会询问 Resolve。
- **Data level in ingresso**（输入数据电平）——当 Resolve 用错误的码值范围解码片段时，用它修正。脚本的 *Correct the Data Level*（修正数据电平）可以为整个项目修正。
- **Stato**（状态）——节点检测到的情况；开启伪色视图时也会在这里提示。
- **Sblocca controlli senza metadata** —— 见 [§11](#11-特殊情况)。

> 节点中的 **Rec.709 输出**是一个*不带*色调映射的 CST（高光超过 Bianco 会被剪切），并且会屏蔽 Detail 节点。建议把输出 CST 放在节点树的最后。

---

## 6. 伪色

每个控件对应一个视图，位于它所服务的滑块上方。**该视图会替换画面——渲染前请关闭它。**（OpenFX 插件无法在 Resolve 的检视器上叠加绘制。）

请先把 *Decode Using* 设为 **Clip**：在 *Camera metadata* 下控件是锁定的。

### Exposure（曝光）

以 18% 灰为中心、按档分带，ARRI 风格。其余部分全部变灰。

![曝光分带](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/falsecolor_bands.png)

| 颜色 | 含义 |
|---|---|
| 绿色 | 中灰（18%） |
| 粉色 | 高一档——肤色 |
| 黄色 | 接近过曝 |
| 红色 | 已过曝 |
| 蓝色 / 紫色 | 深暗部 / 黑位 |

打开视图，对准中灰或肤色，移动 **Exposure**，直到相应区域变为绿色（中灰）或粉色（肤色）。

![检视器中的曝光伪色](images/10-falsecolor-exposure.png)

### Temperature 与 Tint（色温与色调）

工作方式与 CineMatch 相同。画面变为灰色，偏色以彩色显示，接近中性的偏色会被放大最多 8 倍以便看清。

| 视图 | 你看到的 | 该做的 |
|---|---|---|
| Temperature | 蓝色（偏冷） | 提高 Color Temp |
| Temperature | 橙色（偏暖） | 降低 Color Temp |
| Tint | 绿色 | 提高 Tint |
| Tint | 品红 | 降低 Tint |

选一个本应是中性色的表面，移动滑块直到它保持灰色。在 Temp 和 Tint 之间来回调整几次即可收敛。每个视图只响应自己对应的滑块。

![色温伪色](images/11-falsecolor-temp.png)

深入说明（意大利语）：[docs/FALSE_COLOR.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/FALSE_COLOR.md)

---

## 7. 影调与分区

### Toni（影调）

![Toni 分组](images/12-toni.png)

所有滑块的范围均为 −100…+100，顺序与 Camera Raw 相同。Highlights 是一个胶片式肩部；其余各项是**作用于一段影调范围、以距 18% 灰的档数衡量的曝光调整**：在该范围内，画面像曝光一样整体移动，因此纹理得以保留。任何滑块组合都不会产生色调分离（solarise）。

| 滑块 | 行为 |
|---|---|
| **Contrast** | 围绕 Pivot 旋转。+100 会使枢轴处的斜率加倍，−100 则减半，两端均有限制。 |
| **Highlights** | 负值：胶片式肩部——在 −100 时，记录到的最亮值（S-Log3 中约为中灰以上 +6 档）正好落在 **Bianco** 上，没有灰雾，也没有剪切。中灰及以下保持不动，+1 档的肤色最多移动 0.05 档。色相保持不变。正值：更有力度。 |
| **Bianco**（白点，单位为档） | 该最大值的落点，也是 Soft Clip 的上限。**2.5** = 经过不带色调映射的 CST 后的 Rec.709 白。**如果之后有 DRT（ACES、AgX、DaVinci），请调到 4–5**，否则高光会被压缩两次。 |
| **Shadows** | 作用于 −1 档以下的影调；100 = 2 档。也会提升黑位——需配合 Blacks 使用以压住它。 |
| **Whites** | 从 +3.5 档到剪切点；100 = 1 档。 |
| **Blacks** | 线性薄雾：移动黑位（−3 / +1 档）而不移动中灰。 |
| **Vibrance** | 围绕明度调整；保护肤色。 |
| **Saturation** | 围绕明度调整；在任何色彩空间中效果一致。 |

**Azzera toni** 会重置该组参数。

![Highlights 为 0 与 −100 的对比](images/15-highlights-before-after.png)

![影调曲线](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/tone_curve.png)

> **逐点节点的坦诚代价：** 它压缩的部分，纹理也会一起被压缩。能够保留纹理的恢复操作，在 Detail 节点中进行。

### Zone（分区）

![Zone 分组](images/13-zone.png)

- 四个分区，与 Resolve 的 HDR 调色板相同：**Black、Shadow、Light、Specular**——各自拥有 **Exp**（档）、**Sat**、**Range**（边界，单位为档）和 **Falloff**（过渡宽度）。
- **Contrast Pivot**——Contrast 旋转所围绕的影调位置。
- **Soft Clip** / **Soft Clip Color**——把高光折叠到一个它们永远无法到达的上限（Bianco）之下。*Color* 决定被折叠的高光是趋向白色（胶片式）还是保留原本的颜色。这是约束，而非恢复。
- **Zone false colour**（分区伪色）——用对应分区的颜色标出移动了每个像素的分区。
- **Azzera zone** 会重置该组参数。

分区边界会随 Contrast 一起变化，因此无论使用怎样的 Contrast 设置，它们始终保持在相同的场景档位上。

### Generate LUT

该节点是逐点处理的，因此 Generate LUT 会包含它。建议使用 **65 点**：在 ±100 的滑块范围内，误差保持在约 3.5 个 S-Log3 码值以内。在 33 点时，S-Log3 的线性趾部（Shadows +100）误差达到约 10 CV。某些极端的 Zone 设置（Black 或 Shadow +3）即使在 65 点时也会超过这个数值——这类调色请保留该节点为实时状态。

深入说明（意大利语）：[docs/TONE_MAPPING.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/TONE_MAPPING.md)

---

## 8. S-Log MetaRaw Detail 节点

**Color › OpenFX › S-Log MetaRaw Detail** — 紧跟在 S-Log MetaRaw 之后，位于任何 CST / LUT / DRT 之前。

它在一个保留边缘的基础层上**按区域**工作：移动大面积的明暗区域，而不压平精细的细节——这正是逐点节点无法做到的、类似 Lightroom 中 Highlights/Shadows 的部分。

![Detail 节点面板](images/16-detail-panel.png)

| 分组 | 控件 |
|---|---|
| **Gamma dinamica**（动态范围） | Local Contrast、Local Highlights、Local Shadows；*gain* 和 *base* 检视视图。 |
| **Presenza**（质感） | **Texture**（精细细节；不会提升噪声阈值以下的颗粒）、**Clarity**（中等尺度的局部对比度）、**Dehaze**。 |
| **Zone locali**（局部分区） | 主节点的分区，作用于区域而非像素。 |
| **Avanzate**（高级） | 细节保留、半径、边缘阈值（*Soglia bordi*）、噪声阈值、Clarity 中心、Local Highlights 专用的 **Bianco**、节点输入。 |
| **Velo**（雾霭） | Dehaze 所去除雾霭的亮度和颜色。由你手动设定；从不逐帧估计 → 不会闪烁。 |

**Azzera dettaglio** 会重置该节点。

**Local Highlights 与 Highlights 的区别。** Local Highlights 压缩大面积的明亮区域，同时保留——甚至强化——精细纹理：天空变暗，云层保留细节。主节点的 Highlights 则像胶片一样柔化高光处的纹理。Local Highlights 在 Avanzate 中拥有自己的 **Bianco**，并不知道主节点中选择的 EI：如果你大幅调整 EI，请相应调整它。

![天空上的 Local Highlights，前后对比](images/17-detail-before-after.png)

**规则**

- 它会把接收到的画面解码为线性光，再按**相同编码**写回。它只接受场景对数编码：S-Log3、S-Log2、DaVinci WG/Intermediate、ACEScct。**不**接受 Rec.709、Gamma 2.4 或 sRGB。
- 它是**空间**处理的：Generate LUT 会把它及其所在的整个节点排除在外。请把它放在单独的节点中。
- 半径随画面高度缩放：全分辨率、代理和检视器中效果一致。没有逐帧统计 → 不会闪烁。
- Metal：在 Apple 芯片上处理一帧 UHD 约需 6–18 毫秒。CPU 备用路径要慢得多。

**实测局限**

- Local Highlights 为 −100 时：边缘暗侧的光晕低于台阶幅度的 3%。
- Local Shadows 或局部分区为 ±100 时：在一档的硬边缘处约为台阶幅度的 12%，在 2–3 档的边缘处为 4–6%。如果看到光晕，请降低 *Soglia bordi*。
- Texture 在强边缘附近可能会使颗粒增大 1.25–1.7 倍。
- Dehaze 需要画面中真的存在雾霭才能去除。

深入说明（意大利语）：[docs/DETAIL.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DETAIL.md)

---

## 9. 工作流程：各节点的位置

### A — 在 DaVinci Wide Gamut 中调色（推荐）

```
S-Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → Detail             Ingresso nodo: DaVinci WG/Intermediate
  → 你的调色（DWG）
  → 输出 CST          DaVinci WG/Intermediate → Rec.709 · Gamma 2.4，DaVinci 色调映射
```

之后如有色调映射，请在**两个节点中都把 Bianco 设为 4–5**。在未做色彩管理的时间线上，你必须自行声明 Detail 节点的输入。

### B — 全程在 Log 中

```
S-Log MetaRaw          Color Space/Gamma: Timeline
  → Detail             Ingresso nodo: Automatico（使用片段自带的 S-Log3）
  → 你的调色
  → CST / LUT          S-Gamut3.Cine/S-Log3 → 你的输出
```

### C — 色彩管理项目（DaVinci YRGB Color Managed / ACES）

在主节点中把 Color Space/Gamma 保持为 **Timeline**。只有在你希望脚本设置每个片段的输入时，才启用脚本的 *Also set Input Color Space*——别忘了之后要手动把它改回 *Project*。

---

## 10. 配方

| 问题 | 位置 | 设置 |
|---|---|---|
| 天空或窗户过曝 | Main › Toni | Highlights −50…−100（之后有 DRT 时：Bianco 4–5） |
| ……并保留云层纹理 | Detail › Gamma dinamica | Local Highlights |
| 逆光人脸处于阴影中 | Main › Toni | Shadows +30…+60，Blacks −20…−40 |
| 更"胶片"的观感 | Main › Toni | Contrast +20…+30，Highlights −60 |
| 噪点多的高饱和阴影 | Main › Zone | Shadow Sat −30…−50 |
| 镜面反光 | Main › Zone | Specular Exp −1…−2 |
| 平淡、雾霭弥漫的风景 | Detail › Presenza + Velo | Dehaze；手动设置雾霭的亮度/颜色 |

---

## 11. 特殊情况

**来自外部录机的 ProRes / 无法读取的片段。** 节点保持中性。勾选 **Avanzate › Sblocca controlli senza metadata**，并手动输入拍摄时的 EI、开尔文值和色调：控件随即启用，节点从中性状态开始。

**非对数（non-log）配置文件**（Cine、HLG、S-Cinetone）：节点保持中性，并在 *Stato* 中说明原因。

**数据电平错误。** 如果直接从摄影机出来的画面看起来黑位被提升或压死，说明 Resolve 用错误的码值范围在解码。运行脚本并开启 *Correct the Data Level*（项目级修复），或对单个片段使用 *Avanzate › Data level in ingresso*。背景说明（意大利语）：[docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**没有开尔文值的摄影机**（例如 a6300）：开尔文值会根据光源预设估算，并会给出标注。

**性能。** 打开项目不会读取任何文件。打开面板最多等待半秒；慢速磁盘会在后台完成读取，上限 15 秒。

---

## 12. 更新与隐私

- **节点**每天最多在后台向 GitHub 查询一次最新版本。请求中只携带版本号（`User-Agent: SLogMetaRaw/2.3.0`）。
- **脚本**只在你点击其版本号时才会检查。
- 点击只会打开本项目 GitHub 发布页中的下载链接，未经你的操作不会安装任何东西。

![更新提示标志](images/18-update-badge.png)

**关闭检查：** 创建以下空文件

```bash
touch ~/Library/Application\ Support/SLogMetaRaw/no_update_check
```

**从 1.x 更新。** 1.x 的影调参数（Highlights、Shadows、Contrast、Saturation、Color Boost、Color Recovery）无法转换，会被重置为零。若要保留某个已完成片段的效果，**请在更新前**对该节点运行 Generate LUT。更新后，*Stato* 会显示原来的设置（例如 "Toni 1.1 azzerati (H −40, C +15)"）。Exposure、数据电平、Color Space 和 Gamma 不受影响。

---

## 13. 卸载

在 DMG 中双击 **Disinstalla S-Log MetaRaw.command**（第一次请右键 › 打开）。它会：

1. 列出将要移除的全部内容（所有 1.x 和 2.x 版本、旧名称 *SonyMeta*、开发版安装、缓存、设置、日志）；
2. 要求你先退出 Resolve；
3. 询问是否同时删除导出的 CSV；
4. 检查是否已清理干净。

加上 `--dry-run` 运行可以只查看会移除哪些内容，而不实际改动任何东西。已经写入 Resolve 项目的元数据会保留——它属于项目的一部分。

![终端中的卸载程序](images/20-uninstaller.png)

---

## 14. 疑难排解 / 常见问题

**Detail 节点不见了 / 面板看起来是旧版本。**
Resolve 使用的是旧的插件缓存。退出 Resolve，删除 `OFXPluginCacheV2.xml`（安装程序通常会自动执行此操作），然后重启。

**节点没有任何效果。**
在拍摄时的数值下，这是设计使然。请检查 *Decode Using* 是否为 **Clip**，并查看 *Avanzate › Stato*：它会说明配置文件是否非对数，或元数据是否缺失。

**渲染结果里出现了伪色。**
某个视图被遗漏未关闭。渲染前请关闭所有伪色视图。

**高光看起来被压缩了两次 / 发闷。**
节点之后接了 DRT（ACES、AgX、DaVinci 色调映射）：把 **Bianco** 调到 4–5（Detail 节点中也要调整）。

**使用 Detail 节点时边缘出现光晕。**
降低 *Avanzate › Soglia bordi*，或减小 Local Shadows / 局部分区的数值。

**脚本跳过了某个片段。**
查看 *Status* 列：会说明原因（不支持的文件、磁盘无响应等）。

**FX6 上出现 "Camera Aperture F53343"。**
这是 Resolve 对 MXF 的误读；**2 · 写入 Resolve** 可以修正它。

**macOS 无法打开安装程序。**
它未经签名：右键点击 › 打开（或 系统设置 › 隐私与安全性 › 仍要打开）。

---

## 15. 已知限制

- Resolve 自带的 Camera Raw 面板和陀螺仪防抖无法为 MP4 解锁：它们位于 Resolve 的解码器内部。S-Log MetaRaw 重建的是色彩控件，而不是打开通往 Resolve 内部的某扇门。
- S-Log2 遵循索尼文档实现；Resolve 自带的 S-Log2 曲线与之相差约 0.15 档。
- 仍需在更多文件上验证：XAVC HS（HEVC）、HLG、S-Cinetone、电动变焦。
- 仅在 macOS 上的 Resolve Studio 21.1 中测试过。
- 独立的业余项目，按现状提供，不作任何担保，也不对专业用途承担责任。与 Sony 或 Blackmagic Design 无隶属关系，也未获其认可。

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0+](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [更新日志](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)

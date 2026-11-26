<p align="center"><img src="assets/icon_1024.png" width="140" alt="Log MetaRaw"></p>

<h1 align="center">Log MetaRaw</h1>
<p align="center"><b>为 DaVinci Resolve 20 和 21 提供索尼、松下、苹果、大疆等摄影机的 Log 元数据与场景线性显影控制</b><br>
Ivan Mazzone + Claude · <a href="https://github.com/ivan-94m">github.com/ivan-94m</a> · <a href="https://instagram.com/ivan_94m">@ivan_94m</a></p>

<p align="center">
<a href="README.md">English</a> · <a href="README.it.md">Italiano</a> · <a href="README.es.md">Español</a> · <a href="README.pt.md">Português</a> · <b>简体中文</b>
</p>

---

## 这是什么

索尼摄影机会把拍摄设置写进每个文件：以开尔文表示的白平衡、色调（Tint）、EI、镜头、光圈、快门、色彩配置。Resolve
会读取 FX6、FX9 的 MXF 文件中的这些数据，并为其提供 *Camera Raw* 面板；但对于 FX30、FX3、a7 或 a6000 系列的 MP4
文件，它会忽略这些数据。

Log MetaRaw 读取并使用这些数据，由一个脚本和五个节点组成，图标上的编号即串联顺序：

| | 位置 | 作用 |
|---|---|---|
| **脚本** | Workspace › Scripts › Log MetaRaw | 读取项目中所有片段的元数据并写入媒体池 |
| **1 · Log MetaRaw Noise** 节点 | Color › OpenFX | 在 log 画面上降噪，依据摄影机、ISO 与格式的预期噪声；时域与空域，Metal GPU |
| **2 · Stabilize**、**3 · Motion Blur** | Color › OpenFX › Log MetaRaw | 陀螺仪稳定（内置 Gyroflow）与基于光流重建的运动模糊；仅限 Apple 芯片 Mac |
| **4 · Log MetaRaw** 节点 | Color › OpenFX | 以拍摄时的数值为起点显影单个片段：白平衡、曝光、色彩空间、分区影调、伪色 |
| **5 · Log MetaRaw Creative** 节点 | Color › OpenFX | 创意冲印：16 种摄影工艺（负片与拷贝、反转片、黑白、Technicolor、Autochrome、三版分色、达盖尔银版、火棉胶、早期印相），以及模拟录像和老式数码相机；拷贝与翻印代数；按设备与型号的预设（电影摄影机、摄像管与 CCD 摄像机、数码相机与手机）、14 支老镜头、转录（胶转磁、显像管录片、胶片输出）与观看介质（影院、放映机、CRT 与 Trinitron 电视、等离子、早期 LCD、掌机、相纸） |
| **Log MetaRaw Ripresa** 与 **Mix e supporto** 插件（VST3；Mac 上另有 Audio Unit） | Fairlight › 效果 › VST3；Logic Pro › Audio Units › Mecena SRL | 年代声音：音轨上的真实录音机与麦克风，总线和母带上的调音台、载体（磁带、卡带、黑胶、VHS、光学声带、广播、电话）、转录与回放；每个环节可单独关闭；Mac 与 Windows |

不转码任何文件，也从不写入任何原始文件。

### 坦诚地说，能期待什么

**它不是 RAW。** Log MP4 已经过去马赛克和压缩，8 或 10 位，通常为 4:2:0，并且已烘焙了机内降噪。任何插件都无法找回摄影机丢弃的信息。

节点所做的，是严谨地应用色彩科学。曝光和白平衡在线性光中工作，以摄影机记录的数值为起点，遵循公开的曲线和色域；影调以档（stop）为单位移动画面。这样处理后，Log 画面**的表现会让人联想到 RAW 文件**：白平衡移动干净，曝光像一档光那样变化，高光平滑过渡而不是断裂。

但在大幅度调整时，仅靠色彩科学是不够的。一旦超出摄影机的极限，画面中缺失的信息就会显现：天空出现色带，提亮的阴影出现噪点，过曝的高光依然过曝，颜色在压缩通道中崩散。请在拍摄时正确曝光。Log MetaRaw 帮你充分利用已有的信息，但无法创造不存在的信息。

---

## 安装

1. 从 **Releases** 下载 `SLogMetaRaw-4.3.1.dmg` 并打开。
2. 双击 **Installa Log MetaRaw.pkg**。安装包未使用 Apple 证书签名：第一次请右键点击并选择**打开**。因为插件要放入系统文件夹，安装时会要求输入 Mac 密码。
3. 重启 DaVinci Resolve。

安装程序还会删除 Resolve 的插件缓存（`OFXPluginCacheV2.xml`），Resolve 会在下次启动时重建。否则 Resolve 会继续显示旧面板，也看不到 Creative 节点。

| 安装内容 | 路径 |
|---|---|
| 两个节点（同一个 bundle） | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Python 库 | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| 菜单脚本 | `…/DaVinci Resolve/Fusion/Scripts/Utility/Log MetaRaw.py` |

运行时，它会在 `~/Library/Application Support/SLogMetaRaw/cache` 中为每个片段保存一个小的 JSON 记录。磁盘映像中还有十一页的意大利语和英语图解指南。

**干净安装与卸载。** 每次安装都从干净状态开始：安装程序会完整替换之前的插件和库，并移除开发版安装，不会留下旧版本的任何文件。磁盘映像中的 **Disinstalla Log MetaRaw.command**（第一次请右键 › 打开）会移除所有已安装的版本（包括 1.x 和 2.x）以及缓存、设置和日志。它在动手之前会列出全部内容，要求你关闭 Resolve，并询问是否同时删除导出的 CSV。已经写入 Resolve 项目的元数据属于项目本身，会保留。

**系统要求：** macOS 12 或更高版本，Apple 芯片或 Intel，以及 DaVinci Resolve 21 或 20。Resolve 20 不自带 Python：请从 [python.org](https://www.python.org/downloads/) 安装 Python 3（其 Workspace › Scripts 菜单本来也需要）。**仅在 macOS 上的 Resolve Studio 21.1 中测试过；Resolve 20 尚未测试。**

**片段：** `.MP4` 或 `.MXF` 格式的索尼 XAVC，以及 [docs/CAMERAS.md](docs/CAMERAS.md) 中列出的摄影机（Panasonic V-Log、Apple Log、DJI D-Log 与 D-Log M 等），每种都注明读取的可靠程度。脚本全部可以读取。节点可显影 **S-Log3**（S-Gamut3.Cine 或 S-Gamut3）、**S-Log2**、**S-Log**（S-Gamut）以及这些摄影机的曲线。自 3.2.0 起 DJI D-Log M 使用在 Osmo Action 6 上实测的曲线（[docs/DJI_LOG.md](docs/DJI_LOG.md)）：对其他 DJI 摄影机这是最佳估计，而非校准。遇到未知配置时节点保持中性并给出提示。

---

## 快速上手

1. 导入素材。**Workspace › Scripts › Log MetaRaw**：先点 **1 · 读取元数据**，再点 **2 · 写入 Resolve**。
2. 在 Color 页面把 **Log MetaRaw** 放在**第一个节点**。它会取得片段的 EI、开尔文值和色调，在这些数值下画面不变。
3. 在节点中校正白平衡和曝光，伪色视图会有帮助。
4. 用 **Toni**（影调）塑造影调。想要某种胶片、古老工艺、录像带或老式相机的质感时，在其后添加 **Log MetaRaw Creative** 节点。
5. 然后再接你的 CST、LUT 或 DRT。

```
Noise  →  Stabilize  →  Motion Blur  →  Log MetaRaw  →  Log MetaRaw Creative  →  CST / LUT / DRT  →  其余调色
```

---

## 脚本

窗口有一行按钮、一行选项和片段列表，界面语言跟随 Resolve（简体中文、英语、意大利语、西班牙语、葡萄牙语）。节点面板为意大利语，下文在其标签后附有中文说明。

- 用菜单选择片段：*整个媒体池*或*媒体池中选中的片段*。
- **1 · 读取元数据**：每个片段一行，显示摄影机、镜头、光圈、快门、EI、白平衡、色彩空间和数据电平。*状态*列显示 `已读取`、拍摄过程中变化的参数（光圈、对焦……）或跳过片段的原因。点击某一行即可查看读取到的全部数据，分组方式与 Catalyst Browse 相同。
- **2 · 写入 Resolve**：填写媒体池字段（元数据面板、列、用于智能素材箱的关键词、数据叠印），并修正 Resolve 从 MXF 中读错的值，例如 FX6 上的 *Camera Aperture* `F53343`。
- **导出 CSV**：以 Resolve 自己的元数据 CSV 格式导出 Resolve 没有对应字段的数值（EI、色调、白平衡模式、对焦距离、拍摄伽马……）。
- 选项：
  - **相机标签**（关）：把摄影机、伽马和原色加入关键词。
  - **覆盖元数据**（开）：替换 Resolve 已填写的值；关闭时只填写空字段。
  - **修正数据电平**（开）：按伽马的需要把每个片段的 *Data Level* 设为 Full 或 Video。原因见 [docs/DATA_LEVELS.md](docs/DATA_LEVELS.md)。
  - **同时设置 Input Color Space**（关）：用于色彩管理项目。脚本无法把它改回 *Project*，只能手动改回。
- 点击右下角的版本号即可检查 GitHub 上的更新。

读取很快，因为不解码任何内容：每个片段最多读取元数据轨道的 24 个样本，时间上限为一秒。停止响应的卷只会被跳过一次并给出提示，而不会卡住列表。

---

## Log MetaRaw 节点

该节点是逐点处理的：每个像素只取决于它自身。它不会产生光晕，**Generate LUT** 可以导出它（建议 65 点）。

| 控件 | 作用 |
|---|---|
| **版本**（顶部） | `v4.3.1`。每天最多向 GitHub 查询一次最新版本。若有新版本，显示 **🟢 v3.1.0 → 3.x.y**，点击会在浏览器中打开适合本系统的安装程序下载（`.dmg`、`.exe`、`.deb`/`.rpm`/`.run`）。它自己从不安装任何东西 |
| **Camera** · **Rileggi metadata** | 读取到的摄影机。*Rileggi*（重新读取）会重新读取片段，把所有控件恢复为摄影机数值，并把该片段的元数据写入媒体池 |
| **Ripresa**（拍摄）· **Decodifica** | *Clip* 可以修改控件；*Camera metadata* 把控件锁定为拍摄时的数值 |
| **Bilanciamento**（平衡）· **Temperatura** · **Tint** | As shot 或预设。在线性光中做 Bradford 色适应，基于相机记录的白点 |
| **Punto bianco**（白点） | Resolve 的吸管：在图像上选一个应为中性的点，Temperatura 和 Tint 自动调整，与渲染使用相同的数学 |
| **Esposizione**（EI） | 曝光指数：EI 加倍 = +1 档 |
| **False color** | Temperatura 上方、Tint 下方和 Esposizione 下方各一个复选框；见下文 |
| **Spazio colore**（色彩空间） | 三个子组。**Ingresso**（输入）：进入节点的 Spazio colore 和 Gamma、Rilevato（检测到的）与 *Ripristina da camera*、Data level、Livello 与 *Ripristina da clip*。**Senza metadata**：*Sblocca controlli* 和拍摄参考值。**Uscita**（输出）：**Tone mapping**（Nessuno/CST、ACES 2.0、Luminanza、Filmico、ACES 1.3、OpenDRT）、Compressione 与 Mappatura gamut、Vibrance luci、Spazio colore 和 Gamma（类似 Color Space Transform；*Timeline* 不转换）、LUT 大小与导出 |
| **Toni**（色调） | Highlights、**Bianco**（档）、Soft Clip、Saturation（−100…+100）。*Mostra tutti i controlli* 加入 Contrast、Shadows、Whites、Blacks、Vibrance 和 **Zone**（Black、Shadow、Light、Specular 的 Exp、Sat、Range 和 Falloff，Contrast Pivot，*Zone* false color）；节点使用它们时会自动出现 |
| **Dati di ripresa**（拍摄数据） | 只读：镜头、焦距、光圈、对焦、快门、EI、白平衡、帧率、ND、机内 LUT |

**Highlights 是胶片式肩部。** 向负方向调节时，它以向上逐渐平缓的斜率压缩高光，就像胶片、ACES 2.0 和 AgX 一样。在 −100 时，摄影机记录到的最亮值（S-Log3 中约为中灰以上 +6 档）正好落在 **Bianco** 上：没有灰雾，也没有剪切。中灰及以下保持不动，+1 档的肤色最多移动 0.05 档。压缩最多的高光会平缓地趋向白色而不改变色相。向正方向调节时，高光更有力度，斜率有上限。**Bianco** 是这个最大值（以及 Soft Clip 顶部）所在的位置：2.5 档是不带色调映射的 CST 下的 Rec.709 白。如果节点之后有 DRT（ACES、AgX、DaVinci），请调到 4–5，否则高光会被压缩两次。

**分区影调。** Shadows、Whites 和各分区是对一段影调范围、以档为单位的曝光调整：Shadows 作用于 −1 档以下，Whites 从 +3.5 档开始。Blacks 是一层线性薄雾，移动黑位而不移动中灰。在分区内部，画面像曝光一样整体移动，因此纹理得以保留；压缩只发生在声明的过渡带中。任何滑块组合都不会产生色调分离（solarise）。坦白说逐点节点的代价是：它压缩的部分，纹理也一起被压缩。正因如此，局部恢复放在单独的节点中。公式与用法：[docs/TONE_MAPPING.md](docs/TONE_MAPPING.md)。

**伪色。** 每个控件一个视图，位于它所服务的滑块旁（Temperatura 上方、Tint 和 Esposizione 下方）：
- **曝光**：以 18% 灰为中心按档分带，采用 ARRI 风格。绿色是中灰，粉色高一档（肤色），黄色接近过曝，红色为过曝，蓝色和紫色为暗部底端。
- **色温**和**色调**：与 CineMatch 相同。画面变为灰色，偏色显示为橙/蓝或绿/品红，接近中性的偏色最多放大 8 倍以便看清。移动滑块，直到应为中性的区域保持灰色。每个视图只响应自己的滑块。

伪色视图会替换画面：渲染前请关闭。[docs/FALSE_COLOR.md](docs/FALSE_COLOR.md)

**数据电平。** 如果 Resolve 用错误的码值范围解码片段，节点会在 Log 曲线之前进行校正。脚本的*修正数据电平*选项可以为整个项目修正。[docs/DATA_LEVELS.md](docs/DATA_LEVELS.md)

**带场景输出的 tone mapping（3.2.0）。** 选用渲染（ACES 2.0、Filmico、OpenDRT…）时，DaVinci WG / DaVinci Intermediate 这类场景 Spazio colore 和 Gamma 不再被替换为 Rec.709 / Gamma 2.4：为 Rec.709 渲染的画面以该编码传递，便于交给其他插件（Dehancer…）；之后用不带 tone mapping 的 CST 转到 Rec.709 / Gamma 2.4 即可完全还原。显示输出（Rec.709、P3、Rec.2020）不变。

**没有元数据时。** 外部记录器录制的 ProRes 或无法读取的片段会让节点保持中性。勾选 *Spazio colore › Senza metadata › Sblocca controlli* 并输入拍摄时的 EI、开尔文值和色调：控件随即启用，节点从中性状态开始。

**速度。** 打开项目时不会读取任何文件。打开面板最多等待半秒；慢速磁盘会在后台完成读取，上限 15 秒。*Rileggi* 最多约 2 秒内响应。

---

## Log MetaRaw Creative 节点

一个引擎重做**工艺本身**，而不是模仿结果。场景光解码为线性 Rec.2020 后，变成最多三个带光谱灵敏度的**记录**；每个记录在材料的曲线上曝光，再按该工艺的方式成像：减色染料、加色滤光、纸上的银或铂沉积，或视频信号。每种工艺是一张数字表，面板只显示所选工艺用得到的控件。

**Setup d'epoca**（面板顶部）：一次载入某部影片或某个时代的真实器材链：À bout de souffle, Vivre sa vie, Le Mépris, Pierrot le fou, Numéro deux, Accattone, Il Vangelo secondo Matteo、卢米埃尔电影机、早期国家电视台和 80 年代商业电视。摄影机、画幅、胶片、冲洗、镜头、拷贝、转录和观看方式都依据资料；同一列表也是两个音频插件的第一个菜单（录音机、话筒和磁带；调音台、母带、拷贝声带和监听）。资料与推断部分见 `docs/SETUPS.md`。

| 工艺 | 模拟内容 |
|---|---|
| **彩色负片与拷贝** | 三层各自的反差、趾部与肩部、层间效应、分层颗粒、光晕、带 R G B 印片光的正片；14 种胶片，包括东欧的 Sovcolor、ORWO、Svema、Gevacolor。*Log 输出*给出负片扫描 |
| **反转片**、**黑白** | 幻灯片；黑白可选蓝敏、正色或全色以及补偿后的拍摄滤镜 |
| **Technicolor** 三条与双色 | 分光棱镜的三张负片、染印矩阵、1944 年前的 K 键；双色只有红与绿 |
| **Autochrome**、**Dufaycolor** | 染色淀粉颗粒或规则网屏的加色马赛克，透射观看 |
| **三版分色**、**Kinemacolor** | 顺序曝光：*Intervallo* > 0 时节点读取后续帧，运动的物体分离成彩色（Prokudin-Gorsky） |
| **达盖尔银版**、**火棉胶**、盐纸与蛋白、蓝晒、铂金 | 蓝敏与紫外敏感的沉积、镜面与氧化、片基、边缘、调色 |
| **模拟录像** | 11 种格式，从广播到 VHS LP 与 Betacam SP：亮度与色度带宽、色度延迟、峰化、噪声、磁头切换、掉码 |
| **老式数码** | 10 款相机：削波与拐点、电子锐化、噪声、色度子采样、色带 |

- 放在 Log MetaRaw **之后**。它解码为线性光，输出 Rec.709 · Gamma 2.4（放映拷贝），选 *Log 输出*时保持输入编码。
- 它是**空间**节点，Generate LUT 会排除它。半径随帧高变化；噪声是确定性的，CPU 与 Metal 画面一致。
- 它在同一位置取代了 Detail 节点：用 Detail 保存的调色会以全新的 Creative 打开。

数值是 [docs/CREATIVE.md](docs/CREATIVE.md) 中研究得出的估计，不是任何产品的配置，需要用真实素材校准。尚未在 Resolve 中测试；Metal 由 macOS CI 测试验证。

**4.3.1 新功能。**

- **音频选项一致**：两个音频插件的每个菜单只提供与上方选择相符的项：先是录音机自带的介质、话筒和降噪，再是兼容选项（Nagra 不能录 VHS）；在 *Mix e supporto* 中，调音台决定母带介质，母带决定可转录到什么。
- **片门毛发、灰尘和划痕更真实**：毛发向尖端变细，密度不均，阴影像胶片上一样模糊（Super 8 和拷贝上更明显）；有时卷曲，或是双股纤维。灰尘边缘不规则、大小不一；划痕沿线条改变粗细和强度。
- **带厂商边缘印字的扫描**：边到边视图的片边印有品牌（1990 年前 EASTMAN，之后 KODAK；FUJI、AGFA、ORWO…）、该规格的片号、NITRATE FILM / SAFETY FILM / ESTAR、流水号，1990 年起还有 KeyKode 条码；照相胶片标出帧号。扫描仪光线在齿孔周围和片基切边处有光晕。新增「扫描过扫」控制。
- **拷贝上的声带**：边到边扫描时，35 mm 或 16 mm 拷贝按 SMPTE 位置显示声带：光学声带（变密度、变面积、Dolby
  Stereo），1993 年起片孔之间的 Dolby Digital、DTS 和 SDDS，2004 年起青色声带；Super 8 和 16 mm 可带磁条。新菜单
  *Colonna sonora della copia*（按年代或手动）。

**4.3.0 新功能。**

- **按画幅筛选胶片**：选定摄影机后只显示该画幅生产过的胶片，涵盖所有厂商并使用该画幅的编号（16 mm 中 500T 为 7219）；完整清单只需勾选即可。
- **扫描视图**：仅画面、带片门边缘的画面，或带齿孔、画格线和相邻画格的整条胶片，按各画幅的真实尺寸。片门可选方形、圆角或锉开。
- **负片原貌**：透光观看的彩色负片，在橙色片基上呈互补色，带有自身的颗粒。
- **片基材料**：硝酸片基、醋酸片基或涤纶片基（或按年代和画幅自动选择）及其老化：硝酸片基的琥珀色斑块，醋酸片基的结晶与不稳定。
- **真实的画面晃动**：拍摄、印片和放映各自独立，各有开关；按摄影机产生的**片门毛发**。
- **游戏机**作为观看介质：NES、Master System、Mega Drive、Super Nintendo、PlayStation 与 Saturn，接显像管电视。
- 修复了选择 4.2.0 新增观看介质时的崩溃。

**4.2.0 新功能。**

- **设备 › 型号**：从机器出发（电影摄影机、摄像机、数字电影机、手机），自动载入画幅、磁带、传感器与缺陷；再选厂商与胶片，均为真实名称。*色调*、*色彩*、*纹理* 开关可以只在自己的视频上使用颗粒等单项。
- **模拟视频**：VHS 与 Betacam 磁带逐行重建（FM 噪声、色度延迟、磁头切换、信号脱落），以及摄像机传感器：摄像管拖影与彗尾、CCD 拖尾、条纹管的伪色。
- **按型号的数码相机**：传感器尺寸与混叠、机内锐化、编码器的 8x8 块与色度抽样（从 Motorola RAZR 到 ARRI Alexa）。
- **转录到**：转录到 VHS、Betacam、第三代 VHS、16 mm 显像管录片或 35 mm 胶片输出，颗粒叠加。
- **镜头**：14 支镜头（Cooke、Zeiss Super Speed、Canon K35、Panavision、LOMO、Helios、Petzval…），含畸变、边缘柔化、旋涡、眩光与变形宽银幕拉丝。
- **新的观看介质**：Super 8 放映机、Trinitron、等离子、早期 LCD。

---

## Noise、Stabilize 与 Motion Blur 节点（4.0.0）

它们处理摄影机画面，位于 **Log MetaRaw** 之前。

- **Log MetaRaw Noise** 知道该预期多少噪声、出现在哪里。它从脚本记录中读取摄影机、ISO（或 EI 与原生 ISO）、编码、每像素
  比特数与 4:2:0，并为 Sony FX6、FX30、iPhone 17 Pro、DJI Osmo Action 4 和 5 Pro 提供配置。按钮 **Rileggi metadata** 会从文件重新读取这些数据（打开面板时如缺失会自动读取）。要按真实噪声校准：勾选 **Mostra campione**，把方框的两个角拖到平坦区域，再按 **Usa il campione**（类似 Neat Video）；频带、暗部和肤色在 **Avanzate** 组中。
  - *时域*：±1 至 ±3 个相邻帧，按块对齐并逐像素加权，运动部分和剪辑点不会糊在一起。
  - *空域*：亮度与色彩分开处理，分 5 个小波频带。每个系数与其邻域的能量比较，从而保留纹理。
  - 提供已去除噪声的视图、细节与肤色保护，以及在剪辑时关闭所有 Noise 节点的 **Salta Noise ovunque**。
  - 在 Mac 上全部在 Metal GPU 上运行，测试验证其结果与 CPU 一致；其他系统在 CPU 上运行。
  - 配置在用真实样本校准之前都是估计值。[docs/DENOISE.md](docs/DENOISE.md)
- **Stabilize** 读取片段的陀螺仪数据（内置 Gyroflow），提供从微抖到摇臂的风格，以及可设关键帧的 **MASTER**。
  **Motion Blur** 基于光流重建快门，例如从 50p 90° 到 25p 180°，并在模糊区域保留传感器颗粒。两者都来自 Mecena Motion（`motion/`），由 macOS 安装包
  安装，仅限 Apple 芯片。
- **深度采集**是脚本窗口中的一个勾选项：把每一帧的陀螺仪和镜头数据存入 MecenaClipDB，供 Stabilize 使用。

这三个节点都尚未在 Resolve 中用真实片段测试。

## Log MetaRaw Audio（VST3，4.2.0）

Creative 的声音孪生，用于 **Fairlight**（Mac 与 Windows）：一个模块 `Log MetaRaw Audio.vst3` 含两个插件。

- **Ripresa**（录音），用于音轨：录音机（Geloso G.257、Nagra、Uher、Studer、Revox、Walkman、Auricon、爱迪生留声机、钢丝录音机…）、话筒（原配或 RCA 44、U 47、C12、SM58、MKH 416…）、介质与降噪。
- **Mix e supporto**（混音与介质），用于总线与母带：调音台（RCA、EMI REDD.51、Neve、SSL、Mackie）、介质（磁带、卡带、78/45/33 转、VHS、光学声迹、收音机、电话）、转录与收听（电子管收音机、点唱机、电视、影院）。
- 每一级都有开关；饱和、噪声、抖晃、干/湿与输出调整结果。
- 在安装程序中为独立组件，默认勾选。Linux：Resolve 不加载原生 VST3。

数值为根据当年规格与描述得出的估计（[docs/audio/AUDIO.md](docs/audio/AUDIO.md)）；尚未在 Fairlight 中测试。

## 更新与隐私

- **节点**每天最多在后台向 GitHub 查询一次本项目的最新版本。请求中只包含程序版本（`User-Agent: SLogMetaRaw/4.3.1`）。如需关闭，请创建空文件 `~/Library/Application Support/SLogMetaRaw/no_update_check`。
- **脚本**只在你点击其版本号时检查。
- 点击只会打开本项目 GitHub 发布页中的下载链接。未经你的操作，不会安装任何东西。

---

## 工作原理（简述）

- **元数据。** 解析器为纯 Python，无任何依赖。它们逐帧读取 SMPTE RDD 18 采集元数据（MP4 的 `rtmd` 轨道、MXF 的 ST 436 数据包）、索尼的 NonRealTimeMeta XML、H.264/HEVC 的 SPS 以及 MXF 图像描述符。一个数 GB 的片段只需读取约 100 KB。
- **从脚本到节点。** 每个片段在缓存中有一条 JSON 记录。节点通过 Resolve 提供的源文件路径找到文件，读取该记录；若记录缺失，则在后台生成。
- **色彩数学。** 处理链为：解码摄影机曲线，按 EI 比例调整，从普朗克轨迹出发进行 Bradford 白平衡（色调在 CIE 1960 uv 中垂直于轨迹），然后以像素各通道范数计算的单一增益处理影调（色度保持不变），再处理色彩，最后输出。全部在线性光中进行，使用索尼公开的曲线与色域。
- **一套数学，三种实现。** 数学代码位于 `ofx/SLogMetaRaw/math`，同时编译为 C++ 和 Metal。一个 Python 参考模型对两者进行校验：CPU 与 GPU 的差异在 2·10⁻⁴ 以内，所有路径均禁用 FMA。
- **测试。** 520 项自动化测试：解析器、参考模型、C++、Metal、一个像 Resolve 一样加载两个节点的微型 OpenFX 宿主，以及面板的 golden 文件。

---

## 从源码构建

```bash
make -C ofx/SLogMetaRaw                     # 通用插件，arm64 + x86_64（需要 Xcode）
make -C ofx/SLogMetaRaw test-bins           # 测试程序
python3 -m unittest discover tests          # 运行测试
./install.sh --dev                          # 让脚本和插件指向当前文件夹
./packaging/build_installer.sh              # 生成 dist/SLogMetaRaw-<版本>.dmg
python3 -m slogmetaraw clip.MP4             # 命令行中以 Catalyst 风格显示
```

```
slogmetaraw/         解析器、写入 Resolve、脚本窗口、更新检查
resolve_script/      Workspace › Scripts 的启动器
ofx/SLogMetaRaw/     math/（CPU/Metal 共用）、src/（common、develop、detail、noise）、metal/
motion/              Mecena Motion：Stabilize、Motion Blur、mecena-clipdb（Rust + C++，macOS）
tools/               矩阵、图标和图表生成器
tests/               测试与参考模型（示例片段不在仓库中）
packaging/           安装程序、指南、卸载程序
docs/                TONE_MAPPING、CREATIVE、DENOISE、FALSE_COLOR、DATA_LEVELS
```

### Linux 与 Windows（CPU 渲染，实验性）

该插件也可以用 CMake 为 Linux 和 Windows 编译。这些移植版本仅使用 CPU 渲染（尚无 Metal、CUDA 或 OpenCL 支持），
且尚未在 DaVinci Resolve 内部测试过：目前仍以 macOS 版本为官方支持对象。

**Linux 安装**（从 **Releases** 下载适合你发行版的文件）：

| 文件 | 适用于 |
|---|---|
| `slogmetaraw_<版本>_amd64.deb` | Ubuntu、Debian、Mint：`sudo apt install ./slogmetaraw_*.deb` |
| `slogmetaraw-<版本>-1.x86_64.rpm` | Rocky、Alma、RHEL、CentOS、Fedora：`sudo dnf install ./slogmetaraw-*.rpm` |
| `SLogMetaRaw-<版本>-linux-x86_64.run` | 任意发行版：`sh SLogMetaRaw-*.run`（免 sudo 用 `--user`，卸载用 `--uninstall`） |
| `SLogMetaRaw-<版本>-linux-x86_64.tar.gz` | 同上，未打包版本：运行其中的 `install.sh` |

插件安装到 `/usr/OFX/Plugins`；Python 库安装到 `/usr/lib/slogmetaraw`（.deb/.rpm）或
`~/.local/share/SLogMetaRaw`（.run、.tar.gz）。如果系统中有 Resolve，安装包会把菜单脚本放到
`/opt/resolve/Fusion/Scripts/Utility`；否则请为当前用户运行一次 `slogmetaraw-setup`。然后重启 Resolve。
只需 Python 3.6 及以上（Rocky/RHEL 8 也满足）。该二进制基于 glibc 2.28 编译，可在 Rocky/Alma/RHEL 8+、
Ubuntu 20.04+ 和 Debian 10+ 上运行。

```bash
# 自行编译（需要 cmake、C++17 编译器、python3；OpenFX SDK 会自动下载，或设置 OFX_SDK_DIR）
./packaging/linux/build_package.sh          # dist/：.tar.gz、.run、.deb、.rpm
# Windows（PowerShell）：需要 cmake、Visual Studio Build Tools、python；Inno Setup 可选
powershell -File packaging\windows\build_package.ps1   # dist\...-windows-x64.zip（以及用 Inno Setup 生成的安装程序 .exe）
```

状态文件在 Linux 上位于 `~/.local/share/SLogMetaRaw`，在 Windows 上位于 `%APPDATA%\SLogMetaRaw`。
`.github/workflows/` 会在每次改动时构建这些安装包，并在 Rocky 8/9、Ubuntu 20.04-24.04 和 Debian 12 上安装
.deb/.rpm 进行验证；推送 `v*` 标签会把它们附加到一个草稿 release。

---

## 已知限制

- Resolve 自带的 Camera Raw 面板和陀螺仪稳定无法为 MP4 解锁（陀螺仪稳定可用 Stabilize 节点）：它们位于 Resolve 的解码器内部。Log MetaRaw 重建的是色彩控件，并不是打开 Resolve 内部的某扇门。
- 部分摄影机（包括 a6300）不记录开尔文值：此时根据光源预设估算，并加以标注。
- S-Log2 按照索尼文档实现。Resolve 自带的 S-Log2 曲线与之相差约 0.15 档。
- 尚需在更多文件上验证：XAVC HS（HEVC）、HLG、S-Cinetone、电动变焦。
- **独立的业余项目**，按现状分发，不提供任何担保，也不对专业用途承担责任。代码开源：欢迎阅读、测试和修改。

---

## 致谢与许可

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m)。
与 Claude（Anthropic）共同编写：1.0 于 2026 年 9 月 19 日，1.1.0 于 9 月 22 日，2.0.0 于 2026 年 9 月 23 日，3.2.0 与 4.0.0 于 2026 年 11 月 24 日。完整历史见 [RELEASE_NOTES.md](RELEASE_NOTES.md)（意大利语）。

索尼标签参考：SMPTE RDD 18、[ExifTool](https://exiftool.org) 以及 AdrianEddy 的 [telemetry-parser](https://github.com/AdrianEddy/telemetry-parser)（MIT），部分标签表来自后者。

渲染与摄影机数据：ACES 2.0 Output Transform 移植自 OpenColorIO 2.6，ACES 1.3 以其内置变换核对（ACES：Academy of Motion Picture Arts and Sciences）；[OpenDRT](https://github.com/jedypod/open-display-transform) v1.1.0 由 Jed Smith 编写（GPL-3.0），移植自其 DCTL。DJI D-Log M 的曲线和色域来自 [jens-krause/dlogm-dctl](https://github.com/jens-krause/dlogm-dctl) v1.3 中 Osmo Action 6 的实测，以及 DJI 的 Action 6 LUT（仅作参考，不随项目分发）；3.1.x 及之前的曲线是 Thatcher Freeman 的拟合。

Stabilize 基于 AdrianEddy 及贡献者的 [Gyroflow](https://github.com/gyroflow/gyroflow)（GPL-3.0）及其镜头库。Noise 节点的空域滤波参考 NeighShrink（Chen、Bui、Krzyżak，2004）。

[GNU GPL v3.0 或更高版本](LICENSE)。OpenFX SDK 版权归 The Open Effects Association 所有（BSD-3）。VST3 SDK 版权归 Steinberg Media Technologies 所有（MIT）；VST 是 Steinberg 的商标。Sony、XAVC 和 Catalyst 是 Sony Group Corporation 的商标；DaVinci Resolve 是 Blackmagic Design 的商标。本项目为独立项目，与上述两家公司无隶属关系，也未获其认可。

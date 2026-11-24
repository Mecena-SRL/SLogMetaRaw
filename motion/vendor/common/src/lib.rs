
use lru::LruCache;
use parking_lot::{ Mutex, RwLock };
use std::cell::Cell;
use std::sync::{ Arc, Weak, atomic::AtomicBool };

pub use gyroflow_core::{ StabilizationManager, keyframes::*, stabilization::*, filesystem, gpu::* };
pub use gyroflow_core;

// re-exports
pub use rfd;
pub use parking_lot;
pub use lru;
pub use serde_json;
pub mod clipdb;

pub type PluginResult<T> = Result<T, Box<dyn std::error::Error>>;

#[derive(Debug, Copy, Clone, Hash, PartialEq, PartialOrd, Eq, Ord, serde::Serialize, serde::Deserialize)]
pub enum Params {
    Logo,
    InstanceId,
    ProjectData,
    EmbeddedLensProfile,
    EmbeddedPreset,
    ProjectGroup, ProjectGroupEnd,
    LoadCurrent,
    ProjectPath,
    Browse,
    LoadLens,
    OpenGyroflow,
    ReloadProject,
    OpenRecentProject,
    Status, ClipReport,
    AdjustGroup, AdjustGroupEnd,
    Fov,
    Smoothness,
    ZoomLimit,
    LensCorrectionStrength,
    HorizonLockAmount,
    HorizonLockRoll,
    StabPreset,
    SmoothingMethod,
    SmoothPitch,
    SmoothYaw,
    SmoothRoll,
    TimeConstant,
    FastTimeConstant,
    MotionInertia,
    MotionBrake,
    CropConstraint,
    ZoomMode,
    ZoomWindow,
    // PositionX,
    // PositionY,
    AdditionalPitch,
    AdditionalYaw,
    InputRotation,
    Rotation,
    VideoSpeed,
    DisableStretch,
    IntegrationMethod,
    KeyframesGroup, KeyframesGroupEnd,
    UseGyroflowsKeyframes,
    RecalculateKeyframes,
    OutputSizeGroup, OutputSizeGroupEnd,
    OutputWidth,
    OutputHeight,
    OutputSizeToTimeline,
    OutputSizeSwap,
    ToggleOverview,
    DontDrawOutside,
    IncludeProjectData,
    StabilizationSpeedRamp,
    InfoGroup, InfoGroupEnd,
    LoadedProject,
    LoadedPreset,
    LoadedLens,
    CreateCamera,
    Interpolation,
    FusionStartFrame,
    SaveDefault,
}

thread_local! {
    pub static LOG_INITIALIZED: Cell<bool> = Cell::new(false);
}

pub struct GyroflowPluginBase {
    // We should cache managers globally because it's common to have the effect applied to the same clip and cut the clip into multiple pieces
    // We don't want to create a new manager for each piece of the same clip
    // Cache key is specific enough
    pub manager_cache: Mutex<LruCache<String, Arc<StabilizationManager>>>,

    pub context_initialized: bool,
}
impl Default for GyroflowPluginBase {
    fn default() -> Self {
        Self {
            manager_cache: Mutex::new(LruCache::new(std::num::NonZeroUsize::new(8).unwrap())),
            context_initialized: false,
        }
    }
}

impl GyroflowPluginBase {
    /// If `disable_stretch` is true, inject a `plugin_disable_stretch` flag into gyroflow JSON data
    /// so that the setting persists when the data is embedded in a preset or project.
    fn maybe_inject_disable_stretch(data: &str, disable_stretch: bool) -> String {
        if !disable_stretch { return data.to_string(); }
        if let Ok(mut json) = serde_json::from_str::<serde_json::Value>(data) {
            json["plugin_disable_stretch"] = serde_json::Value::Bool(true);
            if let Ok(s) = serde_json::to_string(&json) {
                return s;
            }
        }
        data.to_string()
    }

    /// Marks `data`, a preset or a lens profile loaded with "Load preset/lens profile", as loaded over a project. One loaded
    /// over the video doesn't apply over a project loaded after it: the project made in the app has its settings then
    pub fn mark_loaded_over_project(data: &str) -> String {
        if let Ok(mut json) = serde_json::from_str::<serde_json::Value>(data) {
            if let Some(obj) = json.as_object_mut() {
                obj.insert("plugin_over_project".into(), serde_json::Value::Bool(true));
                if let Ok(s) = serde_json::to_string(&json) {
                    return s;
                }
            }
        }
        data.to_string()
    }
    /// Whether `data`, a preset or a lens profile, was loaded over a project (see `mark_loaded_over_project`)
    pub fn loaded_over_project(data: &str) -> bool {
        serde_json::from_str::<serde_json::Value>(data).ok().and_then(|x| x.get("plugin_over_project")?.as_bool()).unwrap_or_default()
    }

    /// The video file a project was made for, `None` for a preset
    pub fn project_video_file(data: &str) -> Option<String> {
        let json = serde_json::from_str::<serde_json::Value>(data).ok()?;
        json.get("videofile").and_then(|x| x.as_str()).filter(|x| !x.is_empty()).map(|x| x.to_string())
    }
    /// The output size the project `json` exports at (its "output" section), `None` when it doesn't say. The core only
    /// keeps the aspect of it: `StabilizationManager::set_output_size` fits every size to the video, so a 1496x2660
    /// export of a vertical 4K clip is 2160x3840 in `params.output_size` after the project loads
    pub fn project_output_size(json: &serde_json::Value) -> Option<(usize, usize)> {
        let output = json.get("output")?;
        let w = output.get("output_width")?.as_u64()? as usize;
        let h = output.get("output_height")?.as_u64()? as usize;
        (w > 0 && h > 0).then_some((w, h))
    }

    /// The settings of a project, as the preset the app makes of it by default (see `SettingsSelector.qml` in the app).
    /// The project's video file, motion data, sync points and trim belong to its clip and don't come along
    pub fn project_to_preset(data: &str, name: &str) -> Option<String> {
        let json = serde_json::from_str::<serde_json::Value>(data).ok()?;
        let mut preset = serde_json::Map::new();
        let mut copy = |group: Option<&str>, keys: &[&str]| {
            let (src, dst) = match group {
                Some(group) => (json.get(group), preset.entry(group).or_insert_with(|| serde_json::json!({})).as_object_mut()),
                None => (Some(&json), Some(&mut preset)),
            };
            if let (Some(src), Some(dst)) = (src, dst) {
                for key in keys {
                    if let Some(v) = src.get(*key) {
                        dst.insert(key.to_string(), v.clone());
                    }
                }
            }
        };
        copy(None, &["version", "calibration_data", "light_refraction_coefficient", "plugin_disable_stretch", "keyframes",
                     "background_color", "background_mode", "background_margin", "background_margin_feather"]);
        copy(Some("gyro_source"), &["lpf", "mf", "rotation", "acc_rotation", "gyro_bias", "imu_orientation", "integration_method"]);
        copy(Some("synchronization"), &["initial_offset", "initial_offset_inv", "search_size", "calc_initial_fast", "max_sync_points",
                                        "every_nth_frame", "time_per_syncpoint", "of_method", "offset_method", "pose_method", "auto_sync_points"]);
        copy(Some("stabilization"), &["fov", "method", "smoothing_params", "horizon_lock_amount", "horizon_lock_roll", "horizon_lock_pitch_enabled",
                                      "horizon_lock_pitch", "use_gravity_vectors", "automatic_lock", "turn_threshold", "turn_smoothing_ms", "turn_multiplier",
                                      "tilt_accel_limit", "frame_readout_time", "frame_readout_direction",
                                      "adaptive_zoom_window", "adaptive_zoom_center_offset", "adaptive_zoom_method", "additional_rotation",
                                      "additional_translation", "max_zoom", "max_zoom_iterations", "fixed_zoom", "zoom_limit_whole_clip", "lens_correction_amount",
                                      "output_projection", "video_speed", "video_speed_affects_smoothing", "video_speed_affects_zooming", "video_speed_affects_zooming_limit",
                                      "focal_length_smoothing_enabled", "focal_length_max_zoom_rate", "lens_metadata_delay_frames", "lens_breathing_enabled"]);
        copy(Some("output"), &["output_width", "output_height", "interpolation"]);
        preset.retain(|_, v| !v.as_object().is_some_and(|x| x.is_empty()));
        preset.insert("name".into(), serde_json::Value::String(name.into()));
        serde_json::to_string(&preset).ok()
    }

    /// The footage of the video file `path`, `None` when it can't be read
    pub fn video_footage(path: &str) -> Option<Footage> {
        let url = filesystem::path_to_url(path);
        let mut file = filesystem::open_file(&url, false, false).ok()?;
        let filesize = file.size;
        let md = gyroflow_core::util::get_video_metadata(file.get_file(), filesize, &url).ok()?;
        Some(Footage { size: (md.width, md.height), fps: md.fps, duration_ms: md.duration_s * 1000.0 }).filter(Footage::is_known)
    }
    /// The footage the project `data` was made for (a video's data embedded with "Include project data" says it too)
    pub fn project_footage(data: &str) -> Option<Footage> {
        let json = serde_json::from_str::<serde_json::Value>(data).ok()?;
        let info = json.get("video_info")?;
        let num = |key: &str| info.get(key).and_then(|x| x.as_f64());
        Some(Footage { size: (num("width")? as usize, num("height")? as usize), fps: num("fps")?, duration_ms: num("duration_ms")? }).filter(Footage::is_known)
    }

    /// Whether two paths are of the same file, by name when one of them is only a name
    pub fn same_file(a: &str, b: &str) -> bool {
        let normalize = |x: &str| {
            let x = x.trim().replace('\\', "/");
            if cfg!(any(target_os = "windows", target_os = "macos")) { x.to_lowercase() } else { x }
        };
        let (a, b) = (normalize(a), normalize(b));
        if a.contains('/') && b.contains('/') {
            a == b
        } else {
            a.rsplit('/').next() == b.rsplit('/').next()
        }
    }

    pub fn initialize_gpu_context(&mut self) {
        log::info!("GyroflowPluginBase::initialize_gpu_context");
        if !self.context_initialized {
            gyroflow_core::gpu::initialize_contexts();
            self.context_initialized = true;
        }
    }
    pub fn deinitialize_gpu_context(&mut self) {
        log::info!("GyroflowPluginBase::deinitialize_gpu_context");
    }

    pub fn initialize_log(&self, name: &str) {
        LOG_INITIALIZED.with(|x| {
            if !x.get() {
                log_panics::init();

                // #[cfg(target_os = "windows")] { win_dbg_logger::init(); }

                let tmp_log = std::env::temp_dir().join(format!("gyroflow-{name}.log"));

                let log_path = gyroflow_core::settings::data_dir().join(format!("gyroflow-{name}.log"));
                let log_config = [ "mp4parse", "wgpu", "naga", "akaze", "ureq", "rustls", "ofx" ]
                    .into_iter()
                    .fold(simplelog::ConfigBuilder::new(), |mut cfg, x| { cfg.add_filter_ignore_str(x); cfg })
                    .build();

                if let Ok(file_log) = std::fs::File::create(&log_path) {
                    let _ = simplelog::WriteLogger::init(log::LevelFilter::Debug, log_config, file_log);
                    x.set(true);
                } else if let Ok(file_log) = std::fs::File::create(&tmp_log) {
                    let _ = simplelog::WriteLogger::init(log::LevelFilter::Debug, log_config, file_log);
                    x.set(true);
                } else if cfg!(target_os = "linux") {
                    if let Ok(file_log) = std::fs::File::create(&format!("/tmp/gyroflow-{name}.log")) {
                        let _ = simplelog::WriteLogger::init(log::LevelFilter::Debug, log_config, file_log);
                        x.set(true);
                    } else {
                        eprintln!("Failed to create log file: {log_path:?}, {tmp_log:?}, /tmp/gyroflow-ofx.log");
                    }
                }
            }
        });
    }

    pub fn get_center_rect(width: usize, height: usize, org_ratio: f64) -> (usize, usize, usize, usize) {
        // If aspect ratio is different
        let new_ratio = width as f64 / height as f64;
        if (new_ratio - org_ratio).abs() > 0.1 {
            // Get center rect of original aspect ratio
            let rect = if new_ratio > org_ratio {
                ((height as f64 * org_ratio).round() as usize, height)
            } else {
                (width, (width as f64 / org_ratio).round() as usize)
            };
            (
                (width - rect.0) / 2, // x
                (height - rect.1) / 2, // y
                rect.0, // width
                rect.1 // height
            )
        } else {
            (0, 0, width, height)
        }
    }

    /// Whether a rotation in degrees is a quarter turn either way, ie. swaps the width and height of the frame
    pub fn is_quarter_turn(rotation: f64) -> bool {
        (rotation.rem_euclid(180.0) - 90.0).abs() < 1.0
    }
    /// `size` turned by `rotation` degrees
    pub fn rotated_size(size: (usize, usize), rotation: f64) -> (usize, usize) {
        if Self::is_quarter_turn(rotation) { (size.1, size.0) } else { size }
    }
    /// The InputRotation of a video file: hosts show the video the right way up, turned by its rotation metadata, and the
    /// input rotation is that turn from the frame Gyroflow stabilizes (the decoded frame, before the metadata is applied)
    pub fn input_rotation_from_metadata(metadata_rotation: i32) -> f64 {
        ((360 - metadata_rotation.rem_euclid(360)) % 360) as f64
    }

    /// Where the clip is in the host's buffers, and how it's turned there.
    ///
    /// `video_size` is the frame Gyroflow stabilizes and `input_rotation` how the host turned it (the InputRotation
    /// parameter). The host fits the turned frame in the input buffer, centered: Resolve's Edit and Color pages hand the
    /// timeline, with the clip letterboxed or pillarboxed in it. The rotation can't be told from the buffer's size there (a
    /// vertical 4K clip in a 4K timeline is a 3840x2160 buffer either way), but with `clip_sized_input` the host hands the
    /// clip at its own size (Resolve's Fusion page, Adobe), so a buffer of the frame's own, unturned shape means the host
    /// didn't turn it, whatever the metadata says.
    ///
    /// With `keep_clip_frame` the stabilized image is drawn where the clip is fitted in the output buffer, at its aspect
    /// (the output size has to be the clip's then, see `GyroflowPluginBaseInstance::output_size_for`), otherwise over the
    /// whole output buffer. Hosts with top-down buffers pass `framebuffer_inverted = false` and get the rotation the kernel
    /// needs for them, which is the other way around (InputRotation is a turn of the image as seen on screen)
    pub fn render_geometry(video_size: (usize, usize), input_rotation: f64, src_size: (usize, usize), out_size: (usize, usize), clip_sized_input: bool, keep_clip_frame: bool, framebuffer_inverted: bool) -> RenderGeometry {
        let mut input_rotation = input_rotation;
        if clip_sized_input && Self::is_quarter_turn(input_rotation) && video_size.0 != video_size.1 && src_size.0 > 0 && src_size.1 > 0 {
            let ratio = |s: (usize, usize)| s.0 as f64 / s.1 as f64;
            let src = ratio(src_size);
            if (src / ratio(video_size) - 1.0).abs() < 0.01 && (src / ratio((video_size.1, video_size.0)) - 1.0).abs() > 0.01 {
                input_rotation = 0.0;
            }
        }
        let clip_size = Self::rotated_size(video_size, input_rotation);
        let clip_ratio = clip_size.0 as f64 / clip_size.1.max(1) as f64;
        RenderGeometry {
            input_rotation,
            kernel_rotation: if framebuffer_inverted { input_rotation } else { -input_rotation },
            src_rect: Self::get_center_rect(src_size.0, src_size.1, clip_ratio),
            out_rect: if keep_clip_frame { Some(Self::get_center_rect(out_size.0, out_size.1, clip_ratio)) } else { None },
        }
    }

    pub fn get_project_path(file_path: &str) -> Option<String> {
        let mut project_path = std::path::Path::new(file_path).with_extension("gyroflow");
        if !project_path.exists() {
            // Find first project path that begins with the file name
            if let Some(parent) = project_path.parent() {
                if let Ok(paths) = std::fs::read_dir(parent) {
                    if let Some(fname) = project_path.with_extension("").file_name().map(|x| x.to_string_lossy().to_string()) {
                        for path in paths {
                            if let Ok(path) = path {
                                let path_fname = path.file_name().to_string_lossy().to_string();
                                if path_fname.starts_with(&fname) && path_fname.ends_with(".gyroflow") {
                                    project_path = path.path();
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        if project_path.exists() {
            Some(project_path.to_string_lossy().to_string())
        } else {
            None
        }
    }

    pub fn get_gyroflow_location() -> Option<String> {
        match gyroflow_core::settings::try_get("exeLocation") {
            Some(serde_json::Value::String(v)) if !v.is_empty() => {
                Some(v)
            },
            _ => {
                if cfg!(target_os = "macos") && std::path::Path::new("/Applications/Gyroflow.app/Contents/MacOS/gyroflow").exists() {
                    Some("/Applications/Gyroflow.app".into())
                } else {
                    None
                }
            }
        }
    }

    pub fn get_param_definitions() -> [ParameterType; 8] {
        [
            ParameterType::HiddenString { id: "InstanceId" },
            ParameterType::HiddenString { id: "ProjectPath" },
            ParameterType::HiddenString { id: "ProjectData" },
            ParameterType::HiddenString { id: "EmbeddedLensProfile" },
            ParameterType::HiddenString { id: "EmbeddedPreset" },
            ParameterType::Group { id: "ProjectGroup", label: "Dati della clip", opened: true, parameters: vec![
                ParameterType::Text    { id: "Status",            label: "Stato",                   hint: "Status" },
                ParameterType::Text    { id: "ClipReport", label: "Dati rilevati", hint: "Informazioni lette dall’originale" },
                ParameterType::Button  { id: "LoadCurrent",       label: "Leggi dati clip",    hint: "Rileggi dall’originale telemetria, dati della camera e profilo ottico" },
                ParameterType::Button  { id: "Browse",            label: "Browse",                   hint: "Browse for the Gyroflow project file" },
                ParameterType::Button  { id: "LoadLens",          label: "Load preset/lens profile", hint: "Browse for the lens profile or a preset" },
                ParameterType::Button  { id: "OpenGyroflow",      label: "Open Gyroflow",            hint: "Open project in Gyroflow" },
                ParameterType::Button  { id: "ReloadProject",     label: "Reload project",           hint: "Reload currently loaded project" },
                ParameterType::Button  { id: "OpenRecentProject", label: "Last saved project",       hint: "Load most recently saved project in the Gyroflow app" },
            ] },
            ParameterType::Group { id: "AdjustGroup", label: "Stabilizzazione", opened: true, parameters: vec![
                ParameterType::Slider { id: "Master", label: "MASTER (%)", hint: "Anima la quantità di stabilizzazione tra 0 e 100.", min: 0.0, max: 100.0, default: 100.0 },
                ParameterType::Select   { id: "StabPreset", label: "Stile", hint: "Scegli il comportamento. Ogni stile imposta fluidità, crop, orizzonte e le regolazioni avanzate; poi puoi rifinire con i cursori qui sotto. Toccando le regolazioni avanzate lo stile diventa Personalizzato.", options: STAB_PRESET_LABELS.to_vec(), default: STAB_PRESET_LABELS[PRESET_CUSTOM as usize] },
                ParameterType::Slider   { id: "Smoothness",             label: "Fluidità", hint: "Intensità dello stile. Verso sinistra conservi la mano, verso destra attenui di più. Accetta keyframe.",                   min: 1.0,    max: 300.0, default: 50.0 },
                ParameterType::Slider   { id: "ZoomLimit",              label: "Crop massimo (zoom %)", hint: "100% corrisponde alla scala originale. 110% permette uno zoom fino a 1,10×.",                   min: 51.0,   max: 300.0, default: 130.0 },
                ParameterType::Slider   { id: "HorizonLockAmount",      label: "Blocco orizzonte (%)",         hint: "0 segue la camera. 100 punta a mantenere l’orizzonte in piano.",          min: 0.0,    max: 100.0, default: 0.0 },
                ParameterType::Group { id: "MotionFine", label: "Regolazioni avanzate", opened: false, parameters: vec![
                ParameterType::Group { id: "MotionSourceGroup", label: "Sorgente movimento", opened: false, parameters: vec![
                    ParameterType::Select { id: "MotionSource", label: "Sorgente movimento", hint: "Automatica usa il database MecenaClipDB (originali offline, proxy, ProRes RAW collegato alla clip interna, adjustment clip) e altrimenti il file della clip. Solo file legge sempre la telemetria dall’originale.", options: MOTION_SOURCE_LABELS.to_vec(), default: MOTION_SOURCE_LABELS[0] },
                    ParameterType::Slider { id: "SyncOffset", label: "Offset sincronismo (frame)", hint: "Sposta i dati movimento rispetto all’immagine, in frame della clip che li ha registrati. Si somma all’offset del collegamento per timecode. Accetta keyframe.", min: -250.0, max: 250.0, default: 0.0 },
                ] },
                ParameterType::Select   { id: "SmoothingMethod",        label: "Motore",               hint: "Percorso ottimale calcola la traiettoria più fluida su tutta la clip restando nel crop massimo: segue le panoramiche senza ritardo. Adattivo è il filtro classico di Gyroflow.", options: SMOOTHING_METHOD_LABELS.to_vec(), default: SMOOTHING_METHOD_LABELS[METHOD_ADAPTIVE as usize] },
                ParameterType::Slider   { id: "SmoothPitch",            label: "Bilanciamento tilt (%)",  hint: "Fluidità sul beccheggio (alto/basso), in percentuale della Fluidità. Il rimbalzo dei passi arriva soprattutto qui.", min: 1.0, max: 200.0, default: 100.0 },
                ParameterType::Slider   { id: "SmoothYaw",              label: "Bilanciamento pan (%)",   hint: "Fluidità sull’imbardata (destra/sinistra), in percentuale della Fluidità. Meno di 100 lascia più libere le panoramiche.", min: 1.0, max: 200.0, default: 100.0 },
                ParameterType::Slider   { id: "SmoothRoll",             label: "Bilanciamento rollio (%)", hint: "Fluidità sul rollio, in percentuale della Fluidità.", min: 1.0, max: 200.0, default: 100.0 },
                ParameterType::Slider   { id: "TimeConstant",           label: "Tempo di fluidità massimo (s)", hint: "Costante di tempo alla Fluidità 100. Più alta rende il movimento più lento e morbido.", min: 0.1, max: 10.0, default: 1.0 },
                ParameterType::Slider   { id: "FastTimeConstant",       label: "Reattività nei movimenti rapidi (s)", hint: "Solo motore Adattivo: fluidità minima durante i movimenti veloci. Valori più alti evitano scatti all’inizio delle panoramiche.", min: 0.01, max: 1.0, default: 0.1 },
                ParameterType::Slider   { id: "MotionInertia",          label: "Inerzia",              hint: "Solo Percorso ottimale: frena le variazioni di velocità, per partenze e arresti morbidi come su un crane.", min: 0.0, max: 5.0, default: 0.0 },
                ParameterType::Slider   { id: "MotionBrake",            label: "Tendenza a restare fermo", hint: "Solo Percorso ottimale: attenua anche i movimenti lenti e intenzionali. 0 segue fedelmente le panoramiche.", min: 0.0, max: 5.0, default: 0.0 },
                ParameterType::Checkbox { id: "CropConstraint",         label: "Resta nel crop massimo", hint: "Solo Percorso ottimale: riduce la fluidità solo dove servirebbe uscire dal Crop massimo.", default: true },
                ParameterType::Select   { id: "ZoomMode",               label: "Zoom adattivo",        hint: "Dinamico adatta il crop al movimento. Statico usa un unico crop per tutta la clip, senza respiro dell’inquadratura.", options: ZOOM_MODE_LABELS.to_vec(), default: ZOOM_MODE_LABELS[0] },
                ParameterType::Slider   { id: "ZoomWindow",             label: "Finestra zoom dinamico (s)", hint: "Più ampia rende più lente le variazioni del crop.", min: 0.5, max: 15.0, default: 4.0 },
                ParameterType::Slider   { id: "LensCorrectionStrength", label: "Lens correction",      hint: "Lens correction",              min: 0.0,    max: 100.0, default: 100.0 },
                ParameterType::Slider   { id: "HorizonLockRoll",        label: "Horizon roll",         hint: "Horizon lock roll adjustment", min: -100.0, max: 100.0, default: 0.0 },
                //ParameterType::Slider   { id: "PositionX",              label: "Position offset X",    hint: "Position offset X",            min: -100.0, max: 100.0, default: 0.0 },
                //ParameterType::Slider   { id: "PositionY",              label: "Position offset Y",    hint: "Position offset Y",            min: -100.0, max: 100.0, default: 0.0 },
                ParameterType::Slider   { id: "AdditionalPitch",        label: "Additional pitch",     hint: "Additional pitch rotation",    min: -180.0, max: 180.0, default: 0.0 },
                ParameterType::Slider   { id: "AdditionalYaw",          label: "Additional yaw",       hint: "Additional yaw rotation",      min: -180.0, max: 180.0, default: 0.0 },
                ParameterType::Slider   { id: "Rotation",               label: "Video rotation",       hint: "Video rotation",               min: -360.0, max: 360.0, default: 0.0 },
                ParameterType::Slider   { id: "InputRotation",          label: "Input rotation",       hint: "Input rotation",               min: -360.0, max: 360.0, default: 0.0 },
                ParameterType::Slider   { id: "Fov",                    label: "FOV",                  hint: "FOV",                          min: 0.1,    max: 3.0,   default: 1.0 },
                ParameterType::Slider   { id: "VideoSpeed",             label: "Video speed",          hint: "Use this slider to change video speed or keyframe it, instead of built-in speed changes in the editor", min: 0.0001, max: 1000.0, default: 100.0 },
                ParameterType::Checkbox { id: "DisableStretch",         label: "Disable Gyroflow's stretch", hint: "If you used Input stretch in the lens profile in Gyroflow, and you de-stretched the video separately in your editor (by setting anamorphic squeeze factor), check this to disable Gyroflow's internal stretching.", default: false },
                ParameterType::Select   { id: "IntegrationMethod",      label: "Integration method",   hint: "IMU integration method", options: vec!["None", "Complementary", "VQF", "Simple gyro", "Simple gyro + accel", "Mahony", "Madgwick"], default: "VQF" },
                ParameterType::Button   { id: "SaveDefault",            label: "Save as default",      hint: "Save the current smoothness, zoom limit, lens correction, horizon lock and FOV as the defaults for every newly loaded clip (lens_profiles/default.gyroflow)" },
                //ParameterType::Slider   { id: "FusionStartFrame",       label: "Fusion Start Frame",   hint: "Fusion Start Frame (from Project Settings)", min: 0.0, max: 100000.0, default: 0.0 },
            ParameterType::Group { id: "KeyframesGroup", label: "Fotogrammi chiave", opened: false, parameters: vec![
                ParameterType::Checkbox { id: "UseGyroflowsKeyframes", label: "Use Gyroflow's keyframes", hint: "Use internal Gyroflow's keyframes, instead of the editor ones.", default: false },
                ParameterType::Checkbox { id: "StabilizationSpeedRamp",label: "Adjust stabilization to speed", hint: "When you speed ramp the clip, let Gyroflow adjust the stabilization amount to the video speed.", default: true },
                ParameterType::Button   { id: "RecalculateKeyframes",  label: "Recalculate keyframes",         hint: "Recalculate keyframes after adjusting the splines (in Fusion mode)" },
                ParameterType::Button   { id: "CreateCamera",  label: "Create camera", hint: "Create camera layer" },
            ] },
            ParameterType::Group { id: "OutputSizeGroup", label: "Dimensioni di uscita", opened: false, parameters: vec![
                ParameterType::Slider   { id: "OutputWidth",    label: "Width",  hint: "Width",  min: 1.0, max: 16384.0, default: 3840.0 },
                ParameterType::Slider   { id: "OutputHeight",   label: "Height", hint: "Height", min: 1.0, max: 16384.0, default: 2160.0 },
                ParameterType::Button   { id: "OutputSizeToTimeline", label: "Fit to timeline", hint: "Set the output size to the timeline dimensions" },
                ParameterType::Button   { id: "OutputSizeSwap",  label: "Swap", hint: "Swap width and height" },
                ParameterType::Select   { id: "Interpolation",   label: "Interpolation", hint: "Scaling interpolation method", options: vec!["Lanczos4", "RobidouxSharp", "Bilinear", "Bicubic", "Robidoux", "Mitchell", "CatmullRom"], default: "Lanczos4" },
            ] },
            ParameterType::Checkbox { id: "ToggleOverview",     label: "Stabilization overview",         hint: "Zooms out the view to see the stabilization results. Disable this before rendering.", default: false },
            ParameterType::Checkbox { id: "DontDrawOutside",    label: "Don't draw outside source clip", hint: "When clip and timeline aspect ratio don't match, draw the final image inside the source clip, instead of drawing outside it.", default: false },
            ParameterType::Checkbox { id: "IncludeProjectData", label: "Embed .gyroflow data in plugin", hint: "If you intend to share the project to someone else, the plugin can embed the Gyroflow project data including gyro data inside the video editor project. This way you don't have to share .gyroflow project files. Enabling this option will make the project bigger.", default: false },
                ] },
            ] },
            ParameterType::Group { id: "InfoGroup", label: "Dettagli della lettura", opened: false, parameters: vec![
                ParameterType::Text { id: "LoadedProject",      label: "Loaded project",      hint: "Loaded project or video file" },
                ParameterType::Text { id: "LoadedPreset",       label: "Loaded preset",       hint: "Loaded preset" },
                ParameterType::Text { id: "LoadedLens",         label: "Loaded lens profile", hint: "Loaded lens profile" },
            ] },
        ]
    }
}

/// The "Sorgente movimento" choices, by index (saved with the projects: append only)
pub const MOTION_SOURCE_LABELS: [&str; 2] = [
    "Automatica · database, collegamenti, file",
    "Solo file della clip",
];

/// The "Stile" choices, by index (saved with the projects: append only)
pub const STAB_PRESET_LABELS: [&str; 5] = [
    "Micromosso · solo tremolio",
    "Controllato · limita il movimento",
    "Cinematografico · attenua i passi",
    "Crane / gimbal · fluido su tutti gli assi",
    "Personalizzato",
];
pub const PRESET_CUSTOM: i32 = 4;
/// The "Motore" choices, by index
pub const SMOOTHING_METHOD_LABELS: [&str; 2] = ["Percorso ottimale (più fluido)", "Adattivo (Gyroflow classico)"];
pub const METHOD_OPTIMAL: i32 = 0;
pub const METHOD_ADAPTIVE: i32 = 1;
/// The "Zoom adattivo" choices, by index
pub const ZOOM_MODE_LABELS: [&str; 2] = ["Dinamico", "Statico (nessun respiro)"];
pub const ZOOM_STATIC: i32 = 1;

/// What a "Stile" writes into the parameters. With the Optimal path the time constant per axis is
/// `time_constant · (smoothness · axis)²`, the cutoff of the jerk filter about `1 / (2π·τ)`
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct StabPresetValues {
    pub smoothness: f64,
    pub zoom_limit: f64,
    pub horizon_lock: f64,
    pub method: i32,
    /// Pitch, yaw, roll, % of the smoothness
    pub axes: [f64; 3],
    pub time_constant: f64,
    pub fast_time_constant: f64,
    pub inertia: f64,
    pub brake: f64,
    pub zoom_mode: i32,
    pub zoom_window: f64,
}
pub const STAB_PRESETS: [StabPresetValues; 4] = [
    // Micromosso: τ ≈ 0.06 s, cutoff ≈ 2.5 Hz. Vibration and the hand's tremor go, the operator's motion stays
    StabPresetValues { smoothness: 25.0, zoom_limit: 108.0, horizon_lock: 0.0,   method: METHOD_OPTIMAL, axes: [100.0, 100.0, 100.0], time_constant: 1.0, fast_time_constant: 0.1,  inertia: 0.0, brake: 0.0, zoom_mode: 0, zoom_window: 4.0 },
    // Controllato: τ ≈ 0.6 s, with a pull toward standing still that tames sudden, large moves
    StabPresetValues { smoothness: 55.0, zoom_limit: 120.0, horizon_lock: 0.0,   method: METHOD_OPTIMAL, axes: [100.0, 100.0, 100.0], time_constant: 2.0, fast_time_constant: 0.2,  inertia: 0.5, brake: 0.3, zoom_mode: 0, zoom_window: 6.0 },
    // Cinematografico: pitch and roll τ ≈ 0.9 s, the cutoff ~11× below the step rate (~2 Hz), yaw freer (τ ≈ 0.32 s) so
    // pans follow the operator and the parallax of the camera's travel stays as shot
    StabPresetValues { smoothness: 60.0, zoom_limit: 125.0, horizon_lock: 0.0,   method: METHOD_OPTIMAL, axes: [100.0, 60.0, 100.0],  time_constant: 2.5, fast_time_constant: 0.25, inertia: 0.3, brake: 0.0, zoom_mode: 0, zoom_window: 8.0 },
    // Crane / gimbal: pitch and roll τ ≈ 1.7 s, yaw ≈ 0.6 s, eased starts and stops, level horizon, one crop for the clip
    StabPresetValues { smoothness: 75.0, zoom_limit: 135.0, horizon_lock: 100.0, method: METHOD_OPTIMAL, axes: [100.0, 60.0, 100.0],  time_constant: 3.0, fast_time_constant: 0.3,  inertia: 0.5, brake: 0.0, zoom_mode: ZOOM_STATIC, zoom_window: 8.0 },
];
/// The preset new instances start with
pub const DEFAULT_PRESET: i32 = 2;
/// The advanced parameters a preset sets: editing one makes the style "Personalizzato"
pub const PRESET_ADVANCED_PARAMS: [Params; 10] = [
    Params::SmoothingMethod, Params::SmoothPitch, Params::SmoothYaw, Params::SmoothRoll, Params::TimeConstant,
    Params::FastTimeConstant, Params::MotionInertia, Params::MotionBrake, Params::ZoomMode, Params::ZoomWindow,
];
/// The parameters that change the smoothing algorithm or the zoom it's set up with (see `configure_smoothing`)
pub const SMOOTHING_PARAMS: [Params; 11] = [
    Params::SmoothingMethod, Params::SmoothPitch, Params::SmoothYaw, Params::SmoothRoll, Params::TimeConstant,
    Params::FastTimeConstant, Params::MotionInertia, Params::MotionBrake, Params::CropConstraint, Params::ZoomMode, Params::ZoomWindow,
];

/// See `GyroflowPluginBase::render_geometry`
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct RenderGeometry {
    /// How the host turned the clip, in degrees (InputRotation, or 0 when the host evidently didn't turn it)
    pub input_rotation: f64,
    /// The rotation to give the input buffer
    pub kernel_rotation: f64,
    /// Where the clip is in the input buffer
    pub src_rect: (usize, usize, usize, usize),
    /// Where the stabilized image goes in the output buffer, `None` for all of it
    pub out_rect: Option<(usize, usize, usize, usize)>,
}

pub enum ParameterType {
    HiddenString { id: &'static str },
    TextBox      { id: &'static str, label: &'static str, hint: &'static str },
    Text         { id: &'static str, label: &'static str, hint: &'static str },
    Slider       { id: &'static str, label: &'static str, hint: &'static str, min: f64, max: f64, default: f64 },
    Checkbox     { id: &'static str, label: &'static str, hint: &'static str, default: bool },
    Button       { id: &'static str, label: &'static str, hint: &'static str },
    Group        { id: &'static str, label: &'static str, opened: bool, parameters: Vec<ParameterType> },
    Select       { id: &'static str, label: &'static str, hint: &'static str, options: Vec<&'static str>, default: &'static str },
}

/// What a stabilization depends on of a video: the frames, at their rate. A copy of the file, renamed or moved, or a
/// transcode of it has the same footage, and the same project stabilizes it
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Footage {
    pub size: (usize, usize),
    pub fps: f64,
    pub duration_ms: f64,
}
impl Footage {
    fn is_known(&self) -> bool {
        self.size.0 > 0 && self.size.1 > 0 && self.fps > 0.0 && self.duration_ms > 0.0
    }
    /// Whether it's the same footage, as far as the readers of the plugin and the app agree on it: a frame as wide or as
    /// high (either way up, the other side may be scaled by the pixel aspect), the same frame rate, and the same duration
    /// to a few frames (an edit list's)
    pub fn matches(&self, other: &Footage) -> bool {
        let (a, b) = (self.size, other.size);
        let size = a.0 == b.0 || a.1 == b.1 || a.0 == b.1 || a.1 == b.0;
        size && self.same_timing(other)
    }
    /// Whether it's the same footage at any size, as a transcode of it can be: as `matches`, or of the same aspect
    pub fn matches_scaled(&self, other: &Footage) -> bool {
        let aspect = |s: (usize, usize)| s.0 as f64 / s.1.max(1) as f64;
        let (a, b) = (aspect(self.size), aspect(other.size));
        self.matches(other) || (((a / b - 1.0).abs() < 0.01 || (a * b - 1.0).abs() < 0.01) && self.same_timing(other))
    }
    fn same_timing(&self, other: &Footage) -> bool {
        let fps = (self.fps - other.fps).abs() <= self.fps.max(other.fps) * 0.005;
        let frame_ms = 1000.0 / self.fps.max(other.fps);
        let duration = (self.duration_ms - other.duration_ms).abs() <= (frame_ms * 3.0).max(self.duration_ms.max(other.duration_ms) * 0.002);
        fps && duration
    }
}

#[derive(Debug, Clone)]
pub enum TimeType {
    Frame(f64),
    Milliseconds(f64),
    Microseconds(i64),
    FrameOrMicrosecond((Option<f64>, Option<i64>))
}
pub trait GyroflowPluginParams {
    fn set_enabled(&mut self, param: Params, enabled: bool) -> PluginResult<()>;
    fn set_label(&mut self, param: Params, label: &str) -> PluginResult<()>;
    fn set_hint(&mut self, param: Params, hint: &str) -> PluginResult<()>;

    fn set_f64(&mut self, param: Params, value: f64) -> PluginResult<()>;
    fn get_f64(&self, param: Params) -> PluginResult<f64>;
    fn get_f64_at_time(&self, param: Params, time: TimeType) -> PluginResult<f64>;
    fn set_bool(&mut self, param: Params, value: bool) -> PluginResult<()>;
    fn get_bool(&self, param: Params) -> PluginResult<bool>;
    fn get_bool_at_time(&self, param: Params, time: TimeType) -> PluginResult<bool>;
    fn set_string(&mut self, param: Params, value: &str) -> PluginResult<()>;
    fn get_string(&self, param: Params) -> PluginResult<String>;
    fn set_i32(&mut self, param: Params, value: i32) -> PluginResult<()>;
    fn get_i32(&self, param: Params) -> PluginResult<i32>;

    fn is_keyframed(&self, param: Params) -> bool;
    fn get_keyframes(&self, param: Params) -> Vec<(TimeType, f64)>;
    fn clear_keyframes(&mut self, param: Params) -> PluginResult<()>;
    fn set_f64_at_time(&mut self, param: Params, time: TimeType, value: f64) -> PluginResult<()>;
}

/// The parameters with another project in Project path, for a clip from the clip database (`clipdb`): the stabilization
/// loads and is keyed by it, and the instance's own Project path stays as the user or the host set it
pub struct ProjectPathOverride<'a> {
    pub inner: &'a mut dyn GyroflowPluginParams,
    pub path: &'a str,
}
impl GyroflowPluginParams for ProjectPathOverride<'_> {
    fn set_enabled(&mut self, p: Params, e: bool) -> PluginResult<()> { self.inner.set_enabled(p, e) }
    fn set_label(&mut self, p: Params, l: &str) -> PluginResult<()> { self.inner.set_label(p, l) }
    fn set_hint(&mut self, p: Params, h: &str) -> PluginResult<()> { self.inner.set_hint(p, h) }
    fn set_f64(&mut self, p: Params, v: f64) -> PluginResult<()> { self.inner.set_f64(p, v) }
    fn get_f64(&self, p: Params) -> PluginResult<f64> { self.inner.get_f64(p) }
    fn get_f64_at_time(&self, p: Params, t: TimeType) -> PluginResult<f64> { self.inner.get_f64_at_time(p, t) }
    fn set_bool(&mut self, p: Params, v: bool) -> PluginResult<()> { self.inner.set_bool(p, v) }
    fn get_bool(&self, p: Params) -> PluginResult<bool> { self.inner.get_bool(p) }
    fn get_bool_at_time(&self, p: Params, t: TimeType) -> PluginResult<bool> { self.inner.get_bool_at_time(p, t) }
    fn set_string(&mut self, p: Params, v: &str) -> PluginResult<()> {
        if p == Params::ProjectPath { Ok(()) } else { self.inner.set_string(p, v) }
    }
    fn get_string(&self, p: Params) -> PluginResult<String> {
        if p == Params::ProjectPath { Ok(self.path.to_string()) } else { self.inner.get_string(p) }
    }
    fn set_i32(&mut self, p: Params, v: i32) -> PluginResult<()> { self.inner.set_i32(p, v) }
    fn get_i32(&self, p: Params) -> PluginResult<i32> { self.inner.get_i32(p) }
    fn is_keyframed(&self, p: Params) -> bool { self.inner.is_keyframed(p) }
    fn get_keyframes(&self, p: Params) -> Vec<(TimeType, f64)> { self.inner.get_keyframes(p) }
    fn clear_keyframes(&mut self, p: Params) -> PluginResult<()> { self.inner.clear_keyframes(p) }
    fn set_f64_at_time(&mut self, p: Params, t: TimeType, v: f64) -> PluginResult<()> { self.inner.set_f64_at_time(p, t, v) }
}

#[derive(Default, Clone)]
pub struct KeyframableParams {
    pub use_gyroflows_keyframes: bool,
    pub cached_keyframes: KeyframeManager
}
impl KeyframableParams {
    /// What a stabilization reads of them, to tell whether instances agree on it
    fn checksum(&self) -> (bool, u64) {
        let types = GyroflowPluginBaseInstance::KEYFRAMABLE_PARAMS.map(|x| x.0);
        (self.use_gyroflows_keyframes, self.cached_keyframes.get_checksum_for(&types))
    }
}

/// The keyframable parameters each stabilization reads its keyframes from (see `set_keyframe_provider`), by the
/// stabilization's keyframes, which only the stabilization holds. The `Weak` keeps their address from being reused
static KEYFRAME_PROVIDERS: std::sync::LazyLock<Mutex<Vec<(Weak<RwLock<KeyframeManager>>, Arc<RwLock<KeyframableParams>>)>>> = std::sync::LazyLock::new(Default::default);

/// The keyframable parameters `stab` reads its keyframes from, as set up by `set_keyframe_provider`
fn keyframe_provider(stab: &StabilizationManager) -> Option<Arc<RwLock<KeyframableParams>>> {
    let providers = KEYFRAME_PROVIDERS.lock();
    providers.iter().find(|(x, _)| x.strong_count() > 0 && std::ptr::eq(x.as_ptr(), Arc::as_ptr(&stab.keyframes))).map(|x| x.1.clone())
}

/// The Adobe plugin saves what it needs of it with the project in a layout of its own (see `adobe/src/sequence_data.rs`),
/// so fields can be added here freely
pub struct GyroflowPluginBaseInstance {
    pub keyframable_params: Arc<RwLock<KeyframableParams>>,

    pub managers: LruCache<String, Arc<StabilizationManager>>,

    pub reload_values_from_project: bool,
    /// The adjustments (smoothness, FOV...) came with the effect, copied from another clip or applied as a preset, and
    /// the project loaded next mustn't replace them. Only what belongs to the clip is loaded from it then
    pub keep_adjustments: bool,

    pub original_video_size: (usize, usize),
    pub original_output_size: (usize, usize),
    pub timeline_size: (usize, usize),
    pub num_frames: usize,
    pub fps: f64,
    pub has_motion: bool,
    /// The instance id, and so the stabilization (see `stab_key`), is the instance's alone. Otherwise it may share them
    /// with the instances copied with it (pasted, duplicated, the pieces of a cut clip): the stabilization is set up for
    /// all of them, and the instance mustn't change it for itself. It gets one of its own first (see `detach`)
    pub ever_changed: bool,
    pub cache_keyframes_every_frame: bool,
    pub framebuffer_inverted: bool,
    pub anamorphic_adjust_size: bool,
    /// The host's output buffer has a size of its own, which the plugin can't change (Resolve's Edit and Color pages hand the timeline)
    pub fixed_output_size: bool,
    /// The output buffer's size at the last render, to fit the output size to again when parameters change
    pub last_out_size: (usize, usize),

    pub opencl_disabled: bool,
}
impl Clone for GyroflowPluginBaseInstance {
    fn clone(&self) -> Self {
        Self {
            managers:                       self.managers.clone(),
            original_output_size:           self.original_output_size,
            original_video_size:            self.original_video_size,
            timeline_size:                  self.timeline_size,
            num_frames:                     self.num_frames,
            fps:                            self.fps,
            has_motion:                     self.has_motion,
            reload_values_from_project:     self.reload_values_from_project,
            keep_adjustments:               self.keep_adjustments,
            ever_changed:                   self.ever_changed,
            opencl_disabled:                self.opencl_disabled,
            cache_keyframes_every_frame:    self.cache_keyframes_every_frame,
            framebuffer_inverted:           self.framebuffer_inverted,
            anamorphic_adjust_size:         self.anamorphic_adjust_size,
            fixed_output_size:              self.fixed_output_size,
            last_out_size:                  self.last_out_size,
            keyframable_params:             Arc::new(RwLock::new(self.keyframable_params.read().clone())),
        }
    }
}
impl Default for GyroflowPluginBaseInstance {
    fn default() -> Self {
        Self {
            managers:                       LruCache::new(std::num::NonZeroUsize::new(20).unwrap()),
            original_output_size:           (0, 0),
            original_video_size:            (0, 0),
            timeline_size:                  (0, 0),
            num_frames:                     0,
            fps:                            0.0,
            has_motion:                     false,
            reload_values_from_project:     true,
            keep_adjustments:               false,
            ever_changed:                   false,
            opencl_disabled:                false,
            cache_keyframes_every_frame:    true,
            framebuffer_inverted:           false,
            anamorphic_adjust_size:         true,
            fixed_output_size:              false,
            last_out_size:                  (0, 0),
            keyframable_params: Arc::new(RwLock::new(KeyframableParams {
                use_gyroflows_keyframes:  false, // TODO param_set.parameter::<Bool>("UseGyroflowsKeyframes")?.get_value()?,
                cached_keyframes:         KeyframeManager::default()
            })),
        }
    }
}

impl GyroflowPluginBaseInstance {
    pub fn update_loaded_state(&mut self, params: &mut dyn GyroflowPluginParams, loaded: bool) {
        let _ = params.set_enabled(Params::Fov, loaded);
        let _ = params.set_enabled(Params::Smoothness, loaded);
        let _ = params.set_enabled(Params::ZoomLimit, loaded);
        let _ = params.set_enabled(Params::LensCorrectionStrength, loaded);
        let _ = params.set_enabled(Params::HorizonLockAmount, loaded);
        let _ = params.set_enabled(Params::HorizonLockRoll, loaded);
        for p in SMOOTHING_PARAMS { let _ = params.set_enabled(p, loaded); }
        //let _ = params.set_enabled(Params::PositionX, loaded);
        //let _ = params.set_enabled(Params::PositionY, loaded);
        let _ = params.set_enabled(Params::AdditionalPitch, loaded);
        let _ = params.set_enabled(Params::AdditionalYaw, loaded);
        let _ = params.set_enabled(Params::Rotation, loaded);
        let _ = params.set_enabled(Params::VideoSpeed, loaded);
        let _ = params.set_enabled(Params::DisableStretch, loaded);
        let _ = params.set_enabled(Params::IntegrationMethod, loaded);
        let _ = params.set_enabled(Params::ToggleOverview, loaded);
        let _ = params.set_enabled(Params::ReloadProject, loaded);
        let _ = params.set_enabled(Params::OutputWidth, loaded);
        let _ = params.set_enabled(Params::OutputHeight, loaded);
        let _ = params.set_enabled(Params::OutputSizeToTimeline, loaded);
        let _ = params.set_enabled(Params::OutputSizeSwap, loaded);
        let _ = params.set_string(Params::Status, if loaded { "OK" } else { "Project not loaded" });
        let _ = params.set_label(Params::OpenGyroflow, if loaded { "Open in Gyroflow" } else { "Open Gyroflow" });
    }

    /// Whether the stabilized image is drawn where the clip is, at its aspect (see `GyroflowPluginBase::render_geometry`):
    /// when asked to with "Don't draw outside source clip", and for a vertical clip in a horizontal timeline the plugin
    /// can't resize. Filling the timeline is what a 4:3 clip in a 16:9 timeline or a horizontal clip in a vertical one is
    /// after, but out of a vertical clip in a horizontal timeline it would keep a third of its height, so it stays pillarboxed
    pub fn keep_clip_frame(&self, params: &dyn GyroflowPluginParams, video_size: (usize, usize), input_rotation: f64, out_size: (usize, usize)) -> bool {
        if params.get_bool(Params::DontDrawOutside).unwrap_or_default() {
            return true;
        }
        let clip = GyroflowPluginBase::rotated_size(video_size, input_rotation);
        self.fixed_output_size && clip.1 > clip.0 && out_size.0 > out_size.1
    }

    /// The output size to stabilize to: the clip's own frame when the image is drawn where the clip is, the host's buffer
    /// when it has a size of its own, and the plugin's output size otherwise
    pub fn output_size_for(&self, params: &dyn GyroflowPluginParams, video_size: (usize, usize), input_rotation: f64, out_size: (usize, usize)) -> (usize, usize) {
        if self.keep_clip_frame(params, video_size, input_rotation, out_size) {
            GyroflowPluginBase::rotated_size(video_size, input_rotation)
        } else if self.fixed_output_size && out_size.0 > 0 && out_size.1 > 0 {
            out_size
        } else {
            (params.get_f64(Params::OutputWidth).unwrap_or_default() as usize, params.get_f64(Params::OutputHeight).unwrap_or_default() as usize)
        }
    }

    /// Makes the output size's aspect `size`'s. The size itself doesn't matter, the output is drawn into whatever rect it
    /// gets, but a different aspect would stretch it. Each change of the output size starts the stabilization over, and
    /// hosts render thumbnails through the plugin at sizes of their own (Resolve: 504x284 for the clip, about the
    /// timeline's aspect, and 161x80 for the node, an aspect of its own), so small ones and near misses are let through
    pub fn ensure_output_aspect(stab: &StabilizationManager, size: (usize, usize)) {
        if size.0.max(size.1) < 320 || size.0 == 0 || size.1 == 0 { return; }
        let current = stab.params.read().output_size;
        let aspect = |s: (usize, usize)| s.0 as f64 / s.1.max(1) as f64;
        if current.0 == 0 || current.1 == 0 || (aspect(current) / aspect(size) - 1.0).abs() > 0.01 {
            stab.set_output_size(size.0, size.1);
        }
    }

    /// Applies the Rotation and InputRotation parameters to the stabilization: the video rotation, and the output size,
    /// which is fitted to the video turned by it and, drawn where the clip is, has the clip's aspect
    pub fn apply_rotation(&self, params: &dyn GyroflowPluginParams, stab: &StabilizationManager, out_size: (usize, usize)) {
        if let Ok(rotation) = params.get_f64(Params::Rotation) {
            stab.params.write().video_rotation = rotation;
        }
        let video_size = stab.params.read().size;
        let input_rotation = params.get_f64(Params::InputRotation).unwrap_or_default();
        let (w, h) = self.output_size_for(params, video_size, input_rotation, out_size);
        if w > 0 && h > 0 {
            stab.set_output_size(w, h);
        }
    }

    /// Applies the lens profile and the preset loaded with "Load preset/lens profile". Over a project, only the ones loaded
    /// over one: one loaded over the video was before the project was made in the app (see
    /// `GyroflowPluginBase::mark_loaded_over_project`)
    fn apply_embedded_presets(params: &dyn GyroflowPluginParams, stab: &StabilizationManager, over_project: bool) {
        let applies = |d: &str| !d.is_empty() && (!over_project || GyroflowPluginBase::loaded_over_project(d));
        if let Ok(d) = params.get_string(Params::EmbeddedLensProfile) {
            if applies(&d) {
                if let Err(e) = stab.load_lens_profile(&d) {
                    rfd::MessageDialog::new()
                        .set_description(&format!("Failed to load lens profile: {e:?}"))
                        .show();
                }
            }
        }
        if let Ok(mut d) = params.get_string(Params::EmbeddedPreset) {
            if !applies(&d) {
                return;
            }
            // Earlier versions embedded projects whole. One of this clip still loads whole, of another its settings do
            if let Some(video) = GyroflowPluginBase::project_video_file(&d) {
                let url = |x: &str| if x.contains("://") { x.to_owned() } else { filesystem::path_to_url(x) };
                let current_video = stab.input_file.read().url.clone();
                if !filesystem::get_filename(&url(&video)).eq_ignore_ascii_case(&filesystem::get_filename(&current_video)) {
                    d = GyroflowPluginBase::project_to_preset(&d, "Project").unwrap_or_default();
                }
            }
            if !d.is_empty() {
                let mut is_preset = false;
                if let Err(e) = stab.import_gyroflow_data(d.as_bytes(), true, None, |_|(), Arc::new(AtomicBool::new(false)), &mut is_preset, true) {
                    rfd::MessageDialog::new()
                        .set_description(&format!("Failed to load preset: {e:?}"))
                        .show();
                }
            }
        }
    }

    pub fn initialize_instance_id(&mut self, instance_id: &mut String) {
        if instance_id.is_empty() {
            self.ever_changed = true;
            *instance_id = format!("{}", fastrand::u64(..));
        }
    }

    /// A parameter's value, or `default` where it has none a slider can give (a host or a test that doesn't have it)
    fn positive(params: &dyn GyroflowPluginParams, param: Params, time: Option<&TimeType>, default: f64) -> f64 {
        let v = match time { Some(t) => params.get_f64_at_time(param, t.clone()), None => params.get_f64(param) };
        v.ok().filter(|v| v.is_finite() && *v > 0.0).unwrap_or(default)
    }
    /// The smoothness of each axis (pitch, yaw, roll) in Gyroflow's scale: the smoothness times the axis' share
    fn axis_smoothness(params: &dyn GyroflowPluginParams, time: Option<&TimeType>) -> [f64; 3] {
        let s = Self::positive(params, Params::Smoothness, time, 50.0) / 100.0;
        [Params::SmoothPitch, Params::SmoothYaw, Params::SmoothRoll].map(|p| s * Self::positive(params, p, time, 100.0) / 100.0)
    }
    fn per_axis(params: &dyn GyroflowPluginParams) -> bool {
        [Params::SmoothPitch, Params::SmoothYaw, Params::SmoothRoll].iter().any(|p| (Self::positive(params, *p, None, 100.0) - 100.0).abs() > 1e-6)
    }
    /// Gyroflow's adaptive zoom window for the parameters: -1 for a static zoom, the window in seconds otherwise
    fn zoom_window(params: &dyn GyroflowPluginParams) -> f64 {
        if params.get_i32(Params::ZoomMode).ok() == Some(ZOOM_STATIC) { -1.0 } else { Self::positive(params, Params::ZoomWindow, None, 4.0) }
    }

    /// Sets up `smoothing` for the parameters: the algorithm, and what of it isn't keyframable. The smoothness and the
    /// per axis smoothness the algorithms read from the keyframes too (see `cache_keyframes`)
    pub fn configure_smoothing(params: &dyn GyroflowPluginParams, smoothing: &mut gyroflow_core::smoothing::Smoothing) {
        let optimal = params.get_i32(Params::SmoothingMethod).unwrap_or(METHOD_ADAPTIVE) == METHOD_OPTIMAL;
        let s = Self::positive(params, Params::Smoothness, None, 50.0) / 100.0;
        let axes = Self::axis_smoothness(params, None);
        let time_constant = Self::positive(params, Params::TimeConstant, None, if optimal { 2.0 } else { 1.0 });
        smoothing.set_current(if optimal { 2 } else { 1 });
        let alg = smoothing.current_mut();
        alg.set_parameter("smoothness", s);
        alg.set_parameter("smoothness_pitch", axes[0]);
        alg.set_parameter("smoothness_yaw", axes[1]);
        alg.set_parameter("smoothness_roll", axes[2]);
        alg.set_parameter("per_axis", if Self::per_axis(params) { 1.0 } else { 0.0 });
        if optimal {
            let weight = |p: Params| params.get_f64(p).ok().filter(|v| v.is_finite()).unwrap_or(0.0).max(0.0);
            alg.set_parameter("max_time_constant", time_constant);
            alg.set_parameter("crop_constraint", if params.get_bool(Params::CropConstraint).unwrap_or(true) { 1.0 } else { 0.0 });
            alg.set_parameter("order_1", weight(Params::MotionBrake));
            alg.set_parameter("order_2", weight(Params::MotionInertia));
            alg.set_parameter("order_3", 1.0);
        } else {
            alg.set_parameter("max_smoothness", time_constant);
            alg.set_parameter("alpha_0_1s", Self::positive(params, Params::FastTimeConstant, None, 0.1));
        }
    }
    /// Applies the smoothing algorithm and the adaptive zoom of the parameters to `stab`. The caller invalidates
    pub fn apply_smoothing(params: &dyn GyroflowPluginParams, stab: &StabilizationManager) {
        Self::configure_smoothing(params, &mut stab.smoothing.write());
        let window = Self::zoom_window(params);
        if stab.params.read().adaptive_zoom_window != window {
            stab.set_adaptive_zoom(window);
        }
    }
    /// Whether `stab` smooths and zooms as `apply_smoothing` would set it up
    fn smoothing_agrees(params: &dyn GyroflowPluginParams, stab: &StabilizationManager) -> bool {
        let current = stab.smoothing.read();
        let mut expected = current.clone();
        Self::configure_smoothing(params, &mut expected);
        expected.current().get_name() == current.current().get_name()
            && expected.current().get_checksum() == current.current().get_checksum()
            && stab.params.read().adaptive_zoom_window == Self::zoom_window(params)
    }
    /// Reads the parameters of the "Personalizzato" style back from a loaded project or the defaults of a video
    fn read_smoothing(params: &mut dyn GyroflowPluginParams, smoothing: &gyroflow_core::smoothing::Smoothing, zoom_window: f64) -> PluginResult<()> {
        let alg = smoothing.current();
        let optimal = alg.get_name() == "Optimal path";
        params.set_i32(Params::SmoothingMethod, if optimal { METHOD_OPTIMAL } else { METHOD_ADAPTIVE })?;
        let s = alg.get_parameter("smoothness");
        let per_axis = alg.get_parameter("per_axis") > 0.1 && s > 0.0;
        for (p, name) in [(Params::SmoothPitch, "smoothness_pitch"), (Params::SmoothYaw, "smoothness_yaw"), (Params::SmoothRoll, "smoothness_roll")] {
            params.set_f64(p, if per_axis { (alg.get_parameter(name) / s * 100.0).clamp(1.0, 200.0) } else { 100.0 })?;
        }
        let set_positive = |params: &mut dyn GyroflowPluginParams, p: Params, v: f64| -> PluginResult<()> { if v.is_finite() && v > 0.0 { params.set_f64(p, v)?; } Ok(()) };
        if optimal {
            set_positive(params, Params::TimeConstant, alg.get_parameter("max_time_constant"))?;
            params.set_f64(Params::MotionBrake, alg.get_parameter("order_1").max(0.0))?;
            params.set_f64(Params::MotionInertia, alg.get_parameter("order_2").max(0.0))?;
            params.set_bool(Params::CropConstraint, alg.get_parameter("crop_constraint") > 0.1)?;
        } else {
            set_positive(params, Params::TimeConstant, alg.get_parameter("max_smoothness"))?;
            set_positive(params, Params::FastTimeConstant, alg.get_parameter("alpha_0_1s"))?;
        }
        if zoom_window < -0.9 {
            params.set_i32(Params::ZoomMode, ZOOM_STATIC)?;
        } else {
            params.set_i32(Params::ZoomMode, 0)?;
            set_positive(params, Params::ZoomWindow, zoom_window)?;
        }
        Ok(())
    }
    /// Whether `param` has the value `preset` gives it
    fn preset_has(params: &dyn GyroflowPluginParams, preset: &StabPresetValues, param: Params) -> bool {
        let f = |v: f64| params.get_f64(param).is_ok_and(|x| (x - v).abs() < 1e-6);
        match param {
            Params::SmoothingMethod  => params.get_i32(param).ok() == Some(preset.method),
            Params::ZoomMode         => params.get_i32(param).ok() == Some(preset.zoom_mode),
            Params::SmoothPitch      => f(preset.axes[0]),
            Params::SmoothYaw        => f(preset.axes[1]),
            Params::SmoothRoll       => f(preset.axes[2]),
            Params::TimeConstant     => f(preset.time_constant),
            Params::FastTimeConstant => f(preset.fast_time_constant),
            Params::MotionInertia    => f(preset.inertia),
            Params::MotionBrake      => f(preset.brake),
            Params::ZoomWindow       => f(preset.zoom_window),
            _ => true,
        }
    }
    /// Writes a style's values into the parameters, but for the keyframed ones, which keep their animation
    pub fn write_preset(params: &mut dyn GyroflowPluginParams, preset: &StabPresetValues) -> PluginResult<()> {
        let f64s = [
            (Params::Smoothness, preset.smoothness), (Params::ZoomLimit, preset.zoom_limit), (Params::HorizonLockAmount, preset.horizon_lock),
            (Params::SmoothPitch, preset.axes[0]), (Params::SmoothYaw, preset.axes[1]), (Params::SmoothRoll, preset.axes[2]),
            (Params::TimeConstant, preset.time_constant), (Params::FastTimeConstant, preset.fast_time_constant),
            (Params::MotionInertia, preset.inertia), (Params::MotionBrake, preset.brake), (Params::ZoomWindow, preset.zoom_window),
        ];
        for (p, v) in f64s {
            if !params.is_keyframed(p) { params.set_f64(p, v)?; }
        }
        params.set_i32(Params::SmoothingMethod, preset.method)?;
        params.set_i32(Params::ZoomMode, preset.zoom_mode)?;
        params.set_bool(Params::CropConstraint, true)?;
        Ok(())
    }

    /// Makes `stab` read the keyframable parameters from this instance's cache (see `cache_keyframes`). Only for a
    /// stabilization the instance has to itself, or sets up for the instances that share it (see `ever_changed`)
    pub fn set_keyframe_provider(&self, stab: &StabilizationManager) {
        let kparams = self.keyframable_params.clone();
        {
            let mut providers = KEYFRAME_PROVIDERS.lock();
            providers.retain(|(x, _)| x.strong_count() > 0 && !std::ptr::eq(x.as_ptr(), Arc::as_ptr(&stab.keyframes)));
            providers.push((Arc::downgrade(&stab.keyframes), kparams.clone()));
        }
        stab.keyframes.write().set_custom_provider(move |kf, typ, timestamp_ms| -> Option<f64> {
            let params = kparams.read();
            if params.use_gyroflows_keyframes && kf.is_keyframed_internally(typ) { return None; }
            params.cached_keyframes.value_at_video_timestamp(typ, timestamp_ms)
        });
    }
    /// The parameters whose values over time `cache_keyframes` caches for the stabilization: the keyframe type of each,
    /// and the scale from the parameter's values to Gyroflow's
    pub const KEYFRAMABLE_PARAMS: [(KeyframeType, Params, f64); 10] = [
        (KeyframeType::Fov,                       Params::Fov,                    1.0),
        (KeyframeType::MaxZoom,                   Params::ZoomLimit,              1.0),
        (KeyframeType::SmoothingParamSmoothness,  Params::Smoothness,             100.0),
        (KeyframeType::LensCorrectionStrength,    Params::LensCorrectionStrength, 100.0),
        (KeyframeType::LockHorizonAmount,         Params::HorizonLockAmount,      1.0),
        (KeyframeType::LockHorizonRoll,           Params::HorizonLockRoll,        1.0),
        (KeyframeType::VideoSpeed,                Params::VideoSpeed,             100.0),
        (KeyframeType::VideoRotation,             Params::Rotation,               1.0),
        //(KeyframeType::ZoomingCenterX,            Params::PositionX,              100.0),
        //(KeyframeType::ZoomingCenterY,            Params::PositionY,              100.0),
        (KeyframeType::AdditionalRotationX,       Params::AdditionalYaw,          1.0),
        (KeyframeType::AdditionalRotationY,       Params::AdditionalPitch,        1.0),
    ];
    pub fn cache_keyframes(&mut self, params: &dyn GyroflowPluginParams, use_gyroflows_keyframes: bool, num_frames: usize, fps: f64) {
        let mut mgr = KeyframeManager::new();
        // Vertical clips get their quarter-turn rotation both as the input and the video rotation, and come out upside
        // down when the horizon is locked: lock it 180° around in that case
        let horizon_roll_offset = {
            let input_rot = params.get_f64(Params::InputRotation).unwrap_or(0.0).rem_euclid(360.0);
            let video_rot = params.get_f64(Params::Rotation).unwrap_or(0.0).rem_euclid(360.0);
            if GyroflowPluginBase::is_quarter_turn(input_rot) && (video_rot - input_rot).abs() < 1.0 { 180.0 } else { 0.0 }
        };
        for (typ, param, scale) in Self::KEYFRAMABLE_PARAMS {
            let offset = if typ == KeyframeType::LockHorizonRoll { horizon_roll_offset } else { 0.0 };
            if params.is_keyframed(param) {
                log::info!("param: {:?} is keyframed, cache_keyframes_every_frame: {}", param, self.cache_keyframes_every_frame);
                if self.cache_keyframes_every_frame { // Query every frame
                    for t in 0..num_frames {
                        let time = t as f64;
                        let timestamp_us = ((time / fps * 1_000_000.0)).round() as i64;

                        if let Ok(v) = params.get_f64_at_time(param, TimeType::FrameOrMicrosecond((Some(time), Some(timestamp_us)))) {
                            mgr.set(&typ, timestamp_us, v / scale + offset);
                        }
                    }
                } else {
                    // Cache only the keyframes at their timestamps
                    for (t, v) in params.get_keyframes(param) {
                        let timestamp_us = match t {
                            TimeType::FrameOrMicrosecond((Some(f), None)) |
                            TimeType::Frame(f) => ((f / fps * 1_000_000.0)).round() as i64,
                            TimeType::Milliseconds(ms) => (ms * 1_000.0).round() as i64,
                            TimeType::Microseconds(us) => us,
                            TimeType::FrameOrMicrosecond((_,    Some(timestamp_us))) => timestamp_us,
                            TimeType::FrameOrMicrosecond((None, None)) => unreachable!(),
                        };

                        mgr.set(&typ, timestamp_us, v / scale + offset);
                    }
                }
            } else {
                log::info!("param: {:?} NOT keyframed", param);
                if let Ok(v) = params.get_f64(param) {
                    mgr.set(&typ, 0, v / scale + offset);
                }
            }
        }

        // The smoothness per axis, the smoothness times each axis' share: animated when either is
        let animated = params.is_keyframed(Params::Smoothness);
        for (i, (typ, param)) in [(KeyframeType::SmoothingParamPitch, Params::SmoothPitch), (KeyframeType::SmoothingParamYaw, Params::SmoothYaw), (KeyframeType::SmoothingParamRoll, Params::SmoothRoll)].into_iter().enumerate() {
            if animated || params.is_keyframed(param) {
                for t in 0..num_frames {
                    let time = t as f64;
                    let timestamp_us = ((time / fps * 1_000_000.0)).round() as i64;
                    let axes = Self::axis_smoothness(params, Some(&TimeType::FrameOrMicrosecond((Some(time), Some(timestamp_us)))));
                    mgr.set(&typ, timestamp_us, axes[i]);
                }
            } else {
                mgr.set(&typ, 0, Self::axis_smoothness(params, None)[i]);
            }
        }

        let mut kparams = self.keyframable_params.write();
        kparams.use_gyroflows_keyframes = use_gyroflows_keyframes;
        kparams.cached_keyframes = mgr;
    }
    /// Caches the keyframable parameters again after the values of the `changed` ones changed in a way the host didn't
    /// report as an edit (After Effects doesn't for keyframes removed with the stopwatch, moved or eased in the timeline,
    /// or an undo), and recomputes what depends on them. A shared stabilization isn't for these values anymore, the
    /// instance gets one of its own, set up with them
    pub fn keyframable_params_changed(&mut self, params: &mut dyn GyroflowPluginParams, manager_cache: &Mutex<LruCache<String, Arc<StabilizationManager>>>, changed: &[Params]) -> PluginResult<()> {
        let use_gyroflows_keyframes = params.get_bool(Params::UseGyroflowsKeyframes).unwrap_or_default();
        if self.ever_changed {
            self.cache_keyframes(params, use_gyroflows_keyframes, self.num_frames, self.fps.max(1.0));
        } else {
            self.detach(params, manager_cache)?;
        }
        for param in changed {
            self.recompute_for(params, *param, use_gyroflows_keyframes);
        }
        Ok(())
    }
    /// Recomputes what depends on `param` in the loaded stabilizations, after its value changed (and was cached again,
    /// for a keyframable one)
    fn recompute_for(&self, params: &dyn GyroflowPluginParams, param: Params, use_gyroflows_keyframes: bool) {
        let managers = self.managers.iter().map(|(_, v)| v.clone()).collect::<Vec<_>>();
        for v in &managers {
            match param {
                Params::IntegrationMethod => {
                    if let Ok(im) = params.get_i32(Params::IntegrationMethod) {
                        let mut gyro = v.gyro.write();
                        gyro.integration_method = im as usize;
                        gyro.apply_transforms();
                    }
                    v.invalidate_blocking_smoothing();
                    v.invalidate_blocking_zooming();
                }
                Params::Smoothness | Params::ZoomLimit | Params::HorizonLockAmount | Params::HorizonLockRoll |
                Params::AdditionalPitch | Params::AdditionalYaw | Params::RecalculateKeyframes | Params::StabPreset |
                Params::SmoothingMethod | Params::SmoothPitch | Params::SmoothYaw | Params::SmoothRoll | Params::TimeConstant |
                Params::FastTimeConstant | Params::MotionInertia | Params::MotionBrake | Params::CropConstraint |
                Params::ZoomMode | Params::ZoomWindow => {
                    Self::apply_smoothing(params, v);
                    v.invalidate_blocking_smoothing();
                    v.invalidate_blocking_zooming();
                },
                //Params::PositionX | Params::PositionY |
                Params::LensCorrectionStrength => {
                    v.invalidate_blocking_zooming();
                },
                Params::Rotation | Params::InputRotation => {
                    self.apply_rotation(params, v, self.last_out_size);
                    v.invalidate_blocking_zooming();
                },
                _ => { }
            }
            v.invalidate_blocking_undistortion();
            match param {
                Params::VideoSpeed | Params::UseGyroflowsKeyframes | Params::RecalculateKeyframes => {
                    let inverse = !(use_gyroflows_keyframes && v.keyframes.read().is_keyframed_internally(&KeyframeType::VideoSpeed));
                    v.params.write().calculate_ramped_timestamps(&v.keyframes.read(), inverse, inverse);
                },
                _ => { }
            }
        }
    }

    /// The key of the stabilization the instance renders with, in `managers` and the manager cache: of the project, loaded
    /// with Disable stretch or not, for the instance id. The instances that share it share the instance id (see `ever_changed`)
    pub fn stab_key(params: &dyn GyroflowPluginParams) -> PluginResult<String> {
        Ok(format!("{}{}{}", params.get_string(Params::ProjectPath)?, params.get_bool(Params::DisableStretch)?, params.get_string(Params::InstanceId)?))
    }

    /// Whether an edit of `param` changes what the instance sets up its stabilization with (see `configure`), which it
    /// may only do to one of its own
    pub fn changes_stabilization(param: Params) -> bool {
        matches!(param,
            Params::Fov | Params::Smoothness | Params::ZoomLimit | Params::LensCorrectionStrength |
            Params::HorizonLockAmount | Params::HorizonLockRoll | Params::AdditionalPitch | Params::AdditionalYaw |
            Params::Rotation | Params::InputRotation | Params::VideoSpeed | Params::IntegrationMethod |
            Params::UseGyroflowsKeyframes | Params::RecalculateKeyframes |
            Params::StabPreset | Params::SmoothingMethod | Params::SmoothPitch | Params::SmoothYaw | Params::SmoothRoll |
            Params::TimeConstant | Params::FastTimeConstant | Params::MotionInertia | Params::MotionBrake |
            Params::CropConstraint | Params::ZoomMode | Params::ZoomWindow |
            Params::OutputWidth | Params::OutputHeight | Params::OutputSizeSwap | Params::OutputSizeToTimeline |
            Params::ToggleOverview | Params::DontDrawOutside | Params::Interpolation)
    }

    fn interpolation(params: &dyn GyroflowPluginParams) -> gyroflow_core::stabilization::Interpolation {
        match params.get_i32(Params::Interpolation) {
            Ok(1) => gyroflow_core::stabilization::Interpolation::RobidouxSharp,
            Ok(2) => gyroflow_core::stabilization::Interpolation::Bilinear,
            Ok(3) => gyroflow_core::stabilization::Interpolation::Bicubic,
            Ok(4) => gyroflow_core::stabilization::Interpolation::Robidoux,
            Ok(5) => gyroflow_core::stabilization::Interpolation::Mitchell,
            Ok(6) => gyroflow_core::stabilization::Interpolation::CatmullRom,
            _     => gyroflow_core::stabilization::Interpolation::Lanczos4,
        }
    }
    /// Sets up the renderer of a new stabilization, loaded or forked
    fn setup_renderer(params: &dyn GyroflowPluginParams, stab: &StabilizationManager) {
        let mut stab = stab.stabilization.write();
        stab.share_wgpu_instances = true;
        stab.interpolation = Self::interpolation(params);
        log::info!("Interpolation: {:?}", stab.interpolation);
    }

    /// Sets up `stab` for the instance with what it applies of its parameters: the overview, the output size and the
    /// rotation, the keyframable parameters and the integration method. For a stabilization loaded or forked for it, or
    /// one of its own it set up for values it doesn't have anymore (restored from before an undo or a revert).
    /// `recompute` computes the stabilization right away, otherwise its first render does
    fn configure(&mut self, params: &mut dyn GyroflowPluginParams, stab: &StabilizationManager, out_size: (usize, usize), recompute: bool) -> PluginResult<()> {
        {
            let gf_params = stab.params.read();
            self.num_frames = gf_params.frame_count;
            self.fps = gf_params.fps;
        }
        stab.set_fov_overview(params.get_bool(Params::ToggleOverview)?);
        stab.params.write().framebuffer_inverted = self.framebuffer_inverted;

        stab.init_size();
        // The video rotation from the parameter, which the host restores with the project, and the output size fitted to
        // it, for the output buffer of the last render that wasn't a thumbnail
        self.apply_rotation(params, stab, if self.last_out_size != (0, 0) { self.last_out_size } else { out_size });

        let use_gyroflows_keyframes = params.get_bool(Params::UseGyroflowsKeyframes).unwrap_or_default();
        if !self.ever_changed {
            // Cached anew rather than in place: a stabilization the instance shares may read the ones it has (see
            // `set_keyframe_provider`), and has to keep them (another project picked, loaded under another key)
            self.keyframable_params = Arc::new(RwLock::new(KeyframableParams::default()));
        }
        self.cache_keyframes(params, use_gyroflows_keyframes, self.num_frames, self.fps.max(1.0));
        self.set_keyframe_provider(stab);

        if let Ok(im) = params.get_i32(Params::IntegrationMethod) {
            let mut gyro = stab.gyro.write();
            if recompute || gyro.integration_method != im as usize {
                gyro.integration_method = im as usize;
                gyro.apply_transforms();
            }
        }

        Self::apply_smoothing(params, stab);

        if recompute {
            stab.invalidate_smoothing();
            stab.recompute_blocking();
        } else {
            stab.invalidate_blocking_smoothing();
        }
        let inverse = !(use_gyroflows_keyframes && stab.keyframes.read().is_keyframed_internally(&KeyframeType::VideoSpeed));
        stab.params.write().calculate_ramped_timestamps(&stab.keyframes.read(), inverse, inverse);
        Ok(())
    }

    /// Whether `stab`, shared with other instances, is set up for what this one would set it up with (see `configure`).
    /// The instances only render with it as long as they all agree on it
    fn agrees_with(&mut self, params: &dyn GyroflowPluginParams, stab: &StabilizationManager, out_size: (usize, usize)) -> bool {
        {
            let gf_params = stab.params.read();
            self.num_frames = gf_params.frame_count;
            self.fps = gf_params.fps;
        }
        // Cached anew rather than in place: the stabilization may read the ones the instance has (see `set_keyframe_provider`)
        let use_gyroflows_keyframes = params.get_bool(Params::UseGyroflowsKeyframes).unwrap_or_default();
        self.keyframable_params = Arc::new(RwLock::new(KeyframableParams::default()));
        self.cache_keyframes(params, use_gyroflows_keyframes, self.num_frames, self.fps.max(1.0));
        let Some(provider) = keyframe_provider(stab) else { return false; };
        let keyframes = provider.read().checksum() == self.keyframable_params.read().checksum();

        let gf_params = stab.params.read();
        let input_rotation = params.get_f64(Params::InputRotation).unwrap_or_default();
        let size = self.output_size_for(params, gf_params.size, input_rotation, if self.last_out_size != (0, 0) { self.last_out_size } else { out_size });
        // Its aspect, the stabilization fits the size to the video's, and not a thumbnail's (see `ensure_output_aspect`)
        let aspect = |s: (usize, usize)| s.0 as f64 / s.1.max(1) as f64;
        let output_size = size.0.max(size.1) < 320 || size.0 == 0 || size.1 == 0 || (aspect(gf_params.output_size) / aspect(size) - 1.0).abs() <= 0.01;
        let overview = gf_params.fov_overview == params.get_bool(Params::ToggleOverview).unwrap_or_default();
        drop(gf_params);
        let integration = params.get_i32(Params::IntegrationMethod).map_or(true, |x| stab.gyro.read().integration_method == x as usize);
        let interpolation = stab.stabilization.read().interpolation as u32 == Self::interpolation(params) as u32;

        let smoothing = Self::smoothing_agrees(params, stab);

        keyframes && output_size && overview && integration && interpolation && smoothing
    }

    /// Gives the instance a stabilization of its own, under a new instance id, to change from now on. It shared it with
    /// the instances copied with it (see `ever_changed`), which keep it as it is: the loaded one is forked rather than
    /// loaded again, and the stabilization is computed again on its first render
    pub fn detach(&mut self, params: &mut dyn GyroflowPluginParams, manager_cache: &Mutex<LruCache<String, Arc<StabilizationManager>>>) -> PluginResult<()> {
        let shared = Self::stab_key(params).ok().and_then(|key| self.managers.peek(&key).cloned().or_else(|| manager_cache.lock().peek(&key).cloned()));
        // First, as it can fail: the instance still shares its stabilization then
        params.set_string(Params::InstanceId, &format!("{}", fastrand::u64(..)))?;
        self.ever_changed = true;
        self.managers.clear();
        // The shared stabilization may read its keyframes from these (see `set_keyframe_provider`), and has to keep them
        let keyframable_params = self.keyframable_params.read().clone();
        self.keyframable_params = Arc::new(RwLock::new(keyframable_params));

        if let Some(shared) = shared {
            let key = Self::stab_key(params)?;
            log::info!("Forking the shared stabilization for key: {key}");
            let stab = shared.get_cloned();
            Self::setup_renderer(params, &stab);
            self.configure(params, &stab, self.last_out_size, false)?;
            let stab = Arc::new(stab);
            manager_cache.lock().put(key.clone(), stab.clone());
            self.managers.put(key, stab);
        }
        Ok(())
    }

    pub fn stab_manager(&mut self, params: &mut dyn GyroflowPluginParams, manager_cache: &Mutex<LruCache<String, Arc<StabilizationManager>>>, out_size: (usize, usize), open_gyroflow_if_no_data: bool) -> PluginResult<Arc<StabilizationManager>> {
        let mut disable_stretch = params.get_bool(Params::DisableStretch)?;

        let path = params.get_string(Params::ProjectPath)?;
        if path.is_empty() {
            self.update_loaded_state(params, false);
            return Err("Path is empty".into());
        }

        if self.timeline_size == (0, 0) {
            self.timeline_size = out_size;
        }
        if out_size.0.max(out_size.1) >= 320 { // not a thumbnail, see `ensure_output_aspect`
            self.last_out_size = out_size;
        }

        let key = Self::stab_key(params)?;
        let cloned = manager_cache.lock().get(&key).map(Arc::clone);
        let stab = if let Some(stab) = cloned {
            // First used by this instance: its own, set up before the instance was restored, or shared with the instances
            // copied with it, which set it up
            if !self.managers.contains(&key) {
                if self.ever_changed {
                    self.configure(params, &stab, out_size, false)?;
                } else if !self.agrees_with(params, &stab, out_size) {
                    log::info!("The shared stabilization is set up for other values, forking it");
                    self.detach(params, manager_cache)?;
                    return self.stab_manager(params, manager_cache, out_size, open_gyroflow_if_no_data);
                }
                // Cache it in this instance as well
                self.managers.put(key.to_owned(), stab.clone());
            }
            stab
        } else {
            log::info!("new stab manager for key: {key}");
            let mut stab = StabilizationManager::default();
            {
                // Find first lens profile database with loaded profiles
                let lock = manager_cache.lock();
                for (_, v) in lock.iter() {
                    if v.lens_profile_db.read().loaded {
                        stab.lens_profile_db = v.lens_profile_db.clone();
                        break;
                    }
                }
            }
            Self::setup_renderer(params, &stab);

            // The output size the project exports at, for the Output size parameters: the core keeps it fitted to the video
            let mut project_output_size: Option<(usize, usize)> = None;

            if !path.ends_with(".gyroflow") {
                let url = filesystem::path_to_url(&path);
                // A file that can't be opened (moved, or on a volume that isn't mounted) is one that can't be loaded
                let loaded = filesystem::open_file(&url, false, false).map_err(gyroflow_core::GyroflowCoreError::from).and_then(|mut file| {
                    let filesize = file.size;
                    stab.load_video_file(file.get_file(), filesize, &url, None, true)
                });
                match loaded {
                    Ok(md) => {
                        {
                            let gyro = stab.gyro.read();
                            let meta = gyro.file_metadata.read();
                            let source = meta.detected_source.as_deref().unwrap_or("Camera non identificata");
                            let readout = meta.frame_readout_time.map(|v| format!("{v:.2} ms")).unwrap_or_else(|| "n/d".into());
                            let focus = meta.lens_params.values().find_map(|v| v.focus_distance.filter(|d| d.is_finite() && *d > 0.0))
                                .map(|v| format!("{v:.2} m")).unwrap_or_else(|| "n/d".into());
                            let report = format!("{source} · {}×{} · {:.3} fps · {} campioni IMU · {} orientamenti · {} campioni ottica · prima distanza fuoco {focus} · readout {readout}",
                                md.width, md.height, md.fps, meta.raw_imu.len(), gyro.quaternions.len(), meta.lens_params.len());
                            params.set_string(Params::ClipReport, &report)?;
                        }
                        if out_size != (0, 0) {
                            stab.params.write().output_size = out_size; // Default to timeline output size
                        }
                        if let Some(preset_out_size) = stab.input_file.read().preset_output_size {
                            stab.params.write().output_size = preset_out_size;
                        }

                        Self::apply_embedded_presets(params, &stab, false);
                        if self.reload_values_from_project {
                            // The host shows the video turned by its rotation metadata, and so does the output by default.
                            // Set for unrotated videos too, a vertical video loaded before must not leave its rotation behind
                            let r = GyroflowPluginBase::input_rotation_from_metadata(md.rotation);
                            params.set_f64(Params::InputRotation, r)?;
                            stab.params.write().video_rotation = r;
                        }
                        if params.get_bool(Params::IncludeProjectData)? {
                            if let Ok(data) = stab.export_gyroflow_data(gyroflow_core::GyroflowProjectType::WithGyroData, "{}", None) {
                                let data = GyroflowPluginBase::maybe_inject_disable_stretch(&data, disable_stretch);
                                params.set_string(Params::ProjectData, &data)?;
                            }
                        }
                        params.set_string(Params::LoadedProject, &filesystem::get_filename(&filesystem::path_to_url(&path)))?;
                        
                    },
                    Err(e) => {
                        let embedded_data = params.get_string(Params::ProjectData)?;
                        if !embedded_data.is_empty() {
                            log::warn!("Failed to load {path}: {e:?}, loading the embedded project data");
                            let mut is_preset = false;
                            let project = stab.import_gyroflow_data(embedded_data.as_bytes(), true, None, |_|(), Arc::new(AtomicBool::new(false)), &mut is_preset, true).map_err(|e| {
                                self.update_loaded_state(params, false);
                                format!("load_gyro_data error: {e}")
                            })?;
                            project_output_size = GyroflowPluginBase::project_output_size(&project);
                        } else {
                            log::error!("An error occured: {e:?}");
                            self.update_loaded_state(params, false);
                            params.set_string(Params::Status, "Failed to load file info!")?;
                            params.set_hint(Params::Status, &format!("Error loading {path}: {e:?}."))?;
                            
                            return Err(e.into());
                        }
                    }
                }
            } else {
                let project_data = {
                    if params.get_bool(Params::IncludeProjectData)? && !params.get_string(Params::ProjectData)?.is_empty() {
                        params.get_string(Params::ProjectData)?
                    } else if let Ok(data) = std::fs::read_to_string(&path) {
                        if params.get_bool(Params::IncludeProjectData)? {
                            params.set_string(Params::ProjectData, &data)?;
                        } else {
                            params.set_string(Params::ProjectData, "")?;
                        }
                        data
                    } else {
                        "".to_string()
                    }
                };
                let mut is_preset = false;
                let project = stab.import_gyroflow_data(project_data.as_bytes(), true, Some(&filesystem::path_to_url(&path)), |_|(), Arc::new(AtomicBool::new(false)), &mut is_preset, true).map_err(|e| {
                    self.update_loaded_state(params, false);
                    format!("load_gyro_data error: {e}")
                })?;
                project_output_size = GyroflowPluginBase::project_output_size(&project);
                // Loaded with "Load preset/lens profile" over the project
                Self::apply_embedded_presets(params, &stab, true);
                params.set_string(Params::LoadedProject, &filesystem::get_filename(&filesystem::path_to_url(&path)))?;

                if self.reload_values_from_project {
                    // The host shows the video turned by its rotation metadata (Resolve on all its pages, Premiere and After
                    // Effects). The output rotation is the project's, which the app set from the same metadata or the user changed
                    let url = stab.input_file.read().url.clone();
                    if let Ok(mut file) = filesystem::open_file(&url, false, false) {
                        let filesize = file.size;
                        if let Ok(video_md) = gyroflow_core::util::get_video_metadata(file.get_file(), filesize, &url) {
                            params.set_f64(Params::InputRotation, GyroflowPluginBase::input_rotation_from_metadata(video_md.rotation))?;
                        }
                    }
                }
            }

            let loaded = {
                stab.params.write().calculate_ramped_timestamps(&stab.keyframes.read(), false, true);
                let gf_params = stab.params.read();
                self.original_video_size = gf_params.size;
                // The project's own export size when it has one, as the preset's is above (`preset_output_size`): what
                // `params.output_size` holds is that size fitted to the video, the clip's full size for an export of its aspect
                self.original_output_size = project_output_size.unwrap_or(gf_params.output_size);
                self.num_frames = gf_params.frame_count;
                self.fps = gf_params.fps;
                let loaded = gf_params.duration_ms > 0.0;
                let reload_adjustments = self.reload_values_from_project;
                if loaded && self.reload_values_from_project {
                    self.reload_values_from_project = false;
                    // What belongs to the clip
                    params.set_f64(Params::Rotation,               gf_params.video_rotation)?;
                    params.set_f64(Params::OutputWidth,            self.original_output_size.0 as f64)?;
                    params.set_f64(Params::OutputHeight,           self.original_output_size.1 as f64)?;
                    if let Some(name) = stab.input_file.read().preset_name.clone() {
                        params.set_string(Params::LoadedPreset, &name)?;
                    }
                    params.set_string(Params::LoadedLens, &stab.lens.read().get_display_name())?;
                }
                // The adjustments, unless they came with the effect
                if loaded && reload_adjustments && !std::mem::take(&mut self.keep_adjustments) {
                    let smooth = stab.smoothing.read();
                    let smoothness = smooth.current().get_parameter("smoothness");
                    params.set_f64(Params::Fov,                    gf_params.fov)?;
                    // A style keeps its own smoothness, crop and horizon. "Personalizzato" takes the project's
                    if params.get_i32(Params::StabPreset).map_or(true, |x| x == PRESET_CUSTOM) {
                        params.set_f64(Params::Smoothness,         smoothness * 100.0)?;
                        params.set_f64(Params::ZoomLimit,          gf_params.max_zoom.unwrap_or(0.0))?;
                        params.set_f64(Params::HorizonLockAmount,  if smooth.horizon_lock.lock_enabled { smooth.horizon_lock.horizonlockpercent } else { 0.0 })?;
                        Self::read_smoothing(params, &smooth, gf_params.adaptive_zoom_window)?;
                    }
                    params.set_f64(Params::LensCorrectionStrength, (gf_params.lens_correction_amount * 100.0).min(100.0))?;
                    params.set_f64(Params::HorizonLockRoll,        if smooth.horizon_lock.lock_enabled { smooth.horizon_lock.horizonroll } else { 0.0 })?;
                    params.set_f64(Params::VideoSpeed,             gf_params.video_speed * 100.0)?;
                    //params.set_f64(Params::PositionX,              gf_params.adaptive_zoom_center_offset.0 * 100.0)?;
                    //params.set_f64(Params::PositionY,              gf_params.adaptive_zoom_center_offset.1 * 100.0)?;
                    params.set_f64(Params::AdditionalYaw,          gf_params.additional_rotation.0)?;
                    params.set_f64(Params::AdditionalPitch,        gf_params.additional_rotation.1)?;
                    params.set_i32(Params::IntegrationMethod,      stab.gyro.read().integration_method as i32)?;

                    params.set_i32(Params::Interpolation, match stab.stabilization.read().interpolation {
                        gyroflow_core::stabilization::Interpolation::Lanczos4      => 0,
                        gyroflow_core::stabilization::Interpolation::RobidouxSharp => 1,
                        gyroflow_core::stabilization::Interpolation::Bilinear      => 2,
                        gyroflow_core::stabilization::Interpolation::Bicubic       => 3,
                        gyroflow_core::stabilization::Interpolation::Robidoux      => 4,
                        gyroflow_core::stabilization::Interpolation::Mitchell      => 5,
                        gyroflow_core::stabilization::Interpolation::CatmullRom    => 6,
                    })?;

                    let keyframes = stab.keyframes.read();
                    let all_keys = keyframes.get_all_keys();
                    params.set_bool(Params::UseGyroflowsKeyframes, !all_keys.is_empty())?;

                    for k in all_keys {
                        if let Some(keys) = keyframes.get_keyframes(k) {
                            if !keys.is_empty() {
                                macro_rules! set_keys {
                                    ($name:expr, $scale:expr) => {
                                        params.clear_keyframes($name)?;
                                        for (ts, v) in keys {
                                            let ts = if k == &KeyframeType::VideoSpeed { gf_params.get_source_timestamp_at_ramped_timestamp(*ts) } else { *ts };
                                            let time = (((ts as f64 / 1000.0) * gf_params.fps) / 1000.0).round();
                                            params.set_f64_at_time($name, TimeType::Frame(time), v.value * $scale)?;
                                        }
                                    };
                                }
                                match k {
                                    KeyframeType::Fov                      => { set_keys!(Params::Fov,                    1.0); },
                                    KeyframeType::SmoothingParamSmoothness => { set_keys!(Params::Smoothness,             100.0); },
                                    KeyframeType::MaxZoom                  => { set_keys!(Params::ZoomLimit,              1.0); },
                                    KeyframeType::LensCorrectionStrength   => { set_keys!(Params::LensCorrectionStrength, 100.0); },
                                    KeyframeType::LockHorizonAmount        => { set_keys!(Params::HorizonLockAmount,      1.0); },
                                    KeyframeType::LockHorizonRoll          => { set_keys!(Params::HorizonLockRoll,        1.0); },
                                    KeyframeType::VideoSpeed               => { set_keys!(Params::VideoSpeed,             100.0); },
                                    KeyframeType::VideoRotation            => { set_keys!(Params::Rotation,               1.0); },
                                    //KeyframeType::ZoomingCenterX           => { set_keys!(Params::PositionX,              100.0); },
                                    //KeyframeType::ZoomingCenterY           => { set_keys!(Params::PositionY,              100.0); },
                                    KeyframeType::AdditionalRotationX      => { set_keys!(Params::AdditionalYaw,          1.0); },
                                    KeyframeType::AdditionalRotationY      => { set_keys!(Params::AdditionalPitch,        1.0); },
                                    _ => { }
                                }
                            }
                        }
                    }
                }
                self.has_motion = stab.gyro.read().has_motion();
                loaded
            };

            self.update_loaded_state(params, loaded);

            // Check if loaded preset/project/lens data contains the plugin_disable_stretch flag
            if !disable_stretch {
                let has_flag = |d: &str| -> bool {
                    serde_json::from_str::<serde_json::Value>(d).ok()
                        .and_then(|v| v.get("plugin_disable_stretch").and_then(|v| v.as_bool()))
                        .unwrap_or(false)
                };
                for param_id in [Params::EmbeddedLensProfile, Params::EmbeddedPreset, Params::ProjectData] {
                    if let Ok(d) = params.get_string(param_id) {
                        if !d.is_empty() && has_flag(&d) {
                            disable_stretch = true;
                            let _ = params.set_bool(Params::DisableStretch, true);
                            break;
                        }
                    }
                }

            }

            if disable_stretch {
                stab.disable_lens_stretch(self.anamorphic_adjust_size);
            }

            self.configure(params, &stab, out_size, true)?;

            let stab = Arc::new(stab);
            // Insert to static global cache
            manager_cache.lock().put(key.to_owned(), stab.clone());
            // Cache it in this instance as well
            self.managers.put(key.to_owned(), stab.clone());

            stab
        };

        Ok(stab)
    }

    pub fn clear_stab(&mut self, manager_cache: &Mutex<LruCache<String, Arc<StabilizationManager>>>) {
        let local_keys = self.managers.iter().map(|x| x.0.clone()).collect::<Vec<_>>();
        self.managers.clear();

        // If there are no more local references, delete it from global cache
        let mut lock = manager_cache.lock();
        for key in local_keys {
            if let Some(v) = lock.get(&key) {
                if Arc::strong_count(v) == 1 {
                    lock.pop(&key);
                }
            }
        }
    }

    pub fn disable_opencl(&mut self) {
        if !self.opencl_disabled {
            unsafe { std::env::set_var("NO_OPENCL", "1") };
            self.opencl_disabled = true;
        }
    }

    pub fn set_status(&mut self, params: &mut dyn GyroflowPluginParams, status: &str, hint: &str, ok: bool) {
        if params.get_string(Params::Status).unwrap_or_default() != status {
            let _ = params.set_string(Params::Status, status);
            let _ = params.set_hint(Params::Status, hint);
            if ok {
                self.update_loaded_state(params, ok);
            }
        }
    }

    pub fn browse(current_path: &str) -> String {
        let mut d = rfd::FileDialog::new()
            .add_filter("Project and video files", &["mp4", "mov", "mxf", "braw", "r3d", "insv", "gyroflow"]);
        if !current_path.is_empty() {
            if let Some(path) = std::path::Path::new(current_path).parent() {
                d = d.set_directory(path);
            }
        }
        if let Some(d) = d.pick_file() {
            d.display().to_string()
        } else {
            String::new()
        }
    }

    /// Points the effect at `path`, a project or a video picked by the user, and loads all of its values. It's read from the
    /// file, again when it's the one loaded: the project data embedded with "Include project data" is a copy of the one
    /// picked before, for when its file is gone, and it's embedded again from this one
    fn pick_project(&mut self, params: &mut dyn GyroflowPluginParams, manager_cache: &Mutex<LruCache<String, Arc<StabilizationManager>>>, path: &str) -> PluginResult<()> {
        params.set_string(Params::ProjectPath, path)?;
        params.set_string(Params::ProjectData, "")?;
        self.reload_values_from_project = true;
        self.keep_adjustments = false;
        self.clear_stab(manager_cache);
        Ok(())
    }

    /// The video the effect stabilizes now: the one loaded for the project path, or else the one it names
    fn current_video(&self, params: &dyn GyroflowPluginParams) -> String {
        let path = params.get_string(Params::ProjectPath).unwrap_or_default();
        if let Some(stab) = Self::stab_key(params).ok().and_then(|key| self.managers.peek(&key)) {
            return stab.input_file.read().url.clone();
        }
        if !path.ends_with(".gyroflow") {
            return path;
        }
        std::fs::read_to_string(&path).ok().and_then(|x| GyroflowPluginBase::project_video_file(&x))
            .or_else(|| params.get_string(Params::ProjectData).ok().and_then(|x| GyroflowPluginBase::project_video_file(&x)))
            .unwrap_or_default()
    }

    pub fn param_changed(&mut self, params: &mut dyn GyroflowPluginParams, manager_cache: &Mutex<LruCache<String, Arc<StabilizationManager>>>, param: Params, user_edited: bool) -> Result<(), Box<dyn std::error::Error>> {
        if user_edited && !self.ever_changed && Self::changes_stabilization(param) {
            // From now on it's set up for this instance's values, rather than for the ones it shares with its copies
            self.detach(params, manager_cache)?;
        }
        if param == Params::ProjectPath && user_edited {
            // Typed in, the embedded copy is of the project before
            params.set_string(Params::ProjectData, "")?;
            self.keep_adjustments = false;
        }
        if param == Params::ReloadProject {
            // From the file, re-synced in the app maybe, and embedded again. The embedded copy stays when the file is gone
            let path = params.get_string(Params::ProjectPath)?;
            if path.ends_with(".gyroflow") && std::path::Path::new(&path).is_file() {
                params.set_string(Params::ProjectData, "")?;
            }
            self.keep_adjustments = false;
        }
        if param == Params::ProjectPath || param == Params::ReloadProject || param == Params::DontDrawOutside {
            if param == Params::ProjectPath || param == Params::ReloadProject {
                self.reload_values_from_project = true;
            }
            self.clear_stab(&manager_cache);
        }
        if param == Params::IncludeProjectData {
            let path = params.get_string(Params::ProjectPath)?;
            let ds = params.get_bool(Params::DisableStretch).unwrap_or(false);
            if params.get_bool(Params::IncludeProjectData).unwrap_or_default() {
                if path.ends_with(".gyroflow") {
                    if let Ok(data) = std::fs::read_to_string(&path) {
                        if StabilizationManager::project_has_motion_data(data.as_bytes()) {
                            let data = GyroflowPluginBase::maybe_inject_disable_stretch(&data, ds);
                            params.set_string(Params::ProjectData, &data)?;
                        } else {
                            if let Some((_, stab)) = self.managers.peek_lru() {
                                if let Ok(data) = stab.export_gyroflow_data(gyroflow_core::GyroflowProjectType::WithGyroData, "{}", None) {
                                    let data = GyroflowPluginBase::maybe_inject_disable_stretch(&data, ds);
                                    params.set_string(Params::ProjectData, &data)?;
                                }
                            }
                        }
                    } else {
                        params.set_string(Params::ProjectData, "")?;
                    }
                } else {
                    if let Some((_, stab)) = self.managers.peek_lru() {
                        if let Ok(data) = stab.export_gyroflow_data(gyroflow_core::GyroflowProjectType::WithGyroData, "{}", None) {
                            let data = GyroflowPluginBase::maybe_inject_disable_stretch(&data, ds);
                            params.set_string(Params::ProjectData, &data)?;
                        }
                    }
                }
            } else {
                params.set_string(Params::ProjectData, &"")?;
            }
        }
        if user_edited && param == Params::StabPreset {
            if let Some(preset) = params.get_i32(Params::StabPreset).ok().and_then(|x| STAB_PRESETS.get(x as usize)) {
                Self::write_preset(params, preset)?;
            }
        }
        // An advanced value edited away from the style's makes it "Personalizzato". Compared by value, as a host may
        // report the values a style writes as edits of the user too
        if user_edited && PRESET_ADVANCED_PARAMS.contains(&param) {
            if let Some(preset) = params.get_i32(Params::StabPreset).ok().and_then(|x| STAB_PRESETS.get(x as usize)) {
                if !Self::preset_has(params, preset, param) {
                    params.set_i32(Params::StabPreset, PRESET_CUSTOM)?;
                }
            }
        }
        if user_edited {
            if param == Params::OutputWidth || param == Params::OutputHeight || param == Params::OutputSizeSwap || param == Params::OutputSizeToTimeline {
                if param == Params::OutputSizeSwap {
                    let (w, h) = (params.get_f64(Params::OutputWidth)?, params.get_f64(Params::OutputHeight)? as _);
                    params.set_f64(Params::OutputWidth, h)?;
                    params.set_f64(Params::OutputHeight, w)?;
                }
                if param == Params::OutputSizeToTimeline {
                    params.set_f64(Params::OutputWidth, self.timeline_size.0 as f64)?;
                    params.set_f64(Params::OutputHeight, self.timeline_size.1 as f64)?;
                }
                let managers = self.managers.iter().map(|(_, v)| v.clone()).collect::<Vec<_>>();
                for v in &managers {
                    self.apply_rotation(params, v, self.last_out_size);
                    v.invalidate_blocking_zooming();
                }
            }
            match param {
                Params::Fov | Params::Smoothness | Params::ZoomLimit | Params::LensCorrectionStrength |
                Params::HorizonLockAmount | Params::HorizonLockRoll |
                //Params::PositionX | Params::PositionY |
                Params::AdditionalPitch | Params::AdditionalYaw |
                Params::Rotation | Params::InputRotation | Params::VideoSpeed | Params::IntegrationMethod |
                Params::UseGyroflowsKeyframes | Params::RecalculateKeyframes |
                Params::StabPreset | Params::SmoothingMethod | Params::SmoothPitch | Params::SmoothYaw | Params::SmoothRoll |
                Params::TimeConstant | Params::FastTimeConstant | Params::MotionInertia | Params::MotionBrake |
                Params::CropConstraint | Params::ZoomMode | Params::ZoomWindow => {

                    params.set_string(Params::Status, "Calculating...")?;
                    let use_gyroflows_keyframes = params.get_bool(Params::UseGyroflowsKeyframes).unwrap_or_default();
                    self.cache_keyframes(params, use_gyroflows_keyframes, self.num_frames, self.fps.max(1.0));
                    self.recompute_for(params, param, use_gyroflows_keyframes);
                    params.set_string(Params::Status, "OK")?;
                },
                _ => { }
            }
            if param == Params::ToggleOverview {
                let on = params.get_bool(Params::ToggleOverview)?;
                for (_, v) in self.managers.iter_mut() {
                    v.set_fov_overview(on);
                    v.invalidate_blocking_undistortion();
                }
            }
            if param == Params::Interpolation {
                self.managers.clear();
                manager_cache.lock().clear();
            }
        }

        Ok(())
    }
}

pub fn hash_string(s: &str) -> u64 {
    use std::hash::{Hash, Hasher};
    let mut hasher = std::collections::hash_map::DefaultHasher::new();
    s.hash(&mut hasher);
    hasher.finish()
}

impl std::str::FromStr for Params {
    type Err = serde_json::Error;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        serde_json::from_str(&format!("\"{}\"", s))
    }
}
impl ToString for Params {
    fn to_string(&self) -> String {
        format!("{:?}", self)
    }
}

#[macro_export]
macro_rules! define_params {
    ($name:ident {
        strings: [ $($str_enum:ident  => $str_field:ident: $str_host_type:ty,)* ],
        bools:   [ $($bool_enum:ident => $bool_field:ident: $bool_host_type:ty,)* ],
        f64s:    [ $($f64_enum:ident  => $f64_field:ident: $f64_host_type:ty,)* ],
        i32s:    [ $($i32_enum:ident  => $i32_field:ident: $i32_host_type:ty,)* ],

        get_string:  $gstr_s:ident   $gstr_p:ident                    $gstr_block:block,
        set_string:  $sstr_s:ident   $sstr_p:ident,   $sstr_v:ident   $sstr_block:block,
        get_bool:    $gbool_s:ident  $gbool_p:ident                   $gbool_block:block,
        set_bool:    $sbool_s:ident  $sbool_p:ident,  $sbool_v:ident  $sbool_block:block,
        get_f64:     $gf64_s:ident   $gf64_p:ident                    $gf64_block:block,
        set_f64:     $sf64_s:ident   $sf64_p:ident,   $sf64_v:ident   $sf64_block:block,
        get_i32:     $gi32_s:ident   $gi32_p:ident                    $gi32_block:block,
        set_i32:     $si32_s:ident   $si32_p:ident,   $si32_v:ident   $si32_block:block,
        set_label:   $slabel_s:ident $slabel_p:ident, $slabel_v:ident $slabel_block:block,
        set_hint:    $shint_s:ident  $shint_p:ident,  $shint_v:ident  $shint_block:block,
        set_enabled: $sen_s:ident    $sen_p:ident,    $sen_v:ident    $sen_block:block,
        get_bool_at_time: $gtbool_s:ident  $gtbool_p:ident, $gtbool_t:ident                $gtbool_block:block,
        get_f64_at_time:  $gtf64_s:ident   $gtf64_p:ident,  $gtf64_t:ident                 $gtf64_block:block,
        set_f64_at_time:  $stf64_s:ident  $stf64_p:ident,  $stf64_t:ident, $stf64_v:ident $stf64_block:block,
        is_keyframed: $iskeyframe_s:ident  $iskeyframe_p:ident $iskeyframe_block:block,
        get_keyframes: $gkeyframes_s:ident $gkeyframes_p:ident $gkeyframes_block:block,
        clear_keyframes: $clr_s:ident      $clr_p:ident $clr_block:block,

        $($additional_fields:ident: $additional_fields_t:ty,)*
    }) => {
        #[derive(Default)]
        pub struct ParamsAdditionalFields {
            $( pub $additional_fields: $additional_fields_t, )*
        }
        pub struct $name {
            $( $str_field: $str_host_type, )*
            $( $bool_field: $bool_host_type, )*
            $( $f64_field: $f64_host_type, )*
            $( $i32_field: $i32_host_type, )*

            pub fields: ParamsAdditionalFields,
        }
        impl GyroflowPluginParams for $name {
            fn get_string(&self, param: Params) -> $crate::PluginResult<String> {
                let $gstr_s = &self.fields;
                match param {
                    $( Params::$str_enum => { let $gstr_p = &self.$str_field; $gstr_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn set_string(&mut self, param: Params, value: &str) -> $crate::PluginResult<()> {
                let mut $sstr_s = &mut self.fields;
                match param {
                    $( Params::$str_enum => { let $sstr_p = &mut self.$str_field; let $sstr_v = value; $sstr_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn get_bool(&self, param: Params) -> $crate::PluginResult<bool> {
                let $gbool_s = &self.fields;
                match param {
                    $( Params::$bool_enum => { let $gbool_p = &self.$bool_field; $gbool_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn set_bool(&mut self, param: Params, value: bool) -> $crate::PluginResult<()> {
                let mut $sbool_s = &mut self.fields;
                match param {
                    $( Params::$bool_enum => { let $sbool_p = &mut self.$bool_field; let $sbool_v = value; $sbool_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn get_f64(&self, param: Params) -> $crate::PluginResult<f64> {
                let $gf64_s = &self.fields;
                match param {
                    $( Params::$f64_enum => { let $gf64_p = &self.$f64_field; $gf64_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn set_f64(&mut self, param: Params, value: f64) -> $crate::PluginResult<()> {
                let mut $sf64_s = &mut self.fields;
                match param {
                    $( Params::$f64_enum => { let $sf64_p = &mut self.$f64_field; let $sf64_v = value; $sf64_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn get_i32(&self, param: Params) -> $crate::PluginResult<i32> {
                let $gi32_s = &self.fields;
                match param {
                    $( Params::$i32_enum => { let $gi32_p = &self.$i32_field; $gi32_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn set_i32(&mut self, param: Params, value: i32) -> $crate::PluginResult<()> {
                let mut $si32_s = &mut self.fields;
                match param {
                    $( Params::$i32_enum => { let $si32_p = &mut self.$i32_field; let $si32_v = value; $si32_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn set_label(&mut self, param: Params, label: &str) -> $crate::PluginResult<()> {
                let mut $slabel_s = &mut self.fields;
                let $slabel_v = label;
                match param {
                    $( Params::$str_enum  => { let $slabel_p = &mut self.$str_field;  $slabel_block }, )*
                    $( Params::$bool_enum => { let $slabel_p = &mut self.$bool_field; $slabel_block }, )*
                    $( Params::$f64_enum  => { let $slabel_p = &mut self.$f64_field;  $slabel_block }, )*
                    $( Params::$i32_enum  => { let $slabel_p = &mut self.$i32_field;  $slabel_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn set_hint(&mut self, param: Params, hint: &str) -> $crate::PluginResult<()> {
                let mut $shint_s = &mut self.fields;
                let $shint_v = hint;
                match param {
                    $( Params::$str_enum  => { let $shint_p = &mut self.$str_field;  $shint_block }, )*
                    $( Params::$bool_enum => { let $shint_p = &mut self.$bool_field; $shint_block }, )*
                    $( Params::$f64_enum  => { let $shint_p = &mut self.$f64_field;  $shint_block }, )*
                    $( Params::$i32_enum  => { let $shint_p = &mut self.$i32_field;  $shint_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn set_enabled(&mut self, param: Params, enabled: bool) -> $crate::PluginResult<()> {
                let mut $sen_s = &mut self.fields;
                let $sen_v = enabled;
                match param {
                    $( Params::$str_enum  => { let $sen_p = &mut self.$str_field;  $sen_block }, )*
                    $( Params::$bool_enum => { let $sen_p = &mut self.$bool_field; $sen_block }, )*
                    $( Params::$f64_enum  => { let $sen_p = &mut self.$f64_field;  $sen_block }, )*
                    $( Params::$i32_enum  => { let $sen_p = &mut self.$i32_field;  $sen_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn get_f64_at_time(&self, param: Params, time: TimeType) -> $crate::PluginResult<f64> {
                let $gtf64_s = &self.fields;
                match param {
                    $( Params::$f64_enum => { let $gtf64_p = &self.$f64_field; let $gtf64_t = time; $gtf64_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn get_bool_at_time(&self, param: Params, time: TimeType) -> $crate::PluginResult<bool> {
                let $gtbool_s = &self.fields;
                match param {
                    $( Params::$bool_enum => { let $gtbool_p = &self.$bool_field; let $gtbool_t = time; $gtbool_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn clear_keyframes(&mut self, param: Params) -> $crate::PluginResult<()> {
                let mut $clr_s = &mut self.fields;
                match param {
                    $( Params::$f64_enum => { let $clr_p = &mut self.$f64_field; $clr_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn is_keyframed(&self, param: Params) -> bool {
                let $iskeyframe_s = &self.fields;
                match param {
                    $( Params::$f64_enum => { let $iskeyframe_p = &self.$f64_field; $iskeyframe_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn get_keyframes(&self, param: Params) -> Vec<(TimeType, f64)> {
                let $gkeyframes_s = &self.fields;
                match param {
                    $( Params::$f64_enum => { let $gkeyframes_p = &self.$f64_field; $gkeyframes_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
            fn set_f64_at_time(&mut self, param: Params, time: TimeType, value: f64) -> $crate::PluginResult<()> {
                let mut $stf64_s = &mut self.fields;
                match param {
                    $( Params::$f64_enum => { let $stf64_p = &mut self.$f64_field; let $stf64_t = time; let $stf64_v = value; $stf64_block }, )*
                    _ => panic!("Wrong parameter type"),
                }
            }
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    /// Parameters as a host keeps them, none keyframed
    #[derive(Default, Clone)]
    struct TestParams {
        strings: HashMap<Params, String>,
        bools: HashMap<Params, bool>,
        f64s: HashMap<Params, f64>,
        i32s: HashMap<Params, i32>,
    }
    impl GyroflowPluginParams for TestParams {
        fn set_enabled(&mut self, _: Params, _: bool) -> PluginResult<()> { Ok(()) }
        fn set_label(&mut self, _: Params, _: &str) -> PluginResult<()> { Ok(()) }
        fn set_hint(&mut self, _: Params, _: &str) -> PluginResult<()> { Ok(()) }
        fn set_f64(&mut self, p: Params, v: f64) -> PluginResult<()> { self.f64s.insert(p, v); Ok(()) }
        fn get_f64(&self, p: Params) -> PluginResult<f64> { Ok(self.f64s.get(&p).copied().unwrap_or_default()) }
        fn get_f64_at_time(&self, p: Params, _: TimeType) -> PluginResult<f64> { self.get_f64(p) }
        fn set_bool(&mut self, p: Params, v: bool) -> PluginResult<()> { self.bools.insert(p, v); Ok(()) }
        fn get_bool(&self, p: Params) -> PluginResult<bool> { Ok(self.bools.get(&p).copied().unwrap_or_default()) }
        fn get_bool_at_time(&self, p: Params, _: TimeType) -> PluginResult<bool> { self.get_bool(p) }
        fn set_string(&mut self, p: Params, v: &str) -> PluginResult<()> { self.strings.insert(p, v.into()); Ok(()) }
        fn get_string(&self, p: Params) -> PluginResult<String> { Ok(self.strings.get(&p).cloned().unwrap_or_default()) }
        fn set_i32(&mut self, p: Params, v: i32) -> PluginResult<()> { self.i32s.insert(p, v); Ok(()) }
        fn get_i32(&self, p: Params) -> PluginResult<i32> { self.i32s.get(&p).copied().ok_or_else(|| "not set".into()) }
        fn is_keyframed(&self, _: Params) -> bool { false }
        fn get_keyframes(&self, _: Params) -> Vec<(TimeType, f64)> { Vec::new() }
        fn clear_keyframes(&mut self, _: Params) -> PluginResult<()> { Ok(()) }
        fn set_f64_at_time(&mut self, _: Params, _: TimeType, _: f64) -> PluginResult<()> { Ok(()) }
    }

    type Cache = Mutex<LruCache<String, Arc<StabilizationManager>>>;

    fn cache() -> Cache {
        Mutex::new(LruCache::new(std::num::NonZeroUsize::new(8).unwrap()))
    }
    #[test]
    fn project_output_size_is_the_export_size() {
        let project = serde_json::json!({ "video_info": { "width": 3840, "height": 2160, "rotation": 90.0 }, "output": { "output_width": 1496, "output_height": 2660 } });
        assert_eq!(GyroflowPluginBase::project_output_size(&project), Some((1496, 2660)));
        assert_eq!(GyroflowPluginBase::project_output_size(&serde_json::json!({ "output": { "output_width": 0, "output_height": 2660 } })), None);
        assert_eq!(GyroflowPluginBase::project_output_size(&serde_json::json!({ "output": { "interpolation": "Lanczos4" } })), None);
        assert_eq!(GyroflowPluginBase::project_output_size(&serde_json::json!({ "videofile": "" })), None);
    }
    fn params() -> TestParams {
        let mut params = TestParams::default();
        params.strings.insert(Params::ProjectPath, format!("D:/clips/{}.gyroflow", fastrand::u64(..)));
        params.strings.insert(Params::InstanceId, format!("{}", fastrand::u64(..)));
        params.f64s.insert(Params::Smoothness, 50.0);
        params
    }
    /// An instance with a stabilization set up for `params` in `cache`, as if it loaded it
    fn loaded(params: &mut TestParams, cache: &Cache, ever_changed: bool) -> (GyroflowPluginBaseInstance, Arc<StabilizationManager>) {
        let mut inst = GyroflowPluginBaseInstance { ever_changed, ..Default::default() };
        let stab = StabilizationManager::default();
        GyroflowPluginBaseInstance::setup_renderer(params, &stab);
        inst.configure(params, &stab, (0, 0), false).unwrap();
        let stab = Arc::new(stab);
        let key = GyroflowPluginBaseInstance::stab_key(params).unwrap();
        cache.lock().put(key.clone(), stab.clone());
        inst.managers.put(key, stab.clone());
        (inst, stab)
    }
    fn reads_from(stab: &StabilizationManager, inst: &GyroflowPluginBaseInstance) -> bool {
        keyframe_provider(stab).is_some_and(|x| Arc::ptr_eq(&x, &inst.keyframable_params))
    }
    fn id(params: &TestParams) -> String {
        params.get_string(Params::InstanceId).unwrap()
    }

    #[test]
    fn copies_share_the_stabilization_while_they_agree() {
        let cache = cache();
        let mut params_a = params();
        let (a, shared) = loaded(&mut params_a, &cache, false);

        // A copy, with the same values
        let mut params_b = params_a.clone();
        let mut b = GyroflowPluginBaseInstance::default();
        let stab = b.stab_manager(&mut params_b, &cache, (1920, 1080), false).unwrap();
        assert!(Arc::ptr_eq(&stab, &shared));
        assert_eq!(id(&params_b), id(&params_a));
        assert!(!b.ever_changed);
        // As the one that loaded it set it up
        assert!(reads_from(&shared, &a));
    }

    #[test]
    fn a_copy_set_up_for_other_values_forks_the_stabilization() {
        let changes: [fn(&mut TestParams); 2] = [
            |p| { p.bools.insert(Params::ToggleOverview, true); },
            |p| { p.f64s.insert(Params::Smoothness, 80.0); },
        ];
        for change in changes {
            let cache = cache();
            let mut params_a = params();
            let (a, shared) = loaded(&mut params_a, &cache, false);

            let mut params_b = params_a.clone();
            change(&mut params_b);
            let mut b = GyroflowPluginBaseInstance::default();
            let stab = b.stab_manager(&mut params_b, &cache, (1920, 1080), false).unwrap();
            assert!(!Arc::ptr_eq(&stab, &shared));
            assert_ne!(id(&params_b), id(&params_a));
            assert!(b.ever_changed && reads_from(&stab, &b));
            assert_eq!(stab.params.read().fov_overview, params_b.get_bool(Params::ToggleOverview).unwrap());
            // The shared one as it was
            assert!(!shared.params.read().fov_overview && reads_from(&shared, &a));
            assert!(cache.lock().contains(&GyroflowPluginBaseInstance::stab_key(&params_a).unwrap()));
        }
    }

    #[test]
    fn edits_of_a_copy_leave_the_shared_stabilization_alone() {
        for param in [Params::Smoothness, Params::ToggleOverview, Params::OutputWidth] {
            let cache = cache();
            let mut params_a = params();
            let (a, shared) = loaded(&mut params_a, &cache, false);
            let checksum = a.keyframable_params.read().checksum();

            let mut params_b = params_a.clone();
            let mut b = GyroflowPluginBaseInstance::default();
            b.stab_manager(&mut params_b, &cache, (1920, 1080), false).unwrap();
            params_b.f64s.insert(Params::Smoothness, 80.0);
            params_b.bools.insert(Params::ToggleOverview, true);
            params_b.f64s.insert(Params::OutputWidth, 1080.0);
            b.param_changed(&mut params_b, &cache, param, true).unwrap();

            assert!(b.ever_changed);
            assert_ne!(id(&params_b), id(&params_a));
            assert!(b.managers.iter().all(|(_, x)| !Arc::ptr_eq(x, &shared)));
            assert!(reads_from(&shared, &a) && a.keyframable_params.read().checksum() == checksum);
            assert!(!shared.params.read().fov_overview);
        }
    }

    #[test]
    fn keyframes_changed_without_an_edit_fork_a_copy() {
        let cache = cache();
        let mut params_a = params();
        let (a, shared) = loaded(&mut params_a, &cache, false);
        let checksum = a.keyframable_params.read().checksum();

        let mut params_b = params_a.clone();
        let mut b = GyroflowPluginBaseInstance::default();
        b.stab_manager(&mut params_b, &cache, (1920, 1080), false).unwrap();
        params_b.f64s.insert(Params::Fov, 1.5);
        b.keyframable_params_changed(&mut params_b, &cache, &[Params::Fov]).unwrap();

        assert!(b.ever_changed);
        let stab = b.stab_manager(&mut params_b, &cache, (1920, 1080), false).unwrap();
        assert!(!Arc::ptr_eq(&stab, &shared) && reads_from(&stab, &b));
        assert_ne!(b.keyframable_params.read().checksum(), checksum);
        assert!(reads_from(&shared, &a) && a.keyframable_params.read().checksum() == checksum);
    }

    #[test]
    fn an_instance_of_its_own_sets_up_its_stabilization_again() {
        let cache = cache();
        let mut params = params();
        let (_, stab) = loaded(&mut params, &cache, true);

        // Restored from before an undo or a revert, with other values than it had when it set it up
        params.bools.insert(Params::ToggleOverview, true);
        let mut restored = GyroflowPluginBaseInstance { ever_changed: true, ..Default::default() };
        let found = restored.stab_manager(&mut params, &cache, (1920, 1080), false).unwrap();
        assert!(Arc::ptr_eq(&found, &stab));
        assert!(stab.params.read().fov_overview && reads_from(&stab, &restored));
    }

    #[test]
    fn a_shared_instance_sets_up_another_stabilization_without_changing_the_shared_one() {
        let cache = cache();
        let mut params = params();
        let (mut a, shared) = loaded(&mut params, &cache, false);
        let checksum = keyframe_provider(&shared).unwrap().read().checksum();

        // Another project picked, loaded under another key, with values of its own
        params.f64s.insert(Params::Smoothness, 80.0);
        let other = StabilizationManager::default();
        a.configure(&mut params, &other, (0, 0), false).unwrap();
        assert!(reads_from(&other, &a));
        assert_eq!(keyframe_provider(&shared).unwrap().read().checksum(), checksum);
    }

    #[test]
    fn detaches_without_a_loaded_stabilization() {
        let cache = cache();
        let mut params = params();
        let before = id(&params);
        let mut inst = GyroflowPluginBaseInstance::default();
        inst.detach(&mut params, &cache).unwrap();
        assert!(inst.ever_changed && inst.managers.is_empty() && cache.lock().is_empty());
        assert_ne!(id(&params), before);
    }

    /// The parameters of a style, as `write_preset` leaves them
    fn styled(preset: i32) -> TestParams {
        let mut params = params();
        GyroflowPluginBaseInstance::write_preset(&mut params, &STAB_PRESETS[preset as usize]).unwrap();
        params.i32s.insert(Params::StabPreset, preset);
        params
    }
    /// Walking with a pan: a 1° bob at the step rate (2 Hz) on the pitch, a 0.2° tremor at 12 Hz on every axis, and a
    /// 20°/s pan from 3 to 9 s, eased in and out over 0.5 s. Sampled at 200 Hz over 12 s
    fn walking_pan() -> (gyroflow_core::gyro_source::TimeQuat, Vec<[f64; 3]>) {
        use std::f64::consts::PI;
        let d = PI / 180.0;
        let pan = |t: f64| -> f64 {
            // Angle (°) of a pan at 20°/s between 3.25 and 8.75 s, with linear ramps of the speed over 0.5 s at each end
            let v = 20.0;
            let ramp = |x: f64| if x <= 0.0 { 0.0 } else if x < 0.5 { v * x * x / 1.0 } else { v * (x - 0.25) };
            ramp(t - 3.0) - ramp(t - 8.5)
        };
        let mut quats = std::collections::BTreeMap::new();
        let mut intended = Vec::new();
        for i in 0..2400 {
            let t = i as f64 / 200.0;
            let tremor = 0.2 * (2.0 * PI * 12.0 * t).sin();
            let a = [1.0 * (2.0 * PI * 2.0 * t).sin() + tremor, pan(t) + tremor, tremor];
            quats.insert((t * 1_000_000.0) as i64, gyroflow_core::gyro_source::Quat64::from_euler_angles(a[0] * d, a[1] * d, a[2] * d));
            intended.push([0.0, pan(t), 0.0]);
        }
        (quats, intended)
    }
    /// The smoothed path's distance to the intended one (°): its RMS on the pitch and the roll between 1 and 11 s, and
    /// the largest on the yaw while the pan is steady (5 to 7 s), how far the path lags it
    fn smoothing_error(params: &TestParams) -> [f64; 3] {
        let (quats, intended) = walking_pan();
        let mut smoothing = gyroflow_core::smoothing::Smoothing::default();
        GyroflowPluginBaseInstance::configure_smoothing(params, &mut smoothing);
        let smoothed = smoothing.current().smooth(&quats, 12_000.0, &gyroflow_core::stabilization::ComputeParams::default());
        let (mut sum, mut lag, mut n) = ([0.0; 3], 0.0f64, 0.0);
        for (i, (_, q)) in smoothed.iter().enumerate() {
            if i < 200 || i >= 2200 { continue; }
            let d = std::f64::consts::PI / 180.0;
            let want = gyroflow_core::gyro_source::Quat64::from_euler_angles(intended[i][0] * d, intended[i][1] * d, intended[i][2] * d);
            let e = (want.inverse() * q).euler_angles();
            sum[0] += (e.0 / d).powi(2);
            sum[2] += (e.2 / d).powi(2);
            if (1000..1400).contains(&i) { lag = lag.max((e.1 / d).abs()); }
            n += 1.0;
        }
        [(sum[0] / n).sqrt(), lag, (sum[2] / n).sqrt()]
    }

    #[test]
    fn effects_saved_before_the_styles_smooth_as_before() {
        // No style parameters: the classic algorithm with Gyroflow's defaults
        let mut smoothing = gyroflow_core::smoothing::Smoothing::default();
        let before = smoothing.current().get_checksum();
        GyroflowPluginBaseInstance::configure_smoothing(&params(), &mut smoothing);
        assert_eq!(smoothing.current().get_name(), "Default");
        assert_eq!(smoothing.current().get_checksum(), before);
    }

    #[test]
    fn styles_set_up_the_optimal_path_per_axis() {
        for preset in 0..STAB_PRESETS.len() as i32 {
            let params = styled(preset);
            let mut smoothing = gyroflow_core::smoothing::Smoothing::default();
            GyroflowPluginBaseInstance::configure_smoothing(&params, &mut smoothing);
            let alg = smoothing.current();
            let p = &STAB_PRESETS[preset as usize];
            assert_eq!(alg.get_name(), "Optimal path");
            assert_eq!(alg.get_parameter("max_time_constant"), p.time_constant);
            assert_eq!(alg.get_parameter("order_2"), p.inertia);
            assert_eq!(alg.get_parameter("per_axis") > 0.5, p.axes != [100.0; 3]);
            assert!((alg.get_parameter("smoothness_yaw") - p.smoothness / 100.0 * p.axes[1] / 100.0).abs() < 1e-12);
            assert_eq!(GyroflowPluginBaseInstance::zoom_window(&params), if p.zoom_mode == ZOOM_STATIC { -1.0 } else { p.zoom_window });
        }
    }

    #[test]
    fn styles_take_the_steps_out_and_keep_the_pan() {
        let classic = smoothing_error(&params());
        let micro = smoothing_error(&styled(0));
        let cinema = smoothing_error(&styled(2));
        let crane = smoothing_error(&styled(3));
        eprintln!("Pitch RMS / pan lag / roll RMS (°) · classic {classic:.3?} · micro {micro:.3?} · cinema {cinema:.3?} · crane {crane:.3?}");
        // Unsmoothed, the bob and the tremor are 0.72° RMS on the pitch
        assert!(cinema[0] < 0.05 && crane[0] < 0.05, "steps left on the pitch");
        // Cinematografico follows a steady pan closer than the classic algorithm, Crane eases into it over about a second
        assert!(cinema[1] < classic[1] / 3.0, "the pan lags");
        assert!(crane[1] < 1.5, "the pan lags");
        // Micromosso takes the tremor out and leaves the walk
        assert!(micro[0] > 0.4 && micro[2] < 0.05);
    }

    #[test]
    fn advanced_edits_away_from_a_style_make_it_custom() {
        let cache = cache();
        let mut params = styled(2);
        let (mut inst, _) = loaded(&mut params, &cache, true);
        // The value the style wrote, reported as an edit: still the style
        inst.param_changed(&mut params, &cache, Params::SmoothYaw, true).unwrap();
        assert_eq!(params.get_i32(Params::StabPreset).unwrap(), 2);
        params.f64s.insert(Params::SmoothYaw, 50.0);
        inst.param_changed(&mut params, &cache, Params::SmoothYaw, true).unwrap();
        assert_eq!(params.get_i32(Params::StabPreset).unwrap(), PRESET_CUSTOM);
        // Picking a style writes it
        params.i32s.insert(Params::StabPreset, 3);
        inst.param_changed(&mut params, &cache, Params::StabPreset, true).unwrap();
        assert_eq!(params.get_f64(Params::HorizonLockAmount).unwrap(), 100.0);
        assert_eq!(params.get_i32(Params::ZoomMode).unwrap(), ZOOM_STATIC);
        let stab = inst.managers.peek(&GyroflowPluginBaseInstance::stab_key(&params).unwrap()).unwrap().clone();
        assert_eq!(stab.smoothing.read().current().get_parameter("order_2"), STAB_PRESETS[3].inertia);
        assert_eq!(stab.params.read().adaptive_zoom_window, -1.0);
    }

    #[test]
    fn a_copy_with_another_style_forks_the_stabilization() {
        let cache = cache();
        let mut params_a = styled(2);
        let (_a, stab) = loaded(&mut params_a, &cache, true);
        let mut params_b = params_a.clone();
        GyroflowPluginBaseInstance::write_preset(&mut params_b, &STAB_PRESETS[0]).unwrap();
        let mut b = GyroflowPluginBaseInstance::default();
        assert!(!b.agrees_with(&params_b, &stab, (0, 0)));
        assert!(b.agrees_with(&params_a, &stab, (0, 0)));
    }

    #[test]
    fn transcodes_are_the_same_footage() {
        let original = Footage { size: (3840, 2160), fps: 25.0, duration_ms: 10_000.0 };
        assert!(original.matches_scaled(&Footage { size: (1920, 1080), fps: 25.0, duration_ms: 10_040.0 }));
        assert!(original.matches_scaled(&Footage { size: (1080, 1920), ..original }));
        assert!(!original.matches(&Footage { size: (1920, 1080), ..original }));
        assert!(!original.matches_scaled(&Footage { size: (1440, 1080), ..original }));
        assert!(!original.matches_scaled(&Footage { size: (1920, 1080), fps: 30.0, ..original }));
        assert!(!original.matches_scaled(&Footage { size: (1920, 1080), duration_ms: 12_000.0, ..original }));
    }
}


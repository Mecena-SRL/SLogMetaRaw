// SPDX-License-Identifier: GPL-3.0-or-later
//! Database Mecena dei metadati per clip (formato `mecena-clipdb` v1).
//!
//! Una cartella `MecenaClipDB/` contiene, per ogni originale, tutto ciò che la telemetria descrive: il progetto Gyroflow
//! con i dati IMU incorporati (che Stabilize carica come un `.gyroflow`), i campioni scorporati in CSV e un riepilogo di
//! camera, sensore e ottica. Lo scrive `mecena-clipdb` (vedi `src/bin/mecena-clipdb.rs` nel crate OpenFX); Stabilize lo
//! legge con [`resolve_media`] quando conosce l'originale, anche offline o sostituito da un proxy, e con
//! [`resolve_timeline`] su un adjustment clip, dove l'host non espone alcun file.
//!
//! ```text
//! MecenaClipDB/
//!   index.json                     elenco delle clip (IndexEntry)
//!   clips/<id>/clip.json           riepilogo completo: camera, video, posa, readout, IMU, sensore, ottica
//!   clips/<id>/gyro.gyroflow       progetto Gyroflow con i dati movimento (GyroflowProjectType::WithGyroData)
//!   clips/<id>/imu.csv             t_ms, gyro °/s xyz, accel m/s² xyz, magn xyz (campioni grezzi del file)
//!   clips/<id>/orientation.csv     t_ms, quaternione integrato w x y z
//!   clips/<id>/lens.csv            t_ms, focale, fuoco, diaframma, zoom, sensore… per campione ottico
//!   clips/<id>/frames.csv          frame, t_ms, offset di cattura per frame
//!   clips/<id>/metadata.json       FileMetadata senza le serie (dati aggiuntivi della camera inclusi)
//!   timelines/<nome>.json          mappa timeline → clip, scritta dallo script di Resolve
//! ```
//!
//! Ricerca dei database: variabile `MECENA_CLIPDB` (percorsi separati da `:`), file di configurazione utente
//! ([`config_path`]) e una cartella `MecenaClipDB` nella cartella dell'originale o fino a cinque livelli sopra.

use serde::{ Deserialize, Serialize };
use std::collections::HashMap;
use std::io::{ Read, Seek, SeekFrom };
use std::path::{ Path, PathBuf };
use std::sync::{ Arc, Mutex };
use std::time::SystemTime;

pub const DB_DIR_NAME: &str = "MecenaClipDB";
pub const INDEX_FORMAT: &str = "mecena-clipdb";
pub const TIMELINE_FORMAT: &str = "mecena-clipdb-timeline";
pub const VERSION: u32 = 1;
pub const PROJECT_FILE: &str = "gyro.gyroflow";

/// Una clip in `index.json`. Ciò che serve per riconoscere l'originale e per il rapporto del pannello
#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq)]
#[serde(default)]
pub struct IndexEntry {
    pub id: String,
    /// Percorso assoluto dell'originale al momento della scansione
    pub path: String,
    pub file_name: String,
    pub size: u64,
    /// `size` più FNV-1a dei primi e degli ultimi 64 KiB, in esadecimale
    pub fingerprint: String,
    pub fps: f64,
    pub frame_count: u64,
    pub width: u32,
    pub height: u32,
    pub has_gyro: bool,
    /// Riga breve per «Dati rilevati»
    pub summary: String,
    /// Timecode iniziale come lo legge Resolve, scritto dallo script: serve ad abbinare i file dei registratori esterni
    pub start_tc: String,
    pub scanned_at: u64,
}

/// Un file senza dati movimento propri (ProRes RAW da un registratore esterno, un intermedio) che usa quelli di una
/// clip interna registrata insieme: il suo frame `n` è il frame `n + frame_offset` della clip
#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq)]
#[serde(default)]
pub struct Link {
    pub external: String,
    pub clip_id: String,
    /// In frame della clip interna; positivo se l'esterno parte dopo
    pub frame_offset: f64,
    /// `timecode`, `manuale` o `sequenza`
    pub method: String,
    pub note: String,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct Index {
    pub format: String,
    pub version: u32,
    pub clips: Vec<IndexEntry>,
    pub links: Vec<Link>,
}

pub struct ClipDb {
    pub root: PathBuf,
    pub index: Index,
}

impl ClipDb {
    pub fn open(root: &Path) -> Option<Self> {
        let data = std::fs::read_to_string(root.join("index.json")).ok()?;
        let index: Index = serde_json::from_str(&data).ok()?;
        (index.format == INDEX_FORMAT && index.version <= VERSION).then(|| Self { root: root.to_path_buf(), index })
    }
    /// Un database vuoto in `root`, per la prima scansione
    pub fn open_or_create(root: &Path) -> Self {
        Self::open(root).unwrap_or_else(|| Self { root: root.to_path_buf(), index: Index { format: INDEX_FORMAT.into(), version: VERSION, clips: Vec::new(), links: Vec::new() } })
    }
    pub fn clip_dir(&self, id: &str) -> PathBuf { self.root.join("clips").join(id) }
    pub fn project_path(&self, id: &str) -> PathBuf { self.clip_dir(id).join(PROJECT_FILE) }
    pub fn get(&self, id: &str) -> Option<&IndexEntry> { self.index.clips.iter().find(|c| c.id == id) }

    /// Il collegamento di un file esterno: per percorso, o per nome se uno solo lo porta
    pub fn find_link(&self, media_path: &str) -> Option<&Link> {
        let wanted = normalize_path(media_path);
        if let Some(l) = self.index.links.iter().find(|l| normalize_path(&l.external) == wanted) {
            return Some(l);
        }
        let name = file_name(media_path).to_lowercase();
        let same: Vec<_> = self.index.links.iter().filter(|l| file_name(&l.external).to_lowercase() == name).collect();
        (same.len() == 1).then(|| same[0])
    }

    pub fn set_link(&mut self, link: Link) {
        let key = normalize_path(&link.external);
        self.index.links.retain(|l| normalize_path(&l.external) != key);
        self.index.links.push(link);
    }

    pub fn upsert(&mut self, entry: IndexEntry) {
        self.index.clips.retain(|c| c.id != entry.id && c.path != entry.path);
        self.index.clips.push(entry);
        self.index.clips.sort_by(|a, b| a.path.cmp(&b.path));
    }
    /// Scrive `index.json` passando da un file temporaneo, così il plugin non legge mai un indice a metà
    pub fn save(&mut self) -> std::io::Result<()> {
        self.index.format = INDEX_FORMAT.into();
        self.index.version = VERSION;
        std::fs::create_dir_all(&self.root)?;
        let tmp = self.root.join("index.json.tmp");
        std::fs::write(&tmp, serde_json::to_vec_pretty(&self.index)?)?;
        std::fs::rename(tmp, self.root.join("index.json"))
    }

    /// La clip di un originale. Nell'ordine: stesso percorso; stesso nome e stessa dimensione se il file è raggiungibile;
    /// stesso nome normalizzato (un proxy `C0001_proxy.mov` per `C0001.MP4`), se uno solo o uno solo con più cartelle in
    /// comune. Due schede con le stesse numerazioni non vengono confuse: senza un criterio che le separi non c'è risultato
    pub fn find_by_media(&self, media_path: &str) -> Option<&IndexEntry> {
        let wanted = normalize_path(media_path);
        if let Some(e) = self.index.clips.iter().find(|c| normalize_path(&c.path) == wanted) {
            return Some(e);
        }
        let name = file_name(media_path).to_lowercase();
        if let Ok(md) = std::fs::metadata(media_path) {
            let same: Vec<_> = self.index.clips.iter().filter(|c| c.file_name.to_lowercase() == name && c.size == md.len()).collect();
            if same.len() == 1 {
                return Some(same[0]);
            }
        }
        let stem = normalize_stem(media_path);
        if stem.is_empty() {
            return None;
        }
        let candidates: Vec<_> = self.index.clips.iter().filter(|c| normalize_stem(&c.file_name) == stem).collect();
        match candidates.len() {
            0 => None,
            1 => Some(candidates[0]),
            _ => {
                let mut scored: Vec<(usize, &IndexEntry)> = candidates.into_iter().map(|c| (shared_dirs(&c.path, media_path), c)).collect();
                scored.sort_by(|a, b| b.0.cmp(&a.0));
                (scored[0].0 > 0 && scored[0].0 > scored[1].0).then(|| scored[0].1)
            }
        }
    }
}

fn normalize_path(p: &str) -> String {
    let p = p.strip_prefix("file://").unwrap_or(p).replace('\\', "/");
    if cfg!(any(target_os = "macos", target_os = "windows")) { p.to_lowercase() } else { p }
}

fn file_name(p: &str) -> String {
    p.replace('\\', "/").rsplit('/').next().unwrap_or_default().to_string()
}

/// Nome senza estensione, minuscolo, senza i suffissi tipici dei proxy e delle copie ottimizzate
pub fn normalize_stem(p: &str) -> String {
    let name = file_name(p).to_lowercase();
    let mut stem = match name.rfind('.') { Some(i) if i > 0 => name[..i].to_string(), _ => name };
    loop {
        let before = stem.len();
        for suffix in ["proxy", "prx", "lowres", "optimized", "optimised", "s001"] {
            for sep in ["_", "-", " ", "."] {
                let s = format!("{sep}{suffix}");
                if stem.ends_with(&s) {
                    stem.truncate(stem.len() - s.len());
                }
            }
        }
        if stem.len() == before { break; }
    }
    stem
}

/// Quante cartelle, a partire da quella del file, i due percorsi hanno in comune (scheda, giorno, camera…)
fn shared_dirs(a: &str, b: &str) -> usize {
    let skip = |s: &str| matches!(s, "proxy" | "proxies" | "optimized media" | "cache" | "");
    let da: Vec<String> = a.replace('\\', "/").to_lowercase().split('/').rev().skip(1).filter(|s| !skip(s)).map(String::from).collect();
    let db: Vec<String> = b.replace('\\', "/").to_lowercase().split('/').rev().skip(1).filter(|s| !skip(s)).map(String::from).collect();
    da.iter().filter(|d| db.contains(d)).count()
}

pub fn fnv1a64(state: u64, bytes: &[u8]) -> u64 {
    bytes.iter().fold(state, |h, b| (h ^ *b as u64).wrapping_mul(0x100000001b3))
}
pub const FNV_OFFSET: u64 = 0xcbf29ce484222325;

/// Impronta rapida anche su dischi di rete: dimensione, primi e ultimi 64 KiB
pub fn fingerprint_file(path: &Path) -> std::io::Result<String> {
    const CHUNK: u64 = 64 * 1024;
    let mut f = std::fs::File::open(path)?;
    let size = f.metadata()?.len();
    let mut h = fnv1a64(FNV_OFFSET, &size.to_le_bytes());
    let mut buf = vec![0u8; CHUNK.min(size) as usize];
    f.read_exact(&mut buf)?;
    h = fnv1a64(h, &buf);
    if size > CHUNK {
        let tail = CHUNK.min(size - CHUNK);
        f.seek(SeekFrom::Start(size - tail))?;
        buf.resize(tail as usize, 0);
        f.read_exact(&mut buf)?;
        h = fnv1a64(h, &buf);
    }
    Ok(format!("{size:x}-{h:016x}"))
}

/// Identificativo stabile e leggibile: nome dell'originale più otto cifre dell'impronta
pub fn clip_id(file_name: &str, fingerprint: &str) -> String {
    let stem: String = normalize_stem(file_name).chars().map(|c| if c.is_ascii_alphanumeric() { c } else { '_' }).collect();
    let h = fnv1a64(FNV_OFFSET, fingerprint.as_bytes());
    format!("{stem}-{:08x}", h as u32)
}

// ---------------------------------------------------------------------------------------------------------------------
// Configurazione utente: i database registrati e la timeline attiva

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct Config {
    pub databases: Vec<PathBuf>,
    /// La mappa letta da Stabilize su un adjustment clip, impostata dallo script di Resolve
    pub active_timeline: Option<PathBuf>,
}

pub fn config_path() -> Option<PathBuf> {
    let home = std::env::var_os("HOME").map(PathBuf::from)?;
    Some(if cfg!(target_os = "macos") {
        home.join("Library/Application Support/Mecena/clipdb.json")
    } else {
        home.join(".config/mecena/clipdb.json")
    })
}

pub fn read_config() -> Config {
    config_path().and_then(|p| std::fs::read_to_string(p).ok()).and_then(|s| serde_json::from_str(&s).ok()).unwrap_or_default()
}

pub fn write_config(cfg: &Config) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| std::io::Error::other("HOME non impostata"))?;
    if let Some(dir) = path.parent() { std::fs::create_dir_all(dir)?; }
    std::fs::write(path, serde_json::to_vec_pretty(cfg)?)
}

pub fn register_database(root: &Path) -> std::io::Result<()> {
    let mut cfg = read_config();
    let root = root.canonicalize().unwrap_or_else(|_| root.to_path_buf());
    if !cfg.databases.contains(&root) {
        cfg.databases.push(root);
        write_config(&cfg)?;
    }
    Ok(())
}

/// I database da consultare per un originale, senza duplicati
pub fn discover(media_path: Option<&str>) -> Vec<PathBuf> {
    let mut out: Vec<PathBuf> = Vec::new();
    let mut push = |p: PathBuf| if p.join("index.json").is_file() && !out.contains(&p) { out.push(p) };
    if let Some(env) = std::env::var_os("MECENA_CLIPDB") {
        for p in std::env::split_paths(&env) { push(p); }
    }
    for p in read_config().databases { push(p); }
    if let Some(media) = media_path {
        let mut dir = Path::new(media.strip_prefix("file://").unwrap_or(media)).parent();
        for _ in 0..6 {
            let Some(d) = dir else { break };
            push(d.join(DB_DIR_NAME));
            dir = d.parent();
        }
    }
    out
}

// ---------------------------------------------------------------------------------------------------------------------
// Mappa di una timeline: quale clip si vede a ogni frame

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq)]
#[serde(default)]
pub struct Segment {
    /// Frame di timeline, inizio incluso, fine esclusa
    pub start: i64,
    pub end: i64,
    pub track: i32,
    /// Percorso dell'originale come lo vede Resolve
    pub media: String,
    /// Facoltativo: la clip nel database, se lo script l'ha già risolta
    pub clip_id: String,
    /// Frame dell'originale mostrato a `start`
    pub source_start: f64,
    /// Frame dell'originale per frame di timeline (2 per 50p a velocità reale su 25p, 1 per un rallenty conformato)
    pub source_rate: f64,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq)]
#[serde(default)]
pub struct Range { pub start: i64, pub end: i64, pub track: i32 }

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct TimelineMap {
    pub format: String,
    pub version: u32,
    pub timeline: String,
    pub fps: f64,
    pub start_frame: i64,
    /// Database delle clip; relativo alla cartella del file, se non assoluto. Vuoto: la cartella sopra `timelines/`
    pub db: String,
    pub adjustments: Vec<Range>,
    pub segments: Vec<Segment>,
}

impl TimelineMap {
    pub fn open(path: &Path) -> Option<Self> {
        let map: Self = serde_json::from_str(&std::fs::read_to_string(path).ok()?).ok()?;
        (map.format == TIMELINE_FORMAT && map.version <= VERSION).then_some(map)
    }

    pub fn db_root(&self, map_path: &Path) -> PathBuf {
        let dir = map_path.parent().unwrap_or(Path::new("."));
        if self.db.is_empty() {
            dir.parent().unwrap_or(dir).to_path_buf()
        } else if Path::new(&self.db).is_absolute() {
            PathBuf::from(&self.db)
        } else {
            dir.join(&self.db)
        }
    }

    /// Il frame di timeline di un tempo OFX. Se è già un frame di timeline (dall'inizio della timeline in poi) resta
    /// quello; altrimenti è relativo all'adjustment clip: l'unico, o quello con la durata che l'host indica
    pub fn timeline_frame(&self, ofx_time: f64, host_range: Option<(f64, f64)>) -> i64 {
        let t = ofx_time.round() as i64;
        if t >= self.start_frame && self.start_frame > 0 {
            return t;
        }
        let rel = t - host_range.map(|r| r.0.round() as i64).filter(|m| *m <= t).unwrap_or(0);
        let adj = match self.adjustments.len() {
            0 => None,
            1 => self.adjustments.first(),
            _ => host_range.and_then(|(a, b)| {
                let dur = (b - a).round() as i64;
                self.adjustments.iter().find(|r| ((r.end - r.start) - dur).abs() <= 1)
            }).or(self.adjustments.first()),
        };
        adj.map(|r| r.start).unwrap_or(self.start_frame) + rel
    }

    /// Il segmento più in alto visibile a quel frame e il frame dell'originale corrispondente
    pub fn at(&self, timeline_frame: i64) -> Option<(&Segment, f64)> {
        self.segments.iter().filter(|s| s.start <= timeline_frame && timeline_frame < s.end).max_by_key(|s| s.track).map(|s| {
            let rate = if s.source_rate.is_finite() && s.source_rate > 0.0 { s.source_rate } else { 1.0 };
            (s, s.source_start + (timeline_frame - s.start) as f64 * rate)
        })
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Lato plugin: ricerca con cache, ricaricata quando il file cambia

#[derive(Debug, Clone, PartialEq)]
pub struct Resolved {
    /// Il `.gyroflow` da caricare al posto dell'originale
    pub project: String,
    /// Frame dell'originale da rendere (solo dalla mappa di timeline)
    pub source_frame: Option<f64>,
    /// Da sommare al frame della clip: un file esterno collegato a una clip interna (`Link`)
    pub frame_offset: f64,
    pub report: String,
}

fn mtime(p: &Path) -> Option<SystemTime> { std::fs::metadata(p).and_then(|m| m.modified()).ok() }

lazy_static::lazy_static! {
    static ref DBS: Mutex<HashMap<PathBuf, (Option<SystemTime>, Arc<ClipDb>)>> = Mutex::new(HashMap::new());
    static ref MAPS: Mutex<HashMap<PathBuf, (Option<SystemTime>, Arc<TimelineMap>)>> = Mutex::new(HashMap::new());
}

fn cached_db(root: &Path) -> Option<Arc<ClipDb>> {
    let stamp = mtime(&root.join("index.json"));
    let mut lock = DBS.lock().ok()?;
    if let Some((t, db)) = lock.get(root) {
        if *t == stamp { return Some(db.clone()); }
    }
    let db = Arc::new(ClipDb::open(root)?);
    lock.insert(root.to_path_buf(), (stamp, db.clone()));
    Some(db)
}

fn cached_map(path: &Path) -> Option<Arc<TimelineMap>> {
    let stamp = mtime(path);
    let mut lock = MAPS.lock().ok()?;
    if let Some((t, map)) = lock.get(path) {
        if *t == stamp { return Some(map.clone()); }
    }
    let map = Arc::new(TimelineMap::open(path)?);
    lock.insert(path.to_path_buf(), (stamp, map.clone()));
    Some(map)
}

fn resolve_in(db: &ClipDb, media: &str, clip_id: &str) -> Option<Resolved> {
    let found = db.get(clip_id).filter(|_| !clip_id.is_empty()).or_else(|| db.find_by_media(media)).map(|e| (e, None));
    let (entry, link) = found.or_else(|| {
        let link = db.find_link(media)?;
        Some((db.get(&link.clip_id)?, Some(link)))
    })?;
    let project = db.project_path(&entry.id);
    project.is_file().then(|| Resolved {
        project: project.to_string_lossy().to_string(),
        source_frame: None,
        frame_offset: link.map(|l| l.frame_offset).unwrap_or_default(),
        report: match link {
            Some(l) => format!("DB Mecena · dati di {} (collegamento {}, offset {:+} frame) · {}", entry.file_name, l.method, l.frame_offset, entry.summary),
            None => format!("DB Mecena · {} · {}", entry.file_name, entry.summary),
        },
    })
}

/// Il progetto della clip di un originale, se un database la conosce
pub fn resolve_media(media_path: &str) -> Option<Resolved> {
    if media_path.is_empty() || media_path.ends_with(".gyroflow") {
        return None;
    }
    discover(Some(media_path)).iter().filter_map(|root| cached_db(root)).find_map(|db| resolve_in(&db, media_path, ""))
}

/// La clip sotto un adjustment clip al tempo OFX dato, dalla timeline attiva
pub fn resolve_timeline(ofx_time: f64, host_range: Option<(f64, f64)>) -> Option<Resolved> {
    let map_path = std::env::var_os("MECENA_CLIPDB_TIMELINE").map(PathBuf::from).or_else(|| read_config().active_timeline)?;
    resolve_timeline_with(&map_path, ofx_time, host_range)
}

pub fn resolve_timeline_with(map_path: &Path, ofx_time: f64, host_range: Option<(f64, f64)>) -> Option<Resolved> {
    let map = cached_map(map_path)?;
    let tf = map.timeline_frame(ofx_time, host_range);
    let (seg, source_frame) = map.at(tf)?;
    let root = map.db_root(map_path);
    let db = cached_db(&root);
    let mut r = db.as_deref().and_then(|db| resolve_in(db, &seg.media, &seg.clip_id))
        .or_else(|| discover(Some(&seg.media)).iter().filter_map(|p| cached_db(p)).find_map(|db| resolve_in(&db, &seg.media, &seg.clip_id)))?;
    r.source_frame = Some(source_frame);
    r.report = format!("Timeline «{}» · frame {tf} · {} · origine {:.0}", map.timeline, r.report, source_frame);
    Some(r)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn entry(id: &str, path: &str) -> IndexEntry {
        IndexEntry { id: id.into(), path: path.into(), file_name: file_name(path), ..Default::default() }
    }

    #[test]
    fn stems_ignore_proxy_suffixes() {
        assert_eq!(normalize_stem("/a/C0001.MP4"), "c0001");
        assert_eq!(normalize_stem("/a/Proxy/C0001_proxy.mov"), "c0001");
        assert_eq!(normalize_stem("C0001-PRX.mxf"), "c0001");
        assert_eq!(normalize_stem("C0001_S001.mov"), "c0001");
        assert_eq!(normalize_stem("DJI_0042.MP4"), "dji_0042");
    }

    #[test]
    fn lookup_prefers_path_then_unique_stem_then_folders() {
        let db = ClipDb { root: "/db".into(), index: Index { format: INDEX_FORMAT.into(), version: 1, clips: vec![
            entry("a", "/shoot/day1/cardA/C0001.MP4"),
            entry("b", "/shoot/day1/cardB/C0001.MP4"),
            entry("c", "/shoot/day1/cardA/C0002.MP4"),
        ], links: Vec::new() } };
        assert_eq!(db.find_by_media("/shoot/day1/cardB/C0001.MP4").unwrap().id, "b");
        assert_eq!(db.find_by_media("/proxy/C0002_proxy.mov").unwrap().id, "c");
        assert_eq!(db.find_by_media("/proxy/cardA/C0001_proxy.mov").unwrap().id, "a");
        // Two cards, nothing to tell them apart
        assert!(db.find_by_media("/proxy/C0001_proxy.mov").is_none());
    }

    #[test]
    fn external_recordings_use_the_linked_clip() {
        let dir = std::env::temp_dir().join(format!("clipdb-link-{}", std::process::id()));
        let mut db = ClipDb::open_or_create(&dir);
        db.upsert(entry("int-1", "/cards/A/C0001.MP4"));
        db.set_link(Link { external: "/ninja/RAW_0001.mov".into(), clip_id: "int-1".into(), frame_offset: 12.0, method: "timecode".into(), ..Default::default() });
        db.set_link(Link { external: "/ninja/RAW_0001.mov".into(), clip_id: "int-1".into(), frame_offset: 13.0, method: "manuale".into(), ..Default::default() });
        assert_eq!(db.index.links.len(), 1);
        let project = db.project_path("int-1");
        std::fs::create_dir_all(project.parent().unwrap()).unwrap();
        std::fs::write(&project, "{}").unwrap();
        let r = resolve_in(&db, "/Volumes/X/RAW_0001.mov", "").unwrap();
        assert_eq!(r.frame_offset, 13.0);
        assert!(resolve_in(&db, "/ninja/RAW_0002.mov", "").is_none());
        std::fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn timeline_maps_frames_and_rates() {
        let map = TimelineMap {
            format: TIMELINE_FORMAT.into(), version: 1, timeline: "T".into(), fps: 25.0, start_frame: 90000, db: String::new(),
            adjustments: vec![Range { start: 90000, end: 90500, track: 3 }],
            segments: vec![
                Segment { start: 90000, end: 90100, track: 1, media: "/a.mp4".into(), source_start: 10.0, source_rate: 2.0, ..Default::default() },
                Segment { start: 90050, end: 90200, track: 2, media: "/b.mp4".into(), source_start: 0.0, source_rate: 1.0, ..Default::default() },
            ],
        };
        // Relative to the adjustment clip
        assert_eq!(map.timeline_frame(10.0, Some((0.0, 500.0))), 90010);
        // Absolute timeline frame
        assert_eq!(map.timeline_frame(90020.0, None), 90020);
        let (s, f) = map.at(90010).unwrap();
        assert_eq!((s.media.as_str(), f), ("/a.mp4", 30.0));
        // The upper track wins where they overlap
        let (s, f) = map.at(90060).unwrap();
        assert_eq!((s.media.as_str(), f), ("/b.mp4", 10.0));
        assert!(map.at(90300).is_none());
        assert_eq!(map.db_root(Path::new("/x/MecenaClipDB/timelines/T.json")), PathBuf::from("/x/MecenaClipDB"));
    }

    #[test]
    fn fingerprint_and_id_are_stable() {
        let dir = std::env::temp_dir().join(format!("clipdb-test-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let f = dir.join("C0001.MP4");
        std::fs::write(&f, vec![7u8; 200_000]).unwrap();
        let a = fingerprint_file(&f).unwrap();
        assert_eq!(a, fingerprint_file(&f).unwrap());
        assert!(clip_id("C0001.MP4", &a).starts_with("c0001-"));
        let mut db = ClipDb::open_or_create(&dir.join(DB_DIR_NAME));
        db.upsert(IndexEntry { id: clip_id("C0001.MP4", &a), path: f.to_string_lossy().into(), file_name: "C0001.MP4".into(), ..Default::default() });
        db.save().unwrap();
        let project = db.project_path(&db.index.clips[0].id);
        std::fs::create_dir_all(project.parent().unwrap()).unwrap();
        std::fs::write(&project, "{}").unwrap();
        let r = resolve_media(&f.to_string_lossy()).unwrap();
        assert_eq!(r.project, project.to_string_lossy());
        std::fs::remove_dir_all(dir).unwrap();
    }
}

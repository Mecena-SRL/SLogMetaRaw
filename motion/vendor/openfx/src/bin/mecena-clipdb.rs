// SPDX-License-Identifier: GPL-3.0-or-later
//! mecena-clipdb: estrae da ogni originale giroscopio, accelerometro, posa, readout, sensore e ottica nel database
//! `MecenaClipDB` (formato in `gyroflow_plugin_base::clipdb`). Usa lo stesso parser e le stesse calibrazioni di Stabilize.
//!
//!   mecena-clipdb scan <database> <cartella|file>... [--force]
//!   mecena-clipdb find <database> <file>
//!   mecena-clipdb link <database> <file esterno> <clip interna|id> [--offset N]
//!                                                 il RAW di un registratore usa i dati della clip interna
//!   mecena-clipdb timeline <mappa.json>          rende attiva una mappa di timeline per gli adjustment clip
//!   mecena-clipdb list

use gyroflow_plugin_base::clipdb::{ self, ClipDb, IndexEntry };
use gyroflow_plugin_base::gyroflow_core::{ self, StabilizationManager, filesystem };
use gyroflow_plugin_base::serde_json::{ self, json, Value };
use std::fmt::Write as _;
use std::path::{ Path, PathBuf };

const VIDEO_EXT: &[&str] = &["mp4", "mov", "mxf", "m4v", "insv", "360", "braw", "r3d", "mkv", "avi", "crm"];

fn usage() -> ! {
    eprintln!("Uso:\n  mecena-clipdb scan <database> <cartella|file>... [--force]\n  mecena-clipdb find <database> <file>\n  mecena-clipdb link <database> <file esterno> <clip interna|id> [--offset N]\n  mecena-clipdb timeline <mappa.json>\n  mecena-clipdb list");
    std::process::exit(2)
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let code = match args.first().map(String::as_str) {
        Some("scan") if args.len() >= 3 => scan(Path::new(&args[1]), &args[2..]),
        Some("find") if args.len() == 3 => {
            let db = ClipDb::open(Path::new(&args[1])).unwrap_or_else(|| { eprintln!("Database non valido: {}", args[1]); std::process::exit(1) });
            match db.find_by_media(&args[2]) {
                Some(e) => { println!("{}\n{}", db.project_path(&e.id).display(), e.summary); 0 }
                None => { eprintln!("Nessuna clip per {}", args[2]); 1 }
            }
        }
        Some("link") if args.len() >= 4 => link(Path::new(&args[1]), &args[2], &args[3], &args[4..]),
        Some("timeline") if args.len() == 2 => {
            let path = std::fs::canonicalize(&args[1]).unwrap_or_else(|_| PathBuf::from(&args[1]));
            match clipdb::TimelineMap::open(&path) {
                Some(map) => {
                    let mut cfg = clipdb::read_config();
                    cfg.active_timeline = Some(path.clone());
                    clipdb::write_config(&cfg).expect("configurazione non scrivibile");
                    println!("Timeline attiva: «{}», {} segmenti, {} adjustment clip", map.timeline, map.segments.len(), map.adjustments.len());
                    0
                }
                None => { eprintln!("Mappa non valida: {}", path.display()); 1 }
            }
        }
        Some("list") => {
            let cfg = clipdb::read_config();
            for d in &cfg.databases { println!("{}", d.display()); }
            if let Some(t) = &cfg.active_timeline { println!("timeline attiva: {}", t.display()); }
            0
        }
        _ => usage(),
    };
    std::process::exit(code);
}

fn link(root: &Path, external: &str, internal: &str, rest: &[String]) -> i32 {
    let offset = match rest {
        [] => 0.0,
        [flag, v] if flag == "--offset" => match v.parse::<f64>() { Ok(v) => v, Err(_) => usage() },
        _ => usage(),
    };
    let Some(mut db) = ClipDb::open(root) else { eprintln!("Database non valido: {}", root.display()); return 1 };
    let Some(entry) = db.get(internal).or_else(|| db.find_by_media(internal)).cloned() else {
        eprintln!("La clip interna non è nel database: esegui prima `scan` sugli originali");
        return 1
    };
    let external = std::fs::canonicalize(external).map(|p| p.to_string_lossy().to_string()).unwrap_or_else(|_| external.to_string());
    db.set_link(clipdb::Link { external: external.clone(), clip_id: entry.id.clone(), frame_offset: offset, method: "manuale".into(), note: String::new() });
    if let Err(e) = db.save() { eprintln!("index.json: {e}"); return 1; }
    println!("{external} → {} (offset {offset:+} frame)", entry.file_name);
    0
}

fn collect(path: &Path, out: &mut Vec<PathBuf>) {
    if path.is_dir() {
        if path.file_name().is_some_and(|n| n == clipdb::DB_DIR_NAME || n.to_string_lossy().starts_with('.')) {
            return;
        }
        if let Ok(rd) = std::fs::read_dir(path) {
            let mut entries: Vec<_> = rd.flatten().map(|e| e.path()).collect();
            entries.sort();
            for p in entries { collect(&p, out); }
        }
    } else if path.extension().map(|e| e.to_string_lossy().to_lowercase()).is_some_and(|e| VIDEO_EXT.contains(&e.as_str()))
        && !path.file_name().is_some_and(|n| n.to_string_lossy().starts_with("._")) {
        out.push(path.to_path_buf());
    }
}

fn scan(root: &Path, inputs: &[String]) -> i32 {
    let force = inputs.iter().any(|a| a == "--force");
    let mut files = Vec::new();
    for i in inputs.iter().filter(|a| *a != "--force") {
        collect(Path::new(i), &mut files);
    }
    let mut db = ClipDb::open_or_create(root);
    let (mut done, mut skipped, mut failed) = (0, 0, 0);
    for (n, file) in files.iter().enumerate() {
        let file = std::fs::canonicalize(file).unwrap_or_else(|_| file.clone());
        let path = file.to_string_lossy().to_string();
        let Ok(fingerprint) = clipdb::fingerprint_file(&file) else { failed += 1; eprintln!("[{}/{}] illeggibile: {path}", n + 1, files.len()); continue };
        let name = file.file_name().unwrap_or_default().to_string_lossy().to_string();
        let id = clipdb::clip_id(&name, &fingerprint);
        if !force && db.get(&id).is_some_and(|e| e.fingerprint == fingerprint) && db.project_path(&id).is_file() {
            // Same content, maybe moved: only the path is updated
            let mut e = db.get(&id).cloned().unwrap();
            e.path = path;
            db.upsert(e);
            skipped += 1;
            continue;
        }
        match extract(&db.clip_dir(&id), &file, &id, &fingerprint) {
            Ok(mut entry) => {
                if let Some(old) = db.get(&id).or_else(|| db.index.clips.iter().find(|c| c.path == entry.path)) {
                    entry.start_tc = old.start_tc.clone();
                }
                println!("[{}/{}] {name}: {}", n + 1, files.len(), entry.summary);
                db.upsert(entry);
                done += 1;
                // Saved as it goes: an interrupted scan keeps what it read
                if let Err(e) = db.save() { eprintln!("index.json: {e}"); return 1; }
            }
            Err(e) => { failed += 1; eprintln!("[{}/{}] {name}: {e}", n + 1, files.len()); }
        }
    }
    if let Err(e) = db.save() { eprintln!("index.json: {e}"); return 1; }
    let _ = clipdb::register_database(root);
    println!("{done} estratte, {skipped} già aggiornate, {failed} senza dati movimento o illeggibili · {}", root.display());
    if failed > 0 && done + skipped == 0 { 1 } else { 0 }
}

fn opt<T: std::fmt::Display>(v: Option<T>) -> String { v.map(|v| v.to_string()).unwrap_or_default() }

fn range(values: impl Iterator<Item = f64>) -> Option<(f64, f64)> {
    values.filter(|v| v.is_finite()).fold(None, |acc, v| Some(match acc { None => (v, v), Some((a, b)) => (a.min(v), b.max(v)) }))
}

fn extract(dir: &Path, file: &Path, id: &str, fingerprint: &str) -> Result<IndexEntry, String> {
    let url = filesystem::path_to_url(&file.to_string_lossy());
    let stab = StabilizationManager::default();
    let md = {
        let mut f = filesystem::open_file(&url, false, false).map_err(|e| format!("{e:?}"))?;
        let size = f.size;
        stab.load_video_file(f.get_file(), size, &url, None, true).map_err(|e| format!("{e:?}"))?
    };
    {
        let gyro = stab.gyro.read();
        if gyro.quaternions.is_empty() && gyro.file_metadata.read().raw_imu.is_empty() {
            // Not indexed: a file without motion data (an external recorder's) takes it from a clip through a link
            return Err("nessun dato movimento".into());
        }
    }
    std::fs::create_dir_all(dir).map_err(|e| e.to_string())?;

    let project = stab.export_gyroflow_data(gyroflow_core::GyroflowProjectType::WithGyroData, "{}", None).map_err(|e| format!("{e:?}"))?;
    std::fs::write(dir.join(clipdb::PROJECT_FILE), project).map_err(|e| e.to_string())?;

    let gyro = stab.gyro.read();
    let meta = gyro.file_metadata.read();
    let params = stab.params.read();
    let camera = stab.camera_id.read().clone().or_else(|| meta.camera_identifier.clone());
    let fps = if params.fps > 0.0 { params.fps } else { md.fps };

    // imu.csv
    let mut s = String::from("t_ms,gyro_x_dps,gyro_y_dps,gyro_z_dps,accl_x,accl_y,accl_z,magn_x,magn_y,magn_z\n");
    let (mut has_gyro, mut has_accl, mut has_magn) = (false, false, false);
    for x in &meta.raw_imu {
        let v = |o: Option<[f64; 3]>, flag: &mut bool| o.map(|a| { *flag = true; format!("{},{},{}", a[0], a[1], a[2]) }).unwrap_or_else(|| ",,".into());
        let _ = writeln!(s, "{:.4},{},{},{}", x.timestamp_ms, v(x.gyro, &mut has_gyro), v(x.accl, &mut has_accl), v(x.magn, &mut has_magn));
    }
    std::fs::write(dir.join("imu.csv"), s).map_err(|e| e.to_string())?;
    let imu_rate = match (meta.raw_imu.first(), meta.raw_imu.last()) {
        (Some(a), Some(b)) if b.timestamp_ms > a.timestamp_ms => (meta.raw_imu.len() - 1) as f64 * 1000.0 / (b.timestamp_ms - a.timestamp_ms),
        _ => 0.0,
    };

    // orientation.csv
    let mut s = String::from("t_ms,w,x,y,z\n");
    for (ts, q) in gyro.quaternions.iter() {
        let c = q.quaternion().coords;
        let _ = writeln!(s, "{:.4},{},{},{},{}", *ts as f64 / 1000.0, c[3], c[0], c[1], c[2]);
    }
    std::fs::write(dir.join("orientation.csv"), s).map_err(|e| e.to_string())?;

    // lens.csv
    let mut s = String::from("t_ms,focal_mm,focus_m,f_stop,t_stop,zoom_ring_pct,pixel_focal_x,pixel_focal_y,principal_x,principal_y,sensor_w_px,sensor_h_px,pixel_pitch_x_nm,pixel_pitch_y_nm\n");
    for (ts, l) in &meta.lens_params {
        let _ = writeln!(s, "{:.4},{},{},{},{},{},{},{},{},{},{},{},{},{}", *ts as f64 / 1000.0,
            opt(l.focal_length), opt(l.focus_distance), opt(l.iris_fstop), opt(l.iris_tstop), opt(l.zoom_ring_position),
            opt(l.pixel_focal_length.map(|p| p.0)), opt(l.pixel_focal_length.map(|p| p.1)),
            opt(l.principal_point.map(|p| p.0)), opt(l.principal_point.map(|p| p.1)),
            opt(l.sensor_size_px.map(|p| p.0)), opt(l.sensor_size_px.map(|p| p.1)),
            opt(l.pixel_pitch.map(|p| p.0)), opt(l.pixel_pitch.map(|p| p.1)));
    }
    std::fs::write(dir.join("lens.csv"), s).map_err(|e| e.to_string())?;

    // frames.csv
    let mut s = String::from("frame,t_ms,capture_offset_ms\n");
    for f in 0..params.frame_count {
        let _ = writeln!(s, "{f},{:.4},{}", f as f64 * 1000.0 / fps.max(1e-6), opt(meta.per_frame_time_offsets.get(f)));
    }
    std::fs::write(dir.join("frames.csv"), s).map_err(|e| e.to_string())?;

    // metadata.json: what the file says, without the series above
    let thin = serde_json::to_value(meta.thin()).unwrap_or(Value::Null);
    std::fs::write(dir.join("metadata.json"), serde_json::to_vec_pretty(&thin).unwrap_or_default()).map_err(|e| e.to_string())?;

    let lens = meta.lens_params.values();
    let focal = range(lens.clone().filter_map(|l| l.focal_length.map(f64::from)));
    let focus = range(lens.clone().filter_map(|l| l.focus_distance.map(f64::from)).filter(|d| *d > 0.0));
    let fstop = range(lens.clone().filter_map(|l| l.iris_fstop.map(f64::from)));
    let zoom = range(lens.clone().filter_map(|l| l.zoom_ring_position.map(f64::from)));
    let sensor_px = lens.clone().find_map(|l| l.sensor_size_px);
    let pitch = lens.clone().find_map(|l| l.pixel_pitch);
    let sensor_mm = sensor_px.zip(pitch).map(|(s, p)| (s.0 as f64 * p.0 as f64 / 1e6, s.1 as f64 * p.1 as f64 / 1e6));
    let exposure_ms = meta.additional_data.get("mecena_stable_exposure_ms").and_then(Value::as_f64);
    let shutter_deg = exposure_ms.map(|ms| ms * fps * 0.36);

    let size = std::fs::metadata(file).map(|m| m.len()).unwrap_or_default();
    let record = json!({
        "format": clipdb::INDEX_FORMAT, "version": clipdb::VERSION, "id": id,
        "source": { "path": file, "file_name": file.file_name().map(|n| n.to_string_lossy().to_string()), "size": size, "fingerprint": fingerprint },
        "video": { "width": md.width, "height": md.height, "fps": fps, "duration_ms": params.duration_ms, "frame_count": params.frame_count, "rotation": md.rotation },
        "camera": {
            "detected_source": meta.detected_source,
            "brand": camera.as_ref().map(|c| c.brand.clone()), "model": camera.as_ref().map(|c| c.model.clone()),
            "lens_model": camera.as_ref().map(|c| c.lens_model.clone()), "lens_info": camera.as_ref().map(|c| c.lens_info.clone()),
            "setting": camera.as_ref().map(|c| c.camera_setting.clone()), "identifier": camera.as_ref().map(|c| c.identifier.clone()),
        },
        "capture": {
            "exposure_ms": exposure_ms, "shutter_angle_deg": shutter_deg,
            "readout_ms": meta.frame_readout_time, "readout_direction": serde_json::to_value(&meta.frame_readout_direction).ok(),
            "has_accurate_timestamps": meta.has_accurate_timestamps,
        },
        "imu": {
            "orientation": meta.imu_orientation, "samples": meta.raw_imu.len(), "rate_hz": imu_rate,
            "has_gyro": has_gyro, "has_accelerometer": has_accl, "has_magnetometer": has_magn,
            "orientations": gyro.quaternions.len(), "has_gravity_vectors": meta.gravity_vectors.is_some(),
            "camera_stab_frames": meta.camera_stab_data.len(),
        },
        "lens": {
            "profile": meta.lens_profile.as_ref().and_then(|p| p.get("name").cloned()),
            "samples": meta.lens_params.len(), "digital_zoom": meta.digital_zoom,
            "focal_mm": focal, "focus_m": focus, "f_stop": fstop, "zoom_ring_pct": zoom,
            "sensor_px": sensor_px, "pixel_pitch_nm": pitch, "sensor_mm": sensor_mm,
            "breathing_frames": meta.lens_breathing.len(),
        },
        "files": { "project": clipdb::PROJECT_FILE, "imu": "imu.csv", "orientation": "orientation.csv", "lens": "lens.csv", "frames": "frames.csv", "metadata": "metadata.json" },
    });
    std::fs::write(dir.join("clip.json"), serde_json::to_vec_pretty(&record).unwrap_or_default()).map_err(|e| e.to_string())?;

    let mut summary = vec![meta.detected_source.clone().unwrap_or_else(|| "Camera non identificata".into()),
        format!("{}×{}", md.width, md.height), format!("{fps:.3} fps")];
    if let (Some(ms), Some(deg)) = (exposure_ms, shutter_deg) { summary.push(format!("1/{:.0} s ({deg:.0}°)", 1000.0 / ms)); }
    if let Some((a, b)) = focal { summary.push(if (b - a).abs() < 0.05 { format!("{a:.1} mm") } else { format!("{a:.1}–{b:.1} mm") }); }
    if let Some((a, b)) = focus { summary.push(format!("fuoco {a:.2}–{b:.2} m")); }
    if let Some((w, h)) = sensor_mm { summary.push(format!("sensore {w:.1}×{h:.1} mm")); }
    if let Some(r) = meta.frame_readout_time { summary.push(format!("readout {r:.2} ms")); }
    summary.push(format!("{} IMU", meta.raw_imu.len()));

    Ok(IndexEntry {
        id: id.into(), path: file.to_string_lossy().into(), file_name: file.file_name().unwrap_or_default().to_string_lossy().into(),
        size, fingerprint: fingerprint.into(), fps, frame_count: params.frame_count as u64, width: md.width as u32, height: md.height as u32,
        has_gyro: !gyro.quaternions.is_empty(), summary: summary.join(" · "), start_tc: String::new(),
        scanned_at: std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or_default(),
    })
}

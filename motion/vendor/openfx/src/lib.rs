// SPDX-License-Identifier: GPL-3.0-or-later
extern crate ofx;
use ofx::*;
use std::sync::Once;

mod gyroflow;

unsafe extern "C" {
    fn mecena_shutter_descriptor() -> *const OfxPlugin;
}

fn initialize() {
    static REGISTER: Once = Once::new();
    REGISTER.call_once(|| init_registry(|registry| {
        register_plugin!(registry, gyroflow);
    }));
}

#[unsafe(no_mangle)]
pub extern "C" fn OfxGetNumberOfPlugins() -> Int {
    initialize();
    2
}

#[unsafe(no_mangle)]
pub extern "C" fn OfxGetPlugin(index: Int) -> *const OfxPlugin {
    initialize();
    match index {
        0 => get_registry().ofx_plugin(0),
        1 => unsafe { mecena_shutter_descriptor() },
        _ => std::ptr::null(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mecena_source_path_property() -> *const std::ffi::c_char {
    <ofx::SrcFilePath::Property as ofx::Named>::name().as_ptr().cast()
}

// The sensor and container can run at different rates in off-speed recordings.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SourceRates {
    capture: f64,
    file: f64,
    exposure_ms: f64,
}

fn read_source_rates(path: &str) -> Option<SourceRates> {
    use gyroflow_plugin_base::{filesystem, gyroflow_core::{util, gyro_source::GyroSource}};
    use std::{io::{Seek, SeekFrom}, sync::{Arc, atomic::AtomicBool}};
    let url = filesystem::path_to_url(path);
    let mut file = filesystem::open_file(&url, false, false).ok()?;
    let size = file.size;
    let stream = file.get_file();
    let video = util::get_video_metadata(&mut *stream, size, &url).ok()?;
    let metadata = stream.seek(SeekFrom::Start(0)).ok().and_then(|_| {
        GyroSource::parse_telemetry_file(&mut *stream, size, &url, &Default::default(),
            (video.width, video.height), video.fps, |_| (), Arc::new(AtomicBool::new(false)))
            .ok()
    });
    Some(SourceRates {
        capture: metadata.as_ref().and_then(|m| m.frame_rate).filter(|v| v.is_finite() && *v > 0.0).unwrap_or_default(),
        file: video.fps,
        exposure_ms: metadata.as_ref().and_then(|m| m.additional_data.get("mecena_stable_exposure_ms")?.as_f64()).unwrap_or_default(),
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn mecena_source_rates(path: *const std::ffi::c_char) -> SourceRates {
    use std::{ffi::CStr, path::Path, sync::{Mutex, OnceLock}, time::SystemTime};
    type Entry = (String, u64, Option<SystemTime>, SourceRates);
    static CACHE: OnceLock<Mutex<Vec<Entry>>> = OnceLock::new();
    if path.is_null() { return SourceRates::default(); }
    std::panic::catch_unwind(|| {
        let Ok(path) = unsafe { CStr::from_ptr(path) }.to_str() else { return SourceRates::default(); };
        let Ok(info) = Path::new(path).metadata() else { return SourceRates::default(); };
        let modified = info.modified().ok();
        let mut cache = CACHE.get_or_init(Mutex::default).lock().unwrap_or_else(|e| e.into_inner());
        if let Some(entry) = cache.iter().find(|e| e.0 == path && e.1 == info.len() && e.2 == modified) {
            return entry.3;
        }
        let rates = read_source_rates(path).unwrap_or_default();
        if cache.len() >= 64 { cache.remove(0); }
        cache.push((path.to_owned(), info.len(), modified, rates));
        rates
    }).unwrap_or_default()
}

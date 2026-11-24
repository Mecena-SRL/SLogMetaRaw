// SPDX-License-Identifier: GPL-3.0-or-later
use std::{env, path::PathBuf, process::Command};

fn run(command: &mut Command) {
    let status = command.status().expect("cannot start native compiler/archiver");
    assert!(status.success(), "failed build command: {command:?}");
}

fn main() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..");
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let obj = out.join("shutter.o");
    let archive = out.join("libmecena_shutter.a");
    let mac = env::var("CARGO_CFG_TARGET_OS").unwrap() == "macos";
    let mut cxx = Command::new(env::var_os("CXX").unwrap_or_else(|| "c++".into()));
    cxx.args(["-std=c++17", "-O3", "-fPIC", "-fvisibility=hidden"]);
    if mac {
        match env::var("CARGO_CFG_TARGET_ARCH").unwrap().as_str() {
            "aarch64" => { cxx.args(["-arch", "arm64"]); }
            "x86_64" => { cxx.args(["-arch", "x86_64"]); }
            _ => panic!("unsupported macOS architecture"),
        }
    }
    run(cxx.arg("-I").arg(root.join("SDK")).arg("-c").arg(root.join("src/plugin.cpp")).arg("-o").arg(&obj));
    let mut ar=Command::new("ar");
    ar.arg("rcs").arg(&archive).arg(&obj);
    if mac {
        let metal_obj=out.join("shutter_metal.o");
        let mut mm=Command::new(env::var_os("CXX").unwrap_or_else(|| "c++".into()));
        mm.args(["-std=c++17","-O3","-fobjc-arc","-fPIC","-fvisibility=hidden","-arch",
            if env::var("CARGO_CFG_TARGET_ARCH").as_deref()==Ok("aarch64") {"arm64"} else {"x86_64"}]);
        run(mm.arg("-c").arg(root.join("src/shutter_metal.mm")).arg("-o").arg(&metal_obj));
        ar.arg(metal_obj);
        println!("cargo:rustc-link-lib=framework=Metal");
        println!("cargo:rustc-link-lib=framework=Foundation");
        println!("cargo:rustc-link-lib=objc");
    }
    run(&mut ar);
    println!("cargo:rustc-link-search=native={}",out.display());
    println!("cargo:rustc-link-lib=static=mecena_shutter");
    println!("cargo:rustc-link-lib={}",if mac {"c++"} else {"stdc++"});
    for src in ["src/plugin.cpp","src/motion.hpp","src/optical.hpp","src/camera.hpp","src/exposure.hpp","src/shutter_metal.mm","src/blur.hpp","src/blur_core.inc","SDK"] {
        println!("cargo:rerun-if-changed={}",root.join(src).display());
    }
}

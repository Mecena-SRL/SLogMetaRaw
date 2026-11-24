#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -s)" == Darwin ]] || { echo "Questa build richiede macOS e il suo SDK."; exit 1; }
target_dir=$(mktemp -d)
trap 'rm -rf "$target_dir"' EXIT
export CARGO_TARGET_DIR="$target_dir/target"
# Patch only a private build checkout. The installed bundle cannot consult the user's lens database.
src="$target_dir/src"
mkdir -p "$src"
cp -R SDK src vendor "$src/"
core="$src/gyroflow"
rev=a298bec3844c0bf070ad001080ec67a411a68791
git clone --quiet --no-checkout "${MECENA_CORE_REPO:-https://github.com/gyroflow/gyroflow.git}" "$core"
git -C "$core" checkout --quiet --detach "$rev"
git -C "$core" submodule update --init --recursive
# Normalize the patched files in this disposable checkout before git apply.
python3 - "$core" <<'PYLINES'
from pathlib import Path
import sys
for name in ("lens_profile_database.rs", "lib.rs", "stabilization/mod.rs", "stabilization/frame_transform.rs", "gyro_source/mod.rs"):
    path = Path(sys.argv[1]) / "src/core" / name
    data = path.read_bytes()
    if b"\r\n" in data:
        path.write_bytes(data.replace(b"\r\n", b"\n"))
PYLINES
git -C "$core" apply --check "$PWD/release/embedded-only.patch"
git -C "$core" apply "$PWD/release/embedded-only.patch"
git -C "$core" apply --check "$PWD/release/master.patch"
git -C "$core" apply "$PWD/release/master.patch"
git -C "$core" apply --check "$PWD/release/shutter-time.patch"
git -C "$core" apply "$PWD/release/shutter-time.patch"
python3 - "$src" <<'PYCORE'
import pathlib,sys
root=pathlib.Path(sys.argv[1])
p=root/'vendor/common/Cargo.toml'
s=p.read_text().replace('git = "https://github.com/gyroflow/gyroflow.git", default-features = false, rev = "a298bec"', 'path = "../../gyroflow/src/core", default-features = false')
p.write_text(s)
p=root/'vendor/openfx/Cargo.lock'
p.write_text(''.join(l for l in p.read_text().splitlines(True) if not l.startswith('source = "git+https://github.com/gyroflow/gyroflow.git?rev=a298bec#')))
PYCORE
cargo build --locked --release --manifest-path "$src/vendor/openfx/Cargo.toml"
bundle="$target_dir/MecenaMotion.ofx.bundle"
bin="$bundle/Contents/MacOS/MecenaMotion.ofx"
mkdir -p "$(dirname "$bin")" "$bundle/Contents/Resources"
cp "$CARGO_TARGET_DIR/release/libgyroflow_ofx.dylib" "$bin"
# Clip database extractor and its Resolve script, beside the plugin: the script finds the tool there
cp "$CARGO_TARGET_DIR/release/mecena-clipdb" "$bundle/Contents/Resources/"
cp "tools/resolve/Mecena ClipDB.py" "$bundle/Contents/Resources/"
cp release/Info.plist "$bundle/Contents/"
cp LICENSE "$bundle/Contents/Resources/GPL-3.txt"
# Icone degli effetti: OFX le cerca come <identificativo plugin>.png/.svg in Resources.
cp assets/icons/it.mecena.motion.*.png assets/icons/it.mecena.motion.*.svg "$bundle/Contents/Resources/"
for dep in $(otool -L "$bin" | tail -n +2 | awk '{print $1}'); do
    case "$dep" in /System/Library/*|/usr/lib/*) ;; *)
        # A linked dylib lists itself as LC_ID_DYLIB; that entry is not a dependency.
        [[ "$dep" == "$(otool -D "$bin" | tail -1)" ]] || { echo "Dipendenza esterna: $dep"; exit 2; }
    esac
done
# Con MECENA_APP_SIGN="Developer ID Application: …" il bundle è firmato per la notarizzazione.
if [[ -n "${MECENA_APP_SIGN:-}" ]]; then
    sign=(--force --options runtime --timestamp --sign "$MECENA_APP_SIGN")
else
    sign=(--force --sign -)
fi
codesign "${sign[@]}" "$bundle/Contents/Resources/mecena-clipdb"
codesign "${sign[@]}" "$bundle"
codesign --verify --strict "$bundle"
c++ -std=c++17 -O2 -I SDK tests/bundle.cpp -o "$target_dir/check"
"$target_dir/check" "$bin"
c++ -std=c++17 -O2 -DTEST_PACKAGED_OFX -I SDK -I src tests/host.cpp -o "$target_dir/shutter"
"$target_dir/shutter" "$bin"
c++ -std=c++17 -O3 -DTEST_PACKAGED_OFX -I SDK -I src tests/cinema.cpp -o "$target_dir/cinema"
"$target_dir/cinema" "$bin"
mkdir -p dist
c++ -std=c++17 -O3 -fobjc-arc -I src src/shutter_metal.mm tests/metal_benchmark.mm -framework Metal -framework Foundation -o "$target_dir/metal-check"
"$target_dir/metal-check" | tee dist/Shutter-Metal.txt
version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" release/Info.plist)
ditto -c -k --keepParent "$bundle" "dist/MecenaMotion-$version-$(uname -m).zip"
bash release/make-pkg.sh "$bundle" "$version" dist
cp "release/installer/Disinstalla Mecena Motion.command" dist/

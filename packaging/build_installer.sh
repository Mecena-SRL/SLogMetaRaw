#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds dist/SLogMetaRaw-<version>.dmg with the installer (.pkg), the illustrated guides (at most eleven pages) and the
# uninstaller, and dist/LogMetaRawAudio-<version>.dmg: the audio plugins alone (VST3 and AU, for Logic and other DAWs)
# with their own guide and uninstaller.
#   ./packaging/build_installer.sh
set -euo pipefail
export COPYFILE_DISABLE=1   # no AppleDouble ._ files in the package
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG_DIR="$ROOT/packaging"
VERSION="$(python3 -c 'import sys; sys.path.insert(0, sys.argv[1]); import slogmetaraw; print(slogmetaraw.__version__)' "$ROOT")"
BUILD="$ROOT/build/installer"
DIST="$ROOT/dist"
# Mecena Motion (Stabilize, Shutter, mecena-clipdb), the same package as Log MetaRaw: built from motion/
# (Rust and the Gyroflow core, see motion/release/build-mac.sh). Apple silicon only.
MOTION="$ROOT/motion"
MOTION_VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$MOTION/release/Info.plist")"
rm -rf "$BUILD" && mkdir -p "$BUILD/root" "$BUILD/resources" "$BUILD/dmg" "$DIST"

echo "1/7 plugin"
make -C "$ROOT/ofx/SLogMetaRaw" >/dev/null

echo "2/7 icone e guida"
swift "$ROOT/tools/make_icons.swift" "$ROOT/assets" >/dev/null
# .icns for the installer package and the disk image
ICONSET="$BUILD/SLogMetaRaw.iconset"; mkdir -p "$ICONSET"
for s in 16 32 128 256 512; do
  sips -z $s $s "$ROOT/assets/icon_1024.png" --out "$ICONSET/icon_${s}x${s}.png" >/dev/null
  sips -z $((s*2)) $((s*2)) "$ROOT/assets/icon_1024.png" --out "$ICONSET/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$ROOT/assets/SLogMetaRaw.icns"
cp "$ROOT/assets/effect_256.png" "$ROOT/ofx/SLogMetaRaw/com.slogmetaraw.SLogMetaRaw.png"
cp "$ROOT/assets/detail_256.png" "$ROOT/ofx/SLogMetaRaw/com.slogmetaraw.SLogMetaRawDetail.png"
cp "$ROOT/assets/noise_256.png" "$ROOT/ofx/SLogMetaRaw/com.slogmetaraw.SLogMetaRawNoise.png"
# Stabilize and Motion Blur keep their plugin identifiers (saved projects): the icons take those names
cp "$ROOT/assets/stabilize_256.png" "$ROOT/motion/assets/icons/it.mecena.motion.stabilize.png"
cp "$ROOT/assets/motionblur_256.png" "$ROOT/motion/assets/icons/it.mecena.motion.shutter.png"
make -C "$ROOT/ofx/SLogMetaRaw" >/dev/null
cp "$ROOT/assets/icon_1024.png" "$PKG_DIR/guide/icon.png"
# the guides carry @VERSION@ like the installer pages: print a substituted copy (with img/, the figures of
# tools/make_guide_figures.py)
GUIDE="$BUILD/guide"; mkdir -p "$GUIDE"
cp -R "$PKG_DIR/guide/"* "$GUIDE/"
sed -i '' "s/@VERSION@/$VERSION/g" "$GUIDE/guida.html" "$GUIDE/guide-en.html" "$GUIDE/guida-audio.html" \
    "$GUIDE/guide-audio-en.html"
# find any Chromium-based browser for headless PDF generation
CHROME=""
for app in \
  "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
  "/Applications/Chromium.app/Contents/MacOS/Chromium" \
  "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge" \
  "/Applications/Brave Browser.app/Contents/MacOS/Brave Browser" \
  "$HOME/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"; do
  if [[ -x "$app" ]]; then CHROME="$app"; break; fi
done
if [[ -z "$CHROME" ]]; then
  echo "ERRORE: nessun browser Chromium trovato per generare le guide PDF (installa Chrome/Chromium/Edge/Brave)." >&2
  exit 1
fi
"$CHROME" --headless=new --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$BUILD/dmg/Guida Log MetaRaw (italiano).pdf" "file://$GUIDE/guida.html" 2>/dev/null
"$CHROME" --headless=new --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$BUILD/dmg/Log MetaRaw Guide (english).pdf" "file://$GUIDE/guide-en.html" 2>/dev/null
# the audio guides go on the audio-only disk (four pages each)
mkdir -p "$BUILD/audio-dmg"
"$CHROME" --headless=new --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$BUILD/audio-dmg/Guida Log MetaRaw Audio (italiano).pdf" "file://$GUIDE/guida-audio.html" 2>/dev/null
"$CHROME" --headless=new --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$BUILD/audio-dmg/Log MetaRaw Audio Guide (english).pdf" "file://$GUIDE/guide-audio-en.html" 2>/dev/null
# each guide: page 1 is the summary of every function, eleven pages at most (the 4.2 Audio page)
for g in "$BUILD/dmg/Guida Log MetaRaw (italiano).pdf" "$BUILD/dmg/Log MetaRaw Guide (english).pdf" \
         "$BUILD/audio-dmg/Guida Log MetaRaw Audio (italiano).pdf" "$BUILD/audio-dmg/Log MetaRaw Audio Guide (english).pdf"; do
  python3 - "$g" <<'EOF'
import sys
d = open(sys.argv[1], 'rb').read()
n = d.count(b'/Type /Page') - d.count(b'/Type /Pages')
if not 1 <= n <= 11:
    sys.exit('%s: %d pagine invece di 11 al massimo' % (sys.argv[1], n))
EOF
done

echo "3/7 contenuto del pacchetto (Log MetaRaw e Mecena Motion)"
R="$BUILD/root"
mkdir -p "$R/Library/OFX/Plugins" "$R/Library/Application Support/SLogMetaRaw/lib" \
         "$R/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts/Utility"
cp -R "$ROOT/ofx/SLogMetaRaw/SLogMetaRaw.ofx.bundle" "$R/Library/OFX/Plugins/"
rsync -a --exclude "__pycache__" "$ROOT/slogmetaraw" "$R/Library/Application Support/SLogMetaRaw/lib/"
python3 "$ROOT/tools/render_launcher.py" "$ROOT/resolve_script/SLogMetaRaw.py" \
    "/Library/Application Support/SLogMetaRaw/lib" \
    "$R/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts/Utility/Log MetaRaw.py"
# Log MetaRaw Audio (VST3 for Fairlight): Ripresa and Mix e supporto
AUDIO_BUILD="$BUILD/audio"
cmake -S "$ROOT/audio/vst3" -B "$AUDIO_BUILD" -DCMAKE_BUILD_TYPE=Release > "$BUILD/audio-build.log" 2>&1 &&
  cmake --build "$AUDIO_BUILD" --config Release --parallel --target LogMetaRawAudio LogMetaRawAU >> "$BUILD/audio-build.log" 2>&1 ||
  { tail -40 "$BUILD/audio-build.log" >&2; exit 1; }
A="$BUILD/audio-root"   # its own component: the installer lets the user leave it out
mkdir -p "$A/Library/Audio/Plug-Ins/VST3"
cp -R "$AUDIO_BUILD/VST3/Release/Log MetaRaw Audio.vst3" "$A/Library/Audio/Plug-Ins/VST3/"
U="$BUILD/au-root"   # the Audio Units for Logic Pro: a component of their own too
mkdir -p "$U/Library/Audio/Plug-Ins/Components"
cp -R "$AUDIO_BUILD/AU/"*.component "$U/Library/Audio/Plug-Ins/Components/"
find "$R" "$A" "$U" -name ".DS_Store" -delete
xattr -rc "$U"
xattr -rc "$A"
xattr -rc "$R"

M="$BUILD/motion-root"
mkdir -p "$M/Library/OFX/Plugins" "$BUILD/motion-scripts"
bash "$MOTION/release/build-mac.sh" > "$BUILD/motion-build.log" 2>&1 || { tail -40 "$BUILD/motion-build.log" >&2; exit 1; }
ditto -x -k "$MOTION/dist/MecenaMotion-$MOTION_VERSION-$(uname -m).zip" "$M/Library/OFX/Plugins/"
cp "$MOTION/release/installer/preinstall" "$MOTION/release/installer/postinstall" "$BUILD/motion-scripts/"

echo "4/7 pacchetto"
# a checkout that lost the mode bit would ship a package whose scripts execve cannot run
chmod +x "$PKG_DIR/scripts/preinstall" "$PKG_DIR/scripts/postinstall" \
         "$BUILD/motion-scripts/preinstall" "$BUILD/motion-scripts/postinstall"
pkgbuild --root "$R" --install-location / --identifier com.slogmetaraw.pkg --version "$VERSION" \
         --scripts "$PKG_DIR/scripts" "$BUILD/SLogMetaRaw-component.pkg" > "$BUILD/pkgbuild.log" 2>&1
# the receipt of Mecena Motion's own installer: either one updates the other
pkgbuild --root "$M" --install-location / --identifier it.mecena.motion.pkg --version "$MOTION_VERSION" \
         --scripts "$BUILD/motion-scripts" "$BUILD/MecenaMotion-component.pkg" >> "$BUILD/pkgbuild.log" 2>&1
pkgbuild --root "$A" --install-location / --identifier com.slogmetaraw.audio.pkg --version "$VERSION" \
         "$BUILD/LogMetaRawAudio-component.pkg" >> "$BUILD/pkgbuild.log" 2>&1
pkgbuild --root "$U" --install-location / --identifier com.slogmetaraw.audiounit.pkg --version "$VERSION" \
         "$BUILD/LogMetaRawAU-component.pkg" >> "$BUILD/pkgbuild.log" 2>&1
cp "$PKG_DIR/resources/"* "$BUILD/resources/"
# the version lives in slogmetaraw/__init__.py alone: substitute it into the
# installer pages too, so they cannot announce a different one from the disk
for f in "$BUILD/resources/"*.html; do
  sed -i '' "s/@VERSION@/$VERSION/g" "$f"
done
sips -z 190 190 "$ROOT/assets/icon_1024.png" --out "$BUILD/resources/background.png" >/dev/null
# the version lives in slogmetaraw/__init__.py alone: substitute it in rather than
# keeping a second copy in distribution.xml that can drift out of step
sed -e "s/@VERSION@/$VERSION/g" -e "s/@MOTION_VERSION@/$MOTION_VERSION/g" "$PKG_DIR/distribution.xml" \
    > "$BUILD/distribution.xml"
productbuild --distribution "$BUILD/distribution.xml" --resources "$BUILD/resources" \
             --package-path "$BUILD" "$BUILD/dmg/Installa Log MetaRaw.pkg" > "$BUILD/productbuild.log" 2>&1
swift "$ROOT/tools/set_icon.swift" "$ROOT/assets/SLogMetaRaw.icns" "$BUILD/dmg/Installa Log MetaRaw.pkg"

echo "5/7 disinstallatore"
cp "$PKG_DIR/Disinstalla Log MetaRaw.command" "$BUILD/dmg/"
chmod +x "$BUILD/dmg/Disinstalla Log MetaRaw.command"   # Finder opens it only if executable
cp "$ROOT/assets/SLogMetaRaw.icns" "$BUILD/dmg/.VolumeIcon.icns"
xattr -c "$BUILD/dmg/Guida Log MetaRaw (italiano).pdf" "$BUILD/dmg/Log MetaRaw Guide (english).pdf" \
         "$BUILD/dmg/Disinstalla Log MetaRaw.command" "$BUILD/dmg/.VolumeIcon.icns"

echo "6/7 disco"
DMG="$DIST/SLogMetaRaw-$VERSION.dmg"
rm -f "$DMG" "$BUILD/rw.dmg"
hdiutil create -quiet -volname "Log MetaRaw $VERSION" -srcfolder "$BUILD/dmg" -fs HFS+ -format UDRW "$BUILD/rw.dmg"
MNT="$(hdiutil attach -nobrowse -noverify -noautoopen "$BUILD/rw.dmg" | awk -F'\t' '/\/Volumes\//{print $NF}')"
xcrun SetFile -a C "$MNT" 2>/dev/null || true   # show .VolumeIcon.icns as the disk icon
hdiutil detach -quiet "$MNT"
hdiutil convert -quiet "$BUILD/rw.dmg" -format UDZO -o "$DMG"
rm -f "$BUILD/rw.dmg"
swift "$ROOT/tools/set_icon.swift" "$ROOT/assets/SLogMetaRaw.icns" "$DMG"
echo "Creato: $DMG"

echo "7/7 disco solo audio"
# the same component packages as above, under their own installer: Logic and the other DAWs without Resolve
sed -e "s/@VERSION@/$VERSION/g" "$PKG_DIR/distribution-audio.xml" > "$BUILD/distribution-audio.xml"
mkdir -p "$BUILD/audio-resources"
cp "$PKG_DIR/resources/license.txt" "$BUILD/resources/background.png" "$BUILD/audio-resources/"
productbuild --distribution "$BUILD/distribution-audio.xml" --resources "$BUILD/audio-resources" \
             --package-path "$BUILD" "$BUILD/audio-dmg/Installa Log MetaRaw Audio.pkg" >> "$BUILD/productbuild.log" 2>&1
swift "$ROOT/tools/set_icon.swift" "$ROOT/assets/SLogMetaRaw.icns" "$BUILD/audio-dmg/Installa Log MetaRaw Audio.pkg"
cp "$PKG_DIR/Disinstalla Log MetaRaw Audio.command" "$BUILD/audio-dmg/"
chmod +x "$BUILD/audio-dmg/Disinstalla Log MetaRaw Audio.command"
cp "$ROOT/assets/SLogMetaRaw.icns" "$BUILD/audio-dmg/.VolumeIcon.icns"
xattr -c "$BUILD/audio-dmg/"*.pdf "$BUILD/audio-dmg/Disinstalla Log MetaRaw Audio.command" "$BUILD/audio-dmg/.VolumeIcon.icns"
ADMG="$DIST/LogMetaRawAudio-$VERSION.dmg"
rm -f "$ADMG" "$BUILD/rw.dmg"
hdiutil create -quiet -volname "Log MetaRaw Audio $VERSION" -srcfolder "$BUILD/audio-dmg" -fs HFS+ -format UDRW "$BUILD/rw.dmg"
MNT="$(hdiutil attach -nobrowse -noverify -noautoopen "$BUILD/rw.dmg" | awk -F'\t' '/\/Volumes\//{print $NF}')"
xcrun SetFile -a C "$MNT" 2>/dev/null || true
hdiutil detach -quiet "$MNT"
hdiutil convert -quiet "$BUILD/rw.dmg" -format UDZO -o "$ADMG"
rm -f "$BUILD/rw.dmg"
swift "$ROOT/tools/set_icon.swift" "$ROOT/assets/SLogMetaRaw.icns" "$ADMG"
echo "Creato: $ADMG"

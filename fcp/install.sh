#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds and installs Log MetaRaw for Final Cut Pro on this Mac:
#   1. the FxPlug wrapper app (with the effect inside) -> /Applications/Log MetaRaw.app
#   2. the Motion Template that lists the effect in Final Cut's Effects browser
#      -> ~/Movies/Motion Templates.localized/Effects.localized/Log MetaRaw
# Needs Xcode, the Metal Toolchain component and the Apple FxPlug SDK.
#
#   CODE_SIGN_IDENTITY=<identity or SHA-1> ./install.sh     (default "-": ad hoc)
#
# Final Cut Pro must be restarted afterwards. Uninstall: ./install.sh --uninstall
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ID="${CODE_SIGN_IDENTITY:--}"
APP_DEST="/Applications/Log MetaRaw.app"
PLUG_REL="Contents/PlugIns/LogMetaRawPlugin.pluginkit"
LSR=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
TPL_DEST="$HOME/Movies/Motion Templates.localized/Effects.localized/Log MetaRaw"
DD="$HERE/build/xc"

forget() {   # unregister a copy of the app so that pkd cannot prefer it over the installed one
    [ -d "$1" ] || return 0
    pluginkit -r "$1/$PLUG_REL" >/dev/null 2>&1 || true
    "$LSR" -u "$1" >/dev/null 2>&1 || true
}

# Replacing the plug-in under a running Final Cut Pro leaves it retrying the old XPC service (it hangs).
if pgrep -x "Final Cut Pro" >/dev/null; then
    echo "Chiudi Final Cut Pro prima di installare o disinstallare Log MetaRaw." >&2
    exit 1
fi

if [ "${1:-}" = "--uninstall" ]; then
    forget "$APP_DEST"
    rm -rf "$APP_DEST" "$TPL_DEST"
    echo "Log MetaRaw rimosso. Riavvia Final Cut Pro."
    exit 0
fi

echo "== Progetto Xcode e build"
python3 "$HERE/make_xcodeproj.py" >/dev/null
xcodebuild -project "$HERE/LogMetaRaw.xcodeproj" -scheme LogMetaRaw -configuration Release \
    -derivedDataPath "$DD" CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY="$ID" DEVELOPMENT_TEAM= build \
    > "$HERE/build/xcodebuild.log" 2>&1 || { tail -20 "$HERE/build/xcodebuild.log"; exit 1; }
BUILT="$DD/Build/Products/Release/Log MetaRaw.app"
codesign --verify --deep --strict "$BUILT"

echo "== Installazione in /Applications"
forget "$BUILT"                       # xcodebuild registers the build copy: it must not win
for old in "$HOME/Applications/Log MetaRaw.app" "/Library/Plug-Ins/FxPlug/Log MetaRaw.app" \
           "$HOME"/Library/Developer/Xcode/DerivedData/LogMetaRaw-*/Build/Products/*/"Log MetaRaw.app"; do
    forget "$old"
done
forget "$APP_DEST"
rm -rf "$APP_DEST"
cp -R "$BUILT" "$APP_DEST"
rm -rf "$DD/Build/Products"            # no second copy with the same identifier for pkd to prefer
"$LSR" -f "$APP_DEST"
pluginkit -a "$APP_DEST/$PLUG_REL"
pluginkit -e use -i com.slogmetaraw.fcp.LogMetaRaw.Plugin

echo "== Motion Template (elenca l'effetto in Final Cut)"
python3 "$HERE/make_motion_template.py" --install

echo "== Comando dei metadata (logmetaraw-fcp)"
TOOL="$HOME/Library/Application Support/Log MetaRaw FCP"
rm -rf "$TOOL/lib" "$TOOL/logmetaraw-fcp"   # catalog.json (the index of the libraries) is kept
mkdir -p "$TOOL/lib"
rsync -a --exclude __pycache__ "$HERE/../slogmetaraw" "$TOOL/lib/"
cat > "$TOOL/logmetaraw-fcp" <<'EOF'
#!/bin/bash
# Log MetaRaw for Final Cut Pro: camera metadata -> FCPXML with the effect set to each clip.
#   logmetaraw-fcp -o Clip.fcpxml --event NAME CLIP_OR_FOLDER...      import new clips
#   logmetaraw-fcp --apply EXPORTED.fcpxml(d) -o OUT.fcpxml           clips already in Final Cut (File > Export XML)
HERE="$(cd "$(dirname "$0")" && pwd)"
PYTHONPATH="$HERE/lib" exec python3 -m slogmetaraw.fcp_io "$@"
EOF
chmod +x "$TOOL/logmetaraw-fcp"

echo "== Verifica della registrazione"
# pkd handles the removals above asynchronously and can drop the new registration seconds later (same identifier):
# keep checking for 20 s and register again whenever it is gone.
for i in $(seq 1 10); do
    sleep 2
    if ! pluginkit -mA -p FxPlug | grep -q "/Applications/Log MetaRaw.app"; then
        pluginkit -a "$APP_DEST/$PLUG_REL"
        pluginkit -e use -i com.slogmetaraw.fcp.LogMetaRaw.Plugin
    fi
done
pluginkit -mAv -p FxPlug | grep -i slogmetaraw || { echo "ATTENZIONE: plugin non registrato"; exit 1; }
echo "Fatto. Riavvia Final Cut Pro: Effetti > Log MetaRaw."

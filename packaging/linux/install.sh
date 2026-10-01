#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Installs S-Log MetaRaw for DaVinci Resolve on Linux.
#   ./install.sh                 plugin in /usr/OFX/Plugins (sudo), script + library for this user
#   ./install.sh --user          plugin in ~/.local/share/OFX/Plugins instead of /usr/OFX/Plugins (no sudo)
#   ./install.sh --uninstall     remove everything this script installed
# Works from the unpacked release (this folder holds SLogMetaRaw.ofx.bundle, slogmetaraw/, tools/, resolve_script/)
# and from a repository checkout after `cmake --build` (the bundle is then in ofx/SLogMetaRaw/build).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE"
[[ -d "$ROOT/slogmetaraw" ]] || ROOT="$(cd "$HERE/../.." && pwd)"
BUNDLE="$HERE/SLogMetaRaw.ofx.bundle"
[[ -d "$BUNDLE" ]] || BUNDLE="$ROOT/ofx/SLogMetaRaw/build/SLogMetaRaw.ofx.bundle"

USER_PLUGIN=0
UNINSTALL=0
for arg in "$@"; do
  case "$arg" in
    --user) USER_PLUGIN=1 ;;
    --uninstall) UNINSTALL=1 ;;
    --help|-h) sed -n 3,9p "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
SUPPORT="$DATA/SLogMetaRaw"
SCRIPTS="$DATA/DaVinciResolve/Fusion/Scripts/Utility"
if [[ "$USER_PLUGIN" == 1 ]]; then
  PLUGINS="$DATA/OFX/Plugins"; SUDO=""
else
  PLUGINS="/usr/OFX/Plugins"; SUDO="sudo"
fi

if [[ "$UNINSTALL" == 1 ]]; then
  $SUDO rm -rf "${PLUGINS:?}/SLogMetaRaw.ofx.bundle"
  rm -rf "${SUPPORT:?}/lib" "${SUPPORT:?}/lib_path" "${SUPPORT:?}/pycache" "${SCRIPTS:?}/S-Log MetaRaw.py"
  echo "Rimosso. Cache e log restano in $SUPPORT (cancellali a mano se vuoi)."
  exit 0
fi

PYTHON_BIN="$(command -v python3 || true)"
[[ -n "$PYTHON_BIN" ]] || { echo "ERRORE: Python 3 non trovato." >&2; exit 1; }
[[ -d "$BUNDLE" ]] || { echo "ERRORE: SLogMetaRaw.ofx.bundle non trovato (compila con cmake, vedi README)." >&2; exit 1; }

LIB="$SUPPORT/lib"
mkdir -p "$LIB" "$SCRIPTS" "$SUPPORT/cache"
rm -rf "${LIB:?}/slogmetaraw"
cp -R "$ROOT/slogmetaraw" "$LIB/"
find "$LIB/slogmetaraw" -name '__pycache__' -prune -exec rm -rf {} +
"$PYTHON_BIN" "$ROOT/tools/render_launcher.py" "$ROOT/resolve_script/SLogMetaRaw.py" "$LIB" "$SCRIPTS/S-Log MetaRaw.py"
printf '%s\n' "$LIB" > "$SUPPORT/lib_path"

echo "Installo il plugin in $PLUGINS ${SUDO:+(serve la password di amministratore)}..."
$SUDO mkdir -p "$PLUGINS"
$SUDO rm -rf "${PLUGINS:?}/SLogMetaRaw.ofx.bundle"
$SUDO cp -R "$BUNDLE" "$PLUGINS/"
[[ "$USER_PLUGIN" == 1 ]] && echo "Nota: Resolve legge ~/.local/share/OFX/Plugins solo se OFX_PLUGIN_PATH lo include."
echo "Fatto. Riavvia DaVinci Resolve: il nodo e in OpenFX, lo script in Workspace > Scripts > S-Log MetaRaw."

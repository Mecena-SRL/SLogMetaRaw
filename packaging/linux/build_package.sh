#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds dist/SLogMetaRaw-<version>-linux-<arch>.tar.gz: the plugin bundle, the Python library, the launcher
# and install.sh. Needs cmake, a C++17 compiler and python3. OFX_SDK_DIR may point to a local OpenFX SDK
# (otherwise CMake fetches it).
#   ./packaging/linux/build_package.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION="$(python3 -c 'import sys; sys.path.insert(0, sys.argv[1]); import slogmetaraw; print(slogmetaraw.__version__)' "$ROOT")"
ARCH="$(uname -m)"
BUILD="$ROOT/ofx/SLogMetaRaw/build"
NAME="SLogMetaRaw-$VERSION-linux-$ARCH"
STAGE="$ROOT/build/package/$NAME"
DIST="$ROOT/dist"

cmake -S "$ROOT/ofx/SLogMetaRaw" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DSLOGMETARAW_BUILD_TESTS=OFF \
  ${OFX_SDK_DIR:+-DOFX_SDK_DIR="$OFX_SDK_DIR"}
cmake --build "$BUILD" --parallel

rm -rf "${STAGE:?}"
mkdir -p "$STAGE/tools" "$STAGE/resolve_script" "$DIST"
cp -R "$BUILD/SLogMetaRaw.ofx.bundle" "$STAGE/"
cp -R "$ROOT/slogmetaraw" "$STAGE/"
find "$STAGE/slogmetaraw" -name '__pycache__' -prune -exec rm -rf {} +
cp "$ROOT/tools/render_launcher.py" "$STAGE/tools/"
cp "$ROOT/resolve_script/SLogMetaRaw.py" "$STAGE/resolve_script/"
cp "$ROOT/packaging/linux/install.sh" "$ROOT/LICENSE" "$ROOT/README.md" "$STAGE/"
tar -C "$ROOT/build/package" -czf "$DIST/$NAME.tar.gz" "$NAME"
echo "$DIST/$NAME.tar.gz"

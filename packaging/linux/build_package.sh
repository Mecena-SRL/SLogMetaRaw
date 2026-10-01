#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the Linux packages into dist/: .tar.gz and self-extracting .run (any distribution), .deb (Ubuntu / Debian)
# and .rpm (Rocky / Alma / RHEL / Fedora). Needs cmake, a C++17 compiler and python3; dpkg-deb for the .deb and
# rpmbuild for the .rpm. OFX_SDK_DIR may point to a local OpenFX SDK (otherwise CMake fetches it).
#
#   ./packaging/linux/build_package.sh                    build the plugin, then every package
#   BUNDLE=/path/SLogMetaRaw.ofx.bundle ./packaging/linux/build_package.sh    packages only, from a built plugin
#   ONLY=tar,run ./packaging/linux/build_package.sh       only some formats
#
# The plugin binary must be built on an old distribution (CI uses a glibc 2.28 container): a binary built on a recent
# Ubuntu needs a recent glibc and does not load on Rocky 8/9 or Ubuntu 20.04/22.04.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

if [[ -z "${BUNDLE:-}" ]]; then
  BUILD="$ROOT/build/linux-package"
  cmake -S "$ROOT/ofx/SLogMetaRaw" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DSLOGMETARAW_BUILD_TESTS=OFF \
    ${OFX_SDK_DIR:+-DOFX_SDK_DIR="$OFX_SDK_DIR"}
  cmake --build "$BUILD" --parallel
  BUNDLE="$BUILD/SLogMetaRaw.ofx.bundle"
fi

python3 "$ROOT/packaging/linux/build_packages.py" --bundle "$BUNDLE" --out "$ROOT/dist" --only "${ONLY:-tar,run,deb,rpm}"

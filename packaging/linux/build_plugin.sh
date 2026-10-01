#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds only the Linux plugin bundle and checks its ABI. CI runs it in a glibc 2.28 container (manylinux_2_28), so
# the binary loads on Rocky/Alma 8+, RHEL 8+, Ubuntu 20.04+ and Debian 10+.
#   ./packaging/linux/build_plugin.sh [BUILD_DIR]      default build/linux-plugin; prints the bundle path last
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build/linux-plugin}"
# manylinux images keep their pythons here, outside the PATH
for dir in /opt/python/cp311-cp311/bin /opt/python/cp312-cp312/bin; do
  if [[ -d "$dir" ]]; then export PATH="$dir:$PATH"; break; fi
done
command -v cmake >/dev/null || python3 -m pip install --quiet cmake
command -v git >/dev/null || { yum install -y git >/dev/null 2>&1 || dnf install -y git >/dev/null 2>&1 || true; }

cmake -S "$ROOT/ofx/SLogMetaRaw" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DSLOGMETARAW_BUILD_TESTS=OFF \
  ${OFX_SDK_DIR:+-DOFX_SDK_DIR="$OFX_SDK_DIR"}
cmake --build "$BUILD" --parallel
python3 "$ROOT/packaging/linux/check_abi.py" "$BUILD/SLogMetaRaw.ofx" --max-glibc "${MAX_GLIBC:-2.28}"
echo "$BUILD/SLogMetaRaw.ofx.bundle"

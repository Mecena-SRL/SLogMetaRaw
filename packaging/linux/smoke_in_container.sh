#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs INSIDE a distribution container (CI): installs the package for that distribution from /pkg, then checks that
# the plugin binary loads, the Python library imports on that distribution's python3, the Resolve menu script is put
# in place, and removing the package takes everything away.
#   smoke_in_container.sh deb|rpm
set -eu
KIND="${1:?deb or rpm}"
PLUGIN=/usr/OFX/Plugins/SLogMetaRaw.ofx.bundle/Contents/Linux-x86-64/SLogMetaRaw.ofx
MENU="/opt/resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py"

mkdir -p /opt/resolve    # as if DaVinci Resolve were installed: the package then puts its menu script there
case "$KIND" in
  deb)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq /pkg/*.deb
    ;;
  rpm)
    dnf install -y -q /pkg/*.rpm
    ;;
  *) echo "unknown kind $KIND" >&2; exit 2 ;;
esac

test -x "$PLUGIN" || { echo "FAIL: $PLUGIN missing"; exit 1; }
test -f /usr/lib/slogmetaraw/slogmetaraw/__init__.py || { echo "FAIL: library missing"; exit 1; }
test -f "$MENU" || { echo "FAIL: menu script missing in /opt/resolve"; exit 1; }
test -x /usr/bin/slogmetaraw-setup || { echo "FAIL: slogmetaraw-setup missing"; exit 1; }

echo "== python: $(python3 --version)"
# the binary loads here: right glibc, no dependency on this distribution's libstdc++
python3 - "$PLUGIN" <<'PY'
import ctypes, sys
lib = ctypes.CDLL(sys.argv[1])
assert lib.OfxGetNumberOfPlugins() == 2, 'expected the two nodes'
print('plugin loads, 2 nodes')
PY
# the library runs on this python3 (Rocky/RHEL 8: 3.6)
python3 - <<'PY'
import sys
sys.path.insert(0, '/usr/lib/slogmetaraw')
import slogmetaraw, slogmetaraw.extract, slogmetaraw.mp4, slogmetaraw.plugin_cache, slogmetaraw.update, slogmetaraw.i18n
from slogmetaraw import __main__ as cli
rc = cli.main(['--cache'])      # no path: must refuse politely, not crash
assert rc == 2, rc
print('library', slogmetaraw.__version__, 'ok')
PY
# the launcher points at the system library and compiles
python3 - "$MENU" <<'PY'
import sys
src = open(sys.argv[1], encoding='utf-8').read()
compile(src, sys.argv[1], 'exec')
assert "LIB_DIR = '/usr/lib/slogmetaraw'" in src
print('menu script ok')
PY
# a user can also put it in their own folder
HOME=/tmp/smokehome slogmetaraw-setup
test -f "/tmp/smokehome/.local/share/DaVinciResolve/Fusion/Scripts/Utility/S-Log MetaRaw.py" || { echo "FAIL: user setup"; exit 1; }

case "$KIND" in
  deb) apt-get remove -y -qq slogmetaraw ;;
  rpm) dnf remove -y -q slogmetaraw ;;
esac
for f in "$PLUGIN" /usr/lib/slogmetaraw/slogmetaraw/__init__.py "$MENU" /usr/bin/slogmetaraw-setup; do
  test ! -e "$f" || { echo "FAIL: $f still there after removal"; exit 1; }
done
test ! -d /usr/OFX/Plugins/SLogMetaRaw.ofx.bundle || { echo "FAIL: bundle directory left behind"; exit 1; }
echo "OK $KIND"

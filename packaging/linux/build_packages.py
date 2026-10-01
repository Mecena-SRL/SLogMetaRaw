#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds every Linux package from a built plugin bundle.

  build_packages.py --bundle <SLogMetaRaw.ofx.bundle> --out dist [--only tar,run,deb,rpm]

  SLogMetaRaw-<v>-linux-<arch>.tar.gz   plugin + library + install.sh (any distribution)
  SLogMetaRaw-<v>-linux-<arch>.run      the same, self-extracting: `sh SLogMetaRaw-*.run`
  slogmetaraw_<v>_<deb-arch>.deb        Ubuntu / Debian / Mint
  slogmetaraw-<v>-1.<arch>.rpm          Rocky / Alma / RHEL / CentOS / Fedora / openSUSE

The .deb and .rpm install the plugin into /usr/OFX/Plugins, the Python library into /usr/lib/slogmetaraw and the
Resolve menu script into /opt/resolve/Fusion/Scripts/Utility when Resolve is installed there; `slogmetaraw-setup`
puts the menu script in a user's own folder otherwise. dpkg-deb and rpmbuild are needed for those two formats.
"""
import argparse
import gzip
import io
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from slogmetaraw import __version__  # noqa: E402
from render_launcher import render_launcher  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
LIB_DIR = '/usr/lib/slogmetaraw'
EPOCH = int(os.environ.get('SOURCE_DATE_EPOCH', '1767225600'))   # fixed mtime: the same input gives the same bytes
MENU_NAME = 'S-Log MetaRaw.py'
MAINTAINER = 'Mecena SRL <noreply@github.com>'
HOMEPAGE = 'https://github.com/Mecena-SRL/SLogMetaRaw'

SETUP_SCRIPT = r'''#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Puts the S-Log MetaRaw menu script (Workspace > Scripts) where DaVinci Resolve looks for it.
#   slogmetaraw-setup            for this user: ~/.local/share/DaVinciResolve/Fusion/Scripts/Utility
#   slogmetaraw-setup --system   for everyone, in /opt/resolve (needs root)
#   slogmetaraw-setup --remove   remove the user's copy (--system --remove: the system one)
set -eu
SRC="/usr/share/slogmetaraw/launcher/S-Log MetaRaw.py"
SYSTEM=0
REMOVE=0
for arg in "$@"; do
  case "$arg" in
    --system) SYSTEM=1 ;;
    --remove) REMOVE=1 ;;
    -h|--help) sed -n 3,7p "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "slogmetaraw-setup: unknown option $arg" >&2; exit 2 ;;
  esac
done
if [ "$SYSTEM" = 1 ]; then
  DEST="/opt/resolve/Fusion/Scripts/Utility"
else
  DEST="${XDG_DATA_HOME:-$HOME/.local/share}/DaVinciResolve/Fusion/Scripts/Utility"
fi
if [ "$REMOVE" = 1 ]; then
  rm -f "${DEST:?}/S-Log MetaRaw.py"
  echo "Removed $DEST/S-Log MetaRaw.py"
  exit 0
fi
mkdir -p "$DEST"
cp "$SRC" "$DEST/S-Log MetaRaw.py"
echo "Installed $DEST/S-Log MetaRaw.py - restart DaVinci Resolve."
'''

POSTINST_BODY = r'''if [ -d /opt/resolve ]; then
  mkdir -p /opt/resolve/Fusion/Scripts/Utility
  cp "/usr/share/slogmetaraw/launcher/S-Log MetaRaw.py" "/opt/resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py"
fi
'''

PRERM_BODY = r'''rm -f "/opt/resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py"
'''

RUN_STUB = r'''#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# S-Log MetaRaw %(version)s for DaVinci Resolve - self-extracting installer.
#   sh %(name)s              install: plugin in /usr/OFX/Plugins (asks for sudo), menu script for this user
#   sh %(name)s --user       plugin in ~/.local/share/OFX/Plugins instead (no sudo)
#   sh %(name)s --uninstall  remove what it installed
#   sh %(name)s --extract DIR  only unpack into DIR
set -eu
if [ "${1:-}" = "--extract" ]; then
  DIR="${2:?usage: --extract DIR}"
  mkdir -p "$DIR"
  LINE=$(awk '/^__SLOGMETARAW_ARCHIVE__$/ { print NR + 1; exit }' "$0")
  tail -n +"$LINE" "$0" | tar -xz -C "$DIR"
  echo "Unpacked into $DIR"
  exit 0
fi
TMP=$(mktemp -d)
trap 'rm -rf "${TMP:?}"' EXIT
LINE=$(awk '/^__SLOGMETARAW_ARCHIVE__$/ { print NR + 1; exit }' "$0")
tail -n +"$LINE" "$0" | tar -xz -C "$TMP"
set +e
"$TMP"/SLogMetaRaw-*/install.sh "$@"   # its own shebang: it needs bash, and sh may be dash
RC=$?
exit $RC
__SLOGMETARAW_ARCHIVE__
'''


def deb_arch():
    return {'x86_64': 'amd64', 'aarch64': 'arm64'}.get(platform.machine(), platform.machine())


def rpm_arch():
    return platform.machine()


def copy_tree(src, dst):
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'), symlinks=True)


def normalise(top):
    """Modes and times that do not depend on who built the package."""
    for base, dirs, files in os.walk(top):
        for name in dirs:
            os.chmod(os.path.join(base, name), 0o755)
        for name in files:
            path = os.path.join(base, name)
            if os.path.islink(path):
                continue
            executable = os.access(path, os.X_OK) or path.endswith(('.ofx', '.sh', '/slogmetaraw-setup'))
            os.chmod(path, 0o755 if executable else 0o644)
            os.utime(path, (EPOCH, EPOCH))
        os.utime(base, (EPOCH, EPOCH))


def write(path, text, mode=0o644):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(text)
    os.chmod(path, mode)


def stage_system_tree(stage, bundle):
    """The files of the .deb / .rpm, laid out as they land on the machine."""
    copy_tree(bundle, os.path.join(stage, 'usr/OFX/Plugins/SLogMetaRaw.ofx.bundle'))
    copy_tree(os.path.join(ROOT, 'slogmetaraw'), os.path.join(stage, 'usr/lib/slogmetaraw/slogmetaraw'))
    launcher = os.path.join(stage, 'usr/share/slogmetaraw/launcher', MENU_NAME)
    os.makedirs(os.path.dirname(launcher))
    render_launcher(os.path.join(ROOT, 'resolve_script/SLogMetaRaw.py'), LIB_DIR, launcher)
    write(os.path.join(stage, 'usr/bin/slogmetaraw-setup'), SETUP_SCRIPT, 0o755)
    doc = os.path.join(stage, 'usr/share/doc/slogmetaraw')
    os.makedirs(doc)
    shutil.copy(os.path.join(ROOT, 'README.md'), doc)
    shutil.copy(os.path.join(ROOT, 'LICENSE'), os.path.join(doc, 'copyright'))
    normalise(stage)


def build_tar(bundle, out, name):
    """tar.gz with install.sh at its top: also the payload of the .run."""
    top = 'SLogMetaRaw-%s-linux-%s' % (__version__, platform.machine())
    with tempfile.TemporaryDirectory() as tmp:
        stage = os.path.join(tmp, top)
        os.makedirs(os.path.join(stage, 'tools'))
        os.makedirs(os.path.join(stage, 'resolve_script'))
        copy_tree(bundle, os.path.join(stage, 'SLogMetaRaw.ofx.bundle'))
        copy_tree(os.path.join(ROOT, 'slogmetaraw'), os.path.join(stage, 'slogmetaraw'))
        shutil.copy(os.path.join(ROOT, 'tools/render_launcher.py'), os.path.join(stage, 'tools'))
        shutil.copy(os.path.join(ROOT, 'resolve_script/SLogMetaRaw.py'), os.path.join(stage, 'resolve_script'))
        for f in ('LICENSE', 'README.md'):
            shutil.copy(os.path.join(ROOT, f), stage)
        shutil.copy(os.path.join(HERE, 'install.sh'), stage)
        normalise(stage)
        path = os.path.join(out, top + '.tar.gz')
        buf = io.BytesIO()
        with gzip.GzipFile(fileobj=buf, mode='wb', mtime=EPOCH) as gz:
            with tarfile.open(fileobj=gz, mode='w', format=tarfile.GNU_FORMAT) as tar:
                def clean(info):
                    info.uid = info.gid = 0
                    info.uname = info.gname = 'root'
                    info.mtime = EPOCH
                    return info
                for base, dirs, files in sorted(os.walk(stage)):
                    dirs.sort()
                    rel = os.path.relpath(base, tmp)
                    tar.add(base, arcname=rel, recursive=False, filter=clean)
                    for f in sorted(files):
                        tar.add(os.path.join(base, f), arcname=os.path.join(rel, f), recursive=False, filter=clean)
        with open(path, 'wb') as fh:
            fh.write(buf.getvalue())
    return path


def build_run(tar_path, out):
    top = os.path.basename(tar_path)[:-len('.tar.gz')]
    path = os.path.join(out, top + '.run')
    stub = RUN_STUB % {'version': __version__, 'name': top + '.run'}
    with open(tar_path, 'rb') as src, open(path, 'wb') as fh:
        fh.write(stub.encode('utf-8'))
        fh.write(src.read())
    os.chmod(path, 0o755)
    return path


def build_deb(bundle, out):
    if not shutil.which('dpkg-deb'):
        raise SystemExit('dpkg-deb not found (apt install dpkg-dev), cannot build the .deb')
    with tempfile.TemporaryDirectory() as tmp:
        stage = os.path.join(tmp, 'root')
        stage_system_tree(stage, bundle)
        size = sum(os.path.getsize(os.path.join(b, f)) for b, _d, fs in os.walk(stage) for f in fs) // 1024
        control = '\n'.join([
            'Package: slogmetaraw',
            'Version: %s' % __version__,
            'Section: video',
            'Priority: optional',
            'Architecture: %s' % deb_arch(),
            'Depends: python3 (>= 3.8)',
            'Recommends: zenity | kdialog',
            'Installed-Size: %d' % size,
            'Maintainer: %s' % MAINTAINER,
            'Homepage: %s' % HOMEPAGE,
            'Description: Sony camera metadata and RAW-style controls for DaVinci Resolve',
            ' OpenFX nodes (S-Log MetaRaw and Detail) and a Workspace > Scripts window that reads the',
            ' acquisition metadata of Sony XAVC clips into the Media Pool. CPU rendering.',
            ''])
        debian = os.path.join(stage, 'DEBIAN')
        write(os.path.join(debian, 'control'), control)
        write(os.path.join(debian, 'postinst'), '#!/bin/sh\nset -e\n' + POSTINST_BODY + 'exit 0\n', 0o755)
        write(os.path.join(debian, 'prerm'),
              '#!/bin/sh\nset -e\nif [ "$1" = remove ] || [ "$1" = purge ] || [ "$1" = upgrade ]; then\n'
              + PRERM_BODY + 'fi\nexit 0\n', 0o755)
        os.utime(debian, (EPOCH, EPOCH))
        path = os.path.join(out, 'slogmetaraw_%s_%s.deb' % (__version__, deb_arch()))
        subprocess.run(['dpkg-deb', '--root-owner-group', '-Zxz', '--build', stage, path], check=True,
                       stdout=subprocess.DEVNULL, env=dict(os.environ, SOURCE_DATE_EPOCH=str(EPOCH)))
    return path


def build_rpm(bundle, out):
    if not shutil.which('rpmbuild'):
        raise SystemExit('rpmbuild not found (apt install rpm / dnf install rpm-build), cannot build the .rpm')
    with tempfile.TemporaryDirectory() as tmp:
        stage = os.path.join(tmp, 'stage')
        stage_system_tree(stage, bundle)
        files = []
        owned = ('usr/OFX/Plugins/SLogMetaRaw.ofx.bundle', 'usr/lib/slogmetaraw', 'usr/share/slogmetaraw',
                 'usr/share/doc/slogmetaraw')   # directories that are ours; /usr/OFX/Plugins is shared
        for base, dirs, fs in sorted(os.walk(stage)):
            dirs.sort()
            rel = os.path.relpath(base, stage)
            if any(rel == o or rel.startswith(o + '/') for o in owned):
                files.append('%dir "/' + rel + '"')
            for f in sorted(fs):
                files.append('"/' + os.path.join(rel, f) + '"')
        spec = '\n'.join([
            'Name: slogmetaraw',
            'Version: %s' % __version__,
            'Release: 1',
            'Summary: Sony camera metadata and RAW-style controls for DaVinci Resolve',
            'License: GPL-3.0-or-later',
            'URL: %s' % HOMEPAGE,
            'BuildArch: %s' % rpm_arch(),
            'Requires: python3 >= 3.8',
            'AutoReqProv: no',
            '%define _build_id_links none',
            '%define __os_install_post %{nil}',
            '%description',
            'OpenFX nodes (S-Log MetaRaw and Detail) and a Workspace > Scripts window that reads the',
            'acquisition metadata of Sony XAVC clips into the Media Pool. CPU rendering.',
            '%install',
            'mkdir -p %{buildroot}',
            'cp -a "' + stage + '/." %{buildroot}/',
            '%files',
            '%defattr(-,root,root,-)',
            *files,
            '%post',
            POSTINST_BODY.rstrip('\n'),
            '%preun',
            'if [ "$1" = 0 ]; then',
            PRERM_BODY.rstrip('\n'),
            'fi',
            ''])
        spec_path = os.path.join(tmp, 'slogmetaraw.spec')
        write(spec_path, spec)
        top = os.path.join(tmp, 'rpmtop')
        subprocess.run(['rpmbuild', '-bb', '--define', '_topdir ' + top, '--define', '_rpmdir ' + top + '/RPMS',
                        '--define', 'source_date_epoch_from_changelog 0', '--define', 'use_source_date_epoch_as_buildtime 1',
                        '--define', 'clamp_mtime_to_source_date_epoch 1', '--define', '_buildhost localhost',
                        spec_path], check=True, stdout=subprocess.DEVNULL,
                       env=dict(os.environ, SOURCE_DATE_EPOCH=str(EPOCH)))
        built = [os.path.join(b, f) for b, _d, fs in os.walk(os.path.join(top, 'RPMS')) for f in fs if f.endswith('.rpm')]
        if len(built) != 1:
            raise SystemExit('rpmbuild produced %d packages' % len(built))
        path = os.path.join(out, os.path.basename(built[0]))
        shutil.copy(built[0], path)
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--bundle', required=True, help='the built SLogMetaRaw.ofx.bundle folder')
    ap.add_argument('--out', default=os.path.join(ROOT, 'dist'))
    ap.add_argument('--only', default='tar,run,deb,rpm', help='comma separated: tar, run, deb, rpm')
    args = ap.parse_args()
    bundle = os.path.abspath(args.bundle)
    so = [f for _b, _d, fs in os.walk(bundle) for f in fs if f.endswith('.ofx')]
    if not so:
        raise SystemExit('%s holds no .ofx binary' % bundle)
    os.makedirs(args.out, exist_ok=True)
    wanted = [w.strip() for w in args.only.split(',') if w.strip()]
    made = []
    tar_path = None
    if 'tar' in wanted or 'run' in wanted:
        tar_path = build_tar(bundle, args.out, None)
        if 'tar' in wanted:
            made.append(tar_path)
    if 'run' in wanted:
        made.append(build_run(tar_path, args.out))
        if 'tar' not in wanted:
            os.remove(tar_path)
    if 'deb' in wanted:
        made.append(build_deb(bundle, args.out))
    if 'rpm' in wanted:
        made.append(build_rpm(bundle, args.out))
    for path in made:
        print(path)


if __name__ == '__main__':
    main()

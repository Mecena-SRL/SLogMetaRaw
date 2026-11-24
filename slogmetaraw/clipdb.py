# SPDX-License-Identifier: GPL-3.0-or-later
"""Deep acquisition: the clip's motion and lens data, read by Mecena Motion's mecena-clipdb.

One database for the script and for Stabilize: MecenaClipDB (format in GyroMotion,
vendor/common/src/clipdb.rs). mecena-clipdb reads every camera Gyroflow knows (Sony, DJI,
GoPro, Insta360, Blackmagic...) with Gyroflow's parser and calibrations and writes, per clip,
clips/<id>/ with gyro.gyroflow, imu.csv, orientation.csv, lens.csv, frames.csv and clip.json.
This module only runs it and reports what it wrote; database and tool are found the way the
Mecena ClipDB script of Resolve finds them.
"""
import json
import os
import shutil
import subprocess
import sys

from .extract import _warn

BUNDLE_BIN = '/Library/OFX/Plugins/MecenaMotion.ofx.bundle/Contents/Resources/mecena-clipdb'
SCAN_TIMEOUT = 150   # seconds per clip (the window waits CLIP_TIMEOUT_DEEP)
SECTION = 'MOVIMENTO (acquisizione profonda, MecenaClipDB per Stabilize)'


def config_path():
    home = os.path.expanduser('~')
    if sys.platform == 'darwin':
        return os.path.join(home, 'Library', 'Application Support', 'Mecena', 'clipdb.json')
    return os.path.join(home, '.config', 'mecena', 'clipdb.json')


def database():
    """MECENA_CLIPDB, else the first database mecena-clipdb registered, else ~/Movies/MecenaClipDB."""
    env = os.environ.get('MECENA_CLIPDB')
    if env:
        return env.split(os.pathsep)[0]
    try:
        with open(config_path(), encoding='utf-8') as fh:
            dbs = json.load(fh).get('databases') or []
    except (OSError, ValueError, AttributeError):
        dbs = []
    return dbs[0] if dbs else os.path.join(os.path.expanduser('~'), 'Movies', 'MecenaClipDB')


def tool():
    """Path of mecena-clipdb, or None when Mecena Motion is not installed."""
    for p in (os.environ.get('MECENA_CLIPDB_BIN'), BUNDLE_BIN):
        if p and os.path.isfile(p) and os.access(p, os.X_OK):
            return p
    return shutil.which('mecena-clipdb')


def entry(db, clip_path):
    """The index.json entry of an original, by its real path."""
    try:
        with open(os.path.join(db, 'index.json'), encoding='utf-8') as fh:
            clips = json.load(fh).get('clips') or []
    except (OSError, ValueError, AttributeError):
        return None
    real = os.path.realpath(clip_path)
    return next((c for c in clips if c.get('path') in (real, clip_path)), None)


def deep(r, timeout=SCAN_TIMEOUT):
    """Scan the clip of a read_clip() result into MecenaClipDB; add its summary and section.

    Returns the summary, or None (with a warning) when there is no tool or no motion data."""
    exe = tool()
    if not exe:
        _warn(r, 'Acquisizione profonda: Mecena Motion (mecena-clipdb) non installato.')
        return None
    db = database()
    try:
        # no capture_output/text: Rocky/RHEL 8 run Python 3.6
        proc = subprocess.run([exe, 'scan', db, r['path']], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, encoding='utf-8', errors='replace', timeout=timeout)
        detail = (proc.stderr.strip().splitlines() or [''])[-1]
    except subprocess.TimeoutExpired:
        detail = 'oltre %d s' % timeout
    except OSError as exc:
        detail = str(exc)
    e = entry(db, r['path'])
    if not e:
        _warn(r, 'Acquisizione profonda: nessun dato di movimento (%s).', detail or 'mecena-clipdb')
        return None
    s = r['motion'] = {
        'database': db, 'folder': os.path.join(db, 'clips', e.get('id', '')), 'summary': e.get('summary', ''),
        'has_gyro': bool(e.get('has_gyro')), 'fps': e.get('fps'), 'frames': e.get('frame_count')}
    r['sections'].append((SECTION, [
        ('Summary', s['summary']),
        ('Gyroscope', 'yes' if s['has_gyro'] else 'no (orientation only)'),
        ('Clip folder', s['folder'])]))
    return s

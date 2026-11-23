# SPDX-License-Identifier: GPL-3.0-or-later
"""Index of the camera files of the Final Cut Pro libraries, for the Log MetaRaw effect.

An FxPlug effect cannot see the file of the clip it is applied to, but it knows the clip's source start time
(its timecode, as seconds), its duration and its frame duration. This index gives, for every file referenced by
the libraries' "Original Media" folders, those three values and the as-shot values the Resolve node reads from
the clip: the effect looks itself up here and sets itself, as the node does.

    python3 -m slogmetaraw.fcp_catalog [--library LIB.fcpbundle ...] [--output catalog.json]

Incremental: a file already indexed with the same size and modification time is not read again.
"""
import argparse
import glob
import json
import os
import subprocess
import sys
import time

from . import camera, resolve_io
from .extract import read_clip, tc_to_frames

CATALOG = os.path.expanduser('~/Library/Application Support/Log MetaRaw FCP/catalog.json')
VERSION = 1
EXTS = ('.mp4', '.mov', '.mxf', '.mts', '.m2ts')
NTSC = {23.976: 24000 / 1001, 29.97: 30000 / 1001, 59.94: 60000 / 1001, 119.88: 120000 / 1001}


def libraries(extra=()):
    """Final Cut libraries on this Mac: ~/Movies, the ones Spotlight knows, and the ones given."""
    found = set(glob.glob(os.path.expanduser('~/Movies/*.fcpbundle')))
    try:
        out = subprocess.run(['mdfind', 'kMDItemFSName == "*.fcpbundle"'], capture_output=True, text=True,
                             timeout=10).stdout
        found.update(p for p in out.splitlines() if p.endswith('.fcpbundle'))
    except (OSError, subprocess.SubprocessError):
        pass
    found.update(os.path.abspath(p) for p in extra)
    return sorted(p for p in found if os.path.isdir(p) and '/.Trash/' not in p)


def media_files(libs):
    """The camera files the libraries use: their Original Media entries (links to the originals, or copies)."""
    files = set()
    for lib in libs:
        for media in glob.glob(os.path.join(glob.escape(lib), '*', 'Original Media')):
            for dirpath, _dirs, names in os.walk(media, followlinks=False):
                for n in names:
                    if n.lower().endswith(EXTS):
                        real = os.path.realpath(os.path.join(dirpath, n))
                        if os.path.isfile(real):   # a disconnected drive leaves the link dangling: skip it
                            files.add(real)
    return sorted(files)


def _fps(meta):
    fps = meta.get('fps')
    if not fps:
        return None
    for nominal, exact in NTSC.items():
        if abs(fps - nominal) < 0.01:
            return exact
    return float(fps)


def start_seconds(meta, fps):
    """Source start time as Final Cut gives it to an effect: the start timecode as seconds, 0 without one."""
    tc = meta.get('start_tc')
    if not tc or not fps:
        return 0.0
    try:
        return tc_to_frames(tc, int(round(fps))) / fps
    except (ValueError, TypeError):
        return 0.0


def develop_values(r):
    """As-shot values the effect starts from, as the Resolve node reads them. None when not decodable."""
    m = r['meta']
    try:
        gamut, gamma = camera.camera_space(m)
    except Exception:
        return None
    if gamut not in camera.SPACE_CODE or gamma not in camera.GAMMA_CODE:
        return None
    k, tint, ei, estimated = camera.shot_values(m)
    line = resolve_io.summary_line(r) or os.path.basename(r['path'])
    if estimated:
        line += ' · WB non registrato (%dK stimato)' % k
    return {'camera': line, 'ei': ei, 'kelvin': k, 'tint': tint, 'space': camera.SPACE_CODE[gamut],
            'gamma': camera.GAMMA_CODE[gamma], 'space_name': gamut, 'gamma_name': gamma}


def entry(path):
    r = read_clip(path, max_samples=4)
    m = r['meta']
    fps = _fps(m)
    frames = m.get('duration_frames') or m.get('frames') or 0
    if not frames and fps and m.get('duration_s'):
        frames = round(m['duration_s'] * fps)
    return {'fps': fps, 'start': start_seconds(m, fps), 'duration': frames / fps if fps and frames else None,
            'values': develop_values(r)}


def update(output=CATALOG, extra_libraries=(), log=None):
    """Bring the index up to date. Returns (indexed, read now, failed)."""
    old = {}
    try:
        with open(output, encoding='utf-8') as fh:
            data = json.load(fh)
        if data.get('version') == VERSION:
            old = data.get('files', {})
    except (OSError, ValueError):
        pass
    files, read_now, failed = {}, 0, 0
    for path in media_files(libraries(extra_libraries)):
        try:
            st = os.stat(path)
        except OSError:
            continue
        prev = old.get(path)
        if prev and prev.get('size') == st.st_size and prev.get('mtime') == int(st.st_mtime):
            files[path] = prev
            continue
        try:
            e = entry(path)
        except Exception as exc:   # one unreadable file must not stop the index
            failed += 1
            if log:
                log('salto %s: %s' % (path, exc))
            continue
        e.update(size=st.st_size, mtime=int(st.st_mtime))
        files[path] = e
        read_now += 1
    os.makedirs(os.path.dirname(output), exist_ok=True)
    tmp = output + '.tmp'
    with open(tmp, 'w', encoding='utf-8') as fh:
        json.dump({'version': VERSION, 'updated': time.time(), 'files': files}, fh)
    os.replace(tmp, output)   # the effect never reads a half-written index
    return len(files), read_now, failed


def match(files, start, duration, frame, media=None):
    """The file of a clip from what the effect knows: exact start and duration, else the one file of that frame
    rate whose time range holds the frame being rendered. Same rule as DevelopEffect.swift (MetadataCatalog)."""
    fps = 1.0 / frame if frame else None
    same = [(p, e) for p, e in files.items() if e.get('fps') and fps and abs(e['fps'] - fps) < 0.01]
    exact = [(p, e) for p, e in same if abs(e['start'] - start) < frame / 2 and e.get('duration')
             and abs(e['duration'] - duration) < frame * 1.5]
    if len(exact) == 1:
        return exact[0]
    if media is not None:
        inside = [(p, e) for p, e in same if e.get('duration') and e['start'] - frame / 2 <= media < e['start'] + e['duration']]
        if len(inside) == 1:
            return inside[0]
    return None


def main(argv=None):
    ap = argparse.ArgumentParser(description='Index the camera files of the Final Cut Pro libraries for Log MetaRaw.')
    ap.add_argument('--library', action='append', default=[], help='an extra .fcpbundle to index')
    ap.add_argument('--output', default=CATALOG)
    args = ap.parse_args(argv)
    total, read_now, failed = update(args.output, args.library, log=lambda s: print(s, file=sys.stderr))
    print('indice: %d file (%d letti ora, %d illeggibili) -> %s' % (total, read_now, failed, args.output))
    return 0


if __name__ == '__main__':
    sys.exit(main())

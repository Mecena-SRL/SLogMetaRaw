# SPDX-License-Identifier: GPL-3.0-or-later
"""Deep acquisition: every frame's motion and lens data, for stabilisation.

read(path) walks the whole Sony rtmd track (XAVC MP4, or MXF with an index) and
returns the gyroscope and accelerometer readings of each frame, as raw integers
with their scale, plus focus distance, focal length and iris per frame.
write(motion, clip_path) saves them next to the plugin record:

  <hash>.gcsv      Gyroflow IMU log (also loads in Gyroflow as a motion file)
  <hash>.lens.csv  frame, focus_m, focal_mm, iris_f

Tag layout from telemetry-parser (src/sony/rtmd_tags.rs, MIT, (c) AdrianEddy):
0xe43b/0xe44b = i32 count, i32 size (6), count x (x, y, z) i16 big-endian;
0xe439/0xe449 = LSB per unit (f32); 0xe438/0xe448 = unit flag (deg/s or rad/s,
g or m/s2); 0xe43a = axis order, one nibble per axis. Other cameras: not yet.
"""
import math
import os
import struct
import time

from . import mp4, mxf, plugin_cache, rtmd

GYRO, ACCEL = 0xe43b, 0xe44b
CONTAINER = 0x8300               # a local tag holding more local tags
LENS_KEYS = ('focus_distance_m', 'focal_length_mm', 'iris_fnumber')
FRAME_MAX = 4 << 20              # a damaged sample size must not pull in gigabytes
CONSTANTS = {                    # tag -> (key, decoder): read once, from the first frame holding them
    0xe435: ('gyro_hz', lambda d: struct.unpack('>i', d[:4])[0]),
    0xe438: ('gyro_rad', lambda d: d[0] != 0),
    0xe439: ('gyro_scale', lambda d: struct.unpack('>f', d[:4])[0]),
    0xe43a: ('orientation', lambda d: orientation(struct.unpack('>H', d[:2])[0])),
    0xe445: ('accel_hz', lambda d: struct.unpack('>i', d[:4])[0]),
    0xe448: ('accel_ms2', lambda d: d[0] != 0),
    0xe449: ('accel_scale', lambda d: struct.unpack('>f', d[:4])[0]),
    0xe40e: ('readout_ms', lambda d: struct.unpack('>i', d[:4])[0] / 1000.0),
}


def orientation(num):
    """Axis order of the IMU, e.g. 0x420 -> 'XYZ' (lowercase = inverted), first axis in the low nibble."""
    return ''.join('XxYyZz'[(num >> s) & 0xF] for s in (0, 4, 8))


def gyroflow_orientation(o):
    """Sony's axis order as Gyroflow spells it: X and Y swapped, Z inverted (telemetry-parser)."""
    return o[1] + o[0] + o[2].swapcase()


def _vectors(d):
    if len(d) < 8:
        return []
    count, size = struct.unpack('>ii', d[:8])
    if size != 6 or count <= 0:
        return []
    n = min(count, (len(d) - 8) // 6)
    return list(struct.iter_unpack('>hhh', d[8:8 + 6 * n]))


def _tags(body):
    """Local tags of a pack, descending into containers."""
    for tag, data in rtmd.iter_local_tags(body):
        if tag == CONTAINER:
            yield from _tags(data)
        else:
            yield tag, data


def _frame(payload, consts):
    """(gyro, accel, lens) of one frame; fills consts the first time each appears."""
    gyro, accel, lens = [], [], {}
    for _set, body in rtmd.iter_packs(payload):
        for tag, data in _tags(body):
            if tag == GYRO:
                gyro = _vectors(data)
            elif tag == ACCEL:
                accel = _vectors(data)
            elif tag in CONSTANTS:
                key, fn = CONSTANTS[tag]
                if key not in consts:
                    try:
                        consts[key] = fn(data)
                    except (struct.error, IndexError, ValueError):
                        pass
            else:
                spec = rtmd.TAGS.get(tag)
                if spec and spec[0] in LENS_KEYS:
                    try:
                        lens[spec[0]] = spec[2](data)
                    except Exception:   # malformed value: that field only
                        pass
    return gyro, accel, lens


def _mp4_payloads(f):
    m = mp4.MP4(f)
    track = m.track(handler=b'meta', codec=b'rtmd')
    if not track:
        return None, None
    n = track.sample_count()
    video = m.track(handler=b'vide')
    fps = None
    stts = video.tables.get(b'stts') if video else None
    if stts and len(stts) >= 16 and video.timescale:
        delta = struct.unpack('>I', stts[12:16])[0]
        fps = video.timescale / delta if delta else None
    offsets = track.sample_offsets(range(n))

    def payloads():
        for i in range(n):
            if i not in offsets:
                yield None
                continue
            buf = f.read_at(offsets[i], min(track.sample_size(i), FRAME_MAX))
            yield buf[struct.unpack('>H', buf[:2])[0]:] if len(buf) >= 2 else None
    return fps, (n, payloads())


def _mxf_payloads(f):
    lay = mxf.layout(f)
    start = mxf.content_start(f, lay) if lay else None
    if start is None:
        return None, None
    index = mxf.read_index(f, lay)
    units = index.count
    if index.edit_unit_bytes:
        units = ((lay['footer'] or f.size) - start) // index.edit_unit_bytes
    if units < 1:
        return None, None

    def payloads():
        for u in range(units):
            off = index.offset(u)
            yield mxf.read_package(f, start + off)[0] if off is not None else None
    return None, (units, payloads())


def read(path, fps=None, deadline=None):
    """Motion data of the whole clip, or None when the clip records none.

    fps: frame rate when the container does not state it (MXF: from the NRT XML).
    deadline: time.monotonic() after which reading stops ('partial': True)."""
    with mp4.CountingFile(path) as f:
        head = f.read_at(0, 16)
        own_fps, frames = (_mxf_payloads if mxf.is_mxf(head) else _mp4_payloads)(f)
        if not frames:
            return None
        total, payloads = frames
        consts = {}
        gyro, accel, lens = [], [], []
        partial = False
        for i, payload in enumerate(payloads):
            if deadline is not None and time.monotonic() > deadline:
                partial = True
                break
            g, a, l = _frame(payload, consts) if payload else ([], [], {})
            gyro.append(g)
            accel.append(a)
            lens.append(l)
    if not any(gyro):
        return None
    return dict(consts, source='Sony rtmd', fps=own_fps or fps, frames=total, frames_read=len(gyro),
                partial=partial, gyro=gyro, accel=accel, lens=lens,
                gyro_samples=sum(map(len, gyro)), accel_samples=sum(map(len, accel)))


def gcsv(motion):
    """Gyroflow's IMU log: raw integers, scaled by gscale (to deg/s) and ascale (to g).

    Each frame's readings are spread evenly over that frame, as telemetry-parser times them."""
    fps = motion.get('fps') or 25.0
    gscale = motion.get('gyro_scale') or 1.0
    if motion.get('gyro_rad'):
        gscale *= math.pi / 180
    ascale = motion.get('accel_scale') or 1.0
    if motion.get('accel_ms2'):
        ascale *= 9.80665
    lines = ['GYROFLOW IMU LOG', 'version,1.3', 'id,slogmetaraw',
             'orientation,%s' % gyroflow_orientation(motion.get('orientation') or 'XYZ'),
             'vendor,Sony']
    if motion.get('readout_ms'):
        lines += ['frame_readout_time,%g' % motion['readout_ms'], 'frame_readout_direction,0']
    lines += ['tscale,0.000001', 'gscale,%r' % gscale, 'ascale,%r' % ascale, 't,gx,gy,gz,ax,ay,az']
    for i, (g, a) in enumerate(zip(motion['gyro'], motion['accel'])):
        n = len(g)
        for j, v in enumerate(g):
            t = round((i + j / n) / fps * 1e6)
            acc = a[min(len(a) - 1, j * len(a) // n)] if a else (0, 0, 0)
            lines.append('%d,%d,%d,%d,%d,%d,%d' % ((t,) + v + acc))
    return '\n'.join(lines) + '\n'


def lens_csv(motion):
    def cell(v):
        return '' if v is None else '%g' % v
    rows = ['frame,focus_m,focal_mm,iris_f']
    for i, l in enumerate(motion['lens']):
        rows.append(','.join([str(i)] + [cell(l.get(k)) for k in LENS_KEYS]))
    return '\n'.join(rows) + '\n'


def write(motion, clip_path):
    """Write the IMU log and the lens table beside the plugin record; returns their paths."""
    base = plugin_cache.cache_path(clip_path)[:-len('.json')]
    os.makedirs(os.path.dirname(base), exist_ok=True)
    out = {}
    for ext, text in (('.gcsv', gcsv(motion)), ('.lens.csv', lens_csv(motion))):
        with open(base + ext, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write(text)
        out[ext] = base + ext
    return out


def summary(motion, files):
    """The small part that travels with the clip's result (the window shows it)."""
    keys = ('source', 'fps', 'frames', 'frames_read', 'partial', 'gyro_hz', 'accel_hz', 'orientation',
            'readout_ms', 'gyro_samples', 'accel_samples')
    s = {k: motion.get(k) for k in keys}
    s['focus_frames'] = sum(1 for l in motion['lens'] if 'focus_distance_m' in l)
    s['files'] = files
    return s


SECTION = 'MOVIMENTO (acquisizione profonda, per Stabilize e Gyroflow)'


def section(s):
    rows = [('Source', s['source']),
            ('Frames read', '%d / %d%s' % (s['frames_read'], s['frames'], ' (partial)' if s['partial'] else '')),
            ('Gyroscope', '%d readings · %s Hz' % (s['gyro_samples'], s['gyro_hz'] or '?')),
            ('Accelerometer', '%d readings · %s Hz' % (s['accel_samples'], s['accel_hz'] or '?')),
            ('IMU orientation', s['orientation'] or '?'),
            ('Frame readout time (rolling shutter)', '%.3f ms' % s['readout_ms'] if s['readout_ms'] else '—'),
            ('Focus position per frame', '%d frames' % s['focus_frames'])]
    rows += [('File ' + ext, p) for ext, p in sorted(s['files'].items())]
    return SECTION, rows


def deep(r, deadline=None):
    """Add the deep acquisition to a read_clip() result: files written, summary and section added.

    Returns the summary, or None when the clip has no motion data this reader knows."""
    m = read(r['path'], fps=r['meta'].get('fps'), deadline=deadline)
    if not m:
        return None
    s = r['motion'] = summary(m, write(m, r['path']))
    r['sections'].append(section(s))
    return s

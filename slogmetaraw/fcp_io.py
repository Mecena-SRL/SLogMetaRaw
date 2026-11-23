# SPDX-License-Identifier: GPL-3.0-or-later
"""Map Log MetaRaw results to a Final Cut Pro FCPXML 1.15 file (beta).

Final Cut Pro has no API to write metadata into an existing library. The route
that works is FCPXML: importing the file creates an Event with one clip per
source file, carrying the camera data as keywords, a note and metadata fields.
The source media is referenced in place (no copy).

Metadata fields use the generic <md key= type= displayName=> element of the
FCPXML DTD shipped in Final Cut Pro 13 (Interchange.framework). Whether the
Inspector shows custom keys in a given metadata view is still to be checked in
the application: the note and the keywords always show.

Usage: python3 -m slogmetaraw.fcp_io [-o OUT.fcpxml] [--event NAME] PATH [PATH...]
"""
import argparse
import os
import sys
import xml.etree.ElementTree as ET
from fractions import Fraction
from urllib.parse import quote

from . import resolve_io

FCPXML_VERSION = '1.15'
KEY_PREFIX = 'com.logmetaraw.'
NTSC_RATES = {24000 / 1001: (1001, 24000), 30000 / 1001: (1001, 30000), 60000 / 1001: (1001, 60000),
              120000 / 1001: (1001, 120000)}

# resolve_io.build_fields labels -> (key suffix, FCPXML type); the label is the display name
FIELD_KEYS = {
    'Camera Manufacturer': 'manufacturer', 'Camera Type': 'model', 'Camera Serial #': 'serial',
    'Camera Firmware': 'firmware', 'Camera FPS': 'capture_fps', 'Shutter Angle': 'shutter_angle',
    'Shutter Speed': 'shutter_speed', 'Exposure Mode': 'exposure_mode', 'ISO': 'iso',
    'White Point (Kelvin)': 'white_point_k', 'White Balance Tint': 'tint', 'Lens Type': 'lens',
    'Camera Aperture': 'aperture', 'Focal Point (mm)': 'focal_mm', 'Distance': 'distance',
    'ND Filter': 'nd_filter', 'Codec Bitrate': 'bitrate', 'Gamma Notes': 'gamma',
    'Color Space Notes': 'gamut', 'Date Recorded': 'date_recorded', 'Mon Color Space': 'monitor_color_space',
    'LUT Used': 'lut_used', 'Sensor Area Captured': 'sensor',
}


def frame_duration(fps):
    """FCPXML rational frame duration: 29.97 -> '1001/30000s', 25 -> '100/2500s'."""
    for rate, (n, d) in NTSC_RATES.items():
        if abs(fps - rate) < 0.005:
            return n, d
    f = Fraction(fps).limit_denominator(1000)
    return f.denominator, f.numerator


def _time(frames, fd):
    n, d = fd
    return '%d/%ds' % (frames * n, d)


def file_url(path):
    return 'file://' + quote(os.path.abspath(path))


# The Log MetaRaw effect for Final Cut Pro (fcp/): its Motion Template and the keys of its published
# parameters, as Final Cut writes them in an exported FCPXML ("9999/<layer>/<image>/3/<filter>/<group>/<param>").
EFFECT_UID = '~/Effects.localized/Log MetaRaw/Log MetaRaw/Log MetaRaw.moef'
EFFECT_ROOT = '9999/10140/10143/3/10145'


def _key(*ids):
    """FCPXML key of a parameter of the effect, from its group path and ID (enum P of DevelopEffect.swift)."""
    return '/'.join([EFFECT_ROOT] + [str(i) for i in ids])


def develop_params(r):
    """[(name, key, value)] that start the effect from the camera's own values, like the Resolve node does.
    None when the clip's profile is not one the effect decodes (it is then left without the effect)."""
    from . import camera
    m = r['meta']
    try:
        gamut, gamma = camera.camera_space(m)
    except Exception:
        return None
    if gamut not in camera.SPACE_CODE or gamma not in camera.GAMMA_CODE:
        return None
    k, tint, ei, estimated = camera.shot_values(m)
    s, g = camera.SPACE_CODE[gamut], camera.GAMMA_CODE[gamma]
    line = resolve_io.summary_line(r) or os.path.basename(r['path'])
    if estimated:
        line += ' · WB non registrato (%dK stimato)' % k
    return [
        ('Camera', _key(1), line),
        ('Bilanciamento', _key(100, 101), '0 (As shot)'),
        ('Temperatura', _key(100, 102), str(k)),                 # as shot: the develop starts neutral
        ('Tint', _key(100, 104), '%g' % tint),
        ('EI', _key(100, 111), str(ei)),
        ('Spazio colore', _key(200, 210, 211), '%d (%s)' % (s, gamut)),
        ('Gamma', _key(200, 210, 212), '%d (%s)' % (g, gamma)),
        ('EI di ripresa', _key(400, 401), str(ei)),
        ('Kelvin di ripresa', _key(400, 402), str(k)),
        ('Tint di ripresa', _key(400, 403), '%g' % tint),
    ]


def _filter_element(ref, params):
    fv = ET.Element('filter-video', ref=ref, name='Log MetaRaw')
    for pname, pkey, pvalue in params:
        ET.SubElement(fv, 'param', name=pname, key=pkey, value=pvalue)
    return fv


def _fields(r):
    out = []
    fields = resolve_io.build_fields(r)
    for label, suffix in FIELD_KEYS.items():
        if label in fields:
            out.append((KEY_PREFIX + suffix, label, fields[label]))
    return out


def build_fcpxml(results, event_name='Log MetaRaw', effect=True):
    """results: iterable of extract.read_clip() dicts. Returns the FCPXML text.
    effect=True applies the Log MetaRaw effect to every clip it can decode, set to the clip's as-shot values."""
    root = ET.Element('fcpxml', version=FCPXML_VERSION)
    resources = ET.SubElement(root, 'resources')
    event = ET.SubElement(ET.SubElement(root, 'library'), 'event', name=event_name)
    formats = {}
    effect_ref = None
    for n, r in enumerate(results, 1):
        m = r['meta']
        w, h, fps = m.get('v_width'), m.get('v_height'), m.get('fps')
        if not (w and h and fps):
            continue   # no picture geometry: FCP could not place the clip
        frames = m.get('duration_frames') or m.get('frames') or round((m.get('duration_s') or 0) * fps)
        if not frames:
            continue   # no duration: skip before a format is created for it
        fd = frame_duration(fps)
        key = (w, h, fd)
        if key not in formats:
            fid = 'r%d' % (len(formats) + 1)
            formats[key] = fid
            ET.SubElement(resources, 'format', id=fid, name='FFVideoFormat%dx%d' % (w, h),
                          frameDuration=_time(1, fd), width=str(w), height=str(h))
        fid = formats[key]
        name = os.path.splitext(os.path.basename(r['path']))[0]
        aid = 'a%d' % n
        asset = ET.SubElement(resources, 'asset', id=aid, name=name, start='0s',
                              duration=_time(frames, fd), hasVideo='1', format=fid)
        ET.SubElement(asset, 'media-rep', kind='original-media', src=file_url(r['path']))
        clip = ET.SubElement(event, 'asset-clip', ref=aid, name=name, duration=_time(frames, fd),
                             start='0s', format=fid)
        ET.SubElement(clip, 'note').text = resolve_io.notes_block(r)
        kws = resolve_io.keywords(r)
        if kws:
            ET.SubElement(clip, 'keyword', start='0s', duration=_time(frames, fd), value=','.join(kws))
        params = develop_params(r) if effect else None
        if params:
            if effect_ref is None:
                effect_ref = 'fx1'
                ET.SubElement(resources, 'effect', id=effect_ref, name='Log MetaRaw', uid=EFFECT_UID)
            clip.append(_filter_element(effect_ref, params))
        meta = ET.SubElement(clip, 'metadata')
        for key_, label, value in _fields(r):
            ET.SubElement(meta, 'md', key=key_, value=str(value), type='string', displayName=label)
        if not len(meta):
            clip.remove(meta)
    ET.indent(root)
    return '<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE fcpxml>\n' + ET.tostring(root, encoding='unicode') + '\n'


# Elements an exported FCPXML may hold after a clip's effects: our filter goes before them (DTD order)
_AFTER_FILTERS = ('filter-audio', 'metadata')


def _insert_filter(clip, fv):
    """Put the develop first among the clip's video filters, as the first node of a Resolve grade."""
    for old in [c for c in clip if c.tag == 'filter-video' and c.get('name') == 'Log MetaRaw']:
        clip.remove(old)
    kids = list(clip)
    for i, c in enumerate(kids):
        if c.tag in ('filter-video', 'filter-video-mask') or c.tag in _AFTER_FILTERS:
            clip.insert(i, fv)
            return
    clip.append(fv)


def apply_to_fcpxml(src_path, read=None):
    """Read an FCPXML exported by Final Cut Pro (file or .fcpxmld bundle) and return it with the Log MetaRaw effect,
    set from each source file's metadata, on every clip of a camera profile it decodes.
    Returns (text, applied, skipped)."""
    from urllib.parse import unquote, urlparse
    if read is None:
        from .extract import read_clip
        read = lambda p: read_clip(p, max_samples=8)
    if os.path.isdir(src_path):
        src_path = os.path.join(src_path, 'Info.fcpxml')
    tree = ET.parse(src_path)
    root = tree.getroot()
    resources = root.find('resources')
    files = {}
    for a in resources.iter('asset'):
        rep = a.find('media-rep')
        if rep is not None and rep.get('src', '').startswith('file://'):
            files[a.get('id')] = unquote(urlparse(rep.get('src')).path)
    ref = next((e.get('id') for e in resources.iter('effect') if e.get('uid') == EFFECT_UID), None)
    if ref is None:
        ref = 'rLogMetaRaw'
        ET.SubElement(resources, 'effect', id=ref, name='Log MetaRaw', uid=EFFECT_UID)
    cache, applied, skipped = {}, 0, 0
    for clip in root.iter('asset-clip'):
        path = files.get(clip.get('ref'))
        if not path:
            continue
        if path not in cache:
            try:
                cache[path] = develop_params(read(path))
            except Exception as e:
                print('salto %s: %s' % (path, e), file=sys.stderr)
                cache[path] = None
        if cache[path]:
            _insert_filter(clip, _filter_element(ref, cache[path]))
            applied += 1
        else:
            skipped += 1
    for tag in ('project', 'event'):   # imported next to the original, not merged into it
        for e in root.iter(tag):
            if e.get('name') and not e.get('name').endswith(' · Log MetaRaw'):
                e.set('name', e.get('name') + ' · Log MetaRaw')
    ET.indent(root)
    text = '<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE fcpxml>\n' + ET.tostring(root, encoding='unicode') + '\n'
    return text, applied, skipped


def collect_paths(paths):
    from .__main__ import EXTS
    found = []
    for p in paths:
        if os.path.isdir(p):
            for dirpath, _dirs, files in os.walk(p):
                found += [os.path.join(dirpath, f) for f in sorted(files) if f.lower().endswith(EXTS)]
        else:
            found.append(p)
    return found


def main(argv=None):
    ap = argparse.ArgumentParser(description='Read camera metadata and write a Final Cut Pro FCPXML '
                                             'with the Log MetaRaw effect set to each clip.')
    ap.add_argument('paths', nargs='*', help='clips or folders to import')
    ap.add_argument('-o', '--output', default='LogMetaRaw.fcpxml')
    ap.add_argument('--event', default='Log MetaRaw', help='Event name in Final Cut Pro')
    ap.add_argument('--no-effect', action='store_true', help='do not apply the Log MetaRaw effect')
    ap.add_argument('--apply', metavar='EXPORTED.fcpxml',
                    help='add the effect to the clips of an FCPXML exported from Final Cut Pro (File > Export XML)')
    args = ap.parse_args(argv)
    if args.apply:
        text, applied, skipped = apply_to_fcpxml(args.apply)
        with open(args.output, 'w', encoding='utf-8') as fh:
            fh.write(text)
        print('Log MetaRaw applicato a %d clip, %d senza profilo decodificabile: %s' % (applied, skipped, args.output))
        return 0 if applied else 1
    if not args.paths:
        ap.error('indica clip o cartelle, oppure --apply')
    from .extract import read_clip
    results, skipped = [], 0
    for p in collect_paths(args.paths):
        try:
            results.append(read_clip(p, max_samples=8))
        except Exception as e:   # one unreadable file must not stop the batch
            skipped += 1
            print('salto %s: %s' % (p, e), file=sys.stderr)
    text = build_fcpxml(results, args.event, effect=not args.no_effect)
    written = text.count('<asset-clip ')
    with open(args.output, 'w', encoding='utf-8') as fh:
        fh.write(text)
    print('%d clip scritte in %s (%d illeggibili, %d senza durata o geometria)'
          % (written, args.output, skipped, len(results) - written))
    return 0 if written else 1


if __name__ == '__main__':
    sys.exit(main())

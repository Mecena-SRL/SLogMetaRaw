# SPDX-License-Identifier: GPL-3.0-or-later
"""DJI per-frame metadata: the 'djmd' track (protobuf, one message per frame) of Osmo Action and similar cameras.

Field paths were read on an Osmo Action 5 Pro clip (schema 'dvtm_ac204.proto'); the Osmo Action 4 writes
'dvtm_ac203.proto' with the same layout according to exiftool's DJI tables. A path that is missing or holds an
unexpected type is skipped, so another schema costs the fields that moved, not the reading.
"""
import struct

# path of nested field numbers -> normalised key
CLIP_FIELDS = {(1, 1, 1): 'dji_schema', (1, 1, 5): 'serial', (1, 1, 6): 'firmware', (1, 1, 10): 'model'}
FRAME_FIELDS = {(3, 2, 3, 1): 'iso', (3, 2, 4, 1): 'shutter_speed', (3, 2, 6, 1): 'white_balance_k',
                (3, 2, 5, 1): 'digital_zoom'}


def _varint(d, i):
    r = s = 0
    while True:
        b = d[i]
        i += 1
        r |= (b & 127) << s
        s += 7
        if b < 128:
            return r, i


def fields(d):
    """[(field number, wire type, value)] of one protobuf message; stops quietly at the first damaged byte."""
    out, i = [], 0
    try:
        while i < len(d):
            key, i = _varint(d, i)
            num, wire = key >> 3, key & 7
            if wire == 0:
                v, i = _varint(d, i)
            elif wire == 1:
                v, i = d[i:i + 8], i + 8
            elif wire == 2:
                n, i = _varint(d, i)
                v, i = d[i:i + n], i + n
            elif wire == 5:
                v, i = d[i:i + 4], i + 4
            else:
                break
            out.append((num, wire, v))
    except IndexError:
        pass
    return out


def lookup(d, path):
    """(wire type, value) at a path of field numbers, or None."""
    for depth, num in enumerate(path):
        hit = next(((w, v) for n, w, v in fields(d) if n == num), None)
        if hit is None:
            return None
        if depth == len(path) - 1:
            return hit
        if hit[0] != 2:
            return None
        d = hit[1]
    return None


def _value(key, hit):
    wire, v = hit
    if key == 'shutter_speed' and wire == 2:   # two varints: numerator, denominator (1, 53 = 1/53 s)
        try:
            num, i = _varint(v, 0)
            den, _ = _varint(v, i)
            return (num, den) if num and den else None
        except IndexError:
            return None
    if wire == 5:
        x = struct.unpack('<f', v)[0]
        return int(round(x)) if key == 'iso' else round(x, 3)
    if wire == 2:
        return v.decode('utf-8', 'replace')
    return v


def read(d, table):
    out = {}
    for path, key in table.items():
        hit = lookup(d, path)
        if hit is not None:
            val = _value(key, hit)
            if val not in (None, ''):
                out[key] = val
    return out


# StreamMeta (path 2-4-1) colour mode, enum ColorModeType of DJI's dvtm_library.proto
COLOR_MODES = {19: ('D-Log M', 'DJI D-Log M Gamut'), 2: ('D-Log', 'D-Gamut'), 9: ('HLG', 'Rec.2020'),
               0: ('Rec.709', 'Rec.709'), 1: ('Rec.709', 'Rec.709')}   # Normal and D-Cinelike: video curves


def profile(sample, meta):
    """Capture curve from the colour mode; read on an Osmo Action 5 Pro (19 = D-Log M)."""
    hit = lookup(sample, (2, 4, 1))
    mode = hit[1] if hit and hit[0] == 0 else None
    if mode in COLOR_MODES:
        gamma, gamut = COLOR_MODES[mode]
        return {'capture_gamma': gamma, 'color_primaries': gamut, 'dji_color_mode': mode,
                'profile_source': 'tarato' if mode == 19 and meta.get('dji_schema', '').startswith('dvtm_ac20')
                else 'compatibile'}
    return {'dji_color_mode': mode} if mode is not None else {}


def clip_values(sample):
    out = read(sample, CLIP_FIELDS)
    if out.get('model', '').startswith('DJI '):   # "DJI OsmoAction5 Pro": the maker is its own field
        out['model'] = out['model'][4:]
    return out


def frame_values(sample):
    return read(sample, FRAME_FIELDS)

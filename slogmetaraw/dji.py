# SPDX-License-Identifier: GPL-3.0-or-later
"""DJI per-frame metadata: the 'djmd' track (protobuf, one message per frame) of Osmo Action and similar cameras.

Field paths were read on an Osmo Action 5 Pro clip (schema 'dvtm_ac204.proto'); the Osmo Action 4 writes
'dvtm_ac203.proto' with the same layout according to exiftool's DJI tables. A path that is missing or holds an
unexpected type is skipped, so another schema costs the fields that moved, not the reading.
"""
import math
import struct

# path of nested field numbers -> normalised key
CLIP_FIELDS = {(1, 1, 1): 'dji_schema', (1, 1, 5): 'serial', (1, 1, 6): 'firmware', (1, 1, 10): 'model'}
FRAME_FIELDS = {(3, 2, 3, 1): 'iso', (3, 2, 4, 1): 'shutter_speed', (3, 2, 6, 1): 'white_balance_k',
                (3, 2, 5, 1): 'digital_zoom'}
# numbers outside these ranges belong to another schema that moved the field: dropped, not shown
RANGES = {'iso': (1, 1e6), 'white_balance_k': (1000, 50000), 'digital_zoom': (0.01, 1000)}
TEXT_MAX = 128
SAMPLE_MAX = 64 * 1024   # an Osmo Action message is ~300 bytes: a bigger one is read only this far
FIELDS_MAX = 1024        # fields looked at per message level (~20 in a real one): damage must not cost seconds


class Damaged(ValueError):
    pass


def _varint(d, i):
    """(value, next index); at most 10 bytes, as protobuf writes them."""
    r = 0
    for s in range(0, 70, 7):
        b = d[i]
        i += 1
        r |= (b & 127) << s
        if b < 128:
            return r, i
    raise Damaged('varint')


def iter_fields(d):
    """(field number, wire type, value) of one protobuf message; stops quietly at the first damaged byte."""
    i, n = 0, len(d)
    try:
        for _ in range(FIELDS_MAX):
            if i >= n:
                return
            key, i = _varint(d, i)
            num, wire = key >> 3, key & 7
            if wire == 0:
                v, i = _varint(d, i)
            elif wire in (1, 2, 5):
                size = 8 if wire == 1 else 4 if wire == 5 else None
                if size is None:
                    size, i = _varint(d, i)
                if i + size > n:
                    return   # cut off: the rest of the message is not there
                v, i = d[i:i + size], i + size
            else:
                return   # groups and unknown wire types: not written by DJI, so the bytes are damaged
            yield num, wire, v
    except (IndexError, Damaged):
        return


def fields(d):
    return list(iter_fields(d))


def lookup(d, path):
    """(wire type, value) at a path of field numbers, or None."""
    for depth, num in enumerate(path):
        hit = next(((w, v) for n, w, v in iter_fields(d) if n == num), None)
        if hit is None:
            return None
        if depth == len(path) - 1:
            return hit
        if hit[0] != 2:
            return None
        d = hit[1]
    return None


def _text(v):
    t = ''.join(ch for ch in v[:TEXT_MAX].decode('utf-8', 'replace') if ch.isprintable()).strip()
    return t or None


def _value(key, hit):
    """The value of a field as the reader names it, or None when its type or size does not fit the key."""
    wire, v = hit
    if key in ('dji_schema', 'serial', 'firmware', 'model'):
        return _text(v) if wire == 2 else None
    if key == 'shutter_speed':   # two varints: numerator, denominator (1, 53 = 1/53 s)
        if wire != 2:
            return None
        try:
            num, i = _varint(v, 0)
            den, _ = _varint(v, i)
        except (IndexError, Damaged):
            return None
        return (num, den) if 0 < num < 1 << 31 and 0 < den < 1 << 31 else None
    if wire == 5:
        x = struct.unpack('<f', v)[0]
    elif wire == 0:
        x = v
    else:
        return None
    lo, hi = RANGES[key]
    if not (math.isfinite(x) and lo <= x <= hi):
        return None
    return int(round(x)) if key in ('iso', 'white_balance_k') else round(float(x), 3)


def read(d, table):
    out = {}
    for path, key in table.items():
        hit = lookup(d, path)
        if hit is not None:
            val = _value(key, hit)
            if val is not None:
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
                'profile_source': 'tarato' if mode == 19 and str(meta.get('dji_schema') or '').startswith('dvtm_ac20')
                else 'compatibile'}
    return {'dji_color_mode': mode} if mode is not None else {}


def clip_values(sample):
    out = read(sample, CLIP_FIELDS)
    if str(out.get('model') or '').startswith('DJI '):   # "DJI OsmoAction5 Pro": the maker is its own field
        out['model'] = out['model'][4:]
    return out


def frame_values(sample):
    return read(sample, FRAME_FIELDS)

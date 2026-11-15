# SPDX-License-Identifier: GPL-3.0-or-later
"""Camera metadata outside Sony's rtmd/NRT: QuickTime keys, vendor XML, the codec's colour boxes.

read(m, meta) fills the same normalised keys the Sony reader uses (manufacturer, model, iso,
white_balance_k, tint, shutter_angle, iris_fnumber, lens, capture_gamma, color_primaries...) and
says how much the reading can be trusted in meta['profile_source']:

  'tarato'       a reader checked on real clips of that camera (Panasonic clip XML, Blackmagic Camera app,
                 Apple 'logs' box);
  'compatibile'  the profile was found by name somewhere in the metadata (any camera, any vendor key);
  'stimato'      nothing names a profile: the codec's colour description (VUI / colr) decides, Rec.709 if silent.
"""
import math
import re

from . import nrt

# Profile names as cameras write them -> (gamma, gamut) as camera.py names them. Most specific first:
# "D-Log M" before "D-Log", "F-Log2" before "F-Log". The gamut is None when the curve alone does not fix it.
LOG_NAMES = (
    (r'apple[ _.-]?log[ _-]?2', 'Apple Log', 'Apple Wide Gamut'),   # same curve, wider gamut
    (r'apple[ _.-]?log', 'Apple Log', None),
    (r'\bd[ _-]?log[ _-]?m\b', 'D-Log M', None),
    (r'\bd[ _-]?log', 'D-Log', 'D-Gamut'),
    (r'\bv[ _-]?log', 'V-Log', 'V-Gamut'),
    (r'\bf[ _-]?log[ _-]?2', 'F-Log2', 'Rec.2020'),
    (r'\bf[ _-]?log', 'F-Log', 'Rec.2020'),
    (r'\bn[ _-]?log', 'N-Log', 'Rec.2020'),
    (r'\b(?:c|canon)[ _-]?log[ _-]?3', 'C-Log3', 'Cinema Gamut'),
    (r'\b(?:c|canon)[ _-]?log[ _-]?2', 'C-Log2', 'Cinema Gamut'),
    (r'\blog[ _-]?c[ _-]?4', 'LogC4', 'ARRI WG4'),
    (r'\blog[ _-]?c(?:[ _-]?3)?\b', 'LogC3', 'ARRI WG3'),
    (r'log3g10', 'Log3G10', 'RED WG'),
    (r'film[ _-]?gen[ _-]?5', 'BMD Film Gen5', 'BMD WG Gen5'),
    (r'\bhlg\d?\b|hybrid[ _-]?log', 'HLG', 'Rec.2020'),
)
GAMUT_NAMES = (
    (r'apple[ _-]?wide[ _-]?gamut', 'Apple Wide Gamut'),
    (r'v[ _-]?gamut', 'V-Gamut'), (r'd[ _-]?gamut', 'D-Gamut'), (r'cinema[ _-]?gamut', 'Cinema Gamut'),
    (r'(?:arri[ _-]?)?wide[ _-]?gamut[ _-]?4|awg4', 'ARRI WG4'), (r'(?:arri[ _-]?)?wide[ _-]?gamut[ _-]?3|awg3', 'ARRI WG3'),
    (r'red[ _-]?wide[ _-]?gamut', 'RED WG'), (r'wide[ _-]?gamut[ _-]?gen[ _-]?5', 'BMD WG Gen5'),
    (r'rec\.?[ _-]?2020|bt\.?[ _-]?2020', 'Rec.2020'), (r'rec\.?[ _-]?709|bt\.?[ _-]?709', 'Rec.709'),
)
# codec colour description -> camera.py names (only what a video file can declare)
VUI_GAMMA = {'ARIB STD-B67 (HLG)': 'HLG', 'BT.709': 'Rec.709', 'SMPTE 170M': 'Rec.709', 'IEC 61966-2-4': 'Rec.709', 'BT.2020 10-bit': 'Rec.709',
             'BT.2020 12-bit': 'Rec.709', 'sRGB': 'sRGB', 'Gamma 2.2': 'Gamma 2.2', 'Linear': 'Linear'}
VUI_GAMUT = {'BT.709': 'Rec.709', 'BT.2020': 'Rec.2020', 'SMPTE 170M': 'Rec.709', 'BT.470BG': 'Rec.709',
             'DCI-P3': 'P3 DCI', 'Display P3': 'P3 D65'}


def find_log(text):
    """(gamma, gamut) named in a free text, (None, None) when it names no log curve."""
    t = str(text or '').lower()
    gamma = next(((g, d) for rx, g, d in LOG_NAMES if re.search(rx, t)), (None, None))
    gamut = next((name for rx, name in GAMUT_NAMES if re.search(rx, t)), None)
    return gamma[0], gamut or gamma[1]


def _text(v):
    """A metadata value as printable text (bytes of an unknown QuickTime type decoded), None for numbers."""
    if isinstance(v, bytes):
        v = v.decode('utf-8', 'replace')
    if not isinstance(v, str):
        return None
    v = ''.join(ch for ch in v[:256] if ch.isprintable()).strip()
    return v or None


def _number(text):
    m = re.search(r'-?\d{1,15}(?:\.\d{1,15})?', str(text or ''))
    x = float(m.group()) if m else None
    return x if x is not None and math.isfinite(x) else None


def _int(text):
    n = _number(text)
    return int(round(n)) if n is not None else None


def _local(tag):
    return tag.rsplit('}', 1)[-1]


def _xml_values(xml):
    """{element local name: text} of every leaf element, the first occurrence of each name."""
    out = {}
    try:
        root = nrt.parse_xml(xml.encode('utf-8') if isinstance(xml, str) else xml)
    except nrt.XMLError:
        return out
    for el in root.iter():
        if len(el) == 0 and el.text and el.text.strip():
            out.setdefault(_local(el.tag), el.text.strip())
    return out


def _panasonic(xml, meta):
    """Panasonic clip XML (P2 / Semi-Pro schema), embedded as com.panasonic.Semi-Pro.metadata.xml (DC-S5)."""
    v = _xml_values(xml)
    put = {'manufacturer': v.get('Manufacturer'), 'model': v.get('ModelName'), 'serial': v.get('SerialNo.'),
           'iso': _int(v.get('ISOSensitivity')), 'white_balance_k': _int(v.get('WhiteBalanceColorTemperature')),
           'start_tc': v.get('StartTimecode'), 'creation_date': v.get('CreationDate') or v.get('StartDate'),
           'lens': v.get('LensModelName') or v.get('LensName')}
    meta.update({k: x for k, x in put.items() if x not in (None, '')})
    gamma = find_log(v.get('CaptureGamma'))[0]
    if gamma:
        meta['capture_gamma'] = gamma
        meta['color_primaries'] = find_log(v.get('CaptureGamut'))[1] or find_log(gamma)[1]
        meta['profile_source'] = 'tarato'


def _blackmagic(keys, meta):
    """Blackmagic Camera (iPhone app and cameras): com.blackmagic-design.camera.* text keys."""
    k = {name.rsplit('.', 1)[-1]: v for name, v in keys.items() if name.startswith('com.blackmagic-design.')}
    if not k:
        return
    put = {'iso': _int(k.get('iso')), 'white_balance_k': _int(k.get('whiteBalanceKelvin')),
           'tint': _number(k.get('whiteBalanceTint')), 'shutter_angle': _number(k.get('shutterAngle')),
           'iris_fnumber': _number(k.get('aperture')), 'lens': _text(k.get('lensType')),
           'capture_fps': _text(k.get('sensorFPS'))}
    meta.update({key: x for key, x in put.items() if x not in (None, '')})


def _apple(keys, meta):
    model = _text(keys.get('com.apple.quicktime.model'))
    make = _text(keys.get('com.apple.quicktime.make')) or _text(keys.get('com.apple.proapps.manufacturer'))
    if make:
        meta['manufacturer'] = re.sub(r'\s+Inc\.?$', '', make)
    if model:
        # "Apple iPhone 17 Pro 13mm": the lens rides along in the model name
        meta['model'] = re.sub(r'^Apple\s+', '', model)
    if _text(keys.get('com.apple.quicktime.software')):
        meta['firmware'] = _text(keys['com.apple.quicktime.software'])
    if _text(keys.get('com.apple.quicktime.creationdate')):
        meta.setdefault('creation_date', _text(keys['com.apple.quicktime.creationdate']))


def _apple_logs(name, meta):
    """Apple 'logs' box, '<vendor>.<gamut>.<curve>': com.apple.apple-wide-gamut.apple-log (iPhone 17 Pro)."""
    parts = name.split('.')
    if len(parts) < 4:
        return
    gamma, gamut = find_log(parts[-1])
    if gamma:
        meta['capture_gamma'] = gamma
        meta['color_primaries'] = find_log(parts[-2])[1] or gamut or 'Rec.2020'
        meta['profile_source'] = 'tarato'


def _udta(udta, meta):
    """QuickTime user data: maker and model as text atoms."""
    for atom, key in (('\xa9mak', 'manufacturer'), ('\xa9mod', 'model'), ('\xa9swr', 'firmware')):
        if _text(udta.get(atom)):
            meta.setdefault(key, _text(udta[atom]))


def read(m, meta, entry_info):
    """Fill meta from an MP4/MOV that the Sony reader does not cover. entry_info: codec.parse_sample_entry."""
    for name, value in m.keys.items():
        if name.startswith('com.panasonic.') and isinstance(value, str) and '<' in value:
            _panasonic(value, meta)
    _blackmagic(m.keys, meta)
    _apple(m.keys, meta)
    _udta(m.udta, meta)
    if entry_info.get('log_profile') and 'capture_gamma' not in meta:
        _apple_logs(entry_info['log_profile'], meta)
    if 'capture_gamma' not in meta:
        texts = [v for v in list(m.keys.values()) + list(m.udta.values()) if isinstance(v, str)]
        texts.append(entry_info.get('log_profile') or '')
        for text in texts:
            gamma, gamut = find_log(text)
            if gamma:
                meta['capture_gamma'], meta['color_primaries'] = gamma, gamut
                meta['profile_source'] = 'compatibile'
                break


def from_codec(meta):
    """Last resort: the colour description of the codec (VUI, then colr); Rec.709 when it says nothing."""
    transfer = meta.get('v_vui_transfer') if meta.get('v_vui_transfer') not in (None, 'Unspecified') \
        else meta.get('v_colr_transfer')
    primaries = meta.get('v_vui_primaries') if meta.get('v_vui_primaries') not in (None, 'Unspecified') \
        else meta.get('v_colr_primaries')
    meta['capture_gamma'] = VUI_GAMMA.get(transfer, 'Rec.709')
    meta['color_primaries'] = VUI_GAMUT.get(primaries, 'Rec.2020' if meta['capture_gamma'] == 'HLG' else 'Rec.709')
    meta['profile_source'] = 'stimato'

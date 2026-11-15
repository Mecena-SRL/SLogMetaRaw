# SPDX-License-Identifier: GPL-3.0-or-later
"""Camera colour and as-shot values used by the SLogMetaRaw plugin.

Codes match ofx/SLogMetaRaw/DevelopMath.h (gamut and transfer tables).
"""
import re

from . import vendors

SPACE_CODE = {'DaVinci WG': 0, 'Rec.709': 1, 'Rec.2020': 2, 'P3 D65': 3, 'P3 D60': 4, 'P3 DCI': 5,
              'S-Gamut': 6, 'S-Gamut3': 7, 'S-Gamut3.Cine': 8, 'ACES AP0': 9, 'ACES AP1': 10,
              'V-Gamut': 11, 'Apple Wide Gamut': 12, 'D-Gamut': 13, 'Cinema Gamut': 14, 'ARRI WG3': 15,
              'ARRI WG4': 16, 'RED WG': 17, 'BMD WG Gen5': 18, 'DJI D-Log M Gamut': 19}
GAMMA_CODE = {'DaVinci Intermediate': 0, 'Linear': 1, 'Gamma 2.2': 2, 'Gamma 2.4': 3, 'Gamma 2.6': 4,
              'Rec.709': 5, 'sRGB': 6, 'SLog': 7, 'SLog2': 8, 'SLog3': 9, 'ACEScct': 10,
              'V-Log': 11, 'Apple Log': 12, 'D-Log M': 13, 'D-Log': 14, 'F-Log': 15, 'F-Log2': 16, 'N-Log': 17,
              'C-Log2': 18, 'C-Log3': 19, 'LogC3': 20, 'LogC4': 21, 'Log3G10': 22, 'BMD Film Gen5': 23, 'HLG': 24}
# the gamut a curve implies when the file names only the curve
CURVE_GAMUT = {'Rec.709': 'Rec.709', 'sRGB': 'Rec.709', 'Gamma 2.2': 'Rec.709', 'Linear': 'Rec.709',
               'V-Log': 'V-Gamut', 'Apple Log': 'Rec.2020', 'D-Log M': 'DJI D-Log M Gamut', 'D-Log': 'D-Gamut',
               'F-Log': 'Rec.2020', 'F-Log2': 'Rec.2020', 'N-Log': 'Rec.2020', 'C-Log2': 'Cinema Gamut',
               'C-Log3': 'Cinema Gamut', 'LogC3': 'ARRI WG3', 'LogC4': 'ARRI WG4', 'Log3G10': 'RED WG',
               'BMD Film Gen5': 'BMD WG Gen5', 'HLG': 'Rec.2020'}
# Sony broadcast curves (rtmd / NRT names): decoded as the video curve they are built on
VIDEO_CURVE = re.compile(r'709|cine\d|standard|still|s-cinetone|hg\d', re.I)
# nominal Kelvin for the Sony "Lighting preset" when the camera did not record 0x810E
PRESET_KELVIN = {'Daylight': 5600, 'Cloudy': 6500, 'Incandescent': 3200, 'Fluorescent': 4000}


class NotSupported(Exception):
    pass


def curve(name):
    """(gamma, implied gamut) of a curve name written by any camera, (None, None) when not decodable."""
    name = str(name or '').strip()
    if name in GAMMA_CODE:
        return name, CURVE_GAMUT.get(name)
    gamma, gamut = vendors.find_log(name)
    if gamma in GAMMA_CODE:
        return gamma, gamut or CURVE_GAMUT.get(gamma)
    if VIDEO_CURVE.search(name):
        return 'Rec.709', 'Rec.709'
    return None, None


def camera_space(meta):
    """(gamut, gamma) the camera recorded, as named in the Camera Raw menus."""
    cs = meta.get('color_space') or ''
    for prefix, gamut, gamma in (('S-Gamut3.Cine/S-Log3', 'S-Gamut3.Cine', 'SLog3'),
                                 ('S-Gamut3/S-Log3', 'S-Gamut3', 'SLog3'),
                                 ('S-Gamut/S-Log2', 'S-Gamut', 'SLog2'),
                                 ('S-Gamut/S-Log', 'S-Gamut', 'SLog')):
        if cs.startswith(prefix):
            return gamut, gamma
    gamma, implied = curve(meta.get('capture_gamma') or meta.get('xml_gamma'))
    if not gamma:
        raise NotSupported('Profilo "%s" non riconosciuto: Log MetaRaw non sa decodificarlo.'
                           % (cs or meta.get('capture_gamma') or '?'))
    named = vendors.find_log(meta.get('color_primaries'))[1]
    gamut = named if named in SPACE_CODE else implied
    return gamut or 'Rec.709', gamma


def shot_values(meta):
    """As-shot (Kelvin, tint, EI, estimated_wb) from the clip metadata."""
    k = meta.get('white_balance_k')
    estimated = False
    if not k:
        k = PRESET_KELVIN.get(meta.get('lighting_preset'), 5600)
        estimated = True
    tint = max(-99.0, min(99.0, float(meta.get('tint') or 0)))   # the camera's own range
    ei = meta.get('exposure_index') or meta.get('iso') or 800
    return int(k), float(tint), int(ei), estimated

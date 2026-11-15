# SPDX-License-Identifier: GPL-3.0-or-later
"""Cameras other than Sony: synthetic MOV/MP4/MPEG files laid out as the real ones were (Panasonic DC-S5,
Blackmagic Camera on iPhone 17 Pro, DJI Osmo Action 5 Pro, a DV camcorder), and the compatible reading."""
import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from slogmetaraw import camera, extract, plugin_cache, resolve_io, vendors  # noqa: E402
from clip_fixtures import _box, _full  # noqa: E402

PANASONIC_XML = '''<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<ClipMain xmlns="urn:schemas-Professional-Plug-in:Semi-Pro:ClipMetadata:v1.0"><ClipContent>
<EssenceList><Video><StartTimecode>10:00:00:00</StartTimecode></Video></EssenceList>
<ClipMetadata><Device><Manufacturer>Panasonic</Manufacturer><ModelName>DC-S5</ModelName></Device></ClipMetadata>
</ClipContent><UserArea><AcquisitionMetadata xmlns="urn:schemas-Professional-Plug-in:P2:CameraMetadata:v1.2">
<CameraUnitMetadata><ISOSensitivity>4000</ISOSensitivity><WhiteBalanceColorTemperature>4000K</WhiteBalanceColorTemperature>
<Gamma><CaptureGamma>V-Log</CaptureGamma></Gamma><Gamut><CaptureGamut>V-Gamut</CaptureGamut></Gamut>
</CameraUnitMetadata></AcquisitionMetadata></UserArea></ClipMain>'''

BLACKMAGIC_KEYS = {'com.apple.proapps.manufacturer': 'Apple Inc.', 'com.apple.quicktime.model': 'Apple iPhone 17 Pro 13mm',
                   'com.blackmagic-design.camera.iso': '200', 'com.blackmagic-design.camera.whiteBalanceKelvin': '3200',
                   'com.blackmagic-design.camera.whiteBalanceTint': '0', 'com.blackmagic-design.camera.shutterAngle': '180°',
                   'com.blackmagic-design.camera.aperture': 'f/2.2'}


def keys_meta(keys):
    """moov/meta as QuickTime writes it: hdlr 'mdta', keys, ilst of UTF-8 'data' boxes."""
    names = list(keys)
    kbox = _full(b'keys', struct.pack('>I', len(names)) + b''.join(
        struct.pack('>I4s', 8 + len(n.encode()), b'mdta') + n.encode() for n in names))
    ilst = _box(b'ilst', b''.join(
        _box(struct.pack('>I', i + 1), _box(b'data', struct.pack('>II', 1, 0) + keys[n].encode()))
        for i, n in enumerate(names)))
    return _box(b'meta', _full(b'hdlr', b'\0\0\0\0mdta' + b'\0' * 13) + kbox + ilst)


def visual_entry(codec, children):
    return _box(codec, b'\0' * 6 + struct.pack('>H', 1) + b'\0' * 16 + struct.pack('>HH', 3840, 2160)
                + b'\0' * 50 + children)


def trak(handler, entry, samples=(), offsets=()):
    n = len(samples)
    stbl = _box(b'stbl', _full(b'stsd', struct.pack('>I', 1) + entry)
                + _full(b'stts', struct.pack('>III', 1, max(n, 1), 1))
                + _full(b'stsc', struct.pack('>IIII', 1, 1, 1, 1))
                + _full(b'stsz', struct.pack('>II', 0, n) + b''.join(struct.pack('>I', len(s)) for s in samples))
                + _full(b'stco', struct.pack('>I', n) + b''.join(struct.pack('>I', o) for o in offsets)))
    return _box(b'trak', _box(b'mdia', _full(b'mdhd', struct.pack('>IIIIHH', 0, 0, 25, max(n, 1), 0, 0))
                                + _full(b'hdlr', b'\0\0\0\0' + handler + b'\0' * 13) + _box(b'minf', stbl)))


def pb(num, value):
    """One protobuf field: int -> varint, float -> fixed32, bytes -> length-delimited."""
    def varint(v):
        out = b''
        while True:
            out += bytes([(v & 127) | (128 if v > 127 else 0)])
            v >>= 7
            if not v:
                return out
    if isinstance(value, float):
        return varint(num << 3 | 5) + struct.pack('<f', value)
    if isinstance(value, int):
        return varint(num << 3) + varint(value)
    return varint(num << 3 | 2) + varint(len(value)) + value


def djmd_sample(i, mode=19):
    header = pb(1, pb(1, pb(1, b'dvtm_ac204.proto') + pb(5, b'SERIAL') + pb(6, b'10.00.16.13')
                      + pb(10, b'DJI OsmoAction5 Pro')))
    stream = pb(2, pb(4, pb(1, mode)))
    frame = pb(3, pb(2, pb(3, pb(1, 1600.0)) + pb(4, pb(1, bytes([1, 50 + i]))) + pb(6, pb(1, 5600))))
    return header + stream + frame


class Cameras(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.dir.cleanup()

    def write(self, name, data):
        path = os.path.join(self.dir.name, name)
        with open(path, 'wb') as fh:
            fh.write(data)
        return path

    def mov(self, name, video_children=b'', moov_extra=b'', samples=(), sample_codec=b'djmd'):
        ftyp = _box(b'ftyp', b'qt  \0\0\0\0qt  ')
        start = len(ftyp) + 8
        offsets, pos = [], start
        for s in samples:
            offsets.append(pos)
            pos += len(s)
        mdat = _box(b'mdat', b''.join(samples))
        traks = trak(b'vide', visual_entry(b'hvc1', video_children))
        if samples:
            traks += trak(b'meta', _box(sample_codec, b'\0' * 8), samples, offsets)
        return self.write(name, ftyp + mdat + _box(b'moov', traks + moov_extra))

    def test_panasonic_clip_xml(self):
        r = extract.read_clip(self.mov('P1.MOV', moov_extra=keys_meta({'com.panasonic.Semi-Pro.metadata.xml': PANASONIC_XML})))
        m = r['meta']
        self.assertEqual((m['manufacturer'], m['model'], m['iso'], m['white_balance_k']), ('Panasonic', 'DC-S5', 4000, 4000))
        self.assertEqual((m['color_space'], m['profile_source'], m['level_required']), ('V-Gamut/V-Log', 'tarato', 'Full'))
        self.assertEqual(camera.camera_space(m), ('V-Gamut', 'V-Log'))

    def test_blackmagic_camera_on_iphone_with_apple_log_2(self):
        logs = _box(b'logs', b'com.apple.apple-wide-gamut.apple-log')
        r = extract.read_clip(self.mov('A.mov', logs + _box(b'colr', b'nclc' + struct.pack('>HHH', 2, 2, 9)),
                                       keys_meta(BLACKMAGIC_KEYS)))
        m = r['meta']
        self.assertEqual((m['manufacturer'], m['model']), ('Apple', 'iPhone 17 Pro 13mm'))
        self.assertEqual((m['iso'], m['white_balance_k'], m['shutter_angle'], m['iris_fnumber']), (200, 3200, 180.0, 2.2))
        self.assertEqual(camera.camera_space(m), ('Apple Wide Gamut', 'Apple Log'))
        self.assertEqual(m['level_required'], 'Video')

    def test_dji_djmd_track(self):
        r = extract.read_clip(self.mov('DJI_0001.MP4', samples=[djmd_sample(i) for i in range(50)]))
        m = r['meta']
        self.assertEqual((m['manufacturer'], m['model'], m['firmware']), ('DJI', 'OsmoAction5 Pro', '10.00.16.13'))
        self.assertEqual((m['iso'], m['shutter_speed'], m['white_balance_k']), (1600, (1, 50), 5600))
        self.assertEqual(camera.camera_space(m), ('DJI D-Log M Gamut', 'D-Log M'))
        self.assertEqual(r['changes']['shutter_speed']['last'], (1, 99))   # sampled across the clip

    def test_dji_other_colour_modes(self):
        m = extract.read_clip(self.mov('DJI_0002.MP4', samples=[djmd_sample(0, mode=9)]))['meta']
        self.assertEqual(camera.camera_space(m), ('Rec.2020', 'HLG'))

    def test_any_key_that_names_a_log_curve(self):
        m = extract.read_clip(self.mov('X.MOV', moov_extra=keys_meta({'com.example.camera.gamma': 'F-Log2'})))['meta']
        self.assertEqual((camera.camera_space(m), m['profile_source']), (('Rec.2020', 'F-Log2'), 'compatibile'))

    def test_nothing_named_falls_back_to_the_codec_and_keeps_the_manual_route(self):
        r = extract.read_clip(self.mov('REC.MOV', _box(b'colr', b'nclc' + struct.pack('>HHH', 9, 18, 9))))
        m = r['meta']
        self.assertEqual((camera.camera_space(m), m['profile_source']), (('Rec.2020', 'HLG'), 'stimato'))
        self.assertEqual(plugin_cache._record(r)['camera_found'], 0)
        self.assertTrue(any('descrizione colore del codec' in w for w in r['warnings']))

    def test_what_the_script_writes_in_resolve(self):
        r = extract.read_clip(self.mov('DJI_0003.MP4', samples=[djmd_sample(i) for i in range(5)]))
        f = resolve_io.build_fields(r)
        self.assertEqual((f['Camera Manufacturer'], f['Camera Type'], f['ISO'], f['White Point (Kelvin)']),
                         ('DJI', 'OsmoAction5 Pro', '1600', '5600'))
        self.assertEqual((f['Gamma Notes'], f['Shutter Speed']), ('D-Log M', '1/50'))
        r = extract.read_clip(self.mov('REC.MOV'))
        self.assertNotIn('Camera Manufacturer', resolve_io.build_fields(r))   # no camera: not written as Sony
        self.assertEqual(resolve_io.keywords(r), ['Rec.709'])

    def test_mpeg_program_stream(self):
        seq = b'\0\0\1\xb3' + bytes([720 >> 4, ((720 & 15) << 4) | (576 >> 8), 576 & 255, 0x23]) + b'\0' * 8
        r = extract.read_clip(self.write('M2U00001.MPG', b'\0\0\1\xba' + b'\0' * 10 + seq + b'\0' * 64))
        m = r['meta']
        self.assertEqual((r['container'], m['v_width'], m['v_height'], m['fps']), ('MPEG', 720, 576, 25.0))
        self.assertEqual(camera.camera_space(m), ('Rec.709', 'Rec.709'))

    def test_names_written_by_cameras(self):
        for text, expected in (('V-Log', ('V-Log', 'V-Gamut')), ('D-Log M', ('D-Log M', None)),
                               ('DLog', ('D-Log', 'D-Gamut')), ('ARRI LogC4', ('LogC4', 'ARRI WG4')),
                               ('Canon Log 3', ('C-Log3', 'Cinema Gamut')), ('Log Camera', (None, None)), ('C-Log3', ('C-Log3', 'Cinema Gamut')),
                               ('HLG2', ('HLG', 'Rec.2020')), ('Standard', (None, None))):
            self.assertEqual(vendors.find_log(text), expected, text)

    def test_every_camera_name_has_a_plugin_code(self):
        for gamma, gamut in camera.CURVE_GAMUT.items():
            self.assertIn(gamma, camera.GAMMA_CODE)
            self.assertIn(gamut, camera.SPACE_CODE)


if __name__ == '__main__':
    unittest.main()

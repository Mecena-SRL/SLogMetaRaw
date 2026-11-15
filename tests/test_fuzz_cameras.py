# SPDX-License-Identifier: GPL-3.0-or-later
"""Damaged clips of the cameras added in 3.1 (DJI djmd, Panasonic clip XML, Blackmagic/Apple keys): truncated,
corrupted and random files are read in bounded time and memory, and a value of an unexpected type costs that
value, never the clip."""
import os
import random
import struct
import sys
import tempfile
import time
import tracemalloc
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, 'tests'))

from slogmetaraw import camera, dji, extract, mp4, nrt, plugin_cache, resolve_io, vendors  # noqa: E402
from clip_fixtures import _box, _full  # noqa: E402
from test_cameras import (BLACKMAGIC_KEYS, PANASONIC_XML, djmd_sample, keys_meta, pb, trak,  # noqa: E402
                          visual_entry)

READ_SECONDS = 2.0          # per damaged clip, on a slow CI machine
READ_PEAK = 8 * 1024 * 1024


def mov(video_children=b'', moov_extra=b'', samples=(), moov_free=0):
    ftyp = _box(b'ftyp', b'qt  \0\0\0\0qt  ')
    start = len(ftyp) + 8
    offsets, pos = [], start
    for s in samples:
        offsets.append(pos)
        pos += len(s)
    traks = trak(b'vide', visual_entry(b'hvc1', video_children))
    if samples:
        traks += trak(b'meta', _box(b'djmd', b'\0' * 8), samples, offsets)
    free = _box(b'free', b'\0' * moov_free) if moov_free else b''
    return ftyp + _box(b'mdat', b''.join(samples)) + _box(b'moov', free + traks + moov_extra)


def full_read(r):
    """Everything the script and the plugin do with a result."""
    plugin_cache.build_record(r)
    resolve_io.summary_line(r)
    resolve_io.build_fields(r)
    return r


class Fuzz(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        logs = _box(b'logs', b'com.apple.apple-wide-gamut.apple-log')
        cls.clips = {
            'dji': mov(samples=[djmd_sample(i) for i in range(30)]),
            'panasonic': mov(moov_extra=keys_meta({'com.panasonic.Semi-Pro.metadata.xml': PANASONIC_XML})),
            'apple': mov(logs + _box(b'colr', b'nclc' + struct.pack('>HHH', 9, 2, 9)), keys_meta(BLACKMAGIC_KEYS)),
        }

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def read(self, data, name='x.MOV'):
        path = os.path.join(self.tmp.name, name)
        with open(path, 'wb') as fh:
            fh.write(data)
        tracemalloc.start()
        began = time.monotonic()
        try:
            return full_read(extract.read_clip(path))
        except (ValueError, OSError):
            return None   # a clean refusal; a struct/Index/Key/Attribute/TypeError is a defect
        finally:
            elapsed = time.monotonic() - began
            peak = tracemalloc.get_traced_memory()[1]
            tracemalloc.stop()
            self.assertLess(elapsed, READ_SECONDS, name)
            self.assertLess(peak, READ_PEAK, name)

    def test_the_clean_clips_read(self):
        self.assertEqual(self.read(self.clips['dji'])['meta']['model'], 'OsmoAction5 Pro')
        self.assertEqual(self.read(self.clips['panasonic'])['meta']['model'], 'DC-S5')
        self.assertEqual(self.read(self.clips['apple'])['meta']['capture_gamma'], 'Apple Log')

    def test_truncated_and_corrupted(self):
        rnd = random.Random(31)
        for kind, data in self.clips.items():
            with self.subTest(kind=kind):
                for cut in range(8, len(data), max(1, len(data) // 97)):
                    self.read(data[:cut])
                for _ in range(300):
                    b = bytearray(data)
                    for _ in range(rnd.choice((1, 2, 4, 16))):
                        b[rnd.randrange(len(b))] = rnd.choice((0, 0x7f, 0x80, 0xff, rnd.randrange(256)))
                    self.read(bytes(b))

    def test_random_bytes(self):
        rnd = random.Random(5)
        for n in (0, 1, 7, 16, 100, 4096, 65536):
            self.read(bytes(rnd.randrange(256) for _ in range(n)))
        # a QuickTime header followed by noise
        self.read(_box(b'ftyp', b'qt  \0\0\0\0') + bytes(rnd.randrange(256) for _ in range(20000)))


class DjiVariants(unittest.TestCase):
    """Another DJI schema may move a field or change its type: that field is dropped, the clip is read."""

    @staticmethod
    def at(path, leaf):
        for n in reversed(path[:-1]):
            leaf = pb(n, leaf)
        return leaf

    def values(self, sample):
        meta = {}
        meta.update(dji.clip_values(sample))
        meta.update(dji.profile(sample, meta))
        meta.update(dji.frame_values(sample))
        full_read({'meta': dict(meta, capture_gamma='D-Log M'), 'display': {}, 'path': '/x/DJI_0001.MP4',
                   'changes': {}, 'sampling': {}})
        return meta

    def test_every_field_with_every_wire_type(self):
        leaves = {'varint': 7, 'huge varint': 1 << 63, 'float': 1600.0, 'nan': float('nan'), 'inf': float('inf'),
                  'text': b'abc', 'empty': b'', 'message': pb(1, 5)}
        for path, key in list(dji.CLIP_FIELDS.items()) + list(dji.FRAME_FIELDS.items()) + [((2, 4, 1), 'mode')]:
            for kind, leaf in leaves.items():
                with self.subTest(key=key, kind=kind):
                    meta = self.values(self.at(path, pb(path[-1], leaf)))
                    v = meta.get(key)
                    if key in ('dji_schema', 'serial', 'firmware', 'model'):
                        self.assertTrue(v is None or isinstance(v, str))
                    elif key in ('iso', 'white_balance_k'):
                        self.assertTrue(v is None or isinstance(v, int))
                # a fixed32 cut off by the end of the message
                self.values(self.at(path, bytes([path[-1] << 3 | 5]) + b'\x01\x02'))

    def test_out_of_range_numbers_are_dropped(self):
        self.assertNotIn('white_balance_k', self.values(self.at((3, 2, 6, 1), pb(1, 7))))
        self.assertNotIn('iso', self.values(self.at((3, 2, 3, 1), pb(1, float('nan')))))
        self.assertEqual(self.values(self.at((3, 2, 3, 1), pb(1, 800)))['iso'], 800)

    def test_a_numeric_model_does_not_stop_the_reading(self):
        """dvtm_ac204 writes the model as text: a schema with a number there loses only the model."""
        sample = pb(1, pb(1, pb(1, b'dvtm_xx100.proto') + pb(10, 3))) + pb(3, pb(2, pb(3, pb(1, 1600.0))))
        meta = self.values(sample)
        self.assertNotIn('model', meta)
        self.assertEqual((meta['dji_schema'], meta['iso']), ('dvtm_xx100.proto', 1600))

    def test_a_damaged_message_costs_milliseconds(self):
        for junk in (b'\x08\x00' * 40000, b'\x1a\x02\x08\x00' * 20000, b'\xff' * 70000):
            began = time.monotonic()
            self.values(junk[:dji.SAMPLE_MAX])
            self.assertLess(time.monotonic() - began, 0.5)

    def test_a_varint_longer_than_ten_bytes_is_damage(self):
        self.assertEqual(dji.fields(b'\x08' + b'\x80' * 50 + b'\x01'), [])


class Values(unittest.TestCase):
    def test_shot_values_take_only_numbers(self):
        self.assertEqual(camera.shot_values({'white_balance_k': 'abc', 'iso': b'1', 'tint': 'x'}),
                         (5600, 0.0, 800, True))
        self.assertEqual(camera.shot_values({'white_balance_k': float('inf'), 'exposure_index': float('nan'),
                                             'iso': 640}), (5600, 0.0, 640, True))
        self.assertEqual(camera.shot_values({'white_balance_k': '4300', 'iso': 1600.0}), (4300, 0.0, 1600, False))

    def test_quicktime_values_that_are_not_text(self):
        meta = {}
        keys = dict(BLACKMAGIC_KEYS, **{'com.blackmagic-design.camera.lensType': b'\xff\xfe',
                                        'com.apple.quicktime.model': 7})
        vendors.read(type('M', (), {'keys': keys, 'udta': {'\xa9mod': b'x'}})(), meta, {})
        self.assertEqual((meta['lens'], meta['model']), ('\ufffd\ufffd', 'x'))   # text, whatever the type said
        self.assertEqual(vendors._number('9' * 400), 999999999999999.0)


class Xml(unittest.TestCase):
    BOMB = (b'<?xml version="1.0"?><!DOCTYPE l [<!ENTITY a "aaaaaaaaaa">'
            + b''.join(b'<!ENTITY %s "%s">' % (bytes([98 + i]), (b'&%s;' % bytes([97 + i])) * 10) for i in range(9))
            + b']><ClipMain><ModelName>&j;</ModelName></ClipMain>')

    def test_entities_are_never_expanded(self):
        with self.assertRaises(nrt.XMLError):
            nrt.parse_xml(self.BOMB)
        meta = {}
        vendors._panasonic(self.BOMB.decode(), meta)
        self.assertEqual(meta, {})
        external = b'<!DOCTYPE r [<!ENTITY x SYSTEM "file:///etc/passwd">]><r>&x;</r>'
        with self.assertRaises(nrt.XMLError):
            nrt.parse_xml(external)

    def test_utf16_is_refused(self):
        with self.assertRaises(nrt.XMLError):
            nrt.parse_xml('<?xml version="1.0" encoding="UTF-16"?><r/>'.encode('utf-16'))


class Reading(unittest.TestCase):
    def test_free_space_inside_moov_is_not_read(self):
        """The DC-S5 reserves 10 MB of 'free' inside moov: the reader skips it instead of loading it."""
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'P1.MOV')
            with open(path, 'wb') as fh:
                fh.write(mov(moov_extra=keys_meta({'com.panasonic.Semi-Pro.metadata.xml': PANASONIC_XML}),
                             moov_free=10 * 1024 * 1024))
            r = extract.read_clip(path)
        self.assertEqual(r['meta']['model'], 'DC-S5')
        self.assertLess(r['bytes_read'], 2 * mp4.MOOV_WINDOW + mp4.META_PRELOAD)
        self.assertLessEqual(r['reads'], 6)


if __name__ == '__main__':
    unittest.main()

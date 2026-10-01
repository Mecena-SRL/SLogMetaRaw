# SPDX-License-Identifier: GPL-3.0-or-later
"""Copying Sony values onto clips without metadata of their own (ProRes of an external recorder)."""
import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, 'tests'))

from slogmetaraw import read_clip, plugin_cache, resolve_io  # noqa: E402
import clip_fixtures as fx  # noqa: E402


class CopyToClip(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp()
        cls.sony = os.path.join(cls.tmp, 'C0001.MP4')
        fx.build_mp4(cls.sony, frames=30)
        cls.result = read_clip(cls.sony, max_samples=8)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self):
        self.cache = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.cache, True)
        patcher = mock.patch.object(plugin_cache, 'CACHE_DIR', self.cache)
        patcher.start()
        self.addCleanup(patcher.stop)

    def target(self, name='A001.mov'):
        clip = mock.Mock()
        clip.GetClipProperty.side_effect = lambda k: os.path.join(self.tmp, name) if k == 'File Path' else ''
        clip.GetMetadata.return_value = {}
        clip.SetMetadata.return_value = True
        return clip

    def test_pairs_follow_the_order_and_report_the_leftover(self):
        pairs, leftover = resolve_io.pair_clips(['a', 'b', 'c'], ['x', 'y'])
        self.assertEqual((pairs, leftover), ([('a', 'x'), ('b', 'y')], 1))

    def test_natural_order(self):
        names = ['C0010', 'C0002', 'C0001']
        self.assertEqual(sorted(names, key=resolve_io.natural_key), ['C0001', 'C0002', 'C0010'])

    def test_record_is_filed_under_the_target_and_leaves_the_data_level_alone(self):
        target = self.target()
        resolve_io.apply_copy(target, self.result, 'C0001.MP4')
        path = plugin_cache.cache_path(os.path.join(self.tmp, 'A001.mov'))
        self.assertTrue(os.path.isfile(path))
        import json
        with open(path, encoding='utf-8') as fh:
            rec = json.load(fh)
        self.assertEqual(rec['level_fix'], 0)
        self.assertEqual(rec['level_required'], -1)
        self.assertIn('copiato da C0001.MP4', rec['file'])
        self.assertEqual(rec['shot_temp'], plugin_cache.build_record(self.result)['shot_temp'])

    def test_notes_say_where_the_values_come_from_and_skip_file_specific_fields(self):
        target = self.target()
        resolve_io.apply_copy(target, self.result, 'C0001.MP4')
        fields = target.SetMetadata.call_args[0][0]
        self.assertIn('Copiato dalla clip Sony: C0001.MP4', fields['Camera Notes'])
        self.assertNotIn('Codec Bitrate', fields)

    def test_source_result_is_not_modified(self):
        before = dict(self.result['meta'])
        resolve_io.apply_copy(self.target(), self.result, 'C0001.MP4')
        self.assertEqual(self.result['meta'], before)
        self.assertNotIn('copied_from', self.result)


if __name__ == '__main__':
    unittest.main()

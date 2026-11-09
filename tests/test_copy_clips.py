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
        pairs, leftover, mode = resolve_io.pair_clips(['a', 'b', 'c'], ['x', 'y'])
        self.assertEqual((pairs, leftover, mode), ([('a', 'x'), ('b', 'y')], 1, 'sequence'))

    def test_timecode_pairs_survive_a_missing_recorder_clip(self):
        src = [resolve_io.tc_span('10:00:00:00', '10:01:00:00', '25'),
               resolve_io.tc_span('10:05:00:00', '10:06:00:00', '25'),
               resolve_io.tc_span('10:10:00:00', '10:11:00:00', '25')]
        # the recorder missed the second take and started a little late on the others
        dst = [resolve_io.tc_span('10:00:02:10', '10:01:00:00', '25'),
               resolve_io.tc_span('10:10:01:00', '10:11:00:00', '25')]
        pairs, leftover, mode = resolve_io.pair_clips(['a', 'b', 'c'], ['x', 'y'], src, dst)
        self.assertEqual((pairs, leftover, mode), ([('a', 'x'), ('c', 'y')], 1, 'timecode'))

    def test_each_original_goes_to_one_recorder_clip_only(self):
        src = [resolve_io.tc_span('01:00:00:00', '01:00:10:00', '24')]
        dst = [resolve_io.tc_span('01:00:00:00', '01:00:08:00', '24'),
               resolve_io.tc_span('01:00:09:00', '01:00:10:00', '24')]
        pairs, leftover, mode = resolve_io.pair_clips(['a'], ['x', 'y'], src, dst)
        self.assertEqual((pairs, leftover, mode), ([('a', 'x')], 1, 'timecode'))

    def test_rates_that_share_the_timecode_overlap(self):
        cam = resolve_io.tc_span('12:00:00:00', '12:00:30:00', '50')
        rec = resolve_io.tc_span('12:00:00:00', '12:00:30:00', '25')
        self.assertEqual(cam, rec)
        self.assertEqual(resolve_io.tc_span('00:00:00;00', '00:01:00;02', '29.97'), (0.0, 60.0))

    def test_without_shared_timecode_the_sequence_is_kept(self):
        src = [resolve_io.tc_span('10:00:00:00', '10:01:00:00', '25')] * 2
        dst = [resolve_io.tc_span('00:00:00:00', '00:01:00:00', '25'),   # recorder on free-run from zero
               resolve_io.tc_span('00:01:00:00', '00:02:00:00', '25')]
        pairs, leftover, mode = resolve_io.pair_clips(['a', 'b'], ['x', 'y'], src, dst)
        self.assertEqual((pairs, mode), ([('a', 'x'), ('b', 'y')], 'sequence'))
        # a clip without timecode turns the whole set to the sequence, so nothing is guessed
        pairs, _, mode = resolve_io.pair_clips(['a', 'b'], ['x', 'y'], [src[0], None], src)
        self.assertEqual(mode, 'sequence')

    def test_span_reads_the_clip_properties_and_rejects_bad_ones(self):
        props = {'Start TC': '23:59:50:00', 'End TC': '00:00:10:00', 'FPS': 25.0}
        clip = mock.Mock()
        clip.GetClipProperty.side_effect = props.get
        self.assertEqual(resolve_io.clip_span(clip), (86390.0, 86410.0))   # across midnight
        props['Start TC'] = ''
        self.assertIsNone(resolve_io.clip_span(clip))
        self.assertIsNone(resolve_io.tc_span('xx', '00:00:10:00', '25'))
        self.assertIsNone(resolve_io.tc_span('00:00:00:00', '00:00:10:00', ''))

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

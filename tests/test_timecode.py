# SPDX-License-Identifier: GPL-3.0-or-later
"""Timecode: non-drop and SMPTE drop-frame (29.97/59.94 DF) conversions."""
import unittest

from slogmetaraw import nrt
from slogmetaraw.extract import _fps_value, frames_to_tc, tc_to_frames


class Timecode(unittest.TestCase):
    def test_interlaced_rate_counts_fields(self):
        self.assertEqual(_fps_value('59.94i'), 29.97)
        self.assertEqual(_fps_value('50i'), 25.0)
        self.assertEqual(_fps_value('25p'), 25.0)
        self.assertIsNone(_fps_value('p'))

    def test_non_drop_is_unchanged(self):
        self.assertEqual(frames_to_tc(1800, 30), '00:01:00:00')
        self.assertEqual(tc_to_frames('01:00:00:00', 25), 90000)

    def test_drop_frame_skips_two_numbers_each_minute_but_the_tenth(self):
        self.assertEqual(frames_to_tc(1799, 30, True), '00:00:59;29')
        self.assertEqual(frames_to_tc(1800, 30, True), '00:01:00;02')
        self.assertEqual(frames_to_tc(17982, 30, True), '00:10:00;00')
        self.assertEqual(frames_to_tc(107892, 30, True), '01:00:00;00')
        self.assertEqual(frames_to_tc(3600, 60, True), '00:01:00;04')

    def test_drop_frame_round_trip(self):
        for fps, day in ((30, 2589408), (60, 5178816)):   # a DF day is 144 ten-minute blocks
            self.assertEqual(frames_to_tc(day, fps, True), '00:00:00;00')
            for n in range(0, day, 997):
                tc = frames_to_tc(n, fps, True)
                self.assertEqual(tc_to_frames(tc, fps), n, tc)

    def test_drop_frame_is_ignored_for_rates_without_it(self):
        self.assertEqual(frames_to_tc(1500, 25, True), '00:01:00:00')

    def test_ltc_drop_flag_selects_the_separator(self):
        self.assertEqual(nrt.ltc_to_tc('19071019'), '19:10:07:19')
        self.assertEqual(nrt.ltc_to_tc('42000100'), '00:01:00;02')


if __name__ == '__main__':
    unittest.main()

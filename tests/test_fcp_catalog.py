# SPDX-License-Identifier: GPL-3.0-or-later
"""Index of the Final Cut libraries' camera files: the timing an FxPlug sees -> the file and its as-shot values."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from slogmetaraw import fcp_catalog  # noqa: E402

FILES = {
    '/m/A.MXF': {'fps': 25.0, 'start': 62342.36, 'duration': 279.88, 'values': {'camera': 'A'}},
    '/m/B.MXF': {'fps': 25.0, 'start': 62700.0, 'duration': 60.0, 'values': {'camera': 'B'}},
    '/m/C.MP4': {'fps': 30000 / 1001, 'start': 0.0, 'duration': 10.0, 'values': {'camera': 'C'}},
}


class CatalogTest(unittest.TestCase):
    def test_exact_start_and_duration(self):
        # values logged by the effect in Final Cut Pro 13 for C006C096 (FX6, 25p)
        self.assertEqual(fcp_catalog.match(FILES, 62342.36, 279.88, 0.04, 62386.32)[0], '/m/A.MXF')

    def test_trimmed_clip_found_by_the_frame_it_renders(self):
        self.assertEqual(fcp_catalog.match(FILES, 62710.0, 5.0, 0.04, 62712.0)[0], '/m/B.MXF')

    def test_other_frame_rate_never_matches(self):
        self.assertIsNone(fcp_catalog.match(FILES, 62342.36, 279.88, 1001 / 24000, 62386.32))

    def test_start_seconds_from_timecode(self):
        self.assertAlmostEqual(fcp_catalog.start_seconds({'start_tc': '17:19:02:09'}, 25.0), 62342.36)
        ntsc = 30000 / 1001
        self.assertAlmostEqual(fcp_catalog.start_seconds({'start_tc': '00:00:01:00'}, ntsc), 30 * 1001 / 30000)
        self.assertEqual(fcp_catalog.start_seconds({}, 25.0), 0.0)


if __name__ == '__main__':
    unittest.main()

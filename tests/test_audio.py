# SPDX-License-Identifier: GPL-3.0-or-later
"""Log MetaRaw Audio engine (audio/src, tests/audio_test.cpp): presets render, switches bypass, stages behave."""
import os
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from plugin_build import test_bin  # noqa: E402


class Audio(unittest.TestCase):
    def test_engine(self):
        exe = test_bin('audio_test')
        if not exe:
            self.skipTest('binari di test non compilabili qui')
        run = subprocess.run([exe], capture_output=True, text=True, timeout=300)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()

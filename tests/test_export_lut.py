# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/export_lut.py (#13): the sampled LUT agrees with the independent Python model of the node."""
import math
import os
import shutil
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
import develop_model as dm  # noqa: E402
import export_lut  # noqa: E402


@unittest.skipUnless(shutil.which('c++') or shutil.which('g++') or shutil.which('clang++'), 'serve un compilatore C++')
class ExportLut(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = export_lut.build_evaluator()
        cls.tmp = tempfile.mkdtemp()

    def make(self, *args, size=5):
        path = os.path.join(self.tmp, 'lut.cube')
        self.assertEqual(export_lut.main([path, '--size', str(size), '--evaluator', self.exe, *args]), 0)
        with open(path, encoding='utf-8') as fh:
            lines = fh.read().splitlines()
        rows = [tuple(float(v) for v in l.split()) for l in lines if l and l[0] in '-0123456789']
        return lines, rows

    def test_header_and_size(self):
        lines, rows = self.make()
        self.assertIn('LUT_3D_SIZE 5', lines)
        self.assertEqual(len(rows), 5 ** 3)

    def test_matches_the_python_model_red_fastest(self):
        size = 5
        _, rows = self.make('--out-space', 'Rec.709', '--out-gamma', 'Rec.709', '--k', '4300', '--tint', '8',
                            '--ei', '1600', '--set', 'toneContrast=25', size=size)
        for idx in (1, 5, 31, 62, 93, 124):
            r, g, b = idx % size, (idx // size) % size, idx // size ** 2
            rgb = [c / (size - 1) for c in (r, g, b)]
            want = dm.develop(rgb, 8, 9, shot=(5600, 0, 800), temp=4300, tint=8, ei=1600, out_space=1, out_gamma=5,
                              tone={'toneContrast': 25})
            for got, exp in zip(rows[idx], want):
                self.assertTrue(math.isclose(got, exp, rel_tol=1e-3, abs_tol=2e-4), (idx, rows[idx], want))

    def test_grey_axis_stays_neutral_and_rises(self):
        size = 9
        _, rows = self.make(size=size)
        grey = [rows[i * (1 + size + size * size)] for i in range(size)]
        for r, g, b in grey:
            self.assertTrue(math.isclose(r, g, abs_tol=2e-4) and math.isclose(g, b, abs_tol=2e-4), (r, g, b))
        self.assertEqual([g[0] for g in grey], sorted(g[0] for g in grey))

    def test_unknown_control_is_refused(self):
        with self.assertRaises(SystemExit):
            export_lut.main([os.path.join(self.tmp, 'x.cube'), '--set', 'nope=1', '--evaluator', self.exe])


if __name__ == '__main__':
    unittest.main()

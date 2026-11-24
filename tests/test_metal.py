# SPDX-License-Identifier: GPL-3.0-or-later
"""The Metal kernel (compiled at runtime, as Resolve does) matches the CPU path."""
import subprocess
import unittest

from plugin_build import test_bin


class Metal(unittest.TestCase):
    def test_kernel_matches_cpu(self):
        exe = test_bin('metal_test')
        if not exe:
            self.skipTest('binari di test non compilabili qui')
        out = subprocess.run([exe], capture_output=True, text=True).stdout.strip()
        if out == 'NO_DEVICE':
            self.skipTest('no Metal device')
        self.assertTrue(out.startswith('MAXDIFF'), out)
        fields = out.split()
        self.assertLess(float(fields[1]), 2e-4)
        self.assertEqual(float(fields[3]), 0.0, 'il kernel Metal contrae a*b+c in una FMA')


class NoiseMetal(unittest.TestCase):
    """The Noise node on Metal: same picture as noiseRenderCPU (spatial, temporal, views, a missing neighbour)."""

    def run_bin(self, *args):
        exe = test_bin('noise_metal_test')
        if not exe:
            self.skipTest('binari di test non compilabili qui')
        res = subprocess.run([exe, *args], capture_output=True, text=True, timeout=600)
        out = res.stdout.strip()
        if out == 'NO_DEVICE':
            self.skipTest('no Metal device')
        return res, out

    def test_kernels_match_cpu(self):
        res, out = self.run_bin()
        self.assertTrue(out.startswith('MAXDIFF'), out + res.stderr)
        self.assertLess(float(out.split()[1]), 2e-4, res.stderr)

    def test_uhd_timing(self):
        res, out = self.run_bin('--time')
        print('\n' + out)   # in the CI log: GPU time of a UHD frame
        self.assertIn('UHD radius 2', out)


if __name__ == '__main__':
    unittest.main()

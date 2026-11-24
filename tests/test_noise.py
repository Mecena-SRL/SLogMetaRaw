# SPDX-License-Identifier: GPL-3.0-or-later
"""The Noise node: its CPU pipeline on synthetic frames (tests/noise_test.cpp) and the camera profiles.

What these check is behaviour on frames whose clean picture is known: white noise of a known σ, a still scene,
a moving one and a cut. Real footage (codec noise, in-camera noise reduction) is not modelled here."""
import os
import subprocess
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
from plugin_build import test_bin  # noqa: E402

CAMERAS = ('fx6_xavci', 'fx6_xavcl_high', 'fx6_raw', 'fx30_hs', 'fx30_120', 'iphone_prores', 'iphone_hevc',
           'action4', 'action5')


class NoisePipeline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        exe = test_bin('noise_test')
        if not exe:
            raise unittest.SkipTest('noise_test non compilabile qui')
        run = subprocess.run([exe], capture_output=True, text=True, timeout=120)
        assert run.returncode == 0, run.stdout + run.stderr
        cls.v = dict(line.split('=', 1) for line in run.stdout.splitlines() if '=' in line)

    def f(self, key):
        return float(self.v[key])

    def test_measure_finds_the_noise(self):
        """On a ramp with white noise the finest band measures 0.889 σ, within 10%."""
        self.assertEqual(self.v['measure_ok'], '1')
        for key in ('measured_luma0', 'measured_chroma0'):
            self.assertAlmostEqual(self.f(key) / self.f('expected_band0'), 1.0, delta=0.1, msg=key)

    def test_profile_text_roundtrip(self):
        self.assertEqual(self.v['text_roundtrip'], '1')
        self.assertEqual(self.v['text_rejects_garbage'], '1')

    def test_strength_zero_gives_the_picture_back(self):
        self.assertLess(self.f('zero_maxdiff'), 1e-5)
        self.assertEqual(self.v['alpha_kept'], '1')   # Resolve keys the node with the plugin's alpha: never change it

    def test_spatial_halves_the_error(self):
        """Threshold on the neighbourhood energy: 3.3 times less error (the coefficient alone gave 2.3)."""
        self.assertLess(self.f('spatial_rmse'), self.f('noisy_rmse') / 3.0)

    def test_spatial_keeps_the_detail(self):
        """The clean scene through the same filter loses less than a quarter of the noise σ (texture of 18 px,
        amplitude 2σ), and the noise left is under a quarter of what came in."""
        self.assertLess(self.f('detail_loss'), self.f('noisy_rmse') * 0.25)
        self.assertLess(self.f('noise_left'), self.f('noisy_rmse') * 0.25)

    def test_temporal_on_a_still_scene(self):
        """Five frames: ideally sqrt(5) = 2.24 times less noise; at least 2."""
        self.assertLess(self.f('still_temporal_rmse'), self.f('still_noisy_rmse') / 2.0)

    def test_temporal_follows_the_motion(self):
        """3 and 2 px per frame: aligned, the gain is the same as on the still scene (no smear on the edges)."""
        self.assertLess(self.f('moving_temporal_rmse'), self.f('moving_noisy_rmse') / 2.0)

    def test_a_cut_does_not_leak(self):
        """Two neighbours from another picture: only the three of the same shot count (sqrt(3) = 1.73)."""
        self.assertLess(self.f('cut_temporal_rmse'), self.f('still_noisy_rmse') / 1.4)

    def test_temporal_and_spatial_together(self):
        self.assertLess(self.f('full_rmse'), self.f('moving_noisy_rmse') / 5.0)

    def test_camera_profiles(self):
        for cam in CAMERAS:
            self.assertEqual(self.v['model_%s_finite' % cam], '1', cam)
            self.assertGreater(self.f('model_%s_dark' % cam), self.f('model_%s_grey' % cam), cam + ': ombre più rumorose')
        # the format decides the class and the temporal window
        self.assertEqual(self.v['model_fx6_raw_class'], '0')
        self.assertEqual(self.v['model_fx6_xavci_class'], '1')
        self.assertEqual(self.v['model_fx6_xavcl_high_class'], '3')   # UHD XAVC-L is 8 bit
        self.assertEqual(self.v['model_iphone_hevc_class'], '4')      # in-camera processing
        self.assertEqual(self.v['model_fx6_xavci_radius'], '2')
        self.assertEqual(self.v['model_action5_radius'], '1')
        # ProRes RAW has no in-camera noise reduction: noisier than XAVC-I at the same EI
        self.assertGreater(self.f('model_fx6_raw_dark'), self.f('model_fx6_xavci_dark'))
        # real-gain cameras: higher ISO, more noise
        self.assertGreater(self.f('model_action4_grey'), self.f('model_action5_grey'))
        self.assertIn('crop 120p', self.v['model_fx30_120_info'])


if __name__ == '__main__':
    unittest.main()

# SPDX-License-Identifier: GPL-3.0-or-later
"""Display renderings (math/Display.h): the model against OpenColorIO's ACES built-ins, the C++ kernel
against the model, and the properties the two own renderings promise (grey, white, black, range)."""
import os
import random
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import develop_model as dm  # noqa: E402
from model import display as D  # noqa: E402
from plugin_build import test_bin  # noqa: E402
from test_ofx_math import random_panel, tone_values  # noqa: E402

try:
    import numpy as np
    import PyOpenColorIO as ocio
except ImportError:
    ocio = None

REC709, REC2020, P3D65 = 1, 2, 3
SLOG3, SGAMUT3CINE, G24 = 9, 8, 3


def scene_xyz(rnd):
    return [rnd.choice([rnd.uniform(0, 1), rnd.uniform(0, 40), rnd.uniform(-0.1, 0.3)]) for _ in range(3)]


class DisplayModel(unittest.TestCase):
    @unittest.skipUnless(ocio, 'PyOpenColorIO non installato')
    def test_aces_matches_opencolorio(self):
        cfg = ocio.Config.CreateRaw()

        def proc(name):
            return cfg.getProcessor(ocio.BuiltinTransform(name)).getDefaultCPUProcessor()
        rgc = proc('ACES-LMT - ACES 1.3 Reference Gamut Compression')
        odt = proc('ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0')
        rnd = random.Random(3)
        for _ in range(2000):
            xyz = scene_xyz(rnd)
            a = np.array(dm.mul(dm.MATS[D.AP0][1], xyz), dtype=np.float32)
            rgc.applyRGB(a)
            odt.applyRGB(a)
            ref = [min(max(v, 0.0), 1.0) for v in dm.mul(dm.MATS[REC709][1], [float(v) for v in a])]
            got = D.render(xyz, 1, REC709)
            for r, g in zip(ref, got):
                self.assertAlmostEqual(r, g, delta=5e-5, msg='XYZ %s' % xyz)

    def test_grey_white_black(self):
        grey = [0.18 * w for w in (0.95046, 1.0, 1.08906)]
        for mode in (1, 2, 3):
            for space in (REC709, REC2020, P3D65):
                g = D.render(grey, mode, space)
                self.assertTrue(all(abs(v - g[0]) < 2e-3 for v in g), 'grigio neutro: %s' % g)
                self.assertTrue(0.08 < g[0] < 0.12, 'grigio 18%% vicino a 10 nit: %s' % g)
                self.assertLess(max(D.render([0.0, 0.0, 0.0], mode, space)), 1e-6)
                hot = D.render([1e4 * w for w in (0.95046, 1.0, 1.08906)], mode, space)
                self.assertTrue(all(v > 0.97 for v in hot), 'il bianco arriva al picco: %s' % hot)

    def test_own_renderings_are_monotonic_and_reach_white(self):
        """Luminanza and Filmico: brighter scene, brighter display; a saturated light ends near white."""
        red = dm.mul(dm.MATS[REC2020][0], [1.0, 0.02, 0.02])
        for mode in (2, 3):
            prev = -1.0
            for i in range(40):
                c = D.render([v * 2 ** (i / 3 - 6) for v in red], mode, REC709)
                y = sum(dm.MATS[REC709][0][1][k] * c[k] for k in range(3))
                self.assertGreaterEqual(y, prev - 1e-9)
                prev = y
            self.assertGreater(min(c), 0.8, 'il rosso molto esposto va verso il bianco: %s' % c)


class DisplayKernel(unittest.TestCase):
    def test_matches_model(self):
        exe = test_bin('ofx_math_test')
        if not exe:
            self.skipTest('binari di test non compilabili qui')
        rnd = random.Random(11)
        cases = []
        for i in range(180):
            rgb = [rnd.uniform(0.1, 0.95) for _ in range(3)]
            out = rnd.choice([(REC709, G24), (REC709, 5), (P3D65, G24), (REC2020, 6)])
            shot = (rnd.choice([3200, 5600]), 0.0, 800)
            panel = random_panel(rnd, rnd.choice([0.0, 0.3]))
            cases.append((rgb, out, shot, rnd.uniform(400, 3200), panel, 1 + i % 3))
        lines = [' '.join(repr(v) for v in rgb + [SGAMUT3CINE, SLOG3, out[0], out[1], *shot, shot[0], 0.0, ei,
                                                  0, 0, 0, 1.0, 0.0, 0, 0, 0, 0, 0, 0, 0]
                          + tone_values(panel) + [drt])
                 for rgb, out, shot, ei, panel, drt in cases]
        out = subprocess.run([exe], input='\n'.join(lines) + '\n', capture_output=True, text=True, check=True).stdout
        got = [[float(v) for v in l.split()] for l in out.strip().splitlines()]
        self.assertEqual(len(got), len(cases))
        for (rgb, out, shot, ei, panel, drt), g in zip(cases, got):
            exp = dm.develop(rgb, SGAMUT3CINE, SLOG3, shot=shot, ei=ei, out_space=out[0], out_gamma=out[1],
                             tone=panel, drt=drt)
            for a, b in zip(exp, g):
                self.assertAlmostEqual(a, b, delta=5e-4, msg='modo %d, %s -> C++ %s, modello %s' % (drt, rgb, g, exp))


if __name__ == '__main__':
    unittest.main()

# SPDX-License-Identifier: GPL-3.0-or-later
"""The Creative node's CPU pipeline (tests/creative_test.cpp): every process renders, behaves as its physics says
and is deterministic. Small frames stand in for full ones: the radii scale with the height."""
import math
import os
import subprocess
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
import develop_model as dm  # noqa: E402
from plugin_build import test_bin  # noqa: E402

SPACE, GAMMA = 8, 9          # S-Gamut3.Cine / S-Log3
W, H = 96, 40
PROCESSES = 16
NEGATIVE, BW, THREE_PLATES, KINEMACOLOR, ANALOG, DIGITAL = 0, 2, 7, 8, 14, 15
MOSAIC = (5, 6)               # Autochrome, Dufaycolor: colour grains even on grey
EDGED = (9, 10, 12, 13)       # mirror flip or a border at the edges


def ramp(shift=0, tinted=False):
    """Encoded pixels: twelve 8-px steps from -6 to +6 stops, optionally coloured bars sliding by `shift`."""
    img = []
    for y in range(H):
        for x in range(W):
            ev = -6.0 + 12.0 * (x // 8) / 11.0
            tint = (1.0, 1.0, 1.0)
            if tinted:
                tint = (1.0, 0.7, 0.5) if ((x + shift) // 6) % 2 else (0.5, 0.75, 1.0)
            img.append([dm.encode1(0.18 * 2 ** ev * c, GAMMA) for c in tint])
    return img


class Creative(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = test_bin('creative_test')

    def render(self, controls, frames, frame=7, threads=3, size=(W, H)):
        if not self.exe:
            self.skipTest('binari di test non compilabili qui')
        head = '%d %d %d %d %d %d %d\n' % (size[0], size[1], SPACE, GAMMA, frame, len(frames), threads)
        ctl = ' '.join('%s=%s' % kv for kv in controls.items()) + '\n'
        body = ''.join('%.7f %.7f %.7f\n' % tuple(px) for f in frames for px in f)
        res = subprocess.run([self.exe], input=head + ctl + body, capture_output=True, text=True, timeout=120)
        self.assertEqual(res.returncode, 0, res.stderr)
        return [tuple(map(float, line.split())) for line in res.stdout.splitlines()]

    @staticmethod
    def steps(out):
        """Mean of each step's centre (rows 16-23), per channel."""
        res = []
        for b in range(12):
            acc = [0.0, 0.0, 0.0]
            for y in range(16, 24):
                for x in range(b * 8 + 2, b * 8 + 6):
                    for c in range(3):
                        acc[c] += out[y * W + x][c] / 32.0
            res.append(acc)
        return res

    def test_every_process_renders_a_finite_display_picture(self):
        for proc in range(PROCESSES):
            with self.subTest(process=proc):
                out = self.render({'process': proc, 'dust': 0.5, 'scratches': 0.5, 'bloom': 0.3}, [ramp(tinted=True)])
                vals = [c for px in out for c in px]
                self.assertEqual(len(out), W * H)
                self.assertTrue(all(math.isfinite(v) for v in vals))
                self.assertGreaterEqual(min(vals), 0.0)
                self.assertLessEqual(max(vals), 1.001)

    def test_more_light_is_brighter(self):
        """The tone scale rises with the light (the daguerreotype is a mirror image: its scale runs backwards)."""
        for proc in range(PROCESSES):
            if proc in MOSAIC:
                continue
            with self.subTest(process=proc):
                y = [sum(s) / 3 for s in self.steps(self.render({'process': proc}, [ramp()]))]
                if proc == 9:
                    y = y[::-1]
                inner = y[1:-1] if proc in EDGED else y
                for a, b in zip(inner, inner[1:]):
                    self.assertGreaterEqual(b, a - 0.01)
                self.assertGreater(inner[-1] - inner[0], 0.3)

    def test_exposure_lifts_the_middle(self):
        for proc in (NEGATIVE, BW, ANALOG, DIGITAL):
            with self.subTest(process=proc):
                base = self.steps(self.render({'process': proc}, [ramp()]))[6]
                up = self.steps(self.render({'process': proc, 'exposure': 1}, [ramp()]))[6]
                self.assertGreater(sum(up), sum(base) + 0.05)

    def test_black_and_white_is_neutral(self):
        for px in self.render({'process': BW, 'filter': 3}, [ramp(tinted=True)]):
            self.assertAlmostEqual(px[0], px[1], delta=1e-4)
            self.assertAlmostEqual(px[1], px[2], delta=1e-4)

    def test_sequential_plates_split_what_moves(self):
        """Three plates exposed in sequence: a still subject stays grey, a moving one separates into colours."""
        still = [ramp(0, True)] * 3
        moving = [ramp(0, True), ramp(4, True), ramp(8, True)]
        def spread(out):
            return sum(max(px) - min(px) for px in out) / len(out)
        for proc in (THREE_PLATES, KINEMACOLOR):
            with self.subTest(process=proc):
                a = self.render({'process': proc, 'interval': 1}, still)
                b = self.render({'process': proc, 'interval': 1}, moving)
                c = self.render({'process': proc, 'interval': 0}, moving)
                self.assertNotEqual(a, b)
                self.assertEqual(a, c)   # simultaneous: only the current frame counts

    def test_every_stock_renders_and_rises_with_light(self):
        for name, count, proc in (('negative', 20, NEGATIVE), ('reversal', 6, 1), ('bwStock', 6, BW),
                                  ('print', 9, NEGATIVE), ('generation', 4, NEGATIVE)):
            for i in range(count):
                with self.subTest(stock=name, index=i):
                    y = [sum(s) / 3 for s in self.steps(self.render({'process': proc, name: i}, [ramp()]))]
                    self.assertTrue(all(math.isfinite(v) for v in y))
                    self.assertLess(y[1], y[6])
                    self.assertLess(y[6], y[10])

    @staticmethod
    def flat(ev=0.0, n=W * H):
        return [[dm.encode1(0.18 * 2 ** ev, GAMMA)] * 3 for _ in range(n)]

    def test_duplicates_and_print_add_their_grain(self):
        def rough(out):   # spread of the green channel over the middle rows
            g = [out[y * W + x][1] for y in range(10, 30) for x in range(10, 86)]
            m = sum(g) / len(g)
            return math.sqrt(sum((v - m) ** 2 for v in g) / len(g))
        base = rough(self.render({'process': NEGATIVE, 'printGrain': 0}, [self.flat()]))
        printed = rough(self.render({'process': NEGATIVE, 'printGrain': 3}, [self.flat()]))
        dupe = rough(self.render({'process': NEGATIVE, 'printGrain': 0, 'generation': 3}, [self.flat()]))
        self.assertGreater(printed, base * 1.05)
        self.assertGreater(dupe, base * 1.1)

    def test_every_medium_renders_and_changes_the_picture(self):
        plain = self.render({'process': NEGATIVE, 'grain': 0}, [ramp(tinted=True)])
        for medium in range(1, 8):
            with self.subTest(medium=medium):
                out = self.render({'process': NEGATIVE, 'grain': 0, 'medium': medium}, [ramp(tinted=True)])
                vals = [c for px in out for c in px]
                self.assertTrue(all(math.isfinite(v) and -1e-6 <= v <= 1.000001 for v in vals))
                self.assertNotEqual(out, plain)
                none = self.render({'process': NEGATIVE, 'grain': 0, 'medium': medium, 'mediumAmount': 0},
                                   [ramp(tinted=True)])
                if medium != 6:   # the console is a screen of its own even at 0: crop, four shades, bezel
                    self.assertLess(max(abs(a - b) for p, q in zip(none, plain) for a, b in zip(p, q)), 0.02)

    def test_log_output_ignores_the_medium(self):
        ctl = {'process': NEGATIVE, 'output': 1, 'print': 0}
        self.assertEqual(self.render(ctl, [ramp()]), self.render(dict(ctl, medium=3), [ramp()]))

    def test_tube_draws_scan_lines_that_keep_the_mean(self):
        w, h = 48, 4 * 576   # four pixels per line of a PAL tube
        img = self.flat(-1.0, w * h)
        ctl = {'process': DIGITAL, 'digital': 0, 'noise': 0, 'compression': 0}
        plain = self.render(ctl, [img], size=(w, h))
        tube = self.render(dict(ctl, medium=3), [img], size=(w, h))
        lin = lambda v: max(v, 0.0) ** 2.4  # noqa: E731  (Gamma 2.4)
        rows = [lin(tube[y * w + 4][1]) for y in range(h // 2 - 8, h // 2 + 8)]
        self.assertGreater(max(rows), 1.5 * min(rows))   # bright line centres, dark gaps
        mean = sum(lin(tube[y * w + x][1]) for y in range(h // 2 - 40, h // 2 + 40) for x in range(w)) / (80 * w)
        ref = lin(plain[(h // 2) * w + 4][1])
        self.assertAlmostEqual(mean / ref, 1.0, delta=0.12)   # masks and lines move light, they do not remove it

    def test_console_shows_four_greens(self):
        w, h = 320, 288   # under two pixels per LCD pixel: no grid
        out = self.render({'process': NEGATIVE, 'medium': 6}, [self.flat(0.0, w * h)], size=(w, h))
        greens = {(0x0f, 0x38, 0x0f), (0x30, 0x62, 0x30), (0x8b, 0xac, 0x0f), (0x9b, 0xbc, 0x0f)}
        centre = {tuple(round(c * 255) for c in out[y * w + x]) for y in range(130, 158) for x in range(140, 180)}
        self.assertTrue(centre <= greens, centre - greens)
        self.assertGreater(len(centre), 1)   # grey is dithered between two shades

    def test_paper_has_its_white_and_its_black(self):
        out = self.render({'process': NEGATIVE, 'medium': 7, 'mediumAmount': 1}, [ramp()])
        vals = [c for px in out for c in px]
        self.assertGreater(min(vals), 0.08)            # Dmax 2.1: about 0.8 % reflectance, 0.13 in Gamma 2.4
        self.assertLess(max(vals), 0.97)               # the paper's white is not the screen's

    def test_negative_scan_is_the_input_encoding(self):
        """Uscita Log: the negative scanned and re-encoded, flatter than the print and in the input's code range."""
        log = self.steps(self.render({'process': NEGATIVE, 'output': 1, 'print': 0}, [ramp()]))
        rec = self.steps(self.render({'process': NEGATIVE}, [ramp()]))
        span = lambda s: sum(s[-2]) / 3 - sum(s[1]) / 3
        self.assertLess(span(log), span(rec))
        grey = dm.encode1(0.18, GAMMA)
        self.assertAlmostEqual(sum(log[6]) / 3, grey, delta=0.12)

    def test_deterministic_and_thread_independent(self):
        ctl = {'process': NEGATIVE, 'grain': 2, 'dust': 1, 'weave': 1, 'halation': 2, 'bloom': 0.5}
        a = self.render(ctl, [ramp(tinted=True)], threads=1)
        self.assertEqual(a, self.render(ctl, [ramp(tinted=True)], threads=4))
        self.assertNotEqual(a, self.render(ctl, [ramp(tinted=True)], frame=8))   # grain and dust change per frame

    def test_signal_chains_soften_and_bleed(self):
        """VHS: the colour smears along the line far more than the luma; the broadcast chain keeps it."""
        img = ramp(tinted=True)
        def edge(out):   # sharpness of the colour (R - B) along a line
            c = [out[20 * W + x][0] - out[20 * W + x][2] for x in range(W)]
            return max(abs(a - b) for a, b in zip(c, c[1:]))
        vhs = edge(self.render({'process': ANALOG, 'analog': 1, 'noise': 0, 'instability': 0, 'wear': 0, 'heads': 0}, [img]))
        bc = edge(self.render({'process': ANALOG, 'analog': 0, 'noise': 0, 'instability': 0, 'wear': 0, 'heads': 0}, [img]))
        self.assertLess(vhs, bc)


if __name__ == '__main__':
    unittest.main()

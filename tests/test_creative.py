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


def ramp(shift=0, tinted=False, gamma=GAMMA):
    """Encoded pixels: twelve 8-px steps from -6 to +6 stops, optionally coloured bars sliding by `shift`."""
    img = []
    for y in range(H):
        for x in range(W):
            ev = -6.0 + 12.0 * (x // 8) / 11.0
            tint = (1.0, 1.0, 1.0)
            if tinted:
                tint = (1.0, 0.7, 0.5) if ((x + shift) // 6) % 2 else (0.5, 0.75, 1.0)
            img.append([dm.encode1(0.18 * 2 ** ev * c, gamma) for c in tint])
    return img


# The response rows set to the reference monitor's (the panel holds them: mediumLoaded = the medium shown).
FLAT_RESPONSE = {'mediumGamma': 2.4, 'mediumContrast': 100000, 'mediumAmbient': 0, 'mediumNits': 100,
                 'mediumKelvin': 6504, 'mediumColour': 100, 'mediumOverscan': 0, 'mediumRoom': 1}


class Creative(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = test_bin('creative_test')

    def render(self, controls, frames, frame=7, threads=3, size=(W, H), enc=(SPACE, GAMMA)):
        if not self.exe:
            self.skipTest('binari di test non compilabili qui')
        head = '%d %d %d %d %d %d %d\n' % (size[0], size[1], enc[0], enc[1], frame, len(frames), threads)
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

    def test_inputs_after_a_grade_read_the_same_light(self):
        """Rec.709, DaVinci WG, ACES (cct, cg, 2065-1) and Cineon: the same grey scale, encoded in each, develops
        into the same print as from S-Log3 (each decodes to the same scene light, grey 18%)."""
        self.assertAlmostEqual(dm.encode1(0.18, 25), 468.2 / 1023, delta=0.001)   # Cineon grey, Kodak's 470
        self.assertAlmostEqual(dm.decode1(dm.encode1(0.18, 5), 5), 0.18, places=6)
        base = self.steps(self.render({'process': NEGATIVE}, [ramp()]))
        for enc in ((0, 0), (1, 5), (10, 10), (10, 1), (9, 1), (1, 25)):
            with self.subTest(input=enc):
                got = self.steps(self.render({'process': NEGATIVE}, [ramp(gamma=enc[1])], enc=enc))
                for a, b in zip(base[1:-1], got[1:-1]):
                    for c in range(3):
                        self.assertAlmostEqual(a[c], b[c], delta=0.01)

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
        for medium in range(1, 18):
            with self.subTest(medium=medium):
                out = self.render({'process': NEGATIVE, 'grain': 0, 'medium': medium}, [ramp(tinted=True)])
                vals = [c for px in out for c in px]
                self.assertTrue(all(math.isfinite(v) and -1e-6 <= v <= 1.000001 for v in vals))
                self.assertNotEqual(out, plain)
                none = self.render({'process': NEGATIVE, 'grain': 0, 'medium': medium, 'mediumAmount': 0},
                                   [ramp(tinted=True)])
                if medium != 6 and medium < 12:   # consoles are devices of their own even at 0: frame, colours
                    self.assertLess(max(abs(a - b) for p, q in zip(none, plain) for a, b in zip(p, q)), 0.02)

    def test_log_output_ignores_the_medium(self):
        ctl = {'process': NEGATIVE, 'output': 1, 'print': 0}
        self.assertEqual(self.render(ctl, [ramp()]), self.render(dict(ctl, medium=3), [ramp()]))

    def test_tube_draws_scan_lines_that_keep_the_mean(self):
        w, h = 48, 4 * 576   # four pixels per line of a PAL tube
        img = self.flat(-1.0, w * h)
        ctl = {'process': DIGITAL, 'digital': 0, 'noise': 0, 'compression': 0}
        plain = self.render(ctl, [img], size=(w, h))
        tube = self.render(dict(ctl, medium=3, mediumLoaded=3, **FLAT_RESPONSE), [img], size=(w, h))   # lines alone
        lin = lambda v: max(v, 0.0) ** 2.4  # noqa: E731  (Gamma 2.4)
        rows = [lin(tube[y * w + 4][1]) for y in range(h // 2 - 8, h // 2 + 8)]
        self.assertGreater(max(rows), 1.5 * min(rows))   # bright line centres, dark gaps
        centre = [(x, y) for y in range(h // 2 - 40, h // 2 + 40) for x in range(18, 30)]   # clear of the edge
        mean = sum(lin(tube[y * w + x][1]) for x, y in centre) / len(centre)
        ref = lin(plain[(h // 2) * w + 4][1])
        self.assertAlmostEqual(mean / ref, 1.0, delta=0.12)   # masks and lines move light, they do not remove it

    def test_console_shows_four_greens(self):
        w, h = 320, 288   # under two pixels per LCD pixel: no grid
        out = self.render({'process': NEGATIVE, 'medium': 6}, [self.flat(0.0, w * h)], size=(w, h))
        greens = {(0x0f, 0x38, 0x0f), (0x30, 0x62, 0x30), (0x8b, 0xac, 0x0f), (0x9b, 0xbc, 0x0f)}
        centre = {tuple(round(c * 255) for c in out[y * w + x]) for y in range(130, 158) for x in range(140, 180)}
        self.assertTrue(centre <= greens, centre - greens)
        self.assertGreater(len(centre), 1)   # grey is dithered between two shades

    def console(self, medium, img, w, h):
        """A console's frame with the tube at 0: lines one per row, no glass or response, so colours read clean."""
        ctl = {'process': DIGITAL, 'digital': 0, 'noise': 0, 'compression': 0, 'medium': medium, 'mediumAmount': 0}
        out = self.render(ctl, [img], size=(w, h))
        return [[tuple(round(c * 255) for c in out[y * w + x]) for x in range(w)] for y in range(h)]

    @staticmethod
    def hues(w, h):   # a colour wheel across, darker downwards in bands of 60 rows (a beam mixes its neighbours)
        img = []
        for y in range(h):
            for x in range(w):
                t, v = x / w * 6.0, 0.9 - 0.7 * (y // 60) / 8
                rgb = [max(0.0, min(1.0, abs(((t + o) % 6.0) - 3.0) - 1.0)) for o in (0.0, 4.0, 2.0)]
                img.append([dm.encode1(0.18 * 4 * v * (0.15 + 0.85 * c), GAMMA) for c in rgb])
        return img

    def test_nes_shows_only_its_palette_inside_a_four_by_three_frame(self):
        w, h = 854, 480
        lo, hi = [0.350, 0.518, 0.962, 1.550], [1.094, 1.506, 1.962, 1.962]
        pal = set()
        for c in range(64):   # the 2C02's square wave, decoded as YIQ (same as nesPalette)
            hh, lv = c & 15, (c >> 4) & 3
            y = i = q = 0.0
            if hh < 14:
                vh, vl = (lo[lv] if hh == 13 else hi[lv]), (hi[lv] if hh == 0 else lo[lv])
                for s in range(12):
                    n = ((vh if (hh + s) % 12 < 6 else vl) - 0.518) / 1.444
                    a = math.pi / 6 * (s + 4)
                    y, i, q = y + n / 12, i + n * math.cos(a) / 6, q + n * math.sin(a) / 6
            rgb = (y + 0.946882 * i + 0.623557 * q, y - 0.274788 * i - 0.635691 * q, y - 1.108545 * i + 1.709007 * q)
            pal.add(tuple(round(255 * max(0.0, min(1.0, v))) for v in rgb))
        self.assertIn((208, 39, 17), pal)   # 0x16, the hardware's red
        out = self.console(12, self.hues(w, h), w, h)
        seen = {out[y][x] for y in range(h) if 8 <= y % 60 < 52 for x in range(120, w - 120)}
        far = [c for c in seen if min(sum(abs(a - b) for a, b in zip(c, e)) for e in pal) > 3]
        self.assertFalse(far, far[:5])
        self.assertGreater(len(seen), 12)
        self.assertEqual({out[y][x] for y in range(h) for x in list(range(100)) + list(range(w - 100, w))}, {(0, 0, 0)})

    def test_consoles_keep_their_bits_and_the_playstation_dithers(self):
        w, h = 640, 480
        on_grid = lambda v, n: abs(v / 255 * (n - 1) - round(v / 255 * (n - 1))) < 0.07  # noqa: E731
        sms = self.console(13, self.hues(w, h), w, h)
        self.assertTrue(all(on_grid(c, 4) for y in range(60, h - 60) if 8 <= y % 60 < 52 for px in sms[y] for c in px))
        self.assertEqual({px for y in range(40) for px in sms[y]}, {(0, 0, 0)})   # 192 of 240 lines: the border
        md = self.console(14, self.hues(w, h), w, h)
        levels = (0, 52, 87, 116, 144, 172, 206, 255)
        self.assertTrue(all(min(abs(c - l) for l in levels) <= 1 for y in range(24, h - 24) if 8 <= y % 60 < 52 for px in md[y] for c in px))
        for medium in (15, 17):   # the PlayStation dithers from line to line, which the beam mixes
            out = self.console(medium, self.hues(w, h), w, h)
            self.assertTrue(all(on_grid(c, 32) for y in range(24, h - 24) if 8 <= y % 60 < 52 for px in out[y] for c in px), medium)
        grey = [[dm.encode1(0.18 * 0.93, GAMMA)] * 3 for _ in range(w * h)]   # between two 5-bit steps
        ps1 = {px for y in range(200, 280) for px in self.console(16, grey, w, h)[y][300:340]}
        saturn = {px for y in range(200, 280) for px in self.console(17, grey, w, h)[y][300:340]}
        self.assertGreater(len(ps1), 1)
        self.assertEqual(len(saturn), 1)

    def test_duplicates_lose_sharpness(self):
        w, h = 32, 1080   # the blur is in px at 1080
        img = [[dm.encode1(0.18 * (4.0 if x >= 16 else 0.25), GAMMA)] * 3 for y in range(h) for x in range(w)]

        def spread(out):   # the edge's width: share of the rise outside its two central pixels
            row = [out[540 * w + x][1] for x in range(w)]
            return 1 - (row[16] - row[15]) / (row[w - 1] - row[0])
        ctl = {'process': NEGATIVE, 'grain': 0, 'printGrain': 0, 'halation': 0}
        sharp = spread(self.render(dict(ctl, generation=0), [img], size=(w, h)))
        soft = spread(self.render(dict(ctl, generation=2), [img], size=(w, h)))
        self.assertGreater(soft, sharp + 0.2)

    def test_screens_glow_around_bright_areas(self):
        w, h = 64, 64
        img = [[dm.encode1(0.18 * (16.0 if abs(x - 32) < 4 and abs(y - 32) < 4 else 0.02), GAMMA)] * 3
               for y in range(h) for x in range(w)]
        ctl = {'process': DIGITAL, 'digital': 0, 'noise': 0, 'compression': 0, 'medium': 1}
        lit = self.render(ctl, [img], size=(w, h))
        dark = self.render(dict(ctl, mediumAmount=0), [img], size=(w, h))
        near = (32 * w + 40)   # beside the bright square
        self.assertGreater(lit[near][1], dark[near][1])

    def test_console_keeps_part_of_the_previous_picture(self):
        w, h = 320, 288
        black, white = self.flat(-6.0, w * h), self.flat(3.0, w * h)
        ctl = {'process': NEGATIVE, 'medium': 6, 'grain': 0, 'printGrain': 0}
        still = self.render(ctl, [white, white], size=(w, h))
        trail = self.render(ctl, [white, black], size=(w, h))   # the last frame is the previous one
        centre = lambda out: sum(out[y * w + x][1] for y in range(130, 158) for x in range(140, 180))  # noqa: E731
        self.assertLess(centre(trail), centre(still))

    def test_each_medium_answers_with_its_own_tone_white_and_colour(self):
        """The preset's response: a home tube lifts the black (300:1) and closes the mids (gamma 2.5), the Japanese
        NTSC set is bluer than the PAL one, the cinema in the dark shows less contrast than the studio monitor."""
        ctl = {'process': DIGITAL, 'digital': 0, 'noise': 0, 'compression': 0, 'sharpness': 0}
        steps = lambda m, **kw: self.steps(self.render(dict(ctl, medium=m, **kw), [ramp()]))  # noqa: E731
        studio, pal, ntsc, cinema = steps(5), steps(3), steps(4), steps(1)
        self.assertGreater(pal[1][1], studio[1][1] + 0.02)      # deep shadow lifted by the tube's own black
        self.assertGreater(ntsc[8][2] / ntsc[8][0], pal[8][2] / pal[8][0] + 0.03)   # 9300 K against 7500 K
        span = lambda s: s[9][1] - s[2][1]  # noqa: E731
        self.assertLess(span(cinema), span(studio))

    def test_panel_values_drive_the_medium_once_loaded(self):
        """With the panel holding the medium's values (mediumLoaded), its rows count; without, the preset does."""
        ctl = {'process': DIGITAL, 'digital': 0, 'noise': 0, 'compression': 0, 'medium': 5}
        preset = self.steps(self.render(ctl, [ramp()]))
        ignored = self.steps(self.render(dict(ctl, mediumGamma=2.8), [ramp()]))
        used = self.steps(self.render(dict(ctl, mediumGamma=2.8, mediumLoaded=5), [ramp()]))
        self.assertEqual(preset, ignored)
        self.assertLess(used[6][1], preset[6][1] - 0.02)        # a higher gamma darkens the middle

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


    def test_every_menu_entry_and_text_exists(self):
        """A missing entry is a null pointer: Resolve crashed choosing the 4.2.0 viewing media without their text."""
        if not self.exe:
            self.skipTest('binari di test non compilabili qui')
        res = subprocess.run([self.exe], input='names\n', capture_output=True, text=True, timeout=60)
        self.assertEqual(res.returncode, 0, res.stdout + res.stderr)

    # ---- unsteadiness: camera, printer, projector

    def weave(self, controls, n=240, size=(1920, 1080)):
        """creativeWeave's offsets for frames 0..n-1: rows of (camera x, y, printer x, y, projector x, y)."""
        if not self.exe:
            self.skipTest('binari di test non compilabili qui')
        ctl = ' '.join('%s=%s' % kv for kv in controls.items()) + '\n'
        res = subprocess.run([self.exe], input='weave %d %d %d\n' % (size[0], size[1], n) + ctl,
                             capture_output=True, text=True, timeout=60)
        self.assertEqual(res.returncode, 0, res.stderr)
        return [tuple(map(float, line.split())) for line in res.stdout.splitlines()]

    @staticmethod
    def rms(v):
        return math.sqrt(sum(x * x for x in v) / len(v))

    @staticmethod
    def corr(v, lag):
        a, b = v[:-lag], v[lag:]
        ma, mb = sum(a) / len(a), sum(b) / len(b)
        num = sum((x - ma) * (y - mb) for x, y in zip(a, b))
        den = math.sqrt(sum((x - ma) ** 2 for x in a) * sum((y - mb) ** 2 for y in b))
        return num / den

    def test_camera_weave_drifts_sideways_and_jumps_vertically(self):
        """The claw errs at every frame (vertical, little memory); the side rail drifts (lateral, smooth); no
        rhythm (the old sine repeated every 7 frames)."""
        w = self.weave({'device': 1, 'filmCamera': 3, 'maker': 0})   # Canon 1014 XL-S, Super 8
        x, y = [r[0] for r in w], [r[1] for r in w]
        self.assertGreater(self.corr(x, 1), 0.75)
        self.assertLess(self.corr(y, 1), 0.5)
        self.assertLess(abs(self.corr(x, 7)), 0.6)
        # RMS about a third of the peak (0.9 * 0.8 % of the width, research: Super 8 cartridge), a few pixels in HD
        self.assertGreater(self.rms(x), 2)
        self.assertLess(self.rms(x), 8)

    def test_camera_weave_follows_the_mechanism_and_switches_off(self):
        s8 = self.weave({'device': 1, 'filmCamera': 3, 'maker': 0})
        arri = self.weave({'device': 1, 'filmCamera': 17, 'maker': 0})   # Arriflex 435, pin registered, Super 35
        self.assertGreater(self.rms([r[0] for r in s8]), 20 * self.rms([r[0] for r in arri]))
        off = self.weave({'device': 1, 'filmCamera': 3, 'maker': 0, 'camWeave': 0})
        self.assertTrue(all(r[0] == 0 and r[1] == 0 for r in off))

    def test_projector_weave_repeats_the_sprocket_every_four_frames(self):
        """35 mm: the intermittent sprocket turns a quarter per frame, a worn tooth jumps again 4 frames later; a
        Super 8 projector's claw has no such period and moves more."""
        sala = self.weave({'process': NEGATIVE, 'medium': 1})
        y = [r[5] for r in sala]
        self.assertGreater(self.corr(y, 4), self.corr(y, 1) + 0.3)
        s8 = self.weave({'process': NEGATIVE, 'medium': 8})
        self.assertGreater(self.rms([r[5] for r in s8]), 3 * self.rms(y))
        self.assertLess(abs(self.corr([r[5] for r in s8], 4)), 0.4)
        off = self.weave({'process': NEGATIVE, 'medium': 8, 'projWeave': 0})
        self.assertTrue(all(r[4] == 0 and r[5] == 0 for r in off))
        none = self.weave({'process': NEGATIVE, 'medium': 0})
        self.assertTrue(all(r[4] == 0 and r[5] == 0 for r in none))

    def test_each_printing_step_adds_its_drift(self):
        one = self.weave({'process': NEGATIVE, 'generation': 0})
        three = self.weave({'process': NEGATIVE, 'generation': 1})
        scan = self.weave({'process': NEGATIVE, 'print': 0})
        r1, r3 = self.rms([r[2] for r in one]), self.rms([r[2] for r in three])
        self.assertGreater(r1, 0)
        self.assertGreater(r3, 1.5 * r1)
        self.assertTrue(all(r[2] == 0 and r[3] == 0 for r in scan))   # a scanned negative was never printed

    def test_gate_hair_comes_in_from_an_edge_and_stays_for_the_shot(self):
        """A hair caught in the gate: a dark, sharp shape touching an edge of the frame, in the same place for the
        whole shot, bigger on Super 8 than on Super 35 (it is 60-90 um everywhere); none at 0."""
        w, h = 320, 180
        flat = [[dm.encode1(0.18, GAMMA)] * 3 for _ in range(w * h)]
        render = lambda ctl, frame: self.render(ctl, [flat], frame=frame, size=(w, h))
        def dark(out):
            m = sorted(sum(px) / 3 for px in out)[len(out) // 2]
            return [(i % w, i // w) for i, px in enumerate(out) if sum(px) / 3 < 0.5 * m]
        for fmt in (0, 2):
            with self.subTest(format=fmt):
                a = dark(render({'process': NEGATIVE, 'hair': 1, 'grain': 0, 'format': fmt}, 5))
                b = dark(render({'process': NEGATIVE, 'hair': 1, 'grain': 0, 'format': fmt}, 40))
                self.assertTrue(a)
                xs, ys = [x for x, _ in a], [y for _, y in a]
                self.assertTrue(min(xs) <= 4 or min(ys) <= 4 or max(xs) >= w - 5 or max(ys) >= h - 5)
                self.assertGreater(len(set(a) & set(b)), len(a) // 2)   # same shot: it barely moves
        s8 = dark(render({'process': NEGATIVE, 'hair': 1, 'grain': 0, 'format': 0}, 5))
        s35 = dark(render({'process': NEGATIVE, 'hair': 1, 'grain': 0, 'format': 2}, 5))
        self.assertGreater(len(s8), 2 * len(s35))
        self.assertFalse(dark(render({'process': NEGATIVE, 'hair': 0, 'grain': 0}, 5)))

    # ---- presets: device, model, format, manufacturer, stock; what the film applies

    FILM = {'device': 1, 'maker': 0, 'filmCamera': 17}   # Arriflex 435, Kodak

    def grain(self, controls):
        """Spread of the grey steps (rows 16-23, centre columns), averaged over steps 3-8 and channels."""
        out = self.render(controls, [ramp()])
        total = 0.0
        for b in range(3, 9):
            for c in range(3):
                v = [out[y * W + x][c] for y in range(16, 24) for x in range(b * 8 + 1, b * 8 + 7)]
                m = sum(v) / len(v)
                total += math.sqrt(sum((a - m) ** 2 for a in v) / len(v))
        return total / 18

    def test_film_with_everything_off_is_the_input(self):
        src = ramp(tinted=True)
        out = self.render(dict(self.FILM, useTone=0, useColour=0, useTexture=0), [src])
        self.assertLess(max(abs(a - b) for p, q in zip(out, src) for a, b in zip(p, q)), 2e-4)

    def test_only_the_grain_keeps_the_picture_and_adds_the_stock_grain(self):
        """Tone and colour off: the steps keep their code values on average, the grain grows with the stock (50D <
        500T < 5247 of 1950) and with a smaller format (Super 8 over Super 35)."""
        only = dict(self.FILM, useTone=0, useColour=0, deviceLoaded=1017)
        src = self.steps([tuple(px) for px in ramp()])
        got = self.steps(self.render(dict(only, stockKodak=0), [ramp()]))
        for a, b in zip(src[2:-2], got[2:-2]):
            for c in range(3):
                self.assertAlmostEqual(a[c], b[c], delta=0.01)
        fine, mid, coarse = (self.grain(dict(only, stockKodak=k)) for k in (3, 0, 15))
        self.assertGreater(fine, 0.001)
        self.assertLess(fine, mid)
        self.assertLess(mid, coarse)
        self.assertGreater(self.grain(dict(only, stockKodak=0, format=0)), 1.5 * mid)

    def test_a_stock_sets_its_process(self):
        """Double-X is black and white; without its colour it keeps the picture's colour; Kodachrome differs from
        Vision3 (a reversal: its own tone scale)."""
        def chroma(out):
            return max(max(s) - min(s) for s in self.steps(out)[3:9])
        bw = self.render(dict(self.FILM, stockKodak=38, useTexture=0), [ramp(tinted=True)])
        self.assertLess(chroma(bw), 0.01)
        kept = self.render(dict(self.FILM, stockKodak=38, useTexture=0, useColour=0), [ramp(tinted=True)])
        self.assertGreater(chroma(kept), 0.05)
        vision = self.steps(self.render(dict(self.FILM, stockKodak=0, useTexture=0), [ramp()]))
        kodachrome = self.steps(self.render(dict(self.FILM, stockKodak=17, useTexture=0), [ramp()]))
        self.assertGreater(max(abs(sum(a) - sum(b)) for a, b in zip(vision, kodachrome)), 0.1)

    def test_a_device_is_the_process_it_names(self):
        """A model picks the engine's row: the Panasonic VHS camera renders as the VHS SP tape with that camera's
        faults and sensor. A digital model adds its own pipeline (sensor size, codec) to its row: not the row alone."""
        def same(a, b):
            self.assertLess(max(abs(u - v) for p, q in zip(a, b) for u, v in zip(p, q)), 1e-5)
        phone = self.render({'device': 4, 'phoneCamera': 0}, [ramp(tinted=True)])
        row = self.render({'process': DIGITAL, 'digital': 6}, [ramp(tinted=True)])
        self.assertGreater(max(abs(u - v) for p, q in zip(phone, row) for u, v in zip(p, q)), 0.01)
        same(self.render({'device': 2, 'videoCamera': 5}, [ramp(tinted=True)]),
             self.render({'process': ANALOG, 'analog': 1, 'wear': 0.2, 'heads': 0.35, 'instability': 0.3, 'sensor': 7},
                         [ramp(tinted=True)]))


    # ---- analog tape, emulated line by line

    def tape(self, rows, controls, size, before=()):
        """Render picture rows (top first, Gamma 2.4 Rec.709 code values) through a tape; rows of output, top first.
        before: the pictures 1, 2, 3 frames earlier (a pickup tube remembers them)."""
        w, h = size
        flip = lambda r: [r[h - 1 - y][x] for y in range(h) for x in range(w)]   # bottom-up  # noqa: E731
        frames = [flip(rows)] + [flip(b) for b in before]
        out = self.render(dict({'process': ANALOG}, **controls), frames, size=size, enc=(1, 3))
        return [out[(h - 1 - y) * w: (h - y) * w] for y in range(h)]

    def test_tape_edges_ring_after_and_big_edges_soften(self):
        """Causal filters: the left of a step stays flat, the tail comes after it. The FM clip: a full black-to-white
        step rises more slowly than a small one of the same shape."""
        w, h = 720, 4
        def step(lo, hi):
            return [[(lo,) * 3 if x < 360 else (hi,) * 3 for x in range(w)] for _ in range(h)]
        quiet = {'analog': 1, 'noise': 0, 'instability': 0, 'heads': 0, 'wear': 0}
        def rise(lo, hi):   # samples from 10 % to 90 % of the step, and the flatness before it
            row = [px[0] for px in self.tape(step(lo, hi), quiet, (w, h))[2]]
            a, b = sum(row[300:340]) / 40, sum(row[600:700]) / 100
            ys = [(v - a) / (b - a) for v in row[340:500]]
            before = ys[:18]
            self.assertLess(max(before) - min(before), 0.02)
            return next(i for i, v in enumerate(ys) if v > 0.9) - next(i for i, v in enumerate(ys) if v > 0.1)
        self.assertGreater(rise(0.0, 1.0), rise(0.45, 0.55))

    def test_tape_noise_is_a_horizontal_grain_per_line(self):
        """FM noise through the playback low-pass: correlated along the line, independent from line to line (each
        frame line is its own field line)."""
        w, h = 720, 576
        grey = [[(0.5, 0.5, 0.5)] * w for _ in range(h)]
        out = self.tape(grey, {'analog': 1, 'instability': 0, 'heads': 0, 'wear': 0}, (w, h))
        rows = [[px[1] for px in row[100:600]] for row in out[40:240]]
        mean = sum(map(sum, rows)) / (200 * 500)
        v = [[a - mean for a in r] for r in rows]
        def corr(pairs):
            num = sum(a * b for a, b in pairs)
            return num / math.sqrt(sum(a * a for a, _ in pairs) * sum(b * b for _, b in pairs))
        horiz = corr([(r[i], r[i + 1]) for r in v for i in range(len(r) - 1)])
        vert = corr([(v[j][i], v[j + 1][i]) for j in range(len(v) - 1) for i in range(len(v[0]))])
        self.assertGreater(horiz, 0.5)
        self.assertLess(abs(vert), 0.15)

    def test_tape_colour_arrives_late_and_soft(self):
        """Colour-under: the colour edge is wider than the luma's and centred to the right of it."""
        w, h = 720, 4
        rows = [[(0.5, 0.5, 0.5) if x < 360 else (0.7, 0.45, 0.3) for x in range(w)] for _ in range(h)]
        quiet = {'analog': 1, 'noise': 0, 'instability': 0, 'heads': 0, 'wear': 0}
        row = self.tape(rows, quiet, (w, h))[2]
        red = [px[0] - px[2] for px in row]
        lo, hi = sum(red[250:340]) / 90, sum(red[600:700]) / 100
        centre = next(i for i in range(340, 700) if (red[i] - lo) / (hi - lo) > 0.5)
        self.assertGreater(centre, 363)

    # ---- the video camera's sensor

    QUIET = {'analog': 1, 'noise': 0, 'instability': 0, 'heads': 0, 'wear': 0}

    @staticmethod
    def mean(rows, x0, x1, y0, y1, c=1):
        return sum(r[x][c] for r in rows[y0:y1] for x in range(x0, x1)) / ((x1 - x0) * (y1 - y0))

    def test_a_tube_remembers_the_frames_before_in_the_shadows(self):
        """Lag: dim light after a bright frame reads brighter than the same light after darkness; the tube's own
        lag (1) more than none (0)."""
        w, h = 160, 32
        dim = [[(0.12,) * 3] * w for _ in range(h)]
        lit = [[(1.0,) * 3] * w for _ in range(h)]
        tube = dict(self.QUIET, sensor=5)
        after_dark = self.mean(self.tape(dim, tube, (w, h), [dim, dim, dim]), 40, 120, 8, 24)
        after_lit = self.mean(self.tape(dim, tube, (w, h), [lit, lit, lit]), 40, 120, 8, 24)
        no_lag = self.mean(self.tape(dim, dict(tube, lag=0, comet=0), (w, h), [lit, lit, lit]), 40, 120, 8, 24)
        self.assertGreater(after_lit, after_dark + 0.03)
        self.assertLess(abs(no_lag - after_dark), 0.01)

    def test_a_tube_leaves_a_comet_tail_behind_a_bright_light(self):
        """A light far above the beam's capacity, gone in this frame, still reads saturated (comet tail); with no
        comet the place is dark again."""
        w, h = 160, 32
        dark = [[(0.1,) * 3] * w for _ in range(h)]
        spot = [[(2.6,) * 3 if 60 <= x < 100 and 8 <= y < 24 else (0.1,) * 3 for x in range(w)] for y in range(h)]
        tube = dict(self.QUIET, sensor=1, lag=0)
        tail = self.mean(self.tape(dark, tube, (w, h), [spot, dark, dark]), 70, 90, 12, 20)
        gone = self.mean(self.tape(dark, dict(tube, comet=0), (w, h), [spot, dark, dark]), 70, 90, 12, 20)
        self.assertGreater(tail, 0.6)
        self.assertLess(gone, 0.3)

    def test_a_ccd_smears_a_very_bright_light_down_its_column(self):
        """Smear: the light above the full well spreads over the whole column, far above and below the light."""
        w, h = 160, 96
        rows = [[(4.0,) * 3 if 76 <= x < 84 and 44 <= y < 52 else (0.3,) * 3 for x in range(w)] for y in range(h)]
        ccd = dict(self.QUIET, sensor=8)
        out = self.tape(rows, ccd, (w, h))
        column, beside = self.mean(out, 78, 82, 4, 30), self.mean(out, 30, 60, 4, 30)
        self.assertGreater(column, beside + 0.05)
        out = self.tape(rows, dict(ccd, smear=0), (w, h))
        self.assertLess(abs(self.mean(out, 78, 82, 4, 30) - self.mean(out, 30, 60, 4, 30)), 0.01)

    def test_a_striped_tube_turns_fine_detail_into_colour(self):
        """False colour: grey stripes near the stripe filter's carrier come out coloured; plain grey does not."""
        w, h = 720, 8
        lines = [[(0.75,) * 3 if (x // 2) % 2 else (0.25,) * 3 for x in range(w)] for _ in range(h)]
        def chroma(sensor):
            row = self.tape(lines, dict(self.QUIET, sensor=sensor, lag=0, comet=0), (w, h))[4]
            return sum(abs(px[0] - px[2]) + abs(px[1] - 0.5 * (px[0] + px[2])) for px in row[100:600]) / 500
        self.assertGreater(chroma(5), chroma(0) + 0.02)

    # ---- digital devices, emulated

    def device(self, rows, controls, size):
        """Rows (top first, Gamma 2.4 Rec.709) through a digital device; rows out, top first."""
        w, h = size
        px = [rows[h - 1 - y][x] for y in range(h) for x in range(w)]
        out = self.render(dict({'noise': 0}, **controls), [px], size=size, enc=(1, 3))
        return [out[(h - 1 - y) * w: (h - y) * w] for y in range(h)]

    def test_a_codec_leaves_its_8x8_blocks(self):
        """The DCT's quantiser: on a smooth picture the steps between neighbours are larger across the 8x8 block
        edges than inside the blocks, and grow with the compression."""
        w, h = 256, 64
        rows = [[(0.3 + 0.4 * x / w + 0.03 * math.sin(y * 0.2),) * 3 for x in range(w)] for y in range(h)]
        def jumps(comp):   # the GoPro: 1080 lines, so no scaling here and the blocks sit on the pixels
            out = self.device(rows, {'device': 4, 'phoneCamera': 3, 'compression': comp, 'sharpness': -1}, (w, h))
            edge = [abs(r[x + 1][1] - r[x][1]) for r in out for x in range(7, w - 1, 8)]
            inner = [abs(r[x + 1][1] - r[x][1]) for r in out for x in range(w - 1) if x % 8 not in (7, 0, 6)]
            return sum(edge) / len(edge), sum(inner) / len(inner)
        e0, i0 = jumps(0)
        e2, i2 = jumps(2)
        self.assertGreater(e2, 1.3 * i2)
        self.assertGreater(e2 - i2, e0 - i0 + 0.002)

    def test_a_tiny_phone_sensor_loses_the_fine_detail(self):
        """A 176x144 phone keeps almost nothing of 1-pixel stripes; a camera at the picture's own size keeps them."""
        w, h = 640, 360
        rows = [[(0.8,) * 3 if x % 2 else (0.2,) * 3 for x in range(w)] for _ in range(h)]
        spread = lambda out: max(px[1] for px in out[180][200:400]) - min(px[1] for px in out[180][200:400])  # noqa
        razr = self.device(rows, {'device': 4, 'phoneCamera': 5, 'compression': 0, 'sharpness': -1}, (w, h))
        alexa = self.device(rows, {'device': 3, 'cinemaCamera': 0}, (w, h))
        self.assertLess(spread(razr), 0.25)
        self.assertGreater(spread(alexa), 0.4)

    def test_a_copy_to_tape_takes_the_tape_s_softness_and_noise(self):
        """A telecine to VHS: the colour picture passes through the tape, so fine stripes soften along the line and
        a flat grey gets the tape's noise; with no copy the same picture is the original."""
        w, h = 640, 480
        rows = [[(0.8,) * 3 if (x // 2) % 2 else (0.2,) * 3 for x in range(w // 2)] + [(0.5,) * 3] * (w // 2)
                for _ in range(h)]
        ctl = {'process': NEGATIVE, 'grain': 0, 'noise': 0}
        orig = self.device(rows, ctl, (w, h))
        vhs = self.device(rows, dict(ctl, transfer=1), (w, h))
        spread = lambda out: sum(abs(r[x + 2][1] - r[x][1]) for r in out[100:380] for x in range(60, 260)) / 56000  # noqa
        flat = lambda out: [px[1] for r in out[100:380] for px in r[400:600]]  # noqa
        sd = lambda v: math.sqrt(sum((a - sum(v) / len(v)) ** 2 for a in v) / len(v))  # noqa
        self.assertLess(spread(vhs), 0.6 * spread(orig))
        self.assertGreater(sd(flat(vhs)), sd(flat(orig)) + 0.003)

    def test_a_kinescope_is_black_and_white_with_grain_on_top(self):
        """A kinescope of a colour tape: no colour left, and the film's grain adds to the picture's."""
        w, h = 160, 64
        rows = [[(0.7, 0.3, 0.2)] * (w // 2) + [(0.2, 0.4, 0.7)] * (w // 2) for _ in range(h)]
        ctl = {'process': ANALOG, 'analog': 1, 'noise': 0, 'wear': 0, 'heads': 0}
        orig = self.tape(rows, ctl, (w, h))
        kine = self.tape(rows, dict(ctl, transfer=4), (w, h))
        chroma = lambda out: max(max(px) - min(px) for r in out[10:50] for px in r[10:70])  # noqa
        self.assertGreater(chroma(orig), 0.1)
        self.assertLess(chroma(kine), 0.02)
        flat = [px[1] for r in kine[10:50] for px in r[100:150]]
        m = sum(flat) / len(flat)
        self.assertGreater(math.sqrt(sum((a - m) ** 2 for a in flat) / len(flat)), 0.005)

    def test_log_output_ignores_the_copy(self):
        ctl = {'process': NEGATIVE, 'output': 1, 'print': 0}
        self.assertEqual(self.render(ctl, [ramp()]), self.render(dict(ctl, transfer=4), [ramp()]))

    def lens(self, scene, controls, size):
        """Scene-linear rows (top first, S-Log3) through a colour negative with no grain or halation, and a lens."""
        w, h = size
        px = [[dm.encode1(v, GAMMA) for v in scene[h - 1 - y][x]] for y in range(h) for x in range(w)]
        ctl = dict({'process': NEGATIVE, 'grain': 0, 'halation': 0, 'printGrain': 0}, **controls)
        out = self.render(ctl, [px], size=size)
        return [out[(h - 1 - y) * w: (h - y) * w] for y in range(h)]

    def test_barrel_distortion_bows_straight_lines_out(self):
        """A vertical line near the left edge stays at the corners and moves outwards at mid-height."""
        w, h = 160, 90
        scene = [[(0.02,) * 3 if x == 24 else (0.18,) * 3 for x in range(w)] for _ in range(h)]
        def where(out, y):
            row = [px[1] for px in out[y]]
            return min(range(w), key=lambda x: row[x])
        flat = self.lens(scene, {}, (w, h))
        bent = self.lens(scene, {'distortion': 6}, (w, h))
        self.assertEqual(where(flat, 45), where(flat, 1))
        self.assertLess(where(bent, 45), where(bent, 1))

    def test_soft_edges_and_swirl_blur_the_corners_not_the_centre(self):
        w, h = 160, 90
        scene = [[(0.6,) * 3 if (x + y) % 2 else (0.05,) * 3 for x in range(w)] for y in range(h)]
        spread = lambda out, x0, y0: max(px[1] for r in out[y0:y0 + 6] for px in r[x0:x0 + 6]) - min(  # noqa
            px[1] for r in out[y0:y0 + 6] for px in r[x0:x0 + 6])
        flat = self.lens(scene, {}, (w, h))
        soft = self.lens(scene, {'edgeSoft': 1, 'swirl': 1}, (w, h))
        self.assertAlmostEqual(spread(soft, 77, 42), spread(flat, 77, 42), delta=0.02)
        self.assertLess(spread(soft, 2, 2), 0.5 * spread(flat, 2, 2))

    def test_an_anamorphic_streak_runs_along_the_line(self):
        """A bright point on black: the streak lights the line far to both sides, not the rows above."""
        w, h = 200, 60
        scene = [[(0.002,) * 3] * w for _ in range(h)]
        for y in range(29, 31):
            for x in range(99, 101):
                scene[y][x] = (30.0,) * 3
        flat = self.lens(scene, {}, (w, h))
        lit = self.lens(scene, {'streak': 0.3}, (w, h))
        self.assertGreater(lit[30][40][2] - flat[30][40][2], 0.02)
        self.assertGreater(lit[30][160][2] - flat[30][160][2], 0.02)
        self.assertLess(abs(lit[10][100][2] - flat[10][100][2]), 0.005)
        self.assertGreater(lit[30][40][2], lit[30][40][0])   # the Panavision's blue, with no lens chosen

    def test_veiling_glare_lifts_the_blacks_near_the_light(self):
        w, h = 160, 90
        scene = [[(4.0,) * 3 if x > 110 else (0.003,) * 3 for x in range(w)] for _ in range(h)]
        flat = self.lens(scene, {}, (w, h))
        veil = self.lens(scene, {'glare': 5}, (w, h))
        self.assertGreater(veil[45][100][1] - flat[45][100][1], 0.02)
        self.assertGreater(veil[45][100][1] - flat[45][100][1], veil[45][5][1] - flat[45][5][1])

    def test_a_lens_loads_its_rows_and_brings_its_glass(self):
        """A lens chosen with no values loaded behaves as its preset written in the rows; the Super-Takumar's
        yellowed glass takes blue away."""
        w, h = 64, 36
        scene = [[(0.18,) * 3] * w for _ in range(h)]
        rows = {'lens': 12, 'lensLoaded': 12, 'distortion': 0.5, 'edgeSoft': 0.1, 'swirl': 0.1, 'glare': 2,
                'streak': 0, 'vignette': 1, 'aberration': 0.25, 'bloom': 0.15}
        self.assertEqual(self.lens(scene, {'lens': 12}, (w, h)), self.lens(scene, rows, (w, h)))
        none = self.lens(scene, {}, (w, h))[18][32]
        tak = self.lens(scene, {'lens': 12, 'lensLoaded': 12}, (w, h))[18][32]
        self.assertLess(tak[2] / tak[1], none[2] / none[1] - 0.02)

    def test_new_screens_show_their_own_faults(self):
        """Trinitron: two dark damper wires across a white field; plasma: a speckle in the shadows that changes from
        frame to frame; early LCD: a bluish black and bands on a smooth ramp."""
        w, h = 160, 120
        white = [(0.8, 0.8, 0.8)] * (w * h)
        tri = self.render({'process': DIGITAL, 'digital': 0, 'noise': 0, 'compression': 0, 'medium': 9}, [white],
                          size=(w, h), enc=(1, 3))
        row = lambda y: sum(px[1] for px in tri[y * w + 20: y * w + 140]) / 120  # noqa: E731
        self.assertLess(row(h // 3), 0.9 * row(h // 3 + 8))   # rows are bottom-up: the wire at a third either way
        dark = [(0.02, 0.02, 0.02)] * (w * h)
        ctl = {'process': DIGITAL, 'digital': 0, 'noise': 0, 'compression': 0, 'medium': 10}
        a = self.render(ctl, [dark], frame=3, size=(w, h), enc=(1, 3))
        b = self.render(ctl, [dark], frame=4, size=(w, h), enc=(1, 3))
        self.assertGreater(sum(abs(p[1] - q[1]) for p, q in zip(a, b)) / len(a), 0.002)
        black = [(0.0, 0.0, 0.0)] * (w * h)
        lcd = self.render(dict(ctl, medium=11), [black], size=(w, h), enc=(1, 3))[60 * w + 80]
        self.assertGreater(lcd[2], lcd[0] + 0.01)
        ramp_px = [(0.2 + 0.3 * x / w,) * 3 for y in range(h) for x in range(w)]
        lcd = self.render(dict(ctl, medium=11), [ramp_px], size=(w, h), enc=(1, 3))
        line = [lcd[60 * w + x][1] for x in range(20, 140)]
        steps = [abs(b - a) for a, b in zip(line, line[1:])]
        self.assertGreater(max(steps), 3 * sorted(steps)[len(steps) // 2] + 1e-4)   # flat runs, then a jump

if __name__ == '__main__':
    unittest.main()

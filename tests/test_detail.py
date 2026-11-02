# SPDX-License-Identifier: GPL-3.0-or-later
"""The Detail node: its reference model (tests/model/detail.py) and the CPU pipeline against it.

Small images stand in for full frames: `href` makes the radii those of a 1080-line frame and a
small `base` forces a reduced working grid, so every path of the plugin is exercised quickly."""
import math
import os
import random
import struct
import subprocess
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
import develop_model as dm  # noqa: E402
from model import detail as D  # noqa: E402
from model import tone as T  # noqa: E402
from plugin_build import test_bin  # noqa: E402

SPACE, GAMMA = 8, 9          # S-Gamut3.Cine / S-Log3
W, H, HREF, BASE = 96, 54, 1080, 20


def scene(w=W, h=H, seed=3):
    """Encoded pixels: a sky gradient, a dark wall, a bright window, texture and a coloured patch."""
    rnd = random.Random(seed)
    img = []
    for y in range(h):
        for x in range(w):
            ev = -3.0 + 5.0 * x / w if y < h // 2 else -4.5
            if w * 0.6 < x < w * 0.8 and h * 0.55 < y < h * 0.85:
                ev = 4.0                                        # a window 8.5 stops over the wall
            ev += 0.12 * math.sin(x * 1.7) * math.cos(y * 1.3) + rnd.gauss(0.0, 0.02)
            tint = (1.0, 0.9, 0.75) if x < w // 3 else (0.7, 0.85, 1.0)
            img.append([dm.encode1(0.18 * 2 ** ev * c, GAMMA) for c in tint])
    return img


def ev_of(px):
    lin = [dm.decode1(c, GAMMA) for c in px]
    return math.log2(max(T.t5_norm(dm.mul(dm.MATS[2][1], dm.mul(dm.MATS[SPACE][0], lin))), 1e-12) / 0.18)


def model(img, controls, w=W, h=H, href=HREF, base=BASE):
    p = D.prepare(w, h, controls, space=SPACE, base=base, href=href)
    return D.render(img, w, h, p, GAMMA)


def model_unweighted(img, controls, w=W, h=H):
    """The model with the presence in full everywhere: what Texture and Clarity do before the zones weigh them."""
    p = D.prepare(w, h, controls, space=SPACE, base=BASE, href=HREF)
    p['pzOn'] = False
    return D.render(img, w, h, p, GAMMA)


CASES = [{}, {'localHighlights': -100}, {'localShadows': 100, 'localContrast': -60},
         {'localHighlights': 60, 'localShadows': -40, 'localContrast': 40, 'preserveDetail': 60},
         {'clarity': 80}, {'clarity': -70, 'clarityCenter': 1.5}, {'texture': 90}, {'texture': -90, 'noiseThreshold': 0},
         {'dehaze': 70, 'hazeWarmth': 40, 'hazeLevel': 1.0}, {'dehaze': -50},
         {'localHighlights': -80, 'viewGain': 1}, {'viewBase': 1, 'detailRadius': 8, 'edgeThreshold': 0.25},
         {'texture': 50, 'clarity': 50, 'localShadows': 50, 'dehaze': 30},
         {'localHighlights': 60, 'localContrast': 30}, {'localHighlights': -100, 'localWhite': 4.5, 'texture': 40},
         {'localHighlights': -50, 'noiseThreshold': 0},
         {'localHighlights': -100, 'destination': 2, 'localWhite': 1.5},
         {'devMode': 0, 'adamsShadowL': -4.5, 'adamsHighL': 2.0, 'adamsShadowPlace': 3, 'adamsHighPlace': 9,
          'texture': 30, 'localShadows': 60},
         {'devMode': 0, 'adamsShadowL': -3.0, 'adamsHighL': 4.0, 'adamsShadowPlace': 2.5, 'adamsHighPlace': 7,
          'destination': 4, 'viewZones': 1},
         {'viewZones': 2, 'destination': 1, 'localShadows': 50}, {'viewZones': 1, 'localHighlights': -60},
         {'devMode': 0, 'zonalPhase': 1, 'adamsShadowL': -2.5, 'adamsHighL': 3.0, 'adamsShadowPlace': 3,
          'adamsHighPlace': 8, 'destination': 1, 'texture': 60, 'localShadows': 60},
         {'devMode': 0, 'zonalPhase': 2, 'adamsShadowL': -2.5, 'adamsHighL': 3.0, 'localHighlights': -70,
          'localShadows': 40, 'printEdgeBurn': 1.5, 'printEdgeSize': 40, 'clarity': 50},
         {'devMode': 0, 'zonalPhase': 3, 'texture': 50, 'clarity': 40, 'dehaze': 30, 'localShadows': 80,
          'printEdgeBurn': 2}]


class Model(unittest.TestCase):
    """Invariants of the reference (spec_detail.md L1-L10), on the model alone."""

    def test_l1_at_rest_the_node_is_the_identity(self):
        img = scene()
        out, planes = model(img, {})
        self.assertEqual(out, img)
        self.assertEqual(max(abs(g) for g in planes['G']), 0.0)

    def test_l2_on_a_flat_field_the_base_is_the_pointwise_curve(self):
        """No detail to keep: the local tones reduce to the same curve the develop node applies."""
        for ev in (-8.0, -4.0, -1.0, 0.0, 2.0, 4.5):
            img = [[dm.encode1(0.18 * 2 ** ev, GAMMA)] * 3] * (32 * 18)
            ctrl = {'localHighlights': -70, 'localShadows': 60, 'localContrast': 30}
            p = D.prepare(32, 18, ctrl, space=SPACE, base=BASE, href=HREF)
            _, planes = D.render(img, 32, 18, p, GAMMA)
            L = planes['L'][0]
            want = T.t5_curve(L, p['curve'])[0]
            if want > 0.0:
                want *= T.t5_ramp((L - D.FLOOR_EV) / D.FLOOR_W)
            want += D.shoulder(L, p['hlE'], p['hlStart'])[0]   # no detail: Local Highlights is its curve
            self.assertAlmostEqual(planes['G'][200], want, places=6, msg='ev %g' % ev)

    def test_l3_texture_and_clarity_commute_with_exposure(self):
        """Their gains read differences of L: +1 stop in, +1 stop out, once the anchors that follow the
        absolute level move with it (the noise floor off, the Clarity centre raised by a stop)."""
        img = scene()
        brighter = [[dm.encode1(dm.decode1(c, GAMMA) * 2.0, GAMMA) for c in px] for px in img]
        for ctrl in ({'texture': 70, 'noiseThreshold': 0}, {'clarity': 70, 'noiseThreshold': 0}):
            _, a = model_unweighted(img, dict(ctrl, clarityCenter=0))
            _, b = model_unweighted(brighter, dict(ctrl, clarityCenter=1.0) if 'clarity' in ctrl else ctrl)
            worst = max(abs(x - y) for x, y in zip(a['G'], b['G']))
            self.assertLess(worst, 1e-6, ctrl)

    def test_l10_a_zone_never_moves_back_as_its_slider_goes_on(self):
        img = scene(48, 27)
        prev = None
        for v in range(0, -101, -20):
            _, planes = model(img, {'localHighlights': v}, 48, 27)
            if prev is not None:
                self.assertTrue(all(g <= q + 1e-9 for g, q in zip(planes['G'], prev)), v)
            prev = planes['G']

    def test_l4_the_halo_of_a_strong_recovery_stays_bounded(self):
        """A 3-stop step at full strength: the dark side next to the edge moves by a small share of it."""
        w, h = 96, 54
        img = [[dm.encode1(0.18 * 2 ** (3.0 if x >= w // 2 else 0.0), GAMMA)] * 3 for y in range(h) for x in range(w)]
        out, planes = model(img, {'localHighlights': -100}, w, h)
        row = (h // 2) * w
        far = planes['G'][row + 4]
        near = max(abs(planes['G'][row + x] - far) for x in range(w // 2 - 6, w // 2))
        self.assertLess(near, 0.35 * 3.0 * 0.1 + 1e-9)

    def test_local_highlights_keeps_the_texture_the_main_node_softens(self):
        """On a +4 stop plateau a +-0.1..0.3 stop texture gains contrast locally, while the main node's shoulder,
        pointwise, keeps less than a third of it. That is the difference between the two Highlights."""
        p = D.prepare(64, 64, {'localHighlights': -100})
        main = T.set_tone({'toneHighlights': -100})
        for amp in (0.1, 0.3):
            local = ((4 + amp + D.local_highlights(4 + amp, 4.0, p)) - (4 - amp + D.local_highlights(4 - amp, 4.0, p))) / (2 * amp)
            point = ((4 + amp + T.t5_curve(4 + amp, main)[0]) - (4 - amp + T.t5_curve(4 - amp, main)[0])) / (2 * amp)
            self.assertGreater(local, 1.1, amp)
            self.assertLess(point, 0.3, amp)

    def test_local_highlights_never_inverts_the_detail(self):
        """Over a fixed base, a brighter pixel always comes out brighter, whatever Soglia bordi says."""
        for edge in (0.25, 0.5, 1.0):
            p = D.prepare(64, 64, {'localHighlights': -100, 'edgeThreshold': edge})
            for base in (0.0, 2.0, 4.0, 6.0):
                Ls = [base - 2.0 + 0.002 * i for i in range(2001)]
                o = [L + D.local_highlights(L, base, p) for L in Ls]
                self.assertGreater(min(b - a for a, b in zip(o, o[1:])), 0.0, 'Soglia bordi %g, base %+g' % (edge, base))

    def test_local_highlights_never_brightens_and_follows_its_slider(self):
        for L, B in ((4.3, 4.0), (0.2, -0.3), (1.0, 3.0), (5.0, 2.0)):
            prev = 0.0
            for v in range(-10, -101, -10):
                g = D.local_highlights(L, B, D.prepare(64, 64, {'localHighlights': v}))
                self.assertLessEqual(g, 0.0)
                self.assertLessEqual(g, prev + 1e-12, (L, B, v))
                prev = g

    def test_local_highlights_halos_stay_within_2_0_0(self):
        """A 3-stop step: the band along the bright side is no wider than 2.0.0's, the dark side moves < 0.1 stop."""
        w, h = 96, 54
        img = [[dm.encode1(0.18 * 2 ** (3.0 if x >= w // 2 else 0.0), GAMMA)] * 3 for y in range(h) for x in range(w)]
        _, planes = model(img, {'localHighlights': -100}, w, h)
        row = (h // 2) * w
        G = planes['G'][row:row + w]
        self.assertLess(max(abs(g - G[4]) for g in G[w // 2 - 6:w // 2]), 0.1)
        self.assertLess(max(abs(g - G[w - 5]) for g in G[w // 2:w // 2 + 6]), 0.5)

    def test_local_highlights_takes_a_clipped_plateau_towards_white(self):
        """The sensor clip is cream-coloured: pulled 3.5 stops down it must not become a cream plate. The same path
        to white as the main node lowers its chroma, at its own hue."""
        w, h = 48, 27
        cream = [1.0, 0.93, 0.80]
        img = [[dm.encode1(0.18 * 2 ** 6.0 * c, GAMMA) for c in cream]] * (w * h)
        out, _ = model(img, {'localHighlights': -100}, w, h)
        lin_in = dm.mul(dm.MATS[SPACE][0], [dm.decode1(c, GAMMA) for c in img[300]])
        lin_out = dm.mul(dm.MATS[SPACE][0], [dm.decode1(c, GAMMA) for c in out[300]])
        def uv(xyz):
            d = xyz[0] + 15 * xyz[1] + 3 * xyz[2]
            return 4 * xyz[0] / d - 0.19783, 6 * xyz[1] / d - 0.31221
        (ui, vi), (uo, vo) = uv(lin_in), uv(lin_out)
        self.assertLess(math.hypot(uo, vo), 0.8 * math.hypot(ui, vi))

    def test_n1_non_finite_pixels_are_copied_and_do_not_spread(self):
        img = scene(48, 27)
        img[5 * 48 + 7] = [float('nan'), 0.4, 0.4]
        img[9 * 48 + 30] = [float('inf')] * 3
        out, planes = model(img, {'localShadows': 80, 'texture': 60}, 48, 27)
        self.assertTrue(math.isnan(out[5 * 48 + 7][0]))
        self.assertEqual(out[9 * 48 + 30], img[9 * 48 + 30])
        self.assertTrue(all(math.isfinite(c) for i, px in enumerate(out) if i not in (5 * 48 + 7, 9 * 48 + 30)
                            for c in px))

    def flat_g(self, ev, ctrl):
        img = [[dm.encode1(0.18 * 2 ** ev, GAMMA)] * 3] * (32 * 18)
        _, planes = D.render(img, 32, 18, D.prepare(32, 18, ctrl, space=SPACE, base=BASE, href=HREF), GAMMA)
        return planes['L'][200], planes['G'][200]

    def test_zonale_places_the_shadow_and_develops_the_highlight(self):
        """Zone V is grey: the shadow lands on its zone, the highlight on its own, pixel by pixel on flat fields."""
        Ls, Lh = self.flat_g(-1.3, {})[0], self.flat_g(4.1, {})[0]
        for zs, zh in ((3, 8), (3, 7), (2, 9.5), (4, 9)):
            ctrl = {'devMode': 0, 'adamsShadowL': Ls, 'adamsHighL': Lh, 'adamsShadowPlace': zs, 'adamsHighPlace': zh}
            (l1, g1), (l2, g2) = self.flat_g(-1.3, ctrl), self.flat_g(4.1, ctrl)
            self.assertAlmostEqual(D.zone_of(l1 + g1, 2.5, 4.5), zs, places=6, msg=ctrl)
            self.assertAlmostEqual(D.zone_of(l2 + g2, 2.5, 4.5), zh, places=6, msg=ctrl)

    def test_zones_follow_the_destination(self):
        """Rec.709 with a CST: zone 0 to X span 7 stops; the scene (DaVinci WG, no ODT yet) 18; zone V is grey."""
        for dest, span in ((1, 7.0), (4, 18.0)):
            v = D.controls_clamped({'destination': dest})
            w, b = D.destination_white(v), D.destination_black(v)
            self.assertAlmostEqual(D.zone_L(10, w, b) - D.zone_L(0, w, b), span)
            self.assertEqual(D.zone_L(5, w, b), 0.0)
            for z in (0.5, 3.2, 5.0, 7.7, 9.9):
                self.assertAlmostEqual(D.zone_of(D.zone_L(z, w, b), w, b), z)

    def test_zonale_on_the_scene_destination_moves_less(self):
        """Zone VIII is 3 stops over grey with a Rec.709 CST only when the white is 5 stops: here the CST puts it at
        1.5, the scene at 5.4."""
        v = D.controls_clamped({'destination': 1})
        self.assertAlmostEqual(D.zone_L(8, D.destination_white(v), D.destination_black(v)), 1.5)
        v = D.controls_clamped({'destination': 4})
        self.assertAlmostEqual(D.zone_L(8, D.destination_white(v), D.destination_black(v)), 5.4)

    def test_view_zones_paints_eleven_bands(self):
        img = [[dm.encode1(0.18 * 2 ** (-6 + 0.016 * i), GAMMA)] * 3 for i in range(32 * 18)]
        for grey in (1, 2):
            out, _ = model(img, {'viewZones': grey, 'destination': 1}, 32, 18)
            self.assertIn(len({tuple(px) for px in out}), (11, 12, 13))   # 0 and X are also hatched

    def test_zonale_development_is_monotone_and_smooth(self):
        """One curve, toe and shoulder included: the tones keep their order and the slope has no jump."""
        for zs, zh, dest in ((2, 10, 1), (3, 8, 4), (4, 4.5, 2), (1, 9, 0)):
            v = D.controls_clamped({'devMode': 0, 'adamsShadowL': -2.0, 'adamsHighL': 2.0, 'adamsShadowPlace': zs,
                                    'adamsHighPlace': zh, 'destination': dest})
            zd = D.zonal_develop(v)
            out = [L + D.zonal_gain(L, zd) for L in [-12 + 0.01 * i for i in range(2400)]]
            steps = [b - a for a, b in zip(out, out[1:])]
            self.assertGreater(min(steps), 0.0)
            self.assertLess(max(abs(b - a) for a, b in zip(steps, steps[1:])), 1e-3)

    def test_zonale_development_is_pointwise(self):
        """Development reads no neighbour: on a picture with edges each gain is the curve at its own pixel."""
        ctrl = {'devMode': 0, 'zonalPhase': 1, 'adamsShadowL': -3.0, 'adamsHighL': 3.5, 'adamsShadowPlace': 3,
                'adamsHighPlace': 7, 'destination': 1, 'localShadows': 100, 'texture': 80}
        _, planes = model(scene(), ctrl)
        zd = D.zonal_develop(D.controls_clamped(ctrl))
        for L, G in zip(planes['L'], planes['G']):
            want = D.zonal_gain(L, zd)
            if want > 0.0:
                want *= T.t5_ramp((L - D.FLOOR_EV) / D.FLOOR_W)
            self.assertAlmostEqual(G, want, places=9)

    def test_zonale_phases_split_the_work(self):
        """Print and presence leave the development alone, and each phase ignores the others' controls."""
        img = scene(48, 27)
        meas = {'devMode': 0, 'adamsShadowL': -3.0, 'adamsHighL': 3.5, 'adamsShadowPlace': 3, 'adamsHighPlace': 7}
        rest, _ = model(img, {}, 48, 27)
        self.assertEqual(model(img, dict(meas, zonalPhase=2), 48, 27)[0], rest)
        self.assertEqual(model(img, dict(meas, zonalPhase=3, localShadows=80, printEdgeBurn=2), 48, 27)[0], rest)
        plain = model_unweighted(img, {'texture': 70, 'clarity': 50}, 48, 27)[1]['G']
        pres = model(img, dict(meas, zonalPhase=3, texture=70, clarity=50), 48, 27)[1]
        v = D.controls_clamped(meas)
        white, black = D.destination_white(v), D.destination_black(v)
        for L, a, b in zip(pres['L'], plain, pres['G']):
            if 2.0 <= D.zone_of(L, white, black) <= 8.0:
                self.assertEqual(a, b)       # zones II-VIII: the presence in full
            self.assertLessEqual(abs(b), abs(a) + 1e-12)   # elsewhere it can only fade
        self.assertTrue(any(a != b for a, b in zip(plain, pres['G'])))
        shadows = model(img, {'localShadows': 70}, 48, 27)[0]
        self.assertEqual(model(img, dict(meas, zonalPhase=2, localShadows=70, texture=70), 48, 27)[0], shadows)

    def test_print_burns_the_edges_only(self):
        p = D.prepare(64, 36, {'devMode': 0, 'zonalPhase': 2, 'printEdgeBurn': 1.5, 'printEdgeSize': 30})
        self.assertEqual(D.edge_burn(31, 17, 64, 36, p), 0.0)
        self.assertAlmostEqual(D.edge_burn(0, 17, 64, 36, p), -1.5, delta=0.02)
        row = [D.edge_burn(x, 17, 64, 36, p) for x in range(32)]
        self.assertTrue(all(a <= b for a, b in zip(row, row[1:])))
        self.assertEqual(D.prepare(64, 36, {'zonalPhase': 1, 'printEdgeBurn': 1.5})['ebAmt'], 0.0)   # the print only

    def test_zonale_an_unreachable_contraction_says_where_it_stops(self):
        """Past the flattest slope the highlight stops short of its zone, and the panel says so."""
        v = D.controls_clamped({'devMode': 0, 'adamsShadowL': -1.0, 'adamsHighL': 5.0, 'adamsShadowPlace': 4,
                                'adamsHighPlace': 5, 'destination': 1})
        zs, zh = D.zonal_reached(v)
        self.assertAlmostEqual(zs, 4.0, places=6)
        self.assertGreater(zh, 5.5)

    def test_destination_white_moves_the_highlight_landing(self):
        top = D.prepare(64, 64, {'localHighlights': -100, 'destination': 1, 'localWhite': 8})
        same = D.prepare(64, 64, {'localHighlights': -100, 'localWhite': 2.5})
        self.assertEqual((top['hlE'], top['hlWhiteLin']), (same['hlE'], same['hlWhiteLin']))
        pq = D.prepare(64, 64, {'localHighlights': -100, 'destination': 3})
        self.assertGreater(pq['hlE'], same['hlE'])

    def test_dehaze_leaves_everything_above_the_veil_alone(self):
        img = [[dm.encode1(0.18 * 2 ** ev, GAMMA)] * 3 for ev in [3.0 + 0.01 * i for i in range(32 * 18)]]
        out, _ = model(img, {'dehaze': 100, 'hazeLevel': 2.0}, 32, 18)
        worst = max(abs(ev_of(a) - ev_of(b)) for a, b in zip(img, out))
        self.assertLess(worst, 1e-6)


class Panel(unittest.TestCase):
    def test_the_hidden_values_reach_the_controls(self):
        """Every value the model reads from the picture is bound by the effect (kDetailSliders) or read as a choice."""
        src = open(os.path.join(ROOT, 'ofx', 'SLogMetaRaw', 'src', 'detail', 'DetailEffect.cpp'), encoding='utf-8').read()
        for name in D.DETAIL_CONTROLS:
            if name.startswith('adams'):
                self.assertIn('{ "%s", &SMDetailControls::%s }' % (name, name), src)
        for name in ('m_DevMode->getValueAtTime', 'm_Destination->getValueAtTime'):
            self.assertIn(name, src)


class CpuParity(unittest.TestCase):
    """src/detail/DetailPasses.cpp against the model: planes within 2e-4 EV, pixels within 5e-4."""

    @classmethod
    def setUpClass(cls):
        cls.exe = test_bin('detail_test')
        if not cls.exe:
            raise unittest.SkipTest('binari di test non compilabili qui')

    def run_cpu(self, img, controls, w=W, h=H, href=HREF, base=BASE, threads=1):
        head = '%d %d %r %d %d %d 1.0 %d\n' % (w, h, float(href), base, SPACE, GAMMA, threads)
        ctrl = ' '.join('%s=%r' % (k, float(v)) for k, v in controls.items()) + '\n'
        body = '\n'.join(' '.join(repr(c) for c in px) for px in img) + '\n'
        out = subprocess.run([self.exe], input=head + ctrl + body, capture_output=True, text=True, check=True).stdout
        return [[float(v) for v in line.split()] for line in out.splitlines()]

    def test_every_control_matches_the_model(self):
        img = scene()
        for ctrl in CASES:
            got = self.run_cpu(img, ctrl)
            out, planes = model(img, ctrl)
            self.assertEqual(len(got), len(out))
            for name, col in (('L', 3), ('B', 4), ('Dg', 5), ('G', 6)):
                # L and B are log2 luminance: near black float32 cancellation (Dehaze removing the veil) shows
                # up in EV while the light differs by nothing, so there they are compared like the pixels below
                visible = (lambda a, b: abs(0.18 * (2 ** a - 2 ** b)) > 2e-6) if name in 'LB' else (lambda a, b: True)
                worst = max([abs(g[col] - m) for g, m in zip(got, planes[name]) if visible(g[col], m)] or [0.0])
                self.assertLess(worst, 2e-4, '%s: piano %s scarto %g' % (ctrl, name, worst))
            worst = 0.0
            for g, m in zip(got, out):
                for a, b in zip(g[:3], m):
                    if abs(dm.decode1(a, GAMMA) - dm.decode1(b, GAMMA)) > 2e-6:
                        worst = max(worst, abs(a - b))
            self.assertLess(worst, 5e-4, '%s: uscita scarto %g' % (ctrl, worst))

    def test_at_rest_the_output_is_the_input_bit_for_bit(self):
        img = scene()
        got = self.run_cpu(img, {})
        f32 = lambda v: struct.unpack('f', struct.pack('f', v))[0]   # the plugin works in float32
        for g, px in zip(got, img):
            self.assertEqual([f32(v) for v in g[:3]], [f32(c) for c in px])   # %.9g round-trips a float32

    def test_l6_one_thread_and_many_give_the_same_bits(self):
        img = scene()
        ctrl = {'texture': 50, 'clarity': 50, 'localShadows': 50, 'dehaze': 30}
        self.assertEqual(self.run_cpu(img, ctrl, threads=1), self.run_cpu(img, ctrl, threads=5))

    def test_full_size_grid_and_reduced_grid(self):
        """s = 1 (base large) and s = 4 (base tiny) both follow the model."""
        img = scene(64, 36)
        for base in (4000, 9):
            got = self.run_cpu(img, {'localHighlights': -60, 'clarity': 40}, 64, 36, base=base)
            _, planes = model(img, {'localHighlights': -60, 'clarity': 40}, 64, 36, base=base)
            worst = max(abs(g[6] - m) for g, m in zip(got, planes['G']))
            self.assertLess(worst, 2e-4, 'base %d' % base)


if __name__ == '__main__':
    unittest.main()

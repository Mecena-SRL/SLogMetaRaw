# SPDX-License-Identifier: GPL-3.0-or-later
"""Render the figures of the illustrated user guides (packaging/guide/img/*.svg), in Italian and English.

Usage: python3 tools/make_guide_figures.py [output dir]
       (default: packaging/guide/img)

No dependencies: the SVG is written by hand. Every curve that describes the plugin is computed by the repository's
own reference models, so a figure cannot drift from what the node does:

  log_curves     tests/develop_model.py  decode1/encode1 (math/Transfer.h); also data_levels, curve_tops
  wb_locus       tests/develop_model.py  locus_uv, white_xyz, Bradford (math/Develop.h)
  false_color    tests/develop_model.py  false_color, through tools/make_falsecolor_chart.py
  tone_curves    tests/model/tone.py     set_tone, tone5
  drt_curves     tests/model/display.py  render (ACES 2.0, Luminanza, Filmico, ACES 1.3, OpenDRT)
  gamut_rgc      tests/model/display.py  compress (ACES 1.3 Reference Gamut Compression)
  dlogm          tests/develop_model.py  DLM (17-node D-Log M table, Osmo Action 6)

One figure is an illustration, and says so in its caption: local_vs_point (a 1-D signal, not a node's filter).
The block diagrams (flow, develop_chain) are drawn by hand from the order of sm_develop.
"""
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import develop_model as dm  # noqa: E402
from model import tone as T  # noqa: E402
from model import display as D  # noqa: E402
import make_falsecolor_chart as FC  # noqa: E402

FULL, HALF = 736, 356          # px; the guide prints 1 px = 0.25 mm (184 mm and 89 mm columns)
FONT = "Helvetica Neue, Helvetica, Arial, sans-serif"
INK, MUTED, GRID, SOFT = '#1c1f26', '#6b7280', '#e3e5ea', '#f6f7f9'
WARM, COOL = '#e8893a', '#3d7bff'
PAL = ['#1c1f26', '#e8893a', '#3d7bff', '#2e9e5b', '#b04fc4', '#d64545', '#8a8f99', '#c9a227']

# ------------------------------------------------------------------ text, in two languages
TXT = {
    'stops_grey': ('stop dal grigio 18%', 'stops from 18% grey'),
    'code': ('code value (0-1)', 'code value (0-1)'),
    'grey': ('grigio 18%', '18% grey'),
    'scene_in': ('scena in ingresso (stop)', 'scene in (stops)'),
    'out_stops': ('uscita (stop)', 'out (stops)'),
    'none': ('nessun tono', 'no tone'),
    'hl100': ('Highlights −100', 'Highlights −100'),
    'hl100sc': ('Highlights −100 + Soft Clip', 'Highlights −100 + Soft Clip'),
    'sh': ('Shadows +100, Blacks −67', 'Shadows +100, Blacks −67'),
    'con': ('Contrast +50', 'Contrast +50'),
    'hlp': ('Highlights +50', 'Highlights +50'),
    'bianco': ('Bianco 2,5 stop', 'Bianco 2.5 stops'),
    'rec709code': ('Rec.709 · Gamma 2.4, code value', 'Rec.709 · Gamma 2.4, code value'),
    'cst': ('Nessuno (CST): taglia a +2,47', 'Nessuno (CST): clips at +2.47'),
    'aces2': ('ACES 2.0 · Luminanza · Filmico (sul neutro)', 'ACES 2.0 · Luminanza · Filmico (on neutrals)'),
    'aces13': ('ACES 1.3', 'ACES 1.3'),
    'opendrt': ('OpenDRT v1.1.0', 'OpenDRT v1.1.0'),
    'dist_in': ('distanza dall\'acromatico, ingresso', 'distance from achromatic, in'),
    'dist_out': ('uscita', 'out'),
    'gamut_edge': ('bordo del gamut', 'gamut boundary'),
    'cyan': ('ciano (0,815 → 1,147)', 'cyan (0.815 → 1.147)'),
    'magenta': ('magenta (0,803 → 1,264)', 'magenta (0.803 → 1.264)'),
    'yellow': ('giallo (0,880 → 1,312)', 'yellow (0.880 → 1.312)'),
    'identity': ('nessuna compressione', 'no compression'),
    'zone_dest': ('Destinazione (ODT)', 'Destinazione (ODT)'),
    'pointwise': ('puntuale (nodo principale)', 'pointwise (main node)'),
    'local': ('per aree (base + dettaglio)', 'by areas (base + detail)'),
    'input': ('ingresso', 'input'),
    'position': ('posizione nell\'immagine', 'position in the image'),
    'luma_stops': ('luminanza (stop)', 'luminance (stops)'),
    'nodes': ('17 nodi misurati (Osmo Action 6)', '17 measured nodes (Osmo Action 6)'),
    'top': ('cima: +4,1 stop', 'top: +4.1 stops'),
    'warm_l': ('più caldo', 'warmer'),
    'cool_l': ('più freddo', 'cooler'),
    'green': ('bianco con Tint +', 'white with Tint +'),
    'magenta_t': ('bianco con Tint −', 'white with Tint −'),
    'locus': ('luogo di Planck', 'Planckian locus'),
    'gains': ('guadagni per cono, ripresa 5600 K', 'cone gains, shot at 5600 K'),
    'to3200': ('Temperatura 3200', 'Temperatura 3200'),
    'to8000': ('Temperatura 8000', 'Temperatura 8000'),
    'exposure_fc': ('Esposizione', 'Exposure'),
    'temp_fc': ('Temperatura: luce da −1200 K a +1200 K', 'Temperature: light from −1200 K to +1200 K'),
    'tint_fc': ('Tint: da −35 a +35', 'Tint: from −35 to +35'),
    'clip': ('clip', 'clip'),
    'near_clip': ('vicino al clip', 'near clip'),
    'skin': ('incarnato', 'skin'),
    'deep': ('ombra profonda', 'deep shadow'),
    'black': ('nero', 'black'),
    'full_ok': ('S-Log3 su scala full (giusto)', 'S-Log3 on the full scale (right)'),
    'as_video': ('la stessa letta come Video', 'the same read as Video'),
    'video_range': ('scala video 64-940', 'video scale 64-940'),
    'cv_axis': ('code value a 10 bit', '10-bit code value'),
    'neg': ('nero sotto il nero: lineare negativo', 'black below black: negative linear'),
    'top_axis': ('cima della curva: stop sopra il grigio 18%', 'top of the curve: stops above 18% grey'),
    'red_band': ('rosso da +5,5', 'red from +5.5'),
    'yellow_band': ('giallo da +4', 'yellow from +4'),
}

LANG = 0


def t(key):
    return TXT[key][LANG]


def esc(s):
    return str(s).replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def num(v, d=1):
    s = ('%.' + str(d) + 'f') % v
    if s.startswith('-'):
        s = '−' + s[1:]
    return s.replace('.', ',') if LANG == 0 else s


def hexc(rgb):
    return '#%02x%02x%02x' % tuple(int(max(0.0, min(1.0, c)) * 255 + 0.5) for c in rgb)


class Svg:
    def __init__(self, w, h):
        self.w, self.h, self.items = w, h, []

    def add(self, s):
        self.items.append(s)

    def text(self, x, y, s, size=11, anchor='start', color=INK, weight='normal', rotate=None, italic=False):
        tr = ' transform="rotate(%g %g %g)"' % (rotate, x, y) if rotate is not None else ''
        st = ' font-style="italic"' if italic else ''
        self.add('<text x="%.1f" y="%.1f" font-size="%g" text-anchor="%s" fill="%s" font-weight="%s"%s%s>%s</text>'
                 % (x, y, size, anchor, color, weight, tr, st, esc(s)))

    def line(self, x1, y1, x2, y2, color=INK, width=1.0, dash=None):
        d = ' stroke-dasharray="%s"' % dash if dash else ''
        self.add('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s" stroke-width="%g"%s/>'
                 % (x1, y1, x2, y2, color, width, d))

    def rect(self, x, y, w, h, fill, stroke='none', rx=0, sw=1.0, opacity=1.0):
        self.add('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" fill="%s" stroke="%s" stroke-width="%g" '
                 'rx="%g" fill-opacity="%g"/>' % (x, y, w, h, fill, stroke, sw, rx, opacity))

    def circle(self, x, y, r, fill, stroke='none', sw=1.0):
        self.add('<circle cx="%.1f" cy="%.1f" r="%g" fill="%s" stroke="%s" stroke-width="%g"/>'
                 % (x, y, r, fill, stroke, sw))

    def poly(self, pts, color=INK, width=1.6, dash=None, fill='none'):
        d = ' stroke-dasharray="%s"' % dash if dash else ''
        self.add('<polyline points="%s" fill="%s" stroke="%s" stroke-width="%g" stroke-linejoin="round"%s/>'
                 % (' '.join('%.1f,%.1f' % p for p in pts), fill, color, width, d))

    def arrow(self, x1, y1, x2, y2, color=INK, width=1.4):
        self.line(x1, y1, x2, y2, color, width)
        a = math.atan2(y2 - y1, x2 - x1)
        p = [(x2, y2), (x2 - 7 * math.cos(a - 0.4), y2 - 7 * math.sin(a - 0.4)),
             (x2 - 7 * math.cos(a + 0.4), y2 - 7 * math.sin(a + 0.4))]
        self.add('<polygon points="%s" fill="%s"/>' % (' '.join('%.1f,%.1f' % q for q in p), color))

    def save(self, path):
        body = '\n'.join(self.items)
        with open(path, 'w', encoding='utf-8') as fh:
            fh.write('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
                     'font-family="%s">\n<rect width="100%%" height="100%%" fill="#ffffff"/>\n%s\n</svg>\n'
                     % (self.w, self.h, self.w, self.h, FONT, body))


class Plot:
    """Axes inside an Svg: data -> pixels, ticks, grid."""

    def __init__(self, svg, box, xr, yr, xlabel='', ylabel='', xticks=(), yticks=(), xfmt=None, yfmt=None):
        self.s, (self.x0, self.y0, self.x1, self.y1) = svg, box
        self.xr, self.yr = xr, yr
        s = svg
        for v in xticks:
            px = self.px(v)
            s.line(px, self.y0, px, self.y1, GRID, 0.8)
            s.text(px, self.y1 + 13, xfmt(v) if xfmt else num(v, 0), 10, 'middle', MUTED)
        for v in yticks:
            py = self.py(v)
            s.line(self.x0, py, self.x1, py, GRID, 0.8)
            s.text(self.x0 - 5, py + 3.5, yfmt(v) if yfmt else num(v, 1), 10, 'end', MUTED)
        s.line(self.x0, self.y1, self.x1, self.y1, '#9aa0aa', 1)
        s.line(self.x0, self.y0, self.x0, self.y1, '#9aa0aa', 1)
        if xlabel:
            s.text((self.x0 + self.x1) / 2, self.y1 + 27, xlabel, 10.5, 'middle', INK)
        if ylabel:
            s.text(self.x0 - 34, (self.y0 + self.y1) / 2, ylabel, 10.5, 'middle', INK, rotate=-90)

    def px(self, x):
        return self.x0 + (x - self.xr[0]) / (self.xr[1] - self.xr[0]) * (self.x1 - self.x0)

    def py(self, y):
        return self.y1 - (y - self.yr[0]) / (self.yr[1] - self.yr[0]) * (self.y1 - self.y0)

    def curve(self, xs, ys, color, width=1.8, dash=None):
        pts = [(self.px(x), self.py(y)) for x, y in zip(xs, ys)
               if self.yr[0] - 1e-9 <= y <= self.yr[1] + 1e-9 and self.xr[0] <= x <= self.xr[1]]
        self.s.poly(pts, color, width, dash)

    def legend(self, x, y, items, size=10, step=14):
        for i, (label, color, dash) in enumerate(items):
            yy = y + i * step
            self.s.line(x, yy - 3.5, x + 18, yy - 3.5, color, 2.2, dash)
            self.s.text(x + 23, yy, label, size, 'start', INK)


def frange(a, b, n):
    return [a + (b - a) * i / (n - 1) for i in range(n)]


# ------------------------------------------------------------------ figures
CURVES = [(9, 'S-Log3'), (8, 'S-Log2'), (11, 'V-Log'), (21, 'LogC4'), (12, 'Apple Log'), (13, 'D-Log M'),
          (5, 'Rec.709')]


def fig_log_curves(path):
    s = Svg(HALF, 300)
    p = Plot(s, (44, 14, HALF - 10, 248), (-8, 10), (0, 1), t('stops_grey'), t('code'),
             range(-8, 11, 2), [0, 0.2, 0.4, 0.6, 0.8, 1.0])
    xs = frange(-8, 10, 220)
    s.line(p.px(0), p.y0, p.px(0), p.y1, '#c3c7cf', 1, '3,3')
    for i, (g, name) in enumerate(CURVES):
        ys = [dm.encode1(0.18 * 2 ** x, g) for x in xs]
        p.curve(xs, ys, PAL[i])
        gy = dm.encode1(0.18, g)
        s.circle(p.px(0), p.py(gy), 2.6, PAL[i], '#fff', 0.8)
    items = [('%s · %s' % (name, num(dm.encode1(0.18, g), 3)), PAL[i], None) for i, (g, name) in enumerate(CURVES)]
    p.legend(p.px(2.6), p.py(0.42), items, 9.5, 12.5)
    s.text(p.px(0) + 3, p.y0 + 10, t('grey'), 9, 'start', MUTED)
    s.save(path)


def fig_wb_locus(path):
    s = Svg(HALF, 300)
    p = Plot(s, (44, 14, HALF - 10, 176), (0.175, 0.30), (0.27, 0.37), 'u (CIE 1960)', 'v',
             [0.18, 0.20, 0.22, 0.24, 0.26, 0.28, 0.30], [0.28, 0.30, 0.32, 0.34, 0.36],
             xfmt=lambda v: num(v, 2), yfmt=lambda v: num(v, 2))
    ks = frange(1800, 15000, 200)
    us, vs = zip(*[dm.locus_uv(k)[:2] for k in ks])
    p.curve(us, vs, INK, 2.2)
    for k in (2000, 2500, 3200, 4000, 5600, 6500, 8000, 10000, 15000):
        u, v, n = dm.locus_uv(k)
        x, y = p.px(u), p.py(v)
        s.circle(x, y, 2.4, WARM if k < 5600 else (COOL if k > 5600 else INK))
        s.text(x + 3, y + 12, '%d K' % k, 8.5, 'start', MUTED)
    u, v, n = dm.locus_uv(5600)
    d = 0.02
    s.arrow(p.px(u), p.py(v), p.px(u + n[0] * d), p.py(v + n[1] * d), '#2e9e5b')
    s.arrow(p.px(u), p.py(v), p.px(u - n[0] * d), p.py(v - n[1] * d), '#b04fc4')
    s.text(p.px(u + n[0] * d) - 8, p.py(v + n[1] * d) - 6, t('green'), 9, 'start', '#2e9e5b')
    s.text(p.px(u - n[0] * d) + 4, p.py(v - n[1] * d) + 8, t('magenta_t'), 9, 'start', '#b04fc4')
    s.text(p.px(0.288), p.py(0.355), t('locus'), 9.5, 'end', INK)
    # bar chart: Bradford LMS gains, shot 5600 K
    s.text(14, 226, t('gains'), 10, 'start', INK, 'bold')
    ws = dm.mul(dm.BR, dm.white_xyz(5600, 0))
    for row, (k, key) in enumerate(((3200, 'to3200'), (8000, 'to8000'))):
        wt = dm.mul(dm.BR, dm.white_xyz(k, 0))
        r = [ws[i] / wt[i] for i in range(3)]
        y = 236 + row * 28
        s.text(14, y + 11, t(key), 9.5, 'start', INK)
        for i, (lab, col) in enumerate((('L', '#d64545'), ('M', '#2e9e5b'), ('S', COOL))):
            x0 = 112 + i * 80
            w = min(r[i], 2.4) / 2.4 * 34
            s.rect(x0, y + 2, 34, 11, SOFT, GRID)
            s.rect(x0, y + 2, w, 11, col, opacity=0.85)
            s.line(x0 + 34 / 2.4, y, x0 + 34 / 2.4, y + 15, INK, 0.8, '2,2')
            s.text(x0 + 38, y + 11, '%s ×%s' % (lab, num(r[i], 2)), 9, 'start', INK)
    s.save(path)


def fig_false_color(path):
    w, h = FULL, 150
    s = Svg(w, h)
    cal = dm.fc_calibration(FC.K0, FC.TINT0)
    x0, x1 = 120, w - 12
    n = 280
    rows = ((1, t('exposure_fc')), (2, t('temp_fc')), (3, t('tint_fc')))
    for r, (mode, label) in enumerate(rows):
        y = 8 + r * 40
        s.text(x0 - 8, y + 18, label.split(':')[0], 10.5, 'end', INK, 'bold')
        for i in range(n):
            f = (i + 0.5) / n
            if mode == 1:
                xyz = FC.neutral(FC.K0, FC.TINT0, stops=-8.0 + 14.0 * f)
            elif mode == 2:
                xyz = FC.neutral(FC.K0 + (-1200.0 + 2400.0 * f), FC.TINT0)
            else:
                xyz = FC.neutral(FC.K0, FC.TINT0 + (-35.0 + 70.0 * f))
            c = dm.false_color(xyz, mode, *cal)
            s.rect(x0 + (x1 - x0) * i / n, y, (x1 - x0) / n + 0.6, 26, hexc(c))
        if mode == 1:
            for v in range(-8, 7, 1):
                px = x0 + (v + 8) / 14 * (x1 - x0)
                s.line(px, y + 26, px, y + 30, MUTED, 0.8)
                s.text(px, y + 38, num(v, 0), 8.5, 'middle', MUTED)
        else:
            note = t('temp_fc' if mode == 2 else 'tint_fc').split(': ')[1]
            s.text(x0, y + 37, note, 9, 'start', MUTED)
    s.save(path)


def tone_out(ctrl, xs):
    p = T.set_tone(ctrl, 1, True)
    out = []
    for x in xs:
        y = 0.18 * 2 ** x
        xyz = [0.95047 * y, y, 1.08883 * y]
        o = T.tone5(xyz, p) if T.active(p) else xyz
        out.append(math.log2(max(o[1], 1e-9) / 0.18))
    return out


def fig_tone_curves(path):
    s = Svg(HALF, 300)
    p = Plot(s, (44, 14, HALF - 10, 248), (-8, 8), (-8, 8), t('scene_in'), t('out_stops'),
             range(-8, 9, 2), range(-8, 9, 2), yfmt=lambda v: num(v, 0))
    xs = frange(-8, 8, 200)
    sets = [({}, 'none', INK, '4,3'), ({'toneHighlights': -100}, 'hl100', WARM, None),
            ({'toneHighlights': -100, 'softClip': 1}, 'hl100sc', '#d64545', '2,2'),
            ({'toneShadows': 100, 'toneBlacks': -67}, 'sh', COOL, None),
            ({'toneContrast': 50}, 'con', '#2e9e5b', None), ({'toneHighlights': 50}, 'hlp', '#b04fc4', None)]
    s.line(p.x0, p.py(2.5), p.x1, p.py(2.5), '#c3c7cf', 1, '3,3')
    s.text(p.x0 + 4, p.py(2.5) - 4, t('bianco'), 9, 'start', MUTED)
    for ctrl, key, col, dash in sets:
        p.curve(xs, tone_out(ctrl, xs), col, 1.9, dash)
    p.legend(p.px(0.6), p.py(-2.6), [(t(k), c, d) for _, k, c, d in sets], 9.5, 12.5)
    s.save(path)


def fig_drt_curves(path):
    s = Svg(HALF, 262)
    p = Plot(s, (44, 10, HALF - 10, 214), (-8, 10), (0, 1), t('stops_grey'), t('rec709code'),
             range(-8, 11, 2), [0, 0.2, 0.4, 0.6, 0.8, 1.0])
    xs = frange(-8, 10, 160)
    d65 = [0.95047, 1.0, 1.08883]

    def enc(v):
        return max(v, 0.0) ** (1 / 2.4)
    p.curve(xs, [min(enc(0.18 * 2 ** x), 1.0) for x in xs], INK, 1.8, '4,3')
    for mode, key, col in ((1, 'aces2', WARM), (4, 'aces13', COOL), (5, 'opendrt', '#2e9e5b')):
        ys = [enc(D.render([c * 0.18 * 2 ** x for c in d65], mode, 1)[1]) for x in xs]
        p.curve(xs, ys, col, 2.0)
    g2 = enc(D.render([c * 0.18 for c in d65], 1, 1)[1])
    g4 = enc(D.render([c * 0.18 for c in d65], 4, 1)[1])
    s.circle(p.px(0), p.py(g2), 2.6, WARM, '#fff')
    s.circle(p.px(0), p.py(g4), 2.6, COOL, '#fff')
    s.text(p.px(0) - 5, p.py(g2) - 6, '%s / %s' % (num(g2, 3), num(g4, 3)), 9, 'end', MUTED)
    p.legend(p.px(1.6), p.py(0.30), [(t('cst'), INK, '4,3'), (t('aces2'), WARM, None), (t('aces13'), COOL, None),
                                     (t('opendrt'), '#2e9e5b', None)], 9, 12.5)
    s.save(path)


def fig_gamut_rgc(path):
    s = Svg(HALF, 250)
    p = Plot(s, (44, 14, HALF - 10, 200), (0.6, 1.4), (0.6, 1.1), t('dist_in'), t('dist_out'),
             [0.6, 0.8, 1.0, 1.2, 1.4], [0.6, 0.7, 0.8, 0.9, 1.0, 1.1], xfmt=lambda v: num(v, 1))
    xs = frange(0.6, 1.4, 160)
    p.curve(xs, xs, '#9aa0aa', 1.2, '4,3')
    s.line(p.x0, p.py(1.0), p.x1, p.py(1.0), '#d64545', 0.9, '2,2')
    s.text(p.x0 + 4, p.py(1.0) - 4, t('gamut_edge'), 9, 'start', '#d64545')
    cols = ('#16a3b5', '#b04fc4', '#c9a227')
    for i, key in enumerate(('cyan', 'magenta', 'yellow')):
        ys = []
        for d in xs:
            c = [1.0, 1.0, 1.0]
            c[i] = 1.0 - d
            o = D.compress(c, D.RGC_THR, D.RGC_LIM, D.RGC_PWR)
            ys.append(1.0 - o[i])
        p.curve(xs, ys, cols[i], 2.0)
    p.legend(p.px(1.0), p.py(0.80), [(t('cyan'), cols[0], None), (t('magenta'), cols[1], None),
                                      (t('yellow'), cols[2], None), (t('identity'), '#9aa0aa', '4,3')], 9, 12.5)
    s.save(path)


def fig_dlogm(path):
    s = Svg(HALF, 250)
    p = Plot(s, (44, 14, HALF - 10, 196), (-8, 5), (0, 1.05), t('stops_grey'), t('code'),
             range(-8, 6, 2), [0, 0.2, 0.4, 0.6, 0.8, 1.0])
    xs = frange(-8, 5, 200)
    p.curve(xs, [dm.encode1(0.18 * 2 ** x, 9) for x in xs], '#9aa0aa', 1.4, '4,3')
    p.curve(xs, [min(dm.encode1(0.18 * 2 ** x, 13), 1.05) for x in xs], WARM, 2.2)
    for i, lg in enumerate(dm.DLM):          # DLM[i] is log2 of the light at code (i + 1) / 16; code 0 is 0
        st = lg - math.log2(0.18)
        if st >= -8:
            s.circle(p.px(st), p.py((i + 1) / 16), 2.4, INK, '#fff', 0.6)
    top = dm.decode1(1.0, 13)
    g = dm.encode1(0.18, 13)
    s.line(p.px(0), p.y1, p.px(0), p.py(g), INK, 0.8, '2,2')
    s.text(p.px(0) + 4, p.py(g) + 14, '%s %s' % (t('grey'), num(g, 3)), 9, 'start', INK)
    s.text(p.px(math.log2(top / 0.18)) - 4, p.py(1.0) + 14, t('top'), 9, 'end', INK)
    p.legend(p.px(-7.6), p.py(0.95), [('D-Log M (3.2)', WARM, None), ('S-Log3', '#9aa0aa', '4,3'),
                                      (t('nodes'), INK, None)], 9, 12.5)
    s.save(path)


def fig_local_vs_point(path):
    """A 1-D illustration: a bright area with fine texture, compressed pointwise or by areas."""
    s = Svg(HALF, 250)
    p = Plot(s, (44, 14, HALF - 10, 196), (0, 1), (-1, 6.5), t('position'), t('luma_stops'),
             [0, 0.25, 0.5, 0.75, 1], range(-1, 7), xfmt=lambda v: '', yfmt=lambda v: num(v, 0))
    xs = frange(0, 1, 400)

    def base(x):
        return 0.5 + 4.5 / (1 + math.exp(-(x - 0.5) * 30))
    tex = [0.35 * math.sin(x * 2 * math.pi * 18) for x in xs]
    inp = [base(x) + d for x, d in zip(xs, tex)]
    ctrl = {'toneHighlights': -100}
    point = tone_out(ctrl, inp)
    local = [b + d for b, d in zip(tone_out(ctrl, [base(x) for x in xs]), tex)]
    p.curve(xs, inp, '#9aa0aa', 1.2)
    p.curve(xs, point, WARM, 1.8)
    p.curve(xs, local, COOL, 1.8)
    p.legend(p.px(0.03), p.py(6.1), [(t('input'), '#9aa0aa', None), (t('pointwise'), WARM, None),
                                     (t('local'), COOL, None)], 9.5, 12.5)
    s.save(path)


def fig_data_levels(path):
    """S-Log3 code values (black, grey, 90% white from encode1; 870 = top of the recorded content, docs/DATA_LEVELS.md)
    on the right scale and stretched as if they were video (datalevel.remap: x' = (x - 64) * 1023 / 876)."""
    s = Svg(HALF, 148)
    x0, x1 = 12, HALF - 12

    def px(c):
        return x0 + c / 1023 * (x1 - x0)
    marks = [('nero' if LANG == 0 else 'black', dm.encode1(0.0, 9) * 1023), ('18%', dm.encode1(0.18, 9) * 1023),
             ('90%', dm.encode1(0.9, 9) * 1023), ('max', 870.0)]
    for r, (key, f) in enumerate((('full_ok', lambda c: c), ('as_video', lambda c: (c - 64) * 1023 / 876))):
        y = 18 + r * 50
        s.text(x0, y - 5, t(key), 10.5, 'start', INK, 'bold')
        s.rect(px(0), y, px(1023) - px(0), 16, SOFT, GRID)
        s.rect(px(64), y, px(940) - px(64), 16, '#dce6fc')
        for i, (lab, c) in enumerate(marks):
            v = f(c)
            col = PAL[i] if i else '#b04fc4'
            s.line(px(v), y - 1, px(v), y + 17, col, 2)
            s.text(px(v), y + 29, '%s %d' % (lab, round(v)), 9.5, 'middle', col)
    s.text(px(36) + 2, 18 + 50 + 40, '↑ ' + t('neg'), 9.5, 'start', '#b04fc4', italic=True)
    for c in (0, 64, 512, 940, 1023):
        s.text(px(c), 142, '%d' % c, 9, 'middle', MUTED)
    s.text(px(502), 130, t('video_range'), 9.5, 'middle', COOL)
    s.save(path)


def fig_curve_tops(path):
    """Top of each input curve (code 1.0) in stops over grey, from decode1: why short curves need the clip rule."""
    curves = [(5, 'Rec.709'), (13, 'D-Log M'), (24, 'HLG'), (15, 'F-Log'), (7, 'S-Log'), (12, 'Apple Log'),
              (8, 'S-Log2'), (19, 'C-Log3'), (9, 'S-Log3'), (11, 'V-Log'), (20, 'LogC3'), (22, 'Log3G10'),
              (21, 'LogC4')]
    s = Svg(FULL, 112)
    p = Plot(s, (44, 8, FULL - 8, 80), (0, len(curves)), (0, 12), '', 'stop' if LANG == 0 else 'stops',
             (), (0, 4, 5.5, 8, 12), yfmt=lambda v: num(v, 1 if v % 1 else 0))
    s.rect(p.x0, p.py(5.5), p.x1 - p.x0, p.py(4.0) - p.py(5.5), '#ffe01f', opacity=0.28)
    s.rect(p.x0, p.py(12), p.x1 - p.x0, p.py(5.5) - p.py(12), '#f5291a', opacity=0.10)
    for i, (g, name) in enumerate(curves):
        top = math.log2(dm.decode1(1.0, g) / 0.18)
        x = p.px(i + 0.18)
        w = p.px(i + 0.82) - x
        s.rect(x, p.py(top), w, p.py(0) - p.py(top), WARM if top < 5.5 else '#9aa0aa', rx=1.5)
        s.text(x + w / 2, p.py(top) - 3, '+' + num(top, 1), 8.5, 'middle', INK)
        s.text(x + w / 2, p.y1 + 13, name, 9, 'middle', INK)
    s.text(p.x0 + 6, p.py(10.3), t('yellow_band') + ' · ' + t('red_band'), 9, 'start', MUTED)
    s.text(p.x0 + (p.x1 - p.x0) / 2, p.y1 + 27, t('top_axis'), 9.5, 'middle', INK)
    s.save(path)


# --- block diagrams
def boxes(s, items, y, h, x0, x1, gap=14, fill=SOFT, size=10.5):
    n = len(items)
    w = (x1 - x0 - gap * (n - 1)) / n
    centres = []
    for i, it in enumerate(items):
        label, sub, col = it
        x = x0 + i * (w + gap)
        s.rect(x, y, w, h, fill, col, 5, 1.4)
        lines = label.split('\n')
        ty = y + h / 2 - (len(lines) - 1) * 6.5 + (3 if not sub else -3)
        for j, ln in enumerate(lines):
            s.text(x + w / 2, ty + j * 13, ln, size, 'middle', INK, 'bold')
        if sub:
            s.text(x + w / 2, y + h - 7, sub, 8 if size < 10 else 8.5, 'middle', MUTED)
        if i:
            s.arrow(x - gap + 1, y + h / 2, x - 1, y + h / 2, MUTED, 1.2)
        centres.append(x + w / 2)
    return centres


FLOW = {
    0: [('Script', 'Workspace › Scripts', INK), ('1 · Noise', 'rumore, sul log', '#7a8fa6'),
        ('2 · Stabilize', 'giroscopio', '#7a8fa6'), ('3 · Motion\nBlur', 'mosso', '#7a8fa6'),
        ('4 · Log\nMetaRaw', 'Develop', WARM), ('5 · Creative', 'pellicola, video', COOL),
        ('grade ·\nCST / LUT', 'per ultimo', INK)],
    1: [('Script', 'Workspace › Scripts', INK), ('1 · Noise', 'noise, on log', '#7a8fa6'),
        ('2 · Stabilize', 'gyro', '#7a8fa6'), ('3 · Motion\nBlur', 'motion blur', '#7a8fa6'),
        ('4 · Log\nMetaRaw', 'Develop', WARM), ('5 · Creative', 'film, video', COOL),
        ('grade ·\nCST / LUT', 'last', INK)],
}
CACHE = ('metadata → Media Pool + record JSON per clip', 'metadata → Media Pool + one JSON record per clip')


def fig_flow(path):
    s = Svg(FULL, 92)
    c = boxes(s, FLOW[LANG], 8, 50, 6, FULL - 6, gap=9, size=9.5)
    s.text(c[0], 78, CACHE[LANG], 9, 'start', MUTED, italic=True)
    s.save(path)


CHAIN = {
    0: [('decodifica', 'curva → lineare', INK), ('Data level', 'code value', INK), ('EI', '× 2^stop', WARM),
        ('Bradford', 'Temp. · Tint', WARM),
        ('Toni · Zone', '1 guadagno', COOL), ('gamut', 'RGC · mappa', COOL),
        ('tone\nmapping', 'o CST', INK), ('codifica', 'uscita', INK)],
    1: [('decode', 'curve → linear', INK), ('Data level', 'code values', INK), ('EI', '× 2^stops', WARM),
        ('Bradford', 'Temp. · Tint', WARM),
        ('Toni · Zone', '1 gain', COOL), ('gamut', 'RGC · map', COOL),
        ('tone\nmapping', 'or CST', INK), ('encode', 'output', INK)],
}
FC_NOTE = ('False color: sostituisce l\'immagine dopo il bilanciamento',
           'False color: replaces the image after the balance')


def fig_develop_chain(path):
    s = Svg(FULL, 86)
    c = boxes(s, CHAIN[LANG], 6, 50, 4, FULL - 4, gap=10, size=9.5)
    s.text(c[3], 76, FC_NOTE[LANG], 9, 'middle', MUTED, italic=True)
    s.save(path)


FIGURES = [('log_curves', fig_log_curves), ('wb_locus', fig_wb_locus), ('false_color', fig_false_color),
           ('tone_curves', fig_tone_curves), ('drt_curves', fig_drt_curves), ('gamut_rgc', fig_gamut_rgc),
           ('dlogm', fig_dlogm), ('local_vs_point', fig_local_vs_point),
           ('flow', fig_flow), ('develop_chain', fig_develop_chain), ('data_levels', fig_data_levels),
           ('curve_tops', fig_curve_tops)]


def main(argv):
    global LANG
    out = argv[1] if len(argv) > 1 else os.path.join(ROOT, 'packaging', 'guide', 'img')
    os.makedirs(out, exist_ok=True)
    for LANG, suffix in ((0, 'it'), (1, 'en')):
        for name, fn in FIGURES:
            fn(os.path.join(out, '%s_%s.svg' % (name, suffix)))
    print('written %d figures in %s' % (2 * len(FIGURES), out))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

# SPDX-License-Identifier: GPL-3.0-or-later
"""Python reference of the display renderings (math/Display.h).

Every rendering takes the scene XYZ (D65) and returns display-linear RGB in the output gamut, 0..1:
    1 ACES 2.0: Output Transform SDR 100 nit, Rec.709 limiting gamut, D65 (tools/aces2.py, from OCIO 2.6)
    2 Luminanza: max-RGB norm through the tonescale, ratios kept, path to white, display gamut compression
    3 Filmico: per channel on an inset gamut (AgX-like), same tonescale and compressions
    4 ACES 1.3: Reference Gamut Compression, RRT, ODT Rec.709 100 nit dim (CTL, ACES 1.x)
    5 OpenDRT v1.1.0, look Standard, Rec.1886 preset (model/opendrt.py, a port of Jed Smith's DCTL)
1 and 4 are checked against OpenColorIO's built-ins (tests/test_display.py), 5 against values of the original DCTL;
2 and 3 are this plugin's own.
"""
import math

from .opendrt import opendrt
from .tone import dm
import aces2   # tools/, on the path through develop_model

ACES2 = aces2.Aces2()

mul = dm.mul
AP0_TO_AP1 = [[1.4514393161, -0.2365107469, -0.2149285693], [-0.0765537734, 1.1762296998, -0.0996759264],
              [0.0083161484, -0.0060324498, 0.9977163014]]
AP1_TO_AP0 = [[0.6954522414, 0.1406786965, 0.1638690622], [0.0447945634, 0.8596711185, 0.0955343182],
              [-0.0055258826, 0.0040252103, 1.0015006723]]
AP1_TO_XYZ = [[0.6624541811, 0.1340042065, 0.1561876870], [0.2722287168, 0.6740817658, 0.0536895174],
              [-0.0055746495, 0.0040607335, 1.0103391003]]
XYZ_TO_AP1 = [[1.6410233797, -0.3248032942, -0.2364246952], [-0.6636628587, 1.6153315917, 0.0167563477],
              [0.0117218943, -0.0082844420, 0.9883948585]]
REC709, AP0, AP1 = 1, 9, 10   # gamut codes (math/Gamut.h), Bradford-adapted to D65

C5_LO = [-4.0, -4.0, -3.1573765773, -0.4852499958, 1.8477324706, 1.8477324706]
C5_HI = [-0.7185482425, 2.0810307172, 3.6681241237, 4.0, 4.0, 4.0]
C9_LO = [-1.6989700043, -1.6989700043, -1.4779, -1.2291, -0.8648, -0.448, 0.00518, 0.4511080334,
         0.9113744414, 0.9113744414]
C9_HI = [0.5154386965, 0.8470437783, 1.1358, 1.3802, 1.5197, 1.5985, 1.6467, 1.6746091357,
         1.6878733390, 1.6878733390]
# log10 x and log10 y of the min, mid and max points; the ODT ones (C9_PTS, below) are the RRT spline
# at grey -/+ 6.5 stops
C5_PTS = (math.log10(0.18 * 2 ** -15), -4.0, math.log10(0.18), math.log10(4.8), math.log10(0.18 * 2 ** 18), 4.0)
HALF_MIN = 5.96046448e-08

RGC_THR = (0.815, 0.803, 0.880)    # ACES 1.3 Reference Gamut Compression: cyan, magenta, yellow
RGC_LIM = (1.147, 1.264, 1.312)
RGC_PWR = 1.2
DISP_THR, DISP_LIM = 0.9, 1.2      # into the display gamut, every channel

# Tonescale (Siragusano, as in ACES 2.0 and OpenDRT), 100 nit: grey 0.18 -> 0.1, 128 -> 1.0
TS_G, TS_T1, TS_C, TS_CD, TS_RHIT = 1.15, 0.04, 0.18, 10.013, 128.0
INSET, OUTSET = 0.2, 0.18          # Filmico: mix towards the luminance before and after the curve
WHITE_POW = 8.0                    # Luminanza: weight of the path to white, the toned norm to this power


def spline(x, lo, hi, pts, n, slope_hi):
    lmin_x, lmin_y, lmid_x, _lmid_y, lmax_x, lmax_y = pts
    logx = math.log10(max(x, HALF_MIN))
    if logx <= lmin_x:
        return 10 ** lmin_y
    if logx >= lmax_x:
        return 10 ** (lmax_y + slope_hi * (logx - lmax_x))
    if logx < lmid_x:
        k, c = (n - 1) * (logx - lmin_x) / (lmid_x - lmin_x), lo
    else:
        k, c = (n - 1) * (logx - lmid_x) / (lmax_x - lmid_x), hi
    j = int(k)
    t = k - j
    c0, c1, c2 = c[j], c[j + 1], c[j + 2]
    return 10 ** (t * t * (0.5 * c0 - c1 + 0.5 * c2) + t * (c1 - c0) + 0.5 * (c0 + c1))


def sat_mat(c, s):
    y = sum(AP1_TO_XYZ[1][i] * c[i] for i in range(3))
    return [(1 - s) * y + s * v for v in c]


def compress(c, thr, lim):
    """ACES RGC distance compression towards the max channel, per channel."""
    ach = max(c)
    if ach <= 0.0:
        return list(c)
    out = []
    for i in range(3):
        d = (ach - c[i]) / ach
        t, l = thr[i], lim[i]
        s = (l - t) / (((1 - t) / (l - t)) ** -RGC_PWR - 1) ** (1 / RGC_PWR)
        if d > t:
            n = (d - t) / s
            d = t + s * n / (1 + n ** RGC_PWR) ** (1 / RGC_PWR)
        out.append(ach - d * ach)
    return out


def rrt(a):
    mx, mn = max(a), min(a)
    sat = (max(mx, 1e-10) - max(mn, 1e-10)) / max(mx, 1e-2)
    r, g, b = a
    yc = (b + g + r + 1.75 * math.sqrt(max(b * (b - g) + g * (g - r) + r * (r - b), 0.0))) / 3
    x = (sat - 0.4) / 0.2
    t = max(1 - abs(x / 2), 0.0)
    s = (1 + math.copysign(1.0, x) * (1 - t * t)) / 2
    gain = 0.05 * s
    glow = gain if yc <= 0.08 * 2 / 3 else 0.0 if yc >= 0.16 else gain * (0.08 / yc - 0.5)
    a = [v * (1 + glow) for v in a]
    r, g, b = a
    hue = 0.0 if r == g == b else math.degrees(math.atan2(math.sqrt(3) * (g - b), 2 * r - g - b))
    hue = hue - 360 if hue > 180 else hue
    w = 0.0
    if -67.5 < hue < 67.5:   # cubic B-spline basis, width 135
        k = (hue + 67.5) * 4 / 135
        j = int(k)
        t = k - j
        w = [t ** 3 / 6, (-3 * t ** 3 + 3 * t * t + 3 * t + 1) / 6,
             (3 * t ** 3 - 6 * t * t + 4) / 6, (1 - t) ** 3 / 6][j] * 1.5
    a[0] += w * sat * (0.03 - a[0]) * (1 - 0.82)
    p = [max(v, 0.0) for v in mul(AP0_TO_AP1, [max(v, 0.0) for v in a])]
    p = sat_mat(p, 0.96)
    return [spline(v, C5_LO, C5_HI, C5_PTS, 4, 0.0) for v in p]   # AP1, as the ODT reads it


def odt(p):
    p = [spline(v, C9_LO, C9_HI, C9_PTS, 8, 0.04) for v in p]
    lin = [(v - 0.02) / (48.0 - 0.02) for v in p]
    X, Y, Z = mul(AP1_TO_XYZ, lin)   # dark to dim surround: Y^0.9811 at the same xy
    s = X + Y + Z
    if s != 0.0:
        k = (max(Y, 0.0) ** 0.9811) / max(Y / s, 1e-10) / s if Y > 0 else 0.0
        lin = mul(XYZ_TO_AP1, [X * k, Y * k if Y > 0 else 0.0, Z * k])
    return sat_mat(lin, 0.93)        # display linear AP1 (D60 white, adapted below)


def aces(xyz):
    a = compress(mul(dm.MATS[AP1][1], xyz), RGC_THR, RGC_LIM)
    return mul(dm.MATS[AP1][0], odt(rrt(mul(AP1_TO_AP0, a))))


def ts_constants():
    m1 = 0.5 * (1 + math.sqrt(1 + 4 * TS_T1))
    u = ((TS_RHIT / m1) / (TS_RHIT / m1 + 1)) ** TS_G
    m = m1 / u
    ct = TS_CD / 100.0
    gip = 0.5 * (ct + math.sqrt(ct * (ct + 4 * TS_T1)))
    q = (gip / m) ** (1 / TS_G)
    gipp2 = -m1 * q / (q - 1)
    w2 = TS_C / gipp2
    s2 = w2 * m1
    u2 = ((TS_RHIT / m1) / (TS_RHIT / m1 + w2)) ** TS_G
    return m1 / u2, s2


TS_M2, TS_S2 = ts_constants()
C9_PTS = (math.log10(spline(0.18 * 2 ** -6.5, C5_LO, C5_HI, C5_PTS, 4, 0.0)), math.log10(0.02),
          math.log10(spline(0.18, C5_LO, C5_HI, C5_PTS, 4, 0.0)), math.log10(4.8),
          math.log10(spline(0.18 * 2 ** 6.5, C5_LO, C5_HI, C5_PTS, 4, 0.0)), math.log10(48.0))


def tonescale(x):
    x = max(x, 0.0)
    f = TS_M2 * (x / (x + TS_S2)) ** TS_G
    return min(f * f / (f + TS_T1), 1.0)


def into_display(xyz, space):
    c = mul(dm.MATS[space][1], mul(dm.MATS[AP1][0], compress(mul(dm.MATS[AP1][1], xyz), RGC_THR, RGC_LIM)))
    return [max(v, 0.0) for v in compress(c, (DISP_THR,) * 3, (DISP_LIM,) * 3)]


def luminance(xyz, space):
    c = into_display(xyz, space)
    n = max(c)
    if n <= 0.0:
        return [0.0, 0.0, 0.0]
    nt = tonescale(n)
    w = nt ** WHITE_POW
    return [nt * (v / n + w * (1 - v / n)) for v in c]


def filmic(xyz, space):
    c = into_display(xyz, space)
    ly = dm.MATS[space][0][1]
    y = sum(ly[i] * c[i] for i in range(3))
    c = [tonescale((1 - INSET) * v + INSET * y) for v in c]
    y = sum(ly[i] * c[i] for i in range(3))
    return [(v - OUTSET * y) / (1 - OUTSET) for v in c]


def highlight_vibrance(c, a, space):
    """Vibrance luci (sm_highlight_vibrance): more chroma where bright and little saturated, Y kept, 0..1 kept."""
    ly = dm.MATS[space][0][1]
    y = sum(ly[i] * c[i] for i in range(3))
    mx, mn = max(c), min(c)
    if a <= 0 or mx <= 0:
        return list(c)
    u = 1 - (mx - mn) / mx
    h = min(max((y - 0.25) / 0.6, 0.0), 1.0)
    m = 1 + 2 * a * u * u * h * h * (3 - 2 * h)
    d = [v - y for v in c]
    for v in d:
        if v > 0:
            m = min(m, (1 - y) / v)
        elif v < 0:
            m = min(m, y / -v)
    m = max(m, 1.0)
    return [y + m * v for v in d]


def render(xyz, mode, space):
    """Display-linear RGB in the gamut `space`, clamped to 0..1."""
    if mode == 1:
        c = mul(dm.MATS[space][1], mul(dm.MATS[REC709][0], ACES2.output_transform(mul(dm.MATS[AP0][1], xyz))))
    elif mode == 4:
        c = mul(dm.MATS[space][1], aces(xyz))
    elif mode == 5:
        c = mul(dm.MATS[space][1], mul(dm.MATS[REC709][0], opendrt(xyz)))
    elif mode == 2:
        c = luminance(xyz, space)
    else:
        c = filmic(xyz, space)
    return [min(max(v, 0.0), 1.0) for v in c]

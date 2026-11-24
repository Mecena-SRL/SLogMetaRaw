# SPDX-License-Identifier: GPL-3.0-or-later
"""Python reference of the SLogMetaRaw develop math (ofx/SLogMetaRaw/DevelopMath.h)."""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'tools'))
from gamut_matrices import matrices  # noqa: E402

MATS = [(m, mi) for _name, m, mi in matrices()]  # index = space code
BR = [[0.8951, 0.2664, -0.1614], [-0.7502, 1.7135, 0.0367], [0.0389, -0.0685, 1.0296]]
BRI = [[0.98699291, -0.14705426, 0.15996265], [0.43230527, 0.51836027, 0.04929123],
       [-0.00852866, 0.04004282, 0.96848669]]
PRESETS = {1: 5600, 2: 6500, 3: 7500, 4: 3200, 5: 4000, 6: 5500}


def mul(m, v):
    return [sum(m[i][k] * v[k] for k in range(3)) for i in range(3)]


def spow(x, p):
    return -((-x) ** p) if x < 0 else x ** p


def slog_enc(r):
    y = 0.432699 * math.log10(r + 0.037584) + 0.616596 + 0.03 if r >= 0 else r * 5 + 0.030001222851889303
    return (y * 876 + 64) / 1023


def slog_dec(x):
    y = (x * 1023 - 64) / 876
    return 10 ** ((y - 0.616596 - 0.03) / 0.432699) - 0.037584 if y >= 0.030001222851889303 \
        else (y - 0.030001222851889303) / 5


LC4_A, LC4_B, LC4_C = (2 ** 18 - 16) / 117.45, (1023 - 95) / 1023, 95 / 1023
LC4_S = 7 * math.log(2) * 2 ** (7 - 14 * LC4_C / LC4_B) / (LC4_A * LC4_B)
LC4_T = (2 ** (14 * (-LC4_C / LC4_B) + 6) - 64) / LC4_A
HLG_A, HLG_B, HLG_C = 0.17883277, 0.28466892, 0.55991073
HLG_GREY = 0.18 / (0.38 * 0.38 / 3)          # BT.2408: 38% HLG signal = 18% grey
CINE = {15: (0.555556, 0.009468, 0.344676, 0.790453, 8.735631, 0.092864, 0.00089),     # F-Log
        16: (5.555556, 0.064829, 0.245281, 0.384316, 8.799461, 0.092864, 0.000889),    # F-Log2
        20: (5.555556, 0.052272, 0.247190, 0.385537, 5.367655, 0.092809, 0.010591)}    # LogC3 EI 800
DLM = (-7.1430240, -5.7811094, -4.7320245, -3.8868240, -3.2453822, -2.7471208, -2.3173791, -1.9180706,   # log2 at i/16
       -1.5247973, -1.1241255, -0.7105378, -0.2958065, 0.1171398, 0.5602375, 1.0579305, 1.6015527)


def clog3_enc(x):
    if x < -0.014: return -0.36726845 * math.log10(-x * 14.98325 + 1) + 0.12783901
    if x <= 0.014: return 1.9754798 * x + 0.12512219
    return 0.36726845 * math.log10(x * 14.98325 + 1) + 0.12240537


def clog3_dec(y):
    if y < 0.097465473: return -(10 ** ((0.12783901 - y) / 0.36726845) - 1) / 14.98325
    if y <= 0.15277891: return (y - 0.12512219) / 1.9754798
    return (10 ** ((y - 0.12240537) / 0.36726845) - 1) / 14.98325


def encode_camera(y, g):
    """Codes 11+: the other cameras' log curves (Transfer.h)."""
    if g == 11: return 5.6 * y + 0.125 if y < 0.01 else 0.241514 * math.log10(y + 0.00873) + 0.598206
    if g == 12:
        if y >= 0.01: return 0.08550479 * math.log2(y + 0.00964052) + 0.69336945
        return 47.28711236 * (y + 0.05641088) ** 2 if y >= -0.05641088 else 0.0
    if g == 13:
        top = 16 * (DLM[15] - DLM[14])
        if y < 2 ** DLM[0]: return y / (16 * 2 ** DLM[0])
        lg = math.log2(y)
        if lg >= DLM[15]: return 1 + (lg - DLM[15]) / top
        i = next(i for i in range(1, 16) if DLM[i] > lg)
        return (i + (lg - DLM[i - 1]) / (DLM[i] - DLM[i - 1])) / 16
    if g == 14: return 6.025 * y + 0.0929 if y <= 0.0078 else math.log10(y * 0.9892 + 0.0108) * 0.256663 + 0.584555
    if g in CINE:
        a, b, c, d, e, f, cut = CINE[g]
        return c * math.log10(a * y + b) + d if y >= cut else e * y + f
    if g == 17: return 650 * spow(y + 0.0075, 1 / 3) / 1023 if y < 0.328 else (150 * math.log(y) + 619) / 1023
    if g == 18:
        x = y / 0.9
        return math.copysign(0.24136077 * math.log10(abs(x) * 87.09937546 + 1), x) + 0.092864125
    if g == 19: return clog3_enc(y / 0.9)
    if g == 21: return (math.log2(LC4_A * y + 64) - 6) / 14 * LC4_B + LC4_C if y >= LC4_T else (y - LC4_T) / LC4_S
    if g == 22: return (y + 0.01) * 15.1927 if y + 0.01 < 0 else 0.224282 * math.log10((y + 0.01) * 155.975327 + 1)
    if g == 23: return 8.283605932402494 * y + 0.09246575342465753 if y < 0.005 \
        else 0.08692876065491224 * math.log(y + 0.005494072432257808) + 0.5300133392291939
    if g == 24:
        e = max(y, 0.0) / HLG_GREY
        return math.sqrt(3 * e) if e <= 1 / 12 else HLG_A * math.log(12 * e - HLG_B) + HLG_C
    return y


def decode_camera(x, g):
    if g == 11: return (x - 0.125) / 5.6 if x < 0.181 else 10 ** ((x - 0.598206) / 0.241514) - 0.00873
    if g == 12:
        if x >= 0.20855532: return 2 ** ((x - 0.69336945) / 0.08550479) - 0.00964052
        return math.sqrt(x / 47.28711236) - 0.05641088 if x > 0 else -0.05641088
    if g == 13:
        if x < 1 / 16: return x * 16 * 2 ** DLM[0]
        if x >= 1: return 2 ** (DLM[15] + (x - 1) * 16 * (DLM[15] - DLM[14]))
        i = int(x * 16)
        return 2 ** (DLM[i - 1] + (x * 16 - i) * (DLM[i] - DLM[i - 1]))
    if g == 14: return (x - 0.0929) / 6.025 if x <= 0.14 else (10 ** (3.89616 * x - 2.27752) - 0.0108) / 0.9892
    if g in CINE:
        a, b, c, d, e, f, cut = CINE[g]
        return (10 ** ((x - d) / c) - b) / a if x >= e * cut + f else (x - f) / e
    if g == 17:
        v = x * 1023
        return (v / 650) ** 3 - 0.0075 if v < 452 else math.exp((v - 619) / 150)
    if g == 18:
        o = 0.092864125
        return math.copysign((10 ** (abs(x - o) / 0.24136077) - 1) / 87.09937546, x - o) * 0.9
    if g == 19: return clog3_dec(x) * 0.9
    if g == 21: return (2 ** (14 * (x - LC4_C) / LC4_B + 6) - 64) / LC4_A if x >= 0 else x * LC4_S + LC4_T
    if g == 22: return x / 15.1927 - 0.01 if x < 0 else (10 ** (x / 0.224282) - 1) / 155.975327 - 0.01
    if g == 23: return (x - 0.09246575342465753) / 8.283605932402494 if x < 0.13388378308667 \
        else math.exp((x - 0.5300133392291939) / 0.08692876065491224) - 0.005494072432257808
    if g == 24:
        e = x * x / 3 if x <= 0.5 else (math.exp((x - HLG_C) / HLG_A) + HLG_B) / 12
        return e * HLG_GREY
    return x


def encode1(y, g):
    if g == 0: return y * 10.44426855 if y <= 0.00262409 else (math.log2(y + 0.0075) + 7) * 0.07329248
    if g == 2: return spow(y, 1 / 2.2)
    if g == 3: return spow(y, 1 / 2.4)
    if g == 4: return spow(y, 1 / 2.6)
    if g == 5: return 4.5 * y if y < 0.018 else 1.099 * y ** 0.45 - 0.099
    if g == 6: return 12.92 * y if y <= 0.0031308 else 1.055 * y ** (1 / 2.4) - 0.055
    if g == 7: return slog_enc(y / 0.9)
    if g == 8: return slog_enc(y * 155 / 219 / 0.9)
    if g == 9: return (420 + math.log10((y + 0.01) / 0.19) * 261.5) / 1023 if y >= 0.01125 \
        else (y * (171.2102946929 - 95) / 0.01125 + 95) / 1023
    if g == 10: return 10.5402377416545 * y + 0.0729055341958355 if y <= 0.0078125 else (math.log2(y) + 9.72) / 17.52
    return encode_camera(y, g)


def decode1(x, g):
    if g == 0: return x / 10.44426855 if x <= 0.02740668 else 2 ** (x / 0.07329248 - 7) - 0.0075
    if g == 2: return spow(x, 2.2)
    if g == 3: return spow(x, 2.4)
    if g == 4: return spow(x, 2.6)
    if g == 5: return x / 4.5 if x < 0.081 else ((x + 0.099) / 1.099) ** (1 / 0.45)
    if g == 6: return x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4
    if g == 7: return slog_dec(x) * 0.9
    if g == 8: return slog_dec(x) * 0.9 * 219 / 155
    if g == 9: return 10 ** ((x * 1023 - 420) / 261.5) * 0.19 - 0.01 if x >= 171.2102946929 / 1023 \
        else (x * 1023 - 95) * 0.01125 / (171.2102946929 - 95)
    if g == 10: return (x - 0.0729055341958355) / 10.5402377416545 if x <= 0.155251141552511 else 2 ** (x * 17.52 - 9.72)
    return decode_camera(x, g)


def xy_uv(x, y):
    d = -2 * x + 12 * y + 3
    return 4 * x / d, 6 * y / d


def uv_xy(u, v):
    d = 2 * u - 8 * v + 4
    return 3 * u / d, 2 * v / d


WB_KMIN, WB_KMAX, WB_TINT_MAX = 1500.0, 15000.0, 150.0


def locus_uv(t):
    """Planckian locus in CIE 1960 uv (Krystek 1985) and its unit normal towards green, as sm_locus_uv."""
    t = min(max(t, WB_KMIN), WB_KMAX)
    au = 0.860117757 + 1.54118254e-4 * t + 1.28641212e-7 * t * t
    bu = 1.0 + 8.42420235e-4 * t + 7.08145163e-7 * t * t
    av = 0.317398726 + 4.22806245e-5 * t + 4.20481691e-8 * t * t
    bv = 1.0 - 2.89741816e-5 * t + 1.61456053e-7 * t * t
    du = ((1.54118254e-4 + 2 * 1.28641212e-7 * t) * bu - au * (8.42420235e-4 + 2 * 7.08145163e-7 * t)) / bu ** 2
    dv = ((4.22806245e-5 + 2 * 4.20481691e-8 * t) * bv - av * (-2.89741816e-5 + 2 * 1.61456053e-7 * t)) / bv ** 2
    l = math.hypot(du, dv)
    n = (-dv / l, du / l)
    if n[1] < 0:
        n = (-n[0], -n[1])
    return au / bu, av / bv, n


def uv_xyz(u, v):
    x, y = uv_xy(u, v)
    return [x / y, 1.0, (1 - x - y) / y]


def bradford_s(u, v):
    return mul(BR, uv_xyz(u, v))[2]


def green_limit(u, v, n):
    """Largest green Duv that leaves the white's Bradford S cone 30% of its value on the locus."""
    target = 0.3 * bradford_s(u, v)
    lo, hi = 0.0, 0.2
    for _ in range(60):
        m = 0.5 * (lo + hi)
        if bradford_s(u + n[0] * m, v + n[1] * m) > target:
            lo = m
        else:
            hi = m
    return lo


def white_xyz(k, tint):
    u, v, n = locus_uv(k)
    d = min(max(tint, -WB_TINT_MAX), WB_TINT_MAX) / 3000
    if d > 0:
        lim = green_limit(u, v, n)
        a, b = 0.5 * lim, 0.5 * lim
        if d > a:
            d = a + b * (1 - math.exp(-(d - a) / b))
    return uv_xyz(u + n[0] * d, v + n[1] * d)


def fix_levels(lin, node_space, level_space, level_gamma, gain, offset):
    """Reference for sm_fix_levels: affine remap of the code values, done in the
    camera encoding, reached by an exact round-trip from whatever the node is in."""
    same = level_space == node_space
    cam = lin if same else mul(MATS[level_space][1], mul(MATS[node_space][0], lin))
    cv = [encode1(c, level_gamma) * gain + offset for c in cam]
    cam = [decode1(c, level_gamma) for c in cv]
    return cam if same else mul(MATS[node_space][1], mul(MATS[level_space][0], cam))


# --- false colour (math/FalseColor.h) -------------------------------------

def neutral_uv(shot_k, shot_t, k, t):
    """CIE 1960 uv a neutral pixel lands on after the white balance, as neutralUV()."""
    d65 = [0.95046, 1.0, 1.08906]
    ws, wt = mul(BR, white_xyz(shot_k, shot_t)), mul(BR, white_xyz(k, t))
    r = [ws[i] / wt[i] for i in range(3)]
    lms = mul(BR, d65)
    xyz = mul(BRI, [lms[i] * r[i] for i in range(3)])
    nwy = mul(BRI, [mul(BR, d65)[i] * r[i] for i in range(3)])[1]
    X, Y, Z = [c / nwy for c in xyz]
    d = X + 15 * Y + 3 * Z
    return (4 * X / d, 6 * Y / d) if d > 1e-9 else (0.0, 0.0)


def fc_calibration(k, t):
    """(u0, v0, ku, kv, tu, tv): the inverse of the sliders' response, as buildParams."""
    dk = 100.0 if k + 100.0 <= WB_KMAX else -100.0
    dt = 5.0 if t + 5.0 <= WB_TINT_MAX else -5.0
    u0, v0 = neutral_uv(k, t, k, t)
    uk, vk = neutral_uv(k, t, k + dk, t)
    ut, vt = neutral_uv(k, t, k, t + dt)
    a, c = (uk - u0) / dk, (vk - v0) / dk
    b, e = (ut - u0) / dt, (vt - v0) / dt
    det = a * e - b * c
    return u0, v0, e / det, -b / det, -c / det, a / det


def fc_headroom(lin, node_gamma, expo):
    """sm_fc_headroom: stops from the largest channel to the top of the input curve (code 1.0), same exposure."""
    mx = max(lin)
    if node_gamma == 1 or not mx > 0.0:
        return math.inf
    return math.log2(decode1(1.0, node_gamma) * expo / mx)


def fc_exposure(ev, head=math.inf):
    if ev >= 5.5 or head <= 0.02: return [0.96, 0.16, 0.10]
    if ev >= 4.0 or head <= 0.25: return [1.00, 0.88, 0.12]
    if 0.70 <= ev <= 1.30: return [1.00, 0.56, 0.72]
    if -0.33 <= ev <= 0.33: return [0.13, 0.82, 0.20]
    if ev <= -6.0: return [0.42, 0.12, 0.62]
    if ev <= -4.0: return [0.10, 0.28, 0.88]
    g = min(max((ev + 6.0) / 11.5, 0.06), 0.92)
    return [g, g, g]


# Hues of the white-balance views, as CineMatch paints them (HSL): cool, warm, green, magenta
FC_HUE = {'cool': 0.586, 'warm': 0.08, 'green': 0.30, 'magenta': 0.85}


def hsl_rgb(h, s, l):
    q = l * (1.0 + s) if l < 0.5 else l + s - l * s
    p = 2.0 * l - q

    def channel(t):
        t = t - math.floor(t)
        if t < 1.0 / 6.0:
            return p + (q - p) * 6.0 * t
        if t < 0.5:
            return q
        if t < 2.0 / 3.0:
            return p + (q - p) * (2.0 / 3.0 - t) * 6.0
        return p
    return [channel(h + 1.0 / 3.0), channel(h), channel(h - 1.0 / 3.0)]


def fc_balance(xyz, mode, u, v, ku, kv, tu, tv, ev, mono, head=math.inf):
    """Temperature (2) and tint (3) views: the picture in grey, a pixel coloured only when its cast lies
    on this slider's axis - orange/blue, green/magenta - with its saturation boosted up to 8x near
    neutral as CineMatch does, and faded out where the pixel is too dark or too bright to judge."""
    d = xyz[0] + 15 * xyz[1] + 3 * xyz[2]
    rgb = [max(c, 0.0) for c in mul(MATS[1][1], xyz)]
    mx, mn = max(rgb), min(rgb)
    if d < 1e-9 or mx <= 0.0:
        return [mono] * 3
    du, dv = 4 * xyz[0] / d - u, 6 * xyz[1] / d - v
    kelvin = -(ku * du + kv * dv) / 100.0     # > 0: reads cool, wants warming
    tint = -(tu * du + tv * dv) / 5.0         # > 0: reads green
    if (abs(kelvin) >= abs(tint)) != (mode == 2):
        return [mono] * 3
    hi, lo = 1.0, (mn / mx) ** (1.0 / 2.2)    # display-encoded, normalised: exposure does not matter
    r = (hi - lo) / (hi + lo)
    sat = min(max(r * (8.0 - 7.0 * r), 0.0), 1.0)
    sat *= min(max((ev + 6.0) / 2.0, 0.0), 1.0) * min(max(min(5.5 - ev, 4.0 * head), 0.0), 1.0)
    if mode == 2:
        hue = FC_HUE['cool'] if kelvin > 0 else FC_HUE['warm']
    else:
        hue = FC_HUE['green'] if tint > 0 else FC_HUE['magenta']
    return hsl_rgb(hue, sat, mono)


def false_color(xyz, mode, u, v, ku, kv, tu, tv, head=math.inf):
    ev = math.log2(max(xyz[1], 1e-9) / 0.18)
    if mode == 1: return fc_exposure(ev, head)
    mono = min(max((ev + 6.0) / 11.5, 0.06), 0.92)
    return fc_balance(xyz, mode, u, v, ku, kv, tu, tv, ev, mono, head)


def fc_emit(c, node_space, node_gamma, out_space, out_gamma):
    convert = out_space != node_space or out_gamma != node_gamma
    space, gamma = (out_space, out_gamma) if convert else (node_space, node_gamma)
    lin = [decode1(x, 2) for x in c]
    return [encode1(x, gamma) for x in mul(MATS[space][1], mul(MATS[1][0], lin))]


def develop(rgb, node_space, node_gamma, shot=(5600, 0, 800),
            temp=None, tint=None, ei=None, wb_mode=0, out_space=None, out_gamma=None,
            tone=None, decode_meta=False,
            level_fix=0, level_space=0, level_gamma=0, level_gain=1.0, level_offset=0.0,
            fc_mode=0, fc_u=0.0, fc_v=0.0, fc_ku=0.0, fc_kv=0.0, fc_tu=0.0, fc_tv=0.0, drt=0, hl_vib=0.0,
            rgc='rendering', gmap='rendering'):
    """sm_develop. tone: the Toni and Zone panel values by OFX name (tests/model/tone.CONTROLS). rgc / gmap: Compressione
    and Mappatura gamut (model/display.render); 'rendering' = the pair the rendering proposes."""
    from model import tone as T   # the tone model imports this module: import it late
    shot_k, shot_t, shot_ei = shot
    temp = shot_k if temp is None else temp
    tint = shot_t if tint is None else tint
    ei = shot_ei if ei is None else ei
    out_space = node_space if out_space is None else out_space
    out_gamma = node_gamma if out_gamma is None else out_gamma
    convert = out_space != node_space or out_gamma != node_gamma
    if decode_meta and not level_fix:
        return list(rgb)
    lin = [decode1(c, node_gamma) for c in rgb]
    if level_fix:
        lin = fix_levels(lin, node_space, level_space, level_gamma, level_gain, level_offset)
    if decode_meta:   # develop off, data level only
        return [encode1(c, node_gamma) for c in lin]
    lin = [c * (ei / shot_ei) for c in lin]
    xyz = mul(MATS[node_space][0], lin)
    k, t = (PRESETS[wb_mode], 0.0) if wb_mode in PRESETS else (temp, tint)
    ws, wt = mul(BR, white_xyz(shot_k, shot_t)), mul(BR, white_xyz(k, t))
    r = [ws[i] / wt[i] for i in range(3)]
    lms = mul(BR, xyz)
    xyz = mul(BRI, [lms[i] * r[i] for i in range(3)])
    wl = mul(BR, [0.95046, 1.0, 1.08906])
    nwy = mul(BRI, [wl[i] * r[i] for i in range(3)])[1]
    xyz = [c / nwy for c in xyz]
    p = T.set_tone(tone or {}, out_space, convert, ei / shot_ei)
    p['outGamut'] = out_space          # the gamut the node writes: the path to white keeps it non-negative
    if fc_mode:   # measured before the trims, so it answers only to its own controls
        view = (T.false_color_zones(xyz, p) if fc_mode == 4
                else false_color(xyz, fc_mode, fc_u, fc_v, fc_ku, fc_kv, fc_tu, fc_tv,
                                 fc_headroom(lin, node_gamma, ei / shot_ei)))
        return fc_emit(view, node_space, node_gamma, out_space, out_gamma)
    if T.active(p):
        xyz = T.tone5(xyz, p)
    from model import display
    if rgc == 'rendering':
        rgc = display.RECOMMENDED[drt][0]
    if gmap == 'rendering':
        gmap = display.RECOMMENDED[drt][1]
    if rgc:
        xyz = display.gamut_compress(xyz, rgc)
    if drt:   # rendered for a display gamut: the output one, else Rec.709 carried in the output space
        ds = out_space if 1 <= out_space <= 5 else 1
        c = display.render(xyz, drt, ds, None, gmap)
        if hl_vib > 0:
            c = display.highlight_vibrance(c, hl_vib, ds)
        if ds != out_space:
            c = mul(MATS[out_space][1], mul(MATS[ds][0], c))
        return [encode1(v, out_gamma) for v in c]
    space, gamma = (out_space, out_gamma) if convert else (node_space, node_gamma)
    o = mul(MATS[space][1], xyz)
    return [encode1(c, gamma) for c in (display.gamut_map(o) if gmap else o)]

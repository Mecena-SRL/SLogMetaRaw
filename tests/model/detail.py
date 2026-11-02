# SPDX-License-Identifier: GPL-3.0-or-later
"""Python reference of the S-Log MetaRaw Detail node (math/Detail.h, src/detail).

Everything works on log2 luminance L = log2(N / 0.18) of the input's power norm (in Rec.2020), and
returns one log2 gain G per pixel: out = in * 2^G, in the input encoding.

    grid      L reduced with a tent filter to a working height in [382, 764), radii relative to H
    base B    self-guided filter of L on the grid, applied to a noise-gated guide at full size
    tones     the same zone curve as the develop node (model.tone.t5_curve) on B, detail kept
    Clarity   the difference of two guided filters (0.5 % and 2.5 % of H), on the grid, mid-tone weighted
    Texture   a difference of gaussians (0.05 % and 0.4 % of H), gated by the code-value noise floor
    Dehaze    a user-set veil A, a soft dark channel and a transmission refined by a joint guided filter

Discrete kernels are fixed so that CPU, Metal and this model sum the same terms in the same order:
tent reduction with edge replication, box filters with a symmetric window that shrinks at the
borders, gaussians truncated at 3 sigma and renormalised at the borders, bilinear upsampling on
pixel centres.
"""
import math

from . import color
from . import tone as T
from .tone import dm

GRID_BASE = 540            # working height: H / s in [0.707, 1.414) * GRID_BASE
ETA = 1e-12
PSI = 0.25                 # Texture soft limit, EV
C_GREY = 261.5 * math.log10(2.0) * 0.18 / 0.19   # S-Log3 code values per stop at grey
FLOOR_EV, FLOOR_W = -9.5, 3.0
CLARITY_W = 3.0
HAZE_T0 = 1e6 / 153.8
HAZE_MIN_T = 0.4
HAZE_G0 = 2.0 ** -6
HL_START = -0.5            # Local Highlights starts half a stop under grey
HL_GAMMA = 0.6             # where the base is compressed, the kept detail is raised by GAMMA * (1 - T')
HL_T = (0.7, 0.6)          # x edgeThreshold: detail kept whole up to T1, saturating by T1 + T2 (edges)
HL_B = (0.3, 0.2)          # x edgeThreshold: the same, smaller, for the boost

DETAIL_CONTROLS = {
    'localContrast': (-100.0, 50.0, 0.0), 'localHighlights': (-100.0, 100.0, 0.0),
    'localShadows': (-100.0, 100.0, 0.0), 'texture': (-100.0, 100.0, 0.0), 'clarity': (-100.0, 100.0, 0.0),
    'dehaze': (-100.0, 100.0, 0.0), 'localZonePivot': (-4.0, 4.0, 0.0),
    'preserveDetail': (0.0, 100.0, 100.0), 'detailRadius': (1.0, 8.0, 4.0), 'edgeThreshold': (0.25, 1.0, 0.5),
    'noiseThreshold': (0.0, 0.15, 0.04), 'clarityCenter': (-3.0, 3.0, 0.0), 'hazeLevel': (-2.0, 6.0, 2.0),
    'hazeWarmth': (-100.0, 100.0, 0.0), 'localWhite': (1.0, 10.0, 2.5),
    'viewGain': (0.0, 1.0, 0.0), 'viewBase': (0.0, 1.0, 0.0),
    # development mode and destination (the transform after the node)
    'viewZones': (0.0, 2.0, 0.0),
    'devMode': (0.0, 2.0, 0.0), 'destination': (0.0, 4.0, 0.0), 'subject': (0.0, 1.0, 0.0),
    # Cinematografico: the two points read on the picture (L of the norm) and their ratio in stops
    'ratioLight': (-16.0, 16.0, 0.0), 'ratioShadow': (-16.0, 16.0, 0.0), 'ratioMeasured': (0.0, 16.0, 0.0),
    'ratioTarget': (0.0, 5.0, 1.0), 'ratioHardness': (0.0, 100.0, 50.0), 'ratioBackground': (0.0, 100.0, 0.0),
    'ratioSubjectLift': (0.0, 100.0, 0.0),
    # Zonale: the shadow and the highlight read on the picture (L of the norm), the zones they are placed on
    'adamsShadowL': (-16.0, 16.0, 0.0), 'adamsHighL': (-16.0, 16.0, 0.0),
    'adamsShadowPlace': (0.0, 10.0, 3.0), 'adamsHighPlace': (0.0, 10.0, 8.0),
    # Zonale in phases: one node (all), 1 development, 2 print, 3 presence; the print's edge burn
    'zonalPhase': (0.0, 3.0, 0.0), 'printEdgeBurn': (0.0, 2.0, 0.0), 'printEdgeSize': (10.0, 60.0, 30.0)}
for _z, _r, _f in zip(T.ZONES, T.ZONE_RANGE, T.ZONE_FALLOFF):
    DETAIL_CONTROLS['localZone%sExp' % _z] = (-6.0, 6.0, 0.0)
    DETAIL_CONTROLS['localZone%sRange' % _z] = (-10.0, 10.0, _r)
    DETAIL_CONTROLS['localZone%sFalloff' % _z] = (0.5, 6.0, _f)

MODE_STANDARD, MODE_CINEMA, MODE_ZONAL = 0, 1, 2
ADAMS_MIN_SPREAD = 0.25           # the highlight stays at least this many zones above the shadow
PHASE_ALL, PHASE_DEVELOP, PHASE_PRINT, PHASE_PRESENCE = 0, 1, 2, 3
ZD_K_MIN, ZD_K_MAX = 0.25, 4.0    # development slope: about N-2.5 to N+5 on a 3-stop scene
ZD_ROOM = 1.0                     # toe and shoulder approach a stop beyond zone 0 and zone X
# Destinations: manual (localWhite), Rec.709 CST, Rec.709 DRT, PQ 1000, scene (DaVinci WG/Intermediate, no ODT yet).
# White and black in stops from grey, zone X and zone 0: Rec.709 CST 2.5 + 4.5 = 7 stops.
DEST_WHITE = (None, 2.5, 4.5, 6.5, 9.0)
DEST_BLACK = (4.5, 4.5, 6.0, 7.0, 9.0)
ZONE_RGB = ((0.25, 0.00, 0.35), (0.10, 0.10, 0.55), (0.10, 0.35, 0.85), (0.00, 0.60, 0.70), (0.25, 0.65, 0.30),
            (0.50, 0.50, 0.50), (0.95, 0.65, 0.55), (0.95, 0.85, 0.25), (1.00, 0.60, 0.10), (0.95, 0.20, 0.10),
            (1.00, 1.00, 1.00))
SUBJECT_RADIUS = (0.75, 2.0)        # Cinematografico: base radius x this, Viso / Paesaggio
RATIO_MIN_GAP = 0.05                # below this the two points are not a light and a shadow
RATIO_E_MAX = 6.0
RATIO_STEPS = 60


# ---------------------------------------------------------------- host side

def grid_scale(H, base=GRID_BASE):
    """n with 2 H^2 >= base^2 4^n: s = 2^n puts H / s in [0.707, 1.414) * base, integers only."""
    n = 0
    while 2 * H * H >= base * base * 4 ** (n + 1):
        n += 1
    return n


def tent_variance(s):
    return (2.0 * s * s + 1.0) / 12.0 if s >= 2 else 0.0


KERNEL_CAP = {'gG': 4, 'g1': 16, 'gT': 32}   # half-kernel capacities of DetailParams


def gauss_half(sigma, cap):
    R = min(int(math.ceil(3.0 * sigma)), cap - 1)
    return [math.exp(-k * k / (2.0 * sigma * sigma)) for k in range(R + 1)]


def veil_colour(warmth, space):
    """The veil's colour in the input gamut: a neutral seen under a light of 10^6/(153.8 + 0.8 v) K."""
    t = 1e6 / (153.8 + 0.8 * warmth)
    w, w0 = dm.white_xyz(t, 0.0), dm.white_xyz(HAZE_T0, 0.0)
    ls, l0 = dm.mul(dm.BR, w), dm.mul(dm.BR, w0)
    lms = dm.mul(dm.BR, [0.95046, 1.0, 1.08906])
    xyz = dm.mul(dm.BRI, [lms[i] * ls[i] / l0[i] for i in range(3)])
    rgb = [max(c, 0.02) for c in dm.mul(dm.MATS[space][1], xyz)]
    n = max(T.t5_norm(rgb), 1e-3)
    return [c / n for c in rgb]


def controls_clamped(controls):
    unknown = set(controls) - set(DETAIL_CONTROLS)
    if unknown:
        raise KeyError('not a detail control: %s' % ', '.join(sorted(unknown)))
    return {n: min(max(float(controls.get(n, d)), lo), hi) for n, (lo, hi, d) in DETAIL_CONTROLS.items()}


def destination_white(v):
    """The level the recorded top lands on: the node's own Bianco, or the destination's."""
    d = int(round(v['destination']))
    return v['localWhite'] if d == 0 else DEST_WHITE[d]


def destination_black(v):
    return DEST_BLACK[int(round(v['destination']))]


def zone_of(L, white, black):
    """dt_zone_of: zone V grey, zone 0 the destination's black, zone X its white, five zones on each side."""
    return 5.0 + 5.0 * L / black if L <= 0.0 else 5.0 + 5.0 * L / white


def zone_L(z, white, black):
    return (z - 5.0) * black / 5.0 if z <= 5.0 else (z - 5.0) * white / 5.0


def view_zones_colour(Lout, white, black, grey):
    z = int(min(max(f32(zone_of(f32(Lout), f32(white), f32(black))) + 0.5, 0.0), 10.0))
    return [0.1 * z] * 3 if grey else list(ZONE_RGB[z])


def f32(x):
    """The plugin computes the zone in float32: the band edges follow it."""
    import struct
    return struct.unpack('f', struct.pack('f', x))[0]


def cinema_nominal(v):
    """Cinematografico: slots of the zone curve that bring the shadow side of the subject to the target ratio.

    Slot 4 (down) starts at the light and lowers or raises what is under it; its E is solved so that
    G(shadow) - G(light) = measured - target, with the other slots in place: the light does not move
    (the slot is 0 there), the shadow lands on the ratio. Slot 1 (up) brightens the subject, slot 6
    (down) takes the background under the shadow down. Returns the 7 nominal slots."""
    nominal = [(0.0, 0.0, 1.0, T.SM_T5_SMAX_CR)] * T.SM_T5_SLOTS
    Ll, Ls = v['ratioLight'], v['ratioShadow']
    gap = Ll - Ls
    if gap <= RATIO_MIN_GAP:
        return nominal
    lift = v['ratioSubjectLift'] / 100.0
    if lift > 0.0:
        nominal[1] = (lift, Ll - 0.5, 1.0, T.SM_T5_SMAX_CR)
    push = max(v['ratioTarget'] - v['ratioMeasured'], 0.0)
    bg = v['ratioBackground'] / 100.0
    if bg > 0.0:
        nominal[6] = (-4.0 * bg, Ls - push - 0.25, 1.5, T.SM_T5_SMAX_ZONE)
    f = max(0.25, gap * (1.0 - 0.0075 * v['ratioHardness']))
    want = v['ratioMeasured'] - v['ratioTarget']

    def spread(E):
        n = list(nominal)
        n[4] = (E, Ll, f, T.SM_T5_SMAX_ZONE)
        c = T.zone_policy(0.0, 0.0, n)
        return T.t5_curve(Ls, c)[0] - T.t5_curve(Ll, c)[0]
    lo, hi = -RATIO_E_MAX, RATIO_E_MAX
    if spread(lo) >= want:
        E = lo
    elif spread(hi) <= want:
        E = hi
    else:
        for _ in range(RATIO_STEPS):
            mid = 0.5 * (lo + hi)
            if spread(mid) < want:
                lo = mid
            else:
                hi = mid
        E = 0.5 * (lo + hi)
    nominal[4] = (E, Ll, f, T.SM_T5_SMAX_ZONE)
    return nominal


def zonal_develop(v):
    """Zonale, development: one curve on the pixel's own L, as a negative is developed (no neighbours, no halo).

    Exposure places the measured shadow on its zone; the slope k (N+/N-) turns the line about that shadow so the
    measured highlight lands on its zone. Under the shadow a toe, over the highlight a shoulder: exponential, with
    the line's slope where they start, towards a stop beyond zone 0 and zone X. None without a measure."""
    Ls, Lh = v['adamsShadowL'], v['adamsHighL']
    gap = Lh - Ls
    if gap <= RATIO_MIN_GAP:
        return None
    zs = v['adamsShadowPlace']
    zh = max(v['adamsHighPlace'], zs + ADAMS_MIN_SPREAD)
    white, black = destination_white(v), destination_black(v)
    Lso, Lho = zone_L(zs, white, black), zone_L(zh, white, black)
    k = min(max((Lho - Lso) / gap, ZD_K_MIN), ZD_K_MAX)
    hi = Lso + k * gap
    return {'Ls': Ls, 'Lso': Lso, 'k': k, 'hi': hi,
            'hiD': max(white + ZD_ROOM - hi, 0.25), 'loD': max(Lso + black + ZD_ROOM, 0.25)}


def zonal_gain(L, zd):
    """dt_zonal_gain: the development's log2 gain at L."""
    y = zd['Lso'] + zd['k'] * (L - zd['Ls'])
    if y > zd['hi']:
        d = zd['hiD']
        y = zd['hi'] + d * (1.0 - math.exp(-(y - zd['hi']) / d))
    elif y < zd['Lso']:
        d = zd['loD']
        y = zd['Lso'] - d * (1.0 - math.exp(-(zd['Lso'] - y) / d))
    return y - L


def zonal_reached(v):
    """The zones the shadow and the highlight land on: exact, unless the slope hits its limit."""
    zd = zonal_develop(v)
    white, black = destination_white(v), destination_black(v)
    if zd is None:
        return tuple(zone_of(L, white, black) for L in (v['adamsShadowL'], v['adamsHighL']))
    return zone_of(zd['Lso'], white, black), zone_of(zd['hi'], white, black)


def edge_burn(X, Y, W, H, p):
    """dt_edge_burn: the print's burned edges, log2 gain. A superellipse distance, 1 at the middle of each side,
    the burn growing smoothly over the outer printEdgeSize of it."""
    u = 2.0 * (X + 0.5) / W - 1.0
    w = 2.0 * (Y + 0.5) / H - 1.0
    r = math.sqrt(math.sqrt(u ** 4 + w ** 4))
    t = min(max((r - p['ebR0']) / p['ebS'], 0.0), 1.0)
    return -p['ebAmt'] * t * t * (3.0 - 2.0 * t)


def cinema_reached(v):
    """The ratio in stops the slots give between the two points (= target unless it is out of reach)."""
    c = T.zone_policy(0.0, 0.0, cinema_nominal(v))
    return v['ratioMeasured'] + T.t5_curve(v['ratioLight'], c)[0] - T.t5_curve(v['ratioShadow'], c)[0]


def prepare(W, H, controls, space=8, par=1.0, base=GRID_BASE, href=None):
    """Everything the kernels read, from the frame size and the panel (= dt_prepare).
    href: the height the radii are relative to, H in the plugin; tests set it to make a small image
    behave like a full frame."""
    v = controls_clamped(controls)
    href = float(href or H)
    n = grid_scale(H, base)
    s = 2 ** n
    gh = href / float(s)
    sigma1 = max(0.5, 0.0005 * href)
    sigma2 = 0.004 * href
    nt = min(max(int(math.floor(math.log2(sigma2 / 4.0))), 0), n) if sigma2 > 0 else 0
    st = 2 ** nt
    sigma_t = math.sqrt(max(sigma2 * sigma2 - tent_variance(st), 0.25)) / st

    mode = int(round(v['devMode']))
    cinema = mode == MODE_CINEMA
    radius = v['detailRadius'] * (SUBJECT_RADIUS[int(round(v['subject']))] if cinema else 1.0)
    zonal = mode == MODE_ZONAL
    phase = int(round(v['zonalPhase'])) if zonal else PHASE_ALL
    develop, printing, presence = (phase in (PHASE_ALL, ph) for ph in (PHASE_DEVELOP, PHASE_PRINT, PHASE_PRESENCE))
    if not presence:
        v['texture'] = v['clarity'] = v['dehaze'] = 0.0
    if not printing:
        v['localContrast'] = v['localShadows'] = v['localHighlights'] = 0.0
    zd = zonal_develop(v) if zonal and develop else None
    if cinema:
        curve = T.zone_policy(0.0, 0.0, cinema_nominal(v))
    else:
        nominal = [(0.0, 0.0, 1.0, T.SM_T5_SMAX_CR)] * T.SM_T5_SLOTS
        nominal[5] = (0.02 * v['localShadows'], 0.0, 1.5, T.SM_T5_SMAX_CR)
        for z, slot in zip(T.ZONES, T.ZONE_SLOT):   # Zonale prints with the three local tones alone
            nominal[slot] = (0.0 if zonal else v['localZone%sExp' % z], v['localZone%sRange' % z],
                             v['localZone%sFalloff' % z], T.SM_T5_SMAX_ZONE)
        curve = T.zone_policy(2.0 ** (v['localContrast'] / 100.0) - 1.0, 0.0 if zonal else v['localZonePivot'],
                              nominal)

    haze_v = v['dehaze']
    veil = veil_colour(v['hazeWarmth'], space)
    A = [T.SM_T5_GREY * 2.0 ** v['hazeLevel'] * c for c in veil]
    p = {
        'W': W, 'H': H, 's': s, 'w': -(-W // s), 'h': -(-H // s), 'st': st, 'wt': -(-W // st), 'ht': -(-H // st),
        'rB': radius / 100.0 * gh, 'r2': 0.005 * gh, 'r3': 0.025 * gh, 'rp': 0.005 * gh, 'rt': 0.02 * gh,
        'par': par, 'eps': (v['edgeThreshold'] / 2.0) ** 2, 'eps_t': 0.01,
        'gG': gauss_half(1.0, 4), 'g1': gauss_half(sigma1, 16), 'gT': gauss_half(sigma_t, 32),
        'curve': curve, 'lam': v['preserveDetail'] / 100.0, 'tau0': v['noiseThreshold'],
        'clarity': v['clarity'] / 100.0, 'clarityCenter': v['clarityCenter'], 'texture': v['texture'] / 100.0,
        'hazeOn': int(haze_v != 0.0), 'hazeOmega': 0.006 * haze_v if haze_v > 0 else 0.0,
        'hazeMix': 0.5 * (haze_v / 100.0) ** 2 if haze_v < 0 else 0.0,
        'hazeA': A, 'hazeInvA': [1.0 / a for a in A], 'hazeLevel': v['hazeLevel'],
        'view': 1 if v['viewGain'] else (2 if v['viewBase'] else (2 + int(round(v['viewZones'])) if v['viewZones'] else 0)),
        'zoneWhite': destination_white(v), 'zoneBlack': destination_black(v),
        'space': space, 'zd': zd,
        'ebAmt': v['printEdgeBurn'] if zonal and printing else 0.0,
        'ebR0': 1.0 - v['printEdgeSize'] / 100.0, 'ebS': v['printEdgeSize'] / 100.0,
    }
    p['toneOn'] = int(curve['conC'] != 0.0 or any(E != 0.0 for E in curve['zoneE']))
    # Local Highlights: the main node's shoulder, landing the recorded top on this node's Bianco at -100
    hv = 0.0 if cinema else v['localHighlights']
    white = destination_white(v)
    a = abs(hv) / 100.0
    a *= 1.5 - 0.5 * a
    p['hlE'], p['hlStart'] = 0.0, HL_START
    p['hlT'] = tuple(k * v['edgeThreshold'] for k in HL_T)
    p['hlB'] = tuple(k * v['edgeThreshold'] for k in HL_B)
    if hv > 0.0:
        p['hlE'] = a * T.SM_T5_HL_CPLUS
    p['hlAmount'], p['hlWhiteLin'], p['hlTop'] = 0.0, 0.0, T.SM_T5_HL_TOP
    if hv < 0.0:
        top = T.SM_T5_HL_TOP
        land = min(white, top - T.SM_T5_HL_MINPULL)
        D = T.t5_shoulder(top, {'hlE': 1.0, 'hlStart': HL_START})[0]
        k = min(max(top - land, 0.0) / D, 1.0 - T.SM_T5_HL_SMIN) if D > 0.0 else 0.0
        p['hlE'] = -a * k
        p['hlAmount'] = a
        p['hlWhiteLin'] = T.SM_T5_GREY * 2.0 ** max(white, top - k * D)
    return p


# ---------------------------------------------------------------- discrete kernels (1D, then 2D)

def tent_1d(row, s, n_out):
    if s == 1:
        return list(row)
    n = len(row)
    out = []
    for i in range(n_out):
        c = (i + 0.5) * s - 0.5
        acc = wsum = 0.0
        for x in range(int(math.floor(c - s)) + 1, int(math.ceil(c + s))):
            w = s - abs(x - c)
            if w <= 0.0:
                continue
            acc += w * row[min(max(x, 0), n - 1)]
            wsum += w
        out.append(acc / wsum)
    return out


def box_1d(row, r):
    n = len(row)
    out = []
    for i in range(n):
        rr = min(r, float(i), float(n - 1 - i))
        k = int(math.floor(rr))
        f = rr - k
        acc = 0.0
        for j in range(i - k, i + k + 1):
            acc += row[j]
        if f > 0.0:
            acc += f * (row[i - k - 1] + row[i + k + 1])
        out.append(acc / (2.0 * rr + 1.0))
    return out


def gauss_1d(row, half):
    n, R = len(row), len(half) - 1
    out = []
    for i in range(n):
        acc = wsum = 0.0
        for k in range(-R, R + 1):
            j = i + k
            if 0 <= j < n:
                w = half[abs(k)]
                acc += w * row[j]
                wsum += w
        out.append(acc / wsum)
    return out


def rows(img, W, H):
    return [img[y * W:(y + 1) * W] for y in range(H)]


def separable(img, W, H, fx, fy, W2=None, H2=None):
    """fx on every row, then fy on every column; fx may change the width to W2, fy the height to H2."""
    W2, H2 = W2 or W, H2 or H
    tmp = [fx(r) for r in rows(img, W, H)]                      # H rows of W2
    cols = [fy([tmp[y][x] for y in range(H)]) for x in range(W2)]  # W2 columns of H2
    return [cols[x][y] for y in range(H2) for x in range(W2)]


def tent(img, W, H, s):
    w, h = -(-W // s), -(-H // s)
    return separable(img, W, H, lambda r: tent_1d(r, s, w), lambda c: tent_1d(c, s, h), w, h)


def box(img, W, H, r, par=1.0):
    return separable(img, W, H, lambda row: box_1d(row, r / par), lambda col: box_1d(col, r))


def gauss(img, W, H, half):
    return separable(img, W, H, lambda row: gauss_1d(row, half), lambda col: gauss_1d(col, half))


def bilinear(grid, w, h, s, x, y):
    u = min(max((x + 0.5) / s - 0.5, 0.0), w - 1.0)
    v = min(max((y + 0.5) / s - 0.5, 0.0), h - 1.0)
    i0, j0 = int(math.floor(u)), int(math.floor(v))
    i1, j1 = min(i0 + 1, w - 1), min(j0 + 1, h - 1)
    fx, fy = u - i0, v - j0
    top = grid[j0 * w + i0] + (grid[j0 * w + i1] - grid[j0 * w + i0]) * fx
    bot = grid[j1 * w + i0] + (grid[j1 * w + i1] - grid[j1 * w + i0]) * fx
    return top + (bot - top) * fy


def guided_self(I, w, h, r, eps, par):
    mu = box(I, w, h, r, par)
    nu = box([v * v for v in I], w, h, r, par)
    a = [max(q - m * m, 0.0) / (max(q - m * m, 0.0) + eps) for m, q in zip(mu, nu)]
    b = [m * (1.0 - ai) for m, ai in zip(mu, a)]
    return box(a, w, h, r, par), box(b, w, h, r, par)


def guided_joint(I, p, w, h, r, eps, par):
    mI, mp = box(I, w, h, r, par), box(p, w, h, r, par)
    mIp = box([x * y for x, y in zip(I, p)], w, h, r, par)
    mII = box([x * x for x in I], w, h, r, par)
    a, b = [], []
    for i in range(len(I)):
        var = max(mII[i] - mI[i] * mI[i], 0.0)
        ai = (mIp[i] - mI[i] * mp[i]) / (var + eps)
        a.append(ai)
        b.append(mp[i] - ai * mI[i])
    return box(a, w, h, r, par), box(b, w, h, r, par)


# ---------------------------------------------------------------- per pixel

def luma(rgb, space):
    """L of an input pixel (linear, input gamut), clamped so a NaN is -16 and an Inf +16 as fmax/fmin do."""
    n = T.t5_norm(dm.mul(dm.MATS[T.SM_T5_REF][1], dm.mul(dm.MATS[space][0], rgb)))
    e = T.t5_log(n)
    if e != e:
        return -16.0
    return min(max(e, -16.0), 16.0)


def tau(B, tau0):
    """The noise floor in EV at log2 luminance B: tau0 at grey, larger where S-Log3 has fewer codes per stop."""
    N = T.SM_T5_GREY * 2.0 ** B
    c = 261.5 * math.log10(2.0) * N / (N + 0.01) if N >= 0.01125 else math.log(2.0) * N * 76.2102946929 / 0.01125
    return min(1.0, tau0 * C_GREY / c)


def shoulder(t, E, start):
    """sm_t5_shoulder_at: (T - t, E * D'), the shoulder and its slope minus 1."""
    u = t - start
    if not u > 0.0 or E == 0.0:
        return 0.0, 0.0
    f = T.SM_T5_HL_FILL
    if u < f:
        v = u / f
        ps, w = f * (v * v * v - 0.5 * v * v * v * v), v * v * (3.0 - 2.0 * v)
    else:
        ps, w = u - 0.5 * f, 1.0
    y = T.SM_T5_HL_RATE * (ps - T.SM_T5_HL_MID)
    q = 2.0 ** -abs(y)
    return E * (T.t5_softplus2(y) - T.SM_T5_HL_SP0) / T.SM_T5_HL_RATE, E * (1.0 / (1.0 + q) if y >= 0.0 else q / (1.0 + q)) * w


def lim(x, t1, t2):
    """dt_lim: x up to t1, then saturating smoothly to t1 + t2."""
    return x if abs(x) <= t1 else math.copysign(t1 + t2 * math.tanh((abs(x) - t1) / t2), x)


def local_highlights(L, Bl, p, taper=False):
    """log2 gain of Local Highlights: the shoulder on the base with the detail split off, the detail raised where the
    base is compressed (above the noise, by a smaller amount on edges), never brighter than it came in. Near the
    recorded top the base is the sensor clip: the detail fades out there. taper: also the white taper's weight."""
    keep = 1.0 - T.t5_ramp((Bl - (p['hlTop'] - 1.5)) / 1.5)
    Dt = keep * lim(L - Bl, *p['hlT'])
    u = L - Dt - p['hlStart']
    d, s = shoulder(L - Dt, p['hlE'], p['hlStart'])
    w = 0.0
    if p['hlE'] < 0.0:
        Db = keep * lim(L - Bl, *p['hlB'])
        tn = 3.0 * tau(L - Dt, p['tau0'])
        d = min(d - HL_GAMMA * s * Db * Db * Db / (Db * Db + tn * tn + ETA), 0.0)
        f = T.SM_T5_HL_FILL
        w = p['hlAmount'] * (0.0 if u <= 0.0 else (1.0 if u >= f else (u / f) ** 2 * (3.0 - 2.0 * u / f)))
    return (d, w) if taper else d


def soft1(g):
    a = abs(g)
    if a <= 1.0:
        return g
    return math.copysign(1.0 + 0.5 * math.tanh((a - 1.0) / 0.5), g)


def haze_pixel(x, A, invA, t, mix):
    """J from x: the dehazed pixel, or the veil added back (mix > 0) when Dehaze is negative."""
    if mix > 0.0:
        return [mix * a + (1.0 - mix) * c for a, c in zip(A, x)]
    out = []
    for c, a in zip(x, A):
        if c <= 0.0:
            out.append(c)
            continue
        g = 1.0 / t - (1.0 / t - 1.0) * a / c
        u = g / HAZE_G0
        out.append(c * HAZE_G0 * (u if u >= 2.0 else 1.0 + math.exp(u - 2.0)))
    return out


def dark_energy(x, invA):
    """Soft minimum of x/A over the channels: 1/3 sum exp(-clamp(x/A)/0.05)."""
    return sum(math.exp(-min(max(c * ia, 0.0), 1.5) / 0.05) for c, ia in zip(x, invA)) / 3.0


def view_gain_colour(G, L):
    """viewGain: monochrome picture where |G| < 0.1, then blue (lower) or orange (raise) bands."""
    a = abs(G)
    if a < 0.1:
        m = min(max((L + 6.0) / 11.5, 0.06), 0.92)
        return [m, m, m]
    band = 0 if a < 0.25 else 1 if a < 0.5 else 2 if a < 1.0 else 3
    blue = ((0.62, 0.78, 1.00), (0.35, 0.58, 1.00), (0.15, 0.35, 0.95), (0.08, 0.12, 0.70))
    orange = ((1.00, 0.85, 0.60), (1.00, 0.68, 0.30), (1.00, 0.50, 0.10), (0.85, 0.25, 0.05))
    return list((blue if G < 0 else orange)[band])


# ---------------------------------------------------------------- the node

def render(img, W, H, p, gamma):
    """img: W*H input pixels (3 channels, encoded with transfer `gamma`, gamut p['space']).
    Returns (out pixels encoded like the input, planes {L, B, Dg, G} for the parity tests)."""
    space, s, w, h = p['space'], p['s'], p['w'], p['h']
    lin = [[dm.decode1(c, gamma) for c in px] for px in img]
    finite = [all(math.isfinite(c) for c in px) for px in lin]
    x = [px if ok else [0.0, 0.0, 0.0] for px, ok in zip(lin, finite)]
    L0 = [luma(px, space) for px in x]
    J = x

    if p['hazeOn']:
        invA = p['hazeInvA']
        if p['hazeMix'] > 0.0:
            J = [haze_pixel(px, p['hazeA'], invA, 1.0, p['hazeMix']) for px in x]
        else:
            ch = [tent([px[c] * invA[c] for px in x], W, H, s) for c in range(3)]
            L0w = tent(L0, W, H, s)
            E = [sum(math.exp(-min(max(ch[c][i], 0.0), 1.5) / 0.05) for c in range(3)) / 3.0
                 for i in range(w * h)]
            Eb = box(E, w, h, p['rp'], p['par'])
            D = [max(-0.05 * math.log(max(e, 1e-30)), 0.0) for e in Eb]
            # centred on the veil level and on t = 1 (where t_raw mostly is): the same filter, but the
            # float32 sums of the plugin no longer cancel the small covariance away
            guide = [l - p['hazeLevel'] for l in L0w]
            tc = [-p['hazeOmega'] * min(d, 1.0) * (1.0 - T.t5_ramp(g)) for d, g in zip(D, guide)]
            at, bt = guided_joint(guide, tc, w, h, p['rt'], p['eps_t'], p['par'])
            J = []
            for i, px in enumerate(x):
                X, Y = i % W, i // W
                t = bilinear(at, w, h, s, X, Y) * (L0[i] - p['hazeLevel']) + bilinear(bt, w, h, s, X, Y) + 1.0
                t = min(max(t, HAZE_MIN_T), 1.0)
                J.append(haze_pixel(px, p['hazeA'], invA, t, 0.0))
    L = [luma(px, space) for px in J] if p['hazeOn'] else L0

    Lw = tent(L, W, H, s)
    aB, bB = guided_self(Lw, w, h, p['rB'], p['eps'], p['par'])
    Dg = [0.0] * (w * h)
    if p['clarity'] != 0.0:
        a2, b2 = guided_self(Lw, w, h, p['r2'], p['eps'], p['par'])
        a3, b3 = guided_self(Lw, w, h, p['r3'], p['eps'], p['par'])
        Dg = [(a2[i] - a3[i]) * Lw[i] + (b2[i] - b3[i]) for i in range(w * h)]
    Gg = gauss(L, W, H, p['gG'])
    if p['texture'] != 0.0:
        G1 = gauss(L, W, H, p['g1'])
        st = p['st']
        Lt = tent(L, W, H, st)
        G2 = gauss(Lt, p['wt'], p['ht'], p['gT'])
    out, planes = [], {'L': L, 'B': [], 'Dg': [], 'G': []}
    for i in range(W * H):
        X, Y = i % W, i // W
        Lu = bilinear(Lw, w, h, s, X, Y)
        tg = 2.0 * tau(Lu, p['tau0'])
        dg = L[i] - Gg[i]
        Lg = L[i] - dg * tg * tg / (dg * dg + tg * tg + ETA)
        B = bilinear(aB, w, h, s, X, Y) * Lg + bilinear(bB, w, h, s, X, Y)
        Bl = p['lam'] * B + (1.0 - p['lam']) * L[i]
        Gb = T.t5_curve(Bl, p['curve'])[0] if p['toneOn'] else 0.0
        if Gb > 0.0:
            Gb *= T.t5_ramp((Bl - FLOOR_EV) / FLOOR_W)
        H = W_hl = 0.0
        if p['hlE'] != 0.0:
            gh, W_hl = local_highlights(L[i], Bl, p, taper=True)
            Gb += gh
            if p['hlE'] < 0.0:
                H = gh
        K = 0.0
        if p['clarity'] != 0.0:
            K = p['clarity'] * math.exp(-((B - p['clarityCenter']) / CLARITY_W) ** 2) * bilinear(Dg, w, h, s, X, Y)
        Tx = 0.0
        if p['texture'] != 0.0:
            DT = G1[i] - bilinear(G2, p['wt'], p['ht'], p['st'], X, Y)
            ta = tau(B, p['tau0'])
            gate = DT * DT / (DT * DT + ta * ta + ETA) if p['texture'] > 0.0 else 1.0
            Tx = p['texture'] * DT / (1.0 + (DT / PSI) ** 2) * gate
        G = Gb + soft1(K + Tx)
        if p['zd'] is not None:
            Gz = zonal_gain(L[i], p['zd'])
            if Gz > 0.0:
                Gz *= T.t5_ramp((L[i] - FLOOR_EV) / FLOOR_W)
            G += Gz
        if p['ebAmt'] != 0.0:
            G += edge_burn(X, Y, p['W'], p['H'], p)
        planes['B'].append(B)
        planes['Dg'].append(bilinear(Dg, w, h, s, X, Y))
        planes['G'].append(G)
        if not finite[i]:
            out.append(list(img[i]))
        elif p['view'] == 1:
            out.append(emit_display(view_gain_colour(G, L[i]), space, gamma))
        elif p['view'] == 2:
            v = T.SM_T5_GREY * 2.0 ** B
            out.append([dm.encode1(v, gamma)] * 3)
        elif p['view'] >= 3:
            out.append(emit_display(view_zones_colour(L[i] + G, p['zoneWhite'], p['zoneBlack'], p['view'] == 4),
                                    space, gamma))
        elif G == 0.0 and not p['hazeOn']:
            out.append(list(img[i]))
        else:
            g = 2.0 ** G
            lin = [c * g for c in J[i]]
            if H < 0.0:
                xyz = color.t5_hl_white(dm.mul(dm.MATS[space][0], lin), H, W_hl, p['hlWhiteLin'], 1, space)
                lin = dm.mul(dm.MATS[space][1], xyz)
            out.append([dm.encode1(c, gamma) for c in lin])
    return out, planes


def emit_display(c, space, gamma):
    """A display colour (Rec.709, gamma 2.2) re-encoded in the node's space, as the false colours do."""
    lin = [dm.decode1(v, 2) for v in c]
    return [dm.encode1(v, gamma) for v in dm.mul(dm.MATS[space][1], dm.mul(dm.MATS[1][0], lin))]

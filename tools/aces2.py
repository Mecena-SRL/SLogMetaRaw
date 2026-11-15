# SPDX-License-Identifier: GPL-3.0-or-later
"""ACES 2.0 Output Transform, SDR 100 nit, Rec.709 limiting gamut, D65 white: Python reference.

Port of OpenColorIO 2.6 (src/OpenColorIO/ops/fixedfunction/ACES2, builtin 'ACES-OUTPUT -
ACES2065-1_to_CIE-XYZ-D65 - SDR-100nit-REC709_2.0'), in double. The parameters and the tables OCIO
builds at init (reach M, hue samples, limiting cusp, upper hull gamma) are fixed here and computed once:
tools/build_math.py writes them into gen/Aces2Tables.h for math/Display.h, tests/model/display.py
runs output_transform(). Hue in degrees.
"""
import math

from gamut_matrices import inv, mm, mv, rgb2xyz

AP0 = ((0.7347, 0.2653), (0.0, 1.0), (0.0001, -0.0770), (0.32168, 0.33767))
AP1 = ((0.713, 0.293), (0.165, 0.830), (0.128, 0.044), (0.32168, 0.33767))
REC709 = ((0.64, 0.33), (0.30, 0.60), (0.15, 0.06), (0.3127, 0.3290))
CAM16 = ((0.8336, 0.1735), (2.3854, -1.4659), (0.087, -0.125), (0.333, 0.333))

PEAK = 100.0                       # nit; 1.0 out = 100 nit
L_A, Y_B, SURROUND_C, SURROUND_NC = 100.0, 20.0, 0.59, 0.9   # dim surround
NL_OFFSET = 0.2713 * 100.0
CZ = SURROUND_C * (1.48 + math.sqrt(Y_B / 100.0))           # model gamma
SMOOTH_CUSPS, SMOOTH_M, CUSP_MID_BLEND, FOCUS_GAIN_BLEND = 0.12, 0.27, 1.3, 0.3
FOCUS_DIST, COMPRESSION_THRESHOLD = 1.35, 0.75
LOWER_HULL_GAMMA_INV = 1.0 / 1.14
AP0_TO_AP1 = mm(inv(rgb2xyz(*AP1)), rgb2xyz(*AP0))
AP1_TO_AP0 = inv(AP0_TO_AP1)
N_HUES = 360                       # tables: [0] wraps the last, [361], [362] the first two


def lerp(a, b, t):
    return (b - a) * t + a


def _nl_fwd(v):                    # post-adaptation cone response compression
    f = abs(v) ** 0.42
    return math.copysign(f / (NL_OFFSET + f), v)


def _nl_inv(v):
    r = min(abs(v), 0.99)
    return math.copysign((NL_OFFSET * r / (1.0 - r)) ** (1 / 0.42), v)


class JMh:
    """Hellwig 2022 JMh for an RGB space, matrices prescaled as OCIO's init_JMhParams."""

    def __init__(self, prims):
        m16 = inv(rgb2xyz(*CAM16))
        to_xyz = rgb2xyz(*prims)
        xyz_w = mv(to_xyz, [100.0] * 3)
        rgb_w = mv(m16, xyz_w)
        k = 1.0 / (5.0 * L_A + 1.0)
        k4 = k ** 4
        self.f_l = 0.2 * k4 * (5.0 * L_A) + 0.1 * (1.0 - k4) ** 2 * (5.0 * L_A) ** (1 / 3)
        self.f_l_n = self.f_l / 100.0
        d = [self.f_l_n * xyz_w[1] / v for v in rgb_w]
        rgb_aw = [_nl_fwd(d[i] * rgb_w[i]) for i in range(3)]
        base = [[2.0, 1.0, 1.0 / 20.0], [1.0, -12.0 / 11.0, 1.0 / 11.0], [1 / 9, 1 / 9, -2 / 9]]
        cr = [[400.0 * v for v in r] for r in base]
        a_w = sum(cr[0][i] * rgb_aw[i] for i in range(3))
        self.a_w_j = _nl_fwd(self.f_l)
        m = mm(inv(rgb2xyz(*CAM16)), to_xyz)
        self.to_cam = [[d[i] * 100.0 * m[i][j] for j in range(3)] for i in range(3)]
        self.from_cam = inv(self.to_cam)
        self.to_aab = [[v / a_w for v in cr[0]], [v * 43.0 * SURROUND_NC for v in cr[1]],
                       [v * 43.0 * SURROUND_NC for v in cr[2]]]
        self.from_aab = inv(self.to_aab)

    def aab(self, rgb):
        return mv(self.to_aab, [_nl_fwd(v) for v in mv(self.to_cam, rgb)])

    def rgb(self, aab):
        return mv(self.from_cam, [_nl_inv(v) for v in mv(self.from_aab, aab)])

    def jmh(self, rgb):
        return aab_to_jmh(self.aab(rgb))

    def jmh_to_rgb(self, j, m, h):
        return self.rgb(jmh_to_aab(j, m, math.cos(math.radians(h)), math.sin(math.radians(h))))

    def y_to_j(self, y):
        ra = _nl_fwd(abs(y) * self.f_l_n)
        return math.copysign(100.0 * (ra / self.a_w_j) ** CZ, y)

    def a_to_y(self, a):
        return _nl_inv(self.a_w_j * a) / self.f_l_n


def aab_to_jmh(aab):
    a, x, y = aab
    if a <= 0.0:
        return [0.0, 0.0, 0.0]
    h = math.degrees(math.atan2(y, x))
    return [100.0 * a ** CZ, math.hypot(x, y), h + 360.0 if h < 0.0 else h]


def jmh_to_aab(j, m, cos_h, sin_h):
    return [(j / 100.0) ** (1 / CZ), m * cos_h, m * sin_h]


def tonescale_params():
    n_r, g, c, c_d, t_1, r_hit = 100.0, 1.15, 0.18, 10.013, 0.04, 128.0
    m_1 = 0.5 * (1.0 + math.sqrt(1.0 + 4.0 * t_1))
    m = m_1 / ((r_hit / m_1) / (r_hit / m_1 + 1.0)) ** g
    c_t = c_d / n_r
    g_ip = 0.5 * (c_t + math.sqrt(c_t * (c_t + 4.0 * t_1)))
    q = (g_ip / m) ** (1.0 / g)
    w_2 = c / (-m_1 * q / (q - 1.0))
    u_2 = ((r_hit / m_1) / (r_hit / m_1 + w_2)) ** g
    return dict(g=g, t_1=t_1, c_t=c_t, s_2=w_2 * m_1 * 100.0, m_2=m_1 / u_2, forward_limit=8.0 * r_hit)


def tonescale(y, ts):              # luminance in nit -> nit, 100 nit peak
    f = ts['m_2'] * (y / (y + ts['s_2'])) ** ts['g']
    return max(0.0, f * f / (f + ts['t_1'])) * 100.0


def chroma_norm(cos_h, sin_h, scale):
    c2, s2 = 2.0 * cos_h * cos_h - 1.0, 2.0 * cos_h * sin_h
    c3, s3 = 4.0 * cos_h ** 3 - 3.0 * cos_h, 3.0 * sin_h - 4.0 * sin_h ** 3
    return (11.34072 * cos_h + 16.46899 * c2 + 7.88380 * c3 + 14.66441 * sin_h - 6.37224 * s2
            + 9.19364 * s3 + 77.12896) * scale


def toe(x, limit, k1, k2):
    if x > limit:
        return x
    k2 = max(k2, 0.001)
    k1 = math.sqrt(k1 * k1 + k2 * k2)
    k3 = (limit + k1) / (limit + k2)
    b = k3 * x - k1
    return 0.5 * (b + math.sqrt(b * b + 4.0 * k2 * k3 * x))


def focus_gain(j, threshold, j_max, focus_dist):
    gain = j_max * focus_dist
    if j > threshold:
        adj = math.log10((j_max - threshold) / max(0.0001, j_max - j))
        gain *= adj * adj + 1.0
    return gain


def j_intersect(j, m, focus_j, j_max, slope_gain):
    ms = m / slope_gain
    a = ms / focus_j
    if j < focus_j:
        b = 1.0 - ms
        return 2.0 * j / (b + math.sqrt(b * b + 4.0 * a * j))
    b = -(1.0 + ms + j_max * a)
    c = j_max * ms + j
    return -2.0 * c / (b - math.sqrt(b * b - 4.0 * a * c))


def vector_slope(j_i, focus_j, j_max, slope_gain):
    return (j_i if j_i < focus_j else j_max - j_i) * (j_i - focus_j) / (focus_j * slope_gain)


def boundary_m(j_axis, slope, inv_gamma, j_max, m_max, j_ref):
    return j_ref * (j_axis / j_ref) ** inv_gamma * m_max / (j_max - slope * m_max)


def gamut_boundary(cusp_j, cusp_m, j_max, gamma_top_inv, j_i, slope, j_i_cusp):
    lo = boundary_m(j_i, slope, LOWER_HULL_GAMMA_INV, cusp_j, cusp_m, j_i_cusp)
    hi = boundary_m(j_max - j_i, -slope, gamma_top_inv, j_max - cusp_j, cusp_m, j_max - j_i_cusp)
    s = SMOOTH_CUSPS * cusp_m      # smooth minimum, cubic
    k = max(s - abs(lo - hi), 0.0) / s
    return min(lo, hi) - k * k * k * s / 6.0


def focus_j(cusp_j, mid_j, j_max):
    return lerp(cusp_j, mid_j, min(1.0, CUSP_MID_BLEND - cusp_j / j_max))


def _corners():                    # unit cube corners R, Y, G, C, B, M
    return [[float((i + 1) % 6 < 3), float((i + 5) % 6 < 3), float((i + 3) % 6 < 3)] for i in range(6)]


def _cycle(items, hue):
    """Rotate so the lowest hue is at [1], copy the ends to [0] and [7] with the hue wrapped."""
    k = min(range(6), key=lambda i: (hue(items[i]), i))
    r = [items[(i + k) % 6] for i in range(6)]
    return [r[5]] + r + [r[0]]


class Aces2:
    def __init__(self):
        self.p_in, self.p_out, reach = JMh(AP0), JMh(REC709), JMh(AP1)
        self.ts = tonescale_params()
        self.j_max = self.p_in.y_to_j(PEAK)
        self.mid_j = self.p_in.y_to_j(self.ts['c_t'] * 100.0)
        self.sat, self.sat_thr, self.compr = 1.3, 0.5 / PEAK, 2.4
        self.cc_scale = (0.03379 * PEAK) ** 0.30596 - 0.45135
        self.reach_m = self._reach_table(reach)
        self.hues, self.cusps = self._cusp_table(reach)
        self.search = self._search_range()
        self._upper_hull_gamma()

    def _reach_table(self, reach):
        t = []
        for h in range(N_HUES):
            low, high, outside = 0.0, 50.0, False
            while not outside and high < 1300.0:
                outside = min(reach.jmh_to_rgb(self.j_max, high, h)) < 0.0
                if not outside:
                    low, high = high, high + 50.0
            while high - low > 1e-2:
                m = (high + low) / 2.0
                if min(reach.jmh_to_rgb(self.j_max, m, h)) < 0.0:
                    high = m
                else:
                    low = m
            t.append(high)
        return [t[-1]] + t + t[:2]

    def _cusp_table(self, reach):
        limit_a = (self.j_max / 100.0) ** (1 / CZ)
        reach_c = []
        for c in _corners():
            lower, upper = 0.0, self.ts['forward_limit']
            while upper - lower > 1e-3:
                test = (lower + upper) / 2.0
                a = reach.aab([test * v for v in c])[0]
                if a < limit_a:
                    lower = test
                else:
                    upper = test
                if a == limit_a:
                    break
            reach_c.append(reach.jmh([upper * v for v in c]))
        reach_c = _cycle(reach_c, lambda v: v[2])
        lim = _cycle([(c, self.p_out.jmh(c)) for c in _corners()], lambda v: v[1][2])
        lim_rgb, lim_jmh = [c for c, _ in lim], [list(j) for _, j in lim]
        for cs in (reach_c, lim_jmh):
            cs[0], cs[7] = list(cs[0]), list(cs[7])
            cs[0][2] -= 360.0
            cs[7][2] += 360.0
        # merge the corner hues of reach and limiting gamut
        sorted_h, ri, di = [], 1, 1
        while ri < 7 or di < 7:
            rh, dh = reach_c[ri][2], lim_jmh[di][2]
            if rh == dh:
                sorted_h.append(rh)
                ri, di = ri + 1, di + 1
            elif rh < dh:
                sorted_h.append(rh)
                ri += 1
            else:
                sorted_h.append(dh)
                di += 1
        hues = self._hue_table(sorted_h)
        cusps = [None] * (N_HUES + 3)
        prev = [0.0, 0.0]
        for i in range(1, N_HUES + 1):
            j, m = self._display_cusp(hues[i], lim_rgb, lim_jmh, prev)
            cusps[i] = [j, m * (1.0 + SMOOTH_M * SMOOTH_CUSPS), hues[i]]
        cusps[0] = cusps[N_HUES][:2] + [hues[0]]
        cusps[N_HUES + 1] = cusps[1][:2] + [hues[N_HUES + 1]]
        cusps[N_HUES + 2] = cusps[2][:2] + [hues[N_HUES + 2]]
        return hues, cusps

    @staticmethod
    def _hue_table(sorted_h):
        counts, last, min_i = [0] * len(sorted_h), None, 0 if sorted_h[0] == 0.0 else 1
        for k, h in enumerate(sorted_h):
            idx = min(max(int(math.floor(h + 0.5)), min_i), N_HUES - 1)
            if idx == last:
                if k > 1 and counts[k - 2] != counts[k - 1] - 1:
                    counts[k - 1] -= 1
                else:
                    idx += 1
            counts[k] = min(idx, N_HUES - 1)
            last = min_i = idx
        t = [0.0] * (N_HUES + 3)
        bounds = [0.0] + sorted_h + [360.0]
        sizes = [counts[0]] + [counts[k] - counts[k - 1] for k in range(1, len(counts))]
        sizes.append(N_HUES - sum(sizes))
        pos = 1
        for k, n in enumerate(sizes):
            for i in range(n):
                t[pos + i] = bounds[k] + i * (bounds[k + 1] - bounds[k]) / n
            pos += n
        t[0] = t[N_HUES] - 360.0
        t[N_HUES + 1] = t[1] + 360.0
        t[N_HUES + 2] = t[2] + 360.0
        return t

    def _display_cusp(self, hue, rgb_c, jmh_c, prev):
        up = next((i for i in range(1, 8) if jmh_c[i][2] > hue), 1)
        lo = up - 1
        if jmh_c[lo][2] == hue:
            return jmh_c[lo][0], jmh_c[lo][1]
        lower_t, upper_t = (prev[1] if up == prev[0] else 0.0), 1.0
        while upper_t - lower_t > 1e-7:
            t = (lower_t + upper_t) / 2.0
            h = self.p_out.jmh([lerp(rgb_c[lo][i], rgb_c[up][i], t) for i in range(3)])[2]
            if h < jmh_c[lo][2]:
                upper_t = t
            elif h >= jmh_c[up][2]:
                lower_t = t
            elif h > hue:
                upper_t = t
            else:
                lower_t = t
        t = (lower_t + upper_t) / 2.0
        jmh = self.p_out.jmh([lerp(rgb_c[lo][i], rgb_c[up][i], t) for i in range(3)])
        prev[0], prev[1] = up, t
        return jmh[0], jmh[1]

    def _search_range(self):
        lo, hi = 0, 1
        for i in range(1, N_HUES + 1):
            d = i - (1 + int(self.cusps[i][2]))
            lo, hi = min(lo, d), max(hi, d + 1)
        return lo, hi

    def _upper_hull_gamma(self):
        for i in range(1, N_HUES + 1):
            cj, cm = self.cusps[i][0], self.cusps[i][1]
            hue = self.hues[i]
            thr = lerp(cj, self.j_max, FOCUS_GAIN_BLEND)
            fj = focus_j(cj, self.mid_j, self.j_max)
            tests = []
            for pos in (0.01, 0.1, 0.5, 0.8, 0.99):
                tj = lerp(cj, self.j_max, pos)
                gain = focus_gain(tj, thr, self.j_max, FOCUS_DIST)
                ji = j_intersect(tj, cm, fj, self.j_max, gain)
                tests.append((ji, vector_slope(ji, fj, self.j_max, gain), j_intersect(cj, cm, fj, self.j_max, gain)))

            def fits(gamma):
                for ji, slope, jic in tests:
                    m = gamut_boundary(cj, cm, self.j_max, 1.0 / gamma, ji, slope, jic)
                    if max(self.p_out.jmh_to_rgb(ji + slope * m, m, hue)) <= PEAK / 100.0:
                        return False
                return True
            low, high = 0.0, 0.4
            while high < 5.0 and not fits(high):
                low, high = high, high + 0.4
            while high - low > 1e-5:
                g = (high + low) / 2.0
                if fits(g):
                    high = g
                else:
                    low = g
            self.cusps[i][2] = 1.0 / high
        for i, src in ((0, N_HUES), (N_HUES + 1, 1), (N_HUES + 2, 2)):
            self.cusps[i][2] = self.cusps[src][2]

    def cusp(self, h):
        """Limiting cusp J, M and upper hull 1/gamma at hue h, interpolated in the hue table."""
        i = 1 + int(h)
        i_lo, i_hi = max(0, i + self.search[0]), min(N_HUES + 1, i + self.search[1])
        while i_lo + 1 < i_hi:     # same bisection as OCIO's lookup_hue_interval
            if h > self.hues[i]:
                i_lo = i
            else:
                i_hi = i
            i = (i_lo + i_hi) // 2
        i_hi = max(1, i_hi)
        t = (h - self.hues[i_hi - 1]) / (self.hues[i_hi] - self.hues[i_hi - 1])
        return [lerp(self.cusps[i_hi - 1][k], self.cusps[i_hi][k], t) for k in range(3)]

    def reach(self, h):
        b = int(h)
        return lerp(self.reach_m[b + 1], self.reach_m[b + 2], h - b)

    def output_transform(self, ap0):
        """ACES2065-1 -> display-linear Rec.709 0..1 (OCIO's fixed function plus its clamps)."""
        ap1 = [min(max(v, 0.0), self.ts['forward_limit']) for v in mv(AP0_TO_AP1, ap0)]
        aab = self.p_in.aab(mv(AP1_TO_AP0, ap1))
        if aab[0] <= 0.0:
            return [0.0, 0.0, 0.0]
        j, m, h = aab_to_jmh(aab)
        cos_h, sin_h = math.cos(math.radians(h)), math.sin(math.radians(h))
        m_norm = chroma_norm(cos_h, sin_h, self.cc_scale)
        reach_m = self.reach(h)
        j_ts = self.p_in.y_to_j(tonescale(self.p_in.a_to_y(aab[0]), self.ts))
        if m != 0.0:               # chroma compression
            nj = j_ts / self.j_max
            snj = max(0.0, 1.0 - nj)
            limit = nj ** (1 / CZ) * reach_m / m_norm
            m = m * (j_ts / j) ** (1 / CZ) / m_norm
            m = limit - toe(limit - m, limit - 0.001, snj * self.sat, math.sqrt(nj * nj + self.sat_thr))
            m = toe(m, limit, nj * self.compr, snj) * m_norm
        j = j_ts
        if j <= 0.0:
            j, m = 0.0, 0.0
        elif m <= 0.0 or j > self.j_max:
            m = 0.0
        else:                      # gamut compression
            cj, cm, gamma_top_inv = self.cusp(h)
            fj = focus_j(cj, self.mid_j, self.j_max)
            gain = focus_gain(j, lerp(cj, self.j_max, FOCUS_GAIN_BLEND), self.j_max, FOCUS_DIST)
            ji = j_intersect(j, m, fj, self.j_max, gain)
            slope = vector_slope(ji, fj, self.j_max, gain)
            gm = gamut_boundary(cj, cm, self.j_max, gamma_top_inv, ji, slope,
                                j_intersect(cj, cm, fj, self.j_max, gain))
            if gm <= 0.0:
                m = 0.0
            else:
                rm = boundary_m(ji, slope, 1 / CZ, self.j_max, reach_m, self.j_max)
                prop = max(gm / rm, COMPRESSION_THRESHOLD)
                thr = prop * gm
                if m > thr and prop < 1.0:
                    scale = (rm - thr) / ((rm - thr) / (gm - thr) - 1.0)
                    nd = (m - thr) / scale
                    m = thr + scale * nd / (1.0 + nd)
                j = ji + m * slope
        rgb = self.p_out.rgb(jmh_to_aab(j, m, cos_h, sin_h))
        return [min(max(v, 0.0), PEAK / 100.0) for v in rgb]

# SPDX-License-Identifier: GPL-3.0-or-later
"""OpenDRT v1.1.0 (Jed Smith, https://github.com/jedypod/open-display-transform, GPL-3.0), ported from
display-transforms/opendrt/OpenDRT.dctl at tag v1.1.0, commit 8804747def7cdc9089f2b2ea5011596792134304.

Fixed to the DCTL defaults: look preset Standard, tonescale and creative white from the look (D65), display
encoding preset Rec.1886 (Rec.709 gamut, dim surround), peak 100 nit, grey 10 nit, clamp on. Input is CIE XYZ D65,
linear; the output is display-linear Rec.709 0..1 (the 2.4 encoding is left to the node). The modules disabled by
the preset (contrast low/high), the HDR terms (zero at 100 nit) and the creative white (D65 is the identity) are left
out. Same order and names as the DCTL's transform(), in double precision (C Display.h follows this)."""
import math

# Standard look preset
TN_CON, TN_SH, TN_TOE, TN_OFF = 1.66, 0.5, 0.003, 0.005
RS_SA, RS_RW, RS_BW = 0.35, 0.25, 0.55
PT_LML, PT_LML_R, PT_LML_G, PT_LML_B = 0.25, 0.5, 0.0, 0.1
PT_LMH, PT_LMH_R, PT_LMH_B = 0.25, 0.5, 0.0
PTL_C, PTL_M, PTL_Y = 0.06, 0.08, 0.06
PTM_LOW, PTM_LOW_RNG, PTM_LOW_ST = 0.4, 0.25, 0.5
PTM_HIGH, PTM_HIGH_RNG, PTM_HIGH_ST = -0.8, 0.35, 0.4
BRL, BRL_R, BRL_G, BRL_B, BRL_RNG, BRL_ST = 0.0, -2.5, -1.5, -1.5, 0.5, 0.35
BRLP, BRLP_R, BRLP_G, BRLP_B = -0.5, -1.25, -1.25, -0.25
HC_R, HC_R_RNG = 1.0, 0.3
HS_R, HS_R_RNG, HS_G, HS_G_RNG, HS_B, HS_B_RNG = 0.6, 0.6, 0.35, 1.0, 0.66, 1.0
HS_C, HS_C_RNG = 0.25, 1.0     # hs_m and hs_y are 0 in the preset
TN_LP, TN_LG, TN_SU = 100.0, 10.0, 1   # SDR 100 nit, grey 10 nit, dim surround

# The DCTL's matrices: XYZ D65 -> P3-D65, and P3-D65 -> XYZ -> Rec.709 (its display_gamut_whitepoint)
XYZ_TO_P3 = [[2.49349691194142542, -0.93138361791912383, -0.402710784450716841],
             [-0.829488969561574696, 1.76266406031834655, 0.0236246858419435941],
             [0.0358458302437844531, -0.0761723892680418041, 0.956884524007687309]]
P3_TO_XYZ = [[0.486570948648216151, 0.265667693169093, 0.198217285234362467],
             [0.228974564069748754, 0.691738521836506193, 0.079286914093744984],
             [-4.00000000000000029e-17, 0.0451133818589026167, 1.04394436890097575]]
XYZ_TO_REC709 = [[3.24096994190452348, -1.53738317757009435, -0.498610760293003552],
                 [-0.969243636280879506, 1.87596750150771996, 0.0415550574071755843],
                 [0.0556300796969936354, -0.20397695888897649, 1.05697151424287816]]


def mul(m, v):
    return [sum(m[i][k] * v[k] for k in range(3)) for i in range(3)]


P3_TO_REC709 = [[sum(XYZ_TO_REC709[i][k] * P3_TO_XYZ[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def spow(a, b):
    return a if a <= 0.0 else a ** b


def toe(x, t, inv=False):   # compress_toe_quadratic
    return (x + math.sqrt(x * (4 * t + x))) / 2 if inv else spow(x, 2.0) / (x + t)


def hyperbolic(x, s, p):    # compress_hyperbolic_power
    return spow(x / (x + s), p)


def softplus(x, s):
    if x > 10 * s or s < 1e-4:
        return x
    return s * math.log(max(0.0, 1 + math.exp(x / s)))


def gauss(x, w):
    return math.exp(-x * x / w)


def hue_offset(h, o):       # C fmod: keeps the sign of h - o + pi, as the DCTL does
    return math.fmod(h - o + math.pi, 2 * math.pi) - math.pi


def opponent(c):
    return c[0] - c[2], c[1] - (c[0] + c[2]) / 2


# Tonescale constraints (pre-calculated by the DCTL per pixel)
TS_X1 = 2.0 ** (6 * TN_SH + 4)
TS_Y1 = TN_LP / 100
TS_X0 = 0.18 + TN_OFF
TS_Y0 = TN_LG / 100              # the HDR grey boost is log2(1) = 0
TS_S0 = toe(TS_Y0, TN_TOE, True)
TS_P = TN_CON / (1 + TN_SU * 0.05)
TS_S10 = TS_X0 * (TS_S0 ** (-1 / TN_CON) - 1)
TS_M1 = TS_Y1 / (TS_X1 / (TS_X1 + TS_S10)) ** TN_CON
TS_M2 = toe(TS_M1, TN_TOE, True)
TS_S = TS_X0 * ((TS_S0 / TS_M2) ** (-1 / TN_CON) - 1)
S_LP100 = TS_X0 * ((TN_LG / 100) ** (-1 / TN_CON) - 1)   # = ts_s1 at 100 nit


def opendrt(xyz):
    """CIE XYZ D65 (scene linear) -> display-linear Rec.709 0..1."""
    rgb = mul(XYZ_TO_P3, xyz)
    rs_w = (RS_RW, 1 - RS_RW - RS_BW, RS_BW)
    sat_l = sum(rgb[i] * rs_w[i] for i in range(3))
    rgb = [sat_l * RS_SA + v * (1 - RS_SA) + TN_OFF for v in rgb]   # render space, offset

    tsn = math.sqrt(max(0.0, sum(v * v for v in rgb))) / math.sqrt(3)
    rgb = [v / tsn if tsn != 0 else 0.0 for v in rgb]   # RGB ratios
    opp = opponent(rgb)
    ach_d = 1.25 * toe(math.sqrt(max(0.0, opp[0] ** 2 + opp[1] ** 2)) / 2, 0.25)
    hue = math.fmod(math.atan2(opp[0], opp[1]) + math.pi + 1.10714931, 2 * math.pi)
    ha_rgb = (gauss(hue_offset(hue, 0.1), 0.66), gauss(hue_offset(hue, 4.3), 0.66), gauss(hue_offset(hue, 2.3), 0.66))
    ha_hs = (gauss(hue_offset(hue, -0.4), 0.66), ha_rgb[1], gauss(hue_offset(hue, 2.5), 0.66))
    ha_c = gauss(hue_offset(hue, 3.3), 0.5)

    # Brilliance
    brl_tsf = (tsn / (tsn + 1)) ** (1 - BRL_RNG)
    brl_exf = (BRL + BRL_R * ha_rgb[0] + BRL_G * ha_rgb[1] + BRL_B * ha_rgb[2]) * ach_d ** (1 / BRL_ST)
    tsn *= 2.0 ** (brl_exf * (brl_tsf if brl_exf < 0 else 1 - brl_tsf))

    # Hyperbolic compression: tsn_pt and tsn_const coincide at 100 nit
    tsn_const = hyperbolic(tsn, S_LP100, TS_P)
    tsn_pt = tsn_const
    tsn = hyperbolic(tsn, TS_S, TS_P)

    # Hue contrast R
    hc_ts = 1 - tsn_const
    hc_c = (hc_ts * (1 - ach_d) + ach_d * (1 - hc_ts)) * ach_d * ha_rgb[0]
    hc_ts = hc_ts ** (1 / HC_R_RNG)
    hc_f = HC_R * (hc_c - 2 * hc_c * hc_ts) + 1
    rgb = [rgb[0], rgb[1] * hc_f, rgb[2] * hc_f]

    # Hue shift RGB, then CMY (only C is non-zero)
    hs = (ha_hs[0] * ach_d * tsn_pt ** (1 / HS_R_RNG) * HS_R, ha_hs[1] * ach_d * tsn_pt ** (1 / HS_G_RNG) * -HS_G,
          ha_hs[2] * ach_d * tsn_pt ** (1 / HS_B_RNG) * -HS_B)
    rgb = [rgb[0] + hs[2] - hs[1], rgb[1] + hs[0] - hs[2], rgb[2] + hs[1] - hs[0]]
    hc = ha_c * ach_d * (1 - tsn_pt) ** (1 / HS_C_RNG) * -HS_C
    rgb = [rgb[0], rgb[1] + hc, rgb[2] - hc]

    # Purity compression, limits low and high, mid-range purity
    pt_lml_p = 1 + 4 * (1 - tsn_pt) * (PT_LML + PT_LML_R * ha_hs[0] + PT_LML_G * ha_hs[1] + PT_LML_B * ha_hs[2])
    ptf = 1 - tsn_pt ** pt_lml_p
    ptf = ptf ** ((1 - ach_d * (PT_LMH_R * ha_hs[0] + PT_LMH_B * ha_hs[2])) * (1 - PT_LMH * ach_d))
    ptm_low = 1 + PTM_LOW * math.exp(-2 * ach_d * ach_d / PTM_LOW_ST) * (1 - tsn_const) ** (1 / PTM_LOW_RNG)
    ptm_high = 1 + PTM_HIGH * math.exp(-2 * ach_d * ach_d / PTM_HIGH_ST) * tsn_pt ** (1 / (4 * PTM_HIGH_RNG))
    ptf *= ptm_low * ptm_high
    rgb = [v * ptf + 1 - ptf for v in rgb]

    # Inverse render space, to the display gamut
    sat_l = sum(rgb[i] * rs_w[i] for i in range(3))
    rgb = mul(P3_TO_REC709, [(sat_l * RS_SA - v) / (RS_SA - 1) for v in rgb])

    # Post brilliance
    bo = opponent(rgb)
    bd = math.sqrt(max(0.0, bo[0] ** 2 + bo[1] ** 2)) / 4
    bd = 1.1 * (bd * bd / (bd + 0.1))
    brlp_m = BRLP + ach_d * (BRLP_R * ha_rgb[0] + BRLP_G * ha_rgb[1] + BRLP_B * ha_rgb[2])
    rgb = [v * 2.0 ** (brlp_m * bd * tsn) for v in rgb]

    # Purity softclip, final tonescale (toe), back from RGB ratios, clamp
    rgb = [softplus(rgb[0], PTL_C), softplus(rgb[1], PTL_M), softplus(rgb[2], PTL_Y)]
    tsn = toe(tsn * TS_M2, TN_TOE)
    return [min(max(v * tsn, 0.0), 1.0) for v in rgb]

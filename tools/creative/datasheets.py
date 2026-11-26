#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds ofx/SLogMetaRaw/src/creative/CreativeSheets.h: film stocks from their manufacturers' datasheets.

The measurements are the datasheets' curves (spectral sensitivity, characteristic curves, spectral dye density, base)
as digitised by the spektrafilm project (Andrea Volpato, https://github.com/andreavolpato/agx-emulsion, profiles
CC BY-SA 4.0; the data belong to Kodak and Fujifilm). The script keeps what the per-pixel chain (math/Creative.h,
cr_sheet_*) needs:

  shooting  a 3 x 3 matrix, linear Rec.2020 -> the three layers' exposures (green 1 at 18 % grey). The scene is lit
            by the stock's reference illuminant (3200 K tungsten or D55: filtered as the stock is balanced) and the
            footage saw it white-balanced (CAT02 to D65); fitted as an Academy IDT is, over the 190 training
            reflectances of rawtoaces (relative least squares, white exact), checked on the ColorChecker
  curves    each layer's characteristic curve: density over its minimum against log10 exposure (grey 0), sampled
  spectra   dyes (density of each layer's dye per unit) and base at 400-740 nm every 10 nm; for a print stock its
            layers' sensitivities times the printer's lamp (3400 K behind a Schott KG3 heat filter)
  viewing   the eye's response to each wavelength on a lamp (display-linear Rec.2020, adapted to the lamp, CAT02):
            the projector's xenon (Kinoton 75P) for prints, D50 for slides, D65 unadapted for a negative on a light box

Usage: tools/creative/datasheets.py PROFILES_DIR [--check]   (needs numpy and colour-science)
PROFILES_DIR: spektrafilm's src/spektrafilm/data/profiles. --check prints the fits' errors and writes nothing.
"""
import json
import os
import sys

import numpy as np
import colour

N = 32                       # curve samples
LO, STEP = -2.8, 0.2         # log10 exposure of the first sample (grey = 0) and the step
SW = np.arange(400.0, 741.0, 10.0)   # the chain's wavelengths
GREY = 0.18
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'ofx', 'SLogMetaRaw', 'src', 'creative',
                   'CreativeSheets.h')

# The stocks taken, in the header's order (permanent: CreativePresets.h links stocks to these indices).
PRINTS = ['kodak_2383', 'kodak_2393']
FILMS = ['kodak_vision3_500t', 'kodak_vision3_200t', 'kodak_vision3_250d', 'kodak_vision3_50d', 'kodak_verita_200d',
         'kodak_portra_160', 'kodak_portra_400', 'kodak_portra_800', 'kodak_ektar_100', 'kodak_gold_200',
         'kodak_ultramax_400', 'fujifilm_c200', 'fujifilm_pro_400h', 'fujifilm_xtra_400',
         'fujifilm_provia_100f', 'fujifilm_velvia_100', 'kodak_ektachrome_100', 'kodak_kodachrome_64']

WL = np.arange(380.0, 781.0, 5.0)
SHAPE = colour.SpectralShape(380, 780, 5)
CMF = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer'].copy().align(SHAPE).values
XYZ_TO_2020 = colour.models.RGB_COLOURSPACE_BT2020.matrix_XYZ_to_RGB
D65 = colour.xy_to_XYZ(colour.models.RGB_COLOURSPACE_BT2020.whitepoint)
TRAINING = colour.characterisation.read_training_data_rawtoaces_v1().copy().align(SHAPE).values.T
CHECKER = np.array([v.copy().align(SHAPE).values
                    for v in colour.characterisation.SDS_COLOURCHECKERS['BabelColor Average'].values()])


def kg3():
    path = os.path.join(sys.argv[1], '..', 'filters', 'heat_absorbing', 'schott', 'KG3.csv')
    d = np.loadtxt(path, delimiter=',')
    return np.clip(np.interp(WL, d[:, 0], d[:, 1]), 0, 1)


def light(kind):
    if kind == 'T':
        v = colour.SDS_LIGHT_SOURCES['Incandescent'].copy().align(SHAPE).values
    elif kind == 'K75P':
        v = colour.SDS_LIGHT_SOURCES['Kinoton 75P'].copy().align(SHAPE).values
    elif kind == 'TH-KG3':
        v = colour.colorimetry.blackbody_spectral_radiance(WL * 1e-9, 3400) * kg3()
    else:
        v = colour.SDS_ILLUMINANTS[kind].copy().align(SHAPE).values
    v = np.nan_to_num(np.asarray(v, float))
    return v / v.mean()


def load(name):
    with open(os.path.join(sys.argv[1], name + '.json')) as f:
        p = json.load(f)
    return p['info'], {k: np.asarray(v, dtype=float) for k, v in p['data'].items()
                       if k in ('log_sensitivity', 'channel_density', 'base_density', 'log_exposure', 'density_curves')}


def fill(v):
    """NaNs of a sampled curve (its ends, beyond what the datasheet plots): the nearest value."""
    v = np.array(v, float)
    ok = ~np.isnan(v)
    i = np.arange(v.size)
    return np.interp(i, i[ok], v[ok]) if ok.any() else np.zeros_like(v)


def at(v):
    """Samples on WL -> on SW."""
    return np.interp(SW, WL, v)


def curves(d):
    le = LO + STEP * np.arange(N)
    return np.array([np.interp(le, d['log_exposure'], fill(d['density_curves'][:, k])) for k in range(3)])


def sensitivity(d):
    s = np.nan_to_num(10 ** d['log_sensitivity'])
    s[(WL < 400) | (WL > 740)] = 0   # beyond the plotted range the digitisation extrapolates; glass cuts the UV
    return s


def scene(info, d, refl):
    """For reflectances under the stock's light: the footage (linear Rec.2020, white-balanced) and the exposures."""
    E = light(info['reference_illuminant'])
    S = sensitivity(d)
    spec = refl * E
    grey = GREY * (E @ S)[1]
    Xw = E @ CMF
    cat = colour.adaptation.matrix_chromatic_adaptation_VonKries(Xw / Xw[1], D65 / D65[1], 'CAT02')
    rgb = (XYZ_TO_2020 @ cat @ (spec @ CMF).T).T / Xw[1]
    white = XYZ_TO_2020 @ cat @ Xw / Xw[1]
    return rgb, spec @ S / grey, white, (E @ S) / grey


def shooting(info, d):
    """Rows: layers; relative least squares over the training set with the white kept exact (Lagrange)."""
    rgb, raw, white, wraw = scene(info, d, TRAINING)
    M = np.zeros((3, 3))
    for k in range(3):
        w = 1.0 / np.maximum(raw[:, k], 1e-3)
        A = rgb * w[:, None]
        b = raw[:, k] * w
        K = np.block([[2 * A.T @ A, white[:, None]], [white[None, :], np.zeros((1, 1))]])
        M[k] = np.linalg.solve(K, np.r_[2 * A.T @ b, wraw[k]])[:3]
    stops = lambda r, t: np.abs(np.log2(np.maximum(r, 1e-4) / np.maximum(t, 1e-4)))
    rc, rawc, _, _ = scene(info, d, CHECKER)
    err = stops(rgb @ M.T, raw), stops(rc @ M.T, rawc)
    return M, err


def dyes(d):
    eps = np.array([at(fill(d['channel_density'][:, k])) for k in range(3)])
    return eps, at(fill(d['base_density']))


def view(lamp, adapt=True):
    """3 x len(SW): display-linear Rec.2020 per unit transmittance at each wavelength, the lamp's white Y = 1."""
    L = light(lamp)
    Xw = L @ CMF
    cat = (colour.adaptation.matrix_chromatic_adaptation_VonKries(Xw / Xw[1], D65 / D65[1], 'CAT02') if adapt
           else np.eye(3))
    cmf = np.array([at(L * CMF[:, i]) for i in range(3)])   # 3 x SW
    V = XYZ_TO_2020 @ cat @ cmf
    return V / (cmf[1].sum())


def printer(d):
    S = sensitivity(d) * light('TH-KG3')[:, None]
    Q = np.array([at(S[:, k]) for k in range(3)])
    return Q / Q.sum(1, keepdims=True)


def check_sampling(film, prt):
    """The chain at 10 nm against the datasheets' 5 nm: print exposures (log10) of negatives near neutral."""
    eps5 = np.array([fill(film['channel_density'][:, k]) for k in range(3)]).T
    base5 = fill(film['base_density'])
    S = sensitivity(prt) * light('TH-KG3')[:, None]
    eps, base = dyes(film)
    Q = printer(prt)
    rng = np.random.default_rng(0)
    D = rng.uniform(0.2, 2.2, (2000, 1)) + rng.uniform(-0.7, 0.7, (2000, 3))
    full = np.log10((10 ** -(base5 + D @ eps5.T)) @ S)
    full -= full.mean(0)
    fast = np.log10((10 ** -(base + D @ eps)) @ Q.T)
    fast -= fast.mean(0)
    return np.abs(full - fast)


def main():
    check = '--check' in sys.argv
    prints = []
    for name in PRINTS:
        info, d = load(name)
        eps, base = dyes(d)
        prints.append({'info': info, 'd': d, 'curve': curves(d), 'eps': eps, 'base': base, 'Q': printer(d)})
    films = []
    for name in FILMS:
        info, d = load(name)
        M, (et, ec) = shooting(info, d)
        eps, base = dyes(d)
        pos = info['type'] == 'positive'
        films.append({'info': info, 'M': M, 'curve': curves(d), 'eps': eps, 'base': base, 'pos': pos})
        line = '%-22s shoot: training %.2f/%.2f, checker %.2f/%.2f stops (mean/max)' % (
            name, et.mean(), et.max(), ec.mean(), ec.max())
        if not pos:
            e = check_sampling(d, prints[0]['d'])
            line += '; 10 nm printing %.3f/%.3f log' % (e.mean(), e.max())
        print(line)
    if not check:
        write(prints, films)


def fmt(v):
    return ', '.join(('%.6g' % x) + ('f' if ('.' in '%.6g' % x or 'e' in '%.6g' % x) else '') for x in np.ravel(v))


def rows(v, ind):
    return (',\n' + ' ' * ind).join('{ %s }' % fmt(r) for r in v)


def write(prints, films):
    L = ['// SPDX-License-Identifier: GPL-3.0-or-later',
         '// Generated by tools/creative/datasheets.py: do not edit. Film stocks measured on their manufacturers\'',
         '// datasheets (Kodak, Fujifilm) as digitised by spektrafilm (Andrea Volpato, CC BY-SA 4.0), reduced to what',
         '// the Creative chain needs (math/Creative.h, cr_sheet_*). Method in the script, sources in',
         '// docs/creative/SCHEDE_TECNICHE.md.',
         '#pragma once',
         '',
         '// Included after math/Creative.h (CR_HD_N, CR_SPEC_N).',
         '',
         'static_assert(CR_HD_N == %d && CR_SPEC_N == %d, "regenerate CreativeSheets.h");' % (N, SW.size),
         'constexpr float kCrSheetLo = %gf, kCrSheetStep = %gf;   // log10 exposure of the first sample, step (grey 0)'
         % (LO, STEP),
         '',
         '// The eye on a lamp: display-linear Rec.2020 per unit of transmittance at each wavelength (400-740 nm, 10 nm),',
         '// the lamp\'s white of luminance 1: the projector\'s xenon, D50 for slides (both adapted to), D65 unadapted.',
         'struct CrSheetView { float v[3][CR_SPEC_N]; };']
    for nm, lamp, ad in (('kCrViewXenon', 'K75P', True), ('kCrViewSlide', 'D50', True), ('kCrViewBox', 'D65', False)):
        L.append('inline constexpr CrSheetView %s = { { %s } };' % (nm, rows(view(lamp, ad), 4)))
    L += ['',
          '// A stock: its layers\' characteristic curves (density over the minimum against log10 exposure, grey 0; a',
          '// reversal falls), its dyes (density per unit, rows: the layers red, green, blue) and base; a camera stock',
          '// its exposures from linear Rec.2020 (rows: layers), a print stock its layers\' sensitivity on the printer\'s',
          '// lamp (rows sum 1).',
          'struct CrSheet { const char* name; int reversal, daylight; float shoot[9]; float curve[3][CR_HD_N];',
          '                 float eps[3][CR_SPEC_N]; float base[CR_SPEC_N]; float sens[3][CR_SPEC_N]; };',
          '']
    def entry(r, shoot, sens):
        info = r['info']
        day = 0 if info['reference_illuminant'] == 'T' else 1
        return ['    { "%s", %d, %d,' % (info['name'], 1 if r.get('pos') else 0, day),
                '      { %s },' % fmt(shoot),
                '      { %s },' % rows(r['curve'], 8),
                '      { %s },' % rows(r['eps'], 8),
                '      { %s },' % fmt(r['base']),
                '      { %s } },' % rows(sens, 8)]
    L.append('inline constexpr CrSheet kCrSheetPrints[] = {')
    for r in prints:
        L += entry(r, np.zeros(9), r['Q'])
    L += ['};', '', 'inline constexpr CrSheet kCrSheetFilms[] = {']
    for r in films:
        L += entry(r, r['M'], np.zeros((3, SW.size)))
    L += ['};', '']
    with open(OUT, 'w') as f:
        f.write('\n'.join(L))
    print('wrote', os.path.normpath(OUT))


if __name__ == '__main__':
    main()

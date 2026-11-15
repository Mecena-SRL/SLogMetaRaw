# SPDX-License-Identifier: GPL-3.0-or-later
"""Native primaries of a log encoding from a maker's "log to Rec.709" 3D LUT (.cube).

The neutral axis of the LUT gives a per-channel linearisation (Rec.709 OETF inverted on its output); every LUT
point inside the range (no clipped channel) then gives one equation lin_out = M lin_in, solved by least squares with
each row of M summing to 1 (the camera's white stays white). M is native -> Rec.709, so the native primaries are the
columns of RGB709->XYZ times M. Whatever look the maker put in the LUT (saturation, hue twists) ends up in M too.

    python3 tools/lut_gamut.py "DJI OSMO Action 5 Pro D-Log M to Rec.709 V1.cube"
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gamut_matrices import matrices  # noqa: E402


def rec709_decode(v):
    v = np.asarray(v, float)
    return np.where(v < 0.081, v / 4.5, ((v + 0.099) / 1.099) ** (1 / 0.45))


def load_cube(path):
    rows = [line.split() for line in open(path, encoding='utf-8', errors='replace')
            if line[:1] in '-0123456789.' and len(line.split()) == 3]
    a = np.array(rows, float)
    n = round(len(a) ** (1 / 3))
    return a.reshape(n, n, n, 3)   # .cube order: red fastest -> [b][g][r]


def fit(lut):
    n = lut.shape[0]
    grid = np.linspace(0, 1, n)
    neutral = rec709_decode(np.array([lut[i, i, i, 1] for i in range(n)]))
    idx = np.array([(r, g, b) for b in range(n) for g in range(n) for r in range(n)])
    out = lut[idx[:, 2], idx[:, 1], idx[:, 0]]
    lin_in = np.interp(idx / (n - 1), grid, neutral)
    lin_out = rec709_decode(np.clip(out, 0, 1))
    ok = (out.max(1) < 0.95) & (out.min(1) > 0.03) & (idx.max(1) / (n - 1) < 0.75)
    li, lo = lin_in[ok], lin_out[ok]
    m = np.zeros((3, 3))
    for row in range(3):   # lo = a (r - b) + b' (g - b) + b: rows sum to 1
        a = np.stack([li[:, 0] - li[:, 2], li[:, 1] - li[:, 2]], 1)
        x, y = np.linalg.lstsq(a, lo[:, row] - li[:, 2], rcond=None)[0]
        m[row] = [x, y, 1 - x - y]
    return m, float(np.abs(li @ m.T - lo).mean()), int(ok.sum())


def main():
    m, err, used = fit(load_cube(sys.argv[1]))
    xyz = np.array(matrices()[1][1]) @ m
    print('native -> Rec.709:\n', np.round(m, 5))
    print('mean error %.4f (linear) on %d points' % (err, used))
    for name, c in zip('RGB', np.eye(3)):
        v = xyz @ c
        print('%s  x %.6f  y %.6f' % (name, v[0] / v.sum(), v[1] / v.sum()))


if __name__ == '__main__':
    main()

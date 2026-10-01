#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prototype for #13: sample the S-Log MetaRaw node into a 3D .cube LUT.

The develop maths (ofx/SLogMetaRaw/math, one pixel at a time) is the single source of
truth, so the LUT is sampled by the same compiled code the node runs on the CPU:
tests/ofx_math_test reads one pixel + parameter line per row and prints the developed pixel.
The grid is indexed by the node's input encoding (the camera log), red fastest, as .cube wants.

  python3 tools/export_lut.py out.cube --size 33 --k 5600 --ei 1600 --set toneContrast=20

Only what the node does per pixel lands in the LUT. Detail (Texture, Clarity, Dehaze), false
colour views and the data-level auto-fix (needs the clip) do not: this prototype leaves them off.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from slogmetaraw import camera  # noqa: E402

OFX = os.path.join(ROOT, 'ofx', 'SLogMetaRaw')
ZONES = ('Black', 'Shadow', 'Light', 'Specular')
# the order ofx_math_test reads the 27 tone values in, with the node's defaults
TONE = [('toneContrast', 0), ('toneHighlights', 0), ('toneShadows', 0), ('toneWhites', 0), ('toneBlacks', 0),
        ('toneVibrance', 0), ('toneSaturation', 0), ('zonePivot', 0)]
TONE += [('zone%sExp' % z, 0) for z in ZONES] + [('zone%sSat' % z, 0) for z in ZONES]
TONE += [('zone%sRange' % z, r) for z, r in zip(ZONES, (-4, 1, -1, 4))]
TONE += [('zone%sFalloff' % z, f) for z, f in zip(ZONES, (1, 2, 2, 1))]
TONE += [('softClip', 0), ('softClipLevel', 2.5), ('softClipColor', 0)]


def build_evaluator():
    """Path of ofx_math_test: compiled on the spot (a few seconds) into a temp dir."""
    cxx = shutil.which('c++') or shutil.which('clang++') or shutil.which('g++')
    if not cxx:
        raise SystemExit('serve un compilatore C++ (c++, clang++ o g++)')
    exe = os.path.join(tempfile.mkdtemp(prefix='slog_lut_'), 'ofx_math_test')
    subprocess.run([cxx, '-std=c++17', '-O2', '-ffp-contract=off', '-I', OFX,
                    os.path.join(ROOT, 'tests', 'ofx_math_test.cpp'), '-o', exe], check=True)
    return exe


def grid_lines(size, head, tail):
    """One parameter line per grid point, red index fastest."""
    top = size - 1.0
    for b in range(size):
        for g in range(size):
            for r in range(size):
                yield '%r %r %r %s %s' % (r / top, g / top, b / top, head, tail)


def sample(exe, size, node, out, shot, chosen, tone):
    """The developed value of every grid point, as (r, g, b) tuples in .cube order."""
    head = '%d %d %d %d %r %r %r %r %r %r' % (node[0], node[1], out[0], out[1], *shot, *chosen)
    # levelFix levelSpace levelGamma levelGain levelOffset, fcMode and its six constants: all off
    tail = '0 0 0 1 0 0 0 0 0 0 0 0 ' + ' '.join(repr(float(tone[n])) for n, _ in TONE)
    text = subprocess.run([exe], input='\n'.join(grid_lines(size, head, tail)) + '\n',
                          capture_output=True, text=True, check=True).stdout
    rows = [tuple(float(v) for v in line.split()) for line in text.splitlines()]
    if len(rows) != size ** 3:
        raise SystemExit('il valutatore ha risposto con %d righe invece di %d' % (len(rows), size ** 3))
    return rows


def write_cube(path, title, size, rows):
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write('TITLE "%s"\nLUT_3D_SIZE %d\nDOMAIN_MIN 0 0 0\nDOMAIN_MAX 1 1 1\n' % (title, size))
        for r, g, b in rows:
            fh.write('%.6f %.6f %.6f\n' % (r, g, b))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('output')
    ap.add_argument('--size', type=int, default=33, help='points per axis (default 33)')
    ap.add_argument('--node-space', default='S-Gamut3.Cine', choices=camera.SPACE_CODE)
    ap.add_argument('--node-gamma', default='SLog3', choices=camera.GAMMA_CODE)
    ap.add_argument('--out-space', default='Rec.709', choices=camera.SPACE_CODE)
    ap.add_argument('--out-gamma', default='Rec.709', choices=camera.GAMMA_CODE)
    ap.add_argument('--shot-k', type=float, default=5600, help='as-shot Kelvin')
    ap.add_argument('--shot-tint', type=float, default=0)
    ap.add_argument('--shot-ei', type=float, default=800)
    ap.add_argument('--k', type=float, help='chosen Kelvin (default: as shot)')
    ap.add_argument('--tint', type=float, help='chosen tint (default: as shot)')
    ap.add_argument('--ei', type=float, help='chosen EI (default: as shot)')
    ap.add_argument('--set', action='append', default=[], metavar='NAME=VALUE',
                    help='a tone or zone control: ' + ', '.join(n for n, _ in TONE))
    ap.add_argument('--evaluator', help='path of a built ofx_math_test (default: compile one)')
    a = ap.parse_args(argv)
    if not 2 <= a.size <= 129:
        ap.error('--size must be between 2 and 129')
    tone = dict(TONE)
    for item in a.set:
        name, _, value = item.partition('=')
        if name not in tone:
            ap.error('unknown control %r' % name)
        tone[name] = float(value)
    chosen = (a.k if a.k is not None else a.shot_k, a.tint if a.tint is not None else a.shot_tint,
              a.ei if a.ei is not None else a.shot_ei)
    exe = a.evaluator or build_evaluator()
    rows = sample(exe, a.size, (camera.SPACE_CODE[a.node_space], camera.GAMMA_CODE[a.node_gamma]),
                  (camera.SPACE_CODE[a.out_space], camera.GAMMA_CODE[a.out_gamma]),
                  (a.shot_k, a.shot_tint, a.shot_ei), chosen, tone)
    write_cube(a.output, 'S-Log MetaRaw %s/%s to %s/%s' % (a.node_space, a.node_gamma, a.out_space, a.out_gamma),
               a.size, rows)
    print('%s: %d punti, %d^3' % (a.output, len(rows), a.size))
    return 0


if __name__ == '__main__':
    sys.exit(main())

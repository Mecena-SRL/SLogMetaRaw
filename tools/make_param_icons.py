# SPDX-License-Identifier: GPL-3.0-or-later
"""Draw the toggle icons of the SLogMetaRaw OpenFX nodes.

Usage: python3 tools/make_param_icons.py [output dir]   (default: ofx/SLogMetaRaw)

Each icon is a disc split down the middle into the two ends of the axis its
control moves, which is the same idea as the half-black/half-white disc every
editor reads as "contrast":

    fc_exposure.png     black | white        stops around 18% grey
    fc_temperature.png  blue  | orange       cool | warm, along the Planckian locus
    fc_tint.png         green | magenta      the axis across it
    fc_zones.png        four bands           the tone zones, in their false-colour tints
    view_gain.png       blue | grey | orange what the Detail node lowers, leaves, raises
    view_base.png       smooth dark -> light the edge-aware base, without its detail
    view_zones.png      eleven bands         the zones 0..X of the Detail's zone view, in its colours
    view_zones_grey.png eleven greys         the same as a grey scale
    subject_face.png    a face lit from one side   Cinematografico: the subject is a face (skin)
    subject_landscape.png  a mountain under a sky  Cinematografico: the subject is a landscape

Written with zlib only, so the build needs nothing installed.
"""
import os
import struct
import sys
import zlib

SIZE = 256
SS = 4                      # supersampling factor for the edges
RING = (0.62, 0.64, 0.68)   # a thin neutral rim, so the disc reads on any panel

ICONS = {
    'fc_exposure':    [(0.07, 0.07, 0.08), (0.94, 0.94, 0.94)],
    'fc_temperature': [(0.17, 0.55, 1.00), (1.00, 0.54, 0.12)],
    'fc_tint':        [(0.24, 0.81, 0.29), (0.88, 0.29, 0.82)],
    'fc_zones':       [(0.42, 0.20, 0.72), (0.18, 0.45, 0.95), (0.98, 0.78, 0.20), (0.98, 0.98, 0.95)],
    'view_gain':      [(0.17, 0.45, 1.00), (0.55, 0.55, 0.57), (1.00, 0.54, 0.12)],
    # math/Detail.h SM_DT_ZONE_RGB
    'view_zones':     [(0.25, 0.00, 0.35), (0.10, 0.10, 0.55), (0.10, 0.35, 0.85), (0.00, 0.60, 0.70),
                       (0.25, 0.65, 0.30), (0.50, 0.50, 0.50), (0.95, 0.65, 0.55), (0.95, 0.85, 0.25),
                       (1.00, 0.60, 0.10), (0.95, 0.20, 0.10), (1.00, 1.00, 1.00)],
    'view_zones_grey': [(0.1 * z,) * 3 for z in range(11)],
}
SMOOTH = {'view_base': ((0.08, 0.08, 0.09), (0.94, 0.94, 0.94))}


def face(x, y):
    """A face lit from the left, dark background: chiaroscuro."""
    if ((x - 0.5) / 0.27) ** 2 + ((y - 0.5) / 0.36) ** 2 > 1.0:
        return (0.07, 0.07, 0.08)
    return (0.96, 0.80, 0.66) if x < 0.5 else (0.30, 0.20, 0.15)


def landscape(x, y):
    """A mountain with a snowy top under a sky, on a green ground."""
    if y > 0.72:
        return (0.28, 0.55, 0.25)
    peak = 0.25 + 1.3 * abs(x - 0.5)
    if y >= peak:
        return (0.96, 0.96, 0.97) if y < 0.40 else (0.48, 0.50, 0.55)
    return (0.45, 0.70, 1.00)


PICTURES = {'subject_face': face, 'subject_landscape': landscape}


def disc(colour_at):
    """RGBA rows of one icon, anti-aliased by supersampling; colour_at(x, y) for x, y in [0, 1], x across."""
    n = SIZE * SS
    c = (n - 1) / 2.0
    outer = n * 0.47
    inner = outer - n * 0.045
    rows = []
    for y in range(SIZE):
        row = bytearray([0])
        for x in range(SIZE):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(SS):
                for sx in range(SS):
                    px, py = x * SS + sx - c, y * SS + sy - c
                    r = (px * px + py * py) ** 0.5
                    if r > outer:
                        continue
                    col = RING if r > inner else colour_at(min(max((px + outer) / (2 * outer), 0.0), 0.999),
                                                           min(max((py + outer) / (2 * outer), 0.0), 0.999))
                    acc[0] += col[0]
                    acc[1] += col[1]
                    acc[2] += col[2]
                    acc[3] += 1.0
            k = SS * SS
            a = acc[3] / k
            if a > 0:                      # un-premultiply, PNG alpha is straight
                for i in range(3):           # the colours above are already display values
                    row.append(int(min(1.0, acc[i] / acc[3]) * 255 + 0.5))
            else:
                row += b'\x00\x00\x00'
            row.append(int(a * 255 + 0.5))
        rows.append(bytes(row))
    return b''.join(rows)


def write_png(path, raw, width, height):
    def chunk(tag, data):
        body = tag + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xffffffff)
    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(raw, 9))
    out += chunk(b'IEND', b'')
    with open(path, 'wb') as fh:
        fh.write(out)


def main(argv):
    out_dir = argv[1] if len(argv) > 1 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'ofx', 'SLogMetaRaw')
    shapes = {name: (lambda t, _y, b=bands: b[int(t * len(b))]) for name, bands in ICONS.items()}
    shapes.update({name: (lambda t, _y, a=a, b=b: tuple(a[i] + (b[i] - a[i]) * t for i in range(3)))
                   for name, (a, b) in SMOOTH.items()})
    shapes.update(PICTURES)
    for name, colour_at in shapes.items():
        path = os.path.join(out_dir, name + '.png')
        write_png(path, disc(colour_at), SIZE, SIZE)
        print('written', path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

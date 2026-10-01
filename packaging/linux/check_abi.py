#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that the plugin binary loads on the distributions DaVinci Resolve runs on.

  check_abi.py SLogMetaRaw.ofx [--max-glibc 2.28]

Fails when the binary needs a glibc newer than --max-glibc (it would not load on Rocky 8, Ubuntu 20.04, ...),
links libstdc++ / libgcc_s dynamically (Resolve loads its own, older copies first) or does not export the two
OpenFX entry points.
"""
import argparse
import re
import subprocess
import sys


def version_key(text):
    return tuple(int(p) for p in text.split('.'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('binary')
    ap.add_argument('--max-glibc', default='2.28')
    args = ap.parse_args()

    dynamic = subprocess.run(['objdump', '-p', args.binary], capture_output=True, text=True, check=True).stdout
    needed = re.findall(r'NEEDED\s+(\S+)', dynamic)
    symbols = subprocess.run(['objdump', '-T', args.binary], capture_output=True, text=True, check=True).stdout

    problems = []
    glibc = sorted({m for m in re.findall(r'GLIBC_(\d+(?:\.\d+)+)', symbols)}, key=version_key)
    top = glibc[-1] if glibc else '0'
    if version_key(top) > version_key(args.max_glibc):
        problems.append('needs GLIBC_%s (more than %s)' % (top, args.max_glibc))
    for lib in needed:
        if lib.startswith(('libstdc++', 'libgcc_s')):
            problems.append('links %s dynamically' % lib)
    exported = set(re.findall(r'\s(OfxGetNumberOfPlugins|OfxGetPlugin)\s*$', symbols, re.M))
    if exported != {'OfxGetNumberOfPlugins', 'OfxGetPlugin'}:
        problems.append('does not export the OpenFX entry points (found %s)' % sorted(exported))

    print('%s: needs glibc up to %s, libraries %s' % (args.binary, top, ', '.join(needed) or '-'))
    for p in problems:
        print('ERROR', p, file=sys.stderr)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())

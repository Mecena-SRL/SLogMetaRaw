#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fails when the documented version disagrees with slogmetaraw.__version__.

Checks the DMG name in each README, the User-Agent line and the newest
RELEASE_NOTES.md section. Run from CI, or by hand before tagging a release."""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from slogmetaraw import __version__  # noqa: E402

READMES = ('README.md', 'README.it.md', 'README.es.md', 'README.pt.md', 'README.zh.md')


def read(name):
    with open(os.path.join(ROOT, name), encoding='utf-8') as fh:
        return fh.read()


def main():
    errors = []
    for name in READMES:
        text = read(name)
        for pattern in (r'SLogMetaRaw-(\d+\.\d+\.\d+)\.dmg', r'User-Agent: SLogMetaRaw/(\d+\.\d+\.\d+)',
                        r'`v(\d+\.\d+\.\d+)`'):
            found = set(re.findall(pattern, text))
            if not found:
                errors.append('%s: nessuna corrispondenza per %s' % (name, pattern))
            elif found != {__version__}:
                errors.append('%s: %s invece di %s' % (name, sorted(found), __version__))
    top = re.search(r'^## S-Log MetaRaw (\d+\.\d+\.\d+)', read('RELEASE_NOTES.md'), re.M)
    if not top or top.group(1) != __version__:
        errors.append('RELEASE_NOTES.md: ultima sezione %s invece di %s'
                      % (top.group(1) if top else None, __version__))
    for line in errors:
        print('ERROR', line, file=sys.stderr)
    if not errors:
        print('versione %s coerente' % __version__)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())

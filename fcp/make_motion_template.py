#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the Motion Template that makes the Log MetaRaw FxPlug appear in Final Cut Pro's Effects browser.

Final Cut Pro lists effects from Motion Templates (.moef). A template of this kind is a small Motion
project: an "Effect Source" image with the FxPlug filter applied (by its effect UUID from the plug-in's
Info.plist) and the filter's parameter groups published to the Inspector.

    python3 make_motion_template.py [--install]

--install copies it to ~/Movies/Motion Templates.localized/Effects.localized/Log MetaRaw/, the per-user
folder Final Cut Pro reads at launch (no administrator rights needed).
"""
import os
import plistlib
import shutil
import sys
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
PLIST = os.path.join(HERE, 'Resources', 'Plugin-Info.plist')
OUT_DIR = os.path.join(HERE, 'build', 'Motion Templates', 'Log MetaRaw', 'Log MetaRaw')
INSTALL = os.path.expanduser('~/Movies/Motion Templates.localized/Effects.localized/Log MetaRaw')

FILTER_ID = 10145
GROUP_FLAGS = 8589938704      # a parameter group, as Motion writes it
PARAM_FLAGS = 12884901904     # a plug-in parameter, animatable

# The parameter tree of DevelopEffect.swift (enum P): ('g', id, name, children) for a group, ('p', id, name, default)
# for a value, ('s', id, name, text) for a string. Top-level entries are published to Final Cut's Inspector.
TREE = [
    ('s', 1, 'Camera', 'Nessun metadata'),
    ('p', 2, 'Leggi metadata', 0),
    ('g', 100, 'Ripresa', [
        ('p', 101, 'Bilanciamento', 0), ('p', 102, 'Temperatura', 5600), ('p', 103, 'False color temperatura', 0),
        ('p', 104, 'Tint', 0), ('p', 105, 'False color tint', 0),
        ('p', 111, 'EI', 800), ('p', 112, 'False color esposizione', 0),
    ]),
    ('g', 300, 'Toni', [
        ('p', 301, 'Highlights', 0), ('p', 302, 'Bianco (stop)', 2.5), ('p', 303, 'Soft Clip', 0),
        ('p', 304, 'Saturation', 0), ('p', 305, 'Mostra tutti i controlli', 0), ('p', 306, 'Contrast', 0),
        ('p', 307, 'Shadows', 0), ('p', 308, 'Whites', 0), ('p', 309, 'Blacks', 0), ('p', 310, 'Vibrance', 0),
    ]),
    ('g', 200, 'Spazio colore', [
        ('g', 210, 'Ingresso', [('p', 211, 'Spazio colore', 8), ('p', 212, 'Gamma', 9)]),
        ('g', 220, 'Uscita', [('p', 221, 'Tone mapping', 0), ('p', 222, 'Compressione gamut', 0),
                              ('p', 223, 'Mappatura gamut', 0), ('p', 224, 'Vibrance luci', 0),
                              ('p', 225, 'Spazio colore', 0), ('p', 226, 'Gamma', 0)]),
    ]),
    ('g', 400, 'Senza metadata', [('p', 401, 'EI di ripresa', 800), ('p', 402, 'Kelvin di ripresa', 5600),
                                  ('p', 403, 'Tint di ripresa', 0)]),
]

T = '<timing in="0 1 1 0" out="1199198 120000 1 0" offset="0 1 1 0"/>'


def effect_uuid():
    with open(PLIST, 'rb') as fh:
        d = plistlib.load(fh)
    e = d['ProPlugPlugInList'][0]
    return e['uuid'], e['displayName']


def _params_xml(entries, depth):
    pad = '\t' * depth
    out = []
    for e in entries:
        kind, pid, name = e[0], e[1], escape(e[2])
        if kind == 'g':
            out.append('%s<parameter name="%s" id="%d" flags="%d">' % (pad, name, pid, GROUP_FLAGS))
            out.append('%s\t<foldFlags>4</foldFlags>' % pad)
            out += _params_xml(e[3], depth + 1)
            out.append('%s</parameter>' % pad)
        elif kind == 's':
            out.append('%s<parameter name="%s" id="%d" flags="%d">' % (pad, name, pid, PARAM_FLAGS))
            out.append('%s\t<text>%s</text>' % (pad, escape(e[3])))
            out.append('%s</parameter>' % pad)
        else:
            out.append('%s<parameter name="%s" id="%d" flags="%d" default="%s" value="%s"/>'
                       % (pad, name, pid, PARAM_FLAGS, e[3], e[3]))
    return out


def filter_xml(uuid, name):
    out = ['\t\t\t<filter name="%s" id="%d" factoryID="6" pluginUUID="%s" pluginVersion="1" pluginName="%s" '
           'pluginDynamicParams="0">' % (escape(name), FILTER_ID, uuid, escape(name)),
           '\t\t\t\t' + T, '\t\t\t\t<baseFlags>8589934608</baseFlags>']
    out += _params_xml(TREE, 4)
    out.append('\t\t\t</filter>')
    return '\n'.join(out)


SCAFFOLD = os.path.join(HERE, 'Resources', 'MotionTemplate.moef.in')


def template():
    """The Motion project scaffold (Resources/MotionTemplate.moef.in, as Motion writes it) with our filter and
    the published parameter groups filled in."""
    uuid, name = effect_uuid()
    with open(SCAFFOLD, encoding='utf-8') as fh:
        t = fh.read()
    publish = '\n'.join('\t\t<target object="%d" channel="./%d" name="%s"/>' % (FILTER_ID, e[1], escape(e[2]))
                        for e in TREE)
    return t.replace('@@FILTER@@', filter_xml(uuid, name)).replace('@@PUBLISH@@', publish)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, 'Log MetaRaw.moef')
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write(template())
    print('scritto', path)
    if '--install' in sys.argv:
        dest = os.path.join(INSTALL, 'Log MetaRaw')
        if os.path.isdir(dest):
            shutil.rmtree(dest)
        os.makedirs(INSTALL, exist_ok=True)
        shutil.copytree(OUT_DIR, dest)
        print('installato in', dest)


if __name__ == '__main__':
    main()

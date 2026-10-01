# SPDX-License-Identifier: GPL-3.0-or-later
"""Guards for slogmetaraw/i18n.py: no window string may stay untranslated."""
import ast
import os
import re
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from slogmetaraw import i18n  # noqa: E402


def call_strings(module, func, arg=0):
    """All string literals passed as argument `arg` of func() in slogmetaraw/<module>."""
    with open(os.path.join(ROOT, 'slogmetaraw', module), encoding='utf-8') as fh:
        src = fh.read()
    out = set()
    for node in ast.walk(ast.parse(src)):
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                and node.func.id == func and len(node.args) > arg
                and isinstance(node.args[arg], ast.Constant)
                and isinstance(node.args[arg].value, str)):
            out.add(node.args[arg].value)
    return out


def ui_t_strings():
    """All string literals passed to t() in ui.py."""
    return call_strings('ui.py', 't')


class I18n(unittest.TestCase):
    def test_language_is_supported(self):
        self.assertIn(i18n.language(), i18n.LANGS)

    def test_unknown_strings_fall_back(self):
        self.assertEqual(i18n.t('stringa inesistente'), 'stringa inesistente')

    def test_window_strings_are_translated_everywhere(self):
        """Every t() string in ui.py must exist in en/es/pt/zh (it is the source)."""
        missing = {}
        for lang in ('en', 'es', 'pt', 'zh'):
            missing[lang] = sorted(s for s in ui_t_strings()
                                   if s not in i18n.TABLES[lang])
        self.assertEqual({k: v for k, v in missing.items() if v}, {},
                         'stringhe della finestra senza traduzione')

    def test_clip_warnings_are_translated_everywhere(self):
        """Every warning template of extract.py must exist in en/es/pt/zh."""
        templates = call_strings('extract.py', '_warn', 1)
        self.assertGreaterEqual(len(templates), 6)
        missing = {lang: sorted(s for s in templates if s not in i18n.TABLES[lang])
                   for lang in ('en', 'es', 'pt', 'zh')}
        self.assertEqual({k: v for k, v in missing.items() if v}, {})

    def test_warning_is_translated_before_formatting(self):
        fmt = 'NRT XML illeggibile: %s'
        r = {'warnings': [fmt % 'boom'], 'warning_keys': [[fmt, ['boom']]]}
        old = i18n._lang
        try:
            i18n._lang = 'en'
            self.assertEqual(i18n.warning_texts(r), ['Unreadable NRT XML: boom'])
            i18n._lang = 'it'
            self.assertEqual(i18n.warning_texts(r), ['NRT XML illeggibile: boom'])
            self.assertEqual(i18n.warning_texts({'warnings': ['x']}), ['x'])   # older cache
        finally:
            i18n._lang = old

    def test_no_placeholders_are_lost(self):
        """Translation templates must keep the same %s/%d placeholders."""
        for lang, table in i18n.TABLES.items():
            for src, tr in table.items():
                if '%' in src:
                    self.assertEqual(re.findall(r'%[sd]', src), re.findall(r'%[sd]', tr),
                                     '%s: segnaposto diversi in %r -> %r' % (lang, src, tr))


if __name__ == '__main__':
    unittest.main()

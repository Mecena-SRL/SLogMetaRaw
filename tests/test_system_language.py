# SPDX-License-Identifier: GPL-3.0-or-later
"""The window language follows the system on every platform, and the library keeps to Python 3.6 (Rocky/RHEL 8)."""
import ast
import glob
import os
import re
import sys
import unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from slogmetaraw import i18n  # noqa: E402


class SystemLanguage(unittest.TestCase):
    def language(self, **env):
        clean = {k: v for k, v in os.environ.items() if k not in ('LANGUAGE', 'LC_ALL', 'LC_MESSAGES', 'LANG')}
        clean.update(env)
        with mock.patch.object(i18n.sys, 'platform', 'linux'), mock.patch.object(i18n.os, 'name', 'posix'), \
                mock.patch.dict(os.environ, clean, clear=True):
            return i18n._system_language()

    def test_linux_lang(self):
        self.assertEqual(self.language(LANG='it_IT.UTF-8'), 'it')
        self.assertEqual(self.language(LANG='es_ES.UTF-8'), 'es')
        self.assertEqual(self.language(LANG='pt_BR.UTF-8'), 'pt')
        self.assertEqual(self.language(LANG='zh_CN.UTF-8'), 'zh')

    def test_language_wins_over_lang_and_takes_the_first_known(self):
        self.assertEqual(self.language(LANGUAGE='fr:it:en', LANG='en_US.UTF-8'), 'it')

    def test_unknown_and_c_locales_are_english(self):
        self.assertEqual(self.language(LANG='C.UTF-8'), 'en')
        self.assertEqual(self.language(LANG='POSIX'), 'en')
        self.assertEqual(self.language(LANG='de_DE.UTF-8'), 'en')
        self.assertEqual(self.language(), 'en')

    def test_macos_still_reads_apple_languages(self):
        out = mock.Mock(stdout='(\n    "it-IT",\n    en\n)\n')
        with mock.patch.object(i18n.sys, 'platform', 'darwin'), mock.patch.object(i18n.subprocess, 'run', return_value=out):
            self.assertEqual(i18n._system_language(), 'it')

    def test_windows_reads_the_user_locale(self):
        class Buffer:
            value = ''

            def __len__(self):
                return 85

        class Kernel32:
            @staticmethod
            def GetUserDefaultLocaleName(buf, size):
                buf.value = 'es-ES'

        fake = mock.Mock(windll=mock.Mock(kernel32=Kernel32), create_unicode_buffer=lambda n: Buffer())
        with mock.patch.object(i18n.sys, 'platform', 'win32'), mock.patch.object(i18n.os, 'name', 'nt'), \
                mock.patch.dict(sys.modules, {'ctypes': fake}):
            self.assertEqual(i18n._system_language(), 'es')


class PythonFloor(unittest.TestCase):
    """RHEL/Rocky 8 ship Python 3.6: no syntax or call newer than that in what runs inside Resolve."""
    FILES = sorted(glob.glob(os.path.join(ROOT, 'slogmetaraw', '*.py'))) + [
        os.path.join(ROOT, 'resolve_script', 'SLogMetaRaw.py'), os.path.join(ROOT, 'tools', 'render_launcher.py')]

    def test_syntax_is_python_3_6(self):
        for path in self.FILES:
            with open(path, encoding='utf-8') as fh:
                try:
                    ast.parse(fh.read(), path, feature_version=(3, 6))
                except SyntaxError as exc:
                    self.fail('%s: not Python 3.6 syntax: %s' % (path, exc))

    def test_no_calls_added_after_3_6(self):
        newer = (r'capture_output\s*=', r'\btext\s*=\s*True', r'\.reconfigure\(', r'\.isascii\(', r'time_ns\(',
                 r'\.removeprefix\(', r'\.removesuffix\(', r'nullcontext', r'cached_property', r'shlex\.join',
                 r'math\.isqrt', r'math\.dist', r'from __future__ import annotations', r'dataclass')
        for path in self.FILES:
            with open(path, encoding='utf-8') as fh:
                for number, line in enumerate(fh, 1):
                    code = line.split('#')[0]
                    for pattern in newer:
                        if re.search(pattern, code) and 'except AttributeError' not in code:
                            if pattern == r'\.reconfigure\(' and 'stream.reconfigure' in code:
                                continue        # the guarded call in __main__._utf8_stream
                            self.fail('%s:%d: %s' % (path, number, line.strip()))


if __name__ == '__main__':
    unittest.main()

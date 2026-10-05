# SPDX-License-Identifier: GPL-3.0-or-later
"""UTF-8 paths and environment in src/common on every system: on Windows they go through the "W" APIs."""
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from plugin_build import test_bin  # noqa: E402


class Utf8(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = test_bin('common_test')
        if not cls.exe:
            raise unittest.SkipTest('binari di test non compilabili qui')

    def test_accented_folders_and_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = subprocess.run([self.exe, 'utf8files', tmp], capture_output=True, text=True, timeout=30).stdout
            self.assertEqual(out.split(), ['written=1', 'read=1', 'text=ok', 'exists=1', 'cachekey=1', 'removed=1',
                                           'gone=1'])
            self.assertTrue(os.path.isdir(os.path.join(tmp, 'caffè')))

    def test_the_child_receives_accented_variables(self):
        env = dict(os.environ, SMR_TEST_VALUE='Caffè à Bahía')
        out = subprocess.run([self.exe, 'runout', '20000', sys.executable, '-c',
                              'import os; print(os.environ["SMR_TEST_VALUE"].encode("utf-8").hex())'],
                             capture_output=True, text=True, timeout=30, env=env)
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(bytes.fromhex(out.stdout.strip()).decode('utf-8'), env['SMR_TEST_VALUE'])

    def test_same_cache_record_as_the_script(self):
        sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        from slogmetaraw import plugin_cache
        with tempfile.TemporaryDirectory() as tmp:
            for name in ('A001.MP4', 'Citta\u0300.MP4', 'Citt\u00e0.MP4'):   # NFD and NFC spellings
                clip = os.path.join(tmp, name)
                open(clip, 'wb').close()
                with self.subTest(name=name):
                    out = subprocess.run([self.exe, 'cachekey', clip], capture_output=True, timeout=30).stdout
                    key = os.path.basename(out.decode('utf-8').strip())
                    self.assertEqual(key, os.path.basename(plugin_cache.cache_path(clip)))


class FlatJson(unittest.TestCase):
    """The update worker parses whatever a killed child left on stdout: a cut-off string must not throw."""
    @classmethod
    def setUpClass(cls):
        cls.exe = test_bin('common_test')
        if not cls.exe:
            raise unittest.SkipTest('binari di test non compilabili qui')

    def parse(self, text):
        out = subprocess.run([self.exe, 'json', text], capture_output=True, text=True, timeout=30)
        self.assertEqual(out.returncode, 0, out.stderr)
        return out.stdout.splitlines()

    def test_complete(self):
        self.assertEqual(self.parse('{"latest": "2.2.3", "newer": true}'), ['latest=2.2.3', 'newer=true'])

    def test_cut_off_key_or_value(self):
        for text in ('{"lat', '{"latest": "2.2.3", "dmg', '{"latest": "2.2', '{"a": "x\\'):
            with self.subTest(text=text):
                lines = self.parse(text)
                self.assertTrue(all(not l.startswith(('dmg', 'a=')) for l in lines), lines)
                self.assertNotIn('latest=2.2', lines)


if __name__ == '__main__':
    unittest.main()

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


class FindPython(unittest.TestCase):
    """Resolve 20 and earlier have no Python of their own: the plugin must find the one the user installed."""
    @classmethod
    def setUpClass(cls):
        cls.exe = test_bin('common_test')
        if not cls.exe:
            raise unittest.SkipTest('binari di test non compilabili qui')

    def candidates(self, **env):
        full = dict(os.environ)
        full.pop('SLOGMETARAW_PYTHON', None)
        full.update(env)
        out = subprocess.run([self.exe, 'pythons'], capture_output=True, text=True, timeout=30, env=full)
        self.assertEqual(out.returncode, 0, out.stderr)
        lines = out.stdout.splitlines()
        return lines[:-1], lines[-1][len('found='):]

    def test_forced_python_comes_first(self):
        listed, found = self.candidates(SLOGMETARAW_PYTHON=sys.executable)
        self.assertEqual(listed[0], sys.executable)
        self.assertEqual(found, sys.executable)

    def test_finds_a_python(self):
        self.assertTrue(self.candidates()[1])

    @unittest.skipUnless(sys.platform == 'darwin', 'macOS only')
    def test_macos_skips_the_command_line_tools_stub(self):
        listed, _ = self.candidates(PATH='/usr/bin:/bin:/usr/sbin:/sbin')
        self.assertNotIn('/usr/bin/python3', listed)
        framework = '/Library/Frameworks/Python.framework/Versions/Current/bin/python3'
        self.assertLess(listed.index(framework), listed.index('/usr/local/bin/python3'))
        self.assertIn('/Library/Developer/CommandLineTools/usr/bin/python3', listed)

    @unittest.skipUnless(os.name == 'nt', 'Windows only')
    def test_windows_skips_the_store_alias(self):
        alias = os.path.join(os.environ.get('LOCALAPPDATA', r'C:\Users\x\AppData\Local'), 'Microsoft', 'WindowsApps')
        listed, _ = self.candidates(PATH=alias)
        self.assertFalse([c for c in listed if '\\windowsapps\\python.exe' in c.lower().replace('/', '\\')], listed)

    @unittest.skipUnless(os.name == 'nt', 'Windows only')
    def test_windows_lists_the_registered_pythons(self):
        import winreg
        expected = []
        for root, view in ((winreg.HKEY_CURRENT_USER, 0), (winreg.HKEY_LOCAL_MACHINE, winreg.KEY_WOW64_64KEY),
                           (winreg.HKEY_LOCAL_MACHINE, winreg.KEY_WOW64_32KEY)):
            try:
                core = winreg.OpenKey(root, r'Software\Python\PythonCore', 0, winreg.KEY_READ | view)
            except OSError:
                continue
            with core:
                for i in range(winreg.QueryInfoKey(core)[0]):
                    tag = winreg.EnumKey(core, i)
                    try:
                        major, minor = (int(x) for x in tag.split('-')[0].rstrip('t').split('.')[:2])
                        with winreg.OpenKey(core, tag + r'\InstallPath') as ip:
                            try:
                                exe = winreg.QueryValueEx(ip, 'ExecutablePath')[0]
                            except OSError:
                                exe = os.path.join(winreg.QueryValueEx(ip, '')[0], 'python.exe')
                    except (OSError, ValueError):
                        continue
                    if major == 3 and minor >= 6 and exe:
                        expected.append(os.path.normcase(exe))
        if not expected:
            self.skipTest('nessun Python 3.6+ registrato (PEP 514)')
        listed, _ = self.candidates(PATH='')
        listed = [os.path.normcase(c) for c in listed]
        for exe in expected:
            self.assertIn(exe, listed)

if __name__ == '__main__':
    unittest.main()

# SPDX-License-Identifier: GPL-3.0-or-later
"""How the plugin starts its Python child on Linux (src/common/Child.cpp): what the child inherits from Resolve."""
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from plugin_build import test_bin  # noqa: E402


@unittest.skipUnless(sys.platform.startswith('linux'), 'Linux only')
class LinuxChild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = test_bin('common_test')
        if not cls.exe:
            raise unittest.SkipTest('binari di test non compilabili qui')

    def run_child(self, argv, env=None, pass_fds=()):
        out = subprocess.run([self.exe, 'runout', '10000'] + argv, capture_output=True, text=True, timeout=30,
                             env=dict(os.environ if env is None else env), pass_fds=pass_fds)
        self.assertEqual(out.returncode, 0, out.stderr)
        return out.stdout

    def test_the_hosts_descriptors_do_not_reach_the_child(self):
        with tempfile.TemporaryFile() as leaked:
            fd = os.dup2(leaked.fileno(), 57, inheritable=True)   # what a host's own socket looks like to a fork;
            self.addCleanup(os.close, fd)                         # 57: python's own handles take 3 and 4
            seen = self.run_child(['/usr/bin/env', 'python3', '-c',
                                   'import os; print(sorted(int(n) for n in os.listdir("/proc/self/fd")))'],
                                  pass_fds=(fd,))
        opened = eval(seen.strip())
        self.assertNotIn(fd, opened, 'fd %d leaked into the child: %s' % (fd, opened))
        self.assertTrue(set(opened) <= {0, 1, 2, 3}, opened)   # 0-2 plus python's own directory handle

    def test_resolves_own_libraries_are_not_passed_on(self):
        env = dict(os.environ, LD_LIBRARY_PATH='/opt/resolve/libs:/usr/local/lib/mine:/opt/DaVinciResolve/x')
        self.assertEqual(self.run_child(['/usr/bin/printenv', 'LD_LIBRARY_PATH'], env).strip(), '/usr/local/lib/mine')

    def test_nothing_left_means_no_variable(self):
        env = dict(os.environ, LD_LIBRARY_PATH='/opt/resolve/libs:/opt/resolve/bin')
        env.pop('LD_PRELOAD', None)
        out = subprocess.run([self.exe, 'runout', '10000', '/usr/bin/env'], capture_output=True, text=True,
                             timeout=30, env=env).stdout
        self.assertNotIn('LD_LIBRARY_PATH', out)

    def test_ld_preload_is_dropped(self):
        env = dict(os.environ, LD_PRELOAD='/nonexistent-preload.so')
        out = subprocess.run([self.exe, 'runout', '10000', '/usr/bin/env'], capture_output=True, text=True,
                             timeout=30, env=env).stdout
        self.assertNotIn('LD_PRELOAD', out)

    def test_which_looks_on_the_path_and_in_the_usual_folders(self):
        sh = subprocess.run([self.exe, 'which', 'sh'], capture_output=True, text=True, timeout=10).stdout.strip()
        self.assertTrue(sh.endswith('/sh'), sh)
        bare = dict(os.environ, PATH='')                      # a host started with an empty PATH
        sh = subprocess.run([self.exe, 'which', 'sh'], capture_output=True, text=True, timeout=10, env=bare).stdout.strip()
        self.assertTrue(sh.endswith('/sh'), sh)
        none = subprocess.run([self.exe, 'which', 'no-such-tool-xyz'], capture_output=True, text=True,
                              timeout=10).stdout.strip()
        self.assertEqual(none, '')

    def test_python_and_pythonpath_variables_are_dropped(self):
        env = dict(os.environ, PYTHONPATH='/x', PYTHONHOME='/y')
        out = subprocess.run([self.exe, 'runout', '10000', '/usr/bin/env'], capture_output=True, text=True,
                             timeout=30, env=env).stdout
        self.assertNotIn('PYTHONPATH', out)
        self.assertNotIn('PYTHONHOME', out)


    def open_url(self, script):
        with tempfile.TemporaryDirectory() as tmp:
            opener = os.path.join(tmp, 'xdg-open')
            with open(opener, 'w') as fh:
                fh.write('#!/bin/sh\n' + script + '\n')
            os.chmod(opener, 0o755)
            env = dict(os.environ, PATH=tmp + os.pathsep + os.environ.get('PATH', ''))
            out = subprocess.run([self.exe, 'openurl', 'https://example.com/x'], capture_output=True, text=True,
                                 timeout=30, env=env)
            return out.stdout.strip()

    def test_open_url_reports_a_failing_xdg_open(self):
        self.assertEqual(self.open_url('exit 4'), 'opened=0')   # no handler, no display

    def test_open_url_leaves_a_foreground_browser_running(self):
        self.assertEqual(self.open_url('exit 0'), 'opened=1')
        self.assertEqual(self.open_url('exec sleep 4'), 'opened=1')   # xdg-open waiting on the browser


if __name__ == '__main__':
    unittest.main()

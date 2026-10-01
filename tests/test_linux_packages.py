# SPDX-License-Identifier: GPL-3.0-or-later
"""packaging/linux/build_packages.py: the four formats built from a stand-in plugin bundle."""
import hashlib
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILDER = os.path.join(ROOT, 'packaging', 'linux', 'build_packages.py')
sys.path.insert(0, ROOT)
from slogmetaraw import __version__  # noqa: E402

LINUX = sys.platform.startswith('linux')


def fake_bundle(parent):
    bundle = os.path.join(parent, 'SLogMetaRaw.ofx.bundle')
    os.makedirs(os.path.join(bundle, 'Contents', 'Linux-x86-64'))
    os.makedirs(os.path.join(bundle, 'Contents', 'Resources'))
    with open(os.path.join(bundle, 'Contents', 'Linux-x86-64', 'SLogMetaRaw.ofx'), 'wb') as fh:
        fh.write(b'\x7fELF stand-in')
    with open(os.path.join(bundle, 'Contents', 'Resources', 'icon.png'), 'wb') as fh:
        fh.write(b'png')
    return bundle


@unittest.skipUnless(LINUX, 'the packages are built on Linux')
class Packages(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp()
        cls.bundle = fake_bundle(cls.tmp)
        cls.out = os.path.join(cls.tmp, 'out')

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def build(self, only, out=None):
        out = out or self.out
        run = subprocess.run([sys.executable, BUILDER, '--bundle', self.bundle, '--out', out, '--only', only],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        return run.stdout.split()

    def test_tar_has_the_installer_and_everything_it_installs(self):
        (path,) = self.build('tar')
        with tarfile.open(path) as tar:
            names = tar.getnames()
        top = 'SLogMetaRaw-%s-linux-%s' % (__version__, os.uname().machine)
        for need in ('install.sh', 'slogmetaraw/__init__.py', 'tools/render_launcher.py', 'resolve_script/SLogMetaRaw.py',
                     'SLogMetaRaw.ofx.bundle/Contents/Linux-x86-64/SLogMetaRaw.ofx', 'LICENSE'):
            self.assertIn('%s/%s' % (top, need), names)
        self.assertFalse([n for n in names if '__pycache__' in n or n.endswith('.pyc')])

    def test_the_same_input_gives_the_same_bytes(self):
        a = self.build('tar', os.path.join(self.tmp, 'a'))[0]
        b = self.build('tar', os.path.join(self.tmp, 'b'))[0]
        def digest(path):
            with open(path, 'rb') as fh:
                return hashlib.sha256(fh.read()).hexdigest()
        self.assertEqual(digest(a), digest(b))

    def test_run_unpacks_and_installs_for_a_user(self):
        run = [p for p in self.build('run') if p.endswith('.run')][0]
        self.assertTrue(os.access(run, os.X_OK))
        with open(run, 'rb') as fh:
            self.assertEqual(fh.readline(), b'#!/bin/sh\n')
        home = os.path.join(self.tmp, 'home')
        os.makedirs(home)
        env = dict(os.environ, HOME=home)
        env.pop('XDG_DATA_HOME', None)
        out = subprocess.run(['sh', run, '--user'], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             universal_newlines=True)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
        plugin = os.path.join(home, '.local/share/OFX/Plugins/SLogMetaRaw.ofx.bundle/Contents/Linux-x86-64/SLogMetaRaw.ofx')
        self.assertTrue(os.path.exists(plugin))
        launcher = os.path.join(home, '.local/share/DaVinciResolve/Fusion/Scripts/Utility/S-Log MetaRaw.py')
        self.assertTrue(os.path.exists(launcher))
        out = subprocess.run(['sh', run, '--uninstall', '--user'], env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, universal_newlines=True)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
        self.assertFalse(os.path.exists(plugin))
        self.assertFalse(os.path.exists(launcher))

    def test_run_can_only_extract(self):
        run = [p for p in self.build('run') if p.endswith('.run')][0]
        target = os.path.join(self.tmp, 'extracted')
        subprocess.run(['sh', run, '--extract', target], check=True, stdout=subprocess.DEVNULL)
        top = os.listdir(target)[0]
        self.assertTrue(os.path.exists(os.path.join(target, top, 'install.sh')))

    @unittest.skipUnless(shutil.which('dpkg-deb'), 'dpkg-deb not installed')
    def test_deb(self):
        (path,) = self.build('deb')
        info = subprocess.run(['dpkg-deb', '-f', path], stdout=subprocess.PIPE, universal_newlines=True).stdout
        self.assertIn('Package: slogmetaraw', info)
        self.assertIn('Version: %s' % __version__, info)
        self.assertIn('Depends: python3', info)
        listing = subprocess.run(['dpkg-deb', '-c', path], stdout=subprocess.PIPE, universal_newlines=True).stdout
        for need in ('./usr/OFX/Plugins/SLogMetaRaw.ofx.bundle/Contents/Linux-x86-64/SLogMetaRaw.ofx',
                     './usr/lib/slogmetaraw/slogmetaraw/__init__.py', './usr/bin/slogmetaraw-setup',
                     './usr/share/slogmetaraw/launcher/S-Log MetaRaw.py'):
            self.assertIn(need, listing)
        self.assertIn('root/root', listing)          # not owned by whoever built it

    @unittest.skipUnless(shutil.which('rpmbuild') and shutil.which('rpm'), 'rpm not installed')
    def test_rpm(self):
        (path,) = self.build('rpm')
        files = subprocess.run(['rpm', '-qpl', path], stdout=subprocess.PIPE, universal_newlines=True).stdout
        for need in ('/usr/OFX/Plugins/SLogMetaRaw.ofx.bundle/Contents/Linux-x86-64/SLogMetaRaw.ofx',
                     '/usr/lib/slogmetaraw/slogmetaraw/__init__.py', '/usr/bin/slogmetaraw-setup'):
            self.assertIn(need, files)
        requires = subprocess.run(['rpm', '-qpR', path], stdout=subprocess.PIPE, universal_newlines=True).stdout
        self.assertIn('python3', requires)
        self.assertNotIn('>= 3.8', requires)         # Rocky/RHEL 8 have Python 3.6


if __name__ == '__main__':
    unittest.main()

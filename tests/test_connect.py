# SPDX-License-Identifier: GPL-3.0-or-later
"""Connection and single-clip write used by the plugin's --to-resolve child."""
import os
import sys
import tempfile
import unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from slogmetaraw import connect


class LocalAddresses(unittest.TestCase):
    def test_parses_ifconfig_and_filters_loopback(self):
        output = ('lo0: flags=8049<UP,LOOPBACK,RUNNING> mtu 16384\n'
                  '\tinet 127.0.0.1 netmask 0xff000000\n'
                  'en0: flags=8863<UP,BROADCAST> mtu 1500\n'
                  '\tinet 192.168.1.11 netmask 0xffffff00 broadcast 192.168.1.255\n'
                  '\tinet6 fe80::1%en0 prefixlen 64\n'
                  'utun3: flags=8051<UP,POINTOPOINT> mtu 1380\n'
                  '\tinet 0.0.0.0 --> 0.0.0.0\n'
                  '\tinet 10.9.0.5 --> 10.9.0.5 netmask 0xffffffff\n'
                  '\tinet 192.168.1.11 netmask 0xffffff00\n')
        with mock.patch('subprocess.run', return_value=mock.Mock(stdout=output, returncode=0)):
            self.assertEqual(connect._local_ipv4_addresses(), ['192.168.1.11', '10.9.0.5'])

    def test_ifconfig_failure_gives_no_addresses(self):
        with mock.patch('subprocess.run', side_effect=OSError('no ifconfig')):
            self.assertEqual(connect._local_ipv4_addresses(), [])

    def test_linux_reads_ip_and_falls_back_to_ifconfig(self):
        ip = ('1: lo    inet 127.0.0.1/8 scope host lo\\       valid_lft forever\n'
              '2: eth0    inet 192.168.1.20/24 brd 192.168.1.255 scope global eth0\\       valid_lft 1d\n')
        net_tools = 'eth0      Link encap:Ethernet\n          inet addr:10.1.2.3  Bcast:10.1.2.255\n'
        with mock.patch.object(connect.sys, 'platform', 'linux'):
            with mock.patch('subprocess.run', return_value=mock.Mock(stdout=ip, returncode=0)) as run:
                self.assertEqual(connect._local_ipv4_addresses(), ['192.168.1.20'])
            self.assertEqual(run.call_args.args[0][0], 'ip')
            with mock.patch('subprocess.run', side_effect=[OSError('no ip'), mock.Mock(stdout=net_tools)]) as run:
                self.assertEqual(connect._local_ipv4_addresses(), ['10.1.2.3'])
            self.assertEqual(run.call_args.args[0], ['/sbin/ifconfig', '-a'])

    def test_windows_uses_the_default_route_and_the_host_name_without_subprocesses(self):
        sock = mock.MagicMock()
        sock.__enter__.return_value.getsockname.return_value = ('192.168.1.30', 50000)
        with mock.patch.object(connect.sys, 'platform', 'win32'), \
                mock.patch('subprocess.run') as run, \
                mock.patch.object(connect.socket, 'socket', return_value=sock), \
                mock.patch.object(connect.socket, 'gethostbyname_ex',
                                  return_value=('pc', [], ['127.0.0.1', '192.168.1.30', '172.20.0.1'])):
            self.assertEqual(connect._local_ipv4_addresses(), ['192.168.1.30', '172.20.0.1'])
        run.assert_not_called()


class Connect(unittest.TestCase):
    def fake_bmd(self, results):
        module = mock.Mock()
        calls = []

        def scriptapp(*args):
            calls.append(args)
            return results.pop(0) if results else None

        module.scriptapp = scriptapp
        return module, calls

    def test_localhost_connection_wins(self):
        resolve = object()
        bmd, _ = self.fake_bmd([resolve])
        with mock.patch.dict(sys.modules, {'DaVinciResolveScript': bmd}):
            self.assertIs(connect.connect(), resolve)

    def test_falls_back_to_local_ipv4_addresses(self):
        resolve = object()
        bmd, calls = self.fake_bmd([None, resolve])
        with mock.patch.dict(sys.modules, {'DaVinciResolveScript': bmd}):
            with mock.patch.object(connect, '_local_ipv4_addresses', return_value=['192.168.1.11', '10.9.0.5']):
                self.assertIs(connect.connect(), resolve)
        self.assertEqual(calls, [('Resolve',), ('Resolve', '192.168.1.11', 1.0)])

    def test_timeout_is_shared_by_the_fallback_addresses(self):
        bmd, calls = self.fake_bmd([None, None, None])
        clock = iter([100.0, 100.0, 101.0, 101.5])
        with mock.patch.dict(sys.modules, {'DaVinciResolveScript': bmd}), \
                mock.patch.object(connect.time, 'monotonic', lambda: next(clock)), \
                mock.patch.object(connect, '_local_ipv4_addresses', return_value=['10.0.0.1', '10.0.0.2', '10.0.0.3']):
            with self.assertRaisesRegex(RuntimeError, 'connessione'):
                connect.connect(1.5)
        self.assertEqual(calls, [('Resolve',), ('Resolve', '10.0.0.1', 1.0), ('Resolve', '10.0.0.2', 0.5)])

    def test_connection_failure_raises(self):
        bmd, _ = self.fake_bmd([None, None])
        with mock.patch.dict(sys.modules, {'DaVinciResolveScript': bmd}):
            with mock.patch.object(connect, '_local_ipv4_addresses', return_value=['192.168.1.11']):
                with self.assertRaisesRegex(RuntimeError, 'connessione'):
                    connect.connect()

    def test_missing_module_raises(self):
        with mock.patch('builtins.__import__', side_effect=ImportError('no module')):
            with self.assertRaisesRegex(RuntimeError, 'DaVinciResolveScript'):
                connect.connect()


class ScriptingModule(unittest.TestCase):
    """A python3 that Resolve did not start (Linux, Windows) finds DaVinciResolveScript in Resolve's own folder."""

    def setUp(self):
        self.saved_path = list(sys.path)
        self.saved_module = sys.modules.pop('DaVinciResolveScript', None)
        self.addCleanup(self.restore)

    def restore(self):
        sys.path[:] = self.saved_path
        sys.modules.pop('DaVinciResolveScript', None)
        if self.saved_module is not None:
            sys.modules['DaVinciResolveScript'] = self.saved_module

    def test_the_folder_comes_from_resolve_script_api(self):
        import tempfile
        with tempfile.TemporaryDirectory() as api:
            os.makedirs(os.path.join(api, 'Modules'))
            with open(os.path.join(api, 'Modules', 'DaVinciResolveScript.py'), 'w') as fh:
                fh.write('MARK = 42\n')
            with mock.patch.dict(os.environ, {'RESOLVE_SCRIPT_API': api}):
                self.assertIn(os.path.join(api, 'Modules'), connect.scripting_module_dirs())
                module = connect._resolve_script_module()
            self.assertEqual(module.MARK, 42)

    def test_default_folders_per_system(self):
        def default(platform, name):
            with mock.patch.object(connect.sys, 'platform', platform), mock.patch.object(connect.os, 'name', name), \
                    mock.patch.object(connect.os.path, 'isdir', lambda p: True), \
                    mock.patch.dict(os.environ, {}):
                os.environ.pop('RESOLVE_SCRIPT_API', None)
                return connect.scripting_module_dirs()
        self.assertEqual(default('linux', 'posix'), ['/opt/resolve/Developer/Scripting/Modules'])
        self.assertTrue(default('darwin', 'posix')[0].startswith('/Library/Application Support/Blackmagic Design'))
        self.assertIn('Scripting', default('win32', 'nt')[0])

    def test_a_folder_that_does_not_exist_is_not_added(self):
        before = list(sys.path)
        with mock.patch.dict(os.environ, {'RESOLVE_SCRIPT_API': '/nonexistent/api'}):
            self.assertEqual([d for d in connect.scripting_module_dirs() if 'nonexistent' in d], [])
        self.assertEqual(sys.path, before)


class ApplyPath(unittest.TestCase):
    def make_resolve(self, clip_paths):
        clips = []
        for path in clip_paths:
            clip = mock.Mock()
            clip.GetClipProperty.return_value = path
            clips.append(clip)
        root = mock.Mock()
        root.GetClipList.return_value = clips
        root.GetSubFolderList.return_value = []
        pool = mock.Mock()
        pool.GetRootFolder.return_value = root
        project = mock.Mock()
        project.GetMediaPool.return_value = pool
        resolve = mock.Mock()
        resolve.GetProjectManager.return_value.GetCurrentProject.return_value = project
        return resolve, clips

    def test_writes_to_matching_clip_with_script_defaults(self):
        resolve, clips = self.make_resolve(['/Volumes/Drive/shot.MP4'])
        r = {'meta': {}, 'display': {}}
        with mock.patch('slogmetaraw.resolve_io.apply_to_clip',
                        return_value={'written': ['A', 'B'], 'failed': [], 'color_space': None}) as apply:
            report = connect.apply_path(resolve, '/Volumes/Drive/shot.MP4', r)
        self.assertEqual(report, {'written': 2, 'failed': 0, 'clips': 1, 'data_level': {}})
        apply.assert_called_once_with(clips[0], r, set_color_space=False, overwrite=True, add_tags=False,
                                     set_data_level=True)

    def test_matches_real_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            real = os.path.join(directory, 'clip.MP4')
            with open(real, 'w') as fh:
                fh.write('x')
            link = os.path.join(directory, 'alias.MP4')
            os.symlink(real, link)
            resolve, _ = self.make_resolve([link])
            r = {'meta': {}, 'display': {}}
            with mock.patch('slogmetaraw.resolve_io.apply_to_clip',
                            return_value={'written': ['A'], 'failed': [], 'color_space': None}) as apply:
                report = connect.apply_path(resolve, real, r)
        self.assertEqual(report['clips'], 1)
        apply.assert_called_once()

    def test_clip_not_in_pool_raises(self):
        resolve, _ = self.make_resolve(['/other/clip.MP4'])
        with self.assertRaisesRegex(RuntimeError, 'Media Pool'):
            connect.apply_path(resolve, '/wanted/clip.MP4', {'meta': {}, 'display': {}})

    def test_no_project_raises(self):
        resolve = mock.Mock()
        resolve.GetProjectManager.return_value.GetCurrentProject.return_value = None
        with self.assertRaisesRegex(RuntimeError, 'progetto'):
            connect.apply_path(resolve, '/x.MP4', {'meta': {}, 'display': {}})


if __name__ == '__main__':
    unittest.main()

# SPDX-License-Identifier: GPL-3.0-or-later
"""The reader must work where os.pread, signal.alarm and os.fork do not exist (Windows). The tests take those
functions away on any system, so the Windows paths run in every CI."""
import os
import sys
import tempfile
import types
import unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from slogmetaraw import __main__ as cli, mp4  # noqa: E402


class NoPread(unittest.TestCase):
    def setUp(self):
        self.saved = getattr(os, 'pread', None)
        if self.saved is not None:
            del os.pread
        self.addCleanup(self._restore)

    def _restore(self):
        if self.saved is not None:
            os.pread = self.saved

    def test_positional_reads_without_pread(self):
        data = bytes(range(256)) * 40
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'x.bin')
            with open(path, 'wb') as fh:
                fh.write(data)
            with mp4.CountingFile(path) as f:
                self.assertFalse(hasattr(os, 'pread'))
                self.assertEqual(f.read_at(5, 10), data[5:15])
                self.assertEqual(f.read_at(len(data) - 4, 100), data[-4:])   # clamped to the end
                self.assertEqual(f.read_at(0, 3), data[:3])                  # seeks back
                f.preload(100, 64)
                self.assertEqual(f.read_at(110, 8), data[110:118])           # served from the window


class NoAlarm(unittest.TestCase):
    def test_a_timer_thread_replaces_sigalrm(self):
        fake_signal = types.SimpleNamespace()          # no SIGALRM, no alarm
        with mock.patch.object(cli, 'signal', fake_signal), mock.patch.object(cli.threading, 'Timer') as timer:
            cli._alarm(7)
        timer.assert_called_once()
        self.assertEqual(timer.call_args.args[0], 7)
        self.assertTrue(timer.return_value.daemon)
        timer.return_value.start.assert_called_once()

    def test_sigalrm_is_used_when_there_is(self):
        fake_signal = types.SimpleNamespace(SIGALRM=14, alarm=mock.Mock())
        with mock.patch.object(cli, 'signal', fake_signal):
            cli._alarm(3)
        fake_signal.alarm.assert_called_once_with(3)


class NoFork(unittest.TestCase):
    def test_the_writer_is_a_detached_process(self):
        with mock.patch.object(cli.subprocess, 'Popen') as popen:
            cli._spawn_resolve_writer('/clips/A001.MP4')
        argv = popen.call_args.args[0]
        self.assertEqual(argv[0], sys.executable)
        self.assertEqual(argv[-2:], ['--resolve-writer', '/clips/A001.MP4'])
        self.assertEqual(os.path.normpath(argv[-3]), os.path.normpath(ROOT))   # the library folder
        self.assertEqual(popen.call_args.kwargs['stdout'], cli.subprocess.DEVNULL)
        self.assertTrue(popen.call_args.kwargs['creationflags'])

    def test_to_resolve_uses_it_when_fork_is_missing(self):
        with tempfile.TemporaryDirectory() as d, \
                mock.patch.object(cli, 'read_clip', return_value={'path': 'x'}), \
                mock.patch('slogmetaraw.plugin_cache.write_cache'), \
                mock.patch('slogmetaraw.plugin_cache.resolve_status_path', return_value=os.path.join(d, 's.json')), \
                mock.patch.object(cli, '_spawn_resolve_writer') as spawn, \
                mock.patch.object(cli, '_emit'):
            fork = getattr(os, 'fork', None)
            if fork is not None:
                del os.fork
            try:
                self.assertEqual(cli.to_resolve([os.path.join(d, 'A.MP4')]), 0)
            finally:
                if fork is not None:
                    os.fork = fork
        spawn.assert_called_once()


if __name__ == '__main__':
    unittest.main()

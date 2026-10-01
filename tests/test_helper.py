# SPDX-License-Identifier: GPL-3.0-or-later
"""The --helper mode feeds the script window: one path in, one JSON line out."""
import json
import os
import subprocess
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class Helper(unittest.TestCase):
    def run_helper(self, lines):
        proc = subprocess.run([sys.executable, '-m', 'slogmetaraw', '--helper'],
                              input='\n'.join(lines) + '\n', capture_output=True,
                              text=True, cwd=ROOT, timeout=60)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return [json.loads(l) for l in proc.stdout.splitlines()]

    def test_missing_file_reports_error(self):
        out = self.run_helper(['/percorso/inesistente.MP4'])
        self.assertEqual(len(out), 1)
        self.assertFalse(out[0]['ok'])
        self.assertEqual(out[0]['kind'], 'error')
        self.assertIn('inesistente', out[0]['error'])

    def test_blank_lines_are_ignored(self):
        out = self.run_helper(['', '   '])
        self.assertEqual(out, [])

    def test_accented_path_under_c_locale(self):
        # Resolve's embedded Python may run under the C locale (ASCII pipes): #39
        env = dict(os.environ, LC_ALL='C', LANG='C', PYTHONUTF8='0', PYTHONCOERCECLOCALE='0')
        path = '/percorso/inesistente/Caff\u00e9 \u00e8.MP4'
        proc = subprocess.run([sys.executable, '-X', 'utf8=0', '-m', 'slogmetaraw', '--helper'],
                              input=(path + '\n').encode('utf-8'), capture_output=True,
                              cwd=ROOT, env=env, timeout=60)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        msg = json.loads(proc.stdout.splitlines()[0])
        self.assertEqual(msg['path'], path)
        self.assertFalse(msg['ok'])


if __name__ == '__main__':
    unittest.main()

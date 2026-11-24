# SPDX-License-Identifier: GPL-3.0-or-later
"""Deep acquisition (clipdb.py): the reader runs mecena-clipdb and reports the MecenaClipDB entry.

mecena-clipdb itself (Rust, GyroMotion) is replaced by a stand-in that writes index.json as it does."""
import json
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import clip_fixtures as cf  # noqa: E402
from slogmetaraw import clipdb  # noqa: E402

FAKE = textwrap.dedent('''
    import json, os, sys
    _cmd, db, clip = sys.argv[1:4]
    if 'NOGYRO' in clip:
        sys.exit('[1/1] %s: nessun dato movimento' % os.path.basename(clip))
    os.makedirs(os.path.join(db, 'clips', 'C0001-abc'), exist_ok=True)
    entry = {'id': 'C0001-abc', 'path': os.path.realpath(clip), 'file_name': os.path.basename(clip),
             'fps': 25.0, 'frame_count': 10, 'has_gyro': True, 'summary': 'Sony ILME-FX30 · IMU 2000 Hz'}
    with open(os.path.join(db, 'index.json'), 'w') as fh:
        json.dump({'format': 'mecena-clipdb', 'version': 1, 'clips': [entry], 'links': []}, fh)
''')


class ClipDb(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = cls.tmp.name
        cls.clip = os.path.join(d, 'C0001.MP4')
        cf.build_mp4(cls.clip, frames=10, free=1024)
        cls.nogyro = os.path.join(d, 'NOGYRO.MP4')
        cf.build_mp4(cls.nogyro, frames=10, free=1024)
        script = os.path.join(d, 'fake_clipdb.py')
        with open(script, 'w') as fh:
            fh.write(FAKE)
        if os.name == 'nt':
            cls.tool = os.path.join(d, 'mecena-clipdb.cmd')
            with open(cls.tool, 'w') as fh:
                fh.write('@"%s" "%s" %%*\n' % (sys.executable, script))
        else:
            cls.tool = os.path.join(d, 'mecena-clipdb')
            with open(cls.tool, 'w') as fh:
                fh.write('#!/bin/sh\nexec "%s" "%s" "$@"\n' % (sys.executable, script))
            os.chmod(cls.tool, 0o755)
        cls.db = os.path.join(d, 'MecenaClipDB')

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_helper(self, paths, tool):
        env = dict(os.environ, MECENA_CLIPDB=self.db, MECENA_CLIPDB_BIN=tool)
        proc = subprocess.run([sys.executable, '-m', 'slogmetaraw', '--helper', '--deep'],
                              input='\n'.join(paths) + '\n', capture_output=True,
                              text=True, cwd=ROOT, env=env, timeout=60)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return [json.loads(l)['result'] for l in proc.stdout.splitlines()]

    def test_scan_reports_the_database_entry(self):
        r, = self.run_helper([self.clip], self.tool)
        self.assertEqual(r['motion']['summary'], 'Sony ILME-FX30 · IMU 2000 Hz')
        self.assertTrue(r['motion']['has_gyro'])
        self.assertEqual(r['motion']['folder'], os.path.join(self.db, 'clips', 'C0001-abc'))
        self.assertIn(clipdb.SECTION, [title for title, _rows in r['sections']])

    def test_clip_without_motion_data_is_a_warning(self):
        r, = self.run_helper([self.nogyro], self.tool)
        self.assertNotIn('motion', r)
        self.assertEqual(r['warning_keys'][-1][0], 'Acquisizione profonda: nessun dato di movimento (%s).')
        self.assertIn('nessun dato movimento', r['warnings'][-1])

    def test_without_mecena_motion(self):
        env = dict(os.environ, MECENA_CLIPDB_BIN=os.path.join(self.tmp.name, 'assente'), PATH=self.tmp.name + '/vuota')
        old = dict(os.environ)
        os.environ.update(env)
        try:
            if os.path.isfile(clipdb.BUNDLE_BIN):
                self.skipTest('Mecena Motion installato su questa macchina')
            self.assertIsNone(clipdb.tool())
        finally:
            os.environ.clear()
            os.environ.update(old)

    def test_database_default_and_override(self):
        old = os.environ.get('MECENA_CLIPDB')
        try:
            os.environ['MECENA_CLIPDB'] = os.pathsep.join(['/a/MecenaClipDB', '/b/MecenaClipDB'])
            self.assertEqual(clipdb.database(), '/a/MecenaClipDB')
        finally:
            if old is None:
                os.environ.pop('MECENA_CLIPDB', None)
            else:
                os.environ['MECENA_CLIPDB'] = old


if __name__ == '__main__':
    unittest.main()

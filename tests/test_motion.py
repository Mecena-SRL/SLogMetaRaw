# SPDX-License-Identifier: GPL-3.0-or-later
"""Deep acquisition (motion.py): every frame's IMU readings, Gyroflow log and lens table."""
import json
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import clip_fixtures as cf  # noqa: E402
from slogmetaraw import motion  # noqa: E402

FRAMES = 10


class Motion(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = cls.tmp.name
        cls.mp4 = os.path.join(d, 'C0001.MP4')
        cf.build_mp4(cls.mp4, frames=FRAMES, free=1024, imu=True)
        cls.mxf = os.path.join(d, 'C0002.MXF')
        cf.build_mxf(cls.mxf, frames=FRAMES, picture=2048, header_fill=4096, imu=True)
        cls.plain = os.path.join(d, 'C0003.MP4')
        cf.build_mp4(cls.plain, frames=FRAMES, free=1024)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def check(self, m):
        self.assertEqual((m['frames'], m['frames_read'], m['partial']), (FRAMES, FRAMES, False))
        self.assertEqual(m['gyro_samples'], FRAMES * cf.IMU_PER_FRAME)
        self.assertEqual(m['accel_samples'], FRAMES * cf.IMU_PER_FRAME // 2)
        self.assertEqual(m['gyro'][7], [(7, j, -7) for j in range(cf.IMU_PER_FRAME)])
        self.assertEqual((m['gyro_hz'], m['accel_hz'], m['orientation'], m['readout_ms']), (100, 50, 'xZY', 15.6))
        self.assertAlmostEqual(m['gyro_scale'], 16.4, places=5)
        self.assertIn('iris_fnumber', m['lens'][0])

    def test_mp4_every_frame(self):
        self.check(motion.read(self.mp4, fps=25))

    def test_mxf_every_frame(self):
        self.check(motion.read(self.mxf, fps=25))

    def test_clip_without_imu(self):
        self.assertIsNone(motion.read(self.plain, fps=25))

    def test_deadline_marks_partial(self):
        m = motion.read(self.mp4, fps=25, deadline=0)
        self.assertTrue(m is None or m['partial'])

    def test_orientation(self):
        self.assertEqual(motion.orientation(0x420), 'XYZ')   # A7S III
        self.assertEqual(motion.orientation(0x152), 'Yzx')   # RX100 VII
        self.assertEqual(motion.gyroflow_orientation('XYZ'), 'YXz')

    def test_gcsv(self):
        text = motion.gcsv(motion.read(self.mp4, fps=25))
        lines = text.splitlines()
        self.assertEqual(lines[0], 'GYROFLOW IMU LOG')
        header = dict(l.split(',', 1) for l in lines[1:lines.index('t,gx,gy,gz,ax,ay,az')])
        self.assertEqual(header['orientation'], 'Zxy')
        self.assertEqual(header['tscale'], '0.000001')
        self.assertAlmostEqual(float(header['gscale']), 16.4, places=5)
        self.assertEqual(header['frame_readout_time'], '15.6')
        rows = lines[lines.index('t,gx,gy,gz,ax,ay,az') + 1:]
        self.assertEqual(len(rows), FRAMES * cf.IMU_PER_FRAME)
        # frame 1, reading 2: 1/25 s + 2/4 of a frame = 60 ms
        self.assertEqual(rows[cf.IMU_PER_FRAME + 2], '60000,1,2,-1,0,0,4096')

    def test_lens_csv(self):
        rows = motion.lens_csv(motion.read(self.mp4, fps=25)).splitlines()
        self.assertEqual(rows[0], 'frame,focus_m,focal_mm,iris_f')
        self.assertEqual(len(rows), FRAMES + 1)
        self.assertTrue(rows[1].startswith('0,,,5.68'))

    def test_helper_deep_writes_files(self):
        env = dict(os.environ, HOME=self.tmp.name, XDG_DATA_HOME=os.path.join(self.tmp.name, 'data'),
                   APPDATA=os.path.join(self.tmp.name, 'appdata'))
        proc = subprocess.run([sys.executable, '-m', 'slogmetaraw', '--helper', '--deep'],
                              input=self.mp4 + '\n' + self.plain + '\n', capture_output=True,
                              text=True, cwd=ROOT, env=env, timeout=60)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        deep, plain = [json.loads(l) for l in proc.stdout.splitlines()]
        s = deep['result']['motion']
        self.assertEqual(s['gyro_samples'], FRAMES * cf.IMU_PER_FRAME)
        for path in s['files'].values():
            self.assertTrue(os.path.isfile(path), path)
            self.assertTrue(path.startswith(self.tmp.name))
        self.assertIn(motion.SECTION, [title for title, _rows in deep['result']['sections']])
        self.assertNotIn('motion', plain['result'])


if __name__ == '__main__':
    unittest.main()

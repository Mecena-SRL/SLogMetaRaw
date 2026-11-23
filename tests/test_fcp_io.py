# SPDX-License-Identifier: GPL-3.0-or-later
"""FCPXML writer (beta): structure, rational times, escaping, DTD validation when Final Cut Pro is installed."""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from slogmetaraw import fcp_io  # noqa: E402

FCP_DTD = '/Applications/Final Cut Pro.app/Contents/Frameworks/Interchange.framework/Versions/A/Resources/FCPXMLv1_15.dtd'


def _result(path='/media/A & B/C0001.MP4', fps=29.97, frames=300):
    return {'path': path, 'display': {}, 'warnings': [], 'changes': {},
            'meta': {'v_width': 3840, 'v_height': 2160, 'fps': fps, 'frames': frames, 'manufacturer': 'Sony',
                     'model': 'ILME-FX30', 'iso': 800, 'white_balance_k': 5600, 'gamma_name': 'S-Log3',
                     'color_primaries': 'S-Gamut3.Cine', 'color_space': 'S-Gamut3.Cine/S-Log3'}}


class FcpIoTest(unittest.TestCase):
    def test_frame_duration(self):
        self.assertEqual(fcp_io.frame_duration(25), (1, 25))
        self.assertEqual(fcp_io.frame_duration(29.97), (1001, 30000))
        self.assertEqual(fcp_io.frame_duration(23.976), (1001, 24000))
        self.assertEqual(fcp_io.frame_duration(59.94), (1001, 60000))

    def test_structure(self):
        root = ET.fromstring(fcp_io.build_fcpxml([_result(), _result('/media/D0002.MP4')], 'Test'))
        self.assertEqual(root.get('version'), '1.15')
        self.assertEqual(len(root.findall('resources/format')), 1)   # same geometry and rate: one format
        self.assertEqual(len(root.findall('resources/asset')), 2)
        clip = root.find('library/event/asset-clip')
        self.assertEqual(clip.get('duration'), '300300/30000s')
        self.assertIn('FX30', clip.find('keyword').get('value'))
        md = {e.get('key'): e.get('value') for e in clip.iter('md')}
        self.assertEqual(md['com.logmetaraw.iso'], '800')
        self.assertEqual(md['com.logmetaraw.white_point_k'], '5600')

    def test_path_is_url_escaped(self):
        root = ET.fromstring(fcp_io.build_fcpxml([_result()]))
        self.assertEqual(root.find('resources/asset/media-rep').get('src'), 'file:///media/A%20%26%20B/C0001.MP4')

    def test_clip_without_geometry_is_skipped(self):
        r = _result()
        r['meta']['v_width'] = None
        root = ET.fromstring(fcp_io.build_fcpxml([r]))
        self.assertEqual(root.findall('library/event/asset-clip'), [])

    def test_clip_without_duration_leaves_no_orphan_format(self):
        r = _result()
        r['meta']['frames'] = 0
        root = ET.fromstring(fcp_io.build_fcpxml([r]))
        self.assertEqual(root.findall('resources/format'), [])


    def test_effect_set_from_camera_values(self):
        r = _result()
        r['meta'].update({'capture_gamma': 'S-Log3', 'color_space': 'S-Gamut3.Cine/S-Log3', 'exposure_index': 800,
                          'white_balance_k': 4300})
        root = ET.fromstring(fcp_io.build_fcpxml([r]))
        self.assertEqual(root.find('resources/effect').get('uid'), fcp_io.EFFECT_UID)
        fv = root.find('library/event/asset-clip/filter-video')
        p = {e.get('name'): (e.get('key'), e.get('value')) for e in fv.iter('param')}
        self.assertEqual(p['Gamma'], ('9999/10140/10143/3/10145/200/210/212', '9 (SLog3)'))
        self.assertEqual(p['Spazio colore'][1], '8 (S-Gamut3.Cine)')
        self.assertEqual(p['Temperatura'][1], '4300')
        self.assertEqual(p['Kelvin di ripresa'][1], '4300')
        self.assertIn('FX30', p['Camera'][1])

    def test_no_effect_option(self):
        root = ET.fromstring(fcp_io.build_fcpxml([_result()], effect=False))
        self.assertIsNone(root.find('library/event/asset-clip/filter-video'))

    def test_apply_puts_effect_first_and_replaces_old(self):
        src = """<?xml version="1.0"?><fcpxml version="1.15"><resources>
<format id="r1" frameDuration="1/25s" width="1920" height="1080"/>
<asset id="r2" name="A" start="0s" duration="10s" hasVideo="1" format="r1"><media-rep kind="original-media" src="file:///media/A.MP4"/></asset>
<effect id="r9" name="Other" uid="FFOther"/></resources>
<library><event name="E"><project name="P"><sequence format="r1"><spine>
<asset-clip ref="r2" name="A" duration="10s"><filter-video ref="r9" name="Other"/>
<filter-video ref="r9" name="Log MetaRaw"/></asset-clip></spine></sequence></project></event></library></fcpxml>"""
        with tempfile.NamedTemporaryFile('w', suffix='.fcpxml', delete=False) as fh:
            fh.write(src)
        try:
            r = _result('/media/A.MP4')
            r['meta'].update({'capture_gamma': 'S-Log3', 'color_space': 'S-Gamut3.Cine/S-Log3'})
            text, applied, skipped = fcp_io.apply_to_fcpxml(fh.name, read=lambda p: r)
        finally:
            os.unlink(fh.name)
        root = ET.fromstring(text)
        filters = root.findall('.//asset-clip/filter-video')
        self.assertEqual((applied, skipped), (1, 0))
        self.assertEqual([f.get('name') for f in filters], ['Log MetaRaw', 'Other'])
        self.assertEqual(root.find('.//project').get('name'), 'P · Log MetaRaw')

    @unittest.skipUnless(os.path.exists(FCP_DTD) and shutil.which('xmllint'), 'Final Cut Pro DTD not available')
    def test_validates_against_final_cut_dtd(self):
        with tempfile.NamedTemporaryFile('w', suffix='.fcpxml', delete=False) as fh:
            fh.write(fcp_io.build_fcpxml([_result(), _result('/media/E.MP4', fps=25)]))
        try:
            p = subprocess.run(['xmllint', '--noout', '--dtdvalid', os.path.basename(FCP_DTD), fh.name],
                               cwd=os.path.dirname(FCP_DTD), capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, p.stderr)
        finally:
            os.unlink(fh.name)


if __name__ == '__main__':
    unittest.main()

# SPDX-License-Identifier: GPL-3.0-or-later
"""Load the built plugin in a miniature OpenFX host and run the actions Resolve runs.

This is the check that would have caught the two crashes we hit in DaVinci Resolve:
a duplicate parameter name, and a set of claimed names shared between the two
describeInContext calls (which left the second context with no parameters).
"""
import os
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
from plugin_build import BUNDLE_BINARY as BINARY, SUPPORT_REL, test_bin  # noqa: E402
sys.path.insert(0, ROOT)
from slogmetaraw import __version__  # noqa: E402

GOLDEN = {0: os.path.join(ROOT, 'tests', 'golden', 'params_develop.txt'),
          1: os.path.join(ROOT, 'tests', 'golden', 'params_detail.txt')}


class OfxHost(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = test_bin('host_test')
        if not cls.exe or not os.path.exists(BINARY):
            raise unittest.SkipTest('plugin o host di test non compilabili qui')

    def run_host(self, clip=None, env=None):
        child_env = dict(env if env is not None else os.environ)
        # The plugin's "Rileggi metadata" now also writes into Resolve: tests must
        # never touch a real Resolve, so the child skips the connection.
        child_env['SLOGMETARAW_TEST_NO_RESOLVE'] = '1'
        run = subprocess.run([self.exe, BINARY] + ([clip] if clip else []), capture_output=True, text=True,
                             env=child_env, timeout=20)
        report = run.stdout + run.stderr
        self.assertEqual(run.returncode, 0, 'il plugin non supera il caricamento:\n' + report)
        self.assertIn('OK', run.stdout, report)
        self.assertNotIn('DUPLICATO', report)
        return dict(line.split('=', 1) for line in run.stdout.splitlines() if '=' in line)

    def test_load_describe_and_both_contexts(self):
        self.run_host()

    def host(self, *args):
        run = subprocess.run([self.exe, BINARY] + list(args), capture_output=True, text=True, timeout=20,
                             env=dict(os.environ, SLOGMETARAW_TEST_NO_RESOLVE='1', SLOGMETARAW_NO_UPDATE_CHECK='1'))
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        return run.stdout

    def test_the_panels_match_the_golden_files(self):
        """Every parameter's name, type, label, default, parent and range, in order. A change here
        is a change of what Resolve saves with every grade: update the golden file on purpose."""
        for index, golden in GOLDEN.items():
            out = self.host('--plugin', str(index), '--dump-params', '--no-reload')
            got = [l.replace('|v%s|' % __version__, '|v{release}|') for l in out.splitlines() if l.startswith('param:')]
            with open(golden, encoding='utf-8') as fh:
                self.assertEqual(got, fh.read().splitlines(), golden)

    def values(self, *args, plugin=0):
        """name=value lines of a run of the mini host (a changed parameter shows its value after the actions)."""
        return dict(line.split('=', 1) for line in self.host('--plugin', str(plugin), '--no-reload', *args).splitlines()
                    if '=' in line)

    def test_a_grade_saved_by_2_4_0_still_loads(self):
        """Resolve restores a node by parameter name and, for a choice, by index: every name of the released panel
        must still exist with the same type, default and range, and its options may only grow at the end. Where
        a parameter sits in the panel is free to change."""
        for plugin, released_file in ((0, 'params_develop_2.4.0.txt'), (1, 'params_detail_2.4.0.txt')):
            released = {}
            with open(os.path.join(ROOT, 'tests', 'golden', 'released', released_file), encoding='utf-8') as fh:
                for line in fh.read().splitlines():
                    f = line.split('|')
                    released[f[0]] = f
            now = {}
            for line in self.host('--plugin', str(plugin), '--dump-params', '--no-reload').splitlines():
                if line.startswith('param:'):
                    f = line.split('|')
                    now[f[0]] = f
            gone = [n for n in set(released) - set(now) if released[n][1] != 'OfxParamTypeGroup']   # no value
            self.assertEqual(sorted(gone), [], 'parametri della 2.4.0 spariti: %s' % released_file)
            TYPE, DEFAULT, MIN, MAX, OPTIONS = 1, 3, 7, 8, 15
            for name, old in released.items():
                if name not in now:
                    continue
                new_ = now[name]
                self.assertEqual(new_[TYPE], old[TYPE], name)
                self.assertEqual(new_[DEFAULT], old[DEFAULT], name)
                self.assertEqual((new_[MIN], new_[MAX]), (old[MIN], old[MAX]), name)
                self.assertTrue(new_[OPTIONS] == old[OPTIONS] or new_[OPTIONS].startswith(old[OPTIONS] + ','),
                                '%s: le opzioni non possono cambiare ordine' % name)

    def test_a_new_detail_starts_zonale_at_development_and_a_saved_one_keeps_one_node(self):
        """Zonale in phases: a new node is guided from 1 · Sviluppo; a node saved before the phases (version 1)
        keeps every phase in one node, as it was."""
        new = self.values(plugin=1)
        self.assertEqual((new.get('zonalPhase'), new.get('settingsVersion')), ('1', '2'))
        saved = self.values('--set', 'settingsVersion=1', plugin=1)
        self.assertEqual((saved.get('zonalPhase', '0'), saved.get('settingsVersion')), ('0', '2'))

    def test_the_old_input_list_fills_the_two_choices(self):
        """Before 2.5 "Ingresso nodo" held space and gamma together: S-Gamut3/S-Log3 becomes both choices."""
        values = self.values('--set', 'nodeInput=3', '--set', 'settingsVersion=5')
        self.assertEqual((values['inputSpace'], values['inputGamma']), ('8', '10'))   # S-Gamut3, SLog3, after Automatico
        self.assertEqual(values['settingsVersion'], '6')
        again = self.values('--set', 'nodeInput=3', '--set', 'settingsVersion=5', '--set', 'inputSpace=2')
        self.assertEqual(again['inputSpace'], '2', 'a choice already made is not overwritten')

    def test_the_eyedropper_sets_temperature_and_tint(self):
        """A warm-looking neutral lowers the Kelvin, a cool one raises it, and the balance becomes Custom."""
        declared = ['--set', 'inputSpace=2', '--set', 'inputGamma=2']   # Rec.709, linear
        warm = self.values(*declared, '--change', 'whitePick=0.6,0.5,0.4')
        cool = self.values(*declared, '--change', 'whitePick=0.4,0.5,0.62')
        self.assertLess(float(warm['colorTemp']), 5600)
        self.assertGreater(float(cool['colorTemp']), 5600)
        self.assertEqual(warm['whiteBalance'], '7')
        grey = self.values(*declared, '--change', 'whitePick=0.18,0.18,0.18')
        self.assertAlmostEqual(float(grey['colorTemp']), 5600, delta=50)   # already neutral: the as-shot values
        self.assertAlmostEqual(float(grey['tint']), 0, delta=1)

    def test_only_what_resolve_lacks_is_shown_until_the_node_uses_the_rest(self):
        def hidden(values_args):
            out = self.host('--plugin', '0', '--no-reload', *values_args)
            return {l.split(':', 1)[1].split('=')[0] for l in out.splitlines() if l.startswith('secret:') and l.endswith('=1')}
        plain = hidden([])
        self.assertTrue({'toneContrast', 'toneShadows', 'toneWhites', 'toneBlacks', 'toneVibrance', 'zonesGroup',
                         'zoneLightExp', 'softClipColor', 'fcZones'} <= plain, plain)
        self.assertFalse({'toneHighlights', 'toneSaturation', 'softClipLevel', 'softClip', 'tonesMore', 'toneReset'} & plain)
        used = hidden(['--set', 'toneContrast=20', '--set', 'zoneLightExp=1'])
        self.assertFalse({'toneContrast', 'zonesGroup', 'zoneLightExp'} & used, 'a saved grade keeps its controls')
        self.assertIn('toneShadows', used)
        self.assertEqual({n for n in hidden(['--set', 'tonesMore=1']) if n.startswith(('tone', 'zone'))}, set())

    def test_a_relinked_clip_keeps_its_grade(self):
        """Same file name and size in another folder (relink, another computer): the sliders stay where the colourist
        left them. A different clip takes the camera values again."""
        with tempfile.TemporaryDirectory() as directory:
            home = Path(directory) / 'user'
            cache = home / SUPPORT_REL / 'cache'
            cache.mkdir(parents=True)
            moved = Path(directory) / 'nuova cartella' / 'C0001.MP4'
            moved.parent.mkdir()
            moved.write_bytes(b'0123456789')
            value = 0xcbf29ce484222325
            import unicodedata
            for byte in unicodedata.normalize('NFC', os.path.realpath(moved)).encode('utf-8'):
                value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
            (cache / ('%016x.json' % value)).write_text(json.dumps({
                'version': 6, 'supported': 1, 'camera_name': 'Relink camera', 'shot_temp': 4300, 'shot_tint': 0,
                'shot_ei': 1250}), encoding='utf-8')
            env = dict(os.environ, HOME=str(home), SLOGMETARAW_TEST_NO_RESOLVE='1', SLOGMETARAW_NO_UPDATE_CHECK='1')

            def run(identity):
                out = subprocess.run([self.exe, BINARY, str(moved), '--plugin', '0', '--no-reload',
                                      '--set', 'refSource=1', '--set', 'colorTemp=3333',
                                      '--set', 'boundPath=/vecchio/percorso/C0001.MP4', '--set', 'boundId=' + identity],
                                     capture_output=True, text=True, env=env, timeout=20).stdout
                return dict(line.split('=', 1) for line in out.splitlines() if '=' in line)

            same = run('C0001.MP4|10')
            self.assertEqual(float(same['colorTemp']), 3333)
            self.assertEqual(same['boundPath'], str(moved), 'the new place is remembered')
            other = run('C0002.MP4|10')
            self.assertEqual(float(other['colorTemp']), 4300)

            def pasted(*settings):
                """A node copied from a clip shot at EI 800 (+1 stop: Exposure 1600) onto the clip of the record
                (shot at EI 1250, 4300 K)."""
                out = subprocess.run([self.exe, BINARY, str(moved), '--plugin', '0', '--no-reload',
                                      '--set', 'refSource=1', '--set', 'shotEI=800', '--set', 'exposure=1600',
                                      '--set', 'boundPath=/altra/clip/C0009.MP4', '--set', 'boundId=C0009.MP4|99'] +
                                     list(settings), capture_output=True, text=True, env=env, timeout=20).stdout
                return dict(line.split('=', 1) for line in out.splitlines() if '=' in line)

            as_shot = pasted('--set', 'colorTemp=3333')
            self.assertAlmostEqual(float(as_shot['exposure']), 2500, delta=1, msg='+1 stop above the new camera EI')
            self.assertEqual(float(as_shot['colorTemp']), 4300, 'As shot follows the new camera')
            custom = pasted('--set', 'whiteBalance=7', '--set', 'colorTemp=3333', '--set', 'tint=12')
            self.assertEqual((float(custom['colorTemp']), float(custom['tint'])), (3333, 12), 'a Custom balance travels')
            self.assertAlmostEqual(float(custom['exposure']), 2500, delta=1)

    def test_the_lut_safe_node_comes_first_and_the_spatial_one_second(self):
        """Resolve identifies a plugin by bundle index too, and only a pointwise node may be baked into
        a LUT: a spatial one declaring otherwise would corrupt Generate LUT without a warning."""
        first = self.host('--plugin', '0', '--no-reload')
        second = self.host('--plugin', '1', '--no-reload')
        self.assertIn('plugins=2', first)
        self.assertIn('plugin: com.slogmetaraw.SLogMetaRaw v', first)
        self.assertIn('noSpatialAwareness=true', first)
        self.assertIn('plugin: com.slogmetaraw.SLogMetaRawDetail v1.0', second)
        self.assertIn('noSpatialAwareness=false', second)
        self.assertIn('identity=1', second)

    def test_instance_without_a_clip_stays_neutral(self):
        """No source path (a compound clip, for instance): the node must still build."""
        values = self.run_host()
        self.assertEqual(values['metaValid'], '0')
        self.assertIn('percorso', values['status'])

    def test_data_level_starts_on_automatico_and_corrects_nothing(self):
        """With no metadata the data level stage has nothing to go on, and a node that
        corrects a clip it knows nothing about would be worse than one that does not."""
        values = self.run_host()
        self.assertEqual(values['dataLevel'], '0')        # Automatico
        self.assertEqual(values['levelRequired'], '-1')   # nothing read yet
        self.assertEqual(values['levelHost'], '-1')       # Resolve not asked yet
        self.assertIn('non nota', values['levelInfo'])

    def test_instance_sets_itself_to_the_clip(self):
        clip = os.path.join(ROOT, 'samples', 'Sony FX30', 'F002C005_260717TL.MP4')
        if not os.path.exists(clip):
            raise unittest.SkipTest('clip di esempio non presente')
        subprocess.run([sys.executable, '-m', 'slogmetaraw', '--cache', clip],
                       cwd=ROOT, check=True, capture_output=True)
        values = self.run_host(clip)
        self.assertEqual(values['camera'], 'Sony ILME-FX30 (FX30)')
        self.assertEqual(values['metaValid'], '1')
        self.assertEqual(float(values['colorTemp']), 4000)
        self.assertEqual(float(values['exposure']), 4000)

    def test_missing_cache_launches_isolated_resolve_python(self):
        """The OFX fallback must import its library even when PYTHONPATH is ignored."""
        bundled = '/Applications/DaVinci Resolve/DaVinci Resolve.app/Contents/'
        if not any(os.access(bundled + suffix, os.X_OK) for suffix in (
                'Applications/ResolvePython', 'Resources/ResolvePython/ResolvePython')):
            self.skipTest('ResolvePython bundled non disponibile')
        with tempfile.TemporaryDirectory() as directory:
            user_directory = Path(directory) / 'user'
            support = user_directory / SUPPORT_REL
            support.mkdir(parents=True)
            library = Path(directory) / "libreria d'Ivan & è"
            package = library / 'slogmetaraw'
            package.mkdir(parents=True)
            (package / '__init__.py').write_text('', encoding='utf-8')
            clip = Path(directory) / "clip d'Ivan è.MP4"
            clip.write_bytes(b'fixture: only the subprocess bootstrap is under test')
            # The fake reader isolates process launching from Sony parsing. It must
            # receive the exact clip path and create the record the real plugin reads;
            # in --to-resolve mode (the reload button) it also prints the JSON result.
            record = {'version': 6, 'supported': 1, 'camera_name': 'Bootstrap test camera',
                      'shot_temp': 4300, 'shot_tint': 0, 'shot_ei': 1250}
            reader = (
                'import json, os, sys, unicodedata\n'
                'from pathlib import Path\n'
                'assert sys.argv[1] in ("--cache", "--to-resolve"), sys.argv\n'
                'assert sys.argv[2] == ' + repr(str(clip)) + '\n'
                'value = 0xcbf29ce484222325\n'
                'for byte in unicodedata.normalize("NFC", os.path.realpath(sys.argv[2])).encode("utf-8"):\n'
                '    value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff\n'
                'cache = Path(os.environ["HOME"]) / %r / "cache"\n' % SUPPORT_REL +
                'cache.mkdir(parents=True, exist_ok=True)\n'
                '(cache / ("%016x.json" % value)).write_text(' + repr(json.dumps(record)) + ', encoding="utf-8")\n'
                'if sys.argv[1] == "--to-resolve":\n'
                '    print(json.dumps({"ok": 1, "written": 34, "failed": 0, "clips": 1}))\n'
            )
            (package / '__main__.py').write_text(reader, encoding='utf-8')
            (support / 'lib_path').write_text(str(library) + '\n', encoding='utf-8')
            env = dict(os.environ, HOME=str(user_directory),
                       PYTHONPATH='/nonexistent/foreign-library', PYTHONHOME='/nonexistent/foreign-python')
            self.assertFalse((support / 'cache').exists())
            values = self.run_host(str(clip), env=env)
            self.assertEqual(values['metaValid'], '1', values.get('status'))
            self.assertEqual(values['camera'], 'Bootstrap test camera')
            self.assertEqual(float(values['colorTemp']), 4300)
            self.assertIn('scritti', values.get('status', ''))
            self.assertEqual(len(list((support / 'cache').glob('*.json'))), 1)


if __name__ == '__main__':
    unittest.main()

# SPDX-License-Identifier: GPL-3.0-or-later
"""Atlante della logica (tools/atlas): the behaviour model must keep pointing at real code.

A renamed or removed function, UI binding or test cited by tools/atlas/model.json fails here, in the same change
that broke the reference. Code changed after the model was last reviewed is reported, not failed: the page marks it
'modificato' until `python3 tools/atlas/atlas.py --accept <behaviour>` records the review."""
import copy
import json
import os
import re
import sys
import tempfile
import unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'atlas'))

import atlas  # noqa: E402

KINDS = {'trigger', 'call', 'request', 'response', 'data', 'async', 'error', 'persistence', 'decision', 'control'}


class AtlasModel(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(atlas.MODEL, encoding='utf-8') as fh:
            cls.model = json.load(fh)
        cls.facts = atlas.extract()

    def test_every_reference_resolves(self):
        problems, stale, _, _ = atlas.resolve(copy.deepcopy(self.model), self.facts)
        self.assertEqual(problems, [])
        for line in stale:   # visible in the test log, explicit on the page
            print('atlas: da rivedere', line, file=sys.stderr)

    def test_ids_are_unique_and_edges_have_direction_and_meaning(self):
        seen = set()
        for b in self.model['behaviours']:
            self.assertNotIn(b['id'], seen)
            seen.add(b['id'])
            local = [n['id'] for n in b['nodes']] + [e['id'] for e in b['edges']] + [p['id'] for p in b['proposals']]
            self.assertEqual(len(local), len(set(local)), b['id'])
            nodes = {n['id'] for n in b['nodes']}
            self.assertEqual(set(b['view']['pos']), nodes, 'every block needs a place in the view')
            for e in b['edges']:
                self.assertIn(e['kind'], KINDS, e['id'])
                self.assertTrue(e['label'].strip(), e['id'])
                self.assertIn(e['from'], nodes)
                self.assertIn(e['to'], nodes)

    def test_changed_code_is_marked_for_review(self):
        model = copy.deepcopy(self.model)
        beh = model['behaviours'][0]['id']
        code = next(iter(model['fingerprints'][beh]))
        model['fingerprints'][beh][code] = '000000000000'
        _, stale, _, _ = atlas.resolve(model, self.facts)
        self.assertTrue(any(code in line for line in stale))

    def test_extraction_finds_ui_bindings_and_nested_calls(self):
        b = self.facts['bindings']['binding:Clips.ItemClicked']
        self.assertEqual(b['handler'], 'slogmetaraw/ui.py::main.on_select')
        show = self.facts['functions']['slogmetaraw/ui.py::main.show_details']
        self.assertIn('slogmetaraw/ui.py::main._section_open', show['calls'])
        helper = self.facts['functions']['slogmetaraw/__main__.py::_helper']
        self.assertIn('slogmetaraw/extract.py::read_clip', helper['calls'])

    def test_page_carries_no_local_paths(self):
        # the repository is public: the generated page must hold repository-relative paths only
        with tempfile.TemporaryDirectory() as out:
            with mock.patch.object(atlas, 'OUT_DIR', out):
                page_path = atlas.write(atlas.build()[0])
            with open(page_path, encoding='utf-8') as fh:
                page = fh.read()
        self.assertNotIn('/*__ATLAS_DATA__*/null', page)
        self.assertNotIn(ROOT, page)
        self.assertIsNone(re.search(r'/(home|Users)/[^/\s"]+/', page))
        self.assertIsNone(re.search(r'[\w.+-]+@[\w-]+\.[\w.]+', page.replace('noreply@', '')))


if __name__ == '__main__':
    unittest.main()

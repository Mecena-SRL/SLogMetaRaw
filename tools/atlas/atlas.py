#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Atlante della logica: development tool, not part of the program.

Extracts what the code says (functions, line ranges, calls, imports, UI event bindings, tests) with the Python
AST, joins it with the hand-written behaviour model (tools/atlas/model.json) and writes a self-contained page.

    python3 tools/atlas/atlas.py              # build/atlas/index.html + build/atlas/atlas.json
    python3 tools/atlas/atlas.py --run-tests  # also runs the tests the model cites and records the outcome
    python3 tools/atlas/atlas.py --check      # exit 1 on broken references (used by tests/test_atlas.py)
    python3 tools/atlas/atlas.py --accept ID  # after review: store the current code fingerprints for ID

Nothing here reads outside the repository and nothing absolute or personal is written into the output.
"""
import argparse
import ast
import hashlib
import json
import os
import re
import subprocess
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.join(ROOT, 'tools', 'atlas')
MODEL = os.path.join(HERE, 'model.json')
TEMPLATE = os.path.join(HERE, 'template.html')
OUT_DIR = os.path.join(ROOT, 'build', 'atlas')
PY_DIRS = ('slogmetaraw', 'resolve_script')
REPO_URL = 'https://github.com/Mecena-SRL/SLogMetaRaw'
SCHEMA = 'slogmetaraw-atlas/1'


# --- extraction (facts read from the code) ---------------------------------------------------------------------

def _fingerprint(text):
    norm = '\n'.join(line.rstrip() for line in text.strip().splitlines())
    return hashlib.sha1(norm.encode('utf-8')).hexdigest()[:12]


def _dotted(node):
    parts = []
    while isinstance(node, ast.Attribute):
        parts.append(node.attr)
        node = node.value
    if isinstance(node, ast.Name):
        parts.append(node.id)
        return '.'.join(reversed(parts))
    return None


class _ModuleVisitor(ast.NodeVisitor):
    def __init__(self, rel, source):
        self.rel, self.source = rel, source
        self.functions, self.bindings, self.imports = {}, [], {}
        self.names = {}      # from .x import y -> {'y': 'x.py'}
        self.stack = []      # qualified-name parts of the enclosing functions/classes
        self.scopes = [{}]   # name -> qualified name of the functions visible in each scope

    def visit_ImportFrom(self, node):
        if node.level == 1:      # from . import x / from .x import y
            for alias in node.names:
                if node.module:
                    self.imports[alias.asname or alias.name] = '%s.py' % node.module
                    self.names[alias.asname or alias.name] = ('%s.py' % node.module, alias.name)
                else:
                    self.imports[alias.asname or alias.name] = '%s.py' % alias.name

    def _function(self, node):
        qual = '.'.join(self.stack + [node.name])
        self.scopes[-1][node.name] = qual
        segment = ast.get_source_segment(self.source, node) or ''
        doc = ast.get_docstring(node) or ''
        self.functions[qual] = {'id': '%s::%s' % (self.rel, qual), 'file': self.rel, 'name': qual,
                                'line': node.lineno, 'end': node.end_lineno, 'fingerprint': _fingerprint(segment),
                                'doc': doc.split('\n\n')[0].strip(), 'calls_raw': []}
        # nested functions must be visible before the body runs (closures call later siblings)
        inner = {}
        for child in ast.walk(node):
            if child is not node and isinstance(child, (ast.FunctionDef, ast.AsyncFunctionDef)):
                inner.setdefault(child.name, qual + '.' + child.name)
        self.stack.append(node.name)
        self.scopes.append(inner)
        for child in ast.walk(node):
            if isinstance(child, ast.Call):
                name = _dotted(child.func)
                if name:
                    self.functions[qual]['calls_raw'].append((name, [dict(s) for s in self.scopes]))
        self.generic_visit(node)
        self.scopes.pop()
        self.stack.pop()

    visit_FunctionDef = visit_AsyncFunctionDef = _function

    def visit_ClassDef(self, node):
        self.stack.append(node.name)
        self.generic_visit(node)
        self.stack.pop()

    def visit_Assign(self, node):
        # win.On.<Widget>.<Event> = guard(handler) / disp.On.<Event> = handler
        for target in node.targets:
            name = _dotted(target)
            if not name or '.On.' not in name:
                continue
            value = node.value
            if isinstance(value, ast.Call) and value.args:
                value = value.args[0]
            handler = _dotted(value)
            parts = name.split('.On.', 1)[1].split('.')
            widget, event = (parts[0], parts[1]) if len(parts) == 2 else ('', parts[0])
            self.bindings.append({'id': 'binding:%s.%s' % (widget, event) if widget else 'binding:%s' % event,
                                  'file': self.rel, 'line': node.lineno, 'widget': widget, 'event': event,
                                  'handler_name': handler, 'scope': '.'.join(self.stack)})
        self.generic_visit(node)


def extract():
    modules, functions, bindings = {}, {}, []
    for d in PY_DIRS:
        for name in sorted(os.listdir(os.path.join(ROOT, d))):
            if not name.endswith('.py'):
                continue
            rel = '%s/%s' % (d, name)
            with open(os.path.join(ROOT, rel), encoding='utf-8') as fh:
                source = fh.read()
            v = _ModuleVisitor(rel, source)
            v.visit(ast.parse(source, rel))
            modules[rel] = {'id': rel, 'imports': sorted({'slogmetaraw/' + m for m in v.imports.values()
                                                          if os.path.exists(os.path.join(ROOT, 'slogmetaraw', m))}),
                            'lines': source.count('\n') + 1, 'aliases': v.imports, 'names': v.names}
            for f in v.functions.values():
                functions[f['id']] = f
            bindings.extend(v.bindings)
    # resolve calls: nested/sibling names first, then module-level names, then imported module attributes
    for f in functions.values():
        rel = f['file']
        aliases, names = modules[rel]['aliases'], modules[rel]['names']
        calls = set()
        for name, scopes in f.pop('calls_raw'):
            target = None
            if '.' not in name:
                for scope in reversed(scopes):
                    if name in scope:
                        target = '%s::%s' % (rel, scope[name])
                        break
                if target is None and '%s::%s' % (rel, name) in functions:
                    target = '%s::%s' % (rel, name)
                if target is None and name in names:
                    target = 'slogmetaraw/%s::%s' % names[name]
            else:
                head, attr = name.split('.', 1)
                if head in aliases and '.' not in attr:
                    target = 'slogmetaraw/%s::%s' % (aliases[head], attr)
            if target and target in functions and target != f['id']:
                calls.add(target)
        f['calls'] = sorted(calls)
    for m in modules.values():
        m.pop('aliases')
        m.pop('names')
    for b in bindings:
        scope = b.pop('scope')
        cand = '%s::%s' % (b['file'], '.'.join(filter(None, [scope, b['handler_name'] or ''])))
        b['handler'] = cand if cand in functions else None
    tests = {}
    for name in sorted(os.listdir(os.path.join(ROOT, 'tests'))):
        if not (name.startswith('test_') and name.endswith('.py')):
            continue
        rel = 'tests/' + name
        with open(os.path.join(ROOT, rel), encoding='utf-8') as fh:
            tree = ast.parse(fh.read(), rel)
        for cls in tree.body:
            if isinstance(cls, ast.ClassDef):
                for fn in cls.body:
                    if isinstance(fn, ast.FunctionDef) and fn.name.startswith('test'):
                        tid = '%s::%s.%s' % (rel, cls.name, fn.name)
                        tests[tid] = {'id': tid, 'file': rel, 'line': fn.lineno,
                                      'doc': (ast.get_docstring(fn) or '').split('\n\n')[0].strip()}
    return {'modules': modules, 'functions': functions, 'bindings': {b['id']: b for b in bindings}, 'tests': tests}


# --- revision -------------------------------------------------------------------------------------------------

def _git(*args):
    try:
        return subprocess.run(['git', '-C', ROOT] + list(args), capture_output=True, text=True,
                              timeout=10).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ''


def revision():
    commit = _git('rev-parse', 'HEAD')
    dirty = [line[3:] for line in _git('status', '--porcelain').splitlines() if line[3:]]
    return {'commit': commit or None, 'short': commit[:9] if commit else None,
            'branch': _git('rev-parse', '--abbrev-ref', 'HEAD') or None,
            'dirty': sorted(dirty), 'version': _version()}


def _version():
    with open(os.path.join(ROOT, 'slogmetaraw', '__init__.py'), encoding='utf-8') as fh:
        m = re.search(r"__version__ = '([^']+)'", fh.read())
    return m.group(1) if m else None


# --- join with the model --------------------------------------------------------------------------------------

def _refs(element):
    return element.get('refs', [])


def resolve(model, facts, test_results=None):
    """Annotates every model element with the status of its references. Returns (atlas, problems, stale)."""
    problems, stale, unreviewed = [], [], set()
    fps = model.get('fingerprints', {})
    calls = {(f['id'], c) for f in facts['functions'].values() for c in f['calls']}
    owner_of = {}

    def check(element, owner):
        out = []
        for ref in _refs(element):
            r = dict(ref)
            if 'code' in ref:
                f = facts['functions'].get(ref['code'])
                if f:
                    r.update(status='estratto', file=f['file'], line=f['line'], end=f['end'],
                             fingerprint=f['fingerprint'])
                    known = fps.get(owner, {}).get(ref['code'])
                    if known and known != f['fingerprint']:
                        r['status'] = 'modificato'
                        stale.append('%s: %s cambiato dopo la revisione del modello' % (owner, ref['code']))
                    elif not known:
                        r['status'] = 'non revisionato'
                        unreviewed.add('%s: %s' % (owner, ref['code']))
                    owner_of.setdefault(ref['code'], set()).add(owner)
                else:
                    r['status'] = 'mancante'
                    problems.append('%s: funzione %s non trovata' % (element['id'], ref['code']))
            elif 'binding' in ref:
                b = facts['bindings'].get(ref['binding'])
                if b:
                    r.update(status='estratto', file=b['file'], line=b['line'], handler=b['handler'])
                else:
                    r['status'] = 'mancante'
                    problems.append('%s: binding %s non trovato' % (element['id'], ref['binding']))
            elif 'test' in ref:
                tst = facts['tests'].get(ref['test'])
                if tst:
                    r.update(status='presente', file=tst['file'], line=tst['line'])
                    if test_results and ref['test'] in test_results:
                        r['status'] = test_results[ref['test']]
                else:
                    r['status'] = 'mancante'
                    problems.append('%s: test %s non trovato' % (element['id'], ref['test']))
            elif 'file' in ref:
                r['status'] = 'presente' if os.path.exists(os.path.join(ROOT, ref['file'])) else 'mancante'
                if r['status'] == 'mancante':
                    problems.append('%s: file %s non trovato' % (element['id'], ref['file']))
            out.append(r)
        element['refs'] = out

    for sub in model['subsystems']:
        check(sub, sub['id'])
        for mod in sub.get('modules', []):
            if mod not in facts['modules'] and not os.path.exists(os.path.join(ROOT, mod)):
                problems.append('%s: modulo %s non trovato' % (sub['id'], mod))
    for beh in model['behaviours']:
        check(beh, beh['id'])
        ids = {n['id'] for n in beh['nodes']}
        for n in beh['nodes']:
            check(n, beh['id'])
        for e in beh['edges']:
            if e['from'] not in ids or e['to'] not in ids:
                problems.append('%s: collegamento %s con estremi sconosciuti' % (beh['id'], e['id']))
            check(e, beh['id'])
            # an edge that claims a call is confirmed only when the AST has that call
            if e.get('kind') == 'call':
                src = next((r['code'] for n in beh['nodes'] if n['id'] == e['from'] for r in n['refs']
                            if 'code' in r), None)
                dst = next((r['code'] for n in beh['nodes'] if n['id'] == e['to'] for r in n['refs']
                            if 'code' in r), None)
                e['evidence'] = 'estratto' if (src, dst) in calls else 'dedotto'
        for p in beh.get('proposals', []):
            check(p, beh['id'])
    assigned = {m for s in model['subsystems'] for m in s.get('modules', [])}
    uncovered = sorted(m for m in facts['modules'] if m not in assigned)
    return problems, stale, uncovered, sorted(unreviewed)


def graphify_freshness(facts):
    path = os.path.join(ROOT, 'docs', 'GRAPHIFY.md')
    if not os.path.exists(path):
        return {'status': 'mancante'}
    with open(path, encoding='utf-8') as fh:
        text = fh.read()
    m = re.search(r'alla (\d+\.\d+\.\d+)', text)
    missing = sorted(rel for rel in facts['modules']
                     if rel.startswith('slogmetaraw/') and not rel.endswith('__init__.py')
                     and os.path.basename(rel) not in text)
    version = _version()
    return {'documented_version': m.group(1) if m else None, 'current_version': version,
            'modules_missing': missing,
            'status': 'aggiornato' if (not missing and m and m.group(1) == version) else 'da verificare'}


def run_tests(test_ids):
    results = {}
    for tid in sorted(test_ids):
        rel, name = tid.split('::')
        dotted = rel[:-3].replace('/', '.') + '.' + name
        sys.path.insert(0, ROOT)
        try:
            suite = unittest.defaultTestLoader.loadTestsFromName(dotted)
            outcome = unittest.TextTestRunner(stream=open(os.devnull, 'w'), verbosity=0).run(suite)
            results[tid] = 'superato' if outcome.wasSuccessful() and not outcome.skipped else (
                'saltato' if outcome.wasSuccessful() else 'fallito')
        except Exception:
            results[tid] = 'errore'
        finally:
            sys.path.remove(ROOT)
    return results


def build(run=False):
    with open(MODEL, encoding='utf-8') as fh:
        model = json.load(fh)
    facts = extract()
    test_ids = {r['test'] for b in model['behaviours'] for el in [b] + b['nodes'] + b['edges'] +
                b.get('proposals', []) for r in el.get('refs', []) if 'test' in r}
    results = run_tests(test_ids) if run else None
    problems, stale, uncovered, unreviewed = resolve(model, facts, results)
    rev = revision()
    atlas = {'schema': SCHEMA, 'revision': rev, 'repo_url': REPO_URL, 'model': model,
             'facts': {'functions': facts['functions'], 'bindings': facts['bindings'],
                       'modules': facts['modules'], 'tests': {k: v for k, v in facts['tests'].items()
                                                              if k in test_ids}},
             'coverage': {'problems': problems, 'stale': stale, 'unreviewed': unreviewed, 'modules_unassigned': uncovered,
                          'tests_run': bool(run), 'graphify': graphify_freshness(facts),
                          'extracted': ['funzioni Python (nome, righe, impronta)', 'chiamate fra funzioni Python',
                                        'import fra moduli slogmetaraw', 'binding eventi UI (win.On.X.Y)',
                                        'elenco test unittest'],
                          'not_extracted': ['codice C++ del plugin OFX (solo riferimenti a file, controllati)',
                                            'testi, eventi e decisioni dei comportamenti (modello manuale)',
                                            'disposizione grafica (vista, manuale)']}}
    return atlas, problems, stale


def write(atlas):
    os.makedirs(OUT_DIR, exist_ok=True)
    data = json.dumps(atlas, ensure_ascii=False, indent=1, sort_keys=True)
    with open(os.path.join(OUT_DIR, 'atlas.json'), 'w', encoding='utf-8') as fh:
        fh.write(data)
    with open(TEMPLATE, encoding='utf-8') as fh:
        page = fh.read()
    page = page.replace('/*__ATLAS_DATA__*/null', data.replace('</', '<\\/'))
    out = os.path.join(OUT_DIR, 'index.html')
    with open(out, 'w', encoding='utf-8') as fh:
        fh.write(page)
    return out


def accept(owner):
    """Deterministic update of the model: store the current fingerprints of the code an element cites."""
    with open(MODEL, encoding='utf-8') as fh:
        model = json.load(fh)
    facts = extract()
    beh = next((b for b in model['behaviours'] + model['subsystems'] if b['id'] == owner), None)
    if beh is None:
        raise SystemExit('elemento %s non trovato nel modello' % owner)
    codes = {r['code'] for el in [beh] + beh.get('nodes', []) + beh.get('edges', []) + beh.get('proposals', [])
             for r in el.get('refs', []) if 'code' in r and r['code'] in facts['functions']}
    model.setdefault('fingerprints', {})[owner] = {c: facts['functions'][c]['fingerprint'] for c in sorted(codes)}
    with open(MODEL, 'w', encoding='utf-8') as fh:
        json.dump(model, fh, ensure_ascii=False, indent=1)
        fh.write('\n')
    return len(codes)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--check', action='store_true', help='solo controllo dei riferimenti, nessun file scritto')
    ap.add_argument('--run-tests', action='store_true', help='esegue i test citati dal modello')
    ap.add_argument('--accept', metavar='ID', help='registra le impronte correnti per un comportamento')
    a = ap.parse_args(argv)
    if a.accept:
        print('%d impronte registrate per %s' % (accept(a.accept), a.accept))
        return 0
    atlas, problems, stale = build(a.run_tests)
    for p in problems:
        print('RIFERIMENTO ROTTO', p, file=sys.stderr)
    for s in stale:
        print('DA RIVEDERE', s, file=sys.stderr)
    if a.check:
        return 1 if problems else 0
    out = write(atlas)
    print(os.path.relpath(out, ROOT))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())

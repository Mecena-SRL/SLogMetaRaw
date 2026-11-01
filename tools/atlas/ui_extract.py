# SPDX-License-Identifier: GPL-3.0-or-later
"""Interfaccia: what the three panels contain, read from their sources (development tool).

- Script window: the widget tree passed to disp.AddWindow in slogmetaraw/ui.py, read with the AST.
- Develop and Detail panels: tests/golden/params_*.txt, the dump of the parameters as a miniature OpenFX host sees
  them when it loads the built plugin (tests/test_ofx_host.py keeps them equal to the plugin). Each parameter is
  located in the C++ sources by a text search for its name.
- Components: the UIManager widget types of the script, the define* helpers of src/common/ParamDefs.h.
"""
import ast
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OFX_SRC = os.path.join('ofx', 'SLogMetaRaw', 'src')
GOLDEN_FIELDS = ('label', 'default', 'parent', 'secret', 'enabled', 'min', 'max', 'displayMin', 'displayMax',
                 'increment', 'digits', 'animates', 'hint', 'options', 'icon', 'groupOpen')
PANELS = (('develop', 'S-Log MetaRaw (Develop)', 'tests/golden/params_develop.txt', 'develop'),
          ('detail', 'S-Log MetaRaw Detail', 'tests/golden/params_detail.txt', 'detail'))


def _read(rel):
    with open(os.path.join(ROOT, rel), encoding='utf-8') as fh:
        return fh.read()


# --- script window ------------------------------------------------------------------------------------------

def _value(node):
    """A widget property as data: literals, t('...') texts (marked for translation), names for the rest."""
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.Call) and getattr(node.func, 'id', None) == 't' and node.args:
        v = _value(node.args[0])
        return {'t': v} if isinstance(v, str) else v
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Mod):
        left = _value(node.left)
        text = left['t'] if isinstance(left, dict) else left
        return {'t': text, 'format': True} if isinstance(left, dict) else text
    if isinstance(node, (ast.List, ast.Tuple)):
        return [_value(e) for e in node.elts]
    if isinstance(node, ast.Dict):
        return {k.value: _value(v) for k, v in zip(node.keys, node.values) if isinstance(k, ast.Constant)}
    if isinstance(node, ast.Name):
        return {'name': node.id}
    return {'expr': ast.unparse(node)}


def _widget(node, path, counter):
    if not (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
            and getattr(node.func.value, 'id', None) == 'ui'):
        return None
    kind = node.func.attr
    props = _value(node.args[0]) if node.args and isinstance(node.args[0], ast.Dict) else {}
    counter[kind] = counter.get(kind, 0) + 1
    wid = props.get('ID') or '%s%d' % (kind, counter[kind])
    out = {'id': 'ui:script.%s' % wid, 'type': kind, 'widget_id': props.get('ID'), 'props': props,
           'line': node.lineno, 'children': []}
    kids = node.args[1] if len(node.args) > 1 else None
    if isinstance(kids, (ast.List, ast.Tuple)):
        for k in kids.elts:
            w = _widget(k, path + [wid], counter)
            if w:
                out['children'].append(w)
    return out


def script_window(rel='slogmetaraw/ui.py'):
    tree = ast.parse(_read(rel), rel)
    for node in ast.walk(tree):
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr == 'AddWindow'
                and len(node.args) == 2):
            props = _value(node.args[0])
            root = _widget(node.args[1], [], {})
            return {'id': 'ui:script', 'label': 'Finestra dello script', 'file': rel, 'line': node.lineno,
                    'window': props, 'root': root, 'source': 'struttura estratta (AST di %s)' % rel}
    return None


# --- OFX panels ---------------------------------------------------------------------------------------------

def _ofx_files():
    out = []
    for base, _, names in os.walk(os.path.join(ROOT, OFX_SRC)):
        for n in sorted(names):
            if n.endswith(('.cpp', '.h')):
                out.append(os.path.relpath(os.path.join(base, n), ROOT).replace(os.sep, '/'))
    return sorted(out)


def _locate(names, folder):
    """name -> {'define': where the literal name is defined, 'handlers': [file:line, ...]}.

    Text search in this panel's folder and common/ (the two panels share several names), ranked: a define*/trim
    call or table entry first, factories and *Params files first, then any other mention."""
    texts = {rel: _read(rel).splitlines() for rel in _ofx_files()}
    own, common = '/%s/' % folder, '/common/'
    found = {}
    for name in names:
        lit = '"%s"' % name
        hits = [(rel, i + 1, line.strip()) for rel, lines in texts.items() for i, line in enumerate(lines) if lit in line]

        def rank(h):
            is_def = bool(re.search(r'(define\w*|trim)\(|\{\s*"' + re.escape(name), h[2]))
            place = 0 if own in h[0] else 1 if common in h[0] else 2
            return (0 if is_def else 1, place, 0 if 'Factory' in h[0] or 'Params' in h[0] else 1, h[0], h[1])
        hits.sort(key=rank)
        define = hits[0] if hits and rank(hits[0])[1] < 2 else None
        handlers = [h for h in hits if 'ParamName' in h[2] and own in h[0]]
        found[name] = {'define': {'file': define[0], 'line': define[1], 'how': 'testo'} if define else None,
                       'handlers': [{'file': h[0], 'line': h[1]} for h in sorted(handlers, key=lambda h: (h[0], h[1]))]}
    return found


def _component(p):
    t = p['type'].replace('OfxParamType', '')
    if p['secret'] == '1':
        return 'hidden'
    return {'PushButton': 'defineButton', 'Double': 'defineSlider', 'Choice': 'defineChoice',
            'Group': 'defineGroup', 'String': 'defineInfo', 'Page': 'page', 'Integer': 'defineHiddenInt'
            }.get(t, 'defineToggle' if t == 'Boolean' and p['icon'] else 'defineCheck' if t == 'Boolean' else t)


def ofx_panel(key, label, golden, folder):
    params = []
    for line in _read(golden).splitlines():
        if not line.startswith('param:'):
            continue
        cells = line[len('param:'):].split('|')
        p = {'name': cells[0], 'type': cells[1]}
        p.update(dict(zip(GOLDEN_FIELDS, cells[2:])))
        p['options'] = p['options'].split(',') if p.get('options') else []
        p['id'] = 'param:%s.%s' % (key, p['name'])
        p['component'] = 'comp:ofx.' + _component(p)
        params.append(p)
    where = _locate([p['name'] for p in params], folder)
    zone = re.compile(r'^(local)?[zZ]one')
    for p in params:
        loc = where[p['name']]
        p['define'] = loc['define']
        if p['define'] is None and zone.match(p['name']):
            p['define'] = {'file': OFX_SRC.replace(os.sep, '/') + '/common/ZoneParams.cpp', 'line': _line_of(
                OFX_SRC + '/common/ZoneParams.cpp', 'void defineZoneParams'), 'how': 'nome generato (dedotto)'}
        p['handlers'] = loc['handlers']
    return {'id': 'ui:%s' % key, 'label': label, 'golden': golden, 'params': params,
            'factory': '%s/%s/%sFactory.cpp' % (OFX_SRC.replace(os.sep, '/'), folder, folder.capitalize()),
            'source': 'struttura verificata (%s, confrontato col plugin compilato da tests/test_ofx_host.py)' % golden}


def _line_of(rel, needle):
    for i, line in enumerate(_read(rel).splitlines()):
        if needle in line:
            return i + 1
    return None


# --- components ---------------------------------------------------------------------------------------------

def components(script, panels):
    rel = OFX_SRC.replace(os.sep, '/') + '/common/ParamDefs.h'
    defs = {}
    for i, line in enumerate(_read(rel).splitlines()):
        m = re.search(r'\b(define\w+)\(', line)
        if m and m.group(1) not in defs and m.group(1) != 'defineClips':
            defs[m.group(1)] = {'id': 'comp:ofx.' + m.group(1), 'family': 'plugin OFX', 'name': m.group(1),
                                'file': rel, 'line': i + 1, 'uses': []}
    defs['hidden'] = {'id': 'comp:ofx.hidden', 'family': 'plugin OFX', 'name': 'parametro nascosto (hideParam)',
                      'file': rel, 'line': _line_of(rel, 'void hideParam'), 'uses': []}
    defs['page'] = {'id': 'comp:ofx.page', 'family': 'plugin OFX', 'name': 'pagina', 'file': rel, 'line': None,
                    'uses': []}
    for panel in panels:
        for p in panel['params']:
            key = p['component'].split('.', 1)[1]
            defs.setdefault(key, {'id': p['component'], 'family': 'plugin OFX', 'name': key, 'file': None,
                                  'line': None, 'uses': []})['uses'].append(p['id'])
    widgets = {}

    def visit(w):
        widgets.setdefault(w['type'], {'id': 'comp:script.' + w['type'], 'family': 'finestra script',
                                       'name': 'ui.' + w['type'] + ' (UIManager di Resolve)', 'file': None,
                                       'line': None, 'uses': []})['uses'].append(w['id'])
        for c in w['children']:
            visit(c)
    if script:
        visit(script['root'])
    return sorted(list(defs.values()) + list(widgets.values()), key=lambda c: (c['family'], c['id']))


def extract_ui():
    script = script_window()
    panels = [ofx_panel(*p) for p in PANELS]
    return {'script': script, 'panels': panels, 'components': components(script, panels)}

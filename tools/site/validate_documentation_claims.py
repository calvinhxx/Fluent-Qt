#!/usr/bin/env python3
"""Check callable references in website prose against their public API owners.

This catches unknown owners and known methods attributed to the wrong component.
Otherwise unknown callables on Qt-derived types require review and produce warnings.
It does not prove prose about runtime behavior, defaults, platforms or ownership;
those claims still require review against implementation and tests.
"""

import argparse
import ast
import json
import re
from functools import lru_cache
from pathlib import Path

from component_inventory import ROOT, catalog, config
from documentation_guides import guides
from public_api import inventory, resolve_type


CALL = re.compile(r'(?<![\w:])(?:(?P<owner>(?:[A-Za-z_]\w*::)*[A-Z]\w*)(?:::|\.)'
                  r'|(?P<python>fluentqt)\.)?(?P<name>[a-z]\w*)\(')

# Reviewed Qt declarations used by the prose, not an unrestricted Qt exemption.
# Check the corresponding Qt public header before extending a member list.
# https://doc.qt.io/qt-6/<lowercase class name>.html owns the upstream contract.
QT = {
    'QWidget': ((), {'setAccessibleName', 'setFont', 'setEnabled', 'isEnabled', 'setFocus',
                    'setGeometry', 'setToolTip', 'setMinimumSize', 'setFixedSize', 'setStyleSheet',
                    'resize', 'move', 'show', 'hide'}),
    'QFrame': (('QWidget',), set()),
    'QAbstractScrollArea': (('QFrame',), set()),
    'QAbstractItemView': (('QAbstractScrollArea',), set()),
    'QListView': (('QAbstractItemView',), set()),
    'QTreeView': (('QAbstractItemView',), {'header', 'setHeader', 'isHeaderHidden', 'setHeaderHidden', 'setColumnWidth'}),
    'QTableView': (('QAbstractItemView',), set()),
    'QScrollArea': (('QAbstractScrollArea',), set()),
    'QStackedWidget': (('QFrame',), set()),
    'QLabel': (('QFrame',), set()),
    'QDialog': (('QWidget',), set()),
    'QAbstractButton': (('QWidget',), {'setAutoExclusive', 'setText', 'text', 'setShortcut', 'clicked', 'toggled'}),
    'QPushButton': (('QAbstractButton',), set()),
    'QRadioButton': (('QAbstractButton',), set()),
    'QCheckBox': (('QAbstractButton',), {'setTristate'}),
    'QComboBox': (('QWidget',), {'addItems', 'currentText', 'lineEdit'}),
    'QAbstractSlider': (('QWidget',), {'setRange', 'setPageStep', 'setValue'}),
    'QSlider': (('QAbstractSlider',), set()),
    'QScrollBar': (('QAbstractSlider',), set()),
    'QLineEdit': (('QWidget',), {'text', 'textChanged'}),
    'QMenu': (('QWidget',), {'addAction', 'addSeparator', 'addMenu'}),
    'QMenuBar': (('QWidget',), {'addMenu'}),
    'QAction': ((), {'setShortcut'}),
}


def public_methods(name, seen=None):
    seen = set() if seen is None else seen
    if name in seen:
        return set()
    seen.add(name)
    record = resolve_type(name)
    if record and '::' in name and record['qualified_name'] != name:
        record = None
    if record:
        members = {m['name'] for m in record['members']
                   if m['access'] == 'public' and m['kind'] in ('methods', 'signals')}
        bases = record['bases']
    elif name in QT:
        bases, members = QT[name]
        members = set(members)
    else:
        return set()
    for base in bases:
        members.update(public_methods(base, seen))
    return members


def python_symbols():
    functions, methods = set(), set()
    for path in (ROOT / 'bindings/pyside6/src/fluentqt').glob('*.py'):
        tree = ast.parse(path.read_text(encoding='utf-8'))
        for node in tree.body:
            if isinstance(node, ast.FunctionDef) and not node.name.startswith('_'):
                functions.add(node.name)
            elif isinstance(node, ast.ClassDef):
                methods.update(n.name for n in node.body
                               if isinstance(n, ast.FunctionDef) and not n.name.startswith('_'))
    root = ast.parse((ROOT / 'bindings/pyside6/src/fluentqt/__init__.py').read_text(encoding='utf-8'))
    exports = next(ast.literal_eval(node.value) for node in root.body
                   if isinstance(node, ast.Assign)
                   and any(isinstance(target, ast.Name) and target.id == '__all__' for target in node.targets))
    return functions.intersection(exports), methods


@lru_cache(maxsize=1)
def known_callables():
    return {m['name'] for r in inventory() for m in r['members']
            if m['kind'] in ('methods', 'signals')} | set().union(*(members for _, members in QT.values()))


def has_qt_base(name, seen=None):
    seen = set() if seen is None else seen
    if name in seen:
        return False
    seen.add(name)
    record = resolve_type(name)
    if record and ('::' not in name or record['qualified_name'] == name):
        return any(has_qt_base(base, seen) for base in record['bases'])
    return name in QT or bool(re.fullmatch(r'Q[A-Z]\w*', name))


def check_text(text, available, location, python_functions, context_owner=None):
    errors, references = [], []
    for call in CALL.finditer(text):
        owner, name = call['owner'], call['name']
        allowed = (public_methods(owner) if owner else
                   python_functions if call['python'] else available)
        unverified = False
        if name not in allowed:
            # Missing from a partial Qt index is not proof that a method is
            # nonexistent. Still reject known methods attributed to the wrong
            # Fluent/Qt type, unknown owners, and missing Python exports.
            candidate = owner or context_owner
            unverified = (not call['python'] and name not in known_callables()
                          and (candidate is None or has_qt_base(candidate))
                          and (not owner or resolve_type(owner) is not None or owner in QT))
        if name not in allowed and not unverified:
            errors.append(f'{location}: no public callable {owner + "::" if owner else ""}{name}() '
                          'on the documented owner; qualify a different owner explicitly')
        reference = {'owner': owner or ('fluentqt' if call['python'] else None), 'name': name}
        if unverified:
            reference['warning'] = f'{location}: {name}() needs review against the Qt base API; the index is incomplete'
        references.append(reference)
    return errors, references


def validate(documents=None, introductions=None):
    from docs_i18n import EXTRA_LOCALE_KEYS, component_locale

    load_extra_components = documents is None
    documents = config() if documents is None else documents
    introductions = guides() if introductions is None else introductions
    components = {c['id']: c for c in catalog()['components']}
    localized_components = {lang: component_locale(lang) for lang in EXTRA_LOCALE_KEYS} if load_extra_components else {}
    functions, extensions = python_symbols()
    errors, coverage = [], []
    # Guides introduce several types. Explicitly qualified names are still
    # checked against that exact type; unqualified names use the public inventory.
    guide_methods = set().union(*(public_methods(r['qualified_name']) for r in inventory()),
                                *(members for _, members in QT.values()), functions, extensions)
    for document in [*documents, *introductions]:
        cid = document['id']
        component = components.get(cid)
        owner = component['cpp']['qualified_type'] if component else None
        available = public_methods(owner) if owner else guide_methods
        desc_translations = dict(document['description'])
        if component and localized_components:
            for lang in EXTRA_LOCALE_KEYS:
                loc_doc = localized_components[lang].get(cid)
                if loc_doc:
                    desc_translations[lang] = loc_doc['description']
        fields = [('description', desc_translations)]
        for example in document.get('examples', []):
            ex_translations = dict(example['description'])
            if component and localized_components:
                for lang in EXTRA_LOCALE_KEYS:
                    loc_doc = localized_components[lang].get(cid)
                    if not loc_doc:
                        continue
                    if example['sample'] in loc_doc.get('examples', {}):
                        ex_translations[lang] = loc_doc['examples'][example['sample']]['description']
                    elif 'usage' in loc_doc and len(document.get('examples', [])) == 1:
                        ex_translations[lang] = loc_doc['usage']
            fields.append((f'example/{example["sample"]}', ex_translations))
        fields += [(f'section/{i}', s['body']) for i, s in enumerate(document.get('sections', []))]
        references = []
        for field, translations in fields:
            for language, text in translations.items():
                found, calls = check_text(text, available, f'{cid}/{field}/{language}', functions, owner)
                errors.extend(found)
                references.extend(calls)
        for i, section in enumerate(document.get('sections', [])):
            try:
                ast.parse(section.get('python', ''))
            except SyntaxError as error:
                errors.append(f'{cid}/section/{i}/python: {error}')
        coverage.append({'id': cid, 'owner': owner,
                         'source': component['cpp']['declaration'] if component else document['headers'],
                         'callable_references': references})
    return errors, coverage


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    errors, coverage = validate()
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps({
            'scope': 'Public callable references in bilingual prose and Python guide syntax; '
                     'not runtime behavior, argument validation or platform verification.',
            'documents': coverage, 'errors': errors,
        }, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if errors:
        print('\n'.join(errors))
        return 1
    warnings = [ref['warning'] for row in coverage for ref in row['callable_references'] if 'warning' in ref]
    for warning in warnings:
        print('Review required: ' + warning)
    print(f'Checked callable references in {len(coverage)} bilingual documents; {len(warnings)} unverified Qt references')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

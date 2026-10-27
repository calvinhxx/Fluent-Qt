"""Read public C++ declarations without Qt, a compiler, or generated bindings.

This is a declaration scanner, not a C++ compiler. Bodies and private sections
are skipped; unclassified public declarations remain in the output rather than
silently disappearing. Source locations let readers inspect the original API.
"""

from dataclasses import dataclass
import ast
import bisect
from functools import lru_cache
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
LEXER = re.compile(r'''(?P<comment>/\*[\s\S]*?\*/|//[^\n]*)
    |(?P<directive>^[ \t]*\#(?:[^\n\\]|\\[\s\S])*)
    |(?P<string>R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delimiter)"
        |"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')
    |(?P<word>[A-Za-z_]\w*)|(?P<number>\d[\w.]*)
    |(?P<operator>::|->|&&|\|\||\.\.\.|==|!=|<=|>=|<<|>>|[^\s])''', re.X | re.M)
SKIP_MACROS = {'Q_OBJECT', 'Q_GADGET', 'Q_DISABLE_COPY', 'Q_DISABLE_COPY_MOVE',
               'Q_ENUM', 'Q_ENUMS', 'Q_FLAG', 'Q_FLAGS', 'Q_CLASSINFO',
               'Q_DECLARE_METATYPE', 'Q_DECLARE_OPERATORS_FOR_FLAGS',
               'Q_INTERFACES', 'Q_DECLARE_INTERFACE', 'Q_ENUM_NS', 'Q_FLAG_NS', 'Q_NAMESPACE',
               'Q_DECLARE_PRIVATE', 'Q_DECLARE_PUBLIC'}
ACCESS = {'public', 'protected', 'private', 'signals', 'Q_SIGNALS'}
CJK = re.compile(r'[一-鿿]')


@dataclass
class Token:
    text: str
    start: int
    end: int
    kind: str


def documentation(comments):
    lines = []
    for comment in comments:
        value = re.sub(r'^/\*+<?|\*/$|^///?<?', '', comment.strip())
        lines.extend(re.sub(r'^\s*\*\s?', '', line).strip() for line in value.splitlines())
    text = '\n'.join(lines).strip()
    en, marker, zh = text.partition('zh_CN:')
    en = re.sub(r'^@brief\s*', '', en)
    en = re.split(r'\n\s*\n|@(?:param|return|returns|note|see|warning|code|endcode)\b', en)[0]
    zh = re.split(r'\n\s*\n|@(?:param|return|returns|note|see|warning|code|endcode)\b', zh)[0]
    # A later zh_CN: starts another pair. English details written straight after the
    # Chinese line form a trailing run without CJK; ASCII lines between CJK lines stay.
    zh_lines = zh.split('zh_CN:', 1)[0].splitlines()
    while len(zh_lines) > 1 and not CJK.search(zh_lines[-1]):
        zh_lines.pop()
    zh = '\n'.join(zh_lines)
    en, zh = (' '.join(value.split()).strip() for value in (en, zh))
    if not marker and CJK.search(en):
        return {'en': '', 'zh': en}
    return {'en': en, 'zh': zh}


class Scanner:
    def __init__(self, source, path=''):
        self.source, self.path = source, path
        self.tokens = [Token(m.group(), m.start(), m.end(), m.lastgroup)
                       for m in LEXER.finditer(source)]
        self.newlines = [m.start() for m in re.finditer('\n', source)]
        self.types = []
        self.namespaces = {}

    def text(self, tokens):
        if not tokens:
            return ''
        # Preserve spelling and string literals, but collapse layout whitespace.
        value = self.source[tokens[0].start:tokens[-1].end]
        pieces, cursor = [], 0
        for match in LEXER.finditer(value):
            pieces.append(re.sub(r'\s+', ' ', value[cursor:match.start()]))
            pieces.append(' ' if match.lastgroup in ('comment', 'directive') else match.group())
            cursor = match.end()
        return ''.join(pieces).strip()

    def line(self, token):
        return bisect.bisect_left(self.newlines, token.start) + 1

    def trailing_doc(self, anchor, limit, leading):
        """Return the docs and last consumed index for the declaration ending at anchor.

        `///<` and `/**<` always document that declaration. A plain comment on the
        same line does too, unless only the leading comment carries the bilingual
        zh_CN: contract; that keeps a real brief over a terse note such as `// 0-255`
        while a plain heading above still yields to the trailing description. The
        comment is consumed either way so it cannot leak into the next declaration.
        """
        following = anchor + 1
        if following >= limit or self.tokens[following].kind != 'comment':
            return leading, anchor
        comment = self.tokens[following].text
        explicit = re.match(r'^(?:///+|/\*+)<', comment)
        if not explicit and self.line(self.tokens[following]) != self.line(self.tokens[anchor]):
            return leading, anchor
        keep_leading = not explicit and 'zh_CN:' not in comment and any('zh_CN:' in c for c in leading)
        return (leading if keep_leading else [comment]), following

    def matching(self, index, opening='{', closing='}'):
        depth = 0
        for j in range(index, len(self.tokens)):
            if self.tokens[j].kind in ('comment', 'directive', 'string'):
                continue
            if self.tokens[j].text == opening:
                depth += 1
            elif self.tokens[j].text == closing:
                depth -= 1
                if not depth:
                    return j
        raise ValueError(f'Unbalanced {opening} in {self.path}:{self.line(self.tokens[index])}')

    def member(self, owner, kind, name, tokens, comment, access='public', **extra):
        if access == 'private' or not tokens:
            return
        prefix = self.source[max(0, tokens[0].start - 240):tokens[0].start]
        deprecated = re.search(r'Q_DECL_DEPRECATED_X\("([^"\n]*)"\)\s*$', prefix)
        if deprecated:
            extra['deprecated'] = deprecated[1]
        owner['members'].append({'kind': kind, 'name': name,
            'signature': self.text(tokens).rstrip(';'), 'description': documentation(comment),
            'access': access, 'source': self.path, 'line': self.line(tokens[0]), **extra})

    def namespace(self, scope):
        key = '::'.join(scope) or 'Global'
        return self.namespaces.setdefault(key, {'name': key, 'qualified_name': key,
            'kind': 'namespace', 'source': self.path, 'line': 1, 'bases': [], 'members': [],
            'description': {'en': '', 'zh': ''}})

    def parse(self, start=0, end=None, scope=(), owner=None, default_access='public'):
        end = len(self.tokens) if end is None else end
        i, access, comment = start, default_access, []
        while i < end:
            token = self.tokens[i]
            value = token.text
            if token.kind == 'comment':
                comment.append(value); i += 1; continue
            if token.kind == 'directive':
                i += 1; continue
            if value in (';', '}'):
                comment = []; i += 1; continue
            # Access labels, including Qt's public slots and signals sections.
            if value in ACCESS:
                j = i + 1
                if j < end and self.tokens[j].text in ('slots', 'Q_SLOTS'):
                    j += 1
                if j < end and self.tokens[j].text == ':':
                    access = 'signals' if value in ('signals', 'Q_SIGNALS') else value
                    # A comment on the label's line describes the section, not its first member.
                    _, j = self.trailing_doc(j, end, [])
                    comment = []; i = j + 1; continue
            if value in SKIP_MACROS:
                i += 1
                if i < end and self.tokens[i].text == '(':
                    i = self.matching(i, '(', ')') + 1
                comment = []; continue
            if value in ('Q_DECL_DEPRECATED', 'Q_DECL_DEPRECATED_X', 'Q_REQUIRED_RESULT',
                         'Q_INVOKABLE', 'Q_NODISCARD', 'Q_SIGNAL', 'Q_SLOT'):
                i += 1
                if i < end and self.tokens[i].text == '(':
                    i = self.matching(i, '(', ')') + 1
                continue
            if value == 'Q_PROPERTY':
                close = self.matching(i + 1, '(', ')')
                parts = self.tokens[i + 2:close]
                flags = {'READ', 'WRITE', 'MEMBER', 'RESET', 'NOTIFY', 'BINDABLE', 'CONSTANT', 'FINAL',
                         'DESIGNABLE', 'SCRIPTABLE', 'STORED', 'USER', 'REVISION', 'REQUIRED'}
                first = next((n for n, t in enumerate(parts) if t.text in flags), len(parts))
                if owner and first >= 2:
                    attrs = {}
                    for n, t in enumerate(parts):
                        if t.text in flags:
                            attrs[t.text.lower()] = parts[n + 1].text if n + 1 < len(parts) and parts[n + 1].text not in flags else True
                    self.member(owner, 'properties', parts[first - 1].text, parts[:first], comment,
                                type=self.text(parts[:first - 1]), **attrs)
                comment = []; i = close + 1; continue
            if value == 'Q_DECLARE_FLAGS':
                close = self.matching(i + 1, '(', ')')
                if owner:
                    self.member(owner, 'types', self.tokens[i + 2].text, self.tokens[i:close + 1], comment, access)
                comment = []; i = close + 1; continue

            # Find the end of one declaration without entering parameters or bodies.
            j, parens, brackets, angles, function_open = i, 0, 0, 0, None
            while j < end:
                t = self.tokens[j]
                if t.kind in ('comment', 'directive', 'string'):
                    j += 1; continue
                if t.text == '(':
                    if not parens and not brackets and not angles and function_open is None and self.tokens[j - 1].text not in ('decltype', 'alignas', 'noexcept'):
                        function_open = j
                    parens += 1
                elif t.text == ')':
                    parens -= 1
                elif t.text == '[':
                    brackets += 1
                elif t.text == ']':
                    brackets -= 1
                elif not parens and t.text == '<' and (j == i or self.tokens[j - 1].text != 'operator'):
                    angles += 1
                elif not parens and t.text in ('>', '>>'):
                    angles = max(0, angles - len(t.text))
                if not parens and not brackets and t.text in (';', '{'):
                    break
                j += 1
            if j >= end:
                break
            prefix = [t for t in self.tokens[i:j] if t.kind not in ('comment', 'directive')]
            words = [t.text for t in prefix]
            opening = self.tokens[j].text == '{'
            close = self.matching(j) if opening else j
            if not opening:
                comment, close = self.trailing_doc(close, end, comment)
            target = owner or self.namespace(scope)
            class_match = re.search(r'(?:^|>)\s*(class|struct)\s+(?:[A-Z_][A-Z0-9_]*\s+)?(\w+)(?:\s+final)?\s*(?::(.*))?$', self.text(prefix)) if opening else None

            if value == 'namespace' and opening:
                names = tuple(t.text for t in prefix[1:] if t.kind == 'word' and t.text != 'inline')
                if names and not any(name in ('detail', 'private') or name.endswith('_detail') for name in names):
                    self.parse(j + 1, close, scope + names)
            elif class_match:
                # A template parameter's `class T` is not the declared type.
                if access != 'private':
                    kind, name, bases = class_match.groups()
                    parsed_bases = []
                    for base in (bases or '').split(','):
                        parts = base.strip().split()
                        if not parts:
                            continue
                        base_access = next((p for p in parts if p in ACCESS), 'public' if kind == 'struct' else 'private')
                        if base_access == 'public':
                            parsed_bases.append(parts[-1])
                    record = {'name': name, 'qualified_name': '::'.join((*scope, name)),
                        'kind': kind, 'source': self.path, 'line': self.line(token), 'bases': parsed_bases,
                        'description': documentation(comment), 'members': []}
                    self.types.append(record)
                    if owner:
                        self.member(owner, 'types', name, prefix, comment, access,
                                    qualified_name=record['qualified_name'])
                    self.parse(j + 1, close, (*scope, name), record, 'public' if kind == 'struct' else 'private')
            elif value == 'enum' and opening:
                name = next((t.text for t in prefix[1:] if t.kind == 'word' and t.text not in ('class', 'struct')), '')
                values, begin, nested, docs, n = [], j + 1, 0, [], j + 1
                while n <= close:
                    t = self.tokens[n]
                    if t.kind == 'comment':
                        docs.append(t.text)
                    elif t.text in ('(', '[', '{'):
                        nested += 1
                    elif t.text in (')', ']'):
                        nested -= 1
                    if n == close or t.text == ',' and nested == 0:
                        entry = [e for e in self.tokens[begin:n] if e.kind not in ('comment', 'directive')]
                        if t.text == ',':
                            docs, n = self.trailing_doc(n, close, docs)
                        if entry:
                            values.append({'name': entry[0].text, 'signature': self.text(entry), 'description': documentation(docs)})
                        begin, docs = n + 1, []
                    n += 1
                self.member(target, 'enums', name or '(anonymous)', prefix, comment, access, values=values)
            elif words and words[0] not in ('friend', 'class', 'struct', 'namespace'):
                if words[0] in ('using', 'typedef'):
                    name = words[1] if words[0] == 'using' else words[-1]
                    self.member(target, 'types', name, prefix, comment, access)
                elif function_open is not None and '=' not in words[:max(0, function_open - i)]:
                    before = self.tokens[i:function_open]
                    operator = next((n for n, t in enumerate(before) if t.text == 'operator'), None)
                    name = self.text(before[operator:]) if operator is not None else before[-1].text
                    if len(before) > 1 and before[-2].text == '~':
                        name = '~' + name
                    kind = 'signals' if access == 'signals' else 'constructors' if owner and name == owner['name'] else 'methods'
                    if name.startswith('~'):
                        kind = 'destructors'
                    # Constructor initializer lists are implementation, not signature.
                    signature = prefix
                    last_parameter = self.matching(function_open, '(', ')')
                    colon = next((n for n, t in enumerate(signature) if t.text == ':' and t.start > self.tokens[last_parameter].end), None)
                    if colon is not None:
                        signature = signature[:colon]
                    self.member(target, kind, name, signature, comment, 'public' if access == 'signals' else access)
                else:
                    field = prefix
                    if opening:
                        field = self.tokens[i:close + 1]
                    declaration = prefix[:words.index('=')] if '=' in words else prefix
                    names = [t.text for t in declaration if t.kind == 'word']
                    self.member(target, 'constants' if 'constexpr' in words or 'const' in words else 'fields',
                                names[-1] if names else self.text(prefix), field, comment, access)
            comment = []
            i = close + 1
        return self.types + [v for v in self.namespaces.values() if v['members']]


def parse_header(source, path=''):
    records = Scanner(source, path).parse()
    for record in records:
        unique = {}
        for member in record['members']:
            unique.setdefault((member['kind'], member['signature'], member['access']), member)
        record['members'] = list(unique.values())
        properties = {m['name']: m for m in record['members'] if m['kind'] == 'properties'}
        for member in record['members']:
            if member['kind'] == 'properties':
                continue
            for prop in properties.values():
                if member['name'] in (prop.get('read'), prop.get('write'), prop.get('reset'), prop.get('notify')):
                    if not any(member['description'].values()):
                        member['description'] = dict(prop['description'])
                    if member['name'] == prop.get('read') and not any(prop['description'].values()):
                        prop['description'] = dict(member['description'])
    return records


@lru_cache(maxsize=1)
def inventory():
    from generate_api_reference import _installed_header_paths
    records = []
    for path in _installed_header_paths():
        records.extend(parse_header((ROOT / path).read_text(encoding='utf-8'), path))
    return records


def resolve_type(name, records=None):
    if records is None:
        exact, simple = type_lookup()
        return exact.get(name) or simple.get(name.split('::')[-1])
    exact = next((r for r in records if r['qualified_name'] == name), None)
    if exact:
        return exact
    simple = [r for r in records if r['name'] == name.split('::')[-1]]
    return simple[0] if len(simple) == 1 else None


@lru_cache(maxsize=1)
def type_lookup():
    records = inventory()
    exact = {r['qualified_name']: r for r in records if r['kind'] != 'namespace'}
    simple = {}
    for record in records:
        name = record['name']
        simple[name] = None if name in simple else record
    for record in records:
        if record['kind'] == 'namespace' and record['qualified_name'] not in exact:
            exact[record['qualified_name']] = record
    return exact, simple


def inherited_types(record, seen=None):
    seen = set() if seen is None else seen
    result = []
    for name in record['bases']:
        base = resolve_type(name)
        if base and base['qualified_name'] not in seen:
            seen.add(base['qualified_name']); result.append(base)
            result.extend(inherited_types(base, seen))
    return result


@lru_cache(maxsize=None)
def python_extensions(module, type_name):
    """Python-owned methods must not be invented from C++ signatures."""
    path = ROOT / 'bindings/pyside6/src' / (module.replace('.', '/') + '.py')
    if not path.is_file():
        return []
    tree = ast.parse(path.read_text(encoding='utf-8'))
    cls = next((n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == type_name), None)
    if cls is None:
        return []
    result = []
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and not node.name.startswith('_'):
            arguments = ast.unparse(node.args)
            arguments = re.sub(r'^(self|cls)(?:,\s*|$)', '', arguments)
            signature = node.name + '(' + arguments + ')'
            if node.returns:
                signature += ' -> ' + ast.unparse(node.returns)
            result.append({'kind': 'python', 'name': node.name, 'signature': signature,
                'description': {'en': ast.get_docstring(node) or '', 'zh': ''},
                'source': str(path.relative_to(ROOT)), 'line': node.lineno, 'access': 'public'})
    return result

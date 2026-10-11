"""Patch registered literal values, preserving every other source byte."""

import ast
import hashlib
import os
from pathlib import Path
import stat
import tempfile


def digest(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def replace_source(path: Path, expected: bytes, replacement: bytes):
    """Compare before replacement; never roll back over an external edit."""
    if path.read_bytes() != expected:
        raise RuntimeError("Source changed outside this edit. Reload and inspect it again.")
    mode = path.stat().st_mode & 0o777
    descriptor, temporary = tempfile.mkstemp(prefix="." + path.name, dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(replacement)
            stream.flush()
            os.fsync(stream.fileno())
            if hasattr(os, "fchmod"):
                os.fchmod(stream.fileno(), mode)
        if not hasattr(os, "fchmod"):
            # Windows exposes chmod only by path. Apply it to the closed
            # temporary file, preserving the atomic replace and conflict check.
            os.chmod(temporary, mode)
        if path.read_bytes() != expected:
            raise RuntimeError("Source changed while preparing the edit; no replacement was made.")
        os.replace(temporary, path)
    finally:
        try:
            Path(temporary).unlink(missing_ok=True)
        except PermissionError:
            if os.name != "nt":
                raise
            # Copying a read-only source's mode also makes our temporary file
            # read-only. Clear that flag only on the file created above so a
            # failed replace can still clean up without changing the source.
            os.chmod(temporary, mode | stat.S_IWRITE)
            Path(temporary).unlink(missing_ok=True)


def dictionary(node):
    if not isinstance(node, ast.Dict):
        raise ValueError("PREVIEW_EDITS and each region must be literal dictionaries.")
    items = {}
    for key, value in zip(node.keys, node.values):
        if not isinstance(key, ast.Constant) or type(key.value) is not str or key.value in items:
            raise ValueError("Editable dictionaries require unique string keys and no unpacking.")
        items[key.value] = value
    return items


class SourceDocument:
    def __init__(self, path: Path):
        self.path = Path(path).resolve()
        self.content = self.path.read_bytes()
        if len(self.content) > 1024 * 1024:
            raise ValueError("Editable scene exceeds 1 MiB.")
        # UTF-8 keeps AST byte columns and the patch byte offsets equivalent.
        try:
            tree = ast.parse(self.content.decode("utf-8"), filename=str(self.path))
        except SyntaxError as error:
            raise ValueError(f"The scene source cannot be parsed at line {error.lineno}.") from error
        assignments = [node for node in tree.body if isinstance(node, ast.Assign)
                       and any(isinstance(target, ast.Name) and target.id == "PREVIEW_EDITS"
                               for target in node.targets)]
        if len(assignments) != 1 or len(assignments[0].targets) != 1:
            raise ValueError("Declare exactly one module-level PREVIEW_EDITS = {...} dictionary.")
        self.regions = {key: dictionary(value) for key, value in dictionary(assignments[0].value).items()}
        self.revision = digest(self.content)
        self.lines = self.content.splitlines(keepends=True)

    def value(self, region, key):
        try:
            node = self.regions[region][key]
        except KeyError as error:
            raise ValueError(f"Missing registered source value: {region}.{key}") from error
        value = ast.literal_eval(node)
        if type(value) not in (int, bool, str):
            raise ValueError("Editable values must be integer, boolean, or choice literals.")
        return value

    def location(self, region, key):
        node = self.regions[region][key]
        prefix = self.lines[node.lineno - 1][:node.col_offset].decode("utf-8")
        return {"file": str(self.path), "line": node.lineno, "column": len(prefix) + 1,
                "column_unit": "unicode-codepoint"}

    def patch(self, region, changes, fields, revision):
        if revision != self.revision:
            raise RuntimeError("The source revision is stale. Inspect the selected region again.")
        allowed = {field.key: field for field in fields}
        if not isinstance(changes, dict) or not changes or changes.keys() - allowed.keys():
            raise ValueError("Supply only the selected region's registered fields.")
        patches = []
        for key, value in changes.items():
            allowed[key].validate(value)
            if value == self.value(region, key):
                continue
            node = self.regions[region][key]
            begin = sum(map(len, self.lines[:node.lineno - 1])) + node.col_offset
            end = sum(map(len, self.lines[:node.end_lineno - 1])) + node.end_col_offset
            patches.append((begin, end, repr(value).encode("utf-8")))
        if not patches:
            raise ValueError("No values changed.")
        result = self.content
        for begin, end, value in sorted(patches, reverse=True):
            result = result[:begin] + value + result[end:]
        ast.parse(result.decode("utf-8"), filename=str(self.path))
        return result

"""Compile a Designer form with the active PySide6 uic, including Qt 6.2."""
import argparse
import ast
from itertools import accumulate
from pathlib import Path
import shutil
import subprocess
import sys
import sysconfig


def portable_string_lists(source):
    """Convert Qt 6.2 uic's C++ QStringList() << item expressions to Python lists."""
    if b"QStringList()" not in source:
        return source
    offsets = list(accumulate(map(len, source.splitlines(keepends=True)), initial=0))
    replacements = []

    def span(node):
        return offsets[node.lineno - 1] + node.col_offset, offsets[node.end_lineno - 1] + node.end_col_offset

    def visit(node):
        head = node
        values = []
        while isinstance(head, ast.BinOp) and isinstance(head.op, ast.LShift):
            start, end = span(head.right)
            values.append(source[start:end])
            head = head.left
        if (isinstance(head, ast.Call) and isinstance(head.func, ast.Name)
                and head.func.id == "QStringList" and not head.args and not head.keywords):
            start, end = span(node)
            replacements.append((start, end, b"[" + b", ".join(reversed(values)) + b"]"))
            return
        for child in ast.iter_child_nodes(node):
            visit(child)

    visit(ast.parse(source))
    for start, end, value in sorted(replacements, reverse=True):
        source = source[:start] + value + source[end:]
    return source


def compile_form(path):
    uic = shutil.which("pyside6-uic", path=sysconfig.get_path("scripts"))
    if not uic:
        raise RuntimeError("Install PySide6 in this Python environment to use pyside6-uic.")
    return portable_string_lists(subprocess.check_output([uic, str(path)]))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("form", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()
    code = compile_form(args.form)
    if args.output:
        args.output.write_bytes(code)
    else:
        sys.stdout.buffer.write(code)

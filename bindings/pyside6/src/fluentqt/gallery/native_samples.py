"""Behavioral Python ports of canonical C++ Gallery SampleCards.

Every registered builder owns one or more exact ``(route_id, sample_id)`` keys.
Returning a ``PreviewResult`` with ``native-equivalent`` means the preview and
the displayed Python source exercise the same public capability as the native
card.  Unregistered cards intentionally fall back to ``component-smoke`` and
are rejected by the parity acceptance test.
"""

from __future__ import annotations

import ast
from collections.abc import Callable, Mapping, Sequence
import re

import fluentqt
from PySide6.QtCore import QDate, QItemSelectionModel, QRect, QRectF, QSize, Qt, QTime
from PySide6.QtGui import (
    QColor,
    QFont,
    QGuiApplication,
    QPainter,
    QPainterPath,
    QPixmap,
    QStandardItem,
    QStandardItemModel,
)
from PySide6.QtWidgets import (
    QHBoxLayout,
    QListView,
    QSizePolicy,
    QStyle,
    QStyledItemDelegate,
    QVBoxLayout,
    QWidget,
)

from .catalog import SAMPLE_BY_KEY
from .samples import PreviewResult


Builder = Callable[[str, QWidget | None], PreviewResult]
_BUILDERS: dict[tuple[str, str], Builder] = {}
SourceSpec = tuple[str, str]


def _apply_root_layout_contract(
    route_id: str,
    sample_id: str,
    source: str,
    widget_name: str,
) -> str:
    """Give source-driven roots the zero-margin C++ sample-group contract."""

    sample = SAMPLE_BY_KEY[(route_id, sample_id)]
    orientation = sample.preview_orientation
    expected_layout_type = {
        "horizontal": "QHBoxLayout",
        "vertical": "QVBoxLayout",
    }.get(orientation)
    if expected_layout_type is None:
        return source
    expected_alignment = {
        "horizontal": (
            "Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter"
        ),
        "vertical": (
            "Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft"
        ),
    }[orientation]
    pattern = re.compile(
        r"^(?P<layout>[A-Za-z_][A-Za-z0-9_]*) = "
        r"Q(?:HBox|VBox)Layout\({0}\)$".format(re.escape(widget_name)),
        re.MULTILINE,
    )
    match = pattern.search(source)
    if match is None:
        return source
    layout_name = match.group("layout")
    replacement = "{0} = {1}({2})\n".format(
        layout_name, expected_layout_type, widget_name
    )
    replacement += "{0}.setContentsMargins(0, 0, 0, 0)\n".format(layout_name)
    replacement += "{0}.setSpacing({1})".format(
        layout_name, sample.preview_spacing
    )
    replacement += "\n{0}.setAlignment({1})".format(
        layout_name, expected_alignment
    )
    qt_core_imports = re.findall(
        r"from\s+PySide6\.QtCore\s+import\s+(?:\([^)]*\)|[^\n]*)",
        source,
        flags=re.MULTILINE,
    )
    if not any(re.search(r"\bQt\b", imported) for imported in qt_core_imports):
        source = "from PySide6.QtCore import Qt\n" + source
    return pattern.sub(replacement, source, count=1)


def _apply_root_parent_contract(source: str, widget_name: str) -> str:
    """Construct QWidget sample roots with the C++ preview parent immediately.

    Reparenting an already-polished widget tree can leave native child effects
    with stale source offsets.  The C++ Gallery passes the preview host into
    every sample factory up front, so source-driven Python samples do the same
    while remaining executable without a Gallery host.
    """

    pattern = re.compile(
        r"^{0} = QWidget\(\)$".format(re.escape(widget_name)),
        re.MULTILINE,
    )
    return pattern.sub(
        "{0} = QWidget(globals().get('gallery_parent'))".format(widget_name),
        source,
        count=1,
    )


def native_samples(route_id: str, *sample_ids: str):
    """Register one builder for exact native SampleCard ids."""

    def decorate(builder: Builder) -> Builder:
        for sample_id in sample_ids:
            key = (route_id, sample_id)
            if key in _BUILDERS:
                raise RuntimeError("duplicate Python Gallery sample builder: {0}".format(key))
            _BUILDERS[key] = builder
        return builder

    return decorate


def _source(*lines: str, imports: str = "") -> str:
    prefix = "import fluentqt\n"
    if imports:
        prefix += imports.rstrip() + "\n"
    return prefix + "\n" + "\n".join(lines) + "\n"


def _assigned_names(node: ast.AST) -> set[str]:
    names: set[str] = set()
    for child in ast.walk(node):
        if isinstance(child, ast.Name) and isinstance(child.ctx, ast.Store):
            names.add(child.id)
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
        names.add(node.name)
    return names


def _format_import(node: ast.Import | ast.ImportFrom) -> str:
    rendered = ast.unparse(node)
    if len(rendered) <= 92 or not isinstance(node, ast.ImportFrom):
        return rendered
    module = "." * node.level + (node.module or "")
    names = []
    for alias in node.names:
        value = alias.name
        if alias.asname:
            value += " as " + alias.asname
        names.append("    " + value + ",")
    return "from {0} import (\n{1}\n)".format(module, "\n".join(names))


def _strip_gallery_parent(source: str) -> str:
    parent = r"globals\(\)\.get\((['\"])gallery_parent\1\)"
    source = re.sub(r",\s*" + parent, "", source)
    source = re.sub(parent + r"\s*,\s*", "", source)
    return re.sub(parent, "", source)


def _snake_case(name: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def _call_receiver(call: ast.Call) -> tuple[str, str] | None:
    if not isinstance(call.func, ast.Attribute):
        return None
    method = call.func.attr
    value: ast.AST = call.func.value
    while isinstance(value, (ast.Attribute, ast.Call)):
        if isinstance(value, ast.Attribute):
            value = value.value
        elif isinstance(value.func, ast.Attribute):
            value = value.func.value
        else:
            break
    if isinstance(value, ast.Name):
        return value.id, method
    return None


def _covered_constructor_names(
    node: ast.AST, covered_types: Sequence[str]
) -> set[str]:
    covered = set(covered_types)
    names: set[str] = set()
    for child in ast.walk(node):
        if not isinstance(child, (ast.Assign, ast.AnnAssign)):
            continue
        value = child.value
        if not isinstance(value, ast.Call):
            continue
        function = value.func
        if not (
            isinstance(function, ast.Attribute)
            and isinstance(function.value, ast.Name)
            and function.value.id == "fluentqt"
            and function.attr in covered
        ):
            continue
        names.update(_assigned_names(child))
    return names


def _focused_short_function(
    node: ast.FunctionDef | ast.AsyncFunctionDef,
    covered_types: Sequence[str],
    canonical_members: set[str],
) -> ast.FunctionDef | ast.AsyncFunctionDef:
    """Keep only canonical component operations from a long popup helper."""

    covered = set(covered_types)
    local_targets: set[str] = set()
    constructor_statements: set[int] = set()
    for child in ast.walk(node):
        if not isinstance(child, (ast.Assign, ast.AnnAssign)):
            continue
        value = child.value
        if not isinstance(value, ast.Call):
            continue
        function = value.func
        if not (
            isinstance(function, ast.Attribute)
            and isinstance(function.value, ast.Name)
            and function.value.id == "fluentqt"
            and function.attr in covered
        ):
            continue
        local_targets.update(_assigned_names(child))
        constructor_statements.add(id(child))
    if not local_targets:
        return node

    kept: list[ast.stmt] = []
    seen: set[int] = set()
    for child in ast.walk(node):
        if not isinstance(child, (ast.Assign, ast.AnnAssign, ast.Expr)):
            continue
        if id(child) in seen:
            continue
        names = {
            item.id
            for item in ast.walk(child)
            if isinstance(item, ast.Name)
        }
        attributes = {
            item.attr
            for item in ast.walk(child)
            if isinstance(item, ast.Attribute)
        }
        if id(child) not in constructor_statements and not (
            names & local_targets and attributes & canonical_members
        ):
            continue
        kept.append(child)
        seen.add(id(child))
    kept.sort(key=lambda statement: (statement.lineno, statement.col_offset))
    if not kept:
        return node
    focused = ast.FunctionDef(
        name=node.name,
        args=node.args,
        body=kept,
        decorator_list=node.decorator_list,
        returns=node.returns,
        type_comment=getattr(node, "type_comment", None),
    )
    ast.copy_location(focused, node)
    if len(ast.unparse(focused).splitlines()) >= len(
        ast.unparse(node).splitlines()
    ):
        return node
    return focused


def _focused_canonical_function(
    node: ast.FunctionDef | ast.AsyncFunctionDef,
    canonical_members: set[str],
) -> ast.FunctionDef | ast.AsyncFunctionDef:
    """Keep the public operations represented by the matching C++ lambda."""

    selected: list[ast.stmt] = []
    for statement in node.body:
        attributes = {
            child.attr
            for child in ast.walk(statement)
            if isinstance(child, ast.Attribute)
        }
        if attributes & canonical_members:
            selected.append(statement)
    if not selected or len(selected) == len(node.body):
        return node

    focused = ast.FunctionDef(
        name=node.name,
        args=node.args,
        body=selected,
        decorator_list=node.decorator_list,
        returns=node.returns,
        type_comment=getattr(node, "type_comment", None),
    )
    ast.copy_location(focused, node)
    return focused


def _calls_named_helper(statement: ast.stmt, helper_names: set[str]) -> bool:
    if not isinstance(statement, (ast.Assign, ast.AnnAssign)):
        return False
    value = statement.value
    return (
        isinstance(value, ast.Call)
        and isinstance(value.func, ast.Name)
        and value.func.id in helper_names
    )


class _PreviewParentStripper(ast.NodeTransformer):
    """Remove Gallery container parents from public component constructors."""

    def visit_Call(self, node: ast.Call) -> ast.AST:
        self.generic_visit(node)
        function = node.func
        if not (
            isinstance(function, ast.Attribute)
            and isinstance(function.value, ast.Name)
            and function.value.id == "fluentqt"
            and function.attr[:1].isupper()
        ):
            return node
        preview_parents = {"root", "surface", "panel"}
        node.args = [
            argument
            for argument in node.args
            if not (
                isinstance(argument, ast.Name)
                and argument.id in preview_parents
            )
        ]
        return node


def _canonical_outline(
    module: ast.Module,
    body_index: int,
    route_id: str,
    sample_id: str,
    covered_types: Sequence[str],
) -> str | None:
    """Select the Python statements matching the canonical C++ snippet."""

    sample = SAMPLE_BY_KEY.get((route_id, sample_id))
    if sample is None:
        return None
    cpp = sample.cpp_snippet
    canonical_members = set(
        re.findall(r"(?:->|\.)([A-Za-z_][A-Za-z0-9_]*)\s*\(", cpp)
    )
    canonical_members.update(
        re.findall(r"(?:->|\.)([A-Za-z_][A-Za-z0-9_]*)\s*=", cpp)
    )
    canonical_members.update(
        re.findall(r"::([a-z][A-Za-z0-9_]*)\b", cpp)
    )
    canonical_members.update(
        _snake_case(name) for name in tuple(canonical_members)
    )
    if not canonical_members:
        return None

    body = module.body[body_index:]
    helper_names = {
        name
        for statement in module.body[:body_index]
        for name in _assigned_names(statement)
        if isinstance(
            statement, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)
        )
    }
    target_names: set[str] = set()
    for statement in body:
        target_names.update(
            _covered_constructor_names(statement, covered_types)
        )

    definitions: dict[str, int] = {}
    loads: list[set[str]] = []
    calls: list[set[tuple[str, str]]] = []
    attributes: list[set[str]] = []
    stored_attributes: list[set[str]] = []
    for index, statement in enumerate(body):
        for name in _assigned_names(statement):
            definitions.setdefault(name, index)
        loads.append(
            {
                node.id
                for node in ast.walk(statement)
                if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
            }
        )
        calls.append(
            {
                receiver
                for node in ast.walk(statement)
                if isinstance(node, ast.Call)
                for receiver in [_call_receiver(node)]
                if receiver is not None
            }
        )
        attributes.append(
            {
                node.attr
                for node in ast.walk(statement)
                if isinstance(node, ast.Attribute)
            }
        )
        stored_attributes.append(
            {
                node.attr
                for node in ast.walk(statement)
                if isinstance(node, ast.Attribute)
                and isinstance(node.ctx, ast.Store)
            }
        )

    receiver_hits: dict[str, int] = {}
    for statement_calls in calls:
        for receiver, member in statement_calls:
            if member in canonical_members and receiver in definitions:
                receiver_hits[receiver] = receiver_hits.get(receiver, 0) + 1
    target_names.update(
        receiver for receiver, count in receiver_hits.items() if count >= 2
    )
    if not target_names:
        return None

    selected: set[int] = {
        index
        for index, statement in enumerate(body)
        if _covered_constructor_names(statement, covered_types)
    }
    tracked = set(target_names)
    ignored_dependencies = {
        "root",
        "surface",
        "panel",
        "controls",
        "status",
        "layout",
    }
    changed = True
    while changed:
        changed = False
        for index, statement in enumerate(body):
            receiver_match = any(
                receiver in tracked and member in canonical_members
                for receiver, member in calls[index]
            ) or bool(
                attributes[index] & canonical_members
                and loads[index] & tracked
            )
            compound_match = isinstance(
                statement,
                (ast.FunctionDef, ast.AsyncFunctionDef, ast.For, ast.While),
            ) and bool(loads[index] & tracked) and any(
                member in canonical_members for _receiver, member in calls[index]
            )
            definition_match = bool(_assigned_names(statement) & tracked)
            attribute_store_match = bool(
                stored_attributes[index] & canonical_members
                and loads[index] & tracked
            )
            if definition_match and _calls_named_helper(statement, helper_names):
                definition_match = False
            if (
                receiver_match
                or compound_match
                or definition_match
                or attribute_store_match
            ):
                if index not in selected:
                    selected.add(index)
                    changed = True

        dependency_names: set[str] = set()
        selected_definitions: set[str] = set()
        for index in selected:
            dependency_names.update(loads[index])
            selected_definitions.update(_assigned_names(body[index]))
        for name in dependency_names:
            lowered = name.lower()
            if name not in definitions:
                continue
            if name in ignored_dependencies or "layout" in lowered:
                continue
            if name not in tracked:
                tracked.add(name)
                changed = True
        for index, statement in enumerate(body):
            if index in selected:
                continue
            if not (loads[index] & selected_definitions):
                continue
            if any(member == "connect" for _receiver, member in calls[index]):
                selected.add(index)
                changed = True

    if not selected:
        return None
    cpp_lines = len(cpp.splitlines())
    selected_nodes: list[ast.stmt] = []
    for index in sorted(selected):
        statement = body[index]
        if isinstance(statement, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if cpp_lines < 16:
                statement = _focused_short_function(
                    statement, covered_types, canonical_members
                )
            statement = _focused_canonical_function(
                statement, canonical_members
            )
        selected_nodes.append(statement)
    used_names = {
        node.id
        for statement in selected_nodes
        for node in ast.walk(statement)
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
    }
    imports: list[str] = []
    for statement in module.body:
        if isinstance(statement, ast.Import):
            aliases = [
                alias
                for alias in statement.names
                if (alias.asname or alias.name.split(".", 1)[0]) in used_names
                or alias.name == "fluentqt"
            ]
            if aliases:
                imports.append(_format_import(ast.Import(names=aliases)))
        elif isinstance(statement, ast.ImportFrom):
            if (statement.module or "").startswith("fluentqt.gallery"):
                continue
            aliases = [
                alias
                for alias in statement.names
                if (alias.asname or alias.name) in used_names
            ]
            if aliases:
                imports.append(
                    _format_import(
                        ast.ImportFrom(
                            module=statement.module,
                            names=aliases,
                            level=statement.level,
                        )
                    )
                )
    display_module = ast.Module(body=selected_nodes, type_ignores=[])
    display_module = _PreviewParentStripper().visit(display_module)
    ast.fix_missing_locations(display_module)
    rendered = ast.unparse(display_module)
    rendered = re.sub(
        r"([A-Za-z_][A-Za-z0-9_.]*)\(globals\(\)\.get\((['\"])gallery_parent\2\)\)",
        r"\1()",
        rendered,
    )
    rendered = _strip_gallery_parent(rendered)
    return "\n\n".join(
        part
        for part in (
            "\n".join(dict.fromkeys(imports)),
            rendered,
        )
        if part
    ).strip() + "\n"


def _concise_display_source(
    source: str,
    widget_name: str,
    route_id: str,
    sample_id: str,
    covered_types: Sequence[str],
) -> str:
    """Remove app-only preview infrastructure from a teaching snippet.

    The exact executable source remains in ``PreviewResult.preview_source``.
    C++ Gallery cards likewise show the component-facing setup rather than the
    implementation of reusable sample painters, delegates, and surfaces.
    """

    try:
        module = ast.parse(source)
    except SyntaxError:
        return source

    root_index = None
    for index, statement in enumerate(module.body):
        if widget_name in _assigned_names(statement):
            root_index = index
            break
    if root_index is None:
        return source

    last_leading_helper = -1
    for index, statement in enumerate(module.body[:root_index]):
        if isinstance(
            statement, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)
        ):
            last_leading_helper = index

    if last_leading_helper >= 0:
        body_index = last_leading_helper + 1
    else:
        body_index = next(
            (
                index
                for index, statement in enumerate(module.body)
                if not isinstance(statement, (ast.Import, ast.ImportFrom))
            ),
            root_index,
        )
    while body_index < len(module.body) and isinstance(
        module.body[body_index], (ast.Import, ast.ImportFrom)
    ):
        body_index += 1
    if body_index >= len(module.body):
        return source

    body_nodes = module.body[body_index:]
    used_names = {
        node.id
        for statement in body_nodes
        for node in ast.walk(statement)
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
    }
    imports: list[str] = []
    omitted_names: set[str] = set()
    for statement in module.body:
        if isinstance(statement, ast.Import):
            aliases = [
                alias
                for alias in statement.names
                if (alias.asname or alias.name.split(".", 1)[0]) in used_names
                or alias.name == "fluentqt"
            ]
            if aliases:
                imports.append(_format_import(ast.Import(names=aliases)))
        elif isinstance(statement, ast.ImportFrom):
            module_name = statement.module or ""
            aliases = [
                alias
                for alias in statement.names
                if (alias.asname or alias.name) in used_names
            ]
            if module_name.startswith("fluentqt.gallery"):
                omitted_names.update(alias.asname or alias.name for alias in aliases)
                continue
            if aliases:
                imports.append(
                    _format_import(
                        ast.ImportFrom(
                            module=statement.module,
                            names=aliases,
                            level=statement.level,
                        )
                    )
                )

    for statement in module.body[:body_index]:
        omitted_names.update(_assigned_names(statement))

    lines = source.splitlines()
    body = "\n".join(lines[module.body[body_index].lineno - 1 :]).rstrip()
    body = re.sub(
        r"QWidget\(globals\(\)\.get\((['\"])gallery_parent\1\)\)",
        "QWidget()",
        body,
    )
    body = _strip_gallery_parent(body)
    parts = ["\n".join(dict.fromkeys(imports))]
    if omitted_names & used_names:
        parts.append(
            "# Gallery-only preview drawing/model helpers are omitted."
        )
    parts.append(body)
    concise = "\n\n".join(part for part in parts if part).strip() + "\n"
    cpp_lines = len(SAMPLE_BY_KEY[(route_id, sample_id)].cpp_snippet.splitlines())
    has_public_component = any(
        "fluentqt.{0}".format(covered) in concise
        for covered in covered_types
    )
    if not has_public_component:
        relevant_helpers = [
            index
            for index, statement in enumerate(module.body[:body_index])
            if isinstance(
                statement,
                (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef),
            )
            and _covered_constructor_names(statement, covered_types)
        ]
        if relevant_helpers:
            helper_outline = _canonical_outline(
                module,
                relevant_helpers[-1],
                route_id,
                sample_id,
                covered_types,
            )
            if helper_outline is not None:
                return helper_outline
    needs_outline = (
        len(concise.splitlines()) > max(14, int(cpp_lines * 1.6))
        or "from fluentqt.gallery" in source
    )
    if needs_outline:
        outline = _canonical_outline(
            module,
            body_index,
            route_id,
            sample_id,
            covered_types,
        )
        if outline is not None and len(outline.splitlines()) < len(
            concise.splitlines()
        ):
            return outline
    return concise


def _row(parent: QWidget | None = None, spacing: int = 12) -> tuple[QWidget, QHBoxLayout]:
    root = QWidget(parent)
    layout = QHBoxLayout(root)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(spacing)
    layout.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
    return root, layout


def _column(
    parent: QWidget | None = None, spacing: int = 12
) -> tuple[QWidget, QVBoxLayout]:
    root = QWidget(parent)
    layout = QVBoxLayout(root)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(spacing)
    layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)
    return root, layout


def _hold(owner: QWidget, *values: object) -> None:
    owner._fluentqt_gallery_references = values


def _enum_name(value: object) -> str:
    """Normalize old Shiboken enum byte names for generated Python source."""

    name = value.name
    return name.decode("ascii") if isinstance(name, bytes) else str(name)


def _result(
    widget: QWidget,
    source: str,
    *covered_types: str,
    source_driven: bool = False,
    display_source: str | None = None,
) -> PreviewResult:
    return PreviewResult(
        widget=widget,
        source=display_source if display_source is not None else source,
        covered_types=tuple(covered_types),
        preview_source=source,
        parity_level="native-equivalent",
        source_driven=source_driven,
    )


def register_source_samples(
    route_id: str,
    covered_types: Sequence[str],
    samples: Mapping[str, SourceSpec],
) -> None:
    """Register source-driven cards whose preview executes displayed code.

    Source-driven cards make semantic drift impossible: the exact Python text
    shown in the Gallery is also what constructs the live preview.
    """

    sample_specs = dict(samples)

    @native_samples(route_id, *sample_specs)
    def build(sample_id: str, parent: QWidget | None) -> PreviewResult:
        widget_name, source = sample_specs[sample_id]
        source = _apply_root_parent_contract(source, widget_name)
        source = _apply_root_layout_contract(
            route_id, sample_id, source, widget_name
        )
        namespace: dict[str, object] = {
            "__name__": "fluentqt_gallery_{0}_{1}".format(
                route_id.replace("-", "_"), sample_id.replace("-", "_")
            ),
            # Source snippets may opt into the real Gallery host when an API
            # resolves behavior at construction time (for example the
            # window-scoped EditingCommandRouter). Standalone execution falls
            # back to None via globals().get("gallery_parent").
            "gallery_parent": parent,
        }
        exec(compile(source, "<{0}/{1}>".format(route_id, sample_id), "exec"), namespace)
        widget = namespace.get(widget_name)
        if not isinstance(widget, QWidget):
            raise TypeError(
                "Source-driven Gallery sample {0}/{1} did not create QWidget {2!r}"
                .format(route_id, sample_id, widget_name)
            )
        if parent is not None and widget.parentWidget() is not parent:
            widget.setParent(parent)
        widget._fluentqt_gallery_source_namespace = namespace
        return _result(
            widget,
            source,
            *covered_types,
            source_driven=True,
            display_source=_concise_display_source(
                source,
                widget_name,
                route_id,
                sample_id,
                covered_types,
            ),
        )


def _materialize_displayed_source(
    route_id: str,
    sample_id: str,
    result: PreviewResult,
    parent: QWidget | None,
) -> PreviewResult:
    """Use the displayed Python text itself as the live preview.

    Legacy hand-authored builders returned a separately constructed widget and
    an executable snippet.  Even when both were intended to match, that left
    two implementations able to drift.  Materializing the snippet here makes
    the user-visible source the single implementation for every SampleCard.
    """

    namespace: dict[str, object] = {
        "__name__": "fluentqt_gallery_{0}_{1}".format(
            route_id.replace("-", "_"), sample_id.replace("-", "_")
        ),
        "gallery_parent": parent,
    }
    preview_source = result.preview_source
    exec(
        compile(
            preview_source,
            "<{0}/{1}>".format(route_id, sample_id),
            "exec",
        ),
        namespace,
    )
    candidates: dict[int, QWidget] = {}
    for name, value in namespace.items():
        if name == "gallery_parent" or value is parent:
            continue
        if not isinstance(value, QWidget):
            continue
        widget_parent = value.parentWidget()
        if widget_parent is None or widget_parent is parent:
            candidates[id(value)] = value
    if len(candidates) != 1:
        for candidate in candidates.values():
            candidate.close()
            candidate.deleteLater()
        raise RuntimeError(
            "Displayed source for {0}/{1} created {2} preview roots; "
            "expected exactly one".format(
                route_id, sample_id, len(candidates)
            )
        )

    widget = next(iter(candidates.values()))
    widget_name = next(
        (
            name
            for name, value in namespace.items()
            if value is widget and name != "gallery_parent"
        ),
        "",
    )
    if parent is not None and widget.parentWidget() is not parent:
        widget.setParent(parent)
    widget._fluentqt_gallery_source_namespace = namespace

    original = result.widget
    if original is not widget:
        original.close()
        original.deleteLater()
    return PreviewResult(
        widget=widget,
        source=(
            result.source
            if result.source != preview_source
            else _concise_display_source(
                preview_source,
                widget_name,
                route_id,
                sample_id,
                result.covered_types,
            )
        ),
        covered_types=result.covered_types,
        preview_source=preview_source,
        parity_level=result.parity_level,
        source_driven=True,
    )


def _expander_item(
    title: str,
    detail: str,
    parent: QWidget | None = None,
) -> fluentqt.Expander:
    body = QWidget()
    body_layout = QVBoxLayout(body)
    body_layout.setContentsMargins(16, 12, 16, 14)
    body_layout.setSpacing(4)
    heading = fluentqt.Label("Additional details", body)
    heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
    heading.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    heading.setWordWrap(True)
    body_layout.addWidget(heading)
    description = fluentqt.Label(detail, body)
    description.setFluentTypography(fluentqt.FontRole.Body)
    description.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    description.setWordWrap(True)
    body_layout.addWidget(description)
    item = fluentqt.Expander(parent)
    item.setHeaderText(title)
    item.setOwnedContentWidget(body)
    return item


@native_samples("calendar-date-picker", "calendar-date-picker-basic")
def _calendar_date_picker(_sample_id: str, parent: QWidget | None) -> PreviewResult:
    picker = fluentqt.CalendarDatePicker(parent)
    picker.setPlaceholderText("Pick a date")
    selected_dates: list[QDate] = []
    picker.dateChanged.connect(selected_dates.append)
    _hold(picker, selected_dates)
    return _result(
        picker,
        _source(
            "picker = fluentqt.CalendarDatePicker()",
            'picker.setPlaceholderText("Pick a date")',
            "picker.dateChanged.connect(lambda date: print(date.toString()))",
        ),
        "CalendarDatePicker",
    )


@native_samples("calendar-view", "calendar-view-basic")
def _calendar_view(_sample_id: str, parent: QWidget | None) -> PreviewResult:
    calendar = fluentqt.CalendarView(parent)
    calendar.setSelectedDate(QDate.currentDate())
    return _result(
        calendar,
        _source(
            "calendar = fluentqt.CalendarView()",
            "calendar.setSelectedDate(QDate.currentDate())",
            imports="from PySide6.QtCore import QDate",
        ),
        "CalendarView",
    )


@native_samples("date-picker", "date-picker-basic")
def _date_picker(_sample_id: str, parent: QWidget | None) -> PreviewResult:
    picker = fluentqt.DatePicker(parent)
    picker.setPlaceholderText(fluentqt.DatePicker.DateField.Month, "month")
    picker.setPlaceholderText(fluentqt.DatePicker.DateField.Day, "day")
    picker.setPlaceholderText(fluentqt.DatePicker.DateField.Year, "year")
    picker.setConfirmButtonAccessibleName("Accept date")
    picker.setCancelButtonAccessibleName("Cancel date")
    picker.setDate(QDate.currentDate())
    return _result(
        picker,
        _source(
            "picker = fluentqt.DatePicker()",
            'picker.setPlaceholderText(fluentqt.DatePicker.DateField.Month, "month")',
            'picker.setPlaceholderText(fluentqt.DatePicker.DateField.Day, "day")',
            'picker.setPlaceholderText(fluentqt.DatePicker.DateField.Year, "year")',
            'picker.setConfirmButtonAccessibleName("Accept date")',
            'picker.setCancelButtonAccessibleName("Cancel date")',
            "picker.setDate(QDate.currentDate())",
            imports="from PySide6.QtCore import QDate",
        ),
        "DatePicker",
    )


@native_samples("time-picker", "time-picker-basic")
def _time_picker(_sample_id: str, parent: QWidget | None) -> PreviewResult:
    picker = fluentqt.TimePicker(parent)
    picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Hour, "hour")
    picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Minute, "minute")
    picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Period, "AM/PM")
    picker.setConfirmButtonAccessibleName("Accept time")
    picker.setCancelButtonAccessibleName("Cancel time")
    picker.setTime(QTime(9, 30))
    picker.setMinuteIncrement(5)
    return _result(
        picker,
        _source(
            "picker = fluentqt.TimePicker()",
            'picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Hour, "hour")',
            'picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Minute, "minute")',
            'picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Period, "AM/PM")',
            'picker.setConfirmButtonAccessibleName("Accept time")',
            'picker.setCancelButtonAccessibleName("Cancel time")',
            "picker.setTime(QTime(9, 30))",
            "picker.setMinuteIncrement(5)",
            imports="from PySide6.QtCore import QTime",
        ),
        "TimePicker",
    )


@native_samples("accordion", "accordion-single-expansion", "accordion-multiple-expansion")
def _accordion(sample_id: str, parent: QWidget | None) -> PreviewResult:
    multiple = sample_id == "accordion-multiple-expansion"
    spacing = 8 if multiple else 12
    root, layout = _column(parent, spacing)
    accordion = fluentqt.Accordion(root)
    accordion.setExpansionMode(
        fluentqt.Accordion.ExpansionMode.Multiple
        if multiple
        else fluentqt.Accordion.ExpansionMode.Single
    )
    accordion.setFixedWidth(520)
    accordion.setObjectName(
        "galleryAccordionMultiple" if multiple else "galleryAccordionSingle"
    )
    if multiple:
        items = (
            _expander_item("Network", "Wi-Fi and Ethernet are connected.", accordion),
            _expander_item("Proxy", "Use system proxy settings.", accordion),
        )
    else:
        items = (
            _expander_item("Account", "Profile, sign-in, and recovery options.", accordion),
            _expander_item("Notifications", "Choose which activity can interrupt you.", accordion),
            _expander_item("Privacy", "Review diagnostics and personalization settings.", accordion),
        )
    for item in items:
        accordion.addOwnedItem(item)
    items[0].setExpandedAnimated(True, False)
    if multiple:
        items[1].setExpandedAnimated(True, False)
    layout.addWidget(accordion)
    mode = "Multiple" if multiple else "Single"
    source_lines = [
        "def make_section(title, detail):",
        "    body = QWidget()",
        "    body_layout = QVBoxLayout(body)",
        "    body_layout.setContentsMargins(16, 12, 16, 14)",
        "    body_layout.setSpacing(4)",
        '    heading = fluentqt.Label("Additional details", body)',
        "    heading.setFluentTypography(fluentqt.FontRole.BodyStrong)",
        "    heading.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
        "    heading.setWordWrap(True)",
        "    body_layout.addWidget(heading)",
        "    detail_label = fluentqt.Label(detail, body)",
        "    detail_label.setFluentTypography(fluentqt.FontRole.Body)",
        "    detail_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
        "    detail_label.setWordWrap(True)",
        "    body_layout.addWidget(detail_label)",
        "    item = fluentqt.Expander()",
        "    item.setHeaderText(title)",
        "    item.setOwnedContentWidget(body)",
        "    return item",
        "",
        "root = QWidget()",
        "layout = QVBoxLayout(root)",
        "layout.setContentsMargins(0, 0, 0, 0)",
        "layout.setSpacing({0})".format(spacing),
        "layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)",
        "accordion = fluentqt.Accordion(root)",
        "accordion.setExpansionMode(fluentqt.Accordion.ExpansionMode.{0})".format(mode),
        "accordion.setFixedWidth(520)",
    ]
    values = (
        (("Network", "Wi-Fi and Ethernet are connected."), ("Proxy", "Use system proxy settings."))
        if multiple
        else (
            ("Account", "Profile, sign-in, and recovery options."),
            ("Notifications", "Choose which activity can interrupt you."),
            ("Privacy", "Review diagnostics and personalization settings."),
        )
    )
    for index, (title, detail) in enumerate(values):
        source_lines.extend(
            (
                'item_{0} = make_section({1!r}, {2!r})'.format(index, title, detail),
                "accordion.addOwnedItem(item_{0})".format(index),
            )
        )
    source_lines.append("item_0.setExpandedAnimated(True, False)")
    if multiple:
        source_lines.append("item_1.setExpandedAnimated(True, False)")
    source_lines.append("layout.addWidget(accordion)")
    _hold(root, accordion, *items)
    return _result(
        root,
        _source(
            *source_lines,
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QVBoxLayout, QWidget",
        ),
        "Accordion",
        "Expander",
    )


def _appearance_card(
    parent: QWidget,
    title: str,
    detail: str,
    appearance: fluentqt.Card.Appearance,
    border_visible: bool = True,
) -> fluentqt.Card:
    card = fluentqt.Card(parent)
    card.setAppearance(appearance)
    card.setBorderVisible(border_visible)
    card.setFixedSize(170, 88)
    layout = QVBoxLayout(card)
    layout.setContentsMargins(16, 12, 16, 12)
    layout.setSpacing(3)
    heading = fluentqt.Label(title, card)
    heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
    heading.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    heading.setWordWrap(True)
    layout.addWidget(heading)
    description = fluentqt.Label(detail, card)
    description.setFluentTypography(fluentqt.FontRole.Caption)
    description.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    description.setWordWrap(True)
    layout.addWidget(description)
    layout.addStretch(1)
    return card


@native_samples("card", "card-surface-appearances", "card-border-visibility")
def _card(sample_id: str, parent: QWidget | None) -> PreviewResult:
    root, layout = _row(parent)
    if sample_id == "card-surface-appearances":
        values = (
            ("Layer", "Default grouped surface", fluentqt.Card.Appearance.Layer, True),
            ("LayerAlt", "Alternate layer tone", fluentqt.Card.Appearance.LayerAlt, True),
            ("Canvas", "Matches the page canvas", fluentqt.Card.Appearance.Canvas, True),
        )
    else:
        values = (
            ("Bordered", "Independent surface", fluentqt.Card.Appearance.Layer, True),
            ("Borderless", "Nested composition", fluentqt.Card.Appearance.Layer, False),
        )
    cards = []
    source_lines = [
        "root = QWidget()",
        "layout = QHBoxLayout(root)",
        "layout.setContentsMargins(0, 0, 0, 0)",
        "layout.setSpacing(12)",
        "layout.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)",
    ]
    for index, (title, detail, appearance, border) in enumerate(values):
        card = _appearance_card(root, title, detail, appearance, border)
        layout.addWidget(card)
        cards.append(card)
        source_lines.extend(
            (
                "card_{0} = fluentqt.Card(root)".format(index),
                "card_{0}.setAppearance(fluentqt.Card.Appearance.{1})".format(
                    index, _enum_name(appearance)
                ),
                "card_{0}.setBorderVisible({1})".format(index, border),
                "card_{0}.setFixedSize(170, 88)".format(index),
                "card_{0}_layout = QVBoxLayout(card_{0})".format(index),
                "card_{0}_layout.setContentsMargins(16, 12, 16, 12)".format(index),
                "card_{0}_layout.setSpacing(3)".format(index),
                "card_{0}_title = fluentqt.Label({1!r}, card_{0})".format(index, title),
                "card_{0}_title.setFluentTypography(fluentqt.FontRole.BodyStrong)".format(index),
                "card_{0}_title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)".format(index),
                "card_{0}_title.setWordWrap(True)".format(index),
                "card_{0}_layout.addWidget(card_{0}_title)".format(index),
                "card_{0}_detail = fluentqt.Label({1!r}, card_{0})".format(index, detail),
                "card_{0}_detail.setFluentTypography(fluentqt.FontRole.Caption)".format(index),
                "card_{0}_detail.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)".format(index),
                "card_{0}_detail.setWordWrap(True)".format(index),
                "card_{0}_layout.addWidget(card_{0}_detail)".format(index),
                "card_{0}_layout.addStretch(1)".format(index),
                "layout.addWidget(card_{0})".format(index),
            )
        )
    layout.addStretch()
    source_lines.append("layout.addStretch()")
    _hold(root, *cards)
    return _result(
        root,
        _source(
            *source_lines,
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget",
        ),
        "Card",
    )


@native_samples("divider", "divider-horizontal-insets", "divider-vertical-orientation")
def _divider(sample_id: str, parent: QWidget | None) -> PreviewResult:
    if sample_id == "divider-horizontal-insets":
        card = fluentqt.Card(parent)
        card.setFixedSize(520, 116)
        layout = QVBoxLayout(card)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(10)
        full = fluentqt.Divider(card)
        inset = fluentqt.Divider(card)
        inset.setLeadingInset(24)
        inset.setTrailingInset(48)
        full_label = fluentqt.Label("Full-width separator", card)
        full_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        full_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        full_label.setWordWrap(True)
        inset_label = fluentqt.Label("Inset separator", card)
        inset_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        inset_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        inset_label.setWordWrap(True)
        layout.addWidget(full_label)
        layout.addWidget(full)
        layout.addWidget(inset_label)
        layout.addWidget(inset)
        return _result(
            card,
            _source(
                "card = fluentqt.Card()",
                "card.setFixedSize(520, 116)",
                "layout = QVBoxLayout(card)",
                "layout.setContentsMargins(16, 14, 16, 14)",
                "layout.setSpacing(10)",
                'full_label = fluentqt.Label("Full-width separator", card)',
                "full_label.setFluentTypography(fluentqt.FontRole.BodyStrong)",
                "full_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
                "full_label.setWordWrap(True)",
                "layout.addWidget(full_label)",
                "full = fluentqt.Divider(card)",
                "layout.addWidget(full)",
                'inset_label = fluentqt.Label("Inset separator", card)',
                "inset_label.setFluentTypography(fluentqt.FontRole.BodyStrong)",
                "inset_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
                "inset_label.setWordWrap(True)",
                "layout.addWidget(inset_label)",
                "inset = fluentqt.Divider(card)",
                "inset.setLeadingInset(24)",
                "inset.setTrailingInset(48)",
                "layout.addWidget(inset)",
                imports="from PySide6.QtWidgets import QVBoxLayout",
            ),
            "Divider",
        )

    card = fluentqt.Card(parent)
    card.setFixedSize(420, 76)
    layout = QHBoxLayout(card)
    layout.setContentsMargins(20, 16, 20, 16)
    layout.setSpacing(16)
    dividers = []
    for index, text in enumerate(("Details", "Activity", "History")):
        label = fluentqt.Label(text, card)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        label.setWordWrap(True)
        layout.addWidget(label)
        if index < 2:
            divider = fluentqt.Divider(Qt.Vertical, card)
            divider.setLeadingInset(4)
            divider.setTrailingInset(4)
            divider.setFixedHeight(44)
            layout.addWidget(divider)
            dividers.append(divider)
    layout.addStretch(1)
    _hold(card, *dividers)
    return _result(
        card,
        _source(
            "card = fluentqt.Card()",
            "card.setFixedSize(420, 76)",
            "layout = QHBoxLayout(card)",
            "layout.setContentsMargins(20, 16, 20, 16)",
            "layout.setSpacing(16)",
            'details = fluentqt.Label("Details", card)',
            "details.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
            "details.setWordWrap(True)",
            "layout.addWidget(details)",
            "first = fluentqt.Divider(Qt.Vertical, card)",
            "first.setLeadingInset(4)",
            "first.setTrailingInset(4)",
            "first.setFixedHeight(44)",
            "layout.addWidget(first)",
            'activity = fluentqt.Label("Activity", card)',
            "activity.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
            "activity.setWordWrap(True)",
            "layout.addWidget(activity)",
            "second = fluentqt.Divider(Qt.Vertical, card)",
            "second.setLeadingInset(4)",
            "second.setTrailingInset(4)",
            "second.setFixedHeight(44)",
            "layout.addWidget(second)",
            'history = fluentqt.Label("History", card)',
            "history.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
            "history.setWordWrap(True)",
            "layout.addWidget(history)",
            "layout.addStretch(1)",
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QHBoxLayout",
        ),
        "Divider",
    )


@native_samples("expander", "expander-text-content", "expander-state-signal")
def _expander(sample_id: str, parent: QWidget | None) -> PreviewResult:
    if sample_id == "expander-text-content":
        root, layout = _column(parent, 12)
        expander = fluentqt.Expander(root)
        expander.setObjectName("galleryExpanderTextContent")
        expander.setFixedWidth(520)
        expander.setHeaderText("Connection details")
        body = QWidget()
        body_layout = QVBoxLayout(body)
        body_layout.setContentsMargins(16, 12, 16, 14)
        body_layout.setSpacing(4)
        heading = fluentqt.Label("Additional details", body)
        heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
        heading.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        heading.setWordWrap(True)
        body_layout.addWidget(heading)
        detail = fluentqt.Label("Server: api.example.com\nTransport: TLS 1.3", body)
        detail.setFluentTypography(fluentqt.FontRole.Body)
        detail.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        detail.setWordWrap(True)
        body_layout.addWidget(detail)
        expander.setOwnedContentWidget(body)
        expander.setExpandedAnimated(True, False)
        layout.addWidget(expander)
        _hold(root, expander, body, heading, detail)
        return _result(
            root,
            _source(
                "root = QWidget()",
                "layout = QVBoxLayout(root)",
                "layout.setContentsMargins(0, 0, 0, 0)",
                "layout.setSpacing(12)",
                "layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)",
                "body = QWidget()",
                "body_layout = QVBoxLayout(body)",
                "body_layout.setContentsMargins(16, 12, 16, 14)",
                "body_layout.setSpacing(4)",
                'heading = fluentqt.Label("Additional details", body)',
                "heading.setFluentTypography(fluentqt.FontRole.BodyStrong)",
                "body_layout.addWidget(heading)",
                'detail = fluentqt.Label("Server: api.example.com\\nTransport: TLS 1.3", body)',
                "detail.setWordWrap(True)",
                "body_layout.addWidget(detail)",
                "expander = fluentqt.Expander(root)",
                'expander.setHeaderText("Connection details")',
                "expander.setFixedWidth(520)",
                "expander.setOwnedContentWidget(body)",
                "expander.setExpandedAnimated(True, False)",
                "layout.addWidget(expander)",
                imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QVBoxLayout, QWidget",
            ),
            "Expander",
        )

    root, layout = _column(parent, 8)
    status = fluentqt.Label("Collapsed", root)
    status.setObjectName("galleryExpanderStateLabel")
    status.setFluentTypography(fluentqt.FontRole.Caption)
    status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    expander = _expander_item(
        "Advanced options", "Diagnostic logging and retry behavior.", root
    )
    expander.setFixedWidth(520)
    expander.expandedChanged.connect(
        lambda expanded: status.setText("Expanded" if expanded else "Collapsed")
    )
    layout.addWidget(expander)
    layout.addWidget(status)
    _hold(root, expander, status)
    return _result(
        root,
        _source(
            "root = QWidget()",
            "layout = QVBoxLayout(root)",
            "layout.setContentsMargins(0, 0, 0, 0)",
            "layout.setSpacing(8)",
            "layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)",
            "body = QWidget()",
            "body_layout = QVBoxLayout(body)",
            'body_layout.addWidget(fluentqt.Label("Additional details", body))',
            'body_layout.addWidget(fluentqt.Label("Diagnostic logging and retry behavior.", body))',
            "expander = fluentqt.Expander(root)",
            'expander.setHeaderText("Advanced options")',
            "expander.setFixedWidth(520)",
            "expander.setOwnedContentWidget(body)",
            'status = fluentqt.Label("Collapsed", root)',
            "status.setFluentTypography(fluentqt.FontRole.Caption)",
            "expander.expandedChanged.connect(",
            '    lambda expanded: status.setText("Expanded" if expanded else "Collapsed")',
            ")",
            "layout.addWidget(expander)",
            "layout.addWidget(status)",
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QVBoxLayout, QWidget",
        ),
        "Expander",
    )


@native_samples("font-icon", "font-icon-optical-sizes", "font-icon-color-rotation")
def _font_icon(sample_id: str, parent: QWidget | None) -> PreviewResult:
    def icon_cell(
        owner: QWidget,
        glyph: str,
        size: int,
        caption: str,
        color: QColor = QColor(),
        rotation: float = 0.0,
    ) -> QWidget:
        cell = QWidget(owner)
        cell_layout = QVBoxLayout(cell)
        cell_layout.setContentsMargins(8, 4, 8, 4)
        cell_layout.setSpacing(8)
        cell_layout.setAlignment(Qt.AlignHCenter | Qt.AlignTop)
        icon = fluentqt.FontIcon(glyph, cell)
        icon.setIconSize(size)
        icon.setColor(color)
        icon.setRotation(rotation)
        icon.setAccessibleName(caption)
        label = fluentqt.Label(caption, cell)
        label.setFluentTypography(fluentqt.FontRole.Caption)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        label.setAlignment(Qt.AlignCenter)
        cell_layout.addWidget(icon, 0, Qt.AlignHCenter)
        cell_layout.addWidget(label)
        _hold(cell, icon, label)
        return cell

    cells = []
    if sample_id == "font-icon-optical-sizes":
        root = fluentqt.Card(parent)
        root.setFixedSize(420, 104)
        layout = QHBoxLayout(root)
        layout.setContentsMargins(18, 14, 18, 14)
        layout.setSpacing(22)
        values = (
            (12, "16 px", QColor(), 0.0),
            (16, "20 px", QColor(), 0.0),
            (24, "24 px", QColor(), 0.0),
            (32, "32 px", QColor(), 0.0),
        )
        glyph = "\ue721"
    else:
        root, layout = _row(parent, 20)
        values = (
            (24, "Inherited", QColor(), 0.0),
            (24, "Accent · 90°", QColor("#0F6CBD"), 90.0),
            (24, "Warning · 180°", QColor("#F7630C"), 180.0),
        )
        glyph = "\ue72a"
    for size, caption, color, rotation in values:
        cell = icon_cell(root, glyph, size, caption, color, rotation)
        layout.addWidget(cell)
        cells.append(cell)
    if sample_id == "font-icon-optical-sizes":
        layout.addStretch(1)
    source_lines = [
        "def make_icon_cell(parent, glyph, size, caption, color=QColor(), rotation=0.0):",
        "    cell = QWidget(parent)",
        "    cell_layout = QVBoxLayout(cell)",
        "    cell_layout.setContentsMargins(8, 4, 8, 4)",
        "    cell_layout.setSpacing(8)",
        "    cell_layout.setAlignment(Qt.AlignHCenter | Qt.AlignTop)",
        "    icon = fluentqt.FontIcon(glyph, cell)",
        "    icon.setIconSize(size)",
        "    icon.setColor(color)",
        "    icon.setRotation(rotation)",
        "    icon.setAccessibleName(caption)",
        "    label = fluentqt.Label(caption, cell)",
        "    label.setFluentTypography(fluentqt.FontRole.Caption)",
        "    label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)",
        "    label.setAlignment(Qt.AlignCenter)",
        "    cell_layout.addWidget(icon, 0, Qt.AlignHCenter)",
        "    cell_layout.addWidget(label)",
        "    return cell",
        "",
        "root = fluentqt.Card()" if sample_id == "font-icon-optical-sizes" else "root = QWidget()",
        "layout = QHBoxLayout(root)",
    ]
    if sample_id == "font-icon-optical-sizes":
        source_lines.extend(
            (
                "root.setFixedSize(420, 104)",
                "layout.setContentsMargins(18, 14, 18, 14)",
                "layout.setSpacing(22)",
            )
        )
    else:
        source_lines.extend(
            (
                "layout.setContentsMargins(0, 0, 0, 0)",
                "layout.setSpacing(20)",
                "layout.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)",
            )
        )
    if sample_id == "font-icon-optical-sizes":
        source_lines.append(
            "values = ((12, '16 px', QColor(), 0.0), "
            "(16, '20 px', QColor(), 0.0), "
            "(24, '24 px', QColor(), 0.0), "
            "(32, '32 px', QColor(), 0.0))"
        )
    else:
        source_lines.append(
            "values = ((24, 'Inherited', QColor(), 0.0), "
            "(24, 'Accent \\u00b7 90\\u00b0', QColor('#0F6CBD'), 90.0), "
            "(24, 'Warning \\u00b7 180\\u00b0', QColor('#F7630C'), 180.0))"
        )
    source_lines.extend(
        (
            "for size, caption, color, rotation in values:",
            "    layout.addWidget(make_icon_cell(root, {0!r}, size, caption, color, rotation))".format(glyph),
        )
    )
    if sample_id == "font-icon-optical-sizes":
        source_lines.append("layout.addStretch(1)")
    _hold(root, *cells)
    return _result(
        root,
        _source(
            *source_lines,
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtGui import QColor\nfrom PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget",
        ),
        "FontIcon",
    )


@native_samples("shimmer", "shimmer-templates", "shimmer-custom-elements", "shimmer-static-phase")
def _shimmer(sample_id: str, parent: QWidget | None) -> PreviewResult:
    # Reuse the same painted sample surface and caption helpers as every other
    # Status & info route.  The executed source is also the displayed source,
    # so the preview cannot silently diverge from the Python example.
    from .native_samples_status import _status_script

    if sample_id == "shimmer-templates":
        body = """
        root, layout = make_status_surface()
        row, row_layout = horizontal_group(root, 24)
        values = (
            (fluentqt.Shimmer.ShimmerTemplate.ImageCard, QSize(160, 92), "ImageCard"),
            (fluentqt.Shimmer.ShimmerTemplate.AvatarTextRow, QSize(180, 56), "AvatarTextRow"),
            (fluentqt.Shimmer.ShimmerTemplate.TextBlock, QSize(220, 72), "TextBlock"),
        )
        for template, size, caption in values:
            shimmer = fluentqt.Shimmer(row)
            shimmer.setShimmerTemplate(template)
            shimmer.setFixedSize(size)
            row_layout.addWidget(labeled_column(row, caption, shimmer))
        layout.addWidget(row)
        """
        extra_imports = "from PySide6.QtCore import QSize"
    elif sample_id == "shimmer-custom-elements":
        body = """
        root, layout = make_status_surface()
        shimmer = fluentqt.Shimmer(root)
        shimmer.setFixedSize(330, 160)
        shimmer.setElements([
            fluentqt.Shimmer.Element(fluentqt.Shimmer.Shape.RoundedRect, QRectF(0, 0, 330, 82), 8.0),
            fluentqt.Shimmer.Element(fluentqt.Shimmer.Shape.Circle, QRectF(0, 104, 36, 36)),
            fluentqt.Shimmer.Element(fluentqt.Shimmer.Shape.Line, QRectF(50, 106, 180, 12)),
            fluentqt.Shimmer.Element(fluentqt.Shimmer.Shape.Line, QRectF(50, 128, 124, 12)),
            fluentqt.Shimmer.Element(fluentqt.Shimmer.Shape.RoundedRect, QRectF(250, 106, 72, 30), 6.0),
        ])
        layout.addWidget(shimmer)
        """
        extra_imports = ""
    else:
        body = """
        root, layout = make_status_surface()
        shimmer = fluentqt.Shimmer(root)
        shimmer.setShimmerTemplate(fluentqt.Shimmer.ShimmerTemplate.TextBlock)
        shimmer.setFixedSize(260, 72)
        shimmer.setAnimationEnabled(False)
        shimmer.setShimmerProgress(0.42)
        status = make_status_label(root, "Progress phase: 0.42")
        layout.addWidget(shimmer)
        layout.addWidget(status)
        """
        extra_imports = ""

    source = _status_script(body, extra_imports)
    namespace: dict[str, object] = {
        "__name__": "fluentqt_gallery_shimmer_{0}".format(
            sample_id.replace("-", "_")
        ),
        "gallery_parent": parent,
    }
    exec(compile(source, "<shimmer/{0}>".format(sample_id), "exec"), namespace)
    root = namespace["root"]
    if not isinstance(root, QWidget):
        raise TypeError("Shimmer Gallery source did not construct a QWidget root")
    root._fluentqt_gallery_source_namespace = namespace
    return _result(root, source, "Shimmer")


def _list_model(view: fluentqt.ListView, values: tuple[str, ...]) -> QStandardItemModel:
    model = QStandardItemModel(view)
    for value in values:
        model.appendRow(QStandardItem(value))
    view.setModel(model)
    return model


_ACCENT_PALETTE = (
    QColor(0x00, 0x78, 0xD4),
    QColor(0x03, 0x83, 0x87),
    QColor(0xCA, 0x50, 0x10),
    QColor(0x87, 0x64, 0xB8),
    QColor(0xC2, 0x39, 0xB3),
    QColor(0x49, 0x82, 0x05),
)


def _initials_avatar(name: str, background: QColor, size: int = 28) -> QPixmap:
    screen = QGuiApplication.primaryScreen()
    dpr = max(1.0, screen.devicePixelRatio() if screen is not None else 1.0)
    physical = max(1, round(size * dpr))
    pixmap = QPixmap(physical, physical)
    pixmap.setDevicePixelRatio(dpr)
    pixmap.fill(Qt.GlobalColor.transparent)
    initials = "".join(word[0].upper() for word in name.split()[:2])
    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
    painter.setPen(Qt.PenStyle.NoPen)
    painter.setBrush(background)
    painter.drawEllipse(QRectF(0, 0, size, size))
    font = QFont()
    font.setPixelSize(round(size * 0.42))
    font.setWeight(QFont.Weight.DemiBold)
    painter.setFont(font)
    painter.setPen(Qt.GlobalColor.white)
    painter.drawText(
        QRectF(0, 0, size, size),
        Qt.AlignmentFlag.AlignCenter,
        initials,
    )
    painter.end()
    return pixmap


def _glyph_pixmap(glyph: str, background: QColor, size: int) -> QPixmap:
    screen = QGuiApplication.primaryScreen()
    dpr = max(1.0, screen.devicePixelRatio() if screen is not None else 1.0)
    physical = max(1, round(size * dpr))
    pixmap = QPixmap(physical, physical)
    pixmap.setDevicePixelRatio(dpr)
    pixmap.fill(Qt.GlobalColor.transparent)
    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
    tile = QRectF(0, 0, size, size)
    painter.setPen(Qt.PenStyle.NoPen)
    painter.setBrush(background)
    painter.drawRoundedRect(tile, size / 4.0, size / 4.0)
    icon_font = QFont("FluentQt Icons")
    icon_font.setPixelSize(round(size * 0.55))
    painter.setFont(icon_font)
    painter.setPen(Qt.GlobalColor.white)
    painter.drawText(tile, Qt.AlignmentFlag.AlignCenter, glyph)
    painter.end()
    return pixmap


class _GalleryListRowDelegate(QStyledItemDelegate):
    """Python port of the C++ Gallery's ListRowDelegate."""

    def __init__(self, view: fluentqt.ListView):
        super().__init__(view)
        self._view = view

    def paint(self, painter, option, index):
        if not index.isValid():
            return
        from .foundation_pages import _theme_tokens

        colors = _theme_tokens()
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
        selected = bool(option.state & QStyle.StateFlag.State_Selected)
        enabled = bool(option.state & QStyle.StateFlag.State_Enabled)
        hovered = bool(option.state & QStyle.StateFlag.State_MouseOver)
        pressed = bool(option.state & QStyle.StateFlag.State_Sunken) and hovered
        background = QColor(Qt.GlobalColor.transparent)
        text_on_accent = False
        language = str(fluentqt.current_design_language())
        dark = fluentqt.current_theme() == fluentqt.Theme.Dark
        if enabled and "Material" in language:
            if selected:
                background = QColor(colors["accentDefault"])
                background.setAlphaF(0.28 if dark else 0.16)
            elif hovered:
                background = QColor(255, 255, 255, 0x14) if dark else QColor(0, 0, 0, 0x14)
        elif enabled and ("Cupertino" in language or "Mac" in language):
            if selected:
                background = colors["accentDefault"]
                text_on_accent = True
            elif hovered:
                background = QColor(255, 255, 255, 0x12) if dark else QColor(0, 0, 0, 0x10)
        elif enabled:
            if pressed:
                background = colors["subtleTertiary"]
            elif selected or hovered:
                background = colors["subtleSecondary"]

        background_rect = QRectF(option.rect).adjusted(2.0, 1.0, -2.0, -1.0)
        if background.alpha() > 0:
            path = QPainterPath()
            path.addRoundedRect(background_rect, 4.0, 4.0)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(background)
            painter.drawPath(path)

        cursor_x = background_rect.left() + 14.0
        extent = option.decorationSize
        if not extent.isValid() or extent.isEmpty():
            extent = self._view.iconSize()
        if not extent.isValid() or extent.isEmpty():
            extent = QSize(24, 24)
        decoration = index.data(Qt.ItemDataRole.DecorationRole)
        if isinstance(decoration, QPixmap) and not decoration.isNull():
            icon_rect = QRect(
                round(cursor_x),
                round(background_rect.center().y() - extent.height() / 2.0),
                extent.width(),
                extent.height(),
            )
            painter.drawPixmap(icon_rect, decoration)
            cursor_x = icon_rect.right() + 12.0

        text_rect = QRectF(
            cursor_x,
            background_rect.top(),
            background_rect.right() - cursor_x - 8.0,
            background_rect.height(),
        )
        if not enabled:
            text_color = colors["textDisabled"]
        elif text_on_accent:
            text_color = colors["textOnAccent"]
        else:
            text_color = colors["textPrimary"]
        painter.setPen(text_color)
        font = QFont(option.font)
        if selected:
            font.setWeight(QFont.Weight.DemiBold)
        painter.setFont(font)
        text = str(index.data(Qt.ItemDataRole.DisplayRole) or "")
        painter.drawText(
            text_rect,
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
            painter.fontMetrics().elidedText(
                text,
                Qt.TextElideMode.ElideRight,
                int(text_rect.width()),
            ),
        )
        painter.restore()

    def sizeHint(self, option, index):
        hint = super().sizeHint(option, index)
        hint.setHeight(max(hint.height(), 36))
        hint.setWidth(hint.width() + 26)
        return hint


@native_samples(
    "list-view",
    "list-view-basic",
    "list-view-multi-select",
    "list-view-horizontal",
    "list-view-reorder",
    "list-view-sections",
    "list-view-scroll-chaining",
    "list-view-placeholder",
)
def _list_view(sample_id: str, parent: QWidget | None) -> PreviewResult:
    view = fluentqt.ListView(parent)
    view.setBackgroundVisible(False)
    view.setBorderVisible(False)
    model = QStandardItemModel(view)
    delegate: _GalleryListRowDelegate | None = None

    def populate_glyph_rows(rows: Sequence[tuple[str, str]], icon_size: int) -> None:
        for index, (text, glyph) in enumerate(rows):
            item = QStandardItem(text)
            item.setEditable(False)
            item.setData(
                _glyph_pixmap(
                    glyph,
                    _ACCENT_PALETTE[index % len(_ACCENT_PALETTE)],
                    icon_size,
                ),
                Qt.ItemDataRole.DecorationRole,
            )
            model.appendRow(item)

    source_values: tuple[str, ...] = ()
    source_glyph_rows: tuple[tuple[str, str], ...] = ()
    source_icon_size = 24
    source_lines = [
        "view = fluentqt.ListView(globals().get('gallery_parent'))",
        "model = QStandardItemModel(view)",
        "view.setBackgroundVisible(False)",
        "view.setBorderVisible(False)",
    ]
    if sample_id == "list-view-placeholder":
        view.setHeaderText("Downloads")
        view.setFooterText("0 items")
        view.setPlaceholderText("No downloads yet")
        view.setFixedSize(340, 178)
        source_lines.extend(
            (
                "view.setHeaderText('Downloads')",
                "view.setFooterText('0 items')",
                "view.setPlaceholderText('No downloads yet')",
                "view.setFixedSize(340, 178)",
            )
        )
    else:
        delegate = _GalleryListRowDelegate(view)
        view.setItemDelegate(delegate)
        source_lines.extend(
            (
                "delegate = GalleryListRowDelegate(view)",
                "view.setItemDelegate(delegate)",
            )
        )
        if sample_id == "list-view-basic":
            source_values = (
                "Kendall Collins",
                "Henry Ross",
                "Nicole Wagner",
                "Adam Wolfe",
                "Stephanie Meyer",
                "Maya Patel",
                "Alex Chen",
                "Priya Shah",
                "Omar Rivera",
                "Elena Rossi",
                "Jordan Lee",
                "Riley Brooks",
            )
            for index, value in enumerate(source_values):
                item = QStandardItem(value)
                item.setEditable(False)
                item.setData(
                    _initials_avatar(
                        value,
                        _ACCENT_PALETTE[index % len(_ACCENT_PALETTE)],
                    ),
                    Qt.ItemDataRole.DecorationRole,
                )
                model.appendRow(item)
            view.setHeaderText("Contacts")
            view.setIconSize(QSize(28, 28))
            view.setFixedSize(320, 240)
            source_lines.extend(
                (
                    "view.setHeaderText('Contacts')",
                    "view.setIconSize(QSize(28, 28))",
                    "view.setFixedSize(320, 240)",
                )
            )
        elif sample_id == "list-view-multi-select":
            rows = (
                ("Unread", "\ue715"),
                ("Flagged", "\ue7c1"),
                ("Has photos", "\ue722"),
                ("From contacts", "\ue716"),
                ("Favorites", "\ue734"),
                ("With documents", "\ue8a5"),
                ("Pinned", "\ue718"),
                ("Scheduled", "\ue787"),
                ("Archived", "\ue838"),
            )
            source_glyph_rows = rows
            source_icon_size = 24
            source_values = tuple(text for text, _glyph in rows)
            populate_glyph_rows(rows, 24)
            view.setFixedSize(320, 244)
            view.setHeaderText("Filters")
            view.setIconSize(QSize(24, 24))
            view.setSelectionMode(fluentqt.SelectionMode.Multiple)
            source_lines.extend(
                (
                    "view.setFixedSize(320, 244)",
                    "view.setHeaderText('Filters')",
                    "view.setIconSize(QSize(24, 24))",
                    "view.setSelectionMode(fluentqt.SelectionMode.Multiple)",
                )
            )
        elif sample_id == "list-view-horizontal":
            rows = (
                ("Home", "\ue80f"),
                ("Music", "\ue8d6"),
                ("Videos", "\ue714"),
                ("Photos", "\ue722"),
                ("Calendar", "\ue787"),
                ("Settings", "\ue713"),
            )
            source_glyph_rows = rows
            source_icon_size = 26
            source_values = tuple(text for text, _glyph in rows)
            populate_glyph_rows(rows, 26)
            view.setFixedSize(540, 132)
            view.setHeaderText("Library")
            view.setFlow(QListView.Flow.LeftToRight)
            view.setIconSize(QSize(26, 26))
            source_lines.extend(
                (
                    "view.setFixedSize(540, 132)",
                    "view.setHeaderText('Library')",
                    "view.setFlow(QListView.Flow.LeftToRight)",
                    "view.setIconSize(QSize(26, 26))",
                )
            )
        elif sample_id == "list-view-reorder":
            source_values = (
                "Bloom",
                "Northern Lights",
                "Driftwood",
                "Paper Boats",
                "Blue Hour",
                "Signal Fire",
                "Slow Orbit",
                "City Lights",
                "Afterglow",
                "Quiet Roads",
            )
            source_glyph_rows = tuple(
                (text, "\ue8d6") for text in source_values
            )
            source_icon_size = 24
            populate_glyph_rows(source_glyph_rows, source_icon_size)
            view.setFixedSize(320, 220)
            view.setHeaderText("Playlist")
            view.setIconSize(QSize(24, 24))
            view.setCanReorderItems(True)
            source_lines.extend(
                (
                    "view.setFixedSize(320, 220)",
                    "view.setHeaderText('Playlist')",
                    "view.setIconSize(QSize(24, 24))",
                    "view.setCanReorderItems(True)",
                )
            )
        elif sample_id == "list-view-sections":
            rows = (
                ("Build completed", "\ue73e"),
                ("New comment", "\ue8bd"),
                ("Meeting starts soon", "\ue787"),
                ("Pull request updated", "\ue8a5"),
                ("File synced", "\ue895"),
                ("Download ready", "\ue896"),
                ("Reminder", "\ue917"),
                ("Settings changed", "\ue713"),
            )
            source_glyph_rows = rows
            source_icon_size = 24
            source_values = tuple(text for text, _glyph in rows)
            populate_glyph_rows(rows, 24)
            view.setFixedSize(340, 252)
            view.setHeaderText("Notifications")
            view.setIconSize(QSize(24, 24))
            view.setSectionKeyFunction(
                lambda row: "Today" if row < 3 else ("Yesterday" if row < 6 else "Earlier")
            )
            view.setSectionEnabled(True)
            source_lines.extend(
                (
                    "view.setSectionKeyFunction(",
                    "    lambda row: 'Today' if row < 3 else ('Yesterday' if row < 6 else 'Earlier')",
                    ")",
                    "view.setSectionEnabled(True)",
                    "view.setFixedSize(340, 252)",
                    "view.setHeaderText('Notifications')",
                    "view.setIconSize(QSize(24, 24))",
                )
            )
        elif sample_id == "list-view-scroll-chaining":
            rows = tuple(
                (
                    "Queued item {0}".format(index + 1),
                    "\ue8a5" if index % 2 == 0 else "\ue896",
                )
                for index in range(20)
            )
            source_glyph_rows = rows
            source_icon_size = 24
            source_values = tuple(text for text, _glyph in rows)
            populate_glyph_rows(rows, 24)
            view.setFixedSize(340, 238)
            view.setScrollChainingEnabled(False)
            view.setHeaderText("Queue")
            view.setFooterText("Wheel input stays in this ListView")
            view.setIconSize(QSize(24, 24))
            source_lines.extend(
                (
                    "view.setFixedSize(340, 238)",
                    "view.setScrollChainingEnabled(False)",
                    "view.setHeaderText('Queue')",
                    "view.setFooterText('Wheel input stays in this ListView')",
                    "view.setIconSize(QSize(24, 24))",
                )
            )

    if sample_id == "list-view-basic":
        source_lines.extend(
            (
                "for index, value in enumerate({0!r}):".format(source_values),
                "    item = QStandardItem(value)",
                "    item.setEditable(False)",
                "    item.setData(",
                "        gallery_initials_avatar(",
                "            value, GALLERY_ACCENT_PALETTE[index % len(GALLERY_ACCENT_PALETTE)]",
                "        ),",
                "        Qt.ItemDataRole.DecorationRole,",
                "    )",
                "    model.appendRow(item)",
            )
        )
    elif source_glyph_rows:
        source_lines.extend(
            (
                "for index, (text, glyph) in enumerate({0!r}):".format(
                    source_glyph_rows
                ),
                "    item = QStandardItem(text)",
                "    item.setEditable(False)",
                "    item.setData(",
                "        gallery_glyph_pixmap(",
                "            glyph,",
                "            GALLERY_ACCENT_PALETTE[index % len(GALLERY_ACCENT_PALETTE)],",
                "            {0},".format(source_icon_size),
                "        ),",
                "        Qt.ItemDataRole.DecorationRole,",
                "    )",
                "    model.appendRow(item)",
            )
        )
    view.setModel(model)
    source_lines.append("view.setModel(model)")
    if sample_id == "list-view-basic":
        view.setSelectedIndex(0)
        source_lines.append("view.setSelectedIndex(0)")
    elif sample_id == "list-view-multi-select":
        selection = view.selectionModel()
        for row in (0, 2):
            selection.select(
                model.index(row, 0),
                QItemSelectionModel.SelectionFlag.Select
                | QItemSelectionModel.SelectionFlag.Rows,
            )
        source_lines.extend(
            (
                "selection = view.selectionModel()",
                "for row in (0, 2):",
                "    selection.select(",
                "        model.index(row, 0),",
                "        QItemSelectionModel.SelectionFlag.Select",
                "        | QItemSelectionModel.SelectionFlag.Rows,",
                "    )",
            )
        )
    elif sample_id == "list-view-horizontal":
        view.setSelectedIndex(0)
        source_lines.append("view.setSelectedIndex(0)")
    elif sample_id == "list-view-sections":
        view.setSelectedIndex(1)
        source_lines.append("view.setSelectedIndex(1)")
    elif sample_id == "list-view-scroll-chaining":
        view.setSelectedIndex(0)
        source_lines.append("view.setSelectedIndex(0)")
    _hold(view, model, delegate)
    return _result(
        view,
        _source(
            *source_lines,
            imports=(
                "from PySide6.QtCore import QItemSelectionModel, QSize, Qt\n"
                "from PySide6.QtGui import QStandardItem, QStandardItemModel\n"
                "from PySide6.QtWidgets import QListView\n"
                "# Gallery-only helpers reproduce the native card's avatars "
                "and row painting; ListView itself is the public binding API.\n"
                "from fluentqt.gallery.native_samples import (\n"
                "    _ACCENT_PALETTE as GALLERY_ACCENT_PALETTE,\n"
                "    _GalleryListRowDelegate as GalleryListRowDelegate,\n"
                "    _glyph_pixmap as gallery_glyph_pixmap,\n"
                "    _initials_avatar as gallery_initials_avatar,\n"
                ")"
            ),
        ),
        "ListView",
    )


def build_native_sample(
    route_id: str,
    sample_id: str,
    parent: QWidget | None,
) -> PreviewResult | None:
    builder = _BUILDERS.get((route_id, sample_id))
    if builder is None:
        return None
    result = builder(sample_id, parent)
    if not result.source_driven:
        result = _materialize_displayed_source(
            route_id,
            sample_id,
            result,
            parent,
        )
    result.route_id = route_id
    result.sample_id = sample_id
    result.parity_level = "native-equivalent"
    return result


def ported_sample_keys() -> frozenset[tuple[str, str]]:
    return frozenset(_BUILDERS)


# Registration modules are imported only after the registry API is complete.
from . import native_samples_basic as _native_samples_basic  # noqa: E402,F401
from . import native_samples_collections as _native_samples_collections  # noqa: E402,F401
from . import native_samples_dialogs as _native_samples_dialogs  # noqa: E402,F401
from . import native_samples_navigation as _native_samples_navigation  # noqa: E402,F401
from . import native_samples_scrolling as _native_samples_scrolling  # noqa: E402,F401
from . import native_samples_status as _native_samples_status  # noqa: E402,F401
from . import native_samples_text_window as _native_samples_text_window  # noqa: E402,F401


__all__ = ["build_native_sample", "ported_sample_keys", "register_source_samples"]

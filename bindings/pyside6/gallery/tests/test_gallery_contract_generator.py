"""Verify that the packaged Gallery contract is a live native-source derivation."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from tempfile import TemporaryDirectory


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--contract", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if not args.self_test and args.contract is None:
        parser.error("--contract is required unless --self-test is selected")
    return args


def verify_qml_source_parser() -> None:
    from generate_gallery_contract import (
        QML_VIEW_SOURCES,
        _cpp_string_expression,
        _function_bodies,
        _qml_bridge_source_expressions,
        _qml_bridge_source_files,
        _samples_in_body,
    )

    # Quoted braces and commas inside raw code belong to the exported file.
    cpp = '// Focused example\nconst auto value = QStringLiteral("},) and \\"quoted\\"");\n'
    raw = 'QString::fromUtf8(u8R"CPP(' + cpp + ')CPP")'
    if _cpp_string_expression(raw + ' + "\\nfinished"') != cpp + "\nfinished":
        raise AssertionError("C++ raw strings must retain the teaching code verbatim")
    template = (
        '// main.cpp\nvoid build() {\n@BODY@\n}\n'
        'const auto name = "qrc:/@NAME@";\n'
        'int main() { build(); }\n'
    )
    replacements = (
        '.replace(QStringLiteral("@NAME@"), name)'
        '.replace(QStringLiteral("@BODY@"), cpp)'
    )
    resource_template = '<RCC><qresource prefix="/"><file>@NAME@</file></qresource></RCC>\n'
    cmake_template = 'add_executable(qml_view_example main.cpp qml.qrc)\n'

    def helper(contents=template, chain=replacements):
        return (
            'QString withQml(const QString& cpp, const QString& name) {\n'
            'const auto example = QString::fromUtf8(R"example(' + contents + ')example");\n'
            'return QString(example)' + chain + ';\n}\n'
        )

    resources = (
        'QString qmlResourceSource(const QString& name) {\n'
        'const auto resource = QString::fromUtf8(R"resource(' + resource_template + ')resource");\n'
        'return QString(resource).replace(QStringLiteral("@NAME@"), name);\n}\n'
        'QString qmlCmakeSource() { return QString::fromUtf8(R"cmake(' + cmake_template + ')cmake"); }\n'
    )
    filenames = (
        'QString qmlFileName(const QString& id) {'
        + ''.join('if (id == QStringLiteral("' + sample_id + '")) '
                  'return QStringLiteral("' + asset + '");'
                  for sample_id, _, asset in QML_VIEW_SOURCES if asset != "Basic.qml")
        + 'return QStringLiteral("Basic.qml");}\n'
    )
    keys = (
        'QString keyUsage(const QString& id) {'
        + ''.join('if (id == QStringLiteral("' + sample_id + '")) { return ' + raw + '; }\n'
                  for sample_id, _, _ in QML_VIEW_SOURCES)
        + 'return {};}\n'
    )
    declarations = ',\n'.join(
        'samples::makeSample(QStringLiteral("' + sample_id + '"), QStringLiteral("Title"), '
        'QStringLiteral("Description"), ' + name + '(), [](QWidget*) { return nullptr; }, true)'
        for sample_id, name, _ in QML_VIEW_SOURCES
    )
    assignment = (
        'for (auto& sample : result) { const auto name = qmlFileName(sample.id);'
        'sample.usageSnippet = keyUsage(sample.id); sample.sourceFiles = {'
        '{QStringLiteral("main.cpp"), QStringLiteral("cpp"), sample.codeSnippet, sample.usageSnippet},'
        '{name, QStringLiteral("qml"), qmlAsset(name), {}},'
        '{QStringLiteral("qml.qrc"), QStringLiteral("xml"), qmlResourceSource(name), {}},'
        '{QStringLiteral("CMakeLists.txt"), QStringLiteral("cmake"), qmlCmakeSource(), {}}};}'
        'return result;'
    )
    sample_function = ('QVector<GallerySample> qmlViewSamples() {'
                       'QVector<GallerySample> result{' + declarations + '};' + assignment + '}')

    def rejects(source, samples_root):
        try:
            _qml_bridge_source_files(source, samples_root)
        except ValueError:
            pass
        else:
            raise AssertionError("Malformed QML source/template/file bundle must be rejected")

    with TemporaryDirectory(prefix="fluentqt-qml-contract-") as temporary:
        samples_root = Path(temporary)
        qml_root = samples_root / "qmlbridge"
        qml_root.mkdir()
        qml_files = {}
        for _, _, asset in QML_VIEW_SOURCES:
            qml_files[asset] = 'import QtQuick 2.15\r\nItem { objectName: "' + asset + '" }\r\n'
            (qml_root / asset).write_bytes(qml_files[asset].encode("utf-8"))
        functions = "\n".join(
            'QString ' + name + '() { return withQml(' + raw + ', QStringLiteral("' + asset + '")); }'
            for _, name, asset in QML_VIEW_SOURCES
        )
        suffix = resources + filenames + keys + functions + sample_function
        source = helper() + suffix
        expressions = _qml_bridge_source_expressions(source, samples_root)
        files = _qml_bridge_source_files(source, samples_root)
        for sample_id, name, asset in QML_VIEW_SOURCES:
            expected = template.replace("@NAME@", asset).replace("@BODY@", cpp)
            if expressions[name + "()"] != expected:
                raise AssertionError("The C++ host file must match its native template: " + name)
            expected_files = [
                {"name": "main.cpp", "language": "cpp", "code": expected, "usage_snippet": cpp},
                {"name": asset, "language": "qml", "code": qml_files[asset], "usage_snippet": ""},
                {"name": "qml.qrc", "language": "xml",
                 "code": resource_template.replace("@NAME@", asset), "usage_snippet": ""},
                {"name": "CMakeLists.txt", "language": "cmake", "code": cmake_template,
                 "usage_snippet": ""},
            ]
            if files[sample_id] != expected_files:
                raise AssertionError("Each file must retain its language and native bytes: " + sample_id)
        parsed = _samples_in_body(declarations, source_expressions=expressions, source_files=files)
        if [sample["id"] for sample in parsed] != [sample_id for sample_id, _, _ in QML_VIEW_SOURCES]:
            raise AssertionError("The six source expressions must map to their own cards")
        if any(sample["cpp_snippet"] != expressions[name + "()"] or
               sample["source_files"] != files[sample_id]
               for sample, (sample_id, name, _) in zip(parsed, QML_VIEW_SOURCES)):
            raise AssertionError("A focused card received another card's source files")
        body = 'QVector<GallerySample> exampleSamples() {' + declarations + '\n' + raw + ';\n}'
        if "exampleSamples" not in _function_bodies(body):
            raise AssertionError("Raw teaching code broke the sample-function scanner")
        for invalid in (
            functions.replace('QStringLiteral("Theme.qml")', 'QStringLiteral("../Theme.qml")'),
            functions.replace("QString actionSource()", "QString renamedSource()"),
            functions.replace('QStringLiteral("Theme.qml")', 'QStringLiteral("Theme.qml") + dynamicName'),
            functions.replace(')CPP")', ')CPP") + dynamicCpp'),
            functions.replace("{ return withQml", "{ if (condition) return withQml"),
            functions.replace('u8R"CPP(', 'LR"CPP('),
            functions.replace('Focused example', '@QML@'),
        ):
            rejects(helper() + resources + filenames + keys + invalid + sample_function, samples_root)
        for invalid_helper in (
            "", helper() + helper(),
            helper().replace("const QString& cpp", "const QString& code"),
            helper().replace('const auto example', 'const auto qml = qmlAsset(name); const auto example'),
            helper().replace(')example");', ')example") + suffix;'),
            helper().replace('R"example(', 'u8R"example('),
            helper().replace(')example");', ')example"); QString another = example;'),
            helper(chain=replacements.replace(
                '.replace(QStringLiteral("@NAME@"), name)',
                '.replace(QStringLiteral("@NAME@"), cpp)')),
            helper(chain=(
                '.replace(QStringLiteral("@BODY@"), cpp)'
                '.replace(QStringLiteral("@NAME@"), name)')),
            helper(chain=replacements + '.replace(QStringLiteral("x"), name)'),
            helper(template + "@NAME@"), helper(template.replace("@BODY@", "")),
            helper(template + "@QML@"), helper(template + "@OTHER@"),
            helper(template + 'const auto qml = R"qml(Item {})qml";'),
        ):
            rejects(invalid_helper + suffix, samples_root)
        for invalid in (
            source.replace('QString qmlResourceSource', 'QString renamedResourceSource'),
            source.replace('QStringLiteral("@NAME@"), name);', 'QStringLiteral("@NAME@"), other);'),
            source.replace('<file>@NAME@</file>', '<file>@NAME@</file><file>@NAME@</file>'),
            source.replace(')resource");', ')resource") + suffix;'),
            source.replace(')cmake");', ')cmake") + suffix;'),
            source.replace('R"cmake(', 'R"other('),
            source.replace('QString keyUsage', 'QString renamedKeyUsage'),
            source.replace('if (id == QStringLiteral("qml-view-action")) {',
                           'if (id == QStringLiteral("unknown-card")) {'),
            source.replace('return {};}\n', 'return QStringLiteral("fallback");}\n'),
            source.replace('qmlAsset(name), {}', 'qmlAsset(sample.id), {}'),
            source.replace('QStringLiteral("qml.qrc"), QStringLiteral("xml")',
                           'QStringLiteral("qml.qrc"), QStringLiteral("cpp")'),
            source.replace('qmlFileName(sample.id)', 'qmlFileName(sample.title)'),
            source.replace('return QStringLiteral("Theme.qml");', 'return QStringLiteral("Basic.qml");'),
        ):
            rejects(invalid, samples_root)
        # QML is now a separate file: C++ raw delimiters and template-looking
        # data must remain safe, verbatim contents rather than corrupt the host.
        for separate_qml in ("Item { property string value: ')qml\"' }", 'Item { property string value: "@BODY@" }'):
            (qml_root / "Basic.qml").write_bytes(separate_qml.encode("utf-8"))
            if _qml_bridge_source_files(source, samples_root)["qml-view-load"][1]["code"] != separate_qml:
                raise AssertionError("QML file data must not be interpolated into C++")
        (qml_root / "Basic.qml").write_bytes(qml_files["Basic.qml"].encode("utf-8"))
        (qml_root / "Theme.qml").write_bytes(b"\xff")
        rejects(source, samples_root)
        (qml_root / "Theme.qml").unlink()
        rejects(source, samples_root)


def main() -> int:
    args = parse_args()
    project_root = args.project_root.resolve()
    tools_dir = Path(__file__).resolve().parents[1] / "tools"
    sys.path.insert(0, str(tools_dir))
    from generate_gallery_contract import (
        QML_VIEW_SOURCES,
        _category_metadata,
        _route_sample_functions,
        generate_contract,
    )

    verify_qml_source_parser()
    if args.self_test:
        print("Verified focused QmlBridge C++/QML source parsing")
        return 0

    qualified_dispatch = _route_sample_functions(
        'if (routeId == QStringLiteral("tree-view"))\n'
        '    return detail::treeViewSamples();'
    )
    if qualified_dispatch != {"tree-view": "treeViewSamples"}:
        raise AssertionError(
            "qualified split-sample dispatch is not recognized: {0!r}".format(
                qualified_dispatch
            )
        )

    metadata = _category_metadata('''
        static const QVector<GalleryComponentCategory> catalog{
            {QStringLiteral("basic"), QStringLiteral("Basic"), QStringLiteral("basic"),
             Typography::Icons::Grid,
             {{QStringLiteral("button"), QStringLiteral("Button"), Typography::Icons::Add}}},
            {QStringLiteral("extension"), QStringLiteral("Extension"), QStringLiteral("extension"),
             Typography::Icons::Grid,
             {{QStringLiteral("extension-view"), QStringLiteral("ExtensionView"), Typography::Icons::Add}}, true}
        };
    ''')
    if list(metadata) != ["basic", "extension"] or metadata["basic"]["plugin"] \
            or not metadata["extension"]["plugin"]:
        raise AssertionError("native category order and plugin bool must drive the contract")

    generated = generate_contract(project_root)
    for route_id, sample_id, filename in (
        ("tag-input", "tag-input-project-members", "MemberSelectionSample.h"),
        ("auto-suggest-box", "auto-suggest-box-async", "AsyncSearchSample.h"),
    ):
        component = next(c for c in generated["components"] if c["id"] == route_id)
        sample = next(s for s in component["samples"] if s["id"] == sample_id)
        expected = (project_root / "app/view/widgets/samples" / filename).read_text(encoding="utf-8")
        if sample["cpp_snippet"] != expected:
            raise AssertionError("Function-local resource source crossed sample boundaries: " + sample_id)
    packaged = json.loads(args.contract.resolve().read_text(encoding="utf-8"))
    if generated != packaged:
        raise AssertionError(
            "packaged Python Gallery contract differs from current native C++ sources"
        )
    base_contract = generate_contract(project_root, include_cpp_only=False)
    cpp_only_ids = {"spatial-view", "spatial-item", "qml-view"}
    if cpp_only_ids & {c["id"] for c in base_contract["components"]}:
        raise AssertionError("The base Python Gallery must not require optional renderer bindings")
    if any(route["id"] in ("plugins", "spatial", "spatial-view", "spatial-item", "qml-bridge", "qml-view")
           for route in base_contract["routes"]):
        raise AssertionError("The base Gallery must omit the empty Plugins subtree")
    cpp_spatial = [c for c in generated["components"] if c["category_id"] == "spatial"]
    if len(cpp_spatial) != 2 or any(c["category_id"] != "spatial" for c in cpp_spatial):
        raise AssertionError("native Spatial components must share their plugin module")
    if sum(len(c["samples"]) for c in cpp_spatial) != 11:
        raise AssertionError("Spatial composition samples are missing")
    summary = generated["summary"]
    if summary != {
        "route_count": 121,
        "component_count": 96,
        "sample_count": 277,
    }:
        raise AssertionError("unexpected Gallery contract summary: {0!r}".format(summary))
    routes = {route["id"]: route for route in generated["routes"]}
    foundation = generated["foundation_samples"]["foundation-qmlplus"]
    if len(foundation) != 12 or [sample["id"] for sample in foundation] != [
        "qmlplus-binding-one-way", "qmlplus-binding-two-way", "qmlplus-binding-computed",
        "qmlplus-binding-pause", "qmlplus-state-manual", "qmlplus-state-conditional",
        "qmlplus-state-inheritance", "qmlplus-state-binding-restore", "qmlplus-behavior",
        "qmlplus-transition", "qmlplus-anchors-center", "qmlplus-anchors-state",
    ]:
        raise AssertionError("QMLPlus must export the twelve canonical atomic examples in order")
    for route_id, parent, depth, group in (
        ("plugins", "", 0, "Plugins"), ("spatial", "plugins", 1, "Plugins"),
        ("spatial-view", "spatial", 2, "Spatial"), ("spatial-item", "spatial", 2, "Spatial"),
        ("qml-bridge", "plugins", 1, "Plugins"), ("qml-view", "qml-bridge", 2, "QmlBridge"),
    ):
        route = routes[route_id]
        if (route["parent_id"], route["depth"], route["group"]) != (parent, depth, group):
            raise AssertionError("incorrect plugin navigation hierarchy: " + route_id)
    route_ids = [route["id"] for route in generated["routes"]]
    start = route_ids.index("plugins")
    if route_ids[start:start + 7] != ["plugins", "spatial", "spatial-view", "spatial-item", "qml-bridge", "qml-view", "all-controls"]:
        raise AssertionError("Plugins must precede Controls and keep its subtree together")
    native_categories = _category_metadata(
        (project_root / "app/model/GalleryComponentCatalog.cpp").read_text(encoding="utf-8")
    )
    if [category["id"] for category in generated["categories"]] != list(native_categories):
        raise AssertionError("Gallery category order must come from native metadata")
    if [category["id"] for category in generated["categories"] if category["plugin"]] != ["spatial", "qml-bridge"]:
        raise AssertionError("Optional renderers must be classified as plugin modules")
    qml_view = next(c for c in generated["components"] if c["id"] == "qml-view")
    if [s["id"] for s in qml_view["samples"]] != [sample_id for sample_id, _, _ in QML_VIEW_SOURCES]:
        raise AssertionError("QmlBridge must expose the six focused examples in order")
    for sample, (_, _, asset) in zip(qml_view["samples"], QML_VIEW_SOURCES):
        qml = (project_root / "app/view/widgets/samples/qmlbridge" / asset).read_bytes().decode("utf-8")
        files = sample["source_files"]
        if [file["name"] for file in files] != ["main.cpp", asset, "qml.qrc", "CMakeLists.txt"]:
            raise AssertionError("QmlBridge full examples must expose four named source files")
        if [file["language"] for file in files] != ["cpp", "qml", "xml", "cmake"]:
            raise AssertionError("Each source file must keep its own language")
        if files[0]["code"] != sample["cpp_snippet"] or files[1]["code"] != qml:
            raise AssertionError("QmlBridge host and QML file must match their native source bytes")
        if not files[0]["usage_snippet"] or any(file["usage_snippet"] for file in files[1:]):
            raise AssertionError("Only the host file has a Key usage excerpt")
        if "import QtQuick" in files[0]["code"] or "QmlBridgePanel" in files[0]["code"]:
            raise AssertionError("C++ must remain host source for only this card's capability")
        if "qrc:/" + asset not in files[0]["code"]:
            raise AssertionError("Host code must load its own supplied QML resource")
    print(
        "Verified Gallery contract: {route_count} routes, "
        "{component_count} components, {sample_count} samples".format(**summary)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

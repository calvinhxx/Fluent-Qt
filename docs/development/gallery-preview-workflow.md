# Gallery preview workflow

Use the Python Live Scene while adjusting a focused Gallery surface. It keeps
one process and window alive and replaces only the scene widget after a
successful save. Use Native Verify when the result has been moved into C++ and
must be checked against the compiled Gallery sample.

Catalog-backed `--route` / `--sample` commands require a built PySide6 Gallery
contract. Configure the selected bindings build with
`FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON`; see the
[PySide6 Gallery build guide](../../bindings/pyside6/gallery/README.md).

## Start from any catalog sample

List the available route and sample ids:

```bash
python3 tools/dev/fluent_qt_live_preview.py --list-routes
python3 tools/dev/fluent_qt_live_preview.py --list-samples --route button
```

Create an editable scene from a real catalog sample:

```bash
python3 tools/dev/fluent_qt_live_preview.py \
  --route button \
  --sample button-styles \
  --fork-scene \
  --theme dark
```

The launcher prints the generated `.preview.py` path. Edit that file and save;
the open window updates without restarting. A fork is never overwritten.
Without `--fork-scene`, the launcher creates a disposable managed scene for
inspection and regenerates it on the next launch.

On each save the host:

1. builds the new scene under a hidden parent;
2. restores interaction changes for controls with stable `objectName` values;
3. swaps the scene widget only after construction succeeds;
4. keeps the last usable scene visible if the new file fails.

The Reload button handles editors that do not emit a normal save event. Light
and Dark can be switched in the same window.

## Scene contract

A scene is a trusted local Python file that exports
`build(parent) -> QWidget`. `SCENE_TITLE` is optional.

```python
import fluentqt
from PySide6.QtWidgets import QHBoxLayout, QWidget

SCENE_TITLE = "Button styles"


def build(parent):
    root = QWidget(parent)
    layout = QHBoxLayout(root)
    button = fluentqt.Button("Save", root)
    button.setObjectName("saveAction")
    button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    layout.addWidget(button)
    return root
```

Open a hand-written scene with:

```bash
python3 tools/dev/fluent_qt_live_preview.py --scene path/to/example.preview.py
```

Scenes execute with the launcher's filesystem and network permissions. This is
a repository development tool, not a sandbox.

## Capture evidence

Both preview layers accept `--snapshot`, `--report`, `--theme`, `--size`, and
`--rtl`. A Live report records reload status and the FluentQt Inspector result:

```bash
QT_QPA_PLATFORM=offscreen \
python3 tools/dev/fluent_qt_live_preview.py \
  --route button --sample button-styles \
  --theme light --size 920x680 \
  --snapshot build/preview/live.png \
  --report build/preview/live.json
```

Inspector findings are review prompts. They are not automatically treated as a
CI failure.

## Verify the compiled C++ sample

After applying the accepted layout and properties to C++, render the real
SampleCard in an isolated native host:

```bash
python3 tools/dev/fluent_qt_preview.py \
  --route button \
  --sample button-styles \
  --theme dark \
  --size 920x680
```

The wrapper builds `fluent_qt_gallery` in parallel unless `--no-build` is
passed. Normal Gallery startup is unchanged when `--preview` is absent.

For a side-by-side review of an edited fork and the compiled sample:

```bash
python3 tools/dev/fluent_qt_compare.py \
  --route button \
  --sample button-styles \
  --scene /path/printed/by-live-scene.preview.py \
  --theme dark \
  --size 920x680
```

The command writes `comparison.html`, both PNGs, and a small JSON manifest under
`build/preview/compare/`. It checks that both runs used the same selection,
theme, direction, and size. `ready-for-review` means those capture conditions
match; it is not a visual verdict or pixel-equality gate.

## Boundary

The Python scene is an authoring aid, not a second production implementation.
C++ components and Gallery samples remain canonical. After Native Verify, run
the component's focused unit, accessibility, geometry, and VisualCheck gates as
usual. Full CTest, packaging, WebAssembly, and cross-platform checks remain at
the normal CI boundary.

The workflow deliberately does not reload native code in process. FluentQt and
the Gallery sample factories are linked into the executable, so safe arbitrary
plugin unloading would add lifetime and ABI complexity without improving this
authoring loop.

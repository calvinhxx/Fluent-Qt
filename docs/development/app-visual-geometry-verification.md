# App Visual Geometry Verification

Use this workflow for application-level UI under `app/`, especially gallery
shell views where alignment bugs are faster to diagnose from widget geometry
than from screenshots.

## Scope

- Applies to app-owned UI and tests, such as `app/view/*` and `tests/gallery/*`.
- Does not apply as a blanket rule to stable reusable components under
  `src/components/`.
- Component tests should keep their existing API, behavior, and VisualCheck
  contracts unless a specific component visual bug needs geometry evidence.

## What to Verify

Use geometry assertions for deterministic layout contracts:

- stable app-owned widget discovery by object name
- vertical or horizontal center alignment
- fixed logical sizes
- edge offsets and spacing
- containment within a parent visual region
- logical icon dimensions, separate from device-pixel backing size

Keep manual VisualCheck and screenshots for subjective polish: color,
typography, material, animation, perceived balance, and brand fidelity.

## Object Names

Give visually important app-owned widgets scoped object names before testing
their geometry:

```cpp
backButton->setObjectName(QStringLiteral("GalleryTitleBar.BackButton"));
searchBox->setObjectName(QStringLiteral("GalleryTitleBar.SearchBox"));
```

Do not rename component-owned internals just to satisfy an app geometry test.
When an app uses a component, prefer asserting the public component widget's
geometry from the app layer.

## Test Helpers

Use `tests/support/VisualGeometryTestUtils.h` from app/gallery tests for:

- required named-widget lookup with readable failure output
- center, size, spacing, containment, and rectangle assertions
- optional text geometry dumps

Example:

```cpp
auto* searchBox = vg::findRequiredChild<AutoSuggestBox>(
    content, QStringLiteral("GalleryTitleBar.SearchBox"));
EXPECT_TRUE(vg::centerYWithin(searchBox, content, 1));
EXPECT_TRUE(vg::sizeIs(searchBox, QSize(360, 28)));
```

## Geometry Dump

Normal CTest runs must stay quiet. Enable dumps only while investigating app
layout issues:

```bash
FLUENT_QT_GEOMETRY_DUMP=1 ctest --preset vcpkg-osx -L '^test_gallery_shell_framework$' --output-on-failure
```

Use the dump output before taking screenshots when the bug is about measurable
geometry. Use screenshots after geometry checks when reviewing visual polish.

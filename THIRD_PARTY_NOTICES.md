# Third-party notices

FluentQt's MIT license covers only the project's own source code. Bundled
assets and runtime dependencies remain under their upstream licenses. Nothing
in FluentQt's MIT license relicenses those materials or grants trademark
rights.

## Inter

The text faces in `res/fonts/` are renamed derivatives of the hinted static
Inter 4.1 TrueType fonts, copyright The Inter Project Authors. Inter is
licensed under the SIL Open Font License 1.1. The derivatives use
FluentQt-specific family names and do not use Inter's Reserved Font Name.

- Source and provenance: `third_party/fonts/inter/README.md`
- License: `third_party/fonts/inter/LICENSE.txt`

## Fluent UI System Icons

The complete Regular icon face and name-to-codepoint catalog in `res/icons/`
are renamed derivatives of Microsoft Fluent UI System Icons 1.1.328. Fluent UI
System Icons is licensed under the MIT License. The generator preserves every
upstream catalog entry and adds FluentQt's semantic shortcuts without copying
any proprietary Segoe font outlines.

- Source and provenance: `third_party/icons/fluentui-system-icons/README.md`
- License: `third_party/icons/fluentui-system-icons/LICENSE.txt`
- Notice: `third_party/icons/fluentui-system-icons/NOTICE.txt`

## Qt runtime

The reusable FluentQt library dynamically links to Qt Core, Qt GUI, and Qt
Widgets. The Gallery also dynamically links to Qt Network. Official Windows
and macOS Gallery packages use Qt's shared libraries and deploy only the Qt
libraries and plug-ins selected by `windeployqt` or `macdeployqt`. Linux DEB
packages do not bundle Qt; they depend on the distribution's Qt packages.

Open-source Gallery packages that contain Qt use the applicable Qt components
under the GNU Lesser General Public License version 3. A generated
`RUNTIME_DEPENDENCIES.txt` in each package records the exact Qt version and
whether the package contains Qt binaries. The full GPLv3/LGPLv3 terms,
corresponding-source offer, and replacement/relinking instructions are under
`third_party/runtime/qt/NOTICE.md` and are installed with binary packages.

For every official package that contains Qt binaries, the distributor must
retain the exact corresponding Qt Base source under its control for the period
stated in the written offer. `qtbase` covers the Qt Core, GUI, Widgets, Network,
and platform plug-in runtime contract currently used by Gallery, including the
third-party source and license material shipped inside Qt Base. If the runtime
contract grows beyond Qt Base, the corresponding module source must be added
before publishing the package.

## spdlog

The Gallery's application-side logging support uses spdlog. The reusable
FluentQt library does not depend on spdlog. Windows packages may redistribute
the shared spdlog runtime selected by vcpkg; spdlog is licensed under the MIT
License.

- License: `third_party/runtime/spdlog/LICENSE.txt`

## fmt

spdlog uses fmt. Windows packages may redistribute the shared fmt runtime
selected by vcpkg; fmt is licensed under the MIT License with the optional
binary-embedding exception reproduced in its license file.

- License: `third_party/runtime/fmt/LICENSE.txt`

GoogleTest is used only by the test suite and is not included in Gallery or
library release packages. Platform runtime libraries, when present, retain
their vendor terms. See `TRADEMARKS.md` for names, logos, and external design
references that are not covered by the project license.

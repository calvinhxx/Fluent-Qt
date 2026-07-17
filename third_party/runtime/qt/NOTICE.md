# Qt runtime notice

Fluent-Qt's reusable library and Gallery are dynamically linked to Qt. The
package's generated `RUNTIME_DEPENDENCIES.txt` records the exact Qt version and
whether that package contains Qt binaries. `LICENSE.txt` contains the full
GPLv3/LGPLv3 terms.

## Replacing the Qt runtime

Fluent-Qt does not prohibit reverse engineering for the purpose of debugging a
modification to Qt. The Gallery source is MIT licensed, and official desktop
packages use dynamic linking so recipients can use a modified ABI-compatible Qt
build.

- Windows Qt DLLs are beside the Gallery executable under `bin`, with plug-ins
  in subdirectories such as `bin/platforms`. They may be replaced with
  same-named files from a compatible shared Qt build.
- macOS frameworks and plug-ins are under the app bundle's `Contents/Frameworks`
  and `Contents/PlugIns`. After replacing them and correcting any changed
  install names, ad-hoc sign the modified bundle again with
  `codesign --force --deep --sign - "Fluent-Qt Gallery.app"`.
- Official Linux DEBs do not contain Qt libraries. A modified shared Qt can be
  selected through its loader and plug-in paths, or the MIT-licensed Gallery
  can be rebuilt against it.

## Corresponding-source offer

For an official Fluent-Qt Gallery package that contains Qt under GNU LGPLv3,
the maintainers retain under their control the exact
`qtbase-everywhere-src-<Qt-version>.tar.xz` source named in
`RUNTIME_DEPENDENCIES.txt`.

To receive a copy, open an issue at
<https://github.com/calvinhxx/Fluent-Qt/issues> and include the Gallery version,
platform, architecture, and Qt version. The source will be provided without
charge other than reasonable physical-media and delivery cost if physical
delivery is requested. This offer remains valid for at least three years after
the applicable official binary package was last distributed.

A downstream distributor must retain and offer the exact source for the Qt
binaries it conveys; an upstream URL alone is not a substitute for its own
obligation.

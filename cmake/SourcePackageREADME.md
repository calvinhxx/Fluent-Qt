# FluentQt Library Source Package

This archive contains the reusable FluentQt UI component library, its optional
PySide6 binding sources, and minimal integration material. The native C++
Gallery, top-level C++ tests, and application logging support are not part of
this package. The standalone `FluentQt-Gallery` application is intentionally
excluded because it depends on the native Gallery catalogs and artwork from
the complete repository. This archive still builds the reusable `FluentQt`
Python wheel; use a full checkout to build the separate Gallery wheel.

Requirements:

- C++17
- CMake 3.16+
- Qt Widgets 5.15+ or 6.2+

The optional PySide6 binding target requires Python 3.10+ and matching Qt,
PySide6, Shiboken6, and Shiboken6 generator versions from 6.2 onward. See
`bindings/pyside6/README.md` for setup, wheel, and validation commands,
`bindings/pyside6/API_COMPATIBILITY.md` for API version governance, and
`bindings/pyside6/MANYLINUX.md` for the publishable Linux wheel boundary.

Top-level development builds include `FluentQt` and the
`fluentqt_hello_world` executable example. Source-subproject builds keep the
example disabled and build only the library. The included
`examples/hello_world` project demonstrates both in-tree and installed-package
integration.

The project's own source is MIT licensed. Bundled assets retain the licenses
and notices included in `THIRD_PARTY_NOTICES.md` and `third_party/`. Qt is a
consumer-supplied dynamic dependency of this source package and is not covered
by the FluentQt MIT license. See `TRADEMARKS.md` for name and design-reference
disclaimers.

Minimal source integration:

```cmake
add_subdirectory(path/to/FluentQt-source)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

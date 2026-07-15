# FluentQt Library Source Package

This archive contains the reusable FluentQt UI component library and its minimal
integration material. Gallery, tests, and application logging support are not
part of this package.

Requirements:

- C++17
- CMake 3.16+
- Qt Widgets 5.15+ or 6.2+

Top-level development builds include `FluentQt` and the
`fluentqt_hello_world` executable example. Source-subproject builds keep the
example disabled and build only the library. The included
`examples/hello_world` project demonstrates both in-tree and installed-package
integration.

Minimal source integration:

```cmake
add_subdirectory(path/to/FluentQt-source)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

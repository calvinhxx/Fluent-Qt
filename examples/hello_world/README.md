# FluentQt Hello World

This Qt Widgets application works both as an in-tree executable example and as
a standalone consumer of an installed FluentQt package. It shows the complete
runtime setup: construct `QApplication`, initialize the bundled resources,
create a FluentQt window, and add one component.

```cmake
find_package(FluentQt CONFIG REQUIRED)
add_executable(fluentqt_hello_world main.cpp)
target_link_libraries(fluentqt_hello_world PRIVATE FluentQt::FluentQt)
```

When the repository root is opened in Qt Creator or another CMake IDE, select
the `fluentqt_hello_world` target directly. Top-level development builds include
it by default; the release presets disable it for Gallery packaging.

It can also be opened and built as a standalone project after installing
FluentQt:

```bash
cmake -S examples/hello_world -B build/examples/hello_world \
  -DFluentQt_DIR=/path/to/fluentqt/lib/cmake/FluentQt
cmake --build build/examples/hello_world --config Release
```

See the repository [README](../../README.md) for `add_subdirectory`,
`FetchContent`, Qt selection, and deployment guidance.

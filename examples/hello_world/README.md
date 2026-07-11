# Fluent-Qt Hello World

This is a standalone Qt Widgets project that consumes Fluent-Qt as a library
through `find_package`. The main window demonstrates the public initialization
entry point, Fluent controls, typography/icon fonts, style/accent presets
through `StyleThemeCatalog`, and runtime font scaling through `ThemeRegistry`.

The style and accent API used by the sample lives in the reusable library, not
the Gallery application layer:

```cpp
fluent::StyleThemeCatalog::apply(fluent::StyleTheme::Material);
fluent::StyleThemeCatalog::applyAccentOverride(QColor(0, 120, 212));
fluent::ThemeRegistry::instance().setFontScale(1.0);
fluent::FluentElement::refreshTheme();
```

The example links the installed SDK target:

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(fluentqt_hello_world PRIVATE FluentQt::FluentQt)
```

Call `fluent::initializeResources()` after constructing `QApplication` so the
bundled static Segoe UI typography faces and Segoe Fluent Icons are registered.
Set the application font to `Typography::Styles::Body.toQFont()` when raw Qt
widgets should inherit the same Text Regular face as Fluent components.

Build from the repository root:

```bash
cmake --install build/vcpkg-osx \
  --prefix /tmp/fluentqt-sdk \
  --component Development

cmake -S examples/hello_world -B build/examples/hello_world \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DFluentQt_DIR=/tmp/fluentqt-sdk/lib/cmake/FluentQt \
  -DCMAKE_PREFIX_PATH=$PWD/build/vcpkg-osx/vcpkg_installed/arm64-osx \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/examples/hello_world
./build/examples/hello_world/fluentqt_hello_world
```

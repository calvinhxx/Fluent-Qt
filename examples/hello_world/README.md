# Fluent-Qt Hello World

This is a minimal standalone Qt Widgets project that consumes Fluent-Qt as a
library through `find_package`.

The example links the installed SDK target:

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(fluentqt_hello_world PRIVATE FluentQt::FluentQt)
```

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

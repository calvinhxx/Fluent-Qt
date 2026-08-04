#!/usr/bin/env bash
set -euo pipefail

required_variables=(
    FLUENTQT_MANYLINUX_POLICY
    FLUENTQT_MANYLINUX_ARCH
    FLUENTQT_MANYLINUX_BUILD_DIR
    FLUENTQT_PYTHON_TAG
    FLUENTQT_QT_ROOT
    FLUENTQT_PYSIDE_VERSION
    FLUENTQT_SHIBOKEN_VERSION
    FLUENTQT_AUDITWHEEL_VERSION
    FLUENTQT_HOST_UID
    FLUENTQT_HOST_GID
)
for variable in "${required_variables[@]}"; do
    if [[ -z "${!variable:-}" ]]; then
        echo "Missing required environment variable: $variable" >&2
        exit 2
    fi
done

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
case "$FLUENTQT_MANYLINUX_BUILD_DIR" in
    build/pyside6-manylinux-*) ;;
    *)
        echo "Unsafe manylinux build directory: $FLUENTQT_MANYLINUX_BUILD_DIR" >&2
        exit 2
        ;;
esac
build_dir="$repo_root/$FLUENTQT_MANYLINUX_BUILD_DIR"
if [[ ! "$FLUENTQT_HOST_UID" =~ ^[0-9]+$ ]] ||
    [[ ! "$FLUENTQT_HOST_GID" =~ ^[0-9]+$ ]]; then
    echo "Host UID and GID must be numeric." >&2
    exit 2
fi

restore_build_ownership() {
    if [[ -e "$build_dir" ]]; then
        chown -R "$FLUENTQT_HOST_UID:$FLUENTQT_HOST_GID" "$build_dir"
    fi
}
trap restore_build_ownership EXIT

python_root="/opt/python/${FLUENTQT_PYTHON_TAG}-${FLUENTQT_PYTHON_TAG}"
python="$python_root/bin/python"
if [[ ! -x "$python" ]]; then
    echo "The manylinux image does not provide $python" >&2
    exit 2
fi
if [[ ! -f "$FLUENTQT_QT_ROOT/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "The mounted Qt SDK is invalid: $FLUENTQT_QT_ROOT" >&2
    exit 2
fi

# Qt's public CMake targets and GUI library use these manylinux-policy system
# libraries, while the Shiboken generator requires the libxslt runtime. They
# stay external; auditwheel remains responsible for every non-policy library
# other than the pinned PySide6/Shiboken6/Qt wheel runtime.
dnf install -y \
    fontconfig-devel \
    freetype-devel \
    libX11-devel \
    libXext-devel \
    libXrender-devel \
    libxslt \
    libxcb-devel \
    libxkbcommon-devel \
    mesa-libGL-devel

"$python" -m pip install \
    "auditwheel==$FLUENTQT_AUDITWHEEL_VERSION" \
    "PySide6-Essentials==$FLUENTQT_PYSIDE_VERSION"
"$python" -m pip install \
    --index-url https://download.qt.io/official_releases/QtForPython/ \
    "shiboken6_generator==$FLUENTQT_SHIBOKEN_VERSION"

cmake -E remove_directory "$build_dir"
clang_root="$build_dir/shiboken-clang-19"
"$python" "$repo_root/.github/scripts/setup-shiboken-clang.py" \
    --clang-major 19 \
    --output "$clang_root"
export CLANG_INSTALL_DIR="$clang_root"

cmake -S "$repo_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$FLUENTQT_QT_ROOT" \
    -DPython_EXECUTABLE="$python" \
    -DFLUENT_QT_BUILD_PYSIDE6_BINDINGS=ON \
    -DFLUENT_QT_BUILD_EXAMPLES=OFF \
    -DFLUENT_QT_BUILD_GALLERY=OFF \
    -DFLUENT_QT_BUILD_TESTS=OFF \
    -DFLUENT_QT_INSTALL=OFF \
    -DFLUENT_QT_PYSIDE6_MANYLINUX_POLICY="$FLUENTQT_MANYLINUX_POLICY" \
    -DFLUENT_QT_PYSIDE6_AUDITWHEEL_VERSION="$FLUENTQT_AUDITWHEEL_VERSION" \
    -DBUILD_TESTING=ON
cmake --build "$build_dir" \
    --target fluentqt_pyside6_manylinux_wheel \
    --parallel "${FLUENTQT_BUILD_PARALLEL:-4}"

wheel_count="$(find "$build_dir/manylinux-wheelhouse" -maxdepth 1 -type f -name '*.whl' | wc -l)"
if [[ "$wheel_count" -ne 1 ]]; then
    echo "Expected one repaired manylinux wheel, found $wheel_count" >&2
    exit 1
fi
test -s "$build_dir/manylinux-audit.json"

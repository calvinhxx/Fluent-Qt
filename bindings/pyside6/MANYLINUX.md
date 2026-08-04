# PySide6 manylinux build and audit policy

Linux wheels intended for publication must be built and repaired at the same
glibc floors as the exact pinned PySide6 runtime. For the first FluentQt
Python release, PySide6-Essentials 6.9.3 publishes
`manylinux_2_28_x86_64` and `manylinux_2_39_aarch64` wheels, so FluentQt uses
the same two platform tags. A native `linux_*` artifact is test evidence only
and must never be uploaded to PyPI.

The authoritative values live in [`wheel-matrix.json`](wheel-matrix.json):

| Architecture | CPython | Build image | Publish tag |
|---|---|---|---|
| x86_64 | 3.11 | `quay.io/pypa/manylinux_2_28_x86_64` | `manylinux_2_28_x86_64` |
| aarch64 | 3.12 | `quay.io/pypa/manylinux_2_39_aarch64` | `manylinux_2_39_aarch64` |

The ARM64 policy is intentionally newer because the official PySide6 6.9.3
ARM64 wheel itself requires glibc 2.39. Do not relabel either architecture to
an older policy. PyPA currently labels the `manylinux_2_39` image as alpha, so
the immutable image digest and full native ARM64 lane are mandatory evidence.
musllinux is outside the first-release matrix.

Linux ARM64 also uses CPython 3.12. The official Shiboken 6.9.3 aarch64
runtime omits the owned-reference increment when returning `Py_None` from
wrapped void functions; on CPython 3.11 this eventually aborts with
`none_dealloc`, while CPython 3.12 singletons are immortal. Binding
configuration runs a small ownership probe and rejects the unsafe runtime
combination. This architecture-specific release floor does not change the
project-wide Python 3.10 or Qt 6.2 compatibility baselines.

## Build and repair

The full CI matrix mounts the matching official Qt 6.9.3 SDK into the native
PyPA manylinux image and runs
[`tools/build_manylinux_wheel.sh`](tools/build_manylinux_wheel.sh). The script
uses `/opt/python/cp311-cp311` for x86_64 and `/opt/python/cp312-cp312` for
aarch64, the exact PySide6/Shiboken6 6.9.3 packages, and the auditwheel version
pinned by the matrix. It configures a fresh release build and invokes the
opt-in CMake target:

```bash
cmake --build build/pyside6-manylinux-... \
  --target fluentqt_pyside6_manylinux_wheel \
  --parallel
```

`tools/repair_manylinux_wheel.py` calls `auditwheel repair` with the exact
policy plus `--only-plat`. FluentQt does not bundle Qt, PySide6, or Shiboken6:
the wheel metadata pins `PySide6-Essentials` and `shiboken6`, and auditwheel is
told to exclude `libQt6*.so.6`, `libpyside6*.so.*`, and
`libshiboken6*.so.*`. This avoids loading a second Qt runtime in one process.
All other non-policy ELF dependencies remain auditwheel errors or are repaired
normally.

The extension must retain these relocatable lookup paths into the dependent
wheels (auditwheel may additionally add a wheel-internal path for libraries it
grafts normally):

```text
$ORIGIN/../PySide6
$ORIGIN/../shiboken6
$ORIGIN/../PySide6/Qt/lib
```

## Required audit evidence

Every Linux release lane must produce one repaired wheel and
`manylinux-audit.json`. The gate verifies:

- the filename and WHEEL metadata contain only the architecture's declared
  manylinux tag;
- wheel metadata pins the exact PySide6/Shiboken6 versions that provide the
  excluded shared libraries;
- no Qt, PySide6, or Shiboken6 library was copied into FluentQt's wheel;
- the extension declares the expected Qt Widgets, PySide6, and Shiboken6
  dependencies and all three relative runtime paths;
- `auditwheel show` completes, and the report records the repaired wheel hash,
  auditwheel version, exclusions, tags, ELF dependencies, and resolved
  manylinux image digest;
- the repaired wheel installs in a fresh environment on the matching native
  runner, passes the full wheel smoke, `pip check`, strict mypy, and `ldd`
  resolution without `LD_LIBRARY_PATH`.

The six release architectures must pass together before a publication job can
consume their artifacts. Compatibility lanes using Qt/PySide 6.2.4 remain
non-publishable regression gates and are not repaired.

Upstream references: [PyPA manylinux images](https://github.com/pypa/manylinux),
[auditwheel](https://github.com/pypa/auditwheel), and the
[PySide6-Essentials 6.9.3 files](https://pypi.org/project/PySide6-Essentials/6.9.3/).

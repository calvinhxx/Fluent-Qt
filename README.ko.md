<p align="center">
  <a href="README.md">English</a> | <a href="README.zh-CN.md">简体中文</a> | <a href="README.zh-TW.md">繁體中文</a> | <a href="README.ja.md">日本語</a> | 한국어 | <a href="README.de.md">Deutsch</a> | <a href="README.ru.md">Русский</a> | <a href="README.es.md">Español</a> | <a href="README.fr.md">Français</a>
</p>

<p align="center">
  <img src="app/assets/app-icon.png" width="88" alt="Fluent-Qt logo">
</p>

<h1 align="center">Fluent-Qt</h1>

<p align="center">
  MIT 라이선스의 Qt Widgets용 네이티브 Fluent UI 컴포넌트 라이브러리 — 단일 C++17 구현으로 C++, PySide6, WebAssembly 지원.
</p>

<p align="center">
  <a href="https://github.com/calvinhxx/Fluent-Qt/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/calvinhxx/Fluent-Qt/actions/workflows/ci.yml/badge.svg"></a>
  <a href="https://github.com/calvinhxx/Fluent-Qt/stargazers"><img alt="GitHub stars" src="https://img.shields.io/github/stars/calvinhxx/Fluent-Qt?style=flat&color=111827"></a>
  <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/badge/license-MIT-111827.svg"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20WebAssembly-111827.svg">
  <img alt="Qt Widgets" src="https://img.shields.io/badge/UI-Qt%20Widgets-41CD52.svg">
  <img alt="Qt" src="https://img.shields.io/badge/Qt-5.15%2B%20%7C%206.2%2B-41CD52.svg">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C.svg">
  <a href="bindings/pyside6/README.md"><img alt="PySide6" src="https://img.shields.io/badge/PySide6-6.2.4%2B-3776AB.svg"></a>
  <a href="https://pypi.org/project/FluentQt/"><img alt="PyPI" src="https://img.shields.io/pypi/v/FluentQt?color=3776AB"></a>
</p>

<p align="center">
  <a href="https://calvinhxx.github.io/Fluent-Qt/ko/#gallery"><img src="site/assets/gallery/hero-showcase.webp" width="830" alt="Fluent-Qt 애니메이션 데모: Light/Dark Mica Gallery, Page Studio 실시간 C++ WASM 편집, C++/PySide6/.ui 내보내기, SpatialView 3D"></a>
</p>

<p align="center">
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/ko/#gallery">실시간 C++ Web Gallery 체험하기</a></strong>
  ·
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/studio/?lang=en">Page Studio</a></strong>
  ·
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/ko/api/checkbox/?playground=1">속성 미리 보기</a></strong>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/ko/api/">문서 및 API</a>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/ko/#top">프로젝트 웹사이트</a>
  ·
  <a href="https://github.com/calvinhxx/Fluent-Qt/discussions">질문 및 커뮤니티</a>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/ko/#support">프로젝트 후원</a>
</p>

Fluent-Qt(FluentQt)는 단일 네이티브 C++17 코드베이스로 C++, PySide6, WebAssembly에 Fluent UI를 제공합니다:

- **상용 및 오픈소스 모두 자유로운 MIT 라이선스** — C++17 라이브러리, PySide6 바인딩, Qt Designer 플러그인, 앱 스타터 모두 MIT 라이선스를 따릅니다.
- **하나의 네이티브 C++17 구현** — 단순 스타일시트가 아니라 `DataGrid`, [차트](docs/architecture/charts.md), 내비게이션, 오버레이, 대화 상자, 창 등 99개 Gallery 항목이 `Qt 5.15+`/`6.2+`, `PySide6`, `WebAssembly`에서 동일한 C++17 위젯으로 동작합니다.
- **Page Studio 및 Qt Designer 시각적 설계** — 브라우저 **Page Studio**에서 C++, PySide6, `.ui` 파일로 내보내거나 **Qt Creator** 및 `pyside6-designer`에서 95개 위젯 항목과 4개 폼 템플릿을 바로 사용할 수 있습니다.
- **앱 스타터 CLI 및 AI GUI 스킬** — `python3 tools/onboarding/fluentqt`로 계층화된 `monitor`, `workbench`, `existing-qt` 앱을 생성하고 [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md) 스킬로 UI를 구축할 수 있습니다.
- **선택형 확장 모듈** — 기본 2D 앱은 `Qt Widgets`에만 의존하며 CPU 파티클 배경([Particles](docs/architecture/particles.md)), 3D 원근 뷰([Spatial](docs/architecture/spatial-view.md)), 임베디드 QML 페이지([QmlBridge](docs/architecture/qml-bridge.md))는 필요할 때만 활성화합니다.

## 🧩 라이브러리 모듈

| 모듈 | CMake 타깃 | 설명 |
|---|---|---|
| Core | `FluentQt::FluentQt` | 모든 2D 컨트롤, 테마, 타이포그래피, 레이아웃 헬퍼 및 차트. 일반 2D 앱은 이 타깃만 링크하면 됩니다. |
| [Plugins](docs/architecture/plugins.md) | `FluentQt::Plugins` | 인프로세스 플러그인 등록, 의존성 정렬, 활성화 및 정리. |
| [Particles](docs/architecture/particles.md) | `FluentQt::Particles` | CPU 렌더링 애니메이션 파티클 배경(`ParticleBackdrop`, OpenGL 불필요) 및 `ParticlesSession` 일시 중지/재개. |
| [Spatial](docs/architecture/spatial-view.md) | `FluentQt::Spatial` | 기존 `QWidget` 레이아웃에 원근감을 더합니다. 세션 활성화나 뷰 생성에 실패하면 앱이 [2D 레이아웃](examples/hello_world/README.md)을 제공합니다. |
| [QmlBridge](docs/architecture/qml-bridge.md) | `FluentQt::QmlBridge` | 앱 모델과 Fluent 테마를 공유하면서 Widgets 창 안에 QML 페이지를 임베딩하는 `QmlView`. |

C++ 앱은 이 타깃들을 직접 링크하고, Python 앱은 대응하는 PySide6 모듈을 사용하며, WebAssembly는 동일한 C++ 코드를 브라우저용으로 컴파일합니다. Gallery의 **Plugins → Particles**, **Plugins → Spatial**(**Settings → 3D Gallery** 활성화), **Plugins → QmlBridge → QmlView**에서 확장 예제를 체험하거나 [최소 QML 예제](examples/qml_bridge/)를 참고하세요.

## 🖼 Gallery 및 온라인 문서

Gallery를 통해 컨트롤을 둘러보고 인터랙션을 시험하며 C++ 및 Python 예제 코드를 확인할 수 있습니다.

| 실행 환경 | 바로가기 |
|---|---|
| 브라우저 (WebAssembly Gallery) | [라이브 Gallery](https://calvinhxx.github.io/Fluent-Qt/gallery/) · [프로젝트 웹사이트](https://calvinhxx.github.io/Fluent-Qt/ko/#gallery) |
| 온라인 문서 및 속성 미리 보기 | [가이드 및 API 문서](https://calvinhxx.github.io/Fluent-Qt/ko/api/) · [속성 미리 보기 체험](https://calvinhxx.github.io/Fluent-Qt/ko/api/checkbox/?playground=1) |
| Windows, macOS, Linux | [Gallery 패키지 다운로드](https://github.com/calvinhxx/Fluent-Qt/releases/latest) |

### 컴포넌트 문서 및 속성 미리 보기

[온라인 문서](https://calvinhxx.github.io/Fluent-Qt/ko/api/)에서 기초 가이드, 컴포넌트 API와 [릴리스 노트](https://calvinhxx.github.io/Fluent-Qt/ko/api/releases/)를 한국어로 볼 수 있습니다. 각 컴포넌트 페이지에서 **속성 미리 보기**를 열면 실제 Qt 속성을 조절하고 생성된 C++ 또는 PySide6 코드를 복사할 수 있습니다:

<p>
  <a href="https://calvinhxx.github.io/Fluent-Qt/ko/api/checkbox/?playground=1">
    <img src="site/assets/playground/checkbox-en-light.webp" width="760" alt="체크된 상자와 생성된 C++ 코드를 보여 주는 CheckBox 속성 패널">
  </a>
</p>

빠른 체험: [Button](https://calvinhxx.github.io/Fluent-Qt/ko/api/button/?playground=1) · [CheckBox](https://calvinhxx.github.io/Fluent-Qt/ko/api/checkbox/?playground=1) · [ComboBox](https://calvinhxx.github.io/Fluent-Qt/ko/api/combobox/?playground=1) · [DataGrid](https://calvinhxx.github.io/Fluent-Qt/ko/api/data-grid/?playground=1) · [LineChart](https://calvinhxx.github.io/Fluent-Qt/ko/api/line-chart/?playground=1) · [ParticleBackdrop](https://calvinhxx.github.io/Fluent-Qt/ko/api/particle-backdrop/?playground=1) · [SpatialView](https://calvinhxx.github.io/Fluent-Qt/ko/api/spatial-view/?playground=1) · [QmlView](https://calvinhxx.github.io/Fluent-Qt/ko/api/qml-view/?playground=1)

### Python Gallery

[PyPI](https://pypi.org/project/FluentQt-Gallery/)에서 Python Gallery를 설치할 수 있습니다(호환되는 `FluentQt` 버전이 함께 설치됩니다):

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

## 🎨 시각적 설계

- **Page Studio(웹)**: 브라우저에서 [Page Studio](https://calvinhxx.github.io/Fluent-Qt/studio/?lang=en)로 Fluent-Qt 컨트롤을 배치하고, 템플릿이나 AI 기반 레이아웃에서 시작해 실시간으로 미리 본 뒤 C++, PySide6, Qt Designer `.ui` 파일로 내보낼 수 있습니다.

### Qt Designer

모든 모듈을 설치하면 Qt Creator와 `pyside6-designer`의 위젯 상자에 95개 항목이 표시되며 새 폼을 만들 때 템플릿 4개를 선택할 수 있습니다. 코어 모듈만 설치하면 각각 92개와 3개입니다. [Designer 설정](tools/designer/README.md).

<p>
  <a href="tools/designer/README.md">
    <img src="docs/assets/readme/qt-designer.webp" width="760" alt="FluentQt 위젯 상자, 폼 캔버스 및 FluentButton 속성 편집기를 보여 주는 Qt Designer">
  </a>
</p>

## 🤖 AI로 GUI 구축하기

**AI 스킬 및 온보딩 CLI**: Codex, Claude Code 또는 Cursor에서 [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md) 스킬을 사용하고([예제 보기](https://calvinhxx.github.io/Fluent-Qt/ko/#ai-build) · [설치 및 사용법](docs/ai/README.md)), `python3 tools/onboarding/fluentqt`([온보딩 도구](tools/onboarding/README.md))로 환경 점검과 계층형 C++ / PySide6 스타터(`monitor`, `workbench`, `existing-qt`) 생성을 진행할 수 있습니다. Gallery 예제를 다듬을 때는 저장된 변경 사항을 Live Scene에서 먼저 미리 본 뒤 컴파일된 C++ 예제로 확인하세요([작동 원리](docs/development/gallery-preview-workflow.md)).

```bash
python3 tools/onboarding/fluentqt create my-monitor --language cpp --starter monitor
python3 tools/onboarding/fluentqt create my-py-monitor --language pyside6 --starter monitor
```

## 🚀 빠른 시작

Fluent-Qt는 **Python(PySide6)** 과 **C++17(CMake)** 모두에서 네이티브로 실행됩니다.

### Python (PySide6)

Qt/PySide6 6.9.3 wheel은 CPython 3.11–3.13을 지원합니다. Linux ARM64에는 CPython 3.12–3.13과 glibc 2.39 이상이 필요합니다.

```bash
python -m pip install FluentQt
```

`main.py`:

```python
import sys
import fluentqt
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication, QVBoxLayout, QWidget
from fluentqt.basicinput import Button
from fluentqt.windowing import Window

fluentqt.prepare_high_dpi_application()
app = QApplication(sys.argv)
fluentqt.initialize_resources()
app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

window = Window()
window.resize(480, 320)
content = QWidget()
button = Button("Hello from FluentQt", content)
button.setFluentStyle(Button.ButtonStyle.Accent)
QVBoxLayout(content).addWidget(button, alignment=Qt.AlignmentFlag.AlignCenter)
window.setContentWidget(content)
window.show()
sys.exit(app.exec())
```

[Python 가이드](bindings/pyside6/README.md) · [Hello World](bindings/pyside6/examples/hello_world/main.py)

### C++ 설정

C++ 통합에는 C++17, CMake 3.16+, Qt Widgets 5.15+ 또는 6.2+가 필요합니다. 새 프로젝트에는 `FetchContent` 사용을 권장합니다.

#### `FetchContent` 통합

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    fluentqt
    GIT_REPOSITORY https://github.com/calvinhxx/Fluent-Qt.git
    GIT_TAG v1.12.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fluentqt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### 소스 통합

애플리케이션 타깃을 정의한 뒤 Fluent-Qt 소스 디렉터리를 추가하고 내보낸 타깃을 링크하세요:

```cmake
add_subdirectory(Fluent-Qt)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### 설치된 라이브러리 통합

개발용 헤더와 CMake 패키지 파일이 포함된 FluentQt 설치본을 사용하세요. 자세한 내용은 [라이브러리 빌드 및 설치 가이드](docs/development/build-workflow.md#install-the-library-or-create-a-source-package)를 참고하세요.

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

FluentQt가 시스템 검색 경로 밖에 있는 경우 `-DCMAKE_PREFIX_PATH=/path/to/fluentqt`로 구성하세요.

#### C++ 최소 예제

`main.cpp`:

```cpp
#include <FluentQt/FluentQt.h>

#include <QApplication>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char* argv[])
{
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    fluent::initializeResources();
    app.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());

    fluent::windowing::Window window;
    window.setWindowTitle(QStringLiteral("FluentQt Hello World"));
    window.resize(480, 320);

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(32, 32, 32, 32);

    auto* button = new fluent::basicinput::Button(
        QStringLiteral("Hello from FluentQt"), content);
    button->setFluentStyle(fluent::basicinput::Button::Accent);
    layout->addStretch();
    layout->addWidget(button, 0, Qt::AlignCenter);
    layout->addStretch();

    window.setContentWidget(content);
    window.show();
    return app.exec();
}
```

전체 프로젝트는 [`examples/hello_world`](examples/hello_world/)에서 확인하거나 IDE에서 `fluentqt_hello_world` 타깃을 바로 실행해 보세요.

## 🛠 빌드

### 의존성

| 범위 | 의존성 |
|---|---|
| FluentQt C++ 라이브러리 | C++17, CMake 3.16+, Qt Widgets 5.15+ 또는 6.2+ |
| C++ Gallery | FluentQt, Qt Network, spdlog/fmt |
| C++ WebAssembly | Qt 6.9.3 `wasm_singlethread`, Emscripten 3.1.70 |
| 테스트 | FluentQt, Qt Test/Network, GTest, spdlog/fmt |
| 선택적 PySide6 바인딩 | Qt/PySide6/Shiboken6 6.2.4+; 소스 빌드는 Python 3.10+ |

### 라이브러리

```bash
cmake -S . -B build/fluentqt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt
python3 tools/dev/fluent_qt_build.py build/fluentqt --config Release --target FluentQt
cmake --install build/fluentqt --config Release \
  --component Development --prefix /path/to/install
```

빌드 스크립트는 사용 가능한 CPU와 메모리에 따라 병렬 작업 수를 자동 선택합니다. 구성 방법은 [빌드 워크플로](docs/development/build-workflow.md)를 참조하세요.

### 선택적 확장 모듈

FluentQt 구성 시 Plugins 모듈이나 특정 확장을 활성화한 뒤 앱에서 해당 CMake 타깃을 링크하세요:

| 확장 범위 | 빌드 옵션 | CMake 타깃 | 통합 예제 |
|---|---|---|---|
| Plugins 코어 | `-DFLUENT_QT_BUILD_PLUGINS=ON` | `FluentQt::Plugins` | [최소 플러그인 예제](docs/architecture/plugins.md#minimal-extension-example) |
| Plugins → Particles | `-DFLUENT_QT_BUILD_PARTICLES=ON` | `FluentQt::Particles` | [Particles 통합](docs/architecture/particles.md) |
| Plugins → Spatial | `-DFLUENT_QT_BUILD_SPATIAL=ON` | `FluentQt::Spatial` (Plugins 자동 포함) | [Spatial Hello World](examples/hello_world/README.md) |
| Plugins → QmlBridge | `-DFLUENT_QT_BUILD_QML_BRIDGE=ON` | `FluentQt::QmlBridge` (Plugins 자동 포함) | [QML 호스트 예제](examples/qml_bridge/) |

소스에서 빌드할 때는 `add_subdirectory()` 또는 `FetchContent_MakeAvailable()` 전에 이 옵션들을 설정하세요. 대응하는 Python 빌드는 `fluentqt.plugins`, `fluentqt.spatial`, `fluentqt.qmlbridge` 모듈을 노출하며 WebAssembly는 공유된 C++ 구현을 컴파일합니다.

### 로컬에서 Gallery 빌드

저장소 프리셋은 CMake 3.25+와 `VCPKG_ROOT`가 필요합니다. [초기 설정](docs/development/build-workflow.md#first-use-setup)을 참고해 Qt kit를 구성하세요.

현재 호스트에서 사용 가능한 프리셋 목록 확인:

```bash
cmake --list-presets
```

Apple Silicon에서는 `vcpkg-osx`를 사용합니다:

```bash
cmake --preset vcpkg-osx
python3 tools/dev/fluent_qt_build.py \
  --preset vcpkg-osx \
  --target fluent_qt_gallery
```

다른 플랫폼에서는 대응하는 프리셋을 사용하세요:

| 플랫폼 | 프리셋 |
|---|---|
| Apple Silicon Mac | `vcpkg-osx` |
| Intel Mac 또는 Rosetta | `vcpkg-osx-x64` |
| Linux x64 | `vcpkg-linux` |
| Linux ARM64 | `vcpkg-linux-arm64` |
| Windows x64 | `vcpkg-windows` |
| Windows ARM64 | `vcpkg-windows-arm64` |

패키징 시에는 대응하는 `-release` 프리셋을 사용하고 [패키징 워크플로](docs/development/packaging-workflow.md)의 검증 단계를 따르세요.

### WebAssembly

브라우저 툴체인, 로컬 빌드, 검증 및 Pages 배포 안내는 [WebAssembly 워크플로](docs/development/webassembly-workflow.md)를 확인하세요.

### 소스 패키지

오프라인 또는 소스 통합을 위한 축소된 라이브러리 소스 패키지를 생성합니다:

```bash
python3 tools/dev/fluent_qt_build.py build/fluentqt --target fluent_qt_source_package
```

## 📚 문서

[문서 지도](docs/README.md)에서 시작하거나 필요한 항목으로 바로 이동하세요:

| 목표 | 바로가기 |
|---|---|
| 컨트롤 및 API 탐색 | [컴포넌트 문서 및 API](https://calvinhxx.github.io/Fluent-Qt/ko/api/) · [WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/ko/#gallery) |
| 앱 구축 | [Qt Designer](tools/designer/README.md) · [AI 지원 개발](docs/ai/README.md) · [온보딩 도구](tools/onboarding/README.md) |
| FluentQt 기여 | [개발 가이드](docs/development/README.md) · [아키텍처](docs/architecture/README.md) · [Fluent 디자인](docs/design-languages/README.md) |
| 패키징 및 릴리스 | [패키징](docs/development/packaging-workflow.md) · [릴리스 거버넌스](docs/development/release-governance.md) · [릴리스 노트](docs/releases/README.md) |
| 도움 받기 및 버그 제보 | [커뮤니티](docs/community/README.md) · [지원](SUPPORT.md) · [보안](SECURITY.md) |

Pull Request를 열기 전에 [기여 가이드](CONTRIBUTING.md)와 [행동 강령](CODE_OF_CONDUCT.md)을 읽어 주세요.

## 🔗 참고 자료

| 출처 | 용도 |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows 시각적 참고 자료 |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | 컨트롤 동작 및 예제 페이지 참고 |

## 라이선스

Fluent-Qt 자체 소스 코드는 [MIT License](LICENSE)로 배포됩니다. 번들된 애셋 및 패키징된 런타임 종속성은 각 업스트림 라이선스를 따르며, 버전·출처·소스 제공 여부 및 라이선스 세부 정보는 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에 정리되어 있습니다. 상표 및 서드파티 디자인 참조에 대한 안내는 [TRADEMARKS.md](TRADEMARKS.md)를 확인하세요.

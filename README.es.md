<p align="center">
  <a href="README.md">English</a> | <a href="README.zh-CN.md">简体中文</a> | <a href="README.zh-TW.md">繁體中文</a> | <a href="README.ja.md">日本語</a> | <a href="README.ko.md">한국어</a> | <a href="README.de.md">Deutsch</a> | <a href="README.ru.md">Русский</a> | Español | <a href="README.fr.md">Français</a>
</p>

<p align="center">
  <img src="app/assets/app-icon.png" width="88" alt="Fluent-Qt logo">
</p>

<h1 align="center">Fluent-Qt</h1>

<p align="center">
  Biblioteca multiplataforma de componentes UI en C++ con estilo Fluent para Qt Widgets.
</p>

<p align="center">
  <a href="https://github.com/calvinhxx/Fluent-Qt/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/calvinhxx/Fluent-Qt/actions/workflows/ci.yml/badge.svg"></a>
  <a href="https://github.com/calvinhxx/Fluent-Qt/stargazers"><img alt="GitHub stars" src="https://img.shields.io/github/stars/calvinhxx/Fluent-Qt?style=flat&color=111827"></a>
  <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/badge/license-MIT-111827.svg"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20WebAssembly-111827.svg">
  <img alt="Qt Widgets" src="https://img.shields.io/badge/UI-Qt%20Widgets-41CD52.svg">
  <img alt="Qt" src="https://img.shields.io/badge/Qt-5.15%2B%20%7C%206.2%2B-41CD52.svg">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C.svg">
  <a href="https://pypi.org/project/FluentQt/"><img alt="PyPI" src="https://img.shields.io/pypi/v/FluentQt?color=3776AB"></a>
</p>

<p align="center">
  <a href="https://calvinhxx.github.io/Fluent-Qt/es/#top"><img src="docs/assets/readme/hero.png" alt="Fluent-Qt Gallery Home en modo 3D en Windows con Mica"></a>
</p>

<p align="center">
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/es/#gallery">Probar la Web Gallery en C++ en vivo</a></strong>
  ·
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/es/api/checkbox/?playground=1">Vista previa de propiedades</a></strong>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/es/api/">Documentación y API</a>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/es/#top">Sitio web del proyecto</a>
  ·
  <a href="https://github.com/calvinhxx/Fluent-Qt/discussions">Preguntas y comunidad</a>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/es/#support">Apoyar al proyecto</a>
</p>

Fluent-Qt (FluentQt) ofrece controles con estilo Fluent para Qt Widgets: entradas, navegación, colecciones, tablas de datos, [gráficos](docs/architecture/charts.md), diálogos, ventanas emergentes y ventanas principales. Escrita en C++17, se integra en proyectos Qt existentes mediante CMake y ofrece enlaces opcionales para PySide6.

Las aplicaciones funcionan en Windows, macOS, Linux y WebAssembly con temas Claro, Oscuro y Alto Contraste además de modos de animación configurables. Los módulos opcionales añaden fondos de partículas ([Particles](docs/architecture/particles.md)), vistas con perspectiva 3D ([Spatial](docs/architecture/spatial-view.md)) y páginas QML incrustadas ([QmlBridge](docs/architecture/qml-bridge.md)).

## 🧩 Módulos de la biblioteca

| Módulo | Objetivo CMake | Propósito |
|---|---|---|
| Core | `FluentQt::FluentQt` | Todos los controles 2D, temas, tipografía, utilidades de diseño y gráficos. Las apps 2D habituales solo necesitan este objetivo. |
| [Plugins](docs/architecture/plugins.md) | `FluentQt::Plugins` | Registro de plugins en el proceso, orden de dependencias, activación y limpieza. |
| [Particles](docs/architecture/particles.md) | `FluentQt::Particles` | Fondos animados de partículas renderizados en CPU (`ParticleBackdrop`, sin necesidad de OpenGL) y pausa/reanudación con `ParticlesSession`. |
| [Spatial](docs/architecture/spatial-view.md) | `FluentQt::Spatial` | Añade perspectiva 3D a diseños `QWidget` existentes. Si falla la activación de sesión o la vista, la app ofrece un [diseño 2D](examples/hello_world/README.md). |
| [QmlBridge](docs/architecture/qml-bridge.md) | `FluentQt::QmlBridge` | `QmlView` para alojar páginas QML dentro de Widgets compartiendo modelos de la app y temas Fluent. |

Las aplicaciones C++ enlazan estos objetivos directamente, las aplicaciones Python usan los módulos PySide6 equivalentes y WebAssembly compila el mismo código C++ para el navegador. Prueba los ejemplos de extensiones en Gallery bajo **Plugins → Particles**, **Plugins → Spatial** (activa **Settings → 3D Gallery**) y **Plugins → QmlBridge → QmlView**, o consulta el [ejemplo mínimo de QML](examples/qml_bridge/).

## 🖼 Gallery y Documentación en línea

Usa Gallery para explorar controles, probar interacciones y consultar código de ejemplo en C++ y Python.

| Dónde probarlo | Punto de entrada |
|---|---|
| Navegador (WebAssembly Gallery) | [Gallery en vivo](https://calvinhxx.github.io/Fluent-Qt/gallery/) · [Sitio web del proyecto](https://calvinhxx.github.io/Fluent-Qt/es/#gallery) |
| Documentación y vista previa | [Guías y API](https://calvinhxx.github.io/Fluent-Qt/es/api/) · [Probar vista previa de propiedades](https://calvinhxx.github.io/Fluent-Qt/es/api/checkbox/?playground=1) |
| Windows, macOS, Linux | [Descargar un paquete de Gallery](https://github.com/calvinhxx/Fluent-Qt/releases/latest) |

### Documentación de componentes y vista previa de propiedades

La [documentación en línea](https://calvinhxx.github.io/Fluent-Qt/es/api/) incluye las guías, la referencia de la API de los componentes y las [notas de versión](https://calvinhxx.github.io/Fluent-Qt/es/api/releases/) en español. Abre **Vista de propiedades** en cualquier página de componente para ajustar propiedades reales de Qt y copiar el código C++ o PySide6 generado:

<p>
  <a href="https://calvinhxx.github.io/Fluent-Qt/es/api/checkbox/?playground=1">
    <img src="site/assets/playground/checkbox-en-light.webp" width="760" alt="Panel de propiedades de CheckBox mostrando una casilla marcada y su código C++">
  </a>
</p>

Acceso rápido: [Button](https://calvinhxx.github.io/Fluent-Qt/es/api/button/?playground=1) · [CheckBox](https://calvinhxx.github.io/Fluent-Qt/es/api/checkbox/?playground=1) · [ComboBox](https://calvinhxx.github.io/Fluent-Qt/es/api/combobox/?playground=1) · [DataGrid](https://calvinhxx.github.io/Fluent-Qt/es/api/data-grid/?playground=1) · [LineChart](https://calvinhxx.github.io/Fluent-Qt/es/api/line-chart/?playground=1) · [ParticleBackdrop](https://calvinhxx.github.io/Fluent-Qt/es/api/particle-backdrop/?playground=1) · [SpatialView](https://calvinhxx.github.io/Fluent-Qt/es/api/spatial-view/?playground=1) · [QmlView](https://calvinhxx.github.io/Fluent-Qt/es/api/qml-view/?playground=1)

### Python Gallery

Instala la Gallery para Python desde [PyPI](https://pypi.org/project/FluentQt-Gallery/) (instalará automáticamente la versión compatible de `FluentQt`):

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

## 🤖 Construir con IA

Usa el skill [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md) en Codex, Claude Code o Cursor para crear una aplicación de escritorio, añadir una interfaz gráfica a un proyecto existente o refinar una UI. [Ejemplo](https://calvinhxx.github.io/Fluent-Qt/es/#ai-build) · [Instalación y uso](docs/ai/README.md)

Al ajustar un ejemplo de Gallery, previsualiza los cambios guardados en Live Scene antes de verificar el ejemplo compilado en C++. [Cómo funciona](docs/development/gallery-preview-workflow.md)

## 🚀 Inicio rápido

La integración en C++ requiere C++17, CMake 3.16+ y Qt Widgets 5.15+ o 6.2+. Usa `FetchContent` en proyectos nuevos.

### Configuración en C++

#### Integración con `FetchContent`

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    fluentqt
    GIT_REPOSITORY https://github.com/calvinhxx/Fluent-Qt.git
    GIT_TAG v1.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fluentqt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Integración desde el código fuente

Tras definir el objetivo de tu aplicación, añade el directorio fuente de Fluent-Qt y enlaza el objetivo exportado:

```cmake
add_subdirectory(Fluent-Qt)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Integración con biblioteca instalada

Usa una instalación de FluentQt que incluya las cabeceras de desarrollo y archivos de paquete CMake. Consulta la [guía de compilación e instalación](docs/development/build-workflow.md#install-the-library-or-create-a-source-package).

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

Si FluentQt está fuera de la ruta de búsqueda del sistema, configura con `-DCMAKE_PREFIX_PATH=/path/to/fluentqt`.

### Ejemplo mínimo en C++

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

Consulta [`examples/hello_world`](examples/hello_world/) para ver el proyecto completo o ejecuta el objetivo `fluentqt_hello_world` directamente desde tu IDE.

### Python (PySide6)

Instala los enlaces de PySide6 desde PyPI (mediante Shiboken6):

```bash
python -m pip install FluentQt
```

Consulta la [guía de Python](bindings/pyside6/README.md) para ver ejemplos, compatibilidad y compilación desde fuente.

## 🛠 Compilación

### Dependencias

| Ámbito | Dependencias |
|---|---|
| Biblioteca C++ FluentQt | C++17, CMake 3.16+, Qt Widgets 5.15+ o 6.2+ |
| Gallery en C++ | FluentQt, Qt Network, spdlog/fmt |
| WebAssembly en C++ | Qt 6.9.3 `wasm_singlethread`, Emscripten 3.1.70 |
| Pruebas | FluentQt, Qt Test/Network, GTest, spdlog/fmt |
| Enlaces PySide6 opcionales | Qt/PySide6/Shiboken6 6.2.4+; Python 3.10+ para compilar desde fuente |

### Biblioteca

```bash
cmake -S . -B build/fluentqt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt
python3 tools/dev/fluent_qt_build.py build/fluentqt --config Release --target FluentQt
cmake --install build/fluentqt --config Release \
  --component Development --prefix /path/to/install
```

El script de compilación ajusta el paralelismo según la CPU y memoria disponibles. Consulta el [flujo de compilación](docs/development/build-workflow.md) para ver opciones de configuración.

### Extensiones opcionales

Activa el módulo Plugins o una extensión concreta al configurar FluentQt y enlaza después su objetivo CMake en tu aplicación:

| Alcance de extensión | Opción de compilación | Objetivo CMake | Ejemplo de integración |
|---|---|---|---|
| Núcleo de Plugins | `-DFLUENT_QT_BUILD_PLUGINS=ON` | `FluentQt::Plugins` | [Ejemplo mínimo de plugin](docs/architecture/plugins.md#minimal-extension-example) |
| Plugins → Particles | `-DFLUENT_QT_BUILD_PARTICLES=ON` | `FluentQt::Particles` | [Integración de Particles](docs/architecture/particles.md) |
| Plugins → Spatial | `-DFLUENT_QT_BUILD_SPATIAL=ON` | `FluentQt::Spatial` (incluye Plugins) | [Spatial Hello World](examples/hello_world/README.md) |
| Plugins → QmlBridge | `-DFLUENT_QT_BUILD_QML_BRIDGE=ON` | `FluentQt::QmlBridge` (incluye Plugins) | [Ejemplo de host QML](examples/qml_bridge/) |

Al compilar desde las fuentes, define estas opciones antes de `add_subdirectory()` o `FetchContent_MakeAvailable()`. Las compilaciones Python correspondientes exponen `fluentqt.plugins`, `fluentqt.spatial` y `fluentqt.qmlbridge`, y WebAssembly compila la implementación C++ compartida.

### Compilar Gallery localmente

Los presets del repositorio requieren CMake 3.25+ y `VCPKG_ROOT`. Configura tu kit de Qt siguiendo la [configuración inicial](docs/development/build-workflow.md#first-use-setup).

Lista los presets disponibles en tu máquina:

```bash
cmake --list-presets
```

En Apple Silicon usa `vcpkg-osx`:

```bash
cmake --preset vcpkg-osx
python3 tools/dev/fluent_qt_build.py \
  --preset vcpkg-osx \
  --target fluent_qt_gallery
```

Usa el preset correspondiente en otras plataformas:

| Plataforma | Preset |
|---|---|
| Mac con Apple Silicon | `vcpkg-osx` |
| Mac Intel o Rosetta | `vcpkg-osx-x64` |
| Linux x64 | `vcpkg-linux` |
| Linux ARM64 | `vcpkg-linux-arm64` |
| Windows x64 | `vcpkg-windows` |
| Windows ARM64 | `vcpkg-windows-arm64` |

Para empaquetar, usa el preset `-release` correspondiente y los pasos de verificación del [flujo de empaquetado](docs/development/packaging-workflow.md).

### WebAssembly

Consulta el [flujo de WebAssembly](docs/development/webassembly-workflow.md) para herramientas del navegador, compilaciones locales, validación y despliegue en Pages.

### Paquete de código fuente

Crea el paquete de código fuente reducido de la biblioteca para integración sin conexión o desde fuente:

```bash
python3 tools/dev/fluent_qt_build.py build/fluentqt --target fluent_qt_source_package
```

## 📚 Documentación

Comienza en el [mapa de documentación](docs/README.md) o ve directo a lo que necesitas:

| Objetivo | Punto de entrada |
|---|---|
| Explorar controles y API | [Documentación y API](https://calvinhxx.github.io/Fluent-Qt/es/api/) · [WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/es/#gallery) |
| Construir una app | [Desarrollo asistido por IA](docs/ai/README.md) · [Herramientas de inicio](tools/onboarding/README.md) |
| Contribuir a FluentQt | [Guías de desarrollo](docs/development/README.md) · [Arquitectura](docs/architecture/README.md) · [Diseño Fluent](docs/design-languages/README.md) |
| Empaquetar o publicar | [Empaquetado](docs/development/packaging-workflow.md) · [Gobernanza de versiones](docs/development/release-governance.md) · [Notas de versión](docs/releases/README.md) |
| Ayuda o reporte de errores | [Comunidad](docs/community/README.md) · [Soporte](SUPPORT.md) · [Seguridad](SECURITY.md) |

Antes de abrir un pull request, lee [Contributing](CONTRIBUTING.md) y el [Código de Conducta](CODE_OF_CONDUCT.md).

## 🔗 Referencias

| Fuente | Propósito |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Referencia visual de Fluent / Windows |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | Referencia de comportamiento de controles y ejemplos |

## Licencia

El código fuente propio de Fluent-Qt se publica bajo la [Licencia MIT](LICENSE). Los recursos incluidos y las dependencias de ejecución empaquetadas conservan sus licencias originales; versiones, procedencia, disponibilidad del código fuente y detalles figuran en [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Las marcas comerciales y referencias de diseño de terceros se detallan en [TRADEMARKS.md](TRADEMARKS.md).

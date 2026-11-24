<p align="center">
  <a href="README.md">English</a> | <a href="README.zh-CN.md">简体中文</a> | <a href="README.zh-TW.md">繁體中文</a> | <a href="README.ja.md">日本語</a> | <a href="README.ko.md">한국어</a> | <a href="README.de.md">Deutsch</a> | Русский | <a href="README.es.md">Español</a> | <a href="README.fr.md">Français</a>
</p>

<p align="center">
  <img src="app/assets/app-icon.png" width="88" alt="Fluent-Qt logo">
</p>

<h1 align="center">Fluent-Qt</h1>

<p align="center">
  Нативная библиотека Fluent UI для Qt Widgets под лицензией MIT — единая реализация на C++17 для C++, PySide6 и WebAssembly.
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
  <a href="https://calvinhxx.github.io/Fluent-Qt/ru/#gallery"><img src="site/assets/gallery/hero-showcase.webp" width="830" alt="Анимированная демонстрация Fluent-Qt: Gallery со светлой и тёмной Mica, Page Studio на C++ WASM, экспорт в C++/PySide6/.ui и SpatialView 3D"></a>
</p>

<p align="center">
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/ru/#gallery">Открыть живую C++ Web Gallery</a></strong>
  ·
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/studio/?lang=en">Page Studio</a></strong>
  ·
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/ru/api/checkbox/?playground=1">Предпросмотр свойств</a></strong>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/ru/api/">Документация и API</a>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/ru/#top">Сайт проекта</a>
  ·
  <a href="https://github.com/calvinhxx/Fluent-Qt/discussions">Вопросы и сообщество</a>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/ru/#support">Поддержать проект</a>
</p>

Fluent-Qt (FluentQt) добавляет Fluent UI в Qt Widgets из единой нативной кодовой базы на C++17 для C++, PySide6 и WebAssembly:

- **Лицензия MIT для коммерческих и открытых проектов** — библиотека на C++17, привязки PySide6, плагин Qt Designer и стартовые шаблоны распространяются по лицензии MIT.
- **Единая нативная реализация на C++17** — Gallery включает 99 позиций (в том числе `DataGrid`, [графики](docs/architecture/charts.md), навигацию, всплывающие слои, диалоги и окна) с общими виджетами C++17 в C++ (`Qt 5.15+` / `6.2+`), `PySide6` и `WebAssembly`, а не просто таблицы стилей QSS.
- **Визуальное проектирование и общие `.ui`-формы** — собирайте экраны в браузерном **Page Studio** (экспорт в C++, PySide6 или `.ui`) или в **Qt Creator** и `pyside6-designer` (95 элементов палитры и 4 шаблона форм).
- **Стартовые шаблоны приложений и ИИ-навык** — создавайте многослойные проекты `monitor`, `workbench` и `existing-qt` через `python3 tools/onboarding/fluentqt` или проектируйте интерфейсы с [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md).
- **Подключаемые модули расширения** — базовым 2D-приложениям нужен только `Qt Widgets`; фоны с частицами на CPU ([Particles](docs/architecture/particles.md)), виды с 3D-перспективой ([Spatial](docs/architecture/spatial-view.md)) и встроенный QML ([QmlBridge](docs/architecture/qml-bridge.md)) подключаются по требованию.

## 🧩 Модули библиотеки

| Модуль | Цель CMake | Назначение |
|---|---|---|
| Core | `FluentQt::FluentQt` | Все 2D-элементы управления, темы, типографика, помощники компоновки и графики. Обычным 2D-приложениям нужна только эта цель. |
| [Plugins](docs/architecture/plugins.md) | `FluentQt::Plugins` | Внутрипроцессная регистрация плагинов, порядок зависимостей, активация и очистка. |
| [Particles](docs/architecture/particles.md) | `FluentQt::Particles` | Анимированные фоны с частицами на CPU (`ParticleBackdrop`, без OpenGL) и управление паузой/возобновлением через `ParticlesSession`. |
| [Spatial](docs/architecture/spatial-view.md) | `FluentQt::Spatial` | Добавляет 3D-перспективу к существующим макетам `QWidget`. При ошибке запуска сеанса или создания вида приложение использует [2D-макет](examples/hello_world/README.md). |
| [QmlBridge](docs/architecture/qml-bridge.md) | `FluentQt::QmlBridge` | `QmlView` для встраивания страниц QML в окна Widgets с общими моделями приложения и темами Fluent. |

Приложения на C++ линкуют эти цели напрямую, приложения на Python используют соответствующие модули PySide6, а WebAssembly компилирует тот же код C++ для браузера. Попробуйте примеры расширений в Gallery в разделах **Plugins → Particles**, **Plugins → Spatial** (включите **Settings → 3D Gallery**) и **Plugins → QmlBridge → QmlView**, либо изучите [минимальный пример QML](examples/qml_bridge/).

## 🖼 Gallery и онлайн-документация

Используйте Gallery, чтобы просматривать компоненты, тестировать взаимодействие и изучать примеры кода на C++ и Python.

| Где попробовать | Ссылка |
|---|---|
| Браузер (WebAssembly Gallery) | [Живая Gallery](https://calvinhxx.github.io/Fluent-Qt/gallery/) · [Сайт проекта](https://calvinhxx.github.io/Fluent-Qt/ru/#gallery) |
| Онлайн-документация и предпросмотр | [Документация и API](https://calvinhxx.github.io/Fluent-Qt/ru/api/) · [Попробовать предпросмотр свойств](https://calvinhxx.github.io/Fluent-Qt/ru/api/checkbox/?playground=1) |
| Windows, macOS, Linux | [Скачать пакет Gallery](https://github.com/calvinhxx/Fluent-Qt/releases/latest) |

### Документация компонентов и предпросмотр свойств

В [онлайн-документации](https://calvinhxx.github.io/Fluent-Qt/ru/api/) на русском языке доступны руководства, справочник по API компонентов и [заметки о релизах](https://calvinhxx.github.io/Fluent-Qt/ru/api/releases/). Откройте **Предпросмотр свойств** на странице любого компонента, чтобы вживую менять свойства Qt и копировать готовый код на C++ или PySide6:

<p>
  <a href="https://calvinhxx.github.io/Fluent-Qt/ru/api/checkbox/?playground=1">
    <img src="site/assets/playground/checkbox-en-light.webp" width="760" alt="Панель свойств CheckBox с отмеченным флажком и сгенерированным кодом C++">
  </a>
</p>

Быстрый переход: [Button](https://calvinhxx.github.io/Fluent-Qt/ru/api/button/?playground=1) · [CheckBox](https://calvinhxx.github.io/Fluent-Qt/ru/api/checkbox/?playground=1) · [ComboBox](https://calvinhxx.github.io/Fluent-Qt/ru/api/combobox/?playground=1) · [DataGrid](https://calvinhxx.github.io/Fluent-Qt/ru/api/data-grid/?playground=1) · [LineChart](https://calvinhxx.github.io/Fluent-Qt/ru/api/line-chart/?playground=1) · [ParticleBackdrop](https://calvinhxx.github.io/Fluent-Qt/ru/api/particle-backdrop/?playground=1) · [SpatialView](https://calvinhxx.github.io/Fluent-Qt/ru/api/spatial-view/?playground=1) · [QmlView](https://calvinhxx.github.io/Fluent-Qt/ru/api/qml-view/?playground=1)

### Python Gallery

Установите Python-версию Gallery с [PyPI](https://pypi.org/project/FluentQt-Gallery/) (подходящая версия `FluentQt` установится автоматически):

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

## 🎨 Визуальное проектирование

- **Page Studio (Веб)**: размещайте компоненты Fluent-Qt в браузере в [Page Studio](https://calvinhxx.github.io/Fluent-Qt/studio/?lang=en), начинайте с шаблонов или раскладок с помощью ИИ, проверяйте интерактивность в реальном времени и экспортируйте файлы C++, PySide6 или Qt Designer `.ui`.

### Qt Designer

При установке всех модулей в палитре Qt Creator и `pyside6-designer` доступны 95 элементов, а при создании формы — четыре шаблона. Базовый модуль содержит 92 элемента и три шаблона. [Настройка Designer](tools/designer/README.md).

<p>
  <a href="tools/designer/README.md">
    <img src="docs/assets/readme/qt-designer.webp" width="760" alt="Qt Designer с панелью виджетов FluentQt, формой настроек и редактором свойств FluentButton">
  </a>
</p>

## 🤖 Разработка с ИИ

**ИИ-навык и CLI онбординга**: используйте навык [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md) в Codex, Claude Code или Cursor ([Пример](https://calvinhxx.github.io/Fluent-Qt/ru/#ai-build) · [Установка и использование](docs/ai/README.md)) и запускайте `python3 tools/onboarding/fluentqt` ([Инструменты онбординга](tools/onboarding/README.md)) для проверки окружения и генерации многослойных шаблонов C++ / PySide6 (`monitor`, `workbench`, `existing-qt`). При доработке примера в Gallery сначала посмотрите сохранённые правки в Live Scene, а затем проверьте скомпилированный пример на C++ ([Как это работает](docs/development/gallery-preview-workflow.md)).

```bash
python3 tools/onboarding/fluentqt create my-monitor --language cpp --starter monitor
python3 tools/onboarding/fluentqt create my-py-monitor --language pyside6 --starter monitor
```

## 🚀 Быстрый старт

Fluent-Qt нативно работает как в **Python (PySide6)**, так и в **C++17 (CMake)**.

### Python (PySide6)

Готовые пакеты wheel для Qt/PySide6 6.9.3 поддерживают CPython 3.11–3.13. Для Linux ARM64 нужны CPython 3.12–3.13 и glibc версии 2.39 или новее.

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

[руководстве по Python](bindings/pyside6/README.md) · [Hello World](bindings/pyside6/examples/hello_world/main.py)

### Настройка C++

Для интеграции на C++ требуются C++17, CMake 3.16+ и Qt Widgets 5.15+ или 6.2+. Для новых проектов используйте `FetchContent`.

#### Интеграция через `FetchContent`

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    fluentqt
    GIT_REPOSITORY https://github.com/calvinhxx/Fluent-Qt.git
    GIT_TAG v1.12.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fluentqt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Интеграция исходников

После объявления цели приложения добавьте каталог исходников Fluent-Qt и слинкуйте экспортированную цель:

```cmake
add_subdirectory(Fluent-Qt)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Интеграция установленной библиотеки

Используйте установленный пакет FluentQt с заголовочными файлами разработки и конфигурацией CMake. Подробнее см. в [руководстве по сборке и установке библиотеки](docs/development/build-workflow.md#install-the-library-or-create-a-source-package).

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

Если FluentQt находится вне системных путей поиска, передайте `-DCMAKE_PREFIX_PATH=/path/to/fluentqt` при конфигурации.

#### Минимальный пример на C++

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

Полный проект находится в [`examples/hello_world`](examples/hello_world/); также цель `fluentqt_hello_world` можно запустить прямо из IDE.

## 🛠 Сборка

### Зависимости

| Область | Зависимости |
|---|---|
| C++-библиотека FluentQt | C++17, CMake 3.16+, Qt Widgets 5.15+ или 6.2+ |
| C++ Gallery | FluentQt, Qt Network, spdlog/fmt |
| C++ WebAssembly | Qt 6.9.3 `wasm_singlethread`, Emscripten 3.1.70 |
| Тесты | FluentQt, Qt Test/Network, GTest, spdlog/fmt |
| Опциональные привязки PySide6 | Qt/PySide6/Shiboken6 6.2.4+; Python 3.10+ для сборки из исходников |

### Библиотека

```bash
cmake -S . -B build/fluentqt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt
python3 tools/dev/fluent_qt_build.py build/fluentqt --config Release --target FluentQt
cmake --install build/fluentqt --config Release \
  --component Development --prefix /path/to/install
```

Скрипт сборки выбирает число параллельных задач с учётом доступных CPU и памяти. Подробности конфигурации см. в [руководстве по сборке](docs/development/build-workflow.md).

### Дополнительные расширения

Включите модуль Plugins или конкретное расширение при конфигурации FluentQt и слинкуйте его цель CMake в вашем приложении:

| Расширение | Опция сборки | Цель CMake | Пример интеграции |
|---|---|---|---|
| Ядро Plugins | `-DFLUENT_QT_BUILD_PLUGINS=ON` | `FluentQt::Plugins` | [Минимальный пример плагина](docs/architecture/plugins.md#minimal-extension-example) |
| Plugins → Particles | `-DFLUENT_QT_BUILD_PARTICLES=ON` | `FluentQt::Particles` | [Интеграция Particles](docs/architecture/particles.md) |
| Plugins → Spatial | `-DFLUENT_QT_BUILD_SPATIAL=ON` | `FluentQt::Spatial` (автоматически включает Plugins) | [Spatial Hello World](examples/hello_world/README.md) |
| Plugins → QmlBridge | `-DFLUENT_QT_BUILD_QML_BRIDGE=ON` | `FluentQt::QmlBridge` (автоматически включает Plugins) | [Пример хоста QML](examples/qml_bridge/) |

При сборке из исходников задавайте эти опции до `add_subdirectory()` или `FetchContent_MakeAvailable()`. Соответствующие сборки Python экспортируют модули `fluentqt.plugins`, `fluentqt.spatial` и `fluentqt.qmlbridge`, а WebAssembly компилирует ту же реализацию на C++.

### Локальная сборка Gallery

Пресеты репозитория требуют CMake 3.25+ и `VCPKG_ROOT`. Настройте комплект Qt по инструкции [первичной настройки](docs/development/build-workflow.md#first-use-setup).

Список доступных на текущей машине пресетов:

```bash
cmake --list-presets
```

На Apple Silicon используйте `vcpkg-osx`:

```bash
cmake --preset vcpkg-osx
python3 tools/dev/fluent_qt_build.py \
  --preset vcpkg-osx \
  --target fluent_qt_gallery
```

На других платформах выберите соответствующий пресет:

| Платформа | Пресет |
|---|---|
| Apple Silicon Mac | `vcpkg-osx` |
| Intel Mac или Rosetta | `vcpkg-osx-x64` |
| Linux x64 | `vcpkg-linux` |
| Linux ARM64 | `vcpkg-linux-arm64` |
| Windows x64 | `vcpkg-windows` |
| Windows ARM64 | `vcpkg-windows-arm64` |

Для упаковки используйте соответствующий пресет `-release` и шаги проверки из [процесса упаковки](docs/development/packaging-workflow.md).

### WebAssembly

Инструментарий для браузера, локальная сборка, проверка и деплой на Pages описаны в [руководстве по WebAssembly](docs/development/webassembly-workflow.md).

### Пакет исходников

Создание облегчённого пакета исходников библиотеки для офлайн- или исходной интеграции:

```bash
python3 tools/dev/fluent_qt_build.py build/fluentqt --target fluent_qt_source_package
```

## 📚 Документация

Начните с [карты документации](docs/README.md) или перейдите сразу в нужный раздел:

| Задача | Точка входа |
|---|---|
| Обзор виджетов и API | [Документация и API](https://calvinhxx.github.io/Fluent-Qt/ru/api/) · [WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/ru/#gallery) |
| Создание приложения | [Qt Designer](tools/designer/README.md) · [Разработка с ИИ](docs/ai/README.md) · [Инструменты онбординга](tools/onboarding/README.md) |
| Вклад в FluentQt | [Руководства для разработчиков](docs/development/README.md) · [Архитектура](docs/architecture/README.md) · [Дизайн Fluent](docs/design-languages/README.md) |
| Упаковка и релизы | [Упаковка](docs/development/packaging-workflow.md) · [Правила релизов](docs/development/release-governance.md) · [Заметки о релизах](docs/releases/README.md) |
| Помощь и сообщения об ошибках | [Сообщество](docs/community/README.md) · [Поддержка](SUPPORT.md) · [Безопасность](SECURITY.md) |

Перед отправкой pull request ознакомьтесь с [правилами участия](CONTRIBUTING.md) и [кодексом поведения](CODE_OF_CONDUCT.md).

## 🔗 Ссылки

| Источник | Назначение |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Визуальный ориентир Fluent / Windows |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | Ориентир по поведению элементов и примерам страниц |

## Лицензия

Собственный исходный код Fluent-Qt распространяется по лицензии [MIT License](LICENSE). Включённые ресурсы и упакованные зависимости времени выполнения сохраняют лицензии своих исходных проектов; версии, происхождение, доступность исходников и подробности лицензирования перечислены в [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Товарные знаки и сторонние дизайн-референсы описаны в [TRADEMARKS.md](TRADEMARKS.md).

<p align="center">
  <a href="README.md">English</a> | <a href="README.zh-CN.md">简体中文</a> | <a href="README.zh-TW.md">繁體中文</a> | <a href="README.ja.md">日本語</a> | <a href="README.ko.md">한국어</a> | <a href="README.de.md">Deutsch</a> | <a href="README.ru.md">Русский</a> | <a href="README.es.md">Español</a> | Français
</p>

<p align="center">
  <img src="app/assets/app-icon.png" width="88" alt="Fluent-Qt logo">
</p>

<h1 align="center">Fluent-Qt</h1>

<p align="center">
  Bibliothèque native de composants Fluent UI pour Qt Widgets sous licence MIT — une seule implémentation C++17 pour C++, PySide6 et WebAssembly.
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
  <a href="https://calvinhxx.github.io/Fluent-Qt/fr/#gallery"><img src="site/assets/gallery/hero-showcase.webp" width="830" alt="Aperçu animé de Fluent-Qt : Gallery avec Mica clair/sombre, Page Studio en direct C++ WASM, export C++/PySide6/.ui et SpatialView 3D"></a>
</p>

<p align="center">
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/fr/#gallery">Tester la Web Gallery C++ en direct</a></strong>
  ·
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/studio/?lang=en">Page Studio</a></strong>
  ·
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/fr/api/checkbox/?playground=1">Aperçu des propriétés</a></strong>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/fr/api/">Docs &amp; API</a>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/fr/#top">Site du projet</a>
  ·
  <a href="https://github.com/calvinhxx/Fluent-Qt/discussions">Questions &amp; communauté</a>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/fr/#support">Soutenir le projet</a>
</p>

Fluent-Qt (FluentQt) apporte Fluent UI à Qt Widgets en C++, PySide6 et WebAssembly à partir d’une base native unique en C++17 :

- **Licence MIT pour applications commerciales et open source** — la bibliothèque C++17, les liaisons PySide6, le greffon Qt Designer et les modèles de démarrage sont tous sous licence MIT.
- **Une seule implémentation native en C++17** — 99 entrées dans la Gallery dont `DataGrid`, des [graphiques](docs/architecture/charts.md), la navigation, les overlays, les boîtes de dialogue et les fenêtres exécutent les mêmes widgets C++17 en C++ (`Qt 5.15+` / `6.2+`), `PySide6` et `WebAssembly`, et non de simples feuilles de style.
- **Conception visuelle et formulaires `.ui` partagés** — composez vos pages dans le navigateur avec **Page Studio** (export C++, PySide6 ou `.ui`) ou dans **Qt Creator** et `pyside6-designer` avec 95 composants et 4 modèles de formulaire.
- **Modèles de démarrage et Skill IA** — générez des applications structurées en couches (`monitor`, `workbench`, `existing-qt`) via `python3 tools/onboarding/fluentqt` ou créez vos interfaces avec [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md).
- **Modules d’extension optionnels** — les applications 2D classiques ne dépendent que de `Qt Widgets` ; activez les arrière-plans de particules sur CPU ([Particles](docs/architecture/particles.md)), les vues 3D en perspective ([Spatial](docs/architecture/spatial-view.md)) ou les pages QML intégrées ([QmlBridge](docs/architecture/qml-bridge.md)) uniquement selon vos besoins.

## 🧩 Modules de la bibliothèque

| Module | Cible CMake | Rôle |
|---|---|---|
| Core | `FluentQt::FluentQt` | Tous les contrôles 2D, thèmes, typographie, utilitaires de mise en page et graphiques. Les apps 2D classiques n’ont besoin que de cette cible. |
| [Plugins](docs/architecture/plugins.md) | `FluentQt::Plugins` | Enregistrement de plugins in-process, ordre des dépendances, activation et nettoyage. |
| [Particles](docs/architecture/particles.md) | `FluentQt::Particles` | Arrière-plans de particules animés rendus sur CPU (`ParticleBackdrop`, sans OpenGL) et pause/reprise via `ParticlesSession`. |
| [Spatial](docs/architecture/spatial-view.md) | `FluentQt::Spatial` | Ajoute de la perspective 3D aux mises en page `QWidget` existantes. Si l’activation de session ou la création de vue échoue, l’app fournit une [mise en page 2D](examples/hello_world/README.md). |
| [QmlBridge](docs/architecture/qml-bridge.md) | `FluentQt::QmlBridge` | `QmlView` pour héberger des pages QML dans Widgets tout en partageant les modèles applicatifs et les thèmes Fluent. |

Les applications C++ lient ces cibles directement, les applications Python utilisent les modules PySide6 correspondants et WebAssembly compile le même code C++ pour le navigateur. Testez les exemples d’extensions dans Gallery sous **Plugins → Particles**, **Plugins → Spatial** (activez **Settings → 3D Gallery**) et **Plugins → QmlBridge → QmlView**, ou consultez l’[exemple QML minimal](examples/qml_bridge/).

## 🖼 Gallery & Documentation en ligne

Utilisez Gallery pour parcourir les contrôles, tester les interactions et lire les exemples de code en C++ et Python.

| Où l’essayer | Point d’entrée |
|---|---|
| Navigateur (WebAssembly Gallery) | [Gallery en direct](https://calvinhxx.github.io/Fluent-Qt/gallery/) · [Site du projet](https://calvinhxx.github.io/Fluent-Qt/fr/#gallery) |
| Documentation & aperçu | [Guides & API](https://calvinhxx.github.io/Fluent-Qt/fr/api/) · [Essayer l’aperçu des propriétés](https://calvinhxx.github.io/Fluent-Qt/fr/api/checkbox/?playground=1) |
| Windows, macOS, Linux | [Télécharger un paquet Gallery](https://github.com/calvinhxx/Fluent-Qt/releases/latest) |

### Documentation des composants & aperçu des propriétés

La [documentation en ligne](https://calvinhxx.github.io/Fluent-Qt/fr/api/) propose en français les guides, la référence des API des composants et les [notes de version](https://calvinhxx.github.io/Fluent-Qt/fr/api/releases/). Ouvrez **Aperçu des propriétés** sur n’importe quelle page de composant pour ajuster les propriétés Qt en direct et copier le code C++ ou PySide6 généré :

<p>
  <a href="https://calvinhxx.github.io/Fluent-Qt/fr/api/checkbox/?playground=1">
    <img src="site/assets/playground/checkbox-en-light.webp" width="760" alt="Panneau de propriétés CheckBox affichant une case cochée et son code C++">
  </a>
</p>

Accès rapide : [Button](https://calvinhxx.github.io/Fluent-Qt/fr/api/button/?playground=1) · [CheckBox](https://calvinhxx.github.io/Fluent-Qt/fr/api/checkbox/?playground=1) · [ComboBox](https://calvinhxx.github.io/Fluent-Qt/fr/api/combobox/?playground=1) · [DataGrid](https://calvinhxx.github.io/Fluent-Qt/fr/api/data-grid/?playground=1) · [LineChart](https://calvinhxx.github.io/Fluent-Qt/fr/api/line-chart/?playground=1) · [ParticleBackdrop](https://calvinhxx.github.io/Fluent-Qt/fr/api/particle-backdrop/?playground=1) · [SpatialView](https://calvinhxx.github.io/Fluent-Qt/fr/api/spatial-view/?playground=1) · [QmlView](https://calvinhxx.github.io/Fluent-Qt/fr/api/qml-view/?playground=1)

### Python Gallery

Installez la version Python de Gallery depuis [PyPI](https://pypi.org/project/FluentQt-Gallery/) (la version correspondante de `FluentQt` est installée automatiquement) :

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

## 🎨 Conception visuelle

- **Page Studio (Web)** : disposez des contrôles Fluent-Qt dans le navigateur avec [Page Studio](https://calvinhxx.github.io/Fluent-Qt/studio/?lang=en), démarrez depuis des modèles ou des mises en page assistées par IA, testez les interactions en direct et exportez du code C++, PySide6 ou des fichiers `.ui` Qt Designer.

### Qt Designer

Avec tous les modules, Qt Creator et `pyside6-designer` proposent 95 entrées dans la boîte à widgets et quatre modèles à la création d’un formulaire. Le module de base comprend 92 entrées et trois modèles. [Configurer Designer](tools/designer/README.md).

<p>
  <a href="tools/designer/README.md">
    <img src="docs/assets/readme/qt-designer.webp" width="760" alt="Qt Designer affichant la boîte à widgets FluentQt, le formulaire et l’éditeur de propriétés FluentButton">
  </a>
</p>

## 🤖 Construire avec l’IA

**Skill IA et CLI d’intégration** : utilisez le skill [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md) dans Codex, Claude Code ou Cursor ([Exemple](https://calvinhxx.github.io/Fluent-Qt/fr/#ai-build) · [Installation et utilisation](docs/ai/README.md)) et lancez `python3 tools/onboarding/fluentqt` ([Outils d’intégration](tools/onboarding/README.md)) pour vérifier l’environnement et générer des projets de départ en couches C++ ou PySide6 (`monitor`, `workbench`, `existing-qt`). Lorsque vous ajustez un exemple Gallery, prévisualisez vos modifications enregistrées dans Live Scene avant de vérifier l’exemple C++ compilé ([Fonctionnement](docs/development/gallery-preview-workflow.md)).

```bash
python3 tools/onboarding/fluentqt create my-monitor --language cpp --starter monitor
python3 tools/onboarding/fluentqt create my-py-monitor --language pyside6 --starter monitor
```

## 🚀 Démarrage rapide

Fluent-Qt fonctionne nativement en **Python (PySide6)** comme en **C++17 (CMake)**.

### Python (PySide6)

Les wheels Qt/PySide6 6.9.3 prennent en charge CPython 3.11–3.13. Sous Linux ARM64, il faut CPython 3.12–3.13 et glibc 2.39 ou ultérieure.

```bash
python -m pip install FluentQt
```

`main.py` :

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

[guide Python](bindings/pyside6/README.md) · [Hello World](bindings/pyside6/examples/hello_world/main.py)

### Configuration C++

L’intégration C++ requiert C++17, CMake 3.16+ et Qt Widgets 5.15+ ou 6.2+. Utilisez `FetchContent` pour les nouveaux projets.

#### Intégration avec `FetchContent`

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    fluentqt
    GIT_REPOSITORY https://github.com/calvinhxx/Fluent-Qt.git
    GIT_TAG v1.12.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fluentqt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Intégration par sources

Après avoir défini votre cible applicative, ajoutez le répertoire source de Fluent-Qt et liez la cible exportée :

```cmake
add_subdirectory(Fluent-Qt)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Intégration d’une bibliothèque installée

Utilisez une installation de FluentQt incluant les en-têtes de développement et les fichiers de paquet CMake. Consultez le [guide de compilation et d’installation](docs/development/build-workflow.md#install-the-library-or-create-a-source-package).

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

Si FluentQt se trouve hors des chemins de recherche système, configurez avec `-DCMAKE_PREFIX_PATH=/path/to/fluentqt`.

Si vous gérez les dépendances de votre application Qt 6 avec vcpkg, vous pouvez installer la bibliothèque de base de Widgets à l’aide du [port overlay de Fluent-Qt](docs/development/build-workflow.md#install-the-widgets-library-with-vcpkg).

Avec Conan 2, vous pouvez [compiler la bibliothèque de base de Widgets dans le cache local](docs/development/build-workflow.md#build-the-widgets-library-with-conan-2).

#### Exemple minimal C++

`main.cpp` :

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

Voir [`examples/hello_world`](examples/hello_world/) pour le projet complet, ou lancez directement la cible `fluentqt_hello_world` depuis votre IDE.

## 🛠 Compilation

### Dépendances

| Périmètre | Dépendances |
|---|---|
| Bibliothèque C++ FluentQt | C++17, CMake 3.16+, Qt Widgets 5.15+ ou 6.2+ |
| C++ Gallery | FluentQt, Qt Network, spdlog/fmt |
| C++ WebAssembly | Qt 6.9.3 `wasm_singlethread`, Emscripten 3.1.70 |
| Tests | FluentQt, Qt Test/Network, GTest, spdlog/fmt |
| Bindings PySide6 optionnels | Qt/PySide6/Shiboken6 6.2.4+; Python 3.10+ pour compiler depuis les sources |

### Bibliothèque

```bash
cmake -S . -B build/fluentqt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt
python3 tools/dev/fluent_qt_build.py build/fluentqt --config Release --target FluentQt
cmake --install build/fluentqt --config Release \
  --component Development --prefix /path/to/install
```

Le script de compilation choisit le parallélisme en fonction du CPU et de la mémoire disponibles. Consultez le [workflow de compilation](docs/development/build-workflow.md) pour la configuration.

### Extensions optionnelles

Activez le module Plugins ou une extension précise lors de la configuration de FluentQt, puis liez sa cible CMake dans votre application :

| Périmètre d’extension | Option de compilation | Cible CMake | Exemple d’intégration |
|---|---|---|---|
| Cœur Plugins | `-DFLUENT_QT_BUILD_PLUGINS=ON` | `FluentQt::Plugins` | [Exemple de plugin minimal](docs/architecture/plugins.md#minimal-extension-example) |
| Plugins → Particles | `-DFLUENT_QT_BUILD_PARTICLES=ON` | `FluentQt::Particles` | [Intégration Particles](docs/architecture/particles.md) |
| Plugins → Spatial | `-DFLUENT_QT_BUILD_SPATIAL=ON` | `FluentQt::Spatial` (inclut Plugins) | [Spatial Hello World](examples/hello_world/README.md) |
| Plugins → QmlBridge | `-DFLUENT_QT_BUILD_QML_BRIDGE=ON` | `FluentQt::QmlBridge` (inclut Plugins) | [Exemple hôte QML](examples/qml_bridge/) |

Lors d’une compilation depuis les sources, définissez ces options avant `add_subdirectory()` ou `FetchContent_MakeAvailable()`. Les builds Python correspondants exposent `fluentqt.plugins`, `fluentqt.spatial` et `fluentqt.qmlbridge`, et WebAssembly compile l’implémentation C++ partagée.

### Compiler Gallery localement

Les presets du dépôt nécessitent CMake 3.25+ et `VCPKG_ROOT`. Configurez votre kit Qt via la [configuration initiale](docs/development/build-workflow.md#first-use-setup).

Lister les presets disponibles sur votre machine :

```bash
cmake --list-presets
```

Sur Apple Silicon, utilisez `vcpkg-osx` :

```bash
cmake --preset vcpkg-osx
python3 tools/dev/fluent_qt_build.py \
  --preset vcpkg-osx \
  --target fluent_qt_gallery
```

Utilisez le preset correspondant sur les autres plateformes :

| Plateforme | Preset |
|---|---|
| Mac Apple Silicon | `vcpkg-osx` |
| Mac Intel ou Rosetta | `vcpkg-osx-x64` |
| Linux x64 | `vcpkg-linux` |
| Linux ARM64 | `vcpkg-linux-arm64` |
| Windows x64 | `vcpkg-windows` |
| Windows ARM64 | `vcpkg-windows-arm64` |

Pour l’empaquetage, utilisez le preset `-release` correspondant et suivez les vérifications du [workflow d’empaquetage](docs/development/packaging-workflow.md).

### WebAssembly

Consultez le [workflow WebAssembly](docs/development/webassembly-workflow.md) pour la chaîne d’outils navigateur, les builds locaux, la validation et le déploiement Pages.

### Paquet source

Créez le paquet source allégé de la bibliothèque pour une intégration hors ligne ou par sources :

```bash
python3 tools/dev/fluent_qt_build.py build/fluentqt --target fluent_qt_source_package
```

## 📚 Documentation

Commencez par la [carte de documentation](docs/README.md) ou allez directement à ce dont vous avez besoin :

| Objectif | Point d’entrée |
|---|---|
| Parcourir contrôles & API | [Docs & API](https://calvinhxx.github.io/Fluent-Qt/fr/api/) · [WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/fr/#gallery) |
| Créer une application | [Qt Designer](tools/designer/README.md) · [Développement assisté par IA](docs/ai/README.md) · [Outils d’intégration](tools/onboarding/README.md) |
| Contribuer à FluentQt | [Guides de développement](docs/development/README.md) · [Architecture](docs/architecture/README.md) · [Design Fluent](docs/design-languages/README.md) |
| Empaqueter ou publier | [Empaquetage](docs/development/packaging-workflow.md) · [Gouvernance des versions](docs/development/release-governance.md) · [Notes de version](docs/releases/README.md) |
| Obtenir de l’aide ou signaler un bug | [Communauté](docs/community/README.md) · [Support](SUPPORT.md) · [Sécurité](SECURITY.md) |

Avant d’ouvrir une pull request, lisez [Contributing](CONTRIBUTING.md) et le [Code de conduite](CODE_OF_CONDUCT.md).

## 🔗 Références

| Source | Rôle |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Référence visuelle Fluent / Windows |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | Référence de comportement des contrôles et pages d’exemple |

## Licence

Le code source propre à Fluent-Qt est publié sous [Licence MIT](LICENSE). Les ressources incluses et dépendances d’exécution empaquetées conservent leurs licences d’origine ; versions, provenance, disponibilité des sources et détails figurent dans [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Les marques déposées et références de design tierces sont couvertes dans [TRADEMARKS.md](TRADEMARKS.md).

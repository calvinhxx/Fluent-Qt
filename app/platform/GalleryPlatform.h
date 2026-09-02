#ifndef FLUENTQT_GALLERY_PLATFORM_H
#define FLUENTQT_GALLERY_PLATFORM_H

#include <functional>

#include <QString>
#include <QUrl>

class QObject;
class QSettings;
class QRect;
class QWidget;

namespace fluent::gallery::platform {

enum class HostTheme { System, Light, Dark, HighContrast };

using HostThemeChangedHandler = std::function<void(HostTheme)>;

/**
 * @brief Capabilities supplied by the selected Gallery runtime adapter.
 * zh_CN: 由所选 Gallery 运行时适配器提供的能力集合。
 */
struct Capabilities {
    bool persistsWindowPlacement = true;
    bool exposesCloseBehavior = true;
    bool checksForUpdates = true;
    bool editsThemeFiles = true;
    bool prewarmsRoutes = true;
    bool usesClientSideTitleBar = false;
    bool hostControlsTheme = false;

    // Browser component pages can be cross-language documentation surfaces
    // even though their live previews run as native C++ WebAssembly. Installed
    // C++ and PySide6 Galleries keep their source presentation runtime-native.
    // zh_CN: 浏览器组件页可以作为跨语言文档入口，但实时预览仍运行原生 C++
    // WebAssembly；安装版 C++ 与 PySide6 Gallery 保持各自运行时原生展示。
    bool showsBilingualDocumentation = false;

    // Browser visitors enter through an explanatory website and should land
    // directly in the catalog; desktop packages keep the first-run tour.
    // zh_CN: 浏览器访客已由官网引导，应直接进入目录；桌面安装包保留首启引导。
    bool showsIntroTour = true;

    // Zero keeps every visited page resident. Browser adapters can cap the
    // cache so a long single-threaded session does not retain every live demo.
    // zh_CN: 0 表示保留所有访问过的页面；浏览器适配层可限制缓存，避免单线程长会话
    // 常驻全部 live demo。
    int maxResidentRoutes = 0;

    QString applicationName;
    QString windowTitle;
    QString distributionSectionTitle;
    QString distributionTitle;
    QString distributionDescription;
    QString runtimeLabel;
    QString distributionActionText;
    QUrl distributionActionUrl;
};

const Capabilities& capabilities();
bool persistenceAvailable();
QSettings createSettings();
HostTheme hostTheme();
void setHostThemeChangedHandler(QObject* context, HostThemeChangedHandler handler);
void showTopLevelWindow(QWidget* window, const QRect& normalGeometry, bool maximized = false);
int runApplication(int argc, char** argv);

} // namespace fluent::gallery::platform

#endif // FLUENTQT_GALLERY_PLATFORM_H

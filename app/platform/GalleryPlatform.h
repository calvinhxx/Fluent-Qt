#ifndef FLUENTQT_GALLERY_PLATFORM_H
#define FLUENTQT_GALLERY_PLATFORM_H

#include <QString>
#include <QUrl>

class QSettings;
class QRect;
class QWidget;

namespace fluent::gallery::platform {

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
void showTopLevelWindow(QWidget* window,
                        const QRect& normalGeometry,
                        bool maximized = false);
int runApplication(int argc, char** argv);

} // namespace fluent::gallery::platform

#endif // FLUENTQT_GALLERY_PLATFORM_H

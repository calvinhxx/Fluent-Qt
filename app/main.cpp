#include <algorithm>

#include <QApplication>
#include <QFontDatabase>
#include <QRect>
#include <QScreen>
#include <QSize>

#include "view/shell/AppIcon.h"
#include "view/shell/GalleryWindow.h"
#include "view/shell/GalleryWindowMetrics.h"
#include "viewmodel/GallerySettings.h"
#include "utils/Log.h"

static void initializeFluentQtResources()
{
    Q_INIT_RESOURCE(resources);
    QFontDatabase::addApplicationFont(QStringLiteral(":/res/Segoe Fluent Icons.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/res/SegoeUI-VF.ttf"));
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WinUI 3 Gallery"));
    QApplication::setOrganizationName(QStringLiteral("Fluent-QT"));

    initializeFluentQtResources();
    fluent::gallery::GallerySettings::instance();

    // App logging policy: persist to the platform log file by default at Info
    // level, and route Qt's own qWarning/qDebug through the same logger. The
    // component library itself stays silent; SPDLOG_LEVEL still overrides the
    // level for ad-hoc debugging (the explicit file path wins over SPDLOG_FILE).
    // zh_CN: 应用日志策略：默认以 Info 级别持久化到平台日志文件，并把 Qt 自身的
    // qWarning/qDebug 汇入同一日志器。组件库本身保持静默；SPDLOG_LEVEL 环境变量
    // 仍可临时覆盖级别（显式文件路径优先于 SPDLOG_FILE）。
    utils::logging::InitializationOptions loggingOptions;
    loggingOptions.defaultLevel = utils::logging::Level::Info;
    loggingOptions.installQtMessageHandler = true;
    loggingOptions.logFilePath = utils::logging::defaultLogFilePath();
    utils::logging::initialize(loggingOptions);
    LOG_INFO(QStringLiteral("GalleryApp startup appName=%1 organization=%2 logFile=%3")
                 .arg(QApplication::applicationName(), QApplication::organizationName(),
                      loggingOptions.logFilePath));
    app.setWindowIcon(fluent::gallery::appicon::icon());

    fluent::gallery::GalleryWindow window;

    // Pick an initial size that fits the screen (shrinking on small displays, never below the
    // window's minimum), then place it centered vertically and centered horizontally within
    // the area that remains after reserving a strip on the left. macOS does not subtract the
    // Stage Manager thumbnail strip from availableGeometry(), so a plain center would tuck the
    // window under it; reserving a left inset keeps that strip (or any left-edge panel) clear.
    // Placement happens after show() because the custom window chrome re-applies platform
    // window flags in showEvent, which re-places the native window and would otherwise nudge a
    // pre-show position off.
    // zh_CN: 先选一个适配屏幕的初始尺寸（小屏收缩、但不小于最小尺寸），再垂直居中、并在"预留
    // 左侧条之后剩余的区域"里水平居中。macOS 不会把 Stage Manager 的缩略图条从
    // availableGeometry() 里扣掉，所以纯居中会把窗口压到它下面；预留一条左边距即可让开那条
    // 带子（或任何贴左边缘的面板）。放置放在 show() 之后：自定义窗口边框会在 showEvent 中重新
    // 应用平台窗口标志、重新摆放原生窗口，否则 show 前设置的位置会被挪偏。
    QSize initialSize(fluent::gallery::metrics::AppWindow::InitialWidth,
                      fluent::gallery::metrics::AppWindow::InitialHeight);
    if (QScreen* screen = QApplication::primaryScreen())
        initialSize = initialSize.boundedTo(screen->availableGeometry().size());
    window.resize(initialSize.expandedTo(window.minimumSize()));
    window.show();

    if (QScreen* screen = window.screen()) {
        const QRect available = screen->availableGeometry();
        const QRect frame = window.frameGeometry();
        const int horizontalSlack = std::max(0, available.width() - frame.width());
        const int leftReserve = std::min(fluent::gallery::metrics::AppWindow::LeftPanelReserve,
                                         horizontalSlack);
        const int x = available.x() + leftReserve + (horizontalSlack - leftReserve) / 2;
        const int y = available.y() + std::max(0, available.height() - frame.height()) / 2;
        window.move(x, y);
    }

    return app.exec();
}

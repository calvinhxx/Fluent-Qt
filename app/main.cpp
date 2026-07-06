#include <algorithm>

#include <FluentQt/FluentQt.h>

#include <QApplication>
#include <QRect>
#include <QScreen>
#include <QSize>

#include "view/shell/AppIcon.h"
#include "view/shell/GalleryApplicationController.h"
#include "view/shell/GalleryWindow.h"
#include "view/shell/GalleryWindowMetrics.h"
#include "viewmodel/GallerySettings.h"
#include "utils/Log.h"

#ifndef FLUENT_QT_GALLERY_VERSION
#define FLUENT_QT_GALLERY_VERSION "0.0.0"
#endif

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Fluent-Qt Gallery"));
    QApplication::setOrganizationName(QStringLiteral("Fluent-Qt"));
    QApplication::setApplicationVersion(QString::fromLatin1(FLUENT_QT_GALLERY_VERSION));
    app.setQuitOnLastWindowClosed(false);

    fluent::initializeResources();
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
    fluent::gallery::GalleryApplicationController applicationController(&window, &app);

    // Pick an initial size that fits the screen (shrinking on small displays, never below the
    // window's minimum), then place it centered vertically and centered horizontally within
    // the area that remains after reserving a strip on the left. macOS does not subtract the
    // Stage Manager thumbnail strip from availableGeometry(), so a plain center would tuck the
    // window under it; reserving a left inset keeps that strip (or any left-edge panel) clear.
    // zh_CN: 先选一个适配屏幕的初始尺寸（小屏收缩、但不小于最小尺寸），再垂直居中、并在"预留
    // 左侧条之后剩余的区域"里水平居中。macOS 不会把 Stage Manager 的缩略图条从
    // availableGeometry() 里扣掉，所以纯居中会把窗口压到它下面；预留一条左边距即可让开那条带子。
    QSize initialSize(fluent::gallery::metrics::AppWindow::InitialWidth,
                      fluent::gallery::metrics::AppWindow::InitialHeight);
    if (QScreen* screen = QApplication::primaryScreen())
        initialSize = initialSize.boundedTo(screen->availableGeometry().size());
    window.resize(initialSize.expandedTo(window.minimumSize()));

    auto centerWindow = [&window]() {
        QScreen* screen = window.screen() ? window.screen() : QApplication::primaryScreen();
        if (!screen)
            return;
        const QRect available = screen->availableGeometry();
        const QRect frame = window.frameGeometry();
        const int horizontalSlack = std::max(0, available.width() - frame.width());
        const int leftReserve = std::min(fluent::gallery::metrics::AppWindow::LeftPanelReserve,
                                         horizontalSlack);
        const int x = available.x() + leftReserve + (horizontalSlack - leftReserve) / 2;
        const int y = available.y() + std::max(0, available.height() - frame.height()) / 2;
        window.move(x, y);
    };

    // Center BEFORE show() so the window first appears in place — no visible slide from the OS
    // default (staggered top-left) position to center. On Windows the custom chrome re-applies
    // window flags in showEvent with SWP_NOMOVE, so the pre-show position is preserved. We re-assert
    // once more after show() as a cross-platform safety net (e.g. macOS, where the first realized
    // frame can differ); by then it is at most a sub-pixel frame-margin nudge, not a visible jump.
    // zh_CN: 在 show() 之前居中，使窗口首次出现即在位——不再从系统默认（左上角错位）位置滑到中央。Windows 上
    // 自定义边框在 showEvent 里以 SWP_NOMOVE 重应用窗口标志，故 show 前的位置会被保留；show 后再居中一次作为
    // 跨平台兜底（如 macOS 首次实现的 frame 可能不同），此时至多是亚像素级 frame 边距微调，而非可见跳动。
    centerWindow();
    window.show();
    centerWindow();

    return app.exec();
}

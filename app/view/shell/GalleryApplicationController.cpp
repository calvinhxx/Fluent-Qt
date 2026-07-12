#include "GalleryApplicationController.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTimer>

#include "compatibility/WindowChromeCompat.h"
#include "components/dialogs_flyouts/ContentDialog.h"
#include "utils/Log.h"
#include "view/shell/AppIcon.h"
#include "view/shell/GalleryWindow.h"
#include "view/support/GalleryCloseBehaviorPrompt.h"
#include "view/support/GalleryCloseBehaviorUi.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {

namespace {

// A zero-duration timer can run before Qt's Unix event dispatcher flushes the
// hide/unmap request to the compositor. A short real delay lets the event loop
// block once, flush the platform connection, and only then remap the surface.
// zh_CN: 0ms 定时器可能早于 Qt Unix 事件分发器把 hide/unmap 刷给 compositor。
// 使用短暂真实延迟，让事件循环至少阻塞并刷新一次平台连接后再重新映射 surface。
constexpr int LinuxMinimizedRemapDelayMs = 100;

} // namespace

GalleryApplicationController::GalleryApplicationController(GalleryWindow* window,
                                                           QObject* parent)
    : QObject(parent)
    , m_window(window)
{
    setObjectName(QStringLiteral("galleryApplicationController"));
    if (m_window)
        m_window->installEventFilter(this);
#ifdef Q_OS_MACOS
    if (qApp)
        qApp->installEventFilter(this);
#endif
    setupStatusItem();
}

GalleryApplicationController::~GalleryApplicationController()
{
    if (m_statusIcon)
        m_statusIcon->setContextMenu(nullptr);
    delete m_statusMenu;
}

void GalleryApplicationController::setupStatusItem()
{
    if (!m_window)
        return;

    m_statusAreaAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    m_statusIcon = new QSystemTrayIcon(appicon::icon(), this);
    m_statusIcon->setObjectName(QStringLiteral("galleryStatusAreaIcon"));
    m_statusIcon->setToolTip(QStringLiteral("Fluent-Qt Gallery"));

    m_statusMenu = new QMenu;
    m_statusMenu->setObjectName(QStringLiteral("galleryStatusAreaMenu"));
    auto* openAction = m_statusMenu->addAction(QStringLiteral("Open Fluent-Qt Gallery"));
    auto* settingsAction = m_statusMenu->addAction(QStringLiteral("Settings"));
    m_statusMenu->addSeparator();
    auto* quitAction = m_statusMenu->addAction(QStringLiteral("Quit Fluent-Qt Gallery"));
    openAction->setObjectName(QStringLiteral("galleryStatusOpenAction"));
    settingsAction->setObjectName(QStringLiteral("galleryStatusSettingsAction"));
    quitAction->setObjectName(QStringLiteral("galleryStatusQuitAction"));

    connect(openAction, &QAction::triggered, this, [this]() { restoreWindow(); });
    connect(settingsAction, &QAction::triggered, this, [this]() { openSettings(); });
    connect(quitAction, &QAction::triggered, this, [this]() { requestQuit(); });
    connect(m_statusIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick) {
                    restoreWindow();
                }
            });

    m_statusIcon->setContextMenu(m_statusMenu);
    m_statusIcon->show();
    LOG_INFO(QStringLiteral("GalleryApplicationController statusArea initialized available=%1 area=%2")
                 .arg(m_statusAreaAvailable)
                 .arg(closebehaviorui::statusAreaName()));
}

bool GalleryApplicationController::eventFilter(QObject* watched, QEvent* event)
{
    if (!event)
        return QObject::eventFilter(watched, event);

    if (watched == m_window && event->type() == QEvent::Close && !m_exitRequested) {
        if (qApp && qApp->isSavingSession())
            return QObject::eventFilter(watched, event);
        static_cast<QCloseEvent*>(event)->ignore();
        if (!m_closeRequestScheduled) {
            m_closeRequestScheduled = true;
            QTimer::singleShot(0, this, [this]() { handleCloseRequested(); });
        }
        return true;
    }

#ifdef Q_OS_MACOS
    if (watched == qApp && event->type() == QEvent::ApplicationActivate
        && m_window && !m_window->isVisible() && !m_exitRequested && !m_closePromptOpen) {
        QTimer::singleShot(0, this, [this]() { restoreWindow(); });
    }
#endif

    return QObject::eventFilter(watched, event);
}

void GalleryApplicationController::handleCloseRequested()
{
    m_closeRequestScheduled = false;
    if (!m_window || m_exitRequested || m_closePromptOpen)
        return;

    if (!GallerySettings::instance().closeBehaviorConfirmed()) {
        showCloseBehaviorDialog();
        return;
    }

    applyConfiguredCloseBehavior();
}

void GalleryApplicationController::showCloseBehaviorDialog()
{
    if (!m_window || m_closePromptOpen)
        return;

    m_closePromptOpen = true;
    auto& settings = GallerySettings::instance();
    auto* content = new CloseBehaviorPromptContent(settings.closeBehavior());
    auto* dialog = new fluent::dialogs_flyouts::ContentDialog(m_window);
    dialog->setObjectName(QStringLiteral("galleryCloseBehaviorDialog"));
    dialog->setWindowTitle(QStringLiteral("Close behavior"));
    dialog->setTitle(QStringLiteral("Close behavior"));
    dialog->setContent(content);
    dialog->setPrimaryButtonText(QStringLiteral("Save"));
    dialog->setCloseButtonText(QStringLiteral("Cancel"));
    dialog->setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Primary);
    // Compact command bar: this bespoke prompt is short, so the standard 68px bar
    // looks bottom-heavy. zh_CN: 紧凑命令栏：此定制弹窗内容很短，标准 68px 命令栏显得头重脚轻。
    dialog->setButtonBarHeight(52);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->resize(closeBehaviorDialogSize());

    const bool restoreChromeInteractive = m_window->isChromeInteractive();
    m_window->setChromeInteractive(false);

    connect(dialog, &QDialog::finished, this,
            [this, dialog, content, restoreChromeInteractive](int result) {
                m_closePromptOpen = false;
                if (m_window)
                    m_window->setChromeInteractive(restoreChromeInteractive);
                if (result == fluent::dialogs_flyouts::ContentDialog::ResultPrimary) {
                    auto& currentSettings = GallerySettings::instance();
                    currentSettings.setCloseBehavior(content->selectedBehavior());
                    currentSettings.setCloseBehaviorConfirmed(true);
                    applyConfiguredCloseBehavior();
                }
                dialog->deleteLater();
            });
    dialog->open();
}

void GalleryApplicationController::captureRestoreState()
{
    if (!m_window)
        return;
    m_restoreState = m_window->windowState();
    m_restoreState &= ~Qt::WindowMinimized;
    m_restoreState &= ~Qt::WindowActive;
}

void GalleryApplicationController::applyConfiguredCloseBehavior()
{
    if (!m_window)
        return;

    const auto behavior = GallerySettings::instance().closeBehavior();
    LOG_INFO(QStringLiteral("GalleryApplicationController applyCloseBehavior behavior=%1")
                 .arg(static_cast<int>(behavior)));
    switch (behavior) {
    case GallerySettings::CloseBehavior::Minimize:
        captureRestoreState();
        m_window->showMinimized();
        break;
    case GallerySettings::CloseBehavior::Tray:
        captureRestoreState();
        if (m_statusAreaAvailable && m_statusIcon) {
            m_window->hide();
        } else {
            LOG_WARN(QStringLiteral("GalleryApplicationController status area unavailable; minimizing instead"));
            m_window->showMinimized();
        }
        break;
    case GallerySettings::CloseBehavior::Quit:
        requestQuit();
        break;
    }
}

void GalleryApplicationController::restoreWindow()
{
    if (!m_window || m_exitRequested)
        return;

    const bool wasHidden = !m_window->isVisible();
    const bool wasMinimized = m_window->windowState().testFlag(Qt::WindowMinimized);
    const bool needsRestore = wasHidden || wasMinimized;
    const std::uint64_t generation = ++m_restoreGeneration;
    LOG_INFO(QStringLiteral("GalleryApplicationController restore requested generation=%1 hidden=%2 minimized=%3 platform=%4")
                 .arg(static_cast<qulonglong>(generation))
                 .arg(wasHidden)
                 .arg(wasMinimized)
                 .arg(QGuiApplication::platformName()));
    if (needsRestore) {
        // A tray-hidden window uses the state captured immediately before hiding.
        // A manually minimized window uses its current native state so an older
        // tray restore state cannot unexpectedly maximize it.
        // zh_CN: 托盘隐藏窗口使用隐藏前保存的状态；手动最小化窗口使用当前原生状态，
        // 避免较早的托盘恢复状态意外把窗口最大化。
        Qt::WindowStates targetState = wasHidden ? m_restoreState : m_window->windowState();
        targetState &= ~Qt::WindowMinimized;
        targetState &= ~Qt::WindowActive;

        // Linux compositors may keep an already-minimized surface iconified even
        // after Qt has cleared WindowMinimized. Unmap it first, then remap it on
        // the next event-loop turn so Wayland/X11 must observe two distinct state
        // transitions. A synchronous hide()+showNormal() can be coalesced and is
        // therefore not a restore operation.
        // zh_CN: Linux compositor 可能在 Qt 清除 WindowMinimized 后仍保持 surface 最小化。
        // 先取消映射，再在下一轮事件循环重新映射，确保 Wayland/X11 观察到两个独立状态切换；
        // 同步 hide()+showNormal() 可能被合并，不能作为可靠的恢复操作。
        const bool forceLinuxRemap = needsRestore
            && compatibility::WindowChromeCompat::currentPlatform()
                == compatibility::WindowChromeCompat::Platform::Linux;
        if (forceLinuxRemap) {
            m_window->hide();
            // Reassert the client-side frame while the native surface is
            // unmapped. Doing this after show can expose a second window-manager
            // caption on Qt 5/X11 until the next state transition.
            // zh_CN: 在原生表面未映射时重新声明客户端边框；Qt 5/X11 若在显示后
            // 再修改窗口标志，可能一直残留第二条系统标题栏直到下次状态切换。
            m_window->prepareForNativeRestore();
            const int remapDelayMs = wasMinimized ? LinuxMinimizedRemapDelayMs : 0;
            LOG_INFO(QStringLiteral("GalleryApplicationController Linux surface unmapped generation=%1 remapDelayMs=%2")
                         .arg(static_cast<qulonglong>(generation))
                         .arg(remapDelayMs));
            QTimer::singleShot(remapDelayMs, this,
                               [this, generation, targetState, wasHidden, wasMinimized]() {
                showRestoredWindow(generation, targetState, wasHidden, wasMinimized);
            });
        } else {
            showRestoredWindow(generation, targetState, wasHidden, wasMinimized);
        }
    }
    if (!needsRestore)
        completeWindowRestore(generation, /*refreshNativeFrame*/ false);
    QTimer::singleShot(250, this, [this, generation]() {
        if (generation == m_restoreGeneration && m_window && !m_window->isActiveWindow()) {
            LOG_DEBUG(QStringLiteral("GalleryApplicationController foreground activation deferred to platform attention"));
            QApplication::alert(m_window, 3000);
        }
    });
}

void GalleryApplicationController::showRestoredWindow(std::uint64_t generation,
                                                       Qt::WindowStates targetState,
                                                       bool wasHidden,
                                                       bool wasMinimized)
{
    if (generation != m_restoreGeneration || !m_window || m_exitRequested)
        return;

    if (targetState.testFlag(Qt::WindowFullScreen))
        m_window->showFullScreen();
    else if (targetState.testFlag(Qt::WindowMaximized))
        m_window->showMaximized();
    else if (wasHidden && !wasMinimized)
        // A tray-hidden normal window does not need a window-state transition.
        // Qt 5/X11 showNormal() can re-negotiate decorations and leave a detached
        // system caption behind.
        // zh_CN: 托盘隐藏的普通窗口无需切换窗口状态；Qt 5/X11 在此调用 showNormal()
        // 可能重新协商系统装饰并残留一条脱离窗口的标题栏。
        m_window->show();
    else
        m_window->showNormal();

    LOG_INFO(QStringLiteral("GalleryApplicationController surface remapped generation=%1 visible=%2 state=%3")
                 .arg(static_cast<qulonglong>(generation))
                 .arg(m_window->isVisible())
                 .arg(static_cast<int>(m_window->windowState())));

    // Window-state transitions are asynchronous. Refresh and request activation
    // only after the remapped surface has been submitted to the compositor; the
    // delayed retry covers slower virtual-machine compositors.
    // zh_CN: 窗口状态切换是异步的；重新映射的 surface 提交给 compositor 后再刷新并请求激活，
    // 延迟重试用于较慢的虚拟机 compositor。
    QTimer::singleShot(0, this, [this, generation]() {
        completeWindowRestore(generation, /*refreshNativeFrame*/ true);
    });
    QTimer::singleShot(80, this, [this, generation]() {
        completeWindowRestore(generation, /*refreshNativeFrame*/ false);
    });
}

void GalleryApplicationController::completeWindowRestore(std::uint64_t generation,
                                                         bool refreshNativeFrame)
{
    if (generation != m_restoreGeneration || !m_window || m_exitRequested)
        return;

    if (refreshNativeFrame)
        m_window->reapplySystemBackdrop();
    m_window->requestForegroundActivation();
}

void GalleryApplicationController::openSettings()
{
    restoreWindow();
    if (m_window)
        m_window->selectRoute(QStringLiteral("settings"));
}

void GalleryApplicationController::requestQuit()
{
    if (m_exitRequested)
        return;
    m_exitRequested = true;
    if (m_statusIcon)
        m_statusIcon->hide();
    LOG_INFO(QStringLiteral("GalleryApplicationController quit requested"));
    QCoreApplication::quit();
}

} // namespace fluent::gallery

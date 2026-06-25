#include "GalleryApplicationController.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTimer>

#include "components/dialogs_flyouts/ContentDialog.h"
#include "utils/Log.h"
#include "view/shell/AppIcon.h"
#include "view/shell/GalleryWindow.h"
#include "view/support/GalleryCloseBehaviorPrompt.h"
#include "view/support/GalleryCloseBehaviorUi.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {

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
    m_statusIcon->setToolTip(QStringLiteral("WinUI 3 Gallery"));

    m_statusMenu = new QMenu;
    m_statusMenu->setObjectName(QStringLiteral("galleryStatusAreaMenu"));
    auto* openAction = m_statusMenu->addAction(QStringLiteral("Open WinUI 3 Gallery"));
    auto* settingsAction = m_statusMenu->addAction(QStringLiteral("Settings"));
    m_statusMenu->addSeparator();
    auto* quitAction = m_statusMenu->addAction(QStringLiteral("Quit WinUI 3 Gallery"));
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

    if (m_restoreState.testFlag(Qt::WindowFullScreen))
        m_window->showFullScreen();
    else if (m_restoreState.testFlag(Qt::WindowMaximized))
        m_window->showMaximized();
    else
        m_window->showNormal();
    m_window->raise();
    m_window->activateWindow();
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

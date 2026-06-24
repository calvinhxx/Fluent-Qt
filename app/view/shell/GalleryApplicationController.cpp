#include "GalleryApplicationController.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QSizePolicy>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

#include "components/basicinput/RadioButton.h"
#include "components/dialogs_flyouts/ContentDialog.h"
#include "components/foundation/FluentElement.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "utils/Log.h"
#include "view/shell/AppIcon.h"
#include "view/shell/GalleryWindow.h"
#include "view/support/GalleryCloseBehaviorUi.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {
namespace {

class PromptIcon final : public QWidget, public FluentElement {
public:
    explicit PromptIcon(QWidget* parent)
        : QWidget(parent)
    {
        setFixedSize(28, 28);
    }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
        iconFont.setPixelSize(18);
        painter.setFont(iconFont);
        painter.setPen(themeColors().accentDefault);
        painter.drawText(rect(), Qt::AlignCenter, Typography::Icons::Power);
    }
};

class CloseBehaviorPromptContent final : public QWidget, public FluentElement {
public:
    explicit CloseBehaviorPromptContent(GallerySettings::CloseBehavior current,
                                        QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("galleryCloseBehaviorPromptContent"));
        setMinimumSize(328, 0);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(10);

        // Add the intro row as a sub-layout (not a wrapper QWidget): a QBoxLayout propagates a
        // word-wrapped child's height-for-width to its parent layout, whereas a nested plain QWidget
        // does not — so the 2-line explanation actually reserves two lines instead of overlapping the
        // first option. zh_CN: intro 行用子布局而非包裹 QWidget 加入:QBoxLayout 会把自动换行子项的
        // height-for-width 上传给父布局,而嵌套的普通 QWidget 不会——这样两行说明会真正占两行高,而非压到第一项上。
        auto* introLayout = new QHBoxLayout();
        introLayout->setContentsMargins(0, 0, 0, 0);
        introLayout->setSpacing(8);
        introLayout->addWidget(new PromptIcon(this), 0, Qt::AlignTop);

        auto* explanation = new fluent::textfields::Label(
            QStringLiteral("Choose what happens when you close the window. "
                           "You can change this later in Settings. 🙂"),
            this);
        explanation->setObjectName(QStringLiteral("galleryCloseBehaviorPromptDescription"));
        explanation->setFluentTypography(Typography::FontRole::Caption);
        explanation->setWordWrap(true);
        QSizePolicy explanationPolicy = explanation->sizePolicy();
        explanationPolicy.setHeightForWidth(true);
        explanation->setSizePolicy(explanationPolicy);
        m_secondaryLabels.append(explanation);
        introLayout->addWidget(explanation, 1);
        root->addLayout(introLayout);

        m_group = new QButtonGroup(this);
        addChoice(root,
                  GallerySettings::CloseBehavior::Minimize,
                  QStringLiteral("Minimize window"),
                  QStringLiteral("Keep it in the taskbar or Dock."));
        addChoice(root,
                  GallerySettings::CloseBehavior::Tray,
                  closebehaviorui::keepRunningChoice(),
                  closebehaviorui::keepRunningDescription());
        addChoice(root,
                  GallerySettings::CloseBehavior::Quit,
                  QStringLiteral("Quit the app"),
                  QStringLiteral("Exit completely."));

        if (auto* selected = m_group->button(static_cast<int>(current)))
            selected->setChecked(true);
        onThemeUpdated();
    }

    GallerySettings::CloseBehavior selectedBehavior() const
    {
        const int id = m_group->checkedId();
        return id >= 0 ? static_cast<GallerySettings::CloseBehavior>(id)
                       : GallerySettings::CloseBehavior::Tray;
    }

    void onThemeUpdated() override
    {
        const QColor secondary = themeColors().textSecondary;
        for (auto* label : m_secondaryLabels) {
            QPalette labelPalette = label->palette();
            labelPalette.setColor(QPalette::WindowText, secondary);
            label->setPalette(labelPalette);
        }
        update();
    }

private:
    void addChoice(QVBoxLayout* root,
                   GallerySettings::CloseBehavior behavior,
                   const QString& title,
                   const QString& description)
    {
        auto* choice = new QWidget(this);
        auto* choiceLayout = new QVBoxLayout(choice);
        choiceLayout->setContentsMargins(0, 0, 0, 0);
        choiceLayout->setSpacing(2);

        auto* radio = new fluent::basicinput::RadioButton(title, choice);
        radio->setObjectName(QStringLiteral("galleryCloseBehaviorChoice%1")
                                 .arg(static_cast<int>(behavior)));
        radio->setAccessibleName(title);
        m_group->addButton(radio, static_cast<int>(behavior));

        auto* detail = new fluent::textfields::Label(description, choice);
        detail->setFluentTypography(Typography::FontRole::Caption);
        detail->setContentsMargins(28, 0, 0, 0);
        m_secondaryLabels.append(detail);

        choiceLayout->addWidget(radio);
        choiceLayout->addWidget(detail);
        root->addWidget(choice);
    }

    QButtonGroup* m_group = nullptr;
    QVector<fluent::textfields::Label*> m_secondaryLabels;
};

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
    dialog->resize(452, 356);

    connect(dialog, &QDialog::finished, this,
            [this, dialog, content](int result) {
                m_closePromptOpen = false;
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

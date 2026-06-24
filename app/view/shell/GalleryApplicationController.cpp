#include "GalleryApplicationController.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
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

constexpr int kCloseBehaviorContentWidth = 360;
constexpr QSize kCloseBehaviorDialogSize(440, 352);

class CloseBehaviorChoiceRow final : public QWidget, public FluentElement {
public:
    CloseBehaviorChoiceRow(GallerySettings::CloseBehavior behavior,
                           const QString& glyph,
                           const QString& title,
                           const QString& description,
                           QWidget* parent)
        : QWidget(parent)
        , m_glyph(glyph)
    {
        setObjectName(QStringLiteral("galleryCloseBehaviorRow%1")
                          .arg(static_cast<int>(behavior)));
        setAccessibleName(title);
        setAccessibleDescription(description);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(52);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 6, 12, 6);
        layout->setSpacing(10);

        m_iconSlot = new QWidget(this);
        m_iconSlot->setFixedSize(20, 20);
        m_iconSlot->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* textColumn = new QWidget(this);
        textColumn->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* textLayout = new QVBoxLayout(textColumn);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        m_title = new fluent::textfields::Label(title, textColumn);
        m_title->setFluentTypography(Typography::FontRole::BodyStrong);
        m_description = new fluent::textfields::Label(description, textColumn);
        m_description->setFluentTypography(Typography::FontRole::Caption);
        m_description->setWordWrap(false);
        textLayout->addWidget(m_title);
        textLayout->addWidget(m_description);

        m_radio = new fluent::basicinput::RadioButton(this);
        m_radio->setObjectName(QStringLiteral("galleryCloseBehaviorChoice%1")
                                   .arg(static_cast<int>(behavior)));
        m_radio->setAccessibleName(title);
        m_radio->setAccessibleDescription(description);
        m_radio->setCursor(Qt::PointingHandCursor);
        m_radio->setFixedSize(20, 20);

        layout->addWidget(m_iconSlot, 0, Qt::AlignVCenter);
        layout->addWidget(textColumn, 1, Qt::AlignVCenter);
        layout->addWidget(m_radio, 0, Qt::AlignVCenter);

        connect(m_radio, &QAbstractButton::toggled, this, [this]() { update(); });
        onThemeUpdated();
    }

    fluent::basicinput::RadioButton* radioButton() const { return m_radio; }

    void onThemeUpdated() override
    {
        if (m_title)
            m_title->onThemeUpdated();
        if (m_description) {
            m_description->onThemeUpdated();
            QPalette palette = m_description->palette();
            palette.setColor(QPalette::WindowText, themeColors().textSecondary);
            m_description->setPalette(palette);
        }
        if (m_radio)
            m_radio->onThemeUpdated();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto colors = themeColors();
        const bool selected = m_radio && m_radio->isChecked();
        const QRectF rowRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        QColor fill = Qt::transparent;
        if (selected) {
            fill = colors.accentDefault;
            fill.setAlpha(currentTheme() == Dark ? 28 : 16);
        } else if (m_pressed) {
            fill = colors.subtleTertiary;
        } else if (m_hovered) {
            fill = colors.subtleSecondary;
        }

        painter.setBrush(fill);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rowRect, 6, 6);

        const QRectF iconRect = QRectF(m_iconSlot->geometry()).adjusted(0.5, 0.5, -0.5, -0.5);
        QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
        iconFont.setPixelSize(15);
        painter.setFont(iconFont);
        painter.setPen(selected ? colors.textAccentPrimary : colors.textSecondary);
        painter.drawText(iconRect, Qt::AlignCenter, m_glyph);
    }

    void enterEvent(FluentEnterEvent* event) override
    {
        QWidget::enterEvent(event);
        m_hovered = true;
        update();
    }

    void leaveEvent(QEvent* event) override
    {
        QWidget::leaveEvent(event);
        m_hovered = false;
        m_pressed = false;
        update();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const bool activate = event->button() == Qt::LeftButton
            && m_pressed && rect().contains(event->pos());
        m_pressed = false;
        update();
        if (activate && m_radio) {
            m_radio->click();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    QString m_glyph;
    QWidget* m_iconSlot = nullptr;
    fluent::textfields::Label* m_title = nullptr;
    fluent::textfields::Label* m_description = nullptr;
    fluent::basicinput::RadioButton* m_radio = nullptr;
    bool m_hovered = false;
    bool m_pressed = false;
};

class CloseBehaviorPromptContent final : public QWidget, public FluentElement {
public:
    explicit CloseBehaviorPromptContent(GallerySettings::CloseBehavior current,
                                        QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("galleryCloseBehaviorPromptContent"));
        setFixedWidth(kCloseBehaviorContentWidth);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(4);

        m_group = new QButtonGroup(this);
        addChoice(root,
                  GallerySettings::CloseBehavior::Minimize,
                  Typography::Icons::ChromeMinimize,
                  QStringLiteral("Minimize window"),
                  closebehaviorui::minimizeDescription());
        addChoice(root,
                  GallerySettings::CloseBehavior::Tray,
                  Typography::Icons::Hide,
                  closebehaviorui::keepRunningChoice(),
                  closebehaviorui::keepRunningDescription());
        addChoice(root,
                  GallerySettings::CloseBehavior::Quit,
                  Typography::Icons::Power,
                  QStringLiteral("Quit the app"),
                  QStringLiteral("Stop the Gallery completely."));

        root->activate();
        const int contentHeight = root->hasHeightForWidth()
            ? root->heightForWidth(kCloseBehaviorContentWidth)
            : root->sizeHint().height();
        setFixedHeight(contentHeight);

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
        for (auto* row : m_rows)
            row->onThemeUpdated();
        update();
    }

private:
    void addChoice(QVBoxLayout* root,
                   GallerySettings::CloseBehavior behavior,
                   const QString& glyph,
                   const QString& title,
                   const QString& description)
    {
        auto* row = new CloseBehaviorChoiceRow(behavior, glyph, title, description, this);
        m_group->addButton(row->radioButton(), static_cast<int>(behavior));
        m_rows.append(row);
        root->addWidget(row);
    }

    QButtonGroup* m_group = nullptr;
    QVector<CloseBehaviorChoiceRow*> m_rows;
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
    dialog->setTitle(QStringLiteral("Close the Gallery?"));
    dialog->setContent(content);
    dialog->setPrimaryButtonText(QStringLiteral("Save"));
    dialog->setCloseButtonText(QStringLiteral("Cancel"));
    dialog->setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Primary);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->resize(kCloseBehaviorDialogSize);

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

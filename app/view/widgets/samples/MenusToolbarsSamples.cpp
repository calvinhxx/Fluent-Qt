#include "MenusToolbarsSamples.h"

#include <QAction>
#include <QActionGroup>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <functional>
#include <utility>

#include "components/basicinput/Button.h"
#include "components/basicinput/DropDownButton.h"
#include "components/foundation/FontIcon.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandBarFlyout.h"
#include "components/menus_toolbars/Menu.h"
#include "components/menus_toolbars/MenuBar.h"
#include "components/textfields/EditingCommandRouter.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "view/support/GalleryEditingCommands.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::basicinput::DropDownButton;
using fluent::FontIcon;
using fluent::menus_toolbars::CommandBar;
using fluent::menus_toolbars::CommandBarFlyout;
using fluent::menus_toolbars::FluentMenu;
using fluent::menus_toolbars::FluentMenuBar;
using fluent::textfields::EditingCommandRouter;
using fluent::textfields::Label;
using fluent::textfields::LineEdit;
using samples::horizontalGroup;
using samples::makeSample;

class MenusToolbarsSampleSurface : public QWidget, public fluent::FluentElement {
public:
    explicit MenusToolbarsSampleSurface(QWidget* parent = nullptr, int spacing = 12)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 16);
        layout->setSpacing(spacing);
        layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    }

    void setActionGlyph(QAction* action, const QString& glyph)
    {
        if (!action || glyph.isEmpty())
            return;

        for (ActionGlyph& entry : m_actionGlyphs) {
            if (entry.action == action) {
                entry.glyph = glyph;
                updateActionIcon(entry);
                return;
            }
        }

        m_actionGlyphs.append({action, glyph});
        updateActionIcon(m_actionGlyphs.last());
    }

    void onThemeUpdated() override
    {
        for (const ActionGlyph& entry : m_actionGlyphs)
            updateActionIcon(entry);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(themeColors().strokeCard);
        painter.setBrush(themeColors().bgCanvas);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1),
                                themeRadius().overlay,
                                themeRadius().overlay);
    }

private:
    struct ActionGlyph {
        QPointer<QAction> action;
        QString glyph;
    };

    QPixmap glyphPixmapForColor(const QString& glyph,
                                int logicalSize,
                                const QColor& color) const
    {
        const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
        const int physicalSize =
            qMax(1, qCeil(logicalSize * dpr));
        QPixmap pixmap(physicalSize, physicalSize);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setPen(color);
        Typography::Icons::paintGlyph(
            painter,
            QRectF(0, 0, logicalSize, logicalSize),
            glyph,
            logicalSize,
            Qt::AlignCenter);
        return pixmap;
    }

    void updateActionIcon(const ActionGlyph& entry) const
    {
        if (!entry.action)
            return;

        const auto& colors = themeColorsRef();
        QIcon icon;
        for (const int size : {16, 20, 24}) {
            const QPixmap normal =
                glyphPixmapForColor(
                    entry.glyph, size, colors.textPrimary);
            const QPixmap disabled =
                glyphPixmapForColor(
                    entry.glyph, size, colors.textDisabled);
            icon.addPixmap(normal, QIcon::Normal, QIcon::Off);
            icon.addPixmap(normal, QIcon::Active, QIcon::Off);
            icon.addPixmap(normal, QIcon::Selected, QIcon::Off);
            icon.addPixmap(normal, QIcon::Normal, QIcon::On);
            icon.addPixmap(normal, QIcon::Active, QIcon::On);
            icon.addPixmap(disabled, QIcon::Disabled, QIcon::Off);
            icon.addPixmap(disabled, QIcon::Disabled, QIcon::On);
        }
        entry.action->setIcon(icon);
    }

    QVector<ActionGlyph> m_actionGlyphs;
};

class CommandPreviewPanel : public QWidget, public fluent::FluentElement {
public:
    explicit CommandPreviewPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(themeColorsRef().strokeCard);
        painter.setBrush(themeColorsRef().bgLayer);
        painter.drawRoundedRect(
            rect().adjusted(0, 0, -1, -1),
            themeRadius().overlay,
            themeRadius().overlay);
    }
};

class ContextMediaTile final
    : public QWidget,
      public fluent::FluentElement {
public:
    using InvokeHandler =
        std::function<void(const QPoint& localPosition,
                           bool standard)>;

    explicit ContextMediaTile(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_photo(samples::gradientPixmap(
              QSize(276, 160),
              QColor(0x35, 0x5C, 0x7D),
              QColor(0xA7, 0xC6, 0xD9),
              QStringLiteral("Northern ridge")))
    {
        setObjectName(
            QStringLiteral("Gallery.CommandBarFlyout.ContextTile"));
        setFixedSize(560, 184);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setAccessibleName(
            QStringLiteral("Northern ridge photo"));
        setAccessibleDescription(
            QStringLiteral(
                "Click for quick commands without moving focus, or right-click for the expanded context menu."));
    }

    void setInvokeHandler(InvokeHandler handler)
    {
        m_invokeHandler = std::move(handler);
    }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        const auto& colors = themeColorsRef();
        QColor background = colors.bgLayer;
        if (m_hovered)
            background = colors.subtleSecondary;

        painter.setPen(
            hasFocus() && m_keyboardFocusVisible
                ? colors.accentDefault
                : colors.strokeCard);
        painter.setBrush(background);
        painter.drawRoundedRect(
            rect().adjusted(0, 0, -1, -1),
            themeRadius().overlay,
            themeRadius().overlay);

        const QRect photoRect(12, 12, 276, 160);
        painter.drawPixmap(photoRect, m_photo);

        const int textLeft = 312;
        const int textWidth = width() - textLeft - 20;
        painter.setPen(colors.textPrimary);
        painter.setFont(
            themeFont(Typography::FontRole::BodyStrong)
                .toQFont());
        painter.drawText(
            QRect(textLeft, 28, textWidth, 24),
            Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Northern ridge"));

        painter.setPen(colors.textSecondary);
        painter.setFont(
            themeFont(Typography::FontRole::Caption)
                .toQFont());
        painter.drawText(
            QRect(textLeft, 54, textWidth, 20),
            Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Photo · 4.8 MB"));

        painter.setFont(
            themeFont(Typography::FontRole::Body)
                .toQFont());
        painter.drawText(
            QRect(textLeft, 94, textWidth, 48),
            Qt::AlignLeft | Qt::AlignTop
                | Qt::TextWordWrap,
            QStringLiteral(
                "Click: quick commands\n"
                "Right-click: full context menu"));
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
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event
            && event->button() == Qt::LeftButton
            && rect().contains(event->pos())) {
            m_keyboardFocusVisible = false;
            setFocus(Qt::MouseFocusReason);
            invoke(rect().center(), false);
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        if (!event)
            return;
        const bool keyboardInvocation =
            event->reason() == QContextMenuEvent::Keyboard;
        setFocus(
            keyboardInvocation
                ? Qt::OtherFocusReason
                : Qt::MouseFocusReason);
        m_keyboardFocusVisible = keyboardInvocation;
        update();
        invoke(event->pos(), true);
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event
            && (event->key() == Qt::Key_Return
                || event->key() == Qt::Key_Enter
                || event->key() == Qt::Key_Space)) {
            m_keyboardFocusVisible = true;
            update();
            invoke(rect().center(), true);
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void focusInEvent(QFocusEvent* event) override
    {
        m_keyboardFocusVisible =
            event && event->reason() != Qt::MouseFocusReason;
        QWidget::focusInEvent(event);
        update();
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        QWidget::focusOutEvent(event);
        update();
    }

private:
    void invoke(const QPoint& localPosition, bool standard)
    {
        if (m_invokeHandler)
            m_invokeHandler(localPosition, standard);
    }

    QPixmap m_photo;
    InvokeHandler m_invokeHandler;
    bool m_hovered = false;
    bool m_keyboardFocusVisible = false;
};

MenusToolbarsSampleSurface* sampleSurface(
    QWidget* parent,
    int spacing = 12)
{
    return new MenusToolbarsSampleSurface(parent, spacing);
}

QBoxLayout* boxLayout(QWidget* widget)
{
    return qobject_cast<QBoxLayout*>(widget->layout());
}

Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    label->setWordWrap(true);
    label->setTextColorRole(Label::TextColorRole::Primary);  // QSS-proof on the styled preview surface
    return label;
}

Label* makePreviewLabel(
    QWidget* parent,
    const QString& text,
    Typography::FontRole role,
    Label::TextColorRole colorRole)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(role);
    label->setTextColorRole(colorRole);
    return label;
}

Label* makeHintLabel(QWidget* parent, const QString& text)
{
    auto* label = makePreviewLabel(
        parent,
        text,
        Typography::FontRole::Caption,
        Label::TextColorRole::Secondary);
    label->setWordWrap(true);
    return label;
}

Button* sampleButton(QWidget* parent, const QString& text)
{
    auto* button = new Button(text, parent);
    button->setFluentSize(Button::Small);
    button->setMinimumWidth(76);
    return button;
}

QString displayActionText(QString text)
{
    const int tabIndex = text.indexOf(QLatin1Char('\t'));
    if (tabIndex >= 0)
        text.truncate(tabIndex);
    text.remove(QLatin1Char('&'));
    return text;
}

QAction* addStatusAction(FluentMenu* menu,
                         Label* status,
                         const QString& text,
                         const QKeySequence& shortcut = QKeySequence())
{
    QAction* action = menu->addAction(text);
    if (!shortcut.isEmpty())
        action->setShortcut(shortcut);
    QObject::connect(action, &QAction::triggered, status, [status, text]() {
        status->setText(QStringLiteral("Clicked: %1").arg(displayActionText(text)));
    });
    return action;
}

void configureMenuBar(FluentMenuBar* menuBar)
{
    menuBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    menuBar->setMinimumWidth(340);
}

QVector<GallerySample> menuSamples()
{
    return {
        makeSample(QStringLiteral("menu-command-shortcuts"),
                   QStringLiteral("Commands and shortcuts"),
                   QStringLiteral("Menu commands can expose native shortcuts and update app state when triggered."),
                   QStringLiteral("auto* button = new DropDownButton(\"File\", this);\n"
                                  "auto* status = new Label(\"Clicked: (none)\", this);\n"
                                  "auto* menu = new FluentMenu(QString(), button);\n"
                                  "\n"
                                  "auto* newAction = menu->addAction(\"New\");\n"
                                  "newAction->setShortcut(QKeySequence::New);\n"
                                  "connect(newAction, &QAction::triggered, status,\n"
                                  "        [status] { status->setText(\"Clicked: New\"); });\n"
                                  "\n"
                                  "menu->addAction(\"Open...\")->setShortcut(QKeySequence::Open);\n"
                                  "menu->addAction(\"Save\")->setShortcut(QKeySequence::Save);\n"
                                  "menu->addSeparator();\n"
                                  "auto* disabled = menu->addAction(\"Publish\");\n"
                                  "disabled->setEnabled(false);\n"
                                  "button->setMenu(menu);"),
                   [](QWidget* parent) {
                       auto* surface = sampleSurface(parent);
                       auto* status = makeStatusLabel(surface, QStringLiteral("Clicked: (none)"));
                       auto* button = new DropDownButton(QStringLiteral("File"), surface);
                       button->setMinimumWidth(120);

                       auto* menu = new FluentMenu(QString(), button);
                       addStatusAction(menu, status, QStringLiteral("New"), QKeySequence::New);
                       addStatusAction(menu, status, QStringLiteral("Open..."), QKeySequence::Open);
                       addStatusAction(menu, status, QStringLiteral("Save"), QKeySequence::Save);
                       menu->addSeparator();
                       auto* publish = menu->addAction(QStringLiteral("Publish"));
                       publish->setEnabled(false);
                       addStatusAction(menu, status, QStringLiteral("Close"));
                       button->setMenu(menu);
                       boxLayout(surface)->addWidget(button, 0, Qt::AlignLeft);
                       boxLayout(surface)->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("menu-cascading-selection"),
                   QStringLiteral("Submenus and checked items"),
                   QStringLiteral("Nested menus and exclusive check items keep related choices in one command surface."),
                   QStringLiteral("auto* menu = new FluentMenu(QString(), this);\n"
                                  "auto* sortMenu = new FluentMenu(\"Sort by\", menu);\n"
                                  "sortMenu->addAction(\"Name\");\n"
                                  "sortMenu->addAction(\"Date modified\");\n"
                                  "menu->addMenu(sortMenu);\n"
                                  "\n"
                                  "auto* viewGroup = new QActionGroup(menu);\n"
                                  "viewGroup->setExclusive(true);\n"
                                  "auto* compact = menu->addAction(\"Compact list\");\n"
                                  "compact->setCheckable(true);\n"
                                  "viewGroup->addAction(compact);\n"
                                  "auto* comfortable = menu->addAction(\"Comfortable list\");\n"
                                  "comfortable->setCheckable(true);\n"
                                  "comfortable->setChecked(true);\n"
                                  "viewGroup->addAction(comfortable);\n"
                                  "\n"
                                  "auto* hidden = menu->addAction(\"Show hidden files\");\n"
                                  "hidden->setCheckable(true);"),
                   [](QWidget* parent) {
                       auto* surface = sampleSurface(parent);
                       auto* status = makeStatusLabel(surface, QStringLiteral("View: Comfortable list"));
                       auto* button = new DropDownButton(QStringLiteral("View options"), surface);
                       button->setMinimumWidth(150);

                       auto* menu = new FluentMenu(QString(), button);

                       auto* sortMenu = new FluentMenu(QStringLiteral("Sort by"), menu);
                       addStatusAction(sortMenu, status, QStringLiteral("Name"));
                       addStatusAction(sortMenu, status, QStringLiteral("Date modified"));
                       addStatusAction(sortMenu, status, QStringLiteral("Type"));
                       menu->addMenu(sortMenu);
                       menu->addSeparator();

                       auto* viewGroup = new QActionGroup(menu);
                       viewGroup->setExclusive(true);
                       auto addViewMode = [menu, viewGroup, status](const QString& text, bool checked) {
                           QAction* action = menu->addAction(text);
                           action->setCheckable(true);
                           action->setChecked(checked);
                           viewGroup->addAction(action);
                           QObject::connect(action, &QAction::triggered, status, [status, text]() {
                               status->setText(QStringLiteral("View: %1").arg(text));
                           });
                           return action;
                       };
                       addViewMode(QStringLiteral("Compact list"), false);
                       addViewMode(QStringLiteral("Comfortable list"), true);
                       addViewMode(QStringLiteral("Large icons"), false);

                       auto* hidden = menu->addAction(QStringLiteral("Show hidden files"));
                       hidden->setCheckable(true);
                       QObject::connect(hidden, &QAction::triggered, status, [status, hidden]() {
                           status->setText(hidden->isChecked()
                                               ? QStringLiteral("Hidden files: shown")
                                               : QStringLiteral("Hidden files: hidden"));
                       });

                       button->setMenu(menu);
                       boxLayout(surface)->addWidget(button, 0, Qt::AlignLeft);
                       boxLayout(surface)->addWidget(status);
                       return surface;
                   })
    };
}

QVector<GallerySample> menuBarSamples()
{
    return {
        makeSample(QStringLiteral("menu-bar-hosted-surface"),
                   QStringLiteral("Hosted menu bar"),
                   QStringLiteral("A MenuBar can blend into a card or custom title bar by hiding its own canvas."),
                   QStringLiteral("auto* menuBar = new FluentMenuBar(this);\n"
                                  "auto* status = new Label(\"Clicked: (none)\", this);\n"
                                  "menuBar->setBackgroundVisible(false);\n"
                                  "\n"
                                  "auto* fileMenu = new FluentMenu(\"File\", menuBar);\n"
                                  "auto* newAction = fileMenu->addAction(\"New\");\n"
                                  "connect(newAction, &QAction::triggered, status,\n"
                                  "        [status] { status->setText(\"Clicked: New\"); });\n"
                                  "fileMenu->addAction(\"Open...\");\n"
                                  "menuBar->addMenu(fileMenu);\n"
                                  "\n"
                                  "auto* editMenu = new FluentMenu(\"Edit\", menuBar);\n"
                                  "editMenu->addAction(\"Undo\");\n"
                                  "menuBar->addMenu(editMenu);"),
                   [](QWidget* parent) {
                       auto* surface = sampleSurface(parent);
                       auto* status = makeStatusLabel(surface, QStringLiteral("Clicked: (none)"));
                       auto* menuBar = new FluentMenuBar(surface);
                       configureMenuBar(menuBar);
                       menuBar->setBackgroundVisible(false);

                       auto* fileMenu = new FluentMenu(QStringLiteral("File"), menuBar);
                       addStatusAction(fileMenu, status, QStringLiteral("New"));
                       addStatusAction(fileMenu, status, QStringLiteral("Open..."));
                       addStatusAction(fileMenu, status, QStringLiteral("Save"));
                       menuBar->addMenu(fileMenu);

                       auto* editMenu = new FluentMenu(QStringLiteral("Edit"), menuBar);
                       addStatusAction(editMenu, status, QStringLiteral("Undo"), QKeySequence::Undo);
                       addStatusAction(editMenu, status, QStringLiteral("Cut"), QKeySequence::Cut);
                       addStatusAction(editMenu, status, QStringLiteral("Copy"), QKeySequence::Copy);
                       addStatusAction(editMenu, status, QStringLiteral("Paste"), QKeySequence::Paste);
                       menuBar->addMenu(editMenu);

                       auto* helpMenu = new FluentMenu(QStringLiteral("Help"), menuBar);
                       addStatusAction(helpMenu, status, QStringLiteral("About"));
                       menuBar->addMenu(helpMenu);

                       boxLayout(surface)->addWidget(menuBar);
                       boxLayout(surface)->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("menu-bar-access-keys"),
                   QStringLiteral("Access keys and actions"),
                   QStringLiteral("Top-level menu items support access keys, keyboard focus, and regular QAction commands."),
                   QStringLiteral("auto* menuBar = new FluentMenuBar(this);\n"
                                  "auto* focusButton = new Button(\"Focus\", this);\n"
                                  "auto* status = new Label(\"Command: (none)\", this);\n"
                                  "connect(focusButton, &Button::clicked, menuBar,\n"
                                  "        [menuBar] { menuBar->setFocus(Qt::OtherFocusReason); });\n"
                                  "\n"
                                  "auto* fileMenu = new FluentMenu(\"&File\", menuBar);\n"
                                  "fileMenu->menuAction()->setProperty(\"accessKey\", \"F\");\n"
                                  "fileMenu->addAction(\"Save\")->setShortcut(QKeySequence::Save);\n"
                                  "menuBar->addMenu(fileMenu);\n"
                                  "\n"
                                  "auto* runAction = new QAction(\"Run\", menuBar);\n"
                                  "connect(runAction, &QAction::triggered, status,\n"
                                  "        [status] { status->setText(\"Command: Run\"); });\n"
                                  "menuBar->addAction(runAction);"),
                   [](QWidget* parent) {
                       auto* surface = sampleSurface(parent);
                       auto* status = makeStatusLabel(surface, QStringLiteral("Command: (none)"));
                       auto* row = horizontalGroup(surface, 8);

                       auto* focusButton = sampleButton(row, QStringLiteral("Focus"));
                       auto* menuBar = new FluentMenuBar(surface);
                       configureMenuBar(menuBar);
                       menuBar->setMinimumWidth(390);

                       auto* fileMenu = new FluentMenu(QStringLiteral("&File"), menuBar);
                       fileMenu->menuAction()->setProperty("accessKey", QStringLiteral("F"));
                       addStatusAction(fileMenu, status, QStringLiteral("New"), QKeySequence::New);
                       addStatusAction(fileMenu, status, QStringLiteral("Save"), QKeySequence::Save);
                       menuBar->addMenu(fileMenu);

                       auto* viewMenu = new FluentMenu(QStringLiteral("&View"), menuBar);
                       viewMenu->menuAction()->setProperty("accessKey", QStringLiteral("V"));
                       addStatusAction(viewMenu, status, QStringLiteral("Zoom in"));
                       addStatusAction(viewMenu, status, QStringLiteral("Zoom out"));
                       menuBar->addMenu(viewMenu);

                       auto* runAction = new QAction(QStringLiteral("Run"), menuBar);
                       QObject::connect(runAction, &QAction::triggered, status, [status]() {
                           status->setText(QStringLiteral("Command: Run"));
                       });
                       menuBar->addAction(runAction);

                       auto* disabledAction = new QAction(QStringLiteral("Deploy"), menuBar);
                       disabledAction->setEnabled(false);
                       menuBar->addAction(disabledAction);

                       QObject::connect(focusButton, &Button::clicked, menuBar, [menuBar, status]() {
                           menuBar->setFocus(Qt::OtherFocusReason);
                           status->setText(QStringLiteral("Command: MenuBar focused"));
                       });

                       boxLayout(row)->addWidget(focusButton, 0, Qt::AlignVCenter);
                       boxLayout(row)->addWidget(menuBar, 0, Qt::AlignVCenter);
                       boxLayout(surface)->addWidget(row);
                       boxLayout(surface)->addWidget(status);
                       return surface;
                   })
    };
}

QVector<GallerySample> commandBarSamples()
{
    return {
        makeSample(
            QStringLiteral("command-bar-responsive-overflow"),
            QStringLiteral("A responsive document command strip"),
            QStringLiteral("Try priority-aware overflow, icon or right-side labels, primary and secondary sections, separators, and an optional command-bar background."),
            QStringLiteral(
                "auto* barHost = new QWidget(this);\n"
                "barHost->setFixedWidth(536);\n"
                "auto* barLayout = new QHBoxLayout(barHost);\n"
                "barLayout->setContentsMargins(0, 0, 0, 0);\n"
                "auto* bar = new CommandBar(barHost);\n"
                "bar->setAccessibleName(\"Document commands\");\n"
                "bar->setLabelPosition(\n"
                "    CommandBar::LabelPosition::Right);\n"
                "bar->setBackgroundVisible(false);\n"
                "barLayout->addWidget(bar);\n"
                "\n"
                "auto* addAction = new QAction(\n"
                "    QIcon(\":/icons/add.svg\"), \"Add\", this);\n"
                "auto* editAction = new QAction(\n"
                "    QIcon(\":/icons/edit.svg\"), \"Edit\", this);\n"
                "editAction->setPriority(QAction::HighPriority);\n"
                "auto* shareAction = new QAction(\n"
                "    QIcon(\":/icons/share.svg\"), \"Share\", this);\n"
                "auto* separator = new QAction(this);\n"
                "separator->setSeparator(true);\n"
                "auto* syncAction = new QAction(\n"
                "    QIcon(\":/icons/sync.svg\"), \"Sync\", this);\n"
                "syncAction->setPriority(QAction::LowPriority);\n"
                "auto* pinAction = new QAction(\n"
                "    QIcon(\":/icons/pin.svg\"), \"Pin\", this);\n"
                "pinAction->setCheckable(true);\n"
                "\n"
                "bar->addPrimaryAction(addAction);\n"
                "bar->addPrimaryAction(editAction);\n"
                "bar->addPrimaryAction(shareAction);\n"
                "bar->addPrimaryAction(separator);\n"
                "bar->addPrimaryAction(syncAction);\n"
                "bar->addPrimaryAction(pinAction);\n"
                "bar->addSecondaryAction(new QAction(\n"
                "    QIcon(\":/icons/settings.svg\"), \"Settings\", this));\n"
                "bar->addSecondaryAction(new QAction(\n"
                "    QIcon(\":/icons/help.svg\"), \"Help\", this));\n"
                "\n"
                "auto* compactButton =\n"
                "    new Button(\"Compact view\", this);\n"
                "auto* labelsButton =\n"
                "    new Button(\"Labels: Right\", this);\n"
                "auto* backgroundButton =\n"
                "    new Button(\"Show background\", this);\n"
                "connect(compactButton, &Button::clicked, barHost,\n"
                "        [barHost] {\n"
                "            barHost->setFixedWidth(\n"
                "                barHost->width() > 300 ? 288 : 536);\n"
                "        });\n"
                "connect(labelsButton, &Button::clicked, bar, [=] {\n"
                "    const bool showLabels =\n"
                "        bar->labelPosition()\n"
                "        == CommandBar::LabelPosition::Collapsed;\n"
                "    bar->setLabelPosition(\n"
                "        showLabels\n"
                "            ? CommandBar::LabelPosition::Right\n"
                "            : CommandBar::LabelPosition::Collapsed);\n"
                "});\n"
                "connect(backgroundButton, &Button::clicked, bar, [=] {\n"
                "    bar->setBackgroundVisible(\n"
                "        !bar->backgroundVisible());\n"
                "});"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* status = makeHintLabel(
                    surface,
                    QStringLiteral(
                        "Full width · all primary commands are visible"));

                auto* panel = new CommandPreviewPanel(surface);
                panel->setFixedSize(560, 132);
                auto* panelLayout = new QVBoxLayout(panel);
                panelLayout->setContentsMargins(12, 10, 12, 12);
                panelLayout->setSpacing(8);

                auto* barHost = new QWidget(panel);
                barHost->setObjectName(
                    QStringLiteral("Gallery.CommandBar.Host"));
                barHost->setFixedSize(536, 40);
                auto* hostLayout = new QHBoxLayout(barHost);
                hostLayout->setContentsMargins(0, 0, 0, 0);
                hostLayout->setSpacing(0);

                auto* bar = new CommandBar(barHost);
                bar->setObjectName(
                    QStringLiteral("Gallery.CommandBar.Responsive"));
                bar->setAccessibleName(
                    QStringLiteral("Document commands"));
                bar->setLabelPosition(
                    CommandBar::LabelPosition::Right);
                bar->setBackgroundVisible(false);
                hostLayout->addWidget(bar);

                const auto makeAction =
                    [surface, status](
                        const QString& text,
                        const QString& glyph) {
                        auto* action = new QAction(text, surface);
                        surface->setActionGlyph(action, glyph);
                        QObject::connect(
                            action,
                            &QAction::triggered,
                            status,
                            [status, text]() {
                                status->setText(
                                    QStringLiteral("Command: %1")
                                        .arg(text));
                            });
                        return action;
                    };
                QAction* addAction = makeAction(
                    QStringLiteral("Add"),
                    Typography::Icons::Add);
                QAction* editAction = makeAction(
                    QStringLiteral("Edit"),
                    Typography::Icons::Edit);
                editAction->setPriority(QAction::HighPriority);
                QAction* shareAction = makeAction(
                    QStringLiteral("Share"),
                    Typography::Icons::Share);
                auto* separator = new QAction(surface);
                separator->setSeparator(true);
                QAction* syncAction = makeAction(
                    QStringLiteral("Sync"),
                    Typography::Icons::Sync);
                syncAction->setPriority(QAction::LowPriority);
                QAction* pinAction = makeAction(
                    QStringLiteral("Pin"),
                    Typography::Icons::Pin);
                pinAction->setCheckable(true);
                QAction* settingsAction = makeAction(
                    QStringLiteral("Settings"),
                    Typography::Icons::Settings);
                QAction* helpAction = makeAction(
                    QStringLiteral("Help"),
                    Typography::Icons::Info);

                bar->addPrimaryAction(addAction);
                bar->addPrimaryAction(editAction);
                bar->addPrimaryAction(shareAction);
                bar->addPrimaryAction(separator);
                bar->addPrimaryAction(syncAction);
                bar->addPrimaryAction(pinAction);
                bar->addSecondaryAction(settingsAction);
                bar->addSecondaryAction(helpAction);

                auto* documentRow = horizontalGroup(panel, 10);
                auto* documentIcon =
                    new FontIcon(Typography::Icons::Document, documentRow);
                documentIcon->setIconSize(28);
                auto* documentText = samples::verticalGroup(
                    documentRow, 1);
                auto* documentTitle = makePreviewLabel(
                    documentText,
                    QStringLiteral("Quarterly report"),
                    Typography::FontRole::BodyStrong,
                    Label::TextColorRole::Primary);
                auto* documentMeta = makePreviewLabel(
                    documentText,
                    QStringLiteral("Edited just now · shared with 4 people"),
                    Typography::FontRole::Caption,
                    Label::TextColorRole::Secondary);
                boxLayout(documentText)->addWidget(documentTitle);
                boxLayout(documentText)->addWidget(documentMeta);
                boxLayout(documentRow)->addWidget(documentIcon);
                boxLayout(documentRow)->addWidget(documentText);
                panelLayout->addWidget(barHost);
                panelLayout->addWidget(documentRow);

                auto* controls = horizontalGroup(surface, 8);
                auto* compact = sampleButton(
                    controls, QStringLiteral("Compact view"));
                compact->setMinimumWidth(112);
                QObject::connect(
                    compact,
                    &Button::clicked,
                    barHost,
                    [barHost, compact, status]() {
                        const bool compactMode =
                            barHost->width() > 300;
                        barHost->setFixedWidth(
                            compactMode ? 288 : 536);
                        compact->setText(
                            compactMode
                                ? QStringLiteral("Full view")
                                : QStringLiteral("Compact view"));
                        status->setText(
                            compactMode
                                ? QStringLiteral(
                                      "Compact width · lower-priority commands moved to More")
                                : QStringLiteral(
                                      "Full width · all primary commands are visible"));
                    });
                boxLayout(controls)->addWidget(compact);

                auto* labels = sampleButton(
                    controls, QStringLiteral("Labels: Right"));
                labels->setMinimumWidth(112);
                QObject::connect(
                    labels,
                    &Button::clicked,
                    bar,
                    [bar, labels, status]() {
                        const bool showLabels =
                            bar->labelPosition()
                            == CommandBar::LabelPosition::Collapsed;
                        bar->setLabelPosition(
                            showLabels
                                ? CommandBar::LabelPosition::Right
                                : CommandBar::LabelPosition::Collapsed);
                        labels->setText(
                            showLabels
                                ? QStringLiteral("Labels: Right")
                                : QStringLiteral("Labels: Icons"));
                        status->setText(
                            showLabels
                                ? QStringLiteral(
                                      "Labels on the right · text remains available beside icons")
                                : QStringLiteral(
                                      "Icons only · accessible names still come from QAction text"));
                    });
                boxLayout(controls)->addWidget(labels);

                auto* background = sampleButton(
                    controls, QStringLiteral("Show background"));
                background->setMinimumWidth(128);
                QObject::connect(
                    background,
                    &Button::clicked,
                    bar,
                    [bar, background, status]() {
                        const bool visible =
                            !bar->backgroundVisible();
                        bar->setBackgroundVisible(visible);
                        background->setText(
                            visible
                                ? QStringLiteral("Hide background")
                                : QStringLiteral("Show background"));
                        status->setText(
                            visible
                                ? QStringLiteral(
                                      "Background on · use the self-contained command surface")
                                : QStringLiteral(
                                      "Background off · blend the commands into their host"));
                    });
                boxLayout(controls)->addWidget(background);

                boxLayout(surface)->addWidget(panel);
                boxLayout(surface)->addWidget(controls);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(
            QStringLiteral("command-bar-editing-router"),
            QStringLiteral("Context-aware editing commands"),
            QStringLiteral("Undo and Redo are the first two labeled commands. Edit the note to enable them, then select text or toggle read-only to watch the shared editing actions update."),
            QStringLiteral(
                "auto* editor = new LineEdit(this);\n"
                "editor->setText(\n"
                "    \"Review the release notes before Friday\");\n"
                "using Command = EditingCommandRouter::Command;\n"
                "auto* router = new EditingCommandRouter(this, this);\n"
                "auto* bar = new CommandBar(this);\n"
                "bar->setAccessibleName(\"Editing commands\");\n"
                "bar->setLabelPosition(\n"
                "    CommandBar::LabelPosition::Right);\n"
                "bar->setBackgroundVisible(false);\n"
                "\n"
                "// Reuse the Router actions; only their presentation changes.\n"
                "const auto commandAction =\n"
                "    [router](Command command, const QString& iconPath) {\n"
                "        QAction* action = router->action(command);\n"
                "        action->setIcon(QIcon(iconPath));\n"
                "        return action;\n"
                "    };\n"
                "\n"
                "bar->addPrimaryAction(\n"
                "    commandAction(Command::Undo, \":/icons/undo.svg\"));\n"
                "bar->addPrimaryAction(\n"
                "    commandAction(Command::Redo, \":/icons/redo.svg\"));\n"
                "auto* separator = new QAction(this);\n"
                "separator->setSeparator(true);\n"
                "bar->addPrimaryAction(separator);\n"
                "bar->addPrimaryAction(\n"
                "    commandAction(Command::Cut, \":/icons/cut.svg\"));\n"
                "bar->addPrimaryAction(\n"
                "    commandAction(Command::Copy, \":/icons/copy.svg\"));\n"
                "bar->addPrimaryAction(\n"
                "    commandAction(Command::Paste, \":/icons/paste.svg\"));\n"
                "bar->addSecondaryAction(\n"
                "    commandAction(Command::Delete, \":/icons/delete.svg\"));\n"
                "bar->addSecondaryAction(\n"
                "    commandAction(\n"
                "        Command::SelectAll, \":/icons/select-all.svg\"));\n"
                "\n"
                "auto* selectTextButton =\n"
                "    new Button(\"Select text\", this);\n"
                "auto* clearSelectionButton =\n"
                "    new Button(\"Clear selection\", this);\n"
                "auto* readOnlyButton =\n"
                "    new Button(\"Read-only: Off\", this);\n"
                "connect(selectTextButton, &Button::clicked, editor, [=] {\n"
                "    QTimer::singleShot(0, editor, [=] {\n"
                "        editor->setFocus(Qt::OtherFocusReason);\n"
                "        editor->selectAll();\n"
                "        router->refresh();\n"
                "    });\n"
                "});\n"
                "connect(clearSelectionButton, &Button::clicked, editor, [=] {\n"
                "    QTimer::singleShot(0, editor, [=] {\n"
                "        editor->setFocus(Qt::OtherFocusReason);\n"
                "        editor->deselect();\n"
                "        router->refresh();\n"
                "    });\n"
                "});\n"
                "readOnlyButton->setCheckable(true);\n"
                "connect(readOnlyButton, &QAbstractButton::toggled,\n"
                "        editor, [=](bool readOnly) {\n"
                "    QTimer::singleShot(0, editor, [=] {\n"
                "        editor->setFocus(Qt::OtherFocusReason);\n"
                "        editor->setReadOnly(readOnly);\n"
                "        editor->selectAll();\n"
                "        router->refresh();\n"
                "    });\n"
                "});"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* status = makeHintLabel(
                    surface,
                    QStringLiteral(
                        "Edit the note to enable Undo · select text to enable Cut and Copy"));
                auto* panel = new CommandPreviewPanel(surface);
                panel->setFixedSize(560, 154);
                auto* panelLayout = new QVBoxLayout(panel);
                panelLayout->setContentsMargins(12, 10, 12, 12);
                panelLayout->setSpacing(8);

                auto* heading = horizontalGroup(panel, 8);
                auto* headingIcon =
                    new FontIcon(Typography::Icons::Edit, heading);
                headingIcon->setIconSize(20);
                auto* headingLabel = makePreviewLabel(
                    heading,
                    QStringLiteral("Quick note"),
                    Typography::FontRole::BodyStrong,
                    Label::TextColorRole::Primary);
                boxLayout(heading)->addWidget(headingIcon);
                boxLayout(heading)->addWidget(headingLabel);

                auto* editor = new LineEdit(panel);
                editor->setObjectName(
                    QStringLiteral(
                        "Gallery.CommandBar.EditingTarget"));
                editor->setText(
                    QStringLiteral(
                        "Review the release notes before Friday"));
                editor->setFixedWidth(536);

                auto* router =
                    galleryWindowEditingCommandRouter(surface);
                auto* bar = new CommandBar(panel);
                bar->setObjectName(
                    QStringLiteral(
                        "Gallery.CommandBar.EditingRouter"));
                bar->setAccessibleName(
                    QStringLiteral("Editing commands"));
                bar->setLabelPosition(
                    CommandBar::LabelPosition::Right);
                bar->setBackgroundVisible(false);
                bar->setFixedWidth(536);
                auto* separator = new QAction(surface);
                separator->setSeparator(true);
                bar->addPrimaryAction(
                    router->action(
                        EditingCommandRouter::Command::Undo));
                bar->addPrimaryAction(
                    router->action(
                        EditingCommandRouter::Command::Redo));
                bar->addPrimaryAction(separator);
                bar->addPrimaryAction(
                    router->action(
                        EditingCommandRouter::Command::Cut));
                bar->addPrimaryAction(
                    router->action(
                        EditingCommandRouter::Command::Copy));
                bar->addPrimaryAction(
                    router->action(
                        EditingCommandRouter::Command::Paste));
                bar->addSecondaryAction(
                    router->action(
                        EditingCommandRouter::Command::Delete));
                bar->addSecondaryAction(
                    router->action(
                        EditingCommandRouter::Command::SelectAll));

                const auto setRouterGlyph =
                    [surface, router](
                        EditingCommandRouter::Command command,
                        const QString& glyph) {
                        surface->setActionGlyph(
                            router->action(command), glyph);
                    };
                setRouterGlyph(
                    EditingCommandRouter::Command::Undo,
                    Typography::Icons::Undo);
                setRouterGlyph(
                    EditingCommandRouter::Command::Redo,
                    Typography::Icons::Redo);
                setRouterGlyph(
                    EditingCommandRouter::Command::Cut,
                    Typography::Icons::Cut);
                setRouterGlyph(
                    EditingCommandRouter::Command::Copy,
                    Typography::Icons::Copy);
                setRouterGlyph(
                    EditingCommandRouter::Command::Paste,
                    Typography::Icons::Paste);
                setRouterGlyph(
                    EditingCommandRouter::Command::Delete,
                    Typography::Icons::Delete);
                setRouterGlyph(
                    EditingCommandRouter::Command::SelectAll,
                    Typography::Icons::SelectAll);

                panelLayout->addWidget(heading);
                panelLayout->addWidget(editor);
                panelLayout->addWidget(bar);

                auto* controls = horizontalGroup(surface, 8);
                auto* selectText = sampleButton(
                    controls, QStringLiteral("Select text"));
                auto* clearSelection = sampleButton(
                    controls, QStringLiteral("Clear selection"));
                auto* readOnly = sampleButton(
                    controls, QStringLiteral("Read-only: Off"));
                readOnly->setCheckable(true);

                using Command =
                    EditingCommandRouter::Command;
                const auto availabilityText =
                    [router, editor]() {
                        const auto state = [](bool enabled) {
                            return enabled
                                ? QStringLiteral("on")
                                : QStringLiteral("off");
                        };
                        return QStringLiteral(
                                   "Cut: %1 · Copy: %2 · Paste: %3 · Read-only: %4")
                            .arg(state(
                                router->canExecute(Command::Cut)))
                            .arg(state(
                                router->canExecute(Command::Copy)))
                            .arg(state(
                                router->canExecute(Command::Paste)))
                            .arg(state(editor->isReadOnly()));
                    };
                const auto updateAvailability =
                    [status, availabilityText]() {
                        status->setText(availabilityText());
                    };
                QObject::connect(
                    router,
                    &EditingCommandRouter::activeTargetChanged,
                    status,
                    [updateAvailability](bool) {
                        updateAvailability();
                    });
                QObject::connect(
                    router,
                    &EditingCommandRouter::commandCapabilityChanged,
                    status,
                    [updateAvailability](Command, bool) {
                        updateAvailability();
                    });

                QObject::connect(
                    selectText,
                    &Button::clicked,
                    editor,
                    [editor, router, updateAvailability]() {
                        QTimer::singleShot(
                            0,
                            editor,
                            [editor, router, updateAvailability]() {
                                editor->setFocus(
                                    Qt::OtherFocusReason);
                                editor->selectAll();
                                router->refresh();
                                updateAvailability();
                            });
                    });
                QObject::connect(
                    clearSelection,
                    &Button::clicked,
                    editor,
                    [editor, router, updateAvailability]() {
                        QTimer::singleShot(
                            0,
                            editor,
                            [editor, router, updateAvailability]() {
                                editor->setFocus(
                                    Qt::OtherFocusReason);
                                editor->deselect();
                                router->refresh();
                                updateAvailability();
                            });
                    });
                QObject::connect(
                    readOnly,
                    &QAbstractButton::toggled,
                    editor,
                    [editor, router, readOnly, updateAvailability](
                        bool checked) {
                        readOnly->setText(
                            checked
                                ? QStringLiteral("Read-only: On")
                                : QStringLiteral("Read-only: Off"));
                        QTimer::singleShot(
                            0,
                            editor,
                            [editor,
                             router,
                             checked,
                             updateAvailability]() {
                                editor->setFocus(
                                    Qt::OtherFocusReason);
                                editor->setReadOnly(checked);
                                editor->selectAll();
                                router->refresh();
                                updateAvailability();
                            });
                    });

                boxLayout(controls)->addWidget(selectText);
                boxLayout(controls)->addWidget(clearSelection);
                boxLayout(controls)->addWidget(readOnly);
                boxLayout(surface)->addWidget(panel);
                boxLayout(surface)->addWidget(controls);
                boxLayout(surface)->addWidget(status);
                return surface;
            })
    };
}

QVector<GallerySample> commandBarFlyoutSamples()
{
    return {
        makeSample(
            QStringLiteral("command-bar-flyout-show-modes"),
            QStringLiteral("Contextual actions for a photo"),
            QStringLiteral("Click for a focus-preserving Transient toolbar; right-click or use the keyboard for an expanded, keyboard-focused Standard context menu."),
            QStringLiteral(
                "auto* flyout = new CommandBarFlyout(this);\n"
                "auto* shareAction = new QAction(\n"
                "    QIcon(\":/icons/share.svg\"), \"Share\", this);\n"
                "auto* saveAction = new QAction(\n"
                "    QIcon(\":/icons/save.svg\"), \"Save\", this);\n"
                "auto* deleteAction = new QAction(\n"
                "    QIcon(\":/icons/delete.svg\"), \"Delete\", this);\n"
                "auto* resizeAction = new QAction(\n"
                "    QIcon(\":/icons/resize.svg\"), \"Resize\", this);\n"
                "auto* moveAction = new QAction(\n"
                "    QIcon(\":/icons/move.svg\"), \"Move\", this);\n"
                "flyout->addPrimaryAction(shareAction);\n"
                "flyout->addPrimaryAction(saveAction);\n"
                "flyout->addPrimaryAction(deleteAction);\n"
                "flyout->addSecondaryAction(resizeAction);\n"
                "flyout->addSecondaryAction(moveAction);\n"
                "\n"
                "// Proactive click: keep focus on the invoking content.\n"
                "connect(photo, &QAbstractButton::clicked, flyout, [=] {\n"
                "    flyout->showAt(\n"
                "        photo, CommandBarFlyout::ShowMode::Transient);\n"
                "});\n"
                "\n"
                "// Reactive context invocation: expand and enter the menu.\n"
                "photo->setContextMenuPolicy(Qt::CustomContextMenu);\n"
                "connect(photo, &QWidget::customContextMenuRequested,\n"
                "        flyout, [=](const QPoint& localPosition) {\n"
                "    flyout->showAtPoint(\n"
                "        photo, localPosition,\n"
                "        CommandBarFlyout::ShowMode::Standard);\n"
                "});"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* status = makeHintLabel(
                    surface,
                    QStringLiteral(
                        "Click: Transient · right-click or keyboard: Standard"));
                auto* tile = new ContextMediaTile(surface);

                auto* flyout = new CommandBarFlyout(surface);
                flyout->setObjectName(
                    QStringLiteral(
                        "Gallery.CommandBarFlyout"));
                auto* share =
                    new QAction(QStringLiteral("Share"), surface);
                auto* save =
                    new QAction(QStringLiteral("Save"), surface);
                auto* deleteAction =
                    new QAction(QStringLiteral("Delete"), surface);
                auto* resizeAction =
                    new QAction(QStringLiteral("Resize"), surface);
                auto* moveAction =
                    new QAction(QStringLiteral("Move"), surface);
                flyout->addPrimaryAction(share);
                flyout->addPrimaryAction(save);
                flyout->addPrimaryAction(deleteAction);
                flyout->addSecondaryAction(resizeAction);
                flyout->addSecondaryAction(moveAction);

                surface->setActionGlyph(
                    share, Typography::Icons::Share);
                surface->setActionGlyph(
                    save, Typography::Icons::Save);
                surface->setActionGlyph(
                    deleteAction, Typography::Icons::Delete);
                surface->setActionGlyph(
                    resizeAction, Typography::Icons::FullScreen);
                surface->setActionGlyph(
                    moveAction, Typography::Icons::Forward);

                const auto connectStatus =
                    [status](QAction* action) {
                        QObject::connect(
                            action,
                            &QAction::triggered,
                            status,
                            [status, action]() {
                                status->setText(
                                    QStringLiteral("Command: %1")
                                        .arg(action->text()));
                            });
                    };
                connectStatus(share);
                connectStatus(save);
                connectStatus(deleteAction);
                connectStatus(resizeAction);
                connectStatus(moveAction);

                tile->setInvokeHandler(
                    [flyout, tile, status](
                        const QPoint& localPosition,
                        bool standard) {
                        flyout->setAlwaysExpanded(false);
                        if (standard) {
                            flyout->showAtPoint(
                                tile,
                                localPosition,
                                CommandBarFlyout::ShowMode::Standard);
                            status->setText(
                                QStringLiteral(
                                    "Standard · expanded and keyboard focused"));
                            return;
                        }
                        flyout->showAt(
                            tile,
                            CommandBarFlyout::ShowMode::Transient);
                        status->setText(
                            QStringLiteral(
                                "Transient · collapsed and focus preserved"));
                    });

                boxLayout(surface)->addWidget(tile);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(
            QStringLiteral(
                "command-bar-flyout-always-expanded"),
            QStringLiteral("Keep secondary commands visible"),
            QStringLiteral("AlwaysExpanded removes the More step and keeps secondary commands visible even in Transient mode; turn it off to compare the collapsed presentation."),
            QStringLiteral(
                "auto* flyout = new CommandBarFlyout(this);\n"
                "flyout->setAlwaysExpanded(true);\n"
                "auto* copyLinkAction = new QAction(\n"
                "    QIcon(\":/icons/link.svg\"), \"Copy link\", this);\n"
                "auto* favoriteAction = new QAction(\n"
                "    QIcon(\":/icons/favorite.svg\"), \"Favorite\", this);\n"
                "favoriteAction->setCheckable(true);\n"
                "auto* renameAction = new QAction(\n"
                "    QIcon(\":/icons/edit.svg\"), \"Rename\", this);\n"
                "auto* propertiesAction = new QAction(\n"
                "    QIcon(\":/icons/info.svg\"), \"Properties\", this);\n"
                "flyout->addPrimaryAction(copyLinkAction);\n"
                "flyout->addPrimaryAction(favoriteAction);\n"
                "flyout->addSecondaryAction(renameAction);\n"
                "flyout->addSecondaryAction(propertiesAction);\n"
                "\n"
                "auto* openButton =\n"
                "    new Button(\"Open actions\", this);\n"
                "auto* alwaysExpandedButton =\n"
                "    new Button(\"Always expanded: On\", this);\n"
                "connect(openButton, &Button::clicked, flyout, [=] {\n"
                "    flyout->showAt(\n"
                "        openButton,\n"
                "        CommandBarFlyout::ShowMode::Transient);\n"
                "});\n"
                "alwaysExpandedButton->setCheckable(true);\n"
                "alwaysExpandedButton->setChecked(true);\n"
                "connect(alwaysExpandedButton,\n"
                "        &QAbstractButton::toggled, flyout,\n"
                "        &CommandBarFlyout::setAlwaysExpanded);"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* status = makeHintLabel(
                    surface,
                    QStringLiteral(
                        "Always expanded · primary and secondary commands open together"));

                auto* panel = new CommandPreviewPanel(surface);
                panel->setFixedSize(560, 88);
                auto* panelLayout = new QHBoxLayout(panel);
                panelLayout->setContentsMargins(16, 12, 16, 12);
                panelLayout->setSpacing(12);

                auto* fileIcon = new FontIcon(
                    Typography::Icons::Document, panel);
                fileIcon->setIconSize(28);
                auto* fileText = samples::verticalGroup(panel, 1);
                auto* fileTitle = makePreviewLabel(
                    fileText,
                    QStringLiteral("Release-notes.md"),
                    Typography::FontRole::BodyStrong,
                    Label::TextColorRole::Primary);
                auto* fileMeta = makePreviewLabel(
                    fileText,
                    QStringLiteral("Markdown · 18 KB"),
                    Typography::FontRole::Caption,
                    Label::TextColorRole::Secondary);
                boxLayout(fileText)->addWidget(fileTitle);
                boxLayout(fileText)->addWidget(fileMeta);

                auto* open = sampleButton(
                    panel, QStringLiteral("Open actions"));
                open->setObjectName(
                    QStringLiteral(
                        "Gallery.CommandBarFlyout.OpenAlwaysExpanded"));
                panelLayout->addWidget(fileIcon);
                panelLayout->addWidget(fileText, 1);
                panelLayout->addWidget(open);

                auto* flyout = new CommandBarFlyout(surface);
                flyout->setObjectName(
                    QStringLiteral(
                        "Gallery.CommandBarFlyout.AlwaysExpanded"));
                flyout->setAlwaysExpanded(true);
                auto* copyLink =
                    new QAction(QStringLiteral("Copy link"), surface);
                auto* favorite =
                    new QAction(QStringLiteral("Favorite"), surface);
                favorite->setCheckable(true);
                auto* rename =
                    new QAction(QStringLiteral("Rename"), surface);
                auto* properties =
                    new QAction(QStringLiteral("Properties"), surface);
                flyout->addPrimaryAction(copyLink);
                flyout->addPrimaryAction(favorite);
                flyout->addSecondaryAction(rename);
                flyout->addSecondaryAction(properties);
                surface->setActionGlyph(
                    copyLink, Typography::Icons::Link);
                surface->setActionGlyph(
                    favorite, Typography::Icons::FavoriteStar);
                surface->setActionGlyph(
                    rename, Typography::Icons::Edit);
                surface->setActionGlyph(
                    properties, Typography::Icons::Info);

                auto* controls = horizontalGroup(surface, 8);
                auto* alwaysExpanded = sampleButton(
                    controls,
                    QStringLiteral("Always expanded: On"));
                alwaysExpanded->setCheckable(true);
                alwaysExpanded->setChecked(true);
                QObject::connect(
                    alwaysExpanded,
                    &QAbstractButton::toggled,
                    flyout,
                    [flyout, alwaysExpanded, status](
                        bool checked) {
                        flyout->setAlwaysExpanded(checked);
                        alwaysExpanded->setText(
                            checked
                                ? QStringLiteral(
                                      "Always expanded: On")
                                : QStringLiteral(
                                      "Always expanded: Off"));
                        status->setText(
                            checked
                                ? QStringLiteral(
                                      "Always expanded · primary and secondary commands open together")
                                : QStringLiteral(
                                      "Collapsed · secondary commands are available behind More"));
                    });
                QObject::connect(
                    open,
                    &Button::clicked,
                    flyout,
                    [flyout, open, status]() {
                        flyout->showAt(
                            open,
                            CommandBarFlyout::ShowMode::Transient);
                        status->setText(
                            flyout->isAlwaysExpanded()
                                ? QStringLiteral(
                                      "Transient + AlwaysExpanded · focus stays on Open actions")
                                : QStringLiteral(
                                      "Transient · focus stays on Open actions; use More for secondary commands"));
                    });
                for (QAction* action :
                     flyout->primaryActions()
                         + flyout->secondaryActions()) {
                    QObject::connect(
                        action,
                        &QAction::triggered,
                        status,
                        [status, action]() {
                            status->setText(
                                QStringLiteral("Command: %1")
                                    .arg(displayActionText(
                                        action->text())));
                        });
                }

                boxLayout(controls)->addWidget(alwaysExpanded);
                boxLayout(surface)->addWidget(panel);
                boxLayout(surface)->addWidget(controls);
                boxLayout(surface)->addWidget(status);
                return surface;
            })
    };
}

} // namespace

QVector<GallerySample> menusToolbarsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("menu"))
        return menuSamples();
    if (routeId == QStringLiteral("menu-bar"))
        return menuBarSamples();
    if (routeId == QStringLiteral("command-bar"))
        return commandBarSamples();
    if (routeId == QStringLiteral("command-bar-flyout"))
        return commandBarFlyoutSamples();
    return {};
}

} // namespace fluent::gallery

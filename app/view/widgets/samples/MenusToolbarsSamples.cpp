#include "view/support/GalleryToast.h"
#include "MenusToolbarsSamples.h"

#include <QAction>
#include <QActionGroup>
#include <QBitmap>
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
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <functional>
#include <utility>

#include "components/basicinput/Button.h"
#include "components/basicinput/CheckBox.h"
#include "components/basicinput/Slider.h"
#include "components/basicinput/DropDownButton.h"
#include "components/foundation/FontIcon.h"
#include "components/collections/ListView.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandBarFlyout.h"
#include "components/menus_toolbars/CommandPalette.h"
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
    explicit MenusToolbarsSampleSurface(QWidget* parent = nullptr, int spacing = 12) : QWidget(parent)
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
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), themeRadius().overlay, themeRadius().overlay);
    }

private:
    struct ActionGlyph {
        QPointer<QAction> action;
        QString glyph;
    };

    QPixmap glyphPixmapForColor(const QString& glyph, int logicalSize, const QColor& color) const
    {
        const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
        const int physicalSize = qMax(1, qCeil(logicalSize * dpr));
        QPixmap pixmap(physicalSize, physicalSize);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setPen(color);
        Typography::Icons::paintGlyph(painter, QRectF(0, 0, logicalSize, logicalSize), glyph, logicalSize,
            Qt::AlignCenter);
        painter.end();
        // Font icons have font-line bearings; center their visible artwork in
        // the QAction slot before caching at the actual screen resolution.
        // zh_CN: 字体图标带有行框留白，按可见图形居中后再缓存到实际屏幕分辨率。
        const QRect ink = QRegion(pixmap.mask()).boundingRect();
        if (!ink.isEmpty()) {
            QPixmap centered(pixmap.size());
            centered.setDevicePixelRatio(dpr);
            centered.fill(Qt::transparent);
            QPainter centeredPainter(&centered);
            const QPointF offset((physicalSize - ink.width()) / 2 - ink.x(),
                (physicalSize - ink.height()) / 2 - ink.y());
            centeredPainter.drawPixmap(offset / dpr, pixmap);
            return centered;
        }
        return pixmap;
    }

    void updateActionIcon(const ActionGlyph& entry) const
    {
        if (!entry.action)
            return;

        const auto& colors = themeColorsRef();
        QIcon icon;
        for (const int size : {16, 20, 24}) {
            const QPixmap normal = glyphPixmapForColor(entry.glyph, size, colors.textPrimary);
            const QPixmap disabled = glyphPixmapForColor(entry.glyph, size, colors.textDisabled);
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
    explicit CommandPreviewPanel(QWidget* parent = nullptr) : QWidget(parent)
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
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), themeRadius().overlay, themeRadius().overlay);
    }
};

class ContextMediaTile final : public QWidget, public fluent::FluentElement {
public:
    using InvokeHandler = std::function<void(const QPoint& localPosition, bool standard)>;

    explicit ContextMediaTile(QWidget* parent = nullptr)
        : QWidget(parent),
          m_photo(samples::gradientPixmap(QSize(276, 160),
              QColor(0x35, 0x5C, 0x7D),
              QColor(0xA7, 0xC6, 0xD9),
              QStringLiteral("Northern ridge")))
    {
        setObjectName(QStringLiteral("Gallery.CommandBarFlyout.ContextTile"));
        setFixedSize(560, 184);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setAccessibleName(QStringLiteral("Northern ridge photo"));
        setAccessibleDescription(QStringLiteral(
            "Click for quick commands without moving focus, or right-click for the expanded context menu."));
    }

    void setInvokeHandler(InvokeHandler handler) { m_invokeHandler = std::move(handler); }

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

        painter.setPen(hasFocus() && m_keyboardFocusVisible ? colors.accentDefault : colors.strokeCard);
        painter.setBrush(background);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), themeRadius().overlay, themeRadius().overlay);

        const QRect photoRect(12, 12, 276, 160);
        painter.drawPixmap(photoRect, m_photo);

        const int textLeft = 312;
        const int textWidth = width() - textLeft - 20;
        painter.setPen(colors.textPrimary);
        painter.setFont(themeFont(Typography::FontRole::BodyStrong).toQFont());
        painter.drawText(QRect(textLeft, 28, textWidth, 24), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Northern ridge"));

        painter.setPen(colors.textSecondary);
        painter.setFont(themeFont(Typography::FontRole::Caption).toQFont());
        painter.drawText(QRect(textLeft, 54, textWidth, 20), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Photo · 4.8 MB"));

        painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
        painter.drawText(QRect(textLeft, 94, textWidth, 48), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
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
        if (event && event->button() == Qt::LeftButton && rect().contains(event->pos())) {
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
        const bool keyboardInvocation = event->reason() == QContextMenuEvent::Keyboard;
        setFocus(keyboardInvocation ? Qt::OtherFocusReason : Qt::MouseFocusReason);
        m_keyboardFocusVisible = keyboardInvocation;
        update();
        invoke(event->pos(), true);
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event
            && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space)) {
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
        m_keyboardFocusVisible = event && event->reason() != Qt::MouseFocusReason;
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

MenusToolbarsSampleSurface* sampleSurface(QWidget* parent, int spacing = 12)
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
    label->setTextColorRole(Label::TextColorRole::Primary); // QSS-proof on the styled preview surface
    return label;
}

Label* makePreviewLabel(QWidget* parent, const QString& text, Typography::FontRole role, Label::TextColorRole colorRole)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(role);
    label->setTextColorRole(colorRole);
    return label;
}

Label* makeHintLabel(QWidget* parent, const QString& text)
{
    auto* label = makePreviewLabel(parent, text, Typography::FontRole::Caption, Label::TextColorRole::Secondary);
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

QAction*
addFeedbackAction(FluentMenu* menu, QWidget* anchor, const QString& text, const QKeySequence& shortcut = QKeySequence())
{
    QAction* action = menu->addAction(text);
    if (!shortcut.isEmpty())
        action->setShortcut(shortcut);
    QObject::connect(action, &QAction::triggered, anchor, [anchor, text]() {
        showGalleryToast(anchor, QStringLiteral("%1 selected").arg(displayActionText(text)),
            status_info::Toast::Informational);
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
        makeSample(QStringLiteral("menu-command-shortcuts"), QStringLiteral("Commands and shortcuts"),
            QStringLiteral("Menu commands can expose native shortcuts and update app state when triggered."),
            QStringLiteral(R"(using fluent::status_info::Toast;
auto addCommand = [this](FluentMenu* menu, const QString& text, const QString& glyph,
                         const QKeySequence& shortcut = QKeySequence()) {
    auto* action = menu->addAction(text);
    if (!shortcut.isEmpty())
        action->setShortcut(shortcut);
    FontIcon icon(glyph);
    icon.setIconSize(16);
    icon.resize(20, 20);
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);
    icon.render(&pixmap);
    action->setIcon(QIcon(pixmap));
    connect(action, &QAction::triggered, this, [this, text] {
        Toast::showOrUpdateToast(this, "gallery.action-feedback", text + " selected",
            Toast::Informational, 2200, Toast::Top, QMargins(16, 50, 16, 16));
    });
    return action;
};
auto* button = new DropDownButton("File", this);
button->setIconGlyph(Typography::Icons::Document);
auto* menu = new FluentMenu(QString(), button);
addCommand(menu, "New", Typography::Icons::Add, QKeySequence::New);
addCommand(menu, "Open...", Typography::Icons::Folder, QKeySequence::Open);
addCommand(menu, "Save", Typography::Icons::Save, QKeySequence::Save);
menu->addSeparator();
menu->addAction("Publish")->setEnabled(false);
addCommand(menu, "Close", Typography::Icons::Cancel);
button->setMenu(menu);)"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* status = makeHintLabel(surface,
                    QStringLiteral("Open File or use shortcuts · disabled items keep icon and shortcut alignment"));

                auto* panel = new CommandPreviewPanel(surface);
                panel->setFixedSize(560, 88);
                auto* panelLayout = new QHBoxLayout(panel);
                panelLayout->setContentsMargins(16, 12, 16, 12);
                panelLayout->setSpacing(12);

                auto* docIcon = new FontIcon(Typography::Icons::Document, panel);
                docIcon->setIconSize(28);
                auto* docText = samples::verticalGroup(panel, 1);
                docText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                docText->setObjectName(QStringLiteral("Gallery.CommandPreview.Text"));
                auto* docTitle = makePreviewLabel(docText, QStringLiteral("Quarterly brief.md"),
                    Typography::FontRole::BodyStrong, Label::TextColorRole::Primary);
                auto* docMeta = makePreviewLabel(docText, QStringLiteral("Ctrl+N New · Ctrl+O Open · Ctrl+S Save"),
                    Typography::FontRole::Caption, Label::TextColorRole::Secondary);
                boxLayout(docText)->addWidget(docTitle);
                boxLayout(docText)->addWidget(docMeta);

                auto* button = new DropDownButton(QStringLiteral("File"), panel);
                button->setIconGlyph(Typography::Icons::Document);
                button->setMinimumWidth(112);

                auto* menu = new FluentMenu(QString(), button);
                const auto addMenuItem =
                    [menu, surface, status](const QString& text, const QString& glyph, const QString& feedback,
                        const QKeySequence& shortcut = QKeySequence()) {
                        QAction* action = addFeedbackAction(menu, surface, text, shortcut);
                        surface->setActionGlyph(action, glyph);
                        QObject::connect(action, &QAction::triggered, status, [status, feedback]() {
                            status->setText(feedback);
                        });
                        return action;
                    };
                addMenuItem(QStringLiteral("New"), Typography::Icons::Add,
                    QStringLiteral("Last command: New · created a blank draft"), QKeySequence::New);
                addMenuItem(QStringLiteral("Open..."), Typography::Icons::Folder,
                    QStringLiteral("Last command: Open... · ready to browse documents"), QKeySequence::Open);
                addMenuItem(QStringLiteral("Save"), Typography::Icons::Save,
                    QStringLiteral("Last command: Save · saved Quarterly brief.md"), QKeySequence::Save);
                menu->addSeparator();
                auto* publish = menu->addAction(QStringLiteral("Publish"));
                publish->setEnabled(false);
                surface->setActionGlyph(publish, Typography::Icons::Share);
                addMenuItem(QStringLiteral("Close"), Typography::Icons::Cancel,
                    QStringLiteral("Last command: Close · closed document session"));
                button->setMenu(menu);

                panelLayout->addWidget(docIcon);
                panelLayout->addWidget(docText, 1);
                panelLayout->addWidget(button);
                boxLayout(surface)->addWidget(panel);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(QStringLiteral("menu-cascading-selection"), QStringLiteral("Submenus and checked items"),
            QStringLiteral(
                "Open Sort by, then Order for a third menu level. Exclusive choices keep their checked state."),
            QStringLiteral(
                "auto* menu = new FluentMenu(QString(), this);\n"
                "auto* sortMenu = new FluentMenu(\"Sort by\", menu);\n"
                "sortMenu->addAction(\"Name\");\n"
                "sortMenu->addAction(\"Date modified\");\n"
                "sortMenu->addAction(\"Type\");\n"
                "for (QAction* action : sortMenu->actions()) {\n"
                "    connect(action, &QAction::triggered, this, [this, action] {\n"
                "        Toast::showOrUpdateToast(this, \"gallery.action-feedback\",\n"
                "            action->text() + \" selected\", Toast::Informational, 2200,\n"
                "            Toast::Top, QMargins(16, 50, 16, 16));\n"
                "    });\n"
                "}\n"
                "auto* orderMenu = new FluentMenu(\"Order\", sortMenu);\n"
                "auto* orderGroup = new QActionGroup(orderMenu);\n"
                "orderGroup->setExclusive(true);\n"
                "for (const QString& name : {QStringLiteral(\"Ascending\"), QStringLiteral(\"Descending\")}) {\n"
                "    auto* action = orderMenu->addAction(name);\n"
                "    action->setCheckable(true);\n"
                "    action->setChecked(name == QStringLiteral(\"Ascending\"));\n"
                "    orderGroup->addAction(action);\n"
                "}\n"
                "sortMenu->addMenu(orderMenu);\n"
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
                button->setObjectName(QStringLiteral("Gallery.Menu.ViewOptions"));
                button->setMinimumWidth(150);

                auto* menu = new FluentMenu(QString(), button);

                menu->setObjectName(QStringLiteral("Gallery.Menu.Cascading"));
                auto* sortMenu = new FluentMenu(QStringLiteral("Sort by"), menu);
                sortMenu->setObjectName(QStringLiteral("Gallery.Menu.Sort"));
                addFeedbackAction(sortMenu, surface, QStringLiteral("Name"));
                addFeedbackAction(sortMenu, surface, QStringLiteral("Date modified"));
                addFeedbackAction(sortMenu, surface, QStringLiteral("Type"));
                auto* orderMenu = new FluentMenu(QStringLiteral("Order"), sortMenu);
                orderMenu->setObjectName(QStringLiteral("Gallery.Menu.Order"));
                auto* orderGroup = new QActionGroup(orderMenu);
                orderGroup->setExclusive(true);
                for (const QString& name : {QStringLiteral("Ascending"), QStringLiteral("Descending")}) {
                    auto* action = orderMenu->addAction(name);
                    action->setCheckable(true);
                    action->setChecked(name == QStringLiteral("Ascending"));
                    orderGroup->addAction(action);
                    QObject::connect(action, &QAction::triggered, status, [status, name] {
                        status->setText(QStringLiteral("Order: %1").arg(name));
                    });
                }
                sortMenu->addSeparator();
                sortMenu->addMenu(orderMenu);
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
            })};
}

QVector<GallerySample> menuBarSamples()
{
    return {
        makeSample(QStringLiteral("menu-bar-hosted-surface"), QStringLiteral("Hosted menu bar"),
            QStringLiteral("A MenuBar can blend into a card or custom title bar by hiding its own canvas."),
            QStringLiteral(R"(using fluent::status_info::Toast;
auto addCommand = [this](FluentMenu* menu, const QString& text,
                         const QKeySequence& shortcut = QKeySequence()) {
    auto* action = menu->addAction(text);
    if (!shortcut.isEmpty()) action->setShortcut(shortcut);
    connect(action, &QAction::triggered, this, [this, text] {
        Toast::showOrUpdateToast(this, "gallery.action-feedback", text + " selected",
            Toast::Informational, 2200, Toast::Top, QMargins(16, 50, 16, 16));
    });
    return action;
};
auto* menuBar = new FluentMenuBar(this);
menuBar->setBackgroundVisible(false);
auto* fileMenu = new FluentMenu("File", menuBar);
addCommand(fileMenu, "New", QKeySequence::New);
addCommand(fileMenu, "Open...", QKeySequence::Open);
addCommand(fileMenu, "Save", QKeySequence::Save);
menuBar->addMenu(fileMenu);
auto* editMenu = new FluentMenu("Edit", menuBar);
addCommand(editMenu, "Undo", QKeySequence::Undo);
addCommand(editMenu, "Cut", QKeySequence::Cut);
addCommand(editMenu, "Copy", QKeySequence::Copy);
addCommand(editMenu, "Paste", QKeySequence::Paste);
menuBar->addMenu(editMenu);
auto* viewMenu = new FluentMenu("View", menuBar);
auto* wrap = viewMenu->addAction("Word wrap");
wrap->setCheckable(true);
wrap->setChecked(true);
menuBar->addMenu(viewMenu);
auto* helpMenu = new FluentMenu("Help", menuBar);
addCommand(helpMenu, "About");
menuBar->addMenu(helpMenu);)"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* status = makeHintLabel(surface,
                    QStringLiteral("Background hidden · the menu bar inherits the enclosing panel header"));

                auto* panel = new CommandPreviewPanel(surface);
                panel->setFixedSize(560, 124);
                auto* panelLayout = new QVBoxLayout(panel);
                panelLayout->setContentsMargins(14, 10, 14, 12);
                panelLayout->setSpacing(10);

                auto* header = horizontalGroup(panel, 10);
                auto* headerIcon = new FontIcon(Typography::Icons::Document, header);
                headerIcon->setIconSize(18);
                auto* headerTitle = makePreviewLabel(header, QStringLiteral("Studio"), Typography::FontRole::BodyStrong,
                    Label::TextColorRole::Primary);

                auto* menuBar = new FluentMenuBar(header);
                menuBar->setObjectName(QStringLiteral("Gallery.MenuBar.Hosted"));
                configureMenuBar(menuBar);
                menuBar->setBackgroundVisible(false);

                const auto addMenuItem =
                    [surface, status](FluentMenu* menu, const QString& text, const QString& glyph,
                        const QKeySequence& shortcut = QKeySequence()) {
                        QAction* action = addFeedbackAction(menu, surface, text, shortcut);
                        surface->setActionGlyph(action, glyph);
                        QObject::connect(action, &QAction::triggered, status, [status, text]() {
                            status->setText(QStringLiteral("Triggered: %1").arg(displayActionText(text)));
                        });
                        return action;
                    };

                auto* fileMenu = new FluentMenu(QStringLiteral("File"), menuBar);
                addMenuItem(fileMenu, QStringLiteral("New"), Typography::Icons::Add, QKeySequence::New);
                addMenuItem(fileMenu, QStringLiteral("Open..."), Typography::Icons::Folder, QKeySequence::Open);
                addMenuItem(fileMenu, QStringLiteral("Save"), Typography::Icons::Save, QKeySequence::Save);
                menuBar->addMenu(fileMenu);

                auto* editMenu = new FluentMenu(QStringLiteral("Edit"), menuBar);
                editMenu->setObjectName(QStringLiteral("Gallery.MenuBar.Edit"));
                addMenuItem(editMenu, QStringLiteral("Undo"), Typography::Icons::Undo, QKeySequence::Undo);
                editMenu->addSeparator();
                addMenuItem(editMenu, QStringLiteral("Cut"), Typography::Icons::Cut, QKeySequence::Cut);
                addMenuItem(editMenu, QStringLiteral("Copy"), Typography::Icons::Copy, QKeySequence::Copy);
                addMenuItem(editMenu, QStringLiteral("Paste"), Typography::Icons::Paste, QKeySequence::Paste);
                menuBar->addMenu(editMenu);

                auto* viewMenu = new FluentMenu(QStringLiteral("View"), menuBar);
                auto* wordWrap = viewMenu->addAction(QStringLiteral("Word wrap"));
                wordWrap->setCheckable(true);
                wordWrap->setChecked(true);
                QObject::connect(wordWrap, &QAction::toggled, status, [status](bool checked) {
                    status->setText(checked ? QStringLiteral("Word wrap: On") : QStringLiteral("Word wrap: Off"));
                });
                menuBar->addMenu(viewMenu);

                auto* helpMenu = new FluentMenu(QStringLiteral("Help"), menuBar);
                addMenuItem(helpMenu, QStringLiteral("About"), Typography::Icons::Info);
                menuBar->addMenu(helpMenu);

                boxLayout(header)->addWidget(headerIcon);
                boxLayout(header)->addWidget(headerTitle);
                boxLayout(header)->addWidget(menuBar, 1);

                auto* noteRow = horizontalGroup(panel, 10);
                auto* noteIcon = new FontIcon(Typography::Icons::Edit, noteRow);
                noteIcon->setIconSize(24);
                auto* noteText = samples::verticalGroup(noteRow, 1);
                noteText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                noteText->setObjectName(QStringLiteral("Gallery.CommandPreview.Text"));
                auto* noteTitle = makePreviewLabel(noteText, QStringLiteral("Release checklist.md"),
                    Typography::FontRole::BodyStrong, Label::TextColorRole::Primary);
                auto* noteMeta = makePreviewLabel(noteText,
                    QStringLiteral("Hosted canvas disabled · blends cleanly into the panel surface"),
                    Typography::FontRole::Caption, Label::TextColorRole::Secondary);
                boxLayout(noteText)->addWidget(noteTitle);
                boxLayout(noteText)->addWidget(noteMeta);
                boxLayout(noteRow)->addWidget(noteIcon);
                boxLayout(noteRow)->addWidget(noteText);

                panelLayout->addWidget(header);
                panelLayout->addWidget(noteRow);
                boxLayout(surface)->addWidget(panel);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(QStringLiteral("menu-bar-access-keys"), QStringLiteral("Access keys and actions"),
            QStringLiteral("Top-level menu items support access keys, keyboard focus, and regular QAction commands."),
            QStringLiteral(R"(using fluent::status_info::Toast;
auto addCommand = [this](FluentMenu* menu, const QString& text,
                         const QKeySequence& shortcut = QKeySequence()) {
    auto* action = menu->addAction(text);
    if (!shortcut.isEmpty()) action->setShortcut(shortcut);
    connect(action, &QAction::triggered, this, [this, text] {
        Toast::showOrUpdateToast(this, "gallery.action-feedback", text + " selected",
            Toast::Informational, 2200, Toast::Top, QMargins(16, 50, 16, 16));
    });
    return action;
};
auto* menuBar = new FluentMenuBar(this);
auto* focusButton = new Button("Focus", this);
connect(focusButton, &Button::clicked, menuBar, [menuBar] {
    menuBar->setFocus(Qt::OtherFocusReason);
    Toast::showOrUpdateToast(menuBar, "gallery.action-feedback", "Menu bar focused",
        Toast::Informational, 2200, Toast::Top, QMargins(16, 50, 16, 16));
});
auto* fileMenu = new FluentMenu("&File", menuBar);
fileMenu->menuAction()->setProperty("accessKey", "F");
addCommand(fileMenu, "New", QKeySequence::New);
addCommand(fileMenu, "Save", QKeySequence::Save);
menuBar->addMenu(fileMenu);
auto* viewMenu = new FluentMenu("&View", menuBar);
viewMenu->menuAction()->setProperty("accessKey", "V");
addCommand(viewMenu, "Zoom in");
addCommand(viewMenu, "Zoom out");
menuBar->addMenu(viewMenu);
auto* runAction = new QAction("Run", menuBar);
connect(runAction, &QAction::triggered, menuBar, [menuBar] {
    Toast::showOrUpdateToast(menuBar, "gallery.action-feedback", "Run selected",
        Toast::Informational, 2200, Toast::Top, QMargins(16, 50, 16, 16));
});
menuBar->addAction(runAction);
menuBar->addAction("Deploy")->setEnabled(false);)"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* row = horizontalGroup(surface, 8);

                auto* focusButton = sampleButton(row, QStringLiteral("Focus"));
                auto* menuBar = new FluentMenuBar(surface);
                configureMenuBar(menuBar);
                menuBar->setMinimumWidth(390);

                auto* fileMenu = new FluentMenu(QStringLiteral("&File"), menuBar);
                fileMenu->menuAction()->setProperty("accessKey", QStringLiteral("F"));
                addFeedbackAction(fileMenu, surface, QStringLiteral("New"), QKeySequence::New);
                addFeedbackAction(fileMenu, surface, QStringLiteral("Save"), QKeySequence::Save);
                menuBar->addMenu(fileMenu);

                auto* viewMenu = new FluentMenu(QStringLiteral("&View"), menuBar);
                viewMenu->menuAction()->setProperty("accessKey", QStringLiteral("V"));
                addFeedbackAction(viewMenu, surface, QStringLiteral("Zoom in"));
                addFeedbackAction(viewMenu, surface, QStringLiteral("Zoom out"));
                menuBar->addMenu(viewMenu);

                auto* runAction = new QAction(QStringLiteral("Run"), menuBar);
                QObject::connect(runAction, &QAction::triggered, surface, [surface]() {
                    showGalleryToast(surface, QStringLiteral("Run selected"), status_info::Toast::Informational);
                });
                menuBar->addAction(runAction);

                auto* disabledAction = new QAction(QStringLiteral("Deploy"), menuBar);
                disabledAction->setEnabled(false);
                menuBar->addAction(disabledAction);

                QObject::connect(focusButton, &Button::clicked, menuBar, [menuBar]() {
                    menuBar->setFocus(Qt::OtherFocusReason);
                    showGalleryToast(menuBar, QStringLiteral("Menu bar focused"), status_info::Toast::Informational);
                });

                boxLayout(row)->addWidget(focusButton, 0, Qt::AlignVCenter);
                boxLayout(row)->addWidget(menuBar, 0, Qt::AlignVCenter);
                boxLayout(surface)->addWidget(row);
                return surface;
            })};
}

QWidget* commandSubmenusPreview(QWidget* parent, bool contextual)
{
    auto* surface = new MenusToolbarsSampleSurface(parent);
    auto* status = makeStatusLabel(surface, QStringLiteral("Order: Ascending"));
    status->setObjectName(QStringLiteral("Gallery.CommandSubmenus.Status"));
    auto* view = new FluentMenu(QStringLiteral("View"), surface);
    view->setObjectName(QStringLiteral("Gallery.CommandSubmenus.View"));
    auto* sort = new FluentMenu(QStringLiteral("Sort by"), view);
    auto* order = new FluentMenu(QStringLiteral("Order"), sort);
    view->addMenu(sort);
    sort->addMenu(order);
    auto* group = new QActionGroup(surface);
    auto* ascending = order->addAction(QStringLiteral("Ascending"));
    auto* descending = order->addAction(QStringLiteral("Descending"));
    ascending->setCheckable(true);
    descending->setCheckable(true);
    group->addAction(ascending);
    group->addAction(descending);
    ascending->setChecked(true);
    QObject::connect(group, &QActionGroup::triggered, status, [status](QAction* action) {
        status->setText(QStringLiteral("Order: %1").arg(action->text()));
    });
    if (contextual) {
        auto* open = sampleButton(surface, QStringLiteral("Open view commands"));
        open->setObjectName(QStringLiteral("Gallery.CommandSubmenus.Open"));
        auto* flyout = new CommandBarFlyout(surface);
        flyout->setObjectName(QStringLiteral("Gallery.CommandSubmenus.Flyout"));
        flyout->setAlwaysExpanded(true);
        flyout->addPrimaryAction(view->menuAction());
        flyout->addSecondaryAction(sort->menuAction());
        QObject::connect(open, &Button::clicked, flyout, [flyout, open] { flyout->showAt(open); });
        boxLayout(surface)->addWidget(open);
    } else {
        surface->setMinimumWidth(512);
        boxLayout(surface)->setAlignment(Qt::AlignTop);
        auto* bar = new CommandBar(surface);
        bar->setObjectName(QStringLiteral("Gallery.CommandSubmenus.Bar"));
        bar->setLabelPosition(CommandBar::LabelPosition::Right);
        bar->setFixedWidth(480);
        bar->addPrimaryAction(view->menuAction());
        bar->addSecondaryAction(sort->menuAction());
        auto* compact = sampleButton(surface, QStringLiteral("Compact width"));
        compact->setObjectName(QStringLiteral("Gallery.CommandSubmenus.Compact"));
        compact->setCheckable(true);
        QObject::connect(compact, &Button::toggled, bar, [bar](bool checked) {
            bar->setFixedWidth(checked ? 80 : 480);
        });
        boxLayout(surface)->addWidget(bar, 0, Qt::AlignLeft);
        boxLayout(surface)->addWidget(compact, 0, Qt::AlignLeft);
    }
    boxLayout(surface)->addWidget(status);
    return surface;
}

QVector<GallerySample> commandPaletteSamples()
{
    QVector<GallerySample> samples{
        makeSample(QStringLiteral("command-palette-basic"), QStringLiteral("Find and run a command"),
            QStringLiteral(
                "Search a page name, choose with the arrow keys, and press Enter. Escape closes the palette."),
            QStringLiteral(R"sample(auto* layout = new QVBoxLayout(this);
auto* location = new Label(QStringLiteral("Current page: Home"), this);
auto* palette = new fluent::menus_toolbars::CommandPalette(this);
palette->setPlaceholderText(QStringLiteral("Search pages..."));
palette->setDim(true);
palette->setDimOpacity(0.12);
const QStringList pages{QStringLiteral("Home"), QStringLiteral("Inbox"),
    QStringLiteral("Documents"), QStringLiteral("Folders")};
for (const QString& page : pages) {
    auto* action = new QAction(page, this);
    palette->addCommand(action, QStringLiteral("Navigation"));
    QObject::connect(action, &QAction::triggered, location, [location, page] {
        location->setText(QStringLiteral("Current page: %1").arg(page));
    });
}
auto* open = new Button(QStringLiteral("Search pages"), this);
open->setFluentStyle(Button::Accent);
open->setIconGlyph(Typography::Icons::Search);
QObject::connect(open, &Button::clicked, palette, &fluent::menus_toolbars::CommandPalette::open);
layout->addWidget(open, 0, Qt::AlignLeft);
layout->addWidget(location);)sample"),
            [](QWidget* parent) {
                auto* root = sampleSurface(parent);
                auto* status = makeHintLabel(root,
                    QStringLiteral("Search a page name, move with Up/Down, and press Enter or Escape."));

                auto* panel = new CommandPreviewPanel(root);
                panel->setFixedSize(560, 88);
                auto* panelLayout = new QHBoxLayout(panel);
                panelLayout->setContentsMargins(16, 12, 16, 12);
                panelLayout->setSpacing(12);

                auto* pageIcon = new FontIcon(Typography::Icons::Home, panel);
                pageIcon->setIconSize(28);
                auto* pageText = samples::verticalGroup(panel, 1);
                pageText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                pageText->setObjectName(QStringLiteral("Gallery.CommandPreview.Text"));
                auto* location = makePreviewLabel(pageText, QStringLiteral("Current page: Home"),
                    Typography::FontRole::BodyStrong, Label::TextColorRole::Primary);
                location->setObjectName(QStringLiteral("Gallery.CommandPalette.BasicLocation"));
                auto* subtitle = makePreviewLabel(pageText,
                    QStringLiteral("Navigation · Home, Inbox, Documents, Folders"), Typography::FontRole::Caption,
                    Label::TextColorRole::Secondary);
                boxLayout(pageText)->addWidget(location);
                boxLayout(pageText)->addWidget(subtitle);

                auto* palette = new fluent::menus_toolbars::CommandPalette(root);
                palette->setObjectName(QStringLiteral("Gallery.CommandPalette.Basic"));
                palette->setPlaceholderText(QStringLiteral("Search pages..."));
                palette->setDim(true);
                palette->setDimOpacity(0.12);
                palette->setBackdropBlurRadius(8);

                const QStringList pages{QStringLiteral("Home"), QStringLiteral("Inbox"), QStringLiteral("Documents"),
                    QStringLiteral("Folders")};
                const QStringList glyphs{Typography::Icons::Home, Typography::Icons::Mail, Typography::Icons::Document,
                    Typography::Icons::Folder};
                for (int i = 0; i < pages.size(); ++i) {
                    const QString& page = pages.at(i);
                    const QString& glyph = glyphs.at(i);
                    auto* action = new QAction(page, root);
                    root->setActionGlyph(action, glyph);
                    palette->addCommand(action, QStringLiteral("Navigation"));
                    QObject::connect(action, &QAction::triggered, location, [location, pageIcon, page, glyph] {
                        location->setText(QStringLiteral("Current page: %1").arg(page));
                        pageIcon->setGlyph(glyph);
                    });
                }
                auto* open = new Button(QStringLiteral("Search pages"), panel);
                open->setObjectName(QStringLiteral("Gallery.CommandPalette.OpenBasic"));
                open->setFluentStyle(Button::Accent);
                open->setIconGlyph(Typography::Icons::Search);
                open->setMinimumWidth(124);
                QObject::connect(open, &Button::clicked, palette, &fluent::menus_toolbars::CommandPalette::open);

                panelLayout->addWidget(pageIcon);
                panelLayout->addWidget(pageText, 1);
                panelLayout->addWidget(open);
                boxLayout(root)->addWidget(panel);
                boxLayout(root)->addWidget(status);
                return root;
            }),
        makeSample(
            QStringLiteral("command-palette-shared-actions"), QStringLiteral("Shared menu and toolbar commands"),
            QStringLiteral(
                "Jump between Home, Inbox, Documents and Folders, or add, clear and sort notes. The menu and toolbar share the same actions."),
            QStringLiteral(
                "auto setActionGlyph = [this](QAction* action, const QString& glyph) {\n"
                "    QPixmap pixmap(20, 20);\n"
                "    pixmap.fill(Qt::transparent);\n"
                "    QPainter painter(&pixmap);\n"
                "    painter.setPen(this->palette().color(QPalette::WindowText));\n"
                "    Typography::Icons::paintGlyph(painter, pixmap.rect(), glyph, 16, Qt::AlignCenter);\n"
                "    painter.end();\n"
                "    action->setIcon(QIcon(pixmap));\n"
                "};\n"
                "auto* layout = new QVBoxLayout(this);\n"
                "auto* notes = new QStringListModel(this);\n"
                "auto* list = new fluent::collections::ListView(this);\n"
                "list->setAccessibleName(QStringLiteral(\"Notes\"));\n"
                "list->setModel(notes);\n"
                "list->setFixedHeight(104);\n"
                "list->setPlaceholderText(QStringLiteral(\"No notes yet. Run Add note to create one.\"));\n"
                "list->setBorderVisible(false);\n"
                "list->setBackgroundVisible(false);\n"
                "list->setProperty(\"fluentPreserveParentSurface\", true);\n"
                "auto* status = new Label(QStringLiteral(\"0 notes\"), this);\n"
                "status->setTextColorRole(Label::TextColorRole::Secondary);\n"
                "status->hide();\n"
                "auto* palette = new fluent::menus_toolbars::CommandPalette(this);\n"
                "palette->setPlaceholderText(QStringLiteral(\"Type a command or search…\"));\n"
                "palette->setDim(true);\n"
                "palette->setDimOpacity(0.12);\n"
                "palette->setBackdropBlurRadius(8);\n"
                "auto* location = new Label(QStringLiteral(\"Home\"), this);\n"
                "auto* menu = new FluentMenu(QStringLiteral(\"Workspace commands\"), this);\n"
                "auto* add = new QAction(QStringLiteral(\"Add note\"), this);\n"
                "add->setShortcut(QKeySequence(QStringLiteral(\"Ctrl+N\")));\n"
                "auto* clear = new QAction(QStringLiteral(\"Clear notes\"), this);\n"
                "clear->setEnabled(false);\n"
                "auto* details = new QAction(QStringLiteral(\"Show details\"), this);\n"
                "details->setCheckable(true);\n"
                "const auto updateCount = [list, clear, status]() {\n"
                "    const int count = list->model()->rowCount();\n"
                "    clear->setEnabled(count > 0);\n"
                "    status->setText(count == 1 ? QStringLiteral(\"1 note\") : QStringLiteral(\"%1 notes\").arg(count));\n"
                "};\n"
                "const QStringList locations{QStringLiteral(\"Home\"), QStringLiteral(\"Inbox\"), QStringLiteral(\"Documents\"), QStringLiteral(\"Folders\")};\n"
                "const QStringList glyphs{Typography::Icons::Home, Typography::Icons::Mail, Typography::Icons::Document, Typography::Icons::Folder};\n"
                "const QStringList keys{QStringLiteral(\"Ctrl+H\"), QStringLiteral(\"Ctrl+I\"), QStringLiteral(\"Ctrl+D\"), QStringLiteral(\"Ctrl+F\")};\n"
                "for (int i = 0; i < locations.size(); ++i) {\n"
                "    auto* model = i == 0 ? notes : new QStringListModel(QStringList{locations.at(i) + QStringLiteral(\" note\")}, this);\n"
                "    auto* navigate = new QAction(locations.at(i), this);\n"
                "    navigate->setShortcut(QKeySequence(keys.at(i)));\n"
                "    setActionGlyph(navigate, glyphs.at(i));\n"
                "    palette->addCommand(navigate, QStringLiteral(\"Navigation\"));\n"
                "    menu->addAction(navigate);\n"
                "    QObject::connect(model, &QAbstractItemModel::modelReset, this, updateCount);\n"
                "    QObject::connect(navigate, &QAction::triggered, this, [list, location, model, navigate, updateCount]() {\n"
                "        list->setModel(model);\n"
                "        location->setText(navigate->text());\n"
                "        updateCount();\n"
                "    });\n"
                "}\n"
                "menu->addSeparator();\n"
                "palette->addCommand(add, QStringLiteral(\"Notes\"), {QStringLiteral(\"new\"), QStringLiteral(\"create\")});\n"
                "palette->addCommand(clear, QStringLiteral(\"Notes\"),\n"
                "    {QStringLiteral(\"delete\"), QStringLiteral(\"remove\")});\n"
                "palette->addCommand(details, QStringLiteral(\"View\"),\n"
                "    {QStringLiteral(\"count\"), QStringLiteral(\"properties\")});\n"
                "setActionGlyph(add, Typography::Icons::Add);\n"
                "setActionGlyph(clear, Typography::Icons::Delete);\n"
                "setActionGlyph(details, Typography::Icons::Info);\n"
                "QObject::connect(add, &QAction::triggered, list, [list]() {\n"
                "    auto* notes = qobject_cast<QStringListModel*>(list->model());\n"
                "    QStringList items = notes->stringList();\n"
                "    items.append(QStringLiteral(\"Note %1\").arg(items.size() + 1));\n"
                "    notes->setStringList(items);\n"
                "});\n"
                "QObject::connect(clear, &QAction::triggered, list, [list]() {\n"
                "    qobject_cast<QStringListModel*>(list->model())->setStringList({});\n"
                "});\n"
                "auto* sort = new QAction(QStringLiteral(\"Sort notes by name\"), this);\n"
                "setActionGlyph(sort, Typography::Icons::Up);\n"
                "palette->addCommand(sort, QStringLiteral(\"View\"), {QStringLiteral(\"order\")});\n"
                "QObject::connect(sort, &QAction::triggered, list, [list]() { list->model()->sort(0); });\n"
                "QObject::connect(details, &QAction::toggled, status, &QWidget::setVisible);\n"
                "auto* bar = new CommandBar(this);\n"
                "bar->setAccessibleName(QStringLiteral(\"Notes commands\"));\n"
                "bar->setLabelPosition(CommandBar::LabelPosition::Right);\n"
                "bar->setBackgroundVisible(false);\n"
                "bar->addPrimaryAction(add);\n"
                "bar->addPrimaryAction(details);\n"
                "bar->addSecondaryAction(clear);\n"
                "menu->addAction(add);\n"
                "menu->addAction(details);\n"
                "menu->addAction(clear);\n"
                "menu->addAction(sort);\n"
                "auto* menuButton = new DropDownButton(QStringLiteral(\"Workspace commands\"), this);\n"
                "menuButton->setMenu(menu);\n"
                "auto* open = new Button(QStringLiteral(\"Open command palette\"), this);\n"
                "open->setFluentStyle(Button::Accent);\n"
                "open->setIconGlyph(Typography::Icons::Search);\n"
                "QObject::connect(open, &Button::clicked, palette, &fluent::menus_toolbars::CommandPalette::open);\n"
                "auto* modal = new fluent::basicinput::CheckBox(QStringLiteral(\"Modal\"), this);\n"
                "modal->setChecked(true);\n"
                "QObject::connect(modal, &QCheckBox::toggled, palette, &fluent::menus_toolbars::CommandPalette::setModal);\n"
                "auto* openRow = new QHBoxLayout;\n"
                "openRow->addWidget(open);\n"
                "openRow->addWidget(modal);\n"
                "openRow->addStretch();\n"
                "layout->addLayout(openRow);\n"
                "auto* commandRow = new QHBoxLayout;\n"
                "commandRow->addWidget(bar, 1);\n"
                "commandRow->addWidget(menuButton);\n"
                "layout->addLayout(commandRow);\n"
                "layout->addWidget(location);\n"
                "layout->addWidget(list);\n"
                "layout->addWidget(status);\n"),
            [](QWidget* parent) -> QWidget* {
                auto* surface = new MenusToolbarsSampleSurface(parent);
                auto* notes = new QStringListModel(surface);
                auto* list = new fluent::collections::ListView(surface);
                list->setObjectName(QStringLiteral("Gallery.CommandPalette.Notes"));
                list->setAccessibleName(QStringLiteral("Notes"));
                list->setModel(notes);
                list->setFixedHeight(104);
                list->setPlaceholderText(QStringLiteral("No notes yet. Run Add note to create one."));
                list->setBorderVisible(false);
                list->setBackgroundVisible(false);
                list->setProperty("fluentPreserveParentSurface", true);
                auto* status = new Label(QStringLiteral("0 notes"), surface);
                status->setObjectName(QStringLiteral("Gallery.CommandPalette.Status"));
                status->setTextColorRole(Label::TextColorRole::Secondary);
                status->hide();
                auto* palette = new fluent::menus_toolbars::CommandPalette(surface);
                palette->setObjectName(QStringLiteral("Gallery.CommandPalette.Shared"));
                palette->setPlaceholderText(QStringLiteral("Type a command or search…"));
                palette->setDim(true);
                palette->setDimOpacity(0.12);
                palette->setBackdropBlurRadius(8);
                auto* location = new Label(QStringLiteral("Home"), surface);
                location->setObjectName(QStringLiteral("Gallery.CommandPalette.Location"));
                auto* menu = new FluentMenu(QStringLiteral("Workspace commands"), surface);
                auto* add = new QAction(QStringLiteral("Add note"), surface);
                add->setShortcut(QKeySequence(QStringLiteral("Ctrl+N")));
                auto* clear = new QAction(QStringLiteral("Clear notes"), surface);
                clear->setEnabled(false);
                auto* details = new QAction(QStringLiteral("Show details"), surface);
                details->setCheckable(true);
                const auto updateCount = [list, clear, status]() {
                    const int count = list->model()->rowCount();
                    clear->setEnabled(count > 0);
                    status->setText(count == 1 ? QStringLiteral("1 note") : QStringLiteral("%1 notes").arg(count));
                };
                const QStringList locations{QStringLiteral("Home"), QStringLiteral("Inbox"),
                    QStringLiteral("Documents"), QStringLiteral("Folders")};
                const QStringList glyphs{Typography::Icons::Home, Typography::Icons::Mail, Typography::Icons::Document,
                    Typography::Icons::Folder};
                const QStringList keys{QStringLiteral("Ctrl+H"), QStringLiteral("Ctrl+I"), QStringLiteral("Ctrl+D"),
                    QStringLiteral("Ctrl+F")};
                for (int i = 0; i < locations.size(); ++i) {
                    auto* model = i == 0
                        ? notes
                        : new QStringListModel(QStringList{locations.at(i) + QStringLiteral(" note")}, surface);
                    auto* navigate = new QAction(locations.at(i), surface);
                    navigate->setShortcut(QKeySequence(keys.at(i)));
                    surface->setActionGlyph(navigate, glyphs.at(i));
                    palette->addCommand(navigate, QStringLiteral("Navigation"));
                    menu->addAction(navigate);
                    QObject::connect(model, &QAbstractItemModel::modelReset, surface, updateCount);
                    QObject::connect(navigate, &QAction::triggered, surface,
                        [list, location, model, navigate, updateCount]() {
                            list->setModel(model);
                            location->setText(navigate->text());
                            updateCount();
                        });
                }
                menu->addSeparator();
                palette->addCommand(add, QStringLiteral("Notes"), {QStringLiteral("new"), QStringLiteral("create")});
                palette->addCommand(clear, QStringLiteral("Notes"),
                    {QStringLiteral("delete"), QStringLiteral("remove")});
                palette->addCommand(details, QStringLiteral("View"),
                    {QStringLiteral("count"), QStringLiteral("properties")});
                surface->setActionGlyph(add, Typography::Icons::Add);
                surface->setActionGlyph(clear, Typography::Icons::Delete);
                surface->setActionGlyph(details, Typography::Icons::Info);
                QObject::connect(add, &QAction::triggered, list, [list]() {
                    auto* notes = qobject_cast<QStringListModel*>(list->model());
                    QStringList items = notes->stringList();
                    items.append(QStringLiteral("Note %1").arg(items.size() + 1));
                    notes->setStringList(items);
                });
                QObject::connect(clear, &QAction::triggered, list, [list]() {
                    qobject_cast<QStringListModel*>(list->model())->setStringList({});
                });
                auto* sort = new QAction(QStringLiteral("Sort notes by name"), surface);
                surface->setActionGlyph(sort, Typography::Icons::Up);
                palette->addCommand(sort, QStringLiteral("View"), {QStringLiteral("order")});
                QObject::connect(sort, &QAction::triggered, list, [list]() { list->model()->sort(0); });
                QObject::connect(details, &QAction::toggled, status, &QWidget::setVisible);
                auto* bar = new CommandBar(surface);
                bar->setAccessibleName(QStringLiteral("Notes commands"));
                bar->setLabelPosition(CommandBar::LabelPosition::Right);
                bar->setBackgroundVisible(false);
                bar->addPrimaryAction(add);
                bar->addPrimaryAction(details);
                bar->addSecondaryAction(clear);
                menu->addAction(add);
                menu->addAction(details);
                menu->addAction(clear);
                menu->addAction(sort);
                auto* menuButton = new DropDownButton(QStringLiteral("Workspace commands"), surface);
                menuButton->setMenu(menu);
                auto* open = new Button(QStringLiteral("Open command palette"), surface);
                open->setObjectName(QStringLiteral("Gallery.CommandPalette.Open"));
                open->setFluentStyle(Button::Accent);
                open->setIconGlyph(Typography::Icons::Search);
                QObject::connect(open, &Button::clicked, palette, &fluent::menus_toolbars::CommandPalette::open);
                auto* modal = new fluent::basicinput::CheckBox(QStringLiteral("Modal"), surface);
                modal->setChecked(true);
                QObject::connect(modal, &QCheckBox::toggled, palette,
                    &fluent::menus_toolbars::CommandPalette::setModal);
                auto* openRow = new QHBoxLayout;
                openRow->addWidget(open);
                openRow->addWidget(modal);
                openRow->addStretch();
                boxLayout(surface)->addLayout(openRow);
                auto* commandRow = new QHBoxLayout;
                commandRow->addWidget(bar, 1);
                commandRow->addWidget(menuButton);
                boxLayout(surface)->addLayout(commandRow);
                boxLayout(surface)->addWidget(location);
                boxLayout(surface)->addWidget(list);
                boxLayout(surface)->addWidget(status);
                return surface;
            },
            true),
        makeSample(
            QStringLiteral("command-palette-editing-context"), QStringLiteral("Copy or cut text through search"),
            QStringLiteral(
                "Search Copy to copy the selected word, or Cut to remove it from the field. Reset text lets you try again."),
            QStringLiteral(
                "auto* layout = new QVBoxLayout(this);\n"
                "auto* editor = new LineEdit(this);\n"
                "editor->setText(\"Review the release notes\");\n"
                "editor->setPlaceholderText(\"Type text or reset the sample\");\n"
                "editor->setAccessibleName(\"Text to edit\");\n"
                "auto* router = new EditingCommandRouter(window());\n"
                "auto* palette = new CommandPalette(this);\n"
                "palette->setPlaceholderText(\"Search Copy, Cut, or Select All\");\n"
                "palette->addCommand(router->action(EditingCommandRouter::Command::Copy), \"Edit\", {\"copy\"});\n"
                "palette->addCommand(router->action(EditingCommandRouter::Command::Cut), \"Edit\", {\"cut\"});\n"
                "palette->addCommand(router->action(EditingCommandRouter::Command::SelectAll), \"Edit\", {\"select all\"});\n"
                "auto* open = new Button(\"Search editing commands\", this);\n"
                "open->setFocusPolicy(Qt::NoFocus);\n"
                "QObject::connect(open, &Button::clicked, palette, [editor, palette]() {\n"
                "    editor->setFocus();\n"
                "    if (!editor->hasSelectedText())\n"
                "        editor->setSelection(0, qMin(6, editor->text().size()));\n"
                "    palette->setQuery(QString());\n"
                "    palette->setCurrentAction(palette->commands().first());\n"
                "    palette->open();\n"
                "});\n"
                "QObject::connect(editor, &QLineEdit::textChanged, open, [open](const QString& text) { open->setEnabled(!text.isEmpty()); });\n"
                "auto* reset = new Button(\"Reset text\", this);\n"
                "QObject::connect(reset, &Button::clicked, editor, [editor]() {\n"
                "    editor->setText(\"Review the release notes\");\n"
                "    editor->setFocus();\n"
                "});\n"
                "layout->addWidget(editor);\n"
                "auto* buttons = new QHBoxLayout;\n"
                "buttons->addWidget(open);\n"
                "buttons->addWidget(reset);\n"
                "buttons->addStretch();\n"
                "layout->addLayout(buttons);"),
            [](QWidget* parent) -> QWidget* {
                auto* surface = new MenusToolbarsSampleSurface(parent);
                auto* editor = new LineEdit(surface);
                editor->setObjectName(QStringLiteral("Gallery.CommandPalette.Editor"));
                editor->setText(QStringLiteral("Review the release notes"));
                editor->setPlaceholderText(QStringLiteral("Type text or reset the sample"));
                editor->setAccessibleName(QStringLiteral("Text to edit"));
                auto* router = galleryWindowEditingCommandRouter(surface);
                auto* palette = new fluent::menus_toolbars::CommandPalette(surface);
                palette->setObjectName(QStringLiteral("Gallery.CommandPalette.Editing"));
                palette->setPlaceholderText(QStringLiteral("Search Copy, Cut, or Select All"));
                palette->addCommand(router->action(EditingCommandRouter::Command::Copy), QStringLiteral("Edit"),
                    {QStringLiteral("copy")});
                palette->addCommand(router->action(EditingCommandRouter::Command::Cut), QStringLiteral("Edit"),
                    {QStringLiteral("cut")});
                palette->addCommand(router->action(EditingCommandRouter::Command::SelectAll), QStringLiteral("Edit"),
                    {QStringLiteral("select all")});
                auto* open = new Button(QStringLiteral("Search editing commands"), surface);
                open->setObjectName(QStringLiteral("Gallery.CommandPalette.OpenEditing"));
                open->setFocusPolicy(Qt::NoFocus);
                QObject::connect(open, &Button::clicked, palette, [editor, palette]() {
                    editor->setFocus();
                    if (!editor->hasSelectedText())
                        editor->setSelection(0, qMin(6, editor->text().size()));
                    palette->setQuery(QString());
                    palette->setCurrentAction(palette->commands().first());
                    palette->open();
                });
                QObject::connect(editor, &QLineEdit::textChanged, open, [open](const QString& text) {
                    open->setEnabled(!text.isEmpty());
                });
                auto* reset = new Button(QStringLiteral("Reset text"), surface);
                reset->setObjectName(QStringLiteral("Gallery.CommandPalette.ResetEditing"));
                QObject::connect(reset, &Button::clicked, editor, [editor]() {
                    editor->setText(QStringLiteral("Review the release notes"));
                    editor->setFocus();
                });
                boxLayout(surface)->addWidget(editor);
                auto* buttons = new QHBoxLayout;
                buttons->addWidget(open);
                buttons->addWidget(reset);
                buttons->addStretch();
                boxLayout(surface)->addLayout(buttons);
                return surface;
            },
            true),
    };
    auto& workspace = samples[1];
    workspace.usageSnippet = QStringLiteral(
        "// The workspace actions, menu and toolbar are defined in Full example.\n"
        "auto* palette = new fluent::menus_toolbars::CommandPalette(this);\n"
        "palette->setModal(true);\n"
        "palette->setPlaceholderText(\"Type a command or search…\");\n"
        "for (QAction* action : menu->actions()) {\n"
        "    if (!action->isSeparator())\n"
        "        palette->addCommand(action, \"Navigation\");\n"
        "}\n"
        "// Register metadata for the same actions; execution stays QAction-owned.\n"
        "palette->addCommand(add, \"Notes\", {\"new\", \"create\"});\n"
        "palette->addCommand(clear, \"Notes\", {\"delete\", \"remove\"});\n"
        "palette->addCommand(details, \"View\", {\"count\", \"properties\"});\n"
        "palette->addCommand(sort, \"View\", {\"order\"});\n"
        "QObject::connect(open, &Button::clicked, palette,\n"
        "    &fluent::menus_toolbars::CommandPalette::open);\n");
    workspace.sourceFiles = {
        {QStringLiteral("Workspace.cpp"), QStringLiteral("cpp"), workspace.codeSnippet, workspace.usageSnippet}};
    return samples;
}

QVector<GallerySample> commandBarSamples()
{
    return {
        makeSample(QStringLiteral("command-bar-responsive-overflow"),
            QStringLiteral("A responsive document command strip"),
            QStringLiteral(
                "Try priority-aware overflow, icon or right-side labels, primary and secondary sections, separators, and an optional command-bar background."),
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
                "});")
                + QStringLiteral(R"(
using fluent::status_info::Toast;
for (QAction* action : bar->primaryActions() + bar->secondaryActions()) {
    if (action->isSeparator()) continue;
    connect(action, &QAction::triggered, this, [this, action] {
        QString text = action->text().section('\t', 0, 0).remove('&');
        Toast::showOrUpdateToast(this, "gallery.action-feedback", text + " selected",
            Toast::Informational, 2200, Toast::Top, QMargins(16, 50, 16, 16));
    });
})"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* status = makeHintLabel(surface, QStringLiteral("Full width · all primary commands are visible"));

                auto* panel = new CommandPreviewPanel(surface);
                panel->setFixedSize(560, 132);
                auto* panelLayout = new QVBoxLayout(panel);
                panelLayout->setContentsMargins(12, 10, 12, 12);
                panelLayout->setSpacing(8);

                auto* barHost = new QWidget(panel);
                barHost->setObjectName(QStringLiteral("Gallery.CommandBar.Host"));
                barHost->setFixedSize(536, 40);
                auto* hostLayout = new QHBoxLayout(barHost);
                hostLayout->setContentsMargins(0, 0, 0, 0);
                hostLayout->setSpacing(0);

                auto* bar = new CommandBar(barHost);
                bar->setObjectName(QStringLiteral("Gallery.CommandBar.Responsive"));
                bar->setAccessibleName(QStringLiteral("Document commands"));
                bar->setLabelPosition(CommandBar::LabelPosition::Right);
                bar->setBackgroundVisible(false);
                hostLayout->addWidget(bar);

                const auto makeAction = [surface](const QString& text, const QString& glyph) {
                    auto* action = new QAction(text, surface);
                    surface->setActionGlyph(action, glyph);
                    QObject::connect(action, &QAction::triggered, surface, [surface, text]() {
                        showGalleryToast(surface, QStringLiteral("%1 selected").arg(text),
                            status_info::Toast::Informational);
                    });
                    return action;
                };
                QAction* addAction = makeAction(QStringLiteral("Add"), Typography::Icons::Add);
                QAction* editAction = makeAction(QStringLiteral("Edit"), Typography::Icons::Edit);
                editAction->setPriority(QAction::HighPriority);
                QAction* shareAction = makeAction(QStringLiteral("Share"), Typography::Icons::Share);
                auto* separator = new QAction(surface);
                separator->setSeparator(true);
                QAction* syncAction = makeAction(QStringLiteral("Sync"), Typography::Icons::Sync);
                syncAction->setPriority(QAction::LowPriority);
                QAction* pinAction = makeAction(QStringLiteral("Pin"), Typography::Icons::Pin);
                pinAction->setCheckable(true);
                QAction* settingsAction = makeAction(QStringLiteral("Settings"), Typography::Icons::Settings);
                QAction* helpAction = makeAction(QStringLiteral("Help"), Typography::Icons::Info);

                bar->addPrimaryAction(addAction);
                bar->addPrimaryAction(editAction);
                bar->addPrimaryAction(shareAction);
                bar->addPrimaryAction(separator);
                bar->addPrimaryAction(syncAction);
                bar->addPrimaryAction(pinAction);
                bar->addSecondaryAction(settingsAction);
                bar->addSecondaryAction(helpAction);

                auto* documentRow = horizontalGroup(panel, 10);
                auto* documentIcon = new FontIcon(Typography::Icons::Document, documentRow);
                documentIcon->setIconSize(28);
                auto* documentText = samples::verticalGroup(documentRow, 1);
                documentText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                documentText->setObjectName(QStringLiteral("Gallery.CommandPreview.Text"));
                auto* documentTitle = makePreviewLabel(documentText, QStringLiteral("Quarterly report"),
                    Typography::FontRole::BodyStrong, Label::TextColorRole::Primary);
                auto* documentMeta = makePreviewLabel(documentText,
                    QStringLiteral("Edited just now · shared with 4 people"), Typography::FontRole::Caption,
                    Label::TextColorRole::Secondary);
                boxLayout(documentText)->addWidget(documentTitle);
                boxLayout(documentText)->addWidget(documentMeta);
                boxLayout(documentRow)->addWidget(documentIcon);
                boxLayout(documentRow)->addWidget(documentText);
                panelLayout->addWidget(barHost);
                panelLayout->addWidget(documentRow);

                auto* controls = horizontalGroup(surface, 8);
                auto* compact = sampleButton(controls, QStringLiteral("Compact view"));
                compact->setMinimumWidth(112);
                QObject::connect(compact, &Button::clicked, barHost, [barHost, compact, status]() {
                    const bool compactMode = barHost->width() > 300;
                    barHost->setFixedWidth(compactMode ? 288 : 536);
                    compact->setText(compactMode ? QStringLiteral("Full view") : QStringLiteral("Compact view"));
                    status->setText(compactMode
                            ? QStringLiteral("Compact width · lower-priority commands moved to More")
                            : QStringLiteral("Full width · all primary commands are visible"));
                });
                boxLayout(controls)->addWidget(compact);

                auto* labels = sampleButton(controls, QStringLiteral("Labels: Right"));
                labels->setMinimumWidth(112);
                QObject::connect(labels, &Button::clicked, bar, [bar, labels, status]() {
                    const bool showLabels = bar->labelPosition() == CommandBar::LabelPosition::Collapsed;
                    bar->setLabelPosition(
                        showLabels ? CommandBar::LabelPosition::Right : CommandBar::LabelPosition::Collapsed);
                    labels->setText(showLabels ? QStringLiteral("Labels: Right") : QStringLiteral("Labels: Icons"));
                    status->setText(showLabels
                            ? QStringLiteral("Labels on the right · text remains available beside icons")
                            : QStringLiteral("Icons only · accessible names still come from QAction text"));
                });
                boxLayout(controls)->addWidget(labels);

                auto* background = sampleButton(controls, QStringLiteral("Show background"));
                background->setMinimumWidth(128);
                QObject::connect(background, &Button::clicked, bar, [bar, background, status]() {
                    const bool visible = !bar->backgroundVisible();
                    bar->setBackgroundVisible(visible);
                    background->setText(
                        visible ? QStringLiteral("Hide background") : QStringLiteral("Show background"));
                    status->setText(visible
                            ? QStringLiteral("Background on · use the self-contained command surface")
                            : QStringLiteral("Background off · blend the commands into their host"));
                });
                boxLayout(controls)->addWidget(background);

                boxLayout(surface)->addWidget(panel);
                boxLayout(surface)->addWidget(controls);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(QStringLiteral("command-bar-editing-router"), QStringLiteral("Context-aware editing commands"),
            QStringLiteral(
                "Undo and Redo are the first two labeled commands. Edit the note to enable them, then select text or toggle read-only to watch the shared editing actions update."),
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
                auto* status = makeHintLabel(surface,
                    QStringLiteral("Edit the note to enable Undo · select text to enable Cut and Copy"));
                auto* panel = new CommandPreviewPanel(surface);
                panel->setFixedSize(560, 154);
                auto* panelLayout = new QVBoxLayout(panel);
                panelLayout->setContentsMargins(12, 10, 12, 12);
                panelLayout->setSpacing(8);

                auto* heading = horizontalGroup(panel, 8);
                auto* headingIcon = new FontIcon(Typography::Icons::Edit, heading);
                headingIcon->setIconSize(20);
                auto* headingLabel = makePreviewLabel(heading, QStringLiteral("Quick note"),
                    Typography::FontRole::BodyStrong, Label::TextColorRole::Primary);
                boxLayout(heading)->addWidget(headingIcon);
                boxLayout(heading)->addWidget(headingLabel);

                auto* editor = new LineEdit(panel);
                editor->setObjectName(QStringLiteral("Gallery.CommandBar.EditingTarget"));
                editor->setText(QStringLiteral("Review the release notes before Friday"));
                editor->setFixedWidth(536);

                auto* router = galleryWindowEditingCommandRouter(surface);
                auto* bar = new CommandBar(panel);
                bar->setObjectName(QStringLiteral("Gallery.CommandBar.EditingRouter"));
                bar->setAccessibleName(QStringLiteral("Editing commands"));
                bar->setLabelPosition(CommandBar::LabelPosition::Right);
                bar->setBackgroundVisible(false);
                bar->setFixedWidth(536);
                auto* separator = new QAction(surface);
                separator->setSeparator(true);
                bar->addPrimaryAction(router->action(EditingCommandRouter::Command::Undo));
                bar->addPrimaryAction(router->action(EditingCommandRouter::Command::Redo));
                bar->addPrimaryAction(separator);
                bar->addPrimaryAction(router->action(EditingCommandRouter::Command::Cut));
                bar->addPrimaryAction(router->action(EditingCommandRouter::Command::Copy));
                bar->addPrimaryAction(router->action(EditingCommandRouter::Command::Paste));
                bar->addSecondaryAction(router->action(EditingCommandRouter::Command::Delete));
                bar->addSecondaryAction(router->action(EditingCommandRouter::Command::SelectAll));

                const auto setRouterGlyph =
                    [surface, router](EditingCommandRouter::Command command, const QString& glyph) {
                        surface->setActionGlyph(router->action(command), glyph);
                    };
                setRouterGlyph(EditingCommandRouter::Command::Undo, Typography::Icons::Undo);
                setRouterGlyph(EditingCommandRouter::Command::Redo, Typography::Icons::Redo);
                setRouterGlyph(EditingCommandRouter::Command::Cut, Typography::Icons::Cut);
                setRouterGlyph(EditingCommandRouter::Command::Copy, Typography::Icons::Copy);
                setRouterGlyph(EditingCommandRouter::Command::Paste, Typography::Icons::Paste);
                setRouterGlyph(EditingCommandRouter::Command::Delete, Typography::Icons::Delete);
                setRouterGlyph(EditingCommandRouter::Command::SelectAll, Typography::Icons::SelectAll);

                panelLayout->addWidget(heading);
                panelLayout->addWidget(editor);
                panelLayout->addWidget(bar);

                auto* controls = horizontalGroup(surface, 8);
                auto* selectText = sampleButton(controls, QStringLiteral("Select text"));
                auto* clearSelection = sampleButton(controls, QStringLiteral("Clear selection"));
                auto* readOnly = sampleButton(controls, QStringLiteral("Read-only: Off"));
                readOnly->setCheckable(true);

                using Command = EditingCommandRouter::Command;
                const auto availabilityText = [router, editor]() {
                    const auto state = [](bool enabled) {
                        return enabled ? QStringLiteral("on") : QStringLiteral("off");
                    };
                    return QStringLiteral("Cut: %1 · Copy: %2 · Paste: %3 · Read-only: %4")
                        .arg(state(router->canExecute(Command::Cut)))
                        .arg(state(router->canExecute(Command::Copy)))
                        .arg(state(router->canExecute(Command::Paste)))
                        .arg(state(editor->isReadOnly()));
                };
                const auto updateAvailability = [status, availabilityText]() { status->setText(availabilityText()); };
                QObject::connect(router, &EditingCommandRouter::activeTargetChanged, status,
                    [updateAvailability](bool) { updateAvailability(); });
                QObject::connect(router, &EditingCommandRouter::commandCapabilityChanged, status,
                    [updateAvailability](Command, bool) { updateAvailability(); });

                QObject::connect(selectText, &Button::clicked, editor, [editor, router, updateAvailability]() {
                    QTimer::singleShot(0, editor, [editor, router, updateAvailability]() {
                        editor->setFocus(Qt::OtherFocusReason);
                        editor->selectAll();
                        router->refresh();
                        updateAvailability();
                    });
                });
                QObject::connect(clearSelection, &Button::clicked, editor, [editor, router, updateAvailability]() {
                    QTimer::singleShot(0, editor, [editor, router, updateAvailability]() {
                        editor->setFocus(Qt::OtherFocusReason);
                        editor->deselect();
                        router->refresh();
                        updateAvailability();
                    });
                });
                QObject::connect(readOnly, &QAbstractButton::toggled, editor,
                    [editor, router, readOnly, updateAvailability](bool checked) {
                        readOnly->setText(checked ? QStringLiteral("Read-only: On") : QStringLiteral("Read-only: Off"));
                        QTimer::singleShot(0, editor, [editor, router, checked, updateAvailability]() {
                            editor->setFocus(Qt::OtherFocusReason);
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
            }),
        makeSample(QStringLiteral("command-bar-submenus"), QStringLiteral("Three levels of view commands"),
            QStringLiteral(
                "Open View, Sort by, then Order. Checked choices stay shared between the primary command and the secondary Sort by entry."),
            QStringLiteral(
                "auto* status = new Label(\"Order: Ascending\", this);\n"
                "auto* view = new FluentMenu(\"View\", this);\n"
                "auto* sort = new FluentMenu(\"Sort by\", view);\n"
                "auto* order = new FluentMenu(\"Order\", sort);\n"
                "view->addMenu(sort);\n"
                "sort->addMenu(order);\n"
                "auto* group = new QActionGroup(this);\n"
                "auto* ascending = order->addAction(\"Ascending\");\n"
                "auto* descending = order->addAction(\"Descending\");\n"
                "ascending->setCheckable(true);\n"
                "descending->setCheckable(true);\n"
                "group->addAction(ascending);\n"
                "group->addAction(descending);\n"
                "ascending->setChecked(true);\n"
                "connect(group, &QActionGroup::triggered, status,\n"
                "    [status](QAction* action) {\n"
                "        status->setText(\"Order: \" + action->text());\n"
                "    });\n"
                "auto* bar = new CommandBar(this);\n"
                "bar->setLabelPosition(CommandBar::LabelPosition::Right);\n"
                "bar->setFixedWidth(480);\n"
                "bar->addPrimaryAction(view->menuAction());\n"
                "bar->addSecondaryAction(sort->menuAction());\n"
                "auto* compact = new Button(\"Compact width\", this);\n"
                "compact->setCheckable(true);\n"
                "connect(compact, &Button::toggled, bar, [bar](bool checked) {\n"
                "    bar->setFixedWidth(checked ? 80 : 480);\n"
                "});\n"
                "auto* layout = new QVBoxLayout(this);\n"
                "layout->addWidget(bar, 0, Qt::AlignLeft);\n"
                "layout->addWidget(compact, 0, Qt::AlignLeft);\n"
                "layout->addWidget(status);\n"),
            [](QWidget* parent) { return commandSubmenusPreview(parent, false); })};
}

QVector<GallerySample> commandBarFlyoutSamples()
{
    return {
        makeSample(QStringLiteral("command-bar-flyout-show-modes"), QStringLiteral("Contextual actions for a photo"),
            QStringLiteral(
                "Click for a focus-preserving Transient toolbar; right-click or use the keyboard for an expanded, keyboard-focused Standard context menu."),
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
                "});")
                + QStringLiteral(R"(
using fluent::status_info::Toast;
for (QAction* action : flyout->primaryActions() + flyout->secondaryActions()) {
    if (action->isSeparator()) continue;
    connect(action, &QAction::triggered, this, [this, action] {
        QString text = action->text().section('\t', 0, 0).remove('&');
        Toast::showOrUpdateToast(this, "gallery.action-feedback", text + " selected",
            Toast::Informational, 2200, Toast::Top, QMargins(16, 50, 16, 16));
    });
})"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* status = makeHintLabel(surface,
                    QStringLiteral("Click: Transient · right-click or keyboard: Standard"));
                auto* tile = new ContextMediaTile(surface);

                auto* flyout = new CommandBarFlyout(surface);
                flyout->setObjectName(QStringLiteral("Gallery.CommandBarFlyout"));
                auto* share = new QAction(QStringLiteral("Share"), surface);
                auto* save = new QAction(QStringLiteral("Save"), surface);
                auto* deleteAction = new QAction(QStringLiteral("Delete"), surface);
                auto* resizeAction = new QAction(QStringLiteral("Resize"), surface);
                auto* moveAction = new QAction(QStringLiteral("Move"), surface);
                flyout->addPrimaryAction(share);
                flyout->addPrimaryAction(save);
                flyout->addPrimaryAction(deleteAction);
                flyout->addSecondaryAction(resizeAction);
                flyout->addSecondaryAction(moveAction);

                surface->setActionGlyph(share, Typography::Icons::Share);
                surface->setActionGlyph(save, Typography::Icons::Save);
                surface->setActionGlyph(deleteAction, Typography::Icons::Delete);
                surface->setActionGlyph(resizeAction, Typography::Icons::FullScreen);
                surface->setActionGlyph(moveAction, Typography::Icons::Forward);

                const auto connectFeedback = [surface](QAction* action) {
                    QObject::connect(action, &QAction::triggered, surface, [surface, action]() {
                        showGalleryToast(surface, QStringLiteral("%1 selected").arg(action->text()),
                            status_info::Toast::Informational);
                    });
                };
                connectFeedback(share);
                connectFeedback(save);
                connectFeedback(deleteAction);
                connectFeedback(resizeAction);
                connectFeedback(moveAction);

                tile->setInvokeHandler([flyout, tile, status](const QPoint& localPosition, bool standard) {
                    flyout->setAlwaysExpanded(false);
                    if (standard) {
                        flyout->showAtPoint(tile, localPosition, CommandBarFlyout::ShowMode::Standard);
                        status->setText(QStringLiteral("Standard · expanded and keyboard focused"));
                        return;
                    }
                    flyout->showAt(tile, CommandBarFlyout::ShowMode::Transient);
                    status->setText(QStringLiteral("Transient · collapsed and focus preserved"));
                });

                boxLayout(surface)->addWidget(tile);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(QStringLiteral("command-bar-flyout-always-expanded"),
            QStringLiteral("Keep secondary commands visible"),
            QStringLiteral(
                "AlwaysExpanded removes the More step and keeps secondary commands visible even in Transient mode; turn it off to compare the collapsed presentation."),
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
                "        &CommandBarFlyout::setAlwaysExpanded);")
                + QStringLiteral(R"(
using fluent::status_info::Toast;
for (QAction* action : flyout->primaryActions() + flyout->secondaryActions()) {
    if (action->isSeparator()) continue;
    connect(action, &QAction::triggered, this, [this, action] {
        QString text = action->text().section('\t', 0, 0).remove('&');
        Toast::showOrUpdateToast(this, "gallery.action-feedback", text + " selected",
            Toast::Informational, 2200, Toast::Top, QMargins(16, 50, 16, 16));
    });
})"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* status = makeHintLabel(surface,
                    QStringLiteral("Always expanded · primary and secondary commands open together"));

                auto* panel = new CommandPreviewPanel(surface);
                panel->setFixedSize(560, 88);
                auto* panelLayout = new QHBoxLayout(panel);
                panelLayout->setContentsMargins(16, 12, 16, 12);
                panelLayout->setSpacing(12);

                auto* fileIcon = new FontIcon(Typography::Icons::Document, panel);
                fileIcon->setIconSize(28);
                auto* fileText = samples::verticalGroup(panel, 1);
                fileText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                fileText->setObjectName(QStringLiteral("Gallery.CommandPreview.Text"));
                auto* fileTitle = makePreviewLabel(fileText, QStringLiteral("Release-notes.md"),
                    Typography::FontRole::BodyStrong, Label::TextColorRole::Primary);
                auto* fileMeta = makePreviewLabel(fileText, QStringLiteral("Markdown · 18 KB"),
                    Typography::FontRole::Caption, Label::TextColorRole::Secondary);
                boxLayout(fileText)->addWidget(fileTitle);
                boxLayout(fileText)->addWidget(fileMeta);

                auto* open = sampleButton(panel, QStringLiteral("Open actions"));
                open->setObjectName(QStringLiteral("Gallery.CommandBarFlyout.OpenAlwaysExpanded"));
                panelLayout->addWidget(fileIcon);
                panelLayout->addWidget(fileText, 1);
                panelLayout->addWidget(open);

                auto* flyout = new CommandBarFlyout(surface);
                flyout->setObjectName(QStringLiteral("Gallery.CommandBarFlyout.AlwaysExpanded"));
                flyout->setAlwaysExpanded(true);
                auto* copyLink = new QAction(QStringLiteral("Copy link"), surface);
                auto* favorite = new QAction(QStringLiteral("Favorite"), surface);
                favorite->setCheckable(true);
                auto* rename = new QAction(QStringLiteral("Rename"), surface);
                auto* properties = new QAction(QStringLiteral("Properties"), surface);
                flyout->addPrimaryAction(copyLink);
                flyout->addPrimaryAction(favorite);
                flyout->addSecondaryAction(rename);
                flyout->addSecondaryAction(properties);
                surface->setActionGlyph(copyLink, Typography::Icons::Link);
                surface->setActionGlyph(favorite, Typography::Icons::FavoriteStar);
                surface->setActionGlyph(rename, Typography::Icons::Edit);
                surface->setActionGlyph(properties, Typography::Icons::Info);

                auto* controls = horizontalGroup(surface, 8);
                auto* alwaysExpanded = sampleButton(controls, QStringLiteral("Always expanded: On"));
                alwaysExpanded->setCheckable(true);
                alwaysExpanded->setChecked(true);
                QObject::connect(alwaysExpanded, &QAbstractButton::toggled, flyout,
                    [flyout, alwaysExpanded, status](bool checked) {
                        flyout->setAlwaysExpanded(checked);
                        alwaysExpanded->setText(
                            checked ? QStringLiteral("Always expanded: On") : QStringLiteral("Always expanded: Off"));
                        status->setText(checked
                                ? QStringLiteral("Always expanded · primary and secondary commands open together")
                                : QStringLiteral("Collapsed · secondary commands are available behind More"));
                    });
                QObject::connect(open, &Button::clicked, flyout, [flyout, open, status]() {
                    flyout->showAt(open, CommandBarFlyout::ShowMode::Transient);
                    status->setText(flyout->isAlwaysExpanded()
                            ? QStringLiteral("Transient + AlwaysExpanded · focus stays on Open actions")
                            : QStringLiteral(
                                  "Transient · focus stays on Open actions; use More for secondary commands"));
                });
                for (QAction* action : flyout->primaryActions() + flyout->secondaryActions()) {
                    QObject::connect(action, &QAction::triggered, surface, [surface, action]() {
                        showGalleryToast(surface, QStringLiteral("%1 selected").arg(displayActionText(action->text())),
                            status_info::Toast::Informational);
                    });
                }

                boxLayout(controls)->addWidget(alwaysExpanded);
                boxLayout(surface)->addWidget(panel);
                boxLayout(surface)->addWidget(controls);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(QStringLiteral("command-bar-flyout-submenus"), QStringLiteral("Three levels of view commands"),
            QStringLiteral(
                "Open View, Sort by, then Order. Checked choices stay shared between the primary command and the secondary Sort by entry."),
            QStringLiteral(
                "auto* status = new Label(\"Order: Ascending\", this);\n"
                "auto* view = new FluentMenu(\"View\", this);\n"
                "auto* sort = new FluentMenu(\"Sort by\", view);\n"
                "auto* order = new FluentMenu(\"Order\", sort);\n"
                "view->addMenu(sort);\n"
                "sort->addMenu(order);\n"
                "auto* group = new QActionGroup(this);\n"
                "auto* ascending = order->addAction(\"Ascending\");\n"
                "auto* descending = order->addAction(\"Descending\");\n"
                "ascending->setCheckable(true);\n"
                "descending->setCheckable(true);\n"
                "group->addAction(ascending);\n"
                "group->addAction(descending);\n"
                "ascending->setChecked(true);\n"
                "connect(group, &QActionGroup::triggered, status,\n"
                "    [status](QAction* action) {\n"
                "        status->setText(\"Order: \" + action->text());\n"
                "    });\n"
                "auto* flyout = new CommandBarFlyout(this);\n"
                "flyout->setAlwaysExpanded(true);\n"
                "flyout->addPrimaryAction(view->menuAction());\n"
                "flyout->addSecondaryAction(sort->menuAction());\n"
                "auto* open = new Button(\"Open view commands\", this);\n"
                "connect(open, &Button::clicked, flyout,\n"
                "    [flyout, open] { flyout->showAt(open); });\n"),
            [](QWidget* parent) { return commandSubmenusPreview(parent, true); })};
}

} // namespace

QVector<GallerySample> menusToolbarsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("menu"))
        return menuSamples();
    if (routeId == QStringLiteral("menu-bar"))
        return menuBarSamples();
    if (routeId == QStringLiteral("command-palette"))
        return commandPaletteSamples();
    if (routeId == QStringLiteral("command-bar"))
        return commandBarSamples();
    if (routeId == QStringLiteral("command-bar-flyout"))
        return commandBarFlyoutSamples();
    return {};
}

} // namespace fluent::gallery

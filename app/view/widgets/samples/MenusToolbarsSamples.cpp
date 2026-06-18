#include "MenusToolbarsSamples.h"

#include <QActionGroup>
#include <QBoxLayout>
#include <QKeySequence>
#include <QPainter>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/basicinput/DropDownButton.h"
#include "components/menus_toolbars/Menu.h"
#include "components/menus_toolbars/MenuBar.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::basicinput::DropDownButton;
using fluent::menus_toolbars::FluentMenu;
using fluent::menus_toolbars::FluentMenuBar;
using fluent::textfields::Label;
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
};

QWidget* sampleSurface(QWidget* parent, int spacing = 12)
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

} // namespace

QVector<GallerySample> menusToolbarsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("menu"))
        return menuSamples();
    if (routeId == QStringLiteral("menu-bar"))
        return menuBarSamples();
    return {};
}

} // namespace fluent::gallery

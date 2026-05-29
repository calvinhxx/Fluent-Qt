#include <gtest/gtest.h>

#include <QActionGroup>
#include <QApplication>
#include <QFontMetrics>
#include <QImage>
#include <QKeyEvent>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>

#include "design/Spacing.h"
#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/menus_toolbars/Menu.h"
#include "components/menus_toolbars/MenuBar.h"
#include "components/textfields/Label.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::menus_toolbars::FluentMenu;
using fluent::menus_toolbars::FluentMenuBar;
using fluent::menus_toolbars::FluentMenuItem;
using fluent::textfields::Label;

namespace {

class MenuBarTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

void showAndProcess(QWidget& widget)
{
    if (widget.window() && widget.window() != &widget)
        widget.window()->show();
    widget.show();
    QApplication::processEvents();
}

void addAnchored(AnchorLayout* layout, QWidget* widget)
{
    layout->addWidget(widget);
}

FluentMenuItem* addMenuItem(FluentMenu* menu, const QString& text, const QKeySequence& shortcut = QKeySequence())
{
    auto* item = new FluentMenuItem(text, menu);
    if (!shortcut.isEmpty())
        item->setShortcut(shortcut);
    menu->addAction(item);
    return item;
}

struct MenuBarSample {
    FluentMenuBar* bar = nullptr;
    FluentMenu* fileMenu = nullptr;
    FluentMenu* editMenu = nullptr;
    FluentMenu* viewMenu = nullptr;
    FluentMenu* helpMenu = nullptr;
    FluentMenu* sendToMenu = nullptr;
    FluentMenu* compressedMenu = nullptr;
    QAction* fileAction = nullptr;
    QAction* editAction = nullptr;
    QAction* viewAction = nullptr;
    QAction* helpAction = nullptr;
    QAction* sendToAction = nullptr;
    QAction* compressedAction = nullptr;
    QAction* bluetoothAction = nullptr;
    QAction* desktopShortcutAction = nullptr;
    QAction* compressEmailAction = nullptr;
    QAction* compress7zAction = nullptr;
    QAction* compressZipAction = nullptr;
    QAction* plainTopAction = nullptr;
    QAction* saveAction = nullptr;
    QAction* undoAction = nullptr;
    QAction* cutAction = nullptr;
    QAction* copyAction = nullptr;
    QAction* pasteAction = nullptr;
    QAction* disabledTopAction = nullptr;
    QAction* hiddenTopAction = nullptr;
};

MenuBarSample createSimpleMenuBar(QWidget* parent, bool includePlainAction = false)
{
    MenuBarSample sample;
    sample.bar = new FluentMenuBar(parent);
    sample.bar->resize(620, 40);

    sample.fileMenu = new FluentMenu(QStringLiteral("&File"), sample.bar);
    sample.fileMenu->menuAction()->setProperty("accessKey", QStringLiteral("F"));
    addMenuItem(sample.fileMenu, QStringLiteral("New"));
    addMenuItem(sample.fileMenu, QStringLiteral("Open"));
    sample.saveAction = addMenuItem(sample.fileMenu, QStringLiteral("Save"), QKeySequence(Qt::CTRL | Qt::Key_S));
    addMenuItem(sample.fileMenu, QStringLiteral("Exit"));
    sample.bar->addMenu(sample.fileMenu);
    sample.fileAction = sample.fileMenu->menuAction();

    sample.editMenu = new FluentMenu(QStringLiteral("&Edit"), sample.bar);
    sample.editMenu->menuAction()->setProperty("accessKey", QStringLiteral("E"));
    sample.undoAction = addMenuItem(sample.editMenu, QStringLiteral("Undo"), QKeySequence(Qt::CTRL | Qt::Key_Z));
    sample.cutAction = addMenuItem(sample.editMenu, QStringLiteral("Cut"), QKeySequence(Qt::CTRL | Qt::Key_X));
    sample.copyAction = addMenuItem(sample.editMenu, QStringLiteral("Copy"), QKeySequence(Qt::CTRL | Qt::Key_C));
    sample.pasteAction = addMenuItem(sample.editMenu, QStringLiteral("Paste"), QKeySequence(Qt::CTRL | Qt::Key_V));
    sample.bar->addMenu(sample.editMenu);
    sample.editAction = sample.editMenu->menuAction();

    sample.helpMenu = new FluentMenu(QStringLiteral("&Help"), sample.bar);
    sample.helpMenu->menuAction()->setProperty("accessKey", QStringLiteral("H"));
    addMenuItem(sample.helpMenu, QStringLiteral("About"));
    sample.bar->addMenu(sample.helpMenu);
    sample.helpAction = sample.helpMenu->menuAction();

    if (includePlainAction) {
        sample.plainTopAction = new QAction(QStringLiteral("Run"), sample.bar);
        sample.bar->addAction(sample.plainTopAction);
    }

    return sample;
}

MenuBarSample createComplexMenuBar(QWidget* parent)
{
    MenuBarSample sample = createSimpleMenuBar(parent);

    sample.sendToMenu = new FluentMenu(QStringLiteral("Send to"), sample.fileMenu);
    sample.bluetoothAction = addMenuItem(sample.sendToMenu, QStringLiteral("Bluetooth"));
    sample.desktopShortcutAction = addMenuItem(sample.sendToMenu, QStringLiteral("Desktop (shortcut)"));

    sample.compressedMenu = new FluentMenu(QStringLiteral("Compressed file"), sample.sendToMenu);
    sample.compressEmailAction = addMenuItem(sample.compressedMenu, QStringLiteral("Compress and email"));
    sample.compress7zAction = addMenuItem(sample.compressedMenu, QStringLiteral("Compress to .7z"));
    sample.compressZipAction = addMenuItem(sample.compressedMenu, QStringLiteral("Compress to .zip"));
    sample.compressedAction = sample.sendToMenu->addMenu(sample.compressedMenu);
    sample.sendToAction = sample.fileMenu->insertMenu(sample.saveAction, sample.sendToMenu);

    sample.viewMenu = new FluentMenu(QStringLiteral("&View"), sample.bar);
    addMenuItem(sample.viewMenu, QStringLiteral("Output"));
    sample.viewMenu->addSeparator();

    auto* orientationGroup = new QActionGroup(sample.viewMenu);
    orientationGroup->setExclusive(true);
    auto* landscape = new FluentMenuItem(QStringLiteral("Landscape"), sample.viewMenu);
    landscape->setCheckable(true);
    orientationGroup->addAction(landscape);
    sample.viewMenu->addAction(landscape);
    auto* portrait = new FluentMenuItem(QStringLiteral("Portrait"), sample.viewMenu);
    portrait->setCheckable(true);
    portrait->setChecked(true);
    orientationGroup->addAction(portrait);
    sample.viewMenu->addAction(portrait);
    sample.viewMenu->addSeparator();

    auto* sizeGroup = new QActionGroup(sample.viewMenu);
    sizeGroup->setExclusive(true);
    for (const QString& text : {QStringLiteral("Small icons"), QStringLiteral("Medium icons"), QStringLiteral("Large icons")}) {
        auto* item = new FluentMenuItem(text, sample.viewMenu);
        item->setCheckable(true);
        item->setChecked(text.startsWith(QStringLiteral("Medium")));
        sizeGroup->addAction(item);
        sample.viewMenu->addAction(item);
    }

    sample.viewAction = sample.viewMenu->menuAction();
    sample.bar->insertMenu(sample.helpAction, sample.viewMenu);
    return sample;
}

void closeMenus(MenuBarSample& sample)
{
    for (FluentMenu* menu : {sample.fileMenu, sample.editMenu, sample.viewMenu, sample.helpMenu, sample.sendToMenu, sample.compressedMenu}) {
        if (menu)
            menu->hide();
    }
    QApplication::processEvents();
}

void appendMenuTree(QVector<FluentMenu*>& menus, FluentMenu* menu)
{
    if (!menu || menus.contains(menu))
        return;

    menus.append(menu);
    for (QAction* action : menu->actions()) {
        if (!action || !action->menu())
            continue;
        appendMenuTree(menus, qobject_cast<FluentMenu*>(action->menu()));
    }
}

QVector<FluentMenu*> sampleMenus(const MenuBarSample& sample)
{
    QVector<FluentMenu*> result;
    for (FluentMenu* menu : {sample.fileMenu, sample.editMenu, sample.viewMenu, sample.helpMenu})
        appendMenuTree(result, menu);
    return result;
}

void closeMenus(const QVector<FluentMenu*>& menus)
{
    for (FluentMenu* menu : menus) {
        if (menu)
            menu->hide();
    }
    QApplication::processEvents();
}

QString actionLabelText(QString text)
{
    const int tabIndex = text.indexOf(QLatin1Char('\t'));
    if (tabIndex >= 0)
        text.truncate(tabIndex);
    text.remove(QLatin1Char('&'));
    return text;
}

void bindMenuActions(QWidget* host, const MenuBarSample& sample, Label* status = nullptr)
{
    const QVector<FluentMenu*> menus = sampleMenus(sample);

    for (FluentMenu* menu : menus) {
        if (!menu)
            continue;

        for (QAction* action : menu->actions()) {
            if (!action || action->isSeparator() || action->menu())
                continue;

            QObject::connect(action, &QAction::triggered, host, [status, action]() {
                if (!status)
                    return;
                status->setText(QStringLiteral("Clicked: %1").arg(actionLabelText(action->text())));
            });
        }
    }
}

int colorDistance(const QColor& lhs, const QColor& rhs)
{
    return qAbs(lhs.red() - rhs.red())
         + qAbs(lhs.green() - rhs.green())
         + qAbs(lhs.blue() - rhs.blue())
         + qAbs(lhs.alpha() - rhs.alpha());
}

QColor renderedPixel(QWidget* widget, const QPoint& point)
{
    QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    widget->render(&image);
    return image.pixelColor(point);
}

} // namespace

class MenuBarTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window = new MenuBarTestWindow();
        window->resize(760, 420);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    MenuBarTestWindow* window = nullptr;
};

TEST_F(MenuBarTest, TopLevelGeometryVisibilityAndThemeAreDeterministic)
{
    MenuBarSample sample = createSimpleMenuBar(window);
    sample.bar->setParent(window);
    sample.bar->move(0, 0);
    showAndProcess(*sample.bar);

    const QRect fileRect = sample.bar->fluentActionGeometry(sample.fileAction);
    const QRect editRect = sample.bar->fluentActionGeometry(sample.editAction);
    const QRect helpRect = sample.bar->fluentActionGeometry(sample.helpAction);
    EXPECT_EQ(fileRect.height(), 32);
    EXPECT_EQ(editRect.height(), 32);
    EXPECT_LT(fileRect.left(), editRect.left());
    EXPECT_LT(editRect.left(), helpRect.left());
    EXPECT_EQ(sample.bar->sizeHint().height(), 32);

    sample.bar->setFocus(Qt::TabFocusReason);
    QApplication::processEvents();
    const QColor focusEdge = renderedPixel(sample.bar, QPoint(fileRect.left() + 1, fileRect.center().y()));
    EXPECT_GT(qMin(focusEdge.red(), qMin(focusEdge.green(), focusEdge.blue())), 160);

    sample.disabledTopAction = new QAction(QStringLiteral("Disabled"), sample.bar);
    sample.disabledTopAction->setEnabled(false);
    sample.bar->addAction(sample.disabledTopAction);
    sample.hiddenTopAction = new QAction(QStringLiteral("Hidden"), sample.bar);
    sample.hiddenTopAction->setVisible(false);
    sample.bar->addAction(sample.hiddenTopAction);
    QApplication::processEvents();
    EXPECT_FALSE(sample.bar->fluentActionGeometry(sample.disabledTopAction).isEmpty());
    EXPECT_TRUE(sample.bar->fluentActionGeometry(sample.hiddenTopAction).isEmpty());

    auto* toolsMenu = new FluentMenu(QStringLiteral("Tools"), sample.bar);
    sample.bar->insertMenu(sample.helpAction, toolsMenu);
    QApplication::processEvents();
    EXPECT_LT(sample.bar->fluentActionGeometry(toolsMenu->menuAction()).left(),
              sample.bar->fluentActionGeometry(sample.helpAction).left());

    const QRect beforeTheme = sample.bar->fluentActionGeometry(sample.fileAction);
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    sample.bar->onThemeUpdated();
    QApplication::processEvents();
    EXPECT_EQ(sample.bar->fluentActionGeometry(sample.fileAction).height(), beforeTheme.height());
}

TEST_F(MenuBarTest, PointerAndKeyboardInteractionsUseQtActions)
{
    MenuBarSample sample = createSimpleMenuBar(window, true);
    sample.bar->setParent(window);
    sample.bar->move(0, 0);
    showAndProcess(*sample.bar);

    QSignalSpy plainSpy(sample.plainTopAction, &QAction::triggered);
    QTest::mouseClick(sample.bar, Qt::LeftButton, Qt::NoModifier, sample.bar->fluentActionGeometry(sample.plainTopAction).center());
    EXPECT_EQ(plainSpy.count(), 1);

    QTest::mouseClick(sample.bar, Qt::LeftButton, Qt::NoModifier, sample.bar->fluentActionGeometry(sample.fileAction).center());
    QApplication::processEvents();
    EXPECT_EQ(sample.bar->openAction(), sample.fileAction);
    EXPECT_TRUE(sample.fileMenu->isVisible());

    const QFontMetrics fileMetrics(sample.fileMenu->font());
    const int fileShortcutWidth = fileMetrics.horizontalAdvance(sample.fileMenu->shortcutTextForAction(sample.saveAction));
    const int fileShortcutColumn = fileShortcutWidth > 0 ? fileShortcutWidth + ::Spacing::Gap::Section : 0;
    const int fileLabelBudget = sample.fileMenu->width()
                              - 2 * ::Spacing::Standard
                              - 2 * ::Spacing::Gap::Tight
                              - 2 * ::Spacing::Padding::ControlHorizontal
                              - ::Spacing::Small
                              - fileShortcutColumn;
    EXPECT_GE(fileLabelBudget, fileMetrics.horizontalAdvance(QStringLiteral("Open")));

    const QPoint expectedPopupAnchor = sample.bar->mapToGlobal(QPoint(sample.bar->fluentActionGeometry(sample.fileAction).left(),
                                                                      sample.bar->fluentActionGeometry(sample.fileAction).bottom() + 1));
    EXPECT_LE(sample.fileMenu->pos().x(), expectedPopupAnchor.x());
    EXPECT_GE(sample.fileMenu->pos().x(), expectedPopupAnchor.x() - 32);
    EXPECT_LE(sample.fileMenu->pos().y(), expectedPopupAnchor.y());
    EXPECT_GE(sample.fileMenu->pos().y(), expectedPopupAnchor.y() - 48);

    QTest::keyClick(sample.bar, Qt::Key_Escape);
    QApplication::processEvents();
    EXPECT_EQ(sample.bar->openAction(), nullptr);
    EXPECT_EQ(sample.bar->hoveredAction(), nullptr);
    EXPECT_FALSE(sample.fileMenu->isVisible());

    sample.bar->setFocus(Qt::TabFocusReason);
    QApplication::processEvents();
    EXPECT_EQ(sample.bar->focusedAction(), sample.fileAction);
    QTest::keyClick(sample.bar, Qt::Key_Right);
    EXPECT_EQ(sample.bar->focusedAction(), sample.editAction);
    QTest::keyClick(sample.bar, Qt::Key_End);
    EXPECT_EQ(sample.bar->focusedAction(), sample.plainTopAction);
    QTest::keyClick(sample.bar, Qt::Key_Home);
    EXPECT_EQ(sample.bar->focusedAction(), sample.fileAction);
    QTest::keyClick(sample.bar, Qt::Key_Down);
    QApplication::processEvents();
    EXPECT_EQ(sample.bar->openAction(), sample.fileAction);

    closeMenus(sample);
}

TEST_F(MenuBarTest, AccessKeysAndShortcutsRemainInvokable)
{
    MenuBarSample sample = createSimpleMenuBar(window);
    sample.bar->setParent(window);
    sample.bar->move(0, 0);
    showAndProcess(*sample.bar);
    window->show();
    QApplication::processEvents();

    QKeyEvent altFile(QEvent::KeyPress, Qt::Key_F, Qt::AltModifier, QStringLiteral("f"));
    QApplication::sendEvent(sample.bar, &altFile);
    QApplication::processEvents();
    EXPECT_EQ(sample.bar->openAction(), sample.fileAction);
    EXPECT_TRUE(sample.fileMenu->isVisible());

    closeMenus(sample);
    window->activateWindow();
    window->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();

    bindMenuActions(window, sample);
    QSignalSpy saveSpy(sample.saveAction, &QAction::triggered);
    QSignalSpy undoSpy(sample.undoAction, &QAction::triggered);
    QTest::mouseClick(sample.bar, Qt::LeftButton, Qt::NoModifier, sample.bar->fluentActionGeometry(sample.fileAction).center());
    QApplication::processEvents();
    EXPECT_TRUE(sample.fileMenu->isVisible());
    QTest::keyClick(sample.fileMenu, Qt::Key_S, Qt::ControlModifier);
    EXPECT_EQ(saveSpy.count(), 1);
    EXPECT_EQ(sample.bar->openAction(), nullptr);
    EXPECT_FALSE(sample.fileMenu->isVisible());

    QTest::mouseClick(sample.bar, Qt::LeftButton, Qt::NoModifier, sample.bar->fluentActionGeometry(sample.fileAction).center());
    QApplication::processEvents();
    EXPECT_TRUE(sample.fileMenu->isVisible());
    QTest::keyClick(sample.fileMenu, Qt::Key_S, Qt::ControlModifier);
    EXPECT_EQ(saveSpy.count(), 2);
    EXPECT_EQ(sample.bar->openAction(), nullptr);
    EXPECT_FALSE(sample.fileMenu->isVisible());

    QTest::mouseClick(sample.bar, Qt::LeftButton, Qt::NoModifier, sample.bar->fluentActionGeometry(sample.editAction).center());
    QApplication::processEvents();
    EXPECT_TRUE(sample.editMenu->isVisible());
    QTest::keyClick(sample.editMenu, Qt::Key_Z, Qt::ControlModifier);
    EXPECT_EQ(undoSpy.count(), 1);
    EXPECT_EQ(sample.bar->openAction(), nullptr);
    EXPECT_FALSE(sample.editMenu->isVisible());
}

TEST_F(MenuBarTest, FluentMenuContentPatternsExposeDeterministicGeometry)
{
    auto* menu = new FluentMenu(QStringLiteral("View"), window);
    auto* output = addMenuItem(menu, QStringLiteral("Output"));
    QAction* separator = menu->addSeparator();
    auto* save = addMenuItem(menu, QStringLiteral("Save"), QKeySequence(Qt::CTRL | Qt::Key_S));

    auto* submenu = new FluentMenu(QStringLiteral("New"), menu);
    addMenuItem(submenu, QStringLiteral("Plain Text Document"));
    QAction* submenuAction = menu->addMenu(submenu);

    auto* compressedMenu = new FluentMenu(QStringLiteral("Compressed file"), submenu);
    auto* compressEmail = addMenuItem(compressedMenu, QStringLiteral("Compress and email"));
    addMenuItem(compressedMenu, QStringLiteral("Compress to .7z"));
    addMenuItem(compressedMenu, QStringLiteral("Compress to .zip"));
    QAction* compressedAction = submenu->addMenu(compressedMenu);

    auto* disabled = addMenuItem(menu, QStringLiteral("Disabled"));
    disabled->setEnabled(false);

    auto* group = new QActionGroup(menu);
    group->setExclusive(true);
    auto* landscape = new FluentMenuItem(QStringLiteral("Landscape"), menu);
    landscape->setCheckable(true);
    group->addAction(landscape);
    menu->addAction(landscape);
    auto* portrait = new FluentMenuItem(QStringLiteral("Portrait"), menu);
    portrait->setCheckable(true);
    portrait->setChecked(true);
    group->addAction(portrait);
    menu->addAction(portrait);

    menu->show();
    QApplication::processEvents();

    EXPECT_FALSE(menu->actionGeometry(output).isEmpty());
    EXPECT_GE(menu->actionGeometry(output).height(), 32);
    EXPECT_FALSE(menu->actionGeometry(separator).isEmpty());
    EXPECT_FALSE(menu->shortcutTextForAction(save).isEmpty());
    EXPECT_FALSE(menu->itemShortcutGeometry(save).isEmpty());
    EXPECT_FALSE(menu->itemSubmenuIndicatorGeometry(submenuAction).isEmpty());
    EXPECT_TRUE(portrait->isChecked());
    EXPECT_FALSE(landscape->isChecked());
    EXPECT_FALSE(disabled->isEnabled());
    EXPECT_GE(menu->sizeHint().width(), menu->itemSubmenuIndicatorGeometry(submenuAction).right() + 1);

    const QFontMetrics menuMetrics(menu->font());
    const int menuShortcutWidth = menuMetrics.horizontalAdvance(menu->shortcutTextForAction(save));
    const int menuShortcutColumn = menuShortcutWidth > 0 ? menuShortcutWidth + ::Spacing::Gap::Section : 0;
    const int menuLabelBudget = menu->width()
                              - 2 * ::Spacing::Standard
                              - 2 * ::Spacing::Gap::Tight
                              - 2 * ::Spacing::Padding::ControlHorizontal
                              - ::Spacing::ControlHeight::Small
                              - ::Spacing::ControlHeight::Small
                              - menuShortcutColumn;
    EXPECT_GE(menuLabelBudget, menuMetrics.horizontalAdvance(QStringLiteral("Landscape")));

    menu->setActiveAction(output);
    QApplication::processEvents();
    const int hoverY = menu->actionGeometry(output).center().y();
    const QColor hoverLeft = renderedPixel(menu, QPoint(::Spacing::Standard + ::Spacing::Gap::Tight + 2, hoverY));
    const QColor hoverRight = renderedPixel(menu, QPoint(menu->width() - ::Spacing::Standard - ::Spacing::Gap::Tight - 2, hoverY));
    EXPECT_LT(colorDistance(hoverLeft, hoverRight), 12);

    const int selectedY = menu->actionGeometry(portrait).center().y();
    const int fillSampleX = menu->width() - ::Spacing::Standard - ::Spacing::Gap::Tight - 8;
    const QColor hoverFill = renderedPixel(menu, QPoint(fillSampleX, hoverY));
    const QColor selectedFill = renderedPixel(menu, QPoint(fillSampleX, selectedY));
    EXPECT_LT(colorDistance(hoverFill, selectedFill), 12);

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    menu->onThemeUpdated();
    QApplication::processEvents();
    EXPECT_FALSE(menu->actionGeometry(save).isEmpty());

    submenu->show();
    QApplication::processEvents();
    EXPECT_FALSE(submenu->actionGeometry(compressedAction).isEmpty());
    EXPECT_FALSE(submenu->itemSubmenuIndicatorGeometry(compressedAction).isEmpty());
    compressedMenu->show();
    QApplication::processEvents();
    EXPECT_TRUE(menu->isVisible());
    EXPECT_TRUE(submenu->isVisible());
    EXPECT_TRUE(compressedMenu->isVisible());
    compressEmail->trigger();
    QApplication::processEvents();
    EXPECT_FALSE(menu->isVisible());
    EXPECT_FALSE(submenu->isVisible());
    EXPECT_FALSE(compressedMenu->isVisible());
    menu->hide();
}

TEST_F(MenuBarTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;
    window->resize(920, 560);
    window->setWindowTitle(QStringLiteral("MenuBar VisualCheck"));
    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);

    auto* title = new Label(QStringLiteral("MenuBar WinUI Gallery samples"), window);
    title->setFluentTypography(QStringLiteral("Subtitle"));
    title->anchors()->top = {window, Edge::Top, 24};
    title->anchors()->left = {window, Edge::Left, 32};
    addAnchored(layout, title);

    auto* themeButton = new Button(QStringLiteral("Theme"), window);
    themeButton->setFluentLayout(Button::IconBefore);
    themeButton->setIconGlyph(Typography::Icons::Color, 16);
    themeButton->setFixedSize(104, 32);
    themeButton->anchors()->top = {window, Edge::Top, 24};
    themeButton->anchors()->right = {window, Edge::Right, -32};
    addAnchored(layout, themeButton);

    auto* status = new Label(QStringLiteral("You clicked: (none)"), window);
    status->setFluentTypography(QStringLiteral("Body"));
    status->anchors()->left = {title, Edge::Left, 0};
    status->anchors()->bottom = {window, Edge::Bottom, -28};
    addAnchored(layout, status);

    auto* simpleLabel = new Label(QStringLiteral("Simple MenuBar"), window);
    simpleLabel->setFluentTypography(QStringLiteral("Body Strong"));
    simpleLabel->anchors()->top = {title, Edge::Bottom, 28};
    simpleLabel->anchors()->left = {title, Edge::Left, 0};
    addAnchored(layout, simpleLabel);
    MenuBarSample simple = createSimpleMenuBar(window);
    simple.bar->anchors()->top = {simpleLabel, Edge::Bottom, 8};
    simple.bar->anchors()->left = {simpleLabel, Edge::Left, 0};
    simple.bar->anchors()->right = {window, Edge::Right, -32};
    addAnchored(layout, simple.bar);
    bindMenuActions(window, simple, status);

    auto* acceleratorLabel = new Label(QStringLiteral("MenuBar with keyboard accelerators"), window);
    acceleratorLabel->setFluentTypography(QStringLiteral("Body Strong"));
    acceleratorLabel->anchors()->top = {simple.bar, Edge::Bottom, 42};
    acceleratorLabel->anchors()->left = {title, Edge::Left, 0};
    addAnchored(layout, acceleratorLabel);
    MenuBarSample accelerator = createSimpleMenuBar(window);
    accelerator.bar->anchors()->top = {acceleratorLabel, Edge::Bottom, 8};
    accelerator.bar->anchors()->left = {title, Edge::Left, 0};
    accelerator.bar->anchors()->right = {window, Edge::Right, -32};
    addAnchored(layout, accelerator.bar);
    bindMenuActions(window, accelerator, status);

    auto* complexLabel = new Label(QStringLiteral("MenuBar with cascading submenus, separators, and radio items"), window);
    complexLabel->setFluentTypography(QStringLiteral("Body Strong"));
    complexLabel->anchors()->top = {accelerator.bar, Edge::Bottom, 42};
    complexLabel->anchors()->left = {title, Edge::Left, 0};
    addAnchored(layout, complexLabel);
    MenuBarSample complex = createComplexMenuBar(window);
    complex.bar->anchors()->top = {complexLabel, Edge::Bottom, 8};
    complex.bar->anchors()->left = {title, Edge::Left, 0};
    complex.bar->anchors()->right = {window, Edge::Right, -32};
    addAnchored(layout, complex.bar);
    bindMenuActions(window, complex, status);

    QObject::connect(themeButton, &Button::clicked, window, [this]() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                    ? fluent::FluentElement::Dark
                                    : fluent::FluentElement::Light);
        window->onThemeUpdated();
    });

    window->onThemeUpdated();
    window->show();
    qApp->exec();
}

#include "QtTestEnvironment.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QIntValidator>
#include <QMenu>
#include <QTimer>
#include <QtMath>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <gtest/gtest.h>

using namespace fluent::textfields;
using namespace fluent::basicinput;
using namespace fluent;

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class LineEditTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(500, 400);
        window->setWindowTitle("Fluent LineEdit Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    FluentTestWindow* window;
    AnchorLayout* layout;
};

namespace {

bool actionMatchesStandardKey(const QAction* action,
                              QKeySequence::StandardKey standardKey)
{
    if (!action)
        return false;

    QList<QKeySequence> shortcuts = action->shortcuts();
    if (shortcuts.isEmpty()) {
        const int tabIndex = action->text().indexOf(QLatin1Char('\t'));
        if (tabIndex >= 0) {
            const QKeySequence embedded(
                action->text().mid(tabIndex + 1).trimmed(),
                QKeySequence::NativeText);
            if (!embedded.isEmpty())
                shortcuts.append(embedded);
        }
    }

    const QList<QKeySequence> bindings =
        QKeySequence::keyBindings(standardKey);
    for (const QKeySequence& shortcut : shortcuts) {
        for (const QKeySequence& binding : bindings) {
            if (shortcut.matches(binding) == QKeySequence::ExactMatch)
                return true;
        }
    }
    return false;
}

bool triggerContextAction(LineEdit* edit,
                          QKeySequence::StandardKey standardKey)
{
    if (!edit)
        return false;

    bool triggered = false;
    QTimer::singleShot(0, [&]() {
        auto* menu =
            qobject_cast<fluent::menus_toolbars::FluentMenu*>(
                QApplication::activePopupWidget());
        if (!menu)
            return;

        for (QAction* action : menu->actions()) {
            if (!actionMatchesStandardKey(action, standardKey))
                continue;
            if (action->isEnabled()) {
                action->trigger();
                triggered = true;
            }
            break;
        }
        if (menu->isVisible())
            menu->close();
    });

    const QPoint localPos = edit->rect().center();
    const QPoint globalPos = edit->mapToGlobal(localPos);
    QContextMenuEvent event(
        QContextMenuEvent::Mouse, localPos, globalPos);
    QApplication::sendEvent(edit, &event);
    QTest::qWait(1);
    return triggered;
}

} // namespace

TEST_F(LineEditTest, TextAndPlaceholder) {
    LineEdit* edit = new LineEdit(window);
    edit->setPlaceholderText("Enter value");
    EXPECT_EQ(edit->placeholderText(), "Enter value");

    edit->setText("hello");
    EXPECT_EQ(edit->text(), "hello");
}

TEST_F(LineEditTest, PlaceholderPaletteUsesResolvedOpaqueToken) {
    const auto previousTheme = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);

    LineEdit edit(window);
    edit.onThemeUpdated();
    const QColor placeholder = edit.palette().color(QPalette::Active,
                                                     QPalette::PlaceholderText);

    EXPECT_EQ(placeholder.alpha(), 255);
    EXPECT_GT(placeholder.red(), 130);
    EXPECT_LT(placeholder.red(), 150);
    EXPECT_EQ(placeholder.red(), placeholder.green());
    EXPECT_EQ(placeholder.green(), placeholder.blue());

    fluent::FluentElement::setTheme(previousTheme);
}

TEST_F(LineEditTest, StyledAncestorDoesNotReplaceDarkThemeTextPalette) {
    const auto previousTheme = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    window->onThemeUpdated();

    LineEdit edit(window);
    edit.setText(QStringLiteral("42"));
    edit.onThemeUpdated();

    const auto colors = edit.themeColors();
    EXPECT_EQ(edit.palette().color(QPalette::Active, QPalette::Text),
              colors.textPrimary);
    EXPECT_EQ(edit.palette().color(QPalette::Inactive, QPalette::Text),
              colors.textPrimary);
    EXPECT_EQ(edit.palette().color(QPalette::Disabled, QPalette::Text),
              colors.textDisabled);

    fluent::FluentElement::setTheme(previousTheme);
    window->onThemeUpdated();
}

TEST_F(LineEditTest, ContentMargins) {
    LineEdit* edit = new LineEdit(window);
    QMargins margins(10, 2, 10, 2);
    edit->setContentMargins(margins);
    EXPECT_EQ(edit->contentMargins(), margins);
}

TEST_F(LineEditTest, ReadOnly) {
    LineEdit* edit = new LineEdit(window);
    edit->setText("read only");
    edit->setReadOnly(true);
    EXPECT_TRUE(edit->isReadOnly());
    edit->setReadOnly(false);
    EXPECT_FALSE(edit->isReadOnly());
}

TEST_F(LineEditTest, Validator) {
    LineEdit* edit = new LineEdit(window);
    auto* validator = new QIntValidator(0, 100, edit);
    edit->setValidator(validator);
    EXPECT_EQ(edit->validator(), validator);
}

TEST_F(LineEditTest, Contract_StandardEditingActionsUseFluentContextMenu)
{
    auto* edit = new LineEdit(window);
    edit->setText(QStringLiteral("Alpha Beta"));
    edit->selectAll();
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    bool sawFluentMenu = false;
    bool sawCopy = false;
    bool sawSelectAll = false;
    bool sawCopyGlyph = false;
    bool sawDeleteGlyph = false;
    bool sawSelectAllGlyph = false;
    QTimer::singleShot(0, [&]() {
        auto* menu =
            qobject_cast<fluent::menus_toolbars::FluentMenu*>(
                QApplication::activePopupWidget());
        sawFluentMenu = menu != nullptr;
        if (!menu)
            return;

        EXPECT_EQ(
            menu->objectName(),
            QStringLiteral("FluentLineEdit.ContextMenu"));
        EXPECT_EQ(menu->fontStyle(), Typography::FontRole::Caption);
        EXPECT_EQ(
            menu->font().pixelSize(),
            Typography::FontSize::Caption);
        EXPECT_FALSE(
            menu->property(
                    "_fluentqt_menuQuietSeparators")
                .toBool());
        for (QAction* action : menu->actions()) {
            if (!action->isSeparator()) {
                EXPECT_LT(
                    menu->actionGeometry(action).height(),
                    ::Spacing::ControlHeight::Standard);
            }
            const bool isCopy =
                actionMatchesStandardKey(action, QKeySequence::Copy);
            const bool isSelectAll =
                actionMatchesStandardKey(action, QKeySequence::SelectAll);
            const bool isDelete =
                action->text().contains(
                    QStringLiteral("Delete"), Qt::CaseInsensitive);
            sawCopy = sawCopy || isCopy;
            sawSelectAll = sawSelectAll || isSelectAll;
            sawCopyGlyph =
                sawCopyGlyph || (isCopy && !action->icon().isNull());
            sawDeleteGlyph =
                sawDeleteGlyph || (isDelete && !action->icon().isNull());
            sawSelectAllGlyph =
                sawSelectAllGlyph
                || (isSelectAll && !action->icon().isNull());
            if (!action->icon().isNull()) {
                const QSize iconSize =
                    action->icon().actualSize(QSize(64, 64));
                const int maximumBackingExtent = qCeil(
                    Typography::IconSize::Standard
                    * qMax<qreal>(
                        1.0, menu->devicePixelRatioF()));
                EXPECT_GT(iconSize.width(), 0);
                EXPECT_LE(
                    iconSize.width(),
                    maximumBackingExtent);
                EXPECT_LE(
                    iconSize.height(),
                    maximumBackingExtent);
            }
        }
        menu->close();
    });

    const QPoint localPos = edit->rect().center();
    const QPoint globalPos = edit->mapToGlobal(localPos);
    QContextMenuEvent event(
        QContextMenuEvent::Mouse, localPos, globalPos);
    QApplication::sendEvent(edit, &event);

    EXPECT_TRUE(event.isAccepted());
    QTRY_VERIFY_WITH_TIMEOUT(sawFluentMenu, 1000);
    EXPECT_TRUE(sawFluentMenu);
    EXPECT_TRUE(sawCopy);
    EXPECT_TRUE(sawSelectAll);
    EXPECT_TRUE(sawCopyGlyph);
    EXPECT_TRUE(sawDeleteGlyph);
    EXPECT_TRUE(sawSelectAllGlyph);
}

TEST_F(LineEditTest, Contract_UndoRedoRemainFunctionalFromContextMenu)
{
    auto* edit = new LineEdit(window);
    edit->setText(QStringLiteral("Alpha"));
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    edit->setCursorPosition(edit->text().size());
    edit->insert(QStringLiteral(" Beta"));
    const QString editedText = QStringLiteral("Alpha Beta");
    ASSERT_EQ(edit->text(), editedText);

    edit->setFocus(Qt::OtherFocusReason);
    QTest::keySequence(edit, QKeySequence(QKeySequence::Undo));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha"));
    QTest::keySequence(edit, QKeySequence(QKeySequence::Redo));
    EXPECT_EQ(edit->text(), editedText);

    ASSERT_TRUE(triggerContextAction(edit, QKeySequence::Undo));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha"));
    ASSERT_TRUE(triggerContextAction(edit, QKeySequence::Redo));
    EXPECT_EQ(edit->text(), editedText);
}

TEST_F(LineEditTest, FluentPropertiesDefaultsAndSetters) {
    LineEdit* edit = new LineEdit(window);

    // 默认值验证（引用 Spacing/Typography 常量）
    EXPECT_TRUE(edit->isClearButtonEnabled());
    EXPECT_EQ(edit->clearButtonSize(), 22);
    EXPECT_EQ(edit->clearButtonOffset(), QPoint(Spacing::XSmall, 0));
    EXPECT_EQ(edit->focusedBorderWidth(), Spacing::Border::Focused);
    EXPECT_EQ(edit->unfocusedBorderWidth(), Spacing::Border::Normal);
    EXPECT_EQ(edit->fontRole(), Typography::FontRole::Body);

    QSignalSpy spyOffset(edit, SIGNAL(clearButtonOffsetChanged()));
    QSignalSpy spyFocused(edit, SIGNAL(focusedBorderWidthChanged()));
    QSignalSpy spyUnfocused(edit, SIGNAL(unfocusedBorderWidthChanged()));

    edit->setClearButtonOffset(QPoint(10, 3));
    EXPECT_EQ(edit->clearButtonOffset(), QPoint(10, 3));
    EXPECT_EQ(spyOffset.count(), 1);

    edit->setFocusedBorderWidth(3);
    EXPECT_EQ(edit->focusedBorderWidth(), 3);
    EXPECT_EQ(spyFocused.count(), 1);

    edit->setUnfocusedBorderWidth(2);
    EXPECT_EQ(edit->unfocusedBorderWidth(), 2);
    EXPECT_EQ(spyUnfocused.count(), 1);

    // 相同值不应再次触发信号
    edit->setClearButtonOffset(QPoint(10, 3));
    edit->setFocusedBorderWidth(3);
    edit->setUnfocusedBorderWidth(2);
    EXPECT_EQ(spyOffset.count(), 1);
    EXPECT_EQ(spyFocused.count(), 1);
    EXPECT_EQ(spyUnfocused.count(), 1);
}

TEST_F(LineEditTest, ClearButtonOffsetAffectsGeometry) {
    LineEdit* edit = new LineEdit(window);
    edit->setClearButtonEnabled(true);
    edit->setText("x");
    edit->setFixedSize(200, 40);
    // 在无屏环境下 resizeEvent 可能不会立刻触发，这里用 setter 主动刷新几何
    edit->setClearButtonSize(20);
    edit->setClearButtonOffset(QPoint(12, 5));

    // internal clear button is a fluent::basicinput::Button child
    const auto buttons = edit->findChildren<::fluent::basicinput::Button*>();
    ASSERT_EQ(buttons.size(), 1);
    auto* clearBtn = buttons.first();
    ASSERT_NE(clearBtn, nullptr);

    const int expectedX = edit->width() - 20 - 12;
    const int expectedY = (edit->height() - 20) / 2 + 5;
    EXPECT_EQ(clearBtn->pos(), QPoint(expectedX, expectedY));
}


TEST_F(LineEditTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    Label* header = new Label("LineEdit (single-line):", window);
    header->anchors()->top = {window, Edge::Top, 30};
    header->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(header);

    // 默认样式
    LineEdit* edit = new LineEdit(window);
    edit->setPlaceholderText("Default LineEdit...");
    edit->setText("Sample text");
    edit->setContentMargins(QMargins(8, 4, 8, 4));
    edit->anchors()->top = {header, Edge::Bottom, 8};
    edit->anchors()->left = {window, Edge::Left, 40};
    edit->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(edit);

    // 带自定义 clearButtonOffset 的例子
    Label* offsetHeader = new Label("With custom clearButtonOffset:", window);
    offsetHeader->anchors()->top = {edit, Edge::Bottom, 20};
    offsetHeader->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(offsetHeader);

    LineEdit* offsetEdit = new LineEdit(window);
    offsetEdit->setPlaceholderText("Clear button offset (x=12, y=4)...");
    offsetEdit->setText("Offset clear button");
    offsetEdit->setContentMargins(QMargins(8, 4, 8, 4));
    offsetEdit->setClearButtonOffset(QPoint(12, 4));
    offsetEdit->anchors()->top = {offsetHeader, Edge::Bottom, 8};
    offsetEdit->anchors()->left = {window, Edge::Left, 40};
    offsetEdit->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(offsetEdit);

    // 带自定义边框粗细的例子
    Label* borderHeader = new Label("Custom focused/unfocused border widths:", window);
    borderHeader->anchors()->top = {offsetEdit, Edge::Bottom, 20};
    borderHeader->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(borderHeader);

    LineEdit* borderEdit = new LineEdit(window);
    borderEdit->setPlaceholderText("Focused=3px, Unfocused=2px...");
    borderEdit->setText("Border thickness demo");
    borderEdit->setContentMargins(QMargins(8, 4, 8, 4));
    borderEdit->setFocusedBorderWidth(3);
    borderEdit->setUnfocusedBorderWidth(2);
    borderEdit->anchors()->top = {borderHeader, Edge::Bottom, 8};
    borderEdit->anchors()->left = {window, Edge::Left, 40};
    borderEdit->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(borderEdit);

    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFluentStyle(Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right = {window, Edge::Right, -30};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    });

    window->show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        ASSERT_TRUE(tests::support::captureVisualSnapshot(window));
        return;
    }

    qApp->exec();
}

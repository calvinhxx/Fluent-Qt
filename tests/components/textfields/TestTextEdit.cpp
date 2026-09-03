#include "QtTestEnvironment.h"
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/menus_toolbars/Menu.h"
#include "components/menus_toolbars/private/TextEditingMenu_p.h"
#include "components/textfields/Label.h"
#include "components/textfields/TextEdit.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QImage>
#include <QInputMethodEvent>
#include <QMenu>
#include <QMetaProperty>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QWheelEvent>
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
    void onThemeUpdated() override
    {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class TextEditTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window = new FluentTestWindow();
        window->setFixedSize(500, 400);
        window->setWindowTitle("Fluent TextEdit Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override { delete window; }

    FluentTestWindow* window;
    AnchorLayout* layout;
};

namespace {

QWheelEvent makeWheelEvent(QWidget* target, QPoint pixelDelta, QPoint angleDelta,
                           Qt::ScrollPhase phase = Qt::NoScrollPhase)
{
    const QPointF pos = target->rect().center();
    const QPointF globalPos = target->mapToGlobal(pos.toPoint());
    return QWheelEvent(pos, globalPos, pixelDelta, angleDelta, Qt::NoButton, Qt::NoModifier, phase,
                       false);
}

QTextEdit* innerTextEdit(TextEdit* edit)
{
    return edit ? edit->findChild<QTextEdit*>() : nullptr;
}

bool actionMatchesStandardKey(const QAction* action, QKeySequence::StandardKey standardKey)
{
    if (!action)
        return false;

    QList<QKeySequence> shortcuts = action->shortcuts();
    if (shortcuts.isEmpty()) {
        const int tabIndex = action->text().indexOf(QLatin1Char('\t'));
        if (tabIndex >= 0) {
            const QKeySequence embedded(action->text().mid(tabIndex + 1).trimmed(),
                                        QKeySequence::NativeText);
            if (!embedded.isEmpty())
                shortcuts.append(embedded);
        }
    }

    const QList<QKeySequence> bindings = QKeySequence::keyBindings(standardKey);
    for (const QKeySequence& shortcut : shortcuts) {
        for (const QKeySequence& binding : bindings) {
            if (shortcut.matches(binding) == QKeySequence::ExactMatch)
                return true;
        }
    }
    return false;
}

bool triggerContextAction(QTextEdit* inner, QKeySequence::StandardKey standardKey)
{
    if (!inner)
        return false;

    bool triggered = false;
    QTimer::singleShot(0, [&]() {
        auto* menu =
            qobject_cast<fluent::menus_toolbars::FluentMenu*>(QApplication::activePopupWidget());
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

    const QPoint localPos = inner->viewport()->rect().center();
    const QPoint globalPos = inner->viewport()->mapToGlobal(localPos);
    QContextMenuEvent event(QContextMenuEvent::Mouse, localPos, globalPos);
    QApplication::sendEvent(inner->viewport(), &event);
    QTest::qWait(1);
    return triggered;
}

QImage renderWidget(QWidget* widget)
{
    QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget->render(&painter);
    return image;
}

int differingPixels(const QImage& lhs, const QImage& rhs, const QRect& area)
{
    if (lhs.size() != rhs.size())
        return -1;
    const QRect bounded = area.intersected(lhs.rect()).intersected(rhs.rect());
    int count = 0;
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            if (lhs.pixel(x, y) != rhs.pixel(x, y))
                ++count;
        }
    }
    return count;
}

} // namespace

TEST_F(TextEditTest, TextAndPlaceholder)
{
    TextEdit* edit = new TextEdit(window);
    edit->setPlaceholderText("Multi-line placeholder");
    EXPECT_EQ(edit->placeholderText(), "Multi-line placeholder");

    edit->setPlainText("line1\nline2");
    EXPECT_EQ(edit->toPlainText(), "line1\nline2");
}

TEST_F(TextEditTest, PlaceholderIsHiddenDuringInputMethodPreedit)
{
    auto* edit = new TextEdit(window);
    edit->setFixedSize(360, edit->lineHeight());
    edit->move(20, 20);
    window->show();
    edit->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    inner->setFocus(Qt::OtherFocusReason);
    const QString placeholder = QStringLiteral("Ask Claude to inspect this project");

    edit->setPlaceholderText(QString());
    QApplication::processEvents();
    const QImage blank = renderWidget(inner->viewport());

    edit->setPlaceholderText(placeholder);
    QApplication::processEvents();
    const QImage withPlaceholder = renderWidget(inner->viewport());

    QInputMethodEvent preedit(QStringLiteral("a's'd"), {});
    QApplication::sendEvent(inner, &preedit);
    QApplication::processEvents();
    EXPECT_TRUE(edit->toPlainText().isEmpty());
    const QImage duringPreedit = renderWidget(inner->viewport());

    const QRect placeholderTail(64, 0, qMax(0, inner->viewport()->width() - 64),
                                inner->viewport()->height());
    EXPECT_GT(differingPixels(withPlaceholder, blank, placeholderTail), 20)
        << "The fixture must contain visible placeholder glyphs in the tail "
           "region";
    EXPECT_LT(differingPixels(duringPreedit, blank, placeholderTail), 8)
        << "IME preedit text must replace, not overlap, the placeholder";
}

TEST_F(TextEditTest, Contract_LayoutPropertiesAreAvailableThroughQtMetaObject)
{
    TextEdit* edit = new TextEdit(window);
    const QMetaObject* metaObject = edit->metaObject();

    for (const char* propertyName :
         {"lineHeight", "minVisibleLines", "maxVisibleLines", "tabChangesFocus"}) {
        const int propertyIndex = metaObject->indexOfProperty(propertyName);
        ASSERT_GE(propertyIndex, 0) << propertyName;
        const QMetaProperty property = metaObject->property(propertyIndex);
        EXPECT_TRUE(property.isReadable()) << propertyName;
        EXPECT_TRUE(property.isWritable()) << propertyName;
        EXPECT_TRUE(property.hasNotifySignal()) << propertyName;
    }
}

TEST_F(TextEditTest, Contract_TabChangesFocusIsOptInAndForwardsToEditor)
{
    TextEdit* edit = new TextEdit(window);
    auto* next = new fluent::basicinput::Button(QStringLiteral("Next"), window);
    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);

    edit->move(20, 20);
    next->move(20, 80);
    next->setFocusPolicy(Qt::StrongFocus);
    QWidget::setTabOrder(edit, next);
    window->show();
    edit->show();
    next->show();
    QApplication::processEvents();

    EXPECT_FALSE(edit->tabChangesFocus());
    EXPECT_FALSE(inner->tabChangesFocus());
    QSignalSpy spy(edit, &TextEdit::tabChangesFocusChanged);

    edit->setPlainText(QStringLiteral("tab probe"));
    QTextCursor insertionCursor = inner->textCursor();
    insertionCursor.movePosition(QTextCursor::End);
    inner->setTextCursor(insertionCursor);
    inner->setFocus(Qt::OtherFocusReason);
    ASSERT_EQ(QApplication::focusWidget(), inner);
    QTest::keyClick(inner, Qt::Key_Tab);
    EXPECT_EQ(edit->toPlainText(), QStringLiteral("tab probe\t"));
    EXPECT_EQ(QApplication::focusWidget(), inner);

    edit->setTabChangesFocus(true);
    EXPECT_TRUE(edit->tabChangesFocus());
    EXPECT_TRUE(inner->tabChangesFocus());
    EXPECT_EQ(spy.count(), 1);

    edit->setPlainText(QStringLiteral("tab probe"));
    inner->setFocus(Qt::OtherFocusReason);
    ASSERT_EQ(QApplication::focusWidget(), inner);
    QTest::keyClick(inner, Qt::Key_Tab);
    EXPECT_EQ(edit->toPlainText(), QStringLiteral("tab probe"));
    EXPECT_EQ(QApplication::focusWidget(), next);

    edit->setTabChangesFocus(true);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TextEditTest, Contract_MaxLineViewportDoesNotRevealPartialOverflowLine)
{
    const FluentElement::Theme previousTheme = FluentElement::currentTheme();
    struct ThemeRestorer {
        FluentElement::Theme theme;
        ~ThemeRestorer()
        {
            FluentElement::setTheme(theme);
            QApplication::processEvents();
        }
    } restoreTheme{previousTheme};
    FluentElement::setTheme(FluentElement::Light);

    TextEdit* edit = new TextEdit(window);
    edit->setFontRole(Typography::FontRole::Body);
    const int fontLineHeight =
        QFontMetrics(edit->themeFont(Typography::FontRole::Body).toQFont()).lineSpacing();
    edit->setLineHeight(fontLineHeight);
    edit->setMinVisibleLines(1);
    edit->setMaxVisibleLines(6);
    edit->setContentMargins(QMargins(12, 6, 12, 6));
    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    const QString fullText = QStringLiteral("请检查这个仓库中 JSON-RPC 运行时的关闭流程。\n"
                                            "确认 stdin、terminate 和 kill 的顺序。\n"
                                            "说明启动阶段失败时如何重试。\n"
                                            "检查非常长的工作区路径是否会截断。\n"
                                            "给出最小且安全的修改建议。\n"
                                            "同时列出需要保留的兼容行为。\n"
                                            "这一行用于验证超过最大可见行数后的内部滚动。");
    // Use the real editing/accessibility surface. This deliberately bypasses
    // TextEdit::setPlainText(), just as typing and AX value replacement do.
    inner->setPlainText(fullText);
    edit->setGeometry(0, 0, 475, edit->height());
    window->show();
    QApplication::processEvents();

    QTextBlock sixthBlock = inner->document()->begin();
    for (int index = 0; index < 5; ++index)
        sixthBlock = sixthBlock.next();
    const QTextBlock seventhBlock = sixthBlock.next();
    ASSERT_TRUE(sixthBlock.isValid());
    ASSERT_TRUE(seventhBlock.isValid());
    const QTextLine seventhLine = seventhBlock.layout()->lineAt(0);
    QTextCursor seventhCursor(seventhBlock);

    const QRect seventhCaret = inner->cursorRect(seventhCursor);
    // Qt 5/X11 rounds a fractional QTextLine origin down when cursorRect()
    // converts it to QRect. The exact pixel comparison below remains strict;
    // this geometry assertion only accounts for that subpixel conversion.
    EXPECT_GE(seventhCaret.top() + 1, inner->viewport()->height())
        << "seventh visual line top=" << seventhCaret.top()
        << ", viewport height=" << inner->viewport()->height()
        << ", layout line height=" << seventhLine.height()
        << ", declared line height=" << edit->lineHeight();

    const int sixLineLength = seventhBlock.position() - 1;
    inner->setPlainText(fullText.left(sixLineLength));
    QApplication::processEvents();
    const QImage sixLines = renderWidget(inner->viewport());

    inner->setPlainText(fullText);
    inner->verticalScrollBar()->setValue(inner->verticalScrollBar()->minimum());
    QApplication::processEvents();
    const QImage overflow = renderWidget(inner->viewport());
    const QRect textViewport(0, 0, qMax(0, overflow.width() - 24), overflow.height());
    EXPECT_EQ(differingPixels(sixLines, overflow, textViewport), 0)
        << "overflow content changed pixels inside the capped text viewport";

    QScrollBar* innerScrollBar = inner->verticalScrollBar();
    ASSERT_NE(innerScrollBar, nullptr);
    const auto caretRectForBlock = [inner](int blockIndex) {
        QTextBlock block = inner->document()->begin();
        for (int index = 0; index < blockIndex && block.isValid(); ++index)
            block = block.next();
        return block.isValid() ? inner->cursorRect(QTextCursor(block)) : QRect();
    };
    const auto sixthCaretBottom = [inner]() {
        QTextBlock sixth = inner->document()->begin();
        for (int index = 0; index < 5; ++index)
            sixth = sixth.next();
        if (!sixth.isValid())
            return -1;
        return inner->cursorRect(QTextCursor(sixth)).bottom();
    };
    const int lightMaximum = innerScrollBar->maximum();
    const int lightPageStep = innerScrollBar->pageStep();
    const int lightSixthBottom = sixthCaretBottom();
    innerScrollBar->setValue(lightMaximum);
    QApplication::processEvents();
    const QRect lightSecondAtTail = caretRectForBlock(1);
    const QRect lightSeventhAtTail = caretRectForBlock(6);

    FluentElement::setThemeDeferred(FluentElement::Dark);
    QApplication::processEvents();
    QTest::qWait(1);
    QApplication::processEvents();
    innerScrollBar->setValue(innerScrollBar->minimum());
    QApplication::processEvents();
    const int darkMaximum = innerScrollBar->maximum();
    const int darkPageStep = innerScrollBar->pageStep();
    const int darkSixthBottom = sixthCaretBottom();
    innerScrollBar->setValue(darkMaximum);
    QApplication::processEvents();
    const QRect darkSecondAtTail = caretRectForBlock(1);
    const QRect darkSeventhAtTail = caretRectForBlock(6);

    FluentElement::setThemeDeferred(FluentElement::Light);
    QApplication::processEvents();
    QTest::qWait(1);
    QApplication::processEvents();
    innerScrollBar->setValue(innerScrollBar->minimum());
    QApplication::processEvents();
    const int returnedLightMaximum = innerScrollBar->maximum();
    const int returnedLightPageStep = innerScrollBar->pageStep();
    const int returnedLightSixthBottom = sixthCaretBottom();
    innerScrollBar->setValue(returnedLightMaximum);
    QApplication::processEvents();
    const QRect returnedLightSecondAtTail = caretRectForBlock(1);
    const QRect returnedLightSeventhAtTail = caretRectForBlock(6);

    EXPECT_EQ(darkMaximum, lightMaximum)
        << "Light-to-Dark must not change the capped editor scroll range";
    EXPECT_EQ(darkPageStep, lightPageStep)
        << "Light-to-Dark must not change the capped editor viewport extent";
    EXPECT_EQ(returnedLightMaximum, lightMaximum)
        << "Dark-to-Light must restore identical capped editor geometry";
    EXPECT_EQ(returnedLightPageStep, lightPageStep)
        << "Dark-to-Light must preserve the capped editor viewport extent";
    EXPECT_LT(lightSixthBottom, inner->viewport()->height())
        << "the sixth line must fit before the Light theme transition";
    EXPECT_LT(darkSixthBottom, inner->viewport()->height())
        << "the sixth line must fit after the Light-to-Dark transition";
    EXPECT_LT(returnedLightSixthBottom, inner->viewport()->height())
        << "the sixth line must remain fitted after Dark-to-Light";
    const auto expectWholeTail = [inner](const char* phase, const QRect& firstVisible,
                                         const QRect& lastVisible) {
        EXPECT_TRUE(firstVisible.isValid()) << phase;
        EXPECT_TRUE(lastVisible.isValid()) << phase;
        EXPECT_GE(firstVisible.top(), 0)
            << phase << " must not clip the first visible line at the top";
        EXPECT_LT(firstVisible.top(), inner->viewport()->height())
            << phase << " must keep the first tail line visible";
        EXPECT_GE(lastVisible.top(), 0) << phase << " must keep the final line inside the viewport";
        EXPECT_LT(lastVisible.bottom(), inner->viewport()->height())
            << phase << " must not clip the final line at the bottom";
    };
    expectWholeTail("Light tail", lightSecondAtTail, lightSeventhAtTail);
    expectWholeTail("Dark tail", darkSecondAtTail, darkSeventhAtTail);
    expectWholeTail("returned Light tail", returnedLightSecondAtTail, returnedLightSeventhAtTail);
}

TEST_F(TextEditTest, Contract_SelectionPaletteTracksTheme)
{
    const FluentElement::Theme previousTheme = FluentElement::currentTheme();
    struct ThemeRestorer {
        FluentElement::Theme theme;
        ~ThemeRestorer()
        {
            FluentElement::setTheme(theme);
            QApplication::processEvents();
        }
    } restoreTheme{previousTheme};

    auto* edit = new TextEdit(window);
    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);

    const auto expectSelectionPalette = [edit, inner](const char* phase) {
        const QPalette palette = inner->palette();
        const auto& colors = edit->themeColorsRef();
        EXPECT_EQ(palette.color(QPalette::Active, QPalette::Highlight), colors.accentDefault)
            << phase;
        EXPECT_EQ(palette.color(QPalette::Active, QPalette::HighlightedText), colors.textOnAccent)
            << phase;
        EXPECT_EQ(palette.color(QPalette::Inactive, QPalette::Highlight), colors.accentDefault)
            << phase;
        EXPECT_EQ(palette.color(QPalette::Inactive, QPalette::HighlightedText), colors.textOnAccent)
            << phase;
        EXPECT_FALSE(inner->styleSheet().contains(QStringLiteral("selection-"))) << phase;
    };

    FluentElement::setTheme(FluentElement::Light);
    QApplication::processEvents();
    expectSelectionPalette("Light");

    FluentElement::setTheme(FluentElement::Dark);
    QApplication::processEvents();
    expectSelectionPalette("Dark");
}

TEST_F(TextEditTest, Contract_ScopedThemeTransitionPreservesTailAnchor)
{
    window->setProperty("fluentThemeOverride", static_cast<int>(FluentElement::Light));

    auto* edit = new TextEdit(window);
    edit->setMinVisibleLines(2);
    edit->setMaxVisibleLines(3);
    edit->setPlainText(QStringLiteral("Alpha\nBeta\nGamma\nDelta\nEpsilon\nZeta"));
    edit->setGeometry(20, 20, 360, edit->height());
    window->show();
    edit->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    QScrollBar* bar = inner->verticalScrollBar();
    ASSERT_NE(bar, nullptr);
    ASSERT_GT(bar->maximum(), bar->minimum());
    bar->setValue(bar->maximum());
    QApplication::processEvents();

    const auto caretRectForBlock = [inner](int blockIndex) {
        QTextBlock block = inner->document()->begin();
        for (int index = 0; index < blockIndex && block.isValid(); ++index)
            block = block.next();
        return block.isValid() ? inner->cursorRect(QTextCursor(block)) : QRect();
    };
    const auto expectWholeTail = [edit, inner, bar, &caretRectForBlock](const char* phase) {
        const QRect firstVisible = caretRectForBlock(3);
        const QRect finalVisible = caretRectForBlock(5);
        const QString geometry =
            QStringLiteral("%1 value=%2 max=%3 page=%4 viewport=%5 lineHeight=%6 fontLine=%7 "
                           "first=%8,%9,%10,%11 final=%12,%13,%14,%15")
                .arg(QString::fromLatin1(phase))
                .arg(bar->value())
                .arg(bar->maximum())
                .arg(bar->pageStep())
                .arg(inner->viewport()->height())
                .arg(edit->lineHeight())
                .arg(QFontMetrics(inner->font()).lineSpacing())
                .arg(firstVisible.x())
                .arg(firstVisible.y())
                .arg(firstVisible.width())
                .arg(firstVisible.height())
                .arg(finalVisible.x())
                .arg(finalVisible.y())
                .arg(finalVisible.width())
                .arg(finalVisible.height());
        EXPECT_EQ(bar->value(), bar->maximum()) << geometry.toStdString();
        EXPECT_GE(firstVisible.top(), 0)
            << geometry.toStdString() << " must not clip Delta at the top";
        EXPECT_LT(finalVisible.bottom(), inner->viewport()->height())
            << geometry.toStdString() << " must not clip Zeta at the bottom";
    };
    expectWholeTail("Light tail");

    window->setProperty("fluentThemeOverride", static_cast<int>(FluentElement::Dark));
    edit->onThemeUpdated();
    QApplication::processEvents();
    QTest::qWait(1);
    QApplication::processEvents();
    expectWholeTail("Dark tail");

    window->setProperty("fluentThemeOverride", static_cast<int>(FluentElement::Light));
    edit->onThemeUpdated();
    QApplication::processEvents();
    QTest::qWait(1);
    QApplication::processEvents();
    expectWholeTail("returned Light tail");
}

TEST_F(TextEditTest, Contract_WidthReflowRecomputesVisibleLineHeight)
{
    TextEdit* edit = new TextEdit(window);
    edit->setLineHeight(24);
    edit->setMinVisibleLines(1);
    edit->setMaxVisibleLines(10);
    edit->setGeometry(0, 0, 420, 24);
    edit->setPlainText(QString(160, QLatin1Char('W')));
    window->show();
    QApplication::processEvents();

    const int wideHeight = edit->height();
    edit->resize(100, wideHeight);
    QApplication::processEvents();

    EXPECT_GT(edit->height(), wideHeight);
}

TEST_F(TextEditTest, Contract_BaseWidgetFocusForwardsToInnerEditor)
{
    TextEdit* edit = new TextEdit(window);
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    QWidget* widgetFacade = edit;
    widgetFacade->setFocus(Qt::TabFocusReason);
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    EXPECT_TRUE(inner->hasFocus());
    EXPECT_EQ(edit->focusProxy(), inner);
}

TEST_F(TextEditTest, Contract_ReapplyingCurrentTextPreservesUndoHistory)
{
    TextEdit* edit = new TextEdit(window);
    edit->setPlainText(QStringLiteral("Alpha"));
    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);

    inner->moveCursor(QTextCursor::End);
    inner->insertPlainText(QStringLiteral(" Beta"));
    ASSERT_TRUE(inner->document()->isUndoAvailable());

    edit->setPlainText(edit->toPlainText());

    EXPECT_TRUE(inner->document()->isUndoAvailable());
    inner->undo();
    EXPECT_EQ(edit->toPlainText(), QStringLiteral("Alpha"));
}

TEST_F(TextEditTest, Contract_VisibleLineBoundsRemainOrdered)
{
    TextEdit* edit = new TextEdit(window);

    edit->setMinVisibleLines(8);
    EXPECT_EQ(edit->minVisibleLines(), 8);
    EXPECT_EQ(edit->maxVisibleLines(), 8);

    edit->setMaxVisibleLines(3);
    EXPECT_EQ(edit->minVisibleLines(), 3);
    EXPECT_EQ(edit->maxVisibleLines(), 3);
    EXPECT_EQ(edit->height(), 3 * edit->lineHeight());
}

TEST_F(TextEditTest, ContentMargins)
{
    TextEdit* edit = new TextEdit(window);
    QMargins margins(12, 4, 12, 4);
    edit->setContentMargins(margins);
    EXPECT_EQ(edit->contentMargins(), margins);
}

TEST_F(TextEditTest, ContentMarginsOwnPaintedTextInsetsWithoutInflatingDefaults)
{
    auto* edit = new TextEdit(window);
    edit->setLineHeight(32);
    edit->setMinVisibleLines(2);
    edit->setMaxVisibleLines(5);
    edit->setContentMargins(QMargins(12, 10, 12, 8));
    edit->setPlainText(QStringLiteral("Alpha\nBeta"));
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);

    QTextCursor cursor(inner->document());
    cursor.movePosition(QTextCursor::Start);
    inner->setTextCursor(cursor);
    const QPoint firstCaret = inner->viewport()->mapTo(edit, inner->cursorRect().topLeft());
    EXPECT_GE(firstCaret.x(), 11);
    EXPECT_GE(firstCaret.y(), 9);

    cursor.movePosition(QTextCursor::End);
    inner->setTextCursor(cursor);
    const QPoint lastBottom = inner->viewport()->mapTo(edit, inner->cursorRect().bottomRight());
    EXPECT_GE(edit->height() - lastBottom.y(), 7);

    TextEdit compact;
    EXPECT_EQ(compact.height(), Spacing::ControlHeight::Standard)
        << "default margins must remain inside the standard line slot";
}

TEST_F(TextEditTest, FluentPropertiesDefaultsAndSetters)
{
    TextEdit* edit = new TextEdit(window);

    EXPECT_EQ(edit->contentMargins(),
              QMargins(Spacing::Padding::TextFieldHorizontal, Spacing::Padding::TextFieldVertical,
                       Spacing::Padding::TextFieldHorizontal, Spacing::Padding::TextFieldVertical));
    EXPECT_EQ(edit->fontRole(), Typography::FontRole::Body);
    EXPECT_EQ(edit->focusedBorderWidth(), Spacing::Border::Focused);
    EXPECT_EQ(edit->unfocusedBorderWidth(), Spacing::Border::Normal);
    EXPECT_EQ(edit->lineHeight(), Spacing::ControlHeight::Standard);
    EXPECT_EQ(edit->minVisibleLines(), 1);
    EXPECT_EQ(edit->maxVisibleLines(), 4);

    QSignalSpy spyFocused(edit, SIGNAL(focusedBorderWidthChanged()));
    QSignalSpy spyUnfocused(edit, SIGNAL(unfocusedBorderWidthChanged()));
    QSignalSpy spyLayout(edit, SIGNAL(layoutMetricsChanged()));
    QSignalSpy spyFont(edit, SIGNAL(fontRoleChanged()));

    edit->setFocusedBorderWidth(3);
    EXPECT_EQ(edit->focusedBorderWidth(), 3);
    EXPECT_EQ(spyFocused.count(), 1);

    edit->setUnfocusedBorderWidth(2);
    EXPECT_EQ(edit->unfocusedBorderWidth(), 2);
    EXPECT_EQ(spyUnfocused.count(), 1);

    edit->setMinVisibleLines(2);
    EXPECT_EQ(edit->minVisibleLines(), 2);
    EXPECT_EQ(spyLayout.count(), 1);

    edit->setMaxVisibleLines(6);
    EXPECT_EQ(edit->maxVisibleLines(), 6);
    EXPECT_EQ(spyLayout.count(), 2);

    edit->setFontRole(Typography::FontRole::Subtitle);
    EXPECT_EQ(edit->fontRole(), Typography::FontRole::Subtitle);
    EXPECT_EQ(spyFont.count(), 1);

    // 相同值不应再次触发信号
    edit->setFocusedBorderWidth(3);
    edit->setUnfocusedBorderWidth(2);
    edit->setMinVisibleLines(2);
    EXPECT_EQ(spyFocused.count(), 1);
    EXPECT_EQ(spyUnfocused.count(), 1);
    EXPECT_EQ(spyLayout.count(), 2);
}

TEST_F(TextEditTest, MinVisibleLinesClampsBelowContent)
{
    TextEdit* edit = new TextEdit(window);
    edit->setLineHeight(32);
    edit->setMinVisibleLines(2);
    edit->setMaxVisibleLines(4);

    // height = clampedLines × lineHeight（无额外 top/bottom padding）
    EXPECT_EQ(edit->height(), 2 * 32);

    edit->setPlainText("A\nB\nC");
    EXPECT_EQ(edit->height(), 3 * 32);

    edit->clear();
    EXPECT_EQ(edit->height(), 2 * 32);
}

TEST_F(TextEditTest, MaxVisibleLinesClampsAboveContent)
{
    TextEdit* edit = new TextEdit(window);
    edit->setLineHeight(32);
    edit->setMinVisibleLines(1);
    edit->setMaxVisibleLines(3);

    // 写入超过 3 行：高度固定在 maxVisibleLines × lineHeight，滚动条出现
    edit->setPlainText("A\nB\nC\nD\nE");
    EXPECT_EQ(edit->height(), 3 * 32);
}

TEST_F(TextEditTest, SingleLineDefaultHeight)
{
    // 默认 minVisibleLines=1：空控件高度应与单行 TextBox 等高（lineHeight = 32）
    TextEdit* edit = new TextEdit(window);
    EXPECT_EQ(edit->height(), Spacing::ControlHeight::Standard);
}

TEST_F(TextEditTest, ReadOnly)
{
    TextEdit* edit = new TextEdit(window);
    edit->setPlainText("read only content");
    edit->setReadOnly(true);
    EXPECT_TRUE(edit->isReadOnly());
    edit->setReadOnly(false);
    EXPECT_FALSE(edit->isReadOnly());
}

TEST_F(TextEditTest, StandardEditingActionsUseFluentContextMenu)
{
    TextEdit* edit = new TextEdit(window);
    edit->setPlainText(QStringLiteral("Alpha Beta"));
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);

    bool sawFluentMenu = false;
    bool sawCopy = false;
    bool sawSelectAll = false;
    bool sawCopyGlyph = false;
    bool sawDeleteGlyph = false;
    bool sawSelectAllGlyph = false;
    QTimer::singleShot(0, [&]() {
        auto* menu =
            qobject_cast<fluent::menus_toolbars::FluentMenu*>(QApplication::activePopupWidget());
        sawFluentMenu = menu != nullptr;
        if (!menu)
            return;

        EXPECT_EQ(menu->objectName(), QStringLiteral("FluentTextEdit.ContextMenu"));
        EXPECT_EQ(menu->fontStyle(), Typography::FontRole::Caption);
        EXPECT_EQ(menu->font().pixelSize(), Typography::FontSize::Caption);
        EXPECT_FALSE(menu->property("_fluentqt_menuQuietSeparators").toBool());
        for (QAction* action : menu->actions()) {
            if (!action->isSeparator()) {
                EXPECT_LT(menu->actionGeometry(action).height(),
                          ::Spacing::ControlHeight::Standard);
            }
            const QString text = action->text();
            const bool isCopy = text.contains(QStringLiteral("Copy"), Qt::CaseInsensitive);
            const bool isSelectAll = text.contains(QStringLiteral("Select"), Qt::CaseInsensitive) &&
                                     text.contains(QStringLiteral("All"), Qt::CaseInsensitive);
            const bool isDelete = text.contains(QStringLiteral("Delete"), Qt::CaseInsensitive);
            sawCopy = sawCopy || isCopy;
            sawSelectAll = sawSelectAll || isSelectAll;
            sawCopyGlyph = sawCopyGlyph || (isCopy && !action->icon().isNull());
            sawDeleteGlyph = sawDeleteGlyph || (isDelete && !action->icon().isNull());
            sawSelectAllGlyph = sawSelectAllGlyph || (isSelectAll && !action->icon().isNull());
            if (!action->icon().isNull()) {
                const QSize iconSize = action->icon().actualSize(QSize(64, 64));
                const int maximumBackingExtent = qCeil(Typography::IconSize::Standard *
                                                       qMax<qreal>(1.0, menu->devicePixelRatioF()));
                EXPECT_GT(iconSize.width(), 0);
                EXPECT_LE(iconSize.width(), maximumBackingExtent);
                EXPECT_LE(iconSize.height(), maximumBackingExtent);
            }
        }
        menu->close();
    });

    const QPoint localPos = inner->viewport()->rect().center();
    const QPoint globalPos = inner->viewport()->mapToGlobal(localPos);
    QContextMenuEvent event(QContextMenuEvent::Mouse, localPos, globalPos);
    QApplication::sendEvent(inner->viewport(), &event);

    EXPECT_TRUE(event.isAccepted());
    QTRY_VERIFY_WITH_TIMEOUT(sawFluentMenu, 1000);
    EXPECT_TRUE(sawFluentMenu);
    EXPECT_TRUE(sawCopy);
    EXPECT_TRUE(sawSelectAll);
    EXPECT_TRUE(sawCopyGlyph);
    EXPECT_TRUE(sawDeleteGlyph);
    EXPECT_TRUE(sawSelectAllGlyph);
}

TEST_F(TextEditTest, StandardEditingActionsReceiveIconsAndShortcutTextWithoutPlatformMetadata)
{
    window->show();
    QApplication::processEvents();

    auto* standardMenu = new QMenu(window);
    standardMenu->addAction(QStringLiteral("Undo"));
    auto* disabledRedo = standardMenu->addAction(QStringLiteral("Redo"));
    disabledRedo->setEnabled(false);
    standardMenu->addSeparator();
    standardMenu->addAction(QStringLiteral("Cut"));
    standardMenu->addAction(QStringLiteral("Copy"));
    auto* disabledPaste = standardMenu->addAction(QStringLiteral("Paste"));
    disabledPaste->setEnabled(false);
    standardMenu->addAction(QStringLiteral("Delete"));
    standardMenu->addSeparator();
    standardMenu->addAction(QStringLiteral("Select All"));

    bool sawFluentMenu = false;
    int editingActionCount = 0;
    int iconCount = 0;
    int paintedIconCount = 0;
    int enabledActionCount = 0;
    int enabledShortcutCount = 0;
    int disabledActionCount = 0;
    int disabledShortcutCount = 0;
    QTimer::singleShot(0, [&]() {
        QWidget* popup = QApplication::activePopupWidget();
        auto* menu = qobject_cast<fluent::menus_toolbars::FluentMenu*>(popup);
        sawFluentMenu = menu != nullptr;
        if (menu) {
            EXPECT_FALSE(standardMenu->isVisible());
            EXPECT_TRUE(standardMenu->isHidden());
            EXPECT_TRUE(standardMenu->testAttribute(Qt::WA_DontShowOnScreen));
            for (QAction* action : menu->actions()) {
                if (action->isSeparator())
                    continue;
                ++editingActionCount;
                const QString shortcutText = menu->shortcutTextForAction(action);
                EXPECT_FALSE(shortcutText.isEmpty());
                EXPECT_FALSE(menu->itemShortcutGeometry(action).isEmpty());
                if (action->isEnabled()) {
                    ++enabledActionCount;
                    if (!shortcutText.isEmpty())
                        ++enabledShortcutCount;
                } else {
                    ++disabledActionCount;
                    if (!shortcutText.isEmpty())
                        ++disabledShortcutCount;
                }
                if (!action->icon().isNull()) {
                    ++iconCount;
                    const QImage image = action->icon().pixmap(QSize(16, 16)).toImage();
                    bool hasVisiblePixel = false;
                    for (int y = 0; y < image.height() && !hasVisiblePixel; ++y) {
                        for (int x = 0; x < image.width(); ++x) {
                            if (image.pixelColor(x, y).alpha() > 0) {
                                hasVisiblePixel = true;
                                break;
                            }
                        }
                    }
                    if (hasVisiblePixel)
                        ++paintedIconCount;
                }
            }
        }
        if (popup)
            popup->close();
    });

    EXPECT_TRUE(fluent::menus_toolbars::detail::showTextEditingContextMenu(
        window, standardMenu, window->mapToGlobal(QPoint(40, 40)),
        QStringLiteral("FluentTextEdit.PlatformFallbackMenu")));
    QTRY_VERIFY_WITH_TIMEOUT(sawFluentMenu, 1000);
    EXPECT_TRUE(sawFluentMenu);
    EXPECT_EQ(editingActionCount, 7);
    EXPECT_EQ(iconCount, editingActionCount);
    EXPECT_EQ(paintedIconCount, editingActionCount);
    EXPECT_EQ(enabledActionCount, 5);
    EXPECT_EQ(enabledShortcutCount, enabledActionCount);
    EXPECT_EQ(disabledActionCount, 2);
    EXPECT_EQ(disabledShortcutCount, disabledActionCount);
}

TEST_F(TextEditTest, UndoRedoRemainFunctionalFromKeyboardAndContextMenu)
{
    TextEdit* edit = new TextEdit(window);
    edit->setPlainText(QStringLiteral("Alpha"));
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    inner->moveCursor(QTextCursor::End);
    inner->insertPlainText(QStringLiteral(" Beta"));
    const QString editedText = QStringLiteral("Alpha Beta");
    ASSERT_EQ(inner->toPlainText(), editedText);

    inner->setFocus(Qt::OtherFocusReason);
    QTest::keySequence(inner, QKeySequence(QKeySequence::Undo));
    EXPECT_EQ(inner->toPlainText(), QStringLiteral("Alpha"));
    QTest::keySequence(inner, QKeySequence(QKeySequence::Redo));
    EXPECT_EQ(inner->toPlainText(), editedText);

    ASSERT_TRUE(triggerContextAction(inner, QKeySequence::Undo));
    EXPECT_EQ(inner->toPlainText(), QStringLiteral("Alpha"));
    ASSERT_TRUE(triggerContextAction(inner, QKeySequence::Redo));
    EXPECT_EQ(inner->toPlainText(), editedText);

    // A light/dark palette refresh must not add an invisible formatting
    // command ahead of the user's text edit in QTextDocument's undo stack.
    edit->onThemeUpdated();
    QTest::keySequence(inner, QKeySequence(QKeySequence::Undo));
    EXPECT_EQ(inner->toPlainText(), QStringLiteral("Alpha"));
    QTest::keySequence(inner, QKeySequence(QKeySequence::Redo));
    EXPECT_EQ(inner->toPlainText(), editedText);
}

TEST_F(TextEditTest, ContextMenuVisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    TextEdit* edit = new TextEdit(window);
    edit->setPlainText(QStringLiteral("Alpha Beta\nGamma Delta"));
    edit->setMinVisibleLines(2);
    edit->setMaxVisibleLines(2);
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    QTextCursor cursor = inner->textCursor();
    cursor.select(QTextCursor::Document);
    inner->setTextCursor(cursor);

    const QPoint localPos = inner->viewport()->rect().center();
    const QPoint globalPos = inner->viewport()->mapToGlobal(localPos);

    if (tests::support::shouldCaptureVisualSnapshot()) {
        bool snapshotSaved = false;
        QString snapshotError;
        QTimer::singleShot(0, [&]() {
            auto* menu = qobject_cast<fluent::menus_toolbars::FluentMenu*>(
                QApplication::activePopupWidget());
            if (!menu) {
                snapshotError = QStringLiteral("Fluent context menu did not become active");
                return;
            }

            tests::support::VisualSnapshotOptions options;
            options.windowSize = menu->size();
            options.variant = QStringLiteral("light");
            const auto result = tests::support::captureVisualSnapshot(menu, options);
            snapshotSaved = result;
            if (!result)
                snapshotError = QString::fromUtf8(result.message());
            menu->close();
        });

        QContextMenuEvent event(QContextMenuEvent::Mouse, localPos, globalPos);
        QApplication::sendEvent(inner->viewport(), &event);
        QTRY_VERIFY_WITH_TIMEOUT(snapshotSaved || !snapshotError.isEmpty(), 1000);
        ASSERT_TRUE(snapshotSaved) << snapshotError.toStdString();
        return;
    }

    QTimer::singleShot(0, [inner, localPos, globalPos]() {
        QContextMenuEvent event(QContextMenuEvent::Mouse, localPos, globalPos);
        QApplication::sendEvent(inner->viewport(), &event);
    });
    qApp->exec();
}

TEST_F(TextEditTest, ScrollChainingPropertyControlsBoundaryWheel)
{
    TextEdit* edit = new TextEdit(window);
    edit->setMinVisibleLines(3);
    edit->setMaxVisibleLines(3);
    edit->setPlainText("A\nB\nC\nD\nE\nF\nG\nH");
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(inner->verticalScrollBar(), nullptr);
    ASSERT_GT(inner->verticalScrollBar()->maximum(), inner->verticalScrollBar()->minimum());

    EXPECT_FALSE(edit->isScrollChainingEnabled());
    QSignalSpy spy(edit, &TextEdit::scrollChainingEnabledChanged);

    inner->verticalScrollBar()->setValue(inner->verticalScrollBar()->maximum());
    QWheelEvent containedWheel = makeWheelEvent(inner->viewport(), QPoint(0, 0), QPoint(0, -120));
    containedWheel.setAccepted(false);
    QApplication::sendEvent(inner->viewport(), &containedWheel);
    QApplication::processEvents();

    EXPECT_TRUE(containedWheel.isAccepted());
    EXPECT_EQ(inner->verticalScrollBar()->value(), inner->verticalScrollBar()->maximum());

    edit->setScrollChainingEnabled(true);
    EXPECT_TRUE(edit->isScrollChainingEnabled());
    EXPECT_EQ(spy.count(), 1);
    edit->setScrollChainingEnabled(true);
    EXPECT_EQ(spy.count(), 1);

    QWheelEvent chainedWheel = makeWheelEvent(inner->viewport(), QPoint(0, 0), QPoint(0, -120));
    chainedWheel.setAccepted(false);
    QApplication::sendEvent(inner->viewport(), &chainedWheel);
    QApplication::processEvents();

    EXPECT_FALSE(chainedWheel.isAccepted());
    EXPECT_EQ(inner->verticalScrollBar()->value(), inner->verticalScrollBar()->maximum());
}

TEST_F(TextEditTest, WheelPassesThroughWhenContentFits)
{
    TextEdit* edit = new TextEdit(window);
    edit->setMinVisibleLines(3);
    edit->setMaxVisibleLines(3);
    edit->setPlainText("A\nB");
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(inner->verticalScrollBar(), nullptr);
    ASSERT_EQ(inner->verticalScrollBar()->maximum(), inner->verticalScrollBar()->minimum());

    QWheelEvent wheel = makeWheelEvent(inner->viewport(), QPoint(0, 0), QPoint(0, -120));
    wheel.setAccepted(false);
    QApplication::sendEvent(inner->viewport(), &wheel);
    QApplication::processEvents();

    EXPECT_FALSE(wheel.isAccepted());
}

TEST_F(TextEditTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    Label* header = new Label("TextEdit - 自适应行高 + 垂直居中:", window);
    header->anchors()->top = {window, Edge::Top, 30};
    header->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(header);

    // 默认 1 行（同 LineEdit 高度），自动居中
    TextEdit* edit1 = new TextEdit(window);
    edit1->setPlaceholderText("Type here... (auto grows up to 4 lines)");
    edit1->anchors()->top = {header, Edge::Bottom, 8};
    edit1->anchors()->left = {window, Edge::Left, 40};
    edit1->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(edit1);

    Label* header2 = new Label("预填 2 行（选区应使用强调色）:", window);
    header2->anchors()->top = {edit1, Edge::Bottom, 12};
    header2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(header2);

    TextEdit* edit2 = new TextEdit(window);
    edit2->setPlainText("First line\nSecond line");
    edit2->anchors()->top = {header2, Edge::Bottom, 8};
    edit2->anchors()->left = {window, Edge::Left, 40};
    edit2->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(edit2);

    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFluentStyle(Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right = {window, Edge::Right, -30};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() ==
                                                fluent::FluentElement::Light
                                            ? fluent::FluentElement::Dark
                                            : fluent::FluentElement::Light);
    });

    window->show();
    if (QTextEdit* inner = innerTextEdit(edit2)) {
        QTextCursor cursor = inner->textCursor();
        cursor.setPosition(0);
        cursor.setPosition(QStringLiteral("First line").size(), QTextCursor::KeepAnchor);
        inner->setTextCursor(cursor);
        inner->setFocus(Qt::OtherFocusReason);
    }
    if (tests::support::shouldCaptureVisualSnapshot()) {
        ASSERT_TRUE(tests::support::captureVisualSnapshot(window));
        return;
    }

    qApp->exec();
}

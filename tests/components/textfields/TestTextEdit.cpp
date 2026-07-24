#include <gtest/gtest.h>
#include <QApplication>
#include <QContextMenuEvent>
#include <QMetaProperty>
#include <QMenu>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QWheelEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include "QtTestEnvironment.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/TextEdit.h"
#include "components/textfields/Label.h"
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Spacing.h"
#include "design/Typography.h"

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

class TextEditTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(500, 400);
        window->setWindowTitle("Fluent TextEdit Test");
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

QWheelEvent makeWheelEvent(QWidget* target, QPoint pixelDelta, QPoint angleDelta,
                           Qt::ScrollPhase phase = Qt::NoScrollPhase)
{
    const QPointF pos = target->rect().center();
    const QPointF globalPos = target->mapToGlobal(pos.toPoint());
    return QWheelEvent(pos, globalPos, pixelDelta, angleDelta,
                       Qt::NoButton, Qt::NoModifier, phase, false);
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
            const QKeySequence embedded(
                action->text().mid(tabIndex + 1).trimmed(),
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
        auto* menu = qobject_cast<fluent::menus_toolbars::FluentMenu*>(
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

    const QPoint localPos = inner->viewport()->rect().center();
    const QPoint globalPos = inner->viewport()->mapToGlobal(localPos);
    QContextMenuEvent event(QContextMenuEvent::Mouse, localPos, globalPos);
    QApplication::sendEvent(inner->viewport(), &event);
    return triggered;
}

} // namespace

TEST_F(TextEditTest, TextAndPlaceholder) {
    TextEdit* edit = new TextEdit(window);
    edit->setPlaceholderText("Multi-line placeholder");
    EXPECT_EQ(edit->placeholderText(), "Multi-line placeholder");

    edit->setPlainText("line1\nline2");
    EXPECT_EQ(edit->toPlainText(), "line1\nline2");
}

TEST_F(TextEditTest, Contract_LayoutPropertiesAreAvailableThroughQtMetaObject) {
    TextEdit* edit = new TextEdit(window);
    const QMetaObject* metaObject = edit->metaObject();

    for (const char* propertyName : {
             "lineHeight", "minVisibleLines", "maxVisibleLines"}) {
        const int propertyIndex = metaObject->indexOfProperty(propertyName);
        ASSERT_GE(propertyIndex, 0) << propertyName;
        const QMetaProperty property = metaObject->property(propertyIndex);
        EXPECT_TRUE(property.isReadable()) << propertyName;
        EXPECT_TRUE(property.isWritable()) << propertyName;
        EXPECT_TRUE(property.hasNotifySignal()) << propertyName;
    }
}

TEST_F(TextEditTest, Contract_WidthReflowRecomputesVisibleLineHeight) {
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

TEST_F(TextEditTest, Contract_BaseWidgetFocusForwardsToInnerEditor) {
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

TEST_F(TextEditTest, Contract_ReapplyingCurrentTextPreservesUndoHistory) {
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

TEST_F(TextEditTest, Contract_VisibleLineBoundsRemainOrdered) {
    TextEdit* edit = new TextEdit(window);

    edit->setMinVisibleLines(8);
    EXPECT_EQ(edit->minVisibleLines(), 8);
    EXPECT_EQ(edit->maxVisibleLines(), 8);

    edit->setMaxVisibleLines(3);
    EXPECT_EQ(edit->minVisibleLines(), 3);
    EXPECT_EQ(edit->maxVisibleLines(), 3);
    EXPECT_EQ(edit->height(), 3 * edit->lineHeight());
}

TEST_F(TextEditTest, ContentMargins) {
    TextEdit* edit = new TextEdit(window);
    QMargins margins(12, 4, 12, 4);
    edit->setContentMargins(margins);
    EXPECT_EQ(edit->contentMargins(), margins);
}

TEST_F(TextEditTest, FluentPropertiesDefaultsAndSetters) {
    TextEdit* edit = new TextEdit(window);

    EXPECT_EQ(edit->contentMargins(),
              QMargins(Spacing::Padding::TextFieldHorizontal, Spacing::Padding::TextFieldVertical,
                       Spacing::Padding::TextFieldHorizontal, Spacing::Padding::TextFieldVertical));
    EXPECT_EQ(edit->fontRole(), Typography::FontRole::Body);
    EXPECT_EQ(edit->focusedBorderWidth(),   Spacing::Border::Focused);
    EXPECT_EQ(edit->unfocusedBorderWidth(), Spacing::Border::Normal);
    EXPECT_EQ(edit->lineHeight(), Spacing::ControlHeight::Standard);
    EXPECT_EQ(edit->minVisibleLines(), 1);
    EXPECT_EQ(edit->maxVisibleLines(), 4);

    QSignalSpy spyFocused(edit,   SIGNAL(focusedBorderWidthChanged()));
    QSignalSpy spyUnfocused(edit, SIGNAL(unfocusedBorderWidthChanged()));
    QSignalSpy spyLayout(edit,    SIGNAL(layoutMetricsChanged()));
    QSignalSpy spyFont(edit,      SIGNAL(fontRoleChanged()));

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

TEST_F(TextEditTest, MinVisibleLinesClampsBelowContent) {
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

TEST_F(TextEditTest, MaxVisibleLinesClampsAboveContent) {
    TextEdit* edit = new TextEdit(window);
    edit->setLineHeight(32);
    edit->setMinVisibleLines(1);
    edit->setMaxVisibleLines(3);

    // 写入超过 3 行：高度固定在 maxVisibleLines × lineHeight，滚动条出现
    edit->setPlainText("A\nB\nC\nD\nE");
    EXPECT_EQ(edit->height(), 3 * 32);
}

TEST_F(TextEditTest, SingleLineDefaultHeight) {
    // 默认 minVisibleLines=1：空控件高度应与单行 TextBox 等高（lineHeight = 32）
    TextEdit* edit = new TextEdit(window);
    EXPECT_EQ(edit->height(), Spacing::ControlHeight::Standard);
}

TEST_F(TextEditTest, ReadOnly) {
    TextEdit* edit = new TextEdit(window);
    edit->setPlainText("read only content");
    edit->setReadOnly(true);
    EXPECT_TRUE(edit->isReadOnly());
    edit->setReadOnly(false);
    EXPECT_FALSE(edit->isReadOnly());
}

TEST_F(TextEditTest, StandardEditingActionsUseFluentContextMenu) {
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
        auto* menu = qobject_cast<fluent::menus_toolbars::FluentMenu*>(
            QApplication::activePopupWidget());
        sawFluentMenu = menu != nullptr;
        if (!menu)
            return;

        EXPECT_EQ(menu->objectName(), QStringLiteral("FluentTextEdit.ContextMenu"));
        EXPECT_EQ(menu->fontStyle(), Typography::FontRole::Caption);
        EXPECT_EQ(menu->font().pixelSize(), 10);
        for (QAction* action : menu->actions()) {
            const QString text = action->text();
            const bool isCopy = text.contains(QStringLiteral("Copy"), Qt::CaseInsensitive);
            const bool isSelectAll = text.contains(QStringLiteral("Select"), Qt::CaseInsensitive)
                && text.contains(QStringLiteral("All"), Qt::CaseInsensitive);
            const bool isDelete = text.contains(QStringLiteral("Delete"), Qt::CaseInsensitive);
            sawCopy = sawCopy || isCopy;
            sawSelectAll = sawSelectAll || isSelectAll;
            sawCopyGlyph = sawCopyGlyph
                || (isCopy && !action->icon().isNull());
            sawDeleteGlyph = sawDeleteGlyph
                || (isDelete && !action->icon().isNull());
            sawSelectAllGlyph = sawSelectAllGlyph
                || (isSelectAll && !action->icon().isNull());
        }
        menu->close();
    });

    const QPoint localPos = inner->viewport()->rect().center();
    const QPoint globalPos = inner->viewport()->mapToGlobal(localPos);
    QContextMenuEvent event(QContextMenuEvent::Mouse, localPos, globalPos);
    QApplication::sendEvent(inner->viewport(), &event);

    EXPECT_TRUE(event.isAccepted());
    EXPECT_TRUE(sawFluentMenu);
    EXPECT_TRUE(sawCopy);
    EXPECT_TRUE(sawSelectAll);
    EXPECT_TRUE(sawCopyGlyph);
    EXPECT_TRUE(sawDeleteGlyph);
    EXPECT_TRUE(sawSelectAllGlyph);
}

TEST_F(TextEditTest, UndoRedoRemainFunctionalFromKeyboardAndContextMenu) {
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

TEST_F(TextEditTest, ContextMenuVisualCheck) {
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
        ASSERT_TRUE(snapshotSaved) << snapshotError.toStdString();
        return;
    }

    QTimer::singleShot(0, [inner, localPos, globalPos]() {
        QContextMenuEvent event(QContextMenuEvent::Mouse, localPos, globalPos);
        QApplication::sendEvent(inner->viewport(), &event);
    });
    qApp->exec();
}

TEST_F(TextEditTest, ScrollChainingPropertyControlsBoundaryWheel) {
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

TEST_F(TextEditTest, WheelPassesThroughWhenContentFits) {
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

// ── Design-language × theme compatibility ────────────────────────────────────
//
// TextEdit paints a per-brand frame (Fluent / Material 3 / macOS) under each App theme
// (Light / Dark). This suite grabs the rendered control across the full {language × theme}
// matrix to lock in that every combination paints, never crashes, and produces painted
// content. Design language + theme are GLOBAL state, so TearDown restores the built-in
// defaults. zh_CN: TextEdit 按品牌(Fluent / Material 3 / macOS)在明暗主题下分支绘制外框。
// 本套件遍历 {设计语言 × 主题} 全矩阵抓取渲染结果,确保每种组合都能绘制、不崩溃且有绘制内容。
// 设计语言与主题是全局状态,故 TearDown 恢复内置默认值。

class TextEditDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so other suites start clean.
        // zh_CN: 设计语言与主题是全局状态——重置以保证其它套件从干净状态开始。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(TextEditDesignLanguageTest, AllLanguagesThemesPaint) {
    const fluent::FluentElement::DesignLanguage langs[] = {
        fluent::FluentElement::DesignFluent,
        fluent::FluentElement::DesignMaterial,
        fluent::FluentElement::DesignCupertino,
    };
    const fluent::FluentElement::Theme themes[] = {
        fluent::FluentElement::Light,
        fluent::FluentElement::Dark,
    };

    for (auto lang : langs) {
        for (auto theme : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang);
            fluent::FluentElement::setTheme(theme);

            TextEdit te;
            te.setPlainText("Multi-line\nsample text");
            te.resize(240, 80);

            QImage img = te.grab().toImage();

            // No crash + valid image. zh_CN: 不崩溃 + 图像有效。
            ASSERT_FALSE(img.isNull()) << "lang=" << lang << " theme=" << theme;
            EXPECT_GT(img.width(), 0) << "lang=" << lang << " theme=" << theme;
            EXPECT_GT(img.height(), 0) << "lang=" << lang << " theme=" << theme;

            // Frame/text must paint content: some pixel differs from the top-left background.
            // zh_CN: 外框/文本必须绘制内容:存在与左上角背景不同的像素。
            const QRgb bg = img.pixel(0, 0);
            bool paintedContent = false;
            for (int y = 0; y < img.height() && !paintedContent; ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    if (img.pixel(x, y) != bg) {
                        paintedContent = true;
                        break;
                    }
                }
            }
            EXPECT_TRUE(paintedContent)
                << "painted nothing for lang=" << lang << " theme=" << theme;
        }
    }
}

TEST_F(TextEditTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    Label* header = new Label("TextEdit - 自适应行高 + 垂直居中:", window);
    header->anchors()->top  = {window, Edge::Top,  30};
    header->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(header);

    // 默认 1 行（同 LineEdit 高度），自动居中
    TextEdit* edit1 = new TextEdit(window);
    edit1->setPlaceholderText("Type here... (auto grows up to 4 lines)");
    edit1->anchors()->top   = {header, Edge::Bottom, 8};
    edit1->anchors()->left  = {window, Edge::Left, 40};
    edit1->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(edit1);

    Label* header2 = new Label("预填 2 行（高度 = 64px）:", window);
    header2->anchors()->top  = {edit1, Edge::Bottom, 12};
    header2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(header2);

    TextEdit* edit2 = new TextEdit(window);
    edit2->setPlainText("First line\nSecond line");
    edit2->anchors()->top   = {header2, Edge::Bottom, 8};
    edit2->anchors()->left  = {window, Edge::Left, 40};
    edit2->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(edit2);

    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFluentStyle(Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right  = {window, Edge::Right,  -30};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                    ? fluent::FluentElement::Dark
                                    : fluent::FluentElement::Light);
    });

    window->show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        ASSERT_TRUE(tests::support::captureVisualSnapshot(window));
        return;
    }

    qApp->exec();
}

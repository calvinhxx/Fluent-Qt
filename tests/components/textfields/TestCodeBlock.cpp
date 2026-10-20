#include <gtest/gtest.h>
#include <QApplication>
#include <QClipboard>
#include <QPointer>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>
#include <QTextDocument>
#include <QVBoxLayout>

#include <FluentQt/TextFields.h>
#include "components/textfields/private/CodeHighlighter_p.h"
#include "compatibility/private/ClipboardCompat_p.h"
#include "components/basicinput/Button.h"
#include "components/scrolling/ScrollView.h"

using fluent::textfields::CodeBlock;
using fluent::textfields::Label;
using fluent::basicinput::Button;
using fluent::scrolling::ScrollView;

namespace {
class CodeBlockTest : public testing::Test {
protected:
    void SetUp() override
    {
        previousTheme = fluent::FluentElement::currentTheme();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
    void TearDown() override { fluent::FluentElement::setTheme(previousTheme); }
    fluent::FluentElement::Theme previousTheme;
};
} // namespace

TEST_F(CodeBlockTest, SmallPublicContractNormalizesValuesAndDoesNotRepeatSignals)
{
    CodeBlock block;
    EXPECT_TRUE(block.isExpanded());
    EXPECT_TRUE(block.isWordWrapEnabled());
    EXPECT_EQ(block.maxVisibleLines(), 16);
    EXPECT_EQ(block.language(), QStringLiteral("plain"));
    auto* copy = block.findChild<Button*>(QStringLiteral("fluentCodeBlockCopyButton"));
    ASSERT_NE(copy, nullptr);
    EXPECT_FALSE(copy->isEnabled());
    QSignalSpy languageChanged(&block, &CodeBlock::languageChanged);
    block.setLanguage(QStringLiteral(" C++ "));
    block.setLanguage(QStringLiteral("cpp"));
    EXPECT_EQ(languageChanged.count(), 1);
    EXPECT_EQ(block.headerText(), QStringLiteral("C++"));
    block.setLanguage(QStringLiteral("py"));
    EXPECT_EQ(block.language(), QStringLiteral("python"));
    block.setLanguage(QStringLiteral("JSON"));
    EXPECT_EQ(block.language(), QStringLiteral("json"));
    QSignalSpy limitChanged(&block, &CodeBlock::maxVisibleLinesChanged);
    block.setMaxVisibleLines(-1);
    block.setMaxVisibleLines(0);
    EXPECT_EQ(limitChanged.count(), 1);
    EXPECT_EQ(block.maxVisibleLines(), 0);
    QSignalSpy codeChanged(&block, &CodeBlock::codeChanged);
    block.setCode(QStringLiteral("{\"value\": 2}"));
    block.setCode(block.code());
    EXPECT_EQ(codeChanged.count(), 1);
    EXPECT_TRUE(copy->isEnabled());
}

TEST_F(CodeBlockTest, CopyAndSelectionPreserveSourceInHighlightedPlainAndHighContrastModes)
{
    const QString source = QStringLiteral("prefix\tType::Nested::Value, \"real\u200bvalue\";  \r\n\tsuffix");
    CodeBlock block(source);
    block.resize(420, block.sizeHint().height());
    block.show();
    auto* label = block.findChild<Label*>(QStringLiteral("fluentCodeBlockText"));
    ASSERT_NE(label, nullptr);
    QSignalSpy copied(&block, &CodeBlock::copyFinished);
    for (const QString& language :
        {QStringLiteral("cpp"), QStringLiteral("python"), QStringLiteral("qml"), QStringLiteral("unknown")}) {
        SCOPED_TRACE(language.toStdString());
        block.setLanguage(language);
        for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::HighContrast}) {
            fluent::FluentElement::setTheme(theme);
            QApplication::processEvents();
            block.copy();
            EXPECT_EQ(QApplication::clipboard()->text(), source);
            EXPECT_TRUE(copied.last().at(0).toBool());
            QTest::keySequence(label, QKeySequence(QKeySequence::SelectAll));
            QTest::keySequence(label, QKeySequence(QKeySequence::Copy));
            EXPECT_EQ(QApplication::clipboard()->text(), source);
            QTextDocument rendered;
            rendered.setHtml(label->text());
            const int end = rendered.toPlainText().indexOf(QStringLiteral("suffix"));
            label->setSelection(6, end - 6);
            QTest::keySequence(label, QKeySequence(QKeySequence::Copy));
            EXPECT_EQ(QApplication::clipboard()->text(), source.mid(6, source.indexOf(QStringLiteral("suffix")) - 6));
        }
    }
}

TEST_F(CodeBlockTest, CopyButtonIsKeyboardReachableAndDoesNotToggleFolding)
{
    CodeBlock block(QStringLiteral("answer = 42"));
    block.resize(400, block.sizeHint().height());
    block.show();
    QApplication::processEvents();
    auto* copy = block.findChild<Button*>(QStringLiteral("fluentCodeBlockCopyButton"));
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy->focusPolicy(), Qt::NoFocus);
    EXPECT_FALSE(copy->accessibleName().isEmpty());
    QSignalSpy expanded(&block, &CodeBlock::expandedChanged);
    QTest::mouseClick(copy, Qt::LeftButton);
    EXPECT_TRUE(block.isExpanded());
    EXPECT_EQ(expanded.count(), 0);
    block.setExpandedAnimated(false, false);
    EXPECT_EQ(block.findChild<Label*>(QStringLiteral("fluentCodeBlockText"))->focusPolicy(), Qt::NoFocus);
    copy->click();
    EXPECT_FALSE(block.isExpanded());
    EXPECT_EQ(QApplication::clipboard()->text(), block.code());
}

TEST_F(CodeBlockTest, BoundedContentScrollsAndUnlimitedContentReflows)
{
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    layout->setAlignment(Qt::AlignTop);
    auto* block = new CodeBlock(QStringLiteral("result = fluentqt.Button.ButtonStyle.Accent\n").repeated(40), &window);
    block->setLanguage(QStringLiteral("python"));
    block->setLayoutDirection(Qt::RightToLeft);
    block->setMaxVisibleLines(5);
    layout->addWidget(block);
    window.resize(500, 800);
    window.show();
    QApplication::processEvents();
    auto* scroll = block->findChild<ScrollView*>(QStringLiteral("fluentCodeBlockScroll"));
    auto* label = block->findChild<Label*>(QStringLiteral("fluentCodeBlockText"));
    ASSERT_NE(scroll, nullptr);
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(scroll->layoutDirection(), Qt::LeftToRight);
    Label fiveLines;
    fiveLines.setFont(label->font());
    fiveLines.setTextFormat(Qt::RichText);
    fiveLines.setText(QStringLiteral("<span>x<br/>x<br/>x<br/>x<br/>x</span>"));
    const int fiveLineHeight = fiveLines.heightForWidth(label->width());
    EXPECT_GE(scroll->viewport()->height(), fiveLineHeight);
    EXPECT_LT(scroll->height(), label->height() / 2);
    EXPECT_GT(scroll->verticalScrollBar()->maximum(), 0);
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    EXPECT_GT(scroll->verticalScrollBar()->value(), 0);
    block->setCode(
        QStringLiteral("result = fluentqt.Button.ButtonStyle.Accent + fluentqt.Button.ButtonLayout.IconBefore\n")
            .repeated(3));
    EXPECT_EQ(scroll->verticalScrollBar()->value(), 0);
    block->setMaxVisibleLines(0);
    QApplication::processEvents();
    const int wideHeight = block->height();
    window.resize(280, 800);
    QApplication::processEvents();
    EXPECT_GT(block->height(), wideHeight);
    EXPECT_EQ(scroll->verticalScrollBar()->maximum(), 0);
    EXPECT_EQ(scroll->horizontalScrollBar()->maximum(), 0);
    block->setWordWrap(false);
    QApplication::processEvents();
    EXPECT_GT(scroll->horizontalScrollBar()->maximum(), 0);
    EXPECT_LT(block->height(), wideHeight);
}

TEST_F(CodeBlockTest, VisibleLineLimitFitsRowsWithFallbackFontsAndWrapOpportunities)
{
    const QString sixLines = QStringLiteral("// Example\nitems = [\n")
        + QStringLiteral("    {\"name\": \"中文\", \"path\": \"workspace/design\"},\n").repeated(3)
        + QStringLiteral("]");
    CodeBlock reference(sixLines);
    reference.setLanguage(QStringLiteral("python"));
    reference.setMaxVisibleLines(0);
    reference.setWordWrap(false);
    reference.resize(700, reference.height());
    reference.show();
    CodeBlock bounded(sixLines + QStringLiteral("\n") + sixLines);
    bounded.setLanguage(QStringLiteral("python"));
    bounded.setMaxVisibleLines(6);
    bounded.setWordWrap(false);
    bounded.resize(700, bounded.height());
    bounded.show();
    QApplication::processEvents();
    auto* fullText = reference.findChild<Label*>(QStringLiteral("fluentCodeBlockText"));
    auto* scroll = bounded.findChild<ScrollView*>(QStringLiteral("fluentCodeBlockScroll"));
    ASSERT_NE(fullText, nullptr);
    ASSERT_NE(scroll, nullptr);
    EXPECT_GE(scroll->viewport()->height(), fullText->height());
    EXPECT_LE(scroll->viewport()->height(), fullText->height() + 1);
    EXPECT_GT(scroll->verticalScrollBar()->maximum(), 0);
}

TEST_F(CodeBlockTest, CollapsedUpdatesAreLazyAndExpansionUsesTheLatestCode)
{
    CodeBlock block(QStringLiteral("old"));
    block.setExpandedAnimated(false, false);
    auto* label = block.findChild<Label*>(QStringLiteral("fluentCodeBlockText"));
    ASSERT_NE(label, nullptr);
    block.setCode(QStringLiteral("new <b>source</b>"));
    EXPECT_TRUE(label->text().isEmpty());
    block.setExpandedAnimated(true, false);
    QTextDocument rendered;
    rendered.setHtml(label->text());
    EXPECT_EQ(rendered.toPlainText(), block.code());
}

TEST_F(CodeBlockTest, ClipboardObserverMayDestroyTheBlockDuringCopy)
{
    QPointer<CodeBlock> block = new CodeBlock(QStringLiteral("clipboard lifetime"));
    bool completed = false;
    QObject::connect(block, &CodeBlock::copyFinished, qApp, [&](bool) { completed = true; });
    QObject::connect(QApplication::clipboard(), &QClipboard::dataChanged, block, [block] { delete block.data(); });
    block->copy();
    EXPECT_TRUE(block.isNull());
    EXPECT_FALSE(completed);
}

enum class TestLanguage { Cpp, Python };

TEST_F(CodeBlockTest, Contract_ClipboardCompletionFollowsTheHostWrite)
{
    QObject context;
    const QString source = QStringLiteral("// 中文\nvalue = 1;\n");
    int completions = 0;
    ::compatibility::detail::copyText(&context, source, [&](bool copied) {
        ++completions;
        EXPECT_TRUE(copied);
        EXPECT_EQ(QApplication::clipboard()->text(), source);
    });
    EXPECT_EQ(completions, 1);
}

TEST_F(CodeBlockTest, Contract_ClipboardCompletionDoesNotOutliveItsContext)
{
    QApplication::clipboard()->setText(QStringLiteral("before context cancellation"));
    QPointer<QObject> context = new QObject;
    bool completed = false;
    QObject::connect(QApplication::clipboard(), &QClipboard::dataChanged, context, [context] {
        delete context.data();
    });
    ::compatibility::detail::copyText(context, QStringLiteral("context cancellation payload"), [&](bool) {
        completed = true;
    });
    EXPECT_TRUE(context.isNull());
    EXPECT_FALSE(completed);
}

TEST_F(CodeBlockTest, Contract_CodeHighlightingPreservesWhitespaceWhileWrapping)
{
    const QString source = QStringLiteral(
        "    value = Type::Nested::Value;\r\n"
        "\t// keep  two spaces & < >\r\n"
        "next();");
    const QString expected = QStringLiteral(
        "    value = Type::Nested::Value;\n"
        "    // keep  two spaces & < >\n"
        "next();");
    for (bool dark : {false, true}) {
        const QString cpp = fluent::textfields::detail::highlightCppToHtml(source, dark);
        const QString python = fluent::textfields::detail::highlightPythonToHtml(source, dark);
        for (const QString& html : {cpp, python}) {
            QTextDocument document;
            document.setHtml(html);
            QString plain = document.toPlainText();
            plain.remove(QChar(0x200b));
            EXPECT_EQ(plain, expected);
            EXPECT_TRUE(html.contains(QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;")));
            EXPECT_TRUE(html.contains(QStringLiteral("keep&nbsp; two spaces")));
            EXPECT_FALSE(html.contains(QStringLiteral("keep&nbsp;&nbsp;two&nbsp;spaces")));
        }
        EXPECT_TRUE(cpp.contains(QStringLiteral("::&#8203;")));
        const QString dotted = fluent::textfields::detail::highlightPythonToHtml(
            QStringLiteral("fluentqt.Button.ButtonStyle.Accent, value"), dark);
        EXPECT_TRUE(dotted.contains(QStringLiteral(".&#8203;")));
        EXPECT_TRUE(dotted.contains(QStringLiteral(",&#8203;")));
        EXPECT_FALSE(dotted.contains(QStringLiteral("fluentqt.&#8203;")));
    }
}

TEST_F(CodeBlockTest, Contract_CodeHighlightingMapsRenderedCharactersToExactSource)
{
    const QString source = QStringLiteral(
        "\tType::Nested::Value, fluentqt.Button.Style.Accent;  \r\n"
        "    // keep  spaces\r\n\t\"real\u200bvalue\"\r\n");
    for (bool dark : {false, true}) {
        for (const auto language : {TestLanguage::Cpp, TestLanguage::Python}) {
            fluent::textfields::detail::CodeSourceMap map;
            const QString html = language == TestLanguage::Cpp
                ? fluent::textfields::detail::highlightCppToHtml(source, dark, &map)
                : fluent::textfields::detail::highlightPythonToHtml(source, dark, &map);
            QTextDocument document;
            document.setHtml(html);
            const QString rendered = document.toPlainText();
            ASSERT_EQ(map.size(), rendered.size());
            QString reconstructed;
            int sourceEnd = 0;
            int syntheticBreaks = 0;
            int originalBreaks = 0;
            for (int i = 0; i < map.size(); ++i) {
                const auto& span = map.at(i);
                ASSERT_GE(span.start, 0);
                ASSERT_GE(span.end, span.start);
                ASSERT_LE(span.end, source.size());
                if (span.start == span.end) {
                    EXPECT_EQ(rendered.at(i), QChar(0x200b));
                    ++syntheticBreaks;
                    continue;
                }
                if (source.at(span.start) == QChar(0x200b))
                    ++originalBreaks;
                if (span.end > sourceEnd) {
                    EXPECT_EQ(span.start, sourceEnd);
                    reconstructed += source.mid(span.start, span.end - span.start);
                    sourceEnd = span.end;
                }
            }
            EXPECT_GT(syntheticBreaks, 0);
            EXPECT_EQ(originalBreaks, 1);
            EXPECT_EQ(reconstructed, source);
        }
    }
}

#include <gtest/gtest.h>

#include <QAction>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include "utils/private/Inspector_p.h"

namespace {

using fluent::diagnostics::Inspector;
using fluent::diagnostics::InspectorFinding;
using fluent::diagnostics::InspectorOptions;
using fluent::diagnostics::inspectFindings;

bool containsCode(const QVector<InspectorFinding>& findings, const QString& code)
{
    return std::any_of(findings.cbegin(), findings.cend(),
                       [&](const InspectorFinding& item) { return item.code == code; });
}

int countCode(const QVector<InspectorFinding>& findings, const QString& code)
{
    return static_cast<int>(
        std::count_if(findings.cbegin(), findings.cend(),
                      [&](const InspectorFinding& item) { return item.code == code; }));
}

const InspectorFinding* firstFinding(const QVector<InspectorFinding>& findings, const QString& code)
{
    const auto it = std::find_if(findings.cbegin(), findings.cend(),
                                 [&](const InspectorFinding& item) { return item.code == code; });
    return it == findings.cend() ? nullptr : &*it;
}

InspectorOptions focusedOptions()
{
    InspectorOptions options;
    options.checkClippedText = false;
    options.checkAccessibilityNames = false;
    options.checkHitAreas = false;
    options.checkFocusOrder = false;
    options.checkDuplicateActions = false;
    options.checkNestedScrolling = false;
    return options;
}

TEST(InspectorTest, Contract_NullRootProducesVersionedEmptyReport)
{
    const QJsonObject report = Inspector::report(nullptr);

    EXPECT_EQ(report.value(QStringLiteral("schema_version")).toInt(), 1);
    EXPECT_EQ(report.value(QStringLiteral("tool")).toString(),
              QStringLiteral("FluentQt Inspector"));
    EXPECT_EQ(report.value(QStringLiteral("findings")).toArray().size(), 0);
    EXPECT_EQ(report.value(QStringLiteral("summary"))
                  .toObject()
                  .value(QStringLiteral("findings"))
                  .toInt(),
              0);
}

TEST(InspectorTest, Contract_InteractiveWidgetReportsMissingNameAndSmallHitArea)
{
    QWidget root;
    root.setObjectName(QStringLiteral("root"));
    root.resize(320, 200);
    auto* button = new QToolButton(&root);
    button->setObjectName(QStringLiteral("iconOnly"));
    button->setGeometry(12, 12, 18, 18);
    root.show();

    const QVector<InspectorFinding> findings = inspectFindings(&root);

    EXPECT_TRUE(containsCode(findings, QStringLiteral("accessibility.missing-name")));
    EXPECT_TRUE(containsCode(findings, QStringLiteral("input.small-hit-area")));
    const InspectorFinding* hitArea =
        firstFinding(findings, QStringLiteral("input.small-hit-area"));
    ASSERT_NE(hitArea, nullptr);
    EXPECT_EQ(hitArea->path, QStringLiteral("root/iconOnly"));
    EXPECT_EQ(hitArea->rect, QRect(12, 12, 18, 18));
}

TEST(InspectorTest, Contract_StructuralScrollerAndSelectableTextAreNotControls)
{
    QWidget root;
    root.resize(320, 200);
    auto* scroller = new QScrollArea(&root);
    scroller->setGeometry(8, 8, 280, 160);
    auto* content = new QWidget;
    content->resize(260, 140);
    scroller->setWidget(content);
    auto* selectable = new QLabel(QStringLiteral("Selectable reference"), content);
    selectable->setTextInteractionFlags(Qt::TextSelectableByKeyboard);
    selectable->setGeometry(8, 8, 140, 17);
    root.show();

    const QVector<InspectorFinding> findings = inspectFindings(&root);

    EXPECT_FALSE(containsCode(findings, QStringLiteral("accessibility.missing-name")));
    EXPECT_FALSE(containsCode(findings, QStringLiteral("input.small-hit-area")));
}

TEST(InspectorTest, Contract_FocusProxyUsesAuthoredContainerName)
{
    QWidget root;
    root.resize(320, 160);
    auto* editorHost = new QWidget(&root);
    editorHost->setAccessibleName(QStringLiteral("Message"));
    editorHost->setGeometry(8, 8, 240, 40);
    auto* editor = new QLineEdit(editorHost);
    editor->setGeometry(0, 0, 240, 40);
    editorHost->setFocusProxy(editor);
    root.show();

    const QVector<InspectorFinding> findings = inspectFindings(&root);

    EXPECT_FALSE(containsCode(findings, QStringLiteral("accessibility.missing-name")));
}

TEST(InspectorTest, Contract_ClippedOffscreenWidgetIsNotInspected)
{
    QWidget root;
    root.resize(100, 100);
    auto* button = new QToolButton(&root);
    button->setGeometry(180, 180, 18, 18);
    root.show();

    EXPECT_TRUE(inspectFindings(&root).isEmpty());
}

TEST(InspectorTest, Contract_FullAccessibleValueSuppressesClippedTextFinding)
{
    QWidget root;
    root.resize(320, 160);
    auto* label = new QLabel(QStringLiteral("A deliberately long account name"), &root);
    label->setGeometry(8, 8, 48, 24);
    label->setAccessibleName(label->text());
    root.show();

    InspectorOptions options = focusedOptions();
    options.checkClippedText = true;
    const QVector<InspectorFinding> findings = inspectFindings(&root, options);

    EXPECT_FALSE(containsCode(findings, QStringLiteral("text.clipped-without-full-value")));
}

TEST(InspectorTest, Contract_ClippedTextWithoutFullValueIsReported)
{
    QWidget root;
    root.resize(320, 160);
    auto* label = new QLabel(QStringLiteral("A deliberately long account name"), &root);
    label->setGeometry(8, 8, 48, 24);
    label->setAccessibleName(QStringLiteral(" "));
    root.show();

    InspectorOptions options = focusedOptions();
    options.checkClippedText = true;
    const QVector<InspectorFinding> findings = inspectFindings(&root, options);

    EXPECT_TRUE(containsCode(findings, QStringLiteral("text.clipped-without-full-value")));
}

TEST(InspectorTest, Contract_DuplicateSemanticActionProducesOneStableFinding)
{
    QWidget root;
    root.setObjectName(QStringLiteral("root"));
    root.resize(320, 160);
    auto* first = new QToolButton(&root);
    first->setObjectName(QStringLiteral("firstSave"));
    first->setProperty("fluentSemanticAction", QStringLiteral("document.save"));
    first->setGeometry(8, 8, 32, 32);
    auto* second = new QToolButton(&root);
    second->setObjectName(QStringLiteral("secondSave"));
    second->setProperty("fluentSemanticAction", QStringLiteral("document.save"));
    second->setGeometry(48, 8, 32, 32);
    root.show();

    InspectorOptions options = focusedOptions();
    options.checkDuplicateActions = true;
    const QVector<InspectorFinding> findings = inspectFindings(&root, options);
    const InspectorFinding* duplicate =
        firstFinding(findings, QStringLiteral("action.duplicate-entry"));

    ASSERT_NE(duplicate, nullptr);
    EXPECT_EQ(duplicate->details.value(QStringLiteral("action")).toString(),
              QStringLiteral("document.save"));
    EXPECT_EQ(duplicate->details.value(QStringLiteral("entry_count")).toInt(), 2);
    EXPECT_EQ(duplicate->details.value(QStringLiteral("entries")).toArray().size(), 2);
}

TEST(InspectorTest, Contract_SharedQActionProducesDuplicateEntryFinding)
{
    QWidget root;
    root.resize(320, 160);
    QAction save(QStringLiteral("Save"), &root);
    save.setObjectName(QStringLiteral("saveAction"));
    auto* first = new QToolButton(&root);
    first->setDefaultAction(&save);
    first->setGeometry(8, 8, 32, 32);
    auto* second = new QToolButton(&root);
    second->setDefaultAction(&save);
    second->setGeometry(48, 8, 32, 32);
    root.show();

    InspectorOptions options = focusedOptions();
    options.checkDuplicateActions = true;
    const QVector<InspectorFinding> findings = inspectFindings(&root, options);

    EXPECT_TRUE(containsCode(findings, QStringLiteral("action.duplicate-entry")));
}

TEST(InspectorTest, Contract_ExplicitSemanticActionPreventsDuplicateNativeFinding)
{
    QWidget root;
    root.resize(320, 160);
    QAction save(QStringLiteral("Save"), &root);
    auto* first = new QToolButton(&root);
    first->setDefaultAction(&save);
    first->setProperty("fluentSemanticAction", QStringLiteral("document.save"));
    first->setGeometry(8, 8, 32, 32);
    auto* second = new QToolButton(&root);
    second->setDefaultAction(&save);
    second->setProperty("fluentSemanticAction", QStringLiteral("document.save"));
    second->setGeometry(48, 8, 32, 32);
    root.show();

    InspectorOptions options = focusedOptions();
    options.checkDuplicateActions = true;
    const QVector<InspectorFinding> findings = inspectFindings(&root, options);

    EXPECT_EQ(countCode(findings, QStringLiteral("action.duplicate-entry")), 1);
}

TEST(InspectorTest, Contract_NestedScrollableAxisProducesBoundaryFinding)
{
    QWidget root;
    root.setObjectName(QStringLiteral("root"));
    root.resize(400, 320);
    auto* outer = new QScrollArea(&root);
    outer->setObjectName(QStringLiteral("outer"));
    outer->setGeometry(0, 0, 360, 280);
    auto* host = new QWidget;
    host->resize(500, 600);
    outer->setWidget(host);
    auto* inner = new QScrollArea(host);
    inner->setObjectName(QStringLiteral("inner"));
    inner->setGeometry(20, 20, 240, 180);
    auto* content = new QWidget;
    content->resize(300, 500);
    inner->setWidget(content);
    root.show();
    outer->verticalScrollBar()->setRange(0, 100);
    inner->verticalScrollBar()->setRange(0, 100);

    InspectorOptions options = focusedOptions();
    options.checkNestedScrolling = true;
    const QVector<InspectorFinding> findings = inspectFindings(&root, options);
    const InspectorFinding* nested =
        firstFinding(findings, QStringLiteral("scroll.nested-boundary"));

    ASSERT_NE(nested, nullptr);
    EXPECT_EQ(nested->path, QStringLiteral("root/outer/QWidget[0]/inner"));
    EXPECT_EQ(nested->details.value(QStringLiteral("ancestor")).toString(),
              QStringLiteral("root/outer"));
}

TEST(InspectorTest, Contract_ExplicitScrollChainingSuppressesBoundaryPrompt)
{
    QWidget root;
    root.resize(400, 320);
    auto* outer = new QScrollArea(&root);
    outer->setGeometry(0, 0, 360, 280);
    auto* host = new QWidget;
    host->resize(500, 600);
    outer->setWidget(host);
    auto* inner = new QScrollArea(host);
    inner->setProperty("scrollChainingEnabled", true);
    inner->setGeometry(20, 20, 240, 180);
    auto* content = new QWidget;
    content->resize(300, 500);
    inner->setWidget(content);
    root.show();
    outer->verticalScrollBar()->setRange(0, 100);
    inner->verticalScrollBar()->setRange(0, 100);

    InspectorOptions options = focusedOptions();
    options.checkNestedScrolling = true;

    EXPECT_FALSE(
        containsCode(inspectFindings(&root, options), QStringLiteral("scroll.nested-boundary")));
}

TEST(InspectorTest, Contract_LayoutGridCheckIsExplicitlyOptIn)
{
    QWidget root;
    root.setObjectName(QStringLiteral("root"));
    auto* layout = new QVBoxLayout(&root);
    layout->setContentsMargins(5, 8, 8, 8);
    layout->setSpacing(8);
    root.show();

    InspectorOptions options = focusedOptions();
    EXPECT_FALSE(
        containsCode(inspectFindings(&root, options), QStringLiteral("layout.off-grid")));

    options.checkLayoutGrid = true;
    EXPECT_TRUE(
        containsCode(inspectFindings(&root, options), QStringLiteral("layout.off-grid")));
}

} // namespace

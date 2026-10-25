#include <gtest/gtest.h>

#include <QApplication>
#include <QAccessible>
#include <QStandardItemModel>
#include <QImage>
#include <QListView>
#include <QPointer>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "QtTestEnvironment.h"
#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/FieldPopupGeometry_p.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/scrolling/ScrollBar.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "design/Spacing.h"
#include "design/Typography.h"

using namespace fluent;
using namespace fluent::basicinput;
using namespace fluent::textfields;
using fluent::collections::ListView;

class AutoSuggestBoxTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override
    {
        const auto& colors = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(colors.bgCanvas.name()));
    }
};

class AutoSuggestBoxTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        fluentRegisterMetaTypeNames<fluent::textfields::AutoSuggestBox::TextChangeReason>(
            "fluent::textfields::AutoSuggestBox::TextChangeReason", "AutoSuggestBox::TextChangeReason");
        fluentRegisterMetaTypeNames<fluent::textfields::AutoSuggestBox::QueryButtonPlacement>(
            "fluent::textfields::AutoSuggestBox::QueryButtonPlacement", "AutoSuggestBox::QueryButtonPlacement");
    }

    void SetUp() override
    {
        window = new AutoSuggestBoxTestWindow();
        window->setFixedSize(520, 360);
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override { delete window; }

    bool showAndFocus(AutoSuggestBox* box)
    {
        QWidget* host = box->window();
        host->show();
        if (!tests::support::isHeadlessPlatform()) {
            if (!QTest::qWaitForWindowExposed(host))
                return false;
            if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland"), Qt::CaseInsensitive)) {
                host->activateWindow();
                if (!QTest::qWaitForWindowActive(host))
                    return false;
            }
        }
        box->setFocus(Qt::OtherFocusReason);
        // Wayland can deliver focus after exposing a window without supporting activation requests.
        // zh_CN: Wayland 不支持主动激活请求，窗口显示后焦点仍可能稍后才到达。
        return QTest::qWaitFor([box] { return box->hasFocus(); });
    }

    AutoSuggestBoxTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

namespace {

QWheelEvent
makeWheelEvent(QWidget* target, QPoint pixelDelta, QPoint angleDelta, Qt::ScrollPhase phase = Qt::NoScrollPhase)
{
    const QPointF pos = target->rect().center();
    const QPointF globalPos = target->mapToGlobal(pos.toPoint());
    return QWheelEvent(pos, globalPos, pixelDelta, angleDelta, Qt::NoButton, Qt::NoModifier, phase, false);
}

} // namespace

TEST_F(AutoSuggestBoxTest, DefaultsAndButtons)
{
    AutoSuggestBox box(window);

    EXPECT_TRUE(box.suggestions().isEmpty());
    EXPECT_TRUE(box.header().isEmpty());
    EXPECT_TRUE(box.isQueryIconVisible());
    EXPECT_EQ(box.queryButtonPlacement(), AutoSuggestBox::QueryButtonPlacement::Right);
    EXPECT_EQ(box.queryIconGlyph(), Typography::Icons::Search);
    EXPECT_EQ(box.inputHeight(), 32);
    EXPECT_EQ(box.queryButtonSize(), 24);
    EXPECT_EQ(box.clearButtonSize(), 24);
    EXPECT_EQ(box.suggestionFontRole(), Typography::FontRole::Body);
    EXPECT_EQ(box.suggestionItemHeight(), 40);
    EXPECT_FALSE(box.isSuggestionListOpen());
    EXPECT_EQ(box.sizeHint().height(), 32);

    auto* queryButton = box.findChild<Button*>("AutoSuggestBoxQueryButton");
    auto* clearButton = box.findChild<Button*>("AutoSuggestBoxClearButton");
    auto* popup = box.findChild<fluent::dialogs_flyouts::Flyout*>("AutoSuggestBoxSuggestionPopup");
    ASSERT_NE(queryButton, nullptr);
    ASSERT_NE(clearButton, nullptr);
    EXPECT_EQ(popup, nullptr);
    EXPECT_FALSE(queryButton->isHidden());
    EXPECT_TRUE(clearButton->isHidden());
    box.setSuggestions({"Alpha"});
    ASSERT_TRUE(showAndFocus(&box));
    QTest::keyClick(&box, Qt::Key_Down);
    popup = window->findChild<fluent::dialogs_flyouts::Flyout*>("AutoSuggestBoxSuggestionPopup");
    ASSERT_NE(popup, nullptr);
    EXPECT_EQ(popup->anchorOffset(), Spacing::Gap::Normal);
    box.cancelSuggestions();
    QTest::keyClick(&box, Qt::Key_Down);
    EXPECT_EQ(window->findChildren<fluent::dialogs_flyouts::Flyout*>("AutoSuggestBoxSuggestionPopup").size(), 1);
}

TEST_F(AutoSuggestBoxTest, PropertySettersEmitSignals)
{
    AutoSuggestBox box(window);
    QSignalSpy suggestionsSpy(&box, &AutoSuggestBox::suggestionsChanged);
    QSignalSpy headerSpy(&box, &AutoSuggestBox::headerChanged);
    QSignalSpy iconSpy(&box, &AutoSuggestBox::queryIconGlyphChanged);
    QSignalSpy iconVisibleSpy(&box, &AutoSuggestBox::queryIconVisibleChanged);
    QSignalSpy queryPlacementSpy(&box, &AutoSuggestBox::queryButtonPlacementChanged);
    QSignalSpy inputHeightSpy(&box, &AutoSuggestBox::inputHeightChanged);
    QSignalSpy queryButtonSizeSpy(&box, &AutoSuggestBox::queryButtonSizeChanged);
    QSignalSpy clearButtonSizeSpy(&box, &AutoSuggestBox::clearButtonSizeChanged);
    QSignalSpy suggestionFontSpy(&box, &AutoSuggestBox::suggestionFontRoleChanged);
    QSignalSpy suggestionItemHeightSpy(&box, &AutoSuggestBox::suggestionItemHeightChanged);

    const QStringList items{"Alpha", "Beta"};
    box.setSuggestions(items);
    box.setHeader("Search files");
    box.setQueryIconGlyph(Typography::Icons::Filter);
    box.setQueryIconVisible(false);
    box.setQueryButtonPlacement(AutoSuggestBox::QueryButtonPlacement::Left);
    box.setInputHeight(24);
    box.setQueryButtonSize(18);
    box.setClearButtonSize(16);
    box.setSuggestionFontRole(Typography::FontRole::Caption);
    box.setSuggestionItemHeight(24);

    EXPECT_EQ(box.suggestions(), items);
    EXPECT_EQ(box.header(), "Search files");
    EXPECT_EQ(box.queryIconGlyph(), Typography::Icons::Filter);
    EXPECT_FALSE(box.isQueryIconVisible());
    EXPECT_EQ(box.queryButtonPlacement(), AutoSuggestBox::QueryButtonPlacement::Left);
    EXPECT_EQ(box.inputHeight(), 24);
    EXPECT_EQ(box.queryButtonSize(), 18);
    EXPECT_EQ(box.clearButtonSize(), 16);
    EXPECT_EQ(box.suggestionFontRole(), Typography::FontRole::Caption);
    EXPECT_EQ(box.suggestionItemHeight(), 24);
    EXPECT_EQ(box.sizeHint().height(), 48);
    EXPECT_GT(box.sizeHint().height(), 32);

    EXPECT_EQ(suggestionsSpy.count(), 1);
    EXPECT_EQ(headerSpy.count(), 1);
    EXPECT_EQ(iconSpy.count(), 1);
    EXPECT_EQ(iconVisibleSpy.count(), 1);
    EXPECT_EQ(queryPlacementSpy.count(), 1);
    EXPECT_EQ(inputHeightSpy.count(), 1);
    EXPECT_EQ(queryButtonSizeSpy.count(), 1);
    EXPECT_EQ(clearButtonSizeSpy.count(), 1);
    EXPECT_EQ(suggestionFontSpy.count(), 1);
    EXPECT_EQ(suggestionItemHeightSpy.count(), 1);

    box.setSuggestions(items);
    box.setHeader("Search files");
    box.setQueryIconGlyph(Typography::Icons::Filter);
    box.setQueryIconVisible(false);
    box.setQueryButtonPlacement(AutoSuggestBox::QueryButtonPlacement::Left);
    box.setInputHeight(24);
    box.setQueryButtonSize(18);
    box.setClearButtonSize(16);
    box.setSuggestionFontRole(Typography::FontRole::Caption);
    box.setSuggestionItemHeight(24);
    EXPECT_EQ(suggestionsSpy.count(), 1);
    EXPECT_EQ(headerSpy.count(), 1);
    EXPECT_EQ(iconSpy.count(), 1);
    EXPECT_EQ(iconVisibleSpy.count(), 1);
    EXPECT_EQ(queryPlacementSpy.count(), 1);
    EXPECT_EQ(inputHeightSpy.count(), 1);
    EXPECT_EQ(queryButtonSizeSpy.count(), 1);
    EXPECT_EQ(clearButtonSizeSpy.count(), 1);
    EXPECT_EQ(suggestionFontSpy.count(), 1);
    EXPECT_EQ(suggestionItemHeightSpy.count(), 1);
}

TEST_F(AutoSuggestBoxTest, CompactInputAndButtonSizes)
{
    AutoSuggestBox box(window);
    box.setFontRole(Typography::FontRole::Caption);
    box.setSuggestionFontRole(Typography::FontRole::Caption);
    box.setSuggestionItemHeight(24);
    box.setInputHeight(24);
    box.setQueryButtonSize(18);
    box.setClearButtonSize(16);
    box.resize(220, box.sizeHint().height());

    auto* queryButton = box.findChild<Button*>("AutoSuggestBoxQueryButton");
    auto* clearButton = box.findChild<Button*>("AutoSuggestBoxClearButton");
    ASSERT_NE(queryButton, nullptr);
    ASSERT_NE(clearButton, nullptr);

    EXPECT_EQ(box.sizeHint().height(), 24);
    EXPECT_EQ(box.font().pixelSize(), Typography::FontSize::Caption);
    EXPECT_EQ(queryButton->size(), QSize(18, 18));

    box.setSuggestions({"Alpha", "Beta"});
    ASSERT_TRUE(showAndFocus(&box));
    QTest::keyClick(&box, Qt::Key_Down);
    auto* listView = window->findChild<QListView*>("AutoSuggestBoxSuggestionList");
    ASSERT_NE(listView, nullptr);
    EXPECT_EQ(listView->sizeHintForRow(0), 24);

    box.setText("compact");
    QApplication::processEvents();
    EXPECT_EQ(clearButton->size(), QSize(16, 16));
    EXPECT_EQ(queryButton->geometry().center().y(), box.rect().center().y());
    EXPECT_EQ(clearButton->geometry().center().y(), box.rect().center().y());
    EXPECT_LT(clearButton->geometry().right(), queryButton->geometry().left());
}

TEST_F(AutoSuggestBoxTest, ButtonsCenterWithinInputRectWhenHostHeightDiffers)
{
    AutoSuggestBox box(window);
    box.setInputHeight(28);
    box.setQueryButtonSize(24);
    box.setClearButtonSize(24);
    box.setFixedSize(240, 36);
    box.setText("query");
    QApplication::processEvents();

    auto* queryButton = box.findChild<Button*>("AutoSuggestBoxQueryButton");
    auto* clearButton = box.findChild<Button*>("AutoSuggestBoxClearButton");
    ASSERT_NE(queryButton, nullptr);
    ASSERT_NE(clearButton, nullptr);

    EXPECT_EQ(queryButton->geometry().top(), 2);
    EXPECT_EQ(clearButton->geometry().top(), 2);
    EXPECT_LT(clearButton->geometry().right(), queryButton->geometry().left());
}

TEST_F(AutoSuggestBoxTest, QueryButtonPlacementSupportsLeftAndRight)
{
    AutoSuggestBox box(window);
    box.resize(240, box.sizeHint().height());
    box.setText("Text");
    QApplication::processEvents();

    auto* queryButton = box.findChild<Button*>("AutoSuggestBoxQueryButton");
    auto* clearButton = box.findChild<Button*>("AutoSuggestBoxClearButton");
    ASSERT_NE(queryButton, nullptr);
    ASSERT_NE(clearButton, nullptr);

    EXPECT_EQ(box.queryButtonPlacement(), AutoSuggestBox::QueryButtonPlacement::Right);
    EXPECT_GT(queryButton->geometry().center().x(), box.rect().center().x());
    EXPECT_LT(clearButton->geometry().right(), queryButton->geometry().left());
    EXPECT_EQ(box.contentMargins().left(), Spacing::Padding::TextFieldHorizontal);

    box.setQueryButtonPlacement(AutoSuggestBox::QueryButtonPlacement::Left);
    QApplication::processEvents();

    EXPECT_LT(queryButton->geometry().center().x(), box.rect().center().x());
    EXPECT_GT(clearButton->geometry().center().x(), box.rect().center().x());
    EXPECT_LT(queryButton->geometry().right(), clearButton->geometry().left());
    EXPECT_EQ(box.contentMargins().left(), 4 + box.queryButtonSize() + 2);
    EXPECT_GT(box.contentMargins().right(), Spacing::Padding::TextFieldHorizontal);

    box.setQueryButtonPlacement(AutoSuggestBox::QueryButtonPlacement::Right);
    QApplication::processEvents();

    EXPECT_GT(queryButton->geometry().center().x(), box.rect().center().x());
    EXPECT_LT(clearButton->geometry().right(), queryButton->geometry().left());
    EXPECT_EQ(box.contentMargins().left(), Spacing::Padding::TextFieldHorizontal);
}

TEST_F(AutoSuggestBoxTest, ProgrammaticAndUserTextReasons)
{
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    layout->addWidget(box);

    QSignalSpy textSpy(box, &AutoSuggestBox::textChangedWithReason);
    box->setText("seed");
    ASSERT_EQ(textSpy.count(), 1);
    EXPECT_EQ(textSpy.takeFirst().at(1).value<AutoSuggestBox::TextChangeReason>(),
        AutoSuggestBox::TextChangeReason::ProgrammaticChange);

    ASSERT_TRUE(showAndFocus(box));
    QTest::keyClicks(box, "x");
    ASSERT_GE(textSpy.count(), 1);
    EXPECT_EQ(textSpy.last().at(1).value<AutoSuggestBox::TextChangeReason>(),
        AutoSuggestBox::TextChangeReason::UserInput);
}

TEST_F(AutoSuggestBoxTest, FieldPopupStaysBoundedAndReflowsWithItsHost)
{
    QWidget host;
    host.resize(360, 260);
    AutoSuggestBox box(&host);
    box.setGeometry(60, 120, 200, 32);
    box.setSuggestions({"Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta"});
    host.show();
    box.setFocus();
    QTest::keyClick(&box, Qt::Key_Down);
    QApplication::processEvents();
    auto* popup = host.findChild<fluent::dialogs_flyouts::Flyout*>("AutoSuggestBoxSuggestionPopup");
    ASSERT_NE(popup, nullptr);
    auto* list = popup->findChild<ListView*>("AutoSuggestBoxSuggestionList");
    ASSERT_NE(list, nullptr);
    QEvent wheel(QEvent::Wheel);
    EXPECT_FALSE(fluent::overlay::fieldPopupAnchorGeometryMayChange(list->viewport(), &wheel, &box, popup));
    EXPECT_TRUE(fluent::overlay::fieldPopupAnchorGeometryMayChange(&host, &wheel, &box, popup));
    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        box.setLayoutDirection(direction);
        host.resize(direction == Qt::LeftToRight ? 360 : 300, 240);
        QApplication::processEvents();
        QApplication::processEvents();
        const QRect card = fluent::overlay::visibleCardGeometry(popup->geometry());
        EXPECT_TRUE(host.rect().adjusted(4, 4, -4, -4).contains(card));
        if (direction == Qt::LeftToRight)
            EXPECT_EQ(card.left(), box.geometry().left());
        else
            EXPECT_EQ(card.right(), box.geometry().right());
        const bool above = popup->placement() == fluent::dialogs_flyouts::Flyout::Top;
        EXPECT_EQ(above ? box.y() - card.bottom() - 1 : card.top() - box.geometry().bottom() - 1, Spacing::Gap::Normal);
        EXPECT_EQ(QApplication::focusWidget(), &box);
    }
}

TEST_F(AutoSuggestBoxTest, UserInputOpensAndEscapeClosesSuggestions)
{
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    box->setSuggestions({"Alpha", "Alpine", "Azure"});
    layout->addWidget(box);
    ASSERT_TRUE(showAndFocus(box));

    QSignalSpy openSpy(box, &AutoSuggestBox::suggestionListOpenChanged);
    QSignalSpy focusSpy(qApp, &QApplication::focusChanged);
    QTest::keyClicks(box, "a");
    QApplication::processEvents();

    EXPECT_TRUE(box->isSuggestionListOpen());
    EXPECT_EQ(QApplication::focusWidget(), box);
    EXPECT_EQ(focusSpy.count(), 0)
        << "Opening suggestions must not interrupt "
           "IME composition with a focus transfer";
    ASSERT_GE(openSpy.count(), 1);
    EXPECT_TRUE(openSpy.first().at(0).toBool());

    QTest::keyClick(box, Qt::Key_Escape);
    QApplication::processEvents();
    EXPECT_FALSE(box->isSuggestionListOpen());
    EXPECT_FALSE(openSpy.last().at(0).toBool());
}

TEST_F(AutoSuggestBoxTest, SuggestionPopupIsRecreatedAfterOwnerMovesTopLevel)
{
    auto* firstWindow = window;
    auto* secondWindow = new AutoSuggestBoxTestWindow;
    secondWindow->setFixedSize(520, 360);
    secondWindow->onThemeUpdated();

    auto* host = new QWidget(firstWindow);
    auto* box = new AutoSuggestBox(host);
    box->setSuggestions({QStringLiteral("Alpha"), QStringLiteral("Beta")});
    box->resize(240, box->height());
    host->show();
    ASSERT_TRUE(showAndFocus(box));
    QTest::keyClicks(box, QStringLiteral("a"));
    QApplication::processEvents();
    ASSERT_TRUE(box->isSuggestionListOpen());
    QPointer<fluent::dialogs_flyouts::Flyout> firstPopup(
        firstWindow->findChild<fluent::dialogs_flyouts::Flyout*>(QStringLiteral("AutoSuggestBoxSuggestionPopup")));
    ASSERT_FALSE(firstPopup.isNull());

    host->setParent(secondWindow);
    host->show();
    secondWindow->show();
    delete firstWindow;
    window = secondWindow;
    layout = nullptr;
    QApplication::processEvents();
    ASSERT_TRUE(firstPopup.isNull());

    box->clear();
    ASSERT_TRUE(showAndFocus(box));
    QTest::keyClicks(box, QStringLiteral("b"));
    QApplication::processEvents();

    EXPECT_TRUE(box->isSuggestionListOpen());
    auto* recreatedPopup = secondWindow->findChild<fluent::dialogs_flyouts::Flyout*>(
        QStringLiteral("AutoSuggestBoxSuggestionPopup"));
    ASSERT_NE(recreatedPopup, nullptr);
    EXPECT_EQ(recreatedPopup->parentWidget(), secondWindow);
}

TEST_F(AutoSuggestBoxTest, KeyboardPreviewAndSubmitSuggestion)
{
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    box->setSuggestions({"Alpha", "Alpine", "Azure"});
    layout->addWidget(box);
    ASSERT_TRUE(showAndFocus(box));

    QSignalSpy textSpy(box, &AutoSuggestBox::textChangedWithReason);
    QSignalSpy chosenSpy(box, &AutoSuggestBox::suggestionChosen);
    QSignalSpy querySpy(box, &AutoSuggestBox::querySubmitted);

    QTest::keyClicks(box, "a");
    QApplication::processEvents();
    EXPECT_TRUE(box->isSuggestionListOpen());

    QTest::keyClick(box, Qt::Key_Down);
    QApplication::processEvents();
    ASSERT_EQ(chosenSpy.count(), 1);
    EXPECT_EQ(chosenSpy.last().at(0).toString(), "Alpha");
    EXPECT_EQ(box->text(), "Alpha");
    ASSERT_GE(textSpy.count(), 1);
    EXPECT_EQ(textSpy.last().at(1).value<AutoSuggestBox::TextChangeReason>(),
        AutoSuggestBox::TextChangeReason::ProgrammaticChange);

    QTest::keyClick(box, Qt::Key_Up);
    QApplication::processEvents();
    EXPECT_EQ(box->text(), "a");
    EXPECT_TRUE(box->isSuggestionListOpen());

    QTest::keyClick(box, Qt::Key_Down);
    QTest::keyClick(box, Qt::Key_Return);
    QApplication::processEvents();

    ASSERT_EQ(querySpy.count(), 1);
    EXPECT_EQ(querySpy.last().at(0).toString(), "Alpha");
    EXPECT_EQ(querySpy.last().at(1).toString(), "Alpha");
    EXPECT_EQ(textSpy.last().at(1).value<AutoSuggestBox::TextChangeReason>(),
        AutoSuggestBox::TextChangeReason::SuggestionChosen);
    EXPECT_FALSE(box->isSuggestionListOpen());
}

TEST_F(AutoSuggestBoxTest, QueryAndClearButtons)
{
    AutoSuggestBox box(window);
    box.resize(240, box.sizeHint().height());
    QSignalSpy querySpy(&box, &AutoSuggestBox::querySubmitted);
    QSignalSpy textSpy(&box, &AutoSuggestBox::textChangedWithReason);

    auto* queryButton = box.findChild<Button*>("AutoSuggestBoxQueryButton");
    auto* clearButton = box.findChild<Button*>("AutoSuggestBoxClearButton");
    ASSERT_NE(queryButton, nullptr);
    ASSERT_NE(clearButton, nullptr);

    box.setText("hello");
    EXPECT_FALSE(clearButton->isHidden());
    EXPECT_LT(clearButton->geometry().right(), queryButton->geometry().left());
    EXPECT_EQ(queryButton->size(), QSize(24, 24));
    EXPECT_EQ(clearButton->size(), QSize(24, 24));

    QTest::keyClick(&box, Qt::Key_Return);
    ASSERT_EQ(querySpy.count(), 1);
    EXPECT_EQ(querySpy.last().at(0).toString(), "hello");
    EXPECT_FALSE(querySpy.last().at(1).isValid());

    queryButton->click();
    ASSERT_EQ(querySpy.count(), 2);
    EXPECT_EQ(querySpy.last().at(0).toString(), "hello");
    EXPECT_FALSE(querySpy.last().at(1).isValid());

    clearButton->click();
    EXPECT_TRUE(box.text().isEmpty());
    EXPECT_TRUE(clearButton->isHidden());
    ASSERT_GE(textSpy.count(), 2);
    EXPECT_EQ(textSpy.last().at(1).value<AutoSuggestBox::TextChangeReason>(),
        AutoSuggestBox::TextChangeReason::UserInput);
}

TEST_F(AutoSuggestBoxTest, ClearButtonReceivesMouseInputWhileSuggestionsOpen)
{
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setQueryIconVisible(false);
    box->setInputHeight(24);
    box->setClearButtonSize(16);
    box->setSuggestionFontRole(Typography::FontRole::Caption);
    box->setSuggestionItemHeight(24);
    box->setFixedSize(220, 24);
    box->setSuggestions({"TitleBar", "WindowChromeCompat", "AutoSuggestBox"});
    layout->addWidget(box);
    ASSERT_TRUE(showAndFocus(box));

    QSignalSpy textSpy(box, &AutoSuggestBox::textChangedWithReason);

    QTest::keyClicks(box, "asdasd");
    QApplication::processEvents();
    ASSERT_TRUE(box->isSuggestionListOpen());

    auto* clearButton = box->findChild<Button*>("AutoSuggestBoxClearButton");
    ASSERT_NE(clearButton, nullptr);
    ASSERT_TRUE(clearButton->isVisible());

    const QPoint clearCenterGlobal = clearButton->mapToGlobal(clearButton->rect().center());
    QWidget* hitWidget = QApplication::widgetAt(clearCenterGlobal);
    ASSERT_NE(hitWidget, nullptr);
    EXPECT_TRUE(hitWidget == clearButton || clearButton->isAncestorOf(hitWidget))
        << "Popup must not be the mouse target over AutoSuggestBox clear button";

    EXPECT_TRUE(box->isSuggestionListOpen());

    QTest::mouseClick(hitWidget, Qt::LeftButton, Qt::NoModifier, hitWidget->mapFromGlobal(clearCenterGlobal));
    QApplication::processEvents();

    EXPECT_TRUE(box->text().isEmpty());
    EXPECT_TRUE(clearButton->isHidden());
    EXPECT_FALSE(box->isSuggestionListOpen());
    ASSERT_GE(textSpy.count(), 1);
    EXPECT_EQ(textSpy.last().at(1).value<AutoSuggestBox::TextChangeReason>(),
        AutoSuggestBox::TextChangeReason::UserInput);
}

TEST_F(AutoSuggestBoxTest, MouseClickSuggestionChoosesAndSubmits)
{
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    box->setSuggestions({"Alpha", "Alpine", "Azure"});
    layout->addWidget(box);
    ASSERT_TRUE(showAndFocus(box));

    QSignalSpy chosenSpy(box, &AutoSuggestBox::suggestionChosen);
    QSignalSpy querySpy(box, &AutoSuggestBox::querySubmitted);

    QTest::keyClicks(box, "a");
    QApplication::processEvents();
    ASSERT_TRUE(box->isSuggestionListOpen());

    auto* popup = window->findChild<fluent::dialogs_flyouts::Flyout*>("AutoSuggestBoxSuggestionPopup");
    ASSERT_NE(popup, nullptr);
    auto* listView = popup->findChild<QListView*>("AutoSuggestBoxSuggestionList");
    ASSERT_NE(listView, nullptr);

    const QModelIndex first = listView->model()->index(0, 0);
    ASSERT_TRUE(first.isValid());
    const QPoint clickPos = listView->visualRect(first).center();
    QTest::mouseClick(listView->viewport(), Qt::LeftButton, Qt::NoModifier, clickPos);
    QApplication::processEvents();

    EXPECT_EQ(box->text(), "Alpha");
    ASSERT_EQ(chosenSpy.count(), 1);
    EXPECT_EQ(chosenSpy.last().at(0).toString(), "Alpha");
    ASSERT_EQ(querySpy.count(), 1);
    EXPECT_EQ(querySpy.last().at(0).toString(), "Alpha");
    EXPECT_EQ(querySpy.last().at(1).toString(), "Alpha");
    EXPECT_FALSE(box->isSuggestionListOpen());
}

TEST_F(AutoSuggestBoxTest, SuggestionsUseFluentScrollChromeAndContainBoundaryWheel)
{
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(260);
    box->setSuggestions(
        {"Apple", "Apricot", "Banana", "Blueberry", "Cherry", "Grape", "Orange", "Strawberry", "Watermelon"});
    layout->addWidget(box);
    ASSERT_TRUE(showAndFocus(box));

    QTest::keyClicks(box, "a");
    QApplication::processEvents();
    ASSERT_TRUE(box->isSuggestionListOpen());

    auto* popup = window->findChild<fluent::dialogs_flyouts::Flyout*>("AutoSuggestBoxSuggestionPopup");
    ASSERT_NE(popup, nullptr);
    auto* listView = popup->findChild<ListView*>("AutoSuggestBoxSuggestionList");
    ASSERT_NE(listView, nullptr);
    EXPECT_TRUE(listView->property("fluentPreserveParentSurface").toBool());
    ASSERT_NE(listView->viewport(), nullptr);
    EXPECT_TRUE(listView->viewport()->property("fluentPreserveParentSurface").toBool());
    EXPECT_FALSE(listView->isScrollChainingEnabled());

    auto* fluentBar = listView->verticalFluentScrollBar();
    ASSERT_NE(fluentBar, nullptr);
    listView->doItemsLayout();
    listView->refreshFluentScrollChrome();
    QApplication::processEvents();

    ASSERT_GT(listView->verticalScrollBar()->maximum(), listView->verticalScrollBar()->minimum());
    EXPECT_TRUE(fluentBar->isVisible());

    listView->verticalScrollBar()->setValue(listView->verticalScrollBar()->maximum());
    QWheelEvent wheel = makeWheelEvent(listView->viewport(), QPoint(0, 0), QPoint(0, -120));
    wheel.setAccepted(false);
    QApplication::sendEvent(listView->viewport(), &wheel);
    QApplication::processEvents();

    EXPECT_TRUE(wheel.isAccepted());
    EXPECT_EQ(listView->verticalScrollBar()->value(), listView->verticalScrollBar()->maximum());
}

TEST_F(AutoSuggestBoxTest, ReplacingScrolledSuggestionsMakesSingleResultVisibleImmediately)
{
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(260);
    box->setSuggestions(
        {"Apple", "Apricot", "Banana", "Blueberry", "Cherry", "Grape", "Orange", "Strawberry", "Watermelon"});
    layout->addWidget(box);
    ASSERT_TRUE(showAndFocus(box));

    QTest::keyClicks(box, "a");
    QApplication::processEvents();
    ASSERT_TRUE(box->isSuggestionListOpen());

    auto* popup = window->findChild<fluent::dialogs_flyouts::Flyout*>("AutoSuggestBoxSuggestionPopup");
    ASSERT_NE(popup, nullptr);
    auto* listView = popup->findChild<ListView*>("AutoSuggestBoxSuggestionList");
    ASSERT_NE(listView, nullptr);
    listView->doItemsLayout();
    QApplication::processEvents();

    ASSERT_GT(listView->verticalScrollBar()->maximum(), listView->verticalScrollBar()->minimum());
    listView->verticalScrollBar()->setValue(listView->verticalScrollBar()->maximum());

    box->setSuggestions({"TreeView"});

    ASSERT_EQ(listView->model()->rowCount(), 1);
    EXPECT_EQ(listView->verticalScrollBar()->value(), listView->verticalScrollBar()->minimum());
    const QRect itemRect = static_cast<QListView*>(listView)->visualRect(listView->model()->index(0, 0));
    EXPECT_TRUE(itemRect.isValid());
    EXPECT_TRUE(listView->viewport()->rect().intersects(itemRect));
}

TEST_F(AutoSuggestBoxTest, OutsidePressLightDismissesSuggestions)
{
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    box->setSuggestions({"Alpha", "Alpine", "Azure"});
    layout->addWidget(box);
    ASSERT_TRUE(showAndFocus(box));

    QTest::keyClicks(box, "a");
    QApplication::processEvents();
    ASSERT_TRUE(box->isSuggestionListOpen());

    const QPoint outsideGlobal = window->mapToGlobal(QPoint(window->width() - 24, window->height() - 24));
    QWidget* hitWidget = QApplication::widgetAt(outsideGlobal);
    ASSERT_NE(hitWidget, nullptr);
    QTest::mouseClick(hitWidget, Qt::LeftButton, Qt::NoModifier, hitWidget->mapFromGlobal(outsideGlobal));
    QApplication::processEvents();

    EXPECT_FALSE(box->isSuggestionListOpen());
}

TEST_F(AutoSuggestBoxTest, Contract_AccessibilityAsyncDebounceAndStaleResults)
{
    auto* box = new AutoSuggestBox(window);
    box->setGeometry(20, 20, 360, 32);
    box->setAsyncSuggestionsEnabled(true);
    box->setDebounceInterval(15);
    QSignalSpy requests(box, &AutoSuggestBox::suggestionsRequested);
    QSignalSpy canceled(box, &AutoSuggestBox::suggestionRequestCanceled);
    ASSERT_TRUE(showAndFocus(box));
    QTest::keyClicks(box, "Aur");
    ASSERT_TRUE(QTest::qWaitFor([&] { return requests.count() == 1; }));
    const quint64 first = requests.at(0).at(1).toULongLong();
    EXPECT_EQ(requests.at(0).at(0).toString(), QStringLiteral("Aur"));
    EXPECT_TRUE(box->isSuggestionRequestCurrent(first));
    EXPECT_EQ(box->loadState(), AutoSuggestBox::LoadState::Loading);
    EXPECT_TRUE(box->isSuggestionListOpen());
#if QT_CONFIG(accessibility)
    auto* accessible = QAccessible::queryAccessibleInterface(box);
    ASSERT_NE(accessible, nullptr);
    EXPECT_TRUE(accessible->state().busy);
    EXPECT_TRUE(accessible->state().expanded);
    EXPECT_FALSE(accessible->relations(QAccessible::Controller).isEmpty());
#endif
    QTest::keyClicks(box, "ora");
    EXPECT_FALSE(box->setSuggestionsForRequest(first, {QStringLiteral("Old result")}));
    EXPECT_TRUE(box->suggestions().isEmpty());
    ASSERT_TRUE(QTest::qWaitFor([&] { return requests.count() == 2; }));
    const quint64 current = requests.at(1).at(1).toULongLong();
    EXPECT_NE(first, current);
    EXPECT_EQ(canceled.count(), 1);
    EXPECT_TRUE(box->setSuggestionsForRequest(current, {QStringLiteral("Aurora Workspace")}, true));
    EXPECT_EQ(box->loadState(), AutoSuggestBox::LoadState::Ready);
    EXPECT_EQ(box->suggestionCount(), 1);
    EXPECT_TRUE(box->hasMoreSuggestions());
#if QT_CONFIG(accessibility)
    EXPECT_FALSE(accessible->state().busy);
#endif
    QTest::keyClick(box, Qt::Key_Down);
    QTest::qWait(25);
    EXPECT_EQ(requests.count(), 2) << "Keyboard previews must not start a new query";
    EXPECT_EQ(box->text(), QStringLiteral("Aurora Workspace"));
}

TEST_F(AutoSuggestBoxTest, Contract_AsyncPaginationRetainsRowsSelectionAndScroll)
{
    auto* box = new AutoSuggestBox(window);
    box->setGeometry(20, 20, 360, 32);
    box->setAsyncSuggestionsEnabled(true);
    box->setText(QStringLiteral("Aurora"));
    QSignalSpy requests(box, &AutoSuggestBox::suggestionsRequested);
    ASSERT_TRUE(showAndFocus(box));
    box->requestSuggestions();
    QStringList rows;
    for (int i = 0; i < 40; ++i)
        rows.append(QStringLiteral("Aurora %1").arg(i));
    ASSERT_TRUE(box->setSuggestionsForRequest(requests.last().at(1).toULongLong(), rows, true));
    auto* list = window->findChild<QListView*>(QStringLiteral("AutoSuggestBoxSuggestionList"));
    ASSERT_NE(list, nullptr);
    list->setCurrentIndex(list->model()->index(25, 0));
    list->scrollTo(list->currentIndex(), QAbstractItemView::PositionAtCenter);
    QApplication::processEvents();
    const int scroll = list->verticalScrollBar()->value();
    ASSERT_GT(scroll, 0);
    box->loadMoreSuggestions();
    EXPECT_TRUE(requests.last().at(2).toBool());
    EXPECT_EQ(box->suggestionCount(), 40);
    const quint64 failed = requests.last().at(1).toULongLong();
    ASSERT_TRUE(box->failSuggestionRequest(failed, QStringLiteral("Could not load more")));
    EXPECT_EQ(box->suggestionCount(), 40);
    EXPECT_EQ(list->currentIndex().row(), 25);
    EXPECT_EQ(list->verticalScrollBar()->value(), scroll);
    box->retrySuggestions();
    const quint64 retried = requests.last().at(1).toULongLong();
    EXPECT_NE(failed, retried);
    EXPECT_TRUE(requests.last().at(2).toBool());
    EXPECT_FALSE(box->setSuggestionsForRequest(failed, {QStringLiteral("Late page")}, false));
    ASSERT_TRUE(box->setSuggestionsForRequest(retried, {QStringLiteral("Aurora 40")}, false));
    QApplication::processEvents();
    EXPECT_EQ(box->suggestionCount(), 41);
    EXPECT_EQ(list->currentIndex().row(), 25);
    EXPECT_EQ(list->verticalScrollBar()->value(), scroll);
    EXPECT_FALSE(box->hasMoreSuggestions());
}

TEST_F(AutoSuggestBoxTest, Contract_BorrowedModelPayloadDisabledRowsAndDestruction)
{
    auto* box = new AutoSuggestBox(window);
    box->setGeometry(20, 20, 360, 32);
    auto* model = new QStandardItemModel(window);
    auto* unavailable = new QStandardItem(QStringLiteral("Unavailable"));
    unavailable->setEnabled(false);
    model->appendRow(unavailable);
    auto* available = new QStandardItem(QStringLiteral("Aurora"));
    available->setData(42, Qt::UserRole);
    model->appendRow(available);
    box->setSuggestionModel(model);
    EXPECT_EQ(model->parent(), window);
    EXPECT_TRUE(box->suggestions().isEmpty());
    EXPECT_EQ(box->suggestionCount(), 2);
    ASSERT_TRUE(showAndFocus(box));
    QSignalSpy submit(box, &AutoSuggestBox::querySubmitted);
    QTest::keyClick(box, Qt::Key_Down);
    EXPECT_EQ(box->text(), QStringLiteral("Aurora"));
    QTest::keyClick(box, Qt::Key_Return);
    ASSERT_EQ(submit.count(), 1);
    EXPECT_EQ(submit.last().at(1).toInt(), 42);
    QTest::keyClick(box, Qt::Key_Down);
    delete model;
    QApplication::processEvents();
    EXPECT_EQ(box->suggestionModel(), nullptr);
    EXPECT_EQ(box->suggestionCount(), 0);
    EXPECT_FALSE(box->isSuggestionListOpen());
    box->setSuggestions({QStringLiteral("Local")});
    EXPECT_EQ(box->suggestionCount(), 1);
}

TEST_F(AutoSuggestBoxTest, Contract_AsyncEmptyRetryAndKeyboardAction)
{
    auto* box = new AutoSuggestBox(window);
    box->setGeometry(20, 20, 360, 32);
    box->setAsyncSuggestionsEnabled(true);
    box->setText(QStringLiteral("project"));
    box->setEmptyText(QStringLiteral("No projects found"));
    QSignalSpy requests(box, &AutoSuggestBox::suggestionsRequested);
    ASSERT_TRUE(showAndFocus(box));
    box->requestSuggestions();
    ASSERT_TRUE(box->setSuggestionsForRequest(requests.last().at(1).toULongLong(), {}));
    EXPECT_TRUE(box->isSuggestionListOpen());
    auto* status = window->findChild<Label*>(QStringLiteral("AutoSuggestBoxStatusTitle"));
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->text(), QStringLiteral("No projects found"));
    box->requestSuggestions();
    ASSERT_TRUE(box->failSuggestionRequest(requests.last().at(1).toULongLong(), QStringLiteral("Service unavailable"),
        QStringLiteral("Try again.")));
    auto* retry = window->findChild<Button*>(QStringLiteral("AutoSuggestBoxStatusAction"));
    ASSERT_NE(retry, nullptr);
    EXPECT_TRUE(retry->isVisible());
    QTest::keyClick(box, Qt::Key_Tab);
    EXPECT_TRUE(retry->hasFocus());
    QTest::keyClick(retry, Qt::Key_Space);
    ASSERT_EQ(requests.count(), 3);
    EXPECT_FALSE(requests.last().at(2).toBool());
    ASSERT_TRUE(box->setSuggestionsForRequest(requests.last().at(1).toULongLong(), {QStringLiteral("Recovered")}));
    EXPECT_TRUE(box->hasFocus());
    EXPECT_EQ(box->suggestionCount(), 1);
}

TEST_F(AutoSuggestBoxTest, Contract_AsyncCopyIsCallerOwnedAcrossPagingAndRetry)
{
    auto* box = new AutoSuggestBox(window);
    box->setGeometry(20, 20, 360, 32);
    box->setAsyncSuggestionsEnabled(true);
    box->setLoadingText(QStringLiteral("正在搜索"));
    box->setLoadingMoreText(QStringLiteral("正在加载下一页"));
    box->setMoreResultsText(QStringLiteral("还有更多结果"));
    box->setLoadMoreText(QStringLiteral("下一页"));
    box->setRetryText(QStringLiteral("重新尝试"));
    box->setText(QStringLiteral("project"));
    QSignalSpy requests(box, &AutoSuggestBox::suggestionsRequested);
    ASSERT_TRUE(showAndFocus(box));
    box->requestSuggestions();
    auto* status = window->findChild<Label*>(QStringLiteral("AutoSuggestBoxStatusTitle"));
    auto* action = window->findChild<Button*>(QStringLiteral("AutoSuggestBoxStatusAction"));
    ASSERT_TRUE(status && action);
    EXPECT_EQ(status->text(), box->loadingText());
    ASSERT_TRUE(box->setSuggestionsForRequest(requests.last().at(1).toULongLong(), {QStringLiteral("项目")}, true));
    EXPECT_EQ(status->text(), box->moreResultsText());
    EXPECT_EQ(action->text(), box->loadMoreText());
    action->click();
    EXPECT_TRUE(requests.last().at(2).toBool());
    EXPECT_EQ(status->text(), box->loadingMoreText());
    ASSERT_TRUE(box->failSuggestionRequest(requests.last().at(1).toULongLong(), QStringLiteral("暂时无法加载")));
    EXPECT_EQ(status->text(), QStringLiteral("暂时无法加载"));
    EXPECT_EQ(action->text(), box->retryText());
    QSignalSpy changed(box, &AutoSuggestBox::retryTextChanged);
    const int requestCount = requests.count();
    box->setRetryText(QStringLiteral("再试一次"));
    box->setRetryText(QStringLiteral("再试一次"));
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(requests.count(), requestCount);
    EXPECT_EQ(action->text(), QStringLiteral("再试一次"));
    action->click();
    EXPECT_EQ(requests.count(), requestCount + 1);
    EXPECT_TRUE(requests.last().at(2).toBool());
    EXPECT_EQ(status->text(), box->loadingMoreText());
}

TEST_F(AutoSuggestBoxTest, Contract_AsyncStatusFitsContentAndKeepsRetryReachable)
{
    auto* box = new AutoSuggestBox(window);
    box->setGeometry(20, 20, 320, 32);
    box->setAsyncSuggestionsEnabled(true);
    box->setText(QStringLiteral("project"));
    QSignalSpy requests(box, &AutoSuggestBox::suggestionsRequested);
    ASSERT_TRUE(showAndFocus(box));
    box->requestSuggestions();
    auto* popup = window->findChild<QWidget*>(QStringLiteral("AutoSuggestBoxSuggestionPopup"));
    ASSERT_NE(popup, nullptr);
    EXPECT_LE(fluent::overlay::visibleCardSize(popup->size()).height(), 44);
    EXPECT_EQ(fluent::overlay::visibleCardSize(popup->size()).width(), box->width());

    box->setEmptyDescription(QStringLiteral("Try a different name."));
    ASSERT_TRUE(box->setSuggestionsForRequest(requests.last().at(1).toULongLong(), {}));
    QApplication::processEvents();
    EXPECT_LE(fluent::overlay::visibleCardSize(popup->size()).height(), 60);
    box->requestSuggestions();
    ASSERT_TRUE(box->failSuggestionRequest(requests.last().at(1).toULongLong(),
        QStringLiteral("Couldn’t load projects"), QStringLiteral("Try again in a moment.")));
    QApplication::processEvents();
    auto* retry = popup->findChild<Button*>(QStringLiteral("AutoSuggestBoxStatusAction"));
    auto* details = popup->findChild<Label*>(QStringLiteral("AutoSuggestBoxStatusDetails"));
    ASSERT_TRUE(retry && details);
    EXPECT_LE(fluent::overlay::visibleCardSize(popup->size()).height(), 60);
    const int shortHeight = popup->height();
    box->setGeometry(20, 20, 180, 32);
    box->requestSuggestions();
    ASSERT_TRUE(box->failSuggestionRequest(requests.last().at(1).toULongLong(), QStringLiteral("Search failed"),
        QStringLiteral("The service is unavailable. Please try again after checking your connection.")));
    QApplication::processEvents();
    EXPECT_GT(popup->height(), shortHeight);
    EXPECT_GE(details->height(), details->heightForWidth(details->width()));
    const QRect detailsRect(details->mapTo(popup, QPoint()), details->size());
    const QRect retryRect(retry->mapTo(popup, QPoint()), retry->size());
    EXPECT_FALSE(detailsRect.intersects(retryRect));
    EXPECT_TRUE(fluent::overlay::visibleCardRect(popup->rect()).contains(retryRect));
    QTest::keyClick(box, Qt::Key_Tab);
    EXPECT_TRUE(retry->hasFocus());
    QTest::keyClick(retry, Qt::Key_Escape);
    EXPECT_FALSE(box->isSuggestionListOpen());
}

TEST_F(AutoSuggestBoxTest, Contract_AsyncDismissalCancelsPendingAndLateWork)
{
    auto* box = new AutoSuggestBox(window);
    box->setGeometry(20, 20, 360, 32);
    box->setAsyncSuggestionsEnabled(true);
    box->setDebounceInterval(30);
    QSignalSpy requests(box, &AutoSuggestBox::suggestionsRequested);
    ASSERT_TRUE(showAndFocus(box));
    QTest::keyClicks(box, "query");
    QTest::keyClick(box, Qt::Key_Escape);
    QTest::qWait(50);
    EXPECT_EQ(requests.count(), 0);
    EXPECT_FALSE(box->isSuggestionListOpen());
    box->requestSuggestions();
    const quint64 escaped = requests.last().at(1).toULongLong();
    QTest::keyClick(box, Qt::Key_Escape);
    EXPECT_FALSE(box->setSuggestionsForRequest(escaped, {QStringLiteral("Late")}));
    EXPECT_FALSE(box->isSuggestionListOpen());
    box->requestSuggestions();
    const quint64 outside = requests.last().at(1).toULongLong();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(500, 320));
    EXPECT_FALSE(box->isSuggestionRequestCurrent(outside));
    box->requestSuggestions();
    const quint64 disabled = requests.last().at(1).toULongLong();
    box->setEnabled(false);
    EXPECT_FALSE(box->completeSuggestionRequest(disabled));
    box->setEnabled(true);
    box->requestSuggestions();
    const quint64 hidden = requests.last().at(1).toULongLong();
    window->hide();
    EXPECT_FALSE(box->failSuggestionRequest(hidden, QStringLiteral("Late failure")));
}

TEST_F(AutoSuggestBoxTest, Contract_DenseBorrowedSuggestionsStayVirtualized)
{
    class DenseModel final : public QAbstractListModel {
    public:
        mutable int reads = 0;
        int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 10000; }
        QVariant data(const QModelIndex& index, int role) const override
        {
            ++reads;
            return role == Qt::DisplayRole ? QVariant(QStringLiteral("Project %1").arg(index.row())) : QVariant();
        }
    } model;
    auto* box = new AutoSuggestBox(window);
    box->setGeometry(20, 20, 360, 32);
    box->setSuggestionModel(&model);
    ASSERT_TRUE(showAndFocus(box));
    QTest::keyClick(box, Qt::Key_Down);
    QApplication::processEvents();
    auto* list = window->findChild<QListView*>(QStringLiteral("AutoSuggestBoxSuggestionList"));
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->model(), &model);
    EXPECT_FALSE(list->accessibleName().isEmpty());
    EXPECT_EQ(list->visualRect(model.index(1, 0)).top() - list->visualRect(model.index(0, 0)).top(),
        box->suggestionItemHeight());
    EXPECT_LT(list->findChildren<QWidget*>().size(), 20);
    EXPECT_LT(model.reads, 1000);
    EXPECT_EQ(box->suggestionCount(), 10000);
}

TEST_F(AutoSuggestBoxTest, Contract_AsyncCallbacksMayDeleteTheControl)
{
    QPointer<AutoSuggestBox> requesting = new AutoSuggestBox(window);
    requesting->setAsyncSuggestionsEnabled(true);
    requesting->setText(QStringLiteral("query"));
    QObject::connect(requesting, &AutoSuggestBox::suggestionsRequested, requesting, [requesting] {
        delete requesting;
    });
    requesting->requestSuggestions();
    EXPECT_TRUE(requesting.isNull());
    QPointer<AutoSuggestBox> completing = new AutoSuggestBox(window);
    completing->setAsyncSuggestionsEnabled(true);
    completing->setText(QStringLiteral("query"));
    QSignalSpy requests(completing, &AutoSuggestBox::suggestionsRequested);
    completing->requestSuggestions();
    QObject::connect(completing, &AutoSuggestBox::loadStateChanged, completing,
        [completing](AutoSuggestBox::LoadState state) {
            if (state == AutoSuggestBox::LoadState::Ready)
                delete completing;
        });
    EXPECT_TRUE(completing->completeSuggestionRequest(requests.last().at(1).toULongLong()));
    EXPECT_TRUE(completing.isNull());
}

TEST_F(AutoSuggestBoxTest, Contract_AsyncBlankInputAndExplicitClearStopLoading)
{
    auto* box = new AutoSuggestBox(window);
    box->setAsyncSuggestionsEnabled(true);
    box->setDebounceInterval(0);
    QSignalSpy requests(box, &AutoSuggestBox::suggestionsRequested);
    ASSERT_TRUE(showAndFocus(box));
    QTest::keyClicks(box, "   ");
    QApplication::processEvents();
    EXPECT_EQ(requests.count(), 0);
    EXPECT_EQ(box->loadState(), AutoSuggestBox::LoadState::Idle);
    EXPECT_FALSE(box->isSuggestionListOpen());
    QTest::keyClicks(box, "query");
    ASSERT_TRUE(QTest::qWaitFor([&] { return requests.count() == 1; }));
    const quint64 token = requests.last().at(1).toULongLong();
    box->clearSuggestions();
    EXPECT_FALSE(box->isSuggestionRequestCurrent(token));
    EXPECT_EQ(box->suggestionCount(), 0);
    EXPECT_EQ(box->loadState(), AutoSuggestBox::LoadState::Ready);
    EXPECT_FALSE(box->setSuggestionsForRequest(token, {QStringLiteral("Late")}));
}

TEST_F(AutoSuggestBoxTest, Contract_LoadingObserverCancellationDoesNotRestartDebounce)
{
    auto* box = new AutoSuggestBox(window);
    box->setAsyncSuggestionsEnabled(true);
    box->setDebounceInterval(15);
    QSignalSpy requests(box, &AutoSuggestBox::suggestionsRequested);
    bool cancelOnce = true;
    QObject::connect(box, &AutoSuggestBox::loadStateChanged, box, [&](AutoSuggestBox::LoadState state) {
        if (state == AutoSuggestBox::LoadState::Loading && cancelOnce) {
            cancelOnce = false;
            box->cancelSuggestions();
        }
    });
    ASSERT_TRUE(showAndFocus(box));
    QTest::keyClicks(box, "q");
    QTest::qWait(50);
    EXPECT_EQ(requests.count(), 0);
    EXPECT_EQ(box->loadState(), AutoSuggestBox::LoadState::Idle);
    EXPECT_FALSE(box->isSuggestionListOpen());
    QTest::keyClicks(box, "u");
    ASSERT_TRUE(QTest::qWaitFor([&] { return requests.count() == 1; }));
    EXPECT_EQ(requests.last().at(0).toString(), QStringLiteral("qu"));
}

TEST_F(AutoSuggestBoxTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;
    window->setFixedSize(520, 460);

    Label* title = new Label("AutoSuggestBox", window);
    title->setFluentTypography(Typography::FontRole::Subtitle);
    title->anchors()->top = {window, Edge::Top, 28};
    title->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(title);

    AutoSuggestBox* rest = new AutoSuggestBox(window);
    rest->setPlaceholderText("Search");
    rest->setSuggestions({"Calendar", "Calculator", "Camera", "Clock"});
    rest->anchors()->top = {title, Edge::Bottom, 16};
    rest->anchors()->left = {window, Edge::Left, 40};
    rest->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(rest);

    AutoSuggestBox* typingFlyout = new AutoSuggestBox(window);
    typingFlyout->setPlaceholderText("Focused typing with flyout");
    typingFlyout->setSuggestions({"Calendar", "Calculator", "Camera", "Clock"});
    typingFlyout->anchors()->top = {rest, Edge::Bottom, 16};
    typingFlyout->anchors()->left = {window, Edge::Left, 40};
    typingFlyout->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(typingFlyout);

    AutoSuggestBox* withText = new AutoSuggestBox(window);
    withText->setText("settings");
    withText->setQueryButtonPlacement(AutoSuggestBox::QueryButtonPlacement::Left);
    withText->setSuggestions({"Settings", "Sound settings", "Display settings"});
    withText->anchors()->top = {typingFlyout, Edge::Bottom, 20};
    withText->anchors()->left = {window, Edge::Left, 40};
    withText->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(withText);

    AutoSuggestBox* withHeader = new AutoSuggestBox(window);
    withHeader->setHeader("Command");
    withHeader->setPlaceholderText("Type a command");
    withHeader->setSuggestions({"Open", "Save", "Share"});
    withHeader->anchors()->top = {withText, Edge::Bottom, 20};
    withHeader->anchors()->left = {window, Edge::Left, 40};
    withHeader->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(withHeader);

    AutoSuggestBox* disabled = new AutoSuggestBox(window);
    disabled->setText("Disabled state");
    disabled->setEnabled(false);
    disabled->anchors()->top = {withHeader, Edge::Bottom, 20};
    disabled->anchors()->left = {window, Edge::Left, 40};
    disabled->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(disabled);

    Button* themeButton = new Button("Switch Theme", window);
    themeButton->setFluentStyle(Button::Accent);
    themeButton->setFixedSize(120, 32);
    themeButton->anchors()->bottom = {window, Edge::Bottom, -28};
    themeButton->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(themeButton);

    QObject::connect(themeButton, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                ? fluent::FluentElement::Dark
                : fluent::FluentElement::Light);
    });

    window->show();
    QTimer::singleShot(0, [typingFlyout]() {
        typingFlyout->setFocus();
        QTest::keyClicks(typingFlyout, "c");
    });
    qApp->exec();
}

TEST_F(AutoSuggestBoxTest, Contract_SynchronousDismissalSurvivesPassiveModelUpdates)
{
    AutoSuggestBox box;
    QStandardItemModel model;
    model.appendRow(new QStandardItem("Alpha"));
    box.setSuggestionModel(&model);
    box.resize(320, 32);
    box.show();
    box.activateWindow();
    box.setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return box.hasFocus(); }));
    QTest::keyClicks(&box, "Al");
    ASSERT_TRUE(box.isSuggestionListOpen());
    QTest::keyClick(&box, Qt::Key_Escape);
    model.setData(model.index(0, 0), "Alpine");
    QApplication::processEvents();
    EXPECT_FALSE(box.isSuggestionListOpen());
    box.setSuggestions({"Alpha", "Alpine"});
    QApplication::processEvents();
    EXPECT_FALSE(box.isSuggestionListOpen());
    QTest::keyClicks(&box, "p");
    EXPECT_TRUE(box.isSuggestionListOpen());
    box.setSuggestions({});
    EXPECT_FALSE(box.isSuggestionListOpen());
    box.setSuggestions({"Alpine"});
    QApplication::processEvents();
    EXPECT_TRUE(box.isSuggestionListOpen());
}

TEST_F(AutoSuggestBoxTest, Contract_SynchronousPopupClosesAfterFocusLeavesOwner)
{
    QWidget host;
    AutoSuggestBox box(&host);
    LineEdit other(&host);
    box.setGeometry(20, 20, 280, 32);
    other.setGeometry(20, 100, 280, 32);
    host.resize(400, 240);
    box.setSuggestions({"Alpha"});
    host.show();
    host.activateWindow();
    box.setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return box.hasFocus(); }));
    QTest::keyClicks(&box, "Al");
    ASSERT_TRUE(box.isSuggestionListOpen());
    other.setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return other.hasFocus() && !box.isSuggestionListOpen(); }));
}

TEST_F(AutoSuggestBoxTest, Contract_FrameVisibilityControlsCustomSuggestionFrame)
{
    AutoSuggestBox box;
    box.setText("Value");
    box.resize(300, 32);
    box.show();
    QApplication::processEvents();
    ASSERT_TRUE(box.isFrameVisible());
    const auto framed = box.grab().toImage();
    box.setFrameVisible(false);
    const auto plain = box.grab().toImage();
    EXPECT_NE(framed, plain);
    box.setFrameVisible(true);
    EXPECT_EQ(framed, box.grab().toImage());
}

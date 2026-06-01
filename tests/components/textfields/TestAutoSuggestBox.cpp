#include <gtest/gtest.h>

#include <QApplication>
#include <QListView>
#include <QTimer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "design/Spacing.h"
#include "design/Typography.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"

using namespace fluent;
using namespace fluent::basicinput;
using namespace fluent::textfields;

class AutoSuggestBoxTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& colors = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(colors.bgCanvas.name()));
    }
};

class AutoSuggestBoxTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::textfields::AutoSuggestBox::TextChangeReason>(
            "fluent::textfields::AutoSuggestBox::TextChangeReason");
        qRegisterMetaType<fluent::textfields::AutoSuggestBox::QueryButtonPlacement>(
            "fluent::textfields::AutoSuggestBox::QueryButtonPlacement");
    }

    void SetUp() override {
        window = new AutoSuggestBoxTestWindow();
        window->setFixedSize(520, 360);
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    void showAndFocus(AutoSuggestBox* box) {
        window->show();
        box->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents();
    }

    AutoSuggestBoxTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

TEST_F(AutoSuggestBoxTest, DefaultsAndButtons) {
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
    ASSERT_NE(popup, nullptr);
    EXPECT_FALSE(queryButton->isHidden());
    EXPECT_TRUE(clearButton->isHidden());
    EXPECT_EQ(popup->anchorOffset(), Spacing::XSmall);
}

TEST_F(AutoSuggestBoxTest, PropertySettersEmitSignals) {
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

TEST_F(AutoSuggestBoxTest, CompactInputAndButtonSizes) {
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
    auto* listView = box.findChild<QListView*>("AutoSuggestBoxSuggestionList");
    ASSERT_NE(listView, nullptr);
    EXPECT_EQ(listView->sizeHintForRow(0), 24);

    box.setText("compact");
    QApplication::processEvents();
    EXPECT_EQ(clearButton->size(), QSize(16, 16));
    EXPECT_EQ(queryButton->geometry().center().y(), box.rect().center().y());
    EXPECT_EQ(clearButton->geometry().center().y(), box.rect().center().y());
    EXPECT_LT(clearButton->geometry().right(), queryButton->geometry().left());
}

TEST_F(AutoSuggestBoxTest, ButtonsCenterWithinInputRectWhenHostHeightDiffers) {
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

TEST_F(AutoSuggestBoxTest, QueryButtonPlacementSupportsLeftAndRight) {
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

TEST_F(AutoSuggestBoxTest, ProgrammaticAndUserTextReasons) {
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    layout->addWidget(box);

    QSignalSpy textSpy(box, &AutoSuggestBox::textChangedWithReason);
    box->setText("seed");
    ASSERT_EQ(textSpy.count(), 1);
    EXPECT_EQ(textSpy.takeFirst().at(1).value<AutoSuggestBox::TextChangeReason>(),
              AutoSuggestBox::TextChangeReason::ProgrammaticChange);

    showAndFocus(box);
    QTest::keyClicks(box, "x");
    ASSERT_GE(textSpy.count(), 1);
    EXPECT_EQ(textSpy.last().at(1).value<AutoSuggestBox::TextChangeReason>(),
              AutoSuggestBox::TextChangeReason::UserInput);
}

TEST_F(AutoSuggestBoxTest, UserInputOpensAndEscapeClosesSuggestions) {
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    box->setSuggestions({"Alpha", "Alpine", "Azure"});
    layout->addWidget(box);
    showAndFocus(box);

    QSignalSpy openSpy(box, &AutoSuggestBox::suggestionListOpenChanged);
    QTest::keyClicks(box, "a");
    QApplication::processEvents();

    EXPECT_TRUE(box->isSuggestionListOpen());
    ASSERT_GE(openSpy.count(), 1);
    EXPECT_TRUE(openSpy.first().at(0).toBool());

    QTest::keyClick(box, Qt::Key_Escape);
    QApplication::processEvents();
    EXPECT_FALSE(box->isSuggestionListOpen());
    EXPECT_FALSE(openSpy.last().at(0).toBool());
}

TEST_F(AutoSuggestBoxTest, KeyboardPreviewAndSubmitSuggestion) {
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    box->setSuggestions({"Alpha", "Alpine", "Azure"});
    layout->addWidget(box);
    showAndFocus(box);

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

TEST_F(AutoSuggestBoxTest, QueryAndClearButtons) {
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

TEST_F(AutoSuggestBoxTest, ClearButtonReceivesMouseInputWhileSuggestionsOpen) {
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setQueryIconVisible(false);
    box->setInputHeight(24);
    box->setClearButtonSize(16);
    box->setSuggestionFontRole(Typography::FontRole::Caption);
    box->setSuggestionItemHeight(24);
    box->setFixedSize(220, 24);
    box->setSuggestions({"TitleBar", "WindowChromeCompat", "AutoSuggestBox"});
    layout->addWidget(box);
    showAndFocus(box);

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

    QTest::mouseClick(hitWidget, Qt::LeftButton, Qt::NoModifier,
                      hitWidget->mapFromGlobal(clearCenterGlobal));
    QApplication::processEvents();

    EXPECT_TRUE(box->text().isEmpty());
    EXPECT_TRUE(clearButton->isHidden());
    EXPECT_FALSE(box->isSuggestionListOpen());
    ASSERT_GE(textSpy.count(), 1);
    EXPECT_EQ(textSpy.last().at(1).value<AutoSuggestBox::TextChangeReason>(),
              AutoSuggestBox::TextChangeReason::UserInput);
}

TEST_F(AutoSuggestBoxTest, MouseClickSuggestionChoosesAndSubmits) {
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    box->setSuggestions({"Alpha", "Alpine", "Azure"});
    layout->addWidget(box);
    showAndFocus(box);

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

TEST_F(AutoSuggestBoxTest, OutsidePressLightDismissesSuggestions) {
    AutoSuggestBox* box = new AutoSuggestBox(window);
    box->setFixedWidth(220);
    box->setSuggestions({"Alpha", "Alpine", "Azure"});
    layout->addWidget(box);
    showAndFocus(box);

    QTest::keyClicks(box, "a");
    QApplication::processEvents();
    ASSERT_TRUE(box->isSuggestionListOpen());

    const QPoint outsideGlobal = window->mapToGlobal(QPoint(window->width() - 24,
                                                           window->height() - 24));
    QWidget* hitWidget = QApplication::widgetAt(outsideGlobal);
    ASSERT_NE(hitWidget, nullptr);
    QTest::mouseClick(hitWidget, Qt::LeftButton, Qt::NoModifier,
                      hitWidget->mapFromGlobal(outsideGlobal));
    QApplication::processEvents();

    EXPECT_FALSE(box->isSuggestionListOpen());
}

TEST_F(AutoSuggestBoxTest, VisualCheck) {
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
            ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    });

    window->show();
    QTimer::singleShot(0, [typingFlyout]() {
        typingFlyout->setFocus();
        QTest::keyClicks(typingFlyout, "c");
    });
    qApp->exec();
}

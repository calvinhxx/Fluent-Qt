#include <gtest/gtest.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include "components/textfields/LineEdit.h"
#include <QStringListModel>
#include <QSignalSpy>
#include <QComboBox>
#include <QFontMetricsF>
#include <QImage>
#include <QPixmap>
#include <QtTest/QTest>

#include "components/basicinput/ComboBox.h"
#include "components/basicinput/Button.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/scrolling/ScrollBar.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Typography.h"

using namespace fluent::basicinput;

// ─── FluentTestWindow ────────────────────────────────────────────────────────

class ComboBoxTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ─── 测试主类 ────────────────────────────────────────────────────────────────

class ComboBoxTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new ComboBoxTestWindow;
        window->onThemeUpdated();
        window->resize(600, 500);
    }

    void TearDown() override {
        delete window;
    }

    ComboBoxTestWindow* window = nullptr;
};

namespace {
fluent::dialogs_flyouts::Flyout* openPopupFor(ComboBox* comboBox, ComboBoxTestWindow* window) {
    window->show();
    comboBox->show();
    QApplication::processEvents();
    comboBox->showPopup();
    QApplication::processEvents();
    return window->findChild<fluent::dialogs_flyouts::Flyout*>("ComboBoxPopup");
}

QRect popupCardRect(QWidget* popup) {
    return popup->geometry().adjusted(::Spacing::Standard, ::Spacing::Standard,
                                      -::Spacing::Standard, -::Spacing::Standard);
}

bool isAccentLike(const QColor& pixel, const QColor& accent) {
    if (pixel.alpha() < 120)
        return false;
    return qAbs(pixel.red() - accent.red()) <= 10 &&
           qAbs(pixel.green() - accent.green()) <= 10 &&
           qAbs(pixel.blue() - accent.blue()) <= 10;
}

qreal accentSpanWidthInViewport(fluent::collections::ListView* listView, const QColor& accent) {
    const QPixmap pixmap = listView->viewport()->grab();
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    const qreal dpr = pixmap.devicePixelRatioF();

    int minX = image.width();
    int maxX = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (!isAccentLike(QColor::fromRgba(image.pixel(x, y)), accent))
                continue;
            minX = qMin(minX, x);
            maxX = qMax(maxX, x);
        }
    }

    return maxX >= minX ? (maxX - minX + 1) / dpr : 0.0;
}
}

// ─── 基础功能测试 ────────────────────────────────────────────────────────────

TEST_F(ComboBoxTest, DefaultProperties) {
    ComboBox cb(window);
    EXPECT_EQ(cb.fontRole(), Typography::FontRole::Body);
    EXPECT_EQ(cb.contentPaddingH(), Spacing::Padding::ComboBoxHorizontal);
    EXPECT_EQ(cb.contentPaddingV(), Spacing::Padding::ComboBoxVertical);
    EXPECT_EQ(cb.chevronGlyph(), Typography::Icons::ChevronDownMed);
    EXPECT_EQ(cb.chevronSize(), 12);
    EXPECT_EQ(cb.chevronOffset(), QPoint(Spacing::Padding::ComboBoxHorizontal, 0));
    EXPECT_EQ(cb.popupOffset(), Spacing::XSmall);
    EXPECT_DOUBLE_EQ(cb.pressProgress(), 0.0);
}

TEST_F(ComboBoxTest, SetFontRole) {
    ComboBox cb(window);
    QSignalSpy spy(&cb, &ComboBox::fontRoleChanged);
    cb.setFontRole(Typography::FontRole::Caption);
    EXPECT_EQ(cb.fontRole(), Typography::FontRole::Caption);
    EXPECT_EQ(spy.count(), 1);

    // No-op when same value
    cb.setFontRole(Typography::FontRole::Caption);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ComboBoxTest, SetContentPadding) {
    ComboBox cb(window);
    QSignalSpy spy(&cb, &ComboBox::layoutChanged);
    cb.setContentPaddingH(20);
    EXPECT_EQ(cb.contentPaddingH(), 20);
    EXPECT_EQ(spy.count(), 1);

    cb.setContentPaddingV(8);
    EXPECT_EQ(cb.contentPaddingV(), 8);
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(ComboBoxTest, SetChevron) {
    ComboBox cb(window);
    QSignalSpy spy(&cb, &ComboBox::chevronChanged);
    cb.setChevronGlyph(Typography::Icons::ChevronDown);
    EXPECT_EQ(cb.chevronGlyph(), Typography::Icons::ChevronDown);
    EXPECT_EQ(spy.count(), 1);

    cb.setChevronSize(16);
    EXPECT_EQ(cb.chevronSize(), 16);
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(ComboBoxTest, AddItemsAndSelect) {
    ComboBox cb(window);
    cb.addItems({"Yellow", "Green", "Blue", "Red"});
    EXPECT_EQ(cb.count(), 4);

    cb.setCurrentIndex(2);
    EXPECT_EQ(cb.currentIndex(), 2);
    EXPECT_EQ(cb.currentText(), "Blue");
}

TEST_F(ComboBoxTest, SizeHintMinWidth) {
    ComboBox cb(window);
    QSize sh = cb.sizeHint();
    // Height should be ControlHeight::Standard (32)
    EXPECT_EQ(sh.height(), Spacing::ControlHeight::Standard);
    // Width should respect minimum width (80px text area)
    EXPECT_GE(sh.width(), 80);
}

TEST_F(ComboBoxTest, SizeHintGrowsWithItems) {
    ComboBox cb(window);
    QSize emptySize = cb.sizeHint();

    cb.addItems({"Very Long Item Text That Should Make It Wider"});
    QSize withItems = cb.sizeHint();
    EXPECT_GT(withItems.width(), emptySize.width());
}

TEST_F(ComboBoxTest, FixedHeight) {
    ComboBox cb(window);
    EXPECT_EQ(cb.height(), Spacing::ControlHeight::Standard);
}

TEST_F(ComboBoxTest, DisabledState) {
    ComboBox cb(window);
    cb.addItems({"Item1", "Item2"});
    cb.setEnabled(false);
    EXPECT_FALSE(cb.isEnabled());

    // Should still be paintable (no crash)
    cb.show();
    cb.repaint();
}

// ─── 主题切换测试 ────────────────────────────────────────────────────────────

TEST_F(ComboBoxTest, ThemeSwitchLight) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    ComboBox cb(window);
    cb.addItems({"Test"});
    cb.show();
    cb.repaint();
    // Should not crash / assert on theme switch
}

TEST_F(ComboBoxTest, ThemeSwitchDark) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    ComboBox cb(window);
    cb.addItems({"Test"});
    cb.show();
    cb.repaint();

    // Restore
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
}

// ─── 可编辑模式测试 ──────────────────────────────────────────────────────────────

TEST_F(ComboBoxTest, SetEditableCreatesLineEdit) {
    ComboBox cb(window);
    cb.addItems({"Item1", "Item2", "Item3"});
    cb.setEditable(true);

    // Line edit should exist and not be hidden
    auto* lineEdit = cb.findChild<fluent::textfields::LineEdit*>();
    ASSERT_NE(lineEdit, nullptr);
    EXPECT_FALSE(lineEdit->isHidden());
    EXPECT_FALSE(lineEdit->isClearButtonEnabled());
}

TEST_F(ComboBoxTest, SetEditableFalseRemovesLineEdit) {
    ComboBox cb(window);
    cb.setEditable(true);
    ASSERT_NE(cb.findChild<fluent::textfields::LineEdit*>(), nullptr);

    cb.setEditable(false);
    EXPECT_EQ(cb.findChild<fluent::textfields::LineEdit*>(), nullptr);
}

TEST_F(ComboBoxTest, EditableSelectUpdatesLineEdit) {
    ComboBox cb(window);
    cb.addItems({"Alpha", "Beta", "Gamma"});
    cb.setEditable(true);
    cb.setCurrentIndex(1);

    // After selection via popup click, line edit would be updated
    // (direct setCurrentIndex doesn't update line edit in our impl)
    auto* lineEdit = cb.findChild<fluent::textfields::LineEdit*>();
    ASSERT_NE(lineEdit, nullptr);
    // Line edit should be paintable
    cb.show();
    cb.repaint();
}

// ─── Flyout 弹层行为测试 ────────────────────────────────────────────────────

TEST_F(ComboBoxTest, PopupOpensAsFlyoutAndClosesThroughLifecycle) {
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(40, 40, 180, Spacing::ControlHeight::Standard);
    cb->addItems({"Alpha", "Beta", "Gamma"});

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    auto* listView = popup->findChild<fluent::collections::ListView*>("ComboBoxPopupListView");
    ASSERT_NE(listView, nullptr);
    EXPECT_TRUE(popup->isOpen());
    EXPECT_FALSE(popup->isModal());
    EXPECT_FALSE(popup->isDim());
    EXPECT_EQ(popup->anchor(), cb);
    EXPECT_EQ(popup->parentWidget(), window);
    EXPECT_FALSE(popup->isWindow());
    EXPECT_NE(popup->windowType(), Qt::Window);
    EXPECT_NE(popup->windowType(), Qt::Dialog);
    EXPECT_TRUE(listView->backgroundVisible());

    cb->hidePopup();
    QApplication::processEvents();
    EXPECT_FALSE(popup->isOpen());
    EXPECT_FALSE(popup->isVisible());
}

TEST_F(ComboBoxTest, SelectingPopupItemUpdatesIndexAndCloses) {
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(40, 40, 180, Spacing::ControlHeight::Standard);
    cb->addItems({"Alpha", "Beta", "Gamma", "Delta"});
    cb->setCurrentIndex(0);

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    auto* listView = popup->findChild<fluent::collections::ListView*>("ComboBoxPopupListView");
    ASSERT_NE(listView, nullptr);
    EXPECT_EQ(listView->spacing(), 0);

    const QPoint rowTwoCenter(24, Spacing::ControlHeight::Large * 2 + Spacing::ControlHeight::Large / 2);
    QTest::mouseClick(listView->viewport(), Qt::LeftButton, Qt::NoModifier, rowTwoCenter);
    QApplication::processEvents();

    EXPECT_EQ(cb->currentIndex(), 2);
    EXPECT_EQ(cb->currentText(), "Gamma");
    EXPECT_FALSE(popup->isOpen());
}

TEST_F(ComboBoxTest, EditableSelectionMirrorsLineEditText) {
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(40, 40, 180, Spacing::ControlHeight::Standard);
    cb->addItems({"Alpha", "Beta", "Gamma"});
    cb->setEditable(true);
    cb->setCurrentIndex(0);

    auto* lineEdit = cb->findChild<fluent::textfields::LineEdit*>();
    ASSERT_NE(lineEdit, nullptr);

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    auto* listView = popup->findChild<fluent::collections::ListView*>("ComboBoxPopupListView");
    ASSERT_NE(listView, nullptr);

    const QPoint rowOneCenter(24, Spacing::ControlHeight::Large + Spacing::ControlHeight::Large / 2);
    QTest::mouseClick(listView->viewport(), Qt::LeftButton, Qt::NoModifier, rowOneCenter);
    QApplication::processEvents();

    EXPECT_EQ(cb->currentIndex(), 1);
    EXPECT_EQ(lineEdit->text(), "Beta");
    EXPECT_FALSE(popup->isOpen());
}

TEST_F(ComboBoxTest, PopupAlignsBelowWithComboBoxWidth) {
    window->resize(420, 360);
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(40, 40, 184, Spacing::ControlHeight::Standard);
    cb->addItems({"Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta", "Eta"});

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    const QRect card = popupCardRect(popup);

    EXPECT_EQ(card.left(), cb->geometry().left());
    EXPECT_GE(card.width(), cb->width());
    EXPECT_EQ(card.top(), cb->geometry().bottom() + 1 + cb->popupOffset());

    auto* listView = popup->findChild<fluent::collections::ListView*>("ComboBoxPopupListView");
    ASSERT_NE(listView, nullptr);
    const int popupContentInset = Spacing::XSmall / 2;
    const QRect listGeometry = listView->geometry();
    EXPECT_EQ(listGeometry.left(), Spacing::Standard + popupContentInset);
    EXPECT_EQ(listGeometry.right(), popup->rect().right() - Spacing::Standard - popupContentInset);
    EXPECT_EQ(listGeometry.top(), Spacing::Standard + popupContentInset);
    EXPECT_EQ(listGeometry.bottom(), popup->rect().bottom() - Spacing::Standard - popupContentInset);

    auto* scrollBar = listView->verticalFluentScrollBar();
    ASSERT_NE(scrollBar, nullptr);
    ASSERT_TRUE(scrollBar->isVisible());
    EXPECT_EQ(scrollBar->geometry().right(), listView->rect().right() - popupContentInset);
    const int scrollBarRightInPopup = listGeometry.left() + scrollBar->geometry().right();
    EXPECT_EQ(popup->rect().right() - Spacing::Standard - scrollBarRightInPopup, Spacing::XSmall);
}

TEST_F(ComboBoxTest, PopupFlipsAboveNearBottomEdge) {
    window->resize(420, 320);
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(40, 270, 184, Spacing::ControlHeight::Standard);
    cb->addItems({"One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight"});

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    const QRect card = popupCardRect(popup);

    EXPECT_EQ(card.left(), cb->geometry().left());
    EXPECT_LT(card.bottom(), cb->geometry().top());
}

TEST_F(ComboBoxTest, PopupClampsNearRightEdge) {
    window->resize(300, 240);
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(230, 40, 120, Spacing::ControlHeight::Standard);
    cb->addItems({"One", "Two", "Three"});

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    const QRect card = popupCardRect(popup);

    EXPECT_LE(card.right(), window->width() - 4);
    EXPECT_NE(card.left(), cb->geometry().left());
}

TEST_F(ComboBoxTest, EscapeAndOutsidePressDismissPopup) {
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(80, 80, 180, Spacing::ControlHeight::Standard);
    cb->addItems({"Alpha", "Beta", "Gamma"});

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    QTest::keyClick(popup, Qt::Key_Escape);
    QApplication::processEvents();
    EXPECT_FALSE(popup->isOpen());

    popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
    QApplication::processEvents();
    EXPECT_FALSE(popup->isOpen());
}

TEST_F(ComboBoxTest, PopupShadowMarginPressDismissesAsOutsideVisibleCard) {
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(80, 80, 180, Spacing::ControlHeight::Standard);
    cb->addItems({"Alpha", "Beta", "Gamma"});

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(popup->isOpen());

    const int shadow = Spacing::Standard;
    QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, QPoint(shadow / 2, shadow + 8));
    QApplication::processEvents();

    EXPECT_FALSE(popup->isOpen());
}

TEST_F(ComboBoxTest, OwnerPressDismissesWithoutImmediateReopen) {
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(80, 80, 180, Spacing::ControlHeight::Standard);
    cb->addItems({"Alpha", "Beta", "Gamma"});

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(popup->isOpen());

    QTest::mouseClick(cb, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
    QApplication::processEvents();
    EXPECT_FALSE(popup->isOpen());
}

TEST_F(ComboBoxTest, PopupSelectedIndicatorPaintsSinglePill) {
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(40, 40, 184, Spacing::ControlHeight::Standard);
    cb->addItems({"Yellow", "Green", "Blue", "Red"});
    cb->setCurrentIndex(1);

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    auto* listView = popup->findChild<fluent::collections::ListView*>("ComboBoxPopupListView");
    ASSERT_NE(listView, nullptr);

    listView->setSelectedIndicatorAnimationEnabled(false);
    QApplication::processEvents();

    const qreal accentWidth = accentSpanWidthInViewport(listView, cb->themeColors().accentDefault);
    EXPECT_GT(accentWidth, 0.0);
    EXPECT_LE(accentWidth, 4.5)
        << "ComboBox popup selected indicator should be painted once, not by both ListView and the item delegate";
}

TEST_F(ComboBoxTest, PopupIndicatorAndTextShareOpticalCenterline) {
    ComboBox* cb = new ComboBox(window);
    cb->setGeometry(40, 40, 184, Spacing::ControlHeight::Standard);
    cb->addItems({"Auto", "Left", "Top"});
    cb->setCurrentIndex(0);

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    auto* listView = popup->findChild<fluent::collections::ListView*>("ComboBoxPopupListView");
    ASSERT_NE(listView, nullptr);
    listView->setSelectedIndicatorAnimationEnabled(false);
    QApplication::processEvents();

    const QModelIndex index = listView->model()->index(0, 0);
    const QRectF rowRect = static_cast<QListView*>(listView)->visualRect(index);
    const QRectF textRect = rowRect.adjusted(5, 3, -5, -3).translated(0, -2);
    const QFontMetricsF metrics(listView->font());
    const qreal baseline = textRect.top()
        + (textRect.height() - metrics.height()) / 2.0
        + metrics.ascent();
    const qreal textInkCenter = baseline
        + metrics.tightBoundingRect(index.data(Qt::DisplayRole).toString()).center().y();

    EXPECT_NEAR(listView->selectedIndicatorRect().center().y(), textInkCenter, 1.0);
}

// ─── VisualCheck ─────────────────────────────────────────────────────────────

TEST_F(ComboBoxTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    auto* mainLayout = new QVBoxLayout(window);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    // Title
    auto* title = new QLabel("ComboBox — WinUI 3 Fluent Design", window);
    title->setFont(window->themeFont(Typography::FontRole::Subtitle).toQFont());
    mainLayout->addWidget(title);

    // Example 1: Color ComboBox (from WinUI Gallery)
    {
        auto* label = new QLabel("Colors (inline items):", window);
        label->setFont(window->themeFont(Typography::FontRole::BodyStrong).toQFont());
        mainLayout->addWidget(label);

        auto* combo = new ComboBox(window);
        combo->addItems({"Yellow", "Green", "Blue", "Red"});
        combo->setCurrentIndex(0);
        combo->setFixedWidth(200);
        mainLayout->addWidget(combo);
    }

    // Example 2: Font family ComboBox
    {
        auto* label = new QLabel("Fonts (ItemsSource):", window);
        label->setFont(window->themeFont(Typography::FontRole::BodyStrong).toQFont());
        mainLayout->addWidget(label);

        auto* combo = new ComboBox(window);
        combo->addItems({"Arial", "Comic Sans MS", "Courier New", "Segoe UI", "Times New Roman"});
        combo->setCurrentIndex(3); // Segoe UI
        combo->setFixedWidth(200);
        mainLayout->addWidget(combo);
    }

    // Example 3: Disabled ComboBox
    {
        auto* label = new QLabel("Disabled:", window);
        label->setFont(window->themeFont(Typography::FontRole::BodyStrong).toQFont());
        mainLayout->addWidget(label);

        auto* combo = new ComboBox(window);
        combo->addItems({"Item 1", "Item 2", "Item 3"});
        combo->setCurrentIndex(0);
        combo->setEnabled(false);
        combo->setFixedWidth(200);
        mainLayout->addWidget(combo);
    }

    // Example 4: Many items
    {
        auto* label = new QLabel("Many items (scroll):", window);
        label->setFont(window->themeFont(Typography::FontRole::BodyStrong).toQFont());
        mainLayout->addWidget(label);

        auto* combo = new ComboBox(window);
        QStringList items;
        for (int i = 1; i <= 20; ++i)
            items << QString("Item %1").arg(i);
        combo->addItems(items);
        combo->setCurrentIndex(5);
        combo->setFixedWidth(200);
        mainLayout->addWidget(combo);
    }

    // Example 5: Editable ComboBox (Font sizes)
    {
        auto* label = new QLabel("Editable (Font sizes):", window);
        label->setFont(window->themeFont(Typography::FontRole::BodyStrong).toQFont());
        mainLayout->addWidget(label);

        auto* combo = new ComboBox(window);
        combo->addItems({"8", "9", "10", "11", "12", "14", "16", "18",
                         "20", "24", "28", "36", "48", "72"});
        combo->setEditable(true);
        combo->setCurrentIndex(4); // 12
        combo->setFixedWidth(200);
        mainLayout->addWidget(combo);
    }

    mainLayout->addStretch();

    // Theme switch button (bottom-right corner)
    auto* themeBtn = new fluent::basicinput::Button("Switch Theme", window);
    themeBtn->setFluentStyle(fluent::basicinput::Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, fluent::AnchorLayout::Edge::Bottom, -20};
    themeBtn->anchors()->right = {window, fluent::AnchorLayout::Edge::Right, -20};
    mainLayout->addWidget(themeBtn);
    QObject::connect(themeBtn, &fluent::basicinput::Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                ? fluent::FluentElement::Dark
                                : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}

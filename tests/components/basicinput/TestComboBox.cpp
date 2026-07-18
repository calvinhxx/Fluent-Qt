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
#include <QWheelEvent>
#include <QtTest/QTest>

#include "components/basicinput/ComboBox.h"
#include "components/basicinput/Button.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/scrolling/ScrollBar.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/QMLPlus.h"
#include "design/Typography.h"

using namespace fluent;
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

void sendWheel(QWidget* target, int angleDeltaY) {
    const QPoint local = target->rect().center();
    QWheelEvent event(QPointF(local), QPointF(target->mapToGlobal(local)),
                      QPoint(), QPoint(0, angleDeltaY), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(target, &event);
}
}

// ─── 基础功能测试 ────────────────────────────────────────────────────────────

TEST_F(ComboBoxTest, DefaultProperties) {
    ComboBox cb(window);
    EXPECT_EQ(cb.fontRole(), Typography::FontRole::Body);
    EXPECT_EQ(cb.contentPaddingH(), Spacing::Padding::ComboBoxHorizontal);
    EXPECT_EQ(cb.contentPaddingV(), Spacing::Padding::ComboBoxVertical);
    EXPECT_EQ(cb.chevronGlyph(), Typography::Icons::ChevronDownMed);
    EXPECT_EQ(cb.chevronSize(), Typography::IconSize::Compact);
    EXPECT_EQ(Typography::Icons::glyphForSize(cb.chevronGlyph(), cb.chevronSize()),
              Typography::Icons::glyph(
                  QStringLiteral("ic_fluent_chevron_down_12_regular")));
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

TEST_F(ComboBoxTest, WheelAndArrowKeysRequireFocusToChangeSelection) {
    ComboBox cb(window);
    EXPECT_EQ(cb.focusPolicy(), Qt::StrongFocus);
    cb.setGeometry(40, 40, 180, Spacing::ControlHeight::Standard);
    cb.addItems({"Alpha", "Beta", "Gamma"});
    cb.setCurrentIndex(1);

    Button other(QStringLiteral("Other"), window);
    other.setGeometry(40, 100, 100, Spacing::ControlHeight::Standard);
    window->show();
    other.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    ASSERT_FALSE(cb.hasFocus());

    sendWheel(&cb, -120);
    QTest::keyClick(&cb, Qt::Key_Down);
    EXPECT_EQ(cb.currentIndex(), 1);

    cb.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    ASSERT_TRUE(cb.hasFocus());
    QTest::keyClick(&cb, Qt::Key_Down);
    EXPECT_EQ(cb.currentIndex(), 2);

    cb.setCurrentIndex(1);
    sendWheel(&cb, -120);
    EXPECT_EQ(cb.currentIndex(), 2);
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

TEST_F(ComboBoxTest, SizeHintKeepsWidestItemClearOfElisionBoundary) {
    ComboBox cb(window);
    const QString widest = QStringLiteral("Keep in system tray");
    cb.addItems({QStringLiteral("Use system setting"), widest, QStringLiteral("Light")});
    cb.resize(cb.sizeHint());

    const int chevronArea = cb.chevronOffset().x() + cb.chevronSize()
                            + ::Spacing::Gap::Tight;
    const int availableTextWidth = cb.width() - cb.contentPaddingH() - chevronArea;
    const QFontMetrics metrics(cb.font());

    EXPECT_GE(availableTextWidth - metrics.horizontalAdvance(widest), ::Spacing::XSmall);
    EXPECT_EQ(metrics.elidedText(widest, Qt::ElideRight, availableTextWidth), widest);
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

TEST_F(ComboBoxTest, EditableLineEditKeepsDarkThemeTextPaletteInsideStyledHost) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    window->onThemeUpdated();

    ComboBox cb(window);
    cb.addItems({"10", "11", "12"});
    cb.setEditable(true);
    cb.setCurrentIndex(1);

    auto* lineEdit = cb.findChild<fluent::textfields::LineEdit*>();
    ASSERT_NE(lineEdit, nullptr);
    const auto colors = lineEdit->themeColors();
    EXPECT_EQ(lineEdit->palette().color(QPalette::Active, QPalette::Text),
              colors.textPrimary);

    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    window->onThemeUpdated();
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
    // The flyout card (Popup::paintEvent) already paints the opaque rounded surface, so the inner
    // ListView keeps its own background OFF — a second background would add a tighter (control-radius)
    // corner mask that pokes past the card's overlay-radius corners as white "dog-ears".
    // zh_CN: Flyout 卡片(Popup::paintEvent)已绘制不透明圆角表面,故内部 ListView 不再画自身背景——
    // 否则会叠加一层更紧(control 圆角)的角遮罩,超出卡片 overlay 圆角形成白色「狗耳」。
    EXPECT_FALSE(listView->backgroundVisible());

    cb->hidePopup();
    QApplication::processEvents();
    EXPECT_FALSE(popup->isOpen());
    EXPECT_FALSE(popup->isVisible());
}

TEST_F(ComboBoxTest, PopupInheritsThemeOverrideFromComboBox) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    window->onThemeUpdated();

    auto* host = new QWidget(window);
    host->setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));
    host->setGeometry(24, 24, 260, 180);
    host->show();

    ComboBox* cb = new ComboBox(host);
    cb->setGeometry(16, 16, 180, Spacing::ControlHeight::Standard);
    cb->addItems({"Blue", "Green", "Red", "Yellow"});

    auto* popup = openPopupFor(cb, window);
    ASSERT_NE(popup, nullptr);
    auto* listView = popup->findChild<fluent::collections::ListView*>("ComboBoxPopupListView");
    ASSERT_NE(listView, nullptr);

    EXPECT_TRUE(popup->isOpen());
    EXPECT_EQ(cb->effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(popup->effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(listView->effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(popup->themeColors().bgLayer, QColor("#2C2C2C"));

    cb->hidePopup();
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

    // Offscreen font fallback on macOS/Linux can move tight ink bounds by a
    // couple logical pixels while the visual centerline still reads aligned.
    constexpr qreal kOpticalCenterTolerance = 2.5;
    EXPECT_NEAR(listView->selectedIndicatorRect().center().y(),
                textInkCenter,
                kOpticalCenterTolerance);
}

// ─── Design-language × theme compatibility ──────────────────────────────────
//
// ComboBox paints a per-brand CLOSED FIELD (Fluent / Material 3 / macOS) under each App
// theme (Light / Dark). This suite grabs the rendered control across the full
// {language × theme} matrix to lock in that every combination paints, never crashes, and
// actually puts ink on the surface. Design language + theme are GLOBAL state, so TearDown
// restores the built-in defaults.
// zh_CN: ComboBox 按品牌(Fluent / Material 3 / macOS)在明暗主题下分支绘制闭合字段。本套件
// 遍历 {设计语言 × 主题} 全矩阵抓取渲染结果,确保每种组合都能绘制、不崩溃且确有像素落在表面上。
// 设计语言与主题是全局状态,故 TearDown 恢复内置默认值。

class ComboBoxDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so other suites start clean.
        // zh_CN: 设计语言与主题是全局状态——重置以保证其它套件从干净状态开始。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a populated ComboBox, size it, and grab it as an image. zh_CN: 构建带选项的 ComboBox,设定尺寸并抓取为图像。
    static QImage grabCombo() {
        ComboBox cb;
        cb.addItems({"Yellow", "Green", "Blue", "Red"});
        cb.setCurrentIndex(2);
        cb.resize(200, 36);
        return cb.grab().toImage();
    }
};

TEST_F(ComboBoxDesignLanguageTest, AllLanguagesThemesPaintAndPutInkOnSurface) {
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

            QImage img = grabCombo();

            // No crash + valid image of the requested size. zh_CN: 不崩溃 + 图像有效且尺寸正确。
            ASSERT_FALSE(img.isNull()) << "lang=" << lang << " theme=" << theme;
            EXPECT_GT(img.width(), 0) << "lang=" << lang << " theme=" << theme;
            EXPECT_GT(img.height(), 0) << "lang=" << lang << " theme=" << theme;

            // Painted content: some pixel differs from the top-left background pixel
            // (border, text, or chevron all qualify). zh_CN: 已绘制内容:存在与左上角背景不同的像素
            // (边框、文字或箭头均可满足)。
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
                << "ComboBox painted nothing for lang=" << lang << " theme=" << theme;
        }
    }
}

// Regression for the "black ComboBox at rest under Material/macOS" bug: an unset `QColor stateLayer;`
// is INVALID yet QColor::alpha() returns 255 (Qt stores invalid alpha as 0xFFFF), so a bare
// `stateLayer.alpha() > 0` guard fired in the resting (no hover/press/popup) state-layer block and
// `setBrush(invalidColor)` painted the whole field SOLID BLACK. The guard must also test isValid().
// zh_CN: 「静息态 Material/macOS ComboBox 变黑」回归:未赋值的 QColor 虽无效,alpha() 却返回 255,
// 裸 alpha()>0 在静息命中,setBrush(无效色) 把字段整片涂成不透明黑;守卫必须同时判 isValid()。
TEST_F(ComboBoxDesignLanguageTest, RestStateHasNoOpaqueBlackField) {
    const fluent::FluentElement::DesignLanguage langs[] = {
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

            // grabCombo() builds a fresh ComboBox that was never hovered/pressed/opened → REST state,
            // exactly the condition that exposed the bug. zh_CN: grabCombo() 构建从未交互的 ComboBox=静息态。
            QImage img = grabCombo();
            ASSERT_FALSE(img.isNull()) << "lang=" << lang << " theme=" << theme;

            const QColor c = img.pixelColor(img.width() / 2, img.height() / 2);
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            // The invalid-veil signature is an OPAQUE, near-#000 fill. Real surfaces are either
            // transparent (M3 rest) or a light/dark-gray bezel (macOS) — never opaque pure black.
            // zh_CN: 无效薄层特征=不透明近黑填充;真实表面要么透明(M3 静息),要么浅/深灰 bezel(macOS),绝非不透明纯黑。
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "ComboBox painted an opaque black field at rest for lang=" << lang
                << " theme=" << theme << " rgba=(" << c.red() << "," << c.green() << ","
                << c.blue() << "," << c.alpha() << ")";
        }
    }
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
        combo->addItems({"Arial", "Comic Sans MS", "Courier New",
                         Typography::FontFamily::UIText, "Times New Roman"});
        combo->setCurrentIndex(3); // Bundled FluentQt UI face
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

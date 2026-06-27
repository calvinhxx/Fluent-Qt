#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>
#include <QTime>

#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/date_time/TimePicker.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/textfields/Label.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::date_time::TimePicker;
using fluent::dialogs_flyouts::Flyout;
using fluent::textfields::Label;

namespace {

using Edge = AnchorLayout::Edge;

class TimePickerTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(palette);
        setAutoFillBackground(true);
    }
};

void processEvents()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

void showWindow(TimePickerTestWindow* window)
{
    window->show();
    const bool exposed = QTest::qWaitForWindowExposed(window);
    Q_UNUSED(exposed);
    processEvents();
}

Flyout* openPopupFor(TimePicker* picker, TimePickerTestWindow* window)
{
    showWindow(window);
    picker->show();
    processEvents();
    picker->openPicker();
    processEvents();
    return window->findChild<Flyout*>(QStringLiteral("TimePickerFlyout"));
}

QWidget* columnFor(Flyout* popup, const QString& objectName)
{
    return popup ? popup->findChild<QWidget*>(objectName) : nullptr;
}

Button* buttonFor(Flyout* popup, const QString& objectName)
{
    return popup ? popup->findChild<Button*>(objectName) : nullptr;
}

QPoint nextRowCenter(QWidget* column)
{
    constexpr int kPickerRowHeight = 36;
    return column->rect().center() + QPoint(0, kPickerRowHeight);
}

QString pickerChevronUpGlyph()
{
    return Typography::Icons::FlipViewPrevV;
}

QString pickerChevronDownGlyph()
{
    return Typography::Icons::FlipViewNextV;
}

} // namespace

class TimePickerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<QTime>("QTime");
        qRegisterMetaType<fluent::date_time::TimePicker::TimeField>(
            "fluent::date_time::TimePicker::TimeField");
        qRegisterMetaType<fluent::date_time::TimePicker::ClockIdentifier>(
            "fluent::date_time::TimePicker::ClockIdentifier");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new TimePickerTestWindow();
        window->resize(760, 540);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    TimePickerTestWindow* window = nullptr;
};

TEST_F(TimePickerTest, DefaultsAndInheritanceMatchComponentPattern)
{
    TimePicker picker;

    EXPECT_TRUE(picker.time().isValid());
    EXPECT_FALSE(picker.selectedTime().isValid());
    EXPECT_EQ(picker.minuteIncrement(), 1);
    EXPECT_EQ(picker.clockIdentifier(), TimePicker::ClockIdentifier::TwelveHourClock);
    EXPECT_FALSE(picker.isDropDownOpen());
    EXPECT_FALSE(picker.isOpen());
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Hour), QStringLiteral("hour"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Minute), QStringLiteral("minute"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Period), QStringLiteral("AM/PM"));
    EXPECT_EQ(picker.focusPolicy(), Qt::StrongFocus);
#ifdef Q_OS_MAC
    EXPECT_FALSE(picker.testAttribute(Qt::WA_MacShowFocusRect));
#endif
    EXPECT_FALSE(picker.sizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<Button*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&picker), nullptr);
}

TEST_F(TimePickerTest, SelectedTimeClearAndFormattingDriveSegments)
{
    TimePicker picker;
    QSignalSpy selectedSpy(&picker, &TimePicker::selectedTimeChanged);
    QSignalSpy timeSpy(&picker, &TimePicker::timeChanged);

    picker.setSelectedTime(QTime(0, 5));

    ASSERT_EQ(selectedSpy.count(), 1);
    ASSERT_EQ(timeSpy.count(), 1);
    EXPECT_EQ(picker.selectedTime(), QTime(0, 5));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Hour), QStringLiteral("12"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Minute), QStringLiteral("05"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Period), QStringLiteral("AM"));

    picker.setSelectedTime(QTime(12, 0));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Hour), QStringLiteral("12"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Minute), QStringLiteral("00"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Period), QStringLiteral("PM"));

    picker.setSelectedTime(QTime(12, 0));
    EXPECT_EQ(selectedSpy.count(), 2);
    EXPECT_EQ(timeSpy.count(), 2);

    picker.clearSelectedTime();
    ASSERT_EQ(selectedSpy.count(), 3);
    EXPECT_FALSE(picker.selectedTime().isValid());
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Hour), QStringLiteral("hour"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Minute), QStringLiteral("minute"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Period), QStringLiteral("AM/PM"));
}

TEST_F(TimePickerTest, TwentyFourHourModeHidesPeriodAndPreservesAbsoluteTime)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(40, 40, 360, picker->sizeHint().height());
    picker->setSelectedTime(QTime(23, 45));

    QSignalSpy modeSpy(picker, &TimePicker::clockIdentifierChanged);
    picker->setClockIdentifier(TimePicker::ClockIdentifier::TwentyFourHourClock);

    EXPECT_EQ(modeSpy.count(), 1);
    EXPECT_EQ(picker->selectedTime(), QTime(23, 45));
    EXPECT_EQ(picker->fieldDisplayText(TimePicker::TimeField::Hour), QStringLiteral("23"));
    EXPECT_EQ(picker->fieldDisplayText(TimePicker::TimeField::Minute), QStringLiteral("45"));
    EXPECT_TRUE(picker->fieldDisplayText(TimePicker::TimeField::Period).isEmpty());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    EXPECT_TRUE(columnFor(popup, QStringLiteral("TimePickerHourColumn"))->isVisible());
    EXPECT_TRUE(columnFor(popup, QStringLiteral("TimePickerMinuteColumn"))->isVisible());
    EXPECT_FALSE(columnFor(popup, QStringLiteral("TimePickerPeriodColumn"))->isVisible());
}

TEST_F(TimePickerTest, MinuteIncrementClampsAndSnapsWithoutRollingHour)
{
    TimePicker picker;
    QSignalSpy incrementSpy(&picker, &TimePicker::minuteIncrementChanged);
    QSignalSpy selectedSpy(&picker, &TimePicker::selectedTimeChanged);

    picker.setMinuteIncrement(0);
    EXPECT_EQ(picker.minuteIncrement(), 1);
    EXPECT_EQ(incrementSpy.count(), 0);

    picker.setMinuteIncrement(15);
    ASSERT_EQ(incrementSpy.count(), 1);
    EXPECT_EQ(picker.minuteIncrement(), 15);

    picker.setSelectedTime(QTime(9, 58));
    ASSERT_EQ(selectedSpy.count(), 1);
    EXPECT_EQ(picker.selectedTime(), QTime(9, 45));
    EXPECT_EQ(picker.time(), QTime(9, 45));

    picker.setMinuteIncrement(99);
    EXPECT_EQ(picker.minuteIncrement(), 59);
    EXPECT_EQ(picker.selectedTime(), QTime(9, 59));
}

TEST_F(TimePickerTest, EntryOpensWithMouseAndKeyboardAndDisabledStateBlocksOpen)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(80, 80, 320, picker->sizeHint().height());
    QSignalSpy openSpy(picker, &TimePicker::dropDownOpenChanged);

    showWindow(window);
    QTest::mouseClick(picker, Qt::LeftButton, Qt::NoModifier, picker->rect().center());
    processEvents();
    EXPECT_TRUE(picker->isDropDownOpen());
    EXPECT_TRUE(picker->isOpen());
    ASSERT_EQ(openSpy.count(), 1);
    picker->closePicker();
    processEvents();
    EXPECT_FALSE(picker->isOpen());

    picker->setFocus();
    QTest::keyClick(picker, Qt::Key_Space);
    processEvents();
    EXPECT_TRUE(picker->isDropDownOpen());
    picker->closePicker();
    processEvents();

    picker->setEnabled(false);
    picker->openPicker();
    processEvents();
    EXPECT_FALSE(picker->isDropDownOpen());
}

TEST_F(TimePickerTest, FlyoutCommitCancelEscapeAndLightDismiss)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setSelectedTime(QTime(9, 30));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    QTest::keyClick(minuteColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerCancelButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(9, 30));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    QTest::keyClick(minuteColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(9, 31));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    QTest::keyClick(hourColumn, Qt::Key_Down);
    QTest::keyClick(hourColumn, Qt::Key_Return);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(10, 31));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    QTest::keyClick(hourColumn, Qt::Key_Down);
    QTest::keyClick(popup, Qt::Key_Escape);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(10, 31));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    QTest::keyClick(hourColumn, Qt::Key_Down);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(10, 31));
    EXPECT_FALSE(picker->isDropDownOpen());
}

TEST_F(TimePickerTest, FlyoutWheelRowsAndPeriodColumnUpdatePendingTime)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setMinuteIncrement(15);
    picker->setSelectedTime(QTime(11, 30));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    QTest::mouseClick(minuteColumn, Qt::LeftButton, Qt::NoModifier, nextRowCenter(minuteColumn));
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(11, 45));

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* periodColumn = columnFor(popup, QStringLiteral("TimePickerPeriodColumn"));
    ASSERT_NE(periodColumn, nullptr);
    EXPECT_EQ(periodColumn->property("visibleItemCount").toInt(), 2);
    QTest::keyClick(periodColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(23, 45));

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    periodColumn = columnFor(popup, QStringLiteral("TimePickerPeriodColumn"));
    ASSERT_NE(periodColumn, nullptr);
    QTest::keyClick(periodColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(23, 45));

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    periodColumn = columnFor(popup, QStringLiteral("TimePickerPeriodColumn"));
    ASSERT_NE(periodColumn, nullptr);
    QTest::keyClick(periodColumn, Qt::Key_Up);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(11, 45));

    picker->setSelectedTime(QTime(23, 45));
    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    const QPoint wheelPoint = minuteColumn->rect().center();
    FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -120, Qt::NoModifier);
    QApplication::sendEvent(minuteColumn, &wheel);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(23, 0));
}

TEST_F(TimePickerTest, FlyoutColumnsUseCaretGlyphsAndNoFocusFrame)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(40, 60, 420, picker->sizeHint().height());
    picker->setSelectedTime(QTime(9, 30));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);

    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    minuteColumn->setFocus();
    processEvents();

    EXPECT_EQ(minuteColumn->property("previousButtonGlyph").toString(), pickerChevronUpGlyph());
    EXPECT_EQ(minuteColumn->property("nextButtonGlyph").toString(), pickerChevronDownGlyph());
    EXPECT_EQ(minuteColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignHCenter));
    EXPECT_FALSE(minuteColumn->property("focusFrameVisible").toBool());
#ifdef Q_OS_MAC
    EXPECT_FALSE(minuteColumn->testAttribute(Qt::WA_MacShowFocusRect));
#endif
    EXPECT_TRUE(minuteColumn->property("selectedRowHasBackground").toBool());

    FLUENT_MAKE_ENTER_EVENT(enterEvent, minuteColumn->rect().center().x(), minuteColumn->rect().center().y());
    QApplication::sendEvent(minuteColumn, &enterEvent);
    QTest::qWait(180);
    processEvents();
    EXPECT_TRUE(minuteColumn->property("columnHovered").toBool());
    EXPECT_GT(minuteColumn->property("navButtonOpacity").toReal(), 0.9);
}

TEST_F(TimePickerTest, FieldTextAlignmentConfiguresFlyoutColumns)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(40, 60, 420, picker->sizeHint().height());
    picker->setSelectedTime(QTime(9, 30));

    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Hour), Qt::AlignLeft);
    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Minute), Qt::AlignHCenter);
    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Period), Qt::AlignHCenter);

    picker->setFieldTextAlignment(TimePicker::TimeField::Hour, Qt::AlignRight);
    picker->setFieldTextAlignment(TimePicker::TimeField::Minute, Qt::AlignLeft);
    picker->setFieldTextAlignment(TimePicker::TimeField::Period, Qt::AlignCenter);

    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Hour), Qt::AlignRight);
    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Minute), Qt::AlignLeft);
    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Period), Qt::AlignHCenter);

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    QWidget* periodColumn = columnFor(popup, QStringLiteral("TimePickerPeriodColumn"));
    ASSERT_NE(hourColumn, nullptr);
    ASSERT_NE(minuteColumn, nullptr);
    ASSERT_NE(periodColumn, nullptr);

    EXPECT_EQ(hourColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignRight));
    EXPECT_EQ(minuteColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignLeft));
    EXPECT_EQ(periodColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignHCenter));
}

TEST_F(TimePickerTest, FlyoutWheelUsesThresholdAccumulation)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setSelectedTime(QTime(9, 30));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    QPoint wheelPoint = minuteColumn->rect().center();
    for (int i = 0; i < 3; ++i) {
        FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -30, Qt::NoModifier);
        QApplication::sendEvent(minuteColumn, &wheel);
        EXPECT_TRUE(wheel.isAccepted());
    }
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(9, 30));

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    wheelPoint = minuteColumn->rect().center();
    for (int i = 0; i < 4; ++i) {
        FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -30, Qt::NoModifier);
        QApplication::sendEvent(minuteColumn, &wheel);
        EXPECT_TRUE(wheel.isAccepted());
    }
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(9, 31));
}

TEST_F(TimePickerTest, ThemeUpdateRefreshesVisiblePopup)
{
    TimePicker* picker = new TimePicker(window);
    picker->setSelectedTime(QTime(9, 30));
    EXPECT_EQ(picker->sizeHint().height(), picker->minimumSizeHint().height());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    processEvents();
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
    EXPECT_TRUE(popup->isVisible());
}

// ─── Design-language × theme compatibility ──────────────────────────────────
//
// TimePicker's CLOSED FIELD must read as the correct per-brand surface under each design language
// (Fluent / Material 3 / macOS) crossed with each App theme (Light / Dark): Fluent stays the WinUI
// control surface, Material 3 switches to the Outlined field, macOS to the push-button bezel. Design
// language + theme are GLOBAL singletons, so the fixture restores both in TearDown. The flyout's
// spinner columns (where the per-brand SELECTED-ROW highlight lives) are internal and need an open
// popup whose animation does not advance headlessly, so this suite exercises the closed field
// statically via grab() — that already drives TimePicker::paintEvent's branded surface.
// zh_CN: TimePicker 的闭合字段在三种设计语言(Fluent/Material 3/macOS)× 两种主题(Light/Dark)下都必须
// 呈现正确的品牌表面:Fluent 保持 WinUI 控件面,Material 3 切换为描边字段,macOS 切换为按钮 bezel。设计语言
// 与主题为全局单例,夹具在 TearDown 中恢复二者。弹层的滚轮列(品牌选中行高亮所在)为内部组件且需展开弹层
//(其动画在无头环境下不前进),故本套件通过 grab() 静态检验闭合字段——这已驱动 TimePicker::paintEvent 的品牌表面。
class TimePickerDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a TimePicker sized like a real closed field and grab it as an image. zh_CN: 以真实闭合字段尺寸构建 TimePicker 并抓取为图像。
    static QImage grabClosedField()
    {
        TimePicker picker;
        picker.setSelectedTime(QTime(9, 30));
        picker.resize(220, 36);
        return picker.grab().toImage();
    }

    static bool hasPaintedContent(const QImage& img)
    {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }

    static int differingPixels(const QImage& a, const QImage& b)
    {
        if (a.size() != b.size())
            return -1;
        int diff = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = 0; x < a.width(); ++x)
                if (a.pixel(x, y) != b.pixel(x, y))
                    ++diff;
        return diff;
    }
};

TEST_F(TimePickerDesignLanguageTest, ClosedFieldPaintsPerBrandWithoutOpaqueBlackAtRest)
{
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };

    // Capture the Fluent baseline per theme so Material can be asserted DISTINCT from it. zh_CN: 按主题记录 Fluent 基线,以断言 Material 与之不同。
    QImage fluentByTheme[2];

    for (const auto& th : themes) {
        fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignFluent);
        fluent::FluentElement::setTheme(th.theme);
        fluentByTheme[th.theme] = grabClosedField();
    }

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const std::string ctx = std::string(lang.name) + "/" + th.name;
            const QImage field = grabClosedField();

            // 1. The closed field paints a valid, non-empty image with content. zh_CN: 闭合字段绘制出有效、非空且有内容的图像。
            ASSERT_FALSE(field.isNull()) << ctx;
            EXPECT_GT(field.width(), 0) << ctx;
            EXPECT_GT(field.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(field)) << "closed field painted nothing: " << ctx;

            // 2. No opaque near-black fill at rest (the invalid-QColor trap regression): sample a
            // field-background pixel near the left edge, away from the segment text. zh_CN: 静息下无不透明近黑
            // 填充(无效 QColor 陷阱回归):在左缘附近、远离分段文字处采样字段背景像素。
            const QColor c = field.pixelColor(6, field.height() / 2);
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "TimePicker painted an opaque black field surface at rest: " << ctx
                << " rgba=(" << c.red() << "," << c.green() << "," << c.blue() << "," << c.alpha() << ")";

            // 3. The Material field must be visibly distinct from the Fluent field (Outlined vs. control
            // surface). zh_CN: Material 字段必须与 Fluent 字段明显不同(描边 vs 控件面)。
            if (lang.lang == fluent::FluentElement::DesignMaterial) {
                const int diff = differingPixels(field, fluentByTheme[th.theme]);
                ASSERT_GE(diff, 0) << ctx << " (size mismatch)";
                EXPECT_GT(diff, 0)
                    << "Material closed field is indistinguishable from Fluent: " << ctx;
            }
        }
    }
}

TEST_F(TimePickerTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->resize(920, 560);
    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);

    auto* title = new Label(QStringLiteral("TimePicker"), window);
    title->setFluentTypography(QStringLiteral("Title"));
    title->anchors()->top = {window, Edge::Top, 28};
    title->anchors()->left = {window, Edge::Left, 32};
    title->anchors()->right = {window, Edge::Right, -32};
    layout->addWidget(title);

    auto* simpleLabel = new Label(QStringLiteral("Pick a time"), window);
    simpleLabel->setFluentTypography(Typography::FontRole::Body);
    simpleLabel->anchors()->top = {title, Edge::Bottom, 28};
    simpleLabel->anchors()->left = {title, Edge::Left, 0};
    layout->addWidget(simpleLabel);

    auto* simple = new TimePicker(window);
    simple->anchors()->top = {simpleLabel, Edge::Bottom, 6};
    simple->anchors()->left = {simpleLabel, Edge::Left, 0};
    layout->addWidget(simple);

    auto* afternoonLabel = new Label(QStringLiteral("Afternoon"), window);
    afternoonLabel->setFluentTypography(Typography::FontRole::Body);
    afternoonLabel->anchors()->top = {simple, Edge::Bottom, 28};
    afternoonLabel->anchors()->left = {simple, Edge::Left, 0};
    layout->addWidget(afternoonLabel);

    auto* afternoon = new TimePicker(window);
    afternoon->setSelectedTime(QTime(14, 30));
    afternoon->anchors()->top = {afternoonLabel, Edge::Bottom, 6};
    afternoon->anchors()->left = {afternoonLabel, Edge::Left, 0};
    layout->addWidget(afternoon);

    auto* steppedLabel = new Label(QStringLiteral("15 minute increment"), window);
    steppedLabel->setFluentTypography(Typography::FontRole::Body);
    steppedLabel->anchors()->top = {afternoon, Edge::Bottom, 28};
    steppedLabel->anchors()->left = {afternoon, Edge::Left, 0};
    layout->addWidget(steppedLabel);

    auto* stepped = new TimePicker(window);
    stepped->setMinuteIncrement(15);
    stepped->setSelectedTime(QTime(9, 45));
    stepped->anchors()->top = {steppedLabel, Edge::Bottom, 6};
    stepped->anchors()->left = {steppedLabel, Edge::Left, 0};
    layout->addWidget(stepped);

    auto* twentyFourLabel = new Label(QStringLiteral("24 hour"), window);
    twentyFourLabel->setFluentTypography(Typography::FontRole::Body);
    twentyFourLabel->anchors()->top = {simpleLabel, Edge::Top, 0};
    twentyFourLabel->anchors()->left = {simple, Edge::Right, 48};
    layout->addWidget(twentyFourLabel);

    auto* twentyFour = new TimePicker(window);
    twentyFour->setFieldTextAlignment(TimePicker::TimeField::Hour, Qt::AlignCenter);
    twentyFour->setClockIdentifier(TimePicker::ClockIdentifier::TwentyFourHourClock);
    twentyFour->setSelectedTime(QTime(23, 5));
    twentyFour->anchors()->top = {twentyFourLabel, Edge::Bottom, 6};
    twentyFour->anchors()->left = {twentyFourLabel, Edge::Left, 0};
    layout->addWidget(twentyFour);

    auto* disabledLabel = new Label(QStringLiteral("Disabled"), window);
    disabledLabel->setFluentTypography(Typography::FontRole::Body);
    disabledLabel->anchors()->top = {twentyFour, Edge::Bottom, 28};
    disabledLabel->anchors()->left = {twentyFour, Edge::Left, 0};
    layout->addWidget(disabledLabel);

    auto* disabled = new TimePicker(window);
    disabled->setSelectedTime(QTime(6, 15));
    disabled->setEnabled(false);
    disabled->anchors()->top = {disabledLabel, Edge::Bottom, 6};
    disabled->anchors()->left = {disabledLabel, Edge::Left, 0};
    layout->addWidget(disabled);

    auto* clearButton = new Button(QStringLiteral("Clear"), window);
    clearButton->setFixedSize(96, 32);
    clearButton->anchors()->top = {stepped, Edge::Bottom, 28};
    clearButton->anchors()->left = {stepped, Edge::Left, 0};
    layout->addWidget(clearButton);
    QObject::connect(clearButton, &Button::clicked, simple, &TimePicker::clearSelectedTime);

    auto* openButton = new Button(QStringLiteral("Open"), window);
    openButton->setFixedSize(96, 32);
    openButton->anchors()->top = {clearButton, Edge::Top, 0};
    openButton->anchors()->left = {clearButton, Edge::Right, 12};
    layout->addWidget(openButton);
    QObject::connect(openButton, &Button::clicked, simple, &TimePicker::openPicker);

    auto* themeButton = new Button(QStringLiteral("Dark"), window);
    themeButton->setFluentStyle(Button::Accent);
    themeButton->setIconGlyph(Typography::Icons::Color, 16);
    themeButton->setFluentLayout(Button::IconBefore);
    themeButton->setFixedSize(112, 32);
    themeButton->anchors()->top = {title, Edge::Top, 4};
    themeButton->anchors()->right = {title, Edge::Right, 0};
    layout->addWidget(themeButton);
    QObject::connect(themeButton, &Button::clicked, themeButton, [themeButton]() {
        const bool dark = fluent::FluentElement::currentTheme() == fluent::FluentElement::Dark;
        fluent::FluentElement::setTheme(dark ? fluent::FluentElement::Light : fluent::FluentElement::Dark);
        themeButton->setText(dark ? QStringLiteral("Dark") : QStringLiteral("Light"));
    });

    window->onThemeUpdated();
    window->show();
    qApp->exec();
}

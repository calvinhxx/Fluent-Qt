#include "compatibility/EventConstructionCompat.h"
#include "compatibility/private/WidgetCursorCompat_p.h"
#include "components/basicinput/ColorPicker.h"
#include "components/collections/SplitView.h"
#include "components/collections/FlipView.h"
#include "components/scrolling/PipsPager.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/scrolling/ImageViewer.h"
#include "components/textfields/NumberBox.h"
#include "components/textfields/PasswordBox.h"
#include "components/textfields/TagInput.h"
#include "components/windowing/WindowChromeFrame.h"

#include <QAbstractButton>
#include <QApplication>
#include <functional>
#include <QPixmap>
#include <QTest>
#include <gtest/gtest.h>

namespace {

using compatibility::detail::ScopedWidgetCursor;

class CursorObserver final : public QWidget {
public:
    using QWidget::QWidget;
    int writes = 0;
    std::function<void()> onChange;

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::CursorChange) {
            ++writes;
            if (auto change = std::exchange(onChange, {}))
                change();
        }
        return QWidget::event(event);
    }
};

void sendMouse(QWidget& widget, QEvent::Type type, QPoint local, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    FLUENT_MAKE_MOUSE_EVENT(event, type, &widget, local, button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
}

TEST(WidgetCursorTest, RestoresExplicitAndInheritedStateAndSkipsRepeatedWrites)
{
    QWidget parent;
    CursorObserver target(&parent);
    ScopedWidgetCursor feedback(&target);
    parent.setCursor(Qt::CrossCursor);
    feedback.apply(Qt::OpenHandCursor);
    const int writes = target.writes;
    for (int i = 0; i < 200; ++i)
        feedback.apply(Qt::OpenHandCursor);
    EXPECT_EQ(target.writes, writes);
    parent.setCursor(Qt::PointingHandCursor);
    feedback.apply(Qt::ClosedHandCursor);
    feedback.clear();
    EXPECT_FALSE(target.testAttribute(Qt::WA_SetCursor));
    EXPECT_EQ(target.cursor().shape(), Qt::PointingHandCursor);
    QPixmap pixels(19, 19);
    pixels.fill(Qt::red);
    target.setCursor(QCursor(pixels, 2, 3));
    const auto key = target.cursor().pixmap().cacheKey();
    feedback.apply(Qt::SizeHorCursor);
    feedback.clear();
    EXPECT_EQ(target.cursor().hotSpot(), QPoint(2, 3));
    EXPECT_EQ(target.cursor().pixmap().cacheKey(), key);
    const int done = target.writes;
    feedback.clear();
    EXPECT_EQ(target.writes, done);
}

TEST(WidgetCursorTest, ExternalChangesAndDestructionDuringDeliveryWin)
{
    QWidget parent;
    CursorObserver target(&parent);
    auto feedback = std::make_unique<ScopedWidgetCursor>(&target);
    target.onChange = [&] { target.setCursor(Qt::CrossCursor); };
    feedback->apply(Qt::OpenHandCursor);
    feedback->apply(Qt::ClosedHandCursor);
    feedback->clear();
    EXPECT_EQ(target.cursor().shape(), Qt::CrossCursor);
    feedback->apply(Qt::OpenHandCursor);
    target.setCursor(Qt::OpenHandCursor);
    feedback->clear();
    EXPECT_EQ(target.cursor().shape(), Qt::OpenHandCursor);
    feedback->apply(Qt::ClosedHandCursor);
    target.unsetCursor();
    feedback->clear();
    EXPECT_FALSE(target.testAttribute(Qt::WA_SetCursor));
    target.onChange = [&] { feedback.reset(); };
    feedback->apply(Qt::BlankCursor);
    EXPECT_FALSE(target.testAttribute(Qt::WA_SetCursor));
    auto receiver = std::make_unique<QWidget>();
    ScopedWidgetCursor expires(receiver.get());
    expires.apply(Qt::OpenHandCursor);
    receiver.reset();
    expires.clear();
}

TEST(WidgetCursorTest, EmbeddedFieldCommandsAndScrubbingKeepSeparateCursors)
{
    using namespace fluent::textfields;
    NumberBox number;
    PasswordBox password;
    TagInput tags;
    tags.addItem("one", "One");
    for (QWidget* field : QList<QWidget*>{&number, &password, &tags})
        EXPECT_EQ(field->cursor().shape(), Qt::IBeamCursor);
    auto checkButton = [](QWidget& field, const char* name) {
        auto* button = field.findChild<QAbstractButton*>(QString::fromLatin1(name));
        ASSERT_NE(button, nullptr) << name;
        EXPECT_TRUE(button->testAttribute(Qt::WA_SetCursor)) << name;
        EXPECT_EQ(button->cursor().shape(), Qt::ArrowCursor) << name;
    };
    checkButton(number, "fluentLineEditClearButton");
    checkButton(password, "PasswordBoxRevealButton");
    checkButton(tags, "AutoSuggestBoxQueryButton");
    checkButton(tags, "AutoSuggestBoxClearButton");
    checkButton(tags, "TagInputRemoveButton");

    number.setRange(0, 10);
    number.setValue(9);
    number.resize(220, number.sizeHint().height());
    for (auto mode : {NumberBox::SpinButtonPlacementMode::Inline, NumberBox::SpinButtonPlacementMode::Compact}) {
        number.setSpinButtonPlacementMode(mode);
        checkButton(number, "NumberBoxSpinDownButton");
        checkButton(number, "NumberBoxSpinUpButton");
        auto* up = number.findChild<QAbstractButton*>("NumberBoxSpinUpButton");
        ASSERT_NE(up, nullptr);
        EXPECT_EQ(number.childAt(up->geometry().center()), up);
        up->click();
        EXPECT_EQ(number.value(), 10.0);
        EXPECT_FALSE(up->isEnabled());
        const QPoint buttonPoint = up->geometry().center();
        FLUENT_MAKE_HOVER_EVENT(hover, QEvent::HoverMove, &number, buttonPoint, QPoint(30, 12));
        QApplication::sendEvent(&number, &hover);
        EXPECT_EQ(number.cursor().shape(), Qt::ArrowCursor);
        number.cancelScrub();
        number.setValue(10);
        EXPECT_EQ(number.cursor().shape(), Qt::ArrowCursor);
        sendMouse(number, QEvent::MouseMove, QPoint(40, 12), Qt::NoButton, Qt::NoButton);
        EXPECT_EQ(number.cursor().shape(), Qt::IBeamCursor);
        number.setValue(9);
    }
    number.setCursor(Qt::CrossCursor);
    number.setScrubbingEnabled(true);
    number.setScrubbingEnabled(false);
    EXPECT_EQ(number.cursor().shape(), Qt::CrossCursor);
    number.setScrubbingEnabled(true);
    sendMouse(number, QEvent::MouseMove, number.scrubHandleRect().center(), Qt::NoButton, Qt::NoButton);
    ASSERT_EQ(number.cursor().shape(), Qt::SizeHorCursor);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&number, &leave);
    EXPECT_EQ(number.cursor().shape(), Qt::CrossCursor);
    sendMouse(number, QEvent::MouseMove, number.scrubHandleRect().center(), Qt::NoButton, Qt::NoButton);
    QLineEdit native;
    native.setCursor(Qt::CrossCursor);
    native.setReadOnly(true);
    number.setReadOnly(true);
    EXPECT_EQ(number.cursor().shape(), native.cursor().shape());
}

TEST(WidgetCursorTest, OnlyAnAcceptedPressMovesTheDialog)
{
    QWidget host;
    fluent::dialogs_flyouts::Dialog dialog(&host);
    dialog.setDragEnabled(false);
    dialog.setCursor(Qt::ClosedHandCursor);
    const QPoint start = dialog.pos();
    sendMouse(dialog, QEvent::MouseMove, QPoint(60, 40), Qt::NoButton, Qt::LeftButton);
    EXPECT_EQ(dialog.pos(), start);
    sendMouse(dialog, QEvent::MouseButtonRelease, QPoint(60, 40), Qt::LeftButton, Qt::NoButton);
    EXPECT_EQ(dialog.cursor().shape(), Qt::ClosedHandCursor);

    dialog.setDragEnabled(true);
    dialog.setCursor(Qt::CrossCursor);
    sendMouse(dialog, QEvent::MouseButtonPress, QPoint(4, 4), Qt::LeftButton, Qt::LeftButton);
    EXPECT_EQ(dialog.cursor().shape(), Qt::ClosedHandCursor);
    sendMouse(dialog, QEvent::MouseMove, QPoint(60, 40), Qt::NoButton, Qt::LeftButton);
    EXPECT_NE(dialog.pos(), start);
    QEvent ungrab(QEvent::UngrabMouse);
    QApplication::sendEvent(&dialog, &ungrab);
    EXPECT_EQ(dialog.cursor().shape(), Qt::CrossCursor);
    const QPoint end = dialog.pos();
    sendMouse(dialog, QEvent::MouseMove, QPoint(100, 60), Qt::NoButton, Qt::LeftButton);
    EXPECT_EQ(dialog.pos(), end);
}

TEST(WidgetCursorTest, InterruptedColorPickRestoresTheCallerCursor)
{
    fluent::basicinput::ColorPicker picker;
    for (const char* name : {"ColorPicker.Spectrum", "ColorPicker.HueBar"}) {
        auto* surface = picker.findChild<QWidget*>(QString::fromLatin1(name));
        ASSERT_NE(surface, nullptr);
        surface->setCursor(Qt::CrossCursor);
        sendMouse(*surface, QEvent::MouseButtonPress, QPoint(4, 4), Qt::LeftButton, Qt::LeftButton);
        ASSERT_EQ(surface->cursor().shape(), Qt::BlankCursor);
        QEvent lost(QEvent::UngrabMouse);
        QApplication::sendEvent(surface, &lost);
        EXPECT_EQ(surface->cursor().shape(), Qt::CrossCursor);
    }
}

TEST(WidgetCursorTest, IdleSurfacesPreserveCallerCursorAcrossLayoutAndImageChanges)
{
    fluent::collections::SplitView split;
    QWidget first, second;
    split.addPane(&first);
    split.addPane(&second);
    split.setCursor(Qt::CrossCursor);
    split.setHandleWidth(split.handleWidth() + 1);
    EXPECT_EQ(split.cursor().shape(), Qt::CrossCursor);

    fluent::scrolling::ImageViewer image;
    image.setCursor(Qt::CrossCursor);
    image.setImage(QImage(1, 1, QImage::Format_RGB32));
    EXPECT_EQ(image.cursor().shape(), Qt::CrossCursor);
}

TEST(WidgetCursorTest, NavigationRefreshesUnderAStationaryPointer)
{
    fluent::collections::FlipView flip;
    flip.resize(240, 160);
    flip.addPage(new QWidget);
    flip.addPage(new QWidget);
    flip.setCursor(Qt::CrossCursor);
    FLUENT_MAKE_ENTER_EVENT(enter, 0, 0);
    QApplication::sendEvent(&flip, &enter);
    sendMouse(flip, QEvent::MouseMove, QPoint(flip.width() - 10, flip.height() / 2), Qt::NoButton, Qt::NoButton);
    ASSERT_EQ(flip.cursor().shape(), Qt::PointingHandCursor);
    flip.setCurrentIndex(1);
    EXPECT_EQ(flip.cursor().shape(), Qt::CrossCursor);

    fluent::scrolling::PipsPager pager;
    pager.setCursor(Qt::CrossCursor);
    pager.setNumberOfPages(3);
    pager.resize(pager.sizeHint());
    QApplication::sendEvent(&pager, &enter);
    sendMouse(pager, QEvent::MouseMove, pager.pipHitRect(1).center(), Qt::NoButton, Qt::NoButton);
    ASSERT_EQ(pager.cursor().shape(), Qt::PointingHandCursor);
    pager.setNumberOfPages(0);
    EXPECT_EQ(pager.cursor().shape(), Qt::CrossCursor);
}

TEST(WidgetCursorTest, WindowEdgesShareHoverAndDragAndRestoreTheHost)
{
    QWidget host;
    host.setCursor(Qt::CrossCursor);
    fluent::windowing::ClientSideFrameEdgeOverlay frame(&host);
    frame.setResizeInputEnabled(true, 4);
    auto* top = host.findChild<QWidget*>("fluentWindowResizeHitZone0");
    ASSERT_NE(top, nullptr);
    FLUENT_MAKE_HOVER_EVENT(enter, QEvent::HoverEnter, top, QPoint(1, 1), QPoint(-1, -1));
    QApplication::sendEvent(top, &enter);
    EXPECT_EQ(host.cursor().shape(), Qt::SizeVerCursor);
    frame.setResizeGesture(Qt::SizeFDiagCursor);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(top, &leave);
    EXPECT_EQ(host.cursor().shape(), Qt::SizeFDiagCursor);
    frame.setResizeGesture(std::nullopt);
    EXPECT_EQ(host.cursor().shape(), Qt::CrossCursor);
}

} // namespace

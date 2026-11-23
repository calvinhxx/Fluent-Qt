#include "compatibility/private/WidgetCursorCompat_p.h"

#include <functional>
#include <QPixmap>
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

} // namespace

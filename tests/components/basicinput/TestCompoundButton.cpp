#include <gtest/gtest.h>

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>

#include "components/basicinput/Button.h"
#include "components/basicinput/CompoundButton.h"
#include "design/Typography.h"

using fluent::basicinput::Button;
using fluent::basicinput::CompoundButton;

namespace {

class CompoundButtonProbe final : public CompoundButton {
public:
    using CompoundButton::CompoundButton;

    QRectF exposedContentRect(const QRectF& surface) const
    {
        return contentPaintRect(surface);
    }
};

} // namespace

TEST(CompoundButtonTest, Contract_InheritsButtonInteractionSurface)
{
    static_assert(std::is_base_of<Button, CompoundButton>::value,
                  "CompoundButton must reuse Button interaction and painting");

    CompoundButton button(
        QStringLiteral("Install"),
        QStringLiteral("Download and restart"));
    EXPECT_EQ(button.text(), QStringLiteral("Install"));
    EXPECT_EQ(button.secondaryText(),
              QStringLiteral("Download and restart"));
    EXPECT_EQ(button.fluentSize(), Button::Large);
    EXPECT_EQ(button.accessibleDescription(),
              QStringLiteral("Download and restart"));
}

TEST(CompoundButtonTest, Contract_TextAndParentConstructorMatchesButton)
{
    QWidget parent;
    CompoundButton button(QStringLiteral("Save"), &parent);
    EXPECT_EQ(button.text(), QStringLiteral("Save"));
    EXPECT_EQ(button.parentWidget(), &parent);
    EXPECT_TRUE(button.secondaryText().isEmpty());
}

TEST(CompoundButtonTest, Contract_SecondaryTextSetterIsRepeatSafe)
{
    CompoundButton button;
    QSignalSpy secondarySpy(
        &button, &CompoundButton::secondaryTextChanged);
    button.setSecondaryText(QStringLiteral("Secondary"));
    button.setSecondaryText(QStringLiteral("Secondary"));
    EXPECT_EQ(secondarySpy.count(), 1);
    EXPECT_EQ(button.accessibleDescription(),
              QStringLiteral("Secondary"));

    button.setAccessibleDescription(QStringLiteral("Custom description"));
    button.setSecondaryText(QStringLiteral("Updated"));
    EXPECT_EQ(button.accessibleDescription(),
              QStringLiteral("Custom description"));
}

TEST(CompoundButtonTest, Contract_SecondaryLineChangesMeasurementAndContentRegion)
{
    CompoundButtonProbe button(QStringLiteral("Primary"));
    const QSize singleLineSize = button.sizeHint();
    const QRectF surface(0, 0, 220, 52);
    EXPECT_EQ(button.exposedContentRect(surface), surface);

    button.setSecondaryText(
        QStringLiteral("A longer secondary description"));
    const QSize compoundSize = button.sizeHint();
    EXPECT_GT(compoundSize.height(), singleLineSize.height());
    EXPECT_GE(compoundSize.width(), singleLineSize.width());
    EXPECT_LT(button.exposedContentRect(surface).height(),
              surface.height());
}

TEST(CompoundButtonTest, Contract_ClickAndIconBehaviorRemainButtonCompatible)
{
    CompoundButton button(
        QStringLiteral("Share"),
        QStringLiteral("Send a copy to collaborators"));
    button.setFluentLayout(Button::IconBefore);
    button.setIconGlyph(Typography::Icons::Share);
    button.resize(button.sizeHint());
    button.show();
    QApplication::processEvents();

    QSignalSpy clickSpy(&button, &QPushButton::clicked);
    QTest::mouseClick(&button, Qt::LeftButton);
    EXPECT_EQ(clickSpy.count(), 1);

    QImage rendered(button.size(), QImage::Format_ARGB32_Premultiplied);
    rendered.fill(Qt::transparent);
    QPainter painter(&rendered);
    button.render(&painter);
    painter.end();
    EXPECT_GT(qAlpha(rendered.pixel(rendered.rect().center())), 0);
}

#include <gtest/gtest.h>

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>

#include "compatibility/QtCompat.h"
#include "components/status_info/Avatar.h"
#include "components/status_info/InfoBadge.h"

using fluent::status_info::Avatar;
using fluent::status_info::InfoBadge;

class AvatarTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<Avatar::AvatarShape>("AvatarShape");
        qRegisterMetaType<Avatar::AvatarSize>("AvatarSize");
        qRegisterMetaType<Avatar::PresenceStatus>("PresenceStatus");
    }
};

TEST_F(AvatarTest, Contract_DefaultsAndGeneratedInitials)
{
    static_assert(std::is_base_of<QWidget, Avatar>::value,
                  "Avatar remains a QWidget");
    static_assert(std::is_base_of<fluent::FluentElement, Avatar>::value,
                  "Avatar participates in Fluent theming");

    Avatar avatar;
    EXPECT_TRUE(avatar.name().isEmpty());
    EXPECT_TRUE(avatar.initials().isEmpty());
    EXPECT_TRUE(avatar.image().isNull());
    EXPECT_EQ(avatar.shape(), Avatar::AvatarShape::Circular);
    EXPECT_EQ(avatar.avatarSize(), Avatar::AvatarSize::Medium);
    EXPECT_EQ(avatar.presence(), Avatar::PresenceStatus::None);
    EXPECT_EQ(avatar.size(), QSize(32, 32));
    EXPECT_TRUE(avatar.effectiveInitials().isEmpty());

    avatar.setName(QStringLiteral("Ada Lovelace"));
    EXPECT_EQ(avatar.effectiveInitials(), QStringLiteral("AL"));
    EXPECT_EQ(avatar.accessibleName(), QStringLiteral("Ada Lovelace"));

    avatar.setLayoutDirection(Qt::RightToLeft);
    EXPECT_EQ(avatar.effectiveInitials(), QStringLiteral("LA"));
}

TEST_F(AvatarTest, Contract_ExplicitInitialsAndAccessibleNameRemainCallerControlled)
{
    Avatar avatar;
    avatar.setAccessibleName(QStringLiteral("Profile owner"));
    QSignalSpy nameSpy(&avatar, &Avatar::nameChanged);
    QSignalSpy initialsSpy(&avatar, &Avatar::initialsChanged);

    avatar.setName(QStringLiteral("Grace Hopper"));
    avatar.setName(QStringLiteral("Grace Hopper"));
    EXPECT_EQ(nameSpy.count(), 1);
    EXPECT_EQ(avatar.accessibleName(), QStringLiteral("Profile owner"));

    avatar.setInitials(QStringLiteral(" GH "));
    avatar.setInitials(QStringLiteral("GH"));
    EXPECT_EQ(initialsSpy.count(), 1);
    EXPECT_EQ(avatar.initials(), QStringLiteral("GH"));
    EXPECT_EQ(avatar.effectiveInitials(), QStringLiteral("GH"));
}

TEST_F(AvatarTest, Contract_InitialsPreserveUnicodeGraphemeClusters)
{
    Avatar avatar;
    avatar.setInitials(QStringLiteral("A\u0301B"));
    EXPECT_EQ(avatar.effectiveInitials(), QStringLiteral("A\u0301B"));

    avatar.setInitials(QString());
    avatar.setName(QStringLiteral("\U0001F600 Alpha"));
    EXPECT_EQ(avatar.effectiveInitials(),
              QStringLiteral("\U0001F600A"));
}

TEST_F(AvatarTest, Contract_SizeShapeAndPresenceUseInfoBadge)
{
    Avatar avatar(QStringLiteral("Lin Chen"));
    QSignalSpy sizeSpy(&avatar, &Avatar::avatarSizeChanged);
    QSignalSpy shapeSpy(&avatar, &Avatar::shapeChanged);
    QSignalSpy presenceSpy(&avatar, &Avatar::presenceChanged);

    avatar.setAvatarSize(Avatar::AvatarSize::ExtraLarge);
    avatar.setAvatarSize(Avatar::AvatarSize::ExtraLarge);
    EXPECT_EQ(sizeSpy.count(), 1);
    EXPECT_EQ(
        sizeSpy.at(0).at(0).value<Avatar::AvatarSize>(),
        Avatar::AvatarSize::ExtraLarge);
    EXPECT_EQ(avatar.size(), QSize(56, 56));

    avatar.setShape(Avatar::AvatarShape::Square);
    avatar.setShape(Avatar::AvatarShape::Square);
    EXPECT_EQ(shapeSpy.count(), 1);
    EXPECT_EQ(
        shapeSpy.at(0).at(0).value<Avatar::AvatarShape>(),
        Avatar::AvatarShape::Square);

    ASSERT_NE(avatar.presenceBadge(), nullptr);
    EXPECT_FALSE(avatar.presenceBadge()->isVisibleTo(&avatar));
    avatar.setPresence(Avatar::PresenceStatus::Available);
    avatar.show();
    QApplication::processEvents();
    EXPECT_EQ(presenceSpy.count(), 1);
    EXPECT_EQ(
        presenceSpy.at(0).at(0).value<Avatar::PresenceStatus>(),
        Avatar::PresenceStatus::Available);
    EXPECT_TRUE(avatar.presenceBadge()->isVisibleTo(&avatar));
    EXPECT_EQ(avatar.presenceBadge()->status(),
              InfoBadge::InfoBadgeStatus::Success);

    avatar.setPresence(Avatar::PresenceStatus::Offline);
    EXPECT_EQ(avatar.presenceBadge()->status(),
              InfoBadge::InfoBadgeStatus::Attention);
    EXPECT_TRUE(
        avatar.presenceBadge()->customBackgroundColor().isValid());
}

TEST_F(AvatarTest, Contract_HighDpiPixmapIsRetainedAndRenders)
{
    QPixmap pixmap(80, 80);
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(QColor(22, 120, 210));

    Avatar avatar;
    QSignalSpy imageSpy(&avatar, &Avatar::imageChanged);
    avatar.setImage(pixmap);
    avatar.setImage(pixmap);
    EXPECT_EQ(imageSpy.count(), 1);
    EXPECT_EQ(avatar.image().cacheKey(), pixmap.cacheKey());
    EXPECT_DOUBLE_EQ(avatar.image().devicePixelRatioF(), 2.0);

    QImage rendered(avatar.size(), QImage::Format_ARGB32_Premultiplied);
    rendered.fill(Qt::transparent);
    QPainter painter(&rendered);
    avatar.render(&painter);
    painter.end();
    EXPECT_GT(qAlpha(rendered.pixel(rendered.rect().center())), 0);
}

TEST_F(AvatarTest, Contract_HighDpiCoverCropKeepsCenterBand)
{
    // Fractional logical dimensions exercise exact cover math: 100x50 at DPR=3
    // is 33.333x16.667 logical pixels. Rounding before crop calculation would
    // include the red/blue guard pixels at the destination edges.
    // zh_CN: 通过 DPR=3 下不可整除的逻辑尺寸验证精确 cover 裁切；若提前取整，
    // 目标边缘会错误采样红/蓝保护像素。
    constexpr int physicalWidth = 100;
    constexpr int physicalHeight = 50;
    QImage source(physicalWidth, physicalHeight, QImage::Format_ARGB32);
    for (int y = 0; y < physicalHeight; ++y) {
        for (int x = 0; x < physicalWidth; ++x) {
            QColor color(40, 200, 80);
            if (x < 25)
                color = QColor(220, 40, 40); // left band
            else if (x >= 75)
                color = QColor(40, 40, 220); // right band
            source.setPixelColor(x, y, color);
        }
    }

    QPixmap pixmap = QPixmap::fromImage(source);
    pixmap.setDevicePixelRatio(3.0);

    QImage exactCover(QSize(60, 60), QImage::Format_ARGB32_Premultiplied);
    exactCover.fill(Qt::transparent);
    {
        QPainter painter(&exactCover);
        fluentDrawCoverPixmapInLogicalRect(
            painter, QRectF(0, 0, 60, 60), pixmap);
    }
    for (const QPoint sample : {
             QPoint(0, 30),
             QPoint(30, 30),
             QPoint(59, 30)}) {
        const QColor color = exactCover.pixelColor(sample);
        EXPECT_GT(color.green(), color.red() + 40);
        EXPECT_GT(color.green(), color.blue() + 40);
    }

    Avatar avatar;
    avatar.setAvatarSize(Avatar::AvatarSize::ExtraLarge); // 56x56
    avatar.setImage(pixmap);

    QImage rendered(avatar.size(), QImage::Format_ARGB32_Premultiplied);
    rendered.fill(Qt::transparent);
    {
        QPainter painter(&rendered);
        avatar.render(&painter);
    }

    const QPoint center = rendered.rect().center();
    const QColor centerColor = QColor::fromRgba(rendered.pixel(center));
    EXPECT_GT(centerColor.alpha(), 200);
    EXPECT_GT(centerColor.green(), centerColor.red() + 40);
    EXPECT_GT(centerColor.green(), centerColor.blue() + 40);
}

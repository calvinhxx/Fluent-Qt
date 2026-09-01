#include <gtest/gtest.h>

#include <QAction>
#include <QHash>
#include <QWidget>

#include "components/menus_toolbars/private/CommandActionModel_p.h"
#include "components/menus_toolbars/private/CommandOverflowProjection_p.h"

namespace detail = fluent::menus_toolbars::detail;

TEST(CommandOverflowProjectionTest, Contract_FiltersInvisibleActionsAndNormalizesSeparators)
{
    QWidget owner;
    detail::CommandActionModel model(&owner);
    QAction leadingSeparator;
    leadingSeparator.setSeparator(true);
    QAction first(QStringLiteral("First"));
    QAction firstSeparator;
    firstSeparator.setSeparator(true);
    QAction duplicateSeparator;
    duplicateSeparator.setSeparator(true);
    QAction hidden(QStringLiteral("Hidden"));
    hidden.setVisible(false);
    QAction second(QStringLiteral("Second"));
    QAction trailingSeparator;
    trailingSeparator.setSeparator(true);

    for (QAction* action : {&leadingSeparator, &first, &firstSeparator, &duplicateSeparator,
                            &hidden, &second, &trailingSeparator}) {
        ASSERT_TRUE(model.add(detail::CommandActionModel::Section::Primary, action));
    }

    const QList<QAction*> visible =
        detail::visiblePresentableActions(model, detail::CommandActionModel::Section::Primary);
    EXPECT_FALSE(visible.contains(&hidden));
    EXPECT_EQ(detail::normalizedProjection(visible, detail::nonSeparatorSet(visible)),
              (QList<QAction*>{&first, &firstSeparator, &second}));
}

TEST(CommandOverflowProjectionTest, Contract_PriorityTailAndStableSourceOrderAtNarrowWidths)
{
    QAction normal(QStringLiteral("Normal"));
    QAction lowFirst(QStringLiteral("Low first"));
    QAction lowTail(QStringLiteral("Low tail"));
    QAction high(QStringLiteral("High"));
    lowFirst.setPriority(QAction::LowPriority);
    lowTail.setPriority(QAction::LowPriority);
    high.setPriority(QAction::HighPriority);
    const QList<QAction*> source{&normal, &lowFirst, &lowTail, &high};
    const QHash<QAction*, int> widths{
        {&normal, 70},
        {&lowFirst, 60},
        {&lowTail, 50},
        {&high, 80},
    };
    const auto widthResolver = [&widths](QAction* action) { return widths.value(action); };

    detail::CommandOverflowProjectionOptions options;
    options.availableWidth = 263;
    options.moreButtonWidth = 40;
    options.itemSpacing = 4;
    const detail::CommandOverflowProjection firstOverflow =
        detail::projectCommandOverflow(source, options, widthResolver);
    EXPECT_EQ(firstOverflow.inlineActions, (QList<QAction*>{&normal, &lowFirst, &high}));
    EXPECT_EQ(firstOverflow.overflowActions, (QList<QAction*>{&lowTail}));

    options.availableWidth = 230;
    const detail::CommandOverflowProjection narrower =
        detail::projectCommandOverflow(source, options, widthResolver);
    EXPECT_EQ(narrower.inlineActions, (QList<QAction*>{&normal, &high}));
    EXPECT_EQ(narrower.overflowActions, (QList<QAction*>{&lowFirst, &lowTail}));

    options.availableWidth = 0;
    const detail::CommandOverflowProjection narrowest =
        detail::projectCommandOverflow(source, options, widthResolver);
    EXPECT_TRUE(narrowest.inlineActions.isEmpty());
    EXPECT_EQ(narrowest.overflowActions, source);
}

TEST(CommandOverflowProjectionTest, Contract_MoreReservationParticipatesInTheWidthBoundary)
{
    QAction first(QStringLiteral("First"));
    QAction tail(QStringLiteral("Tail"));
    const QList<QAction*> source{&first, &tail};
    const auto widthResolver = [](QAction* action) {
        return action ? (action->text() == QStringLiteral("First") ? 80 : 70) : 0;
    };

    detail::CommandOverflowProjectionOptions options;
    options.availableWidth = 120;
    options.moreButtonWidth = 40;
    options.itemSpacing = 4;
    options.includeMoreWhenOverflowing = false;
    const detail::CommandOverflowProjection withoutReservedMore =
        detail::projectCommandOverflow(source, options, widthResolver);
    EXPECT_EQ(withoutReservedMore.inlineActions, (QList<QAction*>{&first}));
    EXPECT_EQ(withoutReservedMore.overflowActions, (QList<QAction*>{&tail}));

    options.includeMoreWhenOverflowing = true;
    const detail::CommandOverflowProjection withReservedMore =
        detail::projectCommandOverflow(source, options, widthResolver);
    EXPECT_TRUE(withReservedMore.inlineActions.isEmpty());
    EXPECT_EQ(withReservedMore.overflowActions, source);
}

TEST(CommandOverflowProjectionTest, Contract_EnablementAndInitialMoreReservationStayIndependent)
{
    QAction action(QStringLiteral("Action"));
    const QList<QAction*> source{&action};
    const auto widthResolver = [](QAction*) { return 80; };

    detail::CommandOverflowProjectionOptions options;
    options.availableWidth = 80;
    options.moreButtonWidth = 40;
    options.itemSpacing = 4;
    options.overflowEnabled = false;
    options.includeMoreBeforeOverflow = true;
    const detail::CommandOverflowProjection disabled =
        detail::projectCommandOverflow(source, options, widthResolver);
    EXPECT_EQ(disabled.inlineActions, source);
    EXPECT_TRUE(disabled.overflowActions.isEmpty());

    options.overflowEnabled = true;
    options.includeMoreBeforeOverflow = false;
    const detail::CommandOverflowProjection withoutInitialMore =
        detail::projectCommandOverflow(source, options, widthResolver);
    EXPECT_EQ(withoutInitialMore.inlineActions, source);
    EXPECT_TRUE(withoutInitialMore.overflowActions.isEmpty());

    options.includeMoreBeforeOverflow = true;
    const detail::CommandOverflowProjection withInitialMore =
        detail::projectCommandOverflow(source, options, widthResolver);
    EXPECT_TRUE(withInitialMore.inlineActions.isEmpty());
    EXPECT_EQ(withInitialMore.overflowActions, source);
}

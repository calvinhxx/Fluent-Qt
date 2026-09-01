#include "CommandOverflowProjection_p.h"

#include <QAction>
#include <QVector>

#include <algorithm>

namespace fluent::menus_toolbars::detail {
namespace {

int overflowPriorityRank(QAction::Priority priority)
{
    switch (priority) {
    case QAction::LowPriority:
        return 0;
    case QAction::NormalPriority:
        return 1;
    case QAction::HighPriority:
        return 2;
    }
    return 1;
}

struct OverflowCandidate {
    QAction* action = nullptr;
    int logicalIndex = -1;
    int priorityRank = 1;
};

} // namespace

QList<QAction*> visiblePresentableActions(const CommandActionModel& model,
                                          CommandActionModel::Section section)
{
    QList<QAction*> result;
    const QList<QAction*> registered = model.actions(section);
    result.reserve(registered.size());
    for (QAction* action : registered) {
        if (action && action->isVisible() && model.isPresentable(action)) {
            result.append(action);
        }
    }
    return result;
}

QSet<QAction*> nonSeparatorSet(const QList<QAction*>& actions)
{
    QSet<QAction*> result;
    for (QAction* action : actions) {
        if (action && !action->isSeparator())
            result.insert(action);
    }
    return result;
}

QList<QAction*> normalizedProjection(const QList<QAction*>& source,
                                     const QSet<QAction*>& includedCommands)
{
    QList<QAction*> result;
    QAction* pendingSeparator = nullptr;
    for (QAction* action : source) {
        if (!action)
            continue;
        if (action->isSeparator()) {
            if (!result.isEmpty() && !pendingSeparator)
                pendingSeparator = action;
            continue;
        }
        if (!includedCommands.contains(action))
            continue;
        if (pendingSeparator) {
            result.append(pendingSeparator);
            pendingSeparator = nullptr;
        }
        result.append(action);
    }
    return result;
}

bool hasCommandRows(const QList<QAction*>& actions)
{
    for (QAction* action : actions) {
        if (action && !action->isSeparator())
            return true;
    }
    return false;
}

int projectedCommandWidth(const QList<QAction*>& projection, bool includeMore, int moreButtonWidth,
                          int itemSpacing, const CommandWidthResolver& widthResolver)
{
    int width = 0;
    int itemCount = 0;
    for (QAction* action : projection) {
        const int itemWidth = widthResolver ? widthResolver(action) : 0;
        if (itemWidth <= 0)
            continue;
        width += itemWidth;
        ++itemCount;
    }
    if (includeMore) {
        width += moreButtonWidth;
        ++itemCount;
    }
    if (itemCount > 1)
        width += (itemCount - 1) * itemSpacing;
    return width;
}

CommandOverflowProjection projectCommandOverflow(const QList<QAction*>& source,
                                                 const CommandOverflowProjectionOptions& options,
                                                 const CommandWidthResolver& widthResolver)
{
    CommandOverflowProjection result;
    QSet<QAction*> inlineCommands = nonSeparatorSet(source);
    result.inlineActions = normalizedProjection(source, inlineCommands);
    if (!options.overflowEnabled)
        return result;

    if (projectedCommandWidth(result.inlineActions, options.includeMoreBeforeOverflow,
                              options.moreButtonWidth, options.itemSpacing,
                              widthResolver) <= options.availableWidth) {
        return result;
    }

    QVector<OverflowCandidate> candidates;
    candidates.reserve(inlineCommands.size());
    for (int index = 0; index < source.size(); ++index) {
        QAction* action = source.at(index);
        if (!action || action->isSeparator())
            continue;
        OverflowCandidate candidate;
        candidate.action = action;
        candidate.logicalIndex = index;
        candidate.priorityRank = overflowPriorityRank(action->priority());
        candidates.append(candidate);
    }
    // Move low priority commands first and, within one priority, move the
    // logical tail first. The final projection is rebuilt from source so its
    // surviving and overflowed rows keep stable source order.
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const OverflowCandidate& first, const OverflowCandidate& second) {
                         if (first.priorityRank != second.priorityRank) {
                             return first.priorityRank < second.priorityRank;
                         }
                         return first.logicalIndex > second.logicalIndex;
                     });

    QSet<QAction*> overflowCommands;
    for (const OverflowCandidate& candidate : candidates) {
        inlineCommands.remove(candidate.action);
        overflowCommands.insert(candidate.action);
        result.inlineActions = normalizedProjection(source, inlineCommands);
        if (projectedCommandWidth(result.inlineActions, options.includeMoreWhenOverflowing,
                                  options.moreButtonWidth, options.itemSpacing,
                                  widthResolver) <= options.availableWidth) {
            break;
        }
    }
    result.overflowActions = normalizedProjection(source, overflowCommands);
    return result;
}

} // namespace fluent::menus_toolbars::detail

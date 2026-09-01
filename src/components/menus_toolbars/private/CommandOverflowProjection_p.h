#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDOVERFLOWPROJECTION_P_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDOVERFLOWPROJECTION_P_H

#include <QList>
#include <QSet>

#include <functional>

#include "components/menus_toolbars/private/CommandActionModel_p.h"

class QAction;

namespace fluent::menus_toolbars::detail {

using CommandWidthResolver = std::function<int(QAction*)>;

struct CommandOverflowProjectionOptions {
    int availableWidth = 0;
    int moreButtonWidth = 0;
    int itemSpacing = 0;
    bool overflowEnabled = true;
    // Existing secondary rows and newly overflowed primary rows can reserve
    // More independently. Always-expanded flyouts intentionally reserve it
    // in neither case.
    bool includeMoreBeforeOverflow = false;
    bool includeMoreWhenOverflowing = true;
};

struct CommandOverflowProjection {
    QList<QAction*> inlineActions;
    QList<QAction*> overflowActions;
};

QList<QAction*> visiblePresentableActions(const CommandActionModel& model,
                                          CommandActionModel::Section section);

QSet<QAction*> nonSeparatorSet(const QList<QAction*>& actions);

QList<QAction*> normalizedProjection(const QList<QAction*>& source,
                                     const QSet<QAction*>& includedCommands);

bool hasCommandRows(const QList<QAction*>& actions);

int projectedCommandWidth(const QList<QAction*>& projection, bool includeMore, int moreButtonWidth,
                          int itemSpacing, const CommandWidthResolver& widthResolver);

CommandOverflowProjection projectCommandOverflow(const QList<QAction*>& source,
                                                 const CommandOverflowProjectionOptions& options,
                                                 const CommandWidthResolver& widthResolver);

} // namespace fluent::menus_toolbars::detail

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDOVERFLOWPROJECTION_P_H

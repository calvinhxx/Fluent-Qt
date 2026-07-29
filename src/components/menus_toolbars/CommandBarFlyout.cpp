#include "CommandBarFlyout.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QFrame>
#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QScrollBar>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <algorithm>

#include "compatibility/QtCompat.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayLightDismiss.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/layout/Divider.h"
#include "components/menus_toolbars/private/CommandAccessibility_p.h"
#include "components/menus_toolbars/private/CommandActionModel_p.h"
#include "components/menus_toolbars/private/CommandPresenter_p.h"
#include "components/scrolling/ScrollView.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "utils/private/FluentQtLogging_p.h"

namespace fluent::menus_toolbars {

using detail::CommandAccessibleRole;
using detail::CommandActionModel;
using detail::CommandMoreButton;
using detail::CommandPresenter;

namespace {

constexpr const char* kCommandPresentationProperty =
    "_fluentqt_commandActionPresentation";
constexpr int kCommandTargetExtent = ::Spacing::ControlHeight::Large;
constexpr int kCardInset = ::Spacing::XSmall;
constexpr int kItemSpacing = ::Spacing::Gap::Tight;
constexpr int kSeparatorExtent = ::Spacing::Small + 1;
constexpr int kHostEdgeMargin = ::Spacing::XSmall;
constexpr int kDefaultMenuWidth = 180;

QList<QAction*> visiblePresentableActions(
    const CommandActionModel& model,
    CommandActionModel::Section section)
{
    QList<QAction*> result;
    const QList<QAction*> registered = model.actions(section);
    result.reserve(registered.size());
    for (QAction* action : registered) {
        if (action && action->isVisible()
            && model.isPresentable(action)) {
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

QList<QAction*> normalizedProjection(
    const QList<QAction*>& source,
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

QWidget* owningTopLevel(const CommandBarFlyout* flyout)
{
    QWidget* parent = flyout ? flyout->parentWidget() : nullptr;
    return parent ? parent->window() : nullptr;
}

bool isValidInvocationTarget(const CommandBarFlyout* flyout,
                             QWidget* target,
                             const char* invocation)
{
    QWidget* ownerTopLevel = owningTopLevel(flyout);
    if (!ownerTopLevel) {
        qCWarning(logging::commandBarCategory)
            << invocation
            << "rejected: CommandBarFlyout has no owning window";
        return false;
    }
    if (!target || !target->window()) {
        qCWarning(logging::commandBarCategory)
            << invocation
            << "rejected: target is null or has no window"
            << "target=" << target;
        return false;
    }
    if (target->window() != ownerTopLevel) {
        qCWarning(logging::commandBarCategory)
            << invocation
            << "rejected: target belongs to another top-level window"
            << "ownerWindow=" << ownerTopLevel
            << "targetWindow=" << target->window();
        return false;
    }
    return true;
}

struct OverflowCandidate {
    QAction* action = nullptr;
    int logicalIndex = -1;
    int priorityRank = 1;
};

class CommandBarFlyoutScrollView final
    : public scrolling::ScrollView {
public:
    explicit CommandBarFlyoutScrollView(QWidget* parent)
        : scrolling::ScrollView(parent)
    {
        keepViewportTransparent();
    }

protected:
    void onThemeUpdated() override
    {
        scrolling::ScrollView::onThemeUpdated();
        keepViewportTransparent();
    }

private:
    void keepViewportTransparent()
    {
        if (!viewport())
            return;
        viewport()->setAutoFillBackground(false);
        viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
};

} // namespace

class CommandBarFlyoutPrivate final : public QObject {
public:
    friend class CommandBarFlyout;

    enum class RecomputeReason {
        Closed,
        PreparingOpen,
        OpenStateChange,
    };

    enum class RowKind {
        Command,
        ActionSeparator,
        GroupSeparator,
    };

    enum class FocusArea {
        Primary,
        More,
        Menu,
    };

    enum class CloseFocusDisposition {
        Default,
        Restore,
        Preserve,
    };

    struct RowSpec {
        QPointer<QAction> action;
        RowKind kind = RowKind::Command;
        bool secondary = false;
    };

    struct FocusTarget {
        QPointer<QWidget> widget;
        QPointer<QAction> action;
        FocusArea area = FocusArea::Primary;
    };

    struct FocusSnapshot {
        QPointer<QAction> action;
        int visualIndex = -1;
        FocusArea area = FocusArea::Primary;
        bool inside = false;
    };

    explicit CommandBarFlyoutPrivate(CommandBarFlyout* owner)
        : QObject(nullptr),
          q(owner),
          actions(owner)
    {
        q->setObjectName(QStringLiteral("FluentCommandBarFlyout"));
        q->setProperty(kCommandPresentationProperty, true);
        detail::markCommandAccessibleWidget(
            q, CommandAccessibleRole::PopupRoot);
        q->setAccessibleName(
            QCoreApplication::translate(
                "fluent::menus_toolbars::CommandBarFlyout",
                "Command bar flyout"));
        q->setModal(false);
        q->setDim(false);
        q->setClosePolicy(
            dialogs_flyouts::Popup::ClosePolicy(
                dialogs_flyouts::Popup::CloseOnPressOutside
                | dialogs_flyouts::Popup::CloseOnEscape));
        q->setPlacement(dialogs_flyouts::Flyout::Auto);
        q->setAnchorOffset(::Spacing::Small);
        q->setFont(
            q->themeFont(Typography::FontRole::Body).toQFont());
        q->installEventFilter(this);

        primaryRow = new QWidget(q);
        primaryRow->setObjectName(
            QStringLiteral(
                "FluentCommandBarFlyout.PrimaryRow"));
        primaryRow->setAutoFillBackground(false);
        detail::markCommandAccessibleWidget(
            primaryRow, CommandAccessibleRole::PrimaryRow);

        primaryMenuDivider = new layout::Divider(
            Qt::Horizontal, q);
        primaryMenuDivider->setObjectName(
            QStringLiteral(
                "FluentCommandBarFlyout.PrimaryMenuDivider"));
        primaryMenuDivider->setLeadingInset(::Spacing::Small);
        primaryMenuDivider->setTrailingInset(::Spacing::Small);
        primaryMenuDivider->hide();

        moreButton = new CommandMoreButton(primaryRow);
        moreButton->setObjectName(
            QStringLiteral(
                "FluentCommandBarFlyout.MoreButton"));
        moreButton->setFixedSize(
            kCommandTargetExtent, kCommandTargetExtent);
        moreButton->hide();
        moreButton->installEventFilter(this);
        QObject::connect(
            moreButton,
            &QPushButton::clicked,
            q,
            [this]() {
                if (!q || (!q->isOpen() && !q->isVisible()))
                    return;
                const bool expand = !expanded;
                const bool focusMenu =
                    focusMenuOnNextExpansion;
                focusMenuOnNextExpansion = true;
                if (!setExpandedRequested(
                        expand, focusMenu)) {
                    return;
                }
                if (expand && focusMenu) {
                    enterKeyboardMode();
                    focusFirstMenuTarget();
                }
            });

        scrollView = new CommandBarFlyoutScrollView(q);
        scrollView->setObjectName(
            QStringLiteral(
                "FluentCommandBarFlyout.ScrollView"));
        scrollView->setFrameShape(QFrame::NoFrame);
        scrollView->setHorizontalScrollMode(
            scrolling::ScrollView::ScrollMode::Disabled);
        scrollView->setHorizontalScrollBarVisibility(
            scrolling::ScrollView::ScrollBarVisibility::Disabled);
        scrollView->setVerticalScrollMode(
            scrolling::ScrollView::ScrollMode::Auto);
        scrollView->setVerticalScrollBarVisibility(
            scrolling::ScrollView::ScrollBarVisibility::Auto);
        scrollView->setWidgetResizable(false);
        scrollView->setStyleSheet(
            QStringLiteral(
                "QScrollArea { background: transparent; border: none; }"
                "QScrollArea > QWidget > QWidget {"
                " background: transparent; }"));
        scrollView->viewport()->setAutoFillBackground(false);
        scrollView->hide();

        menuContent = new QWidget();
        menuContent->setObjectName(
            QStringLiteral(
                "FluentCommandBarFlyout.MenuContent"));
        menuContent->setAutoFillBackground(false);
        detail::markCommandAccessibleWidget(
            menuContent, CommandAccessibleRole::MenuList);
        scrollView->setWidget(menuContent);

        QObject::connect(
            &actions,
            &CommandActionModel::structureChanged,
            this,
            [this]() {
                const FocusSnapshot snapshot = captureFocus();
                rebuildPrimaryPresenters();
                recomputePresentation(
                    q && (q->isOpen() || q->isVisible())
                        ? RecomputeReason::OpenStateChange
                        : RecomputeReason::Closed,
                    snapshot);
            });
        QObject::connect(
            &actions,
            &CommandActionModel::presentationChanged,
            this,
            [this]() {
                const FocusSnapshot snapshot = captureFocus();
                refreshPrimaryPresenters();
                recomputePresentation(
                    q && (q->isOpen() || q->isVisible())
                        ? RecomputeReason::OpenStateChange
                        : RecomputeReason::Closed,
                    snapshot);
            });
        QObject::connect(
            q,
            &dialogs_flyouts::Popup::aboutToShow,
            q,
            [this]() { prepareForOpen(); });
        QObject::connect(
            q,
            &dialogs_flyouts::Popup::opened,
            q,
            [this]() { afterOpen(); });
        QObject::connect(
            q,
            &dialogs_flyouts::Popup::aboutToHide,
            q,
            [this]() { beforeClose(); });
        QObject::connect(
            q,
            &dialogs_flyouts::Popup::closed,
            q,
            [this]() { afterClose(); });

        rebuildPrimaryPresenters();
        recomputePresentation(RecomputeReason::Closed);
    }

    ~CommandBarFlyoutPrivate() override
    {
        if (q)
            q->removeEventFilter(this);
        if (moreButton)
            moreButton->removeEventFilter(this);
        for (QWidget* widget : primaryPresenters) {
            if (widget)
                widget->removeEventFilter(this);
        }
        for (QWidget* widget : menuRows) {
            if (widget)
                widget->removeEventFilter(this);
        }
    }

    bool isTransientPointerFocus(
        const QEvent* event) const
    {
        if (!event
            || event->type() != QEvent::FocusIn
            || showMode
                != CommandBarFlyout::ShowMode::Transient) {
            return false;
        }
        return static_cast<const QFocusEvent*>(event)->reason()
            == Qt::MouseFocusReason;
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!event)
            return QObject::eventFilter(watched, event);

        if (watched == q) {
            switch (event->type()) {
            case QEvent::Resize:
                if (!recomputing)
                    layoutPresentation(false);
                break;
            case QEvent::LayoutDirectionChange:
            case QEvent::FontChange:
                if (!recomputing) {
                    refreshPrimaryPresenters();
                    recomputePresentation(
                        q->isOpen() || q->isVisible()
                            ? RecomputeReason::OpenStateChange
                            : RecomputeReason::Closed);
                }
                break;
            case QEvent::FocusIn:
                if (!keyboardInteractive
                    && !isTransientPointerFocus(event)) {
                    enterKeyboardMode();
                }
                if (keyboardInteractive)
                    focusFirstTarget();
                break;
            default:
                break;
            }
            return QObject::eventFilter(watched, event);
        }

        const int targetIndex = focusTargetIndex(watched);
        if (targetIndex < 0)
            return QObject::eventFilter(watched, event);

        if (event->type() == QEvent::FocusIn) {
            rememberFocusTarget(targetIndex);
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const bool transientPointerMore =
                    watched == moreButton
                    && showMode
                        == CommandBarFlyout::ShowMode::Transient
                    && !keyboardInteractive;
                if (transientPointerMore) {
                    focusMenuOnNextExpansion = false;
                } else if (keyboardInteractive
                           || watched == moreButton) {
                    if (!keyboardInteractive)
                        enterKeyboardMode();
                    focusTarget(
                        targetIndex, Qt::MouseFocusReason);
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }

    bool handleApplicationEvent(
        QObject* watched,
        QEvent* event)
    {
        if (!q || !event
            || (!q->isOpen() && !q->isVisible())) {
            return false;
        }

        if (event->type() == QEvent::Resize
            && watched == owningTopLevel(q)) {
            queueHostRelayout();
        }

        if (event->type() == QEvent::FocusIn) {
            auto* focusedWidget =
                qobject_cast<QWidget*>(watched);
            if (focusedWidget
                && (focusedWidget == q
                    || q->isAncestorOf(focusedWidget))) {
                if (!keyboardInteractive
                    && !isTransientPointerFocus(event)) {
                    enterKeyboardMode();
                }
            }
        }

        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint local = q->mapFromGlobal(
                fluentMouseGlobalPos(mouseEvent));
            if (!overlay::visibleCardContains(
                    q->rect(), local)) {
                closeFocusDisposition =
                    CloseFocusDisposition::Preserve;
            }
        }

        if (event->type() == QEvent::KeyPress
            && focusIsInside()) {
            if (!keyboardInteractive)
                enterKeyboardMode();
            return handleKeyEvent(
                static_cast<QKeyEvent*>(event));
        }
        return false;
    }

    QList<QAction*> primaryActions() const
    {
        return actions.actions(
            CommandActionModel::Section::Primary);
    }

    QList<QAction*> secondaryActions() const
    {
        return actions.actions(
            CommandActionModel::Section::Secondary);
    }

    bool hasPresentableContent() const
    {
        return hasCommandRows(
                   visiblePresentableActions(
                       actions,
                       CommandActionModel::Section::Primary))
            || hasCommandRows(
                   visiblePresentableActions(
                       actions,
                       CommandActionModel::Section::Secondary));
    }

    QSize preferredSize() const
    {
        return preferredOuterSize.isValid()
            ? preferredOuterSize
            : overlay::outerSizeForVisibleCard(
                  QSize(
                      kDefaultMenuWidth + kCardInset * 2,
                      kCommandTargetExtent + kCardInset * 2),
                  overlay::defaultShadowMargin());
    }

    QSize minimumPreferredSize() const
    {
        return overlay::outerSizeForVisibleCard(
            QSize(
                kCommandTargetExtent + kCardInset * 2,
                kCommandTargetExtent + kCardInset * 2),
            overlay::defaultShadowMargin());
    }

    void applyTheme()
    {
        if (!q)
            return;
        q->setFont(
            q->themeFont(Typography::FontRole::Body).toQFont());
        if (moreButton)
            moreButton->onThemeUpdated();
        if (primaryMenuDivider)
            primaryMenuDivider->onThemeUpdated();
        for (QWidget* widget : primaryPresenters) {
            if (auto* presenter =
                    dynamic_cast<CommandPresenter*>(widget)) {
                presenter->onThemeUpdated();
            } else if (auto* divider =
                           dynamic_cast<layout::Divider*>(widget)) {
                divider->onThemeUpdated();
            }
        }
        for (QWidget* widget : menuRows) {
            if (auto* presenter =
                    dynamic_cast<CommandPresenter*>(widget)) {
                presenter->onThemeUpdated();
            } else if (auto* divider =
                           dynamic_cast<layout::Divider*>(widget)) {
                divider->onThemeUpdated();
            }
        }
        refreshPrimaryPresenters();
        recomputePresentation(
            q->isOpen() || q->isVisible()
                ? RecomputeReason::OpenStateChange
                : RecomputeReason::Closed);
    }

    bool setExpandedRequested(
        bool requested,
        bool enterKeyboardOnExpand = true)
    {
        if (!q || (!q->isOpen() && !q->isVisible()))
            return false;
        if (!expandableContent)
            return false;
        if (!requested
            && (alwaysExpanded || !originalHasPrimary)) {
            return false;
        }
        if (expanded == requested)
            return true;

        const bool focusWasInMenu =
            focusedArea() == FocusArea::Menu;
        if (!updateExpandedState(requested))
            return false;
        layoutPresentation(true);
        if (requested && enterKeyboardOnExpand) {
            enterKeyboardMode();
        } else if (focusWasInMenu && keyboardInteractive) {
            focusMoreOrFirstPrimary();
        }
        return true;
    }

    void handleAlwaysExpandedChanged()
    {
        if (!q)
            return;
        recomputePresentation(
            q->isOpen() || q->isVisible()
                ? RecomputeReason::OpenStateChange
                : RecomputeReason::Closed);
    }

    void prepareForOpen()
    {
        if (!q)
            return;
        preOpenFocus = QApplication::focusWidget();
        if (preOpenFocus
            && preOpenFocus->window() != owningTopLevel(q)) {
            preOpenFocus.clear();
        }
        closeFocusDisposition =
            CloseFocusDisposition::Default;
        focusMenuOnNextExpansion = true;
        keyboardInteractive =
            showMode == CommandBarFlyout::ShowMode::Standard;
        recomputePresentation(
            RecomputeReason::PreparingOpen);
    }

    void afterOpen()
    {
        if (!q)
            return;
        // Reconcile the nested scroll viewport after the popup backing store
        // and native geometry are live. Before exposure, QScrollArea can still
        // report its default viewport width rather than the flyout card width.
        // zh_CN: 弹出层 backing store 与原生几何生效后再校准嵌套滚动视口；
        // 显示前 QScrollArea 仍可能报告默认视口宽度，而不是 Flyout 卡片宽度。
        layoutPresentation(true);
        // Let QAbstractScrollArea settle an as-needed vertical scrollbar, then
        // reconcile once more so the fixed content never creates horizontal
        // range behind that gutter.
        // zh_CN: 等按需垂直滚动条完成布局后再校准一次，避免固定内容宽度
        // 在沟槽后方产生水平滚动范围。
        queueHostRelayout();
        if (showMode
                == CommandBarFlyout::ShowMode::Standard
            || keyboardInteractive) {
            enterKeyboardMode();
            focusFirstTarget();
        }
    }

    void beforeClose()
    {
        if (closeFocusDisposition
            == CloseFocusDisposition::Default) {
            closeFocusDisposition =
                keyboardInteractive && focusIsInside()
                ? CloseFocusDisposition::Restore
                : CloseFocusDisposition::Preserve;
        }
    }

    void afterClose()
    {
        const bool restore =
            closeFocusDisposition
                == CloseFocusDisposition::Restore;
        const QPointer<QWidget> restoreTarget = preOpenFocus;

        keyboardInteractive = false;
        focusMenuOnNextExpansion = true;
        closeFocusDisposition =
            CloseFocusDisposition::Default;
        preOpenFocus.clear();
        if (!updateExpandedState(false))
            return;
        layoutPresentation(false);

        if (restore && restoreTarget
            && restoreTarget->isVisible()
            && restoreTarget->isEnabled()
            && q
            && restoreTarget->window() == owningTopLevel(q)) {
            restoreTarget->setFocus(Qt::PopupFocusReason);
        }
    }

    CommandBarFlyout* q = nullptr;
    CommandActionModel actions;
    CommandBarFlyout::ShowMode showMode =
        CommandBarFlyout::ShowMode::Standard;
    bool expanded = false;
    bool alwaysExpanded = false;
    bool pointPlacement = false;
    QPointer<QWidget> pointSource;
    QPoint localPoint;

private:
    static QVector<RowSpec> rowSpecs(
        const QList<QAction*>& overflowedPrimary,
        const QList<QAction*>& secondary)
    {
        QVector<RowSpec> result;
        result.reserve(
            overflowedPrimary.size() + secondary.size() + 1);
        const auto append =
            [&result](const QList<QAction*>& source,
                      bool secondaryGroup) {
                for (QAction* action : source) {
                    if (!action)
                        continue;
                    RowSpec spec;
                    spec.action = action;
                    spec.kind = action->isSeparator()
                        ? RowKind::ActionSeparator
                        : RowKind::Command;
                    spec.secondary = secondaryGroup;
                    result.append(spec);
                }
            };

        append(overflowedPrimary, false);
        if (hasCommandRows(overflowedPrimary)
            && hasCommandRows(secondary)) {
            RowSpec separator;
            separator.kind = RowKind::GroupSeparator;
            result.append(separator);
        }
        append(secondary, true);
        return result;
    }

    static bool sameSpecs(
        const QVector<RowSpec>& first,
        const QVector<RowSpec>& second)
    {
        if (first.size() != second.size())
            return false;
        for (int index = 0; index < first.size(); ++index) {
            if (first.at(index).kind != second.at(index).kind
                || first.at(index).secondary
                    != second.at(index).secondary
                || first.at(index).action.data()
                    != second.at(index).action.data()) {
                return false;
            }
        }
        return true;
    }

    FocusSnapshot captureFocus() const
    {
        FocusSnapshot snapshot;
        QWidget* focused = QApplication::focusWidget();
        snapshot.inside =
            focused
            && q
            && (focused == q || q->isAncestorOf(focused));
        if (!snapshot.inside || focused == q)
            return snapshot;

        int visualIndex = 0;
        const auto inspectWidget =
            [&snapshot, focused, &visualIndex](
                QWidget* widget,
                FocusArea area) {
                if (!widget || !widget->isVisible()
                    || !widget->isEnabled()) {
                    return false;
                }
                if (widget == focused) {
                    snapshot.visualIndex = visualIndex;
                    snapshot.area = area;
                    if (auto* presenter =
                            dynamic_cast<CommandPresenter*>(
                                widget)) {
                        snapshot.action = presenter->action();
                    }
                    return true;
                }
                ++visualIndex;
                return false;
            };

        const auto inspectPrimary =
            [this, &inspectWidget](QAction* action) {
                QWidget* widget =
                    primaryPresenters.value(action);
                return dynamic_cast<CommandPresenter*>(
                           widget)
                    && inspectWidget(
                        widget, FocusArea::Primary);
            };
        if (q->layoutDirection() == Qt::RightToLeft
            && showMore
            && inspectWidget(
                moreButton, FocusArea::More)) {
            return snapshot;
        }
        if (q->layoutDirection() == Qt::LeftToRight) {
            for (QAction* action : inlinePrimary) {
                if (inspectPrimary(action))
                    return snapshot;
            }
        } else {
            for (auto it = inlinePrimary.crbegin();
                 it != inlinePrimary.crend();
                 ++it) {
                if (inspectPrimary(*it))
                    return snapshot;
            }
        }
        if (q->layoutDirection() == Qt::LeftToRight
            && showMore
            && inspectWidget(
                moreButton, FocusArea::More)) {
            return snapshot;
        }
        if (expanded) {
            for (QWidget* widget : menuRows) {
                if (dynamic_cast<CommandPresenter*>(
                        widget)
                    && inspectWidget(
                        widget, FocusArea::Menu)) {
                    return snapshot;
                }
            }
        }
        return snapshot;
    }

    void rebuildPrimaryPresenters()
    {
        for (QWidget* widget : primaryPresenters) {
            if (!widget)
                continue;
            widget->removeEventFilter(this);
            widget->hide();
            widget->deleteLater();
        }
        primaryPresenters.clear();
        primarySeparatorState.clear();

        const QList<QAction*> primary = actions.actions(
            CommandActionModel::Section::Primary);
        for (QAction* action : primary) {
            if (!action)
                continue;
            QWidget* widget = nullptr;
            if (action->isSeparator()) {
                auto* divider = new layout::Divider(
                    Qt::Vertical, primaryRow);
                divider->setObjectName(
                    QStringLiteral(
                        "FluentCommandBarFlyout.PrimarySeparator"));
                divider->setLeadingInset(::Spacing::Small);
                divider->setTrailingInset(::Spacing::Small);
                widget = divider;
                primarySeparatorState.insert(action, true);
            } else {
                auto* presenter = new CommandPresenter(
                    action,
                    CommandPresenter::Mode::Primary,
                    [this](QAction* command) {
                        activateAction(command);
                    },
                    primaryRow);
                presenter->setObjectName(
                    QStringLiteral(
                        "FluentCommandBarFlyout.PrimaryPresenter"));
                presenter->setPrimaryLabelCollapsed(false);
                presenter->installEventFilter(this);
                widget = presenter;
                primarySeparatorState.insert(action, false);
            }
            widget->hide();
            primaryPresenters.insert(action, widget);
        }
    }

    void refreshPrimaryPresenters()
    {
        const QList<QAction*> primary = actions.actions(
            CommandActionModel::Section::Primary);
        bool rebuild =
            primary.size() != primaryPresenters.size();
        if (!rebuild) {
            for (QAction* action : primary) {
                if (!action
                    || !primaryPresenters.contains(action)
                    || primarySeparatorState.value(
                           action, false)
                        != action->isSeparator()) {
                    rebuild = true;
                    break;
                }
            }
        }
        if (rebuild) {
            rebuildPrimaryPresenters();
            return;
        }
        for (QAction* action : primary) {
            auto* presenter =
                dynamic_cast<CommandPresenter*>(
                    primaryPresenters.value(action));
            if (!presenter)
                continue;
            presenter->setPrimaryLabelCollapsed(false);
            presenter->synchronize();
        }
    }

    int primaryPresenterWidth(QAction* action) const
    {
        QWidget* widget = primaryPresenters.value(action);
        if (!widget)
            return 0;
        return action && action->isSeparator()
            ? kSeparatorExtent
            : qMax(kCommandTargetExtent,
                   widget->sizeHint().width());
    }

    int projectedPrimaryWidth(
        const QList<QAction*>& projection,
        bool includeMore) const
    {
        int width = 0;
        int count = 0;
        for (QAction* action : projection) {
            const int itemWidth = primaryPresenterWidth(action);
            if (itemWidth <= 0)
                continue;
            width += itemWidth;
            ++count;
        }
        if (includeMore) {
            width += kCommandTargetExtent;
            ++count;
        }
        if (count > 1)
            width += (count - 1) * kItemSpacing;
        return width;
    }

    QSize maximumCardSize() const
    {
        QWidget* topLevel = owningTopLevel(q);
        QRect surface = topLevel
            ? overlay::overlaySurfaceRect(topLevel)
            : QRect(0, 0, 640, 480);
        if (!surface.isValid() || surface.isEmpty())
            surface = QRect(0, 0, 640, 480);

        const int minimumExtent =
            kCommandTargetExtent + kCardInset * 2;
        const QSize maximum(
            qMax(
                minimumExtent,
                surface.width() - kHostEdgeMargin * 2),
            qMax(
                minimumExtent,
                surface.height() - kHostEdgeMargin * 2));
        if ((surface.width() < minimumExtent
             || surface.height() < minimumExtent)
            && !impossibleGeometryWarningIssued) {
            impossibleGeometryWarningIssued = true;
            qCWarning(logging::commandBarCategory)
                << "CommandBarFlyout host is smaller than one"
                << "minimum command target plus card insets"
                << "surface=" << surface
                << "minimumExtent=" << minimumExtent;
        }
        return maximum;
    }

    void recomputeProjection()
    {
        const QList<QAction*> primarySource =
            visiblePresentableActions(
                actions,
                CommandActionModel::Section::Primary);
        const QList<QAction*> secondarySource =
            visiblePresentableActions(
                actions,
                CommandActionModel::Section::Secondary);
        const QSet<QAction*> allPrimaryCommands =
            nonSeparatorSet(primarySource);
        const QSet<QAction*> allSecondaryCommands =
            nonSeparatorSet(secondarySource);

        originalHasPrimary =
            hasCommandRows(normalizedProjection(
                primarySource, allPrimaryCommands));
        QSet<QAction*> inlineCommands = allPrimaryCommands;
        QSet<QAction*> overflowCommands;
        QList<QAction*> nextInline = normalizedProjection(
            primarySource, inlineCommands);
        QList<QAction*> nextSecondary =
            normalizedProjection(
                secondarySource, allSecondaryCommands);

        const QSize maximum = maximumCardSize();
        const int availableWidth = qMax(
            0, maximum.width() - kCardInset * 2);
        bool reserveMore =
            originalHasPrimary
            && hasCommandRows(nextSecondary)
            && !alwaysExpanded;
        if (projectedPrimaryWidth(nextInline, reserveMore)
            > availableWidth) {
            reserveMore = !alwaysExpanded;
            QVector<OverflowCandidate> candidates;
            candidates.reserve(allPrimaryCommands.size());
            for (int index = 0; index < primarySource.size();
                 ++index) {
                QAction* action = primarySource.at(index);
                if (!action || action->isSeparator())
                    continue;
                OverflowCandidate candidate;
                candidate.action = action;
                candidate.logicalIndex = index;
                candidate.priorityRank =
                    overflowPriorityRank(action->priority());
                candidates.append(candidate);
            }
            std::stable_sort(
                candidates.begin(),
                candidates.end(),
                [](const OverflowCandidate& first,
                   const OverflowCandidate& second) {
                    if (first.priorityRank
                        != second.priorityRank) {
                        return first.priorityRank
                            < second.priorityRank;
                    }
                    return first.logicalIndex
                        > second.logicalIndex;
                });

            for (const OverflowCandidate& candidate
                 : candidates) {
                inlineCommands.remove(candidate.action);
                overflowCommands.insert(candidate.action);
                nextInline = normalizedProjection(
                    primarySource, inlineCommands);
                if (projectedPrimaryWidth(
                        nextInline, reserveMore)
                    <= availableWidth) {
                    break;
                }
            }
        }

        inlinePrimary = nextInline;
        overflowedPrimary = normalizedProjection(
            primarySource, overflowCommands);
        normalizedSecondary = nextSecondary;
        expandableContent =
            hasCommandRows(overflowedPrimary)
            || hasCommandRows(normalizedSecondary);
        showMore =
            originalHasPrimary
            && expandableContent
            && !alwaysExpanded;
    }

    void setMenuRows()
    {
        const QVector<RowSpec> next =
            rowSpecs(
                overflowedPrimary, normalizedSecondary);
        if (sameSpecs(menuSpecs, next)) {
            for (QWidget* widget : menuRows) {
                if (auto* presenter =
                        dynamic_cast<CommandPresenter*>(widget)) {
                    presenter->synchronize();
                }
            }
            return;
        }

        for (QWidget* widget : menuRows) {
            if (!widget)
                continue;
            widget->removeEventFilter(this);
            widget->hide();
            widget->deleteLater();
        }
        menuRows.clear();
        menuSpecs = next;

        for (const RowSpec& spec : menuSpecs) {
            if (spec.kind == RowKind::Command) {
                auto* presenter = new CommandPresenter(
                    spec.action.data(),
                    CommandPresenter::Mode::Overflow,
                    [this](QAction* command) {
                        activateAction(command);
                    },
                    menuContent);
                presenter->setObjectName(
                    spec.secondary
                        ? QStringLiteral(
                              "FluentCommandBarFlyout.SecondaryRow")
                        : QStringLiteral(
                              "FluentCommandBarFlyout.OverflowRow"));
                presenter->installEventFilter(this);
                menuRows.append(presenter);
                continue;
            }

            auto* divider = new layout::Divider(
                Qt::Horizontal, menuContent);
            divider->setObjectName(
                spec.kind == RowKind::GroupSeparator
                    ? QStringLiteral(
                          "FluentCommandBarFlyout.GroupSeparator")
                    : QStringLiteral(
                          "FluentCommandBarFlyout.MenuSeparator"));
            divider->setLeadingInset(::Spacing::Small);
            divider->setTrailingInset(::Spacing::Small);
            menuRows.append(divider);
        }
    }

    int menuContentHeight() const
    {
        int height = 0;
        for (QWidget* widget : menuRows) {
            if (!widget)
                continue;
            height +=
                dynamic_cast<CommandPresenter*>(widget)
                ? kCommandTargetExtent
                : kSeparatorExtent;
        }
        return height;
    }

    int menuDesiredWidth() const
    {
        int width = kDefaultMenuWidth;
        bool hasRows = false;
        for (QWidget* widget : menuRows) {
            auto* presenter =
                dynamic_cast<CommandPresenter*>(widget);
            if (!presenter)
                continue;
            hasRows = true;
            presenter->synchronize();
            width = qMax(width, presenter->sizeHint().width());
        }
        return hasRows ? width : 0;
    }

    void recomputePresentation(RecomputeReason reason)
    {
        recomputePresentation(reason, FocusSnapshot{});
    }

    void recomputePresentation(
        RecomputeReason reason,
        const FocusSnapshot& suppliedSnapshot)
    {
        if (!q || recomputing)
            return;
        recomputing = true;

        FocusSnapshot snapshot = suppliedSnapshot;
        if (!snapshot.inside
            && snapshot.visualIndex < 0
            && snapshot.action.isNull()) {
            snapshot = captureFocus();
        }

        recomputeProjection();
        setMenuRows();

        bool nextExpanded = expanded;
        if (reason == RecomputeReason::Closed) {
            nextExpanded = false;
        } else if (!expandableContent) {
            nextExpanded = false;
        } else if (reason
                   == RecomputeReason::PreparingOpen) {
            nextExpanded =
                alwaysExpanded
                || !originalHasPrimary
                || showMode
                    == CommandBarFlyout::ShowMode::Standard;
        } else if (alwaysExpanded || !originalHasPrimary) {
            nextExpanded = true;
        }

        if (!updateExpandedState(nextExpanded)) {
            return;
        }
        layoutPresentation(true);
        repairFocus(snapshot);

        const bool nowHasContent =
            originalHasPrimary
            || hasCommandRows(normalizedSecondary);
        if (!nowHasContent
            && (q->isOpen() || q->isVisible())) {
            closeFocusDisposition =
                focusIsInside()
                ? CloseFocusDisposition::Restore
                : CloseFocusDisposition::Preserve;
            const QPointer<CommandBarFlyout> ownerGuard = q;
            recomputing = false;
            ownerGuard->close();
            return;
        }
        recomputing = false;
    }

    bool updateExpandedState(bool value)
    {
        if (expanded == value) {
            updateMoreAccessibility();
            return true;
        }
        expanded = value;
        updateMoreAccessibility();
        const QPointer<CommandBarFlyout> ownerGuard = q;
        emit q->expandedChanged(value);
        return !ownerGuard.isNull();
    }

    void updateMoreAccessibility()
    {
        if (moreButton) {
            moreButton->setExpandedState(
                expanded, showMore);
        }
    }

    void layoutPresentation(bool reposition)
    {
        if (!q || !primaryRow || !scrollView || !menuContent)
            return;

        const QSize maximum = maximumCardSize();
        const bool primaryVisible =
            hasCommandRows(inlinePrimary) || showMore;
        const int primaryWidth =
            projectedPrimaryWidth(inlinePrimary, showMore);
        const int rowsWidth =
            expanded ? menuDesiredWidth() : 0;
        int contentWidth = qMax(primaryWidth, rowsWidth);
        if (contentWidth <= 0)
            contentWidth = kCommandTargetExtent;
        const int cardWidth = qMin(
            maximum.width(),
            contentWidth + kCardInset * 2);

        const int rowsHeight =
            expanded ? menuContentHeight() : 0;
        const int primaryHeight =
            primaryVisible ? kCommandTargetExtent : 0;
        const bool primaryMenuDividerVisible =
            primaryVisible && expanded && rowsHeight > 0;
        const int primaryMenuDividerHeight =
            primaryMenuDividerVisible ? kSeparatorExtent : 0;
        const int desiredCardHeight =
            kCardInset * 2
            + primaryHeight
            + primaryMenuDividerHeight
            + rowsHeight;
        const int cardHeight = qMin(
            maximum.height(),
            qMax(
                kCommandTargetExtent + kCardInset * 2,
                desiredCardHeight));

        const QSize outerSize =
            overlay::outerSizeForVisibleCard(
                QSize(cardWidth, cardHeight),
                overlay::defaultShadowMargin());
        preferredOuterSize = outerSize;
        if (q->size() != outerSize)
            q->resize(outerSize);

        const QRect cardRect =
            overlay::visibleCardRect(q->rect());
        const QRect contentRect =
            cardRect.adjusted(
                kCardInset,
                kCardInset,
                -kCardInset,
                -kCardInset);

        if (primaryVisible) {
            primaryRow->setGeometry(
                contentRect.left(),
                contentRect.top(),
                contentRect.width(),
                kCommandTargetExtent);
            primaryRow->show();
            layoutPrimaryRow(primaryRow->rect());
        } else {
            primaryRow->hide();
            for (QWidget* widget : primaryPresenters) {
                if (widget)
                    widget->hide();
            }
            moreButton->hide();
        }

        const int dividerTop =
            contentRect.top() + primaryHeight;
        if (primaryMenuDividerVisible) {
            primaryMenuDivider->setGeometry(
                contentRect.left(),
                dividerTop,
                contentRect.width(),
                primaryMenuDividerHeight);
            primaryMenuDivider->show();
        } else {
            primaryMenuDivider->hide();
        }

        const int scrollTop =
            dividerTop + primaryMenuDividerHeight;
        const int scrollHeight = qMax(
            0,
            contentRect.bottom() + 1 - scrollTop);
        if (expanded && rowsHeight > 0 && scrollHeight > 0) {
            scrollView->setGeometry(
                contentRect.left(),
                scrollTop,
                contentRect.width(),
                scrollHeight);
            scrollView->show();
            const int measuredViewportWidth = qMax(
                0, scrollView->viewport()->width());
            const int maximumGutter =
                qMax(
                    ::Spacing::Large,
                    scrollView->verticalScrollBar()
                        ->sizeHint()
                        .width());
            const bool measuredWidthIsCurrent =
                measuredViewportWidth > 0
                && measuredViewportWidth <= contentRect.width()
                && contentRect.width() - measuredViewportWidth
                    <= maximumGutter;
            const int viewportWidth =
                measuredWidthIsCurrent
                ? measuredViewportWidth
                : contentRect.width();
            menuContent->setFixedSize(
                viewportWidth, rowsHeight);
            layoutMenuRows(viewportWidth);
        } else {
            scrollView->hide();
            for (QWidget* widget : menuRows) {
                if (widget)
                    widget->hide();
            }
        }

        updateMoreAccessibility();
        if (reposition
            && (q->isOpen() || q->isVisible())) {
            q->move(q->computePosition());
            q->raise();
        }
    }

    void layoutPrimaryRow(const QRect& rect)
    {
        QSet<QAction*> shown;
        for (QAction* action : inlinePrimary)
            shown.insert(action);
        for (auto it = primaryPresenters.cbegin();
             it != primaryPresenters.cend();
             ++it) {
            if (it.value() && !shown.contains(it.key()))
                it.value()->hide();
        }

        const int y =
            (rect.height() - kCommandTargetExtent) / 2;
        if (q->layoutDirection() == Qt::LeftToRight) {
            int x = rect.left();
            for (QAction* action : inlinePrimary) {
                QWidget* widget =
                    primaryPresenters.value(action);
                if (!widget)
                    continue;
                const int width =
                    primaryPresenterWidth(action);
                widget->setGeometry(
                    x, y, width, kCommandTargetExtent);
                widget->show();
                x += width + kItemSpacing;
            }
            if (showMore) {
                moreButton->setGeometry(
                    x,
                    y,
                    kCommandTargetExtent,
                    kCommandTargetExtent);
            }
        } else {
            int x = rect.right() + 1;
            for (QAction* action : inlinePrimary) {
                QWidget* widget =
                    primaryPresenters.value(action);
                if (!widget)
                    continue;
                const int width =
                    primaryPresenterWidth(action);
                x -= width;
                widget->setGeometry(
                    x, y, width, kCommandTargetExtent);
                widget->show();
                x -= kItemSpacing;
            }
            if (showMore) {
                x -= kCommandTargetExtent;
                moreButton->setGeometry(
                    x,
                    y,
                    kCommandTargetExtent,
                    kCommandTargetExtent);
            }
        }
        moreButton->setVisible(showMore);
    }

    void layoutMenuRows(int width)
    {
        menuContent->setLayoutDirection(q->layoutDirection());
        int y = 0;
        for (QWidget* widget : menuRows) {
            if (!widget)
                continue;
            widget->setLayoutDirection(q->layoutDirection());
            const int height =
                dynamic_cast<CommandPresenter*>(widget)
                ? kCommandTargetExtent
                : kSeparatorExtent;
            widget->setGeometry(0, y, width, height);
            widget->show();
            // A hidden popup can receive a theme or layout-direction refresh
            // before its scroll viewport is exposed. Explicitly invalidate
            // each row when it becomes visible so the custom overflow text
            // paint is not left with the pre-open backing-store contents.
            // zh_CN: 隐藏的弹出层可能在滚动视口显示前收到主题或布局方向刷新；
            // 行重新可见时显式失效，避免自绘溢出文本沿用打开前的 backing store。
            widget->update();
            y += height;
        }
        menuContent->update();
        scrollView->viewport()->update();
    }

    QVector<FocusTarget> primaryFocusTargets() const
    {
        QVector<FocusTarget> result;
        const auto appendAction =
            [this, &result](QAction* action) {
                QWidget* widget =
                    primaryPresenters.value(action);
                if (!action || action->isSeparator()
                    || !widget || !widget->isVisible()
                    || !action->isVisible()
                    || !action->isEnabled()) {
                    return;
                }
                FocusTarget target;
                target.widget = widget;
                target.action = action;
                target.area = FocusArea::Primary;
                result.append(target);
            };

        if (q->layoutDirection() == Qt::RightToLeft
            && showMore && moreButton->isVisible()) {
            FocusTarget more;
            more.widget = moreButton;
            more.area = FocusArea::More;
            result.append(more);
        }
        if (q->layoutDirection() == Qt::LeftToRight) {
            for (QAction* action : inlinePrimary)
                appendAction(action);
        } else {
            for (auto it = inlinePrimary.crbegin();
                 it != inlinePrimary.crend();
                 ++it) {
                appendAction(*it);
            }
        }
        if (q->layoutDirection() == Qt::LeftToRight
            && showMore && moreButton->isVisible()) {
            FocusTarget more;
            more.widget = moreButton;
            more.area = FocusArea::More;
            result.append(more);
        }
        return result;
    }

    QVector<FocusTarget> menuFocusTargets() const
    {
        QVector<FocusTarget> result;
        if (!expanded || !scrollView->isVisible())
            return result;
        for (QWidget* widget : menuRows) {
            auto* presenter =
                dynamic_cast<CommandPresenter*>(widget);
            if (!presenter
                || !presenter->isVisible()
                || !presenter->isEnabled()
                || !presenter->action()) {
                continue;
            }
            FocusTarget target;
            target.widget = presenter;
            target.action = presenter->action();
            target.area = FocusArea::Menu;
            result.append(target);
        }
        return result;
    }

    QVector<FocusTarget> focusTargets() const
    {
        QVector<FocusTarget> result = primaryFocusTargets();
        const QVector<FocusTarget> menu = menuFocusTargets();
        result.reserve(result.size() + menu.size());
        for (const FocusTarget& target : menu)
            result.append(target);
        return result;
    }

    int focusTargetIndex(QObject* object) const
    {
        const QVector<FocusTarget> targets = focusTargets();
        for (int index = 0; index < targets.size(); ++index) {
            if (targets.at(index).widget.data() == object)
                return index;
        }
        return -1;
    }

    FocusArea focusedArea() const
    {
        const QVector<FocusTarget> targets = focusTargets();
        QWidget* focused = QApplication::focusWidget();
        for (const FocusTarget& target : targets) {
            if (target.widget.data() == focused)
                return target.area;
        }
        return FocusArea::Primary;
    }

    bool focusIsInside() const
    {
        QWidget* focused = QApplication::focusWidget();
        return focused
            && q
            && (focused == q || q->isAncestorOf(focused));
    }

    void rememberFocusTarget(int index)
    {
        const QVector<FocusTarget> targets = focusTargets();
        if (index < 0 || index >= targets.size())
            return;
        lastFocusedAction = targets.at(index).action;
        lastFocusedArea = targets.at(index).area;
    }

    void focusTarget(int index, Qt::FocusReason reason)
    {
        const QVector<FocusTarget> targets = focusTargets();
        if (index < 0 || index >= targets.size())
            return;
        rememberFocusTarget(index);
        QWidget* widget = targets.at(index).widget.data();
        if (widget) {
            widget->setFocus(reason);
            if (targets.at(index).area == FocusArea::Menu
                && scrollView) {
                scrollView->ensureWidgetVisible(widget);
            }
        }
    }

    void focusFirstTarget()
    {
        const QVector<FocusTarget> targets = focusTargets();
        if (targets.isEmpty()) {
            q->setFocus(Qt::PopupFocusReason);
            return;
        }

        for (int index = 0; index < targets.size(); ++index) {
            if (lastFocusedAction
                && targets.at(index).action
                    == lastFocusedAction
                && targets.at(index).area
                    == lastFocusedArea) {
                focusTarget(index, Qt::PopupFocusReason);
                return;
            }
        }
        focusTarget(0, Qt::PopupFocusReason);
    }

    void focusFirstMenuTarget()
    {
        const QVector<FocusTarget> targets = focusTargets();
        for (int index = 0; index < targets.size(); ++index) {
            if (targets.at(index).area == FocusArea::Menu) {
                focusTarget(index, Qt::PopupFocusReason);
                return;
            }
        }
        focusFirstTarget();
    }

    void focusMoreOrFirstPrimary()
    {
        const QVector<FocusTarget> targets = focusTargets();
        for (int index = 0; index < targets.size(); ++index) {
            if (targets.at(index).area == FocusArea::More) {
                focusTarget(index, Qt::OtherFocusReason);
                return;
            }
        }
        if (!targets.isEmpty())
            focusTarget(0, Qt::OtherFocusReason);
        else
            q->setFocus(Qt::OtherFocusReason);
    }

    void repairFocus(const FocusSnapshot& snapshot)
    {
        if (!snapshot.inside || !keyboardInteractive)
            return;
        const QVector<FocusTarget> targets = focusTargets();
        if (targets.isEmpty()) {
            q->setFocus(Qt::OtherFocusReason);
            return;
        }
        for (int index = 0; index < targets.size(); ++index) {
            const bool sameMore =
                snapshot.area == FocusArea::More
                && targets.at(index).area == FocusArea::More;
            const bool sameAction =
                snapshot.action
                && targets.at(index).action
                    == snapshot.action
                && targets.at(index).area == snapshot.area;
            if (sameMore || sameAction) {
                focusTarget(index, Qt::OtherFocusReason);
                return;
            }
        }
        focusTarget(
            snapshot.visualIndex < 0
                ? 0
                : qMin(
                      snapshot.visualIndex,
                      targets.size() - 1),
            Qt::OtherFocusReason);
    }

    void enterKeyboardMode()
    {
        keyboardInteractive = true;
    }

    bool handleKeyEvent(QKeyEvent* event)
    {
        if (!event || !q)
            return false;
        if (event->key() == Qt::Key_Escape) {
            closeFocusDisposition =
                focusIsInside()
                ? CloseFocusDisposition::Restore
                : CloseFocusDisposition::Preserve;
            q->close();
            event->accept();
            return true;
        }

        const QVector<FocusTarget> allTargets = focusTargets();
        if (allTargets.isEmpty())
            return false;
        int current = focusTargetIndex(
            QApplication::focusWidget());
        if (current < 0)
            current = 0;

        if (event->key() == Qt::Key_Tab
            || event->key() == Qt::Key_Backtab) {
            const bool forward =
                event->key() != Qt::Key_Backtab
                && !event->modifiers().testFlag(
                    Qt::ShiftModifier);
            const int next =
                (current + (forward ? 1 : -1)
                 + allTargets.size())
                % allTargets.size();
            focusTarget(next, Qt::TabFocusReason);
            event->accept();
            return true;
        }

        const FocusTarget target = allTargets.at(current);
        if (event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Enter
            || event->key() == Qt::Key_Space) {
            auto* button = qobject_cast<QAbstractButton*>(
                target.widget.data());
            if (!button)
                return false;
            button->click();
            event->accept();
            return true;
        }

        if (target.area == FocusArea::Menu) {
            const QVector<FocusTarget> menu =
                menuFocusTargets();
            int menuIndex = -1;
            for (int index = 0; index < menu.size(); ++index) {
                if (menu.at(index).widget
                    == target.widget) {
                    menuIndex = index;
                    break;
                }
            }
            if (menuIndex < 0)
                return false;
            int destination = menuIndex;
            switch (event->key()) {
            case Qt::Key_Up:
                destination =
                    (menuIndex - 1 + menu.size())
                    % menu.size();
                break;
            case Qt::Key_Down:
                destination =
                    (menuIndex + 1) % menu.size();
                break;
            case Qt::Key_Home:
                destination = 0;
                break;
            case Qt::Key_End:
                destination = menu.size() - 1;
                break;
            default:
                return false;
            }
            QWidget* widget =
                menu.at(destination).widget.data();
            const int allIndex =
                focusTargetIndex(widget);
            focusTarget(allIndex, Qt::OtherFocusReason);
            event->accept();
            return true;
        }

        const QVector<FocusTarget> primary =
            primaryFocusTargets();
        int primaryIndex = -1;
        for (int index = 0; index < primary.size(); ++index) {
            if (primary.at(index).widget
                == target.widget) {
                primaryIndex = index;
                break;
            }
        }
        if (primaryIndex < 0)
            return false;

        int destination = primaryIndex;
        switch (event->key()) {
        case Qt::Key_Left:
            destination = qMax(0, primaryIndex - 1);
            break;
        case Qt::Key_Right:
            destination =
                qMin(primary.size() - 1, primaryIndex + 1);
            break;
        case Qt::Key_Home:
            destination = 0;
            break;
        case Qt::Key_End:
            destination = primary.size() - 1;
            break;
        case Qt::Key_Down:
            if (target.area == FocusArea::More
                && !expanded) {
                if (!setExpandedRequested(true))
                    return false;
            }
            if (expanded) {
                focusFirstMenuTarget();
                event->accept();
                return true;
            }
            return false;
        default:
            return false;
        }
        focusTarget(
            focusTargetIndex(
                primary.at(destination).widget.data()),
            Qt::OtherFocusReason);
        event->accept();
        return true;
    }

    void activateAction(QAction* action)
    {
        if (!q || !action || !action->isEnabled()
            || !action->isVisible()) {
            return;
        }
        const QPointer<CommandBarFlyout> ownerGuard = q;
        const QPointer<QAction> actionGuard = action;
        actionGuard->trigger();
        if (!ownerGuard)
            return;

        QWidget* focusedAfter = QApplication::focusWidget();
        const bool focusStillInside =
            focusedAfter
            && (focusedAfter == ownerGuard
                || ownerGuard->isAncestorOf(focusedAfter));
        closeFocusDisposition =
            focusStillInside
            ? CloseFocusDisposition::Restore
            : CloseFocusDisposition::Preserve;
        ownerGuard->close();
    }

    void queueHostRelayout()
    {
        if (hostRelayoutPending || !q)
            return;
        hostRelayoutPending = true;
        QTimer::singleShot(
            0,
            q,
            [this]() {
                hostRelayoutPending = false;
                if (!q
                    || (!q->isOpen() && !q->isVisible())) {
                    return;
                }
                recomputePresentation(
                    RecomputeReason::OpenStateChange);
            });
    }

    QWidget* primaryRow = nullptr;
    layout::Divider* primaryMenuDivider = nullptr;
    CommandMoreButton* moreButton = nullptr;
    scrolling::ScrollView* scrollView = nullptr;
    QWidget* menuContent = nullptr;
    QHash<QAction*, QWidget*> primaryPresenters;
    QHash<QAction*, bool> primarySeparatorState;
    QList<QAction*> inlinePrimary;
    QList<QAction*> overflowedPrimary;
    QList<QAction*> normalizedSecondary;
    QVector<RowSpec> menuSpecs;
    QVector<QWidget*> menuRows;
    QSize preferredOuterSize;
    QPointer<QWidget> preOpenFocus;
    QPointer<QAction> lastFocusedAction;
    FocusArea lastFocusedArea = FocusArea::Primary;
    CloseFocusDisposition closeFocusDisposition =
        CloseFocusDisposition::Default;
    bool originalHasPrimary = false;
    bool expandableContent = false;
    bool showMore = false;
    bool keyboardInteractive = false;
    bool focusMenuOnNextExpansion = true;
    bool recomputing = false;
    bool hostRelayoutPending = false;
    mutable bool impossibleGeometryWarningIssued = false;
};

CommandBarFlyout::CommandBarFlyout(QWidget* parent)
    : dialogs_flyouts::Flyout(parent),
      d(new CommandBarFlyoutPrivate(this))
{
    setFocusOnOpenEnabled(true);
}

CommandBarFlyout::~CommandBarFlyout()
{
    delete d;
}

void CommandBarFlyout::addAction(QAction* action)
{
    if (d->actions.contains(
            CommandActionModel::Section::Primary, action)
        || d->actions.contains(
            CommandActionModel::Section::Secondary, action)) {
        return;
    }
    d->actions.add(
        CommandActionModel::Section::Primary, action);
}

void CommandBarFlyout::insertAction(
    QAction* before,
    QAction* action)
{
    if (d->actions.contains(
            CommandActionModel::Section::Primary, action)
        || d->actions.contains(
            CommandActionModel::Section::Secondary, action)) {
        return;
    }
    if (d->actions.contains(
            CommandActionModel::Section::Primary, before)) {
        d->actions.insert(
            CommandActionModel::Section::Primary,
            before,
            action);
        return;
    }
    d->actions.add(
        CommandActionModel::Section::Primary, action);
}

void CommandBarFlyout::removeAction(QAction* action)
{
    if (d->actions.remove(action))
        return;
    QWidget::removeAction(action);
}

bool CommandBarFlyout::addPrimaryAction(QAction* action)
{
    return d->actions.add(
        CommandActionModel::Section::Primary, action);
}

bool CommandBarFlyout::insertPrimaryAction(
    QAction* before,
    QAction* action)
{
    return d->actions.insert(
        CommandActionModel::Section::Primary,
        before,
        action);
}

bool CommandBarFlyout::addSecondaryAction(QAction* action)
{
    return d->actions.add(
        CommandActionModel::Section::Secondary, action);
}

bool CommandBarFlyout::insertSecondaryAction(
    QAction* before,
    QAction* action)
{
    return d->actions.insert(
        CommandActionModel::Section::Secondary,
        before,
        action);
}

bool CommandBarFlyout::removeCommandAction(QAction* action)
{
    return d->actions.remove(action);
}

void CommandBarFlyout::clearPrimaryActions()
{
    d->actions.clear(CommandActionModel::Section::Primary);
}

void CommandBarFlyout::clearSecondaryActions()
{
    d->actions.clear(CommandActionModel::Section::Secondary);
}

QList<QAction*> CommandBarFlyout::primaryActions() const
{
    return d->primaryActions();
}

QList<QAction*> CommandBarFlyout::secondaryActions() const
{
    return d->secondaryActions();
}

CommandBarFlyout::ShowMode CommandBarFlyout::showMode() const
{
    return d->showMode;
}

void CommandBarFlyout::setShowMode(ShowMode mode)
{
    if (d->showMode == mode)
        return;
    d->showMode = mode;
    setFocusOnOpenEnabled(mode == ShowMode::Standard);
    emit showModeChanged(mode);
}

bool CommandBarFlyout::isExpanded() const
{
    return d->expanded;
}

bool CommandBarFlyout::isAlwaysExpanded() const
{
    return d->alwaysExpanded;
}

void CommandBarFlyout::setAlwaysExpanded(bool expanded)
{
    if (d->alwaysExpanded == expanded)
        return;
    d->alwaysExpanded = expanded;
    const QPointer<CommandBarFlyout> ownerGuard = this;
    emit alwaysExpandedChanged(expanded);
    if (!ownerGuard)
        return;
    d->handleAlwaysExpandedChanged();
}

void CommandBarFlyout::onThemeUpdated()
{
    dialogs_flyouts::Flyout::onThemeUpdated();
    d->applyTheme();
}

QSize CommandBarFlyout::sizeHint() const
{
    return d->preferredSize();
}

QSize CommandBarFlyout::minimumSizeHint() const
{
    return d->minimumPreferredSize();
}

void CommandBarFlyout::setAnchor(QWidget* anchor)
{
    d->pointPlacement = false;
    d->pointSource.clear();
    dialogs_flyouts::Flyout::setAnchor(anchor);
}

void CommandBarFlyout::showAt(QWidget* anchor)
{
    if (!isValidInvocationTarget(this, anchor, "showAt"))
        return;
    if (!d->hasPresentableContent()) {
        qCWarning(logging::commandBarCategory)
            << "showAt rejected: CommandBarFlyout has no"
            << "presentable commands";
        return;
    }

    setAnchor(anchor);
    if (isOpen() || isVisible()) {
        d->recomputePresentation(
            CommandBarFlyoutPrivate::RecomputeReason::
                OpenStateChange);
        move(computePosition());
        return;
    }
    open();
}

void CommandBarFlyout::showAt(
    QWidget* anchor,
    ShowMode mode)
{
    setShowMode(mode);
    showAt(anchor);
}

void CommandBarFlyout::showAtPoint(
    QWidget* relativeTo,
    const QPoint& localPosition)
{
    if (!isValidInvocationTarget(
            this, relativeTo, "showAtPoint")) {
        return;
    }
    if (!d->hasPresentableContent()) {
        qCWarning(logging::commandBarCategory)
            << "showAtPoint rejected: CommandBarFlyout has no"
            << "presentable commands";
        return;
    }

    dialogs_flyouts::Flyout::setAnchor(nullptr);
    d->pointPlacement = true;
    d->pointSource = relativeTo;
    d->localPoint = localPosition;
    if (isOpen() || isVisible()) {
        d->recomputePresentation(
            CommandBarFlyoutPrivate::RecomputeReason::
                OpenStateChange);
        move(computePosition());
        return;
    }
    open();
}

void CommandBarFlyout::showAtPoint(
    QWidget* relativeTo,
    const QPoint& localPosition,
    ShowMode mode)
{
    setShowMode(mode);
    showAtPoint(relativeTo, localPosition);
}

void CommandBarFlyout::setExpanded(bool expanded)
{
    d->setExpandedRequested(expanded);
}

void CommandBarFlyout::actionEvent(QActionEvent* event)
{
    dialogs_flyouts::Flyout::actionEvent(event);
    d->actions.handleActionEvent(event);
}

bool CommandBarFlyout::eventFilter(
    QObject* watched,
    QEvent* event)
{
    if (d && d->handleApplicationEvent(watched, event))
        return true;
    return dialogs_flyouts::Flyout::eventFilter(
        watched, event);
}

QPoint CommandBarFlyout::computePosition() const
{
    if (!d->pointPlacement)
        return dialogs_flyouts::Flyout::computePosition();
    if (!d->pointSource || !d->pointSource->window())
        return dialogs_flyouts::Popup::computePosition();

    QWidget* topLevel = d->pointSource->window();
    QPoint cardTopLeft =
        d->pointSource->mapTo(topLevel, d->localPoint);
    cardTopLeft.ry() += anchorOffset();
    cardTopLeft = overlay::clampCardTopLeft(
        cardTopLeft,
        overlay::visibleCardSize(size()),
        overlay::overlaySurfaceRect(topLevel),
        kHostEdgeMargin);
    return overlay::outerTopLeftForVisibleCard(cardTopLeft);
}

QWidget* CommandBarFlyout::automaticPositionAnchor() const
{
    return d->pointPlacement
        ? d->pointSource.data()
        : dialogs_flyouts::Flyout::
              automaticPositionAnchor();
}

} // namespace fluent::menus_toolbars

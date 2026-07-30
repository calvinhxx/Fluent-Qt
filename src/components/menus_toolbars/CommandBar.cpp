#include "CommandBar.h"

#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QSet>
#include <QStyle>
#include <QTimer>
#include <QVector>

#include <algorithm>
#include <functional>
#include <utility>

#include "compatibility/QtCompat.h"
#include "components/dialogs_flyouts/Flyout.h"
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

using detail::CommandActionModel;
using detail::CommandAccessibleRole;
using detail::CommandMoreButton;
using detail::CommandPresenter;

namespace {

constexpr const char* kCommandPresentationProperty =
    "_fluentqt_commandActionPresentation";
constexpr int kCommandTargetExtent = ::Spacing::ControlHeight::Large;
constexpr int kBarInset = ::Spacing::XSmall;
constexpr int kItemSpacing = ::Spacing::Gap::Tight;
constexpr int kSeparatorExtent = ::Spacing::Small + 1;
constexpr int kPopupInset = ::Spacing::XSmall;
constexpr int kPopupEdgeMargin = ::Spacing::XSmall;

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

class CommandBarOverflowPopup final
    : public dialogs_flyouts::Flyout {
public:
    using ActivationHandler = std::function<void(QAction*)>;
    using ClosedHandler = std::function<void(bool)>;
    using AnchorPressHandler = std::function<void()>;

    CommandBarOverflowPopup(
        CommandBar* owner,
        CommandMoreButton* anchor,
        ActivationHandler activationHandler,
        ClosedHandler closedHandler,
        AnchorPressHandler anchorPressHandler)
        : dialogs_flyouts::Flyout(owner),
          m_owner(owner),
          m_anchorButton(anchor),
          m_activationHandler(std::move(activationHandler)),
          m_closedHandler(std::move(closedHandler)),
          m_anchorPressHandler(std::move(anchorPressHandler))
    {
        setObjectName(
            QStringLiteral("FluentCommandBar.OverflowPopup"));
        setProperty(kCommandPresentationProperty, true);
        detail::markCommandAccessibleWidget(
            this, CommandAccessibleRole::PopupRoot);
        setAccessibleName(tr("More commands"));
        setAnimationEnabled(false);
        setModal(false);
        setDim(false);
        setClosePolicy(
            ClosePolicy(CloseOnPressOutside | CloseOnEscape));
        setPlacement(dialogs_flyouts::Flyout::Bottom);
        setAnchorOffset(kItemSpacing);
        dialogs_flyouts::Flyout::setAnchor(anchor);

        m_scrollView = new scrolling::ScrollView(this);
        m_scrollView->setObjectName(
            QStringLiteral("FluentCommandBar.OverflowScrollView"));
        m_scrollView->setFrameShape(QFrame::NoFrame);
        m_scrollView->setHorizontalScrollMode(
            scrolling::ScrollView::ScrollMode::Disabled);
        m_scrollView->setHorizontalScrollBarVisibility(
            scrolling::ScrollView::ScrollBarVisibility::Disabled);
        m_scrollView->setVerticalScrollMode(
            scrolling::ScrollView::ScrollMode::Auto);
        m_scrollView->setVerticalScrollBarVisibility(
            scrolling::ScrollView::ScrollBarVisibility::Auto);
        m_scrollView->setWidgetResizable(false);
        m_scrollView->setStyleSheet(
            QStringLiteral(
                "QScrollArea { background: transparent; border: none; }"
                "QScrollArea > QWidget > QWidget {"
                " background: transparent; }"));
        m_scrollView->viewport()->setAutoFillBackground(false);

        m_content = new QWidget();
        m_content->setObjectName(
            QStringLiteral("FluentCommandBar.OverflowContent"));
        detail::markCommandAccessibleWidget(
            m_content, CommandAccessibleRole::MenuList);
        m_content->setAutoFillBackground(false);
        m_scrollView->setWidget(m_content);

        connect(this,
                &dialogs_flyouts::Popup::opened,
                this,
                [this]() {
                    layoutRows();
                    // The as-needed vertical gutter is finalized on the next
                    // event turn; a second pass removes any horizontal range.
                    // zh_CN: 按需垂直沟槽会在下一轮事件中定型；
                    // 第二次布局用于消除残留水平滚动范围。
                    QTimer::singleShot(
                        0,
                        this,
                        [this]() {
                            if (isOpen() || isVisible())
                                layoutRows();
                        });
                });
        connect(this,
                &dialogs_flyouts::Popup::closed,
                this,
                [this]() {
                    clearAssociatedActions();
                    const bool restoreFocus = m_restoreFocusOnClose;
                    m_restoreFocusOnClose = false;
                    const ClosedHandler handler = m_closedHandler;
                    if (handler)
                        handler(restoreFocus);
                });
    }

    void setSections(const QList<QAction*>& overflowedPrimary,
                     const QList<QAction*>& secondary)
    {
        const QVector<RowSpec> next =
            rowSpecs(overflowedPrimary, secondary);
        const QPointer<QAction> previousAction = focusedAction();
        const int previousIndex = focusedRowIndex();

        if (sameSpecs(m_specs, next)) {
            for (QWidget* widget : m_rows) {
                if (auto* presenter =
                        dynamic_cast<CommandPresenter*>(widget)) {
                    presenter->synchronize();
                }
            }
            layoutRows();
            repairFocus(previousAction.data(), previousIndex);
            if (isOpen() || isVisible())
                synchronizeAssociatedActions();
            return;
        }

        for (QWidget* widget : m_rows) {
            if (!widget)
                continue;
            widget->hide();
            widget->deleteLater();
        }
        m_rows.clear();
        m_specs = next;

        for (const RowSpec& spec : m_specs) {
            if (spec.kind == RowKind::Command) {
                auto* presenter = new CommandPresenter(
                    spec.action.data(),
                    CommandPresenter::Mode::Overflow,
                    [this](QAction* action) {
                        const ActivationHandler handler =
                            m_activationHandler;
                        if (handler)
                            handler(action);
                    },
                    m_content);
                presenter->setObjectName(
                    QStringLiteral(
                        "FluentCommandBar.OverflowRow"));
                m_rows.append(presenter);
                continue;
            }

            auto* divider = new layout::Divider(
                Qt::Horizontal, m_content);
            divider->setObjectName(
                spec.kind == RowKind::GroupSeparator
                    ? QStringLiteral(
                          "FluentCommandBar.OverflowGroupSeparator")
                    : QStringLiteral(
                          "FluentCommandBar.OverflowSeparator"));
            divider->setLeadingInset(::Spacing::Small);
            divider->setTrailingInset(::Spacing::Small);
            m_rows.append(divider);
        }

        layoutRows();
        repairFocus(previousAction.data(), previousIndex);
        if (isOpen() || isVisible())
            synchronizeAssociatedActions();
    }

    bool hasRows() const
    {
        for (const RowSpec& spec : m_specs) {
            if (spec.kind == RowKind::Command)
                return true;
        }
        return false;
    }

    void openAtAnchor(bool focusFirstCommand)
    {
        if (!hasRows() || !m_anchorButton)
            return;
        synchronizeAssociatedActions();
        dialogs_flyouts::Flyout::setAnchor(m_anchorButton);
        setFocusOnOpenEnabled(focusFirstCommand);
        showAt(m_anchorButton);
        if (focusFirstCommand)
            focusFirstEnabled();
    }

    void closeWithFocusRestoration(bool restore)
    {
        m_restoreFocusOnClose = restore;
        close();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event && event->type() == QEvent::KeyPress
            && focusIsInside()) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            switch (keyEvent->key()) {
            case Qt::Key_Up:
                moveRowFocus(-1);
                keyEvent->accept();
                return true;
            case Qt::Key_Down:
                moveRowFocus(1);
                keyEvent->accept();
                return true;
            case Qt::Key_Home:
                focusRowAt(0);
                keyEvent->accept();
                return true;
            case Qt::Key_End:
                focusRowAt(focusableRows().size() - 1);
                keyEvent->accept();
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Space: {
                CommandPresenter* row = focusedRow();
                if (!row)
                    break;
                keyEvent->accept();
                row->click();
                return true;
            }
            case Qt::Key_Escape:
                m_restoreFocusOnClose = true;
                break;
            default:
                break;
            }
        }

        if (event && event->type() == QEvent::MouseButtonPress
            && (isOpen() || isVisible())) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint globalPosition =
                fluentMouseGlobalPos(mouseEvent);
            const QPoint localPosition =
                mapFromGlobal(globalPosition);
            if (!overlay::visibleCardContains(
                    rect(), localPosition)) {
                const bool hitAnchor =
                    m_anchorButton
                    && m_anchorButton->isVisible()
                    && m_anchorButton->rect().contains(
                        m_anchorButton->mapFromGlobal(
                            globalPosition));
                m_restoreFocusOnClose = hitAnchor;
                if (hitAnchor && m_anchorPressHandler)
                    m_anchorPressHandler();
            }
        }

        return dialogs_flyouts::Flyout::eventFilter(
            watched, event);
    }

    QPoint computePosition() const override
    {
        if (!m_anchorButton || !m_anchorButton->window())
            return dialogs_flyouts::Flyout::computePosition();

        QWidget* topLevel = m_anchorButton->window();
        const QPoint anchorTopLeft =
            m_anchorButton->mapTo(topLevel, QPoint());
        const QRect anchorRect(
            anchorTopLeft, m_anchorButton->size());
        const QSize cardSize =
            overlay::visibleCardSize(size());
        const QRect surface =
            overlay::overlaySurfaceRect(topLevel);

        int cardX = layoutDirection() == Qt::RightToLeft
            ? anchorRect.left()
            : anchorRect.right() + 1 - cardSize.width();
        int cardY = anchorRect.bottom() + 1 + anchorOffset();
        const int aboveY =
            anchorRect.top() - anchorOffset() - cardSize.height();
        const int spaceBelow =
            surface.bottom() - anchorRect.bottom();
        const int spaceAbove =
            anchorRect.top() - surface.top();
        if (spaceBelow < cardSize.height() + anchorOffset()
            && spaceAbove > spaceBelow) {
            cardY = aboveY;
        }

        const QPoint cardTopLeft =
            overlay::clampCardTopLeft(
                QPoint(cardX, cardY),
                cardSize,
                surface,
                kPopupEdgeMargin);
        return overlay::outerTopLeftForVisibleCard(cardTopLeft);
    }

private:
    enum class RowKind {
        Command,
        ActionSeparator,
        GroupSeparator,
    };

    struct RowSpec {
        QPointer<QAction> action;
        RowKind kind = RowKind::Command;
    };

    static QVector<RowSpec> rowSpecs(
        const QList<QAction*>& overflowedPrimary,
        const QList<QAction*>& secondary)
    {
        QVector<RowSpec> result;
        result.reserve(
            overflowedPrimary.size() + secondary.size() + 1);

        const auto append = [&result](const QList<QAction*>& source) {
            for (QAction* action : source) {
                if (!action)
                    continue;
                RowSpec spec;
                spec.action = action;
                spec.kind = action->isSeparator()
                    ? RowKind::ActionSeparator
                    : RowKind::Command;
                result.append(spec);
            }
        };

        append(overflowedPrimary);
        if (hasCommandRows(overflowedPrimary)
            && hasCommandRows(secondary)) {
            RowSpec separator;
            separator.kind = RowKind::GroupSeparator;
            result.append(separator);
        }
        append(secondary);
        return result;
    }

    static bool sameSpecs(const QVector<RowSpec>& first,
                          const QVector<RowSpec>& second)
    {
        if (first.size() != second.size())
            return false;
        for (int index = 0; index < first.size(); ++index) {
            if (first.at(index).kind != second.at(index).kind
                || first.at(index).action.data()
                    != second.at(index).action.data()) {
                return false;
            }
        }
        return true;
    }

    void layoutRows()
    {
        if (!m_scrollView || !m_content)
            return;

        setLayoutDirection(
            m_owner ? m_owner->layoutDirection()
                    : Qt::LeftToRight);

        int desiredWidth = 180;
        int contentHeight = 0;
        for (QWidget* widget : m_rows) {
            if (!widget)
                continue;
            if (auto* presenter =
                    dynamic_cast<CommandPresenter*>(widget)) {
                presenter->setLayoutDirection(layoutDirection());
                presenter->synchronize();
                desiredWidth =
                    qMax(desiredWidth,
                         presenter->sizeHint().width());
                contentHeight += kCommandTargetExtent;
            } else {
                contentHeight += kSeparatorExtent;
            }
        }

        QWidget* topLevel = m_anchorButton
            ? m_anchorButton->window()
            : nullptr;
        const QRect surface = topLevel
            ? overlay::overlaySurfaceRect(topLevel)
            : QRect(0, 0, desiredWidth + 16, contentHeight + 16);
        const int maximumCardWidth =
            qMax(kCommandTargetExtent,
                 surface.width() - kPopupEdgeMargin * 2);
        const int maximumCardHeight =
            qMax(kCommandTargetExtent,
                 surface.height() - kPopupEdgeMargin * 2);
        const int cardWidth = qMin(
            maximumCardWidth,
            desiredWidth + kPopupInset * 2);
        const int cardHeight = qMin(
            maximumCardHeight,
            contentHeight + kPopupInset * 2);

        resize(overlay::outerSizeForVisibleCard(
            QSize(cardWidth, cardHeight),
            overlay::defaultShadowMargin()));
        const QRect cardRect =
            overlay::visibleCardRect(rect());
        const QRect scrollRect =
            cardRect.adjusted(
                kPopupInset,
                kPopupInset,
                -kPopupInset,
                -kPopupInset);
        m_scrollView->setGeometry(scrollRect);

        // Before the popup is exposed, QScrollArea can still report its
        // default 100 px viewport. Accept only a measurement that differs
        // from the card by at most a plausible vertical-scrollbar gutter.
        // zh_CN: 弹出层显示前 QScrollArea 可能仍报告默认 100 px 视口；
        // 仅接受与卡片宽度之差不超过合理垂直滚动条沟槽的测量值。
        const int measuredViewportWidth = qMax(
            0, m_scrollView->viewport()->width());
        const int maximumGutter =
            qMax(
                ::Spacing::Large,
                m_scrollView->verticalScrollBar()
                    ->sizeHint()
                    .width());
        const bool measuredWidthIsCurrent =
            measuredViewportWidth > 0
            && measuredViewportWidth <= scrollRect.width()
            && scrollRect.width() - measuredViewportWidth
                <= maximumGutter;
        const int contentWidth =
            measuredWidthIsCurrent
            ? measuredViewportWidth
            : scrollRect.width();
        m_content->setFixedSize(contentWidth, contentHeight);
        int y = 0;
        for (QWidget* widget : m_rows) {
            if (!widget)
                continue;
            const int rowHeight =
                dynamic_cast<CommandPresenter*>(widget)
                ? kCommandTargetExtent
                : kSeparatorExtent;
            widget->setGeometry(
                0, y, contentWidth, rowHeight);
            widget->show();
            widget->update();
            y += rowHeight;
        }
        m_content->update();
        m_scrollView->viewport()->update();

        if (isOpen() || isVisible()) {
            move(computePosition());
            raise();
        }
    }

    QVector<CommandPresenter*> focusableRows() const
    {
        QVector<CommandPresenter*> result;
        for (QWidget* widget : m_rows) {
            auto* presenter =
                dynamic_cast<CommandPresenter*>(widget);
            if (presenter && presenter->isVisible()
                && presenter->isEnabled()) {
                result.append(presenter);
            }
        }
        return result;
    }

    CommandPresenter* focusedRow() const
    {
        QWidget* focused = QApplication::focusWidget();
        for (CommandPresenter* row : focusableRows()) {
            if (row == focused)
                return row;
        }
        return nullptr;
    }

    QPointer<QAction> focusedAction() const
    {
        CommandPresenter* row = focusedRow();
        return row ? row->action() : nullptr;
    }

    int focusedRowIndex() const
    {
        const QVector<CommandPresenter*> rows = focusableRows();
        return rows.indexOf(focusedRow());
    }

    bool focusIsInside() const
    {
        QWidget* focused = QApplication::focusWidget();
        return focused
            && (focused == this || isAncestorOf(focused));
    }

    void focusFirstEnabled()
    {
        const QVector<CommandPresenter*> rows = focusableRows();
        if (rows.isEmpty()) {
            setFocus(Qt::PopupFocusReason);
            return;
        }
        rows.first()->setFocus(Qt::PopupFocusReason);
        if (m_scrollView)
            m_scrollView->ensureWidgetVisible(rows.first());
    }

    void focusRowAt(int index)
    {
        const QVector<CommandPresenter*> rows = focusableRows();
        if (rows.isEmpty()) {
            setFocus(Qt::OtherFocusReason);
            return;
        }
        index = qBound(0, index, rows.size() - 1);
        rows.at(index)->setFocus(Qt::OtherFocusReason);
        if (m_scrollView)
            m_scrollView->ensureWidgetVisible(rows.at(index));
    }

    void moveRowFocus(int delta)
    {
        const QVector<CommandPresenter*> rows = focusableRows();
        if (rows.isEmpty()) {
            setFocus(Qt::OtherFocusReason);
            return;
        }
        int index = rows.indexOf(focusedRow());
        if (index < 0)
            index = delta < 0 ? rows.size() : -1;
        index = (index + delta + rows.size()) % rows.size();
        focusRowAt(index);
    }

    void repairFocus(QAction* previousAction, int previousIndex)
    {
        if (!isOpen() && !isVisible())
            return;
        if (!focusIsInside())
            return;

        const QVector<CommandPresenter*> rows = focusableRows();
        if (rows.isEmpty()) {
            setFocus(Qt::OtherFocusReason);
            return;
        }
        for (int index = 0; index < rows.size(); ++index) {
            if (rows.at(index)->action() == previousAction) {
                focusRowAt(index);
                return;
            }
        }
        focusRowAt(
            previousIndex < 0
                ? 0
                : qMin(previousIndex, rows.size() - 1));
    }

    void synchronizeAssociatedActions()
    {
        QSet<QAction*> desired;
        for (const RowSpec& spec : m_specs) {
            if (spec.kind == RowKind::Command && spec.action)
                desired.insert(spec.action.data());
        }
        for (QAction* action : actions()) {
            if (action && !desired.contains(action))
                QWidget::removeAction(action);
        }
        for (QAction* action : desired) {
            if (!actions().contains(action))
                QWidget::addAction(action);
        }
    }

    void clearAssociatedActions()
    {
        const QList<QAction*> associated = actions();
        for (QAction* action : associated)
            QWidget::removeAction(action);
    }

    QPointer<CommandBar> m_owner;
    QPointer<CommandMoreButton> m_anchorButton;
    scrolling::ScrollView* m_scrollView = nullptr;
    QWidget* m_content = nullptr;
    QVector<RowSpec> m_specs;
    QVector<QWidget*> m_rows;
    ActivationHandler m_activationHandler;
    ClosedHandler m_closedHandler;
    AnchorPressHandler m_anchorPressHandler;
    bool m_restoreFocusOnClose = false;
};

struct OverflowCandidate {
    QAction* action = nullptr;
    int logicalIndex = -1;
    int priorityRank = 1;
};

struct FocusTarget {
    QPointer<QWidget> widget;
    QPointer<QAction> action;
    bool more = false;
};

} // namespace

class CommandBarPrivate final : public QObject {
public:
    explicit CommandBarPrivate(CommandBar* owner)
        : QObject(nullptr),
          q(owner),
          actions(owner)
    {
        q->setObjectName(QStringLiteral("FluentCommandBar"));
        detail::markCommandAccessibleWidget(
            q, CommandAccessibleRole::ToolbarRoot);
        q->setContentsMargins(
            kBarInset, kBarInset, kBarInset, kBarInset);
        q->setFocusPolicy(Qt::StrongFocus);
        q->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        q->setFont(
            q->themeFont(Typography::FontRole::Body).toQFont());
        q->installEventFilter(this);

        moreButton = new CommandMoreButton(q);
        moreButton->setObjectName(
            QStringLiteral("FluentCommandBar.MoreButton"));
        moreButton->setFixedSize(
            kCommandTargetExtent, kCommandTargetExtent);
        moreButton->hide();
        moreButton->installEventFilter(this);
        QObject::connect(
            moreButton,
            &QPushButton::clicked,
            q,
            [this]() {
                if (suppressNextMoreActivation) {
                    suppressNextMoreActivation = false;
                    focusFirstCommandOnNextOpen = true;
                    return;
                }
                if (q->isOverflowOpen()) {
                    closeOverflow(true);
                } else {
                    openOverflow(focusFirstCommandOnNextOpen);
                }
                focusFirstCommandOnNextOpen = true;
            });
        moreButton->setExpandedState(false, false);

        QObject::connect(
            &actions,
            &CommandActionModel::structureChanged,
            this,
            [this]() { rebuildPresenters(); });
        QObject::connect(
            &actions,
            &CommandActionModel::presentationChanged,
            this,
            [this]() { refreshPresenters(); });

        rebuildPresenters();
    }

    ~CommandBarPrivate() override
    {
        if (overflowPopup)
            delete overflowPopup.data();
    }

    QList<QAction*> overflowedActions() const
    {
        return overflowedPrimary;
    }

    bool isActionOverflowed(const QAction* action) const
    {
        return overflowedPrimary.contains(
            const_cast<QAction*>(action));
    }

    bool overflowIsOpen() const
    {
        return overflowPopup && overflowPopup->isOpen();
    }

    QSize preferredSize() const
    {
        const QList<QAction*> primarySource =
            visiblePresentableActions(
                actions, CommandActionModel::Section::Primary);
        const QList<QAction*> secondarySource =
            visiblePresentableActions(
                actions, CommandActionModel::Section::Secondary);
        const QList<QAction*> normalizedPrimary =
            normalizedProjection(
                primarySource, nonSeparatorSet(primarySource));
        const QList<QAction*> secondaryProjection =
            normalizedProjection(
                secondarySource, nonSeparatorSet(secondarySource));
        const bool showMore = hasCommandRows(secondaryProjection);
        const int contentWidth =
            projectedWidth(normalizedPrimary, showMore);
        return QSize(
            contentWidth > 0
                ? contentWidth + kBarInset * 2
                : 0,
            kCommandTargetExtent + kBarInset * 2);
    }

    QSize minimumPreferredSize() const
    {
        const QList<QAction*> primarySource =
            visiblePresentableActions(
                actions, CommandActionModel::Section::Primary);
        const QList<QAction*> secondarySource =
            visiblePresentableActions(
                actions, CommandActionModel::Section::Secondary);
        const QList<QAction*> normalizedPrimary =
            normalizedProjection(
                primarySource, nonSeparatorSet(primarySource));
        const QList<QAction*> secondaryProjection =
            normalizedProjection(
                secondarySource, nonSeparatorSet(secondarySource));
        const bool hasPrimary = hasCommandRows(normalizedPrimary);
        const bool hasSecondary = hasCommandRows(secondaryProjection);
        if (!hasPrimary && !hasSecondary) {
            return QSize(
                0, kCommandTargetExtent + kBarInset * 2);
        }
        if (dynamicOverflowEnabled) {
            return QSize(
                kCommandTargetExtent + kBarInset * 2,
                kCommandTargetExtent + kBarInset * 2);
        }

        const int contentWidth =
            projectedWidth(normalizedPrimary, hasSecondary);
        return QSize(
            contentWidth + kBarInset * 2,
            kCommandTargetExtent + kBarInset * 2);
    }

    void applyTheme()
    {
        q->setFont(
            q->themeFont(Typography::FontRole::Body).toQFont());
        if (moreButton)
            moreButton->onThemeUpdated();
        for (QWidget* widget : primaryPresenters) {
            if (auto* presenter =
                    dynamic_cast<CommandPresenter*>(widget)) {
                presenter->onThemeUpdated();
            } else if (auto* divider =
                           dynamic_cast<layout::Divider*>(widget)) {
                divider->onThemeUpdated();
            }
        }
        if (overflowPopup)
            overflowPopup->onThemeUpdated();
        refreshPresenters();
    }

    void openOverflow(bool focusFirstCommand = true)
    {
        recomputeLayout();
        if (!moreButton || !moreButton->isVisible()
            || (!hasCommandRows(overflowedPrimary)
                && !hasCommandRows(normalizedSecondary))
            || !q->isVisible()) {
            return;
        }

        ensureOverflowPopup();
        overflowPopup->setSections(
            overflowedPrimary, normalizedSecondary);
        if (!overflowPopup->hasRows())
            return;
        overflowPopup->openAtAnchor(focusFirstCommand);
    }

    void closeOverflow(bool restoreFocus)
    {
        if (!overflowPopup || !overflowPopup->isOpen())
            return;
        overflowPopup->closeWithFocusRestoration(restoreFocus);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!event)
            return QObject::eventFilter(watched, event);

        if (watched == q) {
            switch (event->type()) {
            case QEvent::Paint: {
                QPainter painter(q);
                if (backgroundVisible) {
                    const auto& colors = q->themeColorsRef();
                    const FluentElement::DesignLanguage language =
                        q->themeDesignLanguage();
                    if (language
                        == FluentElement::DesignMaterial) {
                        painter.fillRect(q->rect(), colors.bgLayerAlt);
                    } else if (language
                               == FluentElement::DesignCupertino) {
                        painter.fillRect(q->rect(), colors.bgLayer);
                        painter.setPen(colors.strokeDefault);
                        painter.drawLine(
                            q->rect().bottomLeft(),
                            q->rect().bottomRight());
                    } else {
                        painter.fillRect(q->rect(), colors.bgCanvas);
                    }
                }
                return true;
            }
            case QEvent::Resize:
            case QEvent::Show:
                recomputeLayout();
                break;
            case QEvent::LayoutDirectionChange:
            case QEvent::FontChange:
                refreshPresenters();
                break;
            case QEvent::FocusIn:
                if (!suspendFocusTracking) {
                    focusOnEntry(
                        static_cast<QFocusEvent*>(event)->reason());
                }
                break;
            case QEvent::KeyPress:
                return handleBarKey(
                    static_cast<QKeyEvent*>(event));
            default:
                break;
            }
            return QObject::eventFilter(watched, event);
        }

        if (watched == moreButton) {
            if (event->type() == QEvent::MouseButtonPress) {
                auto* mouseEvent =
                    static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    // Pointer invocation keeps focus on More and waits for
                    // hover or an explicit navigation key before highlighting
                    // an overflow command. Keyboard and programmatic opens
                    // still enter the menu at its first enabled command.
                    // zh_CN: 鼠标打开时焦点保留在 More，等待 hover 或显式
                    // 导航键后再高亮菜单项；键盘与编程打开仍进入首个可用命令。
                    focusFirstCommandOnNextOpen = false;
                }
            } else if (
                event->type() == QEvent::MouseButtonRelease
                && suppressNextMoreActivation) {
                QTimer::singleShot(
                    0,
                    q,
                    [this]() {
                        // A press on More closes the open popup before the
                        // button sees the matching release. If that release is
                        // dragged outside, QPushButton emits no click, so clear
                        // the one-shot suppression after dispatch.
                        suppressNextMoreActivation = false;
                    });
            }
        }

        if (event->type() != QEvent::FocusIn
            && event->type() != QEvent::MouseButtonPress
            && event->type() != QEvent::KeyPress) {
            return QObject::eventFilter(watched, event);
        }

        const int targetIndex = focusTargetIndex(watched);
        if (targetIndex < 0)
            return QObject::eventFilter(watched, event);

        if (event->type() == QEvent::FocusIn) {
            if (!suspendFocusTracking)
                rememberFocusTarget(targetIndex);
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
                focusTarget(targetIndex, Qt::MouseFocusReason);
        } else if (event->type() == QEvent::KeyPress) {
            return handleTargetKey(
                targetIndex, static_cast<QKeyEvent*>(event));
        }

        return QObject::eventFilter(watched, event);
    }

    CommandBar* q = nullptr;
    CommandActionModel actions;
    CommandBar::LabelPosition labelPosition =
        CommandBar::LabelPosition::Right;
    bool dynamicOverflowEnabled = true;
    bool backgroundVisible = true;

private:
    QSet<QAction*> focusableCommands(
        const QList<QAction*>& projection) const
    {
        QSet<QAction*> result;
        const QSet<QAction*> registered =
            nonSeparatorSet(actions.actions(
                CommandActionModel::Section::Primary));
        for (QAction* action : projection) {
            if (action && registered.contains(action)
                && !action->isSeparator()
                && action->isVisible() && action->isEnabled()
                && actions.isPresentable(action)) {
                result.insert(action);
            }
        }
        return result;
    }

    QAction* focusedPrimaryAction() const
    {
        QWidget* focused = QApplication::focusWidget();
        for (auto it = primaryPresenters.cbegin();
             it != primaryPresenters.cend();
             ++it) {
            if (it.value() == focused)
                return it.key();
        }
        return nullptr;
    }

    void focusPresenterDirectly(
        QWidget* widget,
        QAction* action,
        bool more,
        Qt::FocusReason reason)
    {
        if (!widget)
            return;
        lastFocusedAction = action;
        lastFocusWasMore = more;
        lastFocusedLogicalIndex = action
            ? actions.actions(
                  CommandActionModel::Section::Primary)
                  .indexOf(action)
            : -1;
        widget->setFocus(reason);
    }

    void focusNearestAllowed(
        const QSet<QAction*>& allowed,
        int logicalCenter,
        bool allowMore)
    {
        const QList<QAction*> primary = actions.actions(
            CommandActionModel::Section::Primary);
        if (logicalCenter < 0)
            logicalCenter = lastFocusedLogicalIndex;

        const auto tryLogicalIndex =
            [this, &allowed, &primary](int logicalIndex) {
                if (logicalIndex < 0
                    || logicalIndex >= primary.size()) {
                    return false;
                }
                QAction* candidate = primary.at(logicalIndex);
                QWidget* presenter =
                    primaryPresenters.value(candidate);
                if (!allowed.contains(candidate) || !presenter)
                    return false;
                focusPresenterDirectly(
                    presenter,
                    candidate,
                    false,
                    Qt::OtherFocusReason);
                return true;
            };

        if (tryLogicalIndex(logicalCenter))
            return;
        for (int distance = 1;
             distance <= primary.size();
             ++distance) {
            const int before = logicalCenter - distance;
            const int after = logicalCenter + distance;
            if (tryLogicalIndex(before)
                || tryLogicalIndex(after)) {
                return;
            }
        }

        if (allowMore && moreButton) {
            moreButton->show();
            focusPresenterDirectly(
                moreButton,
                nullptr,
                true,
                Qt::OtherFocusReason);
            return;
        }
        q->setFocus(Qt::OtherFocusReason);
    }

    void preemptInvalidFocusedPresenter(
        const QList<QAction*>& nextInline,
        bool willShowMore)
    {
        const QSet<QAction*> allowed =
            focusableCommands(nextInline);
        if (QApplication::focusWidget() == moreButton) {
            if (!willShowMore) {
                focusNearestAllowed(
                    allowed,
                    actions.actions(
                        CommandActionModel::Section::Primary)
                        .size(),
                    false);
            }
            return;
        }

        QAction* focusedAction = focusedPrimaryAction();
        if (!focusedAction)
            return;

        if (allowed.contains(focusedAction))
            return;

        const int logicalCenter = actions.actions(
            CommandActionModel::Section::Primary)
            .indexOf(focusedAction);
        focusNearestAllowed(
            allowed,
            logicalCenter,
            willShowMore);
    }

    void rebuildPresenters()
    {
        const bool wasSuspended = suspendFocusTracking;
        suspendFocusTracking = true;
        preemptInvalidFocusedPresenter(
            inlinePrimary,
            moreButton && moreButton->isVisible());
        for (QWidget* widget : primaryPresenters) {
            if (!widget)
                continue;
            widget->removeEventFilter(this);
            widget->hide();
            widget->deleteLater();
        }
        primaryPresenters.clear();
        presenterIsSeparator.clear();

        const QList<QAction*> primary = actions.actions(
            CommandActionModel::Section::Primary);
        for (QAction* action : primary) {
            if (!action)
                continue;

            QWidget* widget = nullptr;
            if (action->isSeparator()) {
                auto* divider = new layout::Divider(
                    Qt::Vertical, q);
                divider->setObjectName(
                    QStringLiteral(
                        "FluentCommandBar.PrimarySeparator"));
                divider->setLeadingInset(::Spacing::Small);
                divider->setTrailingInset(::Spacing::Small);
                widget = divider;
                presenterIsSeparator.insert(action, true);
            } else {
                auto* presenter = new CommandPresenter(
                    action,
                    CommandPresenter::Mode::Primary,
                    [this](QAction* command) {
                        activatePrimaryAction(command);
                    },
                    q);
                presenter->setObjectName(
                    QStringLiteral(
                        "FluentCommandBar.PrimaryPresenter"));
                presenter->setPrimaryLabelCollapsed(
                    labelPosition
                    == CommandBar::LabelPosition::Collapsed);
                presenter->installEventFilter(this);
                widget = presenter;
                presenterIsSeparator.insert(action, false);
            }
            widget->hide();
            primaryPresenters.insert(action, widget);
        }

        recomputeLayout();
        q->updateGeometry();
        q->update();
        suspendFocusTracking = wasSuspended;
    }

    void refreshPresenters()
    {
        const QList<QAction*> primary = actions.actions(
            CommandActionModel::Section::Primary);
        bool rebuild = primary.size() != primaryPresenters.size();
        if (!rebuild) {
            for (QAction* action : primary) {
                if (!action || !primaryPresenters.contains(action)
                    || presenterIsSeparator.value(action, false)
                        != action->isSeparator()) {
                    rebuild = true;
                    break;
                }
            }
        }
        if (rebuild) {
            rebuildPresenters();
            return;
        }

        const bool wasSuspended = suspendFocusTracking;
        suspendFocusTracking = true;
        preemptInvalidFocusedPresenter(
            inlinePrimary,
            moreButton && moreButton->isVisible());
        for (QAction* action : primary) {
            auto* presenter = dynamic_cast<CommandPresenter*>(
                primaryPresenters.value(action));
            if (!presenter)
                continue;
            presenter->setPrimaryLabelCollapsed(
                labelPosition
                == CommandBar::LabelPosition::Collapsed);
            presenter->synchronize();
        }

        recomputeLayout();
        q->updateGeometry();
        q->update();
        suspendFocusTracking = wasSuspended;
    }

    int presenterWidth(QAction* action) const
    {
        QWidget* widget = primaryPresenters.value(action);
        if (!widget)
            return 0;
        return action && action->isSeparator()
            ? kSeparatorExtent
            : qMax(kCommandTargetExtent,
                   widget->sizeHint().width());
    }

    int projectedWidth(
        const QList<QAction*>& primaryProjection,
        bool includeMore) const
    {
        int width = 0;
        int itemCount = 0;
        for (QAction* action : primaryProjection) {
            const int itemWidth = presenterWidth(action);
            if (itemWidth <= 0)
                continue;
            width += itemWidth;
            ++itemCount;
        }
        if (includeMore) {
            width += kCommandTargetExtent;
            ++itemCount;
        }
        if (itemCount > 1)
            width += (itemCount - 1) * kItemSpacing;
        return width;
    }

    void recomputeLayout()
    {
        if (recomputingLayout)
            return;
        const bool wasSuspended = suspendFocusTracking;
        suspendFocusTracking = true;
        recomputingLayout = true;

        const QVector<FocusTarget> previousTargets =
            focusTargets();
        const int previousFocusIndex =
            currentFocusTargetIndex(previousTargets);
        QWidget* focusedBefore = QApplication::focusWidget();
        const bool barHadFocus =
            focusedBefore == q
            || focusTargetIndex(focusedBefore) >= 0;

        const QList<QAction*> primarySource =
            visiblePresentableActions(
                actions, CommandActionModel::Section::Primary);
        const QList<QAction*> secondarySource =
            visiblePresentableActions(
                actions, CommandActionModel::Section::Secondary);
        const QSet<QAction*> allPrimaryCommands =
            nonSeparatorSet(primarySource);
        const QSet<QAction*> allSecondaryCommands =
            nonSeparatorSet(secondarySource);

        QSet<QAction*> inlineCommands = allPrimaryCommands;
        QSet<QAction*> overflowCommands;
        QList<QAction*> nextInline = normalizedProjection(
            primarySource, inlineCommands);
        QList<QAction*> nextSecondary = normalizedProjection(
            secondarySource, allSecondaryCommands);
        bool showMore = hasCommandRows(nextSecondary);

        const int availableWidth =
            qMax(0, q->contentsRect().width());
        if (dynamicOverflowEnabled
            && projectedWidth(nextInline, showMore)
                > availableWidth) {
            showMore = true;
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
                if (projectedWidth(nextInline, true)
                    <= availableWidth) {
                    break;
                }
            }
        }

        QList<QAction*> nextOverflow;
        if (dynamicOverflowEnabled) {
            nextOverflow = normalizedProjection(
                primarySource, overflowCommands);
        }
        showMore =
            hasCommandRows(nextOverflow)
            || hasCommandRows(nextSecondary);

        preemptInvalidFocusedPresenter(nextInline, showMore);
        inlinePrimary = nextInline;
        normalizedSecondary = nextSecondary;
        const bool overflowChanged =
            overflowedPrimary != nextOverflow;
        overflowedPrimary = nextOverflow;

        QSet<QAction*> inlinePresentation;
        for (QAction* action : inlinePrimary)
            inlinePresentation.insert(action);
        for (auto it = primaryPresenters.cbegin();
             it != primaryPresenters.cend();
             ++it) {
            if (it.value()
                && !inlinePresentation.contains(it.key())) {
                it.value()->hide();
            }
        }

        const QRect contentRect = q->contentsRect();
        const int y = contentRect.top()
            + (contentRect.height() - kCommandTargetExtent) / 2;
        if (q->layoutDirection() == Qt::LeftToRight) {
            int x = contentRect.left();
            for (QAction* action : inlinePrimary) {
                QWidget* widget =
                    primaryPresenters.value(action);
                if (!widget)
                    continue;
                const int width = presenterWidth(action);
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
            int x = contentRect.right() + 1;
            for (QAction* action : inlinePrimary) {
                QWidget* widget =
                    primaryPresenters.value(action);
                if (!widget)
                    continue;
                const int width = presenterWidth(action);
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
        moreButton->setExpandedState(
            overflowPopup && overflowPopup->isOpen(),
            showMore);

        if (showMore
            && availableWidth < kCommandTargetExtent
            && !impossibleGeometryWarningIssued) {
            impossibleGeometryWarningIssued = true;
            qCWarning(logging::commandBarCategory)
                << "CommandBar geometry is narrower than its"
                << "minimum More target"
                << "availableWidth=" << availableWidth
                << "minimumWidth=" << kCommandTargetExtent;
        }

        refreshOverflowPopup(showMore);
        repairBarFocus(previousFocusIndex, barHadFocus);

        recomputingLayout = false;
        suspendFocusTracking = wasSuspended;
        if (overflowChanged)
            emit q->overflowedPrimaryActionsChanged();
    }

    void refreshOverflowPopup(bool showMore)
    {
        if (!overflowPopup)
            return;
        if (!showMore) {
            if (overflowPopup->isOpen())
                overflowPopup->closeWithFocusRestoration(false);
            return;
        }
        overflowPopup->setSections(
            overflowedPrimary, normalizedSecondary);
        if (!overflowPopup->hasRows()
            && overflowPopup->isOpen()) {
            overflowPopup->closeWithFocusRestoration(false);
        }
    }

    void ensureOverflowPopup()
    {
        if (overflowPopup)
            return;

        overflowPopup = new CommandBarOverflowPopup(
            q,
            moreButton,
            [this](QAction* action) {
                activateOverflowAction(action);
            },
            [this](bool restoreFocus) {
                if (restoreFocus && moreButton
                    && moreButton->isVisible()) {
                    const int index =
                        focusTargetIndex(moreButton);
                    if (index >= 0)
                        focusTarget(
                            index, Qt::PopupFocusReason);
                }
            },
            [this]() {
                suppressNextMoreActivation = true;
            });
        QObject::connect(
            overflowPopup,
            &dialogs_flyouts::Popup::isOpenChanged,
            q,
            [this](bool open) {
                if (moreButton) {
                    moreButton->setExpandedState(
                        open, moreButton->isVisible());
                }
                emit q->overflowOpenChanged(open);
            });
        overflowPopup->setSections(
            overflowedPrimary, normalizedSecondary);
    }

    void activatePrimaryAction(QAction* action)
    {
        if (!action || !action->isEnabled()
            || !action->isVisible()) {
            return;
        }
        const QPointer<CommandBar> ownerGuard = q;
        const QPointer<QAction> actionGuard = action;
        actionGuard->trigger();
        if (!ownerGuard || !actionGuard)
            return;
    }

    void activateOverflowAction(QAction* action)
    {
        if (!action || !action->isEnabled()
            || !action->isVisible()) {
            return;
        }

        QPointer<CommandBar> ownerGuard = q;
        QPointer<QAction> actionGuard = action;
        QPointer<CommandBarOverflowPopup> popupGuard =
            overflowPopup;
        actionGuard->trigger();
        if (!ownerGuard || !popupGuard)
            return;

        QWidget* focusedAfter = QApplication::focusWidget();
        const bool focusRedirected =
            focusedAfter
            && focusedAfter != popupGuard
            && !popupGuard->isAncestorOf(focusedAfter);
        popupGuard->closeWithFocusRestoration(
            !focusRedirected);
    }

    QVector<FocusTarget> focusTargets() const
    {
        QVector<FocusTarget> result;
        const auto appendAction =
            [this, &result](QAction* action) {
                QWidget* widget =
                    primaryPresenters.value(action);
                if (!action || action->isSeparator()
                    || !widget || !widget->isVisible()
                    || !action->isEnabled()) {
                    return;
                }
                FocusTarget target;
                target.widget = widget;
                target.action = action;
                result.append(target);
            };

        if (q->layoutDirection() == Qt::RightToLeft
            && moreButton && moreButton->isVisible()) {
            FocusTarget more;
            more.widget = moreButton;
            more.more = true;
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
            && moreButton && moreButton->isVisible()) {
            FocusTarget more;
            more.widget = moreButton;
            more.more = true;
            result.append(more);
        }
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

    static int currentFocusTargetIndex(
        const QVector<FocusTarget>& targets)
    {
        QWidget* focused = QApplication::focusWidget();
        for (int index = 0; index < targets.size(); ++index) {
            if (targets.at(index).widget.data() == focused)
                return index;
        }
        return -1;
    }

    void rememberFocusTarget(int index)
    {
        const QVector<FocusTarget> targets = focusTargets();
        if (index < 0 || index >= targets.size())
            return;
        lastFocusedAction = targets.at(index).action;
        lastFocusWasMore = targets.at(index).more;
        lastFocusedLogicalIndex =
            targets.at(index).action
            ? actions.actions(
                  CommandActionModel::Section::Primary)
                  .indexOf(targets.at(index).action.data())
            : -1;
    }

    void focusTarget(int index, Qt::FocusReason reason)
    {
        const QVector<FocusTarget> targets = focusTargets();
        if (index < 0 || index >= targets.size())
            return;
        rememberFocusTarget(index);
        QWidget* widget = targets.at(index).widget.data();
        if (widget)
            widget->setFocus(reason);
    }

    void repairBarFocus(int previousIndex, bool hadFocus)
    {
        const quint64 repairRevision = ++focusRepairRevision;
        const QVector<FocusTarget> targets = focusTargets();
        if (targets.isEmpty()) {
            lastFocusedAction.clear();
            lastFocusWasMore = false;
            lastFocusedLogicalIndex = -1;
            if (hadFocus)
                q->setFocus(Qt::OtherFocusReason);
            return;
        }

        int selected = -1;
        for (int index = 0; index < targets.size(); ++index) {
            if ((lastFocusWasMore && targets.at(index).more)
                || (!lastFocusWasMore
                    && lastFocusedAction
                    && targets.at(index).action
                        == lastFocusedAction)) {
                selected = index;
                break;
            }
        }
        if (selected < 0) {
            const QList<QAction*> primary = actions.actions(
                CommandActionModel::Section::Primary);
            const int logicalCenter =
                lastFocusedAction
                ? primary.indexOf(lastFocusedAction.data())
                : lastFocusedLogicalIndex;
            if (!lastFocusWasMore && logicalCenter >= 0) {
                for (int distance = 1;
                     distance < primary.size();
                     ++distance) {
                    const int before = logicalCenter - distance;
                    const int after = logicalCenter + distance;
                    for (int logicalIndex : {before, after}) {
                        if (logicalIndex < 0
                            || logicalIndex >= primary.size()) {
                            continue;
                        }
                        QAction* candidate =
                            primary.at(logicalIndex);
                        for (int index = 0;
                             index < targets.size();
                             ++index) {
                            if (targets.at(index).action
                                == candidate) {
                                selected = index;
                                break;
                            }
                        }
                        if (selected >= 0)
                            break;
                    }
                    if (selected >= 0)
                        break;
                }
            }
        }
        const bool hadRememberedTarget =
            lastFocusWasMore
            || !lastFocusedAction.isNull()
            || lastFocusedLogicalIndex >= 0
            || previousIndex >= 0;
        if (selected < 0 && hadRememberedTarget) {
            for (int index = 0; index < targets.size(); ++index) {
                if (targets.at(index).more) {
                    selected = index;
                    break;
                }
            }
        }
        if (selected < 0) {
            selected = previousIndex < 0
                ? 0
                : qMin(previousIndex, targets.size() - 1);
        }
        if (selected >= 0) {
            lastFocusedAction = targets.at(selected).action;
            lastFocusWasMore = targets.at(selected).more;
            lastFocusedLogicalIndex =
                targets.at(selected).action
                ? actions.actions(
                      CommandActionModel::Section::Primary)
                      .indexOf(targets.at(selected).action.data())
                : -1;
        }

        if (hadFocus) {
            focusTarget(selected, Qt::OtherFocusReason);
            const QPointer<QAction> desiredAction =
                targets.at(selected).action;
            const bool desiredMore = targets.at(selected).more;
            QTimer::singleShot(
                0,
                q,
                [this,
                 repairRevision,
                 desiredAction,
                 desiredMore]() {
                    if (repairRevision != focusRepairRevision)
                        return;
                    if (overflowPopup && overflowPopup->isOpen())
                        return;
                    QWidget* focused =
                        QApplication::focusWidget();
                    if (focused && focused != q
                        && focusTargetIndex(focused) < 0) {
                        return;
                    }
                    const QVector<FocusTarget> currentTargets =
                        focusTargets();
                    for (int index = 0;
                         index < currentTargets.size();
                         ++index) {
                        if ((desiredMore
                             && currentTargets.at(index).more)
                            || (!desiredMore
                                && desiredAction
                                && currentTargets.at(index).action
                                    == desiredAction)) {
                            focusTarget(
                                index, Qt::OtherFocusReason);
                            return;
                        }
                    }
                });
        }
    }

    void focusOnEntry(Qt::FocusReason reason)
    {
        const QVector<FocusTarget> targets = focusTargets();
        if (targets.isEmpty())
            return;

        int selected = 0;
        for (int index = 0; index < targets.size(); ++index) {
            if ((lastFocusWasMore && targets.at(index).more)
                || (!lastFocusWasMore
                    && lastFocusedAction
                    && targets.at(index).action
                        == lastFocusedAction)) {
                selected = index;
                break;
            }
        }
        focusTarget(selected, reason);
    }

    bool moveFocusOutside(bool forward)
    {
        if (!QApplication::focusWidget())
            return false;

        QWidget* candidate = q;
        do {
            candidate = forward
                ? candidate->nextInFocusChain()
                : candidate->previousInFocusChain();
            if (!candidate || candidate == q)
                break;
            if (candidate == q || q->isAncestorOf(candidate))
                continue;
            if (!candidate->isVisible() || !candidate->isEnabled())
                continue;
            if (!(candidate->focusPolicy() & Qt::TabFocus))
                continue;
            candidate->setFocus(
                forward ? Qt::TabFocusReason
                        : Qt::BacktabFocusReason);
            return true;
        } while (candidate != q);
        return false;
    }

    bool handleBarKey(QKeyEvent* event)
    {
        if (!event)
            return false;
        const QVector<FocusTarget> targets = focusTargets();
        if (targets.isEmpty())
            return false;
        focusTarget(
            event->key() == Qt::Key_End
                ? targets.size() - 1
                : 0,
            Qt::OtherFocusReason);
        event->accept();
        return true;
    }

    bool handleTargetKey(int currentIndex, QKeyEvent* event)
    {
        if (!event)
            return false;
        const QVector<FocusTarget> targets = focusTargets();
        if (currentIndex < 0 || currentIndex >= targets.size())
            return false;

        int destination = currentIndex;
        switch (event->key()) {
        case Qt::Key_Left:
            destination = qMax(0, currentIndex - 1);
            break;
        case Qt::Key_Right:
            destination =
                qMin(targets.size() - 1, currentIndex + 1);
            break;
        case Qt::Key_Home:
            destination = 0;
            break;
        case Qt::Key_End:
            destination = targets.size() - 1;
            break;
        case Qt::Key_Down:
            if (targets.at(currentIndex).more) {
                openOverflow();
                event->accept();
                return true;
            }
            return false;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space: {
            auto* button = qobject_cast<QAbstractButton*>(
                targets.at(currentIndex).widget.data());
            if (!button)
                return false;
            event->accept();
            button->click();
            return true;
        }
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            if (moveFocusOutside(
                    event->key() != Qt::Key_Backtab
                    && !event->modifiers().testFlag(
                        Qt::ShiftModifier))) {
                event->accept();
                return true;
            }
            return false;
        default:
            return false;
        }

        focusTarget(destination, Qt::OtherFocusReason);
        event->accept();
        return true;
    }

    CommandMoreButton* moreButton = nullptr;
    QHash<QAction*, QWidget*> primaryPresenters;
    QHash<QAction*, bool> presenterIsSeparator;
    QList<QAction*> inlinePrimary;
    QList<QAction*> overflowedPrimary;
    QList<QAction*> normalizedSecondary;
    QPointer<CommandBarOverflowPopup> overflowPopup;
    QPointer<QAction> lastFocusedAction;
    int lastFocusedLogicalIndex = -1;
    bool lastFocusWasMore = false;
    bool suppressNextMoreActivation = false;
    bool focusFirstCommandOnNextOpen = true;
    bool suspendFocusTracking = false;
    bool recomputingLayout = false;
    bool impossibleGeometryWarningIssued = false;
    quint64 focusRepairRevision = 0;
};

CommandBar::CommandBar(QWidget* parent)
    : QWidget(parent),
      d(new CommandBarPrivate(this))
{
}

CommandBar::~CommandBar()
{
    delete d;
}

void CommandBar::addAction(QAction* action)
{
    if (d->actions.contains(CommandActionModel::Section::Primary, action)
        || d->actions.contains(
            CommandActionModel::Section::Secondary, action)) {
        return;
    }
    d->actions.add(CommandActionModel::Section::Primary, action);
}

void CommandBar::insertAction(QAction* before, QAction* action)
{
    if (d->actions.contains(CommandActionModel::Section::Primary, action)
        || d->actions.contains(
            CommandActionModel::Section::Secondary, action)) {
        return;
    }
    if (d->actions.contains(CommandActionModel::Section::Primary, before)) {
        d->actions.insert(
            CommandActionModel::Section::Primary, before, action);
        return;
    }
    d->actions.add(CommandActionModel::Section::Primary, action);
}

void CommandBar::removeAction(QAction* action)
{
    if (d->actions.remove(action))
        return;
    QWidget::removeAction(action);
}

bool CommandBar::addPrimaryAction(QAction* action)
{
    return d->actions.add(CommandActionModel::Section::Primary, action);
}

bool CommandBar::insertPrimaryAction(QAction* before, QAction* action)
{
    return d->actions.insert(
        CommandActionModel::Section::Primary, before, action);
}

bool CommandBar::addSecondaryAction(QAction* action)
{
    return d->actions.add(CommandActionModel::Section::Secondary, action);
}

bool CommandBar::insertSecondaryAction(QAction* before, QAction* action)
{
    return d->actions.insert(
        CommandActionModel::Section::Secondary, before, action);
}

bool CommandBar::removeCommandAction(QAction* action)
{
    return d->actions.remove(action);
}

void CommandBar::clearPrimaryActions()
{
    d->actions.clear(CommandActionModel::Section::Primary);
}

void CommandBar::clearSecondaryActions()
{
    d->actions.clear(CommandActionModel::Section::Secondary);
}

QList<QAction*> CommandBar::primaryActions() const
{
    return d->actions.actions(CommandActionModel::Section::Primary);
}

QList<QAction*> CommandBar::secondaryActions() const
{
    return d->actions.actions(CommandActionModel::Section::Secondary);
}

QList<QAction*> CommandBar::overflowedPrimaryActions() const
{
    return d->overflowedActions();
}

bool CommandBar::isPrimaryActionOverflowed(const QAction* action) const
{
    return d->isActionOverflowed(action);
}

CommandBar::LabelPosition CommandBar::labelPosition() const
{
    return d->labelPosition;
}

void CommandBar::setLabelPosition(LabelPosition position)
{
    if (d->labelPosition == position)
        return;
    d->labelPosition = position;
    d->applyTheme();
    emit labelPositionChanged(position);
}

bool CommandBar::isDynamicOverflowEnabled() const
{
    return d->dynamicOverflowEnabled;
}

void CommandBar::setDynamicOverflowEnabled(bool enabled)
{
    if (d->dynamicOverflowEnabled == enabled)
        return;
    d->dynamicOverflowEnabled = enabled;
    d->applyTheme();
    emit dynamicOverflowEnabledChanged(enabled);
}

bool CommandBar::isOverflowOpen() const
{
    return d->overflowIsOpen();
}

bool CommandBar::backgroundVisible() const
{
    return d->backgroundVisible;
}

void CommandBar::setBackgroundVisible(bool visible)
{
    if (d->backgroundVisible == visible)
        return;
    d->backgroundVisible = visible;
    update();
    emit backgroundVisibleChanged(visible);
}

void CommandBar::onThemeUpdated()
{
    d->applyTheme();
}

QSize CommandBar::sizeHint() const
{
    return d->preferredSize();
}

QSize CommandBar::minimumSizeHint() const
{
    return d->minimumPreferredSize();
}

void CommandBar::setOverflowOpen(bool open)
{
    if (open)
        d->openOverflow();
    else
        d->closeOverflow(true);
}

void CommandBar::actionEvent(QActionEvent* event)
{
    QWidget::actionEvent(event);
    d->actions.handleActionEvent(event);
}

} // namespace fluent::menus_toolbars

#include "PipsPagerAccessibility_p.h"

#include <QAccessible>
#include <QCoreApplication>

#include "components/foundation/private/LogicalItemAccessibility_p.h"
#include "components/scrolling/PipsPager.h"

namespace fluent::scrolling::detail {

#if QT_CONFIG(accessibility)

using fluent::accessibility::detail::LogicalItemAccessibleAdapter;
using fluent::accessibility::detail::LogicalItemAccessibleState;

namespace {

QString pagerText(const char* source)
{
    return QCoreApplication::translate("PipsPagerAccessibility", source);
}

} // namespace

class PipsPagerAccessible final : public LogicalItemAccessibleAdapter {
public:
    explicit PipsPagerAccessible(PipsPager* pager)
        : LogicalItemAccessibleAdapter(pager, QAccessible::PageTabList)
    {
    }

    int logicalChildCount() const override
    {
        return view() ? view()->numberOfPages() + 2 : 0;
    }

    QAccessible::Role logicalChildRole(int index) const override
    {
        return isPage(index) ? QAccessible::PageTab : QAccessible::Button;
    }

    QString logicalChildText(int index,
                             QAccessible::Text type) const override
    {
        PipsPager* pager = view();
        if (!pager || type != QAccessible::Name)
            return {};
        if (isPage(index)) {
            return pagerText("Page %1 of %2")
                .arg(index + 1).arg(pager->numberOfPages());
        }
        if (isPrevious(index))
            return pagerText("Previous page");
        return isNext(index) ? pagerText("Next page") : QString();
    }

    QRect logicalChildRect(int index) const override
    {
        PipsPager* pager = view();
        if (!pager)
            return {};
        if (isPage(index))
            return toGlobalRect(pager->pipHitRect(index));
        if (isPrevious(index))
            return toGlobalRect(pager->previousButtonRect());
        return isNext(index)
            ? toGlobalRect(pager->nextButtonRect()) : QRect();
    }

    LogicalItemAccessibleState logicalChildState(int index) const override
    {
        LogicalItemAccessibleState result;
        PipsPager* pager = view();
        if (!pager) {
            result.valid = false;
            result.invisible = true;
            return result;
        }

        if (isPage(index)) {
            result.enabled = pager->isEnabled();
            result.selected = pager->selectedPageIndex() == index;
            result.focused = pager->hasFocus() && result.selected;
            result.offscreen = pager->pipHitRect(index).isEmpty();
            return result;
        }

        result.selectable = false;
        if (isPrevious(index)) {
            result.enabled = pager->isEnabled() && pager->hasPreviousPage();
            result.invisible = pager->previousButtonVisibility()
                == PipsPager::PipsPagerButtonVisibility::Collapsed;
            result.offscreen = !result.invisible
                && pager->previousButtonRect().isEmpty();
            return result;
        }
        if (isNext(index)) {
            result.enabled = pager->isEnabled() && pager->hasNextPage();
            result.invisible = pager->nextButtonVisibility()
                == PipsPager::PipsPagerButtonVisibility::Collapsed;
            result.offscreen = !result.invisible
                && pager->nextButtonRect().isEmpty();
            return result;
        }

        result.valid = false;
        result.invisible = true;
        return result;
    }

    int logicalFocusChild() const override
    {
        return view() && view()->hasFocus() && view()->numberOfPages() > 0
            ? view()->selectedPageIndex() : -1;
    }

    QStringList logicalChildActions(int index) const override
    {
        return LogicalItemAccessibleAdapter::logicalChildActions(index);
    }

    void performLogicalChildAction(
        int index, const QString& actionName) override
    {
        PipsPager* pager = view();
        if (!pager
            || actionName != QAccessibleActionInterface::pressAction()) {
            return;
        }
        if (isPage(index))
            pager->setSelectedPageIndex(index);
        else if (isPrevious(index))
            pager->goToPreviousPage();
        else if (isNext(index))
            pager->goToNextPage();
    }

    bool clearLogicalSelection() override { return false; }

private:
    PipsPager* view() const
    {
        return static_cast<PipsPager*>(ownerWidget());
    }
    bool isPage(int index) const
    {
        return view() && index >= 0 && index < view()->numberOfPages();
    }
    bool isPrevious(int index) const
    {
        return view() && index == view()->numberOfPages();
    }
    bool isNext(int index) const
    {
        return view() && index == view()->numberOfPages() + 1;
    }
};

namespace {

QAccessibleInterface* pipsPagerAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* pager = dynamic_cast<PipsPager*>(object);
    return pager ? new PipsPagerAccessible(pager) : nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensurePipsPagerAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(pipsPagerAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

} // namespace fluent::scrolling::detail

#ifndef STACKCONTENTHOST_H
#define STACKCONTENTHOST_H

#include <QMetaObject>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/WidgetOwnership.h"

class QParallelAnimationGroup;
class QResizeEvent;
class QStackedLayout;

namespace fluent::navigation {

/**
 * @brief Content host that switches stacked pages with configurable transitions.
 * zh_CN: 使用可配置过渡效果切换堆叠页面的内容宿主。
 *
 * StackContentHost provides the page-hosting side of navigation shells, exposing
 * busy state and transition effect without dictating the navigation chrome.
 * zh_CN: StackContentHost 提供导航外壳的页面承载侧能力，暴露 busy 状态和过渡效果，
 * 但不规定导航 chrome。
 */
class StackContentHost : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Whether a transition or navigation operation is running.
     * zh_CN: 是否正在执行转场或导航操作。
     */
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    /**
     * @brief Page transition effect used by the content host.
     * zh_CN: 内容宿主使用的页面转场效果。
     */
    Q_PROPERTY(TransitionEffect transitionEffect READ transitionEffect WRITE setTransitionEffect NOTIFY transitionEffectChanged)

public:
    enum class TransitionEffect {
        SlideFromLeft,
        SlideFromBottom
    };
    Q_ENUM(TransitionEffect)

    explicit StackContentHost(QWidget* parent = nullptr);
    ~StackContentHost() override;

    int count() const { return m_pages.size(); }
    int currentIndex() const { return m_currentIndex; }
    bool busy() const { return m_busy; }

    QWidget* pageWidget(int index) const;
    bool insertPage(int index, QWidget* widget);
    /**
     * @brief Inserts a page with an explicit release policy.
     * zh_CN: 使用显式释放策略插入页面。
     */
    bool insertPage(int index, QWidget* widget, WidgetOwnership ownership);
    QWidget* replacePage(int index, QWidget* widget);
    /**
     * @brief Replaces a page, applying the old policy and recording the new one.
     * zh_CN: 替换页面，执行旧页面策略并记录新页面策略。
     */
    bool replacePage(int index, QWidget* widget, WidgetOwnership ownership);
    QWidget* takePage(int index);
    /**
     * @brief Removes a page and applies its configured ownership policy.
     * zh_CN: 移除页面并执行其已配置的所有权策略。
     */
    bool releasePage(int index);
    void clearPages();
    /**
     * @brief Clears all pages and applies each configured ownership policy.
     * zh_CN: 清空全部页面并执行各自配置的所有权策略。
     */
    void releaseAllPages();
    bool movePage(int from, int to);
    int indexOf(QWidget* widget) const;
    /**
     * @brief Returns the configured release policy for a hosted page.
     * zh_CN: 返回托管页面已配置的释放策略。
     */
    WidgetOwnership pageOwnershipAt(int index) const;

    /**
     * @brief Switches the current page, optionally animating in a direction.
     * zh_CN: 切换当前页面，可按方向播放转场动画。
     *
     * direction >= 0 plays the forward transition (incoming page slides in from the
     * effect's side); direction < 0 reverses it (incoming page slides in from the
     * opposite side), matching back navigation.
     * zh_CN: direction >= 0 播放前进转场（新页面从效果方向侧滑入）；direction < 0 反向
     * 播放（新页面从相反侧滑入），对应后退导航。
     */
    void setCurrentIndex(int index, int direction = 0, bool animated = true);
    void setTransitionAnimationEnabled(bool enabled);
    bool transitionAnimationEnabled() const { return m_transitionAnimationEnabled; }
    TransitionEffect transitionEffect() const { return m_transitionEffect; }
    void setTransitionEffect(TransitionEffect effect);

    /**
     * @brief Configures the optional content surface drawn above the window backdrop.
     * zh_CN: 配置绘制在窗口背景之上的可选内容表面。
     *
     * The host remains transparent until a valid, non-transparent fill is provided,
     * so page gaps continue to reveal the shared native or UILib-painted backdrop.
     * zh_CN: 在提供有效且非透明的填充前宿主保持透明，使页面间隙继续露出共享的原生或
     * UILib 软件背景。
     */
    void setContentSurface(const QColor& fill, qreal topLeftRadius, const QColor& border);

    void onThemeUpdated() override;

signals:
    void currentIndexChanged(int index);
    void busyChanged(bool busy);
    void transitionEffectChanged(TransitionEffect effect);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    struct PageRecord {
        QPointer<QWidget> content;
        QPointer<QWidget> stackWidget;
        QWidget* identity = nullptr;
        QPointer<QWidget> originalParent;
        WidgetOwnership ownership = WidgetOwnership::Owned;
        QMetaObject::Connection destroyedConnection;
        bool placeholder = false;
    };

    bool canHostPage(QWidget* widget) const;
    PageRecord makePage(QWidget* widget, WidgetOwnership ownership);
    bool replacePageImpl(int index,
                         QWidget* widget,
                         WidgetOwnership ownership,
                         bool applyPreviousOwnership,
                         QWidget** transferredPage);
    PageRecord extractPage(int index);
    void removeStackWidget(QWidget* widget);
    void deletePlaceholder(const PageRecord& page);
    QWidget* transferPage(const PageRecord& page);
    void releasePageRecord(const PageRecord& page);
    void clearPagesImpl(bool applyOwnership);
    void handlePageDestroyed(QWidget* widget);
    void finishPageRemoval(int removedIndex);
    void discardTransitionGroup();
    void setBusy(bool busy);
    void finishTransition(int targetIndex, QWidget* toWidget);
    bool canAnimate(QWidget* fromWidget, QWidget* toWidget, bool requested) const;
    QPoint transitionStartOffset(const QRect& rect, int direction) const;
    void showOnlyStackWidget(QWidget* currentWidget);
    void normalizeCurrentIndexAfterRemoval(int removedIndex);
    QWidget* stackWidgetAt(int index) const;

    QColor m_surfaceFill;        // Invalid/transparent → host stays transparent (no panel).
    QColor m_surfaceBorder;
    qreal m_surfaceTopLeftRadius = 0.0;

    QStackedLayout* m_layout = nullptr;
    QVector<PageRecord> m_pages;
    int m_currentIndex = -1;
    bool m_busy = false;
    bool m_transitionAnimationEnabled = true;
    TransitionEffect m_transitionEffect = TransitionEffect::SlideFromLeft;
    QParallelAnimationGroup* m_transitionGroup = nullptr;
};

} // namespace fluent::navigation

Q_DECLARE_METATYPE(fluent::navigation::StackContentHost::TransitionEffect)

#endif // STACKCONTENTHOST_H

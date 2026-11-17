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
 * @brief Stacked page container with animated directional transitions.
 * Hosts page widgets independently of the navigation header or pane chrome.
 * zh_CN: 支持方向动画转场的堆叠页面容器，仅承载页面切换与过渡效果，不绑定外围导航栏样式。
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
    Q_PROPERTY(TransitionEffect transitionEffect
        READ transitionEffect
        WRITE setTransitionEffect
        NOTIFY transitionEffectChanged)

public:
    enum class TransitionEffect { SlideFromLeft, SlideFromBottom };
    Q_ENUM(TransitionEffect)

    explicit StackContentHost(QWidget* parent = nullptr);
    ~StackContentHost() override;

    int count() const { return m_pages.size(); }
    int currentIndex() const { return m_currentIndex; }
    bool busy() const { return m_busy; }

    QWidget* pageWidget(int index) const;
    bool insertPage(int index, QWidget* widget);
    /**
     * @brief Inserts a page with an explicit ownership policy.
     * zh_CN: 插入页面并指定所有权策略。
     */
    bool insertPage(int index, QWidget* widget, WidgetOwnership ownership);
    QWidget* replacePage(int index, QWidget* widget);
    /**
     * @brief Replaces a page, applying the previous ownership policy and recording the new one.
     * zh_CN: 替换页面，按旧页面策略处理后记录新页面的所有权策略。
     */
    bool replacePage(int index, QWidget* widget, WidgetOwnership ownership);
    QWidget* takePage(int index);
    /**
     * @brief Removes a page and applies its configured ownership policy.
     * zh_CN: 移除页面并执行其所有权策略。
     */
    bool releasePage(int index);
    void clearPages();
    /**
     * @brief Clears all pages and applies each page's ownership policy.
     * zh_CN: 清空全部页面并逐一执行各自的释放策略。
     */
    void releaseAllPages();
    bool movePage(int from, int to);
    int indexOf(QWidget* widget) const;
    /**
     * @brief Returns the ownership policy recorded for a hosted page.
     * zh_CN: 返回指定页面记录的所有权策略。
     */
    WidgetOwnership pageOwnershipAt(int index) const;

    /**
     * @brief Switches the current page, optionally animating in a direction.
     * `direction >= 0` plays the forward transition; `direction < 0` reverses it for back navigation.
     * zh_CN: 切换当前页面；direction >= 0 播放正向滑入动画，direction < 0 反向播放（对应后退导航）。
     */
    void setCurrentIndex(int index, int direction = 0, bool animated = true);
    void setTransitionAnimationEnabled(bool enabled);
    bool transitionAnimationEnabled() const { return m_transitionAnimationEnabled; }
    TransitionEffect transitionEffect() const { return m_transitionEffect; }
    void setTransitionEffect(TransitionEffect effect);

    /**
     * @brief Configures an optional background fill and border over the window backdrop.
     * Paints `fill` when valid and non-transparent; otherwise material backdrops stay transparent and regular windows use the default content layer.
     * zh_CN: 配置绘制在窗口背景之上的可选内容区填充与边框；有效且非透明时使用 fill，否则材质窗口保持透明，普通窗口使用默认内容底色。
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

    QColor m_surfaceFill; // Invalid/transparent → host stays transparent (no panel).
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

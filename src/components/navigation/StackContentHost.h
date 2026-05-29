#ifndef STACKCONTENTHOST_H
#define STACKCONTENTHOST_H

#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

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
    QWidget* replacePage(int index, QWidget* widget);
    QWidget* takePage(int index);
    void clearPages();
    bool movePage(int from, int to);

    void setCurrentIndex(int index, int direction = 0, bool animated = true);
    void setTransitionAnimationEnabled(bool enabled);
    bool transitionAnimationEnabled() const { return m_transitionAnimationEnabled; }
    TransitionEffect transitionEffect() const { return m_transitionEffect; }
    void setTransitionEffect(TransitionEffect effect);

    void onThemeUpdated() override;

signals:
    void currentIndexChanged(int index);
    void busyChanged(bool busy);
    void transitionEffectChanged(TransitionEffect effect);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    struct PageRecord {
        QPointer<QWidget> content;
        QPointer<QWidget> stackWidget;
        bool placeholder = false;
    };

    PageRecord makePage(QWidget* widget);
    void removeStackWidget(QWidget* widget);
    void deletePlaceholder(const PageRecord& page);
    void setBusy(bool busy);
    void finishTransition(int targetIndex, QWidget* toWidget);
    bool canAnimate(QWidget* fromWidget, QWidget* toWidget, bool requested) const;
    QPoint transitionStartOffset(const QRect& rect) const;
    void showOnlyStackWidget(QWidget* currentWidget);
    void normalizeCurrentIndexAfterRemoval(int removedIndex);
    QWidget* stackWidgetAt(int index) const;

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

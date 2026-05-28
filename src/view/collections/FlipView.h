#ifndef FLIPVIEW_H
#define FLIPVIEW_H

#include <QWidget>
#include <QElapsedTimer>
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

class QPropertyAnimation;

namespace view::collections {

/**
 * @brief Fluent carousel that presents one item at a time.
 * zh_CN: 一次展示一个 item 的 Fluent 轮播视图。
 *
 * FlipView manages previous/next navigation, orientation, animated transitions,
 * and optional page indicators while leaving item content to the model/delegate path.
 * zh_CN: FlipView 管理上一项/下一项导航、方向、过渡动画和可选页码指示器，
 * item 内容仍由 model/delegate 路径提供。
 */
class FlipView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Index of the currently visible item.
     * zh_CN: 当前可见条目的索引。
     */
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    /**
     * @brief Primary layout or motion orientation.
     * zh_CN: 主要布局或运动方向。
     */
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    /**
     * @brief Whether previous and next navigation buttons are visible.
     * zh_CN: 上一项和下一项导航按钮是否可见。
     */
    Q_PROPERTY(bool showNavigationButtons READ showNavigationButtons WRITE setShowNavigationButtons NOTIFY showNavigationButtonsChanged)
    /**
     * @brief Whether the page indicator is visible.
     * zh_CN: 页码指示器是否可见。
     */
    Q_PROPERTY(bool showPageIndicator READ showPageIndicator WRITE setShowPageIndicator NOTIFY showPageIndicatorChanged)
    /**
     * @brief Current slide transition offset in pixels.
     * zh_CN: 当前滑动转场偏移，单位为像素。
     */
    Q_PROPERTY(qreal slideOffset READ slideOffset WRITE setSlideOffset)

    friend class FlipViewOverlay;

public:
    explicit FlipView(QWidget* parent = nullptr);

    void onThemeUpdated() override;

    // ── 页面管理 ──
    void addPage(QWidget* page);
    void insertPage(int index, QWidget* page);
    void removePage(int index);
    QWidget* pageAt(int index) const;
    int pageCount() const;

    // ── 属性 ──
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);

    bool showNavigationButtons() const { return m_showNavButtons; }
    bool areNavigationButtonsVisible() const { return showNavigationButtons(); }
    void setShowNavigationButtons(bool show);

    bool showPageIndicator() const { return m_showPageIndicator; }
    bool isPageIndicatorVisible() const { return showPageIndicator(); }
    void setShowPageIndicator(bool show);

    qreal slideOffset() const { return m_slideOffset; }
    void setSlideOffset(qreal offset);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // ── 导航 ──
    void goNext();
    void goPrevious();

signals:
    void currentIndexChanged(int index);
    void orientationChanged();
    void showNavigationButtonsChanged();
    void showPageIndicatorChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    // ── 几何 ──
    QRect contentRect() const;
    QRect prevButtonRect() const;
    QRect nextButtonRect() const;
    QRect pageIndicatorRect() const;

    // ── 内部 ──
    void layoutPages();
    void animateSlide(int fromIndex, int toIndex);
    void drawNavButton(QPainter& p, const QRect& rect, bool isNext, bool hovered, bool pressed);
    void drawPageIndicator(QPainter& p);
    void updateMask();
    void raiseOverlay();

    QWidget* m_overlay = nullptr;
    QList<QWidget*> m_pages;
    int m_currentIndex = -1;
    Qt::Orientation m_orientation = Qt::Horizontal;
    bool m_showNavButtons = true;
    bool m_showPageIndicator = true;

    // 动画
    qreal m_slideOffset = 0.0;
    QPropertyAnimation* m_slideAnimation = nullptr;
    int m_animatingFromIndex = -1;

    // 悬停状态
    bool m_isHovered = false;
    bool m_prevBtnHovered = false;
    bool m_nextBtnHovered = false;
    bool m_prevBtnPressed = false;
    bool m_nextBtnPressed = false;

    // 滚轮/触控板
    QElapsedTimer m_wheelCooldown;
    int m_gestureAccum = 0;         // phase-based 累积
    bool m_gestureConsumed = false;  // phase-based 手势已消费
    int m_pendingFlipDir = 0;       // 动画期间排队的翻页方向 (-1=prev, 0=none, 1=next)
    int m_npAccum = 0;              // NoScrollPhase cluster 累积
    bool m_npConsumed = false;      // NoScrollPhase 当前 cluster 已消费
};

} // namespace view::collections

#endif // FLIPVIEW_H

#ifndef FLIPVIEW_H
#define FLIPVIEW_H

#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QWidget>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/WidgetOwnership.h"

class QPropertyAnimation;

namespace fluent::collections {

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
    ~FlipView() override;

    void onThemeUpdated() override;

    // ── Page management. zh_CN: 页面管理 ──
    void addPage(QWidget* page);
    /**
     * @brief Appends a page with an explicit release policy.
     * zh_CN: 使用显式释放策略追加页面。
     */
    bool addPage(QWidget* page, WidgetOwnership ownership);
    void insertPage(int index, QWidget* page);
    /**
     * @brief Inserts a page with an explicit release policy.
     * zh_CN: 使用显式释放策略插入页面。
     */
    bool insertPage(int index, QWidget* page, WidgetOwnership ownership);
    /**
     * @brief Legacy removal that transfers the page to the caller.
     * zh_CN: 兼容旧行为，移除页面并将其转交给调用方。
     */
    void removePage(int index);
    /**
     * @brief Removes a page and applies its configured ownership policy.
     * zh_CN: 移除页面并执行其已配置的所有权策略。
     */
    bool releasePage(int index);
    /**
     * @brief Removes a page without deleting it and transfers it to the caller.
     * zh_CN: 移除页面但不删除，并将其转交给调用方。
     */
    QWidget* takePage(int index);
    QWidget* pageAt(int index) const;
    int pageCount() const;
    /**
     * @brief Returns the configured release policy for a hosted page.
     * zh_CN: 返回托管页面已配置的释放策略。
     */
    WidgetOwnership pageOwnershipAt(int index) const;

    // ── Properties. zh_CN: 属性 ──
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

    // ── Navigation. zh_CN: 导航 ──
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
    struct PageRecord {
        QWidget* identity = nullptr;
        QPointer<QWidget> page;
        QPointer<QWidget> originalParent;
        WidgetOwnership ownership = WidgetOwnership::Owned;
        QMetaObject::Connection destroyedConnection;
    };

    // ── Geometry. zh_CN: 几何 ──
    QRect contentRect() const;
    QRect prevButtonRect() const;
    QRect nextButtonRect() const;
    QRect pageIndicatorRect() const;

    // ── Internals. zh_CN: 内部 ──
    void layoutPages();
    void animateSlide(int fromIndex, int toIndex);
    void drawNavButton(QPainter& p, const QRect& rect, bool isNext, bool hovered, bool pressed);
    void drawPageIndicator(QPainter& p);
    void updateMask();
    void raiseOverlay();
    int indexOfPage(const QWidget* page) const;
    PageRecord extractPageRecord(int index);
    void updateAfterPageRemoval(int index);
    void handlePageDestroyed(QWidget* page);
    bool isValidPageIndex(int index) const;

    QWidget* m_overlay = nullptr;
    QList<PageRecord> m_pages;
    int m_currentIndex = -1;
    Qt::Orientation m_orientation = Qt::Horizontal;
    bool m_showNavButtons = true;
    bool m_showPageIndicator = true;

    // Animation state. zh_CN: 动画。
    qreal m_slideOffset = 0.0;
    QPropertyAnimation* m_slideAnimation = nullptr;
    int m_animatingFromIndex = -1;

    // Hover state. zh_CN: 悬停状态。
    bool m_isHovered = false;
    bool m_prevBtnHovered = false;
    bool m_nextBtnHovered = false;
    bool m_prevBtnPressed = false;
    bool m_nextBtnPressed = false;

    // Wheel / trackpad input. zh_CN: 滚轮/触控板。
    QElapsedTimer m_wheelCooldown;
    int m_gestureAccum = 0;         // Phase-based accumulation. zh_CN: phase-based 累积。
    bool m_gestureConsumed = false;  // Phase-based gesture consumed. zh_CN: phase-based 手势已消费。
    int m_pendingFlipDir = 0;       // Flip queued during animation (-1=prev, 0=none, 1=next). zh_CN: 动画期间排队的翻页方向。
    int m_npAccum = 0;              // NoScrollPhase cluster accumulation. zh_CN: NoScrollPhase cluster 累积。
    bool m_npConsumed = false;      // Current NoScrollPhase cluster consumed. zh_CN: NoScrollPhase 当前 cluster 已消费。
    bool m_destroying = false;
};

} // namespace fluent::collections

#endif // FLIPVIEW_H

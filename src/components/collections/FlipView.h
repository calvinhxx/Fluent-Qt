#ifndef FLIPVIEW_H
#define FLIPVIEW_H

#include "compatibility/InputEventCompat.h"
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
 * @brief Fluent carousel container that displays one QWidget page at a time.
 * Manages previous/next navigation buttons, horizontal or vertical orientation,
 * slide transitions, wheel/trackpad gestures, and optional page indicator dots.
 * zh_CN: 每次展示单个 QWidget 页面的 Fluent 轮播容器。
 * 负责前后翻页按钮、横向/纵向切换、滑动转场动画、滚轮/触控板手势以及可选页码指示点。
 */
class FlipView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Index of the currently displayed page (-1 when empty).
     * zh_CN: 当前显示页面的索引（无页面时为 -1）。
     */
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    /**
     * @brief Navigation and slide direction (Horizontal or Vertical).
     * zh_CN: 翻页与滑动方向（横向或纵向）。
     */
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    /**
     * @brief Whether previous/next navigation buttons appear on hover.
     * zh_CN: 悬停时是否显示上一页/下一页按钮。
     */
    Q_PROPERTY(bool showNavigationButtons
        READ showNavigationButtons
        WRITE setShowNavigationButtons
        NOTIFY showNavigationButtonsChanged)
    /**
     * @brief Whether the page indicator dots are visible.
     * zh_CN: 是否显示页码指示点。
     */
    Q_PROPERTY(bool showPageIndicator READ showPageIndicator WRITE setShowPageIndicator NOTIFY showPageIndicatorChanged)
    /**
     * @brief Current slide transition offset in pixels.
     * zh_CN: 当前滑动过渡的像素偏移量。
     */
    Q_PROPERTY(qreal slideOffset READ slideOffset WRITE setSlideOffset)

    friend class FlipViewOverlay;

public:
    explicit FlipView(QWidget* parent = nullptr);
    ~FlipView() override;

    void onThemeUpdated() override;

    void addPage(QWidget* page);
    /**
     * @brief Appends a page widget with an explicit ownership policy.
     * zh_CN: 按指定所有权策略追加页面控件。
     */
    bool addPage(QWidget* page, WidgetOwnership ownership);
    void insertPage(int index, QWidget* page);
    /**
     * @brief Inserts a page widget at index with an explicit ownership policy.
     * zh_CN: 按指定所有权策略在 index 处插入页面控件。
     */
    bool insertPage(int index, QWidget* page, WidgetOwnership ownership);
    /**
     * @brief Legacy removal that detaches the page without deleting it (ownership returns to caller).
     * zh_CN: 兼容旧接口：移除页面但不删除，将其交还给调用方。
     */
    void removePage(int index);
    /**
     * @brief Removes the page at index and disposes of it according to its ownership policy.
     * zh_CN: 移除指定索引的页面，并按其配置的所有权策略释放或交还。
     */
    bool releasePage(int index);
    /**
     * @brief Removes and returns the page at index without deleting it.
     * zh_CN: 移除并返回指定索引的页面控件，不执行删除。
     */
    QWidget* takePage(int index);
    QWidget* pageAt(int index) const;
    int pageCount() const;
    /**
     * @brief Returns the ownership policy configured for the page at index.
     * zh_CN: 返回指定索引页面的所有权策略。
     */
    WidgetOwnership pageOwnershipAt(int index) const;

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
    void updateVisuals();
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

    // Wheel / trackpad input
    QElapsedTimer m_wheelCooldown;
    int m_gestureAccum = 0;         // Phase-based gesture accumulation
    bool m_gestureConsumed = false; // Current phase gesture already flipped
    int m_pendingFlipDir = 0;       // Queued flip during animation (-1 prev, 0 none, 1 next)
    int m_npAccum = 0;              // NoScrollPhase cluster accumulation
    bool m_npConsumed = false;      // Current NoScrollPhase cluster already flipped
    bool m_destroying = false;
};

} // namespace fluent::collections

#endif // FLIPVIEW_H

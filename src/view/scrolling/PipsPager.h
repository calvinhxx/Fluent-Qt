#ifndef PIPSPAGER_H
#define PIPSPAGER_H

#include <QRect>
#include <QSize>
#include <QWidget>

#include "compatibility/QtCompat.h"
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPropertyAnimation;

namespace view::scrolling {

/**
 * @brief Fluent page indicator with selectable pips and navigation buttons.
 * zh_CN: 带可选圆点和导航按钮的 Fluent 页码指示器。
 *
 * PipsPager controls selected page index, visible pip window, previous/next
 * button visibility, orientation, and selection animation metrics.
 * zh_CN: PipsPager 管理选中页索引、可见圆点窗口、上一页/下一页按钮可见性、方向
 * 和选中动画参数。
 */
class PipsPager : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Total number of pages represented by the pager.
     * zh_CN: 分页器表示的总页数。
     */
    Q_PROPERTY(int numberOfPages READ numberOfPages WRITE setNumberOfPages NOTIFY numberOfPagesChanged)
    /**
     * @brief Currently selected page index.
     * zh_CN: 当前选中的页索引。
     */
    Q_PROPERTY(int selectedPageIndex READ selectedPageIndex WRITE setSelectedPageIndex NOTIFY selectedPageIndexChanged)
    /**
     * @brief Maximum number of page pips visible at once.
     * zh_CN: 一次最多可见的页码圆点数量。
     */
    Q_PROPERTY(int maxVisiblePips READ maxVisiblePips WRITE setMaxVisiblePips NOTIFY maxVisiblePipsChanged)
    /**
     * @brief Primary layout or motion orientation.
     * zh_CN: 主要布局或运动方向。
     */
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    /**
     * @brief Visibility mode for the previous-page button.
     * zh_CN: 上一页按钮的可见性模式。
     */
    Q_PROPERTY(PipsPagerButtonVisibility previousButtonVisibility READ previousButtonVisibility WRITE setPreviousButtonVisibility NOTIFY previousButtonVisibilityChanged)
    /**
     * @brief Visibility mode for the next-page button.
     * zh_CN: 下一页按钮的可见性模式。
     */
    Q_PROPERTY(PipsPagerButtonVisibility nextButtonVisibility READ nextButtonVisibility WRITE setNextButtonVisibility NOTIFY nextButtonVisibilityChanged)
    /**
     * @brief Layout cell size reserved for each pip.
     * zh_CN: 每个页码圆点保留的布局单元尺寸。
     */
    Q_PROPERTY(int pipCellSize READ pipCellSize WRITE setPipCellSize NOTIFY pipCellSizeChanged)
    /**
     * @brief Diameter used by inactive pips.
     * zh_CN: 未选中页码圆点直径。
     */
    Q_PROPERTY(int inactivePipDiameter READ inactivePipDiameter WRITE setInactivePipDiameter NOTIFY inactivePipDiameterChanged)
    /**
     * @brief Diameter used by the selected pip.
     * zh_CN: 选中页码圆点直径。
     */
    Q_PROPERTY(int selectedPipDiameter READ selectedPipDiameter WRITE setSelectedPipDiameter NOTIFY selectedPipDiameterChanged)
    /**
     * @brief Size of pager navigation buttons.
     * zh_CN: 分页导航按钮尺寸。
     */
    Q_PROPERTY(int navigationButtonSize READ navigationButtonSize WRITE setNavigationButtonSize NOTIFY navigationButtonSizeChanged)
    /**
     * @brief Icon size used by pager navigation buttons.
     * zh_CN: 分页导航按钮图标尺寸。
     */
    Q_PROPERTY(int navigationIconSize READ navigationIconSize WRITE setNavigationIconSize NOTIFY navigationIconSizeChanged)
    /**
     * @brief Whether pip selection animation is enabled.
     * zh_CN: 是否启用页码圆点选中动画。
     */
    Q_PROPERTY(bool selectionAnimationEnabled READ selectionAnimationEnabled WRITE setSelectionAnimationEnabled NOTIFY selectionAnimationEnabledChanged)
    /**
     * @brief Duration of pip selection animation in milliseconds.
     * zh_CN: 页码圆点选中动画时长，单位为毫秒。
     */
    Q_PROPERTY(int selectionAnimationDuration READ selectionAnimationDuration WRITE setSelectionAnimationDuration NOTIFY selectionAnimationDurationChanged)
    /**
     * @brief Visual offset applied to the selected pip.
     * zh_CN: 选中页码圆点应用的视觉偏移。
     */
    Q_PROPERTY(qreal selectedVisualOffset READ selectedVisualOffset WRITE setSelectedVisualOffset)
    /**
     * @brief Offset of the visible pip window.
     * zh_CN: 可见页码圆点窗口的偏移。
     */
    Q_PROPERTY(qreal visibleWindowOffset READ visibleWindowOffset WRITE setVisibleWindowOffset)

public:
    enum class PipsPagerButtonVisibility {
        Collapsed,
        Visible,
        VisibleOnPointerOver
    };
    Q_ENUM(PipsPagerButtonVisibility)

    explicit PipsPager(QWidget* parent = nullptr);
    ~PipsPager() override = default;

    int numberOfPages() const { return m_numberOfPages; }
    void setNumberOfPages(int pages);

    int selectedPageIndex() const { return m_selectedPageIndex; }
    void setSelectedPageIndex(int index);

    int maxVisiblePips() const { return m_maxVisiblePips; }
    void setMaxVisiblePips(int count);

    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);

    PipsPagerButtonVisibility previousButtonVisibility() const { return m_previousButtonVisibility; }
    void setPreviousButtonVisibility(PipsPagerButtonVisibility visibility);

    PipsPagerButtonVisibility nextButtonVisibility() const { return m_nextButtonVisibility; }
    void setNextButtonVisibility(PipsPagerButtonVisibility visibility);

    int pipCellSize() const { return m_pipCellSize; }
    void setPipCellSize(int size);

    int inactivePipDiameter() const { return m_inactivePipDiameter; }
    void setInactivePipDiameter(int diameter);

    int selectedPipDiameter() const { return m_selectedPipDiameter; }
    void setSelectedPipDiameter(int diameter);

    int navigationButtonSize() const { return m_navigationButtonSize; }
    void setNavigationButtonSize(int size);

    int navigationIconSize() const { return m_navigationIconSize; }
    void setNavigationIconSize(int size);

    bool selectionAnimationEnabled() const { return m_selectionAnimationEnabled; }
    void setSelectionAnimationEnabled(bool enabled);

    int selectionAnimationDuration() const { return m_selectionAnimationDuration; }
    void setSelectionAnimationDuration(int durationMs);

    int visiblePipCount() const;
    int firstVisiblePage() const;
    QRect pipHitRect(int pageIndex) const;
    QRect previousButtonRect() const;
    QRect nextButtonRect() const;
    bool hasPreviousPage() const;
    bool hasNextPage() const;

    bool goToPreviousPage();
    bool goToNextPage();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void numberOfPagesChanged(int pages);
    void selectedPageIndexChanged(int index);
    void selectedIndexChanged(int oldIndex, int newIndex);
    void maxVisiblePipsChanged(int count);
    void orientationChanged(Qt::Orientation orientation);
    void previousButtonVisibilityChanged(PipsPagerButtonVisibility visibility);
    void nextButtonVisibilityChanged(PipsPagerButtonVisibility visibility);
    void pipCellSizeChanged(int size);
    void inactivePipDiameterChanged(int diameter);
    void selectedPipDiameterChanged(int diameter);
    void navigationButtonSizeChanged(int size);
    void navigationIconSizeChanged(int size);
    void selectionAnimationEnabledChanged(bool enabled);
    void selectionAnimationDurationChanged(int durationMs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    enum class HitKind {
        None,
        PreviousButton,
        NextButton,
        Pip
    };

    struct HitTarget {
        HitKind kind = HitKind::None;
        int pageIndex = -1;

        bool operator==(const HitTarget& other) const {
            return kind == other.kind && pageIndex == other.pageIndex;
        }

        bool operator!=(const HitTarget& other) const {
            return !(*this == other);
        }
    };

    int clampedPageIndex(int index) const;
    QSize controlSize() const;
    QRect controlRect() const;
    bool reservesPreviousButton() const;
    bool reservesNextButton() const;
    bool shouldPaintPreviousButton() const;
    bool shouldPaintNextButton() const;
    bool shouldPaintButton(PipsPagerButtonVisibility visibility, bool hasPage) const;
    static HitTarget makeTarget(HitKind kind, int pageIndex = -1);
    HitTarget hitTest(const QPoint& pos) const;
    QRect pipCellRect(int visibleOffset) const;
    QRectF pipCellRectF(qreal visibleOffset) const;
    QRectF pipsClipRect() const;
    void drawPip(QPainter& painter, int pageIndex) const;
    void drawPipAtVisualOffset(QPainter& painter, int pageIndex, qreal visualOffset) const;
    void drawSelectedPip(QPainter& painter) const;
    void drawButton(QPainter& painter, const QRect& buttonRect, bool previous) const;
    QColor pipColor(bool selected) const;
    QColor caretColor(const HitTarget& target) const;
    QColor buttonFillColor(const HitTarget& target) const;
    int pipDiameter(int pageIndex, bool selected) const;
    qreal selectedVisualOffset() const { return m_selectedVisualOffset; }
    void setSelectedVisualOffset(qreal offset);
    qreal visibleWindowOffset() const { return m_visibleWindowOffset; }
    void setVisibleWindowOffset(qreal offset);
    qreal currentSelectedVisualOffset() const;
    qreal currentVisibleWindowOffset() const;
    void syncSelectedVisualOffset();
    void syncVisibleWindowOffset();
    void animateSelectedVisualOffset(qreal fromOffset, qreal toOffset);
    void animateVisibleWindowOffset(qreal fromOffset, qreal toOffset);
    void clearInteractionState();
    void updateAccessibleText();

    int m_numberOfPages = 5;
    int m_selectedPageIndex = 0;
    int m_maxVisiblePips = 5;
    Qt::Orientation m_orientation = Qt::Horizontal;
    PipsPagerButtonVisibility m_previousButtonVisibility = PipsPagerButtonVisibility::Collapsed;
    PipsPagerButtonVisibility m_nextButtonVisibility = PipsPagerButtonVisibility::Collapsed;
    int m_pipCellSize = 12;
    int m_inactivePipDiameter = 4;
    int m_selectedPipDiameter = 6;
    int m_navigationButtonSize = 24;
    int m_navigationIconSize = 8;
    bool m_selectionAnimationEnabled = true;
    int m_selectionAnimationDuration = 250;
    qreal m_selectedVisualOffset = 0.0;
    qreal m_visibleWindowOffset = 0.0;
    QPropertyAnimation* m_selectedVisualOffsetAnimation = nullptr;
    QPropertyAnimation* m_visibleWindowOffsetAnimation = nullptr;
    bool m_controlHovered = false;
    HitTarget m_hoveredTarget;
    HitTarget m_pressedTarget;
};

} // namespace view::scrolling

Q_DECLARE_METATYPE(view::scrolling::PipsPager::PipsPagerButtonVisibility)

#endif // PIPSPAGER_H

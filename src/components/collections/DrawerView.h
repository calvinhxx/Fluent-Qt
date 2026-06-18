#ifndef DRAWERVIEW_H
#define DRAWERVIEW_H

#include <QFlags>
#include <QMargins>
#include <QPainterPath>
#include <QPointer>
#include <QRect>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QMouseEvent;
class QPaintEvent;
class QPropertyAnimation;

namespace fluent::collections {

/**
 * @brief Same-window drawer overlay with modal, dim, and drag behavior.
 * zh_CN: 支持模态、遮罩和拖拽行为的同窗口抽屉浮层。
 *
 * DrawerView hosts caller content and centralizes edge placement, light-dismiss,
 * drag margins, corner shaping, animation, and scrim participation.
 * zh_CN: DrawerView 承载调用方内容，并统一管理边缘定位、light-dismiss、拖拽边距、
 * 圆角、动画和遮罩参与方式。
 */
class DrawerView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Whether the popup, drawer, or notification surface is open.
     * zh_CN: 弹层、抽屉或通知表面是否处于打开状态。
     */
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
    /**
     * @brief Window edge from which the drawer appears.
     * zh_CN: 抽屉出现的窗口边缘。
     */
    Q_PROPERTY(DrawerEdge edge READ edge WRITE setEdge NOTIFY edgeChanged)
    /**
     * @brief Animated drawer position along its opening axis.
     * zh_CN: 抽屉沿打开方向的动画位置。
     */
    Q_PROPERTY(qreal position READ position WRITE setPosition NOTIFY positionChanged)
    /**
     * @brief Drawer length measured along the opening axis.
     * zh_CN: 抽屉沿打开方向测量的长度。
     */
    Q_PROPERTY(int drawerLength READ drawerLength WRITE setDrawerLength NOTIFY drawerLengthChanged)
    /**
     * @brief Margins removed from the owning window before drawer placement.
     * zh_CN: 抽屉定位前从所属窗口可用区域中移除的边距。
     */
    Q_PROPERTY(QMargins availableMargins READ availableMargins WRITE setAvailableMargins NOTIFY availableMarginsChanged)
    /**
     * @brief Whether the overlay blocks interaction outside itself.
     * zh_CN: 浮层是否阻止其外部交互。
     */
    Q_PROPERTY(bool modal READ isModal WRITE setModal NOTIFY modalChanged)
    /**
     * @brief Whether a dim/scrim layer is shown behind the overlay.
     * zh_CN: 浮层后方是否显示遮罩层。
     */
    Q_PROPERTY(bool dim READ isDim WRITE setDim NOTIFY dimChanged)
    /**
     * @brief Policy controlling outside-click, escape, or programmatic close behavior.
     * zh_CN: 控制外部点击、Esc 或编程关闭行为的策略。
     */
    Q_PROPERTY(ClosePolicy closePolicy READ closePolicy WRITE setClosePolicy NOTIFY closePolicyChanged)
    /**
     * @brief Whether pointer dragging can interact with the drawer.
     * zh_CN: 指针拖拽是否可以操作抽屉。
     */
    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY interactiveChanged)
    /**
     * @brief Edge margin used to begin drawer dragging.
     * zh_CN: 开始抽屉拖拽时使用的边缘区域宽度。
     */
    Q_PROPERTY(int dragMargin READ dragMargin WRITE setDragMargin NOTIFY dragMarginChanged)
    /**
     * @brief Corner radius used on the drawer outer edge.
     * zh_CN: 抽屉外侧边缘使用的圆角半径。
     */
    Q_PROPERTY(int outerCornerRadius READ outerCornerRadius WRITE setOuterCornerRadius NOTIFY outerCornerRadiusChanged)
    /**
     * @brief Whether drawer open/close transitions are animated.
     * zh_CN: 抽屉打开/关闭过程是否播放过渡动画。
     */
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    /**
     * @brief Caller-owned widget hosted as component content.
     * zh_CN: 作为组件内容承载的调用方控件。
     */
    Q_PROPERTY(QWidget* contentWidget READ contentWidget WRITE setContentWidget NOTIFY contentWidgetChanged)

public:
    enum class DrawerEdge {
        Left,
        Right,
        Top,
        Bottom
    };
    Q_ENUM(DrawerEdge)

    enum CloseFlag {
        NoAutoClose = 0,
        CloseOnPressOutside = 1 << 0,
        CloseOnEscape = 1 << 1
    };
    Q_DECLARE_FLAGS(ClosePolicy, CloseFlag)
    Q_FLAG(ClosePolicy)

    explicit DrawerView(QWidget* parent = nullptr);
    ~DrawerView() override;

    void onThemeUpdated() override;

    bool isOpen() const { return m_isOpen; }
    void setIsOpen(bool open);

    DrawerEdge edge() const { return m_edge; }
    void setEdge(DrawerEdge edge);

    qreal position() const { return m_position; }
    void setPosition(qreal position);

    int drawerLength() const { return m_drawerLength; }
    void setDrawerLength(int length);

    QMargins availableMargins() const { return m_availableMargins; }
    void setAvailableMargins(const QMargins& margins);

    bool isModal() const { return m_modal; }
    void setModal(bool modal);

    bool isDim() const { return m_dim; }
    void setDim(bool dim);

    ClosePolicy closePolicy() const { return m_closePolicy; }
    void setClosePolicy(ClosePolicy policy);

    bool isInteractive() const { return m_interactive; }
    void setInteractive(bool interactive);

    int dragMargin() const { return m_dragMargin; }
    void setDragMargin(int margin);

    int outerCornerRadius() const { return m_outerCornerRadius; }
    void setOuterCornerRadius(int radius);

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool enabled);

    QWidget* contentWidget() const { return m_contentWidget.data(); }
    void setContentWidget(QWidget* widget);

    QRect panelGeometry() const { return m_panelGeometry; }
    QRect contentGeometry() const { return m_contentGeometry; }
    QRect scrimGeometry() const { return m_scrimGeometry; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void open();
    void close();

signals:
    void isOpenChanged(bool open);
    void edgeChanged(DrawerEdge edge);
    void positionChanged(qreal position);
    void drawerLengthChanged(int length);
    void availableMarginsChanged(const QMargins& margins);
    void modalChanged(bool modal);
    void dimChanged(bool dim);
    void closePolicyChanged(ClosePolicy policy);
    void interactiveChanged(bool interactive);
    void dragMarginChanged(int margin);
    void outerCornerRadiusChanged(int radius);
    void animationEnabledChanged(bool enabled);
    void contentWidgetChanged(QWidget* widget);
    void aboutToShow();
    void aboutToHide();
    void opened();
    void closed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class TransitionTarget {
        None,
        Open,
        Closed
    };

    struct DragState {
        bool active = false;
        bool startedFromClosed = false;
        QPoint startGlobalPos;
        qreal startPosition = 0.0;
    };

    QWidget* resolveTopLevelWidget() const;
    QWidget* eventTopLevel(QObject* watched) const;
    bool eventBelongsToDrawerTopLevel(QObject* watched) const;

    void ensureApplicationEventFilter();
    void removeApplicationEventFilter();
    void installTopLevelEventFilter(QWidget* topLevel);
    void removeTopLevelEventFilter();
    void ensureAttachedToTopLevel();
    void beginVisibleTransition();
    void finalizeOpened();
    void finalizeClosed();
    void startPositionAnimation(qreal endPosition, TransitionTarget target);
    void stopAnimation();

    QRect availableRect() const;
    int availableAxisLength() const;
    int effectiveDrawerLength() const;
    QRect openPanelRect() const;
    QRect panelRectForPosition(qreal position) const;
    QPainterPath outerRoundedPanelPath(const QRectF& rect) const;
    void updateClipMask();
    void updateOverlayGeometry();
    void updateContentGeometry();
    void updateScrimGeometry();
    void updateScrimOpacity();
    void updateScrimState();
    void destroyScrim();
    void raiseOverlayStack();

    bool isPointInsidePanel(const QPoint& globalPos) const;
    bool isPointInEdgeDragArea(const QPoint& globalPos) const;
    int dragAxisValue(const QPoint& globalPos) const;
    qreal positionForDragDelta(int delta) const;
    void beginDrag(const QPoint& globalPos, bool fromClosed);
    void updateDrag(const QPoint& globalPos);
    void endDrag();
    void cancelDrag();

    bool shouldCloseOnOutsidePress() const;
    bool shouldCloseOnEscape() const;
    bool isEffectivelyNoAutoClose() const;

    static qreal normalizedPosition(qreal position);
    static bool fuzzyEqual(qreal left, qreal right);

    DrawerEdge m_edge = DrawerEdge::Left;
    bool m_isOpen = false;
    bool m_isClosing = false;
    qreal m_position = 0.0;
    int m_drawerLength = 320;
    QMargins m_availableMargins;
    bool m_modal = true;
    bool m_dim = true;
    ClosePolicy m_closePolicy = ClosePolicy(CloseOnPressOutside | CloseOnEscape);
    bool m_interactive = true;
    int m_dragMargin = 24;
    int m_outerCornerRadius = 8;
    bool m_animationEnabled = true;

    QPointer<QWidget> m_originalParent;
    QPointer<QWidget> m_topLevel;
    QPointer<QWidget> m_contentWidget;
    QPointer<QWidget> m_scrim;
    QPropertyAnimation* m_positionAnimation = nullptr;
    TransitionTarget m_transitionTarget = TransitionTarget::None;
    bool m_applicationFilterInstalled = false;
    bool m_topLevelFilterInstalled = false;

    QRect m_panelGeometry;
    QRect m_contentGeometry;
    QRect m_scrimGeometry;
    DragState m_drag;
};

} // namespace fluent::collections

Q_DECLARE_OPERATORS_FOR_FLAGS(fluent::collections::DrawerView::ClosePolicy)
Q_DECLARE_METATYPE(fluent::collections::DrawerView::DrawerEdge)
Q_DECLARE_METATYPE(fluent::collections::DrawerView::ClosePolicy)

#endif // DRAWERVIEW_H

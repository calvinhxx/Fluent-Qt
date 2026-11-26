#ifndef DRAWERVIEW_H
#define DRAWERVIEW_H

#include <QFlags>
#include <QMargins>
#include <QPainterPath>
#include <QPointer>
#include <QRect>
#include <QWidget>

#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"
#include "../foundation/WidgetOwnership.h"

class QMouseEvent;
class QPaintEvent;
class QPropertyAnimation;

namespace fluent::overlay {
class OverlayCoordinator;
}

namespace fluent::collections {

/**
 * @brief Same-window edge drawer overlay supporting modal blocking, dimming, and edge dragging.
 * Hosts a content widget and handles edge placement, light-dismiss, rounded corners, and slide animations.
 * zh_CN: 同窗口边缘抽屉浮层，支持模态遮罩、边缘拖拽滑出及轻点关闭。
 */
class DrawerView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Whether the drawer is open.
     * zh_CN: 抽屉是否处于打开状态。
     */
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
    /**
     * @brief Window edge from which the drawer slides in.
     * zh_CN: 抽屉滑出的窗口边缘。
     */
    Q_PROPERTY(DrawerEdge edge READ edge WRITE setEdge NOTIFY edgeChanged)
    /**
     * @brief Normalized slide progress along the opening axis (0.0 closed, 1.0 open).
     * zh_CN: 抽屉沿打开轴的归一化滑动进度（0.0 关闭，1.0 打开）。
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
     * @brief Scrim opacity in [0, 1]; -1 uses the theme smoke token when dim is enabled.
     * zh_CN: 遮罩透明度（0-1，-1 使用主题默认值，需开启 dim）。
     */
    Q_PROPERTY(double dimOpacity READ dimOpacity WRITE setDimOpacity NOTIFY dimOpacityChanged)
    /**
     * @brief Backdrop blur radius in [0, 32] px (0 disables blur; defaults to 4 when modal, 0 otherwise).
     * zh_CN: 抽屉背景模糊半径（0-32 px，0 为关闭；未设置时模态默认 4、非模态默认 0）。
     */
    Q_PROPERTY(double backdropBlurRadius
        READ backdropBlurRadius
        WRITE setBackdropBlurRadius
        NOTIFY backdropBlurRadiusChanged)

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
     * @brief Widget hosted inside the drawer panel.
     * zh_CN: 承载在抽屉面板中的内容控件。
     */
    Q_PROPERTY(QWidget* contentWidget READ contentWidget WRITE setContentWidget NOTIFY contentWidgetChanged)
    /**
     * @brief Disposal policy for the current content widget.
     * zh_CN: 当前内容控件的销毁方式。
     */
    Q_PROPERTY(fluent::WidgetOwnership contentOwnership READ contentOwnership NOTIFY contentOwnershipChanged)

public:
    enum class DrawerEdge { Left, Right, Top, Bottom };
    Q_ENUM(DrawerEdge)

    enum CloseFlag { NoAutoClose = 0, CloseOnPressOutside = 1 << 0, CloseOnEscape = 1 << 1 };
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

    bool isModal() const;
    void setModal(bool modal);

    bool isDim() const;
    void setDim(bool dim);

    double dimOpacity() const;
    void setDimOpacity(double opacity);
    double backdropBlurRadius() const;
    void setBackdropBlurRadius(double radius);
    /**
     * @brief Recaptures the window backdrop after background content changes.
     * zh_CN: 背景内容变化后重新采集窗口背景。
     */
    Q_SLOT void refreshBackdrop();

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

    /**
     * @brief Sets the drawer content widget (caller-managed by default, WidgetOwnership::Borrowed).
     * zh_CN: 设置抽屉内容控件（默认由应用管理销毁，即 WidgetOwnership::Borrowed）。
     */
    void setContentWidget(QWidget* widget);

    /**
     * @brief Sets the drawer content widget with an explicit WidgetOwnership disposal mode.
     * zh_CN: 设置抽屉内容控件并指定销毁方式；设置成功返回 true。
     */
    bool setContentWidget(QWidget* widget, WidgetOwnership ownership);

    /**
     * @brief Detaches and returns the current content widget without deleting it.
     * zh_CN: 移除并返回当前内容控件（不删除）。
     */
    QWidget* takeContentWidget();

    WidgetOwnership contentOwnership() const { return m_contentOwnership; }

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
    void dimOpacityChanged(double opacity);
    void backdropBlurRadiusChanged(double radius);
    void closePolicyChanged(ClosePolicy policy);
    void interactiveChanged(bool interactive);
    void dragMarginChanged(int margin);
    void outerCornerRadiusChanged(int radius);
    void animationEnabledChanged(bool enabled);
    void contentWidgetChanged(QWidget* widget);
    void contentOwnershipChanged(WidgetOwnership ownership);
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
    enum class TransitionTarget { None, Open, Closed };

    struct DragState {
        bool active = false;
        bool startedFromClosed = false;
        QPoint startGlobalPos;
        qreal startPosition = 0.0;
    };

    QWidget* resolveTopLevelWidget() const;
    QWidget* eventTopLevel(QObject* watched) const;
    bool eventBelongsToDrawerTopLevel(QObject* watched) const;
    bool isInteractionHostVisible() const;
    bool eventHidesInteractionHost(QObject* watched, QEvent* event) const;
    void deactivateForHiddenHost();

    void ensureApplicationEventFilter();
    void removeApplicationEventFilter();
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
    QWidget* releaseContentWidget(bool deleteOwned, bool restoreParent);
    void observeContentWidget(QWidget* widget, QWidget* originalParent, WidgetOwnership ownership);

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
    ClosePolicy m_closePolicy = ClosePolicy(CloseOnPressOutside | CloseOnEscape);
    bool m_interactive = true;
    int m_dragMargin = 24;
    int m_outerCornerRadius = 8;
    bool m_animationEnabled = true;

    QPointer<QWidget> m_originalParent;
    QPointer<QWidget> m_focusRestoreTarget;
    QPointer<QWidget> m_contentWidget;
    QPointer<QWidget> m_originalContentParent;
    WidgetOwnership m_contentOwnership = WidgetOwnership::Borrowed;
    QMetaObject::Connection m_contentDestroyedConnection;
    ::fluent::overlay::OverlayCoordinator* m_overlayCoordinator = nullptr;
    QPropertyAnimation* m_positionAnimation = nullptr;
    TransitionTarget m_transitionTarget = TransitionTarget::None;
    bool m_applicationFilterInstalled = false;
    bool m_hostDeactivationInProgress = false;
    bool m_openInProgress = false;

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

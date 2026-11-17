#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QFlags>
#include <QPointer>
#include <QPropertyAnimation>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"

class QEvent;
class QGraphicsOpacityEffect;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QShowEvent;

namespace fluent::overlay {
class OverlayCoordinator;
}

namespace fluent::dialogs_flyouts {

/**
 * @brief Same-window Fluent dialog host with optional smoke scrim and fade animation.
 * zh_CN: 同窗口 Fluent 对话框基类，支持可选烟雾遮罩与进出场淡入淡出。
 *
 * Hosted as a child widget inside the owning top-level window rather than a
 * separate native window; used as the base for ContentDialog and related dialogs.
 * zh_CN: 作为所属顶层窗口的子控件显示（不创建独立原生窗口），供 ContentDialog 等模态对话框复用。
 */
class Dialog : public QDialog, public FluentElement, public QMLPlus {
    Q_OBJECT

public:
    /**
     * @brief Overlay light-dismiss flags; same bits as Popup::CloseFlag.
     * zh_CN: 浮层轻关闭标志，位值与 Popup::CloseFlag 相同。
     */
    enum CloseFlag {
        NoAutoClose = 0,
        CloseOnPressOutside = 1 << 0,
        CloseOnEscape = 1 << 1,
    };
    Q_DECLARE_FLAGS(ClosePolicy, CloseFlag)
    Q_FLAG(ClosePolicy)

    /**
     * @brief Requested open state (independent of animation progress or widget visibility).
     * zh_CN: 逻辑上的打开状态（不代表动画已结束或 QWidget 当前可见）。
     */
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
    Q_PROPERTY(double animationProgress READ animationProgress WRITE setAnimationProgress)
    Q_PROPERTY(bool dragEnabled READ isDragEnabled WRITE setDragEnabled NOTIFY dragEnabledChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    /**
     * @brief Legacy combined toggle; true only while both modal and dim are enabled.
     * zh_CN: 兼容旧版遮罩开关；仅在 modal 与 dim 同时开启时为 true。
     */
    Q_PROPERTY(bool smokeEnabled READ isSmokeEnabled WRITE setSmokeEnabled NOTIFY smokeEnabledChanged)
    Q_PROPERTY(bool modal READ isModal WRITE setModal NOTIFY modalChanged)
    Q_PROPERTY(bool dim READ isDim WRITE setDim NOTIFY dimChanged)
    /**
     * @brief Scrim opacity in [0, 1]; -1 uses the theme smoke token when dim is enabled.
     * zh_CN: 遮罩透明度范围 [0, 1]，-1 使用主题烟雾值，仅在 dim 开启时生效。
     */
    Q_PROPERTY(double dimOpacity READ dimOpacity WRITE setDimOpacity NOTIFY dimOpacityChanged)
    /**
     * @brief Blur radius in logical pixels [0, 32] for the captured window backdrop (0 disables blur).
     *
     * Defaults to 4 when modal and 0 otherwise until explicitly set; explicit
     * values (including 0) persist across modality changes.
     * zh_CN: 所属窗口背景快照的模糊半径（逻辑像素 [0, 32]，0 表示关闭模糊）；
     * 未手动设置时模态默认 4、非模态默认 0，显式设置后（含 0）不再随模态改变。
     */
    Q_PROPERTY(double backdropBlurRadius
        READ backdropBlurRadius
        WRITE setBackdropBlurRadius
        NOTIFY backdropBlurRadiusChanged)

    Q_PROPERTY(ClosePolicy closePolicy READ closePolicy WRITE setClosePolicy NOTIFY closePolicyChanged)

    explicit Dialog(QWidget* parent = nullptr);
    ~Dialog() override;

    void onThemeUpdated() override;

    bool isOpen() const { return m_isOpen; }

    int shadowSize() const { return m_shadowSize; }

    void setDragEnabled(bool enabled);
    bool isDragEnabled() const { return m_dragEnabled; }

    void setSmokeEnabled(bool enabled);
    bool isSmokeEnabled() const { return isModal() && isDim(); }

    bool isModal() const;
    void setModal(bool modal);

    bool isDim() const;
    void setDim(bool dim);

    double dimOpacity() const;
    void setDimOpacity(double opacity);
    double backdropBlurRadius() const;
    void setBackdropBlurRadius(double radius);
    /**
     * @brief Recaptures the owning-window content after background changes.
     * zh_CN: 背景内容变化后重新采集所属窗口快照。
     */
    Q_SLOT void refreshBackdrop();

    ClosePolicy closePolicy() const { return m_closePolicy; }
    void setClosePolicy(ClosePolicy policy);

    void setAnimationEnabled(bool enabled);
    bool isAnimationEnabled() const { return m_animationEnabled; }

    /**
     * @brief Uses a widget as the local theme source while the dialog is shown.
     * zh_CN: 指定对话框显示时继承局部主题的来源控件。
     */
    void setThemeSource(QWidget* source);

    double animationProgress() const { return m_animationProgress; }
    void setAnimationProgress(double progress);

    void open() override;
    int exec() override;
    void done(int result) override;
    void setIsOpen(bool open);

signals:
    void isOpenChanged(bool open);
    /**
     * @brief Opening transition started. Compatibility alias: aboutToShow().
     * zh_CN: 开始打开。兼容别名：aboutToShow()。
     */
    void opening();
    void opened();
    /**
     * @brief Closing transition started. Compatibility alias: aboutToHide().
     * zh_CN: 开始关闭。兼容别名：aboutToHide()。
     */
    void closing();
    void closed();
    void aboutToShow();
    void aboutToHide();
    void dragEnabledChanged(bool enabled);
    void animationEnabledChanged(bool enabled);
    void smokeEnabledChanged(bool enabled);
    void modalChanged(bool modal);
    void dimChanged(bool dim);
    void dimOpacityChanged(double opacity);
    void backdropBlurRadiusChanged(double radius);
    void closePolicyChanged(ClosePolicy policy);

protected:
    bool isAnimating() const { return m_isAnimating; }

    /**
     * @brief Owning top-level window used for centering, theme inheritance, and the smoke scrim.
     * zh_CN: 所属顶层窗口，用于居中定位、主题继承与烟雾遮罩挂载。
     */
    QWidget* ownerWidget() const;

    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void drawShadow(QPainter& painter, const QRect& contentRect);

private:
    bool syncThemeOverrideFromSource();
    void attachToOwner();
    void prepareSurfaceSize();
    void positionInOwner();
    void setSurfaceOpacity(qreal opacity);
    void showSmokeOverlay();
    void hideSmokeOverlay();
    void updateScrimState();
    void emitOpenedIfNeeded();

    const int m_shadowSize = ::Spacing::Standard;

    ClosePolicy m_closePolicy = ClosePolicy(CloseOnEscape);
    bool m_dragEnabled = true;
    QPoint m_dragOffset;
    QPointer<QWidget> m_originalParent;
    QPointer<QWidget> m_themeSource;
    ::fluent::overlay::OverlayCoordinator* m_overlayCoordinator = nullptr;

    bool m_animationEnabled = true;
    bool m_isAnimating = false;
    bool m_isClosing = false;
    bool m_isOpen = false;
    bool m_openInProgress = false;
    bool m_openedEmitted = false;
    double m_animationProgress = 1.0;
    int m_closingResult = 0;

    QPropertyAnimation* m_animation = nullptr;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QSize m_targetSize;
    QSize m_savedMinSize;
    QSize m_savedMaxSize;
};

} // namespace fluent::dialogs_flyouts

Q_DECLARE_OPERATORS_FOR_FLAGS(fluent::dialogs_flyouts::Dialog::ClosePolicy)

#endif // DIALOG_H

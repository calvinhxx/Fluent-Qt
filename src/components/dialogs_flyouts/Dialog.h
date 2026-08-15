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
 * @brief Same-window Fluent dialog shell with optional smoke scrim and enter/exit fade.
 * zh_CN: 同窗口 Fluent 对话框外壳，可选烟雾遮罩与进出场淡入淡出。
 *
 * Dialog follows the WinUI ContentDialog model: it is a child of the owning top-level window
 * (like Popup / DrawerView), not a separate native top-level window. ContentDialog and related
 * modal surfaces reuse this shell.
 * zh_CN: Dialog 对齐 WinUI ContentDialog：作为 owning top-level 的子控件（与 Popup / DrawerView
 * 相同），而非独立原生顶层窗口。ContentDialog 等模态表面复用此外壳。
 */
class Dialog : public QDialog, public FluentElement, public QMLPlus {
    Q_OBJECT

public:
    /**
     * @brief Overlay light-dismiss flags; same bits as Popup::CloseFlag.
     * zh_CN: 浮层轻关闭标志，位值与 Popup::CloseFlag 相同。
     */
    enum CloseFlag {
        NoAutoClose         = 0,
        CloseOnPressOutside = 1 << 0,
        CloseOnEscape       = 1 << 1,
    };
    Q_DECLARE_FLAGS(ClosePolicy, CloseFlag)
    Q_FLAG(ClosePolicy)

    /**
     * @brief Logical requested open state, not animation-complete or QWidget visibility.
     * zh_CN: 逻辑请求打开态，不是动画完成态，也不是 QWidget 可见性。
     */
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
    Q_PROPERTY(double animationProgress READ animationProgress WRITE setAnimationProgress)
    Q_PROPERTY(bool dragEnabled READ isDragEnabled WRITE setDragEnabled NOTIFY dragEnabledChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    /**
     * @brief Historical smoke bundle; enabled only while modal and dim are both on.
     * zh_CN: 历史烟雾包；仅在 modal 与 dim 同时开启时视为启用。
     */
    Q_PROPERTY(bool smokeEnabled READ isSmokeEnabled WRITE setSmokeEnabled NOTIFY smokeEnabledChanged)
    Q_PROPERTY(bool modal READ isModal WRITE setModal NOTIFY modalChanged)
    Q_PROPERTY(bool dim READ isDim WRITE setDim NOTIFY dimChanged)
    Q_PROPERTY(ClosePolicy closePolicy READ closePolicy WRITE setClosePolicy NOTIFY closePolicyChanged)

    explicit Dialog(QWidget* parent = nullptr);
    ~Dialog() override;

    void onThemeUpdated() override;

    bool isOpen() const { return m_isOpen; }

    int shadowSize() const { return m_shadowSize; }

    void setDragEnabled(bool enabled);
    bool isDragEnabled() const { return m_dragEnabled; }

    void setSmokeEnabled(bool enabled);
    bool isSmokeEnabled() const { return m_modal && m_dim; }

    bool isModal() const { return m_modal; }
    void setModal(bool modal);

    bool isDim() const { return m_dim; }
    void setDim(bool dim);

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
    void closePolicyChanged(ClosePolicy policy);

protected:
    bool isAnimating() const { return m_isAnimating; }

    /**
     * @brief Owning top-level used for centering, theme inheritance, and the smoke scrim.
     * zh_CN: 用于居中、主题继承与烟雾遮罩的 owning top-level。
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

    bool m_modal = false;
    bool m_dim = false;
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
    QPropertyAnimation* m_smokeAnim = nullptr;
    QSize m_targetSize;
    QSize m_savedMinSize;
    QSize m_savedMaxSize;
};

} // namespace fluent::dialogs_flyouts

Q_DECLARE_OPERATORS_FOR_FLAGS(fluent::dialogs_flyouts::Dialog::ClosePolicy)

#endif // DIALOG_H

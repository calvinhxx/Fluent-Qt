#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QPointer>
#include <QPropertyAnimation>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "design/Spacing.h"

class QEvent;
class QGraphicsOpacityEffect;
class QHideEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QShowEvent;

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
    Q_PROPERTY(double animationProgress READ animationProgress WRITE setAnimationProgress)
    Q_PROPERTY(bool dragEnabled READ isDragEnabled WRITE setDragEnabled)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled)

public:
    explicit Dialog(QWidget* parent = nullptr);
    ~Dialog() override;

    void onThemeUpdated() override;

    int shadowSize() const { return m_shadowSize; }

    void setDragEnabled(bool enabled) { m_dragEnabled = enabled; }
    bool isDragEnabled() const { return m_dragEnabled; }

    void setSmokeEnabled(bool enabled) { m_smokeEnabled = enabled; }
    bool isSmokeEnabled() const { return m_smokeEnabled; }

    void setAnimationEnabled(bool enabled) { m_animationEnabled = enabled; }
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

    const int m_shadowSize = ::Spacing::Standard;

    bool m_smokeEnabled = false;
    bool m_dragEnabled = true;
    QPoint m_dragOffset;
    QPointer<QWidget> m_themeSource;
    ::fluent::overlay::OverlayScrim* m_smokeOverlay = nullptr;

    bool m_animationEnabled = true;
    bool m_isAnimating = false;
    bool m_isClosing = false;
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

#endif // DIALOG_H

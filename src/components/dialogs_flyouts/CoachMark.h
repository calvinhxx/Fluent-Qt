#ifndef COACHMARK_H
#define COACHMARK_H

#include <QPointer>
#include <QSize>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPaintEvent;
class QEvent;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QResizeEvent;

namespace fluent::overlay {
class OverlayCoordinator;
}

namespace fluent::dialogs_flyouts {

/**
 * @brief Same-window teaching tip with a pointer tail aimed at a target widget.
 * zh_CN: 指向目标控件的同窗口操作引导浮层。
 *
 * Hosts caller widgets via contentHost(), draws a shadowed card with tail,
 * fades in and out, and glides between targets while open. Always hosted as a
 * child of the owning top-level window.
 * zh_CN: 通过 contentHost() 承载内容，绘制带阴影和指向尾巴的卡片，支持淡入淡出及
 * 目标切换时的平移动效；始终作为所属顶层窗口的子控件显示。
 */
class CoachMark : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT
    Q_PROPERTY(double fadeOpacity READ fadeOpacity WRITE setFadeOpacity)
    Q_PROPERTY(QSize cardSize READ cardSize WRITE setCardSize)
    Q_PROPERTY(QWidget* target READ target WRITE setTarget)
    Q_PROPERTY(Placement placement READ placement WRITE setPlacement)
    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)

public:
    /**
     * @brief Legacy display mode; both values host the tip inside the owning window.
     * zh_CN: 历史显示模式；两种取值均在所属顶层窗口内显示。
     */
    enum SurfaceMode {
        TopLevelSurface,  ///< Deprecated alias of SameWindowSurface. zh_CN: SameWindowSurface 的废弃别名。
        SameWindowSurface ///< Child widget inside the owning top-level window. zh_CN: 所属顶层窗口内的子控件。
    };
    Q_ENUM(SurfaceMode)

    enum Placement {
        Auto,   ///< Resolves to Bottom. zh_CN: 解析为 Bottom。
        Top,    ///< Card above the target, tail down. zh_CN: 卡片在目标上方，尾巴朝下。
        Bottom, ///< Card below the target, tail up. zh_CN: 卡片在目标下方，尾巴朝上。
        Left,   ///< Card left of the target, tail right. zh_CN: 卡片在目标左侧，尾巴朝右。
        Right,  ///< Card right of the target, tail left. zh_CN: 卡片在目标右侧，尾巴朝左。
    };
    Q_ENUM(Placement)

    explicit CoachMark(QWidget* owner = nullptr, SurfaceMode surfaceMode = SameWindowSurface);
    ~CoachMark() override;

    QWidget* contentHost() const { return m_contentHost; }

    /**
     * @brief Always SameWindowSurface; retained for API compatibility.
     * zh_CN: 始终为 SameWindowSurface；保留以兼容既有 API。
     */
    SurfaceMode surfaceMode() const { return SameWindowSurface; }

    QSize cardSize() const { return m_cardSize; }
    void setCardSize(const QSize& size);

    QWidget* target() const { return m_target.data(); }
    void setTarget(QWidget* target);

    Placement placement() const { return m_placement; }
    void setPlacement(Placement placement);

    bool isOpen() const { return m_open; }

    void onThemeUpdated() override;

public slots:
    void open();
    void close();
    void setOpen(bool open);

signals:
    void openChanged(bool open);
    /**
     * @brief Emitted after the entrance fade completes.
     * zh_CN: 入场淡入完成后发出。
     */
    void opened();
    /**
     * @brief Emitted after the exit fade completes and the tip is hidden.
     * zh_CN: 退场淡出完成并隐藏后发出。
     */
    void closed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void queueTargetSync();
    void syncToTarget();
    bool syncThemeOverrideFromSource();
    void attachToOwnerTopLevel();
    double fadeOpacity() const;
    void setFadeOpacity(double opacity);
    void reposition(bool animated);
    QRect cardRect() const;

    QPointer<QWidget> m_owner;
    QWidget* m_contentHost = nullptr;
    ::fluent::overlay::OverlayCoordinator* m_overlayCoordinator = nullptr;
    QSize m_cardSize{330, 168};
    QPointer<QWidget> m_target;
    Placement m_placement = Auto;
    bool m_open = false;
    bool m_targetSyncPending = false;
    bool m_tailVisible = false;
    int m_tailEdge = 0;
    int m_tailCenter = 0;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QPropertyAnimation* m_fadeAnim = nullptr;
    QPropertyAnimation* m_moveAnim = nullptr;
};

} // namespace fluent::dialogs_flyouts

#endif // COACHMARK_H

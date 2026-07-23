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

namespace fluent::dialogs_flyouts {

/**
 * @brief Same-window teaching tip with a tail aimed at a target.
 * zh_CN: 指向目标的同窗口操作引导提示。
 *
 * CoachMark hosts caller content via contentHost(), paints a Fluent card + shadow + tail, fades
 * in/out, and glides between targets while open. Like TeachingTip / Popup, it is always hosted as a
 * child of the owning top-level window (WinUI-aligned same-window overlay).
 * zh_CN: CoachMark 通过 contentHost() 承载调用方内容，绘制 Fluent 卡片 + 阴影 + 尾巴，支持淡入淡出，
 * 并在打开时于目标间滑动。与 TeachingTip / Popup 一样，始终作为 owning top-level 的子控件
 * （对齐 WinUI 的同窗口浮层）。
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
     * @brief Historical surface mode; both values host the tip inside the owner top-level.
     * zh_CN: 历史 surface 模式；两种取值都把提示承载在宿主顶层窗口内。
     */
    enum SurfaceMode {
        TopLevelSurface,   ///< Deprecated alias of SameWindowSurface. zh_CN: SameWindowSurface 的废弃别名。
        SameWindowSurface  ///< Child widget inside the owner top-level. zh_CN: 宿主顶层窗口内的子控件。
    };
    Q_ENUM(SurfaceMode)

    enum Placement {
        Auto,    ///< Resolves to Bottom. zh_CN: 解析为 Bottom。
        Top,     ///< Card above the target, tail down.
        Bottom,  ///< Card below the target, tail up.
        Left,    ///< Card left of the target, tail right.
        Right,   ///< Card right of the target, tail left.
    };
    Q_ENUM(Placement)

    explicit CoachMark(QWidget* owner = nullptr,
                       SurfaceMode surfaceMode = SameWindowSurface);
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
    void opened();
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

    QWidget* m_owner = nullptr;
    QWidget* m_contentHost = nullptr;
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

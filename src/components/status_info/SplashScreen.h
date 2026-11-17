#ifndef FLUENTQT_SPLASHSCREEN_H
#define FLUENTQT_SPLASHSCREEN_H

#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QVariantAnimation;

namespace fluent::textfields {
class Label;
}

namespace fluent::status_info {
class ProgressRing;
class ProgressBar;

/**
 * @brief Reusable startup overlay covering its parent widget's content area.
 * zh_CN: 可复用的启动遮罩，覆盖父控件的内容区域。
 *
 * Shows the application icon and loading progress while blocking input inside
 * the covered widget (other windows remain interactive). Owned by its parent;
 * dismiss() hides the overlay and emits dismissed() without deleting it.
 * zh_CN: 显示应用图标与加载进度并拦截被覆盖区域内的输入（其他窗口与区域外控件仍可用）；
 * 由父控件拥有，dismiss() 后隐藏并发出 dismissed()，不会销毁自身。
 */
class SplashScreen : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(Presentation presentation READ presentation WRITE setPresentation NOTIFY presentationChanged)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle NOTIFY subtitleChanged)
    Q_PROPERTY(QWidget* transitionTarget READ transitionTarget WRITE setTransitionTarget NOTIFY transitionTargetChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool indeterminate READ isIndeterminate WRITE setIndeterminate NOTIFY indeterminateChanged)

public:
    /** @brief Branded is the default light reveal; Simple retains the centered loading ring.
     * zh_CN: Branded 默认使用柔光显影；Simple 保留居中加载环的简洁效果。
     */
    enum class Presentation { Branded, Simple };
    Q_ENUM(Presentation)

    explicit SplashScreen(QWidget* parent = nullptr);
    ~SplashScreen() override;

    Presentation presentation() const { return m_presentation; }
    /** @brief Selects the startup presentation style (ignores unknown enum values).
     * zh_CN: 选择启动画面风格（忽略未知枚举值）。
     */
    void setPresentation(Presentation presentation);
    QIcon icon() const { return m_icon; }
    void setIcon(const QIcon& icon);
    QSize iconSize() const { return m_iconSize; }
    /** @brief Sets the preferred artwork size; each dimension is at least one logical pixel.
     * zh_CN: 设置图标首选尺寸，每个维度至少为一个逻辑像素；空间不足时按比例缩小。
     */
    void setIconSize(const QSize& size);
    QString text() const;
    /** @brief Sets plain status text; long text is elided visually but remains accessible.
     * zh_CN: 设置纯文本状态；长文本显示省略，辅助功能仍能读取完整内容。
     */
    void setText(const QString& text);
    QString title() const;
    /** @brief Sets the optional plain-text brand title shown in Branded presentation.
     * zh_CN: 设置 Branded 效果中可选的纯文本品牌标题。
     */
    void setTitle(const QString& title);
    QString subtitle() const;
    /** @brief Sets the optional plain-text subtitle shown below the brand title.
     * zh_CN: 设置品牌标题下可选的纯文本副标题。
     */
    void setSubtitle(const QString& subtitle);
    QWidget* transitionTarget() const { return m_transitionTarget.data(); }
    /**
     * @brief Sets the destination widget where the icon animates during Branded dismissal.
     *
     * Full motion moves the intact icon into target's contents rectangle when target
     * is visible, outside this splash screen, and in the same top-level window;
     * otherwise dismissal fades out. Does not reparent or alter the target widget;
     * this widget and its descendants are ignored.
     * zh_CN: 设置 Branded 退场时图标飞向的目标控件。Full 动效下若调用 dismiss() 时目标
     * 在同一顶层窗口内可见且位于遮罩外，图标会平滑移入目标内容区，否则改用淡出；
     * 不会修改目标的父控件、外观或可见性，传入自身或其子控件时忽略。
     */
    void setTransitionTarget(QWidget* target);
    int progress() const { return m_progress; }
    /**
     * @brief Sets clamped determinate progress; total <= 0 selects indeterminate mode.
     * zh_CN: 设置确定进度并限制在 0–100%；total <= 0 切换为不确定模式。
     * Does not dismiss at 100%; the application decides when startup is complete.
     * zh_CN: 达到 100% 不自动关闭，由应用判断启动完成时间。
     */
    void setProgress(int done, int total);
    bool isIndeterminate() const { return m_indeterminate; }
    void setIndeterminate(bool indeterminate);
    /**
     * @brief Dismisses, hides, and emits dismissed() once (ignored when already hidden or dismissing).
     *
     * Follows MotionPolicy without waiting for the entrance animation; call show() again to reuse.
     * zh_CN: 退场并隐藏控件，完成后发出一次 dismissed()（已隐藏或正在退场时直接返回）。
     * 过渡遵循 MotionPolicy，不等待入场动画完成，关闭后可再次 show() 复用。
     */
    void dismiss();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void presentationChanged(Presentation presentation);
    void iconChanged(const QIcon& icon);
    void iconSizeChanged(const QSize& size);
    void textChanged(const QString& text);
    void titleChanged(const QString& title);
    void subtitleChanged(const QString& subtitle);
    void transitionTargetChanged(QWidget* target);
    void progressChanged(int progress);
    void indeterminateChanged(bool indeterminate);
    void dismissed();
    /** @brief Emitted when another SplashScreen covers an overlapping area and hides this one.
     *
     * Direct hide() and window minimization do not emit replaced() or dismissed().
     * zh_CN: 当重叠区域被新的 SplashScreen 接管并隐藏当前实例时发出（不触发 dismissed()，
     * 直接 hide 或窗口最小化也不触发）。
     */
    void replaced();

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    class BrandLabel;
    class LogoTransition;
    void layoutContent();
    void layoutBrandedContent();
    void startReveal();
    void refreshBrandColors();
    void refreshBrandOpacity();
    void clearLogoTransition();
    bool hasTransitionTarget() const;
    void refreshProgress(bool relayout = true);
    QRect revealUpdateRect(qreal reveal) const;
    void refreshBackgroundCache();
    void releaseInput();
    bool covers(const QWidget* widget) const;

    QIcon m_icon;
    QPixmap m_backgroundCache;
    QImage m_reflection;
    QSize m_iconSize{96, 96};
    QRect m_iconRect;
    ProgressRing* m_ring = nullptr;
    ProgressBar* m_bar = nullptr;
    QWidget* m_brandText = nullptr;
    BrandLabel* m_title = nullptr;
    BrandLabel* m_subtitle = nullptr;
    textfields::Label* m_text = nullptr;
    textfields::Label* m_percentage = nullptr;
    QGraphicsOpacityEffect* m_opacity = nullptr;
    QPropertyAnimation* m_fade = nullptr;
    QVariantAnimation* m_intro = nullptr;
    QPointer<LogoTransition> m_logoTransition;
    QPointer<QWidget> m_transitionTarget;
    QMetaObject::Connection m_targetDestroyed;
    QPointer<QWidget> m_previousFocus;
    int m_progress = 0;
    qreal m_reveal = 1.0;
    Presentation m_presentation = Presentation::Branded;
    bool m_indeterminate = true;
    bool m_dismissing = false;
    bool m_filterInstalled = false;
};

} // namespace fluent::status_info
#endif // FLUENTQT_SPLASHSCREEN_H

#ifndef FLUENTQT_SPLASHSCREEN_H
#define FLUENTQT_SPLASHSCREEN_H

#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

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
 * @brief Reusable startup overlay covering its parent widget while showing an icon and loading progress.
 * Blocks input only inside the covered area; dismiss() hides it and emits dismissed() without deleting it.
 * zh_CN: 可复用的启动遮罩，覆盖父控件以展示图标与加载进度并拦截区域内输入；dismiss() 仅隐藏并发出 dismissed()，不会销毁自身。
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
     * @brief Sets the target widget that the icon flies into during Branded dismissal.
     * With Full motion, animates smoothly if target is visible in the same window; otherwise fades out.
     * zh_CN: 设置 Branded 退场动画时图标飞向的目标控件；Full 动画且目标在窗口内可见时平滑飞入，否则直接淡出。
     */
    void setTransitionTarget(QWidget* target);
    int progress() const { return m_progress; }
    /**
     * @brief Sets determinate progress (clamped to 0–100%); total <= 0 switches to indeterminate mode.
     * Reaching 100% does not auto-dismiss; call dismiss() when startup completes.
     * zh_CN: 设置确定进度并限制在 0–100%，total <= 0 时切换为不确定模式；达到 100% 不会自动关闭，需由应用调用 dismiss()。
     */
    void setProgress(int done, int total);
    bool isIndeterminate() const { return m_indeterminate; }
    void setIndeterminate(bool indeterminate);
    /**
     * @brief Runs the dismiss transition, hides the splash screen, and emits dismissed() once.
     * Ignored when already hidden or dismissing; call show() again to reuse.
     * zh_CN: 播放退场过渡后隐藏控件并发出一次 dismissed()（已隐藏或退场中时忽略）；关闭后可再次调用 show() 复用。
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
    /** @brief Emitted when another overlapping SplashScreen replaces and hides this one.
     * Does not emit dismissed(); direct hide() or window minimization does not emit replaced().
     * zh_CN: 当重叠区域被新的 SplashScreen 接管并隐藏当前实例时发出（不会触发 dismissed()，直接调用 hide() 或窗口最小化也不会触发）。
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

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
 * @brief Reusable startup surface covering its parent widget's content area.
 * zh_CN: 可重复使用的启动遮罩，覆盖父控件内容区。
 * Set application artwork and status, show the surface, then dismiss when work finishes.
 * The parent owns the surface; dismiss hides it and emits dismissed without deleting it.
 * Input inside the covered host is blocked while visible, including during dismissal.
 * Other windows and controls outside the host remain available. No task or event loop is owned.
 * zh_CN: 设置应用图标和状态后显示，任务完成时调用 dismiss；父控件拥有它，关闭后隐藏并发送
 * dismissed，不自行销毁。显示和退场期间拦截宿主内输入，宿主外控件及其他窗口仍可用；不管理任务或事件循环。
 */
class SplashScreen : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(Presentation presentation READ presentation WRITE setPresentation NOTIFY
                   presentationChanged)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle NOTIFY subtitleChanged)
    Q_PROPERTY(QWidget* transitionTarget READ transitionTarget WRITE setTransitionTarget NOTIFY
                   transitionTargetChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(
        bool indeterminate READ isIndeterminate WRITE setIndeterminate NOTIFY indeterminateChanged)

public:
    /** @brief Branded is the default light reveal; Simple retains the centered loading ring.
     * zh_CN: Branded 默认使用柔光显影；Simple 保留居中加载环的简洁效果。
     */
    enum class Presentation { Branded, Simple };
    Q_ENUM(Presentation)

    explicit SplashScreen(QWidget* parent = nullptr);
    ~SplashScreen() override;

    Presentation presentation() const { return m_presentation; }
    /** @brief Selects the startup presentation without changing progress or ownership.
     * zh_CN: 选择启动效果，不改变进度或所有权；未知枚举值忽略。
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
     * @brief Borrows an icon holder as the destination of the Branded dismissal.
     * zh_CN: 借用图标容器作为 Branded 退场的目标。
     * Full motion moves the intact icon into the target's contents rectangle. The target
     * must be visible, outside this surface, and in the same window when dismiss is called.
     * Otherwise dismissal uses a fade. Never reparents or changes the target's appearance.
     * This surface and its descendants are ignored as targets.
     * zh_CN: Full 动效将完整图标移入目标内容矩形；调用 dismiss 时目标须可见、位于本遮罩外
     * 且属于同一窗口，否则淡出。不改变目标的父控件、外观或可见性；忽略自身及其子控件。
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
     * @brief Dismisses, hides and emits dismissed once; repeated or hidden calls do nothing.
     * zh_CN: 退场并隐藏后发送一次 dismissed；重复调用或已隐藏时不做任何操作。
     * MotionPolicy controls the transition. Show again after dismissal to reuse the surface.
     * Never waits for the decorative entrance to finish.
     * zh_CN: 过渡遵循 MotionPolicy；不等待装饰性入场完成，关闭后可再次 show 复用。
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
    /** @brief Emitted after another cover hides this one on an overlapping host.
     * zh_CN: 被重叠宿主上的新遮罩替换并隐藏后发送；不发送 dismissed，也不自动销毁。
     * Direct hide and window minimization do not emit this signal.
     * zh_CN: 直接 hide 和窗口最小化不发送此信号。
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

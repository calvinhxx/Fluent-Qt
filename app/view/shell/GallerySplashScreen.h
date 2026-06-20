#ifndef GALLERYSPLASHSCREEN_H
#define GALLERYSPLASHSCREEN_H

#include <QPixmap>
#include <QString>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QEvent;
class QPaintEvent;
class QResizeEvent;

namespace fluent::status_info {
class ProgressRing;
}

namespace fluent::gallery {

/**
 * @brief Startup splash: app logo over a loading spinner with a percentage caption, shown while
 * the early pages warm behind it. zh_CN: 启动 splash：app logo 配加载转圈与百分比文字，在前期页面于其背后预热时显示。
 *
 * Follows the Windows UI kit "Splash screen" spec (Win32 variant): a solid surface
 * with the app icon dead-centered and a 32px ProgressRing 144px below it. The widget
 * overlays the window content area (the real title bar / window controls stay live)
 * and tracks its parent's size, so callers just construct it over contentHost() and
 * call dismiss() when work is done.
 * zh_CN: 遵循 Windows UI kit「Splash screen」规范（Win32 变体）：纯色表面，app 图标居中，
 * 下方 144px 处放 32px 的 ProgressRing。它覆盖窗口内容区（真实标题栏/窗口按钮仍可用），
 * 并跟随父级尺寸，调用方只需在 contentHost() 上构造它，完成后调用 dismiss()。
 */
class GallerySplashScreen : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT

public:
    explicit GallerySplashScreen(QWidget* parent = nullptr);

    /**
     * @brief Updates the percentage caption shown under the spinner.
     * zh_CN: 更新转圈下方显示的百分比文字。
     */
    void setProgress(int done, int total);

    /** @brief Fades the splash out and deletes it. zh_CN: 淡出并销毁 splash。*/
    void dismiss();

    void onThemeUpdated() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void layoutContent();

    fluent::status_info::ProgressRing* m_spinner = nullptr;
    QPixmap m_logo;
    QString m_progressText;
    bool m_dismissing = false;
};

} // namespace fluent::gallery

#endif // GALLERYSPLASHSCREEN_H

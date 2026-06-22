#ifndef GALLERYTITLEBARCONTROLLER_H
#define GALLERYTITLEBARCONTROLLER_H

#include <functional>

#include <QObject>
#include <QStringList>

class QEvent;
class QLabel;
class QVariantAnimation;
class QWidget;

namespace fluent::windowing {
class TitleBar;
}
namespace fluent::basicinput {
class Button;
}
namespace fluent::textfields {
class AutoSuggestBox;
class Label;
}
namespace fluent::status_info {
class ToolTip;
}

namespace fluent::gallery {

/**
 * @brief Owns the gallery's custom title-bar chrome: back / menu buttons, app icon + title,
 * the search box, their Fluent tooltips, press feedback, the back-button reveal animation, and
 * the adaptive layout that reflows everything against the live bar width.
 * zh_CN: 负责画廊自定义标题栏：返回/菜单按钮、应用图标+标题、搜索框、它们的 Fluent 气泡提示、按下反馈、
 * 返回按钮显隐动画，以及按实时栏宽重排一切的自适应布局。
 *
 * Extracted from GalleryWindow to decouple title-bar concerns from the shell. The controller
 * talks back to the shell only through the small Callbacks set (navigation intents + a query
 * for the nav's minimal layout state). zh_CN: 从 GalleryWindow 抽出，使标题栏与外壳解耦；控制器仅
 * 通过少量 Callbacks（导航意图 + 查询导航是否处于最小布局）回到外壳。
 */
class GalleryTitleBarController : public QObject {
    Q_OBJECT
public:
    struct Callbacks {
        std::function<void()> onBack;                ///< Back button clicked. zh_CN: 点击返回。
        std::function<void()> onToggleNav;           ///< Menu button clicked. zh_CN: 点击菜单。
        std::function<void(const QString&)> onSearch; ///< Search submitted/chosen. zh_CN: 搜索提交/选中。
        /// True when the nav pane is in its minimal layout, where the app title/icon give way to
        /// the search box. zh_CN: 导航处于最小布局时为真，此时应用标题/图标让位给搜索框。
        std::function<bool()> isMinimalNavLayout;
    };

    GalleryTitleBarController(fluent::windowing::TitleBar* bar,
                              const QStringList& searchTitles,
                              Callbacks callbacks,
                              QObject* parent = nullptr);

    /// Reflows the leading group + search box for the current width / nav layout.
    /// zh_CN: 按当前宽度/导航布局重排前导组与搜索框。
    void updateLayout();

    /// Shows/hides the custom chrome (used while the splash owns the title bar); fades in when
    /// animated. zh_CN: 显示/隐藏自定义 chrome（splash 接管标题栏期间）；animated 时淡入。
    void setChromeVisible(bool visible, bool animated = false);

    /// Reveals the back button (smooth collapse+fade) only while navigation history exists.
    /// zh_CN: 仅在有导航历史时显示返回按钮（平滑收展+淡入淡出）。
    void setBackAvailable(bool available);

    /// Enables/disables the navigation menu button. zh_CN: 启用/禁用导航菜单按钮。
    void setMenuEnabled(bool enabled);

    /// Search box widget the first-launch intro tour anchors a coach mark to.
    /// zh_CN: 首次启动引导用来锚定提示的搜索框控件。
    QWidget* searchBox() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void build(const QStringList& searchTitles);
    void applyBackButtonReveal(qreal reveal);
    void showToolTip(fluent::basicinput::Button* button);
    void hideToolTip();

    fluent::windowing::TitleBar* m_bar = nullptr;
    Callbacks m_callbacks;

    fluent::basicinput::Button* m_backButton = nullptr;
    fluent::basicinput::Button* m_menuButton = nullptr;
    QLabel* m_appIcon = nullptr;
    fluent::textfields::Label* m_title = nullptr;
    fluent::textfields::AutoSuggestBox* m_searchBox = nullptr;
    fluent::status_info::ToolTip* m_toolTip = nullptr;

    QVariantAnimation* m_backRevealAnimation = nullptr;
    qreal m_backReveal = 0.0;   // 0=hidden, 1=fully shown. zh_CN: 0=隐藏，1=完全显示。
    bool m_backRevealed = false;
    bool m_chromeVisible = true;
};

} // namespace fluent::gallery

#endif // GALLERYTITLEBARCONTROLLER_H

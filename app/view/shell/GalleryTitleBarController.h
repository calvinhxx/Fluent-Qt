#ifndef GALLERYTITLEBARCONTROLLER_H
#define GALLERYTITLEBARCONTROLLER_H

#include <functional>

#include <QObject>
#include <QPointer>

class QEvent;
class QLabel;
class QStandardItemModel;
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
} // namespace fluent::textfields
namespace fluent::status_info {
class ToolTip;
}

namespace fluent::gallery {

class GallerySearchService;

/**
 * @brief Owns Gallery custom title-bar controls, search box, tooltips, and width-adaptive layout.
 * zh_CN: 管理 Gallery 自定义标题栏控件、搜索框、提示气泡及随栏宽变化的自适应布局。
 *
 * Communicates navigation actions and layout queries back to GalleryWindow via Callbacks.
 * zh_CN: 通过 Callbacks 与主窗口交互，触发导航操作并查询最小布局状态。
 */
class GalleryTitleBarController : public QObject {
    Q_OBJECT
public:
    struct Callbacks {
        std::function<void()> onBack;      ///< Back button clicked. zh_CN: 点击返回。
        std::function<void()> onToggleNav; ///< Menu button clicked. zh_CN: 点击菜单。
        /// Route selected from search suggestions. zh_CN: 选中搜索结果，参数为路由 id。
        std::function<void(const QString&)> onSearchResultChosen;
        /// Returns true in LeftMinimal layout when app title/icon hide to give search space.
        /// zh_CN: 导航处于最小布局时返回 true，此时隐藏应用标题/图标以给搜索框留出空间。
        std::function<bool()> isMinimalNavLayout;
    };

    /// The search service answers the search box's asynchronous requests and must outlive it.
    /// zh_CN: 搜索服务回复搜索框的异步请求，生命周期须长于搜索框。
    GalleryTitleBarController(fluent::windowing::TitleBar* bar,
        GallerySearchService* search,
        Callbacks callbacks,
        QObject* parent = nullptr);
    ~GalleryTitleBarController() override;

    /// Reflows the leading group + search box for the current width / nav layout.
    /// zh_CN: 按当前宽度/导航布局重排前导组与搜索框。
    void updateLayout();

    /// Shows/hides the custom chrome (used while the splash owns the title bar); fades in when
    /// animated. zh_CN: 显示/隐藏自定义 chrome（splash 接管标题栏期间）；animated 时淡入。
    void setChromeVisible(bool visible, bool animated = false);

    /// Reveals the back button (smooth collapse+fade) only while navigation history exists.
    /// zh_CN: 仅在有导航历史时显示返回按钮（平滑收展+淡入淡出）。
    void setBackAvailable(bool available);

    /// Shows the navigation menu only when a side pane is available.
    /// zh_CN: 仅在存在侧边窗格时显示导航菜单按钮。
    void setMenuEnabled(bool enabled);

    /// Search box widget the first-launch intro tour anchors a coach mark to.
    /// zh_CN: 首次启动引导用来锚定提示的搜索框控件。
    QWidget* searchBox() const;
    /// Destination geometry for the connected startup transition. zh_CN: 启动连续转场的目标图标。
    QWidget* appIconWidget() const;
    /// Holds the icon pixels until startup arrives; preserves layout. zh_CN: 保留布局，暂缓显示目标图标。
    void setAppIconRevealed(bool revealed);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    fluent::windowing::TitleBar* titleBar() const;
    fluent::basicinput::Button* backButton() const;
    fluent::basicinput::Button* menuButton() const;
    void build();
    void buildSearchBox(fluent::windowing::TitleBar* bar);
    void applyBackButtonReveal(qreal reveal);
    void applyChromeOpacity();
    void setChromeRevealOpacity(qreal opacity);
    void showToolTip(fluent::basicinput::Button* button);
    void hideToolTip();
    void refreshAppIcon();

    // Event filters also run after a watched widget's derived destructor.
    // zh_CN: 事件过滤器也会在被监听控件的派生类析构后运行。
    QPointer<QObject> m_bar;
    QPointer<QWidget> m_hostWindow;
    Callbacks m_callbacks;

    QPointer<QObject> m_backButton;
    QPointer<QObject> m_menuButton;
    QPointer<QLabel> m_appIcon;
    QPointer<fluent::textfields::Label> m_title;
    QPointer<fluent::textfields::AutoSuggestBox> m_searchBox;
    QPointer<GallerySearchService> m_search;
    QStandardItemModel* m_searchModel = nullptr;
    quint64 m_submittedRequest = 0;
    bool m_submitNextRequest = false;
    QPointer<fluent::status_info::ToolTip> m_toolTip;

    QVariantAnimation* m_backRevealAnimation = nullptr;
    QVariantAnimation* m_chromeRevealAnimation = nullptr;
    qreal m_backReveal = 0.0; // 0=hidden, 1=fully shown. zh_CN: 0=隐藏，1=完全显示。
    qreal m_chromeRevealOpacity = 1.0;
    bool m_windowActive = false;
    bool m_backRevealed = false;
    bool m_chromeVisible = true;
    bool m_menuAvailable = false;
    bool m_appIconRevealed = true;
};

} // namespace fluent::gallery

#endif // GALLERYTITLEBARCONTROLLER_H

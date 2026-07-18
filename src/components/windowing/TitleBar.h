#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QPointer>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::windowing {

/**
 * @brief Custom title bar container with reserved system button regions.
 * zh_CN: 保留系统按钮区域的自定义标题栏容器。
 *
 * TitleBar hosts caller-provided title content while respecting leading and
 * trailing platform-reserved space and a configurable title-bar height.
 * zh_CN: TitleBar 承载调用方标题内容，同时尊重平台前后预留区域和可配置标题栏高度。
 */
class TitleBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Caller-owned widget hosted as component content.
     * zh_CN: 作为组件内容承载的调用方控件。
     */
    Q_PROPERTY(QWidget* contentWidget READ contentWidget WRITE setContentWidget NOTIFY contentWidgetChanged)
    /**
     * @brief Leading width reserved for platform window controls.
     * zh_CN: 为平台窗口控件保留的前置宽度。
     */
    Q_PROPERTY(int systemReservedLeadingWidth READ systemReservedLeadingWidth WRITE setSystemReservedLeadingWidth NOTIFY systemReservedLeadingWidthChanged)
    /**
     * @brief Trailing width reserved for platform window controls.
     * zh_CN: 为平台窗口控件保留的尾部宽度。
     */
    Q_PROPERTY(int systemReservedTrailingWidth READ systemReservedTrailingWidth WRITE setSystemReservedTrailingWidth NOTIFY systemReservedTrailingWidthChanged)
    /**
     * @brief Title bar height in pixels.
     * zh_CN: 标题栏高度，单位为像素。
     */
    Q_PROPERTY(int titleBarHeight READ titleBarHeight WRITE setTitleBarHeight NOTIFY titleBarHeightChanged)
    /**
     * @brief Whether the owning top-level window is currently active.
     * zh_CN: 所属顶层窗口当前是否处于激活状态。
     */
    Q_PROPERTY(bool windowActive READ isWindowActive NOTIFY windowActiveChanged)

public:
    explicit TitleBar(QWidget* parent = nullptr);

    static constexpr int defaultTitleBarHeight() { return kDefaultHeight; }

    QWidget* contentHost() const { return const_cast<TitleBar*>(this); }
    QWidget* contentWidget() const { return m_contentWidget; }
    void setContentWidget(QWidget* widget);

    int systemReservedLeadingWidth() const { return m_systemReservedLeadingWidth; }
    void setSystemReservedLeadingWidth(int width);

    int systemReservedTrailingWidth() const { return m_systemReservedTrailingWidth; }
    void setSystemReservedTrailingWidth(int width);

    int titleBarHeight() const { return m_titleBarHeight; }
    void setTitleBarHeight(int height);

    bool isWindowActive() const { return m_windowActive; }

    QVector<QRect> dragExclusionRects() const;

    /**
     * @brief Re-publishes the drag-exclusion (native hit-test) regions to the window.
     * zh_CN: 重新向窗口发布拖拽排除（原生命中测试）区域。
     *
     * The owner calls this after moving or enabling/disabling title-bar controls
     * without resizing the bar, so the native chrome keeps them click-through.
     * zh_CN: 当宿主在不改变标题栏尺寸的情况下移动或启用/禁用标题栏控件后调用，
     * 使原生 chrome 保持这些控件可点击。
     */
    void refreshChromeExclusions();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void contentWidgetChanged(QWidget* widget);
    void systemReservedLeadingWidthChanged(int width);
    void systemReservedTrailingWidthChanged(int width);
    void titleBarHeightChanged(int height);
    void windowActiveChanged(bool active);
    void dragStarted(const QPoint& globalPos);
    void dragMoved(const QPoint& globalPos);
    void dragFinished();
    void doubleClicked(const QPoint& globalPos);
    void contextMenuRequested(const QPoint& globalPos);
    void chromeGeometryChanged();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    static constexpr int kDefaultHeight = 36;

    bool isDragExcludedWidget(const QWidget* widget) const;
    void updateContentWidgetAnchor();

    fluent::AnchorLayout* m_anchorLayout = nullptr;
    QPointer<QWidget> m_contentWidget;
    int m_systemReservedLeadingWidth = 0;
    int m_systemReservedTrailingWidth = 0;
    int m_titleBarHeight = kDefaultHeight;
    bool m_windowActive = false;
    bool m_dragging = false;
};

} // namespace fluent::windowing

#endif // TITLEBAR_H

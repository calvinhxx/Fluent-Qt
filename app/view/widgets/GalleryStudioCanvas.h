#ifndef GALLERYSTUDIOCANVAS_H
#define GALLERYSTUDIOCANVAS_H

#include "components/foundation/FluentElement.h"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>
#include <QRect>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <functional>
#include <memory>

class QNetworkAccessManager;
class QPaintEvent;
class QResizeEvent;
class QVBoxLayout;

namespace fluent::scrolling {
class ScrollView;
}

namespace fluent::gallery {

/**
 * @brief Dynamic Fluent-Qt widget canvas driven by a declarative studio JSON document.
 * zh_CN: 由声明式 Studio JSON 文档驱动的真实 Fluent-Qt 控件画布。
 *
 * Builds an interactive Qt Widgets tree inside a Fluent ScrollView, reports settled
 * widget bounding rectangles in canvas coordinates, and forwards live widget events.
 * zh_CN: 在 Fluent ScrollView 中构建可交互的真实控件树，返回各节点相对画布的几何边界，并转发交互事件。
 */
class GalleryStudioCanvas : public QWidget {
    Q_OBJECT
public:
    static constexpr int kMaxDepth = 12;
    static constexpr int kMaxNodes = 160;

    /**
     * @brief Adaptive responsive breakpoint tier active on the canvas.
     * zh_CN: 当前画布生效的响应式布局断点层级。
     */
    enum class LayoutTier { Expanded, Medium, Compact };

    explicit GalleryStudioCanvas(QWidget* parent = nullptr);
    ~GalleryStudioCanvas() override;

    /**
     * @brief Builds and applies a Studio JSON document, optionally skipping node inspection signals during live drag preview.
     * zh_CN: 应用 Studio JSON 文档并构建实时控件树，拖拽预览时可跳过节点内省信号发射以降低开销。
     */
    bool applyDocument(const QJsonObject& document, QString* error = nullptr, bool emitInspection = true);

    /**
     * @brief Applies an in-place property patch to a single live widget without rebuilding sibling widgets.
     * zh_CN: 对单个已有画布控件就地应用属性补丁，不重建或重置兄弟节点状态；不可就地更新时返回 false。
     */
    bool patchNodeProperties(const QString& nodeId, const QJsonObject& patch, bool emitInspection = true);

    QJsonObject layoutSnapshot() const;

    /**
     * @brief Recomputes responsive container geometry for the current viewport width.
     * zh_CN: 根据当前视口宽度重新计算容器与控件的响应式排布。
     */
    void refreshResponsiveLayout();
    LayoutTier currentLayoutTier() const { return m_layoutTier; }

    void setSelectedNodeId(const QString& nodeId);
    QString selectedNodeId() const { return m_selectedNodeId; }

    void setTheme(fluent::FluentElement::Theme theme);
    fluent::FluentElement::Theme theme() const { return m_theme; }

    QWidget* widgetForNode(const QString& nodeId) const;

    /**
     * @brief Returns the active component playground schema and live property values for a node.
     * zh_CN: 返回指定画布节点对应的组件动态属性 Schema 与实时属性值。
     */
    QJsonObject inspectNode(const QString& nodeId) const;

    int nodeCount() const { return m_nodes.size(); }

    static QStringList supportedTypes();

signals:
    void layoutChanged(const QJsonObject& snapshot);
    void interactionEvent(const QString& nodeId, const QString& eventName, const QJsonValue& value);
    void nodeInspected(const QJsonObject& inspection);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct NodeEntry {
        QString id;
        QString type;
        QPointer<QWidget> widget;
        std::function<QRect(const GalleryStudioCanvas*, bool*)> resolveBounds;
        QPointer<QObject> inspectTarget;
        QJsonObject anchorProps;
        int anchorIndex = 0;
        int anchorContainerPadding = 12;
    };

    QWidget* buildNode(const QJsonObject& nodeObj, QWidget* parentWidget, QVector<NodeEntry>& built);
    QWidget* buildContainer(const QString& id,
        const QString& type,
        const QJsonObject& props,
        const QJsonArray& children,
        QWidget* parentWidget,
        QVector<NodeEntry>& built);
    QWidget* buildControl(const QString& id, const QString& type, const QJsonObject& props, QWidget* parentWidget);

    void fetchPhoto(const QUrl& url, QObject* lifetimeGuard, std::function<void(const QPixmap&)> onReady);
    void scheduleLayoutSnapshot();
    void emitLayoutSnapshot();
    void applyResponsiveLayout(int viewportWidth);

    struct ResponsiveBindings;

    fluent::scrolling::ScrollView* m_scroll = nullptr;
    QWidget* m_stage = nullptr;
    QVBoxLayout* m_stageLayout = nullptr;
    QPointer<QWidget> m_rootWidget;
    QVector<NodeEntry> m_nodes;
    QHash<QString, QPointer<QWidget>> m_widgetsById;
    QString m_selectedNodeId;
    fluent::FluentElement::Theme m_theme = fluent::FluentElement::Light;
    LayoutTier m_layoutTier = LayoutTier::Expanded;
    int m_lastLayoutWidth = -1;
    std::unique_ptr<ResponsiveBindings> m_responsive;
    ResponsiveBindings* m_stagingResponsive = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    int m_revision = 0;
    bool m_snapshotScheduled = false;
    bool m_rootIsAnchorBox = false;
    bool m_hasAccentOverride = false;
};

} // namespace fluent::gallery

#endif // GALLERYSTUDIOCANVAS_H

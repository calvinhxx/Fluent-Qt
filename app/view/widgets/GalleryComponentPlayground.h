#ifndef GALLERYCOMPONENTPLAYGROUND_H
#define GALLERYCOMPONENTPLAYGROUND_H

#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QRect>

class QWidget;
namespace fluent::basicinput {
class Button;
}

namespace fluent::gallery {

/** @brief Applies the documentation playground's allowlisted properties to a real control.
 * zh_CN: 将文档属性实验室的白名单配置应用到真实控件。
 */
class GalleryComponentPlayground final : public QObject {
    Q_OBJECT
public:
    static GalleryComponentPlayground* create(const QString& route, QWidget* parent, bool interactive = false);
    static GalleryComponentPlayground* createInteractive(const QString& route, QWidget* parent);
    static GalleryComponentPlayground* fromButton(const basicinput::Button* source, QWidget* parent);
    static GalleryComponentPlayground* fromWidget(const QString& route, const QWidget* source, QWidget* parent);
    static bool accepts(const QString& route, const QWidget* source);

    /**
     * @brief Resolves a Studio PascalCase widget type to its documentation playground route.
     * zh_CN: 将 Studio 控件类型名映射为文档动态属性预览的组件路由。
     */
    static QString routeForStudioType(const QString& studioType);

    /**
     * @brief Returns the filtered playground schema, live values, and theme fallbacks for a Studio widget.
     * zh_CN: 提取 Studio 控件过滤后的属性 Schema、当前控件实时值与主题回退有效值。
     */
    static QJsonObject studioSchemaForWidget(const QString& studioType, QObject* target);

    /**
     * @brief Applies Studio node properties to a live widget in playground schema order.
     * zh_CN: 按属性 Schema 顺序将 Studio 节点属性安全写入真实控件。
     */
    static bool applyStudioProperties(const QString& studioType, QObject* target, const QJsonObject& props);

    /**
     * @brief Returns whether every key in a property patch can be applied in-place without rebuilding siblings.
     * zh_CN: 检查属性补丁中的每一项是否均可通过已有控件就地更新而无需重建兄弟节点。
     */
    static bool canPatchStudioProperties(const QString& studioType, const QObject* target, const QJsonObject& patch);

    ~GalleryComponentPlayground() override;
    QWidget* widget() const;
    QJsonObject snapshot() const;
    QImage renderPreview(qreal devicePixelRatio = 1);
    QJsonObject interactiveViewport(QWidget* host, bool relayout = true);
    bool initializeInteractive(const QJsonObject& values, QString* error = nullptr);
    bool apply(const QJsonObject& command, QString* error = nullptr);

signals:
    void stateChanged();

private:
    explicit GalleryComponentPlayground(const QJsonObject& schema, QWidget* parent, bool interactive = false);
    void layoutSurface();
    void syncOverlaySurfaceRect();
    void presentInteractiveSurface();
    QJsonObject defaults() const;
    QJsonObject values() const;
    QJsonObject valuesFor(const basicinput::Button* button) const;
    bool normalize(const QJsonObject& input, QJsonObject* output, QString* error) const;
    void writeValues(const QJsonObject& values);
    QString code(const QJsonObject& values, bool python) const;
    void recordEvent(const QString& name, bool checked);

    QJsonObject m_schema;
    QJsonObject m_initial;
    QPointer<basicinput::Button> m_button;
    QPointer<QWidget> m_surface;
    QPointer<QObject> m_target;
    QPointer<QWidget> m_overlayHost;
    QJsonObject genericValues(const QObject* target) const;
    void writeGenericValues(const QJsonObject& values);
    void connectGenericEvents();
    QString genericCode(const QJsonObject& values, bool python) const;
    void applyHoverState();
    QString m_icon = QStringLiteral("None");
    bool m_customRadius = false;
    bool m_applying = false;
    bool m_previewHovered = false;
    bool m_customHoverPos = false;
    qreal m_hoverX = 0.6;
    qreal m_hoverY = 0.45;
    int m_revision = 0;
    int m_eventSequence = 0;
    QJsonArray m_events;
    bool m_interactive = false;
    bool m_presented = false;
    QJsonObject m_observedValues;
    QJsonObject m_observedViewport;
};
} // namespace fluent::gallery
#endif

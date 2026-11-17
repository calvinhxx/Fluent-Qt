#ifndef FLUENTQT_QMLBRIDGE_QMLSESSION_H
#define FLUENTQT_QMLBRIDGE_QMLSESSION_H

#include <QObject>
#include <memory>
class QWidget;

namespace fluent::qmlbridge {
class QmlView;

/**
 * @brief Session for starting and stopping QmlBridge (C++, PySide6, and WASM).
 * Deactivating or destroying unloads QML scenes; clean up on the GUI thread before tearing down the host.
 * zh_CN: 用于启停 QmlBridge 的会话类（兼容 C++/PySide6/WASM），停用或销毁会卸载 QML 场景，须在宿主销毁前清理（仅限 GUI 线程）。
 */
class QmlSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    /** @brief Graphics API shared between Qt Quick and widget hosts. zh_CN: Qt Quick 与控件宿主共用的图形后端。 */
    enum class GraphicsApi { Automatic, OpenGL, Direct3D11, Metal };
    Q_ENUM(GraphicsApi)

    explicit QmlSession(QObject* host = nullptr, QObject* parent = nullptr);
    ~QmlSession() override;
    /**
     * @brief Selects OpenGL for Qt Quick before the first Quick view is created.
     * Call before Quick initializes when sharing a window with SpatialView or QOpenGLWidget.
     * zh_CN: 与 SpatialView 或 QOpenGLWidget 共用窗口时，在创建首个 Qt Quick 视图前将渲染后端设为 OpenGL。
     */
    static void prepareApplication();
    /**
     * @brief Selects the Qt Quick graphics API before the first Quick view is created.
     * Automatic restores Qt's default backend; has no effect on Spatial settings.
     * zh_CN: 与 SpatialView 或 QOpenGLWidget 共用窗口时，在创建首个 Qt Quick 视图前指定渲染后端（Automatic 恢复默认设置）。
     */
    static void prepareApplication(GraphicsApi api);
    /**
     * @brief Checks whether Qt Quick's current backend can share a window with the given API.
     * zh_CN: 检查当前 Qt Quick 后端是否能与指定图形后端共用窗口（软件渲染与 Automatic 始终兼容）。
     */
    static bool isGraphicsApiCompatible(GraphicsApi api);
    /** @brief Registers and activates QmlPlugin; returns true if already active. zh_CN: 注册并激活 QML 插件；已激活时直接返回 true。 */
    bool activate(QString* error = nullptr);
    /** @brief Unloads all attached QML scenes while keeping views and external models. zh_CN: 卸载所有已关联视图的 QML 场景，保留控件与外部模型。 */
    bool deactivate(QString* error = nullptr);
    bool isActive() const;
    /** @brief Creates and attaches a QmlView while active; returns nullptr when inactive. zh_CN: 激活时创建并关联 QmlView（由 parent 或调用方管理）；未激活时返回 nullptr。 */
    QmlView* createView(QWidget* parent = nullptr);
    /** @brief Attaches an existing QmlView while active (caller retains ownership). zh_CN: 激活状态下关联已有的 QmlView（仍由应用管理，不会随会话销毁）。 */
    bool attachView(QmlView* view);

signals:
    void activeChanged(bool active);

private:
    struct Private;
    std::shared_ptr<Private> d;
};
} // namespace fluent::qmlbridge
#endif // FLUENTQT_QMLBRIDGE_QMLSESSION_H

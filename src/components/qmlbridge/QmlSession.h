#ifndef FLUENTQT_QMLBRIDGE_QMLSESSION_H
#define FLUENTQT_QMLBRIDGE_QMLSESSION_H

#include <QObject>
#include <memory>
class QWidget;

namespace fluent::qmlbridge {
class QmlView;

/**
 * @brief Manages QmlPlugin activation and view lifecycle for C++, PySide6, and WebAssembly apps.
 * Caller retains ownership of the host, views, and context models; parent owns only this session
 * and must not be a tracked view or its descendant. Deactivate or destroy the session before
 * tearing down the host. All calls must run on the GUI thread, and activation leaves the global
 * graphics API unchanged.
 * zh_CN: 管理 QmlPlugin 激活与视图生命周期的会话类（支持 C++、PySide6 和 WebAssembly）。
 * 调用方负责宿主、视图及上下文模型的生命周期；parent 仅管理本会话，且不能是已关联视图或其子对象。
 * 销毁宿主前需先停用或销毁会话。所有调用须在 GUI 线程执行，激活不会更改全局图形 API。
 */
class QmlSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    /** @brief Graphics API shared between Qt Quick and native widget hosts. zh_CN: Qt Quick 与原生控件宿主共用的图形 API。 */
    enum class GraphicsApi { Automatic, OpenGL, Direct3D11, Metal };
    Q_ENUM(GraphicsApi)

    explicit QmlSession(QObject* host = nullptr, QObject* parent = nullptr);
    ~QmlSession() override;
    /**
     * @brief Selects OpenGL for Qt Quick before the first Quick surface is created.
     * Call explicitly before Quick initializes when sharing a window with SpatialView or QOpenGLWidget;
     * on Qt 5, the existing OpenGL or software selection is kept.
     * zh_CN: 在创建首个 Quick 界面前将 Qt Quick 设为 OpenGL 后端。
     * 与 SpatialView 或 QOpenGLWidget 共用窗口时需显式调用，且不能在 Quick 初始化之后调用；
     * Qt 5 下保持其默认 OpenGL 或软件渲染选择。
     */
    static void prepareApplication();
    /**
     * @brief Selects a Qt Quick graphics API before the first Quick surface is created.
     * Automatic restores Qt's platform/environment default; Qt 5 keeps its existing OpenGL/software
     * backend. Does not affect Spatial settings.
     * zh_CN: 在创建首个 Quick 界面前指定 Qt Quick 图形 API。
     * Automatic 会恢复 Qt 的平台或环境变量默认设置；Qt 5 下保持原有 OpenGL/软件后端，不影响 Spatial 设置。
     */
    static void prepareApplication(GraphicsApi api);
    /**
     * @brief Checks whether Qt Quick's current backend can share a window with the given API.
     * Queries Qt's selection without modifying it; software Quick and Automatic are always compatible.
     * zh_CN: 检查当前 Qt Quick 后端是否可与指定图形 API 共用窗口。
     * 仅查询不修改当前配置；软件渲染的 Quick 与 Automatic 均兼容任意后端。
     */
    static bool isGraphicsApiCompatible(GraphicsApi api);
    /** @brief Registers and activates QmlPlugin; returns true immediately if already active. zh_CN: 注册并激活 QmlPlugin；已激活时直接返回 true。 */
    bool activate(QString* error = nullptr);
    /** @brief Unloads all tracked QML scenes while keeping views and external models intact. zh_CN: 卸载所有已关联视图的 QML 场景，保留视图控件与外部模型。 */
    bool deactivate(QString* error = nullptr);
    bool isActive() const;
    /** @brief Creates a QmlView attached to this session; returns nullptr while inactive. zh_CN: 创建并关联一个 QmlView（由 parent 或调用方管理生命周期）；未激活时返回 nullptr。 */
    QmlView* createView(QWidget* parent = nullptr);
    /** @brief Attaches an existing QmlView while active without taking ownership. zh_CN: 在激活状态下关联已有的 QmlView（不改变父对象或所有权）。 */
    bool attachView(QmlView* view);

signals:
    void activeChanged(bool active);

private:
    struct Private;
    std::shared_ptr<Private> d;
};
} // namespace fluent::qmlbridge
#endif // FLUENTQT_QMLBRIDGE_QMLSESSION_H

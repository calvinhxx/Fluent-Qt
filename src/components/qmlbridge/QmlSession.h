#ifndef FLUENTQT_QMLBRIDGE_QMLSESSION_H
#define FLUENTQT_QMLBRIDGE_QMLSESSION_H

#include <QObject>
#include <memory>
class QWidget;

namespace fluent::qmlbridge {
class QmlView;

/**
 * @brief Owns QML bridge activation for C++, PySide6 and WebAssembly applications.
 * zh_CN: 为 C++、PySide6 与 WebAssembly 应用管理 QML 桥接激活。
 * Host, returned views and business models remain borrowed. The optional QObject
 * parent owns only this session and must not be a tracked view or its descendant.
 * Destroy or deactivate the session before dismantling its host. All calls use
 * QApplication's GUI thread; activation does not select a global graphics API.
 * zh_CN: 宿主、交付视图和业务模型仍被借用。可选 QObject 父对象仅持有本会话，
 * 不得为受追踪视图或其后代。拆卸宿主前先销毁或停用会话。所有调用均须在
 * QApplication 的 GUI 线程执行；激活不选择全局图形 API。
 */
class QmlSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    /** @brief Selects the graphics API shared by Quick and its native widget host.
     * zh_CN: 选择 Quick 与原生控件宿主共用的图形 API。 */
    enum class GraphicsApi { Automatic, OpenGL, Direct3D11, Metal };
    Q_ENUM(GraphicsApi)

    explicit QmlSession(QObject* host = nullptr, QObject* parent = nullptr);
    ~QmlSession() override;
    /**
     * @brief Selects Qt Quick OpenGL before the first Quick surface is created.
     * zh_CN: 在首次创建 Quick 表面前选择 Qt Quick OpenGL。
     * Call explicitly when sharing a window with Spatial/QOpenGLWidget. This is
     * application-wide policy, not an activation side effect; Qt 5 keeps its
     * default OpenGL/software selection. Do not call after Quick initialization.
     * zh_CN: 与 Spatial/QOpenGLWidget 共用窗口时显式调用。这是应用级策略，非激活副作用；
     * Qt 5 保留默认 OpenGL/软件选择。Quick 初始化后不得调用。
     */
    static void prepareApplication();
    /**
     * @brief Selects a Quick graphics API before the first Quick surface exists.
     * zh_CN: 在首次创建 Quick 表面前选择其图形 API。
     * Automatic restores Qt's platform/environment selection. Qt 5 keeps its
     * existing OpenGL/software backend. This never changes Spatial's settings.
     * zh_CN: Automatic 恢复 Qt 的平台及环境选择；Qt 5 保留原有 OpenGL/软件后端。
     * 此调用不会修改 Spatial 设置。
     */
    static void prepareApplication(GraphicsApi api);
    /**
     * @brief Checks whether Quick can share the requested widget graphics API.
     * zh_CN: 检查 Quick 是否能与指定图形 API 的控件共用窗口。
     * Queries Qt's current selection without changing it; software Quick is
     * compatible with any host API. Automatic imposes no host requirement.
     * zh_CN: 只查询 Qt 当前选择；软件 Quick 兼容任意宿主 API，Automatic 不限定宿主。
     */
    static bool isGraphicsApiCompatible(GraphicsApi api);
    /** @brief Activates the bridge; repeating an active call is a no-op.
     * zh_CN: 激活桥接；重复激活为空操作。 */
    bool activate(QString* error = nullptr);
    /** @brief Unloads tracked scenes, retaining caller-owned views and models.
     * zh_CN: 卸载受追踪场景，保留调用方视图和模型。 */
    bool deactivate(QString* error = nullptr);
    bool isActive() const;
    /** @brief Creates a caller-owned view only while the session is active.
     * zh_CN: 仅在会话激活时创建调用方持有的视图。 */
    QmlView* createView(QWidget* parent = nullptr);
    /** @brief Tracks an existing caller-owned view only while active.
     * zh_CN: 仅在激活时追踪调用方已有视图。 */
    bool attachView(QmlView* view);

signals:
    void activeChanged(bool active);

private:
    struct Private;
    std::shared_ptr<Private> d;
};
} // namespace fluent::qmlbridge
#endif // FLUENTQT_QMLBRIDGE_QMLSESSION_H

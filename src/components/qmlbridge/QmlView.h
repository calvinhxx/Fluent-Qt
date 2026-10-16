#ifndef FLUENTQT_QMLBRIDGE_QMLVIEW_H
#define FLUENTQT_QMLBRIDGE_QMLVIEW_H

#include <QStringList>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>
#include <QWidget>
#include <memory>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QOpenGLContext;

namespace fluent::qmlbridge {
class QmlPlugin;

/**
 * @brief Hosts an application QML page inside a caller-owned QWidget.
 * zh_CN: 在调用方持有的 QWidget 中承载应用的 QML 页面。
 * Attach to an active QmlSession or QmlPlugin before loading. Each loaded view
 * owns its Quick widget and engine; application context objects stay borrowed.
 * The QML scene paints its background; unpainted areas reveal the QWidget host.
 * Deactivation unloads the scene, preserving this widget and its context bindings.
 * All calls use QApplication's GUI thread. Load trusted application QML only;
 * this adapter is not a script sandbox. Use deleteLater() from QML callbacks.
 * zh_CN: 加载前关联到已激活的 QmlSession 或 QmlPlugin。每个加载的视图持有自身的
 * Quick 控件和引擎，应用上下文对象仅被借用。停用卸载场景，保留本控件和上下文绑定。
 * QML 场景绘制自身背景，未绘制的区域显示 QWidget 宿主背景。
 * 所有调用均须在 QApplication 的 GUI 线程执行。仅加载可信应用 QML，本适配器不提供
 * 脚本沙箱；QML 回调中通过 deleteLater() 销毁宿主。
 */
class QmlView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source NOTIFY sourceChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QStringList errors READ errors NOTIFY errorsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorsChanged)
    Q_PROPERTY(QObject* theme READ themeObject CONSTANT)
    Q_PROPERTY(bool textureRenderingEnabled
        READ isTextureRenderingEnabled
        WRITE setTextureRenderingEnabled
        NOTIFY textureRenderingEnabledChanged)
public:
    enum Status { Unloaded, Loading, Ready, Error };
    Q_ENUM(Status)

    explicit QmlView(QWidget* parent = nullptr);
    ~QmlView() override;
    QUrl source() const;
    Status status() const;
    bool isActive() const;
    QStringList errors() const;
    QString lastError() const;
    /** @brief Returns the borrowed QML root, invalidated by unload or replacement.
     * zh_CN: 返回借用的 QML 根对象，卸载或替换场景后失效。 */
    QObject* rootObject() const;
    /** @brief Returns the view-owned fluentTheme object.
     * zh_CN: 返回视图持有的 fluentTheme 对象。 */
    QObject* themeObject() const;
    /**
     * @brief Binds a borrowed GUI-thread QObject before loading; nullptr removes it.
     * zh_CN: 加载前绑定借用的 GUI 线程 QObject；nullptr 移除绑定。
     * Names must be QML identifiers other than fluentTheme. Bindings survive
     * unload. Destroying a bound object unloads the scene. No QObject reparenting
     * or ownership transfer occurs; registered objects use C++ QML ownership.
     * zh_CN: 名称须为 fluentTheme 之外的 QML 标识符。绑定在卸载后保留；对象销毁时
     * 卸载场景。不改变 QObject 父对象或转移所有权；注册对象采用 QML C++ 所有权。
     */
    bool setContextObject(const QString& name, QObject* object);
    /**
     * @brief Loads an absolute URL; true means ready or asynchronous loading accepted.
     * zh_CN: 加载绝对 URL；true 表示已就绪或已接受异步加载。
     * Observe statusChanged and errorOccurred for asynchronous errors. Repeating
     * a ready source is a no-op; unload first to reload the same file.
     * zh_CN: 通过 statusChanged 与 errorOccurred 观察异步错误。重复就绪 URL 为空操作；
     * 重新加载同一文件前先卸载。
     */
    bool load(const QUrl& source, QString* error = nullptr);
    /**
     * @brief Detaches the QML scene and schedules its engine's safe destruction.
     * zh_CN: 分离 QML 场景并安排安全销毁其引擎，不销毁借用模型。
     * Clears source/root/status immediately and retains context bindings. The
     * hidden Quick widget and engine use deleteLater(), allowing an executing
     * QML callback to return before destruction on the next event-loop turn.
     * zh_CN: 立即清空 source/root/status，保留上下文绑定。隐藏的 Quick 控件和引擎
     * 通过 deleteLater() 在下一轮事件循环销毁，使执行中的 QML 回调能先返回。
     */
    void unload();
    /**
     * @brief Selects GPU texture presentation while retaining the same QML scene.
     * zh_CN: 选择 GPU 纹理呈现，保留同一 QML 场景。
     * May be requested before load. The Quick widget stays hidden while enabled;
     * the compositor must call renderTexture() and present its texture. Disabling
     * restores the original Quick widget without recreating the root or engine.
     * zh_CN: 可在加载前设置。启用时 Quick 控件隐藏，合成器须调用 renderTexture() 并呈现
     * 其纹理；关闭后恢复原 Quick 控件，不重建根对象或引擎。
     */
    bool setTextureRenderingEnabled(bool enabled);
    /** @brief Returns the requested texture presentation mode.
     * zh_CN: 返回请求的纹理呈现模式。 */
    bool isTextureRenderingEnabled() const;
    /**
     * @brief Renders or reuses the scene in the caller's current OpenGL context.
     * zh_CN: 在调用方当前 OpenGL 上下文中绘制或复用场景。
     * GUI-thread only; context must be current and compatible with Qt Quick's
     * OpenGL backend. DPR must be finite and in (0, 4]. The limit covers the
     * color/depth targets, excluding the QML scene's own textures. Qt 5 and Qt
     * 6.2 require DPR to match the presenting window. Failure leaves the scene
     * available for an explicit return to native widget presentation.
     * zh_CN: 仅 GUI 线程，上下文须当前有效并兼容 Quick 的 OpenGL 后端。DPR 范围 (0, 4]。
     * 限额包含颜色及深度目标，不含 QML 场景自身纹理。Qt 5 与 Qt 6.2 要求 DPR 匹配
     * 呈现窗口；失败后仍可显式返回原生控件呈现。
     */
    bool renderTexture(QOpenGLContext* context, qreal devicePixelRatio, quint64 maximumBytes = 67108864);
    /** @brief Returns a borrowed premultiplied RGBA, bottom-up GL texture or zero.
     * Invalidated by release, unload, resize or context replacement.
     * zh_CN: 返回借用的预乘 RGBA、下原点 GL 纹理，未就绪时为零；释放、卸载、尺寸或上下文改变后失效。 */
    unsigned int textureId() const;
    /** @brief Returns the texture dimensions in device pixels.
     * zh_CN: 返回纹理的设备像素尺寸。 */
    QSize textureSize() const;
    /** @brief Returns the monotonically increasing revision of rendered content.
     * zh_CN: 返回已绘制内容的单调递增版本。 */
    quint64 textureRevision() const;
    /** @brief Returns GPU allocation, frame and failure diagnostics.
     * zh_CN: 返回 GPU 分配、帧及失败诊断。 */
    QVariantMap textureStatistics() const;
    /** @brief Releases GPU targets while retaining the scene and mode request.
     * zh_CN: 释放 GPU 目标，保留场景及模式请求。 */
    void releaseTexture();
    void onThemeUpdated() override;
    QSize sizeHint() const override;

signals:
    void sourceChanged(const QUrl& source);
    void statusChanged(Status status);
    void activeChanged(bool active);
    void errorsChanged();
    void errorOccurred(const QString& message);
    void textureRenderingEnabledChanged(bool enabled);
    void textureUpdateRequested();
    void textureRenderingFailed(const QString& message);

protected:
    bool event(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
    friend class QmlPlugin;
    struct Private;
    std::shared_ptr<Private> d;
    QPointer<QObject> m_plugin;
    bool reportError(const QString& message, QString* error = nullptr);
    void updateStatus();
    void scheduleNativePresentation();
    void setActive(bool active);
    bool ensureTextureRenderer();
};
} // namespace fluent::qmlbridge
#endif // FLUENTQT_QMLBRIDGE_QMLVIEW_H

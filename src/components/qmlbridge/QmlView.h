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
 * @brief QWidget container that hosts an application QML scene.
 * Attach to an active QmlSession or QmlPlugin before calling load(). The view owns its internal
 * QQuickWidget and QQmlEngine, while caller context objects remain owned by the application.
 * Unpainted areas of the QML scene reveal this widget's background. Deactivating the session or
 * plugin unloads the scene but keeps the widget and context bindings intact. All calls must run
 * on the GUI thread; load only trusted QML (no script sandbox is provided), and use deleteLater()
 * when destroying the host from QML callbacks.
 * zh_CN: 在 QWidget 中承载应用 QML 页面的容器控件。
 * 调用 load() 前须先关联到已激活的 QmlSession 或 QmlPlugin。视图管理内部的 QQuickWidget 与
 * QQmlEngine，外部注入的上下文对象仍由应用管理生命周期。QML 未绘制的区域会透出本控件背景；
 * 停用会话或插件时仅卸载场景，保留视图控件与上下文绑定。所有调用须在 GUI 线程执行；仅加载可信
 * QML（本控件不提供脚本沙箱），在 QML 回调中销毁宿主请使用 deleteLater()。
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
    /**
     * @brief Returns the loaded QML root object (owned by the QML scene); becomes invalid after unload() or reload.
     * zh_CN: 返回当前加载的 QML 根对象（由 QML 场景管理）；卸载或重新加载后失效。
     */
    QObject* rootObject() const;
    /** @brief Returns the view-owned fluentTheme context object. zh_CN: 返回视图持有的 fluentTheme 主题对象。 */
    QObject* themeObject() const;
    /**
     * @brief Registers a GUI-thread QObject under name before loading; pass nullptr to remove it.
     * Name must be a valid QML identifier other than "fluentTheme". Bindings persist across unload(),
     * and destroying a bound object unloads the scene. Does not reparent or transfer ownership
     * (objects are marked CppOwnership).
     * zh_CN: 在加载前将 GUI 线程上的 QObject 绑定为 QML 上下文对象；传入 nullptr 可移除绑定。
     * name 须为除 fluentTheme 外的合法 QML 标识符。绑定在卸载后保留，被绑定对象销毁时会自动卸载场景；
     * 不会改变其父对象或所有权（注册为 CppOwnership）。
     */
    bool setContextObject(const QString& name, QObject* object);
    /**
     * @brief Loads an absolute QML URL; returns true when ready or when async loading starts.
     * Watch statusChanged() and errorOccurred() for asynchronous errors. Returns true immediately if
     * the same URL is already Ready; call unload() first to force a reload.
     * zh_CN: 加载绝对路径 QML URL；就绪或已启动异步加载时返回 true。
     * 异步加载错误可通过 statusChanged() 与 errorOccurred() 获取。同一 URL 已处于 Ready 状态时
     * 直接返回 true；如需重新加载同一文件，请先调用 unload()。
     */
    bool load(const QUrl& source, QString* error = nullptr);
    /**
     * @brief Unloads the QML scene and schedules deferred cleanup of its Quick widget and engine.
     * Clears source(), rootObject(), and status() immediately while keeping context bindings intact.
     * Uses deleteLater() so an active QML callback can unwind before engine teardown.
     * zh_CN: 卸载 QML 场景并延迟销毁内部的 Quick 控件与引擎，不会销毁外部绑定的模型。
     * 调用后立即清空 source()、rootObject() 与 status() 并保留上下文绑定；内部通过 deleteLater()
     * 在下一轮事件循环销毁引擎，确保正在执行的 QML 回调能安全返回。
     */
    void unload();
    /**
     * @brief Switches between native Quick widget display and offscreen OpenGL texture rendering without reloading the scene.
     * May be called before load(). When enabled, the Quick widget stays hidden and the caller must
     * drive renderTexture() and present its output; disabling restores the native Quick widget.
     * zh_CN: 切换是否使用离屏 OpenGL 纹理渲染，期间保留当前 QML 场景与引擎。
     * 可在 load() 前设置。启用后内部 Quick 控件保持隐藏，需由调用方执行 renderTexture() 并绘制纹理；
     * 关闭后恢复原生 Quick 控件显示，无需重建根对象。
     */
    bool setTextureRenderingEnabled(bool enabled);
    /** @brief Whether offscreen texture rendering is currently requested. zh_CN: 是否已启用纹理渲染模式。 */
    bool isTextureRenderingEnabled() const;
    /**
     * @brief Renders or reuses the QML texture in the caller's current OpenGL context.
     * GUI-thread only; context must be current and compatible with Qt Quick's OpenGL backend.
     * Device pixel ratio must be finite and in (0, 4] (must match the presenting window on Qt 5
     * and Qt 6.2). maximumBytes covers color/depth targets, excluding QML scene image assets.
     * On failure, the scene remains valid for switching back to native widget display.
     * zh_CN: 在调用方当前 OpenGL 上下文中渲染或复用 QML 纹理。
     * 仅限 GUI 线程；context 须为当前上下文且兼容 Qt Quick 的 OpenGL 后端。DPR 须为有限值且在
     * (0, 4] 范围内（Qt 5 与 Qt 6.2 下须与展示窗口一致）。maximumBytes 限制颜色与深度缓冲大小，
     * 不含 QML 场景自身图片资源；失败后仍可切回原生控件显示。
     */
    bool renderTexture(QOpenGLContext* context, qreal devicePixelRatio, quint64 maximumBytes = 67108864);
    /**
     * @brief Returns the view's premultiplied RGBA OpenGL texture (bottom-left origin), or 0 if unavailable.
     * Invalidated by releaseTexture(), unload(), resize, or OpenGL context change.
     * zh_CN: 返回视图管理的预乘 RGBA OpenGL 纹理（左下角原点），未就绪时返回 0；释放、卸载、尺寸或上下文变化后失效。
     */
    unsigned int textureId() const;
    /** @brief Texture dimensions in device pixels. zh_CN: 纹理的设备像素尺寸。 */
    QSize textureSize() const;
    /** @brief Monotonically increasing revision counter updated on each rendered frame. zh_CN: 每次成功渲染后递增的内容版本号。 */
    quint64 textureRevision() const;
    /** @brief Diagnostics map covering texture allocation, frame count, and last error. zh_CN: 包含显存分配、帧数与最近错误信息的诊断字典。 */
    QVariantMap textureStatistics() const;
    /** @brief Frees GPU render targets while keeping the QML scene and texture-mode setting intact. zh_CN: 释放 GPU 渲染目标，保留 QML 场景与纹理模式设置。 */
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

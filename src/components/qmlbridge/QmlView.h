#ifndef FLUENTQT_QMLBRIDGE_QMLVIEW_H
#define FLUENTQT_QMLBRIDGE_QMLVIEW_H

#include <QStringList>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>
#include <QWidget>
#include <memory>
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QOpenGLContext;

namespace fluent::qmlbridge {
class QmlPlugin;

/**
 * @brief QWidget container for embedding and displaying an application QML page.
 * Attach to an active QmlSession before use; load only trusted QML (scenes unload automatically when the session deactivates).
 * zh_CN: 在 QWidget 中嵌入并显示应用 QML 页面的控件。使用前须先关联已激活的 QmlSession；仅加载受信任的 QML，停用会话时会自动卸载场景。
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
    /** @brief Returns the loaded QML root object, or nullptr until ready or after unload(). zh_CN: 返回已加载的 QML 根对象；未就绪或卸载后失效。 */
    QObject* rootObject() const;
    /** @brief Returns the view-owned fluentTheme context object. zh_CN: 返回视图持有的 fluentTheme 主题对象。 */
    QObject* themeObject() const;
    /**
     * @brief Registers a GUI-thread QObject under name before loading (pass nullptr to remove; "fluentTheme" is reserved).
     * Caller retains object lifetime; bindings persist across unload() and destroying a bound object unloads the scene.
     * zh_CN: 在加载前将 GUI 线程上的 QObject 绑定为 QML 上下文对象（传入 nullptr 移除，fluentTheme 为保留名称）。
     * 对象仍由应用管理，绑定在卸载后保留，但对象销毁时会自动卸载当前场景。
     */
    bool setContextObject(const QString& name, QObject* object);
    /**
     * @brief Loads an absolute QML URL; returns true when ready or async loading starts.
     * Watch statusChanged() and errorOccurred() for async errors; call unload() first to reload the same URL.
     * zh_CN: 加载绝对路径 QML URL，就绪或开始异步加载时返回 true；可通过 statusChanged() 与 errorOccurred() 监听错误，重新加载同一 URL 须先调用 unload()。
     */
    bool load(const QUrl& source, QString* error = nullptr);
    /**
     * @brief Unloads the QML scene and resets status while keeping external context bindings intact.
     * Safe to call from QML callbacks (teardown is deferred until callbacks unwind).
     * zh_CN: 卸载 QML 场景并重置状态，保留外部上下文绑定；内部延迟清理，可在 QML 回调中安全调用。
     */
    void unload();
    /**
     * @brief Toggles offscreen OpenGL texture rendering without reloading the QML scene.
     * When enabled, the caller must invoke renderTexture() and draw the texture; may be set before load().
     * zh_CN: 切换离屏 OpenGL 纹理渲染模式而不重新加载场景（可在 load() 前设置）；启用后须由调用方调用 renderTexture() 并绘制纹理。
     */
    bool setTextureRenderingEnabled(bool enabled);
    /** @brief Whether offscreen texture rendering is enabled. zh_CN: 是否启用离屏纹理渲染模式。 */
    bool isTextureRenderingEnabled() const;
    /**
     * @brief Renders the QML scene into a texture using the caller's current OpenGL context (GUI thread only).
     * devicePixelRatio must be in (0, 4]; maximumBytes caps color/depth buffers.
     * zh_CN: 在当前 OpenGL 上下文中将 QML 场景渲染为纹理（仅限 GUI 线程，缩放倍率须在 (0, 4] 内，maximumBytes 限制帧缓冲显存）。
     */
    bool renderTexture(QOpenGLContext* context, qreal devicePixelRatio, quint64 maximumBytes = 67108864);
    /**
     * @brief Returns the premultiplied RGBA OpenGL texture (bottom-left origin), or 0 if unavailable.
     * Invalidated on releaseTexture(), unload(), resize, or context change.
     * zh_CN: 返回预乘 RGBA OpenGL 纹理（左下角原点），未就绪时返回 0；释放、卸载、尺寸或上下文变化后失效。
     */
    unsigned int textureId() const;
    /** @brief Texture dimensions in device pixels. zh_CN: 纹理的设备像素尺寸。 */
    QSize textureSize() const;
    /** @brief Frame revision counter incremented on each rendered frame. zh_CN: 每次成功渲染后递增的帧版本号。 */
    quint64 textureRevision() const;
    /** @brief Diagnostics map with texture memory usage, frame count, and last error. zh_CN: 包含显存占用、帧数与最近错误信息的诊断字典。 */
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

#pragma once

#include "QuickCompat_p.h"
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPointer>
#include <QQuickRenderControl>
#include <QWidget>
#if FLUENT_QUICK_NEEDS_PRIVATE_RENDER_WINDOW_HOOK
#include <QtQuick/private/qquickrendercontrol_p.h>
#endif
#if FLUENT_QUICK_USES_RHI
#include <QQuickGraphicsDevice>
#include <QQuickOpenGLUtils>
#include <QQuickRenderTarget>
#endif

namespace compatibility::detail {

#if FLUENT_QUICK_NEEDS_PRIVATE_RENDER_WINDOW_HOOK
class LegacyQuickRenderControlPrivate final : public QQuickRenderControlPrivate {
public:
    LegacyQuickRenderControlPrivate(QQuickRenderControl* control, QWidget* view)
        : QQuickRenderControlPrivate(control), m_view(view)
    {}

    bool isRenderWindow(const QWindow* window) override { return m_view && window == m_view->window()->windowHandle(); }

private:
    QPointer<QWidget> m_view;
};
#endif

class QuickRenderControl final : public QQuickRenderControl {
public:
    explicit QuickRenderControl(QWidget* view)
#if FLUENT_QUICK_NEEDS_PRIVATE_RENDER_WINDOW_HOOK
        // Qt 6.2–6.5 only recognize embedded keyboard focus through this hook.
        // zh_CN: Qt 6.2–6.5 仅通过该内部钩子识别嵌入窗口中的键盘焦点。
        : QQuickRenderControl(*new LegacyQuickRenderControlPrivate(this, view), nullptr), m_view(view)
#else
        : m_view(view)
#endif
    {}
    qreal targetDpr = 0;
    bool renderPhase = false;
    QWindow* renderWindow(QPoint* offset) override
    {
        if (!m_view)
            return nullptr;
#if FLUENT_QUICK_HAS_RENDER_TARGET_DPR
        // A real renderWindow overrides QQuickRenderTarget's DPR. Supersampling
        // therefore uses the target metadata rather than the native window DPR.
        // zh_CN: 真实窗口会覆盖目标 DPR，超采样时改用目标元数据中的比例。
        if (renderPhase && targetDpr > 0 && !qFuzzyCompare(targetDpr, m_view->devicePixelRatioF())) {
            if (offset)
                *offset = QPoint();
            return nullptr;
        }
#endif
        if (offset)
            *offset = m_view->mapTo(m_view->window(), QPoint());
        return m_view->window()->windowHandle();
    }

private:
    QPointer<QWidget> m_view;
};

inline void resetQuickRenderTarget(QQuickWindow* window)
{
#if FLUENT_QUICK_USES_RHI
    window->setRenderTarget(QQuickRenderTarget());
#else
    window->setRenderTarget(static_cast<QOpenGLFramebufferObject*>(nullptr));
#endif
}

inline bool initializeQuickOpenGLRendering(QQuickWindow* window, QQuickRenderControl* control, QOpenGLContext* context)
{
#if FLUENT_QUICK_USES_RHI
    window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(context));
    return control->initialize();
#else
    control->initialize(context);
    return window->isSceneGraphInitialized();
#endif
}

inline QOpenGLFramebufferObjectFormat quickFramebufferFormat()
{
    QOpenGLFramebufferObjectFormat format;
    format.setInternalTextureFormat(GL_RGBA8);
#if !FLUENT_QUICK_USES_RHI
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
#endif
    return format;
}

inline void setQuickRenderTarget(QQuickWindow* window, QOpenGLFramebufferObject* framebuffer, qreal dpr)
{
#if FLUENT_QUICK_USES_RHI
    auto target = QQuickRenderTarget::fromOpenGLTexture(framebuffer->texture(), framebuffer->size());
#if FLUENT_QUICK_HAS_RENDER_TARGET_DPR
    target.setDevicePixelRatio(dpr);
#else
    Q_UNUSED(dpr);
#endif
    window->setRenderTarget(target);
#else
    Q_UNUSED(dpr);
    window->setRenderTarget(framebuffer);
#endif
}

inline void beginQuickFrame(QQuickRenderControl* control)
{
#if FLUENT_QUICK_USES_RHI
    control->beginFrame();
#else
    Q_UNUSED(control);
#endif
}

inline void endQuickFrame(QQuickWindow* window, QQuickRenderControl* control)
{
#if FLUENT_QUICK_USES_RHI
    Q_UNUSED(window);
    control->endFrame();
    QQuickOpenGLUtils::resetOpenGLState();
#else
    Q_UNUSED(control);
    window->resetOpenGLState();
#endif
}

} // namespace compatibility::detail

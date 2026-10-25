#ifndef FLUENTQT_SPATIALFRAMEBUFFERCOMPAT_P_H
#define FLUENTQT_SPATIALFRAMEBUFFERCOMPAT_P_H

#include <QOpenGLContext>
#include <QOpenGLFunctions>

namespace compatibility::detail {
// Inspect the bound RGBA8 paint target once, during allocation. Unknown formats
// retain the conservative allowance for separate depth and stencil storage.
// zh_CN: 分配时检查当前 RGBA8 画布；未知格式仍按独立深度和模板存储保守计费。
inline int currentPaintSampleBytes()
{
    auto* context = QOpenGLContext::currentContext();
    if (!context)
        return 12;
    auto* gl = context->functions();
    GLint depth = 0, stencil = 0, type = 0;
    gl->glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    if (type != GL_RENDERBUFFER)
        return 12;
    gl->glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depth);
    gl->glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &stencil);
    if (!depth || depth != stencil)
        return 12;
    GLint previous = 0, format = 0;
    gl->glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous);
    gl->glBindRenderbuffer(GL_RENDERBUFFER, GLuint(depth));
    gl->glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
    gl->glBindRenderbuffer(GL_RENDERBUFFER, GLuint(previous));
    return format == GL_DEPTH24_STENCIL8 ? 8 : 12;
}
} // namespace compatibility::detail
#endif

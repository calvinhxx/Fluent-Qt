#ifndef FLUENTQT_PAINTENGINESTATECOMPAT_P_H
#define FLUENTQT_PAINTENGINESTATECOMPAT_P_H

#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>

namespace compatibility::detail {

// Qt before 6.5 can replay saved clips with the redirected widget offset applied again.
// Forward the painter's effective clip instead of those intermediate states.
// zh_CN: Qt 6.5 之前重放已保存裁剪时可能再次叠加控件重定向偏移；转发实际有效裁剪，避免采用中间状态。
inline void forwardPaintEngineClip(QPainter& target, const QPaintEngineState& state)
{
    const auto dirty = state.state();
    if (!(dirty & (QPaintEngine::DirtyClipRegion | QPaintEngine::DirtyClipPath | QPaintEngine::DirtyClipEnabled)))
        return;
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    if (const QPainter* source = state.painter()) {
        const QTransform transform = target.worldTransform();
        const qreal inverseDpr = 1 / target.device()->devicePixelRatioF();
        target.setWorldTransform(source->deviceTransform() * QTransform::fromScale(inverseDpr, inverseDpr));
        if (source->hasClipping())
            target.setClipPath(source->clipPath(), Qt::ReplaceClip);
        else
            target.setClipping(false);
        target.setWorldTransform(transform);
        return;
    }
#endif
    if (dirty & QPaintEngine::DirtyClipRegion)
        target.setClipRegion(state.clipRegion(), state.clipOperation());
    if (dirty & QPaintEngine::DirtyClipPath)
        target.setClipPath(state.clipPath(), state.clipOperation());
    if (dirty & QPaintEngine::DirtyClipEnabled)
        target.setClipping(state.isClipEnabled());
}

} // namespace compatibility::detail
#endif

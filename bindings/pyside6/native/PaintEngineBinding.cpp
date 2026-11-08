#include "BindingApi.h"
#include "compatibility/private/PaintEngineStateCompat_p.h"

void forwardPaintEngineClipForBinding(QPainter& target, const QPaintEngineState& state)
{
    ::compatibility::detail::forwardPaintEngineClip(target, state);
}

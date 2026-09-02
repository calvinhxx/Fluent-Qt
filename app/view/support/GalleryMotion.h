#ifndef FLUENTQT_GALLERY_VIEW_SUPPORT_GALLERYMOTION_H
#define FLUENTQT_GALLERY_VIEW_SUPPORT_GALLERYMOTION_H

#include <QAbstractAnimation>

class QVariantAnimation;

namespace fluent::gallery::motion {

// Starts a finite Gallery-owned transition through FluentQt's public motion
// policy. Running transitions are also reconciled when the global preference
// changes, so the Gallery demonstrates the same contract expected of clients.
// zh_CN: 通过 FluentQt 公共动效策略启动 Gallery 自有的有限过渡；全局偏好
// 改变时也会收敛运行中的过渡，以示范客户端应遵循的同一契约。
void startFiniteTransition(
    QVariantAnimation* animation, int fullDurationMs, bool localAnimationEnabled = true,
    QAbstractAnimation::DeletionPolicy deletionPolicy = QAbstractAnimation::KeepWhenStopped);

} // namespace fluent::gallery::motion

#endif // FLUENTQT_GALLERY_VIEW_SUPPORT_GALLERYMOTION_H

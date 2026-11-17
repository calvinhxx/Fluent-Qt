#ifndef FLUENTQT_GALLERY_VIEW_SUPPORT_GALLERYMOTION_H
#define FLUENTQT_GALLERY_VIEW_SUPPORT_GALLERYMOTION_H

#include <QAbstractAnimation>

class QVariantAnimation;

namespace fluent::gallery::motion {

/**
 * @brief Starts a finite Gallery transition governed by FluentQt::MotionPolicy, updating running animations when policy changes.
 * zh_CN: 按 FluentQt::MotionPolicy 启动 Gallery 有限时长的过渡动画，并在策略变更时同步收敛运行中的动画。
 */
void startFiniteTransition(QVariantAnimation* animation,
    int fullDurationMs,
    bool localAnimationEnabled = true,
    QAbstractAnimation::DeletionPolicy deletionPolicy = QAbstractAnimation::KeepWhenStopped);

} // namespace fluent::gallery::motion

#endif // FLUENTQT_GALLERY_VIEW_SUPPORT_GALLERYMOTION_H

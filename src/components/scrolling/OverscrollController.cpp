#include "OverscrollController.h"

#include <utility>

#include <QAbstractAnimation>
#include <QDateTime>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QWidget>

#include "design/Animation.h"

namespace fluent::scrolling {

namespace {
// Rubber-band travel cap, cluster gap, the delay before a settled mouse-wheel overscroll
// bounces back, and the discrete boundary-bounce overshoot range. zh_CN: 橡皮筋位移上限、成簇
// 间隔、静止鼠标滚轮回弹前的延迟，以及离散边界回弹的过冲范围。
constexpr qreal kMaxOverscrollPx = 100.0;
constexpr int   kClusterGapMs = 120;
constexpr int   kNoPhaseBoundaryBounceDelayMs = 16;
constexpr qreal kDiscreteBoundaryOverscrollMinPx = 12.0;
constexpr qreal kDiscreteBoundaryOverscrollMaxPx = 48.0;

int scrollSign(qreal value) {
    if (value > 0.0) return 1;
    if (value < 0.0) return -1;
    return 0;
}
} // namespace

OverscrollController::OverscrollController(QWidget* viewport, qreal discreteStepPx, Hooks hooks,
                                          QObject* parent)
    : QObject(parent)
    , m_viewport(viewport)
    , m_step(discreteStepPx)
    , m_hooks(std::move(hooks))
{
    m_bounceAnim = new QVariantAnimation(this);
    m_bounceAnim->setDuration(::Animation::Duration::Normal);
    m_bounceAnim->setEasingCurve(::Animation::getEasing(::Animation::EasingType::Decelerate));
    connect(m_bounceAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_overscroll = v.toReal();
        notifyChanged();
    });
    connect(m_bounceAnim, &QVariantAnimation::finished, this, [this]() {
        resetNoPhaseCluster();
        resetNoPhaseBoundaryBounce();
    });

    m_bounceTimer = new QTimer(this);
    m_bounceTimer->setSingleShot(true);
    m_bounceTimer->setInterval(kNoPhaseBoundaryBounceDelayMs);
    connect(m_bounceTimer, &QTimer::timeout, this, [this]() {
        m_noPhaseBounceArmed = false;
        startBounceBack();
    });
}

bool OverscrollController::horizontal() const {
    return m_hooks.isHorizontal && m_hooks.isHorizontal();
}

void OverscrollController::setOverscrollEnabled(bool enabled) {
    if (m_overscrollEnabled == enabled)
        return;
    m_overscrollEnabled = enabled;
    if (!enabled)
        cancel();
}

void OverscrollController::setScrollChainingEnabled(bool enabled) {
    if (m_chaining == enabled)
        return;
    m_chaining = enabled;
    if (enabled)
        cancel();
}

void OverscrollController::cancel() {
    m_bounceTimer->stop();
    m_bounceAnim->stop();
    resetNoPhaseCluster();
    resetNoPhaseBoundaryBounce();
    if (!qFuzzyIsNull(m_overscroll)) {
        m_overscroll = 0.0;
        notifyChanged();
    }
}

void OverscrollController::notifyChanged() {
    if (m_hooks.onOverscrollChanged)
        m_hooks.onOverscrollChanged();
}

void OverscrollController::resetNoPhaseCluster() {
    m_lastNoPhaseTs = 0;
    m_clusterAccum = 0.0;
    m_clusterDir = 0;
}

void OverscrollController::resetNoPhaseBoundaryBounce() {
    m_noPhaseBoundaryDir = 0;
    m_noPhaseBounceArmed = false;
}

void OverscrollController::startBounceBack() {
    if (qFuzzyIsNull(m_overscroll)) {
        resetNoPhaseBoundaryBounce();
        return;
    }
    resetNoPhaseCluster();
    m_bounceAnim->stop();
    m_bounceAnim->setStartValue(m_overscroll);
    m_bounceAnim->setEndValue(0.0);
    m_bounceAnim->start();
}

void OverscrollController::handleWheel(QWheelEvent* event) {
    enum class WheelKind { PhaseBased, NoPhasePixel, NoPhaseDiscrete };
    const auto phase = event->phase();
    const bool hasPixelDelta = !event->pixelDelta().isNull();
    const WheelKind kind = (phase != Qt::NoScrollPhase) ? WheelKind::PhaseBased
                         : (hasPixelDelta             ? WheelKind::NoPhasePixel
                                                      : WheelKind::NoPhaseDiscrete);

    const bool h = horizontal();

    // For horizontal flow, pick the dominant axis (trackpad users often swipe vertically even
    // on a horizontal list). zh_CN: 横向流动时取主轴（即便横向列表，触控板也常纵向滑）。
    const int delta = h
        ? (qAbs(event->angleDelta().y()) >= qAbs(event->angleDelta().x())
               ? event->angleDelta().y() : event->angleDelta().x())
        : event->angleDelta().y();

    // Zero-delta (e.g. ScrollEnd on a Windows touchpad with no residual).
    if (delta == 0 && !hasPixelDelta) {
        if (!qFuzzyIsNull(m_overscroll) &&
            (phase == Qt::ScrollEnd || phase == Qt::ScrollMomentum)) {
            startBounceBack();
            event->accept();
            return;
        }
        if (m_hooks.fallbackWheel)
            m_hooks.fallbackWheel(event);
        else
            event->ignore();
        return;
    }

    const qreal scrollPx = hasPixelDelta
        ? static_cast<qreal>(h
              ? (qAbs(event->pixelDelta().y()) >= qAbs(event->pixelDelta().x())
                     ? event->pixelDelta().y() : event->pixelDelta().x())
              : event->pixelDelta().y())
        : delta / 120.0 * m_step;

    auto startNoPhaseBoundaryBounce = [&]() {
        const int boundaryDir = scrollSign(scrollPx);
        if (boundaryDir == 0)
            return;
        if (m_noPhaseBoundaryDir == boundaryDir &&
            (m_noPhaseBounceArmed ||
             m_bounceAnim->state() == QAbstractAnimation::Running)) {
            return;
        }
        const qreal amount = qBound(kDiscreteBoundaryOverscrollMinPx,
                                    qAbs(scrollPx) * 0.5,
                                    kDiscreteBoundaryOverscrollMaxPx);
        m_overscroll = (scrollPx > 0.0) ? amount : -amount;
        m_noPhaseBoundaryDir = boundaryDir;
        m_noPhaseBounceArmed = true;
        notifyChanged();
        if (!m_bounceTimer->isActive())
            m_bounceTimer->start(kNoPhaseBoundaryBounceDelayMs);
    };

    // ── NoPhaseDiscrete (mouse wheel / Mac RDP): scroll content first; only tails that already
    // push past the boundary are consumed as a bounce. zh_CN: 鼠标滚轮/RDP：先滚内容，只有已越界
    // 的尾部才作为回弹被吞。
    if (kind == WheelKind::NoPhaseDiscrete) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const int dir = scrollSign(scrollPx);
        const bool clusterExpired =
            m_lastNoPhaseTs != 0 && now - m_lastNoPhaseTs > kClusterGapMs;
        const bool directionChanged =
            m_clusterDir != 0 && dir != 0 && m_clusterDir != dir;
        if (m_lastNoPhaseTs == 0 || clusterExpired || directionChanged) {
            resetNoPhaseCluster();
            if (clusterExpired || directionChanged)
                resetNoPhaseBoundaryBounce();
        }
        m_lastNoPhaseTs = now;
        if (dir != 0)
            m_clusterDir = dir;
        m_clusterAccum += scrollPx;

        if (!qFuzzyIsNull(m_overscroll)) {
            const bool sameBoundaryDirection =
                (m_overscroll > 0.0 && scrollPx > 0.0) ||
                (m_overscroll < 0.0 && scrollPx < 0.0);
            if (sameBoundaryDirection) {
                notifyChanged();
                event->accept();
                return;
            }
            if (m_bounceAnim->state() == QAbstractAnimation::Running)
                m_bounceAnim->stop();
            m_bounceTimer->stop();
            m_overscroll = 0.0;
            resetNoPhaseBoundaryBounce();
            notifyChanged();
        }

        QScrollBar* sb = m_hooks.scrollBar ? m_hooks.scrollBar() : nullptr;
        if (!sb || sb->maximum() <= sb->minimum()) {
            notifyChanged();
            event->ignore();
            return;
        }

        const int before = sb->value();
        const bool atStart = before <= sb->minimum();
        const bool atEnd   = before >= sb->maximum();
        const bool boundaryTail =
            (atStart && scrollPx > 0.0) || (atEnd && scrollPx < 0.0);

        if (boundaryTail) {
            if (m_chaining) {
                resetNoPhaseBoundaryBounce();
                notifyChanged();
                event->ignore();
                return;
            }
            // Overscroll disabled (e.g. a navigation pane): stop cleanly, no bounce.
            // zh_CN: 关闭回弹（如导航窗格）：干脆停住，不回弹。
            if (!m_overscrollEnabled) {
                notifyChanged();
                event->accept();
                return;
            }
            if (qFuzzyIsNull(m_overscroll))
                startNoPhaseBoundaryBounce();
            notifyChanged();
            event->accept();
            return;
        }

        const bool moved = m_hooks.normalScroll && m_hooks.normalScroll(scrollPx);
        if (moved) {
            resetNoPhaseCluster();
            resetNoPhaseBoundaryBounce();
        }
        notifyChanged();
        event->accept();
        return;
    }

    // ── 1. Already overscrolled: rubber-band (trackpad), or consume a stale residual. ────────
    if (!qFuzzyIsNull(m_overscroll)) {
        if (m_bounceAnim->state() == QAbstractAnimation::Running) {
            // Bounce in progress: only a real finger-press (phase-based) interrupts it;
            // NoPhasePixel residuals are consumed. zh_CN: 回弹中：仅带阶段的真实手势可打断，
            // NoPhasePixel 残量被吞。
            if (kind != WheelKind::PhaseBased) {
                event->accept();
                return;
            }
            m_bounceAnim->stop();
        }
        m_bounceTimer->stop();

        if (phase == Qt::ScrollMomentum || phase == Qt::ScrollEnd) {
            startBounceBack();
            event->accept();
            return;
        }

        const qreal ratio = qMin(qAbs(m_overscroll) / kMaxOverscrollPx, 1.0);
        const qreal damping = (1.0 - ratio) * (1.0 - ratio);
        const qreal prev = m_overscroll;
        m_overscroll += scrollPx * qMax(damping, 0.05) * 0.5;
        m_overscroll = qBound(-kMaxOverscrollPx, m_overscroll, kMaxOverscrollPx);
        if ((prev > 0.0 && m_overscroll <= 0.0) || (prev < 0.0 && m_overscroll >= 0.0))
            m_overscroll = 0.0;
        notifyChanged();
        event->accept();
        return;
    }

    // ── 2. At the boundary: enter overscroll, stop cleanly, or chain to a parent. ────────────
    QScrollBar* sb = m_hooks.scrollBar ? m_hooks.scrollBar() : nullptr;
    if (!sb || sb->maximum() <= sb->minimum()) {
        notifyChanged();
        event->ignore();
        return;
    }

    const bool atStart = sb->value() <= sb->minimum();
    const bool atEnd   = sb->value() >= sb->maximum();
    const bool wantsEnter = (atStart && scrollPx > 0.0) || (atEnd && scrollPx < 0.0);

    if (wantsEnter) {
        if (m_chaining) {
            notifyChanged();
            event->ignore();
            return;
        }
        // Don't enter overscroll from inertia or finger-lift.
        if (phase == Qt::ScrollMomentum || phase == Qt::ScrollEnd) {
            event->accept();
            return;
        }
        if (!m_overscrollEnabled) {
            event->accept();
            return;
        }
        m_overscroll = scrollPx * 0.5;
        notifyChanged();
        event->accept();
        return;
    }

    // ── 3. Normal scroll. ───────────────────────────────────────────────────────────────────
    if (m_hooks.normalScroll)
        m_hooks.normalScroll(scrollPx);
    event->accept();
    notifyChanged();
}

} // namespace fluent::scrolling

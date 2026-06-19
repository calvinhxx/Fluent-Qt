#include "OverscrollController.h"

#include <utility>

#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QWidget>

#include "design/Animation.h"

namespace fluent::scrolling {

namespace {
// Rubber-band travel cap and the delay before a settled, mouse-wheel overscroll bounces back.
// zh_CN: 橡皮筋位移上限，以及静止的鼠标滚轮 overscroll 回弹前的延迟。
constexpr qreal kMaxOverscrollPx = 100.0;
constexpr int kBounceDelayMs = 150;
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

    m_bounceTimer = new QTimer(this);
    m_bounceTimer->setSingleShot(true);
    m_bounceTimer->setInterval(kBounceDelayMs);
    connect(m_bounceTimer, &QTimer::timeout, this, &OverscrollController::startBounceBack);
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
    if (!qFuzzyIsNull(m_overscroll)) {
        m_overscroll = 0.0;
        notifyChanged();
    }
}

void OverscrollController::notifyChanged() {
    if (m_hooks.onOverscrollChanged)
        m_hooks.onOverscrollChanged();
}

void OverscrollController::startBounceBack() {
    if (qFuzzyIsNull(m_overscroll))
        return;
    m_bounceAnim->stop();
    m_bounceAnim->setStartValue(m_overscroll);
    m_bounceAnim->setEndValue(0.0);
    m_bounceAnim->start();
}

void OverscrollController::handleWheel(QWheelEvent* event) {
    const int delta = event->angleDelta().y();
    const auto phase = event->phase();

    // Zero-delta (e.g. ScrollEnd on a Windows touchpad with no residual).
    // zh_CN: 零增量事件（如 Windows 触控板无残量的 ScrollEnd）。
    if (delta == 0 && event->pixelDelta().isNull()) {
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

    const qreal scrollPx = !event->pixelDelta().isNull()
        ? static_cast<qreal>(event->pixelDelta().y())
        : delta / 120.0 * m_step;

    // ── 1. Already overscrolled: rubber-band, or recover on a reverse notch. ──────────
    if (!qFuzzyIsNull(m_overscroll)) {
        if (m_bounceAnim->state() == QAbstractAnimation::Running) {
            // Only consume a NoScrollPhase notch that keeps pushing into the boundary; a
            // reverse notch must interrupt the bounce so the wheel doesn't feel stuck.
            // zh_CN: 仅吞掉继续朝边界推的滚轮；反向滚动需打断回弹，避免卡顿。
            const bool pushingIntoBoundary = (m_overscroll > 0.0 && scrollPx > 0.0) ||
                                             (m_overscroll < 0.0 && scrollPx < 0.0);
            if (phase == Qt::NoScrollPhase && pushingIntoBoundary) {
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

        if (!qFuzzyIsNull(m_overscroll) && phase == Qt::NoScrollPhase)
            m_bounceTimer->start();

        event->accept();
        return;
    }

    // ── 2. At the boundary: enter overscroll, stop cleanly, or chain to a parent. ─────
    QScrollBar* sb = m_hooks.scrollBar ? m_hooks.scrollBar() : nullptr;
    if (!sb || sb->maximum() <= sb->minimum()) {
        event->ignore();
        return;
    }

    const bool atStart = sb->value() <= sb->minimum();
    const bool atEnd = sb->value() >= sb->maximum();

    if ((atStart && scrollPx > 0.0) || (atEnd && scrollPx < 0.0)) {
        if (m_chaining) {
            event->ignore();
            return;
        }
        // Don't enter overscroll from inertia or finger-lift.
        if (phase == Qt::ScrollMomentum || phase == Qt::ScrollEnd) {
            event->accept();
            return;
        }
        // Overscroll disabled (e.g. a navigation pane): stop cleanly at the boundary.
        if (!m_overscrollEnabled) {
            event->accept();
            return;
        }

        m_overscroll = scrollPx * 0.5;
        notifyChanged();

        if (phase == Qt::NoScrollPhase)
            m_bounceTimer->start();

        event->accept();
        return;
    }

    // ── 3. Normal scroll. ─────────────────────────────────────────────────────────────
    const bool moved = m_hooks.normalScroll ? m_hooks.normalScroll(scrollPx) : false;
    if (!moved && m_chaining) {
        event->ignore();
        return;
    }
    event->accept();
}

} // namespace fluent::scrolling

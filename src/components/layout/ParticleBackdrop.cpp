#include "ParticleBackdrop.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPolygonF>
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "compatibility/QtCompat.h"
#include "components/foundation/MotionPolicy.h"

namespace fluent::layout {
namespace {
constexpr double Tau = 6.283185307179586;
QColor alpha(QColor color, double opacity)
{
    color.setAlphaF(std::clamp(opacity, 0.0, 1.0));
    return color;
}
double seed(int index)
{
    const double x = std::sin(index * 127.1 + 311.7) * 43758.5453;
    return x - std::floor(x);
}
} // namespace

class ParticleBackdropPrivate {
public:
    explicit ParticleBackdropPrivate(ParticleBackdrop* widget) : q(widget)
    {
        rebuildParticles();
        for (int i = 0; i < 72; ++i)
            dust[i] = {seed(i), seed(i + 99), 2 + seed(i + 6) * 4};
        for (int i = 0; i <= 192; ++i) {
            const double angle = i * Tau / 192;
            unitCircle[i] = QPointF(std::cos(angle), std::sin(angle));
        }
        timer.setTimerType(Qt::PreciseTimer);
        timer.setInterval(qCeil(1000.0 / maximumFrameRate));
        QObject::connect(&timer, &QTimer::timeout, q, [this] {
            const double delta = std::min(clock.nsecsElapsed() / 1e9, .1);
            clock.restart();
            elapsed += delta * speed;
            pointerStrength +=
                ((pointerActive ? 1.0 : 0.0) - pointerStrength) * std::min(1.0, delta * 6);
            pulses.erase(std::remove_if(
                             pulses.begin(), pulses.end(),
                             [this](const Pulse& pulse) { return elapsed - pulse.started > 1.6; }),
                         pulses.end());
            q->update();
        });
        QObject::connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, q,
                         [this] { syncTimer(); });
        QObject::connect(qApp, &QGuiApplication::applicationStateChanged, q,
                         [this] { syncTimer(); });
    }
    struct Particle {
        double phase, spread, radius, opacity;
    };
    struct Dust {
        double x, y, speed;
    };
    struct Pulse {
        QPointF center;
        double started;
    };
    struct FieldParticle {
        double x, y, phase, depth;
    };
    ParticleBackdrop* q;
    QTimer timer;
    QElapsedTimer clock;
    bool animationEnabled = true, interactive = false, pauseWhenInactive = true;
    bool pointerActive = false, visibilityQueued = false;
    qreal speed = 1;
    int particleCount = 240, maximumFrameRate = 30;
    ParticleBackdrop::Effect effect = ParticleBackdrop::FlowingRibbons;
    ParticleBackdrop::BackgroundMode backgroundMode = ParticleBackdrop::Transparent;
    QMarginsF margins;
    double elapsed = 0, pointerStrength = 0;
    QPointF pointer;
    std::array<std::vector<Particle>, 3> particles;
    std::vector<FieldParticle> fieldParticles;
    std::array<Dust, 72> dust;
    std::array<QPointF, 193> unitCircle;
    std::vector<Pulse> pulses;
    std::vector<QPointer<QWidget>> ancestors;
    QPixmap backdrop;
    std::array<QPixmap, 4> fades;
    std::array<QPixmap, 3> dotSprites;
    QImage layer;
    QSize cachedPixels;
    qreal cachedDpr = 0;

    int width() const { return q->width(); }
    int height() const { return q->height(); }
    void rebuildParticles()
    {
        if (effect != ParticleBackdrop::FlowingRibbons) {
            for (auto& lane : particles)
                lane.clear();
            fieldParticles.resize(particleCount);
            for (int i = 0; i < particleCount; ++i)
                fieldParticles[i] = {seed(i + 3000), seed(i + 4000), seed(i + 5000),
                                     seed(i + 6000)};
            return;
        }
        fieldParticles.clear();
        for (int lane = 0; lane < 3; ++lane) {
            auto& values = particles[lane];
            const int count = particleCount / 3 + (lane < particleCount % 3 ? 1 : 0);
            values.resize(count);
            for (int i = 0; i < count; ++i)
                values[i] = {seed(i + lane * 160) * Tau, (seed(i + 91) - .5) * 2,
                             .6 + seed(i + 11) * 1.4, .3 + seed(i + 16) * .65};
        }
    }
    bool motionAllowed() const
    {
        return speed > 0 && q->isEnabled() && q->effectiveTheme() != FluentElement::HighContrast &&
               MotionPolicy::instance().shouldAnimate(animationEnabled,
                                                      MotionPolicy::Kind::Continuous);
    }
    bool inViewport() const
    {
        if (!q->isVisible() || q->window()->isMinimized())
            return false;
        QRect clipped = q->rect();
        const QWidget* child = q;
        for (const QWidget* parent = q->parentWidget(); parent; parent = parent->parentWidget()) {
            clipped.translate(child->pos());
            clipped &= parent->rect();
            if (clipped.isEmpty())
                return false;
            child = parent;
        }
        return !clipped.isEmpty();
    }
    void syncTimer()
    {
        const bool run = motionAllowed() && inViewport() &&
                         (!pauseWhenInactive || qApp->applicationState() == Qt::ApplicationActive);
        if (run == timer.isActive())
            return;
        if (run) {
            clock.start();
            timer.start();
        } else
            timer.stop();
        emit q->animatingChanged(run);
    }
    void watchAncestors()
    {
        for (const auto& ancestor : ancestors)
            if (ancestor)
                ancestor->removeEventFilter(q);
        ancestors.clear();
        for (QWidget* parent = q->parentWidget(); parent; parent = parent->parentWidget()) {
            parent->installEventFilter(q);
            ancestors.push_back(parent);
        }
    }
    void scheduleVisibility()
    {
        if (visibilityQueued)
            return;
        visibilityQueued = true;
        QTimer::singleShot(0, q, [this] {
            visibilityQueued = false;
            watchAncestors();
            syncTimer();
        });
    }
    void invalidate()
    {
        cachedPixels = {};
        backdrop = {};
        layer = {};
        fades = {};
        dotSprites = {};
    }
    bool hasFade() const { return !margins.isNull(); }
    QRectF fadeRect(int edge) const
    {
        switch (edge) {
        case 0:
            return QRectF(0, 0, std::min(margins.left(), qreal(width())), height());
        case 1:
            return QRectF(0, 0, width(), std::min(margins.top(), qreal(height())));
        case 2: {
            const qreal w = std::min(margins.right(), qreal(width()));
            return QRectF(width() - w, 0, w, height());
        }
        default: {
            const qreal h = std::min(margins.bottom(), qreal(height()));
            return QRectF(0, height() - h, width(), h);
        }
        }
    }
    void ensureCache(const QColor& background, const std::array<QColor, 3>& hues, bool dark)
    {
        const qreal dpr = q->devicePixelRatioF();
        const QSize pixels(qCeil(width() * dpr), qCeil(height() * dpr));
        if (pixels == cachedPixels && dpr == cachedDpr)
            return;
        cachedPixels = pixels;
        cachedDpr = dpr;
        if (effect == ParticleBackdrop::FloatingDots) {
            // Small reusable raster sprites avoid per-particle gradients and blur passes.
            for (int hue = 0; hue < 3; ++hue) {
                dotSprites[hue] = QPixmap(QSize(qCeil(32 * dpr), qCeil(32 * dpr)));
                dotSprites[hue].setDevicePixelRatio(dpr);
                dotSprites[hue].fill(Qt::transparent);
                QPainter sprite(&dotSprites[hue]);
                QRadialGradient glow(QPointF(16, 16), 16);
                glow.setColorAt(0, alpha(hues[hue], .85));
                glow.setColorAt(.10, alpha(hues[hue], .65));
                glow.setColorAt(.20, alpha(hues[hue], .16));
                glow.setColorAt(.50, alpha(hues[hue], .05));
                glow.setColorAt(1, Qt::transparent);
                sprite.fillRect(QRectF(0, 0, 32, 32), glow);
            }
        }
        if (backgroundMode == ParticleBackdrop::Solid) {
            backdrop = QPixmap(pixels);
            backdrop.setDevicePixelRatio(dpr);
            backdrop.fill(background);
            QPainter painter(&backdrop);
            QRadialGradient glow(QPointF(width() * .76, height() * .42), width() * .52);
            glow.setColorAt(0, alpha(hues[1], dark ? .12 : .08));
            glow.setColorAt(1, Qt::transparent);
            painter.fillRect(q->rect(), glow);
        } else if (hasFade()) {
            layer = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
            layer.setDevicePixelRatio(dpr);
        }
        for (int edge = 0; edge < 4; ++edge) {
            const QRectF bounds = fadeRect(edge);
            if (bounds.isEmpty())
                continue;
            const bool horizontal = edge % 2 == 0;
            const QSize tilePixels(horizontal ? qCeil(bounds.width() * dpr) : qCeil(dpr),
                                   horizontal ? qCeil(dpr) : qCeil(bounds.height() * dpr));
            fades[edge] = QPixmap(tilePixels);
            fades[edge].setDevicePixelRatio(dpr);
            fades[edge].fill(Qt::transparent);
            QPainter painter(&fades[edge]);
            const qreal length = horizontal ? bounds.width() : bounds.height();
            QLinearGradient fade(QPointF(0, 0),
                                 horizontal ? QPointF(length, 0) : QPointF(0, length));
            const bool reverse = edge >= 2;
            const bool solid = backgroundMode == ParticleBackdrop::Solid;
            const QColor color = solid ? background : QColor(Qt::black);
            fade.setColorAt(0, alpha(color, reverse == solid ? 0 : 1));
            if (edge == 0)
                fade.setColorAt(.48, alpha(color, solid ? .94 : .06));
            fade.setColorAt(1, alpha(color, reverse == solid ? 1 : 0));
            painter.fillRect(QRectF(0, 0, tilePixels.width() / dpr, tilePixels.height() / dpr),
                             fade);
        }
    }
    void paint(QPainter& painter)
    {
        const auto& colors = q->themeColorsRef();
        if (q->effectiveTheme() == FluentElement::HighContrast) {
            if (backgroundMode == ParticleBackdrop::Solid)
                painter.fillRect(q->rect(), colors.bgSolid);
            return;
        }
        const bool dark = q->effectiveThemeUsesDarkAppearance();
        const QColor accent = colors.accentDefault;
        const qreal hue = accent.hsvHueF() < 0 ? .55 : accent.hsvHueF();
        std::array<QColor, 3> hues;
        const double offsets[] = {-.10, .06, -.03};
        for (int i = 0; i < 3; ++i)
            hues[i] = QColor::fromHsvF(std::fmod(hue + offsets[i] + 1, 1.0), dark ? .60 : .78,
                                       dark ? .95 : .66);
        ensureCache(colors.bgSolid, hues, dark);
        if (backgroundMode == ParticleBackdrop::Solid) {
            painter.drawPixmap(0, 0, backdrop);
            draw(painter, hues);
            applyFades(painter);
        } else if (!layer.isNull()) {
            // Mask only this isolated image, never the parent's shared backing-store pixels.
            layer.fill(Qt::transparent);
            QPainter effect(&layer);
            draw(effect, hues);
            effect.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            applyFades(effect);
            effect.end();
            painter.drawImage(0, 0, layer);
        } else {
            draw(painter, hues);
        }
    }
    void applyFades(QPainter& painter)
    {
        for (int edge = 0; edge < 4; ++edge) {
            if (fades[edge].isNull())
                continue;
            painter.save();
            const QRectF bounds = fadeRect(edge);
            painter.setClipRect(bounds);
            painter.drawTiledPixmap(bounds, fades[edge]);
            painter.restore();
        }
    }
    void draw(QPainter& p, const std::array<QColor, 3>& hues)
    {
        if (width() <= 0 || height() <= 0)
            return;
        p.setRenderHint(QPainter::Antialiasing);
        switch (effect) {
        case ParticleBackdrop::FlowingRibbons:
            drawRibbons(p, hues);
            break;
        case ParticleBackdrop::FloatingDots:
            drawFloatingDots(p);
            break;
        case ParticleBackdrop::Starfield:
            drawStarfield(p, hues);
            break;
        }
        drawPulses(p, hues[2]);
    }
    void drawFloatingDots(QPainter& p)
    {
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        for (int i = 0; i < int(fieldParticles.size()); ++i) {
            const auto& particle = fieldParticles[i];
            const double depth = particle.depth;
            const double x = particle.x + std::sin(elapsed * .18 + particle.phase * Tau) * .025;
            const double y = particle.y - elapsed * (.008 + depth * .014);
            const QPointF at =
                displace(QPointF((x - std::floor(x)) * width(), (y - std::floor(y)) * height()));
            const double radius = 4 + depth * 10;
            // Fade at the wrap boundaries; a dot never jumps visibly to the other edge.
            const double edge = std::min({at.x(), width() - at.x(), at.y(), height() - at.y()});
            p.setOpacity(std::clamp(edge / (radius * 2), 0.0, 1.0) * (.45 + depth * .55));
            const auto& sprite = dotSprites[i % 3];
            p.drawPixmap(QRectF(at.x() - radius, at.y() - radius, radius * 2, radius * 2), sprite,
                         QRectF(0, 0, sprite.width(), sprite.height()));
        }
        p.setOpacity(1);
    }
    void drawStarfield(QPainter& p, const std::array<QColor, 3>& hues)
    {
        const QPointF center(width() * .5, height() * .48);
        for (int i = 0; i < int(fieldParticles.size()); ++i) {
            const auto& particle = fieldParticles[i];
            const double cycle = particle.phase + elapsed * (.055 + particle.depth * .035);
            const double phase = cycle - std::floor(cycle);
            const double z = 1 - phase * .88;
            const QPointF ray((particle.x - .5) * width() * 1.5,
                              (particle.y - .5) * height() * 1.5);
            const QPointF at = displace(center + ray * (.32 / z));
            if (!q->rect().adjusted(-8, -8, 8, 8).contains(at.toPoint()))
                continue;
            // Project a short segment at a greater depth, without crossing the respawn plane.
            const QPointF tail = displace(center + ray * (.32 / std::min(1.0, z + .025)));
            const double fade = std::min({1.0, phase / .08, (1 - phase) / .12});
            const double opacity = fade * (.25 + phase * .65);
            p.setPen(
                QPen(alpha(hues[i % 3], opacity * .45), .7 + phase, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(tail, at);
            p.setPen(Qt::NoPen);
            p.setBrush(alpha(hues[i % 3], opacity));
            const double radius = .6 + phase * 1.5;
            p.drawEllipse(at, radius, radius);
        }
    }
    void drawRibbons(QPainter& p, const std::array<QColor, 3>& hues)
    {
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 72; ++i) {
            const double x = std::fmod(dust[i].x * width() + elapsed * dust[i].speed, width());
            const double y = dust[i].y * height();
            p.setBrush(alpha(hues[1], .18));
            p.drawEllipse(QPointF(x, y), 1.1, 1.1);
        }
        for (int lane = 0; lane < 3; ++lane) {
            QPolygonF ribbon;
            ribbon.reserve(141);
            for (int step = 0; step <= 140; ++step) {
                const QPointF at = position(step / 140.0 * Tau, lane);
                ribbon.append(at);
            }
            std::array<QLineF, 140> ribbonSegments;
            for (int i = 0; i < 140; ++i)
                ribbonSegments[i] = QLineF(ribbon[i], ribbon[i + 1]);
            p.setBrush(Qt::NoBrush);
            const double widths[] = {18, 5, .8};
            const double opacities[] = {.025, .08, .38};
            for (int layer = 0; layer < 3; ++layer) {
                // Very faint wide strokes hide pixel edges; only the bright core needs AA.
                p.setRenderHint(QPainter::Antialiasing, layer == 2);
                p.setPen(QPen(alpha(hues[lane], opacities[layer]), widths[layer], Qt::SolidLine,
                              Qt::FlatCap));
                // Batch thin segments instead of rasterizing a large stroked AA path.
                if (layer == 2)
                    p.drawLines(ribbonSegments.data(), int(ribbonSegments.size()));
                else
                    p.drawPolyline(ribbon);
            }
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            for (int i = 0; i < int(particles[lane].size()); ++i) {
                const auto& particle = particles[lane][i];
                const double angle = particle.phase + elapsed * (.09 + lane * .025);
                const auto at = position(angle, lane, particle.spread);
                const double radius = particle.radius;
                p.setBrush(alpha(hues[lane], particle.opacity));
                p.drawEllipse(at, radius, radius);
            }
            p.setBrush(Qt::NoBrush);
            for (int spark = 0; spark < 6; ++spark) {
                const double angle = spark / 6.0 * Tau + elapsed * (.22 + lane * .035) + lane;
                for (int tail = 3; tail >= 0; --tail) {
                    QPainterPath trail;
                    for (int step = 0; step <= 5; ++step) {
                        const auto at = position(angle - (tail * 5 + step) * .009, lane);
                        if (!step)
                            trail.moveTo(at);
                        else
                            trail.lineTo(at);
                    }
                    p.setRenderHint(QPainter::Antialiasing, false);
                    p.setPen(QPen(alpha(hues[lane], .07), 8 - tail, Qt::SolidLine, Qt::RoundCap));
                    p.drawPath(trail);
                    p.setRenderHint(QPainter::Antialiasing);
                    p.setPen(QPen(alpha(hues[lane], .95 - tail * .22), 2.6 - tail * .5,
                                  Qt::SolidLine, Qt::RoundCap));
                    p.drawPath(trail);
                }
            }
        }
        p.setBrush(Qt::NoBrush);
    }
    void drawPulses(QPainter& p, const QColor& hue)
    {
        p.setBrush(Qt::NoBrush);
        for (const auto& pulse : pulses) {
            const double age = elapsed - pulse.started;
            p.setPen(
                QPen(alpha(hue, .35 * std::pow(1 - age / 1.6, 2)), 1, Qt::SolidLine, Qt::FlatCap));
            std::array<QLineF, 192> ring;
            for (int i = 0; i < 192; ++i)
                ring[i] = QLineF(pulse.center + unitCircle[i] * (age * 280),
                                 pulse.center + unitCircle[i + 1] * (age * 280));
            p.drawLines(ring.data(), int(ring.size()));
        }
    }
    QPointF position(double angle, int lane, double spread = 0) const
    {
        const double tilt = -.36 + lane * .16;
        const double x = std::cos(angle) * (width() * .38 + spread * 19);
        const double y = std::sin(angle) * (height() * .23 + spread * 14) +
                         std::sin(angle * 2 + elapsed * .22 + lane * 1.8) * height() * .045;
        QPointF point(width() * .76 + x * std::cos(tilt) - y * std::sin(tilt),
                      height() * .40 + x * std::sin(tilt) + y * std::cos(tilt));
        return displace(point);
    }
    QPointF displace(QPointF point) const
    {
        QPointF d = point - pointer;
        const double influence =
            pointerStrength > .001
                ? std::exp(-(d.x() * d.x() + d.y() * d.y()) / 26000) * pointerStrength
                : 0;
        point += QPointF(d.x() * .24 - d.y() * .32, d.y() * .24 + d.x() * .32) * influence;
        for (const auto& pulse : pulses) {
            d = point - pulse.center;
            const double distance = std::hypot(d.x(), d.y());
            const double age = elapsed - pulse.started;
            const double offset = (distance - age * 280) / 38;
            if (distance > 1)
                point += d / distance * (std::exp(-offset * offset) * (1 - age / 1.6) * 24);
        }
        return point;
    }
};

ParticleBackdrop::ParticleBackdrop(QWidget* parent)
    : QWidget(parent), d(std::make_unique<ParticleBackdropPrivate>(this))
{
    setFocusPolicy(Qt::NoFocus);
    d->watchAncestors();
}
ParticleBackdrop::~ParticleBackdrop() = default;
ParticleBackdrop::Effect ParticleBackdrop::effect() const
{
    return d->effect;
}
void ParticleBackdrop::setEffect(Effect effect)
{
    if ((effect != FlowingRibbons && effect != FloatingDots && effect != Starfield) ||
        d->effect == effect)
        return;
    d->effect = effect;
    d->elapsed = 0;
    d->pointerActive = false;
    d->pointerStrength = 0;
    d->pulses.clear();
    d->clock.restart();
    d->rebuildParticles();
    d->invalidate();
    update();
    emit effectChanged(effect);
}
bool ParticleBackdrop::isAnimationEnabled() const
{
    return d->animationEnabled;
}
bool ParticleBackdrop::isAnimating() const
{
    return d->timer.isActive();
}
qreal ParticleBackdrop::speed() const
{
    return d->speed;
}
int ParticleBackdrop::particleCount() const
{
    return d->particleCount;
}
int ParticleBackdrop::maximumFrameRate() const
{
    return d->maximumFrameRate;
}
bool ParticleBackdrop::isInteractive() const
{
    return d->interactive;
}
bool ParticleBackdrop::isPauseWhenInactive() const
{
    return d->pauseWhenInactive;
}
ParticleBackdrop::BackgroundMode ParticleBackdrop::backgroundMode() const
{
    return d->backgroundMode;
}
QMarginsF ParticleBackdrop::fadeMargins() const
{
    return d->margins;
}

void ParticleBackdrop::setAnimationEnabled(bool enabled)
{
    if (d->animationEnabled == enabled)
        return;
    d->animationEnabled = enabled;
    QPointer<ParticleBackdrop> guard(this);
    d->syncTimer();
    if (guard)
        emit animationEnabledChanged(enabled);
}
void ParticleBackdrop::setSpeed(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp(value, qreal(0), qreal(4));
    if (d->speed == value)
        return;
    d->speed = value;
    QPointer<ParticleBackdrop> guard(this);
    d->syncTimer();
    if (guard)
        emit speedChanged(value);
}
void ParticleBackdrop::setParticleCount(int count)
{
    count = std::clamp(count, 0, 960);
    if (d->particleCount == count)
        return;
    d->particleCount = count;
    d->rebuildParticles();
    update();
    emit particleCountChanged(count);
}
void ParticleBackdrop::setMaximumFrameRate(int fps)
{
    fps = std::clamp(fps, 1, 60);
    if (d->maximumFrameRate == fps)
        return;
    d->maximumFrameRate = fps;
    d->timer.setInterval(qCeil(1000.0 / fps));
    d->clock.restart();
    emit maximumFrameRateChanged(fps);
}
void ParticleBackdrop::setInteractive(bool value)
{
    if (d->interactive == value)
        return;
    d->interactive = value;
    setMouseTracking(value);
    if (!value)
        d->pointerActive = false;
    emit interactiveChanged(value);
}
void ParticleBackdrop::setPauseWhenInactive(bool pause)
{
    if (d->pauseWhenInactive == pause)
        return;
    d->pauseWhenInactive = pause;
    QPointer<ParticleBackdrop> guard(this);
    d->syncTimer();
    if (guard)
        emit pauseWhenInactiveChanged(pause);
}
void ParticleBackdrop::setBackgroundMode(BackgroundMode mode)
{
    if ((mode != Transparent && mode != Solid) || d->backgroundMode == mode)
        return;
    d->backgroundMode = mode;
    setAttribute(Qt::WA_OpaquePaintEvent, mode == Solid);
    d->invalidate();
    update();
    emit backgroundModeChanged(mode);
}
void ParticleBackdrop::setFadeMargins(const QMarginsF& margins)
{
    if (!std::isfinite(margins.left()) || !std::isfinite(margins.top()) ||
        !std::isfinite(margins.right()) || !std::isfinite(margins.bottom()))
        return;
    const QMarginsF normalized(std::max(0.0, margins.left()), std::max(0.0, margins.top()),
                               std::max(0.0, margins.right()), std::max(0.0, margins.bottom()));
    if (d->margins == normalized)
        return;
    d->margins = normalized;
    d->invalidate();
    update();
    emit fadeMarginsChanged(normalized);
}
void ParticleBackdrop::triggerRipple(const QPointF& position)
{
    if (!std::isfinite(position.x()) || !std::isfinite(position.y()) || !d->motionAllowed())
        return;
    if (d->pulses.size() == 3)
        d->pulses.erase(d->pulses.begin());
    d->pulses.push_back({position, d->elapsed});
    update();
}
void ParticleBackdrop::onThemeUpdated()
{
    if (!d)
        return;
    d->invalidate();
    QPointer<ParticleBackdrop> guard(this);
    d->syncTimer();
    if (guard)
        update();
}
bool ParticleBackdrop::event(QEvent* e)
{
    const bool result = QWidget::event(e);
    if (!d)
        return result;
    switch (e->type()) {
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ParentChange:
    case QEvent::WindowStateChange:
    case QEvent::EnabledChange:
        d->scheduleVisibility();
        d->syncTimer();
        break;
    default:
        break;
    }
    return result;
}
bool ParticleBackdrop::eventFilter(QObject* watched, QEvent* e)
{
    switch (e->type()) {
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ParentChange:
    case QEvent::WindowStateChange:
        d->scheduleVisibility();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, e);
}
void ParticleBackdrop::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    d->paint(painter);
}
void ParticleBackdrop::mouseMoveEvent(QMouseEvent* event)
{
    if (d->interactive) {
        d->pointer = fluentMousePos(event);
        d->pointerActive = true;
    }
    QWidget::mouseMoveEvent(event);
}
void ParticleBackdrop::mousePressEvent(QMouseEvent* event)
{
    if (d->interactive && event->button() == Qt::LeftButton)
        triggerRipple(fluentMousePos(event));
    QWidget::mousePressEvent(event);
}
void ParticleBackdrop::leaveEvent(QEvent* event)
{
    d->pointerActive = false;
    QWidget::leaveEvent(event);
}
} // namespace fluent::layout

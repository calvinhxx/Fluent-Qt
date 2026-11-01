#include "compatibility/InputEventCompat.h"
#include "compatibility/private/PointerEventCompat_p.h"
#include "compatibility/private/WidgetPlatformCompat_p.h"
#include "ImageViewer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <QCoreApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QGestureEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPinchGesture>
#include <QResizeEvent>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "components/foundation/private/MotionPolicy_p.h"
#include "private/ImageViewerAccessibility_p.h"
#include "private/ImageViewerChrome_p.h"
#include "private/ImageViewerPixmapCache_p.h"

namespace fluent::scrolling {

namespace {

constexpr qreal kWheelZoomStep = 1.2;
constexpr qreal kPanStep = 48.0;
constexpr qreal kPanTolerance = 0.5;
constexpr int kChromeMargin = 12;
constexpr qint64 kNativeZoomPinchSuppressionMs = 350;
// Browser-style steps used by zoomIn() / zoomOut(), the toolbar and the keyboard.
// zh_CN: zoomIn() / zoomOut()、工具条和键盘使用的浏览器式缩放档位。
constexpr std::array<qreal, 13> kZoomPresets = {0.1, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0,
    10.0};

bool sameZoom(qreal a, qreal b)
{
    return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
}

bool samePoint(const QPointF& a, const QPointF& b)
{
    return std::abs(a.x() - b.x()) <= 1e-9 && std::abs(a.y() - b.y()) <= 1e-9;
}

QString viewerText(const char* source)
{
    return QCoreApplication::translate("ImageViewer", source);
}

} // namespace

ImageViewer::ImageViewer(QWidget* parent)
    : QWidget(parent), m_pixmapCache(std::make_unique<detail::ImageViewerPixmapCache>())
{
    detail::ensureImageViewerAccessibilityFactory();
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoSystemBackground);
    ::compatibility::detail::registerPinchGesture(this);

    m_viewAnimation = new QVariantAnimation(this);
    m_viewAnimation->setStartValue(0.0);
    m_viewAnimation->setEndValue(1.0);
    connect(m_viewAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        updateAnimationFrame(value.toReal());
    });
    // QVariantAnimation skips valueChanged when a zero-length run repeats the previous end value.
    // zh_CN: 零时长动画的终值与上次相同时，QVariantAnimation 不会发出 valueChanged。
    connect(m_viewAnimation, &QVariantAnimation::finished, this, [this]() { updateAnimationFrame(1.0); });

    m_zoomBar = new ImageViewerZoomBar(
        [this](ImageViewerZoomBar::Command command) {
            switch (command) {
            case ImageViewerZoomBar::Command::ZoomOut:
                stepZoom(false, true, ChangeSource::Program);
                break;
            case ImageViewerZoomBar::Command::ZoomIn:
                stepZoom(true, true, ChangeSource::Program);
                break;
            case ImageViewerZoomBar::Command::Fit:
                fitWithMode(FitMode::Contain, true, ChangeSource::Program);
                break;
            case ImageViewerZoomBar::Command::ActualSize:
                zoomAround(1.0, viewportCenter(), true, ChangeSource::Program);
                break;
            case ImageViewerZoomBar::Command::Reset:
                fitWithMode(FitMode::ScaleDown, true, ChangeSource::Program);
                break;
            }
        },
        this);
    m_zoomIndicator = new ImageViewerZoomIndicator(this);
    updateChrome();
}

ImageViewer::~ImageViewer()
{
    if (m_viewAnimation)
        m_viewAnimation->stop();
}

// ── Image. zh_CN: 图片 ─────────────────────────────────────────────────────

void ImageViewer::setPixmap(const QPixmap& pixmap)
{
    if (pixmap.cacheKey() == m_pixmap.cacheKey())
        return;

    stopViewAnimation();
    const bool couldZoomIn = hasImage() && m_zoomFactor < effectiveMaxZoom();
    const bool couldZoomOut = hasImage() && m_zoomFactor > effectiveMinZoom();
    m_pixmap = pixmap;
    m_imageSize = pixmap.isNull() ? QSizeF() : QSizeF(pixmap.size()) / pixmap.devicePixelRatio();
    m_pixmapCache->clear();
    m_dragging = false;

    const qreal zoom = hasImage() ? fitTargetZoom(FitMode::ScaleDown) : 1.0;
    const QPointF center = clampedCenter(zoom, imageCenter());
    const bool zoomChanged = !sameZoom(zoom, m_zoomFactor);
    const bool centerChanged = !samePoint(center, m_viewCenter);
    const bool modeChanged = m_fitMode != FitMode::ScaleDown;
    m_zoomFactor = zoom;
    m_viewCenter = center;
    m_fitMode = FitMode::ScaleDown;
    if (m_zoomIndicator)
        m_zoomIndicator->dismiss();

    updateCursorShape();
    updateChrome();
    update();
    emit pixmapChanged();
    if (zoomChanged)
        emit zoomFactorChanged(m_zoomFactor);
    if (centerChanged)
        emit viewCenterChanged(m_viewCenter);
    if (modeChanged)
        emit fitModeChanged(m_fitMode);
    const bool canZoomIn = hasImage() && m_zoomFactor < effectiveMaxZoom();
    const bool canZoomOut = hasImage() && m_zoomFactor > effectiveMinZoom();
    detail::notifyImageViewerAccessibilityImageChanged(this, couldZoomIn != canZoomIn || couldZoomOut != canZoomOut);
}

void ImageViewer::setImage(const QImage& image)
{
    setPixmap(image.isNull() ? QPixmap() : QPixmap::fromImage(image));
}

qreal ImageViewer::fitZoomFactor() const
{
    if (!hasImage() || width() <= 0 || height() <= 0)
        return 1.0;
    return std::min(width() / m_imageSize.width(), height() / m_imageSize.height());
}

// ── Properties. zh_CN: 属性 ────────────────────────────────────────────────

void ImageViewer::setZoomFactor(qreal factor)
{
    stopViewAnimation();
    const qreal target = clampedZoom(factor);
    if (sameZoom(target, m_zoomFactor))
        return;
    applyView(target, m_viewCenter, FitMode::Manual, ChangeSource::Program);
}

void ImageViewer::setMinZoomFactor(qreal factor)
{
    if (!std::isfinite(factor))
        return;
    factor = std::max(0.01, factor);
    if (qFuzzyCompare(m_minZoomFactor, factor))
        return;

    m_minZoomFactor = factor;
    const bool maxChanged = m_maxZoomFactor < m_minZoomFactor;
    if (maxChanged)
        m_maxZoomFactor = m_minZoomFactor;
    finishViewAnimation();
    emit minZoomFactorChanged(m_minZoomFactor);
    if (maxChanged)
        emit maxZoomFactorChanged(m_maxZoomFactor);
    reapplyConstraints();
}

void ImageViewer::setMaxZoomFactor(qreal factor)
{
    if (!std::isfinite(factor))
        return;
    factor = std::max(0.01, factor);
    if (qFuzzyCompare(m_maxZoomFactor, factor))
        return;

    m_maxZoomFactor = factor;
    const bool minChanged = m_minZoomFactor > m_maxZoomFactor;
    if (minChanged)
        m_minZoomFactor = m_maxZoomFactor;
    finishViewAnimation();
    emit maxZoomFactorChanged(m_maxZoomFactor);
    if (minChanged)
        emit minZoomFactorChanged(m_minZoomFactor);
    reapplyConstraints();
}

void ImageViewer::setViewCenter(const QPointF& center)
{
    stopViewAnimation();
    if (!std::isfinite(center.x()) || !std::isfinite(center.y()))
        return;
    const QPointF target = clampedCenter(m_zoomFactor, center);
    if (samePoint(target, m_viewCenter))
        return;
    applyView(m_zoomFactor, target, FitMode::Manual, ChangeSource::Program);
}

void ImageViewer::setFitMode(FitMode mode)
{
    if (m_fitMode == mode)
        return;
    stopViewAnimation();
    if (mode != FitMode::Manual && hasImage()) {
        fitWithMode(mode, false, ChangeSource::Program);
        return;
    }
    m_fitMode = mode;
    updateChrome();
    emit fitModeChanged(m_fitMode);
}

void ImageViewer::setWheelAction(WheelAction action)
{
    if (m_wheelAction == action)
        return;
    m_wheelAction = action;
    emit wheelActionChanged(m_wheelAction);
}

void ImageViewer::setControlsVisibility(ControlsVisibility visibility)
{
    if (m_controlsVisibility == visibility)
        return;
    m_controlsVisibility = visibility;
    updateChrome();
    emit controlsVisibilityChanged(m_controlsVisibility);
}

void ImageViewer::setZoomIndicatorEnabled(bool enabled)
{
    if (m_zoomIndicatorEnabled == enabled)
        return;
    m_zoomIndicatorEnabled = enabled;
    if (!enabled && m_zoomIndicator)
        m_zoomIndicator->dismiss();
    emit zoomIndicatorEnabledChanged(m_zoomIndicatorEnabled);
}

void ImageViewer::setFrameVisible(bool visible)
{
    if (m_frameVisible == visible)
        return;
    m_frameVisible = visible;
    update();
    emit frameVisibleChanged(m_frameVisible);
}

void ImageViewer::setTransformationMode(Qt::TransformationMode mode)
{
    if (m_transformationMode == mode)
        return;
    m_transformationMode = mode;
    m_pixmapCache->clear();
    update();
    emit transformationModeChanged(m_transformationMode);
}

// ── Commands. zh_CN: 命令 ──────────────────────────────────────────────────

void ImageViewer::zoomIn(bool animated)
{
    stepZoom(true, animated, ChangeSource::Program);
}

void ImageViewer::zoomOut(bool animated)
{
    stepZoom(false, animated, ChangeSource::Program);
}

void ImageViewer::zoomTo(qreal factor, bool animated)
{
    zoomAround(factor, viewportCenter(), animated, ChangeSource::Program);
}

void ImageViewer::zoomTo(qreal factor, const QPointF& viewportAnchor, bool animated)
{
    zoomAround(factor, viewportAnchor, animated, ChangeSource::Program);
}

void ImageViewer::zoomToActualSize(bool animated)
{
    zoomAround(1.0, viewportCenter(), animated, ChangeSource::Program);
}

void ImageViewer::fitToView(bool animated)
{
    fitWithMode(FitMode::Contain, animated, ChangeSource::Program);
}

void ImageViewer::resetView(bool animated)
{
    fitWithMode(FitMode::ScaleDown, animated, ChangeSource::Program);
}

void ImageViewer::panBy(qreal dx, qreal dy)
{
    stopViewAnimation();
    panByInternal(QPointF(dx, dy), ChangeSource::Program);
}

QPointF ImageViewer::mapToImage(const QPointF& viewportPoint) const
{
    const qreal zoom = std::max(m_zoomFactor, 1e-6);
    return m_viewCenter + (viewportPoint - viewportCenter()) / zoom;
}

QPointF ImageViewer::mapFromImage(const QPointF& imagePoint) const
{
    return viewportCenter() + (imagePoint - m_viewCenter) * m_zoomFactor;
}

QSize ImageViewer::sizeHint() const
{
    return QSize(480, 320);
}

QSize ImageViewer::minimumSizeHint() const
{
    return QSize(120, 80);
}

// ── View geometry. zh_CN: 视图几何 ─────────────────────────────────────────

bool ImageViewer::hasImage() const
{
    return !m_pixmap.isNull() && m_imageSize.width() > 0.0 && m_imageSize.height() > 0.0;
}

QPointF ImageViewer::viewportCenter() const
{
    return QPointF(width() / 2.0, height() / 2.0);
}

QPointF ImageViewer::imageCenter() const
{
    return hasImage() ? QPointF(m_imageSize.width() / 2.0, m_imageSize.height() / 2.0) : QPointF();
}

qreal ImageViewer::effectiveMinZoom() const
{
    // The fit factor stays reachable even when it is below minZoomFactor.
    // zh_CN: 即使适应比例低于 minZoomFactor，也保证可以到达。
    return hasImage() ? std::min(m_minZoomFactor, fitZoomFactor()) : m_minZoomFactor;
}

qreal ImageViewer::effectiveMaxZoom() const
{
    return hasImage() ? std::max(m_maxZoomFactor, fitZoomFactor()) : m_maxZoomFactor;
}

qreal ImageViewer::clampedZoom(qreal factor) const
{
    if (!std::isfinite(factor))
        return m_zoomFactor;
    return std::clamp(factor, effectiveMinZoom(), effectiveMaxZoom());
}

QPointF ImageViewer::clampedCenter(qreal zoom, const QPointF& center) const
{
    if (!hasImage())
        return QPointF();
    // An axis that fits stays centered; otherwise the image edges never enter the viewport.
    // zh_CN: 能完整放下的轴保持居中；否则图片边缘不会进入视口内侧。
    const auto clampAxis = [zoom](qreal extent, qreal view, qreal value) {
        if (view <= 0.0 || extent * zoom <= view)
            return extent / 2.0;
        const qreal half = view / (2.0 * zoom);
        return std::clamp(value, half, extent - half);
    };
    return QPointF(clampAxis(m_imageSize.width(), width(), center.x()),
        clampAxis(m_imageSize.height(), height(), center.y()));
}

qreal ImageViewer::fitTargetZoom(FitMode mode) const
{
    switch (mode) {
    case FitMode::Contain:
        return fitZoomFactor();
    case FitMode::ScaleDown:
        return clampedZoom(std::min(1.0, fitZoomFactor()));
    case FitMode::Manual:
        break;
    }
    return m_zoomFactor;
}

bool ImageViewer::canPanHorizontally() const
{
    return hasImage() && m_imageSize.width() * m_zoomFactor > width() + kPanTolerance;
}

bool ImageViewer::canPanVertically() const
{
    return hasImage() && m_imageSize.height() * m_zoomFactor > height() + kPanTolerance;
}

qreal ImageViewer::presetStep(bool zoomIn) const
{
    const qreal low = effectiveMinZoom();
    const qreal high = effectiveMaxZoom();
    if (zoomIn) {
        for (qreal preset : kZoomPresets) {
            if (preset > m_zoomFactor * 1.001)
                return std::min(preset, high);
        }
        return high;
    }
    for (auto it = kZoomPresets.rbegin(); it != kZoomPresets.rend(); ++it) {
        if (*it < m_zoomFactor * 0.999)
            return std::max(*it, low);
    }
    return low;
}

// ── View changes. zh_CN: 视图变更 ──────────────────────────────────────────

void ImageViewer::applyView(qreal zoom, const QPointF& center, FitMode mode, ChangeSource source)
{
    const qreal targetZoom = clampedZoom(zoom);
    const QPointF targetCenter = clampedCenter(targetZoom, center);
    const bool zoomChanged = !sameZoom(targetZoom, m_zoomFactor);
    const bool centerChanged = !samePoint(targetCenter, m_viewCenter);
    const bool modeChanged = mode != m_fitMode;
    if (!zoomChanged && !centerChanged && !modeChanged)
        return;

    const bool couldZoomIn = m_zoomFactor < effectiveMaxZoom();
    const bool couldZoomOut = m_zoomFactor > effectiveMinZoom();
    // Assign the whole view before notifying so slots read a consistent state.
    // zh_CN: 先写入完整视图再通知，确保槽函数读取到一致的状态。
    m_zoomFactor = targetZoom;
    m_viewCenter = targetCenter;
    m_fitMode = mode;

    updateCursorShape();
    updateChrome();
    update();
    if (zoomChanged)
        emit zoomFactorChanged(m_zoomFactor);
    if (centerChanged)
        emit viewCenterChanged(m_viewCenter);
    if (modeChanged)
        emit fitModeChanged(m_fitMode);
    if (zoomChanged) {
        const bool actionsChanged = couldZoomIn != (m_zoomFactor < effectiveMaxZoom())
            || couldZoomOut != (m_zoomFactor > effectiveMinZoom());
        detail::notifyImageViewerAccessibilityZoomChanged(this, actionsChanged);
        if (source == ChangeSource::Input)
            showZoomIndicator();
    }
}

void ImageViewer::zoomAround(qreal factor, const QPointF& viewportAnchor, bool animated, ChangeSource source)
{
    stopViewAnimation();
    if (!hasImage())
        return;
    const qreal target = clampedZoom(factor);
    if (sameZoom(target, m_zoomFactor))
        return;

    // Keep the image point under the anchor fixed. zh_CN: 保持锚点下的图片点不动。
    const QPointF imageAnchor = mapToImage(viewportAnchor);
    const QPointF center = imageAnchor - (viewportAnchor - viewportCenter()) / target;
    if (animated) {
        animateTo({target, center}, FitMode::Manual, viewportAnchor, true, source);
        return;
    }
    applyView(target, center, FitMode::Manual, source);
}

void ImageViewer::fitWithMode(FitMode mode, bool animated, ChangeSource source)
{
    stopViewAnimation();
    if (!hasImage()) {
        if (m_fitMode != mode) {
            m_fitMode = mode;
            updateChrome();
            emit fitModeChanged(m_fitMode);
        }
        return;
    }
    const qreal zoom = fitTargetZoom(mode);
    const QPointF center = clampedCenter(zoom, imageCenter());
    if (animated && (!sameZoom(zoom, m_zoomFactor) || !samePoint(center, m_viewCenter))) {
        animateTo({zoom, center}, mode, QPointF(), false, source);
        return;
    }
    applyView(zoom, center, mode, source);
}

void ImageViewer::stepZoom(bool zoomIn, bool animated, ChangeSource source)
{
    finishViewAnimation();
    if (!hasImage())
        return;
    zoomAround(presetStep(zoomIn), viewportCenter(), animated, source);
}

void ImageViewer::toggleZoomAt(const QPointF& viewportPos)
{
    if (!hasImage())
        return;
    if (m_fitMode == FitMode::Manual) {
        fitWithMode(FitMode::ScaleDown, true, ChangeSource::Input);
        return;
    }
    zoomAround(m_zoomFactor < 1.0 - 1e-6 ? 1.0 : m_zoomFactor * 2.0, viewportPos, true, ChangeSource::Input);
}

void ImageViewer::panByInternal(const QPointF& delta, ChangeSource source)
{
    if (!hasImage() || m_zoomFactor <= 0.0)
        return;
    const QPointF target = clampedCenter(m_zoomFactor, m_viewCenter + delta / m_zoomFactor);
    if (samePoint(target, m_viewCenter))
        return;
    applyView(m_zoomFactor, target, FitMode::Manual, source);
}

void ImageViewer::
    animateTo(const ViewState& target, FitMode mode, const QPointF& anchor, bool anchored, ChangeSource source)
{
    m_viewAnimation->stop();
    m_animationFrom = {m_zoomFactor, m_viewCenter};
    m_animationTo = {target.zoom, clampedCenter(target.zoom, target.center)};
    m_animationMode = mode;
    m_animationAnchor = anchor;
    m_animationImageAnchor = anchored ? mapToImage(anchor) : QPointF();
    m_animationAnchored = anchored;
    m_animationSource = source;
    m_viewAnimation->setEasingCurve(themeAnimation().decelerate);
    ::fluent::detail::startMotionTransition(m_viewAnimation, themeAnimation().normal);
}

void ImageViewer::updateAnimationFrame(qreal progress)
{
    if (progress >= 1.0) {
        applyView(m_animationTo.zoom, m_animationTo.center, m_animationMode, m_animationSource);
        return;
    }
    const qreal from = std::max(m_animationFrom.zoom, 1e-6);
    const qreal to = std::max(m_animationTo.zoom, 1e-6);
    // Interpolate zoom geometrically so equal steps feel equally fast.
    // zh_CN: 缩放按几何插值，使相同倍率的变化速度一致。
    const qreal zoom = from * std::pow(to / from, progress);
    const QPointF center = m_animationAnchored
        ? m_animationImageAnchor - (m_animationAnchor - viewportCenter()) / zoom
        : m_animationFrom.center + (m_animationTo.center - m_animationFrom.center) * progress;
    applyView(zoom, center, m_fitMode, m_animationSource);
}

void ImageViewer::stopViewAnimation()
{
    if (m_viewAnimation)
        m_viewAnimation->stop();
}

void ImageViewer::finishViewAnimation()
{
    if (!m_viewAnimation || m_viewAnimation->state() != QAbstractAnimation::Running)
        return;
    m_viewAnimation->stop();
    applyView(m_animationTo.zoom, m_animationTo.center, m_animationMode, m_animationSource);
}

void ImageViewer::reapplyConstraints()
{
    if (hasImage()) {
        if (m_fitMode != FitMode::Manual)
            applyView(fitTargetZoom(m_fitMode), imageCenter(), m_fitMode, ChangeSource::Program);
        else
            applyView(m_zoomFactor, m_viewCenter, FitMode::Manual, ChangeSource::Program);
    }
    // Fit factor and pan ranges may change without changing the view.
    // zh_CN: 适应比例和平移范围可能变化，即使视图本身没有变化。
    updateCursorShape();
    updateChrome();
}

// ── Chrome. zh_CN: 工具条与提示 ────────────────────────────────────────────

bool ImageViewer::areControlsShown() const
{
    if (!hasImage() || m_controlsVisibility == ControlsVisibility::Hidden)
        return false;
    return m_controlsVisibility == ControlsVisibility::Visible || m_hovered || hasFocus() || m_dragging;
}

void ImageViewer::updateChrome()
{
    if (!m_zoomBar)
        return;
    const bool active = hasImage() && isEnabled();
    const qreal tolerance = 1e-6 * std::max(1.0, m_zoomFactor);
    ImageViewerZoomBar::State state;
    state.percent = hasImage() ? percentText() : QStringLiteral("—");
    state.canZoomOut = active && m_zoomFactor > effectiveMinZoom() + tolerance;
    state.canZoomIn = active && m_zoomFactor < effectiveMaxZoom() - tolerance;
    state.canFit = active
        && !(m_fitMode != FitMode::Manual && std::abs(m_zoomFactor - fitZoomFactor()) <= tolerance
            && samePoint(m_viewCenter, imageCenter()));
    state.canActualSize = active && std::abs(m_zoomFactor - clampedZoom(1.0)) > tolerance;
    state.canReset = active && m_fitMode != FitMode::ScaleDown;
    m_zoomBar->setState(state);
    m_zoomBar->setShown(areControlsShown());
    layoutChrome();
}

void ImageViewer::layoutChrome()
{
    if (!m_zoomBar)
        return;
    // Narrow viewports keep only the zoom group. zh_CN: 视口较窄时只保留缩放组。
    m_zoomBar->setCompact(width() < m_zoomBar->surfaceWidth(false) + 2 * kChromeMargin);
    const QSize hint = m_zoomBar->sizeHint();
    const int margin = ImageViewerZoomBar::shadowMargin();
    m_zoomBar->setGeometry((width() - hint.width()) / 2, height() - kChromeMargin + margin - hint.height(),
        hint.width(), hint.height());
    if (m_zoomIndicator && m_zoomIndicator->isVisible())
        m_zoomIndicator->move((width() - m_zoomIndicator->width()) / 2, (height() - m_zoomIndicator->height()) / 2);
}

void ImageViewer::showZoomIndicator()
{
    if (!m_zoomIndicator || !m_zoomIndicatorEnabled || !hasImage() || areControlsShown())
        return;
    m_zoomIndicator->flash(percentText());
}

QString ImageViewer::percentText() const
{
    return viewerText("%1%").arg(qRound(m_zoomFactor * 100.0));
}

void ImageViewer::updateCursorShape()
{
    if (m_dragging) {
        setCursor(Qt::ClosedHandCursor);
    } else if (hasImage() && isEnabled() && (canPanHorizontally() || canPanVertically())) {
        setCursor(Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
}

// ── Painting. zh_CN: 绘制 ──────────────────────────────────────────────────

void ImageViewer::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    const auto& colors = themeColorsRef();
    const qreal radius = m_frameVisible ? themeRadius().control : 0.0;

    if (m_frameVisible) {
        QPainterPath frame;
        frame.addRoundedRect(QRectF(rect()), radius, radius);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillPath(frame, colors.bgSolid);
        painter.setClipPath(frame);
    }

    if (hasImage()) {
        const QRectF imageRect(mapFromImage(QPointF(0.0, 0.0)), m_imageSize * m_zoomFactor);
        const QRectF visible = imageRect.intersected(QRectF(event->rect()));
        if (!visible.isEmpty()) {
            const qreal deviceScale = m_zoomFactor * devicePixelRatioF() / m_pixmap.devicePixelRatio();
            const QPixmap& source = m_transformationMode == Qt::SmoothTransformation
                ? m_pixmapCache->sourceForScale(m_pixmap, deviceScale)
                : m_pixmap;
            // Rounding the cached width and height can produce different scale factors.
            // zh_CN: 缓存宽高分别取整后，两个方向的实际缩放比可能不同。
            const qreal sourceX = source.width() / imageRect.width();
            const qreal sourceY = source.height() / imageRect.height();
            const QRectF sourceRect((visible.left() - imageRect.left()) * sourceX,
                (visible.top() - imageRect.top()) * sourceY, visible.width() * sourceX, visible.height() * sourceY);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, m_transformationMode == Qt::SmoothTransformation);
            painter.drawPixmap(visible, source, sourceRect);
        }
    }

    painter.setClipping(false);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    if (m_frameVisible) {
        painter.setPen(QPen(colors.strokeDefault, 1.0));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    }
    if (hasFocus() && m_keyboardFocusVisible) {
        // Fluent two-layer focus visual inside the bounds. zh_CN: 在边界内绘制 Fluent 双层焦点框。
        painter.setPen(QPen(colors.strokeFocusOuter, 2.0));
        painter.drawRoundedRect(QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0), radius, radius);
        painter.setPen(QPen(colors.strokeFocusInner, 1.0));
        const qreal innerRadius = std::max(0.0, radius - 2.0);
        painter.drawRoundedRect(QRectF(rect()).adjusted(2.5, 2.5, -2.5, -2.5), innerRadius, innerRadius);
    }
}

// ── Events. zh_CN: 事件 ────────────────────────────────────────────────────

bool ImageViewer::event(QEvent* event)
{
    if (event->type() == QEvent::NativeGesture && handleNativeGesture(event))
        return true;
    if (event->type() == QEvent::Gesture && handlePinchGesture(event))
        return true;
    return QWidget::event(event);
}

void ImageViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    finishViewAnimation();
    reapplyConstraints();
    layoutChrome();
}

void ImageViewer::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled()) {
            m_dragging = false;
            stopViewAnimation();
        }
        updateCursorShape();
        updateChrome();
        detail::notifyImageViewerAccessibilityActionsChanged(this);
    }
}

void ImageViewer::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
    m_hovered = true;
    updateChrome();
}

void ImageViewer::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    updateChrome();
}

void ImageViewer::mousePressEvent(QMouseEvent* event)
{
    // Without pan range the press belongs to the parent, e.g. a FlipView.
    // zh_CN: 没有平移范围时按下事件交给父控件，例如 FlipView。
    if (event->button() != Qt::LeftButton || !hasImage() || !isEnabled()
        || (!canPanHorizontally() && !canPanVertically())) {
        QWidget::mousePressEvent(event);
        return;
    }
    stopViewAnimation();
    m_dragging = true;
    m_lastDragPos = ::compatibility::detail::mouseLocalPosition(event);
    m_keyboardFocusVisible = false;
    updateCursorShape();
    updateChrome();
    event->accept();
}

void ImageViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPointF position = ::compatibility::detail::mouseLocalPosition(event);
    const QPointF delta = position - m_lastDragPos;
    m_lastDragPos = position;
    panByInternal(-delta, ChangeSource::Input);
    event->accept();
}

void ImageViewer::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_dragging || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_dragging = false;
    updateCursorShape();
    updateChrome();
    event->accept();
}

void ImageViewer::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !hasImage() || !isEnabled()) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    m_dragging = false;
    toggleZoomAt(::compatibility::detail::mouseLocalPosition(event));
    updateCursorShape();
    event->accept();
}

void ImageViewer::wheelEvent(QWheelEvent* event)
{
    if (!hasImage() || !isEnabled()) {
        QWidget::wheelEvent(event);
        return;
    }
    if (shouldSuppressWheel()) {
        event->accept();
        return;
    }

    const FluentWheelInputKind kind = fluentWheelInputKind(event);
    const bool zoomModifier = event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier);
    if (zoomModifier || (kind == FluentWheelInputKind::NoPhaseDiscrete && m_wheelAction == WheelAction::Zoom)) {
        const QPoint pixel = event->pixelDelta();
        const QPoint angle = event->angleDelta();
        qreal steps = 0.0;
        if (!pixel.isNull())
            steps = (pixel.y() != 0 ? pixel.y() : pixel.x()) / 120.0;
        else
            steps = (angle.y() != 0 ? angle.y() : angle.x()) / 120.0;
        if (!qFuzzyIsNull(steps))
            zoomAround(m_zoomFactor * std::pow(kWheelZoomStep, steps), fluentWheelPosition(event), false,
                ChangeSource::Input);
        // Zoom input is consumed even at a limit. zh_CN: 缩放输入在达到上下限时仍被消费。
        event->accept();
        return;
    }

    QPointF delta;
    if (kind == FluentWheelInputKind::NoPhaseDiscrete) {
        QPointF steps(event->angleDelta().x() / 120.0, event->angleDelta().y() / 120.0);
        if ((event->modifiers() & Qt::ShiftModifier) && qFuzzyIsNull(steps.x()))
            steps = QPointF(steps.y(), 0.0);
        delta = -steps * kPanStep;
    } else {
        const QPoint pixel = event->pixelDelta();
        delta = pixel.isNull() ? -QPointF(event->angleDelta()) / 120.0 * kPanStep : -QPointF(pixel);
    }

    // Decide by the dominant axis; an axis without range belongs to the parent.
    // zh_CN: 按主方向判断；没有平移范围的轴交给父控件。
    const bool horizontal = std::abs(delta.x()) > std::abs(delta.y());
    const bool canPan = delta.isNull()
        ? (canPanHorizontally() || canPanVertically())
        : (horizontal ? canPanHorizontally() : canPanVertically());
    if (!canPan) {
        event->ignore();
        return;
    }
    stopViewAnimation();
    panByInternal(delta, ChangeSource::Input);
    event->accept();
}

void ImageViewer::keyPressEvent(QKeyEvent* event)
{
    if (!hasImage() || !isEnabled()) {
        QWidget::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        m_keyboardFocusVisible = true;
        stepZoom(true, true, ChangeSource::Input);
        event->accept();
        return;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        m_keyboardFocusVisible = true;
        stepZoom(false, true, ChangeSource::Input);
        event->accept();
        return;
    case Qt::Key_0:
        m_keyboardFocusVisible = true;
        fitWithMode(FitMode::ScaleDown, true, ChangeSource::Input);
        event->accept();
        return;
    case Qt::Key_1:
        m_keyboardFocusVisible = true;
        zoomAround(1.0, viewportCenter(), true, ChangeSource::Input);
        event->accept();
        return;
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down: {
        const bool horizontal = event->key() == Qt::Key_Left || event->key() == Qt::Key_Right;
        if (horizontal ? !canPanHorizontally() : !canPanVertically()) {
            // No range: let a parent such as FlipView navigate. zh_CN: 没有范围时交给父控件（如 FlipView）导航。
            QWidget::keyPressEvent(event);
            return;
        }
        finishViewAnimation();
        m_keyboardFocusVisible = true;
        const qreal step = (event->key() == Qt::Key_Left || event->key() == Qt::Key_Up) ? -kPanStep : kPanStep;
        panByInternal(horizontal ? QPointF(step, 0.0) : QPointF(0.0, step), ChangeSource::Input);
        update();
        event->accept();
        return;
    }
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void ImageViewer::focusInEvent(QFocusEvent* event)
{
    switch (event->reason()) {
    case Qt::TabFocusReason:
    case Qt::BacktabFocusReason:
    case Qt::ShortcutFocusReason:
        m_keyboardFocusVisible = true;
        break;
    case Qt::MouseFocusReason:
        m_keyboardFocusVisible = false;
        break;
    default:
        break;
    }
    QWidget::focusInEvent(event);
    updateChrome();
    update();
}

void ImageViewer::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    updateChrome();
    update();
}

void ImageViewer::onThemeUpdated()
{
    update();
}

bool ImageViewer::shouldSuppressWheel() const
{
    // macOS can deliver residual scroll events right after a pinch.
    // zh_CN: 捏合结束后可能紧接着收到残余滚动事件。
    if (m_nativeZoomGestureActive && m_nativeZoomGestureHasZoomed)
        return true;
    return m_lastNativeZoomGestureTime.isValid()
        && m_lastNativeZoomGestureTime.elapsed() < kNativeZoomPinchSuppressionMs;
}

bool ImageViewer::handleNativeGesture(QEvent* event)
{
    if (!hasImage() || !isEnabled())
        return false;

    auto* gesture = static_cast<FluentNativeGestureEvent*>(event);
    switch (gesture->gestureType()) {
    case Qt::BeginNativeGesture:
        stopViewAnimation();
        m_nativeZoomGestureActive = true;
        m_nativeZoomGestureHasZoomed = false;
        gesture->accept();
        return true;
    case Qt::ZoomNativeGesture: {
        m_lastNativeZoomGestureTime.restart();
        m_nativeZoomGestureActive = true;
        m_nativeZoomGestureHasZoomed = true;
        const qreal scale = std::clamp(1.0 + gesture->value(), 0.25, 4.0);
        zoomAround(m_zoomFactor * scale, fluentNativeGesturePosition(gesture), false, ChangeSource::Input);
        gesture->accept();
        return true;
    }
    case Qt::SmartZoomNativeGesture:
        if (!shouldSuppressWheel())
            toggleZoomAt(fluentNativeGesturePosition(gesture));
        gesture->accept();
        return true;
    case Qt::EndNativeGesture:
        if (m_nativeZoomGestureHasZoomed)
            m_lastNativeZoomGestureTime.restart();
        m_nativeZoomGestureActive = false;
        m_nativeZoomGestureHasZoomed = false;
        gesture->accept();
        return true;
    default:
        return false;
    }
}

bool ImageViewer::handlePinchGesture(QEvent* event)
{
    if (!hasImage() || !isEnabled())
        return false;

    auto* gestureEvent = static_cast<QGestureEvent*>(event);
    auto* pinch = static_cast<QPinchGesture*>(gestureEvent->gesture(Qt::PinchGesture));
    if (!pinch)
        return false;
    if (::compatibility::detail::usesNativePinchGestures() || shouldSuppressWheel()) {
        gestureEvent->accept(pinch);
        return true;
    }
    if (pinch->state() == Qt::GestureStarted)
        stopViewAnimation();
    if (pinch->state() != Qt::GestureFinished && pinch->state() != Qt::GestureCanceled
        && (pinch->changeFlags() & QPinchGesture::ScaleFactorChanged)) {
        qreal scale = pinch->scaleFactor();
        if (pinch->lastScaleFactor() > 0.0)
            scale /= pinch->lastScaleFactor();
        const QPointF center = mapFromGlobal(pinch->centerPoint().toPoint());
        zoomAround(m_zoomFactor * scale, center, false, ChangeSource::Input);
    }
    gestureEvent->accept(pinch);
    return true;
}

} // namespace fluent::scrolling

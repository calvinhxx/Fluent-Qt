#include "compatibility/InputEventCompat.h"
#include "components/foundation/overlay/OverlayScrim.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimer>
#include <QWheelEvent>
#include <cmath>

namespace fluent::overlay {
namespace {

// Three separable box passes approximate a Gaussian without private Qt APIs.
// Work is bounded by a downsampled, opaque content snapshot, never the desktop.
// zh_CN: 三次分离盒式模糊近似高斯，无 Qt 私有 API；仅处理限尺寸的内容快照，不读取桌面。
QImage blurPass(const QImage& input, int radius, bool horizontal)
{
    QImage output(input.size(), QImage::Format_RGB32);
    const int length = horizontal ? input.width() : input.height();
    const int lines = horizontal ? input.height() : input.width();
    const int divisor = radius * 2 + 1;
    const int inputStride = input.bytesPerLine() / sizeof(QRgb);
    const int outputStride = output.bytesPerLine() / sizeof(QRgb);
    const auto* source = reinterpret_cast<const QRgb*>(input.constBits());
    auto* destination = reinterpret_cast<QRgb*>(output.bits());
    const int inputStep = horizontal ? 1 : inputStride;
    const int outputStep = horizontal ? 1 : outputStride;
    for (int line = 0; line < lines; ++line) {
        const auto* row = source + (horizontal ? line * inputStride : line);
        auto* target = destination + (horizontal ? line * outputStride : line);
        int red = 0, green = 0, blue = 0;
        for (int offset = -radius; offset <= radius; ++offset) {
            const QRgb value = row[qBound(0, offset, length - 1) * inputStep];
            red += qRed(value);
            green += qGreen(value);
            blue += qBlue(value);
        }
        for (int position = 0; position < length; ++position) {
            target[position * outputStep] = qRgb(red / divisor, green / divisor, blue / divisor);
            const QRgb leaving = row[qMax(0, position - radius) * inputStep];
            const QRgb entering = row[qMin(length - 1, position + radius + 1) * inputStep];
            red += qRed(entering) - qRed(leaving);
            green += qGreen(entering) - qGreen(leaving);
            blue += qBlue(entering) - qBlue(leaving);
        }
    }
    return output;
}

} // namespace

OverlayScrim::OverlayScrim(QWidget* parent, const QString& objectName) : QWidget(parent)
{
    if (!objectName.isEmpty())
        setObjectName(objectName);
    // Shared-backing SourceOver dim — same contract as former Dialog SmokeOverlay. Do not use
    // WA_TranslucentBackground or CompositionMode_Source clears on Mica hosts.
    // zh_CN: 共享后备缓冲上的 SourceOver 压暗——与原先 Dialog SmokeOverlay 契约一致。Mica 宿主上不要用
    // WA_TranslucentBackground 或 CompositionMode_Source 清屏。
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
    setModalAndDim(true, true);
}

void OverlayScrim::setModalAndDim(bool modal, bool dim)
{
    m_modal = modal;
    m_dim = dim;
    setAttribute(Qt::WA_TransparentForMouseEvents, !m_modal);
    update();
}

void OverlayScrim::setOpacityProgress(qreal progress)
{
    const qreal normalized = qBound<qreal>(0.0, progress, 1.0);
    if (std::abs(m_opacityProgress - normalized) <= 0.0001)
        return;
    m_opacityProgress = normalized;
    update();
}

void OverlayScrim::setColor(const QColor& color)
{
    if (m_color == color)
        return;
    m_color = color;
    update();
}

void OverlayScrim::setDimOpacity(qreal opacity)
{
    if (m_dimOpacity == opacity)
        return;
    m_dimOpacity = opacity;
    update();
}

void OverlayScrim::setBackdrop(QWidget* overlay, qreal radius)
{
    if (m_overlay == overlay && m_blurRadius == radius)
        return;
    m_overlay = overlay;
    m_blurRadius = radius;
    refreshBackdrop();
}

void OverlayScrim::queueBackdropRefresh()
{
    if (m_refreshPending || m_blurRadius <= 0.0)
        return;
    m_refreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        if (!m_refreshPending)
            return;
        m_refreshPending = false;
        refreshBackdrop();
    });
}

void OverlayScrim::refreshBackdrop()
{
    if (m_capturing)
        return;
    m_refreshPending = false;
    QWidget* host = parentWidget();
    if (!host || !m_overlay || size().isEmpty() || m_blurRadius <= 0.0 || effectiveTheme() == HighContrast) {
        m_backdrop = QImage();
        m_backdropSize = QSize();
        update();
        return;
    }
    m_capturing = true;
    const QSize capturedSize = size();
    const qreal scale = qMin<qreal>(0.5, 1024.0 / qMax(width(), height()));
    QImage snapshot(QSize(qMax(1, qRound(width() * scale)), qMax(1, qRound(height() * scale))),
        QImage::Format_ARGB32_Premultiplied);
    snapshot.fill(Qt::transparent);
    {
        QPainter painter(&snapshot);
        painter.scale(scale, scale);
        painter.translate(-geometry().topLeft());
        host->render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground);
        // Render only siblings below this overlay. Hiding widgets to grab the host
        // would disturb focus, layout and nested popups. QObject child order follows stacking.
        // zh_CN: 只绘制当前浮层下方的同级控件；不通过隐藏控件抓图，避免改变焦点、布局和嵌套弹层。
        const auto siblings = host->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget* child : siblings) {
            if (child == m_overlay)
                break;
            if (child != this && !child->isHidden() && !child->isWindow())
                child->render(&painter, child->pos(), QRegion(), QWidget::DrawWindowBackground | QWidget::DrawChildren);
        }
        // Native vibrancy is not part of QWidget rendering. Fill only its transparent
        // areas with the Fluent surface token before filtering the application content.
        // zh_CN: 原生材质不属于 QWidget 绘制；用 Fluent 表面 token 填补透明区域后再模糊应用内容。
        painter.resetTransform();
        painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        const auto* hostTheme = dynamic_cast<FluentElement*>(host);
        painter.fillRect(snapshot.rect(),
            hostTheme ? hostTheme->themeColorsRef().bgCanvas : host->palette().window().color());
    }
    snapshot = snapshot.convertToFormat(QImage::Format_RGB32);
    const int radius = qMax(1, qRound(m_blurRadius * scale * 0.6));
    for (int pass = 0; pass < 3; ++pass) {
        snapshot = blurPass(snapshot, radius, true);
        snapshot = blurPass(snapshot, radius, false);
    }
    m_backdrop = snapshot;
    m_backdropSize = capturedSize;
    m_capturing = false;
    if (size() != capturedSize)
        queueBackdropRefresh();
    update();
}

void OverlayScrim::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // QWidget::render also delivers pending hidden resizes during capture. The
    // capture already handles that size; queuing it again stalls the entrance.
    // zh_CN: QWidget::render 会在抓图期间送达隐藏控件的 resize；无需重复抓取同一尺寸。
    if (!m_capturing && size() != m_backdropSize)
        queueBackdropRefresh();
}

void OverlayScrim::setSurfaceRadius(int radius)
{
    radius = qMax(0, radius);
    if (m_surfaceRadius == radius)
        return;
    m_surfaceRadius = radius;
    update();
}

void OverlayScrim::setSpotlightEnabled(bool enabled)
{
    if (m_spotEnabled == enabled)
        return;
    m_spotEnabled = enabled;
    update();
}

void OverlayScrim::setSpotlightRect(const QRect& rect)
{
    if (m_spot == rect)
        return;
    m_spot = rect;
    if (m_spotEnabled)
        update();
}

void OverlayScrim::setSpotlightRadius(int radius)
{
    if (m_spotRadius == radius)
        return;
    m_spotRadius = radius;
    if (m_spotEnabled)
        update();
}

void OverlayScrim::onThemeUpdated()
{
    queueBackdropRefresh();
    update();
}

void OverlayScrim::paintEvent(QPaintEvent*)
{
    if (!m_dim && m_backdrop.isNull())
        return;

    QPainter painter(this);
    const auto smoke = themeSmoke();
    QColor color = m_color.isValid() ? m_color : smoke.baseColor;
    const qreal baseAlpha = m_dimOpacity >= 0.0 ? m_dimOpacity : m_color.isValid() ? m_color.alphaF() : smoke.opacity;
    color.setAlphaF(m_dim ? baseAlpha * m_opacityProgress : 0.0);

    QPainterPath surface;
    if (m_surfaceRadius > 0) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        surface.addRoundedRect(QRectF(rect()), m_surfaceRadius, m_surfaceRadius);
    } else {
        surface.addRect(QRectF(rect()));
    }

    if (m_spotEnabled && !m_spot.isEmpty()) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath hole;
        hole.addRoundedRect(QRectF(m_spot), m_spotRadius, m_spotRadius);
        surface = surface.subtracted(hole);
    }
    if (!m_backdrop.isNull() && effectiveTheme() != HighContrast) {
        painter.save();
        painter.setClipPath(surface);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setOpacity(m_opacityProgress);
        painter.drawImage(rect(), m_backdrop);
        painter.restore();
    }
    painter.fillPath(surface, color);
}

void OverlayScrim::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QPointer<OverlayScrim> guard(this);
        emit pressed(fluentMouseGlobalPos(event));
        if (!guard)
            return;
    }
    handlePointerEvent(event);
}
void OverlayScrim::mouseReleaseEvent(QMouseEvent* event)
{
    handlePointerEvent(event);
}
void OverlayScrim::mouseMoveEvent(QMouseEvent* event)
{
    handlePointerEvent(event);
}
void OverlayScrim::wheelEvent(QWheelEvent* event)
{
    handlePointerEvent(event);
}

void OverlayScrim::handlePointerEvent(QEvent* event)
{
    if (m_modal)
        event->accept();
    else
        event->ignore();
}

} // namespace fluent::overlay

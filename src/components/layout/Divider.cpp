#include "components/layout/Divider.h"

#include <QPaintEvent>
#include <QPainter>
#include <QVariant>
#include <QtMath>

#include "components/foundation/private/DpiPaintMetrics_p.h"
#include "components/windowing/WindowBackdrop.h"

namespace fluent::layout {

namespace {

bool hasPublishedSurfaceAncestor(const QWidget* widget)
{
    for (const QWidget* ancestor = widget ? widget->parentWidget() : nullptr;
         ancestor && !ancestor->isWindow();
         ancestor = ancestor->parentWidget()) {
        const QVariant surface = ancestor->property("fluentSurfaceColor");
        if (surface.isValid() && surface.canConvert<QColor>()
            && surface.value<QColor>().isValid()) {
            return true;
        }
    }
    return false;
}

bool requiresBackdropReplacement(const QWidget* widget)
{
    const QWidget* topLevel = widget ? widget->window() : nullptr;
    return topLevel
        && topLevel->testAttribute(Qt::WA_TranslucentBackground)
        && windowing::windowBackdropRequiresTransparentClear(topLevel)
        && !hasPublishedSurfaceAncestor(widget);
}

} // namespace

Divider::Divider(QWidget* parent)
    : Divider(Qt::Horizontal, parent)
{
}

Divider::Divider(Qt::Orientation orientation, QWidget* parent)
    : QWidget(parent),
      m_orientation(orientation)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy::Expanding
                                                : QSizePolicy::Fixed,
                  orientation == Qt::Horizontal ? QSizePolicy::Fixed
                                                : QSizePolicy::Expanding);
}

void Divider::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation)
        return;

    m_orientation = orientation;
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy::Expanding
                                                : QSizePolicy::Fixed,
                  orientation == Qt::Horizontal ? QSizePolicy::Fixed
                                                : QSizePolicy::Expanding);
    updateGeometry();
    update();
    emit orientationChanged(m_orientation);
}

void Divider::setLeadingInset(int inset)
{
    inset = qMax(0, inset);
    if (m_leadingInset == inset)
        return;

    m_leadingInset = inset;
    update();
    emit leadingInsetChanged(m_leadingInset);
}

void Divider::setTrailingInset(int inset)
{
    inset = qMax(0, inset);
    if (m_trailingInset == inset)
        return;

    m_trailingInset = inset;
    update();
    emit trailingInsetChanged(m_trailingInset);
}

void Divider::setThickness(qreal thickness)
{
    thickness = qMax<qreal>(0.0, thickness);
    if (qFuzzyCompare(m_thickness + 1.0, thickness + 1.0))
        return;

    m_thickness = thickness;
    updateGeometry();
    update();
    emit thicknessChanged(m_thickness);
}

void Divider::setColor(const QColor& color)
{
    if (m_color == color)
        return;

    m_color = color;
    update();
    emit colorChanged(m_color);
}

QSize Divider::sizeHint() const
{
    const int extent = qMax(1, qCeil(m_thickness));
    return m_orientation == Qt::Horizontal ? QSize(16, extent)
                                           : QSize(extent, 16);
}

QSize Divider::minimumSizeHint() const
{
    const int extent = qMax(1, qCeil(m_thickness));
    return m_orientation == Qt::Horizontal ? QSize(0, extent)
                                           : QSize(extent, 0);
}

void Divider::onThemeUpdated()
{
    update();
}

void Divider::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    const QColor lineColor = resolvedColor();
    if (m_thickness <= 0.0 || !lineColor.isValid() || lineColor.alpha() <= 0)
        return;

    QPainter painter(this);
    painting::DpiPaintMetrics metrics(painter);
    QRectF lineRect;
    if (m_orientation == Qt::Horizontal) {
        const qreal available = width() - m_leadingInset - m_trailingInset;
        if (available <= 0.0)
            return;
        lineRect = QRectF(m_leadingInset,
                          (height() - m_thickness) / 2.0,
                          available,
                          m_thickness);
    } else {
        const qreal available = height() - m_leadingInset - m_trailingInset;
        if (available <= 0.0)
            return;
        lineRect = QRectF((width() - m_thickness) / 2.0,
                          m_leadingInset,
                          m_thickness,
                          available);
    }

    // A translucent hairline painted directly onto a composited OS backdrop can
    // accumulate alpha in Qt's shared backing store. Replace those pixels instead.
    // Components that paint an opaque/elevated surface publish
    // `fluentSurfaceColor`, so dividers nested in Card or Popup continue to use
    // normal SourceOver composition.
    // zh_CN: 直接位于系统合成背景上的半透明细线可能在 Qt 共享后备缓冲中重复叠加
    // alpha；此时改为替换像素。Card、Popup 等自绘表面通过 fluentSurfaceColor
    // 声明自身背景，嵌套其中的 Divider 仍使用正常的 SourceOver。
    if (requiresBackdropReplacement(this)) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
    }
    painter.fillRect(metrics.alignedOuterRect(lineRect), lineColor);
}

QColor Divider::resolvedColor() const
{
    return m_color.isValid() ? m_color : themeColorsRef().strokeDivider;
}

} // namespace fluent::layout

#include "compatibility/InputEventCompat.h"
#include "TitleBar.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QVariant>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/windowing/WindowBackdrop.h"
#include "compatibility/private/RuntimePlatformCapabilities_p.h"

namespace fluent::windowing {

namespace {

constexpr int TitleBarDefaultLeadingMargin = 8;

void refreshFluentDescendants(QWidget* root)
{
    if (!root)
        return;

    const auto widgets = root->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* widget : widgets) {
        if (auto* fluentWidget = dynamic_cast<FluentElement*>(widget))
            fluentWidget->onThemeUpdated();
        refreshFluentDescendants(widget);
    }
}

} // namespace

TitleBar::TitleBar(QWidget* parent) : QWidget(parent)
{
    m_windowActive = compatibility::detail::effectiveWindowActivation(isActiveWindow());
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_OpaquePaintEvent,
        compatibility::detail::runtimePlatformCapabilities().opaqueClientTitleBarSurface);
    setAutoFillBackground(false);
    setFixedHeight(m_titleBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_anchorLayout = new fluent::AnchorLayout(this);
    onThemeUpdated();
}

void TitleBar::setContentWidget(QWidget* widget)
{
    if (m_contentWidget == widget)
        return;

    if (m_contentWidget) {
        if (m_anchorLayout) {
            for (int i = 0; i < m_anchorLayout->count(); ++i) {
                QLayoutItem* item = m_anchorLayout->itemAt(i);
                if (item && item->widget() == m_contentWidget) {
                    delete m_anchorLayout->takeAt(i);
                    break;
                }
            }
        }
        m_contentWidget->setParent(nullptr);
    }

    m_contentWidget = widget;
    if (widget) {
        widget->setParent(this);
        updateContentWidgetAnchor();
        refreshFluentDescendants(widget);
    }

    emit contentWidgetChanged(widget);
    emit chromeGeometryChanged();
}

void TitleBar::setSystemReservedLeadingWidth(int width)
{
    const int normalized = qMax(0, width);
    if (m_systemReservedLeadingWidth == normalized)
        return;

    m_systemReservedLeadingWidth = normalized;
    updateContentWidgetAnchor();
    emit systemReservedLeadingWidthChanged(normalized);
    emit chromeGeometryChanged();
}

void TitleBar::setSystemReservedTrailingWidth(int width)
{
    const int normalized = qMax(0, width);
    if (m_systemReservedTrailingWidth == normalized)
        return;

    m_systemReservedTrailingWidth = normalized;
    updateContentWidgetAnchor();
    emit systemReservedTrailingWidthChanged(normalized);
    emit chromeGeometryChanged();
}

void TitleBar::setTitleBarHeight(int height)
{
    const int normalized = qMax(1, height);
    if (m_titleBarHeight == normalized)
        return;

    m_titleBarHeight = normalized;
    setFixedHeight(m_titleBarHeight);
    updateGeometry();
    if (m_anchorLayout)
        m_anchorLayout->invalidate();
    update();
    emit titleBarHeightChanged(m_titleBarHeight);
    emit chromeGeometryChanged();
}

QVector<QRect> TitleBar::dragExclusionRects() const
{
    QVector<QRect> rects;
    if (m_systemReservedLeadingWidth > 0)
        rects << QRect(0, 0, qMin(m_systemReservedLeadingWidth, width()), height());
    if (m_systemReservedTrailingWidth > 0) {
        const int reservedWidth = qMin(m_systemReservedTrailingWidth, width());
        rects << QRect(width() - reservedWidth, 0, reservedWidth, height());
    }

    const auto widgets = findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively);
    for (QWidget* widget : widgets) {
        if (!isDragExcludedWidget(widget))
            continue;

        const QRect mapped(widget->mapTo(const_cast<TitleBar*>(this), QPoint(0, 0)), widget->size());
        if (mapped.isValid() && mapped.intersects(rect()))
            rects << mapped.intersected(rect());
    }

    return rects;
}

void TitleBar::refreshChromeExclusions()
{
    emit chromeGeometryChanged();
}

QSize TitleBar::sizeHint() const
{
    return QSize(320, m_titleBarHeight);
}

QSize TitleBar::minimumSizeHint() const
{
    return QSize(0, m_titleBarHeight);
}

void TitleBar::onThemeUpdated()
{
    refreshFluentDescendants(this);
    update();
}

bool TitleBar::event(QEvent* event)
{
    // Sync activation state so caption buttons and title-bar controls dim together without changing layout or hit testing.
    // zh_CN: 同步激活状态，使窗口控制按钮与标题栏控件同步置灰，且不影响布局和命中区域。
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate) {
        const bool active = compatibility::detail::effectiveWindowActivation(event->type() == QEvent::WindowActivate);
        if (m_windowActive != active) {
            m_windowActive = active;
            emit windowActiveChanged(active);
        }
        update();
    }
    return QWidget::event(event);
}

void TitleBar::paintEvent(QPaintEvent*)
{
    // Invalid color means a real OS backdrop is active: clear to transparent with Source mode because
    // macOS does not auto-clear translucent backing stores between frames (otherwise animations ghost).
    // zh_CN: 无效色表示由系统合成背景，须用 Source 模式擦除透明（macOS 半透明缓冲不自动清帧，否则动画留残影）。
    QPainter painter(this);
    const bool opaqueClientSurface = compatibility::detail::runtimePlatformCapabilities().opaqueClientTitleBarSurface;
    // The top-level Window already painted the software material. Leaving this
    // child surface untouched preserves one continuous gradient/noise field.
    if (windowBackdropUsesPaintedMaterial(window()) && !opaqueClientSurface)
        return;

    QColor fill = windowChromeBackdropFill(*this, window(), m_windowActive);
    if (opaqueClientSurface) {
        if (!fill.isValid())
            fill = themeBackdrop(m_windowActive);
        fill.setAlpha(255);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), fill);
        return;
    }
    if (!fill.isValid()) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        return;
    }
    const qreal frameRadius = ::fluent::overlay::overlaySurfaceRadius(window());
    if (frameRadius > 0.0) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        QRectF rounded(rect());
        rounded.setBottom(rounded.bottom() + frameRadius);
        path.addRoundedRect(rounded, frameRadius, frameRadius);
        painter.fillPath(path, fill);
        return;
    }

    painter.fillRect(rect(), fill);
}

void TitleBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    emit chromeGeometryChanged();
}

void TitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        for (const QRect& exclusion : dragExclusionRects()) {
            if (exclusion.contains(event->pos())) {
                QWidget::mousePressEvent(event);
                return;
            }
        }

        m_dragging = true;
        emit dragStarted(fluentMouseGlobalPos(event));
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        for (const QRect& exclusion : dragExclusionRects()) {
            if (exclusion.contains(event->pos())) {
                QWidget::mousePressEvent(event);
                return;
            }
        }

        emit contextMenuRequested(fluentMouseGlobalPos(event));
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        for (const QRect& exclusion : dragExclusionRects()) {
            if (exclusion.contains(event->pos())) {
                QWidget::mouseDoubleClickEvent(event);
                return;
            }
        }

        m_dragging = false;
        emit doubleClicked(fluentMouseGlobalPos(event));
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        emit dragMoved(fluentMouseGlobalPos(event));
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        emit dragFinished();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

bool TitleBar::isDragExcludedWidget(const QWidget* widget) const
{
    if (!widget || widget == this)
        return false;
    // Disabled controls remain client area rather than caption drag zones (matches WinUI and keeps
    // re-enabled controls clickable without re-publishing hit-test regions).
    // zh_CN: 禁用控件仍属于客户区而非拖拽区（对齐 WinUI，避免重新启用后还要刷新命中区域）。
    if (!widget->isVisibleTo(const_cast<TitleBar*>(this)))
        return false;
    if (widget->testAttribute(Qt::WA_TransparentForMouseEvents))
        return false;

    return widget->focusPolicy() != Qt::NoFocus || widget->inherits("QAbstractButton")
        || widget->inherits("QAbstractSpinBox") || widget->inherits("QComboBox") || widget->inherits("QLineEdit")
        || widget->inherits("QPlainTextEdit") || widget->inherits("QSlider") || widget->inherits("QTextEdit");
}

void TitleBar::updateContentWidgetAnchor()
{
    if (!m_anchorLayout || !m_contentWidget)
        return;

    fluent::AnchorLayout::Anchors anchors;
    anchors.fill = true;
    anchors.fillMargins = QMargins(qMax(TitleBarDefaultLeadingMargin, m_systemReservedLeadingWidth), 0,
        m_systemReservedTrailingWidth, 0);
    m_anchorLayout->addAnchoredWidget(m_contentWidget, anchors);
}

} // namespace fluent::windowing

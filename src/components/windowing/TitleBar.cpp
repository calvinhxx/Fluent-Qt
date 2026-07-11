#include "TitleBar.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QVariant>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/windowing/WindowBackdrop.h"

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

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_Hover);
    setAutoFillBackground(false);
    setFixedHeight(m_titleBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_anchorLayout = new fluent::AnchorLayout(this);
    onThemeUpdated();
}

void TitleBar::setContentWidget(QWidget* widget) {
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

void TitleBar::setSystemReservedLeadingWidth(int width) {
    const int normalized = qMax(0, width);
    if (m_systemReservedLeadingWidth == normalized)
        return;

    m_systemReservedLeadingWidth = normalized;
    updateContentWidgetAnchor();
    emit systemReservedLeadingWidthChanged(normalized);
    emit chromeGeometryChanged();
}

void TitleBar::setSystemReservedTrailingWidth(int width) {
    const int normalized = qMax(0, width);
    if (m_systemReservedTrailingWidth == normalized)
        return;

    m_systemReservedTrailingWidth = normalized;
    updateContentWidgetAnchor();
    emit systemReservedTrailingWidthChanged(normalized);
    emit chromeGeometryChanged();
}

void TitleBar::setTitleBarHeight(int height) {
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

QVector<QRect> TitleBar::dragExclusionRects() const {
    QVector<QRect> rects;
    if (m_systemReservedLeadingWidth > 0)
        rects << QRect(0, 0, qMin(m_systemReservedLeadingWidth, width()), height());
    if (m_systemReservedTrailingWidth > 0) {
        const int reserved = qMin(m_systemReservedTrailingWidth, width());
        rects << QRect(width() - reserved, 0, reserved, height());
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

void TitleBar::refreshChromeExclusions() {
    emit chromeGeometryChanged();
}

QSize TitleBar::sizeHint() const {
    return QSize(320, m_titleBarHeight);
}

QSize TitleBar::minimumSizeHint() const {
    return QSize(0, m_titleBarHeight);
}

void TitleBar::onThemeUpdated() {
    refreshFluentDescendants(this);
    update();
}

bool TitleBar::event(QEvent* event) {
    // Repaint when the window's focus changes so the backdrop tracks active/inactive,
    // matching the nav pane. zh_CN: 窗口焦点变化时重绘，使背景跟随激活/非激活，与导航栏一致。
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate)
        update();
    return QWidget::event(event);
}

void TitleBar::paintEvent(QPaintEvent*) {
    // chromeBackdropFill() owns the backdrop decision (shared with the nav pane). An invalid color
    // means a real OS-composited backdrop is in play and we must erase to transparent so it shows
    // through. We must clear (not paint nothing): under a translucent top-level the backing store is
    // not auto-cleared on macOS, so animating/repositioning chrome (back-button reveal, title
    // reflow) would ghost over stale pixels. A valid color is the solid fallback backdrop.
    // zh_CN: chromeBackdropFill() 统一背景决策（与导航栏共用）。无效色表示有真实系统合成背景，须擦成透明透出之。
    // 必须清除（而非不绘制）：半透明顶层下 macOS 后备缓冲不会自动清除，否则动画/重排的 chrome（返回按钮展开、
    // 标题重排）会叠在残影上重影。有效色则是纯色回退背景。
    QPainter painter(this);
    // The top-level Window already painted the software material. Leaving this
    // child surface untouched preserves one continuous gradient/noise field.
    if (windowBackdropUsesPaintedMaterial(window()))
        return;

    const QColor fill = chromeBackdropFill(window(), isActiveWindow());
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

void TitleBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    emit chromeGeometryChanged();
}

void TitleBar::mousePressEvent(QMouseEvent* event) {
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

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
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

void TitleBar::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        emit dragMoved(fluentMouseGlobalPos(event));
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent* event) {
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        emit dragFinished();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

bool TitleBar::isDragExcludedWidget(const QWidget* widget) const {
    if (!widget || widget == this)
        return false;
    // A disabled control is still client area (not a caption drag region) — matching WinUI,
    // and so a control re-enabled after the hit-test was published stays clickable without a
    // resize. Only visibility and mouse-transparency remove a control from the exclusions.
    // zh_CN: 禁用控件仍属于客户区（而非标题拖拽区）——对齐 WinUI，并使命中测试发布后再被启用的控件
    // 无需 resize 即可点击。仅「不可见」或「鼠标穿透」才将控件移出排除区。
    if (!widget->isVisibleTo(const_cast<TitleBar*>(this)))
        return false;
    if (widget->testAttribute(Qt::WA_TransparentForMouseEvents))
        return false;

    return widget->focusPolicy() != Qt::NoFocus
           || widget->inherits("QAbstractButton")
           || widget->inherits("QAbstractSpinBox")
           || widget->inherits("QComboBox")
           || widget->inherits("QLineEdit")
           || widget->inherits("QPlainTextEdit")
           || widget->inherits("QSlider")
           || widget->inherits("QTextEdit");
}

void TitleBar::updateContentWidgetAnchor() {
    if (!m_anchorLayout || !m_contentWidget)
        return;

    fluent::AnchorLayout::Anchors anchors;
    anchors.fill = true;
    anchors.fillMargins = QMargins(qMax(TitleBarDefaultLeadingMargin, m_systemReservedLeadingWidth),
                                   0,
                                   m_systemReservedTrailingWidth,
                                   0);
    m_anchorLayout->addAnchoredWidget(m_contentWidget, anchors);
}

} // namespace fluent::windowing

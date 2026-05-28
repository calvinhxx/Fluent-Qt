#include "TitleBar.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

namespace view::windowing {

namespace {

constexpr int TitleBarDefaultLeadingMargin = 8;

} // namespace

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_Hover);
    setAutoFillBackground(false);
    setFixedHeight(m_titleBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_anchorLayout = new view::AnchorLayout(this);
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

QSize TitleBar::sizeHint() const {
    return QSize(320, m_titleBarHeight);
}

QSize TitleBar::minimumSizeHint() const {
    return QSize(0, m_titleBarHeight);
}

void TitleBar::onThemeUpdated() {
    update();
}

void TitleBar::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    const auto& colors = themeColors();
    painter.fillRect(rect(), colors.bgCanvas);
    painter.setPen(colors.strokeDivider);
    painter.drawLine(rect().bottomLeft(), rect().bottomRight());
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
    if (!widget->isEnabled() || !widget->isVisibleTo(const_cast<TitleBar*>(this)))
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

    view::AnchorLayout::Anchors anchors;
    anchors.fill = true;
    anchors.fillMargins = QMargins(qMax(TitleBarDefaultLeadingMargin, m_systemReservedLeadingWidth),
                                   0,
                                   m_systemReservedTrailingWidth,
                                   0);
    m_anchorLayout->addAnchoredWidget(m_contentWidget, anchors);
}

} // namespace view::windowing

#include "AnnotatedScrollBar.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMargins>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QSizePolicy>
#include <QWheelEvent>

#include "ScrollView.h"
#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "components/status_info/ToolTip.h"

namespace fluent::scrolling {

namespace {
constexpr int kDetailGap = 8;

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(std::clamp(alpha, 0.0, 1.0));
    return color;
}
} // namespace

AnnotatedScrollBar::AnnotatedScrollBar(QWidget* parent)
    : QWidget(parent)
{
    init();
}

AnnotatedScrollBar::~AnnotatedScrollBar()
{
    disconnectScrollView();
    if (m_detailPopup)
        m_detailPopup->hide();
}

void AnnotatedScrollBar::init()
{
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setAccessibleName(QStringLiteral("AnnotatedScrollBar"));

    m_detailPopup = new fluent::status_info::ToolTip(this);
    m_detailPopup->setAnimationEnabled(false);
    m_detailPopup->setMargins(QMargins(12, 9, 12, 11));
    QFont detailFont(Typography::FontFamily::SegoeUIText, -1, Typography::FontWeight::SemiBold);
    detailFont.setPixelSize(Typography::FontSize::BodyStrong);
    m_detailPopup->setFont(detailFont);
    m_detailPopup->hide();
    updateColors();
}

void AnnotatedScrollBar::setMinimum(int minimum)
{
    setRange(minimum, m_maximum);
}

void AnnotatedScrollBar::setMaximum(int maximum)
{
    setRange(m_minimum, maximum);
}

void AnnotatedScrollBar::setRange(int minimum, int maximum)
{
    if (maximum < minimum)
        std::swap(minimum, maximum);

    const bool rangeChangedValue = m_minimum != minimum || m_maximum != maximum;
    const int oldValue = m_value;
    m_minimum = minimum;
    m_maximum = maximum;
    m_value = clampedValue(m_value);

    if (!rangeChangedValue && oldValue == m_value)
        return;

    invalidateLabelLayout();
    updateGeometry();
    update();

    if (rangeChangedValue)
        emit rangeChanged(m_minimum, m_maximum);
    if (oldValue != m_value)
        emit valueChanged(m_value);
}

void AnnotatedScrollBar::setValue(int value)
{
    setValueInternal(value, true);
}

void AnnotatedScrollBar::setPageStep(int pageStep)
{
    const int normalized = std::max(0, pageStep);
    if (m_pageStep == normalized)
        return;

    m_pageStep = normalized;
    invalidateLabelLayout();
    update();
    emit pageStepChanged(m_pageStep);
}

void AnnotatedScrollBar::setPreferredSize(const QSize& size)
{
    const QSize normalized(std::max(1, size.width()), std::max(1, size.height()));
    if (m_preferredSize == normalized)
        return;

    m_preferredSize = normalized;
    applyLayoutMetricChange(true);
}

void AnnotatedScrollBar::setMinimumBarSize(const QSize& size)
{
    const QSize normalized(std::max(1, size.width()), std::max(1, size.height()));
    if (m_minimumBarSize == normalized)
        return;

    m_minimumBarSize = normalized;
    applyLayoutMetricChange(true);
}

void AnnotatedScrollBar::setVerticalPadding(int padding)
{
    const int normalized = std::max(0, padding);
    if (m_verticalPadding == normalized)
        return;

    m_verticalPadding = normalized;
    applyLayoutMetricChange();
}

void AnnotatedScrollBar::setCaretSize(const QSize& size)
{
    const QSize normalized(std::max(1, size.width()), std::max(1, size.height()));
    if (m_caretSize == normalized)
        return;

    m_caretSize = normalized;
    applyLayoutMetricChange();
}

void AnnotatedScrollBar::setLabelColumnWidth(int width)
{
    const int normalized = std::max(0, width);
    if (m_labelColumnWidth == normalized)
        return;

    m_labelColumnWidth = normalized;
    applyLayoutMetricChange();
}

void AnnotatedScrollBar::setLabelLineHeight(int height)
{
    const int normalized = std::max(1, height);
    if (m_labelLineHeight == normalized)
        return;

    m_labelLineHeight = normalized;
    applyLayoutMetricChange();
}

void AnnotatedScrollBar::setMinimumLabelSpacing(int spacing)
{
    const int normalized = std::max(0, spacing);
    if (m_minimumLabelSpacing == normalized)
        return;

    m_minimumLabelSpacing = normalized;
    applyLayoutMetricChange();
}

void AnnotatedScrollBar::setIndicatorWidth(int width)
{
    const int normalized = std::max(1, width);
    if (m_indicatorWidth == normalized)
        return;

    m_indicatorWidth = normalized;
    applyLayoutMetricChange();
}

void AnnotatedScrollBar::setIndicatorThickness(int thickness)
{
    const int normalized = std::max(1, thickness);
    if (m_indicatorThickness == normalized)
        return;

    m_indicatorThickness = normalized;
    applyLayoutMetricChange();
}

void AnnotatedScrollBar::setLineStepFallback(int step)
{
    const int normalized = std::max(1, step);
    if (m_lineStepFallback == normalized)
        return;

    m_lineStepFallback = normalized;
    emit layoutMetricsChanged();
}

void AnnotatedScrollBar::setLabels(const QVector<AnnotatedScrollBarLabel>& labels)
{
    if (m_labels == labels)
        return;

    m_labels = labels;
    invalidateLabelLayout();
    update();
    emit labelsChanged();
}

void AnnotatedScrollBar::addLabel(const AnnotatedScrollBarLabel& label)
{
    m_labels.append(label);
    invalidateLabelLayout();
    update();
    emit labelsChanged();
}

void AnnotatedScrollBar::clearLabels()
{
    if (m_labels.isEmpty())
        return;

    m_labels.clear();
    invalidateLabelLayout();
    update();
    emit labelsChanged();
}

void AnnotatedScrollBar::setDetailLabelProvider(DetailLabelProvider provider)
{
    m_detailLabelProvider = std::move(provider);
    if (!m_detailLabelProvider)
        hideDetail();
}

void AnnotatedScrollBar::clearDetailLabelProvider()
{
    setDetailLabelProvider(DetailLabelProvider());
}

void AnnotatedScrollBar::connectToScrollView(ScrollView* scrollView)
{
    if (m_scrollView == scrollView) {
        syncFromScrollBar();
        return;
    }

    disconnectScrollView();
    if (!scrollView)
        return;

    m_scrollView = scrollView;
    syncFromScrollBar();

    QScrollBar* vertical = scrollView->verticalScrollBar();
    if (vertical) {
        m_scrollConnections.append(connect(vertical, &QScrollBar::rangeChanged, this, [this](int, int) {
            syncFromScrollBar();
        }));
        m_scrollConnections.append(connect(vertical, &QScrollBar::valueChanged, this, [this](int value) {
            setValueFromExternal(value);
            if (m_scrollView && m_scrollView->verticalScrollBar())
                setPageStep(m_scrollView->verticalScrollBar()->pageStep());
        }));
    }

    m_scrollConnections.append(connect(scrollView, &ScrollView::scrollPositionChanged, this, [this](int, int verticalOffset) {
        setValueFromExternal(verticalOffset);
        if (m_scrollView && m_scrollView->verticalScrollBar())
            setPageStep(m_scrollView->verticalScrollBar()->pageStep());
    }));
    m_scrollConnections.append(connect(this, &AnnotatedScrollBar::scrollRequested, scrollView, [this, scrollView](int offset) {
        if (!m_scrollView || m_scrollView != scrollView)
            return;
        scrollView->scrollTo(scrollView->horizontalOffset(), offset, false);
    }));
    m_scrollConnections.append(connect(scrollView, &QObject::destroyed, this, [this]() {
        disconnectScrollView();
    }));
}

void AnnotatedScrollBar::disconnectScrollView()
{
    for (const QMetaObject::Connection& connection : std::as_const(m_scrollConnections))
        QObject::disconnect(connection);
    m_scrollConnections.clear();
    m_scrollView = nullptr;
}

QVector<AnnotatedScrollBarLabel> AnnotatedScrollBar::visibleLabels() const
{
    ensureLabelLayout();
    QVector<AnnotatedScrollBarLabel> result;
    result.reserve(m_visibleLabels.size());
    for (const VisibleLabel& visible : m_visibleLabels)
        result.append(visible.label);
    return result;
}

int AnnotatedScrollBar::visibleLabelCount() const
{
    ensureLabelLayout();
    return m_visibleLabels.size();
}

QPointF AnnotatedScrollBar::indicatorCenter() const
{
    return indicatorRectForValue(m_value).center();
}

int AnnotatedScrollBar::offsetAtPosition(const QPoint& position) const
{
    const QRectF track = trackRect();
    if (track.height() <= 0.0 || m_maximum == m_minimum)
        return m_minimum;

    const qreal y = std::clamp<qreal>(position.y(), track.top(), track.bottom());
    const qreal ratio = (y - track.top()) / track.height();
    return clampedValue(m_minimum + qRound(ratio * (m_maximum - m_minimum)));
}

QSize AnnotatedScrollBar::sizeHint() const
{
    return m_preferredSize;
}

QSize AnnotatedScrollBar::minimumSizeHint() const
{
    return m_minimumBarSize;
}

void AnnotatedScrollBar::paintEvent(QPaintEvent*)
{
    ensureLabelLayout();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool enabled = isEnabled();
    const QColor labelColor = enabled ? m_labelColor : m_disabledColor;
    const QColor caretColor = enabled ? m_caretColor : m_disabledColor;
    QColor indicatorColor = enabled ? m_indicatorColor : m_disabledColor;
    if (enabled && (m_pressed || m_hovered))
        indicatorColor = m_indicatorHoverColor;

    painter.setFont(labelFont());
    painter.setPen(labelColor);
    for (const VisibleLabel& visible : m_visibleLabels)
        painter.drawText(visible.rect, Qt::AlignRight | Qt::AlignVCenter, visible.label.text);

    painter.setFont(iconFont(8));
    painter.setPen(caretColor);
    painter.drawText(topCaretRect(), Qt::AlignCenter, Typography::Icons::FlipViewPrevV);
    painter.drawText(bottomCaretRect(), Qt::AlignCenter, Typography::Icons::FlipViewNextV);

    const QRectF indicator = indicatorRectForValue(m_value);
    painter.setPen(Qt::NoPen);
    painter.setBrush(indicatorColor);
    painter.drawRoundedRect(indicator, m_indicatorThickness / 2.0, m_indicatorThickness / 2.0);
}

void AnnotatedScrollBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    invalidateLabelLayout();
    if (m_detailLabelVisible)
        hideDetail();
}

void AnnotatedScrollBar::mouseMoveEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint position = fluentMousePos(event);
    m_hovered = true;
    if (m_pressed || m_dragging)
        requestScrollTo(offsetAtPosition(position));
    updateDetailForPosition(position);
    update();
    event->accept();
}

void AnnotatedScrollBar::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint position = fluentMousePos(event);
    setFocus(Qt::MouseFocusReason);
    m_pressed = true;
    m_dragging = true;
    m_hovered = true;

    const int labelIndex = labelIndexAt(position);
    if (labelIndex >= 0) {
        const VisibleLabel& label = m_visibleLabels.at(labelIndex);
        emit labelActivated(label.label.offset, label.label.text);
        requestScrollTo(label.label.offset);
    } else {
        requestScrollTo(offsetAtPosition(position));
    }

    updateDetailForPosition(position);
    update();
    event->accept();
}

void AnnotatedScrollBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_pressed = false;
    m_dragging = false;
    if (rect().contains(fluentMousePos(event))) {
        updateDetailForPosition(fluentMousePos(event));
    } else {
        hideDetail();
    }
    update();
    event->accept();
}

void AnnotatedScrollBar::leaveEvent(QEvent* event)
{
    m_hovered = false;
    m_hoveredLabelIndex = -1;
    hideDetail();
    update();
    QWidget::leaveEvent(event);
}

void AnnotatedScrollBar::wheelEvent(QWheelEvent* event)
{
    if (!isEnabled()) {
        QWidget::wheelEvent(event);
        return;
    }

    const int deltaY = event->angleDelta().y();
    if (deltaY == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    const int direction = deltaY > 0 ? -1 : 1;
    const int target = m_value + direction * lineStep();
    const int previous = m_value;
    requestScrollTo(target);
    if (previous != m_value)
        updateDetailForPosition(fluentWheelPosition(event).toPoint());
    event->accept();
}

void AnnotatedScrollBar::keyPressEvent(QKeyEvent* event)
{
    if (!isEnabled()) {
        QWidget::keyPressEvent(event);
        return;
    }

    int target = m_value;
    bool handled = true;
    switch (event->key()) {
    case Qt::Key_Up:
        target -= lineStep();
        break;
    case Qt::Key_Down:
        target += lineStep();
        break;
    case Qt::Key_PageUp:
        target -= std::max(lineStep(), m_pageStep);
        break;
    case Qt::Key_PageDown:
        target += std::max(lineStep(), m_pageStep);
        break;
    case Qt::Key_Home:
        target = m_minimum;
        break;
    case Qt::Key_End:
        target = m_maximum;
        break;
    default:
        handled = false;
        break;
    }

    if (!handled) {
        QWidget::keyPressEvent(event);
        return;
    }

    requestScrollTo(target);
    event->accept();
}

void AnnotatedScrollBar::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange && !isEnabled()) {
        m_pressed = false;
        m_dragging = false;
        m_hovered = false;
        hideDetail();
    }
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange)
        update();
}

void AnnotatedScrollBar::onThemeUpdated()
{
    updateColors();
    invalidateLabelLayout();
    if (m_detailPopup)
        m_detailPopup->onThemeUpdated();
    update();
}

void AnnotatedScrollBar::updateColors()
{
    const auto colors = themeColors();
    const bool isDark = effectiveTheme() == FluentElement::Dark;

    m_labelColor = colors.textPrimary;
    m_caretColor = isDark ? withAlpha(QColor(Qt::white), 0.54) : withAlpha(QColor(Qt::black), 0.45);
    m_indicatorColor = colors.accentDefault;
    m_indicatorHoverColor = colors.accentSecondary;
    m_disabledColor = colors.textDisabled;
}

void AnnotatedScrollBar::invalidateLabelLayout()
{
    m_labelLayoutDirty = true;
}

void AnnotatedScrollBar::ensureLabelLayout() const
{
    if (!m_labelLayoutDirty && m_labelLayoutSize == size())
        return;

    struct Candidate {
        AnnotatedScrollBarLabel label;
        int originalIndex = -1;
        qreal centerY = 0.0;
        QRect rect;
    };

    QVector<Candidate> candidates;
    candidates.reserve(m_labels.size());

    const QRect labelsRect = labelColumnRect();
    for (int index = 0; index < m_labels.size(); ++index) {
        const AnnotatedScrollBarLabel& label = m_labels.at(index);
        const qreal y = yForOffset(label.offset);
        QRect textRect(labelsRect.left(),
                       qRound(y - m_labelLineHeight / 2.0),
                       labelsRect.width(),
                       m_labelLineHeight);
        candidates.append({label, index, y, textRect});
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        return lhs.label.offset < rhs.label.offset;
    });

    QVector<VisibleLabel> selected;
    auto canPlace = [this, &selected](const QRect& rect) {
        for (const VisibleLabel& existing : selected) {
            if (std::abs(existing.rect.center().y() - rect.center().y()) < m_minimumLabelSpacing)
                return false;
        }
        return true;
    };
    auto place = [&selected, &canPlace](const Candidate& candidate, bool force = false) {
        if (!force && !canPlace(candidate.rect))
            return false;
        selected.append({candidate.label, candidate.originalIndex, candidate.centerY, candidate.rect});
        return true;
    };

    if (!candidates.isEmpty()) {
        place(candidates.first(), true);
        if (candidates.size() > 1)
            place(candidates.last());
        for (int index = 1; index < candidates.size() - 1; ++index)
            place(candidates.at(index));
    }

    std::sort(selected.begin(), selected.end(), [](const VisibleLabel& lhs, const VisibleLabel& rhs) {
        if (qFuzzyCompare(lhs.centerY, rhs.centerY))
            return lhs.originalIndex < rhs.originalIndex;
        return lhs.centerY < rhs.centerY;
    });

    m_visibleLabels = selected;
    m_labelLayoutSize = size();
    m_labelLayoutDirty = false;
}

void AnnotatedScrollBar::syncFromScrollBar()
{
    if (!m_scrollView)
        return;

    QScrollBar* vertical = m_scrollView->verticalScrollBar();
    if (!vertical)
        return;

    setRange(vertical->minimum(), vertical->maximum());
    setPageStep(vertical->pageStep());
    setValueFromExternal(vertical->value());
}

void AnnotatedScrollBar::setValueFromExternal(int value)
{
    setValueInternal(value, true);
}

bool AnnotatedScrollBar::setValueInternal(int value, bool emitChange)
{
    const int normalized = clampedValue(value);
    if (m_value == normalized)
        return false;

    m_value = normalized;
    update();
    if (emitChange)
        emit valueChanged(m_value);
    return true;
}

void AnnotatedScrollBar::requestScrollTo(int offset)
{
    const int previous = m_value;
    setValueInternal(offset, true);
    if (previous != m_value)
        emit scrollRequested(m_value);
}

void AnnotatedScrollBar::updateDetailForPosition(const QPoint& position)
{
    if (!isEnabled()) {
        hideDetail();
        return;
    }

    ensureLabelLayout();
    const int labelIndex = labelIndexAt(position);
    m_hoveredLabelIndex = labelIndex;

    int offset = offsetAtPosition(position);
    QString text;
    if (labelIndex >= 0) {
        const VisibleLabel& label = m_visibleLabels.at(labelIndex);
        offset = clampedValue(label.label.offset);
        if (!m_detailLabelProvider)
            text = label.label.detailText.isEmpty() ? label.label.text : label.label.detailText;
    }

    emit detailLabelRequested(offset);
    if (m_detailLabelProvider)
        text = m_detailLabelProvider(offset);

    if (text.isEmpty()) {
        hideDetail();
        return;
    }

    showDetail(text, QPoint(width(), qRound(yForOffset(offset))));
}

void AnnotatedScrollBar::showDetail(const QString& text, const QPoint& localAnchor)
{
    m_detailLabelVisible = true;
    m_detailLabelText = text;
    if (m_detailPopup) {
        m_detailPopup->setText(text);
        const QPoint globalAnchor = mapToGlobal(localAnchor);
        const QSize popupSize = m_detailPopup->sizeHint();
        QRect available;
        if (QScreen* screen = QGuiApplication::screenAt(globalAnchor)) {
            available = screen->availableGeometry();
        } else if (QScreen* screen = QGuiApplication::primaryScreen()) {
            available = screen->availableGeometry();
        }

        int x = globalAnchor.x() + kDetailGap;
        int y = globalAnchor.y() - popupSize.height() / 2;
        if (available.isValid()) {
            if (x + popupSize.width() > available.right())
                x = globalAnchor.x() - popupSize.width() - kDetailGap;
            y = std::clamp(y, available.top(), std::max(available.top(), available.bottom() - popupSize.height()));
        }

        m_detailPopup->resize(popupSize);
        m_detailPopup->move(x, y);
        m_detailPopup->setVisible(true);
        if (QGuiApplication::platformName() != QLatin1String("offscreen"))
            m_detailPopup->raise();
    }
}

void AnnotatedScrollBar::hideDetail()
{
    m_detailLabelVisible = false;
    m_detailLabelText.clear();
    if (m_detailPopup)
        m_detailPopup->hide();
}

int AnnotatedScrollBar::lineStep() const
{
    if (m_pageStep > 0)
        return std::max(1, m_pageStep / 10);
    const int range = m_maximum - m_minimum;
    if (range > 0)
        return std::max(1, range / 20);
    return m_lineStepFallback;
}

QRectF AnnotatedScrollBar::trackRect() const
{
    const qreal top = std::min(height() / 2.0, m_verticalPadding + m_indicatorThickness / 2.0);
    const qreal bottom = std::max(top, height() - m_verticalPadding - m_indicatorThickness / 2.0);
    const qreal x = std::max(0, width() - m_indicatorWidth);
    return QRectF(x, top, std::min(m_indicatorWidth, width()), bottom - top);
}

QRect AnnotatedScrollBar::labelColumnRect() const
{
    return QRect(0,
                 qRound(trackRect().top()),
                 std::min(m_labelColumnWidth, width()),
                 qRound(trackRect().height()));
}

QRect AnnotatedScrollBar::topCaretRect() const
{
    return QRect(std::max(0, width() - m_caretSize.width()),
                 1,
                 std::min(m_caretSize.width(), width()),
                 m_caretSize.height());
}

QRect AnnotatedScrollBar::bottomCaretRect() const
{
    return QRect(std::max(0, width() - m_caretSize.width()),
                 std::max(0, height() - m_caretSize.height() - 1),
                 std::min(m_caretSize.width(), width()),
                 m_caretSize.height());
}

QRectF AnnotatedScrollBar::indicatorRectForValue(int value) const
{
    const QRectF track = trackRect();
    const qreal centerY = yForOffset(value);
    return QRectF(track.right() - m_indicatorWidth,
                  centerY - m_indicatorThickness / 2.0,
                  m_indicatorWidth,
                  m_indicatorThickness);
}

qreal AnnotatedScrollBar::yForOffset(int offset) const
{
    const QRectF track = trackRect();
    if (track.height() <= 0.0 || m_maximum == m_minimum)
        return track.center().y();

    const qreal ratio = static_cast<qreal>(clampedValue(offset) - m_minimum) /
                        static_cast<qreal>(m_maximum - m_minimum);
    return track.top() + ratio * track.height();
}

int AnnotatedScrollBar::labelIndexAt(const QPoint& position) const
{
    ensureLabelLayout();
    for (int index = 0; index < m_visibleLabels.size(); ++index) {
        QRect hitRect = m_visibleLabels.at(index).rect.adjusted(-4, -4, 8, 4);
        if (hitRect.contains(position))
            return index;
    }
    return -1;
}

int AnnotatedScrollBar::clampedValue(int value) const
{
    return std::clamp(value, m_minimum, m_maximum);
}

QFont AnnotatedScrollBar::labelFont() const
{
    const auto font = themeFont(Typography::FontRole::Body);
    return font.toQFont();
}

QFont AnnotatedScrollBar::iconFont(int pixelSize) const
{
    QFont font(Typography::FontFamily::SegoeFluentIcons);
    font.setPixelSize(pixelSize);
    return font;
}

void AnnotatedScrollBar::applyLayoutMetricChange(bool updateSizeHints)
{
    invalidateLabelLayout();
    if (updateSizeHints)
        updateGeometry();
    update();
    emit layoutMetricsChanged();
}

} // namespace fluent::scrolling

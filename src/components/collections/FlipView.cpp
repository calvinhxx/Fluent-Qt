#include "compatibility/private/WidgetCursorCompat_p.h"
#include "compatibility/InputEventCompat.h"
#include "FlipView.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "design/Typography.h"
#include "private/FlipViewAccessibility_p.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QVariant>
#include <QWheelEvent>

namespace fluent::collections {

// WinUI 3 FlipView metrics
namespace {
constexpr int kNavBtnW_H = 16;
constexpr int kNavBtnH_H = 28;
constexpr int kNavBtnW_V = 28;
constexpr int kNavBtnH_V = 16;
constexpr int kNavBtnMargin = 2;
constexpr int kNavBtnRadius = 3;
constexpr int kIndicatorDotSize = 6;
constexpr int kIndicatorSpacing = 8;
constexpr int kIndicatorMargin = 12;
constexpr int kArrowFontSize = 10;
constexpr int kGestureThreshold = 50; // Accumulated trackpad pixel/angle threshold
// Gaps > 120 ms start a new NoScrollPhase gesture cluster; covers ~20-100 ms RDP wheel jitter.
constexpr int kClusterGapMs = 120;
} // namespace

// Overlay painting nav buttons and page indicators above child pages
class FlipViewOverlay : public QWidget {
public:
    explicit FlipViewOverlay(FlipView* parent) : QWidget(parent), m_fv(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        auto radius = m_fv->themeRadius();
        const auto& c = m_fv->themeColorsRef();
        qreal r = radius.control;

        // Mask rounded corners using the nearest ancestor's fluentSurfaceColor, skipping layout containers
        // and ignoring QPalette::Window (which stays light in dark theme without a global dark palette).
        QPainterPath outer;
        outer.addRect(QRectF(rect()));
        QPainterPath inner;
        inner.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
        QColor parentBg = c.bgCanvas;
        for (const QWidget* w = m_fv->parentWidget(); w; w = w->parentWidget()) {
            const QVariant surfaceColor = w->property("fluentSurfaceColor");
            if (!surfaceColor.isValid())
                continue;
            const QColor sc = surfaceColor.value<QColor>();
            if (sc.isValid() && sc.alpha() > 0)
                parentBg = sc;
            break;
        }
        p.setPen(Qt::NoPen);
        p.fillPath(outer - inner, parentBg);

        p.setPen(QPen(m_fv->isEnabled() ? c.strokeDefault : c.strokeDivider, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);

        if (m_fv->isEnabled() && m_fv->m_showNavButtons && m_fv->m_isHovered && m_fv->m_pages.size() > 1) {
            if (m_fv->m_currentIndex > 0)
                m_fv->drawNavButton(p, m_fv->prevButtonRect(), false, m_fv->m_prevBtnHovered, m_fv->m_prevBtnPressed);
            if (m_fv->m_currentIndex < m_fv->m_pages.size() - 1)
                m_fv->drawNavButton(p, m_fv->nextButtonRect(), true, m_fv->m_nextBtnHovered, m_fv->m_nextBtnPressed);
        }

        if (m_fv->m_showPageIndicator && m_fv->m_pages.size() > 1)
            m_fv->drawPageIndicator(p);
    }

private:
    FlipView* m_fv;
};

FlipView::FlipView(QWidget* parent) : QWidget(parent)
{
    detail::ensureFlipViewAccessibilityFactory();
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_slideAnimation = new QPropertyAnimation(this, "slideOffset", this);
    m_slideAnimation->setDuration(themeAnimation().normal);
    m_slideAnimation->setEasingCurve(themeAnimation().decelerate);
    connect(m_slideAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_pendingFlipDir != 0) {
            int dir = m_pendingFlipDir;
            m_pendingFlipDir = 0;
            if (dir < 0)
                goPrevious();
            else
                goNext();
            m_wheelCooldown.start();
        }
    });

    m_overlay = new FlipViewOverlay(this);
}

FlipView::~FlipView()
{
    m_destroying = true;
    if (m_slideAnimation)
        m_slideAnimation->stop();

    for (PageRecord& record : m_pages) {
        QObject::disconnect(record.destroyedConnection);
        QWidget* page = record.page.data();
        if (!page)
            continue;
        if (record.ownership == WidgetOwnership::Borrowed) {
            page->setParent(nullptr);
        } else if (record.ownership == WidgetOwnership::Reparented) {
            page->setParent(record.originalParent.data());
        }
    }
    m_pages.clear();
}

void FlipView::onThemeUpdated()
{
    if (m_slideAnimation) {
        m_slideAnimation->setDuration(themeAnimation().normal);
        m_slideAnimation->setEasingCurve(themeAnimation().decelerate);
    }
    updateVisuals();
}

// ── Page management. zh_CN: 页面管理 ─────────────────────────────────────────

void FlipView::addPage(QWidget* page)
{
    addPage(page, WidgetOwnership::Owned);
}

bool FlipView::addPage(QWidget* page, WidgetOwnership ownership)
{
    return insertPage(m_pages.size(), page, ownership);
}

void FlipView::insertPage(int index, QWidget* page)
{
    insertPage(index, page, WidgetOwnership::Owned);
}

bool FlipView::insertPage(int index, QWidget* page, WidgetOwnership ownership)
{
    if (!page || page == this || page->isAncestorOf(this) || indexOfPage(page) >= 0) {
        return false;
    }

    const int oldCount = m_pages.size();
    const int oldIndex = m_currentIndex;
    index = qBound(0, index, m_pages.size());
    PageRecord record;
    record.identity = page;
    record.page = page;
    record.originalParent = page->parentWidget();
    record.ownership = ownership;
    record.destroyedConnection = connect(page, &QObject::destroyed, this, [this, page]() {
        handlePageDestroyed(page);
    });

    page->setParent(this);
    m_pages.insert(index, record);

    if (m_currentIndex < 0) {
        m_currentIndex = 0;
    } else if (index <= m_currentIndex) {
        m_currentIndex++;
    }

    layoutPages();
    raiseOverlay();
    updateVisuals();
    detail::notifyFlipViewAccessibilityStructureChanged(this, oldCount, oldIndex);
    return true;
}

void FlipView::removePage(int index)
{
    takePage(index);
}

bool FlipView::releasePage(int index)
{
    if (!isValidPageIndex(index))
        return false;

    PageRecord record = extractPageRecord(index);
    QWidget* page = record.page.data();
    if (!page)
        return true;

    page->hide();
    if (record.ownership == WidgetOwnership::Owned) {
        delete page;
    } else if (record.ownership == WidgetOwnership::Reparented) {
        page->setParent(record.originalParent.data());
    } else {
        page->setParent(nullptr);
    }
    return true;
}

QWidget* FlipView::takePage(int index)
{
    if (!isValidPageIndex(index))
        return nullptr;

    PageRecord record = extractPageRecord(index);
    QWidget* page = record.page.data();
    if (page) {
        page->hide();
        page->setParent(nullptr);
    }
    return page;
}

FlipView::PageRecord FlipView::extractPageRecord(int index)
{
    const int oldCount = m_pages.size();
    const int oldIndex = m_currentIndex;
    PageRecord record = m_pages.takeAt(index);
    QObject::disconnect(record.destroyedConnection);
    updateAfterPageRemoval(index);
    detail::notifyFlipViewAccessibilityStructureChanged(this, oldCount, oldIndex);
    return record;
}

void FlipView::updateAfterPageRemoval(int index)
{
    if (m_slideAnimation)
        m_slideAnimation->stop();
    m_slideOffset = 0.0;
    m_animatingFromIndex = -1;
    m_pendingFlipDir = 0;

    if (m_pages.isEmpty()) {
        m_currentIndex = -1;
    } else if (index < m_currentIndex) {
        m_currentIndex--;
    } else if (index == m_currentIndex) {
        m_currentIndex = qMin(m_currentIndex, m_pages.size() - 1);
    }

    layoutPages();
    raiseOverlay();
    updateVisuals();
    emit currentIndexChanged(m_currentIndex);
}

void FlipView::handlePageDestroyed(QWidget* page)
{
    if (m_destroying)
        return;
    const int index = indexOfPage(page);
    if (!isValidPageIndex(index))
        return;
    extractPageRecord(index);
}

QWidget* FlipView::pageAt(int index) const
{
    if (!isValidPageIndex(index))
        return nullptr;
    return m_pages.at(index).page.data();
}

int FlipView::pageCount() const
{
    return m_pages.size();
}

WidgetOwnership FlipView::pageOwnershipAt(int index) const
{
    if (!isValidPageIndex(index))
        return WidgetOwnership::Borrowed;
    return m_pages.at(index).ownership;
}

int FlipView::indexOfPage(const QWidget* page) const
{
    if (!page)
        return -1;
    for (int index = 0; index < m_pages.size(); ++index) {
        if (m_pages.at(index).identity == page)
            return index;
    }
    return -1;
}

bool FlipView::isValidPageIndex(int index) const
{
    return index >= 0 && index < m_pages.size();
}

// ── Properties. zh_CN: 属性 ──────────────────────────────────────────────────

void FlipView::setCurrentIndex(int index)
{
    if (m_pages.isEmpty())
        return;
    index = qBound(0, index, m_pages.size() - 1);
    if (m_currentIndex == index)
        return;

    int oldIndex = m_currentIndex;
    m_currentIndex = index;
    animateSlide(oldIndex, index);
    updateVisuals();
    emit currentIndexChanged(m_currentIndex);
    detail::notifyFlipViewAccessibilityCurrentChanged(this, oldIndex);
}

void FlipView::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    layoutPages();
    updateVisuals();
    emit orientationChanged();
    detail::notifyFlipViewAccessibilityOrientationChanged(this);
}

void FlipView::setShowNavigationButtons(bool show)
{
    if (m_showNavButtons == show)
        return;
    m_showNavButtons = show;
    updateVisuals();
    emit showNavigationButtonsChanged();
}

void FlipView::setShowPageIndicator(bool show)
{
    if (m_showPageIndicator == show)
        return;
    m_showPageIndicator = show;
    updateVisuals();
    emit showPageIndicatorChanged();
}

void FlipView::setSlideOffset(qreal offset)
{
    if (qFuzzyCompare(m_slideOffset, offset))
        return;
    m_slideOffset = offset;
    layoutPages();
    updateVisuals();
}

void FlipView::goNext()
{
    if (m_currentIndex < m_pages.size() - 1) {
        setCurrentIndex(m_currentIndex + 1);
    }
}

void FlipView::goPrevious()
{
    if (m_currentIndex > 0) {
        setCurrentIndex(m_currentIndex - 1);
    }
}

QSize FlipView::sizeHint() const
{
    return QSize(400, 270);
}

QSize FlipView::minimumSizeHint() const
{
    return QSize(100, 60);
}

// ── Geometry helpers. zh_CN: 几何辅助 ────────────────────────────────────────

QRect FlipView::contentRect() const
{
    return rect();
}

QRect FlipView::prevButtonRect() const
{
    QRect cr = contentRect();
    if (m_orientation == Qt::Horizontal) {
        int y = cr.center().y() - kNavBtnH_H / 2;
        return QRect(cr.left() + kNavBtnMargin, y, kNavBtnW_H, kNavBtnH_H);
    } else {
        int x = cr.center().x() - kNavBtnW_V / 2;
        return QRect(x, cr.top() + kNavBtnMargin, kNavBtnW_V, kNavBtnH_V);
    }
}

QRect FlipView::nextButtonRect() const
{
    QRect cr = contentRect();
    if (m_orientation == Qt::Horizontal) {
        int y = cr.center().y() - kNavBtnH_H / 2;
        return QRect(cr.right() - kNavBtnW_H - kNavBtnMargin, y, kNavBtnW_H, kNavBtnH_H);
    } else {
        int x = cr.center().x() - kNavBtnW_V / 2;
        return QRect(x, cr.bottom() - kNavBtnH_V - kNavBtnMargin, kNavBtnW_V, kNavBtnH_V);
    }
}

QRect FlipView::pageIndicatorRect() const
{
    QRect cr = contentRect();
    int dotCount = m_pages.size();
    if (dotCount <= 0)
        return QRect();

    if (m_orientation == Qt::Horizontal) {
        int totalW = dotCount * kIndicatorDotSize + (dotCount - 1) * kIndicatorSpacing;
        int x = cr.center().x() - totalW / 2;
        int y = cr.bottom() - kIndicatorMargin - kIndicatorDotSize;
        return QRect(x, y, totalW, kIndicatorDotSize);
    } else {
        int totalH = dotCount * kIndicatorDotSize + (dotCount - 1) * kIndicatorSpacing;
        int x = cr.right() - kIndicatorMargin - kIndicatorDotSize;
        int y = cr.center().y() - totalH / 2;
        return QRect(x, y, kIndicatorDotSize, totalH);
    }
}

// ── Layout. zh_CN: 布局 ──────────────────────────────────────────────────────

void FlipView::layoutPages()
{
    QRect cr = contentRect();
    for (int i = 0; i < m_pages.size(); ++i) {
        QWidget* page = m_pages[i].page.data();
        if (!page)
            continue;
        if (i == m_currentIndex) {
            if (m_orientation == Qt::Horizontal) {
                int offsetPx = static_cast<int>(m_slideOffset * cr.width());
                page->setGeometry(cr.x() + offsetPx, cr.y(), cr.width(), cr.height());
            } else {
                int offsetPx = static_cast<int>(m_slideOffset * cr.height());
                page->setGeometry(cr.x(), cr.y() + offsetPx, cr.width(), cr.height());
            }
            page->setVisible(true);
        } else if (i == m_animatingFromIndex && qAbs(m_slideOffset) > 0.001) {
            if (m_orientation == Qt::Horizontal) {
                int dir = (m_animatingFromIndex < m_currentIndex) ? -1 : 1;
                int offsetPx = static_cast<int>((m_slideOffset + dir) * cr.width());
                page->setGeometry(cr.x() + offsetPx, cr.y(), cr.width(), cr.height());
            } else {
                int dir = (m_animatingFromIndex < m_currentIndex) ? -1 : 1;
                int offsetPx = static_cast<int>((m_slideOffset + dir) * cr.height());
                page->setGeometry(cr.x(), cr.y() + offsetPx, cr.width(), cr.height());
            }
            page->setVisible(true);
        } else {
            page->setVisible(false);
        }
    }
}

// ── Animation. zh_CN: 动画 ───────────────────────────────────────────────────

void FlipView::animateSlide(int fromIndex, int toIndex)
{
    m_animatingFromIndex = fromIndex;
    m_slideAnimation->stop();

    qreal startOffset = (toIndex > fromIndex) ? 1.0 : -1.0;
    m_slideAnimation->setStartValue(startOffset);
    m_slideAnimation->setEndValue(0.0);
    ::fluent::detail::startMotionTransition(m_slideAnimation, themeAnimation().normal);
}

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void FlipView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto& c = themeColorsRef();
    auto radius = themeRadius();

    QPainterPath bgPath;
    bgPath.addRoundedRect(QRectF(rect()), radius.control, radius.control);
    p.setClipPath(bgPath);
    p.fillRect(rect(), c.bgSolid);

    // Nav buttons and the indicator are painted by FlipViewOverlay above child pages.
}

void FlipView::drawNavButton(QPainter& p, const QRect& btnRect, bool isNext, bool hovered, bool pressed)
{
    const auto& c = themeColorsRef();

    // Base acrylic fill on opaque surface tokens (bgLayer/bgSolid) rather than control* tokens,
    // whose baked-in white alpha in dark theme would turn buttons bright white at alpha 230.
    QColor bgColor;
    if (pressed) {
        bgColor = c.bgSolid;
    } else if (hovered) {
        bgColor = c.bgLayerAlt;
    } else {
        bgColor = c.bgLayer;
    }
    bgColor.setAlpha(230);

    QPainterPath btnPath;
    btnPath.addRoundedRect(QRectF(btnRect), kNavBtnRadius, kNavBtnRadius);
    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.drawPath(btnPath);

    // WinUI FlipViewButtonBorderThemeThickness=0 (no outline) and ScalePressed=0.875.
    QColor arrowColor = (pressed || hovered) ? c.textSecondary : c.strokeStrong;
    const int arrowSize = pressed ? static_cast<int>(kArrowFontSize * 0.875) : kArrowFontSize;
    p.setPen(arrowColor);

    const QString glyph = (m_orientation == Qt::Horizontal)
        ? (isNext ? Typography::Icons::ChevronRight : Typography::Icons::ChevronLeft)
        : (isNext ? Typography::Icons::ChevronDown : Typography::Icons::ChevronUp);
    Typography::Icons::paintGlyph(p, QRectF(btnRect), glyph, arrowSize, Qt::AlignCenter);
}

void FlipView::drawPageIndicator(QPainter& p)
{
    const auto& c = themeColorsRef();
    QRect indRect = pageIndicatorRect();
    if (indRect.isNull())
        return;

    QColor selectedColor, unselectedColor;
    selectedColor = isEnabled() ? c.textPrimary : c.textDisabled;
    unselectedColor = isEnabled() ? c.textTertiary : c.strokeDivider;

    for (int i = 0; i < m_pages.size(); ++i) {
        QRect dotRect;
        if (m_orientation == Qt::Horizontal) {
            int x = indRect.x() + i * (kIndicatorDotSize + kIndicatorSpacing);
            dotRect = QRect(x, indRect.y(), kIndicatorDotSize, kIndicatorDotSize);
        } else {
            int y = indRect.y() + i * (kIndicatorDotSize + kIndicatorSpacing);
            dotRect = QRect(indRect.x(), y, kIndicatorDotSize, kIndicatorDotSize);
        }
        QColor dotColor = (i == m_currentIndex) ? selectedColor : unselectedColor;
        p.setPen(Qt::NoPen);
        p.setBrush(dotColor);
        p.drawEllipse(dotRect);
    }
}

void FlipView::updateMask()
{
    // Rounded clipping uses paintEvent + FlipViewOverlay instead of jagged QRegion setMask().
    clearMask();
}

void FlipView::refreshNavigationHover()
{
    const bool available = isEnabled() && m_showNavButtons && m_isHovered && m_pages.size() > 1;
    const bool prev = available && m_currentIndex > 0 && prevButtonRect().contains(m_hoverPosition);
    const bool next = available && m_currentIndex + 1 < m_pages.size() && nextButtonRect().contains(m_hoverPosition);
    if (prev == m_prevBtnHovered && next == m_nextBtnHovered)
        return;
    m_prevBtnHovered = prev;
    m_nextBtnHovered = next;
    if (prev || next)
        updateWidgetCursor(m_pointerCursor, this, Qt::PointingHandCursor);
    else
        updateWidgetCursor(m_pointerCursor, this);
    update();
    if (m_overlay)
        m_overlay->update();
}

void FlipView::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange)
        refreshNavigationHover();
}

void FlipView::updateVisuals()
{
    refreshNavigationHover();
    update();
    // Update the overlay separately because opaque child pages can suppress parent repaints.
    if (m_overlay)
        m_overlay->update();
}

void FlipView::raiseOverlay()
{
    if (m_overlay) {
        m_overlay->raise();
    }
}

void FlipView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_overlay) {
        m_overlay->setGeometry(rect());
    }
    updateMask();
    layoutPages();
    refreshNavigationHover();
}

void FlipView::enterEvent(FluentEnterEvent* event)
{
    m_hoverPosition = fluentEnterPos(event);
    m_isHovered = true;
    updateVisuals();
    QWidget::enterEvent(event);
}

void FlipView::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    m_prevBtnPressed = false;
    m_nextBtnPressed = false;
    updateVisuals();
    QWidget::leaveEvent(event);
}

void FlipView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        if (m_showNavButtons && m_isHovered && m_pages.size() > 1) {
            if (m_currentIndex > 0 && prevButtonRect().contains(pos)) {
                m_prevBtnPressed = true;
                updateVisuals();
                return;
            }
            if (m_currentIndex < m_pages.size() - 1 && nextButtonRect().contains(pos)) {
                m_nextBtnPressed = true;
                updateVisuals();
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void FlipView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        if (m_prevBtnPressed) {
            m_prevBtnPressed = false;
            if (prevButtonRect().contains(pos)) {
                goPrevious();
            }
            updateVisuals();
            return;
        }
        if (m_nextBtnPressed) {
            m_nextBtnPressed = false;
            if (nextButtonRect().contains(pos)) {
                goNext();
            }
            updateVisuals();
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void FlipView::mouseMoveEvent(QMouseEvent* event)
{
    m_hoverPosition = event->pos();
    refreshNavigationHover();
    QWidget::mouseMoveEvent(event);
}

void FlipView::wheelEvent(QWheelEvent* event)
{
    if (m_pages.size() <= 1) {
        QWidget::wheelEvent(event);
        return;
    }

    const auto phase = event->phase();

    // Phase-based trackpad gestures (native macOS / Windows WM_POINTER)
    if (phase == Qt::ScrollBegin) {
        m_gestureAccum = 0;
        m_gestureConsumed = false;
        m_pendingFlipDir = 0;
        m_npAccum = 0;
        m_npConsumed = false;
        event->accept();
        return;
    }
    if (phase == Qt::ScrollMomentum || phase == Qt::ScrollEnd) {
        // Start cooldown so trailing NoScrollPhase WM_MOUSEWHEEL inertia on Windows is ignored.
        m_wheelCooldown.start();
        event->accept();
        return;
    }
    if (phase == Qt::ScrollUpdate) {
        if (m_gestureConsumed) {
            event->accept();
            return;
        }
        // Accumulate even mid-animation and queue at most one pending flip for fast consecutive swipes.
        int delta = !event->pixelDelta().isNull()
            ? (m_orientation == Qt::Horizontal ? event->pixelDelta().x() : event->pixelDelta().y())
            : (m_orientation == Qt::Horizontal
                      ? event->angleDelta().x() + event->angleDelta().y()
                      : event->angleDelta().y());
        m_gestureAccum += delta;
        if (qAbs(m_gestureAccum) >= kGestureThreshold) {
            m_gestureConsumed = true;
            m_npConsumed = true; // Suppress post-gesture NoScrollPhase inertia.
            m_wheelCooldown.start();
            if (m_slideAnimation->state() == QAbstractAnimation::Running) {
                m_pendingFlipDir = (m_gestureAccum > 0) ? -1 : 1;
            } else {
                (m_gestureAccum > 0) ? goPrevious() : goNext();
            }
        }
        event->accept();
        return;
    }

    // NoScrollPhase (mouse wheel, WM_MOUSEWHEEL precision touchpad, RDP): group into time-gap clusters
    // so one gesture flips once and high-rate streams don't chain-flip.
    const bool animating = m_slideAnimation->state() == QAbstractAnimation::Running;

    int delta =
        (m_orientation == Qt::Horizontal) ? event->angleDelta().x() + event->angleDelta().y() : event->angleDelta().y();

    const qint64 sinceLast = m_wheelCooldown.isValid() ? m_wheelCooldown.elapsed() : 10000;
    m_wheelCooldown.start();

    if (sinceLast > kClusterGapMs) {
        m_npConsumed = false;
        m_npAccum = 0;
    }

    // Never queue m_pendingFlipDir here: RDP jitter > kClusterGapMs looks like a new cluster and would chain-flip.
    if (animating) {
        if (!m_npConsumed) {
            m_npAccum += delta;
            if (qAbs(m_npAccum) >= kGestureThreshold) {
                m_npConsumed = true;
            }
        }
        event->accept();
        return;
    }

    if (m_npConsumed) {
        event->accept();
        return;
    }

    m_npAccum += delta;

    if (qAbs(m_npAccum) < kGestureThreshold) {
        event->accept();
        return;
    }

    m_npConsumed = true;
    int dir = (m_npAccum > 0) ? -1 : 1;
    if (dir < 0)
        goPrevious();
    else
        goNext();
    event->accept();
}

void FlipView::keyPressEvent(QKeyEvent* event)
{
    if (m_orientation == Qt::Horizontal) {
        if (event->key() == Qt::Key_Left) {
            goPrevious();
            return;
        }
        if (event->key() == Qt::Key_Right) {
            goNext();
            return;
        }
    } else {
        if (event->key() == Qt::Key_Up) {
            goPrevious();
            return;
        }
        if (event->key() == Qt::Key_Down) {
            goNext();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

} // namespace fluent::collections

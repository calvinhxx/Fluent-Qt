#include "FlipView.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include "design/Typography.h"

namespace fluent::collections {

// ── WinUI 3 FlipView metrics. zh_CN: WinUI 3 FlipView 尺寸常量 ──────────────
namespace {
    constexpr int kNavBtnW_H = 16;        // Nav button width, horizontal mode. zh_CN: 水平模式导航按钮宽度。
    constexpr int kNavBtnH_H = 28;        // Nav button height, horizontal mode. zh_CN: 水平模式导航按钮高度。
    constexpr int kNavBtnW_V = 28;        // Nav button width, vertical mode. zh_CN: 垂直模式导航按钮宽度。
    constexpr int kNavBtnH_V = 16;        // Nav button height, vertical mode. zh_CN: 垂直模式导航按钮高度。
    constexpr int kNavBtnMargin = 2;      // Nav button distance from the edge. zh_CN: 导航按钮到边缘距离。
    constexpr int kNavBtnRadius = 3;      // Button corner radius. zh_CN: 按钮圆角。
    constexpr int kIndicatorDotSize = 6;
    constexpr int kIndicatorSpacing = 8;
    constexpr int kIndicatorMargin = 12;
    constexpr int kArrowFontSize = 10;    // Chevron icon size
    constexpr int kGestureThreshold = 50;  // Accumulated trackpad pixel/angle threshold. zh_CN: trackpad 累积像素/角度阈值。
    // NoScrollPhase cluster detection: gaps above this start a new gesture cluster.
    // Windows precision trackpads emit every ~8-16ms with >100ms between gestures;
    // RDP forwards at ~20-30ms and can spike to 60-100ms under network jitter.
    // 120ms covers a single gesture's tail without mistaking RDP jitter for a new cluster.
    // zh_CN: NoScrollPhase 事件 cluster 检测：事件间隔大于此值视为新手势。
    // Windows 精密触控板事件间隔 ~8-16ms，手势间隔 >100ms；RDP 转发约 20-30ms，
    // 网络抖动下可达 60-100ms。120ms 阈值覆盖单次手势尾巴，避免 RDP 抖动误判为新 cluster。
    constexpr int kClusterGapMs = 120;
}

// ── Overlay: paints nav buttons and the indicator above child pages. zh_CN: 覆盖层：在子页面之上绘制导航按钮和指示器 ──

class FlipViewOverlay : public QWidget {
public:
    explicit FlipViewOverlay(FlipView* parent)
        : QWidget(parent), m_fv(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        auto radius = m_fv->themeRadius();
        const auto& c = m_fv->themeColors();
        qreal r = radius.control;

        // Corner mask: fill the outer corners with the parent background (antialiased).
        // zh_CN: 圆角遮罩：用父控件背景色填充外角（抗锯齿）。
        QPainterPath outer;
        outer.addRect(QRectF(rect()));
        QPainterPath inner;
        inner.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
        QColor parentBg = c.bgCanvas;
        if (m_fv->parentWidget()) {
            const QPalette& pp = m_fv->parentWidget()->palette();
            if (pp.color(QPalette::Window).alpha() > 0)
                parentBg = pp.color(QPalette::Window);
        }
        p.setPen(Qt::NoPen);
        p.fillPath(outer - inner, parentBg);

        // Rounded border. zh_CN: 圆角边框。
        p.setPen(QPen(c.strokeDefault, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);

        // Navigation buttons. zh_CN: 导航按钮。
        if (m_fv->m_showNavButtons && m_fv->m_isHovered && m_fv->m_pages.size() > 1) {
            if (m_fv->m_currentIndex > 0)
                m_fv->drawNavButton(p, m_fv->prevButtonRect(), false,
                                    m_fv->m_prevBtnHovered, m_fv->m_prevBtnPressed);
            if (m_fv->m_currentIndex < m_fv->m_pages.size() - 1)
                m_fv->drawNavButton(p, m_fv->nextButtonRect(), true,
                                    m_fv->m_nextBtnHovered, m_fv->m_nextBtnPressed);
        }

        // Page indicator. zh_CN: 页面指示器。
        if (m_fv->m_showPageIndicator && m_fv->m_pages.size() > 1)
            m_fv->drawPageIndicator(p);
    }

private:
    FlipView* m_fv;
};

FlipView::FlipView(QWidget* parent)
    : QWidget(parent)
{
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
            if (dir < 0) goPrevious(); else goNext();
            m_wheelCooldown.start();
        }
    });

    m_overlay = new FlipViewOverlay(this);
}

void FlipView::onThemeUpdated()
{
    if (m_slideAnimation) {
        m_slideAnimation->setDuration(themeAnimation().normal);
        m_slideAnimation->setEasingCurve(themeAnimation().decelerate);
    }
    update();
}

// ── Page management. zh_CN: 页面管理 ─────────────────────────────────────────

void FlipView::addPage(QWidget* page)
{
    insertPage(m_pages.size(), page);
}

void FlipView::insertPage(int index, QWidget* page)
{
    index = qBound(0, index, m_pages.size());
    page->setParent(this);
    m_pages.insert(index, page);

    if (m_currentIndex < 0) {
        m_currentIndex = 0;
    } else if (index <= m_currentIndex) {
        m_currentIndex++;
    }

    layoutPages();
    raiseOverlay();
    update();
}

void FlipView::removePage(int index)
{
    if (index < 0 || index >= m_pages.size()) return;

    QWidget* page = m_pages.takeAt(index);
    page->setParent(nullptr);

    if (m_pages.isEmpty()) {
        m_currentIndex = -1;
    } else if (index < m_currentIndex) {
        m_currentIndex--;
    } else if (index == m_currentIndex) {
        m_currentIndex = qMin(m_currentIndex, m_pages.size() - 1);
    }

    layoutPages();
    update();
    emit currentIndexChanged(m_currentIndex);
}

QWidget* FlipView::pageAt(int index) const
{
    if (index < 0 || index >= m_pages.size()) return nullptr;
    return m_pages.at(index);
}

int FlipView::pageCount() const
{
    return m_pages.size();
}

// ── Properties. zh_CN: 属性 ──────────────────────────────────────────────────

void FlipView::setCurrentIndex(int index)
{
    if (m_pages.isEmpty()) return;
    index = qBound(0, index, m_pages.size() - 1);
    if (m_currentIndex == index) return;

    int oldIndex = m_currentIndex;
    m_currentIndex = index;
    animateSlide(oldIndex, index);
    emit currentIndexChanged(m_currentIndex);
}

void FlipView::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) return;
    m_orientation = orientation;
    layoutPages();
    update();
    emit orientationChanged();
}

void FlipView::setShowNavigationButtons(bool show)
{
    if (m_showNavButtons == show) return;
    m_showNavButtons = show;
    update();
    emit showNavigationButtonsChanged();
}

void FlipView::setShowPageIndicator(bool show)
{
    if (m_showPageIndicator == show) return;
    m_showPageIndicator = show;
    update();
    emit showPageIndicatorChanged();
}

void FlipView::setSlideOffset(qreal offset)
{
    if (qFuzzyCompare(m_slideOffset, offset)) return;
    m_slideOffset = offset;
    layoutPages();
    update();
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
    if (dotCount <= 0) return QRect();

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
        QWidget* page = m_pages[i];
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
    m_slideAnimation->start();
}

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void FlipView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto& c = themeColors();
    auto radius = themeRadius();

    // ── Background. zh_CN: 背景 ──
    QPainterPath bgPath;
    bgPath.addRoundedRect(QRectF(rect()), radius.control, radius.control);
    p.setClipPath(bgPath);
    p.fillRect(rect(), c.bgSolid);

    // Nav buttons and the indicator are painted by FlipViewOverlay above child pages.
    // zh_CN: 导航按钮和指示器由 FlipViewOverlay 在子页面之上绘制。
}

void FlipView::drawNavButton(QPainter& p, const QRect& btnRect, bool isNext,
                              bool hovered, bool pressed)
{
    const auto& c = themeColors();

    // Background: an in-app acrylic approximation. zh_CN: 背景 — AcrylicInApp 近似。
    QColor bgColor;
    if (pressed) {
        bgColor = c.controlTertiary;
    } else if (hovered) {
        bgColor = c.controlSecondary;
    } else {
        bgColor = c.controlDefault;
    }
    bgColor.setAlpha(230);

    QPainterPath btnPath;
    btnPath.addRoundedRect(QRectF(btnRect), kNavBtnRadius, kNavBtnRadius);
    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.drawPath(btnPath);

    // WinUI FlipViewButtonBorderThemeThickness=0: buttons have no outline.
    // zh_CN: WinUI FlipViewButtonBorderThemeThickness=0，按钮无描边。

    // Arrow glyph. zh_CN: 箭头。
    QColor arrowColor = (pressed || hovered) ? c.textSecondary : c.strokeStrong;

    // FlipViewButtonScalePressed=0.875: the arrow shrinks while pressed.
    // zh_CN: FlipViewButtonScalePressed=0.875，按压时箭头缩小。
    int arrowSize = pressed ? static_cast<int>(kArrowFontSize * 0.875) : kArrowFontSize;
    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(arrowSize);
    p.setFont(iconFont);
    p.setPen(arrowColor);

    // Drawn with the Chevron icon font. zh_CN: 使用 Chevron iconfont。
    QString glyph;
    if (m_orientation == Qt::Horizontal) {
        glyph = isNext ? Typography::Icons::ChevronRight : Typography::Icons::ChevronLeft;
    } else {
        glyph = isNext ? Typography::Icons::ChevronDown : Typography::Icons::ChevronUp;
    }
    p.drawText(btnRect, Qt::AlignCenter, glyph);
}

void FlipView::drawPageIndicator(QPainter& p)
{
    const auto& c = themeColors();
    QRect indRect = pageIndicatorRect();
    if (indRect.isNull()) return;

    for (int i = 0; i < m_pages.size(); ++i) {
        QRect dotRect;
        if (m_orientation == Qt::Horizontal) {
            int x = indRect.x() + i * (kIndicatorDotSize + kIndicatorSpacing);
            dotRect = QRect(x, indRect.y(), kIndicatorDotSize, kIndicatorDotSize);
        } else {
            int y = indRect.y() + i * (kIndicatorDotSize + kIndicatorSpacing);
            dotRect = QRect(indRect.x(), y, kIndicatorDotSize, kIndicatorDotSize);
        }
        QColor dotColor = (i == m_currentIndex) ? c.textPrimary : c.textTertiary;
        p.setPen(Qt::NoPen);
        p.setBrush(dotColor);
        p.drawEllipse(dotRect);
    }
}

// ── Event handling. zh_CN: 事件处理 ──────────────────────────────────────────

void FlipView::updateMask()
{
    // setMask is avoided: QRegion clips per-pixel without antialiasing.
    // Rounded clipping is done via paintEvent clip plus the overlay mask.
    // zh_CN: 不使用 setMask（QRegion 像素级裁剪无抗锯齿）；
    // 圆角裁剪由 paintEvent clip + overlay 遮罩实现。
    clearMask();
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
}

void FlipView::enterEvent(FluentEnterEvent* event)
{
    m_isHovered = true;
    update();
    QWidget::enterEvent(event);
}

void FlipView::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    m_prevBtnHovered = false;
    m_nextBtnHovered = false;
    m_prevBtnPressed = false;
    m_nextBtnPressed = false;
    update();
    QWidget::leaveEvent(event);
}

void FlipView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        if (m_showNavButtons && m_isHovered && m_pages.size() > 1) {
            if (m_currentIndex > 0 && prevButtonRect().contains(pos)) {
                m_prevBtnPressed = true;
                update();
                return;
            }
            if (m_currentIndex < m_pages.size() - 1 && nextButtonRect().contains(pos)) {
                m_nextBtnPressed = true;
                update();
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
            update();
            return;
        }
        if (m_nextBtnPressed) {
            m_nextBtnPressed = false;
            if (nextButtonRect().contains(pos)) {
                goNext();
            }
            update();
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void FlipView::mouseMoveEvent(QMouseEvent* event)
{
    QPoint pos = event->pos();
    bool prevHovered = false;
    bool nextHovered = false;

    if (m_showNavButtons && m_isHovered && m_pages.size() > 1) {
        if (m_currentIndex > 0) {
            prevHovered = prevButtonRect().contains(pos);
        }
        if (m_currentIndex < m_pages.size() - 1) {
            nextHovered = nextButtonRect().contains(pos);
        }
    }

    if (prevHovered != m_prevBtnHovered || nextHovered != m_nextBtnHovered) {
        m_prevBtnHovered = prevHovered;
        m_nextBtnHovered = nextHovered;
        setCursor((prevHovered || nextHovered) ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void FlipView::wheelEvent(QWheelEvent* event)
{
    if (m_pages.size() <= 1) {
        QWidget::wheelEvent(event);
        return;
    }

    const auto phase = event->phase();

    // ── Phase-based: trackpad gestures (Begin → Update → Momentum → End). zh_CN: 触控板手势 ──
    // Native macOS and Windows precision trackpads (WM_POINTER) provide the full phase chain.
    // zh_CN: macOS 原生 / Windows 精密触控板 (WM_POINTER) 提供完整 phase 链。
    if (phase == Qt::ScrollBegin) {
        // New gesture start: reset all state (phase + NoScrollPhase).
        // zh_CN: 新手势起点：重置所有状态（phase + NoScrollPhase）。
        m_gestureAccum = 0;
        m_gestureConsumed = false;
        m_pendingFlipDir = 0;
        m_npAccum = 0;
        m_npConsumed = false;
        event->accept();
        return;
    }
    if (phase == Qt::ScrollMomentum || phase == Qt::ScrollEnd) {
        // Bridge phase → NoScrollPhase debounce: after a Windows precision-trackpad
        // gesture ends, inertia may keep arriving as NoScrollPhase (WM_MOUSEWHEEL).
        // Start a cooldown so those inertia events are debounced correctly.
        // zh_CN: phase → NoScrollPhase 防抖桥接：Windows 精密触控板手势结束后，
        // 惯性滚动可能以 NoScrollPhase (WM_MOUSEWHEEL) 形式到达；
        // 启动 cooldown 使这些惯性事件被正确拦截。
        m_wheelCooldown.start();
        event->accept();
        return;
    }
    if (phase == Qt::ScrollUpdate) {
        if (m_gestureConsumed) { event->accept(); return; }
        // Always accumulate, even mid-animation. If the threshold is hit while the
        // animation still plays, store the direction in m_pendingFlipDir and flip
        // when it finishes. Fixes fast consecutive Windows swipes whose ScrollUpdates
        // all land during the previous animation and used to be dropped by the guard.
        // zh_CN: 始终累积——即使动画进行中。阈值达成时若动画未结束，将方向存入
        // m_pendingFlipDir，动画结束后自动执行；修复 Windows 快速连续滑动时
        // ScrollUpdate 全部落在上一次动画期间而被守卫丢弃的问题。
        int delta = !event->pixelDelta().isNull()
            ? (m_orientation == Qt::Horizontal ? event->pixelDelta().x() : event->pixelDelta().y())
            : (m_orientation == Qt::Horizontal
                ? event->angleDelta().x() + event->angleDelta().y()
                : event->angleDelta().y());
        m_gestureAccum += delta;
        if (qAbs(m_gestureAccum) >= kGestureThreshold) {
            m_gestureConsumed = true;
            m_npConsumed = true; // Bridge: stop post-gesture NoScrollPhase inertia from flipping again. zh_CN: 桥接：防止手势结束后 NoScrollPhase 惯性事件再翻页。
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

    // ── NoScrollPhase: mouse wheel / Windows trackpad WM_MOUSEWHEEL / RDP ──
    // Most Windows precision-trackpad two-finger scrolls arrive as WM_MOUSEWHEEL,
    // i.e. all NoScrollPhase. Mac-over-RDP trackpads also map to WM_MOUSEWHEEL
    // (angleDelta=±120, high frequency). Everything shares the cluster path: the
    // first event flips once |angleDelta| ≥ kGestureThreshold, later events in the
    // same cluster are consumed so high-rate RDP streams cannot chain-flip to the end.
    // zh_CN: NoScrollPhase：鼠标滚轮 / Windows 触控板 WM_MOUSEWHEEL / RDP。
    // Windows 大部分精密触控板两指滚动 → WM_MOUSEWHEEL，全部 NoScrollPhase；
    // Mac RDP → Windows 时触控板同样映射为 WM_MOUSEWHEEL（angleDelta=±120，高频）。
    // 统一走 cluster 累积路径：阈值达成时首个事件立即翻页，同 cluster 后续事件被
    // consumed 拦截，避免 RDP 高频事件链式翻页到底。
    const bool animating = m_slideAnimation->state() == QAbstractAnimation::Running;

    int delta = (m_orientation == Qt::Horizontal)
                    ? event->angleDelta().x() + event->angleDelta().y()
                    : event->angleDelta().y();

    // ── Cluster accumulation ──
    // Event gaps mark cluster boundaries (gap > kClusterGapMs = new gesture).
    // Each cluster accumulates angleDelta; passing the threshold flips (or pends),
    // then the rest is consumed. During the animation cooldown events only update
    // the pending state and never start a new flip.
    // zh_CN: cluster 累积——用事件间隔检测边界（间隔 > kClusterGapMs 即新手势）；
    // 每个 cluster 累积 angleDelta，过阈值后翻页/挂起，再统一 consumed；
    // 动画期间（cooldown）事件只更新 pending，不触发新翻页。
    const qint64 sinceLast = m_wheelCooldown.isValid() ? m_wheelCooldown.elapsed() : 10000;
    m_wheelCooldown.start();

    // Gap above the threshold → a new cluster (new gesture). zh_CN: 间隔超过阈值 → 新 cluster（新手势）。
    if (sinceLast > kClusterGapMs) {
        m_npConsumed = false;
        m_npAccum = 0;
    }

    // While animating: only mark consumed so leftover events cannot flip after the
    // animation, and do not set m_pendingFlipDir. NoScrollPhase cannot distinguish a
    // new gesture from the same gesture delayed by RDP (network lag can exceed
    // kClusterGapMs and fake a new cluster); setting pending here would chain-flip.
    // The pending path is reserved for phase-based input with explicit ScrollBegin.
    // zh_CN: 动画播放中只标记 consumed，防止残余事件在动画结束后再翻页，且不设置
    // m_pendingFlipDir。NoScrollPhase 无法区分“新手势”与“同一手势被 RDP 延迟”
    // （网络延迟可造成 >kClusterGapMs 间隙被误判为新 cluster），设 pending 会链式翻页；
    // pending 机制仅保留给有明确 ScrollBegin 边界的 phase-based 路径。
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

    // Threshold reached → this cluster flips exactly once. zh_CN: 阈值达到 → 本 cluster 翻一次页。
    m_npConsumed = true;
    int dir = (m_npAccum > 0) ? -1 : 1;
    if (dir < 0) goPrevious(); else goNext();
    event->accept();
}

void FlipView::keyPressEvent(QKeyEvent* event)
{
    if (m_orientation == Qt::Horizontal) {
        if (event->key() == Qt::Key_Left) { goPrevious(); return; }
        if (event->key() == Qt::Key_Right) { goNext(); return; }
    } else {
        if (event->key() == Qt::Key_Up) { goPrevious(); return; }
        if (event->key() == Qt::Key_Down) { goNext(); return; }
    }
    QWidget::keyPressEvent(event);
}

} // namespace fluent::collections

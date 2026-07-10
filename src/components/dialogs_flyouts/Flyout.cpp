#include "Flyout.h"

#include "components/foundation/overlay/OverlayGeometry.h"

namespace fluent::dialogs_flyouts {

Flyout::Flyout(QWidget* parent) : Popup(parent) {
    // WinUI flyouts default to light-dismiss: non-modal, no dimming.
    // zh_CN: WinUI Flyout 默认 light-dismiss——非 modal、不变暗。
    setModal(false);
    setDim(false);
    setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));
}

Flyout::~Flyout() = default;

void Flyout::setPlacement(Placement p) {
    if (m_placement == p) return;
    m_placement = p;
    emit placementChanged(p);
}

void Flyout::setAnchor(QWidget* anchor) {
    m_anchor = anchor;
}

void Flyout::showAt(QWidget* anchor) {
    setAnchor(anchor);
    open();
}

// ── Geometry helpers. zh_CN: 几何辅助 ────────────────────────────────────────

QRect Flyout::anchorRectInTopLevel() const {
    if (!m_anchor) return QRect();
    QWidget* top = m_anchor->window();
    if (!top) return QRect();
    const QPoint tl = m_anchor->mapTo(top, QPoint(0, 0));
    return QRect(tl, m_anchor->size());
}

QPoint Flyout::clampCardPos(const QPoint& cardTopLeft) const {
    if (!m_clampToWindow) return cardTopLeft;
    QWidget* top = m_anchor ? m_anchor->window() : parentWidget();
    if (!top) return cardTopLeft;

    const QSize cardSize = ::fluent::overlay::visibleCardSize(size());

    const int margin = 4;  // Breathing room from the window edge. zh_CN: 与窗口边缘留点呼吸空间。
    return ::fluent::overlay::clampCardTopLeft(
        cardTopLeft, cardSize, ::fluent::overlay::overlaySurfaceRect(top), margin);
}

Flyout::Placement Flyout::resolveAutoPlacement() const {
    if (!m_anchor) return Bottom;
    QWidget* top = m_anchor->window();
    if (!top) return Bottom;

    const QRect a = anchorRectInTopLevel();
    const int cardH = ::fluent::overlay::visibleCardSize(size()).height();
    const int needed = cardH + m_anchorOffset;

    const QRect surface = ::fluent::overlay::overlaySurfaceRect(top);
    const int spaceBelow = surface.bottom() - a.bottom();
    const int spaceAbove = a.top() - surface.top();

    if (spaceBelow >= needed)            return Bottom;
    if (spaceAbove >= needed)            return Top;
    return spaceBelow >= spaceAbove ? Bottom : Top;
}

// ── Placement. zh_CN: 位置计算 ───────────────────────────────────────────────

QPoint Flyout::computePosition() const {
    // No anchor: fall back to the base class (centered). zh_CN: 没有 anchor → 退化到基类（居中）。
    if (!m_anchor || !m_anchor->window()) {
        return Popup::computePosition();
    }

    Placement p = m_placement;
    if (p == Auto) p = resolveAutoPlacement();

    if (p == Full) {
        return Popup::computePosition();
    }

    const QRect a = anchorRectInTopLevel();
    const QSize cardSize = ::fluent::overlay::visibleCardSize(size());
    const int cardW = cardSize.width();
    const int cardH = cardSize.height();

    // Card top-left in top-level coordinates. zh_CN: 卡片左上角（top-level 坐标）。
    QPoint card;
    switch (p) {
        case Top:
            card = QPoint(a.center().x() - cardW / 2,
                          a.top()    - m_anchorOffset - cardH);
            break;
        case Bottom:
            card = QPoint(a.center().x() - cardW / 2,
                          a.bottom() + m_anchorOffset);
            break;
        case Left:
            card = QPoint(a.left()  - m_anchorOffset - cardW,
                          a.center().y() - cardH / 2);
            break;
        case Right:
            card = QPoint(a.right() + m_anchorOffset,
                          a.center().y() - cardH / 2);
            break;
        case Full:
        case Auto:
            // Already handled / converted above. zh_CN: 已在前面处理/转换。
            return Popup::computePosition();
    }

    card = clampCardPos(card);

    return ::fluent::overlay::outerTopLeftForVisibleCard(card);
}

} // namespace fluent::dialogs_flyouts

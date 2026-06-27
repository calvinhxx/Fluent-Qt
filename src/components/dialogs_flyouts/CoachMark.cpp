#include "components/dialogs_flyouts/CoachMark.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPoint>
#include <QPolygon>
#include <QPolygonF>
#include <QPropertyAnimation>
#include <QRect>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"

namespace fluent::dialogs_flyouts {

namespace {

void refreshFluentDescendants(QWidget* root)
{
    if (!root)
        return;

    const auto children = root->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* child : children) {
        if (auto* fluentChild = dynamic_cast<FluentElement*>(child))
            fluentChild->onThemeUpdated();
        refreshFluentDescendants(child);
    }
}

constexpr int kTailSize = 9;     // tail triangle height. zh_CN: 尾巴三角高度。
constexpr int kTargetGap = 10;   // gap between target and card+tail. zh_CN: 目标与卡片+尾巴的间距。
}  // namespace

CoachMark::CoachMark(QWidget* owner, SurfaceMode surfaceMode)
    : QWidget(owner)
    , m_owner(owner)
    , m_surfaceMode(surfaceMode)
{
    setObjectName(QStringLiteral("CoachMark"));
    if (m_surfaceMode == TopLevelSurface) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::Tool | Qt::NoDropShadowWindowHint);
    } else {
        attachToOwnerTopLevel();
        setWindowFlags(Qt::Widget);
        m_opacityEffect = new QGraphicsOpacityEffect(this);
        m_opacityEffect->setOpacity(0.0);
        setGraphicsEffect(m_opacityEffect);
    }
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);

    m_contentHost = new QWidget(this);
    m_contentHost->setObjectName(QStringLiteral("CoachMarkContent"));

    m_fadeAnim = new QPropertyAnimation(this, "fadeOpacity", this);
    connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_open)
            hide();  // hide only after a close fade-out finishes
    });
    m_moveAnim = new QPropertyAnimation(this, "pos", this);

    // Size the window to the default card directly (setCardSize would no-op on the equal default).
    // zh_CN: 直接按默认卡片尺寸设置窗口大小(setCardSize 对相等的默认值会空操作)。
    const int margin = ::fluent::overlay::defaultShadowMargin();
    resize(m_cardSize.width() + 2 * margin, m_cardSize.height() + 2 * margin);
    m_contentHost->setGeometry(cardRect());
    onThemeUpdated();
}

CoachMark::~CoachMark()
{
    if (qApp)
        qApp->removeEventFilter(this);
}

void CoachMark::onThemeUpdated()
{
    update();
    refreshFluentDescendants(this);
}

void CoachMark::setCardSize(const QSize& size)
{
    if (m_cardSize == size)
        return;
    m_cardSize = size;
    const int margin = ::fluent::overlay::defaultShadowMargin();
    resize(size.width() + 2 * margin, size.height() + 2 * margin);
    m_contentHost->setGeometry(cardRect());  // keep host in sync even before the window is shown
    if (m_open)
        reposition(/*animated*/ false);
}

void CoachMark::setTarget(QWidget* target)
{
    if (m_target == target)
        return;
    m_target = target;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    if (m_open)
        reposition(/*animated*/ true);
}

void CoachMark::setPlacement(Placement placement)
{
    if (m_placement == placement)
        return;
    m_placement = placement;
    if (m_open)
        reposition(/*animated*/ true);
}

void CoachMark::open()
{
    if (m_open)
        return;
    m_open = true;
    attachToOwnerTopLevel();
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    reposition(/*animated*/ false);
    setFadeOpacity(0.0);
    show();
    raise();
    if (qApp)
        qApp->installEventFilter(this);
    m_fadeAnim->stop();
    m_fadeAnim->setDuration(themeAnimation().normal);
    m_fadeAnim->setEasingCurve(themeAnimation().decelerate);
    m_fadeAnim->setStartValue(0.0);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();
    emit openChanged(true);
    emit opened();
}

void CoachMark::close()
{
    if (!m_open)
        return;
    m_open = false;
    if (qApp)
        qApp->removeEventFilter(this);
    m_moveAnim->stop();
    m_fadeAnim->stop();
    m_fadeAnim->setDuration(themeAnimation().normal);
    m_fadeAnim->setEasingCurve(themeAnimation().exit);
    m_fadeAnim->setStartValue(fadeOpacity());
    m_fadeAnim->setEndValue(0.0);
    m_fadeAnim->start();  // the finished handler hides the window
    emit openChanged(false);
    emit closed();
}

void CoachMark::setOpen(bool open)
{
    if (open)
        this->open();
    else
        close();
}

bool CoachMark::eventFilter(QObject* watched, QEvent* event)
{
    if (!event || !m_open)
        return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::Destroy && watched == m_target) {
        close();
        return false;
    }

    QWidget* trackingAnchor = m_target ? m_target.data()
                                       : (m_owner ? m_owner : parentWidget());
    if (::fluent::overlay::anchorGeometryMayChange(watched, event, trackingAnchor))
        queueTargetSync();
    return QWidget::eventFilter(watched, event);
}

void CoachMark::queueTargetSync()
{
    if (m_targetSyncPending)
        return;
    m_targetSyncPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_targetSyncPending = false;
        syncToTarget();
    });
}

void CoachMark::syncToTarget()
{
    if (!m_open)
        return;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    if (m_target && !::fluent::overlay::isAnchorVisibleInTopLevel(m_target)) {
        close();
        return;
    }
    reposition(/*animated*/ false);
    raise();
}

QRect CoachMark::cardRect() const
{
    return ::fluent::overlay::visibleCardRect(rect());
}

bool CoachMark::syncThemeOverrideFromSource()
{
    QWidget* source = m_target ? m_target.data() : m_owner;
    return ::fluent::overlay::syncInheritedThemeOverride(this, source);
}

void CoachMark::attachToOwnerTopLevel()
{
    if (m_surfaceMode != SameWindowSurface)
        return;

    QWidget* top = m_owner ? m_owner->window() : parentWidget();
    if (!top)
        return;

    if (parentWidget() != top)
        setParent(top);
    setWindowFlags(Qt::Widget);
}

double CoachMark::fadeOpacity() const
{
    return m_opacityEffect ? m_opacityEffect->opacity() : windowOpacity();
}

void CoachMark::setFadeOpacity(double opacity)
{
    const double bounded = qBound(0.0, opacity, 1.0);
    if (m_opacityEffect) {
        m_opacityEffect->setOpacity(bounded);
        return;
    }
    setWindowOpacity(bounded);
}

void CoachMark::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_contentHost)
        m_contentHost->setGeometry(cardRect());
}

void CoachMark::reposition(bool animated)
{
    const int margin = ::fluent::overlay::defaultShadowMargin();
    const QSize win = size();
    QWidget* ownerWindow = m_owner ? m_owner->window() : parentWidget();

    if (!m_target) {
        m_tailVisible = false;
        const QRect ref = (m_surfaceMode == SameWindowSurface)
            ? (ownerWindow ? ownerWindow->rect() : QRect(QPoint(0, 0), win))
            : (ownerWindow ? ownerWindow->frameGeometry()
                           : (screen() ? screen()->availableGeometry() : QRect(QPoint(0, 0), win)));
        const QPoint centered(ref.center().x() - win.width() / 2, ref.center().y() - win.height() / 2);
        if (animated && isVisible()) {
            m_moveAnim->stop();
            m_moveAnim->setDuration(themeAnimation().normal);
            m_moveAnim->setEasingCurve(themeAnimation().decelerate);
            m_moveAnim->setStartValue(pos());
            m_moveAnim->setEndValue(centered);
            m_moveAnim->start();
        } else {
            move(centered);
        }
        update();
        return;
    }

    const QRect targetRef(
        m_surfaceMode == SameWindowSurface && ownerWindow
            ? m_target->mapTo(ownerWindow, QPoint(0, 0))
            : m_target->mapToGlobal(QPoint(0, 0)),
        m_target->size());
    const Placement p = (m_placement == Auto) ? Bottom : m_placement;

    QPoint topLeft;
    m_tailVisible = true;
    switch (p) {
    case Right:
        m_tailEdge = 3;  // tail on card LEFT edge, pointing left at the target
        topLeft = QPoint(targetRef.right() + kTargetGap + kTailSize - margin,
                         targetRef.center().y() - win.height() / 2);
        break;
    case Left:
        m_tailEdge = 4;  // tail on card RIGHT edge
        topLeft = QPoint(targetRef.left() - kTargetGap - kTailSize - (win.width() - margin),
                         targetRef.center().y() - win.height() / 2);
        break;
    case Top:
        m_tailEdge = 2;  // tail on card BOTTOM edge
        topLeft = QPoint(targetRef.center().x() - win.width() / 2,
                         targetRef.top() - kTargetGap - kTailSize - (win.height() - margin));
        break;
    default:  // Bottom
        m_tailEdge = 1;  // tail on card TOP edge, pointing up at the target
        topLeft = QPoint(targetRef.center().x() - win.width() / 2,
                         targetRef.bottom() + kTargetGap + kTailSize - margin);
        break;
    }

    // Keep the surface inside its owning coordinate space, then aim the tail relative to the final
    // position. zh_CN: 先把 surface 限制在所属坐标空间内，再按最终位置让尾巴对准目标。
    if (m_surfaceMode == SameWindowSurface && ownerWindow) {
        const QRect bounds = ownerWindow->rect();
        topLeft.setX(qBound(bounds.left(), topLeft.x(), bounds.right() - win.width()));
        topLeft.setY(qBound(bounds.top(), topLeft.y(), bounds.bottom() - win.height()));
    } else if (QScreen* sc = (m_owner ? m_owner->screen() : screen())) {
        const QRect avail = sc->availableGeometry();
        topLeft.setX(qBound(avail.left(), topLeft.x(), avail.right() - win.width()));
        topLeft.setY(qBound(avail.top(), topLeft.y(), avail.bottom() - win.height()));
    }

    const QRect card = ::fluent::overlay::visibleCardRect(QRect(QPoint(0, 0), win));
    const int r = themeRadius().overlay;
    if (m_tailEdge == 1 || m_tailEdge == 2) {
        const int localX = targetRef.center().x() - topLeft.x();
        m_tailCenter = qBound(card.left() + r + kTailSize, localX, card.right() - r - kTailSize);
    } else {
        const int localY = targetRef.center().y() - topLeft.y();
        m_tailCenter = qBound(card.top() + r + kTailSize, localY, card.bottom() - r - kTailSize);
    }

    if (animated && isVisible()) {
        m_moveAnim->stop();
        m_moveAnim->setDuration(themeAnimation().normal);
        m_moveAnim->setEasingCurve(themeAnimation().decelerate);
        m_moveAnim->setStartValue(pos());
        m_moveAnim->setEndValue(topLeft);
        m_moveAnim->start();
    } else {
        move(topLeft);
    }
    update();
}

void CoachMark::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect card = cardRect();
    const int r = themeRadius().overlay;
    ::fluent::overlay::paintLayeredShadow(painter, card, r, themeShadow(Elevation::High));

    QPainterPath path;
    path.addRoundedRect(card, r, r);
    if (m_tailVisible) {
        QPolygon tail;
        switch (m_tailEdge) {
        case 1:  // top edge, apex up
            tail << QPoint(m_tailCenter - kTailSize, card.top())
                 << QPoint(m_tailCenter + kTailSize, card.top())
                 << QPoint(m_tailCenter, card.top() - kTailSize);
            break;
        case 2:  // bottom edge, apex down
            tail << QPoint(m_tailCenter - kTailSize, card.bottom())
                 << QPoint(m_tailCenter + kTailSize, card.bottom())
                 << QPoint(m_tailCenter, card.bottom() + kTailSize);
            break;
        case 3:  // left edge, apex left
            tail << QPoint(card.left(), m_tailCenter - kTailSize)
                 << QPoint(card.left(), m_tailCenter + kTailSize)
                 << QPoint(card.left() - kTailSize, m_tailCenter);
            break;
        default:  // right edge, apex right
            tail << QPoint(card.right(), m_tailCenter - kTailSize)
                 << QPoint(card.right(), m_tailCenter + kTailSize)
                 << QPoint(card.right() + kTailSize, m_tailCenter);
            break;
        }
        QPainterPath tailPath;
        tailPath.addPolygon(QPolygonF(tail));
        path = path.united(tailPath);
    }

    const auto& colors = themeColors();
    painter.setBrush(colors.bgLayer);
    painter.setPen(colors.strokeDefault);
    painter.drawPath(path);
}

} // namespace fluent::dialogs_flyouts

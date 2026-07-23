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
#include <QTimer>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/overlay/OverlayWindow.h"

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

constexpr int kTailSize = 9;
constexpr int kTargetGap = 10;

} // namespace

CoachMark::CoachMark(QWidget* owner, SurfaceMode /*surfaceMode*/)
    : QWidget(owner)
    , m_owner(owner)
{
    setObjectName(QStringLiteral("CoachMark"));
    attachToOwnerTopLevel();
    setWindowFlags(Qt::Widget);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);

    m_contentHost = new QWidget(this);
    m_contentHost->setObjectName(QStringLiteral("CoachMarkContent"));

    m_fadeAnim = new QPropertyAnimation(this, "fadeOpacity", this);
    connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_open)
            hide();
    });
    m_moveAnim = new QPropertyAnimation(this, "pos", this);

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
    m_contentHost->setGeometry(cardRect());
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
    if (m_open) {
        reposition(/*animated*/ true);
        raise();
    }
}

void CoachMark::setPlacement(Placement placement)
{
    if (m_placement == placement)
        return;
    m_placement = placement;
    if (m_open) {
        reposition(/*animated*/ true);
        raise();
    }
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
    m_fadeAnim->start();
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
    QWidget* top = m_owner ? m_owner->window() : parentWidget();
    if (!top)
        return;
    ::fluent::overlay::attachToTopLevel(this, top);
}

double CoachMark::fadeOpacity() const
{
    return m_opacityEffect ? m_opacityEffect->opacity() : 1.0;
}

void CoachMark::setFadeOpacity(double opacity)
{
    if (m_opacityEffect)
        m_opacityEffect->setOpacity(qBound(0.0, opacity, 1.0));
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
        const QRect ref = ownerWindow ? ::fluent::overlay::overlaySurfaceRect(ownerWindow)
                                      : QRect(QPoint(0, 0), win);
        const QPoint centered(ref.center().x() - win.width() / 2,
                              ref.center().y() - win.height() / 2);
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

    const QRect targetRef(m_target->mapTo(ownerWindow, QPoint(0, 0)), m_target->size());
    const Placement p = (m_placement == Auto) ? Bottom : m_placement;

    QPoint topLeft;
    m_tailVisible = true;
    switch (p) {
    case Right:
        m_tailEdge = 3;
        topLeft = QPoint(targetRef.right() + kTargetGap + kTailSize - margin,
                         targetRef.center().y() - win.height() / 2);
        break;
    case Left:
        m_tailEdge = 4;
        topLeft = QPoint(targetRef.left() - kTargetGap - kTailSize - (win.width() - margin),
                         targetRef.center().y() - win.height() / 2);
        break;
    case Top:
        m_tailEdge = 2;
        topLeft = QPoint(targetRef.center().x() - win.width() / 2,
                         targetRef.top() - kTargetGap - kTailSize - (win.height() - margin));
        break;
    default:
        m_tailEdge = 1;
        topLeft = QPoint(targetRef.center().x() - win.width() / 2,
                         targetRef.bottom() + kTargetGap + kTailSize - margin);
        break;
    }

    if (ownerWindow) {
        const QRect bounds = ::fluent::overlay::overlaySurfaceRect(ownerWindow);
        topLeft.setX(qBound(bounds.left(), topLeft.x(), bounds.right() - win.width()));
        topLeft.setY(qBound(bounds.top(), topLeft.y(), bounds.bottom() - win.height()));
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
        case 1:
            tail << QPoint(m_tailCenter - kTailSize, card.top())
                 << QPoint(m_tailCenter + kTailSize, card.top())
                 << QPoint(m_tailCenter, card.top() - kTailSize);
            break;
        case 2:
            tail << QPoint(m_tailCenter - kTailSize, card.bottom())
                 << QPoint(m_tailCenter + kTailSize, card.bottom())
                 << QPoint(m_tailCenter, card.bottom() + kTailSize);
            break;
        case 3:
            tail << QPoint(card.left(), m_tailCenter - kTailSize)
                 << QPoint(card.left(), m_tailCenter + kTailSize)
                 << QPoint(card.left() - kTailSize, m_tailCenter);
            break;
        default:
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
    const DesignLanguage lang = themeDesignLanguage();
    QPen outlinePen;
    if (lang == DesignMaterial)
        outlinePen = QPen(Qt::NoPen);
    else if (lang == DesignCupertino)
        outlinePen = QPen(colors.strokeStrong, 1);
    else
        outlinePen = QPen(colors.strokeDefault);

    painter.setBrush(colors.bgLayer);
    painter.setPen(outlinePen);
    painter.drawPath(path);
}

} // namespace fluent::dialogs_flyouts

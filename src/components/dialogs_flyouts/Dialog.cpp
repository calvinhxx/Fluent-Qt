#include "Dialog.h"

#include <QGraphicsOpacityEffect>
#include <QHideEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>

#include "compatibility/QtCompat.h"
#include "components/foundation/overlay/OverlayCoordinator.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/overlay/OverlayWindow.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "design/Material.h"

namespace fluent::dialogs_flyouts {

namespace {

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

Dialog::Dialog(QWidget* parent)
    : QDialog(parent),
      m_originalParent(parent)
{
    m_overlayCoordinator =
        new ::fluent::overlay::OverlayCoordinator(this, this);
    connect(m_overlayCoordinator,
            &::fluent::overlay::OverlayCoordinator::hostGeometryChanged,
            this,
            [this]() {
                if (!isVisible())
                    return;
                positionInOwner();
                if (auto* smoke = m_overlayCoordinator->scrim()) {
                    smoke->setSurfaceRadius(qRound(
                        ::fluent::overlay::overlaySurfaceRadius(
                            m_overlayCoordinator->topLevelWidget())));
                }
                m_overlayCoordinator->raiseStack();
            });

    // Same-window overlay contract (WinUI ContentDialog / Popup / DrawerView): stay a Qt::Widget
    // child of the owning top-level. Never host Dialog as a native top-level window.
    // zh_CN: 同窗口浮层契约（WinUI ContentDialog / Popup / DrawerView）：保持为 owning top-level 的
    // Qt::Widget 子控件，绝不把 Dialog 做成原生顶层窗口。
    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint | Qt::CustomizeWindowHint
                   | Qt::NoDropShadowWindowHint);
    attachToOwner();

    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setContentsMargins(m_shadowSize, m_shadowSize, m_shadowSize, m_shadowSize);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);

    m_animation = new QPropertyAnimation(this, "animationProgress", this);
    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (m_isClosing) {
            m_isClosing = false;
            m_isAnimating = false;
            const QSize targetSize = m_targetSize;
            m_targetSize = QSize();
            QDialog::done(m_closingResult);
            if (!targetSize.isEmpty())
                resize(targetSize);
            setMinimumSize(m_savedMinSize);
            setMaximumSize(m_savedMaxSize);
            setSurfaceOpacity(1.0);
        } else {
            resize(m_targetSize);
            setMinimumSize(m_savedMinSize);
            setMaximumSize(m_savedMaxSize);
            m_isAnimating = false;
            m_targetSize = QSize();
        }
    });

    onThemeUpdated();
}

Dialog::~Dialog()
{
    if (m_smokeAnim) {
        m_smokeAnim->stop();
        m_smokeAnim->setTargetObject(nullptr);
    }
    if (m_overlayCoordinator->scrim()) {
        m_overlayCoordinator->scrim()->removeEventFilter(this);
        m_overlayCoordinator->releaseScrim(
            ::fluent::overlay::OverlayCoordinator::ScrimDeletion::Immediate);
    }
}

QWidget* Dialog::ownerWidget() const
{
    if (QWidget* owner =
            ::fluent::overlay::resolveOwningTopLevel(m_originalParent,
                                                     parentWidget())) {
        return owner;
    }
    return m_overlayCoordinator->topLevelWidget();
}

void Dialog::attachToOwner()
{
    QWidget* top = ownerWidget();
    if (top)
        m_overlayCoordinator->attachTo(top);
}

void Dialog::setAnimationProgress(double progress)
{
    m_animationProgress = progress;
    setSurfaceOpacity(progress);
    update();
}

void Dialog::setThemeSource(QWidget* source)
{
    if (m_themeSource == source)
        return;
    m_themeSource = source;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
}

bool Dialog::syncThemeOverrideFromSource()
{
    QWidget* source = m_themeSource ? m_themeSource.data() : ownerWidget();
    return ::fluent::overlay::syncInheritedThemeOverride(this, source);
}

void Dialog::open()
{
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    attachToOwner();
    prepareSurfaceSize();
    if (m_smokeEnabled)
        showSmokeOverlay();

    if (m_animationEnabled && !isVisible()) {
        m_isAnimating = true;
        m_animationProgress = 0.0;
        setSurfaceOpacity(0.0);
    } else {
        setSurfaceOpacity(1.0);
    }

    // Prefer show() over QDialog::open(): the latter's WindowModal path can briefly hide a
    // same-window child and tear down the smoke scrim via hideEvent before the dialog is visible.
    // zh_CN: 优先用 show() 而非 QDialog::open()：后者的 WindowModal 路径会短暂隐藏同窗口子控件，
    // 并通过 hideEvent 在对话框可见前拆掉烟雾遮罩。
    if (windowModality() == Qt::NonModal)
        setWindowModality(Qt::WindowModal);
    QDialog::show();

    if (m_smokeEnabled)
        showSmokeOverlay();
    m_overlayCoordinator->raiseStack();
    setFocus(Qt::ActiveWindowFocusReason);
}

int Dialog::exec()
{
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    attachToOwner();
    prepareSurfaceSize();
    if (m_smokeEnabled)
        showSmokeOverlay();

    if (m_animationEnabled && !isVisible()) {
        m_isAnimating = true;
        m_animationProgress = 0.0;
        setSurfaceOpacity(0.0);
    } else {
        setSurfaceOpacity(1.0);
    }

    const int result = QDialog::exec();
    hideSmokeOverlay();
    return result;
}

void Dialog::showEvent(QShowEvent* event)
{
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    attachToOwner();
    positionInOwner();
    QDialog::showEvent(event);
    m_overlayCoordinator->raiseStack();

    if (!m_animationEnabled || !m_isAnimating) {
        m_animationProgress = 1.0;
        m_isAnimating = false;
        setSurfaceOpacity(1.0);
        return;
    }

    m_isClosing = false;
    ensurePolished();
    for (auto* child : findChildren<QWidget*>()) {
        child->ensurePolished();
        if (auto* fluentChild = dynamic_cast<FluentElement*>(child))
            fluentChild->onThemeUpdated();
        if (child->layout())
            child->layout()->activate();
    }
    if (layout())
        layout()->activate();

    m_targetSize = size();
    m_savedMinSize = minimumSize();
    m_savedMaxSize = maximumSize();

    const auto& anim = themeAnimation();
    m_animation->stop();
    m_animation->setDuration(anim.normal);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(anim.entrance);
    m_animation->start();
}

void Dialog::hideEvent(QHideEvent* event)
{
    hideSmokeOverlay();
    QDialog::hideEvent(event);
}

bool Dialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_overlayCoordinator->scrim() && event) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseButtonRelease:
        case QEvent::Wheel:
            if (isVisible()) {
                raise();
                setFocus(Qt::MouseFocusReason);
            }
            event->accept();
            return true;
        default:
            break;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void Dialog::prepareSurfaceSize()
{
    ensurePolished();
    for (auto* child : findChildren<QWidget*>())
        child->ensurePolished();
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }

    const int collapsedHeight = m_shadowSize * 2 + 48;
    if (height() > collapsedHeight)
        return;

    QSize preferred = sizeHint();
    if (!minimumSizeHint().isEmpty())
        preferred = preferred.expandedTo(minimumSizeHint());
    preferred = preferred.expandedTo(minimumSize());
    if (!preferred.isValid() || preferred.isEmpty())
        return;

    QSize next(qMax(width(), preferred.width()), qMax(height(), preferred.height()));
    next = next.expandedTo(minimumSize()).boundedTo(maximumSize());
    if (next != size())
        resize(next);
}

void Dialog::positionInOwner()
{
    QWidget* owner = ownerWidget();
    if (!owner)
        return;

    QRect surface = ::fluent::overlay::overlaySurfaceRect(owner);
    if (surface.isEmpty())
        surface = owner->rect();

    const QRect current(pos(), size());
    const bool defaultPlacement = pos().isNull();
    const bool outsideSurface = !surface.contains(current);
    QPoint next = pos();
    if (m_smokeEnabled || defaultPlacement || outsideSurface) {
        next = QPoint(surface.left() + (surface.width() - width()) / 2,
                      surface.top() + (surface.height() - height()) / 2);
    }

    if (width() <= surface.width())
        next.setX(qBound(surface.left(), next.x(), surface.right() - width() + 1));
    else
        next.setX(surface.left());
    if (height() <= surface.height())
        next.setY(qBound(surface.top(), next.y(), surface.bottom() - height() + 1));
    else
        next.setY(surface.top());

    if (next != pos())
        move(next);
}

void Dialog::setSurfaceOpacity(qreal opacity)
{
    if (m_opacityEffect)
        m_opacityEffect->setOpacity(qBound<qreal>(0.0, opacity, 1.0));
}

void Dialog::done(int result)
{
    if (!m_animationEnabled) {
        setSurfaceOpacity(0.0);
        QDialog::done(result);
        setSurfaceOpacity(1.0);
        return;
    }

    if (m_isClosing)
        return;
    m_isClosing = true;
    m_closingResult = result;
    m_animation->stop();

    if (!m_isAnimating) {
        m_targetSize = size();
        m_savedMinSize = minimumSize();
        m_savedMaxSize = maximumSize();
        setMinimumSize(QSize(0, 0));
        setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
        m_isAnimating = true;
        m_animationProgress = 1.0;
    }

    const auto& anim = themeAnimation();
    m_animation->setDuration(anim.normal);
    m_animation->setStartValue(m_animationProgress);
    m_animation->setEndValue(0.0);
    m_animation->setEasingCurve(anim.exit);
    m_animation->start();
}

void Dialog::mousePressEvent(QMouseEvent* event)
{
    if (m_dragEnabled && event->button() == Qt::LeftButton) {
        m_dragOffset = fluentMousePos(event);
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
    QDialog::mousePressEvent(event);
}

void Dialog::mouseMoveEvent(QMouseEvent* event)
{
    if (cursor().shape() == Qt::ClosedHandCursor && parentWidget()) {
        const QPoint parentPos = parentWidget()->mapFromGlobal(fluentMouseGlobalPos(event));
        move(parentPos - m_dragOffset);
        event->accept();
    }
    QDialog::mouseMoveEvent(event);
}

void Dialog::mouseReleaseEvent(QMouseEvent* event)
{
    if (cursor().shape() == Qt::ClosedHandCursor)
        unsetCursor();
    QDialog::mouseReleaseEvent(event);
}

void Dialog::showSmokeOverlay()
{
    QWidget* owner = ownerWidget();
    if (!owner || !owner->isVisible())
        return;

    const int surfaceRadius = qRound(::fluent::overlay::overlaySurfaceRadius(owner));
    m_overlayCoordinator->attachTo(owner);
    const bool creating = !m_overlayCoordinator->scrim();
    auto* smoke = m_overlayCoordinator->ensureScrim(
        QStringLiteral("DialogSmokeScrim"));
    if (!smoke)
        return;
    if (creating) {
        const auto& smokeToken = themeSmoke();
        QColor color = smokeToken.baseColor;
        color.setAlphaF(smokeToken.opacity);
        m_overlayCoordinator->scrim()->setColor(color);
        m_overlayCoordinator->scrim()->setProgress(0.0);
        m_overlayCoordinator->scrim()->installEventFilter(this);
    } else {
        ::fluent::overlay::attachToTopLevel(smoke, owner);
    }
    smoke->setSurfaceRadius(surfaceRadius);
    smoke->setModalAndDim(true, true);
    smoke->show();

    if (!m_smokeAnim) {
        m_smokeAnim = new QPropertyAnimation(this);
        m_smokeAnim->setPropertyName("progress");
    }
    m_smokeAnim->stop();
    m_smokeAnim->setTargetObject(smoke);
    const auto& anim = themeAnimation();
    m_smokeAnim->setDuration(anim.normal);
    m_smokeAnim->setStartValue(smoke->progress());
    m_smokeAnim->setEndValue(1.0);
    m_smokeAnim->setEasingCurve(anim.entrance);
    m_smokeAnim->start();

    m_overlayCoordinator->raiseStack();
}

void Dialog::hideSmokeOverlay()
{
    auto* overlay = m_overlayCoordinator->scrim();
    if (!overlay)
        return;

    if (m_smokeAnim) {
        m_smokeAnim->stop();
        m_smokeAnim->setTargetObject(nullptr);
    }

    QWidget* owner = overlay->parentWidget();
    const QRect dirtyRect = overlay->geometry();

    overlay->removeEventFilter(this);
    overlay->setModalAndDim(false, false);
    m_overlayCoordinator->releaseScrim(
        ::fluent::overlay::OverlayCoordinator::ScrimDeletion::Immediate);

    if (owner)
        owner->update(dirtyRect);
}

void Dialog::onThemeUpdated()
{
    update();
    refreshFluentDescendants(this);
    if (auto* smokeOverlay = m_overlayCoordinator->scrim()) {
        const auto& smoke = themeSmoke();
        QColor color = smoke.baseColor;
        color.setAlphaF(smoke.opacity);
        smokeOverlay->setColor(color);
    }
}

void Dialog::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect contentRect = rect().adjusted(m_shadowSize, m_shadowSize, -m_shadowSize, -m_shadowSize);
    drawShadow(painter, contentRect);

    const auto& colors = themeColorsRef();
    const DesignLanguage lang = themeDesignLanguage();

    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.radius = themeRadius().overlay;
    if (lang == DesignMaterial)
        surface.border = Qt::transparent;
    else if (lang == DesignCupertino)
        surface.border = colors.strokeStrong;
    else
        surface.border = colors.strokeDefault;
    fluent::painting::paintRoundedSurface(painter, QRectF(contentRect), surface);
}

void Dialog::drawShadow(QPainter& painter, const QRect& contentRect)
{
    // Medium elevation: High stacked on smoke reads as a heavy halo.
    // zh_CN: 用 Medium；High 叠在烟雾上会形成过重暗晕。
    ::fluent::overlay::paintLayeredShadow(painter, contentRect, themeRadius().overlay,
                                          themeShadow(Elevation::Medium));
}

} // namespace fluent::dialogs_flyouts

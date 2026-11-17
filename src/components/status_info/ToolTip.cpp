#include "ToolTip.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "components/status_info/private/ToolTipAccessibility_p.h"
#include "components/textfields/Label.h"
#include "design/Elevation.h"
#include "design/Typography.h"
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QPainter>
#include <QPalette>
#include <QPropertyAnimation>
#include <QScreen>
#include <QVBoxLayout>

namespace fluent::status_info {

using namespace fluent::textfields;

namespace {
constexpr qreal kHiddenOpacity = 0.0;
constexpr qreal kVisibleOpacity = 1.0;
constexpr qreal kOpacityEpsilon = 0.001;

// Transparent inset around the bubble so the layered shadow has room to spread
// (paintLayeredShadow grows ~11px); the bubble is drawn inside this margin.
// zh_CN: 气泡四周的透明内缩，给分层阴影留出扩散空间（约 11px），气泡绘制在该边距内部。
constexpr int kShadowMargin = 12;
constexpr int kTargetGap = 4;
constexpr char kAttachedToolTipName[] = "FluentAttachedToolTip";

QGraphicsOpacityEffect* opacityEffectFor(const QWidget* widget)
{
    return widget ? qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect()) : nullptr;
}

qreal visualOpacity(const QWidget* widget)
{
    const auto* effect = opacityEffectFor(widget);
    return effect ? effect->opacity() : kVisibleOpacity;
}

void setVisualOpacity(QWidget* widget, qreal opacity)
{
    if (auto* effect = opacityEffectFor(widget))
        effect->setOpacity(opacity);
}
} // namespace

ToolTip::ToolTip(QWidget* parent) : QWidget(parent)
{
    detail::ensureToolTipAccessibilityFactory();
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    ::compatibility::detail::prepareNativeWidgetPopup(this);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // Keep the top-level window opaque and fade the widget subtree via QGraphicsOpacityEffect
    // for Wayland/WSLg and headless QPA backends without native window opacity.
    auto* opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(kHiddenOpacity);
    setGraphicsEffect(opacityEffect);

    m_textBlock = new Label(this);

    QFont f = m_textBlock->font();
    f.setBold(false);
    f.setPixelSize(Typography::FontSize::Caption);
    m_textBlock->setFont(f);
    m_textBlock->setAlignment(Qt::AlignCenter);

    // Figma Tooltip padding: 8 horizontal, 5 top / 7 bottom (optically centers caption text).
    m_margins = QMargins(8, 5, 8, 7);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_textBlock);
    setLayout(layout);
    applyLayoutMargins();

    // Initialize colors and label style sheet immediately, even before a theme source is attached.
    onThemeUpdated();
}

ToolTip* ToolTip::attach(QWidget* target, const QString& text, Placement placement)
{
    if (!target)
        return nullptr;

    auto* toolTip = target->findChild<ToolTip*>(QString::fromLatin1(kAttachedToolTipName), Qt::FindDirectChildrenOnly);
    if (!toolTip) {
        toolTip = new ToolTip(target);
        toolTip->setObjectName(QString::fromLatin1(kAttachedToolTipName));
    }
    toolTip->setText(text);
    toolTip->setTarget(target, placement);
    return toolTip;
}

void ToolTip::setText(const QString& text)
{
    const bool changed = m_textBlock->text() != text;
    m_textBlock->setText(text);
    if (m_target)
        m_target->setToolTip(text);
    adjustSize();
    if (changed)
        detail::notifyToolTipAccessibilityTextChanged(this);
}

QString ToolTip::text() const
{
    return m_textBlock->text();
}

void ToolTip::setMargins(const QMargins& margins)
{
    if (m_margins != margins) {
        m_margins = margins;
        applyLayoutMargins();
        adjustSize();
        emit marginsChanged();
    }
}

int ToolTip::shadowMargin() const
{
    return kShadowMargin;
}

void ToolTip::applyLayoutMargins()
{
    // The layout margins fold the shadow inset together with the content padding so the
    // label sits inside the bubble and the bubble sits inside the transparent shadow band.
    // zh_CN: 布局边距把阴影内缩与内容内边距合并，使标签位于气泡内、气泡位于透明阴影带内。
    if (auto* l = layout())
        l->setContentsMargins(m_margins + fluent::overlay::uniformShadowMargins(kShadowMargin));
}

void ToolTip::setFont(const QFont& font)
{
    QWidget::setFont(font);
    if (m_textBlock) {
        m_textBlock->setFont(font);
    }
    adjustSize();
}

void ToolTip::setThemeSource(QWidget* source)
{
    if (m_themeSource == source)
        return;
    m_themeSource = source;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
}

void ToolTip::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled)
        return;

    m_animationEnabled = enabled;
    if (!m_animationEnabled) {
        if (m_opacityAnimation) {
            m_opacityAnimation->stop();
        }
        if (m_hideOnAnimationFinished) {
            finishHideAnimation();
        } else {
            setVisualOpacity(this, isVisible() ? kVisibleOpacity : kHiddenOpacity);
        }
    }

    emit animationEnabledChanged(m_animationEnabled);
}

void ToolTip::setVisible(bool visible)
{
    if (qApp) {
        if (visible && m_target)
            qApp->installEventFilter(this);
        else
            qApp->removeEventFilter(this);
    }
    if (m_accessibilityVisible != visible) {
        m_accessibilityVisible = visible;
        detail::notifyToolTipAccessibilityVisibilityChanged(this);
    }
    if (visible && syncThemeOverrideFromSource())
        onThemeUpdated();

    if (!m_animationEnabled) {
        if (m_opacityAnimation) {
            m_opacityAnimation->stop();
        }
        m_hideOnAnimationFinished = false;
        setVisualOpacity(this, visible ? kVisibleOpacity : kHiddenOpacity);
        QWidget::setVisible(visible);
        return;
    }

    if (visible) {
        startShowAnimation();
    } else {
        startHideAnimation();
    }
}

void ToolTip::onThemeUpdated()
{
    const auto& c = themeColorsRef();
    m_bgColor = c.bgLayer; // Figma tooltip surface is near-white (#FCFCFC); bgLayer matches in both themes.
    m_borderColor = c.strokeDivider;
    m_textColor = isEnabled() ? c.textPrimary : c.textDisabled;
    // Color via the label's own style sheet so an ancestor style sheet does not override QPalette.
    m_textBlock->setStyleSheet(QStringLiteral("color: rgba(%1, %2, %3, %4); background: transparent;")
            .arg(m_textColor.red())
            .arg(m_textColor.green())
            .arg(m_textColor.blue())
            .arg(m_textColor.alpha()));
    update();
}

bool ToolTip::eventFilter(QObject* watched, QEvent* event)
{
    if (isVisible() && ::fluent::overlay::anchorGeometryMayChange(watched, event, m_target)) {
        if (::fluent::overlay::isAnchorVisibleInTopLevel(m_target))
            positionForTarget();
        else
            hide();
    }
    if (watched != m_target || !event)
        return QWidget::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::ToolTip:
        if (!text().isEmpty() && m_target && m_target->isEnabled()) {
            if (syncThemeOverrideFromSource())
                onThemeUpdated();
            positionForTarget();
            show();
            raise();
        }
        return true;
    case QEvent::Leave:
    case QEvent::Hide:
    case QEvent::MouseButtonPress:
    case QEvent::WindowDeactivate:
        hide();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void ToolTip::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange)
        onThemeUpdated();
}

void ToolTip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto& r = themeRadius();
    const QRect bubble = fluent::overlay::visibleCardRect(rect(), kShadowMargin);

    // Subtle shadow intensity avoids heavy dark halos around the compact bubble.
    fluent::overlay::paintLayeredShadow(p, bubble, r.control, themeShadow(Elevation::Medium), 0.18);

    // Figma Tooltip uses 4px control radius rather than 8px overlay radius.
    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = m_bgColor;
    surface.border = m_borderColor;
    surface.radius = r.control;
    fluent::painting::paintRoundedSurface(p, QRectF(bubble), surface);
}

void ToolTip::setTarget(QWidget* target, Placement placement)
{
    if (m_target == target && m_placement == placement)
        return;
    QWidget* previousTarget = m_target.data();
    if (m_target)
        m_target->removeEventFilter(this);
    m_target = target;
    m_placement = placement;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    if (m_target) {
        m_target->setToolTip(text());
        m_target->installEventFilter(this);
    }
    if (previousTarget != m_target.data())
        detail::notifyToolTipAccessibilityTargetChanged(this);
}

bool ToolTip::syncThemeOverrideFromSource()
{
    QWidget* source = m_themeSource ? m_themeSource.data() : m_target ? m_target.data() : parentWidget();
    return ::fluent::overlay::syncInheritedThemeOverride(this, source);
}

void ToolTip::positionForTarget()
{
    if (!m_target)
        return;

    adjustSize();
    const QSize outerSize = sizeHint().expandedTo(size());
    const QSize cardSize(qMax(0, outerSize.width() - 2 * kShadowMargin),
        qMax(0, outerSize.height() - 2 * kShadowMargin));
    const QRect targetRect = ::fluent::overlay::presentedRectInTopLevel(m_target).translated(
        ::fluent::overlay::presentedTopLevel(m_target)->mapToGlobal(QPoint()));

    auto cardTopLeft = [&](Placement placement) {
        switch (placement) {
        case Below:
            return QPoint(targetRect.center().x() - cardSize.width() / 2, targetRect.bottom() + 1 + kTargetGap);
        case Left:
            return QPoint(targetRect.left() - kTargetGap - cardSize.width(),
                targetRect.center().y() - cardSize.height() / 2);
        case Right:
            return QPoint(targetRect.right() + 1 + kTargetGap, targetRect.center().y() - cardSize.height() / 2);
        case Above:
        default:
            return QPoint(targetRect.center().x() - cardSize.width() / 2,
                targetRect.top() - kTargetGap - cardSize.height());
        }
    };

    QPoint visibleTopLeft = cardTopLeft(m_placement);
    QScreen* screen = QGuiApplication::screenAt(targetRect.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        const QRect preferred(visibleTopLeft, cardSize);
        if (!available.contains(preferred)) {
            Placement fallback = m_placement;
            if (m_placement == Above)
                fallback = Below;
            else if (m_placement == Below)
                fallback = Above;
            else if (m_placement == Left)
                fallback = Right;
            else
                fallback = Left;
            const QPoint fallbackTopLeft = cardTopLeft(fallback);
            if (available.contains(QRect(fallbackTopLeft, cardSize)))
                visibleTopLeft = fallbackTopLeft;
        }
        visibleTopLeft.setX(qBound(available.left(), visibleTopLeft.x(),
            qMax(available.left(), available.right() - cardSize.width() + 1)));
        visibleTopLeft.setY(qBound(available.top(), visibleTopLeft.y(),
            qMax(available.top(), available.bottom() - cardSize.height() + 1)));
    }
    move(visibleTopLeft - QPoint(kShadowMargin, kShadowMargin));
}

void ToolTip::ensureOpacityAnimation()
{
    if (m_opacityAnimation)
        return;

    auto* opacityEffect = opacityEffectFor(this);
    if (!opacityEffect)
        return;

    m_opacityAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
    connect(m_opacityAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_hideOnAnimationFinished) {
            finishHideAnimation();
            return;
        }
        setVisualOpacity(this, kVisibleOpacity);
    });
}

void ToolTip::startShowAnimation()
{
    ensureOpacityAnimation();

    m_hideOnAnimationFinished = false;
    if (!m_opacityAnimation) {
        QWidget::setVisible(true);
        return;
    }
    m_opacityAnimation->stop();

    const qreal startOpacity = isVisible() ? visualOpacity(this) : kHiddenOpacity;
    setVisualOpacity(this, startOpacity);
    QWidget::setVisible(true);

    if (startOpacity >= kVisibleOpacity - kOpacityEpsilon) {
        setVisualOpacity(this, kVisibleOpacity);
        return;
    }

    const auto anim = themeAnimation();
    m_opacityAnimation->setDuration(anim.fast);
    m_opacityAnimation->setEasingCurve(anim.decelerate);
    m_opacityAnimation->setStartValue(startOpacity);
    m_opacityAnimation->setEndValue(kVisibleOpacity);
    ::fluent::detail::startMotionTransition(m_opacityAnimation, anim.fast, m_animationEnabled);
}

void ToolTip::startHideAnimation()
{
    if (!isVisible()) {
        if (m_opacityAnimation) {
            m_opacityAnimation->stop();
        }
        m_hideOnAnimationFinished = false;
        setVisualOpacity(this, kHiddenOpacity);
        return;
    }

    ensureOpacityAnimation();
    if (!m_opacityAnimation) {
        QWidget::setVisible(false);
        return;
    }

    m_hideOnAnimationFinished = true;
    m_opacityAnimation->stop();

    const qreal startOpacity = visualOpacity(this);
    if (startOpacity <= kHiddenOpacity + kOpacityEpsilon) {
        finishHideAnimation();
        return;
    }

    const auto anim = themeAnimation();
    m_opacityAnimation->setDuration(anim.fast);
    m_opacityAnimation->setEasingCurve(anim.accelerate);
    m_opacityAnimation->setStartValue(startOpacity);
    m_opacityAnimation->setEndValue(kHiddenOpacity);
    ::fluent::detail::startMotionTransition(m_opacityAnimation, anim.fast, m_animationEnabled);
}

void ToolTip::finishHideAnimation()
{
    m_hideOnAnimationFinished = false;
    setVisualOpacity(this, kHiddenOpacity);
    QWidget::setVisible(false);
}

} // namespace fluent::status_info

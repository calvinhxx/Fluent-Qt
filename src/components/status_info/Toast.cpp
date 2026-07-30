#include "components/status_info/Toast.h"

#include <QAbstractAnimation>
#include <QAccessible>
#include <QAction>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QVector>
#include <algorithm>

#include "components/basicinput/Button.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/overlay/OverlayCoordinator.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"
#include "design/Elevation.h"
#include "design/Typography.h"

namespace fluent::status_info {
namespace {

constexpr char kManagedToastProperty[] = "_fluentManagedToast";
constexpr char kStackOrderProperty[] = "_fluentToastStackOrder";
constexpr int kShadowMargin = overlay::defaultShadowMargin();
constexpr int kSlideDistance = 10;
constexpr int kCornerRadius = 10;
constexpr int kIconSize = Typography::IconSize::Standard;
constexpr int kMaximumCardWidth = 420;
constexpr int kMaximumTextWidth = 360;
constexpr int kStackGap = 8;
constexpr int kToastShadowLayers = 4;
constexpr qreal kToastShadowOpacityScale = 0.08;

int g_maximumVisible = 3;
quint64 g_stackOrder = 0;

QMargins normalizedMargins(const QMargins& margins)
{
    return QMargins(
        qMax(0, margins.left()),
        qMax(0, margins.top()),
        qMax(0, margins.right()),
        qMax(0, margins.bottom()));
}

QString actionCaption(const QAction* action)
{
    if (!action)
        return {};

    const QString source =
        action->iconText().isEmpty()
        ? action->text()
        : action->iconText();
    QString caption;
    caption.reserve(source.size());
    for (int i = 0; i < source.size(); ++i) {
        if (source.at(i) != QLatin1Char('&')) {
            caption.append(source.at(i));
            continue;
        }
        if (i + 1 < source.size()
            && source.at(i + 1) == QLatin1Char('&')) {
            caption.append(QLatin1Char('&'));
            ++i;
        }
    }
    return caption;
}

} // namespace

QVector<Toast*> Toast::openToastsFor(QWidget* host, Placement placement)
{
    QVector<Toast*> open;
    if (!host)
        return open;

    const auto children =
        host->findChildren<Toast*>(QString(), Qt::FindDirectChildrenOnly);
    open.reserve(children.size());
    for (Toast* toast : children) {
        if (toast && toast->isOpen() && toast->placement() == placement)
            open.append(toast);
    }
    std::sort(open.begin(), open.end(), [](Toast* left, Toast* right) {
        return left->property(kStackOrderProperty).toULongLong()
            < right->property(kStackOrderProperty).toULongLong();
    });
    return open;
}

QVector<Toast*> Toast::managedOpenToastsFor(
    QWidget* host, Placement placement)
{
    QVector<Toast*> managed;
    for (Toast* toast : openToastsFor(host, placement)) {
        if (toast->property(kManagedToastProperty).toBool())
            managed.append(toast);
    }
    return managed;
}

void Toast::relayoutHostStack(QWidget* host, Placement placement)
{
    for (Toast* toast : openToastsFor(host, placement))
        toast->syncGeometry();
}

int Toast::maximumVisible()
{
    return g_maximumVisible;
}

void Toast::setMaximumVisible(int count)
{
    g_maximumVisible = qMax(1, count);
}

Toast::Toast(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("fluentToast"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);

    m_overlayCoordinator =
        new overlay::OverlayCoordinator(this, this);
    connect(m_overlayCoordinator,
            &overlay::OverlayCoordinator::hostGeometryChanged,
            this,
            &Toast::syncGeometry);
    connect(m_overlayCoordinator,
            &overlay::OverlayCoordinator::hostDestroyed,
            this,
            [this]() {
        m_animation->stop();
        m_timer->stop();
        m_dismissInProgress = false;
        m_actionInvocationInProgress = false;
        m_hoverPaused = false;
        m_remainingDuration = 0;
        m_isOpen = false;
    });

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(
        overlay::uniformShadowMargins(kShadowMargin));
    outer->setSpacing(0);
    outer->setSizeConstraint(QLayout::SetFixedSize);

    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("fluentToastCard"));
    m_card->setFrameShape(QFrame::NoFrame);
    m_card->setAttribute(Qt::WA_NoSystemBackground);
    m_card->setSizePolicy(
        QSizePolicy::Minimum, QSizePolicy::Fixed);
    auto* row = new QHBoxLayout(m_card);
    row->setContentsMargins(12, 10, 14, 10);
    row->setSpacing(10);

    m_icon = new fluent::FontIcon(m_card);
    m_icon->setObjectName(QStringLiteral("fluentToastIcon"));
    m_icon->setIconSize(kIconSize);
    m_icon->setFixedSize(kIconSize, kIconSize);

    auto* textColumn = new QVBoxLayout;
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(2);

    m_titleLabel = new textfields::Label(m_card);
    m_titleLabel->setObjectName(
        QStringLiteral("fluentToastTitle"));
    m_titleLabel->setFluentTypography(
        Typography::FontRole::BodyStrong);
    m_titleLabel->setTextColorRole(
        textfields::Label::TextColorRole::Primary);
    m_titleLabel->setWordWrap(false);
    m_titleLabel->setSizePolicy(
        QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_titleLabel->hide();

    m_messageLabel = new textfields::Label(m_card);
    m_messageLabel->setObjectName(
        QStringLiteral("fluentToastMessage"));
    m_messageLabel->setFluentTypography(
        Typography::FontRole::Body);
    m_messageLabel->setTextColorRole(
        textfields::Label::TextColorRole::Primary);
    m_messageLabel->setWordWrap(false);
    m_messageLabel->setSizePolicy(
        QSizePolicy::Preferred, QSizePolicy::Preferred);

    textColumn->addWidget(m_titleLabel);
    textColumn->addWidget(m_messageLabel);
    row->addWidget(m_icon, 0, Qt::AlignVCenter);
    row->addLayout(textColumn, 0);

    m_actionButton = new basicinput::Button(m_card);
    m_actionButton->setObjectName(
        QStringLiteral("fluentToastAction"));
    m_actionButton->setFluentStyle(
        basicinput::Button::Standard);
    m_actionButton->setFluentSize(
        basicinput::Button::Small);
    m_actionButton->setFocusVisual(true);
    m_actionButton->hide();
    connect(m_actionButton,
            &basicinput::Button::clicked,
            this,
            [this]() {
        QPointer<QAction> actionGuard = m_action;
        if (!actionGuard || !actionGuard->isEnabled())
            return;
        QPointer<Toast> toastGuard(this);
        m_actionInvocationInProgress = true;
        actionGuard->trigger();
        if (!toastGuard)
            return;
        m_actionInvocationInProgress = false;
        if (m_isOpen)
            requestDismiss(ActionInvoked);
    });
    row->addWidget(m_actionButton, 0, Qt::AlignVCenter);
    outer->addWidget(m_card);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);

    m_animation =
        new QPropertyAnimation(this, "toastProgress", this);
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        requestDismiss(TimedOut);
    });

    hide();
    syncAccessibleName();
    updatePointerInteraction();
    applyPalette();
}

Toast::~Toast()
{
    QObject::disconnect(m_animationFinishedConnection);
    if (m_isOpen) {
        QWidget* host = m_overlayCoordinator
            ? m_overlayCoordinator->topLevelWidget()
            : parentWidget();
        const Placement placement = m_placement;
        m_isOpen = false;
        if (host)
            relayoutHostStack(host, placement);
    }
}

void Toast::setTitle(const QString& title)
{
    if (m_title == title)
        return;

    m_title = title;
    m_titleLabel->setText(m_title);
    m_titleLabel->setVisible(!m_title.isEmpty());
    if (!m_title.isEmpty()) {
        m_messageLabel->setTextColorRole(
            textfields::Label::TextColorRole::Secondary);
    } else {
        m_messageLabel->setTextColorRole(
            textfields::Label::TextColorRole::Primary);
    }
    updateMessageWrapping();
    syncGeometry();
    syncAccessibleName();
    emit titleChanged(m_title);
}

void Toast::setMessage(const QString& message)
{
    if (m_message == message)
        return;

    m_message = message;
    m_messageLabel->setText(m_message);
    updateMessageWrapping();
    syncGeometry();
    syncAccessibleName();
    emit messageChanged(m_message);
}

void Toast::setSeverity(Severity severity)
{
    if (m_severity == severity)
        return;

    m_severity = severity;
    applyPalette();
    emit severityChanged(m_severity);
}

void Toast::setPlacement(Placement placement)
{
    if (m_placement == placement)
        return;

    QWidget* host = m_overlayCoordinator
        ? m_overlayCoordinator->topLevelWidget()
        : parentWidget();
    const Placement previous = m_placement;
    m_placement = placement;
    if (m_isOpen && host) {
        relayoutHostStack(host, previous);
        relayoutHostStack(host, m_placement);
    } else {
        syncGeometry();
    }
    emit placementChanged(m_placement);
}

void Toast::setPlacementMargins(const QMargins& margins)
{
    const QMargins normalized = normalizedMargins(margins);
    if (m_placementMargins == normalized)
        return;

    m_placementMargins = normalized;
    syncGeometry();
    emit placementMarginsChanged(m_placementMargins);
}

void Toast::setDuration(int durationMs)
{
    durationMs = qMax(0, durationMs);
    if (m_duration == durationMs)
        return;

    m_duration = durationMs;
    if (m_isOpen)
        restartDurationTimer();
    emit durationChanged(m_duration);
}

void Toast::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled)
        return;

    m_animationEnabled = enabled;
    emit animationEnabledChanged(m_animationEnabled);
}

void Toast::setAction(QAction* action)
{
    if (m_action.data() == action)
        return;

    QObject::disconnect(m_actionChangedConnection);
    QObject::disconnect(m_actionDestroyedConnection);
    m_actionChangedConnection = QMetaObject::Connection();
    m_actionDestroyedConnection = QMetaObject::Connection();
    m_action = action;

    if (m_action) {
        m_actionChangedConnection = connect(
            m_action.data(),
            &QAction::changed,
            this,
            &Toast::syncActionButton);
        m_actionDestroyedConnection = connect(
            m_action.data(),
            &QObject::destroyed,
            this,
            [this]() {
            m_action = nullptr;
            m_actionChangedConnection = QMetaObject::Connection();
            m_actionDestroyedConnection = QMetaObject::Connection();
            syncActionButton();
            emit actionChanged(nullptr);
        });
    }

    syncActionButton();
    emit actionChanged(m_action.data());
}

void Toast::setPauseOnHoverEnabled(bool enabled)
{
    if (m_pauseOnHoverEnabled == enabled)
        return;

    m_pauseOnHoverEnabled = enabled;
    if (!m_pauseOnHoverEnabled && m_hoverPaused)
        resumeDurationTimer();
    updatePointerInteraction();
    emit pauseOnHoverEnabledChanged(m_pauseOnHoverEnabled);
}

void Toast::setUpdateKey(const QString& key)
{
    if (m_updateKey == key)
        return;
    m_updateKey = key;
    emit updateKeyChanged(m_updateKey);
}

void Toast::setToastProgress(qreal progress)
{
    progress = qBound<qreal>(0.0, progress, 1.0);
    if (qFuzzyCompare(m_progress + 1.0, progress + 1.0))
        return;

    m_progress = progress;
    if (m_opacityEffect)
        m_opacityEffect->setOpacity(m_progress);
    syncGeometry();
}

bool Toast::present(QWidget* anchor)
{
    QWidget* host = anchor ? anchor->window() : nullptr;
    if (!host)
        return false;

    const bool wasOpen = m_isOpen;
    m_animation->stop();
    QObject::disconnect(m_animationFinishedConnection);
    m_animationFinishedConnection = QMetaObject::Connection();
    m_timer->stop();
    m_dismissInProgress = false;
    m_hoverPaused = false;
    m_remainingDuration = m_duration;
    m_pendingDismissReason = Programmatic;

    m_overlayCoordinator->attachTo(host);
    if (overlay::syncInheritedThemeOverride(this, anchor))
        onThemeUpdated();

    updateMessageWrapping();
    ensurePolished();
    if (layout())
        layout()->activate();
    m_isOpen = true;
    if (!property(kStackOrderProperty).isValid())
        setProperty(kStackOrderProperty, QVariant::fromValue(++g_stackOrder));
    syncAccessibleName();

    if (m_animationEnabled)
        setToastProgress(0.0);
    else
        setToastProgress(1.0);
    syncGeometry();
    show();
    m_overlayCoordinator->raiseStack();
    relayoutHostStack(host, m_placement);
    QPointer<Toast> guard(this);
    if (!wasOpen) {
        emit isOpenChanged(true);
        if (!guard || !m_isOpen)
            return false;
    }
    emit presented();
    if (!guard || !m_isOpen)
        return false;
    announceAccessibility();
    if (!guard || !m_isOpen)
        return false;

    if (m_animationEnabled)
        startAnimation(1.0);
    restartDurationTimer();
    return true;
}

void Toast::dismiss()
{
    requestDismiss(
        m_actionInvocationInProgress
            ? ActionInvoked
            : Programmatic);
}

Toast* Toast::showToast(
    QWidget* anchor,
    const QString& message,
    Severity severity,
    int durationMs,
    Placement placement,
    const QMargins& margins)
{
    QWidget* host = anchor ? anchor->window() : nullptr;
    if (!host)
        return nullptr;

    auto* toast = new Toast(host);
    toast->setProperty(kManagedToastProperty, true);
    toast->m_deleteOnDismiss = true;
    toast->setMessage(message);
    toast->setSeverity(severity);
    toast->setDuration(durationMs);
    toast->setPlacement(placement);
    toast->setPlacementMargins(margins);
    QPointer<Toast> toastGuard(toast);
    QPointer<QWidget> hostGuard(host);
    if (!toast->present(anchor)) {
        if (toastGuard)
            delete toastGuard.data();
        return nullptr;
    }
    if (!toastGuard || !hostGuard)
        return nullptr;

    auto managed = managedOpenToastsFor(host, toast->placement());
    while (managed.size() > g_maximumVisible) {
        Toast* oldest = managed.takeFirst();
        if (!oldest || oldest == toast)
            break;
        oldest->m_deleteOnDismiss = true;
        oldest->requestDismiss(Evicted, true);
        if (!toastGuard || !hostGuard)
            return nullptr;
        managed = managedOpenToastsFor(host, toast->placement());
    }
    relayoutHostStack(host, toast->placement());
    return toastGuard.data();
}

Toast* Toast::showOrUpdateToast(
    QWidget* anchor,
    const QString& updateKey,
    const QString& message,
    Severity severity,
    int durationMs,
    Placement placement,
    const QMargins& margins)
{
    QWidget* host = anchor ? anchor->window() : nullptr;
    if (!host)
        return nullptr;
    if (updateKey.isEmpty())
        return showToast(
            anchor,
            message,
            severity,
            durationMs,
            placement,
            margins);

    const auto managed =
        managedOpenToastsFor(host, placement);
    for (auto it = managed.crbegin(); it != managed.crend(); ++it) {
        Toast* toast = *it;
        if (!toast || toast->updateKey() != updateKey)
            continue;

        QPointer<Toast> guard(toast);
        toast->setMessage(message);
        if (!guard)
            return nullptr;
        toast->setSeverity(severity);
        if (!guard)
            return nullptr;
        toast->setDuration(durationMs);
        if (!guard)
            return nullptr;
        toast->setPlacementMargins(margins);
        if (!guard)
            return nullptr;

        toast->restartDurationTimer();
        toast->syncGeometry();
        emit toast->updated();
        if (!guard || !toast->m_isOpen)
            return guard.data();
        toast->announceAccessibility();
        return guard.data();
    }

    QPointer<Toast> toast = showToast(
        anchor,
        message,
        severity,
        durationMs,
        placement,
        margins);
    if (toast)
        toast->setUpdateKey(updateKey);
    return toast.data();
}

QSize Toast::sizeHint() const
{
    return overlay::outerSizeForVisibleCard(
        visibleCardSizeHint(), kShadowMargin);
}

QSize Toast::minimumSizeHint() const
{
    return overlay::outerSizeForVisibleCard(
        QSize(120, 36), kShadowMargin);
}

void Toast::onThemeUpdated()
{
    applyPalette();
    if (m_titleLabel)
        m_titleLabel->onThemeUpdated();
    if (m_messageLabel)
        m_messageLabel->onThemeUpdated();
    if (m_icon)
        m_icon->onThemeUpdated();
    if (m_actionButton)
        m_actionButton->onThemeUpdated();
    updateMessageWrapping();
}

void Toast::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
    if (m_pauseOnHoverEnabled)
        pauseDurationTimer();
}

void Toast::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    if (m_pauseOnHoverEnabled)
        resumeDurationTimer();
}

void Toast::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const QRectF cardRect =
        QRectF(overlay::visibleCardRect(rect(), kShadowMargin));

    for (int i = 0; i < kToastShadowLayers; ++i) {
        const qreal ratio =
            1.0 - static_cast<qreal>(i) / kToastShadowLayers;
        QColor shadow = m_shadowColor;
        shadow.setAlphaF(
            m_shadowOpacity * ratio * kToastShadowOpacityScale);
        painter.setBrush(shadow);
        const qreal spread = 1.5 + i;
        const QRectF shadowRect =
            cardRect
                .adjusted(-spread, -spread * 0.15, spread, spread)
                .translated(0, 2 + i * 0.4);
        painter.drawRoundedRect(
            shadowRect,
            kCornerRadius + spread * 0.35,
            kCornerRadius + spread * 0.35);
    }

    const QRectF fillRect =
        cardRect.adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setBrush(m_surfaceColor);
    painter.setPen(QPen(m_borderColor, 1.0));
    painter.drawRoundedRect(
        fillRect, kCornerRadius, kCornerRadius);
}

bool Toast::isTopPlacement() const
{
    return m_placement == TopStart
        || m_placement == Top
        || m_placement == TopEnd;
}

bool Toast::isStartPlacement() const
{
    return m_placement == TopStart || m_placement == BottomStart;
}

bool Toast::isEndPlacement() const
{
    return m_placement == TopEnd || m_placement == BottomEnd;
}

void Toast::updateMessageWrapping()
{
    if (!m_messageLabel)
        return;

    m_messageLabel->ensurePolished();
    const QFontMetrics metrics(m_messageLabel->font());
    const int naturalWidth = metrics.horizontalAdvance(m_message);
    const bool wrap =
        !m_message.isEmpty() && naturalWidth > kMaximumTextWidth;
    m_messageLabel->setWordWrap(wrap);
    if (wrap) {
        m_messageLabel->setFixedWidth(kMaximumTextWidth);
    } else {
        m_messageLabel->setMinimumWidth(0);
        m_messageLabel->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    if (m_titleLabel) {
        m_titleLabel->setWordWrap(false);
        if (wrap) {
            m_titleLabel->setMaximumWidth(kMaximumTextWidth);
        } else {
            m_titleLabel->setMinimumWidth(0);
            m_titleLabel->setMaximumWidth(QWIDGETSIZE_MAX);
        }
    }
}

QSize Toast::visibleCardSizeHint() const
{
    QSize hint = m_card ? m_card->sizeHint() : QSize(160, 36);
    hint = hint.expandedTo(QSize(120, 36));
    hint.setWidth(qMin(kMaximumCardWidth, hint.width()));
    return hint;
}

int Toast::stackOffset() const
{
    QWidget* host = m_overlayCoordinator
        ? m_overlayCoordinator->topLevelWidget()
        : nullptr;
    if (!host)
        return 0;

    int offset = 0;
    for (Toast* toast : openToastsFor(host, m_placement)) {
        if (toast == this)
            break;
        offset += toast->visibleCardSizeHint().height() + kStackGap;
    }
    return offset;
}

QPoint Toast::resolvedEndPosition() const
{
    QWidget* host = m_overlayCoordinator
        ? m_overlayCoordinator->topLevelWidget()
        : nullptr;
    if (!host)
        return pos();

    const QRect surface = overlay::overlaySurfaceRect(host);
    const QSize cardSize =
        overlay::visibleCardSize(size(), kShadowMargin);
    const int stackShift = stackOffset();
    const bool rtl = host->layoutDirection() == Qt::RightToLeft;

    int cardX = surface.center().x() - cardSize.width() / 2;
    if (isStartPlacement()) {
        cardX = rtl
            ? surface.right() - m_placementMargins.right()
                  - cardSize.width() + 1
            : surface.left() + m_placementMargins.left();
    } else if (isEndPlacement()) {
        cardX = rtl
            ? surface.left() + m_placementMargins.left()
            : surface.right() - m_placementMargins.right()
                  - cardSize.width() + 1;
    } else {
        const int minX = surface.left() + m_placementMargins.left();
        const int maxX =
            surface.right() - m_placementMargins.right()
            - cardSize.width() + 1;
        cardX = maxX < minX ? minX : qBound(minX, cardX, maxX);
    }

    int cardY = 0;
    if (isTopPlacement()) {
        cardY = surface.top() + m_placementMargins.top() + stackShift;
    } else {
        cardY = surface.bottom() - m_placementMargins.bottom()
            - cardSize.height() + 1 - stackShift;
    }

    return overlay::outerTopLeftForVisibleCard(
        QPoint(cardX, cardY), kShadowMargin);
}

void Toast::syncGeometry()
{
    if (!m_overlayCoordinator
        || !m_overlayCoordinator->topLevelWidget())
        return;

    updateMessageWrapping();
    if (layout())
        layout()->activate();
    const QSize desired = sizeHint();
    if (size() != desired)
        resize(desired);

    QPoint target = resolvedEndPosition();
    const int direction = isTopPlacement() ? -1 : 1;
    target += QPoint(
        0,
        qRound(direction * kSlideDistance * (1.0 - m_progress)));
    move(target);
    if (m_isOpen)
        m_overlayCoordinator->raiseStack();
}

void Toast::startAnimation(qreal endValue)
{
    m_animation->stop();
    QObject::disconnect(m_animationFinishedConnection);
    m_animationFinishedConnection = QMetaObject::Connection();

    const auto motion = themeAnimation();
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(endValue);
    m_animation->setDuration(
        endValue > m_progress ? motion.normal : motion.fast);
    m_animation->setEasingCurve(
        endValue > m_progress ? motion.decelerate : motion.exit);
    if (qFuzzyIsNull(endValue)) {
        m_animationFinishedConnection = connect(
            m_animation,
            &QPropertyAnimation::finished,
            this,
            &Toast::finalizeDismiss);
    }
    m_animation->start();
}

void Toast::requestDismiss(
    DismissReason reason, bool immediate)
{
    if (!m_isOpen)
        return;

    if (m_dismissInProgress) {
        if (immediate) {
            m_animation->stop();
            finalizeDismiss();
        }
        return;
    }

    m_dismissInProgress = true;
    m_pendingDismissReason = reason;
    m_timer->stop();
    m_hoverPaused = false;
    m_remainingDuration = 0;
    if (immediate || !m_animationEnabled) {
        m_animation->stop();
        finalizeDismiss();
        return;
    }
    startAnimation(0.0);
}

void Toast::finalizeDismiss()
{
    if (!m_isOpen && !isVisible())
        return;

    QObject::disconnect(m_animationFinishedConnection);
    m_animationFinishedConnection = QMetaObject::Connection();
    m_animation->stop();
    m_timer->stop();
    m_dismissInProgress = false;
    m_hoverPaused = false;
    m_remainingDuration = 0;
    hide();
    QPointer<QWidget> host = m_overlayCoordinator
        ? m_overlayCoordinator->topLevelWidget()
        : parentWidget();
    const Placement placement = m_placement;
    m_overlayCoordinator->detach();
    const bool wasOpen = m_isOpen;
    const DismissReason reason = m_pendingDismissReason;
    m_pendingDismissReason = Programmatic;
    m_isOpen = false;
    QPointer<Toast> guard(this);
    if (wasOpen) {
        emit isOpenChanged(false);
        if (!guard)
            return;
    }
    emit dismissed();
    if (!guard)
        return;
    emit dismissedWithReason(reason);
    if (!guard)
        return;
    if (host)
        relayoutHostStack(host.data(), placement);
    if (m_deleteOnDismiss && !m_isOpen)
        deleteLater();
}

void Toast::restartDurationTimer()
{
    if (!m_timer)
        return;

    m_timer->stop();
    m_remainingDuration = m_duration;
    m_hoverPaused = false;
    if (!m_isOpen || m_duration <= 0)
        return;
    if (m_pauseOnHoverEnabled && underMouse()) {
        m_hoverPaused = true;
        return;
    }
    m_timer->start(m_remainingDuration);
}

void Toast::pauseDurationTimer()
{
    if (!m_isOpen || !m_pauseOnHoverEnabled || m_hoverPaused)
        return;

    if (m_timer->isActive())
        m_remainingDuration = qMax(1, m_timer->remainingTime());
    else if (m_remainingDuration <= 0)
        m_remainingDuration = m_duration;
    m_timer->stop();
    m_hoverPaused = true;
}

void Toast::resumeDurationTimer()
{
    if (!m_hoverPaused)
        return;

    m_hoverPaused = false;
    if (m_isOpen && m_remainingDuration > 0)
        m_timer->start(m_remainingDuration);
}

void Toast::updatePointerInteraction()
{
    const bool hasVisibleAction =
        m_action && m_actionButton && !m_actionButton->isHidden();
    setAttribute(
        Qt::WA_TransparentForMouseEvents,
        !m_pauseOnHoverEnabled && !hasVisibleAction);
}

void Toast::syncActionButton()
{
    if (!m_actionButton)
        return;

    QAction* action = m_action.data();
    if (!action) {
        m_actionButton->hide();
        m_actionButton->setText(QString());
        m_actionButton->setIcon(QIcon());
        updatePointerInteraction();
        updateMessageWrapping();
        syncGeometry();
        return;
    }

    const QString caption = actionCaption(action);
    const QIcon icon = action->icon();
    const bool presentable =
        action->isVisible()
        && (!caption.isEmpty() || !icon.isNull());
    m_actionButton->setText(caption);
    m_actionButton->setIcon(icon);
    m_actionButton->setEnabled(action->isEnabled());
    m_actionButton->setFluentLayout(
        !caption.isEmpty() && !icon.isNull()
        ? basicinput::Button::IconBefore
        : caption.isEmpty() && !icon.isNull()
            ? basicinput::Button::IconOnly
            : basicinput::Button::TextOnly);
    m_actionButton->setAccessibleName(caption);
    m_actionButton->setVisible(presentable);
    updatePointerInteraction();
    updateMessageWrapping();
    syncGeometry();
}

QString Toast::accessibleAnnouncementText() const
{
    if (m_title.isEmpty())
        return m_message;
    if (m_message.isEmpty())
        return m_title;
    return m_title + QStringLiteral(": ") + m_message;
}

void Toast::syncAccessibleName()
{
    const QString nextName = accessibleAnnouncementText();
    const bool tracksAutomaticName =
        accessibleName().isEmpty()
        || accessibleName() == m_autoAccessibleName;
    m_autoAccessibleName = nextName;
    if (tracksAutomaticName)
        setAccessibleName(m_autoAccessibleName);
}

void Toast::announceAccessibility()
{
#if QT_CONFIG(accessibility)
    const QString announcement = accessibleAnnouncementText();
    if (announcement.isEmpty())
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QAccessibleAnnouncementEvent event(this, announcement);
    event.setPoliteness(
        m_severity == Error
        ? QAccessible::AnnouncementPoliteness::Assertive
        : QAccessible::AnnouncementPoliteness::Polite);
#else
    QAccessibleEvent event(this, QAccessible::Alert);
#endif
    QAccessible::updateAccessibility(&event);
#endif
}

void Toast::applyPalette()
{
    const auto& colors = themeColorsRef();
    m_surfaceColor = colors.bgSolid.isValid() ? colors.bgSolid : colors.bgLayer;
    m_surfaceColor.setAlpha(effectiveTheme() == Dark ? 245 : 250);
    m_borderColor = colors.strokeCard;
    const Elevation::ShadowParams shadow =
        themeShadow(Elevation::Low);
    m_shadowColor = shadow.color;
    m_shadowOpacity = shadow.opacity;
    if (m_icon) {
        m_icon->setGlyph(severityGlyph());
        m_icon->setColor(severityForeground());
    }
    update();
}

QString Toast::severityGlyph() const
{
    switch (m_severity) {
    case Success:
        return Typography::Icons::Success;
    case Warning:
        return Typography::Icons::Warning;
    case Error:
        return Typography::Icons::ErrorIcon;
    case Informational:
    default:
        return Typography::Icons::Info;
    }
}

QColor Toast::severityForeground() const
{
    const auto& colors = themeColorsRef();
    switch (m_severity) {
    case Success:
        return colors.systemSuccess;
    case Warning:
        return colors.systemCaution;
    case Error:
        return colors.systemCritical;
    case Informational:
    default:
        return colors.systemInfo;
    }
}

} // namespace fluent::status_info

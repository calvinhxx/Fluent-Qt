#include "components/status_info/Toast.h"

#include <QAbstractAnimation>
#include <QAction>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QPropertyAnimation>
#include <QStyle>
#include <QTextLayout>
#include <QTimer>
#include <QVariant>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QVector>
#include <QtMath>
#include <algorithm>

#include "components/basicinput/Button.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/overlay/OverlayCoordinator.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "components/textfields/Label.h"
#include "components/layout/Card.h"
#include "design/ToastTokens_p.h"
#include "design/Typography.h"

namespace fluent::status_info {
namespace {

constexpr char kManagedToastProperty[] = "_fluentManagedToast";
constexpr char kStackOrderProperty[] = "_fluentToastStackOrder";
constexpr int kShadowMargin = overlay::defaultShadowMargin();
constexpr int kIconSize = Typography::IconSize::Standard;
constexpr int kStackGap = 8;

QString colorSpec(const QColor& color)
{
    return QString::asprintf("#%02X%02X%02X%02X", color.red(), color.green(), color.blue(),
                             color.alpha());
}

int g_maximumVisible = 3;
quint64 g_stackOrder = 0;

QMargins normalizedMargins(const QMargins& margins)
{
    return QMargins(qMax(0, margins.left()), qMax(0, margins.top()), qMax(0, margins.right()),
                    qMax(0, margins.bottom()));
}

QString actionCaption(const QAction* action)
{
    if (!action)
        return {};

    const QString source = action->iconText().isEmpty() ? action->text() : action->iconText();
    QString caption;
    caption.reserve(source.size());
    for (int i = 0; i < source.size(); ++i) {
        if (source.at(i) != QLatin1Char('&')) {
            caption.append(source.at(i));
            continue;
        }
        if (i + 1 < source.size() && source.at(i + 1) == QLatin1Char('&')) {
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

    const auto children = host->findChildren<Toast*>(QString(), Qt::FindDirectChildrenOnly);
    open.reserve(children.size());
    for (Toast* toast : children) {
        if (toast && toast->isOpen() && toast->placement() == placement)
            open.append(toast);
    }
    std::sort(open.begin(), open.end(), [](Toast* left, Toast* right) {
        return left->property(kStackOrderProperty).toULongLong() <
               right->property(kStackOrderProperty).toULongLong();
    });
    return open;
}

QVector<Toast*> Toast::managedOpenToastsFor(QWidget* host, Placement placement)
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
        toast->syncGeometry(true);
}

int Toast::maximumVisible()
{
    return g_maximumVisible;
}

void Toast::setMaximumVisible(int count)
{
    g_maximumVisible = qMax(1, count);
}

Toast::Toast(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("fluentToast"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);

    m_overlayCoordinator = new overlay::OverlayCoordinator(this, this);
    connect(m_overlayCoordinator, &overlay::OverlayCoordinator::hostGeometryChanged, this,
            [this]() { syncGeometry(); });
    connect(m_overlayCoordinator, &overlay::OverlayCoordinator::hostDestroyed, this, [this]() {
        m_animation->stop();
        m_positionAnimation->stop();
        m_timer->stop();
        m_dismissInProgress = false;
        m_actionInvocationInProgress = false;
        m_hoverPaused = false;
        m_remainingDuration = 0;
        m_isOpen = false;
    });

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(overlay::uniformShadowMargins(kShadowMargin));
    outer->setSpacing(0);
    outer->setSizeConstraint(QLayout::SetNoConstraint);

    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("fluentToastCard"));
    m_card->setFrameShape(QFrame::NoFrame);
    m_card->setAttribute(Qt::WA_NoSystemBackground);
    m_card->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_cardLayout = new QHBoxLayout(m_card);
    m_cardLayout->setSpacing(toast_tokens::columnGap);

    m_statusBadge = new layout::Card(m_card);
    m_statusBadge->setObjectName(QStringLiteral("fluentToastStatusBadge"));
    m_statusBadge->setBorderVisible(false);
    m_statusBadge->setFixedSize(toast_tokens::statusSize, toast_tokens::statusSize);
    auto* statusLayout = new QHBoxLayout(m_statusBadge);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    m_icon = new fluent::FontIcon(m_statusBadge);
    m_icon->setObjectName(QStringLiteral("fluentToastIcon"));
    m_icon->setIconSize(kIconSize);
    m_icon->setFixedSize(kIconSize, kIconSize);
    statusLayout->addWidget(m_icon, 0, Qt::AlignCenter);

    m_textLayout = new QVBoxLayout;
    m_textLayout->setContentsMargins(0, 0, 0, 0);
    m_textLayout->setSpacing(toast_tokens::textGap);

    m_titleLabel = new textfields::Label(m_card);
    m_titleLabel->setObjectName(QStringLiteral("fluentToastTitle"));
    m_titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
    m_titleLabel->setTextColorRole(textfields::Label::TextColorRole::Primary);
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setTextElideMode(Qt::ElideNone);
    m_titleLabel->setWordWrap(false);
    m_titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_titleLabel->hide();

    m_messageLabel = new textfields::Label(m_card);
    m_messageLabel->setObjectName(QStringLiteral("fluentToastMessage"));
    m_messageLabel->setFluentTypography(Typography::FontRole::BodyStrong);
    m_messageLabel->setTextColorRole(textfields::Label::TextColorRole::Primary);
    m_messageLabel->setTextFormat(Qt::PlainText);
    m_messageLabel->setTextElideMode(Qt::ElideNone);
    m_messageLabel->setWordWrap(false);
    m_messageLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_textLayout->addWidget(m_titleLabel);
    m_textLayout->addWidget(m_messageLabel);
    m_cardLayout->addWidget(m_statusBadge, 0, Qt::AlignVCenter);
    m_cardLayout->addLayout(m_textLayout, 1);

    m_actionArea = new QWidget(m_card);
    auto* actionLayout = new QHBoxLayout(m_actionArea);
    actionLayout->setContentsMargins(0, toast_tokens::actionGap - toast_tokens::textGap, 0, 0);
    actionLayout->setSpacing(0);
    m_actionButton = new basicinput::Button(m_actionArea);
    m_actionButton->setObjectName(QStringLiteral("fluentToastAction"));
    m_actionButton->setFluentStyle(basicinput::Button::Standard);
    m_actionButton->setFluentSize(basicinput::Button::StandardSize);
    m_actionButton->setFontRole(Typography::FontRole::BodyStrong);
    m_actionButton->setFocusVisual(true);
    m_actionButton->hide();
    connect(m_actionButton, &basicinput::Button::clicked, this, [this]() {
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
    actionLayout->addWidget(m_actionButton, 0, Qt::AlignLeading);
    actionLayout->addStretch();
    m_textLayout->addWidget(m_actionArea);
    m_actionArea->hide();

    m_closeButton = new basicinput::Button(m_card);
    m_closeButton->setObjectName(QStringLiteral("fluentToastClose"));
    m_closeButton->setFluentStyle(basicinput::Button::Subtle);
    m_closeButton->setFluentSize(basicinput::Button::Small);
    m_closeButton->setFluentLayout(basicinput::Button::IconOnly);
    m_closeButton->setIconGlyph(Typography::Icons::Dismiss, Typography::IconSize::Compact);
    m_closeButton->setFixedSize(toast_tokens::closeSize, toast_tokens::closeSize);
    m_closeButton->setFocusVisual(true);
    m_closeButton->setAccessibleName(tr("Dismiss notification"));
    m_closeButton->hide();
    connect(m_closeButton, &basicinput::Button::clicked, this,
            [this]() { requestDismiss(CloseButton); });
    outer->addWidget(m_card);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    m_opacityEffect->setEnabled(false);
    setGraphicsEffect(m_opacityEffect);

    m_animation = new QPropertyAnimation(this, "toastProgress", this);
    m_animation->setObjectName(QStringLiteral("fluentToastPresentationAnimation"));
    m_positionAnimation = new QVariantAnimation(this);
    m_positionAnimation->setObjectName(QStringLiteral("fluentToastPositionAnimation"));
    connect(m_positionAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                m_basePosition = value.toPointF();
                updatePresentationPosition();
            });
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() { requestDismiss(TimedOut); });

    hide();
    syncAccessibleName();
    updatePointerInteraction();
    applyPalette();
    updateMessageWrapping();
}

Toast::~Toast()
{
    QObject::disconnect(m_animationFinishedConnection);
    if (m_isOpen) {
        QWidget* host =
            m_overlayCoordinator ? m_overlayCoordinator->topLevelWidget() : parentWidget();
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
        m_messageLabel->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    } else {
        m_messageLabel->setTextColorRole(textfields::Label::TextColorRole::Primary);
    }
    refreshStackGeometry();
    syncAccessibleName();
    emit titleChanged(m_title);
}

void Toast::setMessage(const QString& message)
{
    if (m_message == message)
        return;

    m_message = message;
    m_messageLabel->setText(m_message);
    refreshStackGeometry();
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

    QWidget* host = m_overlayCoordinator ? m_overlayCoordinator->topLevelWidget() : parentWidget();
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
    refreshStackGeometry();
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
    QPointer<Toast> guard(this);
    emit animationEnabledChanged(m_animationEnabled);
    if (!guard || m_animationEnabled)
        return;

    if (m_positionAnimation->state() == QAbstractAnimation::Running)
        m_positionAnimation->setCurrentTime(m_positionAnimation->duration());

    // Preserve the regular finished/finalize path when a caller disables
    // animation during an active presentation or dismissal.
    // zh_CN: 展示或关闭过程中禁用动效时，仍沿正常 finished/finalize 路径收敛。
    if (m_animation->state() == QAbstractAnimation::Running)
        m_animation->setCurrentTime(m_animation->duration());
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
        m_actionChangedConnection =
            connect(m_action.data(), &QAction::changed, this, &Toast::syncActionButton);
        m_actionDestroyedConnection = connect(m_action.data(), &QObject::destroyed, this, [this]() {
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

void Toast::setClosable(bool closable)
{
    if (m_closable == closable)
        return;
    m_closable = closable;
    m_closeButton->setVisible(m_closable);
    updatePointerInteraction();
    refreshStackGeometry();
    emit closableChanged(m_closable);
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
    m_progress = progress;
    if (qFuzzyCompare(progress, 1.0)) {
        m_presentationOffset = 0.0;
    } else if (m_entering && m_enterStartProgress < 1.0) {
        m_presentationOffset = m_enterStartOffset * (1.0 - progress) / (1.0 - m_enterStartProgress);
    }
    if (m_opacityEffect) {
        m_opacityEffect->setEnabled(m_progress < 1.0);
        m_opacityEffect->setOpacity(m_progress);
    }
    updatePresentationPosition();
}

bool Toast::present(QWidget* anchor)
{
    QWidget* host = anchor ? anchor->window() : nullptr;
    if (!host)
        return false;

    const bool wasOpen = m_isOpen;
    const bool sameHost = m_overlayCoordinator->topLevelWidget() == host;
    m_animation->stop();
    if (!wasOpen || !sameHost)
        m_positionAnimation->stop();
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

    if (!wasOpen || !sameHost)
        m_geometryInitialized = false;
    if (m_animationEnabled && (!wasOpen || !sameHost)) {
        m_entering = true;
        m_enterStartProgress = 0.0;
        m_enterStartOffset = (isTopPlacement() ? -1 : 1) * toast_tokens::enterDistance;
        setToastProgress(0.0);
    } else if (!m_animationEnabled)
        setToastProgress(1.0);
    syncGeometry(wasOpen && sameHost);
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
    requestDismiss(m_actionInvocationInProgress ? ActionInvoked : Programmatic);
}

Toast* Toast::showToast(QWidget* anchor, const QString& message, Severity severity, int durationMs,
                        Placement placement, const QMargins& margins)
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

Toast* Toast::showOrUpdateToast(QWidget* anchor, const QString& updateKey, const QString& message,
                                Severity severity, int durationMs, Placement placement,
                                const QMargins& margins)
{
    QWidget* host = anchor ? anchor->window() : nullptr;
    if (!host)
        return nullptr;
    if (updateKey.isEmpty())
        return showToast(anchor, message, severity, durationMs, placement, margins);

    const auto managed = managedOpenToastsFor(host, placement);
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

    QPointer<Toast> toast = showToast(anchor, message, severity, durationMs, placement, margins);
    if (toast)
        toast->setUpdateKey(updateKey);
    return toast.data();
}

QSize Toast::sizeHint() const
{
    return overlay::outerSizeForVisibleCard(visibleCardSizeHint(), kShadowMargin);
}

QSize Toast::minimumSizeHint() const
{
    return sizeHint();
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
    if (m_closeButton)
        m_closeButton->onThemeUpdated();
    refreshStackGeometry();
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

void Toast::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_positionAnimation->stop();
    m_basePosition = m_targetPosition;
    if (m_animation->state() == QAbstractAnimation::Running)
        m_animation->setCurrentTime(m_animation->duration());
}

void Toast::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LayoutDirectionChange && m_cardLayout)
        refreshStackGeometry();
}

void Toast::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    const qreal scale = devicePixelRatioF();
    const QSize pixels(qCeil(width() * scale), qCeil(height() * scale));
    if (m_surfaceCache.size() != pixels || m_surfaceCache.devicePixelRatioF() != scale) {
        // Cache the soft surface once per geometry/theme/DPI change. Motion only
        // composites this surface and the existing child controls.
        // zh_CN: 仅在几何、主题或 DPI 变化时缓存柔和表面，动画只合成缓存和已有子控件。
        m_surfaceCache = QPixmap(pixels);
        m_surfaceCache.setDevicePixelRatio(scale);
        m_surfaceCache.fill(Qt::transparent);
        QPainter surface(&m_surfaceCache);
        surface.setRenderHint(QPainter::Antialiasing);
        surface.setPen(Qt::NoPen);
        const QRect cardRect = overlay::visibleCardRect(rect(), kShadowMargin);
        if (effectiveTheme() != HighContrast) {
            for (const auto& shadow :
                 {toast_tokens::ambientShadow(), toast_tokens::contactShadow()}) {
                const int spread = shadow.spreadRadius;
                const int layers = shadow.blurRadius / 2;
                overlay::paintLayeredShadow(surface,
                                            cardRect.adjusted(-spread, -spread, spread, spread),
                                            toast_tokens::cornerRadius + spread, shadow,
                                            2.0 / (layers + 1), layers, shadow.offsetY);
            }
        }
        painting::paintRoundedSurface(
            surface, cardRect, {m_surfaceColor, m_borderColor, 1.0, toast_tokens::cornerRadius});
    }
    QPainter painter(this);
    painter.drawPixmap(QPoint(0, 0), m_surfaceCache);
}

bool Toast::isTopPlacement() const
{
    return m_placement == TopStart || m_placement == Top || m_placement == TopEnd;
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
    if (!m_messageLabel || !m_cardLayout)
        return;
    const bool hasAction = m_actionButton && !m_actionButton->isHidden();
    const bool hasTitle = !m_title.isEmpty();
    const bool detailed = hasTitle || hasAction;
    const int verticalPadding =
        detailed ? toast_tokens::detailPadding : toast_tokens::compactPadding;
    const int trailingPadding =
        m_closable ? toast_tokens::closeInset + toast_tokens::closeSize + toast_tokens::closeGap
                   : toast_tokens::horizontalPadding;
    const bool rtl = layoutDirection() == Qt::RightToLeft;
    m_cardLayout->setContentsMargins(
        rtl ? trailingPadding : toast_tokens::horizontalPadding, verticalPadding,
        rtl ? toast_tokens::horizontalPadding : trailingPadding, verticalPadding);
    const Qt::Alignment alignment = detailed ? Qt::AlignTop : Qt::AlignVCenter;
    m_cardLayout->setAlignment(m_statusBadge, alignment);
    m_cardLayout->setAlignment(m_textLayout, alignment);
    m_titleLabel->setVisible(hasTitle);
    m_messageLabel->setVisible(!m_message.isEmpty());
    m_messageLabel->setFluentTypography(hasTitle ? Typography::FontRole::Body
                                                 : Typography::FontRole::BodyStrong);
    m_messageLabel->setTextColorRole(hasTitle ? textfields::Label::TextColorRole::Secondary
                                              : textfields::Label::TextColorRole::Primary);
    m_actionArea->setVisible(hasAction);

    int maximumWidth = toast_tokens::maximumWidth;
    QWidget* host = m_overlayCoordinator ? m_overlayCoordinator->topLevelWidget() : nullptr;
    if (host) {
        const int available = overlay::overlaySurfaceRect(host).width() -
                              m_placementMargins.left() - m_placementMargins.right();
        maximumWidth = qMin(maximumWidth, qMax(1, available));
    }
    const QMargins padding = m_cardLayout->contentsMargins();
    const int gap = m_cardLayout->spacing();
    const int fixedWidth = padding.left() + padding.right() + toast_tokens::statusSize + gap;

    int naturalTextWidth = 0;
    for (auto* label : {m_titleLabel, m_messageLabel}) {
        label->ensurePolished();
        if (!label->isHidden())
            naturalTextWidth =
                qMax(naturalTextWidth, QFontMetrics(label->font()).size(0, label->text()).width());
    }
    const int cardWidth =
        detailed
            ? maximumWidth
            : qMin(maximumWidth, qMax(toast_tokens::minimumWidth, fixedWidth + naturalTextWidth));
    const int textWidth = qMax(1, cardWidth - fixedWidth);
    int textHeight = 0;
    int visibleLabels = 0;
    for (auto* label : {m_titleLabel, m_messageLabel}) {
        label->ensurePolished();
        const QFontMetrics metrics(label->font());
        const bool wrap = metrics.horizontalAdvance(label->text()) > textWidth ||
                          label->text().contains(QLatin1Char('\n'));
        label->setWordWrap(wrap);
        label->setFixedWidth(textWidth);
        // Old fixed heights must not feed back into QLabel's next measurement.
        // zh_CN: 清除上一轮固定高度，避免长内容的高度影响后续短内容测量。
        label->setMinimumHeight(0);
        label->setMaximumHeight(QWIDGETSIZE_MAX);
        const int height = qMax(label->themeFont(Typography::FontRole::Body).lineHeight,
                                wrap ? label->heightForWidth(textWidth) : metrics.height());
        label->setFixedHeight(height);
        if (!label->isHidden()) {
            textHeight += height;
            ++visibleLabels;
        }
    }
    if (visibleLabels > 1)
        textHeight += m_textLayout->spacing();
    if (hasAction) {
        m_actionButton->ensurePolished();
        const int iconWidth = m_actionButton->icon().isNull()
                                  ? 0
                                  : m_actionButton->iconSize().width() + ::Spacing::Small;
        const int actionPadding = ::Spacing::Padding::ControlHorizontal * 2 + iconWidth;
        const int captionWidth = qMax(1, textWidth - actionPadding);
        QStringList lines;
        qreal longestLine = 0;
        // Keep the native Button behavior; only insert visual line breaks for
        // translated actions that cannot fit in the reserved content area.
        // zh_CN: 保留原生 Button 行为，仅为内容区放不下的长操作文案插入显示换行。
        const QString caption = actionCaption(m_action.data());
        for (const QString& paragraph : caption.split(QLatin1Char('\n'))) {
            QTextLayout captionLayout(paragraph, m_actionButton->font());
            QTextOption option;
            option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            captionLayout.setTextOption(option);
            captionLayout.beginLayout();
            while (true) {
                QTextLine line = captionLayout.createLine();
                if (!line.isValid())
                    break;
                line.setLineWidth(captionWidth);
                lines.append(paragraph.mid(line.textStart(), line.textLength()).trimmed());
                longestLine = qMax(longestLine, line.naturalTextWidth());
            }
            captionLayout.endLayout();
        }
        m_actionButton->setText(lines.join(QLatin1Char('\n')));
        const int lineHeight =
            qMax(m_actionButton->fontMetrics().height(),
                 m_actionButton->themeFont(Typography::FontRole::Body).lineHeight);
        const int actionHeight =
            qMax(toast_tokens::controlHeight, int(lines.size()) * lineHeight + ::Spacing::Small);
        m_actionButton->setFixedSize(
            qMin(textWidth, qMax(toast_tokens::controlHeight, qCeil(longestLine) + actionPadding)),
            actionHeight);
        const int actionSpacing = visibleLabels > 0 ? toast_tokens::actionGap : 0;
        const int areaPadding = qMax(0, actionSpacing - m_textLayout->spacing());
        m_actionArea->layout()->setContentsMargins(0, areaPadding, 0, 0);
        m_actionArea->setFixedHeight(actionHeight + areaPadding);
        textHeight += actionHeight + actionSpacing;
    }
    const int contentHeight = qMax(textHeight, toast_tokens::statusSize);
    m_cardSize = QSize(cardWidth, contentHeight + padding.top() + padding.bottom());
    m_card->setFixedSize(m_cardSize);
    // Dismiss stays at the top trailing corner, independent of body height.
    // zh_CN: 关闭按钮固定在顶部尾侧角落，不随正文高度移动。
    const QRect closeRect(cardWidth - toast_tokens::closeInset - toast_tokens::closeSize,
                          toast_tokens::closeInset, toast_tokens::closeSize,
                          toast_tokens::closeSize);
    m_closeButton->setGeometry(QStyle::visualRect(layoutDirection(), m_card->rect(), closeRect));
}

QSize Toast::visibleCardSizeHint() const
{
    return m_cardSize.isValid()
               ? m_cardSize
               : QSize(toast_tokens::minimumWidth,
                       toast_tokens::statusSize + toast_tokens::compactPadding * 2);
}

int Toast::stackOffset() const
{
    QWidget* host = m_overlayCoordinator ? m_overlayCoordinator->topLevelWidget() : nullptr;
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
    QWidget* host = m_overlayCoordinator ? m_overlayCoordinator->topLevelWidget() : nullptr;
    if (!host)
        return pos();

    const QRect surface = overlay::overlaySurfaceRect(host);
    const QSize cardSize = overlay::visibleCardSize(size(), kShadowMargin);
    const int stackShift = stackOffset();
    const bool rtl = host->layoutDirection() == Qt::RightToLeft;

    int cardX = surface.center().x() - cardSize.width() / 2;
    if (isStartPlacement()) {
        cardX = rtl ? surface.right() - m_placementMargins.right() - cardSize.width() + 1
                    : surface.left() + m_placementMargins.left();
    } else if (isEndPlacement()) {
        cardX = rtl ? surface.left() + m_placementMargins.left()
                    : surface.right() - m_placementMargins.right() - cardSize.width() + 1;
    } else {
        const int minX = surface.left() + m_placementMargins.left();
        const int maxX = surface.right() - m_placementMargins.right() - cardSize.width() + 1;
        cardX = maxX < minX ? minX : qBound(minX, cardX, maxX);
    }

    int cardY = 0;
    if (isTopPlacement()) {
        cardY = surface.top() + m_placementMargins.top() + stackShift;
    } else {
        cardY = surface.bottom() - m_placementMargins.bottom() - cardSize.height() + 1 - stackShift;
    }

    return overlay::outerTopLeftForVisibleCard(QPoint(cardX, cardY), kShadowMargin);
}

void Toast::refreshStackGeometry()
{
    updateMessageWrapping();
    QWidget* host = m_overlayCoordinator ? m_overlayCoordinator->topLevelWidget() : nullptr;
    if (m_isOpen && host)
        relayoutHostStack(host, m_placement);
    else
        syncGeometry();
}

void Toast::syncGeometry(bool animatePosition)
{
    if (!m_overlayCoordinator || !m_overlayCoordinator->topLevelWidget())
        return;

    updateMessageWrapping();
    const QSize desired = sizeHint();
    const bool sizeChanged = size() != desired;
    if (sizeChanged)
        resize(desired);
    if (layout())
        layout()->setGeometry(rect());

    const QPoint target = resolvedEndPosition();
    // Content resizes immediately, so its anchored edge must move with it.
    // zh_CN: 内容尺寸立即变化时同步更新锚点，避免位置动画把卡片留在窗口外。
    const bool animate = animatePosition && !sizeChanged && m_geometryInitialized && isVisible() &&
                         m_animationEnabled &&
                         MotionPolicy::instance().mode() == MotionPolicy::Mode::Full;
    m_geometryInitialized = true;
    if (!animate) {
        m_positionAnimation->stop();
        m_basePosition = target;
        m_targetPosition = target;
        updatePresentationPosition();
    } else if (target != m_targetPosition) {
        m_positionAnimation->stop();
        m_targetPosition = target;
        m_positionAnimation->setStartValue(m_basePosition);
        m_positionAnimation->setEndValue(QPointF(target));
        m_positionAnimation->setEasingCurve(themeAnimation().decelerate);
        ::fluent::detail::startMotionTransition(m_positionAnimation, toast_tokens::stackDuration,
                                                m_animationEnabled);
    }
    if (m_isOpen)
        m_overlayCoordinator->raiseStack();
}

void Toast::updatePresentationPosition()
{
    if (!m_geometryInitialized)
        return;
    const bool translate =
        m_animationEnabled && MotionPolicy::instance().mode() == MotionPolicy::Mode::Full;
    const qreal offset = translate ? m_presentationOffset : 0.0;
    move((m_basePosition + QPointF(0.0, offset)).toPoint());
}

void Toast::startAnimation(qreal endValue)
{
    m_animation->stop();
    QObject::disconnect(m_animationFinishedConnection);
    m_animationFinishedConnection = QMetaObject::Connection();

    if (qFuzzyCompare(m_progress + 1.0, endValue + 1.0)) {
        setToastProgress(endValue);
        if (qFuzzyIsNull(endValue))
            finalizeDismiss();
        return;
    }

    const auto motion = themeAnimation();
    m_entering = endValue > m_progress;
    m_enterStartProgress = m_progress;
    m_enterStartOffset = m_presentationOffset;
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(endValue);
    const int duration =
        qMax(1, qRound((m_entering ? toast_tokens::enterDuration : toast_tokens::exitDuration) *
                       qAbs(endValue - m_progress)));
    m_animation->setDuration(duration);
    m_animation->setEasingCurve(endValue > m_progress ? motion.decelerate : motion.exit);
    if (qFuzzyIsNull(endValue)) {
        m_animationFinishedConnection =
            connect(m_animation, &QPropertyAnimation::finished, this, &Toast::finalizeDismiss);
    }
    ::fluent::detail::startMotionTransition(m_animation, duration,
                                            m_animationEnabled && isVisible());
}

void Toast::requestDismiss(DismissReason reason, bool immediate)
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
    m_positionAnimation->stop();
    m_timer->stop();
    m_dismissInProgress = false;
    m_hoverPaused = false;
    m_remainingDuration = 0;
    hide();
    QPointer<QWidget> host =
        m_overlayCoordinator ? m_overlayCoordinator->topLevelWidget() : parentWidget();
    const Placement placement = m_placement;
    m_overlayCoordinator->detach();
    const bool wasOpen = m_isOpen;
    const DismissReason reason = m_pendingDismissReason;
    m_pendingDismissReason = Programmatic;
    m_isOpen = false;
    m_geometryInitialized = false;
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
    const bool hasVisibleAction = m_action && m_actionButton && !m_actionButton->isHidden();
    setAttribute(Qt::WA_TransparentForMouseEvents,
                 !m_pauseOnHoverEnabled && !hasVisibleAction && !m_closable);
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
        refreshStackGeometry();
        return;
    }

    const QString caption = actionCaption(action);
    const QIcon icon = action->icon();
    const bool presentable = action->isVisible() && (!caption.isEmpty() || !icon.isNull());
    m_actionButton->setText(caption);
    m_actionButton->setIcon(icon);
    m_actionButton->setEnabled(action->isEnabled());
    m_actionButton->setFluentLayout(
        !caption.isEmpty() && !icon.isNull()  ? basicinput::Button::IconBefore
        : caption.isEmpty() && !icon.isNull() ? basicinput::Button::IconOnly
                                              : basicinput::Button::TextOnly);
    m_actionButton->setAccessibleName(caption);
    m_actionButton->setVisible(presentable);
    updatePointerInteraction();
    refreshStackGeometry();
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
        accessibleName().isEmpty() || accessibleName() == m_autoAccessibleName;
    m_autoAccessibleName = nextName;
    if (tracksAutomaticName)
        setAccessibleName(m_autoAccessibleName);
}

void Toast::announceAccessibility()
{
    const QString announcement = accessibleAnnouncementText();
    if (announcement.isEmpty())
        return;
    fluentSendAccessibleAnnouncement(this, announcement,
                                     m_severity == Error
                                         ? FluentAccessibleAnnouncementPoliteness::Assertive
                                         : FluentAccessibleAnnouncementPoliteness::Polite);
}

void Toast::applyPalette()
{
    const auto& colors = themeColorsRef();
    const bool contrast = effectiveTheme() == HighContrast;
    const bool dark = effectiveThemeUsesDarkAppearance();
    m_surfaceColor = colors.bgLayer;
    m_surfaceColor.setAlpha(255);
    m_borderColor = contrast ? colors.strokeCard : QColor(Qt::transparent);
    m_surfaceCache = QPixmap();
    if (m_statusBadge) {
        const QJsonObject tint{{"bgLayer", colorSpec(severityBackground())}};
        m_statusBadge->setThemeOverrides(
            {{"light", tint},
             {"dark", tint},
             {"contrast", tint},
             {"radius", QJsonObject{{"control", toast_tokens::statusRadius}}}});
    }
    if (m_actionButton) {
        const QColor actionFill = dark ? colors.bgLayerAlt : colors.bgCanvas;
        const QJsonObject actionColors{
            {"controlDefault", colorSpec(actionFill)},
            {"controlSecondary", colorSpec(toast_tokens::actionHover(dark))},
            {"controlTertiary", colorSpec(actionFill)},
            {"strokeDefault", "#00000000"},
            {"strokeDivider", "#00000000"}};
        m_actionButton->setThemeOverrides(
            contrast ? QJsonObject{}
                     : QJsonObject{{"light", actionColors}, {"dark", actionColors}});
    }
    if (m_closeButton) {
        const QJsonObject closeColors{{"textPrimary", colorSpec(colors.textSecondary)}};
        m_closeButton->setThemeOverrides(
            {{"light", closeColors}, {"dark", closeColors}, {"contrast", closeColors}});
    }
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
        return Typography::Icons::glyph(QStringLiteral("ic_fluent_checkmark_circle_16_regular"));
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
        return colors.accentDefault;
    }
}

QColor Toast::severityBackground() const
{
    const auto& colors = themeColorsRef();
    switch (m_severity) {
    case Success:
        return colors.systemSuccessBg;
    case Warning:
        return colors.systemCautionBg;
    case Error:
        return colors.systemCriticalBg;
    case Informational:
    default:
        return effectiveTheme() == HighContrast
                   ? colors.systemInfoBg
                   : toast_tokens::informationalTint(effectiveThemeUsesDarkAppearance());
    }
}

} // namespace fluent::status_info

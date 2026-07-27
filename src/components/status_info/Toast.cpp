#include "components/status_info/Toast.h"

#include <QAbstractAnimation>
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
    outer->addWidget(m_card);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);

    m_animation =
        new QPropertyAnimation(this, "toastProgress", this);
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &Toast::dismiss);

    hide();
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
    if (m_isOpen && m_duration > 0)
        m_timer->start(m_duration);
    else if (m_isOpen)
        m_timer->stop();
    emit durationChanged(m_duration);
}

void Toast::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled)
        return;

    m_animationEnabled = enabled;
    emit animationEnabledChanged(m_animationEnabled);
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
    if (accessibleName().isEmpty()) {
        setAccessibleName(
            m_title.isEmpty() ? m_message
                              : m_title + QStringLiteral(": ") + m_message);
    }

    if (m_animationEnabled)
        setToastProgress(0.0);
    else
        setToastProgress(1.0);
    syncGeometry();
    show();
    m_overlayCoordinator->raiseStack();
    relayoutHostStack(host, m_placement);
    if (!wasOpen)
        emit isOpenChanged(true);
    emit presented();

    if (m_animationEnabled)
        startAnimation(1.0);
    if (m_duration > 0)
        m_timer->start(m_duration);
    return true;
}

void Toast::dismiss()
{
    if (!m_isOpen)
        return;

    m_timer->stop();
    if (!m_animationEnabled) {
        finalizeDismiss();
        return;
    }
    startAnimation(0.0);
}

Toast* Toast::showToast(
    QWidget* anchor,
    const QString& message,
    Severity severity,
    int durationMs,
    Placement placement)
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
    if (!toast->present(anchor)) {
        delete toast;
        return nullptr;
    }

    auto managed = managedOpenToastsFor(host, toast->placement());
    while (managed.size() > g_maximumVisible) {
        Toast* oldest = managed.takeFirst();
        if (!oldest || oldest == toast)
            break;
        oldest->m_deleteOnDismiss = true;
        oldest->m_animation->stop();
        oldest->finalizeDismiss();
        managed = managedOpenToastsFor(host, toast->placement());
    }
    relayoutHostStack(host, toast->placement());
    return toast;
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
    updateMessageWrapping();
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

void Toast::finalizeDismiss()
{
    if (!m_isOpen && !isVisible())
        return;

    QObject::disconnect(m_animationFinishedConnection);
    m_animationFinishedConnection = QMetaObject::Connection();
    m_animation->stop();
    m_timer->stop();
    hide();
    QWidget* host = m_overlayCoordinator
        ? m_overlayCoordinator->topLevelWidget()
        : parentWidget();
    const Placement placement = m_placement;
    m_overlayCoordinator->detach();
    const bool wasOpen = m_isOpen;
    m_isOpen = false;
    if (wasOpen)
        emit isOpenChanged(false);
    emit dismissed();
    if (host)
        relayoutHostStack(host, placement);
    if (m_deleteOnDismiss)
        deleteLater();
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

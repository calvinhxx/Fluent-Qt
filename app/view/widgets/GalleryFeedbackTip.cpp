#include "GalleryFeedbackTip.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "components/basicinput/Button.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/layout/Card.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

namespace fluent::gallery {
namespace {

constexpr int kMaxCardWidth = 244;
constexpr int kCompactCardHeight = 66;
constexpr int kCompactActionHeight = 92;
constexpr int kArtifactReadyHeight = 140;
constexpr int kEnterDurationMs = 220;
constexpr int kExitDurationMs = 140;
constexpr int kMorphDurationMs = 160;
constexpr int kSpinDurationMs = 900;
constexpr int kRestMargin = 12;
constexpr int kCompactRailMaxWidth = 64;

QString colorSpec(const QColor& color)
{
    return QString::asprintf("#%02X%02X%02X%02X", color.red(), color.green(), color.blue(), color.alpha());
}

QColor mixColor(const QColor& surface, const QColor& foreground, qreal mix, bool highContrast)
{
    if (highContrast)
        return surface;
    return QColor::fromRgbF(surface.redF() * (1.0 - mix) + foreground.redF() * mix,
        surface.greenF() * (1.0 - mix) + foreground.greenF() * mix,
        surface.blueF() * (1.0 - mix) + foreground.blueF() * mix, 1.0);
}

QColor badgeTint(const QColor& surface, const QColor& foreground, bool dark, bool highContrast)
{
    return mixColor(surface, foreground, dark ? 0.16 : 0.11, highContrast);
}

QString formatByteSize(qint64 bytes)
{
    if (bytes <= 0)
        return {};
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) {
        const qint64 kb = qMax<qint64>(1, (bytes + 512) / 1024);
        return QStringLiteral("%1 KB").arg(kb);
    }
    return QStringLiteral("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
}

} // namespace

GalleryFeedbackTip::GalleryFeedbackTip(QWidget* parent) : TeachingTip(parent)
{
    setObjectName(QStringLiteral("galleryFeedbackTip"));
    setAccessibleName(tr("Problem report"));
    setCardSize(m_targetCardSize);
    setTailVisible(false);
    setLightDismissEnabled(true);
    setFocusOnOpenEnabled(false);

    // Reserve a 4px outer left gutter so Small Button's 8px internal horizontal padding
    // places its painted text flush at x = 12 alongside the header badge and ZIP capsule.
    // zh_CN: 左侧预留 4px 外边距，使 Small Button 的 8px 内部水平内边距令绘制文字严格落在 x = 12，与顶部徽标及文件胶囊对齐。
    m_rootLayout = new QVBoxLayout(contentHost());
    m_rootLayout->setContentsMargins(4, 12, 12, 12);
    m_rootLayout->setSpacing(0);

    auto* headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(8, 0, 0, 0);
    headerRow->setSpacing(8);

    m_statusBadge = new layout::Card(contentHost());
    m_statusBadge->setObjectName(QStringLiteral("galleryFeedbackStatusBadge"));
    m_statusBadge->setBorderVisible(false);
    m_statusBadge->setFixedSize(24, 24);
    auto* badgeLayout = new QHBoxLayout(m_statusBadge);
    badgeLayout->setContentsMargins(0, 0, 0, 0);
    m_icon = new FontIcon(m_statusBadge);
    m_icon->setObjectName(QStringLiteral("galleryFeedbackStatusIcon"));
    m_icon->setIconSize(Typography::IconSize::Standard);
    m_icon->setFixedSize(Typography::IconSize::Standard, Typography::IconSize::Standard);
    badgeLayout->addWidget(m_icon, 0, Qt::AlignCenter);
    headerRow->addWidget(m_statusBadge, 0, Qt::AlignTop);

    m_title = new textfields::Label(contentHost());
    m_title->setObjectName(QStringLiteral("galleryFeedbackTitle"));
    m_title->setWordWrap(true);
    m_title->setMinimumHeight(24);
    headerRow->addWidget(m_title, 1);

    auto* close = new basicinput::Button(contentHost());
    close->setObjectName(QStringLiteral("galleryFeedbackClose"));
    close->setAccessibleName(tr("Close report notification"));
    close->setFluentStyle(basicinput::Button::Subtle);
    close->setTextColorRole(basicinput::Button::TextColorRole::Secondary);
    close->setFluentLayout(basicinput::Button::IconOnly);
    close->setIconGlyph(Typography::Icons::Dismiss, Typography::IconSize::Compact);
    close->setFixedSize(24, 24);
    connect(close, &basicinput::Button::clicked, this, [this] { closeWithReason(CloseButton); });
    headerRow->addWidget(close, 0, Qt::AlignTop);
    m_rootLayout->addLayout(headerRow);

    m_artifactHost = new QWidget(contentHost());
    auto* artifactOuter = new QVBoxLayout(m_artifactHost);
    artifactOuter->setContentsMargins(8, 6, 0, 0);
    artifactOuter->setSpacing(0);
    m_artifactCard = new layout::Card(m_artifactHost);
    m_artifactCard->setObjectName(QStringLiteral("galleryFeedbackArtifact"));
    m_artifactCard->setBorderVisible(true);
    m_artifactCard->setFixedHeight(40);

    auto* artifactLayout = new QHBoxLayout(m_artifactCard);
    artifactLayout->setContentsMargins(8, 5, 10, 5);
    artifactLayout->setSpacing(8);

    m_artifactIconBox = new layout::Card(m_artifactCard);
    m_artifactIconBox->setObjectName(QStringLiteral("galleryFeedbackArtifactIconBox"));
    m_artifactIconBox->setBorderVisible(false);
    m_artifactIconBox->setFixedSize(26, 26);
    auto* artifactIconLayout = new QHBoxLayout(m_artifactIconBox);
    artifactIconLayout->setContentsMargins(0, 0, 0, 0);
    m_artifactIcon = new FontIcon(m_artifactIconBox);
    m_artifactIcon->setObjectName(QStringLiteral("galleryFeedbackArtifactIcon"));
    m_artifactIcon->setGlyph(QStringLiteral("ic_fluent_folder_zip_16_regular"));
    m_artifactIcon->setIconSize(Typography::IconSize::Standard);
    m_artifactIcon->setFixedSize(Typography::IconSize::Standard, Typography::IconSize::Standard);
    artifactIconLayout->addWidget(m_artifactIcon, 0, Qt::AlignCenter);
    artifactLayout->addWidget(m_artifactIconBox, 0, Qt::AlignVCenter);

    auto* metaColumn = new QVBoxLayout;
    metaColumn->setContentsMargins(0, 0, 0, 0);
    metaColumn->setSpacing(1);

    auto* nameRow = new QHBoxLayout;
    nameRow->setContentsMargins(0, 0, 0, 0);
    nameRow->setSpacing(6);
    m_artifactName = new textfields::Label(m_artifactCard);
    m_artifactName->setObjectName(QStringLiteral("galleryFeedbackArtifactName"));
    m_artifactName->setTextColorRole(textfields::Label::TextColorRole::Primary);
    m_artifactName->setTextElideMode(Qt::ElideMiddle);
    nameRow->addWidget(m_artifactName, 1);

    m_artifactSize = new textfields::Label(m_artifactCard);
    m_artifactSize->setObjectName(QStringLiteral("galleryFeedbackArtifactSize"));
    m_artifactSize->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    nameRow->addWidget(m_artifactSize, 0, Qt::AlignRight);
    metaColumn->addLayout(nameRow);

    auto* stateRow = new QHBoxLayout;
    stateRow->setContentsMargins(0, 0, 0, 0);
    stateRow->setSpacing(4);
    m_artifactStateIcon = new FontIcon(m_artifactCard);
    m_artifactStateIcon->setObjectName(QStringLiteral("galleryFeedbackArtifactStateIcon"));
    m_artifactStateIcon->setIconSize(Typography::IconSize::Compact);
    m_artifactStateIcon->setFixedSize(Typography::IconSize::Compact, Typography::IconSize::Compact);
    stateRow->addWidget(m_artifactStateIcon, 0, Qt::AlignVCenter);

    m_artifactState = new textfields::Label(m_artifactCard);
    m_artifactState->setObjectName(QStringLiteral("galleryFeedbackArtifactState"));
    m_artifactState->setTextColorRole(textfields::Label::TextColorRole::Primary);
    stateRow->addWidget(m_artifactState, 1);
    metaColumn->addLayout(stateRow);

    artifactLayout->addLayout(metaColumn, 1);
    artifactOuter->addWidget(m_artifactCard);
    m_artifactHost->hide();
    m_rootLayout->addWidget(m_artifactHost);

    m_messageLayout = new QHBoxLayout;
    m_messageLayout->setSpacing(0);
    m_message = new textfields::Label(contentHost());
    m_message->setObjectName(QStringLiteral("galleryFeedbackMessage"));
    m_message->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    m_message->setWordWrap(true);
    m_message->setMinimumHeight(16);
    m_messageLayout->addWidget(m_message, 1);
    m_rootLayout->addLayout(m_messageLayout);

    m_actionHost = new QWidget(contentHost());
    m_actionLayout = new QHBoxLayout(m_actionHost);
    m_actionLayout->setSpacing(0);
    m_action = new basicinput::Button(m_actionHost);
    m_action->setObjectName(QStringLiteral("galleryFeedbackAction"));
    m_action->setFluentStyle(basicinput::Button::Subtle);
    m_action->setTextColorRole(basicinput::Button::TextColorRole::Accent);
    m_action->setFluentSize(basicinput::Button::Small);
    m_action->setCursor(Qt::PointingHandCursor);
    m_action->setFixedHeight(24);
    m_actionLayout->addWidget(m_action, 0, Qt::AlignLeft);
    m_actionHost->hide();
    m_rootLayout->addWidget(m_actionHost);

    updateBodyIndentation();

    m_spinAnimation = new QPropertyAnimation(m_icon, "rotation", this);
    m_spinAnimation->setStartValue(0.0);
    m_spinAnimation->setEndValue(360.0);
    m_spinAnimation->setDuration(kSpinDurationMs);
    m_spinAnimation->setEasingCurve(QEasingCurve::Linear);
    m_spinAnimation->setLoopCount(-1);

    m_actionFadeAnimation = new QPropertyAnimation(m_action, "contentOpacity", this);
    m_actionFadeAnimation->setEasingCurve(QEasingCurve::OutCubic);

    m_sizeAnimation = new QVariantAnimation(this);
    m_sizeAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_sizeAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setCardSize(value.toSize());
    });

    connect(this, &dialogs_flyouts::Popup::popupProgressChanged, this, [this] { applyRevealProgress(); });
    connect(this, &dialogs_flyouts::Popup::aboutToHide, this, [this] {
        m_spinAnimation->stop();
        m_icon->setRotation(0.0);
        m_sizeAnimation->stop();
        m_actionFadeAnimation->stop();
        m_action->setContentOpacity(1.0);
    });
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this, [this] {
        applyRevealProgress();
        updateSpinState();
        if (m_sizeAnimation->state() == QAbstractAnimation::Running
            && MotionPolicy::instance().mode() != MotionPolicy::Mode::Full) {
            m_sizeAnimation->stop();
            setCardSize(m_targetCardSize);
        }
    });

    // Keep Popup's open/close lifecycle and progress property; differentiate crisp
    // entrance deceleration from a snappier exit under the shared motion policy.
    // zh_CN: 保留 Popup 的开关生命周期和进度属性，在共享动效策略下区分舒展入场与利落退场。
    for (auto* animation : findChildren<QPropertyAnimation*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (animation->targetObject() == this && animation->propertyName() == "popupProgress") {
            connect(animation, &QPropertyAnimation::stateChanged, this,
                [this, animation](QAbstractAnimation::State state, QAbstractAnimation::State) {
                    if (state != QAbstractAnimation::Running)
                        return;
                    const auto motion = themeAnimation();
                    const bool entering = isOpen();
                    animation->setEasingCurve(entering ? motion.decelerate : motion.accelerate);
                    animation->setDuration(MotionPolicy::instance().resolvedDuration(
                        entering ? kEnterDurationMs : kExitDurationMs, isAnimationEnabled()));
                });
        }
    }

    updateColors();
}

QString GalleryFeedbackTip::title() const
{
    return m_title->text();
}

QString GalleryFeedbackTip::message() const
{
    return m_message->text();
}

QString GalleryFeedbackTip::artifactName() const
{
    return m_artifactHost->isHidden() ? QString() : m_artifactName->text();
}

QString GalleryFeedbackTip::artifactState() const
{
    return m_artifactHost->isHidden() ? QString() : m_artifactState->text();
}

void GalleryFeedbackTip::setFeedback(const QString& title, const QString& message, bool preparing, bool error)
{
    m_preparing = preparing;
    m_error = error;
    m_title->setText(title);
    m_message->setText(message);
    QObject::disconnect(m_actionConnection);
    m_actionFadeAnimation->stop();
    m_action->setContentOpacity(1.0);
    m_artifactHost->hide();
    m_actionHost->hide();
    m_rootLayout->setContentsMargins(4, 12, 12, 12);
    updateBodyIndentation();
    updateAccessibleSummary();
    m_icon->setGlyph(preparing ? QStringLiteral("ic_fluent_arrow_clockwise_16_regular")
            : error            ? QStringLiteral("ic_fluent_error_circle_16_regular")
                               : QStringLiteral("ic_fluent_checkmark_circle_16_regular"));
    updateColors();
    updateSpinState();
    syncPlacement();
}

void GalleryFeedbackTip::setArtifact(const QString& fileName, qint64 byteCount, bool downloaded)
{
    if (fileName.isEmpty()) {
        m_artifactHost->hide();
        updateBodyIndentation();
        updateAccessibleSummary();
        syncPlacement();
        return;
    }
    m_artifactName->setText(fileName);
    const QString formattedSize = formatByteSize(byteCount);
    m_artifactSize->setText(formattedSize);
    m_artifactSize->setVisible(!formattedSize.isEmpty());
    m_artifactStateIcon->setGlyph(downloaded
            ? QStringLiteral("ic_fluent_arrow_download_16_regular")
            : QStringLiteral("ic_fluent_clipboard_checkmark_16_regular"));
    m_artifactState->setText(downloaded ? tr("Download started") : tr("Copied to clipboard"));
    m_artifactHost->show();
    updateBodyIndentation();
    updateAccessibleSummary();
    updateColors();
    syncPlacement();
}

void GalleryFeedbackTip::setAction(const QString& caption,
    const QString& glyph,
    QObject* context,
    std::function<void()> handler)
{
    QObject::disconnect(m_actionConnection);
    const bool wasHidden = m_actionHost->isHidden();
    m_action->setText(caption);
    m_action->setIconGlyph(glyph, Typography::IconSize::Compact);
    m_action->setFluentLayout(basicinput::Button::IconAfter);
    m_action->setAccessibleName(caption);
    m_rootLayout->setContentsMargins(4, 12, 12, 10);
    m_actionHost->show();
    updateBodyIndentation();
    updateColors();

    m_actionFadeAnimation->stop();
    if (wasHidden && isOpen() && isVisible() && popupProgress() > 0.0 && isAnimationEnabled()
        && MotionPolicy::instance().mode() == MotionPolicy::Mode::Full) {
        m_action->setContentOpacity(0.0);
        m_actionFadeAnimation->setDuration(
            MotionPolicy::instance().resolvedDuration(kMorphDurationMs, isAnimationEnabled()));
        m_actionFadeAnimation->setStartValue(0.0);
        m_actionFadeAnimation->setEndValue(1.0);
        m_actionFadeAnimation->start();
    } else {
        m_action->setContentOpacity(1.0);
    }

    const QPointer<GalleryFeedbackTip> guard(this);
    m_actionConnection = connect(m_action, &basicinput::Button::clicked, context, [guard, handler] {
        if (guard)
            guard->closeWithReason(ActionButton);
        handler();
    });
    syncPlacement();
}

void GalleryFeedbackTip::showFor(QWidget* anchor)
{
    setTarget(anchor);
    syncPlacement();
    if (!isOpen())
        open();
    updateSpinState();
}

void GalleryFeedbackTip::updateBodyIndentation()
{
    if (!m_artifactHost->isHidden()) {
        m_messageLayout->setContentsMargins(8, 6, 0, 0);
        m_actionLayout->setContentsMargins(0, 2, 0, 0);
    } else {
        m_messageLayout->setContentsMargins(40, 2, 4, 0);
        m_actionLayout->setContentsMargins(32, 4, 0, 0);
    }
}

void GalleryFeedbackTip::updateAccessibleSummary()
{
    QString summary = m_title->text();
    if (!m_artifactHost->isHidden()) {
        summary += QStringLiteral(". ") + m_artifactName->text();
        if (!m_artifactSize->isHidden() && !m_artifactSize->text().isEmpty())
            summary += QStringLiteral(" (") + m_artifactSize->text() + QStringLiteral(", ") + m_artifactState->text()
                + QStringLiteral(")");
        else
            summary += QStringLiteral(" (") + m_artifactState->text() + QStringLiteral(")");
    }
    if (!m_message->text().isEmpty())
        summary += QStringLiteral(". ") + m_message->text();
    setAccessibleDescription(summary);
}

void GalleryFeedbackTip::syncPlacement()
{
    auto* anchor = target();
    auto* host = anchor ? anchor->window() : nullptr;
    if (!anchor || !host)
        return;

    const QRect area = overlay::overlaySurfaceRect(host);
    const QRect rect(anchor->mapTo(host, QPoint()), anchor->size());
    const int width = qMin(kMaxCardWidth, qMax(1, area.width() - 24));
    contentHost()->layout()->activate();
    const int minHeight = !m_artifactHost->isHidden()
        ? (m_actionHost->isHidden() ? 116 : kArtifactReadyHeight)
        : (m_actionHost->isHidden() ? kCompactCardHeight : kCompactActionHeight);
    const int height = qMax(minHeight, contentHost()->layout()->totalHeightForWidth(width));
    applyTargetCardSize(QSize(width, height));

    // Reserve the ready-state footprint while preparing, and align with the anchor edge
    // when near a pane boundary so the card does not straddle the navigation divider.
    // zh_CN: 准备期间也按完成态尺寸选方向，并在靠近分栏边缘时与锚点对齐，避免横跨导航分界线。
    constexpr int outerInset = 16;
    constexpr int alignInset = 8;
    const int placementHeight = qMax(kArtifactReadyHeight, height);
    const int centeredTop = rect.center().y() - placementHeight / 2;
    const bool sideCenteredFits = centeredTop >= area.top() + outerInset
        && centeredTop + placementHeight <= area.bottom() + 1 - outerInset;
    const bool rightFits = area.right() - rect.right() >= width + kRestMargin + outerInset;
    const bool leftFits = rect.left() - area.left() >= width + kRestMargin + outerInset;
    const bool topFits = rect.top() - area.top() >= placementHeight + kRestMargin + outerInset;
    const bool bottomFits = area.bottom() - rect.bottom() >= placementHeight + kRestMargin + outerInset;
    const bool alignRightFits = rect.right() - width + 1 >= area.left() + alignInset;
    const bool alignLeftFits = rect.left() + width - 1 <= area.right() - alignInset;
    const bool alignBottomFits = rect.bottom() - placementHeight + 1 >= area.top() + outerInset;

    if (sideCenteredFits && rightFits)
        setPreferredPlacement(Right);
    else if (sideCenteredFits && leftFits)
        setPreferredPlacement(Left);
    else if (topFits && alignRightFits)
        setPreferredPlacement(TopRight);
    else if (rightFits && alignBottomFits)
        setPreferredPlacement(RightBottom);
    else if (topFits && alignLeftFits)
        setPreferredPlacement(TopLeft);
    else if (topFits)
        setPreferredPlacement(Top);
    else if (bottomFits && alignRightFits)
        setPreferredPlacement(BottomRight);
    else if (bottomFits && alignLeftFits)
        setPreferredPlacement(BottomLeft);
    else
        setPreferredPlacement(Bottom);

    applyRevealProgress();
}

void GalleryFeedbackTip::applyTargetCardSize(const QSize& targetSize)
{
    m_targetCardSize = targetSize;
    const bool morph = isOpen() && isVisible() && popupProgress() > 0.0 && isAnimationEnabled()
        && MotionPolicy::instance().mode() == MotionPolicy::Mode::Full && cardSize().width() == targetSize.width()
        && cardSize().height() != targetSize.height();
    if (!morph) {
        m_sizeAnimation->stop();
        setCardSize(targetSize);
        return;
    }
    if (m_sizeAnimation->state() == QAbstractAnimation::Running && m_sizeAnimation->endValue().toSize() == targetSize)
        return;
    m_sizeAnimation->stop();
    m_sizeAnimation->setDuration(MotionPolicy::instance().resolvedDuration(kMorphDurationMs, isAnimationEnabled()));
    m_sizeAnimation->setStartValue(cardSize());
    m_sizeAnimation->setEndValue(targetSize);
    m_sizeAnimation->start();
}

int GalleryFeedbackTip::resolvedRestMargin() const
{
    int margin = kRestMargin;
    const auto* anchor = target();
    const auto* host = anchor ? anchor->window() : nullptr;
    const auto placement = preferredPlacement();
    if (anchor && host && (placement == Right || placement == RightBottom)) {
        if (const auto* pane = anchor->parentWidget(); pane && pane != host && pane->width() <= kCompactRailMaxWidth) {
            const int paneRight = pane->mapTo(host, QPoint(pane->width() - 1, 0)).x();
            const int anchorRight = anchor->mapTo(host, QPoint(anchor->width() - 1, 0)).x();
            margin = qMax(margin, paneRight - anchorRight + 8);
        }
    }
    return margin;
}

void GalleryFeedbackTip::applyRevealProgress()
{
    const int restMargin = resolvedRestMargin();
    const bool slide = isAnimationEnabled() && MotionPolicy::instance().mode() == MotionPolicy::Mode::Full;
    const int margin = !slide ? restMargin
        : isOpen()
        ? qRound((restMargin - 6.0) + 6.0 * popupProgress())
        : qRound((restMargin - 4.0) + 4.0 * popupProgress());
    setPlacementMargin(margin);
}

void GalleryFeedbackTip::updateSpinState()
{
    const bool shouldSpin = m_preparing && isOpen() && isVisible() && isAnimationEnabled()
        && MotionPolicy::instance().mode() == MotionPolicy::Mode::Full;
    if (shouldSpin) {
        if (m_spinAnimation->state() != QAbstractAnimation::Running)
            m_spinAnimation->start();
    } else {
        m_spinAnimation->stop();
        m_icon->setRotation(0.0);
    }
}

bool GalleryFeedbackTip::eventFilter(QObject* watched, QEvent* event)
{
    const bool relevant = overlay::anchorGeometryMayChange(watched, event, target());
    const bool result = TeachingTip::eventFilter(watched, event);
    if (relevant && isOpen() && !m_placementPending) {
        m_placementPending = true;
        QTimer::singleShot(0, this, [this] {
            m_placementPending = false;
            if (isOpen())
                syncPlacement();
        });
    }
    return result;
}

void GalleryFeedbackTip::onThemeUpdated()
{
    TeachingTip::onThemeUpdated();
    if (m_icon)
        updateColors();
}

void GalleryFeedbackTip::updateColors()
{
    const auto colors = themeColors();
    const bool dark = effectiveThemeUsesDarkAppearance();
    const bool highContrast = effectiveTheme() == HighContrast;
    const QColor iconColor = m_error ? colors.systemCritical
        : m_preparing
        ? colors.accentDefault
        : colors.systemSuccess;
    const QColor badgeBg = badgeTint(colors.bgLayer, iconColor, dark, highContrast);
    const QJsonObject tint{{"bgLayer", colorSpec(badgeBg)}};
    m_statusBadge->setThemeOverrides(
        {{"light", tint}, {"dark", tint}, {"contrast", tint}, {"radius", QJsonObject{{"control", 6}}}});
    m_icon->setColor(iconColor);

    const QColor artifactBg = mixColor(colors.bgLayer, dark ? QColor(Qt::white) : QColor(Qt::black),
        dark ? 0.055 : 0.035, highContrast);
    const QColor artifactBorder = highContrast
        ? colors.strokeDefault
        : mixColor(colors.bgLayer, dark ? QColor(Qt::white) : QColor(Qt::black), dark ? 0.11 : 0.08, false);
    const QJsonObject artifactColors{{"bgLayer", colorSpec(artifactBg)}, {"strokeDefault", colorSpec(artifactBorder)}};
    m_artifactCard->setThemeOverrides({{"light", artifactColors}, {"dark", artifactColors},
        {"contrast", artifactColors}, {"radius", QJsonObject{{"control", 6}}}});

    const QColor zipTileBg = badgeTint(artifactBg, colors.accentDefault, dark, highContrast);
    const QJsonObject zipTileColors{{"bgLayer", colorSpec(zipTileBg)}};
    m_artifactIconBox->setThemeOverrides({{"light", zipTileColors}, {"dark", zipTileColors},
        {"contrast", zipTileColors}, {"radius", QJsonObject{{"control", 5}}}});
    m_artifactIcon->setColor(colors.accentDefault);

    m_artifactStateIcon->setColor(colors.systemSuccess);
    const QJsonObject stateTextColors{{"textPrimary", colorSpec(colors.systemSuccess)}};
    m_artifactState->setThemeOverrides(
        {{"light", stateTextColors}, {"dark", stateTextColors}, {"contrast", stateTextColors}});

    const QFont strongBase = themeFont(Typography::FontRole::BodyStrong).toQFont();
    const QFont captionFont = themeFont(Typography::FontRole::Caption).toQFont();
    const qreal scale = strongBase.pixelSize() / 14.0;

    QFont titleFont(strongBase);
    titleFont.setPixelSize(qMax(1, qRound(13.0 * scale)));
    m_title->setFont(titleFont);

    QFont artifactNameFont(strongBase);
    artifactNameFont.setPixelSize(qMax(1, captionFont.pixelSize()));
    m_artifactName->setFont(artifactNameFont);

    QFont artifactMetaFont(captionFont);
    artifactMetaFont.setPixelSize(qMax(1, qRound(11.0 * scale)));
    m_artifactSize->setFont(artifactMetaFont);

    QFont artifactStateFont(strongBase);
    artifactStateFont.setPixelSize(qMax(1, qRound(11.0 * scale)));
    m_artifactState->setFont(artifactStateFont);

    m_message->setFont(captionFont);

    QFont actionFont(strongBase);
    actionFont.setPixelSize(qMax(1, captionFont.pixelSize()));
    m_action->setFont(actionFont);

    m_title->setMinimumHeight(qMax(24, m_title->fontMetrics().height()));
    m_message->setMinimumHeight(qMax(16, m_message->fontMetrics().height()));
    m_action->setFixedSize(m_action->sizeHint().width(), qMax(24, m_action->sizeHint().height()));
    update();
}

} // namespace fluent::gallery

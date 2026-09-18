#include "FileDropZone.h"

#include <QAccessible>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFocusEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QVariantAnimation>

#include "components/basicinput/Button.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/textfields/Label.h"

namespace fluent::basicinput {
namespace {
constexpr int kTileSize = 56;
constexpr int kIconSize = 24;
constexpr int kButtonHeight = 36;
constexpr int kTransitionDuration = 200;
constexpr qreal kCornerRadius = 12.0;

int contentMargin(int width)
{
    return width < 400 ? 24 : 32;
}

int labelHeight(const textfields::Label* label, int width)
{
    if (label->text().isEmpty())
        return 0;
    return qMax(label->fontMetrics().height(), label->heightForWidth(qMax(1, width)));
}

QColor blend(const QColor& from, const QColor& to, qreal progress)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * progress,
                            from.greenF() + (to.greenF() - from.greenF()) * progress,
                            from.blueF() + (to.blueF() - from.blueF()) * progress,
                            from.alphaF() + (to.alphaF() - from.alphaF()) * progress);
}

textfields::Label* makeLabel(QWidget* parent, const QString& name, Typography::FontRole role)
{
    auto* label = new textfields::Label(parent);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    label->setTextElideMode(Qt::ElideNone);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    label->setFluentTypography(role);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    return label;
}
} // namespace

FileDropZone::FileDropZone(QWidget* parent)
    : QWidget(parent), m_title(tr("Drop files here")),
      m_description(tr("or choose files from your computer")), m_browseText(tr("Choose files"))
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::NoFocus);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    m_icon = new FontIcon(Typography::Icons::Upload, this);
    m_icon->setIconSize(kIconSize);
    m_icon->setFixedSize(kIconSize, kIconSize);
    m_titleLabel =
        makeLabel(this, QStringLiteral("FileDropZoneTitle"), Typography::FontRole::Subtitle);
    m_descriptionLabel =
        makeLabel(this, QStringLiteral("FileDropZoneDescription"), Typography::FontRole::Body);
    m_hintLabel =
        makeLabel(this, QStringLiteral("FileDropZoneHint"), Typography::FontRole::Caption);
    m_browseButton = new Button(m_browseText, this);
    m_browseButton->setObjectName(QStringLiteral("FileDropZoneBrowse"));
    m_browseButton->setFluentStyle(Button::Accent);
    m_browseButton->setFixedHeight(kButtonHeight);
    m_browseButton->setFocusPolicy(Qt::StrongFocus);
    m_browseButton->setFocusVisual(false);
    m_browseButton->installEventFilter(this);
    setFocusProxy(m_browseButton);
    connect(m_browseButton, &Button::clicked, this, [this] {
        if (isEnabled() && !m_dragActive)
            emit browseRequested();
    });

    m_transition = new QVariantAnimation(this);
    m_transition->setObjectName(QStringLiteral("FileDropZoneStateTransition"));
    m_transition->setEasingCurve(themeAnimation().decelerate);
    connect(m_transition, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        const qreal progress = value.toReal();
        for (size_t i = 0; i < m_visual.size(); ++i)
            m_visual[i] =
                m_transitionStart[i] + (m_transitionEnd[i] - m_transitionStart[i]) * progress;
        applyVisualState();
    });
    updatePresentation();
}

void FileDropZone::setTitle(const QString& title)
{
    if (m_title == title)
        return;
    m_title = title;
    updatePresentation();
    emit titleChanged(m_title);
}

void FileDropZone::setDescription(const QString& description)
{
    if (m_description == description)
        return;
    m_description = description;
    updatePresentation();
    emit descriptionChanged(m_description);
}

void FileDropZone::setHintText(const QString& text)
{
    if (m_hintText == text)
        return;
    m_hintText = text;
    updatePresentation();
    emit hintTextChanged(m_hintText);
}

void FileDropZone::setBrowseText(const QString& text)
{
    if (m_browseText == text)
        return;
    m_browseText = text;
    updatePresentation();
    emit browseTextChanged(m_browseText);
}

void FileDropZone::setErrorMessage(const QString& message)
{
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    updatePresentation();
    transitionToState();
    const QPointer<FileDropZone> guard(this);
    if (isVisible() && !m_errorMessage.isEmpty()) {
        fluentSendAccessibleAnnouncement(this, m_errorMessage,
                                         FluentAccessibleAnnouncementPoliteness::Polite);
        if (!guard)
            return;
    }
    emit errorMessageChanged(m_errorMessage);
}

void FileDropZone::clearError()
{
    setErrorMessage(QString());
}

QSize FileDropZone::sizeHint() const
{
    return QSize(640, heightForWidth(640));
}

QSize FileDropZone::minimumSizeHint() const
{
    return QSize(240, 288);
}

bool FileDropZone::hasHeightForWidth() const
{
    return true;
}

int FileDropZone::heightForWidth(int width) const
{
    return qMax(288, contentHeight(width) + contentMargin(width) * 2);
}

int FileDropZone::contentHeight(int width) const
{
    const int availableWidth = qMax(1, width - contentMargin(width) * 2);
    int height = kTileSize + 16 + labelHeight(m_titleLabel, availableWidth);
    if (!m_descriptionLabel->text().isEmpty())
        height += 4 + labelHeight(m_descriptionLabel, availableWidth);
    height += 16 + kButtonHeight;
    if (!m_hintLabel->text().isEmpty())
        height += 12 + labelHeight(m_hintLabel, availableWidth);
    return height;
}

void FileDropZone::layoutContent()
{
    const int padding = contentMargin(width());
    const int availableWidth = qMax(1, width() - padding * 2);
    int y = qMax(padding, (height() - contentHeight(width())) / 2);
    m_iconTileRect = QRect((width() - kTileSize) / 2, y, kTileSize, kTileSize);
    y += kTileSize + 16;
    const int titleHeight = labelHeight(m_titleLabel, availableWidth);
    m_titleLabel->setGeometry(padding, y, availableWidth, titleHeight);
    y += titleHeight;
    if (!m_descriptionLabel->text().isEmpty()) {
        y += 4;
        const int descriptionHeight = labelHeight(m_descriptionLabel, availableWidth);
        m_descriptionLabel->setGeometry(padding, y, availableWidth, descriptionHeight);
        y += descriptionHeight;
    }
    y += 16;
    const int buttonWidth = qMin(availableWidth, m_browseButton->sizeHint().width() + 8);
    m_browseButton->setGeometry((width() - buttonWidth) / 2, y, buttonWidth, kButtonHeight);
    y += kButtonHeight;
    if (!m_hintLabel->text().isEmpty()) {
        y += 12;
        m_hintLabel->setGeometry(padding, y, availableWidth,
                                 labelHeight(m_hintLabel, availableWidth));
    }
    applyVisualState();
}

void FileDropZone::updatePresentation()
{
    const bool error = !m_dragActive && !m_errorMessage.isEmpty();
    const QString dragTitle = m_dragFileCount == 1
                                  ? tr("Release to add 1 file")
                                  : tr("Release to add %1 files").arg(m_dragFileCount);
    m_titleLabel->setText(m_dragActive ? dragTitle : m_title);
    m_descriptionLabel->setText(error ? m_errorMessage : m_description);
    m_descriptionLabel->setVisible(!m_descriptionLabel->text().isEmpty());
    m_hintLabel->setText(m_hintText);
    m_hintLabel->setVisible(!m_hintText.isEmpty());
    m_browseButton->setText(m_browseText);
    m_browseButton->setEnabled(isEnabled() && !m_dragActive);
    const auto& colors = themeColorsRef();
    m_titleLabel->setTextColorRole(isEnabled() ? textfields::Label::TextColorRole::Primary
                                               : textfields::Label::TextColorRole::Disabled);
    m_descriptionLabel->setTextColorRole(!isEnabled() ? textfields::Label::TextColorRole::Disabled
                                         : error      ? textfields::Label::TextColorRole::Primary
                                                 : textfields::Label::TextColorRole::Secondary);
    m_hintLabel->setTextColorRole(isEnabled() ? textfields::Label::TextColorRole::Secondary
                                              : textfields::Label::TextColorRole::Disabled);
    const QColor critical = colors.systemCritical;
    const QJsonObject criticalText{
        {"textPrimary", QString::asprintf("#%02X%02X%02X%02X", critical.red(), critical.green(),
                                          critical.blue(), critical.alpha())}};
    m_descriptionLabel->setThemeOverrides(error ? QJsonObject{{"light", criticalText},
                                                              {"dark", criticalText},
                                                              {"contrast", criticalText}}
                                                : QJsonObject{});
    m_icon->setGlyph(error ? Typography::Icons::ErrorIcon : Typography::Icons::Upload);
    m_icon->setColor(!isEnabled() ? colors.textDisabled
                     : error      ? colors.systemCritical
                                  : colors.textAccentPrimary);
    updateAccessibleText();
    layoutContent();
    updateGeometry();
    update();
}

void FileDropZone::updateAccessibleText()
{
    const QString name = m_titleLabel->text();
    if (accessibleName().isEmpty() || accessibleName() == m_autoAccessibleName)
        setAccessibleName(name);
    m_autoAccessibleName = name;
    QStringList parts;
    if (!m_descriptionLabel->text().isEmpty())
        parts.append(m_descriptionLabel->text());
    if (!m_hintText.isEmpty())
        parts.append(m_hintText);
    const QString description = parts.join(QLatin1Char('\n'));
    if (accessibleDescription().isEmpty() || accessibleDescription() == m_autoAccessibleDescription)
        setAccessibleDescription(description);
    m_autoAccessibleDescription = description;
}

void FileDropZone::setDragActive(bool active, int fileCount)
{
    if (m_dragActive == active && m_dragFileCount == fileCount)
        return;
    const bool changed = m_dragActive != active;
    m_dragActive = active;
    m_dragFileCount = fileCount;
    updatePresentation();
    transitionToState();
    const QPointer<FileDropZone> guard(this);
    if (active) {
        fluentSendAccessibleAnnouncement(this, m_titleLabel->text(),
                                         FluentAccessibleAnnouncementPoliteness::Polite);
        if (!guard)
            return;
    }
    if (changed)
        emit dragActiveChanged(m_dragActive);
}

void FileDropZone::transitionToState(bool animated)
{
    m_transition->stop();
    m_transitionEnd = {isEnabled() && m_hovered ? 1.0 : 0.0,
                       isEnabled() && m_dragActive ? 1.0 : 0.0,
                       !m_dragActive && !m_errorMessage.isEmpty() ? 1.0 : 0.0};
    if (!animated || !isVisible() || !isEnabled() || effectiveTheme() == HighContrast) {
        m_visual = m_transitionEnd;
        applyVisualState();
        return;
    }
    if (m_visual == m_transitionEnd)
        return;
    m_transitionStart = m_visual;
    m_transition->setStartValue(0.0);
    m_transition->setEndValue(1.0);
    detail::startMotionTransition(m_transition, kTransitionDuration);
}

void FileDropZone::applyVisualState()
{
    const int offset = (kTileSize - kIconSize) / 2;
    m_icon->move(m_iconTileRect.topLeft() + QPoint(offset, offset - qRound(3.0 * m_visual[1])));
    m_browseButton->setContentOpacity(1.0 - m_visual[1]);
    update();
}

bool FileDropZone::resetInteraction()
{
    const QPointer<FileDropZone> guard(this);
    m_hovered = false;
    m_keyboardFocusVisible = false;
    setDragActive(false);
    if (!guard)
        return false;
    transitionToState(false);
    return true;
}

void FileDropZone::onThemeUpdated()
{
    if (!m_titleLabel)
        return;
    // Resolve child fonts before measuring; batched theme notifications may
    // reach this parent before its children. zh_CN: 批量主题通知可能先到父项，
    // 所以先刷新子控件字体，避免按旧字号计算换行高度。
    m_titleLabel->onThemeUpdated();
    m_descriptionLabel->onThemeUpdated();
    m_hintLabel->onThemeUpdated();
    m_browseButton->onThemeUpdated();
    updatePresentation();
    transitionToState(false);
}

bool FileDropZone::event(QEvent* event)
{
    if (event->type() == QEvent::WindowDeactivate && !resetInteraction())
        return true;
    return QWidget::event(event);
}

bool FileDropZone::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_browseButton) {
        if (event->type() == QEvent::FocusIn) {
            const auto reason = static_cast<QFocusEvent*>(event)->reason();
            m_keyboardFocusVisible = reason == Qt::TabFocusReason ||
                                     reason == Qt::BacktabFocusReason ||
                                     reason == Qt::ShortcutFocusReason;
            update();
        } else if (event->type() == QEvent::FocusOut) {
            m_keyboardFocusVisible = false;
            update();
        } else if (event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (isEnabled() && !m_dragActive &&
                (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
                if (!key->isAutoRepeat())
                    m_browseButton->click();
                key->accept();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FileDropZone::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    const auto& colors = themeColorsRef();
    const bool highContrast = effectiveTheme() == HighContrast;
    const QColor accent = colors.textAccentPrimary;
    const QColor surface = colors.bgLayer;
    const QColor dropFill = highContrast ? surface : blend(surface, accent, 0.045);
    const QColor background = blend(surface, dropFill, m_visual[1]);
    const QColor neutralStroke = blend(colors.strokeCard, colors.strokeStrong, m_visual[0] * 0.55);
    const QColor stroke = blend(neutralStroke, accent, m_visual[1]);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(background);
    const qreal strokeWidth = highContrast ? 1.5 : 1.0 + m_visual[1] * 0.5;
    painter.setPen(QPen(stroke, strokeWidth));
    const QRectF card = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.drawRoundedRect(card, kCornerRadius, kCornerRadius);

    QColor tile = blend(surface, accent, 0.08 + m_visual[1] * 0.06);
    tile = blend(tile, colors.systemCriticalBg, m_visual[2]);
    if (!isEnabled())
        tile = colors.controlDisabled;
    if (highContrast)
        tile = surface;
    painter.setBrush(tile);
    painter.setPen(highContrast ? QPen(colors.strokeStrong, 1.0) : QPen(Qt::NoPen));
    const QRectF tileRect = QRectF(m_iconTileRect).translated(0, -3.0 * m_visual[1]);
    painter.drawRoundedRect(tileRect, kCornerRadius, kCornerRadius);

    if (isEnabled() && m_keyboardFocusVisible && m_browseButton->hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(colors.strokeFocusOuter, 2.0));
        painter.drawRoundedRect(card.adjusted(2, 2, -2, -2), kCornerRadius - 2, kCornerRadius - 2);
    }
}

void FileDropZone::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutContent();
}

void FileDropZone::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
    m_hovered = true;
    transitionToState();
}

void FileDropZone::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    transitionToState();
}

QList<QUrl> FileDropZone::localUrls(const QMimeData* data)
{
    if (!data || !data->hasUrls())
        return {};
    const QList<QUrl> urls = data->urls();
    for (const QUrl& url : urls) {
        if (!url.isValid() || !url.isLocalFile() || url.toLocalFile().isEmpty())
            return {};
    }
    return urls;
}

void FileDropZone::dragEnterEvent(QDragEnterEvent* event)
{
    const QList<QUrl> urls = localUrls(event->mimeData());
    if (!isEnabled() || urls.isEmpty() || !(event->possibleActions() & Qt::CopyAction)) {
        setDragActive(false);
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    const QPointer<FileDropZone> guard(this);
    setDragActive(true, urls.size());
    if (!guard || !isEnabled() || !m_dragActive)
        event->ignore();
}

void FileDropZone::dragMoveEvent(QDragMoveEvent* event)
{
    // The immutable drag payload is checked once on enter, then again on drop.
    // zh_CN: 不变的拖放载荷仅在进入和释放时检查，移动热路径不遍历文件列表。
    if (!isEnabled() || !m_dragActive || !(event->possibleActions() & Qt::CopyAction)) {
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

void FileDropZone::dragLeaveEvent(QDragLeaveEvent* event)
{
    setDragActive(false);
    event->accept();
}

void FileDropZone::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = localUrls(event->mimeData());
    const bool accepted = isEnabled() && m_dragActive && !urls.isEmpty() &&
                          (event->possibleActions() & Qt::CopyAction);
    const QPointer<FileDropZone> guard(this);
    setDragActive(false);
    if (!guard || !accepted || !isEnabled() || !isVisible()) {
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    emit filesDropped(urls);
}

void FileDropZone::hideEvent(QHideEvent* event)
{
    if (!resetInteraction())
        return;
    QWidget::hideEvent(event);
}

void FileDropZone::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled() && !resetInteraction())
            return;
        updatePresentation();
        transitionToState(false);
    } else if (event->type() == QEvent::FontChange ||
               event->type() == QEvent::LayoutDirectionChange) {
        layoutContent();
        updateGeometry();
    }
}

} // namespace fluent::basicinput

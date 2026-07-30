#include "StatusInfoSamples.h"

#include <cmath>
#include <functional>

#include <QAction>
#include <QBoxLayout>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QMargins>
#include <QMetaEnum>
#include <QPainter>
#include <QPoint>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/status_info/Avatar.h"
#include "components/status_info/InfoBadge.h"
#include "components/status_info/InfoBar.h"
#include "components/status_info/ProgressBar.h"
#include "components/status_info/ProgressRing.h"
#include "components/status_info/Shimmer.h"
#include "components/status_info/Toast.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "components/textfields/NumberBox.h"
#include "design/Typography.h"
#include "SampleBuilders.h"
#include "view/support/GalleryToast.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::status_info::Avatar;
using fluent::status_info::InfoBadge;
using fluent::status_info::InfoBar;
using fluent::status_info::ProgressBar;
using fluent::status_info::ProgressRing;
using fluent::status_info::Shimmer;
using fluent::status_info::ShimmerPainter;
using fluent::status_info::Toast;
using fluent::status_info::ToolTip;
using fluent::textfields::Label;
using fluent::textfields::NumberBox;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

class StatusInfoSampleSurface : public QWidget, public fluent::FluentElement {
public:
    explicit StatusInfoSampleSurface(QWidget* parent = nullptr, int spacing = 12)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 16);
        layout->setSpacing(spacing);
        layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(themeColors().strokeCard);
        painter.setBrush(themeColors().bgCanvas);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1),
                                themeRadius().overlay,
                                themeRadius().overlay);
    }
};

class SampleStatusPill : public QWidget, public fluent::FluentElement {
public:
    explicit SampleStatusPill(const QString& text, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_label(new Label(text, this))
    {
        setAutoFillBackground(false);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedHeight(28);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 3, 10, 3);
        layout->setSpacing(0);

        m_label->setFluentTypography(Typography::FontRole::Caption);
        m_label->setAlignment(Qt::AlignCenter);
        m_label->setWordWrap(false);
        layout->addWidget(m_label, 0, Qt::AlignCenter);
        onThemeUpdated();
    }

    void setText(const QString& text) { m_label->setText(text); }

    void onThemeUpdated() override
    {
        const auto colors = themeColors();
        m_label->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(colors.textSecondary.name(QColor::HexArgb)));
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto colors = themeColors();
        painter.setPen(colors.strokeCard);
        painter.setBrush(colors.controlSecondary);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1),
                                themeRadius().control,
                                themeRadius().control);
    }

private:
    Label* m_label = nullptr;
};

QWidget* sampleSurface(QWidget* parent, int spacing = 12)
{
    return new StatusInfoSampleSurface(parent, spacing);
}

QBoxLayout* boxLayout(QWidget* widget)
{
    return qobject_cast<QBoxLayout*>(widget->layout());
}

Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    label->setWordWrap(true);
    label->setTextColorRole(Label::TextColorRole::Primary);  // QSS-proof on the styled preview surface
    return label;
}

Label* makeCaptionLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Caption);
    label->setAlignment(Qt::AlignCenter);
    label->setTextColorRole(Label::TextColorRole::Primary);  // QSS-proof on the styled preview surface
    return label;
}

Label* makeValueStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = makeStatusLabel(parent, text);
    label->setWordWrap(false);
    label->setTextElideMode(Qt::ElideNone);
    label->setFixedWidth(124);
    return label;
}

Button* sampleButton(QWidget* parent, const QString& text)
{
    auto* button = new Button(text, parent);
    button->setFluentSize(Button::Small);
    return button;
}

SampleStatusPill* makeStatusPill(QWidget* parent, const QString& text)
{
    return new SampleStatusPill(text, parent);
}

QWidget* labeledColumn(QWidget* parent,
                       const QString& labelText,
                       QWidget* content,
                       const QString& captionText = QString())
{
    auto* cell = verticalGroup(parent, 6);
    cell->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* layout = boxLayout(cell);
    content->setParent(cell);
    layout->addWidget(content, 0, Qt::AlignHCenter);

    auto* label = makeCaptionLabel(cell, labelText);
    label->setMinimumWidth(qMax(72, content->sizeHint().width()));
    layout->addWidget(label, 0, Qt::AlignHCenter);

    if (!captionText.isEmpty()) {
        auto* caption = makeCaptionLabel(cell, captionText);
        caption->setWordWrap(true);
        caption->setMaximumWidth(130);
        layout->addWidget(caption, 0, Qt::AlignHCenter);
    }
    return cell;
}

InfoBadge* makeBadge(QWidget* parent,
                     InfoBadge::InfoBadgeDisplayMode mode,
                     InfoBadge::InfoBadgeStatus status,
                     int value = -1,
                     const QString& glyph = QString())
{
    auto* badge = new InfoBadge(parent);
    badge->setDisplayMode(mode);
    badge->setStatus(status);
    badge->setValue(value);
    badge->setIconGlyph(glyph);
    return badge;
}

InfoBar* makeInfoBar(QWidget* parent,
                     InfoBar::InfoBarSeverity severity,
                     const QString& title,
                     const QString& message,
                     bool singleLine = true)
{
    auto* bar = new InfoBar(parent);
    bar->setCloseButtonAccessibleName(QStringLiteral("Dismiss notification"));
    bar->setPreferredWidth(520);
    bar->setSeverity(severity);
    bar->setTitle(title);
    bar->setMessage(message);
    bar->setSingleLine(singleLine);
    bar->setIsOpen(true);
    return bar;
}

void keepInfoBarOpenWithToast(InfoBar* bar, Label* statusLabel = nullptr)
{
    QObject::connect(bar, &InfoBar::closed, bar, [bar, statusLabel]() {
        bar->setIsOpen(true);
        if (auto* parent = bar->parentWidget()) {
            if (auto* layout = parent->layout())
                layout->activate();
        }

        if (statusLabel) {
            const int closeCount = statusLabel->property("closeCount").toInt() + 1;
            statusLabel->setProperty("closeCount", closeCount);
            statusLabel->setText(QStringLiteral("Close requests: %1").arg(closeCount));
        }

        showGalleryToast(bar, QStringLiteral("InfoBar close requested"));
    });
}

QWidget* progressBarRow(QWidget* parent,
                        const QString& labelText,
                        ProgressBar* progressBar)
{
    QWidget* row = horizontalGroup(parent, 12);
    auto* label = makeStatusLabel(row, labelText);
    label->setFixedWidth(96);
    progressBar->setParent(row);
    boxLayout(row)->addWidget(label, 0, Qt::AlignVCenter);
    boxLayout(row)->addWidget(progressBar, 0, Qt::AlignVCenter);
    return row;
}

QWidget* progressRingColumn(QWidget* parent,
                            const QString& labelText,
                            ProgressRing* ring)
{
    return labeledColumn(parent, labelText, ring);
}

QVector<GallerySample> avatarSamples()
{
    return {
        makeSample(
            QStringLiteral("avatar-initials-sizes"),
            QStringLiteral("Initials and sizes"),
            QStringLiteral("Avatar generates a compact initials fallback from the caller-provided name and offers four token-aligned sizes."),
            QStringLiteral("auto* small = new Avatar(\"Ada Lovelace\", this);\n"
                           "small->setAvatarSize(Avatar::AvatarSize::Small);\n"
                           "\n"
                           "auto* medium = new Avatar(\"Grace Hopper\", this);\n"
                           "medium->setAvatarSize(Avatar::AvatarSize::Medium);\n"
                           "\n"
                           "auto* large = new Avatar(\"Lin Chen\", this);\n"
                           "large->setAvatarSize(Avatar::AvatarSize::Large);\n"
                           "\n"
                           "auto* extraLarge = new Avatar(\"Sam Rivera\", this);\n"
                           "extraLarge->setAvatarSize(\n"
                           "    Avatar::AvatarSize::ExtraLarge);"),
            [](QWidget* parent) {
                QWidget* surface = sampleSurface(parent);
                QWidget* row = horizontalGroup(surface, 20);

                struct AvatarCase {
                    QString name;
                    QString caption;
                    Avatar::AvatarSize size;
                };
                const AvatarCase cases[] = {
                    {QStringLiteral("Ada Lovelace"),
                     QStringLiteral("Small"),
                     Avatar::AvatarSize::Small},
                    {QStringLiteral("Grace Hopper"),
                     QStringLiteral("Medium"),
                     Avatar::AvatarSize::Medium},
                    {QStringLiteral("Lin Chen"),
                     QStringLiteral("Large"),
                     Avatar::AvatarSize::Large},
                    {QStringLiteral("Sam Rivera"),
                     QStringLiteral("Extra large"),
                     Avatar::AvatarSize::ExtraLarge},
                };
                for (const AvatarCase& avatarCase : cases) {
                    auto* avatar = new Avatar(avatarCase.name, row);
                    avatar->setAvatarSize(avatarCase.size);
                    boxLayout(row)->addWidget(labeledColumn(
                        row, avatarCase.caption, avatar));
                }
                boxLayout(surface)->addWidget(row, 0, Qt::AlignLeft);
                return surface;
            }),
        makeSample(
            QStringLiteral("avatar-image-presence"),
            QStringLiteral("Image, shape, and presence"),
            QStringLiteral("A high-DPI pixmap replaces initials, while the composed InfoBadge communicates presence without adding text."),
            QStringLiteral("auto* profile = new Avatar(\"Product account\", this);\n"
                           "profile->setAvatarSize(\n"
                           "    Avatar::AvatarSize::ExtraLarge);\n"
                           "profile->setImage(QPixmap(\":/app/assets/app-icon.png\"));\n"
                           "profile->setPresence(\n"
                           "    Avatar::PresenceStatus::Available);\n"
                           "\n"
                           "auto* away = new Avatar(\"Alex Morgan\", this);\n"
                           "away->setAvatarSize(Avatar::AvatarSize::Large);\n"
                           "away->setShape(Avatar::AvatarShape::Square);\n"
                           "away->setPresence(Avatar::PresenceStatus::Away);\n"
                           "\n"
                           "auto* busy = new Avatar(\"Jordan Lee\", this);\n"
                           "busy->setAvatarSize(Avatar::AvatarSize::Large);\n"
                           "busy->setPresence(Avatar::PresenceStatus::Busy);\n"
                           "\n"
                           "auto* offline = new Avatar(\"Taylor Reed\", this);\n"
                           "offline->setAvatarSize(Avatar::AvatarSize::Large);\n"
                           "offline->setPresence(\n"
                           "    Avatar::PresenceStatus::Offline);"),
            [](QWidget* parent) {
                QWidget* surface = sampleSurface(parent);
                QWidget* row = horizontalGroup(surface, 20);

                auto* profile = new Avatar(
                    QStringLiteral("Product account"), row);
                profile->setAvatarSize(
                    Avatar::AvatarSize::ExtraLarge);
                profile->setImage(
                    QPixmap(QStringLiteral(":/app/assets/app-icon.png")));
                profile->setPresence(
                    Avatar::PresenceStatus::Available);
                boxLayout(row)->addWidget(labeledColumn(
                    row, QStringLiteral("Available"), profile));

                auto* away = new Avatar(
                    QStringLiteral("Alex Morgan"), row);
                away->setAvatarSize(Avatar::AvatarSize::Large);
                away->setShape(Avatar::AvatarShape::Square);
                away->setPresence(Avatar::PresenceStatus::Away);
                boxLayout(row)->addWidget(labeledColumn(
                    row, QStringLiteral("Away"), away));

                auto* busy = new Avatar(
                    QStringLiteral("Jordan Lee"), row);
                busy->setAvatarSize(Avatar::AvatarSize::Large);
                busy->setPresence(Avatar::PresenceStatus::Busy);
                boxLayout(row)->addWidget(labeledColumn(
                    row, QStringLiteral("Busy"), busy));

                auto* offline = new Avatar(
                    QStringLiteral("Taylor Reed"), row);
                offline->setAvatarSize(Avatar::AvatarSize::Large);
                offline->setPresence(Avatar::PresenceStatus::Offline);
                boxLayout(row)->addWidget(labeledColumn(
                    row, QStringLiteral("Offline"), offline));

                boxLayout(surface)->addWidget(row, 0, Qt::AlignLeft);
                return surface;
            })
    };
}

enum class ToolTipPlacement { Above, Right };

class HoverToolTipController : public QObject {
public:
    HoverToolTipController(QWidget* target,
                           ToolTip* toolTip,
                           ToolTipPlacement placement = ToolTipPlacement::Above)
        : QObject(target)
        , m_target(target)
        , m_toolTip(toolTip)
        , m_placement(placement)
    {
        m_toolTip->setThemeSource(target);
        m_toolTip->hide();
        target->installEventFilter(this);
    }

    HoverToolTipController(QWidget* target,
                           const QString& text,
                           ToolTipPlacement placement = ToolTipPlacement::Above,
                           std::function<void(ToolTip*)> configure = {})
        : HoverToolTipController(target, new ToolTip(), placement)
    {
        m_toolTip->setText(text);
        if (configure)
            configure(m_toolTip);
    }

    ~HoverToolTipController() override
    {
        delete m_toolTip;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_target) {
            if (event->type() == QEvent::Enter) {
                m_toolTip->setThemeSource(m_target);
                m_toolTip->adjustSize();
                const QSize tipSize = m_toolTip->sizeHint();
                const QPoint localAnchor = m_placement == ToolTipPlacement::Right
                    ? QPoint(m_target->width() + 8, m_target->height() / 2 - tipSize.height() / 2)
                    : QPoint(m_target->width() / 2 - tipSize.width() / 2, -tipSize.height() - 4);
                m_toolTip->move(m_target->mapToGlobal(localAnchor));
                m_toolTip->show();
            } else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
                m_toolTip->hide();
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QWidget* m_target = nullptr;
    ToolTip* m_toolTip = nullptr;
    ToolTipPlacement m_placement = ToolTipPlacement::Above;
};

QVector<GallerySample> infoBadgeSamples()
{
    return {
        makeSample(QStringLiteral("info-badge-display-modes"),
                   QStringLiteral("Display modes"),
                   QStringLiteral("Auto resolves to a dot, icon, or value based on the badge content."),
                   QStringLiteral("auto* dotBadge = new InfoBadge(this);\n"
                                  "\n"
                                  "auto* iconBadge = new InfoBadge(this);\n"
                                  "iconBadge->setIconGlyph(Typography::Icons::Mail);\n"
                                  "\n"
                                  "auto* valueBadge = new InfoBadge(this);\n"
                                  "valueBadge->setValue(7);\n"
                                  "\n"
                                  "auto* explicitDot = new InfoBadge(this);\n"
                                  "explicitDot->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Dot);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       QWidget* row = horizontalGroup(surface, 28);
                       boxLayout(surface)->addWidget(row);

                       auto* dotBadge = new InfoBadge(row);
                       auto* iconBadge = new InfoBadge(row);
                       iconBadge->setIconGlyph(Typography::Icons::Mail);
                       auto* valueBadge = new InfoBadge(row);
                       valueBadge->setValue(7);
                       auto* explicitDot = new InfoBadge(row);
                       explicitDot->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Dot);

                       boxLayout(row)->addWidget(labeledColumn(row, QStringLiteral("Auto dot"), dotBadge));
                       boxLayout(row)->addWidget(labeledColumn(row, QStringLiteral("Auto icon"), iconBadge));
                       boxLayout(row)->addWidget(labeledColumn(row, QStringLiteral("Auto value"), valueBadge));
                       boxLayout(row)->addWidget(labeledColumn(row, QStringLiteral("Explicit dot"), explicitDot));
                       return surface;
                   }),
        makeSample(QStringLiteral("info-badge-status-colors"),
                   QStringLiteral("Status colors"),
                   QStringLiteral("Status selects the semantic color used by dot, icon, and value badges."),
                   QStringLiteral("auto* successBadge = new InfoBadge(this);\n"
                                  "successBadge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Value);\n"
                                  "successBadge->setValue(5);\n"
                                  "successBadge->setStatus(InfoBadge::InfoBadgeStatus::Success);\n"
                                  "\n"
                                  "auto* criticalBadge = new InfoBadge(this);\n"
                                  "criticalBadge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Value);\n"
                                  "criticalBadge->setValue(5);\n"
                                  "criticalBadge->setStatus(InfoBadge::InfoBadgeStatus::Critical);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       QWidget* row = horizontalGroup(surface, 22);
                       boxLayout(surface)->addWidget(row);

                       struct StatusBadge {
                           QString label;
                           InfoBadge::InfoBadgeStatus status;
                       };
                       const QVector<StatusBadge> statuses{
                           {QStringLiteral("Info"), InfoBadge::InfoBadgeStatus::Informational},
                           {QStringLiteral("Attention"), InfoBadge::InfoBadgeStatus::Attention},
                           {QStringLiteral("Caution"), InfoBadge::InfoBadgeStatus::Caution},
                           {QStringLiteral("Success"), InfoBadge::InfoBadgeStatus::Success},
                           {QStringLiteral("Critical"), InfoBadge::InfoBadgeStatus::Critical}
                       };

                       for (const StatusBadge& item : statuses) {
                           auto* badge = makeBadge(row,
                                                   InfoBadge::InfoBadgeDisplayMode::Value,
                                                   item.status,
                                                   5);
                           boxLayout(row)->addWidget(labeledColumn(row, item.label, badge));
                       }
                       return surface;
                   }),
        makeSample(QStringLiteral("info-badge-custom-metrics"),
                   QStringLiteral("Custom metrics and colors"),
                   QStringLiteral("Badge sizing, padding, and color overrides let a badge sit on top of another control."),
                   QStringLiteral("auto* inboxButton = new Button(\"Inbox\", this);\n"
                                  "\n"
                                  "auto* badge = new InfoBadge(host);\n"
                                  "badge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);\n"
                                  "badge->setIconGlyph(Typography::Icons::Mail);\n"
                                  "badge->setCustomBackgroundColor(QColor(\"#C42B1C\"));\n"
                                  "badge->setCustomTextColor(Qt::white);\n"
                                  "badge->setBadgeHeight(18);\n"
                                  "badge->setIconGlyphSize(12);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       QWidget* host = new QWidget(surface);
                       host->setFixedSize(170, 70);
                       auto* inboxButton = new Button(QStringLiteral("Inbox"), host);
                       inboxButton->setGeometry(0, 18, 146, 34);

                       auto* badge = new InfoBadge(host);
                       badge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
                       badge->setIconGlyph(Typography::Icons::Mail);
                       badge->setCustomBackgroundColor(QColor(QStringLiteral("#C42B1C")));
                       badge->setCustomTextColor(Qt::white);
                       badge->setBadgeHeight(18);
                       badge->setIconGlyphSize(12);
                       badge->resize(badge->sizeHint());
                       badge->move(132, 8);

                       boxLayout(surface)->addWidget(host, 0, Qt::AlignLeft);
                       return surface;
                   }),
        makeSample(QStringLiteral("info-badge-accessibility"),
                   QStringLiteral("Accessible value and visibility"),
                   QStringLiteral("A value badge remains a non-focusable child of its accessible parent and reports value or visibility changes without duplicating business text."),
                   QStringLiteral("auto* inbox = new Button(\"Inbox\", this);\n"
                                  "inbox->setAccessibleName(\"Inbox\");\n"
                                  "\n"
                                  "auto* badge = new InfoBadge(inbox);\n"
                                  "badge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Value);\n"
                                  "badge->setAccessibleName(\"Unread messages\");\n"
                                  "badge->setValue(3);\n"
                                  "\n"
                                  "auto* increment = new Button(\"Increment\", this);\n"
                                  "auto* toggle = new Button(\"Toggle badge\", this);\n"
                                  "auto* status = new Label(\"Unread value: 3\", this);\n"
                                  "\n"
                                  "connect(increment, &Button::clicked, this, [=]() {\n"
                                  "    badge->setValue(badge->value() + 1);\n"
                                  "    status->setText(QString(\"Unread value: %1\").arg(badge->value()));\n"
                                  "});\n"
                                  "connect(toggle, &Button::clicked, this, [=]() {\n"
                                  "    const bool showBadge = badge->isHidden();\n"
                                  "    badge->setVisible(showBadge);\n"
                                  "    status->setText(showBadge ? \"Badge visible\" : \"Badge hidden\");\n"
                                  "});"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       auto* inbox = new Button(
                           QStringLiteral("Inbox"), surface);
                       inbox->setAccessibleName(
                           QStringLiteral("Inbox"));
                       inbox->setFixedSize(164, 42);

                       auto* badge = new InfoBadge(inbox);
                       badge->setObjectName(
                           QStringLiteral(
                               "galleryInfoBadgeAccessibleValue"));
                       badge->setDisplayMode(
                           InfoBadge::InfoBadgeDisplayMode::Value);
                       badge->setAccessibleName(
                           QStringLiteral("Unread messages"));
                       badge->setValue(3);
                       badge->resize(badge->sizeHint());
                       badge->move(
                           inbox->width() - badge->width() - 6,
                           4);

                       QWidget* row = horizontalGroup(surface, 8);
                       auto* increment = sampleButton(
                           row, QStringLiteral("Increment"));
                       increment->setObjectName(
                           QStringLiteral(
                               "galleryInfoBadgeAccessibleIncrement"));
                       auto* toggle = sampleButton(
                           row, QStringLiteral("Toggle badge"));
                       toggle->setObjectName(
                           QStringLiteral(
                               "galleryInfoBadgeAccessibleToggle"));
                       auto* status = makeStatusLabel(
                           surface,
                           QStringLiteral("Unread value: 3"));
                       status->setObjectName(
                           QStringLiteral(
                               "galleryInfoBadgeAccessibleStatus"));

                       QObject::connect(
                           increment,
                           &Button::clicked,
                           badge,
                           [badge, status]() {
                           badge->setValue(badge->value() + 1);
                           status->setText(
                               QStringLiteral("Unread value: %1")
                                   .arg(badge->value()));
                       });
                       QObject::connect(
                           toggle,
                           &Button::clicked,
                           badge,
                           [badge, status]() {
                           const bool showBadge = badge->isHidden();
                           badge->setVisible(showBadge);
                           status->setText(
                               showBadge
                               ? QStringLiteral("Badge visible")
                               : QStringLiteral("Badge hidden"));
                       });

                       boxLayout(row)->addWidget(increment);
                       boxLayout(row)->addWidget(toggle);
                       boxLayout(surface)->addWidget(
                           inbox, 0, Qt::AlignLeft);
                       boxLayout(surface)->addWidget(row);
                       boxLayout(surface)->addWidget(status);
                       return surface;
                   })
    };
}

QVector<GallerySample> infoBarSamples()
{
    return {
        makeSample(QStringLiteral("info-bar-severities"),
                   QStringLiteral("Severity"),
                   QStringLiteral("Severity changes the leading icon and background treatment for status messages."),
                   QStringLiteral("auto* info = new InfoBar(this);\n"
                                  "info->setSeverity(InfoBar::Informational);\n"
                                  "info->setTitle(\"Update available\");\n"
                                  "info->setMessage(\"Version 3.2 is ready.\");\n"
                                  "\n"
                                  "auto* error = new InfoBar(this);\n"
                                  "error->setSeverity(InfoBar::Error);\n"
                                  "error->setTitle(\"Upload failed\");\n"
                                  "error->setMessage(\"The document could not be saved.\");\n"
                                  "\n"
                                  "keepInfoBarOpenWithToast(info);\n"
                                  "keepInfoBarOpenWithToast(error);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent, 10);
                       const QVector<InfoBar*> bars{
                           makeInfoBar(surface,
                                       InfoBar::Informational,
                                       QStringLiteral("Update available"),
                                       QStringLiteral("Version 3.2 is ready.")),
                           makeInfoBar(surface,
                                       InfoBar::Success,
                                       QStringLiteral("Saved"),
                                       QStringLiteral("All changes were saved.")),
                           makeInfoBar(surface,
                                       InfoBar::Warning,
                                       QStringLiteral("Storage almost full"),
                                       QStringLiteral("Clear space before syncing.")),
                           makeInfoBar(surface,
                                       InfoBar::Error,
                                       QStringLiteral("Upload failed"),
                                       QStringLiteral("The document could not be saved."))
                       };
                       for (auto* bar : bars) {
                           keepInfoBarOpenWithToast(bar);
                           boxLayout(surface)->addWidget(bar);
                       }
                       return surface;
                   }),
        makeSample(QStringLiteral("info-bar-action-layout"),
                   QStringLiteral("Multi-line action"),
                   QStringLiteral("Long messages switch to multi-line layout and can host an action widget."),
                   QStringLiteral("auto* retryButton = new Button(\"Retry\", this);\n"
                                  "\n"
                                  "auto* infoBar = new InfoBar(this);\n"
                                  "infoBar->setSeverity(InfoBar::Warning);\n"
                                  "infoBar->setSingleLine(false);\n"
                                  "infoBar->setTitle(\"Sync paused\");\n"
                                  "infoBar->setMessage(\"Some files need attention before the next sync can finish.\");\n"
                                  "infoBar->setActionWidget(retryButton);\n"
                                  "keepInfoBarOpenWithToast(infoBar);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       auto* retryButton = new Button(QStringLiteral("Retry"), surface);
                       auto* infoBar = makeInfoBar(surface,
                                                   InfoBar::Warning,
                                                   QStringLiteral("Sync paused"),
                                                   QStringLiteral("Some files need attention before the next sync can finish."),
                                                   false);
                       infoBar->setActionWidget(retryButton);
                       keepInfoBarOpenWithToast(infoBar);
                       boxLayout(surface)->addWidget(infoBar);
                       return surface;
                   }),
        makeSample(QStringLiteral("info-bar-open-close"),
                   QStringLiteral("Close notification"),
                   QStringLiteral("closed() hides the InfoBar; a command can reopen it when the message is needed again."),
                   QStringLiteral("auto* status = new Label(\"Visible\", this);\n"
                                  "auto* openButton = new Button(\"Show again\", this);\n"
                                  "\n"
                                  "auto* infoBar = new InfoBar(this);\n"
                                  "infoBar->setTitle(\"Draft saved\");\n"
                                  "infoBar->setMessage(\"You can safely leave this page.\");\n"
                                  "infoBar->setIsClosable(true);\n"
                                  "\n"
                                  "connect(openButton, &Button::clicked, infoBar, [infoBar, status]() {\n"
                                  "    infoBar->setIsOpen(true);\n"
                                  "    status->setText(\"Visible\");\n"
                                  "});\n"
                                  "connect(infoBar, &InfoBar::closed, status, [status]() {\n"
                                  "    status->setText(\"Dismissed\");\n"
                                  "});"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent, 12);
                       QWidget* controls = horizontalGroup(surface, 10);
                       auto* openButton = sampleButton(controls, QStringLiteral("Show again"));
                       openButton->setFluentStyle(Button::Accent);
                       openButton->setFluentLayout(Button::IconBefore);
                       openButton->setIconGlyph(Typography::Icons::Refresh, Typography::IconSize::Standard);
                       openButton->setEnabled(false);
                       auto* status = makeStatusPill(controls, QStringLiteral("Visible"));
                       boxLayout(controls)->addWidget(openButton, 0, Qt::AlignVCenter);
                       boxLayout(controls)->addWidget(status, 0, Qt::AlignVCenter);

                       auto* infoBar = makeInfoBar(surface,
                                                   InfoBar::Informational,
                                                   QStringLiteral("Draft saved"),
                                                   QStringLiteral("You can safely leave this page."));
                       infoBar->setIsClosable(true);

                       QObject::connect(openButton, &Button::clicked, infoBar, [infoBar, status]() {
                           infoBar->setIsOpen(true);
                           status->setText(QStringLiteral("Visible"));
                       });
                       QObject::connect(openButton, &Button::clicked, openButton, [openButton]() {
                           openButton->setEnabled(false);
                       });
                       QObject::connect(infoBar, &InfoBar::closed, status, [status, openButton]() {
                           status->setText(QStringLiteral("Dismissed"));
                           openButton->setEnabled(true);
                       });

                       boxLayout(surface)->addWidget(controls);
                       boxLayout(surface)->addWidget(infoBar);
                       return surface;
                   })
    };
}

QVector<GallerySample> progressBarSamples()
{
    return {
        makeSample(QStringLiteral("progress-bar-determinate-value"),
                   QStringLiteral("Determinate value"),
                   QStringLiteral("A determinate ProgressBar exposes range, value, ratio, and progress text."),
                   QStringLiteral("auto* progressBar = new ProgressBar(this);\n"
                                  "progressBar->setRange(0, 100);\n"
                                  "progressBar->setBarWidth(320);\n"
                                  "\n"
                                  "auto* progressBox = new NumberBox(this);\n"
                                  "progressBox->setHeader(\"Progress\");\n"
                                  "progressBox->setRange(0, 100);\n"
                                  "progressBox->setSmallChange(1);\n"
                                  "progressBox->setLargeChange(10);\n"
                                  "progressBox->setDisplayPrecision(0);\n"
                                  "progressBox->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);\n"
                                  "\n"
                                  "auto* status = new Label(\"Progress: 44%\", this);\n"
                                  "status->setFixedWidth(124);\n"
                                  "\n"
                                  "connect(progressBox, &NumberBox::valueChanged, progressBar, [progressBar, status](double value) {\n"
                                  "    progressBar->setValue(std::isfinite(value) ? value : 0.0);\n"
                                  "    status->setText(QStringLiteral(\"Progress: %1%\").arg(progressBar->progressText()));\n"
                                  "});\n"
                                  "progressBox->setValue(44);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       auto* progressBar = new ProgressBar(surface);
                       progressBar->setRange(0, 100);
                       progressBar->setBarWidth(320);

                       auto* status = makeValueStatusLabel(surface,
                                                           QStringLiteral("Progress: 44%"));

                       auto* progressBox = new NumberBox(surface);
                       progressBox->setHeader(QStringLiteral("Progress"));
                       progressBox->setRange(0, 100);
                       progressBox->setSmallChange(1);
                       progressBox->setLargeChange(10);
                       progressBox->setDisplayPrecision(0);
                       progressBox->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
                       progressBox->setFixedWidth(156);

                       QObject::connect(progressBox, &NumberBox::valueChanged, progressBar, [progressBar, status](double value) {
                           progressBar->setValue(std::isfinite(value) ? value : 0.0);
                           status->setText(QStringLiteral("Progress: %1%")
                                               .arg(progressBar->progressText()));
                       });
                       progressBox->setValue(44);

                       QWidget* row = horizontalGroup(surface, 28);
                       boxLayout(row)->addWidget(progressBar, 0, Qt::AlignVCenter);
                       boxLayout(row)->addWidget(progressBox, 0, Qt::AlignVCenter);

                       boxLayout(surface)->addWidget(row);
                       boxLayout(surface)->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("progress-bar-states"),
                   QStringLiteral("Indeterminate and status states"),
                   QStringLiteral("Indeterminate bars animate while running; paused and error states stop motion and change color."),
                   QStringLiteral("auto* running = new ProgressBar(this);\n"
                                  "running->setIsIndeterminate(true);\n"
                                  "\n"
                                  "auto* paused = new ProgressBar(this);\n"
                                  "paused->setValue(65);\n"
                                  "paused->setShowPaused(true);\n"
                                  "\n"
                                  "auto* error = new ProgressBar(this);\n"
                                  "error->setValue(65);\n"
                                  "error->setShowError(true);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent, 10);
                       auto* running = new ProgressBar(surface);
                       running->setIsIndeterminate(true);
                       running->setBarWidth(300);

                       auto* paused = new ProgressBar(surface);
                       paused->setValue(65);
                       paused->setShowPaused(true);
                       paused->setBarWidth(300);

                       auto* error = new ProgressBar(surface);
                       error->setValue(65);
                       error->setShowError(true);
                       error->setBarWidth(300);

                       boxLayout(surface)->addWidget(progressBarRow(surface, QStringLiteral("Running"), running));
                       boxLayout(surface)->addWidget(progressBarRow(surface, QStringLiteral("Paused"), paused));
                       boxLayout(surface)->addWidget(progressBarRow(surface, QStringLiteral("Error"), error));
                       return surface;
                   }),
        makeSample(QStringLiteral("progress-bar-metrics"),
                   QStringLiteral("Rail and thickness"),
                   QStringLiteral("The rail, width, and track thickness define the geometry without changing value semantics."),
                   QStringLiteral("auto* thickBar = new ProgressBar(this);\n"
                                  "thickBar->setValue(70);\n"
                                  "thickBar->setBarWidth(280);\n"
                                  "thickBar->setTrackThickness(6.0);\n"
                                  "\n"
                                  "auto* noRailBar = new ProgressBar(this);\n"
                                  "noRailBar->setValue(70);\n"
                                  "noRailBar->setBarWidth(280);\n"
                                  "noRailBar->setRailVisible(false);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent, 10);
                       auto* thickBar = new ProgressBar(surface);
                       thickBar->setValue(70);
                       thickBar->setBarWidth(280);
                       thickBar->setTrackThickness(6.0);

                       auto* noRailBar = new ProgressBar(surface);
                       noRailBar->setValue(70);
                       noRailBar->setBarWidth(280);
                       noRailBar->setRailVisible(false);

                       boxLayout(surface)->addWidget(progressBarRow(surface, QStringLiteral("6 px track"), thickBar));
                       boxLayout(surface)->addWidget(progressBarRow(surface, QStringLiteral("No rail"), noRailBar));
                       return surface;
                   })
    };
}

QVector<GallerySample> progressRingSamples()
{
    return {
        makeSample(QStringLiteral("progress-ring-indeterminate-sizes"),
                   QStringLiteral("Active indeterminate sizes"),
                   QStringLiteral("An active indeterminate ProgressRing animates in small, medium, and large sizes."),
                   QStringLiteral("auto* small = new ProgressRing(this);\n"
                                  "small->setRingSize(ProgressRing::ProgressRingSize::Small);\n"
                                  "small->setIsActive(true);\n"
                                  "\n"
                                  "auto* large = new ProgressRing(this);\n"
                                  "large->setRingSize(ProgressRing::ProgressRingSize::Large);\n"
                                  "large->setIsActive(true);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       QWidget* row = horizontalGroup(surface, 30);
                       auto* small = new ProgressRing(row);
                       small->setRingSize(ProgressRing::ProgressRingSize::Small);
                       small->setIsActive(true);

                       auto* medium = new ProgressRing(row);
                       medium->setRingSize(ProgressRing::ProgressRingSize::Medium);
                       medium->setIsActive(true);

                       auto* large = new ProgressRing(row);
                       large->setRingSize(ProgressRing::ProgressRingSize::Large);
                       large->setIsActive(true);

                       boxLayout(row)->addWidget(progressRingColumn(row, QStringLiteral("Small"), small));
                       boxLayout(row)->addWidget(progressRingColumn(row, QStringLiteral("Medium"), medium));
                       boxLayout(row)->addWidget(progressRingColumn(row, QStringLiteral("Large"), large));
                       boxLayout(surface)->addWidget(row);
                       return surface;
                   }),
        makeSample(QStringLiteral("progress-ring-determinate-value"),
                   QStringLiteral("Determinate value"),
                   QStringLiteral("Determinate rings use range, value, and an optional background track."),
                   QStringLiteral("auto* ring = new ProgressRing(this);\n"
                                  "ring->setIsIndeterminate(false);\n"
                                  "ring->setIsActive(true);\n"
                                  "ring->setRingSize(ProgressRing::ProgressRingSize::Large);\n"
                                  "ring->setBackgroundVisible(true);\n"
                                  "\n"
                                  "auto* progressBox = new NumberBox(this);\n"
                                  "progressBox->setHeader(\"Progress\");\n"
                                  "progressBox->setRange(0, 100);\n"
                                  "progressBox->setSmallChange(1);\n"
                                  "progressBox->setLargeChange(10);\n"
                                  "progressBox->setDisplayPrecision(0);\n"
                                  "progressBox->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);\n"
                                  "\n"
                                  "auto* status = new Label(\"Value: 44%\", this);\n"
                                  "status->setFixedWidth(124);\n"
                                  "connect(progressBox, &NumberBox::valueChanged, ring, [ring, status](double value) {\n"
                                  "    const int progress = std::isfinite(value) ? static_cast<int>(value) : 0;\n"
                                  "    ring->setValue(progress);\n"
                                  "    status->setText(QStringLiteral(\"Value: %1%\").arg(progress));\n"
                                  "});\n"
                                  "progressBox->setValue(44);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       QWidget* row = horizontalGroup(surface, 28);

                       auto* ring = new ProgressRing(row);
                       ring->setIsIndeterminate(false);
                       ring->setIsActive(true);
                       ring->setRingSize(ProgressRing::ProgressRingSize::Large);
                       ring->setBackgroundVisible(true);

                       auto* progressBox = new NumberBox(row);
                       progressBox->setHeader(QStringLiteral("Progress"));
                       progressBox->setRange(0, 100);
                       progressBox->setSmallChange(1);
                       progressBox->setLargeChange(10);
                       progressBox->setDisplayPrecision(0);
                       progressBox->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
                       progressBox->setFixedWidth(156);

                       auto* status = makeValueStatusLabel(row, QStringLiteral("Value: 44%"));
                       QObject::connect(progressBox, &NumberBox::valueChanged, ring, [ring, status](double value) {
                           const int progress = std::isfinite(value) ? static_cast<int>(value) : 0;
                           ring->setValue(progress);
                           status->setText(QStringLiteral("Value: %1%").arg(progress));
                       });
                       progressBox->setValue(44);

                       boxLayout(row)->addWidget(ring, 0, Qt::AlignVCenter);
                       boxLayout(row)->addWidget(progressBox, 0, Qt::AlignVCenter);
                       boxLayout(row)->addWidget(status, 0, Qt::AlignVCenter);
                       boxLayout(surface)->addWidget(row);
                       return surface;
                   }),
        makeSample(QStringLiteral("progress-ring-status"),
                   QStringLiteral("Status states"),
                   QStringLiteral("Paused and error statuses reuse determinate geometry with semantic colors."),
                   QStringLiteral("auto* running = new ProgressRing(this);\n"
                                  "running->setIsIndeterminate(false);\n"
                                  "running->setIsActive(true);\n"
                                  "running->setRingSize(ProgressRing::ProgressRingSize::Large);\n"
                                  "running->setValue(65);\n"
                                  "running->setStatus(ProgressRing::ProgressRingStatus::Running);\n"
                                  "running->setBackgroundVisible(true);\n"
                                  "\n"
                                  "auto* paused = new ProgressRing(this);\n"
                                  "paused->setIsIndeterminate(false);\n"
                                  "paused->setIsActive(true);\n"
                                  "paused->setRingSize(ProgressRing::ProgressRingSize::Large);\n"
                                  "paused->setValue(65);\n"
                                  "paused->setStatus(ProgressRing::ProgressRingStatus::Paused);\n"
                                  "paused->setBackgroundVisible(true);\n"
                                  "\n"
                                  "auto* error = new ProgressRing(this);\n"
                                  "error->setIsIndeterminate(false);\n"
                                  "error->setIsActive(true);\n"
                                  "error->setRingSize(ProgressRing::ProgressRingSize::Large);\n"
                                  "error->setValue(65);\n"
                                  "error->setStatus(ProgressRing::ProgressRingStatus::Error);\n"
                                  "error->setBackgroundVisible(true);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       QWidget* row = horizontalGroup(surface, 30);

                       auto* running = new ProgressRing(row);
                       running->setIsIndeterminate(false);
                       running->setIsActive(true);
                       running->setRingSize(ProgressRing::ProgressRingSize::Large);
                       running->setValue(65);
                       running->setBackgroundVisible(true);
                       running->setStatus(ProgressRing::ProgressRingStatus::Running);

                       auto* paused = new ProgressRing(row);
                       paused->setIsIndeterminate(false);
                       paused->setIsActive(true);
                       paused->setRingSize(ProgressRing::ProgressRingSize::Large);
                       paused->setValue(65);
                       paused->setBackgroundVisible(true);
                       paused->setStatus(ProgressRing::ProgressRingStatus::Paused);

                       auto* error = new ProgressRing(row);
                       error->setIsIndeterminate(false);
                       error->setIsActive(true);
                       error->setRingSize(ProgressRing::ProgressRingSize::Large);
                       error->setValue(65);
                       error->setBackgroundVisible(true);
                       error->setStatus(ProgressRing::ProgressRingStatus::Error);

                       boxLayout(row)->addWidget(progressRingColumn(row, QStringLiteral("Running"), running));
                       boxLayout(row)->addWidget(progressRingColumn(row, QStringLiteral("Paused"), paused));
                       boxLayout(row)->addWidget(progressRingColumn(row, QStringLiteral("Error"), error));
                       boxLayout(surface)->addWidget(row);
                       return surface;
                   })
    };
}

QVector<GallerySample> shimmerSamples()
{
    return {
        makeSample(QStringLiteral("shimmer-templates"),
                   QStringLiteral("Built-in templates"),
                   QStringLiteral("Built-in templates cover image cards, avatar rows, and text blocks."),
                   QStringLiteral("auto* image = new Shimmer(this);\n"
                                  "image->setShimmerTemplate(Shimmer::ShimmerTemplate::ImageCard);\n"
                                  "image->setFixedSize(160, 92);\n"
                                  "\n"
                                  "auto* row = new Shimmer(this);\n"
                                  "row->setShimmerTemplate(Shimmer::ShimmerTemplate::AvatarTextRow);\n"
                                  "row->setFixedSize(180, 56);\n"
                                  "\n"
                                  "auto* text = new Shimmer(this);\n"
                                  "text->setShimmerTemplate(Shimmer::ShimmerTemplate::TextBlock);\n"
                                  "text->setFixedSize(220, 72);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       QWidget* row = horizontalGroup(surface, 24);

                       auto* image = new Shimmer(row);
                       image->setShimmerTemplate(Shimmer::ShimmerTemplate::ImageCard);
                       image->setFixedSize(160, 92);

                       auto* avatarRow = new Shimmer(row);
                       avatarRow->setShimmerTemplate(Shimmer::ShimmerTemplate::AvatarTextRow);
                       avatarRow->setFixedSize(180, 56);

                       auto* text = new Shimmer(row);
                       text->setShimmerTemplate(Shimmer::ShimmerTemplate::TextBlock);
                       text->setFixedSize(220, 72);

                       boxLayout(row)->addWidget(labeledColumn(row, QStringLiteral("ImageCard"), image));
                       boxLayout(row)->addWidget(labeledColumn(row, QStringLiteral("AvatarTextRow"), avatarRow));
                       boxLayout(row)->addWidget(labeledColumn(row, QStringLiteral("TextBlock"), text));
                       boxLayout(surface)->addWidget(row);
                       return surface;
                   }),
        makeSample(QStringLiteral("shimmer-custom-elements"),
                   QStringLiteral("Custom elements"),
                   QStringLiteral("Custom skeleton elements let a shimmer match the loading content shape."),
                   QStringLiteral("auto* shimmer = new Shimmer(this);\n"
                                  "shimmer->setFixedSize(330, 160);\n"
                                  "\n"
                                  "using Element = ShimmerPainter::Element;\n"
                                  "using Shape = ShimmerPainter::Shape;\n"
                                  "shimmer->setElements({\n"
                                  "    Element(Shape::RoundedRect, QRectF(0, 0, 330, 82), 8.0),\n"
                                  "    Element(Shape::Circle, QRectF(0, 104, 36, 36)),\n"
                                  "    Element(Shape::Line, QRectF(50, 106, 180, 12)),\n"
                                  "    Element(Shape::Line, QRectF(50, 128, 124, 12)),\n"
                                  "    Element(Shape::RoundedRect, QRectF(250, 106, 72, 30), 6.0)\n"
                                  "});"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       auto* shimmer = new Shimmer(surface);
                       shimmer->setFixedSize(330, 160);
                       using Element = ShimmerPainter::Element;
                       using Shape = ShimmerPainter::Shape;
                       shimmer->setElements({
                           Element(Shape::RoundedRect, QRectF(0, 0, 330, 82), 8.0),
                           Element(Shape::Circle, QRectF(0, 104, 36, 36)),
                           Element(Shape::Line, QRectF(50, 106, 180, 12)),
                           Element(Shape::Line, QRectF(50, 128, 124, 12)),
                           Element(Shape::RoundedRect, QRectF(250, 106, 72, 30), 6.0)
                       });
                       boxLayout(surface)->addWidget(shimmer);
                       return surface;
                   }),
        makeSample(QStringLiteral("shimmer-static-phase"),
                   QStringLiteral("Static phase"),
                   QStringLiteral("Animation can be disabled while keeping a deterministic shimmer highlight visible."),
                   QStringLiteral("auto* shimmer = new Shimmer(this);\n"
                                  "shimmer->setShimmerTemplate(Shimmer::ShimmerTemplate::TextBlock);\n"
                                  "shimmer->setFixedSize(260, 72);\n"
                                  "shimmer->setAnimationEnabled(false);\n"
                                  "shimmer->setShimmerProgress(0.42);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       auto* shimmer = new Shimmer(surface);
                       shimmer->setShimmerTemplate(Shimmer::ShimmerTemplate::TextBlock);
                       shimmer->setFixedSize(260, 72);
                       shimmer->setAnimationEnabled(false);
                       shimmer->setShimmerProgress(0.42);
                       auto* status = makeStatusLabel(surface, QStringLiteral("Progress phase: 0.42"));
                       boxLayout(surface)->addWidget(shimmer);
                       boxLayout(surface)->addWidget(status);
                       return surface;
                   })
    };
}

QVector<GallerySample> toastSamples()
{
    return {
        makeSample(
            QStringLiteral("toast-severity"),
            QStringLiteral("Severity"),
            QStringLiteral("Use a short-lived same-window toast to acknowledge an action without blocking the current task."),
            QStringLiteral("auto* toast = new Toast(this);\n"
                           "toast->setDuration(2200);\n"
                           "\n"
                           "auto* infoButton = new Button(\"Info\", this);\n"
                           "auto* successButton = new Button(\"Success\", this);\n"
                           "auto* warningButton = new Button(\"Warning\", this);\n"
                           "auto* errorButton = new Button(\"Error\", this);\n"
                           "\n"
                           "connect(infoButton, &Button::clicked, this, [=]() {\n"
                           "    toast->setMessage(\"Draft saved locally\");\n"
                           "    toast->setSeverity(Toast::Informational);\n"
                           "    toast->present(infoButton);\n"
                           "});\n"
                           "connect(successButton, &Button::clicked, this, [=]() {\n"
                           "    toast->setMessage(\"Changes published\");\n"
                           "    toast->setSeverity(Toast::Success);\n"
                           "    toast->present(successButton);\n"
                           "});\n"
                           "connect(warningButton, &Button::clicked, this, [=]() {\n"
                           "    toast->setMessage(\"Connection is unstable\");\n"
                           "    toast->setSeverity(Toast::Warning);\n"
                           "    toast->present(warningButton);\n"
                           "});\n"
                           "connect(errorButton, &Button::clicked, this, [=]() {\n"
                           "    toast->setMessage(\"Upload could not finish\");\n"
                           "    toast->setSeverity(Toast::Error);\n"
                           "    toast->present(errorButton);\n"
                           "});"),
            [](QWidget* parent) {
                QWidget* surface = sampleSurface(parent);
                auto* toast = new Toast(surface);
                toast->setObjectName(
                    QStringLiteral("galleryToastSeveritySample"));
                toast->setDuration(2200);
                QObject::connect(
                    surface, &QObject::destroyed,
                    toast, &QObject::deleteLater);
                QWidget* row = horizontalGroup(surface, 8);
                struct SeverityAction {
                    QString title;
                    QString message;
                    Toast::Severity severity;
                };
                const SeverityAction actions[] = {
                    {QStringLiteral("Info"),
                     QStringLiteral("Draft saved locally"),
                     Toast::Informational},
                    {QStringLiteral("Success"),
                     QStringLiteral("Changes published"),
                     Toast::Success},
                    {QStringLiteral("Warning"),
                     QStringLiteral("Connection is unstable"),
                     Toast::Warning},
                    {QStringLiteral("Error"),
                     QStringLiteral("Upload could not finish"),
                     Toast::Error},
                };
                for (const SeverityAction& action : actions) {
                    auto* button = sampleButton(row, action.title);
                    QObject::connect(
                        button,
                        &Button::clicked,
                        button,
                        [button, toast, action]() {
                            toast->setMessage(action.message);
                            toast->setSeverity(action.severity);
                            toast->present(button);
                        });
                    boxLayout(row)->addWidget(button);
                }
                boxLayout(surface)->addWidget(row);
                boxLayout(surface)->addWidget(makeStatusLabel(
                    surface,
                    QStringLiteral(
                        "The same toast instance updates for each severity.")));
                return surface;
            }),
        makeSample(
            QStringLiteral("toast-title-placement"),
            QStringLiteral("Title and placement"),
            QStringLiteral("Add a concise title when the message needs context, and anchor the toast to any top or bottom edge."),
            QStringLiteral("auto* toast = new Toast(this);\n"
                           "toast->setTitle(\"Sync complete\");\n"
                           "toast->setMessage(\"12 files are now available offline.\");\n"
                           "toast->setSeverity(Toast::Success);\n"
                           "toast->setDuration(2600);\n"
                           "\n"
                           "auto* topEnd = new Button(\"Top end\", this);\n"
                           "auto* bottomStart = new Button(\"Bottom start\", this);\n"
                           "\n"
                           "connect(topEnd, &Button::clicked, this, [=]() {\n"
                           "    toast->setPlacement(Toast::TopEnd);\n"
                           "    toast->present(topEnd);\n"
                           "});\n"
                           "connect(bottomStart, &Button::clicked, this, [=]() {\n"
                           "    toast->setPlacement(Toast::BottomStart);\n"
                           "    toast->present(bottomStart);\n"
                           "});"),
            [](QWidget* parent) {
                QWidget* surface = sampleSurface(parent);
                QWidget* row = horizontalGroup(surface, 8);
                auto* toast = new Toast(surface);
                toast->setObjectName(
                    QStringLiteral("galleryToastPlacementSample"));
                toast->setTitle(
                    QStringLiteral("Sync complete"));
                toast->setMessage(
                    QStringLiteral(
                        "12 files are now available offline."));
                toast->setSeverity(Toast::Success);
                toast->setDuration(2600);
                QObject::connect(
                    surface, &QObject::destroyed,
                    toast, &QObject::deleteLater);

                struct PlacementAction {
                    QString title;
                    Toast::Placement placement;
                };
                const PlacementAction actions[] = {
                    {QStringLiteral("Top start"), Toast::TopStart},
                    {QStringLiteral("Top"), Toast::Top},
                    {QStringLiteral("Top end"), Toast::TopEnd},
                    {QStringLiteral("Bottom start"), Toast::BottomStart},
                    {QStringLiteral("Bottom"), Toast::Bottom},
                    {QStringLiteral("Bottom end"), Toast::BottomEnd},
                };
                for (const PlacementAction& action : actions) {
                    auto* button = sampleButton(row, action.title);
                    QObject::connect(
                        button,
                        &Button::clicked,
                        button,
                        [button, toast, action]() {
                            toast->setPlacement(action.placement);
                            toast->present(button);
                        });
                    boxLayout(row)->addWidget(button);
                }
                boxLayout(surface)->addWidget(row);
                return surface;
            }),
        makeSample(
            QStringLiteral("toast-stacking"),
            QStringLiteral("Stacking"),
            QStringLiteral("Managed toasts stack per placement up to Toast::maximumVisible(), shifting away from the chosen edge in show order."),
            QStringLiteral("Toast::setMaximumVisible(3);\n"
                           "\n"
                           "Toast::showToast(this, \"Draft saved\", Toast::Informational);\n"
                           "Toast::showToast(this, \"Upload finished\", Toast::Success);\n"
                           "Toast::showToast(\n"
                           "    this,\n"
                           "    \"Connection is unstable\",\n"
                           "    Toast::Warning,\n"
                           "    2200,\n"
                           "    Toast::TopEnd);"),
            [](QWidget* parent) {
                QWidget* surface = sampleSurface(parent);
                QWidget* row = horizontalGroup(surface, 8);
                auto* stackTop = sampleButton(
                    row, QStringLiteral("Stack at top"));
                auto* stackEnd = sampleButton(
                    row, QStringLiteral("Stack at top end"));
                QObject::connect(
                    stackTop,
                    &Button::clicked,
                    stackTop,
                    [stackTop]() {
                        static int counter = 0;
                        ++counter;
                        Toast::showToast(
                            stackTop,
                            QStringLiteral("Top notice %1")
                                .arg(counter),
                            Toast::Informational);
                    });
                QObject::connect(
                    stackEnd,
                    &Button::clicked,
                    stackEnd,
                    [stackEnd]() {
                        static int counter = 0;
                        ++counter;
                        Toast::showToast(
                            stackEnd,
                            QStringLiteral("Corner notice %1")
                                .arg(counter),
                            Toast::Warning,
                            2200,
                            Toast::TopEnd);
                    });
                boxLayout(row)->addWidget(stackTop);
                boxLayout(row)->addWidget(stackEnd);
                boxLayout(surface)->addWidget(row);
                boxLayout(surface)->addWidget(makeStatusLabel(
                    surface,
                    QStringLiteral(
                        "Default maximumVisible is 3; older toasts dismiss first.")));
                return surface;
            }),
        makeSample(
            QStringLiteral("toast-action-lifecycle"),
            QStringLiteral("Action, hover pause, and dismissal reason"),
            QStringLiteral("An optional borrowed QAction makes the toast interactive; hover can pause its timeout, and every close path reports a stable reason."),
            QStringLiteral("auto* retry = new QAction(\"Retry\", this);\n"
                           "auto* toast = new Toast(this);\n"
                           "toast->setAction(retry);\n"
                           "toast->setPauseOnHoverEnabled(true);\n"
                           "toast->setDuration(5000);\n"
                           "\n"
                           "auto* showToast = new Button(\"Show actionable toast\", this);\n"
                           "auto* status = new Label(\"Ready\", this);\n"
                           "connect(showToast, &Button::clicked, this, [=]() {\n"
                           "    toast->setMessage(\"Upload failed. Retry when ready.\");\n"
                           "    toast->setSeverity(Toast::Error);\n"
                           "    toast->present(showToast);\n"
                           "    status->setText(\"Toast open; hover pauses timeout\");\n"
                           "});\n"
                           "connect(retry, &QAction::triggered, this, [=]() {\n"
                           "    status->setText(\"Retry requested\");\n"
                           "});\n"
                           "connect(toast, &Toast::dismissedWithReason, this,\n"
                           "        [=](Toast::DismissReason reason) {\n"
                           "    const QMetaEnum meta = QMetaEnum::fromType<Toast::DismissReason>();\n"
                           "    status->setText(QString(\"Dismissed: %1\").arg(\n"
                           "        QString::fromLatin1(meta.valueToKey(reason))));\n"
                           "});"),
            [](QWidget* parent) {
                QWidget* surface = sampleSurface(parent);
                auto* retry = new QAction(
                    QStringLiteral("Retry"), surface);
                auto* toast = new Toast(surface);
                toast->setObjectName(
                    QStringLiteral("galleryToastLifecycleSample"));
                toast->setAction(retry);
                toast->setPauseOnHoverEnabled(true);
                toast->setDuration(5000);
                QObject::connect(
                    surface, &QObject::destroyed,
                    toast, &QObject::deleteLater);

                auto* showToast = sampleButton(
                    surface,
                    QStringLiteral("Show actionable toast"));
                showToast->setObjectName(
                    QStringLiteral("galleryToastLifecycleTrigger"));
                auto* status = makeStatusLabel(
                    surface, QStringLiteral("Ready"));
                status->setObjectName(
                    QStringLiteral("galleryToastLifecycleStatus"));

                QObject::connect(
                    showToast,
                    &Button::clicked,
                    toast,
                    [showToast, toast, status]() {
                    toast->setMessage(
                        QStringLiteral(
                            "Upload failed. Retry when ready."));
                    toast->setSeverity(Toast::Error);
                    toast->present(showToast);
                    status->setText(
                        QStringLiteral(
                            "Toast open; hover pauses timeout"));
                });
                QObject::connect(
                    retry,
                    &QAction::triggered,
                    status,
                    [status]() {
                    status->setText(
                        QStringLiteral("Retry requested"));
                });
                QObject::connect(
                    toast,
                    &Toast::dismissedWithReason,
                    status,
                    [status](Toast::DismissReason reason) {
                    const QMetaEnum meta =
                        QMetaEnum::fromType<
                            Toast::DismissReason>();
                    status->setText(
                        QStringLiteral("Dismissed: %1")
                            .arg(QString::fromLatin1(
                                meta.valueToKey(reason))));
                });

                boxLayout(surface)->addWidget(
                    showToast, 0, Qt::AlignLeft);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(
            QStringLiteral("toast-update-key"),
            QStringLiteral("In-place managed updates"),
            QStringLiteral("A non-empty update key refreshes the matching toast within one host and placement without adding another stack entry."),
            QStringLiteral("auto* advance = new Button(\"Advance upload\", this);\n"
                           "auto* status = new Label(\"Progress: 0%\", this);\n"
                           "advance->setProperty(\"progress\", 0);\n"
                           "\n"
                           "connect(advance, &Button::clicked, this, [=]() {\n"
                           "    const int current = advance->property(\"progress\").toInt();\n"
                           "    const int progress = current >= 100 ? 25 : current + 25;\n"
                           "    advance->setProperty(\"progress\", progress);\n"
                           "    Toast::showOrUpdateToast(\n"
                           "        advance,\n"
                           "        \"upload\",\n"
                           "        progress == 100\n"
                           "            ? \"Upload complete\"\n"
                           "            : QString(\"Uploading: %1%\").arg(progress),\n"
                           "        progress == 100 ? Toast::Success : Toast::Informational,\n"
                           "        5000,\n"
                           "        Toast::TopEnd);\n"
                           "    status->setText(QString(\"Progress: %1%\").arg(progress));\n"
                           "});"),
            [](QWidget* parent) {
                QWidget* surface = sampleSurface(parent);
                auto* advance = sampleButton(
                    surface, QStringLiteral("Advance upload"));
                advance->setObjectName(
                    QStringLiteral("galleryToastUpdateTrigger"));
                advance->setProperty("progress", 0);
                auto* status = makeStatusLabel(
                    surface, QStringLiteral("Progress: 0%"));
                status->setObjectName(
                    QStringLiteral("galleryToastUpdateStatus"));

                QObject::connect(
                    advance,
                    &Button::clicked,
                    advance,
                    [advance, status]() {
                    const int current =
                        advance->property("progress").toInt();
                    const int progress =
                        current >= 100 ? 25 : current + 25;
                    advance->setProperty("progress", progress);
                    Toast::showOrUpdateToast(
                        advance,
                        QStringLiteral("upload"),
                        progress == 100
                            ? QStringLiteral("Upload complete")
                            : QStringLiteral("Uploading: %1%")
                                  .arg(progress),
                        progress == 100
                            ? Toast::Success
                            : Toast::Informational,
                        5000,
                        Toast::TopEnd);
                    status->setText(
                        QStringLiteral("Progress: %1%")
                            .arg(progress));
                });

                boxLayout(surface)->addWidget(
                    advance, 0, Qt::AlignLeft);
                boxLayout(surface)->addWidget(status);
                return surface;
            })
    };
}

QVector<GallerySample> toolTipSamples()
{
    return {
        makeSample(QStringLiteral("tooltip-hover"),
                   QStringLiteral("Hover target"),
                   QStringLiteral("ToolTip shows a lightweight message while the target is hovered."),
                   QStringLiteral("auto* button = new Button(\"Archive\", this);\n"
                                  "auto* toolTip = new ToolTip();\n"
                                  "toolTip->setText(\"Move the selected message to Archive\");\n"
                                  "\n"
                                  "// Show the tooltip from the button Enter event and hide it on Leave.\n"
                                  "new HoverToolTipController(button, toolTip);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       auto* button = new Button(QStringLiteral("Archive"), surface);
                       auto* toolTip = new ToolTip();
                       toolTip->setText(QStringLiteral("Move the selected message to Archive"));
                       new HoverToolTipController(button, toolTip);
                       boxLayout(surface)->addWidget(button, 0, Qt::AlignLeft);
                       return surface;
                   }),
        makeSample(QStringLiteral("tooltip-margins-font"),
                   QStringLiteral("Margins and font"),
                   QStringLiteral("Margins and font styling are part of the ToolTip visual contract."),
                   QStringLiteral("auto* button = new Button(\"Custom tip\", this);\n"
                                  "auto* toolTip = new ToolTip();\n"
                                  "toolTip->setText(\"Larger content margins\");\n"
                                  "toolTip->setMargins(QMargins(20, 10, 20, 10));\n"
                                  "\n"
                                  "QFont font;\n"
                                  "font.setPixelSize(15);\n"
                                  "font.setItalic(true);\n"
                                  "toolTip->setFont(font);\n"
                                  "\n"
                                  "new HoverToolTipController(button, toolTip, ToolTipPlacement::Right);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       auto* button = new Button(QStringLiteral("Custom tip"), surface);
                       auto* toolTip = new ToolTip();
                       toolTip->setText(QStringLiteral("Larger content margins"));
                       toolTip->setMargins(QMargins(20, 10, 20, 10));
                       QFont font;
                       font.setPixelSize(15);
                       font.setItalic(true);
                       toolTip->setFont(font);
                       new HoverToolTipController(button, toolTip, ToolTipPlacement::Right);
                       boxLayout(surface)->addWidget(button, 0, Qt::AlignLeft);
                       return surface;
                   }),
        makeSample(QStringLiteral("tooltip-animation"),
                   QStringLiteral("Animation toggle"),
                   QStringLiteral("Animation can be disabled for deterministic or instant tooltip behavior."),
                   QStringLiteral("auto* button = new Button(\"Instant tip\", this);\n"
                                  "auto* toolTip = new ToolTip();\n"
                                  "toolTip->setText(\"Animation disabled\");\n"
                                  "toolTip->setAnimationEnabled(false);\n"
                                  "\n"
                                  "new HoverToolTipController(button, toolTip);"),
                   [](QWidget* parent) {
                       QWidget* surface = sampleSurface(parent);
                       auto* button = new Button(QStringLiteral("Instant tip"), surface);
                       auto* toolTip = new ToolTip();
                       toolTip->setText(QStringLiteral("Animation disabled"));
                       toolTip->setAnimationEnabled(false);
                       new HoverToolTipController(button, toolTip);
                       boxLayout(surface)->addWidget(button, 0, Qt::AlignLeft);
                       return surface;
                   })
    };
}

} // namespace

QVector<GallerySample> statusInfoSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("avatar"))
        return avatarSamples();
    if (routeId == QStringLiteral("info-badge"))
        return infoBadgeSamples();
    if (routeId == QStringLiteral("info-bar"))
        return infoBarSamples();
    if (routeId == QStringLiteral("progress-bar"))
        return progressBarSamples();
    if (routeId == QStringLiteral("progress-ring"))
        return progressRingSamples();
    if (routeId == QStringLiteral("shimmer"))
        return shimmerSamples();
    if (routeId == QStringLiteral("toast"))
        return toastSamples();
    if (routeId == QStringLiteral("tooltip"))
        return toolTipSamples();
    return {};
}

} // namespace fluent::gallery

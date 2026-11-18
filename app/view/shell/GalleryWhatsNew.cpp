#include "GalleryWhatsNew.h"

#include <QCoreApplication>
#include <QDate>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <QJsonObject>
#include <QLocale>
#include <QTimer>
#include <QVersionNumber>

#include "components/basicinput/Button.h"
#include "components/basicinput/HyperlinkButton.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/layout/Card.h"
#include "components/layout/Divider.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "design/IconCatalog.h"

namespace fluent::gallery {
namespace {
using basicinput::Button;
using textfields::Label;
using Role = Typography::FontRole;

QString english(const QJsonValue& field)
{
    QString text = field.toObject().value(QStringLiteral("en")).toString();
    return text.remove(QLatin1Char('`'));
}

Label* label(const QString& text, Role role, QWidget* parent, bool secondary = false)
{
    auto* result = new Label(text, parent);
    result->setTextFormat(Qt::PlainText);
    result->setFluentTypography(role);
    if (secondary)
        result->setTextColorRole(Label::TextColorRole::Secondary);
    return result;
}

void populateHistory(QVBoxLayout* entries, QWidget* page)
{
    QFile file(QStringLiteral(":/app/assets/release-history.json"));
    QVector<QJsonValue> versions;
    if (file.open(QIODevice::ReadOnly))
        for (const auto& release :
            QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("versions")).toArray())
            versions.append(release);
    const auto versionOf = [](const QJsonValue& release) {
        return QVersionNumber::fromString(release.toObject().value(QStringLiteral("version")).toString());
    };
    std::sort(versions.begin(), versions.end(), [&](const QJsonValue& a, const QJsonValue& b) {
        return versionOf(a) > versionOf(b);
    });
    const auto installed = QVersionNumber::fromString(QCoreApplication::applicationVersion());
    for (const auto& value : versions) {
        const auto release = value.toObject();
        const auto version = release.value(QStringLiteral("version")).toString();
        const auto status = release.value(QStringLiteral("status")).toString();
        // Tagged packages are built while the current release still lacks its publication date.
        const bool bundledCandidate = status == QStringLiteral("candidate") && versionOf(value) == installed;
        if ((!bundledCandidate && status != QStringLiteral("published"))
            || (!installed.isNull() && versionOf(value) > installed))
            continue;
        for (const auto& item : release.value(QStringLiteral("groups")).toArray()) {
            const auto group = item.toObject();
            if (group.value(QStringLiteral("scope")).toString() != QStringLiteral("gallery"))
                continue;
            if (entries->count()) {
                entries->addSpacing(12);
                entries->addWidget(new layout::Divider(page));
                entries->addSpacing(4);
            }
            auto* meta = new QHBoxLayout;
            const auto date = QDate::fromString(release.value(QStringLiteral("published_at")).toString().left(10),
                Qt::ISODate);
            meta->addWidget(label(date.isValid()
                    ? QLocale(QLocale::English).toString(date, QStringLiteral("MMMM d, yyyy"))
                    : QStringLiteral("This version"),
                Role::Caption, page, true));
            meta->addStretch();
            auto* badge = new layout::Card(page);
            badge->setAppearance(layout::Card::Canvas);
            badge->setBorderVisible(false);
            auto* badgeLayout = new QHBoxLayout(badge);
            badgeLayout->setContentsMargins(9, 3, 9, 3);
            badgeLayout->addWidget(label(QStringLiteral("v") + version, Role::Caption, badge, true));
            meta->addWidget(badge);
            entries->addLayout(meta);
            const auto headline = english(group.value(QStringLiteral("headline")));
            const auto summary = english(group.value(QStringLiteral("summary")));
            auto* title = label(headline.isEmpty() ? summary : headline, Role::BodyLargeStrong, page);
            title->setWordWrap(true);
            entries->addWidget(title);
            QStringList paragraphs;
            for (const auto& change : group.value(QStringLiteral("changes")).toArray())
                paragraphs.append(english(change));
            if (paragraphs.isEmpty() && !headline.isEmpty())
                paragraphs.append(summary);
            for (const auto& paragraph : paragraphs) {
                auto* body = label(paragraph, Role::Body, page, true);
                body->setTextFormat(Qt::RichText);
                body->setText(
                    QStringLiteral("<p style='margin:0; line-height:125%'>%1</p>").arg(paragraph.toHtmlEscaped()));
                body->setWordWrap(true);
                entries->addWidget(body);
            }
        }
    }
    if (!entries->count())
        entries->addWidget(label(QStringLiteral("No Gallery updates for this version."), Role::Body, page, true));
    entries->addStretch();
}
} // namespace

QString GalleryWhatsNew::announcementGlyph()
{
    return Typography::Icons::glyph(QStringLiteral("ic_fluent_megaphone_20_regular"));
}

GalleryWhatsNew::GalleryWhatsNew(QWidget* trigger) : Dialog(trigger->window()), m_host(trigger->window())
{
    setObjectName(QStringLiteral("GalleryWhatsNew"));
    setAccessibleName(QStringLiteral("What’s new"));
    setThemeSource(trigger);
    setSmokeEnabled(true);
    setBackdropBlurRadius(0);
    setDimOpacity(0.27);
    setDragEnabled(false);
    auto* content = new QVBoxLayout(this);
    content->setContentsMargins(1, 1, 1, 1);
    content->setSpacing(0);
    m_header = new QHBoxLayout;
    m_header->setSpacing(12);
    auto* iconTile = new layout::Card(this);
    iconTile->setBorderVisible(false);
    iconTile->setAppearance(layout::Card::Canvas);
    iconTile->setFixedSize(32, 32);
    auto* iconLayout = new QHBoxLayout(iconTile);
    iconLayout->setContentsMargins(8, 8, 8, 8);
    auto* icon = new FontIcon(announcementGlyph(), iconTile);
    icon->setColor(themeColorsRef().accentDefault);
    iconLayout->addWidget(icon);
    m_header->addWidget(iconTile);
    auto* title = label(QStringLiteral("What’s new"), Role::Subtitle, this);
    auto font = title->font();
    font.setPixelSize(24);
    title->setFont(font);
    m_header->addWidget(title, 1);
    auto* close = new Button(this);
    close->setObjectName(QStringLiteral("GalleryWhatsNew.Close"));
    close->setAccessibleName(QStringLiteral("Close release history"));
    close->setToolTip(close->accessibleName());
    close->setFluentStyle(Button::Subtle);
    close->setFluentLayout(Button::IconOnly);
    close->setIconGlyph(Typography::Icons::ChromeClose, 16);
    close->setFixedSize(32, 32);
    m_header->addWidget(close);
    connect(close, &Button::clicked, this, &QDialog::reject);
    content->addLayout(m_header);
    content->addWidget(new layout::Divider(this));
    auto* scroll = new scrolling::ScrollView(this);
    scroll->setObjectName(QStringLiteral("GalleryWhatsNew.History"));
    scroll->setAccessibleName(QStringLiteral("Gallery release history"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarVisibility(scrolling::ScrollView::ScrollBarVisibility::Hidden);
    auto* page = new layout::Card(scroll);
    page->setBorderVisible(false);
    m_entries = new QVBoxLayout(page);
    m_entries->setSpacing(9);
    populateHistory(m_entries, page);
    scroll->setWidget(page);
    page->setAutoFillBackground(false);
    content->addWidget(scroll, 1);
    content->addWidget(new layout::Divider(this));
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(18, 5, 16, 5);
    auto* history = new basicinput::HyperlinkButton(QStringLiteral("Full release history"),
        QUrl(QStringLiteral("https://calvinhxx.github.io/Fluent-Qt/api/releases/")), this);
    history->setFluentLayout(Button::IconAfter);
    history->setIconGlyph(Typography::Icons::glyph(QStringLiteral("ic_fluent_arrow_up_right_16_regular")), 12);
    footer->addWidget(history);
    footer->addStretch();
    content->addLayout(footer);
    m_host->installEventFilter(this);
    connect(this, &Dialog::opened, this, [this, close, scroll] {
        fitWindow();
        // Repaint the viewport after the fade has finished compositing the dialog.
        scroll->viewport()->update();
        update();
        close->setFocus(Qt::OtherFocusReason);
    });
    connect(this, &QDialog::finished, trigger, [trigger] { trigger->setFocus(Qt::OtherFocusReason); });
    fitWindow();
}

bool GalleryWhatsNew::eventFilter(QObject* watched, QEvent* event)
{
    const bool surfaceChanged = event->type() == QEvent::DynamicPropertyChange
        && static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName()
            == overlay::overlaySurfaceRectPropertyName();
    if (watched == m_host && (event->type() == QEvent::Resize || surfaceChanged) && isVisible())
        QTimer::singleShot(0, this, [this] { fitWindow(); });
    return Dialog::eventFilter(watched, event);
}

void GalleryWhatsNew::showEvent(QShowEvent* event)
{
    Dialog::showEvent(event);
    fitWindow();
}

void GalleryWhatsNew::fitWindow()
{
    if (!m_host)
        return;
    const QRect surface = overlay::overlaySurfaceRect(m_host);
    const auto* bar = m_host->findChild<windowing::TitleBar*>();
    const int top = bar && !bar->isHidden() ? bar->mapTo(m_host, QPoint(0, bar->height())).y() : surface.top();
    const int width = qBound(1, surface.width() - 32, 560);
    const int height = qBound(1, surface.bottom() + 1 - top - 28, 526);
    const bool compact = width < 560;
    m_header->setContentsMargins(compact ? 20 : 26, compact ? 13 : 18, compact ? 14 : 18, compact ? 13 : 18);
    m_entries->setContentsMargins(compact ? 22 : 28, compact ? 16 : 20, compact ? 22 : 28, 20);
    const int x = surface.x() + (surface.width() - width) / 2;
    const int y = qMax(top + 12, surface.y() + (surface.height() - height) / 2);
    setGeometry(overlay::outerGeometryForVisibleCard(QRect(x, y, width, height), shadowSize()));
}
} // namespace fluent::gallery

#include "GalleryWhatsNew.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QTimer>

#include "GalleryReleaseHistoryView.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/HyperlinkButton.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/layout/Card.h"
#include "components/layout/Divider.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "design/IconCatalog.h"

namespace fluent::gallery {
namespace {
using basicinput::Button;
using textfields::Label;
using Role = Typography::FontRole;

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
    auto* title = new Label(QStringLiteral("What’s new"), this);
    title->setFluentTypography(Role::Subtitle);
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
    content->addWidget(new GalleryReleaseHistoryView(this), 1);
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
    connect(this, &Dialog::opened, this, [this, close] {
        fitWindow();
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
    const int x = surface.x() + (surface.width() - width) / 2;
    const int y = qMax(top + 12, surface.y() + (surface.height() - height) / 2);
    setGeometry(overlay::outerGeometryForVisibleCard(QRect(x, y, width, height), shadowSize()));
}
} // namespace fluent::gallery

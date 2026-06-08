#include "SettingsPage.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QPalette>
#include <QScrollArea>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "utils/Log.h"

namespace fluent::gallery {

SettingsPage::SettingsPage(const GalleryNavigationItem& item, QWidget* parent)
    : QWidget(parent)
    , m_routeId(item.id)
{
    setObjectName(QStringLiteral("gallerySettingsPage"));
    setProperty("galleryRouteId", m_routeId);
    setAutoFillBackground(true);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("gallerySettingsScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* page = new QWidget(scrollArea);
    page->setObjectName(QStringLiteral("gallerySettingsViewport"));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 34, 48, 48);
    layout->setSpacing(12);

    m_titleLabel = new fluent::textfields::Label(item.title, page);
    m_titleLabel->setObjectName(QStringLiteral("gallerySettingsTitle"));
    m_titleLabel->setFluentTypography(Typography::FontRole::Title);
    layout->addWidget(m_titleLabel);
    layout->addSpacing(20);

    layout->addWidget(createSectionTitle(QStringLiteral("Appearance & behavior")));
    layout->addWidget(createSettingsRow(Typography::Icons::Color,
                                        QStringLiteral("App theme"),
                                        QStringLiteral("Select which app theme to display"),
                                        createChoiceButton(QStringLiteral("Use system setting"))));
    layout->addWidget(createSettingsRow(Typography::Icons::List,
                                        QStringLiteral("Navigation style"),
                                        QStringLiteral("Choose how the navigation pane is presented"),
                                        createChoiceButton(QStringLiteral("Left"))));
    layout->addWidget(createSettingsRow(Typography::Icons::Music,
                                        QStringLiteral("Sound"),
                                        QStringLiteral("Controls provide audible feedback"),
                                        createChoiceButton(QStringLiteral("Off"))));
    layout->addWidget(createSettingsRow(Typography::Icons::AllApps,
                                        QStringLiteral("Manage samples"),
                                        QStringLiteral("Clear your recent or favorite samples"),
                                        createChoiceButton(QStringLiteral("Clear recents"))));

    layout->addSpacing(24);
    layout->addWidget(createSectionTitle(QStringLiteral("About")));
    layout->addWidget(createSettingsRow(Typography::Icons::AppIconDefault,
                                        QStringLiteral("WinUI 3 Gallery"),
                                        QStringLiteral("© 2026 Microsoft. All rights reserved."),
                                        createChoiceButton(QStringLiteral("2.9.3.0"))));
    layout->addStretch(1);

    scrollArea->setWidget(page);
    outerLayout->addWidget(scrollArea);

    applyPalette();
    LOG_DEBUG(QStringLiteral("SettingsPage created routeId=%1 title=%2")
                  .arg(m_routeId, item.title));
}

void SettingsPage::onThemeUpdated()
{
    applyPalette();
}

void SettingsPage::applyPalette()
{
    QPalette currentPalette = palette();
    currentPalette.setColor(QPalette::Window, QColor(QStringLiteral("#FAFAFA")));
    setPalette(currentPalette);
    setStyleSheet(QStringLiteral("#gallerySettingsViewport { background: #FAFAFA; }"));
}

QWidget* SettingsPage::createSectionTitle(const QString& title)
{
    auto* label = new fluent::textfields::Label(title, this);
    label->setObjectName(QStringLiteral("gallerySettingsSectionTitle"));
    label->setFluentTypography(Typography::FontRole::BodyStrong);
    label->setMinimumHeight(28);
    return label;
}

QWidget* SettingsPage::createChoiceButton(const QString& text)
{
    auto* button = new fluent::basicinput::Button(text, this);
    button->setObjectName(QStringLiteral("gallerySettingsChoiceButton"));
    button->setFluentStyle(fluent::basicinput::Button::Standard);
    button->setFluentSize(fluent::basicinput::Button::StandardSize);
    button->setFluentLayout(fluent::basicinput::Button::IconAfter);
    button->setIconGlyph(Typography::Icons::ChevronDownMed, 12);
    button->setMinimumWidth(136);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QWidget* SettingsPage::createSettingsRow(const QString& icon,
                                         const QString& title,
                                         const QString& subtitle,
                                         QWidget* trailing)
{
    auto* row = new QFrame(this);
    row->setObjectName(QStringLiteral("gallerySettingsRow"));
    row->setFrameShape(QFrame::NoFrame);
    row->setMinimumHeight(88);
    row->setStyleSheet(QStringLiteral(
        "#gallerySettingsRow { background: rgba(255, 255, 255, 235); border: 1px solid #E7E7E7; border-radius: 6px; }"));

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(24, 12, 22, 12);
    layout->setSpacing(20);

    auto* iconLabel = new fluent::textfields::Label(icon, row);
    iconLabel->setObjectName(QStringLiteral("gallerySettingsRowIcon"));
    iconLabel->setAlignment(Qt::AlignCenter);
    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(22);
    iconLabel->setFont(iconFont);
    iconLabel->setFixedSize(34, 34);

    auto* textColumn = new QWidget(row);
    auto* textLayout = new QVBoxLayout(textColumn);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    auto* titleLabel = new fluent::textfields::Label(title, textColumn);
    titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
    auto* subtitleLabel = new fluent::textfields::Label(subtitle, textColumn);
    subtitleLabel->setFluentTypography(Typography::FontRole::Caption);
    subtitleLabel->setStyleSheet(QStringLiteral("color: #646464;"));

    textLayout->addStretch(1);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(subtitleLabel);
    textLayout->addStretch(1);

    layout->addWidget(iconLabel);
    layout->addWidget(textColumn, 1);
    layout->addWidget(trailing, 0, Qt::AlignVCenter);
    return row;
}

} // namespace fluent::gallery

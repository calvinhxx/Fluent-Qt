#include "SettingsPage.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include "components/basicinput/ComboBox.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "utils/Log.h"
#include "view/support/GalleryCloseBehaviorUi.h"
#include "view/widgets/AccentColorControl.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {

namespace {

constexpr int kNarrowPageWidth = 640;
constexpr int kStackedRowWidth = 520;

class SecondaryLabel final : public fluent::textfields::Label {
public:
    SecondaryLabel(const QString& text, QWidget* parent)
        : Label(text, parent)
    {
        onThemeUpdated();
    }

    void onThemeUpdated() override
    {
        Label::onThemeUpdated();
        QPalette textPalette = palette();
        textPalette.setColor(QPalette::WindowText, themeColors().textSecondary);
        setPalette(textPalette);
    }
};

class SettingsRow final : public QFrame, public fluent::FluentElement {
public:
    SettingsRow(const QString& icon,
                const QString& title,
                const QString& subtitle,
                QWidget* trailing,
                QWidget* parent)
        : QFrame(parent)
        , m_trailing(trailing)
    {
        setObjectName(QStringLiteral("gallerySettingsRow"));
        setFrameShape(QFrame::NoFrame);
        setMinimumHeight(88);

        m_layout = new QGridLayout(this);
        m_layout->setContentsMargins(24, 12, 22, 12);
        m_layout->setHorizontalSpacing(20);
        m_layout->setVerticalSpacing(8);
        m_layout->setColumnStretch(1, 1);

        auto* iconLabel = new fluent::textfields::Label(icon, this);
        iconLabel->setObjectName(QStringLiteral("gallerySettingsRowIcon"));
        iconLabel->setAlignment(Qt::AlignCenter);
        QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
        iconFont.setPixelSize(22);
        iconLabel->setFont(iconFont);
        iconLabel->setFixedSize(34, 34);

        auto* textColumn = new QWidget(this);
        auto* textLayout = new QVBoxLayout(textColumn);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        auto* titleLabel = new fluent::textfields::Label(title, textColumn);
        titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
        auto* subtitleLabel = new SecondaryLabel(subtitle, textColumn);
        subtitleLabel->setObjectName(QStringLiteral("gallerySettingsSubtitle"));
        subtitleLabel->setFluentTypography(Typography::FontRole::Caption);
        subtitleLabel->setWordWrap(true);

        textLayout->addStretch(1);
        textLayout->addWidget(titleLabel);
        textLayout->addWidget(subtitleLabel);
        textLayout->addStretch(1);

        m_layout->addWidget(iconLabel, 0, 0, Qt::AlignVCenter);
        m_layout->addWidget(textColumn, 0, 1);
        updateResponsiveLayout();
    }

    void onThemeUpdated() override { update(); }

    void setStacked(bool stacked)
    {
        if (m_stacked == stacked && m_trailing->parentWidget() == this)
            return;

        m_stacked = stacked;
        m_trailing->setParent(this);
        m_layout->removeWidget(m_trailing);
        if (m_stacked) {
            m_layout->addWidget(m_trailing, 1, 1, Qt::AlignRight | Qt::AlignVCenter);
            setMinimumHeight(128);
        } else {
            m_layout->addWidget(m_trailing, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
            setMinimumHeight(88);
        }
        m_trailing->show();
        updateGeometry();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto colors = themeColors();
        const qreal radius = themeRadius().control;
        const QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(colors.strokeCard, 1.0));
        painter.setBrush(colors.bgLayer);
        painter.drawRoundedRect(cardRect, radius, radius);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QFrame::resizeEvent(event);
        updateResponsiveLayout();
    }

private:
    void updateResponsiveLayout()
    {
        setStacked(width() > 0 && width() < kStackedRowWidth);
    }

    QGridLayout* m_layout = nullptr;
    QWidget* m_trailing = nullptr;
    bool m_stacked = false;
};

} // namespace

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

    auto* scrollArea = new fluent::scrolling::ScrollView(this);
    scrollArea->setObjectName(QStringLiteral("gallerySettingsScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollMode(fluent::scrolling::ScrollView::ScrollMode::Disabled);
    scrollArea->setHorizontalScrollBarVisibility(
        fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

    m_viewport = new QWidget(scrollArea);
    m_viewport->setObjectName(QStringLiteral("gallerySettingsViewport"));
    m_viewport->setAutoFillBackground(true);
    m_contentLayout = new QVBoxLayout(m_viewport);
    m_contentLayout->setContentsMargins(48, 34, 48, 48);
    m_contentLayout->setSpacing(12);

    m_titleLabel = new fluent::textfields::Label(item.title, m_viewport);
    m_titleLabel->setObjectName(QStringLiteral("gallerySettingsTitle"));
    m_titleLabel->setFluentTypography(Typography::FontRole::Title);
    m_contentLayout->addWidget(m_titleLabel);
    m_contentLayout->addSpacing(20);

    auto* settings = &GallerySettings::instance();
    m_themeChoice = createChoiceBox(
        QStringLiteral("gallerySettingsThemeChoice"),
        {QStringLiteral("Use system setting"), QStringLiteral("Light"), QStringLiteral("Dark")},
        static_cast<int>(settings->themeMode()));
    // Brand style theme: swaps the whole palette + corner-radius preset at runtime via ThemeRegistry.
    // zh_CN: 品牌样式主题:经 ThemeRegistry 在运行时整体替换调色板 + 圆角预设。
    m_styleChoice = createChoiceBox(
        QStringLiteral("gallerySettingsStyleChoice"),
        {QStringLiteral("Fluent (Windows)"), QStringLiteral("Material 3 (Google)"),
         QStringLiteral("macOS")},
        static_cast<int>(settings->styleTheme()));
    // Match the native WinUI Gallery, which exposes only two navigation styles: "Left" and "Top".
    // "Left" maps to the responsive Auto mode (expanded → compact → minimal by width, just like
    // WinUI's PaneDisplayMode.Auto); "Top" is the horizontal bar. The richer internal enum
    // (Left/LeftCompact/LeftMinimal) stays available for config back-compat and the responsive
    // resolver, but is no longer surfaced as a manual choice.
    // zh_CN: 对齐原生 WinUI Gallery，导航样式只暴露 “Left” 与 “Top” 两项。“Left” 映射到响应式 Auto
    // （按宽度 展开→紧凑→最小，与 WinUI 的 PaneDisplayMode.Auto 一致）；“Top” 为顶部横条。内部更细的枚举
    // （Left/LeftCompact/LeftMinimal）仍保留用于配置兼容与响应式求解，但不再作为手动选项暴露。
    m_navigationChoice = createChoiceBox(
        QStringLiteral("gallerySettingsNavigationChoice"),
        {QStringLiteral("Left"), QStringLiteral("Top")},
        settings->navigationStyle() == GallerySettings::NavigationStyle::Top ? 1 : 0);
    m_effectChoice = createChoiceBox(
        QStringLiteral("gallerySettingsEffectChoice"),
        {QStringLiteral("Normal"), QStringLiteral("Mica"), QStringLiteral("Acrylic")},
        static_cast<int>(settings->windowEffect()));
    m_closeBehaviorChoice = createChoiceBox(
        QStringLiteral("gallerySettingsCloseBehaviorChoice"),
        closebehaviorui::choices(),
        static_cast<int>(settings->closeBehavior()));

    connect(m_themeChoice, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [settings](int index) {
                QTimer::singleShot(0, settings, [settings, index]() {
                    settings->setThemeMode(static_cast<GallerySettings::ThemeMode>(index));
                });
            });
    connect(m_navigationChoice, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [settings](int index) {
                settings->setNavigationStyle(index == 1
                                                 ? GallerySettings::NavigationStyle::Top
                                                 : GallerySettings::NavigationStyle::Auto);
            });
    connect(settings, &GallerySettings::themeModeChanged, this,
            [this](GallerySettings::ThemeMode mode) {
                const QSignalBlocker blocker(m_themeChoice);
                m_themeChoice->setCurrentIndex(static_cast<int>(mode));
            });
    connect(m_styleChoice, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [settings](int index) {
                settings->setStyleTheme(static_cast<GallerySettings::StyleTheme>(index));
            });
    connect(settings, &GallerySettings::styleThemeChanged, this,
            [this](GallerySettings::StyleTheme theme) {
                const QSignalBlocker blocker(m_styleChoice);
                m_styleChoice->setCurrentIndex(static_cast<int>(theme));
            });
    connect(settings, &GallerySettings::navigationStyleChanged, this,
            [this](GallerySettings::NavigationStyle style) {
                const QSignalBlocker blocker(m_navigationChoice);
                m_navigationChoice->setCurrentIndex(
                    style == GallerySettings::NavigationStyle::Top ? 1 : 0);
            });
    connect(m_effectChoice, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [settings](int index) {
                settings->setWindowEffect(static_cast<GallerySettings::WindowEffect>(index));
            });
    connect(settings, &GallerySettings::windowEffectChanged, this,
            [this](GallerySettings::WindowEffect effect) {
                const QSignalBlocker blocker(m_effectChoice);
                m_effectChoice->setCurrentIndex(static_cast<int>(effect));
            });
    connect(m_closeBehaviorChoice, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [settings](int index) {
                settings->setCloseBehavior(
                    static_cast<GallerySettings::CloseBehavior>(index));
                settings->setCloseBehaviorConfirmed(true);
            });
    connect(settings, &GallerySettings::closeBehaviorChanged, this,
            [this](GallerySettings::CloseBehavior behavior) {
                const QSignalBlocker blocker(m_closeBehaviorChoice);
                m_closeBehaviorChoice->setCurrentIndex(static_cast<int>(behavior));
            });

    // Style + accent share one row: both shape the brand palette, so they read as a single
    // "appearance style" choice (preset selector + accent swatch) rather than two near-identical rows.
    // zh_CN: 样式与强调色合并为一行:二者都决定品牌调色板,合成单一「外观样式」选择(预设下拉 + 强调色块),
    // 而非两行近乎雷同的设置。
    m_accentControl = new AccentColorControl(this);
    auto* styleAccentTrailing = new QWidget(this);
    styleAccentTrailing->setObjectName(QStringLiteral("gallerySettingsStyleAccentTrailing"));
    auto* styleAccentLayout = new QHBoxLayout(styleAccentTrailing);
    styleAccentLayout->setContentsMargins(0, 0, 0, 0);
    styleAccentLayout->setSpacing(12);
    styleAccentLayout->addWidget(m_styleChoice);
    styleAccentLayout->addWidget(m_accentControl);

    m_contentLayout->addWidget(createSectionTitle(QStringLiteral("Appearance & behavior")));
    // App theme uses a brightness glyph so it no longer duplicates the palette icon of the style row.
    // zh_CN: 应用主题改用「亮度」字形,不再与样式行的调色板图标重复。
    m_contentLayout->addWidget(createSettingsRow(Typography::Icons::Brightness,
                                                 QStringLiteral("App theme"),
                                                 QStringLiteral("Select which app theme to display"),
                                                 m_themeChoice));
    m_contentLayout->addWidget(createSettingsRow(Typography::Icons::Brush,
                                                 QStringLiteral("Style & accent color"),
                                                 QStringLiteral("Switch the palette and shape language (Fluent, Material 3, macOS) and pick an accent"),
                                                 styleAccentTrailing));
    m_contentLayout->addWidget(createSettingsRow(Typography::Icons::List,
                                                 QStringLiteral("Navigation style"),
                                                 QStringLiteral("Choose how the navigation pane is presented"),
                                                 m_navigationChoice));
    m_contentLayout->addWidget(createSettingsRow(Typography::Icons::Grid,
                                                 QStringLiteral("Window background effect"),
                                                 QStringLiteral("Mica and Acrylic require Windows 11 or macOS"),
                                                 m_effectChoice));
    m_contentLayout->addSpacing(12);
    m_contentLayout->addWidget(createSectionTitle(QStringLiteral("App behavior")));
    m_contentLayout->addWidget(createSettingsRow(
        Typography::Icons::Power,
        QStringLiteral("Close button behavior"),
        QStringLiteral("Choose what happens when the main window is closed"),
        m_closeBehaviorChoice));
    m_contentLayout->addStretch(1);

    scrollArea->setWidget(m_viewport);
    outerLayout->addWidget(scrollArea);

    applyPalette();
    updateResponsiveLayout();
    LOG_DEBUG(QStringLiteral("SettingsPage created routeId=%1 title=%2")
                  .arg(m_routeId, item.title));
}

void SettingsPage::onThemeUpdated()
{
    applyPalette();
}

void SettingsPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateResponsiveLayout();
}

void SettingsPage::applyPalette()
{
    const auto colors = themeColors();
    QPalette currentPalette = palette();
    currentPalette.setColor(QPalette::Window, colors.bgCanvas);
    currentPalette.setColor(QPalette::WindowText, colors.textPrimary);
    setPalette(currentPalette);
    if (m_viewport)
        m_viewport->setPalette(currentPalette);
}

void SettingsPage::updateResponsiveLayout()
{
    if (!m_contentLayout)
        return;
    const bool narrow = width() > 0 && width() < kNarrowPageWidth;
    const int horizontalMargin = narrow ? 24 : 48;
    m_contentLayout->setContentsMargins(horizontalMargin,
                                        narrow ? 24 : 34,
                                        horizontalMargin,
                                        48);
    for (auto* frame : findChildren<QFrame*>(QStringLiteral("gallerySettingsRow"))) {
        if (auto* row = dynamic_cast<SettingsRow*>(frame))
            row->setStacked(narrow || (row->width() > 0 && row->width() < kStackedRowWidth));
    }
}

QWidget* SettingsPage::createSectionTitle(const QString& title)
{
    auto* label = new fluent::textfields::Label(title, this);
    label->setObjectName(QStringLiteral("gallerySettingsSectionTitle"));
    label->setFluentTypography(Typography::FontRole::BodyStrong);
    label->setMinimumHeight(28);
    return label;
}

fluent::basicinput::ComboBox* SettingsPage::createChoiceBox(const QString& objectName,
                                                             const QStringList& choices,
                                                             int currentIndex)
{
    auto* choice = new fluent::basicinput::ComboBox(this);
    choice->setObjectName(objectName);
    choice->addItems(choices);
    choice->setCurrentIndex(currentIndex);
    choice->setMinimumWidth(140);
    choice->setMaximumWidth(168);
    choice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return choice;
}

QWidget* SettingsPage::createSettingsRow(const QString& icon,
                                         const QString& title,
                                         const QString& subtitle,
                                         QWidget* trailing)
{
    return new SettingsRow(icon, title, subtitle, trailing, this);
}

} // namespace fluent::gallery

#include "SettingsPage.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <array>

#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/dialogs_flyouts/ContentDialog.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "support/logging/Log.h"
#include "view/support/GalleryCloseBehaviorUi.h"
#include "view/shell/GalleryApplicationLifecycle.h"
#include "view/widgets/AccentColorControl.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {

namespace {

constexpr int kNarrowPageWidth = 640;
constexpr int kStackedRowWidth = 520;

struct UiScaleChoice {
    int percent;
    const char* label;
};

constexpr std::array<UiScaleChoice, 8> kUiScaleChoices{{
    {100, "Follow system (100%)"},
    {110, "110%"},
    {125, "125%"},
    {150, "150%"},
    {175, "175%"},
    {200, "200%"},
    {250, "250%"},
    {300, "300%"},
}};

QStringList uiScaleChoiceLabels()
{
    QStringList labels;
    labels.reserve(static_cast<int>(kUiScaleChoices.size()));
    for (const auto& choice : kUiScaleChoices)
        labels.append(QString::fromLatin1(choice.label));
    return labels;
}

int uiScaleChoiceIndex(int percent)
{
    percent = GallerySettings::normalizeUiScalePercent(percent);
    for (int index = 0; index < static_cast<int>(kUiScaleChoices.size()); ++index) {
        if (kUiScaleChoices[static_cast<std::size_t>(index)].percent == percent)
            return index;
    }
    return 0;
}

int uiScalePercentForChoice(int index)
{
    if (index < 0 || index >= static_cast<int>(kUiScaleChoices.size()))
        return 100;
    return kUiScaleChoices[static_cast<std::size_t>(index)].percent;
}

class SecondaryLabel final : public fluent::textfields::Label {
public:
    SecondaryLabel(const QString& text, QWidget* parent)
        : Label(text, parent)
    {
        setTextColorRole(TextColorRole::Secondary);
    }
};

class SettingsScrollView final : public fluent::scrolling::ScrollView {
public:
    explicit SettingsScrollView(QWidget* parent = nullptr)
        : fluent::scrolling::ScrollView(parent)
    {
        applyTransparentSurface();
    }

protected:
    void onThemeUpdated() override
    {
        fluent::scrolling::ScrollView::onThemeUpdated();
        applyTransparentSurface();
    }

private:
    void applyTransparentSurface()
    {
        setAutoFillBackground(false);
        QWidget* area = viewport();
        if (!area)
            return;

        area->setAutoFillBackground(false);
        area->setAttribute(Qt::WA_TranslucentBackground, false);
        area->setAttribute(Qt::WA_OpaquePaintEvent, false);
        QPalette transparentPalette = area->palette();
        transparentPalette.setColor(QPalette::Window, Qt::transparent);
        transparentPalette.setColor(QPalette::Base, Qt::transparent);
        area->setPalette(transparentPalette);
        area->update();
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
        setMinimumHeight(74);

        m_layout = new QGridLayout(this);
        m_layout->setContentsMargins(20, 8, 20, 8);
        m_layout->setHorizontalSpacing(16);
        m_layout->setVerticalSpacing(6);
        m_layout->setColumnStretch(1, 1);

        auto* iconLabel = new fluent::textfields::Label(
            Typography::Icons::glyphForSize(icon, Typography::IconSize::Standard), this);
        iconLabel->setObjectName(QStringLiteral("gallerySettingsRowIcon"));
        iconLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        iconLabel->setAlignment(Qt::AlignCenter);
        const QFont iconFont = Typography::Icons::font(Typography::IconSize::Standard);
        iconLabel->setFont(iconFont);
        iconLabel->setFixedSize(30, 30);

        auto* textColumn = new QWidget(this);
        auto* textLayout = new QVBoxLayout(textColumn);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(1);

        auto* titleLabel = new fluent::textfields::Label(title, textColumn);
        titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
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
            // Keep the narrow two-row layout at the documented touch/reading
            // height regardless of platform font metrics.
            setMinimumHeight(120);
        } else {
            m_layout->addWidget(m_trailing, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
            setMinimumHeight(74);
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
    setAutoFillBackground(false);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* scrollArea = new SettingsScrollView(this);
    scrollArea->setObjectName(QStringLiteral("gallerySettingsScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollMode(fluent::scrolling::ScrollView::ScrollMode::Disabled);
    scrollArea->setHorizontalScrollBarVisibility(
        fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

    m_viewport = new QWidget(scrollArea);
    m_viewport->setObjectName(QStringLiteral("gallerySettingsViewport"));
    m_viewport->setAutoFillBackground(false);
    m_viewport->setAttribute(Qt::WA_TranslucentBackground, false);
    m_viewport->setAttribute(Qt::WA_OpaquePaintEvent, false);
    m_contentLayout = new QVBoxLayout(m_viewport);
    m_contentLayout->setContentsMargins(48, 34, 48, 48);
    m_contentLayout->setSpacing(10);

    m_titleLabel = new fluent::textfields::Label(item.title, m_viewport);
    m_titleLabel->setObjectName(QStringLiteral("gallerySettingsTitle"));
    m_titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
    m_titleLabel->setFluentTypography(Typography::FontRole::Title);
    m_contentLayout->addWidget(m_titleLabel);
    m_contentLayout->addSpacing(16);

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
    m_displayScaleChoice = createChoiceBox(
        QStringLiteral("gallerySettingsDisplayScaleChoice"),
        uiScaleChoiceLabels(),
        uiScaleChoiceIndex(settings->uiScalePercent()));
    m_closeBehaviorChoice = createChoiceBox(
        QStringLiteral("gallerySettingsCloseBehaviorChoice"),
        closebehaviorui::choices(),
        static_cast<int>(settings->closeBehavior()));
    m_updateChecker = new UpdateChecker(this);

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
                settings->setWindowEffect(
                    static_cast<fluent::windowing::BackdropEffect>(index));
            });
    connect(m_displayScaleChoice, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this, settings](int index) {
                const int percent = uiScalePercentForChoice(index);
                if (settings->uiScalePercent() == percent)
                    return;
                settings->setUiScalePercent(percent);
                showDisplayScaleRestartDialog();
            });
    connect(settings, &GallerySettings::uiScalePercentChanged, this,
            [this](int percent) {
                const QSignalBlocker blocker(m_displayScaleChoice);
                m_displayScaleChoice->setCurrentIndex(uiScaleChoiceIndex(percent));
            });
    connect(settings, &GallerySettings::windowEffectChanged, this,
            [this](fluent::windowing::BackdropEffect effect) {
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
                                                 QStringLiteral("Uses the system compositor when available, otherwise a software Fluent material"),
                                                 m_effectChoice));
    m_contentLayout->addWidget(createSettingsRow(
        Typography::Icons::ZoomIn,
        QStringLiteral("Display scale"),
        QStringLiteral("Scale the Gallery relative to the operating-system setting; restart required"),
        m_displayScaleChoice));
    m_contentLayout->addSpacing(10);
    m_contentLayout->addWidget(createSectionTitle(QStringLiteral("App behavior")));
    m_contentLayout->addWidget(createSettingsRow(
        Typography::Icons::Power,
        QStringLiteral("Close button behavior"),
        QStringLiteral("Choose what happens when the main window is closed"),
        m_closeBehaviorChoice));
    m_contentLayout->addSpacing(10);
    m_contentLayout->addWidget(createSectionTitle(QStringLiteral("Updates")));
    m_contentLayout->addWidget(createSettingsRow(
        Typography::Icons::Sync,
        QStringLiteral("Gallery updates"),
        QStringLiteral("Check GitHub Releases and open the latest package for this platform"),
        createUpdateCheckControl()));
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
    if (m_updateStatusLabel)
        m_updateStatusLabel->onThemeUpdated();
    if (m_updateButton)
        m_updateButton->onThemeUpdated();
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
    currentPalette.setColor(QPalette::Window, Qt::transparent);
    currentPalette.setColor(QPalette::WindowText, colors.textPrimary);
    setPalette(currentPalette);
    setStyleSheet(QStringLiteral(
        "#gallerySettingsPage, #gallerySettingsViewport { background: transparent; }"));
    if (m_viewport) {
        m_viewport->setPalette(currentPalette);
        m_viewport->setAutoFillBackground(false);
        m_viewport->update();
    }
    update();
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
    label->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
    label->setFluentTypography(Typography::FontRole::BodyStrong);
    label->setMinimumHeight(24);
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
    // ComboBox::sizeHint() accounts for the widest option using the active
    // platform font. Do not clamp that result: the same label can be wider on
    // Windows or Linux than on macOS, and the responsive row already stacks
    // trailing controls when horizontal space is genuinely constrained.
    // zh_CN: ComboBox::sizeHint() 会按当前平台字体计算最长选项宽度；不要再用固定上限截断。
    choice->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return choice;
}

void SettingsPage::showDisplayScaleRestartDialog()
{
    QWidget* host = window();
    if (!host)
        return;
    if (auto* existing = host->findChild<fluent::dialogs_flyouts::ContentDialog*>(
            QStringLiteral("galleryDisplayScaleRestartDialog"))) {
        existing->raise();
        return;
    }

    auto* dialog = new fluent::dialogs_flyouts::ContentDialog(host);
    dialog->setObjectName(QStringLiteral("galleryDisplayScaleRestartDialog"));
    dialog->setWindowTitle(QStringLiteral("Restart required"));
    dialog->setTitle(QStringLiteral("Restart to apply display scale?"));
    auto* message = new fluent::textfields::Label(
        QStringLiteral("The new scale has been saved. Restart Fluent-Qt Gallery now to apply it consistently to the window, dialogs, text, and icons."),
        dialog);
    message->setObjectName(QStringLiteral("galleryDisplayScaleRestartMessage"));
    message->setFluentTypography(Typography::FontRole::Body);
    message->setWordWrap(true);
    message->setMinimumWidth(340);
    dialog->setContent(message);
    dialog->setPrimaryButtonText(QStringLiteral("Restart now"));
    dialog->setCloseButtonText(QStringLiteral("Later"));
    dialog->setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Primary);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->resize(460, 250);
    connect(dialog, &fluent::dialogs_flyouts::ContentDialog::primaryButtonClicked,
            dialog, [dialog]() {
                dialog->setProperty("galleryRestartRequested", true);
            });
    connect(dialog, &QDialog::finished, this, [dialog](int result) {
        const bool restartRequested =
            dialog->property("galleryRestartRequested").toBool()
            || result == fluent::dialogs_flyouts::ContentDialog::ResultPrimary;
        LOG_INFO(QStringLiteral("Display-scale restart dialog finished result=%1 restartRequested=%2")
                     .arg(result)
                     .arg(restartRequested));
        const QPointer<fluent::dialogs_flyouts::ContentDialog> guard(dialog);
        // ContentDialog emits finished from its exit-animation callback, which
        // still restores reusable dialog state after the signal returns. Move
        // destruction to the next event-loop turn, then request application
        // exit one turn later. This gives the dialog, its animation, smoke
        // overlay, and event filters a deterministic teardown order.
        // zh_CN: ContentDialog 在退场动画回调中发出 finished，信号返回后仍会
        // 恢复可复用状态。下一轮事件循环再同步销毁对话框，随后再下一轮请求
        // 退出，使动画、遮罩层及事件过滤器按确定顺序完成清理。
        QTimer::singleShot(0, qApp, [guard, restartRequested]() {
            if (guard)
                delete guard.data();
            LOG_INFO(QStringLiteral("Display-scale restart dialog teardown completed restartRequested=%1")
                         .arg(restartRequested));
            if (restartRequested) {
                QTimer::singleShot(0, qApp,
                                   []() { requestApplicationRestart(); });
            }
        });
    });
    dialog->open();
}

QWidget* SettingsPage::createUpdateCheckControl()
{
    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("gallerySettingsUpdateCheckControl"));
    auto* layout = new QHBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_updateStatusLabel = new SecondaryLabel(
        QStringLiteral("Current %1 / %2")
            .arg(m_updateChecker->currentVersion(), m_updateChecker->platformLabel()),
        panel);
    m_updateStatusLabel->setObjectName(QStringLiteral("gallerySettingsUpdateStatus"));
    m_updateStatusLabel->setFluentTypography(Typography::FontRole::Caption);
    m_updateStatusLabel->setAlignment(Qt::AlignRight);
    m_updateStatusLabel->setWordWrap(true);
    m_updateStatusLabel->setMaximumWidth(240);
    m_updateStatusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_updateButton = new fluent::basicinput::Button(QStringLiteral("Check updates"), panel);
    m_updateButton->setObjectName(QStringLiteral("gallerySettingsCheckUpdatesButton"));
    m_updateButton->setFluentLayout(fluent::basicinput::Button::IconBefore);
    m_updateButton->setIconGlyph(Typography::Icons::Refresh, Typography::IconSize::Standard);
    m_updateButton->setMinimumWidth(148);

    connect(m_updateButton, &QPushButton::clicked, this, &SettingsPage::openUpdateTarget);
    connect(m_updateChecker, &UpdateChecker::checkStarted, this, [this]() {
        m_updateActionUrl = QUrl();
        m_updateStatusLabel->setText(QStringLiteral("Checking latest release..."));
        m_updateButton->setText(QStringLiteral("Checking"));
        m_updateButton->setEnabled(false);
    });
    connect(m_updateChecker, &UpdateChecker::checkFinished, this,
            &SettingsPage::handleUpdateCheckFinished);

    layout->addWidget(m_updateStatusLabel, 1, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_updateButton, 0, Qt::AlignRight);
    return panel;
}

void SettingsPage::startUpdateCheck()
{
    if (!m_updateChecker || m_updateChecker->isChecking())
        return;

    m_updateActionUrl = QUrl();
    m_updateChecker->checkForUpdates();
}

void SettingsPage::handleUpdateCheckFinished(const UpdateChecker::Result& result)
{
    if (!m_updateStatusLabel || !m_updateButton)
        return;

    m_updateButton->setEnabled(true);
    m_updateButton->setToolTip(QString());

    switch (result.status) {
    case UpdateChecker::Result::Status::UpdateAvailable:
        m_updateActionUrl = result.assetUrl.isValid() ? result.assetUrl : result.releaseUrl;
        m_updateStatusLabel->setText(result.assetUrl.isValid()
            ? QStringLiteral("Version %1 available / %2")
                  .arg(result.latestVersion, m_updateChecker->platformLabel())
            : QStringLiteral("Version %1 available").arg(result.latestVersion));
        m_updateButton->setFluentStyle(fluent::basicinput::Button::Accent);
        m_updateButton->setText(result.assetUrl.isValid() ? QStringLiteral("Download")
                                                          : QStringLiteral("Open release"));
        m_updateButton->setIconGlyph(result.assetUrl.isValid() ? Typography::Icons::Download
                                                               : Typography::Icons::Link,
                                     Typography::IconSize::Standard);
        if (m_updateActionUrl.isValid())
            m_updateButton->setToolTip(m_updateActionUrl.toString());
        break;
    case UpdateChecker::Result::Status::UpToDate:
        m_updateActionUrl = QUrl();
        m_updateStatusLabel->setText(QStringLiteral("Latest version %1")
                                         .arg(result.currentVersion));
        m_updateButton->setFluentStyle(fluent::basicinput::Button::Standard);
        m_updateButton->setText(QStringLiteral("Check again"));
        m_updateButton->setIconGlyph(Typography::Icons::Refresh, Typography::IconSize::Standard);
        break;
    case UpdateChecker::Result::Status::Error:
        m_updateActionUrl = QUrl();
        m_updateStatusLabel->setText(QStringLiteral("Update check failed"));
        m_updateButton->setToolTip(result.message);
        m_updateButton->setFluentStyle(fluent::basicinput::Button::Standard);
        m_updateButton->setText(QStringLiteral("Try again"));
        m_updateButton->setIconGlyph(Typography::Icons::Refresh, Typography::IconSize::Standard);
        break;
    }
}

void SettingsPage::openUpdateTarget()
{
    if (!m_updateActionUrl.isValid()) {
        startUpdateCheck();
        return;
    }

    if (!QDesktopServices::openUrl(m_updateActionUrl) && m_updateStatusLabel) {
        m_updateStatusLabel->setText(QStringLiteral("Could not open the update link."));
    }
}

QWidget* SettingsPage::createSettingsRow(const QString& icon,
                                         const QString& title,
                                         const QString& subtitle,
                                         QWidget* trailing)
{
    return new SettingsRow(icon, title, subtitle, trailing, this);
}

} // namespace fluent::gallery

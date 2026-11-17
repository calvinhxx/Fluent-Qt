#include "WindowingSamples.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QPainter>
#include <QPoint>
#include <QPointer>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include "SampleBuilders.h"
#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/layout/Card.h"
#include "components/layout/Divider.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "components/windowing/Window.h"
#include "design/Typography.h"
#include "platform/GalleryPlatform.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::layout::Card;
using fluent::layout::Divider;
using fluent::textfields::AutoSuggestBox;
using fluent::textfields::Label;
using fluent::windowing::TitleBar;
using fluent::windowing::Window;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;
constexpr int kWindowsCaptionButtonWidth = 46;
constexpr int kWindowsCaptionButtonsReservedWidth = kWindowsCaptionButtonWidth * 3;

class SampleStatusPill : public QWidget, public fluent::FluentElement {
public:
    explicit SampleStatusPill(const QString& text, QWidget* parent = nullptr)
        : QWidget(parent), m_label(new Label(text, this))
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
        m_label->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;").arg(colors.textSecondary.name(QColor::HexArgb)));
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
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), themeRadius().control, themeRadius().control);
    }

private:
    Label* m_label = nullptr;
};

Label* makeSampleLabel(QWidget* parent, const QString& text, Typography::FontRole role = Typography::FontRole::Body)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(role);
    label->setWordWrap(true);
    label->setTextColorRole(Label::TextColorRole::Primary); // QSS-proof on the styled preview surface
    return label;
}

Button* makeSampleButton(QWidget* parent, const QString& text, Button::ButtonStyle style = Button::Standard)
{
    auto* button = new Button(text, parent);
    button->setFluentSize(Button::Small);
    button->setFluentStyle(style);
    button->setMinimumWidth(84);
    return button;
}

Button* makeCommandButton(QWidget* parent, const QString& text, const QString& glyph)
{
    auto* button = makeSampleButton(parent, text, Button::Accent);
    button->setFluentLayout(Button::IconBefore);
    button->setIconGlyph(glyph, Typography::IconSize::Standard);
    return button;
}

SampleStatusPill* makeStatusPill(QWidget* parent, const QString& text)
{
    return new SampleStatusPill(text, parent);
}

AutoSuggestBox* makeTitleBarSearch(QWidget* parent)
{
    auto* search = new AutoSuggestBox(parent);
    search->setPlaceholderText(QStringLiteral("Search"));
    search->setSuggestions(
        QStringList{QStringLiteral("TitleBar"), QStringLiteral("Window"), QStringLiteral("Drag regions")});
    search->setQueryIconVisible(true);
    search->setFontRole(Typography::FontRole::Caption);
    search->setSuggestionFontRole(Typography::FontRole::Caption);
    search->setInputHeight(28);
    search->setQueryButtonSize(16);
    search->setClearButtonSize(16);
    search->setSuggestionItemHeight(24);
    search->setFixedSize(180, 28);
    return search;
}

Button* makeWindowsCaptionButton(QWidget* parent, const QString& glyph, const QString& tooltip, bool critical = false)
{
    auto* button = new Button(parent);
    fluent::status_info::ToolTip::attach(button, tooltip);
    button->setFluentStyle(Button::Subtle);
    button->setFluentLayout(Button::IconOnly);
    button->setFluentSize(Button::Small);
    button->setIconGlyph(glyph, Typography::IconSize::Standard);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCriticalOnHover(critical);
    return button;
}

QWidget* makeWindowsCaptionButtons(QWidget* parent)
{
    auto* buttons = new QWidget(parent);
    buttons->setFixedWidth(kWindowsCaptionButtonsReservedWidth);

    auto* layout = new QHBoxLayout(buttons);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(makeWindowsCaptionButton(buttons, Typography::Icons::ChromeMinimize, QStringLiteral("Minimize")));
    layout->addWidget(makeWindowsCaptionButton(buttons, Typography::Icons::ChromeMaximize, QStringLiteral("Maximize")));
    layout->addWidget(makeWindowsCaptionButton(buttons, Typography::Icons::ChromeClose, QStringLiteral("Close"), true));
    return buttons;
}
class TitleBarPreview : public QWidget, public fluent::FluentElement {
public:
    explicit TitleBarPreview(int width, QWidget* parent = nullptr) : QWidget(parent), m_titleBar(new TitleBar(this))
    {
        setFixedSize(width, TitleBar::defaultTitleBarHeight());
        rebuildPlatformControls();
        connect(m_titleBar, &TitleBar::titleBarHeightChanged, this, [this](int height) {
            setFixedHeight(height);
            updateChromeGeometry();
        });
        updateChromeGeometry();
        QEvent activateEvent(QEvent::WindowActivate);
        QCoreApplication::sendEvent(m_titleBar, &activateEvent);
    }

    TitleBar* titleBar() const { return m_titleBar; }

    void onThemeUpdated() override
    {
        if (!m_platformControls)
            rebuildPlatformControls();
        update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        updateChromeGeometry();
    }

private:
    void rebuildPlatformControls()
    {
        if (m_platformControls) {
            delete m_platformControls;
            m_platformControls = nullptr;
        }
        m_platformControls = makeWindowsCaptionButtons(m_titleBar);
        m_titleBar->setSystemReservedLeadingWidth(0);
        m_titleBar->setSystemReservedTrailingWidth(kWindowsCaptionButtonsReservedWidth);
        m_platformControls->show();
        updateChromeGeometry();
    }

    void updateChromeGeometry()
    {
        m_titleBar->setGeometry(rect());
        if (!m_platformControls)
            return;
        m_platformControls->setGeometry(width() - kWindowsCaptionButtonsReservedWidth, 0,
            kWindowsCaptionButtonsReservedWidth, height());
        const auto captionButtons = m_platformControls->findChildren<Button*>();
        for (Button* button : captionButtons)
            button->setFixedSize(kWindowsCaptionButtonWidth, height());
        m_platformControls->raise();
    }

    TitleBar* m_titleBar = nullptr;
    QWidget* m_platformControls = nullptr;
};

QWidget* makeTitleBarContent(QWidget* parent,
    const QString& titleText,
    bool includeSearch,
    const QString& actionLabel = QStringLiteral("Share"))
{
    auto* content = new QWidget(parent);
    auto* layout = new QHBoxLayout(content);
    layout->setContentsMargins(10, 0, 8, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignVCenter);

    auto* appIcon = new fluent::FontIcon(Typography::Icons::Home, content);
    appIcon->setIconSize(14);
    layout->addWidget(appIcon, 0, Qt::AlignVCenter);

    auto* title = makeSampleLabel(content, titleText, Typography::FontRole::BodyStrong);
    title->setWordWrap(false);
    layout->addWidget(title, 0, Qt::AlignVCenter);

    if (includeSearch) {
        layout->addWidget(makeTitleBarSearch(content), 0, Qt::AlignVCenter);
        layout->addStretch(1);
    } else {
        layout->addStretch(1);
    }

    if (!actionLabel.isEmpty()) {
        auto* action = makeSampleButton(content, actionLabel, Button::Subtle);
        action->setMinimumWidth(64);
        layout->addWidget(action, 0, Qt::AlignVCenter);
    }
    return content;
}

struct MiniWindowFrame {
    Card* shell = nullptr;
    TitleBarPreview* titlePreview = nullptr;
    QVBoxLayout* bodyLayout = nullptr;
};

MiniWindowFrame makeMiniWindowFrame(QWidget* parent, int width = 620)
{
    auto* shell = new Card(parent);
    shell->setAppearance(Card::Layer);
    shell->setFixedWidth(width);

    auto* shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(1, 1, 1, 1);
    shellLayout->setSpacing(0);

    auto* preview = new TitleBarPreview(width - 2, shell);
    shellLayout->addWidget(preview);
    shellLayout->addWidget(new Divider(shell));

    auto* body = new QWidget(shell);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(20, 14, 20, 16);
    bodyLayout->setSpacing(10);
    shellLayout->addWidget(body, 1);
    return {shell, preview, bodyLayout};
}

QWidget* makeWindowContent(const QString& heading, const QString& body)
{
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    layout->addWidget(makeSampleLabel(content, heading, Typography::FontRole::Subtitle));
    layout->addWidget(makeSampleLabel(content, body));
    layout->addStretch(1);
    return content;
}

void showWindowNear(Window* window, QWidget* launcher, const QSize& size)
{
    QRect geometry(QPoint(), size);
    if (launcher) {
        const QPoint center = launcher->mapToGlobal(launcher->rect().center());
        geometry.moveTopLeft(center - QPoint(size.width() / 2, size.height() / 2));
    }
    platform::showTopLevelWindow(window, geometry);
    window->requestForegroundActivation();
}

void focusWindow(Window* window)
{
    if (!window)
        return;
    window->showNormal();
    window->requestForegroundActivation();
}

QVector<GallerySample> titleBarSamples()
{
    return {
        makeSample(QStringLiteral("title-bar-content-regions"), QStringLiteral("Content respects system regions"),
            QStringLiteral("Place custom content in TitleBar while preserving macOS leading "
                           "controls or Windows trailing caption buttons."),
            QStringLiteral(
                "auto* titleBar = new TitleBar(this);\n"
                "// macOS reserves leading traffic-light controls;\n"
                "// Windows reserves trailing caption buttons.\n"
                "titleBar->setSystemReservedLeadingWidth(platformLeadingWidth);\n"
                "titleBar->setSystemReservedTrailingWidth(platformTrailingWidth);"
                "\n"
                "\n"
                "auto* content = new QWidget(titleBar);\n"
                "auto* layout = new QHBoxLayout(content);\n"
                "layout->addWidget(titleLabel);\n"
                "layout->addWidget(searchBox);\n"
                "layout->addWidget(shareButton);\n"
                "\n"
                "titleBar->setContentWidget(content);"),
            [](QWidget* parent) {
                auto* group = verticalGroup(parent, 8);
                auto* layout = static_cast<QVBoxLayout*>(group->layout());

                auto mini = makeMiniWindowFrame(group, 620);
                auto* titleBar = mini.titlePreview->titleBar();
                titleBar->setContentWidget(makeTitleBarContent(titleBar, QStringLiteral("Project window"), true));

                auto* heading = makeSampleLabel(mini.shell, QStringLiteral("TitleBar safe regions"),
                    Typography::FontRole::BodyStrong);
                auto* caption = makeSampleLabel(mini.shell,
                    QStringLiteral(
                        "Custom title-bar content remains centered inside reserved leading and trailing "
                        "system regions."),
                    Typography::FontRole::Caption);
                caption->setTextColorRole(Label::TextColorRole::Secondary);
                auto* status = makeStatusPill(mini.shell, QString());
                auto updateStatus = [titleBar, status]() {
                    status->setText(QStringLiteral("Leading %1 px | Trailing %2 px | %3 exclusions")
                            .arg(titleBar->systemReservedLeadingWidth())
                            .arg(titleBar->systemReservedTrailingWidth())
                            .arg(titleBar->dragExclusionRects().size()));
                };
                QObject::connect(titleBar, &TitleBar::chromeGeometryChanged, status, updateStatus);
                QTimer::singleShot(0, status, updateStatus);

                mini.bodyLayout->addWidget(heading);
                mini.bodyLayout->addWidget(caption);
                mini.bodyLayout->addWidget(status, 0, Qt::AlignLeft);
                layout->addWidget(mini.shell);
                return group;
            }),
        makeSample(QStringLiteral("title-bar-height-exclusions"), QStringLiteral("Height and drag exclusions"),
            QStringLiteral("Adjust TitleBar height while platform chrome "
                           "controls stay aligned with drag exclusions."),
            QStringLiteral(
                "auto* titleBar = new TitleBar(this);\n"
                "titleBar->setTitleBarHeight(48);\n"
                "titleBar->setContentWidget(toolbarContent);\n"
                "\n"
                "connect(titleBar, &TitleBar::chromeGeometryChanged,\n"
                "        this, [titleBar] {\n"
                "            const auto exclusions = "
                "titleBar->dragExclusionRects();\n"
                "        });\n"
                "\n"
                "titleBar->refreshChromeExclusions();"),
            [](QWidget* parent) {
                auto* group = verticalGroup(parent, 10);
                auto* layout = static_cast<QVBoxLayout*>(group->layout());

                auto mini = makeMiniWindowFrame(group, 620);
                auto* titleBar = mini.titlePreview->titleBar();
                titleBar->setTitleBarHeight(48);
                titleBar->setContentWidget(makeTitleBarContent(titleBar, QStringLiteral("Review"), false));

                auto* controls = horizontalGroup(mini.shell, 8);
                auto* controlsLayout = static_cast<QHBoxLayout*>(controls->layout());
                auto* compact = makeSampleButton(controls, QStringLiteral("Compact"));
                auto* tall = makeSampleButton(controls, QStringLiteral("Tall"), Button::Accent);
                auto* refresh = makeSampleButton(controls, QStringLiteral("Refresh"));
                refresh->setFluentStyle(Button::Subtle);
                refresh->setFluentLayout(Button::IconBefore);
                refresh->setIconGlyph(Typography::Icons::Refresh, Typography::IconSize::Standard);
                controlsLayout->addWidget(compact, 0, Qt::AlignVCenter);
                controlsLayout->addWidget(tall, 0, Qt::AlignVCenter);
                controlsLayout->addWidget(refresh, 0, Qt::AlignVCenter);

                auto* status = makeStatusPill(controls, QString());
                controlsLayout->addWidget(status, 0, Qt::AlignVCenter);
                mini.bodyLayout->addWidget(controls);

                auto updateStatus = [titleBar, status, compact, tall]() {
                    status->setText(QStringLiteral("%1 px height | %2 exclusions")
                            .arg(titleBar->titleBarHeight())
                            .arg(titleBar->dragExclusionRects().size()));
                    const bool compactSelected = titleBar->titleBarHeight() <= 32;
                    compact->setFluentStyle(compactSelected ? Button::Accent : Button::Standard);
                    tall->setFluentStyle(compactSelected ? Button::Standard : Button::Accent);
                };
                QObject::connect(titleBar, &TitleBar::titleBarHeightChanged, status, [updateStatus](int) {
                    updateStatus();
                });
                QObject::connect(titleBar, &TitleBar::chromeGeometryChanged, status, updateStatus);
                QObject::connect(compact, &Button::clicked, titleBar, [titleBar]() {
                    titleBar->setTitleBarHeight(32);
                });
                QObject::connect(tall, &Button::clicked, titleBar, [titleBar]() { titleBar->setTitleBarHeight(48); });
                QObject::connect(refresh, &Button::clicked, titleBar, [titleBar]() {
                    titleBar->refreshChromeExclusions();
                });
                QTimer::singleShot(0, status, updateStatus);

                layout->addWidget(mini.shell);
                return group;
            })};
}

QVector<GallerySample> windowSamples()
{
    return {
        makeSample(QStringLiteral("window-content-host"), QStringLiteral("Top-level content host"),
            QStringLiteral("Request Mica for a Fluent Window and assign content to its host. "
                           "backdropState() reports the applied effect or fallback."),
            QStringLiteral(
                "auto* window = new Window();\n"
                "window->setAttribute(Qt::WA_DeleteOnClose);\n"
                "window->setWindowTitle(\"Fluent window\");\n"
                "window->setCustomWindowChromeEnabled(true);\n"
                "window->setBackdropEffect(BackdropEffect::Mica);\n"
                "\n"
                "auto* content = new QWidget();\n"
                "content->setAutoFillBackground(false);\n"
                "window->setContentWidget(content);\n"
                "window->resize(640, 520);\n"
                "window->show();"),
            [](QWidget* parent) {
                auto* group = verticalGroup(parent, 10);
                auto* layout = static_cast<QVBoxLayout*>(group->layout());

                auto mini = makeMiniWindowFrame(group, 620);
                mini.titlePreview->titleBar()->setContentWidget(makeTitleBarContent(mini.titlePreview->titleBar(),
                    QStringLiteral("Fluent content window"), false, QString()));

                auto* heading = makeSampleLabel(mini.shell, QStringLiteral("Hosted content"),
                    Typography::FontRole::Subtitle);
                auto* subtitle = makeSampleLabel(mini.shell,
                    QStringLiteral(
                        "Top-level shell with native/custom chrome, Mica backdrop, and Window::contentHost()."),
                    Typography::FontRole::Caption);
                subtitle->setTextColorRole(Label::TextColorRole::Secondary);
                mini.bodyLayout->addWidget(heading);
                mini.bodyLayout->addWidget(subtitle);

                auto* surfaceCard = new Card(mini.shell);
                surfaceCard->setAppearance(Card::LayerAlt);
                auto* cardLayout = new QHBoxLayout(surfaceCard);
                cardLayout->setContentsMargins(16, 12, 16, 12);
                cardLayout->setSpacing(10);
                auto* open = makeCommandButton(surfaceCard, QStringLiteral("Open window"),
                    Typography::Icons::BackToWindow);
                auto* status = makeStatusPill(surfaceCard, QStringLiteral("Closed"));
                auto* backdropTag = makeStatusPill(surfaceCard, QStringLiteral("Mica backdrop"));
                cardLayout->addWidget(open, 0, Qt::AlignVCenter);
                cardLayout->addWidget(status, 0, Qt::AlignVCenter);
                cardLayout->addStretch(1);
                cardLayout->addWidget(backdropTag, 0, Qt::AlignVCenter);
                mini.bodyLayout->addWidget(surfaceCard);

                layout->addWidget(mini.shell);

                QObject::connect(open, &Button::clicked, group, [group, open, status]() {
                    static QPointer<Window> demoWindow;
                    if (demoWindow) {
                        ::fluent::overlay::syncInheritedThemeOverride(demoWindow, open);
                        demoWindow->onThemeUpdated();
                        focusWindow(demoWindow);
                        status->setText(QStringLiteral("Focused"));
                        return;
                    }

                    auto* window = new Window();
                    ::fluent::overlay::syncInheritedThemeOverride(window, open);
                    window->onThemeUpdated();
                    demoWindow = window;
                    window->setAttribute(Qt::WA_DeleteOnClose);
                    window->setWindowTitle(QStringLiteral("Fluent content window"));
                    window->setCustomWindowChromeEnabled(true);
                    window->setBackdropEffect(fluent::windowing::BackdropEffect::Mica);
                    window->setContentWidget(makeWindowContent(QStringLiteral("Hosted content"),
                        QStringLiteral(
                            "This widget is parented under "
                            "Window::contentHost().")));
                    window->onThemeUpdated();
                    QObject::connect(window, &QObject::destroyed, status, [status]() {
                        status->setText(QStringLiteral("Closed"));
                    });
                    showWindowNear(window, group, QSize(640, 520));
                    status->setText(QStringLiteral("Open"));
                });
                return group;
            }),
        makeSample(QStringLiteral("window-custom-titlebar"), QStringLiteral("Window with custom title bar content"),
            QStringLiteral("Use Window's built-in TitleBar to host search or "
                           "actions while keeping chrome behavior."),
            QStringLiteral(
                "auto* window = new Window();\n"
                "window->setWindowTitle(\"Custom title bar\");\n"
                "window->setCustomWindowChromeEnabled(true);\n"
                "window->setBackdropEffect(BackdropEffect::Mica);\n"
                "window->setCaptionButtonToolTips(\"Minimize\", \"Maximize\",\n"
                "                                 \"Close\", \"Restore\");\n"
                "window->setCaptionButtonAccessibleNames(\"Minimize\", "
                "\"Maximize\",\n"
                "                                        \"Close\", "
                "\"Restore\");\n"
                "\n"
                "auto* titleBarContent = new QWidget(window->titleBar());\n"
                "auto* layout = new QHBoxLayout(titleBarContent);\n"
                "layout->addWidget(titleLabel);\n"
                "layout->addWidget(searchBox);\n"
                "layout->addWidget(shareButton);\n"
                "\n"
                "window->titleBar()->setContentWidget(titleBarContent);\n"
                "window->titleBar()->refreshChromeExclusions();\n"
                "window->setContentWidget(pageContent);\n"
                "window->resize(720, 520);\n"
                "window->show();"),
            [](QWidget* parent) {
                auto* group = verticalGroup(parent, 10);
                auto* layout = static_cast<QVBoxLayout*>(group->layout());

                auto mini = makeMiniWindowFrame(group, 620);
                mini.titlePreview->titleBar()->setContentWidget(
                    makeTitleBarContent(mini.titlePreview->titleBar(), QStringLiteral("Samples"), true));

                auto* heading = makeSampleLabel(mini.shell, QStringLiteral("Custom title bar"),
                    Typography::FontRole::Subtitle);
                auto* subtitle = makeSampleLabel(mini.shell,
                    QStringLiteral(
                        "Search, navigation, and actions live inside Window::titleBar() while staying clickable."),
                    Typography::FontRole::Caption);
                subtitle->setTextColorRole(Label::TextColorRole::Secondary);
                mini.bodyLayout->addWidget(heading);
                mini.bodyLayout->addWidget(subtitle);

                auto* surfaceCard = new Card(mini.shell);
                surfaceCard->setAppearance(Card::LayerAlt);
                auto* cardLayout = new QHBoxLayout(surfaceCard);
                cardLayout->setContentsMargins(16, 12, 16, 12);
                cardLayout->setSpacing(10);
                auto* open = makeCommandButton(surfaceCard, QStringLiteral("Open custom"),
                    Typography::Icons::BackToWindow);
                auto* status = makeStatusPill(surfaceCard, QStringLiteral("Closed"));
                cardLayout->addWidget(open, 0, Qt::AlignVCenter);
                cardLayout->addWidget(status, 0, Qt::AlignVCenter);
                cardLayout->addStretch(1);
                mini.bodyLayout->addWidget(surfaceCard);

                layout->addWidget(mini.shell);

                QObject::connect(open, &Button::clicked, group, [group, open, status]() {
                    static QPointer<Window> demoWindow;
                    if (demoWindow) {
                        ::fluent::overlay::syncInheritedThemeOverride(demoWindow, open);
                        demoWindow->onThemeUpdated();
                        focusWindow(demoWindow);
                        status->setText(QStringLiteral("Focused"));
                        return;
                    }

                    auto* window = new Window();
                    ::fluent::overlay::syncInheritedThemeOverride(window, open);
                    window->onThemeUpdated();
                    demoWindow = window;
                    window->setAttribute(Qt::WA_DeleteOnClose);
                    window->setWindowTitle(QStringLiteral("Custom title bar"));
                    window->setCustomWindowChromeEnabled(true);
                    window->setBackdropEffect(fluent::windowing::BackdropEffect::Mica);
                    window->setCaptionButtonToolTips(QStringLiteral("Minimize"), QStringLiteral("Maximize"),
                        QStringLiteral("Close"), QStringLiteral("Restore"));
                    window->setCaptionButtonAccessibleNames(QStringLiteral("Minimize"), QStringLiteral("Maximize"),
                        QStringLiteral("Close"), QStringLiteral("Restore"));
                    window->titleBar()->setContentWidget(
                        makeTitleBarContent(window->titleBar(), QStringLiteral("Samples"), true));
                    window->titleBar()->refreshChromeExclusions();
                    window->setContentWidget(makeWindowContent(QStringLiteral("Custom title bar"),
                        QStringLiteral(
                            "Interactive title-bar children are "
                            "excluded from drag hit testing.")));
                    window->onThemeUpdated();
                    QObject::connect(window, &QObject::destroyed, status, [status]() {
                        status->setText(QStringLiteral("Closed"));
                    });
                    showWindowNear(window, group, QSize(720, 520));
                    status->setText(QStringLiteral("Open"));
                });
                return group;
            })};
}

} // namespace

QVector<GallerySample> windowingSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("title-bar"))
        return titleBarSamples();
    if (routeId == QStringLiteral("window"))
        return windowSamples();
    return {};
}

} // namespace fluent::gallery

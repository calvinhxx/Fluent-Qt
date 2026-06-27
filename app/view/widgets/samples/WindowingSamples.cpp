#include "WindowingSamples.h"

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QFont>
#include <QPainter>
#include <QPoint>
#include <QPointer>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "components/windowing/Window.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::textfields::AutoSuggestBox;
using fluent::textfields::Label;
using fluent::windowing::TitleBar;
using fluent::windowing::Window;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

constexpr int kMacTrafficLightsReservedWidth = 78;
constexpr int kWindowsCaptionButtonWidth = 46;
constexpr int kWindowsCaptionButtonsReservedWidth = kWindowsCaptionButtonWidth * 3;

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

Label* makeSampleLabel(QWidget* parent,
                       const QString& text,
                       const QString& role = Typography::FontRole::Body)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(role);
    label->setWordWrap(true);
    label->setTextColorRole(Label::TextColorRole::Primary);  // QSS-proof on the styled preview surface
    return label;
}

Button* makeSampleButton(QWidget* parent,
                         const QString& text,
                         Button::ButtonStyle style = Button::Standard)
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
    button->setIconGlyph(glyph, 14);
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
    search->setSuggestions(QStringList{
        QStringLiteral("TitleBar"),
        QStringLiteral("Window"),
        QStringLiteral("Drag regions")
    });
    search->setQueryIconVisible(false);
    search->setFontRole(Typography::FontRole::Caption);
    search->setSuggestionFontRole(Typography::FontRole::Caption);
    search->setInputHeight(28);
    search->setQueryButtonSize(16);
    search->setClearButtonSize(16);
    search->setSuggestionItemHeight(24);
    search->setFixedSize(160, 28);
    return search;
}

// macOS traffic lights, custom-painted so they read faithfully: three colored dots at the leading
// edge (close / minimize / zoom), the ×/−/+ glyphs revealed only while the cluster is hovered, and a
// neutral gray when the host window is inactive — matching how AppKit dims its window controls.
// zh_CN: 自绘 macOS 红绿灯,力求传神:前导三色圆点(关闭/最小化/缩放),仅在悬停簇上时显出 ×/−/+ 字形,
// 宿主窗口失焦时转中性灰——对齐 AppKit 对窗口控件的变暗处理。
class MacTrafficLights : public QWidget, public fluent::FluentElement
{
public:
    explicit MacTrafficLights(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedWidth(kMacTrafficLightsReservedWidth);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
    }

    void onThemeUpdated() override { update(); }

protected:
    void enterEvent(QEnterEvent*) override
    {
        m_hovered = true;
        update();
    }

    void leaveEvent(QEvent*) override
    {
        m_hovered = false;
        update();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QWidget* top = window();
        const bool active = !top || top->isActiveWindow();

        // close / minimize / zoom, left-to-right (the canonical macOS order). zh_CN: 关闭/最小化/缩放,自左向右。
        static constexpr int kDotDiameter = 12;
        static constexpr int kLeadingMargin = 14;
        static constexpr int kSpacing = 8;
        const qreal radius = kDotDiameter / 2.0;
        const qreal cy = height() / 2.0;
        const QColor colors[3] = {QColor(0xFF, 0x5F, 0x57), QColor(0xFF, 0xBD, 0x2E), QColor(0x28, 0xC8, 0x40)};
        const QColor inactive(0xC8, 0xC8, 0xC8);

        qreal cx[3];
        for (int i = 0; i < 3; ++i)
            cx[i] = kLeadingMargin + radius + i * (kDotDiameter + kSpacing);

        painter.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) {
            painter.setBrush(active ? colors[i] : inactive);
            painter.drawEllipse(QPointF(cx[i], cy), radius, radius);
        }

        if (m_hovered && active) {
            QPen glyphPen(QColor(0, 0, 0, 150));
            glyphPen.setWidthF(1.2);
            glyphPen.setCapStyle(Qt::RoundCap);
            painter.setPen(glyphPen);
            const qreal g = 3.0;  // glyph half-extent. zh_CN: 字形半幅。
            // close ×
            painter.drawLine(QPointF(cx[0] - g, cy - g), QPointF(cx[0] + g, cy + g));
            painter.drawLine(QPointF(cx[0] - g, cy + g), QPointF(cx[0] + g, cy - g));
            // minimize −
            painter.drawLine(QPointF(cx[1] - g, cy), QPointF(cx[1] + g, cy));
            // zoom +
            painter.drawLine(QPointF(cx[2] - g, cy), QPointF(cx[2] + g, cy));
            painter.drawLine(QPointF(cx[2], cy - g), QPointF(cx[2], cy + g));
        }
    }

private:
    bool m_hovered = false;
};

Button* makeWindowsCaptionButton(QWidget* parent,
                                 const QString& glyph,
                                 const QString& tooltip,
                                 bool critical = false)
{
    auto* button = new Button(parent);
    fluent::status_info::ToolTip::attach(button, tooltip);
    button->setFluentStyle(Button::Subtle);
    button->setFluentLayout(Button::IconOnly);
    button->setFluentSize(Button::Small);
    button->setIconGlyph(glyph, 10);
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
    layout->addWidget(makeWindowsCaptionButton(buttons,
                                               Typography::Icons::ChromeMinimize,
                                               QStringLiteral("Minimize")));
    layout->addWidget(makeWindowsCaptionButton(buttons,
                                               Typography::Icons::ChromeMaximize,
                                               QStringLiteral("Maximize")));
    layout->addWidget(makeWindowsCaptionButton(buttons,
                                               Typography::Icons::ChromeClose,
                                               QStringLiteral("Close"),
                                               true));
    return buttons;
}

// Title-bar preview that swaps its caption controls by the active DESIGN LANGUAGE (not the host OS):
// Cupertino shows leading macOS traffic lights, Fluent/Material show trailing Windows-style caption
// buttons. It rebuilds live on a design-language switch, so toggling Settings → Style theme reskins
// the preview in place. zh_CN: 按当前**设计语言**(而非宿主系统)切换 caption 控件的标题栏预览:Cupertino 显前导
// macOS 红绿灯,Fluent/Material 显尾部 Windows 风格标题栏按钮。设计语言切换时实时重建,故在 设置→样式主题 切换会
// 原地重新换肤。
class TitleBarPreview : public QWidget, public fluent::FluentElement {
public:
    explicit TitleBarPreview(int width, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_titleBar(new TitleBar(this))
    {
        setFixedSize(width, TitleBar::defaultTitleBarHeight());
        rebuildPlatformControls();
        connect(m_titleBar, &TitleBar::titleBarHeightChanged, this, [this](int height) {
            setFixedHeight(height);
            updateChromeGeometry();
        });
        updateChromeGeometry();
    }

    TitleBar* titleBar() const { return m_titleBar; }
    TitleBarCaptionStyle captionStyle() const { return m_captionStyle; }

    void onThemeUpdated() override
    {
        // A design-language switch re-themes visible elements; rebuild the caption controls if the
        // language now calls for a different style. zh_CN: 设计语言切换会重刷可见元素;若新语言需要不同样式则重建。
        if (!m_platformControls || captionStyleForDesignLanguage(themeDesignLanguage()) != m_captionStyle)
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
        m_captionStyle = captionStyleForDesignLanguage(themeDesignLanguage());
        if (m_platformControls) {
            delete m_platformControls;
            m_platformControls = nullptr;
        }

        if (m_captionStyle == TitleBarCaptionStyle::MacTrafficLights) {
            m_platformControls = new MacTrafficLights(m_titleBar);
            m_titleBar->setSystemReservedLeadingWidth(kMacTrafficLightsReservedWidth);
            m_titleBar->setSystemReservedTrailingWidth(0);
        } else {
            m_platformControls = makeWindowsCaptionButtons(m_titleBar);
            m_titleBar->setSystemReservedLeadingWidth(0);
            m_titleBar->setSystemReservedTrailingWidth(kWindowsCaptionButtonsReservedWidth);
        }
        m_platformControls->show();
        updateChromeGeometry();
    }

    void updateChromeGeometry()
    {
        m_titleBar->setGeometry(rect());
        if (!m_platformControls)
            return;

        if (m_captionStyle == TitleBarCaptionStyle::WindowsCaptionButtons) {
            m_platformControls->setGeometry(width() - kWindowsCaptionButtonsReservedWidth,
                                            0,
                                            kWindowsCaptionButtonsReservedWidth,
                                            height());
            const auto captionButtons = m_platformControls->findChildren<Button*>();
            for (Button* button : captionButtons)
                button->setFixedSize(kWindowsCaptionButtonWidth, height());
        } else {
            m_platformControls->setGeometry(0, 0, kMacTrafficLightsReservedWidth, height());
        }
        m_platformControls->raise();
    }

    TitleBar* m_titleBar = nullptr;
    QWidget* m_platformControls = nullptr;
    TitleBarCaptionStyle m_captionStyle = TitleBarCaptionStyle::WindowsCaptionButtons;
};

QWidget* makeTitleBarContent(QWidget* parent,
                             const QString& titleText,
                             bool includeSearch)
{
    auto* content = new QWidget(parent);
    auto* layout = new QHBoxLayout(content);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignVCenter);

    auto* title = makeSampleLabel(content, titleText, Typography::FontRole::Caption);
    title->setMinimumWidth(96);
    QFont titleFont = title->font();
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    layout->addWidget(title, 0, Qt::AlignVCenter);

    if (includeSearch) {
        layout->addWidget(makeTitleBarSearch(content), 0, Qt::AlignVCenter);
    } else {
        layout->addStretch(1);
    }

    auto* share = makeSampleButton(content, QStringLiteral("Share"), Button::Subtle);
    share->setMinimumWidth(64);
    layout->addWidget(share, 0, Qt::AlignVCenter);
    return content;
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
    window->resize(size);
    if (launcher) {
        const QPoint center = launcher->mapToGlobal(launcher->rect().center());
        window->move(center - QPoint(size.width() / 2, size.height() / 2));
    }
    window->show();
    window->raise();
    window->activateWindow();
}

void focusWindow(Window* window)
{
    if (!window)
        return;
    window->showNormal();
    window->raise();
    window->activateWindow();
}

QVector<GallerySample> titleBarSamples()
{
    return {
        makeSample(QStringLiteral("title-bar-content-regions"),
                   QStringLiteral("Content respects system regions"),
                   QStringLiteral("Place custom content in TitleBar while preserving macOS leading controls or Windows trailing caption buttons."),
                   QStringLiteral("auto* titleBar = new TitleBar(this);\n"
                                  "// macOS reserves leading traffic-light controls;\n"
                                  "// Windows reserves trailing caption buttons.\n"
                                  "titleBar->setSystemReservedLeadingWidth(platformLeadingWidth);\n"
                                  "titleBar->setSystemReservedTrailingWidth(platformTrailingWidth);\n"
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

                       auto* preview = new TitleBarPreview(620, group);
                       auto* titleBar = preview->titleBar();
                       titleBar->setContentWidget(makeTitleBarContent(
                           titleBar,
                           QStringLiteral("Project window"),
                           true));

                       auto* status = makeStatusPill(group, QString());
                       auto updateStatus = [titleBar, status]() {
                           status->setText(QStringLiteral("Leading %1 px | Trailing %2 px | %3 exclusions")
                                               .arg(titleBar->systemReservedLeadingWidth())
                                               .arg(titleBar->systemReservedTrailingWidth())
                                               .arg(titleBar->dragExclusionRects().size()));
                       };
                       QObject::connect(titleBar, &TitleBar::chromeGeometryChanged,
                                        status, updateStatus);
                       QTimer::singleShot(0, status, updateStatus);

                       layout->addWidget(preview);
                       layout->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("title-bar-height-exclusions"),
                   QStringLiteral("Height and drag exclusions"),
                   QStringLiteral("Adjust TitleBar height while platform chrome controls stay aligned with drag exclusions."),
                   QStringLiteral("auto* titleBar = new TitleBar(this);\n"
                                  "titleBar->setTitleBarHeight(48);\n"
                                  "titleBar->setContentWidget(toolbarContent);\n"
                                  "\n"
                                  "connect(titleBar, &TitleBar::chromeGeometryChanged,\n"
                                  "        this, [titleBar] {\n"
                                  "            const auto exclusions = titleBar->dragExclusionRects();\n"
                                  "        });\n"
                                  "\n"
                                  "titleBar->refreshChromeExclusions();"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 8);
                       auto* layout = static_cast<QVBoxLayout*>(group->layout());

                       auto* preview = new TitleBarPreview(620, group);
                       auto* titleBar = preview->titleBar();
                       titleBar->setTitleBarHeight(48);
                       titleBar->setContentWidget(makeTitleBarContent(
                           titleBar,
                           QStringLiteral("Review"),
                           false));

                       auto* controls = horizontalGroup(group, 8);
                       auto* controlsLayout = static_cast<QHBoxLayout*>(controls->layout());
                       auto* compact = makeSampleButton(controls, QStringLiteral("Compact"));
                       auto* tall = makeSampleButton(controls, QStringLiteral("Tall"), Button::Accent);
                       auto* refresh = makeSampleButton(controls, QStringLiteral("Refresh"));
                       refresh->setFluentStyle(Button::Subtle);
                       refresh->setFluentLayout(Button::IconBefore);
                       refresh->setIconGlyph(Typography::Icons::Refresh, 14);
                       controlsLayout->addWidget(compact, 0, Qt::AlignVCenter);
                       controlsLayout->addWidget(tall, 0, Qt::AlignVCenter);
                       controlsLayout->addWidget(refresh, 0, Qt::AlignVCenter);

                       auto* status = makeStatusPill(controls, QString());
                       controlsLayout->addWidget(status, 0, Qt::AlignVCenter);

                       auto updateStatus = [titleBar, status, compact, tall]() {
                           status->setText(QStringLiteral("%1 px height | %2 exclusions")
                                               .arg(titleBar->titleBarHeight())
                                               .arg(titleBar->dragExclusionRects().size()));
                           const bool compactSelected = titleBar->titleBarHeight() <= 32;
                           compact->setFluentStyle(compactSelected ? Button::Accent : Button::Standard);
                           tall->setFluentStyle(compactSelected ? Button::Standard : Button::Accent);
                       };
                       QObject::connect(titleBar, &TitleBar::titleBarHeightChanged,
                                        status, [updateStatus](int) { updateStatus(); });
                       QObject::connect(titleBar, &TitleBar::chromeGeometryChanged,
                                        status, updateStatus);
                       QObject::connect(compact, &Button::clicked, titleBar, [titleBar]() {
                           titleBar->setTitleBarHeight(32);
                       });
                       QObject::connect(tall, &Button::clicked, titleBar, [titleBar]() {
                           titleBar->setTitleBarHeight(48);
                       });
                       QObject::connect(refresh, &Button::clicked, titleBar, [titleBar]() {
                           titleBar->refreshChromeExclusions();
                       });
                       QTimer::singleShot(0, status, updateStatus);

                       layout->addWidget(preview);
                       layout->addWidget(controls);
                       return group;
                   })
    };
}

QVector<GallerySample> windowSamples()
{
    return {
        makeSample(QStringLiteral("window-content-host"),
                   QStringLiteral("Top-level content host"),
                   QStringLiteral("Open a Fluent Window and assign caller-owned content to its content host."),
                   QStringLiteral("auto* window = new Window();\n"
                                  "window->setAttribute(Qt::WA_DeleteOnClose);\n"
                                  "window->setWindowTitle(\"Fluent window\");\n"
                                  "\n"
                                  "auto* content = new QWidget();\n"
                                  "window->setContentWidget(content);\n"
                                  "window->resize(520, 340);\n"
                                  "window->show();"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 8);
                       auto* layout = static_cast<QVBoxLayout*>(group->layout());
                       auto* controls = horizontalGroup(group, 10);
                       auto* controlsLayout = static_cast<QHBoxLayout*>(controls->layout());
                       auto* open = makeCommandButton(controls,
                                                      QStringLiteral("Open window"),
                                                      Typography::Icons::BackToWindow);
                       auto* status = makeStatusPill(controls, QStringLiteral("Closed"));
                       controlsLayout->addWidget(open, 0, Qt::AlignVCenter);
                       controlsLayout->addWidget(status, 0, Qt::AlignVCenter);
                       layout->addWidget(controls);

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
                           window->setContentWidget(makeWindowContent(
                               QStringLiteral("Hosted content"),
                               QStringLiteral("This widget is parented under Window::contentHost().")));
                           window->onThemeUpdated();
                           QObject::connect(window, &QObject::destroyed, status, [status]() {
                               status->setText(QStringLiteral("Closed"));
                           });
                           showWindowNear(window, group, QSize(520, 340));
                           status->setText(QStringLiteral("Open"));
                       });
                       return group;
                   }),
        makeSample(QStringLiteral("window-custom-titlebar"),
                   QStringLiteral("Window with custom title bar content"),
                   QStringLiteral("Use Window's built-in TitleBar to host search or actions while keeping chrome behavior."),
                   QStringLiteral("auto* window = new Window();\n"
                                  "window->setWindowTitle(\"Custom title bar\");\n"
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
                                  "window->show();"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 8);
                       auto* layout = static_cast<QVBoxLayout*>(group->layout());
                       auto* controls = horizontalGroup(group, 10);
                       auto* controlsLayout = static_cast<QHBoxLayout*>(controls->layout());
                       auto* open = makeCommandButton(controls,
                                                      QStringLiteral("Open custom"),
                                                      Typography::Icons::BackToWindow);
                       auto* status = makeStatusPill(controls, QStringLiteral("Closed"));
                       controlsLayout->addWidget(open, 0, Qt::AlignVCenter);
                       controlsLayout->addWidget(status, 0, Qt::AlignVCenter);
                       layout->addWidget(controls);

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
                           window->titleBar()->setContentWidget(makeTitleBarContent(
                               window->titleBar(),
                               QStringLiteral("Samples"),
                               true));
                           window->titleBar()->refreshChromeExclusions();
                           window->setContentWidget(makeWindowContent(
                               QStringLiteral("Custom title bar"),
                               QStringLiteral("Interactive title-bar children are excluded from drag hit testing.")));
                           window->onThemeUpdated();
                           QObject::connect(window, &QObject::destroyed, status, [status]() {
                               status->setText(QStringLiteral("Closed"));
                           });
                           showWindowNear(window, group, QSize(680, 360));
                           status->setText(QStringLiteral("Open"));
                       });
                       return group;
                   })
    };
}

} // namespace

TitleBarCaptionStyle captionStyleForDesignLanguage(fluent::FluentElement::DesignLanguage language)
{
    // Cupertino is the only language whose window controls are the leading traffic lights; Fluent and
    // Material both use a trailing minimize/maximize/close row (the Button paints each in its own
    // idiom). zh_CN: 仅 Cupertino 的窗口控件是前导红绿灯;Fluent 与 Material 都用尾部 最小化/最大化/关闭 行
    //(Button 各自按其语言绘制)。
    return language == fluent::FluentElement::DesignCupertino
               ? TitleBarCaptionStyle::MacTrafficLights
               : TitleBarCaptionStyle::WindowsCaptionButtons;
}

QVector<GallerySample> windowingSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("title-bar"))
        return titleBarSamples();
    if (routeId == QStringLiteral("window"))
        return windowSamples();
    return {};
}

} // namespace fluent::gallery

#include <FluentQt/FluentQt.h>

#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QtGlobal>
#include <QVBoxLayout>
#include <QWidget>

namespace {

using fluent::AnchorLayout;
using fluent::FluentElement;
using fluent::StyleTheme;
using fluent::ThemeRegistry;
using fluent::basicinput::Button;
using fluent::basicinput::ColorPicker;
using fluent::basicinput::ComboBox;
using fluent::basicinput::Slider;
using fluent::basicinput::ToggleSwitch;
using fluent::status_info::InfoBar;
using fluent::textfields::Label;
using fluent::windowing::Window;

namespace StyleThemeCatalog = fluent::StyleThemeCatalog;

using Edge = AnchorLayout::Edge;
using TextColorRole = Label::TextColorRole;

constexpr int kPageMargin = 32;
constexpr int kPanelGap = 24;
constexpr int kPanelPadding = 24;
constexpr int kHeaderTop = 28;
constexpr int kHeaderHeight = 92;
constexpr int kPanelHeight = 548;
constexpr int kAccentPanelWidth = 472;
constexpr int kAccentPickerSize = 420;

QColor defaultAccent()
{
    return QColor(0, 120, 212);
}

AnchorLayout::Anchor anchor(QWidget* target, Edge edge, int offset = 0)
{
    return {target, edge, offset};
}

Label* makeLabel(const QString& text, QWidget* parent, const QString& typography,
                 TextColorRole colorRole = TextColorRole::Default, bool wordWrap = false)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(typography);
    label->setTextColorRole(colorRole);
    label->setWordWrap(wordWrap);
    return label;
}

QVBoxLayout* makePanelLayout(QWidget* panel)
{
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(kPanelPadding, 20, kPanelPadding, kPanelPadding);
    layout->setSpacing(12);
    return layout;
}

QWidget* makeFieldRow(QWidget* parent, const QString& title, QWidget* control)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* label = makeLabel(title, row, Typography::FontRole::BodyStrong);
    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(control, 0, Qt::AlignRight);

    return row;
}

class SurfacePanel : public QWidget, public FluentElement {
public:
    explicit SurfacePanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
    }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF panelRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);

        painter.setPen(QPen(themeColors().strokeCard, 1));
        painter.setBrush(themeColors().bgLayer);
        painter.drawRoundedRect(panelRect, themeRadius().overlay, themeRadius().overlay);
    }
};

class AccentSwatch : public QWidget, public FluentElement {
public:
    explicit AccentSwatch(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(88, 38);
    }

    void setColor(const QColor& color)
    {
        if (m_color == color)
            return;

        m_color = color;
        update();
    }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF swatchRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);

        painter.setPen(QPen(themeColors().strokeDefault, 1));
        painter.setBrush(m_color);
        painter.drawRoundedRect(swatchRect, themeRadius().control, themeRadius().control);
    }

private:
    QColor m_color = defaultAccent();
};

class ExamplePanel : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
public:
    explicit ExamplePanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
        setMinimumSize(980, 700);

        buildHeader();
        buildAccentPanel();
        buildThemePanel();
        placeTopLevelWidgets();
        connectControls();

        applyDesignLanguage(0);
        applyFontScale(m_fontScaleSlider->value());
        onThemeUpdated();
    }

    void onThemeUpdated() override
    {
        update();
        refreshPanelChrome();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.fillRect(rect(), themeColors().bgCanvas);
    }

private:
    void buildHeader()
    {
        m_header = new QWidget(this);
        m_header->setFixedHeight(kHeaderHeight);

        auto* headerLayout = new QVBoxLayout(m_header);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(4);

        auto* titleRow = new QWidget(m_header);
        auto* titleLayout = new QHBoxLayout(titleRow);
        titleLayout->setContentsMargins(0, 0, 0, 0);
        titleLayout->setSpacing(16);

        auto* title = makeLabel(QStringLiteral("Fluent-Qt Example"), titleRow,
                                Typography::FontRole::Title);

        auto* iconButton = new Button(titleRow);
        iconButton->setFluentStyle(Button::Subtle);
        iconButton->setFluentLayout(Button::IconOnly);
        iconButton->setIconGlyph(Typography::Icons::FavoriteStarFill, 18);
        iconButton->setToolTip(QStringLiteral("Iconfont glyph from Segoe Fluent Icons"));
        iconButton->setFixedSize(40, 40);

        titleLayout->addWidget(title);
        titleLayout->addStretch();
        titleLayout->addWidget(iconButton);

        auto* subtitle = makeLabel(
            QStringLiteral("Reusable resources, runtime theme tokens, icon font, and Fluent controls in one native window."),
            m_header, Typography::FontRole::Body, TextColorRole::Secondary, true);

        headerLayout->addWidget(titleRow);
        headerLayout->addWidget(subtitle);
        headerLayout->addStretch();
    }

    void buildAccentPanel()
    {
        m_accentPanel = new SurfacePanel(this);
        m_accentPanel->setFixedSize(kAccentPanelWidth, kPanelHeight);

        auto* panelLayout = makePanelLayout(m_accentPanel);

        auto* titleRow = new QWidget(m_accentPanel);
        auto* titleLayout = new QHBoxLayout(titleRow);
        titleLayout->setContentsMargins(0, 0, 0, 0);
        titleLayout->setSpacing(16);

        auto* titleColumn = new QWidget(titleRow);
        auto* titleColumnLayout = new QVBoxLayout(titleColumn);
        titleColumnLayout->setContentsMargins(0, 0, 0, 0);
        titleColumnLayout->setSpacing(4);

        auto* title = makeLabel(QStringLiteral("Accent editor"), titleColumn,
                                Typography::FontRole::Subtitle);
        auto* description = makeLabel(
            QStringLiteral("Pick a color and the shared token registry updates every Fluent control."),
            titleColumn, Typography::FontRole::Caption, TextColorRole::Secondary, true);

        titleColumnLayout->addWidget(title);
        titleColumnLayout->addWidget(description);

        m_accentSwatch = new AccentSwatch(titleRow);

        titleLayout->addWidget(titleColumn);
        titleLayout->addStretch();
        titleLayout->addWidget(m_accentSwatch, 0, Qt::AlignTop);

        m_accentPicker = new ColorPicker(m_accentPanel);
        m_accentPicker->setAlphaEnabled(false);
        m_accentPicker->setColor(defaultAccent());
        m_accentPicker->setFixedSize(kAccentPickerSize, kAccentPickerSize);

        panelLayout->addWidget(titleRow);
        panelLayout->addSpacing(8);
        panelLayout->addWidget(m_accentPicker, 0, Qt::AlignHCenter);
    }

    void buildThemePanel()
    {
        m_themePanel = new SurfacePanel(this);
        m_themePanel->setMinimumSize(420, kPanelHeight);
        m_themePanel->setFixedHeight(kPanelHeight);

        auto* panelLayout = makePanelLayout(m_themePanel);

        auto* title = makeLabel(QStringLiteral("Style & accent from FluentQt"),
                                m_themePanel, Typography::FontRole::Subtitle);
        auto* description = makeLabel(
            QStringLiteral("This example calls StyleThemeCatalog and ThemeRegistry from uilib, then the controls update from shared tokens."),
            m_themePanel, Typography::FontRole::Caption, TextColorRole::Secondary, true);

        m_styleCombo = new ComboBox(m_themePanel);
        m_styleCombo->addItems({QStringLiteral("Fluent"), QStringLiteral("Material 3"),
                                QStringLiteral("macOS")});
        m_styleCombo->setFixedSize(176, 32);

        m_darkSwitch = new ToggleSwitch(m_themePanel);
        m_darkSwitch->setOnContent(QStringLiteral("Dark"));
        m_darkSwitch->setOffContent(QStringLiteral("Light"));
        m_darkSwitch->setFixedWidth(116);

        m_primaryButton = new Button(QStringLiteral("Apply uilib accent"), m_themePanel);
        m_primaryButton->setFluentStyle(Button::Accent);
        m_primaryButton->setFluentLayout(Button::IconBefore);
        m_primaryButton->setIconGlyph(Typography::Icons::CheckMark, 14);
        m_primaryButton->setMinimumHeight(40);
        m_primaryButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        m_resetButton = new Button(QStringLiteral("Reset tokens"), m_themePanel);
        m_resetButton->setFluentLayout(Button::IconBefore);
        m_resetButton->setIconGlyph(Typography::Icons::Refresh, 14);
        m_resetButton->setMinimumHeight(36);
        m_resetButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        m_fontScaleSlider = new Slider(Qt::Horizontal, m_themePanel);
        m_fontScaleSlider->setRange(90, 120);
        m_fontScaleSlider->setValue(100);

        m_status = new InfoBar(m_themePanel);
        m_status->setSeverity(InfoBar::Informational);
        m_status->setTitle(QStringLiteral("fluent::StyleThemeCatalog"));
        m_status->setMessage(QStringLiteral("apply(StyleTheme::Fluent)  applyAccentOverride(#0078D4)"));
        m_status->setSingleLine(false);
        m_status->setIsClosable(false);
        m_status->setMinimumHeight(92);

        panelLayout->addWidget(title);
        panelLayout->addWidget(description);
        panelLayout->addSpacing(16);
        panelLayout->addWidget(makeFieldRow(m_themePanel, QStringLiteral("Style language"), m_styleCombo));
        panelLayout->addWidget(makeFieldRow(m_themePanel, QStringLiteral("App theme"), m_darkSwitch));
        panelLayout->addSpacing(10);
        panelLayout->addWidget(makeLabel(QStringLiteral("Accent action"), m_themePanel,
                                         Typography::FontRole::BodyStrong));
        panelLayout->addWidget(m_primaryButton);
        panelLayout->addWidget(m_resetButton);
        panelLayout->addSpacing(10);
        panelLayout->addWidget(makeLabel(QStringLiteral("Font scale"), m_themePanel,
                                         Typography::FontRole::BodyStrong));
        panelLayout->addWidget(m_fontScaleSlider);
        panelLayout->addStretch();
        panelLayout->addWidget(m_status);
    }

    void placeTopLevelWidgets()
    {
        auto* rootLayout = new AnchorLayout(this);
        setLayout(rootLayout);

        AnchorLayout::Anchors headerAnchors;
        headerAnchors.left = anchor(this, Edge::Left, kPageMargin);
        headerAnchors.right = anchor(this, Edge::Right, -kPageMargin);
        headerAnchors.top = anchor(this, Edge::Top, kHeaderTop);
        rootLayout->addAnchoredWidget(m_header, headerAnchors);

        AnchorLayout::Anchors accentAnchors;
        accentAnchors.left = anchor(this, Edge::Left, kPageMargin);
        accentAnchors.top = anchor(m_header, Edge::Bottom, 20);
        rootLayout->addAnchoredWidget(m_accentPanel, accentAnchors);

        AnchorLayout::Anchors themeAnchors;
        themeAnchors.left = anchor(m_accentPanel, Edge::Right, kPanelGap);
        themeAnchors.right = anchor(this, Edge::Right, -kPageMargin);
        themeAnchors.top = anchor(m_accentPanel, Edge::Top);
        themeAnchors.bottom = anchor(m_accentPanel, Edge::Bottom);
        rootLayout->addAnchoredWidget(m_themePanel, themeAnchors);
    }

    void connectControls()
    {
        QObject::connect(m_darkSwitch, &ToggleSwitch::toggled, this, [this](bool dark) {
            FluentElement::setTheme(dark ? FluentElement::Dark : FluentElement::Light);
            showCatalogStatus(InfoBar::Informational, QStringLiteral("FluentElement::setTheme"));
        });

        QObject::connect(m_styleCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                         this, [this](int index) { applyDesignLanguage(index); });

        QObject::connect(m_accentPicker, &ColorPicker::colorChanged, this, [this](const QColor& color) {
            applyAccent(color);
            m_accentSwatch->setColor(color);
            showCatalogStatus(InfoBar::Success,
                              QStringLiteral("StyleThemeCatalog::applyAccentOverride"));
        });

        QObject::connect(m_fontScaleSlider, &Slider::valueChanged, this, [this](int value) {
            applyFontScale(value);
            showCatalogStatus(InfoBar::Informational, QStringLiteral("ThemeRegistry::setFontScale"));
        });

        QObject::connect(m_resetButton, &Button::clicked, this, [this]() {
            m_styleCombo->setCurrentIndex(0);
            m_accentPicker->setColor(defaultAccent());
            m_fontScaleSlider->setValue(100);
            m_darkSwitch->setIsOn(false);
            applyStyleTheme(0);
            showCatalogStatus(InfoBar::Informational, QStringLiteral("Built-in Fluent tokens"));
        });

        QObject::connect(m_primaryButton, &Button::clicked, this, [this]() {
            showCatalogStatus(InfoBar::Success, QStringLiteral("Accent button uses current token"));
        });
    }

    void applyDesignLanguage(int index)
    {
        applyStyleTheme(index);
        showCatalogStatus(InfoBar::Informational, QStringLiteral("StyleThemeCatalog::apply"));
    }

    void applyStyleTheme(int index)
    {
        m_currentStyleTheme = styleThemeFromIndex(index);
        StyleThemeCatalog::apply(m_currentStyleTheme);
        applyAccent(m_accentPicker ? m_accentPicker->color() : defaultAccent());
    }

    void applyAccent(const QColor& accent)
    {
        StyleThemeCatalog::applyAccentOverride(accent);
        FluentElement::refreshTheme();
    }

    void applyFontScale(int value)
    {
        ThemeRegistry::instance().setFontScale(value / 100.0);
        FluentElement::refreshTheme();
    }

    void showCatalogStatus(InfoBar::InfoBarSeverity severity, const QString& title)
    {
        if (!m_status)
            return;

        m_status->setSeverity(severity);
        m_status->setTitle(title);
        m_status->setMessage(catalogStatusText());
    }

    QString catalogStatusText() const
    {
        const QColor accent = m_accentPicker ? m_accentPicker->color() : defaultAccent();
        const int fontPercent = qRound(ThemeRegistry::instance().fontScale() * 100.0);

        return QStringLiteral("StyleThemeCatalog::apply(%1)\n"
                              "applyAccentOverride(%2)  %3  font %4%")
            .arg(styleThemeSymbol(), accent.name(QColor::HexRgb).toUpper(), designLanguageSymbol())
            .arg(fontPercent);
    }

    QString styleThemeSymbol() const
    {
        switch (m_currentStyleTheme) {
        case StyleTheme::Material:
            return QStringLiteral("StyleTheme::Material");
        case StyleTheme::MacOS:
            return QStringLiteral("StyleTheme::MacOS");
        case StyleTheme::Fluent:
        default:
            return QStringLiteral("StyleTheme::Fluent");
        }
    }

    QString designLanguageSymbol() const
    {
        switch (StyleThemeCatalog::designLanguageFor(m_currentStyleTheme)) {
        case FluentElement::DesignMaterial:
            return QStringLiteral("DesignMaterial");
        case FluentElement::DesignCupertino:
            return QStringLiteral("DesignCupertino");
        case FluentElement::DesignFluent:
        default:
            return QStringLiteral("DesignFluent");
        }
    }

    static StyleTheme styleThemeFromIndex(int index)
    {
        switch (index) {
        case 1:
            return StyleTheme::Material;
        case 2:
            return StyleTheme::MacOS;
        default:
            return StyleTheme::Fluent;
        }
    }

    void refreshPanelChrome()
    {
        if (m_accentPanel)
            m_accentPanel->onThemeUpdated();
        if (m_themePanel)
            m_themePanel->onThemeUpdated();
        if (m_accentSwatch)
            m_accentSwatch->onThemeUpdated();
    }

    QWidget* m_header = nullptr;
    SurfacePanel* m_accentPanel = nullptr;
    SurfacePanel* m_themePanel = nullptr;
    ComboBox* m_styleCombo = nullptr;
    ToggleSwitch* m_darkSwitch = nullptr;
    ColorPicker* m_accentPicker = nullptr;
    AccentSwatch* m_accentSwatch = nullptr;
    Slider* m_fontScaleSlider = nullptr;
    InfoBar* m_status = nullptr;
    Button* m_primaryButton = nullptr;
    Button* m_resetButton = nullptr;
    StyleTheme m_currentStyleTheme = StyleTheme::Fluent;
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Fluent-Qt Hello World"));
    QApplication::setOrganizationName(QStringLiteral("Fluent-Qt"));

    fluent::initializeResources();

    Window window;
    window.setWindowTitle(QStringLiteral("Fluent-Qt Hello World"));
    window.resize(1040, 760);
    window.setMinimumSize(1000, 730);

    auto* title = new Label(QStringLiteral("Fluent-Qt Hello World"), window.titleBar());
    title->setFluentTypography(Typography::FontRole::BodyStrong);
    window.titleBar()->setContentWidget(title);
    window.setContentWidget(new ExamplePanel(&window));

    window.show();
    return app.exec();
}

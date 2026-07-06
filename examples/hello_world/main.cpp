#include <FluentQt/FluentQt.h>

#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QPainter>
#include <QPaintEvent>
#include <QtGlobal>
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

AnchorLayout::Anchor edge(QWidget* target, AnchorLayout::Edge edge, int offset = 0)
{
    return AnchorLayout::Anchor(target, edge, offset);
}

class SurfacePanel : public QWidget, public FluentElement {
public:
    explicit SurfacePanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
    }

    void setPanelSizeHint(const QSize& size) { m_sizeHint = size; }
    QSize sizeHint() const override { return m_sizeHint; }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRectF panelRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(themeColors().strokeCard, 1));
        painter.setBrush(themeColors().bgLayer);
        painter.drawRoundedRect(panelRect, themeRadius().overlay, themeRadius().overlay);
    }

private:
    QSize m_sizeHint = QSize(360, 320);
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
        QRectF swatchRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(themeColors().strokeDefault, 1));
        painter.setBrush(m_color);
        painter.drawRoundedRect(swatchRect, themeRadius().control, themeRadius().control);
    }

private:
    QColor m_color = QColor(0, 120, 212);
};

class ExamplePanel : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
public:
    explicit ExamplePanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
        setMinimumSize(980, 700);
        buildUi();
        applyDesignLanguage(0);
        applyFontScale(m_fontScaleSlider->value());
        onThemeUpdated();
    }

    void onThemeUpdated() override
    {
        update();
        if (m_colorPanel)
            m_colorPanel->onThemeUpdated();
        if (m_controlPanel)
            m_controlPanel->onThemeUpdated();
        if (m_accentSwatch)
            m_accentSwatch->onThemeUpdated();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.fillRect(rect(), themeColors().bgCanvas);
    }

private:
    void buildUi()
    {
        m_rootLayout = new AnchorLayout(this);
        setLayout(m_rootLayout);

        buildHeader();
        buildColorPanel();
        buildControlPanel();
        wireInteractions();
    }

    void buildHeader()
    {
        m_header = new QWidget(this);
        m_header->setFixedHeight(92);

        auto* headerLayout = new AnchorLayout(m_header);
        m_header->setLayout(headerLayout);

        auto* title = new Label(QStringLiteral("Fluent-Qt Example"), m_header);
        title->setFluentTypography(Typography::FontRole::Title);

        auto* subtitle = new Label(
            QStringLiteral("Reusable resources, runtime theme tokens, icon font, and Fluent controls in one native window."),
            m_header);
        subtitle->setFluentTypography(Typography::FontRole::Body);
        subtitle->setTextColorRole(Label::TextColorRole::Secondary);

        auto* iconButton = new Button(m_header);
        iconButton->setFluentStyle(Button::Subtle);
        iconButton->setFluentLayout(Button::IconOnly);
        iconButton->setIconGlyph(Typography::Icons::FavoriteStarFill, 18);
        iconButton->setToolTip(QStringLiteral("Iconfont glyph from Segoe Fluent Icons"));
        iconButton->setFixedSize(40, 40);

        AnchorLayout::Anchors titleAnchors;
        titleAnchors.left = edge(m_header, AnchorLayout::Edge::Left);
        titleAnchors.top = edge(m_header, AnchorLayout::Edge::Top, 8);
        headerLayout->addAnchoredWidget(title, titleAnchors);

        AnchorLayout::Anchors subtitleAnchors;
        subtitleAnchors.left = edge(title, AnchorLayout::Edge::Left);
        subtitleAnchors.right = edge(iconButton, AnchorLayout::Edge::Left, -24);
        subtitleAnchors.top = edge(title, AnchorLayout::Edge::Bottom, 4);
        headerLayout->addAnchoredWidget(subtitle, subtitleAnchors);

        AnchorLayout::Anchors iconAnchors;
        iconAnchors.right = edge(m_header, AnchorLayout::Edge::Right);
        iconAnchors.verticalCenter = edge(m_header, AnchorLayout::Edge::VCenter);
        headerLayout->addAnchoredWidget(iconButton, iconAnchors);

        AnchorLayout::Anchors headerAnchors;
        headerAnchors.left = edge(this, AnchorLayout::Edge::Left, 32);
        headerAnchors.right = edge(this, AnchorLayout::Edge::Right, -32);
        headerAnchors.top = edge(this, AnchorLayout::Edge::Top, 28);
        m_rootLayout->addAnchoredWidget(m_header, headerAnchors);
    }

    void buildColorPanel()
    {
        m_colorPanel = new SurfacePanel(this);
        m_colorPanel->setPanelSizeHint(QSize(472, 548));
        m_colorPanel->setMinimumSize(472, 548);
        m_colorPanel->setMaximumWidth(472);

        auto* panelLayout = new AnchorLayout(m_colorPanel);
        m_colorPanel->setLayout(panelLayout);

        auto* title = new Label(QStringLiteral("Accent editor"), m_colorPanel);
        title->setFluentTypography(Typography::FontRole::Subtitle);

        auto* description = new Label(QStringLiteral("Pick a color and the shared token registry updates every Fluent control."), m_colorPanel);
        description->setFluentTypography(Typography::FontRole::Caption);
        description->setTextColorRole(Label::TextColorRole::Secondary);
        description->setWordWrap(true);

        m_accentSwatch = new AccentSwatch(m_colorPanel);

        m_accentPicker = new ColorPicker(m_colorPanel);
        m_accentPicker->setAlphaEnabled(false);
        m_accentPicker->setColor(QColor(0, 120, 212));
        m_accentPicker->setFixedSize(420, 420);

        AnchorLayout::Anchors titleAnchors;
        titleAnchors.left = edge(m_colorPanel, AnchorLayout::Edge::Left, 24);
        titleAnchors.top = edge(m_colorPanel, AnchorLayout::Edge::Top, 20);
        panelLayout->addAnchoredWidget(title, titleAnchors);

        AnchorLayout::Anchors swatchAnchors;
        swatchAnchors.right = edge(m_colorPanel, AnchorLayout::Edge::Right, -24);
        swatchAnchors.verticalCenter = edge(title, AnchorLayout::Edge::VCenter);
        panelLayout->addAnchoredWidget(m_accentSwatch, swatchAnchors);

        AnchorLayout::Anchors descriptionAnchors;
        descriptionAnchors.left = edge(title, AnchorLayout::Edge::Left);
        descriptionAnchors.right = edge(m_accentSwatch, AnchorLayout::Edge::Left, -16);
        descriptionAnchors.top = edge(title, AnchorLayout::Edge::Bottom, 4);
        panelLayout->addAnchoredWidget(description, descriptionAnchors);

        AnchorLayout::Anchors pickerAnchors;
        pickerAnchors.left = edge(m_colorPanel, AnchorLayout::Edge::Left, 24);
        pickerAnchors.top = edge(m_colorPanel, AnchorLayout::Edge::Top, 74);
        panelLayout->addAnchoredWidget(m_accentPicker, pickerAnchors);

        AnchorLayout::Anchors panelAnchors;
        panelAnchors.left = edge(this, AnchorLayout::Edge::Left, 32);
        panelAnchors.top = edge(m_header, AnchorLayout::Edge::Bottom, 20);
        m_rootLayout->addAnchoredWidget(m_colorPanel, panelAnchors);
    }

    void buildControlPanel()
    {
        m_controlPanel = new SurfacePanel(this);
        m_controlPanel->setPanelSizeHint(QSize(420, 548));
        m_controlPanel->setMinimumSize(420, 548);

        auto* panelLayout = new AnchorLayout(m_controlPanel);
        m_controlPanel->setLayout(panelLayout);

        auto* title = new Label(QStringLiteral("Style & accent (uilib)"), m_controlPanel);
        title->setFluentTypography(Typography::FontRole::Subtitle);

        auto* description = new Label(QStringLiteral("This standalone app calls fluent::StyleThemeCatalog and ThemeRegistry directly from FluentQt."), m_controlPanel);
        description->setFluentTypography(Typography::FontRole::Caption);
        description->setTextColorRole(Label::TextColorRole::Secondary);
        description->setWordWrap(true);

        m_styleCombo = new ComboBox(m_controlPanel);
        m_styleCombo->addItems({QStringLiteral("Fluent"), QStringLiteral("Material 3"),
                                QStringLiteral("macOS")});
        m_styleCombo->setFixedSize(176, 32);

        m_darkSwitch = new ToggleSwitch(m_controlPanel);
        m_darkSwitch->setOnContent(QStringLiteral("Dark"));
        m_darkSwitch->setOffContent(QStringLiteral("Light"));
        m_darkSwitch->setFixedWidth(116);

        auto* styleLabel = new Label(QStringLiteral("Style language"), m_controlPanel);
        styleLabel->setFluentTypography(Typography::FontRole::BodyStrong);

        auto* themeLabel = new Label(QStringLiteral("App theme"), m_controlPanel);
        themeLabel->setFluentTypography(Typography::FontRole::BodyStrong);

        auto* accentLabel = new Label(QStringLiteral("Accent action"), m_controlPanel);
        accentLabel->setFluentTypography(Typography::FontRole::BodyStrong);

        auto* primaryButton = new Button(QStringLiteral("Apply uilib accent"), m_controlPanel);
        primaryButton->setFluentStyle(Button::Accent);
        primaryButton->setFluentLayout(Button::IconBefore);
        primaryButton->setIconGlyph(Typography::Icons::CheckMark, 14);
        primaryButton->setMinimumHeight(40);

        auto* resetButton = new Button(QStringLiteral("Reset tokens"), m_controlPanel);
        resetButton->setFluentLayout(Button::IconBefore);
        resetButton->setIconGlyph(Typography::Icons::Refresh, 14);
        resetButton->setMinimumHeight(36);

        auto* scaleLabel = new Label(QStringLiteral("Font scale"), m_controlPanel);
        scaleLabel->setFluentTypography(Typography::FontRole::BodyStrong);

        m_fontScaleSlider = new Slider(Qt::Horizontal, m_controlPanel);
        m_fontScaleSlider->setRange(90, 120);
        m_fontScaleSlider->setValue(100);

        m_status = new InfoBar(m_controlPanel);
        m_status->setSeverity(InfoBar::Informational);
        m_status->setTitle(QStringLiteral("fluent::StyleThemeCatalog"));
        m_status->setMessage(QStringLiteral("apply(StyleTheme::Fluent)  applyAccentOverride(#0078D4)"));
        m_status->setSingleLine(false);
        m_status->setIsClosable(false);
        m_status->setPreferredWidth(360);
        m_status->setMinimumHeight(92);

        AnchorLayout::Anchors titleAnchors;
        titleAnchors.left = edge(m_controlPanel, AnchorLayout::Edge::Left, 24);
        titleAnchors.top = edge(m_controlPanel, AnchorLayout::Edge::Top, 20);
        panelLayout->addAnchoredWidget(title, titleAnchors);

        AnchorLayout::Anchors descriptionAnchors;
        descriptionAnchors.left = edge(title, AnchorLayout::Edge::Left);
        descriptionAnchors.right = edge(m_controlPanel, AnchorLayout::Edge::Right, -24);
        descriptionAnchors.top = edge(title, AnchorLayout::Edge::Bottom, 4);
        panelLayout->addAnchoredWidget(description, descriptionAnchors);

        AnchorLayout::Anchors styleLabelAnchors;
        styleLabelAnchors.left = edge(title, AnchorLayout::Edge::Left);
        styleLabelAnchors.top = edge(m_controlPanel, AnchorLayout::Edge::Top, 100);
        panelLayout->addAnchoredWidget(styleLabel, styleLabelAnchors);

        AnchorLayout::Anchors styleComboAnchors;
        styleComboAnchors.right = edge(m_controlPanel, AnchorLayout::Edge::Right, -24);
        styleComboAnchors.verticalCenter = edge(styleLabel, AnchorLayout::Edge::VCenter);
        panelLayout->addAnchoredWidget(m_styleCombo, styleComboAnchors);

        AnchorLayout::Anchors themeLabelAnchors;
        themeLabelAnchors.left = edge(title, AnchorLayout::Edge::Left);
        themeLabelAnchors.top = edge(styleLabel, AnchorLayout::Edge::Bottom, 30);
        panelLayout->addAnchoredWidget(themeLabel, themeLabelAnchors);

        AnchorLayout::Anchors switchAnchors;
        switchAnchors.right = edge(m_controlPanel, AnchorLayout::Edge::Right, -24);
        switchAnchors.verticalCenter = edge(themeLabel, AnchorLayout::Edge::VCenter);
        panelLayout->addAnchoredWidget(m_darkSwitch, switchAnchors);

        AnchorLayout::Anchors accentLabelAnchors;
        accentLabelAnchors.left = edge(title, AnchorLayout::Edge::Left);
        accentLabelAnchors.top = edge(themeLabel, AnchorLayout::Edge::Bottom, 34);
        panelLayout->addAnchoredWidget(accentLabel, accentLabelAnchors);

        AnchorLayout::Anchors primaryAnchors;
        primaryAnchors.left = edge(title, AnchorLayout::Edge::Left);
        primaryAnchors.right = edge(m_controlPanel, AnchorLayout::Edge::Right, -24);
        primaryAnchors.top = edge(accentLabel, AnchorLayout::Edge::Bottom, 10);
        panelLayout->addAnchoredWidget(primaryButton, primaryAnchors);

        AnchorLayout::Anchors resetAnchors;
        resetAnchors.left = edge(primaryButton, AnchorLayout::Edge::Left);
        resetAnchors.right = edge(primaryButton, AnchorLayout::Edge::Right);
        resetAnchors.top = edge(primaryButton, AnchorLayout::Edge::Bottom, 10);
        panelLayout->addAnchoredWidget(resetButton, resetAnchors);

        AnchorLayout::Anchors scaleLabelAnchors;
        scaleLabelAnchors.left = edge(primaryButton, AnchorLayout::Edge::Left);
        scaleLabelAnchors.top = edge(resetButton, AnchorLayout::Edge::Bottom, 26);
        panelLayout->addAnchoredWidget(scaleLabel, scaleLabelAnchors);

        AnchorLayout::Anchors sliderAnchors;
        sliderAnchors.left = edge(primaryButton, AnchorLayout::Edge::Left);
        sliderAnchors.right = edge(primaryButton, AnchorLayout::Edge::Right);
        sliderAnchors.top = edge(scaleLabel, AnchorLayout::Edge::Bottom, 10);
        panelLayout->addAnchoredWidget(m_fontScaleSlider, sliderAnchors);

        AnchorLayout::Anchors statusAnchors;
        statusAnchors.left = edge(primaryButton, AnchorLayout::Edge::Left);
        statusAnchors.right = edge(primaryButton, AnchorLayout::Edge::Right);
        statusAnchors.bottom = edge(m_controlPanel, AnchorLayout::Edge::Bottom, -24);
        panelLayout->addAnchoredWidget(m_status, statusAnchors);

        AnchorLayout::Anchors panelAnchors;
        panelAnchors.left = edge(m_colorPanel, AnchorLayout::Edge::Right, 24);
        panelAnchors.right = edge(this, AnchorLayout::Edge::Right, -32);
        panelAnchors.top = edge(m_colorPanel, AnchorLayout::Edge::Top);
        panelAnchors.bottom = edge(m_colorPanel, AnchorLayout::Edge::Bottom);
        m_rootLayout->addAnchoredWidget(m_controlPanel, panelAnchors);

        m_primaryButton = primaryButton;
        m_resetButton = resetButton;
    }

    void wireInteractions()
    {
        QObject::connect(m_darkSwitch, &ToggleSwitch::toggled, this, [this](bool dark) {
            FluentElement::setTheme(dark ? FluentElement::Dark : FluentElement::Light);
            m_status->setSeverity(InfoBar::Informational);
            updateCatalogPreview(QStringLiteral("FluentElement::setTheme"));
        });

        QObject::connect(m_styleCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                         this, [this](int index) { applyDesignLanguage(index); });

        QObject::connect(m_accentPicker, &ColorPicker::colorChanged, this, [this](const QColor& color) {
            applyAccent(color);
            m_accentSwatch->setColor(color);
            m_status->setSeverity(InfoBar::Success);
            updateCatalogPreview(QStringLiteral("StyleThemeCatalog::applyAccentOverride"));
        });

        QObject::connect(m_fontScaleSlider, &Slider::valueChanged,
                         this, [this](int value) {
                             applyFontScale(value);
                             m_status->setSeverity(InfoBar::Informational);
                             updateCatalogPreview(QStringLiteral("ThemeRegistry::setFontScale"));
                         });

        QObject::connect(m_resetButton, &Button::clicked, this, [this]() {
            m_styleCombo->setCurrentIndex(0);
            m_accentPicker->setColor(QColor(0, 120, 212));
            m_fontScaleSlider->setValue(100);
            m_darkSwitch->setIsOn(false);
            applyStyleTheme(0);
            m_status->setSeverity(InfoBar::Informational);
            updateCatalogPreview(QStringLiteral("Built-in Fluent tokens"));
        });

        QObject::connect(m_primaryButton, &Button::clicked, this, [this]() {
            m_status->setSeverity(InfoBar::Success);
            updateCatalogPreview(QStringLiteral("Accent button uses current token"));
        });
    }

    void applyDesignLanguage(int index)
    {
        applyStyleTheme(index);
        m_status->setSeverity(InfoBar::Informational);
        updateCatalogPreview(QStringLiteral("StyleThemeCatalog::apply"));
    }

    void applyStyleTheme(int index)
    {
        StyleTheme theme = StyleTheme::Fluent;
        switch (index) {
        case 1:
            theme = StyleTheme::Material;
            break;
        case 2:
            theme = StyleTheme::MacOS;
            break;
        default:
            break;
        }
        m_currentStyleTheme = theme;
        StyleThemeCatalog::apply(theme);
        applyAccent(m_accentPicker ? m_accentPicker->color() : QColor(0, 120, 212));
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

    QString currentStyleThemeSymbol() const
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

    QString currentDesignLanguageSymbol() const
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

    void updateCatalogPreview(const QString& title)
    {
        const QColor accent = m_accentPicker ? m_accentPicker->color() : QColor(0, 120, 212);
        const int fontPercent = qRound(ThemeRegistry::instance().fontScale() * 100.0);
        m_status->setTitle(title);
        m_status->setMessage(
            QStringLiteral("fluent::StyleThemeCatalog::apply(%1)\n"
                           "applyAccentOverride(%2)  %3  font %4%")
                .arg(currentStyleThemeSymbol(), accent.name(QColor::HexRgb).toUpper(),
                     currentDesignLanguageSymbol())
                .arg(fontPercent));
    }

    AnchorLayout* m_rootLayout = nullptr;
    QWidget* m_header = nullptr;
    SurfacePanel* m_colorPanel = nullptr;
    SurfacePanel* m_controlPanel = nullptr;
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

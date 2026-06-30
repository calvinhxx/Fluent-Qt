#include "view/widgets/AccentColorControl.h"

#include <functional>
#include <utility>

#include <QDesktopServices>
#include <QEnterEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/basicinput/ColorPicker.h"
#include "components/basicinput/HyperlinkButton.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "viewmodel/GallerySettings.h"
#include "viewmodel/ThemeCatalog.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::basicinput::ColorPicker;
using fluent::basicinput::HyperlinkButton;
using fluent::dialogs_flyouts::Flyout;
using fluent::dialogs_flyouts::Popup;
using fluent::textfields::Label;

// Curated accent presets, including the three shipping brand accents (Fluent #005FB8, Material #6750A4,
// macOS #007AFF) so they're one click away. zh_CN: 精选预设强调色,含三套出厂品牌强调色(Fluent #005FB8、
// Material #6750A4、macOS #007AFF),一键可达。
const QList<QColor>& presetSwatches()
{
    static const QList<QColor> swatches = {
        QColor(QStringLiteral("#005FB8")), QColor(QStringLiteral("#0078D4")),
        QColor(QStringLiteral("#6750A4")), QColor(QStringLiteral("#007AFF")),
        QColor(QStringLiteral("#038387")), QColor(QStringLiteral("#0099BC")),
        QColor(QStringLiteral("#107C10")), QColor(QStringLiteral("#498205")),
        QColor(QStringLiteral("#FFB900")), QColor(QStringLiteral("#FF8C00")),
        QColor(QStringLiteral("#F7630C")), QColor(QStringLiteral("#CA5010")),
        QColor(QStringLiteral("#D13438")), QColor(QStringLiteral("#E81123")),
        QColor(QStringLiteral("#EA005E")), QColor(QStringLiteral("#E3008C")),
    };
    return swatches;
}

bool sameRgb(const QColor& a, const QColor& b)
{
    return a.isValid() && b.isValid() && a.rgb() == b.rgb();
}

// A clickable preset color square with hover + selection ring. zh_CN: 可点击的预设色块,带悬停 + 选中环。
class SwatchButton final : public QWidget, public fluent::FluentElement {
public:
    SwatchButton(const QColor& color, std::function<void()> onClick, QWidget* parent)
        : QWidget(parent)
        , m_color(color)
        , m_onClick(std::move(onClick))
    {
        setFixedSize(30, 30);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setToolTip(color.name(QColor::HexRgb).toUpper());
    }

    void setSelected(bool selected)
    {
        if (m_selected == selected)
            return;
        m_selected = selected;
        update();
    }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto colors = themeColors();
        constexpr qreal kRad = 5.0;
        QRectF fill = QRectF(rect()).adjusted(2.5, 2.5, -2.5, -2.5);

        if (m_selected) {
            p.setPen(QPen(colors.textPrimary, 2.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0), kRad + 1.5, kRad + 1.5);
            fill = QRectF(rect()).adjusted(4.5, 4.5, -4.5, -4.5);
        } else if (m_hovered) {
            QColor ring = colors.strokeStrong.isValid() ? colors.strokeStrong : colors.textTertiary;
            p.setPen(QPen(ring, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0), kRad + 1.5, kRad + 1.5);
        }

        // Hairline so very light swatches stay visible on a light card. zh_CN: 细线,使极浅色块在浅色卡片上仍可见。
        p.setPen(QPen(QColor(0, 0, 0, 30), 1.0));
        p.setBrush(m_color);
        p.drawRoundedRect(fill, kRad, kRad);
    }

    void enterEvent(QEnterEvent*) override { m_hovered = true; update(); }
    void leaveEvent(QEvent*) override { m_hovered = false; update(); }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_onClick)
            m_onClick();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) && m_onClick)
            m_onClick();
        else
            QWidget::keyPressEvent(event);
    }

private:
    QColor m_color;
    std::function<void()> m_onClick;
    bool m_selected = false;
    bool m_hovered = false;
};

} // namespace

AccentColorControl::AccentColorControl(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("gallerySettingsAccentControl"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setMinimumSize(64, 32);
    setToolTip(QStringLiteral("Accent color"));

    auto* settings = &GallerySettings::instance();
    m_accent = settings->accentColor();
    connect(settings, &GallerySettings::accentColorChanged, this,
            [this](const QColor&) { refreshAccent(); });
    connect(settings, &GallerySettings::styleThemeChanged, this,
            [this](GallerySettings::StyleTheme) { refreshAccent(); });
}

QSize AccentColorControl::sizeHint() const
{
    return QSize(64, 32);
}

void AccentColorControl::onThemeUpdated()
{
    refreshAccent();
}

void AccentColorControl::refreshAccent()
{
    m_accent = GallerySettings::instance().accentColor();
    update();
}

void AccentColorControl::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto colors = themeColors();
    const qreal rad = themeRadius().control;
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    const QColor surface = colors.controlDefault.isValid() ? colors.controlDefault : colors.bgLayer;
    const QColor border = colors.strokeDefault.isValid() ? colors.strokeDefault : colors.strokeCard;
    p.setPen(QPen(border, 1.0));
    p.setBrush(surface);
    p.drawRoundedRect(box, rad, rad);

    if (m_hovered) {
        // Theme-aware hover veil (darken light, lighten dark). zh_CN: 主题感知悬停遮罩(浅色变暗、深色变亮)。
        p.setPen(Qt::NoPen);
        p.setBrush(effectiveTheme() == Dark ? QColor(255, 255, 255, 14) : QColor(0, 0, 0, 10));
        p.drawRoundedRect(box, rad, rad);
    }

    // Accent swatch on the left. zh_CN: 左侧强调色块。
    const QRectF swatch(8.0, (height() - 18) / 2.0, 24.0, 18.0);
    QPainterPath swatchPath;
    swatchPath.addRoundedRect(swatch, 4.0, 4.0);
    p.setPen(QPen(QColor(0, 0, 0, 30), 1.0));
    p.setBrush(m_accent.isValid() ? m_accent : colors.accentDefault);
    p.drawPath(swatchPath);

    // Trailing chevron hints the swatch opens a picker. zh_CN: 尾部箭头提示色块可展开选择器。
    QFont icon(Typography::FontFamily::SegoeFluentIcons);
    icon.setPixelSize(11);
    p.setFont(icon);
    p.setPen(colors.textSecondary);
    p.drawText(QRectF(width() - 22.0, 0.0, 18.0, height()), Qt::AlignCenter,
               Typography::Icons::ChevronDown);
}

void AccentColorControl::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        openFlyout();
}

void AccentColorControl::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Space)
        openFlyout();
    else
        QWidget::keyPressEvent(event);
}

void AccentColorControl::enterEvent(QEnterEvent*)
{
    m_hovered = true;
    update();
}

void AccentColorControl::leaveEvent(QEvent*)
{
    m_hovered = false;
    update();
}

void AccentColorControl::openFlyout()
{
    auto* settings = &GallerySettings::instance();
    auto* flyout = new Flyout(window());
    flyout->setPlacement(Flyout::Bottom);
    connect(flyout, &Popup::closed, flyout, &QObject::deleteLater);

    auto* layout = new QVBoxLayout(flyout);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto* title = new Label(QStringLiteral("Accent color"), flyout);
    title->setFluentTypography(Typography::FontRole::BodyStrong);
    layout->addWidget(title);

    auto* grid = new QGridLayout();
    grid->setSpacing(6);
    const auto& swatches = presetSwatches();
    const QColor current = settings->accentColor();
    constexpr int kCols = 4;
    for (int i = 0; i < swatches.size(); ++i) {
        const QColor c = swatches.at(i);
        auto* sw = new SwatchButton(c, [settings, c, flyout]() {
            settings->setAccentColor(c);
            flyout->close();
        }, flyout);
        sw->setSelected(sameRgb(c, current));
        grid->addWidget(sw, i / kCols, i % kCols);
    }
    layout->addLayout(grid);

    auto* actions = new QHBoxLayout();
    actions->setSpacing(8);
    auto* custom = new Button(QStringLiteral("Custom…"), flyout);
    custom->setFluentStyle(Button::Standard);
    custom->setFluentSize(Button::Small);
    connect(custom, &Button::clicked, this, [this, flyout]() {
        flyout->close();
        openCustomPicker(this);
    });
    auto* reset = new Button(QStringLiteral("Reset"), flyout);
    reset->setFluentStyle(Button::Subtle);
    reset->setFluentSize(Button::Small);
    connect(reset, &Button::clicked, flyout, [settings, flyout]() {
        settings->resetAccentColor();
        flyout->close();
    });
    actions->addWidget(custom);
    actions->addWidget(reset);
    actions->addStretch(1);
    layout->addLayout(actions);

    // Power-user escape hatch: jump to the folder of editable themes/<key>.json overrides.
    // zh_CN: 进阶用户的逃生口:直接打开可编辑的 themes/<key>.json 覆盖文件夹。
    auto* folder = new HyperlinkButton(QStringLiteral("Open themes folder"), flyout);
    folder->setFluentSize(Button::Small);
    connect(folder, &Button::clicked, flyout, [flyout]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(ThemeCatalog::themesDirectory()));
        flyout->close();
    });
    layout->addWidget(folder);

    flyout->showAt(this);
}

void AccentColorControl::openCustomPicker(QWidget* anchor)
{
    auto* settings = &GallerySettings::instance();
    auto* flyout = new Flyout(window());
    flyout->setPlacement(Flyout::Bottom);
    connect(flyout, &Popup::closed, flyout, &QObject::deleteLater);

    auto* layout = new QVBoxLayout(flyout);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    auto* picker = new ColorPicker(flyout);
    picker->setAlphaEnabled(false);  // Accent colors are opaque. zh_CN: 强调色不透明。
    picker->setColor(settings->accentColor());
    layout->addWidget(picker);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    auto* cancel = new Button(QStringLiteral("Cancel"), flyout);
    cancel->setFluentSize(Button::Small);
    connect(cancel, &Button::clicked, flyout, [flyout]() { flyout->close(); });
    auto* apply = new Button(QStringLiteral("Apply"), flyout);
    apply->setFluentStyle(Button::Accent);
    apply->setFluentSize(Button::Small);
    // Apply only on confirm so we don't re-theme the whole UI on every drag tick. zh_CN: 仅在确认时应用,
    // 避免每次拖拽都重绘整个界面。
    connect(apply, &Button::clicked, flyout, [settings, picker, flyout]() {
        settings->setAccentColor(picker->color());
        flyout->close();
    });
    row->addWidget(cancel);
    row->addWidget(apply);
    layout->addLayout(row);

    flyout->showAt(anchor);
}

} // namespace fluent::gallery

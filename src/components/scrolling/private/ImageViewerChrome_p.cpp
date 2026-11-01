#include "ImageViewerChrome_p.h"

#include <algorithm>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVariantAnimation>

#include "components/basicinput/Button.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/textfields/Label.h"
#include "design/IconCatalog.h"
#include "design/Typography.h"

namespace fluent::scrolling {

namespace {

constexpr int kShadowMargin = 6;
constexpr int kBarPadding = 4;
constexpr int kSeparatorWidth = 9;
constexpr int kBarSpacing = 2;
constexpr int kButtonSize = 32;
constexpr int kPercentMinWidth = 52;
constexpr int kIndicatorHeight = 44;
constexpr int kIndicatorMinWidth = 80;
constexpr int kIndicatorHoldMs = 900;

QString viewerText(const char* source)
{
    return QCoreApplication::translate("ImageViewer", source);
}

struct ChromeColors {
    QColor fill;
    QColor stroke;
    bool shadow = true;
};

ChromeColors chromeColors(const FluentElement::Colors& colors)
{
    // In-app acrylic approximation over an opaque surface token, as FlipView's buttons do.
    // zh_CN: 与 FlipView 按钮一致，在不透明表面 token 上近似 in-app acrylic。
    if (FluentElement::currentTheme() == FluentElement::HighContrast)
        return {colors.bgSolid, colors.strokeStrong, false};
    QColor fill = colors.bgLayer;
    fill.setAlpha(240);
    return {fill, colors.strokeSurface, true};
}

void paintChromeSurface(QPainter& painter, const QRectF& surface, qreal radius, const ChromeColors& colors)
{
    painter.setRenderHint(QPainter::Antialiasing);
    if (colors.shadow) {
        // Layered soft shadow standing in for the Flyout elevation.
        // zh_CN: 用多层柔和阴影近似 Flyout 层级。
        const bool dark = FluentElement::currentTheme() == FluentElement::Dark;
        for (int layer = kShadowMargin; layer >= 1; --layer) {
            QColor shadow(0, 0, 0, (dark ? 18 : 8) * (kShadowMargin - layer + 1) / kShadowMargin);
            painter.setPen(Qt::NoPen);
            painter.setBrush(shadow);
            const QRectF rect = surface.adjusted(-layer, -layer + 2, layer, layer + 2);
            painter.drawRoundedRect(rect, radius + layer, radius + layer);
        }
    }
    painter.setPen(QPen(colors.stroke, 1.0));
    painter.setBrush(colors.fill);
    painter.drawRoundedRect(surface.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}

class ZoomBarSeparator : public QWidget, public FluentElement {
public:
    explicit ZoomBarSeparator(QWidget* parent) : QWidget(parent)
    {
        setFixedSize(kSeparatorWidth, kButtonSize);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(QRectF(4.0, (height() - 16) / 2.0, 1.0, 16.0), themeColorsRef().strokeDivider);
    }
};

} // namespace

ImageViewerZoomBar::ImageViewerZoomBar(std::function<void(Command)> handler, QWidget* parent)
    : QWidget(parent), m_handler(std::move(handler))
{
    setObjectName(QString::fromLatin1(kImageViewerZoomBarName));
    setAccessibleName(viewerText("Zoom controls"));
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setCursor(Qt::ArrowCursor);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(kShadowMargin + kBarPadding, kShadowMargin + kBarPadding, kShadowMargin + kBarPadding,
        kShadowMargin + kBarPadding);
    layout->setSpacing(kBarSpacing);

    m_zoomOut = makeButton(QStringLiteral("ic_fluent_zoom_out_20_regular"), viewerText("Zoom out (-)"),
        viewerText("Zoom out"), Command::ZoomOut);
    m_percent = new textfields::Label(this);
    m_percent->setObjectName(QStringLiteral("fluentImageViewerZoomPercent"));
    m_percent->setAlignment(Qt::AlignCenter);
    m_percent->setMinimumWidth(kPercentMinWidth);
    m_percent->setFluentTypography(Typography::FontRole::Body);
    m_percent->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_zoomIn = makeButton(QStringLiteral("ic_fluent_zoom_in_20_regular"), viewerText("Zoom in (+)"),
        viewerText("Zoom in"), Command::ZoomIn);
    m_firstSeparator = new ZoomBarSeparator(this);
    m_fit = makeButton(QStringLiteral("ic_fluent_full_screen_maximize_20_regular"), viewerText("Fit to view"),
        viewerText("Fit to view"), Command::Fit);
    m_actualSize = makeButton(QStringLiteral("ic_fluent_ratio_one_to_one_20_regular"), viewerText("Actual size (1)"),
        viewerText("Actual size"), Command::ActualSize);
    m_secondSeparator = new ZoomBarSeparator(this);
    m_reset = makeButton(QStringLiteral("ic_fluent_arrow_reset_20_regular"), viewerText("Reset view (0)"),
        viewerText("Reset view"), Command::Reset);

    layout->addWidget(m_zoomOut);
    layout->addWidget(m_percent);
    layout->addWidget(m_zoomIn);
    layout->addWidget(m_firstSeparator);
    layout->addWidget(m_fit);
    layout->addWidget(m_actualSize);
    layout->addWidget(m_secondSeparator);
    layout->addWidget(m_reset);

    m_effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_effect);
    m_fade = new QVariantAnimation(this);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        applyOpacity(value.toReal());
    });
    // A zero-length transition may not report an unchanged end value.
    // zh_CN: 零时长过渡在终值未变化时可能不会发出 valueChanged。
    connect(m_fade, &QVariantAnimation::finished, this, [this]() { applyOpacity(m_fade->endValue().toReal()); });
    applyOpacity(0.0);
    hide();
}

basicinput::Button* ImageViewerZoomBar::makeButton(const QString& iconName,
    const QString& toolTip,
    const QString& accessibleName,
    Command command)
{
    auto* button = new basicinput::Button(this);
    button->setFluentStyle(basicinput::Button::Subtle);
    button->setFluentLayout(basicinput::Button::IconOnly);
    button->setIconGlyph(Typography::Icons::glyph(iconName), Typography::IconSize::Standard);
    button->setFixedSize(kButtonSize, kButtonSize);
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolTip(toolTip);
    button->setAccessibleName(accessibleName);
    connect(button, &QAbstractButton::clicked, this, [this, command]() {
        if (m_handler)
            m_handler(command);
    });
    return button;
}

void ImageViewerZoomBar::setState(const State& state)
{
    m_percent->setText(state.percent);
    m_zoomOut->setEnabled(state.canZoomOut);
    m_zoomIn->setEnabled(state.canZoomIn);
    m_fit->setEnabled(state.canFit);
    m_actualSize->setEnabled(state.canActualSize);
    m_reset->setEnabled(state.canReset);
}

void ImageViewerZoomBar::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    for (QWidget* widget :
        std::initializer_list<QWidget*>{m_firstSeparator, m_fit, m_actualSize, m_secondSeparator, m_reset})
        widget->setVisible(!compact);
    adjustSize();
}

void ImageViewerZoomBar::setShown(bool shown)
{
    if (m_shown == shown)
        return;
    m_shown = shown;
    if (shown)
        show();
    m_fade->stop();
    m_fade->setStartValue(m_opacity);
    m_fade->setEndValue(shown ? 1.0 : 0.0);
    ::fluent::detail::startMotionTransition(m_fade, themeAnimation().fast);
}

QSize ImageViewerZoomBar::surfaceSize() const
{
    const QSize full = sizeHint();
    return QSize(full.width() - 2 * kShadowMargin, full.height() - 2 * kShadowMargin);
}

int ImageViewerZoomBar::surfaceWidth(bool compact) const
{
    const int label = std::max(kPercentMinWidth, m_percent->sizeHint().width());
    if (compact)
        return 2 * kBarPadding + 2 * kButtonSize + label + 2 * kBarSpacing;
    return 2 * kBarPadding + 5 * kButtonSize + label + 2 * kSeparatorWidth + 7 * kBarSpacing;
}

int ImageViewerZoomBar::shadowMargin()
{
    return kShadowMargin;
}

void ImageViewerZoomBar::onThemeUpdated()
{
    update();
}

void ImageViewerZoomBar::applyOpacity(qreal opacity)
{
    // The effect renders off-screen only while fading.
    // zh_CN: 只有淡入淡出期间才通过离屏效果绘制。
    m_opacity = std::clamp(opacity, 0.0, 1.0);
    m_effect->setOpacity(m_opacity);
    m_effect->setEnabled(m_opacity < 1.0);
    if (!m_shown && m_opacity <= 0.0)
        hide();
}

void ImageViewerZoomBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const QRectF surface = QRectF(rect()).adjusted(kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);
    paintChromeSurface(painter, surface, themeRadius().overlay, chromeColors(themeColorsRef()));
}

ImageViewerZoomIndicator::ImageViewerZoomIndicator(QWidget* parent) : QWidget(parent)
{
    setObjectName(QString::fromLatin1(kImageViewerZoomIndicatorName));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);

    m_fade = new QVariantAnimation(this);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        applyOpacity(value.toReal());
    });
    // A zero-length transition may not report an unchanged end value.
    // zh_CN: 零时长过渡在终值未变化时可能不会发出 valueChanged。
    connect(m_fade, &QVariantAnimation::finished, this, [this]() { applyOpacity(m_fade->endValue().toReal()); });
    m_holdTimer = new QTimer(this);
    m_holdTimer->setSingleShot(true);
    m_holdTimer->setInterval(kIndicatorHoldMs);
    connect(m_holdTimer, &QTimer::timeout, this, [this]() { fadeTo(0.0); });
    hide();
}

void ImageViewerZoomIndicator::flash(const QString& text)
{
    m_text = text;
    resizeToText();
    if (QWidget* host = parentWidget())
        move((host->width() - width()) / 2, (host->height() - height()) / 2);
    show();
    raise();
    if (m_opacity < 1.0)
        fadeTo(1.0);
    else
        update();
    m_holdTimer->start();
}

void ImageViewerZoomIndicator::dismiss()
{
    m_holdTimer->stop();
    m_fade->stop();
    m_targetOpacity = 0.0;
    applyOpacity(0.0);
}

void ImageViewerZoomIndicator::onThemeUpdated()
{
    resizeToText();
    update();
}

void ImageViewerZoomIndicator::fadeTo(qreal opacity)
{
    m_targetOpacity = opacity;
    m_fade->stop();
    m_fade->setStartValue(m_opacity);
    m_fade->setEndValue(opacity);
    ::fluent::detail::startMotionTransition(m_fade, themeAnimation().fast);
}

void ImageViewerZoomIndicator::applyOpacity(qreal opacity)
{
    // Hide only once a fade-out ends; a fade-in may report its start value first.
    // zh_CN: 只在淡出结束时隐藏；淡入可能先报告起始值。
    m_opacity = std::clamp(opacity, 0.0, 1.0);
    if (m_opacity <= 0.0 && m_targetOpacity <= 0.0)
        hide();
    update();
}

void ImageViewerZoomIndicator::resizeToText()
{
    const QFont font = themeFont(Typography::FontRole::Subtitle).toQFont();
    const int textWidth = QFontMetrics(font).horizontalAdvance(m_text);
    resize(std::max(kIndicatorMinWidth, textWidth + 36) + 2 * kShadowMargin, kIndicatorHeight + 2 * kShadowMargin);
}

void ImageViewerZoomIndicator::paintEvent(QPaintEvent*)
{
    if (m_opacity <= 0.0)
        return;
    QPainter painter(this);
    painter.setOpacity(m_opacity);
    const QRectF surface = QRectF(rect()).adjusted(kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);
    paintChromeSurface(painter, surface, themeRadius().overlay, chromeColors(themeColorsRef()));
    painter.setFont(themeFont(Typography::FontRole::Subtitle).toQFont());
    painter.setPen(themeColorsRef().textPrimary);
    painter.drawText(surface, Qt::AlignCenter, m_text);
}

} // namespace fluent::scrolling

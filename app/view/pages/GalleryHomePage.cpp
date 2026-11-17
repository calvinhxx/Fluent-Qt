#include "GalleryHomePage.h"
#include "compatibility/PaintCompat.h"

#include <array>

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QEvent>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLinearGradient>
#include <QMetaEnum>
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStandardItemModel>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <QPainterPath>

#include "components/basicinput/Button.h"
#include "components/particles/ParticleBackdrop.h"
#include "components/collections/ListView.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/scrolling/ScrollBar.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "model/GalleryComponentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "support/logging/Log.h"
#include "view/shell/AppIcon.h"
#include "view/support/GalleryStyleSupport.h"
#include "view/widgets/GalleryEntryGrid.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {
namespace {

constexpr int kHeroHeight = 390;
constexpr int kHeroMarginX = 24;
constexpr int kHeroBottomFade = 184; // Height of the bottom fade dissolving the banner into the page.
constexpr int kBodyMarginX = 24;
constexpr int kBodyMarginTop = 20;
constexpr int kHeroIconSize = 56;
// CPU particle painting repaints the hero beneath children; cap it lower than GPU composition.
// zh_CN: CPU 粒子绘制会重绘子控件下方的 hero，帧率上限低于 GPU 合成模式。
constexpr int kHeroParticleCpuFrameRate = 30;
constexpr int kHeroParticleGpuFrameRate = 60;
constexpr int kHeroLinkCardWidth = 198;
constexpr int kHeroLinkCardHeight = 150;
constexpr int kHeroLinkCardSpacing = 16;
constexpr int kHeroLinkCardPadding = 18;
constexpr int kHeroLinkIconSize = 32;
constexpr int kHeroLinkStripTop = 200;
// Reserve vertical room around each link card so hover lift and drop shadow are never clipped.
// zh_CN: 卡片上下预留边距，避免悬停抬升与投影被视口裁切。
constexpr int kHeroLinkTopPad = 8;
constexpr int kHeroLinkShadowMargin = 24;
constexpr int kHeroLinkCellHeight = kHeroLinkTopPad + kHeroLinkCardHeight + kHeroLinkShadowMargin;
constexpr int kHeroLinkStripHeight = kHeroLinkCellHeight;
constexpr int kHeroLinkStripInsetX = kHeroMarginX - 8;
constexpr int kHeroScrollButtonWidth = 16;
constexpr int kHeroScrollButtonHeight = 38;
constexpr int kHeroScrollButtonMarginX = 8;
constexpr int kHeroScrollButtonLift = 8;
const QString kExternalLinkIconName = QStringLiteral("ic_fluent_open_16_regular");

enum HomeLinkRole { LinkTitleRole = Qt::UserRole + 1, LinkDescriptionRole, LinkUrlRole, LinkImageRole };

fluent::particles::ParticleBackdrop::Effect homeParticleEffect()
{
    using fluent::particles::ParticleBackdrop;
    // Keep the launch's choice when navigation or theme changes recreate the home page.
    static const auto effect = [] {
        auto& settings = GallerySettings::instance();
        const auto names = QMetaEnum::fromType<ParticleBackdrop::Effect>();
        const QString previous = settings.lastHomeParticleEffect();
        std::array<ParticleBackdrop::Effect, 3> candidates{};
        int count = 0;
        for (const auto candidate :
            {ParticleBackdrop::FlowingRibbons, ParticleBackdrop::FloatingDots, ParticleBackdrop::Starfield}) {
            if (previous != QString::fromLatin1(names.valueToKey(candidate)))
                candidates[count++] = candidate;
        }
        const auto chosen = candidates[QRandomGenerator::global()->bounded(count)];
        settings.setLastHomeParticleEffect(QString::fromLatin1(names.valueToKey(chosen)));
        return chosen;
    }();
    return effect;
}

QRect visibleAlphaBounds(const QImage& image)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) <= 8)
                continue;
            const QRect pixelRect(x, y, 1, 1);
            bounds = bounds.isNull() ? pixelRect : bounds.united(pixelRect);
        }
    }
    return bounds;
}

// Prescale large source icons once with SmoothTransformation at the target physical size
// to avoid bilinear aliasing and HiDPI blur in drawPixmap().
// zh_CN: 按目标物理尺寸预先高质量缩放图标并记录 DPR，避免 drawPixmap() 缩图锯齿或 HiDPI 发虚。
QPixmap homeLinkPixmap(const QString& resourcePath, const QSize& targetSize, qreal dpr, const QColor& tint)
{
    const bool isGitHub = resourcePath.endsWith(QStringLiteral("GitHub-Mark.png"));
    const QSize physical = (QSizeF(targetSize) * dpr).toSize();
    const QString key =
        QStringLiteral("%1@%2x%3#%4")
            .arg(resourcePath)
            .arg(physical.width())
            .arg(physical.height())
            .arg(isGitHub && tint.isValid() ? tint.name() : QString());
    static QHash<QString, QPixmap> cache;
    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();

    QImage image(resourcePath);
    if (!image.isNull()) {
        image = image.convertToFormat(QImage::Format_ARGB32);
        // GitHub-Mark.png ships as black-on-white; convert luminance to alpha and tint with text color.
        // zh_CN: GitHub 图标为白底黑图，由亮度生成 alpha 并按主题文字色着色，适配深浅背景。
        if (isGitHub) {
            const QColor c = tint.isValid() ? tint : QColor(0, 0, 0);
            for (int y = 0; y < image.height(); ++y) {
                auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    const QRgb pixel = line[x];
                    const int coverage = 255 - qGray(pixel); // ink darkness → opacity
                    line[x] = qRgba(c.red(), c.green(), c.blue(), coverage * qAlpha(pixel) / 255);
                }
            }
        }
        const QRect visible = visibleAlphaBounds(image);
        if (visible.isValid() && visible.size() != image.size())
            image = image.copy(visible);
        image = image.scaled(physical, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(dpr);
    cache.insert(key, pixmap);
    return pixmap;
}

void drawCenteredPixmap(QPainter& painter,
    const QRectF& rect,
    const QString& resourcePath,
    const QColor& tint = QColor())
{
    const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : 1.0;
    const QPixmap source = homeLinkPixmap(resourcePath, rect.size().toSize(), dpr, tint);
    if (source.isNull())
        return;

    const QSizeF logical = QSizeF(source.size()) / source.devicePixelRatioF();
    const QPointF topLeft(rect.center().x() - logical.width() / 2.0, rect.center().y() - logical.height() / 2.0);
    fluentDrawPixmapAt(painter, topLeft, source);
}

// Deterministic grayscale noise tile overlaid at low opacity for acrylic grain.
// zh_CN: 确定性灰度噪点贴片，低透明度叠加产生亚克力磨砂质感。
const QImage& acrylicNoiseTile()
{
    static const QImage tile = []() {
        constexpr int n = 96;
        QImage img(n, n, QImage::Format_ARGB32);
        QRandomGenerator rng(0xACE71C5Eu); // fixed seed → stable texture across repaints
        for (int y = 0; y < n; ++y) {
            auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < n; ++x) {
                const int v = rng.bounded(256);
                line[x] = qRgba(v, v, v, 255);
            }
        }
        return img;
    }();
    return tile;
}

void drawElidedWrappedText(QPainter& painter,
    const QRect& rect,
    const QString& text,
    const QFont& font,
    const QColor& color,
    int maxLines)
{
    if (rect.isEmpty() || text.isEmpty() || maxLines <= 0)
        return;

    QFontMetrics metrics(font);
    const int lineHeight = qMax(1, metrics.lineSpacing() - 1);
    const int availableLines = qMin(maxLines, qMax(1, rect.height() / lineHeight));
    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList lines;
    QString current;

    for (int i = 0; i < words.size(); ++i) {
        const QString candidate = current.isEmpty() ? words.at(i) : current + QLatin1Char(' ') + words.at(i);
        if (metrics.horizontalAdvance(candidate) <= rect.width() || current.isEmpty()) {
            current = candidate;
            continue;
        }

        lines.append(current);
        current = words.at(i);
        if (lines.size() == availableLines - 1) {
            QString tail = current;
            for (int j = i + 1; j < words.size(); ++j)
                tail += QLatin1Char(' ') + words.at(j);
            lines.append(metrics.elidedText(tail, Qt::ElideRight, rect.width()));
            current.clear();
            break;
        }
    }

    if (!current.isEmpty() && lines.size() < availableLines)
        lines.append(metrics.elidedText(current, Qt::ElideRight, rect.width()));

    painter.save();
    painter.setFont(font);
    painter.setPen(color);
    for (int i = 0; i < lines.size(); ++i) {
        const QRect lineRect(rect.left(), rect.top() + i * lineHeight, rect.width(), lineHeight);
        painter.drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter, lines.at(i));
    }
    painter.restore();
}

} // namespace

class GalleryHomeLinkDelegate : public QStyledItemDelegate, public fluent::FluentElement {
public:
    explicit GalleryHomeLinkDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return QSize(kHeroLinkCardWidth, kHeroLinkCellHeight);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!painter || !index.isValid())
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

        const Colors colors = themeColors();
        const bool dark = themeUsesDarkAppearance(currentTheme());
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const bool pressed = option.state.testFlag(QStyle::State_Sunken);

        const bool active = hovered || pressed;
        const qreal radius = ::CornerRadius::Overlay;

        // Inset inside the cell by kHeroLinkTopPad so hover lift and shadow stay within the clip.
        // zh_CN: 单元格内预留上下边距，避免悬停抬升与投影被裁剪。
        const qreal lift = active ? 2.0 : 0.0;
        QRectF cardRect(QPointF(option.rect.left(), option.rect.top() + kHeroLinkTopPad),
            QSizeF(kHeroLinkCardWidth, kHeroLinkCardHeight));
        cardRect.adjust(0.5, 0.5, -0.5, -0.5);
        cardRect.translate(0, -lift);
        QPainterPath cardPath;
        cardPath.addRoundedRect(cardRect, radius, radius);

        // Clip shadow outside the card so it never darkens the translucent surface.
        // zh_CN: 阴影裁剪在卡片外部，避免压暗半透明卡片内部。
        painter->save();
        QPainterPath shadowClip;
        shadowClip.addRect(QRectF(option.rect).adjusted(-40, -40, 40, 60));
        shadowClip = shadowClip.subtracted(cardPath);
        painter->setClipPath(shadowClip);
        const int shadowLayers = 14;
        const qreal reach = active ? 13.0 : 9.0;
        const qreal dyBias = active ? 5.0 : 3.0;
        const int peakAlpha = dark ? (active ? 13 : 9) : (active ? 6 : 4);
        for (int i = 0; i < shadowLayers; ++i) {
            const qreal f = (i + 1.0) / shadowLayers; // 0 → 1 outward
            const qreal grow = reach * f;
            QColor s(0, 0, 0, qRound(peakAlpha * (1.0 - f)));
            QPainterPath sp;
            sp.addRoundedRect(cardRect.adjusted(-grow, -grow * 0.5 + dyBias, grow, grow + dyBias), radius + grow,
                radius + grow);
            painter->fillPath(sp, s);
        }
        painter->restore();

        // Translucent card fill plus low-opacity noise tile; text draws afterwards.
        QColor surface = dark ? colors.bgLayerAlt : colors.bgLayer;
        surface.setAlpha(dark ? 112 : 132);
        painter->fillPath(cardPath, surface);

        painter->save();
        painter->setClipPath(cardPath);
        painter->setOpacity(dark ? 0.05 : 0.035);
        painter->fillRect(cardRect, QBrush(acrylicNoiseTile()));
        painter->restore();

        QColor border = active ? colors.accentDefault : (dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 13));
        if (active)
            border.setAlpha(dark ? 150 : 120);
        painter->setPen(QPen(border, 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(cardPath);

        const int contentLeft = qRound(cardRect.left()) + kHeroLinkCardPadding;
        const int contentWidth = qRound(cardRect.width()) - kHeroLinkCardPadding * 2;
        const QRectF iconRect(contentLeft, cardRect.top() + kHeroLinkCardPadding, kHeroLinkIconSize, kHeroLinkIconSize);
        // Only the monochrome GitHub mark uses the text tint; colored icons ignore it.
        drawCenteredPixmap(*painter, iconRect, index.data(LinkImageRole).toString(), colors.textPrimary);

        int textY = qRound(iconRect.bottom()) + 14;

        QFont titleFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
        QFont descFont = themeFont(Typography::FontRole::Caption).toQFont();
        QFontMetrics titleMetrics(titleFont);

        const QString title = index.data(LinkTitleRole).toString();
        painter->setFont(titleFont);
        painter->setPen(colors.textPrimary);
        painter->drawText(QRect(contentLeft, textY, contentWidth, titleMetrics.height()),
            Qt::AlignLeft | Qt::AlignVCenter, titleMetrics.elidedText(title, Qt::ElideRight, contentWidth));

        textY += titleMetrics.height() + 4;
        const QString description = index.data(LinkDescriptionRole).toString();

        const QFont glyphFont = Typography::Icons::font(Typography::IconSize::Standard);
        const QString externalLinkGlyph = Typography::Icons::glyphForSize(
            Typography::Icons::glyph(kExternalLinkIconName), Typography::IconSize::Standard);
        const QRect externalRect(qRound(cardRect.right()) - kHeroLinkCardPadding - 16,
            qRound(cardRect.top()) + kHeroLinkCardPadding, 16, 16);
        drawElidedWrappedText(*painter,
            QRect(contentLeft, textY, contentWidth, qRound(cardRect.bottom()) - kHeroLinkCardPadding - textY),
            description, descFont, colors.textSecondary, 3);

        painter->setFont(glyphFont);
        painter->setPen(active ? colors.accentDefault : colors.textSecondary);
        painter->drawText(externalRect, Qt::AlignCenter, externalLinkGlyph);

        painter->restore();
    }
};

class GalleryHomeLinkStrip : public fluent::collections::ListView {
public:
    explicit GalleryHomeLinkStrip(QWidget* parent = nullptr) : fluent::collections::ListView(parent)
    {
        setObjectName(QStringLiteral("galleryHomeHeroLinksView"));
        setProperty("externalLinkIconName", kExternalLinkIconName);
        setAccessibleName(QStringLiteral("Home links"));
        setViewMode(QListView::IconMode);
        setFlow(QListView::LeftToRight);
        setWrapping(false);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setUniformItemSizes(true);
        setGridSize(QSize(kHeroLinkCardWidth + kHeroLinkCardSpacing, kHeroLinkCellHeight));
        setSpacing(0);
        setFixedHeight(kHeroLinkStripHeight);
        setCursor(Qt::PointingHandCursor);
        setSelectionMode(SelectionMode::None);
        setBackgroundVisible(false);
        setBorderVisible(false);
        // Preserve the hero gradient underneath this transparent viewport instead of clearing with CompositionMode_Source.
        // zh_CN: 保留下方 hero 渐变，避免 Mica 窗口下 ListView 用 Source 模式清除背景导致镂空。
        setProperty("fluentPreserveParentSurface", true);
        viewport()->setProperty("fluentPreserveParentSurface", true);
        setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setAttribute(Qt::WA_TranslucentBackground);
        viewport()->setAttribute(Qt::WA_TranslucentBackground);
        viewport()->setCursor(Qt::PointingHandCursor);
        setStyleSheet(QStringLiteral(
            "QListView#galleryHomeHeroLinksView { "
            "background: transparent; border: none; }"
            "QListView#galleryHomeHeroLinksView::item { "
            "background: transparent; }"));

        auto* delegate = new GalleryHomeLinkDelegate(this);
        setItemDelegate(delegate);

        auto* model = new QStandardItemModel(this);
        auto append =
            [model](const QString& title, const QString& description, const QString& url, const QString& imagePath) {
                auto* item = new QStandardItem(title);
                item->setEditable(false);
                item->setData(title, LinkTitleRole);
                item->setData(description, LinkDescriptionRole);
                item->setData(QUrl(url), LinkUrlRole);
                item->setData(imagePath, LinkImageRole);
                model->appendRow(item);
            };
        append(QStringLiteral("Design"), QStringLiteral("Guidelines and toolkits for Fluent design."),
            QStringLiteral("https://aka.ms/WinUI/3.0-figma-toolkit"),
            QStringLiteral(":/app/assets/home_header_tiles/Header-WindowsDesign.png"));
        append(QStringLiteral("WinUI Gallery"), QStringLiteral("WinUI Gallery source on GitHub."),
            QStringLiteral("https://github.com/microsoft/WinUI-Gallery"),
            QStringLiteral(":/app/assets/home_header_tiles/GitHub-Mark.png"));
        append(QStringLiteral("Fluent UI"), QStringLiteral("Fluent controls and patterns for the web."),
            QStringLiteral("https://developer.microsoft.com/en-us/fluentui#/controls/web"),
            QStringLiteral(":/app/assets/home_header_tiles/Header-Toolkit.png"));
        append(QStringLiteral("FluentQt"), QStringLiteral("FluentQt UI component library source on GitHub."),
            QStringLiteral("https://github.com/calvinhxx/Fluent-Qt"), QStringLiteral(":/app/assets/app-icon.png"));
        append(QStringLiteral("Qt Quick Controls"), QStringLiteral("Qt Quick Controls reference on doc.qt.io."),
            QStringLiteral("https://doc.qt.io/qt-6/qtquickcontrols-index.html"),
            QStringLiteral(":/app/assets/home_header_tiles/Qt-Logo.png"));
        setModel(model);

        connect(horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this] {
            hideScrollChrome();
            updateScrollButtonsVisibility();
        });
        connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] {
            hideScrollChrome();
            updateScrollButtonsVisibility();
        });

        m_backButton = createScrollButton(Typography::Icons::FlipViewPrevH, QStringLiteral("Scroll left"));
        m_forwardButton = createScrollButton(Typography::Icons::FlipViewNextH, QStringLiteral("Scroll right"));
        connect(m_backButton, &QPushButton::clicked, this, [this] { scrollByViewport(-1); });
        connect(m_forwardButton, &QPushButton::clicked, this, [this] { scrollByViewport(1); });

        hideScrollChrome();
        updateScrollButtonsGeometry();
        updateScrollButtonsVisibility();

        connect(this, &fluent::collections::ListView::itemClicked, this, [model](int row) {
            const QModelIndex index = model->index(row, 0);
            const QUrl url = index.data(LinkUrlRole).toUrl();
            if (url.isValid())
                QDesktopServices::openUrl(url);
        });
    }

    void onThemeUpdated() override
    {
        fluent::collections::ListView::onThemeUpdated();
        if (viewport())
            viewport()->update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        fluent::collections::ListView::resizeEvent(event);
        hideScrollChrome();
        updateScrollButtonsGeometry();
        updateScrollButtonsVisibility();
    }

    void showEvent(QShowEvent* event) override
    {
        fluent::collections::ListView::showEvent(event);
        hideScrollChrome();
        updateScrollButtonsGeometry();
        updateScrollButtonsVisibility();
    }

private:
    fluent::basicinput::Button* createScrollButton(const QString& glyph, const QString& tooltip)
    {
        auto* button = new fluent::basicinput::Button(this);
        button->setObjectName(QStringLiteral("galleryHomeHeroScrollButton"));
        button->setFluentStyle(fluent::basicinput::Button::Standard);
        button->setFluentSize(fluent::basicinput::Button::Small);
        button->setFluentLayout(fluent::basicinput::Button::IconOnly);
        button->setIconGlyph(glyph, Typography::IconSize::Standard);
        button->setContentOpacity(0.62);
        button->setFixedSize(kHeroScrollButtonWidth, kHeroScrollButtonHeight);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        fluent::status_info::ToolTip::attach(button, tooltip);
        button->hide();
        return button;
    }

    void scrollByViewport(int direction)
    {
        QScrollBar* bar = horizontalScrollBar();
        const int delta = qMax(1, viewport()->width() - kHeroLinkCardWidth);
        const int next = qBound(bar->minimum(), bar->value() + direction * delta, bar->maximum());
        bar->setValue(next);
        updateScrollButtonsVisibility();
    }

    void updateScrollButtonsGeometry()
    {
        const int y = qMax(0, (height() - kHeroScrollButtonHeight) / 2 - kHeroScrollButtonLift);
        if (m_backButton) {
            m_backButton->setGeometry(kHeroScrollButtonMarginX, y, kHeroScrollButtonWidth, kHeroScrollButtonHeight);
            m_backButton->raise();
        }
        if (m_forwardButton) {
            m_forwardButton->setGeometry(width() - kHeroScrollButtonMarginX - kHeroScrollButtonWidth, y,
                kHeroScrollButtonWidth, kHeroScrollButtonHeight);
            m_forwardButton->raise();
        }
    }

    void updateScrollButtonsVisibility()
    {
        const QScrollBar* bar = horizontalScrollBar();
        const bool canScroll = bar && bar->maximum() > bar->minimum();
        if (m_backButton)
            m_backButton->setVisible(canScroll && bar->value() > bar->minimum());
        if (m_forwardButton)
            m_forwardButton->setVisible(canScroll && bar->value() < bar->maximum());
    }

    void hideScrollChrome()
    {
        if (auto* bar = horizontalFluentScrollBar()) {
            bar->setAttribute(Qt::WA_DontShowOnScreen, true);
            bar->setThickness(0);
            bar->hide();
        }
        if (auto* bar = verticalFluentScrollBar()) {
            bar->setAttribute(Qt::WA_DontShowOnScreen, true);
            bar->setThickness(0);
            bar->hide();
        }
    }

    fluent::basicinput::Button* m_backButton = nullptr;
    fluent::basicinput::Button* m_forwardButton = nullptr;
};

/**
 * @brief Full-bleed gradient banner with app icon, title, and tagline.
 * zh_CN: 通栏渐变横幅，含应用图标、标题与标语。
 *
 * Mirrors the WinUI Gallery hero: a soft diagonal wash with translucent
 * decorative circles, recolored per theme.
 * zh_CN: 仿 WinUI Gallery hero：柔和的对角渐变叠加半透明装饰圆，随主题换色。
 */
class GalleryHomeHeroBanner : public QWidget, public fluent::FluentElement {
public:
    explicit GalleryHomeHeroBanner(const QString& title, const QString& tagline, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("galleryHomeHero"));
        setFixedHeight(kHeroHeight);
        // Transparent backing lets the bottom fade dissolve directly into the window backdrop material.
        // zh_CN: 透明背景使横幅底部渐隐可直接透出窗口材质，避免接缝。
        setAttribute(Qt::WA_TranslucentBackground);

        m_particles = new fluent::particles::ParticleBackdrop(this);
        m_particles->setObjectName(QStringLiteral("galleryHomeParticles"));
        m_particles->setParticleCount(240);
        m_particles->setSpeed(.6);
        m_particles->setMaximumFrameRate(kHeroParticleCpuFrameRate);
        // The 3D compositor requests acceleration only while it renders this backdrop.
        // zh_CN: 仅当 3D 合成器实际渲染此背景时才请求加速。
        connect(m_particles, &fluent::particles::ParticleBackdrop::gpuAccelerationEnabledChanged, this,
            [this](bool accelerated) {
                m_particles->setMaximumFrameRate(accelerated ? kHeroParticleGpuFrameRate : kHeroParticleCpuFrameRate);
            });
        m_particles->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_particles->lower();
        auto& settings = GallerySettings::instance();
        connect(&settings, &GallerySettings::homeParticlesEnabledChanged, this,
            &GalleryHomeHeroBanner::setParticlesEnabled);
        setParticlesEnabled(settings.homeParticlesEnabled());

        auto* layout = new QVBoxLayout(this);
        // The floating link ListView occupies the lower half of the hero, so keep the
        // text block in the upper band and let the card strip sit on the artwork.
        // zh_CN: 悬浮链接 ListView 占用 hero 下半区，因此文字块放在上方，卡片条压在横幅图上。
        layout->setContentsMargins(kHeroMarginX, 24, kHeroMarginX, kHeroHeight - kHeroLinkStripTop + 12);
        layout->setSpacing(0);

        m_iconLabel = new QLabel(this);
        m_iconLabel->setObjectName(QStringLiteral("galleryHomeHeroIcon"));
        m_iconLabel->setFixedSize(kHeroIconSize, kHeroIconSize);
        m_iconLabel->setPixmap(appicon::pixmap(kHeroIconSize, devicePixelRatioF()));
        m_iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
        layout->addWidget(m_iconLabel);
        layout->addSpacing(12);

        m_titleLabel = new fluent::textfields::Label(title, this);
        m_titleLabel->setObjectName(QStringLiteral("galleryHomeHeroTitle"));
        m_titleLabel->setFluentTypography(Typography::FontRole::TitleLarge);
        m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(m_titleLabel);
        layout->addSpacing(4);

        m_taglineLabel = new fluent::textfields::Label(tagline, this);
        m_taglineLabel->setObjectName(QStringLiteral("galleryHomeHeroTagline"));
        m_taglineLabel->setFluentTypography(Typography::FontRole::Body);
        m_taglineLabel->setWordWrap(true);
        layout->addWidget(m_taglineLabel);
        layout->addStretch(1);

        m_linkStrip = new GalleryHomeLinkStrip(this);
        m_linkStrip->raise();

        applyTextPalette();
    }

    void setParticlesEnabled(bool enabled)
    {
        // A disabled launch must not consume the next random effect.
        if (enabled)
            m_particles->setEffect(homeParticleEffect());
        m_particles->setAnimationEnabled(enabled);
        m_particles->setVisible(enabled);
    }

    void onThemeUpdated() override
    {
        m_artwork = {};
        applyTextPalette();
        if (m_titleLabel)
            m_titleLabel->onThemeUpdated();
        if (m_taglineLabel)
            m_taglineLabel->onThemeUpdated();
        if (m_linkStrip)
            m_linkStrip->onThemeUpdated();
        update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        m_particles->setGeometry(rect());
        m_particles->setFadeMargins(QMarginsF(width() * .45, 0, 0, kHeroBottomFade));
        if (!m_linkStrip)
            return;
        const int stripX = qMax(0, kHeroLinkStripInsetX);
        const int stripWidth = qMax(0, width() - stripX - kHeroMarginX);
        m_linkStrip->setGeometry(stripX, kHeroLinkStripTop, stripWidth, kHeroLinkStripHeight);
    }

    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::WindowActivate || e->type() == QEvent::WindowDeactivate)
            update(); // bottom dissolve tracks the window backdrop
        return QWidget::event(e);
    }

    void paintEvent(QPaintEvent*) override
    {
        // Render into an offscreen image first: DestinationIn directly on a child QPainter
        // clears parent pixels in the shared backing store, punching holes on Linux PaintedOpaque Mica.
        // zh_CN: 先在离屏图像中绘制；若直接在子控件 painter 上用 DestinationIn 会清除父级像素，
        // 在 Linux PaintedOpaque Mica 后端上会将窗口打穿。
        const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
        const QSize pixels(qMax(1, qRound(width() * dpr)), qMax(1, qRound(height() * dpr)));
        const bool material = usesWindowMaterialBackdrop(this);
        const bool composited = usesCompositedWindowBackdrop(this);
        if (m_artwork.size() == pixels && m_artwork.devicePixelRatio() == dpr && m_cachedMaterial == material
            && m_cachedComposited == composited) {
            QPainter target(this);
            target.setCompositionMode(
                composited ? QPainter::CompositionMode_Source : QPainter::CompositionMode_SourceOver);
            fluentDrawImageAt(target, QPointF(), m_artwork);
            return;
        }
        QImage artwork(qMax(1, qRound(width() * dpr)), qMax(1, qRound(height() * dpr)),
            QImage::Format_ARGB32_Premultiplied);
        artwork.setDevicePixelRatio(dpr);
        artwork.fill(Qt::transparent);

        FluentRasterCachePainter painter(&artwork);
        // QPainter already scales logical coordinates by QImage::devicePixelRatio(); extra scaling doubles Retina coordinates.
        // zh_CN: QPainter 已按 QImage 的设备像素比映射逻辑坐标，再次缩放会导致 Retina 坐标翻倍。
        painter.setRenderHint(QPainter::Antialiasing);

        const bool dark = themeUsesDarkAppearance(currentTheme());
        const Colors colors = themeColors();
        const bool materialBackdrop = usesWindowMaterialBackdrop(this);

        // Round only the top-left corner where the banner meets the nav pane; the right edge runs flush to the window.
        // zh_CN: 仅对与导航栏相接的左上角做圆角，右侧直抵窗口边缘保持直角。
        const QRectF banner(0, 0, width(), height());
        const qreal radius = themeRadius().overlay;
        QPainterPath clip = fluent::overlay::roundedCornerRectPath(banner, radius, /*TL*/ true, /*TR*/ false,
            /*BR*/ false, /*BL*/ false);
        painter.save();
        painter.setClipPath(clip);

        // Multi-stop gradient avoids visible banding across the blue-to-purple wash.
        QLinearGradient wash(banner.topLeft(), banner.bottomRight());
        if (dark) {
            wash.setColorAt(0.00, QColor(0x16, 0x22, 0x3A));
            wash.setColorAt(0.22, QColor(0x1A, 0x24, 0x40));
            wash.setColorAt(0.40, QColor(0x1F, 0x24, 0x44));
            wash.setColorAt(0.56, QColor(0x24, 0x24, 0x47));
            wash.setColorAt(0.70, QColor(0x29, 0x23, 0x4A));
            wash.setColorAt(0.84, QColor(0x2F, 0x23, 0x48));
            wash.setColorAt(1.00, QColor(0x35, 0x22, 0x40));
        } else {
            wash.setColorAt(0.00, QColor(0xD9, 0xE9, 0xF8));
            wash.setColorAt(0.24, QColor(0xDE, 0xE5, 0xF6));
            wash.setColorAt(0.46, QColor(0xE3, 0xE1, 0xF4));
            wash.setColorAt(0.66, QColor(0xE9, 0xE2, 0xF0));
            wash.setColorAt(0.84, QColor(0xEF, 0xE5, 0xEE));
            wash.setColorAt(1.00, QColor(0xF4, 0xE8, 0xEA));
        }
        painter.fillRect(banner, wash);

        // Soft radial blooms with transparent outer stops so edges blend smoothly into the wash.
        const qreal w = banner.width();
        const qreal h = banner.height();
        const qreal bx = banner.left();
        const qreal by = banner.top();
        const auto bloom = [&](QPointF center, qreal radius, QColor tint) {
            QRadialGradient g(center, radius);
            g.setColorAt(0.0, tint);
            QColor mid = tint;
            mid.setAlpha(tint.alpha() / 3);
            g.setColorAt(0.5, mid);
            QColor edge = tint;
            edge.setAlpha(0);
            g.setColorAt(1.0, edge);
            painter.fillRect(banner, g);
        };
        bloom({bx + w * 0.86, by - h * 0.30}, h * 1.5, dark ? QColor(86, 150, 232, 82) : QColor(255, 255, 255, 210));
        bloom({bx + w * 0.94, by + h * 1.10}, h * 1.3, dark ? QColor(150, 96, 206, 76) : QColor(214, 180, 236, 128));
        bloom({bx + w * 0.62, by + h * 1.15}, h * 1.0, dark ? QColor(196, 116, 168, 46) : QColor(247, 210, 214, 102));

        // Overlay noise dithers 8-bit gradient banding across the blue-purple range.
        painter.save();
        painter.setOpacity(dark ? 0.05 : 0.045);
        painter.setCompositionMode(QPainter::CompositionMode_Overlay);
        painter.fillRect(banner, QBrush(acrylicNoiseTile()));
        painter.restore();

        const QRectF fadeRect(banner.left(), banner.bottom() - kHeroBottomFade, banner.width(), kHeroBottomFade);

        if (materialBackdrop) {
            // Under Mica/Acrylic, fade the banner alpha with DestinationIn to reveal the underlying material without a seam.
            // zh_CN: Mica/Acrylic 下用 DestinationIn 淡出横幅透明通道，平滑露出下方窗口材质。
            const qreal veil = dark ? 0.90 : 0.92; // upper-band opacity → backdrop shows ~8–10% through the art
            const qreal fadeTop = qBound(0.0, 1.0 - qreal(kHeroBottomFade) / qMax(1.0, banner.height()), 1.0);
            const auto a = [](qreal o) { return QColor(0, 0, 0, qBound(0, qRound(o * 255.0), 255)); };
            QLinearGradient mask(banner.topLeft(), banner.bottomLeft());
            mask.setColorAt(0.0, a(veil));
            mask.setColorAt(fadeTop, a(veil));
            mask.setColorAt(fadeTop + (1.0 - fadeTop) * 0.30, a(veil * 0.74));
            mask.setColorAt(fadeTop + (1.0 - fadeTop) * 0.55, a(veil * 0.46));
            mask.setColorAt(fadeTop + (1.0 - fadeTop) * 0.78, a(veil * 0.18));
            mask.setColorAt(1.0, a(0.0));
            painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            painter.fillRect(banner, mask);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        } else {
            // In Normal mode, fade onto the opaque bgLayerAlt surface behind the page.
            // zh_CN: Normal 模式下渐隐到页面背后的 bgLayerAlt 不透明表面。
            QColor base = colors.bgLayerAlt;
            QColor clear = base;
            clear.setAlpha(0);
            QColor soft = base;
            soft.setAlpha(dark ? 34 : 42);
            QColor lower = base;
            lower.setAlpha(dark ? 74 : 92);
            QColor medium = base;
            medium.setAlpha(dark ? 122 : 148);
            QLinearGradient fade(fadeRect.topLeft(), fadeRect.bottomLeft());
            fade.setColorAt(0.00, clear);
            fade.setColorAt(0.30, soft);
            fade.setColorAt(0.55, lower);
            fade.setColorAt(0.78, medium);
            fade.setColorAt(1.00, base);
            painter.fillRect(fadeRect, fade);
        }

        painter.restore();
        painter.end();

        m_artwork = artwork;
        m_cachedMaterial = material;
        m_cachedComposited = composited;
        QPainter target(this);
        // Native/compositor material needs the alpha layer to replace this
        // region; app-painted material must remain underneath it.
        // zh_CN: 原生/合成器材质由 alpha 图层替换当前区域；应用侧绘制的材质
        // 则保留在图层下方，通过 SourceOver 显露出来。
        target.setCompositionMode(usesCompositedWindowBackdrop(this)
                ? QPainter::CompositionMode_Source
                : QPainter::CompositionMode_SourceOver);
        fluentDrawImageAt(target, QPointF(), artwork);
    }

private:
    void applyTextPalette()
    {
        const Colors colors = themeColors();
        if (m_titleLabel) {
            m_titleLabel->setStyleSheet(
                QStringLiteral("color: %1; background: transparent;").arg(cssColor(colors.textPrimary)));
        }
        if (m_taglineLabel) {
            m_taglineLabel->setStyleSheet(
                QStringLiteral("color: %1; background: transparent;").arg(cssColor(colors.textSecondary)));
        }
    }

    fluent::particles::ParticleBackdrop* m_particles = nullptr;
    QImage m_artwork;
    bool m_cachedMaterial = false, m_cachedComposited = false;
    QLabel* m_iconLabel = nullptr;
    fluent::textfields::Label* m_titleLabel = nullptr;
    fluent::textfields::Label* m_taglineLabel = nullptr;
    GalleryHomeLinkStrip* m_linkStrip = nullptr;
};

GalleryHomePage::GalleryHomePage(const GalleryContentEntry& entry,
    const GalleryNavigationViewModel& navigationViewModel,
    QWidget* parent)
    : GalleryContentPage(entry.routeId, entry.title, QString(), parent)
{
    setObjectName(QStringLiteral("galleryHomePage"));

    // The hero replaces the stock heading and spans the full page width.
    setPageHeaderVisible(false);
    setViewportMargins(QMargins(0, 0, 0, 0));
    setContentSpacing(0);

    m_heroBanner = new GalleryHomeHeroBanner(QStringLiteral("Fluent-Qt Gallery"), entry.description, this);
    addContentWidget(m_heroBanner);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("galleryHomeBody"));
    // Fix vertical policy to content height; Preferred can distribute spare scroll height into QLabel rows on some platforms.
    // zh_CN: 垂直策略设为 Fixed 以贴合内容高度，避免部分平台下将富余高度分摊给 QLabel 行造成上方留白。
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(kBodyMarginX, kBodyMarginTop, kBodyMarginX, 48);
    bodyLayout->setSpacing(16);
    bodyLayout->setAlignment(Qt::AlignTop);

    auto addEntryGrid = [this, body, bodyLayout](const QString& objectName) -> GalleryEntryGrid* {
        auto* grid = new GalleryEntryGrid(body);
        grid->setObjectName(objectName);
        connect(grid, &GalleryEntryGrid::activated, this, &GalleryContentPage::routeActivated);
        bodyLayout->addWidget(grid);
        return grid;
    };

    fluent::textfields::Label* featuredHeader = createTrackedLabel(QStringLiteral("Featured components"),
        Typography::FontRole::Subtitle, TextRole::Primary);
    featuredHeader->setObjectName(QStringLiteral("galleryHomeFeaturedHeader"));
    featuredHeader->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bodyLayout->addWidget(featuredHeader);

    QVector<GalleryEntryGrid::Entry> featuredEntries;
    for (const QString& routeId : entry.relatedRouteIds) {
        const GalleryNavigationItem* item = navigationViewModel.itemById(routeId);
        if (!item)
            continue;
        QString description;
        if (const GalleryContentEntry* componentEntry = galleryContentEntry(routeId))
            description = componentEntry->description;
        const QString imageResource = galleryControlImageResource(item->title);
        featuredEntries.append(
            {item->id, item->title, description, imageResource, imageResource.isEmpty() ? item->iconGlyph : QString()});
    }
    addEntryGrid(QStringLiteral("galleryHomeCards"))->setEntries(featuredEntries);

    bodyLayout->addSpacing(12);
    fluent::textfields::Label* categoriesHeader = createTrackedLabel(QStringLiteral("Browse by category"),
        Typography::FontRole::Subtitle, TextRole::Primary);
    categoriesHeader->setObjectName(QStringLiteral("galleryHomeCategoriesHeader"));
    categoriesHeader->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bodyLayout->addWidget(categoriesHeader);

    QVector<GalleryEntryGrid::Entry> categoryEntries;
    auto appendCategory = [&](const GalleryNavigationItem& item) {
        QString description;
        if (const GalleryContentEntry* categoryEntry = galleryContentEntry(item.id))
            description = categoryEntry->description;
        categoryEntries.append({item.id, item.title, description, QString(), item.iconGlyph});
    };
    if (const GalleryNavigationItem* allControls = navigationViewModel.itemById(QStringLiteral("all-controls")))
        appendCategory(*allControls);
    for (const GalleryNavigationItem& item : navigationViewModel.items()) {
        if (item.kind == GalleryNavigationItem::Kind::CategoryRoute && item.depth == 0)
            appendCategory(item);
    }
    addEntryGrid(QStringLiteral("galleryHomeCategoryCards"))->setEntries(categoryEntries);

    addContentWidget(body);

    LOG_DEBUG(QStringLiteral(
        "GalleryHomePage created routeId=%1 "
        "featuredCards=%2 categoryCards=%3")
            .arg(entry.routeId)
            .arg(featuredEntries.size())
            .arg(categoryEntries.size()));
}

void GalleryHomePage::onThemeUpdated()
{
    GalleryContentPage::onThemeUpdated();
    if (m_heroBanner)
        m_heroBanner->onThemeUpdated();
}

} // namespace fluent::gallery

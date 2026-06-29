#include "GalleryHomePage.h"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QEvent>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <QPainterPath>

#include "components/basicinput/Button.h"
#include "components/collections/ListView.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/scrolling/ScrollBar.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "model/GalleryComponentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/shell/AppIcon.h"
#include "view/support/GalleryStyleSupport.h"
#include "view/widgets/GalleryEntryGrid.h"
#include "utils/Log.h"

namespace fluent::gallery {
namespace {

constexpr int kHeroHeight = 370;
constexpr int kHeroMarginX = 24;     // Text inset (was 48) — content shifts left overall.
constexpr int kHeroBottomFade = 184; // Bottom band that dissolves the banner into the page (lengthened so the seam under the card strip fully dissolves). zh_CN: 加长底部溶解带，使卡片条下方的横向硬缝彻底化开。
constexpr int kBodyMarginX = 24;     // Body content inset (was 48) — matches the hero.
constexpr int kBodyMarginTop = 20;
constexpr int kHeroIconSize = 56;
constexpr int kHeroLinkCardWidth = 198;
constexpr int kHeroLinkCardHeight = 150;
constexpr int kHeroLinkCardSpacing = 16;
constexpr int kHeroLinkCardPadding = 18;
constexpr int kHeroLinkIconSize = 32;
constexpr int kHeroLinkStripTop = 184;
constexpr int kHeroLinkTopPad = 8;        // Room above each card so the hover-lifted top edge + accent border aren't clipped. zh_CN: 卡片上方预留高度，使 hover 抬起的顶边与强调色描边不被裁切。
constexpr int kHeroLinkShadowMargin = 24; // Room below each card for the soft drop shadow + hover lift. zh_CN: 卡片下方预留高度，容纳柔和投影与 hover 抬升。
// The delegate draws the card inset by kHeroLinkTopPad inside a cell this tall, leaving room on
// every side, so the lifted card (and its full border) never touches a clip edge — top OR bottom.
// zh_CN: delegate 在这么高的单元格内、以 kHeroLinkTopPad 内缩绘制卡片，四周都有余量，
// 使抬起的卡片（及其完整描边）永远不贴裁剪边——上边或下边都不会。
constexpr int kHeroLinkCellHeight = kHeroLinkTopPad + kHeroLinkCardHeight + kHeroLinkShadowMargin;
constexpr int kHeroLinkStripHeight = kHeroLinkCellHeight;
constexpr int kHeroLinkStripInsetX = kHeroMarginX - 8;
constexpr int kHeroScrollButtonWidth = 16;
constexpr int kHeroScrollButtonHeight = 38;
constexpr int kHeroScrollButtonMarginX = 8;
constexpr int kHeroScrollButtonLift = 8;
const QString kExternalLinkGlyph = QString::fromUtf16(u"\uE8A7");

enum HomeLinkRole {
    LinkTitleRole = Qt::UserRole + 1,
    LinkDescriptionRole,
    LinkUrlRole,
    LinkImageRole
};

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

// The source art is large (256–1025px) but each icon draws into a small hero box. Downscaling that far in
// a single drawPixmap() (bilinear) aliases into a soft, distorted blob, and ignoring the device pixel
// ratio leaves it soft again on HiDPI. So scale ONCE, high-quality (SmoothTransformation), straight to
// the icon's physical pixel size, tag the result with the dpr, and cache per (path, size, dpr).
// zh_CN: 源图很大(256–1025px),但每个图标只画进较小的 hero 方框。一次性 drawPixmap 缩这么多(双线性)会糊成
// 失真的团块,且忽略设备像素比在 HiDPI 上又会发虚。故一次性高质量(SmoothTransformation)缩到图标的物理像素尺寸,
// 给结果打上 dpr 标记,并按 (路径, 尺寸, dpr) 缓存。
QPixmap homeLinkPixmap(const QString& resourcePath, const QSize& targetSize, qreal dpr,
                       const QColor& tint)
{
    const bool isGitHub = resourcePath.endsWith(QStringLiteral("GitHub-Mark.png"));
    const QSize physical = (QSizeF(targetSize) * dpr).toSize();
    const QString key = QStringLiteral("%1@%2x%3#%4").arg(resourcePath)
                            .arg(physical.width()).arg(physical.height())
                            .arg(isGitHub && tint.isValid() ? tint.name() : QString());
    static QHash<QString, QPixmap> cache;
    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();

    QImage image(resourcePath);
    if (!image.isNull()) {
        image = image.convertToFormat(QImage::Format_ARGB32);
        // The GitHub mark ships as black-on-white. Derive alpha from luminance (black → opaque,
        // white → transparent) and paint the silhouette in the theme text color, so it stays crisp
        // and visible on both light and dark cards instead of vanishing as a black blob on dark.
        // zh_CN: GitHub 标记是黑底白图。用亮度推导 alpha(黑→不透明,白→透明),并以主题文字色绘制剪影,
        // 使其在浅色/深色卡片上都清晰可见,而非在深色卡上糊成看不见的黑块。
        if (isGitHub) {
            const QColor c = tint.isValid() ? tint : QColor(0, 0, 0);
            for (int y = 0; y < image.height(); ++y) {
                auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    const QRgb pixel = line[x];
                    const int coverage = 255 - qGray(pixel);  // ink darkness → opacity
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

void drawCenteredPixmap(QPainter& painter, const QRectF& rect, const QString& resourcePath,
                        const QColor& tint = QColor())
{
    const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : 1.0;
    const QPixmap source = homeLinkPixmap(resourcePath, rect.size().toSize(), dpr, tint);
    if (source.isNull())
        return;

    // The pixmap is dpr-tagged, so its logical size = physical / dpr; centre it in the icon box.
    const QSizeF logical = QSizeF(source.size()) / source.devicePixelRatioF();
    const QPointF topLeft(rect.center().x() - logical.width() / 2.0,
                          rect.center().y() - logical.height() / 2.0);
    painter.drawPixmap(topLeft, source);
}

// A small, deterministic grayscale-noise tile, painted at very low opacity over the card to give the
// translucent surface a frosted "acrylic" grain instead of reading as flat glass. Generated once.
// zh_CN: 一小块确定性灰度噪声贴片,以极低不透明度铺在卡片上,让半透明表面有亚克力磨砂颗粒感,而非平板玻璃。仅生成一次。
const QImage& acrylicNoiseTile()
{
    static const QImage tile = []() {
        constexpr int n = 96;
        QImage img(n, n, QImage::Format_ARGB32);
        QRandomGenerator rng(0xACE71C5Eu);  // fixed seed → stable texture across repaints
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
        const QString candidate = current.isEmpty()
            ? words.at(i)
            : current + QLatin1Char(' ') + words.at(i);
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
        const QRect lineRect(rect.left(), rect.top() + i * lineHeight,
                             rect.width(), lineHeight);
        painter.drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter, lines.at(i));
    }
    painter.restore();
}

} // namespace

class GalleryHomeLinkDelegate : public QStyledItemDelegate, public fluent::FluentElement {
public:
    explicit GalleryHomeLinkDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return QSize(kHeroLinkCardWidth, kHeroLinkCellHeight);
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (!painter || !index.isValid())
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

        const Colors colors = themeColors();
        const bool dark = currentTheme() == Dark;
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const bool pressed = option.state.testFlag(QStyle::State_Sunken);

        const bool active = hovered || pressed;
        const qreal radius = ::CornerRadius::Overlay;

        // Hover raises the card a couple of pixels; the lift, deeper shadow and accent fringe
        // together read as "clickable, floating above the banner" (design: translateY(-2px)).
        // zh_CN: hover 时卡片抬起两像素；抬升 + 更深的阴影 + 强调色发丝边共同传达「可点、浮于横幅之上」。
        // Lift the card on hover. The strip reserves kHeroLinkTopPad above (so the raised top edge
        // and its accent border aren't clipped by the viewport) and kHeroLinkShadowMargin below.
        // zh_CN: hover 抬升。链接条上方预留 kHeroLinkTopPad（使抬起的顶边及其强调色描边不被视口裁切），
        // 下方预留 kHeroLinkShadowMargin。
        const qreal lift = active ? 2.0 : 0.0;
        QRectF cardRect(QPointF(option.rect.left(), option.rect.top() + kHeroLinkTopPad),
                        QSizeF(kHeroLinkCardWidth, kHeroLinkCardHeight));
        cardRect.adjust(0.5, 0.5, -0.5, -0.5);
        cardRect.translate(0, -lift);
        QPainterPath cardPath;
        cardPath.addRoundedRect(cardRect, radius, radius);

        // Soft drop shadow — feathered concentric layers, clipped to OUTSIDE the card so it never
        // darkens the translucent surface, and biased downward so the card rests on a gentle
        // cushion. Light at rest, a touch deeper on hover.
        // zh_CN: 柔和投影——羽化同心层，裁剪到卡片之外，绝不压暗半透明表面；向下偏移，使卡片落在柔垫上。
        // 静息轻、hover 略深。
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
            const qreal f = (i + 1.0) / shadowLayers;        // 0 → 1 outward
            const qreal grow = reach * f;
            QColor s(0, 0, 0, qRound(peakAlpha * (1.0 - f)));
            QPainterPath sp;
            sp.addRoundedRect(cardRect.adjusted(-grow, -grow * 0.5 + dyBias, grow, grow + dyBias),
                              radius + grow, radius + grow);
            painter->fillPath(sp, s);
        }
        painter->restore();

        // Acrylic surface: a translucent layer tint lets the banner gradient and decorative circles
        // show through clearly (Fluent material), and a faint grayscale-noise grain on top gives it
        // the frosted look instead of flat glass. The shadow is clipped out above, so the
        // translucency reveals the banner, not the shadow. Text is drawn afterwards, so it stays crisp.
        // zh_CN: 亚克力表面：半透明 layer 着色让横幅渐变与装饰圆清晰透出（Fluent 材质感），再叠一层淡淡的灰度
        // 噪声颗粒,呈现磨砂感而非平板玻璃。投影已裁到卡外,透出的是横幅而非阴影。文字随后绘制,保持清晰。
        QColor surface = dark ? colors.bgLayerAlt : colors.bgLayer;
        surface.setAlpha(dark ? 112 : 132);
        painter->fillPath(cardPath, surface);

        painter->save();
        painter->setClipPath(cardPath);
        painter->setOpacity(dark ? 0.05 : 0.035);
        painter->fillRect(cardRect, QBrush(acrylicNoiseTile()));
        painter->restore();

        // Border: a barely-there hairline at rest (≈ rgba(0,0,0,0.05)) so the card reads as a
        // shadow-defined surface, NOT an outlined box; on hover/press it becomes the accent fringe.
        // zh_CN: 描边：静息态几乎不可见（≈ rgba(0,0,0,0.05)），让卡片靠阴影成形，而非画成带框的盒子；
        // hover/按下时转为强调色发丝边。
        QColor border = active ? colors.accentDefault
                               : (dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 13));
        if (active)
            border.setAlpha(dark ? 150 : 120);
        painter->setPen(QPen(border, 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(cardPath);

        const int contentLeft = qRound(cardRect.left()) + kHeroLinkCardPadding;
        const int contentWidth = qRound(cardRect.width()) - kHeroLinkCardPadding * 2;
        const QRectF iconRect(contentLeft,
                              cardRect.top() + kHeroLinkCardPadding,
                              kHeroLinkIconSize,
                              kHeroLinkIconSize);
        // The GitHub mark is recolored to the text color (theme-aware); colored logos ignore the tint.
        // zh_CN: GitHub 标记按文字色重新着色(随主题);彩色 logo 忽略该 tint。
        drawCenteredPixmap(*painter, iconRect, index.data(LinkImageRole).toString(),
                           colors.textPrimary);

        int textY = qRound(iconRect.bottom()) + 14;

        QFont titleFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
        QFont descFont = themeFont(Typography::FontRole::Caption).toQFont();
        QFontMetrics titleMetrics(titleFont);

        const QString title = index.data(LinkTitleRole).toString();
        painter->setFont(titleFont);
        painter->setPen(colors.textPrimary);
        painter->drawText(QRect(contentLeft, textY, contentWidth, titleMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(title, Qt::ElideRight, contentWidth));

        textY += titleMetrics.height() + 4;
        const QString description = index.data(LinkDescriptionRole).toString();

        QFont glyphFont(Typography::FontFamily::SegoeFluentIcons);
        glyphFont.setPixelSize(13);
        // External-link glyph moves to the top-right corner and turns accent on hover, reading as
        // a standard "opens externally" affordance instead of floating alone in the bottom corner.
        // zh_CN: 外链字形移至右上角，hover 时转强调色，作为标准“外部打开”记号，而非孤零零浮在右下角。
        const QRect externalRect(qRound(cardRect.right()) - kHeroLinkCardPadding - 16,
                                 qRound(cardRect.top()) + kHeroLinkCardPadding,
                                 16,
                                 16);
        drawElidedWrappedText(*painter,
                              QRect(contentLeft,
                                    textY,
                                    contentWidth,
                                    qRound(cardRect.bottom()) - kHeroLinkCardPadding - textY),
                              description,
                              descFont,
                              colors.textSecondary,
                              3);

        painter->setFont(glyphFont);
        painter->setPen(active ? colors.accentDefault : colors.textSecondary);
        painter->drawText(externalRect, Qt::AlignCenter, kExternalLinkGlyph);

        painter->restore();
    }
};

class GalleryHomeLinkStrip : public fluent::collections::ListView {
public:
    explicit GalleryHomeLinkStrip(QWidget* parent = nullptr)
        : fluent::collections::ListView(parent)
    {
        setObjectName(QStringLiteral("galleryHomeHeroLinksView"));
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
        setSelectionMode(ListSelectionMode::None);
        setBackgroundVisible(false);
        setBorderVisible(false);
        setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setAttribute(Qt::WA_TranslucentBackground);
        viewport()->setAttribute(Qt::WA_TranslucentBackground);
        viewport()->setCursor(Qt::PointingHandCursor);
        setStyleSheet(QStringLiteral(
            "QListView#galleryHomeHeroLinksView { background: transparent; border: none; }"
            "QListView#galleryHomeHeroLinksView::item { background: transparent; }"));

        auto* delegate = new GalleryHomeLinkDelegate(this);
        setItemDelegate(delegate);

        auto* model = new QStandardItemModel(this);
        auto append = [model](const QString& title,
                              const QString& description,
                              const QString& url,
                              const QString& imagePath) {
            auto* item = new QStandardItem(title);
            item->setEditable(false);
            item->setData(title, LinkTitleRole);
            item->setData(description, LinkDescriptionRole);
            item->setData(QUrl(url), LinkUrlRole);
            item->setData(imagePath, LinkImageRole);
            model->appendRow(item);
        };
        append(QStringLiteral("Design"),
               QStringLiteral("Guidelines and toolkits for Fluent design."),
               QStringLiteral("https://aka.ms/WinUI/3.0-figma-toolkit"),
               QStringLiteral(":/app/assets/home_header_tiles/Header-WindowsDesign.png"));
        append(QStringLiteral("macOS 27 Community"),
               QStringLiteral("Community Figma design kit for macOS 27."),
               QStringLiteral("https://www.figma.com/community/file/1651309434229735362"),
               QStringLiteral(":/app/assets/home_header_tiles/Header-macOS27.png"));
        append(QStringLiteral("Material 3 Design Kit"),
               QStringLiteral("Community Figma kit for Material 3."),
               QStringLiteral("https://www.figma.com/community/file/1035203688168086460"),
               QStringLiteral(":/app/assets/home_header_tiles/Header-Material3.png"));
        append(QStringLiteral("WinUI Gallery"),
               QStringLiteral("WinUI Gallery source on GitHub."),
               QStringLiteral("https://github.com/microsoft/WinUI-Gallery"),
               QStringLiteral(":/app/assets/home_header_tiles/GitHub-Mark.png"));
        append(QStringLiteral("Fluent UI"),
               QStringLiteral("Fluent controls and patterns for the web."),
               QStringLiteral("https://developer.microsoft.com/en-us/fluentui#/controls/web"),
               QStringLiteral(":/app/assets/home_header_tiles/Header-Toolkit.png"));
        append(QStringLiteral("Fluent Qt"),
               QStringLiteral("Fluent Qt source on GitHub."),
               QStringLiteral("https://github.com/calvinhxx/Fluent-QT"),
               QStringLiteral(":/app/assets/home_header_tiles/Header-WinUI.png"));
        append(QStringLiteral("Qt Quick Controls"),
               QStringLiteral("Qt Quick Controls reference on doc.qt.io."),
               QStringLiteral("https://doc.qt.io/qt-6/qtquickcontrols-index.html"),
               QStringLiteral(":/app/assets/home_header_tiles/Qt-Logo.png"));
        setModel(model);

        connect(horizontalScrollBar(), &QScrollBar::rangeChanged,
                this, [this] {
                    hideScrollChrome();
                    updateScrollButtonsVisibility();
                });
        connect(horizontalScrollBar(), &QScrollBar::valueChanged,
                this, [this] {
                    hideScrollChrome();
                    updateScrollButtonsVisibility();
                });

        m_backButton = createScrollButton(Typography::Icons::FlipViewPrevH,
                                          QStringLiteral("Scroll left"));
        m_forwardButton = createScrollButton(Typography::Icons::FlipViewNextH,
                                             QStringLiteral("Scroll right"));
        connect(m_backButton, &QPushButton::clicked,
                this, [this] { scrollByViewport(-1); });
        connect(m_forwardButton, &QPushButton::clicked,
                this, [this] { scrollByViewport(1); });

        hideScrollChrome();
        updateScrollButtonsGeometry();
        updateScrollButtonsVisibility();

        connect(this, &fluent::collections::ListView::itemClicked,
                this, [model](int row) {
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
    fluent::basicinput::Button* createScrollButton(const QString& glyph,
                                                   const QString& tooltip)
    {
        auto* button = new fluent::basicinput::Button(this);
        button->setObjectName(QStringLiteral("galleryHomeHeroScrollButton"));
        button->setFluentStyle(fluent::basicinput::Button::Standard);
        button->setFluentSize(fluent::basicinput::Button::Small);
        button->setFluentLayout(fluent::basicinput::Button::IconOnly);
        button->setIconGlyph(glyph, 12);
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
        const int next = qBound(bar->minimum(),
                                bar->value() + direction * delta,
                                bar->maximum());
        bar->setValue(next);
        updateScrollButtonsVisibility();
    }

    void updateScrollButtonsGeometry()
    {
        const int y = qMax(0, (height() - kHeroScrollButtonHeight) / 2 - kHeroScrollButtonLift);
        if (m_backButton) {
            m_backButton->setGeometry(kHeroScrollButtonMarginX,
                                      y,
                                      kHeroScrollButtonWidth,
                                      kHeroScrollButtonHeight);
            m_backButton->raise();
        }
        if (m_forwardButton) {
            m_forwardButton->setGeometry(width() - kHeroScrollButtonMarginX - kHeroScrollButtonWidth,
                                         y,
                                         kHeroScrollButtonWidth,
                                         kHeroScrollButtonHeight);
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
    explicit GalleryHomeHeroBanner(const QString& title,
                                   const QString& tagline,
                                   QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("galleryHomeHero"));
        setFixedHeight(kHeroHeight);
        // Translucent backing so the bottom dissolve can fade the banner art to fully transparent and
        // reveal the SAME OS backdrop (Mica/Acrylic) the content area shows — no opaque seam between
        // them. In Normal mode the opaque wash covers this completely, so it's a no-op there.
        // zh_CN: 透明背板，使底部溶解能把横幅美术层淡到全透明，露出与内容区相同的系统背景（Mica/Acrylic），
        // 两者之间不再有不透明硬缝。Normal 模式下不透明 wash 会完全覆盖它，等同无操作。
        setAttribute(Qt::WA_TranslucentBackground);

        auto* layout = new QVBoxLayout(this);
        // The floating link ListView occupies the lower half of the hero, so keep the
        // text block in the upper band and let the card strip sit on the artwork.
        // zh_CN: 悬浮链接 ListView 占用 hero 下半区，因此文字块放在上方，卡片条压在横幅图上。
        layout->setContentsMargins(kHeroMarginX, 36, kHeroMarginX,
                                   kHeroHeight - kHeroLinkStripTop + 12);
        layout->setSpacing(12);

        m_iconLabel = new QLabel(this);
        m_iconLabel->setObjectName(QStringLiteral("galleryHomeHeroIcon"));
        m_iconLabel->setFixedSize(kHeroIconSize, kHeroIconSize);
        m_iconLabel->setPixmap(appicon::pixmap(kHeroIconSize, devicePixelRatioF()));
        m_iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
        layout->addWidget(m_iconLabel);
        // The TitleLarge glyphs sit low in their line box, so the bare 12px layout spacing reads
        // as a cramped ~8px gap under the icon — add a little breathing room above the title.
        // zh_CN: TitleLarge 字形在行框中偏下，纯 12px 间距视觉上只剩约 8px，显得拥挤——在标题上方补一点留白。
        layout->addSpacing(8);

        m_titleLabel = new fluent::textfields::Label(title, this);
        m_titleLabel->setObjectName(QStringLiteral("galleryHomeHeroTitle"));
        m_titleLabel->setFluentTypography(Typography::FontRole::TitleLarge);
        layout->addWidget(m_titleLabel);

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

    void onThemeUpdated() override
    {
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
        if (!m_linkStrip)
            return;
        const int stripX = qMax(0, kHeroLinkStripInsetX);
        const int stripWidth = qMax(0, width() - stripX - kHeroMarginX);
        m_linkStrip->setGeometry(stripX,
                                 kHeroLinkStripTop,
                                 stripWidth,
                                 kHeroLinkStripHeight);
    }

    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::WindowActivate || e->type() == QEvent::WindowDeactivate)
            update();  // bottom dissolve tracks the window backdrop
        return QWidget::event(e);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool dark = currentTheme() == Dark;
        const Colors colors = themeColors();
        // A real OS-composited backdrop (Win11 Mica / macOS vibrancy) — the same contract the content
        // host reads. Under it the page is transparent and the backdrop shows through, so the hero must
        // dissolve INTO that backdrop rather than onto an opaque bgLayerAlt plate (which reads as a seam).
        // zh_CN: 真实系统合成背景（Win11 Mica / macOS vibrancy）——与内容宿主同一契约。此时页面透明、背景透出，
        // 故 hero 必须溶解进该背景，而非落在不透明 bgLayerAlt 板上（那会显示为硬缝）。
        const bool realBackdrop = window()
            && window()->testAttribute(Qt::WA_TranslucentBackground)
            && window()->property("fluentMicaBackdrop").toBool();

        // The banner is full-bleed with only its top-LEFT corner rounded (matching the content
        // surface, which rounds top-left where it meets the pane and stays square on the right where
        // it runs to the window edge); its bottom dissolves into the content layer so the image
        // connects to the page seamlessly (WinUI home). A rounded top-right would leave a small
        // backdrop notch against the window edge.
        // zh_CN: 横幅通栏铺满、仅左上角圆角（与内容表面一致：左上与窗格相接处圆角，右侧跑到窗口边缘保持直角）；
        // 底部淡入内容层，使图像与页面无缝衔接（WinUI 首页）。右上角若圆角会在窗口边缘留下一小块背景缺口。
        const QRectF banner(0, 0, width(), height());
        const qreal radius = themeRadius().overlay;
        QPainterPath clip = fluent::overlay::roundedCornerRectPath(
            banner, radius, /*TL*/ true, /*TR*/ false, /*BR*/ false, /*BL*/ false);
        painter.save();
        painter.setClipPath(clip);

        // Many eased stops (vs the old 3) smooth the adjacent hues so the deep blue→purple range no
        // longer shows a visible colour band across the wash. (design: silky multi-stop gradient)
        // zh_CN: 用多个缓动色标（取代原来的 3 个）平滑相邻色相，使深蓝→紫区不再出现可见色带。
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

        // Decorative blooms drift off the right edge — soft radial gradients with a fully transparent
        // outer stop, so they glow into the wash instead of leaving the hard-edged rim the old solid
        // circles did. zh_CN: 装饰光晕从右缘漂入——外缘全透明的径向渐变，柔和融入底色，而非旧实心圆那种生硬轮廓。
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
        bloom({bx + w * 0.86, by - h * 0.30}, h * 1.5,
              dark ? QColor(86, 150, 232, 82) : QColor(255, 255, 255, 210));
        bloom({bx + w * 0.94, by + h * 1.10}, h * 1.3,
              dark ? QColor(150, 96, 206, 76) : QColor(214, 180, 236, 128));
        bloom({bx + w * 0.62, by + h * 1.15}, h * 1.0,
              dark ? QColor(196, 116, 168, 46) : QColor(247, 210, 214, 102));

        // Fine grain dither: 8-bit-per-channel gradients band in the deep blue→purple range; a faint
        // overlay-blended noise (~5%) scatters the bands so the wash reads silky rather than stepped.
        // Reuses the cached acrylic noise tile. zh_CN: 细噪点抖动：8bpc 渐变在深蓝→紫区会出现色带；
        // 叠一层极淡(~5%)的 overlay 噪点把色带打散，使底色丝滑而非阶梯状。复用已缓存的亚克力噪声贴片。
        painter.save();
        painter.setOpacity(dark ? 0.05 : 0.045);
        painter.setCompositionMode(QPainter::CompositionMode_Overlay);
        painter.fillRect(banner, QBrush(acrylicNoiseTile()));
        painter.restore();

        const QRectF fadeRect(banner.left(), banner.bottom() - kHeroBottomFade,
                              banner.width(), kHeroBottomFade);

        if (realBackdrop) {
            // Mica/Acrylic: the content area below is transparent and shows the OS backdrop, so fade
            // the banner ART itself to transparent with a DestinationIn alpha mask — it melts into the
            // SAME backdrop with no seam. A gentle hold-back alpha across the upper band also lets the
            // backdrop tint subtly through the whole hero ("a little transparency"), while the text,
            // icon and link cards (separate child widgets painted afterwards) stay fully crisp.
            // zh_CN: Mica/Acrylic：下方内容区透明、透出系统背景，故用 DestinationIn alpha 蒙版把横幅美术层本身
            // 淡到透明——它融入同一背景且无缝。上半区保留一点点透明，让背景色调透过整个 hero（“带一点透明”）；
            // 文字、图标与链接卡片是后绘的独立子控件，保持清晰。
            const qreal veil = dark ? 0.90 : 0.92;  // upper-band opacity → backdrop shows ~8–10% through the art
            const qreal fadeTop = qBound(0.0,
                1.0 - qreal(kHeroBottomFade) / qMax(1.0, banner.height()), 1.0);
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
            // Normal (opaque chrome): dissolve onto the content-layer surface (bgLayerAlt) the host
            // paints behind the transparent page, so the banner transitions seamlessly into the page.
            // A softer four-step curve eases the banner in so the seam under the card strip fully fades.
            // zh_CN: Normal（不透明 chrome）：渐隐到宿主在透明页面之后绘制的内容层表面（bgLayerAlt），无缝过渡到
            // 页面。更柔的四段曲线让横幅渐隐，卡片条下方的接缝彻底化开。
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
    }

private:
    void applyTextPalette()
    {
        const Colors colors = themeColors();
        if (m_titleLabel) {
            m_titleLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                            .arg(cssColor(colors.textPrimary)));
        }
        if (m_taglineLabel) {
            m_taglineLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                              .arg(cssColor(colors.textSecondary)));
        }
    }

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
    // zh_CN: hero 取代默认标题，并横贯整页宽度。
    setPageHeaderVisible(false);
    setViewportMargins(QMargins(0, 0, 0, 0));
    setContentSpacing(0);

    m_heroBanner = new GalleryHomeHeroBanner(
        QStringLiteral("Fluent-QT Gallery"), entry.description, this);
    addContentWidget(m_heroBanner);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("galleryHomeBody"));
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(kBodyMarginX, kBodyMarginTop, kBodyMarginX, 48);
    bodyLayout->setSpacing(16);

    // Each section's cards are drawn by one responsive GalleryEntryGrid (same as the
    // category pages), so they reflow 1/2/3 columns with the window instead of clipping.
    // zh_CN: 每个分区的卡片用一个自适应 GalleryEntryGrid 绘制（与分类页一致），随窗口在 1/2/3 列间重排而非裁切。
    auto addEntryGrid = [this, body, bodyLayout](const QString& objectName) -> GalleryEntryGrid* {
        auto* grid = new GalleryEntryGrid(body);
        grid->setObjectName(objectName);
        connect(grid, &GalleryEntryGrid::activated,
                this, &GalleryContentPage::routeActivated);
        bodyLayout->addWidget(grid);
        return grid;
    };

    // Featured samples mirror the curated routes from the content catalog.
    // zh_CN: 精选示例来自内容目录的精选路由。
    fluent::textfields::Label* featuredHeader = createTrackedLabel(
        QStringLiteral("Featured samples"), Typography::FontRole::Subtitle, TextRole::Primary);
    featuredHeader->setObjectName(QStringLiteral("galleryHomeFeaturedHeader"));
    bodyLayout->addWidget(featuredHeader);

    QVector<GalleryEntryGrid::Entry> featuredEntries;
    for (const QString& routeId : entry.relatedRouteIds) {
        const GalleryNavigationItem* item = navigationViewModel.itemById(routeId);
        if (!item)
            continue;
        QString description;
        if (const GalleryContentEntry* componentEntry = galleryContentEntry(routeId))
            description = componentEntry->description;
        featuredEntries.append({item->id, item->title, description,
                                QPixmap(galleryControlImageResource(item->title)), QString()});
    }
    addEntryGrid(QStringLiteral("galleryHomeCards"))->setEntries(featuredEntries);

    // Category quick links cover the rest of the catalog.
    // zh_CN: 分类捷径覆盖目录其余部分。
    bodyLayout->addSpacing(12);
    fluent::textfields::Label* categoriesHeader = createTrackedLabel(
        QStringLiteral("Browse by category"), Typography::FontRole::Subtitle, TextRole::Primary);
    categoriesHeader->setObjectName(QStringLiteral("galleryHomeCategoriesHeader"));
    bodyLayout->addWidget(categoriesHeader);

    QVector<GalleryEntryGrid::Entry> categoryEntries;
    auto appendCategory = [&](const GalleryNavigationItem& item) {
        QString description;
        if (const GalleryContentEntry* categoryEntry = galleryContentEntry(item.id))
            description = categoryEntry->description;
        categoryEntries.append({item.id, item.title, description, QPixmap(), item.iconGlyph});
    };
    if (const GalleryNavigationItem* allControls =
            navigationViewModel.itemById(QStringLiteral("all-controls")))
        appendCategory(*allControls);
    for (const GalleryNavigationItem& item : navigationViewModel.items()) {
        if (item.kind == GalleryNavigationItem::Kind::CategoryRoute)
            appendCategory(item);
    }
    addEntryGrid(QStringLiteral("galleryHomeCategoryCards"))->setEntries(categoryEntries);

    addContentWidget(body);

    LOG_DEBUG(QStringLiteral("GalleryHomePage created routeId=%1 featuredCards=%2 categoryCards=%3")
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

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
constexpr int kHeroBottomFade = 160; // Bottom band that dissolves the banner into the page.
constexpr int kBodyMarginX = 24;     // Body content inset (was 48) — matches the hero.
constexpr int kBodyMarginTop = 20;
constexpr int kHeroIconSize = 56;
constexpr int kHeroLinkCardWidth = 198;
constexpr int kHeroLinkCardHeight = 150;
constexpr int kHeroLinkCardSpacing = 16;
constexpr int kHeroLinkCardPadding = 18;
constexpr int kHeroLinkIconSize = 30;
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

QPixmap homeLinkPixmap(const QString& resourcePath)
{
    static QHash<QString, QPixmap> cache;
    const auto it = cache.constFind(resourcePath);
    if (it != cache.constEnd())
        return it.value();

    QImage image(resourcePath);
    if (resourcePath.endsWith(QStringLiteral("GitHub-Mark.png"))) {
        image = image.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y) {
            auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const QRgb pixel = line[x];
                if (qRed(pixel) > 245 && qGreen(pixel) > 245 && qBlue(pixel) > 245)
                    line[x] = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), 0);
            }
        }
    }

    const QPixmap pixmap = QPixmap::fromImage(image);
    cache.insert(resourcePath, pixmap);
    return pixmap;
}

void drawCenteredPixmap(QPainter& painter, const QRectF& rect, const QString& resourcePath)
{
    const QPixmap source = homeLinkPixmap(resourcePath);
    if (source.isNull())
        return;

    const QSize targetSize = source.size().scaled(rect.size().toSize(),
                                                  Qt::KeepAspectRatio);
    const QRect target(QPoint(qRound(rect.center().x() - targetSize.width() / 2.0),
                              qRound(rect.center().y() - targetSize.height() / 2.0)),
                       targetSize);
    painter.drawPixmap(target, source);
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

        // Acrylic-style surface: a high-opacity layer tint so the banner gradient faintly shows
        // through (Fluent material) while the card stays clearly a lifted, readable surface. The
        // shadow is clipped out above, so the translucency reveals the banner, not the shadow.
        // zh_CN: 亚克力风表面：高不透明度的 layer 着色，让横幅渐变隐约透出（Fluent 材质感），同时卡片仍
        // 清晰可读、明确抬起。投影已裁到卡外，半透明透出的是横幅而非阴影。
        QColor surface = dark ? colors.bgLayerAlt : colors.bgLayer;
        surface.setAlpha(dark ? 158 : 184);
        painter->fillPath(cardPath, surface);

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
        drawCenteredPixmap(*painter, iconRect, index.data(LinkImageRole).toString());

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
        setSelectionMode(ListSelectionMode::None);
        setBackgroundVisible(false);
        setBorderVisible(false);
        setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setAttribute(Qt::WA_TranslucentBackground);
        viewport()->setAttribute(Qt::WA_TranslucentBackground);
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
        button->setToolTip(tooltip);
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

        QLinearGradient wash(banner.topLeft(), banner.bottomRight());
        if (dark) {
            wash.setColorAt(0.0, QColor(0x1B, 0x2A, 0x41));
            wash.setColorAt(0.55, QColor(0x27, 0x22, 0x44));
            wash.setColorAt(1.0, QColor(0x33, 0x26, 0x3C));
        } else {
            wash.setColorAt(0.0, QColor(0xD6, 0xE7, 0xF7));
            wash.setColorAt(0.55, QColor(0xE4, 0xDF, 0xF6));
            wash.setColorAt(1.0, QColor(0xF4, 0xE7, 0xEA));
        }
        painter.fillRect(banner, wash);

        // Decorative translucent circles drift off the right edge, like the
        // abstract shapes in the WinUI Gallery banner art.
        // zh_CN: 半透明装饰圆漂出右缘，呼应 WinUI Gallery 横幅的抽象图形。
        const QColor circle = dark ? QColor(255, 255, 255, 14) : QColor(255, 255, 255, 90);
        painter.setPen(Qt::NoPen);
        painter.setBrush(circle);
        const qreal w = banner.width();
        const qreal h = banner.height();
        const qreal bx = banner.left();
        const qreal by = banner.top();
        painter.drawEllipse(QPointF(bx + w * 0.80, by + h * 0.12), h * 0.85, h * 0.85);
        painter.drawEllipse(QPointF(bx + w * 0.95, by + h * 0.90), h * 0.55, h * 0.55);
        painter.drawEllipse(QPointF(bx + w * 0.62, by + h * 1.05), h * 0.35, h * 0.35);

        // Bottom dissolve into the content layer surface (bgLayerAlt) painted by the
        // NavigationView behind the transparent page, so the banner transitions seamlessly
        // into the content instead of ending on a hard seam above "Featured samples".
        // zh_CN: 底部渐隐到 NavigationView 在透明页面之后绘制的内容层表面（bgLayerAlt），
        // 使横幅无缝过渡到内容，而非在「Featured samples」上方留硬缝。
        const QRectF fadeRect(banner.left(), banner.bottom() - kHeroBottomFade,
                              banner.width(), kHeroBottomFade);
        QColor base = colors.bgLayerAlt;
        QColor clear = base;
        clear.setAlpha(0);
        QColor soft = base;
        soft.setAlpha(dark ? 34 : 42);
        QColor medium = base;
        medium.setAlpha(dark ? 122 : 148);
        QLinearGradient fade(fadeRect.topLeft(), fadeRect.bottomLeft());
        fade.setColorAt(0.0, clear);
        fade.setColorAt(0.38, soft);
        fade.setColorAt(0.72, medium);
        fade.setColorAt(1.0, base);
        painter.fillRect(fadeRect, fade);

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
        QStringLiteral("WinUI 3 Gallery"), entry.description, this);
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

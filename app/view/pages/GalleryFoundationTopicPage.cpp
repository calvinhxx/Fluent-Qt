#include "GalleryFoundationTopicPage.h"

#include <functional>
#include <utility>

#include <QClipboard>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStringList>
#include <QVBoxLayout>
#include <QVector>

#include "components/basicinput/Button.h"
#include "components/basicinput/Slider.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/foundation/QMLPlus.h"
#include "components/status_info/ProgressBar.h"
#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/support/GalleryToast.h"
#include "view/widgets/GalleryCodeBlock.h"
#include "view/widgets/GalleryIconBrowser.h"
#include "support/logging/Log.h"

namespace fluent::gallery {
namespace {

using Colors = fluent::FluentElement::Colors;

constexpr int kCardRadius = 8;   // CornerRadius::Overlay
constexpr int kCardPadding = 20;
constexpr int kGridSpacing = 12;

/** @brief Resolves a topic's display title from the navigation model. */
QString resolveTitle(const GalleryContentEntry& entry, const GalleryNavigationViewModel& vm)
{
    if (const GalleryNavigationItem* item = vm.itemById(entry.routeId))
        return item->title;
    return entry.title;
}

/** @brief Paints a rounded surface (bgLayer fill + card stroke) into rect. */
void paintCardSurface(QPainter& painter, const QRectF& rect, const Colors& colors)
{
    painter.setPen(QPen(colors.strokeCard, 1.0));
    painter.setBrush(colors.bgLayer);
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), kCardRadius, kCardRadius);
}

// ---------------------------------------------------------------------------
// Typography: the Fluent type ramp, one specimen row per font role.
// zh_CN: 排版：Fluent 字体角色阶梯，每个角色一行样本。
// ---------------------------------------------------------------------------
class TypeRampCard : public QWidget, public fluent::FluentElement {
public:
    explicit TypeRampCard(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_rows = {
            {Typography::FontRole::Display, QStringLiteral("Display"), 68, 92, QStringLiteral("SemiBold")},
            {Typography::FontRole::TitleLarge, QStringLiteral("Title Large"), 40, 52, QStringLiteral("SemiBold")},
            {Typography::FontRole::Title, QStringLiteral("Title"), 28, 36, QStringLiteral("SemiBold")},
            {Typography::FontRole::Subtitle, QStringLiteral("Subtitle"), 20, 28, QStringLiteral("SemiBold")},
            {Typography::FontRole::BodyLargeStrong, QStringLiteral("Body Large Strong"), 18, 24, QStringLiteral("SemiBold")},
            {Typography::FontRole::BodyLarge, QStringLiteral("Body Large"), 18, 24, QStringLiteral("Regular")},
            {Typography::FontRole::BodyStrong, QStringLiteral("Body Strong"), 14, 20, QStringLiteral("SemiBold")},
            {Typography::FontRole::Body, QStringLiteral("Body"), 14, 20, QStringLiteral("Regular")},
            {Typography::FontRole::Caption, QStringLiteral("Caption"), 12, 16, QStringLiteral("Regular")},
        };
    }

    QSize sizeHint() const override { return QSize(480, totalHeight()); }
    QSize minimumSizeHint() const override { return QSize(0, totalHeight()); }
    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const Colors colors = themeColors();
        paintCardSurface(painter, QRectF(rect()), colors);

        const QFont captionFont = themeFont(Typography::FontRole::Caption).toQFont();
        const int contentLeft = kCardPadding;
        const int contentRight = width() - kCardPadding;
        const int metricsWidth = 220;

        int y = kCardPadding;
        for (int i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows.at(i);
            const QFont roleFont = themeFont(row.role).toQFont();
            const int h = rowHeight(row);

            painter.setFont(roleFont);
            painter.setPen(colors.textPrimary);
            painter.drawText(QRect(contentLeft, y, contentRight - contentLeft - metricsWidth, h),
                             Qt::AlignLeft | Qt::AlignVCenter, row.name);

            painter.setFont(captionFont);
            painter.setPen(colors.textSecondary);
            const QString metrics =
                QStringLiteral("%1 / %2 · %3").arg(row.size).arg(row.lineHeight).arg(row.weight);
            painter.drawText(QRect(contentRight - metricsWidth, y, metricsWidth, h),
                             Qt::AlignRight | Qt::AlignVCenter, metrics);

            y += h;
            if (i != m_rows.size() - 1) {
                painter.setPen(QPen(colors.strokeDivider, 1.0));
                painter.drawLine(contentLeft, y + kGridSpacing / 2,
                                 contentRight, y + kGridSpacing / 2);
                y += kGridSpacing;
            }
        }
    }

private:
    struct Row {
        QString role;
        QString name;
        int size;
        int lineHeight;
        QString weight;
    };

    int rowHeight(const Row& row) const
    {
        const QFontMetrics fm(themeFont(row.role).toQFont());
        return qMax(row.lineHeight, fm.height());
    }

    int totalHeight() const
    {
        int h = kCardPadding * 2;
        for (int i = 0; i < m_rows.size(); ++i) {
            h += rowHeight(m_rows.at(i));
            if (i != m_rows.size() - 1)
                h += kGridSpacing;
        }
        return h;
    }

    QVector<Row> m_rows;
};

// ---------------------------------------------------------------------------
// Reflowing tile grid used by the Color token page.
// zh_CN: Color token 页面使用的响应式重排网格。
// ---------------------------------------------------------------------------
class FoundationTileGrid : public QWidget, public fluent::FluentElement {
public:
    enum Kind { Swatch, Icon };

    struct Tile {
        QString title;
        QString glyph;                                 // Icon kind. zh_CN: 图标变体。
        std::function<QColor(const Colors&)> colorOf;  // Swatch kind. zh_CN: 色板变体。
    };

    explicit FoundationTileGrid(Kind kind, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_kind(kind)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // Swatches are click-to-copy; the cursor and hover highlight advertise that.
        // zh_CN: 色板支持点击复制；指针与悬停高亮提示其可点击。
        if (m_kind == Swatch) {
            setMouseTracking(true);
            setCursor(Qt::PointingHandCursor);
        }
    }

    void setTiles(const QVector<Tile>& tiles)
    {
        m_tiles = tiles;
        updateGeometry();
        update();
    }

    QSize sizeHint() const override { return QSize(480, gridHeight()); }
    QSize minimumSizeHint() const override { return QSize(0, gridHeight()); }
    void onThemeUpdated() override { update(); }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        const int cols = columns();
        if (cols != m_lastColumns) {
            m_lastColumns = cols;
            updateGeometry();
        }
        update();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_kind == Swatch)
            setHoveredIndex(cellIndexAt(event->pos()));
        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        setHoveredIndex(-1);
        QWidget::leaveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (m_kind == Swatch && event->button() == Qt::LeftButton) {
            const int idx = cellIndexAt(event->pos());
            if (idx >= 0 && m_tiles.at(idx).colorOf) {
                const QString hex = hexOf(m_tiles.at(idx));
                if (QClipboard* clip = QGuiApplication::clipboard())
                    clip->setText(hex);
                showGalleryToast(this, QStringLiteral("Copied %1").arg(hex));
            }
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        if (m_tiles.isEmpty())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const Colors colors = themeColors();
        const QFont titleFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
        const QFont captionFont = themeFont(Typography::FontRole::Caption).toQFont();

        for (int i = 0; i < m_tiles.size(); ++i) {
            const QRect cell = cellRect(i);
            painter.setPen(QPen(colors.strokeCard, 1.0));
            painter.setBrush(i == m_hoveredIndex ? colors.subtleSecondary : colors.bgLayer);
            painter.drawRoundedRect(QRectF(cell).adjusted(0.5, 0.5, -0.5, -0.5),
                                    kCardRadius, kCardRadius);
            if (m_kind == Swatch)
                paintSwatch(painter, cell, m_tiles.at(i), colors, titleFont, captionFont);
            else
                paintIcon(painter, cell, m_tiles.at(i), colors, captionFont);
        }
    }

private:
    int minCellWidth() const { return m_kind == Icon ? 68 : 200; }
    int cellHeight() const { return m_kind == Icon ? 52 : 112; }

    int columns() const
    {
        const int maxCols = m_kind == Icon ? 12 : 4;
        return qBound(1, (width() + kGridSpacing) / (minCellWidth() + kGridSpacing), maxCols);
    }

    int rowCount() const
    {
        if (m_tiles.isEmpty())
            return 0;
        const int cols = columns();
        return (m_tiles.size() + cols - 1) / cols;
    }

    int gridHeight() const
    {
        const int rows = rowCount();
        return rows == 0 ? 0 : rows * cellHeight() + (rows - 1) * kGridSpacing;
    }

    int columnWidth() const
    {
        const int cols = columns();
        return qMax(0, (width() - (cols - 1) * kGridSpacing) / cols);
    }

    QRect cellRect(int index) const
    {
        const int cols = columns();
        const int w = columnWidth();
        const int x = (index % cols) * (w + kGridSpacing);
        const int y = (index / cols) * (cellHeight() + kGridSpacing);
        return QRect(x, y, w, cellHeight());
    }

    int cellIndexAt(const QPoint& pos) const
    {
        for (int i = 0; i < m_tiles.size(); ++i) {
            if (cellRect(i).contains(pos))
                return i;
        }
        return -1;
    }

    void setHoveredIndex(int index)
    {
        if (m_hoveredIndex == index)
            return;
        const int previous = m_hoveredIndex;
        m_hoveredIndex = index;
        if (previous >= 0 && previous < m_tiles.size())
            update(cellRect(previous));
        if (index >= 0 && index < m_tiles.size())
            update(cellRect(index));
    }

    QString hexOf(const Tile& tile) const
    {
        const QColor c = tile.colorOf ? tile.colorOf(themeColors()) : QColor(Qt::transparent);
        return c.alpha() == 255 ? c.name(QColor::HexRgb).toUpper()
                                : c.name(QColor::HexArgb).toUpper();
    }

    void paintSwatch(QPainter& painter,
                     const QRect& cell,
                     const Tile& tile,
                     const Colors& colors,
                     const QFont& titleFont,
                     const QFont& captionFont) const
    {
        const QColor value = tile.colorOf ? tile.colorOf(colors) : QColor(Qt::transparent);
        const int pad = 12;
        const QRect chip(cell.left() + pad, cell.top() + pad, cell.width() - pad * 2, 40);
        // A faint stroke keeps near-canvas swatches legible against the card.
        // zh_CN: 细描边让接近底色的色板在卡片上仍可辨识。
        painter.setPen(QPen(colors.strokeSurface, 1.0));
        painter.setBrush(value);
        painter.drawRoundedRect(QRectF(chip).adjusted(0.5, 0.5, -0.5, -0.5),
                                ::CornerRadius::Control, ::CornerRadius::Control);

        const int textLeft = cell.left() + pad;
        const int textWidth = cell.width() - pad * 2;
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics captionMetrics(captionFont);
        int textY = chip.bottom() + 8;

        painter.setFont(titleFont);
        painter.setPen(colors.textPrimary);
        painter.drawText(QRect(textLeft, textY, textWidth, titleMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         titleMetrics.elidedText(tile.title, Qt::ElideRight, textWidth));

        textY += titleMetrics.height() + 2;
        painter.setFont(captionFont);
        painter.setPen(colors.textSecondary);
        const QString hex = value.alpha() == 255
            ? value.name(QColor::HexRgb).toUpper()
            : value.name(QColor::HexArgb).toUpper();
        painter.drawText(QRect(textLeft, textY, textWidth, captionMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, hex);
    }

    void paintIcon(QPainter& painter,
                   const QRect& cell,
                   const Tile& tile,
                   const Colors& colors,
                   const QFont& captionFont) const
    {
        painter.setFont(Typography::Icons::font(18));
        painter.setPen(colors.textPrimary);
        painter.drawText(QRect(cell.left(), cell.top() + 6, cell.width(), 22),
                         Qt::AlignHCenter | Qt::AlignVCenter, tile.glyph);

        const int textPad = 3;
        const QFontMetrics captionMetrics(captionFont);
        painter.setFont(captionFont);
        painter.setPen(colors.textSecondary);
        const QRect nameRect(cell.left() + textPad, cell.top() + 6 + 22,
                             cell.width() - textPad * 2, captionMetrics.height());
        painter.drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter,
                         captionMetrics.elidedText(tile.title, Qt::ElideRight,
                                                   nameRect.width()));
    }

    Kind m_kind;
    QVector<Tile> m_tiles;
    int m_lastColumns = 0;
    int m_hoveredIndex = -1;
};

// ---------------------------------------------------------------------------
// Geometry: corner-radius token tiles.
// zh_CN: 几何：圆角 token 示例方块。
// ---------------------------------------------------------------------------
class RadiusCard : public QWidget, public fluent::FluentElement {
public:
    explicit RadiusCard(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_items = {
            {QStringLiteral("None"), 0},
            {QStringLiteral("Control"), 4},
            {QStringLiteral("Overlay"), 8},
        };
    }

    QSize sizeHint() const override { return QSize(480, kHeight); }
    QSize minimumSizeHint() const override { return QSize(0, kHeight); }
    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const Colors colors = themeColors();
        paintCardSurface(painter, QRectF(rect()), colors);

        const int cols = m_items.size();
        const int colWidth = (width() - kCardPadding * 2) / cols;
        const QFont titleFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
        const QFont captionFont = themeFont(Typography::FontRole::Caption).toQFont();
        const int tile = 88;

        for (int i = 0; i < cols; ++i) {
            const int cx = kCardPadding + i * colWidth + colWidth / 2;
            const QRect tileRect(cx - tile / 2, kCardPadding, tile, tile);
            painter.setPen(QPen(colors.strokeStrong, 1.0));
            painter.setBrush(colors.subtleSecondary);
            painter.drawRoundedRect(QRectF(tileRect).adjusted(0.5, 0.5, -0.5, -0.5),
                                    m_items.at(i).radius, m_items.at(i).radius);

            painter.setFont(titleFont);
            painter.setPen(colors.textPrimary);
            painter.drawText(QRect(kCardPadding + i * colWidth, tileRect.bottom() + 12,
                                   colWidth, 20),
                             Qt::AlignHCenter, m_items.at(i).name);
            painter.setFont(captionFont);
            painter.setPen(colors.textSecondary);
            painter.drawText(QRect(kCardPadding + i * colWidth, tileRect.bottom() + 34,
                                   colWidth, 16),
                             Qt::AlignHCenter,
                             QStringLiteral("%1px").arg(m_items.at(i).radius));
        }
    }

private:
    struct Item {
        QString name;
        int radius;
    };
    static constexpr int kHeight = 88 + 12 + 20 + 4 + 16 + kCardPadding * 2;
    QVector<Item> m_items;
};

// ---------------------------------------------------------------------------
// Token measurements: exact logical-pixel values with proportional bars.
// zh_CN: 尺寸 token：显示精确逻辑像素值与同比例长条。
// ---------------------------------------------------------------------------
class TokenMeasureCard : public QWidget, public fluent::FluentElement {
public:
    struct Row {
        QString name;
        QString qualifiedName;
        int value;
        QString usage;
    };

    explicit TokenMeasureCard(const QVector<Row>& rows, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_rows(rows)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override { return QSize(560, m_rows.size() * kRow + kVPad * 2); }
    QSize minimumSizeHint() const override { return QSize(0, sizeHint().height()); }
    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const Colors colors = themeColors();
        paintCardSurface(painter, QRectF(rect()), colors);

        const QFont bodyFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
        const QFont captionFont = themeFont(Typography::FontRole::Caption).toQFont();
        const int nameCol = qMin(230, qMax(140, width() / 3));
        const int valueCol = 56;
        const int barLeft = kCardPadding + nameCol;
        const int usageLeft = barLeft + 112;
        const int usageWidth = qMax(0, width() - usageLeft - kCardPadding);

        int y = kVPad;
        for (int index = 0; index < m_rows.size(); ++index) {
            const Row& row = m_rows.at(index);
            painter.setFont(bodyFont);
            painter.setPen(colors.textPrimary);
            painter.drawText(QRect(kCardPadding, y + 3, nameCol - 8, 20),
                             Qt::AlignLeft | Qt::AlignVCenter, row.name);

            painter.setFont(captionFont);
            painter.setPen(colors.textSecondary);
            painter.drawText(QRect(kCardPadding, y + 23, nameCol - 8, 18),
                             Qt::AlignLeft | Qt::AlignVCenter, row.qualifiedName);

            painter.drawText(QRect(barLeft, y + 3, valueCol, 20),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QStringLiteral("%1 px").arg(row.value));

            // Literal scale: the bar is exactly row.value logical pixels wide, so 4px really is
            // a 4px sliver and 48px is 12x longer. zh_CN: 真实比例：长条宽度恰为 row.value 个逻辑像素，
            // 所以 4px 就是一条 4px 的细丝，48px 是它的 12 倍长。
            const QRect bar(barLeft, y + 29, qMax(1, row.value), 8);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.accentDefault);
            painter.drawRoundedRect(bar, ::CornerRadius::Control, ::CornerRadius::Control);

            if (usageWidth > 0) {
                painter.setFont(captionFont);
                painter.setPen(colors.textSecondary);
                const QFontMetrics metrics(captionFont);
                painter.drawText(QRect(usageLeft, y, usageWidth, kRow),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 metrics.elidedText(row.usage, Qt::ElideRight, usageWidth));
            }

            y += kRow;
            if (index != m_rows.size() - 1) {
                painter.setPen(QPen(colors.strokeDivider, 1.0));
                painter.drawLine(kCardPadding, y, width() - kCardPadding, y);
            }
        }
    }

private:
    static constexpr int kRow = 48;
    static constexpr int kVPad = 12;
    QVector<Row> m_rows;
};

// ---------------------------------------------------------------------------
// QML+: a rounded surface that hosts live controls for a binding/state/anchor demo.
// zh_CN: QML+：承载实时控件的圆角卡片，用于绑定/状态/锚点演示。
// ---------------------------------------------------------------------------
class QmlPlusDemoCard : public QWidget, public fluent::FluentElement {
public:
    explicit QmlPlusDemoCard(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_body = new QVBoxLayout(this);
        m_body->setContentsMargins(kCardPadding, kCardPadding, kCardPadding, kCardPadding);
        m_body->setSpacing(12);
    }

    QVBoxLayout* body() const { return m_body; }
    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        paintCardSurface(painter, QRectF(rect()), themeColors());
    }

private:
    QVBoxLayout* m_body = nullptr;
};

} // namespace

GalleryFoundationTopicPage::GalleryFoundationTopicPage(
    const GalleryContentEntry& entry,
    const GalleryNavigationViewModel& navigationViewModel,
    QWidget* parent)
    : GalleryContentPage(entry.routeId,
                         resolveTitle(entry, navigationViewModel),
                         entry.description,
                         parent)
{
    setObjectName(QStringLiteral("galleryFoundationTopicPage"));

    if (entry.routeId == QStringLiteral("foundation-qmlplus"))
        buildQmlPlus();
    else if (entry.routeId == QStringLiteral("foundation-typography"))
        buildTypography();
    else if (entry.routeId == QStringLiteral("foundation-color"))
        buildColor();
    else if (entry.routeId == QStringLiteral("foundation-iconography"))
        buildIconography();
    else if (entry.routeId == QStringLiteral("foundation-geometry"))
        buildGeometry();
    else if (entry.routeId == QStringLiteral("foundation-spacing"))
        buildSpacing();

    LOG_DEBUG(QStringLiteral("GalleryFoundationTopicPage created routeId=%1").arg(entry.routeId));
}

void GalleryFoundationTopicPage::buildQmlPlus()
{
    using fluent::basicinput::Button;
    using fluent::basicinput::Slider;
    using fluent::basicinput::ToggleSwitch;
    using fluent::status_info::ProgressBar;
    using fluent::textfields::Label;

    // --- Property binding: source.value drives target.value with no manual glue. ---
    // zh_CN: 属性绑定：源属性变化自动驱动目标属性，无需手写同步代码。
    addSectionHeader(QStringLiteral("Property binding"));
    {
        auto* card = new QmlPlusDemoCard(this);

        card->body()->addWidget(createTrackedLabel(
            QStringLiteral("One-way · PropertyBinder::bind(slider, \"value\", bar, \"value\")"),
            Typography::FontRole::Caption, TextRole::Secondary));

        auto* sliderRow = new QWidget(card);
        auto* sliderLayout = new QHBoxLayout(sliderRow);
        sliderLayout->setContentsMargins(0, 0, 0, 0);
        sliderLayout->setSpacing(12);
        auto* slider = new Slider(Qt::Horizontal, sliderRow);
        slider->setRange(0, 100);
        slider->setValue(40);
        auto* valueLabel = new Label(QStringLiteral("40%"), sliderRow);
        valueLabel->setFluentTypography(Typography::FontRole::BodyStrong);
        valueLabel->setMinimumWidth(48);
        trackLabelColor(valueLabel, TextRole::Primary);
        sliderLayout->addWidget(slider, 1);
        sliderLayout->addWidget(valueLabel, 0);
        card->body()->addWidget(sliderRow);

        auto* bar = new ProgressBar(card);
        bar->setRange(0, 100);
        bar->setValue(40);
        card->body()->addWidget(bar);

        fluent::PropertyBinder::bind(slider, "value", bar, "value", fluent::PropertyBinder::OneWay);
        connect(slider, &QSlider::valueChanged, valueLabel, [valueLabel](int v) {
            valueLabel->setText(QStringLiteral("%1%").arg(v));
        });

        card->body()->addSpacing(4);
        card->body()->addWidget(createTrackedLabel(
            QStringLiteral("Two-way · each switch's isOn mirrors the other"),
            Typography::FontRole::Caption, TextRole::Secondary));

        auto* switchRow = new QWidget(card);
        auto* switchLayout = new QHBoxLayout(switchRow);
        switchLayout->setContentsMargins(0, 0, 0, 0);
        switchLayout->setSpacing(24);
        auto* swA = new ToggleSwitch(switchRow);
        swA->setOnContent(QStringLiteral("On"));
        swA->setOffContent(QStringLiteral("Off"));
        auto* swB = new ToggleSwitch(switchRow);
        swB->setOnContent(QStringLiteral("On"));
        swB->setOffContent(QStringLiteral("Off"));
        switchLayout->addWidget(swA, 0);
        switchLayout->addWidget(swB, 0);
        switchLayout->addStretch(1);
        card->body()->addWidget(switchRow);

        fluent::PropertyBinder::bind(swA, "isOn", swB, "isOn", fluent::PropertyBinder::TwoWay);

        addContentWidget(card);
        addContentWidget(new GalleryCodeBlock(QStringLiteral(
            "auto* slider = new Slider(Qt::Horizontal, this);\n"
            "slider->setRange(0, 100);\n"
            "auto* bar = new ProgressBar(this);\n"
            "bar->setRange(0, 100);\n"
            "\n"
            "// One-way: slider.value drives bar.value, no manual glue.\n"
            "PropertyBinder::bind(slider, \"value\", bar, \"value\",\n"
            "                     PropertyBinder::OneWay);\n"
            "\n"
            "// Two-way: each switch's isOn mirrors the other.\n"
            "PropertyBinder::bind(switchA, \"isOn\", switchB, \"isOn\",\n"
            "                     PropertyBinder::TwoWay);"), this));
    }

    // --- States: one named state rewrites a bundle of properties at once. ---
    // zh_CN: 状态：一个具名 state 一次性改写一组属性。
    addSectionHeader(QStringLiteral("States"));
    {
        auto* card = new QmlPlusDemoCard(this);

        card->body()->addWidget(createTrackedLabel(
            QStringLiteral("setState(\"active\") applies a named bundle of property changes."),
            Typography::FontRole::Caption, TextRole::Secondary));

        auto* stateLabel = new Label(QStringLiteral("Idle"), card);
        stateLabel->setFluentTypography(Typography::FontRole::BodyLargeStrong);
        trackLabelColor(stateLabel, TextRole::Primary);
        card->body()->addWidget(stateLabel);

        auto* stateBar = new ProgressBar(card);
        stateBar->setRange(0, 100);
        stateBar->setValue(20);
        card->body()->addWidget(stateBar);

        auto* toggle = new ToggleSwitch(card);
        toggle->setOnContent(QStringLiteral("Active"));
        toggle->setOffContent(QStringLiteral("Idle"));
        card->body()->addWidget(toggle);

        fluent::QMLState active;
        active.name = QStringLiteral("active");
        fluent::PropertyChange textChange;
        textChange.target = stateLabel;
        textChange.propertyName = "text";
        textChange.value = QStringLiteral("Active — one state rewrote text + value");
        fluent::PropertyChange valueChange;
        valueChange.target = stateBar;
        valueChange.propertyName = "value";
        valueChange.value = 90.0;
        active.changes = {textChange, valueChange};
        addState(active);

        connect(toggle, &ToggleSwitch::toggled, this, [this](bool on) {
            setState(on ? QStringLiteral("active") : QString());
        });

        addContentWidget(card);
        addContentWidget(new GalleryCodeBlock(QStringLiteral(
            "QMLState active;\n"
            "active.name = \"active\";\n"
            "active.changes = {\n"
            "    { label, \"text\",  \"Active - one state, many props\" },\n"
            "    { bar,   \"value\", 90.0 },\n"
            "};\n"
            "addState(active);\n"
            "\n"
            "// Flip the whole bundle on/off with one call.\n"
            "connect(toggle, &ToggleSwitch::toggled, this, [this](bool on) {\n"
            "    setState(on ? \"active\" : QString());\n"
            "});"), this));
    }

    // --- Anchors: AnchorLayout pins children by edge, QML-style, on plain QWidgets. ---
    // zh_CN: 锚点：AnchorLayout 以 QML 风格按边缘定位子控件，作用于普通 QWidget。
    addSectionHeader(QStringLiteral("Anchors"));
    {
        auto* card = new QmlPlusDemoCard(this);

        card->body()->addWidget(createTrackedLabel(
            QStringLiteral("AnchorLayout pins children by edge — resize the window and watch them track."),
            Typography::FontRole::Caption, TextRole::Secondary));

        auto* box = new QWidget(card);
        box->setAttribute(Qt::WA_StyledBackground, true);
        box->setMinimumHeight(140);
        box->setStyleSheet(QStringLiteral(
            "border: 1px dashed rgba(128,128,128,0.45); border-radius: 6px;"));
        auto* anchorLayout = new fluent::AnchorLayout(box);

        auto* centered = new Button(QStringLiteral("Centered"), box);
        centered->setFluentStyle(Button::Accent);
        fluent::AnchorLayout::Anchors centerAnchors;
        centerAnchors.horizontalCenter =
            fluent::AnchorLayout::Anchor(box, fluent::AnchorLayout::Edge::HCenter);
        centerAnchors.verticalCenter =
            fluent::AnchorLayout::Anchor(box, fluent::AnchorLayout::Edge::VCenter);
        anchorLayout->addAnchoredWidget(centered, centerAnchors);

        auto* topRight = new Button(QStringLiteral("Top-right"), box);
        topRight->setFluentStyle(Button::Standard);
        fluent::AnchorLayout::Anchors topRightAnchors;
        topRightAnchors.right =
            fluent::AnchorLayout::Anchor(box, fluent::AnchorLayout::Edge::Right, -12);
        topRightAnchors.top =
            fluent::AnchorLayout::Anchor(box, fluent::AnchorLayout::Edge::Top, 12);
        anchorLayout->addAnchoredWidget(topRight, topRightAnchors);

        card->body()->addWidget(box);
        addContentWidget(card);
        addContentWidget(new GalleryCodeBlock(QStringLiteral(
            "auto* box = new QWidget(this);\n"
            "auto* layout = new AnchorLayout(box);\n"
            "\n"
            "// Center a child by its center lines.\n"
            "AnchorLayout::Anchors center;\n"
            "center.horizontalCenter = { box, AnchorLayout::Edge::HCenter };\n"
            "center.verticalCenter   = { box, AnchorLayout::Edge::VCenter };\n"
            "layout->addAnchoredWidget(centered, center);\n"
            "\n"
            "// Pin a child 12px in from the top-right corner.\n"
            "AnchorLayout::Anchors topRight;\n"
            "topRight.right = { box, AnchorLayout::Edge::Right, -12 };\n"
            "topRight.top   = { box, AnchorLayout::Edge::Top,    12 };\n"
            "layout->addAnchoredWidget(pinned, topRight);"), this));
    }

    // Recolor the freshly added tracked caption labels (secondary) — applyPalette only runs
    // in the base ctor and on theme changes, before these existed.
    // zh_CN: 给刚加入的跟踪标题（次要色）重新上色——applyPalette 只在基类构造和主题切换时运行，那时它们还不存在。
    onThemeUpdated();
}

void GalleryFoundationTopicPage::buildTypography()
{
    addSectionHeader(QStringLiteral("Type ramp"));
    addContentWidget(new TypeRampCard(this));

    addSectionHeader(QStringLiteral("Use semantic roles"));
    addContentWidget(new GalleryCodeBlock(QStringLiteral(
        "auto* title = new fluent::textfields::Label(\"Settings\", this);\n"
        "title->setFluentTypography(Typography::FontRole::Title);\n"
        "\n"
        "// All roles resolve to bundled, static, hinted faces.\n"
        "const QFont body = Typography::Styles::Body.toQFont();\n"
        "const QFont heading = Typography::Styles::Title.toQFont();"), this));
}

void GalleryFoundationTopicPage::buildColor()
{
    struct Group {
        QString header;
        QVector<FoundationTileGrid::Tile> tiles;
    };
    auto swatch = [](const QString& name, std::function<QColor(const Colors&)> of) {
        return FoundationTileGrid::Tile{name, QString(), std::move(of)};
    };

    const QVector<Group> groups = {
        {QStringLiteral("Text"), {
            swatch(QStringLiteral("textPrimary"), [](const Colors& c) { return c.textPrimary; }),
            swatch(QStringLiteral("textSecondary"), [](const Colors& c) { return c.textSecondary; }),
            swatch(QStringLiteral("textTertiary"), [](const Colors& c) { return c.textTertiary; }),
            swatch(QStringLiteral("textDisabled"), [](const Colors& c) { return c.textDisabled; }),
            swatch(QStringLiteral("textOnAccent"), [](const Colors& c) { return c.textOnAccent; }),
            swatch(QStringLiteral("textAccentPrimary"), [](const Colors& c) { return c.textAccentPrimary; }),
        }},
        {QStringLiteral("Fill & accent"), {
            swatch(QStringLiteral("accentDefault"), [](const Colors& c) { return c.accentDefault; }),
            swatch(QStringLiteral("accentSecondary"), [](const Colors& c) { return c.accentSecondary; }),
            swatch(QStringLiteral("accentTertiary"), [](const Colors& c) { return c.accentTertiary; }),
            swatch(QStringLiteral("accentDisabled"), [](const Colors& c) { return c.accentDisabled; }),
            swatch(QStringLiteral("controlDefault"), [](const Colors& c) { return c.controlDefault; }),
            swatch(QStringLiteral("controlSecondary"), [](const Colors& c) { return c.controlSecondary; }),
            swatch(QStringLiteral("controlTertiary"), [](const Colors& c) { return c.controlTertiary; }),
            swatch(QStringLiteral("controlDisabled"), [](const Colors& c) { return c.controlDisabled; }),
            swatch(QStringLiteral("controlAltSecondary"), [](const Colors& c) { return c.controlAltSecondary; }),
            swatch(QStringLiteral("controlAltTertiary"), [](const Colors& c) { return c.controlAltTertiary; }),
            swatch(QStringLiteral("subtleTransparent"), [](const Colors& c) { return c.subtleTransparent; }),
            swatch(QStringLiteral("subtleSecondary"), [](const Colors& c) { return c.subtleSecondary; }),
            swatch(QStringLiteral("subtleTertiary"), [](const Colors& c) { return c.subtleTertiary; }),
        }},
        {QStringLiteral("Background & layers"), {
            swatch(QStringLiteral("bgCanvas"), [](const Colors& c) { return c.bgCanvas; }),
            swatch(QStringLiteral("bgLayer"), [](const Colors& c) { return c.bgLayer; }),
            swatch(QStringLiteral("bgLayerAlt"), [](const Colors& c) { return c.bgLayerAlt; }),
            swatch(QStringLiteral("bgSolid"), [](const Colors& c) { return c.bgSolid; }),
            swatch(QStringLiteral("grey10"), [](const Colors& c) { return c.grey10; }),
            swatch(QStringLiteral("grey20"), [](const Colors& c) { return c.grey20; }),
            swatch(QStringLiteral("grey30"), [](const Colors& c) { return c.grey30; }),
            swatch(QStringLiteral("grey40"), [](const Colors& c) { return c.grey40; }),
            swatch(QStringLiteral("grey50"), [](const Colors& c) { return c.grey50; }),
            swatch(QStringLiteral("grey60"), [](const Colors& c) { return c.grey60; }),
            swatch(QStringLiteral("grey90"), [](const Colors& c) { return c.grey90; }),
            swatch(QStringLiteral("grey130"), [](const Colors& c) { return c.grey130; }),
            swatch(QStringLiteral("grey160"), [](const Colors& c) { return c.grey160; }),
            swatch(QStringLiteral("grey190"), [](const Colors& c) { return c.grey190; }),
        }},
        {QStringLiteral("Stroke"), {
            swatch(QStringLiteral("strokeDefault"), [](const Colors& c) { return c.strokeDefault; }),
            swatch(QStringLiteral("strokeSecondary"), [](const Colors& c) { return c.strokeSecondary; }),
            swatch(QStringLiteral("strokeStrong"), [](const Colors& c) { return c.strokeStrong; }),
            swatch(QStringLiteral("strokeCard"), [](const Colors& c) { return c.strokeCard; }),
            swatch(QStringLiteral("strokeDivider"), [](const Colors& c) { return c.strokeDivider; }),
            swatch(QStringLiteral("strokeSurface"), [](const Colors& c) { return c.strokeSurface; }),
            swatch(QStringLiteral("strokeFocusOuter"), [](const Colors& c) { return c.strokeFocusOuter; }),
            swatch(QStringLiteral("strokeFocusInner"), [](const Colors& c) { return c.strokeFocusInner; }),
        }},
        {QStringLiteral("System"), {
            swatch(QStringLiteral("systemCritical"), [](const Colors& c) { return c.systemCritical; }),
            swatch(QStringLiteral("systemCriticalBg"), [](const Colors& c) { return c.systemCriticalBg; }),
            swatch(QStringLiteral("systemCaution"), [](const Colors& c) { return c.systemCaution; }),
            swatch(QStringLiteral("systemCautionBg"), [](const Colors& c) { return c.systemCautionBg; }),
            swatch(QStringLiteral("systemInfo"), [](const Colors& c) { return c.systemInfo; }),
            swatch(QStringLiteral("systemInfoBg"), [](const Colors& c) { return c.systemInfoBg; }),
            swatch(QStringLiteral("systemSuccess"), [](const Colors& c) { return c.systemSuccess; }),
            swatch(QStringLiteral("systemSuccessBg"), [](const Colors& c) { return c.systemSuccessBg; }),
        }},
        {QStringLiteral("Charts"), {
            swatch(QStringLiteral("chart1"), [](const Colors& c) { return c.charts.value(0); }),
            swatch(QStringLiteral("chart2"), [](const Colors& c) { return c.charts.value(1); }),
            swatch(QStringLiteral("chart3"), [](const Colors& c) { return c.charts.value(2); }),
            swatch(QStringLiteral("chart4"), [](const Colors& c) { return c.charts.value(3); }),
            swatch(QStringLiteral("chart5"), [](const Colors& c) { return c.charts.value(4); }),
            swatch(QStringLiteral("chart6"), [](const Colors& c) { return c.charts.value(5); }),
            swatch(QStringLiteral("chart7"), [](const Colors& c) { return c.charts.value(6); }),
            swatch(QStringLiteral("chart8"), [](const Colors& c) { return c.charts.value(7); }),
            swatch(QStringLiteral("chart9"), [](const Colors& c) { return c.charts.value(8); }),
            swatch(QStringLiteral("chart10"), [](const Colors& c) { return c.charts.value(9); }),
            swatch(QStringLiteral("chart11"), [](const Colors& c) { return c.charts.value(10); }),
            swatch(QStringLiteral("chart12"), [](const Colors& c) { return c.charts.value(11); }),
        }},
    };

    for (const Group& group : groups) {
        addSectionHeader(group.header);
        auto* grid = new FoundationTileGrid(FoundationTileGrid::Swatch, this);
        grid->setTiles(group.tiles);
        addContentWidget(grid);
    }
}

void GalleryFoundationTopicPage::buildIconography()
{
    addSectionHeader(QStringLiteral("Complete Regular catalog"));
    addContentWidget(new GalleryIconBrowser(this));
}

void GalleryFoundationTopicPage::buildGeometry()
{
    addSectionHeader(QStringLiteral("Corner radius"));
    addContentWidget(new RadiusCard(this));

    addSectionHeader(QStringLiteral("Stroke widths"));
    const QVector<TokenMeasureCard::Row> strokeRows = {
        {QStringLiteral("Normal"), QStringLiteral("Spacing::Border::Normal"),
         ::Spacing::Border::Normal, QStringLiteral("Default control and surface borders")},
        {QStringLiteral("Focused"), QStringLiteral("Spacing::Border::Focused"),
         ::Spacing::Border::Focused, QStringLiteral("Keyboard focus indicator")},
    };
    addContentWidget(new TokenMeasureCard(strokeRows, this));

    addSectionHeader(QStringLiteral("Use geometry tokens"));
    addContentWidget(new GalleryCodeBlock(QStringLiteral(
        "painter.drawRoundedRect(controlRect,\n"
        "                        CornerRadius::Control,\n"
        "                        CornerRadius::Control);\n"
        "\n"
        "QPen focusPen(colors.strokeFocusOuter, Spacing::Border::Focused);"), this));
}

void GalleryFoundationTopicPage::buildSpacing()
{
    addSectionHeader(QStringLiteral("Spacing scale"));
    const QVector<TokenMeasureCard::Row> scaleRows = {
        {QStringLiteral("XSmall"), QStringLiteral("Spacing::XSmall"), ::Spacing::XSmall,
         QStringLiteral("Icon-to-text and compact inline gaps")},
        {QStringLiteral("Small"), QStringLiteral("Spacing::Small"), ::Spacing::Small,
         QStringLiteral("Related controls")},
        {QStringLiteral("Medium"), QStringLiteral("Spacing::Medium"), ::Spacing::Medium,
         QStringLiteral("Default control padding")},
        {QStringLiteral("Standard"), QStringLiteral("Spacing::Standard"), ::Spacing::Standard,
         QStringLiteral("Card content and grouped regions")},
        {QStringLiteral("Large"), QStringLiteral("Spacing::Large"), ::Spacing::Large,
         QStringLiteral("Sections and dialog content")},
        {QStringLiteral("XLarge"), QStringLiteral("Spacing::XLarge"), ::Spacing::XLarge,
         QStringLiteral("Page-level whitespace")},
        {QStringLiteral("XXLarge"), QStringLiteral("Spacing::XXLarge"), ::Spacing::XXLarge,
         QStringLiteral("Large page separation")},
    };
    addContentWidget(new TokenMeasureCard(scaleRows, this));

    addSectionHeader(QStringLiteral("Component padding"));
    const QVector<TokenMeasureCard::Row> paddingRows = {
        {QStringLiteral("Control horizontal"), QStringLiteral("Spacing::Padding::ControlHorizontal"),
         ::Spacing::Padding::ControlHorizontal, QStringLiteral("Generic controls")},
        {QStringLiteral("Control vertical"), QStringLiteral("Spacing::Padding::ControlVertical"),
         ::Spacing::Padding::ControlVertical, QStringLiteral("Generic controls")},
        {QStringLiteral("ComboBox horizontal"), QStringLiteral("Spacing::Padding::ComboBoxHorizontal"),
         ::Spacing::Padding::ComboBoxHorizontal, QStringLiteral("ComboBox content")},
        {QStringLiteral("ComboBox vertical"), QStringLiteral("Spacing::Padding::ComboBoxVertical"),
         ::Spacing::Padding::ComboBoxVertical, QStringLiteral("ComboBox content")},
        {QStringLiteral("Text field horizontal"), QStringLiteral("Spacing::Padding::TextFieldHorizontal"),
         ::Spacing::Padding::TextFieldHorizontal, QStringLiteral("LineEdit and text inputs")},
        {QStringLiteral("Text field vertical"), QStringLiteral("Spacing::Padding::TextFieldVertical"),
         ::Spacing::Padding::TextFieldVertical, QStringLiteral("LineEdit and text inputs")},
        {QStringLiteral("List item horizontal"), QStringLiteral("Spacing::Padding::ListItemHorizontal"),
         ::Spacing::Padding::ListItemHorizontal, QStringLiteral("Collection rows")},
        {QStringLiteral("List item vertical"), QStringLiteral("Spacing::Padding::ListItemVertical"),
         ::Spacing::Padding::ListItemVertical, QStringLiteral("Collection rows")},
        {QStringLiteral("Card"), QStringLiteral("Spacing::Padding::Card"),
         ::Spacing::Padding::Card, QStringLiteral("Card content")},
        {QStringLiteral("Dialog"), QStringLiteral("Spacing::Padding::Dialog"),
         ::Spacing::Padding::Dialog, QStringLiteral("Dialog content")},
    };
    addContentWidget(new TokenMeasureCard(paddingRows, this));

    addSectionHeader(QStringLiteral("Gaps"));
    const QVector<TokenMeasureCard::Row> gapRows = {
        {QStringLiteral("Tight"), QStringLiteral("Spacing::Gap::Tight"), ::Spacing::Gap::Tight,
         QStringLiteral("Icon and text")},
        {QStringLiteral("Normal"), QStringLiteral("Spacing::Gap::Normal"), ::Spacing::Gap::Normal,
         QStringLiteral("Controls in one group")},
        {QStringLiteral("Loose"), QStringLiteral("Spacing::Gap::Loose"), ::Spacing::Gap::Loose,
         QStringLiteral("Separate control groups")},
        {QStringLiteral("Section"), QStringLiteral("Spacing::Gap::Section"), ::Spacing::Gap::Section,
         QStringLiteral("Page sections")},
    };
    addContentWidget(new TokenMeasureCard(gapRows, this));

    addSectionHeader(QStringLiteral("Control heights"));
    const QVector<TokenMeasureCard::Row> heightRows = {
        {QStringLiteral("Small"), QStringLiteral("Spacing::ControlHeight::Small"),
         ::Spacing::ControlHeight::Small, QStringLiteral("Compact layouts")},
        {QStringLiteral("Standard"), QStringLiteral("Spacing::ControlHeight::Standard"),
         ::Spacing::ControlHeight::Standard, QStringLiteral("Default controls")},
        {QStringLiteral("Large"), QStringLiteral("Spacing::ControlHeight::Large"),
         ::Spacing::ControlHeight::Large, QStringLiteral("Spacious layouts")},
    };
    addContentWidget(new TokenMeasureCard(heightRows, this));
}

} // namespace fluent::gallery

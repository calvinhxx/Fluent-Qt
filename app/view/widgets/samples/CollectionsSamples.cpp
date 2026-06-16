#include "CollectionsSamples.h"

#include <functional>

#include <QEvent>
#include <QItemSelectionModel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/collections/DrawerView.h"
#include "components/collections/FlipView.h"
#include "components/collections/FlowView.h"
#include "components/collections/GridView.h"
#include "components/collections/ListView.h"
#include "components/collections/SplitView.h"
#include "components/collections/StackView.h"
#include "components/collections/TreeView.h"
#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "CollectionSampleDelegates.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::collections::DrawerView;
using fluent::collections::FlipView;
using fluent::collections::FlowView;
using fluent::collections::GridView;
using fluent::collections::ListView;
using fluent::collections::SplitView;
using fluent::collections::StackView;
using fluent::collections::TreeView;
using fluent::textfields::Label;
using samples::glyphPixmap;
using samples::gradientPixmap;
using samples::horizontalGroup;
using samples::initialsAvatar;
using samples::makeSample;
using samples::verticalGroup;

/**
 * @brief Fluent accent palette cycled by the decorated list/grid items.
 * zh_CN: 装饰列表/网格条目循环使用的 Fluent 强调色盘。
 */
const QVector<QColor>& accentPalette()
{
    static const QVector<QColor> palette{
        QColor(0x00, 0x78, 0xD4), QColor(0x03, 0x83, 0x87), QColor(0xCA, 0x50, 0x10),
        QColor(0x87, 0x64, 0xB8), QColor(0xC2, 0x39, 0xB3), QColor(0x49, 0x82, 0x05)};
    return palette;
}

struct FlowPhotoInfo {
    QString title;
    QString subtitle;
    QString seed;
    QColor from;
    QColor to;
    QSize size{160, 118};
};

/**
 * @brief A resizable pane that paints a diagonal gradient with a centered caption.
 * zh_CN: 可缩放的窗格，绘制对角渐变并居中显示标题。
 *
 * Used by SplitView / StackView so their panes read as the same colored "pictures" the
 * FlipView gallery uses, but stretch crisply as the pane resizes (a stretched QPixmap would
 * blur the caption).
 * zh_CN: 供 SplitView / StackView 使用，让窗格呈现与 FlipView 一致的彩色"图片"风格，并随窗格
 * 缩放保持清晰（直接拉伸 QPixmap 会让标题发虚）。
 */
class GradientPane : public QWidget {
public:
    GradientPane(const QString& caption, const QColor& from, const QColor& to,
                 QWidget* parent = nullptr)
        : QWidget(parent)
        , m_caption(caption)
        , m_from(from)
        , m_to(to)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumSize(64, 48);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QLinearGradient gradient(surface.topLeft(), surface.bottomRight());
        gradient.setColorAt(0.0, m_from);
        gradient.setColorAt(1.0, m_to);
        QPainterPath path;
        path.addRoundedRect(surface, 8.0, 8.0);
        painter.fillPath(path, gradient);

        QFont font;
        font.setPixelSize(15);
        font.setWeight(QFont::DemiBold);
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255, 235));
        painter.drawText(surface, Qt::AlignCenter, m_caption);
    }

private:
    QString m_caption;
    QColor m_from;
    QColor m_to;
};

GradientPane* makeGradientPane(QWidget* parent, const QString& caption,
                               const QColor& from, const QColor& to)
{
    return new GradientPane(caption, from, to, parent);
}

/**
 * @brief List model whose rows pair text with a glyph tile icon.
 * zh_CN: 行内文本搭配字形图块图标的列表模型。
 */
QStandardItemModel* makeGlyphListModel(QObject* parent,
                                       const QVector<QPair<QString, QString>>& rows,
                                       int iconSize)
{
    auto* model = new QStandardItemModel(parent);
    int colorIndex = 0;
    for (const auto& row : rows) {
        auto* item = new QStandardItem(row.first);
        item->setEditable(false);
        item->setIcon(glyphPixmap(row.second,
                                  accentPalette().at(colorIndex++ % accentPalette().size()),
                                  iconSize));
        model->appendRow(item);
    }
    return model;
}

class DrawerNavigationItem : public QWidget, public fluent::FluentElement {
public:
    DrawerNavigationItem(const QString& text,
                         const QString& glyph,
                         const QColor& iconColor,
                         bool selected,
                         QWidget* parent = nullptr)
        : QWidget(parent)
        , m_text(text)
        , m_glyph(glyph)
        , m_iconColor(iconColor)
        , m_selected(selected)
    {
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override { return QSize(210, 40); }
    QSize minimumSizeHint() const override { return QSize(160, 36); }

    void setSelected(bool selected)
    {
        if (m_selected == selected)
            return;
        m_selected = selected;
        update();
    }

    bool isSelected() const { return m_selected; }

    void onThemeUpdated() override { update(); }

    std::function<void()> onActivated;

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter) {
            m_hovered = true;
            update();
        } else if (event->type() == QEvent::Leave) {
            m_hovered = false;
            m_pressed = false;
            update();
        }
        return QWidget::event(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const bool activate = m_pressed
            && event->button() == Qt::LeftButton
            && rect().contains(fluentMousePos(event));
        m_pressed = false;
        update();
        if (activate && onActivated)
            onActivated();
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto colors = themeColors();
        const QRectF rowRect = QRectF(rect()).adjusted(0.0, 2.0, 0.0, -2.0);
        if (m_pressed || m_hovered || m_selected) {
            const QColor background = m_pressed ? colors.subtleTertiary : colors.subtleSecondary;
            QPainterPath path;
            path.addRoundedRect(rowRect, 6.0, 6.0);
            painter.fillPath(path, background);
        }

        if (m_selected) {
            const QRectF indicator(rowRect.left() + 5.0, rowRect.center().y() - 7.0, 3.0, 14.0);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.accentDefault);
            painter.drawRoundedRect(indicator, 1.5, 1.5);
        }

        const QRectF iconRect(rowRect.left() + 26.0, rowRect.center().y() - 14.0, 28.0, 28.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_iconColor);
        painter.drawRoundedRect(iconRect, 7.0, 7.0);

        QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
        iconFont.setPixelSize(15);
        painter.setFont(iconFont);
        painter.setPen(Qt::white);
        painter.drawText(iconRect, Qt::AlignCenter, m_glyph);

        QFont textFont = themeFont(m_selected ? Typography::FontRole::BodyStrong
                                              : Typography::FontRole::Body).toQFont();
        painter.setFont(textFont);
        painter.setPen(isEnabled() ? colors.textPrimary : colors.textDisabled);
        const QRectF textRect(iconRect.right() + 14.0,
                              rowRect.top(),
                              rowRect.right() - iconRect.right() - 22.0,
                              rowRect.height());
        painter.drawText(textRect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(m_text, Qt::ElideRight, qRound(textRect.width())));
    }

private:
    QString m_text;
    QString m_glyph;
    QColor m_iconColor;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_pressed = false;
};

class FlowPhotoDelegate : public QStyledItemDelegate {
public:
    explicit FlowPhotoDelegate(fluent::FluentElement* themeHost, QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_themeHost(themeHost)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        const QVariant size = index.data(Qt::SizeHintRole);
        return size.canConvert<QSize>() ? size.toSize() : QSize(160, 118);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (!index.isValid())
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->setRenderHint(QPainter::TextAntialiasing);

        fluent::FluentElement::Colors colors{};
        if (m_themeHost)
            colors = m_themeHost->themeColors();
        const QColor layer = colors.bgLayerAlt.isValid() ? colors.bgLayerAlt : QColor(250, 250, 250);
        const QColor stroke = colors.strokeDefault.isValid() ? colors.strokeDefault : QColor(220, 220, 220);
        const QColor accent = colors.accentDefault.isValid() ? colors.accentDefault : QColor(0, 120, 212);
        const QColor hover = colors.subtleSecondary.isValid() ? colors.subtleSecondary : QColor(0, 0, 0, 18);
        const QColor textOnImage(255, 255, 255, 240);
        const QColor subtitleOnImage(255, 255, 255, 205);

        const QRectF card = QRectF(option.rect).adjusted(2.0, 2.0, -2.0, -2.0);
        const int radius = CornerRadius::Control;
        QPainterPath clip;
        clip.addRoundedRect(card, radius, radius);
        painter->fillPath(clip, layer);

        const QVariant imageData = index.data(PhotoImageRole);
        const QPixmap pixmap = imageData.canConvert<QPixmap>() ? imageData.value<QPixmap>() : QPixmap();
        if (!pixmap.isNull()) {
            painter->setClipPath(clip);
            drawCoverPixmap(painter, card, pixmap);
            if (option.state & QStyle::State_MouseOver)
                painter->fillRect(card, QColor(255, 255, 255, 24));

            const QRectF labelBar(card.left(), card.bottom() - 44.0, card.width(), 44.0);
            QLinearGradient scrim(labelBar.topLeft(), labelBar.bottomLeft());
            scrim.setColorAt(0.0, QColor(0, 0, 0, 20));
            scrim.setColorAt(1.0, QColor(0, 0, 0, 150));
            painter->fillRect(labelBar, scrim);

            const QString title = index.data(Qt::DisplayRole).toString();
            const QString subtitle = index.data(PhotoSubtitleRole).toString();
            QFont titleFont = option.font;
            titleFont.setWeight(QFont::DemiBold);
            painter->setFont(titleFont);
            painter->setPen(textOnImage);
            painter->drawText(labelBar.adjusted(10, 4, -10, -21),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              painter->fontMetrics().elidedText(title, Qt::ElideRight,
                                                                 qRound(labelBar.width()) - 20));

            QFont subtitleFont = option.font;
            subtitleFont.setPixelSize(qMax(10, subtitleFont.pixelSize() - 2));
            painter->setFont(subtitleFont);
            painter->setPen(subtitleOnImage);
            painter->drawText(labelBar.adjusted(10, 22, -10, -4),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              painter->fontMetrics().elidedText(subtitle, Qt::ElideRight,
                                                                 qRound(labelBar.width()) - 20));
            painter->setClipping(false);
        }

        painter->setPen(QPen(option.state & QStyle::State_Selected ? accent : stroke,
                             option.state & QStyle::State_Selected ? 2.0 : 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(clip);

        if ((option.state & QStyle::State_MouseOver) && !(option.state & QStyle::State_Selected)) {
            painter->setPen(QPen(hover, 1.0));
            painter->drawPath(clip);
        }

        painter->restore();
    }

private:
    static void drawCoverPixmap(QPainter* painter, const QRectF& target, const QPixmap& pixmap)
    {
        const QSizeF sourceSize = pixmap.size() / pixmap.devicePixelRatio();
        if (sourceSize.isEmpty())
            return;

        const qreal scale = qMax(target.width() / sourceSize.width(),
                                 target.height() / sourceSize.height());
        const QSizeF visibleSize(target.width() / scale, target.height() / scale);
        const QRectF source((sourceSize.width() - visibleSize.width()) / 2.0,
                            (sourceSize.height() - visibleSize.height()) / 2.0,
                            visibleSize.width(),
                            visibleSize.height());
        painter->drawPixmap(target, pixmap, source);
    }

    fluent::FluentElement* m_themeHost = nullptr;
};

void loadFlowNetworkImage(QStandardItemModel* model,
                          int row,
                          const QUrl& url,
                          QNetworkAccessManager* manager)
{
    if (!model || !manager)
        return;

    QPointer<QStandardItemModel> modelGuard(model);
    const QPersistentModelIndex index(model->index(row, 0));
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = manager->get(request);
    QObject::connect(reply, &QNetworkReply::finished, manager, [modelGuard, index, reply]() {
        reply->deleteLater();
        if (!modelGuard || !index.isValid() || reply->error() != QNetworkReply::NoError)
            return;

        QPixmap pixmap;
        if (!pixmap.loadFromData(reply->readAll()) || pixmap.isNull())
            return;
        modelGuard->setData(index, pixmap, PhotoImageRole);
    });
}

QStandardItemModel* makeFlowPhotoModel(QObject* parent, const QVector<FlowPhotoInfo>& photos)
{
    auto* model = new QStandardItemModel(parent);
    for (const FlowPhotoInfo& photo : photos) {
        auto* item = new QStandardItem(photo.title);
        item->setEditable(false);
        item->setData(photo.subtitle, PhotoSubtitleRole);
        item->setData(photo.size, Qt::SizeHintRole);
        item->setData(gradientPixmap(QSize(photo.size.width() * 2, photo.size.height() * 2),
                                     photo.from,
                                     photo.to,
                                     photo.title),
                      PhotoImageRole);
        model->appendRow(item);
    }
    return model;
}

void loadFlowNetworkImages(QStandardItemModel* model,
                           const QVector<FlowPhotoInfo>& photos,
                           QNetworkAccessManager* manager)
{
    for (int row = 0; row < photos.size(); ++row) {
        loadFlowNetworkImage(
            model,
            row,
            QUrl(QStringLiteral("https://picsum.photos/seed/%1/%2/%3")
                     .arg(photos.at(row).seed)
                     .arg(photos.at(row).size.width() * 2)
                     .arg(photos.at(row).size.height() * 2)),
            manager);
    }
}

QVector<GallerySample> drawerViewSamples()
{
    return {
        makeSample(QStringLiteral("drawer-view-basic"),
                   QStringLiteral("Right-edge drawer"),
                   QStringLiteral("The drawer slides over the hosting surface and dismisses on outside click."),
                   QStringLiteral("auto* drawer = new DrawerView(host);\n"
                                  "drawer->setEdge(DrawerView::DrawerEdge::Right);\n"
                                  "drawer->setDrawerLength(260);\n"
                                  "drawer->setContentWidget(settingsPanel);\n"
                                  "drawer->open();"),
                   [](QWidget* parent) {
                       auto* host = new QWidget(parent);
                       host->setFixedSize(420, 240);
                       host->setStyleSheet(QStringLiteral(
                           "background: rgba(128, 128, 128, 18); border-radius: 8px;"));

                       auto* drawer = new DrawerView(host);
                       drawer->setEdge(DrawerView::DrawerEdge::Right);
                       drawer->setDrawerLength(260);

                       auto* drawerContent = new QWidget;
                       auto* drawerLayout = new QVBoxLayout(drawerContent);
                       drawerLayout->setContentsMargins(14, 18, 14, 18);
                       drawerLayout->setSpacing(6);
                       auto* title = new Label(QStringLiteral("Navigation"), drawerContent);
                       title->setFluentTypography(Typography::FontRole::BodyStrong);
                       drawerLayout->addWidget(title);

                       QVector<DrawerNavigationItem*> navItems;
                       const QVector<QPair<QString, QString>> rows{
                           {QStringLiteral("Home"), Typography::Icons::Home},
                           {QStringLiteral("Music"), Typography::Icons::Music},
                           {QStringLiteral("Downloads"), Typography::Icons::Download},
                           {QStringLiteral("Settings"), Typography::Icons::Settings}};
                       for (int i = 0; i < rows.size(); ++i) {
                           auto* item = new DrawerNavigationItem(rows.at(i).first,
                                                                 rows.at(i).second,
                                                                 accentPalette().at(i % accentPalette().size()),
                                                                 i == 0,
                                                                 drawerContent);
                           navItems.append(item);
                           drawerLayout->addWidget(item);
                       }
                       for (DrawerNavigationItem* item : navItems) {
                           item->onActivated = [navItems, item]() {
                               for (DrawerNavigationItem* navItem : navItems)
                                   navItem->setSelected(navItem == item);
                           };
                       }
                       drawerLayout->addStretch(1);
                       drawer->setContentWidget(drawerContent);

                       auto* openButton = new Button(QStringLiteral("Open drawer"), host);
                       openButton->setFluentStyle(Button::Accent);
                       openButton->move(18, 18);
                       QObject::connect(openButton, &Button::clicked,
                                        drawer, [drawer]() { drawer->open(); });
                       return host;
                   })
    };
}

QVector<GallerySample> flipViewSamples()
{
    return {
        makeSample(QStringLiteral("flip-view-basic"),
                   QStringLiteral("FlipView photo gallery"),
                   QStringLiteral("Use the arrows or swipe to flip between pictures."),
                   QStringLiteral("auto* flipView = new FlipView(this);\n"
                                  "flipView->addPage(sunrisePhoto);\n"
                                  "flipView->addPage(oceanPhoto);\n"
                                  "flipView->addPage(forestPhoto);\n"
                                  "flipView->setShowPageIndicator(true);"),
                   [](QWidget* parent) {
                       auto* flipView = new FlipView(parent);
                       flipView->setFixedSize(420, 220);
                       flipView->setShowPageIndicator(true);
                       const QSize pageSize(420, 220);
                       struct Photo { QString caption; QColor from; QColor to; };
                       const QVector<Photo> photos{
                           {QStringLiteral("Sunrise"), QColor(0xF7, 0x97, 0x5B), QColor(0xF2, 0xC9, 0x4C)},
                           {QStringLiteral("Ocean"), QColor(0x1E, 0x6F, 0xD9), QColor(0x6F, 0xD1, 0xF2)},
                           {QStringLiteral("Forest"), QColor(0x2F, 0x9E, 0x44), QColor(0xA9, 0xE3, 0x4B)}};
                       for (const Photo& photo : photos) {
                           auto* page = new QLabel(flipView);
                           page->setPixmap(gradientPixmap(pageSize, photo.from, photo.to,
                                                          photo.caption));
                           page->setAlignment(Qt::AlignCenter);
                           flipView->addPage(page);
                       }
                       return flipView;
                   })
    };
}

QVector<GallerySample> flowViewSamples()
{
    return {
        makeSample(QStringLiteral("flow-view-basic"),
                   QStringLiteral("Network photo flow"),
                   QStringLiteral("Images load asynchronously while the flow layout wraps cards across rows."),
                   QStringLiteral("auto* flowView = new FlowView(this);\n"
                                  "flowView->setDefaultItemSize(QSize(160, 118));\n"
                                  "flowView->setItemDelegate(photoDelegate);\n"
                                  "flowView->setModel(model);\n"
                                  "flowView->setSelectionMode(FlowView::FlowSelectionMode::Single);"),
                   [](QWidget* parent) {
                       auto* flowView = new FlowView(parent);
                       flowView->setFixedSize(540, 282);
                       flowView->setSelectionMode(FlowView::FlowSelectionMode::Single);
                       flowView->setDefaultItemSize(QSize(160, 118));
                       flowView->setMinimumItemSize(QSize(140, 100));
                       flowView->setMaximumItemSize(QSize(180, 128));
                       flowView->setHorizontalSpacing(10);
                       flowView->setVerticalSpacing(10);
                       flowView->setContentMargins(QMargins(8, 8, 8, 8));
                       flowView->setItemDelegate(new FlowPhotoDelegate(
                           static_cast<fluent::FluentElement*>(flowView), flowView));

                       const QVector<FlowPhotoInfo> photos{
                           {QStringLiteral("Atrium"), QStringLiteral("architecture"), QStringLiteral("atrium-glass"), QColor(0x37, 0x8B, 0xC4), QColor(0x9A, 0xD9, 0xEA), QSize(160, 118)},
                           {QStringLiteral("Harbor"), QStringLiteral("travel"), QStringLiteral("harbor-blue"), QColor(0x1E, 0x6F, 0xD9), QColor(0x67, 0xD0, 0xD6), QSize(160, 118)},
                           {QStringLiteral("Canyon"), QStringLiteral("landscape"), QStringLiteral("warm-canyon"), QColor(0xC2, 0x5B, 0x2B), QColor(0xF2, 0xB8, 0x4B), QSize(160, 118)},
                           {QStringLiteral("Studio"), QStringLiteral("workspace"), QStringLiteral("quiet-studio"), QColor(0x7A, 0x5F, 0xC9), QColor(0xD7, 0x94, 0xE6), QSize(160, 118)},
                           {QStringLiteral("Garden"), QStringLiteral("nature"), QStringLiteral("green-garden"), QColor(0x2F, 0x9E, 0x44), QColor(0xA9, 0xE3, 0x4B), QSize(160, 118)},
                           {QStringLiteral("Dawn"), QStringLiteral("morning"), QStringLiteral("soft-dawn"), QColor(0xD9, 0x6C, 0x75), QColor(0xF2, 0xC9, 0x6B), QSize(160, 118)},
                           {QStringLiteral("Transit"), QStringLiteral("city"), QStringLiteral("metro-lines"), QColor(0x37, 0x5B, 0xA8), QColor(0x58, 0xB7, 0xE8), QSize(160, 118)},
                           {QStringLiteral("Market"), QStringLiteral("street"), QStringLiteral("night-market"), QColor(0x91, 0x43, 0x4A), QColor(0xE0, 0x95, 0x52), QSize(160, 118)},
                           {QStringLiteral("Cabin"), QStringLiteral("retreat"), QStringLiteral("wood-cabin"), QColor(0x5B, 0x75, 0x35), QColor(0xCE, 0xA8, 0x5B), QSize(160, 118)}};

                       auto* model = makeFlowPhotoModel(flowView, photos);
                       flowView->setModel(model);
                       flowView->setSelectedIndex(0);

                       auto* network = new QNetworkAccessManager(flowView);
                       loadFlowNetworkImages(model, photos, network);
                       return flowView;
                   }),
        makeSample(QStringLiteral("flow-view-reorder"),
                   QStringLiteral("Drag photos to reorder"),
                   QStringLiteral("Irregular image cards wrap across rows; drag a card to reorder the model."),
                   QStringLiteral("auto* flowView = new FlowView(this);\n"
                                  "flowView->setItemDelegate(photoDelegate);\n"
                                  "flowView->setModel(model);\n"
                                  "flowView->setCanReorderItems(true);"),
                   [](QWidget* parent) {
                       auto* flowView = new FlowView(parent);
                       flowView->setFixedSize(540, 318);
                       flowView->setCanReorderItems(true);
                       flowView->setDefaultItemSize(QSize(150, 110));
                       flowView->setMinimumItemSize(QSize(112, 92));
                       flowView->setHorizontalSpacing(10);
                       flowView->setVerticalSpacing(10);
                       flowView->setContentMargins(QMargins(8, 8, 8, 8));
                       flowView->setItemDelegate(new FlowPhotoDelegate(
                           static_cast<fluent::FluentElement*>(flowView), flowView));

                       const QVector<FlowPhotoInfo> photos{
                           {QStringLiteral("Loft"), QStringLiteral("interior"), QStringLiteral("loft-interior"), QColor(0x5D, 0x7F, 0xB8), QColor(0xC9, 0xDB, 0xF2), QSize(148, 104)},
                           {QStringLiteral("Ridge"), QStringLiteral("wide"), QStringLiteral("wide-ridge"), QColor(0x24, 0x74, 0x8F), QColor(0x8C, 0xCF, 0xA5), QSize(210, 118)},
                           {QStringLiteral("Cafe"), QStringLiteral("street"), QStringLiteral("corner-cafe"), QColor(0xA9, 0x5B, 0x45), QColor(0xE9, 0xB8, 0x7A), QSize(132, 132)},
                           {QStringLiteral("Mist"), QStringLiteral("forest"), QStringLiteral("mist-forest"), QColor(0x3C, 0x75, 0x52), QColor(0xAF, 0xC9, 0x8E), QSize(172, 148)},
                           {QStringLiteral("Gallery"), QStringLiteral("exhibit"), QStringLiteral("white-gallery"), QColor(0x66, 0x6A, 0x86), QColor(0xD9, 0xD7, 0xEA), QSize(122, 104)},
                           {QStringLiteral("Canal"), QStringLiteral("water"), QStringLiteral("city-canal"), QColor(0x2D, 0x73, 0xA3), QColor(0x9A, 0xD8, 0xE5), QSize(190, 126)},
                           {QStringLiteral("Trail"), QStringLiteral("nature"), QStringLiteral("sun-trail"), QColor(0x70, 0x83, 0x2F), QColor(0xE2, 0xC4, 0x58), QSize(156, 116)},
                           {QStringLiteral("Arcade"), QStringLiteral("night"), QStringLiteral("neon-arcade"), QColor(0x57, 0x3A, 0x9B), QColor(0xDC, 0x72, 0xC8), QSize(198, 140)},
                           {QStringLiteral("Pier"), QStringLiteral("coast"), QStringLiteral("quiet-pier"), QColor(0x2F, 0x80, 0x8F), QColor(0xE0, 0xC1, 0x7E), QSize(136, 106)}};

                       auto* model = makeFlowPhotoModel(flowView, photos);
                       flowView->setModel(model);
                       flowView->setSelectedIndex(1);

                       auto* network = new QNetworkAccessManager(flowView);
                       loadFlowNetworkImages(model, photos, network);
                       return flowView;
                   })
    };
}

// Uniform photo set shared by the GridView samples; each card is the same size so the cells
// align to a tidy grid (GridView's job), while the FlowPhotoDelegate paints the cover image.
// zh_CN: GridView 各示例共用的统一尺寸照片集合；每张卡片同尺寸以对齐网格（GridView 负责），
// FlowPhotoDelegate 负责绘制封面图。
QStandardItemModel* makeGridPhotoModel(GridView* grid, const QSize& cell)
{
    const QVector<FlowPhotoInfo> photos{
        {QStringLiteral("Sunrise"), QStringLiteral("Warm"), QStringLiteral("grid-sunrise"), QColor(0xF7, 0x97, 0x5B), QColor(0xF2, 0xC9, 0x4C), cell},
        {QStringLiteral("Ocean"), QStringLiteral("Blue"), QStringLiteral("grid-ocean"), QColor(0x1E, 0x6F, 0xD9), QColor(0x6F, 0xD1, 0xF2), cell},
        {QStringLiteral("Forest"), QStringLiteral("Green"), QStringLiteral("grid-forest"), QColor(0x2F, 0x9E, 0x44), QColor(0xA9, 0xE3, 0x4B), cell},
        {QStringLiteral("Dusk"), QStringLiteral("Violet"), QStringLiteral("grid-dusk"), QColor(0x6B, 0x4F, 0xA2), QColor(0xC2, 0x6F, 0xB8), cell},
        {QStringLiteral("Desert"), QStringLiteral("Amber"), QStringLiteral("grid-desert"), QColor(0xC8, 0x6B, 0x2D), QColor(0xE8, 0xC0, 0x6E), cell},
        {QStringLiteral("Glacier"), QStringLiteral("Ice"), QStringLiteral("grid-glacier"), QColor(0x3D, 0x8B, 0xA3), QColor(0xB3, 0xE5, 0xE8), cell},
        {QStringLiteral("Meadow"), QStringLiteral("Spring"), QStringLiteral("grid-meadow"), QColor(0x6F, 0xA8, 0x2F), QColor(0xD4, 0xE6, 0x7A), cell},
        {QStringLiteral("Harbor"), QStringLiteral("Teal"), QStringLiteral("grid-harbor"), QColor(0x1C, 0x6E, 0x8C), QColor(0x73, 0xC8, 0xD0), cell}};

    auto* model = makeFlowPhotoModel(grid, photos);
    auto* network = new QNetworkAccessManager(grid);
    loadFlowNetworkImages(model, photos, network);
    return model;
}

QVector<GallerySample> gridViewSamples()
{
    return {
        makeSample(QStringLiteral("grid-view-basic"),
                   QStringLiteral("GridView photo grid"),
                   QStringLiteral("Photo cards align to a uniform grid; a custom delegate paints the cover image and caption."),
                   QStringLiteral("auto* gridView = new GridView(this);\n"
                                  "gridView->setCellSize(QSize(150, 112));\n"
                                  "gridView->setItemDelegate(photoDelegate);\n"
                                  "gridView->setModel(model);\n"
                                  "gridView->setSelectedIndex(0);"),
                   [](QWidget* parent) {
                       auto* gridView = new GridView(parent);
                       gridView->setFixedSize(508, 256);
                       gridView->setCellSize(QSize(150, 112));
                       gridView->setMaxColumns(3);
                       gridView->setHorizontalSpacing(10);
                       gridView->setVerticalSpacing(10);
                       gridView->setItemDelegate(new GridPhotoDelegate(
                           static_cast<fluent::FluentElement*>(gridView), gridView, gridView));
                       gridView->setModel(makeGridPhotoModel(gridView, QSize(150, 112)));
                       gridView->setSelectedIndex(0);
                       return gridView;
                   }),
        makeSample(QStringLiteral("grid-view-multi-select"),
                   QStringLiteral("Multiple selection"),
                   QStringLiteral("In Multiple mode each click toggles a cell; selected cells keep an accent border. Ctrl/Shift extend in Extended mode."),
                   QStringLiteral("gridView->setSelectionMode(\n"
                                  "    GridView::GridSelectionMode::Multiple);\n"
                                  "// each click toggles that cell's selection"),
                   [](QWidget* parent) {
                       auto* gridView = new GridView(parent);
                       gridView->setFixedSize(508, 256);
                       gridView->setCellSize(QSize(150, 112));
                       gridView->setMaxColumns(3);
                       gridView->setHorizontalSpacing(10);
                       gridView->setVerticalSpacing(10);
                       gridView->setSelectionMode(GridView::GridSelectionMode::Multiple);
                       gridView->setItemDelegate(new GridPhotoDelegate(
                           static_cast<fluent::FluentElement*>(gridView), gridView, gridView));
                       auto* model = makeGridPhotoModel(gridView, QSize(150, 112));
                       gridView->setModel(model);
                       if (auto* selection = gridView->selectionModel()) {
                           for (int row : {0, 2, 5}) {
                               selection->select(model->index(row, 0),
                                                 QItemSelectionModel::Select);
                           }
                       }
                       return gridView;
                   }),
        makeSample(QStringLiteral("grid-view-reorder"),
                   QStringLiteral("Multi-select & drag to reorder"),
                   QStringLiteral("Multiple selection plus reordering: tick cells via the top-right check, then drag any selected cell to move the whole group, just like the GridView UT."),
                   QStringLiteral("gridView->setSelectionMode(\n"
                                  "    GridView::GridSelectionMode::Multiple);\n"
                                  "gridView->setCanReorderItems(true);\n"
                                  "// drag a selected cell to move the selection as a group"),
                   [](QWidget* parent) {
                       auto* gridView = new GridView(parent);
                       gridView->setFixedSize(508, 256);
                       gridView->setCellSize(QSize(150, 112));
                       gridView->setMaxColumns(3);
                       gridView->setHorizontalSpacing(10);
                       gridView->setVerticalSpacing(10);
                       gridView->setSelectionMode(GridView::GridSelectionMode::Multiple);
                       gridView->setCanReorderItems(true);
                       gridView->setItemDelegate(new GridPhotoDelegate(
                           static_cast<fluent::FluentElement*>(gridView), gridView, gridView));
                       auto* model = makeGridPhotoModel(gridView, QSize(150, 112));
                       gridView->setModel(model);
                       if (auto* selection = gridView->selectionModel()) {
                           for (int row : {1, 3}) {
                               selection->select(model->index(row, 0),
                                                 QItemSelectionModel::Select);
                           }
                       }
                       return gridView;
                   })
    };
}

QVector<GallerySample> listViewSamples()
{
    return {
        makeSample(QStringLiteral("list-view-basic"),
                   QStringLiteral("ListView with avatars"),
                   QStringLiteral("Rows pair an avatar with text; selection shows the animated indicator."),
                   QStringLiteral("auto* listView = new ListView(this);\n"
                                  "for (const QString& contact : contacts) {\n"
                                  "    auto* item = new QStandardItem(contact);\n"
                                  "    item->setIcon(initialsAvatar(contact));\n"
                                  "    model->appendRow(item);\n"
                                  "}\n"
                                  "listView->setModel(model);\n"
                                  "listView->setHeaderText(\"Contacts\");"),
                   [](QWidget* parent) {
                       auto* listView = new ListView(parent);
                       listView->setFixedSize(320, 240);
                       listView->setHeaderText(QStringLiteral("Contacts"));
                       listView->setIconSize(QSize(28, 28));
                       listView->setItemDelegate(new ListRowDelegate(
                           static_cast<fluent::FluentElement*>(listView), listView, listView));
                       const QStringList names{
                           QStringLiteral("Kendall Collins"), QStringLiteral("Henry Ross"),
                           QStringLiteral("Nicole Wagner"), QStringLiteral("Adam Wolfe"),
                           QStringLiteral("Stephanie Meyer"), QStringLiteral("Maya Patel"),
                           QStringLiteral("Alex Chen"), QStringLiteral("Priya Shah"),
                           QStringLiteral("Omar Rivera"), QStringLiteral("Elena Rossi"),
                           QStringLiteral("Jordan Lee"), QStringLiteral("Riley Brooks")};
                       auto* model = new QStandardItemModel(listView);
                       int colorIndex = 0;
                       for (const QString& name : names) {
                           auto* item = new QStandardItem(name);
                           item->setEditable(false);
                           item->setIcon(initialsAvatar(
                               name, accentPalette().at(colorIndex++ % accentPalette().size())));
                           model->appendRow(item);
                       }
                       listView->setModel(model);
                       listView->setSelectedIndex(0);
                       return listView;
                   }),
        makeSample(QStringLiteral("list-view-multi-select"),
                   QStringLiteral("Multiple selection"),
                   QStringLiteral("In Multiple mode each click toggles a row; every selected row keeps its fill and indicator."),
                   QStringLiteral("listView->setSelectionMode(\n"
                                  "    ListView::ListSelectionMode::Multiple);\n"
                                  "listView->setModel(filterModel);\n"
                                  "// each click toggles that row's selection"),
                   [](QWidget* parent) {
                       auto* listView = new ListView(parent);
                       listView->setFixedSize(320, 244);
                       listView->setHeaderText(QStringLiteral("Filters"));
                       listView->setIconSize(QSize(24, 24));
                       listView->setItemDelegate(new ListRowDelegate(
                           static_cast<fluent::FluentElement*>(listView), listView, listView));
                       listView->setSelectionMode(ListView::ListSelectionMode::Multiple);
                       listView->setModel(makeGlyphListModel(
                           listView,
                           {{QStringLiteral("Unread"), Typography::Icons::Mail},
                            {QStringLiteral("Flagged"), Typography::Icons::Flag},
                            {QStringLiteral("Has photos"), Typography::Icons::Camera},
                            {QStringLiteral("From contacts"), Typography::Icons::People},
                            {QStringLiteral("Favorites"), Typography::Icons::FavoriteStar},
                            {QStringLiteral("With documents"), Typography::Icons::Document},
                            {QStringLiteral("Pinned"), Typography::Icons::Pin},
                            {QStringLiteral("Scheduled"), Typography::Icons::Calendar},
                            {QStringLiteral("Archived"), Typography::Icons::Folder}},
                           24));
                       if (auto* selection = listView->selectionModel()) {
                           for (int row : {0, 2}) {
                               selection->select(listView->model()->index(row, 0),
                                                 QItemSelectionModel::Select | QItemSelectionModel::Rows);
                           }
                       }
                       return listView;
                   }),
        makeSample(QStringLiteral("list-view-horizontal"),
                   QStringLiteral("Horizontal list"),
                   QStringLiteral("Set the flow to LeftToRight for a horizontally scrolling strip of labelled items."),
                   QStringLiteral("listView->setFlow(QListView::LeftToRight);\n"
                                  "listView->setModel(model);"),
                   [](QWidget* parent) {
                       auto* listView = new ListView(parent);
                       listView->setFixedSize(540, 132);
                       listView->setHeaderText(QStringLiteral("Library"));
                       listView->setFlow(QListView::LeftToRight);
                       listView->setIconSize(QSize(26, 26));
                       listView->setItemDelegate(new ListRowDelegate(
                           static_cast<fluent::FluentElement*>(listView), listView, listView));
                       listView->setModel(makeGlyphListModel(
                           listView,
                           {{QStringLiteral("Home"), Typography::Icons::Home},
                            {QStringLiteral("Music"), Typography::Icons::Music},
                            {QStringLiteral("Videos"), Typography::Icons::Video},
                            {QStringLiteral("Photos"), Typography::Icons::Camera},
                            {QStringLiteral("Calendar"), Typography::Icons::Calendar},
                            {QStringLiteral("Settings"), Typography::Icons::Settings}},
                           26));
                       listView->setSelectedIndex(0);
                       return listView;
                   }),
        makeSample(QStringLiteral("list-view-reorder"),
                   QStringLiteral("Drag rows to reorder"),
                   QStringLiteral("With reordering enabled, drag a row to a new position in the playlist."),
                   QStringLiteral("auto* listView = new ListView(this);\n"
                                  "listView->setHeaderText(\"Playlist\");\n"
                                  "listView->setModel(model);\n"
                                  "listView->setCanReorderItems(true);"),
                   [](QWidget* parent) {
                       auto* listView = new ListView(parent);
                       listView->setFixedSize(320, 220);
                       listView->setHeaderText(QStringLiteral("Playlist"));
                       listView->setIconSize(QSize(24, 24));
                       listView->setItemDelegate(new ListRowDelegate(
                           static_cast<fluent::FluentElement*>(listView), listView, listView));
                       listView->setCanReorderItems(true);
                       listView->setModel(makeGlyphListModel(
                           listView,
                           {{QStringLiteral("Bloom"), Typography::Icons::Music},
                            {QStringLiteral("Northern Lights"), Typography::Icons::Music},
                            {QStringLiteral("Driftwood"), Typography::Icons::Music},
                            {QStringLiteral("Paper Boats"), Typography::Icons::Music},
                            {QStringLiteral("Blue Hour"), Typography::Icons::Music},
                            {QStringLiteral("Signal Fire"), Typography::Icons::Music},
                            {QStringLiteral("Slow Orbit"), Typography::Icons::Music},
                            {QStringLiteral("City Lights"), Typography::Icons::Music},
                            {QStringLiteral("Afterglow"), Typography::Icons::Music},
                            {QStringLiteral("Quiet Roads"), Typography::Icons::Music}},
                           24));
                       return listView;
                   })
    };
}

QVector<GallerySample> splitViewSamples()
{
    return {
        makeSample(QStringLiteral("split-view-basic"),
                   QStringLiteral("Resizable panes"),
                   QStringLiteral("Drag the handle between the panes to resize them."),
                   QStringLiteral("auto* splitView = new SplitView(this);\n"
                                  "splitView->addPane(firstPane);\n"
                                  "splitView->addPane(secondPane);\n"
                                  "splitView->addPane(thirdPane);\n"
                                  "splitView->setPanePreferredSize(0, 150);\n"
                                  "splitView->setPaneFill(2, true);"),
                   [](QWidget* parent) {
                       auto* splitView = new SplitView(parent);
                       splitView->setFixedSize(460, 168);
                       splitView->addPane(makeGradientPane(splitView, QStringLiteral("Pane 1"),
                                                           QColor(0x1E, 0x6F, 0xD9), QColor(0x6F, 0xD1, 0xF2)));
                       splitView->addPane(makeGradientPane(splitView, QStringLiteral("Pane 2"),
                                                           QColor(0x2F, 0x9E, 0x44), QColor(0xA9, 0xE3, 0x4B)));
                       splitView->addPane(makeGradientPane(splitView, QStringLiteral("Pane 3"),
                                                           QColor(0xF7, 0x97, 0x5B), QColor(0xF2, 0xC9, 0x4C)));
                       // Give every pane a real minimum so dragging a handle to the limit
                       // leaves a tidy pane instead of a sliver, and never shrinks a pane
                       // below the gradient widget's own minimum (which would overlap).
                       // zh_CN: 每个窗格都设定最小尺寸，拖到极限时仍保留完整窗格而非细条，
                       // 且不会缩到渐变控件自身最小尺寸以下（否则会重叠）。
                       for (int i = 0; i < 3; ++i)
                           splitView->setPaneMinimumSize(i, 96);
                       splitView->setPanePreferredSize(0, 150);
                       splitView->setPanePreferredSize(1, 150);
                       splitView->setPaneFill(2, true);
                       return splitView;
                   })
    };
}

QVector<GallerySample> stackViewSamples()
{
    return {
        makeSample(QStringLiteral("stack-view-basic"),
                   QStringLiteral("Push and pop pages"),
                   QStringLiteral("StackView keeps a navigation stack of pages with transitions."),
                   QStringLiteral("auto* stackView = new StackView(this);\n"
                                  "stackView->setInitialItem(firstPage);\n"
                                  "stackView->push(nextPage);\n"
                                  "stackView->pop();"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 12);

                       auto* stackView = new StackView(group);
                       stackView->setFixedSize(360, 150);
                       stackView->setInitialItem(makeGradientPane(stackView, QStringLiteral("Page 1"),
                                                                  QColor(0x1E, 0x6F, 0xD9), QColor(0x6F, 0xD1, 0xF2)));

                       QWidget* buttons = horizontalGroup(group, 8);
                       auto* pushButton = new Button(QStringLiteral("Push page"), buttons);
                       pushButton->setFluentStyle(Button::Accent);
                       auto* popButton = new Button(QStringLiteral("Pop page"), buttons);
                       QObject::connect(pushButton, &Button::clicked,
                                        stackView, [stackView]() {
                                            static const QVector<QPair<QColor, QColor>> palette{
                                                {QColor(0x2F, 0x9E, 0x44), QColor(0xA9, 0xE3, 0x4B)},
                                                {QColor(0xF7, 0x97, 0x5B), QColor(0xF2, 0xC9, 0x4C)},
                                                {QColor(0x87, 0x64, 0xB8), QColor(0xC2, 0x6F, 0xB8)}};
                                            const int depth = stackView->depth();
                                            const auto& pair = palette.at((depth - 1) % palette.size());
                                            stackView->push(makeGradientPane(
                                                stackView,
                                                QStringLiteral("Page %1").arg(depth + 1),
                                                pair.first, pair.second));
                                        });
                       QObject::connect(popButton, &Button::clicked,
                                        stackView, [stackView]() { stackView->pop(); });
                       buttons->layout()->addWidget(pushButton);
                       buttons->layout()->addWidget(popButton);

                       group->layout()->addWidget(stackView);
                       group->layout()->addWidget(buttons);
                       return group;
                   })
    };
}

// Build a QStandardItem carrying a Segoe Fluent Icons glyph (drawn crisply by the
// TreeRowDelegate via TreeIconGlyphRole) plus an optional accent color, instead of a
// rasterized icon pixmap — matching the role-based model the TreeView UT exercises.
// zh_CN: 构造携带 Segoe Fluent 图标字形（由 TreeRowDelegate 经 TreeIconGlyphRole 清晰绘制）
// 及可选强调色的 QStandardItem，而非位图图标——与 TreeView UT 使用的 role 化模型一致。
QStandardItem* makeTreeItem(const QString& text, const QString& glyph, const QColor& color)
{
    auto* item = new QStandardItem(text);
    item->setEditable(false);
    item->setData(glyph, TreeIconGlyphRole);
    item->setData(color, TreeIconColorRole);
    return item;
}

/**
 * @brief Nested folder/file model shared by the basic and drag-reorder tree samples.
 * zh_CN: 基础树与拖拽换位树示例共用的嵌套文件夹/文件模型。
 */
QStandardItemModel* makeFolderTreeModel(QObject* owner, const QColor& folderColor,
                                        const QColor& fileColor)
{
    auto* model = new QStandardItemModel(owner);
    auto file = [&](const QString& text) {
        return makeTreeItem(text, Typography::Icons::Document, fileColor);
    };
    auto folder = [&](const QString& text) {
        return makeTreeItem(text, Typography::Icons::Folder, folderColor);
    };

    auto* work = folder(QStringLiteral("Work documents"));
    work->appendRow(file(QStringLiteral("Proposal.docx")));
    work->appendRow(file(QStringLiteral("Budget.xlsx")));
    auto* archive = folder(QStringLiteral("Archive"));
    archive->appendRow(file(QStringLiteral("Q1-review.pdf")));
    archive->appendRow(file(QStringLiteral("Q2-review.pdf")));
    work->appendRow(archive);
    model->appendRow(work);

    auto* photos = folder(QStringLiteral("Photos"));
    photos->appendRow(file(QStringLiteral("Trip.png")));
    photos->appendRow(file(QStringLiteral("Family.png")));
    model->appendRow(photos);

    auto* music = folder(QStringLiteral("Music"));
    music->appendRow(file(QStringLiteral("Playlist.m3u")));
    model->appendRow(music);
    return model;
}

QVector<GallerySample> treeViewSamples()
{
    const QColor folderColor(0xCA, 0x8A, 0x1A);
    const QColor fileColor(0x52, 0x8B, 0xC4);
    const int rowHeight = Spacing::ControlHeight::Standard + Spacing::Gap::Tight;

    return {
        makeSample(QStringLiteral("tree-view-basic"),
                   QStringLiteral("Folder hierarchy"),
                   QStringLiteral("The essential tree: click a chevron to expand or collapse a folder, and selecting a row shows the accent indicator. A custom delegate paints the rotating chevron and per-row icon glyph."),
                   QStringLiteral("auto* tree = new TreeView(this);\n"
                                  "tree->setItemDelegate(new TreeRowDelegate(\n"
                                  "    themeHost, rowHeight, tree, tree));\n"
                                  "tree->setModel(model);\n"
                                  "tree->expandAll();"),
                   [folderColor, fileColor, rowHeight](QWidget* parent) {
                       auto* tree = new TreeView(parent);
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(252);
                       tree->setItemDelegate(new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree));

                       auto* model = makeFolderTreeModel(tree, folderColor, fileColor);
                       tree->setModel(model);
                       tree->expandAll();
                       tree->setSelectedItem(model->index(0, 0));
                       return tree;
                   }),
        makeSample(QStringLiteral("tree-view-checkboxes"),
                   QStringLiteral("Checkable items"),
                   QStringLiteral("The delegate's checkbox mode shows a tri-state box on every row: ticking a parent cascades to its children, and editing a child rolls the parent up to checked / unchecked / partial."),
                   QStringLiteral("auto* d = new TreeRowDelegate(\n"
                                  "    themeHost, rowHeight, tree, tree);\n"
                                  "d->setCheckBoxVisible(true);\n"
                                  "tree->setItemDelegate(d);\n"
                                  "// clicks cascade down + roll the tri-state up"),
                   [folderColor, rowHeight](QWidget* parent) {
                       auto* tree = new TreeView(parent);
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(258);
                       auto* delegate = new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree);
                       delegate->setCheckBoxVisible(true);
                       tree->setItemDelegate(delegate);

                       auto* model = new QStandardItemModel(tree);
                       auto leaf = [](const QString& text, Qt::CheckState state,
                                      const QString& glyph, const QColor& color) {
                           auto* item = makeTreeItem(text, glyph, color);
                           item->setCheckable(true);
                           item->setCheckState(state);
                           return item;
                       };
                       auto group = [&](const QString& text, Qt::CheckState state) {
                           auto* node = makeTreeItem(text, Typography::Icons::Folder, folderColor);
                           node->setCheckable(true);
                           node->setCheckState(state);
                           return node;
                       };

                       auto* sync = group(QStringLiteral("Sync these settings"), Qt::PartiallyChecked);
                       sync->appendRow(leaf(QStringLiteral("Passwords"), Qt::Checked,
                                            Typography::Icons::Pin, QColor(0x49, 0x82, 0x05)));
                       sync->appendRow(leaf(QStringLiteral("Bookmarks"), Qt::Checked,
                                            Typography::Icons::FavoriteStar, QColor(0xCA, 0x50, 0x10)));
                       sync->appendRow(leaf(QStringLiteral("History"), Qt::Unchecked,
                                            Typography::Icons::Calendar, QColor(0x87, 0x64, 0xB8)));
                       model->appendRow(sync);

                       auto* notify = group(QStringLiteral("Notifications"), Qt::Checked);
                       notify->appendRow(leaf(QStringLiteral("Email"), Qt::Checked,
                                              Typography::Icons::Mail, QColor(0x00, 0x78, 0xD4)));
                       notify->appendRow(leaf(QStringLiteral("Messages"), Qt::Checked,
                                              Typography::Icons::Message, QColor(0x03, 0x83, 0x87)));
                       model->appendRow(notify);

                       auto* privacy = group(QStringLiteral("Privacy"), Qt::Unchecked);
                       privacy->appendRow(leaf(QStringLiteral("Location"), Qt::Unchecked,
                                               Typography::Icons::MapPin, QColor(0xD8, 0x3B, 0x01)));
                       privacy->appendRow(leaf(QStringLiteral("Camera"), Qt::Unchecked,
                                               Typography::Icons::Camera, QColor(0x2D, 0x7D, 0x9A)));
                       privacy->appendRow(leaf(QStringLiteral("Microphone"), Qt::Unchecked,
                                               Typography::Icons::Microphone, QColor(0x5C, 0x2D, 0x91)));
                       model->appendRow(privacy);

                       tree->setModel(model);
                       tree->expandAll();
                       return tree;
                   }),
        makeSample(QStringLiteral("tree-view-reorder"),
                   QStringLiteral("Drag to reorder"),
                   QStringLiteral("With reordering enabled, drag a row to a new spot among its siblings; the model updates and itemReordered fires with the source and destination."),
                   QStringLiteral("tree->setCanReorderItems(true);\n"
                                  "QObject::connect(tree, &TreeView::itemReordered,\n"
                                  "    [](const QModelIndex& srcParent, int srcRow,\n"
                                  "       const QModelIndex& dstParent, int dstRow) { /* ... */ });"),
                   [folderColor, fileColor, rowHeight](QWidget* parent) {
                       auto* tree = new TreeView(parent);
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(252);
                       tree->setCanReorderItems(true);
                       tree->setItemDelegate(new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree));

                       auto* model = makeFolderTreeModel(tree, folderColor, fileColor);
                       tree->setModel(model);
                       tree->expandAll();
                       return tree;
                   })
    };
}

} // namespace

QVector<GallerySample> collectionsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("drawer-view"))
        return drawerViewSamples();
    if (routeId == QStringLiteral("flip-view"))
        return flipViewSamples();
    if (routeId == QStringLiteral("flow-view"))
        return flowViewSamples();
    if (routeId == QStringLiteral("grid-view"))
        return gridViewSamples();
    if (routeId == QStringLiteral("list-view"))
        return listViewSamples();
    if (routeId == QStringLiteral("split-view"))
        return splitViewSamples();
    if (routeId == QStringLiteral("stack-view"))
        return stackViewSamples();
    if (routeId == QStringLiteral("tree-view"))
        return treeViewSamples();
    return {};
}

} // namespace fluent::gallery

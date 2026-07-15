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
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QtGlobal>

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
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "support/logging/Log.h"
#include "CollectionSampleDelegates.h"
#include "view/shell/GalleryWindowMetrics.h"
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
using fluent::collections::SplitViewPaneOptions;
using fluent::collections::StackView;
using fluent::collections::TreeView;
using fluent::textfields::Label;
using samples::glyphPixmap;
using samples::gradientPixmap;
using samples::horizontalGroup;
using samples::initialsAvatar;
using samples::makeSample;
using samples::verticalGroup;

// Collection controls (ListView/TreeView) paint their own bgLayer container surface, which is DARKER
// than the gallery sample-card preview panel (bgLayerAlt) they sit on — reading as a sunken nested
// "extra layer". In the gallery previews the items should sit flat on the preview panel (matching the
// clean Button-style samples), so hide the control's own surface + border here. This only touches the
// gallery sample instances; the controls keep their default surface everywhere else.
// zh_CN: 集合控件(ListView/TreeView)会自绘 bgLayer 容器表面,比所在的画廊示例卡预览面板(bgLayerAlt)更暗——
// 呈现为下陷的嵌套「多余图层」。画廊预览里条目应平铺在预览面板上(与 Button 风格的干净示例一致),故在此隐藏控件自身
// 的表面与边框。仅作用于画廊示例实例;控件在别处仍保留默认表面。
template <typename View>
View* flatPreviewSurface(View* view)
{
    if (view) {
        view->setBackgroundVisible(false);
        view->setBorderVisible(false);
    }
    return view;
}

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

class DrawerGradientPane : public QWidget {
public:
    DrawerGradientPane(const QString& caption,
                       const QColor& from,
                       const QColor& to,
                       DrawerView::DrawerEdge edge,
                       QWidget* parent = nullptr)
        : QWidget(parent)
        , m_caption(caption)
        , m_from(from)
        , m_to(to)
        , m_edge(edge)
    {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setEdge(DrawerView::DrawerEdge edge)
    {
        if (m_edge == edge)
            return;
        m_edge = edge;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const QRectF surface = QRectF(rect());
        QLinearGradient gradient(surface.topLeft(), surface.bottomRight());
        gradient.setColorAt(0.0, m_from);
        gradient.setColorAt(1.0, m_to);

        const bool roundTopLeft = m_edge == DrawerView::DrawerEdge::Right
            || m_edge == DrawerView::DrawerEdge::Bottom;
        const bool roundTopRight = m_edge == DrawerView::DrawerEdge::Left
            || m_edge == DrawerView::DrawerEdge::Bottom;
        const bool roundBottomRight = m_edge == DrawerView::DrawerEdge::Left
            || m_edge == DrawerView::DrawerEdge::Top;
        const bool roundBottomLeft = m_edge == DrawerView::DrawerEdge::Right
            || m_edge == DrawerView::DrawerEdge::Top;
        const QPainterPath path = ::fluent::overlay::roundedCornerRectPath(
            surface, 8.0, roundTopLeft, roundTopRight, roundBottomRight, roundBottomLeft);

        painter.setPen(Qt::NoPen);
        painter.fillRect(rect(), gradient);
        QPainterPath fullRect;
        fullRect.addRect(surface);
        const QPainterPath cornerCutouts = fullRect.subtracted(path);
        if (!cornerCutouts.isEmpty()) {
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.fillPath(cornerCutouts, Qt::transparent);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }

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
    DrawerView::DrawerEdge m_edge = DrawerView::DrawerEdge::Left;
};

class SampleSurface : public QWidget, public fluent::FluentElement {
public:
    explicit SampleSurface(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
    }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const auto colors = themeColors();
        const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath path;
        path.addRoundedRect(surface, 8.0, 8.0);
        painter.fillPath(path, colors.bgLayerAlt);
        painter.setPen(QPen(colors.strokeDefault, 1.0));
        painter.drawPath(path);
    }
};

GradientPane* makeGradientPane(QWidget* parent, const QString& caption,
                               const QColor& from, const QColor& to)
{
    return new GradientPane(caption, from, to, parent);
}

SampleSurface* makeSampleSurface(QWidget* parent, const QSize& fixedSize)
{
    auto* surface = new SampleSurface(parent);
    if (fixedSize.isValid())
        surface->setFixedSize(fixedSize);
    return surface;
}

Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    label->setTextColorRole(Label::TextColorRole::Primary);  // QSS-proof on the styled preview surface
    return label;
}

QMargins drawerTitleBarAvoidanceMargins()
{
    return metrics::Drawer::titleBarAvoidanceMargins();
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
        item->setData(glyphPixmap(row.second,
                                  accentPalette().at(colorIndex++ % accentPalette().size()),
                                  iconSize),
                      Qt::DecorationRole);
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
        painter->drawPixmap(target, pixmap, fluentPixmapSourceRectForDraw(source, pixmap));
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
    request.setTransferTimeout(10000);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Fluent-Qt Gallery/1.0"));
    QNetworkReply* reply = manager->get(request);
    QObject::connect(reply, &QNetworkReply::finished, manager, [modelGuard, index, reply, url]() {
        reply->deleteLater();
        if (!modelGuard || !index.isValid())
            return;
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN(QStringLiteral("Gallery photo request failed url=%1 error=%2")
                         .arg(url.toString(), reply->errorString()));
            return;
        }

        QPixmap pixmap;
        if (!pixmap.loadFromData(reply->readAll()) || pixmap.isNull()) {
            LOG_WARN(QStringLiteral("Gallery photo response is not a supported image url=%1")
                         .arg(url.toString()));
            return;
        }
        modelGuard->setData(index, pixmap, PhotoImageRole);
    });
}

QUrl flowPhotoUrl(const FlowPhotoInfo& photo, int row)
{
    // Fixed image ids keep the sample deterministic while avoiding the Picsum endpoint,
    // which is unavailable on some macOS/network configurations.
    // zh_CN: 固定图片 id 保证示例稳定，同时避开部分 macOS/网络环境无法访问的 Picsum 端点。
    static const QStringList imageIds{
        QStringLiteral("photo-1500530855697-b586d89ba3ee"),
        QStringLiteral("photo-1470770841072-f978cf4d019e"),
        QStringLiteral("photo-1441974231531-c6227db76b6e"),
        QStringLiteral("photo-1470252649378-9c29740c9fa8"),
        QStringLiteral("photo-1506744038136-46273834b3fb"),
        QStringLiteral("photo-1472214103451-9374bd1c798e"),
        QStringLiteral("photo-1469474968028-56623f02e42e"),
        QStringLiteral("photo-1511818966892-d7d671e672a2"),
        QStringLiteral("photo-1497366754035-f200968a6e72")};
    const int imageIndex = row % static_cast<int>(imageIds.size());
    const QString imageId = imageIds.at(imageIndex);
    QUrl url(QStringLiteral("https://images.unsplash.com/%1").arg(imageId));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fm"), QStringLiteral("jpg"));
    query.addQueryItem(QStringLiteral("fit"), QStringLiteral("crop"));
    query.addQueryItem(QStringLiteral("w"), QString::number(photo.size.width() * 2));
    query.addQueryItem(QStringLiteral("h"), QString::number(photo.size.height() * 2));
    query.addQueryItem(QStringLiteral("q"), QStringLiteral("82"));
    url.setQuery(query);
    return url;
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
        loadFlowNetworkImage(model, row, flowPhotoUrl(photos.at(row), row), manager);
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
                       title->setTextColorRole(Label::TextColorRole::Primary);  // QSS-proof on the styled preview surface
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
                   }),
        makeSample(QStringLiteral("drawer-view-edges"),
                   QStringLiteral("DrawerEdge and drawerLength"),
                   QStringLiteral("The same content can open from any window edge; drawerLength controls the panel size along that edge."),
                   QStringLiteral("auto* drawer = new DrawerView(host);\n"
                                  "drawer->setDrawerLength(170);\n"
                                  "drawer->setModal(false);\n"
                                  "drawer->setDim(false);\n\n"
                                  "auto openFrom = [drawer](DrawerView::DrawerEdge edge) {\n"
                                  "    drawer->setEdge(edge);\n"
                                  "    drawer->open();\n"
                                  "};"),
                   [](QWidget* parent) {
                       auto* host = makeSampleSurface(parent, QSize(460, 238));
                       auto* layout = new QVBoxLayout(host);
                       layout->setContentsMargins(18, 18, 18, 18);
                       layout->setSpacing(12);

                       auto* buttons = horizontalGroup(host, 8);
                       auto* leftButton = new Button(QStringLiteral("Left"), buttons);
                       auto* rightButton = new Button(QStringLiteral("Right"), buttons);
                       auto* topButton = new Button(QStringLiteral("Top"), buttons);
                       auto* bottomButton = new Button(QStringLiteral("Bottom"), buttons);
                       buttons->layout()->addWidget(leftButton);
                       buttons->layout()->addWidget(rightButton);
                       buttons->layout()->addWidget(topButton);
                       buttons->layout()->addWidget(bottomButton);

                       auto* status = makeStatusLabel(host, QStringLiteral("Edge: Left, length: 170"));
                       layout->addWidget(buttons);
                       layout->addWidget(status);
                       layout->addStretch(1);

                       auto* drawer = new DrawerView(host);
                       drawer->setDrawerLength(170);
                       drawer->setModal(false);
                       drawer->setDim(false);
                       auto* drawerContent = new DrawerGradientPane(QStringLiteral("Drawer content"),
                                                                    QColor(0x1E, 0x6F, 0xD9),
                                                                    QColor(0x6F, 0xD1, 0xF2),
                                                                    DrawerView::DrawerEdge::Left,
                                                                    drawer);
                       drawer->setContentWidget(drawerContent);

                       const auto openFrom = [drawer, drawerContent, status](DrawerView::DrawerEdge edge,
                                                                             const QString& text) {
                           drawer->setEdge(edge);
                           drawerContent->setEdge(edge);
                           status->setText(QStringLiteral("Edge: %1, length: 170").arg(text));
                           drawer->open();
                       };
                       QObject::connect(leftButton, &Button::clicked, drawer, [openFrom]() {
                           openFrom(DrawerView::DrawerEdge::Left, QStringLiteral("Left"));
                       });
                       QObject::connect(rightButton, &Button::clicked, drawer, [openFrom]() {
                           openFrom(DrawerView::DrawerEdge::Right, QStringLiteral("Right"));
                       });
                       QObject::connect(topButton, &Button::clicked, drawer, [openFrom]() {
                           openFrom(DrawerView::DrawerEdge::Top, QStringLiteral("Top"));
                       });
                       QObject::connect(bottomButton, &Button::clicked, drawer, [openFrom]() {
                           openFrom(DrawerView::DrawerEdge::Bottom, QStringLiteral("Bottom"));
                       });
                       return host;
                   }),
        makeSample(QStringLiteral("drawer-view-close-policy"),
                   QStringLiteral("Modal, dim, and close policy"),
                   QStringLiteral("ClosePolicy separates light-dismiss behavior from explicit close commands."),
                   QStringLiteral("auto* drawer = new DrawerView(host);\n"
                                  "drawer->setAvailableMargins(drawerTitleBarAvoidanceMargins());\n"
                                  "drawer->setModal(false);\n"
                                  "drawer->setDim(false);\n"
                                  "drawer->setClosePolicy(\n"
                                  "    DrawerView::ClosePolicy(DrawerView::NoAutoClose));\n"
                                  "auto* panel = new QWidget;\n"
                                  "auto* panelLayout = new QVBoxLayout(panel);\n"
                                  "panelLayout->setContentsMargins(16, 18, 16, 18);\n"
                                  "drawer->setContentWidget(panel);\n\n"
                                  "QObject::connect(closeButton, &Button::clicked,\n"
                                  "                 drawer, &DrawerView::close);"),
                   [](QWidget* parent) {
                       auto* host = makeSampleSurface(parent, QSize(460, 238));
                       auto* layout = new QVBoxLayout(host);
                       layout->setContentsMargins(18, 18, 18, 18);
                       layout->setSpacing(12);

                       auto* buttons = horizontalGroup(host, 8);
                       auto* openButton = new Button(QStringLiteral("Open persistent drawer"), buttons);
                       openButton->setFluentStyle(Button::Accent);
                       buttons->layout()->addWidget(openButton);
                       auto* status = makeStatusLabel(host, QStringLiteral("Closed"));
                       layout->addWidget(buttons);
                       layout->addWidget(status);
                       layout->addStretch(1);

                       auto* drawer = new DrawerView(host);
                       drawer->setEdge(DrawerView::DrawerEdge::Left);
                       drawer->setAvailableMargins(drawerTitleBarAvoidanceMargins());
                       drawer->setDrawerLength(224);
                       drawer->setModal(false);
                       drawer->setDim(false);
                       drawer->setClosePolicy(DrawerView::ClosePolicy(DrawerView::NoAutoClose));

                       auto* panel = new QWidget;
                       auto* panelLayout = new QVBoxLayout(panel);
                       panelLayout->setContentsMargins(16, 18, 16, 18);
                       panelLayout->setSpacing(10);
                       auto* title = new Label(QStringLiteral("Persistent panel"), panel);
                       title->setFluentTypography(Typography::FontRole::BodyStrong);
                       title->setTextColorRole(Label::TextColorRole::Primary);  // QSS-proof on the styled preview surface
                       auto* closeButton = new Button(QStringLiteral("Close"), panel);
                       panelLayout->addWidget(title);
                       panelLayout->addStretch(1);
                       panelLayout->addWidget(closeButton);
                       drawer->setContentWidget(panel);

                       QObject::connect(openButton, &Button::clicked, drawer, &DrawerView::open);
                       QObject::connect(closeButton, &Button::clicked, drawer, &DrawerView::close);
                       QObject::connect(drawer, &DrawerView::opened, status, [status]() {
                           status->setText(QStringLiteral("Open: outside click does not dismiss"));
                       });
                       QObject::connect(drawer, &DrawerView::closed, status, [status]() {
                           status->setText(QStringLiteral("Closed"));
                       });
                       return host;
                   }),
        makeSample(QStringLiteral("drawer-view-interactive-drag"),
                   QStringLiteral("Interactive drag margin"),
                   QStringLiteral("Interactive drawers can be opened or adjusted by dragging from the configured edge margin."),
                   QStringLiteral("auto* drawer = new DrawerView(host);\n"
                                  "drawer->setInteractive(true);\n"
                                  "drawer->setDragMargin(36);\n"
                                  "drawer->setAnimationEnabled(true);\n"
                                  "drawer->setEdge(DrawerView::DrawerEdge::Left);"),
                   [](QWidget* parent) {
                       auto* host = makeSampleSurface(parent, QSize(460, 238));
                       auto* layout = new QVBoxLayout(host);
                       layout->setContentsMargins(18, 18, 18, 18);
                       layout->setSpacing(12);

                       auto* controls = horizontalGroup(host, 8);
                       auto* openButton = new Button(QStringLiteral("Open"), controls);
                       openButton->setFluentStyle(Button::Accent);
                       auto* closeButton = new Button(QStringLiteral("Close"), controls);
                       controls->layout()->addWidget(openButton);
                       controls->layout()->addWidget(closeButton);
                       layout->addWidget(controls);
                       layout->addWidget(makeStatusLabel(host, QStringLiteral("Interactive: true, dragMargin: 36")));
                       layout->addStretch(1);

                       auto* drawer = new DrawerView(host);
                       drawer->setEdge(DrawerView::DrawerEdge::Left);
                       drawer->setDrawerLength(210);
                       drawer->setModal(false);
                       drawer->setDim(false);
                       drawer->setInteractive(true);
                       drawer->setDragMargin(36);
                       drawer->setAnimationEnabled(true);
                       drawer->setContentWidget(new DrawerGradientPane(QStringLiteral("Drag surface"),
                                                                       QColor(0x2F, 0x9E, 0x44),
                                                                       QColor(0xA9, 0xE3, 0x4B),
                                                                       DrawerView::DrawerEdge::Left,
                                                                       drawer));

                       QObject::connect(openButton, &Button::clicked, drawer, &DrawerView::open);
                       QObject::connect(closeButton, &Button::clicked, drawer, &DrawerView::close);
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
                   }),
        makeSample(QStringLiteral("flip-view-vertical"),
                   QStringLiteral("Vertical orientation"),
                   QStringLiteral("Orientation changes which axis wheel, swipe, and arrow navigation use."),
                   QStringLiteral("auto* flipView = new FlipView(this);\n"
                                  "flipView->setOrientation(Qt::Vertical);\n"
                                  "flipView->setShowPageIndicator(true);\n"
                                  "flipView->addPage(firstPage);\n"
                                  "flipView->addPage(secondPage);"),
                   [](QWidget* parent) {
                       auto* flipView = new FlipView(parent);
                       flipView->setFixedSize(300, 240);
                       flipView->setOrientation(Qt::Vertical);
                       flipView->setShowPageIndicator(true);

                       const QVector<QPair<QColor, QColor>> colors{
                           {QColor(0x87, 0x64, 0xB8), QColor(0xC2, 0x6F, 0xB8)},
                           {QColor(0x03, 0x83, 0x87), QColor(0x6F, 0xD1, 0xF2)},
                           {QColor(0xCA, 0x50, 0x10), QColor(0xF2, 0xC9, 0x4C)}};
                       for (int i = 0; i < colors.size(); ++i) {
                           auto* page = makeGradientPane(flipView,
                                                         QStringLiteral("Vertical %1").arg(i + 1),
                                                         colors.at(i).first,
                                                         colors.at(i).second);
                           page->setFixedSize(300, 240);
                           flipView->addPage(page);
                       }
                       return flipView;
                   }),
        makeSample(QStringLiteral("flip-view-external-navigation"),
                   QStringLiteral("External navigation buttons"),
                   QStringLiteral("Navigation chrome can be hidden while the app drives currentIndex with its own commands."),
                   QStringLiteral("flipView->setShowNavigationButtons(false);\n"
                                  "flipView->setShowPageIndicator(false);\n"
                                  "QObject::connect(previous, &Button::clicked,\n"
                                  "                 flipView, &FlipView::goPrevious);\n"
                                  "QObject::connect(next, &Button::clicked,\n"
                                  "                 flipView, &FlipView::goNext);\n"
                                  "QObject::connect(flipView, &FlipView::currentIndexChanged,\n"
                                  "                 status, updateStatus);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* flipView = new FlipView(group);
                       flipView->setFixedSize(360, 168);
                       flipView->setShowNavigationButtons(false);
                       flipView->setShowPageIndicator(false);

                       const QVector<QPair<QColor, QColor>> colors{
                           {QColor(0x1E, 0x6F, 0xD9), QColor(0x6F, 0xD1, 0xF2)},
                           {QColor(0x2F, 0x9E, 0x44), QColor(0xA9, 0xE3, 0x4B)},
                           {QColor(0xF7, 0x97, 0x5B), QColor(0xF2, 0xC9, 0x4C)}};
                       for (int i = 0; i < colors.size(); ++i) {
                           auto* page = makeGradientPane(flipView,
                                                         QStringLiteral("Page %1").arg(i + 1),
                                                         colors.at(i).first,
                                                         colors.at(i).second);
                           page->setFixedSize(360, 168);
                           flipView->addPage(page);
                       }

                       QWidget* controls = horizontalGroup(group, 8);
                       auto* previous = new Button(QStringLiteral("Previous"), controls);
                       auto* next = new Button(QStringLiteral("Next"), controls);
                       next->setFluentStyle(Button::Accent);
                       auto* status = makeStatusLabel(controls, QStringLiteral("Current page: 1"));
                       controls->layout()->addWidget(previous);
                       controls->layout()->addWidget(next);
                       controls->layout()->addWidget(status);

                       const auto updateStatus = [status](int index) {
                           status->setText(QStringLiteral("Current page: %1").arg(index + 1));
                       };
                       QObject::connect(previous, &Button::clicked, flipView, &FlipView::goPrevious);
                       QObject::connect(next, &Button::clicked, flipView, &FlipView::goNext);
                       QObject::connect(flipView, &FlipView::currentIndexChanged,
                                        status, updateStatus);

                       group->layout()->addWidget(flipView);
                       group->layout()->addWidget(controls);
                       return group;
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
                           {QStringLiteral("Atrium"), QStringLiteral("architecture"), QColor(0x37, 0x8B, 0xC4), QColor(0x9A, 0xD9, 0xEA), QSize(160, 118)},
                           {QStringLiteral("Harbor"), QStringLiteral("travel"), QColor(0x1E, 0x6F, 0xD9), QColor(0x67, 0xD0, 0xD6), QSize(160, 118)},
                           {QStringLiteral("Canyon"), QStringLiteral("landscape"), QColor(0xC2, 0x5B, 0x2B), QColor(0xF2, 0xB8, 0x4B), QSize(160, 118)},
                           {QStringLiteral("Studio"), QStringLiteral("workspace"), QColor(0x7A, 0x5F, 0xC9), QColor(0xD7, 0x94, 0xE6), QSize(160, 118)},
                           {QStringLiteral("Garden"), QStringLiteral("nature"), QColor(0x2F, 0x9E, 0x44), QColor(0xA9, 0xE3, 0x4B), QSize(160, 118)},
                           {QStringLiteral("Dawn"), QStringLiteral("morning"), QColor(0xD9, 0x6C, 0x75), QColor(0xF2, 0xC9, 0x6B), QSize(160, 118)},
                           {QStringLiteral("Transit"), QStringLiteral("city"), QColor(0x37, 0x5B, 0xA8), QColor(0x58, 0xB7, 0xE8), QSize(160, 118)},
                           {QStringLiteral("Market"), QStringLiteral("street"), QColor(0x91, 0x43, 0x4A), QColor(0xE0, 0x95, 0x52), QSize(160, 118)},
                           {QStringLiteral("Cabin"), QStringLiteral("retreat"), QColor(0x5B, 0x75, 0x35), QColor(0xCE, 0xA8, 0x5B), QSize(160, 118)}};

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
                           {QStringLiteral("Loft"), QStringLiteral("interior"), QColor(0x5D, 0x7F, 0xB8), QColor(0xC9, 0xDB, 0xF2), QSize(148, 104)},
                           {QStringLiteral("Ridge"), QStringLiteral("wide"), QColor(0x24, 0x74, 0x8F), QColor(0x8C, 0xCF, 0xA5), QSize(210, 118)},
                           {QStringLiteral("Cafe"), QStringLiteral("street"), QColor(0xA9, 0x5B, 0x45), QColor(0xE9, 0xB8, 0x7A), QSize(132, 132)},
                           {QStringLiteral("Mist"), QStringLiteral("forest"), QColor(0x3C, 0x75, 0x52), QColor(0xAF, 0xC9, 0x8E), QSize(172, 148)},
                           {QStringLiteral("Gallery"), QStringLiteral("exhibit"), QColor(0x66, 0x6A, 0x86), QColor(0xD9, 0xD7, 0xEA), QSize(122, 104)},
                           {QStringLiteral("Canal"), QStringLiteral("water"), QColor(0x2D, 0x73, 0xA3), QColor(0x9A, 0xD8, 0xE5), QSize(190, 126)},
                           {QStringLiteral("Trail"), QStringLiteral("nature"), QColor(0x70, 0x83, 0x2F), QColor(0xE2, 0xC4, 0x58), QSize(156, 116)},
                           {QStringLiteral("Arcade"), QStringLiteral("night"), QColor(0x57, 0x3A, 0x9B), QColor(0xDC, 0x72, 0xC8), QSize(198, 140)},
                           {QStringLiteral("Pier"), QStringLiteral("coast"), QColor(0x2F, 0x80, 0x8F), QColor(0xE0, 0xC1, 0x7E), QSize(136, 106)}};

                       auto* model = makeFlowPhotoModel(flowView, photos);
                       flowView->setModel(model);
                       flowView->setSelectedIndex(1);

                       auto* network = new QNetworkAccessManager(flowView);
                       loadFlowNetworkImages(model, photos, network);
                       return flowView;
                   }),
        makeSample(QStringLiteral("flow-view-scroll-bounce"),
                   QStringLiteral("Contained scrolling and bounce"),
                   QStringLiteral("FlowView consumes boundary wheel input inside nested Gallery scrollers while keeping the elastic edge feedback visible."),
                   QStringLiteral("flowView->setScrollChainingEnabled(false);\n"
                                  "flowView->setOverscrollEnabled(true);\n"
                                  "flowView->setDefaultItemSize(QSize(126, 88));\n"
                                  "flowView->setModel(model);"),
                   [](QWidget* parent) {
                       auto* flowView = new FlowView(parent);
                       flowView->setFixedSize(420, 238);
                       flowView->setHeaderText(QStringLiteral("Contained flow"));
                       flowView->setDefaultItemSize(QSize(126, 88));
                       flowView->setMinimumItemSize(QSize(112, 80));
                       flowView->setMaximumItemSize(QSize(146, 102));
                       flowView->setHorizontalSpacing(10);
                       flowView->setVerticalSpacing(10);
                       flowView->setContentMargins(QMargins(8, 8, 8, 8));
                       flowView->setScrollChainingEnabled(false);
                       flowView->setOverscrollEnabled(true);
                       flowView->setItemDelegate(new FlowPhotoDelegate(
                           static_cast<fluent::FluentElement*>(flowView), flowView));

                       QVector<FlowPhotoInfo> photos;
                       for (int i = 0; i < 18; ++i) {
                           const QColor from = accentPalette().at(i % accentPalette().size());
                           const QColor to = accentPalette().at((i + 2) % accentPalette().size()).lighter(135);
                          photos.append({QStringLiteral("Tile %1").arg(i + 1),
                                          QStringLiteral("scroll"),
                                          from,
                                          to,
                                          QSize(126 + (i % 3) * 12, 88 + (i % 2) * 12)});
                       }
                       auto* model = makeFlowPhotoModel(flowView, photos);
                       flowView->setModel(model);
                       flowView->setSelectedIndex(0);
                       return flowView;
                   }),
        makeSample(QStringLiteral("flow-view-placeholder"),
                   QStringLiteral("Header and placeholder"),
                   QStringLiteral("When the model is empty, placeholderText occupies the flow content area below the header."),
                   QStringLiteral("auto* flowView = new FlowView(this);\n"
                                  "flowView->setHeaderText(\"Uploads\");\n"
                                  "flowView->setPlaceholderText(\"No queued uploads\");\n"
                                  "flowView->setModel(new QStandardItemModel(flowView));"),
                   [](QWidget* parent) {
                       auto* flowView = new FlowView(parent);
                       flowView->setFixedSize(420, 178);
                       flowView->setHeaderText(QStringLiteral("Uploads"));
                       flowView->setPlaceholderText(QStringLiteral("No queued uploads"));
                       flowView->setModel(new QStandardItemModel(flowView));
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
        {QStringLiteral("Sunrise"), QStringLiteral("Warm"), QColor(0xF7, 0x97, 0x5B), QColor(0xF2, 0xC9, 0x4C), cell},
        {QStringLiteral("Ocean"), QStringLiteral("Blue"), QColor(0x1E, 0x6F, 0xD9), QColor(0x6F, 0xD1, 0xF2), cell},
        {QStringLiteral("Forest"), QStringLiteral("Green"), QColor(0x2F, 0x9E, 0x44), QColor(0xA9, 0xE3, 0x4B), cell},
        {QStringLiteral("Dusk"), QStringLiteral("Violet"), QColor(0x6B, 0x4F, 0xA2), QColor(0xC2, 0x6F, 0xB8), cell},
        {QStringLiteral("Desert"), QStringLiteral("Amber"), QColor(0xC8, 0x6B, 0x2D), QColor(0xE8, 0xC0, 0x6E), cell},
        {QStringLiteral("Glacier"), QStringLiteral("Ice"), QColor(0x3D, 0x8B, 0xA3), QColor(0xB3, 0xE5, 0xE8), cell},
        {QStringLiteral("Meadow"), QStringLiteral("Spring"), QColor(0x6F, 0xA8, 0x2F), QColor(0xD4, 0xE6, 0x7A), cell},
        {QStringLiteral("Harbor"), QStringLiteral("Teal"), QColor(0x1C, 0x6E, 0x8C), QColor(0x73, 0xC8, 0xD0), cell}};

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
                   }),
        makeSample(QStringLiteral("grid-view-scroll-bounce"),
                   QStringLiteral("Scroll chaining and overscroll"),
                   QStringLiteral("A nested GridView keeps wheel input inside the control at scroll boundaries and still shows elastic bounce feedback."),
                   QStringLiteral("gridView->setScrollChainingEnabled(false);\n"
                                  "gridView->setOverscrollEnabled(true);\n"
                                  "gridView->setCellSize(QSize(118, 88));\n"
                                  "gridView->setMaxColumns(3);\n"
                                  "gridView->setModel(model);"),
                   [](QWidget* parent) {
                       auto* gridView = new GridView(parent);
                       gridView->setFixedSize(410, 238);
                       gridView->setHeaderText(QStringLiteral("Contained grid"));
                       gridView->setCellSize(QSize(118, 88));
                       gridView->setMaxColumns(3);
                       gridView->setHorizontalSpacing(10);
                       gridView->setVerticalSpacing(10);
                       gridView->setScrollChainingEnabled(false);
                       gridView->setOverscrollEnabled(true);
                       gridView->setItemDelegate(new GridPhotoDelegate(
                           static_cast<fluent::FluentElement*>(gridView), gridView, gridView));

                       QVector<FlowPhotoInfo> photos;
                       for (int i = 0; i < 18; ++i) {
                           photos.append({QStringLiteral("Cell %1").arg(i + 1),
                                          QStringLiteral("grid"),
                                          accentPalette().at(i % accentPalette().size()),
                                          accentPalette().at((i + 3) % accentPalette().size()).lighter(135),
                                          QSize(118, 88)});
                       }
                       auto* model = makeFlowPhotoModel(gridView, photos);
                       gridView->setModel(model);
                       gridView->setSelectedIndex(0);
                       return gridView;
                   }),
        makeSample(QStringLiteral("grid-view-placeholder"),
                   QStringLiteral("Header and placeholder"),
                   QStringLiteral("Header text and placeholder text stay visible even when the grid has no items."),
                   QStringLiteral("auto* gridView = new GridView(this);\n"
                                  "gridView->setHeaderText(\"Pinned photos\");\n"
                                  "gridView->setPlaceholderText(\"No pinned photos\");\n"
                                  "gridView->setModel(new QStandardItemModel(gridView));"),
                   [](QWidget* parent) {
                       auto* gridView = new GridView(parent);
                       gridView->setFixedSize(410, 178);
                       gridView->setHeaderText(QStringLiteral("Pinned photos"));
                       gridView->setPlaceholderText(QStringLiteral("No pinned photos"));
                       gridView->setModel(new QStandardItemModel(gridView));
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
                                  "    item->setData(initialsAvatar(contact),\n"
                                  "                  Qt::DecorationRole);\n"
                                  "    model->appendRow(item);\n"
                                  "}\n"
                                  "listView->setModel(model);\n"
                                  "listView->setHeaderText(\"Contacts\");"),
                   [](QWidget* parent) {
                       auto* listView = flatPreviewSurface(new ListView(parent));
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
                           item->setData(initialsAvatar(
                               name, accentPalette().at(colorIndex++ % accentPalette().size())),
                                         Qt::DecorationRole);
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
                       auto* listView = flatPreviewSurface(new ListView(parent));
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
                       auto* listView = flatPreviewSurface(new ListView(parent));
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
                       auto* listView = flatPreviewSurface(new ListView(parent));
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
                   }),
        makeSample(QStringLiteral("list-view-sections"),
                   QStringLiteral("Section headers"),
                   QStringLiteral("sectionEnabled groups adjacent rows by a caller-provided key while keeping normal row selection."),
                   QStringLiteral("listView->setSectionEnabled(true);\n"
                                  "listView->setSectionKeyFunction([](int row) {\n"
                                  "    return row < 3 ? \"Today\" : \"Earlier\";\n"
                                  "});\n"
                                  "listView->setModel(model);"),
                   [](QWidget* parent) {
                       auto* listView = flatPreviewSurface(new ListView(parent));
                       listView->setFixedSize(340, 252);
                       listView->setHeaderText(QStringLiteral("Notifications"));
                       listView->setIconSize(QSize(24, 24));
                       listView->setItemDelegate(new ListRowDelegate(
                           static_cast<fluent::FluentElement*>(listView), listView, listView));
                       listView->setSectionEnabled(true);
                       listView->setSectionKeyFunction([](int row) {
                           if (row < 3)
                               return QStringLiteral("Today");
                           if (row < 6)
                               return QStringLiteral("Yesterday");
                           return QStringLiteral("Earlier");
                       });
                       listView->setModel(makeGlyphListModel(
                           listView,
                           {{QStringLiteral("Build completed"), Typography::Icons::CheckMark},
                            {QStringLiteral("New comment"), Typography::Icons::Message},
                            {QStringLiteral("Meeting starts soon"), Typography::Icons::Calendar},
                            {QStringLiteral("Pull request updated"), Typography::Icons::Document},
                            {QStringLiteral("File synced"), Typography::Icons::Sync},
                            {QStringLiteral("Download ready"), Typography::Icons::Download},
                            {QStringLiteral("Reminder"), Typography::Icons::Clock},
                            {QStringLiteral("Settings changed"), Typography::Icons::Settings}},
                           24));
                       listView->setSelectedIndex(1);
                       return listView;
                   }),
        makeSample(QStringLiteral("list-view-scroll-chaining"),
                   QStringLiteral("Contained vertical scrolling"),
                   QStringLiteral("scrollChainingEnabled keeps boundary wheel input from leaking to the surrounding Gallery page while ListView handles its own bounce."),
                   QStringLiteral("auto* listView = new ListView(this);\n"
                                  "listView->setScrollChainingEnabled(false);\n"
                                  "listView->setHeaderText(\"Queue\");\n"
                                  "listView->setModel(model);"),
                   [](QWidget* parent) {
                       auto* listView = flatPreviewSurface(new ListView(parent));
                       listView->setFixedSize(340, 238);
                       listView->setHeaderText(QStringLiteral("Queue"));
                       listView->setFooterText(QStringLiteral("Wheel input stays in this ListView"));
                       listView->setScrollChainingEnabled(false);
                       listView->setIconSize(QSize(24, 24));
                       listView->setItemDelegate(new ListRowDelegate(
                           static_cast<fluent::FluentElement*>(listView), listView, listView));

                       QVector<QPair<QString, QString>> rows;
                       for (int i = 0; i < 20; ++i)
                           rows.append({QStringLiteral("Queued item %1").arg(i + 1),
                                        i % 2 == 0 ? Typography::Icons::Document
                                                   : Typography::Icons::Download});
                       listView->setModel(makeGlyphListModel(listView, rows, 24));
                       listView->setSelectedIndex(0);
                       return listView;
                   }),
        makeSample(QStringLiteral("list-view-placeholder"),
                   QStringLiteral("Header, footer, and placeholder"),
                   QStringLiteral("The convenience text API covers the empty state without requiring a custom model delegate."),
                   QStringLiteral("listView->setHeaderText(\"Downloads\");\n"
                                  "listView->setFooterText(\"0 items\");\n"
                                  "listView->setPlaceholderText(\"No downloads yet\");\n"
                                  "listView->setModel(new QStandardItemModel(listView));"),
                   [](QWidget* parent) {
                       auto* listView = flatPreviewSurface(new ListView(parent));
                       listView->setFixedSize(340, 178);
                       listView->setHeaderText(QStringLiteral("Downloads"));
                       listView->setFooterText(QStringLiteral("0 items"));
                       listView->setPlaceholderText(QStringLiteral("No downloads yet"));
                       listView->setModel(new QStandardItemModel(listView));
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
                   }),
        makeSample(QStringLiteral("split-view-vertical-constraints"),
                   QStringLiteral("Vertical panes and constraints"),
                   QStringLiteral("Orientation changes the resize axis; minimum, preferred, and maximum sizes clamp pane geometry on that axis."),
                   QStringLiteral("auto* splitView = new SplitView(this);\n"
                                  "splitView->setOrientation(Qt::Vertical);\n"
                                  "splitView->addPane(topPane, {80, 110, 150, false});\n"
                                  "splitView->addPane(fillPane, {90, 160, 500, true});\n"
                                  "splitView->addPane(bottomPane, {60, 120, 140, false});"),
                   [](QWidget* parent) {
                       auto* splitView = new SplitView(parent);
                       splitView->setFixedSize(360, 320);
                       splitView->setOrientation(Qt::Vertical);
                       splitView->setHandleWidth(6);
                       splitView->addPane(makeGradientPane(splitView, QStringLiteral("Top 110"),
                                                           QColor(0x1E, 0x6F, 0xD9),
                                                           QColor(0x6F, 0xD1, 0xF2)),
                                          SplitViewPaneOptions{80, 110, 150, false});
                       splitView->addPane(makeGradientPane(splitView, QStringLiteral("Fill pane"),
                                                           QColor(0x2F, 0x9E, 0x44),
                                                           QColor(0xA9, 0xE3, 0x4B)),
                                          SplitViewPaneOptions{90, 160, 500, true});
                       splitView->addPane(makeGradientPane(splitView, QStringLiteral("Bottom max 140"),
                                                           QColor(0xF7, 0x97, 0x5B),
                                                           QColor(0xF2, 0xC9, 0x4C)),
                                          SplitViewPaneOptions{60, 120, 140, false});
                       return splitView;
                   }),
        makeSample(QStringLiteral("split-view-hidden-pane"),
                   QStringLiteral("Hidden panes do not reserve space"),
                   QStringLiteral("When a pane is hidden, SplitView removes its geometry and handle until the pane is shown again."),
                   QStringLiteral("splitView->addPane(firstPane, {60, 120, 240, false});\n"
                                  "splitView->addPane(detailsPane, {60, 150, 260, false});\n"
                                  "splitView->addPane(fillPane, {60, 180, 500, true});\n\n"
                                  "QObject::connect(toggle, &Button::clicked, [detailsPane]() {\n"
                                  "    detailsPane->setVisible(!detailsPane->isVisible());\n"
                                  "});"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* splitView = new SplitView(group);
                       splitView->setFixedSize(460, 160);
                       auto* first = makeGradientPane(splitView, QStringLiteral("Nav"),
                                                      QColor(0x1E, 0x6F, 0xD9),
                                                      QColor(0x6F, 0xD1, 0xF2));
                       auto* details = makeGradientPane(splitView, QStringLiteral("Details"),
                                                        QColor(0x87, 0x64, 0xB8),
                                                        QColor(0xC2, 0x6F, 0xB8));
                       auto* content = makeGradientPane(splitView, QStringLiteral("Content"),
                                                        QColor(0x2F, 0x9E, 0x44),
                                                        QColor(0xA9, 0xE3, 0x4B));
                       splitView->addPane(first, SplitViewPaneOptions{60, 120, 240, false});
                       splitView->addPane(details, SplitViewPaneOptions{60, 150, 260, false});
                       splitView->addPane(content, SplitViewPaneOptions{60, 180, 500, true});

                       QWidget* controls = horizontalGroup(group, 8);
                       auto* toggle = new Button(QStringLiteral("Hide details"), controls);
                       toggle->setFluentStyle(Button::Accent);
                       auto* status = makeStatusLabel(controls, QStringLiteral("Pane count: 3, details visible"));
                       controls->layout()->addWidget(toggle);
                       controls->layout()->addWidget(status);
                       QObject::connect(toggle, &Button::clicked, splitView,
                                        [details, toggle, status]() {
                                            details->setVisible(!details->isVisible());
                                            toggle->setText(details->isVisible()
                                                            ? QStringLiteral("Hide details")
                                                            : QStringLiteral("Show details"));
                                            status->setText(details->isVisible()
                                                            ? QStringLiteral("Pane count: 3, details visible")
                                                            : QStringLiteral("Pane count: 3, details hidden"));
                                        });

                       group->layout()->addWidget(splitView);
                       group->layout()->addWidget(controls);
                       return group;
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
                                  "stackView->pop();\n"
                                  "QObject::connect(stackView, &StackView::depthChanged,\n"
                                  "                 status, updateStatus);"),
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
                       auto* status = makeStatusLabel(buttons, QStringLiteral("Depth: 1"));
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
                       QObject::connect(stackView, &StackView::depthChanged,
                                        status, [status](int depth) {
                                            status->setText(QStringLiteral("Depth: %1").arg(depth));
                                        });
                       buttons->layout()->addWidget(pushButton);
                       buttons->layout()->addWidget(popButton);
                       buttons->layout()->addWidget(status);

                       group->layout()->addWidget(stackView);
                       group->layout()->addWidget(buttons);
                       return group;
                   }),
        makeSample(QStringLiteral("stack-view-transition-type"),
                   QStringLiteral("Transition type"),
                   QStringLiteral("The transition type changes the visual effect used when pages enter and leave the stack."),
                   QStringLiteral("stackView->setTransitionDuration(220);\n"
                                  "stackView->setTransitionType(\n"
                                  "    StackView::StackViewTransitionType::ScaleFade);\n"
                                  "stackView->push(nextPage);\n"
                                  "stackView->pop();"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* stackView = new StackView(group);
                       stackView->setFixedSize(360, 150);
                       stackView->setTransitionDuration(220);
                       stackView->setTransitionType(StackView::StackViewTransitionType::ScaleFade);
                       stackView->setInitialItem(makeGradientPane(stackView, QStringLiteral("Root"),
                                                                  QColor(0x1E, 0x6F, 0xD9),
                                                                  QColor(0x6F, 0xD1, 0xF2)));

                       QWidget* controls = horizontalGroup(group, 8);
                       auto* scale = new Button(QStringLiteral("ScaleFade"), controls);
                       auto* slide = new Button(QStringLiteral("SlideFade"), controls);
                       auto* push = new Button(QStringLiteral("Push"), controls);
                       push->setFluentStyle(Button::Accent);
                       auto* pop = new Button(QStringLiteral("Pop"), controls);
                       controls->layout()->addWidget(scale);
                       controls->layout()->addWidget(slide);
                       controls->layout()->addWidget(push);
                       controls->layout()->addWidget(pop);

                       QObject::connect(scale, &Button::clicked, stackView, [stackView]() {
                           stackView->setTransitionType(StackView::StackViewTransitionType::ScaleFade);
                       });
                       QObject::connect(slide, &Button::clicked, stackView, [stackView]() {
                           stackView->setTransitionType(StackView::StackViewTransitionType::SlideFade);
                       });
                       QObject::connect(push, &Button::clicked, stackView, [stackView]() {
                           const int depth = stackView->depth();
                           stackView->push(makeGradientPane(stackView,
                                                            QStringLiteral("Page %1").arg(depth + 1),
                                                            QColor(0x87, 0x64, 0xB8),
                                                            QColor(0xC2, 0x6F, 0xB8)));
                       });
                       QObject::connect(pop, &Button::clicked, stackView, [stackView]() {
                           stackView->pop();
                       });

                       group->layout()->addWidget(stackView);
                       group->layout()->addWidget(controls);
                       return group;
                   }),
        makeSample(QStringLiteral("stack-view-replace-pop-to-root"),
                   QStringLiteral("Replace and popToRoot"),
                   QStringLiteral("StackView can replace the current page or unwind directly back to the root page."),
                   QStringLiteral("stackView->setTransitionAnimationEnabled(false);\n"
                                  "stackView->setInitialItem(rootPage);\n"
                                  "stackView->push(detailsPage);\n"
                                  "stackView->replace(replacementPage);\n"
                                  "stackView->popToRoot();"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* stackView = new StackView(group);
                       stackView->setFixedSize(360, 150);
                       stackView->setTransitionAnimationEnabled(false);
                       stackView->setInitialItem(makeGradientPane(stackView, QStringLiteral("Root"),
                                                                  QColor(0x1E, 0x6F, 0xD9),
                                                                  QColor(0x6F, 0xD1, 0xF2)));
                       stackView->push(makeGradientPane(stackView, QStringLiteral("Details"),
                                                        QColor(0x2F, 0x9E, 0x44),
                                                        QColor(0xA9, 0xE3, 0x4B)));

                       QWidget* controls = horizontalGroup(group, 8);
                       auto* replace = new Button(QStringLiteral("Replace current"), controls);
                       replace->setFluentStyle(Button::Accent);
                       auto* popRoot = new Button(QStringLiteral("Pop to root"), controls);
                       auto* status = makeStatusLabel(controls, QStringLiteral("Depth: 2"));
                       controls->layout()->addWidget(replace);
                       controls->layout()->addWidget(popRoot);
                       controls->layout()->addWidget(status);

                       QObject::connect(replace, &Button::clicked, stackView, [stackView]() {
                           stackView->replace(makeGradientPane(stackView, QStringLiteral("Replacement"),
                                                               QColor(0xF7, 0x97, 0x5B),
                                                               QColor(0xF2, 0xC9, 0x4C)));
                       });
                       QObject::connect(popRoot, &Button::clicked, stackView, [stackView]() {
                           stackView->popToRoot();
                       });
                       QObject::connect(stackView, &StackView::depthChanged,
                                        status, [status](int depth) {
                                            status->setText(QStringLiteral("Depth: %1").arg(depth));
                                        });

                       group->layout()->addWidget(stackView);
                       group->layout()->addWidget(controls);
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
                       auto* tree = flatPreviewSurface(new TreeView(parent));
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
                       auto* tree = flatPreviewSurface(new TreeView(parent));
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
                       auto* tree = flatPreviewSurface(new TreeView(parent));
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(252);
                       tree->setCanReorderItems(true);
                       tree->setItemDelegate(new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree));

                       auto* model = makeFolderTreeModel(tree, folderColor, fileColor);
                       tree->setModel(model);
                       tree->expandAll();
                       return tree;
                   }),
        makeSample(QStringLiteral("tree-view-indicator-motion"),
                   QStringLiteral("Selection indicator motion"),
                   QStringLiteral("Selecting rows at different depths updates the indicator direction and hierarchy transition."),
                   QStringLiteral("tree->setSelectionIndicatorVisible(true);\n"
                                  "tree->setIndicatorMotionAnimationEnabled(true);\n"
                                  "\n"
                                  "const QModelIndex parentIndex = model->index(0, 0);\n"
                                  "const QModelIndex childIndex = model->index(0, 0, parentIndex);\n"
                                  "const QModelIndex siblingIndex = model->index(1, 0);\n"
                                  "tree->setSelectedItem(parentIndex);\n"
                                  "\n"
                                  "auto bindTarget = [tree](Button* button, const QModelIndex& index) {\n"
                                  "    QObject::connect(button, &Button::clicked, tree,\n"
                                  "                     [tree, index] { tree->setSelectedItem(index); });\n"
                                  "};\n"
                                  "bindTarget(parentButton, parentIndex);\n"
                                  "bindTarget(childButton, childIndex);\n"
                                  "bindTarget(siblingButton, siblingIndex);\n"
                                  "\n"
                                  "auto updateStatus = [tree, status] { /* refresh transition label */ };\n"
                                  "QObject::connect(tree, &TreeView::indicatorHierarchyTransitionChanged,\n"
                                  "                 status, updateStatus);"),
                   [folderColor, fileColor, rowHeight](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* tree = flatPreviewSurface(new TreeView(group));
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(238);
                       tree->setSelectionIndicatorVisible(true);
                       tree->setIndicatorMotionAnimationEnabled(true);
                       tree->setItemDelegate(new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree));

                       auto* model = makeFolderTreeModel(tree, folderColor, fileColor);
                       tree->setModel(model);
                       tree->expandAll();
                       const QModelIndex parentIndex = model->index(0, 0);
                       const QModelIndex childIndex = model->index(0, 0, parentIndex);
                       const QModelIndex siblingIndex = model->index(1, 0);
                       tree->setSelectedItem(parentIndex);

                       QWidget* controls = horizontalGroup(group, 8);
                       auto* parentButton = new Button(QStringLiteral("Parent"), controls);
                       auto* childButton = new Button(QStringLiteral("Child"), controls);
                       auto* siblingButton = new Button(QStringLiteral("Sibling"), controls);
                       auto* status = makeStatusLabel(controls, QStringLiteral("Transition: none"));
                       // Reserve room for the LONGEST transition text up front. This label has no width
                       // floor, so its width tracks the text, which changes length per selection
                       // ("none" → "same level"); the label — and the left-aligned group it shares with the
                       // tree — would then grow/shrink, visibly jumping the TreeView's width and its
                       // translucent backdrop on every selection. zh_CN: 预留「最长过渡文案」的宽度。该标签无宽度下限,
                       // 宽度随文本变化,而文本随选择变化("none"→"same level"),于是标签——以及它与 tree 共处的左对齐 group——
                       // 会伸缩,使每次选择时 TreeView 宽度及其半透明背景跳动。
                       status->setText(QStringLiteral("Transition: same level"));
                       status->setMinimumWidth(qMax(status->minimumWidth(), status->sizeHint().width()));
                       status->setText(QStringLiteral("Transition: none"));
                       controls->layout()->addWidget(parentButton);
                       controls->layout()->addWidget(childButton);
                       controls->layout()->addWidget(siblingButton);
                       controls->layout()->addWidget(status);

                       const auto transitionText = [tree]() {
                           switch (tree->indicatorHierarchyTransition()) {
                           case TreeView::IndicatorHierarchyTransition::Inward:
                               return QStringLiteral("inward");
                           case TreeView::IndicatorHierarchyTransition::Outward:
                               return QStringLiteral("outward");
                           case TreeView::IndicatorHierarchyTransition::SameLevel:
                               return QStringLiteral("same level");
                           case TreeView::IndicatorHierarchyTransition::None:
                               return QStringLiteral("none");
                           }
                           return QStringLiteral("none");
                       };
                       const auto updateStatus = [status, transitionText]() {
                           status->setText(QStringLiteral("Transition: %1").arg(transitionText()));
                       };
                       QObject::connect(parentButton, &Button::clicked, tree, [tree, parentIndex]() {
                           tree->setSelectedItem(parentIndex);
                       });
                       QObject::connect(childButton, &Button::clicked, tree, [tree, childIndex]() {
                           tree->setSelectedItem(childIndex);
                       });
                       QObject::connect(siblingButton, &Button::clicked, tree, [tree, siblingIndex]() {
                           tree->setSelectedItem(siblingIndex);
                       });
                       QObject::connect(tree, &TreeView::indicatorHierarchyTransitionChanged,
                                        status, updateStatus);
                       updateStatus();

                       group->layout()->addWidget(tree);
                       group->layout()->addWidget(controls);
                       return group;
                   }),
        makeSample(QStringLiteral("tree-view-scroll-bounce"),
                   QStringLiteral("Contained tree scrolling"),
                   QStringLiteral("TreeView exposes the same scrollChaining and overscroll controls as the wrapped collection views."),
                   QStringLiteral("tree->setScrollChainingEnabled(false);\n"
                                  "tree->setOverscrollEnabled(true);\n"
                                  "tree->setModel(model);\n"
                                  "tree->expandAll();"),
                   [folderColor, fileColor, rowHeight](QWidget* parent) {
                       auto* tree = flatPreviewSurface(new TreeView(parent));
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(258);
                       tree->setScrollChainingEnabled(false);
                       tree->setOverscrollEnabled(true);
                       tree->setItemDelegate(new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree));

                       auto* model = new QStandardItemModel(tree);
                       for (int folderIndex = 0; folderIndex < 9; ++folderIndex) {
                           auto* folder = makeTreeItem(QStringLiteral("Folder %1").arg(folderIndex + 1),
                                                       Typography::Icons::Folder,
                                                       folderColor);
                           for (int fileIndex = 0; fileIndex < 3; ++fileIndex) {
                               folder->appendRow(makeTreeItem(
                                   QStringLiteral("Document %1.%2").arg(folderIndex + 1).arg(fileIndex + 1),
                                   Typography::Icons::Document,
                                   fileColor));
                           }
                           model->appendRow(folder);
                       }
                       tree->setModel(model);
                       tree->expandAll();
                       tree->setSelectedItem(model->index(0, 0));
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

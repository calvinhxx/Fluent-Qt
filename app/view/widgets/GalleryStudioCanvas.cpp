#include "view/widgets/GalleryStudioCanvas.h"
#include "view/widgets/GalleryComponentPlayground.h"

#include <FluentQt/FluentQt.h>
#if defined(FLUENT_QT_HAS_PARTICLES)
#include <FluentQt/Particles.h>
#endif
#if defined(FLUENT_QT_HAS_SPATIAL)
#include <FluentQt/Spatial.h>
#include <QPolygonF>
#include <QVector3D>
#endif
#if defined(FLUENT_QT_HAS_QML_BRIDGE)
#include <FluentQt/QmlBridge.h>
#include <QUrl>
#endif

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLayout>
#include <QLayoutItem>
#include <QLocale>
#include <QMargins>
#include <QPainter>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>
#include <functional>

namespace fluent::gallery {
namespace {

using fluent::FluentElement;
using fluent::WidgetOwnership;
using namespace fluent;
using namespace fluent::basicinput;
using namespace fluent::charts;
using namespace fluent::collections;
using namespace fluent::date_time;
using namespace fluent::dialogs_flyouts;
using namespace fluent::layout;
using namespace fluent::menus_toolbars;
using namespace fluent::navigation;
using namespace fluent::overlay;
using namespace fluent::scrolling;
using namespace fluent::status_info;
using namespace fluent::textfields;
using namespace fluent::windowing;

const QSet<QString>& containerTypes()
{
    static const QSet<QString> types = {
        QStringLiteral("VBox"),
        QStringLiteral("HBox"),
        QStringLiteral("Grid"),
        QStringLiteral("AnchorBox"),
        QStringLiteral("Card"),
        QStringLiteral("Expander"),
        QStringLiteral("Accordion"),
        QStringLiteral("Field"),
        QStringLiteral("ScrollView"),
        QStringLiteral("SplitView"),
        QStringLiteral("FlipView"),
        QStringLiteral("StackView"),
        QStringLiteral("ListDetailView"),
        QStringLiteral("NavigationView"),
        QStringLiteral("Window"),
        QStringLiteral("ParticleBackdrop"),
        QStringLiteral("SpatialView"),
        QStringLiteral("SpatialItem"),
    };
    return types;
}

void deactivateEmbeddedSessions(QWidget* root)
{
    if (!root)
        return;
#if defined(FLUENT_QT_HAS_SPATIAL)
    for (auto* session : root->findChildren<fluent::spatial::SpatialSession*>())
        session->deactivate();
#endif
#if defined(FLUENT_QT_HAS_QML_BRIDGE)
    for (auto* session : root->findChildren<fluent::qmlbridge::QmlSession*>())
        session->deactivate();
#endif
}

class StudioSessionHost final : public QWidget {
public:
    using QWidget::QWidget;
    ~StudioSessionHost() override { deactivateEmbeddedSessions(this); }
};

#if defined(FLUENT_QT_HAS_SPATIAL)
void applySpatialItemProps(fluent::spatial::SpatialItem* item, const QJsonObject& props)
{
    if (!item)
        return;
    const double px = qBound(-600.0, props.value(QStringLiteral("posX")).toDouble(0.0), 600.0);
    const double py = qBound(-600.0, props.value(QStringLiteral("posY")).toDouble(0.0), 600.0);
    const double pz = qBound(-400.0, props.value(QStringLiteral("posZ")).toDouble(40.0), 400.0);
    item->setPosition(QVector3D(float(px), float(py), float(pz)));
    const double rx = qBound(-60.0, props.value(QStringLiteral("rotX")).toDouble(6.0), 60.0);
    const double ry = qBound(-60.0, props.value(QStringLiteral("rotY")).toDouble(-16.0), 60.0);
    const double rz = qBound(-60.0, props.value(QStringLiteral("rotZ")).toDouble(0.0), 60.0);
    item->setRotation(QVector3D(float(rx), float(ry), float(rz)));
    const double rawScale = props.value(QStringLiteral("scale")).toDouble(100.0);
    item->setScale(qBound(0.25, rawScale <= 4.0 ? rawScale : (rawScale / 100.0), 2.5));
    const double rawSurface = props.value(QStringLiteral("surfaceIntensity")).toDouble(75.0);
    item->setSurfaceIntensity(qBound(0.0, rawSurface <= 1.0 ? rawSurface : (rawSurface / 100.0), 1.0));
    item->setHoverLift(qBound(0.0, props.value(QStringLiteral("hoverLift")).toDouble(6.0), 16.0));
}

std::function<QRect(const GalleryStudioCanvas*, bool*)> makeSpatialBoundsFn(fluent::spatial::SpatialView* view,
    fluent::spatial::SpatialItem* item,
    QWidget* itemRootWidget,
    QWidget* entryWidget)
{
    return [viewPtr = QPointer<fluent::spatial::SpatialView>(view),
               itemPtr = QPointer<fluent::spatial::SpatialItem>(item), rootPtr = QPointer<QWidget>(itemRootWidget),
               nodePtr = QPointer<QWidget>(entryWidget)](const GalleryStudioCanvas* canvas, bool* visible) -> QRect {
        if (!viewPtr || !itemPtr || !viewPtr->isVisibleTo(canvas)) {
            if (visible)
                *visible = false;
            return {};
        }
        if (visible)
            *visible = itemPtr->isVisible();
        const QPoint origin = viewPtr->mapTo(const_cast<GalleryStudioCanvas*>(canvas), QPoint(0, 0));
        QRect itemRect;
        const QPolygonF poly = itemPtr->projectedPolygon();
        if (!poly.isEmpty()) {
            const QRect aligned = poly.boundingRect().toAlignedRect().translated(origin);
            if (aligned.width() > 0 && aligned.height() > 0)
                itemRect = aligned;
        }
        if (itemRect.isEmpty() && rootPtr && rootPtr->width() > 0 && rootPtr->height() > 0)
            itemRect = QRect(origin + QPoint(16, 16), rootPtr->size());
        if (itemRect.isEmpty())
            itemRect = QRect(origin, viewPtr->size());

        if (nodePtr && rootPtr && nodePtr != rootPtr && rootPtr->isAncestorOf(nodePtr)) {
            if (!viewPtr->isSpatialEnabled())
                return QRect(nodePtr->mapTo(const_cast<GalleryStudioCanvas*>(canvas), QPoint(0, 0)), nodePtr->size());
            const QPoint rel = nodePtr->mapTo(rootPtr.data(), QPoint(0, 0));
            return QRect(itemRect.topLeft() + rel, nodePtr->size());
        }
        return itemRect;
    };
}
#endif

AnchorLayout::Anchors resolveChildAnchors(QWidget* host,
    const QJsonObject& childProps,
    const QString& childType,
    int index,
    int containerMargin)
{
    using Edge = AnchorLayout::Edge;
    AnchorLayout::Anchors a;
    QString pos = childProps.value(QStringLiteral("anchorPosition")).toString();
    if (pos.isEmpty())
        pos = (index == 0 && (childType == QStringLiteral("ParticleBackdrop") || childType == QStringLiteral("Card")))
            ? QStringLiteral("Fill")
            : QStringLiteral("TopLeft");

    const int m = childProps.contains(QStringLiteral("anchorMargin"))
        ? qBound(0, childProps.value(QStringLiteral("anchorMargin")).toInt(containerMargin), 96)
        : containerMargin;
    const int ox = qBound(-1200, childProps.value(QStringLiteral("anchorOffsetX")).toInt(0), 1200);
    const bool explicitPos = childProps.contains(QStringLiteral("anchorPosition"));
    const int oy = qBound(-1200,
        childProps.value(QStringLiteral("anchorOffsetY"))
            .toInt((pos == QStringLiteral("TopLeft") && !explicitPos) ? index * 44 : 0),
        1200);

    if (pos == QStringLiteral("Fill")) {
        a.fill = true;
        a.fillMargins = QMargins(m, m, m, m);
    } else if (pos == QStringLiteral("TopCenter")) {
        a.top = {host, Edge::Top, m + oy};
        a.horizontalCenter = {host, Edge::HCenter, ox};
    } else if (pos == QStringLiteral("TopRight")) {
        a.top = {host, Edge::Top, m + oy};
        a.right = {host, Edge::Right, -m + ox};
    } else if (pos == QStringLiteral("Center")) {
        a.horizontalCenter = {host, Edge::HCenter, ox};
        a.verticalCenter = {host, Edge::VCenter, oy};
    } else if (pos == QStringLiteral("BottomLeft")) {
        a.bottom = {host, Edge::Bottom, -m + oy};
        a.left = {host, Edge::Left, m + ox};
    } else if (pos == QStringLiteral("BottomCenter")) {
        a.bottom = {host, Edge::Bottom, -m + oy};
        a.horizontalCenter = {host, Edge::HCenter, ox};
    } else if (pos == QStringLiteral("BottomRight")) {
        a.bottom = {host, Edge::Bottom, -m + oy};
        a.right = {host, Edge::Right, -m + ox};
    } else if (pos == QStringLiteral("TopBar")) {
        a.top = {host, Edge::Top, m + oy};
        a.left = {host, Edge::Left, m};
        a.right = {host, Edge::Right, -m};
    } else if (pos == QStringLiteral("BottomBar")) {
        a.bottom = {host, Edge::Bottom, -m + oy};
        a.left = {host, Edge::Left, m};
        a.right = {host, Edge::Right, -m};
    } else if (pos == QStringLiteral("LeftDock")) {
        a.left = {host, Edge::Left, m + ox};
        a.top = {host, Edge::Top, m};
        a.bottom = {host, Edge::Bottom, -m};
    } else if (pos == QStringLiteral("RightDock")) {
        a.right = {host, Edge::Right, -m + ox};
        a.top = {host, Edge::Top, m};
        a.bottom = {host, Edge::Bottom, -m};
    } else {
        a.top = {host, Edge::Top, m + oy};
        a.left = {host, Edge::Left, m + ox};
    }
    return a;
}

bool isAnchorPropKey(const QString& key)
{
    return key == QStringLiteral("anchorPosition") || key == QStringLiteral("anchorMargin")
        || key == QStringLiteral("anchorOffsetX") || key == QStringLiteral("anchorOffsetY");
}

QJsonObject extractAnchorProps(const QJsonObject& props)
{
    QJsonObject out;
    for (const char* rawKey : {"anchorPosition", "anchorMargin", "anchorOffsetX", "anchorOffsetY"}) {
        const QString key = QString::fromLatin1(rawKey);
        if (props.contains(key))
            out.insert(key, props.value(key));
    }
    return out;
}

void ensureAnchorChildMinSize(QWidget* child, const QString& childType, const QJsonObject& childObj)
{
    if (!child)
        return;

    if ((childType == QStringLiteral("Card") || childType == QStringLiteral("VBox")
            || childType == QStringLiteral("HBox") || childType == QStringLiteral("Grid"))
        && childObj.value(QStringLiteral("children")).toArray().isEmpty()) {
        child->setMinimumSize(qMax(child->minimumWidth(), 240), qMax(child->minimumHeight(), 100));
        return;
    }

    if (childType == QStringLiteral("Divider")) {
        if (auto* div = qobject_cast<Divider*>(child);
            div && div->orientation() == Qt::Horizontal && child->minimumWidth() < 220) {
            child->setMinimumWidth(220);
        }
        return;
    }

    if (childType == QStringLiteral("Slider") || childType == QStringLiteral("RangeSlider")
        || childType == QStringLiteral("ProgressBar")) {
        const auto* slider = qobject_cast<Slider*>(child);
        const auto* rangeSlider = qobject_cast<RangeSlider*>(child);
        const bool vertical = (slider && slider->orientation() == Qt::Vertical)
            || (rangeSlider && rangeSlider->orientation() == Qt::Vertical);
        if (!vertical && child->sizeHint().width() < 200 && child->minimumWidth() < 220)
            child->setMinimumWidth(220);
        return;
    }

    if (childType == QStringLiteral("Field") || childType == QStringLiteral("LineEdit")
        || childType == QStringLiteral("PasswordBox") || childType == QStringLiteral("SearchBox")
        || childType == QStringLiteral("TextEdit") || childType == QStringLiteral("CodeBlock")
        || childType == QStringLiteral("Shimmer")) {
        if (child->sizeHint().width() < 220 && child->minimumWidth() < 240)
            child->setMinimumWidth(240);
        return;
    }

    static const QSet<QString> wideTypes{
        QStringLiteral("InfoBar"),
        QStringLiteral("Expander"),
        QStringLiteral("Accordion"),
        QStringLiteral("DataGrid"),
        QStringLiteral("ListView"),
        QStringLiteral("TableView"),
        QStringLiteral("TreeView"),
        QStringLiteral("FlowView"),
        QStringLiteral("GridView"),
        QStringLiteral("LineChart"),
        QStringLiteral("BarChart"),
        QStringLiteral("AreaChart"),
        QStringLiteral("PieChart"),
        QStringLiteral("ScatterChart"),
        QStringLiteral("Sparkline"),
        QStringLiteral("NavigationView"),
        QStringLiteral("SplitView"),
        QStringLiteral("TabView"),
        QStringLiteral("ListDetailView"),
        QStringLiteral("ScrollView"),
        QStringLiteral("FlipView"),
        QStringLiteral("StackView"),
        QStringLiteral("SpatialView"),
        QStringLiteral("QmlView"),
        QStringLiteral("ParticleBackdrop"),
        QStringLiteral("Window"),
    };
    if (wideTypes.contains(childType) && child->sizeHint().width() < 260 && child->minimumWidth() < 280)
        child->setMinimumWidth(280);
}

template <typename Enum>
Enum lookupEnum(const QString& key, std::initializer_list<std::pair<const char*, Enum>> entries, Enum fallback)
{
    for (const auto& entry : entries) {
        if (key == QLatin1String(entry.first))
            return entry.second;
    }
    return fallback;
}

Typography::FontRole parseTypography(const QString& key)
{
    return lookupEnum<Typography::FontRole>(key,
        {
            {"Caption", Typography::FontRole::Caption},
            {"Body", Typography::FontRole::Body},
            {"BodyStrong", Typography::FontRole::BodyStrong},
            {"BodyLarge", Typography::FontRole::BodyLarge},
            {"BodyLargeStrong", Typography::FontRole::BodyLargeStrong},
            {"Subtitle", Typography::FontRole::Subtitle},
            {"Title", Typography::FontRole::Title},
            {"TitleLarge", Typography::FontRole::TitleLarge},
            {"Display", Typography::FontRole::Display},
        },
        Typography::FontRole::Body);
}

QString resolveIconGlyph(const QString& nameOrGlyph)
{
    if (nameOrGlyph == QLatin1String("Home"))
        return Typography::Icons::Home;
    if (nameOrGlyph == QLatin1String("Add"))
        return Typography::Icons::Add;
    if (nameOrGlyph == QLatin1String("Search"))
        return Typography::Icons::Search;
    if (nameOrGlyph == QLatin1String("Save"))
        return Typography::Icons::Save;
    if (nameOrGlyph == QLatin1String("Edit"))
        return Typography::Icons::Edit;
    if (nameOrGlyph == QLatin1String("Delete"))
        return Typography::Icons::Delete;
    if (nameOrGlyph == QLatin1String("Settings"))
        return Typography::Icons::Settings;
    if (nameOrGlyph == QLatin1String("Info"))
        return Typography::Icons::Info;
    if (nameOrGlyph == QLatin1String("Share"))
        return Typography::Icons::Share;
    if (nameOrGlyph == QLatin1String("Document"))
        return Typography::Icons::Document;
    if (!nameOrGlyph.isEmpty()) {
        const QString lookedUp = Typography::Icons::glyph(nameOrGlyph);
        if (!lookedUp.isEmpty())
            return lookedUp;
        return nameOrGlyph;
    }
    return Typography::Icons::Settings;
}

QStringList jsonStringList(const QJsonValue& value, const QStringList& fallback)
{
    if (!value.isArray())
        return fallback;
    QStringList items;
    const QJsonArray array = value.toArray();
    items.reserve(array.size());
    for (const QJsonValue& item : array) {
        const QString text = item.toString();
        if (!text.isEmpty())
            items.append(text);
    }
    return items.isEmpty() ? fallback : items;
}

QVector<QPointF> jsonSeriesPoints(const QJsonValue& value, QStringList* labelsOut)
{
    QVector<double> values;
    if (value.isArray()) {
        for (const QJsonValue& v : value.toArray()) {
            if (v.isDouble())
                values.append(v.toDouble());
        }
    }
    if (values.isEmpty())
        values = {24.0, 42.0, 38.0, 64.0, 58.0, 82.0};
    QVector<QPointF> pts;
    pts.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        pts.append(QPointF(static_cast<qreal>(i + 1), values.at(i)));
        if (labelsOut)
            labelsOut->append(QStringLiteral("#%1").arg(i + 1));
    }
    return pts;
}

QWidget* makeSamplePane(QWidget* parent, const QString& title, const QString& subtitle)
{
    auto* card = new Card(parent);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);
    auto* hd = new Label(title, card);
    hd->setFluentTypography(Typography::FontRole::BodyStrong);
    auto* body = new Label(subtitle, card);
    body->setTextColorRole(Label::TextColorRole::Secondary);
    body->setWordWrap(true);
    layout->addWidget(hd);
    layout->addWidget(body);
    return card;
}

QStandardItemModel* buildStringModel(QObject* parent, const QStringList& items, bool storeKeyRole = false)
{
    auto* model = new QStandardItemModel(parent);
    for (const QString& itemText : items) {
        auto* item = new QStandardItem(itemText);
        if (storeKeyRole)
            item->setData(itemText, Qt::UserRole);
        model->appendRow(item);
    }
    return model;
}

ChartView* configureChart(ChartView* chart, const QJsonObject& props, int defaultMinHeight)
{
    auto* model = new ChartModel(chart);
    model->setName(props.value(QStringLiteral("seriesName")).toString(QStringLiteral("Series")));
    QStringList labels;
    const QVector<QPointF> points = jsonSeriesPoints(props.value(QStringLiteral("values")), &labels);
    model->setDataSnapshot(ChartData::fromPoints(points, labels));
    chart->setModel(model);
    chart->setTitle(props.value(QStringLiteral("title")).toString());
    chart->setMinimumHeight(qBound(64, props.value(QStringLiteral("minHeight")).toInt(defaultMinHeight), 420));
    return chart;
}

QPixmap createSamplePreviewPixmap()
{
    QPixmap pixmap(320, 180);
    pixmap.fill(QColor(QStringLiteral("#0F6CBD")));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(QRect(0, 90, 320, 90), QColor(QStringLiteral("#115EA3")));
    painter.setBrush(QColor(255, 255, 255, 36));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(250, 56), 54, 54);
    painter.setPen(Qt::white);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("Fluent-Qt ImageViewer"));
    return pixmap;
}

bool validateNodeRecursive(const QJsonValue& value, int depth, int& count, QSet<QString>& seenIds, QString* error)
{
    if (!value.isObject()) {
        if (error)
            *error = QStringLiteral("Every studio node must be a JSON object.");
        return false;
    }
    if (depth > GalleryStudioCanvas::kMaxDepth) {
        if (error)
            *error =
                QStringLiteral("Studio layout exceeds maximum nesting depth (%1).").arg(GalleryStudioCanvas::kMaxDepth);
        return false;
    }
    ++count;
    if (count > GalleryStudioCanvas::kMaxNodes) {
        if (error)
            *error =
                QStringLiteral("Studio layout exceeds maximum node count (%1).").arg(GalleryStudioCanvas::kMaxNodes);
        return false;
    }
    const QJsonObject obj = value.toObject();
    const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
    const QString type = obj.value(QStringLiteral("type")).toString().trimmed();
    if (id.isEmpty()) {
        if (error)
            *error = QStringLiteral("Every studio node requires a non-empty id.");
        return false;
    }
    if (seenIds.contains(id)) {
        if (error)
            *error = QStringLiteral("Duplicate studio node id: %1").arg(id);
        return false;
    }
    seenIds.insert(id);

    if (!GalleryStudioCanvas::supportedTypes().contains(type)) {
        if (error)
            *error = QStringLiteral("Unsupported studio component type: %1").arg(type);
        return false;
    }
    const QJsonValue childrenVal = obj.value(QStringLiteral("children"));
    if (!childrenVal.isUndefined() && !childrenVal.isArray()) {
        if (error)
            *error = QStringLiteral("Node '%1' children must be an array.").arg(id);
        return false;
    }
    const QJsonArray children = childrenVal.toArray();
    if (!containerTypes().contains(type) && !children.isEmpty()) {
        if (error)
            *error = QStringLiteral("Leaf node '%1' (%2) cannot contain children.").arg(id, type);
        return false;
    }
    if ((type == QStringLiteral("Field") || type == QStringLiteral("SpatialItem")) && children.size() > 1) {
        if (error)
            *error = QStringLiteral("%1 '%2' accepts at most one child.").arg(type, id);
        return false;
    }
    for (const QJsonValue& child : children) {
        if (!validateNodeRecursive(child, depth + 1, count, seenIds, error))
            return false;
    }
    return true;
}

class StageLayoutObserver final : public QObject {
public:
    explicit StageLayoutObserver(std::function<void()> onRelayout, QObject* parent = nullptr)
        : QObject(parent), m_onRelayout(std::move(onRelayout))
    {}

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize) {
            if (m_onRelayout)
                m_onRelayout();
        }
        return false;
    }

private:
    std::function<void()> m_onRelayout;
};

QObject* resolveInspectTarget(const QString& type, QWidget* widget)
{
    if (!widget)
        return nullptr;
#if defined(FLUENT_QT_HAS_SPATIAL)
    if (type == QStringLiteral("SpatialView"))
        return widget->findChild<fluent::spatial::SpatialView*>();
    if (type == QStringLiteral("SpatialItem"))
        return widget->findChild<fluent::spatial::SpatialItem*>();
#endif
#if defined(FLUENT_QT_HAS_QML_BRIDGE)
    if (type == QStringLiteral("QmlView"))
        return widget->findChild<fluent::qmlbridge::QmlView*>();
#endif
    Q_UNUSED(type);
    return widget;
}

void applyStudioOrientationConstraints(QWidget* widget)
{
    if (auto* slider = qobject_cast<Slider*>(widget)) {
        slider->setMinimumHeight(slider->orientation() == Qt::Vertical ? 140 : 0);
    } else if (auto* rangeSlider = qobject_cast<RangeSlider*>(widget)) {
        rangeSlider->setMinimumHeight(rangeSlider->orientation() == Qt::Vertical ? 140 : 0);
    } else if (auto* sb = qobject_cast<ScrollBar*>(widget)) {
        sb->setMinimumHeight(sb->orientation() == Qt::Vertical ? 140 : 0);
    } else if (auto* div = qobject_cast<Divider*>(widget)) {
        div->setMinimumHeight(div->orientation() == Qt::Vertical ? 24 : 0);
    }
}

// Map stage pixel width into Expanded / Medium / Compact tiers mirroring Breakpoints.
GalleryStudioCanvas::LayoutTier resolveLayoutTier(int viewportWidth)
{
    if (viewportWidth < 500)
        return GalleryStudioCanvas::LayoutTier::Compact;
    if (viewportWidth < 720)
        return GalleryStudioCanvas::LayoutTier::Medium;
    return GalleryStudioCanvas::LayoutTier::Expanded;
}

QString layoutTierName(GalleryStudioCanvas::LayoutTier tier)
{
    switch (tier) {
    case GalleryStudioCanvas::LayoutTier::Compact:
        return QStringLiteral("compact");
    case GalleryStudioCanvas::LayoutTier::Medium:
        return QStringLiteral("medium");
    case GalleryStudioCanvas::LayoutTier::Expanded:
        return QStringLiteral("expanded");
    }
    return QStringLiteral("expanded");
}

int scaleDensity(int configured,
    GalleryStudioCanvas::LayoutTier tier,
    int floorValue,
    qreal mediumScale,
    qreal compactScale)
{
    if (configured <= 0)
        return 0;
    if (tier == GalleryStudioCanvas::LayoutTier::Compact)
        return qMax(floorValue, qRound(configured * compactScale));
    if (tier == GalleryStudioCanvas::LayoutTier::Medium)
        return qMax(floorValue, qRound(configured * mediumScale));
    return configured;
}

} // namespace

struct GalleryStudioCanvas::ResponsiveBindings {
    struct GridBinding {
        QPointer<QWidget> host;
        QGridLayout* layout = nullptr;
        QVector<QPointer<QWidget>> items;
        int configuredColumns = 2;
        int configuredSpacing = 12;
        int configuredPadding = 0;
    };
    struct HBoxBinding {
        QPointer<QWidget> host;
        QBoxLayout* layout = nullptr;
        QVector<QPointer<QWidget>> items;
        QString alignKey;
        int configuredSpacing = 12;
        int configuredPadding = 0;
        bool hasTrailingStretch = false;
    };
    struct BoxDensityBinding {
        QPointer<QWidget> host;
        QBoxLayout* layout = nullptr;
        int configuredSpacing = 12;
        int configuredPadding = 16;
    };
    struct FieldBinding {
        QPointer<Field> field;
        Field::LabelPosition configuredPosition = Field::LabelPosition::Top;
    };
    struct HeadingBinding {
        QPointer<Label> label;
        Typography::FontRole configuredRole = Typography::FontRole::Body;
    };
    struct NavigationBinding {
        QPointer<NavigationView> nav;
        NavigationView::DisplayMode configuredMode = NavigationView::DisplayMode::Left;
        bool configuredPaneOpen = true;
    };
    struct ListDetailBinding {
        QPointer<ListDetailView> view;
        int configuredPaneWidth = 220;
    };
    struct SplitBinding {
        QPointer<SplitView> split;
        Qt::Orientation configuredOrientation = Qt::Horizontal;
    };
    struct InfoBarBinding {
        QPointer<InfoBar> bar;
        bool configuredSingleLine = true;
    };

    QVector<GridBinding> grids;
    QVector<HBoxBinding> hboxes;
    QVector<BoxDensityBinding> boxes;
    QVector<FieldBinding> fields;
    QVector<HeadingBinding> headings;
    QVector<NavigationBinding> navs;
    QVector<ListDetailBinding> listDetails;
    QVector<SplitBinding> splits;
    QVector<InfoBarBinding> infoBars;
};

GalleryStudioCanvas::GalleryStudioCanvas(QWidget* parent)
    : QWidget(parent), m_responsive(std::make_unique<ResponsiveBindings>())
{
    setObjectName(QStringLiteral("galleryStudioCanvas"));
    setAutoFillBackground(false);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_scroll = new ScrollView(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setAutoFillBackground(false);
    m_scroll->viewport()->setAutoFillBackground(false);
    rootLayout->addWidget(m_scroll);

    m_stage = new QWidget(m_scroll->viewport());
    m_stage->setObjectName(QStringLiteral("galleryStudioStage"));
    m_stage->setMinimumWidth(0);
    m_stage->setAutoFillBackground(false);
    m_stageLayout = new QVBoxLayout(m_stage);
    m_stageLayout->setContentsMargins(0, 0, 0, 0);
    m_stageLayout->setSpacing(0);
    m_stageLayout->addStretch(1);
    m_scroll->setWidget(m_stage);

    m_stage->installEventFilter(new StageLayoutObserver([this]() { scheduleLayoutSnapshot(); }, this));
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() { scheduleLayoutSnapshot(); });
    connect(m_scroll->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() { scheduleLayoutSnapshot(); });
}

GalleryStudioCanvas::~GalleryStudioCanvas()
{
    deactivateEmbeddedSessions(m_rootWidget.data());
}

QStringList GalleryStudioCanvas::supportedTypes()
{
    static const QStringList types = {
        // Layout (12: 4 containers + 8 Fluent layout controls)
        QStringLiteral("VBox"),
        QStringLiteral("HBox"),
        QStringLiteral("Grid"),
        QStringLiteral("AnchorBox"),
        QStringLiteral("Card"),
        QStringLiteral("Expander"),
        QStringLiteral("Accordion"),
        QStringLiteral("Field"),
        QStringLiteral("Divider"),
        QStringLiteral("DataGridToolBar"),
        QStringLiteral("PropertyPanel"),
        QStringLiteral("ListDetailView"),
        // Basic input (19)
        QStringLiteral("Button"),
        QStringLiteral("CompoundButton"),
        QStringLiteral("ToggleButton"),
        QStringLiteral("DropDownButton"),
        QStringLiteral("SplitButton"),
        QStringLiteral("ToggleSplitButton"),
        QStringLiteral("HyperlinkButton"),
        QStringLiteral("RepeatButton"),
        QStringLiteral("CheckBox"),
        QStringLiteral("RadioButton"),
        QStringLiteral("ToggleSwitch"),
        QStringLiteral("ComboBox"),
        QStringLiteral("MultiSelectComboBox"),
        QStringLiteral("TreeSelect"),
        QStringLiteral("Slider"),
        QStringLiteral("RangeSlider"),
        QStringLiteral("RatingControl"),
        QStringLiteral("ColorPicker"),
        QStringLiteral("FileDropZone"),
        // Text fields (8)
        QStringLiteral("Label"),
        QStringLiteral("LineEdit"),
        QStringLiteral("PasswordBox"),
        QStringLiteral("NumberBox"),
        QStringLiteral("TextEdit"),
        QStringLiteral("AutoSuggestBox"),
        QStringLiteral("TagInput"),
        QStringLiteral("CodeBlock"),
        // Date & time (5)
        QStringLiteral("DatePicker"),
        QStringLiteral("TimePicker"),
        QStringLiteral("CalendarDatePicker"),
        QStringLiteral("CalendarView"),
        QStringLiteral("DateTimeRangePicker"),
        // Navigation (6)
        QStringLiteral("Breadcrumb"),
        QStringLiteral("Stepper"),
        QStringLiteral("Pivot"),
        QStringLiteral("SelectorBar"),
        QStringLiteral("TabView"),
        QStringLiteral("NavigationView"),
        // Menus & toolbars (5)
        QStringLiteral("MenuBar"),
        QStringLiteral("CommandBar"),
        QStringLiteral("Menu"),
        QStringLiteral("CommandBarFlyout"),
        QStringLiteral("CommandPalette"),
        // Status & info (9)
        QStringLiteral("InfoBar"),
        QStringLiteral("InfoBadge"),
        QStringLiteral("Avatar"),
        QStringLiteral("ProgressBar"),
        QStringLiteral("ProgressRing"),
        QStringLiteral("Shimmer"),
        QStringLiteral("Toast"),
        QStringLiteral("ToolTip"),
        QStringLiteral("SplashScreen"),
        // Collections (12)
        QStringLiteral("ListView"),
        QStringLiteral("GridView"),
        QStringLiteral("TreeView"),
        QStringLiteral("DataGrid"),
        QStringLiteral("Timeline"),
        QStringLiteral("Transfer"),
        QStringLiteral("FlipView"),
        QStringLiteral("FlowView"),
        QStringLiteral("SplitView"),
        QStringLiteral("StackView"),
        QStringLiteral("DrawerView"),
        QStringLiteral("FileListView"),
        // Charts (9)
        QStringLiteral("LineChart"),
        QStringLiteral("AreaChart"),
        QStringLiteral("BarChart"),
        QStringLiteral("HorizontalBarChart"),
        QStringLiteral("PieChart"),
        QStringLiteral("DonutChart"),
        QStringLiteral("ScatterChart"),
        QStringLiteral("Sparkline"),
        QStringLiteral("ChartView"),
        // Scrolling (5)
        QStringLiteral("ScrollView"),
        QStringLiteral("PipsPager"),
        QStringLiteral("ImageViewer"),
        QStringLiteral("ScrollBar"),
        QStringLiteral("AnnotatedScrollBar"),
        // Dialogs & flyouts (6)
        QStringLiteral("ContentDialog"),
        QStringLiteral("Dialog"),
        QStringLiteral("Flyout"),
        QStringLiteral("Popup"),
        QStringLiteral("TeachingTip"),
        QStringLiteral("CoachMark"),
        // Windowing & foundation (3)
        QStringLiteral("Window"),
        QStringLiteral("TitleBar"),
        QStringLiteral("FontIcon"),
        // Extensions: Particles, Spatial 3D, QmlBridge (4)
        QStringLiteral("ParticleBackdrop"),
        QStringLiteral("SpatialView"),
        QStringLiteral("SpatialItem"),
        QStringLiteral("QmlView"),
    };
    return types;
}

bool GalleryStudioCanvas::applyDocument(const QJsonObject& document, QString* error, bool emitInspection)
{
    const int schemaVersion = document.contains(QStringLiteral("schemaVersion"))
        ? document.value(QStringLiteral("schemaVersion")).toInt(-1)
        : 1;
    if (schemaVersion != 1) {
        if (error)
            *error = QStringLiteral("Unsupported studio document schemaVersion.");
        return false;
    }
    const QJsonValue rootValue = document.value(QStringLiteral("root"));
    int count = 0;
    QSet<QString> seenIds;
    if (!validateNodeRecursive(rootValue, 1, count, seenIds, error))
        return false;

    QWidget* stagedParent = m_stage;
    QVector<NodeEntry> built;
    built.reserve(count);
    auto nextResponsive = std::make_unique<ResponsiveBindings>();
    m_stagingResponsive = nextResponsive.get();
    QWidget* nextRoot = buildNode(rootValue.toObject(), stagedParent, built);
    m_stagingResponsive = nullptr;
    if (!nextRoot) {
        if (error)
            *error = QStringLiteral("Failed to instantiate studio root widget.");
        return false;
    }

    const bool prevStageUpdates = m_stage->updatesEnabled();
    m_stage->setUpdatesEnabled(false);

    if (m_rootWidget) {
        deactivateEmbeddedSessions(m_rootWidget.data());
        m_stageLayout->removeWidget(m_rootWidget.data());
        m_rootWidget->hide();
        delete m_rootWidget.data();
        m_rootWidget.clear();
    }
    while (QLayoutItem* item = m_stageLayout->takeAt(0))
        delete item;

    m_nodes = std::move(built);
    m_responsive = std::move(nextResponsive);
    m_lastLayoutWidth = -1;
    m_widgetsById.clear();
    for (const NodeEntry& entry : m_nodes)
        m_widgetsById.insert(entry.id, entry.widget);

    m_rootIsAnchorBox = (rootValue.toObject().value(QStringLiteral("type")).toString().trimmed()
        == QStringLiteral("AnchorBox"));
    m_rootWidget = nextRoot;
    m_stageLayout->addWidget(m_rootWidget.data(), m_rootIsAnchorBox ? 1 : 0);
    if (!m_rootIsAnchorBox)
        m_stageLayout->addStretch(1);
    m_rootWidget->show();
    const int vpWidth =
        m_scroll && m_scroll->viewport()->width() > 0 ? m_scroll->viewport()->width() : (width() > 0 ? width() : 860);
    applyResponsiveLayout(vpWidth);

    m_stage->setUpdatesEnabled(prevStageUpdates);
    m_stage->update();

    if (emitInspection && !m_selectedNodeId.isEmpty() && m_widgetsById.contains(m_selectedNodeId))
        emit nodeInspected(inspectNode(m_selectedNodeId));

    scheduleLayoutSnapshot();
    return true;
}

bool GalleryStudioCanvas::patchNodeProperties(const QString& nodeId, const QJsonObject& patch, bool emitInspection)
{
    if (nodeId.isEmpty() || patch.isEmpty())
        return false;

    NodeEntry* found = nullptr;
    for (NodeEntry& entry : m_nodes) {
        if (entry.id == nodeId) {
            found = &entry;
            break;
        }
    }
    if (!found)
        return false;

    QObject* target = found->inspectTarget ? found->inspectTarget.data() : found->widget.data();
    QJsonObject anchorPatch;
    QJsonObject widgetPatch;
    for (auto it = patch.constBegin(); it != patch.constEnd(); ++it) {
        if (isAnchorPropKey(it.key()))
            anchorPatch.insert(it.key(), it.value());
        else
            widgetPatch.insert(it.key(), it.value());
    }

    QWidget* parentBox = (!anchorPatch.isEmpty() && found->widget) ? found->widget->parentWidget() : nullptr;
    AnchorLayout* parentAnchorLayout = parentBox ? qobject_cast<AnchorLayout*>(parentBox->layout()) : nullptr;
    if (!anchorPatch.isEmpty() && (!found->widget || !parentBox || !parentAnchorLayout))
        return false;

    bool patchAnchorContainer = false;
    AnchorLayout* containerAnchorLayout = nullptr;
    if (found->type == QStringLiteral("AnchorBox") && !widgetPatch.isEmpty()) {
        patchAnchorContainer = true;
        for (auto it = widgetPatch.constBegin(); it != widgetPatch.constEnd(); ++it) {
            if (it.key() != QStringLiteral("minHeight") && it.key() != QStringLiteral("padding")) {
                patchAnchorContainer = false;
                break;
            }
        }
        containerAnchorLayout = found->widget ? qobject_cast<AnchorLayout*>(found->widget->layout()) : nullptr;
        if (!patchAnchorContainer || !found->widget || !containerAnchorLayout)
            return false;
    }

    if (!patchAnchorContainer && !widgetPatch.isEmpty()
        && !GalleryComponentPlayground::canPatchStudioProperties(found->type, target, widgetPatch)) {
        return false;
    }

    if (patchAnchorContainer) {
        if (widgetPatch.contains(QStringLiteral("minHeight")))
            found->widget->setMinimumHeight(
                qBound(80, widgetPatch.value(QStringLiteral("minHeight")).toInt(220), 1600));
        if (widgetPatch.contains(QStringLiteral("padding"))) {
            found->anchorContainerPadding = qBound(0, widgetPatch.value(QStringLiteral("padding")).toInt(12), 48);
            for (NodeEntry& entry : m_nodes) {
                if (entry.widget && entry.widget->parentWidget() == found->widget.data()) {
                    const AnchorLayout::Anchors anchors = resolveChildAnchors(found->widget.data(), entry.anchorProps,
                        entry.type, entry.anchorIndex, found->anchorContainerPadding);
                    containerAnchorLayout->setWidgetAnchors(entry.widget.data(), anchors);
                }
            }
        }
        containerAnchorLayout->activate();
        found->widget->updateGeometry();
        found->widget->update();
    } else if (!widgetPatch.isEmpty()) {
        auto* combo = qobject_cast<QComboBox*>(target);
        const bool preserveComboIndex = combo && widgetPatch.contains(QStringLiteral("items"))
            && !widgetPatch.contains(QStringLiteral("currentIndex"));
        const int prevComboIndex = preserveComboIndex ? combo->currentIndex() : -1;

        {
            const QSignalBlocker blocker(target);
            if (!GalleryComponentPlayground::applyStudioProperties(found->type, target, widgetPatch))
                return false;
            if (preserveComboIndex && combo->count() > 0 && prevComboIndex >= 0)
                combo->setCurrentIndex(qBound(0, prevComboIndex, combo->count() - 1));
        }

        if (m_responsive) {
            if (auto* lbl = qobject_cast<Label*>(found->widget.data());
                lbl && widgetPatch.contains(QStringLiteral("fluentTypography"))) {
                const auto role = parseTypography(widgetPatch.value(QStringLiteral("fluentTypography")).toString());
                bool updated = false;
                for (auto& hb : m_responsive->headings) {
                    if (hb.label.data() == lbl) {
                        hb.configuredRole = role;
                        updated = true;
                    }
                }
                if (!updated
                    && (role == Typography::FontRole::Display || role == Typography::FontRole::TitleLarge
                        || role == Typography::FontRole::Title)) {
                    m_responsive->headings.append({lbl, role});
                }
            }
            if (auto* field = qobject_cast<Field*>(found->widget.data());
                field && widgetPatch.contains(QStringLiteral("labelPosition"))) {
                const auto pos = lookupEnum<Field::LabelPosition>(
                    widgetPatch.value(QStringLiteral("labelPosition")).toString(),
                    {{"Top", Field::LabelPosition::Top}, {"Leading", Field::LabelPosition::Leading}},
                    Field::LabelPosition::Top);
                for (auto& fb : m_responsive->fields) {
                    if (fb.field.data() == field)
                        fb.configuredPosition = pos;
                }
            }
            if (auto* bar = qobject_cast<InfoBar*>(found->widget.data());
                bar && widgetPatch.contains(QStringLiteral("singleLine"))) {
                const bool sl = widgetPatch.value(QStringLiteral("singleLine")).toBool();
                for (auto& ib : m_responsive->infoBars) {
                    if (ib.bar.data() == bar)
                        ib.configuredSingleLine = sl;
                }
            }
            if (auto* nav = qobject_cast<NavigationView*>(found->widget.data()); nav
                && (widgetPatch.contains(QStringLiteral("displayMode"))
                    || widgetPatch.contains(QStringLiteral("paneOpen")))) {
                for (auto& nb : m_responsive->navs) {
                    if (nb.nav.data() == nav) {
                        if (widgetPatch.contains(QStringLiteral("displayMode"))) {
                            nb.configuredMode = lookupEnum<NavigationView::DisplayMode>(
                                widgetPatch.value(QStringLiteral("displayMode")).toString(),
                                {{"Auto", NavigationView::DisplayMode::Auto},
                                    {"Left", NavigationView::DisplayMode::Left},
                                    {"LeftCompact", NavigationView::DisplayMode::LeftCompact},
                                    {"Top", NavigationView::DisplayMode::Top}},
                                nb.configuredMode);
                        }
                        if (widgetPatch.contains(QStringLiteral("paneOpen")))
                            nb.configuredPaneOpen = widgetPatch.value(QStringLiteral("paneOpen")).toBool();
                    }
                }
            }
            if (auto* split = qobject_cast<SplitView*>(found->widget.data());
                split && widgetPatch.contains(QStringLiteral("orientation"))) {
                const Qt::Orientation orient =
                    widgetPatch.value(QStringLiteral("orientation")).toString() == QStringLiteral("Vertical")
                    ? Qt::Vertical
                    : Qt::Horizontal;
                for (auto& sb : m_responsive->splits) {
                    if (sb.split.data() == split)
                        sb.configuredOrientation = orient;
                }
            }
        }

        applyStudioOrientationConstraints(found->widget.data());
        if (found->widget) {
            found->widget->updateGeometry();
            found->widget->update();
            if (QWidget* pw = found->widget->parentWidget()) {
                if (auto* pal = qobject_cast<AnchorLayout*>(pw->layout()))
                    pal->activate();
            }
        }
    }

    if (!anchorPatch.isEmpty()) {
        for (auto it = anchorPatch.constBegin(); it != anchorPatch.constEnd(); ++it) {
            if (it.value().isUndefined() || it.value().isNull())
                found->anchorProps.remove(it.key());
            else
                found->anchorProps.insert(it.key(), it.value());
        }
        int containerPadding = 12;
        for (const NodeEntry& entry : std::as_const(m_nodes)) {
            if (entry.widget.data() == parentBox && entry.type == QStringLiteral("AnchorBox")) {
                containerPadding = entry.anchorContainerPadding;
                break;
            }
        }
        const AnchorLayout::Anchors anchors = resolveChildAnchors(parentBox, found->anchorProps, found->type,
            found->anchorIndex, containerPadding);
        parentAnchorLayout->setWidgetAnchors(found->widget.data(), anchors);
        parentAnchorLayout->activate();
        parentBox->updateGeometry();
        parentBox->update();
    }

    const int vpWidth =
        m_scroll && m_scroll->viewport()->width() > 0 ? m_scroll->viewport()->width() : (width() > 0 ? width() : 860);
    m_lastLayoutWidth = -1;
    applyResponsiveLayout(vpWidth);

    if (emitInspection && m_selectedNodeId == nodeId)
        emit nodeInspected(inspectNode(nodeId));

    scheduleLayoutSnapshot();
    return true;
}

QJsonObject GalleryStudioCanvas::layoutSnapshot() const
{
    QJsonObject nodes;
    for (const NodeEntry& entry : m_nodes) {
        QWidget* widget = entry.widget.data();
        if (!widget)
            continue;
        QRect bounds;
        bool visible = false;
        if (entry.resolveBounds) {
            bounds = entry.resolveBounds(this, &visible);
        } else {
            const QPoint topLeft = widget->mapTo(const_cast<GalleryStudioCanvas*>(this), QPoint(0, 0));
            bounds = QRect(topLeft, widget->size());
            visible = widget->isVisibleTo(const_cast<GalleryStudioCanvas*>(this));
        }
        nodes.insert(entry.id,
            QJsonObject{
                {QStringLiteral("type"), entry.type},
                {QStringLiteral("x"), bounds.x()},
                {QStringLiteral("y"), bounds.y()},
                {QStringLiteral("w"), bounds.width()},
                {QStringLiteral("h"), bounds.height()},
                {QStringLiteral("visible"), visible},
            });
    }
    const int contentH = !m_rootWidget
        ? 0
        : (m_rootIsAnchorBox ? qMax(m_rootWidget->minimumHeight(), m_rootWidget->sizeHint().height())
                             : qMax(m_rootWidget->height(), m_rootWidget->sizeHint().height()));
    return QJsonObject{
        {QStringLiteral("revision"), m_revision},
        {QStringLiteral("selectedId"), m_selectedNodeId},
        {QStringLiteral("layoutTier"), layoutTierName(m_layoutTier)},
        {QStringLiteral("viewportWidth"), m_scroll ? m_scroll->viewport()->width() : width()},
        {QStringLiteral("viewportHeight"), m_scroll ? m_scroll->viewport()->height() : height()},
        {QStringLiteral("contentHeight"), contentH},
        {QStringLiteral("scrollY"), m_scroll ? m_scroll->verticalScrollBar()->value() : 0},
        {QStringLiteral("nodes"), nodes},
    };
}

void GalleryStudioCanvas::refreshResponsiveLayout()
{
    const int vpWidth =
        m_scroll && m_scroll->viewport()->width() > 0 ? m_scroll->viewport()->width() : (width() > 0 ? width() : 860);
    m_lastLayoutWidth = -1;
    applyResponsiveLayout(vpWidth);
    emitLayoutSnapshot();
}

void GalleryStudioCanvas::setSelectedNodeId(const QString& nodeId)
{
    const bool changed = (m_selectedNodeId != nodeId);
    m_selectedNodeId = nodeId;
    if (!m_selectedNodeId.isEmpty())
        emit nodeInspected(inspectNode(m_selectedNodeId));
    if (changed)
        scheduleLayoutSnapshot();
}

void GalleryStudioCanvas::setTheme(FluentElement::Theme theme)
{
    m_theme = theme;
    FluentElement::setTheme(theme);
    update();
    if (m_scroll)
        m_scroll->viewport()->update();
    if (m_stage)
        m_stage->update();
    scheduleLayoutSnapshot();
}

QWidget* GalleryStudioCanvas::widgetForNode(const QString& nodeId) const
{
    return m_widgetsById.value(nodeId).data();
}

QJsonObject GalleryStudioCanvas::inspectNode(const QString& nodeId) const
{
    for (const NodeEntry& entry : m_nodes) {
        if (entry.id != nodeId)
            continue;
        QObject* target = entry.inspectTarget ? entry.inspectTarget.data() : entry.widget.data();
        const QJsonObject schema = GalleryComponentPlayground::studioSchemaForWidget(entry.type, target);
        return QJsonObject{
            {QStringLiteral("nodeId"), nodeId},
            {QStringLiteral("type"), entry.type},
            {QStringLiteral("supported"), !schema.isEmpty()},
            {QStringLiteral("schema"), schema},
        };
    }
    return QJsonObject{
        {QStringLiteral("nodeId"), nodeId},
        {QStringLiteral("type"), QString()},
        {QStringLiteral("supported"), false},
        {QStringLiteral("schema"), QJsonObject{}},
    };
}

QWidget* GalleryStudioCanvas::buildNode(const QJsonObject& nodeObj, QWidget* parentWidget, QVector<NodeEntry>& built)
{
    const QString id = nodeObj.value(QStringLiteral("id")).toString().trimmed();
    const QString type = nodeObj.value(QStringLiteral("type")).toString().trimmed();
    const QJsonObject props = nodeObj.value(QStringLiteral("props")).toObject();
    const QJsonArray children = nodeObj.value(QStringLiteral("children")).toArray();

    QWidget* widget = containerTypes().contains(type)
        ? buildContainer(id, type, props, children, parentWidget, built)
        : buildControl(id, type, props, parentWidget);
    if (!widget)
        return nullptr;

    QObject* inspectTarget = resolveInspectTarget(type, widget);
    if (inspectTarget && !props.isEmpty()) {
        const QSignalBlocker blocker(inspectTarget);
        GalleryComponentPlayground::applyStudioProperties(type, inspectTarget, props);
    }
    applyStudioOrientationConstraints(widget);

    widget->setObjectName(QStringLiteral("studioNode_%1").arg(id));
    built.append(NodeEntry{id, type, widget, nullptr, inspectTarget});
    if (type == QStringLiteral("AnchorBox"))
        built.last().anchorContainerPadding = qBound(0, props.value(QStringLiteral("padding")).toInt(12), 48);
    return widget;
}

QWidget* GalleryStudioCanvas::buildContainer(const QString& id,
    const QString& type,
    const QJsonObject& props,
    const QJsonArray& children,
    QWidget* parentWidget,
    QVector<NodeEntry>& built)
{
    if (type == QStringLiteral("VBox") || type == QStringLiteral("HBox")) {
        auto* box = new QWidget(parentWidget);
        box->setAutoFillBackground(false);
        box->setMinimumWidth(0);
        const int spacing = qBound(0, props.value(QStringLiteral("spacing")).toInt(12), 48);
        const int pad = qBound(0, props.value(QStringLiteral("padding")).toInt(type == QStringLiteral("VBox") ? 16 : 0),
            48);
        QBoxLayout* layout = (type == QStringLiteral("VBox"))
            ? static_cast<QBoxLayout*>(new QVBoxLayout(box))
            : static_cast<QBoxLayout*>(new QHBoxLayout(box));
        layout->setContentsMargins(pad, pad, pad, pad);
        layout->setSpacing(spacing);
        const QString alignKey = props.value(QStringLiteral("align")).toString();
        const bool isRow = type == QStringLiteral("HBox");
        QVector<QPointer<QWidget>> hItems;
        for (const QJsonValue& childVal : children) {
            if (QWidget* child = buildNode(childVal.toObject(), box, built)) {
                hItems.append(child);
                if (isRow && alignKey == QStringLiteral("Center"))
                    layout->addWidget(child, 0, Qt::AlignVCenter);
                else if (isRow && alignKey == QStringLiteral("Start"))
                    layout->addWidget(child, 0, Qt::AlignTop);
                else if (isRow && alignKey == QStringLiteral("End"))
                    layout->addWidget(child, 0, Qt::AlignBottom);
                else
                    layout->addWidget(child);
            }
        }
        const bool hasStretch = isRow
            && (alignKey == QStringLiteral("Start") || alignKey == QStringLiteral("Center")
                || alignKey == QStringLiteral("End"));
        if (hasStretch)
            layout->addStretch(1);
        if (m_stagingResponsive) {
            if (isRow)
                m_stagingResponsive->hboxes.append({box, layout, hItems, alignKey, spacing, pad, hasStretch});
            else
                m_stagingResponsive->boxes.append({box, layout, spacing, pad});
        }
        return box;
    }

    if (type == QStringLiteral("Grid")) {
        auto* gridWidget = new QWidget(parentWidget);
        gridWidget->setAutoFillBackground(false);
        gridWidget->setMinimumWidth(0);
        auto* layout = new QGridLayout(gridWidget);
        const int columns = qBound(1, props.value(QStringLiteral("columns")).toInt(2), 4);
        const int spacing = qBound(0, props.value(QStringLiteral("spacing")).toInt(12), 48);
        const int pad = qBound(0, props.value(QStringLiteral("padding")).toInt(0), 48);
        layout->setContentsMargins(pad, pad, pad, pad);
        layout->setSpacing(spacing);
        QVector<QPointer<QWidget>> gItems;
        int cell = 0;
        for (const QJsonValue& childVal : children) {
            if (QWidget* child = buildNode(childVal.toObject(), gridWidget, built)) {
                gItems.append(child);
                layout->addWidget(child, cell / columns, cell % columns);
                ++cell;
            }
        }
        for (int c = 0; c < columns; ++c)
            layout->setColumnStretch(c, 1);
        if (m_stagingResponsive)
            m_stagingResponsive->grids.append({gridWidget, layout, gItems, columns, spacing, pad});
        return gridWidget;
    }

    if (type == QStringLiteral("AnchorBox")) {
        auto* box = new QWidget(parentWidget);
        box->setAutoFillBackground(false);
        box->setMinimumHeight(qBound(80, props.value(QStringLiteral("minHeight")).toInt(220), 1600));
        auto* layout = new AnchorLayout(box);
        const int pad = qBound(0, props.value(QStringLiteral("padding")).toInt(12), 48);
        if (children.isEmpty()) {
            auto* hint = new Label(QStringLiteral("AnchorLayout · 锚点容器"), box);
            hint->setTextColorRole(Label::TextColorRole::Secondary);
            AnchorLayout::Anchors centerAnchors;
            centerAnchors.horizontalCenter = {box, AnchorLayout::Edge::HCenter, 0};
            centerAnchors.verticalCenter = {box, AnchorLayout::Edge::VCenter, 0};
            layout->addAnchoredWidget(hint, centerAnchors);
        } else {
            for (int i = 0; i < children.size(); ++i) {
                const QJsonObject childObj = children.at(i).toObject();
                QWidget* child = buildNode(childObj, box, built);
                if (!child)
                    continue;
                const QString childType = childObj.value(QStringLiteral("type")).toString();
                const QJsonObject childProps = childObj.value(QStringLiteral("props")).toObject();
                ensureAnchorChildMinSize(child, childType, childObj);
                const AnchorLayout::Anchors anchors = resolveChildAnchors(box, childProps, childType, i, pad);
                layout->addAnchoredWidget(child, anchors);
                if (!built.isEmpty() && built.last().widget == child) {
                    built.last().anchorProps = extractAnchorProps(childProps);
                    built.last().anchorIndex = i;
                } else {
                    for (int j = built.size() - 1; j >= 0; --j) {
                        if (built[j].widget == child) {
                            built[j].anchorProps = extractAnchorProps(childProps);
                            built[j].anchorIndex = i;
                            break;
                        }
                    }
                }
            }
        }
        return box;
    }

    if (type == QStringLiteral("Card")) {
        auto* card = new Card(parentWidget);
        card->setMinimumWidth(0);
        card->setAppearance(lookupEnum<Card::Appearance>(props.value(QStringLiteral("appearance")).toString(),
            {{"Layer", Card::Layer}, {"LayerAlt", Card::LayerAlt}, {"Canvas", Card::Canvas}}, Card::Layer));
        card->setBorderVisible(props.value(QStringLiteral("borderVisible")).toBool(true));
        const int spacing = qBound(0, props.value(QStringLiteral("spacing")).toInt(10), 48);
        const int pad = qBound(4, props.value(QStringLiteral("padding")).toInt(16), 48);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(pad, pad, pad, pad);
        layout->setSpacing(spacing);
        for (const QJsonValue& childVal : children) {
            if (QWidget* child = buildNode(childVal.toObject(), card, built))
                layout->addWidget(child);
        }
        if (m_stagingResponsive)
            m_stagingResponsive->boxes.append({card, layout, spacing, pad});
        return card;
    }

    if (type == QStringLiteral("Expander")) {
        auto* expander = new Expander(parentWidget);
        expander->setMinimumWidth(0);
        expander->setHeaderText(props.value(QStringLiteral("headerText")).toString(QStringLiteral("Section")));
        expander->setAnimationEnabled(false);
        expander->setExpanded(props.value(QStringLiteral("expanded")).toBool(true));
        auto* body = new QWidget();
        body->setMinimumWidth(0);
        body->setAutoFillBackground(false);
        auto* layout = new QVBoxLayout(body);
        layout->setContentsMargins(16, 12, 16, 14);
        layout->setSpacing(10);
        for (const QJsonValue& childVal : children) {
            if (QWidget* child = buildNode(childVal.toObject(), body, built))
                layout->addWidget(child);
        }
        expander->setContentWidget(body, WidgetOwnership::Owned);
        if (m_stagingResponsive)
            m_stagingResponsive->boxes.append({body, layout, 10, 14});
        connect(expander, &Expander::expandedChanged, this, [this, id](bool on) {
            scheduleLayoutSnapshot();
            emit interactionEvent(id, QStringLiteral("expandedChanged"), on);
        });
        return expander;
    }

    if (type == QStringLiteral("Accordion")) {
        auto* accordion = new Accordion(parentWidget);
        accordion->setExpansionMode(lookupEnum<Accordion::ExpansionMode>(
            props.value(QStringLiteral("expansionMode")).toString(),
            {{"Single", Accordion::ExpansionMode::Single}, {"Multiple", Accordion::ExpansionMode::Multiple}},
            Accordion::ExpansionMode::Single));
        if (children.isEmpty()) {
            const QStringList titles{QStringLiteral("General Settings"), QStringLiteral("Advanced Rules")};
            for (const QString& sectionTitle : titles) {
                auto* exp = new Expander(accordion);
                exp->setHeaderText(sectionTitle);
                exp->setAnimationEnabled(false);
                exp->setContentWidget(
                    makeSamplePane(nullptr, sectionTitle,
                        QStringLiteral("Drop Expander items into this Accordion container.")),
                    WidgetOwnership::Owned);
                accordion->addItem(exp, WidgetOwnership::Owned);
            }
        } else {
            for (const QJsonValue& childVal : children) {
                QWidget* child = buildNode(childVal.toObject(), accordion, built);
                if (!child)
                    continue;
                if (auto* exp = qobject_cast<Expander*>(child)) {
                    accordion->addItem(exp, WidgetOwnership::Owned);
                } else {
                    auto* wrapper = new Expander(accordion);
                    wrapper->setHeaderText(
                        childVal.toObject().value(QStringLiteral("id")).toString(QStringLiteral("Item")));
                    wrapper->setAnimationEnabled(false);
                    wrapper->setContentWidget(child, WidgetOwnership::Owned);
                    accordion->addItem(wrapper, WidgetOwnership::Owned);
                }
            }
        }
        return accordion;
    }

    if (type == QStringLiteral("Field")) {
        auto* field = new Field(parentWidget);
        field->setMinimumWidth(0);
        field->setLabelText(props.value(QStringLiteral("labelText")).toString(QStringLiteral("Field")));
        field->setHelperText(props.value(QStringLiteral("helperText")).toString());
        field->setRequired(props.value(QStringLiteral("required")).toBool(false));
        const auto configuredPosition = lookupEnum<Field::LabelPosition>(
            props.value(QStringLiteral("labelPosition")).toString(),
            {{"Top", Field::LabelPosition::Top}, {"Leading", Field::LabelPosition::Leading}},
            Field::LabelPosition::Top);
        field->setLabelPosition(configuredPosition);
        field->setValidationState(lookupEnum<Field::ValidationState>(
            props.value(QStringLiteral("validationState")).toString(),
            {{"None", Field::ValidationState::None}, {"Error", Field::ValidationState::Error},
                {"Warning", Field::ValidationState::Warning}, {"Success", Field::ValidationState::Success},
                {"Validating", Field::ValidationState::Validating}},
            Field::ValidationState::None));
        field->setValidationMessage(props.value(QStringLiteral("validationMessage")).toString());
        if (!children.isEmpty()) {
            if (QWidget* editor = buildNode(children.first().toObject(), nullptr, built))
                field->setEditor(editor, WidgetOwnership::Owned);
        }
        if (m_stagingResponsive)
            m_stagingResponsive->fields.append({field, configuredPosition});
        return field;
    }

    if (type == QStringLiteral("ScrollView")) {
        auto* sv = new ScrollView(parentWidget);
        sv->setWidgetResizable(true);
        sv->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(180), 420));
        auto* content = new QWidget();
        auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(10);
        if (children.isEmpty()) {
            layout->addWidget(makeSamplePane(content, QStringLiteral("ScrollView Viewport"),
                QStringLiteral("Host taller widgets inside a Fluent scrollable region.")));
        } else {
            for (const QJsonValue& childVal : children) {
                if (QWidget* child = buildNode(childVal.toObject(), content, built))
                    layout->addWidget(child);
            }
        }
        layout->addStretch(1);
        sv->setContentWidget(content, WidgetOwnership::Owned);
        return sv;
    }

    if (type == QStringLiteral("SplitView")) {
        auto* split = new SplitView(parentWidget);
        split->setMinimumWidth(0);
        const auto configuredOrientation =
            props.value(QStringLiteral("orientation")).toString() == QStringLiteral("Vertical")
            ? Qt::Vertical
            : Qt::Horizontal;
        split->setOrientation(configuredOrientation);
        split->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(180), 420));
        if (children.isEmpty()) {
            split->addPane(
                makeSamplePane(split, QStringLiteral("Primary Pane"), QStringLiteral("Draggable splitter pane")),
                WidgetOwnership::Owned);
            split->addPane(
                makeSamplePane(split, QStringLiteral("Secondary Pane"), QStringLiteral("Adaptive detail view")),
                WidgetOwnership::Owned);
        } else {
            for (const QJsonValue& childVal : children) {
                if (QWidget* child = buildNode(childVal.toObject(), split, built))
                    split->addPane(child, WidgetOwnership::Owned);
            }
        }
        if (m_stagingResponsive)
            m_stagingResponsive->splits.append({split, configuredOrientation});
        return split;
    }

    if (type == QStringLiteral("FlipView")) {
        auto* flip = new FlipView(parentWidget);
        flip->setShowNavigationButtons(props.value(QStringLiteral("showButtons")).toBool(true));
        flip->setShowPageIndicator(props.value(QStringLiteral("showIndicator")).toBool(true));
        flip->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(170), 420));
        if (children.isEmpty()) {
            flip->addPage(makeSamplePane(flip, QStringLiteral("Slide 1"), QStringLiteral("First FlipView card")),
                WidgetOwnership::Owned);
            flip->addPage(makeSamplePane(flip, QStringLiteral("Slide 2"), QStringLiteral("Second FlipView card")),
                WidgetOwnership::Owned);
        } else {
            for (const QJsonValue& childVal : children) {
                if (QWidget* child = buildNode(childVal.toObject(), flip, built))
                    flip->addPage(child, WidgetOwnership::Owned);
            }
        }
        connect(flip, &FlipView::currentIndexChanged, this, [this, id](int index) {
            emit interactionEvent(id, QStringLiteral("currentIndexChanged"), index);
        });
        return flip;
    }

    if (type == QStringLiteral("StackView")) {
        auto* stack = new StackView(parentWidget);
        stack->setTransitionAnimationEnabled(false);
        stack->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(150), 420));
        if (children.isEmpty()) {
            stack->setInitialItem(
                makeSamplePane(stack, QStringLiteral("Stack Root View"),
                    QStringLiteral("Supports animated push, pop, and replace navigation.")),
                WidgetOwnership::Owned);
        } else {
            bool first = true;
            for (const QJsonValue& childVal : children) {
                if (QWidget* child = buildNode(childVal.toObject(), stack, built)) {
                    if (first) {
                        stack->setInitialItem(child, WidgetOwnership::Owned);
                        first = false;
                    } else {
                        stack->push(child, WidgetOwnership::Owned);
                    }
                }
            }
        }
        return stack;
    }

    if (type == QStringLiteral("ListDetailView")) {
        auto* ldv = new ListDetailView(parentWidget);
        ldv->setMinimumWidth(0);
        const int paneWidth = qBound(140, props.value(QStringLiteral("listPaneWidth")).toInt(220), 420);
        ldv->setListPaneWidth(paneWidth);
        ldv->setMinimumHeight(qBound(150, props.value(QStringLiteral("minHeight")).toInt(200), 420));

        auto* listLayout = new QVBoxLayout(ldv->listPane());
        listLayout->setContentsMargins(6, 6, 6, 6);
        auto* detailLayout = new QVBoxLayout(ldv->detailPane());
        detailLayout->setContentsMargins(10, 6, 6, 6);

        if (children.isEmpty()) {
            auto* list = new ListView(ldv->listPane());
            list->setModel(
                buildStringModel(list, {QStringLiteral("Inbox"), QStringLiteral("Drafts"), QStringLiteral("Archive")}));
            list->setBorderVisible(true);
            listLayout->addWidget(list);
            detailLayout->addWidget(makeSamplePane(ldv->detailPane(), QStringLiteral("Detail Pane"),
                QStringLiteral("Responsive list-detail split layout.")));
        } else {
            if (QWidget* first = buildNode(children.at(0).toObject(), ldv->listPane(), built))
                listLayout->addWidget(first);
            if (children.size() > 1) {
                for (int i = 1; i < children.size(); ++i) {
                    if (QWidget* detail = buildNode(children.at(i).toObject(), ldv->detailPane(), built))
                        detailLayout->addWidget(detail);
                }
            } else {
                detailLayout->addWidget(makeSamplePane(ldv->detailPane(), QStringLiteral("Detail Pane"),
                    QStringLiteral("Add a second child node for custom detail content.")));
            }
        }
        if (m_stagingResponsive)
            m_stagingResponsive->listDetails.append({ldv, paneWidth});
        return ldv;
    }

    if (type == QStringLiteral("NavigationView")) {
        auto* nav = new NavigationView(parentWidget);
        nav->setMinimumWidth(0);
        nav->setAnimationEnabled(false);
        const auto configuredMode = lookupEnum<NavigationView::DisplayMode>(
            props.value(QStringLiteral("displayMode")).toString(),
            {{"Auto", NavigationView::DisplayMode::Auto}, {"Left", NavigationView::DisplayMode::Left},
                {"LeftCompact", NavigationView::DisplayMode::LeftCompact}, {"Top", NavigationView::DisplayMode::Top}},
            NavigationView::DisplayMode::Left);
        const bool configuredPaneOpen = props.value(QStringLiteral("paneOpen")).toBool(true);
        nav->setDisplayMode(configuredMode);
        nav->setPaneOpen(configuredPaneOpen);
        nav->setExpandedPaneWidth(176);
        nav->setMinimumHeight(qBound(180, props.value(QStringLiteral("minHeight")).toInt(240), 460));

        auto* navPane = new QWidget();
        auto* paneLayout = new QVBoxLayout(navPane);
        paneLayout->setContentsMargins(8, 8, 8, 8);
        paneLayout->setSpacing(4);
        const QStringList navItems = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Home"), QStringLiteral("Analytics"), QStringLiteral("Settings")});
        for (int i = 0; i < navItems.size(); ++i) {
            auto* itemBtn = new Button(navItems.at(i), navPane);
            itemBtn->setFluentStyle(i == 0 ? Button::Accent : Button::Subtle);
            connect(itemBtn, &Button::clicked, this, [this, id, nav, i]() {
                if (nav->contentHost() && i < nav->contentHost()->count())
                    nav->contentHost()->setCurrentIndex(i);
                emit interactionEvent(id, QStringLiteral("navItemClicked"), i);
            });
            paneLayout->addWidget(itemBtn);
        }
        paneLayout->addStretch(1);
        nav->setMainChromeWidget(navPane, WidgetOwnership::Owned);

        if (StackContentHost* host = nav->contentHost()) {
            if (children.isEmpty()) {
                for (int i = 0; i < navItems.size(); ++i) {
                    host->insertPage(i,
                        makeSamplePane(nullptr, navItems.at(i),
                            QStringLiteral("NavigationView page content for %1.").arg(navItems.at(i))),
                        WidgetOwnership::Owned);
                }
            } else {
                for (int i = 0; i < children.size(); ++i) {
                    if (QWidget* child = buildNode(children.at(i).toObject(), nullptr, built))
                        host->insertPage(i, child, WidgetOwnership::Owned);
                }
            }
        }
        if (m_stagingResponsive)
            m_stagingResponsive->navs.append({nav, configuredMode, configuredPaneOpen});
        return nav;
    }

    if (type == QStringLiteral("Window")) {
        const QString title = props.value(QStringLiteral("title")).toString(QStringLiteral("Fluent Window"));
        const QString backdrop = props.value(QStringLiteral("backdropEffect")).toString(QStringLiteral("Mica"));
        auto* frame = new Card(parentWidget);
        frame->setAppearance(backdrop == QStringLiteral("Acrylic")
                ? Card::LayerAlt
                : (backdrop == QStringLiteral("Solid") ? Card::Canvas : Card::Layer));
        frame->setBorderVisible(true);
        frame->setMinimumSize(320, qBound(180, props.value(QStringLiteral("minHeight")).toInt(260), 640));

        auto* frameLayout = new QVBoxLayout(frame);
        frameLayout->setContentsMargins(0, 0, 0, 0);
        frameLayout->setSpacing(0);

        auto* bar = new TitleBar(frame);
        bar->setTitleBarHeight(38);
        auto* titleContent = new QWidget(bar);
        auto* titleLayout = new QHBoxLayout(titleContent);
        titleLayout->setContentsMargins(12, 0, 12, 0);
        titleLayout->setSpacing(8);
        auto* icon = new FontIcon(Typography::Icons::Home, titleContent);
        icon->setIconSize(14);
        titleLayout->addWidget(icon);
        auto* titleLbl = new Label(title, titleContent);
        titleLbl->setFluentTypography(Typography::FontRole::BodyStrong);
        titleLayout->addWidget(titleLbl);
        auto* tagLbl = new Label(backdrop, titleContent);
        tagLbl->setFluentTypography(Typography::FontRole::Caption);
        tagLbl->setTextColorRole(Label::TextColorRole::Secondary);
        titleLayout->addWidget(tagLbl);
        titleLayout->addStretch(1);
        auto* captionGlyphs = new Label(QStringLiteral("—   ▢   ✕"), titleContent);
        captionGlyphs->setFluentTypography(Typography::FontRole::Caption);
        captionGlyphs->setTextColorRole(Label::TextColorRole::Secondary);
        titleLayout->addWidget(captionGlyphs);
        bar->setContentWidget(titleContent);
        frameLayout->addWidget(bar);

        frameLayout->addWidget(new Divider(Qt::Horizontal, frame));

        auto* body = new QWidget(frame);
        body->setAutoFillBackground(false);
        auto* bodyLayout = new QVBoxLayout(body);
        const int pad = qBound(4, props.value(QStringLiteral("padding")).toInt(16), 48);
        const int spacing = qBound(0, props.value(QStringLiteral("spacing")).toInt(12), 48);
        bodyLayout->setContentsMargins(pad, pad, pad, pad);
        bodyLayout->setSpacing(spacing);
        if (children.isEmpty()) {
            bodyLayout->addWidget(makeSamplePane(body, title,
                QStringLiteral("Embedded Fluent Window surface with TitleBar and Backdrop support.")));
        } else {
            for (const QJsonValue& childVal : children) {
                if (QWidget* child = buildNode(childVal.toObject(), body, built))
                    bodyLayout->addWidget(child);
            }
        }
        bodyLayout->addStretch(1);
        frameLayout->addWidget(body, 1);
        return frame;
    }

    if (type == QStringLiteral("ParticleBackdrop")) {
#if defined(FLUENT_QT_HAS_PARTICLES)
        auto* backdrop = new fluent::particles::ParticleBackdrop(parentWidget);
        backdrop->setEffect(lookupEnum<fluent::particles::ParticleBackdrop::Effect>(
            props.value(QStringLiteral("effect")).toString(),
            {{"FlowingRibbons", fluent::particles::ParticleBackdrop::FlowingRibbons},
                {"FloatingDots", fluent::particles::ParticleBackdrop::FloatingDots},
                {"Starfield", fluent::particles::ParticleBackdrop::Starfield}},
            fluent::particles::ParticleBackdrop::FlowingRibbons));
        backdrop->setBackgroundMode(lookupEnum<fluent::particles::ParticleBackdrop::BackgroundMode>(
            props.value(QStringLiteral("backgroundMode")).toString(),
            {{"Transparent", fluent::particles::ParticleBackdrop::Transparent},
                {"Solid", fluent::particles::ParticleBackdrop::Solid}},
            fluent::particles::ParticleBackdrop::Solid));
        backdrop->setGpuAccelerationEnabled(false);
        backdrop->setMaximumFrameRate(30);
        backdrop->setParticleCount(qBound(0, props.value(QStringLiteral("particleCount")).toInt(180), 960));
        backdrop->setAnimationEnabled(props.value(QStringLiteral("animationEnabled")).toBool(true));
        backdrop->setInteractive(props.value(QStringLiteral("interactive")).toBool(true));
        backdrop->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(200), 560));

        auto* layout = new QVBoxLayout(backdrop);
        const int pad = qBound(4, props.value(QStringLiteral("padding")).toInt(16), 48);
        const int spacing = qBound(0, props.value(QStringLiteral("spacing")).toInt(10), 48);
        layout->setContentsMargins(pad, pad, pad, pad);
        layout->setSpacing(spacing);
        if (children.isEmpty()) {
            auto* overlayTitle = new Label(QStringLiteral("ParticleBackdrop"), backdrop);
            overlayTitle->setFluentTypography(Typography::FontRole::Subtitle);
            auto* overlaySub = new Label(QStringLiteral("Animated particle surface behind standard Fluent widgets."),
                backdrop);
            overlaySub->setTextColorRole(Label::TextColorRole::Secondary);
            layout->addWidget(overlayTitle);
            layout->addWidget(overlaySub);
            layout->addStretch(1);
        } else {
            for (const QJsonValue& childVal : children) {
                if (QWidget* child = buildNode(childVal.toObject(), backdrop, built))
                    layout->addWidget(child);
            }
        }
        return backdrop;
#else
        auto* fallback = new Card(parentWidget);
        fallback->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(200), 560));
        auto* layout = new QVBoxLayout(fallback);
        if (children.isEmpty())
            layout->addWidget(makeSamplePane(fallback, QStringLiteral("ParticleBackdrop"),
                QStringLiteral("FluentQt::Particles plugin disabled in this build.")));
        else {
            for (const QJsonValue& childVal : children) {
                if (QWidget* child = buildNode(childVal.toObject(), fallback, built))
                    layout->addWidget(child);
            }
        }
        return fallback;
#endif
    }

    if (type == QStringLiteral("SpatialView") || type == QStringLiteral("SpatialItem")) {
#if defined(FLUENT_QT_HAS_SPATIAL)
        auto* host = new StudioSessionHost(parentWidget);
        const int minH = qBound(160,
            props.value(QStringLiteral("minHeight")).toInt(type == QStringLiteral("SpatialItem") ? 220 : 260), 640);
        host->setMinimumHeight(minH);
        auto* layout = new QVBoxLayout(host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto* session = new fluent::spatial::SpatialSession(host, host);
        const bool active = session->activate();
        fluent::spatial::SpatialView* view = active ? session->createView(host) : nullptr;
        if (!view)
            view = new fluent::spatial::SpatialView(host);
        view->setRenderMode(lookupEnum<fluent::spatial::SpatialView::RenderMode>(
            props.value(QStringLiteral("renderMode")).toString(),
            {{"Auto", fluent::spatial::SpatialView::RenderMode::Auto},
                {"Raster", fluent::spatial::SpatialView::RenderMode::Raster},
                {"OpenGL", fluent::spatial::SpatialView::RenderMode::OpenGL}},
            fluent::spatial::SpatialView::RenderMode::Raster));
        view->setSpatialEnabled(
            type == QStringLiteral("SpatialItem") ? true : props.value(QStringLiteral("spatialEnabled")).toBool(true));
        view->setCameraDistance(qBound(200.0, props.value(QStringLiteral("cameraDistance")).toDouble(1000.0), 4000.0));
        const double rawZoom = props.value(QStringLiteral("zoom")).toDouble(100.0);
        view->setZoom(qBound(0.25, rawZoom <= 4.0 ? rawZoom : (rawZoom / 100.0), 2.5));
        view->setPointerTrackingEnabled(props.value(QStringLiteral("pointerTrackingEnabled")).toBool(true));
        view->setMinimumHeight(minH);
        layout->addWidget(view);

        auto registerSpatialTree = [&](QWidget* rootContent, fluent::spatial::SpatialItem* spItem, int fromIndex) {
            if (!rootContent || !spItem)
                return;
            for (int j = fromIndex; j < built.size(); ++j)
                built[j].resolveBounds = makeSpatialBoundsFn(view, spItem, rootContent, built[j].widget.data());
        };

        auto ensureWidgetSized = [](QWidget* w) {
            if (!w)
                return;
            if (w->minimumWidth() < 160)
                w->setMinimumWidth(240);
            const QSize hint = w->sizeHint();
            w->resize(qMax(240, hint.width()), qMax(120, hint.height()));
        };

        if (type == QStringLiteral("SpatialItem")) {
            QWidget* inner = nullptr;
            const int startIdx = built.size();
            if (!children.isEmpty())
                inner = buildNode(children.first().toObject(), nullptr, built);
            if (!inner)
                inner = makeSamplePane(nullptr, QStringLiteral("SpatialItem Card"),
                    QStringLiteral("3D position, rotation, and hover lift inside SpatialView."));
            ensureWidgetSized(inner);
            auto* spItem = view->addWidget(inner, WidgetOwnership::Owned);
            applySpatialItemProps(spItem, props);
            registerSpatialTree(inner, spItem, startIdx);
            return host;
        }

        if (children.isEmpty()) {
            auto* sample = makeSamplePane(nullptr, QStringLiteral("Spatial 3D Layer"),
                QStringLiteral("Interactive perspective transform with 2D accessible fallback."));
            sample->setFixedSize(260, 140);
            auto* spItem = view->addWidget(sample, WidgetOwnership::Owned);
            applySpatialItemProps(spItem,
                QJsonObject{{QStringLiteral("posZ"), 50}, {QStringLiteral("rotX"), 6}, {QStringLiteral("rotY"), -16},
                    {QStringLiteral("surfaceIntensity"), 75}});
        } else {
            for (int i = 0; i < children.size(); ++i) {
                const QJsonObject childObj = children.at(i).toObject();
                const QString childId = childObj.value(QStringLiteral("id")).toString();
                const QString childType = childObj.value(QStringLiteral("type")).toString();
                if (childType == QStringLiteral("SpatialItem")) {
                    const QJsonArray itemChildren = childObj.value(QStringLiteral("children")).toArray();
                    const int startIdx = built.size();
                    QWidget* inner = itemChildren.isEmpty()
                        ? makeSamplePane(nullptr, childId, QStringLiteral("SpatialItem 3D card"))
                        : buildNode(itemChildren.first().toObject(), nullptr, built);
                    if (!inner)
                        continue;
                    ensureWidgetSized(inner);
                    auto* spItem = view->addWidget(inner, WidgetOwnership::Owned);
                    const QJsonObject itemProps = childObj.value(QStringLiteral("props")).toObject();
                    applySpatialItemProps(spItem, itemProps);
                    if (!itemProps.isEmpty()) {
                        const QSignalBlocker blocker(spItem);
                        GalleryComponentPlayground::applyStudioProperties(childType, spItem, itemProps);
                    }
                    registerSpatialTree(inner, spItem, startIdx);
                    built.append(
                        NodeEntry{childId, childType, inner, makeSpatialBoundsFn(view, spItem, inner, inner), spItem});
                } else {
                    const int startIdx = built.size();
                    QWidget* child = buildNode(childObj, nullptr, built);
                    if (!child)
                        continue;
                    ensureWidgetSized(child);
                    auto* spItem = view->addWidget(child, WidgetOwnership::Owned);
                    const double spreadX = (i - (children.size() - 1) / 2.0) * 90.0;
                    applySpatialItemProps(spItem,
                        QJsonObject{{QStringLiteral("posX"), spreadX}, {QStringLiteral("posZ"), i * 28.0},
                            {QStringLiteral("rotY"), -12.0 + i * 10.0}, {QStringLiteral("surfaceIntensity"), 75}});
                    registerSpatialTree(child, spItem, startIdx);
                }
            }
        }
        return host;
#else
        auto* fallback = new Card(parentWidget);
        fallback->setMinimumHeight(qBound(160, props.value(QStringLiteral("minHeight")).toInt(240), 640));
        auto* layout = new QVBoxLayout(fallback);
        layout->addWidget(
            makeSamplePane(fallback, type, QStringLiteral("FluentQt::Spatial plugin disabled in this build.")));
        return fallback;
#endif
    }

    return nullptr;
}

QWidget* GalleryStudioCanvas::buildControl(const QString& id,
    const QString& type,
    const QJsonObject& props,
    QWidget* parentWidget)
{
    const bool enabled = props.value(QStringLiteral("enabled")).toBool(true);
    auto finish = [enabled](QWidget* widget) -> QWidget* {
        widget->setEnabled(enabled);
        return widget;
    };

    // --- Layout leaves ---
    if (type == QStringLiteral("Divider")) {
        const auto orient = props.value(QStringLiteral("orientation")).toString() == QStringLiteral("Vertical")
            ? Qt::Vertical
            : Qt::Horizontal;
        auto* divider = new Divider(orient, parentWidget);
        if (orient == Qt::Vertical)
            divider->setMinimumHeight(24);
        return finish(divider);
    }

    if (type == QStringLiteral("DataGridToolBar")) {
        auto* bar = new DataGridToolBar(parentWidget);
        bar->setSearchText(props.value(QStringLiteral("searchText")).toString());
        bar->addFilter(QStringLiteral("status"), QStringLiteral("Status"),
            jsonStringList(props.value(QStringLiteral("items")),
                {QStringLiteral("Active"), QStringLiteral("Queued"), QStringLiteral("Done")}));
        connect(bar, &DataGridToolBar::searchTextChanged, this, [this, id](const QString& text) {
            emit interactionEvent(id, QStringLiteral("searchTextChanged"), text);
        });
        return finish(bar);
    }

    if (type == QStringLiteral("PropertyPanel")) {
        auto* panel = new PropertyPanel(parentWidget);
        panel->setLayoutMode(lookupEnum<PropertyPanel::LayoutMode>(props.value(QStringLiteral("layoutMode")).toString(),
            {{"Automatic", PropertyPanel::LayoutMode::Automatic}, {"Stacked", PropertyPanel::LayoutMode::Stacked},
                {"Inline", PropertyPanel::LayoutMode::Inline}},
            PropertyPanel::LayoutMode::Automatic));
        panel->setReadOnly(props.value(QStringLiteral("readOnly")).toBool(false));
        PropertyDefinition titleProp;
        titleProp.key = QStringLiteral("title");
        titleProp.label = QStringLiteral("Title");
        titleProp.group = QStringLiteral("General");
        titleProp.type = PropertyDefinition::Text;
        titleProp.defaultValue = QStringLiteral("Fluent Workspace");
        PropertyDefinition countProp;
        countProp.key = QStringLiteral("columns");
        countProp.label = QStringLiteral("Columns");
        countProp.group = QStringLiteral("General");
        countProp.type = PropertyDefinition::Number;
        countProp.minimum = 1;
        countProp.maximum = 12;
        countProp.defaultValue = 2;
        PropertyDefinition animProp;
        animProp.key = QStringLiteral("motion");
        animProp.label = QStringLiteral("Smooth motion");
        animProp.group = QStringLiteral("Behavior");
        animProp.type = PropertyDefinition::Boolean;
        animProp.defaultValue = true;
        panel->setProperties({titleProp, countProp, animProp});
        connect(panel, &PropertyPanel::valueEdited, this, [this, id](const QString& key, const QVariant& val) {
            emit interactionEvent(id, QStringLiteral("valueEdited"), QStringLiteral("%1=%2").arg(key, val.toString()));
        });
        return finish(panel);
    }

    // --- Basic input ---
    if (type == QStringLiteral("Button")) {
        auto* button = new Button(props.value(QStringLiteral("text")).toString(QStringLiteral("Button")), parentWidget);
        button->setFluentStyle(lookupEnum<Button::ButtonStyle>(props.value(QStringLiteral("fluentStyle")).toString(),
            {{"Standard", Button::Standard}, {"Accent", Button::Accent}, {"Subtle", Button::Subtle}},
            Button::Standard));
        button->setFluentSize(lookupEnum<Button::ButtonSize>(props.value(QStringLiteral("fluentSize")).toString(),
            {{"Small", Button::Small}, {"StandardSize", Button::StandardSize}, {"Large", Button::Large}},
            Button::StandardSize));
        const QString glyph = props.value(QStringLiteral("iconGlyph")).toString();
        if (!glyph.isEmpty()) {
            button->setIconGlyph(glyph);
            button->setProperty("_studioIconGlyph", glyph);
        }
        connect(button, &Button::clicked, this, [this, id]() {
            emit interactionEvent(id, QStringLiteral("clicked"), true);
        });
        connect(button, &Button::toggled, this, [this, id, button](bool on) {
            if (button->isCheckable())
                emit interactionEvent(id, QStringLiteral("toggled"), on);
        });
        return finish(button);
    }

    if (type == QStringLiteral("CompoundButton")) {
        auto* btn = new CompoundButton(props.value(QStringLiteral("text")).toString(QStringLiteral("Build Artifacts")),
            props.value(QStringLiteral("secondaryText"))
                .toString(QStringLiteral("Compile release binaries and verify checksums")),
            parentWidget);
        connect(btn, &Button::clicked, this, [this, id]() {
            emit interactionEvent(id, QStringLiteral("clicked"), true);
        });
        return finish(btn);
    }

    if (type == QStringLiteral("ToggleButton")) {
        auto* button = new ToggleButton(props.value(QStringLiteral("text")).toString(QStringLiteral("Toggle")),
            parentWidget);
        button->setChecked(props.value(QStringLiteral("checked")).toBool(false));
        connect(button, &ToggleButton::toggled, this, [this, id](bool on) {
            emit interactionEvent(id, QStringLiteral("toggled"), on);
        });
        return finish(button);
    }

    if (type == QStringLiteral("DropDownButton") || type == QStringLiteral("SplitButton")
        || type == QStringLiteral("ToggleSplitButton")) {
        const QString text = props.value(QStringLiteral("text")).toString(type);
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Action 1"), QStringLiteral("Action 2"), QStringLiteral("Action 3")});
        auto* menu = new FluentMenu(QString(), parentWidget);
        for (const QString& item : items) {
            QAction* act = menu->addAction(item);
            connect(act, &QAction::triggered, this, [this, id, item]() {
                emit interactionEvent(id, QStringLiteral("menuTriggered"), item);
            });
        }
        if (type == QStringLiteral("DropDownButton")) {
            auto* btn = new DropDownButton(text, parentWidget);
            btn->setMenu(menu);
            return finish(btn);
        }
        if (type == QStringLiteral("SplitButton")) {
            auto* btn = new SplitButton(text, parentWidget);
            btn->setMenu(menu);
            connect(btn, &Button::clicked, this, [this, id]() {
                emit interactionEvent(id, QStringLiteral("clicked"), true);
            });
            return finish(btn);
        }
        auto* btn = new ToggleSplitButton(text, parentWidget);
        btn->setMenu(menu);
        btn->setChecked(props.value(QStringLiteral("checked")).toBool(true));
        connect(btn, &Button::toggled, this, [this, id](bool on) {
            emit interactionEvent(id, QStringLiteral("toggled"), on);
        });
        return finish(btn);
    }

    if (type == QStringLiteral("HyperlinkButton")) {
        auto* link = new HyperlinkButton(props.value(QStringLiteral("text")).toString(QStringLiteral("Learn more")),
            parentWidget);
        connect(link, &HyperlinkButton::clicked, this, [this, id]() {
            emit interactionEvent(id, QStringLiteral("clicked"), true);
        });
        return finish(link);
    }

    if (type == QStringLiteral("RepeatButton")) {
        auto* btn = new RepeatButton(props.value(QStringLiteral("text")).toString(QStringLiteral("Hold to Increment")),
            parentWidget);
        btn->setDelay(qBound(50, props.value(QStringLiteral("delay")).toInt(250), 2000));
        btn->setInterval(qBound(20, props.value(QStringLiteral("interval")).toInt(60), 1000));
        connect(btn, &Button::clicked, this, [this, id]() {
            emit interactionEvent(id, QStringLiteral("repeated"), true);
        });
        return finish(btn);
    }

    if (type == QStringLiteral("CheckBox")) {
        auto* box = new CheckBox(props.value(QStringLiteral("text")).toString(QStringLiteral("CheckBox")),
            parentWidget);
        box->setMinimumWidth(0);
        box->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        box->setChecked(props.value(QStringLiteral("checked")).toBool(false));
        connect(box, &CheckBox::toggled, this, [this, id, box](bool checked) {
            if (!box->isTristate())
                emit interactionEvent(id, QStringLiteral("toggled"), checked);
        });
        connect(box, &QAbstractButton::clicked, this, [this, id, box](bool) {
            if (!box->isTristate())
                return;
            const Qt::CheckState st = box->checkState();
            const QString key = st == Qt::Checked
                ? QStringLiteral("Checked")
                : (st == Qt::PartiallyChecked ? QStringLiteral("PartiallyChecked") : QStringLiteral("Unchecked"));
            emit interactionEvent(id, QStringLiteral("checkStateChanged"), key);
        });
        return finish(box);
    }

    if (type == QStringLiteral("RadioButton")) {
        auto* radio = new RadioButton(props.value(QStringLiteral("text")).toString(QStringLiteral("Radio option")),
            parentWidget);
        radio->setMinimumWidth(0);
        radio->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        radio->setChecked(props.value(QStringLiteral("checked")).toBool(false));
        connect(radio, &RadioButton::toggled, this, [this, id](bool checked) {
            emit interactionEvent(id, QStringLiteral("toggled"), checked);
        });
        return finish(radio);
    }

    if (type == QStringLiteral("ToggleSwitch")) {
        auto* sw = new ToggleSwitch(parentWidget);
        sw->setOnContent(props.value(QStringLiteral("onContent")).toString(QStringLiteral("On")));
        sw->setOffContent(props.value(QStringLiteral("offContent")).toString(QStringLiteral("Off")));
        sw->setIsOn(props.value(QStringLiteral("isOn")).toBool(false));
        connect(sw, &ToggleSwitch::toggled, this, [this, id](bool on) {
            emit interactionEvent(id, QStringLiteral("toggled"), on);
        });
        return finish(sw);
    }

    if (type == QStringLiteral("ComboBox")) {
        auto* combo = new ComboBox(parentWidget);
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Option A"), QStringLiteral("Option B"), QStringLiteral("Option C")});
        combo->addItems(items);
        const int idx = qBound(0, props.value(QStringLiteral("currentIndex")).toInt(0), items.size() - 1);
        combo->setCurrentIndex(idx);
        connect(combo, QOverload<int>::of(&ComboBox::currentIndexChanged), this, [this, id](int index) {
            emit interactionEvent(id, QStringLiteral("currentIndexChanged"), index);
        });
        return finish(combo);
    }

    if (type == QStringLiteral("MultiSelectComboBox")) {
        auto* box = new MultiSelectComboBox(parentWidget);
        box->setPlaceholderText(
            props.value(QStringLiteral("placeholderText")).toString(QStringLiteral("Select modules...")));
        box->setSearchEnabled(props.value(QStringLiteral("searchEnabled")).toBool(true));
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Core"), QStringLiteral("Charts"), QStringLiteral("Spatial"), QStringLiteral("QmlBridge")});
        box->setModel(buildStringModel(box, items));
        box->setSelectedRows({0});
        connect(box, &MultiSelectComboBox::selectedCountChanged, this, [this, id](int count) {
            emit interactionEvent(id, QStringLiteral("selectedCountChanged"), count);
        });
        return finish(box);
    }

    if (type == QStringLiteral("TreeSelect")) {
        auto* select = new TreeSelect(parentWidget);
        select->setPlaceholderText(
            props.value(QStringLiteral("placeholderText")).toString(QStringLiteral("Select category...")));
        auto* model = new QStandardItemModel(select);
        auto* uiRoot = new QStandardItem(QStringLiteral("UI Modules"));
        uiRoot->setData(QStringLiteral("ui"), Qt::UserRole);
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Buttons"), QStringLiteral("Navigation"), QStringLiteral("Charts")});
        for (const QString& itemText : items) {
            auto* child = new QStandardItem(itemText);
            child->setData(itemText.toLower(), Qt::UserRole);
            uiRoot->appendRow(child);
        }
        model->appendRow(uiRoot);
        select->setModel(model);
        select->setKeyRole(Qt::UserRole);
        connect(select, &TreeSelect::selectionChanged, this, [this, id]() {
            emit interactionEvent(id, QStringLiteral("selectionChanged"), true);
        });
        return finish(select);
    }

    if (type == QStringLiteral("Slider")) {
        auto* slider = new Slider(Qt::Horizontal, parentWidget);
        const int minVal = props.value(QStringLiteral("minimum")).toInt(0);
        const int maxVal = qMax(minVal + 1, props.value(QStringLiteral("maximum")).toInt(100));
        slider->setRange(minVal, maxVal);
        slider->setValue(qBound(minVal, props.value(QStringLiteral("value")).toInt(40), maxVal));
        connect(slider, &Slider::valueChanged, this, [this, id](int val) {
            emit interactionEvent(id, QStringLiteral("valueChanged"), val);
        });
        return finish(slider);
    }

    if (type == QStringLiteral("RangeSlider")) {
        auto* slider = new RangeSlider(Qt::Horizontal, parentWidget);
        const int minVal = props.value(QStringLiteral("minimum")).toInt(0);
        const int maxVal = qMax(minVal + 2, props.value(QStringLiteral("maximum")).toInt(100));
        const int lo = qBound(minVal, props.value(QStringLiteral("lowerValue")).toInt(20), maxVal - 1);
        const int hi = qBound(lo + 1, props.value(QStringLiteral("upperValue")).toInt(75), maxVal);
        slider->setRange(minVal, maxVal);
        slider->setValues(lo, hi);
        connect(slider, &RangeSlider::valuesChanged, this, [this, id](int l, int u) {
            emit interactionEvent(id, QStringLiteral("valuesChanged"), QStringLiteral("%1..%2").arg(l).arg(u));
        });
        return finish(slider);
    }

    if (type == QStringLiteral("RatingControl")) {
        auto* rating = new RatingControl(parentWidget);
        const int maxRating = qBound(1, props.value(QStringLiteral("maxRating")).toInt(5), 10);
        rating->setMaxRating(maxRating);
        rating->setValue(qBound(0.0, props.value(QStringLiteral("value")).toDouble(4.0), double(maxRating)));
        connect(rating, &RatingControl::valueChanged, this, [this, id](double val) {
            emit interactionEvent(id, QStringLiteral("valueChanged"), val);
        });
        return finish(rating);
    }

    if (type == QStringLiteral("ColorPicker")) {
        auto* picker = new ColorPicker(parentWidget);
        picker->setAlphaEnabled(props.value(QStringLiteral("alphaEnabled")).toBool(false));
        const QColor color(props.value(QStringLiteral("color")).toString(QStringLiteral("#0062B0")));
        if (color.isValid())
            picker->setColor(color);
        connect(picker, &ColorPicker::colorChanged, this, [this, id](const QColor& c) {
            emit interactionEvent(id, QStringLiteral("colorChanged"), c.name());
        });
        return finish(picker);
    }

    if (type == QStringLiteral("FileDropZone")) {
        auto* zone = new FileDropZone(parentWidget);
        zone->setTitle(props.value(QStringLiteral("title")).toString(QStringLiteral("Drop files here")));
        zone->setDescription(
            props.value(QStringLiteral("description")).toString(QStringLiteral("PNG, SVG, or .ui design files")));
        zone->setHintText(props.value(QStringLiteral("hintText")).toString(QStringLiteral("Max 25 MB per file")));
        zone->setBrowseText(props.value(QStringLiteral("browseText")).toString(QStringLiteral("Browse files")));
        return finish(zone);
    }

    // --- Text fields ---
    if (type == QStringLiteral("Label")) {
        auto* label = new Label(props.value(QStringLiteral("text")).toString(QStringLiteral("Label")), parentWidget);
        const QString typoKey =
            props.value(QStringLiteral("fluentTypography"))
                .toString(props.value(QStringLiteral("typography")).toString());
        const auto configuredRole = parseTypography(typoKey);
        label->setFluentTypography(configuredRole);
        label->setTextColorRole(lookupEnum<Label::TextColorRole>(props.value(QStringLiteral("colorRole")).toString(),
            {{"Default", Label::TextColorRole::Default}, {"Primary", Label::TextColorRole::Primary},
                {"Secondary", Label::TextColorRole::Secondary}, {"Tertiary", Label::TextColorRole::Tertiary},
                {"Disabled", Label::TextColorRole::Disabled}, {"Accent", Label::TextColorRole::Accent},
                {"Critical", Label::TextColorRole::Critical}},
            Label::TextColorRole::Default));
        const bool wrap = props.value(QStringLiteral("wordWrap")).toBool(true);
        label->setWordWrap(wrap);
        label->setMinimumWidth(0);
        if (wrap)
            label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        if (m_stagingResponsive
            && (configuredRole == Typography::FontRole::Display || configuredRole == Typography::FontRole::TitleLarge
                || configuredRole == Typography::FontRole::Title)) {
            m_stagingResponsive->headings.append({label, configuredRole});
        }
        return finish(label);
    }

    if (type == QStringLiteral("LineEdit")) {
        auto* edit = new LineEdit(parentWidget);
        edit->setPlaceholderText(
            props.value(QStringLiteral("placeholderText")).toString(QStringLiteral("Enter text...")));
        edit->setText(props.value(QStringLiteral("text")).toString());
        connect(edit, &LineEdit::textChanged, this, [this, id](const QString& text) {
            emit interactionEvent(id, QStringLiteral("textChanged"), text);
        });
        return finish(edit);
    }

    if (type == QStringLiteral("PasswordBox")) {
        auto* box = new PasswordBox(parentWidget);
        box->setHeader(props.value(QStringLiteral("header")).toString());
        box->setPlaceholderText(props.value(QStringLiteral("placeholderText")).toString(QStringLiteral("Password")));
        connect(box, &PasswordBox::passwordChanged, this, [this, id](const QString&) {
            emit interactionEvent(id, QStringLiteral("passwordChanged"), true);
        });
        return finish(box);
    }

    if (type == QStringLiteral("NumberBox")) {
        auto* box = new NumberBox(parentWidget);
        const double minVal = props.value(QStringLiteral("minimum")).toDouble(0.0);
        const double maxVal = qMax(minVal + 1.0, props.value(QStringLiteral("maximum")).toDouble(1000.0));
        box->setHeader(props.value(QStringLiteral("header")).toString());
        box->setRange(minVal, maxVal);
        box->setValue(qBound(minVal, props.value(QStringLiteral("value")).toDouble(24.0), maxVal));
        connect(box, &NumberBox::valueChanged, this, [this, id](double val) {
            emit interactionEvent(id, QStringLiteral("valueChanged"), val);
        });
        return finish(box);
    }

    if (type == QStringLiteral("TextEdit")) {
        auto* edit = new TextEdit(parentWidget);
        edit->setPlaceholderText(
            props.value(QStringLiteral("placeholderText")).toString(QStringLiteral("Write notes...")));
        edit->setPlainText(props.value(QStringLiteral("plainText")).toString());
        edit->setMinVisibleLines(qBound(2, props.value(QStringLiteral("minVisibleLines")).toInt(3), 12));
        connect(edit, &TextEdit::textChanged, this, [this, id, edit]() {
            emit interactionEvent(id, QStringLiteral("textChanged"), edit->toPlainText());
        });
        return finish(edit);
    }

    if (type == QStringLiteral("AutoSuggestBox")) {
        auto* box = new AutoSuggestBox(parentWidget);
        box->setHeader(props.value(QStringLiteral("header")).toString());
        box->setPlaceholderText(
            props.value(QStringLiteral("placeholderText")).toString(QStringLiteral("Search items...")));
        box->setSuggestions(jsonStringList(props.value(QStringLiteral("suggestions")),
            {QStringLiteral("Button"), QStringLiteral("DataGrid"), QStringLiteral("NavigationView"),
                QStringLiteral("ChartView")}));
        connect(box, &AutoSuggestBox::textChanged, this, [this, id](const QString& text) {
            emit interactionEvent(id, QStringLiteral("textChanged"), text);
        });
        return finish(box);
    }

    if (type == QStringLiteral("TagInput")) {
        auto* tagInput = new TagInput(parentWidget);
        tagInput->setPlaceholderText(
            props.value(QStringLiteral("placeholderText")).toString(QStringLiteral("Add tag...")));
        const QStringList tags = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("C++17"), QStringLiteral("Qt 6"), QStringLiteral("FluentUI")});
        for (const QString& tag : tags)
            tagInput->addItem(tag.toLower(), tag);
        return finish(tagInput);
    }

    if (type == QStringLiteral("CodeBlock")) {
        auto* block = new CodeBlock(
            props.value(QStringLiteral("code")).toString(QStringLiteral("auto* btn = new Button(\"Save\", card);")),
            parentWidget);
        block->setHeaderText(props.value(QStringLiteral("headerText")).toString(QStringLiteral("Snippet")));
        block->setLanguage(props.value(QStringLiteral("language")).toString(QStringLiteral("cpp")));
        block->setAnimationEnabled(false);
        block->setExpanded(props.value(QStringLiteral("expanded")).toBool(true));
        connect(block, &Expander::expandedChanged, this, [this, id](bool on) {
            scheduleLayoutSnapshot();
            emit interactionEvent(id, QStringLiteral("expandedChanged"), on);
        });
        return finish(block);
    }

    // --- Date & time ---
    if (type == QStringLiteral("DatePicker"))
        return finish(new DatePicker(parentWidget));
    if (type == QStringLiteral("TimePicker"))
        return finish(new TimePicker(parentWidget));
    if (type == QStringLiteral("CalendarDatePicker")) {
        auto* picker = new CalendarDatePicker(parentWidget);
        picker->setPlaceholderText(
            props.value(QStringLiteral("placeholderText")).toString(QStringLiteral("Pick a date")));
        picker->setDate(QDate::currentDate());
        connect(picker, &CalendarDatePicker::dateChanged, this, [this, id](const QDate& d) {
            emit interactionEvent(id, QStringLiteral("dateChanged"), d.toString(Qt::ISODate));
        });
        return finish(picker);
    }
    if (type == QStringLiteral("CalendarView")) {
        auto* cal = new CalendarView(parentWidget);
        cal->setSelectedDate(QDate::currentDate());
        connect(cal, &CalendarView::selectedDateChanged, this, [this, id](const QDate& d) {
            emit interactionEvent(id, QStringLiteral("selectedDateChanged"), d.toString(Qt::ISODate));
        });
        return finish(cal);
    }
    if (type == QStringLiteral("DateTimeRangePicker")) {
        auto* picker = new DateTimeRangePicker(parentWidget);
        picker->setTimeEnabled(props.value(QStringLiteral("timeEnabled")).toBool(true));
        picker->setPresetsVisible(props.value(QStringLiteral("presetsVisible")).toBool(true));
        const QDateTime now = QDateTime::currentDateTime();
        picker->setRange(now.addDays(-3), now);
        return finish(picker);
    }

    // --- Navigation ---
    if (type == QStringLiteral("Breadcrumb")) {
        auto* crumb = new Breadcrumb(parentWidget);
        crumb->setItems(jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Home"), QStringLiteral("Settings"), QStringLiteral("Appearance")}));
        connect(crumb, &Breadcrumb::itemClicked, this, [this, id](int index) {
            emit interactionEvent(id, QStringLiteral("itemClicked"), index);
        });
        return finish(crumb);
    }

    if (type == QStringLiteral("Stepper")) {
        auto* stepper = new Stepper(parentWidget);
        const QStringList steps = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Account"), QStringLiteral("Profile"), QStringLiteral("Confirm")});
        for (const QString& step : steps)
            stepper->addItem(step);
        const int cur = qBound(0, props.value(QStringLiteral("currentIndex")).toInt(1), steps.size() - 1);
        stepper->setCurrentIndex(cur);
        for (int i = 0; i < cur; ++i)
            stepper->setItemState(i, Stepper::Completed);
        connect(stepper, &Stepper::stepRequested, this, [this, id, stepper](int index) {
            stepper->setCurrentIndex(index);
            emit interactionEvent(id, QStringLiteral("stepRequested"), index);
        });
        return finish(stepper);
    }

    if (type == QStringLiteral("Pivot")) {
        auto* pivot = new Pivot(parentWidget);
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Overview"), QStringLiteral("Metrics"), QStringLiteral("Logs")});
        for (const QString& item : items)
            pivot->addItem(item);
        pivot->setSelectedIndex(qBound(0, props.value(QStringLiteral("selectedIndex")).toInt(0), items.size() - 1));
        connect(pivot, &Pivot::selectedIndexChanged, this, [this, id](int idx) {
            emit interactionEvent(id, QStringLiteral("selectedIndexChanged"), idx);
        });
        return finish(pivot);
    }

    if (type == QStringLiteral("SelectorBar")) {
        auto* bar = new SelectorBar(parentWidget);
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("All"), QStringLiteral("Active"), QStringLiteral("Archived")});
        for (const QString& item : items)
            bar->addItem(item);
        bar->setSelectedIndex(qBound(0, props.value(QStringLiteral("selectedIndex")).toInt(0), items.size() - 1));
        connect(bar, &SelectorBar::selectedIndexChanged, this, [this, id](int idx) {
            emit interactionEvent(id, QStringLiteral("selectedIndexChanged"), idx);
        });
        return finish(bar);
    }

    if (type == QStringLiteral("TabView")) {
        auto* tabs = new TabView(parentWidget);
        tabs->setTabsClosable(props.value(QStringLiteral("tabsClosable")).toBool(true));
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("main.cpp"), QStringLiteral("studio.json"), QStringLiteral("CMakeLists.txt")});
        for (const QString& item : items)
            tabs->addTab(item);
        tabs->setSelectedIndex(qBound(0, props.value(QStringLiteral("selectedIndex")).toInt(0), items.size() - 1));
        connect(tabs, &TabView::selectedIndexChanged, this, [this, id](int idx) {
            emit interactionEvent(id, QStringLiteral("selectedIndexChanged"), idx);
        });
        return finish(tabs);
    }

    // --- Menus & toolbars ---
    if (type == QStringLiteral("MenuBar")) {
        auto* bar = new FluentMenuBar(parentWidget);
        bar->setBackgroundVisible(true);
        const QStringList menus = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("File"), QStringLiteral("Edit"), QStringLiteral("View"), QStringLiteral("Help")});
        for (const QString& mName : menus) {
            auto* m = new FluentMenu(mName, bar);
            QAction* act = m->addAction(QStringLiteral("%1 Action").arg(mName));
            connect(act, &QAction::triggered, this, [this, id, mName]() {
                emit interactionEvent(id, QStringLiteral("menuTriggered"), mName);
            });
            bar->addMenu(m);
        }
        return finish(bar);
    }

    if (type == QStringLiteral("CommandBar")) {
        auto* bar = new CommandBar(parentWidget);
        bar->setLabelPosition(props.value(QStringLiteral("labelPosition")).toString() == QStringLiteral("Collapsed")
                ? CommandBar::LabelPosition::Collapsed
                : CommandBar::LabelPosition::Right);
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("New"), QStringLiteral("Save"), QStringLiteral("Share"), QStringLiteral("Settings")});
        for (const QString& item : items) {
            auto* act = new QAction(item, bar);
            connect(act, &QAction::triggered, this, [this, id, item]() {
                emit interactionEvent(id, QStringLiteral("commandTriggered"), item);
            });
            bar->addPrimaryAction(act);
        }
        return finish(bar);
    }

    // --- Status & info ---
    if (type == QStringLiteral("InfoBar")) {
        auto* bar = new InfoBar(parentWidget);
        bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        bar->setMinimumWidth(0);
        bar->setTitle(props.value(QStringLiteral("title")).toString(QStringLiteral("Status")));
        bar->setMessage(props.value(QStringLiteral("message")).toString(QStringLiteral("Everything is up to date.")));
        bar->setSeverity(lookupEnum<InfoBar::InfoBarSeverity>(props.value(QStringLiteral("severity")).toString(),
            {{"Informational", InfoBar::Informational}, {"Success", InfoBar::Success}, {"Warning", InfoBar::Warning},
                {"Error", InfoBar::Error}},
            InfoBar::Informational));
        bar->setIsClosable(props.value(QStringLiteral("isClosable")).toBool(false));
        if (props.contains(QStringLiteral("singleLine")))
            bar->setSingleLine(props.value(QStringLiteral("singleLine")).toBool());
        bar->setIsOpen(true);
        if (m_stagingResponsive)
            m_stagingResponsive->infoBars.append({bar, bar->singleLine()});
        return finish(bar);
    }

    if (type == QStringLiteral("InfoBadge")) {
        auto* badge = new InfoBadge(parentWidget);
        badge->setValue(qBound(0, props.value(QStringLiteral("value")).toInt(5), 999));
        badge->setStatus(lookupEnum<InfoBadge::InfoBadgeStatus>(props.value(QStringLiteral("status")).toString(),
            {{"Informational", InfoBadge::InfoBadgeStatus::Informational},
                {"Attention", InfoBadge::InfoBadgeStatus::Attention}, {"Caution", InfoBadge::InfoBadgeStatus::Caution},
                {"Success", InfoBadge::InfoBadgeStatus::Success}, {"Critical", InfoBadge::InfoBadgeStatus::Critical}},
            InfoBadge::InfoBadgeStatus::Informational));
        badge->setDisplayMode(lookupEnum<InfoBadge::InfoBadgeDisplayMode>(
            props.value(QStringLiteral("displayMode")).toString(),
            {{"Auto", InfoBadge::InfoBadgeDisplayMode::Auto}, {"Dot", InfoBadge::InfoBadgeDisplayMode::Dot},
                {"Value", InfoBadge::InfoBadgeDisplayMode::Value}},
            InfoBadge::InfoBadgeDisplayMode::Value));
        return finish(badge);
    }

    if (type == QStringLiteral("Avatar")) {
        auto* avatar = new Avatar(parentWidget);
        avatar->setName(props.value(QStringLiteral("name")).toString(QStringLiteral("Fluent User")));
        avatar->setAvatarSize(lookupEnum<Avatar::AvatarSize>(props.value(QStringLiteral("avatarSize")).toString(),
            {{"Small", Avatar::AvatarSize::Small}, {"Medium", Avatar::AvatarSize::Medium},
                {"Large", Avatar::AvatarSize::Large}, {"ExtraLarge", Avatar::AvatarSize::ExtraLarge}},
            Avatar::AvatarSize::Medium));
        avatar->setPresence(lookupEnum<Avatar::PresenceStatus>(props.value(QStringLiteral("presence")).toString(),
            {{"None", Avatar::PresenceStatus::None}, {"Available", Avatar::PresenceStatus::Available},
                {"Away", Avatar::PresenceStatus::Away}, {"Busy", Avatar::PresenceStatus::Busy},
                {"DoNotDisturb", Avatar::PresenceStatus::DoNotDisturb}, {"Offline", Avatar::PresenceStatus::Offline}},
            Avatar::PresenceStatus::Available));
        return finish(avatar);
    }

    if (type == QStringLiteral("ProgressBar")) {
        auto* bar = new ProgressBar(parentWidget);
        bar->setMinimum(props.value(QStringLiteral("minimum")).toDouble(0.0));
        bar->setMaximum(qMax(1.0, props.value(QStringLiteral("maximum")).toDouble(100.0)));
        bar->setValue(props.value(QStringLiteral("value")).toDouble(64.0));
        return finish(bar);
    }

    if (type == QStringLiteral("ProgressRing")) {
        auto* ring = new ProgressRing(parentWidget);
        ring->setMinimum(props.value(QStringLiteral("minimum")).toInt(0));
        ring->setMaximum(qMax(1, props.value(QStringLiteral("maximum")).toInt(100)));
        ring->setValue(props.value(QStringLiteral("value")).toInt(68));
        ring->setIsIndeterminate(false);
        ring->setIsActive(true);
        ring->setBackgroundVisible(true);
        return finish(ring);
    }

    if (type == QStringLiteral("Shimmer")) {
        auto* shimmer = new Shimmer(parentWidget);
        shimmer->setShimmerTemplate(lookupEnum<Shimmer::ShimmerTemplate>(
            props.value(QStringLiteral("shimmerTemplate")).toString(),
            {{"AvatarTextRow", Shimmer::ShimmerTemplate::AvatarTextRow},
                {"ImageCard", Shimmer::ShimmerTemplate::ImageCard}, {"TextBlock", Shimmer::ShimmerTemplate::TextBlock}},
            Shimmer::ShimmerTemplate::AvatarTextRow));
        shimmer->setActive(props.value(QStringLiteral("active")).toBool(true));
        shimmer->setMinimumHeight(84);
        return finish(shimmer);
    }

    if (type == QStringLiteral("SplashScreen")) {
        auto* host = new Card(parentWidget);
        host->setMinimumHeight(qBound(140, props.value(QStringLiteral("minHeight")).toInt(190), 360));
        auto* splash = new SplashScreen(host);
        splash->setTitle(props.value(QStringLiteral("title")).toString(QStringLiteral("Fluent-Qt")));
        splash->setSubtitle(
            props.value(QStringLiteral("subtitle")).toString(QStringLiteral("Initializing UI workspace...")));
        splash->setProgress(68, 100);
        splash->show();
        return finish(host);
    }

    // --- Collections ---
    if (type == QStringLiteral("ListView")) {
        auto* list = new ListView(parentWidget);
        list->setModel(buildStringModel(list,
            jsonStringList(props.value(QStringLiteral("items")),
                {QStringLiteral("Design tokens"), QStringLiteral("Adaptive layout"),
                    QStringLiteral("Native performance")})));
        list->setBorderVisible(true);
        list->setBackgroundVisible(true);
        list->setMinimumHeight(qBound(96, props.value(QStringLiteral("minHeight")).toInt(156), 420));
        connect(list, &ListView::clicked, this, [this, id](const QModelIndex& idx) {
            emit interactionEvent(id, QStringLiteral("itemClicked"), idx.row());
        });
        return finish(list);
    }

    if (type == QStringLiteral("GridView")) {
        auto* grid = new GridView(parentWidget);
        grid->setHeaderText(props.value(QStringLiteral("headerText")).toString(QStringLiteral("Tiles")));
        grid->setModel(buildStringModel(grid,
            jsonStringList(props.value(QStringLiteral("items")),
                {QStringLiteral("Alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma"), QStringLiteral("Delta")})));
        grid->setBorderVisible(true);
        grid->setBackgroundVisible(true);
        grid->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(170), 420));
        return finish(grid);
    }

    if (type == QStringLiteral("FlowView")) {
        auto* flow = new FlowView(parentWidget);
        flow->setHeaderText(props.value(QStringLiteral("headerText")).toString(QStringLiteral("Flow Tags")));
        flow->setModel(buildStringModel(flow,
            jsonStringList(props.value(QStringLiteral("items")),
                {QStringLiteral("Tokens"), QStringLiteral("Animation"), QStringLiteral("Widgets"),
                    QStringLiteral("WebAssembly")})));
        flow->setBorderVisible(true);
        flow->setMinimumHeight(qBound(110, props.value(QStringLiteral("minHeight")).toInt(150), 420));
        return finish(flow);
    }

    if (type == QStringLiteral("TreeView")) {
        auto* tree = new TreeView(parentWidget);
        auto* model = new QStandardItemModel(tree);
        auto* rootItem = new QStandardItem(QStringLiteral("Workspace"));
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("src/"), QStringLiteral("app/"), QStringLiteral("tests/")});
        for (const QString& item : items)
            rootItem->appendRow(new QStandardItem(item));
        model->appendRow(rootItem);
        tree->setModel(model);
        tree->expandAll();
        tree->setBorderVisible(true);
        tree->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(175), 420));
        return finish(tree);
    }

    if (type == QStringLiteral("DataGrid")) {
        auto* grid = new DataGrid(parentWidget);
        auto* model = new QStandardItemModel(3, 3, grid);
        model->setHorizontalHeaderLabels(
            {QStringLiteral("Module"), QStringLiteral("Qt Target"), QStringLiteral("Status")});
        const QList<QStringList> rows = {
            {QStringLiteral("BasicInput"), QStringLiteral("Qt 6.9"), QStringLiteral("Ready")},
            {QStringLiteral("Charts"), QStringLiteral("Qt 6.9"), QStringLiteral("Verified")},
            {QStringLiteral("Collections"), QStringLiteral("Qt 6.9"), QStringLiteral("Ready")},
        };
        for (int r = 0; r < rows.size(); ++r) {
            for (int c = 0; c < rows[r].size(); ++c)
                model->setItem(r, c, new QStandardItem(rows[r][c]));
        }
        grid->setModel(model);
        grid->setBorderVisible(true);
        grid->setMinimumHeight(qBound(140, props.value(QStringLiteral("minHeight")).toInt(190), 420));
        return finish(grid);
    }

    if (type == QStringLiteral("Timeline")) {
        auto* timeline = new Timeline(parentWidget);
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Queued"), QStringLiteral("Compiling WASM"), QStringLiteral("Published")});
        auto* model = new QStandardItemModel(items.size(), 1, timeline);
        for (int i = 0; i < items.size(); ++i) {
            const QModelIndex idx = model->index(i, 0);
            model->setData(idx, items.at(i));
            model->setData(idx, QStringLiteral("Pipeline stage %1").arg(i + 1), Timeline::DescriptionRole);
            model->setData(idx, QStringLiteral("12:0%1").arg(i), Timeline::TimestampRole);
            model->setData(idx, i == 0 ? Timeline::Success : Timeline::Active, Timeline::StatusRole);
        }
        timeline->setModel(model);
        timeline->setMinimumHeight(qBound(140, props.value(QStringLiteral("minHeight")).toInt(200), 420));
        return finish(timeline);
    }

    if (type == QStringLiteral("Transfer")) {
        auto* transfer = new Transfer(parentWidget);
        transfer->setSourceHeader(props.value(QStringLiteral("sourceHeader")).toString(QStringLiteral("Available")));
        transfer->setTargetHeader(props.value(QStringLiteral("targetHeader")).toString(QStringLiteral("Selected")));
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma"), QStringLiteral("Delta")});
        transfer->setModel(buildStringModel(transfer, items, true));
        if (!items.isEmpty())
            transfer->setSelectedKeys({items.first()});
        transfer->setMinimumHeight(qBound(180, props.value(QStringLiteral("minHeight")).toInt(220), 420));
        connect(transfer, &Transfer::selectedKeysChanged, this, [this, id](const QStringList& keys) {
            emit interactionEvent(id, QStringLiteral("selectedKeysChanged"), keys.size());
        });
        return finish(transfer);
    }

    if (type == QStringLiteral("FileListView")) {
        auto* files = new FileListView(parentWidget);
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("FluentQt.wasm"), QStringLiteral("design-tokens.json")});
        auto* model = new QStandardItemModel(items.size(), 1, files);
        for (int i = 0; i < items.size(); ++i) {
            const QModelIndex idx = model->index(i, 0);
            model->setData(idx, items.at(i));
            model->setData(idx, QStringLiteral("Ready · 1.2 MB"), FileListView::MetadataRole);
            model->setData(idx, 100, FileListView::ProgressRole);
        }
        files->setModel(model);
        files->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(150), 420));
        return finish(files);
    }

    // --- Charts (all 9 chart types share configureChart helper) ---
    if (type == QStringLiteral("LineChart"))
        return finish(configureChart(new LineChart(parentWidget), props, 200));
    if (type == QStringLiteral("AreaChart"))
        return finish(configureChart(new AreaChart(parentWidget), props, 200));
    if (type == QStringLiteral("BarChart"))
        return finish(configureChart(new BarChart(parentWidget), props, 200));
    if (type == QStringLiteral("HorizontalBarChart"))
        return finish(configureChart(new HorizontalBarChart(parentWidget), props, 200));
    if (type == QStringLiteral("PieChart"))
        return finish(configureChart(new PieChart(parentWidget), props, 200));
    if (type == QStringLiteral("DonutChart"))
        return finish(configureChart(new DonutChart(parentWidget), props, 200));
    if (type == QStringLiteral("ScatterChart"))
        return finish(configureChart(new ScatterChart(parentWidget), props, 200));
    if (type == QStringLiteral("Sparkline"))
        return finish(configureChart(new Sparkline(parentWidget), props, 76));
    if (type == QStringLiteral("ChartView")) {
        auto* chart = new ChartView(parentWidget);
        chart->setChartType(lookupEnum<ChartView::ChartType>(props.value(QStringLiteral("chartType")).toString(),
            {{"Line", ChartView::Line}, {"Area", ChartView::Area}, {"Bar", ChartView::Bar}, {"Pie", ChartView::Pie},
                {"Donut", ChartView::Donut}},
            ChartView::Line));
        return finish(configureChart(chart, props, 200));
    }

    // --- Scrolling ---
    if (type == QStringLiteral("PipsPager")) {
        auto* pager = new PipsPager(parentWidget);
        pager->setNumberOfPages(qBound(2, props.value(QStringLiteral("pages")).toInt(6), 20));
        pager->setSelectedPageIndex(0);
        connect(pager, &PipsPager::selectedPageIndexChanged, this, [this, id](int idx) {
            emit interactionEvent(id, QStringLiteral("pageChanged"), idx);
        });
        return finish(pager);
    }

    if (type == QStringLiteral("ImageViewer")) {
        auto* viewer = new ImageViewer(parentWidget);
        viewer->setPixmap(createSamplePreviewPixmap());
        viewer->setFrameVisible(true);
        viewer->setMinimumHeight(qBound(140, props.value(QStringLiteral("minHeight")).toInt(180), 420));
        return finish(viewer);
    }

    if (type == QStringLiteral("ScrollBar")) {
        const auto orient = props.value(QStringLiteral("orientation")).toString() == QStringLiteral("Vertical")
            ? Qt::Vertical
            : Qt::Horizontal;
        auto* sb = new ScrollBar(orient, parentWidget);
        sb->setRange(0, 100);
        sb->setValue(35);
        sb->setOpacity(1.0);
        if (orient == Qt::Vertical)
            sb->setMinimumHeight(96);
        else
            sb->setMinimumHeight(18);
        return finish(sb);
    }

    if (type == QStringLiteral("AnnotatedScrollBar")) {
        auto* asb = new AnnotatedScrollBar(parentWidget);
        asb->setRange(0, 100);
        asb->setLabels({
            AnnotatedScrollBarLabel(QStringLiteral("Top"), 0),
            AnnotatedScrollBarLabel(QStringLiteral("Middle"), 50),
            AnnotatedScrollBarLabel(QStringLiteral("End"), 95),
        });
        asb->setMinimumWidth(84);
        asb->setMinimumHeight(140);
        return finish(asb);
    }

    // --- Interactive Overlays / Modals / Flyouts Trigger Buttons ---
    if (type == QStringLiteral("Menu") || type == QStringLiteral("CommandBarFlyout")
        || type == QStringLiteral("CommandPalette") || type == QStringLiteral("Toast")
        || type == QStringLiteral("ToolTip") || type == QStringLiteral("DrawerView")
        || type == QStringLiteral("ContentDialog") || type == QStringLiteral("Dialog")
        || type == QStringLiteral("Flyout") || type == QStringLiteral("Popup") || type == QStringLiteral("TeachingTip")
        || type == QStringLiteral("CoachMark")) {
        const QString btnLabel = props.value(QStringLiteral("text")).toString(QStringLiteral("Open %1").arg(type));
        auto* trigger = new Button(btnLabel, parentWidget);
        trigger->setFluentStyle(Button::Standard);
        connect(trigger, &Button::clicked, this, [this, id, type, props, trigger]() {
            QWidget* hostWin = trigger->window() ? trigger->window() : this;
            if (type == QStringLiteral("Toast")) {
                Toast::showToast(this,
                    props.value(QStringLiteral("message")).toString(QStringLiteral("Studio Toast notification")),
                    Toast::Informational);
            } else if (type == QStringLiteral("ContentDialog")) {
                auto* dlg = new ContentDialog(hostWin);
                dlg->setTitle(props.value(QStringLiteral("title")).toString(QStringLiteral("Confirm Action")));
                dlg->setContent(new Label(
                    props.value(QStringLiteral("message"))
                        .toString(QStringLiteral("Triggered from Page Studio canvas.")),
                    dlg));
                dlg->setPrimaryButtonText(QStringLiteral("OK"));
                dlg->setCloseButtonText(QStringLiteral("Cancel"));
                connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
                dlg->open();
            } else if (type == QStringLiteral("Dialog")) {
                auto* dlg = new Dialog(hostWin);
                dlg->setModal(true);
                dlg->setClosePolicy(Dialog::CloseOnPressOutside | Dialog::CloseOnEscape);
                auto* layout = new QVBoxLayout(dlg);
                layout->setContentsMargins(20, 18, 20, 18);
                layout->addWidget(makeSamplePane(dlg, QStringLiteral("Fluent Dialog"),
                    props.value(QStringLiteral("message"))
                        .toString(QStringLiteral("Click outside or press Esc to close."))));
                connect(dlg, &Dialog::isOpenChanged, dlg, [dlg](bool open) {
                    if (!open)
                        dlg->deleteLater();
                });
                dlg->setIsOpen(true);
            } else if (type == QStringLiteral("Flyout")) {
                auto* flyout = new Flyout(hostWin);
                auto* layout = new QVBoxLayout(flyout);
                layout->setContentsMargins(14, 12, 14, 12);
                layout->addWidget(new Label(
                    props.value(QStringLiteral("message")).toString(QStringLiteral("Anchored Fluent Flyout")), flyout));
                connect(flyout, &Popup::closed, flyout, &QObject::deleteLater);
                flyout->showAt(trigger);
            } else if (type == QStringLiteral("TeachingTip")) {
                auto* tip = new TeachingTip(hostWin);
                tip->setCardSize(QSize(280, 116));
                tip->setLightDismissEnabled(true);
                auto* layout = new QVBoxLayout(tip->contentHost());
                layout->setContentsMargins(14, 12, 14, 12);
                layout->addWidget(makeSamplePane(tip->contentHost(), QStringLiteral("TeachingTip"),
                    props.value(QStringLiteral("message"))
                        .toString(QStringLiteral("Contextual onboarding guidance."))));
                connect(tip, &Popup::closed, tip, &QObject::deleteLater);
                tip->showAt(trigger);
            } else if (type == QStringLiteral("CoachMark")) {
                auto* coach = new CoachMark(hostWin);
                coach->setCardSize(QSize(280, 118));
                coach->setTarget(trigger);
                auto* layout = new QVBoxLayout(coach->contentHost());
                layout->setContentsMargins(14, 12, 14, 12);
                auto* closeBtn = new Button(QStringLiteral("Got it"), coach->contentHost());
                layout->addWidget(
                    new Label(QStringLiteral("CoachMark spotlight anchored to button"), coach->contentHost()));
                layout->addWidget(closeBtn);
                connect(closeBtn, &Button::clicked, coach, &CoachMark::close);
                connect(coach, &CoachMark::openChanged, coach, [coach](bool open) {
                    if (!open)
                        coach->deleteLater();
                });
                coach->open();
            } else if (type == QStringLiteral("Menu")) {
                auto* menu = new FluentMenu(QString(), trigger);
                menu->addAction(QStringLiteral("Copy"));
                menu->addAction(QStringLiteral("Duplicate"));
                menu->popup(trigger->mapToGlobal(QPoint(0, trigger->height())));
            } else if (type == QStringLiteral("CommandBarFlyout")) {
                auto* flyout = new CommandBarFlyout(trigger);
                flyout->setAlwaysExpanded(true);
                flyout->addPrimaryAction(new QAction(QStringLiteral("Cut"), flyout));
                flyout->addPrimaryAction(new QAction(QStringLiteral("Copy"), flyout));
                flyout->addSecondaryAction(new QAction(QStringLiteral("Properties"), flyout));
                flyout->showAt(trigger);
            } else if (type == QStringLiteral("CommandPalette")) {
                auto* palette = new CommandPalette(hostWin);
                palette->addCommand(new QAction(QStringLiteral("Toggle Theme"), palette), QStringLiteral("Appearance"));
                palette->addCommand(new QAction(QStringLiteral("Export C++ Code"), palette), QStringLiteral("Studio"));
                connect(palette, &Popup::closed, palette, &QObject::deleteLater);
                palette->setIsOpen(true);
            } else if (type == QStringLiteral("ToolTip")) {
                auto* tip = ToolTip::attach(trigger,
                    props.value(QStringLiteral("text")).toString(QStringLiteral("Fluent ToolTip preview")),
                    ToolTip::Above);
                if (tip) {
                    tip->move(trigger->mapToGlobal(QPoint(0, -38)));
                    tip->show();
                }
            } else if (type == QStringLiteral("DrawerView")) {
                auto* drawer = new DrawerView(hostWin);
                drawer->setDrawerLength(260);
                drawer->setClosePolicy(DrawerView::CloseOnPressOutside | DrawerView::CloseOnEscape);
                drawer->setContentWidget(
                    makeSamplePane(nullptr, QStringLiteral("DrawerView Panel"),
                        QStringLiteral("Slide-out Fluent drawer anchored to window edge.")),
                    WidgetOwnership::Owned);
                connect(drawer, &DrawerView::isOpenChanged, drawer, [drawer](bool open) {
                    if (!open)
                        drawer->deleteLater();
                });
                drawer->setIsOpen(true);
            } else {
                auto* popup = new Popup(hostWin);
                popup->setClosePolicy(Popup::CloseOnPressOutside | Popup::CloseOnEscape);
                auto* layout = new QVBoxLayout(popup);
                layout->setContentsMargins(14, 12, 14, 12);
                layout->addWidget(new Label(QStringLiteral("Fluent Popup surface"), popup));
                popup->setPosition(trigger, QPoint(0, trigger->height() + 4));
                connect(popup, &Popup::closed, popup, &QObject::deleteLater);
                popup->setIsOpen(true);
            }
            emit interactionEvent(id, QStringLiteral("overlayTriggered"), type);
        });
        return finish(trigger);
    }

    // --- Windowing & Foundation ---
    if (type == QStringLiteral("TitleBar")) {
        auto* bar = new TitleBar(parentWidget);
        bar->setTitleBarHeight(40);
        auto* title = new Label(props.value(QStringLiteral("title")).toString(QStringLiteral("Fluent Studio Window")),
            bar);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        bar->setContentWidget(title);
        return finish(bar);
    }

    if (type == QStringLiteral("FontIcon")) {
        const QString glyph = resolveIconGlyph(
            props.value(QStringLiteral("glyph")).toString(QStringLiteral("Settings")));
        auto* icon = new FontIcon(glyph, parentWidget);
        icon->setIconSize(qBound(12, props.value(QStringLiteral("iconSize")).toInt(24), 64));
        return finish(icon);
    }

    // --- Extensions ---
    if (type == QStringLiteral("QmlView")) {
        const int minH = qBound(96, props.value(QStringLiteral("minHeight")).toInt(160), 420);
#if defined(FLUENT_QT_HAS_QML_BRIDGE)
        auto* host = new StudioSessionHost(parentWidget);
        host->setMinimumHeight(minH);
        auto* layout = new QVBoxLayout(host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto* input = new LineEdit(host);
        input->setText(props.value(QStringLiteral("contextText")).toString(QStringLiteral("Shared Fluent-Qt title")));
        input->hide();
        auto* progress = new ProgressBar(host);
        progress->setRange(0, 100);
        progress->setValue(40);
        progress->hide();

        auto* session = new fluent::qmlbridge::QmlSession(host, host);
        QString err;
        fluent::qmlbridge::QmlView* view = session->activate(&err) ? session->createView(host) : nullptr;
        if (view) {
            view->setMinimumHeight(minH);
            view->setContextObject(QStringLiteral("widgetInput"), input);
            view->setContextObject(QStringLiteral("widgetProgress"), progress);
            QString sample = props.value(QStringLiteral("sample")).toString(QStringLiteral("Basic"));
            static const QSet<QString> allowedSamples{QStringLiteral("Basic"), QStringLiteral("Theme"),
                QStringLiteral("Context"), QStringLiteral("Action")};
            if (!allowedSamples.contains(sample))
                sample = QStringLiteral("Basic");
            view->load(QUrl(QStringLiteral("qrc:/app/qml-bridge/%1.qml").arg(sample)));
            layout->addWidget(view);
        } else {
            QWidget* placeholder = makeSamplePane(host, QStringLiteral("QmlView"),
                err.isEmpty() ? QStringLiteral("Qt Quick surface unavailable in current backend.") : err);
            placeholder->setMinimumHeight(minH);
            layout->addWidget(placeholder);
        }
        return finish(host);
#else
        QWidget* fallback = makeSamplePane(parentWidget, QStringLiteral("QmlView"),
            QStringLiteral("FluentQt::QmlBridge plugin disabled in this build."));
        fallback->setMinimumHeight(minH);
        return finish(fallback);
#endif
    }

    return nullptr;
}

void GalleryStudioCanvas::applyResponsiveLayout(int viewportWidth)
{
    const int width = qMax(240, viewportWidth);
    const LayoutTier tier = resolveLayoutTier(width);
    const bool tierChanged = (tier != m_layoutTier) || (m_lastLayoutWidth < 0);
    m_layoutTier = tier;
    m_lastLayoutWidth = width;

    if (!m_responsive || !m_rootWidget)
        return;

    const bool compact = (tier == LayoutTier::Compact);

    // 1. Leaf & chrome controls (typography, field labels, navigation & split surfaces)
    for (const auto& heading : std::as_const(m_responsive->headings)) {
        if (!heading.label)
            continue;
        Typography::FontRole effectiveRole = heading.configuredRole;
        if (tier == LayoutTier::Medium) {
            if (heading.configuredRole == Typography::FontRole::Display)
                effectiveRole = Typography::FontRole::TitleLarge;
            else if (heading.configuredRole == Typography::FontRole::TitleLarge)
                effectiveRole = Typography::FontRole::Title;
        } else if (compact) {
            if (heading.configuredRole == Typography::FontRole::Display)
                effectiveRole = Typography::FontRole::Title;
            else if (heading.configuredRole == Typography::FontRole::TitleLarge
                || heading.configuredRole == Typography::FontRole::Title)
                effectiveRole = Typography::FontRole::Subtitle;
        }
        heading.label->setFluentTypography(effectiveRole);
    }

    for (const auto& fb : std::as_const(m_responsive->fields)) {
        if (!fb.field)
            continue;
        fb.field->setLabelPosition(tier == LayoutTier::Expanded ? fb.configuredPosition : Field::LabelPosition::Top);
    }

    for (const auto& nb : std::as_const(m_responsive->navs)) {
        if (!nb.nav || nb.configuredMode != NavigationView::DisplayMode::Auto)
            continue;
        if (tier == LayoutTier::Expanded) {
            nb.nav->setDisplayMode(NavigationView::DisplayMode::Left);
            nb.nav->setPaneOpen(nb.configuredPaneOpen);
        } else if (tier == LayoutTier::Medium) {
            nb.nav->setDisplayMode(NavigationView::DisplayMode::LeftCompact);
            nb.nav->setPaneOpen(false);
        } else {
            nb.nav->setDisplayMode(NavigationView::DisplayMode::Top);
            nb.nav->setPaneOpen(false);
        }
    }

    for (const auto& ld : std::as_const(m_responsive->listDetails)) {
        if (!ld.view)
            continue;
        int w = ld.configuredPaneWidth;
        if (tier == LayoutTier::Medium)
            w = qMin(ld.configuredPaneWidth, 172);
        else if (compact)
            w = qMin(ld.configuredPaneWidth, 128);
        ld.view->setListPaneWidth(w);
    }

    for (const auto& sb : std::as_const(m_responsive->splits)) {
        if (!sb.split)
            continue;
        sb.split->setOrientation(compact ? Qt::Vertical : sb.configuredOrientation);
    }

    const int availBannerWidth = qMax(200, width - (compact ? 24 : (tier == LayoutTier::Medium ? 32 : 40)));
    for (const auto& ib : std::as_const(m_responsive->infoBars)) {
        if (!ib.bar)
            continue;
        ib.bar->setPreferredWidth(availBannerWidth);
        ib.bar->setSingleLine(tier == LayoutTier::Expanded && ib.configuredSingleLine);
        ib.bar->updateGeometry();
    }

    // 2. Grids & HBox rows (repack bottom-up before outer VBox/Card containers)
    for (int i = m_responsive->grids.size() - 1; i >= 0; --i) {
        const auto& grid = m_responsive->grids.at(i);
        if (!grid.host || !grid.layout)
            continue;
        int effectiveCols = grid.configuredColumns;
        if (tier == LayoutTier::Medium)
            effectiveCols = qMin(grid.configuredColumns, 2);
        else if (compact)
            effectiveCols = 1;
        effectiveCols = qMax(1, effectiveCols);

        const int pad = scaleDensity(grid.configuredPadding, tier, 6, 0.8, 0.6);
        const int sp = scaleDensity(grid.configuredSpacing, tier, 6, 0.85, 0.75);
        grid.layout->setContentsMargins(pad, pad, pad, pad);
        grid.layout->setSpacing(sp);

        if (tierChanged) {
            while (QLayoutItem* item = grid.layout->takeAt(0))
                delete item;
            for (int c = 0; c < 4; ++c)
                grid.layout->setColumnStretch(c, 0);
            int cell = 0;
            for (const auto& itemPtr : grid.items) {
                if (!itemPtr)
                    continue;
                grid.layout->addWidget(itemPtr.data(), cell / effectiveCols, cell % effectiveCols);
                ++cell;
            }
            for (int c = 0; c < effectiveCols; ++c)
                grid.layout->setColumnStretch(c, 1);
        }
        grid.layout->activate();
        grid.host->updateGeometry();
    }

    for (int i = m_responsive->hboxes.size() - 1; i >= 0; --i) {
        const auto& hbox = m_responsive->hboxes.at(i);
        if (!hbox.host || !hbox.layout)
            continue;
        const int pad = scaleDensity(hbox.configuredPadding, tier, 6, 0.8, 0.6);
        const int sp = scaleDensity(hbox.configuredSpacing, tier, 6, 0.85, 0.7);
        hbox.layout->setContentsMargins(pad, pad, pad, pad);
        hbox.layout->setSpacing(sp);

        if (tierChanged) {
            while (QLayoutItem* item = hbox.layout->takeAt(0))
                delete item;
            hbox.layout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            for (const auto& itemPtr : hbox.items) {
                if (!itemPtr)
                    continue;
                QWidget* child = itemPtr.data();
                if (auto* lbl = qobject_cast<Label*>(child)) {
                    lbl->setWordWrap(compact);
                    lbl->setSizePolicy(compact ? QSizePolicy::Ignored : QSizePolicy::Preferred, QSizePolicy::Preferred);
                } else if (auto* btn = qobject_cast<Button*>(child)) {
                    btn->setSizePolicy(compact
                            ? QSizePolicy::Expanding
                            : (hbox.alignKey == QStringLiteral("Stretch")
                                      ? QSizePolicy::Preferred
                                      : QSizePolicy::Maximum),
                        btn->sizePolicy().verticalPolicy());
                }
                if (!compact && hbox.alignKey == QStringLiteral("Center"))
                    hbox.layout->addWidget(child, 0, Qt::AlignVCenter);
                else if (!compact && hbox.alignKey == QStringLiteral("Start"))
                    hbox.layout->addWidget(child, 0, Qt::AlignTop);
                else if (!compact && hbox.alignKey == QStringLiteral("End"))
                    hbox.layout->addWidget(child, 0, Qt::AlignBottom);
                else
                    hbox.layout->addWidget(child);
            }
            if (!compact && hbox.hasTrailingStretch)
                hbox.layout->addStretch(1);
        }
        hbox.layout->activate();
        hbox.host->updateGeometry();
    }

    // 3. Outer Cards & VBoxes in reverse creation order (innermost first -> root last)
    for (int i = m_responsive->boxes.size() - 1; i >= 0; --i) {
        const auto& box = m_responsive->boxes.at(i);
        if (!box.host || !box.layout)
            continue;
        const int pad = scaleDensity(box.configuredPadding, tier, 8, 0.75, 0.55);
        const int sp = scaleDensity(box.configuredSpacing, tier, 6, 0.85, 0.7);
        box.layout->setContentsMargins(pad, pad, pad, pad);
        box.layout->setSpacing(sp);
        box.layout->activate();
        box.host->updateGeometry();
    }

    if (m_stageLayout)
        m_stageLayout->activate();
    const int vpW = (m_scroll && m_scroll->viewport()->width() > 0) ? m_scroll->viewport()->width() : width;
    const int minStageH = m_scroll ? m_scroll->viewport()->height() : height();
    const int rootDesiredH = m_rootIsAnchorBox
        ? qMax(m_rootWidget->minimumHeight(), m_rootWidget->sizeHint().height())
        : qMax(m_rootWidget->height(), m_rootWidget->sizeHint().height());
    const int neededH = qMax(minStageH, rootDesiredH);
    m_stage->resize(vpW, neededH);
    if (m_stageLayout)
        m_stageLayout->activate();
}

void GalleryStudioCanvas::scheduleLayoutSnapshot()
{
    if (m_snapshotScheduled)
        return;
    m_snapshotScheduled = true;
    QTimer::singleShot(0, this, &GalleryStudioCanvas::emitLayoutSnapshot);
}

void GalleryStudioCanvas::emitLayoutSnapshot()
{
    m_snapshotScheduled = false;
    ++m_revision;
    emit layoutChanged(layoutSnapshot());
}

void GalleryStudioCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), ThemeRegistry::instance().colors(m_theme).bgCanvas);
}

void GalleryStudioCanvas::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    const int vpWidth = m_scroll && m_scroll->viewport()->width() > 0 ? m_scroll->viewport()->width() : width();
    applyResponsiveLayout(vpWidth);
    scheduleLayoutSnapshot();
}

} // namespace fluent::gallery

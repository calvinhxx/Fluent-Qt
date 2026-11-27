#include "view/widgets/GalleryStudioCanvas.h"
#include "components/foundation/UserTheme.h"
#include "view/widgets/GalleryComponentPlayground.h"
#include "view/widgets/samples/CollectionPhotoIllustrations.h"

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
#include <QBitmap>
#include <QBoxLayout>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QLayout>
#include <QLayoutItem>
#include <QLinearGradient>
#include <QLocale>
#include <QMargins>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QRadialGradient>
#include <QRegion>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QSvgRenderer>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QtGlobal>
#include <QtMath>
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

bool isAutomatedTestRun()
{
    return QCoreApplication::instance() && QCoreApplication::instance()->property("fluentqtGalleryAutomated").toBool();
}

const QHash<QString, QString>& studioIconLookupTable()
{
    static const QHash<QString, QString> table = {
        {QStringLiteral("globalnav"), Typography::Icons::GlobalNav},
        {QStringLiteral("chevrondown"), Typography::Icons::ChevronDown},
        {QStringLiteral("chevronup"), Typography::Icons::ChevronUp},
        {QStringLiteral("chevronleft"), Typography::Icons::ChevronLeft},
        {QStringLiteral("chevronright"), Typography::Icons::ChevronRight},
        {QStringLiteral("back"), Typography::Icons::Back},
        {QStringLiteral("forward"), Typography::Icons::Forward},
        {QStringLiteral("home"), Typography::Icons::Home},
        {QStringLiteral("menu"), Typography::Icons::Menu},
        {QStringLiteral("up"), Typography::Icons::Up},
        {QStringLiteral("down"), Typography::Icons::Down},
        {QStringLiteral("sort"), Typography::Icons::Up},
        {QStringLiteral("fullscreen"), Typography::Icons::FullScreen},
        {QStringLiteral("backtowindow"), Typography::Icons::BackToWindow},
        {QStringLiteral("more"), Typography::Icons::More},
        {QStringLiteral("morehorizontal"), Typography::Icons::More},
        {QStringLiteral("allapps"), Typography::Icons::AllApps},
        {QStringLiteral("layers"), Typography::Icons::AllApps},
        {QStringLiteral("add"), Typography::Icons::Add},
        {QStringLiteral("cancel"), Typography::Icons::Cancel},
        {QStringLiteral("delete"), Typography::Icons::Delete},
        {QStringLiteral("trash"), Typography::Icons::Delete},
        {QStringLiteral("save"), Typography::Icons::Save},
        {QStringLiteral("saveas"), Typography::Icons::SaveAs},
        {QStringLiteral("search"), Typography::Icons::Search},
        {QStringLiteral("view"), Typography::Icons::View},
        {QStringLiteral("hide"), Typography::Icons::Hide},
        {QStringLiteral("settings"), Typography::Icons::Settings},
        {QStringLiteral("edit"), Typography::Icons::Edit},
        {QStringLiteral("refresh"), Typography::Icons::Refresh},
        {QStringLiteral("share"), Typography::Icons::Share},
        {QStringLiteral("copy"), Typography::Icons::Copy},
        {QStringLiteral("cut"), Typography::Icons::Cut},
        {QStringLiteral("paste"), Typography::Icons::Paste},
        {QStringLiteral("filter"), Typography::Icons::Filter},
        {QStringLiteral("link"), Typography::Icons::Link},
        {QStringLiteral("favoritestar"), Typography::Icons::FavoriteStar},
        {QStringLiteral("favoritestarfill"), Typography::Icons::FavoriteStarFill},
        {QStringLiteral("sparkle"), Typography::Icons::FavoriteStar},
        {QStringLiteral("sparkles"), Typography::Icons::FavoriteStar},
        {QStringLiteral("sparkle"), Typography::Icons::FavoriteStar},
        {QStringLiteral("magic"), Typography::Icons::FavoriteStar},
        {QStringLiteral("wand"), Typography::Icons::FavoriteStar},
        {QStringLiteral("bot"), Typography::Icons::FavoriteStar},
        {QStringLiteral("robot"), Typography::Icons::FavoriteStar},
        {QStringLiteral("agent"), Typography::Icons::FavoriteStar},
        {QStringLiteral("assistant"), Typography::Icons::FavoriteStar},
        {QStringLiteral("brain"), Typography::Icons::FavoriteStar},
        {QStringLiteral("prism"), Typography::Icons::FavoriteStar},
        {QStringLiteral("cpu"), Typography::Icons::AllApps},
        {QStringLiteral("chip"), Typography::Icons::AllApps},
        {QStringLiteral("circuit"), Typography::Icons::AllApps},
        {QStringLiteral("ai"), Typography::Icons::FavoriteStar},
        {QStringLiteral("chat"), Typography::Icons::Message},
        {QStringLiteral("comment"), Typography::Icons::Message},
        {QStringLiteral("conversation"), Typography::Icons::Message},
        {QStringLiteral("workflow"), Typography::Icons::Sync},
        {QStringLiteral("zap"), Typography::Icons::Sync},
        {QStringLiteral("bolt"), Typography::Icons::Sync},
        {QStringLiteral("activity"), Typography::Icons::Sync},
        {QStringLiteral("pulse"), Typography::Icons::Sync},
        {QStringLiteral("attach"), Typography::Icons::Link},
        {QStringLiteral("paperclip"), Typography::Icons::Link},
        {QStringLiteral("plus"), Typography::Icons::Add},
        {QStringLiteral("inbox"), Typography::Icons::Mail},
        {QStringLiteral("archive"), Typography::Icons::Folder},
        {QStringLiteral("bookmark"), Typography::Icons::Pin},
        {QStringLiteral("tag"), Typography::Icons::Pin},
        {QStringLiteral("chart"), Typography::Icons::Grid},
        {QStringLiteral("barchart"), Typography::Icons::Grid},
        {QStringLiteral("linechart"), Typography::Icons::Grid},
        {QStringLiteral("piechart"), Typography::Icons::Grid},
        {QStringLiteral("metrics"), Typography::Icons::Grid},
        {QStringLiteral("database"), Typography::Icons::Storage},
        {QStringLiteral("server"), Typography::Icons::Storage},
        {QStringLiteral("bug"), Typography::Icons::Settings},
        {QStringLiteral("wrench"), Typography::Icons::Settings},
        {QStringLiteral("sliders"), Typography::Icons::Settings},
        {QStringLiteral("compass"), Typography::Icons::MapPin},
        {QStringLiteral("map"), Typography::Icons::MapPin},
        {QStringLiteral("location"), Typography::Icons::MapPin},
        {QStringLiteral("book"), Typography::Icons::Document},
        {QStringLiteral("notes"), Typography::Icons::Document},
        {QStringLiteral("pen"), Typography::Icons::Edit},
        {QStringLiteral("pencil"), Typography::Icons::Edit},
        {QStringLiteral("check"), Typography::Icons::CheckMark},
        {QStringLiteral("circlecheck"), Typography::Icons::CheckMark},
        {QStringLiteral("alert"), Typography::Icons::Warning},
        {QStringLiteral("alerttriangle"), Typography::Icons::Warning},
        {QStringLiteral("help"), Typography::Icons::Info},
        {QStringLiteral("question"), Typography::Icons::Info},
        {QStringLiteral("moon"), Typography::Icons::Brightness},
        {QStringLiteral("sun"), Typography::Icons::Brightness},
        {QStringLiteral("eye"), Typography::Icons::View},
        {QStringLiteral("eyeoff"), Typography::Icons::Hide},
        {QStringLiteral("key"), Typography::Icons::Lock},
        {QStringLiteral("external"), Typography::Icons::Share},
        {QStringLiteral("pin"), Typography::Icons::Pin},
        {QStringLiteral("pinfill"), Typography::Icons::PinFill},
        {QStringLiteral("unpin"), Typography::Icons::Unpin},
        {QStringLiteral("flag"), Typography::Icons::Flag},
        {QStringLiteral("block"), Typography::Icons::Block},
        {QStringLiteral("zoom"), Typography::Icons::Zoom},
        {QStringLiteral("zoomin"), Typography::Icons::ZoomIn},
        {QStringLiteral("zoomout"), Typography::Icons::ZoomOut},
        {QStringLiteral("undo"), Typography::Icons::Undo},
        {QStringLiteral("redo"), Typography::Icons::Redo},
        {QStringLiteral("selectall"), Typography::Icons::SelectAll},
        {QStringLiteral("play"), Typography::Icons::Play},
        {QStringLiteral("pause"), Typography::Icons::Pause},
        {QStringLiteral("stop"), Typography::Icons::Stop},
        {QStringLiteral("volume"), Typography::Icons::Volume},
        {QStringLiteral("mute"), Typography::Icons::Mute},
        {QStringLiteral("microphone"), Typography::Icons::Microphone},
        {QStringLiteral("video"), Typography::Icons::Video},
        {QStringLiteral("camera"), Typography::Icons::Camera},
        {QStringLiteral("photo"), Typography::Icons::Camera},
        {QStringLiteral("gallery"), Typography::Icons::Camera},
        {QStringLiteral("image"), Typography::Icons::Camera},
        {QStringLiteral("music"), Typography::Icons::Music},
        {QStringLiteral("album"), Typography::Icons::Music},
        {QStringLiteral("audio"), Typography::Icons::Music},
        {QStringLiteral("movie"), Typography::Icons::Movie},
        {QStringLiteral("headphones"), Typography::Icons::Headphones},
        {QStringLiteral("speaker"), Typography::Icons::Speaker},
        {QStringLiteral("skipback"), Typography::Icons::SkipBack},
        {QStringLiteral("skipforward"), Typography::Icons::SkipForward},
        {QStringLiteral("mail"), Typography::Icons::Mail},
        {QStringLiteral("people"), Typography::Icons::People},
        {QStringLiteral("user"), Typography::Icons::People},
        {QStringLiteral("phone"), Typography::Icons::Phone},
        {QStringLiteral("message"), Typography::Icons::Message},
        {QStringLiteral("send"), Typography::Icons::Send},
        {QStringLiteral("contact"), Typography::Icons::Contact},
        {QStringLiteral("group"), Typography::Icons::Group},
        {QStringLiteral("emoji"), Typography::Icons::Emoji},
        {QStringLiteral("world"), Typography::Icons::World},
        {QStringLiteral("globe"), Typography::Icons::World},
        {QStringLiteral("contactinfo"), Typography::Icons::ContactInfo},
        {QStringLiteral("accounts"), Typography::Icons::Accounts},
        {QStringLiteral("folder"), Typography::Icons::Folder},
        {QStringLiteral("file"), Typography::Icons::File},
        {QStringLiteral("document"), Typography::Icons::Document},
        {QStringLiteral("code"), Typography::Icons::Document},
        {QStringLiteral("terminal"), Typography::Icons::Document},
        {QStringLiteral("cloud"), Typography::Icons::Cloud},
        {QStringLiteral("download"), Typography::Icons::Download},
        {QStringLiteral("upload"), Typography::Icons::Upload},
        {QStringLiteral("rocket"), Typography::Icons::Upload},
        {QStringLiteral("sync"), Typography::Icons::Sync},
        {QStringLiteral("storage"), Typography::Icons::Storage},
        {QStringLiteral("calculator"), Typography::Icons::Calculator},
        {QStringLiteral("calendar"), Typography::Icons::Calendar},
        {QStringLiteral("clock"), Typography::Icons::Clock},
        {QStringLiteral("history"), Typography::Icons::History},
        {QStringLiteral("wifi"), Typography::Icons::Wifi},
        {QStringLiteral("bluetooth"), Typography::Icons::Bluetooth},
        {QStringLiteral("battery"), Typography::Icons::Battery},
        {QStringLiteral("print"), Typography::Icons::Print},
        {QStringLiteral("laptop"), Typography::Icons::Laptop},
        {QStringLiteral("mobile"), Typography::Icons::Mobile},
        {QStringLiteral("desktop"), Typography::Icons::Desktop},
        {QStringLiteral("appicondefault"), Typography::Icons::AppIconDefault},
        {QStringLiteral("mouse"), Typography::Icons::Mouse},
        {QStringLiteral("keyboard"), Typography::Icons::Keyboard},
        {QStringLiteral("controller"), Typography::Icons::Controller},
        {QStringLiteral("power"), Typography::Icons::Power},
        {QStringLiteral("brightness"), Typography::Icons::Brightness},
        {QStringLiteral("airplane"), Typography::Icons::Airplane},
        {QStringLiteral("warning"), Typography::Icons::Warning},
        {QStringLiteral("erroricon"), Typography::Icons::ErrorIcon},
        {QStringLiteral("info"), Typography::Icons::Info},
        {QStringLiteral("bell"), Typography::Icons::Info},
        {QStringLiteral("notification"), Typography::Icons::Info},
        {QStringLiteral("checkmark"), Typography::Icons::CheckMark},
        {QStringLiteral("success"), Typography::Icons::Success},
        {QStringLiteral("errorbadge12"), Typography::Icons::ErrorBadge12},
        {QStringLiteral("importantbadge12"), Typography::Icons::ImportantBadge12},
        {QStringLiteral("shield"), Typography::Icons::Shield},
        {QStringLiteral("security"), Typography::Icons::Shield},
        {QStringLiteral("lock"), Typography::Icons::Lock},
        {QStringLiteral("unlock"), Typography::Icons::Unlock},
        {QStringLiteral("heart"), Typography::Icons::Heart},
        {QStringLiteral("heartfill"), Typography::Icons::HeartFill},
        {QStringLiteral("star"), Typography::Icons::Star},
        {QStringLiteral("dismiss"), Typography::Icons::Dismiss},
        {QStringLiteral("close"), Typography::Icons::Dismiss},
        {QStringLiteral("clear"), Typography::Icons::Clear},
        {QStringLiteral("brush"), Typography::Icons::Brush},
        {QStringLiteral("design"), Typography::Icons::Brush},
        {QStringLiteral("color"), Typography::Icons::Color},
        {QStringLiteral("palette"), Typography::Icons::Color},
        {QStringLiteral("font"), Typography::Icons::Font},
        {QStringLiteral("grid"), Typography::Icons::Grid},
        {QStringLiteral("dashboard"), Typography::Icons::Grid},
        {QStringLiteral("analytics"), Typography::Icons::Grid},
        {QStringLiteral("list"), Typography::Icons::List},
        {QStringLiteral("alignleft"), Typography::Icons::AlignLeft},
        {QStringLiteral("aligncenter"), Typography::Icons::AlignCenter},
        {QStringLiteral("alignright"), Typography::Icons::AlignRight},
        {QStringLiteral("mappin"), Typography::Icons::MapPin},
        {QStringLiteral("sunny"), Typography::Icons::Sunny},
        {QStringLiteral("cloudweather"), Typography::Icons::CloudWeather},
        {QStringLiteral("rain"), Typography::Icons::Rain},
        {QStringLiteral("snow"), Typography::Icons::Snow},
    };
    return table;
}

QString inferStudioCommandGlyph(const QString& label, int fallbackIndex, bool navigation);

QString resolveIconGlyph(const QString& nameOrGlyph)
{
    const QString trimmed = nameOrGlyph.trimmed();
    if (!trimmed.isEmpty()) {
        const auto& table = studioIconLookupTable();
        const auto it = table.constFind(trimmed.toLower());
        if (it != table.constEnd() && !it.value().isEmpty())
            return it.value();
        const QString lookedUp = Typography::Icons::glyph(trimmed);
        if (!lookedUp.isEmpty())
            return lookedUp;
        if (trimmed.size() <= 2 && trimmed.at(0).unicode() >= 0x80)
            return trimmed;
        return inferStudioCommandGlyph(trimmed, 0, false);
    }
    return Typography::Icons::Settings;
}

int measureWidgetHeightForWidth(QWidget* widget, int availWidth, int maxCap = 1400)
{
    if (!widget)
        return 0;
    if (QLayout* lay = widget->layout())
        lay->activate();
    int h = qMax(widget->minimumSizeHint().height(), widget->sizeHint().height());
    if (availWidth > 0 && widget->hasHeightForWidth())
        h = qMax(h, widget->heightForWidth(availWidth));
    if (QLayout* lay = widget->layout()) {
        h = qMax(h, qMax(lay->totalMinimumSize().height(), lay->totalSizeHint().height()));
        if (availWidth > 0 && lay->hasHeightForWidth())
            h = qMax(h, lay->totalHeightForWidth(availWidth));
    }
    return qBound(0, h, maxCap);
}

// Accept the same static artwork vocabulary as site/studio/studio-media.mjs.
bool isStudioSvg(const QByteArray& svg)
{
    if (svg.size() > 16384 || !svg.startsWith("<svg"))
        return false;
    static const auto elements =
        QStringLiteral(
            "svg g defs path rect circle ellipse line polyline polygon "
            "linearGradient radialGradient stop clipPath text tspan title desc")
            .split(QLatin1Char(' '));
    static const auto attributes = QStringLiteral(
        "viewBox width height x y x1 y1 x2 y2 cx cy r rx ry fx fy d points "
        "id transform fill stroke color fill-rule clip-rule clip-path fill-opacity stroke-opacity opacity stroke-width "
        "stroke-linecap stroke-linejoin stroke-dasharray stroke-dashoffset gradientUnits gradientTransform "
        "offset stop-color stop-opacity preserveAspectRatio text-anchor font-size font-weight font-family")
                                       .split(QLatin1Char(' '));
    static const auto paints = QStringLiteral("fill stroke color stop-color clip-path").split(QLatin1Char(' '));
    static const QRegularExpression paint(
        QStringLiteral(R"(^(?:[A-Za-z]+|#[0-9a-fA-F]{3,8}|(?:rgb|hsl)a?\([\d.,% +\-]+\)|url\(#[\w.-]+\))$)"));
    static const QRegularExpression geometry(QStringLiteral(R"(^[\w.,%# +()\s-]*$)"));

    QXmlStreamReader xml(svg);
    bool hasRoot = false;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const auto name = xml.qualifiedName().toString();
            if ((!hasRoot && name != QStringLiteral("svg")) || !elements.contains(name))
                return false;
            hasRoot = true;
            for (const auto& ns : xml.namespaceDeclarations()) {
                if (!ns.prefix().isEmpty()
                    || ns.namespaceUri().toString() != QStringLiteral("http://www.w3.org/2000/svg"))
                    return false;
            }
            for (const auto& attr : xml.attributes()) {
                const auto key = attr.qualifiedName().toString();
                const auto& pattern = paints.contains(key) ? paint : geometry;
                if (!attributes.contains(key) || !pattern.match(attr.value().toString()).hasMatch())
                    return false;
            }
        } else if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::ProcessingInstruction
            || token == QXmlStreamReader::EntityReference || token == QXmlStreamReader::Comment || xml.isCDATA()) {
            return false;
        }
    }
    return hasRoot && !xml.hasError();
}

QPixmap renderSanitizedSvgPixmap(const QString& svgText, QSize logicalSize, qreal dpr)
{
    const QByteArray svg = svgText.trimmed().toUtf8();
    if (!isStudioSvg(svg))
        return {};
    QSvgRenderer renderer(svg);
    if (!renderer.isValid())
        return {};

    const qreal safeDpr = qMax<qreal>(1.0, dpr);
    const QSize targetLogical(qMax(32, logicalSize.width()), qMax(32, logicalSize.height()));
    QPixmap pixmap(qCeil(targetLogical.width() * safeDpr), qCeil(targetLogical.height() * safeDpr));
    pixmap.setDevicePixelRatio(safeDpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QSizeF svgSize = renderer.defaultSize();
    if (!svgSize.isValid() || svgSize.isEmpty())
        svgSize = renderer.viewBoxF().size();
    if (!svgSize.isValid() || svgSize.isEmpty())
        svgSize = QSizeF(targetLogical);
    const qreal scale = qMin(targetLogical.width() / svgSize.width(), targetLogical.height() / svgSize.height());
    const QSizeF drawnSize(svgSize.width() * scale, svgSize.height() * scale);
    const QRectF bounds((targetLogical.width() - drawnSize.width()) * 0.5,
        (targetLogical.height() - drawnSize.height()) * 0.5, drawnSize.width(), drawnSize.height());
    renderer.render(&painter, bounds);
    return pixmap;
}

QColor studioThemeTint(const QString& mediaTheme, int index)
{
    static const QColor kFallbackTints[] = {
        QColor(QStringLiteral("#3E6DA8")),
        QColor(QStringLiteral("#2B6F73")),
        QColor(QStringLiteral("#8764B8")),
        QColor(QStringLiteral("#D84A38")),
        QColor(QStringLiteral("#2E7D5E")),
        QColor(QStringLiteral("#4A6572")),
    };
    const QString key = mediaTheme.trimmed().toLower();
    if (key == QLatin1String("coffee"))
        return QColor(QStringLiteral("#9C633B")).darker(100 + (index % 3) * 12);
    if (key == QLatin1String("architecture"))
        return QColor(QStringLiteral("#4D647D")).lighter(100 + (index % 3) * 10);
    if (key == QLatin1String("city"))
        return QColor(QStringLiteral("#355070")).lighter(100 + (index % 3) * 12);
    if (key == QLatin1String("portrait"))
        return QColor(QStringLiteral("#B5655C")).lighter(100 + (index % 3) * 10);
    if (key == QLatin1String("technology"))
        return QColor(QStringLiteral("#008C99")).darker(100 + (index % 3) * 12);
    if (key == QLatin1String("abstract"))
        return QColor(QStringLiteral("#7A5195")).lighter(100 + (index % 3) * 12);
    if (key == QLatin1String("music"))
        return QColor(QStringLiteral("#8764B8")).lighter(100 + (index % 3) * 10);
    if (key == QLatin1String("landscape"))
        return kFallbackTints[(index % 4 == 0 ? 3 : index) % 6];
    return kFallbackTints[qAbs(index) % 6];
}

QUrl studioCuratedPhotoUrl(const QString& mediaTheme, int index, int width)
{
    static const QStringList kLandscapeIds = {
        QStringLiteral("photo-1500530855697-b586d89ba3ee"),
        QStringLiteral("photo-1470770841072-f978cf4d019e"),
        QStringLiteral("photo-1441974231531-c6227db76b6e"),
        QStringLiteral("photo-1470252649378-9c29740c9fa8"),
        QStringLiteral("photo-1506744038136-46273834b3fb"),
        QStringLiteral("photo-1472214103451-9374bd1c798e"),
        QStringLiteral("photo-1469474968028-56623f02e42e"),
    };
    static const QStringList kArchitectureIds = {
        QStringLiteral("photo-1511818966892-d7d671e672a2"),
        QStringLiteral("photo-1497366754035-f200968a6e72"),
        QStringLiteral("photo-1486406146926-c627a92ad1ab"),
        QStringLiteral("photo-1513694203232-719a280e022f"),
    };
    static const QStringList kCoffeeIds = {
        QStringLiteral("photo-1509042239860-f550ce710b93"),
        QStringLiteral("photo-1495474472287-4d71bcdd2085"),
        QStringLiteral("photo-1511920170033-f8396924c348"),
    };
    static const QStringList kCityIds = {
        QStringLiteral("photo-1477959858617-67f85cf4f1df"),
        QStringLiteral("photo-1480714378408-67cf0d13bc1b"),
        QStringLiteral("photo-1519501025264-65ba15a82390"),
    };
    static const QStringList kPortraitIds = {
        QStringLiteral("photo-1534528741775-53994a69daeb"),
        QStringLiteral("photo-1507003211169-0a1dd7228f2d"),
        QStringLiteral("photo-1517841905240-472988babdf9"),
    };
    static const QStringList kTechIds = {
        QStringLiteral("photo-1518770660439-4636190af475"),
        QStringLiteral("photo-1550751827-4bd374c3f58b"),
        QStringLiteral("photo-1526374965328-7f61d4dc18c5"),
    };
    static const QStringList kMusicIds = {
        QStringLiteral("photo-1511671782779-c97d3d27a1d4"),
        QStringLiteral("photo-1470225620780-dba8ba36b745"),
        QStringLiteral("photo-1514525253161-7a46d19cd819"),
    };
    static const QStringList kAbstractIds = {
        QStringLiteral("photo-1557672172-298e090bd0f1"),
        QStringLiteral("photo-1579546929518-9e396f3cc809"),
        QStringLiteral("photo-1558591710-4b4a1ae0f04d"),
    };

    const QString key = mediaTheme.trimmed().toLower();
    const QStringList* list = &kLandscapeIds;
    if (key == QLatin1String("architecture"))
        list = &kArchitectureIds;
    else if (key == QLatin1String("coffee"))
        list = &kCoffeeIds;
    else if (key == QLatin1String("city"))
        list = &kCityIds;
    else if (key == QLatin1String("portrait"))
        list = &kPortraitIds;
    else if (key == QLatin1String("technology"))
        list = &kTechIds;
    else if (key == QLatin1String("music"))
        list = &kMusicIds;
    else if (key == QLatin1String("abstract"))
        list = &kAbstractIds;

    const QString& photoId = list->at(qAbs(index) % list->size());
    return QUrl(QStringLiteral("https://images.unsplash.com/%1?auto=format&fit=crop&w=%2&q=80")
            .arg(photoId)
            .arg(qBound(160, width, 1280)));
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

QStandardItemModel* buildTreeModel(QObject* parent, const QStringList& items)
{
    auto* model = new QStandardItemModel(parent);
    QStandardItem* currentParent = nullptr;
    for (const QString& rawItem : items) {
        const bool isIndented = !rawItem.isEmpty()
            && (rawItem.startsWith(QLatin1Char(' ')) || rawItem.startsWith(QLatin1Char('\t')));
        const QString cleanLabel = rawItem.trimmed();
        if (cleanLabel.isEmpty())
            continue;
        auto* entry = new QStandardItem(cleanLabel);
        if (isIndented && currentParent) {
            currentParent->appendRow(entry);
        } else {
            model->appendRow(entry);
            currentParent = entry;
        }
    }
    return model;
}

QString inferStudioCommandGlyph(const QString& label, int fallbackIndex = 0, bool navigation = false)
{
    const QString low = label.toLower();
    if (low.contains(QStringLiteral("new")) || low.contains(QStringLiteral("add"))
        || low.contains(QStringLiteral("create")) || low.contains(QStringLiteral("import"))
        || low.contains(QStringLiteral("upload")))
        return Typography::Icons::Add;
    if (low.contains(QStringLiteral("save")) || low.contains(QStringLiteral("apply")))
        return Typography::Icons::Save;
    if (low.contains(QStringLiteral("search")) || low.contains(QStringLiteral("find"))
        || low.contains(QStringLiteral("filter")))
        return Typography::Icons::Search;
    if (low.contains(QStringLiteral("delete")) || low.contains(QStringLiteral("remove"))
        || low.contains(QStringLiteral("clear")))
        return Typography::Icons::Delete;
    if (low.contains(QStringLiteral("share")) || low.contains(QStringLiteral("export"))
        || low.contains(QStringLiteral("publish")))
        return Typography::Icons::Share;
    if (low.contains(QStringLiteral("setting")) || low.contains(QStringLiteral("config"))
        || low.contains(QStringLiteral("prefer")))
        return Typography::Icons::Settings;
    if (low.contains(QStringLiteral("favorite")) || low.contains(QStringLiteral("star")))
        return Typography::Icons::FavoriteStar;
    if (low.contains(QStringLiteral("edit")) || low.contains(QStringLiteral("rename"))
        || low.contains(QStringLiteral("note")))
        return Typography::Icons::Edit;
    if (low.contains(QStringLiteral("refresh")) || low.contains(QStringLiteral("sync"))
        || low.contains(QStringLiteral("reload")))
        return Typography::Icons::Sync;
    if (low.contains(QStringLiteral("sort")))
        return Typography::Icons::Up;
    if (low.contains(QStringLiteral("overview")) || low.contains(QStringLiteral("home"))
        || low.contains(QStringLiteral("all")) || low.contains(QStringLiteral("gallery")))
        return Typography::Icons::Home;
    if (low.contains(QStringLiteral("info")) || low.contains(QStringLiteral("detail"))
        || low.contains(QStringLiteral("help")) || low.contains(QStringLiteral("about")))
        return Typography::Icons::Info;
    if (low.contains(QStringLiteral("alert")) || low.contains(QStringLiteral("warn"))
        || low.contains(QStringLiteral("error")))
        return Typography::Icons::Warning;

    if (navigation) {
        if (fallbackIndex == 0)
            return Typography::Icons::Home;
        static const QStringList kNavGlyphs = {
            Typography::Icons::Document,
            Typography::Icons::Info,
            Typography::Icons::Settings,
            Typography::Icons::Sync,
            Typography::Icons::Search,
            Typography::Icons::FavoriteStar,
        };
        return kNavGlyphs.at((qMax(0, fallbackIndex - 1)) % kNavGlyphs.size());
    }
    static const QStringList kCmdGlyphs = {
        Typography::Icons::Add,
        Typography::Icons::FavoriteStar,
        Typography::Icons::Up,
        Typography::Icons::Share,
        Typography::Icons::Edit,
        Typography::Icons::Search,
        Typography::Icons::Settings,
    };
    return kCmdGlyphs.at(qMax(0, fallbackIndex) % kCmdGlyphs.size());
}

struct StudioItemSpec {
    QString glyph;
    QString label;
};

StudioItemSpec parseStudioItemIconAndLabel(const QString& raw, int fallbackIndex, bool navigation)
{
    const int sep = raw.indexOf(QLatin1Char('|'));
    if (sep > 0) {
        const QString iconPart = raw.left(sep).trimmed();
        static const QRegularExpression kIconIdentRe(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
        if (kIconIdentRe.match(iconPart).hasMatch()) {
            const QString labelPart = raw.mid(sep + 1).trimmed();
            return {resolveIconGlyph(iconPart), labelPart.isEmpty() ? iconPart : labelPart};
        }
    }
    return {inferStudioCommandGlyph(raw, fallbackIndex, navigation), raw.trimmed()};
}

void applyStudioExtendedIconProps(QWidget* widget, const QJsonObject& props)
{
    if (!widget || props.isEmpty())
        return;
    if (auto* btn = qobject_cast<Button*>(widget)) {
        if (props.contains(QStringLiteral("icon")) || props.contains(QStringLiteral("iconGlyph"))) {
            const QString iconKey = props.contains(QStringLiteral("icon"))
                ? props.value(QStringLiteral("icon")).toString().trimmed()
                : props.value(QStringLiteral("iconGlyph")).toString().trimmed();
            if (!iconKey.isEmpty() && iconKey != QLatin1String("None")) {
                const QString resolved = resolveIconGlyph(iconKey);
                btn->setIconGlyph(resolved, 16);
                btn->setProperty("_studioIconGlyph", resolved);
                if (!props.contains(QStringLiteral("fluentLayout")) && btn->fluentLayout() == Button::TextOnly)
                    btn->setFluentLayout(Button::IconBefore);
            }
        }
    } else if (auto* fi = qobject_cast<FontIcon*>(widget)) {
        if (props.contains(QStringLiteral("glyph"))) {
            const QString glyphName = props.value(QStringLiteral("glyph")).toString().trimmed();
            if (!glyphName.isEmpty() && glyphName != QLatin1String("None"))
                fi->setGlyph(resolveIconGlyph(glyphName));
        }
    }
}

QPixmap glyphPixmapForColor(const QString& glyph, int logicalSize, const QColor& color, qreal dpr)
{
    const qreal safeDpr = qMax<qreal>(1.0, dpr);
    const int physicalSize = qMax(1, qCeil(logicalSize * safeDpr));
    QPixmap pixmap(physicalSize, physicalSize);
    pixmap.setDevicePixelRatio(safeDpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(color);
    Typography::Icons::paintGlyph(painter, QRectF(0, 0, logicalSize, logicalSize), glyph, logicalSize, Qt::AlignCenter);
    painter.end();

    // Font icons have font-line bearings; center their visible ink in the QAction slot.
    const QRect ink = QRegion(pixmap.mask()).boundingRect();
    if (!ink.isEmpty()) {
        QPixmap centered(pixmap.size());
        centered.setDevicePixelRatio(safeDpr);
        centered.fill(Qt::transparent);
        QPainter centeredPainter(&centered);
        const QPointF offset((physicalSize - ink.width()) / 2.0 - ink.x(),
            (physicalSize - ink.height()) / 2.0 - ink.y());
        centeredPainter.drawPixmap(offset / safeDpr, pixmap);
        return centered;
    }
    return pixmap;
}

QIcon makeThemedGlyphIcon(const QString& glyph, const FluentElement::Colors& colors, qreal dpr)
{
    QIcon icon;
    for (const int size : {16, 20}) {
        const QPixmap normal = glyphPixmapForColor(glyph, size, colors.textPrimary, dpr);
        const QPixmap disabled = glyphPixmapForColor(glyph, size, colors.textDisabled, dpr);
        icon.addPixmap(normal, QIcon::Normal, QIcon::Off);
        icon.addPixmap(normal, QIcon::Active, QIcon::Off);
        icon.addPixmap(disabled, QIcon::Disabled, QIcon::Off);
    }
    return icon;
}

void refreshCommandBarActionIcons(QWidget* root, FluentElement::Theme theme)
{
    if (!root)
        return;
    const auto& colors = ThemeRegistry::instance().colors(theme);
    const qreal dpr = qMax<qreal>(1.0, root->devicePixelRatioF());
    for (auto* bar : root->findChildren<CommandBar*>()) {
        for (QAction* act : bar->primaryActions()) {
            if (!act)
                continue;
            QString glyph = act->property("_studioGlyph").toString();
            if (glyph.isEmpty())
                glyph = inferStudioCommandGlyph(act->text());
            act->setIcon(makeThemedGlyphIcon(glyph, colors, dpr));
        }
    }
}

bool isSidebarLikeNode(const QJsonObject& obj, int depth = 0)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("ListView") || type == QStringLiteral("TreeView")
        || type == QStringLiteral("FileListView") || type == QStringLiteral("NavigationView"))
        return true;
    if (depth >= 2)
        return false;
    const QJsonArray children = obj.value(QStringLiteral("children")).toArray();
    for (const QJsonValue& val : children) {
        if (isSidebarLikeNode(val.toObject(), depth + 1))
            return true;
    }
    return false;
}

bool isPrimaryWideNode(const QJsonObject& obj, int depth = 0)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("GridView") || type == QStringLiteral("DataGrid") || type == QStringLiteral("FlowView")
        || type == QStringLiteral("TextEdit") || type == QStringLiteral("CodeBlock")
        || type == QStringLiteral("AreaChart") || type == QStringLiteral("LineChart")
        || type == QStringLiteral("BarChart") || type == QStringLiteral("ChartView")
        || type == QStringLiteral("Timeline"))
        return true;
    if (depth >= 2)
        return false;
    const QJsonArray children = obj.value(QStringLiteral("children")).toArray();
    for (const QJsonValue& val : children) {
        if (isPrimaryWideNode(val.toObject(), depth + 1))
            return true;
    }
    return false;
}

bool isHeaderLeadingControl(const QString& type)
{
    return type == QStringLiteral("FontIcon") || type == QStringLiteral("Label") || type == QStringLiteral("InfoBadge");
}

bool isHeaderTrailingControl(const QString& type)
{
    return type == QStringLiteral("AutoSuggestBox") || type == QStringLiteral("LineEdit")
        || type == QStringLiteral("ComboBox") || type == QStringLiteral("MultiSelectComboBox")
        || type == QStringLiteral("Button") || type == QStringLiteral("DropDownButton")
        || type == QStringLiteral("SplitButton") || type == QStringLiteral("ToggleButton")
        || type == QStringLiteral("ToggleSwitch") || type == QStringLiteral("SelectorBar")
        || type == QStringLiteral("Pivot") || type == QStringLiteral("Avatar") || type == QStringLiteral("DatePicker")
        || type == QStringLiteral("CalendarDatePicker");
}

// Lightweight WinUI 3 navigation rail item with transparent rest state,
// subtle translucent active pill, and crisp 3px left accent indicator bar.
class StudioNavRailItem final : public QAbstractButton {
public:
    StudioNavRailItem(fluent::FluentElement* themeHost,
        const QString& label,
        const QString& glyph,
        QWidget* parent = nullptr)
        : QAbstractButton(parent), m_themeHost(themeHost), m_glyph(glyph)
    {
        setText(label);
        setCheckable(true);
        setFixedHeight(36);
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }

    QSize sizeHint() const override
    {
        QFont f = font();
        f.setPixelSize(13);
        const int w = QFontMetrics(f).horizontalAdvance(text()) + 52;
        return QSize(qBound(120, w, 240), 36);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        const auto& colors =
            m_themeHost ? m_themeHost->themeColorsRef() : ThemeRegistry::instance().colors(FluentElement::Light);
        const QRectF bounds(rect());
        const bool active = isChecked();
        const bool hovered = !active && underMouse() && isEnabled();

        if (active || hovered) {
            const QRectF pillRect = bounds.adjusted(2.0, 1.0, -2.0, -1.0);
            painter.setPen(Qt::NoPen);
            painter.setBrush(active ? colors.subtleSecondary : colors.subtleTertiary);
            painter.drawRoundedRect(pillRect, CornerRadius::Control, CornerRadius::Control);
        }

        if (active) {
            const QRectF indicator(4.0, (bounds.height() - 16.0) * 0.5, 3.0, 16.0);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.accentDefault);
            painter.drawRoundedRect(indicator, 1.5, 1.5);
        }

        const QColor iconColor =
            !isEnabled() ? colors.textDisabled : (active ? colors.textAccentPrimary : colors.textSecondary);
        painter.setPen(iconColor);
        const QRectF iconRect(15.0, (bounds.height() - 16.0) * 0.5, 16.0, 16.0);
        Typography::Icons::paintGlyph(painter, iconRect, m_glyph, 16, Qt::AlignCenter);

        QFont labelFont = font();
        labelFont.setPixelSize(13);
        labelFont.setWeight(active ? QFont::DemiBold : QFont::Normal);
        painter.setFont(labelFont);
        painter.setPen(!isEnabled() ? colors.textDisabled : colors.textPrimary);
        const QRectF textRect(40.0, 0.0, qMax(12.0, bounds.width() - 48.0), bounds.height());
        const QString elided = QFontMetrics(labelFont).elidedText(text(), Qt::ElideRight, qRound(textRect.width()));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
    }

private:
    fluent::FluentElement* m_themeHost = nullptr;
    QString m_glyph;
};

// Fluent visual card delegate for Studio GridView items: renders cover-cropped media
// above an optional Title | Subtitle two-line footer.
class StudioGridTileDelegate final : public QStyledItemDelegate {
public:
    StudioGridTileDelegate(fluent::FluentElement* themeHost, GridView* view, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_themeHost(themeHost), m_view(view)
    {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!index.isValid())
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        const auto& colors =
            m_themeHost ? m_themeHost->themeColorsRef() : ThemeRegistry::instance().colors(FluentElement::Light);
        const bool isSelected = (option.state & QStyle::State_Selected);
        const bool isHovered = (option.state & QStyle::State_MouseOver);

        const QRectF tileRect = QRectF(option.rect).adjusted(2.0, 2.0, -2.0, -2.0);
        const qreal radius = CornerRadius::Control;

        QPainterPath tileClip;
        tileClip.addRoundedRect(tileRect, radius, radius);
        QColor cardBg = isSelected ? colors.bgLayer : colors.bgLayerAlt;
        painter->fillPath(tileClip, cardBg);

        painter->save();
        painter->setClipPath(tileClip);

        const QString rawLabel = index.data(Qt::DisplayRole).toString();
        const int sep = rawLabel.indexOf(QLatin1Char('|'));
        const QString title = (sep >= 0) ? rawLabel.left(sep).trimmed() : rawLabel;
        const QString subtitle = (sep >= 0) ? rawLabel.mid(sep + 1).trimmed() : QString();

        const qreal footerH = subtitle.isEmpty()
            ? qBound(24.0, tileRect.height() * 0.28, 30.0)
            : qBound(34.0, tileRect.height() * 0.34, 42.0);
        const QRectF thumbRect(tileRect.left() + 4.0, tileRect.top() + 4.0, qMax(24.0, tileRect.width() - 8.0),
            qMax(24.0, tileRect.height() - footerH - 6.0));
        const QRectF textRect(tileRect.left() + 8.0, tileRect.bottom() - footerH + 2.0,
            qMax(20.0, tileRect.width() - 16.0), footerH - 4.0);

        QPainterPath thumbClip;
        thumbClip.addRoundedRect(thumbRect, 4.0, 4.0);
        painter->save();
        painter->setClipPath(thumbClip);

        const QPixmap photo = index.data(Qt::DecorationRole).value<QPixmap>();
        if (!photo.isNull() && thumbRect.width() > 2.0 && thumbRect.height() > 2.0) {
            const QSizeF srcLogical = QSizeF(photo.size()) / qMax<qreal>(1.0, photo.devicePixelRatio());
            if (!srcLogical.isEmpty()) {
                const qreal coverScale = qMax(thumbRect.width() / srcLogical.width(),
                    thumbRect.height() / srcLogical.height());
                const qreal drawW = srcLogical.width() * coverScale;
                const qreal drawH = srcLogical.height() * coverScale;
                const QRectF targetRect(thumbRect.center().x() - drawW * 0.5, thumbRect.center().y() - drawH * 0.5,
                    drawW, drawH);
                painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
                painter->drawPixmap(targetRect, photo, QRectF(QPointF(0.0, 0.0), QSizeF(photo.size())));
            }
        } else {
            struct PalettePair {
                QRgb top;
                QRgb bottom;
                QRgb hillBack;
                QRgb hillFront;
                QRgb sun;
            };
            static constexpr PalettePair kPalettes[] = {
                {0xFF3E6DA8, 0xFF8FB8DE, 0xFF284F82, 0xFF1A365D, 0xFFFFD584},
                {0xFF2B6F73, 0xFF7BC8BF, 0xFF1D5258, 0xFF133B40, 0xFFFFE7A3},
                {0xFF5D5499, 0xFFAB9CE0, 0xFF423A78, 0xFF2C2654, 0xFFFFD8B0},
                {0xFF397654, 0xFF8CC99E, 0xFF27593D, 0xFF193D29, 0xFFFFF0A8},
                {0xFFA45A4A, 0xFFE3A682, 0xFF7D3F35, 0xFF542822, 0xFFFFE5B8},
                {0xFF2E6395, 0xFF6AAEE6, 0xFF1E4973, 0xFF12304D, 0xFFE6F4FF},
            };
            const PalettePair& pal = kPalettes[qAbs(index.row()) % 6];

            QLinearGradient sky(thumbRect.topLeft(), thumbRect.bottomLeft());
            sky.setColorAt(0.0, QColor::fromRgba(pal.top));
            sky.setColorAt(1.0, QColor::fromRgba(pal.bottom));
            painter->fillRect(thumbRect, sky);

            const qreal sunR = qMin(thumbRect.width(), thumbRect.height()) * 0.19;
            const QPointF sunCenter(thumbRect.right() - thumbRect.width() * 0.24,
                thumbRect.top() + thumbRect.height() * 0.30);
            QColor sunColor = QColor::fromRgba(pal.sun);
            sunColor.setAlpha(215);
            painter->setPen(Qt::NoPen);
            painter->setBrush(sunColor);
            painter->drawEllipse(sunCenter, sunR, sunR);

            QPainterPath ridgeBack;
            ridgeBack.moveTo(thumbRect.left(), thumbRect.bottom());
            ridgeBack.lineTo(thumbRect.left(), thumbRect.top() + thumbRect.height() * 0.62);
            ridgeBack.cubicTo(thumbRect.left() + thumbRect.width() * 0.28, thumbRect.top() + thumbRect.height() * 0.44,
                thumbRect.left() + thumbRect.width() * 0.62, thumbRect.top() + thumbRect.height() * 0.72,
                thumbRect.right(), thumbRect.top() + thumbRect.height() * 0.56);
            ridgeBack.lineTo(thumbRect.right(), thumbRect.bottom());
            ridgeBack.closeSubpath();
            painter->setBrush(QColor::fromRgba(pal.hillBack));
            painter->drawPath(ridgeBack);

            QPainterPath ridgeFront;
            ridgeFront.moveTo(thumbRect.left(), thumbRect.bottom());
            ridgeFront.lineTo(thumbRect.left(), thumbRect.top() + thumbRect.height() * 0.76);
            ridgeFront.cubicTo(thumbRect.left() + thumbRect.width() * 0.36, thumbRect.top() + thumbRect.height() * 0.64,
                thumbRect.left() + thumbRect.width() * 0.68, thumbRect.top() + thumbRect.height() * 0.86,
                thumbRect.right(), thumbRect.top() + thumbRect.height() * 0.70);
            ridgeFront.lineTo(thumbRect.right(), thumbRect.bottom());
            ridgeFront.closeSubpath();
            painter->setBrush(QColor::fromRgba(pal.hillFront));
            painter->drawPath(ridgeFront);
        }

        painter->restore();

        if (isHovered && !isSelected)
            painter->fillPath(thumbClip, QColor(255, 255, 255, 18));

        QFont titleFont = option.font;
        titleFont.setPixelSize(12);
        titleFont.setWeight(QFont::DemiBold);
        painter->setFont(titleFont);
        painter->setPen(isSelected ? colors.textAccentPrimary : colors.textPrimary);

        if (subtitle.isEmpty()) {
            const QString elided = QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, qRound(textRect.width()));
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
        } else {
            const QRectF topLine(textRect.left(), textRect.top(), textRect.width(), textRect.height() * 0.54);
            const QRectF subLine(textRect.left(), textRect.top() + textRect.height() * 0.52, textRect.width(),
                textRect.height() * 0.48);
            const QString elidedTitle = QFontMetrics(titleFont).elidedText(title, Qt::ElideRight,
                qRound(topLine.width()));
            painter->drawText(topLine, Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

            QFont subFont = option.font;
            subFont.setPixelSize(11);
            painter->setFont(subFont);
            painter->setPen(colors.textSecondary);
            const QString elidedSub = QFontMetrics(subFont).elidedText(subtitle, Qt::ElideRight,
                qRound(subLine.width()));
            painter->drawText(subLine, Qt::AlignLeft | Qt::AlignVCenter, elidedSub);
        }

        painter->restore();

        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(isSelected ? colors.accentDefault : colors.strokeCard, isSelected ? 2.0 : 1.0));
        painter->drawPath(tileClip);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        if (m_view)
            return m_view->cellSize();
        return QSize(156, 128);
    }

private:
    fluent::FluentElement* m_themeHost = nullptr;
    GridView* m_view = nullptr;
};

// Wraps DataGrid's native delegate to render semantic status pills for recognized tokens.
class StudioDataGridDelegate final : public QStyledItemDelegate {
public:
    explicit StudioDataGridDelegate(DataGrid* grid)
        : QStyledItemDelegate(grid), m_grid(grid), m_baseDelegate(grid ? grid->itemDelegate() : nullptr)
    {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return m_baseDelegate
            ? m_baseDelegate->createEditor(parent, option, index)
            : QStyledItemDelegate::createEditor(parent, option, index);
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index)
        const override
    {
        if (m_baseDelegate)
            m_baseDelegate->updateEditorGeometry(editor, option, index);
        else
            QStyledItemDelegate::updateEditorGeometry(editor, option, index);
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (m_baseDelegate)
            m_baseDelegate->setEditorData(editor, index);
        else
            QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        if (m_baseDelegate)
            m_baseDelegate->setModelData(editor, model, index);
        else
            QStyledItemDelegate::setModelData(editor, model, index);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!m_grid || !index.isValid()) {
            if (m_baseDelegate)
                m_baseDelegate->paint(painter, option, index);
            return;
        }

        const QString text = index.data(Qt::DisplayRole).toString().trimmed();
        const QString low = text.toLower();
        const auto& colors = m_grid->themeColorsRef();
        QColor pillFg;
        QColor pillBg;
        bool isStatus = false;

        auto matchAny = [&](std::initializer_list<const char*> tokens) {
            for (const char* tok : tokens) {
                if (low == QLatin1String(tok))
                    return true;
            }
            return false;
        };

        if (matchAny({"online", "active", "healthy", "ready", "verified", "running", "completed", "pass", "passed",
                "ok", "synced", "enabled", "live", "up"})) {
            pillFg = colors.systemSuccess;
            pillBg = colors.systemSuccessBg;
            isStatus = true;
        } else if (matchAny({"warning", "warn", "degraded", "pending", "queued", "syncing", "indexing", "slow", "busy",
                       "review"})) {
            pillFg = colors.systemCaution;
            pillBg = colors.systemCautionBg;
            isStatus = true;
        } else if (matchAny({"error", "failed", "offline", "critical", "down", "blocked", "timeout", "alert"})) {
            pillFg = colors.systemCritical;
            pillBg = colors.systemCriticalBg;
            isStatus = true;
        } else if (matchAny({"new", "beta", "info", "draft", "scheduled"})) {
            pillFg = colors.textAccentPrimary;
            pillBg = colors.subtleSecondary;
            isStatus = true;
        }

        if (!isStatus || text.isEmpty() || text.size() > 24) {
            if (m_baseDelegate)
                m_baseDelegate->paint(painter, option, index);
            else
                QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        QColor rowBg = m_grid->isBackgroundVisible() ? colors.bgLayer : Qt::transparent;
        painter->fillRect(option.rect, rowBg);
        if (selected)
            painter->fillRect(option.rect, colors.subtleSecondary);
        else if (hovered)
            painter->fillRect(option.rect, colors.subtleTertiary);

        QFont pillFont = option.font;
        pillFont.setPixelSize(11);
        pillFont.setWeight(QFont::DemiBold);
        const QFontMetrics fm(pillFont);
        const int maxPillW = qMax(36, option.rect.width() - 18);
        const int pillW = qMin(maxPillW, fm.horizontalAdvance(text) + 26);
        const QRectF pillRect(option.rect.left() + 10.0, option.rect.center().y() - 10.5, pillW, 21.0);

        QColor tintFill = pillBg.isValid() && pillBg.alpha() > 0 ? pillBg : pillFg;
        if (tintFill == pillFg)
            tintFill.setAlpha(34);
        QColor borderColor = pillFg;
        borderColor.setAlpha(90);

        painter->setBrush(tintFill);
        painter->setPen(QPen(borderColor, 1.0));
        painter->drawRoundedRect(pillRect, 10.5, 10.5);

        painter->setPen(Qt::NoPen);
        painter->setBrush(pillFg);
        painter->drawEllipse(QPointF(pillRect.left() + 9.0, pillRect.center().y()), 2.5, 2.5);

        painter->setFont(pillFont);
        painter->setPen(pillFg);
        const QRectF textRect(pillRect.left() + 15.0, pillRect.top(), qMax(10.0, pillRect.width() - 21.0),
            pillRect.height());
        const QString elided = fm.elidedText(text, Qt::ElideRight, qRound(textRect.width()));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);

        painter->restore();
    }

private:
    DataGrid* m_grid = nullptr;
    QAbstractItemDelegate* m_baseDelegate = nullptr;
};

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
    if (type == QStringLiteral("SplashScreen")) {
        if (auto* splash = widget->findChild<fluent::status_info::SplashScreen*>())
            return splash;
    }
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
        int splitStretchIndex = -1;
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
        int configuredMinHeight = 200;
    };
    struct SplitBinding {
        QPointer<SplitView> split;
        Qt::Orientation configuredOrientation = Qt::Horizontal;
        int configuredMinHeight = 180;
        QVector<QPointer<QWidget>> panes;
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

void GalleryStudioCanvas::fetchPhoto(const QUrl& url,
    QObject* lifetimeGuard,
    std::function<void(const QPixmap&)> onReady)
{
    if (isAutomatedTestRun() || !url.isValid() || url.scheme() != QLatin1String("https")
        || url.host() != QLatin1String("images.unsplash.com")) {
        return;
    }
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    req.setAttribute(QNetworkRequest::AutoDeleteReplyOnFinishAttribute, true);
#ifndef Q_OS_WASM
    req.setRawHeader("User-Agent", "FluentQtGalleryStudio/1.0");
#endif
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    QNetworkReply* reply = m_network->get(req);
    QObject::connect(lifetimeGuard, &QObject::destroyed, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, lifetimeGuard, [reply, onReady = std::move(onReady)]() {
        if (reply->error() != QNetworkReply::NoError || reply->url().host() != QLatin1String("images.unsplash.com"))
            return;
        QImageReader reader(reply);
        const auto format = reader.format().toLower();
        if (format != "jpeg" && format != "png" && format != "webp" && format != "avif")
            return;
        const QSize size = reader.size();
        if (size.width() > 1280 || size.height() > 1280)
            reader.setScaledSize(size.scaled(1280, 1280, Qt::KeepAspectRatio));
        const QImage img = reader.read();
        if (img.isNull())
            return;
        onReady(QPixmap::fromImage(img));
    });
}

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
    if (m_hasAccentOverride) {
        ThemeRegistry::instance().resetToDefaults();
        m_hasAccentOverride = false;
    }
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

    const QJsonObject design = document.value(QStringLiteral("design")).toObject();
    const QString designTheme = design.value(QStringLiteral("theme")).toString().trimmed().toLower();
    if (designTheme == QLatin1String("dark")) {
        m_theme = FluentElement::Dark;
    } else if (designTheme == QLatin1String("light")) {
        m_theme = FluentElement::Light;
    }
    const QString accentHex = design.value(QStringLiteral("accentColor")).toString().trimmed();
    static const QRegularExpression kAccentHexRe(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    const QColor accentColor = kAccentHexRe.match(accentHex).hasMatch() ? QColor(accentHex) : QColor();
    if (accentColor.isValid()) {
        UserTheme::applyAccentOverride(accentColor);
        m_hasAccentOverride = true;
    } else if (m_hasAccentOverride) {
        ThemeRegistry::instance().resetToDefaults();
        m_hasAccentOverride = false;
    }
    FluentElement::setTheme(m_theme);

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
    refreshCommandBarActionIcons(m_rootWidget.data(), m_theme);
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
            applyStudioExtendedIconProps(found->widget.data(), widgetPatch);
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
    refreshCommandBarActionIcons(m_rootWidget.data(), theme);
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
    applyStudioExtendedIconProps(widget, props);
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
        QStringList childTypes;
        for (const QJsonValue& childVal : children) {
            const QJsonObject childObj = childVal.toObject();
            if (QWidget* child = buildNode(childObj, box, built)) {
                hItems.append(child);
                childTypes.append(childObj.value(QStringLiteral("type")).toString());
            }
        }
        int splitStretchIndex = -1;
        if (isRow && hItems.size() >= 2
            && (alignKey == QStringLiteral("Center") || alignKey == QStringLiteral("Stretch") || alignKey.isEmpty())) {
            int prefixLen = 0;
            while (prefixLen < childTypes.size() && isHeaderLeadingControl(childTypes.at(prefixLen)))
                ++prefixLen;
            if (prefixLen >= 1 && prefixLen < childTypes.size() && isHeaderTrailingControl(childTypes.at(prefixLen))) {
                splitStretchIndex = prefixLen;
            }
        }
        for (int idx = 0; idx < hItems.size(); ++idx) {
            QWidget* child = hItems.at(idx).data();
            if (isRow && splitStretchIndex == idx)
                layout->addStretch(1);
            if (isRow && splitStretchIndex >= 0 && idx >= splitStretchIndex
                && (childTypes.at(idx) == QStringLiteral("AutoSuggestBox")
                    || childTypes.at(idx) == QStringLiteral("LineEdit"))) {
                child->setMinimumWidth(qMax(child->minimumWidth(), 180));
                child->setMaximumWidth(qMin(child->maximumWidth(), 260));
            }
            if (isRow && (alignKey == QStringLiteral("Center") || splitStretchIndex >= 0))
                layout->addWidget(child, 0, Qt::AlignVCenter);
            else if (isRow && alignKey == QStringLiteral("Start"))
                layout->addWidget(child, 0, Qt::AlignTop);
            else if (isRow && alignKey == QStringLiteral("End"))
                layout->addWidget(child, 0, Qt::AlignBottom);
            else
                layout->addWidget(child);
        }
        const bool hasStretch = isRow && splitStretchIndex < 0
            && (alignKey == QStringLiteral("Start") || alignKey == QStringLiteral("Center")
                || alignKey == QStringLiteral("End"));
        if (hasStretch)
            layout->addStretch(1);
        if (m_stagingResponsive) {
            if (isRow)
                m_stagingResponsive->hboxes.append(
                    {box, layout, hItems, alignKey, spacing, pad, hasStretch, splitStretchIndex});
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
            auto* hint = new Label(QStringLiteral("AnchorLayout"), box);
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
        sv->setMinimumWidth(0);
        sv->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sv->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sv->setWidgetResizable(true);
        sv->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(180), 640));
        auto* content = new QWidget();
        content->setMinimumWidth(0);
        content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 2, 0, 2);
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
        const int requestedMinH = qBound(120, props.value(QStringLiteral("minHeight")).toInt(180), 960);
        QVector<QPointer<QWidget>> panePtrs;
        if (children.isEmpty()) {
            SplitViewPaneOptions firstOpts;
            firstOpts.minimumSize = 140;
            firstOpts.preferredSize = 240;
            SplitViewPaneOptions secondOpts;
            secondOpts.minimumSize = 180;
            secondOpts.preferredSize = 360;
            secondOpts.fill = true;
            QWidget* p1 = makeSamplePane(split, QStringLiteral("Primary Pane"),
                QStringLiteral("Draggable splitter pane"));
            QWidget* p2 = makeSamplePane(split, QStringLiteral("Secondary Pane"),
                QStringLiteral("Adaptive detail view"));
            panePtrs.append(p1);
            panePtrs.append(p2);
            split->addPane(p1, WidgetOwnership::Owned, firstOpts);
            split->addPane(p2, WidgetOwnership::Owned, secondOpts);
        } else {
            QVector<QPair<QWidget*, QJsonObject>> builtPanes;
            builtPanes.reserve(children.size());
            panePtrs.reserve(children.size());
            for (const QJsonValue& childVal : children) {
                const QJsonObject childObj = childVal.toObject();
                if (QWidget* child = buildNode(childObj, split, built)) {
                    builtPanes.append({child, childObj});
                    panePtrs.append(child);
                }
            }
            const int count = builtPanes.size();
            int primaryIdx = count - 1;
            if (configuredOrientation == Qt::Horizontal && count >= 3) {
                primaryIdx = 1;
                for (int i = 0; i < count; ++i) {
                    if (!isSidebarLikeNode(builtPanes.at(i).second) && isPrimaryWideNode(builtPanes.at(i).second)) {
                        primaryIdx = i;
                        break;
                    }
                }
            }
            for (int i = 0; i < count; ++i) {
                QWidget* paneWidget = builtPanes.at(i).first;
                const QJsonObject& paneObj = builtPanes.at(i).second;
                if (configuredOrientation == Qt::Vertical || count < 2) {
                    split->addPane(paneWidget, WidgetOwnership::Owned);
                    continue;
                }
                SplitViewPaneOptions opts;
                if (count == 2) {
                    const bool leftSidebar = isSidebarLikeNode(builtPanes.at(0).second)
                        && !isSidebarLikeNode(builtPanes.at(1).second);
                    if (i == 0) {
                        opts.minimumSize = leftSidebar ? 150 : 180;
                        opts.preferredSize = leftSidebar ? 204 : 320;
                        opts.fill = false;
                    } else {
                        opts.minimumSize = leftSidebar ? 260 : 180;
                        opts.preferredSize = leftSidebar ? 480 : 320;
                        opts.fill = true;
                    }
                } else if (i == 0 && i != primaryIdx) {
                    const bool sidebar = isSidebarLikeNode(paneObj);
                    opts.minimumSize = sidebar ? 148 : 160;
                    opts.preferredSize = sidebar ? 188 : 210;
                    opts.fill = false;
                } else if (i == primaryIdx) {
                    opts.minimumSize = 260;
                    opts.preferredSize = 380;
                    opts.fill = true;
                } else {
                    opts.minimumSize = 210;
                    opts.preferredSize = 256;
                    opts.fill = false;
                }
                split->addPane(paneWidget, WidgetOwnership::Owned, opts);
            }
        }
        int contentH = 0;
        int hardMinH = 0;
        for (const auto& panePtr : panePtrs) {
            QWidget* pane = panePtr.data();
            const int ph = measureWidgetHeightForWidth(pane, configuredOrientation == Qt::Vertical ? 780 : 380);
            const int pMinH = pane
                ? (pane->layout() ? pane->layout()->totalMinimumSize().height()
                                  : qMax(pane->minimumHeight(), pane->minimumSizeHint().height()))
                : 0;
            contentH = (configuredOrientation == Qt::Vertical) ? (contentH + ph + 10) : qMax(contentH, ph);
            hardMinH = (configuredOrientation == Qt::Vertical) ? (hardMinH + pMinH + 10) : qMax(hardMinH, pMinH);
        }
        split->setMinimumHeight(qMax(qMax(requestedMinH, hardMinH + 16), qMin(contentH + 16, 1280)));
        if (m_stagingResponsive)
            m_stagingResponsive->splits.append({split, configuredOrientation, requestedMinH, panePtrs});
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
        ldv->setCompactThreshold(500);
        const int requestedMinH = qBound(150, props.value(QStringLiteral("minHeight")).toInt(200), 960);

        ldv->listPane()->setMinimumWidth(0);
        ldv->detailPane()->setMinimumWidth(0);
        auto* listLayout = new QVBoxLayout(ldv->listPane());
        listLayout->setContentsMargins(6, 6, 6, 6);
        listLayout->setSpacing(8);
        auto* detailLayout = new QVBoxLayout(ldv->detailPane());
        detailLayout->setContentsMargins(10, 6, 6, 6);
        detailLayout->setSpacing(10);

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
        const int listH = measureWidgetHeightForWidth(ldv->listPane(), paneWidth);
        const int detailH = measureWidgetHeightForWidth(ldv->detailPane(), 560);
        const int hardMinH = qMax(listLayout->totalMinimumSize().height(), detailLayout->totalMinimumSize().height())
            + 16;
        ldv->setMinimumHeight(qMax(qMax(requestedMinH, hardMinH), qMin(qMax(listH, detailH) + 16, 1280)));
        if (m_stagingResponsive)
            m_stagingResponsive->listDetails.append({ldv, paneWidth, requestedMinH});
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
        nav->setExpandedPaneWidth(184);
        const int requestedMinH = qBound(180, props.value(QStringLiteral("minHeight")).toInt(240), 720);

        auto* navPane = new QWidget();
        auto* paneLayout = new QVBoxLayout(navPane);
        paneLayout->setContentsMargins(6, 8, 6, 8);
        paneLayout->setSpacing(2);
        const QStringList navItems = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Home"), QStringLiteral("Analytics"), QStringLiteral("Settings")});
        auto navButtons = std::make_shared<QVector<QPointer<StudioNavRailItem>>>();
        navButtons->reserve(navItems.size());
        QStringList cleanNavLabels;
        cleanNavLabels.reserve(navItems.size());
        for (int i = 0; i < navItems.size(); ++i) {
            const StudioItemSpec spec = parseStudioItemIconAndLabel(navItems.at(i), i, true);
            cleanNavLabels.append(spec.label);
            auto* itemBtn = new StudioNavRailItem(nav, spec.label, spec.glyph, navPane);
            itemBtn->setChecked(i == 0);
            navButtons->append(itemBtn);
            connect(itemBtn, &QAbstractButton::clicked, this, [this, id, nav, i, navButtons]() {
                if (nav->contentHost() && i < nav->contentHost()->count())
                    nav->contentHost()->setCurrentIndex(i, 0, false);
                for (int j = 0; j < navButtons->size(); ++j) {
                    if (StudioNavRailItem* b = navButtons->at(j).data())
                        b->setChecked(j == i);
                }
                scheduleLayoutSnapshot();
                emit interactionEvent(id, QStringLiteral("navItemClicked"), i);
            });
            paneLayout->addWidget(itemBtn);
        }
        paneLayout->addStretch(1);
        nav->setMainChromeWidget(navPane, WidgetOwnership::Owned);

        int maxPageH = measureWidgetHeightForWidth(navPane, 184) + 16;
        if (StackContentHost* host = nav->contentHost()) {
            host->setTransitionAnimationEnabled(false);
            if (children.isEmpty()) {
                for (int i = 0; i < cleanNavLabels.size(); ++i) {
                    QWidget* sample = makeSamplePane(nullptr, cleanNavLabels.at(i),
                        QStringLiteral("NavigationView page content for %1.").arg(cleanNavLabels.at(i)));
                    maxPageH = qMax(maxPageH, measureWidgetHeightForWidth(sample, 620) + 24);
                    host->insertPage(i, sample, WidgetOwnership::Owned);
                }
            } else {
                for (int i = 0; i < children.size(); ++i) {
                    if (QWidget* child = buildNode(children.at(i).toObject(), nullptr, built)) {
                        maxPageH = qMax(maxPageH, measureWidgetHeightForWidth(child, 620) + 24);
                        host->insertPage(i, child, WidgetOwnership::Owned);
                    }
                }
            }
            if (host->count() > 0)
                host->setCurrentIndex(0, 0, false);
        }
        nav->setMinimumHeight(qMax(requestedMinH, qMin(maxPageH, 1280)));
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
        const int requestedMinH = qBound(180, props.value(QStringLiteral("minHeight")).toInt(224), 560);
        panel->setMinimumHeight(qMax(requestedMinH, panel->sizeHint().height() + 28));
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
        edit->setMinimumWidth(0);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
        const auto& colors = ThemeRegistry::instance().colors(m_theme);
        const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("New"), QStringLiteral("Save"), QStringLiteral("Share"), QStringLiteral("Settings")});
        for (int i = 0; i < items.size(); ++i) {
            const StudioItemSpec spec = parseStudioItemIconAndLabel(items.at(i), i, false);
            auto* act = new QAction(spec.label, bar);
            act->setProperty("_studioGlyph", spec.glyph);
            act->setIcon(makeThemedGlyphIcon(spec.glyph, colors, dpr));
            const QString cleanLabel = spec.label;
            connect(act, &QAction::triggered, this, [this, id, cleanLabel]() {
                emit interactionEvent(id, QStringLiteral("commandTriggered"), cleanLabel);
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
        grid->setCellSize(QSize(156, 128));
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(10);
        grid->setItemDelegate(new StudioGridTileDelegate(grid, grid, grid));
        const QStringList tileItems = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("Alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma"), QStringLiteral("Delta")});
        auto* model = buildStringModel(grid, tileItems);
        const QString mediaTheme = props.value(QStringLiteral("mediaTheme")).toString().trimmed();
        const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
        for (int i = 0; i < model->rowCount(); ++i) {
            QStandardItem* rowItem = model->item(i);
            const QColor tint = studioThemeTint(mediaTheme, i);
            const QString seed = QStringLiteral("%1-%2-%3").arg(id, mediaTheme, tileItems.value(i));
            rowItem->setData(detail::renderScenicPhotoPixmap(QSize(240, 140), tint, seed, dpr), Qt::DecorationRole);
            if (!mediaTheme.isEmpty() && mediaTheme != QLatin1String("None")) {
                const QPersistentModelIndex index(model->index(i, 0));
                fetchPhoto(studioCuratedPhotoUrl(mediaTheme, i, 360), model, [model, index](const QPixmap& pix) {
                    if (index.isValid())
                        model->setData(index, pix, Qt::DecorationRole);
                });
            }
        }
        grid->setModel(model);
        if (grid->model() && grid->model()->rowCount() > 0)
            grid->setSelectedIndex(0);
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
        const QStringList items = jsonStringList(props.value(QStringLiteral("items")),
            {QStringLiteral("src/"), QStringLiteral("  main.cpp"), QStringLiteral("tests/")});
        auto* model = buildTreeModel(tree, items);
        tree->setModel(model);
        tree->setHeaderHidden(true);
        tree->expandAll();
        if (model->rowCount() > 0)
            tree->setCurrentIndex(model->index(0, 0));
        tree->setBorderVisible(true);
        tree->setMinimumHeight(qBound(120, props.value(QStringLiteral("minHeight")).toInt(175), 420));
        return finish(tree);
    }

    if (type == QStringLiteral("DataGrid")) {
        auto* grid = new DataGrid(parentWidget);
        QStringList headers = jsonStringList(props.value(QStringLiteral("headers")), {});
        QList<QStringList> rows;
        const QJsonArray customRows = props.value(QStringLiteral("modelData")).toArray();
        if (!customRows.isEmpty()) {
            rows.reserve(customRows.size());
            for (const QJsonValue& rowVal : customRows) {
                QStringList cells;
                if (rowVal.isArray()) {
                    const QJsonArray arr = rowVal.toArray();
                    cells.reserve(arr.size());
                    for (const QJsonValue& c : arr) {
                        if (c.isString())
                            cells.append(c.toString());
                        else if (c.isDouble())
                            cells.append(QString::number(c.toDouble()));
                        else if (c.isBool())
                            cells.append(c.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
                        else
                            cells.append(QString());
                    }
                } else if (!rowVal.toString().isEmpty()) {
                    cells.append(rowVal.toString());
                }
                if (!cells.isEmpty())
                    rows.append(cells);
            }
        }
        int colCount = headers.size();
        for (const QStringList& r : std::as_const(rows))
            colCount = qMax(colCount, r.size());
        if (rows.isEmpty() && headers.isEmpty()) {
            headers = QStringList{QStringLiteral("Module"), QStringLiteral("Qt Target"), QStringLiteral("Status")};
            rows = {
                {QStringLiteral("BasicInput"), QStringLiteral("Qt 6.9"), QStringLiteral("Ready")},
                {QStringLiteral("Charts"), QStringLiteral("Qt 6.9"), QStringLiteral("Verified")},
                {QStringLiteral("Collections"), QStringLiteral("Qt 6.9"), QStringLiteral("Ready")},
            };
            colCount = 3;
        } else {
            colCount = qBound(1, colCount, 12);
            while (headers.size() < colCount)
                headers.append(QStringLiteral("Column %1").arg(headers.size() + 1));
            if (rows.isEmpty()) {
                QStringList emptyRow;
                emptyRow.reserve(colCount);
                for (int c = 0; c < colCount; ++c)
                    emptyRow.append(QStringLiteral("--"));
                rows.append(emptyRow);
            }
        }
        auto* model = new QStandardItemModel(rows.size(), colCount, grid);
        model->setHorizontalHeaderLabels(headers.mid(0, colCount));
        for (int r = 0; r < rows.size(); ++r) {
            for (int c = 0; c < colCount; ++c)
                model->setItem(r, c, new QStandardItem(c < rows[r].size() ? rows[r][c] : QString()));
        }
        grid->setModel(model);
        grid->setItemDelegate(new StudioDataGridDelegate(grid));
        if (grid->horizontalHeader())
            grid->horizontalHeader()->setStretchLastSection(true);
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
        const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
        const QString svgText = props.value(QStringLiteral("svg")).toString();
        const QString mediaTheme = props.value(QStringLiteral("mediaTheme")).toString().trimmed();
        const QString imageUrlStr = props.value(QStringLiteral("imageUrl")).toString().trimmed();
        QPixmap initialPix = renderSanitizedSvgPixmap(svgText, QSize(640, 380), dpr);
        const bool usedSvg = !initialPix.isNull();
        if (!usedSvg) {
            const QColor tint = studioThemeTint(mediaTheme, 0);
            initialPix = detail::renderScenicPhotoPixmap(QSize(640, 380), tint,
                QStringLiteral("%1-%2").arg(id, mediaTheme), dpr);
        }
        viewer->setPixmap(initialPix);
        viewer->setFitMode(ImageViewer::FitMode::Contain);
        viewer->setTransformationMode(Qt::SmoothTransformation);
        viewer->setFrameVisible(true);
        viewer->setMinimumHeight(qBound(140, props.value(QStringLiteral("minHeight")).toInt(200), 420));

        if (!usedSvg) {
            QUrl fetchUrl;
            static const QRegularExpression kUnsplashUrlRe(
                QStringLiteral("^https://images\\.unsplash\\.com/photo-[a-zA-Z0-9_-]+(?:[?&#][^\\s\"'<>]*)?$"));
            if (imageUrlStr.size() <= 512 && kUnsplashUrlRe.match(imageUrlStr).hasMatch())
                fetchUrl = QUrl(imageUrlStr);
            else if (!mediaTheme.isEmpty())
                fetchUrl = studioCuratedPhotoUrl(mediaTheme, 0, 960);

            fetchPhoto(fetchUrl, viewer, [viewer](const QPixmap& pix) { viewer->setPixmap(pix); });
        }
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
        auto* titleContent = new QWidget(bar);
        auto* titleLayout = new QHBoxLayout(titleContent);
        titleLayout->setContentsMargins(8, 0, 8, 0);
        titleLayout->setSpacing(8);
        auto* icon = new FontIcon(Typography::Icons::Home, titleContent);
        icon->setIconSize(14);
        titleLayout->addWidget(icon);
        auto* title = new Label(props.value(QStringLiteral("title")).toString(QStringLiteral("Fluent Studio Window")),
            titleContent);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        titleLayout->addWidget(title);
        bar->setContentWidget(titleContent);
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
            int childIdx = 0;
            for (const auto& itemPtr : hbox.items) {
                if (!itemPtr)
                    continue;
                if (!compact && hbox.splitStretchIndex == childIdx)
                    hbox.layout->addStretch(1);
                QWidget* child = itemPtr.data();
                if (auto* lbl = qobject_cast<Label*>(child)) {
                    lbl->setWordWrap(compact);
                    lbl->setSizePolicy(compact ? QSizePolicy::Ignored : QSizePolicy::Preferred, QSizePolicy::Preferred);
                } else if (auto* btn = qobject_cast<Button*>(child)) {
                    btn->setSizePolicy(compact
                            ? QSizePolicy::Expanding
                            : (hbox.alignKey == QStringLiteral("Stretch") && hbox.splitStretchIndex < 0
                                      ? QSizePolicy::Preferred
                                      : QSizePolicy::Maximum),
                        btn->sizePolicy().verticalPolicy());
                }
                if (!compact && (hbox.alignKey == QStringLiteral("Center") || hbox.splitStretchIndex >= 0))
                    hbox.layout->addWidget(child, 0, Qt::AlignVCenter);
                else if (!compact && hbox.alignKey == QStringLiteral("Start"))
                    hbox.layout->addWidget(child, 0, Qt::AlignTop);
                else if (!compact && hbox.alignKey == QStringLiteral("End"))
                    hbox.layout->addWidget(child, 0, Qt::AlignBottom);
                else
                    hbox.layout->addWidget(child);
                ++childIdx;
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

    // 4. Sync adaptive ListDetailView & SplitView minimum heights from final child layout hints.
    for (const auto& ld : std::as_const(m_responsive->listDetails)) {
        if (!ld.view)
            continue;
        const int effListW = tier == LayoutTier::Medium
            ? qMin(ld.configuredPaneWidth, 172)
            : (compact ? qMin(ld.configuredPaneWidth, 128) : ld.configuredPaneWidth);
        const int detailW = compact ? qMax(200, width - 28) : qMax(220, width - effListW - 36);
        const int listH = measureWidgetHeightForWidth(ld.view->listPane(), effListW);
        const int detailH = measureWidgetHeightForWidth(ld.view->detailPane(), detailW);
        const int listMinH =
            ld.view->listPane()->layout() ? ld.view->listPane()->layout()->totalMinimumSize().height() : 0;
        const int detailMinH =
            ld.view->detailPane()->layout() ? ld.view->detailPane()->layout()->totalMinimumSize().height() : 0;
        const int hardMinH = qMax(listMinH, detailMinH) + 16;
        const int targetH = qMax(qMax(ld.configuredMinHeight, hardMinH), qMin(qMax(listH, detailH) + 16, 1280));
        if (ld.view->minimumHeight() != targetH)
            ld.view->setMinimumHeight(targetH);
        ld.view->updateGeometry();
    }

    for (const auto& sb : std::as_const(m_responsive->splits)) {
        if (!sb.split)
            continue;
        const Qt::Orientation effOrient = compact ? Qt::Vertical : sb.configuredOrientation;
        int contentH = 0;
        int hardMinH = 0;
        const int paneCount = qMax(1, sb.panes.size());
        const int paneAvailW =
            (effOrient == Qt::Vertical) ? qMax(200, width - 32) : qMax(180, (width - 36) / paneCount);
        for (const auto& panePtr : sb.panes) {
            QWidget* pane = panePtr.data();
            const int ph = measureWidgetHeightForWidth(pane, paneAvailW);
            const int pMinH = pane
                ? (pane->layout() ? pane->layout()->totalMinimumSize().height()
                                  : qMax(pane->minimumHeight(), pane->minimumSizeHint().height()))
                : 0;
            contentH = (effOrient == Qt::Vertical) ? (contentH + ph + 10) : qMax(contentH, ph);
            hardMinH = (effOrient == Qt::Vertical) ? (hardMinH + pMinH + 10) : qMax(hardMinH, pMinH);
        }
        const int targetH = qMax(qMax(sb.configuredMinHeight, hardMinH + 16), qMin(contentH + 16, 1280));
        if (sb.split->minimumHeight() != targetH)
            sb.split->setMinimumHeight(targetH);
        sb.split->updateGeometry();
    }

    m_rootWidget->updateGeometry();
    if (QLayout* rootLayout = m_rootWidget->layout())
        rootLayout->invalidate();
    if (m_stageLayout) {
        m_stageLayout->invalidate();
        m_stageLayout->activate();
    }
    const int vpW = (m_scroll && m_scroll->viewport()->width() > 0) ? m_scroll->viewport()->width() : width;
    const int minStageH = m_scroll ? m_scroll->viewport()->height() : height();
    const int rootDesiredH = m_rootIsAnchorBox
        ? qMax(m_rootWidget->minimumHeight(), m_rootWidget->sizeHint().height())
        : measureWidgetHeightForWidth(m_rootWidget.data(), vpW, 4800);
    const int neededH = qMax(minStageH, rootDesiredH);
    m_stage->resize(vpW, neededH);
    if (m_stageLayout)
        m_stageLayout->activate();

    // Settle nested layouts top-down synchronously so pane and card heights match final parent bounds immediately.
    if (QLayout* rootLayout = m_rootWidget->layout())
        rootLayout->activate();
    for (const auto& ld : std::as_const(m_responsive->listDetails)) {
        if (!ld.view)
            continue;
        if (QLayout* ll = ld.view->listPane()->layout())
            ll->activate();
        if (QLayout* dl = ld.view->detailPane()->layout())
            dl->activate();
    }
    for (const auto& sb : std::as_const(m_responsive->splits)) {
        if (!sb.split)
            continue;
        for (const auto& panePtr : sb.panes) {
            if (panePtr && panePtr->layout())
                panePtr->layout()->activate();
        }
    }
    for (const auto& box : std::as_const(m_responsive->boxes)) {
        if (box.host && box.layout)
            box.layout->activate();
    }
    for (const auto& grid : std::as_const(m_responsive->grids)) {
        if (grid.host && grid.layout)
            grid.layout->activate();
    }
    for (const auto& hbox : std::as_const(m_responsive->hboxes)) {
        if (hbox.host && hbox.layout)
            hbox.layout->activate();
    }
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

#include "view/widgets/GalleryStudioCanvas.h"

#include "components/basicinput/Button.h"
#include "components/basicinput/CheckBox.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/Slider.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/charts/BarChart.h"
#include "components/charts/LineChart.h"
#include "components/collections/DataGrid.h"
#include "components/collections/GridView.h"
#include "components/collections/SplitView.h"
#include "components/collections/TreeView.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/UserTheme.h"
#include "components/layout/Card.h"
#include "components/layout/Field.h"
#include "components/layout/ListDetailView.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/scrolling/ImageViewer.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/TextEdit.h"
#include "design/Typography.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

namespace {

using fluent::FluentElement;
using fluent::gallery::GalleryStudioCanvas;

QJsonObject node(const QString& id, const QString& type, const QJsonObject& props = {}, const QJsonArray& children = {})
{
    QJsonObject obj{{QStringLiteral("id"), id}, {QStringLiteral("type"), type}, {QStringLiteral("props"), props}};
    if (!children.isEmpty())
        obj.insert(QStringLiteral("children"), children);
    return obj;
}

class GalleryStudioCanvasTest : public ::testing::Test {
protected:
    void SetUp() override { FluentElement::setTheme(FluentElement::Light); }
};

TEST_F(GalleryStudioCanvasTest, BuildsAllSupportedTypesAndReportsLayoutBounds)
{
    GalleryStudioCanvas canvas;
    canvas.resize(960, 720);
    canvas.show();

    const QStringList allTypes = GalleryStudioCanvas::supportedTypes();
    EXPECT_EQ(allTypes.size(), 103);
    QJsonArray rootChildren;
    QSet<QString> coveredTypes{QStringLiteral("VBox"), QStringLiteral("HBox"), QStringLiteral("Grid"),
        QStringLiteral("AnchorBox"), QStringLiteral("Card"), QStringLiteral("Expander"), QStringLiteral("Field"),
        QStringLiteral("SpatialView"), QStringLiteral("SpatialItem")};

    rootChildren.append(node(QStringLiteral("row_box"), QStringLiteral("HBox"), {{QStringLiteral("spacing"), 8}},
        {node(QStringLiteral("row_btn"), QStringLiteral("Button"), {{QStringLiteral("text"), QStringLiteral("Save")}}),
            node(QStringLiteral("row_badge"), QStringLiteral("InfoBadge"), {{QStringLiteral("value"), 3}})}));

    rootChildren.append(node(QStringLiteral("grid_box"), QStringLiteral("Grid"), {{QStringLiteral("columns"), 2}},
        {node(QStringLiteral("grid_lbl"), QStringLiteral("Label"), {{QStringLiteral("text"), QStringLiteral("Key")}}),
            node(QStringLiteral("grid_edit"), QStringLiteral("LineEdit"),
                {{QStringLiteral("placeholderText"), QStringLiteral("Value")}})}));

    rootChildren.append(node(QStringLiteral("anchor_box"), QStringLiteral("AnchorBox"),
        {{QStringLiteral("minHeight"), 180}, {QStringLiteral("padding"), 12}},
        {node(QStringLiteral("anchor_btn"), QStringLiteral("Button"),
            {{QStringLiteral("text"), QStringLiteral("Anchored")},
                {QStringLiteral("anchorPosition"), QStringLiteral("BottomRight")}})}));

    rootChildren.append(node(QStringLiteral("spatial_box"), QStringLiteral("SpatialView"),
        {{QStringLiteral("minHeight"), 220}},
        {node(QStringLiteral("spatial_item_box"), QStringLiteral("SpatialItem"), {{QStringLiteral("rotY"), -14}},
            {node(QStringLiteral("spatial_card"), QStringLiteral("Card"), {}, {})})}));

    rootChildren.append(node(QStringLiteral("card_box"), QStringLiteral("Card"), {},
        {node(QStringLiteral("field_box"), QStringLiteral("Field"),
            {{QStringLiteral("labelText"), QStringLiteral("Endpoint")}},
            {node(QStringLiteral("field_input"), QStringLiteral("LineEdit"), {})})}));

    rootChildren.append(node(QStringLiteral("expander_box"), QStringLiteral("Expander"),
        {{QStringLiteral("headerText"), QStringLiteral("Advanced")}, {QStringLiteral("expanded"), true}},
        {node(QStringLiteral("exp_chk"), QStringLiteral("CheckBox"),
            {{QStringLiteral("text"), QStringLiteral("Verbose")}})}));

    for (const QString& type : allTypes) {
        if (coveredTypes.contains(type))
            continue;
        rootChildren.append(node(QStringLiteral("item_%1").arg(type), type));
    }

    const QJsonObject doc{{QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("root"), node(QStringLiteral("root_vbox"), QStringLiteral("VBox"), {}, rootChildren)}};

    QString error;
    QSignalSpy layoutSpy(&canvas, &GalleryStudioCanvas::layoutChanged);
    ASSERT_TRUE(canvas.applyDocument(doc, &error)) << error.toStdString();
    EXPECT_TRUE(error.isEmpty());
    QApplication::processEvents();
    EXPECT_GE(layoutSpy.count(), 1);
    EXPECT_EQ(canvas.nodeCount(), rootChildren.size() + 1 + 2 + 2 + 1 + 2 + 2 + 1);

    const QJsonObject snapshot = canvas.layoutSnapshot();
    const QJsonObject nodes = snapshot.value(QStringLiteral("nodes")).toObject();
    for (const QString& type : allTypes) {
        const QString id = type == QStringLiteral("VBox") ? QStringLiteral("root_vbox")
            : type == QStringLiteral("HBox")              ? QStringLiteral("row_box")
            : type == QStringLiteral("Grid")              ? QStringLiteral("grid_box")
            : type == QStringLiteral("AnchorBox")         ? QStringLiteral("anchor_box")
            : type == QStringLiteral("Card")              ? QStringLiteral("card_box")
            : type == QStringLiteral("Expander")          ? QStringLiteral("expander_box")
            : type == QStringLiteral("Field")             ? QStringLiteral("field_box")
            : type == QStringLiteral("SpatialView")       ? QStringLiteral("spatial_box")
            : type == QStringLiteral("SpatialItem")
            ? QStringLiteral("spatial_item_box")
            : QStringLiteral("item_%1").arg(type);
        ASSERT_TRUE(nodes.contains(id)) << id.toStdString();
        const QJsonObject box = nodes.value(id).toObject();
        EXPECT_TRUE(box.value(QStringLiteral("visible")).toBool()) << id.toStdString();
        EXPECT_GT(box.value(QStringLiteral("w")).toInt(), 0) << id.toStdString();
        EXPECT_GT(box.value(QStringLiteral("h")).toInt(), 0) << id.toStdString();
    }

    QWidget* anchorHost = canvas.widgetForNode(QStringLiteral("anchor_box"));
    ASSERT_NE(anchorHost, nullptr);
    EXPECT_NE(qobject_cast<fluent::AnchorLayout*>(anchorHost->layout()), nullptr);
    EXPECT_NE(qobject_cast<fluent::charts::LineChart*>(canvas.widgetForNode(QStringLiteral("item_LineChart"))),
        nullptr);
    EXPECT_NE(qobject_cast<fluent::charts::BarChart*>(canvas.widgetForNode(QStringLiteral("item_BarChart"))), nullptr);
}

TEST_F(GalleryStudioCanvasTest, AppliesTypedPropertiesAndEmitsInteractionEvents)
{
    GalleryStudioCanvas canvas;
    canvas.resize(760, 620);
    canvas.show();

    const QJsonObject doc{{QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("root"),
            node(QStringLiteral("root"), QStringLiteral("VBox"), {},
                {
                    node(QStringLiteral("btn"), QStringLiteral("Button"),
                        {{QStringLiteral("text"), QStringLiteral("Submit")},
                            {QStringLiteral("fluentStyle"), QStringLiteral("Accent")}}),
                    node(QStringLiteral("chk"), QStringLiteral("CheckBox"),
                        {{QStringLiteral("text"), QStringLiteral("Enable")}, {QStringLiteral("checked"), false}}),
                    node(QStringLiteral("sw"), QStringLiteral("ToggleSwitch"), {{QStringLiteral("isOn"), false}}),
                    node(QStringLiteral("sld"), QStringLiteral("Slider"),
                        {{QStringLiteral("minimum"), 0}, {QStringLiteral("maximum"), 100},
                            {QStringLiteral("value"), 20}}),
                    node(QStringLiteral("cmb"), QStringLiteral("ComboBox"),
                        {{QStringLiteral("items"), QJsonArray{QStringLiteral("Alpha"), QStringLiteral("Beta")}},
                            {QStringLiteral("currentIndex"), 0}}),
                    node(QStringLiteral("edt"), QStringLiteral("LineEdit"),
                        {{QStringLiteral("placeholderText"), QStringLiteral("Name")}}),
                })}};

    QString error;
    ASSERT_TRUE(canvas.applyDocument(doc, &error)) << error.toStdString();
    QApplication::processEvents();

    QSignalSpy eventSpy(&canvas, &GalleryStudioCanvas::interactionEvent);
    auto* button = qobject_cast<fluent::basicinput::Button*>(canvas.widgetForNode(QStringLiteral("btn")));
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text(), QStringLiteral("Submit"));
    EXPECT_EQ(button->fluentStyle(), fluent::basicinput::Button::Accent);
    button->click();

    auto* check = qobject_cast<fluent::basicinput::CheckBox*>(canvas.widgetForNode(QStringLiteral("chk")));
    ASSERT_NE(check, nullptr);
    check->setChecked(true);

    auto* sw = qobject_cast<fluent::basicinput::ToggleSwitch*>(canvas.widgetForNode(QStringLiteral("sw")));
    ASSERT_NE(sw, nullptr);
    sw->setIsOn(true);

    auto* slider = qobject_cast<fluent::basicinput::Slider*>(canvas.widgetForNode(QStringLiteral("sld")));
    ASSERT_NE(slider, nullptr);
    slider->setValue(75);

    auto* combo = qobject_cast<fluent::basicinput::ComboBox*>(canvas.widgetForNode(QStringLiteral("cmb")));
    ASSERT_NE(combo, nullptr);
    combo->setCurrentIndex(1);

    auto* edit = qobject_cast<fluent::textfields::LineEdit*>(canvas.widgetForNode(QStringLiteral("edt")));
    ASSERT_NE(edit, nullptr);
    edit->setText(QStringLiteral("Fluent"));

    EXPECT_GE(eventSpy.count(), 6);
}

TEST_F(GalleryStudioCanvasTest, RejectsInvalidDocumentsAndEnforcesDepthAndNodeLimits)
{
    GalleryStudioCanvas canvas;
    QString error;

    EXPECT_FALSE(canvas.applyDocument(QJsonObject{{QStringLiteral("schemaVersion"), 9}}, &error));
    EXPECT_FALSE(error.isEmpty());

    EXPECT_FALSE(canvas.applyDocument(
        QJsonObject{{QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("root"), node(QStringLiteral("x"), QStringLiteral("UnknownWidget"))}},
        &error));
    EXPECT_TRUE(error.contains(QStringLiteral("Unsupported")));

    QJsonObject deep = node(QStringLiteral("leaf"), QStringLiteral("Label"));
    for (int i = 0; i < GalleryStudioCanvas::kMaxDepth + 1; ++i)
        deep = node(QStringLiteral("box_%1").arg(i), QStringLiteral("VBox"), {}, {deep});
    EXPECT_FALSE(canvas.applyDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("root"), deep}},
        &error));
    EXPECT_TRUE(error.contains(QStringLiteral("depth")));

    QJsonArray tooMany;
    for (int i = 0; i < GalleryStudioCanvas::kMaxNodes + 1; ++i)
        tooMany.append(node(QStringLiteral("n_%1").arg(i), QStringLiteral("Divider")));
    EXPECT_FALSE(canvas.applyDocument(
        QJsonObject{{QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("root"), node(QStringLiteral("root"), QStringLiteral("VBox"), {}, tooMany)}},
        &error));
    EXPECT_TRUE(error.contains(QStringLiteral("maximum node count")));
}

TEST_F(GalleryStudioCanvasTest, SwitchesThemeAndRebuildsWithoutLeakingOldNodes)
{
    GalleryStudioCanvas canvas;
    canvas.resize(720, 540);
    canvas.show();

    const QJsonObject first{{QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("root"),
            node(QStringLiteral("root1"), QStringLiteral("VBox"), {},
                {node(QStringLiteral("old_btn"), QStringLiteral("Button"))})}};
    ASSERT_TRUE(canvas.applyDocument(first));
    QApplication::processEvents();
    ASSERT_NE(canvas.widgetForNode(QStringLiteral("old_btn")), nullptr);

    canvas.setTheme(FluentElement::Dark);
    EXPECT_EQ(canvas.theme(), FluentElement::Dark);
    canvas.setSelectedNodeId(QStringLiteral("old_btn"));
    EXPECT_EQ(canvas.selectedNodeId(), QStringLiteral("old_btn"));

    const QJsonObject second{{QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("root"),
            node(QStringLiteral("root2"), QStringLiteral("VBox"), {},
                {node(QStringLiteral("new_lbl"), QStringLiteral("Label"))})}};
    ASSERT_TRUE(canvas.applyDocument(second));
    QApplication::processEvents();
    EXPECT_EQ(canvas.widgetForNode(QStringLiteral("old_btn")), nullptr);
    EXPECT_NE(canvas.widgetForNode(QStringLiteral("new_lbl")), nullptr);
    EXPECT_EQ(canvas.nodeCount(), 2);
}

TEST_F(GalleryStudioCanvasTest, AdaptsLayoutsAcrossExpandedMediumAndCompactBreakpoints)
{
    GalleryStudioCanvas canvas;
    canvas.resize(860, 720);
    canvas.show();

    const QJsonObject root = node(QStringLiteral("root"), QStringLiteral("VBox"),
        {{QStringLiteral("spacing"), 14}, {QStringLiteral("padding"), 20}},
        {
            node(QStringLiteral("grid_3"), QStringLiteral("Grid"),
                {{QStringLiteral("columns"), 3}, {QStringLiteral("spacing"), 12}},
                {
                    node(QStringLiteral("g3_1"), QStringLiteral("Button"),
                        {{QStringLiteral("text"), QStringLiteral("One")}}),
                    node(QStringLiteral("g3_2"), QStringLiteral("Button"),
                        {{QStringLiteral("text"), QStringLiteral("Two")}}),
                    node(QStringLiteral("g3_3"), QStringLiteral("Button"),
                        {{QStringLiteral("text"), QStringLiteral("Three")}}),
                }),
            node(QStringLiteral("grid_2"), QStringLiteral("Grid"),
                {{QStringLiteral("columns"), 2}, {QStringLiteral("spacing"), 14}},
                {
                    node(QStringLiteral("field_theme"), QStringLiteral("Field"),
                        {{QStringLiteral("labelText"), QStringLiteral("Default Density")},
                            {QStringLiteral("labelPosition"), QStringLiteral("Top")}},
                        {node(QStringLiteral("combo_density"), QStringLiteral("ComboBox"))}),
                    node(QStringLiteral("field_port"), QStringLiteral("Field"),
                        {{QStringLiteral("labelText"), QStringLiteral("Render Scale")},
                            {QStringLiteral("labelPosition"), QStringLiteral("Top")}},
                        {node(QStringLiteral("slider_scale"), QStringLiteral("Slider"))}),
                }),
            node(QStringLiteral("lead_field"), QStringLiteral("Field"),
                {{QStringLiteral("labelText"), QStringLiteral("Username")},
                    {QStringLiteral("labelPosition"), QStringLiteral("Leading")}},
                {node(QStringLiteral("edit_user"), QStringLiteral("LineEdit"))}),
            node(QStringLiteral("row_actions"), QStringLiteral("HBox"),
                {{QStringLiteral("spacing"), 10}, {QStringLiteral("align"), QStringLiteral("Start")}},
                {
                    node(QStringLiteral("btn_apply"), QStringLiteral("Button"),
                        {{QStringLiteral("text"), QStringLiteral("Apply Settings")}}),
                    node(QStringLiteral("btn_reset"), QStringLiteral("Button"),
                        {{QStringLiteral("text"), QStringLiteral("Reset Defaults")}}),
                }),
            node(QStringLiteral("nav_auto"), QStringLiteral("NavigationView"),
                {{QStringLiteral("displayMode"), QStringLiteral("Auto")}, {QStringLiteral("paneOpen"), true}}),
            node(QStringLiteral("split_view"), QStringLiteral("SplitView"),
                {{QStringLiteral("orientation"), QStringLiteral("Horizontal")}}),
        });

    ASSERT_TRUE(
        canvas.applyDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("root"), root}}));
    QApplication::processEvents();
    QTest::qWait(20);

    auto* leadField = qobject_cast<fluent::layout::Field*>(canvas.widgetForNode(QStringLiteral("lead_field")));
    auto* navAuto = qobject_cast<fluent::navigation::NavigationView*>(canvas.widgetForNode(QStringLiteral("nav_auto")));
    auto* splitView = qobject_cast<fluent::collections::SplitView*>(canvas.widgetForNode(QStringLiteral("split_view")));
    ASSERT_NE(leadField, nullptr);
    ASSERT_NE(navAuto, nullptr);
    ASSERT_NE(splitView, nullptr);

    auto rectOf = [](const QJsonObject& nodes, const QString& id) {
        const QJsonObject obj = nodes.value(id).toObject();
        return QRect(obj.value(QStringLiteral("x")).toInt(), obj.value(QStringLiteral("y")).toInt(),
            obj.value(QStringLiteral("w")).toInt(), obj.value(QStringLiteral("h")).toInt());
    };

    // Expanded (860px)
    QJsonObject snap = canvas.layoutSnapshot();
    EXPECT_EQ(canvas.currentLayoutTier(), GalleryStudioCanvas::LayoutTier::Expanded);
    EXPECT_EQ(snap.value(QStringLiteral("layoutTier")).toString(), QStringLiteral("expanded"));
    QJsonObject nodes = snap.value(QStringLiteral("nodes")).toObject();
    EXPECT_GT(rectOf(nodes, QStringLiteral("g3_2")).x(), rectOf(nodes, QStringLiteral("g3_1")).x());
    EXPECT_GT(rectOf(nodes, QStringLiteral("g3_3")).x(), rectOf(nodes, QStringLiteral("g3_2")).x());
    EXPECT_LE(qAbs(rectOf(nodes, QStringLiteral("g3_3")).y() - rectOf(nodes, QStringLiteral("g3_1")).y()), 4);
    EXPECT_GT(rectOf(nodes, QStringLiteral("field_port")).x(), rectOf(nodes, QStringLiteral("field_theme")).x() + 80);
    EXPECT_GT(rectOf(nodes, QStringLiteral("btn_reset")).x(), rectOf(nodes, QStringLiteral("btn_apply")).x() + 20);
    EXPECT_EQ(leadField->labelPosition(), fluent::layout::Field::LabelPosition::Leading);
    EXPECT_EQ(navAuto->displayMode(), fluent::navigation::NavigationView::DisplayMode::Left);
    EXPECT_EQ(splitView->orientation(), Qt::Horizontal);

    // Medium / Tablet (620px)
    canvas.resize(620, 720);
    QApplication::processEvents();
    QTest::qWait(20);
    snap = canvas.layoutSnapshot();
    EXPECT_EQ(canvas.currentLayoutTier(), GalleryStudioCanvas::LayoutTier::Medium);
    EXPECT_EQ(snap.value(QStringLiteral("layoutTier")).toString(), QStringLiteral("medium"));
    nodes = snap.value(QStringLiteral("nodes")).toObject();
    EXPECT_GT(rectOf(nodes, QStringLiteral("g3_2")).x(), rectOf(nodes, QStringLiteral("g3_1")).x());
    EXPECT_GT(rectOf(nodes, QStringLiteral("g3_3")).y(), rectOf(nodes, QStringLiteral("g3_1")).y() + 10);
    EXPECT_GT(rectOf(nodes, QStringLiteral("field_port")).x(), rectOf(nodes, QStringLiteral("field_theme")).x() + 60);
    EXPECT_EQ(leadField->labelPosition(), fluent::layout::Field::LabelPosition::Top);
    EXPECT_EQ(navAuto->displayMode(), fluent::navigation::NavigationView::DisplayMode::LeftCompact);
    EXPECT_EQ(splitView->orientation(), Qt::Horizontal);

    // Compact (400px)
    canvas.resize(400, 800);
    QApplication::processEvents();
    QTest::qWait(20);
    snap = canvas.layoutSnapshot();
    EXPECT_EQ(canvas.currentLayoutTier(), GalleryStudioCanvas::LayoutTier::Compact);
    EXPECT_EQ(snap.value(QStringLiteral("layoutTier")).toString(), QStringLiteral("compact"));
    nodes = snap.value(QStringLiteral("nodes")).toObject();
    EXPECT_GT(rectOf(nodes, QStringLiteral("field_port")).y(), rectOf(nodes, QStringLiteral("field_theme")).y() + 10);
    EXPECT_LE(qAbs(rectOf(nodes, QStringLiteral("field_port")).x() - rectOf(nodes, QStringLiteral("field_theme")).x()),
        6);
    EXPECT_GT(rectOf(nodes, QStringLiteral("btn_reset")).y(), rectOf(nodes, QStringLiteral("btn_apply")).y() + 10);
    EXPECT_EQ(leadField->labelPosition(), fluent::layout::Field::LabelPosition::Top);
    EXPECT_EQ(navAuto->displayMode(), fluent::navigation::NavigationView::DisplayMode::Top);
    EXPECT_EQ(splitView->orientation(), Qt::Vertical);

    // Round-trip back to Expanded (860px)
    canvas.resize(860, 720);
    QApplication::processEvents();
    QTest::qWait(20);
    snap = canvas.layoutSnapshot();
    EXPECT_EQ(canvas.currentLayoutTier(), GalleryStudioCanvas::LayoutTier::Expanded);
    nodes = snap.value(QStringLiteral("nodes")).toObject();
    EXPECT_GT(rectOf(nodes, QStringLiteral("field_port")).x(), rectOf(nodes, QStringLiteral("field_theme")).x() + 80);
    EXPECT_GT(rectOf(nodes, QStringLiteral("btn_reset")).x(), rectOf(nodes, QStringLiteral("btn_apply")).x() + 20);
    EXPECT_EQ(leadField->labelPosition(), fluent::layout::Field::LabelPosition::Leading);
    EXPECT_EQ(splitView->orientation(), Qt::Horizontal);
}

TEST_F(GalleryStudioCanvasTest, InspectsAndAppliesPlaygroundPropertiesAcrossControls)
{
    GalleryStudioCanvas canvas;
    canvas.resize(760, 640);
    QSignalSpy inspectedSpy(&canvas, &GalleryStudioCanvas::nodeInspected);

    const QJsonObject root = node(QStringLiteral("root"), QStringLiteral("VBox"),
        {{QStringLiteral("spacing"), 12}, {QStringLiteral("padding"), 16}},
        {
            node(QStringLiteral("sld"), QStringLiteral("Slider"),
                {{QStringLiteral("minimum"), 50}, {QStringLiteral("maximum"), 200}, {QStringLiteral("value"), 120},
                    {QStringLiteral("tickPosition"), QStringLiteral("TicksBothSides")},
                    {QStringLiteral("tickInterval"), 10}, {QStringLiteral("handleSize"), 22}}),
            node(QStringLiteral("btn"), QStringLiteral("Button"),
                {{QStringLiteral("text"), QStringLiteral("Save Item")},
                    {QStringLiteral("icon"), QStringLiteral("Save")}, {QStringLiteral("cornerRadius"), 12},
                    {QStringLiteral("contentOpacity"), 0.85}, {QStringLiteral("checkable"), true},
                    {QStringLiteral("checked"), true}}),
            node(QStringLiteral("info"), QStringLiteral("InfoBar"),
                {{QStringLiteral("title"), QStringLiteral("Heads up")},
                    {QStringLiteral("message"), QStringLiteral("OK")}}),
        });

    ASSERT_TRUE(
        canvas.applyDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("root"), root}}));
    QApplication::processEvents();

    auto* slider = qobject_cast<fluent::basicinput::Slider*>(canvas.widgetForNode(QStringLiteral("sld")));
    ASSERT_NE(slider, nullptr);
    EXPECT_EQ(slider->minimum(), 50);
    EXPECT_EQ(slider->maximum(), 200);
    EXPECT_EQ(slider->value(), 120);
    EXPECT_EQ(slider->tickPosition(), QSlider::TicksBothSides);
    EXPECT_EQ(slider->tickInterval(), 10);
    EXPECT_EQ(slider->handleSize(), 22);

    canvas.setSelectedNodeId(QStringLiteral("sld"));
    ASSERT_GE(inspectedSpy.count(), 1);
    const QJsonObject sldInfo = canvas.inspectNode(QStringLiteral("sld"));
    EXPECT_TRUE(sldInfo.value(QStringLiteral("supported")).toBool());
    const QJsonObject sldSchema = sldInfo.value(QStringLiteral("schema")).toObject();
    const QJsonObject sldVals = sldSchema.value(QStringLiteral("values")).toObject();
    EXPECT_EQ(sldVals.value(QStringLiteral("value")).toInt(), 120);
    EXPECT_EQ(sldVals.value(QStringLiteral("tickPosition")).toString(), QStringLiteral("TicksBothSides"));
    EXPECT_EQ(sldVals.value(QStringLiteral("handleSize")).toInt(), 22);

    auto* btn = qobject_cast<fluent::basicinput::Button*>(canvas.widgetForNode(QStringLiteral("btn")));
    ASSERT_NE(btn, nullptr);
    EXPECT_FALSE(btn->property("_studioIconGlyph").toString().isEmpty());
    EXPECT_EQ(btn->fluentLayout(), fluent::basicinput::Button::IconBefore);
    EXPECT_EQ(btn->cornerRadii().left(), 12);
    EXPECT_NEAR(btn->contentOpacity(), 0.85, 0.01);
    EXPECT_TRUE(btn->isCheckable());
    EXPECT_TRUE(btn->isChecked());

    const QJsonObject btnInfo = canvas.inspectNode(QStringLiteral("btn"));
    EXPECT_TRUE(btnInfo.value(QStringLiteral("supported")).toBool());
    const QJsonObject btnSchema = btnInfo.value(QStringLiteral("schema")).toObject();
    EXPECT_EQ(btnSchema.value(QStringLiteral("values")).toObject().value(QStringLiteral("icon")).toString(),
        QStringLiteral("Save"));
    EXPECT_EQ(btnSchema.value(QStringLiteral("values")).toObject().value(QStringLiteral("cornerRadius")).toInt(), 12);
    EXPECT_EQ(btnSchema.value(QStringLiteral("effective")).toObject().value(QStringLiteral("cornerRadius")).toInt(),
        12);
    for (const QJsonValue& propVal : btnSchema.value(QStringLiteral("properties")).toArray()) {
        EXPECT_NE(propVal.toObject().value(QStringLiteral("id")).toString(), QStringLiteral("focusVisual"));
    }

    const QJsonObject infoSchema =
        canvas.inspectNode(QStringLiteral("info")).value(QStringLiteral("schema")).toObject();
    for (const QJsonValue& propVal : infoSchema.value(QStringLiteral("properties")).toArray()) {
        EXPECT_NE(propVal.toObject().value(QStringLiteral("id")).toString(), QStringLiteral("isOpen"));
    }

    const QJsonObject rootInfo = canvas.inspectNode(QStringLiteral("root"));
    EXPECT_FALSE(rootInfo.value(QStringLiteral("supported")).toBool());

    // Passing null cornerRadius clears the custom radius override back to theme following.
    const QJsonObject nullRadiusRoot = node(QStringLiteral("root"), QStringLiteral("VBox"), {},
        {node(QStringLiteral("btn"), QStringLiteral("Button"),
            {{QStringLiteral("text"), QStringLiteral("Theme")}, {QStringLiteral("cornerRadius"), QJsonValue::Null}})});
    ASSERT_TRUE(canvas.applyDocument(
        QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("root"), nullRadiusRoot}}));
    EXPECT_TRUE(canvas.inspectNode(QStringLiteral("btn"))
            .value(QStringLiteral("schema"))
            .toObject()
            .value(QStringLiteral("values"))
            .toObject()
            .value(QStringLiteral("cornerRadius"))
            .isNull());

    // Live drag preview mode updates the layout tree cleanly while skipping nodeInspected emissions.
    canvas.setSelectedNodeId(QStringLiteral("btn"));
    QApplication::processEvents();
    const int inspectedBeforePreview = inspectedSpy.count();
    const QJsonObject previewRoot = node(QStringLiteral("root"), QStringLiteral("VBox"), {},
        {
            node(QStringLiteral("btn"), QStringLiteral("Button"), {{QStringLiteral("text"), QStringLiteral("Theme")}}),
            node(QStringLiteral("drag_preview"), QStringLiteral("InfoBadge"), {{QStringLiteral("value"), 7}}),
        });
    ASSERT_TRUE(canvas.applyDocument(
        QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("root"), previewRoot}}, nullptr, false));
    QApplication::processEvents();
    EXPECT_EQ(inspectedSpy.count(), inspectedBeforePreview);
    EXPECT_NE(canvas.widgetForNode(QStringLiteral("drag_preview")), nullptr);
    const QJsonObject previewNodes = canvas.layoutSnapshot().value(QStringLiteral("nodes")).toObject();
    EXPECT_TRUE(
        previewNodes.value(QStringLiteral("drag_preview")).toObject().value(QStringLiteral("visible")).toBool());
}

TEST_F(GalleryStudioCanvasTest, PatchesSingleNodeInPlaceWithoutResettingSiblingState)
{
    GalleryStudioCanvas canvas;
    canvas.resize(760, 640);
    QSignalSpy inspectedSpy(&canvas, &GalleryStudioCanvas::nodeInspected);
    QSignalSpy eventSpy(&canvas, &GalleryStudioCanvas::interactionEvent);

    const QJsonObject root = node(QStringLiteral("root"), QStringLiteral("VBox"),
        {{QStringLiteral("spacing"), 12}, {QStringLiteral("padding"), 16}},
        {
            node(QStringLiteral("slider_scale"), QStringLiteral("Slider"),
                {{QStringLiteral("minimum"), 50}, {QStringLiteral("maximum"), 200}, {QStringLiteral("value"), 100}}),
            node(QStringLiteral("switch_motion"), QStringLiteral("ToggleSwitch"),
                {{QStringLiteral("isOn"), true}, {QStringLiteral("onContent"), QStringLiteral("Enabled")},
                    {QStringLiteral("offContent"), QStringLiteral("Reduced")}}),
            node(QStringLiteral("combo_density"), QStringLiteral("ComboBox"),
                {{QStringLiteral("items"),
                     QJsonArray{QStringLiteral("Compact"), QStringLiteral("Standard"),
                         QStringLiteral("Touch Friendly")}},
                    {QStringLiteral("currentIndex"), 1}}),
            node(QStringLiteral("chk_diag"), QStringLiteral("CheckBox"),
                {{QStringLiteral("text"), QStringLiteral("Diagnostics")}, {QStringLiteral("checked"), true}}),
        });

    ASSERT_TRUE(
        canvas.applyDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("root"), root}}));
    QApplication::processEvents();

    QPointer<fluent::basicinput::Slider> sliderBefore = qobject_cast<fluent::basicinput::Slider*>(
        canvas.widgetForNode(QStringLiteral("slider_scale")));
    QPointer<fluent::basicinput::ToggleSwitch> toggleBefore = qobject_cast<fluent::basicinput::ToggleSwitch*>(
        canvas.widgetForNode(QStringLiteral("switch_motion")));
    QPointer<fluent::basicinput::ComboBox> comboBefore = qobject_cast<fluent::basicinput::ComboBox*>(
        canvas.widgetForNode(QStringLiteral("combo_density")));
    QPointer<fluent::basicinput::CheckBox> checkBefore = qobject_cast<fluent::basicinput::CheckBox*>(
        canvas.widgetForNode(QStringLiteral("chk_diag")));
    ASSERT_NE(sliderBefore, nullptr);
    ASSERT_NE(toggleBefore, nullptr);
    ASSERT_NE(comboBefore, nullptr);
    ASSERT_NE(checkBefore, nullptr);

    // Simulate interactive state changes on sibling widgets (as in Play mode).
    toggleBefore->setIsOn(false);
    comboBefore->setCurrentIndex(2);
    checkBefore->setChecked(false);
    QApplication::processEvents();
    eventSpy.clear();

    // Select slider_scale and patch it in place -> siblings keep both widget identity and live state.
    canvas.setSelectedNodeId(QStringLiteral("slider_scale"));
    inspectedSpy.clear();

    ASSERT_TRUE(canvas.patchNodeProperties(QStringLiteral("slider_scale"),
        {{QStringLiteral("handleSize"), 24}, {QStringLiteral("trackHeight"), 6}}));
    QApplication::processEvents();

    EXPECT_EQ(canvas.widgetForNode(QStringLiteral("slider_scale")), sliderBefore.data());
    EXPECT_EQ(canvas.widgetForNode(QStringLiteral("switch_motion")), toggleBefore.data());
    EXPECT_EQ(canvas.widgetForNode(QStringLiteral("combo_density")), comboBefore.data());
    EXPECT_EQ(canvas.widgetForNode(QStringLiteral("chk_diag")), checkBefore.data());

    EXPECT_EQ(sliderBefore->handleSize(), 24);
    EXPECT_EQ(sliderBefore->trackHeight(), 6);
    EXPECT_FALSE(toggleBefore->isOn());
    EXPECT_EQ(comboBefore->currentIndex(), 2);
    EXPECT_FALSE(checkBefore->isChecked());
    EXPECT_GE(inspectedSpy.count(), 1);
    EXPECT_EQ(eventSpy.count(), 0);

    // Structural container keys must reject in-place patch and fall back to full document apply.
    EXPECT_FALSE(canvas.patchNodeProperties(QStringLiteral("root"), {{QStringLiteral("spacing"), 20}}));
}

TEST_F(GalleryStudioCanvasTest, RootAnchorBoxFillsCanvasAndPatchesChildAnchorsInPlace)
{
    GalleryStudioCanvas canvas;
    canvas.resize(860, 540);
    canvas.show();

    const QJsonObject root = node(QStringLiteral("root"), QStringLiteral("AnchorBox"),
        {{QStringLiteral("minHeight"), 540}, {QStringLiteral("padding"), 16}},
        {
            node(QStringLiteral("button_1"), QStringLiteral("Button"),
                {{QStringLiteral("text"), QStringLiteral("Button")},
                    {QStringLiteral("fluentStyle"), QStringLiteral("Accent")},
                    {QStringLiteral("anchorPosition"), QStringLiteral("Center")}, {QStringLiteral("anchorMargin"), 16},
                    {QStringLiteral("anchorOffsetX"), 0}, {QStringLiteral("anchorOffsetY"), 0}}),
            node(QStringLiteral("switch_1"), QStringLiteral("ToggleSwitch"),
                {{QStringLiteral("isOn"), false}, {QStringLiteral("anchorPosition"), QStringLiteral("TopLeft")},
                    {QStringLiteral("anchorMargin"), 16}}),
            node(QStringLiteral("card_1"), QStringLiteral("Card"),
                {{QStringLiteral("anchorPosition"), QStringLiteral("TopRight")}, {QStringLiteral("anchorMargin"), 16}},
                {}),
        });

    QString error;
    ASSERT_TRUE(
        canvas.applyDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("root"), root}}, &error))
        << error.toStdString();
    QApplication::processEvents();
    QTest::qWait(20);

    QWidget* rootWidget = canvas.widgetForNode(QStringLiteral("root"));
    QPointer<fluent::basicinput::Button> btnBefore = qobject_cast<fluent::basicinput::Button*>(
        canvas.widgetForNode(QStringLiteral("button_1")));
    QPointer<fluent::basicinput::ToggleSwitch> swBefore = qobject_cast<fluent::basicinput::ToggleSwitch*>(
        canvas.widgetForNode(QStringLiteral("switch_1")));
    QWidget* cardWidget = canvas.widgetForNode(QStringLiteral("card_1"));
    ASSERT_NE(rootWidget, nullptr);
    ASSERT_NE(qobject_cast<fluent::AnchorLayout*>(rootWidget->layout()), nullptr);
    ASSERT_NE(btnBefore, nullptr);
    ASSERT_NE(swBefore, nullptr);
    ASSERT_NE(cardWidget, nullptr);

    auto rectOf = [](const QJsonObject& nodes, const QString& id) {
        const QJsonObject obj = nodes.value(id).toObject();
        return QRect(obj.value(QStringLiteral("x")).toInt(), obj.value(QStringLiteral("y")).toInt(),
            obj.value(QStringLiteral("w")).toInt(), obj.value(QStringLiteral("h")).toInt());
    };

    const QJsonObject nodesBefore = canvas.layoutSnapshot().value(QStringLiteral("nodes")).toObject();
    const QRect rootRect = rectOf(nodesBefore, QStringLiteral("root"));
    const QRect btnRect = rectOf(nodesBefore, QStringLiteral("button_1"));
    const QRect cardRect = rectOf(nodesBefore, QStringLiteral("card_1"));
    EXPECT_GE(rootRect.height(), 520);
    EXPECT_LE(qAbs((btnRect.x() + btnRect.width() / 2) - (rootRect.x() + rootRect.width() / 2)), 6);
    EXPECT_LE(qAbs((btnRect.y() + btnRect.height() / 2) - (rootRect.y() + rootRect.height() / 2)), 6);
    EXPECT_GE(cardRect.width(), 200);
    EXPECT_GE(cardRect.height(), 80);

    swBefore->setIsOn(true);
    QApplication::processEvents();

    ASSERT_TRUE(canvas.patchNodeProperties(QStringLiteral("button_1"),
        {{QStringLiteral("anchorPosition"), QStringLiteral("BottomRight")}, {QStringLiteral("anchorMargin"), 20},
            {QStringLiteral("anchorOffsetX"), -12}, {QStringLiteral("anchorOffsetY"), -8}}));
    QApplication::processEvents();

    EXPECT_EQ(canvas.widgetForNode(QStringLiteral("button_1")), btnBefore.data());
    EXPECT_EQ(canvas.widgetForNode(QStringLiteral("switch_1")), swBefore.data());
    EXPECT_TRUE(swBefore->isOn());

    const QJsonObject nodesAfter = canvas.layoutSnapshot().value(QStringLiteral("nodes")).toObject();
    const QRect movedRect = rectOf(nodesAfter, QStringLiteral("button_1"));
    EXPECT_GT(movedRect.x(), 600);
    EXPECT_GT(movedRect.y(), 400);

    ASSERT_TRUE(canvas.patchNodeProperties(QStringLiteral("root"), {{QStringLiteral("minHeight"), 680}}));
    QApplication::processEvents();
    EXPECT_EQ(rootWidget->minimumHeight(), 680);
    EXPECT_EQ(canvas.widgetForNode(QStringLiteral("switch_1")), swBefore.data());
    EXPECT_TRUE(swBefore->isOn());
}

TEST_F(GalleryStudioCanvasTest, PolishesSplitViewTreeViewGridViewCommandBarAndHeaderHBox)
{
    GalleryStudioCanvas canvas;
    canvas.resize(860, 720);
    canvas.show();

    const QJsonObject doc{{QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("root"),
            node(QStringLiteral("root"), QStringLiteral("VBox"),
                {{QStringLiteral("spacing"), 10}, {QStringLiteral("padding"), 14}},
                {
                    node(QStringLiteral("hdr"), QStringLiteral("HBox"),
                        {{QStringLiteral("spacing"), 10}, {QStringLiteral("align"), QStringLiteral("Center")}},
                        {
                            node(QStringLiteral("hdr_ico"), QStringLiteral("FontIcon"),
                                {{QStringLiteral("glyph"), QStringLiteral("Home")}}),
                            node(QStringLiteral("hdr_title"), QStringLiteral("Label"),
                                {{QStringLiteral("text"), QStringLiteral("Photos")},
                                    {QStringLiteral("typography"), QStringLiteral("Title")}}),
                            node(QStringLiteral("hdr_search"), QStringLiteral("AutoSuggestBox"),
                                {{QStringLiteral("placeholderText"), QStringLiteral("Search photos...")}}),
                            node(QStringLiteral("hdr_cta"), QStringLiteral("Button"),
                                {{QStringLiteral("text"), QStringLiteral("Import")},
                                    {QStringLiteral("fluentStyle"), QStringLiteral("Accent")}}),
                        }),
                    node(QStringLiteral("cmd"), QStringLiteral("CommandBar"),
                        {{QStringLiteral("items"),
                            QJsonArray{QStringLiteral("New"), QStringLiteral("Favorite"), QStringLiteral("Sort")}}}),
                    node(QStringLiteral("split"), QStringLiteral("SplitView"),
                        {{QStringLiteral("orientation"), QStringLiteral("Horizontal")},
                            {QStringLiteral("minHeight"), 360}},
                        {
                            node(QStringLiteral("albums_card"), QStringLiteral("Card"), {},
                                {node(QStringLiteral("albums_tree"), QStringLiteral("TreeView"),
                                    {{QStringLiteral("items"),
                                        QJsonArray{QStringLiteral("All Photos"), QStringLiteral("Favorites"),
                                            QStringLiteral("  2026"), QStringLiteral("Travel")}}})}),
                            node(QStringLiteral("gallery"), QStringLiteral("GridView"),
                                {{QStringLiteral("headerText"), QStringLiteral("2026")},
                                    {QStringLiteral("items"),
                                        QJsonArray{QStringLiteral("sunset.jpg"), QStringLiteral("trail.jpg"),
                                            QStringLiteral("lake.jpg"), QStringLiteral("coffee.jpg")}}}),
                            node(QStringLiteral("preview_card"), QStringLiteral("Card"), {},
                                {node(QStringLiteral("viewer"), QStringLiteral("ImageViewer"), {})}),
                        }),
                    node(QStringLiteral("grid_table"), QStringLiteral("DataGrid"), {}),
                })}};

    QString error;
    ASSERT_TRUE(canvas.applyDocument(doc, &error)) << error.toStdString();
    QApplication::processEvents();
    QTest::qWait(20);

    const QJsonObject nodes = canvas.layoutSnapshot().value(QStringLiteral("nodes")).toObject();
    auto rectOf = [&nodes](const QString& id) {
        const QJsonObject obj = nodes.value(id).toObject();
        return QRect(obj.value(QStringLiteral("x")).toInt(), obj.value(QStringLiteral("y")).toInt(),
            obj.value(QStringLiteral("w")).toInt(), obj.value(QStringLiteral("h")).toInt());
    };

    // Header HBox pushes trailing search + CTA toward the right edge instead of jamming everything left.
    const QRect hdrRect = rectOf(QStringLiteral("hdr"));
    const QRect titleRect = rectOf(QStringLiteral("hdr_title"));
    const QRect ctaRect = rectOf(QStringLiteral("hdr_cta"));
    EXPECT_GT(ctaRect.x(), titleRect.right() + 100);
    EXPECT_GE(ctaRect.right(), hdrRect.right() - 24);

    // 3-pane SplitView gives middle GridView fill width instead of squishing to a narrow strip.
    const QRect albumsRect = rectOf(QStringLiteral("albums_card"));
    const QRect galleryRect = rectOf(QStringLiteral("gallery"));
    const QRect previewRect = rectOf(QStringLiteral("preview_card"));
    EXPECT_GE(galleryRect.width(), 240);
    EXPECT_GT(galleryRect.width(), albumsRect.width());
    EXPECT_GE(previewRect.width(), 190);

    // CommandBar primary actions receive themed icons.
    auto* cmdBar = qobject_cast<fluent::menus_toolbars::CommandBar*>(canvas.widgetForNode(QStringLiteral("cmd")));
    ASSERT_NE(cmdBar, nullptr);
    ASSERT_EQ(cmdBar->primaryActions().size(), 3);
    for (QAction* act : cmdBar->primaryActions()) {
        ASSERT_NE(act, nullptr);
        EXPECT_FALSE(act->icon().isNull());
    }

    // GridView uses custom visual card delegate & larger cell dimensions.
    auto* gridView = qobject_cast<fluent::collections::GridView*>(canvas.widgetForNode(QStringLiteral("gallery")));
    ASSERT_NE(gridView, nullptr);
    EXPECT_GE(gridView->cellSize().width(), 130);
    EXPECT_GE(gridView->cellSize().height(), 100);
    EXPECT_NE(gridView->itemDelegate(), nullptr);

    // TreeView uses items directly at root (no hardcoded "Workspace" wrapper) and supports nested sub-items.
    auto* treeView = qobject_cast<fluent::collections::TreeView*>(canvas.widgetForNode(QStringLiteral("albums_tree")));
    ASSERT_NE(treeView, nullptr);
    ASSERT_NE(treeView->model(), nullptr);
    EXPECT_EQ(treeView->model()->rowCount(), 3);
    EXPECT_EQ(treeView->model()->index(0, 0).data().toString(), QStringLiteral("All Photos"));
    const QModelIndex favIdx = treeView->model()->index(1, 0);
    EXPECT_EQ(favIdx.data().toString(), QStringLiteral("Favorites"));
    EXPECT_EQ(treeView->model()->rowCount(favIdx), 1);
    EXPECT_EQ(treeView->model()->index(0, 0, favIdx).data().toString(), QStringLiteral("2026"));

    // DataGrid stretches last header section so table fills available card width.
    auto* dataGrid = qobject_cast<fluent::collections::DataGrid*>(canvas.widgetForNode(QStringLiteral("grid_table")));
    ASSERT_NE(dataGrid, nullptr);
    ASSERT_NE(dataGrid->horizontalHeader(), nullptr);
    EXPECT_TRUE(dataGrid->horizontalHeader()->stretchLastSection());

    // NavigationView selects page 0 immediately so nested page widgets are visible and sized without clipping.
    const QJsonObject navDoc{{QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("root"),
            node(QStringLiteral("root"), QStringLiteral("VBox"),
                {{QStringLiteral("spacing"), 12}, {QStringLiteral("padding"), 14}},
                {node(QStringLiteral("nav"), QStringLiteral("NavigationView"),
                    {{QStringLiteral("displayMode"), QStringLiteral("Left")},
                        {QStringLiteral("items"), QJsonArray{QStringLiteral("Overview"), QStringLiteral("Logs")}},
                        {QStringLiteral("minHeight"), 260}},
                    {node(QStringLiteral("page_0"), QStringLiteral("VBox"),
                        {{QStringLiteral("spacing"), 12}, {QStringLiteral("padding"), 12}},
                        {
                            node(QStringLiteral("page_title"), QStringLiteral("Label"),
                                {{QStringLiteral("text"), QStringLiteral("Telemetry")},
                                    {QStringLiteral("typography"), QStringLiteral("Title")}}),
                            node(QStringLiteral("page_chart"), QStringLiteral("BarChart"),
                                {{QStringLiteral("minHeight"), 200}}),
                            node(QStringLiteral("page_grid"), QStringLiteral("DataGrid"),
                                {{QStringLiteral("minHeight"), 180}}),
                        })})})}};
    ASSERT_TRUE(canvas.applyDocument(navDoc, &error)) << error.toStdString();
    QApplication::processEvents();
    QTest::qWait(20);

    auto* navWidget = qobject_cast<fluent::navigation::NavigationView*>(canvas.widgetForNode(QStringLiteral("nav")));
    ASSERT_NE(navWidget, nullptr);
    ASSERT_NE(navWidget->contentHost(), nullptr);
    EXPECT_EQ(navWidget->contentHost()->currentIndex(), 0);
    const QJsonObject navNodes = canvas.layoutSnapshot().value(QStringLiteral("nodes")).toObject();
    const QJsonObject p0 = navNodes.value(QStringLiteral("page_0")).toObject();
    const QJsonObject pg = navNodes.value(QStringLiteral("page_grid")).toObject();
    EXPECT_TRUE(p0.value(QStringLiteral("visible")).toBool());
    EXPECT_TRUE(pg.value(QStringLiteral("visible")).toBool());
    EXPECT_GT(pg.value(QStringLiteral("w")).toInt(), 200);
    EXPECT_GE(navNodes.value(QStringLiteral("nav")).toObject().value(QStringLiteral("h")).toInt(), 420);
}

TEST_F(GalleryStudioCanvasTest, AppliesDesignAccentIconsRichMediaAndCustomDataGrid)
{
    GalleryStudioCanvas canvas;
    canvas.resize(1024, 720);

    const QString validSvg = QStringLiteral(
        "<svg viewBox=\"0 0 320 180\" xmlns=\"http://www.w3.org/2000/svg\">"
        "<rect width=\"320\" height=\"180\" rx=\"12\" fill=\"#2B1E4A\"/>"
        "<circle cx=\"160\" cy=\"90\" r=\"52\" fill=\"#8764B8\"/></svg>");
    const QString xssSvg = QStringLiteral(
        "<svg viewBox=\"0 0 100 100\" xmlns=\"http://www.w3.org/2000/svg\">"
        "<script>alert(1)</script><rect width=\"100\" height=\"100\" fill=\"red\"/></svg>");

    const QJsonObject richDoc{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("design"),
            QJsonObject{{QStringLiteral("theme"), QStringLiteral("dark")},
                {QStringLiteral("accentColor"), QStringLiteral("#D84A38")}}},
        {QStringLiteral("root"),
            node(QStringLiteral("root"), QStringLiteral("VBox"),
                {{QStringLiteral("spacing"), 12}, {QStringLiteral("padding"), 14}},
                {
                    node(QStringLiteral("nav"), QStringLiteral("NavigationView"),
                        {{QStringLiteral("displayMode"), QStringLiteral("Left")},
                            {QStringLiteral("items"),
                                QJsonArray{QStringLiteral("Grid|Overview"), QStringLiteral("Camera|Photos")}},
                            {QStringLiteral("minHeight"), 260}},
                        {
                            node(QStringLiteral("page_0"), QStringLiteral("VBox"),
                                {{QStringLiteral("spacing"), 10}, {QStringLiteral("padding"), 10}},
                                {
                                    node(QStringLiteral("cmd"), QStringLiteral("CommandBar"),
                                        {{QStringLiteral("items"),
                                            QJsonArray{QStringLiteral("Camera|Shot"), QStringLiteral("Music|Mix")}}}),
                                    node(QStringLiteral("btn_cam"), QStringLiteral("Button"),
                                        {{QStringLiteral("text"), QStringLiteral("Capture")},
                                            {QStringLiteral("icon"), QStringLiteral("Camera")}}),
                                    node(QStringLiteral("btn_mus"), QStringLiteral("Button"),
                                        {{QStringLiteral("text"), QStringLiteral("Play")},
                                            {QStringLiteral("icon"), QStringLiteral("Music")}}),
                                    node(QStringLiteral("fi_heart"), QStringLiteral("FontIcon"),
                                        {{QStringLiteral("glyph"), QStringLiteral("Heart")}}),
                                    node(QStringLiteral("tiles"), QStringLiteral("GridView"),
                                        {{QStringLiteral("mediaTheme"), QStringLiteral("Landscape")},
                                            {QStringLiteral("items"),
                                                QJsonArray{QStringLiteral("Sunset | RAW 4K"),
                                                    QStringLiteral("Alpine | 35mm")}}}),
                                    node(QStringLiteral("grid"), QStringLiteral("DataGrid"),
                                        {{QStringLiteral("headers"),
                                             QJsonArray{QStringLiteral("Node"), QStringLiteral("Load"),
                                                 QStringLiteral("Latency"), QStringLiteral("State")}},
                                            {QStringLiteral("modelData"),
                                                QJsonArray{QJsonArray{QStringLiteral("gpu-01"), QStringLiteral("72%"),
                                                               QStringLiteral("32ms"), QStringLiteral("Online")},
                                                    QJsonArray{QStringLiteral("gpu-02"), QStringLiteral("91%"),
                                                        QStringLiteral("84ms"), QStringLiteral("Warning")}}}}),
                                    node(QStringLiteral("img_svg"), QStringLiteral("ImageViewer"),
                                        {{QStringLiteral("svg"), validSvg}}),
                                    node(QStringLiteral("img_xss"), QStringLiteral("ImageViewer"),
                                        {{QStringLiteral("svg"), xssSvg},
                                            {QStringLiteral("mediaTheme"), QStringLiteral("Music")}}),
                                }),
                            node(QStringLiteral("page_1"), QStringLiteral("VBox"), {},
                                {node(QStringLiteral("p1_lbl"), QStringLiteral("Label"),
                                    {{QStringLiteral("text"), QStringLiteral("Second Page")}})}),
                        }),
                })},
    };

    QString error;
    ASSERT_TRUE(canvas.applyDocument(richDoc, &error)) << error.toStdString();
    QApplication::processEvents();
    QTest::qWait(20);

    EXPECT_EQ(canvas.theme(), FluentElement::Dark);
    EXPECT_EQ(fluent::ThemeRegistry::instance().colors(FluentElement::Dark).accentDefault,
        QColor(QStringLiteral("#D84A38")));

    auto* nav = qobject_cast<fluent::navigation::NavigationView*>(canvas.widgetForNode(QStringLiteral("nav")));
    ASSERT_NE(nav, nullptr);
    ASSERT_NE(nav->contentHost(), nullptr);
    QAbstractButton* photosTab = nullptr;
    QAbstractButton* overviewTab = nullptr;
    for (QAbstractButton* btn : nav->findChildren<QAbstractButton*>()) {
        if (btn && btn->text() == QLatin1String("Overview"))
            overviewTab = btn;
        if (btn && btn->text() == QLatin1String("Photos"))
            photosTab = btn;
    }
    ASSERT_NE(overviewTab, nullptr);
    ASSERT_NE(photosTab, nullptr);
    EXPECT_TRUE(overviewTab->isChecked());
    photosTab->click();
    QApplication::processEvents();
    EXPECT_EQ(nav->contentHost()->currentIndex(), 1);
    EXPECT_TRUE(photosTab->isChecked());

    auto* cmdBar = qobject_cast<fluent::menus_toolbars::CommandBar*>(canvas.widgetForNode(QStringLiteral("cmd")));
    ASSERT_NE(cmdBar, nullptr);
    ASSERT_EQ(cmdBar->primaryActions().size(), 2);
    EXPECT_EQ(cmdBar->primaryActions().at(0)->text(), QStringLiteral("Shot"));
    EXPECT_EQ(cmdBar->primaryActions().at(1)->text(), QStringLiteral("Mix"));
    EXPECT_FALSE(cmdBar->primaryActions().at(0)->icon().isNull());

    auto* btnCam = qobject_cast<fluent::basicinput::Button*>(canvas.widgetForNode(QStringLiteral("btn_cam")));
    auto* btnMus = qobject_cast<fluent::basicinput::Button*>(canvas.widgetForNode(QStringLiteral("btn_mus")));
    auto* fiHeart = qobject_cast<fluent::FontIcon*>(canvas.widgetForNode(QStringLiteral("fi_heart")));
    ASSERT_NE(btnCam, nullptr);
    ASSERT_NE(btnMus, nullptr);
    ASSERT_NE(fiHeart, nullptr);
    EXPECT_EQ(btnCam->property("_studioIconGlyph").toString(), Typography::Icons::Camera);
    EXPECT_EQ(btnMus->property("_studioIconGlyph").toString(), Typography::Icons::Music);
    EXPECT_EQ(fiHeart->glyph(), Typography::Icons::Heart);

    auto* tiles = qobject_cast<fluent::collections::GridView*>(canvas.widgetForNode(QStringLiteral("tiles")));
    ASSERT_NE(tiles, nullptr);
    ASSERT_NE(tiles->model(), nullptr);
    const QPixmap tilePix = tiles->model()->index(0, 0).data(Qt::DecorationRole).value<QPixmap>();
    EXPECT_FALSE(tilePix.isNull());

    auto* dataGrid = qobject_cast<fluent::collections::DataGrid*>(canvas.widgetForNode(QStringLiteral("grid")));
    ASSERT_NE(dataGrid, nullptr);
    ASSERT_NE(dataGrid->model(), nullptr);
    EXPECT_EQ(dataGrid->model()->columnCount(), 4);
    EXPECT_EQ(dataGrid->model()->rowCount(), 2);
    EXPECT_EQ(dataGrid->model()->headerData(0, Qt::Horizontal).toString(), QStringLiteral("Node"));
    EXPECT_EQ(dataGrid->model()->index(1, 3).data().toString(), QStringLiteral("Warning"));

    auto* viewerSvg = qobject_cast<fluent::scrolling::ImageViewer*>(canvas.widgetForNode(QStringLiteral("img_svg")));
    auto* viewerXss = qobject_cast<fluent::scrolling::ImageViewer*>(canvas.widgetForNode(QStringLiteral("img_xss")));
    ASSERT_NE(viewerSvg, nullptr);
    ASSERT_NE(viewerXss, nullptr);
    EXPECT_FALSE(viewerSvg->pixmap().isNull());
    EXPECT_FALSE(viewerXss->pixmap().isNull());

    // Applying a document without design resets the custom accent override cleanly.
    const QJsonObject plainDoc{{QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("root"),
            node(QStringLiteral("root"), QStringLiteral("VBox"), {},
                {node(QStringLiteral("plain_lbl"), QStringLiteral("Label"),
                    {{QStringLiteral("text"), QStringLiteral("Reset")}})})}};
    ASSERT_TRUE(canvas.applyDocument(plainDoc, &error)) << error.toStdString();
    EXPECT_EQ(fluent::ThemeRegistry::instance().colors(FluentElement::Light).accentDefault,
        fluent::UserTheme::defaultAccent(false));

    fluent::ThemeRegistry::instance().resetToDefaults();
    FluentElement::setTheme(FluentElement::Light);
}

TEST_F(GalleryStudioCanvasTest, ListDetailViewAutoHeightsWithoutOverlapAndNeverRendersAsciiIconText)
{
    GalleryStudioCanvas canvas;
    canvas.resize(860, 720);
    canvas.show();

    const QJsonObject agentDoc{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("design"),
            QJsonObject{{QStringLiteral("theme"), QStringLiteral("dark")},
                {QStringLiteral("accentColor"), QStringLiteral("#00B7C3")}}},
        {QStringLiteral("root"),
            node(QStringLiteral("root"), QStringLiteral("VBox"),
                {{QStringLiteral("spacing"), 10}, {QStringLiteral("padding"), 14}},
                {
                    node(QStringLiteral("hdr"), QStringLiteral("HBox"),
                        {{QStringLiteral("spacing"), 10}, {QStringLiteral("align"), QStringLiteral("Center")}},
                        {
                            node(QStringLiteral("hdr_prism"), QStringLiteral("FontIcon"),
                                {{QStringLiteral("glyph"), QStringLiteral("Prism")}}),
                            node(QStringLiteral("hdr_spark"), QStringLiteral("FontIcon"),
                                {{QStringLiteral("glyph"), QStringLiteral("Sparkles")}}),
                            node(QStringLiteral("hdr_unk"), QStringLiteral("FontIcon"),
                                {{QStringLiteral("glyph"), QStringLiteral("UnknownRobotGlyph")}}),
                            node(QStringLiteral("hdr_title"), QStringLiteral("Label"),
                                {{QStringLiteral("text"), QStringLiteral("AI Agent Workbench")},
                                    {QStringLiteral("typography"), QStringLiteral("Title")}}),
                        }),
                    node(QStringLiteral("workspace"), QStringLiteral("ListDetailView"),
                        {{QStringLiteral("listPaneWidth"), 220}, {QStringLiteral("minHeight"), 240}},
                        {
                            node(QStringLiteral("left_pane"), QStringLiteral("VBox"),
                                {{QStringLiteral("spacing"), 8}, {QStringLiteral("padding"), 8}},
                                {
                                    node(QStringLiteral("new_chat"), QStringLiteral("Button"),
                                        {{QStringLiteral("text"), QStringLiteral("New Chat")},
                                            {QStringLiteral("icon"), QStringLiteral("Add")}}),
                                    node(QStringLiteral("hist_list"), QStringLiteral("ListView"),
                                        {{QStringLiteral("items"),
                                            QJsonArray{QStringLiteral("Weekly Report"),
                                                QStringLiteral("Feedback Summary"), QStringLiteral("Release Draft")}}}),
                                }),
                            node(QStringLiteral("right_pane"), QStringLiteral("VBox"),
                                {{QStringLiteral("spacing"), 10}, {QStringLiteral("padding"), 8}},
                                {
                                    node(QStringLiteral("assistant_bar"), QStringLiteral("HBox"),
                                        {{QStringLiteral("spacing"), 8},
                                            {QStringLiteral("align"), QStringLiteral("Center")}},
                                        {
                                            node(QStringLiteral("av"), QStringLiteral("Avatar"),
                                                {{QStringLiteral("name"), QStringLiteral("AI")}}),
                                            node(QStringLiteral("asst_lbl"), QStringLiteral("Label"),
                                                {{QStringLiteral("text"), QStringLiteral("Assistant")},
                                                    {QStringLiteral("typography"), QStringLiteral("Subtitle")}}),
                                        }),
                                    node(QStringLiteral("msg_scroll"), QStringLiteral("ScrollView"),
                                        {{QStringLiteral("minHeight"), 180}},
                                        {
                                            node(QStringLiteral("user_msg"), QStringLiteral("Card"),
                                                {{QStringLiteral("padding"), 12}, {QStringLiteral("spacing"), 6}},
                                                {
                                                    node(QStringLiteral("user_hdr"), QStringLiteral("Label"),
                                                        {{QStringLiteral("text"), QStringLiteral("You")},
                                                            {QStringLiteral("typography"), QStringLiteral("Caption")}}),
                                                    node(QStringLiteral("user_txt"), QStringLiteral("Label"),
                                                        {{QStringLiteral("text"),
                                                            QStringLiteral("Summarize progress and next steps.")}}),
                                                }),
                                            node(QStringLiteral("bot_msg"), QStringLiteral("Card"),
                                                {{QStringLiteral("padding"), 12}, {QStringLiteral("spacing"), 6}},
                                                {
                                                    node(QStringLiteral("bot_hdr"), QStringLiteral("Label"),
                                                        {{QStringLiteral("text"), QStringLiteral("Assistant")},
                                                            {QStringLiteral("typography"), QStringLiteral("Caption")}}),
                                                    node(QStringLiteral("bot_txt"), QStringLiteral("Label"),
                                                        {{QStringLiteral("text"),
                                                            QStringLiteral(
                                                                "Drafting weekly report with key milestones.")}}),
                                                }),
                                        }),
                                    node(QStringLiteral("composer_card"), QStringLiteral("Card"),
                                        {{QStringLiteral("padding"), 12}, {QStringLiteral("spacing"), 8}},
                                        {
                                            node(QStringLiteral("composer_edit"), QStringLiteral("TextEdit"),
                                                {{QStringLiteral("placeholderText"),
                                                     QStringLiteral("Ask or attach files...")},
                                                    {QStringLiteral("minVisibleLines"), 4}}),
                                            node(QStringLiteral("composer_actions"), QStringLiteral("HBox"),
                                                {{QStringLiteral("spacing"), 8},
                                                    {QStringLiteral("align"), QStringLiteral("Start")}},
                                                {
                                                    node(QStringLiteral("attach_btn"), QStringLiteral("Button"),
                                                        {{QStringLiteral("text"), QStringLiteral("Attach")},
                                                            {QStringLiteral("icon"), QStringLiteral("Attach")}}),
                                                    node(QStringLiteral("send_btn"), QStringLiteral("Button"),
                                                        {{QStringLiteral("text"), QStringLiteral("Send")},
                                                            {QStringLiteral("icon"), QStringLiteral("Send")},
                                                            {QStringLiteral("fluentStyle"), QStringLiteral("Accent")}}),
                                                }),
                                        }),
                                    node(QStringLiteral("disclaimer"), QStringLiteral("Label"),
                                        {{QStringLiteral("text"), QStringLiteral("AI outputs may contain mistakes.")},
                                            {QStringLiteral("typography"), QStringLiteral("Caption")}}),
                                }),
                        }),
                })},
    };

    QString error;
    ASSERT_TRUE(canvas.applyDocument(agentDoc, &error)) << error.toStdString();
    QApplication::processEvents();
    QTest::qWait(30);

    for (const char* iconId : {"hdr_prism", "hdr_spark", "hdr_unk"}) {
        auto* icon = qobject_cast<fluent::FontIcon*>(canvas.widgetForNode(QString::fromLatin1(iconId)));
        ASSERT_NE(icon, nullptr);
        EXPECT_FALSE(icon->glyph().isEmpty());
        EXPECT_LE(icon->glyph().size(), 2);
        EXPECT_GE(icon->glyph().at(0).unicode(), 0x80);
    }

    auto* attachBtn = qobject_cast<fluent::basicinput::Button*>(canvas.widgetForNode(QStringLiteral("attach_btn")));
    ASSERT_NE(attachBtn, nullptr);
    EXPECT_EQ(attachBtn->property("_studioIconGlyph").toString(), Typography::Icons::Link);

    const QJsonObject nodes = canvas.layoutSnapshot().value(QStringLiteral("nodes")).toObject();
    auto rectOf = [&nodes](const QString& id) {
        const QJsonObject obj = nodes.value(id).toObject();
        return QRect(obj.value(QStringLiteral("x")).toInt(), obj.value(QStringLiteral("y")).toInt(),
            obj.value(QStringLiteral("w")).toInt(), obj.value(QStringLiteral("h")).toInt());
    };

    const QRect ldvRect = rectOf(QStringLiteral("workspace"));
    const QRect scrollRect = rectOf(QStringLiteral("msg_scroll"));
    const QRect composerRect = rectOf(QStringLiteral("composer_card"));
    const QRect disclaimerRect = rectOf(QStringLiteral("disclaimer"));

    EXPECT_GT(ldvRect.height(), 420);
    EXPECT_GE(scrollRect.height(), 170);
    EXPECT_GE(composerRect.top(), scrollRect.bottom() - 2);
    EXPECT_GE(disclaimerRect.top(), composerRect.bottom() - 2);
    EXPECT_LE(composerRect.right(), ldvRect.right() + 2);
    EXPECT_LE(disclaimerRect.bottom(), ldvRect.bottom() + 2);

    fluent::ThemeRegistry::instance().resetToDefaults();
    FluentElement::setTheme(FluentElement::Light);
}

} // namespace

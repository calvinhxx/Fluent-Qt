#include "SpatialSamples.h"
#include "SampleBuilders.h"
#include "CollectionSampleDelegates.h"
#include <FluentQt/FluentQt.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStringListModel>
#include <QStandardItemModel>
#include <QTimer>
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {
namespace {
using samples::makeSample;

// Gallery owns one mode; SpatialView owns rendering and visibility adaptation.
// zh_CN: Gallery 统一管理模式，SpatialView 自行适配渲染后端和可见性。
class SpatialPreviewBinding final : public QObject {
public:
    explicit SpatialPreviewBinding(QWidget* panel) : QObject(panel)
    {
        auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
        // Maintenance previews use an explicit local state without loading user preferences.
        // zh_CN: 维护预览使用显式局部状态，不加载用户偏好。
        if (panel->window()->property("galleryPreviewRouteId").isValid()) {
            view->setSpatialEnabled(false);
            return;
        }
        auto& settings = GallerySettings::instance();
        const auto synchronize = [this, view, &settings] {
            m_synchronizing = true;
            view->setSpatialEnabled(settings.spatialAvailable() && settings.spatialModeEnabled());
            m_synchronizing = false;
        };
        connect(&settings, &GallerySettings::spatialModeEnabledChanged, this, synchronize);
        connect(&settings, &GallerySettings::spatialAvailabilityChanged, this, synchronize);
        connect(view, &spatial::SpatialView::spatialEnabledChanged, this,
                [this, &settings](bool enabled) {
                    // Tab/Escape can restore native input. Keep the whole Gallery in that mode.
                    // zh_CN: Tab/Escape 可恢复原生输入，Gallery 同步回到同一模式。
                    if (!m_synchronizing && !enabled)
                        settings.setSpatialModeEnabled(false);
                });
        synchronize();
    }

private:
    bool m_synchronizing = false;
};

// Show the small Spatial integration first; keep the complete preview source available.
// zh_CN: 优先展示简短的 Spatial 接入代码，完整预览源码仍可按需查看。
QString spatialUsage(const QString& id)
{
    if (id == QStringLiteral("spatial-view-scene"))
        return QStringLiteral(
            "using namespace fluent;\n"
            "// view already contains two cards; distance, zoom and follow are toolbar controls.\n"
            "// Full example includes card creation and responsive positioning.\n"
            "view->setCameraDistance(1200);\n"
            "view->setZoom(0.9);\n"
            "view->setMaximumTilt(QPointF(6, 12));\n"
            "QObject::connect(distance, &basicinput::Slider::valueChanged, view,\n"
            "                 [view](int value) { view->setCameraDistance(value); });\n"
            "QObject::connect(zoom, &basicinput::Slider::valueChanged, view,\n"
            "                 [view](int value) { view->setZoom(value / 100.0); });\n"
            "QObject::connect(follow, &basicinput::ToggleSwitch::toggled,\n"
            "                 view, &spatial::SpatialView::setPointerTrackingEnabled);\n");
    if (id == QStringLiteral("spatial-view-cards"))
        return QStringLiteral(
            "using namespace fluent;\n"
            "// settings and summary are existing cards; view is their SpatialView host.\n"
            "// Full example builds both cards with ordinary Fluent controls.\n"
            "auto* back = view->addWidget(settings, WidgetOwnership::Owned);\n"
            "auto* front = view->addWidget(summary, WidgetOwnership::Owned);\n"
            "back->setPosition(QVector3D(-110, -64, -80));\n"
            "front->setPosition(QVector3D(110, 64, 100));\n"
            "back->setRotation(QVector3D(6, 12, 0));\n"
            "front->setRotation(QVector3D(-6, -14, 0));\n"
            "back->setSurfaceIntensity(0.75);\n"
            "front->setSurfaceIntensity(0.75);\n");
    if (id == QStringLiteral("spatial-item-pose"))
        return QStringLiteral(
            "using namespace fluent;\n"
            "// card, settings and their sliders are built in Full example.\n"
            "// Each card gets its own transform inside the same view.\n"
            "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
            "item->setRotation(QVector3D(0, -18, 0));\n"
            "item->setSurfaceIntensity(0.75);\n"
            "item->setHoverLift(5);\n"
            "auto* controlItem = view->addWidget(settings, WidgetOwnership::Owned);\n"
            "controlItem->setRotation(QVector3D(0, -6, 0));\n"
            "controlItem->setSurfaceIntensity(0.75);\n"
            "QObject::connect(finish, &basicinput::Slider::valueChanged, item,\n"
            "                 [item](int value) { item->setSurfaceIntensity(value / 100.0); });\n"
            "QObject::connect(angle, &basicinput::Slider::valueChanged, item,\n"
            "                 [item](int value) { item->setRotation(QVector3D(0, value, 0)); });\n"
            "QObject::connect(depth, &basicinput::Slider::valueChanged, item, [item](int value) {\n"
            "    auto position = item->position();\n"
            "    position.setZ(value);\n"
            "    item->setPosition(position);\n"
            "});\n");
    if (id == QStringLiteral("spatial-view-tree"))
        return QStringLiteral("using namespace fluent;\n"
                              "// card and the surrounding layout are built in Full example.\n"
                              "auto* view = new spatial::SpatialView(panel);\n"
                              "column->addWidget(view);\n"
                              "view->setCameraDistance(1400);\n"
                              "view->setMaximumTilt(QPointF(4, 6));\n"
                              "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                              "item->setRotation(QVector3D(5, -12, 0));\n"
                              "item->setSurfaceIntensity(0.75);\n"
                              "item->setHoverLift(4.5);\n"
                              "// The TreeView keeps its model, row delegate and selection.\n");
    if (id == QStringLiteral("spatial-view-hybrid"))
        return QStringLiteral(
            "using namespace fluent;\n"
            "// card and the surrounding layout are built in Full example.\n"
            "auto* view = new spatial::SpatialView(panel);\n"
            "column->addWidget(view);\n"
            "view->setCameraDistance(1400);\n"
            "view->setMaximumTilt(QPointF(4, 6));\n"
            "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
            "item->setRotation(QVector3D(5, -12, 0));\n"
            "item->setSurfaceIntensity(0.75);\n"
            "item->setHoverLift(4.5);\n"
            "// formats stays outside view; its signal updates the labels inside card.\n");
    if (id == QStringLiteral("spatial-view-navigation"))
        return QStringLiteral(
            "using namespace fluent;\n"
            "// card and the surrounding layout are built in Full example.\n"
            "auto* view = new spatial::SpatialView(panel);\n"
            "column->addWidget(view);\n"
            "view->setCameraDistance(1400);\n"
            "view->setMaximumTilt(QPointF(4, 6));\n"
            "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
            "item->setRotation(QVector3D(5, -12, 0));\n"
            "item->setSurfaceIntensity(0.75);\n"
            "item->setHoverLift(4.5);\n"
            "// The LineEdit stays outside view; its textChanged signal updates card.\n");
    return QStringLiteral("using namespace fluent;\n"
                          "// card and the surrounding layout are built in Full example.\n"
                          "auto* view = new spatial::SpatialView(panel);\n"
                          "column->addWidget(view);\n"
                          "view->setCameraDistance(1400);\n"
                          "view->setMaximumTilt(QPointF(4, 6));\n"
                          "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                          "item->setRotation(QVector3D(5, -12, 0));\n"
                          "item->setSurfaceIntensity(0.75);\n"
                          "item->setHoverLift(4.5);\n"
                          "// The existing controls keep their models and signal connections.\n");
}

QVector<GallerySample> spatialViewSamples()
{
    QVector<GallerySample> samples = {
        makeSample(
            QStringLiteral("spatial-view-scene"), QStringLiteral("Scene perspective"),
            QStringLiteral("Adjust the camera and zoom, then move the pointer over the cards."),
            QStringLiteral(
                "using namespace fluent;\n"
                "// The two cards keep their own poses while the view changes the whole scene.\n"
                "class SceneView final : public spatial::SpatialView {\n"
                "public:\n"
                "    using SpatialView::SpatialView;\n"
                "    void arrange()\n"
                "    {\n"
                "        if (itemCount() != 2)\n"
                "            return;\n"
                "        const float spread = width() < 650 ? 24 : 100;\n"
                "        items().first()->setPosition(QVector3D(-spread, -44, -160));\n"
                "        items().last()->setPosition(QVector3D(spread, 44, 160));\n"
                "    }\n"
                "protected:\n"
                "    void resizeEvent(QResizeEvent* event) override\n"
                "    {\n"
                "        SpatialView::resizeEvent(event);\n"
                "        arrange();\n"
                "    }\n"
                "};\n"
                "auto* panel = new QWidget(parent);\n"
                "auto* column = new QVBoxLayout(panel);\n"
                "column->setContentsMargins(0, 0, 0, 0);\n"
                "column->setSpacing(16);\n"
                "auto* view = new SceneView(panel);\n"
                "view->setObjectName(\"spatialPreviewView\");\n"
                "view->setFixedHeight(400);\n"
                "view->setCameraDistance(1200);\n"
                "view->setZoom(0.9);\n"
                "view->setMaximumTilt(QPointF(6, 12));\n"
                "// Response time (140 ms), animation limit (60 fps), cache and Auto renderer use "
                "UILib defaults.\n"
                "// See SpatialView.h for the full parameter contract.\n"
                "\n"
                "// Both layers use the same size and data, making perspective easy to compare.\n"
                "const auto makeLayer = [](const QString& title, const QString& caption) {\n"
                "    auto* card = new layout::Card;\n"
                "    card->setFixedSize(220, 150);\n"
                "    auto* content = new QVBoxLayout(card);\n"
                "    content->setContentsMargins(16, 12, 16, 12);\n"
                "    content->setSpacing(6);\n"
                "    auto* heading = new textfields::Label(title, card);\n"
                "    heading->setFluentTypography(Typography::FontRole::Subtitle);\n"
                "    heading->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "    content->addWidget(heading);\n"
                "    auto* chart = new charts::Sparkline(card);\n"
                "    chart->setAccessibleName(title + \" sample chart\");\n"
                "    auto* model = new charts::ChartModel(card);\n"
                "    model->setPoints({{0, 12}, {1, 24}, {2, 18}, {3, 38}, {4, 32}, {5, 54}});\n"
                "    chart->setModel(model);\n"
                "    content->addWidget(chart, 1);\n"
                "    auto* note = new textfields::Label(caption, card);\n"
                "    note->setTextColorRole(textfields::Label::TextColorRole::Secondary);\n"
                "    content->addWidget(note);\n"
                "    return card;\n"
                "};\n"
                "spatial::SpatialItem* back = view->addWidget(\n"
                "    makeLayer(\"Back layer\", \"Z -160 · sample data\"), "
                "WidgetOwnership::Owned);\n"
                "spatial::SpatialItem* front = view->addWidget(\n"
                "    makeLayer(\"Front layer\", \"Z +160 · sample data\"), "
                "WidgetOwnership::Owned);\n"
                "back->setRotation(QVector3D(6, 16, 0));\n"
                "front->setRotation(QVector3D(-6, -16, 0));\n"
                "back->setSurfaceIntensity(0.75);\n"
                "front->setSurfaceIntensity(0.75);\n"
                "view->arrange();\n"
                "\n"
                "auto* toolbar = new QHBoxLayout;\n"
                "auto* follow = new basicinput::ToggleSwitch(panel);\n"
                "follow->setObjectName(\"spatialViewFollow\");\n"
                "follow->setAccessibleName(\"Follow pointer\");\n"
                "follow->setOnContent(\"Follow pointer\");\n"
                "follow->setOffContent(\"Follow pointer\");\n"
                "follow->setIsOn(true);\n"
                "toolbar->addWidget(follow);\n"
                "toolbar->addStretch();\n"
                "column->addLayout(toolbar);\n"
                "column->addWidget(view);\n"
                "\n"
                "// Keep view-wide controls outside the projection so their drag targets stay "
                "still.\n"
                "auto* controls = new QWidget(panel);\n"
                "controls->setObjectName(\"spatialViewControls\");\n"
                "auto* grid = new QGridLayout(controls);\n"
                "grid->setContentsMargins(0, 0, 0, 0);\n"
                "grid->setHorizontalSpacing(24);\n"
                "grid->setVerticalSpacing(12);\n"
                "grid->setColumnStretch(0, 1);\n"
                "grid->setColumnStretch(1, 1);\n"
                "const auto addSlider = [controls, grid](const QString& name, const QString& "
                "label,\n"
                "                                      const QString& unit, int minimum, int "
                "maximum,\n"
                "                                      int initial, int row, int col) {\n"
                "    auto* group = new QWidget(controls);\n"
                "    auto* layout = new QVBoxLayout(group);\n"
                "    layout->setContentsMargins(0, 0, 0, 0);\n"
                "    layout->setSpacing(4);\n"
                "    auto* text = new textfields::Label(group);\n"
                "    text->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "    auto* slider = new basicinput::Slider(group);\n"
                "    slider->setObjectName(name);\n"
                "    slider->setAccessibleName(label);\n"
                "    slider->setRange(minimum, maximum);\n"
                "    slider->setValue(initial);\n"
                "    const auto updateText = [text, label, unit](int value) {\n"
                "        text->setText(QString(\"%1: %2%3\").arg(label).arg(value).arg(unit));\n"
                "    };\n"
                "    updateText(initial);\n"
                "    QObject::connect(slider, &basicinput::Slider::valueChanged, text, "
                "updateText);\n"
                "    layout->addWidget(text);\n"
                "    layout->addWidget(slider);\n"
                "    grid->addWidget(group, row, col);\n"
                "    return slider;\n"
                "};\n"
                "auto* distance = addSlider(\"spatialViewDistance\", \"Camera distance\", \" "
                "px\",\n"
                "                           650, 2400, 1200, 0, 0);\n"
                "auto* zoom = addSlider(\"spatialViewZoom\", \"Scene zoom\", \"%\", 50, 110, 90, "
                "0, 1);\n"
                "QObject::connect(distance, &basicinput::Slider::valueChanged, view,\n"
                "                 [view](int value) { view->setCameraDistance(value); });\n"
                "QObject::connect(zoom, &basicinput::Slider::valueChanged, view,\n"
                "                 [view](int value) { view->setZoom(value / 100.0); });\n"
                "column->addWidget(controls);\n"
                "\n"
                "QObject::connect(follow, &basicinput::ToggleSwitch::toggled,\n"
                "                 view, &spatial::SpatialView::setPointerTrackingEnabled);\n"
                "QObject::connect(view, &spatial::SpatialView::pointerTrackingEnabledChanged,\n"
                "                 follow, &basicinput::ToggleSwitch::setIsOn);\n"
                "const auto updateAvailability = [view, controls, follow] {\n"
                "    controls->setEnabled(view->isSpatialEnabled());\n"
                "    follow->setEnabled(view->isSpatialEnabled());\n"
                "};\n"
                "QObject::connect(view, &spatial::SpatialView::spatialEnabledChanged, controls, "
                "updateAvailability);\n"
                "updateAvailability();\n"),
            [](QWidget* parent) {
                // The two cards keep their own poses while the view changes the whole scene.
                class SceneView final : public spatial::SpatialView {
                public:
                    using SpatialView::SpatialView;
                    void arrange()
                    {
                        if (itemCount() != 2)
                            return;
                        const float spread = width() < 650 ? 24 : 100;
                        items().first()->setPosition(QVector3D(-spread, -44, -160));
                        items().last()->setPosition(QVector3D(spread, 44, 160));
                    }

                protected:
                    void resizeEvent(QResizeEvent* event) override
                    {
                        SpatialView::resizeEvent(event);
                        arrange();
                    }
                };
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(16);
                auto* view = new SceneView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(400);
                view->setCameraDistance(1200);
                view->setZoom(0.9);
                view->setMaximumTilt(QPointF(6, 12));
                // Response time (140 ms), animation limit (60 fps), cache and Auto renderer use UILib defaults.
                // See SpatialView.h for the full parameter contract.

                // Both layers use the same size and data, making perspective easy to compare.
                const auto makeLayer = [](const QString& title, const QString& caption) {
                    auto* card = new layout::Card;
                    card->setFixedSize(220, 150);
                    auto* content = new QVBoxLayout(card);
                    content->setContentsMargins(16, 12, 16, 12);
                    content->setSpacing(6);
                    auto* heading = new textfields::Label(title, card);
                    heading->setFluentTypography(Typography::FontRole::Subtitle);
                    heading->setTextColorRole(textfields::Label::TextColorRole::Primary);
                    content->addWidget(heading);
                    auto* chart = new charts::Sparkline(card);
                    chart->setAccessibleName(title + " sample chart");
                    auto* model = new charts::ChartModel(card);
                    model->setPoints({{0, 12}, {1, 24}, {2, 18}, {3, 38}, {4, 32}, {5, 54}});
                    chart->setModel(model);
                    content->addWidget(chart, 1);
                    auto* note = new textfields::Label(caption, card);
                    note->setTextColorRole(textfields::Label::TextColorRole::Secondary);
                    content->addWidget(note);
                    return card;
                };
                spatial::SpatialItem* back = view->addWidget(
                    makeLayer("Back layer", "Z -160 · sample data"), WidgetOwnership::Owned);
                spatial::SpatialItem* front = view->addWidget(
                    makeLayer("Front layer", "Z +160 · sample data"), WidgetOwnership::Owned);
                back->setRotation(QVector3D(6, 16, 0));
                front->setRotation(QVector3D(-6, -16, 0));
                back->setSurfaceIntensity(0.75);
                front->setSurfaceIntensity(0.75);
                view->arrange();

                auto* toolbar = new QHBoxLayout;
                auto* follow = new basicinput::ToggleSwitch(panel);
                follow->setObjectName("spatialViewFollow");
                follow->setAccessibleName("Follow pointer");
                follow->setOnContent("Follow pointer");
                follow->setOffContent("Follow pointer");
                follow->setIsOn(true);
                toolbar->addWidget(follow);
                toolbar->addStretch();
                column->addLayout(toolbar);
                column->addWidget(view);

                // Keep view-wide controls outside the projection so their drag targets stay still.
                auto* controls = new QWidget(panel);
                controls->setObjectName("spatialViewControls");
                auto* grid = new QGridLayout(controls);
                grid->setContentsMargins(0, 0, 0, 0);
                grid->setHorizontalSpacing(24);
                grid->setVerticalSpacing(12);
                grid->setColumnStretch(0, 1);
                grid->setColumnStretch(1, 1);
                const auto addSlider = [controls, grid](const QString& name, const QString& label,
                                                        const QString& unit, int minimum,
                                                        int maximum, int initial, int row,
                                                        int col) {
                    auto* group = new QWidget(controls);
                    auto* layout = new QVBoxLayout(group);
                    layout->setContentsMargins(0, 0, 0, 0);
                    layout->setSpacing(4);
                    auto* text = new textfields::Label(group);
                    text->setTextColorRole(textfields::Label::TextColorRole::Primary);
                    auto* slider = new basicinput::Slider(group);
                    slider->setObjectName(name);
                    slider->setAccessibleName(label);
                    slider->setRange(minimum, maximum);
                    slider->setValue(initial);
                    const auto updateText = [text, label, unit](int value) {
                        text->setText(QString("%1: %2%3").arg(label).arg(value).arg(unit));
                    };
                    updateText(initial);
                    QObject::connect(slider, &basicinput::Slider::valueChanged, text, updateText);
                    layout->addWidget(text);
                    layout->addWidget(slider);
                    grid->addWidget(group, row, col);
                    return slider;
                };
                auto* distance = addSlider("spatialViewDistance", "Camera distance", " px", 650,
                                           2400, 1200, 0, 0);
                auto* zoom = addSlider("spatialViewZoom", "Scene zoom", "%", 50, 110, 90, 0, 1);
                QObject::connect(distance, &basicinput::Slider::valueChanged, view,
                                 [view](int value) { view->setCameraDistance(value); });
                QObject::connect(zoom, &basicinput::Slider::valueChanged, view,
                                 [view](int value) { view->setZoom(value / 100.0); });
                column->addWidget(controls);

                QObject::connect(follow, &basicinput::ToggleSwitch::toggled, view,
                                 &spatial::SpatialView::setPointerTrackingEnabled);
                QObject::connect(view, &spatial::SpatialView::pointerTrackingEnabledChanged, follow,
                                 &basicinput::ToggleSwitch::setIsOn);
                const auto updateAvailability = [view, controls, follow] {
                    controls->setEnabled(view->isSpatialEnabled());
                    follow->setEnabled(view->isSpatialEnabled());
                };
                QObject::connect(view, &spatial::SpatialView::spatialEnabledChanged, controls,
                                 updateAvailability);
                updateAvailability();

                new SpatialPreviewBinding(panel);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("spatial-view-cards"), QStringLiteral("Two cards at different depths"),
            QStringLiteral("A settings card and a Sparkline share one view at different depths."),
            QStringLiteral(
                "using namespace fluent;\n"
                "auto* panel = new QWidget(parent);\n"
                "auto* column = new QVBoxLayout(panel);\n"
                "column->setContentsMargins(0, 0, 0, 0);\n"
                "column->setSpacing(12);\n"
                "auto* view = new spatial::SpatialView(panel);\n"
                "view->setObjectName(\"spatialPreviewView\");\n"
                "view->setFixedHeight(340);\n"
                "view->setCameraDistance(1400);\n"
                "view->setMaximumTilt(QPointF(4, 6));\n"
                "column->addWidget(view);\n"
                "view->setZoom(0.78);\n"
                "// Each intact card is a separate item. Models and control signals stay ordinary "
                "Qt.\n"
                "auto* settings = new layout::Card;\n"
                "settings->setFixedSize(270, 180);\n"
                "auto* controls = new QVBoxLayout(settings);\n"
                "controls->setContentsMargins(20, 16, 20, 16);\n"
                "auto* heading = new textfields::Label(\"Notifications\", settings);\n"
                "heading->setFluentTypography(Typography::FontRole::Subtitle);\n"
                "heading->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "controls->addWidget(heading);\n"
                "auto* alerts = new basicinput::ToggleSwitch(settings);\n"
                "alerts->setAccessibleName(\"Desktop alerts\");\n"
                "alerts->setOnContent(\"Desktop alerts on\");\n"
                "alerts->setOffContent(\"Desktop alerts off\");\n"
                "alerts->setIsOn(true);\n"
                "controls->addWidget(alerts);\n"
                "auto* note = new textfields::Label(\"Try the switch in either mode.\", "
                "settings);\n"
                "note->setTextColorRole(textfields::Label::TextColorRole::Secondary);\n"
                "note->setWordWrap(true);\n"
                "controls->addWidget(note);\n"
                "auto* back = view->addWidget(settings, WidgetOwnership::Owned);\n"
                "back->setSurfaceIntensity(0.75);\n"
                "back->setHoverLift(4.5);\n"
                "back->setPosition(QVector3D(-110, -64, -80));\n"
                "back->setRotation(QVector3D(6, 12, 0));\n"
                "\n"
                "auto* summary = new layout::Card;\n"
                "summary->setFixedSize(270, 180);\n"
                "auto* summaryLayout = new QVBoxLayout(summary);\n"
                "summaryLayout->setContentsMargins(20, 16, 20, 16);\n"
                "auto* chart = new charts::Sparkline(summary);\n"
                "chart->setObjectName(\"spatialDemoChart\");\n"
                "chart->setAccessibleName(\"Illustrative activity trend\");\n"
                "chart->setMinimumHeight(72);\n"
                "auto* model = new charts::ChartModel(summary);\n"
                "model->setPoints({{0, 12}, {1, 24}, {2, 18}, {3, 38}, {4, 32}, {5, 54}});\n"
                "chart->setModel(model);\n"
                "auto* caption = new textfields::Label(\"Activity trend · demo data\", summary);\n"
                "caption->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "summaryLayout->addWidget(caption);\n"
                "summaryLayout->addWidget(chart);\n"
                "auto* front = view->addWidget(summary, WidgetOwnership::Owned);\n"
                "front->setSurfaceIntensity(0.75);\n"
                "front->setHoverLift(4.5);\n"
                "front->setPosition(QVector3D(110, 64, 100));\n"
                "front->setRotation(QVector3D(-6, -14, 0));\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(12);
                auto* view = new spatial::SpatialView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(340);
                view->setCameraDistance(1400);
                view->setMaximumTilt(QPointF(4, 6));
                column->addWidget(view);
                view->setZoom(0.78);
                // Each intact card is a separate item. Models and control signals stay ordinary Qt.
                auto* settings = new layout::Card;
                settings->setFixedSize(270, 180);
                auto* controls = new QVBoxLayout(settings);
                controls->setContentsMargins(20, 16, 20, 16);
                auto* heading = new textfields::Label("Notifications", settings);
                heading->setFluentTypography(Typography::FontRole::Subtitle);
                heading->setTextColorRole(textfields::Label::TextColorRole::Primary);
                controls->addWidget(heading);
                auto* alerts = new basicinput::ToggleSwitch(settings);
                alerts->setAccessibleName("Desktop alerts");
                alerts->setOnContent("Desktop alerts on");
                alerts->setOffContent("Desktop alerts off");
                alerts->setIsOn(true);
                controls->addWidget(alerts);
                auto* note = new textfields::Label("Try the switch in either mode.", settings);
                note->setTextColorRole(textfields::Label::TextColorRole::Secondary);
                note->setWordWrap(true);
                controls->addWidget(note);
                auto* back = view->addWidget(settings, WidgetOwnership::Owned);
                back->setSurfaceIntensity(0.75);
                back->setHoverLift(4.5);
                back->setPosition(QVector3D(-110, -64, -80));
                back->setRotation(QVector3D(6, 12, 0));

                auto* summary = new layout::Card;
                summary->setFixedSize(270, 180);
                auto* summaryLayout = new QVBoxLayout(summary);
                summaryLayout->setContentsMargins(20, 16, 20, 16);
                auto* chart = new charts::Sparkline(summary);
                chart->setObjectName("spatialDemoChart");
                chart->setAccessibleName("Illustrative activity trend");
                chart->setMinimumHeight(72);
                auto* model = new charts::ChartModel(summary);
                model->setPoints({{0, 12}, {1, 24}, {2, 18}, {3, 38}, {4, 32}, {5, 54}});
                chart->setModel(model);
                auto* caption = new textfields::Label("Activity trend · demo data", summary);
                caption->setTextColorRole(textfields::Label::TextColorRole::Primary);
                summaryLayout->addWidget(caption);
                summaryLayout->addWidget(chart);
                auto* front = view->addWidget(summary, WidgetOwnership::Owned);
                front->setSurfaceIntensity(0.75);
                front->setHoverLift(4.5);
                front->setPosition(QVector3D(110, 64, 100));
                front->setRotation(QVector3D(-6, -14, 0));
                new SpatialPreviewBinding(panel);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("spatial-view-inputs"), QStringLiteral("Slider and CheckBox"),
            QStringLiteral("Drag the level slider, then mute and restore the preview."),
            QStringLiteral("using namespace fluent;\n"
                           "auto* panel = new QWidget(parent);\n"
                           "auto* column = new QVBoxLayout(panel);\n"
                           "column->setContentsMargins(0, 0, 0, 0);\n"
                           "column->setSpacing(12);\n"
                           "auto* view = new spatial::SpatialView(panel);\n"
                           "view->setObjectName(\"spatialPreviewView\");\n"
                           "view->setFixedHeight(340);\n"
                           "view->setCameraDistance(1400);\n"
                           "view->setMaximumTilt(QPointF(4, 6));\n"
                           "column->addWidget(view);\n"
                           "auto* card = new layout::Card;\n"
                           "card->setFixedSize(320, 260);\n"
                           "auto* content = new QVBoxLayout(card);\n"
                           "content->setContentsMargins(20, 16, 20, 16);\n"
                           "content->setSpacing(12);\n"
                           "auto* title = new textfields::Label(\"Level preview\", card);\n"
                           "title->setFluentTypography(Typography::FontRole::Subtitle);\n"
                           "title->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                           "content->addWidget(title);\n"
                           "auto* value = new textfields::Label(\"Level: 40%\", card);\n"
                           "value->setObjectName(\"spatialLevelText\");\n"
                           "value->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                           "content->addWidget(value);\n"
                           "auto* level = new basicinput::Slider(card);\n"
                           "level->setObjectName(\"spatialLevelSlider\");\n"
                           "level->setAccessibleName(\"Preview level\");\n"
                           "level->setRange(0, 100);\n"
                           "level->setValue(40);\n"
                           "content->addWidget(level);\n"
                           "auto* meter = new status_info::ProgressBar(card);\n"
                           "meter->setObjectName(\"spatialLevelMeter\");\n"
                           "meter->setAccessibleName(\"Preview output\");\n"
                           "meter->setValue(40);\n"
                           "content->addWidget(meter);\n"
                           "auto* mute = new basicinput::CheckBox(\"Mute preview\", card);\n"
                           "mute->setObjectName(\"spatialMuteCheckBox\");\n"
                           "content->addWidget(mute);\n"
                           "QObject::connect(level, &basicinput::Slider::valueChanged, meter,\n"
                           "                 [value, meter, mute](int number) {\n"
                           "    value->setText(QString(\"Level: %1%\").arg(number));\n"
                           "    meter->setValue(mute->isChecked() ? 0 : number);\n"
                           "});\n"
                           "QObject::connect(mute, &basicinput::CheckBox::toggled, meter,\n"
                           "                 [level, meter](bool muted) {\n"
                           "    meter->setValue(muted ? 0 : level->value());\n"
                           "});\n"
                           "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                           "item->setSurfaceIntensity(0.75);\n"
                           "item->setHoverLift(4.5);\n"
                           "item->setRotation(QVector3D(5, -12, 0));\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(12);
                auto* view = new spatial::SpatialView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(340);
                view->setCameraDistance(1400);
                view->setMaximumTilt(QPointF(4, 6));
                column->addWidget(view);
                auto* card = new layout::Card;
                card->setFixedSize(320, 260);
                auto* content = new QVBoxLayout(card);
                content->setContentsMargins(20, 16, 20, 16);
                content->setSpacing(12);
                auto* title = new textfields::Label("Level preview", card);
                title->setFluentTypography(Typography::FontRole::Subtitle);
                title->setTextColorRole(textfields::Label::TextColorRole::Primary);
                content->addWidget(title);
                auto* value = new textfields::Label("Level: 40%", card);
                value->setObjectName("spatialLevelText");
                value->setTextColorRole(textfields::Label::TextColorRole::Primary);
                content->addWidget(value);
                auto* level = new basicinput::Slider(card);
                level->setObjectName("spatialLevelSlider");
                level->setAccessibleName("Preview level");
                level->setRange(0, 100);
                level->setValue(40);
                content->addWidget(level);
                auto* meter = new status_info::ProgressBar(card);
                meter->setObjectName("spatialLevelMeter");
                meter->setAccessibleName("Preview output");
                meter->setValue(40);
                content->addWidget(meter);
                auto* mute = new basicinput::CheckBox("Mute preview", card);
                mute->setObjectName("spatialMuteCheckBox");
                content->addWidget(mute);
                QObject::connect(level, &basicinput::Slider::valueChanged, meter,
                                 [value, meter, mute](int number) {
                                     value->setText(QString("Level: %1%").arg(number));
                                     meter->setValue(mute->isChecked() ? 0 : number);
                                 });
                QObject::connect(
                    mute, &basicinput::CheckBox::toggled, meter,
                    [level, meter](bool muted) { meter->setValue(muted ? 0 : level->value()); });
                auto* item = view->addWidget(card, WidgetOwnership::Owned);
                item->setSurfaceIntensity(0.75);
                item->setHoverLift(4.5);
                item->setRotation(QVector3D(5, -12, 0));
                new SpatialPreviewBinding(panel);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("spatial-view-list"), QStringLiteral("ListView and InfoBadge"),
            QStringLiteral("Select a task and mark it done. The list model and remaining count "
                           "update together."),
            QStringLiteral("using namespace fluent;\n"
                           "auto* panel = new QWidget(parent);\n"
                           "auto* column = new QVBoxLayout(panel);\n"
                           "column->setContentsMargins(0, 0, 0, 0);\n"
                           "column->setSpacing(12);\n"
                           "auto* view = new spatial::SpatialView(panel);\n"
                           "view->setObjectName(\"spatialPreviewView\");\n"
                           "view->setFixedHeight(340);\n"
                           "view->setCameraDistance(1400);\n"
                           "view->setMaximumTilt(QPointF(4, 6));\n"
                           "column->addWidget(view);\n"
                           "auto* card = new layout::Card;\n"
                           "card->setFixedSize(340, 290);\n"
                           "auto* content = new QVBoxLayout(card);\n"
                           "content->setContentsMargins(20, 16, 20, 16);\n"
                           "content->setSpacing(12);\n"
                           "auto* heading = new QHBoxLayout;\n"
                           "auto* title = new textfields::Label(\"Demo tasks\", card);\n"
                           "title->setFluentTypography(Typography::FontRole::Subtitle);\n"
                           "title->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                           "heading->addWidget(title, 1);\n"
                           "auto* count = new status_info::InfoBadge(card);\n"
                           "count->setObjectName(\"spatialTaskCount\");\n"
                           "count->setAccessibleName(\"Remaining demo tasks\");\n"
                           "count->setValue(3);\n"
                           "heading->addWidget(count);\n"
                           "content->addLayout(heading);\n"
                           "auto* model = new QStringListModel({\"Review colors\", \"Check "
                           "spacing\", \"Try dark mode\"}, card);\n"
                           "auto* tasks = new collections::ListView(card);\n"
                           "tasks->setObjectName(\"spatialTaskList\");\n"
                           "tasks->setAccessibleName(\"Demo tasks\");\n"
                           "tasks->setModel(model);\n"
                           "tasks->setEditTriggers(QAbstractItemView::NoEditTriggers);\n"
                           "tasks->setSelectedIndex(0);\n"
                           "tasks->setPlaceholderText(\"All demo tasks done\");\n"
                           "content->addWidget(tasks, 1);\n"
                           "auto* done = new basicinput::Button(\"Mark selected done\", card);\n"
                           "done->setObjectName(\"spatialTaskDone\");\n"
                           "done->setFluentStyle(basicinput::Button::Accent);\n"
                           "content->addWidget(done);\n"
                           "QObject::connect(done, &basicinput::Button::clicked, model, [model, "
                           "tasks, count, done] {\n"
                           "    const int row = tasks->selectedIndex();\n"
                           "    if (row < 0)\n"
                           "        return;\n"
                           "    model->removeRow(row);\n"
                           "    count->setValue(model->rowCount());\n"
                           "    tasks->setSelectedIndex(qMin(row, model->rowCount() - 1));\n"
                           "    done->setEnabled(model->rowCount() > 0);\n"
                           "});\n"
                           "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                           "item->setSurfaceIntensity(0.75);\n"
                           "item->setHoverLift(4.5);\n"
                           "item->setRotation(QVector3D(5, -12, 0));\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(12);
                auto* view = new spatial::SpatialView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(340);
                view->setCameraDistance(1400);
                view->setMaximumTilt(QPointF(4, 6));
                column->addWidget(view);
                auto* card = new layout::Card;
                card->setFixedSize(340, 290);
                auto* content = new QVBoxLayout(card);
                content->setContentsMargins(20, 16, 20, 16);
                content->setSpacing(12);
                auto* heading = new QHBoxLayout;
                auto* title = new textfields::Label("Demo tasks", card);
                title->setFluentTypography(Typography::FontRole::Subtitle);
                title->setTextColorRole(textfields::Label::TextColorRole::Primary);
                heading->addWidget(title, 1);
                auto* count = new status_info::InfoBadge(card);
                count->setObjectName("spatialTaskCount");
                count->setAccessibleName("Remaining demo tasks");
                count->setValue(3);
                heading->addWidget(count);
                content->addLayout(heading);
                auto* model =
                    new QStringListModel({"Review colors", "Check spacing", "Try dark mode"}, card);
                auto* tasks = new collections::ListView(card);
                tasks->setObjectName("spatialTaskList");
                tasks->setAccessibleName("Demo tasks");
                tasks->setModel(model);
                tasks->setEditTriggers(QAbstractItemView::NoEditTriggers);
                tasks->setSelectedIndex(0);
                tasks->setPlaceholderText("All demo tasks done");
                content->addWidget(tasks, 1);
                auto* done = new basicinput::Button("Mark selected done", card);
                done->setObjectName("spatialTaskDone");
                done->setFluentStyle(basicinput::Button::Accent);
                content->addWidget(done);
                QObject::connect(done, &basicinput::Button::clicked, model,
                                 [model, tasks, count, done] {
                                     const int row = tasks->selectedIndex();
                                     if (row < 0)
                                         return;
                                     model->removeRow(row);
                                     count->setValue(model->rowCount());
                                     tasks->setSelectedIndex(qMin(row, model->rowCount() - 1));
                                     done->setEnabled(model->rowCount() > 0);
                                 });
                auto* item = view->addWidget(card, WidgetOwnership::Owned);
                item->setSurfaceIntensity(0.75);
                item->setHoverLift(4.5);
                item->setRotation(QVector3D(5, -12, 0));
                new SpatialPreviewBinding(panel);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("spatial-view-calendar"), QStringLiteral("CalendarView and Label"),
            QStringLiteral("Choose a date on the tilted calendar. The selected date stays when you "
                           "return to 2D."),
            QStringLiteral(
                "using namespace fluent;\n"
                "auto* panel = new QWidget(parent);\n"
                "auto* column = new QVBoxLayout(panel);\n"
                "column->setContentsMargins(0, 0, 0, 0);\n"
                "column->setSpacing(12);\n"
                "auto* view = new spatial::SpatialView(panel);\n"
                "view->setObjectName(\"spatialPreviewView\");\n"
                "view->setFixedHeight(380);\n"
                "view->setZoom(0.82);\n"
                "view->setCameraDistance(1400);\n"
                "view->setMaximumTilt(QPointF(4, 6));\n"
                "column->addWidget(view);\n"
                "auto* card = new layout::Card;\n"
                "card->setFixedSize(360, 410);\n"
                "auto* content = new QVBoxLayout(card);\n"
                "content->setContentsMargins(16, 12, 16, 12);\n"
                "content->setSpacing(8);\n"
                "auto* calendar = new date_time::CalendarView(card);\n"
                "calendar->setObjectName(\"spatialCalendar\");\n"
                "calendar->setAccessibleName(\"Demo appointment date\");\n"
                "calendar->setSelectedDate(QDate(2026, 9, 15));\n"
                "calendar->setFrameVisible(false);\n"
                "content->addWidget(calendar, 1);\n"
                "auto* selected = new textfields::Label(\"Selected: 2026-09-15\", card);\n"
                "selected->setObjectName(\"spatialSelectedDate\");\n"
                "selected->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "content->addWidget(selected);\n"
                "QObject::connect(calendar, &date_time::CalendarView::selectedDateChanged, "
                "selected,\n"
                "                 [selected](const QDate& date) {\n"
                "    selected->setText(\"Selected: \" + date.toString(Qt::ISODate));\n"
                "});\n"
                "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                "item->setSurfaceIntensity(0.75);\n"
                "item->setHoverLift(4.5);\n"
                "item->setRotation(QVector3D(5, -12, 0));\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(12);
                auto* view = new spatial::SpatialView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(380);
                view->setZoom(0.82);
                view->setCameraDistance(1400);
                view->setMaximumTilt(QPointF(4, 6));
                column->addWidget(view);
                auto* card = new layout::Card;
                card->setFixedSize(360, 410);
                auto* content = new QVBoxLayout(card);
                content->setContentsMargins(16, 12, 16, 12);
                content->setSpacing(8);
                auto* calendar = new date_time::CalendarView(card);
                calendar->setObjectName("spatialCalendar");
                calendar->setAccessibleName("Demo appointment date");
                calendar->setSelectedDate(QDate(2026, 9, 15));
                calendar->setFrameVisible(false);
                content->addWidget(calendar, 1);
                auto* selected = new textfields::Label("Selected: 2026-09-15", card);
                selected->setObjectName("spatialSelectedDate");
                selected->setTextColorRole(textfields::Label::TextColorRole::Primary);
                content->addWidget(selected);
                QObject::connect(calendar, &date_time::CalendarView::selectedDateChanged, selected,
                                 [selected](const QDate& date) {
                                     selected->setText("Selected: " + date.toString(Qt::ISODate));
                                 });
                auto* item = view->addWidget(card, WidgetOwnership::Owned);
                item->setSurfaceIntensity(0.75);
                item->setHoverLift(4.5);
                item->setRotation(QVector3D(5, -12, 0));
                new SpatialPreviewBinding(panel);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("spatial-view-navigation"),
            QStringLiteral("SelectorBar with a live text preview"),
            QStringLiteral(
                "Switch tabs inside the card. Edit the name below to update the preview."),
            QStringLiteral(
                "using namespace fluent;\n"
                "auto* panel = new QWidget(parent);\n"
                "auto* column = new QVBoxLayout(panel);\n"
                "column->setContentsMargins(0, 0, 0, 0);\n"
                "column->setSpacing(12);\n"
                "auto* view = new spatial::SpatialView(panel);\n"
                "view->setObjectName(\"spatialPreviewView\");\n"
                "view->setFixedHeight(340);\n"
                "view->setCameraDistance(1400);\n"
                "view->setMaximumTilt(QPointF(4, 6));\n"
                "column->addWidget(view);\n"
                "auto* card = new layout::Card;\n"
                "card->setFixedSize(340, 260);\n"
                "auto* content = new QVBoxLayout(card);\n"
                "content->setContentsMargins(20, 16, 20, 16);\n"
                "content->setSpacing(12);\n"
                "auto* title = new textfields::Label(\"Studio\", card);\n"
                "title->setObjectName(\"spatialProfileTitle\");\n"
                "title->setWordWrap(true);\n"
                "title->setFluentTypography(Typography::FontRole::Subtitle);\n"
                "title->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "content->addWidget(title);\n"
                "auto* tabs = new navigation::SelectorBar(card);\n"
                "tabs->setObjectName(\"spatialProfileTabs\");\n"
                "tabs->addItem(\"Overview\");\n"
                "tabs->addItem(\"Details\");\n"
                "content->addWidget(tabs);\n"
                "auto* pages = new navigation::StackContentHost(card);\n"
                "pages->setObjectName(\"spatialProfilePages\");\n"
                "pages->setTransitionAnimationEnabled(false);\n"
                "auto* overview = new textfields::Label(\"Your workspace preview\", pages);\n"
                "overview->setWordWrap(true);\n"
                "overview->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "auto* details = new textfields::Label(\"Card + SelectorBar + StackContentHost\", "
                "pages);\n"
                "details->setWordWrap(true);\n"
                "details->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "pages->insertPage(0, overview, WidgetOwnership::Owned);\n"
                "pages->insertPage(1, details, WidgetOwnership::Owned);\n"
                "pages->setCurrentIndex(0, 0, false);\n"
                "content->addWidget(pages, 1);\n"
                "tabs->setSelectedIndex(0);\n"
                "QObject::connect(tabs, &navigation::SelectorBar::selectedIndexChanged, pages,\n"
                "                 [pages](int index) { pages->setCurrentIndex(index, 0, false); "
                "});\n"
                "// Keep text editing in the normal layout, connected to the spatial preview.\n"
                "auto* nameLabel = new textfields::Label(\"Workspace name\", panel);\n"
                "nameLabel->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "column->addWidget(nameLabel);\n"
                "auto* name = new textfields::LineEdit(panel);\n"
                "name->setObjectName(\"spatialProfileName\");\n"
                "name->setAccessibleName(\"Workspace name\");\n"
                "name->setText(\"Studio\");\n"
                "name->setMaxLength(24);\n"
                "column->addWidget(name);\n"
                "QObject::connect(name, &textfields::LineEdit::textChanged, title,\n"
                "                 [title](const QString& text) {\n"
                "    title->setText(text.isEmpty() ? \"Workspace\" : text);\n"
                "});\n"
                "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                "item->setSurfaceIntensity(0.75);\n"
                "item->setHoverLift(4.5);\n"
                "item->setRotation(QVector3D(5, -12, 0));\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(12);
                auto* view = new spatial::SpatialView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(340);
                view->setCameraDistance(1400);
                view->setMaximumTilt(QPointF(4, 6));
                column->addWidget(view);
                auto* card = new layout::Card;
                card->setFixedSize(340, 260);
                auto* content = new QVBoxLayout(card);
                content->setContentsMargins(20, 16, 20, 16);
                content->setSpacing(12);
                auto* title = new textfields::Label("Studio", card);
                title->setObjectName("spatialProfileTitle");
                title->setWordWrap(true);
                title->setFluentTypography(Typography::FontRole::Subtitle);
                title->setTextColorRole(textfields::Label::TextColorRole::Primary);
                content->addWidget(title);
                auto* tabs = new navigation::SelectorBar(card);
                tabs->setObjectName("spatialProfileTabs");
                tabs->addItem("Overview");
                tabs->addItem("Details");
                content->addWidget(tabs);
                auto* pages = new navigation::StackContentHost(card);
                pages->setObjectName("spatialProfilePages");
                pages->setTransitionAnimationEnabled(false);
                auto* overview = new textfields::Label("Your workspace preview", pages);
                overview->setWordWrap(true);
                overview->setTextColorRole(textfields::Label::TextColorRole::Primary);
                auto* details =
                    new textfields::Label("Card + SelectorBar + StackContentHost", pages);
                details->setWordWrap(true);
                details->setTextColorRole(textfields::Label::TextColorRole::Primary);
                pages->insertPage(0, overview, WidgetOwnership::Owned);
                pages->insertPage(1, details, WidgetOwnership::Owned);
                pages->setCurrentIndex(0, 0, false);
                content->addWidget(pages, 1);
                tabs->setSelectedIndex(0);
                QObject::connect(tabs, &navigation::SelectorBar::selectedIndexChanged, pages,
                                 [pages](int index) { pages->setCurrentIndex(index, 0, false); });
                // Keep text editing in the normal layout, connected to the spatial preview.
                auto* nameLabel = new textfields::Label("Workspace name", panel);
                nameLabel->setTextColorRole(textfields::Label::TextColorRole::Primary);
                column->addWidget(nameLabel);
                auto* name = new textfields::LineEdit(panel);
                name->setObjectName("spatialProfileName");
                name->setAccessibleName("Workspace name");
                name->setText("Studio");
                name->setMaxLength(24);
                column->addWidget(name);
                QObject::connect(name, &textfields::LineEdit::textChanged, title,
                                 [title](const QString& text) {
                                     title->setText(text.isEmpty() ? "Workspace" : text);
                                 });
                auto* item = view->addWidget(card, WidgetOwnership::Owned);
                item->setSurfaceIntensity(0.75);
                item->setHoverLift(4.5);
                item->setRotation(QVector3D(5, -12, 0));
                new SpatialPreviewBinding(panel);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("spatial-view-rating"), QStringLiteral("RadioButton and RatingControl"),
            QStringLiteral("Choose a preset and rate it inside the tilted card."),
            QStringLiteral(
                "using namespace fluent;\n"
                "auto* panel = new QWidget(parent);\n"
                "auto* column = new QVBoxLayout(panel);\n"
                "column->setContentsMargins(0, 0, 0, 0);\n"
                "column->setSpacing(12);\n"
                "auto* view = new spatial::SpatialView(panel);\n"
                "view->setObjectName(\"spatialPreviewView\");\n"
                "view->setFixedHeight(380);\n"
                "view->setCameraDistance(1400);\n"
                "view->setMaximumTilt(QPointF(4, 6));\n"
                "column->addWidget(view);\n"
                "auto* card = new layout::Card;\n"
                "card->setFixedSize(340, 300);\n"
                "auto* content = new QVBoxLayout(card);\n"
                "content->setContentsMargins(20, 16, 20, 16);\n"
                "content->setSpacing(12);\n"
                "auto* title = new textfields::Label(\"Review a preset\", card);\n"
                "title->setFluentTypography(Typography::FontRole::Subtitle);\n"
                "title->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "content->addWidget(title);\n"
                "auto* balanced = new basicinput::RadioButton(\"Balanced\", card);\n"
                "balanced->setObjectName(\"spatialBalancedPreset\");\n"
                "auto* vivid = new basicinput::RadioButton(\"Vivid\", card);\n"
                "vivid->setObjectName(\"spatialVividPreset\");\n"
                "balanced->setChecked(true);\n"
                "content->addWidget(balanced);\n"
                "content->addWidget(vivid);\n"
                "auto* rating = new basicinput::RatingControl(card);\n"
                "rating->setObjectName(\"spatialPresetRating\");\n"
                "rating->setAccessibleName(\"Preset rating\");\n"
                "rating->setValue(3);\n"
                "rating->setCaption(\"3 / 5\");\n"
                "content->addWidget(rating);\n"
                "auto* result = new textfields::Label(\"Balanced selected\", card);\n"
                "result->setObjectName(\"spatialPresetResult\");\n"
                "result->setTextColorRole(textfields::Label::TextColorRole::Secondary);\n"
                "content->addWidget(result);\n"
                "QObject::connect(balanced, &basicinput::RadioButton::toggled, result,\n"
                "                 [result](bool checked) { if (checked) result->setText(\"Balanced "
                "selected\"); });\n"
                "QObject::connect(vivid, &basicinput::RadioButton::toggled, result,\n"
                "                 [result](bool checked) { if (checked) result->setText(\"Vivid "
                "selected\"); });\n"
                "QObject::connect(rating, &basicinput::RatingControl::valueChanged, rating,\n"
                "                 [rating](double value) {\n"
                "    rating->setCaption(value < 0 ? \"Not rated\" : QString(\"%1 / "
                "5\").arg(value));\n"
                "});\n"
                "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                "item->setSurfaceIntensity(0.75);\n"
                "item->setHoverLift(4.5);\n"
                "item->setRotation(QVector3D(5, -12, 0));\n"),
            [](QWidget* parent) {
                using namespace fluent;
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(12);
                auto* view = new spatial::SpatialView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(380);
                view->setCameraDistance(1400);
                view->setMaximumTilt(QPointF(4, 6));
                column->addWidget(view);
                auto* card = new layout::Card;
                card->setFixedSize(340, 300);
                auto* content = new QVBoxLayout(card);
                content->setContentsMargins(20, 16, 20, 16);
                content->setSpacing(12);
                auto* title = new textfields::Label("Review a preset", card);
                title->setFluentTypography(Typography::FontRole::Subtitle);
                title->setTextColorRole(textfields::Label::TextColorRole::Primary);
                content->addWidget(title);
                auto* balanced = new basicinput::RadioButton("Balanced", card);
                balanced->setObjectName("spatialBalancedPreset");
                auto* vivid = new basicinput::RadioButton("Vivid", card);
                vivid->setObjectName("spatialVividPreset");
                balanced->setChecked(true);
                content->addWidget(balanced);
                content->addWidget(vivid);
                auto* rating = new basicinput::RatingControl(card);
                rating->setObjectName("spatialPresetRating");
                rating->setAccessibleName("Preset rating");
                rating->setValue(3);
                rating->setCaption("3 / 5");
                content->addWidget(rating);
                auto* result = new textfields::Label("Balanced selected", card);
                result->setObjectName("spatialPresetResult");
                result->setTextColorRole(textfields::Label::TextColorRole::Secondary);
                content->addWidget(result);
                QObject::connect(balanced, &basicinput::RadioButton::toggled, result,
                                 [result](bool checked) {
                                     if (checked)
                                         result->setText("Balanced selected");
                                 });
                QObject::connect(vivid, &basicinput::RadioButton::toggled, result,
                                 [result](bool checked) {
                                     if (checked)
                                         result->setText("Vivid selected");
                                 });
                QObject::connect(rating, &basicinput::RatingControl::valueChanged, rating,
                                 [rating](double value) {
                                     rating->setCaption(value < 0 ? "Not rated"
                                                                  : QString("%1 / 5").arg(value));
                                 });
                auto* item = view->addWidget(card, WidgetOwnership::Owned);
                item->setSurfaceIntensity(0.75);
                item->setHoverLift(4.5);
                item->setRotation(QVector3D(5, -12, 0));
                new SpatialPreviewBinding(panel);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("spatial-view-tree"), QStringLiteral("File browser"),
            QStringLiteral("Expand folders and select files inside the card."),
            QStringLiteral(
                "// Gallery row styling: app/view/widgets/samples/CollectionSampleDelegates.h\n"
                "#include \"CollectionSampleDelegates.h\"\n"
                "using namespace fluent;\n"
                "auto* panel = new QWidget(parent);\n"
                "auto* column = new QVBoxLayout(panel);\n"
                "column->setContentsMargins(0, 0, 0, 0);\n"
                "column->setSpacing(12);\n"
                "auto* view = new spatial::SpatialView(panel);\n"
                "view->setObjectName(\"spatialPreviewView\");\n"
                "view->setFixedHeight(356);\n"
                "view->setCameraDistance(1400);\n"
                "view->setMaximumTilt(QPointF(4, 6));\n"
                "column->addWidget(view);\n"
                "auto* card = new layout::Card;\n"
                "card->setFixedSize(360, 264);\n"
                "auto* content = new QVBoxLayout(card);\n"
                "content->setContentsMargins(20, 16, 20, 16);\n"
                "content->setSpacing(8);\n"
                "auto* title = new textfields::Label(\"Project files\", card);\n"
                "title->setFluentTypography(Typography::FontRole::Subtitle);\n"
                "title->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "content->addWidget(title);\n"
                "auto* tree = new collections::TreeView(card);\n"
                "tree->setObjectName(\"spatialFileTree\");\n"
                "tree->setAccessibleName(\"Project files\");\n"
                "tree->setHeaderHidden(true);\n"
                "tree->setBorderVisible(false);\n"
                "tree->setBackgroundVisible(false);\n"
                "// Reuse Gallery's row styling; this delegate is not part of the Spatial API.\n"
                "tree->setItemDelegate(new gallery::TreeRowDelegate(tree, 32, tree, tree));\n"
                "auto* model = new QStandardItemModel(card);\n"
                "const auto node = [](const QString& name, const QString& icon) {\n"
                "    auto* row = new QStandardItem(name);\n"
                "    row->setData(icon, gallery::TreeIconGlyphRole);\n"
                "    return row;\n"
                "};\n"
                "auto* sources = node(\"Sources\", Typography::Icons::Folder);\n"
                "sources->appendRow(node(\"main.cpp\", Typography::Icons::Document));\n"
                "sources->appendRow(node(\"window.cpp\", Typography::Icons::Document));\n"
                "model->appendRow(sources);\n"
                "auto* assets = node(\"Assets\", Typography::Icons::Folder);\n"
                "assets->appendRow(node(\"logo.svg\", Typography::Icons::Document));\n"
                "model->appendRow(assets);\n"
                "tree->setModel(model);\n"
                "tree->expandAll();\n"
                "tree->setCurrentIndex(model->index(0, 0, model->index(0, 0)));\n"
                "content->addWidget(tree, 1);\n"
                "auto* selected = new textfields::Label(\"main.cpp\", card);\n"
                "selected->setObjectName(\"spatialSelectedFile\");\n"
                "selected->setTextColorRole(textfields::Label::TextColorRole::Secondary);\n"
                "content->addWidget(selected);\n"
                "QObject::connect(tree, &QTreeView::clicked, selected,\n"
                "                 [selected](const QModelIndex& index) {\n"
                "    selected->setText(index.data().toString());\n"
                "});\n"
                "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                "item->setSurfaceIntensity(0.75);\n"
                "item->setHoverLift(4.5);\n"
                "item->setRotation(QVector3D(5, -12, 0));\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(12);
                auto* view = new spatial::SpatialView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(356);
                view->setCameraDistance(1400);
                view->setMaximumTilt(QPointF(4, 6));
                column->addWidget(view);
                auto* card = new layout::Card;
                card->setFixedSize(360, 264);
                auto* content = new QVBoxLayout(card);
                content->setContentsMargins(20, 16, 20, 16);
                content->setSpacing(8);
                auto* title = new textfields::Label("Project files", card);
                title->setFluentTypography(Typography::FontRole::Subtitle);
                title->setTextColorRole(textfields::Label::TextColorRole::Primary);
                content->addWidget(title);
                auto* tree = new collections::TreeView(card);
                tree->setObjectName("spatialFileTree");
                tree->setAccessibleName("Project files");
                tree->setHeaderHidden(true);
                tree->setBorderVisible(false);
                tree->setBackgroundVisible(false);
                // Reuse Gallery's row styling; this delegate is not part of the Spatial API.
                tree->setItemDelegate(new gallery::TreeRowDelegate(tree, 32, tree, tree));
                auto* model = new QStandardItemModel(card);
                const auto node = [](const QString& name, const QString& icon) {
                    auto* row = new QStandardItem(name);
                    row->setData(icon, gallery::TreeIconGlyphRole);
                    return row;
                };
                auto* sources = node("Sources", Typography::Icons::Folder);
                sources->appendRow(node("main.cpp", Typography::Icons::Document));
                sources->appendRow(node("window.cpp", Typography::Icons::Document));
                model->appendRow(sources);
                auto* assets = node("Assets", Typography::Icons::Folder);
                assets->appendRow(node("logo.svg", Typography::Icons::Document));
                model->appendRow(assets);
                tree->setModel(model);
                tree->expandAll();
                tree->setCurrentIndex(model->index(0, 0, model->index(0, 0)));
                content->addWidget(tree, 1);
                auto* selected = new textfields::Label("main.cpp", card);
                selected->setObjectName("spatialSelectedFile");
                selected->setTextColorRole(textfields::Label::TextColorRole::Secondary);
                content->addWidget(selected);
                QObject::connect(tree, &QTreeView::clicked, selected,
                                 [selected](const QModelIndex& index) {
                                     selected->setText(index.data().toString());
                                 });
                auto* item = view->addWidget(card, WidgetOwnership::Owned);
                item->setSurfaceIntensity(0.75);
                item->setHoverLift(4.5);
                item->setRotation(QVector3D(5, -12, 0));

                new SpatialPreviewBinding(panel);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("spatial-view-donut"), QStringLiteral("DonutChart and Slider"),
            QStringLiteral(
                "Adjust the allocation. The same ChartModel updates the projected chart."),
            QStringLiteral(
                "using namespace fluent;\n"
                "auto* panel = new QWidget(parent);\n"
                "auto* column = new QVBoxLayout(panel);\n"
                "column->setContentsMargins(0, 0, 0, 0);\n"
                "column->setSpacing(12);\n"
                "auto* view = new spatial::SpatialView(panel);\n"
                "view->setObjectName(\"spatialPreviewView\");\n"
                "view->setFixedHeight(380);\n"
                "view->setCameraDistance(1400);\n"
                "view->setMaximumTilt(QPointF(4, 6));\n"
                "column->addWidget(view);\n"
                "auto* card = new layout::Card;\n"
                "card->setFixedSize(340, 300);\n"
                "auto* content = new QVBoxLayout(card);\n"
                "content->setContentsMargins(20, 16, 20, 16);\n"
                "content->setSpacing(12);\n"
                "auto* chart = new charts::DonutChart(card);\n"
                "chart->setObjectName(\"spatialAllocationChart\");\n"
                "chart->setAccessibleName(\"Illustrative storage allocation\");\n"
                "chart->setTitle(\"Storage allocation\");\n"
                "chart->setLegendVisible(false);\n"
                "chart->setCenterText(\"65%\");\n"
                "chart->setCenterCaption(\"used\");\n"
                "auto* model = new charts::ChartModel(card);\n"
                "model->setPoints({{0, 65}, {1, 35}}, {\"Used\", \"Free\"});\n"
                "chart->setModel(model);\n"
                "content->addWidget(chart, 1);\n"
                "auto* allocation = new basicinput::Slider(card);\n"
                "allocation->setObjectName(\"spatialAllocationSlider\");\n"
                "allocation->setAccessibleName(\"Used storage percentage\");\n"
                "allocation->setRange(5, 95);\n"
                "allocation->setValue(65);\n"
                "content->addWidget(allocation);\n"
                "QObject::connect(allocation, &basicinput::Slider::valueChanged, chart,\n"
                "                 [chart, model](int value) {\n"
                "    model->setPoints({{0, double(value)}, {1, double(100 - value)}}, {\"Used\", "
                "\"Free\"});\n"
                "    chart->setCenterText(QString(\"%1%\").arg(value));\n"
                "});\n"
                "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                "item->setSurfaceIntensity(0.75);\n"
                "item->setHoverLift(4.5);\n"
                "item->setRotation(QVector3D(5, -12, 0));\n"),
            [](QWidget* parent) {
                using namespace fluent;
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(12);
                auto* view = new spatial::SpatialView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(380);
                view->setCameraDistance(1400);
                view->setMaximumTilt(QPointF(4, 6));
                column->addWidget(view);
                auto* card = new layout::Card;
                card->setFixedSize(340, 300);
                auto* content = new QVBoxLayout(card);
                content->setContentsMargins(20, 16, 20, 16);
                content->setSpacing(12);
                auto* chart = new charts::DonutChart(card);
                chart->setObjectName("spatialAllocationChart");
                chart->setAccessibleName("Illustrative storage allocation");
                chart->setTitle("Storage allocation");
                chart->setLegendVisible(false);
                chart->setCenterText("65%");
                chart->setCenterCaption("used");
                auto* model = new charts::ChartModel(card);
                model->setPoints({{0, 65}, {1, 35}}, {"Used", "Free"});
                chart->setModel(model);
                content->addWidget(chart, 1);
                auto* allocation = new basicinput::Slider(card);
                allocation->setObjectName("spatialAllocationSlider");
                allocation->setAccessibleName("Used storage percentage");
                allocation->setRange(5, 95);
                allocation->setValue(65);
                content->addWidget(allocation);
                QObject::connect(allocation, &basicinput::Slider::valueChanged, chart,
                                 [chart, model](int value) {
                                     model->setPoints(
                                         {{0, double(value)}, {1, double(100 - value)}},
                                         {"Used", "Free"});
                                     chart->setCenterText(QString("%1%").arg(value));
                                 });
                auto* item = view->addWidget(card, WidgetOwnership::Owned);
                item->setSurfaceIntensity(0.75);
                item->setHoverLift(4.5);
                item->setRotation(QVector3D(5, -12, 0));
                new SpatialPreviewBinding(panel);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("spatial-view-hybrid"), QStringLiteral("File preview"),
            QStringLiteral("Choose a format. The dropdown updates the card in either mode."),
            QStringLiteral(
                "using namespace fluent;\n"
                "auto* panel = new QWidget(parent);\n"
                "auto* column = new QVBoxLayout(panel);\n"
                "column->setContentsMargins(0, 0, 0, 0);\n"
                "column->setSpacing(12);\n"
                "auto* view = new spatial::SpatialView(panel);\n"
                "view->setObjectName(\"spatialPreviewView\");\n"
                "view->setFixedHeight(276);\n"
                "view->setCameraDistance(1400);\n"
                "view->setMaximumTilt(QPointF(4, 6));\n"
                "column->addWidget(view);\n"
                "auto* card = new layout::Card;\n"
                "card->setFixedSize(360, 176);\n"
                "auto* content = new QVBoxLayout(card);\n"
                "content->setContentsMargins(20, 16, 20, 16);\n"
                "content->setSpacing(12);\n"
                "auto* heading = new QHBoxLayout;\n"
                "auto* title = new textfields::Label(\"Export preview\", card);\n"
                "title->setFluentTypography(Typography::FontRole::Subtitle);\n"
                "title->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "heading->addWidget(title);\n"
                "heading->addStretch();\n"
                "auto* format = new textfields::Label(\"PNG\", card);\n"
                "format->setObjectName(\"spatialExportFormat\");\n"
                "format->setTextColorRole(textfields::Label::TextColorRole::Accent);\n"
                "heading->addWidget(format);\n"
                "content->addLayout(heading);\n"
                "auto* file = new QHBoxLayout;\n"
                "file->setSpacing(16);\n"
                "auto* icon = new FontIcon(Typography::Icons::Document, card);\n"
                "icon->setIconSize(48);\n"
                "file->addWidget(icon);\n"
                "auto* details = new QVBoxLayout;\n"
                "details->setSpacing(4);\n"
                "details->setAlignment(Qt::AlignVCenter);\n"
                "auto* filename = new textfields::Label(\"gallery-preview.png\", card);\n"
                "filename->setObjectName(\"spatialExportFilename\");\n"
                "filename->setFluentTypography(Typography::FontRole::BodyStrong);\n"
                "filename->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
                "details->addWidget(filename);\n"
                "auto* note = new textfields::Label(\"Image with transparency\", card);\n"
                "note->setObjectName(\"spatialExportNote\");\n"
                "note->setWordWrap(true);\n"
                "note->setTextColorRole(textfields::Label::TextColorRole::Secondary);\n"
                "details->addWidget(note);\n"
                "file->addLayout(details, 1);\n"
                "content->addLayout(file, 1);\n"
                "// Keep the native popup outside the projected surface.\n"
                "auto* formats = new basicinput::ComboBox(panel);\n"
                "formats->setObjectName(\"spatialExportChoice\");\n"
                "formats->setAccessibleName(\"Export format\");\n"
                "formats->setFixedWidth(120);\n"
                "formats->addItems({\"PNG\", \"JPEG\", \"SVG\"});\n"
                "QObject::connect(formats, "
                "qOverload<int>(&basicinput::ComboBox::currentIndexChanged),\n"
                "                 note, [format, filename, note, formats](int index) {\n"
                "    const auto type = formats->itemText(index);\n"
                "    format->setText(type);\n"
                "    filename->setText(\"gallery-preview.\" + type.toLower());\n"
                "    const QStringList descriptions = {\"Image with transparency\", \"Compact "
                "photo\", \"Scalable vector\"};\n"
                "    note->setText(descriptions.value(index));\n"
                "});\n"
                "auto* item = view->addWidget(card, WidgetOwnership::Owned);\n"
                "item->setSurfaceIntensity(0.75);\n"
                "item->setHoverLift(4.5);\n"
                "item->setRotation(QVector3D(5, -12, 0));\n"
                "auto* toolbar = new QHBoxLayout;\n"
                "toolbar->addStretch();\n"
                "toolbar->addWidget(formats);\n"
                "column->insertLayout(0, toolbar);\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* column = new QVBoxLayout(panel);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(12);
                auto* view = new spatial::SpatialView(panel);
                view->setObjectName("spatialPreviewView");
                view->setFixedHeight(276);
                view->setCameraDistance(1400);
                view->setMaximumTilt(QPointF(4, 6));
                column->addWidget(view);
                auto* card = new layout::Card;
                card->setFixedSize(360, 176);
                auto* content = new QVBoxLayout(card);
                content->setContentsMargins(20, 16, 20, 16);
                content->setSpacing(12);
                auto* heading = new QHBoxLayout;
                auto* title = new textfields::Label("Export preview", card);
                title->setFluentTypography(Typography::FontRole::Subtitle);
                title->setTextColorRole(textfields::Label::TextColorRole::Primary);
                heading->addWidget(title);
                heading->addStretch();
                auto* format = new textfields::Label("PNG", card);
                format->setObjectName("spatialExportFormat");
                format->setTextColorRole(textfields::Label::TextColorRole::Accent);
                heading->addWidget(format);
                content->addLayout(heading);
                auto* file = new QHBoxLayout;
                file->setSpacing(16);
                auto* icon = new FontIcon(Typography::Icons::Document, card);
                icon->setIconSize(48);
                file->addWidget(icon);
                auto* details = new QVBoxLayout;
                details->setSpacing(4);
                details->setAlignment(Qt::AlignVCenter);
                auto* filename = new textfields::Label("gallery-preview.png", card);
                filename->setObjectName("spatialExportFilename");
                filename->setFluentTypography(Typography::FontRole::BodyStrong);
                filename->setTextColorRole(textfields::Label::TextColorRole::Primary);
                details->addWidget(filename);
                auto* note = new textfields::Label("Image with transparency", card);
                note->setObjectName("spatialExportNote");
                note->setWordWrap(true);
                note->setTextColorRole(textfields::Label::TextColorRole::Secondary);
                details->addWidget(note);
                file->addLayout(details, 1);
                content->addLayout(file, 1);
                // Keep the native popup outside the projected surface.
                auto* formats = new basicinput::ComboBox(panel);
                formats->setObjectName("spatialExportChoice");
                formats->setAccessibleName("Export format");
                formats->setFixedWidth(120);
                formats->addItems({"PNG", "JPEG", "SVG"});
                QObject::connect(
                    formats, qOverload<int>(&basicinput::ComboBox::currentIndexChanged), note,
                    [format, filename, note, formats](int index) {
                        const auto type = formats->itemText(index);
                        format->setText(type);
                        filename->setText("gallery-preview." + type.toLower());
                        const QStringList descriptions = {"Image with transparency",
                                                          "Compact photo", "Scalable vector"};
                        note->setText(descriptions.value(index));
                    });
                auto* item = view->addWidget(card, WidgetOwnership::Owned);
                item->setSurfaceIntensity(0.75);
                item->setHoverLift(4.5);
                item->setRotation(QVector3D(5, -12, 0));
                auto* toolbar = new QHBoxLayout;
                toolbar->addStretch();
                toolbar->addWidget(formats);
                column->insertLayout(0, toolbar);

                new SpatialPreviewBinding(panel);
                return panel;
            },
            true)};
    for (auto& sample : samples) {
        sample.usageSnippet = spatialUsage(sample.id);
        sample.supplementary = sample.id != QStringLiteral("spatial-view-scene") &&
                               sample.id != QStringLiteral("spatial-view-cards") &&
                               sample.id != QStringLiteral("spatial-view-donut") &&
                               sample.id != QStringLiteral("spatial-view-hybrid");
    }
    return samples;
}

QVector<GallerySample> spatialItemSamples()
{
    QVector<GallerySample> samples = {makeSample(
        QStringLiteral("spatial-item-pose"), QStringLiteral("Card finish and pose"),
        QStringLiteral("Adjust one card while the controls keep their own position."),
        QStringLiteral(
            "using namespace fluent;\n"
            "// Keep the control card independent of the card it adjusts.\n"
            "class WorkbenchView final : public spatial::SpatialView {\n"
            "public:\n"
            "    using SpatialView::SpatialView;\n"
            "    void arrange()\n"
            "    {\n"
            "        if (itemCount() != 2)\n"
            "            return;\n"
            "        const bool stacked = width() < 720;\n"
            "        setFixedHeight(!isSpatialEnabled() ? 520 : (stacked ? 560 : 360));\n"
            "        auto* preview = items().first();\n"
            "        preview->setPosition(QVector3D(stacked ? 0 : -164, stacked ? -140 : 0,\n"
            "                                      preview->position().z()));\n"
            "        items().last()->setPosition(QVector3D(stacked ? 0 : 164, stacked ? 110 : 0, "
            "-50));\n"
            "    }\n"
            "protected:\n"
            "    void resizeEvent(QResizeEvent* event) override\n"
            "    {\n"
            "        SpatialView::resizeEvent(event);\n"
            "        // Update the responsive height after the current layout pass.\n"
            "        QTimer::singleShot(0, this, [this] { arrange(); });\n"
            "    }\n"
            "};\n"
            "auto* panel = new QWidget(parent);\n"
            "auto* column = new QVBoxLayout(panel);\n"
            "column->setContentsMargins(0, 0, 0, 0);\n"
            "column->setSpacing(12);\n"
            "auto* view = new WorkbenchView(panel);\n"
            "view->setObjectName(\"spatialPreviewView\");\n"
            "view->setCameraDistance(1400);\n"
            "view->setZoom(0.92);\n"
            "view->setMaximumTilt(QPointF(4, 6));\n"
            "column->addWidget(view);\n"
            "auto* card = new layout::Card;\n"
            "card->setObjectName(\"spatialDemoCard\");\n"
            "card->setFixedSize(280, 180);\n"
            "auto* content = new QVBoxLayout(card);\n"
            "content->setContentsMargins(20, 16, 20, 16);\n"
            "content->setSpacing(8);\n"
            "auto* title = new textfields::Label(\"Activity\", card);\n"
            "title->setFluentTypography(Typography::FontRole::Subtitle);\n"
            "title->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
            "content->addWidget(title);\n"
            "auto* chart = new charts::Sparkline(card);\n"
            "chart->setAccessibleName(\"Activity trend, demo data\");\n"
            "auto* model = new charts::ChartModel(card);\n"
            "model->setPoints({{0, 12}, {1, 24}, {2, 18}, {3, 38}, {4, 32}, {5, 54}});\n"
            "chart->setModel(model);\n"
            "content->addWidget(chart, 1);\n"
            "auto* caption = new textfields::Label(\"Last 6 days · demo data\", card);\n"
            "caption->setTextColorRole(textfields::Label::TextColorRole::Secondary);\n"
            "content->addWidget(caption);\n"
            "spatial::SpatialItem* item = view->addWidget(card, WidgetOwnership::Owned);\n"
            "item->setSurfaceIntensity(0.75);\n"
            "item->setHoverLift(5);\n"
            "item->setRotation(QVector3D(0, -18, 0));\n"
            "// Scale, pivot and visibility retain their defaults; see SpatialItem.h.\n"
            "auto* settings = new layout::Card;\n"
            "settings->setObjectName(\"spatialParameterCard\");\n"
            "settings->setFixedSize(280, 260);\n"
            "auto* controls = new QVBoxLayout(settings);\n"
            "controls->setContentsMargins(20, 16, 20, 16);\n"
            "controls->setSpacing(8);\n"
            "auto* heading = new textfields::Label(\"Card\", settings);\n"
            "heading->setFluentTypography(Typography::FontRole::Subtitle);\n"
            "heading->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
            "controls->addWidget(heading);\n"
            "const auto addSlider = [settings, controls](const QString& name, const QString& "
            "label,\n"
            "                                           const QString& unit, int minimum, int "
            "maximum,\n"
            "                                           int initial) {\n"
            "    auto* group = new QVBoxLayout;\n"
            "    group->setSpacing(4);\n"
            "    auto* text = new textfields::Label(settings);\n"
            "    text->setTextColorRole(textfields::Label::TextColorRole::Primary);\n"
            "    auto* slider = new basicinput::Slider(settings);\n"
            "    slider->setObjectName(name);\n"
            "    slider->setAccessibleName(label);\n"
            "    slider->setRange(minimum, maximum);\n"
            "    slider->setValue(initial);\n"
            "    const auto updateText = [text, label, unit](int value) {\n"
            "        text->setText(QString(\"%1: %2%3\").arg(label).arg(value).arg(unit));\n"
            "    };\n"
            "    updateText(initial);\n"
            "    QObject::connect(slider, &basicinput::Slider::valueChanged, text, updateText);\n"
            "    group->addWidget(text);\n"
            "    group->addWidget(slider);\n"
            "    controls->addLayout(group);\n"
            "    return slider;\n"
            "};\n"
            "auto* finish = addSlider(\"spatialItemSurfaceIntensity\", \"Surface\", \"%\", 0, 100, "
            "75);\n"
            "auto* angle = addSlider(\"spatialItemRotation\", \"Rotation\", \"°\", -35, 35, -18);\n"
            "auto* depth = addSlider(\"spatialItemDepth\", \"Depth\", \" px\", -120, 120, 0);\n"
            "QObject::connect(finish, &basicinput::Slider::valueChanged, item,\n"
            "                 [item](int value) { item->setSurfaceIntensity(value / 100.0); });\n"
            "QObject::connect(angle, &basicinput::Slider::valueChanged, item,\n"
            "                 [item](int value) { item->setRotation(QVector3D(0, value, 0)); });\n"
            "QObject::connect(depth, &basicinput::Slider::valueChanged, item, [item](int value) {\n"
            "    auto position = item->position();\n"
            "    position.setZ(value);\n"
            "    item->setPosition(position);\n"
            "});\n"
            "auto* controlItem = view->addWidget(settings, WidgetOwnership::Owned);\n"
            "controlItem->setSurfaceIntensity(0.75);\n"
            "controlItem->setRotation(QVector3D(0, -6, 0));\n"
            "// No hover lift for the parameter card: keep its drag targets steady.\n"
            "QObject::connect(view, &spatial::SpatialView::spatialEnabledChanged,\n"
            "                 view, [view] { view->arrange(); });\n"
            "view->arrange();\n"),
        [](QWidget* parent) {
            // Keep the control card independent of the card it adjusts.
            class WorkbenchView final : public spatial::SpatialView {
            public:
                using SpatialView::SpatialView;
                void arrange()
                {
                    if (itemCount() != 2)
                        return;
                    const bool stacked = width() < 720;
                    setFixedHeight(!isSpatialEnabled() ? 520 : (stacked ? 560 : 360));
                    auto* preview = items().first();
                    preview->setPosition(
                        QVector3D(stacked ? 0 : -164, stacked ? -140 : 0, preview->position().z()));
                    items().last()->setPosition(
                        QVector3D(stacked ? 0 : 164, stacked ? 110 : 0, -50));
                }

            protected:
                void resizeEvent(QResizeEvent* event) override
                {
                    SpatialView::resizeEvent(event);
                    // Changing height during resize leaves the inner canvas at the old size.
                    // zh_CN: resize 期间修改高度会让内部画布保留旧尺寸，等待本轮布局结束。
                    QTimer::singleShot(0, this, [this] { arrange(); });
                }
            };
            auto* panel = new QWidget(parent);
            auto* column = new QVBoxLayout(panel);
            column->setContentsMargins(0, 0, 0, 0);
            column->setSpacing(12);
            auto* view = new WorkbenchView(panel);
            view->setObjectName("spatialPreviewView");
            view->setCameraDistance(1400);
            view->setZoom(0.92);
            view->setMaximumTilt(QPointF(4, 6));
            column->addWidget(view);
            auto* card = new layout::Card;
            card->setObjectName("spatialDemoCard");
            card->setFixedSize(280, 180);
            auto* content = new QVBoxLayout(card);
            content->setContentsMargins(20, 16, 20, 16);
            content->setSpacing(8);
            auto* title = new textfields::Label("Activity", card);
            title->setFluentTypography(Typography::FontRole::Subtitle);
            title->setTextColorRole(textfields::Label::TextColorRole::Primary);
            content->addWidget(title);
            auto* chart = new charts::Sparkline(card);
            chart->setAccessibleName("Activity trend, demo data");
            auto* model = new charts::ChartModel(card);
            model->setPoints({{0, 12}, {1, 24}, {2, 18}, {3, 38}, {4, 32}, {5, 54}});
            chart->setModel(model);
            content->addWidget(chart, 1);
            auto* caption = new textfields::Label("Last 6 days · demo data", card);
            caption->setTextColorRole(textfields::Label::TextColorRole::Secondary);
            content->addWidget(caption);
            spatial::SpatialItem* item = view->addWidget(card, WidgetOwnership::Owned);
            item->setSurfaceIntensity(0.75);
            item->setHoverLift(5);
            item->setRotation(QVector3D(0, -18, 0));
            // Scale, pivot and visibility retain their defaults; see SpatialItem.h.
            auto* settings = new layout::Card;
            settings->setObjectName("spatialParameterCard");
            settings->setFixedSize(280, 260);
            auto* controls = new QVBoxLayout(settings);
            controls->setContentsMargins(20, 16, 20, 16);
            controls->setSpacing(8);
            auto* heading = new textfields::Label("Card", settings);
            heading->setFluentTypography(Typography::FontRole::Subtitle);
            heading->setTextColorRole(textfields::Label::TextColorRole::Primary);
            controls->addWidget(heading);
            const auto addSlider = [settings, controls](const QString& name, const QString& label,
                                                        const QString& unit, int minimum,
                                                        int maximum, int initial) {
                auto* group = new QVBoxLayout;
                group->setSpacing(4);
                auto* text = new textfields::Label(settings);
                text->setTextColorRole(textfields::Label::TextColorRole::Primary);
                auto* slider = new basicinput::Slider(settings);
                slider->setObjectName(name);
                slider->setAccessibleName(label);
                slider->setRange(minimum, maximum);
                slider->setValue(initial);
                const auto updateText = [text, label, unit](int value) {
                    text->setText(QString("%1: %2%3").arg(label).arg(value).arg(unit));
                };
                updateText(initial);
                QObject::connect(slider, &basicinput::Slider::valueChanged, text, updateText);
                group->addWidget(text);
                group->addWidget(slider);
                controls->addLayout(group);
                return slider;
            };
            auto* finish = addSlider("spatialItemSurfaceIntensity", "Surface", "%", 0, 100, 75);
            auto* angle = addSlider("spatialItemRotation", "Rotation", "°", -35, 35, -18);
            auto* depth = addSlider("spatialItemDepth", "Depth", " px", -120, 120, 0);
            QObject::connect(finish, &basicinput::Slider::valueChanged, item,
                             [item](int value) { item->setSurfaceIntensity(value / 100.0); });
            QObject::connect(angle, &basicinput::Slider::valueChanged, item,
                             [item](int value) { item->setRotation(QVector3D(0, value, 0)); });
            QObject::connect(depth, &basicinput::Slider::valueChanged, item, [item](int value) {
                auto position = item->position();
                position.setZ(value);
                item->setPosition(position);
            });
            auto* controlItem = view->addWidget(settings, WidgetOwnership::Owned);
            controlItem->setSurfaceIntensity(0.75);
            controlItem->setRotation(QVector3D(0, -6, 0));
            // No hover lift for the parameter card: keep its drag targets steady.
            QObject::connect(view, &spatial::SpatialView::spatialEnabledChanged, view,
                             [view] { view->arrange(); });
            view->arrange();

            new SpatialPreviewBinding(panel);
            return panel;
        },
        true)};
    for (auto& sample : samples)
        sample.usageSnippet = spatialUsage(sample.id);
    return samples;
}

} // namespace
QVector<GallerySample> spatialSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("spatial-view"))
        return spatialViewSamples();
    if (routeId == QStringLiteral("spatial-item"))
        return spatialItemSamples();
    return {};
}
} // namespace fluent::gallery

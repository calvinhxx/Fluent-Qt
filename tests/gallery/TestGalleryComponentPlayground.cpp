#include "view/widgets/GalleryComponentPlayground.h"
#include "view/widgets/GalleryPlaygroundFactory.h"
#include "components/basicinput/Button.h"
#include "components/charts/ChartView.h"
#include "components/collections/DataGrid.h"
#include "components/collections/DrawerView.h"
#include "components/collections/Timeline.h"
#include "components/collections/TreeView.h"
#include <QAbstractItemView>
#include <QTableView>
#include <QHeaderView>
#include "components/basicinput/DropDownButton.h"
#include "components/basicinput/HyperlinkButton.h"
#include "components/basicinput/RangeSlider.h"
#include "components/scrolling/PipsPager.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/basicinput/TreeSelect.h"
#include "components/scrolling/ScrollBar.h"
#include "components/date_time/TimePicker.h"
#include "components/dialogs_flyouts/CoachMark.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/layout/Accordion.h"
#include "components/layout/Card.h"
#include "components/layout/DataGridToolBar.h"
#include "components/layout/Expander.h"
#include "components/layout/Field.h"
#include "components/layout/ListDetailView.h"
#include "components/layout/PropertyPanel.h"
#include "components/menus_toolbars/CommandBarFlyout.h"
#include "components/menus_toolbars/CommandPalette.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include <QMenu>
#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QHBoxLayout>
#include <QApplication>
#include <QCursor>
#include <QPainter>
#include <QScopeGuard>
#include <QTest>
#include <QComboBox>
#include <QStandardItemModel>
#include <QLineEdit>
#include <QSet>

namespace {
QJsonObject playgroundField(const QJsonObject& state, const QString& id)
{
    for (const auto& field : state.value("schema").toObject().value("properties").toArray())
        if (field.toObject().value("id") == id)
            return field.toObject();
    return {};
}
} // namespace

TEST(GalleryComponentPlaygroundIsolationTest, TreeSelectionEditorFollowsActualSelectionMode)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("tree-select", &host);
    ASSERT_NE(playground, nullptr);
    auto* tree = qobject_cast<fluent::basicinput::TreeSelect*>(playground->widget());
    ASSERT_NE(tree, nullptr);
    EXPECT_TRUE(playgroundField(playground->snapshot(), "selectedKeys").value("singleSelection").toBool());
    for (const auto* key : {"Design", "Engineering", "Research", "Design"}) {
        const QJsonArray selected{key};
        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"selectedKeys", selected}}}}));
        EXPECT_EQ(tree->selectedKeys(), QStringList{QString::fromLatin1(key)});
        EXPECT_EQ(playground->snapshot().value("values").toObject().value("selectedKeys"), selected);
    }
    tree->setSelectionMode(fluent::basicinput::TreeSelect::SelectionMode::Multiple);
    EXPECT_FALSE(playgroundField(playground->snapshot(), "selectedKeys").value("singleSelection").toBool());
    tree->setSelectedKeys({"Engineering", "Research"});
    EXPECT_EQ(playground->snapshot().value("values").toObject().value("selectedKeys"),
        (QJsonArray{"Engineering", "Research"}));
    tree->setSelectionMode(fluent::basicinput::TreeSelect::SelectionMode::Single);
    EXPECT_TRUE(playgroundField(playground->snapshot(), "selectedKeys").value("singleSelection").toBool());
    EXPECT_EQ(playground->snapshot().value("values").toObject().value("selectedKeys"), QJsonArray{"Engineering"});
    ASSERT_TRUE(playground->apply({{"operation", "reset"}}));
    EXPECT_TRUE(playgroundField(playground->snapshot(), "selectedKeys").value("singleSelection").toBool());
    EXPECT_EQ(playground->snapshot().value("values").toObject().value("selectedKeys"), QJsonArray{});
}

TEST(GalleryComponentPlaygroundIsolationTest, TickChoicesHaveDistinctValuesAndFollowOrientation)
{
    for (const auto* route : {"slider", "range-slider"}) {
        SCOPED_TRACE(route);
        QWidget host;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create(route, &host);
        ASSERT_NE(playground, nullptr);
        ASSERT_TRUE(playground->apply({{"operation", "hover"}, {"hovered", true}}));
        for (const auto* orientation : {"Horizontal", "Vertical"}) {
            SCOPED_TRACE(orientation);
            ASSERT_TRUE(
                playground->apply({{"operation", "set"}, {"values", QJsonObject{{"orientation", orientation}}}}));
            const QJsonArray expected = QString(orientation) == "Horizontal"
                ? QJsonArray{"NoTicks", "TicksAbove", "TicksBelow", "TicksBothSides"}
                : QJsonArray{"NoTicks", "TicksLeft", "TicksRight", "TicksBothSides"};
            EXPECT_EQ(playgroundField(playground->snapshot(), "tickPosition").value("options"), expected);
            QImage previous;
            for (const auto& tick : expected) {
                SCOPED_TRACE(tick.toString().toStdString());
                ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"tickPosition", tick}}}}));
                const auto state = playground->snapshot();
                EXPECT_EQ(state.value("values").toObject().value("tickPosition"), tick);
                EXPECT_TRUE(state.value("cpp").toString().contains(tick.toString()));
                const auto image = playground->renderPreview(2);
                if (!previous.isNull())
                    EXPECT_NE(previous, image);
                previous = image;
            }
        }
    }
}

TEST(GalleryComponentPlaygroundIsolationTest, LinkedRangesUseCurrentBoundsAndExportEndpointPairsAtomically)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("range-slider", &host);
    ASSERT_NE(playground, nullptr);
    ASSERT_TRUE(playground->apply({{"operation", "set"},
        {"values", QJsonObject{{"minimum", -39}, {"maximum", 120}, {"lowerValue", 95}, {"upperValue", 95}}}}));
    auto state = playground->snapshot();
    EXPECT_EQ(state.value("values").toObject().value("lowerValue"), 95);
    EXPECT_EQ(state.value("values").toObject().value("upperValue"), 95);
    EXPECT_EQ(playgroundField(state, "lowerValue").value("maximum"), 95);
    EXPECT_EQ(playgroundField(state, "upperValue").value("minimum"), 95);
    for (const auto* language : {"cpp", "python"}) {
        EXPECT_TRUE(state.value(language).toString().contains("setValues(95, 95)"));
        EXPECT_FALSE(state.value(language).toString().contains("setLowerValue("));
    }
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"minimumRange", 20}}}}));
    state = playground->snapshot();
    EXPECT_EQ(playgroundField(state, "lowerValue").value("maximum").toInt(),
        state.value("values").toObject().value("upperValue").toInt() - 20);
    EXPECT_EQ(playgroundField(state, "upperValue").value("minimum").toInt(),
        state.value("values").toObject().value("lowerValue").toInt() + 20);
    ASSERT_TRUE(playground->apply({{"operation", "set"},
        {"values",
            QJsonObject{{"minimum", 0}, {"maximum", 100}, {"lowerValue", 75}, {"upperValue", 95},
                {"minimumRange", 50}}}}));
    state = playground->snapshot();
    EXPECT_EQ(state.value("values").toObject().value("lowerValue"), 50);
    EXPECT_EQ(state.value("values").toObject().value("upperValue"), 100);
}

TEST(GalleryComponentPlaygroundIsolationTest, SelectionEditorsUseAllCurrentItemsAndPages)
{
    for (const auto* route : {"pivot", "selector-bar", "stepper", "tab-view", "pips-pager", "flip-view"}) {
        QWidget host;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create(route, &host);
        ASSERT_NE(playground, nullptr);
        const QString id = QString(route) == "pips-pager" ? "selectedPageIndex"
            : (QString(route) == "stepper" || QString(route) == "flip-view")
            ? "currentIndex"
            : "selectedIndex";
        auto field = playgroundField(playground->snapshot(), id);
        EXPECT_EQ(field.value("control"), "model-select");
        for (const auto& entry : field.value("choices").toArray()) {
            const auto choice = entry.toObject();
            if (!choice.value("enabled").toBool(true))
                continue;
            ASSERT_TRUE(
                playground->apply({{"operation", "set"}, {"values", QJsonObject{{id, choice.value("value")}}}}));
            EXPECT_EQ(playground->snapshot().value("values").toObject().value(id), choice.value("value"));
        }
        if (QString(route) == "pips-pager") {
            EXPECT_EQ(field.value("choices").toArray().size(), 10);
            ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"numberOfPages", 3}}}}));
            field = playgroundField(playground->snapshot(), id);
            EXPECT_EQ(field.value("choices").toArray().size(), 3);
            EXPECT_LE(playground->snapshot().value("values").toObject().value(id).toInt(), 2);
        }
    }
}

TEST(GalleryComponentPlaygroundIsolationTest, TextEditorLengthFollowsTheControlLimit)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("line-edit", &host);
    ASSERT_NE(playground, nullptr);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"maxLength", 3}}}}));
    EXPECT_EQ(playgroundField(playground->snapshot(), "text").value("maximumLength"), 3);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"text", "abc"}}}}));
    EXPECT_EQ(playground->snapshot().value("values").toObject().value("text"), "abc");
    EXPECT_FALSE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"text", "abcd"}}}}));
    EXPECT_EQ(playground->snapshot().value("values").toObject().value("text"), "abc");
}

TEST(GalleryComponentPlaygroundIsolationTest, EditableComboPlaceholderUsesTheRealEditorAndExportsItsSetup)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("combobox", &host);
    ASSERT_NE(playground, nullptr);
    ASSERT_TRUE(playground->apply({{"operation", "set"},
        {"values", QJsonObject{{"currentIndex", -1}, {"editable", true}, {"placeholderText", "First hint"}}}}));
    auto* combo = qobject_cast<QComboBox*>(playground->widget());
    ASSERT_NE(combo, nullptr);
    ASSERT_NE(combo->lineEdit(), nullptr);
    EXPECT_EQ(combo->lineEdit()->placeholderText(), QStringLiteral("First hint"));
    const auto before = playground->renderPreview(2);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"placeholderText", "Second hint"}}}}));
    EXPECT_EQ(combo->lineEdit()->placeholderText(), QStringLiteral("Second hint"));
    EXPECT_NE(before, playground->renderPreview(2));
    EXPECT_TRUE(playground->snapshot().value("cpp").toString().contains("lineEdit()->setPlaceholderText"));
    EXPECT_TRUE(playground->snapshot().value("python").toString().contains("lineEdit().setPlaceholderText"));
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"currentIndex", 1}}}}));
    const auto selected = playground->renderPreview(2);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"placeholderText", "Stored hint"}}}}));
    EXPECT_EQ(selected, playground->renderPreview(2));
    EXPECT_EQ(combo->currentText(), QStringLiteral("Light"));
}

TEST(GalleryComponentPlaygroundIsolationTest, HyperlinkUnderlineChangesTheHoveredStackedPreview)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("hyperlink-button", &host);
    ASSERT_NE(playground, nullptr);
    ASSERT_TRUE(playground->apply({{"operation", "set"},
        {"values", QJsonObject{{"icon", "Save"}, {"fluentLayout", "IconAbove"}, {"showUnderline", false}}}}));
    ASSERT_TRUE(playground->apply({{"operation", "hover"}, {"hovered", true}}));
    const auto before = playground->renderPreview(2);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"showUnderline", true}}}}));
    EXPECT_NE(before, playground->renderPreview(2));
    EXPECT_TRUE(playground->snapshot().value("cpp").toString().contains("setShowUnderline(true)"));
}

TEST(GalleryComponentPlaygroundIsolationTest, DropdownSpecificAppearanceAndOffsetsRoundTrip)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("dropdown-button", &host);
    ASSERT_NE(playground, nullptr);
    auto* button = qobject_cast<fluent::basicinput::DropDownButton*>(playground->widget());
    ASSERT_NE(button, nullptr);
    const auto before = playground->renderPreview(2);
    ASSERT_TRUE(playground->apply({{"operation", "set"},
        {"values",
            QJsonObject{{"isOpen", true}, {"chevronGlyph", "ChevronUp"}, {"chevronOffsetX", 18},
                {"chevronOffsetY", 3}}}}));
    EXPECT_TRUE(button->isOpen());
    EXPECT_EQ(button->chevronGlyph(), Typography::Icons::ChevronUp);
    EXPECT_EQ(button->chevronOffset(), QPoint(18, 3));
    EXPECT_NE(before, playground->renderPreview(2));
    for (const auto* language : {"cpp", "python"}) {
        const auto code = playground->snapshot().value(language).toString();
        EXPECT_EQ(code.count("setChevronOffset("), 1);
        EXPECT_TRUE(code.contains("QPoint(18, 3)"));
    }
}

TEST(GalleryComponentPlaygroundIsolationTest, MultiSelectVisibleRowsChangeGeometryWithoutLimitingSelection)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("multi-select-combobox", &host);
    ASSERT_NE(playground, nullptr);
    auto* combo = playground->widget()->findChild<fluent::basicinput::MultiSelectComboBox*>();
    ASSERT_NE(combo, nullptr);
    ASSERT_TRUE(playground->apply({{"operation", "set"},
        {"values",
            QJsonObject{{"isOpen", true}, {"searchEnabled", true}, {"maximumVisibleItems", 2},
                {"selectedRows", QJsonArray{"Engineering", "Research", "Marketing", "Security"}}}}}));
    const auto twoRows = playground->renderPreview(2);
    EXPECT_EQ(combo->selectedCount(), 4);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"maximumVisibleItems", 5}}}}));
    const auto fiveRows = playground->renderPreview(2);
    EXPECT_EQ(combo->selectedCount(), 4);
    EXPECT_GT(fiveRows.height(), twoRows.height());
    EXPECT_NE(fiveRows, twoRows);
    ASSERT_TRUE(
        playground->apply({{"operation", "set"}, {"values", QJsonObject{{"searchPlaceholderText", "Search teams"}}}}));
    EXPECT_NE(fiveRows, playground->renderPreview(2));
}

TEST(GalleryComponentPlaygroundIsolationTest, ThinScrollbarPreviewRespectsItsRequestedCrossAxisSize)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("scrollbar", &host);
    ASSERT_NE(playground, nullptr);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"thickness", 4}}}}));
    const auto thin = playground->renderPreview(2);
    EXPECT_EQ(playground->widget()->height(), 4);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"thickness", 12}}}}));
    EXPECT_GT(playground->renderPreview(2).height(), thin.height());
}

TEST(GalleryComponentPlaygroundIsolationTest, FlyoutExpansionSurvivesRepositioningAndExportsAfterOpening)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("command-bar-flyout", &host);
    ASSERT_NE(playground, nullptr);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"expanded", false}}}}));
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"anchorOffset", 24}}}}));
    const auto state = playground->snapshot();
    EXPECT_TRUE(state.value("values").toObject().value("isOpen").toBool());
    EXPECT_FALSE(state.value("values").toObject().value("expanded").toBool());
    for (const auto* language : {"cpp", "python"}) {
        const auto code = state.value(language).toString();
        EXPECT_GT(code.indexOf("setExpanded("), code.indexOf("setIsOpen("));
    }
}

TEST(GalleryComponentPlaygroundIsolationTest, ComboChoicesFollowTheActualModelAndKeepNumericExports)
{
    QWidget isolated;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("combobox", &isolated);
    ASSERT_NE(playground, nullptr);
    auto* combo = qobject_cast<QComboBox*>(playground->widget());
    ASSERT_NE(combo, nullptr);
    auto selectionField = [&] {
        for (const auto& entry : playground->snapshot().value("schema").toObject().value("properties").toArray()) {
            if (entry.toObject().value("id") == QStringLiteral("currentIndex"))
                return entry.toObject();
        }
        return QJsonObject{};
    };
    auto field = selectionField();
    EXPECT_EQ(field.value("control").toString(), QStringLiteral("model-select"));
    auto choices = field.value("choices").toArray();
    ASSERT_EQ(choices.size(), combo->count() + 1);
    EXPECT_EQ(choices.first().toObject().value("value").toInt(), -1);
    for (int row = 0; row < combo->count(); ++row) {
        EXPECT_EQ(choices[row + 1].toObject().value("value").toInt(), row);
        EXPECT_EQ(choices[row + 1].toObject().value("label").toString(), combo->itemText(row));
    }
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"currentIndex", 2}}}}));
    EXPECT_EQ(combo->currentText(), QStringLiteral("Dark"));
    for (const auto* language : {"cpp", "python"})
        EXPECT_TRUE(playground->snapshot().value(language).toString().contains(QStringLiteral("setCurrentIndex(2)")));
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"currentIndex", -1}}}}));
    EXPECT_EQ(combo->currentIndex(), -1);
    combo->setItemText(1, QStringLiteral("Renamed entry"));
    combo->addItem(QStringLiteral("New model row"));
    field = selectionField();
    choices = field.value("choices").toArray();
    EXPECT_EQ(choices[2].toObject().value("label").toString(), QStringLiteral("Renamed entry"));
    EXPECT_EQ(choices.last().toObject().value("label").toString(), QStringLiteral("New model row"));
    EXPECT_EQ(field.value("maximum").toInt(), combo->count() - 1);
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    ASSERT_NE(model, nullptr);
    model->item(1)->setEnabled(false);
    EXPECT_FALSE(selectionField().value("choices").toArray()[2].toObject().value("enabled").toBool());
    EXPECT_FALSE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"currentIndex", 1}}}}));
    EXPECT_EQ(combo->currentIndex(), -1);
}

TEST(GalleryComponentPlaygroundIsolationTest, TimeEditsCommitOnlyThePreviewSelection)
{
    QWidget example;
    fluent::date_time::TimePicker source(&example);
    source.setSelectedTime(QTime(9, 30));
    QWidget isolated;
    auto* playground = fluent::gallery::GalleryComponentPlayground::fromWidget(QStringLiteral("time-picker"), &source,
        &isolated);
    ASSERT_NE(playground, nullptr);
    auto* preview = qobject_cast<fluent::date_time::TimePicker*>(playground->widget());
    ASSERT_NE(preview, nullptr);
    QSignalSpy sourceChanges(&source, &fluent::date_time::TimePicker::selectedTimeChanged);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"time", "15:45:00"}}}}));
    EXPECT_EQ(preview->selectedTime(), QTime(15, 45));
    EXPECT_EQ(source.selectedTime(), QTime(9, 30));
    EXPECT_EQ(sourceChanges.count(), 0);
    const auto state = playground->snapshot();
    for (const auto& language : {"cpp", "python"}) {
        const QString code = state.value(language).toString();
        EXPECT_TRUE(code.contains(QStringLiteral("setSelectedTime(")));
        EXPECT_FALSE(code.contains(QStringLiteral("setTime(")));
    }
    ASSERT_TRUE(playground->apply({{"operation", "reset"}}));
    preview = qobject_cast<fluent::date_time::TimePicker*>(playground->widget());
    ASSERT_NE(preview, nullptr);
    EXPECT_EQ(preview->selectedTime(), QTime(14, 30));
}

TEST(GalleryComponentPlaygroundIsolationTest, PickedPreviewDoesNotWriteOrReparentTheOriginal)
{
    QWidget example;
    auto* layout = new QHBoxLayout(&example);
    auto* source = new fluent::basicinput::Button(QStringLiteral("Accent"), &example);
    source->setFluentStyle(fluent::basicinput::Button::Accent);
    source->setCheckable(true);
    source->setChecked(true);
    source->setFocusVisual(true);
    layout->addWidget(source);
    example.resize(320, 100);
    layout->activate();
    const QRect originalGeometry = source->geometry();
    QSignalSpy originalClicks(source, &QAbstractButton::clicked);
    QWidget isolated;
    auto* preview = fluent::gallery::GalleryComponentPlayground::fromButton(source, &isolated);
    ASSERT_NE(preview, nullptr);
    EXPECT_EQ(preview->snapshot().value("values").toObject().value("text"), "Accent");
    EXPECT_TRUE(preview->snapshot().value("modified").toArray().isEmpty());
    ASSERT_TRUE(preview->apply({{"operation", "set"},
        {"values", QJsonObject{{"text", "Confirm"}, {"fluentStyle", "Subtle"}, {"checked", false}}}}));
    ASSERT_TRUE(preview->apply({{"operation", "activate"}}));
    EXPECT_EQ(source->text(), "Accent");
    EXPECT_EQ(source->fluentStyle(), fluent::basicinput::Button::Accent);
    EXPECT_TRUE(source->isChecked());
    EXPECT_EQ(source->geometry(), originalGeometry);
    EXPECT_EQ(source->parentWidget(), &example);
    EXPECT_EQ(originalClicks.count(), 0);
    EXPECT_EQ(example.findChildren<fluent::basicinput::Button*>().size(), 1);
    ASSERT_TRUE(preview->apply({{"operation", "reset"}}));
    EXPECT_EQ(preview->snapshot().value("values").toObject().value("text"), "Accent");
    EXPECT_TRUE(preview->snapshot().value("modified").toArray().isEmpty());
    EXPECT_TRUE(preview->snapshot().value("cpp").toString().contains("setFocusVisual(true)"));
    delete source;
    EXPECT_FALSE(preview->renderPreview(2).isNull());
}

TEST(GalleryComponentPlaygroundIsolationTest, PreviewPaintIsBoundedSharpAndDoesNotShowAWindow)
{
    QWidget isolated;
    auto* preview = fluent::gallery::GalleryComponentPlayground::create("button", &isolated);
    ASSERT_NE(preview, nullptr);
    const auto first = preview->renderPreview(2);
    EXPECT_EQ(first.devicePixelRatio(), 2);
    EXPECT_LE(first.width(), 480);
    EXPECT_LE(first.height(), 104);
    EXPECT_FALSE(preview->widget()->isVisible());
    EXPECT_FALSE(isolated.isVisible());
    ASSERT_TRUE(preview->apply({{"operation", "set"}, {"values", QJsonObject{{"focusVisual", true}}}}));
    EXPECT_NE(first, preview->renderPreview(2));
    EXPECT_FALSE(preview->widget()->hasFocus());
    bool painted = false;
    for (int y = 0; y < first.height(); ++y)
        for (int x = 0; x < first.width(); ++x)
            painted = painted || qAlpha(first.pixel(x, y)) > 0;
    EXPECT_TRUE(painted);
    ASSERT_TRUE(preview->apply(
        {{"operation", "set"}, {"values", QJsonObject{{"text", "Different"}, {"fluentStyle", "Accent"}}}}));
    EXPECT_NE(first, preview->renderPreview(2));
    EXPECT_EQ(preview->renderPreview(100).devicePixelRatio(), 3);
}

namespace {
using fluent::gallery::GalleryComponentPlayground;
using fluent::basicinput::Button;

bool set(GalleryComponentPlayground* playground, const QJsonObject& values)
{
    return playground->apply({{"operation", "set"}, {"values", values}});
}

TEST(GalleryComponentPlaygroundIsolationTest, FocusPreviewMatchesRealFocusWithoutMovingItOrChangingTheSource)
{
    const auto previousTheme = fluent::FluentElement::currentTheme();
    const QPoint previousCursor = QCursor::pos();
    const auto restore = qScopeGuard([&] {
        fluent::FluentElement::setTheme(previousTheme);
        QCursor::setPos(previousCursor);
    });
    QCursor::setPos(QPoint(0, 0));
    QWidget window;
    Button source(QStringLiteral("Focus"), &window);
    source.move(40, 40);
    window.resize(300, 160);
    window.show();
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
    source.setFocus(Qt::TabFocusReason);
    ASSERT_TRUE(source.hasFocus());
    QSignalSpy focusChanges(qApp, &QApplication::focusChanged);

    for (auto theme :
        {fluent::FluentElement::Light, fluent::FluentElement::Dark, fluent::FluentElement::HighContrast}) {
        SCOPED_TRACE(theme);
        fluent::FluentElement::setTheme(theme);
        for (auto style : {Button::Standard, Button::Accent, Button::Subtle}) {
            SCOPED_TRACE(style);
            source.setFluentStyle(style);
            for (qreal opacity : {1.0, 0.5}) {
                source.setContentOpacity(opacity);
                source.setFocusVisual(true);
                QWidget isolated;
                auto* preview = GalleryComponentPlayground::fromButton(&source, &isolated);
                ASSERT_NE(preview, nullptr);
                for (auto state : {Button::Rest, Button::Hover, Button::Pressed, Button::Disabled}) {
                    SCOPED_TRACE(state);
                    source.setInteractionState(state);
                    preview->widget()->setProperty("interactionState", state);
                    for (qreal dpr : {1.0, 2.0}) {
                        ASSERT_TRUE(set(preview, {{"focusVisual", true}}));
                        const QImage focused = preview->renderPreview(dpr);
                        source.resize(preview->widget()->size());
                        QImage actual(focused.size(), focused.format());
                        actual.setDevicePixelRatio(dpr);
                        actual.fill(Qt::transparent);
                        QPainter painter(&actual);
                        source.render(&painter, QPoint(2, 2), QRegion(), QWidget::DrawChildren);
                        painter.end();
                        EXPECT_EQ(focused, actual);
                        ASSERT_TRUE(set(preview, {{"focusVisual", false}}));
                        const QImage withoutFocus = preview->renderPreview(dpr);
                        if (state == Button::Disabled)
                            EXPECT_EQ(focused, withoutFocus);
                        else
                            EXPECT_NE(focused, withoutFocus);
                        ASSERT_TRUE(set(preview, {{"focusVisual", true}}));
                        EXPECT_EQ(focused, preview->renderPreview(dpr));
                    }
                }
                source.setInteractionState(Button::Rest);
                preview->widget()->setProperty("interactionState", Button::Rest);
                ASSERT_TRUE(set(preview, {{"enabled", false}, {"focusVisual", true}}));
                const QImage disabled = preview->renderPreview(2);
                ASSERT_TRUE(set(preview, {{"focusVisual", false}}));
                EXPECT_EQ(disabled, preview->renderPreview(2));
                EXPECT_TRUE(source.hasFocusVisual());
                EXPECT_TRUE(source.isEnabled());
                EXPECT_TRUE(source.hasFocus());
                EXPECT_FALSE(preview->widget()->hasFocus());
                EXPECT_FALSE(isolated.isVisible());
            }
        }
    }
    EXPECT_EQ(focusChanges.count(), 0);
}

TEST(GalleryComponentPlaygroundTest, DefaultsDescribeTheActualButtonAndOnlySupportedRoutes)
{
    QWidget parent;
    EXPECT_EQ(GalleryComponentPlayground::create("unknown", &parent), nullptr);
    EXPECT_EQ(GalleryComponentPlayground::create("button", nullptr), nullptr);
    auto* playground = GalleryComponentPlayground::create("button", &parent);
    ASSERT_NE(playground, nullptr);
    auto* button = qobject_cast<Button*>(playground->widget());
    ASSERT_NE(button, nullptr);
    const auto state = playground->snapshot();
    EXPECT_EQ(button->text(), "Continue");
    EXPECT_EQ(button->fluentStyle(), Button::Standard);
    EXPECT_TRUE(button->isEnabled());
    EXPECT_TRUE(state.value("modified").toArray().isEmpty());
    EXPECT_EQ(state.value("preset"), "default");
    EXPECT_TRUE(state.value("values").toObject().value("cornerRadius").isNull());
    for (const auto& entry : state.value("schema").toObject().value("properties").toArray()) {
        const auto field = entry.toObject();
        EXPECT_EQ(state.value("values").toObject().value(field.value("id").toString()), field.value("default"));
        if (field.contains("property")) {
            const auto property = button->metaObject()->property(
                button->metaObject()->indexOfProperty(field.value("property").toString().toLatin1().constData()));
            EXPECT_TRUE(property.isValid());
            EXPECT_TRUE(property.isWritable());
        }
    }
}

TEST(GalleryComponentPlaygroundTest, InvalidPatchesAreRejectedBeforeAnyPropertyChanges)
{
    QWidget parent;
    auto* playground = GalleryComponentPlayground::create("button", &parent);
    ASSERT_NE(playground, nullptr);
    const auto before = playground->snapshot();
    QSignalSpy changes(playground, &GalleryComponentPlayground::stateChanged);
    const QList<QJsonObject> invalid = {{{"text", "Changed"}, {"fluentStyle", "Unknown"}},
        {{"text", "Changed"}, {"styleSheet", "background:red"}}, {{"enabled", "false"}}, {{"icon", "../../other"}},
        {{"contentOpacity", "0.5"}}, {{"contentOpacity", QJsonValue::Null}}, {{"text", QString(161, QLatin1Char('x'))}},
        {{"cornerRadius", QJsonObject{}}}};
    for (const auto& values : invalid) {
        QString error;
        EXPECT_FALSE(playground->apply({{"operation", "set"}, {"values", values}}, &error));
        EXPECT_FALSE(error.isEmpty());
        EXPECT_EQ(playground->snapshot(), before);
    }
    EXPECT_FALSE(playground->apply({{"operation", "invoke"}, {"method", "deleteLater"}}));
    EXPECT_FALSE(playground->apply({{"operation", "preset"}, {"preset", "missing"}}));
    EXPECT_FALSE(playground->apply({{"operation", "reset"}, {"property", "styleSheet"}}));
    EXPECT_FALSE(playground->apply({{"operation", "set"}, {"values", QJsonArray{}}}));
    EXPECT_EQ(changes.count(), 0);
}

TEST(GalleryComponentPlaygroundTest, ClampingAndDependenciesAreReadBackBeforeCodeGeneration)
{
    QWidget parent;
    auto* playground = GalleryComponentPlayground::create("button", &parent);
    ASSERT_NE(playground, nullptr);
    auto* button = qobject_cast<Button*>(playground->widget());
    ASSERT_TRUE(set(playground,
        {{"contentOpacity", 5}, {"iconScale", 100}, {"cornerRadius", -10}, {"checkable", true}, {"checked", true}}));
    EXPECT_EQ(button->contentOpacity(), 1.0);
    EXPECT_EQ(button->iconScale(), 2.0);
    EXPECT_EQ(button->cornerRadii(), QMargins(0, 0, 0, 0));
    EXPECT_TRUE(button->isChecked());
    auto state = playground->snapshot();
    EXPECT_EQ(state.value("values").toObject().value("iconScale"), 2);
    EXPECT_TRUE(state.value("cpp").toString().contains("setIconScale(2)"));
    EXPECT_FALSE(state.value("cpp").toString().contains("setContentOpacity"));
    EXPECT_FALSE(state.value("cpp").toString().contains("100"));
    ASSERT_TRUE(set(playground, {{"checkable", false}}));
    EXPECT_FALSE(button->isChecked());
    EXPECT_FALSE(playground->snapshot().value("values").toObject().value("checked").toBool());
    EXPECT_FALSE(playground->snapshot().value("cpp").toString().contains("setChecked"));
}

TEST(GalleryComponentPlaygroundTest, RadiusResetRestoresThemeFollowingAndFullResetRestoresExample)
{
    QWidget parent;
    auto* playground = GalleryComponentPlayground::create("button", &parent);
    ASSERT_NE(playground, nullptr);
    auto* button = qobject_cast<Button*>(playground->widget());
    Button reference;
    ASSERT_TRUE(set(playground, {{"cornerRadius", 18}, {"text", "Changed"}, {"fluentStyle", "Accent"}}));
    ASSERT_TRUE(playground->apply({{"operation", "reset"}, {"property", "cornerRadius"}}));
    EXPECT_EQ(button->cornerRadii(), reference.cornerRadii());
    EXPECT_TRUE(playground->snapshot().value("values").toObject().value("cornerRadius").isNull());
    EXPECT_FALSE(playground->snapshot().value("cpp").toString().contains("setCornerRadii"));
    EXPECT_EQ(button->text(), "Changed");
    ASSERT_TRUE(playground->apply({{"operation", "reset"}}));
    EXPECT_TRUE(playground->snapshot().value("modified").toArray().isEmpty());
    EXPECT_EQ(button->text(), "Continue");
    EXPECT_EQ(button->fluentStyle(), Button::Standard);
}

TEST(GalleryComponentPlaygroundTest, PresetsExportCurrentPublicCppAndPythonApis)
{
    QWidget parent;
    auto* playground = GalleryComponentPlayground::create("button", &parent);
    ASSERT_NE(playground, nullptr);
    ASSERT_TRUE(playground->apply({{"operation", "preset"}, {"preset", "with-icon"}}));
    auto* button = qobject_cast<Button*>(playground->widget());
    EXPECT_EQ(button->fluentStyle(), Button::Accent);
    EXPECT_EQ(button->fluentLayout(), Button::IconBefore);
    const auto state = playground->snapshot();
    EXPECT_EQ(state.value("preset"), "with-icon");
    EXPECT_TRUE(state.value("cpp").toString().contains("setIconGlyph(Typography::Icons::Add)"));
    EXPECT_TRUE(state.value("python").toString().contains("fluentqt.Button.ButtonStyle.Accent"));
    EXPECT_TRUE(state.value("python").toString().contains("setIconGlyph(fluentqt.Typography.Icons.Add)"));
    ASSERT_TRUE(set(playground, {{"enabled", false}}));
    EXPECT_EQ(playground->snapshot().value("preset"), "custom");
    EXPECT_TRUE(playground->snapshot().value("cpp").toString().contains("setEnabled(false)"));
}

TEST(GalleryComponentPlaygroundTest, ActualEventsStayBoundedAndDisabledPaintDoesNotDisableInput)
{
    QWidget parent;
    auto* playground = GalleryComponentPlayground::create("button", &parent);
    ASSERT_NE(playground, nullptr);
    auto* button = qobject_cast<Button*>(playground->widget());
    ASSERT_TRUE(set(playground, {{"interactionState", "Disabled"}}));
    EXPECT_TRUE(button->isEnabled());
    for (int i = 0; i < 12; ++i)
        button->click();
    auto events = playground->snapshot().value("events").toArray();
    ASSERT_EQ(events.size(), 8);
    EXPECT_EQ(events.last().toObject().value("sequence"), 12);
    ASSERT_TRUE(set(playground, {{"enabled", false}}));
    button->click();
    EXPECT_EQ(playground->snapshot().value("events"), events);
    ASSERT_TRUE(playground->apply({{"operation", "reset"}}));
    EXPECT_TRUE(playground->snapshot().value("events").toArray().isEmpty());
}

TEST(GalleryComponentPlaygroundTest, EscapesUserTextAndExportsAReproducibleConfiguration)
{
    QWidget parent;
    auto* playground = GalleryComponentPlayground::create("button", &parent);
    ASSERT_NE(playground, nullptr);
    const QString text = QString::fromUtf8("同步 \"draft\"\\path\n<script>测试</script> 😀");
    ASSERT_TRUE(set(playground,
        {{"text", text}, {"icon", "Save"}, {"fluentStyle", "Accent"}, {"cornerRadius", 12}, {"contentOpacity", 0.6},
            {"iconOffsetX", -2}, {"iconOffsetY", 3}, {"checkable", true}, {"checked", true}}));
    const auto state = playground->snapshot();
    EXPECT_EQ(qobject_cast<Button*>(playground->widget())->text(), text);
    EXPECT_TRUE(state.value("cpp").toString().contains("\\\"draft\\\"\\\\path\\n"));
    EXPECT_TRUE(state.value("python").toString().contains("\\\"draft\\\"\\\\path\\n"));
    EXPECT_TRUE(state.value("cpp").toString().contains("setIconOffset(QPoint(-2, 3))"));
    EXPECT_TRUE(state.value("python").toString().contains("from PySide6.QtCore import QMargins, QPoint"));
    const QString output = qEnvironmentVariable("FLUENT_QT_PLAYGROUND_EXPORT_DIR");
    if (!output.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(output));
        for (const QString language : {QStringLiteral("cpp"), QStringLiteral("python")}) {
            QFile file(output + QLatin1Char('/') + language + QStringLiteral(".txt"));
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(state.value(language).toString().toUtf8());
        }
        QFile file(output + QStringLiteral("/state.json"));
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(state).toJson());
    }
}

TEST(GalleryComponentPlaygroundTest, PreviewOwnsItsObjectsAndDestroyedControlRejectsFurtherRequests)
{
    auto* parent = new QWidget;
    QPointer<GalleryComponentPlayground> playground = GalleryComponentPlayground::create("button", parent);
    ASSERT_NE(playground, nullptr);
    QPointer<QWidget> button = playground->widget();
    delete button.data();
    EXPECT_TRUE(playground->snapshot().isEmpty());
    EXPECT_FALSE(set(playground, {{"enabled", true}}));
    delete parent;
    EXPECT_TRUE(playground.isNull());
    EXPECT_TRUE(button.isNull());
}

TEST(GalleryComponentPlaygroundTest, IconPlacementTextOnlyHidesIconAndGeneratesExplicitLayoutCode)
{
    QWidget parent;
    auto* playground = GalleryComponentPlayground::create("button", &parent);
    ASSERT_NE(playground, nullptr);
    auto* button = qobject_cast<Button*>(playground->widget());
    ASSERT_NE(button, nullptr);

    ASSERT_TRUE(set(playground, {{"icon", "CheckMark"}}));
    EXPECT_EQ(button->fluentLayout(), Button::IconBefore);
    EXPECT_TRUE(
        playground->snapshot().value("cpp").toString().contains("button->setFluentLayout(Button::IconBefore);"));

    ASSERT_TRUE(set(playground, {{"fluentLayout", "IconAbove"}}));
    EXPECT_EQ(button->fluentLayout(), Button::IconAbove);
    EXPECT_TRUE(playground->snapshot().value("cpp").toString().contains("button->setFluentLayout(Button::IconAbove);"));
    EXPECT_TRUE(playground->snapshot().value("python").toString().contains(
        "button.setFluentLayout(fluentqt.Button.ButtonLayout.IconAbove)"));

    ASSERT_TRUE(set(playground, {{"fluentLayout", "IconBelow"}}));
    EXPECT_EQ(button->fluentLayout(), Button::IconBelow);
    EXPECT_TRUE(playground->snapshot().value("cpp").toString().contains("button->setFluentLayout(Button::IconBelow);"));
    EXPECT_TRUE(playground->snapshot().value("python").toString().contains(
        "button.setFluentLayout(fluentqt.Button.ButtonLayout.IconBelow)"));

    ASSERT_TRUE(set(playground, {{"fluentLayout", "TextOnly"}}));
    EXPECT_EQ(button->fluentLayout(), Button::TextOnly);
    EXPECT_TRUE(playground->snapshot().value("cpp").toString().contains("button->setFluentLayout(Button::TextOnly);"));
    EXPECT_TRUE(playground->snapshot().value("python").toString().contains(
        "button.setFluentLayout(fluentqt.Button.ButtonLayout.TextOnly)"));

    ASSERT_TRUE(set(playground, {{"icon", "None"}}));
    EXPECT_FALSE(playground->snapshot().value("cpp").toString().contains("setFluentLayout"));
}

TEST(GalleryComponentPlaygroundTest, RemovingIconsRestoresTextAndReplacingIconsPreservesExplicitTextOnly)
{
    QWidget parent;
    auto* playground = GalleryComponentPlayground::create("button", &parent);
    const QImage textOnly = playground->renderPreview(2);
    for (const auto& layout : {"TextOnly", "IconBefore", "IconAfter", "IconOnly", "IconAbove", "IconBelow"}) {
        SCOPED_TRACE(layout);
        ASSERT_TRUE(set(playground, {{"icon", "Save"}, {"fluentLayout", layout}}));
        ASSERT_TRUE(set(playground, {{"icon", "None"}}));
        const auto state = playground->snapshot();
        EXPECT_EQ(state.value("values").toObject().value("fluentLayout"), "TextOnly");
        EXPECT_EQ(playground->renderPreview(2), textOnly);
        EXPECT_FALSE(state.value("cpp").toString().contains("setFluentLayout"));
        EXPECT_FALSE(state.value("python").toString().contains("setIconGlyph"));
    }
    ASSERT_TRUE(set(playground, {{"icon", "Save"}, {"fluentLayout", "TextOnly"}}));
    ASSERT_TRUE(set(playground, {{"icon", "CheckMark"}}));
    EXPECT_EQ(playground->snapshot().value("values").toObject().value("fluentLayout"), "TextOnly");
    EXPECT_EQ(playground->renderPreview(2), textOnly);
}

TEST(GalleryComponentPlaygroundTest, PresetSelectionAndReadbackAgreeForPickedButtons)
{
    QWidget parent;
    Button original(QStringLiteral("Picked"));
    original.setFluentStyle(Button::Accent);
    original.setContentOpacity(0.5);
    auto* playground = GalleryComponentPlayground::fromButton(&original, &parent);
    const auto initial = playground->snapshot();
    for (const auto& id : {"primary", "subtle", "with-icon", "default"}) {
        SCOPED_TRACE(id);
        ASSERT_TRUE(playground->apply({{"operation", "preset"}, {"preset", id}}));
        EXPECT_EQ(playground->snapshot().value("preset"), id);
    }
    EXPECT_EQ(playground->snapshot().value("values"), initial.value("values"));
    EXPECT_EQ(playground->snapshot().value("cpp"), initial.value("cpp"));
    EXPECT_EQ(playground->snapshot().value("python"), initial.value("python"));
}
} // namespace

TEST(GalleryComponentPlaygroundTest, RegisteredControlsKeepTheirOwnPropertiesAndExportBothLanguages)
{
    QJsonArray cases;
    QDir descriptors(QStringLiteral(":/app/playgrounds"));
    for (const auto& filename : descriptors.entryList({QStringLiteral("*.json")}, QDir::Files)) {
        const auto route = filename.chopped(5);
        if (route == QStringLiteral("button") || !fluent::gallery::playgroundRoutes().contains(route))
            continue;
        SCOPED_TRACE(route.toStdString());
        QWidget isolated;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create(route, &isolated);
        ASSERT_NE(playground, nullptr);
        const auto initial = playground->snapshot();
        const auto initialImage = playground->renderPreview(2);
        ASSERT_FALSE(initialImage.isNull());
        EXPECT_FALSE(isolated.isVisible());
        EXPECT_FALSE(playground->widget()->isVisible());
        EXPECT_GE(playground->widget()->height(), playground->widget()->minimumSizeHint().height());
        EXPECT_TRUE(initial.value("cpp").toString().contains(QStringLiteral("control")));
        EXPECT_TRUE(initial.value("python").toString().contains(QStringLiteral("control")));
        cases.append(QJsonObject{{"case", route + QStringLiteral("/default")}, {"state", initial}});
        for (const auto& entry : initial.value("schema").toObject().value("properties").toArray()) {
            const auto field = entry.toObject();
            const auto id = field.value("id").toString();
            SCOPED_TRACE(id.toStdString());
            QJsonArray variants;
            const auto kind = field.value("type").toString();
            if (kind == QStringLiteral("enum") && field.contains("enumValues")) {
                QSet<int> unique;
                const auto numbers = field.value("enumValues").toObject();
                for (const auto& option : field.value("options").toArray()) {
                    const int number = numbers.value(option.toString()).toInt();
                    EXPECT_FALSE(unique.contains(number)) << "Duplicate semantic enum choice";
                    unique.insert(number);
                }
            }
            if (kind == QStringLiteral("table")) {
                auto data = initial.value("values").toObject().value(id).toArray();
                if (!data.isEmpty()) {
                    auto row = data[0].toArray();
                    if (field.value("adapter") == "chart-data")
                        row[1] = row[1].toDouble() + 10;
                    else
                        row[0] = QStringLiteral("Edited row");
                    data[0] = row;
                }
                variants.append(data);
                if (!field.value("fixedRows").toBool())
                    variants.append(QJsonArray{});
            } else if (kind == QStringLiteral("multi-enum"))
                variants = {QJsonArray{}, field.value("options").toArray()};
            else if (kind == QStringLiteral("string-list"))
                variants = {QJsonArray{}, QJsonArray{QStringLiteral("First"), QStringLiteral("Second")}};
            else if (kind == QStringLiteral("enum"))
                variants = field.value("options").toArray();
            else if (kind == QStringLiteral("boolean"))
                variants = {true, false};
            else if (kind == QStringLiteral("number"))
                variants = {field.value("minimum"), field.value("maximum")};
            else if (kind == QStringLiteral("color"))
                variants = {QStringLiteral("#ffff6600")};
            else if (kind == QStringLiteral("date")) {
                variants = {QStringLiteral("2026-08-12")};
                if (field.value("nullable").toBool())
                    variants.append(QJsonValue(QJsonValue::Null));
            } else if (kind == QStringLiteral("datetime"))
                variants = {QStringLiteral("2026-08-12T15:30:00"), QJsonValue(QJsonValue::Null)};
            else if (kind == QStringLiteral("time"))
                variants = {QStringLiteral("15:30:00")};
            else if (!field.value("options").toArray().isEmpty())
                variants = field.value("options").toArray();
            else
                variants = {QStringLiteral("Edited \"sample\"\\value").left(field.value("maximumLength").toInt(512))};
            for (const auto& value : variants) {
                ASSERT_TRUE(playground->apply({{"operation", "reset"}}));
                EXPECT_EQ(playground->snapshot().value("values"), initial.value("values"));
                QString error;
                if (field.contains("effectFixture"))
                    ASSERT_TRUE(
                        playground->apply({{"operation", "set"}, {"values", field.value("effectFixture")}}, &error))
                        << error.toStdString();
                if (field.value("previewInteraction") == QStringLiteral("hover"))
                    ASSERT_TRUE(playground->apply({{"operation", "hover"}, {"hovered", true}}));
                const auto effectImage = playground->renderPreview(2);
                const auto effectBefore = playground->snapshot();
                ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{id, value}}}}, &error))
                    << error.toStdString();
                const auto state = playground->snapshot();
                if (kind == QStringLiteral("enum") || kind == QStringLiteral("number"))
                    EXPECT_EQ(state.value("values").toObject().value(id), value)
                        << "An offered choice must not silently become another value";
                EXPECT_EQ(state.value("values").toObject().size(), initial.value("values").toObject().size());
                auto changedImage = playground->renderPreview(2);
                if (field.value("adapter") == "chart-data"
                    && state.value("values").toObject().value(id) != effectBefore.value("values").toObject().value(id))
                    EXPECT_TRUE(QTest::qWaitFor(
                        [&] {
                            changedImage = playground->renderPreview(2);
                            return changedImage != effectImage;
                        },
                        1000));
                EXPECT_FALSE(changedImage.isNull());
                const bool valueChanged = state.value("values").toObject().value(id)
                    != effectBefore.value("values").toObject().value(id);
                // A successful setter is insufficient: an unchanged image needs an explained
                // interaction/visibility condition. Enabled also governs nonvisual input policy.
                // zh_CN: setter 成功不代表视觉验证通过；画面未变时必须说明交互或可见条件。
                if (valueChanged && changedImage == effectImage && id != QStringLiteral("enabled")) {
                    EXPECT_TRUE(field.contains("effectHint") || field.contains("previewInteraction"))
                        << "Changed property has no visible effect or explanation: " << route.toStdString() << '/'
                        << id.toStdString();
                }
                EXPECT_EQ(playground->snapshot().value("values"), state.value("values"))
                    << "Rendering changed properties";
                cases.append(QJsonObject{{"case", route + QLatin1Char('/') + id}, {"state", state},
                    {"requested", value}, {"valueChanged", valueChanged},
                    {"pixelsChanged", changedImage != effectImage},
                    {"renderStableBeforeEdit",
                        field.contains("effectFixture") || field.contains("previewInteraction")
                            || effectImage == initialImage},
                    {"cppChanged", state.value("cpp") != effectBefore.value("cpp")},
                    {"pythonChanged", state.value("python") != effectBefore.value("python")},
                    {"effectFixture", field.value("effectFixture")}});
                const auto before = playground->snapshot();
                EXPECT_FALSE(playground->apply(
                    {{"operation", "set"}, {"values", QJsonObject{{id, value}, {"styleSheet", "invalid"}}}}));
                EXPECT_EQ(before, playground->snapshot());
            }
        }
        ASSERT_TRUE(playground->apply({{"operation", "reset"}}));
        EXPECT_EQ(playground->snapshot().value("values"), initial.value("values"));
        EXPECT_EQ(playground->renderPreview(2), initialImage);
    }
    EXPECT_GT(cases.size(), 100);
    const auto output = qEnvironmentVariable("FLUENT_QT_PLAYGROUND_AUDIT_DIR");
    if (!output.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(output));
        QFile file(output + QStringLiteral("/property-audit.json"));
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject{{"theme", "light"}, {"cases", cases}}).toJson());
    }
}

TEST(GalleryComponentPlaygroundIsolationTest, CompositeInputSnapshotsDoNotTakeFocusFromTheSource)
{
    QWidget example;
    fluent::basicinput::Button source(QStringLiteral("Keep focus here"), &example);
    example.resize(320, 100);
    example.show();
    example.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&example));
    source.setFocus(Qt::TabFocusReason);
    ASSERT_TRUE(source.hasFocus());
    QSignalSpy focusChanges(qApp, &QApplication::focusChanged);
    for (const auto& route : {"code-block", "text-edit", "tag-input", "number-box"}) {
        SCOPED_TRACE(route);
        QWidget isolated(&example, Qt::Window);
        auto* editor = fluent::gallery::GalleryComponentPlayground::create(route, &isolated);
        ASSERT_NE(editor, nullptr);
        EXPECT_FALSE(editor->renderPreview(2).isNull());
        EXPECT_TRUE(editor->apply({{"operation", "reset"}}));
        EXPECT_FALSE(editor->renderPreview(2).isNull());
        EXPECT_FALSE(isolated.isVisible());
        EXPECT_FALSE(editor->widget()->isVisible());
        EXPECT_TRUE(source.hasFocus());
    }
    EXPECT_EQ(focusChanges.count(), 0);
}

TEST(GalleryComponentPlaygroundTest, FixedVisualAuditPropertiesProducePixelDifferences)
{
    const auto expectDiffAfter =
        [](const QString& route, const QJsonObject& setup, const QJsonObject& changed, bool hoverPreview = false) {
            SCOPED_TRACE(route.toStdString());
            QWidget isolated;
            auto* playground = fluent::gallery::GalleryComponentPlayground::create(route, &isolated);
            ASSERT_NE(playground, nullptr);
            QString error;
            if (!setup.isEmpty())
                ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", setup}}, &error))
                    << error.toStdString();
            if (hoverPreview)
                ASSERT_TRUE(playground->apply({{"operation", "hover"}, {"hovered", true}}, &error))
                    << error.toStdString();
            const QImage before = playground->renderPreview(2);
            ASSERT_FALSE(before.isNull());
            const auto scrollState = [&] {
                QJsonArray bars;
                for (auto* bar : playground->widget()->findChildren<QScrollBar*>())
                    bars.append(QJsonObject{{"class", bar->metaObject()->className()}, {"minimum", bar->minimum()},
                        {"maximum", bar->maximum()}, {"hidden", bar->isHidden()},
                        {"opacity", QJsonValue::fromVariant(bar->property("opacity"))}, {"width", bar->width()},
                        {"height", bar->height()}});
                return QJsonDocument(bars).toJson(QJsonDocument::Compact).toStdString();
            };
            const auto beforeScrollState = scrollState();
            ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", changed}}, &error)) << error.toStdString();
            const QImage after = playground->renderPreview(2);
            ASSERT_FALSE(after.isNull());
            EXPECT_NE(after, before)
                << "Before scroll state: " << beforeScrollState << "\nAfter scroll state: " << scrollState();
            EXPECT_FALSE(isolated.isVisible());
            EXPECT_FALSE(playground->widget()->isVisible());
        };

    expectDiffAfter(QStringLiteral("line-chart"), {},
        QJsonObject{{QStringLiteral("valueSuffix"), QStringLiteral(" ms")}}, true);
    expectDiffAfter(QStringLiteral("multi-select-combobox"), {}, QJsonObject{{QStringLiteral("isOpen"), false}});
    expectDiffAfter(QStringLiteral("multi-select-combobox"),
        QJsonObject{{QStringLiteral("isOpen"), true}, {QStringLiteral("searchEnabled"), true}},
        QJsonObject{{QStringLiteral("searchEnabled"), false}});
    expectDiffAfter(QStringLiteral("flip-view"), {}, QJsonObject{{QStringLiteral("showNavigationButtons"), false}},
        true);
    expectDiffAfter(QStringLiteral("flow-view"), QJsonObject{{QStringLiteral("verticalSpacing"), 4}},
        QJsonObject{{QStringLiteral("verticalSpacing"), 24}});
    expectDiffAfter(QStringLiteral("code-block"), QJsonObject{{QStringLiteral("maxVisibleLines"), 2}},
        QJsonObject{{QStringLiteral("maxVisibleLines"), 10}});
    expectDiffAfter(QStringLiteral("text-edit"),
        QJsonObject{{QStringLiteral("autoResizeEnabled"), true}, {QStringLiteral("maxVisibleLines"), 3}},
        QJsonObject{{QStringLiteral("autoResizeEnabled"), true}, {QStringLiteral("maxVisibleLines"), 12}});
    expectDiffAfter(QStringLiteral("donut-chart"), QJsonObject{{QStringLiteral("centerText"), QStringLiteral("48%")}},
        QJsonObject{{QStringLiteral("centerText"), QStringLiteral("92%")}});
    expectDiffAfter(QStringLiteral("field"),
        QJsonObject{{QStringLiteral("validationMessage"), QString()},
            {QStringLiteral("helperText"), QStringLiteral("Helper")}},
        QJsonObject{{QStringLiteral("validationMessage"), QString()},
            {QStringLiteral("helperText"), QStringLiteral("Updated helper")}});
    expectDiffAfter(QStringLiteral("combobox"),
        QJsonObject{{QStringLiteral("currentIndex"), -1}, {QStringLiteral("editable"), false},
            {QStringLiteral("placeholderText"), QString()}},
        QJsonObject{{QStringLiteral("currentIndex"), -1}, {QStringLiteral("editable"), false},
            {QStringLiteral("placeholderText"), QStringLiteral("Choose one")}});
    expectDiffAfter(QStringLiteral("flyout"), QJsonObject{{"placement", "Top"}}, QJsonObject{{"placement", "Bottom"}});
    expectDiffAfter(QStringLiteral("flyout"), QJsonObject{{"anchorOffset", 0}}, QJsonObject{{"anchorOffset", 24}});
    expectDiffAfter(QStringLiteral("label"),
        QJsonObject{{"text", "A long label that extends beyond its available preview width"}, {"wordWrap", false},
            {"textElideMode", "ElideNone"}},
        QJsonObject{{"textElideMode", "ElideRight"}});
    expectDiffAfter(QStringLiteral("tree-view"), QJsonObject{{"horizontalFluentScrollBarEnabled", true}},
        QJsonObject{{"horizontalFluentScrollBarEnabled", false}}, true);
    expectDiffAfter(QStringLiteral("scroll-view"), QJsonObject{{"verticalScrollBarVisibility", "Visible"}},
        QJsonObject{{"verticalScrollBarVisibility", "Hidden"}}, true);
    expectDiffAfter(QStringLiteral("scroll-view"), QJsonObject{{"verticalScrollBarVisibility", "Auto"}},
        QJsonObject{{"verticalScrollBarVisibility", "Hidden"}}, true);
    expectDiffAfter(QStringLiteral("scroll-view"), QJsonObject{{"horizontalScrollBarVisibility", "Auto"}},
        QJsonObject{{"horizontalScrollBarVisibility", "Hidden"}}, true);
}

TEST(GalleryComponentPlaygroundInteractionTest, PopupGeometryKeepsCanvasAndControlScaleIndependent)
{
    QWidget host;
    host.resize(680, 640);
    host.show();
    auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("combobox", &host);
    ASSERT_NE(playground, nullptr);
    auto* combo = qobject_cast<QComboBox*>(playground->widget());
    ASSERT_NE(combo, nullptr);
    const auto before = playground->interactiveViewport(&host);
    const auto geometry = combo->geometry();
    const auto state = playground->snapshot();
    EXPECT_EQ(playground->interactiveViewport(&host, false), before);
    EXPECT_EQ(playground->snapshot(), state);
    combo->showPopup();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return playground->interactiveViewport(&host, false).value("hasOpenOverlay").toBool();
    }));
    const auto opened = playground->interactiveViewport(&host, false);
    EXPECT_EQ(opened.value("width"), before.value("width"));
    EXPECT_EQ(opened.value("displayWidth"), before.value("displayWidth"));
    EXPECT_EQ(opened.value("target"), before.value("target"));
    EXPECT_EQ(combo->geometry(), geometry);
    combo->hidePopup();
    EXPECT_FALSE(playground->interactiveViewport(&host, false).value("hasOpenOverlay").toBool());
}

TEST(GalleryComponentPlaygroundInteractionTest, NativeButtonInputPublishesActualStateAndKeepsOriginalUntouched)
{
    fluent::basicinput::Button original(QStringLiteral("Original"));
    original.setCheckable(true);
    QWidget host;
    host.resize(400, 640);
    host.show();
    auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("button", &host);
    ASSERT_NE(playground, nullptr);
    auto* button = qobject_cast<fluent::basicinput::Button*>(playground->widget());
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"checkable", true}}}}));
    playground->interactiveViewport(&host);
    QSignalSpy changed(playground, &fluent::gallery::GalleryComponentPlayground::stateChanged);
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_TRUE(playground->snapshot().value("values").toObject().value("checked").toBool());
    EXPECT_TRUE(playground->snapshot().value("cpp").toString().contains("setChecked(true)"));
    EXPECT_GT(changed.size(), 0);
    EXPECT_FALSE(original.isChecked());
    EXPECT_EQ(original.text(), QStringLiteral("Original"));
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"enabled", false}}}}));
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_TRUE(button->isChecked());
    EXPECT_TRUE(playgroundField(playground->snapshot(), "interactionState").isEmpty());
}

TEST(GalleryComponentPlaygroundInteractionTest, NativeTextInputKeepsFocusAndRespectsReadOnlyAndDisabled)
{
    QWidget host;
    host.resize(400, 640);
    host.show();
    auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("line-edit", &host);
    ASSERT_NE(playground, nullptr);
    auto* edit = qobject_cast<QLineEdit*>(playground->widget());
    ASSERT_NE(edit, nullptr);
    playground->interactiveViewport(&host);
    edit->setFocus();
    edit->selectAll();
    QSignalSpy changed(playground, &fluent::gallery::GalleryComponentPlayground::stateChanged);
    QTest::keyClicks(edit, "Native typing");
    QTest::qWait(100);
    EXPECT_EQ(playground->snapshot().value("values").toObject().value("text"), "Native typing");
    EXPECT_TRUE(playground->snapshot().value("python").toString().contains("Native typing"));
    EXPECT_NE(edit->focusPolicy(), Qt::NoFocus);
    EXPECT_GT(changed.size(), 0);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"readOnly", true}}}}));
    QTest::keyClicks(edit, " blocked");
    EXPECT_EQ(edit->text(), QStringLiteral("Native typing"));
    ASSERT_TRUE(
        playground->apply({{"operation", "set"}, {"values", QJsonObject{{"readOnly", false}, {"enabled", false}}}}));
    EXPECT_FALSE(edit->isEnabled());
}

TEST(GalleryComponentPlaygroundInteractionTest, EditableComboRetainsUserTextAndInsertedModelItems)
{
    QWidget host;
    host.resize(400, 640);
    host.show();
    auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("combobox", &host);
    ASSERT_NE(playground, nullptr);
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"editable", true}}}}));
    auto* combo = qobject_cast<QComboBox*>(playground->widget());
    ASSERT_NE(combo, nullptr);
    playground->interactiveViewport(&host);
    combo->lineEdit()->selectAll();
    QTest::keyClicks(combo->lineEdit(), "New item");
    EXPECT_EQ(playground->snapshot().value("values").toObject().value("currentText"), "New item");
    QTest::keyClick(combo->lineEdit(), Qt::Key_Return);
    const auto state = playground->snapshot();
    EXPECT_TRUE(state.value("values").toObject().value("items").toArray().contains("New item"));
    EXPECT_TRUE(state.value("cpp").toString().contains("addItems(QStringList{"));
    EXPECT_EQ(playgroundField(state, "currentIndex").value("choices").toArray().size(), combo->count() + 1);
}

TEST(GalleryComponentPlaygroundInteractionTest, EveryLiveFixtureUsesAnIndependentOwnerAndSurvivesReset)
{
    for (const auto& route : fluent::gallery::playgroundRoutes()) {
        SCOPED_TRACE(route.toStdString());
        QWidget host;
        host.resize(680, 640);
        auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive(route, &host);
        ASSERT_NE(playground, nullptr);
        const auto initial = playground->snapshot();
        EXPECT_TRUE(initial.value("schema").toObject().value("interactive").toBool());
        QString error;
        ASSERT_TRUE(playground->initializeInteractive(initial.value("values").toObject(), &error))
            << error.toStdString();
        const auto viewport = playground->interactiveViewport(&host);
        EXPECT_TRUE(viewport.value("live").toBool());
        EXPECT_FALSE(viewport.contains("url"));
        ASSERT_TRUE(playground->apply({{"operation", "reset"}}, &error)) << error.toStdString();
        EXPECT_FALSE(playground->snapshot().value("cpp").toString().isEmpty());
        QPointer<QWidget> target(playground->widget());
        delete playground;
        // QObject owner retains the independent surface until its host is destroyed.
        EXPECT_TRUE(target && host.findChildren<QWidget*>().contains(target));
    }
}

TEST(GalleryComponentPlaygroundInteractionTest, ResetDoesNotPublishPartiallyDestroyedObjectsDuringNestedEvents)
{
    QWidget host;
    host.resize(640, 640);
    host.show();
    auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("command-bar-flyout", &host);
    ASSERT_NE(playground, nullptr);
    playground->interactiveViewport(&host);
    const int propertyCount = playground->snapshot().value("schema").toObject().value("properties").toArray().size();
    bool incomplete = false;
    QObject::connect(playground, &fluent::gallery::GalleryComponentPlayground::stateChanged, &host, [&] {
        incomplete |= playground->snapshot().value("values").toObject().size() != propertyCount;
    });
    QObject::connect(playground->widget(), &QObject::destroyed, &host, [] { QTest::qWait(70); });
    ASSERT_TRUE(playground->apply({{"operation", "reset"}}));
    QTest::qWait(70);
    EXPECT_FALSE(incomplete);
    EXPECT_EQ(playground->snapshot().value("values").toObject().size(), propertyCount);
}

TEST(GalleryComponentPlaygroundInteractionTest, ActionOnlyControlReportsItsSignalAndRespectsDisabled)
{
    QWidget host;
    host.resize(500, 640);
    host.show();
    auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("file-drop-zone", &host);
    ASSERT_NE(playground, nullptr);
    playground->interactiveViewport(&host);
    auto* browse = playground->widget()->findChild<QAbstractButton*>();
    ASSERT_NE(browse, nullptr);
    const auto initial = playground->snapshot();
    QTest::mouseClick(browse, Qt::LeftButton);
    const auto clicked = playground->snapshot();
    ASSERT_FALSE(clicked.value("events").toArray().isEmpty());
    EXPECT_EQ(clicked.value("events").toArray().last().toObject().value("name"), "browseRequested");
    EXPECT_EQ(clicked.value("values"), initial.value("values"));
    EXPECT_EQ(clicked.value("cpp"), initial.value("cpp"));
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"enabled", false}}}}));
    QTest::mouseClick(browse, Qt::LeftButton);
    EXPECT_EQ(playground->snapshot().value("events"), clicked.value("events"));
}

TEST(GalleryComponentPlaygroundInteractionTest, AutomaticImageFitAndAnimationFramesDoNotBecomeManualConfiguration)
{
    QWidget host;
    host.resize(500, 640);
    host.show();
    auto* image = fluent::gallery::GalleryComponentPlayground::createInteractive("image-viewer", &host);
    ASSERT_NE(image, nullptr);
    image->interactiveViewport(&host);
    ASSERT_TRUE(
        image->apply({{"operation", "set"}, {"values", QJsonObject{{"fitMode", "ScaleDown"}, {"zoomFactor", 2}}}}));
    EXPECT_EQ(image->snapshot().value("values").toObject().value("fitMode"), "ScaleDown");
    EXPECT_FALSE(image->snapshot().value("cpp").toString().contains("setZoomFactor("));
    ASSERT_TRUE(image->apply({{"operation", "set"}, {"values", QJsonObject{{"zoomFactor", 2}}}}));
    EXPECT_EQ(image->snapshot().value("values").toObject().value("fitMode"), "Manual");
    EXPECT_TRUE(image->snapshot().value("cpp").toString().contains("setZoomFactor(2)"));
    auto* shimmer = fluent::gallery::GalleryComponentPlayground::createInteractive("shimmer", &host);
    ASSERT_NE(shimmer, nullptr);
    EXPECT_TRUE(playgroundField(shimmer->snapshot(), "shimmerProgress").isEmpty());
    const auto code = shimmer->snapshot().value("cpp");
    QTest::qWait(100);
    EXPECT_EQ(shimmer->snapshot().value("cpp"), code);
}

TEST(GalleryComponentPlaygroundIsolationTest, ButtonFamiliesExposeAllInheritedIconLayoutsAndOffsets)
{
    for (const auto* route : {"button", "compound-button", "dropdown-button", "hyperlink-button", "repeat-button",
             "split-button", "toggle-button", "toggle-split-button"}) {
        SCOPED_TRACE(route);
        QWidget host;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create(route, &host);
        ASSERT_NE(playground, nullptr);
        auto* button = qobject_cast<fluent::basicinput::Button*>(playground->widget());
        ASSERT_NE(button, nullptr);
        const auto options = playgroundField(playground->snapshot(), "fluentLayout").value("options").toArray();
        EXPECT_EQ(options.size(), 6);
        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"icon", "Save"}}}}));
        EXPECT_EQ(button->fluentLayout(), Button::IconBefore);
        for (const auto* layout : {"TextOnly", "IconBefore", "IconAfter", "IconOnly", "IconAbove", "IconBelow"}) {
            ASSERT_TRUE(playground->apply({{"operation", "set"},
                {"values",
                    QJsonObject{{"fluentLayout", layout}, {"iconRotation", 45}, {"iconScale", 0.7}, {"iconOffsetX", 5},
                        {"iconOffsetY", -3}}}}));
            const auto state = playground->snapshot();
            EXPECT_EQ(state.value("values").toObject().value("fluentLayout"), layout);
            EXPECT_EQ(button->iconOffset(), QPoint(5, -3));
            EXPECT_DOUBLE_EQ(button->iconRotation(), 45);
            EXPECT_DOUBLE_EQ(button->iconScale(), 0.7);
            for (const auto* language : {"cpp", "python"}) {
                const auto code = state.value(language).toString();
                EXPECT_TRUE(code.contains(layout));
                EXPECT_TRUE(code.contains("setIconOffset(QPoint(5, -3))"));
                EXPECT_EQ(code.count("setIconOffset("), 1);
            }
        }
    }
}

TEST(GalleryComponentPlaygroundDataTest, NativeModelEditsAreReflectedInDataAndBothExports)
{
    for (const auto* route :
        {"data-grid", "file-list-view", "list-view", "flow-view", "grid-view", "tree-view", "timeline"}) {
        SCOPED_TRACE(route);
        QWidget host;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create(route, &host);
        ASSERT_NE(playground, nullptr);
        auto* view = qobject_cast<QAbstractItemView*>(playground->widget());
        ASSERT_NE(view, nullptr);
        auto* model = view->model();
        ASSERT_TRUE(model->setData(model->index(0, 0), QStringLiteral("Native edited item")));
        const auto state = playground->snapshot();
        EXPECT_EQ(state.value("values").toObject().value("modelData").toArray()[0].toArray()[0], "Native edited item");
        EXPECT_TRUE(state.value("cpp").toString().contains(
            "model->setData(model->index(0, 0), QStringLiteral(\"Native edited item\"))"));
        EXPECT_TRUE(
            state.value("python").toString().contains("model.setData(model.index(0, 0), \"Native edited item\")"));
        const auto field = playgroundField(state, "selectedIndexes");
        const auto labels = field.value("optionLabels").toObject();
        ASSERT_FALSE(labels.isEmpty());
        for (const auto& label : labels)
            EXPECT_FALSE(label.toString().startsWith("0:0"));
        ASSERT_TRUE(playground->apply({{"operation", "reset"}}));
        EXPECT_NE(playground->snapshot().value("values").toObject().value("modelData"),
            state.value("values").toObject().value("modelData"));
    }
}

TEST(GalleryComponentPlaygroundDataTest, ChartsShareEditablePointsAndPreserveNativeCursor)
{
    for (const auto* route : {"chart-view", "line-chart", "area-chart", "bar-chart", "horizontal-bar-chart",
             "pie-chart", "donut-chart", "scatter-chart", "sparkline"}) {
        SCOPED_TRACE(route);
        QWidget host;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create(route, &host);
        ASSERT_NE(playground, nullptr);
        auto* chart = qobject_cast<fluent::charts::ChartView*>(playground->widget());
        if (!chart)
            chart = playground->widget()->findChild<fluent::charts::ChartView*>();
        ASSERT_NE(chart, nullptr);
        QJsonArray rows{QJsonArray{0, 20, "First"}, QJsonArray{1, 55, "Second"}, QJsonArray{2, 38, "Third"}};
        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"seriesData", rows}}}}));
        EXPECT_EQ(chart->model()->rowCount(), 3);
        EXPECT_EQ(chart->model()->pointAt(1), QPointF(1, 55));
        chart->setCurrentPoint(0, 1);
        auto state = playground->snapshot();
        if (state.value("values").toObject().contains("currentPoint")) {
            EXPECT_EQ(state.value("values").toObject().value("currentPoint"), 1);
            EXPECT_TRUE(state.value("cpp").toString().contains("setCurrentPoint(0, 1)"));
        }
        for (const auto* language : {"cpp", "python"}) {
            const auto source = state.value(language).toString();
            EXPECT_TRUE(source.contains("QPointF(1, 55)"));
            EXPECT_TRUE(source.contains("Third"));
        }
        QJsonArray invalid{QJsonArray{2, 4, "A"}, QJsonArray{1, 5, "B"}};
        EXPECT_FALSE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"seriesData", invalid}}}}));
        EXPECT_EQ(chart->model()->rowCount(), 3);
        rows[1] = QJsonArray{1, 70, "Second"};
        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"seriesData", rows}}}}));
        EXPECT_EQ(chart->currentRow(), 1);
    }
}

TEST(GalleryComponentPlaygroundDataTest, GridSortStateAndResetKeepTheModelOrderHonest)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("data-grid", &host);
    const auto initial = playground->snapshot().value("values");
    ASSERT_TRUE(playground->apply({{"operation", "set"},
        {"values", QJsonObject{{"sortingEnabled", true}, {"sortColumn", 0}, {"sortOrder", "AscendingOrder"}}}}));
    auto* view = qobject_cast<QTableView*>(playground->widget());
    ASSERT_NE(view, nullptr);
    view->sortByColumn(1, Qt::DescendingOrder);
    view->setColumnWidth(0, 248);
    const auto state = playground->snapshot();
    EXPECT_EQ(state.value("values").toObject().value("sortColumn"), 1);
    EXPECT_EQ(state.value("values").toObject().value("sortOrder"), "DescendingOrder");
    EXPECT_EQ(state.value("values").toObject().value("columnWidth0"), 248);
    EXPECT_TRUE(state.value("cpp").toString().contains("sortByColumn(1, Qt::DescendingOrder)"));
    EXPECT_TRUE(state.value("cpp").toString().contains("setColumnWidth(0, 248)"));
    EXPECT_TRUE(state.value("python").toString().contains("setColumnWidth(0, 248)"));
    ASSERT_TRUE(playground->apply({{"operation", "reset"}}));
    EXPECT_EQ(playground->snapshot().value("values"), initial);
}

TEST(GalleryComponentPlaygroundDataTest, DrawerStaysInsideItsPreviewStage)
{
    QWidget host;
    host.resize(680, 720);
    host.show();
    auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("drawer-view", &host);
    ASSERT_NE(playground, nullptr);
    auto* drawer = host.findChild<fluent::collections::DrawerView*>();
    ASSERT_NE(drawer, nullptr);
    drawer->setAnimationEnabled(false);
    for (const auto* edge : {"Left", "Right", "Top", "Bottom"}) {
        ASSERT_TRUE(
            playground->apply({{"operation", "set"}, {"values", QJsonObject{{"edge", edge}, {"isOpen", true}}}}));
        const auto viewport = playground->interactiveViewport(&host);
        EXPECT_EQ(drawer->parentWidget(), &host);
        const auto target = viewport.value("target").toObject();
        const auto surface = viewport.value("surface").toObject();
        EXPECT_GE(target.value("y").toInt(), surface.value("y").toInt());
        EXPECT_GE(target.value("x").toInt(), surface.value("x").toInt());
        EXPECT_LE(target.value("y").toInt() + target.value("height").toInt(),
            surface.value("y").toInt() + surface.value("height").toInt());
        EXPECT_LE(target.value("x").toInt() + target.value("width").toInt(),
            surface.value("x").toInt() + surface.value("width").toInt());
        EXPECT_TRUE(viewport.value("hasOpenOverlay").toBool());
    }
    auto* close = host.findChild<fluent::basicinput::Button*>("drawerClose");
    auto* open = host.findChild<fluent::basicinput::Button*>("drawerOpen");
    ASSERT_NE(close, nullptr);
    ASSERT_NE(open, nullptr);
    close->click();
    EXPECT_FALSE(playground->snapshot().value("values").toObject().value("isOpen").toBool());
    EXPECT_FALSE(playground->interactiveViewport(&host).value("hasOpenOverlay").toBool());
    open->click();
    EXPECT_TRUE(playground->snapshot().value("values").toObject().value("isOpen").toBool());
}

TEST(GalleryComponentPlaygroundDataTest, TimelineCellsRetainTheirModelRoles)
{
    QWidget host;
    auto* playground = fluent::gallery::GalleryComponentPlayground::create("timeline", &host);
    ASSERT_NE(playground, nullptr);
    auto* view = qobject_cast<fluent::collections::Timeline*>(playground->widget());
    ASSERT_NE(view, nullptr);
    auto data = playground->snapshot().value("values").toObject().value("modelData").toArray();
    ASSERT_EQ(data[0].toArray().size(), 3);
    EXPECT_EQ(data[0].toArray()[1], "09:00");
    auto row = data[0].toArray();
    row[1] = "16:30";
    row[2] = "Updated description";
    data[0] = row;
    ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"modelData", data}}}}));
    const auto index = view->model()->index(0, 0);
    EXPECT_EQ(index.data(fluent::collections::Timeline::TimestampRole).toString(), "16:30");
    EXPECT_EQ(index.data(fluent::collections::Timeline::DescriptionRole).toString(), "Updated description");
    EXPECT_TRUE(playground->snapshot().value("cpp").toString().contains(
        "QStringLiteral(\"16:30\"), fluent::collections::Timeline::TimestampRole"));
    EXPECT_TRUE(playground->snapshot().value("python").toString().contains(
        "\"16:30\", fluentqt.Timeline.DataRole.TimestampRole"));
}

TEST(GalleryComponentPlaygroundDataTest, CollectionsExposeReorderingPinningAndIndicatorOptions)
{
    for (const auto* route : {"list-view", "grid-view", "flow-view", "tree-view"}) {
        SCOPED_TRACE(route);
        QWidget host;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create(route, &host);
        ASSERT_NE(playground, nullptr);
        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"canReorderItems", true}}}}));
        const auto state = playground->snapshot();
        EXPECT_TRUE(state.value("values").toObject().value("canReorderItems").toBool());
        EXPECT_TRUE(playground->widget()->property("canReorderItems").toBool());
        EXPECT_TRUE(state.value("cpp").toString().contains("setCanReorderItems(true)"));
        EXPECT_TRUE(state.value("python").toString().contains("setCanReorderItems(True)"));
    }
    {
        QWidget host;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create("data-grid", &host);
        ASSERT_NE(playground, nullptr);
        auto* grid = qobject_cast<fluent::collections::DataGrid*>(playground->widget());
        ASSERT_NE(grid, nullptr);
        ASSERT_TRUE(playground->apply(
            {{"operation", "set"}, {"values", QJsonObject{{"pinFirstColumn", true}, {"sectionsMovable", false}}}}));
        EXPECT_TRUE(grid->isColumnPinned(0));
        EXPECT_FALSE(grid->horizontalHeader()->sectionsMovable());
        const auto state = playground->snapshot();
        EXPECT_TRUE(state.value("cpp").toString().contains("setColumnPinned(0, true)"));
        EXPECT_TRUE(state.value("python").toString().contains("setColumnPinned(0, True)"));
        EXPECT_TRUE(state.value("cpp").toString().contains("horizontalHeader()->setSectionsMovable(false)"));
        EXPECT_TRUE(state.value("python").toString().contains("horizontalHeader().setSectionsMovable(False)"));
    }
    {
        QWidget host;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create("tree-view", &host);
        ASSERT_NE(playground, nullptr);
        auto* tree = qobject_cast<fluent::collections::TreeView*>(playground->widget());
        ASSERT_NE(tree, nullptr);
        ASSERT_TRUE(
            playground->apply({{"operation", "set"}, {"values", QJsonObject{{"selectionIndicatorVisible", true}}}}));
        EXPECT_TRUE(tree->selectionIndicatorVisible());
        const auto state = playground->snapshot();
        EXPECT_TRUE(state.value("cpp").toString().contains("setSelectionIndicatorVisible(true)"));
        EXPECT_TRUE(state.value("python").toString().contains("setSelectionIndicatorVisible(True)"));
    }
}

TEST(GalleryComponentPlaygroundInteractionTest, DialogsAndFlyoutsStayInsideTheirPreviewStage)
{
    const QStringList routes{QStringLiteral("content-dialog"), QStringLiteral("dialog"), QStringLiteral("flyout"),
        QStringLiteral("popup"), QStringLiteral("teaching-tip"), QStringLiteral("coach-mark")};
    const auto findOverlay = [](QWidget* root) -> QWidget* {
        for (auto* widget : root->findChildren<QWidget*>()) {
            if (widget->isVisible()
                && (qobject_cast<fluent::dialogs_flyouts::Dialog*>(widget)
                    || qobject_cast<fluent::dialogs_flyouts::Popup*>(widget)
                    || qobject_cast<fluent::dialogs_flyouts::CoachMark*>(widget))) {
                return widget;
            }
        }
        return nullptr;
    };

    for (const auto& route : routes) {
        SCOPED_TRACE(route.toStdString());
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive(route, &host);
        ASSERT_NE(playground, nullptr);
        const auto opened = playground->interactiveViewport(&host);
        EXPECT_TRUE(opened.value("hasOpenOverlay").toBool());
        EXPECT_TRUE(opened.value("hasVisibleOverlay").toBool());

        const auto surfaceObj = opened.value("surface").toObject();
        const QRect surfaceRect(surfaceObj.value("x").toInt(), surfaceObj.value("y").toInt(),
            surfaceObj.value("width").toInt(), surfaceObj.value("height").toInt());
        QWidget* overlay = findOverlay(&host);
        ASSERT_NE(overlay, nullptr);
        const QRect overlayRect(host.mapFromGlobal(overlay->mapToGlobal(QPoint())), overlay->size());
        EXPECT_TRUE(host.rect().contains(overlayRect));
        EXPECT_TRUE(surfaceRect.adjusted(-20, -20, 20, 20).contains(overlayRect));

        const QString openProp =
            route == QStringLiteral("coach-mark") ? QStringLiteral("open") : QStringLiteral("isOpen");
        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{openProp, false}}}}));
        const auto closed = playground->interactiveViewport(&host, false);
        EXPECT_FALSE(closed.value("hasOpenOverlay").toBool());
        EXPECT_FALSE(closed.value("hasVisibleOverlay").toBool());
        EXPECT_EQ(closed.value("height"), opened.value("height"));

        auto* trigger = playground->widget()->findChild<QAbstractButton*>();
        ASSERT_NE(trigger, nullptr);
        QTest::mouseClick(trigger, Qt::LeftButton);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return playground->interactiveViewport(&host, false).value("hasOpenOverlay").toBool();
        }));

        delete playground;
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        EXPECT_FALSE(host.property("fluentOverlaySurfaceRect").isValid());
    }

    // Live placement and anchorOffset updates move an open Flyout inside its stage.
    {
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("flyout", &host);
        ASSERT_NE(playground, nullptr);
        playground->interactiveViewport(&host);
        auto* flyout = host.findChild<fluent::dialogs_flyouts::Flyout*>();
        ASSERT_NE(flyout, nullptr);
        ASSERT_TRUE(flyout->isOpen());
        const int bottomY = flyout->y();
        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"placement", "Top"}}}}));
        EXPECT_TRUE(flyout->isOpen());
        EXPECT_LT(flyout->y(), bottomY);
        ASSERT_TRUE(playground->apply(
            {{"operation", "set"}, {"values", QJsonObject{{"placement", "Bottom"}, {"anchorOffset", 0}}}}));
        const int offsetZeroY = flyout->y();
        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"anchorOffset", 24}}}}));
        EXPECT_EQ(flyout->y() - offsetZeroY, 24);
    }

    // ContentDialog exposes defaultButton with C++ and PySide6 exports.
    {
        QWidget host;
        auto* playground = fluent::gallery::GalleryComponentPlayground::create("content-dialog", &host);
        ASSERT_NE(playground, nullptr);
        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"defaultButton", "Close"}}}}));
        const auto state = playground->snapshot();
        EXPECT_TRUE(
            state.value("cpp").toString().contains("setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Close)"));
        EXPECT_TRUE(state.value("python").toString().contains(
            "setDefaultButton(fluentqt.ContentDialog.ContentDialogButton.Close)"));
    }
}

TEST(GalleryComponentPlaygroundInteractionTest, LayoutPlaygroundsSizeInteractAndExportCleanly)
{
    // DataGridToolBar: contentHeight avoids clipping wrapped rows, model populates Columns menu, and filter round-trips.
    {
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("data-grid-tool-bar", &host);
        ASSERT_NE(playground, nullptr);
        const auto viewport = playground->interactiveViewport(&host);
        auto* bar = qobject_cast<fluent::layout::DataGridToolBar*>(playground->widget());
        ASSERT_NE(bar, nullptr);
        QApplication::processEvents();

        ASSERT_NE(bar->searchField(), nullptr);
        EXPECT_EQ(bar->searchField()->placeholderText(), QStringLiteral("Search projects..."));
        ASSERT_NE(bar->columnsButton(), nullptr);
        ASSERT_NE(bar->columnsButton()->menu(), nullptr);
        QTest::mouseClick(bar->columnsButton(), Qt::LeftButton);
        QApplication::processEvents();
        EXPECT_GE(bar->columnsButton()->menu()->actions().size(), 3);
        bar->columnsButton()->menu()->close();
        QApplication::processEvents();

        const int surfaceHeight = viewport.value("surface").toObject().value("height").toInt();
        EXPECT_GE(surfaceHeight, bar->columnsButton()->geometry().bottom() + 1);
        EXPECT_GE(surfaceHeight, bar->clearButton()->geometry().bottom() + 1);

        ASSERT_TRUE(
            playground->apply({{"operation", "set"}, {"values", QJsonObject{{"statusFilter", QJsonArray{"Open"}}}}}));
        EXPECT_EQ(bar->filters().value("status").toStringList(), QStringList{QStringLiteral("Open")});
        const auto state = playground->snapshot();
        EXPECT_EQ(state.value("values").toObject().value("statusFilter"), QJsonArray{"Open"});
        EXPECT_TRUE(state.value("cpp").toString().contains("setFilterValues(QStringLiteral(\"status\")"));
        EXPECT_TRUE(state.value("python").toString().contains("setFilterValues(\"status\", [\"Open\"])"));

        bar->searchField()->setText(QStringLiteral("Gallery"));
        QApplication::processEvents();
        EXPECT_EQ(playground->snapshot().value("values").toObject().value("searchText"), QStringLiteral("Gallery"));
    }

    // Accordion: dynamic contentHeight tracks section expansions and header clicks sync values.
    {
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("accordion", &host);
        ASSERT_NE(playground, nullptr);
        auto* accordion = qobject_cast<fluent::layout::Accordion*>(playground->widget());
        ASSERT_NE(accordion, nullptr);
        ASSERT_GE(accordion->count(), 2);

        ASSERT_TRUE(
            playground->apply({{"operation", "set"}, {"values", QJsonObject{{"expandedItems", QJsonArray{}}}}}));
        const int collapsedHeight =
            playground->interactiveViewport(&host, false).value("surface").toObject().value("height").toInt();

        ASSERT_TRUE(playground->apply({{"operation", "set"},
            {"values",
                QJsonObject{{"expansionMode", "Multiple"}, {"expandedItems", QJsonArray{"Account", "Privacy"}}}}}));
        const int expandedHeight =
            playground->interactiveViewport(&host, false).value("surface").toObject().value("height").toInt();
        EXPECT_GT(expandedHeight, collapsedHeight + 40);

        auto* secondItem = accordion->itemAt(1);
        ASSERT_NE(secondItem, nullptr);
        ASSERT_NE(secondItem->headerButton(), nullptr);
        QTest::mouseClick(secondItem->headerButton(), Qt::LeftButton);
        ASSERT_TRUE(QTest::qWaitFor([&] { return !secondItem->isExpanded(); }));
        EXPECT_EQ(playground->snapshot().value("values").toObject().value("expandedItems"), QJsonArray{"Account"});
    }

    // Expander: content wrapper provides inset margins, contentHeight adapts, and animationEnabled exports.
    {
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("expander", &host);
        ASSERT_NE(playground, nullptr);
        auto* expander = qobject_cast<fluent::layout::Expander*>(playground->widget());
        ASSERT_NE(expander, nullptr);
        ASSERT_NE(expander->contentWidget(), nullptr);
        ASSERT_NE(expander->contentWidget()->layout(), nullptr);
        EXPECT_GE(expander->contentWidget()->layout()->contentsMargins().left(), 12);

        const int expandedHeight =
            playground->interactiveViewport(&host).value("surface").toObject().value("height").toInt();
        ASSERT_TRUE(playground->apply(
            {{"operation", "set"}, {"values", QJsonObject{{"expanded", false}, {"animationEnabled", false}}}}));
        const int collapsedHeight =
            playground->interactiveViewport(&host, false).value("surface").toObject().value("height").toInt();
        EXPECT_LT(collapsedHeight, expandedHeight);
        const auto state = playground->snapshot();
        EXPECT_TRUE(state.value("cpp").toString().contains("setAnimationEnabled(false)"));
        EXPECT_TRUE(state.value("python").toString().contains("setAnimationEnabled(False)"));
    }

    // ListDetailView: 380 px canvas keeps initial 156 px pane unclamped and wires Open/Back actions.
    {
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("list-detail-view", &host);
        ASSERT_NE(playground, nullptr);
        playground->interactiveViewport(&host);
        auto* view = qobject_cast<fluent::layout::ListDetailView*>(playground->widget());
        ASSERT_NE(view, nullptr);
        EXPECT_EQ(view->listPaneWidth(), 156);
        EXPECT_FALSE(view->isCompact());
        EXPECT_GE(view->listPane()->width(), 150);

        auto* openBtn = view->listPane()->findChild<fluent::basicinput::Button*>();
        ASSERT_NE(openBtn, nullptr);
        QTest::mouseClick(openBtn, Qt::LeftButton);
        EXPECT_EQ(view->currentPane(), fluent::layout::ListDetailView::Detail);
        EXPECT_EQ(playground->snapshot().value("values").toObject().value("currentPane"), QStringLiteral("Detail"));

        ASSERT_TRUE(playground->apply({{"operation", "set"}, {"values", QJsonObject{{"compactThreshold", 440}}}}));
        playground->interactiveViewport(&host, false);
        EXPECT_TRUE(view->isCompact());
        EXPECT_TRUE(view->detailPane()->isVisible());

        auto* backBtn = view->detailPane()->findChild<fluent::basicinput::Button*>();
        ASSERT_NE(backBtn, nullptr);
        QTest::mouseClick(backBtn, Qt::LeftButton);
        EXPECT_EQ(view->currentPane(), fluent::layout::ListDetailView::List);
        EXPECT_EQ(playground->snapshot().value("values").toObject().value("currentPane"), QStringLiteral("List"));
    }

    // PropertyPanel: General & Layout groups host 4 native editors while value_title and value_visible round-trip.
    {
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* playground = fluent::gallery::GalleryComponentPlayground::createInteractive("property-panel", &host);
        ASSERT_NE(playground, nullptr);
        playground->interactiveViewport(&host);
        auto* panel = qobject_cast<fluent::layout::PropertyPanel*>(playground->widget());
        ASSERT_NE(panel, nullptr);
        EXPECT_EQ(panel->propertyKeys(), (QStringList{"title", "visible", "density", "spacing"}));
        const auto defs = panel->properties();
        ASSERT_EQ(defs.size(), 4);
        EXPECT_EQ(defs.at(0).group, QStringLiteral("General"));
        EXPECT_EQ(defs.at(2).group, QStringLiteral("Layout"));

        ASSERT_TRUE(playground->apply(
            {{"operation", "set"}, {"values", QJsonObject{{"value_title", "Annual Plan"}, {"value_visible", false}}}}));
        EXPECT_EQ(panel->value("title").toString(), QStringLiteral("Annual Plan"));
        EXPECT_FALSE(panel->value("visible").toBool());
    }

    // Card and Field: contentHeight adapts cleanly with typography/margins preserved.
    {
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* cardPg = fluent::gallery::GalleryComponentPlayground::createInteractive("card", &host);
        ASSERT_NE(cardPg, nullptr);
        const int cardHeight = cardPg->interactiveViewport(&host).value("surface").toObject().value("height").toInt();
        auto* card = qobject_cast<fluent::layout::Card*>(cardPg->widget());
        ASSERT_NE(card, nullptr);
        ASSERT_NE(card->layout(), nullptr);
        EXPECT_GE(card->layout()->contentsMargins().left(), 14);
        EXPECT_GE(cardHeight, card->sizeHint().height());

        auto* fieldPg = fluent::gallery::GalleryComponentPlayground::createInteractive("field", &host);
        ASSERT_NE(fieldPg, nullptr);
        const int fieldHeight = fieldPg->interactiveViewport(&host).value("surface").toObject().value("height").toInt();
        EXPECT_GE(fieldHeight, fieldPg->widget()->sizeHint().height());
        const auto fieldState = fieldPg->snapshot();
        EXPECT_TRUE(fieldState.value("cpp").toString().contains("setHelperText"));
        EXPECT_TRUE(fieldState.value("cpp").toString().contains("setValidationMessage"));
    }

    // CommandPalette fits inside the playground overlay surface rect and updates smoothly.
    {
        QWidget host;
        host.resize(760, 640);
        host.show();
        auto* palettePg = fluent::gallery::GalleryComponentPlayground::createInteractive("command-palette", &host);
        ASSERT_NE(palettePg, nullptr);
        ASSERT_TRUE(palettePg->apply({{"operation", "set"}, {"values", QJsonObject{{"isOpen", true}}}}));
        const auto viewport = palettePg->interactiveViewport(&host);
        const auto surface = viewport.value("surface").toObject();
        const QRect surfaceRect(surface.value("x").toInt(), surface.value("y").toInt(), surface.value("width").toInt(),
            surface.value("height").toInt());
        auto* palette = host.findChild<fluent::menus_toolbars::CommandPalette*>();
        if (!palette && palettePg->widget())
            palette = palettePg->widget()->findChild<fluent::menus_toolbars::CommandPalette*>();
        ASSERT_NE(palette, nullptr);
        QApplication::processEvents();
        EXPECT_TRUE(palette->isVisible());
        EXPECT_LE(palette->width(), surfaceRect.width());
        EXPECT_LE(palette->height(), surfaceRect.height());
    }

    // Embedded Menu in Playground stays visible on click and shrinks cleanly when fontStyle returns to Body.
    {
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* menuPg = fluent::gallery::GalleryComponentPlayground::createInteractive("menu", &host);
        ASSERT_NE(menuPg, nullptr);
        menuPg->interactiveViewport(&host);
        auto* menu = qobject_cast<fluent::menus_toolbars::FluentMenu*>(menuPg->widget());
        ASSERT_NE(menu, nullptr);
        ASSERT_FALSE(menu->isWindow());
        ASSERT_TRUE(menu->isVisible());
        const int initialWidth = menu->width();
        QAction* firstAction = nullptr;
        for (QAction* action : menu->actions()) {
            if (action && !action->isSeparator()) {
                firstAction = action;
                break;
            }
        }
        ASSERT_NE(firstAction, nullptr);
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(firstAction).center());
        QApplication::processEvents();
        EXPECT_TRUE(menu->isVisible());

        ASSERT_TRUE(menuPg->apply({{"operation", "set"}, {"values", QJsonObject{{"fontStyle", "TitleLarge"}}}}));
        QApplication::processEvents();
        EXPECT_GT(menu->width(), initialWidth);
        ASSERT_TRUE(menuPg->apply({{"operation", "set"}, {"values", QJsonObject{{"fontStyle", "Body"}}}}));
        QApplication::processEvents();
        EXPECT_LE(menu->width(), initialWidth + 2);
        EXPECT_TRUE(menu->isVisible());
    }

    // CommandBarFlyout anchor button reopens a closed flyout and light-dismisses cleanly when clicked open.
    {
        QWidget host;
        host.resize(680, 640);
        host.show();
        auto* flyoutPg = fluent::gallery::GalleryComponentPlayground::createInteractive("command-bar-flyout", &host);
        ASSERT_NE(flyoutPg, nullptr);
        const auto opened = flyoutPg->interactiveViewport(&host);
        EXPECT_TRUE(opened.value("hasOpenOverlay").toBool());
        auto* flyout = host.findChild<fluent::menus_toolbars::CommandBarFlyout*>();
        ASSERT_NE(flyout, nullptr);
        ASSERT_TRUE(flyout->isOpen());
        fluent::basicinput::Button* selectionButton = nullptr;
        for (auto* btn : flyoutPg->widget()->findChildren<fluent::basicinput::Button*>()) {
            if (btn->text() == QStringLiteral("Selection")) {
                selectionButton = btn;
                break;
            }
        }
        ASSERT_NE(selectionButton, nullptr);

        // Dismiss via schema toggle then click the anchor button to reopen.
        ASSERT_TRUE(flyoutPg->apply({{"operation", "set"}, {"values", QJsonObject{{"isOpen", false}}}}));
        ASSERT_FALSE(flyout->isOpen());
        QTest::mouseClick(selectionButton, Qt::LeftButton);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return flyoutPg->interactiveViewport(&host, false).value("hasOpenOverlay").toBool();
        }));
        EXPECT_TRUE(flyout->isOpen());

        // Clicking Selection while open consumes light-dismiss without immediately reopening.
        QTest::mouseClick(selectionButton, Qt::LeftButton);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return !flyoutPg->interactiveViewport(&host, false).value("hasOpenOverlay").toBool();
        }));
        EXPECT_FALSE(flyout->isOpen());

        // Clicking Selection again opens it cleanly.
        QTest::mouseClick(selectionButton, Qt::LeftButton);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return flyoutPg->interactiveViewport(&host, false).value("hasOpenOverlay").toBool();
        }));
        EXPECT_TRUE(flyout->isOpen());
    }
}

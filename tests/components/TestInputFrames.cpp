#include <gtest/gtest.h>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QTest>
#include <QtMath>

#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/basicinput/TreeSelect.h"
#include "components/date_time/CalendarDatePicker.h"
#include "components/date_time/DatePicker.h"
#include "components/date_time/DateTimeRangePicker.h"
#include "components/date_time/TimePicker.h"
#include "components/foundation/private/TextInputFrame_p.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/NumberBox.h"
#include "components/textfields/PasswordBox.h"
#include "components/textfields/TagInput.h"
#include "components/textfields/TextEdit.h"

using namespace fluent;
using namespace fluent::basicinput;
using namespace fluent::textfields;

namespace {

QImage renderFrame(QWidget& widget, qreal scale)
{
    QImage image(QSize(qCeil(widget.width() * scale), qCeil(widget.height() * scale)),
        QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(scale);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget.render(&painter);
    return image;
}

void expectSymmetricEdges(const QImage& image)
{
    // Inspect several columns away from text and rounded corners. An extra
    // bottom stroke must not hide behind a correct public default property.
    // zh_CN: 检查远离文字和圆角的多列像素，防止属性默认值正确但仍多画底线。
    for (int numerator : {1, 2, 3}) {
        const int x = image.width() * numerator / 4;
        EXPECT_EQ(image.pixelColor(x, 0), image.pixelColor(x, image.height() - 1));
    }
}

struct FieldCase {
    const char* name;
    QWidget* owner;
    QWidget* surface;
};

} // namespace

TEST(InputFrameTest, Contract_DefaultFieldsOmitRestingUnderline)
{
    QWidget host;
    host.resize(640, 480);
    Button focusTarget(QStringLiteral("Focus"), &host);
    focusTarget.setGeometry(10, 10, 100, 32);
    QVector<FieldCase> fields;
    const QVector<QWidget*> editors{new LineEdit(&host), new AutoSuggestBox(&host), new NumberBox(&host),
        new PasswordBox(&host), new TagInput(&host), new TextEdit(&host)};
    for (auto* editor : editors) {
        if (auto* multiline = qobject_cast<TextEdit*>(editor))
            multiline->setAutoResizeEnabled(false);
        fields.append({editor->metaObject()->className(), editor, editor});
    }
    for (bool editable : {false, true}) {
        auto* combo = new ComboBox(&host);
        combo->addItems({QStringLiteral("2 seconds"), QStringLiteral("Automatic")});
        combo->setEditable(editable);
        fields.append({editable ? "Editable ComboBox" : "ComboBox", combo, combo});
    }
    for (auto mode : {TreeSelect::SelectionMode::Single, TreeSelect::SelectionMode::Multiple}) {
        auto* tree = new TreeSelect(&host);
        tree->setSelectionMode(mode);
        fields.append({mode == TreeSelect::SelectionMode::Single ? "TreeSelect single" : "TreeSelect multiple", tree,
            tree->findChild<LineEdit*>(mode == TreeSelect::SelectionMode::Single
                    ? QStringLiteral("TreeSelectSingleInput")
                    : QStringLiteral("TreeSelectMultipleInput"))});
    }
    auto* multi = new MultiSelectComboBox(&host);
    fields.append(
        {"MultiSelectComboBox", multi, multi->findChild<Button*>(QStringLiteral("MultiSelectComboBox.Trigger"))});
    const QVector<QWidget*> pickers{new date_time::CalendarDatePicker(&host), new date_time::DatePicker(&host),
        new date_time::TimePicker(&host)};
    for (auto* picker : pickers)
        fields.append({picker->metaObject()->className(), picker, picker});
    auto* range = new date_time::DateTimeRangePicker(&host);
    fields.append({"DateTimeRangePicker", range, range->findChild<Button*>(QStringLiteral("rangeEntry"))});
    for (const auto& field : fields)
        field.owner->hide();
    host.show();
    QApplication::setActiveWindow(&host);
    focusTarget.setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return focusTarget.hasFocus(); }, 1000));

    for (const auto& field : fields) {
        SCOPED_TRACE(field.name);
        ASSERT_NE(field.surface, nullptr);
        field.owner->show();
        for (auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
            SCOPED_TRACE(static_cast<int>(theme));
            field.owner->setProperty("fluentThemeOverride", static_cast<int>(theme));
            for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
                field.owner->setLayoutDirection(direction);
                for (int width : {160, 280}) {
                    field.owner->setGeometry(20, 70, qMax(width, field.owner->minimumSizeHint().width()),
                        qobject_cast<TextEdit*>(field.owner) ? 64 : field.owner->sizeHint().height());
                    QApplication::processEvents();
                    for (int state = 0; state < 4; ++state) {
                        SCOPED_TRACE(::testing::Message()
                            << "direction=" << direction << " width=" << width << " state=" << state);
                        // Rest, hover, read-only and disabled all retain a quiet outline.
                        // zh_CN: 普通、悬停、只读和禁用状态均保留无额外底线的外框。
                        field.owner->setEnabled(state != 3);
                        if (auto* edit = qobject_cast<QLineEdit*>(field.surface))
                            edit->setReadOnly(state == 2);
                        if (auto* combo = qobject_cast<ComboBox*>(field.surface)) {
                            if (combo->lineEdit())
                                combo->lineEdit()->setReadOnly(state == 2);
                        }
                        if (auto* edit = qobject_cast<TextEdit*>(field.surface))
                            edit->setReadOnly(state == 2);
                        focusTarget.setFocus();
                        QTest::mouseMove(state == 1 ? field.surface : &focusTarget);
                        QApplication::processEvents();
                        for (qreal scale : {1.0, 1.25, 1.5, 2.0}) {
                            SCOPED_TRACE(scale);
                            expectSymmetricEdges(renderFrame(*field.surface, scale));
                        }
                    }
                }
            }
        }
        field.owner->hide();
    }
}

TEST(InputFrameTest, Contract_SharedDefaultKeepsOnlyFocusAndValidationUnderlines)
{
    LineEdit themeHost;
    for (auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
        themeHost.setProperty("fluentThemeOverride", static_cast<int>(theme));
        for (qreal scale : {1.0, 1.25, 1.5, 2.0}) {
            for (int variant = 0; variant < 7; ++variant) {
                SCOPED_TRACE(::testing::Message() << "theme=" << theme << " scale=" << scale << " variant=" << variant);
                fluent::detail::TextInputFrameState state;
                state.hovered = variant == 1;
                state.pressed = variant == 2;
                state.readOnly = variant == 3;
                state.enabled = variant != 4;
                state.focused = variant == 5;
                state.invalid = variant == 6;
                QImage image(QSize(qRound(240 * scale), qRound(32 * scale)), QImage::Format_ARGB32_Premultiplied);
                image.setDevicePixelRatio(scale);
                image.fill(Qt::transparent);
                QPainter painter(&image);
                painter.setRenderHint(QPainter::Antialiasing);
                fluent::detail::paintTextInputFrame(painter, QRectF(0, 0, 240, 32), themeHost.themeColors(),
                    themeHost.themeRadius().control, state);
                painter.end();
                if (state.focused || state.invalid)
                    EXPECT_NE(image.pixelColor(image.width() / 2, 0),
                        image.pixelColor(image.width() / 2, image.height() - 1));
                else
                    expectSymmetricEdges(image);
            }
        }
    }
}

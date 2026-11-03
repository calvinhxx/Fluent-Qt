#include <gtest/gtest.h>

#include <QApplication>
#include <QImage>
#include <QLineEdit>
#include <QPainter>
#include <QtMath>
#include <QStandardItemModel>
#include <QTest>

#include "components/basicinput/ComboBox.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/basicinput/TreeSelect.h"
#include "components/foundation/ThemeRegistry.h"
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
const QString value = QStringLiteral("1280");

QRect textInk(QWidget& widget, qreal dpr, Qt::LayoutDirection direction = Qt::LeftToRight)
{
    QImage image(QSize(qCeil(widget.width() * dpr), qCeil(widget.height() * dpr)), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        widget.render(&painter);
    }
    // A diagnostic text color separates glyph ink from borders and trailing buttons.
    const int start = direction == Qt::RightToLeft ? image.width() / 2 : 0;
    const int end = direction == Qt::RightToLeft ? image.width() : image.width() / 2;
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = start; x < end; ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.red() > 200 && pixel.blue() > 200 && pixel.green() < 80)
                bounds |= QRect(x, y, 1, 1);
        }
    }
    return bounds;
}

void expectSameTextStart(QWidget& reference,
    QWidget& actual,
    qreal dpr,
    Qt::LayoutDirection direction = Qt::LeftToRight)
{
    SCOPED_TRACE(actual.metaObject()->className());
    const QRect expected = textInk(reference, dpr, direction);
    const QRect observed = textInk(actual, dpr, direction);
    ASSERT_FALSE(expected.isEmpty());
    ASSERT_FALSE(observed.isEmpty());
    if (direction == Qt::RightToLeft)
        EXPECT_NEAR(observed.right(), expected.right(), 1.0);
    else
        EXPECT_NEAR(observed.left(), expected.left(), 1.0);
}
} // namespace

class InputTextInsetsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        previous = ThemeRegistry::instance().extendedSnapshot();
        theme = FluentElement::currentTheme();
        auto diagnostic = previous;
        for (auto* colors : {&diagnostic.base.lightColors, &diagnostic.base.darkColors}) {
            colors->textPrimary = Qt::magenta;
            colors->textSecondary = Qt::magenta;
            colors->textTertiary = Qt::magenta;
            colors->textDisabled = Qt::magenta;
        }
        ThemeRegistry::instance().applyExtendedSnapshot(diagnostic);
    }

    void TearDown() override
    {
        ThemeRegistry::instance().applyExtendedSnapshot(previous);
        FluentElement::setTheme(theme);
    }

    ThemeRegistry::ExtendedSnapshot previous;
    FluentElement::Theme theme;
};

TEST_F(InputTextInsetsTest, Contract_DefaultFieldsSharePaintedTextStart)
{
    for (auto appearance : {FluentElement::Light, FluentElement::Dark}) {
        SCOPED_TRACE(appearance);
        FluentElement::setTheme(appearance);
        QStandardItemModel model;
        auto* item = new QStandardItem(value);
        item->setData(QStringLiteral("key"), Qt::UserRole);
        model.appendRow(item);
        QWidget host;
        ComboBox combo(&host);
        combo.addItem(value);
        ComboBox editable(&host);
        editable.addItem(value);
        editable.setEditable(true);
        LineEdit line(&host);
        line.setText(value);
        AutoSuggestBox suggest(&host);
        suggest.setText(value);
        NumberBox number(&host);
        number.setValue(1280);
        PasswordBox password(&host);
        password.setPasswordRevealMode(PasswordBox::PasswordRevealMode::Visible);
        password.setText(value);
        TagInput tags(&host);
        tags.setText(value);
        TreeSelect tree(&host);
        tree.setModel(&model);
        tree.setSelectedKeys({QStringLiteral("key")});
        MultiSelectComboBox multi(&host);
        multi.setModel(&model);
        multi.setSelectedRows({0});
        TextEdit text(&host);
        text.setPlainText(value);
        const QList<QWidget*> controls{&combo, &editable, &line, &suggest, &number, &password, &tags, &tree, &multi,
            &text};
        host.setFocusPolicy(Qt::StrongFocus);
        host.resize(360, 500);
        for (int width : {160, 280}) {
            SCOPED_TRACE(width);
            int y = 0;
            for (auto* control : controls) {
                control->setGeometry(0, y, width, control->sizeHint().height());
                y += control->height() + 4;
            }
            host.show();
            host.setFocus();
            QApplication::processEvents();
            for (bool enabled : {true, false}) {
                SCOPED_TRACE(enabled);
                for (auto* control : controls)
                    control->setEnabled(enabled);
                for (qreal dpr : {1.0, 1.25, 1.5, 2.0, 3.0}) {
                    SCOPED_TRACE(dpr);
                    for (auto* control : controls)
                        expectSameTextStart(combo, *control, dpr);
                }
            }
        }
    }
}

TEST_F(InputTextInsetsTest, Contract_ExplicitInsetsAndPlaceholderMatchPaintedFields)
{
    QWidget host;
    ComboBox combo(&host);
    combo.addItem(value);
    LineEdit line(&host);
    line.setText(value);
    line.setClearButtonEnabled(false);
    TextEdit text(&host);
    text.setPlainText(value);
    host.resize(260, 160);
    combo.setGeometry(0, 0, 240, 32);
    line.setGeometry(0, 40, 240, 32);
    text.setGeometry(0, 80, 240, text.sizeHint().height());
    host.setFocusPolicy(Qt::StrongFocus);
    host.show();
    host.setFocus();
    QApplication::processEvents();
    for (int inset : {0, 1, 8, 11, 20}) {
        SCOPED_TRACE(inset);
        combo.setContentPaddingH(inset);
        line.setContentMargins(QMargins(inset, 4, inset, 4));
        text.setContentMargins(QMargins(inset, 4, inset, 4));
        for (bool placeholder : {false, true}) {
            line.setText(placeholder ? QString() : value);
            line.setPlaceholderText(placeholder ? value : QString());
            text.setPlainText(placeholder ? QString() : value);
            text.setPlaceholderText(placeholder ? value : QString());
            line.setReadOnly(placeholder);
            for (qreal dpr : {1.0, 1.25, 2.0}) {
                expectSameTextStart(combo, line, dpr);
                expectSameTextStart(combo, text, dpr);
            }
        }
    }
}

TEST_F(InputTextInsetsTest, Contract_EditableFieldsTrackInsetsWithoutResize)
{
    QWidget host;
    ComboBox reference(&host);
    reference.addItem(value);
    ComboBox editable(&host);
    editable.addItem(value);
    editable.setEditable(true);
    LineEdit line(&host);
    line.setText(value);
    host.resize(280, 160);
    reference.setGeometry(0, 0, 240, 32);
    editable.setGeometry(0, 40, 240, 32);
    line.setGeometry(0, 80, 240, 32);
    host.setFocusPolicy(Qt::StrongFocus);
    host.show();
    host.setFocus();
    QApplication::processEvents();
    for (bool nativeEditor : {false, true}) {
        SCOPED_TRACE(nativeEditor);
        if (nativeEditor) {
            auto* editor = new QLineEdit();
            editor->setPalette(line.palette());
            editable.setLineEdit(editor);
        }
        line.setTextMargins(0, 0, 0, 0);
        for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            SCOPED_TRACE(direction);
            reference.setLayoutDirection(direction);
            editable.setLayoutDirection(direction);
            line.setLayoutDirection(direction);
            const Qt::Alignment alignment = Qt::AlignVCenter
                | (direction == Qt::RightToLeft ? Qt::AlignRight : Qt::AlignLeft);
            line.setAlignment(alignment);
            editable.lineEdit()->setAlignment(alignment);
            for (int inset : {11, 20, 8, 0}) {
                SCOPED_TRACE(inset);
                reference.setContentPaddingH(inset);
                editable.setContentPaddingH(inset);
                line.setContentMargins(QMargins(inset, 4, inset, 4));
                EXPECT_EQ(line.layoutDirection(), direction) << line.styleSheet().toStdString();
                for (qreal dpr : {1.0, 1.25, 2.0}) {
                    if (direction == Qt::LeftToRight) {
                        expectSameTextStart(reference, editable, dpr);
                        expectSameTextStart(reference, line, dpr);
                    } else {
                        // Qt reserves caret room for right-aligned editor text.
                        // Preserve that native behavior across both editing surfaces.
                        expectSameTextStart(line, editable, dpr, direction);
                    }
                }
            }
        }
        const QMargins extra(5, 0, 7, 0);
        editable.lineEdit()->setTextMargins(extra);
        line.setTextMargins(extra);
        editable.setContentPaddingH(12);
        line.setContentMargins(QMargins(12, 4, 12, 4));
        EXPECT_EQ(editable.lineEdit()->textMargins(), extra);
        for (qreal dpr : {1.0, 1.25, 2.0})
            expectSameTextStart(line, editable, dpr, Qt::RightToLeft);
    }
}

#include <gtest/gtest.h>
#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractItemDelegate>
#include <QList>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QtMath>
#include <QScopeGuard>
#include <QStandardItemModel>
#include <QStyleOptionViewItem>
#include <QTest>
#include <QWidget>

#include "QtTestEnvironment.h"
#include "compatibility/TextPaintCompat.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/basicinput/TreeSelect.h"
#include "components/collections/ListView.h"
#include "components/foundation/FluentElement.h"
#include "components/textfields/AutoSuggestBox.h"
#include "design/Typography.h"

namespace {
const QStringList labels{QStringLiteral("Settin"), QStringLiteral("Settings"), QStringLiteral("Settingyp"),
    QString::fromUtf8("SettinÉ")};

QImage canvas(const QSize& size, qreal dpr)
{
    QImage image(QSize(qCeil(size.width() * dpr), qCeil(size.height() * dpr)), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::white);
    return image;
}

bool isTextPixel(QRgb pixel)
{
    return qGray(pixel) < 100
        && qMax(qRed(pixel), qMax(qGreen(pixel), qBlue(pixel))) - qMin(qRed(pixel), qMin(qGreen(pixel), qBlue(pixel)))
        < 40;
}

QRect referenceGlyph(const QImage& image, const QFont& font)
{
    const qreal dpr = image.devicePixelRatioF();
    const QRect content = image.rect().adjusted(qCeil(4 * dpr), qCeil(3 * dpr), -qCeil(40 * dpr), -qCeil(3 * dpr));
    int left = image.width();
    for (int y = content.top(); y <= content.bottom(); ++y) {
        const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = content.left(); x <= content.right(); ++x)
            if (isTextPixel(pixels[x]))
                left = qMin(left, x);
    }
    if (left == image.width())
        return {};
    const int width = qMax(1, qFloor(QFontMetricsF(font).horizontalAdvance(QStringLiteral("S")) * dpr) - 2);
    QRect result;
    for (int y = content.top(); y <= content.bottom(); ++y) {
        const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = left; x < qMin(left + width, image.width()); ++x)
            if (isTextPixel(pixels[x]))
                result |= QRect(x, y, 1, 1);
    }
    return result;
}

void saveCapture(const QImage& image, const QString& name)
{
    const QString directory = qEnvironmentVariable("FLUENT_QT_TEXT_ALIGNMENT_CAPTURE_DIR");
    if (directory.isEmpty())
        return;
    QDir().mkpath(directory);
    image.save(directory + '/' + name + QStringLiteral(".png"));
}

QImage renderControl(QWidget& widget, qreal dpr)
{
    QImage image = canvas(widget.size(), dpr);
    QPainter painter(&image);
    widget.render(&painter);
    return image;
}

void compareGlyphHeight(const QRect& reference, const QRect& actual)
{
    ASSERT_FALSE(reference.isEmpty());
    ASSERT_FALSE(actual.isEmpty());
    // Rasterization can differ by one physical pixel at a fractional DPR.
    // zh_CN: 分数 DPR 下栅格化最多允许一个物理像素的取整差异。
    EXPECT_NEAR(actual.top(), reference.top(), 1.0);
    EXPECT_NEAR(actual.bottom(), reference.bottom(), 1.0);
}

void verifyDelegate(QAbstractItemView* view, const QString& name)
{
    ASSERT_NE(view, nullptr) << name.toStdString();
    ASSERT_GE(view->model()->rowCount(), labels.size()) << name.toStdString();
    const QFont font = Typography::fontStyle(Typography::FontRole::Body).toQFont();
    for (qreal dpr : {1.0, 1.34, 2.0}) {
        SCOPED_TRACE(::testing::Message() << name.toStdString() << " DPR=" << dpr);
        QRect reference;
        for (int row = 0; row < labels.size(); ++row) {
            const QModelIndex index = view->model()->index(row, 0);
            SCOPED_TRACE(index.data().toString().toStdString());
            QStyleOptionViewItem option;
            option.initFrom(view);
            option.widget = view;
            option.font = font;
            option.fontMetrics = QFontMetrics(font);
            option.rect = QRect(0, 0, 260, 40);
            option.state = QStyle::State_Enabled;
            QImage image = canvas(option.rect.size(), dpr);
            {
                QPainter painter(&image);
                view->itemDelegate()->paint(&painter, option, index);
            }
            saveCapture(image, QStringLiteral("%1-%2-%3").arg(name).arg(dpr).arg(row));
            const QRect glyph = referenceGlyph(image, font);
            ASSERT_FALSE(glyph.isEmpty());
            if (!row)
                reference = glyph;
            else
                compareGlyphHeight(reference, glyph);
        }
    }
}
} // namespace

TEST(ControlTextAlignment, FieldsKeepTheSameGlyphBaselineWhenTheValueChanges)
{
    const auto previousTheme = fluent::FluentElement::currentTheme();
    const auto restoreTheme = qScopeGuard([&] { fluent::FluentElement::setTheme(previousTheme); });
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    QStandardItemModel model;
    for (const QString& label : labels)
        model.appendRow(new QStandardItem(label));
    QWidget host;
    host.resize(300, 200);
    fluent::basicinput::Button button(labels.first(), &host);
    fluent::basicinput::ComboBox combo(&host);
    fluent::basicinput::MultiSelectComboBox multi(&host);
    combo.addItems(labels);
    multi.setModel(&model);
    const QFont font = Typography::fontStyle(Typography::FontRole::Body).toQFont();
    int y = 10;
    for (QWidget* widget : QList<QWidget*>{&button, &combo, &multi}) {
        widget->setGeometry(10, y, 240, 32);
        widget->setFont(font);
        y += 50;
    }
    host.show();
    QApplication::processEvents();
    for (qreal dpr : {1.0, 1.34, 2.0}) {
        SCOPED_TRACE(dpr);
        QRect buttonReference, comboReference, multiReference;
        for (int row = 0; row < labels.size(); ++row) {
            SCOPED_TRACE(labels[row].toStdString());
            button.setText(labels[row]);
            combo.setCurrentIndex(row);
            multi.setSelectedRows({row});
            const QImage buttonImage = renderControl(button, dpr);
            const QImage comboImage = renderControl(combo, dpr);
            const QImage multiImage = renderControl(multi, dpr);
            saveCapture(buttonImage, QStringLiteral("button-%1-%2").arg(dpr).arg(row));
            saveCapture(comboImage, QStringLiteral("combo-field-%1-%2").arg(dpr).arg(row));
            saveCapture(multiImage, QStringLiteral("multi-field-%1-%2").arg(dpr).arg(row));
            const QRect buttonGlyph = referenceGlyph(buttonImage, font);
            const QRect comboGlyph = referenceGlyph(comboImage, font);
            const QRect multiGlyph = referenceGlyph(multiImage, font);
            if (!row) {
                buttonReference = buttonGlyph;
                comboReference = comboGlyph;
                multiReference = multiGlyph;
            }
            compareGlyphHeight(buttonReference, buttonGlyph);
            compareGlyphHeight(comboReference, comboGlyph);
            compareGlyphHeight(multiReference, multiGlyph);
            compareGlyphHeight(buttonGlyph, comboGlyph);
            compareGlyphHeight(buttonGlyph, multiGlyph);
        }
    }
}

TEST(ControlTextAlignment, OptionAndSuggestionDelegatesKeepOneBaselineAcrossLabels)
{
    const auto previousTheme = fluent::FluentElement::currentTheme();
    const auto restoreTheme = qScopeGuard([&] { fluent::FluentElement::setTheme(previousTheme); });
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    QStandardItemModel model;
    for (int row = 0; row < labels.size(); ++row) {
        auto* item = new QStandardItem(labels[row]);
        item->setData(QString::number(row), Qt::UserRole + 1);
        model.appendRow(item);
    }
    QWidget host;
    host.resize(400, 420);
    fluent::collections::ListView list(&host);
    fluent::basicinput::ComboBox combo(&host);
    fluent::basicinput::MultiSelectComboBox multi(&host);
    fluent::basicinput::TreeSelect tree(&host);
    fluent::textfields::AutoSuggestBox suggest(&host);
    list.setModel(&model);
    combo.addItems(labels);
    multi.setModel(&model);
    tree.setModel(&model);
    suggest.setSuggestions(labels);
    int y = 10;
    for (QWidget* widget : QList<QWidget*>{&list, &combo, &multi, &tree, &suggest}) {
        widget->setGeometry(10, y, 260, 40);
        y += 60;
    }
    host.show();
    QApplication::processEvents();
    verifyDelegate(&list, QStringLiteral("list"));
    combo.showPopup();
    QApplication::processEvents();
    verifyDelegate(host.findChild<QAbstractItemView*>(QStringLiteral("ComboBoxPopupListView")),
        QStringLiteral("combo-options"));
    combo.hidePopup();
    multi.open();
    QApplication::processEvents();
    verifyDelegate(host.findChild<QAbstractItemView*>(QStringLiteral("MultiSelectComboBox.ListView")),
        QStringLiteral("multi-options"));
    multi.close();
    tree.open();
    QApplication::processEvents();
    verifyDelegate(host.findChild<QAbstractItemView*>(QStringLiteral("TreeSelectTree")), QStringLiteral("tree"));
    tree.close();
    ASSERT_TRUE(tests::support::showAndFocusWidget(&suggest));
    suggest.setText(QStringLiteral("Settin"));
    QTest::keyClick(&suggest, Qt::Key_Down);
    QApplication::processEvents();
    verifyDelegate(host.findChild<QAbstractItemView*>(QStringLiteral("AutoSuggestBoxSuggestionList")),
        QStringLiteral("suggestions"));
}

namespace {
void expectCenteredBadgeInk(const QStringList& texts, bool shaped)
{
    const QFont font = Typography::fontStyle(Typography::FontRole::Body).toQFont();
    for (qreal dpr : {1.0, 1.34, 2.0}) {
        for (const QString& text : texts) {
            SCOPED_TRACE(::testing::Message() << text.toStdString() << " DPR=" << dpr);
            QImage image = canvas(QSize(120, 40), dpr);
            {
                QPainter painter(&image);
                painter.setFont(font);
                painter.setPen(Qt::black);
                const QRectF target(0, 0, 120, 40);
                const QRectF centered = shaped
                    ? fluent::painting::verticallyCenteredTextInkRect(target, painter, text)
                    : fluent::painting::verticallyCenteredTextInkRect(target,
                          QFontMetricsF(painter.font(), painter.device()), text);
                painter.drawText(centered, Qt::AlignHCenter | Qt::AlignVCenter, text);
            }
            QRect ink;
            for (int y = 0; y < image.height(); ++y) {
                const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
                for (int x = 0; x < image.width(); ++x)
                    // This canvas contains only black text on white. Include
                    // subpixel edge coverage: the chroma filter used for the
                    // colorful controls above would discard valid fallback ink.
                    // zh_CN: 此画布只有白底黑字，须纳入子像素边缘；上方彩色控件的色度过滤会丢失有效回退字形。
                    if (qGray(pixels[x]) < 100)
                        ink |= QRect(x, y, 1, 1);
            }
            ASSERT_FALSE(ink.isEmpty());
            EXPECT_NEAR((ink.top() + ink.bottom() + 1) / 2.0, 20 * image.devicePixelRatioF(), 1.0);
        }
    }
}
} // namespace

TEST(ControlTextAlignment, IsolatedBadgesCenterInkAtDifferentPixelRatios)
{
    expectCenteredBadgeInk({QStringLiteral("27"), QStringLiteral("gyp"), QString::fromUtf8("中文")}, false);
}

TEST(ControlTextAlignment, ShapedBadgesCenterMixedFallbackInkAtDifferentPixelRatios)
{
    expectCenteredBadgeInk(
        {QStringLiteral("27"), QStringLiteral("gyp"), QString::fromUtf8("中文"), QString::fromUtf8("A中g")}, true);
}

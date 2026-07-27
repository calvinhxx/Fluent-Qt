#include "FoundationSamples.h"

#include <QBoxLayout>
#include <QColor>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "components/foundation/FontIcon.h"
#include "components/layout/Card.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::FontIcon;
using fluent::layout::Card;
using fluent::textfields::Label;
using samples::horizontalGroup;
using samples::makeSample;

QBoxLayout* boxLayout(QWidget* widget)
{
    return qobject_cast<QBoxLayout*>(widget ? widget->layout() : nullptr);
}

QWidget* iconCell(QWidget* parent,
                  const QString& glyph,
                  int size,
                  const QString& caption,
                  const QColor& color = QColor(),
                  qreal rotation = 0.0)
{
    auto* cell = new QWidget(parent);
    auto* layout = new QVBoxLayout(cell);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    auto* icon = new FontIcon(glyph, cell);
    icon->setIconSize(size);
    icon->setColor(color);
    icon->setRotation(rotation);
    icon->setAccessibleName(caption);

    auto* label = new Label(caption, cell);
    label->setFluentTypography(Typography::FontRole::Caption);
    label->setTextColorRole(Label::TextColorRole::Secondary);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon, 0, Qt::AlignHCenter);
    layout->addWidget(label);
    return cell;
}

QVector<GallerySample> fontIconSamples()
{
    return {
        makeSample(
            QStringLiteral("font-icon-optical-sizes"),
            QStringLiteral("Optical sizes"),
            QStringLiteral("FontIcon resolves the bundled glyph variant that best fits each requested icon size."),
            QStringLiteral("auto* compact = new FontIcon(Typography::Icons::Search, this);\n"
                           "compact->setIconSize(Typography::IconSize::Compact); // 16 px\n"
                           "\n"
                           "auto* standard = new FontIcon(Typography::Icons::Search, this);\n"
                           "standard->setIconSize(Typography::IconSize::Standard); // 20 px\n"
                           "\n"
                           "auto* medium = new FontIcon(Typography::Icons::Search, this);\n"
                           "medium->setIconSize(24);\n"
                           "\n"
                           "auto* large = new FontIcon(Typography::Icons::Search, this);\n"
                           "large->setIconSize(32);"),
            [](QWidget* parent) {
                auto* card = new Card(parent);
                card->setFixedSize(420, 104);
                auto* layout = new QHBoxLayout(card);
                layout->setContentsMargins(18, 14, 18, 14);
                layout->setSpacing(22);
                layout->addWidget(iconCell(
                    card,
                    Typography::Icons::Search,
                    Typography::IconSize::Compact,
                    QStringLiteral("16 px")));
                layout->addWidget(iconCell(
                    card,
                    Typography::Icons::Search,
                    Typography::IconSize::Standard,
                    QStringLiteral("20 px")));
                layout->addWidget(iconCell(
                    card,
                    Typography::Icons::Search,
                    24,
                    QStringLiteral("24 px")));
                layout->addWidget(iconCell(
                    card,
                    Typography::Icons::Search,
                    32,
                    QStringLiteral("32 px")));
                layout->addStretch(1);
                return card;
            }),
        makeSample(
            QStringLiteral("font-icon-color-rotation"),
            QStringLiteral("Color and rotation"),
            QStringLiteral("A semantic glyph can inherit the theme color or use an explicit color and rotation."),
            QStringLiteral("auto* inherited = new FontIcon(Typography::Icons::Forward, this);\n"
                           "inherited->setIconSize(24);\n"
                           "\n"
                           "auto* accent = new FontIcon(Typography::Icons::Forward, this);\n"
                           "accent->setIconSize(24);\n"
                           "accent->setColor(QColor(\"#0F6CBD\"));\n"
                           "accent->setRotation(90.0);\n"
                           "\n"
                           "auto* warning = new FontIcon(Typography::Icons::Forward, this);\n"
                           "warning->setIconSize(24);\n"
                           "warning->setColor(QColor(\"#F7630C\"));\n"
                           "warning->setRotation(180.0);"),
            [](QWidget* parent) {
                QWidget* row = horizontalGroup(parent, 20);
                boxLayout(row)->addWidget(iconCell(
                    row,
                    Typography::Icons::Forward,
                    24,
                    QStringLiteral("Inherited")));
                boxLayout(row)->addWidget(iconCell(
                    row,
                    Typography::Icons::Forward,
                    24,
                    QStringLiteral("Accent · 90°"),
                    QColor(QStringLiteral("#0F6CBD")),
                    90.0));
                boxLayout(row)->addWidget(iconCell(
                    row,
                    Typography::Icons::Forward,
                    24,
                    QStringLiteral("Warning · 180°"),
                    QColor(QStringLiteral("#F7630C")),
                    180.0));
                return row;
            })
    };
}

} // namespace

QVector<GallerySample> foundationSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("font-icon"))
        return fontIconSamples();
    return {};
}

} // namespace fluent::gallery

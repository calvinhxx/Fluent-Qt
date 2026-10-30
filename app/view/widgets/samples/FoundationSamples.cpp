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
    qreal rotation = 0.0,
    const QString& fontFamily = QString(),
    bool enabled = true)
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
    icon->setFontFamily(fontFamily);
    icon->setEnabled(enabled);
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
        makeSample(QStringLiteral("font-icon-optical-sizes"), QStringLiteral("Optical sizes"),
            QStringLiteral("FontIcon resolves the bundled glyph variant that best fits each requested icon size."),
            QStringLiteral(
                "auto* compact = new FontIcon(Typography::Icons::Search, this);\n"
                "compact->setIconSize(Typography::IconSize::Compact); // 12 px\n"
                "\n"
                "auto* standard = new FontIcon(Typography::Icons::Search, this);\n"
                "standard->setIconSize(Typography::IconSize::Standard); // 16 px\n"
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
                layout->addWidget(
                    iconCell(card, Typography::Icons::Search, Typography::IconSize::Compact, QStringLiteral("12 px")));
                layout->addWidget(
                    iconCell(card, Typography::Icons::Search, Typography::IconSize::Standard, QStringLiteral("16 px")));
                layout->addWidget(iconCell(card, Typography::Icons::Search, 24, QStringLiteral("24 px")));
                layout->addWidget(iconCell(card, Typography::Icons::Search, 32, QStringLiteral("32 px")));
                layout->addStretch(1);
                return card;
            }),
        makeSample(QStringLiteral("font-icon-color-rotation"), QStringLiteral("Color and rotation"),
            QStringLiteral("A semantic glyph can inherit the theme color or use an explicit color and rotation."),
            QStringLiteral(
                "auto* inherited = new FontIcon(Typography::Icons::Forward, this);\n"
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
                boxLayout(row)->addWidget(iconCell(row, Typography::Icons::Forward, 24, QStringLiteral("Inherited")));
                boxLayout(row)->addWidget(iconCell(row, Typography::Icons::Forward, 24, QStringLiteral("Accent · 90°"),
                    QColor(QStringLiteral("#0F6CBD")), 90.0));
                boxLayout(row)->addWidget(iconCell(row, Typography::Icons::Forward, 24,
                    QStringLiteral("Warning · 180°"), QColor(QStringLiteral("#F7630C")), 180.0));
                return row;
            }),
        makeSample(QStringLiteral("font-icon-custom-font"), QStringLiteral("Custom font family"),
            QStringLiteral(
                "This example draws F using the already registered UI font. "
                "Register your own icon font with QFontDatabase and pass its family the same way. "
                "An empty family restores bundled Fluent icons."),
            QStringLiteral(
                "const QString family = Typography::fontStyle(Typography::FontRole::Body).family;\n"
                "auto* custom = new FontIcon(QStringLiteral(\"F\"), this);\n"
                "custom->setFontFamily(family);\n"
                "custom->setIconSize(24);\n"
                "\n"
                "auto* rotated = new FontIcon(QStringLiteral(\"F\"), this);\n"
                "rotated->setFontFamily(family);\n"
                "rotated->setIconSize(24);\n"
                "rotated->setRotation(90.0);\n"
                "\n"
                "auto* disabled = new FontIcon(QStringLiteral(\"F\"), this);\n"
                "disabled->setFontFamily(family);\n"
                "disabled->setIconSize(24);\n"
                "disabled->setEnabled(false);"),
            [](QWidget* parent) {
                QWidget* row = horizontalGroup(parent, 20);
                const QString family = Typography::fontStyle(Typography::FontRole::Body).family;
                boxLayout(row)->addWidget(
                    iconCell(row, QStringLiteral("F"), 24, QStringLiteral("UI font"), QColor(), 0.0, family));
                boxLayout(row)->addWidget(
                    iconCell(row, QStringLiteral("F"), 24, QStringLiteral("Rotated · 90°"), QColor(), 90.0, family));
                boxLayout(row)->addWidget(
                    iconCell(row, QStringLiteral("F"), 24, QStringLiteral("Disabled"), QColor(), 0.0, family, false));
                return row;
            })};
}

} // namespace

QVector<GallerySample> foundationSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("font-icon"))
        return fontIconSamples();
    return {};
}

} // namespace fluent::gallery

#include "LayoutSamples.h"

#include <QBoxLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "components/foundation/WidgetOwnership.h"
#include "components/layout/Accordion.h"
#include "components/layout/Card.h"
#include "components/layout/Divider.h"
#include "components/layout/Expander.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::layout::Accordion;
using fluent::layout::Card;
using fluent::layout::Divider;
using fluent::layout::Expander;
using fluent::textfields::Label;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

QBoxLayout* boxLayout(QWidget* widget)
{
    return qobject_cast<QBoxLayout*>(widget ? widget->layout() : nullptr);
}

Label* sampleLabel(QWidget* parent,
                   const QString& text,
                   Typography::FontRole role = Typography::FontRole::Body,
                   Label::TextColorRole colorRole =
                       Label::TextColorRole::Primary)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(role);
    label->setTextColorRole(colorRole);
    label->setWordWrap(true);
    return label;
}

Card* appearanceCard(QWidget* parent,
                     const QString& title,
                     const QString& detail,
                     Card::Appearance appearance,
                     bool borderVisible = true)
{
    auto* card = new Card(parent);
    card->setAppearance(appearance);
    card->setBorderVisible(borderVisible);
    card->setFixedSize(170, 88);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(3);
    layout->addWidget(sampleLabel(
        card, title, Typography::FontRole::BodyStrong));
    layout->addWidget(sampleLabel(
        card,
        detail,
        Typography::FontRole::Caption,
        Label::TextColorRole::Secondary));
    layout->addStretch(1);
    return card;
}

QWidget* expanderBody(const QString& detail)
{
    auto* body = new QWidget;
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 12, 16, 14);
    layout->setSpacing(4);
    layout->addWidget(sampleLabel(
        body,
        QStringLiteral("Additional details"),
        Typography::FontRole::BodyStrong));
    layout->addWidget(sampleLabel(body, detail));
    return body;
}

Expander* accordionItem(QWidget* parent,
                        const QString& title,
                        const QString& detail)
{
    auto* item = new Expander(parent);
    item->setHeaderText(title);
    item->setContentWidget(
        expanderBody(detail), WidgetOwnership::Owned);
    return item;
}

QVector<GallerySample> cardSamples()
{
    return {
        makeSample(
            QStringLiteral("card-surface-appearances"),
            QStringLiteral("Surface appearances"),
            QStringLiteral("Choose a semantic surface token so grouped content follows the active theme."),
            QStringLiteral("auto* layer = new Card(this);\n"
                           "layer->setAppearance(Card::Layer);\n"
                           "layer->setFixedSize(170, 88);\n"
                           "auto* layerLayout = new QVBoxLayout(layer);\n"
                           "layerLayout->setContentsMargins(16, 12, 16, 12);\n"
                           "layerLayout->addWidget(new Label(\"Layer\", layer));\n"
                           "layerLayout->addWidget(new Label(\"Default grouped surface\", layer));\n"
                           "\n"
                           "auto* alternate = new Card(this);\n"
                           "alternate->setAppearance(Card::LayerAlt);\n"
                           "alternate->setFixedSize(170, 88);\n"
                           "auto* alternateLayout = new QVBoxLayout(alternate);\n"
                           "alternateLayout->setContentsMargins(16, 12, 16, 12);\n"
                           "alternateLayout->addWidget(new Label(\"LayerAlt\", alternate));\n"
                           "alternateLayout->addWidget(new Label(\"Alternate layer tone\", alternate));\n"
                           "\n"
                           "auto* canvas = new Card(this);\n"
                           "canvas->setAppearance(Card::Canvas);\n"
                           "canvas->setFixedSize(170, 88);\n"
                           "auto* canvasLayout = new QVBoxLayout(canvas);\n"
                           "canvasLayout->setContentsMargins(16, 12, 16, 12);\n"
                           "canvasLayout->addWidget(new Label(\"Canvas\", canvas));\n"
                           "canvasLayout->addWidget(new Label(\"Matches the page canvas\", canvas));"),
            [](QWidget* parent) {
                QWidget* row = horizontalGroup(parent, 12);
                boxLayout(row)->addWidget(appearanceCard(
                    row,
                    QStringLiteral("Layer"),
                    QStringLiteral("Default grouped surface"),
                    Card::Layer));
                boxLayout(row)->addWidget(appearanceCard(
                    row,
                    QStringLiteral("LayerAlt"),
                    QStringLiteral("Alternate layer tone"),
                    Card::LayerAlt));
                boxLayout(row)->addWidget(appearanceCard(
                    row,
                    QStringLiteral("Canvas"),
                    QStringLiteral("Matches the page canvas"),
                    Card::Canvas));
                return row;
            }),
        makeSample(
            QStringLiteral("card-border-visibility"),
            QStringLiteral("Optional border"),
            QStringLiteral("Keep the hairline for discrete cards or remove it when the surrounding layout already defines the group."),
            QStringLiteral("auto* bordered = new Card(this);\n"
                           "bordered->setAppearance(Card::Layer);\n"
                           "bordered->setBorderVisible(true);\n"
                           "bordered->setFixedSize(170, 88);\n"
                           "auto* borderedLayout = new QVBoxLayout(bordered);\n"
                           "borderedLayout->addWidget(new Label(\"Bordered\", bordered));\n"
                           "borderedLayout->addWidget(new Label(\"Independent surface\", bordered));\n"
                           "\n"
                           "auto* borderless = new Card(this);\n"
                           "borderless->setAppearance(Card::Layer);\n"
                           "borderless->setBorderVisible(false);\n"
                           "borderless->setFixedSize(170, 88);\n"
                           "auto* borderlessLayout = new QVBoxLayout(borderless);\n"
                           "borderlessLayout->addWidget(new Label(\"Borderless\", borderless));\n"
                           "borderlessLayout->addWidget(new Label(\"Nested composition\", borderless));"),
            [](QWidget* parent) {
                QWidget* row = horizontalGroup(parent, 12);
                boxLayout(row)->addWidget(appearanceCard(
                    row,
                    QStringLiteral("Bordered"),
                    QStringLiteral("Independent surface"),
                    Card::Layer,
                    true));
                boxLayout(row)->addWidget(appearanceCard(
                    row,
                    QStringLiteral("Borderless"),
                    QStringLiteral("Nested composition"),
                    Card::Layer,
                    false));
                return row;
            })
    };
}

QVector<GallerySample> dividerSamples()
{
    return {
        makeSample(
            QStringLiteral("divider-horizontal-insets"),
            QStringLiteral("Horizontal dividers and insets"),
            QStringLiteral("Insets align a separator with the text or icon content around it."),
            QStringLiteral("auto* card = new Card(this);\n"
                           "auto* layout = new QVBoxLayout(card);\n"
                           "layout->setContentsMargins(16, 14, 16, 14);\n"
                           "layout->setSpacing(10);\n"
                           "\n"
                           "layout->addWidget(new Label(\"Full-width separator\", card));\n"
                           "layout->addWidget(new Divider(card));\n"
                           "\n"
                           "layout->addWidget(new Label(\"Inset separator\", card));\n"
                           "auto* inset = new Divider(card);\n"
                           "inset->setLeadingInset(24);\n"
                           "inset->setTrailingInset(48);\n"
                           "layout->addWidget(inset);"),
            [](QWidget* parent) {
                auto* card = new Card(parent);
                card->setFixedSize(520, 116);
                auto* layout = new QVBoxLayout(card);
                layout->setContentsMargins(16, 14, 16, 14);
                layout->setSpacing(10);
                layout->addWidget(sampleLabel(
                    card,
                    QStringLiteral("Full-width separator"),
                    Typography::FontRole::BodyStrong));
                layout->addWidget(new Divider(card));
                layout->addWidget(sampleLabel(
                    card,
                    QStringLiteral("Inset separator"),
                    Typography::FontRole::BodyStrong));
                auto* inset = new Divider(card);
                inset->setLeadingInset(24);
                inset->setTrailingInset(48);
                layout->addWidget(inset);
                return card;
            }),
        makeSample(
            QStringLiteral("divider-vertical-orientation"),
            QStringLiteral("Vertical separators"),
            QStringLiteral("Vertical orientation divides neighboring commands or content regions without adding interaction."),
            QStringLiteral("auto* card = new Card(this);\n"
                           "auto* layout = new QHBoxLayout(card);\n"
                           "layout->setContentsMargins(20, 16, 20, 16);\n"
                           "layout->setSpacing(16);\n"
                           "\n"
                           "layout->addWidget(new Label(\"Details\", card));\n"
                           "\n"
                           "auto* first = new Divider(Qt::Vertical, card);\n"
                           "first->setLeadingInset(4);\n"
                           "first->setTrailingInset(4);\n"
                           "first->setFixedHeight(44);\n"
                           "layout->addWidget(first);\n"
                           "\n"
                           "layout->addWidget(new Label(\"Activity\", card));\n"
                           "\n"
                           "auto* second = new Divider(Qt::Vertical, card);\n"
                           "second->setLeadingInset(4);\n"
                           "second->setTrailingInset(4);\n"
                           "second->setFixedHeight(44);\n"
                           "layout->addWidget(second);\n"
                           "\n"
                           "layout->addWidget(new Label(\"History\", card));"),
            [](QWidget* parent) {
                auto* card = new Card(parent);
                card->setFixedSize(420, 76);
                auto* layout = new QHBoxLayout(card);
                layout->setContentsMargins(20, 16, 20, 16);
                layout->setSpacing(16);
                layout->addWidget(sampleLabel(card, QStringLiteral("Details")));
                auto* first = new Divider(Qt::Vertical, card);
                first->setLeadingInset(4);
                first->setTrailingInset(4);
                first->setFixedHeight(44);
                layout->addWidget(first);
                layout->addWidget(sampleLabel(card, QStringLiteral("Activity")));
                auto* second = new Divider(Qt::Vertical, card);
                second->setLeadingInset(4);
                second->setTrailingInset(4);
                second->setFixedHeight(44);
                layout->addWidget(second);
                layout->addWidget(sampleLabel(card, QStringLiteral("History")));
                layout->addStretch(1);
                return card;
            })
    };
}

QVector<GallerySample> expanderSamples()
{
    return {
        makeSample(
            QStringLiteral("expander-text-content"),
            QStringLiteral("Header and content"),
            QStringLiteral("Use an Expander to keep secondary details available without showing them all the time."),
            QStringLiteral("auto* expander = new Expander(this);\n"
                           "expander->setHeaderText(\"Connection details\");\n"
                           "\n"
                           "auto* body = new QWidget;\n"
                           "auto* bodyLayout = new QVBoxLayout(body);\n"
                           "bodyLayout->setContentsMargins(16, 12, 16, 14);\n"
                           "bodyLayout->setSpacing(4);\n"
                           "bodyLayout->addWidget(new Label(\"Additional details\", body));\n"
                           "bodyLayout->addWidget(new Label(\n"
                           "    \"Server: api.example.com\\nTransport: TLS 1.3\", body));\n"
                           "\n"
                           "expander->setContentWidget(body, WidgetOwnership::Owned);\n"
                           "expander->setExpandedAnimated(true, false);"),
            [](QWidget* parent) {
                QWidget* group = verticalGroup(parent);
                auto* expander = new Expander(group);
                expander->setObjectName(
                    QStringLiteral("galleryExpanderTextContent"));
                expander->setFixedWidth(520);
                expander->setHeaderText(
                    QStringLiteral("Connection details"));
                expander->setContentWidget(
                    expanderBody(QStringLiteral(
                        "Server: api.example.com\nTransport: TLS 1.3")),
                    WidgetOwnership::Owned);
                expander->setExpandedAnimated(true, false);
                boxLayout(group)->addWidget(expander);
                return group;
            }),
        makeSample(
            QStringLiteral("expander-state-signal"),
            QStringLiteral("Expanded state"),
            QStringLiteral("The expandedChanged signal keeps nearby status or application state synchronized with disclosure."),
            QStringLiteral("auto* status = new Label(\"Collapsed\", this);\n"
                           "\n"
                           "auto* options = new QWidget;\n"
                           "auto* optionsLayout = new QVBoxLayout(options);\n"
                           "optionsLayout->setContentsMargins(16, 12, 16, 14);\n"
                           "optionsLayout->addWidget(new Label(\"Additional details\", options));\n"
                           "optionsLayout->addWidget(new Label(\n"
                           "    \"Diagnostic logging and retry behavior.\", options));\n"
                           "\n"
                           "auto* expander = new Expander(this);\n"
                           "expander->setHeaderText(\"Advanced options\");\n"
                           "expander->setContentWidget(options, WidgetOwnership::Owned);\n"
                           "\n"
                           "connect(expander, &Expander::expandedChanged,\n"
                           "        status, [status](bool expanded) {\n"
                           "    status->setText(expanded ? \"Expanded\" : \"Collapsed\");\n"
                           "});"),
            [](QWidget* parent) {
                QWidget* group = verticalGroup(parent, 8);
                auto* status = sampleLabel(
                    group,
                    QStringLiteral("Collapsed"),
                    Typography::FontRole::Caption);
                status->setObjectName(
                    QStringLiteral("galleryExpanderStateLabel"));

                auto* expander = new Expander(group);
                expander->setFixedWidth(520);
                expander->setHeaderText(
                    QStringLiteral("Advanced options"));
                expander->setContentWidget(
                    expanderBody(QStringLiteral(
                        "Diagnostic logging and retry behavior.")),
                    WidgetOwnership::Owned);
                QObject::connect(
                    expander,
                    &Expander::expandedChanged,
                    status,
                    [status](bool expanded) {
                        status->setText(
                            expanded
                                ? QStringLiteral("Expanded")
                                : QStringLiteral("Collapsed"));
                    });
                boxLayout(group)->addWidget(expander);
                boxLayout(group)->addWidget(status);
                return group;
            })
    };
}

QVector<GallerySample> accordionSamples()
{
    return {
        makeSample(
            QStringLiteral("accordion-single-expansion"),
            QStringLiteral("Single expansion"),
            QStringLiteral("Single mode closes the previous section when another header is opened."),
            QStringLiteral("auto* accordion = new Accordion(this);\n"
                           "accordion->setExpansionMode(\n"
                           "    Accordion::ExpansionMode::Single);\n"
                           "accordion->setFixedWidth(520);\n"
                           "\n"
                           "auto makeSection = [](const QString& title,\n"
                           "                     const QString& detail) {\n"
                           "    auto* item = new Expander;\n"
                           "    item->setHeaderText(title);\n"
                           "    auto* body = new QWidget;\n"
                           "    auto* bodyLayout = new QVBoxLayout(body);\n"
                           "    bodyLayout->setContentsMargins(16, 12, 16, 14);\n"
                           "    bodyLayout->addWidget(\n"
                           "        new Label(\"Additional details\", body));\n"
                           "    bodyLayout->addWidget(new Label(detail, body));\n"
                           "    item->setContentWidget(\n"
                           "        body, WidgetOwnership::Owned);\n"
                           "    return item;\n"
                           "};\n"
                           "\n"
                           "accordion->addItem(\n"
                           "    makeSection(\"Account\",\n"
                           "        \"Profile, sign-in, and recovery options.\"),\n"
                           "    WidgetOwnership::Owned);\n"
                           "accordion->addItem(\n"
                           "    makeSection(\"Notifications\",\n"
                           "        \"Choose which activity can interrupt you.\"),\n"
                           "    WidgetOwnership::Owned);\n"
                           "accordion->addItem(\n"
                           "    makeSection(\"Privacy\",\n"
                           "        \"Review diagnostics and personalization settings.\"),\n"
                           "    WidgetOwnership::Owned);\n"
                           "accordion->itemAt(0)->setExpandedAnimated(true, false);"),
            [](QWidget* parent) {
                QWidget* group = verticalGroup(parent);
                auto* accordion = new Accordion(group);
                accordion->setObjectName(
                    QStringLiteral("galleryAccordionSingle"));
                accordion->setFixedWidth(520);
                accordion->addItem(
                    accordionItem(
                        accordion,
                        QStringLiteral("Account"),
                        QStringLiteral(
                            "Profile, sign-in, and recovery options.")),
                    WidgetOwnership::Owned);
                accordion->addItem(
                    accordionItem(
                        accordion,
                        QStringLiteral("Notifications"),
                        QStringLiteral(
                            "Choose which activity can interrupt you.")),
                    WidgetOwnership::Owned);
                accordion->addItem(
                    accordionItem(
                        accordion,
                        QStringLiteral("Privacy"),
                        QStringLiteral(
                            "Review diagnostics and personalization settings.")),
                    WidgetOwnership::Owned);
                accordion->itemAt(0)->setExpandedAnimated(true, false);
                boxLayout(group)->addWidget(accordion);
                return group;
            }),
        makeSample(
            QStringLiteral("accordion-multiple-expansion"),
            QStringLiteral("Multiple expansion and keyboard navigation"),
            QStringLiteral("Multiple mode keeps independent sections open; Up, Down, Home, and End move focus between headers."),
            QStringLiteral("auto* accordion = new Accordion(this);\n"
                           "accordion->setExpansionMode(\n"
                           "    Accordion::ExpansionMode::Multiple);\n"
                           "accordion->setFixedWidth(520);\n"
                           "\n"
                           "auto* networkItem = new Expander;\n"
                           "networkItem->setHeaderText(\"Network\");\n"
                           "auto* networkBody = new QWidget;\n"
                           "auto* networkLayout = new QVBoxLayout(networkBody);\n"
                           "networkLayout->setContentsMargins(16, 12, 16, 14);\n"
                           "networkLayout->addWidget(\n"
                           "    new Label(\"Additional details\", networkBody));\n"
                           "networkLayout->addWidget(new Label(\n"
                           "    \"Wi-Fi and Ethernet are connected.\",\n"
                           "    networkBody));\n"
                           "networkItem->setContentWidget(\n"
                           "    networkBody, WidgetOwnership::Owned);\n"
                           "\n"
                           "auto* proxyItem = new Expander;\n"
                           "proxyItem->setHeaderText(\"Proxy\");\n"
                           "auto* proxyBody = new QWidget;\n"
                           "auto* proxyLayout = new QVBoxLayout(proxyBody);\n"
                           "proxyLayout->setContentsMargins(16, 12, 16, 14);\n"
                           "proxyLayout->addWidget(\n"
                           "    new Label(\"Additional details\", proxyBody));\n"
                           "proxyLayout->addWidget(new Label(\n"
                           "    \"Use system proxy settings.\", proxyBody));\n"
                           "proxyItem->setContentWidget(\n"
                           "    proxyBody, WidgetOwnership::Owned);\n"
                           "\n"
                           "accordion->addItem(\n"
                           "    networkItem, WidgetOwnership::Owned);\n"
                           "accordion->addItem(\n"
                           "    proxyItem, WidgetOwnership::Owned);\n"
                           "networkItem->setExpandedAnimated(true, false);\n"
                           "proxyItem->setExpandedAnimated(true, false);"),
            [](QWidget* parent) {
                QWidget* group = verticalGroup(parent, 8);
                auto* accordion = new Accordion(group);
                accordion->setObjectName(
                    QStringLiteral("galleryAccordionMultiple"));
                accordion->setFixedWidth(520);
                accordion->setExpansionMode(
                    Accordion::ExpansionMode::Multiple);

                auto* network = accordionItem(
                    accordion,
                    QStringLiteral("Network"),
                    QStringLiteral(
                        "Wi-Fi and Ethernet are connected."));
                auto* proxy = accordionItem(
                    accordion,
                    QStringLiteral("Proxy"),
                    QStringLiteral(
                        "Use system proxy settings."));
                accordion->addItem(network, WidgetOwnership::Owned);
                accordion->addItem(proxy, WidgetOwnership::Owned);
                network->setExpandedAnimated(true, false);
                proxy->setExpandedAnimated(true, false);
                boxLayout(group)->addWidget(accordion);
                return group;
            })
    };
}

} // namespace

QVector<GallerySample> layoutSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("accordion"))
        return accordionSamples();
    if (routeId == QStringLiteral("card"))
        return cardSamples();
    if (routeId == QStringLiteral("divider"))
        return dividerSamples();
    if (routeId == QStringLiteral("expander"))
        return expanderSamples();
    return {};
}

} // namespace fluent::gallery

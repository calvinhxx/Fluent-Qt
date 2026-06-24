#include "TextFieldsSamples.h"

#include <cmath>

#include <QHBoxLayout>
#include <QIntValidator>
#include <QPainter>
#include <QPoint>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/NumberBox.h"
#include "components/textfields/PasswordBox.h"
#include "components/textfields/TextEdit.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::textfields::AutoSuggestBox;
using fluent::textfields::Label;
using fluent::textfields::LineEdit;
using fluent::textfields::NumberBox;
using fluent::textfields::PasswordBox;
using fluent::textfields::TextEdit;
using samples::horizontalGroup;
using samples::makeSample;

class TextFieldSampleSurface : public QWidget, public fluent::FluentElement {
public:
    explicit TextFieldSampleSurface(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 16);
        layout->setSpacing(12);
        layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(themeColors().strokeCard);
        painter.setBrush(themeColors().bgCanvas);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1),
                                themeRadius().overlay,
                                themeRadius().overlay);
    }
};

class ElideValueCell : public QWidget, public fluent::FluentElement {
public:
    explicit ElideValueCell(const QString& text,
                            Qt::TextElideMode mode,
                            int labelWidth,
                            QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(labelWidth + 22, 34);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 0, 10, 0);
        layout->setSpacing(0);

        auto* label = new Label(text, this);
        label->setFluentTypography(Typography::FontRole::Body);
        label->setTextElideMode(mode);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setFixedWidth(labelWidth);
        layout->addWidget(label, 0, Qt::AlignVCenter);
    }

    void onThemeUpdated() override
    {
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF bgRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(themeColors().strokeDefault, 1));
        painter.setBrush(themeColors().controlDefault);
        painter.drawRoundedRect(bgRect, themeRadius().control, themeRadius().control);
    }
};

TextFieldSampleSurface* textFieldSurface(QWidget* parent, int spacing = 12)
{
    auto* surface = new TextFieldSampleSurface(parent);
    surface->layout()->setSpacing(spacing);
    return surface;
}

Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    label->setWordWrap(true);
    label->setTextColorRole(Label::TextColorRole::Primary);  // QSS-proof on the styled preview surface
    return label;
}

QString textChangeReasonName(AutoSuggestBox::TextChangeReason reason)
{
    switch (reason) {
    case AutoSuggestBox::TextChangeReason::UserInput:
        return QStringLiteral("UserInput");
    case AutoSuggestBox::TextChangeReason::ProgrammaticChange:
        return QStringLiteral("ProgrammaticChange");
    case AutoSuggestBox::TextChangeReason::SuggestionChosen:
        return QStringLiteral("SuggestionChosen");
    }
    return QString();
}

QString numberValueText(double value)
{
    if (std::isnan(value))
        return QStringLiteral("Value: NaN");
    return QStringLiteral("Value: %1").arg(value, 0, 'f', 2);
}

QString lineCountText(const TextEdit* textEdit)
{
    const int lineCount = qMax(1, textEdit->toPlainText().count(QLatin1Char('\n')) + 1);
    return QStringLiteral("Lines: %1, visible range: %2-%3")
        .arg(lineCount)
        .arg(textEdit->minVisibleLines())
        .arg(textEdit->maxVisibleLines());
}

const QStringList& fruitSuggestions()
{
    static const QStringList suggestions{
        QStringLiteral("Apple"),
        QStringLiteral("Apricot"),
        QStringLiteral("Banana"),
        QStringLiteral("Blueberry"),
        QStringLiteral("Cherry"),
        QStringLiteral("Grape"),
        QStringLiteral("Orange"),
        QStringLiteral("Strawberry")
    };
    return suggestions;
}

void addLabeledWidget(QWidget* parent,
                      QVBoxLayout* layout,
                      const QString& labelText,
                      QWidget* widget)
{
    auto* row = horizontalGroup(parent, 12);
    auto* rowLayout = static_cast<QHBoxLayout*>(row->layout());

    auto* label = makeStatusLabel(row, labelText);
    label->setMinimumWidth(label->fontMetrics().horizontalAdvance(
        QStringLiteral("Header placeholder")));

    rowLayout->addWidget(label, 0, Qt::AlignVCenter);
    rowLayout->addWidget(widget, 0, Qt::AlignVCenter);
    rowLayout->addStretch(1);
    layout->addWidget(row);
}

void addElideRow(QWidget* parent,
                 QVBoxLayout* layout,
                 const QString& caption,
                 const QString& text,
                 Qt::TextElideMode mode,
                 int width)
{
    auto* row = horizontalGroup(parent, 12);
    auto* rowLayout = static_cast<QHBoxLayout*>(row->layout());

    auto* captionLabel = makeStatusLabel(row, caption);
    captionLabel->setFluentTypography(Typography::FontRole::Caption);
    captionLabel->setMinimumWidth(captionLabel->fontMetrics().horizontalAdvance(
        QStringLiteral("ElideMiddle")) + 6);

    auto* valueCell = new ElideValueCell(text, mode, width, row);

    rowLayout->addWidget(captionLabel, 0, Qt::AlignVCenter);
    rowLayout->addWidget(valueCell, 0, Qt::AlignVCenter);
    rowLayout->addStretch(1);
    layout->addWidget(row);
}

QVector<GallerySample> autoSuggestBoxSamples()
{
    return {
        makeSample(QStringLiteral("auto-suggest-box-suggestions"),
                   QStringLiteral("Suggestions and query submit"),
                   QStringLiteral("Suggestions, selection, and query submission are surfaced as separate signals."),
                   QStringLiteral("auto* box = new AutoSuggestBox(this);\n"
                                  "box->setHeader(\"Fruit\");\n"
                                  "box->setPlaceholderText(\"Type a fruit name\");\n"
                                  "box->setSuggestions({\"Apple\", \"Apricot\", \"Banana\", \"Blueberry\"});\n"
                                  "\n"
                                  "connect(box, &AutoSuggestBox::textChangedWithReason,\n"
                                  "        statusLabel, [statusLabel](const QString& text,\n"
                                  "                        AutoSuggestBox::TextChangeReason reason) {\n"
                                  "            statusLabel->setText(QStringLiteral(\"Text: %1, reason: %2\")\n"
                                  "                .arg(text, textChangeReasonName(reason)));\n"
                                  "        });\n"
                                  "connect(box, &AutoSuggestBox::suggestionChosen,\n"
                                  "        statusLabel, [statusLabel](const QVariant& item) {\n"
                                  "            statusLabel->setText(QStringLiteral(\"Chosen: %1\").arg(item.toString()));\n"
                                  "        });\n"
                                  "connect(box, &AutoSuggestBox::querySubmitted,\n"
                                  "        statusLabel, [statusLabel](const QString& query, const QVariant& chosen) {\n"
                                  "            statusLabel->setText(QStringLiteral(\"Submitted: %1 (%2)\")\n"
                                  "                .arg(query, chosen.toString()));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* box = new AutoSuggestBox(surface);
                       box->setHeader(QStringLiteral("Fruit"));
                       box->setPlaceholderText(QStringLiteral("Type a fruit name"));
                       box->setSuggestions(fruitSuggestions());
                       box->setFixedWidth(320);

                       auto* status = makeStatusLabel(
                           surface, QStringLiteral("Text: , reason: ProgrammaticChange"));
                       status->setMinimumWidth(status->fontMetrics().horizontalAdvance(
                           QStringLiteral("Submitted: Strawberry (Strawberry)")));

                       QObject::connect(box, &AutoSuggestBox::textChangedWithReason,
                                        status,
                                        [status](const QString& text, AutoSuggestBox::TextChangeReason reason) {
                                            status->setText(QStringLiteral("Text: %1, reason: %2")
                                                                .arg(text, textChangeReasonName(reason)));
                                        });
                       QObject::connect(box, &AutoSuggestBox::suggestionChosen,
                                        status, [status](const QVariant& item) {
                                            status->setText(QStringLiteral("Chosen: %1").arg(item.toString()));
                                        });
                       QObject::connect(box, &AutoSuggestBox::querySubmitted,
                                        status, [status](const QString& query, const QVariant& chosen) {
                                            status->setText(QStringLiteral("Submitted: %1 (%2)")
                                                                .arg(query, chosen.toString()));
                                        });

                       layout->addWidget(box);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("auto-suggest-box-query-placement"),
                   QStringLiteral("Search-style query button"),
                   QStringLiteral("The query affordance can move left and use compact suggestion rows."),
                   QStringLiteral("auto* box = new AutoSuggestBox(this);\n"
                                  "box->setPlaceholderText(\"Filter commands\");\n"
                                  "box->setSuggestions({\"Open\", \"Open recent\", \"Save\", \"Save as\"});\n"
                                  "box->setQueryButtonPlacement(AutoSuggestBox::QueryButtonPlacement::Left);\n"
                                  "box->setQueryIconGlyph(Typography::Icons::Filter);\n"
                                  "box->setInputHeight(28);\n"
                                  "box->setQueryButtonSize(20);\n"
                                  "box->setClearButtonSize(18);\n"
                                  "box->setSuggestionFontRole(Typography::FontRole::Caption);\n"
                                  "box->setSuggestionItemHeight(28);\n"
                                  "\n"
                                  "connect(box, &AutoSuggestBox::querySubmitted,\n"
                                  "        statusLabel, [statusLabel](const QString& query, const QVariant&) {\n"
                                  "            statusLabel->setText(QStringLiteral(\"Filter: %1\").arg(query));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* box = new AutoSuggestBox(surface);
                       box->setPlaceholderText(QStringLiteral("Filter commands"));
                       box->setSuggestions({QStringLiteral("Open"), QStringLiteral("Open recent"),
                                            QStringLiteral("Save"), QStringLiteral("Save as"),
                                            QStringLiteral("Settings"), QStringLiteral("Sync")});
                       box->setQueryButtonPlacement(AutoSuggestBox::QueryButtonPlacement::Left);
                       box->setQueryIconGlyph(Typography::Icons::Filter);
                       box->setInputHeight(28);
                       box->setQueryButtonSize(20);
                       box->setClearButtonSize(18);
                       box->setSuggestionFontRole(Typography::FontRole::Caption);
                       box->setSuggestionItemHeight(28);
                       box->setFixedWidth(320);

                       auto* status = makeStatusLabel(surface, QStringLiteral("Filter:"));
                       QObject::connect(box, &AutoSuggestBox::querySubmitted,
                                        status, [status](const QString& query, const QVariant&) {
                                            status->setText(QStringLiteral("Filter: %1").arg(query));
                                        });

                       layout->addWidget(box);
                       layout->addWidget(status);
                       return surface;
                   })
    };
}

QVector<GallerySample> labelSamples()
{
    return {
        makeSample(QStringLiteral("label-typography"),
                   QStringLiteral("Typography roles"),
                   QStringLiteral("Labels map text to the Fluent type ramp and preserve the role across theme updates."),
                   QStringLiteral("const QVector<QPair<QString, QString>> roles = {\n"
                                  "    {Typography::FontRole::Caption, \"Caption\"},\n"
                                  "    {Typography::FontRole::Body, \"Body\"},\n"
                                  "    {Typography::FontRole::BodyStrong, \"Body strong\"},\n"
                                  "    {Typography::FontRole::Subtitle, \"Subtitle\"},\n"
                                  "    {Typography::FontRole::Title, \"Title\"}\n"
                                  "};\n"
                                  "\n"
                                  "for (const auto& role : roles) {\n"
                                  "    auto* label = new Label(role.second, this);\n"
                                  "    label->setFluentTypography(role.first);\n"
                                  "}"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent, 8);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());
                       const QVector<QPair<QString, QString>> roles{
                           {Typography::FontRole::Caption, QStringLiteral("Caption")},
                           {Typography::FontRole::Body, QStringLiteral("Body")},
                           {Typography::FontRole::BodyStrong, QStringLiteral("Body strong")},
                           {Typography::FontRole::Subtitle, QStringLiteral("Subtitle")},
                           {Typography::FontRole::Title, QStringLiteral("Title")}
                       };
                       for (const auto& role : roles) {
                           auto* label = new Label(role.second, surface);
                           label->setFluentTypography(role.first);
                           layout->addWidget(label);
                       }
                       return surface;
                   }),
        makeSample(QStringLiteral("label-elide"),
                   QStringLiteral("Elided values with full-text tooltip"),
                   QStringLiteral("Elide modes keep the full text value while rendering a constrained label."),
                   QStringLiteral("auto* componentLabel = new Label(componentPathText, this);\n"
                                  "componentLabel->setTextElideMode(Qt::ElideMiddle);\n"
                                  "componentLabel->setFixedWidth(260);\n"
                                  "\n"
                                  "auto* releaseLabel = new Label(releaseText, this);\n"
                                  "releaseLabel->setTextElideMode(Qt::ElideRight);\n"
                                  "releaseLabel->setFixedWidth(190);\n"
                                  "\n"
                                  "auto* artifactLabel = new Label(artifactText, this);\n"
                                  "artifactLabel->setTextElideMode(Qt::ElideLeft);\n"
                                  "artifactLabel->setFixedWidth(240);"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent, 10);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       addElideRow(surface, layout,
                                   QStringLiteral("ElideRight"),
                                   QStringLiteral("Quarterly release summary for the planning review"),
                                   Qt::ElideRight,
                                   190);
                       addElideRow(surface, layout,
                                   QStringLiteral("ElideMiddle"),
                                   QStringLiteral("src/components/textfields/examples/LabelSample.cpp"),
                                   Qt::ElideMiddle,
                                   260);
                       addElideRow(surface, layout,
                                   QStringLiteral("ElideLeft"),
                                   QStringLiteral("fluent-qt-release-textfields-label-preview-bundle"),
                                   Qt::ElideLeft,
                                   240);
                       return surface;
                   })
    };
}

QVector<GallerySample> lineEditSamples()
{
    return {
        makeSample(QStringLiteral("line-edit-clear"),
                   QStringLiteral("Input with clear button"),
                   QStringLiteral("The clear button appears when text is present and text changes are easy to observe."),
                   QStringLiteral("auto* lineEdit = new LineEdit(this);\n"
                                  "lineEdit->setPlaceholderText(\"Enter your name\");\n"
                                  "lineEdit->setClearButtonEnabled(true);\n"
                                  "\n"
                                  "connect(lineEdit, &QLineEdit::textChanged,\n"
                                  "        statusLabel, [statusLabel](const QString& text) {\n"
                                  "            statusLabel->setText(QStringLiteral(\"Text length: %1\").arg(text.size()));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* lineEdit = new LineEdit(surface);
                       lineEdit->setPlaceholderText(QStringLiteral("Enter your name"));
                       lineEdit->setClearButtonEnabled(true);
                       lineEdit->setFixedWidth(300);

                       auto* status = makeStatusLabel(surface, QStringLiteral("Text length: 0"));
                       QObject::connect(lineEdit, &QLineEdit::textChanged,
                                        status, [status](const QString& text) {
                                            status->setText(QStringLiteral("Text length: %1").arg(text.size()));
                                        });

                       layout->addWidget(lineEdit);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("line-edit-validator"),
                   QStringLiteral("Validated numeric input"),
                   QStringLiteral("LineEdit can host a Qt validator while keeping Fluent visuals and clear-button behavior."),
                   QStringLiteral("auto* lineEdit = new LineEdit(this);\n"
                                  "lineEdit->setPlaceholderText(\"0-100\");\n"
                                  "lineEdit->setValidator(new QIntValidator(0, 100, lineEdit));\n"
                                  "lineEdit->setText(\"42\");\n"
                                  "\n"
                                  "connect(lineEdit, &QLineEdit::textChanged,\n"
                                  "        statusLabel, [lineEdit, statusLabel] {\n"
                                  "            statusLabel->setText(lineEdit->hasAcceptableInput()\n"
                                  "                ? \"Acceptable input\" : \"Out of range\");\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* lineEdit = new LineEdit(surface);
                       lineEdit->setPlaceholderText(QStringLiteral("0-100"));
                       lineEdit->setValidator(new QIntValidator(0, 100, lineEdit));
                       lineEdit->setText(QStringLiteral("42"));
                       lineEdit->setFixedWidth(220);

                       auto* status = makeStatusLabel(surface, QStringLiteral("Acceptable input"));
                       QObject::connect(lineEdit, &QLineEdit::textChanged, status,
                                        [lineEdit, status] {
                                            status->setText(lineEdit->hasAcceptableInput()
                                                ? QStringLiteral("Acceptable input")
                                                : QStringLiteral("Out of range"));
                                        });

                       layout->addWidget(lineEdit);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("line-edit-frame-metrics"),
                   QStringLiteral("Frame and content metrics"),
                   QStringLiteral("Margins, border widths, and frame visibility can be tuned independently."),
                   QStringLiteral("auto* emphasizedEdit = new LineEdit(this);\n"
                                  "emphasizedEdit->setText(\"Border thickness demo\");\n"
                                  "emphasizedEdit->setContentMargins(QMargins(10, 4, 10, 4));\n"
                                  "emphasizedEdit->setFocusedBorderWidth(3);\n"
                                  "emphasizedEdit->setUnfocusedBorderWidth(2);\n"
                                  "\n"
                                  "auto* inlineEdit = new LineEdit(this);\n"
                                  "inlineEdit->setText(\"Frameless inline field\");\n"
                                  "inlineEdit->setFrameVisible(false);\n"
                                  "inlineEdit->setClearButtonOffset(QPoint(12, 0));"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent, 10);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* emphasizedEdit = new LineEdit(surface);
                       emphasizedEdit->setText(QStringLiteral("Border thickness demo"));
                       emphasizedEdit->setContentMargins(QMargins(10, 4, 10, 4));
                       emphasizedEdit->setFocusedBorderWidth(3);
                       emphasizedEdit->setUnfocusedBorderWidth(2);
                       emphasizedEdit->setFixedWidth(300);

                       auto* inlineEdit = new LineEdit(surface);
                       inlineEdit->setText(QStringLiteral("Frameless inline field"));
                       inlineEdit->setFrameVisible(false);
                       inlineEdit->setClearButtonOffset(QPoint(12, 0));
                       inlineEdit->setFixedWidth(300);

                       addLabeledWidget(surface, layout, QStringLiteral("Emphasized"), emphasizedEdit);
                       addLabeledWidget(surface, layout, QStringLiteral("Frameless"), inlineEdit);
                       return surface;
                   })
    };
}

QVector<GallerySample> numberBoxSamples()
{
    return {
        makeSample(QStringLiteral("number-box-expression"),
                   QStringLiteral("Expression input"),
                   QStringLiteral("Typed arithmetic can be evaluated on commit when expression support is enabled."),
                   QStringLiteral("auto* numberBox = new NumberBox(this);\n"
                                  "numberBox->setHeader(\"Equation\");\n"
                                  "numberBox->setPlaceholderText(\"1 + 2^2\");\n"
                                  "numberBox->setAcceptsExpression(true);\n"
                                  "\n"
                                  "connect(numberBox, &NumberBox::valueChanged,\n"
                                  "        statusLabel, [statusLabel](double value) {\n"
                                  "            statusLabel->setText(numberValueText(value));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* numberBox = new NumberBox(surface);
                       numberBox->setHeader(QStringLiteral("Equation"));
                       numberBox->setPlaceholderText(QStringLiteral("1 + 2^2"));
                       numberBox->setAcceptsExpression(true);
                       numberBox->setFixedWidth(260);

                       auto* status = makeStatusLabel(surface, QStringLiteral("Value: NaN"));
                       QObject::connect(numberBox, &NumberBox::valueChanged,
                                        status, [status](double value) {
                                            status->setText(numberValueText(value));
                                        });

                       layout->addWidget(numberBox);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("number-box-spin-placement"),
                   QStringLiteral("Spin button placement"),
                   QStringLiteral("Inline and compact placement expose the same value model with different chrome density."),
                   QStringLiteral("auto* inlineBox = new NumberBox(this);\n"
                                  "inlineBox->setHeader(\"Inline\");\n"
                                  "inlineBox->setRange(0, 100);\n"
                                  "inlineBox->setSmallChange(5);\n"
                                  "inlineBox->setLargeChange(25);\n"
                                  "inlineBox->setValue(50);\n"
                                  "inlineBox->setSpinButtonPlacementMode(\n"
                                  "    NumberBox::SpinButtonPlacementMode::Inline);\n"
                                  "\n"
                                  "auto* compactBox = new NumberBox(this);\n"
                                  "compactBox->setHeader(\"Compact\");\n"
                                  "compactBox->setRange(0, 100);\n"
                                  "compactBox->setSmallChange(5);\n"
                                  "compactBox->setLargeChange(25);\n"
                                  "compactBox->setValue(50);\n"
                                  "compactBox->setSpinButtonPlacementMode(\n"
                                  "    NumberBox::SpinButtonPlacementMode::Compact);"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent, 10);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto createBox = [surface](const QString& header,
                                                  NumberBox::SpinButtonPlacementMode mode) {
                           auto* box = new NumberBox(surface);
                           box->setHeader(header);
                           box->setRange(0, 100);
                           box->setSmallChange(5);
                           box->setLargeChange(25);
                           box->setValue(50);
                           box->setSpinButtonPlacementMode(mode);
                           box->setFixedWidth(260);
                           return box;
                       };

                       addLabeledWidget(surface, layout, QStringLiteral("Inline"),
                                        createBox(QStringLiteral("Inline"), NumberBox::SpinButtonPlacementMode::Inline));
                       addLabeledWidget(surface, layout, QStringLiteral("Compact"),
                                        createBox(QStringLiteral("Compact"), NumberBox::SpinButtonPlacementMode::Compact));
                       return surface;
                   }),
        makeSample(QStringLiteral("number-box-formatting"),
                   QStringLiteral("Precision and format step"),
                   QStringLiteral("Formatting can round committed values to a fixed step and display precision."),
                   QStringLiteral("auto* numberBox = new NumberBox(this);\n"
                                  "numberBox->setHeader(\"Amount\");\n"
                                  "numberBox->setDisplayPrecision(2);\n"
                                  "numberBox->setFormatStep(0.25);\n"
                                  "numberBox->setValue(1.13);\n"
                                  "\n"
                                  "connect(numberBox, &NumberBox::valueChanged,\n"
                                  "        statusLabel, [statusLabel](double value) {\n"
                                  "            statusLabel->setText(QStringLiteral(\"Rounded: %1\")\n"
                                  "                .arg(value, 0, 'f', 2));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* numberBox = new NumberBox(surface);
                       numberBox->setHeader(QStringLiteral("Amount"));
                       numberBox->setDisplayPrecision(2);
                       numberBox->setFormatStep(0.25);
                       numberBox->setValue(1.13);
                       numberBox->setFixedWidth(240);

                       auto* status = makeStatusLabel(
                           surface, QStringLiteral("Rounded: %1").arg(numberBox->value(), 0, 'f', 2));
                       QObject::connect(numberBox, &NumberBox::valueChanged,
                                        status, [status](double value) {
                                            status->setText(QStringLiteral("Rounded: %1").arg(value, 0, 'f', 2));
                                        });

                       layout->addWidget(numberBox);
                       layout->addWidget(status);
                       return surface;
                   })
    };
}

QVector<GallerySample> passwordBoxSamples()
{
    return {
        makeSample(QStringLiteral("password-box-basic"),
                   QStringLiteral("Password value"),
                   QStringLiteral("Password text is stored separately from presentation and emits passwordChanged."),
                   QStringLiteral("auto* passwordBox = new PasswordBox(this);\n"
                                  "passwordBox->setPassword(\"Fluent123\");\n"
                                  "\n"
                                  "connect(passwordBox, &PasswordBox::passwordChanged,\n"
                                  "        statusLabel, [statusLabel](const QString& password) {\n"
                                  "            statusLabel->setText(QStringLiteral(\"Length: %1\").arg(password.size()));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* passwordBox = new PasswordBox(surface);
                       passwordBox->setPassword(QStringLiteral("Fluent123"));
                       passwordBox->setFixedWidth(300);

                       auto* status = makeStatusLabel(surface, QStringLiteral("Length: 9"));
                       QObject::connect(passwordBox, &PasswordBox::passwordChanged,
                                        status, [status](const QString& password) {
                                            status->setText(QStringLiteral("Length: %1").arg(password.size()));
                                        });

                       layout->addWidget(passwordBox);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("password-box-header-placeholder"),
                   QStringLiteral("Header and placeholder"),
                   QStringLiteral("A header labels the credential field while placeholder text explains the expected input."),
                   QStringLiteral("auto* passwordBox = new PasswordBox(this);\n"
                                  "passwordBox->setHeader(\"Password\");\n"
                                  "passwordBox->setPlaceholderText(\"Enter your password\");"),
                   [](QWidget* parent) {
                       auto* passwordBox = new PasswordBox(parent);
                       passwordBox->setHeader(QStringLiteral("Password"));
                       passwordBox->setPlaceholderText(QStringLiteral("Enter your password"));
                       passwordBox->setFixedWidth(300);
                       return passwordBox;
                   }),
        makeSample(QStringLiteral("password-box-reveal-modes"),
                   QStringLiteral("Reveal modes"),
                   QStringLiteral("Peek, hidden, and visible modes control whether the reveal button is available."),
                   QStringLiteral("auto* peekBox = new PasswordBox(this);\n"
                                  "peekBox->setPassword(\"Peek mode\");\n"
                                  "peekBox->setPasswordRevealMode(PasswordBox::PasswordRevealMode::Peek);\n"
                                  "\n"
                                  "auto* hiddenBox = new PasswordBox(this);\n"
                                  "hiddenBox->setPassword(\"Hidden mode\");\n"
                                  "hiddenBox->setPasswordRevealMode(PasswordBox::PasswordRevealMode::Hidden);\n"
                                  "\n"
                                  "auto* visibleBox = new PasswordBox(this);\n"
                                  "visibleBox->setPassword(\"Visible mode\");\n"
                                  "visibleBox->setPasswordRevealMode(PasswordBox::PasswordRevealMode::Visible);"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent, 10);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto createBox = [surface](const QString& password,
                                                  PasswordBox::PasswordRevealMode mode) {
                           auto* box = new PasswordBox(surface);
                           box->setPassword(password);
                           box->setPasswordRevealMode(mode);
                           box->setFixedWidth(280);
                           return box;
                       };

                       addLabeledWidget(surface, layout, QStringLiteral("Peek"),
                                        createBox(QStringLiteral("Peek mode"), PasswordBox::PasswordRevealMode::Peek));
                       addLabeledWidget(surface, layout, QStringLiteral("Hidden"),
                                        createBox(QStringLiteral("Hidden mode"), PasswordBox::PasswordRevealMode::Hidden));
                       addLabeledWidget(surface, layout, QStringLiteral("Visible"),
                                        createBox(QStringLiteral("Visible mode"), PasswordBox::PasswordRevealMode::Visible));
                       return surface;
                   })
    };
}

QVector<GallerySample> textEditSamples()
{
    return {
        makeSample(QStringLiteral("text-edit-visible-lines"),
                   QStringLiteral("Auto height from visible lines"),
                   QStringLiteral("TextEdit grows between min and max visible lines before scrolling is needed."),
                   QStringLiteral("auto* textEdit = new TextEdit(this);\n"
                                  "textEdit->setPlaceholderText(\"Type your notes here\");\n"
                                  "textEdit->setMinVisibleLines(2);\n"
                                  "textEdit->setMaxVisibleLines(4);\n"
                                  "textEdit->setPlainText(\"First line\\nSecond line\");\n"
                                  "\n"
                                  "connect(textEdit, &TextEdit::textChanged,\n"
                                  "        statusLabel, [textEdit, statusLabel] {\n"
                                  "            statusLabel->setText(lineCountText(textEdit));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* surface = textFieldSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* textEdit = new TextEdit(surface);
                       textEdit->setPlaceholderText(QStringLiteral("Type your notes here"));
                       textEdit->setMinVisibleLines(2);
                       textEdit->setMaxVisibleLines(4);
                       textEdit->setPlainText(QStringLiteral("First line\nSecond line"));
                       textEdit->setFixedWidth(360);

                       auto* status = makeStatusLabel(surface, lineCountText(textEdit));
                       QObject::connect(textEdit, &TextEdit::textChanged,
                                        status, [textEdit, status] {
                                            status->setText(lineCountText(textEdit));
                                        });

                       layout->addWidget(textEdit);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("text-edit-scrollable-content"),
                   QStringLiteral("Scrollable multi-line content"),
                   QStringLiteral("When text exceeds the maximum visible line count, the Fluent vertical scrollbar takes over."),
                   QStringLiteral("auto* textEdit = new TextEdit(this);\n"
                                  "textEdit->setMinVisibleLines(3);\n"
                                  "textEdit->setMaxVisibleLines(3);\n"
                                  "textEdit->setLineHeight(28);\n"
                                  "textEdit->setPlainText(\"Alpha\\nBeta\\nGamma\\nDelta\\nEpsilon\\nZeta\");"),
                   [](QWidget* parent) {
                       auto* textEdit = new TextEdit(parent);
                       textEdit->setMinVisibleLines(3);
                       textEdit->setMaxVisibleLines(3);
                       textEdit->setLineHeight(28);
                       textEdit->setPlainText(QStringLiteral("Alpha\nBeta\nGamma\nDelta\nEpsilon\nZeta"));
                       textEdit->setFixedWidth(360);
                       return textEdit;
                   }),
        makeSample(QStringLiteral("text-edit-read-only"),
                   QStringLiteral("Read-only text surface"),
                   QStringLiteral("Read-only text keeps the Fluent frame and typography while blocking edits."),
                   QStringLiteral("auto* textEdit = new TextEdit(this);\n"
                                  "textEdit->setPlainText(\"Terms reviewed and locked for approval.\");\n"
                                  "textEdit->setReadOnly(true);\n"
                                  "textEdit->setFontRole(Typography::FontRole::BodyStrong);\n"
                                  "textEdit->setContentMargins(QMargins(12, 6, 12, 6));"),
                   [](QWidget* parent) {
                       auto* textEdit = new TextEdit(parent);
                       textEdit->setPlainText(QStringLiteral("Terms reviewed and locked for approval."));
                       textEdit->setReadOnly(true);
                       textEdit->setFontRole(Typography::FontRole::BodyStrong);
                       textEdit->setContentMargins(QMargins(12, 6, 12, 6));
                       textEdit->setFixedWidth(360);
                       return textEdit;
                   })
    };
}

} // namespace

QVector<GallerySample> textFieldsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("auto-suggest-box"))
        return autoSuggestBoxSamples();
    if (routeId == QStringLiteral("label"))
        return labelSamples();
    if (routeId == QStringLiteral("line-edit"))
        return lineEditSamples();
    if (routeId == QStringLiteral("number-box"))
        return numberBoxSamples();
    if (routeId == QStringLiteral("password-box"))
        return passwordBoxSamples();
    if (routeId == QStringLiteral("text-edit"))
        return textEditSamples();
    return {};
}

} // namespace fluent::gallery

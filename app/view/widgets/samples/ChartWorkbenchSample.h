#ifndef FLUENTQT_CHARTWORKBENCHSAMPLE_H
#define FLUENTQT_CHARTWORKBENCHSAMPLE_H

#include <FluentQt/FluentQt.h>
#include <QBoxLayout>
#include <QFileDialog>
#include <QGridLayout>
#include <QJsonObject>
#include <QResizeEvent>

namespace fluent::gallery {

class ChartWorkbenchSample final : public layout::Card {
public:
    explicit ChartWorkbenchSample(QWidget* parent = nullptr) : Card(parent)
    {
        setObjectName(QStringLiteral("chartWorkbench"));
        setBorderVisible(false);
        setMaximumWidth(1080);
        auto* body = new QVBoxLayout(this);
        body->setContentsMargins(16, 16, 16, 16);
        body->setSpacing(16);
        m_columns = new QBoxLayout(QBoxLayout::LeftToRight);
        m_columns->setSpacing(20);
        m_chart = new charts::ChartView(this);
        m_chart->setObjectName(QStringLiteral("workbenchChart"));
        m_chart->setAccessibleName(QStringLiteral("Response time preview"));
        m_chart->setMinimumSize(260, 320);
        m_chart->setSubtitle(QStringLiteral("Milliseconds · last 7 days"));
        m_chart->setValueSuffix(QStringLiteral(" ms"));
        auto* model = new charts::ChartModel(this);
        model->setName(QStringLiteral("Primary"));
        model->setPoints({{0, 18}, {1, 32}, {2, 24}, {3, 48}, {4, 36}, {5, 54}, {6, 42}},
            {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"});
        m_chart->setModel(model);
        m_columns->addWidget(m_chart, 1);
        m_editorPane = new QWidget(this);
        auto* inspector = new QVBoxLayout(m_editorPane);
        inspector->setContentsMargins(0, 0, 0, 0);
        inspector->setSpacing(16);
        auto* propertyHeader = new QHBoxLayout;
        auto* heading = new textfields::Label(QStringLiteral("Properties"), m_editorPane);
        heading->setFluentTypography(Typography::FontRole::BodyStrong);
        propertyHeader->addWidget(heading);
        propertyHeader->addStretch();
        auto* reset = new basicinput::Button(QStringLiteral("Reset all"), m_editorPane);
        reset->setObjectName(QStringLiteral("propertyResetAll"));
        reset->setFluentStyle(basicinput::Button::Subtle);
        propertyHeader->addWidget(reset);
        inspector->addLayout(propertyHeader);
        m_properties = new layout::PropertyPanel(m_editorPane);
        m_properties->setObjectName(QStringLiteral("workbenchProperties"));
        m_properties->setMinimumHeight(240);
        inspector->addWidget(m_properties, 1);
        m_columns->addWidget(m_editorPane);
        body->addLayout(m_columns, 1);

        layout::PropertyDefinition title;
        title.key = "title";
        title.label = "Title";
        title.group = "Presentation";
        title.defaultValue = QStringLiteral("Response time");
        layout::PropertyDefinition type;
        type.key = "type";
        type.label = "Chart type";
        type.group = "Presentation";
        type.type = layout::PropertyDefinition::Choice;
        type.choices = {"Line", "Area", "Bar", "Scatter"};
        type.defaultValue = QStringLiteral("Area");
        layout::PropertyDefinition legend;
        legend.key = "legend";
        legend.label = "Show legend";
        legend.group = "Presentation";
        legend.type = layout::PropertyDefinition::Boolean;
        legend.defaultValue = true;
        layout::PropertyDefinition maximum;
        maximum.key = "maximum";
        maximum.label = "Y-axis maximum";
        maximum.group = "Axes";
        maximum.type = layout::PropertyDefinition::Number;
        maximum.minimum = 10;
        maximum.maximum = 200;
        maximum.step = 5;
        maximum.defaultValue = 80.0;
        layout::PropertyDefinition accent;
        accent.key = "accent";
        accent.label = "Series accent";
        accent.group = "Presentation";
        accent.type = layout::PropertyDefinition::Color;
        accent.defaultValue = QColor("#0078d4");
        m_properties->setProperties({title, type, legend, accent, maximum});
        reset->setEnabled(false);
        connect(reset, &QAbstractButton::clicked, m_properties, &layout::PropertyPanel::resetAll);
        connect(m_properties, &layout::PropertyPanel::modifiedPropertiesChanged, reset, [this, reset] {
            reset->setEnabled(!m_properties->modifiedPropertyKeys().isEmpty());
        });

        auto* footer = new QHBoxLayout;
        m_scope = new basicinput::ComboBox(this);
        m_scope->setObjectName(QStringLiteral("chartExportScope"));
        m_scope->setAccessibleName(QStringLiteral("Export range"));
        m_scope->addItems({"All data", "Tue–Fri"});
        footer->addWidget(m_scope);
        m_status = new textfields::Label(QStringLiteral("7 original rows · all data"), this);
        m_status->setObjectName(QStringLiteral("chartWorkbenchStatus"));
        m_status->setWordWrap(true);
        m_status->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        m_status->setFluentTypography(Typography::FontRole::Caption);
        footer->addWidget(m_status, 1);
        auto* exportButton = new basicinput::DropDownButton(QStringLiteral("Export"), this);
        exportButton->setObjectName(QStringLiteral("chartExportMenuButton"));
        auto* exportMenu = new menus_toolbars::FluentMenu(QStringLiteral("Export"), this);
        exportButton->setMenu(exportMenu);
        auto* png = exportMenu->addAction(QStringLiteral("Save image as PNG…"));
        png->setObjectName(QStringLiteral("chartSavePng"));
        auto* csv = exportMenu->addAction(QStringLiteral("Save data as CSV…"));
        csv->setObjectName(QStringLiteral("chartSaveCsv"));
        exportMenu->addSeparator();
        auto* copyImage = exportMenu->addAction(QStringLiteral("Copy image"));
        copyImage->setObjectName(QStringLiteral("chartCopyImage"));
        auto* copyData = exportMenu->addAction(QStringLiteral("Copy data"));
        copyData->setObjectName(QStringLiteral("chartCopyData"));
        footer->addWidget(exportButton);
        body->addLayout(footer);
        connect(m_properties, &layout::PropertyPanel::valueEdited, this, [this](const QString& key, const QVariant&) {
            applyProperties();
            m_properties->formController()->validateField(key);
        });
        auto* form = m_properties->formController();
        connect(form, &FormController::validationRequested, this, [this, form](const QString& key, quint64 token) {
            const bool valid = key != "title" || !m_properties->value(key).toString().trimmed().isEmpty();
            form->completeValidation(key, token, valid, valid ? QString() : QStringLiteral("Enter a title."));
        });
        connect(m_scope, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (index == 0)
                m_chart->resetXRange();
            else
                m_chart->setXRange(1, 4);
            m_status->setText(index == 0
                    ? QStringLiteral("7 original rows · all data")
                    : QStringLiteral("4 original rows · Tue–Fri"));
        });
        connect(png, &QAction::triggered, this, [this] {
            const QByteArray bytes = m_chart->exportPng(2);
            if (bytes.isEmpty()) {
                notify(QStringLiteral("Image export failed. Try again."), status_info::Toast::Error);
                return;
            }
            QFileDialog::saveFileContent(bytes, QStringLiteral("response-time.png"));
            notify(QStringLiteral("PNG ready to save"), status_info::Toast::Informational);
        });
        connect(csv, &QAction::triggered, this, [this] {
            const QByteArray bytes = m_chart->exportCsv(m_scope->currentIndex() == 1);
            QFileDialog::saveFileContent(bytes, QStringLiteral("response-time.csv"));
            notify(QStringLiteral("CSV ready to save"), status_info::Toast::Informational);
        });
        connect(copyImage, &QAction::triggered, this, [this] { m_chart->copyImage(2); });
        connect(copyData, &QAction::triggered, this, [this] { m_chart->copyData(m_scope->currentIndex() == 1); });
        connect(m_chart, &charts::ChartView::imageCopied, this, [this](bool success) {
            notify(success ? QStringLiteral("Image copied") : QStringLiteral("Copy unavailable · use Save PNG"),
                success ? status_info::Toast::Success : status_info::Toast::Warning);
        });
        connect(m_chart, &charts::ChartView::dataCopied, this, [this](bool success) {
            notify(success ? QStringLiteral("Data copied") : QStringLiteral("Copy unavailable · use Save CSV"),
                success ? status_info::Toast::Success : status_info::Toast::Warning);
        });
        applyProperties();
        m_properties->formController()->markClean();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        Card::resizeEvent(event);
        const bool narrow = width() < 760;
        m_columns->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_editorPane->setMaximumWidth(narrow ? QWIDGETSIZE_MAX : 380);
        m_editorPane->setMinimumWidth(narrow ? 0 : 360);
        setMinimumHeight(narrow ? 720 : 440);
    }

private:
    void notify(const QString& message, status_info::Toast::Severity severity)
    {
        const bool problem = severity == status_info::Toast::Warning || severity == status_info::Toast::Error;
        status_info::Toast::showOrUpdateToast(this, QStringLiteral("gallery.action-feedback"), message, severity,
            problem ? 4500 : 2200, status_info::Toast::Top, QMargins(16, 50, 16, 16));
    }

    void applyProperties()
    {
        m_chart->setTitle(m_properties->value("title").toString());
        const QString type = m_properties->value("type").toString();
        m_chart->setChartType(type == "Line" ? charts::ChartView::Line
                : type == "Bar"              ? charts::ChartView::Bar
                : type == "Scatter"
                ? charts::ChartView::Scatter
                : charts::ChartView::Area);
        m_chart->setYRange(0, m_properties->value("maximum").toDouble());
        m_chart->setLegendVisible(m_properties->value("legend").toBool());
        const QJsonObject colors{{"accentDefault", m_properties->value("accent").value<QColor>().name()}};
        m_chart->setThemeOverrides(QJsonObject{{"light", colors}, {"dark", colors}});
    }
    charts::ChartView* m_chart;
    layout::PropertyPanel* m_properties;
    basicinput::ComboBox* m_scope;
    textfields::Label* m_status;
    QBoxLayout* m_columns;
    QWidget* m_editorPane;
};
} // namespace fluent::gallery
#endif

"""Property editors and the shared chart configuration workflow."""

from textwrap import dedent

from .native_samples import register_source_samples


def workbench_source_files(result):
    from .native_source_files import complete_sample_source_files
    return complete_sample_source_files(result, "chart_workbench.py")


register_source_samples(
    "property-panel",
    ("PropertyPanel", "ChartView", "NumberBox"),
    {
        "property-panel-basic": (
            "document",
            dedent('''
                import fluentqt
                from PySide6.QtCore import Qt
                from PySide6.QtWidgets import QHBoxLayout, QSizePolicy, QVBoxLayout

                document = fluentqt.Card()
                document.setObjectName("propertyDocumentSample")
                document.setMaximumWidth(480)
                document.setBorderVisible(False)
                body = QVBoxLayout(document)
                body.setContentsMargins(16, 16, 16, 16)
                body.setSpacing(12)
                identity = QHBoxLayout()
                identity.setSpacing(8)
                icon = fluentqt.FontIcon(fluentqt.Typography.Icons.Document, document)
                icon.setIconSize(20)
                identity.addWidget(icon)
                preview = fluentqt.Label("Quarterly report", document)
                preview.setObjectName("propertyDocumentTitle")
                preview.setFluentTypography(fluentqt.FontRole.Body)
                preview.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                preview.setTextFormat(Qt.PlainText)
                preview.setTextElideMode(Qt.ElideRight)
                preview.setMinimumWidth(0)
                preview.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Preferred)
                identity.addWidget(preview, 1)
                panel = fluentqt.PropertyPanel(document)
                panel.setObjectName("documentProperties")
                panel.setLayoutMode(fluentqt.PropertyPanel.LayoutMode.Stacked)
                title = fluentqt.PropertyDefinition()
                title.key = "title"
                title.label = "Title"
                title.defaultValue = "Quarterly report"
                panel.setProperties([title])
                panel.setFixedHeight(64)
                body.addWidget(panel)
                body.addLayout(identity)

                def update_title(key, value):
                    preview.setText(value.strip() or "Untitled document")

                panel.valueEdited.connect(update_title)
            ''').strip() + "\n",
        ),
        "property-panel-chart-workbench": (
            "panel",
            dedent('''
                import fluentqt
                from PySide6.QtCore import QMargins, QPointF
                from PySide6.QtGui import QColor
                from PySide6.QtWidgets import (
                    QBoxLayout, QFileDialog, QHBoxLayout, QVBoxLayout, QWidget,
                )


                class ChartWorkbench(fluentqt.Card):
                    def __init__(self, parent=None):
                        super().__init__(parent)
                        self.setObjectName("chartWorkbench")
                        self.setBorderVisible(False)
                        self.setMaximumWidth(1080)
                        body = QVBoxLayout(self)
                        body.setContentsMargins(16, 16, 16, 16)
                        body.setSpacing(16)
                        self.columns = QBoxLayout(QBoxLayout.LeftToRight)
                        self.columns.setSpacing(20)
                        self.chart = fluentqt.ChartView(self)
                        self.chart.setObjectName("workbenchChart")
                        self.chart.setAccessibleName("Response time preview")
                        self.chart.setMinimumSize(260, 320)
                        self.chart.setSubtitle("Milliseconds · last 7 days")
                        self.chart.setValueSuffix(" ms")
                        model = fluentqt.ChartModel(self)
                        model.setName("Primary")
                        model.setPoints(
                            [QPointF(0, 18), QPointF(1, 32), QPointF(2, 24),
                             QPointF(3, 48), QPointF(4, 36), QPointF(5, 54), QPointF(6, 42)],
                            ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"],
                        )
                        self.chart.setModel(model)
                        self.columns.addWidget(self.chart, 1)
                        self.editor_pane = QWidget(self)
                        inspector = QVBoxLayout(self.editor_pane)
                        inspector.setContentsMargins(0, 0, 0, 0)
                        inspector.setSpacing(16)
                        property_header = QHBoxLayout()
                        heading = fluentqt.Label("Properties", self.editor_pane)
                        heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
                        property_header.addWidget(heading)
                        property_header.addStretch()
                        reset = fluentqt.Button("Reset all", self.editor_pane)
                        reset.setObjectName("propertyResetAll")
                        reset.setFluentStyle(fluentqt.Button.Subtle)
                        property_header.addWidget(reset)
                        inspector.addLayout(property_header)
                        self.properties = fluentqt.PropertyPanel(self.editor_pane)
                        self.properties.setObjectName("workbenchProperties")
                        self.properties.setMinimumHeight(240)
                        inspector.addWidget(self.properties, 1)
                        self.columns.addWidget(self.editor_pane)
                        body.addLayout(self.columns, 1)
                        title = fluentqt.PropertyDefinition()
                        title.key = "title"
                        title.label = "Title"
                        title.group = "Presentation"
                        title.defaultValue = "Response time"
                        kind = fluentqt.PropertyDefinition()
                        kind.key = "type"
                        kind.label = "Chart type"
                        kind.group = "Presentation"
                        kind.type = fluentqt.PropertyDefinition.Type.Choice
                        kind.choices = ["Line", "Area", "Bar", "Scatter"]
                        kind.defaultValue = "Area"
                        legend = fluentqt.PropertyDefinition()
                        legend.key = "legend"
                        legend.label = "Show legend"
                        legend.group = "Presentation"
                        legend.type = fluentqt.PropertyDefinition.Type.Boolean
                        legend.defaultValue = True
                        maximum = fluentqt.PropertyDefinition()
                        maximum.key = "maximum"
                        maximum.label = "Y-axis maximum"
                        maximum.group = "Axes"
                        maximum.type = fluentqt.PropertyDefinition.Type.Number
                        maximum.minimum = 10
                        maximum.maximum = 200
                        maximum.step = 5
                        maximum.defaultValue = 80.0
                        accent = fluentqt.PropertyDefinition()
                        accent.key = "accent"
                        accent.label = "Series accent"
                        accent.group = "Presentation"
                        accent.type = fluentqt.PropertyDefinition.Type.Color
                        accent.defaultValue = QColor("#0078d4")
                        self.properties.setProperties([title, kind, legend, accent, maximum])
                        reset.setEnabled(False)
                        reset.clicked.connect(self.properties.resetAll)
                        self.properties.modifiedPropertiesChanged.connect(
                            lambda: reset.setEnabled(bool(self.properties.modifiedPropertyKeys()))
                        )
                        footer = QHBoxLayout()
                        self.scope = fluentqt.ComboBox(self)
                        self.scope.setObjectName("chartExportScope")
                        self.scope.setAccessibleName("Export range")
                        self.scope.addItems(["All data", "Tue–Fri"])
                        footer.addWidget(self.scope)
                        self.status = fluentqt.Label("7 original rows · all data", self)
                        self.status.setObjectName("chartWorkbenchStatus")
                        self.status.setWordWrap(True)
                        self.status.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
                        self.status.setFluentTypography(fluentqt.FontRole.Caption)
                        footer.addWidget(self.status, 1)
                        export_button = fluentqt.DropDownButton("Export", self)
                        export_button.setObjectName("chartExportMenuButton")
                        export_menu = fluentqt.FluentMenu("Export", self)
                        export_button.setMenu(export_menu)
                        png = export_menu.addAction("Save image as PNG…")
                        png.setObjectName("chartSavePng")
                        csv = export_menu.addAction("Save data as CSV…")
                        csv.setObjectName("chartSaveCsv")
                        export_menu.addSeparator()
                        copy_image = export_menu.addAction("Copy image")
                        copy_image.setObjectName("chartCopyImage")
                        copy_data = export_menu.addAction("Copy data")
                        copy_data.setObjectName("chartCopyData")
                        footer.addWidget(export_button)
                        body.addLayout(footer)
                        self.properties.valueEdited.connect(self.edited)
                        self.properties.formController().validationRequested.connect(self.validate)
                        self.scope.currentIndexChanged.connect(self.set_scope)
                        png.triggered.connect(self.save_png)
                        csv.triggered.connect(self.save_csv)
                        copy_image.triggered.connect(lambda: self.chart.copyImage(2))
                        copy_data.triggered.connect(
                            lambda: self.chart.copyData(self.scope.currentIndex() == 1)
                        )
                        self.chart.imageCopied.connect(lambda ok: self.notify(
                            "Image copied" if ok else "Copy unavailable · use Save PNG",
                            fluentqt.Toast.Success if ok else fluentqt.Toast.Warning,
                        ))
                        self.chart.dataCopied.connect(lambda ok: self.notify(
                            "Data copied" if ok else "Copy unavailable · use Save CSV",
                            fluentqt.Toast.Success if ok else fluentqt.Toast.Warning,
                        ))
                        self.apply_properties()
                        self.properties.formController().markClean()

                    def edited(self, key, value):
                        self.apply_properties()
                        self.properties.formController().validateField(key)

                    def validate(self, key, token):
                        valid = key != "title" or bool(self.properties.value(key).strip())
                        self.properties.formController().completeValidation(
                            key, token, valid, "" if valid else "Enter a title."
                        )

                    def apply_properties(self):
                        self.chart.setTitle(self.properties.value("title"))
                        kinds = {"Line": fluentqt.ChartView.Line,
                                 "Area": fluentqt.ChartView.Area,
                                 "Bar": fluentqt.ChartView.Bar,
                                 "Scatter": fluentqt.ChartView.Scatter}
                        self.chart.setChartType(kinds[self.properties.value("type")])
                        self.chart.setYRange(0, float(self.properties.value("maximum")))
                        self.chart.setLegendVisible(self.properties.value("legend"))
                        color = self.properties.value("accent").name()
                        fluentqt.set_widget_theme_overrides(
                            self.chart, {"light": {"accentDefault": color},
                                         "dark": {"accentDefault": color}}
                        )

                    def set_scope(self, index):
                        if index == 0:
                            self.chart.resetXRange()
                        else:
                            self.chart.setXRange(1, 4)
                        self.status.setText("7 original rows · all data" if index == 0
                                            else "4 original rows · Tue–Fri")

                    def notify(self, message, severity):
                        problem = severity in (fluentqt.Toast.Warning, fluentqt.Toast.Error)
                        fluentqt.Toast.showOrUpdateToast(
                            self, "gallery.action-feedback", message, severity,
                            4500 if problem else 2200, fluentqt.Toast.Top,
                            QMargins(16, 50, 16, 16),
                        )

                    def save_png(self):
                        data = self.chart.exportPng(2)
                        if data.isEmpty():
                            self.notify("Image export failed. Try again.", fluentqt.Toast.Error)
                            return
                        QFileDialog.saveFileContent(data, "response-time.png")
                        self.notify("PNG ready to save", fluentqt.Toast.Informational)

                    def save_csv(self):
                        data = self.chart.exportCsv(self.scope.currentIndex() == 1)
                        QFileDialog.saveFileContent(data, "response-time.csv")
                        self.notify("CSV ready to save", fluentqt.Toast.Informational)

                    def resizeEvent(self, event):
                        super().resizeEvent(event)
                        if not hasattr(self, "editor_pane"):
                            return
                        narrow = self.width() < 760
                        self.columns.setDirection(
                            QBoxLayout.TopToBottom if narrow else QBoxLayout.LeftToRight
                        )
                        self.editor_pane.setMaximumWidth(16777215 if narrow else 380)
                        self.editor_pane.setMinimumWidth(0 if narrow else 360)
                        self.setMinimumHeight(720 if narrow else 440)


                panel = ChartWorkbench()
            ''').strip() + "\n",
        ),
    },
)

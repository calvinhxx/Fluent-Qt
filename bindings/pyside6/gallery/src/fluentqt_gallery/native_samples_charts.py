"""Native chart samples with caller-owned, shareable models."""

from textwrap import dedent
from .native_samples import register_source_samples


def _script(body: str) -> str:
    return (
        "import fluentqt\n"
        "from PySide6.QtCore import QPointF, Qt\n"
        "from PySide6.QtWidgets import QVBoxLayout, QWidget\n\n"
        + dedent(body).strip() + "\n"
    )


register_source_samples(
    "chart-view",
    ("ChartModel", "ChartView", "AreaChart", "Sparkline", "ComboBox", "Button"),
    {
        "chart-view-basic": (
            "chart",
            _script("""
                chart = fluentqt.ChartView()
                chart.setMinimumHeight(420)
                chart.setTitle("Sample response times")
                primary = fluentqt.ChartModel(chart)
                primary.setName("Primary")
                primary.setPoints([QPointF(x, y) for x, y in enumerate([18, 32, 24, 48, 36, 54, 42])])
                secondary = fluentqt.ChartModel(chart)
                secondary.setName("Secondary")
                secondary.setPoints([QPointF(x, y) for x, y in enumerate([12, 20, 18, 32, 28, 38, 30])])
                chart.setModel(primary)
                chart.addSeries(secondary)
            """),
        ),
        "chart-view-types": (
            "panel",
            _script("""
                panel = QWidget()
                layout = QVBoxLayout(panel)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(12)
                model = fluentqt.ChartModel(panel)
                model.setName("Sample activity")
                model.setPoints([QPointF(x, y) for x, y in enumerate([18, 32, 24, 48, 36, 54, 42])],
                                ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"])
                chart = fluentqt.ChartView(panel)
                chart.setObjectName("chartPresentation")
                chart.setModel(model)
                chart.setMinimumHeight(420)
                chart.setChartType(fluentqt.ChartView.ChartType.Bar)
                presentation = fluentqt.ComboBox(panel)
                presentation.setObjectName("chartType")
                presentation.setAccessibleName("Chart presentation")
                presentation.addItems(["Line", "Area", "Bar", "Horizontal bar"])
                presentation.addItems(["Pie", "Donut", "Scatter", "Sparkline"])
                presentation.setCurrentIndex(2)
                presentation.currentIndexChanged.connect(
                    lambda index: chart.setChartType(fluentqt.ChartView.ChartType(index))
                )
                layout.addWidget(presentation)
                layout.addWidget(chart)
            """),
        ),
        "chart-view-shared-model": (
            "panel",
            _script("""
                panel = QWidget()
                layout = QVBoxLayout(panel)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(12)
                model = fluentqt.ChartModel(panel)
                model.setName("Sample signal")
                model.setPoints([QPointF(x, y) for x, y in enumerate([18, 32, 24, 48, 36, 54, 42])])
                detail = fluentqt.ChartView(panel)
                detail.setChartType(fluentqt.ChartView.ChartType.Area)
                detail.setModel(model)
                detail.setMinimumHeight(420)
                trend = fluentqt.Sparkline(panel)
                trend.setModel(model)
                trend.setFixedSize(220, 52)
                append = fluentqt.Button("Append sample batch", panel)

                def append_batch():
                    x = model.maximumX()
                    model.appendPoints([QPointF(x + 1, 30), QPointF(x + 2, 58), QPointF(x + 3, 40)])

                append.clicked.connect(append_batch)
                layout.addWidget(detail)
                layout.addWidget(trend)
                layout.addWidget(append, 0, Qt.AlignLeft)
            """),
        ),
    },
)

register_source_samples('line-chart', ('LineChart', "ChartModel"), {
    'line-chart-basic': ("chart", _script("""
        chart = fluentqt.LineChart()
        chart.setObjectName("chartPresentation")
        chart.setMinimumHeight(420)
        chart.setTitle('Response time')
        chart.setSubtitle('Milliseconds · last 7 days')
        model = fluentqt.ChartModel(chart)
        model.setName("Primary")
        values = [27, 32, 28, 34, 30, 36, 34, 32, 41, 39, 37, 42, 46, 43, 40,
         46, 42, 47, 52, 48, 54, 59, 56, 61, 57, 60, 58, 64, 59, 66,
         62]
        labels = ['Mon', 'Mon, 04:48', 'Mon, 09:36', 'Mon, 14:24',
         'Mon, 19:12', 'Tue', 'Tue, 04:48', 'Tue, 09:36',
         'Tue, 14:24', 'Tue, 19:12', 'Wed', 'Wed, 04:48',
         'Wed, 09:36', 'Wed, 14:24', 'Wed, 19:12', 'Thu',
         'Thu, 04:48', 'Thu, 09:36', 'Thu, 14:24', 'Thu, 19:12',
         'Fri', 'Fri, 04:48', 'Fri, 09:36', 'Fri, 14:24',
         'Fri, 19:12', 'Sat', 'Sat, 04:48', 'Sat, 09:36',
         'Sat, 14:24', 'Sat, 19:12', 'Sun']
        model.setPoints([QPointF(i, y) for i, y in enumerate(values)], labels)
        chart.setModel(model)
        secondary = fluentqt.ChartModel(chart)
        secondary.setName("Secondary")
        secondary_values = [14, 18, 16, 20, 17, 22, 20, 18, 25, 23, 22, 26, 29, 27, 25,
         28, 26, 31, 34, 30, 35, 41, 38, 42, 39, 43, 40, 44, 41, 46,
         43]
        secondary.setPoints([QPointF(i, y) for i, y in enumerate(secondary_values)], labels)
        chart.addSeries(secondary)
        chart.setValueSuffix(" ms")
        chart.setYRange(0, 80)
    """)),
})

register_source_samples('area-chart', ('AreaChart', "ChartModel"), {
    'area-chart-basic': ("chart", _script("""
        chart = fluentqt.AreaChart()
        chart.setObjectName("chartPresentation")
        chart.setMinimumHeight(420)
        chart.setTitle('Active sessions')
        chart.setSubtitle('Sessions · last 7 days')
        model = fluentqt.ChartModel(chart)
        model.setName("Primary")
        values = [27, 32, 28, 34, 30, 36, 34, 32, 41, 39, 37, 42, 46, 43, 40,
         46, 42, 47, 52, 48, 54, 59, 56, 61, 57, 60, 58, 64, 59, 66,
         62]
        labels = ['Mon', 'Mon, 04:48', 'Mon, 09:36', 'Mon, 14:24',
         'Mon, 19:12', 'Tue', 'Tue, 04:48', 'Tue, 09:36',
         'Tue, 14:24', 'Tue, 19:12', 'Wed', 'Wed, 04:48',
         'Wed, 09:36', 'Wed, 14:24', 'Wed, 19:12', 'Thu',
         'Thu, 04:48', 'Thu, 09:36', 'Thu, 14:24', 'Thu, 19:12',
         'Fri', 'Fri, 04:48', 'Fri, 09:36', 'Fri, 14:24',
         'Fri, 19:12', 'Sat', 'Sat, 04:48', 'Sat, 09:36',
         'Sat, 14:24', 'Sat, 19:12', 'Sun']
        model.setPoints([QPointF(i, y) for i, y in enumerate(values)], labels)
        chart.setModel(model)
        secondary = fluentqt.ChartModel(chart)
        secondary.setName("Secondary")
        secondary_values = [14, 18, 16, 20, 17, 22, 20, 18, 25, 23, 22, 26, 29, 27, 25,
         28, 26, 31, 34, 30, 35, 41, 38, 42, 39, 43, 40, 44, 41, 46,
         43]
        secondary.setPoints([QPointF(i, y) for i, y in enumerate(secondary_values)], labels)
        chart.addSeries(secondary)
    """)),
})

register_source_samples('bar-chart', ('BarChart', "ChartModel"), {
    'bar-chart-basic': ("chart", _script("""
        chart = fluentqt.BarChart()
        chart.setObjectName("chartPresentation")
        chart.setMinimumHeight(420)
        chart.setTitle('Weekly activity')
        chart.setSubtitle('Events · last 7 days')
        model = fluentqt.ChartModel(chart)
        model.setName("Primary")
        values = [32, 44, 38, 58, 46, 66, 54]
        labels = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
        model.setPoints([QPointF(i, y) for i, y in enumerate(values)], labels)
        chart.setModel(model)
        secondary = fluentqt.ChartModel(chart)
        secondary.setName("Secondary")
        secondary.setPoints([QPointF(i, y) for i, y in enumerate([22, 32, 28, 42, 34, 48, 40])])
        chart.addSeries(secondary)
    """)),
})

register_source_samples('horizontal-bar-chart', ('HorizontalBarChart', "ChartModel"), {
    'horizontal-bar-chart-basic': ("chart", _script("""
        chart = fluentqt.HorizontalBarChart()
        chart.setObjectName("chartPresentation")
        chart.setMinimumHeight(420)
        chart.setTitle('Traffic sources')
        chart.setSubtitle('Share of sessions')
        model = fluentqt.ChartModel(chart)
        model.setName("Primary")
        values = [48.2, 26.8, 16.4, 8.6]
        labels = ['Direct', 'Search', 'Referral', 'Campaign']
        model.setPoints([QPointF(i, y) for i, y in enumerate(values)], labels)
        chart.setModel(model)
        chart.setYRange(0, 100)
        chart.setValueSuffix("%")
    """)),
})

register_source_samples('pie-chart', ('PieChart', "ChartModel"), {
    'pie-chart-basic': ("chart", _script("""
        chart = fluentqt.PieChart()
        chart.setObjectName("chartPresentation")
        chart.setMinimumHeight(420)
        chart.setTitle('Traffic sources')
        chart.setSubtitle('Share of sessions')
        model = fluentqt.ChartModel(chart)
        model.setName("Primary")
        values = [48.2, 26.8, 16.4, 8.6]
        labels = ['Direct', 'Search', 'Referral', 'Campaign']
        model.setPoints([QPointF(i, y) for i, y in enumerate(values)], labels)
        chart.setModel(model)
    """)),
})

register_source_samples('donut-chart', ('DonutChart', "ChartModel"), {
    'donut-chart-basic': ("chart", _script("""
        chart = fluentqt.DonutChart()
        chart.setObjectName("chartPresentation")
        chart.setMinimumHeight(420)
        chart.setTitle('Traffic sources')
        chart.setSubtitle('Share of sessions')
        model = fluentqt.ChartModel(chart)
        model.setName("Primary")
        values = [48.2, 26.8, 16.4, 8.6]
        labels = ['Direct', 'Search', 'Referral', 'Campaign']
        model.setPoints([QPointF(i, y) for i, y in enumerate(values)], labels)
        chart.setModel(model)
        chart.setCenterText("28,640")
        chart.setCenterCaption("sessions")
    """)),
})

register_source_samples('scatter-chart', ('ScatterChart', "ChartModel"), {
    'scatter-chart-basic': ("chart", _script("""
        chart = fluentqt.ScatterChart()
        chart.setObjectName("chartPresentation")
        chart.setMinimumHeight(420)
        chart.setXRange(0, 600)
        chart.setYRange(0, 80)
        chart.setTitle('Request distribution')
        chart.setSubtitle('Response time (ms) by payload (KB)')
        model = fluentqt.ChartModel(chart)
        model.setName("Primary")
        values = [22, 31, 26, 42, 37, 50, 45, 58, 52, 62, 56, 68, 60, 70, 64]
        model.setPoints([QPointF(i * 40, y) for i, y in enumerate(values)])
        chart.setModel(model)
        secondary = fluentqt.ChartModel(chart)
        secondary.setName("Secondary")
        secondary_values = [18, 26, 21, 33, 29, 40, 35, 45, 39, 49, 43, 53, 48, 58, 51]
        secondary.setPoints([QPointF(i * 40 + 12, y) for i, y in enumerate(secondary_values)])
        chart.addSeries(secondary)
    """)),
})

register_source_samples('sparkline', ('Sparkline', "ChartModel"), {
    'sparkline-basic': ("chart", _script("""
        chart = fluentqt.Sparkline()
        chart.setObjectName("chartPresentation")
        chart.setFixedSize(220, 56)
        chart.setAccessibleName("Recent trend")
        model = fluentqt.ChartModel(chart)
        model.setName("Primary")
        values = [20, 26, 22, 32, 28, 36, 33, 42, 38, 47, 42, 50]
        model.setPoints([QPointF(i, y) for i, y in enumerate(values)])
        chart.setModel(model)
    """)),
})

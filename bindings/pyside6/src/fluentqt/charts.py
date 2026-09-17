"""Native indexed charts. Models remain caller-owned; views retain Python wrappers."""
from shiboken6 import isValid
from . import _fluentqt as _native

ChartData = _native.fluent.ChartData
ChartModel = _native.fluent.ChartModel


class _BorrowedChartModels:
    def __init__(self, parent=None):
        super().__init__(parent)
        self._chart_models = []

    def setModel(self, model):
        super().setModel(model)
        self._sync_models()

    def addSeries(self, model):
        added = super().addSeries(model)
        self._sync_models()
        return added

    def removeSeries(self, model):
        removed = super().removeSeries(model)
        self._sync_models()
        return removed

    def clearSeries(self):
        super().clearSeries()
        self._sync_models()

    def _sync_models(self):
        if isValid(self):
            self._chart_models = [self.seriesModel(i) for i in range(self.seriesCount())]



class ChartView(_BorrowedChartModels, _native.fluent.ChartView):
    pass

class LineChart(_BorrowedChartModels, _native.fluent.LineChart):
    pass

class AreaChart(_BorrowedChartModels, _native.fluent.AreaChart):
    pass

class BarChart(_BorrowedChartModels, _native.fluent.BarChart):
    pass

class HorizontalBarChart(_BorrowedChartModels, _native.fluent.HorizontalBarChart):
    pass

class PieChart(_BorrowedChartModels, _native.fluent.PieChart):
    pass

class DonutChart(_BorrowedChartModels, _native.fluent.DonutChart):
    pass

class ScatterChart(_BorrowedChartModels, _native.fluent.ScatterChart):
    pass

class Sparkline(_BorrowedChartModels, _native.fluent.Sparkline):
    pass

__all__ = ['ChartData', 'ChartModel', 'ChartView', 'LineChart', 'AreaChart', 'BarChart', 'HorizontalBarChart', 'PieChart', 'DonutChart', 'ScatterChart', 'Sparkline']

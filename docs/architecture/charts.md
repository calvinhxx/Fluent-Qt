# Charts

> **Status:** Current contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Inspector Report Contract](inspector-report.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:top:end -->

Charts uses the existing Qt Core, Gui, and Widgets dependencies. It does not
require Qt Charts, Qt Graphs, a browser, or a third-party plotting engine.
Include `<FluentQt/Charts.h>` or the main `<FluentQt/FluentQt.h>` header.

## Model and view

`fluent::charts::ChartModel` is a `QAbstractTableModel`: column 0 is X,
column 1 is Y, and optional vertical headers contain category labels. One
model represents one series. Applications own models; `ChartView` borrows up
to 32 of them and removes a series when its model is destroyed. Multiple chart
or Qt table views can share the same model. The Python view retains model
wrappers without changing QObject parents.

```cpp
auto* model = new fluent::charts::ChartModel(this);
model->setName("Requests");
model->setPoints({{0, 18}, {1, 32}, {2, 24}});
auto* chart = new fluent::charts::AreaChart(this);
chart->setModel(model);
```

`ChartView` accepts indexed `ChartModel` instances. It does not silently scan
an arbitrary `QAbstractItemModel` to discover bounds on each repaint.
Applications adapting a database or another Qt model should publish a
prepared `ChartData` snapshot.

## Dedicated components

Use `LineChart`, `AreaChart`, `BarChart`, `HorizontalBarChart`, `PieChart`,
`DonutChart`, `ScatterChart`, or `Sparkline` for a fixed presentation. Each has
its own public header and renderer translation unit. Their type stays fixed,
including when addressed through a `ChartView*`; `setChartType()` remains an
option on the generic `ChartView` host.

The common host owns borrowed-model connections, frame scheduling, axes,
legend, data cursor, value readout and accessibility. The private renderer
contract owns projection and drawing; Cartesian sampling and radial sector
geometry are shared helpers. There is one factory dispatch, not a shared paint switch.
Adding a presentation does not add branches to the existing renderers.

The approved [Figma study](https://www.figma.com/design/eReWc8M7FqwF4PwUwRNEdf?node-id=7-15)
owns the visual direction: 24px card padding, Fluent typography, restrained
blue/teal series, quiet grids and shared hover readouts. `subtitle` and
`valueSuffix` describe units without changing the data. `setYRange(0, 100)`
keeps percentage tracks proportional to 100%. Automatic Y bounds use rounded
ticks. A DonutChart can show caller-supplied `centerText` and `centerCaption`;
these are not implicitly computed totals. Radial legends move below the plot
at narrow widths and expose `heightForWidth()` to Qt layouts. Sparkline owns
only the trend, without a metric or card.

`setLoading()` is host-controlled presentation while a snapshot is prepared;
it does not introduce a worker or alter model ownership. Disabled, empty,
hover and keyboard-selection states use the same component.

## Large data and streaming

- X must be finite and nondecreasing. Duplicate X values are allowed. A NaN Y
  marks missing data; infinities and mismatched label counts are rejected.
- `ChartData::fromPoints()` validates data and builds a multilevel min/max,
  count, and normalized-sum index in O(N). Its raw storage and index use O(N)
  memory. Copies share storage.
- Build large snapshots on a worker and deliver them to
  `ChartModel::setDataSnapshot()` on the model's thread, for example through a
  queued invocation guarded by `QPointer`. Model and widget operations stay on
  the GUI thread. `setPoints()` is the synchronous small-data convenience.
  Releasing the final reference to an old snapshot can still free O(N) storage;
  for latency-sensitive replacements, retain and release that snapshot on the
  worker after publication. Single-thread WASM must schedule data preparation
  outside interactive frames; it cannot use the desktop worker pattern.
- `appendPoints()` validates just the incoming batch, emits one insertion
  notification, and updates the affected index tail. Append batches, not one
  point per event. Holding an old snapshot or input vector can force an O(N)
  copy on the next write. Release those references before streaming.
- Line/area/scatter projections binary-search the visible X range and query
  indexed bucket endpoints and extrema. Query work is bounded by the projection
  budget and index depth, rather than a full history scan. One outside point on
  each side keeps crossing segments visible. Automatic Y bounds cover the
  complete series and do not rescale with the X viewport.
- The total representative-point limit defaults to 4096 and is further limited
  by viewport resolution. Cached projections are reused on ordinary repaints.
  Dense strokes draw independently and area fills use adjacent trapezoids,
  avoiding Qt rasterization of a large overlapping path. Dense line strokes use
  flat caps without antialiasing; zoomed detail restores round caps and
  antialiasing.
  Data notifications coalesce at 30 Hz by default (configurable from 1 to 60);
  hidden views stop scheduling work and refresh once when shown. Resize and
  explicit configuration changes rebuild immediately.
- This is an in-memory model, not an unlimited-history store. Applications
  decide retention and publish a bounded window when history grows indefinitely.
  Rendering budgets do not remove input parsing, indexing, or storage costs.

## Presentations and aggregation

| Type | Representation |
|---|---|
| Line / Area / Sparkline | Bucket endpoints and extrema; conservative breaks around missing data |
| Scatter | Representative sampled points; not a density estimate |
| Bar / HorizontalBar | Grouped series; dense source ranges become arithmetic-mean bars |
| Pie / Donut | Positive values from the first model only; up to 12 rows render directly, otherwise the first 11 rows plus an `Other` tail |

Small datasets render individual values. Dense bar readouts identify the source
range and mean; pie tails identify the count and fraction. Pointer activation
emits `rangeActivated(series, first, last)`, a half-open source range. Keyboard
activation addresses one original row. Raw data stays accessible through the
model; aggregation does not mutate it. Missing-data buckets may show isolated
representatives instead of joining across a gap.

Use `setModel()` for Pie and Donut. Additional models passed to `addSeries()`
remain attached for generic `ChartView` presentation changes, but these radial
presentations draw only `seriesModel(0)`.

Log axes, stacked bars, financial series, density plots, editable points, and
GPU rendering are outside this initial module. Category labels appear on bar axes, in
readouts, and in pie legends; numeric axes use the widget locale.

## Interaction and accessibility

Data strokes and legend swatches use the chart semantic roles in
[`ChartTokens_p.h`](../../src/design/ChartTokens_p.h), the configured accent and
contrast adjustment against Fluent neutral surfaces. HighContrast uses the
theme chart palette. `BarChart` uses block legend swatches, `ScatterChart` uses
dots, and line legends preserve the series stroke styles.

Hover hit testing reads cached projections. Each chart lazily reuses one passive
`Popup` with `Label` children for its readout; the popup supplies the shared
surface, shadow, theme, and placement lifecycle. It does not take focus or
intercept pointer events. The number of labels is bounded by the visible series,
not the data points. Readout width follows the text within the chart's available
space. Headings and series names wrap instead of eliding; values use their
measured width and move below the name when two columns do not fit.
Comparison rows are limited by their measured height; any remaining series
are reported as a count, with the active series always shown first.
`LineChart` and `AreaChart` show a shared readout at the
active point's X. The active series keeps
its exact value; a keyboard-selected raw row is read directly when absent from
the projection. Other series appear only when their cached projection contains
an exactly equal X. Nearby samples are omitted. The active series appears first
and uses Fluent `BodyStrong` typography, including keyboard selections. Bar,
radial and scatter readouts describe only the selected series or aggregate;
Sparkline shows cursor markers without a readout panel.

The outer focus ring appears for keyboard navigation and hides on mouse clicks.
Mouse selection retains the data cursor without outlining the card. Readouts
close when the pointer moves away from data, leaves the chart, or focus leaves
the chart. Hiding, disabling, deactivating, or invalidating the chart also
dismisses the readout without clearing the selected raw point. Keyboard
navigation and explicit `setCurrentPoint()` calls show the current value again;
keyboard navigation takes precedence over a stationary pointer's hover.
Left/Right and Home/End navigate original rows; Up/Down select a series;
Enter/Space activate a row. For Cartesian presentations, Ctrl+wheel zooms the X
range. Escape consumes the key only when it restores an explicit X range to
automatic bounds. With automatic bounds, or for Pie and Donut, Escape follows
Qt key propagation so the parent dialog can apply its own close policy. After
resetting a zoomed chart, a second Escape can close the parent dialog.
Applications can set an exact viewport through `setXRange()`. Pie and Donut
always use the complete first model and ignore X-axis zoom.

The root accessible role is Chart, with the application-supplied name and
description taking precedence. The current raw value is exposed as accessible
Value, with next/previous actions and value-change events. No widget or
accessible-object tree is allocated for every point. Applications needing a
tabular alternative can attach a Qt table view to the same model.

## Verification

`test_chart_model` covers validation, batch signals, snapshot independence,
index updates, extreme values, gaps, and million-point extrema preservation.
`test_chart_view` covers borrowed ownership, bounded model reads, cached
repainting, coalesced streaming, hidden views, all eight presentations, themes,
narrow geometry, and accessible keyboard navigation. `ChartViewTest.VisualCheck`
is a separate native review surface. The Gallery has a route for each dedicated component; `chart-view` demonstrates
runtime presentation changes and model sharing.

Point-count and query-count assertions are portable performance contracts.
For repeatable native measurements, run
[`benchmark_charts.py`](../../tools/dev/benchmark_charts.py) with the matching
PySide6 interpreter and `PYTHONPATH=<binding-build>/python`. It defaults to a
million points and 30 viewport changes per presentation; `--output` saves the
platform fingerprint and raw timings as JSON. Use a Release binding build.

Elapsed timings depend on the machine, Qt build, scale factor, and data shape;
they are diagnostics rather than universal frame-time guarantees.

<!-- docs-nav:bottom:start -->
---
[← Inspector Report Contract](inspector-report.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:bottom:end -->

#include "ChartsSamples.h"
#include "SampleBuilders.h"
#include <FluentQt/FluentQt.h>
#include <QVBoxLayout>

namespace fluent::gallery {
namespace {
using namespace charts;
using samples::makeSample;
QVector<GallerySample> lineChartSamples()
{
    return {makeSample(
        QStringLiteral("line-chart-basic"), QStringLiteral("Response time"),
        QStringLiteral("Illustrative data. The model is independent of this chart component."),
        QStringLiteral(
            "auto* chart = new "
            "LineChart(this);\nchart->setObjectName(\"chartPresentation\");\nchart->"
            "setMinimumHeight(420);\nchart->setTitle(\"Response "
            "time\");\nchart->setSubtitle(\"Milliseconds · last 7 days\");\nauto* model = new "
            "ChartModel(chart);\nmodel->setName(\"Primary\");\nconst QStringList labels = "
            "{\"Mon\",        \"Mon, 04:48\", \"Mon, 09:36\", \"Mon, 14:24\", \"Mon, 19:12\", "
            "\"Tue\",\n     \"Tue, 04:48\", \"Tue, 09:36\", \"Tue, 14:24\", \"Tue, 19:12\", "
            "\"Wed\",        \"Wed, 04:48\",\n     \"Wed, 09:36\", \"Wed, 14:24\", \"Wed, 19:12\", "
            "\"Thu\",        \"Thu, 04:48\", \"Thu, 09:36\",\n     \"Thu, 14:24\", \"Thu, 19:12\", "
            "\"Fri\",        \"Fri, 04:48\", \"Fri, 09:36\", \"Fri, 14:24\",\n     \"Fri, 19:12\", "
            "\"Sat\",        \"Sat, 04:48\", \"Sat, 09:36\", \"Sat, 14:24\", \"Sat, 19:12\",\n     "
            "\"Sun\"};\nmodel->setPoints(\n    {{0, 27},  {1, 32},  {2, 28},  {3, 34},  {4, 30},  "
            "{5, 36},  {6, 34},  {7, 32},\n     {8, 41},  {9, 39},  {10, 37}, {11, 42}, {12, 46}, "
            "{13, 43}, {14, 40}, {15, 46},\n     {16, 42}, {17, 47}, {18, 52}, {19, 48}, {20, 54}, "
            "{21, 59}, {22, 56}, {23, 61},\n     {24, 57}, {25, 60}, {26, 58}, {27, 64}, {28, 59}, "
            "{29, 66}, {30, 62}},\n    labels);\nchart->setModel(model);\nauto* secondary = new "
            "ChartModel(chart);\nsecondary->setName(\"Secondary\");\nsecondary->setPoints(\n    "
            "{{0, 14},  {1, 18},  {2, 16},  {3, 20},  {4, 17},  {5, 22},  {6, 20},  {7, 18},\n     "
            "{8, 25},  {9, 23},  {10, 22}, {11, 26}, {12, 29}, {13, 27}, {14, 25}, {15, 28},\n     "
            "{16, 26}, {17, 31}, {18, 34}, {19, 30}, {20, 35}, {21, 41}, {22, 38}, {23, 42},\n     "
            "{24, 39}, {25, 43}, {26, 40}, {27, 44}, {28, 41}, {29, 46}, {30, 43}}, "
            "labels);\nchart->addSeries(secondary);\nchart->setValueSuffix(\" "
            "ms\");\nchart->setYRange(0, 80);\n"),
        [](QWidget* parent) {
            auto* chart = new LineChart(parent);
            chart->setObjectName("chartPresentation");
            chart->setMinimumHeight(420);
            chart->setTitle("Response time");
            chart->setSubtitle("Milliseconds · last 7 days");
            auto* model = new ChartModel(chart);
            model->setName("Primary");
            const QStringList labels = {
                "Mon",        "Mon, 04:48", "Mon, 09:36", "Mon, 14:24", "Mon, 19:12", "Tue",
                "Tue, 04:48", "Tue, 09:36", "Tue, 14:24", "Tue, 19:12", "Wed",        "Wed, 04:48",
                "Wed, 09:36", "Wed, 14:24", "Wed, 19:12", "Thu",        "Thu, 04:48", "Thu, 09:36",
                "Thu, 14:24", "Thu, 19:12", "Fri",        "Fri, 04:48", "Fri, 09:36", "Fri, 14:24",
                "Fri, 19:12", "Sat",        "Sat, 04:48", "Sat, 09:36", "Sat, 14:24", "Sat, 19:12",
                "Sun"};
            model->setPoints({{0, 27},  {1, 32},  {2, 28},  {3, 34},  {4, 30},  {5, 36},  {6, 34},
                              {7, 32},  {8, 41},  {9, 39},  {10, 37}, {11, 42}, {12, 46}, {13, 43},
                              {14, 40}, {15, 46}, {16, 42}, {17, 47}, {18, 52}, {19, 48}, {20, 54},
                              {21, 59}, {22, 56}, {23, 61}, {24, 57}, {25, 60}, {26, 58}, {27, 64},
                              {28, 59}, {29, 66}, {30, 62}},
                             labels);
            chart->setModel(model);
            auto* secondary = new ChartModel(chart);
            secondary->setName("Secondary");
            secondary->setPoints(
                {{0, 14},  {1, 18},  {2, 16},  {3, 20},  {4, 17},  {5, 22},  {6, 20},  {7, 18},
                 {8, 25},  {9, 23},  {10, 22}, {11, 26}, {12, 29}, {13, 27}, {14, 25}, {15, 28},
                 {16, 26}, {17, 31}, {18, 34}, {19, 30}, {20, 35}, {21, 41}, {22, 38}, {23, 42},
                 {24, 39}, {25, 43}, {26, 40}, {27, 44}, {28, 41}, {29, 46}, {30, 43}},
                labels);
            chart->addSeries(secondary);
            chart->setValueSuffix(" ms");
            chart->setYRange(0, 80);
            return chart;
        },
        true)};
}

QVector<GallerySample> areaChartSamples()
{
    return {makeSample(
        QStringLiteral("area-chart-basic"), QStringLiteral("Active sessions"),
        QStringLiteral("Illustrative data. The model is independent of this chart component."),
        QStringLiteral(
            "auto* chart = new "
            "AreaChart(this);\nchart->setObjectName(\"chartPresentation\");\nchart->"
            "setMinimumHeight(420);\nchart->setTitle(\"Active "
            "sessions\");\nchart->setSubtitle(\"Sessions · last 7 days\");\nauto* model = new "
            "ChartModel(chart);\nmodel->setName(\"Primary\");\nconst QStringList labels = "
            "{\"Mon\",        \"Mon, 04:48\", \"Mon, 09:36\", \"Mon, 14:24\", \"Mon, 19:12\", "
            "\"Tue\",\n     \"Tue, 04:48\", \"Tue, 09:36\", \"Tue, 14:24\", \"Tue, 19:12\", "
            "\"Wed\",        \"Wed, 04:48\",\n     \"Wed, 09:36\", \"Wed, 14:24\", \"Wed, 19:12\", "
            "\"Thu\",        \"Thu, 04:48\", \"Thu, 09:36\",\n     \"Thu, 14:24\", \"Thu, 19:12\", "
            "\"Fri\",        \"Fri, 04:48\", \"Fri, 09:36\", \"Fri, 14:24\",\n     \"Fri, 19:12\", "
            "\"Sat\",        \"Sat, 04:48\", \"Sat, 09:36\", \"Sat, 14:24\", \"Sat, 19:12\",\n     "
            "\"Sun\"};\nmodel->setPoints(\n    {{0, 27},  {1, 32},  {2, 28},  {3, 34},  {4, 30},  "
            "{5, 36},  {6, 34},  {7, 32},\n     {8, 41},  {9, 39},  {10, 37}, {11, 42}, {12, 46}, "
            "{13, 43}, {14, 40}, {15, 46},\n     {16, 42}, {17, 47}, {18, 52}, {19, 48}, {20, 54}, "
            "{21, 59}, {22, 56}, {23, 61},\n     {24, 57}, {25, 60}, {26, 58}, {27, 64}, {28, 59}, "
            "{29, 66}, {30, 62}},\n    labels);\nchart->setModel(model);\nauto* secondary = new "
            "ChartModel(chart);\nsecondary->setName(\"Secondary\");\nsecondary->setPoints(\n    "
            "{{0, 14},  {1, 18},  {2, 16},  {3, 20},  {4, 17},  {5, 22},  {6, 20},  {7, 18},\n     "
            "{8, 25},  {9, 23},  {10, 22}, {11, 26}, {12, 29}, {13, 27}, {14, 25}, {15, 28},\n     "
            "{16, 26}, {17, 31}, {18, 34}, {19, 30}, {20, 35}, {21, 41}, {22, 38}, {23, 42},\n     "
            "{24, 39}, {25, 43}, {26, 40}, {27, 44}, {28, 41}, {29, 46}, {30, 43}}, "
            "labels);\nchart->addSeries(secondary);\n"),
        [](QWidget* parent) {
            auto* chart = new AreaChart(parent);
            chart->setObjectName("chartPresentation");
            chart->setMinimumHeight(420);
            chart->setTitle("Active sessions");
            chart->setSubtitle("Sessions · last 7 days");
            auto* model = new ChartModel(chart);
            model->setName("Primary");
            const QStringList labels = {
                "Mon",        "Mon, 04:48", "Mon, 09:36", "Mon, 14:24", "Mon, 19:12", "Tue",
                "Tue, 04:48", "Tue, 09:36", "Tue, 14:24", "Tue, 19:12", "Wed",        "Wed, 04:48",
                "Wed, 09:36", "Wed, 14:24", "Wed, 19:12", "Thu",        "Thu, 04:48", "Thu, 09:36",
                "Thu, 14:24", "Thu, 19:12", "Fri",        "Fri, 04:48", "Fri, 09:36", "Fri, 14:24",
                "Fri, 19:12", "Sat",        "Sat, 04:48", "Sat, 09:36", "Sat, 14:24", "Sat, 19:12",
                "Sun"};
            model->setPoints({{0, 27},  {1, 32},  {2, 28},  {3, 34},  {4, 30},  {5, 36},  {6, 34},
                              {7, 32},  {8, 41},  {9, 39},  {10, 37}, {11, 42}, {12, 46}, {13, 43},
                              {14, 40}, {15, 46}, {16, 42}, {17, 47}, {18, 52}, {19, 48}, {20, 54},
                              {21, 59}, {22, 56}, {23, 61}, {24, 57}, {25, 60}, {26, 58}, {27, 64},
                              {28, 59}, {29, 66}, {30, 62}},
                             labels);
            chart->setModel(model);
            auto* secondary = new ChartModel(chart);
            secondary->setName("Secondary");
            secondary->setPoints(
                {{0, 14},  {1, 18},  {2, 16},  {3, 20},  {4, 17},  {5, 22},  {6, 20},  {7, 18},
                 {8, 25},  {9, 23},  {10, 22}, {11, 26}, {12, 29}, {13, 27}, {14, 25}, {15, 28},
                 {16, 26}, {17, 31}, {18, 34}, {19, 30}, {20, 35}, {21, 41}, {22, 38}, {23, 42},
                 {24, 39}, {25, 43}, {26, 40}, {27, 44}, {28, 41}, {29, 46}, {30, 43}},
                labels);
            chart->addSeries(secondary);
            return chart;
        },
        true)};
}

QVector<GallerySample> barChartSamples()
{
    return {makeSample(
        QStringLiteral("bar-chart-basic"), QStringLiteral("Weekly activity"),
        QStringLiteral("Illustrative data. The model is independent of this chart component."),
        QStringLiteral(
            "auto* chart = new "
            "BarChart(this);\nchart->setObjectName(\"chartPresentation\");\nchart->"
            "setMinimumHeight(420);\nchart->setTitle(\"Weekly "
            "activity\");\nchart->setSubtitle(\"Events · last 7 days\");\nauto* model = new "
            "ChartModel(chart);\nmodel->setName(\"Primary\");\nmodel->setPoints({{0, 32}, {1, 44}, "
            "{2, 38}, {3, 58}, {4, 46}, {5, 66}, {6, 54}}, {\"Mon\", \"Tue\", \"Wed\", \"Thu\", "
            "\"Fri\", \"Sat\", \"Sun\"});\nchart->setModel(model);\nauto* secondary = new "
            "ChartModel(chart);\nsecondary->setName(\"Secondary\");\nsecondary->setPoints({{0, "
            "22}, {1, 32}, {2, 28}, {3, 42}, {4, 34}, {5, 48}, {6, "
            "40}});\nchart->addSeries(secondary);\n"),
        [](QWidget* parent) {
            auto* chart = new BarChart(parent);
            chart->setObjectName("chartPresentation");
            chart->setMinimumHeight(420);
            chart->setTitle("Weekly activity");
            chart->setSubtitle("Events · last 7 days");
            auto* model = new ChartModel(chart);
            model->setName("Primary");
            model->setPoints({{0, 32}, {1, 44}, {2, 38}, {3, 58}, {4, 46}, {5, 66}, {6, 54}},
                             {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"});
            chart->setModel(model);
            auto* secondary = new ChartModel(chart);
            secondary->setName("Secondary");
            secondary->setPoints({{0, 22}, {1, 32}, {2, 28}, {3, 42}, {4, 34}, {5, 48}, {6, 40}});
            chart->addSeries(secondary);
            return chart;
        },
        true)};
}

QVector<GallerySample> horizontalBarChartSamples()
{
    return {makeSample(
        QStringLiteral("horizontal-bar-chart-basic"), QStringLiteral("Traffic sources"),
        QStringLiteral("Illustrative data. The model is independent of this chart component."),
        QStringLiteral(
            "auto* chart = new "
            "HorizontalBarChart(this);\nchart->setObjectName(\"chartPresentation\");\nchart->"
            "setMinimumHeight(420);\nchart->setTitle(\"Traffic "
            "sources\");\nchart->setSubtitle(\"Share of sessions\");\nauto* model = new "
            "ChartModel(chart);\nmodel->setName(\"Primary\");\nmodel->setPoints({{0, 48.2}, {1, "
            "26.8}, {2, 16.4}, {3, 8.6}}, {\"Direct\", \"Search\", \"Referral\", "
            "\"Campaign\"});\nchart->setModel(model);\nchart->setYRange(0, "
            "100);\nchart->setValueSuffix(\"%\");\n"),
        [](QWidget* parent) {
            auto* chart = new HorizontalBarChart(parent);
            chart->setObjectName("chartPresentation");
            chart->setMinimumHeight(420);
            chart->setTitle("Traffic sources");
            chart->setSubtitle("Share of sessions");
            auto* model = new ChartModel(chart);
            model->setName("Primary");
            model->setPoints({{0, 48.2}, {1, 26.8}, {2, 16.4}, {3, 8.6}},
                             {"Direct", "Search", "Referral", "Campaign"});
            chart->setModel(model);
            chart->setYRange(0, 100);
            chart->setValueSuffix("%");
            return chart;
        },
        true)};
}

QVector<GallerySample> pieChartSamples()
{
    return {makeSample(
        QStringLiteral("pie-chart-basic"), QStringLiteral("Traffic sources"),
        QStringLiteral("Illustrative data. The model is independent of this chart component."),
        QStringLiteral("auto* chart = new "
                       "PieChart(this);\nchart->setObjectName(\"chartPresentation\");\nchart->"
                       "setMinimumHeight(420);\nchart->setTitle(\"Traffic "
                       "sources\");\nchart->setSubtitle(\"Share of sessions\");\nauto* model = new "
                       "ChartModel(chart);\nmodel->setName(\"Primary\");\nmodel->setPoints({{0, "
                       "48.2}, {1, 26.8}, {2, 16.4}, {3, 8.6}}, {\"Direct\", \"Search\", "
                       "\"Referral\", \"Campaign\"});\nchart->setModel(model);\n"),
        [](QWidget* parent) {
            auto* chart = new PieChart(parent);
            chart->setObjectName("chartPresentation");
            chart->setMinimumHeight(420);
            chart->setTitle("Traffic sources");
            chart->setSubtitle("Share of sessions");
            auto* model = new ChartModel(chart);
            model->setName("Primary");
            model->setPoints({{0, 48.2}, {1, 26.8}, {2, 16.4}, {3, 8.6}},
                             {"Direct", "Search", "Referral", "Campaign"});
            chart->setModel(model);
            return chart;
        },
        true)};
}

QVector<GallerySample> donutChartSamples()
{
    return {makeSample(
        QStringLiteral("donut-chart-basic"), QStringLiteral("Traffic sources"),
        QStringLiteral("Illustrative data. The model is independent of this chart component."),
        QStringLiteral(
            "auto* chart = new "
            "DonutChart(this);\nchart->setObjectName(\"chartPresentation\");\nchart->"
            "setMinimumHeight(420);\nchart->setTitle(\"Traffic "
            "sources\");\nchart->setSubtitle(\"Share of sessions\");\nauto* model = new "
            "ChartModel(chart);\nmodel->setName(\"Primary\");\nmodel->setPoints({{0, 48.2}, {1, "
            "26.8}, {2, 16.4}, {3, 8.6}}, {\"Direct\", \"Search\", \"Referral\", "
            "\"Campaign\"});\nchart->setModel(model);\nchart->setCenterText(\"28,640\");\nchart->"
            "setCenterCaption(\"sessions\");\n"),
        [](QWidget* parent) {
            auto* chart = new DonutChart(parent);
            chart->setObjectName("chartPresentation");
            chart->setMinimumHeight(420);
            chart->setTitle("Traffic sources");
            chart->setSubtitle("Share of sessions");
            auto* model = new ChartModel(chart);
            model->setName("Primary");
            model->setPoints({{0, 48.2}, {1, 26.8}, {2, 16.4}, {3, 8.6}},
                             {"Direct", "Search", "Referral", "Campaign"});
            chart->setModel(model);
            chart->setCenterText("28,640");
            chart->setCenterCaption("sessions");
            return chart;
        },
        true)};
}

QVector<GallerySample> scatterChartSamples()
{
    return {makeSample(
        QStringLiteral("scatter-chart-basic"), QStringLiteral("Request distribution"),
        QStringLiteral("Illustrative data. The model is independent of this chart component."),
        QStringLiteral(
            "auto* chart = new "
            "ScatterChart(this);\nchart->setObjectName(\"chartPresentation\");\nchart->"
            "setMinimumHeight(420);\nchart->setXRange(0, 600);\nchart->setYRange(0, "
            "80);\nchart->setTitle(\"Request distribution\");\nchart->setSubtitle(\"Response time "
            "(ms) by payload (KB)\");\nauto* model = new "
            "ChartModel(chart);\nmodel->setName(\"Primary\");\nmodel->setPoints({{0, 22}, {40, "
            "31}, {80, 26}, {120, 42}, {160, 37}, {200, 50}, {240, 45}, {280, 58}, {320, 52}, "
            "{360, 62}, {400, 56}, {440, 68}, {480, 60}, {520, 70}, {560, "
            "64}});\nchart->setModel(model);\nauto* secondary = new "
            "ChartModel(chart);\nsecondary->setName(\"Secondary\");\nsecondary->setPoints({{12, "
            "18}, {52, 26}, {92, 21}, {132, 33}, {172, 29}, {212, 40}, {252, 35}, {292, 45}, {332, "
            "39}, {372, 49}, {412, 43}, {452, 53}, {492, 48}, {532, 58}, {572, "
            "51}});\nchart->addSeries(secondary);\n"),
        [](QWidget* parent) {
            auto* chart = new ScatterChart(parent);
            chart->setObjectName("chartPresentation");
            chart->setMinimumHeight(420);
            chart->setXRange(0, 600);
            chart->setYRange(0, 80);
            chart->setTitle("Request distribution");
            chart->setSubtitle("Response time (ms) by payload (KB)");
            auto* model = new ChartModel(chart);
            model->setName("Primary");
            model->setPoints({{0, 22},
                              {40, 31},
                              {80, 26},
                              {120, 42},
                              {160, 37},
                              {200, 50},
                              {240, 45},
                              {280, 58},
                              {320, 52},
                              {360, 62},
                              {400, 56},
                              {440, 68},
                              {480, 60},
                              {520, 70},
                              {560, 64}});
            chart->setModel(model);
            auto* secondary = new ChartModel(chart);
            secondary->setName("Secondary");
            secondary->setPoints({{12, 18},
                                  {52, 26},
                                  {92, 21},
                                  {132, 33},
                                  {172, 29},
                                  {212, 40},
                                  {252, 35},
                                  {292, 45},
                                  {332, 39},
                                  {372, 49},
                                  {412, 43},
                                  {452, 53},
                                  {492, 48},
                                  {532, 58},
                                  {572, 51}});
            chart->addSeries(secondary);
            return chart;
        },
        true)};
}

QVector<GallerySample> sparklineSamples()
{
    return {makeSample(
        QStringLiteral("sparkline-basic"), QStringLiteral("Recent trend"),
        QStringLiteral("Illustrative data. The model is independent of this chart component."),
        QStringLiteral(
            "auto* chart = new "
            "Sparkline(this);\nchart->setObjectName(\"chartPresentation\");\nchart->setFixedSize("
            "220, 56);\nchart->setAccessibleName(\"Recent trend\");\nauto* model = new "
            "ChartModel(chart);\nmodel->setName(\"Primary\");\nmodel->setPoints({{0, 20}, {1, 26}, "
            "{2, 22}, {3, 32}, {4, 28}, {5, 36}, {6, 33}, {7, 42}, {8, 38}, {9, 47}, {10, 42}, "
            "{11, 50}});\nchart->setModel(model);\n"),
        [](QWidget* parent) {
            auto* chart = new Sparkline(parent);
            chart->setObjectName("chartPresentation");
            chart->setFixedSize(220, 56);
            chart->setAccessibleName("Recent trend");
            auto* model = new ChartModel(chart);
            model->setName("Primary");
            model->setPoints({{0, 20},
                              {1, 26},
                              {2, 22},
                              {3, 32},
                              {4, 28},
                              {5, 36},
                              {6, 33},
                              {7, 42},
                              {8, 38},
                              {9, 47},
                              {10, 42},
                              {11, 50}});
            chart->setModel(model);
            return chart;
        },
        false)};
}
QVector<GallerySample> chartViewSamples()
{
    return {
        makeSample(
            QStringLiteral("chart-view-basic"), QStringLiteral("Compare two series"),
            QStringLiteral("Illustrative values. Each caller-owned model is shared independently "
                           "of its chart view."),
            QStringLiteral("auto* chart = new ChartView(this);\n"
                           "chart->setMinimumHeight(420);\n"
                           "chart->setTitle(\"Sample response times\");\n"
                           "auto* primary = new ChartModel(this);\n"
                           "primary->setName(\"Primary\");\n"
                           "primary->setPoints({{0, 18}, {1, 32}, {2, 24}, {3, 48}, {4, 36}, {5, "
                           "54}, {6, 42}});\n"
                           "auto* secondary = new ChartModel(this);\n"
                           "secondary->setName(\"Secondary\");\n"
                           "secondary->setPoints({{0, 12}, {1, 20}, {2, 18}, {3, 32}, {4, 28}, {5, "
                           "38}, {6, 30}});\n"
                           "chart->setModel(primary);\n"
                           "chart->addSeries(secondary);\n"),
            [](QWidget* parent) {
                auto* chart = new ChartView(parent);
                chart->setMinimumHeight(420);
                chart->setTitle(QStringLiteral("Sample response times"));
                auto* primary = new ChartModel(chart);
                primary->setName(QStringLiteral("Primary"));
                primary->setPoints({{0, 18}, {1, 32}, {2, 24}, {3, 48}, {4, 36}, {5, 54}, {6, 42}});
                auto* secondary = new ChartModel(chart);
                secondary->setName(QStringLiteral("Secondary"));
                secondary->setPoints(
                    {{0, 12}, {1, 20}, {2, 18}, {3, 32}, {4, 28}, {5, 38}, {6, 30}});
                chart->setModel(primary);
                chart->addSeries(secondary);
                return chart;
            },
            true),
        makeSample(
            QStringLiteral("chart-view-types"), QStringLiteral("Chart presentations"),
            QStringLiteral("Switch the presentation without replacing the model. Hover for values; "
                           "use arrow keys to explore the raw data."),
            QStringLiteral(
                "auto* panel = new QWidget(this);\n"
                "auto* layout = new QVBoxLayout(panel);\n"
                "layout->setContentsMargins(0, 0, 0, 0);\n"
                "auto* model = new ChartModel(panel);\n"
                "model->setName(\"Sample activity\");\n"
                "model->setPoints({{0, 18}, {1, 32}, {2, 24}, {3, 48}, {4, 36}, {5, 54}, {6, "
                "42}},\n"
                "                 {\"Mon\", \"Tue\", \"Wed\", \"Thu\", \"Fri\", \"Sat\", "
                "\"Sun\"});\n"
                "auto* chart = new ChartView(panel);\n"
                "chart->setModel(model);\n"
                "chart->setMinimumHeight(420);\n"
                "chart->setChartType(ChartView::Bar);\n"
                "auto* type = new ComboBox(panel);\n"
                "type->setAccessibleName(\"Chart presentation\");\n"
                "type->addItems({\"Line\", \"Area\", \"Bar\", \"Horizontal bar\", \"Pie\", "
                "\"Donut\", \"Scatter\", \"Sparkline\"});\n"
                "type->setCurrentIndex(int(ChartView::Bar));\n"
                "QObject::connect(type, qOverload<int>(&ComboBox::currentIndexChanged), chart,\n"
                "    [chart](int index) { "
                "chart->setChartType(static_cast<ChartView::ChartType>(index)); });\n"
                "layout->addWidget(type);\n"
                "layout->addWidget(chart);\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* layout = new QVBoxLayout(panel);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(12);
                auto* model = new ChartModel(panel);
                model->setName(QStringLiteral("Sample activity"));
                model->setPoints({{0, 18}, {1, 32}, {2, 24}, {3, 48}, {4, 36}, {5, 54}, {6, 42}},
                                 {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"});
                auto* chart = new ChartView(panel);
                chart->setModel(model);
                chart->setMinimumHeight(420);
                chart->setChartType(ChartView::Bar);
                chart->setObjectName(QStringLiteral("chartPresentation"));
                auto* type = new basicinput::ComboBox(panel);
                type->setObjectName(QStringLiteral("chartType"));
                type->setAccessibleName(QStringLiteral("Chart presentation"));
                type->addItems({"Line", "Area", "Bar", "Horizontal bar", "Pie", "Donut", "Scatter",
                                "Sparkline"});
                type->setCurrentIndex(int(ChartView::Bar));
                QObject::connect(type, qOverload<int>(&basicinput::ComboBox::currentIndexChanged),
                                 chart, [chart](int index) {
                                     chart->setChartType(static_cast<ChartView::ChartType>(index));
                                 });
                layout->addWidget(type);
                layout->addWidget(chart);
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("chart-view-shared-model"), QStringLiteral("One model, two views"),
            QStringLiteral("Append a batch once. Both the detailed chart and the compact trend "
                           "update from the same model."),
            QStringLiteral("auto* panel = new QWidget(this);\n"
                           "auto* layout = new QVBoxLayout(panel);\n"
                           "layout->setContentsMargins(0, 0, 0, 0);\n"
                           "auto* model = new ChartModel(panel);\n"
                           "model->setName(\"Sample signal\");\n"
                           "model->setPoints({{0, 18}, {1, 32}, {2, 24}, {3, 48}, {4, 36}, {5, "
                           "54}, {6, 42}});\n"
                           "auto* detail = new ChartView(panel);\n"
                           "detail->setChartType(ChartView::Area);\n"
                           "detail->setModel(model);\n"
                           "detail->setMinimumHeight(420);\n"
                           "auto* trend = new Sparkline(panel);\n"
                           "trend->setModel(model);\n"
                           "trend->setFixedSize(220, 52);\n"
                           "auto* append = new Button(\"Append sample batch\", panel);\n"
                           "QObject::connect(append, &Button::clicked, model, [model] {\n"
                           "    const double x = model->maximumX();\n"
                           "    model->appendPoints({{x + 1, 30}, {x + 2, 58}, {x + 3, 40}});\n"
                           "});\n"
                           "layout->addWidget(detail);\n"
                           "layout->addWidget(trend);\n"
                           "layout->addWidget(append, 0, Qt::AlignLeft);\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* layout = new QVBoxLayout(panel);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(12);
                auto* model = new ChartModel(panel);
                model->setName(QStringLiteral("Sample signal"));
                model->setPoints({{0, 18}, {1, 32}, {2, 24}, {3, 48}, {4, 36}, {5, 54}, {6, 42}});
                auto* detail = new ChartView(panel);
                detail->setChartType(ChartView::Area);
                detail->setModel(model);
                detail->setMinimumHeight(420);
                auto* trend = new Sparkline(panel);
                trend->setModel(model);
                trend->setFixedSize(220, 52);
                auto* append = new basicinput::Button(QStringLiteral("Append sample batch"), panel);
                QObject::connect(append, &basicinput::Button::clicked, model, [model] {
                    const double x = model->maximumX();
                    model->appendPoints({{x + 1, 30}, {x + 2, 58}, {x + 3, 40}});
                });
                layout->addWidget(detail);
                layout->addWidget(trend);
                layout->addWidget(append, 0, Qt::AlignLeft);
                return panel;
            },
            true)};
}
} // namespace
QVector<GallerySample> chartsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("chart-view"))
        return chartViewSamples();
    if (routeId == QStringLiteral("line-chart"))
        return lineChartSamples();
    if (routeId == QStringLiteral("area-chart"))
        return areaChartSamples();
    if (routeId == QStringLiteral("bar-chart"))
        return barChartSamples();
    if (routeId == QStringLiteral("horizontal-bar-chart"))
        return horizontalBarChartSamples();
    if (routeId == QStringLiteral("pie-chart"))
        return pieChartSamples();
    if (routeId == QStringLiteral("donut-chart"))
        return donutChartSamples();
    if (routeId == QStringLiteral("scatter-chart"))
        return scatterChartSamples();
    if (routeId == QStringLiteral("sparkline"))
        return sparklineSamples();
    return {};
}
} // namespace fluent::gallery

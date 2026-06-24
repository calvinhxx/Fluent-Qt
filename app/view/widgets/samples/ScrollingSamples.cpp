#include "ScrollingSamples.h"

#include <algorithm>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/basicinput/Slider.h"
#include "components/collections/FlipView.h"
#include "components/foundation/FluentElement.h"
#include "components/scrolling/AnnotatedScrollBar.h"
#include "components/scrolling/PipsPager.h"
#include "components/scrolling/ScrollBar.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::basicinput::Slider;
using fluent::collections::FlipView;
using fluent::scrolling::AnnotatedScrollBar;
using fluent::scrolling::AnnotatedScrollBarLabel;
using fluent::scrolling::PipsPager;
using fluent::scrolling::ScrollBar;
using fluent::scrolling::ScrollView;
using fluent::textfields::Label;
using samples::gradientPixmap;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    label->setWordWrap(true);
    label->setTextColorRole(Label::TextColorRole::Primary);  // QSS-proof on the styled preview surface
    return label;
}

class ScrollingSampleSurface : public QWidget, public fluent::FluentElement {
public:
    explicit ScrollingSampleSurface(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 16);
        layout->setSpacing(12);
        layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        advertiseSurfaceColor();
    }

protected:
    void onThemeUpdated() override
    {
        advertiseSurfaceColor();
        update();
    }

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

private:
    // Publish the real (painted) background via a dynamic property so rounded children that
    // antialias their corners against the surface — e.g. FlipView's corner mask, which walks up
    // looking for "fluentSurfaceColor" — pick up the correct color. The app themes via paint, not
    // a global QPalette, and a palette would not survive QStyleSheetStyle on a styled ancestor;
    // the property is read directly and is immune to both.
    // zh_CN: 通过动态属性公布真实（绘制用）背景色，让对角做抗锯齿的圆角子控件
    //（如向上查找 "fluentSurfaceColor" 的 FlipView 圆角遮罩）取到正确颜色。本应用靠绘制而非全局
    // QPalette 上主题，且 palette 在带样式表的祖先下会被 QStyleSheetStyle 重置；动态属性直接读取，二者皆免疫。
    void advertiseSurfaceColor()
    {
        setProperty("fluentSurfaceColor", themeColors().bgCanvas);
    }
};

QWidget* pipsSurface(QWidget* parent, int spacing = 12)
{
    auto* surface = new ScrollingSampleSurface(parent);
    surface->layout()->setSpacing(spacing);
    return surface;
}

Button* pipsButton(QWidget* parent, const QString& text)
{
    auto* button = new Button(text, parent);
    button->setFluentSize(Button::Small);
    button->setMinimumWidth(74);
    return button;
}

Button* sampleButton(QWidget* parent, const QString& text)
{
    auto* button = new Button(text, parent);
    button->setFluentSize(Button::Small);
    button->setMinimumWidth(74);
    return button;
}

void setButtonActive(Button* button, bool active)
{
    button->setFluentStyle(active ? Button::Accent : Button::Standard);
}

void refreshPagerFixedSize(PipsPager* pager)
{
    pager->setFixedSize(pager->sizeHint());
}

QString visibilityName(PipsPager::PipsPagerButtonVisibility visibility)
{
    switch (visibility) {
    case PipsPager::PipsPagerButtonVisibility::Collapsed:
        return QStringLiteral("Collapsed");
    case PipsPager::PipsPagerButtonVisibility::Visible:
        return QStringLiteral("Visible");
    case PipsPager::PipsPagerButtonVisibility::VisibleOnPointerOver:
        return QStringLiteral("VisibleOnPointerOver");
    }
    return QString();
}

struct PagerPicturePage {
    QString title;
    QColor from;
    QColor to;
};

const QVector<PagerPicturePage>& pagerPicturePages()
{
    static const QVector<PagerPicturePage> pages = {
        {QStringLiteral("Coastal route"), QColor(QStringLiteral("#375F90")), QColor(QStringLiteral("#B9D7EA"))},
        {QStringLiteral("Forest trail"), QColor(QStringLiteral("#3F7B52")), QColor(QStringLiteral("#C8E3B4"))},
        {QStringLiteral("Desert light"), QColor(QStringLiteral("#9A6339")), QColor(QStringLiteral("#F0CF9A"))},
        {QStringLiteral("Evening ridge"), QColor(QStringLiteral("#7B4F91")), QColor(QStringLiteral("#D9C2EA"))},
        {QStringLiteral("Harbor morning"), QColor(QStringLiteral("#4B6D73")), QColor(QStringLiteral("#C7E6E4"))},
        {QStringLiteral("Mountain dusk"), QColor(QStringLiteral("#8C4F4F")), QColor(QStringLiteral("#E9C7C7"))}
    };
    return pages;
}

QLabel* makePagerPicture(QWidget* parent,
                         const PagerPicturePage& page,
                         int pageIndex,
                         int pageCount,
                         const QSize& size)
{
    auto* picture = new QLabel(parent);
    picture->setFixedSize(size);
    picture->setPixmap(gradientPixmap(size, page.from, page.to,
                                      QStringLiteral("%1  Page %2 of %3")
                                          .arg(page.title)
                                          .arg(pageIndex + 1)
                                          .arg(pageCount)));
    return picture;
}

class ScrollViewDemoCanvas : public QWidget, public fluent::scrolling::ScrollViewZoomAware, public fluent::FluentElement {
public:
    explicit ScrollViewDemoCanvas(const QSize& logicalSize,
                                  const QString& title,
                                  QWidget* parent = nullptr)
        : QWidget(parent)
        , m_logicalSize(logicalSize)
        , m_title(title)
    {
        setFixedSize(logicalSize);
    }

    QSizeF scrollViewUnscaledSize() const override
    {
        return QSizeF(m_logicalSize);
    }

    void setScrollViewZoomFactor(qreal factor) override
    {
        if (qFuzzyCompare(m_zoomFactor, factor))
            return;
        m_zoomFactor = factor;
        update();
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
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto colors = themeColors();
        painter.fillRect(rect(), colors.bgLayer);
        painter.scale(m_zoomFactor, m_zoomFactor);

        QPen gridPen(colors.strokeDivider);
        gridPen.setWidth(1);
        painter.setPen(gridPen);
        for (int x = 0; x < m_logicalSize.width(); x += 48)
            painter.drawLine(x, 0, x, m_logicalSize.height());
        for (int y = 0; y < m_logicalSize.height(); y += 48)
            painter.drawLine(0, y, m_logicalSize.width(), y);

        const QVector<QColor> swatches = {
            colors.accentDefault,
            colors.systemSuccess,
            colors.systemCaution,
            colors.systemInfo
        };
        for (int index = 0; index < 18; ++index) {
            const int col = index % 6;
            const int row = index / 6;
            QRect tile(28 + col * 102, 76 + row * 96, 72, 64);
            QColor fill = swatches.at(index % swatches.size());
            fill.setAlphaF(currentTheme() == fluent::FluentElement::Dark ? 0.72 : 0.88);
            painter.setBrush(fill);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(tile, themeRadius().control, themeRadius().control);
        }

        QFont titleFont = themeFont(QStringLiteral("BodyStrong")).toQFont();
        titleFont.setWeight(QFont::DemiBold);
        painter.setFont(titleFont);
        painter.setPen(colors.textPrimary);
        painter.drawText(QRect(24, 20, m_logicalSize.width() - 48, 28),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         m_title);

        QFont bodyFont = themeFont(QStringLiteral("Caption")).toQFont();
        painter.setFont(bodyFont);
        painter.setPen(colors.textSecondary);
        painter.drawText(QRect(24, 46, m_logicalSize.width() - 48, 22),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("%1 x %2, zoom %3x")
                             .arg(m_logicalSize.width())
                             .arg(m_logicalSize.height())
                             .arg(m_zoomFactor, 0, 'f', 1));
    }

private:
    QSize m_logicalSize;
    QString m_title;
    qreal m_zoomFactor = 1.0;
};

QWidget* makeScrollViewStackContent(QWidget* parent,
                                    const QSize& imageSize = QSize(360, 130))
{
    auto* content = new QWidget(parent);
    const auto& pages = pagerPicturePages();
    const int margin = 12;
    const int spacing = 10;
    content->setFixedSize(imageSize.width() + margin * 2,
                          margin * 2 + pages.size() * imageSize.height()
                              + (pages.size() - 1) * spacing);

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(spacing);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    for (int index = 0; index < pages.size(); ++index) {
        layout->addWidget(makePagerPicture(content, pages.at(index),
                                           index, pages.size(), imageSize));
    }
    return content;
}

QString scrollViewOffsetText(const ScrollView* scrollView)
{
    return QStringLiteral("Offset: %1, %2 / Range: %3, %4 / Zoom: %5x")
        .arg(scrollView->horizontalOffset())
        .arg(scrollView->verticalOffset())
        .arg(scrollView->scrollableWidth())
        .arg(scrollView->scrollableHeight())
        .arg(scrollView->zoomFactor(), 0, 'f', 1);
}

void bindScrollViewStatus(ScrollView* scrollView, Label* status)
{
    auto updateStatus = [scrollView, status]() {
        status->setText(scrollViewOffsetText(scrollView));
    };
    QObject::connect(scrollView, &ScrollView::scrollPositionChanged,
                     status, [updateStatus](int, int) { updateStatus(); });
    QObject::connect(scrollView, &ScrollView::zoomFactorChanged,
                     status, updateStatus);
    updateStatus();
}

QVector<AnnotatedScrollBarLabel> yearLabels()
{
    QVector<AnnotatedScrollBarLabel> labels;
    for (int year = 2023; year >= 2015; --year) {
        const int offset = (2023 - year) * 120;
        labels.append(AnnotatedScrollBarLabel(QString::number(year), offset,
                                              QStringLiteral("October %1").arg(year)));
    }
    return labels;
}

QString yearDetailForOffset(int offset)
{
    const int year = 2023 - std::clamp(offset / 120, 0, 8);
    return QStringLiteral("October %1").arg(year);
}

QString labelTextForOffset(const QVector<AnnotatedScrollBarLabel>& labels, int offset)
{
    if (labels.isEmpty())
        return QString();

    AnnotatedScrollBarLabel current = labels.first();
    for (const AnnotatedScrollBarLabel& label : labels) {
        if (offset >= label.offset)
            current = label;
    }
    return current.text;
}

struct ColorSection {
    QString name;
    QColor color;
    int count = 0;
};

const QVector<ColorSection>& colorSections()
{
    static const QVector<ColorSection> sections = {
        {QStringLiteral("Azure"), QColor(QStringLiteral("#0078D4")), 32},
        {QStringLiteral("Crimson"), QColor(QStringLiteral("#DC143C")), 50},
        {QStringLiteral("Cyan"), QColor(QStringLiteral("#00B7C3")), 8},
        {QStringLiteral("Fuchsia"), QColor(QStringLiteral("#C239B3")), 70},
        {QStringLiteral("Gold"), QColor(QStringLiteral("#FFB900")), 90}
    };
    return sections;
}

constexpr int kAnnotatedItemWidth = 120;
constexpr int kAnnotatedItemHeight = 90;
constexpr int kAnnotatedContentWidth = kAnnotatedItemWidth * 3;

int totalColorItemCount()
{
    int total = 0;
    for (const ColorSection& section : colorSections())
        total += section.count;
    return total;
}

int itemsPerRowForWidth(int width)
{
    return std::max(1, width / kAnnotatedItemWidth);
}

int offsetForItem(int itemIndex, int width)
{
    return kAnnotatedItemHeight * (itemIndex / itemsPerRowForWidth(width));
}

int colorContentHeightForWidth(int width)
{
    const int rows = (totalColorItemCount() + itemsPerRowForWidth(width) - 1)
                     / itemsPerRowForWidth(width);
    return rows * kAnnotatedItemHeight;
}

QVector<AnnotatedScrollBarLabel> colorSectionLabels(int width)
{
    QVector<AnnotatedScrollBarLabel> labels;
    int firstItemIndex = 0;
    for (const ColorSection& section : colorSections()) {
        labels.append(AnnotatedScrollBarLabel(section.name,
                                              offsetForItem(firstItemIndex, width),
                                              section.name));
        firstItemIndex += section.count;
    }
    return labels;
}

QString colorSectionForOffset(int offset, int width)
{
    if (colorSections().isEmpty())
        return QString();

    QString current = colorSections().first().name;
    int firstItemIndex = 0;
    for (const ColorSection& section : colorSections()) {
        if (offset >= offsetForItem(firstItemIndex, width))
            current = section.name;
        firstItemIndex += section.count;
    }
    return current;
}

QVector<AnnotatedScrollBarLabel> monthLabels()
{
    static const QVector<QString> months = {
        QStringLiteral("Jan"), QStringLiteral("Feb"), QStringLiteral("Mar"),
        QStringLiteral("Apr"), QStringLiteral("May"), QStringLiteral("Jun"),
        QStringLiteral("Jul"), QStringLiteral("Aug"), QStringLiteral("Sep"),
        QStringLiteral("Oct"), QStringLiteral("Nov"), QStringLiteral("Dec")
    };

    QVector<AnnotatedScrollBarLabel> labels;
    for (int index = 0; index < months.size(); ++index) {
        labels.append(AnnotatedScrollBarLabel(months.at(index), index * 100,
                                              QStringLiteral("%1 section").arg(months.at(index))));
    }
    return labels;
}

class StandaloneAnnotatedScrollBarCard : public QWidget {
public:
    explicit StandaloneAnnotatedScrollBarCard(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(18);

        auto* bar = new AnnotatedScrollBar(this);
        const QVector<AnnotatedScrollBarLabel> labels = yearLabels();
        bar->setFixedSize(148, 300);
        bar->setRange(0, 960);
        bar->setPageStep(120);
        bar->setLabelColumnWidth(56);
        bar->setIndicatorWidth(32);
        bar->setLabels(labels);
        bar->setDetailLabelProvider(&yearDetailForOffset);

        auto* details = new QWidget(this);
        auto* detailsLayout = new QVBoxLayout(details);
        detailsLayout->setContentsMargins(0, 4, 0, 0);
        detailsLayout->setSpacing(8);
        detailsLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

        auto* offsetLabel = makeStatusLabel(details, QStringLiteral("Offset: 0"));
        auto* currentLabel = makeStatusLabel(details, QStringLiteral("Current label: 2023"));
        auto* detailLabel = makeStatusLabel(details, QStringLiteral("Detail: October 2023"));
        offsetLabel->setMinimumWidth(offsetLabel->fontMetrics().horizontalAdvance(
            QStringLiteral("Offset: 8888")));
        currentLabel->setMinimumWidth(currentLabel->fontMetrics().horizontalAdvance(
            QStringLiteral("Current label: 8888")));
        detailLabel->setMinimumWidth(detailLabel->fontMetrics().horizontalAdvance(
            QStringLiteral("Detail: October 8888")));

        detailsLayout->addWidget(offsetLabel);
        detailsLayout->addWidget(currentLabel);
        detailsLayout->addWidget(detailLabel);
        detailsLayout->addStretch(1);

        auto updateDetails = [labels, offsetLabel, currentLabel, detailLabel](int value) {
            offsetLabel->setText(QStringLiteral("Offset: %1").arg(value));
            currentLabel->setText(QStringLiteral("Current label: %1")
                                      .arg(labelTextForOffset(labels, value)));
            detailLabel->setText(QStringLiteral("Detail: %1").arg(yearDetailForOffset(value)));
        };

        QObject::connect(bar, &AnnotatedScrollBar::valueChanged, details, updateDetails);
        QObject::connect(bar, &AnnotatedScrollBar::labelActivated, details,
                         [updateDetails](int offset, const QString&) { updateDetails(offset); });

        layout->addWidget(bar);
        layout->addWidget(details, 0, Qt::AlignTop);
    }

    QSize sizeHint() const override
    {
        return QSize(390, 300);
    }
};

class AnnotatedColorSectionsContent : public QWidget, public fluent::FluentElement {
public:
    explicit AnnotatedColorSectionsContent(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(kAnnotatedContentWidth,
                     colorContentHeightForWidth(kAnnotatedContentWidth));
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
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.fillRect(rect(), themeColors().bgLayer);

        QFont labelFont = themeFont(QStringLiteral("Caption")).toQFont();
        labelFont.setWeight(QFont::DemiBold);
        painter.setFont(labelFont);

        const int itemsPerRow = itemsPerRowForWidth(width());
        int itemIndex = 0;
        for (const ColorSection& section : colorSections()) {
            QColor fill = section.color;
            fill.setAlphaF(currentTheme() == fluent::FluentElement::Dark ? 0.72 : 0.88);

            for (int indexInSection = 0; indexInSection < section.count; ++indexInSection) {
                const int row = itemIndex / itemsPerRow;
                const int column = itemIndex % itemsPerRow;
                const QRect cell(column * kAnnotatedItemWidth,
                                 row * kAnnotatedItemHeight,
                                 kAnnotatedItemWidth,
                                 kAnnotatedItemHeight);
                const QRect itemRect = cell.adjusted(12, 10, -12, -10);

                painter.setPen(Qt::NoPen);
                painter.setBrush(fill);
                painter.drawRoundedRect(itemRect, 6, 6);

                if (indexInSection == 0) {
                    painter.setPen(QColor(255, 255, 255, 235));
                    painter.drawText(itemRect.adjusted(6, 0, -6, 0),
                                     Qt::AlignCenter | Qt::TextSingleLine,
                                     section.name);
                }

                ++itemIndex;
            }
        }
    }
};

class LinkedAnnotatedScrollBarCard : public QWidget {
public:
    explicit LinkedAnnotatedScrollBarCard(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        auto* row = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);

        auto* scrollView = new ScrollView(row);
        scrollView->setFixedSize(380, 320);
        scrollView->setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Hidden);
        scrollView->setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Hidden);
        scrollView->setWidget(new AnnotatedColorSectionsContent());

        auto* bar = new AnnotatedScrollBar(row);
        bar->setFixedSize(150, 320);
        bar->setPreferredSize(QSize(150, 320));
        bar->setMinimumBarSize(QSize(120, 220));
        bar->setLabelColumnWidth(86);
        bar->setMinimumLabelSpacing(56);
        bar->setIndicatorWidth(34);
        bar->setCaretSize(QSize(16, 18));
        bar->setLabels(colorSectionLabels(kAnnotatedContentWidth));
        bar->setDetailLabelProvider([](int offset) {
            return colorSectionForOffset(offset, kAnnotatedContentWidth);
        });
        bar->connectToScrollView(scrollView);

        rowLayout->addWidget(scrollView);
        rowLayout->addWidget(bar);

        auto* status = makeStatusLabel(this, QStringLiteral("Section: Azure - offset 0"));
        status->setMinimumWidth(status->fontMetrics().horizontalAdvance(
            QStringLiteral("Section: Fuchsia - offset 8888")));

        auto updateStatus = [status](int offset) {
            status->setText(QStringLiteral("Section: %1 - offset %2")
                                .arg(colorSectionForOffset(offset, kAnnotatedContentWidth))
                                .arg(offset));
        };
        QObject::connect(scrollView, &ScrollView::scrollPositionChanged, status,
                         [updateStatus](int, int verticalOffset) {
                             updateStatus(verticalOffset);
                         });
        QObject::connect(bar, &AnnotatedScrollBar::labelActivated, status,
                         [updateStatus](int offset, const QString&) { updateStatus(offset); });

        layout->addWidget(row);
        layout->addWidget(status);
    }

    QSize sizeHint() const override
    {
        return QSize(542, 354);
    }
};

class AnnotatedScrollBarHeightCard : public QWidget {
public:
    explicit AnnotatedScrollBarHeightCard(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(18);

        auto* bar = new AnnotatedScrollBar(this);
        bar->setFixedSize(144, 360);
        bar->setRange(0, 1100);
        bar->setPageStep(120);
        bar->setLabelColumnWidth(56);
        bar->setMinimumLabelSpacing(28);
        bar->setIndicatorWidth(32);
        bar->setLabels(monthLabels());

        auto* controls = new QWidget(this);
        auto* controlsLayout = new QVBoxLayout(controls);
        controlsLayout->setContentsMargins(0, 4, 0, 0);
        controlsLayout->setSpacing(8);
        controlsLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

        auto* heightValue = makeStatusLabel(controls, QStringLiteral("Height: 360 px"));
        auto* visibleValue = makeStatusLabel(controls, QStringLiteral("Visible labels: 12 of 12"));
        heightValue->setMinimumWidth(heightValue->fontMetrics().horizontalAdvance(
            QStringLiteral("Height: 888 px")));
        visibleValue->setMinimumWidth(visibleValue->fontMetrics().horizontalAdvance(
            QStringLiteral("Visible labels: 88 of 88")));

        auto* slider = new Slider(Qt::Horizontal, controls);
        slider->setRange(180, 360);
        slider->setSingleStep(20);
        slider->setPageStep(40);
        slider->setValue(360);
        slider->setFixedSize(220, 36);

        auto updateSummary = [bar, heightValue, visibleValue]() {
            heightValue->setText(QStringLiteral("Height: %1 px").arg(bar->height()));
            visibleValue->setText(QStringLiteral("Visible labels: %1 of %2")
                                      .arg(bar->visibleLabelCount())
                                      .arg(bar->labels().size()));
        };

        QObject::connect(slider, &Slider::valueChanged, controls, [bar, updateSummary](int value) {
            bar->setFixedHeight(value);
            bar->updateGeometry();
            updateSummary();
        });

        controlsLayout->addWidget(heightValue);
        controlsLayout->addWidget(visibleValue);
        controlsLayout->addWidget(slider);
        controlsLayout->addStretch(1);

        layout->addWidget(bar);
        layout->addWidget(controls, 0, Qt::AlignTop);
        updateSummary();
    }

    QSize sizeHint() const override
    {
        return QSize(382, 360);
    }
};

QVector<GallerySample> annotatedScrollBarSamples()
{
    return {
        makeSample(QStringLiteral("annotated-scrollbar-basic"),
                   QStringLiteral("Labeled scroll range with details"),
                   QStringLiteral("Labels mark notable offsets and the detail provider resolves hover text."),
                   QStringLiteral("auto* bar = new AnnotatedScrollBar(this);\n"
                                  "bar->setRange(0, 960);\n"
                                  "bar->setPageStep(120);\n"
                                  "bar->setLabels({\n"
                                  "    {\"2023\", 0, \"October 2023\"},\n"
                                  "    {\"2022\", 120, \"October 2022\"},\n"
                                  "    {\"2021\", 240, \"October 2021\"},\n"
                                  "    {\"2020\", 360, \"October 2020\"},\n"
                                  "    {\"2019\", 480, \"October 2019\"},\n"
                                  "    {\"2018\", 600, \"October 2018\"},\n"
                                  "    {\"2017\", 720, \"October 2017\"},\n"
                                  "    {\"2016\", 840, \"October 2016\"},\n"
                                  "    {\"2015\", 960, \"October 2015\"}\n"
                                  "});\n"
                                  "bar->setDetailLabelProvider([](int offset) {\n"
                                  "    const int year = 2023 - std::clamp(offset / 120, 0, 8);\n"
                                  "    return QStringLiteral(\"October %1\").arg(year);\n"
                                  "});"),
                   [](QWidget* parent) {
                       return new StandaloneAnnotatedScrollBarCard(parent);
                   }),
        makeSample(QStringLiteral("annotated-scrollbar-scrollview"),
                   QStringLiteral("Annotated ScrollView controller"),
                   QStringLiteral("A hidden vertical ScrollView bar is mirrored and driven by an external AnnotatedScrollBar."),
                   QStringLiteral("auto* scrollView = new ScrollView(this);\n"
                                  "scrollView->setWidget(colorSectionsContent);\n"
                                  "scrollView->setVerticalScrollBarVisibility(\n"
                                  "    ScrollView::ScrollBarVisibility::Hidden);\n"
                                  "\n"
                                  "auto* bar = new AnnotatedScrollBar(this);\n"
                                  "bar->setLabels({\n"
                                  "    {\"Azure\", 0}, {\"Crimson\", 900}, {\"Cyan\", 2430},\n"
                                  "    {\"Fuchsia\", 2700}, {\"Gold\", 4770}\n"
                                  "});\n"
                                  "bar->setDetailLabelProvider([](int offset) {\n"
                                  "    return colorSectionForOffset(offset);\n"
                                  "});\n"
                                  "bar->connectToScrollView(scrollView);"),
                   [](QWidget* parent) {
                       return new LinkedAnnotatedScrollBarCard(parent);
                   }),
        makeSample(QStringLiteral("annotated-scrollbar-label-density"),
                   QStringLiteral("Label density adapts to height"),
                   QStringLiteral("Reducing the rail height hides colliding labels while keeping the full label model intact."),
                   QStringLiteral("auto* bar = new AnnotatedScrollBar(this);\n"
                                  "bar->setRange(0, 1100);\n"
                                  "bar->setMinimumLabelSpacing(28);\n"
                                  "bar->setLabels(monthLabels);\n"
                                  "\n"
                                  "auto* heightSlider = new Slider(Qt::Horizontal, this);\n"
                                  "heightSlider->setRange(180, 360);\n"
                                  "connect(heightSlider, &Slider::valueChanged,\n"
                                  "        bar, [bar](int height) {\n"
                                  "            bar->setFixedHeight(height);\n"
                                  "        });"),
                   [](QWidget* parent) {
                       return new AnnotatedScrollBarHeightCard(parent);
                   })
    };
}

QVector<GallerySample> pipsPagerSamples()
{
    return {
        makeSample(QStringLiteral("pips-pager-flipview"),
                   QStringLiteral("PipsPager integrated with FlipView"),
                   QStringLiteral("The pager and FlipView keep their selected page indexes synchronized."),
                   QStringLiteral("auto* flipView = new FlipView(this);\n"
                                  "flipView->setShowPageIndicator(false);\n"
                                  "for (QWidget* page : pages)\n"
                                  "    flipView->addPage(page);\n"
                                  "\n"
                                  "auto* pager = new PipsPager(this);\n"
                                  "pager->setNumberOfPages(flipView->pageCount());\n"
                                  "pager->setMaxVisiblePips(5);\n"
                                  "\n"
                                  "connect(pager, &PipsPager::selectedPageIndexChanged,\n"
                                  "        flipView, &FlipView::setCurrentIndex);\n"
                                  "connect(flipView, &FlipView::currentIndexChanged,\n"
                                  "        pager, &PipsPager::setSelectedPageIndex);"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       const QSize pageSize(420, 220);
                       const auto& pages = pagerPicturePages();

                       auto* flipView = new FlipView(surface);
                       flipView->setFixedSize(pageSize);
                       flipView->setShowPageIndicator(false);
                       flipView->setShowNavigationButtons(true);
                       for (int index = 0; index < pages.size(); ++index) {
                           flipView->addPage(makePagerPicture(flipView, pages.at(index),
                                                              index, pages.size(), pageSize));
                       }

                       auto* pager = new PipsPager(surface);
                       pager->setNumberOfPages(flipView->pageCount());
                       pager->setMaxVisiblePips(5);
                       refreshPagerFixedSize(pager);

                       auto* status = makeStatusLabel(surface, QStringLiteral("Selected page: Coastal route"));
                       auto updateStatus = [status](int index) {
                           const auto& pages = pagerPicturePages();
                           if (index >= 0 && index < pages.size())
                               status->setText(QStringLiteral("Selected page: %1").arg(pages.at(index).title));
                       };

                       QObject::connect(pager, &PipsPager::selectedPageIndexChanged,
                                        flipView, &FlipView::setCurrentIndex);
                       QObject::connect(flipView, &FlipView::currentIndexChanged,
                                        pager, &PipsPager::setSelectedPageIndex);
                       QObject::connect(pager, &PipsPager::selectedPageIndexChanged,
                                        status, updateStatus);

                       layout->addWidget(flipView, 0, Qt::AlignHCenter);
                       layout->addWidget(pager, 0, Qt::AlignHCenter);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("pips-pager-orientation"),
                   QStringLiteral("Orientation"),
                   QStringLiteral("Switching orientation changes the pip layout and keyboard direction."),
                   QStringLiteral("auto* pager = new PipsPager(this);\n"
                                  "pager->setNumberOfPages(7);\n"
                                  "pager->setSelectedPageIndex(3);\n"
                                  "pager->setPreviousButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "pager->setNextButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "\n"
                                  "auto applyOrientation = [pager](Qt::Orientation orientation) {\n"
                                  "    pager->setOrientation(orientation);\n"
                                  "    pager->setFixedSize(pager->sizeHint());\n"
                                  "};\n"
                                  "connect(horizontalButton, &QPushButton::clicked,\n"
                                  "        pager, [applyOrientation] { applyOrientation(Qt::Horizontal); });\n"
                                  "connect(verticalButton, &QPushButton::clicked,\n"
                                  "        pager, [applyOrientation] { applyOrientation(Qt::Vertical); });"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* controls = horizontalGroup(surface, 8);
                       auto* horizontal = pipsButton(controls, QStringLiteral("Horizontal"));
                       auto* vertical = pipsButton(controls, QStringLiteral("Vertical"));
                       controls->layout()->addWidget(horizontal);
                       controls->layout()->addWidget(vertical);

                       auto* row = horizontalGroup(surface, 28);
                       auto* pager = new PipsPager(row);
                       pager->setNumberOfPages(7);
                       pager->setSelectedPageIndex(3);
                       pager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       pager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       pager->setOrientation(Qt::Vertical);
                       const int reservedRowHeight = pager->sizeHint().height();
                       pager->setOrientation(Qt::Horizontal);
                       refreshPagerFixedSize(pager);

                       auto* status = makeStatusLabel(row, QStringLiteral("Orientation: Horizontal"));
                       status->setMinimumWidth(status->fontMetrics().horizontalAdvance(
                           QStringLiteral("Orientation: Horizontal")));
                       row->setMinimumHeight(reservedRowHeight);
                       auto* rowLayout = static_cast<QHBoxLayout*>(row->layout());
                       rowLayout->addWidget(pager, 0, Qt::AlignCenter);
                       rowLayout->addWidget(status, 0, Qt::AlignCenter);

                       auto applyOrientation = [pager, status, horizontal, vertical](Qt::Orientation orientation) {
                           pager->setOrientation(orientation);
                           refreshPagerFixedSize(pager);
                           const bool isHorizontal = orientation == Qt::Horizontal;
                           status->setText(QStringLiteral("Orientation: %1")
                                               .arg(isHorizontal ? QStringLiteral("Horizontal")
                                                                 : QStringLiteral("Vertical")));
                           setButtonActive(horizontal, isHorizontal);
                           setButtonActive(vertical, !isHorizontal);
                       };

                       QObject::connect(horizontal, &QPushButton::clicked, surface,
                                        [applyOrientation] { applyOrientation(Qt::Horizontal); });
                       QObject::connect(vertical, &QPushButton::clicked, surface,
                                        [applyOrientation] { applyOrientation(Qt::Vertical); });
                       applyOrientation(Qt::Horizontal);

                       layout->addWidget(controls);
                       layout->addWidget(row);
                       return surface;
                   }),
        makeSample(QStringLiteral("pips-pager-button-visibility"),
                   QStringLiteral("Button visibility"),
                   QStringLiteral("Previous and next buttons can be collapsed, always visible, or visible on pointer over."),
                   QStringLiteral("auto configure = [](PipsPager* pager,\n"
                                  "                    PipsPager::PipsPagerButtonVisibility visibility) {\n"
                                  "    pager->setNumberOfPages(7);\n"
                                  "    pager->setSelectedPageIndex(3);\n"
                                  "    pager->setPreviousButtonVisibility(visibility);\n"
                                  "    pager->setNextButtonVisibility(visibility);\n"
                                  "};\n"
                                  "\n"
                                  "auto* collapsed = new PipsPager(this);\n"
                                  "configure(collapsed,\n"
                                  "          PipsPager::PipsPagerButtonVisibility::Collapsed);\n"
                                  "auto* visible = new PipsPager(this);\n"
                                  "configure(visible,\n"
                                  "          PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "auto* hover = new PipsPager(this);\n"
                                  "configure(hover,\n"
                                  "          PipsPager::PipsPagerButtonVisibility::VisibleOnPointerOver);"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent, 10);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto addVisibilityRow = [surface, layout](
                                                   const QString& title,
                                                   PipsPager::PipsPagerButtonVisibility visibility) {
                           auto* row = horizontalGroup(surface, 18);
                           auto* label = makeStatusLabel(row, title);
                           label->setMinimumWidth(label->fontMetrics().horizontalAdvance(
                               QStringLiteral("VisibleOnPointerOver")));

                           auto* pager = new PipsPager(row);
                           pager->setNumberOfPages(7);
                           pager->setSelectedPageIndex(3);
                           pager->setPreviousButtonVisibility(visibility);
                           pager->setNextButtonVisibility(visibility);
                           refreshPagerFixedSize(pager);

                           auto* rowLayout = static_cast<QHBoxLayout*>(row->layout());
                           rowLayout->addWidget(label);
                           rowLayout->addWidget(pager, 0, Qt::AlignCenter);
                           rowLayout->addStretch(1);
                           layout->addWidget(row);
                       };

                       addVisibilityRow(visibilityName(PipsPager::PipsPagerButtonVisibility::Collapsed),
                                        PipsPager::PipsPagerButtonVisibility::Collapsed);
                       addVisibilityRow(visibilityName(PipsPager::PipsPagerButtonVisibility::Visible),
                                        PipsPager::PipsPagerButtonVisibility::Visible);
                       addVisibilityRow(visibilityName(PipsPager::PipsPagerButtonVisibility::VisibleOnPointerOver),
                                        PipsPager::PipsPagerButtonVisibility::VisibleOnPointerOver);
                       return surface;
                   }),
        makeSample(QStringLiteral("pips-pager-visible-window"),
                   QStringLiteral("MaxVisiblePips window"),
                   QStringLiteral("When there are more pages than visible pips, the visible window recenters around the selection."),
                   QStringLiteral("auto* pager = new PipsPager(this);\n"
                                  "pager->setNumberOfPages(10);\n"
                                  "pager->setMaxVisiblePips(5);\n"
                                  "pager->setPreviousButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "pager->setNextButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "\n"
                                  "connect(firstButton, &QPushButton::clicked,\n"
                                  "        pager, [pager] { pager->setSelectedPageIndex(0); });\n"
                                  "connect(middleButton, &QPushButton::clicked,\n"
                                  "        pager, [pager] { pager->setSelectedPageIndex(4); });\n"
                                  "connect(lastButton, &QPushButton::clicked,\n"
                                  "        pager, [pager] { pager->setSelectedPageIndex(9); });\n"
                                  "\n"
                                  "const int firstVisible = pager->firstVisiblePage();"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* controls = horizontalGroup(surface, 8);
                       auto* first = pipsButton(controls, QStringLiteral("First"));
                       auto* middle = pipsButton(controls, QStringLiteral("Middle"));
                       auto* last = pipsButton(controls, QStringLiteral("Last"));
                       controls->layout()->addWidget(first);
                       controls->layout()->addWidget(middle);
                       controls->layout()->addWidget(last);

                       auto* pager = new PipsPager(surface);
                       pager->setNumberOfPages(10);
                       pager->setMaxVisiblePips(5);
                       pager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       pager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       refreshPagerFixedSize(pager);

                       auto* status = makeStatusLabel(surface, QString());
                       auto updateStatus = [pager, status, first, middle, last]() {
                           status->setText(QStringLiteral("Selected: %1, first visible page: %2")
                                               .arg(pager->selectedPageIndex())
                                               .arg(pager->firstVisiblePage()));
                           setButtonActive(first, pager->selectedPageIndex() == 0);
                           setButtonActive(middle, pager->selectedPageIndex() == 4);
                           setButtonActive(last, pager->selectedPageIndex() == 9);
                       };

                       QObject::connect(first, &QPushButton::clicked, surface,
                                        [pager] { pager->setSelectedPageIndex(0); });
                       QObject::connect(middle, &QPushButton::clicked, surface,
                                        [pager] { pager->setSelectedPageIndex(4); });
                       QObject::connect(last, &QPushButton::clicked, surface,
                                        [pager] { pager->setSelectedPageIndex(9); });
                       QObject::connect(pager, &PipsPager::selectedPageIndexChanged,
                                        status, [updateStatus](int) { updateStatus(); });
                       updateStatus();

                       layout->addWidget(controls);
                       layout->addWidget(pager, 0, Qt::AlignHCenter);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("pips-pager-metrics-animation"),
                   QStringLiteral("Metrics and animation"),
                   QStringLiteral("Pip, button, and animation metrics can be tuned without changing page logic."),
                   QStringLiteral("auto* pager = new PipsPager(this);\n"
                                  "pager->setNumberOfPages(8);\n"
                                  "pager->setSelectedPageIndex(2);\n"
                                  "pager->setMaxVisiblePips(6);\n"
                                  "pager->setPipCellSize(16);\n"
                                  "pager->setInactivePipDiameter(6);\n"
                                  "pager->setSelectedPipDiameter(10);\n"
                                  "pager->setNavigationButtonSize(30);\n"
                                  "pager->setNavigationIconSize(12);\n"
                                  "pager->setSelectionAnimationDuration(420);\n"
                                  "pager->setPreviousButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "pager->setNextButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "\n"
                                  "connect(previousButton, &QPushButton::clicked,\n"
                                  "        pager, [pager] { pager->goToPreviousPage(); });\n"
                                  "connect(nextButton, &QPushButton::clicked,\n"
                                  "        pager, [pager] { pager->goToNextPage(); });"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* controls = horizontalGroup(surface, 8);
                       auto* previous = pipsButton(controls, QStringLiteral("Previous"));
                       auto* next = pipsButton(controls, QStringLiteral("Next"));
                       controls->layout()->addWidget(previous);
                       controls->layout()->addWidget(next);

                       auto* pager = new PipsPager(surface);
                       pager->setNumberOfPages(8);
                       pager->setSelectedPageIndex(2);
                       pager->setMaxVisiblePips(6);
                       pager->setPipCellSize(16);
                       pager->setInactivePipDiameter(6);
                       pager->setSelectedPipDiameter(10);
                       pager->setNavigationButtonSize(30);
                       pager->setNavigationIconSize(12);
                       pager->setSelectionAnimationDuration(420);
                       pager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       pager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       refreshPagerFixedSize(pager);

                       auto* status = makeStatusLabel(surface, QString());
                       auto updateStatus = [pager, status]() {
                           status->setText(QStringLiteral("Page %1 of %2")
                                               .arg(pager->selectedPageIndex() + 1)
                                               .arg(pager->numberOfPages()));
                       };
                       QObject::connect(previous, &QPushButton::clicked, surface,
                                        [pager] { pager->goToPreviousPage(); });
                       QObject::connect(next, &QPushButton::clicked, surface,
                                        [pager] { pager->goToNextPage(); });
                       QObject::connect(pager, &PipsPager::selectedPageIndexChanged,
                                        status, [updateStatus](int) { updateStatus(); });
                       updateStatus();

                       layout->addWidget(controls);
                       layout->addWidget(pager, 0, Qt::AlignHCenter);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("pips-pager-disabled-empty"),
                   QStringLiteral("Disabled and empty states"),
                   QStringLiteral("Disabled pagers ignore input; zero pages collapse the visible pip window."),
                   QStringLiteral("auto* disabledPager = new PipsPager(this);\n"
                                  "disabledPager->setNumberOfPages(6);\n"
                                  "disabledPager->setSelectedPageIndex(2);\n"
                                  "disabledPager->setPreviousButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "disabledPager->setNextButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "disabledPager->setEnabled(false);\n"
                                  "\n"
                                  "auto* emptyPager = new PipsPager(this);\n"
                                  "emptyPager->setNumberOfPages(0);\n"
                                  "emptyPager->setPreviousButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);\n"
                                  "emptyPager->setNextButtonVisibility(\n"
                                  "    PipsPager::PipsPagerButtonVisibility::Visible);"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent, 10);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto addStateRow = [surface, layout](const QString& title, PipsPager* pager) {
                           auto* row = horizontalGroup(surface, 18);
                           auto* label = makeStatusLabel(row, title);
                           label->setMinimumWidth(label->fontMetrics().horizontalAdvance(
                               QStringLiteral("Disabled")));
                           auto* rowLayout = static_cast<QHBoxLayout*>(row->layout());
                           rowLayout->addWidget(label);
                           rowLayout->addWidget(pager, 0, Qt::AlignCenter);
                           rowLayout->addStretch(1);
                           layout->addWidget(row);
                       };

                       auto* disabledPager = new PipsPager(surface);
                       disabledPager->setNumberOfPages(6);
                       disabledPager->setSelectedPageIndex(2);
                       disabledPager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       disabledPager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       disabledPager->setEnabled(false);
                       refreshPagerFixedSize(disabledPager);

                       auto* emptyPager = new PipsPager(surface);
                       emptyPager->setNumberOfPages(0);
                       emptyPager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       emptyPager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
                       refreshPagerFixedSize(emptyPager);

                       addStateRow(QStringLiteral("Disabled"), disabledPager);
                       addStateRow(QStringLiteral("Empty"), emptyPager);
                       layout->addWidget(makeStatusLabel(
                           surface, QStringLiteral("Empty state accessible description: No pages selected")));
                       return surface;
                   })
    };
}

void configureScrollBarForPreview(ScrollBar* scrollBar,
                                  int maximum,
                                  int pageStep,
                                  int value,
                                  qreal opacity = 1.0)
{
    scrollBar->setRange(0, maximum);
    scrollBar->setPageStep(pageStep);
    {
        const QSignalBlocker blocker(scrollBar);
        scrollBar->setValue(value);
    }
    scrollBar->setOpacity(opacity);
}

class ScrollBarOrientationCard : public ScrollingSampleSurface {
public:
    explicit ScrollBarOrientationCard(QWidget* parent = nullptr)
        : ScrollingSampleSurface(parent)
    {
        auto* layout = static_cast<QVBoxLayout*>(this->layout());

        auto* row = horizontalGroup(this, 22);
        auto* rowLayout = static_cast<QHBoxLayout*>(row->layout());

        auto* horizontalColumn = verticalGroup(row, 10);
        auto* horizontalColumnLayout = static_cast<QVBoxLayout*>(horizontalColumn->layout());

        auto* horizontalBar = new ScrollBar(Qt::Horizontal, horizontalColumn);
        horizontalBar->setFixedWidth(360);
        configureScrollBarForPreview(horizontalBar, 1000, 100, 420);

        auto* status = makeStatusLabel(horizontalColumn, QStringLiteral("Value: 420"));
        status->setMinimumWidth(status->fontMetrics().horizontalAdvance(
            QStringLiteral("Value: 8888")));

        auto* verticalBar = new ScrollBar(Qt::Vertical, row);
        verticalBar->setFixedHeight(178);
        configureScrollBarForPreview(verticalBar, 1000, 100, 420);

        auto updateStatus = [status](int value) {
            status->setText(QStringLiteral("Value: %1").arg(value));
        };

        QObject::connect(horizontalBar, &ScrollBar::valueChanged, row,
                         [verticalBar, updateStatus](int value) {
                             if (verticalBar->value() != value)
                                 verticalBar->setValue(value);
                             updateStatus(value);
                         });
        QObject::connect(verticalBar, &ScrollBar::valueChanged, row,
                         [horizontalBar, updateStatus](int value) {
                             if (horizontalBar->value() != value)
                                 horizontalBar->setValue(value);
                             updateStatus(value);
                         });

        horizontalColumnLayout->addWidget(horizontalBar);
        horizontalColumnLayout->addWidget(status);
        rowLayout->addWidget(horizontalColumn, 0, Qt::AlignTop);
        rowLayout->addWidget(verticalBar, 0, Qt::AlignTop);
        rowLayout->addStretch(1);
        layout->addWidget(row);
    }

    QSize sizeHint() const override
    {
        return QSize(440, 220);
    }
};

class ScrollBarThicknessCard : public ScrollingSampleSurface {
public:
    explicit ScrollBarThicknessCard(QWidget* parent = nullptr)
        : ScrollingSampleSurface(parent)
    {
        auto* layout = static_cast<QVBoxLayout*>(this->layout());

        auto addRow = [this, layout](const QString& labelText,
                                     int thickness,
                                     int value) {
            auto* row = horizontalGroup(this, 14);
            auto* rowLayout = static_cast<QHBoxLayout*>(row->layout());

            auto* label = makeStatusLabel(row, labelText);
            label->setMinimumWidth(label->fontMetrics().horizontalAdvance(
                QStringLiteral("Default 7 px")));

            auto* bar = new ScrollBar(Qt::Horizontal, row);
            bar->setFixedWidth(340);
            bar->setThickness(thickness);
            configureScrollBarForPreview(bar, 1000, 100, value);

            rowLayout->addWidget(label);
            rowLayout->addWidget(bar);
            rowLayout->addStretch(1);
            layout->addWidget(row);
        };

        addRow(QStringLiteral("Thin 6 px"), 6, 220);
        addRow(QStringLiteral("Default 7 px"), 7, 420);
        addRow(QStringLiteral("Large 24 px"), 24, 640);
    }

    QSize sizeHint() const override
    {
        return QSize(470, 170);
    }
};

class ScrollBarOpacityCard : public ScrollingSampleSurface {
public:
    explicit ScrollBarOpacityCard(QWidget* parent = nullptr)
        : ScrollingSampleSurface(parent)
    {
        auto* layout = static_cast<QVBoxLayout*>(this->layout());

        auto addRow = [this, layout](const QString& labelText, qreal opacity) {
            auto* row = horizontalGroup(this, 14);
            auto* rowLayout = static_cast<QHBoxLayout*>(row->layout());

            auto* label = makeStatusLabel(row, labelText);
            label->setMinimumWidth(label->fontMetrics().horizontalAdvance(
                QStringLiteral("Opacity 0.45")));

            auto* bar = new ScrollBar(Qt::Horizontal, row);
            bar->setFixedWidth(340);
            configureScrollBarForPreview(bar, 1000, 100, 420, opacity);

            rowLayout->addWidget(label);
            rowLayout->addWidget(bar);
            rowLayout->addStretch(1);
            layout->addWidget(row);
        };

        addRow(QStringLiteral("Opacity 1.0"), 1.0);
        addRow(QStringLiteral("Opacity 0.45"), 0.45);
    }

    QSize sizeHint() const override
    {
        return QSize(470, 128);
    }
};

QVector<GallerySample> scrollBarSamples()
{
    return {
        makeSample(QStringLiteral("scrollbar-basic"),
                   QStringLiteral("Horizontal and vertical values"),
                   QStringLiteral("Both orientations keep QScrollBar range, page step, and value semantics."),
                   QStringLiteral("auto* horizontalBar = new ScrollBar(Qt::Horizontal, this);\n"
                                  "horizontalBar->setRange(0, 1000);\n"
                                  "horizontalBar->setPageStep(100);\n"
                                  "horizontalBar->setValue(420);\n"
                                  "horizontalBar->setOpacity(1.0);\n"
                                  "\n"
                                  "auto* verticalBar = new ScrollBar(Qt::Vertical, this);\n"
                                  "verticalBar->setRange(0, 1000);\n"
                                  "verticalBar->setPageStep(100);\n"
                                  "verticalBar->setValue(420);\n"
                                  "verticalBar->setOpacity(1.0);\n"
                                  "\n"
                                  "connect(horizontalBar, &ScrollBar::valueChanged,\n"
                                  "        verticalBar, [verticalBar, statusLabel](int value) {\n"
                                  "            verticalBar->setValue(value);\n"
                                  "            statusLabel->setText(QStringLiteral(\"Value: %1\").arg(value));\n"
                                  "        });\n"
                                  "connect(verticalBar, &ScrollBar::valueChanged,\n"
                                  "        horizontalBar, [horizontalBar, statusLabel](int value) {\n"
                                  "            horizontalBar->setValue(value);\n"
                                  "            statusLabel->setText(QStringLiteral(\"Value: %1\").arg(value));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       return new ScrollBarOrientationCard(parent);
                   }),
        makeSample(QStringLiteral("scrollbar-thickness"),
                   QStringLiteral("Thickness"),
                   QStringLiteral("The thickness property changes the occupied cross-axis size without changing scroll range."),
                   QStringLiteral("auto* thinBar = new ScrollBar(Qt::Horizontal, this);\n"
                                  "thinBar->setRange(0, 1000);\n"
                                  "thinBar->setPageStep(100);\n"
                                  "thinBar->setValue(220);\n"
                                  "thinBar->setThickness(6);\n"
                                  "thinBar->setOpacity(1.0);\n"
                                  "\n"
                                  "auto* defaultBar = new ScrollBar(Qt::Horizontal, this);\n"
                                  "defaultBar->setRange(0, 1000);\n"
                                  "defaultBar->setPageStep(100);\n"
                                  "defaultBar->setValue(420);\n"
                                  "defaultBar->setThickness(7);\n"
                                  "defaultBar->setOpacity(1.0);\n"
                                  "\n"
                                  "auto* largeBar = new ScrollBar(Qt::Horizontal, this);\n"
                                  "largeBar->setRange(0, 1000);\n"
                                  "largeBar->setPageStep(100);\n"
                                  "largeBar->setValue(640);\n"
                                  "largeBar->setThickness(24);\n"
                                  "largeBar->setOpacity(1.0);"),
                   [](QWidget* parent) {
                       return new ScrollBarThicknessCard(parent);
                   }),
        makeSample(QStringLiteral("scrollbar-opacity"),
                   QStringLiteral("Pinned opacity"),
                   QStringLiteral("Opacity can keep a demo bar visible or show the same value in a subdued state."),
                   QStringLiteral("auto* visibleBar = new ScrollBar(Qt::Horizontal, this);\n"
                                  "visibleBar->setRange(0, 1000);\n"
                                  "visibleBar->setPageStep(100);\n"
                                  "visibleBar->setValue(420);\n"
                                  "visibleBar->setOpacity(1.0);\n"
                                  "\n"
                                  "auto* subduedBar = new ScrollBar(Qt::Horizontal, this);\n"
                                  "subduedBar->setRange(0, 1000);\n"
                                  "subduedBar->setPageStep(100);\n"
                                  "subduedBar->setValue(420);\n"
                                  "subduedBar->setOpacity(0.45);"),
                   [](QWidget* parent) {
                       return new ScrollBarOpacityCard(parent);
                   })
    };
}

QVector<GallerySample> scrollViewSamples()
{
    return {
        makeSample(QStringLiteral("scroll-view-content-zoom"),
                   QStringLiteral("Content scrolling and zoom"),
                   QStringLiteral("A large content surface can scroll in both directions and zoom within configured bounds."),
                   QStringLiteral("auto* scrollView = new ScrollView(this);\n"
                                  "scrollView->setFixedSize(420, 240);\n"
                                  "scrollView->setWidget(new ScrollViewDemoCanvas(\n"
                                  "    QSize(760, 520), \"Content canvas\"));\n"
                                  "scrollView->setZoomMode(ScrollView::ZoomMode::Enabled);\n"
                                  "scrollView->setMinZoomFactor(0.5);\n"
                                  "scrollView->setMaxZoomFactor(2.0);\n"
                                  "scrollView->setHorizontalScrollBarVisibility(\n"
                                  "    ScrollView::ScrollBarVisibility::Auto);\n"
                                  "scrollView->setVerticalScrollBarVisibility(\n"
                                  "    ScrollView::ScrollBarVisibility::Auto);\n"
                                  "\n"
                                  "connect(zoomInButton, &QPushButton::clicked,\n"
                                  "        scrollView, [scrollView] { scrollView->zoomBy(1.25, true); });\n"
                                  "connect(zoomOutButton, &QPushButton::clicked,\n"
                                  "        scrollView, [scrollView] { scrollView->zoomBy(0.8, true); });\n"
                                  "connect(resetButton, &QPushButton::clicked,\n"
                                  "        scrollView, [scrollView] { scrollView->resetZoom(true); });"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* controls = horizontalGroup(surface, 8);
                       auto* zoomOut = sampleButton(controls, QStringLiteral("Zoom out"));
                       auto* reset = sampleButton(controls, QStringLiteral("Reset"));
                       auto* zoomIn = sampleButton(controls, QStringLiteral("Zoom in"));
                       controls->layout()->addWidget(zoomOut);
                       controls->layout()->addWidget(reset);
                       controls->layout()->addWidget(zoomIn);

                       auto* scrollView = new ScrollView(surface);
                       scrollView->setFixedSize(420, 240);
                       scrollView->setWidget(new ScrollViewDemoCanvas(
                           QSize(760, 520), QStringLiteral("Content canvas"), scrollView));
                       scrollView->setZoomMode(ScrollView::ZoomMode::Enabled);
                       scrollView->setMinZoomFactor(0.5);
                       scrollView->setMaxZoomFactor(2.0);
                       scrollView->setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);
                       scrollView->setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);

                       auto* status = makeStatusLabel(surface, QString());
                       bindScrollViewStatus(scrollView, status);

                       QObject::connect(zoomIn, &QPushButton::clicked, scrollView,
                                        [scrollView] { scrollView->zoomBy(1.25, true); });
                       QObject::connect(zoomOut, &QPushButton::clicked, scrollView,
                                        [scrollView] { scrollView->zoomBy(0.8, true); });
                       QObject::connect(reset, &QPushButton::clicked, scrollView,
                                        [scrollView] { scrollView->resetZoom(true); });

                       layout->addWidget(controls);
                       layout->addWidget(scrollView);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("scroll-view-scrollbar-policies"),
                   QStringLiteral("Scroll modes and bar visibility"),
                   QStringLiteral("Scroll modes gate interaction per axis, while bar visibility controls the chrome."),
                   QStringLiteral("auto applyAutoBars = [scrollView] {\n"
                                  "    scrollView->setHorizontalScrollMode(ScrollView::ScrollMode::Auto);\n"
                                  "    scrollView->setVerticalScrollMode(ScrollView::ScrollMode::Auto);\n"
                                  "    scrollView->setHorizontalScrollBarVisibility(\n"
                                  "        ScrollView::ScrollBarVisibility::Auto);\n"
                                  "    scrollView->setVerticalScrollBarVisibility(\n"
                                  "        ScrollView::ScrollBarVisibility::Auto);\n"
                                  "};\n"
                                  "\n"
                                  "auto applyVisibleBars = [scrollView] {\n"
                                  "    scrollView->setHorizontalScrollMode(ScrollView::ScrollMode::Enabled);\n"
                                  "    scrollView->setVerticalScrollMode(ScrollView::ScrollMode::Enabled);\n"
                                  "    scrollView->setHorizontalScrollBarVisibility(\n"
                                  "        ScrollView::ScrollBarVisibility::Visible);\n"
                                  "    scrollView->setVerticalScrollBarVisibility(\n"
                                  "        ScrollView::ScrollBarVisibility::Visible);\n"
                                  "};\n"
                                  "\n"
                                  "auto applyHiddenHorizontal = [scrollView] {\n"
                                  "    scrollView->setHorizontalScrollMode(ScrollView::ScrollMode::Enabled);\n"
                                  "    scrollView->setHorizontalScrollBarVisibility(\n"
                                  "        ScrollView::ScrollBarVisibility::Hidden);\n"
                                  "};\n"
                                  "\n"
                                  "auto applyVerticalDisabled = [scrollView] {\n"
                                  "    scrollView->setVerticalScrollMode(ScrollView::ScrollMode::Disabled);\n"
                                  "    scrollView->setVerticalScrollBarVisibility(\n"
                                  "        ScrollView::ScrollBarVisibility::Disabled);\n"
                                  "};"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* controls = horizontalGroup(surface, 8);
                       auto* autoBars = sampleButton(controls, QStringLiteral("Auto"));
                       auto* visibleBars = sampleButton(controls, QStringLiteral("Visible"));
                       auto* hiddenHorizontal = sampleButton(controls, QStringLiteral("Hide H"));
                       auto* verticalDisabled = sampleButton(controls, QStringLiteral("Disable V"));
                       controls->layout()->addWidget(autoBars);
                       controls->layout()->addWidget(visibleBars);
                       controls->layout()->addWidget(hiddenHorizontal);
                       controls->layout()->addWidget(verticalDisabled);

                       auto* scrollView = new ScrollView(surface);
                       scrollView->setFixedSize(420, 220);
                       scrollView->setWidget(new ScrollViewDemoCanvas(
                           QSize(680, 420), QStringLiteral("Policy canvas"), scrollView));

                       auto* status = makeStatusLabel(surface, QString());
                       auto updateStatus = [scrollView, status](const QString& policy) {
                           status->setText(QStringLiteral("%1 - %2")
                                               .arg(policy, scrollViewOffsetText(scrollView)));
                       };

                       auto resetButtons = [autoBars, visibleBars, hiddenHorizontal, verticalDisabled]() {
                           setButtonActive(autoBars, false);
                           setButtonActive(visibleBars, false);
                           setButtonActive(hiddenHorizontal, false);
                           setButtonActive(verticalDisabled, false);
                       };

                       auto applyAutoBars = [scrollView, updateStatus, resetButtons, autoBars]() {
                           scrollView->setHorizontalScrollMode(ScrollView::ScrollMode::Auto);
                           scrollView->setVerticalScrollMode(ScrollView::ScrollMode::Auto);
                           scrollView->setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);
                           scrollView->setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);
                           scrollView->scrollTo(80, 80, false);
                           resetButtons();
                           setButtonActive(autoBars, true);
                           updateStatus(QStringLiteral("Auto bars"));
                       };

                       auto applyVisibleBars = [scrollView, updateStatus, resetButtons, visibleBars]() {
                           scrollView->setHorizontalScrollMode(ScrollView::ScrollMode::Enabled);
                           scrollView->setVerticalScrollMode(ScrollView::ScrollMode::Enabled);
                           scrollView->setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Visible);
                           scrollView->setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Visible);
                           scrollView->scrollTo(80, 80, false);
                           resetButtons();
                           setButtonActive(visibleBars, true);
                           updateStatus(QStringLiteral("Visible bars"));
                       };

                       auto applyHiddenHorizontal = [scrollView, updateStatus, resetButtons, hiddenHorizontal]() {
                           scrollView->setHorizontalScrollMode(ScrollView::ScrollMode::Enabled);
                           scrollView->setVerticalScrollMode(ScrollView::ScrollMode::Auto);
                           scrollView->setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Hidden);
                           scrollView->setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);
                           scrollView->scrollTo(160, 80, false);
                           resetButtons();
                           setButtonActive(hiddenHorizontal, true);
                           updateStatus(QStringLiteral("Hidden horizontal bar"));
                       };

                       auto applyVerticalDisabled = [scrollView, updateStatus, resetButtons, verticalDisabled]() {
                           scrollView->setHorizontalScrollMode(ScrollView::ScrollMode::Auto);
                           scrollView->setVerticalScrollMode(ScrollView::ScrollMode::Disabled);
                           scrollView->setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);
                           scrollView->setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Disabled);
                           scrollView->scrollTo(120, 160, false);
                           resetButtons();
                           setButtonActive(verticalDisabled, true);
                           updateStatus(QStringLiteral("Vertical disabled"));
                       };

                       QObject::connect(autoBars, &QPushButton::clicked, surface, applyAutoBars);
                       QObject::connect(visibleBars, &QPushButton::clicked, surface, applyVisibleBars);
                       QObject::connect(hiddenHorizontal, &QPushButton::clicked, surface, applyHiddenHorizontal);
                       QObject::connect(verticalDisabled, &QPushButton::clicked, surface, applyVerticalDisabled);
                       QObject::connect(scrollView, &ScrollView::scrollPositionChanged,
                                        status, [scrollView, status](int, int) {
                                            status->setText(scrollViewOffsetText(scrollView));
                                        });
                       applyAutoBars();

                       layout->addWidget(controls);
                       layout->addWidget(scrollView);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("scroll-view-programmatic-scroll"),
                   QStringLiteral("Programmatic scroll with animation"),
                   QStringLiteral("Buttons call scrollTo or scrollBy and the view animates to the requested offset."),
                   QStringLiteral("auto* scrollView = new ScrollView(this);\n"
                                  "scrollView->setFixedSize(420, 250);\n"
                                  "scrollView->setWidget(makeScrollViewStackContent(scrollView));\n"
                                  "\n"
                                  "connect(topButton, &QPushButton::clicked,\n"
                                  "        scrollView, [scrollView] { scrollView->scrollTo(0, 0, true); });\n"
                                  "connect(centerButton, &QPushButton::clicked,\n"
                                  "        scrollView, [scrollView] {\n"
                                  "            scrollView->scrollTo(scrollView->scrollableWidth() / 2,\n"
                                  "                                 scrollView->scrollableHeight() / 2,\n"
                                  "                                 true);\n"
                                  "        });\n"
                                  "connect(endButton, &QPushButton::clicked,\n"
                                  "        scrollView, [scrollView] {\n"
                                  "            scrollView->scrollTo(scrollView->scrollableWidth(),\n"
                                  "                                 scrollView->scrollableHeight(),\n"
                                  "                                 true);\n"
                                  "        });\n"
                                  "connect(nudgeButton, &QPushButton::clicked,\n"
                                  "        scrollView, [scrollView] { scrollView->scrollBy(0, 120, true); });"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* controls = horizontalGroup(surface, 8);
                       auto* top = sampleButton(controls, QStringLiteral("Top"));
                       auto* center = sampleButton(controls, QStringLiteral("Center"));
                       auto* end = sampleButton(controls, QStringLiteral("End"));
                       auto* nudge = sampleButton(controls, QStringLiteral("Nudge"));
                       controls->layout()->addWidget(top);
                       controls->layout()->addWidget(center);
                       controls->layout()->addWidget(end);
                       controls->layout()->addWidget(nudge);

                       auto* scrollView = new ScrollView(surface);
                       scrollView->setFixedSize(420, 250);
                       scrollView->setWidget(makeScrollViewStackContent(scrollView));

                       auto* status = makeStatusLabel(surface, QString());
                       bindScrollViewStatus(scrollView, status);

                       QObject::connect(top, &QPushButton::clicked, scrollView,
                                        [scrollView] { scrollView->scrollTo(0, 0, true); });
                       QObject::connect(center, &QPushButton::clicked, scrollView, [scrollView] {
                           scrollView->scrollTo(scrollView->scrollableWidth() / 2,
                                                scrollView->scrollableHeight() / 2,
                                                true);
                       });
                       QObject::connect(end, &QPushButton::clicked, scrollView, [scrollView] {
                           scrollView->scrollTo(scrollView->scrollableWidth(),
                                                scrollView->scrollableHeight(),
                                                true);
                       });
                       QObject::connect(nudge, &QPushButton::clicked, scrollView,
                                        [scrollView] { scrollView->scrollBy(0, 120, true); });

                       layout->addWidget(controls);
                       layout->addWidget(scrollView);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("scroll-view-constant-velocity"),
                   QStringLiteral("Constant velocity scrolling"),
                   QStringLiteral("A timer-driven velocity can keep the hosted content moving until stopped."),
                   QStringLiteral("auto* scrollView = new ScrollView(this);\n"
                                  "scrollView->setWidget(makeScrollViewStackContent(scrollView));\n"
                                  "\n"
                                  "auto* velocityTimer = new QTimer(scrollView);\n"
                                  "velocityTimer->setInterval(50);\n"
                                  "velocityTimer->setProperty(\"velocity\", 0);\n"
                                  "connect(velocityTimer, &QTimer::timeout,\n"
                                  "        scrollView, [scrollView, velocityTimer] {\n"
                                  "            scrollView->scrollBy(0,\n"
                                  "                velocityTimer->property(\"velocity\").toInt(),\n"
                                  "                false);\n"
                                  "        });\n"
                                  "\n"
                                  "auto setVelocity = [velocityTimer](int velocity) {\n"
                                  "    velocityTimer->setProperty(\"velocity\", velocity);\n"
                                  "    velocity == 0 ? velocityTimer->stop() : velocityTimer->start();\n"
                                  "};"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* controls = horizontalGroup(surface, 8);
                       auto* up = sampleButton(controls, QStringLiteral("Up"));
                       auto* stop = sampleButton(controls, QStringLiteral("Stop"));
                       auto* down = sampleButton(controls, QStringLiteral("Down"));
                       controls->layout()->addWidget(up);
                       controls->layout()->addWidget(stop);
                       controls->layout()->addWidget(down);

                       auto* scrollView = new ScrollView(surface);
                       scrollView->setFixedSize(420, 250);
                       scrollView->setWidget(makeScrollViewStackContent(scrollView));

                       auto* timer = new QTimer(scrollView);
                       timer->setInterval(50);
                       timer->setProperty("velocity", 0);
                       QObject::connect(timer, &QTimer::timeout, scrollView,
                                        [scrollView, timer] {
                                            scrollView->scrollBy(0, timer->property("velocity").toInt(), false);
                                        });

                       auto* status = makeStatusLabel(surface, QString());
                       auto updateStatus = [scrollView, status, timer]() {
                           status->setText(QStringLiteral("Velocity: %1 px/tick - %2")
                                               .arg(timer->property("velocity").toInt())
                                               .arg(scrollViewOffsetText(scrollView)));
                       };
                       QObject::connect(scrollView, &ScrollView::scrollPositionChanged,
                                        status, [updateStatus](int, int) { updateStatus(); });

                       auto setVelocity = [timer, updateStatus, up, stop, down](int velocity) {
                           timer->setProperty("velocity", velocity);
                           if (velocity == 0)
                               timer->stop();
                           else
                               timer->start();
                           setButtonActive(up, velocity < 0);
                           setButtonActive(stop, velocity == 0);
                           setButtonActive(down, velocity > 0);
                           updateStatus();
                       };

                       QObject::connect(up, &QPushButton::clicked, surface,
                                        [setVelocity] { setVelocity(-12); });
                       QObject::connect(stop, &QPushButton::clicked, surface,
                                        [setVelocity] { setVelocity(0); });
                       QObject::connect(down, &QPushButton::clicked, surface,
                                        [setVelocity] { setVelocity(12); });
                       setVelocity(0);

                       layout->addWidget(controls);
                       layout->addWidget(scrollView);
                       layout->addWidget(status);
                       return surface;
                   }),
        makeSample(QStringLiteral("scroll-view-scroll-chaining"),
                   QStringLiteral("Scroll chaining"),
                   QStringLiteral("Nested scrollers can keep boundary wheel input inside the inner view."),
                   QStringLiteral("auto* defaultView = new ScrollView(this);\n"
                                  "defaultView->setWidget(makeScrollViewStackContent(defaultView));\n"
                                  "defaultView->setScrollChainingEnabled(true);\n"
                                  "\n"
                                  "auto* containedView = new ScrollView(this);\n"
                                  "containedView->setWidget(makeScrollViewStackContent(containedView));\n"
                                  "containedView->setScrollChainingEnabled(false);\n"
                                  "\n"
                                  "// With chaining disabled, a boundary wheel is accepted by\n"
                                  "// the inner view when it still has scrollable range."),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* row = horizontalGroup(surface, 14);
                       auto* defaultColumn = verticalGroup(row, 6);
                       auto* containedColumn = verticalGroup(row, 6);

                       auto* defaultLabel = makeStatusLabel(defaultColumn, QStringLiteral("Default: chaining enabled"));
                       auto* containedLabel = makeStatusLabel(containedColumn, QStringLiteral("Contained: chaining disabled"));

                       auto* defaultView = new ScrollView(defaultColumn);
                       defaultView->setFixedSize(220, 180);
                       defaultView->setWidget(makeScrollViewStackContent(defaultView, QSize(190, 100)));
                       defaultView->setScrollChainingEnabled(true);

                       auto* containedView = new ScrollView(containedColumn);
                       containedView->setFixedSize(220, 180);
                       containedView->setWidget(makeScrollViewStackContent(containedView, QSize(190, 100)));
                       containedView->setScrollChainingEnabled(false);

                       defaultColumn->layout()->addWidget(defaultLabel);
                       defaultColumn->layout()->addWidget(defaultView);
                       containedColumn->layout()->addWidget(containedLabel);
                       containedColumn->layout()->addWidget(containedView);
                       row->layout()->addWidget(defaultColumn);
                       row->layout()->addWidget(containedColumn);

                       layout->addWidget(row);
                       layout->addWidget(makeStatusLabel(
                           surface, QStringLiteral("Scroll each inner view to an edge, then continue the wheel gesture.")));
                       return surface;
                   }),
        makeSample(QStringLiteral("scroll-view-zoom-aware-content"),
                   QStringLiteral("Zoom-aware content"),
                   QStringLiteral("Content implementing ScrollViewZoomAware receives the logical zoom factor separately from widget resizing."),
                   QStringLiteral("class ZoomAwareCanvas : public QWidget,\n"
                                  "                        public ScrollViewZoomAware {\n"
                                  "public:\n"
                                  "    QSizeF scrollViewUnscaledSize() const override\n"
                                  "    {\n"
                                  "        return QSizeF(560, 360);\n"
                                  "    }\n"
                                  "\n"
                                  "    void setScrollViewZoomFactor(qreal factor) override\n"
                                  "    {\n"
                                  "        m_zoomFactor = factor;\n"
                                  "        update();\n"
                                  "    }\n"
                                  "};\n"
                                  "\n"
                                  "auto* scrollView = new ScrollView(this);\n"
                                  "scrollView->setZoomMode(ScrollView::ZoomMode::Enabled);\n"
                                  "scrollView->setWidget(new ZoomAwareCanvas(scrollView));\n"
                                  "scrollView->zoomTo(1.5, true);"),
                   [](QWidget* parent) {
                       auto* surface = pipsSurface(parent);
                       auto* layout = static_cast<QVBoxLayout*>(surface->layout());

                       auto* controls = horizontalGroup(surface, 8);
                       auto* zoom50 = sampleButton(controls, QStringLiteral("50%"));
                       auto* zoom100 = sampleButton(controls, QStringLiteral("100%"));
                       auto* zoom150 = sampleButton(controls, QStringLiteral("150%"));
                       controls->layout()->addWidget(zoom50);
                       controls->layout()->addWidget(zoom100);
                       controls->layout()->addWidget(zoom150);

                       auto* scrollView = new ScrollView(surface);
                       scrollView->setFixedSize(420, 230);
                       scrollView->setZoomMode(ScrollView::ZoomMode::Enabled);
                       scrollView->setMinZoomFactor(0.5);
                       scrollView->setMaxZoomFactor(2.0);
                       scrollView->setWidget(new ScrollViewDemoCanvas(
                           QSize(560, 360), QStringLiteral("Zoom-aware canvas"), scrollView));

                       auto* status = makeStatusLabel(surface, QString());
                       bindScrollViewStatus(scrollView, status);

                       auto setZoom = [scrollView, zoom50, zoom100, zoom150](qreal zoom) {
                           scrollView->zoomTo(zoom, true);
                           setButtonActive(zoom50, qFuzzyCompare(zoom, 0.5));
                           setButtonActive(zoom100, qFuzzyCompare(zoom, 1.0));
                           setButtonActive(zoom150, qFuzzyCompare(zoom, 1.5));
                       };
                       QObject::connect(zoom50, &QPushButton::clicked, surface,
                                        [setZoom] { setZoom(0.5); });
                       QObject::connect(zoom100, &QPushButton::clicked, surface,
                                        [setZoom] { setZoom(1.0); });
                       QObject::connect(zoom150, &QPushButton::clicked, surface,
                                        [setZoom] { setZoom(1.5); });
                       setZoom(1.0);

                       layout->addWidget(controls);
                       layout->addWidget(scrollView);
                       layout->addWidget(status);
                       return surface;
                   })
    };
}

} // namespace

QVector<GallerySample> scrollingSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("annotated-scrollbar"))
        return annotatedScrollBarSamples();
    if (routeId == QStringLiteral("pips-pager"))
        return pipsPagerSamples();
    if (routeId == QStringLiteral("scrollbar"))
        return scrollBarSamples();
    if (routeId == QStringLiteral("scroll-view"))
        return scrollViewSamples();
    return {};
}

} // namespace fluent::gallery

#include "ScrollingSamples.h"

#include <QVBoxLayout>

#include "components/scrolling/AnnotatedScrollBar.h"
#include "components/scrolling/PipsPager.h"
#include "components/scrolling/ScrollBar.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::scrolling::AnnotatedScrollBar;
using fluent::scrolling::AnnotatedScrollBarLabel;
using fluent::scrolling::PipsPager;
using fluent::scrolling::ScrollBar;
using fluent::scrolling::ScrollView;
using fluent::textfields::Label;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    return label;
}

QVector<GallerySample> annotatedScrollBarSamples()
{
    return {
        makeSample(QStringLiteral("annotated-scrollbar-basic"),
                   QStringLiteral("Labeled scroll range"),
                   QStringLiteral("Labels mark notable offsets along the scroll range."),
                   QStringLiteral("auto* bar = new AnnotatedScrollBar(this);\n"
                                  "bar->setRange(0, 1000);\n"
                                  "bar->setPageStep(150);\n"
                                  "bar->addLabel({\"April\", 0});\n"
                                  "bar->addLabel({\"May\", 350});"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 20);
                       auto* bar = new AnnotatedScrollBar(group);
                       bar->setFixedHeight(220);
                       bar->setRange(0, 1000);
                       bar->setPageStep(150);
                       bar->addLabel(AnnotatedScrollBarLabel(QStringLiteral("April"), 0));
                       bar->addLabel(AnnotatedScrollBarLabel(QStringLiteral("May"), 350));
                       bar->addLabel(AnnotatedScrollBarLabel(QStringLiteral("June"), 700));
                       Label* status = makeStatusLabel(group, QStringLiteral("Offset: 0"));
                       QObject::connect(bar, &AnnotatedScrollBar::valueChanged,
                                        status, [status](int value) {
                                            status->setText(QStringLiteral("Offset: %1").arg(value));
                                        });
                       group->layout()->addWidget(bar);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

QVector<GallerySample> pipsPagerSamples()
{
    return {
        makeSample(QStringLiteral("pips-pager-basic"),
                   QStringLiteral("Dot pagination"),
                   QStringLiteral("Click a pip or the chevrons to switch pages."),
                   QStringLiteral("auto* pager = new PipsPager(this);\n"
                                  "pager->setNumberOfPages(5);\n"
                                  "connect(pager, &PipsPager::selectedPageIndexChanged,\n"
                                  "        this, [](int index) { /* show page */ });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* pager = new PipsPager(group);
                       pager->setNumberOfPages(5);
                       Label* status = makeStatusLabel(group, QStringLiteral("Page 1 of 5"));
                       QObject::connect(pager, &PipsPager::selectedPageIndexChanged,
                                        status, [status](int index) {
                                            status->setText(QStringLiteral("Page %1 of 5").arg(index + 1));
                                        });
                       group->layout()->addWidget(pager);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

QVector<GallerySample> scrollBarSamples()
{
    return {
        makeSample(QStringLiteral("scrollbar-basic"),
                   QStringLiteral("Fluent ScrollBar"),
                   QStringLiteral("The rail expands on hover; drag the thumb to change the value."),
                   QStringLiteral("auto* scrollBar = new ScrollBar(Qt::Horizontal, this);\n"
                                  "scrollBar->setRange(0, 100);\n"
                                  "scrollBar->setValue(40);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 12);
                       auto* scrollBar = new ScrollBar(Qt::Horizontal, group);
                       scrollBar->setRange(0, 100);
                       scrollBar->setValue(40);
                       scrollBar->setFixedWidth(300);
                       Label* status = makeStatusLabel(group, QStringLiteral("Value: 40"));
                       QObject::connect(scrollBar, &ScrollBar::valueChanged,
                                        status, [status](int value) {
                                            status->setText(QStringLiteral("Value: %1").arg(value));
                                        });
                       group->layout()->addWidget(scrollBar);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

QVector<GallerySample> scrollViewSamples()
{
    return {
        makeSample(QStringLiteral("scroll-view-basic"),
                   QStringLiteral("Scrollable viewport"),
                   QStringLiteral("Content larger than the viewport scrolls with Fluent scrollbars."),
                   QStringLiteral("auto* scrollView = new ScrollView(this);\n"
                                  "scrollView->setWidget(largeContent);"),
                   [](QWidget* parent) {
                       auto* scrollView = new ScrollView(parent);
                       scrollView->setFixedSize(420, 200);

                       auto* content = new QWidget(scrollView);
                       content->setFixedSize(620, 420);
                       content->setStyleSheet(QStringLiteral(
                           "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                           " stop:0 #DCE8F8, stop:0.5 #DFF3E3, stop:1 #FBE8DC);"));
                       auto* layout = new QVBoxLayout(content);
                       layout->setContentsMargins(16, 16, 16, 16);
                       layout->setSpacing(10);
                       for (int line = 1; line <= 12; ++line) {
                           auto* label = new Label(QStringLiteral("Scrollable line %1").arg(line), content);
                           label->setFluentTypography(Typography::FontRole::Body);
                           label->setStyleSheet(QStringLiteral("color: #444; background: transparent;"));
                           layout->addWidget(label);
                       }
                       layout->addStretch(1);
                       scrollView->setWidget(content);
                       return scrollView;
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

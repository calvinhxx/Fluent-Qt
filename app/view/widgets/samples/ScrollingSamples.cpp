#include "ScrollingSamples.h"

#include <QLabel>
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
using samples::gradientPixmap;
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
                       // Reserve the widest offset so live drags never reflow the row.
                       // zh_CN: 预留最大偏移宽度，拖动时的实时更新不再回流整行。
                       status->setMinimumWidth(status->fontMetrics().horizontalAdvance(
                           QStringLiteral("Offset: 8888")));
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
                   QStringLiteral("Dot pagination with paged content"),
                   QStringLiteral("Click a pip or the chevrons; the picture above follows the selected page."),
                   QStringLiteral("auto* pager = new PipsPager(this);\n"
                                  "pager->setNumberOfPages(5);\n"
                                  "connect(pager, &PipsPager::selectedPageIndexChanged,\n"
                                  "        this, [](int index) { /* show page */ });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);

                       // Pre-render one picture per page so pips drive real content.
                       // zh_CN: 为每页预先渲染一张图片，让圆点真正驱动内容切换。
                       const QSize pageSize(320, 150);
                       struct Page { QString caption; QColor from; QColor to; };
                       const QVector<Page> pages{
                           {QStringLiteral("Sunrise"), QColor(0xF7, 0x97, 0x5B), QColor(0xF2, 0xC9, 0x4C)},
                           {QStringLiteral("Ocean"), QColor(0x1E, 0x6F, 0xD9), QColor(0x6F, 0xD1, 0xF2)},
                           {QStringLiteral("Forest"), QColor(0x2F, 0x9E, 0x44), QColor(0xA9, 0xE3, 0x4B)},
                           {QStringLiteral("Dusk"), QColor(0x6B, 0x4F, 0xA2), QColor(0xC2, 0x6F, 0xB8)},
                           {QStringLiteral("Desert"), QColor(0xC8, 0x6B, 0x2D), QColor(0xE8, 0xC0, 0x6E)}};
                       QVector<QPixmap> pictures;
                       for (int i = 0; i < pages.size(); ++i) {
                           const Page& page = pages.at(i);
                           pictures.append(gradientPixmap(
                               pageSize, page.from, page.to,
                               QStringLiteral("%1 — page %2 of %3")
                                   .arg(page.caption).arg(i + 1).arg(pages.size())));
                       }

                       auto* picture = new QLabel(group);
                       picture->setPixmap(pictures.first());
                       picture->setFixedSize(pageSize);

                       auto* pager = new PipsPager(group);
                       pager->setNumberOfPages(pages.size());
                       QObject::connect(pager, &PipsPager::selectedPageIndexChanged,
                                        picture, [picture, pictures](int index) {
                                            if (index >= 0 && index < pictures.size())
                                                picture->setPixmap(pictures.at(index));
                                        });
                       group->layout()->addWidget(picture);
                       group->layout()->addWidget(pager);
                       static_cast<QVBoxLayout*>(group->layout())
                           ->setAlignment(pager, Qt::AlignHCenter);
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
                       status->setMinimumWidth(status->fontMetrics().horizontalAdvance(
                           QStringLiteral("Value: 888")));
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

                       // A picture larger than the viewport exercises both scrollbars.
                       // zh_CN: 比视口更大的图片同时演示横向与纵向滚动条。
                       auto* content = new QLabel(scrollView);
                       const QSize pictureSize(680, 440);
                       content->setPixmap(gradientPixmap(
                           pictureSize, QColor(0x1E, 0x6F, 0xD9), QColor(0xA9, 0xE3, 0x4B),
                           QStringLiteral("Pan around — the picture is larger than the viewport")));
                       content->setFixedSize(pictureSize);
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

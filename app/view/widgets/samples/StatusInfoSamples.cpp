#include "StatusInfoSamples.h"

#include <QEvent>
#include <QPoint>

#include "components/basicinput/Button.h"
#include "components/status_info/InfoBadge.h"
#include "components/status_info/InfoBar.h"
#include "components/status_info/ProgressBar.h"
#include "components/status_info/ProgressRing.h"
#include "components/status_info/ToolTip.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::status_info::InfoBadge;
using fluent::status_info::InfoBar;
using fluent::status_info::ProgressBar;
using fluent::status_info::ProgressRing;
using fluent::status_info::ToolTip;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

/**
 * @brief Shows a Fluent ToolTip above the watched widget while hovered.
 * zh_CN: 悬停时在被监听控件上方显示 Fluent ToolTip。
 */
class HoverToolTipController : public QObject {
public:
    HoverToolTipController(QWidget* target, const QString& text)
        : QObject(target)
        , m_target(target)
    {
        m_toolTip = new ToolTip(target);
        m_toolTip->setText(text);
        m_toolTip->hide();
        target->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_target) {
            if (event->type() == QEvent::Enter) {
                // Reparent at hover time: the preview is built before the page joins the
                // window hierarchy, so the final top-level is only known now.
                // zh_CN: 悬停时再挂到顶层：预览构建早于页面入窗，顶层窗口此刻才确定。
                QWidget* topLevel = m_target->window();
                if (m_toolTip->parentWidget() != topLevel)
                    m_toolTip->setParent(topLevel);
                const QSize tipSize = m_toolTip->sizeHint();
                const QPoint anchor = m_target->mapTo(
                    topLevel,
                    QPoint(m_target->width() / 2 - tipSize.width() / 2, -tipSize.height() - 4));
                m_toolTip->move(anchor);
                m_toolTip->show();
            } else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
                m_toolTip->hide();
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QWidget* m_target = nullptr;
    ToolTip* m_toolTip = nullptr;
};

QVector<GallerySample> infoBadgeSamples()
{
    return {
        makeSample(QStringLiteral("info-badge-basic"),
                   QStringLiteral("Badge variants"),
                   QStringLiteral("Value, dot, and status icon badges decorate other UI."),
                   QStringLiteral("auto* badge = new InfoBadge(this);\n"
                                  "badge->setValue(5);\n"
                                  "badge->setStatus(InfoBadge::InfoBadgeStatus::Critical);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 24);

                       auto* valueBadge = new InfoBadge(group);
                       valueBadge->setValue(5);

                       auto* dotBadge = new InfoBadge(group);
                       dotBadge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Dot);

                       auto* successBadge = new InfoBadge(group);
                       successBadge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
                       successBadge->setStatus(InfoBadge::InfoBadgeStatus::Success);

                       auto* criticalBadge = new InfoBadge(group);
                       criticalBadge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
                       criticalBadge->setStatus(InfoBadge::InfoBadgeStatus::Critical);

                       group->layout()->addWidget(valueBadge);
                       group->layout()->addWidget(dotBadge);
                       group->layout()->addWidget(successBadge);
                       group->layout()->addWidget(criticalBadge);
                       return group;
                   })
    };
}

QVector<GallerySample> infoBarSamples()
{
    return {
        makeSample(QStringLiteral("info-bar-severities"),
                   QStringLiteral("InfoBar severities"),
                   QStringLiteral("Severity drives the icon and accent color; bars can be closable."),
                   QStringLiteral("auto* infoBar = new InfoBar(this);\n"
                                  "infoBar->setSeverity(InfoBar::Success);\n"
                                  "infoBar->setTitle(\"Success\");\n"
                                  "infoBar->setMessage(\"All changes were saved.\");\n"
                                  "infoBar->setIsOpen(true);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 12);

                       auto* infoBar = new InfoBar(group);
                       infoBar->setSeverity(InfoBar::Informational);
                       infoBar->setTitle(QStringLiteral("Heads up"));
                       infoBar->setMessage(QStringLiteral("A new version is available for download."));
                       infoBar->setPreferredWidth(460);
                       infoBar->setIsOpen(true);

                       auto* successBar = new InfoBar(group);
                       successBar->setSeverity(InfoBar::Success);
                       successBar->setTitle(QStringLiteral("Success"));
                       successBar->setMessage(QStringLiteral("All changes were saved."));
                       successBar->setPreferredWidth(460);
                       successBar->setIsOpen(true);

                       auto* errorBar = new InfoBar(group);
                       errorBar->setSeverity(InfoBar::Error);
                       errorBar->setTitle(QStringLiteral("Error"));
                       errorBar->setMessage(QStringLiteral("The document failed to upload."));
                       errorBar->setPreferredWidth(460);
                       errorBar->setIsOpen(true);

                       group->layout()->addWidget(infoBar);
                       group->layout()->addWidget(successBar);
                       group->layout()->addWidget(errorBar);
                       return group;
                   })
    };
}

QVector<GallerySample> progressBarSamples()
{
    return {
        makeSample(QStringLiteral("progress-bar-determinate"),
                   QStringLiteral("Determinate ProgressBar"),
                   QStringLiteral("Shows a known fraction of completed work."),
                   QStringLiteral("auto* progressBar = new ProgressBar(this);\n"
                                  "progressBar->setRange(0, 100);\n"
                                  "progressBar->setValue(50);"),
                   [](QWidget* parent) {
                       auto* progressBar = new ProgressBar(parent);
                       progressBar->setRange(0, 100);
                       progressBar->setValue(50);
                       progressBar->setBarWidth(300);
                       return progressBar;
                   }),
        makeSample(QStringLiteral("progress-bar-indeterminate"),
                   QStringLiteral("Indeterminate ProgressBar"),
                   QStringLiteral("Shows ongoing work without a known duration."),
                   QStringLiteral("auto* progressBar = new ProgressBar(this);\n"
                                  "progressBar->setIsIndeterminate(true);"),
                   [](QWidget* parent) {
                       auto* progressBar = new ProgressBar(parent);
                       progressBar->setIsIndeterminate(true);
                       progressBar->setBarWidth(300);
                       return progressBar;
                   })
    };
}

QVector<GallerySample> progressRingSamples()
{
    return {
        makeSample(QStringLiteral("progress-ring-indeterminate"),
                   QStringLiteral("Indeterminate ProgressRing"),
                   QStringLiteral("The ring spins while work continues."),
                   QStringLiteral("auto* ring = new ProgressRing(this);\n"
                                  "ring->setIsIndeterminate(true);\n"
                                  "ring->setIsActive(true);"),
                   [](QWidget* parent) {
                       auto* ring = new ProgressRing(parent);
                       ring->setIsIndeterminate(true);
                       ring->setIsActive(true);
                       return ring;
                   }),
        makeSample(QStringLiteral("progress-ring-determinate"),
                   QStringLiteral("Determinate ProgressRing"),
                   QStringLiteral("The arc length reflects the completed fraction."),
                   QStringLiteral("auto* ring = new ProgressRing(this);\n"
                                  "ring->setRange(0, 100);\n"
                                  "ring->setValue(75);"),
                   [](QWidget* parent) {
                       auto* ring = new ProgressRing(parent);
                       ring->setIsIndeterminate(false);
                       ring->setRange(0, 100);
                       ring->setValue(75);
                       return ring;
                   })
    };
}

QVector<GallerySample> toolTipSamples()
{
    return {
        makeSample(QStringLiteral("tooltip-basic"),
                   QStringLiteral("ToolTip on hover"),
                   QStringLiteral("Hover the button to reveal the Fluent tooltip."),
                   QStringLiteral("auto* toolTip = new ToolTip(window());\n"
                                  "toolTip->setText(\"More details on hover\");\n"
                                  "// Show/hide from the target's Enter/Leave events."),
                   [](QWidget* parent) {
                       auto* button = new Button(QStringLiteral("Hover me"), parent);
                       new HoverToolTipController(button, QStringLiteral("More details on hover"));
                       return button;
                   })
    };
}

} // namespace

QVector<GallerySample> statusInfoSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("info-badge"))
        return infoBadgeSamples();
    if (routeId == QStringLiteral("info-bar"))
        return infoBarSamples();
    if (routeId == QStringLiteral("progress-bar"))
        return progressBarSamples();
    if (routeId == QStringLiteral("progress-ring"))
        return progressRingSamples();
    if (routeId == QStringLiteral("tooltip"))
        return toolTipSamples();
    return {};
}

} // namespace fluent::gallery

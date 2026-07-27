#include "components/layout/Expander.h"

#include <QHBoxLayout>
#include <QLayout>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QVariantAnimation>
#include <QtGlobal>

#include "components/basicinput/Button.h"
#include "components/foundation/FontIcon.h"
#include "components/layout/Divider.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

namespace fluent::layout {
namespace {

QScrollArea* enclosingScrollArea(QWidget* widget)
{
    for (QWidget* ancestor = widget ? widget->parentWidget() : nullptr;
         ancestor;
         ancestor = ancestor->parentWidget()) {
        if (auto* scrollArea = qobject_cast<QScrollArea*>(ancestor))
            return scrollArea;
    }
    return nullptr;
}

} // namespace

Expander::Expander(QWidget* parent)
    : Card(parent)
{
    setObjectName(QStringLiteral("fluentExpander"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFixedHeight(HeaderHeight);

    m_headerButton = new basicinput::Button(this);
    m_headerButton->setObjectName(QStringLiteral("fluentExpanderHeader"));
    m_headerButton->setFluentStyle(basicinput::Button::Subtle);
    m_headerButton->setFocusVisual(true);
    m_headerButton->setFixedHeight(HeaderHeight);

    auto* headerLayout = new QHBoxLayout(m_headerButton);
    headerLayout->setContentsMargins(16, 0, 14, 0);
    headerLayout->setSpacing(8);

    m_headerLabel = new textfields::Label(m_headerButton);
    m_headerLabel->setObjectName(QStringLiteral("fluentExpanderHeaderText"));
    m_headerLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_headerLabel->setFluentTypography(Typography::FontRole::BodyStrong);
    // Primary forces a self stylesheet color so ancestor Gallery/card style
    // sheets cannot leave the header stuck on a Light WindowText palette.
    // zh_CN: Primary 走标签自身样式表上色，避免祖先 Gallery/卡片样式表
    // 把标题卡在浅色 WindowText palette 上。
    m_headerLabel->setTextColorRole(
        textfields::Label::TextColorRole::Primary);
    m_headerLabel->setTextElideMode(Qt::ElideRight);

    m_chevron = new FontIcon(Typography::Icons::ChevronDown, m_headerButton);
    m_chevron->setObjectName(QStringLiteral("fluentExpanderChevron"));
    m_chevron->setIconSize(Typography::IconSize::Compact);
    m_chevron->setFixedSize(22, 22);

    headerLayout->addWidget(m_headerLabel, 1, Qt::AlignVCenter);
    headerLayout->addWidget(m_chevron, 0, Qt::AlignVCenter);

    m_divider = new Divider(this);
    m_divider->setObjectName(QStringLiteral("fluentExpanderDivider"));
    m_divider->hide();

    m_clip = new QWidget(this);
    m_clip->setObjectName(QStringLiteral("fluentExpanderClip"));
    m_clip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_clip->setFixedHeight(0);

    connect(m_headerButton, &basicinput::Button::clicked,
            this, &Expander::toggleExpanded);

    m_animation = new QVariantAnimation(this);
    connect(m_animation, &QVariantAnimation::valueChanged,
            this, [this](const QVariant& value) {
                applyFraction(value.toReal());
            });
    connect(m_animation, &QVariantAnimation::finished, this, [this]() {
        applyFraction(m_expanded ? 1.0 : 0.0);
        emit expansionTransitionFinished(m_expanded);
        finishViewportTransition();
    });

    updateChildGeometry();
}

Expander::~Expander()
{
    clearViewportAnchor();
    releaseContent(false, true);
}

void Expander::setHeaderText(const QString& text)
{
    if (m_headerText == text)
        return;

    m_headerText = text;
    m_headerLabel->setText(m_headerText);
    m_headerButton->setAccessibleName(m_headerText);
    updateGeometry();
    emit headerTextChanged(m_headerText);
}

void Expander::setContentWidget(QWidget* widget)
{
    setContentWidget(widget, WidgetOwnership::Borrowed);
}

bool Expander::setContentWidget(QWidget* widget, WidgetOwnership ownership)
{
    if (widget == this || (widget && widget->isAncestorOf(this)))
        return false;

    if (m_contentWidget == widget) {
        if (m_contentOwnership == ownership)
            return true;
        m_contentOwnership = ownership;
        emit contentOwnershipChanged(m_contentOwnership);
        return true;
    }

    releaseContent(true, true);
    m_contentWidget = widget;
    m_contentOriginalParent = widget ? widget->parentWidget() : nullptr;
    const WidgetOwnership previousOwnership = m_contentOwnership;
    m_contentOwnership = widget ? ownership : WidgetOwnership::Borrowed;

    if (widget) {
        widget->setParent(m_clip);
        widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        widget->show();
        m_contentDestroyedConnection = connect(
            widget, &QObject::destroyed, this, [this]() {
                handleContentDestroyed();
            });
    }

    m_contentTargetHeight = m_expanded ? naturalContentHeight() : 0;
    applyFraction(m_expanded ? 1.0 : 0.0);
    emit contentWidgetChanged(widget);
    if (previousOwnership != m_contentOwnership)
        emit contentOwnershipChanged(m_contentOwnership);
    return true;
}

QWidget* Expander::takeContentWidget()
{
    QWidget* content = m_contentWidget.data();
    if (!content)
        return nullptr;

    QObject::disconnect(m_contentDestroyedConnection);
    m_contentDestroyedConnection = QMetaObject::Connection();
    m_contentWidget = nullptr;
    m_contentOriginalParent = nullptr;
    const WidgetOwnership previousOwnership = m_contentOwnership;
    m_contentOwnership = WidgetOwnership::Borrowed;
    content->hide();
    content->setParent(nullptr);
    m_contentTargetHeight = 0;
    applyFraction(0.0);
    emit contentWidgetChanged(nullptr);
    if (previousOwnership != m_contentOwnership)
        emit contentOwnershipChanged(m_contentOwnership);
    return content;
}

void Expander::setExpanded(bool expanded)
{
    setExpandedAnimated(expanded, m_animationEnabled);
}

void Expander::setExpandedAnimated(bool expanded, bool animated)
{
    if (m_expanded == expanded)
        return;

    m_expanded = expanded;
    beginViewportTransition();
    emit expandedChanged(m_expanded);
    emit expansionTransitionStarted(m_expanded);

    m_animation->stop();
    if (expanded || m_contentTargetHeight <= 0)
        m_contentTargetHeight = naturalContentHeight();
    applyFraction(m_fraction);

    const qreal target = expanded ? 1.0 : 0.0;
    if (!animated) {
        applyFraction(target);
        emit expansionTransitionFinished(m_expanded);
        finishViewportTransition();
        return;
    }

    const auto motion = themeAnimation();
    const qreal distance = qAbs(target - m_fraction);
    const int duration = qRound(motion.fast
                                + (motion.normal - motion.fast) * distance);
    {
        const QSignalBlocker blocker(m_animation);
        m_animation->setStartValue(m_fraction);
        m_animation->setEndValue(target);
        m_animation->setDuration(duration);
        m_animation->setEasingCurve(motion.standard);
        m_animation->setCurrentTime(0);
    }
    m_animation->start();
}

void Expander::toggleExpanded()
{
    setExpanded(!m_expanded);
}

void Expander::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled)
        return;

    m_animationEnabled = enabled;
    emit animationEnabledChanged(m_animationEnabled);
}

QSize Expander::sizeHint() const
{
    int width = 240;
    if (m_headerLabel)
        width = qMax(width, m_headerLabel->sizeHint().width() + 60);
    if (m_contentWidget)
        width = qMax(width, m_contentWidget->sizeHint().width());
    return QSize(width, totalHeightForContent(currentContentHeight()));
}

QSize Expander::minimumSizeHint() const
{
    return QSize(0, totalHeightForContent(currentContentHeight()));
}

void Expander::onThemeUpdated()
{
    Card::onThemeUpdated();
    if (m_headerButton)
        m_headerButton->onThemeUpdated();
    if (m_headerLabel)
        m_headerLabel->onThemeUpdated();
    if (m_chevron)
        m_chevron->onThemeUpdated();
    if (m_divider)
        m_divider->onThemeUpdated();
}

void Expander::resizeEvent(QResizeEvent* event)
{
    Card::resizeEvent(event);
    if (event && event->oldSize().width() != event->size().width()
        && m_contentWidget) {
        m_contentTargetHeight = naturalContentHeight();
        applyFraction(m_fraction);
    }
    updateChildGeometry();
}

int Expander::naturalContentHeight() const
{
    if (!m_contentWidget)
        return 0;

    QWidget* content = m_contentWidget.data();
    const int availableWidth = qMax(0, width());
    content->ensurePolished();
    content->resize(availableWidth, qMax(content->height(), 1));
    if (QLayout* contentLayout = content->layout()) {
        contentLayout->invalidate();
        contentLayout->activate();
    }

    int height = content->sizeHint().height();
    if (content->hasHeightForWidth())
        height = content->heightForWidth(availableWidth);
    else if (content->layout() && content->layout()->hasHeightForWidth())
        height = content->layout()->totalHeightForWidth(availableWidth);
    return qMax(0, height);
}

int Expander::currentContentHeight() const
{
    return qRound(m_contentTargetHeight * m_fraction);
}

int Expander::totalHeightForContent(int contentHeight) const
{
    return HeaderHeight
        + (contentHeight > 0 ? DividerExtent + contentHeight : 0);
}

void Expander::updateChildGeometry()
{
    if (!m_headerButton || !m_divider || !m_clip)
        return;

    const int contentHeight = currentContentHeight();
    m_headerButton->setGeometry(0, 0, width(), HeaderHeight);
    m_divider->setGeometry(0, HeaderHeight, width(), DividerExtent);
    m_divider->setVisible(contentHeight > 0);
    m_clip->setGeometry(
        0, HeaderHeight + DividerExtent, width(), contentHeight);
    if (m_contentWidget) {
        m_contentWidget->setGeometry(
            0, 0, width(), qMax(0, m_contentTargetHeight));
    }
}

void Expander::applyFraction(qreal fraction)
{
    m_fraction = qBound<qreal>(0.0, fraction, 1.0);
    const int contentHeight = currentContentHeight();
    const int totalHeight = totalHeightForContent(contentHeight);
    m_clip->setFixedHeight(contentHeight);
    if (minimumHeight() != totalHeight || maximumHeight() != totalHeight)
        setFixedHeight(totalHeight);
    updateChildGeometry();
    updateGeometry();
    if (m_chevron)
        m_chevron->setRotation(180.0 * m_fraction);
    if (m_lastEmittedLayoutHeight != totalHeight) {
        m_lastEmittedLayoutHeight = totalHeight;
        emit layoutHeightChanged(totalHeight);
        synchronizeViewportLayout();
    }
}

void Expander::releaseContent(bool deleteOwned, bool restoreParent)
{
    QWidget* content = m_contentWidget.data();
    QObject::disconnect(m_contentDestroyedConnection);
    m_contentDestroyedConnection = QMetaObject::Connection();
    if (!content) {
        m_contentWidget = nullptr;
        m_contentOriginalParent = nullptr;
        m_contentOwnership = WidgetOwnership::Borrowed;
        return;
    }

    const WidgetOwnership ownership = m_contentOwnership;
    QPointer<QWidget> originalParent = m_contentOriginalParent;
    m_contentWidget = nullptr;
    m_contentOriginalParent = nullptr;
    m_contentOwnership = WidgetOwnership::Borrowed;

    content->hide();
    if (ownership == WidgetOwnership::Owned) {
        if (deleteOwned)
            delete content;
        return;
    }
    if (restoreParent && ownership == WidgetOwnership::Reparented)
        content->setParent(originalParent.data());
    else
        content->setParent(nullptr);
}

void Expander::handleContentDestroyed()
{
    m_contentDestroyedConnection = QMetaObject::Connection();
    m_contentWidget = nullptr;
    m_contentOriginalParent = nullptr;
    const WidgetOwnership previousOwnership = m_contentOwnership;
    m_contentOwnership = WidgetOwnership::Borrowed;
    m_contentTargetHeight = 0;
    applyFraction(0.0);
    emit contentWidgetChanged(nullptr);
    if (previousOwnership != m_contentOwnership)
        emit contentOwnershipChanged(m_contentOwnership);
}

void Expander::beginViewportTransition()
{
    ++m_viewportTransitionGeneration;
    clearViewportAnchor();

    m_transitionScrollArea = enclosingScrollArea(this);
    m_transitionScrollBar = m_transitionScrollArea
        ? m_transitionScrollArea->verticalScrollBar()
        : nullptr;
    if (!m_transitionScrollArea || !m_transitionScrollArea->viewport()
        || !m_transitionScrollBar || !m_headerButton) {
        return;
    }

    m_viewportTransitionActive = true;
    m_anchorViewportY = m_transitionScrollArea->viewport()->mapFromGlobal(
        m_headerButton->mapToGlobal(QPoint(0, 0))).y();

    m_scrollRangeConnection = connect(
        m_transitionScrollBar, &QScrollBar::rangeChanged,
        this, [this]() { restoreViewportAnchor(); });
    m_scrollValueConnection = connect(
        m_transitionScrollBar, &QScrollBar::valueChanged,
        this, [this]() { restoreViewportAnchor(); });
}

void Expander::finishViewportTransition()
{
    synchronizeViewportLayout();
    const quint64 generation = m_viewportTransitionGeneration;
    QTimer::singleShot(0, this, [this, generation]() {
        if (!m_viewportTransitionActive
            || generation != m_viewportTransitionGeneration) {
            return;
        }
        synchronizeViewportLayout();
        clearViewportAnchor();
    });
}

void Expander::synchronizeViewportLayout()
{
    if (!m_viewportTransitionActive || !m_transitionScrollArea
        || !m_transitionScrollArea->viewport()) {
        return;
    }

    QWidget* scrollContent = m_transitionScrollArea->widget();
    if (!scrollContent)
        return;

    for (QWidget* ancestor = parentWidget();
         ancestor && ancestor != scrollContent;
         ancestor = ancestor->parentWidget()) {
        if (QLayout* ancestorLayout = ancestor->layout()) {
            ancestorLayout->invalidate();
            ancestorLayout->activate();
        }
    }

    QLayout* pageLayout = scrollContent->layout();
    if (pageLayout) {
        pageLayout->invalidate();
        const int contentHeight = qMax(
            m_transitionScrollArea->viewport()->height(),
            pageLayout->minimumSize().height());
        if (scrollContent->height() != contentHeight)
            scrollContent->resize(scrollContent->width(), contentHeight);
        pageLayout->activate();
    }
    restoreViewportAnchor();
}

void Expander::restoreViewportAnchor()
{
    if (!m_viewportTransitionActive || m_restoringViewportAnchor
        || !m_transitionScrollArea || !m_transitionScrollArea->viewport()
        || !m_transitionScrollBar || !m_headerButton) {
        return;
    }

    QWidget* scrollContent = m_transitionScrollArea->widget();
    if (!scrollContent)
        return;

    const int anchorContentY =
        m_headerButton->mapTo(scrollContent, QPoint(0, 0)).y();
    const int desiredValue = qBound(
        m_transitionScrollBar->minimum(),
        anchorContentY - m_anchorViewportY,
        m_transitionScrollBar->maximum());
    if (m_transitionScrollBar->value() == desiredValue)
        return;

    m_restoringViewportAnchor = true;
    m_transitionScrollBar->setValue(desiredValue);
    m_restoringViewportAnchor = false;
}

void Expander::clearViewportAnchor()
{
    QObject::disconnect(m_scrollRangeConnection);
    QObject::disconnect(m_scrollValueConnection);
    m_scrollRangeConnection = QMetaObject::Connection();
    m_scrollValueConnection = QMetaObject::Connection();
    m_viewportTransitionActive = false;
    m_restoringViewportAnchor = false;
    m_transitionScrollArea = nullptr;
    m_transitionScrollBar = nullptr;
}

} // namespace fluent::layout

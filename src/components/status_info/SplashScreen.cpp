#include "SplashScreen.h"

#include <QApplication>
#include <QAccessibleWidget>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHideEvent>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QtMath>

#include "components/foundation/MotionPolicy.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/status_info/ProgressBar.h"
#include "components/status_info/ProgressRing.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

namespace fluent::status_info {
namespace {
class SplashAccessible final : public QAccessibleWidget {
public:
    explicit SplashAccessible(SplashScreen* splash) : QAccessibleWidget(splash, QAccessible::Pane)
    {}
    QAccessible::State state() const override
    {
        auto result = QAccessibleWidget::state();
        result.busy = widget()->isVisible();
        return result;
    }
};
QAccessibleInterface* splashAccessibleFactory(const QString&, QObject* object)
{
    if (auto* splash = qobject_cast<SplashScreen*>(object))
        return new SplashAccessible(splash);
    return nullptr;
}

qreal smoothStep(qreal value)
{
    value = qBound<qreal>(0.0, value, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

void paintGlow(QPainter& painter, const QPointF& center, qreal radius, QColor color, qreal opacity)
{
    if (radius <= 0.0 || opacity <= 0.0)
        return;
    color.setAlphaF(color.alphaF() * opacity);
    QRadialGradient glow(center, radius);
    glow.setColorAt(0.0, color);
    color.setAlpha(0);
    glow.setColorAt(1.0, color);
    painter.fillRect(QRectF(center.x() - radius, center.y() - radius, radius * 2, radius * 2),
                     glow);
}
} // namespace

// Keep Label's text, elision and accessibility; only the private brand text needs paint opacity.
// zh_CN: 保留 Label 的文本、省略和辅助功能；仅私有品牌文字使用绘制透明度。
class SplashScreen::BrandLabel final : public textfields::Label {
public:
    using Label::Label;
    void setPaintOpacity(qreal opacity)
    {
        if (qFuzzyCompare(m_opacity, opacity))
            return;
        m_opacity = opacity;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setOpacity(m_opacity);
        painter.setPen(textColorRole() == TextColorRole::Primary ? themeColors().textPrimary
                                                                 : themeColors().textSecondary);
        painter.drawText(contentsRect(), alignment() | Qt::TextSingleLine, QLabel::text());
    }

private:
    qreal m_opacity = 1.0;
};

// This child only paints the travelling icon. Its window-wide coordinate system lets an
// embedded cover reach a title-bar sibling without taking ownership of the application's chrome.
// zh_CN: 私有子控件仅绘制移动图标，以窗口坐标连接内容遮罩和标题栏，不接管应用 chrome。
class SplashScreen::LogoTransition final : public QWidget {
public:
    explicit LogoTransition(SplashScreen* owner) : QWidget(owner->window()), m_owner(owner)
    {
        setObjectName(QStringLiteral("splashLogoTransition"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        advance(0.0);
    }

    void advance(qreal progress)
    {
        if (!m_owner || m_owner->window() != parentWidget()) {
            hide();
            return;
        }
        const QRectF source(m_owner->mapTo(parentWidget(), m_owner->m_iconRect.topLeft()),
                            m_owner->m_iconRect.size());
        if (m_owner->hasTransitionTarget()) {
            const auto* target = m_owner->transitionTarget();
            QRectF destination(target->mapTo(parentWidget(), target->contentsRect().topLeft()),
                               target->contentsRect().size());
            QSizeF size = source.size();
            size.scale(destination.size(), Qt::KeepAspectRatio);
            destination =
                QRectF(destination.center() - QPointF(size.width(), size.height()) / 2, size);
            m_rect =
                QRectF(source.topLeft() + (destination.topLeft() - source.topLeft()) * progress,
                       source.size() + (destination.size() - source.size()) * progress);
            const qreal entranceOpacity = 0.12 + smoothStep(m_owner->m_reveal) * 0.88;
            m_opacity = entranceOpacity + (1.0 - entranceOpacity) * progress;
        } else {
            // A removed, hidden, or reparented destination fades from the last valid position.
            // zh_CN: 目标移除、隐藏或换窗时，从最后有效位置淡出，不跳回中央。
            m_opacity = 1.0 - progress;
        }
        setGeometry(m_rect.toAlignedRect());
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_owner)
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setOpacity(m_opacity);
        m_owner->m_icon.paint(&painter, rect(), Qt::AlignCenter, QIcon::Normal, QIcon::Off);
    }

private:
    QPointer<SplashScreen> m_owner;
    QRectF m_rect;
    qreal m_opacity = 1.0;
};

SplashScreen::SplashScreen(QWidget* parent) : QWidget(parent)
{
    static const bool registered = []() {
        QAccessible::installFactory(splashAccessibleFactory);
        return true;
    }();
    Q_UNUSED(registered);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoMousePropagation);
    m_ring = new ProgressRing(this);
    m_ring->setObjectName(QStringLiteral("splashProgressRing"));
    m_ring->setFixedSize(32, 32);
    m_ring->setIsIndeterminate(true);
    m_ring->setFocusPolicy(Qt::NoFocus);
    m_bar = new ProgressBar(this);
    m_bar->setObjectName(QStringLiteral("splashProgressBar"));
    m_bar->setTrackThickness(2.0);
    m_bar->setFocusPolicy(Qt::NoFocus);
    m_brandText = new QWidget(this);
    m_brandText->setObjectName(QStringLiteral("splashBrandText"));
    m_title = new BrandLabel(m_brandText);
    m_title->setObjectName(QStringLiteral("splashTitle"));
    m_subtitle = new BrandLabel(m_brandText);
    m_subtitle->setObjectName(QStringLiteral("splashSubtitle"));
    m_text = new textfields::Label(this);
    m_text->setObjectName(QStringLiteral("splashStatusText"));
    m_percentage = new textfields::Label(this);
    m_percentage->setObjectName(QStringLiteral("splashPercentage"));
    for (auto* label : {m_text, m_percentage, static_cast<textfields::Label*>(m_title),
                        static_cast<textfields::Label*>(m_subtitle)}) {
        label->setTextFormat(Qt::PlainText);
        label->setAlignment(Qt::AlignCenter);
        label->setTextElideMode(Qt::ElideRight);
        label->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        label->setFluentTypography(Typography::FontRole::Caption);
        label->setFocusPolicy(Qt::NoFocus);
    }
    m_title->setTextColorRole(textfields::Label::TextColorRole::Primary);
    m_title->setFluentTypography(Typography::FontRole::Title);
    m_intro = new QVariantAnimation(this);
    m_intro->setObjectName(QStringLiteral("splashRevealAnimation"));
    m_intro->setStartValue(0.0);
    m_intro->setEndValue(1.0);
    connect(m_intro, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        const QRect previous = revealUpdateRect(m_reveal);
        m_reveal = value.toReal();
        refreshBrandOpacity();
        update(previous.united(revealUpdateRect(m_reveal)));
    });
    // Enable subtree compositing only during dismissal, so child updates stay local while loading.
    // zh_CN: 仅退场时启用子树合成，让加载期间的子控件更新保持局部重绘。
    m_opacity = new QGraphicsOpacityEffect(this);
    m_opacity->setOpacity(1.0);
    m_opacity->setEnabled(false);
    setGraphicsEffect(m_opacity);
    m_fade = new QPropertyAnimation(m_opacity, "opacity", this);
    m_fade->setObjectName(QStringLiteral("splashDismissAnimation"));
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        if (m_logoTransition)
            m_logoTransition->advance(1.0 - value.toReal());
    });
    connect(m_fade, &QPropertyAnimation::finished, this, [this]() {
        if (!m_dismissing)
            return;
        QPointer<SplashScreen> guard(this);
        hide();
        if (guard)
            emit dismissed();
    });
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this,
            [this](MotionPolicy::Mode mode) {
                if (mode != MotionPolicy::Mode::Full) {
                    if (m_logoTransition)
                        m_logoTransition->hide();
                    m_intro->stop();
                    m_reveal = 1.0;
                    refreshBrandOpacity();
                    update();
                }
            });
    if (parent)
        setGeometry(parent->rect());
    refreshProgress();
    onThemeUpdated();
}

SplashScreen::~SplashScreen()
{
    clearLogoTransition();
    releaseInput();
}

void SplashScreen::setPresentation(Presentation presentation)
{
    if ((presentation != Presentation::Branded && presentation != Presentation::Simple) ||
        m_presentation == presentation)
        return;
    m_presentation = presentation;
    m_backgroundCache = QPixmap();
    m_intro->stop();
    m_reveal = 1.0;
    refreshBrandOpacity();
    clearLogoTransition();
    refreshProgress();
    emit presentationChanged(presentation);
}

QString SplashScreen::title() const
{
    return m_title->text();
}
void SplashScreen::setTitle(const QString& title)
{
    if (m_title->text() == title)
        return;
    m_title->setText(title);
    layoutContent();
    emit titleChanged(title);
}
QString SplashScreen::subtitle() const
{
    return m_subtitle->text();
}
void SplashScreen::setSubtitle(const QString& subtitle)
{
    if (m_subtitle->text() == subtitle)
        return;
    m_subtitle->setText(subtitle);
    layoutContent();
    emit subtitleChanged(subtitle);
}

void SplashScreen::setTransitionTarget(QWidget* target)
{
    if (target && (target == this || isAncestorOf(target)))
        return;
    if (m_transitionTarget == target)
        return;
    disconnect(m_targetDestroyed);
    m_transitionTarget = target;
    if (target) {
        m_targetDestroyed = connect(target, &QObject::destroyed, this, [this]() {
            m_transitionTarget = nullptr;
            emit transitionTargetChanged(nullptr);
        });
    }
    emit transitionTargetChanged(target);
}

bool SplashScreen::hasTransitionTarget() const
{
    const auto* target = m_transitionTarget.data();
    return target && target != this && !isAncestorOf(target) && target->window() == window() &&
           target->isVisible() && !target->contentsRect().isEmpty();
}

void SplashScreen::clearLogoTransition()
{
    const auto transition = m_logoTransition;
    m_logoTransition = nullptr;
    if (transition) {
        // The window may be iterating its children while hiding the covered host.
        // Retire this sibling immediately, but let Qt delete it after that traversal.
        // zh_CN: 窗口隐藏宿主时可能正在遍历子控件；立即隐藏图标层，遍历完成后再销毁。
        transition->setObjectName(QString());
        transition->hide();
        transition->deleteLater();
    }
}

void SplashScreen::startReveal()
{
    m_intro->stop();
    m_reveal = 1.0;
    refreshBrandOpacity();
    if (m_presentation != Presentation::Branded || effectiveTheme() == HighContrast ||
        MotionPolicy::instance().mode() != MotionPolicy::Mode::Full)
        return;
    m_reveal = 0.0;
    refreshBrandOpacity();
    update();
    m_intro->setStartValue(0.0);
    m_intro->setEndValue(1.0);
    ::fluent::detail::startMotionTransition(m_intro, themeAnimation().slow * 2);
}

void SplashScreen::setIcon(const QIcon& icon)
{
    if (m_icon.cacheKey() == icon.cacheKey())
        return;
    m_icon = icon;
    layoutContent();
    update();
    emit iconChanged(icon);
}

void SplashScreen::setIconSize(const QSize& size)
{
    const QSize normalized(qMax(1, size.width()), qMax(1, size.height()));
    if (normalized == m_iconSize)
        return;
    m_iconSize = normalized;
    layoutContent();
    update();
    emit iconSizeChanged(normalized);
}

QString SplashScreen::text() const
{
    return m_text->text();
}

void SplashScreen::setText(const QString& text)
{
    if (text == m_text->text())
        return;
    m_text->setText(text);
    layoutContent();
    emit textChanged(text);
}

void SplashScreen::refreshProgress(bool relayout)
{
    m_ring->setIsIndeterminate(m_indeterminate);
    m_ring->setValue(m_progress);
    m_bar->setIsIndeterminate(m_indeterminate);
    m_bar->setValue(m_progress);
    m_percentage->setText(m_indeterminate ? QString()
                                          : QString::number(m_progress) + QLatin1Char('%'));
    if (relayout)
        layoutContent();
}

void SplashScreen::setProgress(int done, int total)
{
    if (total <= 0) {
        setIndeterminate(true);
        return;
    }
    const int next = int(qBound<qint64>(qint64(0), qint64(done) * 100 / total, qint64(100)));
    const bool modeChanged = m_indeterminate;
    const bool valueChanged = next != m_progress;
    if (!modeChanged && !valueChanged)
        return;
    m_indeterminate = false;
    m_progress = next;
    refreshProgress(modeChanged);
    QPointer<SplashScreen> guard(this);
    if (modeChanged)
        emit indeterminateChanged(false);
    if (guard && valueChanged && m_progress == next)
        emit progressChanged(next);
}

void SplashScreen::setIndeterminate(bool indeterminate)
{
    if (m_indeterminate == indeterminate)
        return;
    m_indeterminate = indeterminate;
    refreshProgress();
    emit indeterminateChanged(indeterminate);
}

void SplashScreen::dismiss()
{
    if (m_dismissing || !isVisible())
        return;
    m_dismissing = true;
    // An opaque-paint attribute would turn the opacity-effect fade black.
    // zh_CN: 淡出前恢复透明合成，避免不透明绘制属性使退场背景变黑。
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    m_opacity->setEnabled(true);
    m_intro->stop();
    m_ring->setIsActive(false);
    m_bar->setIsIndeterminate(false);
    if (m_presentation == Presentation::Branded && !m_icon.isNull() && !m_iconRect.isEmpty() &&
        hasTransitionTarget() && MotionPolicy::instance().mode() == MotionPolicy::Mode::Full) {
        m_logoTransition = new LogoTransition(this);
        m_logoTransition->show();
        m_logoTransition->raise();
    }
    m_fade->setStartValue(m_opacity->opacity());
    m_fade->setEndValue(0.0);
    const auto animation = themeAnimation();
    // A connected logo travels across the window: give it a gentle departure and arrival.
    // zh_CN: 图标跨窗口移动时使用平滑起落，避免减速曲线把大部分路程挤在最初几帧。
    m_fade->setEasingCurve(m_logoTransition ? animation.standard : animation.decelerate);
    ::fluent::detail::startMotionTransition(m_fade, m_logoTransition ? animation.verySlow
                                                                     : animation.normal);
}

QSize SplashScreen::sizeHint() const
{
    return QSize(640, 480);
}
QSize SplashScreen::minimumSizeHint() const
{
    return QSize(160, 160);
}

void SplashScreen::onThemeUpdated()
{
    m_backgroundCache = QPixmap();
    setAttribute(Qt::WA_OpaquePaintEvent, !m_dismissing && themeColors().bgCanvas.alpha() == 255);
    const QFont caption = themeFont(Typography::FontRole::Caption).toQFont();
    const QColor color = themeColors().textSecondary;
    const QString rgba = color.name(QColor::HexRgb) +
                         QStringLiteral("%1").arg(color.alpha(), 2, 16, QLatin1Char('0'));
    const QJsonObject colors{{"textSecondary", rgba}};
    for (auto* label : {m_text, m_percentage}) {
        label->setFont(caption);
        // Internal captions resolve the surface's colors even if child theme callbacks run later.
        // zh_CN: 内部标题采用当前表面颜色，子控件稍后收到主题回调也不会覆盖它。
        label->setThemeOverrides({{"light", colors}, {"dark", colors}, {"contrast", colors}});
    }
    m_title->setFont(themeFont(Typography::FontRole::Title).toQFont());
    m_subtitle->setFont(caption);
    if (effectiveTheme() == HighContrast) {
        m_intro->stop();
        m_reveal = 1.0;
    }
    refreshBrandColors();
    refreshBrandOpacity();
    layoutContent();
    update();
}

void SplashScreen::refreshBrandColors()
{
    const auto colorValue = [](const QColor& color) {
        return color.name(QColor::HexRgb) +
               QStringLiteral("%1").arg(color.alpha(), 2, 16, QLatin1Char('0'));
    };
    const QJsonObject colors{{"textPrimary", colorValue(themeColors().textPrimary)},
                             {"textSecondary", colorValue(themeColors().textSecondary)}};
    for (auto* label : {m_title, m_subtitle})
        label->setThemeOverrides({{"light", colors}, {"dark", colors}, {"contrast", colors}});
}

void SplashScreen::refreshBrandOpacity()
{
    const qreal opacity = smoothStep((m_reveal - 0.2) / 0.65);
    m_title->setPaintOpacity(opacity);
    m_subtitle->setPaintOpacity(opacity);
}

bool SplashScreen::covers(const QWidget* widget) const
{
    const auto* host = parentWidget();
    return widget && host && (widget == host || host->isAncestorOf(widget));
}

void SplashScreen::releaseInput()
{
    if (m_filterInstalled && qApp) {
        qApp->removeEventFilter(this);
        m_filterInstalled = false;
    }
    const QPointer<QWidget> previous = m_previousFocus;
    m_previousFocus.clear();
    if (previous && covers(previous) && previous->window() == window() && previous->isVisible() &&
        previous->isEnabled() && window()->isActiveWindow() &&
        (!QApplication::focusWidget() || covers(QApplication::focusWidget())))
        previous->setFocus(Qt::OtherFocusReason);
}

void SplashScreen::showEvent(QShowEvent* event)
{
    if (event->spontaneous()) {
        QWidget::showEvent(event);
        return;
    }
    // Only one visible cover may arbitrate overlapping host input and stacking.
    // zh_CN: 重叠宿主只保留一个可见遮罩，避免焦点和堆叠事件互相争抢。
    QList<QPointer<SplashScreen>> previous;
    if (auto* host = parentWidget()) {
        for (auto* other : window()->findChildren<SplashScreen*>()) {
            if (other != this && other->isVisible() && other->parentWidget() &&
                (other->parentWidget() == host || host->isAncestorOf(other->parentWidget()) ||
                 other->parentWidget()->isAncestorOf(host)))
                previous.append(other);
        }
        QPointer<SplashScreen> guard(this);
        for (const auto& other : previous) {
            if (other)
                other->hide();
            if (!guard)
                return;
        }
    }
    QWidget::showEvent(event);
    m_dismissing = false;
    clearLogoTransition();
    m_fade->stop();
    m_opacity->setEnabled(false);
    m_opacity->setOpacity(1.0);
    setAttribute(Qt::WA_OpaquePaintEvent, themeColors().bgCanvas.alpha() == 255);
    if (parentWidget())
        setGeometry(parentWidget()->rect());
    raise();
    refreshProgress();
    startReveal();
    if (!m_filterInstalled) {
        if (covers(QApplication::focusWidget()) && QApplication::focusWidget() != this)
            m_previousFocus = QApplication::focusWidget();
        qApp->installEventFilter(this);
        m_filterInstalled = true;
    }
    if (!QApplication::focusWidget() || covers(QApplication::focusWidget()))
        setFocus(Qt::OtherFocusReason);
    QAccessible::State changed;
    changed.busy = true;
    QAccessibleStateChangeEvent notice(this, changed);
    QAccessible::updateAccessibility(&notice);
    // Notify after the new cover has settled; callbacks may show a third cover or delete either.
    // zh_CN: 新遮罩就绪后再通知；回调可能显示第三个遮罩，或销毁任一遮罩。
    for (const auto& other : previous) {
        if (other)
            emit other->replaced();
    }
}

void SplashScreen::hideEvent(QHideEvent* event)
{
    // Minimizing the window must not cancel an application-requested dismissal.
    // zh_CN: 最小化窗口不应取消应用已经发起的退场。
    if (event->spontaneous()) {
        QWidget::hideEvent(event);
        return;
    }
    m_fade->stop();
    m_intro->stop();
    clearLogoTransition();
    m_backgroundCache = QPixmap();
    m_reflection = QImage();
    m_dismissing = false;
    m_ring->setIsActive(false);
    releaseInput();
    QWidget::hideEvent(event);
    QAccessible::State changed;
    changed.busy = true;
    QAccessibleStateChangeEvent notice(this, changed);
    QAccessible::updateAccessibility(&notice);
}

bool SplashScreen::event(QEvent* event)
{
    if (event->type() == QEvent::ParentAboutToChange)
        clearLogoTransition();
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease ||
        event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

bool SplashScreen::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        raise();
    }
    auto* widget = qobject_cast<QWidget*>(watched);
    if (isVisible() && covers(widget)) {
        if (widget != this && widget != m_logoTransition &&
            widget->parentWidget() == parentWidget() &&
            (event->type() == QEvent::Show || event->type() == QEvent::ZOrderChange)) {
            raise();
            if (m_logoTransition)
                m_logoTransition->raise();
        }
        if (event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true;
        }
        if (widget != this && !isAncestorOf(widget)) {
            switch (event->type()) {
            case QEvent::FocusIn:
                if (!m_previousFocus)
                    m_previousFocus = widget;
                setFocus(Qt::OtherFocusReason);
                break;
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
            case QEvent::Wheel:
            case QEvent::ContextMenu:
            case QEvent::TouchBegin:
            case QEvent::TouchUpdate:
            case QEvent::TouchEnd:
                event->accept();
                return true;
            default:
                break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SplashScreen::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutContent();
}

void SplashScreen::layoutContent()
{
    const bool branded = m_presentation == Presentation::Branded;
    m_ring->setVisible(!branded);
    m_ring->setIsActive(!branded && isVisible() && !m_dismissing);
    m_bar->setVisible(branded);
    m_brandText->setVisible(branded && (!title().isEmpty() || !subtitle().isEmpty()));
    if (branded) {
        layoutBrandedContent();
        return;
    }
    m_text->setAlignment(Qt::AlignCenter);
    m_percentage->setAlignment(Qt::AlignCenter);
    const int margin = qMin(16, qMin(width(), height()) / 4);
    const int availableWidth = qMax(0, width() - 2 * margin);
    const int availableHeight = qMax(0, height() - 2 * margin);
    const bool hasText = !m_text->text().isEmpty();
    const bool hasPercentage = !m_indeterminate;
    const int lines = int(hasText) + int(hasPercentage);
    const int captionHeight =
        qMin(QFontMetrics(m_text->font()).height(), availableHeight / qMax(1, lines + 1));
    const int captionGap = lines ? qMin(12, availableHeight / 12) : 0;
    const int captions = lines * captionHeight + captionGap;
    const int ringSize = qMin(32, qMax(0, availableHeight - captions));
    const int iconGap = qMin(16, availableHeight / 12);
    const int iconSpace = qMax(0, availableHeight - captions - ringSize - iconGap);
    QSize logo = m_icon.isNull() ? QSize(0, 0)
                                 : m_iconSize.scaled(QSize(qMin(availableWidth, m_iconSize.width()),
                                                           qMin(iconSpace, m_iconSize.height())),
                                                     Qt::KeepAspectRatio);
    if (logo.width() < 1 || logo.height() < 1)
        logo = QSize(0, 0);
    const int maxIconTop = height() - margin - captions - ringSize - iconGap - logo.height();
    const int iconTop = qMax(margin, qMin(height() / 2 - logo.height() / 2, maxIconTop));
    m_iconRect = QRect(QPoint((width() - logo.width()) / 2, iconTop), logo);
    const int lastRingTop = height() - margin - captions - ringSize;
    const int preferredRingTop =
        m_icon.isNull() ? (height() - ringSize - captions) / 2 : height() / 2 + 144 - ringSize / 2;
    const int firstRingTop = logo.isEmpty() ? margin : iconTop + logo.height() + iconGap;
    const int ringTop = qMax(firstRingTop, qMin(preferredRingTop, lastRingTop));
    m_ring->setFixedSize(ringSize, ringSize);
    m_ring->move((width() - ringSize) / 2, ringTop);
    int y = ringTop + ringSize + captionGap;
    m_text->setVisible(hasText);
    m_text->setGeometry(margin, y, availableWidth, hasText ? captionHeight : 0);
    if (hasText)
        y += captionHeight;
    m_percentage->setVisible(hasPercentage);
    m_percentage->setGeometry(margin, y, availableWidth, hasPercentage ? captionHeight : 0);
    update();
}

void SplashScreen::layoutBrandedContent()
{
    const int margin = qMin(32, qMin(width(), height()) / 8);
    const int availableWidth = qMax(0, width() - margin * 2);
    const int availableHeight = qMax(0, height() - margin * 2);
    const int gap = qMin(12, availableHeight / 16);
    const int caption = qMin(QFontMetrics(m_text->font()).height(), availableHeight / 6);
    const int barHeight = qMin(4, availableHeight / 8);
    const int barTop = height() - margin - barHeight;
    m_bar->setGeometry(margin, barTop, availableWidth, barHeight);
    m_bar->setBarWidth(availableWidth);
    const int statusTop = barTop - gap - caption;
    const bool percentage = !m_indeterminate;
    const int percentWidth =
        percentage
            ? qMin(availableWidth / 3,
                   QFontMetrics(m_percentage->font()).horizontalAdvance(QStringLiteral("100%")) +
                       gap)
            : 0;
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_percentage->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_text->setVisible(!text().isEmpty());
    m_percentage->setVisible(percentage);
    m_text->setGeometry(margin, statusTop, qMax(0, availableWidth - percentWidth), caption);
    m_percentage->setGeometry(width() - margin - percentWidth, statusTop, percentWidth, caption);
    const int titleHeight =
        title().isEmpty() ? 0 : qMin(QFontMetrics(m_title->font()).height(), availableHeight / 5);
    const int subtitleHeight = subtitle().isEmpty() ? 0 : caption;
    const int wordmarkHeight =
        titleHeight + subtitleHeight + (titleHeight && subtitleHeight ? gap / 2 : 0);
    const int iconSpace = qMax(0, statusTop - margin - wordmarkHeight - gap * 2);
    QSize logo = m_icon.isNull() ? QSize(0, 0)
                                 : m_iconSize.scaled(QSize(qMin(availableWidth, m_iconSize.width()),
                                                           qMin(iconSpace, m_iconSize.height())),
                                                     Qt::KeepAspectRatio);
    if (logo.width() < 1 || logo.height() < 1)
        logo = QSize(0, 0);
    const int lastTop = statusTop - gap * 2 - wordmarkHeight - logo.height();
    const int top = qMax(margin, qMin(qRound(height() * 0.44) - logo.height() / 2, lastTop));
    m_iconRect = QRect(QPoint((width() - logo.width()) / 2, top), logo);
    m_brandText->setGeometry(margin, top + logo.height() + gap, availableWidth, wordmarkHeight);
    m_title->setVisible(titleHeight > 0);
    m_subtitle->setVisible(subtitleHeight > 0);
    m_title->setGeometry(0, 0, availableWidth, titleHeight);
    m_subtitle->setGeometry(0, wordmarkHeight - subtitleHeight, availableWidth, subtitleHeight);
    update();
}

QRect SplashScreen::revealUpdateRect(qreal reveal) const
{
    const qreal radius = qMax(m_iconRect.width() * 2, 1);
    const QPointF center = m_iconRect.center() + QPointF((reveal - 0.5) * 120, 0);
    const QRect glow = QRectF(center - QPointF(radius, radius), QSizeF(radius * 2, radius * 2))
                           .toAlignedRect()
                           .adjusted(-1, -1, 1, 1);
    return glow.united(m_iconRect).intersected(rect());
}

void SplashScreen::refreshBackgroundCache()
{
    const qreal dpr = devicePixelRatioF();
    const QSize pixels(qCeil(width() * dpr), qCeil(height() * dpr));
    if (m_backgroundCache.size() == pixels && m_backgroundCache.devicePixelRatioF() == dpr)
        return;
    // Keep the original gradients at the exact backing-store resolution, including fractional DPR.
    // zh_CN: 按实际 backing store 分辨率缓存原始渐变，保留分数缩放下的绘制效果。
    m_backgroundCache = QPixmap(pixels);
    m_backgroundCache.setDevicePixelRatio(dpr);
    m_backgroundCache.fill(Qt::transparent);
    QPainter painter(&m_backgroundCache);
    const auto colors = themeColors();
    painter.fillRect(rect(), colors.bgCanvas);
    const qreal radius = qMax(width(), height()) * 0.65;
    paintGlow(painter, QPointF(width() * 0.08, -height() * 0.16), radius, colors.accentDefault,
              0.16);
    paintGlow(painter, QPointF(width() * 1.04, height() * 1.1), radius, colors.accentDefault, 0.1);
}

void SplashScreen::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const auto colors = themeColors();
    const bool branded = m_presentation == Presentation::Branded;
    const bool lighting = branded && effectiveTheme() != HighContrast;
    if (lighting) {
        refreshBackgroundCache();
        painter.drawPixmap(0, 0, m_backgroundCache);
        const qreal illumination = qSin(qBound<qreal>(0.0, m_reveal, 1.0) * M_PI);
        paintGlow(painter, m_iconRect.center() + QPointF((m_reveal - 0.5) * 120, 0),
                  qMax(m_iconRect.width() * 2, 1), colors.accentDefault, illumination * 0.12);
    } else {
        painter.fillRect(rect(), colors.bgCanvas);
    }
    if (!m_icon.isNull() && !m_iconRect.isEmpty() && !m_logoTransition &&
        event->region().intersects(m_iconRect)) {
        const qreal reveal = lighting ? smoothStep(m_reveal) : 1.0;
        painter.setOpacity((branded ? 0.12 + reveal * 0.88 : 1.0));
        m_icon.paint(&painter, m_iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);
        if (lighting && m_reveal > 0.0 && m_reveal < 1.0) {
            const qreal dpr = devicePixelRatioF();
            const QSize pixels(qMax(1, qCeil(m_iconRect.width() * dpr)),
                               qMax(1, qCeil(m_iconRect.height() * dpr)));
            if (m_reflection.size() != pixels)
                m_reflection = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
            m_reflection.fill(Qt::transparent);
            QPainter light(&m_reflection);
            light.drawPixmap(QRect(QPoint(), pixels), m_icon.pixmap(pixels));
            light.setCompositionMode(QPainter::CompositionMode_SourceIn);
            const qreal center = pixels.width() * (m_reveal * 2.0 - 0.5);
            QRadialGradient glow(QPointF(center, pixels.height() * 0.3), pixels.width());
            glow.setColorAt(0.0, QColor(255, 255, 255, 38));
            glow.setColorAt(1.0, Qt::transparent);
            light.fillRect(m_reflection.rect(), glow);
            light.end();
            painter.drawImage(m_iconRect, m_reflection);
        }
    }
}

} // namespace fluent::status_info

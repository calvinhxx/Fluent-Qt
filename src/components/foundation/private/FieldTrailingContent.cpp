#include "FieldTrailingContent_p.h"
#include "components/textfields/Label.h"
#include "design/Spacing.h"
#include <QEvent>
#include <QFontMetricsF>
#include <QtMath>

namespace fluent::detail {

FieldTrailingContent::FieldTrailingContent(QWidget* host) : m_host(host)
{
    host->installEventFilter(this);
}

FieldTrailingContent::~FieldTrailingContent()
{
    m_host->removeEventFilter(this);
    // Owned children are destroyed by QWidget; borrowed children must leave first.
    // zh_CN: 拥有的子控件交由 QWidget 销毁，借用的子控件必须先脱离宿主。
    release(false);
}

QWidget* FieldTrailingContent::content() const
{
    return m_widget ? m_widget.data() : m_label.data();
}

void FieldTrailingContent::notify(const State& previous)
{
    const QPointer<FieldTrailingContent> guard(this);
    emit geometryChanged();
    if (!guard)
        return;
    if (previous.text != m_text)
        emit textChanged();
    if (!guard)
        return;
    if (previous.widget != m_widget.data())
        emit widgetChanged();
    if (!guard)
        return;
    if (previous.ownership != m_ownership)
        emit ownershipChanged();
}

void FieldTrailingContent::release(bool destroyOwned)
{
    QObject::disconnect(m_destroyed);
    m_destroyed = {};
    QPointer<QWidget> widget = m_widget;
    QPointer<QWidget> original = m_originalParent;
    const auto ownership = m_ownership;
    m_widget = nullptr;
    m_originalParent = nullptr;
    m_ownership = WidgetOwnership::Borrowed;
    if (!widget)
        return;
    widget->removeEventFilter(this);
    widget->hide();
    if (!widget)
        return;
    if (ownership == WidgetOwnership::Owned) {
        if (destroyOwned)
            delete widget.data();
    } else {
        // Restoring into a dying host would also destroy borrowed content.
        // zh_CN: 析构期间不能恢复到即将销毁的宿主子树，否则借用内容仍会被删除。
        const bool safeParent = original && (destroyOwned || (original != m_host && !m_host->isAncestorOf(original)));
        widget->setParent(ownership == WidgetOwnership::Reparented && safeParent ? original.data() : nullptr);
    }
}

void FieldTrailingContent::setText(const QString& text)
{
    if (m_text == text && !m_widget)
        return;
    const State previous = state();
    const QPointer<FieldTrailingContent> guard(this);
    release(true);
    if (!guard)
        return;
    m_text = text;
    if (!text.isEmpty()) {
        if (!m_label) {
            m_label = new textfields::Label(m_host);
            m_label->setObjectName(QStringLiteral("fluentFieldTrailingText"));
            m_label->setTextFormat(Qt::PlainText);
            m_label->setTextElideMode(Qt::ElideRight);
            m_label->setFluentTypography(Typography::FontRole::Caption);
            m_label->setTextColorRole(textfields::Label::TextColorRole::Secondary);
            m_label->setAttribute(Qt::WA_TransparentForMouseEvents);
            m_label->setFocusPolicy(Qt::NoFocus);
            m_label->installEventFilter(this);
        }
        m_label->setText(text);
        m_label->setTextColorRole(m_host->isEnabled()
                ? textfields::Label::TextColorRole::Secondary
                : textfields::Label::TextColorRole::Disabled);
        m_label->setAccessibleName(text);
        m_label->show();
    } else if (m_label) {
        m_label->hide();
    }
    notify(previous);
}

bool FieldTrailingContent::setWidget(QWidget* widget, WidgetOwnership ownership)
{
    if (widget
        && (widget == m_host || widget == m_label || widget->isAncestorOf(m_host)
            || widget->thread() != m_host->thread()))
        return false;
    if (widget && m_widget && m_ownership == WidgetOwnership::Owned && m_widget->isAncestorOf(widget))
        return false;
    if (widget && widget == m_widget)
        return ownership == m_ownership;
    if (!widget && !m_widget && m_text.isEmpty())
        return true;
    const State previous = state();
    const QPointer<FieldTrailingContent> guard(this);
    const QPointer<QWidget> incoming(widget);
    release(true);
    if (!guard || (widget && !incoming))
        return false;
    m_text.clear();
    if (m_label)
        m_label->hide();
    m_widget = widget;
    m_originalParent = widget ? widget->parentWidget() : nullptr;
    m_ownership = widget ? ownership : WidgetOwnership::Borrowed;
    if (widget) {
        widget->setParent(m_host);
        if (!guard || !incoming)
            return false;
        widget->installEventFilter(this);
        m_destroyed = connect(widget, &QObject::destroyed, this, [this, widget] {
            const State previous{m_text, widget, m_ownership};
            m_widget = nullptr;
            m_originalParent = nullptr;
            m_ownership = WidgetOwnership::Borrowed;
            notify(previous);
        });
        widget->show();
    }
    if (guard)
        notify(previous);
    return true;
}

QWidget* FieldTrailingContent::takeWidget()
{
    if (!m_widget)
        return nullptr;
    const State previous = state();
    QPointer<QWidget> widget = m_widget;
    m_ownership = WidgetOwnership::Borrowed;
    const QPointer<FieldTrailingContent> guard(this);
    release(true);
    if (guard)
        notify(previous);
    return widget.data();
}

void FieldTrailingContent::releaseWidget()
{
    if (!m_widget)
        return;
    const State previous = state();
    const QPointer<FieldTrailingContent> guard(this);
    release(true);
    if (guard)
        notify(previous);
}

QSize FieldTrailingContent::sizeHint() const
{
    QWidget* child = content();
    if (!child || child->isHidden())
        return QSize(0, 0);
    if (!m_text.isEmpty()) {
        // Rounded integer advances can elide even a two-letter unit. Leave the
        // same small raster clearance used by the closed ComboBox text surface.
        // zh_CN: 整数取整可能让两个字母的单位也被省略；保留与 ComboBox 文字表面相同的小幅栅格余量。
        const QFontMetricsF metrics(child->font());
        const int width = qCeil(qMax(metrics.horizontalAdvance(m_text), metrics.boundingRect(m_text).width())) + 2;
        return QSize(width, child->fontMetrics().height());
    }
    return child->sizeHint()
        .expandedTo(child->minimumSizeHint())
        .expandedTo(child->minimumSize())
        .boundedTo(child->maximumSize())
        .expandedTo(QSize(0, 0));
}

int FieldTrailingContent::layout(const QRect& available, Qt::LayoutDirection direction, int minimumTextWidth)
{
    QWidget* child = content();
    if (!child || child->isHidden() || m_layingOut)
        return 0;
    const QSize hint = sizeHint();
    const int gap = Spacing::XSmall;
    const int minimumWidth = m_widget ? qMax(m_widget->minimumWidth(), m_widget->minimumSizeHint().width()) : 0;
    const int width = qMin(hint.width(),
        qMin(qMax(0, available.width() - gap), qMax(minimumWidth, available.width() - minimumTextWidth - gap)));
    const int height = qMin(hint.height(), qMax(0, available.height()));
    const int x = direction == Qt::RightToLeft ? available.left() : available.right() + 1 - width;
    const QRect geometry(x, available.top() + (available.height() - height) / 2, width, height);
    const QPointer<FieldTrailingContent> guard(this);
    m_layingOut = true;
    if (child->geometry() != geometry)
        child->setGeometry(geometry);
    if (guard)
        m_layingOut = false;
    return width > 0 ? width + gap : 0;
}

void FieldTrailingContent::refreshFont()
{
    if (m_label)
        m_label->setFluentTypography(Typography::FontRole::Caption);
}

bool FieldTrailingContent::eventFilter(QObject* watched, QEvent* event)
{
    const QPointer<QObject> guard(watched);
    if (watched == m_host && event->type() == QEvent::EnabledChange && m_label)
        m_label->setTextColorRole(m_host->isEnabled()
                ? textfields::Label::TextColorRole::Secondary
                : textfields::Label::TextColorRole::Disabled);
    if (watched == m_widget && event->type() == QEvent::ParentChange && m_widget->parentWidget() != m_host) {
        const State previous = state();
        QObject::disconnect(m_destroyed);
        m_widget->removeEventFilter(this);
        m_widget = nullptr;
        m_originalParent = nullptr;
        m_ownership = WidgetOwnership::Borrowed;
        notify(previous);
        return !guard;
    }
    if (!m_layingOut
        && ((watched == m_host && event->type() == QEvent::LayoutRequest)
            || (watched == content()
                && (event->type() == QEvent::Show || event->type() == QEvent::Hide
                    || event->type() == QEvent::LayoutRequest || event->type() == QEvent::FontChange
                    || event->type() == QEvent::Resize))))
        emit geometryChanged();
    return !guard;
}

} // namespace fluent::detail

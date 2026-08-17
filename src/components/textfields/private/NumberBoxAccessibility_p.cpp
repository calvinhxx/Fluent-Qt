#include "NumberBoxAccessibility_p.h"

#include <QAccessible>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QLineEdit>

#include <cmath>
#include <limits>

#include "components/foundation/private/ValueAccessibility_p.h"
#include "components/textfields/NumberBox.h"

namespace fluent::textfields::detail {

#if QT_CONFIG(accessibility)

namespace {

using accessibility::detail::ValueAccessibleAdapter;

QString numberText(const char* source)
{
    return QCoreApplication::translate(
        "NumberBoxAccessibility", source);
}

class NumberBoxAccessible final : public ValueAccessibleAdapter,
                                  public QAccessibleTextInterface,
                                  public QAccessibleEditableTextInterface {
public:
    explicit NumberBoxAccessible(NumberBox* box)
        : ValueAccessibleAdapter(box, QAccessible::SpinBox)
    {
    }

    void* interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::TextInterface)
            return static_cast<QAccessibleTextInterface*>(this);
        if (type == QAccessible::EditableTextInterface)
            return static_cast<QAccessibleEditableTextInterface*>(this);
        return ValueAccessibleAdapter::interface_cast(type);
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = ValueAccessibleAdapter::text(type);
        if (type == QAccessible::Name && inherited.isEmpty() && view())
            return view()->header();
        return inherited;
    }

    void setText(QAccessible::Text type, const QString& text) override
    {
        if (type == QAccessible::Value && canEditText()) {
            view()->setText(text);
            return;
        }
        QAccessibleWidget::setText(type, text);
    }

    QVariant currentValue() const override
    {
        return view() && std::isfinite(view()->value())
            ? QVariant(view()->value()) : QVariant();
    }

    void setCurrentValue(const QVariant& value) override
    {
        if (!canEditText())
            return;
        bool ok = false;
        const double requested = value.toDouble(&ok);
        if (ok && std::isfinite(requested))
            view()->setValue(requested);
    }

    QVariant maximumValue() const override
    {
        return view() && std::isfinite(view()->maximum())
            ? QVariant(view()->maximum()) : QVariant();
    }

    QVariant minimumValue() const override
    {
        return view() && std::isfinite(view()->minimum())
            ? QVariant(view()->minimum()) : QVariant();
    }

    QVariant minimumStepSize() const override
    {
        return view() ? QVariant(view()->smallChange()) : QVariant();
    }

    void selection(int selectionIndex, int* startOffset,
                   int* endOffset) const override
    {
        if (startOffset)
            *startOffset = -1;
        if (endOffset)
            *endOffset = -1;
        if (!view() || selectionIndex != 0 || !view()->hasSelectedText())
            return;
        const int start = view()->selectionStart();
        if (startOffset)
            *startOffset = start;
        if (endOffset)
            *endOffset = start + view()->selectedText().size();
    }

    int selectionCount() const override
    {
        return view() && view()->hasSelectedText() ? 1 : 0;
    }

    void addSelection(int startOffset, int endOffset) override
    {
        applySelection(startOffset, endOffset);
    }

    void removeSelection(int selectionIndex) override
    {
        if (view() && selectionIndex == 0)
            view()->deselect();
    }

    void setSelection(int selectionIndex, int startOffset,
                      int endOffset) override
    {
        if (selectionIndex == 0)
            applySelection(startOffset, endOffset);
    }

    int cursorPosition() const override
    {
        return view() ? view()->cursorPosition() : 0;
    }

    void setCursorPosition(int position) override
    {
        if (view())
            view()->setCursorPosition(qBound(0, position, characterCount()));
    }

    QString text(int startOffset, int endOffset) const override
    {
        if (!view())
            return {};
        const QString content = view()->text();
        const int start = qBound(0, startOffset, content.size());
        const int end = endOffset < 0
            ? content.size() : qBound(start, endOffset, content.size());
        return content.mid(start, end - start);
    }

    int characterCount() const override
    {
        return view() ? view()->text().size() : 0;
    }

    QRect characterRect(int offset) const override
    {
        NumberBox* box = view();
        if (!box || offset < 0 || offset > characterCount())
            return {};

        const QFontMetrics metrics(box->font());
        const int y = qMax(0, box->height() - 16);
        int first = -1;
        int last = -1;
        int nearestX = 0;
        int nearestDistance = std::numeric_limits<int>::max();
        for (int x = 0; x < box->width(); ++x) {
            const int candidate = box->cursorPositionAt(QPoint(x, y));
            const int distance = qAbs(candidate - offset);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestX = x;
            }
            if (candidate == offset) {
                if (first < 0)
                    first = x;
                last = x;
            }
        }
        if (first < 0)
            first = last = nearestX;
        const int textHeight = qMax(1, metrics.height());
        const QRect local(first, qMax(0, y - textHeight / 2),
                          qMax(1, last - first + 1), textHeight);
        return QRect(box->mapToGlobal(local.topLeft()), local.size());
    }

    int offsetAtPoint(const QPoint& point) const override
    {
        return view()
            ? view()->cursorPositionAt(view()->mapFromGlobal(point)) : -1;
    }

    void scrollToSubstring(int, int endIndex) override
    {
        setCursorPosition(endIndex);
    }

    QString attributes(int, int* startOffset,
                       int* endOffset) const override
    {
        if (startOffset)
            *startOffset = 0;
        if (endOffset)
            *endOffset = characterCount();
        return {};
    }

    void deleteText(int startOffset, int endOffset) override
    {
        replaceText(startOffset, endOffset, {});
    }

    void insertText(int offset, const QString& text) override
    {
        replaceText(offset, offset, text);
    }

    void replaceText(int startOffset, int endOffset,
                     const QString& replacement) override
    {
        if (!canEditText())
            return;
        QString content = view()->text();
        const int start = qBound(0, startOffset, content.size());
        const int end = qBound(start, endOffset, content.size());
        content.replace(start, end - start, replacement);
        view()->setText(content);
        view()->setCursorPosition(start + replacement.size());
    }

protected:
    bool accessibleValueReadOnly() const override
    {
        return !view() || view()->isReadOnly();
    }

    bool accessibleValueInvalid() const override
    {
        return !view() || !std::isfinite(view()->value());
    }

    bool canIncreaseAccessibleValue() const override
    {
        if (!canEditText())
            return false;
        return !std::isfinite(view()->value())
            || !std::isfinite(view()->maximum())
            || view()->value() < view()->maximum();
    }

    bool canDecreaseAccessibleValue() const override
    {
        if (!canEditText())
            return false;
        return !std::isfinite(view()->value())
            || !std::isfinite(view()->minimum())
            || view()->value() > view()->minimum();
    }

    void changeAccessibleValueByStep(int direction) override
    {
        if (!canEditText())
            return;
        double base = view()->value();
        if (!std::isfinite(base)) {
            if (view()->minimum() <= 0.0 && view()->maximum() >= 0.0)
                base = 0.0;
            else if (view()->minimum() > 0.0)
                base = view()->minimum();
            else if (view()->maximum() < 0.0)
                base = view()->maximum();
            else
                base = 0.0;
        }
        view()->setValue(base + direction * view()->smallChange());
    }

    QString accessibleValueText() const override
    {
        if (!view() || !std::isfinite(view()->value()))
            return numberText("Invalid value");
        return view()->text();
    }

private:
    NumberBox* view() const
    {
        return static_cast<NumberBox*>(widget());
    }

    bool canEditText() const
    {
        return view() && view()->isEnabled() && !view()->isReadOnly();
    }

    void applySelection(int startOffset, int endOffset)
    {
        if (!view())
            return;
        const int start = qBound(0, startOffset, characterCount());
        const int end = qBound(start, endOffset, characterCount());
        view()->setSelection(start, end - start);
    }
};

QAccessibleInterface* numberBoxAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* box = dynamic_cast<NumberBox*>(object);
    return box ? new NumberBoxAccessible(box) : nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureNumberBoxAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(numberBoxAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

} // namespace fluent::textfields::detail

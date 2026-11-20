#include "PropertyPanel.h"
#include "compatibility/PaintCompat.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QPointer>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>
#include <limits>
#include "compatibility/private/VariantCompat_p.h"
#include "Field.h"
#include "Card.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/ColorPicker.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/FormController.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/NumberBox.h"
#include "design/Spacing.h"

namespace fluent::layout {
namespace {
class PropertyRowLayout final : public QHBoxLayout {
public:
    explicit PropertyRowLayout(QWidget* parent) : QHBoxLayout(parent) {}

    void setEditorRow(Field* field, QWidget* editor, QWidget* reset)
    {
        m_field = field;
        m_editor = editor;
        m_reset = reset;
        invalidate();
    }

    void setGeometry(const QRect& rect) override
    {
        const QPointer<PropertyRowLayout> guard(this);
        QHBoxLayout::setGeometry(rect);
        if (!guard || !m_field || !m_editor || !m_reset)
            return;
        m_field->layout()->activate();
        if (!guard || !m_editor || !m_reset)
            return;
        // Center reset button on the editor itself rather than the whole Field (captions/errors vary in height).
        const int centerY = m_editor->mapTo(parentWidget(), m_editor->rect().center()).y();
        m_reset->move(m_reset->x(), centerY - m_reset->rect().center().y());
    }

private:
    QPointer<Field> m_field;
    QPointer<QWidget> m_editor;
    QPointer<QWidget> m_reset;
};

void updateColorButton(basicinput::Button* button, const QColor& color)
{
    const qreal dpr = button->devicePixelRatioF();
    QPixmap swatch(qRound(20 * dpr), qRound(20 * dpr));
    swatch.setDevicePixelRatio(dpr);
    swatch.fill(Qt::transparent);
    FluentRasterCachePainter painter(&swatch);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(button->themeColorsRef().strokeDefault);
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(1, 1, 18, 18), 4, 4);
    painter.end();
    button->setIcon(QIcon(swatch));
    button->setIconSize(QSize(20, 20));
    button->setFluentLayout(basicinput::Button::IconBefore);
    button->setText(color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb).toUpper());
    button->setAccessibleDescription(button->text());
}

bool accepts(const PropertyDefinition& definition, const QVariant& value)
{
    if (!value.isValid())
        return false;
    switch (definition.type) {
    case PropertyDefinition::Text:
        return value.userType() == QMetaType::QString;
    case PropertyDefinition::Number: {
        bool ok = false;
        const double number = value.toDouble(&ok);
        const int type = value.userType();
        const bool numeric = type == QMetaType::Double || type == QMetaType::Float || type == QMetaType::Int
            || type == QMetaType::UInt || type == QMetaType::LongLong || type == QMetaType::ULongLong;
        return numeric && ok && std::isfinite(number) && number >= definition.minimum && number <= definition.maximum;
    }
    case PropertyDefinition::Boolean:
        return value.userType() == QMetaType::Bool;
    case PropertyDefinition::Choice:
        return value.userType() == QMetaType::QString && definition.choices.contains(value.toString());
    case PropertyDefinition::Color:
        return value.canConvert<QColor>() && value.value<QColor>().isValid();
    }
    return false;
}

bool equivalentValue(const PropertyDefinition& definition, const QVariant& first, const QVariant& second)
{
    if (!first.isValid() || !second.isValid())
        return first.isValid() == second.isValid();
    if (definition.type == PropertyDefinition::Color)
        return first.value<QColor>() == second.value<QColor>();
    if (definition.type == PropertyDefinition::Number) {
        const double a = first.toDouble(), b = second.toDouble();
        // Ignore floating-point roundoff, capped well below one step so large values keep small edits distinct.
        const double scale = qMax(qMax(std::abs(a), std::abs(b)), std::abs(definition.step));
        const double tolerance = qMin(4 * std::numeric_limits<double>::epsilon() * scale,
            std::abs(definition.step) * 1e-6);
        return a == b || std::abs(a - b) <= tolerance;
    }
    return first == second;
}
} // namespace

class PropertyPanelPrivate {
public:
    struct Entry {
        PropertyDefinition definition;
        bool pending = true;
        QPointer<Field> field;
        QPointer<QWidget> editor;
        QPointer<QWidget> row;
        QPointer<basicinput::Button> reset;
        QPointer<basicinput::ColorPicker> color;
        QPointer<dialogs_flyouts::Flyout> flyout;
    };
    struct Group {
        QString name;
        QWidget* widget;
        QVBoxLayout* layout;
        textfields::Label* heading;
    };
    PropertyPanel* q;
    QVector<Entry> entries;
    QVector<Group> groups;
    textfields::LineEdit* search = nullptr;
    scrolling::ScrollView* scroll = nullptr;
    FormController* form = nullptr;
    bool viewportUpdateQueued = false;
    bool controllerExposed = false;
    int labelWidth = 96;
    bool leading = false;
    bool readOnly = false;
    bool rebuilding = false;
    PropertyPanel::LayoutMode layoutMode = PropertyPanel::LayoutMode::Automatic;
    PropertyPanel::SearchVisibility searchVisibility = PropertyPanel::SearchVisibility::Automatic;
    QStringList modified;
    quint64 revision = 0;
    int index(const QString& key) const
    {
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].definition.key == key)
                return i;
        return -1;
    }
    QWidget* ensureEditor(int index, bool orderNow = true);
    bool tabOrderDirty = false;
    void arrangeTabOrder();
    void scheduleVisibleEditors()
    {
        if (viewportUpdateQueued || !q->isVisible())
            return;
        viewportUpdateQueued = true;
        QTimer::singleShot(0, q, [this] {
            viewportUpdateQueued = false;
            if (!q->isVisible() || rebuilding)
                return;
            const QPointer<PropertyPanel> guard(q);
            const auto generation = revision;
            const auto* viewport = scroll->viewport();
            const QRect visible = viewport->rect().adjusted(0, -40, 0, 40);
            for (int i = 0; i < entries.size(); ++i) {
                const auto& entry = entries.at(i);
                if (entry.pending && entry.row && entry.row->isVisible()
                    && visible.intersects(QRect(entry.row->mapTo(viewport, QPoint()), entry.row->size())))
                    ensureEditor(i, false);
                if (!guard || generation != revision)
                    return;
            }
            arrangeTabOrder();
        });
    }
    void presentation()
    {
        if (!search)
            return;
        const QPointer<PropertyPanel> guard(q);
        const quint64 currentRevision = revision;
        const auto snapshot = entries;
        const auto groupSnapshot = groups;
        const bool showSearch = searchVisibility == PropertyPanel::SearchVisibility::Visible
            || (searchVisibility == PropertyPanel::SearchVisibility::Automatic
                && (entries.size() >= 8 || !search->text().isEmpty()));
        search->setVisible(showSearch);
        if (!guard || revision != currentRevision)
            return;
        q->setFocusProxy(showSearch
                ? static_cast<QWidget*>(search)
                : (entries.isEmpty()
                          ? nullptr
                          : (entries.first().pending ? entries.first().row.data() : entries.first().editor.data())));
        if (!guard || revision != currentRevision)
            return;
        labelWidth = 96;
        int editorWidth = 164;
        for (const auto& entry : snapshot)
            labelWidth = qMax(labelWidth, q->fontMetrics().horizontalAdvance(entry.definition.label));
        for (const auto& entry : snapshot)
            if (entry.editor)
                editorWidth = qMax(editorWidth, entry.editor->minimumWidth());
        labelWidth = qMin(labelWidth, 128);
        leading = layoutMode == PropertyPanel::LayoutMode::Inline
            || (layoutMode == PropertyPanel::LayoutMode::Automatic && q->width() >= labelWidth + editorWidth + 52);
        for (const auto& group : groupSnapshot) {
            if (group.heading)
                group.heading->setAlignment(
                    (q->isRightToLeft() ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter | Qt::AlignAbsolute);
            if (!guard || revision != currentRevision)
                return;
        }
        for (const auto& entry : snapshot) {
            if (entry.pending && entry.row) {
                // Reserve one control plus the same caption/helper space while offscreen.
                const auto& p = entry.definition;
                const int width = qMax(32, scroll->viewport()->width() - 48);
                const auto textHeight = [&](const QString& text) {
                    return q->fontMetrics().boundingRect(QRect(0, 0, width, 0), Qt::TextWordWrap, text).height();
                };
                int height = 32;
                if (!leading)
                    height += textHeight(p.label) + ::Spacing::XSmall;
                if (!p.description.isEmpty())
                    height += textHeight(p.description) + ::Spacing::XSmall;
                entry.row->setMinimumHeight(height);
                continue;
            }
            if (!entry.field)
                continue;
            entry.field->setLabelWidth(labelWidth);
            if (!guard || revision != currentRevision || !entry.field)
                return;
            entry.field->setLabelPosition(leading ? Field::LabelPosition::Leading : Field::LabelPosition::Top);
            if (!guard || revision != currentRevision)
                return;
        }
    }
    void writeValue(const Entry& e, const QVariant& value)
    {
        const QSignalBlocker blocked(e.editor);
        switch (e.definition.type) {
        case PropertyDefinition::Text:
            static_cast<textfields::LineEdit*>(e.editor.data())->setText(value.toString());
            break;
        case PropertyDefinition::Number:
            static_cast<textfields::NumberBox*>(e.editor.data())->setValue(value.toDouble());
            break;
        case PropertyDefinition::Boolean:
            static_cast<basicinput::ToggleSwitch*>(e.editor.data())->setIsOn(value.toBool());
            break;
        case PropertyDefinition::Choice:
            static_cast<basicinput::ComboBox*>(e.editor.data())->setCurrentText(value.toString());
            break;
        case PropertyDefinition::Color: {
            if (!e.color)
                return;
            const QSignalBlocker colorBlocked(e.color);
            e.color->setColor(value.value<QColor>());
            updateColorButton(static_cast<basicinput::Button*>(e.editor.data()), e.color->color());
            break;
        }
        }
    }

    void updateReset(const Entry& entry)
    {
        if (!entry.reset)
            return;
        const QPointer<PropertyPanel> guard(q);
        const bool available = !readOnly && !entry.definition.readOnly && entry.definition.defaultValue.isValid()
            && !equivalentValue(entry.definition, q->value(entry.definition.key), entry.definition.defaultValue);
        if (!available && entry.reset->hasFocus() && entry.editor)
            entry.editor->setFocus(Qt::OtherFocusReason);
        if (!guard || !entry.reset)
            return;
        entry.reset->setEnabled(available);
        entry.reset->setVisible(available);
    }
    void notifyModified()
    {
        const auto current = q->modifiedPropertyKeys();
        if (modified == current)
            return;
        modified = current;
        emit q->modifiedPropertiesChanged();
    }
    void filter()
    {
        const QString query = search->text().trimmed();
        const auto snapshot = entries;
        const QPointer<PropertyPanel> guard(q);
        const quint64 currentRevision = revision;
        for (const auto& entry : snapshot) {
            const auto& p = entry.definition;
            const bool visible = query.isEmpty()
                || (p.label + ' ' + p.group + ' ' + p.description + ' ' + p.key).contains(query, Qt::CaseInsensitive);
            if (!visible && entry.flyout)
                entry.flyout->close();
            if (!guard || revision != currentRevision)
                return;
            if (entry.row)
                entry.row->setVisible(visible);
        }
        for (const auto& group : groups) {
            bool visible = false;
            for (const auto& entry : entries)
                visible |= entry.row && entry.definition.group == group.name && !entry.row->isHidden();
            group.widget->setVisible(visible);
        }
    }
    void edited(const QString& key)
    {
        if (rebuilding)
            return;
        const int i = index(key);
        if (i < 0 || readOnly || entries[i].definition.readOnly)
            return;
        const QVariant current = q->value(key);
        const quint64 currentRevision = revision;
        const auto entry = entries[i];
        const QPointer<PropertyPanel> guard(q);
        updateReset(entry);
        if (!guard || revision != currentRevision)
            return;
        form->markFieldChanged(key);
        if (!guard || revision != currentRevision)
            return;
        form->setFieldTouched(key);
        if (guard && revision == currentRevision && q->value(key) == current)
            emit q->valueEdited(key, current);
        if (guard && revision == currentRevision)
            notifyModified();
    }
    void applyReadOnly()
    {
        const auto snapshot = entries;
        const QPointer<PropertyPanel> guard(q);
        const quint64 currentRevision = revision;
        for (const auto& entry : snapshot) {
            const bool locked = readOnly || entry.definition.readOnly;
            if (entry.pending && entry.row) {
                const bool focusable = !locked || entry.definition.type == PropertyDefinition::Text
                    || entry.definition.type == PropertyDefinition::Number;
                entry.row->setFocusPolicy(focusable ? Qt::TabFocus : Qt::NoFocus);
            }
            if (auto* input = qobject_cast<textfields::LineEdit*>(entry.editor.data()))
                input->setReadOnly(locked);
            else if (entry.editor)
                entry.editor->setEnabled(!locked);
            if (!guard || revision != currentRevision)
                return;
            updateReset(entry);
            if (!guard || revision != currentRevision)
                return;
            if (locked && entry.flyout)
                entry.flyout->close();
            if (!guard || revision != currentRevision)
                return;
        }
    }
};

PropertyPanel::PropertyPanel(QWidget* parent) : QWidget(parent), d(new PropertyPanelPrivate)
{
    d->q = this;
    d->form = new FormController(this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(::Spacing::Small);
    d->search = new textfields::LineEdit(this);
    d->search->setObjectName(QStringLiteral("propertySearch"));
    d->search->setAccessibleName(tr("Search properties"));
    d->search->setPlaceholderText(tr("Search properties"));
    layout->addWidget(d->search);
    d->scroll = new scrolling::ScrollView(this);
    d->scroll->setWidgetResizable(true);
    d->scroll->setFrameShape(QFrame::NoFrame);
    d->scroll->setHorizontalScrollBarVisibility(scrolling::ScrollView::ScrollBarVisibility::Hidden);
    layout->addWidget(d->scroll, 1);
    connect(d->search, &QLineEdit::textChanged, this, [this](const QString& text) {
        const QPointer<PropertyPanel> guard(this);
        d->filter();
        if (guard)
            d->presentation();
        if (guard)
            d->scheduleVisibleEditors();
        if (guard && searchText() == text)
            emit searchTextChanged(text);
    });
    connect(d->scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { d->scheduleVisibleEditors(); });
    d->scroll->viewport()->installEventFilter(this);
    setFocusProxy(d->search);
    setMinimumWidth(180);
    onThemeUpdated();
}

PropertyPanel::~PropertyPanel()
{
    d->rebuilding = true;
    ++d->revision;
    // Open flyouts get reparented to the top-level window while open, so delete them explicitly.
    const auto entries = d->entries;
    for (const auto& entry : entries)
        delete entry.flyout.data();
}

bool PropertyPanel::setProperties(const QVector<PropertyDefinition>& properties)
{
    if (d->rebuilding || properties.size() > 128)
        return false;
    QSet<QString> keys;
    for (const auto& p : properties) {
        if (p.key.trimmed().isEmpty() || keys.contains(p.key) || p.type < PropertyDefinition::Text
            || p.type > PropertyDefinition::Color)
            return false;
        if (p.type == PropertyDefinition::Number
            && (!std::isfinite(p.minimum) || !std::isfinite(p.maximum) || p.minimum > p.maximum
                || !std::isfinite(p.step) || p.step <= 0))
            return false;
        if (p.type == PropertyDefinition::Choice
            && (p.choices.isEmpty() || QSet<QString>(p.choices.cbegin(), p.choices.cend()).size() != p.choices.size()))
            return false;
        if (p.defaultValue.isValid() && !accepts(p, p.defaultValue))
            return false;
        keys.insert(p.key);
    }
    bool identical = properties.size() == d->entries.size();
    for (int i = 0; identical && i < properties.size(); ++i) {
        const auto& a = properties[i];
        const auto& b = d->entries[i].definition;
        identical = a.key == b.key && a.label == b.label && a.group == b.group && a.description == b.description
            && a.type == b.type && a.minimum == b.minimum && a.maximum == b.maximum && a.step == b.step
            && a.choices == b.choices && compatibility::detail::variantValuesEqual(a.defaultValue, b.defaultValue)
            && a.readOnly == b.readOnly;
    }
    if (identical)
        return true;
    d->rebuilding = true;
    ++d->revision;
    // Replacement is a single transaction; no partial fields escape via form notifications.
    QSignalBlocker formSignals(d->form);
    for (const auto& entry : d->entries) {
        d->form->removeField(entry.definition.key);
        if (entry.flyout)
            delete entry.flyout.data();
    }
    d->entries.clear();
    d->groups.clear();
    auto* body = new Card;
    body->setBorderVisible(false);
    body->setThemeOverrides(QJsonObject{{QStringLiteral("radius"), QJsonObject{{QStringLiteral("control"), 0}}}});
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, ::Spacing::Small, 0);
    layout->setSpacing(::Spacing::Medium);
    for (const auto& p : properties) {
        QVBoxLayout* groupLayout = nullptr;
        for (const auto& group : d->groups)
            if (group.name == p.group)
                groupLayout = group.layout;
        if (!groupLayout) {
            auto* groupWidget = new QWidget(body);
            groupLayout = new QVBoxLayout(groupWidget);
            groupLayout->setContentsMargins(0, 0, 0, 0);
            groupLayout->setSpacing(::Spacing::Small);
            textfields::Label* heading = nullptr;
            if (!p.group.isEmpty()) {
                heading = new textfields::Label(p.group, groupWidget);
                heading->setObjectName(QStringLiteral("propertyGroup_") + p.group);
                heading->setFluentTypography(Typography::FontRole::BodyStrong);
                heading->setWordWrap(true);
                groupLayout->addWidget(heading);
            }
            layout->addWidget(groupWidget);
            d->groups.append({p.group, groupWidget, groupLayout, heading});
        }
        PropertyPanelPrivate::Entry entry;
        entry.definition = p;
        auto* row = new QWidget(body);
        entry.row = row;
        const bool deferEditor = properties.size() > 8 && !d->controllerExposed;
        if (deferEditor) {
            row->setFocusPolicy(Qt::TabFocus);
            row->setAccessibleName(p.label);
            row->setProperty("fluentPropertyIndex", d->entries.size());
            row->installEventFilter(this);
        }
        groupLayout->addWidget(row);
        d->entries.append(entry);
        if (!deferEditor)
            d->ensureEditor(d->entries.size() - 1, false);
    }
    layout->addStretch(1);
    d->scroll->setContentWidget(body, WidgetOwnership::Owned);
    body->setAutoFillBackground(false);
    d->form->reset();
    d->arrangeTabOrder();
    d->applyReadOnly();
    d->filter();
    d->presentation();
    d->rebuilding = false;
    d->scheduleVisibleEditors();
    formSignals.unblock();
    const QPointer<PropertyPanel> guard(this);
    d->notifyModified();
    if (!guard)
        return true;
    emit propertiesChanged();
    return true;
}

QWidget* PropertyPanelPrivate::ensureEditor(int index, bool orderNow)
{
    if (index < 0)
        return nullptr;
    if (!entries.at(index).pending)
        return entries.at(index).editor;
    const QPointer<PropertyPanel> guard(q);
    const auto generation = revision;
    entries[index].pending = false;
    auto entry = entries.at(index);
    const auto& p = entry.definition;
    QWidget* row = entry.row;
    row->setMinimumHeight(0);
    auto* rowLayout = new PropertyRowLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(::Spacing::Small);
    entry.field = new Field(row);
    entry.field->setLabelText(p.label);
    entry.field->setHelperText(p.description);
    entry.field->setLabelWidth(labelWidth);
    entry.field->setLabelPosition(leading ? Field::LabelPosition::Leading : Field::LabelPosition::Top);
    rowLayout->addWidget(entry.field, 1);
    const QString key = p.key;
    const auto changed = [this, key] { edited(key); };
    switch (p.type) {
    case PropertyDefinition::Text: {
        auto* input = new textfields::LineEdit(row);
        entry.editor = input;
        QObject::connect(input, &QLineEdit::textEdited, q, changed);
        break;
    }
    case PropertyDefinition::Number: {
        auto* input = new textfields::NumberBox(row);
        input->setRange(p.minimum, p.maximum);
        input->setSmallChange(p.step);
        input->setLargeChange(p.step * 10);
        input->setScrubbingEnabled(true);
        input->setScrubSensitivity(p.step / 10);
        input->setSpinButtonPlacementMode(textfields::NumberBox::SpinButtonPlacementMode::Hidden);
        entry.editor = input;
        QObject::connect(input, &textfields::NumberBox::valueChanged, q, changed);
        break;
    }
    case PropertyDefinition::Boolean: {
        auto* input = new basicinput::ToggleSwitch(row);
        entry.editor = input;
        QObject::connect(input, &basicinput::ToggleSwitch::toggled, q, changed);
        break;
    }
    case PropertyDefinition::Choice: {
        auto* input = new basicinput::ComboBox(row);
        input->addItems(p.choices);
        entry.editor = input;
        QObject::connect(input, qOverload<int>(&QComboBox::currentIndexChanged), q, changed);
        break;
    }
    case PropertyDefinition::Color: {
        auto* input = new basicinput::Button(row);
        auto* flyout = new dialogs_flyouts::Flyout(q);
        auto* picker = new basicinput::ColorPicker(flyout);
        picker->setObjectName(QStringLiteral("propertyColor_") + key);
        picker->setAccessibleName(p.label);
        auto* colorLayout = new QVBoxLayout(flyout);
        colorLayout->setContentsMargins(16, 16, 16, 16);
        colorLayout->addWidget(picker);
        entry.editor = input;
        entry.color = picker;
        entry.flyout = flyout;
        updateColorButton(input, picker->color());
        QObject::connect(input, &QAbstractButton::clicked, flyout, [input, flyout] { flyout->showAt(input); });
        QObject::connect(picker, &basicinput::ColorPicker::colorChanged, q, [input, changed](const QColor& color) {
            updateColorButton(input, color);
            changed();
        });
        break;
    }
    }
    entry.editor->setObjectName(QStringLiteral("propertyEditor_") + key);
    if (p.type != PropertyDefinition::Boolean)
        entry.editor->setMinimumWidth(p.type == PropertyDefinition::Color ? 132 : 112);
    entry.editor->setAccessibleName(p.label);
    entry.field->setEditor(entry.editor, WidgetOwnership::Owned);
    entry.reset = new basicinput::Button(row);
    entry.reset->setObjectName(QStringLiteral("propertyReset_") + key);
    entry.reset->setAccessibleName(PropertyPanel::tr("Reset %1").arg(p.label));
    entry.reset->setToolTip(PropertyPanel::tr("Reset %1").arg(p.label));
    entry.reset->setIconGlyph(Typography::Icons::Undo);
    entry.reset->setFluentLayout(basicinput::Button::IconOnly);
    entry.reset->setFluentStyle(basicinput::Button::Subtle);
    entry.reset->setFixedSize(32, 32);
    auto resetPolicy = entry.reset->sizePolicy();
    resetPolicy.setRetainSizeWhenHidden(true);
    entry.reset->setSizePolicy(resetPolicy);
    entry.reset->hide();
    rowLayout->addWidget(entry.reset, 0, Qt::AlignTop);
    rowLayout->setEditorRow(entry.field, entry.editor, entry.reset);
    QObject::connect(entry.reset, &QAbstractButton::clicked, q, [this, key] { q->resetProperty(key); });
    if (p.defaultValue.isValid())
        writeValue(entry, p.defaultValue);
    const bool locked = readOnly || p.readOnly;
    if (auto* input = qobject_cast<textfields::LineEdit*>(entry.editor.data()))
        input->setReadOnly(locked);
    else
        entry.editor->setEnabled(!locked);
    if (!guard || generation != revision)
        return nullptr;
    entries[index] = entry;
    entry.field->show();
    if (!guard || generation != revision)
        return nullptr;
    // The validation controller enumerates fields in schema order, even if a
    // search result or a programmatic access materialized the last row first.
    int position = 0;
    for (int i = 0; i < index; ++i)
        position += entries.at(i).field != nullptr;
    form->insertField(position, key, entry.field);
    if (!guard || generation != revision)
        return nullptr;
    if (row->hasFocus() && entry.editor->isEnabled())
        entry.editor->setFocus(Qt::OtherFocusReason);
    if (!guard || generation != revision)
        return nullptr;
    row->setFocusPolicy(Qt::NoFocus);
    tabOrderDirty = true;
    if (orderNow)
        arrangeTabOrder();
    if (q->focusProxy() == row)
        q->setFocusProxy(entry.editor);
    return guard && generation == revision ? entry.editor.data() : nullptr;
}

void PropertyPanelPrivate::arrangeTabOrder()
{
    if (!tabOrderDirty)
        return;
    tabOrderDirty = false;
    QWidget* previous = search;
    for (const auto& item : entries) {
        QWidget* next = item.pending ? item.row.data() : item.editor.data();
        if (next) {
            QWidget::setTabOrder(previous, next);
            previous = next;
        }
        if (item.reset) {
            QWidget::setTabOrder(previous, item.reset);
            previous = item.reset;
        }
    }
}

bool PropertyPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusIn && watched->property("fluentPropertyIndex").isValid()) {
        const int i = watched->property("fluentPropertyIndex").toInt();
        if (i >= 0 && i < d->entries.size() && watched == d->entries.at(i).row && d->entries.at(i).pending) {
            const auto reason = static_cast<QFocusEvent*>(event)->reason();
            if (auto* input = d->ensureEditor(i))
                input->setFocus(reason);
            return true;
        }
    } else if (watched == d->scroll->viewport()
        && (event->type() == QEvent::Show || event->type() == QEvent::Resize
            || event->type() == QEvent::LayoutRequest)) {
        d->scheduleVisibleEditors();
    }
    return QWidget::eventFilter(watched, event);
}

QVector<PropertyDefinition> PropertyPanel::properties() const
{
    QVector<PropertyDefinition> result;
    for (const auto& entry : d->entries)
        result.append(entry.definition);
    return result;
}
QStringList PropertyPanel::propertyKeys() const
{
    QStringList result;
    for (const auto& entry : d->entries)
        result.append(entry.definition.key);
    return result;
}
QVariant PropertyPanel::value(const QString& key) const
{
    const int i = d->index(key);
    if (i < 0)
        return {};
    const auto& definition = d->entries.at(i).definition;
    if (d->entries.at(i).pending && definition.defaultValue.isValid() && definition.type != PropertyDefinition::Number)
        return definition.defaultValue;
    if (!d->ensureEditor(i))
        return {};
    const auto& e = d->entries[i];
    switch (e.definition.type) {
    case PropertyDefinition::Text:
        return static_cast<textfields::LineEdit*>(e.editor.data())->text();
    case PropertyDefinition::Number:
        return static_cast<textfields::NumberBox*>(e.editor.data())->value();
    case PropertyDefinition::Boolean:
        return static_cast<basicinput::ToggleSwitch*>(e.editor.data())->isOn();
    case PropertyDefinition::Choice:
        return static_cast<basicinput::ComboBox*>(e.editor.data())->currentText();
    case PropertyDefinition::Color:
        return e.color ? QVariant(e.color->color()) : QVariant();
    }
    return {};
}
bool PropertyPanel::setValue(const QString& key, const QVariant& value)
{
    const int i = d->index(key);
    if (i < 0 || !accepts(d->entries[i].definition, value))
        return false;
    if (!d->ensureEditor(i))
        return false;
    const auto e = d->entries[i];
    const QPointer<PropertyPanel> guard(this);
    const quint64 revision = d->revision;
    if (auto* number = qobject_cast<textfields::NumberBox*>(e.editor.data()))
        number->cancelScrub();
    if (!guard || revision != d->revision || !e.editor)
        return false;
    if (compatibility::detail::variantValuesEqual(this->value(key), value))
        return true;
    d->writeValue(e, value);
    d->updateReset(e);
    if (!guard || revision != d->revision)
        return true;
    if (!d->rebuilding) {
        d->notifyModified();
        if (!guard || revision != d->revision)
            return true;
        d->form->markFieldChanged(key);
    }
    return true;
}
bool PropertyPanel::resetProperty(const QString& key)
{
    const int i = d->index(key);
    if (i < 0 || d->readOnly || d->entries[i].definition.readOnly)
        return false;
    const QVariant initial = d->entries[i].definition.defaultValue;
    if (!initial.isValid())
        return false;
    if (equivalentValue(d->entries[i].definition, value(key), initial))
        return true;
    const QPointer<PropertyPanel> guard(this);
    const quint64 revision = d->revision;
    if (!setValue(key, initial) || !guard)
        return false;
    if (d->revision != revision)
        return true;
    d->edited(key);
    return true;
}
QString PropertyPanel::searchText() const
{
    return d->search->text();
}
QStringList PropertyPanel::modifiedPropertyKeys() const
{
    QStringList result;
    for (const auto& entry : d->entries)
        if (!entry.pending && entry.definition.defaultValue.isValid()
            && !equivalentValue(entry.definition, value(entry.definition.key), entry.definition.defaultValue))
            result.append(entry.definition.key);
    return result;
}
void PropertyPanel::resetAll()
{
    const auto keys = modifiedPropertyKeys();
    const QPointer<PropertyPanel> guard(this);
    const quint64 revision = d->revision;
    for (const auto& key : keys) {
        resetProperty(key);
        if (!guard || d->revision != revision)
            return;
    }
}
PropertyPanel::LayoutMode PropertyPanel::layoutMode() const
{
    return d->layoutMode;
}
void PropertyPanel::setLayoutMode(LayoutMode mode)
{
    if ((mode != LayoutMode::Automatic && mode != LayoutMode::Stacked && mode != LayoutMode::Inline)
        || d->layoutMode == mode)
        return;
    d->layoutMode = mode;
    const QPointer<PropertyPanel> guard(this);
    d->presentation();
    if (guard && d->layoutMode == mode)
        emit layoutModeChanged(mode);
}
PropertyPanel::SearchVisibility PropertyPanel::searchVisibility() const
{
    return d->searchVisibility;
}
void PropertyPanel::setSearchVisibility(SearchVisibility visibility)
{
    if ((visibility != SearchVisibility::Automatic && visibility != SearchVisibility::Visible
            && visibility != SearchVisibility::Hidden)
        || d->searchVisibility == visibility)
        return;
    d->searchVisibility = visibility;
    const QPointer<PropertyPanel> guard(this);
    d->presentation();
    if (guard && d->searchVisibility == visibility)
        emit searchVisibilityChanged(visibility);
}
void PropertyPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    d->presentation();
}
void PropertyPanel::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LayoutDirectionChange || event->type() == QEvent::FontChange)
        d->presentation();
}
void PropertyPanel::setSearchText(const QString& text)
{
    d->search->setText(text);
}
bool PropertyPanel::isReadOnly() const
{
    return d->readOnly;
}
void PropertyPanel::setReadOnly(bool readOnly)
{
    if (d->readOnly == readOnly)
        return;
    const QPointer<PropertyPanel> guard(this);
    const auto snapshot = d->entries;
    for (const auto& entry : snapshot) {
        if (auto* number = qobject_cast<textfields::NumberBox*>(entry.editor.data()))
            number->cancelScrub();
        if (!guard)
            return;
    }
    d->readOnly = readOnly;
    d->applyReadOnly();
    if (guard && d->readOnly == readOnly)
        emit readOnlyChanged(readOnly);
}
QWidget* PropertyPanel::editor(const QString& key) const
{
    const int i = d->index(key);
    return d->ensureEditor(i);
}
Field* PropertyPanel::field(const QString& key) const
{
    const int i = d->index(key);
    if (i < 0 || (d->entries.at(i).pending && !d->ensureEditor(i)))
        return nullptr;
    return d->entries.at(i).field;
}
FormController* PropertyPanel::formController() const
{
    if (d->controllerExposed)
        return d->form;
    d->controllerExposed = true;
    const QPointer<FormController> form(d->form);
    const QPointer<const PropertyPanel> guard(this);
    const auto revision = d->revision;
    for (int i = 0; i < d->entries.size(); ++i) {
        d->ensureEditor(i, false);
        if (!guard || revision != d->revision)
            return form;
    }
    d->arrangeTabOrder();
    return form;
}
textfields::LineEdit* PropertyPanel::searchField() const
{
    return d->search;
}
QSize PropertyPanel::sizeHint() const
{
    const int contentHeight = d->scroll && d->scroll->widget() ? d->scroll->widget()->sizeHint().height() : 120;
    return QSize(360, qBound(120, contentHeight + (d->search->isHidden() ? 0 : 40), 600));
}
void PropertyPanel::onThemeUpdated()
{
    if (d->scroll) {
        const QJsonObject colors{{QStringLiteral("bgCanvas"), themeColorsRef().bgLayer.name()}};
        d->scroll->setThemeOverrides(QJsonObject{{QStringLiteral("light"), colors}, {QStringLiteral("dark"), colors},
            {QStringLiteral("contrast"), colors}});
    }
    for (const auto& entry : d->entries)
        if (entry.color && entry.editor)
            updateColorButton(static_cast<basicinput::Button*>(entry.editor.data()), entry.color->color());
    if (d->search)
        d->presentation();
    update();
}
} // namespace fluent::layout

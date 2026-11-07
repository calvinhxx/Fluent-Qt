#include "Transfer.h"
#include "ListView.h"
#include "private/CollectionItemState_p.h"
#include "components/basicinput/Button.h"
#include "components/foundation/private/SelectionCheckIndicator_p.h"
#include "components/layout/Card.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "design/Spacing.h"
#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QEvent>
#include <QHash>
#include <QItemSelectionModel>
#include <QPointer>
#include <QResizeEvent>
#include <QSet>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <algorithm>
#include <functional>

namespace fluent::collections {
namespace {

bool canTransfer(const QModelIndex& index)
{
    return index.isValid() && index.flags().testFlag(Qt::ItemIsEnabled) && index.flags().testFlag(Qt::ItemIsSelectable);
}

class TransferFilter final : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    std::function<bool(const QModelIndex&)> accepts;
    void refresh() { invalidateFilter(); }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        return !parent.isValid() && sourceModel() && accepts && accepts(sourceModel()->index(row, 0, parent));
    }
};

class TransferLayout final : public QBoxLayout {
public:
    explicit TransferLayout(Transfer* transfer) : QBoxLayout(LeftToRight, transfer), m_transfer(transfer)
    {
        setSizeConstraint(QLayout::SetNoConstraint);
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return m_transfer->heightForWidth(width); }
    QSize minimumSize() const override { return QSize(248, 280); }

private:
    Transfer* m_transfer;
};

class TransferDelegate final : public QStyledItemDelegate {
public:
    explicit TransferDelegate(ListView* view) : QStyledItemDelegate(view), m_view(view), m_base(view->itemDelegate()) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!painter || !m_base)
            return;
        QStyleOptionViewItem textOption(option);
        // The wrapper owns the full-row selection surface; reuse the base
        // delegate for text and descriptions without stacking translucent fills.
        // zh_CN: 包装层负责完整行选中表面，基类仅绘制文字和描述，避免半透明背景叠加。
        const auto visual = detail::collectionItemVisualStyle(option.state, m_view->themeColorsRef());
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (visual.background.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(visual.background);
            painter->drawRoundedRect(QRectF(option.rect).adjusted(2, 1, -2, -1), m_view->themeRadius().control,
                m_view->themeRadius().control);
        }
        painter->restore();
        textOption.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_Sunken);
        const int reserve = ::Spacing::Large;
        if (option.direction == Qt::RightToLeft)
            textOption.rect.adjust(0, 0, -reserve, 0);
        else
            textOption.rect.adjust(reserve, 0, 0, 0);
        m_base->paint(painter, textOption, index);
        const int extent = 18;
        const QRect logical(option.rect.left() + ::Spacing::Small,
            option.rect.top() + (option.rect.height() - extent) / 2, extent, extent);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        auto colors = m_view->themeColors();
        colors.strokeDefault = colors.strokeStrong;
        if (m_view->effectiveTheme() != FluentElement::HighContrast)
            colors.strokeDefault.setAlphaF(colors.strokeDefault.alphaF() * 0.65);
        ::fluent::detail::paintSelectionCheckIndicator(*painter,
            QStyle::visualRect(option.direction, option.rect, logical), colors, m_view->themeRadius().control,
            option.state.testFlag(QStyle::State_Selected), option.state.testFlag(QStyle::State_MouseOver),
            option.state.testFlag(QStyle::State_Enabled) && canTransfer(index));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QSize base = m_base ? m_base->sizeHint(option, index) : QSize();
        return QSize(base.width() + ::Spacing::Large, qMax(::Spacing::ControlHeight::Large, base.height()));
    }

private:
    ListView* m_view;
    QPointer<QAbstractItemDelegate> m_base;
};

} // namespace

class Transfer::Private {
public:
    struct Pane {
        layout::Card* card = nullptr;
        textfields::Label* title = nullptr;
        textfields::Label* count = nullptr;
        textfields::AutoSuggestBox* search = nullptr;
        ListView* view = nullptr;
        TransferFilter* proxy = nullptr;
    };

    explicit Private(Transfer* owner) : q(owner) {}

    void makePane(Pane& pane, const QString& name, bool target)
    {
        pane.card = new layout::Card(q);
        auto* box = new QVBoxLayout(pane.card);
        box->setContentsMargins(::Spacing::Medium, ::Spacing::Medium, ::Spacing::Medium, ::Spacing::Medium);
        box->setSpacing(::Spacing::Small);
        auto* heading = new QHBoxLayout;
        pane.title = new textfields::Label(pane.card);
        pane.title->setFluentTypography(Typography::FontRole::BodyStrong);
        pane.count = new textfields::Label(pane.card);
        pane.count->setFluentTypography(Typography::FontRole::Caption);
        pane.count->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        heading->addWidget(pane.title, 1);
        heading->addWidget(pane.count);
        box->addLayout(heading);
        pane.search = new textfields::AutoSuggestBox(pane.card);
        pane.search->setObjectName(QStringLiteral("Transfer.%1.Search").arg(name));
        pane.search->setQueryButtonPlacement(textfields::AutoSuggestBox::QueryButtonPlacement::Leading);
        pane.search->setClearButtonEnabled(true);
        box->addWidget(pane.search);
        pane.view = new ListView(pane.card);
        pane.view->setObjectName(QStringLiteral("Transfer.%1.List").arg(name));
        pane.view->setSelectionMode(SelectionMode::Multiple);
        pane.view->setSelectionIndicatorVisible(false);
        pane.view->setBorderVisible(false);
        pane.view->setBackgroundVisible(false);
        // The Card owns this surface, including inside a composited window.
        // zh_CN: 包括合成窗口在内，此处的背景均由父 Card 绘制。
        pane.view->setProperty("fluentPreserveParentSurface", true);
        pane.view->setMinimumSize(200, 160);
        pane.view->setUniformItemSizes(true);
        pane.view->setItemDelegate(new TransferDelegate(pane.view));
        pane.title->setBuddy(pane.view);
        box->addWidget(pane.view, 1);
        pane.proxy = new TransferFilter(q);
        pane.proxy->accepts = [this, target](const QModelIndex& index) {
            const QString key = index.data(keyRole).toString();
            if (!knownKeys.contains(key) || selectedSet.contains(key) != target)
                return false;
            const QString query = target ? targetFilter : sourceFilter;
            const QString haystack = index.data(Qt::DisplayRole).toString() + QLatin1Char(' ')
                + (descriptionRole >= 0 ? index.data(descriptionRole).toString() : QString());
            for (const QString& word : query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
                if (!haystack.contains(word, Qt::CaseInsensitive))
                    return false;
            }
            return true;
        };
        pane.view->setModel(pane.proxy);
        QObject::connect(pane.view->selectionModel(), &QItemSelectionModel::selectionChanged, q, [this]() {
            updateActions();
        });
        QObject::connect(pane.search, &QLineEdit::textChanged, q, [this, target](const QString& value) {
            if (target)
                q->setTargetFilter(value);
            else
                q->setSourceFilter(value);
        });
    }

    void initialize()
    {
        outer = new TransferLayout(q);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(::Spacing::Standard);
        makePane(source, QStringLiteral("Source"), false);
        makePane(target, QStringLiteral("Target"), true);
        actions = new QWidget(q);
        actionLayout = new QBoxLayout(QBoxLayout::TopToBottom, actions);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        actionLayout->setSpacing(::Spacing::Small);
        add = new basicinput::Button(actions);
        remove = new basicinput::Button(actions);
        for (auto* button : {add, remove}) {
            button->setFluentLayout(basicinput::Button::IconOnly);
            button->setFixedSize(::Spacing::ControlHeight::Standard, ::Spacing::ControlHeight::Standard);
        }
        add->setObjectName(QStringLiteral("Transfer.Add"));
        remove->setObjectName(QStringLiteral("Transfer.Remove"));
        add->setFluentStyle(basicinput::Button::Accent);
        actionLayout->addWidget(add);
        actionLayout->addWidget(remove);
        outer->addWidget(source.card, 1);
        outer->addWidget(actions, 0, Qt::AlignCenter);
        outer->addWidget(target.card, 1);
        QObject::connect(add, &QAbstractButton::clicked, q, &Transfer::addSelected);
        QObject::connect(remove, &QAbstractButton::clicked, q, &Transfer::removeSelected);
        updateText();
        updateLayout();
    }

    int breakpoint() const
    {
        return source.card->minimumSizeHint().width() + target.card->minimumSizeHint().width() + add->width()
            + ::Spacing::Standard * 2;
    }

    void updateLayout()
    {
        const bool narrow = q->width() < breakpoint();
        outer->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        actionLayout->setDirection(narrow ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
        const bool rtl = q->layoutDirection() == Qt::RightToLeft;
        add->setIconGlyph(narrow ? Typography::Icons::Down
                : rtl            ? Typography::Icons::Back
                                 : Typography::Icons::Forward);
        remove->setIconGlyph(narrow ? Typography::Icons::Up
                : rtl               ? Typography::Icons::Forward
                                    : Typography::Icons::Back);
        q->updateGeometry();
    }

    void updateText()
    {
        source.title->setText(sourceHeader);
        target.title->setText(targetHeader);
        source.view->setAccessibleName(sourceHeader);
        target.view->setAccessibleName(targetHeader);
        source.search->setAccessibleName(q->tr("Search %1").arg(sourceHeader));
        target.search->setAccessibleName(q->tr("Search %1").arg(targetHeader));
        source.search->setPlaceholderText(placeholder);
        target.search->setPlaceholderText(placeholder);
        source.view->setPlaceholderText(
            sourceFilter.isEmpty() ? q->tr("No available items") : q->tr("No matching items"));
        target.view->setPlaceholderText(
            targetFilter.isEmpty() ? q->tr("No selected items") : q->tr("No matching items"));
        source.count->setText(sourceFilter.isEmpty()
                ? QString::number(q->availableCount())
                : q->tr("%1 / %2").arg(source.proxy->rowCount()).arg(q->availableCount()));
        target.count->setText(targetFilter.isEmpty()
                ? QString::number(q->selectedCount())
                : q->tr("%1 / %2").arg(target.proxy->rowCount()).arg(q->selectedCount()));
        updateActions();
    }

    QStringList checkedKeys(const Pane& pane) const
    {
        auto rows = pane.view->selectionModel()->selectedRows();
        std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) {
            return a.row() < b.row();
        });
        QStringList keys;
        for (const auto& index : rows) {
            if (canTransfer(index))
                keys.append(index.data(keyRole).toString());
        }
        return keys;
    }

    void updateActions()
    {
        const int sourceCount = checkedKeys(source).size();
        const int targetCount = checkedKeys(target).size();
        add->setEnabled(sourceCount > 0);
        remove->setEnabled(targetCount > 0);
        const auto describe = [this](basicinput::Button* button, int count, const QString& destination) {
            QString text;
            if (count == 1)
                text = q->tr("Move 1 selected item to %1").arg(destination);
            else if (count > 1)
                text = q->tr("Move %1 selected items to %2").arg(count).arg(destination);
            else
                text = q->tr("Move selected items to %1").arg(destination);
            button->setToolTip(text);
            button->setAccessibleName(text);
        };
        describe(add, sourceCount, targetHeader);
        describe(remove, targetCount, sourceHeader);
    }

    void refreshFilters()
    {
        source.proxy->refresh();
        target.proxy->refresh();
        updateText();
    }

    void refreshModel()
    {
        const int oldAvailable = q->availableCount();
        const int oldSelected = q->selectedCount();
        QHash<QString, int> occurrences;
        if (model) {
            for (int row = 0; row < model->rowCount(); ++row) {
                const QString key = model->index(row, 0).data(keyRole).toString();
                if (!key.isEmpty())
                    ++occurrences[key];
            }
        }
        knownKeys.clear();
        for (auto it = occurrences.cbegin(); it != occurrences.cend(); ++it) {
            if (it.value() == 1)
                knownKeys.insert(it.key());
        }
        const QStringList previous = selected;
        selected.erase(std::remove_if(selected.begin(), selected.end(),
                           [this](const QString& key) { return !knownKeys.contains(key); }),
            selected.end());
        selectedSet = QSet<QString>(selected.cbegin(), selected.cend());
        refreshFilters();
        const QStringList membership = selected;
        QPointer<Transfer> guard(q);
        if (oldAvailable != q->availableCount() || oldSelected != q->selectedCount())
            emit q->countsChanged();
        if (guard && previous != membership && guard->selectedKeys() == membership)
            emit guard->selectedKeysChanged(membership);
    }

    Transfer* q;
    QPointer<QAbstractItemModel> model;
    QVector<QMetaObject::Connection> connections;
    QSet<QString> knownKeys;
    QSet<QString> selectedSet;
    QStringList selected;
    int keyRole = Qt::UserRole;
    int descriptionRole = -1;
    QString sourceHeader = Transfer::tr("Available");
    QString targetHeader = Transfer::tr("Selected");
    QString sourceFilter, targetFilter;
    QString placeholder = Transfer::tr("Search items");
    Pane source, target;
    QBoxLayout* outer = nullptr;
    QWidget* actions = nullptr;
    QBoxLayout* actionLayout = nullptr;
    basicinput::Button* add = nullptr;
    basicinput::Button* remove = nullptr;
};

Transfer::Transfer(QWidget* parent) : QWidget(parent), d(std::make_unique<Private>(this))
{
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    d->initialize();
    setFocusProxy(d->source.search);
}

Transfer::~Transfer()
{
    for (const auto& connection : d->connections)
        disconnect(connection);
    disconnect(d->source.search, nullptr, this, nullptr);
    disconnect(d->target.search, nullptr, this, nullptr);
    disconnect(d->source.view->selectionModel(), nullptr, this, nullptr);
    disconnect(d->target.view->selectionModel(), nullptr, this, nullptr);
    d->source.proxy->accepts = {};
    d->target.proxy->accepts = {};
    // Destroy views/proxies while their callbacks can still read Private.
    // zh_CN: 在 Private 仍可供回调读取时销毁视图及代理。
    delete d->source.card;
    delete d->target.card;
    delete d->source.proxy;
    delete d->target.proxy;
}

QAbstractItemModel* Transfer::model() const
{
    return d->model;
}
void Transfer::setModel(QAbstractItemModel* model)
{
    if (d->model == model)
        return;
    for (const auto& c : d->connections)
        disconnect(c);
    d->connections.clear();
    d->model = model;
    d->source.proxy->setSourceModel(model);
    d->target.proxy->setSourceModel(model);
    if (model) {
        const auto refresh = [this]() { d->refreshModel(); };
        d->connections
            << connect(model, &QAbstractItemModel::rowsInserted, this, refresh)
            << connect(model, &QAbstractItemModel::rowsRemoved, this, refresh)
            << connect(model, &QAbstractItemModel::rowsMoved, this, refresh)
            << connect(model, &QAbstractItemModel::modelReset, this, refresh)
            << connect(model, &QAbstractItemModel::layoutChanged, this, refresh)
            << connect(model, &QAbstractItemModel::dataChanged, this,
                   [this](const QModelIndex&, const QModelIndex&, const QVector<int>& roles) {
                       if (roles.isEmpty() || roles.contains(d->keyRole))
                           d->refreshModel();
                       else
                           d->refreshFilters();
                   })
            << connect(model, &QObject::destroyed, this, [this]() {
                   QPointer<Transfer> guard(this);
                   d->model = nullptr;
                   d->source.proxy->setSourceModel(nullptr);
                   d->target.proxy->setSourceModel(nullptr);
                   d->refreshModel();
                   if (guard)
                       emit modelChanged();
               });
    }
    QPointer<Transfer> guard(this);
    d->refreshModel();
    if (guard)
        emit modelChanged();
}

int Transfer::keyRole() const
{
    return d->keyRole;
}
void Transfer::setKeyRole(int role)
{
    if (d->keyRole == role || role < 0)
        return;
    d->keyRole = role;
    QPointer<Transfer> guard(this);
    d->refreshModel();
    if (guard)
        emit keyRoleChanged();
}
int Transfer::descriptionRole() const
{
    return d->descriptionRole;
}
void Transfer::setDescriptionRole(int role)
{
    role = qMax(-1, role);
    if (d->descriptionRole == role)
        return;
    d->descriptionRole = role;
    d->source.view->setTrailingTextRole(role);
    d->target.view->setTrailingTextRole(role);
    d->refreshFilters();
    emit descriptionRoleChanged();
}
QStringList Transfer::selectedKeys() const
{
    return d->selected;
}
void Transfer::setSelectedKeys(const QStringList& keys)
{
    QStringList normalized;
    QSet<QString> unique;
    for (const auto& key : keys) {
        if (d->knownKeys.contains(key) && !unique.contains(key)) {
            normalized.append(key);
            unique.insert(key);
        }
    }
    if (normalized == d->selected)
        return;
    const bool countChanged = normalized.size() != d->selected.size();
    d->selected = normalized;
    d->selectedSet = unique;
    d->refreshFilters();
    QPointer<Transfer> guard(this);
    if (countChanged)
        emit countsChanged();
    if (guard && guard->selectedKeys() == normalized)
        emit selectedKeysChanged(normalized);
}
int Transfer::selectedCount() const
{
    return d->selected.size();
}
int Transfer::availableCount() const
{
    return d->knownKeys.size() - d->selected.size();
}
QString Transfer::sourceHeader() const
{
    return d->sourceHeader;
}
void Transfer::setSourceHeader(const QString& text)
{
    if (d->sourceHeader == text)
        return;
    d->sourceHeader = text;
    d->updateText();
    emit sourceHeaderChanged();
}
QString Transfer::targetHeader() const
{
    return d->targetHeader;
}
void Transfer::setTargetHeader(const QString& text)
{
    if (d->targetHeader == text)
        return;
    d->targetHeader = text;
    d->updateText();
    emit targetHeaderChanged();
}
QString Transfer::sourceFilter() const
{
    return d->sourceFilter;
}
void Transfer::setSourceFilter(const QString& text)
{
    if (d->sourceFilter == text)
        return;
    d->sourceFilter = text;
    const QSignalBlocker block(d->source.search);
    d->source.search->setText(text);
    d->refreshFilters();
    emit sourceFilterChanged();
}
QString Transfer::targetFilter() const
{
    return d->targetFilter;
}
void Transfer::setTargetFilter(const QString& text)
{
    if (d->targetFilter == text)
        return;
    d->targetFilter = text;
    const QSignalBlocker block(d->target.search);
    d->target.search->setText(text);
    d->refreshFilters();
    emit targetFilterChanged();
}
QString Transfer::searchPlaceholderText() const
{
    return d->placeholder;
}
void Transfer::setSearchPlaceholderText(const QString& text)
{
    if (d->placeholder == text)
        return;
    d->placeholder = text;
    d->updateText();
    emit searchPlaceholderTextChanged();
}
ListView* Transfer::sourceView() const
{
    return d->source.view;
}
ListView* Transfer::targetView() const
{
    return d->target.view;
}
void Transfer::addSelected()
{
    if (isEnabled())
        setSelectedKeys(d->selected + d->checkedKeys(d->source));
}
void Transfer::removeSelected()
{
    if (!isEnabled())
        return;
    const auto keys = d->checkedKeys(d->target);
    auto remaining = d->selected;
    for (const auto& key : keys)
        remaining.removeAll(key);
    setSelectedKeys(remaining);
}
QSize Transfer::sizeHint() const
{
    return QSize(740, heightForWidth(740));
}
QSize Transfer::minimumSizeHint() const
{
    return QSize(248, 280);
}
int Transfer::heightForWidth(int width) const
{
    const int paneHeight = qMax(d->source.card->sizeHint().height(), d->target.card->sizeHint().height());
    return width < d->breakpoint() ? paneHeight * 2 + d->add->height() + ::Spacing::Standard * 2 : paneHeight;
}
void Transfer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    d->updateLayout();
}
void Transfer::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LayoutDirectionChange || event->type() == QEvent::FontChange)
        d->updateLayout();
}
void Transfer::onThemeUpdated()
{
    if (d && d->source.view) {
        d->source.view->viewport()->update();
        d->target.view->viewport()->update();
        updateGeometry();
    }
}

} // namespace fluent::collections

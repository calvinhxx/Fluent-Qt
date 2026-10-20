#include "CommandPalette.h"

#include <QAbstractListModel>
#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QFontMetricsF>
#include <QHash>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QtMath>

#include "components/collections/ListView.h"
#include "components/collections/private/CollectionItemState_p.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/foundation/FontIcon.h"
#include "components/menus_toolbars/private/CommandActionModel_p.h"
#include "components/textfields/LineEdit.h"
#include "design/Spacing.h"

namespace fluent::menus_toolbars {
namespace {

QString commandLabel(const QAction* action)
{
    QString text = action->text().section(QLatin1Char('\t'), 0, 0);
    if (text.isEmpty())
        text = action->iconText();
    // Keep escaped ampersands while removing mnemonic markers.
    // zh_CN: 保留转义的 &，移除快捷访问标记。
    text.replace(QStringLiteral("&&"), QString(QChar(0x1f)));
    text.remove(QLatin1Char('&'));
    text.replace(QChar(0x1f), QLatin1Char('&'));
    return text.trimmed();
}

bool isLeaf(const QAction* action)
{
    return action && !action->isSeparator() && !action->menu() && !qobject_cast<const QWidgetAction*>(action);
}

struct SearchMetadata {
    QString group;
    QStringList keywords;
};

class PaletteModel final : public QAbstractListModel {
public:
    explicit PaletteModel(QObject* parent) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : rows.size();
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rows.size() || !rows.at(index.row()))
            return {};
        const QAction* action = rows.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
            return commandLabel(action);
        case Qt::DecorationRole:
            return action->icon();
        case Qt::ToolTipRole:
            return action->toolTip();
        case Qt::CheckStateRole:
            return action->isCheckable() ? QVariant(action->isChecked() ? Qt::Checked : Qt::Unchecked) : QVariant();
        case Qt::AccessibleTextRole:
            return QStringList(
                {groups.value(index.row()), commandLabel(action),
                    action->shortcut().toString(QKeySequence::NativeText)})
                .join(QStringLiteral(" "))
                .trimmed();
        case Qt::AccessibleDescriptionRole:
            return action->statusTip();
        case Qt::UserRole:
            return action->shortcut().toString(QKeySequence::NativeText);
        case Qt::UserRole + 1:
            return index.row() == 0 || groups.value(index.row()) != groups.value(index.row() - 1)
                ? groups.value(index.row())
                : QString();
        default:
            return {};
        }
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rows.size() || !rows.at(index.row()))
            return Qt::NoItemFlags;
        return rows.at(index.row())->isEnabled() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
    }

    void replace(const QList<QPointer<QAction>>& actions, const QStringList& labels)
    {
        beginResetModel();
        rows = actions;
        groups = labels;
        endResetModel();
    }

    QList<QPointer<QAction>> rows;
    QStringList groups;
};

class PaletteDelegate final : public QStyledItemDelegate {
public:
    explicit PaletteDelegate(collections::ListView* view) : QStyledItemDelegate(view), m_view(view) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return QSize(240,
            qMax(::Spacing::ControlHeight::Standard + ::Spacing::XSmall, option.fontMetrics.height() + 12)
                + headerHeight(index));
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const auto& colors = m_view->themeColorsRef();
        const auto visual = collections::detail::collectionItemVisualStyle(option.state, colors);
        const int header = headerHeight(index);
        if (header > 0) {
            painter->setFont(m_view->themeFont(Typography::FontRole::Caption).toQFont());
            painter->setPen(colors.textSecondary);
            const int separator = index.row() > 0 ? ::Spacing::Small : 0;
            if (separator) {
                painter->setPen(colors.strokeDivider);
                painter->drawLine(option.rect.left() + ::Spacing::Small, option.rect.top() + 3,
                    option.rect.right() - ::Spacing::Small, option.rect.top() + 3);
                painter->setPen(colors.textSecondary);
            }
            const QRect heading = option.rect.adjusted(::Spacing::Medium, separator, -::Spacing::Medium, 0);
            painter->drawText(QRect(heading.left(), heading.top(), heading.width(), header - separator),
                Qt::AlignVCenter | Qt::AlignLeading, index.data(Qt::UserRole + 1).toString());
        }
        const QRect card = option.rect.adjusted(2, header + 1, -2, -1);
        painter->setPen(Qt::NoPen);
        painter->setBrush(visual.background);
        painter->drawRoundedRect(card, m_view->themeRadius().control, m_view->themeRadius().control);
        const auto visualRect = [&](QRect rect) { return QStyle::visualRect(option.direction, card, rect); };
        if (option.state & QStyle::State_Selected) {
            painter->setBrush(colors.accentDefault);
            painter->drawRoundedRect(visualRect(QRect(card.left() + 3, card.center().y() - 8, 3, 16)), 1.5, 1.5);
        }
        const bool enabled = option.state & QStyle::State_Enabled;
        const int inset = ::Spacing::Medium;
        const QRect iconRect(card.left() + inset, card.center().y() - 10, 20, 20);
        const auto check = index.data(Qt::CheckStateRole);
        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (check.isValid() && check.toInt() == Qt::Checked) {
            painter->setPen(enabled ? colors.textPrimary : colors.textDisabled);
            Typography::Icons::paintGlyph(*painter, visualRect(iconRect), Typography::Icons::CheckMark, 16,
                Qt::AlignCenter);
        } else {
            icon.paint(painter, visualRect(iconRect), Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);
        }
        painter->setFont(option.font);
        const QString shortcut = index.data(Qt::UserRole).toString();
        // Round fractional glyph advances up; an exact integer advance can elide
        // a complete native shortcut (for example, Command+C) to just "...".
        // zh_CN: 字形宽度向上取整，避免完整的原生快捷键被误省略。
        const int shortcutWidth = qMin(qCeil(QFontMetricsF(option.font).horizontalAdvance(shortcut)) + 2,
            qMax(0, card.width() / 3));
        QRect shortcutRect(card.right() - inset - shortcutWidth, card.top(), shortcutWidth, card.height());
        painter->setPen(enabled ? colors.textSecondary : colors.textDisabled);
        painter->drawText(visualRect(shortcutRect), Qt::AlignVCenter | Qt::AlignLeading | Qt::TextForceLeftToRight,
            option.fontMetrics.elidedText(shortcut, Qt::ElideRight, shortcutWidth));
        const int textLeft =
            !icon.isNull() || check.isValid() ? iconRect.right() + ::Spacing::Small : card.left() + inset;
        QRect textRect(textLeft, card.top(), qMax(0, shortcutRect.left() - textLeft - ::Spacing::Small), card.height());
        painter->setPen(visual.foreground);
        painter->drawText(visualRect(textRect), Qt::AlignVCenter | Qt::AlignLeading,
            option.fontMetrics.elidedText(index.data().toString(), Qt::ElideRight, textRect.width()));
        painter->restore();
    }

private:
    int headerHeight(const QModelIndex& index) const
    {
        return index.data(Qt::UserRole + 1).toString().isEmpty()
            ? 0
            : QFontMetrics(m_view->themeFont(Typography::FontRole::Caption).toQFont()).height() + ::Spacing::Small
                + (index.row() > 0 ? ::Spacing::Small : 0);
    }

    collections::ListView* m_view;
};

} // namespace

class CommandPalettePrivate {
public:
    explicit CommandPalettePrivate(CommandPalette* owner) : q(owner), registry(owner)
    {
        q->setObjectName(QStringLiteral("FluentCommandPalette"));
        q->setAccessibleName(CommandPalette::tr("Command palette"));
        q->setProperty("_fluentqt_commandActionPresentation", true);
        q->setModal(true);
        q->setDim(false);
        body = new QWidget(q);
        auto* layout = new QVBoxLayout(body);
        layout->setContentsMargins(::Spacing::Medium, ::Spacing::Medium, ::Spacing::Medium, ::Spacing::Medium);
        layout->setSpacing(::Spacing::Small);
        auto* searchRow = new QHBoxLayout;
        searchRow->setContentsMargins(::Spacing::Small, 0, 0, 0);
        searchRow->setSpacing(::Spacing::XSmall);
        searchRow->addWidget(new FontIcon(Typography::Icons::Search, body));
        search = new textfields::LineEdit(body);
        search->setObjectName(QStringLiteral("FluentCommandPalette.Search"));
        search->setAccessibleName(CommandPalette::tr("Search commands"));
        search->setPlaceholderText(CommandPalette::tr("Type a command or search…"));
        search->setFrameVisible(false);
        search->setAccessibleDescription(
            CommandPalette::tr("Up and Down select a command. Enter runs it. Escape closes the palette."));
        search->setClearButtonEnabled(true);
        view = new collections::ListView(body);
        view->setObjectName(QStringLiteral("FluentCommandPalette.Results"));
        view->setAccessibleName(CommandPalette::tr("Commands"));
        view->setSpacing(0);
        view->setBorderVisible(false);
        view->setBackgroundVisible(false);
        // The popup already paints an opaque surface. Preserve it when the
        // containing window uses Mica/vibrancy instead of clearing through it.
        // zh_CN: 保留弹层已绘制的背景，避免在 Mica/毛玻璃窗口中被清成透明洞。
        view->setProperty("fluentPreserveParentSurface", true);
        view->viewport()->setProperty("fluentPreserveParentSurface", true);
        view->setPlaceholderText(CommandPalette::tr("No matching commands"));
        view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setItemDelegate(new PaletteDelegate(view));
        model = new PaletteModel(view);
        view->setModel(model);
        view->setSelectionIndicatorVisible(false);
        searchRow->addWidget(search, 1);
        layout->addLayout(searchRow);
        layout->addWidget(view, 1);
        q->setFocusProxy(search);
        QObject::connect(search, &QLineEdit::textChanged, q, [this](const QString& text) {
            QPointer<CommandPalette> guard(q);
            rebuild();
            if (guard)
                emit guard->queryChanged(text);
        });
        QObject::connect(&registry, &detail::CommandActionModel::structureChanged, q, [this]() { rebuild(); });
        QObject::connect(&registry, &detail::CommandActionModel::presentationChanged, q, [this]() { rebuild(); });
        QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, q, [this]() {
            notifyCurrent();
        });
        QObject::connect(view, &QAbstractItemView::clicked, q, [this](const QModelIndex& index) {
            if (model->flags(index) & Qt::ItemIsEnabled) {
                QPointer<CommandPalette> guard(q);
                view->setCurrentIndex(index);
                if (guard)
                    guard->triggerCurrent();
            }
        });
        QObject::connect(q, &dialogs_flyouts::Popup::aboutToShow, q, [this]() {
            QPointer<CommandPalette> guard(q);
            rebuild();
            if (!guard)
                return;
            q->resize(q->sizeHint());
            search->selectAll();
        });
    }

    void notifyCurrent()
    {
        QAction* action = model->rows.value(view->currentIndex().row()).data();
        if (current == action)
            return;
        current = action;
        emit q->currentActionChanged(action);
    }

    void rebuild()
    {
        const QList<QAction*> registered = registry.actions(detail::CommandActionModel::Section::Primary);
        for (auto it = metadata.begin(); it != metadata.end();) {
            if (!registered.contains(it.key()))
                it = metadata.erase(it);
            else
                ++it;
        }
        const QStringList words = search->text().simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QList<QPointer<QAction>> rows;
        QStringList groups;
        for (QAction* action : registered) {
            if (!isLeaf(action) || !registry.isPresentable(action) || !action->isVisible())
                continue;
            const SearchMetadata meta = metadata.value(action);
            const QString haystack = commandLabel(action) + QLatin1Char(' ') + meta.group + QLatin1Char(' ')
                + meta.keywords.join(QLatin1Char(' '));
            bool matches = true;
            for (const QString& word : words)
                matches = matches && haystack.contains(word, Qt::CaseInsensitive);
            if (!matches)
                continue;
            int at = groups.lastIndexOf(meta.group) + 1;
            if (at == 0)
                at = rows.size();
            rows.insert(at, action);
            groups.insert(at, meta.group);
        }
        const int oldCount = model->rowCount();
        {
            const QSignalBlocker blocker(view->selectionModel());
            model->replace(rows, groups);
            int selected = -1;
            for (int row = 0; row < rows.size(); ++row) {
                if (!rows.at(row) || !rows.at(row)->isEnabled())
                    continue;
                if (selected < 0 || rows.at(row).data() == current)
                    selected = row;
                if (rows.at(row).data() == current)
                    break;
            }
            view->setCurrentIndex(model->index(selected));
        }
        const int count = model->rowCount();
        if (q->isOpen()) {
            q->resize(q->sizeHint());
            if (q->parentWidget())
                q->move(overlay::clampCardTopLeft(q->pos(), q->size(), overlay::overlaySurfaceRect(q->parentWidget()),
                    ::Spacing::Small));
        }
        QPointer<CommandPalette> guard(q);
        notifyCurrent();
        if (guard && oldCount != count)
            emit guard->resultCountChanged(guard->resultCount());
    }

    void moveCurrent(int direction)
    {
        const int count = model->rowCount();
        int row = view->currentIndex().row();
        for (int i = 0; i < count; ++i) {
            row = (row + direction + count) % count;
            const QModelIndex index = model->index(row);
            if (model->flags(index) & Qt::ItemIsEnabled) {
                QPointer<CommandPalette> guard(q);
                view->setCurrentIndex(index);
                if (guard)
                    view->scrollTo(view->currentIndex());
                return;
            }
        }
    }

    int preferredHeight() const
    {
        int resultsHeight = 0;
        QStyleOptionViewItem option;
        option.initFrom(view);
        option.fontMetrics = view->fontMetrics();
        for (int row = 0; row < model->rowCount(); ++row)
            resultsHeight += view->itemDelegate()->sizeHint(option, model->index(row)).height();
        resultsHeight = qBound(64, resultsHeight, 360);
        return q->contentsMargins().top() + q->contentsMargins().bottom() + 2 * ::Spacing::Medium + ::Spacing::Small
            + search->sizeHint().height() + resultsHeight;
    }

    CommandPalette* q;
    detail::CommandActionModel registry;
    QHash<QAction*, SearchMetadata> metadata;
    QWidget* body = nullptr;
    textfields::LineEdit* search = nullptr;
    collections::ListView* view = nullptr;
    PaletteModel* model = nullptr;
    QAction* current = nullptr; // Identity only; model rows guard lifetime.
};

CommandPalette::CommandPalette(QWidget* parent) : Popup(parent), d(new CommandPalettePrivate(this)) {}
CommandPalette::~CommandPalette()
{
    QObject::disconnect(d->search, nullptr, this, nullptr);
    QObject::disconnect(d->view, nullptr, this, nullptr);
    QObject::disconnect(d->view->selectionModel(), nullptr, this, nullptr);
    d->view->setModel(nullptr);
    delete d;
    d = nullptr;
}

bool CommandPalette::addCommand(QAction* action, const QString& group, const QStringList& keywords)
{
    if (!isLeaf(action))
        return false;
    const SearchMetadata previous = d->metadata.value(action);
    d->metadata.insert(action, {group, keywords});
    QPointer<CommandPalette> guard(this);
    const bool registered = d->registry.add(detail::CommandActionModel::Section::Primary, action);
    if (!guard)
        return false;
    if (!registered) {
        d->metadata.insert(action, previous);
        return false;
    }
    d->rebuild();
    return true;
}

bool CommandPalette::removeCommand(QAction* action)
{
    return d->registry.remove(action);
}
void CommandPalette::clearCommands()
{
    d->registry.clear(detail::CommandActionModel::Section::Primary);
}
QList<QAction*> CommandPalette::commands() const
{
    return d->registry.actions(detail::CommandActionModel::Section::Primary);
}
QList<QAction*> CommandPalette::filteredCommands() const
{
    QList<QAction*> result;
    for (const auto& action : d->model->rows) {
        if (action)
            result.append(action.data());
    }
    return result;
}
QString CommandPalette::query() const
{
    return d->search->text();
}
void CommandPalette::setQuery(const QString& query)
{
    if (d->search->text() == query)
        return;
    {
        const QSignalBlocker blocker(d->search);
        d->search->setText(query);
    }
    QPointer<CommandPalette> guard(this);
    d->rebuild();
    if (guard)
        emit guard->queryChanged(guard->query());
}
QString CommandPalette::placeholderText() const
{
    return d->search->placeholderText();
}
void CommandPalette::setPlaceholderText(const QString& text)
{
    if (placeholderText() == text)
        return;
    d->search->setPlaceholderText(text);
    emit placeholderTextChanged(text);
}
QString CommandPalette::emptyText() const
{
    return d->view->placeholderText();
}
void CommandPalette::setEmptyText(const QString& text)
{
    if (emptyText() == text)
        return;
    d->view->setPlaceholderText(text);
    emit emptyTextChanged(text);
}
int CommandPalette::resultCount() const
{
    return d->model->rowCount();
}
QAction* CommandPalette::currentAction() const
{
    return d->model->rows.value(d->view->currentIndex().row()).data();
}
void CommandPalette::setCurrentAction(QAction* action)
{
    const int row = d->model->rows.indexOf(action);
    if (action && (row < 0 || !action->isEnabled()))
        return;
    d->view->setCurrentIndex(d->model->index(row));
}

bool CommandPalette::triggerCurrent()
{
    QPointer<QAction> action = currentAction();
    if (!isOpen() || !isLeaf(action) || !action->isEnabled() || !action->isVisible())
        return false;
    QPointer<CommandPalette> guard(this);
    const bool exitAnimation = isExitAnimationEnabled();
    setExitAnimationEnabled(false);
    closeWithReason(ActionButton);
    if (guard)
        setExitAnimationEnabled(exitAnimation);
    if (!guard || isOpen() || !action || !commands().contains(action) || !isLeaf(action) || !action->isEnabled()
        || !action->isVisible())
        return false;
    action->trigger();
    return true;
}

QSize CommandPalette::sizeHint() const
{
    QWidget* host = overlay::presentedTopLevel(parentWidget());
    const QSize available = host ? host->size() : QSize(640, 480);
    return QSize(qMax(1, qMin(464, available.width() - 16)),
        qMax(1, qMin(d->preferredHeight(), available.height() - 16)));
}

void CommandPalette::onThemeUpdated()
{
    Popup::onThemeUpdated();
    if (d)
        d->view->viewport()->update();
}

void CommandPalette::actionEvent(QActionEvent* event)
{
    QWidget::actionEvent(event);
    if (d)
        d->registry.handleActionEvent(event);
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    auto* widget = qobject_cast<QWidget*>(watched);
    if (d && isOpen() && widget && (widget == this || isAncestorOf(widget))) {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
            auto* key = static_cast<QKeyEvent*>(event);
            const bool handled = key->key() == Qt::Key_Down || key->key() == Qt::Key_Up || key->key() == Qt::Key_Return
                || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab;
            if (handled
                && (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::KeypadModifier
                    || (key->key() == Qt::Key_Backtab && key->modifiers() == Qt::ShiftModifier))) {
                key->accept();
                if (event->type() == QEvent::KeyPress) {
                    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                        triggerCurrent();
                    } else if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) {
                        const QList<QWidget*> order{d->search, d->view};
                        const int at = d->search->hasFocus() ? 0 : 1;
                        const int step = key->key() == Qt::Key_Backtab ? -1 : 1;
                        order.at((at + step + order.size()) % order.size())->setFocus(Qt::TabFocusReason);
                    } else
                        d->moveCurrent(key->key() == Qt::Key_Down ? 1 : -1);
                }
                return true;
            }
        }
    }
    if (d && isOpen() && widget == parentWidget() && event->type() == QEvent::Resize)
        resize(sizeHint());
    return Popup::eventFilter(watched, event);
}

void CommandPalette::resizeEvent(QResizeEvent* event)
{
    Popup::resizeEvent(event);
    if (d)
        d->body->setGeometry(contentsRect());
}

} // namespace fluent::menus_toolbars

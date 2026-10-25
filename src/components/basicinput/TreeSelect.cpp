#include "TreeSelect.h"
#include "components/collections/TreeView.h"
#include "components/collections/private/CollectionItemState_p.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/foundation/overlay/FieldPopupGeometry_p.h"
#include "components/foundation/private/ControlMetrics_p.h"
#include "compatibility/TextPaintCompat.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/TagInput.h"
#include "components/textfields/Label.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include <QAccessible>
#include <QAccessibleWidget>
#include <QAbstractButton>
#include <QApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegion>
#include <QStyledItemDelegate>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStackedLayout>
#include <QTimer>
#include <functional>
#include <utility>

namespace fluent::basicinput {
namespace {
constexpr int compactRowHeight = 32;
constexpr int shadow = ::Spacing::Standard;
constexpr int inset = ::Spacing::XSmall;
template <typename Base> class TreeSelectInput final : public Base {
public:
    explicit TreeSelectInput(QWidget* parent) : Base(parent) { this->setQueryIconDecorative(true); }
    bool composing() const { return this->isInputMethodComposing(); }
    void onThemeUpdated() override
    {
        Base::onThemeUpdated();
        if (this->parentWidget() && this->parentWidget()->testAttribute(Qt::WA_SetFont))
            this->setFont(this->parentWidget()->font());
    }
};

class TreeSelectTree final : public collections::TreeView {
public:
    using TreeView::TreeView;
    int heightForRows(int count) const
    {
        const auto margins = viewportMargins();
        return count * compactRowHeight + margins.top() + margins.bottom() + 2 * frameWidth();
    }
};

class TreeSelectAccessible final : public QAccessibleWidget {
public:
    explicit TreeSelectAccessible(TreeSelect* owner) : QAccessibleWidget(owner, QAccessible::ComboBox) {}
    TreeSelect* owner() const { return static_cast<TreeSelect*>(object()); }
    QString text(QAccessible::Text type) const override
    {
        if (type == QAccessible::Value) {
            QStringList paths;
            for (const auto& item : owner()->selectedItems())
                paths.append(item.toMap().value(QStringLiteral("path")).toString());
            return paths.join(QStringLiteral(", "));
        }
        return QAccessibleWidget::text(type);
    }
    QAccessible::State state() const override
    {
        auto result = QAccessibleWidget::state();
        result.expandable = true;
        result.expanded = owner()->isOpen();
        result.collapsed = !result.expanded;
        result.readOnly = owner()->isReadOnly();
        result.multiSelectable = owner()->selectionMode() == TreeSelect::SelectionMode::Multiple;
        return result;
    }
    QStringList actionNames() const override { return {QAccessibleActionInterface::showMenuAction()}; }
    void doAction(const QString& action) override
    {
        if (action == QAccessibleActionInterface::showMenuAction())
            owner()->setIsOpen(!owner()->isOpen());
    }
};
QAccessibleInterface* treeSelectFactory(const QString&, QObject* object)
{
    if (auto* selector = qobject_cast<TreeSelect*>(object))
        return new TreeSelectAccessible(selector);
    return nullptr;
}

class TeamRowDelegate final : public QStyledItemDelegate {
public:
    TeamRowDelegate(TreeSelect* owner, collections::TreeView* view, std::function<void()> handled)
        : QStyledItemDelegate(view), m_owner(owner), m_view(view), m_handled(std::move(handled))
    {}
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return QSize(0, compactRowHeight);
    }
    QRect slot(const QStyleOptionViewItem& option, int x, int width) const
    {
        return QStyle::visualRect(option.direction, option.rect,
            QRect(option.rect.left() + x, option.rect.top(), width, option.rect.height()));
    }
    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        auto state = option.state;
        state.setFlag(QStyle::State_Enabled, index.flags().testFlag(Qt::ItemIsEnabled));
        state.setFlag(QStyle::State_Selected, index == m_view->currentIndex());
        const auto& colors = m_owner->themeColorsRef();
        const auto visual = collections::detail::collectionItemVisualStyle(state, colors);
        const qreal radius = m_owner->themeRadius().control;
        p->setPen(Qt::NoPen);
        p->setBrush(visual.background);
        p->drawRoundedRect(QRectF(option.rect).adjusted(4, 1, -4, -1), radius, radius);
        p->setFont(Typography::Icons::font(Typography::IconSize::Compact));
        p->setPen(visual.foreground);
        const bool branch = index.model()->hasChildren(index);
        if (branch)
            p->drawText(slot(option, 8, 16), Qt::AlignCenter,
                m_view->isExpanded(index) ? Typography::Icons::ChevronDown
                    : option.direction == Qt::RightToLeft
                    ? Typography::Icons::ChevronLeftMed
                    : Typography::Icons::ChevronRightMed);
        const bool multiple = m_owner->selectionMode() == TreeSelect::SelectionMode::Multiple;
        const auto checked = index.data(Qt::CheckStateRole);
        int textOffset = branch ? 28 : 16;
        if (multiple && checked.isValid()) {
            const bool selected = checked.toInt() == Qt::Checked;
            const int checkOffset = branch ? 28 : 8;
            QRectF box = slot(option, checkOffset, 16);
            box.setTop(box.center().y() - 8);
            box.setHeight(16);
            p->setPen(selected ? Qt::NoPen : QPen(colors.strokeDefault, 1));
            p->setBrush(selected ? colors.accentDefault : colors.controlDefault);
            p->drawRoundedRect(box, radius, radius);
            if (selected) {
                p->setPen(colors.textOnAccent);
                p->drawText(box, Qt::AlignCenter, Typography::Icons::CheckMark);
            }
            textOffset = checkOffset + 24;
        }
        const bool selected = m_owner->selectedKeys().contains(index.data(m_owner->keyRole()).toString());
        if (!multiple && selected) {
            p->setPen(visual.foreground);
            p->drawText(slot(option, option.rect.width() - 24, 16), Qt::AlignCenter, Typography::Icons::CheckMark);
        }
        const QRect textSlot = slot(option, textOffset,
            qMax(0, option.rect.width() - textOffset - (!multiple && selected ? 32 : 8)));
        p->setFont(m_owner->themeFont(Typography::FontRole::Body).toQFont());
        p->setPen(visual.foreground);
        const QFontMetricsF metrics(p->font());
        const auto text = metrics.elidedText(index.data().toString(), Qt::ElideRight, textSlot.width());
        p->drawText(fluent::painting::verticallyCenteredTextInkRect(textSlot, metrics, text),
            QStyle::visualAlignment(option.direction, Qt::AlignLeft | Qt::AlignVCenter), text);
        p->restore();
    }
    bool editorEvent(QEvent* event, QAbstractItemModel*, const QStyleOptionViewItem& option, const QModelIndex& index)
        override
    {
        if (event->type() == QEvent::MouseButtonRelease && index.model()->hasChildren(index)) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton && slot(option, 8, 16).contains(fluentMousePos(mouse))) {
                m_handled();
                m_view->toggleExpanded(index);
                return true;
            }
        }
        // The owner toggles the whole row, including its check mark, exactly once.
        return false;
    }

private:
    TreeSelect* m_owner;
    collections::TreeView* m_view;
    std::function<void()> m_handled;
};
} // namespace

class TreeSelectProxy final : public QSortFilterProxyModel {
public:
    explicit TreeSelectProxy(TreeSelect* owner) : QSortFilterProxyModel(owner), m_owner(owner)
    {
        setRecursiveFilteringEnabled(true);
        // The owner batches presentation refreshes while keeping identity and
        // selection synchronous. Do not re-filter for each changed source row.
        // zh_CN: 身份与选择同步更新，展示刷新由宿主合并，避免逐行变化时重复筛选。
        setDynamicSortFilter(false);
    }
    void refreshFilter() { invalidateFilter(); }
    QVariant data(const QModelIndex& index, int role) const override
    {
        const auto source = mapToSource(index);
        if (role == Qt::CheckStateRole) {
            if (m_owner->m_mode != TreeSelect::SelectionMode::Multiple || !m_owner->canSelect(source, true))
                return {};
            return m_owner->m_selectedKeys.contains(source.data(m_owner->m_keyRole).toString())
                ? Qt::Checked
                : Qt::Unchecked;
        }
        if (role == Qt::ToolTipRole || role == Qt::AccessibleDescriptionRole)
            return m_owner->pathForIndex(source);
        if (role == Qt::SizeHintRole)
            return QSize(0, compactRowHeight);
        return QSortFilterProxyModel::data(index, role);
    }
    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        auto result = QSortFilterProxyModel::flags(index);
        result &= ~(Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsUserCheckable);
        const auto source = mapToSource(index);
        if (!m_owner->canSelect(source, true))
            result &= ~Qt::ItemIsSelectable;
        else if (m_owner->m_mode == TreeSelect::SelectionMode::Multiple)
            result |= Qt::ItemIsUserCheckable;
        return result;
    }
    bool setData(const QModelIndex& index, const QVariant& value, int role) override
    {
        if (m_owner->m_searchTimer->isActive()) {
            m_owner->m_checkHandled = true;
            m_owner->filter();
            return false;
        }
        if (role != Qt::CheckStateRole || !m_owner->canSelect(mapToSource(index)) || m_owner->isReadOnly())
            return false;
        const QString key = mapToSource(index).data(m_owner->m_keyRole).toString();
        auto keys = m_owner->selectedKeys();
        const bool checked = value.toInt() == Qt::Checked;
        if (checked == keys.contains(key))
            return false;
        if (checked)
            keys.append(key);
        else
            keys.removeAll(key);
        m_owner->m_checkHandled = true;
        m_owner->setSelectedKeys(keys);
        return true;
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        if (m_owner->m_searchTokens.isEmpty())
            return true;
        const auto index = sourceModel()->index(row, 0, parent);
        const QString path = m_owner->pathForIndex(index);
        for (const auto& token : m_owner->m_searchTokens)
            if (!path.contains(token, Qt::CaseInsensitive))
                return false;
        return true;
    }

private:
    TreeSelect* m_owner;
};

class TreeSelectPopup final : public dialogs_flyouts::Flyout {
public:
    explicit TreeSelectPopup(TreeSelect* owner) : Flyout(owner), m_owner(owner)
    {
        setObjectName(QStringLiteral("TreeSelectPopup"));
        setModal(false);
        setDim(false);
        // Match suggestion/selection fields: release the next input immediately.
        // zh_CN: 与建议/选择输入一致，关闭后立即让出命中区域，不残留渐隐列表。
        setAnimationEnabled(false);
        setPlacement(Auto);
        setAnchorOffset(::Spacing::Gap::Normal);
        setFocusOnOpenEnabled(false);
        setFocusPolicy(Qt::NoFocus);
        // The input owns Escape so an IME candidate cancellation never dismisses us.
        setClosePolicy(CloseOnPressOutside);
        m_empty = new textfields::Label(owner->emptyText(), this);
        m_empty->setObjectName(QStringLiteral("TreeSelectEmpty"));
        m_empty->setWordWrap(true);
        m_empty->setAlignment(Qt::AlignCenter);
        m_empty->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    }
    void present(bool open)
    {
        QPointer<TreeSelectPopup> guard(this);
        int rows = 0;
        auto* tree = m_owner->m_tree;
        for (QModelIndex i = tree->model()->index(0, 0); i.isValid() && rows < 8; i = tree->indexBelow(i))
            ++rows;
        const auto* top = overlay::presentedTopLevel(m_owner);
        const QRect surface = overlay::overlaySurfaceRect(top);
        const QRect anchor = overlay::presentedRectInTopLevel(m_owner);
        const auto space = overlay::fieldPopupSpace(anchor, surface, anchorOffset());
        const int width = space.boundedWidth(m_owner->width());
        m_empty->setText(m_owner->emptyText());
        // Hidden viewports can retain their old geometry until the first show.
        // Use the declared margins, not that stale viewport rectangle.
        // zh_CN: 隐藏列表的视口几何尚未更新，按已声明边距预留首帧空间。
        tree->ensurePolished();
        const int desired = rows
            ? static_cast<TreeSelectTree*>(tree)->heightForRows(rows)
            : qMax(64, m_empty->heightForWidth(qMax(1, width - 2 * inset)));
        const bool useTop = space.opensAbove(desired + 2 * inset);
        setPlacement(useTop ? Top : Bottom);
        const int height = qMax(1, qMin(desired, space.capacity(desired + 2 * inset) - 2 * inset));
        resize(width + 2 * shadow, height + 2 * (shadow + inset));
        tree->setGeometry(shadow, shadow + inset, width, height);
        m_empty->setGeometry(tree->geometry());
        tree->setVisible(rows != 0);
        m_empty->setVisible(rows == 0 && !m_owner->emptyText().isEmpty());
        setAnchor(m_owner);
        if (isOpen()) {
            move(computePosition());
            raise();
        } else if (open) {
            showAt(m_owner);
            // QTreeView installs its final viewport margin during showEvent.
            // Size again before the first paint, so that margin cannot create
            // a scrollbar for an otherwise complete set of visible rows.
            // zh_CN: 首次显示后、绘制前纳入树视口边距，避免凭空多出滚动范围。
            if (guard && m_owner && isOpen())
                present(false);
            return;
        }
        if (!guard || !m_owner)
            return;
        const QRect ownerRect(mapFromGlobal(top->mapToGlobal(anchor.topLeft())), anchor.size());
        setMask(QRegion(rect()) - QRegion(ownerRect.adjusted(-1, -1, 1, 1)));
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (isOpen() && !m_geometryUpdatePending
            && overlay::fieldPopupAnchorGeometryMayChange(watched, event, m_owner, this)) {
            m_geometryUpdatePending = true;
            QTimer::singleShot(0, this, [this] {
                m_geometryUpdatePending = false;
                if (m_owner && isOpen())
                    present(false);
            });
        }
        return Flyout::eventFilter(watched, event);
    }

    QPoint computePosition() const override
    {
        const auto* top = overlay::presentedTopLevel(m_owner);
        if (!top)
            return Flyout::computePosition();
        const QRect anchor = overlay::presentedRectInTopLevel(m_owner);
        const QSize card = overlay::visibleCardSize(size(), shadow);
        return overlay::outerTopLeftForVisibleCard(
            overlay::fieldPopupCardTopLeft(anchor, card, overlay::overlaySurfaceRect(top), m_owner->layoutDirection(),
                placement() == Top, anchorOffset()),
            shadow);
    }

    bool shouldDismissOnOutsidePress(const QPoint& point) const override
    {
        if (m_owner && m_owner->rect().contains(m_owner->mapFromGlobal(point)))
            return false;
        return Flyout::shouldDismissOnOutsidePress(point);
    }

private:
    QPointer<TreeSelect> m_owner;
    textfields::Label* m_empty;
    bool m_geometryUpdatePending = false;
};

TreeSelect::TreeSelect(QWidget* parent) : QWidget(parent)
{
    static const bool accessible = [] {
        QAccessible::installFactory(treeSelectFactory);
        return true;
    }();
    Q_UNUSED(accessible);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_placeholder = tr("Select an item");
    m_emptyText = tr("No matching items");
    m_layout = new QStackedLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(120);
    connect(m_searchTimer, &QTimer::timeout, this, [this] { filter(); });
    setFocusPolicy(Qt::StrongFocus);
    ensureEditor();
    syncInputs();
}

void TreeSelect::ensureEditor()
{
    if (editor())
        return;
    textfields::AutoSuggestBox* input;
    if (m_mode == SelectionMode::Single) {
        m_single = new TreeSelectInput<textfields::AutoSuggestBox>(this);
        m_single->setObjectName(QStringLiteral("TreeSelectSingleInput"));
        input = m_single;
    } else {
        m_multiple = new TreeSelectInput<textfields::TagInput>(this);
        m_multiple->setObjectName(QStringLiteral("TreeSelectMultipleInput"));
        input = m_multiple;
        connect(m_multiple, &textfields::TagInput::selectionChanged, this, [this] {
            if (!m_syncing)
                setSelectedKeys(m_multiple->selectedKeys());
        });
    }
    if (testAttribute(Qt::WA_SetFont))
        input->setFont(font());
    input->setQueryIconGlyph(Typography::Icons::ChevronDown);
    input->setQueryIconVisible(true);
    input->setClearButtonEnabled(false);
    input->installEventFilter(this);
    m_layout->addWidget(input);
    connect(input, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (m_syncing || m_composing)
            return;
        QPointer<TreeSelect> guard(this);
        updateSearchText(text, m_indexKeys.size() > 512);
        if (guard && !isOpen())
            open();
    });
    connect(input, &textfields::AutoSuggestBox::inputHeightChanged, this, [this] {
        setFixedHeight(editor()->height());
        resizePopup();
    });
    m_layout->setCurrentWidget(input);
    setFocusProxy(input);
    updateInputMetrics();
}

void TreeSelect::updateInputMetrics()
{
    auto* input = editor();
    if (!input)
        return;
    const QPointer<TreeSelect> guard(this);
    if (m_single)
        input->setInputHeight(::fluent::detail::standardControlHeight(input->fontMetrics()));
    if (!guard)
        return;
    setFixedHeight(input->height());
    if (!guard)
        return;
    updateGeometry();
    resizePopup();
}

void TreeSelect::ensurePopup()
{
    if (m_popup)
        return;
    m_proxy = new TreeSelectProxy(this);
    m_proxy->setSourceModel(m_model);
    m_popup = new TreeSelectPopup(this);
    m_tree = new TreeSelectTree(m_popup);
    m_tree->setObjectName(QStringLiteral("TreeSelectTree"));
    m_tree->setAccessibleName(tr("Options"));
    m_tree->setModel(m_proxy);
    m_tree->setItemDelegate(new TeamRowDelegate(this, m_tree, [this] { m_checkHandled = true; }));
    m_tree->setHeaderHidden(true);
    m_tree->setBorderVisible(false);
    m_tree->setBackgroundVisible(false);
    m_tree->setProperty("fluentPreserveParentSurface", true);
    m_tree->viewport()->setProperty("fluentPreserveParentSurface", true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->setUniformRowHeights(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setIndentation(::Spacing::Standard);
    m_tree->setOverscrollEnabled(false);
    m_tree->setScrollChainingEnabled(false);
    m_tree->setSelectionMode(collections::SelectionMode::Single);
    m_tree->installEventFilter(this);
    connect(m_tree, &QTreeView::pressed, this, [this] { m_checkHandled = false; });
    connect(m_tree, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        if (m_checkHandled) {
            m_checkHandled = false;
            return;
        }
        activateIndex(index);
    });
    connect(m_tree, &QTreeView::expanded, this, [this](const QModelIndex& index) {
        if (!m_filtering && m_searchTokens.isEmpty())
            m_expandedIndexes.insert(m_proxy->mapToSource(index));
        if (!m_filtering)
            resizePopup();
    });
    connect(m_tree, &QTreeView::collapsed, this, [this](const QModelIndex& index) {
        if (!m_filtering && m_searchTokens.isEmpty())
            m_expandedIndexes.remove(m_proxy->mapToSource(index));
        if (!m_filtering)
            resizePopup();
    });
    connect(m_popup, &dialogs_flyouts::Popup::isOpenChanged, this, [this](bool open) {
        if (m_destroying)
            return;
        QPointer<TreeSelect> guard(this);
        if (!open) {
            m_composing = false;
            setSearchText(QString());
            if (!guard)
                return;
            syncInputs();
        }
        QAccessible::State state;
        state.expanded = true;
        state.collapsed = true;
        QAccessibleStateChangeEvent event(this, state);
        QAccessible::updateAccessibility(&event);
        emit isOpenChanged(open);
    });
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget*) {
        if (!isOpen())
            return;
        QTimer::singleShot(0, this, [this] {
            auto* focus = QApplication::focusWidget();
            if (isOpen() && focus && focus != this && !isAncestorOf(focus) && focus != m_popup
                && !m_popup->isAncestorOf(focus))
                close();
        });
    });
    syncInputs();
}

TreeSelect::~TreeSelect()
{
    m_destroying = true;
    disconnect(qApp, nullptr, this, nullptr);
    m_searchTimer->stop();
    for (const auto& c : std::as_const(m_modelConnections))
        disconnect(c);
    // Popup may be reparented to the window while open; destroy it before its owner state.
    auto* popup = m_popup;
    m_popup = nullptr;
    delete popup;
}
QAbstractItemModel* TreeSelect::model() const
{
    return m_model;
}
textfields::AutoSuggestBox* TreeSelect::editor() const
{
    return m_mode == SelectionMode::Single ? m_single : m_multiple;
}
bool TreeSelect::isComposing() const
{
    return m_composing
        || (m_mode == SelectionMode::Single
                ? static_cast<TreeSelectInput<textfields::AutoSuggestBox>*>(m_single)->composing()
                : static_cast<TreeSelectInput<textfields::TagInput>*>(m_multiple)->composing());
}
QString TreeSelect::pathForIndex(QModelIndex source) const
{
    QStringList path;
    while (source.isValid()) {
        path.prepend(source.data().toString());
        source = source.parent();
    }
    return path.join(QStringLiteral(" / "));
}
bool TreeSelect::canSelect(const QModelIndex& index, bool allowDisabled) const
{
    const auto key = index.data(m_keyRole).toString();
    return index.isValid() && !key.isEmpty() && m_indexes.count(key) == 1
        && index.flags().testFlag(Qt::ItemIsSelectable) && (allowDisabled || index.flags().testFlag(Qt::ItemIsEnabled));
}
void TreeSelect::setModel(QAbstractItemModel* model)
{
    if (m_model == model || (m_proxy && model == m_proxy))
        return;
    for (const auto& c : std::as_const(m_modelConnections))
        disconnect(c);
    m_modelConnections.clear();
    m_model = model;
    if (m_proxy)
        m_proxy->setSourceModel(model);
    observeModel();
    QPointer<TreeSelect> guard(this);
    rebuildIndex();
    if (guard)
        emit modelChanged(model);
}
void TreeSelect::observeModel()
{
    if (!m_model)
        return;
    const auto refresh = [this] { refreshSelection(); };
    m_modelConnections
        << connect(m_model, &QAbstractItemModel::modelReset, this, [this] { rebuildIndex(); })
        << connect(m_model, &QAbstractItemModel::rowsInserted, this,
               [this](const QModelIndex& parent, int first, int last) {
                   for (int row = first; row <= last; ++row)
                       indexBranch(m_model->index(row, 0, parent));
                   refreshSelection();
               })
        << connect(m_model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
               [this](const QModelIndex& parent, int first, int last) {
                   for (int row = first; row <= last; ++row)
                       unindexBranch(m_model->index(row, 0, parent));
               })
        << connect(m_model, &QAbstractItemModel::rowsRemoved, this, refresh)
        << connect(m_model, &QAbstractItemModel::rowsMoved, this, refresh)
        << connect(m_model, &QAbstractItemModel::layoutChanged, this, refresh)
        << connect(m_model, &QAbstractItemModel::dataChanged, this,
               [this](const QModelIndex& first, const QModelIndex& last, const QVector<int>& roles) {
                   if (first.column() != 0)
                       return;
                   const bool keysChanged = roles.isEmpty() || roles.contains(m_keyRole);
                   if (keysChanged) {
                       for (int row = first.row(); row <= last.row(); ++row)
                           updateIndexKey(m_model->index(row, 0, first.parent()));
                   }
                   if (keysChanged || roles.contains(Qt::DisplayRole) || roles.contains(Qt::EditRole))
                       refreshSelection();
               })
        << connect(m_model, &QAbstractItemModel::columnsInserted, this,
               [this](const QModelIndex&, int first, int) {
                   if (first == 0)
                       rebuildIndex();
               })
        << connect(m_model, &QAbstractItemModel::columnsRemoved, this,
               [this](const QModelIndex&, int first, int) {
                   if (first == 0)
                       rebuildIndex();
               })
        << connect(m_model, &QAbstractItemModel::columnsMoved, this, [this] { rebuildIndex(); })
        << connect(m_model, &QObject::destroyed, this, [this] {
               m_model = nullptr;
               // The proxy's source-destruction handler clears its own source.
               // zh_CN: 由 proxy 自己处理源模型销毁，不在 QObject 析构中重新解绑其信号。
               QPointer<TreeSelect> guard(this);
               rebuildIndex();
               if (guard)
                   emit modelChanged(nullptr);
           });
}
void TreeSelect::updateIndexKey(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    const QPersistentModelIndex persistent(index);
    const QString key = index.data(m_keyRole).toString();
    const QString previous = m_indexKeys.value(persistent);
    if (previous == key)
        return;
    if (!previous.isEmpty()) {
        m_indexes.remove(previous, persistent);
        m_indexKeys.remove(persistent);
    }
    if (!key.isEmpty()) {
        m_indexes.insert(key, persistent);
        m_indexKeys.insert(persistent, key);
    }
}
void TreeSelect::indexBranch(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    updateIndexKey(index);
    for (int row = 0; row < m_model->rowCount(index); ++row)
        indexBranch(m_model->index(row, 0, index));
}
void TreeSelect::unindexBranch(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    for (int row = 0; row < m_model->rowCount(index); ++row)
        unindexBranch(m_model->index(row, 0, index));
    const QPersistentModelIndex persistent(index);
    const QString key = m_indexKeys.take(persistent);
    if (!key.isEmpty())
        m_indexes.remove(key, persistent);
    m_expandedIndexes.remove(persistent);
}
void TreeSelect::rebuildIndex()
{
    m_indexes.clear();
    m_indexKeys.clear();
    m_expandedIndexes.clear();
    m_browseInitialized = false;
    for (int row = 0; m_model && row < m_model->rowCount(); ++row)
        indexBranch(m_model->index(row, 0));
    refreshSelection();
}
void TreeSelect::refreshSelection()
{
    QPointer<TreeSelect> guard(this);
    applySelection(m_selectedKeys, true);
    if (guard)
        schedulePopupRefresh();
}
void TreeSelect::schedulePopupRefresh()
{
    if (!isOpen() || m_popupRefreshPending)
        return;
    m_popupRefreshPending = true;
    QTimer::singleShot(0, this, [this] {
        m_popupRefreshPending = false;
        if (isOpen())
            filter(false);
    });
}
void TreeSelect::setKeyRole(int role)
{
    if (m_keyRole == role)
        return;
    m_keyRole = role;
    QPointer<TreeSelect> guard(this);
    rebuildIndex();
    if (guard)
        emit keyRoleChanged(role);
}
void TreeSelect::setSelectionMode(SelectionMode mode)
{
    if ((mode != SelectionMode::Single && mode != SelectionMode::Multiple) || m_mode == mode)
        return;
    QPointer<TreeSelect> guard(this);
    close();
    if (!guard)
        return;
    auto* previous = editor();
    const bool hadFocus = previous->hasFocus() || previous->isAncestorOf(QApplication::focusWidget());
    disconnect(previous, nullptr, this, nullptr);
    previous->removeEventFilter(this);
    m_layout->removeWidget(previous);
    m_single = nullptr;
    m_multiple = nullptr;
    m_mode = mode;
    ensureEditor();
    delete previous;
    if (!guard)
        return;
    if (hadFocus)
        editor()->setFocus(Qt::OtherFocusReason);
    applySelection(m_selectedKeys, true);
    if (!guard)
        return;
    syncInputs();
    filter();
    emit selectionModeChanged(mode);
}
void TreeSelect::setSelectedKeys(const QStringList& keys)
{
    applySelection(keys, false);
}
void TreeSelect::clearSelection()
{
    setSelectedKeys({});
}
void TreeSelect::applySelection(const QStringList& requested, bool allowDisabled)
{
    QStringList keys;
    QVariantList items;
    for (const auto& key : requested) {
        const auto index = m_indexes.value(key);
        if (keys.contains(key) || !canSelect(index, allowDisabled))
            continue;
        keys.append(key);
        items.append(QVariantMap{{QStringLiteral("key"), key}, {QStringLiteral("text"), index.data().toString()},
            {QStringLiteral("path"), pathForIndex(index)}});
        if (m_mode == SelectionMode::Single)
            break;
    }
    if (m_selectedKeys == keys && m_selectedItems == items)
        return;
    const auto previousKeys = m_selectedKeys;
    m_selectedKeys = keys;
    m_selectedItems = items;
    syncInputs();
    if (m_tree)
        m_tree->viewport()->update();
    QPointer<TreeSelect> guard(this);
    auto changedKeys = previousKeys + keys;
    changedKeys.removeDuplicates();
    for (const auto& key : changedKeys) {
        if (!m_proxy)
            break;
        if (previousKeys.contains(key) == keys.contains(key))
            continue;
        const auto index = m_proxy->mapFromSource(m_indexes.value(key));
        if (index.isValid())
            emit m_proxy->dataChanged(index, index, {Qt::CheckStateRole});
        if (!guard)
            return;
    }
    QAccessibleValueChangeEvent event(this, QVariant(keys));
    QAccessible::updateAccessibility(&event);
    emit selectionChanged();
}
void TreeSelect::syncInputs()
{
    m_syncing = true;
    auto* input = editor();
    const QSignalBlocker blocker(input);
    input->setPlaceholderText(m_placeholder);
    input->setReadOnly(m_readOnly);
    if (m_single) {
        m_single->setText(isOpen() ? m_search
                : m_selectedItems.isEmpty()
                ? QString()
                : m_selectedItems.first().toMap().value(QStringLiteral("path")).toString());
    } else {
        m_multiple->setSelectedItems(m_selectedItems);
        if (m_multiple->text() != m_search)
            m_multiple->setText(m_search);
        for (auto* tag : m_multiple->findChildren<QWidget*>(QStringLiteral("TagInputTag"))) {
            const auto key = tag->property("tagKey").toString();
            tag->setToolTip(pathForIndex(m_indexes.value(key)));
            tag->setAccessibleDescription(tag->toolTip());
            if (auto* close = tag->focusProxy())
                close->setAccessibleDescription(tag->toolTip());
        }
    }
    input->setAccessibleName(accessibleName().isEmpty() ? tr("Select an item") : accessibleName());
    input->setAccessibleDescription(tr("Type to search. Up and Down browse; Enter selects; Escape closes."));
    setFixedHeight(input->height());
    m_syncing = false;
}
void TreeSelect::setSearchText(const QString& text)
{
    updateSearchText(text, false);
}
void TreeSelect::updateSearchText(const QString& text, bool defer)
{
    if (m_search == text) {
        if (!defer && m_searchTimer->isActive())
            filter();
        return;
    }
    const QString query = text;
    m_search = text;
    if (!m_composing && editor()->text() != text) {
        const QSignalBlocker blocker(editor());
        editor()->setText(text);
    }
    const QPointer<TreeSelect> guard(this);
    if (defer)
        m_searchTimer->start();
    else
        filter();
    if (guard && m_search == query)
        emit searchTextChanged(query);
}
void TreeSelect::filter(bool resetCurrent)
{
    m_searchTimer->stop();
    static const QRegularExpression separator(QStringLiteral("\\s+"));
    m_searchTokens = m_search.split(separator, Qt::SkipEmptyParts);
    if (!m_tree)
        return;
    const QScopedValueRollback<bool> filtering(m_filtering, true);
    const QPersistentModelIndex previous = m_proxy->mapToSource(m_tree->currentIndex());
    const int scroll = m_tree->verticalScrollBar()->value();
    if (resetCurrent || !m_searchTokens.isEmpty())
        m_proxy->refreshFilter();
    const bool restoreBranches = resetCurrent || !m_browseInitialized;
    if (!m_browseInitialized) {
        for (int row = 0; m_model && row < m_model->rowCount(); ++row) {
            const auto source = m_model->index(row, 0);
            if (m_model->hasChildren(source))
                m_expandedIndexes.insert(source);
        }
        m_browseInitialized = true;
    }
    if (!m_searchTokens.isEmpty()) {
        m_tree->expandAll();
    } else if (restoreBranches) {
        m_tree->collapseAll();
        for (const auto& source : std::as_const(m_expandedIndexes)) {
            const auto index = m_proxy->mapFromSource(source);
            if (index.isValid())
                m_tree->expand(index);
        }
    }
    auto current = resetCurrent ? QModelIndex() : m_proxy->mapFromSource(previous);
    if (!current.isValid() && !m_selectedKeys.isEmpty())
        current = m_proxy->mapFromSource(m_indexes.value(m_selectedKeys.first()));
    if (resetCurrent && current.isValid()) {
        for (auto parent = current.parent(); parent.isValid(); parent = parent.parent())
            m_tree->expand(parent);
    }
    m_tree->setCurrentIndex(current);
    if (!current.isValid())
        navigate(1);
    if (!resetCurrent) {
        m_tree->doItemsLayout();
        m_tree->verticalScrollBar()->setValue(scroll);
    }
    resizePopup();
}
void TreeSelect::setPlaceholderText(const QString& text)
{
    if (m_placeholder == text)
        return;
    m_placeholder = text;
    syncInputs();
    emit placeholderTextChanged();
}
void TreeSelect::setEmptyText(const QString& text)
{
    if (m_emptyText == text)
        return;
    m_emptyText = text;
    QPointer<TreeSelect> guard(this);
    resizePopup();
    if (guard)
        emit emptyTextChanged();
}
void TreeSelect::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;
    m_readOnly = readOnly;
    QPointer<TreeSelect> guard(this);
    if (readOnly)
        close();
    if (!guard)
        return;
    syncInputs();
    emit readOnlyChanged(readOnly);
}
bool TreeSelect::isOpen() const
{
    return m_popup && m_popup->isOpen();
}
void TreeSelect::setIsOpen(bool open)
{
    if (open)
        this->open();
    else
        close();
}
void TreeSelect::open()
{
    if (isOpen() || !isEnabled() || m_readOnly || !isVisible())
        return;
    ensurePopup();
    m_tree->setAccessibleName(accessibleName().isEmpty() ? tr("Options") : tr("%1 options").arg(accessibleName()));
    editor()->setFocus(Qt::OtherFocusReason);
    if (!isComposing()) {
        const QSignalBlocker blocker(editor());
        editor()->setText(m_search);
    }
    filter();
    m_popup->present(true);
}
void TreeSelect::close()
{
    if (m_popup)
        m_popup->close();
}
void TreeSelect::resizePopup()
{
    if (isOpen())
        m_popup->present(false);
}
void TreeSelect::activateIndex(const QModelIndex& index)
{
    if (m_readOnly || !isEnabled() || isComposing())
        return;
    if (m_searchTimer->isActive()) {
        filter();
        return;
    }
    const auto source = m_proxy->mapToSource(index);
    if (!canSelect(source)) {
        if (index.isValid())
            m_tree->setExpanded(index, !m_tree->isExpanded(index));
        return;
    }
    const QString key = source.data(m_keyRole).toString();
    QStringList keys = m_mode == SelectionMode::Single ? QStringList{key} : m_selectedKeys;
    if (m_mode == SelectionMode::Multiple) {
        if (keys.contains(key))
            keys.removeAll(key);
        else
            keys.append(key);
    }
    QPointer<TreeSelect> guard(this);
    setSelectedKeys(keys);
    if (!guard)
        return;
    if (m_mode == SelectionMode::Single)
        close();
    else
        editor()->setFocus(Qt::OtherFocusReason);
}
void TreeSelect::navigate(int direction)
{
    auto index = m_tree->currentIndex();
    if (!index.isValid())
        index = m_proxy->index(0, 0);
    else
        index = direction > 0 ? m_tree->indexBelow(index) : m_tree->indexAbove(index);
    while (index.isValid() && !index.flags().testFlag(Qt::ItemIsEnabled))
        index = direction > 0 ? m_tree->indexBelow(index) : m_tree->indexAbove(index);
    if (index.isValid()) {
        m_tree->setCurrentIndex(index);
        m_tree->scrollTo(index);
    }
}
bool TreeSelect::eventFilter(QObject* watched, QEvent* event)
{
    QPointer<TreeSelect> guard(this);
    if (watched != editor() && watched != m_tree)
        return QWidget::eventFilter(watched, event);
    if (watched == editor() && event->type() == QEvent::FontChange)
        updateInputMetrics();
    if (!guard)
        return false;
    if (event->type() == QEvent::Resize)
        resizePopup();
    if (event->type() == QEvent::InputMethod) {
        const auto* input = static_cast<QInputMethodEvent*>(event);
        const bool wasComposing = m_composing;
        QPointer<TreeSelect> guard(this);
        if (!isOpen() && (!input->preeditString().isEmpty() || !input->commitString().isEmpty()))
            open();
        if (!guard)
            return true;
        m_composing = !input->preeditString().isEmpty();
        if (wasComposing && !m_composing) {
            QTimer::singleShot(0, this, [this] {
                if (!m_composing && !m_readOnly)
                    setSearchText(editor()->text());
            });
        }
    }
    if (event->type() == QEvent::MouseButtonPress && watched == editor() && !m_readOnly)
        open();
    if (event->type() == QEvent::KeyPress && !m_readOnly && isEnabled()) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (isComposing())
            return false;
        const int k = key->key();
        if (m_searchTimer->isActive()
            && (k == Qt::Key_Down || k == Qt::Key_Up || k == Qt::Key_Return || k == Qt::Key_Enter)) {
            filter();
            if (!guard)
                return true;
        }
        if (k == Qt::Key_Escape && isOpen()) {
            close();
            return true;
        }
        if (k == Qt::Key_Tab || k == Qt::Key_Backtab) {
            close();
            return !guard;
        }
        if (k == Qt::Key_Down || k == Qt::Key_Up) {
            const bool wasOpen = isOpen();
            open();
            if (!guard)
                return true;
            if (wasOpen)
                navigate(k == Qt::Key_Down ? 1 : -1);
            return true;
        }
        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            if (isOpen())
                activateIndex(m_tree->currentIndex());
            else
                open();
            return true;
        }
        if (isOpen() && m_search.isEmpty() && (k == Qt::Key_Right || k == Qt::Key_Left)) {
            const auto current = m_tree->currentIndex();
            if (k == Qt::Key_Right) {
                if (m_tree->isExpanded(current))
                    navigate(1);
                else
                    m_tree->expand(current);
            } else if (m_tree->isExpanded(current))
                m_tree->collapse(current);
            else if (current.parent().isValid())
                m_tree->setCurrentIndex(current.parent());
            return true;
        }
        const bool edit = k == Qt::Key_Backspace || k == Qt::Key_Delete
            || (!key->text().isEmpty() && key->text().at(0).isPrint()
                && !(key->modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier)));
        if (edit && !isOpen())
            open();
    }
    return guard ? QWidget::eventFilter(watched, event) : true;
}
void TreeSelect::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange && !isEnabled())
        close();
    else if (event->type() == QEvent::FontChange && editor()) {
        editor()->setFont(font());
    }
}
void TreeSelect::onThemeUpdated()
{
    update();
}
QSize TreeSelect::sizeHint() const
{
    return QSize(320, editor()->height());
}
QSize TreeSelect::minimumSizeHint() const
{
    return QSize(120, editor()->height());
}
} // namespace fluent::basicinput

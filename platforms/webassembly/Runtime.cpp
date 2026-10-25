#include <FluentQt/WebAssembly.h>

#include <QAccessible>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QListView>
#include <QTreeView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollBar>
#include <QAccessibleObject>
#include <QAccessibleWidget>
#include <QPointer>
#include <QPersistentModelIndex>
#include <QItemSelectionModel>
#include <QRect>
#include <QResizeEvent>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <utility>

#include <emscripten/emscripten.h>

#include "compatibility/private/RuntimePlatformCapabilities_p.h"

namespace fluent::webassembly {
namespace {

class BrowserDesktopSurface;

QPointer<BrowserDesktopSurface>& desktopSurface()
{
    static QPointer<BrowserDesktopSurface> surface;
    return surface;
}

QPointer<QWidget>& primaryHostedWindow()
{
    static QPointer<QWidget> window;
    return window;
}

QRect constrainedGeometry(const QRect& requested, const QWidget* desktop)
{
    if (!desktop)
        return requested;

    const QRect bounds = desktop->rect();
    QSize size = requested.size().boundedTo(bounds.size());
    size.setWidth(qMax(1, size.width()));
    size.setHeight(qMax(1, size.height()));
    const int x = qBound(bounds.left(), requested.x(), qMax(bounds.left(), bounds.right() - size.width() + 1));
    const int y = qBound(bounds.top(), requested.y(), qMax(bounds.top(), bounds.bottom() - size.height() + 1));
    return QRect(QPoint(x, y), size);
}

void configureAccessibleProxyPresentation()
{
    // Qt 6.9.3 may create accessibility proxies on focus/name events before
    // ObjectShow supplies their geometry. Its negative-z a11y layer is visible
    // through our transparent desktop canvas, exposing native HTML form chrome.
    // Keep proxy semantics, geometry and events intact; only Qt paints the UI.
    // Offscreen proxy rectangles must not enlarge the scrollable Qt window:
    // browser focus otherwise scrolls its canvas away with the hidden proxy.
    // Exclude the screen-reader enable button, and never target the independent
    // document-body input used by Qt's keyboard/IME context.
    // zh_CN: Qt 6.9.3 的焦点/名称事件可能先于 ObjectShow 创建无几何的语义代理。
    // 负 z 层仍会透过透明桌面画布，露出原生 HTML 表单；这里只关闭代理绘制，
    // 保留语义、几何和事件，并排除读屏启用按钮及独立的键盘/IME 输入节点。
    // 裁剪代理层溢出，避免聚焦屏外代理时浏览器连同画布一起滚走。
    EM_ASM({
        const containers = Module['qtContainerElements'];
        if (!Array.isArray(containers))
            return;
        for (const container of containers) {
            if (!container)
                continue;
            const shadow = container.querySelector('#qt-shadow-container') ?.shadowRoot;
            if (!shadow || shadow.getElementById('fluentqt-accessible-proxy-presentation'))
                continue;
            const style = container.ownerDocument.createElement('style');
            style.id = 'fluentqt-accessible-proxy-presentation';
            style.textContent = '.qt-window-a11y-container { overflow: clip; }'
                + '.qt-window-a11y-container > [id]:not(' + '.hidden-visually-read-by-screen-reader) { opacity: 0; }';
            shadow.appendChild(style);
            // Qt 6.9.3's enable button populates the DOM without notifying
            // QAccessible::ActivationObserver. Refresh after that handler runs.
            shadow.addEventListener('click', event => {
                if (event.target?.matches('button.hidden-visually-read-by-screen-reader'))
                    queueMicrotask(() => Module['_fluentqt_refresh_accessible_views']());
            }, true);
        }
    });
}

// QMenu recreates its platform window between openings. Qt 6.9.3's WASM
// accessibility bridge caches action proxies, but ObjectShow only reattaches
// the menu root to the new window. Publish the direct children after layout so
// Qt reuses their proxies, names and current geometry in the current container.
// Do not walk unopened submenus or replace Qt's accessibility/event bridge.
// zh_CN: QMenu 重开时会重建平台窗口；Qt 6.9.3 WASM 缓存的动作代理仍留在旧
// 容器中，ObjectShow 只迁移菜单根。布局完成后补发直接子项的显示事件，由 Qt
// 复用代理并更新名称和几何；不遍历未打开的子菜单，也不另建无障碍事件桥。
// Qt 6.9.3 ignores list model updates and retains DOM proxies even after their
// cell interfaces disappear. Reuse a bounded set of presentation slots instead
// of publishing model-cell identities. The Qt bridge still owns DOM and input.
// zh_CN: Qt 6.9.3 忽略列表模型更新，且在行接口失效后仍缓存 DOM 代理；这里复用
// 有界的可见行展示槽，不发布模型行的身份；DOM 与输入仍由 Qt 事件桥负责。
#if QT_CONFIG(accessibility)
// Match Qt's bridge identifier without changing application object names.
QString accessibleDomId(QAccessibleInterface* accessible)
{
    const QString identifier = accessible->text(QAccessible::Identifier);
    if (!identifier.isEmpty())
        return identifier;
    QStringList names;
    for (; accessible; accessible = accessible->parent()) {
        auto* object = accessible->object();
        names.prepend(!object ? QString()
                : !object->objectName().isEmpty()
                ? object->objectName()
                : QString::fromUtf8(object->metaObject()->className()));
    }
    return names.join(QLatin1Char('.'));
}

class ComboBoxAccessibilitySynchronizer final : public QObject {
public:
    explicit ComboBoxAccessibilitySynchronizer(QWidget* widget) : QObject(widget), m_widget(widget)
    {
        widget->installEventFilter(this);
        schedule();
    }
    void refreshAfterActivation()
    {
        m_lastPresentation.clear();
        schedule();
    }

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::Show || event->type() == QEvent::Paint || event->type() == QEvent::FocusIn)
            schedule();
        return false;
    }

private:
    void schedule()
    {
        if (m_pending)
            return;
        m_pending = true;
        QTimer::singleShot(0, this, [this] {
            m_pending = false;
            auto* accessible = QAccessible::queryAccessibleInterface(m_widget);
            if (!accessible || !m_widget->isVisible())
                return;
            const auto state = accessible->state();
            const QByteArray id = accessibleDomId(accessible).toUtf8();
            const QByteArray description = accessible->text(QAccessible::Value).toUtf8();
            const QByteArray name = accessible->text(QAccessible::Name).toUtf8();
            const QByteArray presentation = QJsonDocument(
                QJsonObject{{"id", QString::fromUtf8(id)}, {"value", QString::fromUtf8(description)},
                    {"name", QString::fromUtf8(name)}, {"expanded", bool(state.expanded)},
                    {"readOnly", bool(state.readOnly)},
                    {"disabled",
                        bool(state.disabled)}}).toJson(QJsonDocument::Compact);
            if (presentation == m_lastPresentation)
                return;
            m_lastPresentation = presentation;
            // clang-format off
            EM_ASM({
                for (const container of Module['qtContainerElements'] || []) {
                    const shadow = container.querySelector('#qt-shadow-container')?.shadowRoot;
                    const node = shadow?.getElementById(UTF8ToString($0));
                    if (!node) continue;
                    node.setAttribute('role', 'combobox');
                    node.setAttribute('aria-label', UTF8ToString($5));
                    node.setAttribute('aria-expanded', $1 ? 'true' : 'false');
                    node.setAttribute('aria-readonly', $2 ? 'true' : 'false');
                    node.setAttribute('aria-disabled', $3 ? 'true' : 'false');
                    node.setAttribute('aria-description', UTF8ToString($4));
                    if (!$1) node.removeAttribute('aria-activedescendant');
                }
            }, id.constData(), int(state.expanded), int(state.readOnly), int(state.disabled), description.constData(), name.constData());
            // clang-format on
        });
    }
    QWidget* m_widget;
    bool m_pending = false;
    QByteArray m_lastPresentation;
};

class VisibleListCell final : public QAccessibleObject {
public:
    VisibleListCell(QObject* owner, QAbstractItemView* view) : QAccessibleObject(owner), m_view(view) {}
    QAccessibleInterface* parent() const override { return QAccessible::queryAccessibleInterface(m_view); }
    QAccessibleInterface* child(int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface*) const override { return -1; }
    QAccessible::Role role() const override
    {
        return qobject_cast<QTreeView*>(m_view) ? QAccessible::TreeItem : QAccessible::ListItem;
    }
    QWindow* window() const override { return m_window; }
    QRect rect() const override { return m_rect; }
    QString text(QAccessible::Text type) const override
    {
        // This backend writes Name using innerHTML; preserve literal model text.
        if (type == QAccessible::Identifier && qobject_cast<QTreeView*>(m_view))
            return object()->objectName();
        if (type == QAccessible::Description)
            return m_description;
        return type == QAccessible::Name ? m_text.toHtmlEscaped() : QString();
    }
    QAccessible::State state() const override
    {
        QAccessible::State result;
        result.invisible = !m_visible || !m_index.isValid();
        if (!m_view || !m_index.isValid())
            return result;
        const auto flags = m_index.flags();
        result.disabled = !m_view->isEnabled() || !flags.testFlag(Qt::ItemIsEnabled);
        result.selectable = flags.testFlag(Qt::ItemIsSelectable)
            && m_view->selectionMode() != QAbstractItemView::NoSelection;
        result.focusable = result.selectable && !result.disabled;
        result.selected = m_view->selectionModel() && m_view->selectionModel()->isSelected(m_index);
        result.focused = m_view->hasFocus() && m_view->currentIndex() == m_index;
        result.checkable = flags.testFlag(Qt::ItemIsUserCheckable);
        const auto checked = m_index.data(Qt::CheckStateRole).toInt();
        result.checked = result.checkable && checked == Qt::Checked;
        result.checkStateMixed = result.checkable && checked == Qt::PartiallyChecked;
        return result;
    }
    void hide()
    {
        if (!m_visible)
            return;
        m_visible = false;
        m_index = QPersistentModelIndex();
        QAccessibleEvent event(this, QAccessible::ObjectHide);
        QAccessible::updateAccessibility(&event);
    }
    void present(const QModelIndex& index, const QRect& rect, QWindow* window)
    {
        m_index = index;
        m_text = index.data(Qt::AccessibleTextRole).toString();
        if (m_text.isEmpty())
            m_text = index.data(Qt::DisplayRole).toString();
        m_description = index.data(Qt::AccessibleDescriptionRole).toString();
        m_rect = rect;
        m_window = window;
        m_visible = true;
        // Publish changed presentation and refresh explicitly after late activation.
        // Qt ignores these events before its enable button is used.
        QAccessibleEvent event(this, QAccessible::ObjectShow);
        QAccessible::updateAccessibility(&event);
    }

private:
    QPointer<QAbstractItemView> m_view;
    QPersistentModelIndex m_index;
    QPointer<QWindow> m_window;
    QString m_text;
    QString m_description;
    QRect m_rect;
    bool m_visible = false;
};

// The list must expose the same slots when Qt walks the tree on late activation;
// otherwise Qt would also publish its original, unbounded model-cell children.
class VisibleListAccessible final : public QAccessibleWidget {
public:
    VisibleListAccessible(QAbstractItemView* view, const QList<VisibleListCell*>* cells)
        : QAccessibleWidget(view, qobject_cast<QTreeView*>(view) ? QAccessible::Tree : QAccessible::List),
          m_cells(cells)
    {
        static quint64 nextTree = 0;
        if (qobject_cast<QTreeView*>(view))
            m_identifier = QStringLiteral("fluentWasmVisibleTree_%1").arg(++nextTree);
    }
    QString text(QAccessible::Text type) const override
    {
        return type == QAccessible::Identifier && !m_identifier.isEmpty()
            ? m_identifier
            : QAccessibleWidget::text(type);
    }
    int childCount() const override { return m_cells->size(); }
    QAccessibleInterface* child(int index) const override
    {
        return index >= 0 && index < m_cells->size() ? m_cells->at(index) : nullptr;
    }
    int indexOfChild(const QAccessibleInterface* child) const override
    {
        for (int i = 0; i < m_cells->size(); ++i) {
            if (m_cells->at(i) == child)
                return i;
        }
        return -1;
    }
    QAccessibleInterface* childAt(int x, int y) const override
    {
        for (auto* cell : *m_cells) {
            if (!cell->state().invisible && cell->rect().contains(x, y))
                return cell;
        }
        return nullptr;
    }

private:
    const QList<VisibleListCell*>* m_cells;
    QString m_identifier;
};
#endif

class ListAccessibilitySynchronizer final : public QObject {
public:
    explicit ListAccessibilitySynchronizer(QAbstractItemView* view) : QObject(view), m_view(view)
    {
#if QT_CONFIG(accessibility)
        ensureAccessibleInterface();
#endif
        view->installEventFilter(this);
        view->viewport()->installEventFilter(this);
        connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { schedule(); });
        connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] { schedule(); });
        if (auto* tree = qobject_cast<QTreeView*>(view)) {
            connect(tree, &QTreeView::expanded, this, [this] { schedule(); });
            connect(tree, &QTreeView::collapsed, this, [this] { schedule(); });
        }
        schedule();
    }
    ~ListAccessibilitySynchronizer() override { hideCells(); }
    void refreshAfterActivation()
    {
        m_lastPresentation.clear();
        schedule();
    }

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::Hide:
            hideCells();
            break;
        case QEvent::Show:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::LayoutRequest:
        case QEvent::Timer:
        case QEvent::FontChange:
        case QEvent::StyleChange:
        case QEvent::LayoutDirectionChange:
        case QEvent::EnabledChange:
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            schedule();
            break;
        case QEvent::Paint:
            // Hover/selection animations repaint without changing accessible data.
            // Observe inherited model/root/column setters and ancestor movement here;
            // model, selection, expansion and scrolling already have explicit signals.
            // zh_CN: 悬停动画不改变语义；绘制时只补查继承的模型/根/列 setter 和祖先移动，
            // 数据、选择、展开与滚动通过各自信号刷新。
            if (presentationChanged())
                schedule();
            break;
        default:
            break;
        }
        return false;
    }

private:
#if QT_CONFIG(accessibility)
    void ensureAccessibleInterface()
    {
        auto* current = QAccessible::queryAccessibleInterface(m_view);
        if (m_accessibleId && current == QAccessible::accessibleInterface(m_accessibleId))
            return;
        // Reparenting a popup can replace Qt's cached interface after Show.
        // Reinstall our bounded presentation before publishing the next frame.
        // zh_CN: 弹层重挂载可能在 Show 后替换 Qt 缓存接口，下一帧发布前恢复有界展示。
        // Qt 6.9.3 keys its DOM cache by interface address and never removes
        // retired entries. Allocate first, otherwise the allocator may reuse
        // current's address and Qt will keep its old DOM id/element type.
        // zh_CN: Qt 以接口地址缓存 DOM 且不清理旧项；先分配替代接口，避免复用旧地址。
        auto* replacement = new VisibleListAccessible(m_view, &m_cells);
        m_lastPresentation.clear();
        if (current) {
            QAccessibleEvent hidden(current, QAccessible::ObjectHide);
            QAccessible::updateAccessibility(&hidden);
            QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(current));
        }
        m_accessibleId = QAccessible::registerAccessibleInterface(replacement);
    }
#endif
    void hideCells()
    {
        m_lastPresentation.clear();
#if QT_CONFIG(accessibility)
        for (auto* cell : std::as_const(m_cells))
            cell->hide();
        const QByteArray rootId = m_rootDomId.toUtf8();
        const QByteArray controllerId = m_controllerDomId.toUtf8();
        // Slots are reused on reset: never leave active-descendant pointing at
        // a hidden row or at the previous query's identity.
        // zh_CN: 重置时复用展示槽，清理指向隐藏行或旧查询的活动项关系。
        // clang-format off
        EM_ASM({
            for (const container of Module['qtContainerElements'] || []) {
                const shadow = container.querySelector('#qt-shadow-container')?.shadowRoot;
                const root = shadow?.getElementById(UTF8ToString($0));
                root?.removeAttribute('aria-activedescendant');
                root?.removeAttribute('aria-owns');
                const combo = shadow?.getElementById(UTF8ToString($1));
                combo?.removeAttribute('aria-activedescendant');
                if (combo && !$2) combo.setAttribute('aria-expanded', 'false');
            }
        }, rootId.constData(), controllerId.constData(), int(m_view && m_view->isVisible()));
        // clang-format on
#endif
    }
    void schedule()
    {
        if (m_pending)
            return;
        m_pending = true;
        QTimer::singleShot(0, this, [this] {
            m_pending = false;
            synchronize();
        });
    }
    void synchronize()
    {
#if QT_CONFIG(accessibility)
        ensureAccessibleInterface();
        auto* model = m_view->model();
        if (m_model != model) {
            hideCells();
            for (const auto& connection : std::as_const(m_connections))
                disconnect(connection);
            m_connections.clear();
            m_model = model;
            if (model) {
                const auto before = [this] { hideCells(); };
                const auto after = [this] { schedule(); };
                m_connections
                    << connect(model, &QAbstractItemModel::modelAboutToBeReset, this, before)
                    << connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, before)
                    << connect(model, &QAbstractItemModel::rowsAboutToBeMoved, this, before)
                    << connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, before)
                    << connect(model, &QObject::destroyed, this, before)
                    << connect(model, &QAbstractItemModel::modelReset, this, after)
                    << connect(model, &QAbstractItemModel::rowsInserted, this, after)
                    << connect(model, &QAbstractItemModel::rowsRemoved, this, after)
                    << connect(model, &QAbstractItemModel::rowsMoved, this, after)
                    << connect(model, &QAbstractItemModel::dataChanged, this, after)
                    << connect(model, &QAbstractItemModel::layoutChanged, this, after);
            }
        }
        if (m_selectionModel != m_view->selectionModel()) {
            for (const auto& connection : std::as_const(m_selectionConnections))
                disconnect(connection);
            m_selectionConnections.clear();
            m_selectionModel = m_view->selectionModel();
            if (m_selectionModel) {
                m_selectionConnections << connect(m_selectionModel, &QItemSelectionModel::selectionChanged, this,
                    [this] {
                        schedule();
                    }) << connect(m_selectionModel, &QItemSelectionModel::currentChanged, this, [this] { schedule(); });
            }
        }
        if (!model || !m_view->isVisible() || model->rowCount(m_view->rootIndex()) == 0) {
            hideCells();
            rememberPresentation();
            return;
        }
        const QRect viewport = m_view->viewport()->rect();
        QModelIndex first;
        for (int y = 0; y < viewport.height() && !first.isValid(); ++y)
            first = m_view->indexAt(QPoint(viewport.center().x(), y));
        auto* tree = qobject_cast<QTreeView*>(m_view);
        auto* list = qobject_cast<QListView*>(m_view);
        if (list && first.isValid())
            first = first.sibling(first.row(), list->modelColumn());
        int used = 0;
        int visited = 0;
        QJsonArray cells;
        bool checkable = false;
        // Only visible, expanded rows get presentation slots. Keep a fixed cap
        // even for malformed custom layouts; never materialize the whole tree.
        for (QModelIndex index = first; index.isValid() && visited++ < 128;) {
            const QRect rect = m_view->visualRect(index);
            if (rect.top() > viewport.bottom())
                break;
            if (rect.intersects(viewport)) {
                if (used == m_cells.size()) {
                    auto* owner = new QObject(this);
                    auto* cell = new VisibleListCell(owner, m_view);
                    const auto id = QAccessible::registerAccessibleInterface(cell);
                    owner->setObjectName((
                        tree ? QStringLiteral("fluentWasmVisibleTreeCell_%1")
                             : QStringLiteral("fluentWasmVisibleListCell_%1"))
                            .arg(id));
                    m_cells.append(cell);
                }
                auto* cell = m_cells.at(used++);
                const QRect screenRect(m_view->viewport()->mapToGlobal(rect.topLeft()), rect.size());
                cell->present(index, screenRect, m_view->window()->windowHandle());
                int level = 1;
                if (tree)
                    for (auto parent = index.parent(); parent.isValid() && parent != m_view->rootIndex();
                        parent = parent.parent())
                        ++level;
                const auto state = cell->state();
                const auto checked = index.data(Qt::CheckStateRole);
                checkable |= checked.isValid();
                cells.append(QJsonObject{{"id", accessibleDomId(cell)},
                    {"description", index.data(Qt::AccessibleDescriptionRole).toString()}, {"level", level},
                    {"position", index.row() + 1}, {"size", model->rowCount(index.parent())},
                    {"checked", checked.isValid() ? checked.toInt() : -1},
                    {"branch", tree && model->hasChildren(index)}, {"expanded", tree && tree->isExpanded(index)},
                    {"selected", bool(state.selected)}, {"disabled", bool(state.disabled)},
                    { "current",
                        m_view->currentIndex() == index }});
            }
            index = tree
                ? tree->indexBelow(index)
                : model->index(index.row() + 1, list->modelColumn(), m_view->rootIndex());
        }
        {
            auto* accessible = QAccessible::queryAccessibleInterface(m_view);
            QAccessibleEvent show(accessible, QAccessible::ObjectShow);
            QAccessible::updateAccessibility(&show);
            QString combo;
            // Prefer the explicit accessible Controller relation for borrowed models.
            // zh_CN: 借用模型时通过无障碍 Controller 关系定位所属输入框。
            for (auto* widget = QApplication::focusWidget(); widget; widget = widget->parentWidget()) {
                auto* candidate = QAccessible::queryAccessibleInterface(widget);
                if (!candidate)
                    continue;
                for (const auto& relation : candidate->relations(QAccessible::Controller)) {
                    if (relation.first && relation.first->object() == m_view) {
                        m_controller = widget;
                        combo = accessibleDomId(candidate);
                        break;
                    }
                }
                if (!combo.isEmpty())
                    break;
            }
            // A popup may be reparented to the desktop, while its presentation
            // model retains the owning ComboBox. Never bind unrelated navigation
            // trees to whichever editor happens to have focus.
            // zh_CN: 弹层重挂载后由展示模型定位所属输入框，不关联恰好聚焦的其他控件。
            auto* ownerWidget = qobject_cast<QWidget*>(model->parent());
            auto* owner = ownerWidget ? QAccessible::queryAccessibleInterface(ownerWidget) : nullptr;
            if (combo.isEmpty() && owner && owner->role() == QAccessible::ComboBox) {
                m_controller = ownerWidget;
                combo = accessibleDomId(owner);
            }
            if (combo.isEmpty() && m_controller) {
                if (auto* controller = QAccessible::queryAccessibleInterface(m_controller))
                    combo = accessibleDomId(controller);
            }
            const bool selectable = m_view->selectionMode() != QAbstractItemView::NoSelection;
            const bool multiple = (tree && checkable)
                || (selectable && m_view->selectionMode() != QAbstractItemView::SingleSelection);
            m_rootDomId = accessibleDomId(accessible);
            m_controllerDomId = combo;
            const QByteArray presentation = QJsonDocument(
                QJsonObject{{"id", accessibleDomId(accessible)}, {"combo", combo},
                    {"name", accessible->text(QAccessible::Name)}, {"multiple", multiple}, {"tree", tree != nullptr},
                    {"selectable", selectable},
                    { "cells",
                        cells }}).toJson(QJsonDocument::Compact);
            if (presentation != m_lastPresentation) {
                m_lastPresentation = presentation;
                // Qt 6.9.3 creates these proxies but omits list/tree roles and item states.
                // Annotate Qt-owned nodes; Qt remains responsible for their lifetime,
                // geometry, visibility and the editor's native keyboard/IME events.
                // zh_CN: 补齐 Qt 已创建节点的列表与树语义，沿用 Qt 的生命周期与输入事件桥。
                // clang-format off
            EM_ASM({
                const data = JSON.parse(UTF8ToString($0));
                for (const container of Module['qtContainerElements'] || []) {
                    const shadow = container.querySelector('#qt-shadow-container')?.shadowRoot;
                    const root = shadow?.getElementById(data.id);
                    if (!root) continue;
                    const role = data.tree ? 'tree' : data.selectable ? 'listbox' : 'list';
                    root.setAttribute('role', role);
                    root.setAttribute('aria-label', data.name);
                    if (data.tree || data.selectable) root.setAttribute('aria-multiselectable', String(data.multiple));
                    else root.removeAttribute('aria-multiselectable');
                    root.setAttribute('aria-owns', data.cells.map(cell => cell.id).join(' '));
                    const current = data.cells.find(cell => cell.current && !cell.disabled);
                    if (current) root.setAttribute('aria-activedescendant', current.id);
                    else root.removeAttribute('aria-activedescendant');
                    const combo = shadow.getElementById(data.combo);
                    if (combo) {
                        combo.setAttribute('role', 'combobox');
                        combo.setAttribute('aria-expanded', 'true');
                        combo.setAttribute('aria-haspopup', role);
                        combo.setAttribute('aria-controls', data.id);
                        if (current) combo.setAttribute('aria-activedescendant', current.id);
                        else combo.removeAttribute('aria-activedescendant');
                    }
                    for (const cell of data.cells) {
                        const node = shadow.getElementById(cell.id);
                        if (!node) continue;
                        node.setAttribute('role', data.tree ? 'treeitem' : data.selectable ? 'option' : 'listitem');
                        node.setAttribute('aria-description', cell.description);
                        if (data.tree) node.setAttribute('aria-level', String(cell.level));
                        else node.removeAttribute('aria-level');
                        node.setAttribute('aria-posinset', String(cell.position));
                        node.setAttribute('aria-setsize', String(cell.size));
                        if (data.tree || data.selectable) node.setAttribute('aria-selected', String(cell.selected));
                        else node.removeAttribute('aria-selected');
                        node.setAttribute('aria-disabled', String(cell.disabled));
                        if (cell.branch) node.setAttribute('aria-expanded', String(cell.expanded));
                        else node.removeAttribute('aria-expanded');
                        if (cell.checked >= 0) node.setAttribute('aria-checked', cell.checked === 1 ? 'mixed' : String(cell.checked === 2));
                        else node.removeAttribute('aria-checked');
                    }
                }
            }, presentation.constData());
                // clang-format on
            }
        }
        for (int i = used; i < m_cells.size(); ++i)
            m_cells.at(i)->hide();
        rememberPresentation();
#endif
    }
    bool presentationChanged() const
    {
#if QT_CONFIG(accessibility)
        if (!m_view)
            return false;
        const auto* list = qobject_cast<QListView*>(m_view);
        return m_model != m_view->model() || m_selectionModel != m_view->selectionModel()
            || m_presentedRoot != m_view->rootIndex() || m_presentedWindow != m_view->window()
            || m_presentedViewport != QRect(m_view->viewport()->mapToGlobal(QPoint()), m_view->viewport()->size())
            || m_presentedColumn != (list ? list->modelColumn() : 0) || m_presentedMode != m_view->selectionMode();
#else
        return false;
#endif
    }
    void rememberPresentation()
    {
        if (!m_view)
            return;
        const auto* list = qobject_cast<QListView*>(m_view);
        m_presentedRoot = m_view->rootIndex();
        m_presentedWindow = m_view->window();
        m_presentedViewport = QRect(m_view->viewport()->mapToGlobal(QPoint()), m_view->viewport()->size());
        m_presentedColumn = list ? list->modelColumn() : 0;
        m_presentedMode = m_view->selectionMode();
    }
    QPointer<QAbstractItemView> m_view;
    QPointer<QAbstractItemModel> m_model;
    QPointer<QItemSelectionModel> m_selectionModel;
    QPointer<QWidget> m_controller;
    QList<QMetaObject::Connection> m_connections;
    QList<QMetaObject::Connection> m_selectionConnections;
    QPersistentModelIndex m_presentedRoot;
    QPointer<QWidget> m_presentedWindow;
    QRect m_presentedViewport;
    int m_presentedColumn = -1;
    QAbstractItemView::SelectionMode m_presentedMode = QAbstractItemView::NoSelection;
    QByteArray m_lastPresentation;
#if QT_CONFIG(accessibility)
    QList<VisibleListCell*> m_cells;
    QAccessible::Id m_accessibleId = 0;
    QString m_rootDomId;
    QString m_controllerDomId;
#endif
    bool m_pending = false;
};

class MenuAccessibilitySynchronizer final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
#if QT_CONFIG(accessibility)
        // A transient view may already have its explicit visible flag when its
        // popup is reparented. Its viewport's first paint is a second attachment
        // opportunity; ordinary frames keep the same synchronizer and slots.
        // zh_CN: 重挂载的临时视图可能没有新的 Show；首帧 viewport 绘制补充一次接入。
        if (event->type() == QEvent::Paint) {
            auto* viewport = qobject_cast<QWidget*>(watched);
            auto* tree = viewport ? qobject_cast<QTreeView*>(viewport->parentWidget()) : nullptr;
            if (tree && viewport == tree->viewport() && !tree->property("_fluent_wasmTreeAccessibility").toBool()) {
                tree->setProperty("_fluent_wasmTreeAccessibility", true);
                new ListAccessibilitySynchronizer(tree);
            }
            return false;
        }
        if (event->type() != QEvent::Show)
            return false;
        // Keep interface registration at Show, including before browser activation.
        // Qt's backend retains interface addresses; defer only redundant repaint work.
        // zh_CN: 保留 Show 阶段的接口登记；Qt 后端缓存接口地址，只跳过重复绘制刷新。
        if (auto* widget = qobject_cast<QWidget*>(watched)) {
            auto* accessible = QAccessible::queryAccessibleInterface(widget);
            if (accessible && accessible->role() == QAccessible::ComboBox
                && !widget->property("_fluent_wasmComboAccessibility").toBool()) {
                widget->setProperty("_fluent_wasmComboAccessibility", true);
                new ComboBoxAccessibilitySynchronizer(widget);
            }
        }
        if (auto* list = qobject_cast<QListView*>(watched)) {
            if (list->viewMode() == QListView::ListMode && list->flow() == QListView::TopToBottom && !list->isWrapping()
                && !list->property("_fluent_wasmListAccessibility").toBool()) {
                list->setProperty("_fluent_wasmListAccessibility", true);
                new ListAccessibilitySynchronizer(list);
            }
        }
        if (auto* tree = qobject_cast<QTreeView*>(watched)) {
            if (!tree->property("_fluent_wasmTreeAccessibility").toBool()) {
                tree->setProperty("_fluent_wasmTreeAccessibility", true);
                new ListAccessibilitySynchronizer(tree);
            }
        }
        auto* menu = qobject_cast<QMenu*>(watched);
        if (!menu || !menu->isVisible() || !menu->windowHandle() || !menu->windowHandle()->handle())
            return false;

        constexpr auto pending = "_fluent_wasmMenuAccessibilityPending";
        if (menu->property(pending).toBool())
            return false;
        menu->setProperty(pending, true);
        // The menu owns the queued call: deletion cancels it, and a close in
        // the same event-loop turn must not publish a hidden popup's children.
        // zh_CN: 回调绑定到菜单生命周期；销毁会取消回调，同轮关闭也不会发布隐藏项。
        QTimer::singleShot(0, menu, [menu]() {
            menu->setProperty("_fluent_wasmMenuAccessibilityPending", false);
            if (!menu->isVisible() || !menu->windowHandle() || !menu->windowHandle()->handle())
                return;
            auto* accessible = QAccessible::queryAccessibleInterface(menu);
            if (!accessible)
                return;
            for (int i = 0; i < accessible->childCount(); ++i) {
                auto* item = accessible->child(i);
                if (!item || !item->isValid())
                    continue;
                QAccessibleEvent visibility(item,
                    item->state().invisible ? QAccessible::ObjectHide : QAccessible::ObjectShow);
                QAccessible::updateAccessibility(&visibility);
            }
        });
#else
        Q_UNUSED(watched);
        Q_UNUSED(event);
#endif
        return false;
    }
};

void publishPrimaryWindowGeometry()
{
    QWidget* window = primaryHostedWindow();
    if (!window || !window->parentWidget())
        return;

    const QRect geometry = window->geometry();
    const bool maximized = geometry == window->parentWidget()->rect();
    EM_ASM(
        {
            const root = document.documentElement;
            root.dataset.fluentQtWindowX = String($0);
            root.dataset.fluentQtWindowY = String($1);
            root.dataset.fluentQtWindowWidth = String($2);
            root.dataset.fluentQtWindowHeight = String($3);
            root.dataset.fluentQtWindowMaximized = $4 ? 'true' : 'false';
        },
        geometry.x(), geometry.y(), geometry.width(), geometry.height(), maximized);
}

class BrowserDesktopSurface final : public QWidget {
public:
    using QWidget::QWidget;

    ~BrowserDesktopSurface() override
    {
#if QT_CONFIG(accessibility)
        // Qt WASM retains a raw root pointer; clear it before the widgets die.
        // zh_CN: Qt WASM 保存原始根指针，需在控件销毁前清除。
        QAccessible::setRootObject(nullptr);
#endif
    }

    void hostWindow(QWidget* window, const QRect& normalGeometry, WindowPresentation presentation)
    {
        if (!window)
            return;

        window->setParent(this, Qt::Widget);
        window->installEventFilter(this);
        if (!primaryHostedWindow()) {
            primaryHostedWindow() = window;
            connect(window, &QObject::destroyed, this, [] { primaryHostedWindow().clear(); });
        }

        // Apply the normal geometry first so Fluent Window can retain a
        // meaningful restore rectangle even when the requested presentation is
        // maximized.
        // zh_CN: 先施加普通几何，使 Fluent Window 即使初始为最大化展示，也能保留
        // 有意义的还原矩形。
        window->setGeometry(constrainedGeometry(normalGeometry, this));
        if (presentation == WindowPresentation::Maximized)
            window->setGeometry(rect());
        window->show();
        window->raise();
        publishPrimaryWindowGeometry();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == primaryHostedWindow() && event) {
            switch (event->type()) {
            case QEvent::Move:
            case QEvent::Resize:
            case QEvent::Show:
            case QEvent::Hide:
                QTimer::singleShot(0, this, &publishPrimaryWindowGeometry);
                break;
            default:
                break;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        const QRect previousStage(QPoint(0, 0), event->oldSize());
        QWidget::resizeEvent(event);
        const QRect nextStage = rect();
        const auto windows = findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget* window : windows) {
            if (window->geometry() == previousStage) {
                window->setGeometry(nextStage);
                continue;
            }
            window->setGeometry(constrainedGeometry(window->geometry(), this));
        }
        publishPrimaryWindowGeometry();
    }
};

void ensureDesktopSurface()
{
    if (!qApp)
        return;

    // QApplication has now created the Qt screen shadow trees. Scope the
    // stylesheet to this module's screens and reuse it for later proxy nodes.
    // zh_CN: QApplication 已创建 Qt screen 的 shadow tree；样式仅作用于当前
    // 模块的屏幕，后续新增代理节点也复用同一规则。
    configureAccessibleProxyPresentation();
    if (desktopSurface())
        return;

    auto* surface = new BrowserDesktopSurface();
    qApp->installEventFilter(new MenuAccessibilitySynchronizer(surface));
    surface->setObjectName(QStringLiteral("fluentQtBrowserDesktopSurface"));
    surface->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    surface->setAttribute(Qt::WA_TranslucentBackground);
    surface->setAutoFillBackground(false);
    desktopSurface() = surface;
#if QT_CONFIG(accessibility)
    // Browser launchers return to JavaScript instead of entering QApplication::exec(),
    // which normally registers the accessible root. This single desktop owns
    // every hosted window, so enabling a screen reader can traverse their tree.
    // zh_CN: 浏览器启动器直接返回 JavaScript，不进入通常负责注册无障碍根的
    // QApplication::exec()。单一桌面拥有所有承载窗口，读屏启用后可遍历完整树。
    QAccessible::setRootObject(surface);
#endif
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, surface, &QObject::deleteLater);
    // Qt WASM assigns a browser-sized canvas to every top-level QWidget. Own
    // exactly one such surface and host Fluent application windows inside it;
    // this preserves real widget move/resize geometry without a fake HTML
    // title bar or duplicated full-screen canvases. The surface must remain
    // focusable: Qt's browser input context focuses a hidden HTML input only
    // while its owning QWindow accepts activation. WindowDoesNotAcceptFocus or
    // WA_ShowWithoutActivating would leave mouse input working but silently
    // discard all real keyboard and IME input for hosted text controls.
    // zh_CN: Qt WASM 会为每个顶层 QWidget 分配浏览器大小的画布。这里只保留一个
    // 桌面表面并在其中承载 Fluent 应用窗口，从而保留真实控件移动/缩放几何，且无需
    // 伪 HTML 标题栏或重复的全屏画布。该表面必须允许聚焦：Qt 浏览器输入上下文
    // 仅会在所属 QWindow 可激活时聚焦隐藏 HTML input；禁用焦点会造成鼠标仍可用，
    // 但所有文本控件的真实键盘及输入法输入被静默丢弃。
    surface->showMaximized();
    surface->lower();
}

} // namespace

// Private browser callback for Qt's one-time screen-reader enable button.
// Scheduling keeps Qt's tree population and subsequent publication separate.
// zh_CN: Qt 浏览器读屏启用按钮的内部回调；排队刷新，等待原生语义树完成创建。
extern "C" EMSCRIPTEN_KEEPALIVE void fluentqt_refresh_accessible_views()
{
#if QT_CONFIG(accessibility)
    for (auto* widget : QApplication::allWidgets()) {
        if (widget->isVisible() && !widget->property("_fluent_wasmComboAccessibility").toBool()) {
            auto* accessible = QAccessible::queryAccessibleInterface(widget);
            if (accessible && accessible->role() == QAccessible::ComboBox) {
                widget->setProperty("_fluent_wasmComboAccessibility", true);
                new ComboBoxAccessibilitySynchronizer(widget);
            }
        }
        const auto children = widget->children();
        for (auto* child : children) {
            if (auto* synchronizer = dynamic_cast<ListAccessibilitySynchronizer*>(child))
                synchronizer->refreshAfterActivation();
            if (auto* synchronizer = dynamic_cast<ComboBoxAccessibilitySynchronizer*>(child))
                synchronizer->refreshAfterActivation();
        }
    }
#endif
}

void configureRuntime()
{
    compatibility::detail::RuntimePlatformCapabilities capabilities;
    capabilities.customWindowChromePreferred = true;
    capabilities.clientSideFrameMargin = 12;
    capabilities.manualMoveResizeFallback = true;
    // Qt's browser popup windows do not provide a dependable alpha surface.
    // Keep menus opaque and animation-stable while regular Fluent windows use
    // the Qt screen compositor.
    // zh_CN: Qt 浏览器 popup 窗口没有稳定的 alpha 表面；菜单使用不透明、
    // 无揭示闪烁的表面，普通 Fluent Window 仍由 Qt screen compositor 管理。
    capabilities.translucentPopupSurfaces = false;
    capabilities.hostsApplicationWindowsInDesktopSurface = true;
    // A transparent child title bar forces the software Mica field below it to
    // repaint and looks soft at reduced backing-store DPR. Give browser chrome
    // an opaque token surface and cache the otherwise static painted window
    // material; desktop compositors keep their existing behavior.
    // zh_CN: 透明子标题栏会迫使下方软件 Mica 重绘，并在降低 backing-store DPR
    // 时显得发虚。浏览器 chrome 使用不透明 token 表面，同时缓存静态窗口材质；
    // 桌面合成器行为保持不变。
    capabilities.opaqueClientTitleBarSurface = true;
    capabilities.cachePaintedWindowSurfaces = true;
    compatibility::detail::setRuntimePlatformCapabilities(capabilities);
}

void showWindow(QWidget* window, const QRect& normalGeometry, WindowPresentation presentation)
{
    if (!window)
        return;

    ensureDesktopSurface();
    BrowserDesktopSurface* surface = desktopSurface();
    if (!surface)
        return;
    surface->hostWindow(window, normalGeometry, presentation);

    // Reassert once Qt has completed the first child layout turn. This remains
    // QWidget-only; browser-specific ownership stays inside this adapter.
    // zh_CN: Qt 完成首轮子控件布局后再确认一次。公共组件仍只处理 QWidget，
    // 浏览器特有的宿主所有权完全留在该适配层。
    const QPointer<QWidget> guard(window);
    const QPointer<BrowserDesktopSurface> surfaceGuard(surface);
    QTimer::singleShot(0, window, [guard, normalGeometry]() {
        if (guard)
            guard->update();
    });
    QTimer::singleShot(100, window, [guard, surfaceGuard, normalGeometry, presentation]() {
        if (guard && surfaceGuard) {
            const QRect geometry = presentation == WindowPresentation::Maximized
                ? surfaceGuard->rect()
                : constrainedGeometry(normalGeometry, surfaceGuard);
            guard->setGeometry(geometry);
            guard->raise();
            publishPrimaryWindowGeometry();
        }
    });
}

} // namespace fluent::webassembly

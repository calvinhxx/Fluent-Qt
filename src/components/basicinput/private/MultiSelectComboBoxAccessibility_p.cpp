#include "MultiSelectComboBoxAccessibility_p.h"

#include <QAbstractItemView>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QListView>
#include <QWidget>

#include "compatibility/QtCompat.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/collections/ListView.h"
#include "components/foundation/private/LogicalItemAccessibility_p.h"

namespace fluent::basicinput::detail {

#if QT_CONFIG(accessibility)

using accessibility::detail::LogicalItemAccessibleAdapter;
using accessibility::detail::LogicalItemAccessibleState;

class MultiSelectComboBoxSelectAllAccessible final : public QAccessibleWidget {
public:
  explicit MultiSelectComboBoxSelectAllAccessible(QCheckBox *checkBox)
      : QAccessibleWidget(checkBox, QAccessible::CheckBox) {}

  QAccessible::State state() const override {
    QAccessible::State result = QAccessibleWidget::state();
    const Qt::CheckState checkState =
        view() ? view()->checkState() : Qt::Unchecked;
    result.checkable = true;
    result.checked = checkState == Qt::Checked;
    result.checkStateMixed = checkState == Qt::PartiallyChecked;
    return result;
  }

  QString text(QAccessible::Text type) const override {
    const QString inherited = QAccessibleWidget::text(type);
    if (!inherited.isEmpty())
      return inherited;
    return type == QAccessible::Name && view() ? view()->text() : inherited;
  }

  QStringList actionNames() const override {
    QStringList result = QAccessibleWidget::actionNames();
    if (!view() || !view()->isEnabled())
      return result;
    const QString press = QAccessibleActionInterface::pressAction();
    const QString toggle = QAccessibleActionInterface::toggleAction();
    if (!result.contains(press))
      result.append(press);
    if (!result.contains(toggle))
      result.append(toggle);
    return result;
  }

  void doAction(const QString &actionName) override {
    if (view() && view()->isEnabled() &&
        (actionName == QAccessibleActionInterface::pressAction() ||
         actionName == QAccessibleActionInterface::toggleAction())) {
      view()->setCheckState(view()->checkState() == Qt::Checked ? Qt::Unchecked
                                                                : Qt::Checked);
      return;
    }
    QAccessibleWidget::doAction(actionName);
  }

  QStringList keyBindingsForAction(const QString &actionName) const override {
    if (actionName == QAccessibleActionInterface::pressAction() ||
        actionName == QAccessibleActionInterface::toggleAction()) {
      return {QStringLiteral("Space")};
    }
    return QAccessibleWidget::keyBindingsForAction(actionName);
  }

private:
  QCheckBox *view() const { return static_cast<QCheckBox *>(widget()); }
};

class MultiSelectComboBoxListAccessible final
    : public LogicalItemAccessibleAdapter {
public:
  explicit MultiSelectComboBoxListAccessible(
      fluent::collections::ListView *list)
      : LogicalItemAccessibleAdapter(list, QAccessible::List) {}

  int logicalChildCount() const override {
    const auto *current = view();
    return current && current->model()
               ? current->model()->rowCount(current->rootIndex())
               : 0;
  }

  QAccessible::Role logicalChildRole(int) const override {
    return QAccessible::ListItem;
  }

  QString logicalChildText(int logicalIndex,
                           QAccessible::Text type) const override {
    if (type != QAccessible::Name && type != QAccessible::Value)
      return {};
    const QModelIndex index = modelIndex(logicalIndex);
    return index.isValid() ? index.data(Qt::DisplayRole).toString() : QString{};
  }

  QRect logicalChildRect(int logicalIndex) const override {
    const auto *current = view();
    const QModelIndex index = modelIndex(logicalIndex);
    if (!current || !current->viewport() || !index.isValid())
      return {};
    const QRect viewportRect =
        static_cast<const QListView *>(current)->visualRect(index);
    if (!viewportRect.isValid())
      return {};
    return toGlobalRect(
        QRect(current->viewport()->mapTo(
                  const_cast<fluent::collections::ListView *>(current),
                  viewportRect.topLeft()),
              viewportRect.size()));
  }

  LogicalItemAccessibleState
  logicalChildState(int logicalIndex) const override {
    LogicalItemAccessibleState result;
    const auto *current = view();
    const QModelIndex index = modelIndex(logicalIndex);
    result.valid = current && index.isValid();
    if (!result.valid) {
      result.invisible = true;
      result.offscreen = true;
      return result;
    }
    const Qt::ItemFlags flags = index.flags();
    result.enabled = current->isEnabled() && flags.testFlag(Qt::ItemIsEnabled);
    result.selectable = flags.testFlag(Qt::ItemIsSelectable);
    result.focusable = result.selectable;
    result.selected = current->selectionModel() &&
                      current->selectionModel()->isSelected(index);
    result.focused = current->hasFocus() && current->currentIndex() == index;
    result.invisible = !current->isVisible();
    result.offscreen =
        !current->viewport() ||
        !current->viewport()->rect().intersects(
            static_cast<const QListView *>(current)->visualRect(index));
    return result;
  }

  int logicalFocusChild() const override {
    const auto *current = view();
    return current && current->currentIndex().isValid()
               ? current->currentIndex().row()
               : -1;
  }

  QStringList logicalChildActions(int logicalIndex) const override {
    const LogicalItemAccessibleState item = logicalChildState(logicalIndex);
    return item.valid && item.enabled && item.selectable
               ? QStringList{QAccessibleActionInterface::pressAction(),
                             QAccessibleActionInterface::toggleAction()}
               : QStringList{};
  }

  QStringList
  logicalChildKeyBindings(int, const QString &actionName) const override {
    if (actionName == QAccessibleActionInterface::pressAction() ||
        actionName == QAccessibleActionInterface::toggleAction())
      return {QStringLiteral("Space"), QStringLiteral("Enter")};
    return {};
  }

  void performLogicalChildAction(int logicalIndex,
                                 const QString &actionName) override {
    if (actionName != QAccessibleActionInterface::pressAction() &&
        actionName != QAccessibleActionInterface::toggleAction())
      return;
    const QModelIndex index = modelIndex(logicalIndex);
    auto *current = view();
    if (!current || !index.isValid() ||
        !logicalChildState(logicalIndex).enabled ||
        !current->selectionModel()) {
      return;
    }
    current->selectionModel()->select(index, QItemSelectionModel::Toggle |
                                                 QItemSelectionModel::Rows);
    current->selectionModel()->setCurrentIndex(index,
                                               QItemSelectionModel::NoUpdate);
    if (current->viewport())
      current->viewport()->update();
  }

  bool setLogicalChildSelected(int logicalIndex, bool selected) override {
    const QModelIndex index = modelIndex(logicalIndex);
    auto *current = view();
    if (!current || !index.isValid() ||
        !logicalChildState(logicalIndex).enabled ||
        !current->selectionModel()) {
      return false;
    }
    current->selectionModel()->select(
        index, (selected ? QItemSelectionModel::Select
                         : QItemSelectionModel::Deselect) |
                   QItemSelectionModel::Rows);
    return current->selectionModel()->isSelected(index) == selected;
  }

  bool clearLogicalSelection() override {
    auto *current = view();
    if (!current || !current->selectionModel())
      return false;
    current->selectionModel()->clearSelection();
    return true;
  }

private:
  fluent::collections::ListView *view() const {
    return static_cast<fluent::collections::ListView *>(widget());
  }

  QModelIndex modelIndex(int logicalIndex) const {
    const auto *current = view();
    if (!current || !current->model() || logicalIndex < 0)
      return {};
    return current->model()->index(logicalIndex, current->modelColumn(),
                                   current->rootIndex());
  }
};

class MultiSelectComboBoxAccessible final : public QAccessibleWidget {
public:
  explicit MultiSelectComboBoxAccessible(MultiSelectComboBox *box)
      : QAccessibleWidget(box, QAccessible::ButtonMenu) {}

  QAccessible::State state() const override {
    QAccessible::State result = QAccessibleWidget::state();
    MultiSelectComboBox *box = view();
    if (!box)
      return result;
    result.focusable = box->isEnabled() && box->focusPolicy() != Qt::NoFocus;
    result.focused = box->hasFocus();
    result.hasPopup = true;
    result.expandable = true;
    result.expanded = box->isOpen();
    result.collapsed = !box->isOpen();
    return result;
  }

  QString text(QAccessible::Text type) const override {
    const QString inherited = QAccessibleWidget::text(type);
    if (!inherited.isEmpty())
      return inherited;
    if (type == QAccessible::Value && view())
      return view()->accessibleValueText();
    return inherited;
  }

  int childCount() const override { return 0; }

  QAccessibleInterface *child(int) const override { return nullptr; }

  int indexOfChild(const QAccessibleInterface *) const override { return -1; }

  FluentAccessibleRelationList
  relations(QAccessible::Relation match) const override {
    auto result = QAccessibleWidget::relations(match);
    if (!(match & QAccessible::Controller) || !view())
      return result;
    QWidget *controller = view()->accessibilityController();
    QAccessibleInterface *target =
        controller ? QAccessible::queryAccessibleInterface(controller)
                   : nullptr;
    if (target)
      result.append({target, QAccessible::Controller});
    return result;
  }

  QStringList actionNames() const override {
    QStringList result = QAccessibleWidget::actionNames();
    if (!view() || !view()->isEnabled())
      return result;
    const QString press = QAccessibleActionInterface::pressAction();
    const QString showMenu = QAccessibleActionInterface::showMenuAction();
    if (!result.contains(press))
      result.append(press);
    if (!result.contains(showMenu))
      result.append(showMenu);
    return result;
  }

  void doAction(const QString &actionName) override {
    if (view() && view()->isEnabled() &&
        actionName == QAccessibleActionInterface::pressAction()) {
      view()->setIsOpen(!view()->isOpen());
      return;
    }
    if (view() && view()->isEnabled() &&
        actionName == QAccessibleActionInterface::showMenuAction()) {
      view()->open();
      return;
    }
    QAccessibleWidget::doAction(actionName);
  }

  QStringList keyBindingsForAction(const QString &actionName) const override {
    if (actionName == QAccessibleActionInterface::pressAction())
      return {QStringLiteral("Space"), QStringLiteral("Enter")};
    if (actionName == QAccessibleActionInterface::showMenuAction())
      return {QStringLiteral("Alt+Down"), QStringLiteral("F4")};
    return QAccessibleWidget::keyBindingsForAction(actionName);
  }

private:
  MultiSelectComboBox *view() const {
    return static_cast<MultiSelectComboBox *>(widget());
  }
};

namespace {

QAccessibleInterface *multiSelectComboBoxAccessibilityFactory(const QString &,
                                                              QObject *object) {
  auto *box = dynamic_cast<MultiSelectComboBox *>(object);
  if (box)
    return new MultiSelectComboBoxAccessible(box);
  auto *selectAll = qobject_cast<QCheckBox *>(object);
  if (selectAll && selectAll->objectName() ==
                       QStringLiteral("MultiSelectComboBox.SelectAll")) {
    return new MultiSelectComboBoxSelectAllAccessible(selectAll);
  }
  auto *list = dynamic_cast<fluent::collections::ListView *>(object);
  if (list &&
      list->objectName() == QStringLiteral("MultiSelectComboBox.ListView")) {
    return new MultiSelectComboBoxListAccessible(list);
  }
  return nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureMultiSelectComboBoxAccessibilityFactory() {
#if QT_CONFIG(accessibility)
  static const bool installed = [] {
    QAccessible::installFactory(multiSelectComboBoxAccessibilityFactory);
    return true;
  }();
  Q_UNUSED(installed)
#endif
}

void notifyMultiSelectComboBoxOpenChanged(MultiSelectComboBox *box) {
#if QT_CONFIG(accessibility)
  if (!box)
    return;
  QAccessible::State changed;
  changed.expanded = true;
  changed.collapsed = true;
  QAccessibleStateChangeEvent event(box, changed);
  QAccessible::updateAccessibility(&event);
#else
  Q_UNUSED(box)
#endif
}

void notifyMultiSelectComboBoxSelectionChanged(MultiSelectComboBox *box,
                                               bool countChanged) {
#if QT_CONFIG(accessibility)
  if (!box)
    return;
  QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(box);
  const QString value =
      interface ? interface->text(QAccessible::Value) : QString{};
  QAccessibleValueChangeEvent valueEvent(box, QVariant(value));
  QAccessible::updateAccessibility(&valueEvent);
  if (countChanged) {
    QAccessibleEvent descriptionEvent(box, QAccessible::DescriptionChanged);
    QAccessible::updateAccessibility(&descriptionEvent);
  }
#else
  Q_UNUSED(box)
  Q_UNUSED(countChanged)
#endif
}

} // namespace fluent::basicinput::detail

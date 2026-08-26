#ifndef MULTISELECTCOMBOBOX_H
#define MULTISELECTCOMBOBOX_H

#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QList>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QEvent;
class QFocusEvent;
class QKeyEvent;

namespace fluent::basicinput {

class MultiSelectComboBoxPopup;
class MultiSelectComboBoxTrigger;

namespace detail {
class MultiSelectComboBoxAccessible;
}

/**
 * @brief Model-backed Fluent dropdown for selecting several rows.
 * zh_CN: 基于 model、用于选择多行的 Fluent 下拉框。
 *
 * MultiSelectComboBox keeps the existing ComboBox single-select contract
 * untouched. It owns only presentation state and a default selection model;
 * applications retain ownership of their data model and may supply a shared
 * QItemSelectionModel.
 * zh_CN: MultiSelectComboBox 不改变现有 ComboBox 的单选契约。控件仅持有展示状态
 * 和默认 selection model；应用继续拥有数据 model，也可提供共享的
 * QItemSelectionModel。
 */
class MultiSelectComboBox : public QWidget,
                            public FluentElement,
                            public QMLPlus {
  Q_OBJECT

  /**
   * @brief Caller-owned source model displayed by the dropdown.
   * zh_CN: 下拉框展示的、由调用方持有的源 model。
   */
  Q_PROPERTY(
      QAbstractItemModel *model READ model WRITE setModel NOTIFY modelChanged)
  /**
   * @brief Selection model used as the component value.
   * zh_CN: 作为控件值使用的 selection model。
   */
  Q_PROPERTY(QItemSelectionModel *selectionModel READ selectionModel WRITE
                 setSelectionModel NOTIFY selectionModelChanged)
  /**
   * @brief Source-model column used for labels and filtering.
   * zh_CN: 用于标签展示与筛选的源 model 列。
   */
  Q_PROPERTY(int modelColumn READ modelColumn WRITE setModelColumn NOTIFY
                 modelColumnChanged)
  /**
   * @brief Source-model root whose direct children form the option list.
   * zh_CN: 其直属子项构成选项列表的源 model 根索引。
   */
  Q_PROPERTY(QModelIndex rootModelIndex READ rootModelIndex WRITE
                 setRootModelIndex NOTIFY rootModelIndexChanged)
  /**
   * @brief Text shown while no selectable row is selected.
   * zh_CN: 未选择任何可选行时显示的文本。
   */
  Q_PROPERTY(QString placeholderText READ placeholderText WRITE
                 setPlaceholderText NOTIFY placeholderTextChanged)
  /**
   * @brief Whether a local substring-search field is shown in the popup.
   * zh_CN: 弹层中是否显示本地子串搜索输入框。
   */
  Q_PROPERTY(bool searchEnabled READ isSearchEnabled WRITE setSearchEnabled
                 NOTIFY searchEnabledChanged)
  /**
   * @brief Placeholder text used by the popup search field.
   * zh_CN: 弹层搜索输入框使用的占位文本。
   */
  Q_PROPERTY(QString searchPlaceholderText READ searchPlaceholderText WRITE
                 setSearchPlaceholderText NOTIFY searchPlaceholderTextChanged)
  /**
   * @brief Whether the popup shows a tri-state select-all action.
   * zh_CN: 弹层是否显示三态全选操作。
   */
  Q_PROPERTY(bool selectAllVisible READ isSelectAllVisible WRITE
                 setSelectAllVisible NOTIFY selectAllVisibleChanged)
  /**
   * @brief Maximum number of option rows visible before scrolling.
   * zh_CN: 开始滚动前最多显示的选项行数。
   */
  Q_PROPERTY(int maximumVisibleItems READ maximumVisibleItems WRITE
                 setMaximumVisibleItems NOTIFY maximumVisibleItemsChanged)
  /**
   * @brief Logical popup open state.
   * zh_CN: 弹层的逻辑打开状态。
   */
  Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
  /**
   * @brief Number of selected selectable rows under rootModelIndex.
   * zh_CN: rootModelIndex 下已选中且可选择的行数。
   */
  Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)

public:
  explicit MultiSelectComboBox(QWidget *parent = nullptr);
  ~MultiSelectComboBox() override;

  QAbstractItemModel *model() const { return m_model.data(); }
  void setModel(QAbstractItemModel *model);

  QItemSelectionModel *selectionModel() const {
    return m_selectionModel.data();
  }
  void setSelectionModel(QItemSelectionModel *selectionModel);

  int modelColumn() const { return m_modelColumn; }
  void setModelColumn(int column);

  QModelIndex rootModelIndex() const { return m_rootModelIndex; }
  void setRootModelIndex(const QModelIndex &index);

  QString placeholderText() const { return m_placeholderText; }
  void setPlaceholderText(const QString &text);

  bool isSearchEnabled() const { return m_searchEnabled; }
  void setSearchEnabled(bool enabled);

  QString searchPlaceholderText() const { return m_searchPlaceholderText; }
  void setSearchPlaceholderText(const QString &text);

  bool isSelectAllVisible() const { return m_selectAllVisible; }
  void setSelectAllVisible(bool visible);

  int maximumVisibleItems() const { return m_maximumVisibleItems; }
  void setMaximumVisibleItems(int count);

  bool isOpen() const { return m_isOpen; }

  QList<int> selectedRows() const;
  QModelIndexList selectedIndexes() const;
  int selectedCount() const { return m_selectedCount; }
  bool isRowSelected(int row) const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  void onThemeUpdated() override;

public slots:
  /**
   * @brief Replaces selected rows under the active root.
   * zh_CN: 替换当前根索引下的已选行。
   */
  void setSelectedRows(const QList<int> &rows);
  /**
   * @brief Clears selected rows under the active root.
   * zh_CN: 清除当前根索引下的已选行。
   */
  void clearSelection();
  /**
   * @brief Selects every enabled and selectable source row under the root.
   * zh_CN: 选择当前根索引下所有启用且可选择的源行。
   */
  void selectAll();
  /**
   * @brief Opens the anchored dropdown.
   * zh_CN: 打开锚定下拉弹层。
   */
  void open();
  /**
   * @brief Closes the dropdown without rolling back selection.
   * zh_CN: 关闭下拉弹层且不回滚选择。
   */
  void close();
  void setIsOpen(bool open);

signals:
  void modelChanged(QAbstractItemModel *model);
  void selectionModelChanged(QItemSelectionModel *selectionModel);
  void modelColumnChanged(int column);
  void rootModelIndexChanged(const QModelIndex &index);
  void selectionChanged(const QItemSelection &selected,
                        const QItemSelection &deselected);
  void selectedCountChanged(int count);
  void placeholderTextChanged(const QString &text);
  void searchEnabledChanged(bool enabled);
  void searchPlaceholderTextChanged(const QString &text);
  void selectAllVisibleChanged(bool visible);
  void maximumVisibleItemsChanged(int count);
  void isOpenChanged(bool open);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  friend class MultiSelectComboBoxPopup;
  friend class MultiSelectComboBoxTrigger;
  friend class detail::MultiSelectComboBoxAccessible;

  void ensurePopup();
  void installInternalSelectionModel();
  void connectSelectionModelSignals();
  void disconnectSelectionModelSignals();
  void connectModelSignals();
  void disconnectModelSignals();
  void handleSelectionChanged(const QItemSelection &selected,
                              const QItemSelection &deselected);
  void refreshSelectionState(bool notifyAccessibility = true);
  void refreshPresentation();
  void refreshPopup();
  void setRowsSelected(const QModelIndexList &indexes, bool selected);
  QModelIndex sourceIndexForRow(int row) const;
  bool isSelectable(const QModelIndex &index) const;
  QString displayTextForWidth(int width) const;
  QString accessibleValueText() const;
  QWidget *accessibilityController() const;
  void togglePopupFromTrigger();
  void advanceFocus(bool next);

  QPointer<QAbstractItemModel> m_model;
  QPointer<QItemSelectionModel> m_selectionModel;
  bool m_ownsSelectionModel = false;
  QVector<QMetaObject::Connection> m_modelConnections;
  QVector<QMetaObject::Connection> m_selectionConnections;
  QPersistentModelIndex m_rootModelIndex;
  int m_modelColumn = 0;

  QString m_placeholderText;
  QString m_searchPlaceholderText;
  bool m_searchEnabled = false;
  bool m_selectAllVisible = true;
  int m_maximumVisibleItems = 6;

  int m_selectedCount = 0;
  QStringList m_selectedLabels;
  bool m_isOpen = false;
  bool m_ignoreNextTriggerClick = false;

  MultiSelectComboBoxTrigger *m_trigger = nullptr;
  QPointer<MultiSelectComboBoxPopup> m_popup;
};

} // namespace fluent::basicinput

#endif // MULTISELECTCOMBOBOX_H

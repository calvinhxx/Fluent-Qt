#include "MultiSelectComboBox.h"

#include <algorithm>
#include <utility>

#include <QAbstractItemView>
#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLocale>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QtMath>

#include "compatibility/QtCompat.h"
#include "compatibility/TextPaintCompat.h"
#include "components/basicinput/CheckBox.h"
#include "components/basicinput/DropDownButton.h"
#include "components/basicinput/private/MultiSelectComboBoxAccessibility_p.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/private/LogicalItemAccessibility_p.h"
#include "components/textfields/LineEdit.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::basicinput {

namespace {

constexpr int kDefaultWidth = 240;
constexpr int kMinimumWidth = 120;
constexpr int kPopupMinimumCardWidth = 240;
constexpr int kPopupWindowMargin = 4;
constexpr int kPopupShadowMargin = ::Spacing::Standard;
constexpr int kPopupContentInset = ::Spacing::XSmall;
constexpr int kPopupRowHeight = ::Spacing::ControlHeight::Large;
constexpr int kPopupEmptyHeight = 56;
constexpr int kCheckBoxSize = 20;
constexpr int kOptionOuterInset = ::Spacing::XSmall;
constexpr int kOptionContentInset = ::Spacing::Small;
constexpr int kOptionTextGap = ::Spacing::Small;
constexpr int kSummaryBadgeHeight = 22;
constexpr int kSummaryBadgeHorizontalPadding = ::Spacing::Small;
constexpr auto kKeyboardFocusVisibleProperty = "fluentKeyboardFocusVisible";
constexpr auto kKeyboardFocusOnOpenProperty = "fluentKeyboardFocusOnOpen";

bool indexIsSelectable(const QModelIndex &index) {
  if (!index.isValid())
    return false;
  const Qt::ItemFlags flags = index.flags();
  return flags.testFlag(Qt::ItemIsEnabled) &&
         flags.testFlag(Qt::ItemIsSelectable);
}

QItemSelection selectionForIndexes(const QModelIndexList &indexes) {
  QItemSelection selection;
  for (const QModelIndex &index : indexes) {
    if (index.isValid())
      selection.select(index, index);
  }
  return selection;
}

} // namespace

class MultiSelectComboBoxTrigger final : public DropDownButton {
public:
  explicit MultiSelectComboBoxTrigger(MultiSelectComboBox *owner)
      : DropDownButton(owner), m_owner(owner) {
    setObjectName(QStringLiteral("MultiSelectComboBox.Trigger"));
    setText(QString());
    setFluentStyle(Button::Standard);
    setFluentSize(Button::StandardSize);
    setFocusPolicy(Qt::NoFocus);
    setFocusVisual(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(::Spacing::ControlHeight::Standard);
  }

  QSize sizeHint() const override {
    return QSize(kDefaultWidth, ::Spacing::ControlHeight::Standard);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    DropDownButton::paintEvent(event);
    if (!m_owner)
      return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(m_owner->font());

    const int trailingReserve = qMax(0, chevronOffset().x()) +
                                qMax(0, chevronSize()) + ::Spacing::Gap::Normal;
    const QRect logicalTextRect = rect().adjusted(
        ::Spacing::Padding::ComboBoxHorizontal, 0, -trailingReserve, 0);
    const QRect textSlot =
        QStyle::visualRect(layoutDirection(), rect(), logicalTextRect);

    const bool hasSelection = m_owner->selectedCount() > 0;
    QColor textColor = isEnabled()
                           ? (hasSelection ? themeColorsRef().textPrimary
                                           : themeColorsRef().textTertiary)
                           : themeColorsRef().textDisabled;
    const QString text = m_owner->displayTextForWidth(textSlot.width());
    const QFontMetricsF metrics(painter.font());
    const bool usesCompactSummary =
        hasSelection && metrics.horizontalAdvance(
                            m_owner->accessibleValueText()) > textSlot.width();

    if (usesCompactSummary) {
      const int badgeWidth =
          qMin(textSlot.width(), qCeil(metrics.horizontalAdvance(text)) +
                                     kSummaryBadgeHorizontalPadding * 2);
      const int badgeHeight = qMin(kSummaryBadgeHeight, textSlot.height());
      const QRect badgeRect = QStyle::alignedRect(
          layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter,
          QSize(badgeWidth, badgeHeight), textSlot);
      painter.setPen(QPen(themeColorsRef().strokeDivider, 1.0));
      painter.setBrush(themeColorsRef().controlSecondary);
      painter.drawRoundedRect(QRectF(badgeRect).adjusted(0.5, 0.5, -0.5, -0.5),
                              badgeHeight / 2.0, badgeHeight / 2.0);

      const QRect badgeTextRect =
          badgeRect.adjusted(kSummaryBadgeHorizontalPadding, 0,
                             -kSummaryBadgeHorizontalPadding, 0);
      const QString elided = metrics.elidedText(text, Qt::ElideRight,
                                                qMax(0, badgeTextRect.width()));
      painter.setPen(textColor);
      const QRectF inkRect = fluent::painting::verticallyCenteredTextInkRect(
          QRectF(badgeTextRect), metrics, elided);
      painter.drawText(inkRect, Qt::AlignHCenter | Qt::AlignVCenter, elided);
    } else {
      painter.setPen(textColor);
      const QString elided =
          metrics.elidedText(text, Qt::ElideRight, qMax(0, textSlot.width()));
      const QRectF inkRect = fluent::painting::verticallyCenteredTextInkRect(
          QRectF(textSlot), metrics, elided);
      painter.drawText(inkRect,
                       QStyle::visualAlignment(
                           layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter),
                       elided);
    }

    if (isEnabled() && m_owner->hasFocus()) {
      QColor focusColor = themeColorsRef().textSecondary;
      focusColor.setAlpha(120);
      painter.setPen(QPen(focusColor, 1.0));
      painter.setBrush(Qt::NoBrush);
      const qreal radius = themeRadius().control;
      painter.drawRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5),
                              radius, radius);
    }
  }

private:
  MultiSelectComboBox *m_owner = nullptr;
};

class MultiSelectComboBoxSelectAll final : public CheckBox {
public:
  explicit MultiSelectComboBoxSelectAll(QWidget *parent) : CheckBox(parent) {
    setTristate(true);
  }

protected:
  void nextCheckState() override {
    setCheckState(checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
  }

  void paintEvent(QPaintEvent *event) override {
    CheckBox::paintEvent(event);
    QPainter painter(this);
    painter.setPen(QPen(themeColorsRef().strokeDivider, 1.0));
    painter.drawLine(0, height() - 1, width(), height() - 1);
  }
};

class MultiSelectComboBoxListView final : public fluent::collections::ListView {
public:
  using fluent::collections::ListView::ListView;

protected:
  void paintEvent(QPaintEvent *event) override {
    QPainter surfacePainter(viewport());
    surfacePainter.setCompositionMode(QPainter::CompositionMode_Source);
    surfacePainter.fillRect(event ? event->rect() : viewport()->rect(),
                            themeColorsRef().bgLayer);
    surfacePainter.end();
    fluent::collections::ListView::paintEvent(event);
  }
};

class MultiSelectComboBoxItemDelegate final : public QStyledItemDelegate {
public:
  MultiSelectComboBoxItemDelegate(MultiSelectComboBox *owner,
                                  QAbstractItemView *view, QObject *parent)
      : QStyledItemDelegate(parent), m_owner(owner), m_view(view) {}

  QSize sizeHint(const QStyleOptionViewItem &,
                 const QModelIndex &) const override {
    return QSize(0, kPopupRowHeight);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!painter || !index.isValid() || !m_owner)
      return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);

    const auto colors = m_owner->themeColors();
    const auto radius = m_owner->themeRadius();
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const bool focused =
        option.state.testFlag(QStyle::State_HasFocus) ||
        (m_view && m_view->hasFocus() && m_view->currentIndex() == index);
    const bool keyboardFocused =
        focused && m_view &&
        m_view->property(kKeyboardFocusVisibleProperty).toBool();
    const bool enabled = indexIsSelectable(index);

    const QRect logicalBackground =
        option.rect.adjusted(kOptionOuterInset, 2, -kOptionOuterInset, -2);
    const QRect background =
        QStyle::visualRect(option.direction, option.rect, logicalBackground);
    QColor backgroundColor = Qt::transparent;
    if (enabled && hovered && !keyboardFocused)
      backgroundColor = colors.subtleSecondary;
    if (backgroundColor.alpha() > 0) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(backgroundColor);
      painter->drawRoundedRect(background, radius.control, radius.control);
    }

    const int checkY =
        option.rect.top() + (option.rect.height() - kCheckBoxSize) / 2;
    const QRect logicalCheck(option.rect.left() + kOptionOuterInset +
                                 kOptionContentInset,
                             checkY, kCheckBoxSize, kCheckBoxSize);
    const QRect checkRect =
        QStyle::visualRect(option.direction, option.rect, logicalCheck);

    QColor checkFill;
    QColor checkStroke;
    QColor checkGlyph;
    if (!enabled) {
      checkFill = colors.controlDisabled;
      checkStroke = colors.strokeDivider;
      checkGlyph = colors.textDisabled;
    } else if (selected) {
      checkFill = hovered ? colors.accentSecondary : colors.accentDefault;
      checkStroke = Qt::transparent;
      checkGlyph = colors.textOnAccent;
    } else {
      checkFill = hovered ? colors.controlSecondary : colors.controlDefault;
      checkStroke = hovered ? colors.strokeStrong : colors.strokeDefault;
      checkGlyph = Qt::transparent;
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(checkFill);
    painter->drawRoundedRect(checkRect, radius.control, radius.control);
    if (checkStroke != Qt::transparent) {
      painter->setPen(QPen(checkStroke, 1.0));
      painter->setBrush(Qt::NoBrush);
      painter->drawRoundedRect(QRectF(checkRect).adjusted(0.5, 0.5, -0.5, -0.5),
                               radius.control, radius.control);
    }
    if (selected) {
      painter->setPen(checkGlyph);
      Typography::Icons::paintGlyph(
          *painter, QRectF(checkRect), Typography::Icons::CheckMark,
          Typography::IconSize::Compact, Qt::AlignCenter);
    }

    int logicalTextLeft = logicalCheck.right() + 1 + kOptionTextGap;
    const QVariant decoration = index.data(Qt::DecorationRole);
    QIcon icon = qvariant_cast<QIcon>(decoration);
    if (!icon.isNull()) {
      const int iconSize = Typography::IconSize::Standard;
      const QRect logicalIcon(logicalTextLeft,
                              option.rect.center().y() - iconSize / 2, iconSize,
                              iconSize);
      const QRect iconRect =
          QStyle::visualRect(option.direction, option.rect, logicalIcon);
      icon.paint(painter, iconRect, Qt::AlignCenter,
                 enabled ? QIcon::Normal : QIcon::Disabled);
      logicalTextLeft = logicalIcon.right() + 1 + kOptionTextGap;
    }

    const QRect logicalText(logicalTextLeft, option.rect.top(),
                            qMax(0, option.rect.right() - kOptionContentInset -
                                        logicalTextLeft + 1),
                            option.rect.height());
    const QRect textSlot =
        QStyle::visualRect(option.direction, option.rect, logicalText);
    painter->setFont(option.font);
    painter->setPen(enabled ? colors.textPrimary : colors.textDisabled);
    const QString label = index.data(Qt::DisplayRole).toString();
    const QFontMetricsF metrics(painter->font());
    const QString elided =
        metrics.elidedText(label, Qt::ElideRight, qMax(0, textSlot.width()));
    const QRectF textRect = fluent::painting::verticallyCenteredTextInkRect(
        QRectF(textSlot), metrics, elided);
    painter->drawText(textRect,
                      QStyle::visualAlignment(option.direction,
                                              Qt::AlignLeft | Qt::AlignVCenter),
                      elided);

    if (keyboardFocused && enabled) {
      QColor focusColor = colors.accentDefault;
      focusColor.setAlpha(180);
      painter->setPen(QPen(focusColor, 1.0));
      painter->setBrush(Qt::NoBrush);
      painter->drawRoundedRect(
          QRectF(background).adjusted(0.5, 0.5, -0.5, -0.5), radius.control,
          radius.control);
    }

    painter->restore();
  }

private:
  MultiSelectComboBox *m_owner = nullptr;
  QAbstractItemView *m_view = nullptr;
};

class MultiSelectComboBoxPopup final : public fluent::dialogs_flyouts::Flyout {
public:
  explicit MultiSelectComboBoxPopup(MultiSelectComboBox *owner)
      : Flyout(owner), m_owner(owner) {
    setObjectName(QStringLiteral("MultiSelectComboBox.Popup"));
    setAnimationEnabled(false);
    setPlacement(Flyout::Auto);
    setAnchorOffset(::Spacing::Small);
    setModal(false);
    setDim(false);
    setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));

    // Popup reserves its shadow through QWidget contents margins. This popup's
    // layout already includes that shadow in outerInset so keeping both would
    // shift every fixed-width child right by one shadow margin and paint it
    // outside the visible card.
    setContentsMargins(0, 0, 0, 0);
    m_layout = new QVBoxLayout(this);
    const int outerInset = kPopupShadowMargin + kPopupContentInset;
    m_layout->setContentsMargins(outerInset, outerInset, outerInset,
                                 outerInset);
    m_layout->setSpacing(::Spacing::XSmall);

    m_searchEdit = new fluent::textfields::LineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("MultiSelectComboBox.Search"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedHeight(::Spacing::ControlHeight::Standard);
    m_layout->addWidget(m_searchEdit);

    m_selectAll = new MultiSelectComboBoxSelectAll(this);
    m_selectAll->setObjectName(QStringLiteral("MultiSelectComboBox.SelectAll"));
    m_selectAll->setHoverBackgroundEnabled(true);
    m_selectAll->setFixedHeight(::Spacing::ControlHeight::Standard);
    m_layout->addWidget(m_selectAll);

    m_listView = new MultiSelectComboBoxListView(this);
    m_listView->setObjectName(QStringLiteral("MultiSelectComboBox.ListView"));
    m_listView->setBorderVisible(false);
    m_listView->setBackgroundVisible(false);
    m_listView->setProperty("fluentPreserveParentSurface", true);
    if (m_listView->viewport())
      m_listView->viewport()->setProperty("fluentPreserveParentSurface", true);
    m_listView->setSelectionMode(fluent::collections::SelectionMode::Multiple);
    m_listView->setSelectionIndicatorVisible(false);
    m_listView->setSelectedIndicatorAnimationEnabled(false);
    m_listView->setProperty(kKeyboardFocusVisibleProperty, false);
    m_listView->setSpacing(0);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setItemDelegate(
        new MultiSelectComboBoxItemDelegate(owner, m_listView, m_listView));
    m_layout->addWidget(m_listView);

    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setDynamicSortFilter(true);

    connect(m_searchEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) {
              if (!m_owner || !m_owner->isSearchEnabled())
                return;
              const bool bridgeWasInProgress = m_bridgeInProgress;
              m_bridgeInProgress = true;
              m_proxyModel->setFilterFixedString(text);
              updateProxyRoot();
              m_bridgeInProgress = bridgeWasInProgress;
              syncSelectionFromOwner();
              updatePlaceholder();
              updatePopupGeometry();
              fluent::accessibility::detail::
                  notifyLogicalItemAccessibilityStructure(m_listView);
            });

    fluentConnectCheckStateChanged(
        m_selectAll, this, [this](Qt::CheckState state) {
          if (m_updatingSelectAll)
            return;
          setVisibleRowsSelected(state == Qt::Checked);
        });

    connect(this, &MultiSelectComboBoxPopup::closed, this, [this]() {
      if (m_searchEdit && !m_searchEdit->text().isEmpty())
        m_searchEdit->clear();
    });

    onThemeUpdated();
  }

  fluent::collections::ListView *listView() const { return m_listView; }

  void detachModelBinding(bool sourceIsBeingDestroyed = false) {
    const bool bridgeWasInProgress = m_bridgeInProgress;
    m_bridgeInProgress = true;
    if (m_proxySelectionConnection) {
      QObject::disconnect(m_proxySelectionConnection);
      m_proxySelectionConnection = {};
    }
    if (m_listView)
      m_listView->setModel(nullptr);
    if (m_proxyModel && !sourceIsBeingDestroyed)
      m_proxyModel->setSourceModel(nullptr);
    if (m_proxySelection) {
      m_proxySelection->deleteLater();
      m_proxySelection = nullptr;
    }
    m_bridgeInProgress = bridgeWasInProgress;
  }

  void prepareForOpen() {
    if (!m_owner)
      return;
    const Qt::LayoutDirection direction = m_owner->layoutDirection();
    setLayoutDirection(direction);
    m_searchEdit->setLayoutDirection(direction);
    m_selectAll->setLayoutDirection(direction);
    m_listView->setLayoutDirection(direction);
    m_searchEdit->setVisible(m_owner->isSearchEnabled());
    m_searchEdit->setPlaceholderText(m_owner->searchPlaceholderText());
    m_selectAll->setVisible(m_owner->isSelectAllVisible());
    m_selectAll->setText(m_owner->tr("Select all"));
    rebuildModelBinding();
    updatePlaceholder();
    syncSelectionFromOwner();
    updatePopupGeometry();
    setAnchor(m_owner);
  }

  void showForOwner() {
    prepareForOpen();
    if (!m_owner)
      return;
    if (isOpen()) {
      move(computePosition());
      raise();
    } else {
      showAt(m_owner);
    }
    focusInitialControl();
  }

  void syncSelectionFromOwner() {
    if (!m_owner || !m_listView)
      return;
    if (!m_owner->isSearchEnabled()) {
      if (m_listView->viewport())
        m_listView->viewport()->update();
      updateSelectAllState();
      return;
    }
    if (!m_proxySelection || !m_owner->selectionModel())
      return;

    m_bridgeInProgress = true;
    {
      QSignalBlocker blocker(m_proxySelection);
      m_proxySelection->clearSelection();
      const QItemSelection sourceSelection =
          m_owner->selectionModel()->selection();
      const QItemSelection proxySelection =
          m_proxyModel->mapSelectionFromSource(sourceSelection);
      if (!proxySelection.isEmpty()) {
        m_proxySelection->select(proxySelection, QItemSelectionModel::Select |
                                                     QItemSelectionModel::Rows);
      }
      const QModelIndex sourceCurrent =
          m_owner->selectionModel()->currentIndex();
      const QModelIndex proxyCurrent =
          m_proxyModel->mapFromSource(sourceCurrent);
      if (proxyCurrent.isValid()) {
        m_proxySelection->setCurrentIndex(proxyCurrent,
                                          QItemSelectionModel::NoUpdate);
      }
    }
    m_bridgeInProgress = false;
    if (m_listView->viewport())
      m_listView->viewport()->update();
    updateSelectAllState();
  }

  void notifySourceSelectionChanged(const QItemSelection &selected,
                                    const QItemSelection &deselected) {
    if (!m_listView || !m_listView->model())
      return;

    QSet<int> changedRows;
    const auto collectRows = [this,
                              &changedRows](const QItemSelection &selection) {
      for (const QModelIndex &source : selection.indexes()) {
        if (!source.isValid() || source.column() != m_owner->modelColumn()) {
          continue;
        }
        const QModelIndex visible = m_owner->isSearchEnabled()
                                        ? m_proxyModel->mapFromSource(source)
                                        : source;
        if (visible.isValid() && visible.parent() == m_listView->rootIndex()) {
          changedRows.insert(visible.row());
        }
      }
    };
    collectRows(selected);
    collectRows(deselected);
    for (int row : std::as_const(changedRows)) {
      fluent::accessibility::detail::notifyLogicalItemAccessibilitySelection(
          m_listView, row);
    }
  }

  void onThemeUpdated() override {
    Flyout::onThemeUpdated();
    if (m_owner) {
      m_listView->setFont(
          m_owner->themeFont(Typography::FontRole::Body).toQFont());
      m_searchEdit->setFontRole(Typography::FontRole::Body);
      m_selectAll->setFont(
          m_owner->themeFont(Typography::FontRole::Body).toQFont());
    }
    m_searchEdit->onThemeUpdated();
    m_selectAll->onThemeUpdated();
    if (m_listView->viewport())
      m_listView->viewport()->update();
    update();
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event && m_owner &&
        ::fluent::overlay::anchorGeometryMayChange(watched, event, m_owner)) {
      queueGeometryUpdate();
    }

    if (event && event->type() == QEvent::KeyPress && m_owner) {
      auto *keyEvent = static_cast<QKeyEvent *>(event);
      if (watched == m_searchEdit && keyEvent->key() == Qt::Key_Down) {
        focusFirstListRow(true);
        keyEvent->accept();
        return true;
      }
      if (watched == m_listView) {
        setKeyboardFocusVisible(true);
        const bool plainToggle = (keyEvent->key() == Qt::Key_Space ||
                                  keyEvent->key() == Qt::Key_Return ||
                                  keyEvent->key() == Qt::Key_Enter) &&
                                 keyEvent->modifiers() == Qt::NoModifier;
        if (plainToggle) {
          toggleCurrentRow();
          keyEvent->accept();
          return true;
        }
        if (keyEvent->matches(QKeySequence::SelectAll)) {
          const bool allSelected = m_selectAll->checkState() == Qt::Checked;
          setVisibleRowsSelected(!allSelected);
          keyEvent->accept();
          return true;
        }
      }
      if (keyEvent->key() == Qt::Key_Tab ||
          keyEvent->key() == Qt::Key_Backtab) {
        const bool next = keyEvent->key() != Qt::Key_Backtab &&
                          !keyEvent->modifiers().testFlag(Qt::ShiftModifier);
        close();
        m_owner->advanceFocus(next);
        keyEvent->accept();
        return true;
      }
    }

    if (event && event->type() == QEvent::MouseButtonPress && m_owner) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      const QPoint global = fluentMouseGlobalPos(mouseEvent);
      if (m_listView && m_listView->viewport()) {
        QWidget *viewport = m_listView->viewport();
        if (viewport->rect().contains(viewport->mapFromGlobal(global)))
          setKeyboardFocusVisible(false);
      }
      const QPoint ownerLocal = m_owner->mapFromGlobal(global);
      const bool pressOnOwner = m_owner->rect().contains(ownerLocal);
      const bool pressInsidePopup = ::fluent::overlay::visibleCardContains(
          rect(), mapFromGlobal(global), kPopupShadowMargin);
      if (pressOnOwner && !pressInsidePopup)
        m_owner->m_ignoreNextTriggerClick = true;
    }

    return Flyout::eventFilter(watched, event);
  }

private:
  void rebuildModelBinding() {
    if (!m_owner)
      return;

    if (m_proxySelectionConnection) {
      QObject::disconnect(m_proxySelectionConnection);
      m_proxySelectionConnection = {};
    }

    if (m_owner->isSearchEnabled()) {
      m_proxyModel->setSourceModel(m_owner->model());
      m_proxyModel->setFilterKeyColumn(m_owner->modelColumn());
      m_proxyModel->setFilterFixedString(m_searchEdit->text());

      if (m_proxySelection) {
        m_listView->setModel(nullptr);
        m_proxySelection->deleteLater();
      }
      m_proxySelection = new QItemSelectionModel(m_proxyModel, this);
      m_listView->setModel(m_proxyModel);
      m_listView->setSelectionModel(m_proxySelection);
      updateProxyRoot();
      m_listView->setModelColumn(m_owner->modelColumn());
      m_proxySelectionConnection = connect(
          m_proxySelection, &QItemSelectionModel::selectionChanged, this,
          [this](const QItemSelection &selected,
                 const QItemSelection &deselected) {
            applyProxySelectionToSource(selected, deselected);
          });
    } else {
      m_proxyModel->setSourceModel(nullptr);
      if (m_proxySelection) {
        m_listView->setModel(nullptr);
        m_proxySelection->deleteLater();
        m_proxySelection = nullptr;
      }
      m_listView->setModel(m_owner->model());
      if (m_owner->selectionModel())
        m_listView->setSelectionModel(m_owner->selectionModel());
      m_listView->setRootIndex(m_owner->rootModelIndex());
      m_listView->setModelColumn(m_owner->modelColumn());
    }

    m_listView->setSelectionMode(fluent::collections::SelectionMode::Multiple);
    m_listView->setSelectionIndicatorVisible(false);
    m_listView->refreshFluentScrollChrome();
    fluent::accessibility::detail::notifyLogicalItemAccessibilityStructure(
        m_listView);
  }

  void updateProxyRoot() {
    if (!m_owner)
      return;
    m_listView->setRootIndex(proxyRootIndex());
  }

  QModelIndex proxyRootIndex() const {
    if (!m_owner || !m_owner->rootModelIndex().isValid())
      return QModelIndex();
    return m_proxyModel->mapFromSource(m_owner->rootModelIndex());
  }

  void applyProxySelectionToSource(const QItemSelection &selected,
                                   const QItemSelection &deselected) {
    if (m_bridgeInProgress || !m_owner || !m_owner->selectionModel()) {
      return;
    }

    m_bridgeInProgress = true;
    const QItemSelection sourceDeselected =
        m_proxyModel->mapSelectionToSource(deselected);
    const QItemSelection sourceSelected =
        m_proxyModel->mapSelectionToSource(selected);
    if (!sourceDeselected.isEmpty()) {
      m_owner->selectionModel()->select(sourceDeselected,
                                        QItemSelectionModel::Deselect |
                                            QItemSelectionModel::Rows);
    }
    if (!sourceSelected.isEmpty()) {
      m_owner->selectionModel()->select(sourceSelected,
                                        QItemSelectionModel::Select |
                                            QItemSelectionModel::Rows);
    }
    const QModelIndex proxyCurrent =
        m_proxySelection ? m_proxySelection->currentIndex() : QModelIndex();
    const QModelIndex sourceCurrent = m_proxyModel->mapToSource(proxyCurrent);
    if (sourceCurrent.isValid()) {
      m_owner->selectionModel()->setCurrentIndex(sourceCurrent,
                                                 QItemSelectionModel::NoUpdate);
    }
    m_bridgeInProgress = false;
    updateSelectAllState();
  }

  QModelIndexList visibleSourceIndexes() const {
    QModelIndexList result;
    if (!m_owner || !m_listView || !m_listView->model())
      return result;
    const QModelIndex root = m_listView->rootIndex();
    const int rows = m_listView->model()->rowCount(root);
    result.reserve(rows);
    for (int row = 0; row < rows; ++row) {
      const QModelIndex visible =
          m_listView->model()->index(row, m_owner->modelColumn(), root);
      const QModelIndex source = m_owner->isSearchEnabled()
                                     ? m_proxyModel->mapToSource(visible)
                                     : visible;
      if (indexIsSelectable(source))
        result.append(source);
    }
    return result;
  }

  void setVisibleRowsSelected(bool selected) {
    if (!m_owner)
      return;
    m_owner->setRowsSelected(visibleSourceIndexes(), selected);
    syncSelectionFromOwner();
  }

  void updateSelectAllState() {
    if (!m_owner || !m_selectAll || !m_owner->isSelectAllVisible()) {
      return;
    }

    const bool mayHaveVisibleSelection =
        m_owner->isSearchEnabled()
            ? m_proxySelection && m_proxySelection->hasSelection()
            : m_owner->selectedCount() > 0;
    bool hasSelectable = false;
    bool hasSelected = false;
    bool hasUnselected = false;
    if (m_listView && m_listView->model()) {
      const QModelIndex root = m_listView->rootIndex();
      const int rows = m_listView->model()->rowCount(root);
      for (int row = 0; row < rows; ++row) {
        const QModelIndex visible =
            m_listView->model()->index(row, m_owner->modelColumn(), root);
        const QModelIndex source = m_owner->isSearchEnabled()
                                       ? m_proxyModel->mapToSource(visible)
                                       : visible;
        if (!indexIsSelectable(source))
          continue;

        hasSelectable = true;
        if (!mayHaveVisibleSelection) {
          hasUnselected = true;
          break;
        }
        if (m_owner->selectionModel() &&
            m_owner->selectionModel()->isSelected(source)) {
          hasSelected = true;
        } else {
          hasUnselected = true;
        }
        if (hasSelected && hasUnselected)
          break;
      }
    }

    Qt::CheckState state = Qt::Unchecked;
    if (hasSelected && !hasUnselected)
      state = Qt::Checked;
    else if (hasSelected)
      state = Qt::PartiallyChecked;

    m_updatingSelectAll = true;
    {
      QSignalBlocker blocker(m_selectAll);
      m_selectAll->setEnabled(hasSelectable);
      m_selectAll->setCheckState(state);
    }
    m_updatingSelectAll = false;
  }

  void updatePlaceholder() {
    if (!m_owner)
      return;
    const bool searching =
        m_owner->isSearchEnabled() && !m_searchEdit->text().isEmpty();
    m_listView->setPlaceholderText(searching ? m_owner->tr("No results")
                                             : m_owner->tr("No options"));
  }

  void updatePopupGeometry() {
    if (!m_owner || !m_listView)
      return;

    int cardWidth = qMax(m_owner->width(), kPopupMinimumCardWidth);
    if (QWidget *top = m_owner->window()) {
      const int available = ::fluent::overlay::overlaySurfaceRect(top).width() -
                            kPopupWindowMargin * 2;
      if (available > 0)
        cardWidth = qMin(cardWidth, available);
    }
    cardWidth = qMax(kMinimumWidth, cardWidth);
    const int contentWidth = qMax(1, cardWidth - kPopupContentInset * 2);

    const QModelIndex root = m_listView->rootIndex();
    const int rowCount =
        m_listView->model() ? m_listView->model()->rowCount(root) : 0;
    const int visibleRows =
        qMin(rowCount, qMax(1, m_owner->maximumVisibleItems()));
    const int desiredListHeight =
        rowCount > 0 ? qMax(kPopupRowHeight, visibleRows * kPopupRowHeight)
                     : kPopupEmptyHeight;

    m_searchEdit->setFixedWidth(contentWidth);
    m_selectAll->setFixedWidth(contentWidth);
    m_listView->setFixedSize(contentWidth, desiredListHeight);
    m_layout->invalidate();
    m_layout->activate();

    int listHeight = desiredListHeight;
    Flyout::Placement placement = Flyout::Auto;
    if (QWidget *top = m_owner->window()) {
      const QRect surface = ::fluent::overlay::overlaySurfaceRect(top);
      const QRect anchorRect(m_owner->mapTo(top, QPoint()), m_owner->size());
      const int desiredCardHeight =
          ::fluent::overlay::visibleCardSize(m_layout->sizeHint(),
                                             kPopupShadowMargin)
              .height();
      const int chromeHeight = qMax(0, desiredCardHeight - desiredListHeight);
      const int belowCapacity = surface.bottom() - kPopupWindowMargin -
                                (anchorRect.bottom() + anchorOffset()) + 1;
      const int aboveCapacity = anchorRect.top() - anchorOffset() -
                                (surface.top() + kPopupWindowMargin);
      const int minimumListHeight =
          rowCount > 0 ? kPopupRowHeight : kPopupEmptyHeight;
      const int minimumCardHeight = chromeHeight + minimumListHeight;

      int cardCapacity = belowCapacity;
      placement = Flyout::Bottom;
      if (belowCapacity < minimumCardHeight) {
        if (aboveCapacity >= minimumCardHeight ||
            aboveCapacity > belowCapacity) {
          placement = Flyout::Top;
          cardCapacity = aboveCapacity;
        }
      }

      const int availableListHeight = qMax(0, cardCapacity - chromeHeight);
      if (rowCount > 0 && availableListHeight >= kPopupRowHeight) {
        const int fittedRows = qMax(1, availableListHeight / kPopupRowHeight);
        listHeight = qMin(desiredListHeight, fittedRows * kPopupRowHeight);
      } else if (rowCount == 0 && availableListHeight > 0) {
        listHeight = qMin(desiredListHeight, availableListHeight);
      }
    }

    setPlacement(placement);
    m_listView->setFixedHeight(listHeight);
    m_layout->invalidate();
    m_layout->activate();
    setFixedSize(m_layout->sizeHint());
    m_listView->refreshFluentScrollChrome();
    if (isOpen() || isVisible())
      move(computePosition());
  }

  void queueGeometryUpdate() {
    if (m_geometryUpdatePending)
      return;
    m_geometryUpdatePending = true;
    QTimer::singleShot(0, this, [this]() {
      m_geometryUpdatePending = false;
      if (m_owner && (isOpen() || isVisible()))
        updatePopupGeometry();
    });
  }

  void focusInitialControl() {
    if (!m_owner)
      return;
    const QVariant focusOnOpen =
        m_owner->property(kKeyboardFocusOnOpenProperty);
    const bool keyboardFocusVisible =
        !focusOnOpen.isValid() || focusOnOpen.toBool();
    m_owner->setProperty(kKeyboardFocusOnOpenProperty, QVariant());
    setKeyboardFocusVisible(keyboardFocusVisible);
    if (m_owner->isSearchEnabled()) {
      m_searchEdit->setFocus(Qt::PopupFocusReason);
      return;
    }
    focusFirstListRow(keyboardFocusVisible);
  }

  void focusFirstListRow(bool keyboardFocusVisible) {
    if (!m_listView || !m_listView->model())
      return;
    setKeyboardFocusVisible(keyboardFocusVisible);
    QModelIndex target;
    const QModelIndex root = m_listView->rootIndex();
    if (m_listView->selectionModel()) {
      const QModelIndexList selected =
          m_listView->selectionModel()->selectedRows(
              m_owner ? m_owner->modelColumn() : 0);
      for (const QModelIndex &index : selected) {
        if (index.parent() == root && indexIsSelectable(index)) {
          target = index;
          break;
        }
      }
    }
    if (!target.isValid()) {
      const int rows = m_listView->model()->rowCount(root);
      for (int row = 0; row < rows; ++row) {
        const QModelIndex candidate = m_listView->model()->index(
            row, m_owner ? m_owner->modelColumn() : 0, root);
        if (indexIsSelectable(candidate)) {
          target = candidate;
          break;
        }
      }
    }
    if (target.isValid()) {
      m_listView->selectionModel()->setCurrentIndex(
          target, QItemSelectionModel::NoUpdate);
      m_listView->scrollTo(target);
    }
    m_listView->setFocus(Qt::PopupFocusReason);
  }

  void setKeyboardFocusVisible(bool visible) {
    if (!m_listView ||
        m_listView->property(kKeyboardFocusVisibleProperty).toBool() ==
            visible) {
      return;
    }
    m_listView->setProperty(kKeyboardFocusVisibleProperty, visible);
    if (m_listView->viewport())
      m_listView->viewport()->update();
  }

  void toggleCurrentRow() {
    if (!m_listView || !m_listView->selectionModel())
      return;
    const QModelIndex current = m_listView->currentIndex();
    if (!indexIsSelectable(current))
      return;
    m_listView->selectionModel()->select(
        current, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
    if (m_listView->viewport())
      m_listView->viewport()->update();
  }

  MultiSelectComboBox *m_owner = nullptr;
  QVBoxLayout *m_layout = nullptr;
  fluent::textfields::LineEdit *m_searchEdit = nullptr;
  CheckBox *m_selectAll = nullptr;
  fluent::collections::ListView *m_listView = nullptr;
  QSortFilterProxyModel *m_proxyModel = nullptr;
  QPointer<QItemSelectionModel> m_proxySelection;
  QMetaObject::Connection m_proxySelectionConnection;
  bool m_bridgeInProgress = false;
  bool m_updatingSelectAll = false;
  bool m_geometryUpdatePending = false;
};

MultiSelectComboBox::MultiSelectComboBox(QWidget *parent)
    : QWidget(parent), m_searchPlaceholderText(tr("Search")) {
  detail::ensureMultiSelectComboBoxAccessibilityFactory();
  setObjectName(QStringLiteral("MultiSelectComboBox"));
  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setFont(themeFont(Typography::FontRole::Body).toQFont());

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  m_trigger = new MultiSelectComboBoxTrigger(this);
  layout->addWidget(m_trigger);

  connect(m_trigger, &Button::pressed, this, [this]() {
    if (isEnabled()) {
      setProperty(kKeyboardFocusOnOpenProperty, false);
      setFocus(Qt::MouseFocusReason);
    }
  });
  connect(m_trigger, &Button::clicked, this,
          [this]() { togglePopupFromTrigger(); });

  installInternalSelectionModel();
  refreshSelectionState(false);
}

MultiSelectComboBox::~MultiSelectComboBox() {
  disconnectModelSignals();
  disconnectSelectionModelSignals();
  if (m_popup)
    delete m_popup.data();
  m_popup = nullptr;
}

void MultiSelectComboBox::setModel(QAbstractItemModel *model) {
  if (m_model == model)
    return;

  close();
  if (m_popup)
    m_popup->detachModelBinding();
  disconnectModelSignals();
  const bool rootChanged = m_rootModelIndex.isValid();
  m_rootModelIndex = QPersistentModelIndex();
  m_model = model;
  connectModelSignals();

  bool selectionModelReplaced = false;
  if (m_ownsSelectionModel || !m_selectionModel ||
      m_selectionModel->model() != m_model) {
    disconnectSelectionModelSignals();
    QPointer<QItemSelectionModel> oldSelection = m_selectionModel;
    const bool deleteOld = m_ownsSelectionModel;
    m_selectionModel = nullptr;
    m_ownsSelectionModel = false;
    installInternalSelectionModel();
    if (deleteOld && oldSelection)
      oldSelection->deleteLater();
    selectionModelReplaced = true;
  }

  refreshSelectionState();
  emit modelChanged(m_model.data());
  if (rootChanged)
    emit rootModelIndexChanged(QModelIndex());
  if (selectionModelReplaced)
    emit selectionModelChanged(m_selectionModel.data());
  refreshPopup();
}

void MultiSelectComboBox::setSelectionModel(
    QItemSelectionModel *selectionModel) {
  if (selectionModel && selectionModel->model() != m_model.data()) {
    return;
  }
  if (!selectionModel && m_ownsSelectionModel && m_selectionModel)
    return;
  if (selectionModel == m_selectionModel)
    return;

  disconnectSelectionModelSignals();
  QPointer<QItemSelectionModel> oldSelection = m_selectionModel;
  const bool deleteOld = m_ownsSelectionModel;
  m_selectionModel = nullptr;
  m_ownsSelectionModel = false;

  if (selectionModel) {
    m_selectionModel = selectionModel;
    connectSelectionModelSignals();
  } else {
    installInternalSelectionModel();
  }

  if (deleteOld && oldSelection)
    oldSelection->deleteLater();
  refreshSelectionState();
  emit selectionModelChanged(m_selectionModel.data());
  refreshPopup();
}

void MultiSelectComboBox::setModelColumn(int column) {
  if (column < 0 || m_modelColumn == column)
    return;
  m_modelColumn = column;
  refreshSelectionState();
  emit modelColumnChanged(m_modelColumn);
  refreshPopup();
}

void MultiSelectComboBox::setRootModelIndex(const QModelIndex &index) {
  if (index.isValid() && index.model() != m_model.data())
    return;
  if (m_rootModelIndex == index)
    return;
  m_rootModelIndex = QPersistentModelIndex(index);
  refreshSelectionState();
  emit rootModelIndexChanged(m_rootModelIndex);
  refreshPopup();
}

void MultiSelectComboBox::setPlaceholderText(const QString &text) {
  if (m_placeholderText == text)
    return;
  m_placeholderText = text;
  refreshPresentation();
  emit placeholderTextChanged(m_placeholderText);
}

void MultiSelectComboBox::setSearchEnabled(bool enabled) {
  if (m_searchEnabled == enabled)
    return;
  m_searchEnabled = enabled;
  emit searchEnabledChanged(m_searchEnabled);
  refreshPopup();
}

void MultiSelectComboBox::setSearchPlaceholderText(const QString &text) {
  if (m_searchPlaceholderText == text)
    return;
  m_searchPlaceholderText = text;
  emit searchPlaceholderTextChanged(m_searchPlaceholderText);
  refreshPopup();
}

void MultiSelectComboBox::setSelectAllVisible(bool visible) {
  if (m_selectAllVisible == visible)
    return;
  m_selectAllVisible = visible;
  emit selectAllVisibleChanged(m_selectAllVisible);
  refreshPopup();
}

void MultiSelectComboBox::setMaximumVisibleItems(int count) {
  count = qMax(1, count);
  if (m_maximumVisibleItems == count)
    return;
  m_maximumVisibleItems = count;
  emit maximumVisibleItemsChanged(m_maximumVisibleItems);
  refreshPopup();
}

QModelIndexList MultiSelectComboBox::selectedIndexes() const {
  QModelIndexList result;
  if (!m_model || !m_selectionModel)
    return result;

  const QModelIndexList rows = m_selectionModel->selectedRows(m_modelColumn);
  for (const QModelIndex &index : rows) {
    if (index.parent() == QModelIndex(m_rootModelIndex) &&
        isSelectable(index)) {
      result.append(index);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const QModelIndex &left, const QModelIndex &right) {
              return left.row() < right.row();
            });
  return result;
}

QList<int> MultiSelectComboBox::selectedRows() const {
  QList<int> result;
  const QModelIndexList indexes = selectedIndexes();
  result.reserve(indexes.size());
  for (const QModelIndex &index : indexes)
    result.append(index.row());
  return result;
}

bool MultiSelectComboBox::isRowSelected(int row) const {
  const QModelIndex index = sourceIndexForRow(row);
  return index.isValid() && m_selectionModel &&
         m_selectionModel->isSelected(index) && isSelectable(index);
}

void MultiSelectComboBox::setSelectedRows(const QList<int> &rows) {
  if (!m_model || !m_selectionModel)
    return;

  QModelIndexList replacement;
  const QModelIndexList current = m_selectionModel->selectedRows(m_modelColumn);
  for (const QModelIndex &index : current) {
    if (index.parent() != QModelIndex(m_rootModelIndex))
      replacement.append(index);
  }

  QSet<int> seen;
  for (int row : rows) {
    if (seen.contains(row))
      continue;
    seen.insert(row);
    const QModelIndex index = sourceIndexForRow(row);
    if (isSelectable(index))
      replacement.append(index);
  }

  m_selectionModel->select(selectionForIndexes(replacement),
                           QItemSelectionModel::ClearAndSelect |
                               QItemSelectionModel::Rows);
  if (!replacement.isEmpty()) {
    const QModelIndex first =
        sourceIndexForRow(rows.isEmpty() ? -1 : rows.first());
    if (first.isValid()) {
      m_selectionModel->setCurrentIndex(first, QItemSelectionModel::NoUpdate);
    }
  }
}

void MultiSelectComboBox::clearSelection() {
  if (!m_selectionModel)
    return;
  QModelIndexList keep;
  const QModelIndexList current = m_selectionModel->selectedRows(m_modelColumn);
  for (const QModelIndex &index : current) {
    if (index.parent() != QModelIndex(m_rootModelIndex))
      keep.append(index);
  }
  m_selectionModel->select(selectionForIndexes(keep),
                           QItemSelectionModel::ClearAndSelect |
                               QItemSelectionModel::Rows);
}

void MultiSelectComboBox::selectAll() {
  if (!m_model || !m_selectionModel)
    return;
  QModelIndexList indexes;
  const int rows = m_model->rowCount(m_rootModelIndex);
  indexes.reserve(rows);
  for (int row = 0; row < rows; ++row) {
    const QModelIndex index = sourceIndexForRow(row);
    if (isSelectable(index))
      indexes.append(index);
  }
  setRowsSelected(indexes, true);
}

void MultiSelectComboBox::open() {
  if (!isEnabled())
    return;
  if (!hasFocus())
    setFocus(Qt::PopupFocusReason);
  ensurePopup();
  m_popup->showForOwner();
}

void MultiSelectComboBox::close() {
  if (m_popup)
    m_popup->close();
}

void MultiSelectComboBox::setIsOpen(bool openState) {
  if (openState)
    open();
  else
    close();
}

QSize MultiSelectComboBox::sizeHint() const {
  return QSize(kDefaultWidth, ::Spacing::ControlHeight::Standard);
}

QSize MultiSelectComboBox::minimumSizeHint() const {
  return QSize(kMinimumWidth, ::Spacing::ControlHeight::Standard);
}

void MultiSelectComboBox::onThemeUpdated() {
  setFont(themeFont(Typography::FontRole::Body).toQFont());
  if (m_trigger) {
    m_trigger->setFont(font());
    m_trigger->onThemeUpdated();
    m_trigger->update();
  }
  if (m_popup)
    m_popup->onThemeUpdated();
  update();
}

void MultiSelectComboBox::keyPressEvent(QKeyEvent *event) {
  const bool altDown = event->key() == Qt::Key_Down &&
                       event->modifiers().testFlag(Qt::AltModifier);
  const bool f4 =
      event->key() == Qt::Key_F4 && event->modifiers() == Qt::NoModifier;
  const bool primary =
      (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return ||
       event->key() == Qt::Key_Enter) &&
      event->modifiers() == Qt::NoModifier;
  if (isEnabled() && (altDown || f4 || primary)) {
    setProperty(kKeyboardFocusOnOpenProperty, true);
    togglePopupFromTrigger();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_Escape && isOpen()) {
    close();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

void MultiSelectComboBox::focusInEvent(QFocusEvent *event) {
  QWidget::focusInEvent(event);
  if (m_trigger)
    m_trigger->update();
}

void MultiSelectComboBox::focusOutEvent(QFocusEvent *event) {
  QWidget::focusOutEvent(event);
  if (m_trigger)
    m_trigger->update();
}

void MultiSelectComboBox::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (!event)
    return;
  if (event->type() == QEvent::LanguageChange) {
    refreshPresentation();
    refreshPopup();
  } else if (event->type() == QEvent::LayoutDirectionChange) {
    if (m_trigger)
      m_trigger->update();
    refreshPopup();
  } else if (event->type() == QEvent::EnabledChange && m_trigger) {
    m_trigger->update();
    if (!isEnabled())
      close();
  }
}

void MultiSelectComboBox::ensurePopup() {
  if (m_popup)
    return;
  m_popup = new MultiSelectComboBoxPopup(this);
  connect(m_popup, &MultiSelectComboBoxPopup::isOpenChanged, this,
          [this](bool openState) {
            if (m_isOpen == openState)
              return;
            m_isOpen = openState;
            if (m_trigger)
              m_trigger->setOpen(openState);
            emit isOpenChanged(m_isOpen);
            detail::notifyMultiSelectComboBoxOpenChanged(this);
          });
}

QWidget *MultiSelectComboBox::accessibilityController() const {
  return m_popup && m_popup->isOpen() ? m_popup->listView() : nullptr;
}

void MultiSelectComboBox::installInternalSelectionModel() {
  m_selectionModel = new QItemSelectionModel(m_model.data(), this);
  m_ownsSelectionModel = true;
  connectSelectionModelSignals();
}

void MultiSelectComboBox::connectSelectionModelSignals() {
  if (!m_selectionModel)
    return;
  m_selectionConnections.append(
      connect(m_selectionModel, &QItemSelectionModel::selectionChanged, this,
              &MultiSelectComboBox::handleSelectionChanged));
  m_selectionConnections.append(
      connect(m_selectionModel, &QObject::destroyed, this, [this]() {
        m_selectionModel = nullptr;
        m_ownsSelectionModel = false;
        disconnectSelectionModelSignals();
        installInternalSelectionModel();
        refreshSelectionState();
        emit selectionModelChanged(m_selectionModel.data());
        refreshPopup();
      }));
}

void MultiSelectComboBox::disconnectSelectionModelSignals() {
  for (const QMetaObject::Connection &connection :
       std::as_const(m_selectionConnections)) {
    QObject::disconnect(connection);
  }
  m_selectionConnections.clear();
}

void MultiSelectComboBox::connectModelSignals() {
  if (!m_model)
    return;
  const auto refresh = [this]() {
    refreshSelectionState();
    refreshPopup();
  };
  m_modelConnections.append(connect(m_model, &QAbstractItemModel::dataChanged,
                                    this, [refresh]() { refresh(); }));
  m_modelConnections.append(connect(m_model, &QAbstractItemModel::rowsInserted,
                                    this, [refresh]() { refresh(); }));
  m_modelConnections.append(connect(m_model, &QAbstractItemModel::rowsRemoved,
                                    this, [refresh]() { refresh(); }));
  m_modelConnections.append(connect(m_model, &QAbstractItemModel::rowsMoved,
                                    this, [refresh]() { refresh(); }));
  m_modelConnections.append(connect(m_model, &QAbstractItemModel::layoutChanged,
                                    this, [refresh]() { refresh(); }));
  m_modelConnections.append(connect(
      m_model, &QAbstractItemModel::modelReset, this, [this, refresh]() {
        const bool rootWasValid = m_rootModelIndex.isValid();
        if (rootWasValid)
          m_rootModelIndex = QPersistentModelIndex();
        refresh();
        if (rootWasValid)
          emit rootModelIndexChanged(QModelIndex());
      }));
  m_modelConnections.append(
      connect(m_model, &QObject::destroyed, this, [this]() {
        m_model = nullptr;
        const bool rootChanged = m_rootModelIndex.isValid();
        m_rootModelIndex = QPersistentModelIndex();
        disconnectModelSignals();

        if (m_popup)
          m_popup->detachModelBinding(true);

        disconnectSelectionModelSignals();
        QPointer<QItemSelectionModel> oldSelection = m_selectionModel;
        const bool deleteOld = m_ownsSelectionModel;
        m_selectionModel = nullptr;
        m_ownsSelectionModel = false;
        installInternalSelectionModel();
        if (deleteOld && oldSelection)
          oldSelection->deleteLater();

        close();
        refreshSelectionState();
        emit modelChanged(nullptr);
        if (rootChanged)
          emit rootModelIndexChanged(QModelIndex());
        emit selectionModelChanged(m_selectionModel.data());
      }));
}

void MultiSelectComboBox::disconnectModelSignals() {
  for (const QMetaObject::Connection &connection :
       std::as_const(m_modelConnections)) {
    QObject::disconnect(connection);
  }
  m_modelConnections.clear();
}

void MultiSelectComboBox::handleSelectionChanged(
    const QItemSelection &selected, const QItemSelection &deselected) {
  emit selectionChanged(selected, deselected);
  refreshSelectionState();
  if (m_popup)
    m_popup->notifySourceSelectionChanged(selected, deselected);
}

void MultiSelectComboBox::refreshSelectionState(bool notifyAccessibility) {
  const int previousCount = m_selectedCount;
  const QStringList previousLabels = m_selectedLabels;
  m_selectedLabels.clear();
  const QModelIndexList indexes = selectedIndexes();
  m_selectedLabels.reserve(indexes.size());
  for (const QModelIndex &index : indexes)
    m_selectedLabels.append(index.data(Qt::DisplayRole).toString());
  m_selectedCount = indexes.size();

  const bool countChanged = previousCount != m_selectedCount;
  const bool valueChanged = previousLabels != m_selectedLabels;
  refreshPresentation();
  if (m_popup)
    m_popup->syncSelectionFromOwner();
  if (countChanged)
    emit selectedCountChanged(m_selectedCount);
  if (notifyAccessibility && valueChanged) {
    detail::notifyMultiSelectComboBoxSelectionChanged(this, countChanged);
  }
}

void MultiSelectComboBox::refreshPresentation() {
  if (m_trigger)
    m_trigger->update();
  updateGeometry();
}

void MultiSelectComboBox::refreshPopup() {
  if (m_popup && m_popup->isOpen())
    m_popup->prepareForOpen();
}

void MultiSelectComboBox::setRowsSelected(const QModelIndexList &indexes,
                                          bool selected) {
  if (!m_selectionModel)
    return;
  QModelIndexList selectable;
  selectable.reserve(indexes.size());
  for (const QModelIndex &index : indexes) {
    if (index.model() == m_model.data() && isSelectable(index))
      selectable.append(index);
  }
  if (selectable.isEmpty())
    return;
  m_selectionModel->select(
      selectionForIndexes(selectable),
      (selected ? QItemSelectionModel::Select : QItemSelectionModel::Deselect) |
          QItemSelectionModel::Rows);
}

QModelIndex MultiSelectComboBox::sourceIndexForRow(int row) const {
  if (!m_model || row < 0 || row >= m_model->rowCount(m_rootModelIndex)) {
    return QModelIndex();
  }
  return m_model->index(row, m_modelColumn, m_rootModelIndex);
}

bool MultiSelectComboBox::isSelectable(const QModelIndex &index) const {
  return index.model() == m_model.data() && indexIsSelectable(index);
}

QString MultiSelectComboBox::displayTextForWidth(int width) const {
  if (m_selectedCount <= 0)
    return m_placeholderText;
  const QString full = accessibleValueText();
  if (QFontMetrics(font()).horizontalAdvance(full) <= width)
    return full;
  return tr("%n selected", nullptr, m_selectedCount);
}

QString MultiSelectComboBox::accessibleValueText() const {
  return QLocale().createSeparatedList(m_selectedLabels);
}

void MultiSelectComboBox::togglePopupFromTrigger() {
  if (m_ignoreNextTriggerClick) {
    m_ignoreNextTriggerClick = false;
    setProperty(kKeyboardFocusOnOpenProperty, QVariant());
    return;
  }
  if (isOpen()) {
    setProperty(kKeyboardFocusOnOpenProperty, QVariant());
    close();
  } else {
    open();
  }
}

void MultiSelectComboBox::advanceFocus(bool next) { focusNextPrevChild(next); }

} // namespace fluent::basicinput

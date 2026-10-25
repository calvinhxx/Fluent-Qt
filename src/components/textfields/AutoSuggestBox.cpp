#include "components/foundation/private/TextInputFrame_p.h"
#include "compatibility/InputEventCompat.h"
#include "AutoSuggestBox.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QMoveEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QRegion>
#include <QResizeEvent>
#include <QStringListModel>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <QStyledItemDelegate>

#include <functional>
#include <utility>

#include "compatibility/TextPaintCompat.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FontIcon.h"
#include "components/status_info/ProgressRing.h"
#include "components/textfields/Label.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/foundation/overlay/FieldPopupGeometry_p.h"
#include "components/textfields/private/AutoSuggestBoxAccessibility_p.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::textfields {

namespace {
bool isEditableKey(QKeyEvent* event)
{
    if (!event)
        return false;
    const int key = event->key();
    if (key == Qt::Key_Backspace || key == Qt::Key_Delete)
        return true;
    if (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier))
        return false;
    return !event->text().isEmpty() && event->text().at(0).isPrint();
}

int normalizedPositiveSize(int size)
{
    return qMax(1, size);
}
} // namespace

class AutoSuggestItemDelegate : public QStyledItemDelegate {
public:
    explicit AutoSuggestItemDelegate(const FluentElement* themeHost, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_themeHost(themeHost)
    {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(0, m_itemHeight); }

    void setFontRole(Typography::FontRole role)
    {
        if (m_fontRole == role)
            return;
        m_fontRole = role;
    }

    void setItemHeight(int height) { m_itemHeight = normalizedPositiveSize(height); }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!index.isValid())
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const auto& colors = m_themeHost->themeColorsRef();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool enabled = option.state & QStyle::State_Enabled;

        const QRectF bgRect = QRectF(option.rect).adjusted(2, 3, -2, -3);
        QColor bgColor = Qt::transparent;
        QColor textColor = enabled ? colors.textPrimary : colors.textDisabled;
        if (enabled && selected)
            bgColor = colors.subtleTertiary;
        else if (enabled && hovered)
            bgColor = colors.subtleSecondary;

        if (bgColor.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(bgColor);
            painter->drawRoundedRect(bgRect, m_themeHost->themeRadius().control, m_themeHost->themeRadius().control);
        }

        painter->setFont(m_themeHost->themeFont(m_fontRole).toQFont());
        painter->setPen(textColor);

        const QRectF textSlot = QRectF(option.rect).adjusted(12, 0, -8, 0);
        const QString display = index.data(Qt::DisplayRole).toString();
        const QFontMetricsF metrics(painter->font());
        const QString elidedText = metrics.elidedText(display, Qt::ElideRight, qRound(textSlot.width()));
        const QRectF textRect = fluent::painting::verticallyCenteredTextInkRect(textSlot, metrics, elidedText);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedText);

        painter->restore();
    }

private:
    const FluentElement* m_themeHost = nullptr;
    Typography::FontRole m_fontRole = Typography::FontRole::Body;
    int m_itemHeight = ::Spacing::ControlHeight::Large;
};

class SuggestionListPopup : public fluent::dialogs_flyouts::Flyout {
public:
    using SuggestionClickedHandler = std::function<void(int)>;

    explicit SuggestionListPopup(AutoSuggestBox* owner) : Flyout(owner), m_owner(owner)
    {
        setObjectName("AutoSuggestBoxSuggestionPopup");
        setAnimationEnabled(false);
        setPlacement(fluent::dialogs_flyouts::Flyout::Auto);
        setAnchorOffset(::Spacing::Gap::Normal);
        setModal(false);
        setDim(false);
        setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));
        setFocusOnOpenEnabled(false);
        m_listView = new fluent::collections::ListView(this);
        m_listView->setObjectName("AutoSuggestBoxSuggestionList");
        m_listView->setAccessibleName(AutoSuggestBox::tr("Suggestions"));
        m_listView->setSpacing(0);
        m_listView->setModel(owner->effectiveSuggestionModel());
        m_listView->setUniformItemSizes(true);
        m_itemDelegate = new AutoSuggestItemDelegate(this, m_listView);
        m_listView->setItemDelegate(m_itemDelegate);
        m_listView->setBorderVisible(false);
        m_listView->setBackgroundVisible(false);
        m_listView->setProperty("fluentPreserveParentSurface", true);
        m_listView->viewport()->setProperty("fluentPreserveParentSurface", true);
        m_listView->setScrollChainingEnabled(false);
        m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_listView->setSelectionMode(fluent::collections::ListView::SelectionMode::Single);
        m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_listView->setMouseTracking(true);
        m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_listView->setStyleSheet(
            "QListView { background: transparent; border: none; outline: none; }"
            "QListView::item { background: transparent; }");
        m_listView->viewport()->setAutoFillBackground(false);
        connect(m_listView, &fluent::collections::ListView::itemClicked, this, [this](int row) {
            if (m_owner->suggestionIndex(row).isValid() && m_suggestionClickedHandler)
                m_suggestionClickedHandler(row);
        });
    }

    void setSuggestionClickedHandler(SuggestionClickedHandler handler)
    {
        m_suggestionClickedHandler = std::move(handler);
    }

    void syncModel()
    {
        if (m_listView->model() != m_owner->effectiveSuggestionModel())
            m_listView->setModel(m_owner->effectiveSuggestionModel());
    }

    void resetPosition()
    {
        m_listView->clearSelection();
        m_listView->setCurrentIndex(QModelIndex());
        m_listView->scrollToTop();
    }

    void setSuggestionMetrics(Typography::FontRole fontRole, int itemHeight)
    {
        m_itemHeight = normalizedPositiveSize(itemHeight);
        m_itemDelegate->setFontRole(fontRole);
        m_itemDelegate->setItemHeight(m_itemHeight);
        m_listView->doItemsLayout();
        if (isOpen())
            showForOwner();
    }

    int currentRow() const
    {
        const QModelIndex current = m_listView->currentIndex();
        return current.isValid() ? current.row() : -1;
    }

    void setCurrentRow(int row)
    {
        const QModelIndex index = m_owner->suggestionIndex(row);
        if (!index.isValid()) {
            m_listView->clearSelection();
            m_listView->setCurrentIndex(QModelIndex());
            return;
        }
        m_listView->setCurrentIndex(index);
        if (m_listView->selectionModel())
            m_listView->selectionModel()->select(index,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_listView->scrollTo(index, QAbstractItemView::EnsureVisible);
    }

    bool focusStatusAction()
    {
        if (!m_statusAction || !m_statusAction->isVisible() || !m_statusAction->isEnabled())
            return false;
        m_statusAction->setFocus(Qt::TabFocusReason);
        return true;
    }

    void showForOwner()
    {
        if (!m_owner || !m_owner->canShowSuggestions()) {
            clearMask();
            close();
            return;
        }
        syncModel();
        const int shadow = ::Spacing::Standard;
        const int count = m_owner->suggestionCount();
        int listHeight = qMin(count, 6) * m_itemHeight;
        const auto* top = ::fluent::overlay::presentedTopLevel(m_owner);
        const QRect surface = ::fluent::overlay::overlaySurfaceRect(top);
        const QRect anchor = ::fluent::overlay::presentedRectInTopLevel(m_owner);
        const auto space = ::fluent::overlay::fieldPopupSpace(anchor, surface, anchorOffset());
        const int contentWidth = space.boundedWidth(qMax(m_owner->width(), 120));
        const int padY = ::Spacing::XSmall;
        int statusHeight = 0;
        const bool showStatus = m_owner->m_asyncEnabled
            && (m_owner->m_loadState != AutoSuggestBox::LoadState::Ready || count == 0 || m_owner->m_hasMore);
        if (showStatus) {
            ensureStatus();
            updateStatus(count);
            auto* statusLayout = m_statusRow->layout();
            statusLayout->invalidate();
            const int contentHeight = statusLayout->hasHeightForWidth()
                ? statusLayout->totalHeightForWidth(contentWidth)
                : statusLayout->sizeHint().height();
            statusHeight = qMax(::Spacing::ControlHeight::Standard, contentHeight);
            m_statusRow->show();
        } else if (m_statusRow) {
            m_statusRow->hide();
            m_progress->setIsActive(false);
        }
        const int desiredHeight = listHeight + statusHeight + padY * 2;
        setPlacement(space.opensAbove(desiredHeight) ? Flyout::Top : Flyout::Bottom);
        const int available = qMax(0, space.capacity(desiredHeight) - padY * 2);
        statusHeight = qMin(statusHeight, available);
        listHeight = qMin(listHeight, qMax(0, available - statusHeight));
        if (listHeight >= m_itemHeight)
            listHeight = listHeight / m_itemHeight * m_itemHeight;
        if (showStatus)
            m_statusRow->setGeometry(shadow, shadow + padY + listHeight, contentWidth, statusHeight);
        m_listView->setVisible(count > 0 && listHeight > 0);
        m_listView->setGeometry(shadow, shadow + padY, contentWidth, listHeight);
        // Uniform rows keep layout independent of per-item widget allocation.
        m_listView->doItemsLayout();
        m_listView->viewport()->update();
        m_listView->refreshFluentScrollChrome();
        resize(contentWidth + shadow * 2, listHeight + statusHeight + padY * 2 + shadow * 2);
        setAnchor(m_owner);
        if (isOpen()) {
            move(computePosition());
            raise();
        } else {
            showAt(m_owner);
        }
        updateOwnerPassthroughMask();
    }

    void onThemeUpdated() override
    {
        Flyout::onThemeUpdated();
        if (m_errorIcon)
            m_errorIcon->setColor(themeColorsRef().systemCritical);
        if (m_listView)
            m_listView->viewport()->update();
    }

protected:
    QPoint computePosition() const override
    {
        const auto* top = ::fluent::overlay::presentedTopLevel(m_owner);
        if (!top)
            return Flyout::computePosition();
        const QRect anchor = ::fluent::overlay::presentedRectInTopLevel(m_owner);
        const QSize card = ::fluent::overlay::visibleCardSize(size());
        return ::fluent::overlay::outerTopLeftForVisibleCard(::fluent::overlay::fieldPopupCardTopLeft(anchor, card,
            ::fluent::overlay::overlaySurfaceRect(top), m_owner->layoutDirection(), placement() == Flyout::Top,
            anchorOffset()));
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (isOpen() && !m_geometryUpdatePending
            && ::fluent::overlay::fieldPopupAnchorGeometryMayChange(watched, event, m_owner, this)) {
            m_geometryUpdatePending = true;
            QTimer::singleShot(0, this, [this] {
                m_geometryUpdatePending = false;
                if (isOpen())
                    showForOwner();
            });
        }
        if (watched == m_statusAction && event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Backtab || key->key() == Qt::Key_Up
                || (key->key() == Qt::Key_Tab && key->modifiers().testFlag(Qt::ShiftModifier))) {
                m_owner->setFocus(Qt::BacktabFocusReason);
                return true;
            }
            if (key->key() == Qt::Key_Escape) {
                m_owner->cancelSuggestions();
                return true;
            }
        }
        return Flyout::eventFilter(watched, event);
    }

    void hideEvent(QHideEvent* event) override
    {
        if (m_progress)
            m_progress->setIsActive(false);
        Flyout::hideEvent(event);
    }

private:
    bool m_geometryUpdatePending = false;

    void ensureStatus()
    {
        if (m_statusRow)
            return;
        m_statusRow = new QWidget(this);
        m_statusRow->setObjectName(QStringLiteral("AutoSuggestBoxStatus"));
        auto* row = new QHBoxLayout(m_statusRow);
        row->setContentsMargins(::Spacing::Medium, ::Spacing::Medium / 2, ::Spacing::Medium, ::Spacing::Medium / 2);
        row->setSpacing(::Spacing::Small);
        m_progress = new fluent::status_info::ProgressRing(m_statusRow);
        m_progress->setObjectName(QStringLiteral("AutoSuggestBoxLoading"));
        m_progress->setFixedSize(16, 16);
        m_progress->setStrokeWidth(2);
        m_progress->setBackgroundVisible(false);
        m_errorIcon = new fluent::FontIcon(Typography::Icons::ErrorIcon, m_statusRow);
        m_errorIcon->setFixedSize(16, 16);
        m_errorIcon->setIconSize(16);
        m_errorIcon->setColor(themeColorsRef().systemCritical);
        auto* copy = new QVBoxLayout();
        copy->setContentsMargins(0, 0, 0, 0);
        copy->setSpacing(0);
        m_statusTitle = new Label(m_statusRow);
        m_statusTitle->setObjectName(QStringLiteral("AutoSuggestBoxStatusTitle"));
        m_statusTitle->setTextFormat(Qt::PlainText);
        m_statusTitle->setTextElideMode(Qt::ElideRight);
        m_statusTitle->setMinimumWidth(0);
        m_statusDetails = new Label(m_statusRow);
        m_statusDetails->setObjectName(QStringLiteral("AutoSuggestBoxStatusDetails"));
        m_statusDetails->setTextFormat(Qt::PlainText);
        m_statusDetails->setFluentTypography(Typography::FontRole::Caption);
        m_statusDetails->setMinimumHeight(themeFont(Typography::FontRole::Caption).lineHeight);
        m_statusDetails->setTextColorRole(Label::TextColorRole::Secondary);
        m_statusDetails->setWordWrap(true);
        m_statusDetails->setMinimumWidth(0);
        copy->addWidget(m_statusTitle);
        copy->addWidget(m_statusDetails);
        m_statusAction = new fluent::basicinput::Button(m_statusRow);
        m_statusAction->setObjectName(QStringLiteral("AutoSuggestBoxStatusAction"));
        m_statusAction->setFluentSize(fluent::basicinput::Button::Small);
        m_statusAction->setFontRole(Typography::FontRole::Caption);
        m_statusAction->setMinimumHeight(::Spacing::ControlHeight::Small);
        m_statusAction->setFocusPolicy(Qt::StrongFocus);
        m_statusAction->installEventFilter(this);
        row->addWidget(m_progress);
        row->addWidget(m_errorIcon);
        row->addLayout(copy, 1);
        row->addWidget(m_statusAction);
        connect(m_statusAction, &QAbstractButton::clicked, m_owner, [owner = m_owner]() {
            owner->setFocus(Qt::OtherFocusReason);
            if (owner->loadState() == AutoSuggestBox::LoadState::Failed)
                owner->retrySuggestions();
            else
                owner->loadMoreSuggestions();
        });
    }

    void updateStatus(int count)
    {
        using State = AutoSuggestBox::LoadState;
        const State state = m_owner->m_loadState;
        const bool loading = state == State::Loading;
        const bool failed = state == State::Failed;
        const bool more = state == State::Ready && m_owner->m_hasMore;
        m_progress->setVisible(loading);
        m_progress->setIsActive(loading);
        m_errorIcon->setVisible(failed);
        m_statusAction->setVisible(failed || more);
        m_statusAction->setText(failed ? m_owner->retryText() : m_owner->loadMoreText());
        QString title;
        QString details;
        if (loading)
            title = count > 0 ? m_owner->loadingMoreText() : m_owner->m_loadingText;
        else if (failed) {
            title = m_owner->m_errorText;
            details = m_owner->m_errorDetails;
        } else if (more) {
            title = m_owner->moreResultsText();
        } else {
            title = m_owner->m_emptyText;
            details = m_owner->m_emptyDescription;
        }
        const auto titleRole = count > 0 && !failed ? Typography::FontRole::Caption : Typography::FontRole::Body;
        m_statusTitle->setFluentTypography(titleRole);
        m_statusTitle->setMinimumHeight(themeFont(titleRole).lineHeight);
        m_statusTitle->setTextColorRole(
            loading || more ? Label::TextColorRole::Secondary : Label::TextColorRole::Primary);
        m_statusTitle->setText(title);
        m_statusDetails->setText(details);
        m_statusDetails->setVisible(!details.isEmpty());
    }

    void updateOwnerPassthroughMask()
    {
        if (!m_owner) {
            clearMask();
            return;
        }
        QRegion hitRegion(rect());
        const QRect presented = ::fluent::overlay::presentedRectInTopLevel(m_owner);
        const QRect ownerRect(mapFrom(::fluent::overlay::presentedTopLevel(m_owner), presented.topLeft()),
            presented.size());
        hitRegion -= QRegion(ownerRect.adjusted(-1, -1, 1, 1));
        setMask(hitRegion);
    }

    AutoSuggestBox* m_owner = nullptr;
    fluent::collections::ListView* m_listView = nullptr;
    AutoSuggestItemDelegate* m_itemDelegate = nullptr;
    QWidget* m_statusRow = nullptr;
    fluent::status_info::ProgressRing* m_progress = nullptr;
    fluent::FontIcon* m_errorIcon = nullptr;
    Label* m_statusTitle = nullptr;
    Label* m_statusDetails = nullptr;
    fluent::basicinput::Button* m_statusAction = nullptr;
    SuggestionClickedHandler m_suggestionClickedHandler;
    int m_itemHeight = ::Spacing::ControlHeight::Large;
};

AutoSuggestBox::AutoSuggestBox(QWidget* parent) : LineEdit(parent)
{
    detail::ensureAutoSuggestBoxAccessibilityFactory();
    setAttribute(Qt::WA_Hover);
    LineEdit::setClearButtonEnabled(false);
    m_inputHeight = kDefaultInputHeight;
    setFixedHeight(totalPreferredHeight());

    m_stringModel = new QStringListModel(this);
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_loadingText = tr("Loading suggestions…");
    m_loadingMoreText = tr("Loading more…");
    m_moreResultsText = tr("More results available");
    m_retryText = tr("Retry");
    m_loadMoreText = tr("Load more");
    m_emptyText = tr("No suggestions found");
    m_emptyDescription = tr("Try a different search.");
    connect(m_debounceTimer, &QTimer::timeout, this, [this] { startSuggestionRequest(false); });
    initializeButtons();
    observeSuggestionModel();

    connect(this, &QLineEdit::textEdited, this, [this](const QString& editedText) {
        m_nextChangeReason = TextChangeReason::UserInput;
        m_userTypedText = editedText;
    });
    connect(this, &QLineEdit::textChanged, this, &AutoSuggestBox::handleTextChanged);

    updateHeaderTextMargins();
    updateTextMargins();
    updateButtonState();
}

AutoSuggestBox::~AutoSuggestBox()
{
    for (const auto& connection : std::as_const(m_modelConnections))
        disconnect(connection);
    m_debounceTimer->stop();
    delete m_suggestionPopup.data();
    m_suggestionPopup = nullptr;
}

bool AutoSuggestBox::isSuggestionListOpen() const
{
    return m_suggestionPopup && m_suggestionPopup->isOpen();
}

QAbstractItemModel* AutoSuggestBox::suggestionModel() const
{
    return m_externalModel.data();
}

QAbstractItemModel* AutoSuggestBox::effectiveSuggestionModel() const
{
    return m_externalModel ? m_externalModel.data() : m_stringModel;
}

int AutoSuggestBox::suggestionCount() const
{
    if (m_asyncEnabled && !m_resultsCurrent)
        return 0;
    auto* model = effectiveSuggestionModel();
    return model ? model->rowCount() : 0;
}

QModelIndex AutoSuggestBox::suggestionIndex(int row) const
{
    auto* model = effectiveSuggestionModel();
    if (!model || row < 0 || row >= suggestionCount())
        return {};
    const QModelIndex index = model->index(row, 0);
    const auto flags = model->flags(index);
    return flags.testFlag(Qt::ItemIsEnabled) && flags.testFlag(Qt::ItemIsSelectable) ? index : QModelIndex();
}

int AutoSuggestBox::nextEnabledSuggestion(int from, int direction) const
{
    for (int row = from + direction; row >= 0 && row < suggestionCount(); row += direction) {
        if (suggestionIndex(row).isValid())
            return row;
    }
    return -1;
}

void AutoSuggestBox::observeSuggestionModel()
{
    for (const auto& connection : std::as_const(m_modelConnections))
        disconnect(connection);
    m_modelConnections.clear();
    auto* model = effectiveSuggestionModel();
    if (!model)
        return;
    const auto refresh = [this] {
        if (m_modelRefreshPending)
            return;
        m_modelRefreshPending = true;
        QTimer::singleShot(0, this, [this] {
            m_modelRefreshPending = false;
            refreshSuggestionPresentation();
        });
    };
    m_modelConnections
        << connect(model, &QAbstractItemModel::modelReset, this, refresh)
        << connect(model, &QAbstractItemModel::layoutChanged, this, refresh)
        << connect(model, &QAbstractItemModel::rowsInserted, this, refresh)
        << connect(model, &QAbstractItemModel::rowsRemoved, this, refresh)
        << connect(model, &QAbstractItemModel::rowsMoved, this, refresh)
        << connect(model, &QAbstractItemModel::dataChanged, this, refresh);
    if (m_externalModel) {
        m_modelConnections << connect(model, &QObject::destroyed, this, [this] {
            QPointer<AutoSuggestBox> guard(this);
            m_externalModel = nullptr;
            cancelSuggestions();
            if (!guard)
                return;
            observeSuggestionModel();
            if (m_suggestionPopup)
                m_suggestionPopup->syncModel();
            refreshSuggestionPresentation();
            if (guard)
                emit suggestionModelChanged(nullptr);
        });
    }
}

void AutoSuggestBox::setSuggestionModel(QAbstractItemModel* model)
{
    if (model == m_externalModel || model == m_stringModel)
        return;
    QPointer<AutoSuggestBox> guard(this);
    cancelSuggestions();
    if (!guard)
        return;
    m_externalModel = model;
    const bool hadStrings = !m_suggestions.isEmpty();
    m_suggestions.clear();
    m_stringModel->setStringList({});
    if (!guard)
        return;
    m_resultsCurrent = !m_asyncEnabled;
    observeSuggestionModel();
    if (m_suggestionPopup)
        m_suggestionPopup->syncModel();
    refreshSuggestionPresentation();
    if (!guard)
        return;
    emit suggestionModelChanged(model);
    if (guard && hadStrings)
        emit suggestionsChanged();
}

void AutoSuggestBox::setAsyncSuggestionsEnabled(bool enabled)
{
    if (m_asyncEnabled == enabled)
        return;
    QPointer<AutoSuggestBox> guard(this);
    cancelSuggestions();
    if (!guard)
        return;
    m_asyncEnabled = enabled;
    m_asyncQuery = text();
    m_resultsCurrent = !enabled;
    setLoadState(LoadState::Idle);
    if (!guard)
        return;
    setHasMoreSuggestions(false);
    if (!guard)
        return;
    refreshSuggestionPresentation();
    if (guard)
        emit asyncSuggestionsEnabledChanged(enabled);
}

void AutoSuggestBox::setDebounceInterval(int milliseconds)
{
    const int normalized = qMax(0, milliseconds);
    if (normalized == m_debounceInterval)
        return;
    m_debounceInterval = normalized;
    if (m_debounceTimer->isActive())
        m_debounceTimer->start(normalized);
    emit debounceIntervalChanged(normalized);
}

void AutoSuggestBox::setLoadingText(const QString& value)
{
    if (m_loadingText == value)
        return;
    m_loadingText = value;
    QPointer<AutoSuggestBox> guard(this);
    refreshSuggestionPresentation();
    if (guard)
        emit loadingTextChanged();
}

void AutoSuggestBox::setEmptyText(const QString& value)
{
    if (m_emptyText == value)
        return;
    m_emptyText = value;
    QPointer<AutoSuggestBox> guard(this);
    refreshSuggestionPresentation();
    if (guard)
        emit emptyTextChanged();
}

void AutoSuggestBox::setEmptyDescription(const QString& value)
{
    if (m_emptyDescription == value)
        return;
    m_emptyDescription = value;
    QPointer<AutoSuggestBox> guard(this);
    refreshSuggestionPresentation();
    if (guard)
        emit emptyDescriptionChanged();
}

void AutoSuggestBox::setLoadingMoreText(const QString& value)
{
    if (m_loadingMoreText == value)
        return;
    m_loadingMoreText = value;
    QPointer<AutoSuggestBox> guard(this);
    refreshSuggestionPresentation();
    if (guard)
        emit loadingMoreTextChanged();
}

void AutoSuggestBox::setMoreResultsText(const QString& value)
{
    if (m_moreResultsText == value)
        return;
    m_moreResultsText = value;
    QPointer<AutoSuggestBox> guard(this);
    refreshSuggestionPresentation();
    if (guard)
        emit moreResultsTextChanged();
}

void AutoSuggestBox::setRetryText(const QString& value)
{
    if (m_retryText == value)
        return;
    m_retryText = value;
    QPointer<AutoSuggestBox> guard(this);
    refreshSuggestionPresentation();
    if (guard)
        emit retryTextChanged();
}

void AutoSuggestBox::setLoadMoreText(const QString& value)
{
    if (m_loadMoreText == value)
        return;
    m_loadMoreText = value;
    QPointer<AutoSuggestBox> guard(this);
    refreshSuggestionPresentation();
    if (guard)
        emit loadMoreTextChanged();
}

void AutoSuggestBox::setLoadState(LoadState state)
{
    if (m_loadState == state)
        return;
    m_loadState = state;
    detail::notifyAutoSuggestSuggestionsChanged(this);
    emit loadStateChanged(state);
}

void AutoSuggestBox::setHasMoreSuggestions(bool more)
{
    if (m_hasMore == more)
        return;
    m_hasMore = more;
    emit hasMoreSuggestionsChanged(more);
}

bool AutoSuggestBox::isSuggestionRequestCurrent(quint64 requestId) const
{
    return requestId != 0 && requestId == m_activeRequest && m_asyncEnabled && !m_dismissed
        && m_loadState == LoadState::Loading && isEnabled() && !isReadOnly();
}

void AutoSuggestBox::invalidateSuggestionRequest()
{
    m_debounceTimer->stop();
    const quint64 previous = m_activeRequest;
    m_activeRequest = 0;
    if (previous)
        emit suggestionRequestCanceled(previous);
}

void AutoSuggestBox::startSuggestionRequest(bool append)
{
    if (!m_asyncEnabled || !isEnabled() || isReadOnly() || m_asyncQuery.trimmed().isEmpty())
        return;
    QPointer<AutoSuggestBox> guard(this);
    invalidateSuggestionRequest();
    if (!guard)
        return;
    m_dismissed = false;
    m_appendRequest = append;
    m_resultsCurrent = append;
    m_errorText.clear();
    m_errorDetails.clear();
    if (!append) {
        if (m_suggestionPopup)
            m_suggestionPopup->resetPosition();
        setHasMoreSuggestions(false);
        if (!guard)
            return;
    }
    const quint64 requestId = ++m_requestSerial;
    m_activeRequest = requestId;
    setLoadState(LoadState::Loading);
    if (!guard || !isSuggestionRequestCurrent(requestId))
        return;
    refreshSuggestionPresentation();
    if (guard && isSuggestionRequestCurrent(requestId))
        emit suggestionsRequested(m_asyncQuery, requestId, append);
}

void AutoSuggestBox::requestSuggestions()
{
    m_asyncQuery = text();
    startSuggestionRequest(false);
}

void AutoSuggestBox::loadMoreSuggestions()
{
    if (m_loadState == LoadState::Ready && m_hasMore && m_resultsCurrent)
        startSuggestionRequest(true);
}

void AutoSuggestBox::retrySuggestions()
{
    if (m_loadState == LoadState::Failed)
        startSuggestionRequest(m_appendRequest);
}

void AutoSuggestBox::cancelSuggestions()
{
    QPointer<AutoSuggestBox> guard(this);
    m_dismissed = true;
    invalidateSuggestionRequest();
    if (!guard)
        return;
    if (m_loadState == LoadState::Loading)
        setLoadState(m_resultsCurrent ? LoadState::Ready : LoadState::Idle);
    if (guard && m_suggestionPopup && m_suggestionPopup->isOpen())
        m_suggestionPopup->close();
}

bool AutoSuggestBox::completeSuggestionRequest(quint64 requestId, bool hasMore)
{
    if (!isSuggestionRequestCurrent(requestId))
        return false;
    QPointer<AutoSuggestBox> guard(this);
    m_resultsCurrent = true;
    m_activeRequest = 0;
    setLoadState(LoadState::Ready);
    if (!guard)
        return true;
    // A state observer may already have started a newer request.
    if (m_requestSerial != requestId || m_loadState != LoadState::Ready || m_dismissed)
        return true;
    setHasMoreSuggestions(hasMore);
    if (guard && m_requestSerial == requestId && m_loadState == LoadState::Ready && !m_dismissed)
        refreshSuggestionPresentation();
    return true;
}

bool AutoSuggestBox::setSuggestionsForRequest(quint64 requestId, const QStringList& suggestions, bool hasMore)
{
    if (!isSuggestionRequestCurrent(requestId) || m_externalModel)
        return false;
    QPointer<AutoSuggestBox> guard(this);
    const bool changed = m_appendRequest ? !suggestions.isEmpty() : m_suggestions != suggestions;
    if (m_appendRequest) {
        const int first = m_stringModel->rowCount();
        m_suggestions.append(suggestions);
        if (!suggestions.isEmpty()) {
            m_stringModel->insertRows(first, suggestions.size());
            if (!guard || !isSuggestionRequestCurrent(requestId))
                return false;
            for (int i = 0; i < suggestions.size(); ++i) {
                m_stringModel->setData(m_stringModel->index(first + i), suggestions.at(i));
                if (!guard || !isSuggestionRequestCurrent(requestId))
                    return false;
            }
        }
    } else {
        m_suggestions = suggestions;
        m_stringModel->setStringList(suggestions);
        if (!guard || !isSuggestionRequestCurrent(requestId))
            return false;
    }
    if (changed)
        emit suggestionsChanged();
    return guard && completeSuggestionRequest(requestId, hasMore);
}

bool AutoSuggestBox::failSuggestionRequest(quint64 requestId, const QString& message, const QString& details)
{
    if (!isSuggestionRequestCurrent(requestId))
        return false;
    QPointer<AutoSuggestBox> guard(this);
    m_activeRequest = 0;
    m_errorText = message;
    m_errorDetails = details;
    setLoadState(LoadState::Failed);
    if (guard && m_requestSerial == requestId && m_loadState == LoadState::Failed && !m_dismissed)
        refreshSuggestionPresentation();
    return true;
}

bool AutoSuggestBox::canShowSuggestions() const
{
    if (!isEnabled() || isReadOnly() || m_composing || m_dismissed)
        return false;
    return suggestionCount() > 0 || (m_asyncEnabled && m_loadState != LoadState::Idle && !m_asyncQuery.isEmpty());
}

void AutoSuggestBox::refreshSuggestionPresentation()
{
    QPointer<AutoSuggestBox> guard(this);
    const int count = suggestionCount();
    if (count != m_reportedSuggestionCount) {
        m_reportedSuggestionCount = count;
        detail::notifyAutoSuggestSuggestionsChanged(this);
        emit suggestionCountChanged(count);
        if (!guard)
            return;
    }
    if (!canShowSuggestions()) {
        if (m_suggestionPopup && m_suggestionPopup->isOpen())
            m_suggestionPopup->close();
        return;
    }
    if (isVisible() && ((hasFocus() && !text().isEmpty()) || isSuggestionListOpen()))
        openSuggestionList();
}

void AutoSuggestBox::setHeader(const QString& header)
{
    if (m_header == header)
        return;
    m_header = header;
    setFixedHeight(totalPreferredHeight());
    updateHeaderTextMargins();
    updateButtonGeometry();
    updateGeometry();
    update();
    if (isSuggestionListOpen())
        m_suggestionPopup->showForOwner();
    detail::notifyAutoSuggestNameChanged(this);
    emit headerChanged();
}

void AutoSuggestBox::setQueryIconGlyph(const QString& glyph)
{
    if (m_queryIconGlyph == glyph)
        return;
    m_queryIconGlyph = glyph;
    if (m_queryButton) {
        m_queryButton->setIconGlyph(m_queryIconGlyph, Typography::IconSize::Standard,
            Typography::FontFamily::FluentIcons);
    }
    if (m_queryDecoration)
        m_queryDecoration->setGlyph(glyph);
    emit queryIconGlyphChanged();
}

void AutoSuggestBox::setQueryIconVisible(bool visible)
{
    if (m_queryIconVisible == visible)
        return;
    m_queryIconVisible = visible;
    updateButtonState();
    updateTextMargins();
    updateButtonGeometry();
    emit queryIconVisibleChanged();
}

void AutoSuggestBox::setQueryButtonPlacement(QueryButtonPlacement placement)
{
    if (m_queryButtonPlacement == placement)
        return;

    m_queryButtonPlacement = placement;
    updateButtonGeometry();
    updateTextMargins();
    update();
    emit queryButtonPlacementChanged();
}

void AutoSuggestBox::setInputHeight(int height)
{
    const int normalized = normalizedPositiveSize(height);
    if (m_inputHeight == normalized)
        return;

    m_inputHeight = normalized;
    setFixedHeight(totalPreferredHeight());
    updateHeaderTextMargins();
    updateTextMargins();
    updateButtonGeometry();
    updateGeometry();
    update();
    if (isSuggestionListOpen())
        m_suggestionPopup->showForOwner();
    emit inputHeightChanged();
}

void AutoSuggestBox::setQueryButtonSize(int size)
{
    const int normalized = normalizedPositiveSize(size);
    if (m_queryButtonSize == normalized)
        return;

    m_queryButtonSize = normalized;
    updateButtonGeometry();
    updateTextMargins();
    update();
    emit queryButtonSizeChanged();
}

void AutoSuggestBox::setClearButtonSize(int size)
{
    const int normalized = normalizedPositiveSize(size);
    if (m_clearButtonSize == normalized)
        return;

    m_clearButtonSize = normalized;
    updateButtonGeometry();
    updateTextMargins();
    update();
    emit clearButtonSizeChanged();
}

void AutoSuggestBox::setSuggestionFontRole(Typography::FontRole role)
{
    if (m_suggestionFontRole == role)
        return;

    m_suggestionFontRole = role;
    updateSuggestionMetrics();
    emit suggestionFontRoleChanged();
}

void AutoSuggestBox::setSuggestionItemHeight(int height)
{
    const int normalized = normalizedPositiveSize(height);
    if (m_suggestionItemHeight == normalized)
        return;

    m_suggestionItemHeight = normalized;
    updateSuggestionMetrics();
    emit suggestionItemHeightChanged();
}

QSize AutoSuggestBox::sizeHint() const
{
    QSize hint = LineEdit::sizeHint();
    hint.setHeight(totalPreferredHeight());
    hint.setWidth(qMax(hint.width(), 160));
    return hint;
}

QSize AutoSuggestBox::minimumSizeHint() const
{
    QSize hint = sizeHint();
    hint.setWidth(qMax(hint.width(), 120));
    return hint;
}

void AutoSuggestBox::onThemeUpdated()
{
    LineEdit::onThemeUpdated();
    if (m_queryButton)
        m_queryButton->onThemeUpdated();
    if (m_clearButton)
        m_clearButton->onThemeUpdated();
    if (m_suggestionPopup)
        m_suggestionPopup->onThemeUpdated();
    update();
}

void AutoSuggestBox::setSuggestions(const QStringList& suggestions)
{
    if (!m_externalModel && m_suggestions == suggestions
        && (!m_asyncEnabled || (m_resultsCurrent && !m_activeRequest && !m_debounceTimer->isActive() && !m_hasMore)))
        return;
    QPointer<AutoSuggestBox> guard(this);
    if (m_externalModel)
        setSuggestionModel(nullptr);
    if (!guard)
        return;
    invalidateSuggestionRequest();
    if (!guard)
        return;
    m_suggestions = suggestions;
    m_resultsCurrent = true;
    m_stringModel->setStringList(suggestions);
    if (!guard)
        return;
    if (m_suggestionPopup)
        m_suggestionPopup->resetPosition();
    if (m_asyncEnabled)
        setLoadState(LoadState::Ready);
    if (!guard)
        return;
    setHasMoreSuggestions(false);
    if (!guard)
        return;
    refreshSuggestionPresentation();
    if (guard)
        emit suggestionsChanged();
}

void AutoSuggestBox::clearSuggestions()
{
    setSuggestions(QStringList{});
}

void AutoSuggestBox::paintEvent(QPaintEvent* event)
{
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!m_header.isEmpty())
            paintHeader(painter);
        if (isFrameVisible())
            paintInputFrame(painter);
    }
    QLineEdit::paintEvent(event);
}

void AutoSuggestBox::resizeEvent(QResizeEvent* event)
{
    LineEdit::resizeEvent(event);
    updateButtonGeometry();
    if (isSuggestionListOpen())
        m_suggestionPopup->showForOwner();
}

void AutoSuggestBox::keyPressEvent(QKeyEvent* event)
{
    if (!event)
        return;

    if (m_composing || isReadOnly() || !isEnabled()) {
        LineEdit::keyPressEvent(event);
        return;
    }

    if (isEnabled() && (suggestionCount() > 0 || m_asyncEnabled)
        && (event->key() == Qt::Key_F4
            || (event->key() == Qt::Key_Down && event->modifiers().testFlag(Qt::AltModifier)))) {
        m_dismissed = false;
        if (m_asyncEnabled && m_loadState == LoadState::Idle)
            requestSuggestions();
        else
            openSuggestionList();
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Down:
        if (suggestionCount() > 0) {
            m_dismissed = false;
            if (!isSuggestionListOpen())
                openSuggestionList();
            const int nextRow = nextEnabledSuggestion(m_suggestionPopup->currentRow(), 1);
            if (nextRow >= 0)
                previewSuggestion(nextRow);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Up:
        if (isSuggestionListOpen()) {
            const int current = m_suggestionPopup->currentRow();
            const int previous = nextEnabledSuggestion(current, -1);
            if (previous >= 0) {
                previewSuggestion(previous);
            } else {
                setPopupCurrentRow(-1);
                m_previewing = true;
                QPointer<AutoSuggestBox> guard(this);
                setTextWithReason(m_userTypedText, TextChangeReason::ProgrammaticChange);
                if (guard)
                    m_previewing = false;
            }
            event->accept();
            return;
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (isSuggestionListOpen() && m_suggestionPopup->currentRow() >= 0) {
            chooseSuggestion(m_suggestionPopup->currentRow());
        } else {
            QPointer<AutoSuggestBox> guard(this);
            closeSuggestionList();
            if (guard)
                emit guard->querySubmitted(guard->text(), QVariant{});
        }
        event->accept();
        return;
    case Qt::Key_Escape:
        if (isSuggestionListOpen() || (m_asyncEnabled && m_loadState == LoadState::Loading)) {
            closeSuggestionList();
            event->accept();
            return;
        }
        break;
    default:
        break;
    }

    if (isEditableKey(event) && !isReadOnly())
        m_nextChangeReason = TextChangeReason::UserInput;
    LineEdit::keyPressEvent(event);
}

void AutoSuggestBox::inputMethodEvent(QInputMethodEvent* event)
{
    if (!event)
        return;
    QPointer<AutoSuggestBox> guard(this);
    const bool beginning = !m_composing && !event->preeditString().isEmpty();
    m_composing = !event->preeditString().isEmpty();
    if (beginning)
        cancelSuggestions();
    if (!guard)
        return;
    // Qt owns preedit, replacement ranges and the platform candidate window.
    // zh_CN: 预编辑、替换范围和系统候选窗由 Qt 管理，只为已提交文字启动搜索。
    m_nextChangeReason = TextChangeReason::UserInput;
    LineEdit::inputMethodEvent(event);
    if (guard)
        m_nextChangeReason = TextChangeReason::ProgrammaticChange;
}

void AutoSuggestBox::setClearButtonEnabled(bool enabled)
{
    if (m_inlineClearEnabled == enabled)
        return;
    m_inlineClearEnabled = enabled;
    updateButtonState();
    updateTextMargins();
    emit clearButtonEnabledChanged();
}

void AutoSuggestBox::focusInEvent(QFocusEvent* event)
{
    m_focused = true;
    LineEdit::focusInEvent(event);
    update();
}

void AutoSuggestBox::focusOutEvent(QFocusEvent* event)
{
    m_focused = false;
    LineEdit::focusOutEvent(event);
    update();
    QTimer::singleShot(0, this, [this] {
        QWidget* focus = QApplication::focusWidget();
        if (!hasFocus() && !(m_suggestionPopup && focus && m_suggestionPopup->isAncestorOf(focus)))
            cancelSuggestions();
    });
}

bool AutoSuggestBox::focusNextPrevChild(bool next)
{
    if (next && isSuggestionListOpen() && m_suggestionPopup->focusStatusAction())
        return true;
    return LineEdit::focusNextPrevChild(next);
}

void AutoSuggestBox::hideEvent(QHideEvent* event)
{
    QPointer<AutoSuggestBox> guard(this);
    cancelSuggestions();
    if (guard)
        LineEdit::hideEvent(event);
}

void AutoSuggestBox::moveEvent(QMoveEvent* event)
{
    LineEdit::moveEvent(event);
    if (isSuggestionListOpen())
        m_suggestionPopup->showForOwner();
}

void AutoSuggestBox::enterEvent(FluentEnterEvent* event)
{
    m_hovered = true;
    LineEdit::enterEvent(event);
    update();
}

void AutoSuggestBox::leaveEvent(QEvent* event)
{
    m_hovered = false;
    LineEdit::leaveEvent(event);
    update();
}

void AutoSuggestBox::changeEvent(QEvent* event)
{
    LineEdit::changeEvent(event);
    if (!event)
        return;

    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::ReadOnlyChange) {
        QPointer<AutoSuggestBox> guard(this);
        if (!isEnabled() || isReadOnly())
            cancelSuggestions();
        if (!guard)
            return;
        updateButtonState();
        updateTextMargins();
        update();
    } else if (event->type() == QEvent::FontChange) {
        updateTextMargins();
    }
}

QRect AutoSuggestBox::inputRect() const
{
    return QRect(0, inputTop(), width(), m_inputHeight);
}

int AutoSuggestBox::inputTop() const
{
    return m_header.isEmpty() ? 0 : kHeaderHeight + kHeaderGap;
}

int AutoSuggestBox::totalPreferredHeight() const
{
    return inputTop() + m_inputHeight;
}

int AutoSuggestBox::inputTextVerticalPadding() const
{
    const QFont inputFont = themeFont(fontRole()).toQFont();
    const int textHeight = QFontMetrics(inputFont).height();
    const int centeredPadding = qMax(0, (m_inputHeight - textHeight) / 2);
    return qMin(::Spacing::Padding::TextFieldVertical, centeredPadding);
}

void AutoSuggestBox::initializeButtons()
{
    m_queryButton = new ::fluent::basicinput::Button(this);
    m_queryButton->setObjectName("AutoSuggestBoxQueryButton");
    m_queryButton->setFluentStyle(::fluent::basicinput::Button::Subtle);
    m_queryButton->setFluentLayout(::fluent::basicinput::Button::IconOnly);
    m_queryButton->setFluentSize(::fluent::basicinput::Button::Small);
    m_queryButton->setFocusPolicy(Qt::NoFocus);
    m_queryButton->setAccessibleName(tr("Submit query"));
    m_queryButton->setFixedSize(m_queryButtonSize, m_queryButtonSize);
    m_queryButton->setIconGlyph(m_queryIconGlyph, Typography::IconSize::Standard, Typography::FontFamily::FluentIcons);

    connect(m_queryButton, &::fluent::basicinput::Button::clicked, this, [this]() {
        QPointer<AutoSuggestBox> guard(this);
        closeSuggestionList();
        if (guard)
            emit guard->querySubmitted(guard->text(), QVariant{});
    });

    m_clearButton = new ::fluent::basicinput::Button(this);
    m_clearButton->setObjectName("AutoSuggestBoxClearButton");
    m_clearButton->setFluentStyle(::fluent::basicinput::Button::Subtle);
    m_clearButton->setFluentLayout(::fluent::basicinput::Button::IconOnly);
    m_clearButton->setFluentSize(::fluent::basicinput::Button::Small);
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    m_clearButton->setAccessibleName(tr("Clear text"));
    m_clearButton->setFixedSize(m_clearButtonSize, m_clearButtonSize);
    m_clearButton->setIconGlyph(Typography::Icons::Cancel, Typography::IconSize::Standard,
        Typography::FontFamily::FluentIcons);

    connect(m_clearButton, &::fluent::basicinput::Button::clicked, this, [this]() {
        QPointer<AutoSuggestBox> guard(this);
        setTextWithReason(QString(), TextChangeReason::UserInput);
        if (!guard)
            return;
        guard->closeSuggestionList();
        if (guard)
            guard->setFocus();
    });

    updateButtonGeometry();
}

void AutoSuggestBox::setQueryIconDecorative(bool decorative)
{
    if (m_queryIconDecorative == decorative)
        return;
    m_queryIconDecorative = decorative;
    if (decorative && !m_queryDecoration) {
        m_queryDecoration = new FontIcon(m_queryIconGlyph, this);
        m_queryDecoration->setObjectName(QStringLiteral("AutoSuggestBoxQueryDecoration"));
        m_queryDecoration->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_queryDecoration->setFocusPolicy(Qt::NoFocus);
        m_queryDecoration->setIconSize(Typography::IconSize::Standard);
    }
    updateButtonState();
}

void AutoSuggestBox::updateButtonGeometry()
{
    const QRect input = inputRect();

    if (m_queryButton) {
        m_queryButton->setFixedSize(m_queryButtonSize, m_queryButtonSize);
        const int y = input.top() + (input.height() - m_queryButtonSize) / 2;
        const int x = m_queryButtonPlacement == QueryButtonPlacement::Left
            ? input.left() + kButtonEdgeMargin
            : input.right() + 1 - kButtonEdgeMargin - m_queryButtonSize;
        m_queryButton->setGeometry(x, y, m_queryButtonSize, m_queryButtonSize);
        if (m_queryDecoration)
            m_queryDecoration->setGeometry(m_queryButton->geometry());
    }

    if (m_clearButton) {
        const bool queryVisible = m_queryIconVisible;
        m_clearButton->setFixedSize(m_clearButtonSize, m_clearButtonSize);
        const int y = input.top() + (input.height() - m_clearButtonSize) / 2;
        const int right = queryVisible && m_queryButtonPlacement == QueryButtonPlacement::Right && m_queryButton
            ? m_queryButton->geometry().left() - 1
            : input.right() + 1 - kButtonEdgeMargin;
        const int x = right - m_clearButtonSize;
        m_clearButton->setGeometry(x, y, m_clearButtonSize, m_clearButtonSize);
    }
}

void AutoSuggestBox::updateButtonState()
{
    if (m_queryButton) {
        m_queryButton->setVisible(m_queryIconVisible && !m_queryIconDecorative);
        if (m_queryDecoration) {
            m_queryDecoration->setGlyph(m_queryIconGlyph);
            m_queryDecoration->setVisible(m_queryIconVisible && m_queryIconDecorative);
        }
        m_queryButton->setEnabled(isEnabled());
    }
    if (m_clearButton) {
        const bool visible = m_inlineClearEnabled && !text().isEmpty() && isEnabled() && !isReadOnly();
        m_clearButton->setVisible(visible);
        m_clearButton->setEnabled(isEnabled() && !isReadOnly());
    }
    updateButtonGeometry();
}

void AutoSuggestBox::updateTextMargins()
{
    int leftMargin = ::Spacing::Padding::TextFieldHorizontal;
    int rightMargin = ::Spacing::Padding::TextFieldHorizontal;
    const bool clearVisible = m_inlineClearEnabled && !text().isEmpty() && isEnabled() && !isReadOnly();

    if (m_queryIconVisible) {
        const int queryReserve = kButtonEdgeMargin + m_queryButtonSize + kTextButtonGap;
        if (m_queryButtonPlacement == QueryButtonPlacement::Left) {
            leftMargin = qMax(leftMargin, queryReserve);
        } else {
            rightMargin += queryReserve;
        }
    }

    if (clearVisible) {
        rightMargin += m_clearButtonSize + kTextButtonGap;
        if (!m_queryIconVisible || m_queryButtonPlacement == QueryButtonPlacement::Left)
            rightMargin += kButtonEdgeMargin;
    }

    QMargins margins = contentMargins();
    const int verticalPadding = inputTextVerticalPadding();
    margins.setLeft(leftMargin);
    margins.setRight(rightMargin);
    margins.setTop(verticalPadding);
    margins.setBottom(verticalPadding);
    setContentMargins(margins);
}

void AutoSuggestBox::updateHeaderTextMargins()
{
    setTextMargins(0, inputTop(), 0, 0);
}

void AutoSuggestBox::updateSuggestionMetrics()
{
    if (m_suggestionPopup) {
        m_suggestionPopup->setSuggestionMetrics(m_suggestionFontRole, m_suggestionItemHeight);
    }
}

void AutoSuggestBox::ensureSuggestionPopup()
{
    if (m_suggestionPopup)
        return;

    auto* popup = new SuggestionListPopup(this);
    m_suggestionPopup = popup;
    updateSuggestionMetrics();
    popup->setSuggestionClickedHandler([this](int row) { chooseSuggestion(row); });
    connect(popup, &fluent::dialogs_flyouts::Popup::opened, this, [this]() {
        detail::notifyAutoSuggestPopupChanged(this);
        emit suggestionListOpenChanged(true);
    });
    connect(popup, &fluent::dialogs_flyouts::Popup::closed, this, [this]() {
        QPointer<AutoSuggestBox> guard(this);
        if (m_asyncEnabled)
            cancelSuggestions();
        else if (suggestionCount() > 0)
            m_dismissed = true;
        if (!guard)
            return;
        detail::notifyAutoSuggestPopupChanged(this);
        emit suggestionListOpenChanged(false);
    });
}

void AutoSuggestBox::handleTextChanged(const QString& changedText)
{
    const TextChangeReason reason = m_nextChangeReason;
    m_nextChangeReason = TextChangeReason::ProgrammaticChange;
    if (reason == TextChangeReason::UserInput && !m_composing)
        m_dismissed = false;
    if (reason == TextChangeReason::UserInput)
        m_userTypedText = changedText;
    updateButtonState();
    updateTextMargins();
    QPointer<AutoSuggestBox> guard(this);
    // Invalidate before notifying application listeners; an old response must
    // not become current while textChangedWithReason is being delivered.
    // zh_CN: 先使请求失效，再通知应用，避免文字变化通知期间旧响应被接收。
    if (m_asyncEnabled && !m_previewing && reason != TextChangeReason::SuggestionChosen) {
        m_asyncQuery = changedText;
        m_userTypedText = changedText;
        m_resultsCurrent = false;
        m_dismissed = false;
        invalidateSuggestionRequest();
        if (!guard)
            return;
        setHasMoreSuggestions(false);
        if (!guard)
            return;
        const bool requestable = !m_composing && !changedText.trimmed().isEmpty()
            && reason == TextChangeReason::UserInput;
        setLoadState(requestable ? LoadState::Loading : LoadState::Idle);
        if (!guard)
            return;
        // A state observer may cancel or start the request synchronously.
        // Do not re-arm debounce after either decision.
        if (requestable && !m_dismissed && m_loadState == LoadState::Loading && !m_activeRequest
            && m_asyncQuery == changedText)
            m_debounceTimer->start(m_debounceInterval);
        refreshSuggestionPresentation();
        if (!guard)
            return;
    }
    emit textChangedWithReason(changedText, reason);
    if (!guard)
        return;
    if (!m_asyncEnabled && reason == TextChangeReason::UserInput) {
        if (changedText.isEmpty() || suggestionCount() == 0) {
            if (m_suggestionPopup && m_suggestionPopup->isOpen())
                m_suggestionPopup->close();
        } else if (hasFocus())
            openSuggestionList();
    }
}

void AutoSuggestBox::openSuggestionList()
{
    if (!canShowSuggestions())
        return;
    ensureSuggestionPopup();
    if (m_suggestionPopup)
        m_suggestionPopup->showForOwner();
}

QWidget* AutoSuggestBox::suggestionListWidget() const
{
    // Flyout reparents to its overlay host when opened; retain the owner-specific relation.
    return m_suggestionPopup
        ? m_suggestionPopup->findChild<QWidget*>(QStringLiteral("AutoSuggestBoxSuggestionList"))
        : nullptr;
}

void AutoSuggestBox::closeSuggestionList()
{
    m_dismissed = true;
    if (m_asyncEnabled) {
        cancelSuggestions();
        return;
    }
    if (m_suggestionPopup && m_suggestionPopup->isOpen())
        m_suggestionPopup->close();
}

void AutoSuggestBox::previewSuggestion(int row)
{
    const QModelIndex index = suggestionIndex(row);
    if (!index.isValid())
        return;
    setPopupCurrentRow(row);
    const QString value = index.data(Qt::DisplayRole).toString();
    const QVariant payload = index.data(Qt::UserRole);
    const QVariant item = payload.isValid() ? payload : QVariant(value);
    QPointer<AutoSuggestBox> guard(this);
    emit suggestionChosen(item);
    if (!guard)
        return;
    m_previewing = true;
    setTextWithReason(value, TextChangeReason::ProgrammaticChange);
    if (guard)
        m_previewing = false;
}

void AutoSuggestBox::chooseSuggestion(int row)
{
    const QModelIndex index = suggestionIndex(row);
    if (!index.isValid())
        return;
    setPopupCurrentRow(row);
    const QString value = index.data(Qt::DisplayRole).toString();
    const QVariant payload = index.data(Qt::UserRole);
    const QVariant item = payload.isValid() ? payload : QVariant(value);
    QPointer<AutoSuggestBox> guard(this);
    setTextWithReason(value, TextChangeReason::SuggestionChosen, true);
    if (!guard)
        return;
    emit suggestionChosen(item);
    if (!guard)
        return;
    guard->closeSuggestionList();
    if (!guard)
        return;
    emit querySubmitted(value, item);
}

void AutoSuggestBox::setPopupCurrentRow(int row)
{
    if (!m_suggestionPopup)
        return;
    const int before = m_suggestionPopup->currentRow();
    m_suggestionPopup->setCurrentRow(row);
    if (before != m_suggestionPopup->currentRow())
        detail::notifyAutoSuggestActiveDescendantChanged(this);
}

void AutoSuggestBox::setTextWithReason(const QString& value, TextChangeReason reason, bool emitWhenUnchanged)
{
    if (text() == value) {
        m_nextChangeReason = TextChangeReason::ProgrammaticChange;
        if (emitWhenUnchanged) {
            updateButtonState();
            updateTextMargins();
            emit textChangedWithReason(value, reason);
        }
        return;
    }

    m_nextChangeReason = reason;
    setText(value);
}

void AutoSuggestBox::paintInputFrame(QPainter& painter)
{
    ::fluent::detail::TextInputFrameState state;
    state.enabled = isEnabled();
    state.readOnly = isReadOnly();
    state.focused = m_focused || (QApplication::focusWidget() && isAncestorOf(QApplication::focusWidget()));
    state.hovered = m_hovered;
    state.pressed = false;
    state.dark = effectiveThemeUsesDarkAppearance();
    ::fluent::detail::paintTextInputFrame(painter, QRectF(inputRect()), themeColorsRef(), themeRadius().control, state);
}

void AutoSuggestBox::paintHeader(QPainter& painter)
{
    const auto& colors = themeColorsRef();
    painter.setPen(colors.textPrimary);
    painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
    const QRect headerRect(0, 0, width(), kHeaderHeight);
    painter.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, m_header);
}

} // namespace fluent::textfields

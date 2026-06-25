#include "AutoSuggestBox.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QRegion>
#include <QStyledItemDelegate>
#include <QStringListModel>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <functional>
#include <utility>

#include "design/Spacing.h"
#include "design/Typography.h"
#include "components/basicinput/Button.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Flyout.h"

namespace fluent::textfields {

namespace {
bool isEditableKey(QKeyEvent* event) {
    if (!event) return false;
    const int key = event->key();
    if (key == Qt::Key_Backspace || key == Qt::Key_Delete) return true;
    if (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier)) return false;
    return !event->text().isEmpty() && event->text().at(0).isPrint();
}

int normalizedPositiveSize(int size) {
    return qMax(1, size);
}
}

class AutoSuggestItemDelegate : public QStyledItemDelegate {
public:
    explicit AutoSuggestItemDelegate(const FluentElement* themeHost, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_themeHost(themeHost) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(0, m_itemHeight);
    }

    void setFontRole(const QString& role) {
        if (m_fontRole == role)
            return;
        m_fontRole = role;
    }

    void setItemHeight(int height) {
        m_itemHeight = normalizedPositiveSize(height);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        if (!index.isValid()) return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const auto colors = m_themeHost->themeColors();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool enabled = option.state & QStyle::State_Enabled;

        const QRectF bgRect = QRectF(option.rect).adjusted(2, 3, -2, -3);
        QColor bgColor = Qt::transparent;
        QColor textColor = enabled ? colors.textPrimary : colors.textDisabled;
        if (enabled && selected) bgColor = colors.subtleTertiary;
        else if (enabled && hovered) bgColor = colors.subtleSecondary;

        if (bgColor.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(bgColor);
            painter->drawRoundedRect(bgRect, m_themeHost->themeRadius().control,
                                      m_themeHost->themeRadius().control);
        }

        painter->setFont(m_themeHost->themeFont(m_fontRole).toQFont());
        painter->setPen(textColor);

        const QRectF textRect = QRectF(option.rect).adjusted(12, 0, -8, 0);
        const QString display = index.data(Qt::DisplayRole).toString();
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(display, Qt::ElideRight, int(textRect.width())));

        painter->restore();
    }

private:
    const FluentElement* m_themeHost = nullptr;
    QString m_fontRole = Typography::FontRole::Body;
    int m_itemHeight = ::Spacing::ControlHeight::Large;
};

class SuggestionListPopup : public fluent::dialogs_flyouts::Flyout {
public:
    using SuggestionClickedHandler = std::function<void(int)>;

    explicit SuggestionListPopup(AutoSuggestBox* owner)
        : Flyout(owner), m_owner(owner) {
        setObjectName("AutoSuggestBoxSuggestionPopup");
        setAnimationEnabled(false);
        setPlacement(fluent::dialogs_flyouts::Flyout::Auto);
        setAnchorOffset(::Spacing::XSmall);
        setModal(false);
        setDim(false);
        setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));

        m_model = new QStringListModel(this);
        m_listView = new fluent::collections::ListView(this);
        m_listView->setObjectName("AutoSuggestBoxSuggestionList");
        m_listView->setModel(m_model);
        m_itemDelegate = new AutoSuggestItemDelegate(this, m_listView);
        m_listView->setItemDelegate(m_itemDelegate);
        m_listView->setBorderVisible(false);
        m_listView->setBackgroundVisible(false);
        m_listView->setScrollChainingEnabled(false);
        m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_listView->setSelectionMode(fluent::collections::ListView::ListSelectionMode::Single);
        m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_listView->setMouseTracking(true);
        m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_listView->setStyleSheet("QListView { background: transparent; border: none; outline: none; }"
                                  "QListView::item { background: transparent; }");
        m_listView->viewport()->setAutoFillBackground(false);

        connect(m_listView, &fluent::collections::ListView::itemClicked, this, [this](int row) {
            if (row >= 0 && row < m_model->rowCount() && m_suggestionClickedHandler) {
                m_suggestionClickedHandler(row);
            }
        });
    }

    void setSuggestionClickedHandler(SuggestionClickedHandler handler) {
        m_suggestionClickedHandler = std::move(handler);
    }

    void setSuggestions(const QStringList& suggestions) {
        m_model->setStringList(suggestions);
    }

    void setSuggestionMetrics(const QString& fontRole, int itemHeight) {
        m_itemHeight = normalizedPositiveSize(itemHeight);
        if (m_itemDelegate) {
            m_itemDelegate->setFontRole(fontRole);
            m_itemDelegate->setItemHeight(m_itemHeight);
        }
        if (m_listView) {
            if (m_listView->viewport()) m_listView->viewport()->update();
            m_listView->refreshFluentScrollChrome();
        }
        if (isOpen()) showForOwner();
    }

    int currentRow() const {
        const QModelIndex current = m_listView->currentIndex();
        return current.isValid() ? current.row() : -1;
    }

    void setCurrentRow(int row) {
        if (row < 0 || row >= m_model->rowCount()) {
            m_listView->clearSelection();
            m_listView->setCurrentIndex(QModelIndex());
            return;
        }

        const QModelIndex index = m_model->index(row, 0);
        m_listView->setCurrentIndex(index);
        if (m_listView->selectionModel()) {
            m_listView->selectionModel()->select(index,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
        m_listView->scrollTo(index, QAbstractItemView::EnsureVisible);
    }

    void showForOwner() {
        if (!m_owner || m_model->rowCount() == 0) {
            clearMask();
            close();
            return;
        }

        const int shadow = ::Spacing::Standard;
        const int visibleRows = qMin(m_model->rowCount(), 6);
        const int listPadY = 2;
        const int listHeight = visibleRows * m_itemHeight + listPadY * 2;
        const int contentWidth = qMax(m_owner->width(), 120);

        resize(contentWidth + shadow * 2, listHeight + shadow * 2);
        m_listView->setGeometry(shadow, shadow + listPadY, contentWidth, listHeight - listPadY * 2);
        m_listView->refreshFluentScrollChrome();
        setAnchor(m_owner);

        if (isOpen()) {
            move(computePosition());
            raise();
        } else {
            showAt(m_owner);
        }
        updateOwnerPassthroughMask();
        m_owner->setFocus(Qt::OtherFocusReason);
    }

    void onThemeUpdated() override {
        Flyout::onThemeUpdated();
        update();
        if (m_listView && m_listView->viewport()) m_listView->viewport()->update();
    }

private:
    void updateOwnerPassthroughMask() {
        if (!m_owner) {
            clearMask();
            return;
        }

        QRegion hitRegion(rect());
        const QRect ownerRect(mapFromGlobal(m_owner->mapToGlobal(QPoint(0, 0))), m_owner->size());
        hitRegion -= QRegion(ownerRect.adjusted(-1, -1, 1, 1));
        setMask(hitRegion);
    }

    AutoSuggestBox* m_owner = nullptr;
    fluent::collections::ListView* m_listView = nullptr;
    AutoSuggestItemDelegate* m_itemDelegate = nullptr;
    QStringListModel* m_model = nullptr;
    SuggestionClickedHandler m_suggestionClickedHandler;
    int m_itemHeight = ::Spacing::ControlHeight::Large;
};

AutoSuggestBox::AutoSuggestBox(QWidget* parent)
    : LineEdit(parent) {
    setAttribute(Qt::WA_Hover);
    setClearButtonEnabled(false);
    setFrameVisible(false);
    m_inputHeight = kDefaultInputHeight;
    setFixedHeight(totalPreferredHeight());

    initializeButtons();

    m_suggestionPopup = new SuggestionListPopup(this);
    updateSuggestionMetrics();
    m_suggestionPopup->setSuggestionClickedHandler([this](int row) {
        chooseSuggestion(row);
    });
    connect(m_suggestionPopup, &fluent::dialogs_flyouts::Popup::opened, this, [this]() {
        emit suggestionListOpenChanged(true);
    });
    connect(m_suggestionPopup, &fluent::dialogs_flyouts::Popup::closed, this, [this]() {
        emit suggestionListOpenChanged(false);
    });

    connect(this, &QLineEdit::textEdited, this, [this](const QString& editedText) {
        m_nextChangeReason = TextChangeReason::UserInput;
        m_userTypedText = editedText;
    });
    connect(this, &QLineEdit::textChanged, this, &AutoSuggestBox::handleTextChanged);

    updateHeaderTextMargins();
    updateTextMargins();
    updateButtonState();
}

AutoSuggestBox::~AutoSuggestBox() {
    delete m_suggestionPopup;
    m_suggestionPopup = nullptr;
}

bool AutoSuggestBox::isSuggestionListOpen() const {
    return m_suggestionPopup && m_suggestionPopup->isOpen();
}

void AutoSuggestBox::setHeader(const QString& header) {
    if (m_header == header) return;
    m_header = header;
    setFixedHeight(totalPreferredHeight());
    updateHeaderTextMargins();
    updateButtonGeometry();
    updateGeometry();
    update();
    if (isSuggestionListOpen()) m_suggestionPopup->showForOwner();
    emit headerChanged();
}

void AutoSuggestBox::setQueryIconGlyph(const QString& glyph) {
    if (m_queryIconGlyph == glyph) return;
    m_queryIconGlyph = glyph;
    if (m_queryButton) {
        m_queryButton->setIconGlyph(m_queryIconGlyph, Typography::FontSize::Body,
                                    Typography::FontFamily::SegoeFluentIcons);
    }
    emit queryIconGlyphChanged();
}

void AutoSuggestBox::setQueryIconVisible(bool visible) {
    if (m_queryIconVisible == visible) return;
    m_queryIconVisible = visible;
    updateButtonState();
    updateTextMargins();
    updateButtonGeometry();
    emit queryIconVisibleChanged();
}

void AutoSuggestBox::setQueryButtonPlacement(QueryButtonPlacement placement) {
    if (m_queryButtonPlacement == placement)
        return;

    m_queryButtonPlacement = placement;
    updateButtonGeometry();
    updateTextMargins();
    update();
    emit queryButtonPlacementChanged();
}

void AutoSuggestBox::setInputHeight(int height) {
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
    if (isSuggestionListOpen()) m_suggestionPopup->showForOwner();
    emit inputHeightChanged();
}

void AutoSuggestBox::setQueryButtonSize(int size) {
    const int normalized = normalizedPositiveSize(size);
    if (m_queryButtonSize == normalized)
        return;

    m_queryButtonSize = normalized;
    updateButtonGeometry();
    updateTextMargins();
    update();
    emit queryButtonSizeChanged();
}

void AutoSuggestBox::setClearButtonSize(int size) {
    const int normalized = normalizedPositiveSize(size);
    if (m_clearButtonSize == normalized)
        return;

    m_clearButtonSize = normalized;
    updateButtonGeometry();
    updateTextMargins();
    update();
    emit clearButtonSizeChanged();
}

void AutoSuggestBox::setSuggestionFontRole(const QString& role) {
    if (m_suggestionFontRole == role)
        return;

    m_suggestionFontRole = role;
    updateSuggestionMetrics();
    emit suggestionFontRoleChanged();
}

void AutoSuggestBox::setSuggestionItemHeight(int height) {
    const int normalized = normalizedPositiveSize(height);
    if (m_suggestionItemHeight == normalized)
        return;

    m_suggestionItemHeight = normalized;
    updateSuggestionMetrics();
    emit suggestionItemHeightChanged();
}

QSize AutoSuggestBox::sizeHint() const {
    QSize hint = LineEdit::sizeHint();
    hint.setHeight(totalPreferredHeight());
    hint.setWidth(qMax(hint.width(), 160));
    return hint;
}

QSize AutoSuggestBox::minimumSizeHint() const {
    QSize hint = sizeHint();
    hint.setWidth(qMax(hint.width(), 120));
    return hint;
}

void AutoSuggestBox::onThemeUpdated() {
    LineEdit::onThemeUpdated();
    if (m_queryButton) m_queryButton->onThemeUpdated();
    if (m_clearButton) m_clearButton->onThemeUpdated();
    if (m_suggestionPopup) m_suggestionPopup->onThemeUpdated();
    update();
}

void AutoSuggestBox::setSuggestions(const QStringList& suggestions) {
    if (m_suggestions == suggestions) return;
    m_suggestions = suggestions;
    if (m_suggestionPopup) m_suggestionPopup->setSuggestions(m_suggestions);
    emit suggestionsChanged();

    if (m_suggestions.isEmpty()) {
        closeSuggestionList();
    } else if ((hasFocus() || isSuggestionListOpen()) && !text().isEmpty()) {
        openSuggestionList();
    }
}

void AutoSuggestBox::clearSuggestions() {
    setSuggestions(QStringList{});
}

void AutoSuggestBox::paintEvent(QPaintEvent* event) {
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!m_header.isEmpty()) paintHeader(painter);
        paintInputFrame(painter);
    }
    LineEdit::paintEvent(event);
}

void AutoSuggestBox::resizeEvent(QResizeEvent* event) {
    LineEdit::resizeEvent(event);
    updateButtonGeometry();
    if (isSuggestionListOpen()) m_suggestionPopup->showForOwner();
}

void AutoSuggestBox::keyPressEvent(QKeyEvent* event) {
    if (!event) return;

    switch (event->key()) {
    case Qt::Key_Down:
        if (!m_suggestions.isEmpty()) {
            if (!isSuggestionListOpen()) openSuggestionList();
            const int nextRow = qMin(m_suggestionPopup->currentRow() + 1, m_suggestions.count() - 1);
            previewSuggestion(nextRow);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Up:
        if (isSuggestionListOpen()) {
            const int current = m_suggestionPopup->currentRow();
            if (current > 0) {
                previewSuggestion(current - 1);
            } else {
                setPopupCurrentRow(-1);
                setTextWithReason(m_userTypedText, TextChangeReason::ProgrammaticChange);
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
            closeSuggestionList();
            emit querySubmitted(text(), QVariant{});
        }
        event->accept();
        return;
    case Qt::Key_Escape:
        if (isSuggestionListOpen()) {
            closeSuggestionList();
            event->accept();
            return;
        }
        break;
    default:
        break;
    }

    if (isEditableKey(event) && !isReadOnly()) m_nextChangeReason = TextChangeReason::UserInput;
    LineEdit::keyPressEvent(event);
}

void AutoSuggestBox::focusInEvent(QFocusEvent* event) {
    m_focused = true;
    LineEdit::focusInEvent(event);
    update();
}

void AutoSuggestBox::focusOutEvent(QFocusEvent* event) {
    m_focused = false;
    LineEdit::focusOutEvent(event);
    update();
}

void AutoSuggestBox::moveEvent(QMoveEvent* event) {
    LineEdit::moveEvent(event);
    if (isSuggestionListOpen()) m_suggestionPopup->showForOwner();
}

void AutoSuggestBox::enterEvent(FluentEnterEvent* event) {
    m_hovered = true;
    LineEdit::enterEvent(event);
    update();
}

void AutoSuggestBox::leaveEvent(QEvent* event) {
    m_hovered = false;
    LineEdit::leaveEvent(event);
    update();
}

void AutoSuggestBox::changeEvent(QEvent* event) {
    LineEdit::changeEvent(event);
    if (!event)
        return;

    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::ReadOnlyChange) {
        updateButtonState();
        updateTextMargins();
        update();
    } else if (event->type() == QEvent::FontChange) {
        updateTextMargins();
    }
}

QRect AutoSuggestBox::inputRect() const {
    return QRect(0, inputTop(), width(), m_inputHeight);
}

int AutoSuggestBox::inputTop() const {
    return m_header.isEmpty() ? 0 : kHeaderHeight + kHeaderGap;
}

int AutoSuggestBox::totalPreferredHeight() const {
    return inputTop() + m_inputHeight;
}

int AutoSuggestBox::inputTextVerticalPadding() const {
    const QFont inputFont = themeFont(fontRole()).toQFont();
    const int textHeight = QFontMetrics(inputFont).height();
    const int centeredPadding = qMax(0, (m_inputHeight - textHeight) / 2);
    return qMin(::Spacing::Padding::TextFieldVertical, centeredPadding);
}

void AutoSuggestBox::initializeButtons() {
    m_queryButton = new ::fluent::basicinput::Button(this);
    m_queryButton->setObjectName("AutoSuggestBoxQueryButton");
    m_queryButton->setFluentStyle(::fluent::basicinput::Button::Subtle);
    m_queryButton->setFluentLayout(::fluent::basicinput::Button::IconOnly);
    m_queryButton->setFluentSize(::fluent::basicinput::Button::Small);
    m_queryButton->setFocusPolicy(Qt::NoFocus);
    m_queryButton->setFixedSize(m_queryButtonSize, m_queryButtonSize);
    m_queryButton->setIconGlyph(m_queryIconGlyph, Typography::FontSize::Body,
                                Typography::FontFamily::SegoeFluentIcons);

    connect(m_queryButton, &::fluent::basicinput::Button::clicked, this, [this]() {
        closeSuggestionList();
        emit querySubmitted(text(), QVariant{});
    });

    m_clearButton = new ::fluent::basicinput::Button(this);
    m_clearButton->setObjectName("AutoSuggestBoxClearButton");
    m_clearButton->setFluentStyle(::fluent::basicinput::Button::Subtle);
    m_clearButton->setFluentLayout(::fluent::basicinput::Button::IconOnly);
    m_clearButton->setFluentSize(::fluent::basicinput::Button::Small);
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    m_clearButton->setFixedSize(m_clearButtonSize, m_clearButtonSize);
    m_clearButton->setIconGlyph(Typography::Icons::Cancel, Typography::FontSize::Body,
                                Typography::FontFamily::SegoeFluentIcons);

    connect(m_clearButton, &::fluent::basicinput::Button::clicked, this, [this]() {
        setTextWithReason(QString(), TextChangeReason::UserInput);
        closeSuggestionList();
        setFocus();
    });

    updateButtonGeometry();
}

void AutoSuggestBox::updateButtonGeometry() {
    const QRect input = inputRect();

    if (m_queryButton) {
        m_queryButton->setFixedSize(m_queryButtonSize, m_queryButtonSize);
        const int y = input.top() + (input.height() - m_queryButtonSize) / 2;
        const int x = m_queryButtonPlacement == QueryButtonPlacement::Left
            ? input.left() + kButtonEdgeMargin
            : input.right() + 1 - kButtonEdgeMargin - m_queryButtonSize;
        m_queryButton->setGeometry(x, y, m_queryButtonSize, m_queryButtonSize);
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

void AutoSuggestBox::updateButtonState() {
    if (m_queryButton) {
        m_queryButton->setVisible(m_queryIconVisible);
        m_queryButton->setEnabled(isEnabled());
    }
    if (m_clearButton) {
        const bool visible = !text().isEmpty() && isEnabled() && !isReadOnly();
        m_clearButton->setVisible(visible);
        m_clearButton->setEnabled(isEnabled() && !isReadOnly());
    }
    updateButtonGeometry();
}

void AutoSuggestBox::updateTextMargins() {
    int leftMargin = ::Spacing::Padding::TextFieldHorizontal;
    int rightMargin = ::Spacing::Padding::TextFieldHorizontal;
    const bool clearVisible = !text().isEmpty() && isEnabled() && !isReadOnly();

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

void AutoSuggestBox::updateHeaderTextMargins() {
    setTextMargins(0, inputTop(), 0, 0);
}

void AutoSuggestBox::updateSuggestionMetrics() {
    if (m_suggestionPopup) {
        m_suggestionPopup->setSuggestionMetrics(m_suggestionFontRole, m_suggestionItemHeight);
    }
}

void AutoSuggestBox::handleTextChanged(const QString& changedText) {
    const TextChangeReason reason = m_nextChangeReason;
    m_nextChangeReason = TextChangeReason::ProgrammaticChange;

    if (reason == TextChangeReason::UserInput) m_userTypedText = changedText;

    updateButtonState();
    updateTextMargins();
    emit textChangedWithReason(changedText, reason);

    if (reason == TextChangeReason::UserInput) {
        if (changedText.isEmpty() || m_suggestions.isEmpty()) closeSuggestionList();
        else if (hasFocus()) openSuggestionList();
    }
}

void AutoSuggestBox::openSuggestionList() {
    if (!m_suggestionPopup || m_suggestions.isEmpty()) return;
    m_suggestionPopup->setSuggestions(m_suggestions);
    m_suggestionPopup->showForOwner();
}

void AutoSuggestBox::closeSuggestionList() {
    if (!m_suggestionPopup || !m_suggestionPopup->isOpen()) return;
    m_suggestionPopup->close();
}

void AutoSuggestBox::previewSuggestion(int row) {
    if (row < 0 || row >= m_suggestions.count()) return;
    setPopupCurrentRow(row);
    const QVariant item(m_suggestions.at(row));
    emit suggestionChosen(item);
    setTextWithReason(m_suggestions.at(row), TextChangeReason::ProgrammaticChange);
}

void AutoSuggestBox::chooseSuggestion(int row) {
    if (row < 0 || row >= m_suggestions.count()) return;
    setPopupCurrentRow(row);
    const QString value = m_suggestions.at(row);
    const QVariant item(value);
    setTextWithReason(value, TextChangeReason::SuggestionChosen, true);
    emit suggestionChosen(item);
    closeSuggestionList();
    emit querySubmitted(value, item);
}

void AutoSuggestBox::setPopupCurrentRow(int row) {
    if (m_suggestionPopup) m_suggestionPopup->setCurrentRow(row);
}

void AutoSuggestBox::setTextWithReason(const QString& value, TextChangeReason reason, bool emitWhenUnchanged) {
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

void AutoSuggestBox::paintInputFrame(QPainter& painter) {
    const auto colors = themeColors();
    const QRectF frameRect = QRectF(inputRect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bgColor;
    QColor borderColor;
    QColor bottomColor;
    int bottomWidth = ::Spacing::Border::Normal;

    if (!isEnabled()) {
        bgColor = colors.controlDisabled;
        borderColor = colors.strokeDivider;
        bottomColor = borderColor;
    } else if (isReadOnly()) {
        bgColor = colors.controlAltSecondary;
        borderColor = colors.strokeDefault;
        bottomColor = colors.strokeDivider;
    } else if (m_focused) {
        bgColor = (effectiveTheme() == Dark) ? colors.bgSolid : colors.controlDefault;
        borderColor = colors.strokeSecondary;
        bottomColor = colors.accentDefault;
        bottomWidth = ::Spacing::Border::Focused;
    } else if (m_hovered) {
        bgColor = colors.controlSecondary;
        borderColor = colors.strokeSecondary;
        bottomColor = colors.strokeSecondary;
    } else {
        bgColor = colors.controlDefault;
        borderColor = colors.strokeDefault;
        bottomColor = colors.strokeDivider;
    }

    const qreal radius = themeRadius().control;
    QPainterPath framePath;
    framePath.addRoundedRect(frameRect, radius, radius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawPath(framePath);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(borderColor, 1));
    painter.drawPath(framePath);

    if (isEnabled() && !isReadOnly()) {
        QPen bottomPen(bottomColor, bottomWidth);
        bottomPen.setCapStyle(Qt::RoundCap);
        painter.setPen(bottomPen);
        const qreal bottomY = frameRect.bottom() - (bottomWidth > 1 ? (bottomWidth - 1) / 2.0 : 0);
        QPainterPath bottomPath;
        bottomPath.moveTo(frameRect.left() + radius, bottomY);
        bottomPath.lineTo(frameRect.right() - radius, bottomY);
        painter.drawPath(bottomPath);
    }
}

void AutoSuggestBox::paintHeader(QPainter& painter) {
    const auto colors = themeColors();
    painter.setPen(colors.textPrimary);
    painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
    const QRect headerRect(0, 0, width(), kHeaderHeight);
    painter.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, m_header);
}

} // namespace fluent::textfields

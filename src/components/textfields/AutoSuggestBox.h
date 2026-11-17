#ifndef AUTOSUGGESTBOX_H
#define AUTOSUGGESTBOX_H

#include "compatibility/InputEventCompat.h"
#include "LineEdit.h"

#include <QAbstractItemModel>
#include <QMargins>
#include <QPointer>
#include <QStringList>
#include <QVariant>

class QKeyEvent;
class QInputMethodEvent;
class QMoveEvent;
class QPainter;
class QStringListModel;
class QTimer;

namespace fluent::basicinput {
class Button;
}

namespace fluent::status_info {
class ProgressRing;
}

namespace fluent {
class FontIcon;
}

namespace fluent::textfields {

class SuggestionListPopup;
namespace detail {
class AutoSuggestBoxAccessible;
}

/**
 * @brief LineEdit-based search box with a query button, clear action, and suggestion popup.
 * Supports static suggestion lists, external models, and asynchronous search loading states.
 * zh_CN: 基于 LineEdit 的搜索输入框，带查询与清除按钮及候选弹层，支持静态列表、外部模型与异步搜索状态。
 */
class AutoSuggestBox : public LineEdit {
    Q_OBJECT
    /**
     * @brief Suggestion strings displayed by the suggestion popup.
     * zh_CN: 建议弹层显示的候选文本列表。
     */
    Q_PROPERTY(QStringList suggestions READ suggestions WRITE setSuggestions NOTIFY suggestionsChanged)
    /**
     * @brief Optional label text displayed above the input row.
     * zh_CN: 显示在输入行上方的可选标签文本。
     */
    Q_PROPERTY(QString header READ header WRITE setHeader NOTIFY headerChanged)
    /**
     * @brief Icon glyph used by the query/search affordance.
     * zh_CN: 查询/搜索入口使用的图标字符。
     */
    Q_PROPERTY(QString queryIconGlyph READ queryIconGlyph WRITE setQueryIconGlyph NOTIFY queryIconGlyphChanged)
    /**
     * @brief Whether the query icon is visible.
     * zh_CN: 查询图标是否可见。
     */
    Q_PROPERTY(bool queryIconVisible READ isQueryIconVisible WRITE setQueryIconVisible NOTIFY queryIconVisibleChanged)
    Q_PROPERTY(bool clearButtonEnabled
        READ isClearButtonEnabled
        WRITE setClearButtonEnabled
        NOTIFY clearButtonEnabledChanged)
    /**
     * @brief Placement of the query action button.
     * zh_CN: 查询操作按钮的位置。
     */
    Q_PROPERTY(QueryButtonPlacement queryButtonPlacement
        READ queryButtonPlacement
        WRITE setQueryButtonPlacement
        NOTIFY queryButtonPlacementChanged)
    /**
     * @brief Preferred height of the input box.
     * zh_CN: 输入框的首选高度。
     */
    Q_PROPERTY(int inputHeight READ inputHeight WRITE setInputHeight NOTIFY inputHeightChanged)
    /**
     * @brief Size of the query action button.
     * zh_CN: 查询操作按钮尺寸。
     */
    Q_PROPERTY(int queryButtonSize READ queryButtonSize WRITE setQueryButtonSize NOTIFY queryButtonSizeChanged)
    /**
     * @brief Size of the clear button.
     * zh_CN: 清除按钮尺寸。
     */
    Q_PROPERTY(int clearButtonSize READ clearButtonSize WRITE setClearButtonSize NOTIFY clearButtonSizeChanged)
    /**
     * @brief Fluent Typography preset used by suggestion rows.
     * zh_CN: 候选项文本使用的预设字号样式。
     */
    Q_PROPERTY(Typography::FontRole suggestionFontRole
        READ suggestionFontRole
        WRITE setSuggestionFontRole
        NOTIFY suggestionFontRoleChanged)
    /**
     * @brief Height of each suggestion row.
     * zh_CN: 每个建议项行的高度。
     */
    Q_PROPERTY(int suggestionItemHeight
        READ suggestionItemHeight
        WRITE setSuggestionItemHeight
        NOTIFY suggestionItemHeightChanged)
    /**
     * @brief Whether the suggestion popup is open.
     * zh_CN: 建议列表弹层是否打开。
     */
    Q_PROPERTY(bool isSuggestionListOpen READ isSuggestionListOpen NOTIFY suggestionListOpenChanged)
    Q_PROPERTY(QAbstractItemModel* suggestionModel
        READ suggestionModel
        WRITE setSuggestionModel
        NOTIFY suggestionModelChanged)
    Q_PROPERTY(int suggestionCount READ suggestionCount NOTIFY suggestionCountChanged)
    Q_PROPERTY(bool asyncSuggestionsEnabled
        READ isAsyncSuggestionsEnabled
        WRITE setAsyncSuggestionsEnabled
        NOTIFY asyncSuggestionsEnabledChanged)
    Q_PROPERTY(int debounceInterval READ debounceInterval WRITE setDebounceInterval NOTIFY debounceIntervalChanged)
    Q_PROPERTY(LoadState loadState READ loadState NOTIFY loadStateChanged)
    Q_PROPERTY(bool hasMoreSuggestions READ hasMoreSuggestions NOTIFY hasMoreSuggestionsChanged)
    Q_PROPERTY(QString loadingText READ loadingText WRITE setLoadingText NOTIFY loadingTextChanged)
    Q_PROPERTY(QString emptyText READ emptyText WRITE setEmptyText NOTIFY emptyTextChanged)
    Q_PROPERTY(QString emptyDescription READ emptyDescription WRITE setEmptyDescription NOTIFY emptyDescriptionChanged)
    Q_PROPERTY(QString loadingMoreText READ loadingMoreText WRITE setLoadingMoreText NOTIFY loadingMoreTextChanged)
    Q_PROPERTY(QString moreResultsText READ moreResultsText WRITE setMoreResultsText NOTIFY moreResultsTextChanged)
    Q_PROPERTY(QString retryText READ retryText WRITE setRetryText NOTIFY retryTextChanged)
    Q_PROPERTY(QString loadMoreText READ loadMoreText WRITE setLoadMoreText NOTIFY loadMoreTextChanged)

public:
    enum class TextChangeReason { UserInput, ProgrammaticChange, SuggestionChosen };
    Q_ENUM(TextChangeReason)

    enum class QueryButtonPlacement { Right, Left, Leading, Trailing };
    Q_ENUM(QueryButtonPlacement)

    /** @brief State of the current asynchronous query; Ready may contain no rows.
     *  zh_CN: 当前异步查询状态；Ready 也可以没有结果。
     */
    enum class LoadState { Idle, Loading, Ready, Failed };
    Q_ENUM(LoadState)

    explicit AutoSuggestBox(QWidget* parent = nullptr);
    ~AutoSuggestBox() override;

    QStringList suggestions() const { return m_suggestions; }
    QString header() const { return m_header; }
    QString queryIconGlyph() const { return m_queryIconGlyph; }
    bool isQueryIconVisible() const { return m_queryIconVisible; }
    /** @brief Controls the inline clear action independently of the query button.
     * zh_CN: 独立于查询按钮控制行内清除操作。
     */
    bool isClearButtonEnabled() const { return m_inlineClearEnabled; }
    void setClearButtonEnabled(bool enabled);
    QueryButtonPlacement queryButtonPlacement() const { return m_queryButtonPlacement; }
    int inputHeight() const { return m_inputHeight; }
    int queryButtonSize() const { return m_queryButtonSize; }
    int clearButtonSize() const { return m_clearButtonSize; }
    Typography::FontRole suggestionFontRole() const { return m_suggestionFontRole; }
    int suggestionItemHeight() const { return m_suggestionItemHeight; }
    bool isSuggestionListOpen() const;

    /**
     * @brief Uses an external model (column 0 DisplayRole, optional UserRole data) as suggestions without copying or deleting it.
     * Pass nullptr or call setSuggestions() to switch back to string-list mode.
     * zh_CN: 使用外部模型作为候选项（不会复制或销毁模型）；传 nullptr 或调用 setSuggestions() 切回字符串列表模式。
     */
    void setSuggestionModel(QAbstractItemModel* model);
    QAbstractItemModel* suggestionModel() const;
    int suggestionCount() const;
    bool isAsyncSuggestionsEnabled() const { return m_asyncEnabled; }
    void setAsyncSuggestionsEnabled(bool enabled);
    int debounceInterval() const { return m_debounceInterval; }
    void setDebounceInterval(int milliseconds);
    LoadState loadState() const { return m_loadState; }
    bool hasMoreSuggestions() const { return m_hasMore; }
    QString loadingText() const { return m_loadingText; }
    void setLoadingText(const QString& text);
    QString emptyText() const { return m_emptyText; }
    void setEmptyText(const QString& text);
    QString emptyDescription() const { return m_emptyDescription; }
    void setEmptyDescription(const QString& text);
    /** @brief Text shown while another result page is loading.
     * zh_CN: 加载下一页结果时显示的文案。 */
    QString loadingMoreText() const { return m_loadingMoreText; }
    void setLoadingMoreText(const QString& text);
    /** @brief Text accompanying the action for another result page.
     * zh_CN: 更多结果操作旁的提示文案。 */
    QString moreResultsText() const { return m_moreResultsText; }
    void setMoreResultsText(const QString& text);
    /** @brief Caption of the retry action after a failed request.
     * zh_CN: 请求失败后重试操作的文案。 */
    QString retryText() const { return m_retryText; }
    void setRetryText(const QString& text);
    /** @brief Caption of the action that requests another result page.
     * zh_CN: 请求下一页结果的操作文案。 */
    QString loadMoreText() const { return m_loadMoreText; }
    void setLoadMoreText(const QString& text);

    /**
     * @brief Checks whether requestId is still current before updating an external model (updates must run on the GUI thread).
     * Editing, closing the popup, replacing the model, or disabling async mode invalidates earlier requests.
     * zh_CN: 更新外部模型前校验 requestId 是否仍为当前请求（更新需在 GUI 线程完成）；编辑、关闭弹层、更换模型或关闭异步模式都会使旧请求失效。
     */
    bool isSuggestionRequestCurrent(quint64 requestId) const;
    /**
     * @brief Completes requestId after updating an external suggestion model; returns false if outdated.
     * zh_CN: 更新外部模型后完成 requestId 对应的请求；请求已过期时返回 false。
     */
    bool completeSuggestionRequest(quint64 requestId, bool hasMore = false);
    /**
     * @brief Sets or appends QStringList results for requestId, returning false if outdated.
     * External-model mode should update the model and call completeSuggestionRequest() instead.
     * zh_CN: 在字符串列表模式下写入或追加 requestId 的结果，请求已过期时返回 false；外部模型请直接更新模型并调用 completeSuggestionRequest()。
     */
    bool setSuggestionsForRequest(quint64 requestId, const QStringList& suggestions, bool hasMore = false);
    /**
     * @brief Shows an error state for requestId (retains existing rows on pagination errors).
     * zh_CN: 显示 requestId 的失败状态（分页加载失败时保留已有结果）；请求已过期时返回 false。
     */
    bool failSuggestionRequest(quint64 requestId, const QString& message, const QString& details = QString());

    void setHeader(const QString& header);
    void setQueryIconGlyph(const QString& glyph);
    void setQueryIconVisible(bool visible);
    void setQueryButtonPlacement(QueryButtonPlacement placement);
    void setInputHeight(int height);
    void setQueryButtonSize(int size);
    void setClearButtonSize(int size);
    void setSuggestionFontRole(Typography::FontRole role);
    void setSuggestionItemHeight(int height);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

public slots:
    void setSuggestions(const QStringList& suggestions);
    void clearSuggestions();
    /** @brief Immediately requests the current query when asynchronous suggestions are enabled.
     *  zh_CN: 开启异步建议时立即请求当前查询。
     */
    void requestSuggestions();
    void loadMoreSuggestions();
    void retrySuggestions();
    void cancelSuggestions();

signals:
    void textChangedWithReason(const QString& text, AutoSuggestBox::TextChangeReason reason);
    void suggestionChosen(const QVariant& item);
    void querySubmitted(const QString& queryText, const QVariant& chosenSuggestion);
    void suggestionsChanged();
    void headerChanged();
    void queryIconGlyphChanged();
    void queryIconVisibleChanged();
    void queryButtonPlacementChanged();
    void inputHeightChanged();
    void queryButtonSizeChanged();
    void suggestionFontRoleChanged();
    void suggestionItemHeightChanged();
    void suggestionListOpenChanged(bool open);
    void suggestionModelChanged(QAbstractItemModel* model);
    void suggestionCountChanged(int count);
    void asyncSuggestionsEnabledChanged(bool enabled);
    void debounceIntervalChanged(int milliseconds);
    void loadStateChanged(LoadState state);
    void hasMoreSuggestionsChanged(bool hasMore);
    void loadingTextChanged();
    void emptyTextChanged();
    void emptyDescriptionChanged();
    void loadingMoreTextChanged();
    void moreResultsTextChanged();
    void retryTextChanged();
    void loadMoreTextChanged();
    /** @brief Emitted to request suggestion data; append is true when loading or retrying the next page.
     * zh_CN: 请求候选项数据时发出；加载下一页或其重试时 append 为 true。
     */
    void suggestionsRequested(const QString& query, quint64 requestId, bool append);
    /** @brief Emitted when an in-flight requestId is canceled or superseded.
     * zh_CN: 进行中的 requestId 被取消或被新请求取代时发出。
     */
    void suggestionRequestCanceled(quint64 requestId);

protected:
    QRect trailingContentBounds() const override;
    void trailingContentGeometryChanged() override;
    ::fluent::basicinput::Button* clearActionButton() const override { return m_clearButton; }
    /** @brief Shows the query icon as non-interactive decoration for composite pickers.
     * zh_CN: 将查询图标设为仅装饰显示，供组合选择器保留单一输入焦点目标。 */
    void setQueryIconDecorative(bool decorative);
    void paintEvent(QPaintEvent* event) override;
    void paintFrame(QPainter& painter) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    bool isInputMethodComposing() const { return m_composing; }
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    friend class detail::AutoSuggestBoxAccessible;
    friend class SuggestionListPopup;

    static constexpr int kDefaultInputHeight = 32;
    static constexpr int kDefaultSuggestionItemHeight = 40;
    static constexpr int kHeaderHeight = 20;
    static constexpr int kHeaderGap = 4;
    static constexpr int kButtonEdgeMargin = 4;
    static constexpr int kTextButtonGap = 4;

    QRect inputRect() const;
    int inputTop() const;
    int totalPreferredHeight() const;
    int inputTextVerticalPadding() const;
    bool queryButtonOnLeadingSide() const;
    Qt::LayoutDirection buttonLayoutDirection() const;

    void initializeButtons();
    void updateButtonGeometry();
    void updateButtonState();
    bool showsLoadingIndicator() const;
    void updateTextMargins();
    void updateHeaderTextMargins();
    void updateSuggestionMetrics();
    void handleTextChanged(const QString& text);

    void ensureSuggestionPopup();
    QWidget* suggestionListWidget() const;
    void openSuggestionList();
    void closeSuggestionList();
    void previewSuggestion(int row);
    void chooseSuggestion(int row);
    void setPopupCurrentRow(int row);
    void setTextWithReason(const QString& value, TextChangeReason reason, bool emitWhenUnchanged = false);
    QAbstractItemModel* effectiveSuggestionModel() const;
    QModelIndex suggestionIndex(int row) const;
    int nextEnabledSuggestion(int from, int direction) const;
    void observeSuggestionModel();
    void refreshSuggestionPresentation();
    void startSuggestionRequest(bool append);
    void invalidateSuggestionRequest();
    void setLoadState(LoadState state);
    void setHasMoreSuggestions(bool hasMore);
    bool canShowSuggestions() const;

    void paintHeader(QPainter& painter);

    QStringList m_suggestions;
    QStringListModel* m_stringModel = nullptr;
    QPointer<QAbstractItemModel> m_externalModel;
    QList<QMetaObject::Connection> m_modelConnections;
    QTimer* m_debounceTimer = nullptr;
    bool m_asyncEnabled = false;
    bool m_resultsCurrent = true;
    bool m_appendRequest = false;
    bool m_previewing = false;
    bool m_composing = false;
    bool m_inlineClearEnabled = true;
    bool m_dismissed = false;
    bool m_modelRefreshPending = false;
    int m_debounceInterval = 200;
    int m_reportedSuggestionCount = 0;
    quint64 m_requestSerial = 0;
    quint64 m_activeRequest = 0;
    LoadState m_loadState = LoadState::Idle;
    bool m_hasMore = false;
    QString m_asyncQuery;
    QString m_errorText;
    QString m_errorDetails;
    QString m_loadingText;
    QString m_loadingMoreText;
    QString m_moreResultsText;
    QString m_retryText;
    QString m_loadMoreText;
    QString m_emptyText;
    QString m_emptyDescription;
    QString m_header;
    QString m_queryIconGlyph = QString::fromUtf16(u"\uE721");
    bool m_queryIconDecorative = false;
    fluent::FontIcon* m_queryDecoration = nullptr;
    bool m_queryIconVisible = true;
    QueryButtonPlacement m_queryButtonPlacement = QueryButtonPlacement::Right;

    ::fluent::basicinput::Button* m_queryButton = nullptr;
    ::fluent::basicinput::Button* m_clearButton = nullptr;
    ::fluent::status_info::ProgressRing* m_loadingIndicator = nullptr;
    QPointer<SuggestionListPopup> m_suggestionPopup;

    TextChangeReason m_nextChangeReason = TextChangeReason::ProgrammaticChange;
    QString m_userTypedText;
    bool m_hovered = false;
    bool m_focused = false;

    int m_inputHeight = kDefaultInputHeight;
    int m_queryButtonSize = 24;
    int m_clearButtonSize = 24;
    Typography::FontRole m_suggestionFontRole = Typography::FontRole::Body;
    int m_suggestionItemHeight = kDefaultSuggestionItemHeight;
};

} // namespace fluent::textfields

#endif // AUTOSUGGESTBOX_H

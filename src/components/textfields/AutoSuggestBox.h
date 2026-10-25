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

namespace fluent {
class FontIcon;
}

namespace fluent::textfields {

class SuggestionListPopup;
namespace detail {
class AutoSuggestBoxAccessible;
}

/**
 * @brief LineEdit-based input box with query affordance and suggestion flyout.
 * zh_CN: 基于 LineEdit、带查询入口和建议浮层的输入框。
 *
 * AutoSuggestBox combines query icon, clear button, suggestion model, popup
 * open state, item metrics, and suggestion typography for search-style
 * workflows. zh_CN: AutoSuggestBox
 * 组合查询图标、清除按钮、建议模型、弹层打开态、item 尺寸和建议排版，
 * 用于搜索式输入流程。
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
     * @brief Preferred height of the input surface.
     * zh_CN: 输入表面的首选高度。
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
     * @brief Typography role used by suggestion rows.
     * zh_CN: 建议项行文本使用的排版角色。
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

    enum class QueryButtonPlacement { Right, Left };
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

    /** @brief Borrows a flat model using column zero, DisplayRole and optional UserRole payloads.
     *  zh_CN: 借用扁平模型第零列，DisplayRole 显示文字，UserRole 可提供选中项数据。
     * The model remains caller-owned. nullptr restores QStringList mode;
     * setSuggestions also returns to that mode. No external rows are copied.
     * zh_CN: 模型由调用方持有；nullptr 或 setSuggestions 切回字符串列表模式，不复制外部行。
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

    /** @brief Tests a request token before the application changes its borrowed model.
     *  zh_CN: 应用修改借用模型之前，用此函数检查请求 token 是否仍有效。
     * Editing, dismissal, model replacement and disabling invalidate requests.
     * Network work and model mutation remain application-owned, on the GUI thread.
     * zh_CN: 编辑、关闭、替换模型和禁用均使请求失效；网络与模型更新由应用负责，结果在 GUI 线程交付。
     */
    bool isSuggestionRequestCurrent(quint64 requestId) const;
    /** @brief Finishes a current request after guarded application model updates.
     *  zh_CN: 应用通过 token 检查并更新模型后，完成当前请求；过期结果返回 false。
     */
    bool completeSuggestionRequest(quint64 requestId, bool hasMore = false);
    /** @brief Accepts current string results, appending only for a load-more request.
     *  zh_CN: 接收当前字符串结果；仅加载更多请求追加结果，过期结果不会写入列表。
     * Available in QStringList mode. External models use completeSuggestionRequest.
     * zh_CN: 仅用于 QStringList 模式；外部模型使用 completeSuggestionRequest。
     */
    bool setSuggestionsForRequest(quint64 requestId, const QStringList& suggestions, bool hasMore = false);
    /** @brief Shows a current request failure; pagination failures retain existing rows.
     *  zh_CN: 展示当前请求失败；分页失败保留已有结果。
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
    /** @brief Requests application data; append is true only for pagination or its retry.
     *  zh_CN: 请求应用数据；仅分页及其重试的 append 为 true。
     */
    void suggestionsRequested(const QString& query, quint64 requestId, bool append);
    /** @brief Asks the application to cancel transport work for an invalidated token.
     *  zh_CN: 通知应用取消已失效 token 对应的传输工作。
     */
    void suggestionRequestCanceled(quint64 requestId);

protected:
    // Composed selectors keep the query slot as decoration and own one input target.
    void setQueryIconDecorative(bool decorative);
    void paintEvent(QPaintEvent* event) override;
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
    static constexpr int kTextButtonGap = 2;

    QRect inputRect() const;
    int inputTop() const;
    int totalPreferredHeight() const;
    int inputTextVerticalPadding() const;

    void initializeButtons();
    void updateButtonGeometry();
    void updateButtonState();
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

    void paintInputFrame(QPainter& painter);
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

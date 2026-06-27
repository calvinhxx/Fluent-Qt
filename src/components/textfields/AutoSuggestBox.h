#ifndef AUTOSUGGESTBOX_H
#define AUTOSUGGESTBOX_H

#include "LineEdit.h"

#include <QMargins>
#include <QStringList>
#include <QVariant>

class QKeyEvent;
class QMoveEvent;
class QPainter;

namespace fluent::basicinput { class Button; }

namespace fluent::textfields {

class SuggestionListPopup;

/**
 * @brief LineEdit-based input box with query affordance and suggestion flyout.
 * zh_CN: 基于 LineEdit、带查询入口和建议浮层的输入框。
 *
 * AutoSuggestBox combines query icon, clear button, suggestion model, popup open
 * state, item metrics, and suggestion typography for search-style workflows.
 * zh_CN: AutoSuggestBox 组合查询图标、清除按钮、建议模型、弹层打开态、item 尺寸和建议排版，
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
    /**
     * @brief Placement of the query action button.
     * zh_CN: 查询操作按钮的位置。
     */
    Q_PROPERTY(QueryButtonPlacement queryButtonPlacement READ queryButtonPlacement WRITE setQueryButtonPlacement NOTIFY queryButtonPlacementChanged)
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
    Q_PROPERTY(QString suggestionFontRole READ suggestionFontRole WRITE setSuggestionFontRole NOTIFY suggestionFontRoleChanged)
    /**
     * @brief Height of each suggestion row.
     * zh_CN: 每个建议项行的高度。
     */
    Q_PROPERTY(int suggestionItemHeight READ suggestionItemHeight WRITE setSuggestionItemHeight NOTIFY suggestionItemHeightChanged)
    /**
     * @brief Whether the suggestion popup is open.
     * zh_CN: 建议列表弹层是否打开。
     */
    Q_PROPERTY(bool isSuggestionListOpen READ isSuggestionListOpen NOTIFY suggestionListOpenChanged)

public:
    enum class TextChangeReason {
        UserInput,
        ProgrammaticChange,
        SuggestionChosen
    };
    Q_ENUM(TextChangeReason)

    enum class QueryButtonPlacement {
        Right,
        Left
    };
    Q_ENUM(QueryButtonPlacement)

    explicit AutoSuggestBox(QWidget* parent = nullptr);
    ~AutoSuggestBox() override;

    QStringList suggestions() const { return m_suggestions; }
    QString header() const { return m_header; }
    QString queryIconGlyph() const { return m_queryIconGlyph; }
    bool isQueryIconVisible() const { return m_queryIconVisible; }
    QueryButtonPlacement queryButtonPlacement() const { return m_queryButtonPlacement; }
    int inputHeight() const { return m_inputHeight; }
    int queryButtonSize() const { return m_queryButtonSize; }
    int clearButtonSize() const { return m_clearButtonSize; }
    QString suggestionFontRole() const { return m_suggestionFontRole; }
    int suggestionItemHeight() const { return m_suggestionItemHeight; }
    bool isSuggestionListOpen() const;

    void setHeader(const QString& header);
    void setQueryIconGlyph(const QString& glyph);
    void setQueryIconVisible(bool visible);
    void setQueryButtonPlacement(QueryButtonPlacement placement);
    void setInputHeight(int height);
    void setQueryButtonSize(int size);
    void setClearButtonSize(int size);
    void setSuggestionFontRole(const QString& role);
    void setSuggestionItemHeight(int height);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

public slots:
    void setSuggestions(const QStringList& suggestions);
    void clearSuggestions();

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

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
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

    void openSuggestionList();
    void closeSuggestionList();
    void previewSuggestion(int row);
    void chooseSuggestion(int row);
    void setPopupCurrentRow(int row);
    void setTextWithReason(const QString& value, TextChangeReason reason, bool emitWhenUnchanged = false);

    void paintInputFrame(QPainter& painter);
    bool paintBrandInputFrame(QPainter& painter);
    void paintHeader(QPainter& painter);

    QStringList m_suggestions;
    QString m_header;
    QString m_queryIconGlyph = QString::fromUtf16(u"\uE721");
    bool m_queryIconVisible = true;
    QueryButtonPlacement m_queryButtonPlacement = QueryButtonPlacement::Right;

    ::fluent::basicinput::Button* m_queryButton = nullptr;
    ::fluent::basicinput::Button* m_clearButton = nullptr;
    SuggestionListPopup* m_suggestionPopup = nullptr;

    TextChangeReason m_nextChangeReason = TextChangeReason::ProgrammaticChange;
    QString m_userTypedText;
    bool m_hovered = false;
    bool m_focused = false;

    int m_inputHeight = kDefaultInputHeight;
    int m_queryButtonSize = 24;
    int m_clearButtonSize = 24;
    QString m_suggestionFontRole = Typography::FontRole::Body;
    int m_suggestionItemHeight = kDefaultSuggestionItemHeight;
};

} // namespace fluent::textfields

#endif // AUTOSUGGESTBOX_H

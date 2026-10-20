#ifndef FLUENTQT_COMPONENTS_TEXTFIELDS_CODEBLOCK_H
#define FLUENTQT_COMPONENTS_TEXTFIELDS_CODEBLOCK_H

#include "components/layout/Expander.h"

namespace fluent::basicinput {
class Button;
}
namespace fluent::textfields {
namespace detail {
class CodeText;
class CodeScroll;
} // namespace detail

/**
 * @brief Read-only, selectable code with syntax colors, folding and bounded scrolling.
 * zh_CN: 支持语法配色、精确复制、折叠和限高滚动的只读代码块。
 *
 * C++, Python and QML are highlighted; other language identifiers use plain text.
 * Copying retains the original whitespace and line endings. The inherited
 * Expander controls folding. The block starts expanded.
 * zh_CN: 高亮 C++、Python 和 QML，其余语言按纯文本显示；复制保留原始空白和换行。
 */
class CodeBlock : public layout::Expander {
    Q_OBJECT
    Q_PROPERTY(QString code READ code WRITE setCode NOTIFY codeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool wordWrap READ isWordWrapEnabled WRITE setWordWrap NOTIFY wordWrapChanged)
    Q_PROPERTY(int maxVisibleLines READ maxVisibleLines WRITE setMaxVisibleLines NOTIFY maxVisibleLinesChanged)

public:
    explicit CodeBlock(QWidget* parent = nullptr);
    explicit CodeBlock(const QString& code, QWidget* parent = nullptr);
    QString code() const { return m_code; }
    void setCode(const QString& code);
    QString language() const { return m_language; }
    void setLanguage(const QString& language);
    bool isWordWrapEnabled() const;
    void setWordWrap(bool enabled);
    /** @brief Maximum visible rows; zero fits all content. Default: 16.
     * zh_CN: 最大可见行数，0 表示完整展开所有内容，默认 16。 */
    int maxVisibleLines() const;
    void setMaxVisibleLines(int lines);
    void onThemeUpdated() override;

public slots:
    /** @brief Copies the exact source and reports host clipboard completion.
     * zh_CN: 复制原始代码，并在宿主剪贴板操作完成后报告结果。 */
    void copy();

signals:
    void codeChanged(const QString& code);
    void languageChanged(const QString& language);
    void wordWrapChanged(bool enabled);
    void maxVisibleLinesChanged(int lines);
    void copyFinished(bool success);

private:
    void refreshCode();
    void ensureHighlighted();
    QString m_code;
    QString m_language = QStringLiteral("plain");
    bool m_dirty = true;
    bool m_copying = false;
    detail::CodeText* m_text = nullptr;
    detail::CodeScroll* m_scroll = nullptr;
    basicinput::Button* m_copyButton = nullptr;
};
} // namespace fluent::textfields
#endif

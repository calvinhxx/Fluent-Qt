#ifndef GALLERYCODEBLOCK_H
#define GALLERYCODEBLOCK_H

#include <QString>

#include "components/layout/Expander.h"
#include "view/support/GalleryCodeLanguage.h"

namespace fluent::basicinput {
class Button;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

class GalleryLanguageSelector;

/**
 * @brief Gallery source-code presentation built on the reusable Expander.
 * zh_CN: 基于通用 Expander 组合的 Gallery 源码展示控件。
 */
class GalleryCodeBlock : public fluent::layout::Expander {
    Q_OBJECT

public:
    explicit GalleryCodeBlock(const QString& code, QWidget* parent = nullptr);
    GalleryCodeBlock(const QString& cppCode,
                     const QString& pythonCode,
                     QWidget* parent = nullptr);

    QString code() const;
    QString cppCode() const { return m_cppCode; }
    QString pythonCode() const { return m_pythonCode; }
    bool hasPythonCode() const { return !m_pythonCode.isEmpty(); }
    GalleryCodeLanguage codeLanguage() const { return m_codeLanguage; }
    GalleryLanguageSelector* languageSelector() const
    {
        return m_languageSelector;
    }
    fluent::basicinput::Button* copyButton() const { return m_copyButton; }

    void setCodeLanguage(GalleryCodeLanguage language);
    void setExpanded(bool expanded, bool animated = true);
    void toggleExpanded() { setExpanded(!isExpanded()); }

    void onThemeUpdated() override;

signals:
    void codeLanguageChanged(
        fluent::gallery::GalleryCodeLanguage language);

private:
    void applyPalette();
    void applyHighlightedCode();
    void ensureHighlighted();

    QString m_cppCode;
    QString m_pythonCode;
    QString m_cppHighlightedHtml;
    QString m_pythonHighlightedHtml;
    GalleryCodeLanguage m_codeLanguage = GalleryCodeLanguage::Cpp;
    QWidget* m_contentInner = nullptr;
    GalleryLanguageSelector* m_languageSelector = nullptr;
    fluent::textfields::Label* m_langLabel = nullptr;
    QWidget* m_langUnderline = nullptr;
    fluent::textfields::Label* m_codeLabel = nullptr;
    fluent::basicinput::Button* m_copyButton = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYCODEBLOCK_H

#ifndef GALLERYCODEBLOCK_H
#define GALLERYCODEBLOCK_H

#include <QString>

#include "components/layout/Expander.h"
#include "model/GalleryContentCatalog.h"
#include "view/support/GalleryCodeLanguage.h"

namespace fluent::basicinput {
class Button;
class ComboBox;
} // namespace fluent::basicinput

namespace fluent::textfields {
class Label;
}

namespace fluent::navigation {
class SelectorBar;
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
    GalleryCodeBlock(const QString& cppCode, const QString& pythonCode, QWidget* parent = nullptr);

    QString code() const;
    void setCppCode(const QString& code);
    void setCppExcerpt(const QString& code);
    void setSourceFiles(const QVector<GallerySourceFile>& cppFiles, const QVector<GallerySourceFile>& pythonFiles = {});
    QString sourceLanguage() const;
    QVector<GallerySourceFile> sourceFileBundle() const { return sourceFiles(); }
    QString cppCode() const { return m_cppCode; }
    QString pythonCode() const { return m_pythonCode; }
    bool hasPythonCode() const { return !m_pythonCode.isEmpty(); }
    GalleryCodeLanguage codeLanguage() const { return m_codeLanguage; }
    GalleryLanguageSelector* languageSelector() const { return m_languageSelector; }
    fluent::basicinput::Button* copyButton() const { return m_copyButton; }

    void setCodeLanguage(GalleryCodeLanguage language);
    void setExpanded(bool expanded, bool animated = true);
    void toggleExpanded() { setExpanded(!isExpanded()); }

    void onThemeUpdated() override;

signals:
    void codeLanguageChanged(fluent::gallery::GalleryCodeLanguage language);

private:
    void applyPalette();
    void applyHighlightedCode();
    void ensureHighlighted();
    void refreshDisplayedCode();
    const QVector<GallerySourceFile>& sourceFiles() const;
    const GallerySourceFile* currentFile() const;
    void rebuildFileSelector(const QString& preferredName = {});
    void updateSourceControls();

    QString m_cppCode;
    QString m_pythonCode;
    QString m_cppExcerpt;
    QString m_excerptHighlightedHtml;
    QString m_cppHighlightedHtml;
    QString m_pythonHighlightedHtml;
    QVector<GallerySourceFile> m_cppFiles;
    QVector<GallerySourceFile> m_pythonFiles;
    GalleryCodeLanguage m_codeLanguage = GalleryCodeLanguage::Cpp;
    QWidget* m_contentInner = nullptr;
    GalleryLanguageSelector* m_languageSelector = nullptr;
    fluent::navigation::SelectorBar* m_sourceSelector = nullptr;
    fluent::basicinput::ComboBox* m_fileSelector = nullptr;
    QWidget* m_fileRow = nullptr;
    fluent::textfields::Label* m_fileLanguage = nullptr;
    fluent::textfields::Label* m_langLabel = nullptr;
    QWidget* m_langUnderline = nullptr;
    fluent::textfields::Label* m_codeLabel = nullptr;
    fluent::basicinput::Button* m_copyButton = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYCODEBLOCK_H

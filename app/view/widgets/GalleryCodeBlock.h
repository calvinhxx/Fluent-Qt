#ifndef GALLERYCODEBLOCK_H
#define GALLERYCODEBLOCK_H

#include <QString>

#include "components/layout/Expander.h"

namespace fluent::basicinput {
class Button;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

/**
 * @brief Gallery source-code presentation built on the reusable Expander.
 * zh_CN: 基于通用 Expander 组合的 Gallery 源码展示控件。
 */
class GalleryCodeBlock : public fluent::layout::Expander {
    Q_OBJECT

public:
    explicit GalleryCodeBlock(const QString& code, QWidget* parent = nullptr);

    QString code() const { return m_code; }
    fluent::basicinput::Button* copyButton() const { return m_copyButton; }

    void setExpanded(bool expanded, bool animated = true);
    void toggleExpanded() { setExpanded(!isExpanded()); }

    void onThemeUpdated() override;

private:
    void applyPalette();
    void applyHighlightedCode();
    void ensureHighlighted();

    QString m_code;
    QWidget* m_contentInner = nullptr;
    fluent::textfields::Label* m_langLabel = nullptr;
    QWidget* m_langUnderline = nullptr;
    fluent::textfields::Label* m_codeLabel = nullptr;
    fluent::basicinput::Button* m_copyButton = nullptr;
    bool m_highlighted = false;
};

} // namespace fluent::gallery

#endif // GALLERYCODEBLOCK_H

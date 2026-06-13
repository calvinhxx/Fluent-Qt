#ifndef GALLERYCODEBLOCK_H
#define GALLERYCODEBLOCK_H

#include <QFrame>
#include <QString>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QResizeEvent;
class QSize;
class QVariantAnimation;

namespace fluent::basicinput {
class Button;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

class CodeBlockHeader;   // defined in GalleryCodeBlock.cpp
class CodeBlockChevron;  // defined in GalleryCodeBlock.cpp

/**
 * @brief Collapsible "Source code" surface: an animated expander hiding a code snippet.
 * zh_CN: 可折叠的"Source code"表面：用动画展开/收起隐藏代码片段的容器。
 *
 * The header (rotating chevron + caption) toggles a height-animated reveal of the code;
 * the copy-to-clipboard affordance appears while expanded. Collapsed by default, mirroring
 * the WinUI Gallery source-code expander.
 * zh_CN: 头部（旋转箭头 + 标题）切换代码的高度动画展开；展开时显示复制到剪贴板入口。
 * 默认折叠，对齐 WinUI Gallery 的源码 expander。
 */
class GalleryCodeBlock : public QFrame, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT

public:
    explicit GalleryCodeBlock(const QString& code, QWidget* parent = nullptr);

    QString code() const { return m_code; }
    fluent::basicinput::Button* copyButton() const { return m_copyButton; }

    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded, bool animated = true);
    void toggleExpanded() { setExpanded(!m_expanded); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void layoutHeightChanged(int height);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyPalette();
    void applyHighlightedCode();
    void ensureHighlighted();
    int naturalContentHeight() const;
    int currentContentHeight() const;
    int blockHeightForContent(int contentHeight) const;
    void updateContentInnerGeometry();
    void applyFraction(double fraction);
    double currentFraction() const { return m_fraction; }

    QString m_code;
    CodeBlockHeader* m_header = nullptr;
    CodeBlockChevron* m_chevron = nullptr;
    fluent::textfields::Label* m_captionLabel = nullptr;
    QWidget* m_content = nullptr;
    QWidget* m_contentInner = nullptr;
    fluent::textfields::Label* m_langLabel = nullptr;
    QWidget* m_langUnderline = nullptr;
    fluent::textfields::Label* m_codeLabel = nullptr;
    fluent::basicinput::Button* m_copyButton = nullptr;
    QVariantAnimation* m_animation = nullptr;

    bool m_expanded = false;
    bool m_highlighted = false;     // code is only syntax-highlighted on first expand (lazy)
    double m_fraction = 0.0;        // 0 = collapsed, 1 = expanded
    int m_contentTargetHeight = 0;  // pixel height the content animates toward
    int m_lastEmittedLayoutHeight = -1;
};

} // namespace fluent::gallery

#endif // GALLERYCODEBLOCK_H

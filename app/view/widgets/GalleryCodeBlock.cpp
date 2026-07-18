#include "GalleryCodeBlock.h"

#include <functional>

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QtGlobal>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "support/logging/Log.h"
#include "view/support/GalleryCodeHighlighter.h"
#include "view/support/GalleryStyleSupport.h"
#include "view/support/GalleryToast.h"

namespace fluent::gallery {

namespace {
constexpr int kHeaderHeight = 44;
// The box must exceed the glyph's diagonal extent, or the chevron clips against the widget
// edges while it rotates through 90deg (which read as a per-toggle jitter).
// zh_CN: 方块要大于字形旋转到 90° 时的对角范围，否则箭头旋转途中会被裁剪，看起来就是每次切换抖一下。
constexpr int kChevronSize = 22;
constexpr int kChevronGlyphPixelSize = 12;
constexpr int kCopyCheckRevertMs = 1300;

QScrollArea* enclosingScrollArea(QWidget* widget)
{
    for (QWidget* ancestor = widget ? widget->parentWidget() : nullptr;
         ancestor;
         ancestor = ancestor->parentWidget()) {
        if (auto* scrollArea = qobject_cast<QScrollArea*>(ancestor))
            return scrollArea;
    }
    return nullptr;
}
}

/**
 * @brief Chevron glyph that paints at an animated rotation (down collapsed, up expanded).
 * zh_CN: 以动画旋转角度绘制的箭头字形（折叠朝下，展开朝上）。
 */
class CodeBlockChevron : public QWidget {
public:
    explicit CodeBlockChevron(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(kChevronSize, kChevronSize);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    void setAngle(double degrees)
    {
        if (qFuzzyCompare(m_angle, degrees))
            return;
        m_angle = degrees;
        update();
    }

    void setColor(const QColor& color)
    {
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(m_angle);

        QFont glyphFont(Typography::FontFamily::FluentIcons);
        glyphFont.setPixelSize(kChevronGlyphPixelSize);
        painter.setFont(glyphFont);
        painter.setPen(m_color);
        const QRectF box(-width() / 2.0, -height() / 2.0, width(), height());
        painter.drawText(box, Qt::AlignCenter, Typography::Icons::ChevronDown);
    }

private:
    double m_angle = 0.0;
    QColor m_color = Qt::black;
};

/**
 * @brief Clickable expander header with a subtle hover highlight.
 * zh_CN: 可点击的 expander 头部，带细微悬停高亮。
 */
class CodeBlockHeader : public QWidget {
public:
    explicit CodeBlockHeader(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(kHeaderHeight);
    }

    std::function<void()> onActivated;

    void setHoverColor(const QColor& color)
    {
        m_hoverColor = color;
        update();
    }

protected:
    void enterEvent(FluentEnterEvent* event) override
    {
        QWidget::enterEvent(event);
        m_hovered = true;
        update();
    }

    void leaveEvent(QEvent* event) override
    {
        QWidget::leaveEvent(event);
        m_hovered = false;
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            if (onActivated)
                onActivated();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        if (!m_hovered || m_hoverColor.alpha() == 0)
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_hoverColor);
        painter.drawRoundedRect(rect().adjusted(3, 3, -3, -3),
                                ::CornerRadius::Control, ::CornerRadius::Control);
    }

private:
    bool m_hovered = false;
    QColor m_hoverColor = Qt::transparent;
};

GalleryCodeBlock::GalleryCodeBlock(const QString& code, QWidget* parent)
    : QFrame(parent)
    , m_code(code)
{
    setObjectName(QStringLiteral("galleryCodeBlock"));
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new AnchorLayout(this);

    // Header: "Source code" on the left, rotating chevron on the right (mirrors the WinUI
    // Gallery expander). The chevron points down collapsed and up expanded.
    // zh_CN: 头部：左侧 "Source code"，右侧旋转箭头（对齐 WinUI Gallery 的 expander）。
    // 箭头折叠时朝下、展开时朝上。
    m_header = new CodeBlockHeader(this);
    m_header->setObjectName(QStringLiteral("galleryCodeBlockHeader"));
    auto* headerRow = new QHBoxLayout(m_header);
    headerRow->setContentsMargins(16, 0, 14, 0);
    headerRow->setSpacing(8);

    m_captionLabel = new fluent::textfields::Label(QStringLiteral("Source code"), m_header);
    m_captionLabel->setObjectName(QStringLiteral("galleryCodeBlockCaption"));
    m_captionLabel->setFluentTypography(Typography::FontRole::BodyStrong);

    m_chevron = new CodeBlockChevron(m_header);

    headerRow->addWidget(m_captionLabel, 0, Qt::AlignVCenter);
    headerRow->addStretch(1);
    headerRow->addWidget(m_chevron, 0, Qt::AlignVCenter);

    m_header->onActivated = [this]() { toggleExpanded(); };

    // Content: an outer clip container whose height animates, plus an inner holder pinned to
    // the top that keeps its natural size. Animating only the outer height reveals the code by
    // clipping the inner holder, so the code never reflows or squeezes during the animation.
    // zh_CN: 内容分两层：外层"裁剪容器"高度做动画，内层"内容固定块"贴顶、保持自然尺寸。
    // 只对外层高度做动画，靠裁剪内层来揭示代码，动画期间代码不会重排/被压扁。
    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("galleryCodeBlockContent"));
    m_content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_contentInner = new QWidget(m_content);
    m_contentInner->setObjectName(QStringLiteral("galleryCodeBlockContentInner"));
    m_contentInner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* innerLayout = new QVBoxLayout(m_contentInner);
    innerLayout->setContentsMargins(16, 12, 14, 16);
    innerLayout->setSpacing(10);

    // Top row of the code area: a language label ("C++") with an accent underline tab on the
    // left, and an icon-only copy button on the right — exactly like the WinUI Gallery sample.
    // zh_CN: 代码区顶部行：左侧语言标签（"C++"）加强调色下划线 tab，右侧图标版复制按钮——与
    // WinUI Gallery 示例一致。
    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);

    auto* langColumn = new QVBoxLayout();
    langColumn->setContentsMargins(0, 0, 0, 0);
    langColumn->setSpacing(4);
    m_langLabel = new fluent::textfields::Label(QStringLiteral("C++"), m_contentInner);
    m_langLabel->setObjectName(QStringLiteral("galleryCodeBlockLang"));
    m_langLabel->setFluentTypography(Typography::FontRole::Caption);
    m_langUnderline = new QWidget(m_contentInner);
    m_langUnderline->setObjectName(QStringLiteral("galleryCodeBlockLangUnderline"));
    m_langUnderline->setFixedSize(22, 3);
    langColumn->addWidget(m_langLabel, 0, Qt::AlignLeft);
    langColumn->addWidget(m_langUnderline, 0, Qt::AlignLeft);

    m_copyButton = new fluent::basicinput::Button(QString(), m_contentInner);
    m_copyButton->setObjectName(QStringLiteral("galleryCodeBlockCopyButton"));
    m_copyButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_copyButton->setFluentSize(fluent::basicinput::Button::Small);
    m_copyButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_copyButton->setIconGlyph(Typography::Icons::Copy, Typography::FontSize::Body);
    m_copyButton->setFocusPolicy(Qt::NoFocus);
    fluent::status_info::ToolTip::attach(m_copyButton, QStringLiteral("Copy"));
    connect(m_copyButton, &fluent::basicinput::Button::clicked, this, [this]() {
        if (QClipboard* clipboard = QApplication::clipboard()) {
            clipboard->setText(m_code);
            LOG_DEBUG(QStringLiteral("GalleryCodeBlock copyCode chars=%1").arg(m_code.size()));
            showGalleryToast(this, QStringLiteral("Copied to clipboard"));
            // Briefly flip the icon to a checkmark for inline confirmation (matches the gif).
            // zh_CN: 短暂把图标切成对勾做就地确认（对齐 gif）。
            m_copyButton->setIconGlyph(Typography::Icons::CheckMark, Typography::FontSize::Body);
            QPointer<fluent::basicinput::Button> button = m_copyButton;
            QTimer::singleShot(kCopyCheckRevertMs, this, [button]() {
                if (button)
                    button->setIconGlyph(Typography::Icons::Copy, Typography::FontSize::Body);
            });
        }
    });

    topRow->addLayout(langColumn, 0);
    topRow->addStretch(1);
    topRow->addWidget(m_copyButton, 0, Qt::AlignTop);

    m_codeLabel = new fluent::textfields::Label(m_contentInner);
    m_codeLabel->setObjectName(QStringLiteral("galleryCodeBlockText"));
    m_codeLabel->setTextFormat(Qt::RichText);
    m_codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_codeLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QFont monospace = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    // Code is primary content once expanded; Caption (12 px) became visibly
    // undersized next to the bundled 14 px UI text, especially on Windows.
    // Keep a platform-native monospace face but use the Body optical size.
    // zh_CN: 源码展开后属于主要内容；12px Caption 与捆绑的 14px UI 字体并排时
    // 明显偏小（Windows 尤甚）。保留平台原生等宽字体，字号改用 Body。
    monospace.setPixelSize(Typography::FontSize::Body);
    m_codeLabel->setFont(monospace);
    // Lazy: syntax highlighting + RichText shaping is the heaviest part of building a card and
    // the block starts collapsed, so defer it until the first expand. Most blocks are never
    // opened, so this keeps page navigation snappy.
    // zh_CN: 懒加载：语法高亮 + 富文本排版是建卡最重的一步，而代码块默认折叠，所以推迟到首次展开再做。
    // 大多数代码块从不展开，这样能让页面切换保持流畅。

    innerLayout->addLayout(topRow);
    innerLayout->addWidget(m_codeLabel);

    using Edge = AnchorLayout::Edge;
    AnchorLayout::Anchors headerAnchors;
    headerAnchors.left = {this, Edge::Left, 0};
    headerAnchors.right = {this, Edge::Right, 0};
    headerAnchors.top = {this, Edge::Top, 0};
    layout->addAnchoredWidget(m_header, headerAnchors);

    AnchorLayout::Anchors contentAnchors;
    contentAnchors.left = {this, Edge::Left, 0};
    contentAnchors.right = {this, Edge::Right, 0};
    contentAnchors.top = {m_header, Edge::Bottom, 1};
    layout->addAnchoredWidget(m_content, contentAnchors);

    // Start collapsed.
    // zh_CN: 初始折叠。
    m_content->setFixedHeight(0);
    updateContentInnerGeometry();

    m_animation = new QVariantAnimation(this);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) { applyFraction(value.toDouble()); });
    connect(m_animation, &QVariantAnimation::finished, this, [this]() {
        applyFraction(m_expanded ? 1.0 : 0.0);
        emit expansionTransitionFinished(m_expanded);
        finishViewportTransition();
    });

    applyPalette();
}

void GalleryCodeBlock::setExpanded(bool expanded, bool animated)
{
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    beginViewportTransition();
    emit expansionTransitionStarted(expanded);

    // Highlight on first reveal, before measuring — the measured content height depends on the
    // (now populated) code label.
    // zh_CN: 首次展开时才高亮，并且在量高之前完成——量到的内容高度依赖（已填充的）代码 label。
    if (expanded)
        ensureHighlighted();

    m_animation->stop();
    // Measure the fixed inner holder directly. Keeping the clipping container visible and
    // height-constrained for the whole lifecycle avoids the show/hide and constraint-release
    // layout jumps that made the page feel shaky.
    // zh_CN: 直接量固定内层内容。裁剪容器始终可见且全程用高度约束，避免 show/hide 和释放约束带来的布局跳动。
    // Measure on reveal, when the clipped body is still at zero height. A
    // second measurement at the start of collapse can differ by a few pixels
    // after rich-text shaping settles, producing an upward jump immediately
    // before the shrinking animation. Collapse must reuse the exact height
    // that was used to expand.
    if (expanded || m_contentTargetHeight <= 0)
        m_contentTargetHeight = naturalContentHeight();
    applyFraction(m_fraction);

    // One anchor per toggle; the per-frame applyFraction path stays quiet.
    // zh_CN: 每次切换打一条；逐帧的 applyFraction 路径保持静默。
    LOG_DEBUG(QStringLiteral("GalleryCodeBlock setExpanded expanded=%1 contentHeight=%2 animated=%3")
                  .arg(expanded)
                  .arg(m_contentTargetHeight)
                  .arg(animated));

    const double target = expanded ? 1.0 : 0.0;
    if (!animated) {
        applyFraction(target);
        emit expansionTransitionFinished(m_expanded);
        finishViewportTransition();
        return;
    }

    const auto motion = themeAnimation();
    // Full toggles run at the normal token, not slow: a 400ms tail makes the
    // last few pixels crawl, which reads as the page stuttering near the end.
    // zh_CN: 整段切换用 normal 而非 slow：400ms 的尾段让最后几像素缓慢爬行，
    // 观感上像页面在收尾时抖动。
    const double distance = qAbs(target - m_fraction);
    const int duration = qRound(motion.fast + (motion.normal - motion.fast) * distance);
    // QVariantAnimation retains currentTime from the previous completed run.
    // Replacing its end value at that old end time emits the new target first,
    // then start() jumps back to the new start value. Configure it silently and
    // rewind before exposing any valueChanged notification.
    {
        const QSignalBlocker blocker(m_animation);
        m_animation->setStartValue(m_fraction);
        m_animation->setEndValue(target);
        m_animation->setDuration(duration);
        m_animation->setEasingCurve(motion.standard);
        m_animation->setCurrentTime(0);
    }
    m_animation->start();
}

QSize GalleryCodeBlock::sizeHint() const
{
    int width = 360;
    if (m_header)
        width = qMax(width, m_header->sizeHint().width());
    return QSize(width, blockHeightForContent(currentContentHeight()));
}

QSize GalleryCodeBlock::minimumSizeHint() const
{
    return QSize(0, blockHeightForContent(currentContentHeight()));
}

int GalleryCodeBlock::blockHeightForContent(int contentHeight) const
{
    // The content area sits 1px below the header (its divider line); include
    // that pixel whenever content shows, or the expanded body's bottom edge is
    // clipped by exactly one row.
    // zh_CN: 内容区位于头部下方 1px（分隔线）；只要内容可见就计入这 1px，
    // 否则展开态的底边正好被裁掉一行。
    return kHeaderHeight + (contentHeight > 0 ? 1 + contentHeight : 0);
}

void GalleryCodeBlock::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    if (!event || event->oldSize().width() == event->size().width())
        return;
    if (!m_contentInner || !m_content)
        return;

    m_contentTargetHeight = naturalContentHeight();
    applyFraction(m_fraction);
}

int GalleryCodeBlock::naturalContentHeight() const
{
    if (!m_contentInner)
        return 0;
    m_contentInner->ensurePolished();
    if (m_contentInner->layout())
        m_contentInner->layout()->activate();
    return qMax(0, m_contentInner->sizeHint().height());
}

int GalleryCodeBlock::currentContentHeight() const
{
    return qRound(m_contentTargetHeight * m_fraction);
}

void GalleryCodeBlock::updateContentInnerGeometry()
{
    if (!m_contentInner || !m_content)
        return;

    m_contentInner->setGeometry(0, 0, qMax(0, m_content->width()), qMax(0, m_contentTargetHeight));
}

void GalleryCodeBlock::applyFraction(double fraction)
{
    m_fraction = qBound(0.0, fraction, 1.0);
    const int contentHeight = currentContentHeight();
    const int blockHeight = blockHeightForContent(contentHeight);
    m_content->setFixedHeight(contentHeight);
    if (minimumHeight() != blockHeight || maximumHeight() != blockHeight)
        setFixedHeight(blockHeight);
    updateGeometry();
    if (layout())
        layout()->activate();
    updateContentInnerGeometry();
    if (m_chevron)
        m_chevron->setAngle(180.0 * m_fraction);
    if (m_lastEmittedLayoutHeight != blockHeight) {
        m_lastEmittedLayoutHeight = blockHeight;
        emit layoutHeightChanged(blockHeight);
        // layoutHeightChanged first gives an enclosing sample card a chance to
        // update its own fixed height. Once all direct slots return, resize the
        // scroll content and activate its layout in this same animation frame.
        // This prevents QScrollArea from briefly squeezing fixed-size siblings
        // while it waits for a later LayoutRequest.
        synchronizeViewportLayout();
    }
}

void GalleryCodeBlock::beginViewportTransition()
{
    ++m_viewportTransitionGeneration;
    clearViewportAnchor();

    m_transitionScrollArea = enclosingScrollArea(this);
    m_transitionScrollBar = m_transitionScrollArea
        ? m_transitionScrollArea->verticalScrollBar()
        : nullptr;
    if (!m_transitionScrollArea || !m_transitionScrollArea->viewport()
        || !m_transitionScrollBar || !m_header) {
        return;
    }

    m_viewportTransitionActive = true;
    m_anchorViewportY = m_transitionScrollArea->viewport()->mapFromGlobal(
        m_header->mapToGlobal(QPoint(0, 0))).y();

    // QScrollArea may update its range after an animation callback returns.
    // Keep the clicked header at one stable viewport coordinate until the
    // transition and its final queued layout pass are both complete.
    m_scrollRangeConnection = connect(
        m_transitionScrollBar, &QScrollBar::rangeChanged, this,
        [this]() { restoreViewportAnchor(); });
    m_scrollValueConnection = connect(
        m_transitionScrollBar, &QScrollBar::valueChanged, this,
        [this]() { restoreViewportAnchor(); });
}

void GalleryCodeBlock::finishViewportTransition()
{
    synchronizeViewportLayout();
    const quint64 generation = m_viewportTransitionGeneration;
    QTimer::singleShot(0, this, [this, generation]() {
        if (!m_viewportTransitionActive
            || generation != m_viewportTransitionGeneration) {
            return;
        }
        synchronizeViewportLayout();
        clearViewportAnchor();
    });
}

void GalleryCodeBlock::synchronizeViewportLayout()
{
    if (!m_viewportTransitionActive || !m_transitionScrollArea
        || !m_transitionScrollArea->viewport()) {
        return;
    }

    QWidget* scrollContent = m_transitionScrollArea->widget();
    if (!scrollContent)
        return;

    // Containers between the code block and the page (notably a
    // GallerySampleCard) must settle before the page asks for its new minimum.
    for (QWidget* ancestor = parentWidget();
         ancestor && ancestor != scrollContent;
         ancestor = ancestor->parentWidget()) {
        if (QLayout* ancestorLayout = ancestor->layout()) {
            ancestorLayout->invalidate();
            ancestorLayout->activate();
        }
    }

    QLayout* pageLayout = scrollContent->layout();
    if (pageLayout) {
        pageLayout->invalidate();
        const int contentHeight = qMax(m_transitionScrollArea->viewport()->height(),
                                       pageLayout->minimumSize().height());
        if (scrollContent->height() != contentHeight)
            scrollContent->resize(scrollContent->width(), contentHeight);
        pageLayout->activate();
    }

    restoreViewportAnchor();
}

void GalleryCodeBlock::restoreViewportAnchor()
{
    if (!m_viewportTransitionActive || m_restoringViewportAnchor
        || !m_transitionScrollArea || !m_transitionScrollArea->viewport()
        || !m_transitionScrollBar || !m_header) {
        return;
    }

    QWidget* scrollContent = m_transitionScrollArea->widget();
    if (!scrollContent)
        return;

    const int anchorContentY = m_header->mapTo(scrollContent, QPoint(0, 0)).y();
    const int desiredValue = qBound(m_transitionScrollBar->minimum(),
                                    anchorContentY - m_anchorViewportY,
                                    m_transitionScrollBar->maximum());
    if (m_transitionScrollBar->value() == desiredValue)
        return;

    m_restoringViewportAnchor = true;
    m_transitionScrollBar->setValue(desiredValue);
    m_restoringViewportAnchor = false;
}

void GalleryCodeBlock::clearViewportAnchor()
{
    QObject::disconnect(m_scrollRangeConnection);
    QObject::disconnect(m_scrollValueConnection);
    m_scrollRangeConnection = QMetaObject::Connection();
    m_scrollValueConnection = QMetaObject::Connection();
    m_viewportTransitionActive = false;
    m_restoringViewportAnchor = false;
    m_transitionScrollArea = nullptr;
    m_transitionScrollBar = nullptr;
}

void GalleryCodeBlock::onThemeUpdated()
{
    applyPalette();
    if (m_captionLabel)
        m_captionLabel->onThemeUpdated();
    if (m_langLabel)
        m_langLabel->onThemeUpdated();
    if (m_copyButton)
        m_copyButton->onThemeUpdated();
    // Re-color the snippet for the new theme (light/dark VSCode palette), but only if it has
    // already been highlighted — an unopened block stays lazy and picks up the theme on expand.
    // zh_CN: 按新主题（明/暗 VSCode 配色）重新着色——但仅当已经高亮过；未展开的块保持懒加载，
    // 等到展开时再按当时主题着色。
    if (m_highlighted)
        applyHighlightedCode();
}

void GalleryCodeBlock::applyHighlightedCode()
{
    if (m_codeLabel) {
        m_codeLabel->setText(highlightCppToHtml(m_code, currentTheme() == Dark));
        m_highlighted = true;
    }
}

void GalleryCodeBlock::ensureHighlighted()
{
    if (!m_highlighted)
        applyHighlightedCode();
}

void GalleryCodeBlock::applyPalette()
{
    const Colors colors = themeColors();
    setStyleSheet(QStringLiteral(
                      "#galleryCodeBlock { background: %1; border: 1px solid %2; border-radius: %3px; }"
                      "#galleryCodeBlockContent { background: transparent; border-top: 1px solid %2; }")
                      .arg(cssColor(colors.bgLayerAlt),
                           cssColor(colors.strokeCard))
                      .arg(::CornerRadius::Control));
    if (m_header)
        m_header->setHoverColor(colors.subtleSecondary);
    if (m_chevron)
        m_chevron->setColor(colors.textSecondary);
    if (m_captionLabel)
        m_captionLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                          .arg(cssColor(colors.textPrimary)));
    if (m_langLabel)
        m_langLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                       .arg(cssColor(colors.textSecondary)));
    if (m_langUnderline)
        m_langUnderline->setStyleSheet(
            QStringLiteral("background: %1; border-radius: 1px;").arg(cssColor(colors.accentDefault)));
    if (m_codeLabel)
        m_codeLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                       .arg(cssColor(colors.textPrimary)));
}

} // namespace fluent::gallery

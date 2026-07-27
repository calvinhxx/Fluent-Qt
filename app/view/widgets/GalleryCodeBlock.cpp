#include "GalleryCodeBlock.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "support/logging/Log.h"
#include "view/support/GalleryCodeHighlighter.h"
#include "view/support/GalleryStyleSupport.h"
#include "view/support/GalleryToast.h"

namespace fluent::gallery {
namespace {

constexpr int kCopyCheckRevertMs = 1300;

} // namespace

GalleryCodeBlock::GalleryCodeBlock(const QString& code, QWidget* parent)
    : Expander(parent),
      m_code(code)
{
    setObjectName(QStringLiteral("galleryCodeBlock"));
    setAppearance(Card::LayerAlt);
    setHeaderText(QStringLiteral("Source code"));

    // Preserve Gallery-specific object names used by focused visual/geometry
    // tests while the reusable component stays free of source-code concepts.
    headerButton()->setObjectName(
        QStringLiteral("galleryCodeBlockHeader"));
    if (auto* caption = findChild<fluent::textfields::Label*>(
            QStringLiteral("fluentExpanderHeaderText"))) {
        caption->setObjectName(
            QStringLiteral("galleryCodeBlockCaption"));
    }
    if (auto* clip = findChild<QWidget*>(
            QStringLiteral("fluentExpanderClip"))) {
        clip->setObjectName(
            QStringLiteral("galleryCodeBlockContent"));
    }

    m_contentInner = new QWidget;
    m_contentInner->setObjectName(
        QStringLiteral("galleryCodeBlockContentInner"));
    m_contentInner->setSizePolicy(
        QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* innerLayout = new QVBoxLayout(m_contentInner);
    innerLayout->setContentsMargins(16, 12, 14, 16);
    innerLayout->setSpacing(10);

    auto* topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);

    auto* langColumn = new QVBoxLayout;
    langColumn->setContentsMargins(0, 0, 0, 0);
    langColumn->setSpacing(4);

    m_langLabel = new fluent::textfields::Label(
        QStringLiteral("C++"), m_contentInner);
    m_langLabel->setObjectName(
        QStringLiteral("galleryCodeBlockLang"));
    m_langLabel->setFluentTypography(
        Typography::FontRole::Caption);
    m_langLabel->setTextColorRole(
        fluent::textfields::Label::TextColorRole::Secondary);

    m_langUnderline = new QWidget(m_contentInner);
    m_langUnderline->setObjectName(
        QStringLiteral("galleryCodeBlockLangUnderline"));
    m_langUnderline->setFixedSize(22, 3);
    langColumn->addWidget(m_langLabel, 0, Qt::AlignLeft);
    langColumn->addWidget(
        m_langUnderline, 0, Qt::AlignLeft);

    m_copyButton = new fluent::basicinput::Button(m_contentInner);
    m_copyButton->setObjectName(
        QStringLiteral("galleryCodeBlockCopyButton"));
    m_copyButton->setFluentStyle(
        fluent::basicinput::Button::Subtle);
    m_copyButton->setFluentSize(
        fluent::basicinput::Button::Small);
    m_copyButton->setFluentLayout(
        fluent::basicinput::Button::IconOnly);
    m_copyButton->setIconGlyph(
        Typography::Icons::Copy,
        Typography::IconSize::Standard);
    m_copyButton->setFocusPolicy(Qt::NoFocus);
    fluent::status_info::ToolTip::attach(
        m_copyButton, QStringLiteral("Copy"));

    connect(m_copyButton,
            &fluent::basicinput::Button::clicked,
            this,
            [this]() {
        if (QClipboard* clipboard = QApplication::clipboard()) {
            clipboard->setText(m_code);
            LOG_DEBUG(
                QStringLiteral(
                    "GalleryCodeBlock copyCode chars=%1")
                    .arg(m_code.size()));
            showGalleryToast(
                this, QStringLiteral("Copied to clipboard"));
            m_copyButton->setIconGlyph(
                Typography::Icons::CheckMark,
                Typography::IconSize::Standard);
            QPointer<fluent::basicinput::Button> button =
                m_copyButton;
            QTimer::singleShot(
                kCopyCheckRevertMs, this, [button]() {
                if (button) {
                    button->setIconGlyph(
                        Typography::Icons::Copy,
                        Typography::IconSize::Standard);
                }
            });
        }
    });

    topRow->addLayout(langColumn);
    topRow->addStretch(1);
    topRow->addWidget(m_copyButton, 0, Qt::AlignTop);

    m_codeLabel =
        new fluent::textfields::Label(m_contentInner);
    m_codeLabel->setObjectName(
        QStringLiteral("galleryCodeBlockText"));
    m_codeLabel->setTextFormat(Qt::RichText);
    m_codeLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse);
    m_codeLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_codeLabel->setTextColorRole(
        fluent::textfields::Label::TextColorRole::Primary);
    QFont monospace =
        QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monospace.setPixelSize(Typography::FontSize::Body);
    m_codeLabel->setFont(monospace);

    innerLayout->addLayout(topRow);
    innerLayout->addWidget(m_codeLabel);
    setContentWidget(
        m_contentInner, WidgetOwnership::Owned);

    // The signal is synchronous and fires before Expander measures its body,
    // so the expensive syntax highlighting remains lazy without measuring an
    // empty label on first expansion.
    connect(this,
            &Expander::expansionTransitionStarted,
            this,
            [this](bool expanding) {
        if (expanding)
            ensureHighlighted();
    });

    applyPalette();
}

void GalleryCodeBlock::setExpanded(
    bool expanded, bool animated)
{
    LOG_DEBUG(
        QStringLiteral(
            "GalleryCodeBlock setExpanded expanded=%1 animated=%2")
            .arg(expanded)
            .arg(animated));
    setExpandedAnimated(expanded, animated);
}

void GalleryCodeBlock::onThemeUpdated()
{
    Expander::onThemeUpdated();
    applyPalette();
    if (m_highlighted)
        applyHighlightedCode();
}

void GalleryCodeBlock::applyHighlightedCode()
{
    if (!m_codeLabel)
        return;

    m_codeLabel->setText(
        highlightCppToHtml(m_code, effectiveTheme() == Dark));
    m_highlighted = true;
}

void GalleryCodeBlock::ensureHighlighted()
{
    if (!m_highlighted)
        applyHighlightedCode();
}

void GalleryCodeBlock::applyPalette()
{
    const auto& colors = themeColorsRef();
    if (m_langUnderline) {
        m_langUnderline->setStyleSheet(
            QStringLiteral(
                "background: %1; border-radius: 1px;")
                .arg(cssColor(colors.accentDefault)));
    }
    if (m_langLabel)
        m_langLabel->onThemeUpdated();
    if (m_codeLabel)
        m_codeLabel->onThemeUpdated();
    if (m_copyButton)
        m_copyButton->onThemeUpdated();
}

} // namespace fluent::gallery

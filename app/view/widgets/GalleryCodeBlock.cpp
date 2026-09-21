#include "GalleryCodeBlock.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include "components/basicinput/Button.h"
#include "components/menus_toolbars/Menu.h"
#include "components/navigation/SelectorBar.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "GalleryLanguageSelector.h"
#include "support/logging/Log.h"
#include "view/support/GalleryCodeHighlighter.h"
#include "view/support/GalleryStyleSupport.h"
#include "view/support/GalleryToast.h"

namespace fluent::gallery {
namespace {

constexpr int kCopyCheckRevertMs = 1300;

class CodeContextMenu final : public menus_toolbars::FluentMenu {
public:
    explicit CodeContextMenu(QWidget* parent) : FluentMenu(QString(), parent)
    {
        setObjectName(QStringLiteral("FluentLabel.ContextMenu"));
        setFontStyle(Typography::FontRole::Caption);
    }

    void onThemeUpdated() override
    {
        FluentMenu::onThemeUpdated();
        const auto spacing = themeSpacing();
        const int shadow = contentsMargins().left();
        const int inset = shadow + qMax(1, spacing.gap.tight / 2);
        setContentsMargins(shadow, inset, shadow, inset);
        setItemLayoutMetrics(qMax(1, spacing.padding.listItemV / 2), spacing.gap.normal);
        setMinimumWidth(sizeHint().width());
        updateGeometry();
        update();
    }

    QIcon glyphIcon(const QString& glyph) const
    {
        auto pixmap = [this, &glyph](const QColor& color) {
            constexpr int size = Typography::IconSize::Standard;
            const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
            QPixmap result(qCeil(size * dpr), qCeil(size * dpr));
            result.setDevicePixelRatio(dpr);
            result.fill(Qt::transparent);
            QPainter painter(&result);
            painter.setPen(color);
            Typography::Icons::paintGlyph(painter, QRectF(0, 0, size, size), glyph, size,
                                          Qt::AlignCenter);
            return result;
        };
        const auto& colors = themeColorsRef();
        QIcon icon;
        icon.addPixmap(pixmap(colors.textPrimary), QIcon::Normal);
        icon.addPixmap(pixmap(colors.textPrimary), QIcon::Active);
        icon.addPixmap(pixmap(colors.textDisabled), QIcon::Disabled);
        return icon;
    }
};

// The displayed rich text contains expanded whitespace and synthetic wrapping
// points. Copy a source range, never strip characters from the displayed text:
// a real U+200B inside a string must remain part of the copied source.
class CodeLabel final : public textfields::Label {
public:
    CodeLabel(GalleryCodeBlock* block, QWidget* parent) : Label(parent), m_block(block) {}

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ShortcutOverride) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->matches(QKeySequence::Copy) || key->matches(QKeySequence::SelectAll)) {
                // Reserve editing shortcuts for the focused source selection.
                // The actual command still runs only on KeyPress.
                key->accept();
                return true;
            }
        }
        return Label::event(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->matches(QKeySequence::Copy)) {
            copySourceSelection();
            event->accept();
        } else if (event->matches(QKeySequence::SelectAll)) {
            selectAllSource();
            event->accept();
        } else {
            Label::keyPressEvent(event);
        }
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        auto* menu = new CodeContextMenu(this);
        auto* copy = menu->addAction(menu->glyphIcon(Typography::Icons::Copy),
                                     QCoreApplication::translate("QLineEdit", "&Copy"));
        copy->setShortcuts(QKeySequence::keyBindings(QKeySequence::Copy));
        copy->setEnabled(hasSelectedText());
        connect(copy, &QAction::triggered, this, [this]() { copySourceSelection(); });
        auto* selectAll = menu->addAction(menu->glyphIcon(Typography::Icons::SelectAll),
                                          QCoreApplication::translate("QLineEdit", "Select All"));
        selectAll->setShortcuts(QKeySequence::keyBindings(QKeySequence::SelectAll));
        selectAll->setEnabled(!text().isEmpty());
        connect(selectAll, &QAction::triggered, this, [this]() { selectAllSource(); });
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
        menu->popup(event->globalPos());
        event->accept();
    }

private:
    void ensureSourceMap()
    {
        if (!m_block)
            return;
        const QString source = m_block->code();
        const auto language = m_block->codeLanguage();
        if (m_source == source && m_language == language && !m_sourceMap.isEmpty())
            return;
        m_source = source;
        m_language = language;
        if (language == GalleryCodeLanguage::Python)
            highlightPythonToHtml(source, false, &m_sourceMap);
        else
            highlightCppToHtml(source, false, &m_sourceMap);
    }

    void selectAllSource()
    {
        ensureSourceMap();
        setSelection(0, static_cast<int>(m_sourceMap.size()));
    }

    void copySourceSelection()
    {
        if (!hasSelectedText() || !m_block)
            return;
        ensureSourceMap();
        const int start = selectionStart();
        const int end = qMin(start + static_cast<int>(selectedText().size()),
                             static_cast<int>(m_sourceMap.size()));
        if (start < 0 || start >= end)
            return;
        int sourceStart = -1;
        int sourceEnd = -1;
        for (int i = start; i < end; ++i) {
            const auto& span = m_sourceMap.at(i);
            if (span.start == span.end)
                continue;
            if (sourceStart < 0)
                sourceStart = span.start;
            sourceEnd = span.end;
        }
        if (QClipboard* clipboard = QApplication::clipboard()) {
            clipboard->setText(
                sourceStart < 0 ? QString() : m_source.mid(sourceStart, sourceEnd - sourceStart));
        }
    }

    QPointer<GalleryCodeBlock> m_block;
    QString m_source;
    GalleryCodeLanguage m_language = GalleryCodeLanguage::Cpp;
    CodeSourceMap m_sourceMap;
};

} // namespace

GalleryCodeBlock::GalleryCodeBlock(const QString& code, QWidget* parent)
    : GalleryCodeBlock(code, QString(), parent)
{}

GalleryCodeBlock::GalleryCodeBlock(const QString& cppCode, const QString& pythonCode,
                                   QWidget* parent)
    : Expander(parent), m_cppCode(cppCode), m_pythonCode(pythonCode)
{
    setObjectName(QStringLiteral("galleryCodeBlock"));
    setAppearance(Card::LayerAlt);
    setHeaderText(QStringLiteral("Source code"));

    // Preserve Gallery-specific object names used by focused visual/geometry
    // tests while the reusable component stays free of source-code concepts.
    headerButton()->setObjectName(QStringLiteral("galleryCodeBlockHeader"));
    if (auto* caption =
            findChild<fluent::textfields::Label*>(QStringLiteral("fluentExpanderHeaderText"))) {
        caption->setObjectName(QStringLiteral("galleryCodeBlockCaption"));
    }
    if (auto* clip = findChild<QWidget*>(QStringLiteral("fluentExpanderClip"))) {
        clip->setObjectName(QStringLiteral("galleryCodeBlockContent"));
    }

    m_contentInner = new QWidget;
    m_contentInner->setObjectName(QStringLiteral("galleryCodeBlockContentInner"));
    m_contentInner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_contentInner->setMinimumWidth(0);
    auto* innerLayout = new QVBoxLayout(m_contentInner);
    innerLayout->setContentsMargins(16, 12, 14, 16);
    innerLayout->setSpacing(10);

    auto* topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);

    if (hasPythonCode()) {
        m_languageSelector = new GalleryLanguageSelector(m_contentInner);
        m_languageSelector->setObjectName(QStringLiteral("galleryCodeBlockLanguageSelector"));
        connect(m_languageSelector, &GalleryLanguageSelector::languageChanged, this,
                &GalleryCodeBlock::setCodeLanguage);
        topRow->addWidget(m_languageSelector, 0, Qt::AlignVCenter);
    } else {
        auto* langColumn = new QVBoxLayout;
        langColumn->setContentsMargins(0, 0, 0, 0);
        langColumn->setSpacing(4);

        m_langLabel = new fluent::textfields::Label(QStringLiteral("C++"), m_contentInner);
        m_langLabel->setObjectName(QStringLiteral("galleryCodeBlockLang"));
        m_langLabel->setFluentTypography(Typography::FontRole::Caption);
        m_langLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

        m_langUnderline = new QWidget(m_contentInner);
        m_langUnderline->setObjectName(QStringLiteral("galleryCodeBlockLangUnderline"));
        m_langUnderline->setFixedSize(22, 3);
        langColumn->addWidget(m_langLabel, 0, Qt::AlignLeft);
        langColumn->addWidget(m_langUnderline, 0, Qt::AlignLeft);
        topRow->addLayout(langColumn);
    }

    m_sourceSelector = new navigation::SelectorBar(m_contentInner);
    m_sourceSelector->setObjectName(QStringLiteral("galleryCodeBlockSourceSelector"));
    m_sourceSelector->setAccessibleName(QStringLiteral("Source detail"));
    m_sourceSelector->setItemFontRole(Typography::FontRole::Caption);
    m_sourceSelector->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_sourceSelector->addItem(QStringLiteral("Key usage"));
    m_sourceSelector->addItem(QStringLiteral("Full example"));
    m_sourceSelector->setSelectedIndex(0);
    m_sourceSelector->hide();
    topRow->addWidget(m_sourceSelector, 0, Qt::AlignVCenter);
    connect(m_sourceSelector, &navigation::SelectorBar::selectedIndexChanged, this,
            [this] { refreshDisplayedCode(); });

    m_copyButton = new fluent::basicinput::Button(m_contentInner);
    m_copyButton->setObjectName(QStringLiteral("galleryCodeBlockCopyButton"));
    m_copyButton->setAccessibleName(QStringLiteral("Copy displayed code"));
    m_copyButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_copyButton->setFluentSize(fluent::basicinput::Button::Small);
    m_copyButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_copyButton->setIconGlyph(Typography::Icons::Copy, Typography::IconSize::Standard);
    m_copyButton->setFocusPolicy(Qt::NoFocus);
    fluent::status_info::ToolTip::attach(m_copyButton, QStringLiteral("Copy"));

    connect(m_copyButton, &fluent::basicinput::Button::clicked, this, [this]() {
        if (QClipboard* clipboard = QApplication::clipboard()) {
            clipboard->setText(code());
            LOG_DEBUG(QStringLiteral("GalleryCodeBlock copyCode chars=%1").arg(code().size()));
            showGalleryToast(this, QStringLiteral("Copied to clipboard"));
            m_copyButton->setIconGlyph(Typography::Icons::CheckMark,
                                       Typography::IconSize::Standard);
            QPointer<fluent::basicinput::Button> button = m_copyButton;
            QTimer::singleShot(kCopyCheckRevertMs, this, [button]() {
                if (button) {
                    button->setIconGlyph(Typography::Icons::Copy, Typography::IconSize::Standard);
                }
            });
        }
    });

    topRow->addStretch(1);
    topRow->addWidget(m_copyButton, 0, Qt::AlignVCenter);

    m_codeLabel = new CodeLabel(this, m_contentInner);
    m_codeLabel->setObjectName(QStringLiteral("galleryCodeBlockText"));
    m_codeLabel->setTextFormat(Qt::RichText);
    m_codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_codeLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_codeLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
    m_codeLabel->setWordWrap(true);
    m_codeLabel->setMinimumWidth(0);
    m_codeLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QFont monospace = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monospace.setPixelSize(Typography::FontSize::Body);
    m_codeLabel->setFont(monospace);

    innerLayout->addLayout(topRow);
    innerLayout->addWidget(m_codeLabel);
    setContentWidget(m_contentInner, WidgetOwnership::Owned);

    // The signal is synchronous and fires before Expander measures its body,
    // so the expensive syntax highlighting remains lazy without measuring an
    // empty label on first expansion.
    connect(this, &Expander::expansionTransitionStarted, this, [this](bool expanding) {
        if (expanding)
            ensureHighlighted();
    });

    applyPalette();
}

QString GalleryCodeBlock::code() const
{
    if (m_codeLanguage == GalleryCodeLanguage::Python && hasPythonCode()) {
        return m_pythonCode;
    }
    if (!m_cppExcerpt.isEmpty() && m_sourceSelector->selectedIndex() == 0)
        return m_cppExcerpt;
    return m_cppCode;
}

void GalleryCodeBlock::setCppExcerpt(const QString& code)
{
    if (m_cppExcerpt == code)
        return;
    m_cppExcerpt = code;
    m_excerptHighlightedHtml.clear();
    m_sourceSelector->setVisible(!code.isEmpty() && m_codeLanguage == GalleryCodeLanguage::Cpp);
    if (m_langUnderline)
        m_langUnderline->setVisible(code.isEmpty());
    refreshDisplayedCode();
}

void GalleryCodeBlock::setCppCode(const QString& code)
{
    if (m_cppCode == code)
        return;
    const bool expanded = isExpanded();
    if (expanded)
        setExpandedAnimated(false, false);
    m_cppCode = code;
    m_cppHighlightedHtml.clear();
    m_codeLabel->clear();
    if (expanded) {
        applyHighlightedCode();
        setExpandedAnimated(true, false);
    }
}

void GalleryCodeBlock::setCodeLanguage(GalleryCodeLanguage language)
{
    if (language == GalleryCodeLanguage::Python && !hasPythonCode())
        return;
    if (m_codeLanguage == language)
        return;

    const bool reopen = isExpanded();
    if (reopen)
        setExpandedAnimated(false, /*animated=*/false);
    m_codeLanguage = language;
    m_sourceSelector->setVisible(!m_cppExcerpt.isEmpty() && language == GalleryCodeLanguage::Cpp);
    if (m_languageSelector)
        m_languageSelector->setLanguage(language);
    if (m_codeLabel)
        m_codeLabel->clear();
    if (reopen) {
        applyHighlightedCode();
        if (m_contentInner && m_contentInner->layout()) {
            m_contentInner->layout()->invalidate();
            m_contentInner->layout()->activate();
            m_contentInner->adjustSize();
        }
        setExpandedAnimated(true, /*animated=*/false);
    }
    emit codeLanguageChanged(language);
}

void GalleryCodeBlock::refreshDisplayedCode()
{
    const bool reopen = isExpanded();
    if (reopen)
        setExpandedAnimated(false, false);
    m_codeLabel->clear();
    if (reopen) {
        applyHighlightedCode();
        m_contentInner->layout()->invalidate();
        m_contentInner->layout()->activate();
        setExpandedAnimated(true, false);
    }
}

void GalleryCodeBlock::setExpanded(bool expanded, bool animated)
{
    LOG_DEBUG(QStringLiteral("GalleryCodeBlock setExpanded expanded=%1 animated=%2")
                  .arg(expanded)
                  .arg(animated));
    setExpandedAnimated(expanded, animated);
}

void GalleryCodeBlock::onThemeUpdated()
{
    Expander::onThemeUpdated();
    applyPalette();
    m_cppHighlightedHtml.clear();
    m_pythonHighlightedHtml.clear();
    m_excerptHighlightedHtml.clear();
    if (isExpanded())
        applyHighlightedCode();
    else if (m_codeLabel)
        m_codeLabel->clear();
}

void GalleryCodeBlock::applyHighlightedCode()
{
    if (!m_codeLabel)
        return;

    const bool dark = effectiveThemeUsesDarkAppearance();
    if (m_codeLanguage == GalleryCodeLanguage::Python && hasPythonCode()) {
        if (m_pythonHighlightedHtml.isEmpty()) {
            m_pythonHighlightedHtml = highlightPythonToHtml(m_pythonCode, dark);
        }
        m_codeLabel->setText(m_pythonHighlightedHtml);
    } else if (!m_cppExcerpt.isEmpty() && m_sourceSelector->selectedIndex() == 0) {
        if (m_excerptHighlightedHtml.isEmpty())
            m_excerptHighlightedHtml = highlightCppToHtml(m_cppExcerpt, dark);
        m_codeLabel->setText(m_excerptHighlightedHtml);
    } else {
        if (m_cppHighlightedHtml.isEmpty())
            m_cppHighlightedHtml = highlightCppToHtml(m_cppCode, dark);
        m_codeLabel->setText(m_cppHighlightedHtml);
    }
}

void GalleryCodeBlock::ensureHighlighted()
{
    if (m_codeLabel && m_codeLabel->text().isEmpty())
        applyHighlightedCode();
}

void GalleryCodeBlock::applyPalette()
{
    const auto& colors = themeColorsRef();
    if (m_langUnderline) {
        m_langUnderline->setStyleSheet(QStringLiteral("background: %1; border-radius: 1px;")
                                           .arg(cssColor(colors.accentDefault)));
    }
    if (m_langLabel)
        m_langLabel->onThemeUpdated();
    if (m_codeLabel)
        m_codeLabel->onThemeUpdated();
    if (m_copyButton)
        m_copyButton->onThemeUpdated();
    if (m_languageSelector)
        m_languageSelector->onThemeUpdated();
    if (m_sourceSelector)
        m_sourceSelector->onThemeUpdated();
}

} // namespace fluent::gallery

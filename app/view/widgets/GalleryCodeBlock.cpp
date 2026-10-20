#include "GalleryCodeBlock.h"

#include <QHBoxLayout>
#include <QPointer>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/navigation/SelectorBar.h"
#include "components/textfields/Label.h"
#include "GalleryLanguageSelector.h"
#include "view/support/GalleryToast.h"

namespace fluent::gallery {

GalleryCodeBlock::GalleryCodeBlock(const QString& code, QWidget* parent) : GalleryCodeBlock(code, QString(), parent) {}

GalleryCodeBlock::GalleryCodeBlock(const QString& cppCode, const QString& pythonCode, QWidget* parent)
    : CodeBlock(parent), m_cppCode(cppCode), m_pythonCode(pythonCode)
{
    setObjectName(QStringLiteral("galleryCodeBlock"));
    setAppearance(Card::LayerAlt);
    setHeaderText(QStringLiteral("Source code"));
    setMaxVisibleLines(0);
    setExpandedAnimated(false, false);

    // Preserve Gallery-specific object names used by focused visual/geometry
    // tests while the reusable component stays free of source-code concepts.
    headerButton()->setObjectName(QStringLiteral("galleryCodeBlockHeader"));
    if (auto* caption = findChild<fluent::textfields::Label*>(QStringLiteral("fluentExpanderHeaderText"))) {
        caption->setObjectName(QStringLiteral("galleryCodeBlockCaption"));
    }
    if (auto* clip = findChild<QWidget*>(QStringLiteral("fluentExpanderClip"))) {
        clip->setObjectName(QStringLiteral("galleryCodeBlockContent"));
    }

    m_contentInner = contentWidget();
    m_contentInner->setObjectName(QStringLiteral("galleryCodeBlockContentInner"));
    auto* innerLayout = qobject_cast<QVBoxLayout*>(m_contentInner->layout());
    auto* codeLabel = findChild<textfields::Label*>(QStringLiteral("fluentCodeBlockText"));
    codeLabel->setObjectName(QStringLiteral("galleryCodeBlockText"));
    m_copyButton = findChild<basicinput::Button*>(QStringLiteral("fluentCodeBlockCopyButton"));
    m_copyButton->setObjectName(QStringLiteral("galleryCodeBlockCopyButton"));
    m_copyButton->setAccessibleName(QStringLiteral("Copy displayed code"));
    connect(this, &CodeBlock::copyFinished, this, [this](bool copied) {
        showGalleryToast(this,
            copied ? QStringLiteral("Copied to clipboard") : QStringLiteral("Could not copy to clipboard. Try again."),
            copied ? status_info::Toast::Success : status_info::Toast::Error);
    });

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
        m_langLabel = new textfields::Label(QStringLiteral("C++"), m_contentInner);
        m_langLabel->setObjectName(QStringLiteral("galleryCodeBlockLang"));
        m_langLabel->setFluentTypography(Typography::FontRole::Caption);
        m_langLabel->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        topRow->addWidget(m_langLabel, 0, Qt::AlignVCenter);
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
    connect(m_sourceSelector, &navigation::SelectorBar::selectedIndexChanged, this, [this] { refreshDisplayedCode(); });

    topRow->addStretch(1);
    innerLayout->insertLayout(0, topRow);
    m_fileRow = new QWidget(m_contentInner);
    auto* filesLayout = new QHBoxLayout(m_fileRow);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->setSpacing(8);
    auto* fileCaption = new textfields::Label(QStringLiteral("File"), m_fileRow);
    fileCaption->setFluentTypography(Typography::FontRole::Caption);
    filesLayout->addWidget(fileCaption);
    m_fileSelector = new basicinput::ComboBox(m_fileRow);
    m_fileSelector->setObjectName(QStringLiteral("galleryCodeBlockFileSelector"));
    m_fileSelector->setAccessibleName(QStringLiteral("Source file"));
    m_fileSelector->setFontRole(Typography::FontRole::Caption);
    m_fileSelector->setContentPaddingV(5);
    m_fileSelector->setFixedHeight(32);
    m_fileSelector->setMinimumWidth(0);
    m_fileSelector->setMaximumWidth(240);
    m_fileSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    filesLayout->addWidget(m_fileSelector);
    fileCaption->setBuddy(m_fileSelector);
    m_fileLanguage = new textfields::Label(m_fileRow);
    m_fileLanguage->setObjectName(QStringLiteral("galleryCodeBlockFileLanguage"));
    m_fileLanguage->setFluentTypography(Typography::FontRole::Caption);
    m_fileLanguage->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    filesLayout->addWidget(m_fileLanguage);
    filesLayout->addStretch(1);
    m_fileRow->hide();
    innerLayout->insertWidget(1, m_fileRow);
    connect(m_fileSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        refreshDisplayedCode();
    });
    refreshDisplayedCode();
}

QString GalleryCodeBlock::code() const
{
    if (const auto* file = currentFile()) {
        if (!file->usageSnippet.isEmpty() && m_sourceSelector->selectedIndex() == 0)
            return file->usageSnippet;
        return file->code;
    }
    if (m_codeLanguage == GalleryCodeLanguage::Python && hasPythonCode()) {
        return m_pythonCode;
    }
    if (!m_cppExcerpt.isEmpty() && m_sourceSelector->selectedIndex() == 0)
        return m_cppExcerpt;
    return m_cppCode;
}

const QVector<GallerySourceFile>& GalleryCodeBlock::sourceFiles() const
{
    return m_codeLanguage == GalleryCodeLanguage::Python ? m_pythonFiles : m_cppFiles;
}

const GallerySourceFile* GalleryCodeBlock::currentFile() const
{
    const auto& files = sourceFiles();
    const int index = m_fileSelector ? m_fileSelector->currentIndex() : -1;
    return index >= 0 && index < files.size() ? &files.at(index) : nullptr;
}

QString GalleryCodeBlock::sourceLanguage() const
{
    if (const auto* file = currentFile())
        return file->language;
    return m_codeLanguage == GalleryCodeLanguage::Python ? QStringLiteral("python") : QStringLiteral("cpp");
}

void GalleryCodeBlock::setSourceFiles(const QVector<GallerySourceFile>& cppFiles,
    const QVector<GallerySourceFile>& pythonFiles)
{
    m_cppFiles = cppFiles;
    m_pythonFiles = pythonFiles;
    rebuildFileSelector();
    refreshDisplayedCode();
}

void GalleryCodeBlock::rebuildFileSelector(const QString& preferredName)
{
    const QSignalBlocker blocker(m_fileSelector);
    m_fileSelector->clear();
    int selected = 0;
    const auto& files = sourceFiles();
    for (int index = 0; index < files.size(); ++index) {
        m_fileSelector->addItem(files.at(index).name);
        if (files.at(index).name == preferredName)
            selected = index;
    }
    if (!files.isEmpty())
        m_fileSelector->setCurrentIndex(selected);
    m_fileRow->setVisible(files.size() > 1);
}

void GalleryCodeBlock::updateSourceControls()
{
    const auto* file = currentFile();
    const bool hasExcerpt = file
        ? !file->usageSnippet.isEmpty() && file->usageSnippet != file->code
        : !m_cppExcerpt.isEmpty() && m_codeLanguage == GalleryCodeLanguage::Cpp;
    m_sourceSelector->setVisible(hasExcerpt);
    const auto language = sourceLanguage();
    m_fileLanguage->setText(language == QStringLiteral("cpp") ? QStringLiteral("C++")
            : language == QStringLiteral("python")            ? QStringLiteral("Python")
            : language == QStringLiteral("qml")               ? QStringLiteral("QML")
            : language == QStringLiteral("xml")
            ? QStringLiteral("XML")
            : QStringLiteral("CMake"));
}

void GalleryCodeBlock::setCppExcerpt(const QString& code)
{
    if (m_cppExcerpt == code)
        return;
    m_cppExcerpt = code;
    refreshDisplayedCode();
}

void GalleryCodeBlock::setCppCode(const QString& code)
{
    if (m_cppCode == code)
        return;
    m_cppCode = code;
    refreshDisplayedCode();
}

void GalleryCodeBlock::setCodeLanguage(GalleryCodeLanguage language)
{
    if (language == GalleryCodeLanguage::Python && !hasPythonCode())
        return;
    if (m_codeLanguage == language)
        return;

    m_codeLanguage = language;
    rebuildFileSelector();
    updateSourceControls();
    if (m_languageSelector)
        m_languageSelector->setLanguage(language);
    QPointer<GalleryCodeBlock> guard(this);
    refreshDisplayedCode();
    if (guard)
        emit codeLanguageChanged(language);
}

void GalleryCodeBlock::refreshDisplayedCode()
{
    updateSourceControls();
    CodeBlock::setLanguage(sourceLanguage());
    CodeBlock::setCode(code());
    setHeaderText(QStringLiteral("Source code"));
}

void GalleryCodeBlock::setExpanded(bool expanded, bool animated)
{
    setExpandedAnimated(expanded, animated);
}

} // namespace fluent::gallery
